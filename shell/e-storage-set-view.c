/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* e-storage-set-view.c
 *
 * Copyright (C) 2000  Helix Code, Inc.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 * Author: Ettore Perazzoli
 * Etree-ification: Chris Toshok
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <gnome.h>

#include "e-util/e-util.h"
#include "e-shell-constants.h"

#include "e-storage-set-view.h"

#include "e-table/e-tree-simple.h"
#include "e-table/e-cell-tree.h"
#include "e-table/e-cell-text.h"

#include "art/tree-expanded.xpm"
#include "art/tree-unexpanded.xpm"

#define ETABLE_SPEC "<ETableSpecification no-header=\"1\">             \
	<columns-shown>                  			       \
		<column> 0 </column>     			       \
	</columns-shown>                 			       \
	<grouping></grouping>                                          \
</ETableSpecification>"


#define PARENT_TYPE E_TABLE_TYPE
static ETableClass *parent_class = NULL;

struct _EStorageSetViewPrivate {
	EStorageSet *storage_set;

	ETreeModel *etree_model;
	ETreePath *root_node;
	GdkPixbuf *tree_expanded_pixbuf;
	GdkPixbuf *tree_unexpanded_pixbuf;

	GHashTable *path_to_etree_node;

	GHashTable *type_name_to_pixbuf;

	/* Path of the row selected by the latest "cursor_change" signal.  */
	const char *selected_row_path;
};


enum {
	FOLDER_SELECTED,
	LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = { 0 };


/* DND stuff.  */

enum _DndTargetType {
	DND_TARGET_TYPE_URI_LIST,
	DND_TARGET_TYPE_E_SHORTCUT
};
typedef enum _DndTargetType DndTargetType;

#define URI_LIST_TYPE   "text/uri-list"
#define E_SHORTCUT_TYPE "E-SHORTCUT"

static GtkTargetEntry drag_types [] = {
	{ URI_LIST_TYPE, 0, DND_TARGET_TYPE_URI_LIST },
	{ E_SHORTCUT_TYPE, 0, DND_TARGET_TYPE_E_SHORTCUT }
};
static const int num_drag_types = sizeof (drag_types) / sizeof (drag_types[0]);

static GtkTargetList *target_list;


/* Helper functions.  */

static gboolean
add_node_to_hash (EStorageSetView *storage_set_view,
		  const char *path,
		  ETreePath *node)
{
	EStorageSetViewPrivate *priv;
	char *hash_path;

	g_return_val_if_fail (g_path_is_absolute (path), FALSE);

	priv = storage_set_view->priv;

	if (g_hash_table_lookup (priv->path_to_etree_node, path) != NULL) {
		g_warning ("EStorageSetView: Node already existing while adding -- %s", path);
		return FALSE;
	}

	g_print ("EStorageSetView: Adding -- %s\n", path);

	hash_path = g_strdup (path);

	g_hash_table_insert (priv->path_to_etree_node, hash_path, node);

	return TRUE;
}

static ETreePath *
remove_node_from_hash (EStorageSetView *storage_set_view,
		       const char *path)
{
	EStorageSetViewPrivate *priv;
	ETreePath *node;

	priv = storage_set_view->priv;

	node = g_hash_table_lookup (priv->path_to_etree_node, path);
	if (node == NULL) {
		g_warning ("EStorageSetView: Node not found while removing -- %s", path);
		return NULL;
	}

	g_print ("EStorageSetView: Removing -- %s\n", path);

	g_hash_table_remove (priv->path_to_etree_node, path);

	return node;
}

static GdkPixbuf*
get_pixbuf_for_folder (EStorageSetView *storage_set_view,
		       EFolder *folder)
{
	GdkPixbuf *scaled_pixbuf;
	const char *type_name;
	EStorageSetViewPrivate *priv;

	priv = storage_set_view->priv;
	
	type_name = e_folder_get_type_string (folder);

	scaled_pixbuf = g_hash_table_lookup (priv->type_name_to_pixbuf, type_name);

	if (scaled_pixbuf == NULL) {
		EFolderTypeRegistry *folder_type_registry;
		EStorageSet *storage_set;
		GdkPixbuf *icon_pixbuf;

		storage_set = priv->storage_set;
		folder_type_registry = e_storage_set_get_folder_type_registry (storage_set);

		icon_pixbuf = e_folder_type_registry_get_icon_for_type (folder_type_registry,
									type_name, TRUE);

		if (icon_pixbuf == NULL) {
			return NULL;
		}

		scaled_pixbuf = gdk_pixbuf_new (gdk_pixbuf_get_colorspace (icon_pixbuf),
						gdk_pixbuf_get_has_alpha (icon_pixbuf),
						gdk_pixbuf_get_bits_per_sample (icon_pixbuf),
						E_SHELL_MINI_ICON_SIZE, E_SHELL_MINI_ICON_SIZE);

		gdk_pixbuf_scale (icon_pixbuf, scaled_pixbuf,
				  0, 0, E_SHELL_MINI_ICON_SIZE, E_SHELL_MINI_ICON_SIZE,
				  0.0, 0.0,
				  (double) E_SHELL_MINI_ICON_SIZE / gdk_pixbuf_get_width (icon_pixbuf),
				  (double) E_SHELL_MINI_ICON_SIZE / gdk_pixbuf_get_height (icon_pixbuf),
				  GDK_INTERP_HYPER);

		g_hash_table_insert (priv->type_name_to_pixbuf, g_strdup(type_name), scaled_pixbuf);
	}

	return scaled_pixbuf;
}


/* Folder context menu.  */
/* FIXME: This should be moved somewhere else, so that also the sortcut code
   can share it.  */

static void
folder_context_menu_activate_cb (BonoboUIHandler *uih,
				 void *data,
				 const char *path)
{
	EStorageSetView *storage_set_view;
	EStorageSetViewPrivate *priv;

	storage_set_view = E_STORAGE_SET_VIEW (data);
	priv = storage_set_view->priv;

	gtk_signal_emit (GTK_OBJECT (storage_set_view), signals[FOLDER_SELECTED],
			 priv->selected_row_path);
}

static void
populate_folder_context_menu_with_common_items (EStorageSetView *storage_set_view,
						BonoboUIHandler *uih)
{
	bonobo_ui_handler_menu_new_item (uih, "/Activate",
					 _("_View"), _("View the selected folder"),
					 0, BONOBO_UI_HANDLER_PIXMAP_NONE,
					 NULL, 0, 0,
					 folder_context_menu_activate_cb,
					 storage_set_view);
}

static void
popup_folder_menu (EStorageSetView *storage_set_view,
		   GdkEventButton *event)
{
	EvolutionShellComponentClient *handler;
	EStorageSetViewPrivate *priv;
	EFolderTypeRegistry *folder_type_registry;
	BonoboUIHandler *uih;
	EFolder *folder;

	priv = storage_set_view->priv;

	uih = bonobo_ui_handler_new ();
	bonobo_ui_handler_create_popup_menu (uih);

	folder = e_storage_set_get_folder (priv->storage_set, priv->selected_row_path);
	if (folder == NULL) {
		/* Uh!?  */
		return;
	}

	folder_type_registry = e_storage_set_get_folder_type_registry (priv->storage_set);
	g_assert (folder_type_registry != NULL);

	handler = e_folder_type_registry_get_handler_for_type (folder_type_registry,
							       e_folder_get_type_string (folder));
	g_assert (handler != NULL);

	evolution_shell_component_client_populate_folder_context_menu (handler,
								       uih,
								       e_folder_get_physical_uri (folder),
								       e_folder_get_type_string (folder));

	populate_folder_context_menu_with_common_items (storage_set_view, uih);

	bonobo_ui_handler_do_popup_menu (uih);

	bonobo_object_unref (BONOBO_OBJECT (uih));
}


/* GtkObject methods.  */

static void
path_free_func (gpointer key, gpointer value, gpointer user_data)
{
	g_free (key);
}

static void
pixbuf_free_func (gpointer key, gpointer value, gpointer user_data)
{
	g_free (key);
	gdk_pixbuf_unref ((GdkPixbuf*)value);
}

static void
destroy (GtkObject *object)
{
	EStorageSetView *storage_set_view;
	EStorageSetViewPrivate *priv;

	storage_set_view = E_STORAGE_SET_VIEW (object);
	priv = storage_set_view->priv;

	/* need to destroy our tree */
	e_tree_model_node_remove (priv->etree_model, priv->root_node);
	gtk_object_unref (GTK_OBJECT (priv->etree_model));

	/* now free up all the paths stored in the hash table and
           destroy the hash table itself */
	g_hash_table_foreach (priv->path_to_etree_node, path_free_func, NULL);
	g_hash_table_destroy (priv->path_to_etree_node);

	/* now free up all the type_names and pixbufs stored in the
           hash table and destroy the hash table itself */
	g_hash_table_foreach (priv->type_name_to_pixbuf, pixbuf_free_func, NULL);
	g_hash_table_destroy (priv->type_name_to_pixbuf);

	gtk_object_unref (GTK_OBJECT (priv->storage_set));

	/* free up our expanded/unexpanded pixmaps */
	gdk_pixbuf_unref (priv->tree_expanded_pixbuf);
	gdk_pixbuf_unref (priv->tree_unexpanded_pixbuf);

	g_free (priv);

	(* GTK_OBJECT_CLASS (parent_class)->destroy) (object);
}


/* ETable methods */

static void
etable_drag_begin (EStorageSetView *storage_set_view,
		   int row, int col,
		   GdkDragContext *context)
{
	EStorageSetViewPrivate *priv;
	ETreePath *node;

	priv = storage_set_view->priv;

	node = e_tree_model_node_at_row (priv->etree_model, row);

	priv->selected_row_path = e_tree_model_node_get_data (priv->etree_model, node);

	g_print ("dragging %s\n", priv->selected_row_path);
}

static void
set_uri_list_selection (EStorageSetView *storage_set_view,
			GtkSelectionData *selection_data)
{
	EStorageSetViewPrivate *priv;
	char *uri_list;

	priv = storage_set_view->priv;

	/* FIXME: Get `evolution:' from somewhere instead of hardcoding it here.  */
	uri_list = g_strconcat ("evolution:", priv->selected_row_path, "\n", NULL);
	gtk_selection_data_set (selection_data, selection_data->target,
				8, (guchar *) uri_list, strlen (uri_list));
	g_free (uri_list);
}

static void
set_e_shortcut_selection (EStorageSetView *storage_set_view,
			  GtkSelectionData *selection_data)
{
	EStorageSetViewPrivate *priv;
	int shortcut_len;
	char *shortcut;
	const char *trailing_slash;
	const char *name;

	g_return_if_fail(storage_set_view != NULL);

	priv = storage_set_view->priv;

	trailing_slash = strrchr (priv->selected_row_path, '/');
	if (trailing_slash == NULL)
		name = NULL;
	else
		name = trailing_slash + 1;

	/* FIXME: Get `evolution:' from somewhere instead of hardcoding it here.  */

	if (name != NULL)
		shortcut_len = strlen (name);
	else
		shortcut_len = 0;
	
	shortcut_len ++;	/* Separating zero.  */

	shortcut_len += strlen ("evolution:");
	shortcut_len += strlen (priv->selected_row_path);
	shortcut_len ++;	/* Trailing zero.  */

	shortcut = g_malloc (shortcut_len);

	if (name == NULL)
		sprintf (shortcut, "%cevolution:%s", '\0', priv->selected_row_path);
	else
		sprintf (shortcut, "%s%cevolution:%s", name, '\0', priv->selected_row_path);

	gtk_selection_data_set (selection_data, selection_data->target,
				8, (guchar *) shortcut, shortcut_len);

	g_free (shortcut);
}

static void
etable_drag_data_get (EStorageSetView *storage_set_view,
		      int drag_row,
		      int drag_col,
		      GdkDragContext *context,
		      GtkSelectionData *selection_data,
		      guint info,
		      guint32 time)
{
	switch (info) {
	case DND_TARGET_TYPE_URI_LIST:
		set_uri_list_selection (storage_set_view, selection_data);
		break;
	case DND_TARGET_TYPE_E_SHORTCUT:
		set_e_shortcut_selection (storage_set_view, selection_data);
		break;
	default:
		g_assert_not_reached ();
	}
}


/* ETableModel Methods */

/* This function returns the number of columns in our ETableModel. */
static int
etree_col_count (ETableModel *etc, void *data)
{
	return 1;
}

/* This function duplicates the value passed to it. */
static void *
etree_duplicate_value (ETableModel *etc, int col, const void *value, void *data)
{
	return g_strdup (value);
}

/* This function frees the value passed to it. */
static void
etree_free_value (ETableModel *etc, int col, void *value, void *data)
{
	g_free (value);
}

/* This function creates an empty value. */
static void *
etree_initialize_value (ETableModel *etc, int col, void *data)
{
	return g_strdup ("");
}

/* This function reports if a value is empty. */
static gboolean
etree_value_is_empty (ETableModel *etc, int col, const void *value, void *data)
{
	return !(value && *(char *)value);
}

/* This function reports if a value is empty. */
static char *
etree_value_to_string (ETableModel *etc, int col, const void *value, void *data)
{
	return g_strdup(value);
}

/* ETreeModel Methods */

static GdkPixbuf*
etree_icon_at (ETreeModel *etree, ETreePath *tree_path, void *model_data)
{
	EStorageSetView *storage_set_view;
	char *path;
	EStorageSet *storage_set;

	/* folders are from depth 2 on.  depth 1 are storages and 0 is
           our (invisible) root node. */
	if (e_tree_model_node_depth (etree, tree_path) < 2)
		return NULL;

	storage_set_view = E_STORAGE_SET_VIEW (model_data);
	storage_set = storage_set_view->priv->storage_set;

	path = (char*)e_tree_model_node_get_data (etree, tree_path);

	return get_pixbuf_for_folder (storage_set_view,
		      e_storage_set_get_folder (storage_set, path));
}

static void*
etree_value_at (ETreeModel *etree, ETreePath *tree_path, int col, void *model_data)
{
	char *path;
	char *last_separator;

	path = (char*)e_tree_model_node_get_data (etree, tree_path);

	last_separator = strrchr (path, G_DIR_SEPARATOR);

	g_return_val_if_fail (last_separator != NULL, NULL);

	return last_separator + 1;
}

static void
etree_set_value_at (ETreeModel *etree, ETreePath *path, int col, const void *val, void *model_data)
{
	/* nada */
}

static gboolean
etree_is_editable (ETreeModel *etree, ETreePath *path, int col, void *model_data)
{
	return FALSE;
}


/* StorageSet signal handling.  */

static ETreeModel *sort_model;

static gint
treepath_compare (ETreePath **node1,
		  ETreePath **node2)
{
	char *path1, *path2;
	path1 = e_tree_model_node_get_data (sort_model, *node1);
	path2 = e_tree_model_node_get_data (sort_model, *node2);

	return strcasecmp (path1, path2);
}

static void
new_storage_cb (EStorageSet *storage_set,
		EStorage *storage,
		void *data)
{
	EStorageSetView *storage_set_view;
	EStorageSetViewPrivate *priv;
	ETreePath *node;
	char *path;

	storage_set_view = E_STORAGE_SET_VIEW (data);
	priv = storage_set_view->priv;

	path = g_strconcat (G_DIR_SEPARATOR_S, e_storage_get_name (storage), NULL);

	node = e_tree_model_node_insert (priv->etree_model,
					 priv->root_node,
					 -1, path);

	e_tree_model_node_set_expanded (priv->etree_model, node, TRUE);

	if (! add_node_to_hash (storage_set_view, path, node)) {
		g_free (path);
		e_tree_model_node_remove (priv->etree_model, node);
		return;
	}

	/* FIXME: We want a more specialized sort, e.g. the local folders should always be
           on top.  */
	sort_model = priv->etree_model;
	e_tree_model_node_sort (priv->etree_model, priv->root_node, (GCompareFunc)treepath_compare);
}

static void
removed_storage_cb (EStorageSet *storage_set,
		    EStorage *storage,
		    void *data)
{
	EStorageSetView *storage_set_view;
	EStorageSetViewPrivate *priv;
	ETreeModel *etree;
	ETreePath *node;
	char *path;
	char *node_data;

	storage_set_view = E_STORAGE_SET_VIEW (data);
	priv = storage_set_view->priv;
	etree = priv->etree_model;

	path = g_strconcat (G_DIR_SEPARATOR_S, e_storage_get_name (storage), NULL);
	node = remove_node_from_hash (storage_set_view, path);
	g_free (path);

	node_data = e_tree_model_node_remove (etree, node);
	g_free (node_data);
}

static void
new_folder_cb (EStorageSet *storage_set,
	       const char *path,
	       void *data)
{
	EStorageSetView *storage_set_view;
	EStorageSetViewPrivate *priv;
	ETreeModel *etree;
	ETreePath *parent_node;
	ETreePath *new_node;
	const char *last_separator;
	char *parent_path;

	g_return_if_fail (g_path_is_absolute (path));

	storage_set_view = E_STORAGE_SET_VIEW (data);
	priv = storage_set_view->priv;
	etree = priv->etree_model;

	last_separator = strrchr (path, G_DIR_SEPARATOR);

	parent_path = g_strndup (path, last_separator - path);
	parent_node = g_hash_table_lookup (priv->path_to_etree_node, parent_path);
	if (parent_node == NULL) {
		g_print ("EStorageSetView: EStorageSet reported new subfolder for non-existing folder -- %s\n",
			 parent_path);
		g_free (parent_path);
		return;
	}

	g_free (parent_path);

	new_node = e_tree_model_node_insert (etree, parent_node, -1, (gpointer)g_strdup(path));

	if (! add_node_to_hash (storage_set_view, path, new_node)) {
		e_tree_model_node_remove (etree, new_node);
		return;
	}

	sort_model = priv->etree_model;
	e_tree_model_node_sort (priv->etree_model, parent_node, (GCompareFunc)treepath_compare);
}

static void
removed_folder_cb (EStorageSet *storage_set,
		   const char *path,
		   void *data)
{
	EStorageSetView *storage_set_view;
	EStorageSetViewPrivate *priv;
	ETreeModel *etree;
	ETreePath *node;
	char *node_data;

	storage_set_view = E_STORAGE_SET_VIEW (data);
	priv = storage_set_view->priv;
	etree = priv->etree_model;

	node = remove_node_from_hash (storage_set_view, path);
	node_data = e_tree_model_node_remove (etree, node);
	g_free (node_data);
}


static void
class_init (EStorageSetViewClass *klass)
{
	GtkObjectClass *object_class;

	parent_class = gtk_type_class (e_table_get_type ());

	object_class = GTK_OBJECT_CLASS (klass);
	object_class->destroy = destroy;

	signals[FOLDER_SELECTED]
		= gtk_signal_new ("folder_selected",
				  GTK_RUN_FIRST,
				  object_class->type,
				  GTK_SIGNAL_OFFSET (EStorageSetViewClass, folder_selected),
				  gtk_marshal_NONE__STRING,
				  GTK_TYPE_NONE, 1,
				  GTK_TYPE_STRING);

	gtk_object_class_add_signals (object_class, signals, LAST_SIGNAL);

	/* Set up DND.  */

	target_list = gtk_target_list_new (drag_types, num_drag_types);
	g_assert (target_list != NULL);
}

static void
init (EStorageSetView *storage_set_view)
{
	EStorageSetViewPrivate *priv;

	priv = g_new (EStorageSetViewPrivate, 1);

	priv->storage_set                    = NULL;
	priv->path_to_etree_node             = g_hash_table_new (g_str_hash, g_str_equal);
	priv->type_name_to_pixbuf            = g_hash_table_new (g_str_hash, g_str_equal);
	priv->selected_row_path              = NULL;

	storage_set_view->priv = priv;
}


static int
folder_compare_cb (gconstpointer a, gconstpointer b)
{
	EFolder *folder_a;
	EFolder *folder_b;
	const char *name_a;
	const char *name_b;

	folder_a = E_FOLDER (a);
	folder_b = E_FOLDER (b);

	name_a = e_folder_get_name (folder_a);
	name_b = e_folder_get_name (folder_b);

	return strcmp (name_a, name_b);
}

static void
insert_folders (EStorageSetView *storage_set_view,
		ETreePath *parent,
		EStorage *storage,
		const char *path)
{
	EStorageSetViewPrivate *priv;
	ETreeModel *etree;
	ETreePath *node;
	GList *folder_list;
	GList *p;
	const char *storage_name;

	priv = storage_set_view->priv;
	etree = priv->etree_model;

	storage_name = e_storage_get_name (storage);

	folder_list = e_storage_list_folders (storage, path);
	if (folder_list == NULL)
		return;

	folder_list = g_list_sort (folder_list, folder_compare_cb);

	for (p = folder_list; p != NULL; p = p->next) {
		EFolder *folder;
		const char *folder_name;
		char *subpath;
		char *full_path;

		folder = E_FOLDER (p->data);
		folder_name = e_folder_get_name (folder);

		subpath = g_concat_dir_and_file (path, folder_name);

		full_path = g_strconcat("/", storage_name, subpath, NULL);

		node = e_tree_model_node_insert (etree, parent, -1, (gpointer)full_path);

		insert_folders (storage_set_view, node, storage, subpath);

		add_node_to_hash (storage_set_view, full_path, node);

		g_free (subpath);
	}

	e_free_object_list (folder_list);
}

static void
right_click (EStorageSetView *storage_set_view, int row, int col,
	     GdkEventButton *event, gboolean *ret)
{
	EStorageSetViewPrivate *priv;

	priv = storage_set_view->priv;

	popup_folder_menu (storage_set_view, event);
}

static void
on_cursor_change (EStorageSetView *storage_set_view, int row, gpointer user_data)
{
	ETreePath *node;
	EStorageSetViewPrivate *priv;

	priv = storage_set_view->priv;

	node = e_tree_model_node_at_row (priv->etree_model, row);

	/* don't emit the folder selected signal for storages */
	if (e_tree_model_node_depth (priv->etree_model, node) < 2)
		return;

	priv->selected_row_path = e_tree_model_node_get_data (priv->etree_model, node);

	gtk_signal_emit (GTK_OBJECT (storage_set_view), signals[FOLDER_SELECTED],
			 priv->selected_row_path);
}

void
e_storage_set_view_construct (EStorageSetView *storage_set_view,
			      EStorageSet *storage_set)
{
	EStorageSetViewPrivate *priv;
	ETreePath *parent;
	EStorage *storage;
	GList *storage_list;
	GList *p;
	const char *name;
	char *text[2];
	char *path;
	ETableCol *ecol;
	ETableHeader *e_table_header;
	ECell *cell_left_just;
	ECell *cell_tree;

	g_return_if_fail (storage_set_view != NULL);
	g_return_if_fail (E_IS_STORAGE_SET_VIEW (storage_set_view));
	g_return_if_fail (storage_set != NULL);
	g_return_if_fail (E_IS_STORAGE_SET (storage_set));

	priv = storage_set_view->priv;

	priv->tree_expanded_pixbuf = gdk_pixbuf_new_from_xpm_data((const char**)tree_expanded_xpm);
	priv->tree_unexpanded_pixbuf = gdk_pixbuf_new_from_xpm_data((const char**)tree_unexpanded_xpm);

	priv->etree_model = e_tree_simple_new (etree_col_count,
					       etree_duplicate_value,
					       etree_free_value,
					       etree_initialize_value,
					       etree_value_is_empty,
					       etree_value_to_string,
					       etree_icon_at,
					       etree_value_at,
					       etree_set_value_at,
					       etree_is_editable,
					       storage_set_view);
	e_tree_model_root_node_set_visible (priv->etree_model, FALSE);

	priv->root_node = e_tree_model_node_insert (priv->etree_model, NULL, -1, "/Root Node");

	e_table_header = e_table_header_new ();
	cell_left_just = e_cell_text_new (E_TABLE_MODEL (priv->etree_model), NULL, GTK_JUSTIFY_LEFT);
	cell_tree = e_cell_tree_new (E_TABLE_MODEL (priv->etree_model),
				     priv->tree_expanded_pixbuf,
				     priv->tree_unexpanded_pixbuf, TRUE, cell_left_just);

	ecol = e_table_col_new (0, "Folder", 80, 20, cell_tree, g_str_compare, TRUE);
	e_table_header_add_column (e_table_header, ecol, 0);

	e_table_construct (E_TABLE (storage_set_view), e_table_header, E_TABLE_MODEL(priv->etree_model),
			   ETABLE_SPEC);

	gtk_object_set (GTK_OBJECT (storage_set_view),
			"cursor_mode", E_TABLE_CURSOR_LINE,
			NULL);

	e_table_drag_source_set (E_TABLE (storage_set_view), GDK_BUTTON1_MASK,
				 drag_types, num_drag_types, GDK_ACTION_MOVE);

	gtk_object_ref (GTK_OBJECT (storage_set));
	priv->storage_set = storage_set;

	gtk_signal_connect_while_alive (GTK_OBJECT (storage_set), "new_storage",
					GTK_SIGNAL_FUNC (new_storage_cb), storage_set_view,
					GTK_OBJECT (storage_set_view));
	gtk_signal_connect_while_alive (GTK_OBJECT (storage_set), "removed_storage",
					GTK_SIGNAL_FUNC (removed_storage_cb), storage_set_view,
					GTK_OBJECT (storage_set_view));
	gtk_signal_connect_while_alive (GTK_OBJECT (storage_set), "new_folder",
					GTK_SIGNAL_FUNC (new_folder_cb), storage_set_view,
					GTK_OBJECT (storage_set_view));
	gtk_signal_connect_while_alive (GTK_OBJECT (storage_set), "removed_folder",
					GTK_SIGNAL_FUNC (removed_folder_cb), storage_set_view,
					GTK_OBJECT (storage_set_view));

	gtk_signal_connect (GTK_OBJECT (storage_set_view), "right_click",
			    GTK_SIGNAL_FUNC (right_click), GTK_OBJECT(storage_set_view));
	gtk_signal_connect (GTK_OBJECT (storage_set_view), "cursor_change",
			    GTK_SIGNAL_FUNC (on_cursor_change), GTK_OBJECT(storage_set_view));
	gtk_signal_connect (GTK_OBJECT (storage_set_view), "table_drag_begin",
			    GTK_SIGNAL_FUNC (etable_drag_begin), GTK_OBJECT(storage_set_view));
	gtk_signal_connect (GTK_OBJECT (storage_set_view), "table_drag_data_get",
			    GTK_SIGNAL_FUNC (etable_drag_data_get), GTK_OBJECT(storage_set_view));

	storage_list = e_storage_set_get_storage_list (storage_set);

	text[1] = NULL;

	for (p = storage_list; p != NULL; p = p->next) {
		storage = E_STORAGE (p->data);

		name = e_storage_get_name (storage);
		path = g_strconcat ("/", name, NULL);

		parent = e_tree_model_node_insert (priv->etree_model, priv->root_node,
						   -1, path);
		e_tree_model_node_set_expanded (priv->etree_model, parent, TRUE);

		g_hash_table_insert (priv->path_to_etree_node, path, parent);

		insert_folders (storage_set_view, parent, storage, "/");
	}

	e_free_object_list (storage_list);
}

GtkWidget *
e_storage_set_view_new (EStorageSet *storage_set)
{
	GtkWidget *new;

	g_return_val_if_fail (storage_set != NULL, NULL);
	g_return_val_if_fail (E_IS_STORAGE_SET (storage_set), NULL);

	new = gtk_type_new (e_storage_set_view_get_type ());
	e_storage_set_view_construct (E_STORAGE_SET_VIEW (new), storage_set);

	return new;
}


void
e_storage_set_view_set_current_folder (EStorageSetView *storage_set_view,
				       const char *path)
{
	EStorageSetViewPrivate *priv;
	ETreePath *node;

	g_return_if_fail (storage_set_view != NULL);
	g_return_if_fail (E_IS_STORAGE_SET_VIEW (storage_set_view));
	g_return_if_fail (path != NULL && g_path_is_absolute (path));

	priv = storage_set_view->priv;

	node = g_hash_table_lookup (priv->path_to_etree_node, path);
	if (node == NULL) {
		return;
	}

	e_table_set_cursor_row (E_TABLE (storage_set_view),
				e_tree_model_row_of_node (priv->etree_model, node));

	gtk_signal_emit (GTK_OBJECT (storage_set_view), signals[FOLDER_SELECTED], path);
}

const char *
e_storage_set_view_get_current_folder (EStorageSetView *storage_set_view)
{
	EStorageSetViewPrivate *priv;
	ETreePath *etree_node;
	const char *path;
	int row;

	g_return_val_if_fail (storage_set_view != NULL, NULL);
	g_return_val_if_fail (E_IS_STORAGE_SET_VIEW (storage_set_view), NULL);

	priv = storage_set_view->priv;

	row = e_table_get_cursor_row (E_TABLE (storage_set_view));
	etree_node = e_tree_model_node_at_row (priv->etree_model, row);

	if (etree_node == NULL)
		return NULL;	/* Mmh? */

	path = (char*)e_tree_model_node_get_data(priv->etree_model, etree_node);

	return path;
}


E_MAKE_TYPE (e_storage_set_view, "EStorageSetView", EStorageSetView, class_init, init, PARENT_TYPE)
