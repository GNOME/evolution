/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* e-storage-set-view.c
 *
 * Copyright (C) 2000, 2001, 2002 Ximian, Inc.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of version 2 of the GNU General Public
 * License as published by the Free Software Foundation.
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

#include "e-storage-set-view.h"

#include "e-util/e-gtk-utils.h"

#include "e-corba-storage.h"
#include "e-icon-factory.h"
#include "e-folder-dnd-bridge.h"
#include "e-shell-constants.h"
#include "e-shell-marshal.h"

#include <gal/util/e-util.h>
#include <gal/widgets/e-gui-utils.h>
#include <gal/e-table/e-tree-memory-callbacks.h>
#include <gal/e-table/e-cell-text.h>
#include <gal/e-table/e-cell-toggle.h>
#include <gal/e-table/e-cell-tree.h>

#include <libgnome/gnome-util.h>
#include <libgnomeui/gnome-popup-menu.h>

#include <bonobo/bonobo-window.h>
#include <bonobo/bonobo-ui-util.h>

#include <gtk/gtksignal.h>

#include <string.h>

#include "check-empty.xpm"
#include "check-filled.xpm"
#include "check-missing.xpm"


static GdkPixbuf *checks [3];


/*#define DEBUG_XML*/

#define ROOT_NODE_NAME "/RootNode"


/* This is used on the source side to define the two basic types that we always
   export.  */
enum _DndTargetTypeIdx {
	E_FOLDER_DND_PATH_TARGET_TYPE_IDX = 0,
	E_SHORTCUT_TARGET_TYPE_IDX = 1
};
typedef enum _DndTargetTypeIdx DndTargetTypeIdx;

#define E_SHORTCUT_TARGET_TYPE     "E-SHORTCUT"


#define PARENT_TYPE E_TREE_TYPE
static ETreeClass *parent_class = NULL;

struct _EStorageSetViewPrivate {
	EStorageSet *storage_set;

	BonoboUIComponent *ui_component;
	BonoboUIContainer *ui_container;

	ETreeModel *etree_model;
	ETreePath root_node;

	GHashTable *path_to_etree_node;

	GHashTable *type_name_to_pixbuf;

	/* Path of the row selected by the latest "cursor_activated" signal.  */
	char *selected_row_path;

	/* Path of the row selected by a right click.  */
	char *right_click_row_path;

	unsigned int show_folders : 1;
	unsigned int show_checkboxes : 1;
	unsigned int allow_dnd : 1;
	unsigned int search_enabled : 1;

	/* The `Evolution::ShellComponentDnd::SourceFolder' interface for the
	   folder we are dragging from, or CORBA_OBJECT_NIL if no dragging is
	   happening.  */
	GNOME_Evolution_ShellComponentDnd_SourceFolder drag_corba_source_interface;

	/* Source context information.  NULL if no dragging is in progress.  */
	GNOME_Evolution_ShellComponentDnd_SourceFolder_Context *drag_corba_source_context;

	/* The data.  */
	GNOME_Evolution_ShellComponentDnd_Data *drag_corba_data;

	GHashTable *checkboxes;

	/* Callback to determine whether the row should have a checkbox or
	   not, when show_checkboxes is TRUE.  */
	EStorageSetViewHasCheckBoxFunc has_checkbox_func;
	void *has_checkbox_func_data;
};


enum {
	FOLDER_SELECTED,
	FOLDER_OPENED,
	DND_ACTION,
	FOLDER_CONTEXT_MENU_POPPING_UP,
	FOLDER_CONTEXT_MENU_POPPED_DOWN,
	CHECKBOXES_CHANGED,
	LAST_SIGNAL
};

static unsigned int signals[LAST_SIGNAL] = { 0 };


/* Forward declarations.  */

static void setup_folder_changed_callbacks (EStorageSetView *storage_set_view,
					    EFolder *folder,
					    const char *path);


/* DND stuff.  */

enum _DndTargetType {
	DND_TARGET_TYPE_URI_LIST,
	DND_TARGET_TYPE_E_SHORTCUT
};
typedef enum _DndTargetType DndTargetType;

#define URI_LIST_TYPE   "text/uri-list"
#define E_SHORTCUT_TYPE "E-SHORTCUT"


/* Sorting callbacks.  */

static int
storage_sort_callback (ETreeMemory *etmm,
		       ETreePath node1,
		       ETreePath node2,
		       void *closure)
{
	char *folder_path_1;
	char *folder_path_2;
	gboolean path_1_local;
	gboolean path_2_local;

	folder_path_1 = e_tree_memory_node_get_data(etmm, node1);
	folder_path_2 = e_tree_memory_node_get_data(etmm, node2);

	/* FIXME bad hack to put the "my evolution" and "local" storages on
	   top.  */

	if (strcmp (folder_path_1, E_PATH_SEPARATOR_S E_SUMMARY_STORAGE_NAME) == 0)
		return -1;
	if (strcmp (folder_path_2, E_PATH_SEPARATOR_S E_SUMMARY_STORAGE_NAME) == 0)
		return +1;
	
	path_1_local = ! strcmp (folder_path_1, E_PATH_SEPARATOR_S E_LOCAL_STORAGE_NAME);
	path_2_local = ! strcmp (folder_path_2, E_PATH_SEPARATOR_S E_LOCAL_STORAGE_NAME);

	if (path_1_local && path_2_local)
		return 0;
	if (path_1_local)
		return -1;
	if (path_2_local)
		return 1;
	
	return g_utf8_collate (e_tree_model_value_at (E_TREE_MODEL (etmm), node1, 0),
	                       e_tree_model_value_at (E_TREE_MODEL (etmm), node2, 0));
}

static int
folder_sort_callback (ETreeMemory *etmm,
		      ETreePath node1,
		      ETreePath node2,
		      void *closure)
{
	EStorageSetViewPrivate *priv;
	EFolder *folder_1, *folder_2;
	const char *folder_path_1, *folder_path_2;
	int priority_1, priority_2;

	priv = E_STORAGE_SET_VIEW (closure)->priv;

	folder_path_1 = e_tree_memory_node_get_data(etmm, node1);
	folder_path_2 = e_tree_memory_node_get_data(etmm, node2);

	folder_1 = e_storage_set_get_folder (priv->storage_set, folder_path_1);
	folder_2 = e_storage_set_get_folder (priv->storage_set, folder_path_2);

	priority_1 = e_folder_get_sorting_priority (folder_1);
	priority_2 = e_folder_get_sorting_priority (folder_2);

	if (priority_1 == priority_2)
		return g_utf8_collate (e_tree_model_value_at (E_TREE_MODEL (etmm), node1, 0),
				       e_tree_model_value_at (E_TREE_MODEL (etmm), node2, 0));
	else if (priority_1 < priority_2)
		return -1;
	else			/* priority_1 > priority_2 */
		return +1;
}


/* Helper functions.  */

static gboolean
add_node_to_hash (EStorageSetView *storage_set_view,
		  const char *path,
		  ETreePath node)
{
	EStorageSetViewPrivate *priv;
	char *hash_path;

	g_return_val_if_fail (g_path_is_absolute (path), FALSE);

	priv = storage_set_view->priv;

	if (g_hash_table_lookup (priv->path_to_etree_node, path) != NULL) {
		g_warning ("EStorageSetView: Node already existing while adding -- %s", path);
		return FALSE;
	}

	hash_path = g_strdup (path);

	g_hash_table_insert (priv->path_to_etree_node, hash_path, node);

	return TRUE;
}

static ETreePath
lookup_node_in_hash (EStorageSetView *storage_set_view,
		     const char *path)
{
	EStorageSetViewPrivate *priv;
	ETreePath node;

	priv = storage_set_view->priv;

	node = g_hash_table_lookup (priv->path_to_etree_node, path);
	if (node == NULL)
		g_warning ("EStorageSetView: Node not found while updating -- %s", path);

	return node;
}

static ETreePath
remove_node_from_hash (EStorageSetView *storage_set_view,
		       const char *path)
{
	EStorageSetViewPrivate *priv;
	ETreePath node;

	priv = storage_set_view->priv;

	node = g_hash_table_lookup (priv->path_to_etree_node, path);
	if (node == NULL) {
		g_warning ("EStorageSetView: Node not found while removing -- %s", path);
		return NULL;
	}

	g_hash_table_remove (priv->path_to_etree_node, path);

	return node;
}

static GdkPixbuf *
get_pixbuf_for_folder (EStorageSetView *storage_set_view,
		       EFolder *folder)
{
	const char *type_name;
	EStorageSetViewPrivate *priv;
	EFolderTypeRegistry *folder_type_registry;
	EStorageSet *storage_set;
	GdkPixbuf *icon_pixbuf;
	GdkPixbuf *scaled_pixbuf;
	const char *custom_icon_name;
	int icon_pixbuf_width, icon_pixbuf_height;

	priv = storage_set_view->priv;

	custom_icon_name = e_folder_get_custom_icon_name (folder);
	if (custom_icon_name != NULL)
		return e_icon_factory_get_icon (custom_icon_name, TRUE);

	type_name = e_folder_get_type_string (folder);

	scaled_pixbuf = g_hash_table_lookup (priv->type_name_to_pixbuf, type_name);
	if (scaled_pixbuf != NULL)
		return scaled_pixbuf;

	storage_set = priv->storage_set;
	folder_type_registry = e_storage_set_get_folder_type_registry (storage_set);

	icon_pixbuf = e_folder_type_registry_get_icon_for_type (folder_type_registry,
								type_name, TRUE);

	if (icon_pixbuf == NULL)
		return NULL;

	icon_pixbuf_width = gdk_pixbuf_get_width (icon_pixbuf);
	icon_pixbuf_height = gdk_pixbuf_get_height (icon_pixbuf);

	if (icon_pixbuf_width == E_SHELL_MINI_ICON_SIZE && icon_pixbuf_height == E_SHELL_MINI_ICON_SIZE) {
		scaled_pixbuf = g_object_ref (icon_pixbuf);
	} else {
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
	}

	g_hash_table_insert (priv->type_name_to_pixbuf, g_strdup (type_name), scaled_pixbuf);

	return scaled_pixbuf;
}

static EFolder *
get_folder_at_node (EStorageSetView *storage_set_view,
		    ETreePath path)
{
	EStorageSetViewPrivate *priv;
	const char *folder_path;

	priv = storage_set_view->priv;

	if (path == NULL)
		return NULL;

	folder_path = e_tree_memory_node_get_data (E_TREE_MEMORY(priv->etree_model), path);
	g_assert (folder_path != NULL);

	return e_storage_set_get_folder (priv->storage_set, folder_path);
}

static EvolutionShellComponentClient *
get_component_at_node (EStorageSetView *storage_set_view,
		       ETreePath path)
{
	EStorageSetViewPrivate *priv;
	EvolutionShellComponentClient *component_client;
	EFolderTypeRegistry *folder_type_registry;
	EFolder *folder;

	priv = storage_set_view->priv;

	folder = get_folder_at_node (storage_set_view, path);
	if (folder == NULL)
		return NULL;

	folder_type_registry = e_storage_set_get_folder_type_registry (priv->storage_set);
	g_assert (folder_type_registry != NULL);

	component_client = e_folder_type_registry_get_handler_for_type (folder_type_registry,
									e_folder_get_type_string (folder));

	return component_client;
}

static GNOME_Evolution_ShellComponentDnd_ActionSet
convert_gdk_drag_action_set_to_corba (GdkDragAction action)
{
	GNOME_Evolution_ShellComponentDnd_Action retval;

	retval = GNOME_Evolution_ShellComponentDnd_ACTION_DEFAULT;

	if (action & GDK_ACTION_COPY)
		retval |= GNOME_Evolution_ShellComponentDnd_ACTION_COPY;
	if (action & GDK_ACTION_MOVE)
		retval |= GNOME_Evolution_ShellComponentDnd_ACTION_MOVE;
	if (action & GDK_ACTION_LINK)
		retval |= GNOME_Evolution_ShellComponentDnd_ACTION_LINK;
	if (action & GDK_ACTION_ASK)
		retval |= GNOME_Evolution_ShellComponentDnd_ACTION_ASK;

	return retval;
}


/* The weakref callback for priv->ui_component.  */

static void
ui_container_destroy_notify (void *data,
			     GObject *where_the_object_was)
{
	EStorageSetViewPrivate *priv  = (EStorageSetViewPrivate *) data;

	priv->ui_container = NULL;
}	


/* DnD selection setup stuff.  */

/* This will create an array of GtkTargetEntries from the specified list of DND
   types.  The type name will *not* be allocated in the list, as this is
   supposed to be used only temporarily to set up the cell as a drag source.  */
static GtkTargetEntry *
create_target_entries_from_dnd_type_list (GList *dnd_types,
					  int *num_entries_return)
{
	GtkTargetEntry *entries;
	GList *p;
	int num_entries;
	int i;

	if (dnd_types == NULL)
		num_entries = 0;
	else
		num_entries = g_list_length (dnd_types);

	/* We always add two entries, one for an Evolution URI type, and one
	   for e-shortcuts.  This will let us do drag & drop within Evolution
	   at least.  */
	num_entries += 2;

	entries = g_new (GtkTargetEntry, num_entries);

	i = 0;

	/* The Evolution URI will always come first.  */
	entries[i].target = E_FOLDER_DND_PATH_TARGET_TYPE;
	entries[i].flags = 0;
	entries[i].info = i;
	g_assert (i == E_FOLDER_DND_PATH_TARGET_TYPE_IDX);
	i ++;

	/* ...Then the shortcut type.  */
	entries[i].target = E_SHORTCUT_TARGET_TYPE;
	entries[i].flags = 0;
	entries[i].info = i;
	g_assert (i == E_SHORTCUT_TARGET_TYPE_IDX);
	i ++;

	for (p = dnd_types; p != NULL; p = p->next, i++) {
		const char *dnd_type;

		g_assert (i < num_entries);

		dnd_type = (const char *) p->data;

		entries[i].target = (char *) dnd_type;
		entries[i].flags  = 0;
		entries[i].info   = i;
	}

	*num_entries_return = num_entries;
	return entries;
}

static void
free_target_entries (GtkTargetEntry *entries)
{
	g_assert (entries != NULL);

	/* The target names are not strdup()ed so a simple free will do.  */
	g_free (entries);
}

static GtkTargetList *
create_target_list_for_node (EStorageSetView *storage_set_view,
			     ETreePath node)
{
	EStorageSetViewPrivate *priv;
	GtkTargetList *target_list;
	EFolderTypeRegistry *folder_type_registry;
	GList *exported_dnd_types;
	GtkTargetEntry *target_entries;
	EFolder *folder;
	const char *folder_type;
	int num_target_entries;

	priv = storage_set_view->priv;

	folder_type_registry = e_storage_set_get_folder_type_registry (priv->storage_set);

	folder = get_folder_at_node (storage_set_view, node);
	folder_type = e_folder_get_type_string (folder);

	exported_dnd_types = e_folder_type_registry_get_exported_dnd_types_for_type (folder_type_registry,
										     folder_type);

	target_entries = create_target_entries_from_dnd_type_list (exported_dnd_types,
								   &num_target_entries);
	g_assert (target_entries != NULL);

	target_list = gtk_target_list_new (target_entries, num_target_entries);

	free_target_entries (target_entries);

	return target_list;
}

static void
set_e_shortcut_selection (EStorageSetView *storage_set_view,
			  GtkSelectionData *selection_data)
{
	EStorageSetViewPrivate *priv;
	ETreePath node;
	EFolder *folder;
	int shortcut_len;
	char *shortcut;
	const char *name;
	const char *folder_path;

	g_assert (storage_set_view != NULL);
	g_assert (selection_data != NULL);

	priv = storage_set_view->priv;

	node = lookup_node_in_hash (storage_set_view, priv->selected_row_path);

	folder_path = e_tree_memory_node_get_data (E_TREE_MEMORY(priv->etree_model), node);
	g_assert (folder_path != NULL);

	folder = e_storage_set_get_folder (priv->storage_set, folder_path);
	if (folder != NULL)
		name = e_folder_get_name (folder);
	else
		name = NULL;

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
set_evolution_path_selection (EStorageSetView *storage_set_view,
			      GtkSelectionData *selection_data)
{
	EStorageSetViewPrivate *priv;

	g_assert (storage_set_view != NULL);
	g_assert (selection_data != NULL);

	priv = storage_set_view->priv;

	gtk_selection_data_set (selection_data, selection_data->target,
				8, (guchar *) priv->selected_row_path, strlen (priv->selected_row_path) + 1);
}


/* Folder context menu.  */

struct _FolderPropertyItemsData {
	EStorageSetView *storage_set_view;
	ECorbaStorage *corba_storage;
	int num_items;
};
typedef struct _FolderPropertyItemsData FolderPropertyItemsData;

static void
folder_property_item_verb_callback    (BonoboUIComponent *component,
				       void *user_data,
				       const char *cname)
{
	FolderPropertyItemsData *data;
	GtkWidget *toplevel_widget;
	const char *p, *path;
	int item_number;

	data = (FolderPropertyItemsData *) user_data;

	p = strrchr (cname, ':');
	g_assert (p != NULL);

	item_number = atoi (p + 1) - 1;
	g_assert (item_number >= 0);

	toplevel_widget = gtk_widget_get_toplevel (GTK_WIDGET (data->storage_set_view));

	path = strchr (data->storage_set_view->priv->right_click_row_path + 1, E_PATH_SEPARATOR);
	if (path == NULL)
		path = "/";
	e_corba_storage_show_folder_properties (data->corba_storage, path,
						item_number, toplevel_widget->window);
}

static FolderPropertyItemsData *
setup_folder_properties_items_if_corba_storage_clicked (EStorageSetView *storage_set_view)
{
	EStorageSetViewPrivate *priv;
	EStorage *storage;
	GSList *items, *p;
	GString *xml;
	FolderPropertyItemsData *data;
	const char *slash;
	char *storage_name;
	int num_property_items;
	int i;

	priv = storage_set_view->priv;

	slash = strchr (priv->right_click_row_path + 1, E_PATH_SEPARATOR);
	if (slash == NULL)
		storage_name = g_strdup (priv->right_click_row_path + 1);

	else
		storage_name = g_strndup (priv->right_click_row_path + 1,
					  slash - (priv->right_click_row_path + 1));

	storage = e_storage_set_get_storage (priv->storage_set, storage_name);
	g_free (storage_name);

	if (storage == NULL || ! E_IS_CORBA_STORAGE (storage))
		return 0;

	items = e_corba_storage_get_folder_property_items (E_CORBA_STORAGE (storage));
	if (items == NULL)
		return 0;

	xml = g_string_new ("<placeholder name=\"StorageFolderPropertiesPlaceholder\">");
	g_string_append (xml, "<separator f=\"\" name=\"EStorageSetViewFolderPropertiesSeparator\"/>");

	num_property_items = 0;
	for (p = items; p != NULL; p = p->next) {
		const ECorbaStoragePropertyItem *item;
		char *encoded_label;
		char *encoded_tooltip;

		item = (const ECorbaStoragePropertyItem *) p->data;
		num_property_items ++;

		g_string_append_printf (xml, "<menuitem name=\"EStorageSetView:FolderPropertyItem:%d\"",
					num_property_items);
		g_string_append_printf (xml, " verb=\"EStorageSetView:FolderPropertyItem:%d\"",
					num_property_items);

		encoded_tooltip = bonobo_ui_util_encode_str (item->tooltip);
		g_string_append_printf (xml, " tip=\"%s\"", encoded_tooltip);

		encoded_label = bonobo_ui_util_encode_str (item->label);
		g_string_append_printf (xml, " label=\"%s\"/>", encoded_label);

		g_free (encoded_tooltip);
		g_free (encoded_label);
	}

	g_string_append (xml, "</placeholder>");

	data = g_new (FolderPropertyItemsData, 1);
	data->storage_set_view = storage_set_view;
	data->corba_storage    = E_CORBA_STORAGE (storage);
	data->num_items        = num_property_items;

	g_object_ref (data->storage_set_view);
	g_object_ref (data->corba_storage);

	for (i = 1; i <= num_property_items; i ++) {
		char *verb;

		verb = g_strdup_printf ("EStorageSetView:FolderPropertyItem:%d", i);
		bonobo_ui_component_add_verb (priv->ui_component, verb,
					      folder_property_item_verb_callback,
					      data);
	}

	bonobo_ui_component_set (priv->ui_component, "/popups/FolderPopup", xml->str, NULL);

	g_string_free (xml, TRUE);
	e_corba_storage_free_property_items_list (items);

	return data;
}

static void
remove_property_items (EStorageSetView *storage_set_view,
		       FolderPropertyItemsData *data)
{
	EStorageSetViewPrivate *priv;

	priv = storage_set_view->priv;

	if (data->num_items > 0) {
		int i;

		bonobo_ui_component_rm (priv->ui_component, 
					"/popups/FolderPopup/StorageFolderPropertiesPlaceholder/EStorageSetViewFolderPropertiesSeparator",
					NULL);

		for (i = 1; i <= data->num_items; i ++) {
			char *path;
			char *verb;

			path = g_strdup_printf ("/popups/FolderPopup/StorageFolderPropertiesPlaceholder/EStorageSetView:FolderPropertyItem:%d", i);
			bonobo_ui_component_rm (priv->ui_component, path, NULL);
			g_free (path);

			verb = g_strdup_printf ("EStorageSetView:FolderPropertyItem:%d", i);
			bonobo_ui_component_remove_verb (priv->ui_component, verb);
			g_free (verb);
		}
	}

	g_object_unref (data->storage_set_view);
	g_object_unref (data->corba_storage);

	g_free (data);
}

static void
popup_folder_menu (EStorageSetView *storage_set_view,
		   GdkEventButton *event)
{
	EvolutionShellComponentClient *handler;
	EStorageSetViewPrivate *priv;
	EFolderTypeRegistry *folder_type_registry;
	EFolder *folder;
	GtkWidget *menu, *window;
	FolderPropertyItemsData *folder_property_items_data;

	priv = storage_set_view->priv;

	folder = e_storage_set_get_folder (priv->storage_set, priv->right_click_row_path);
	g_object_ref (folder);

	folder_type_registry = e_storage_set_get_folder_type_registry (priv->storage_set);
	g_assert (folder_type_registry != NULL);

	handler = e_folder_type_registry_get_handler_for_type (folder_type_registry,
							       e_folder_get_type_string (folder));
	menu = gtk_menu_new ();

	window = gtk_widget_get_ancestor (GTK_WIDGET (storage_set_view),
					  BONOBO_TYPE_WINDOW);
	bonobo_window_add_popup (BONOBO_WINDOW (window),
				 GTK_MENU (menu), "/popups/FolderPopup");

	bonobo_ui_component_set (priv->ui_component,
				 "/popups/FolderPopup/ComponentPlaceholder",
				 "<placeholder name=\"Items\"/>", NULL);

	if (handler != NULL)
		evolution_shell_component_client_populate_folder_context_menu (handler,
									       priv->ui_container,
									       e_folder_get_physical_uri (folder),
									       e_folder_get_type_string (folder));

	folder_property_items_data = setup_folder_properties_items_if_corba_storage_clicked (storage_set_view);

	gtk_widget_show (GTK_WIDGET (menu));

	gnome_popup_menu_do_popup_modal (GTK_WIDGET (menu), NULL, NULL, event, NULL,
					 GTK_WIDGET (storage_set_view));

	if (folder_property_items_data != NULL)
		remove_property_items (storage_set_view, folder_property_items_data);

	if (handler != NULL)
		evolution_shell_component_client_unpopulate_folder_context_menu (handler,
										 priv->ui_container,
										 e_folder_get_physical_uri (folder),
										 e_folder_get_type_string (folder));

	g_object_unref (folder);
	gtk_widget_destroy (GTK_WIDGET (menu));

	e_tree_right_click_up (E_TREE (storage_set_view));
}


/* GtkObject methods.  */

static void
pixbuf_free_func (gpointer key, gpointer value, gpointer user_data)
{
	g_free (key);
	g_object_unref ((GdkPixbuf*)value);
}

static void
impl_dispose (GObject *object)
{
	EStorageSetView *storage_set_view;
	EStorageSetViewPrivate *priv;

	storage_set_view = E_STORAGE_SET_VIEW (object);
	priv = storage_set_view->priv;

	if (priv->etree_model != NULL) {
		/* Destroy the tree.  */
		e_tree_memory_node_remove (E_TREE_MEMORY(priv->etree_model), priv->root_node);
		g_object_unref (priv->etree_model);
		priv->etree_model = NULL;

		/* (The data in the hash table was all freed by freeing the tree.)  */
		g_hash_table_destroy (priv->path_to_etree_node);
		priv->path_to_etree_node = NULL;
	}

	if (priv->storage_set != NULL) {
		g_object_unref (priv->storage_set);
		priv->storage_set = NULL;
	}

	if (priv->drag_corba_source_interface != CORBA_OBJECT_NIL) {
		CORBA_Environment ev;

		CORBA_exception_init (&ev);

		g_assert (priv->drag_corba_source_context != NULL);

		GNOME_Evolution_ShellComponentDnd_SourceFolder_endDrag (priv->drag_corba_source_interface,
									priv->drag_corba_source_context,
									&ev);

		Bonobo_Unknown_unref (priv->drag_corba_source_interface, &ev);
		CORBA_Object_release (priv->drag_corba_source_interface, &ev);

		CORBA_exception_free (&ev);

		priv->drag_corba_source_interface = CORBA_OBJECT_NIL;
	}

	if (priv->ui_component != NULL) {
		bonobo_object_unref (BONOBO_OBJECT (priv->ui_component));
		priv->ui_component = NULL;
	}

	/* (No unreffing for priv->ui_container since we use a weakref.)  */

	(* G_OBJECT_CLASS (parent_class)->dispose) (object);
}

static void
impl_finalize (GObject *object)
{
	EStorageSetView *storage_set_view;
	EStorageSetViewPrivate *priv;

	storage_set_view = E_STORAGE_SET_VIEW (object);
	priv = storage_set_view->priv;

	g_hash_table_foreach (priv->type_name_to_pixbuf, pixbuf_free_func, NULL);
	g_hash_table_destroy (priv->type_name_to_pixbuf);

	if (priv->checkboxes != NULL) {
		g_hash_table_foreach (priv->checkboxes, (GHFunc) g_free, NULL);
		g_hash_table_destroy (priv->checkboxes);
	}

	if (priv->drag_corba_source_context != NULL)
		CORBA_free (priv->drag_corba_source_context);

	if (priv->drag_corba_data != NULL)
		CORBA_free (priv->drag_corba_data);

	g_free (priv->selected_row_path);
	g_free (priv->right_click_row_path);

	g_free (priv);

	(* G_OBJECT_CLASS (parent_class)->finalize) (object);
}


/* ETree methods.  */

/* -- Source-side DnD.  */

static gint
impl_tree_start_drag (ETree *tree,
		      int row,
		      ETreePath path,
		      int col,
		      GdkEvent *event)
{
	GdkDragContext *context;
	GtkTargetList *target_list;
	GdkDragAction actions;
	EStorageSetView *storage_set_view;

	storage_set_view = E_STORAGE_SET_VIEW (tree);

	if (! storage_set_view->priv->allow_dnd)
		return FALSE;

	target_list = create_target_list_for_node (storage_set_view, path);
	if (target_list == NULL)
		return FALSE;

	actions = GDK_ACTION_MOVE | GDK_ACTION_COPY;

	context = e_tree_drag_begin (tree, row, col,
				     target_list,
				     actions,
				     1, event);

	gtk_drag_set_icon_default (context);

	gtk_target_list_unref (target_list);

	return TRUE;
}

static void
impl_tree_drag_begin (ETree *etree,
		      int row,
		      ETreePath path,
		      int col,
		      GdkDragContext *context)
{
	EStorageSetView *storage_set_view;
	EStorageSetViewPrivate *priv;
	EFolder *folder;
	EvolutionShellComponentClient *component_client;
	GNOME_Evolution_ShellComponent corba_component;
	GNOME_Evolution_ShellComponentDnd_ActionSet possible_actions;
	GNOME_Evolution_ShellComponentDnd_Action suggested_action;
	CORBA_Environment ev;

	storage_set_view = E_STORAGE_SET_VIEW (etree);
	priv = storage_set_view->priv;

	g_free (priv->selected_row_path);
	priv->selected_row_path = g_strdup (e_tree_memory_node_get_data (E_TREE_MEMORY(priv->etree_model), path));

	g_assert (priv->drag_corba_source_interface == CORBA_OBJECT_NIL);

	folder = get_folder_at_node (storage_set_view, path);
	component_client = get_component_at_node (storage_set_view, path);

	if (component_client == NULL)
		return;

	/* Query the `ShellComponentDnd::SourceFolder' interface on the
	   component.  */
	/* FIXME we could use the new
	   `evolution_shell_component_client_get_dnd_source_interface()'
	   call. */

	CORBA_exception_init (&ev);

	corba_component = evolution_shell_component_client_corba_objref (component_client);
	priv->drag_corba_source_interface = Bonobo_Unknown_queryInterface (corba_component,
									   "IDL:GNOME/Evolution/ShellComponentDnd/SourceFolder:1.0",
									   &ev);
	if (ev._major != CORBA_NO_EXCEPTION
	    || priv->drag_corba_source_interface == CORBA_OBJECT_NIL) {
		priv->drag_corba_source_interface = CORBA_OBJECT_NIL;

		CORBA_exception_free (&ev);
		return;
	}

	GNOME_Evolution_ShellComponentDnd_SourceFolder_beginDrag (priv->drag_corba_source_interface,
								  e_folder_get_physical_uri (folder),
								  e_folder_get_type_string (folder),
								  &possible_actions,
								  &suggested_action,
								  &ev);

	if (ev._major != CORBA_NO_EXCEPTION) {
		Bonobo_Unknown_unref (priv->drag_corba_source_interface, &ev);
		CORBA_Object_release (priv->drag_corba_source_interface, &ev);

		priv->drag_corba_source_interface = CORBA_OBJECT_NIL;

		CORBA_exception_free (&ev);
		return;
	}

	CORBA_exception_free (&ev);

	if (priv->drag_corba_source_context != NULL)
		CORBA_free (priv->drag_corba_source_context);

	priv->drag_corba_source_context = GNOME_Evolution_ShellComponentDnd_SourceFolder_Context__alloc ();
	priv->drag_corba_source_context->physicalUri     = CORBA_string_dup (e_folder_get_physical_uri (folder));
	priv->drag_corba_source_context->folderType      = CORBA_string_dup (e_folder_get_type_string (folder));
	priv->drag_corba_source_context->possibleActions = possible_actions;
	priv->drag_corba_source_context->suggestedAction = suggested_action;
}

static void
impl_tree_drag_end (ETree *tree,
		    int row,
		    ETreePath path,
		    int col,
		    GdkDragContext *context)
{
	EStorageSetView *storage_set_view;
	EStorageSetViewPrivate *priv;
	CORBA_Environment ev;

	storage_set_view = E_STORAGE_SET_VIEW (tree);
	priv = storage_set_view->priv;

	if (priv->drag_corba_source_interface == CORBA_OBJECT_NIL)
		return;

	CORBA_exception_init (&ev);

	GNOME_Evolution_ShellComponentDnd_SourceFolder_endDrag (priv->drag_corba_source_interface,
								priv->drag_corba_source_context,
								&ev);

	CORBA_free (priv->drag_corba_source_context);
	priv->drag_corba_source_context = NULL;

	Bonobo_Unknown_unref (priv->drag_corba_source_interface, &ev);
	CORBA_Object_release (priv->drag_corba_source_interface, &ev);

	CORBA_exception_free (&ev);
}

static void
impl_tree_drag_data_get (ETree *etree,
			 int drag_row,
			 ETreePath drag_path,
			 int drag_col,
			 GdkDragContext *context,
			 GtkSelectionData *selection_data,
			 unsigned int info,
			 guint32 time)
{
	EStorageSetView *storage_set_view;
	EStorageSetViewPrivate *priv;
	CORBA_Environment ev;
	char *target_type;

	storage_set_view = E_STORAGE_SET_VIEW (etree);
	priv = storage_set_view->priv;

	switch (info) {
	case E_SHORTCUT_TARGET_TYPE_IDX:
		set_e_shortcut_selection (storage_set_view, selection_data);
		return;
	case E_FOLDER_DND_PATH_TARGET_TYPE_IDX:
		set_evolution_path_selection (storage_set_view, selection_data);
		return;
	}

	g_assert (info > 0);

	if (priv->drag_corba_source_interface == CORBA_OBJECT_NIL)
		return;

	target_type = gdk_atom_name ((GdkAtom) context->targets->data);

	CORBA_exception_init (&ev);

	GNOME_Evolution_ShellComponentDnd_SourceFolder_getData (priv->drag_corba_source_interface,
								priv->drag_corba_source_context,
								convert_gdk_drag_action_set_to_corba (context->action),
								target_type,
								& priv->drag_corba_data,
								&ev);

	if (ev._major != CORBA_NO_EXCEPTION)
		gtk_selection_data_set (selection_data,
					selection_data->target, 8, "", -1);
	else
		gtk_selection_data_set (selection_data,
					gdk_atom_intern (priv->drag_corba_data->target, FALSE),
					priv->drag_corba_data->format,
					priv->drag_corba_data->bytes._buffer,
					priv->drag_corba_data->bytes._length);

	g_free (target_type);

	CORBA_exception_free (&ev);
}

static void
impl_tree_drag_data_delete (ETree *tree,
			    int row,
			    ETreePath path,
			    int col,
			    GdkDragContext *context)
{
	EStorageSetView *storage_set_view;
	EStorageSetViewPrivate *priv;
	CORBA_Environment ev;

	storage_set_view = E_STORAGE_SET_VIEW (tree);
	priv = storage_set_view->priv;

	if (priv->drag_corba_source_interface == CORBA_OBJECT_NIL)
		return;

	CORBA_exception_init (&ev);

	GNOME_Evolution_ShellComponentDnd_SourceFolder_deleteData (priv->drag_corba_source_interface,
								   priv->drag_corba_source_context,
								   &ev);

	CORBA_exception_free (&ev);
}

/* -- Destination-side DnD.  */

static gboolean
impl_tree_drag_motion (ETree *tree,
		       int row,
		       ETreePath path,
		       int col,
		       GdkDragContext *context,
		       int x,
		       int y,
		       unsigned int time)
{
	EStorageSetView *storage_set_view;
	EStorageSetViewPrivate *priv;
	const char *folder_path;

	storage_set_view = E_STORAGE_SET_VIEW (tree);
	priv = storage_set_view->priv;

	if (! priv->allow_dnd)
		return FALSE;

	folder_path = e_tree_memory_node_get_data (E_TREE_MEMORY (priv->etree_model),
						   e_tree_node_at_row (E_TREE (storage_set_view), row));
	if (folder_path == NULL)
		return FALSE;

	e_tree_drag_highlight (E_TREE (storage_set_view), row, -1);

	return e_folder_dnd_bridge_motion (GTK_WIDGET (storage_set_view), context, time,
					   priv->storage_set, folder_path);
}

static void
impl_tree_drag_leave (ETree *etree,
		      int row,
		      ETreePath path,
		      int col,
		      GdkDragContext *context,
		      unsigned int time)
{
	e_tree_drag_unhighlight (etree);
}

static gboolean
impl_tree_drag_drop (ETree *etree,
		     int row,
		     ETreePath path,
		     int col,
		     GdkDragContext *context,
		     int x,
		     int y,
		     unsigned int time)
{
	EStorageSetView *storage_set_view;
	EStorageSetViewPrivate *priv;
	const char *folder_path;

	storage_set_view = E_STORAGE_SET_VIEW (etree);
	priv = storage_set_view->priv;

	e_tree_drag_unhighlight (etree);

	folder_path = e_tree_memory_node_get_data (E_TREE_MEMORY (priv->etree_model),
						   e_tree_node_at_row (E_TREE (storage_set_view), row));
	if (folder_path == NULL)
		return FALSE;

	return e_folder_dnd_bridge_drop (GTK_WIDGET (etree), context, time,
					 priv->storage_set, folder_path);
}

static void
impl_tree_drag_data_received (ETree *etree,
			      int row,
			      ETreePath path,
			      int col,
			      GdkDragContext *context,
			      int x,
			      int y,
			      GtkSelectionData *selection_data,
			      unsigned int info,
			      unsigned int time)
{
	EStorageSetView *storage_set_view;
	EStorageSetViewPrivate *priv;
	const char *folder_path;

	storage_set_view = E_STORAGE_SET_VIEW (etree);
	priv = storage_set_view->priv;

	folder_path = e_tree_memory_node_get_data (E_TREE_MEMORY (priv->etree_model),
						   e_tree_node_at_row (E_TREE (storage_set_view), row));
	if (path == NULL) {
		gtk_drag_finish (context, FALSE, FALSE, time);
		return;
	}

	e_folder_dnd_bridge_data_received  (GTK_WIDGET (etree),
					    context,
					    selection_data,
					    time,
					    priv->storage_set,
					    folder_path);
}

static gboolean
impl_right_click (ETree *etree,
		  int row,
		  ETreePath path,
		  int col,
		  GdkEvent *event)
{
	EStorageSetView *storage_set_view;
	EStorageSetViewPrivate *priv;

	storage_set_view = E_STORAGE_SET_VIEW (etree);
	priv = storage_set_view->priv;

	/* This should never happen, but you never know with ETree.  */
	if (priv->right_click_row_path != NULL)
		g_free (priv->right_click_row_path);
	priv->right_click_row_path = g_strdup (e_tree_memory_node_get_data (E_TREE_MEMORY(priv->etree_model), path));

	if (priv->ui_container) {
		g_signal_emit (storage_set_view, signals[FOLDER_CONTEXT_MENU_POPPING_UP], 0, priv->right_click_row_path);

		popup_folder_menu (storage_set_view, (GdkEventButton *) event);

		g_signal_emit (storage_set_view, signals[FOLDER_CONTEXT_MENU_POPPED_DOWN], 0);
	}

	g_free (priv->right_click_row_path);
	priv->right_click_row_path = NULL;

	return TRUE;
}

static void
impl_cursor_activated (ETree *tree,
		       int row,
		       ETreePath path)
{
	EStorageSetView *storage_set_view;
	EStorageSetViewPrivate *priv;

	storage_set_view = E_STORAGE_SET_VIEW (tree);

	priv = storage_set_view->priv;

	g_free (priv->selected_row_path);
	if (path) {
		priv->selected_row_path = g_strdup (e_tree_memory_node_get_data (E_TREE_MEMORY (priv->etree_model), path));

		g_signal_emit (storage_set_view, signals[FOLDER_SELECTED], 0,
			       priv->selected_row_path);
	}
	else
		priv->selected_row_path = NULL;
}


/* ETreeModel Methods */

static gboolean
path_is_storage (ETreeModel *etree,
		 ETreePath tree_path)
{
	return e_tree_model_node_depth (etree, tree_path) == 1;
}

static GdkPixbuf*
etree_icon_at (ETreeModel *etree,
	       ETreePath tree_path,
	       void *model_data)
{
	EStorageSetView *storage_set_view;
	EStorageSet *storage_set;
	EFolder *folder;
	char *path;

	storage_set_view = E_STORAGE_SET_VIEW (model_data);
	storage_set = storage_set_view->priv->storage_set;

	path = (char*) e_tree_memory_node_get_data (E_TREE_MEMORY(etree), tree_path);

	folder = e_storage_set_get_folder (storage_set, path);
	if (folder == NULL)
		return NULL;
		
	/* No icon for a storage with children (or with no real root folder) */
	if (path_is_storage (etree, tree_path)) {
		EStorage *storage;
		GList *subfolder_paths;

		if (! strcmp (e_folder_get_type_string (folder), "noselect"))
			return NULL;

		storage = e_storage_set_get_storage (storage_set, path + 1);
		subfolder_paths = e_storage_get_subfolder_paths (storage, "/");
		if (subfolder_paths != NULL) {
			e_free_string_list (subfolder_paths);
			return NULL;
		}
	}

	return get_pixbuf_for_folder (storage_set_view, folder);
}

/* This function returns the number of columns in our ETreeModel. */
static int
etree_column_count (ETreeModel *etc,
		    void *data)
{
	return 3;
}

static gboolean
etree_has_save_id (ETreeModel *etm,
		   void *data)
{
	return TRUE;
}

static gchar *
etree_get_save_id (ETreeModel *etm,
		   ETreePath node,
		   void *model_data)
{
	return g_strdup(e_tree_memory_node_get_data (E_TREE_MEMORY(etm), node));
}

static gboolean
etree_has_get_node_by_id (ETreeModel *etm,
			  void *data)
{
	return TRUE;
}

static ETreePath
etree_get_node_by_id (ETreeModel *etm,
		      const char *save_id,
		      void *model_data)
{
	EStorageSetView *storage_set_view;
	storage_set_view = E_STORAGE_SET_VIEW (model_data);

	return g_hash_table_lookup (storage_set_view->priv->path_to_etree_node, save_id);
}

static gboolean
has_checkbox (EStorageSetView *storage_set_view, ETreePath tree_path)
{
	EStorageSetViewPrivate *priv;
	const char *folder_path;

	priv = storage_set_view->priv;

	folder_path = e_tree_memory_node_get_data (E_TREE_MEMORY(storage_set_view->priv->etree_model),
						   tree_path);
	g_assert (folder_path != NULL);

	if (strchr (folder_path + 1, '/') == NULL) {
		/* If it's a toplevel, never allow checking it.  */
		return FALSE;
	}

	if (priv->has_checkbox_func)
		return (* priv->has_checkbox_func) (priv->storage_set,
						    folder_path,
						    priv->has_checkbox_func_data);

	return TRUE;
}

static void *
etree_value_at (ETreeModel *etree,
		ETreePath tree_path,
		int col,
		void *model_data)
{
	EStorageSetView *storage_set_view;
	EStorageSetViewPrivate *priv;
	EStorageSet *storage_set;
	EFolder *folder;
	char *path;
	const char *folder_name;
	int unread_count;

	storage_set_view = E_STORAGE_SET_VIEW (model_data);
	priv = storage_set_view->priv;
	storage_set = priv->storage_set;

	/* Storages are always highlighted. */
	if (col == 1 && path_is_storage (etree, tree_path))
		return (void *) TRUE;

	path = (char *) e_tree_memory_node_get_data (E_TREE_MEMORY(etree), tree_path);

	folder = e_storage_set_get_folder (storage_set, path);

	switch (col) {
	case 0: /* Title */
		if (folder == NULL)
			return (void *) "?";
		folder_name = e_folder_get_name (folder);
		unread_count = e_folder_get_unread_count (folder);

		if (unread_count > 0) {
			char *name_with_unread;

			name_with_unread = g_strdup_printf ("%s (%d)", folder_name,
							    unread_count);
			g_object_set_data_full (G_OBJECT (folder), "name_with_unread", name_with_unread, g_free);

			return (void *) name_with_unread;
		} else
			return (void *) folder_name;
	case 1: /* bold */
		if (folder == NULL)
			return GINT_TO_POINTER (FALSE);
		return GINT_TO_POINTER (e_folder_get_highlighted (folder));
	case 2: /* checkbox */
		if (!has_checkbox (storage_set_view, tree_path))
			return GINT_TO_POINTER (2);
		if (priv->checkboxes == NULL)
			return GINT_TO_POINTER (0);
		return GINT_TO_POINTER(g_hash_table_lookup (priv->checkboxes,
							    path) ? 1 : 0);
	default:
		return NULL;
	}

}

static void
etree_fill_in_children (ETreeModel *etree,
			ETreePath tree_path,
			void *model_data)
{
	EStorageSetView *storage_set_view;
	EStorageSet *storage_set;
	ETreePath *parent;
	char *path;

	storage_set_view = E_STORAGE_SET_VIEW (model_data);
	storage_set = storage_set_view->priv->storage_set;

	parent = e_tree_model_node_get_parent (etree, tree_path);
	path = (char *) e_tree_memory_node_get_data (E_TREE_MEMORY(etree), parent);
	if (tree_path == e_tree_model_node_get_first_child (etree, parent)) {
		g_signal_emit (storage_set_view, signals[FOLDER_OPENED], 0, path);
	}
}

static void
etree_set_value_at (ETreeModel *etree,
		    ETreePath tree_path,
		    int col,
		    const void *val,
		    void *model_data)
{
	gboolean value;
	char *path;
	EStorageSetView *storage_set_view;
	EStorageSetViewPrivate *priv;
	char *old_path;

	storage_set_view = E_STORAGE_SET_VIEW (model_data);
	priv = storage_set_view->priv;

	switch (col) {
	case 2: /* checkbox */
		if (!has_checkbox (storage_set_view, tree_path))
			return;

		e_tree_model_pre_change (etree);

		value = GPOINTER_TO_INT (val);
		path = (char *) e_tree_memory_node_get_data (E_TREE_MEMORY(etree), tree_path);
		if (!priv->checkboxes) {
			priv->checkboxes = g_hash_table_new (g_str_hash, g_str_equal);
		}

		old_path = g_hash_table_lookup (priv->checkboxes, path);

		if (old_path) {
			g_hash_table_remove (priv->checkboxes, path);
			g_free (old_path);
		} else {
			path = g_strdup (path);
			g_hash_table_insert (priv->checkboxes, path, path);
		}

		e_tree_model_node_col_changed (etree, tree_path, col);
		g_signal_emit (storage_set_view, signals[CHECKBOXES_CHANGED], 0);
		break;
	}
}

static gboolean
etree_is_editable (ETreeModel *etree,
		   ETreePath path,
		   int col,
		   void *model_data)
{
	if (col == 2)
		return TRUE;
	else
		return FALSE;
}


/* This function duplicates the value passed to it. */
static void *
etree_duplicate_value (ETreeModel *etc,
		       int col,
		       const void *value,
		       void *data)
{
	if (col == 0)
		return (void *)g_strdup (value);
	else
		return (void *)value;
}

/* This function frees the value passed to it. */
static void
etree_free_value (ETreeModel *etc,
		  int col,
		  void *value,
		  void *data)
{
	if (col == 0)
		g_free (value);
}

/* This function creates an empty value. */
static void *
etree_initialize_value (ETreeModel *etc,
			int col,
			void *data)
{
	if (col == 0)
		return g_strdup ("");
	else
		return NULL;
}

/* This function reports if a value is empty. */
static gboolean
etree_value_is_empty (ETreeModel *etc,
		      int col,
		      const void *value,
		      void *data)
{
	if (col == 0)
		return !(value && *(char *)value);
	else
		return !value;
}

/* This function reports if a value is empty. */
static char *
etree_value_to_string (ETreeModel *etc,
		       int col,
		       const void *value,
		       void *data)
{
	if (col == 0)
		return g_strdup(value);
	else
		return g_strdup(value ? "Yes" : "No");
}

static void
etree_node_destroy_func (void *data,
			 void *user_data)
{
	EStorageSetView *storage_set_view;
	char *path;

	path = (char *) data;
	storage_set_view = E_STORAGE_SET_VIEW (user_data);

	if (strcmp (path, ROOT_NODE_NAME))
		remove_node_from_hash (storage_set_view, path);
	g_free (path);
}


/* StorageSet signal handling.  */

static void
new_storage_cb (EStorageSet *storage_set,
		EStorage *storage,
		void *data)
{
	EStorageSetView *storage_set_view;
	EStorageSetViewPrivate *priv;
	ETreePath node;
	char *path;

	storage_set_view = E_STORAGE_SET_VIEW (data);
	priv = storage_set_view->priv;

	path = g_strconcat (E_PATH_SEPARATOR_S, e_storage_get_name (storage), NULL);

	node = e_tree_memory_node_insert (E_TREE_MEMORY(priv->etree_model), priv->root_node, -1, path);
	e_tree_memory_sort_node (E_TREE_MEMORY(priv->etree_model), priv->root_node,
				 storage_sort_callback, storage_set_view);

	if (! add_node_to_hash (storage_set_view, path, node)) {
		e_tree_memory_node_remove (E_TREE_MEMORY(priv->etree_model), node);
		return;
	}
}

static void
removed_storage_cb (EStorageSet *storage_set,
		    EStorage *storage,
		    void *data)
{
	EStorageSetView *storage_set_view;
	EStorageSetViewPrivate *priv;
	ETreeModel *etree;
	ETreePath node;
	char *path;

	storage_set_view = E_STORAGE_SET_VIEW (data);
	priv = storage_set_view->priv;
	etree = priv->etree_model;

	path = g_strconcat (E_PATH_SEPARATOR_S, e_storage_get_name (storage), NULL);
	node = lookup_node_in_hash (storage_set_view, path);
	g_free (path);

	e_tree_memory_node_remove (E_TREE_MEMORY(etree), node);
}

static void
new_folder_cb (EStorageSet *storage_set,
	       const char *path,
	       void *data)
{
	EStorageSetView *storage_set_view;
	EStorageSetViewPrivate *priv;
	ETreeModel *etree;
	ETreePath parent_node;
	ETreePath new_node;
	const char *last_separator;
	char *parent_path;
	char *copy_of_path;

	g_return_if_fail (g_path_is_absolute (path));

	storage_set_view = E_STORAGE_SET_VIEW (data);
	priv = storage_set_view->priv;
	etree = priv->etree_model;

	last_separator = strrchr (path, E_PATH_SEPARATOR);

	parent_path = g_strndup (path, last_separator - path);
	parent_node = g_hash_table_lookup (priv->path_to_etree_node, parent_path);
	if (parent_node == NULL) {
		g_warning ("EStorageSetView: EStorageSet reported new subfolder for non-existing folder -- %s",
			   parent_path);
		g_free (parent_path);
		return;
	}

	g_free (parent_path);

	copy_of_path = g_strdup (path);
	new_node = e_tree_memory_node_insert (E_TREE_MEMORY(etree), parent_node, -1, copy_of_path);
	e_tree_memory_sort_node (E_TREE_MEMORY(etree), parent_node, folder_sort_callback, storage_set_view);

	if (! add_node_to_hash (storage_set_view, path, new_node)) {
		e_tree_memory_node_remove (E_TREE_MEMORY(etree), new_node);
		return;
	}

	setup_folder_changed_callbacks (storage_set_view,
					e_storage_set_get_folder (storage_set, path),
					path);
}

static void
updated_folder_cb (EStorageSet *storage_set,
		   const char *path,
		   void *data)
{
	EStorageSetView *storage_set_view;
	EStorageSetViewPrivate *priv;
	ETreeModel *etree;
	ETreePath node;

	storage_set_view = E_STORAGE_SET_VIEW (data);
	priv = storage_set_view->priv;
	etree = priv->etree_model;

	node = lookup_node_in_hash (storage_set_view, path);
	e_tree_model_pre_change (etree);
	e_tree_model_node_data_changed (etree, node);
}

static void
removed_folder_cb (EStorageSet *storage_set,
		   const char *path,
		   void *data)
{
	EStorageSetView *storage_set_view;
	EStorageSetViewPrivate *priv;
	ETreeModel *etree;
	ETreePath node;

	storage_set_view = E_STORAGE_SET_VIEW (data);
	priv = storage_set_view->priv;
	etree = priv->etree_model;

	node = lookup_node_in_hash (storage_set_view, path);
	e_tree_memory_node_remove (E_TREE_MEMORY(etree), node);
}

static void
close_folder_cb (EStorageSet *storage_set,
		 const char *path,
		 void *data)
{
	EStorageSetView *storage_set_view;
	EStorageSetViewPrivate *priv;
	ETreeModel *etree;
	ETreePath node;

	storage_set_view = E_STORAGE_SET_VIEW (data);
	priv = storage_set_view->priv;
	etree = priv->etree_model;

	node = lookup_node_in_hash (storage_set_view, path);
	e_tree_model_node_request_collapse (priv->etree_model, node);
}


static void
class_init (EStorageSetViewClass *klass)
{
	GObjectClass *object_class;
	ETreeClass *etree_class;

	parent_class = g_type_class_ref(PARENT_TYPE);

	object_class = G_OBJECT_CLASS (klass);
	object_class->dispose  = impl_dispose;
	object_class->finalize = impl_finalize;

	etree_class = E_TREE_CLASS (klass);
	etree_class->right_click             = impl_right_click;
	etree_class->cursor_activated        = impl_cursor_activated;
	etree_class->start_drag              = impl_tree_start_drag;
	etree_class->tree_drag_begin         = impl_tree_drag_begin;
	etree_class->tree_drag_end           = impl_tree_drag_end;
	etree_class->tree_drag_data_get      = impl_tree_drag_data_get;
	etree_class->tree_drag_data_delete   = impl_tree_drag_data_delete;
	etree_class->tree_drag_motion        = impl_tree_drag_motion;
	etree_class->tree_drag_drop          = impl_tree_drag_drop;
	etree_class->tree_drag_leave         = impl_tree_drag_leave;
	etree_class->tree_drag_data_received = impl_tree_drag_data_received;

	signals[FOLDER_SELECTED]
		= g_signal_new ("folder_selected",
				G_OBJECT_CLASS_TYPE (object_class),
				G_SIGNAL_RUN_FIRST,
				G_STRUCT_OFFSET (EStorageSetViewClass, folder_selected),
				NULL, NULL,
				e_shell_marshal_NONE__STRING,
				G_TYPE_NONE, 1,
				G_TYPE_STRING);

	signals[FOLDER_OPENED]
		= g_signal_new ("folder_opened",
				G_OBJECT_CLASS_TYPE (object_class),
				G_SIGNAL_RUN_FIRST,
				G_STRUCT_OFFSET (EStorageSetViewClass, folder_opened),
				NULL, NULL,
				e_shell_marshal_NONE__STRING,
				G_TYPE_NONE, 1,
				G_TYPE_STRING);

	signals[DND_ACTION]
		= g_signal_new ("dnd_action",
				G_OBJECT_CLASS_TYPE (object_class),
				G_SIGNAL_RUN_FIRST,
				G_STRUCT_OFFSET (EStorageSetViewClass, dnd_action),
				NULL, NULL,
				e_shell_marshal_NONE__POINTER_POINTER_POINTER_POINTER,
				G_TYPE_NONE, 4,
				G_TYPE_POINTER,
				G_TYPE_POINTER,
				G_TYPE_POINTER,
				G_TYPE_POINTER);

	signals[FOLDER_CONTEXT_MENU_POPPING_UP]
		= g_signal_new ("folder_context_menu_popping_up",
				G_OBJECT_CLASS_TYPE (object_class),
				G_SIGNAL_RUN_FIRST,
				G_STRUCT_OFFSET (EStorageSetViewClass, folder_context_menu_popping_up),
				NULL, NULL,
				e_shell_marshal_NONE__STRING,
				G_TYPE_NONE, 1,
				G_TYPE_STRING);

	signals[FOLDER_CONTEXT_MENU_POPPED_DOWN]
		= g_signal_new ("folder_context_menu_popped_down",
				G_OBJECT_CLASS_TYPE (object_class),
				G_SIGNAL_RUN_FIRST,
				G_STRUCT_OFFSET (EStorageSetViewClass, folder_context_menu_popped_down),
				NULL, NULL,
				e_shell_marshal_NONE__NONE,
				G_TYPE_NONE, 0);

	signals[CHECKBOXES_CHANGED]
		= g_signal_new ("checkboxes_changed",
				G_OBJECT_CLASS_TYPE (object_class),
				G_SIGNAL_RUN_FIRST,
				G_STRUCT_OFFSET (EStorageSetViewClass, checkboxes_changed),
				NULL, NULL,
				e_shell_marshal_NONE__NONE,
				G_TYPE_NONE, 0);

	checks [0] = gdk_pixbuf_new_from_xpm_data (check_empty_xpm);
	checks [1] = gdk_pixbuf_new_from_xpm_data (check_filled_xpm);
	checks [2] = gdk_pixbuf_new_from_xpm_data (check_missing_xpm);
}

static void
init (EStorageSetView *storage_set_view)
{
	EStorageSetViewPrivate *priv;

	priv = g_new (EStorageSetViewPrivate, 1);

	priv->storage_set                 = NULL;
	priv->path_to_etree_node          = g_hash_table_new (g_str_hash, g_str_equal);
	priv->type_name_to_pixbuf         = g_hash_table_new (g_str_hash, g_str_equal);

	priv->ui_component                = NULL;
	priv->ui_container                = NULL;

	priv->selected_row_path           = NULL;
	priv->right_click_row_path        = NULL;

	priv->show_folders                = TRUE;
	priv->show_checkboxes             = FALSE;
	priv->allow_dnd                   = TRUE;
	priv->search_enabled              = FALSE;

	priv->drag_corba_source_interface = CORBA_OBJECT_NIL;

	priv->drag_corba_source_context   = NULL;
	priv->drag_corba_data             = NULL;

	priv->checkboxes                  = NULL;

	priv->has_checkbox_func           = NULL;
	priv->has_checkbox_func_data      = NULL;

	storage_set_view->priv = priv;
}


/* Handling of the "changed" signal in EFolders displayed in the EStorageSetView.  */

struct _FolderChangedCallbackData {
	EStorageSetView *storage_set_view;
	char *path;
};
typedef struct _FolderChangedCallbackData FolderChangedCallbackData;

static void
folder_changed_callback_data_destroy_notify (void *data)
{
	FolderChangedCallbackData *callback_data;

	callback_data = (FolderChangedCallbackData *) data;

	g_free (callback_data->path);
	g_free (callback_data);
}

static void
folder_changed_cb (EFolder *folder,
		   void *data)
{
	EStorageSetView *storage_set_view;
	EStorageSetViewPrivate *priv;
	FolderChangedCallbackData *callback_data;
	ETreePath node;

	callback_data = (FolderChangedCallbackData *) data;

	storage_set_view = callback_data->storage_set_view;
	priv = callback_data->storage_set_view->priv;

	node = g_hash_table_lookup (priv->path_to_etree_node, callback_data->path);
	if (node == NULL) {
		g_warning ("EStorageSetView -- EFolder::changed emitted for a folder whose path I don't know.");
		return;
	}

	e_tree_model_pre_change (priv->etree_model);
	e_tree_model_node_data_changed (priv->etree_model, node);
}

static void
folder_name_changed_cb (EFolder *folder,
			void *data)
{
	EStorageSetView *storage_set_view;
	EStorageSetViewPrivate *priv;
	FolderChangedCallbackData *callback_data;
	ETreePath parent_node;
	const char *last_separator;
	char *parent_path;

	callback_data = (FolderChangedCallbackData *) data;

	storage_set_view = callback_data->storage_set_view;
	priv = storage_set_view->priv;

	last_separator = strrchr (callback_data->path, E_PATH_SEPARATOR);

	parent_path = g_strndup (callback_data->path, last_separator - callback_data->path);
	parent_node = g_hash_table_lookup (priv->path_to_etree_node, parent_path);
	g_free (parent_path);

	if (parent_node == NULL) {
		g_warning ("EStorageSetView -- EFolder::name_changed emitted for a folder whose path I don't know.");
		return;
	}

	e_tree_memory_sort_node (E_TREE_MEMORY (priv->etree_model), parent_node,
				 folder_sort_callback, storage_set_view);
}

static void
setup_folder_changed_callbacks (EStorageSetView *storage_set_view,
				EFolder *folder,
				const char *path)
{
	FolderChangedCallbackData *folder_changed_callback_data;

	folder_changed_callback_data = g_new (FolderChangedCallbackData, 1);
	folder_changed_callback_data->storage_set_view = storage_set_view;
	folder_changed_callback_data->path = g_strdup (path);

	e_signal_connect_while_alive (folder, "name_changed",
				      G_CALLBACK (folder_name_changed_cb),
				      folder_changed_callback_data,
				      storage_set_view);

	e_signal_connect_full_while_alive (folder, "changed",
					   G_CALLBACK (folder_changed_cb),
					   NULL,
					   folder_changed_callback_data,
					   folder_changed_callback_data_destroy_notify,
					   FALSE, FALSE,
					   storage_set_view);
}


static void
insert_folders (EStorageSetView *storage_set_view,
		ETreePath parent,
		EStorage *storage,
		const char *path)
{
	EStorageSetViewPrivate *priv;
	ETreeModel *etree;
	ETreePath node;
	GList *folder_path_list;
	GList *p;
	const char *storage_name;

	priv = storage_set_view->priv;
	etree = priv->etree_model;

	storage_name = e_storage_get_name (storage);

	folder_path_list = e_storage_get_subfolder_paths (storage, path);
	if (folder_path_list == NULL)
		return;

	for (p = folder_path_list; p != NULL; p = p->next) {
		EFolder *folder;
		const char *folder_name;
		const char *folder_path;
		char *full_path;

		folder_path = (const char *) p->data;
		folder = e_storage_get_folder (storage, folder_path);
		folder_name = e_folder_get_name (folder);

		full_path = g_strconcat ("/", storage_name, folder_path, NULL);

		setup_folder_changed_callbacks (storage_set_view, folder, full_path);

		node = e_tree_memory_node_insert (E_TREE_MEMORY(etree), parent, -1, (void *) full_path);
		e_tree_memory_sort_node(E_TREE_MEMORY(etree), parent, folder_sort_callback, storage_set_view);
		add_node_to_hash (storage_set_view, full_path, node);

		insert_folders (storage_set_view, node, storage, folder_path);
	}

	e_free_string_list (folder_path_list);
}

static void
insert_storages (EStorageSetView *storage_set_view)
{
	EStorageSetViewPrivate *priv;
	EStorageSet *storage_set;
	GList *storage_list;
	GList *p;

	priv = storage_set_view->priv;

	storage_set = priv->storage_set;

	storage_list = e_storage_set_get_storage_list (storage_set);

	for (p = storage_list; p != NULL; p = p->next) {
		EStorage *storage = E_STORAGE (p->data);
		const char *name;
		char *path;
		ETreePath parent;

		name = e_storage_get_name (storage);
		path = g_strconcat ("/", name, NULL);

		parent = e_tree_memory_node_insert (E_TREE_MEMORY(priv->etree_model), priv->root_node, -1, path);
		e_tree_memory_sort_node (E_TREE_MEMORY(priv->etree_model),
					 priv->root_node,
					 storage_sort_callback, storage_set_view);

		g_hash_table_insert (priv->path_to_etree_node, path, parent);

		if (priv->show_folders)
			insert_folders (storage_set_view, parent, storage, "/");
	}

	e_free_object_list (storage_list);
}

void
e_storage_set_view_construct (EStorageSetView   *storage_set_view,
			      EStorageSet       *storage_set,
			      BonoboUIContainer *ui_container)
{
	EStorageSetViewPrivate *priv;
	ETableExtras *extras;
	ECell *cell;

	g_return_if_fail (E_IS_STORAGE_SET_VIEW (storage_set_view));
	g_return_if_fail (E_IS_STORAGE_SET (storage_set));

	priv = storage_set_view->priv;

	priv->ui_container = ui_container;
	if (ui_container != NULL) {
		g_object_weak_ref (G_OBJECT (ui_container), ui_container_destroy_notify, priv);

		priv->ui_component = bonobo_ui_component_new_default ();
		bonobo_ui_component_set_container (priv->ui_component,
						   bonobo_object_corba_objref (BONOBO_OBJECT (ui_container)),
						   NULL);
	}

	priv->etree_model = e_tree_memory_callbacks_new (etree_icon_at,

							 etree_column_count,

							 etree_has_save_id,
							 etree_get_save_id,
							 etree_has_get_node_by_id,
							 etree_get_node_by_id,

							 etree_value_at,
							 etree_set_value_at,
							 etree_is_editable,

							 etree_duplicate_value,
							 etree_free_value,
							 etree_initialize_value,
							 etree_value_is_empty,
							 etree_value_to_string,

							 storage_set_view);

	e_tree_memory_set_node_destroy_func (E_TREE_MEMORY (priv->etree_model),
					     etree_node_destroy_func, storage_set_view);
	e_tree_memory_set_expanded_default (E_TREE_MEMORY (priv->etree_model), FALSE);

	priv->root_node = e_tree_memory_node_insert (E_TREE_MEMORY(priv->etree_model), NULL, -1,
						     g_strdup (ROOT_NODE_NAME));
	add_node_to_hash (storage_set_view, ROOT_NODE_NAME, priv->root_node);

	extras = e_table_extras_new ();
	cell = e_cell_text_new (NULL, GTK_JUSTIFY_LEFT);
	g_object_set((cell), "bold_column", 1, NULL);
	e_table_extras_add_cell (extras, "render_tree",
				 e_cell_tree_new (NULL, NULL, TRUE, cell));

	e_table_extras_add_cell (extras, "optional_checkbox",
				 e_cell_toggle_new (2, 3, checks));

	e_tree_construct_from_spec_file (E_TREE (storage_set_view), priv->etree_model, extras,
					 EVOLUTION_ETSPECDIR "/e-storage-set-view.etspec", NULL);

	e_tree_root_node_set_visible (E_TREE(storage_set_view), FALSE);

	g_object_unref (extras);

	g_object_ref (storage_set);
	priv->storage_set = storage_set;

	e_tree_drag_dest_set (E_TREE (storage_set_view), 0, NULL, 0, GDK_ACTION_MOVE | GDK_ACTION_COPY);

	g_signal_connect_object (storage_set, "new_storage", G_CALLBACK (new_storage_cb), storage_set_view, 0);
	g_signal_connect_object (storage_set, "removed_storage", G_CALLBACK (removed_storage_cb), storage_set_view, 0);
	g_signal_connect_object (storage_set, "new_folder", G_CALLBACK (new_folder_cb), storage_set_view, 0);
	g_signal_connect_object (storage_set, "updated_folder", G_CALLBACK (updated_folder_cb), storage_set_view, 0);
	g_signal_connect_object (storage_set, "removed_folder", G_CALLBACK (removed_folder_cb), storage_set_view, 0);
	g_signal_connect_object (storage_set, "close_folder", G_CALLBACK (close_folder_cb), storage_set_view, 0);

	g_signal_connect_object (priv->etree_model, "fill_in_children", G_CALLBACK (etree_fill_in_children), storage_set_view, 0);

	insert_storages (storage_set_view);
}

/* DON'T USE THIS. Use e_storage_set_new_view() instead. */
GtkWidget *
e_storage_set_view_new (EStorageSet *storage_set,
			BonoboUIContainer *ui_container)
{
	GtkWidget *new;

	g_return_val_if_fail (E_IS_STORAGE_SET (storage_set), NULL);

	new = g_object_new (e_storage_set_view_get_type (), NULL);

	e_storage_set_view_construct (E_STORAGE_SET_VIEW (new), storage_set, ui_container);

	return new;
}


EStorageSet *
e_storage_set_view_get_storage_set (EStorageSetView *storage_set_view)
{
	EStorageSetViewPrivate *priv;

	g_return_val_if_fail (E_IS_STORAGE_SET_VIEW (storage_set_view), NULL);

	priv = storage_set_view->priv;
	return priv->storage_set;
}

void
e_storage_set_view_set_current_folder (EStorageSetView *storage_set_view,
				       const char *path)
{
	EStorageSetViewPrivate *priv;
	ETreePath node;

	g_return_if_fail (storage_set_view != NULL);
	g_return_if_fail (E_IS_STORAGE_SET_VIEW (storage_set_view));
	g_return_if_fail (path != NULL && g_path_is_absolute (path));

	priv = storage_set_view->priv;

	node = g_hash_table_lookup (priv->path_to_etree_node, path);
	if (node == NULL)
		return;

	e_tree_show_node (E_TREE (storage_set_view), node);
	e_tree_set_cursor (E_TREE (storage_set_view), node);

	g_free (priv->selected_row_path);
	priv->selected_row_path = g_strdup (path);

	g_signal_emit (storage_set_view, signals[FOLDER_SELECTED], 0, path);
}

const char *
e_storage_set_view_get_current_folder (EStorageSetView *storage_set_view)
{
	EStorageSetViewPrivate *priv;
	ETreePath etree_node;
	const char *path;

	g_return_val_if_fail (storage_set_view != NULL, NULL);
	g_return_val_if_fail (E_IS_STORAGE_SET_VIEW (storage_set_view), NULL);

	priv = storage_set_view->priv;

	if (!priv->show_folders)
		return NULL; /* Mmh! */

	etree_node = e_tree_get_cursor (E_TREE (storage_set_view));

	if (etree_node == NULL)
		return NULL; /* Mmh? */

	path = (char*)e_tree_memory_node_get_data(E_TREE_MEMORY(priv->etree_model), etree_node);

	return path;
}

void
e_storage_set_view_set_show_folders (EStorageSetView *storage_set_view,
				     gboolean show)
{
	EStorageSetViewPrivate *priv;

	g_return_if_fail (storage_set_view != NULL);
	g_return_if_fail (E_IS_STORAGE_SET_VIEW (storage_set_view));

	priv = storage_set_view->priv;

	if (show == priv->show_folders)
		return;

	/* tear down existing tree and hash table mappings */
	e_tree_memory_node_remove (E_TREE_MEMORY(priv->etree_model), priv->root_node);

	/* now re-add the root node */
	priv->root_node = e_tree_memory_node_insert (E_TREE_MEMORY(priv->etree_model), NULL, -1,
						     g_strdup (ROOT_NODE_NAME));
	add_node_to_hash (storage_set_view, ROOT_NODE_NAME, priv->root_node);

	/* then reinsert the storages after setting the "show_folders"
	   flag.  insert_storages will call insert_folders if
	   show_folders is TRUE */

	priv->show_folders = show;
	insert_storages (storage_set_view);
}

gboolean
e_storage_set_view_get_show_folders (EStorageSetView *storage_set_view)
{
	return storage_set_view->priv->show_folders;
}



void
e_storage_set_view_set_show_checkboxes (EStorageSetView *storage_set_view,
					gboolean show,
					EStorageSetViewHasCheckBoxFunc has_checkbox_func,
					void *func_data)
{
	EStorageSetViewPrivate *priv;
	ETableState *state;

	g_return_if_fail (storage_set_view != NULL);
	g_return_if_fail (E_IS_STORAGE_SET_VIEW (storage_set_view));

	priv = storage_set_view->priv;

	show = !! show;

	if (show == priv->show_checkboxes)
		return;

	priv->show_checkboxes = show;

	state = e_tree_get_state_object (E_TREE (storage_set_view));
	state->col_count = show ? 2 : 1;
	state->columns = g_renew (int, state->columns, state->col_count);
	state->columns [state->col_count - 1] = 0;
	if (show)
		state->columns [0] = 1;

	state->expansions = g_renew (double, state->expansions, state->col_count);
	state->expansions [0] = 1.0;
	if (show)
		state->expansions [1] = 1.0;

	e_tree_set_state_object (E_TREE (storage_set_view), state);

	priv->has_checkbox_func = has_checkbox_func;
	priv->has_checkbox_func_data = func_data;
}

gboolean
e_storage_set_view_get_show_checkboxes (EStorageSetView *storage_set_view)
{
	g_return_val_if_fail (storage_set_view != NULL, FALSE);
	g_return_val_if_fail (E_IS_STORAGE_SET_VIEW (storage_set_view), FALSE);

	return storage_set_view->priv->show_checkboxes;
}

void
e_storage_set_view_enable_search (EStorageSetView *storage_set_view,
				  gboolean enable)
{
	g_return_if_fail (storage_set_view != NULL);
	g_return_if_fail (E_IS_STORAGE_SET_VIEW (storage_set_view));

	enable = !! enable;

	if (enable == storage_set_view->priv->search_enabled)
		return;
	
	storage_set_view->priv->search_enabled = enable;
	e_tree_set_search_column (E_TREE (storage_set_view), enable ? 0 : -1);
}

void
e_storage_set_view_set_checkboxes_list (EStorageSetView *storage_set_view,
					GSList          *checkboxes)
{
	gboolean changed = FALSE;
	EStorageSetViewPrivate *priv = storage_set_view->priv;

	e_tree_model_pre_change (priv->etree_model);
	if (priv->checkboxes) {
		g_hash_table_foreach (priv->checkboxes, (GHFunc) g_free, NULL);
		g_hash_table_destroy (priv->checkboxes);
		changed = TRUE;
	}

	if (checkboxes) {
		priv->checkboxes = g_hash_table_new (g_str_hash, g_str_equal);
		for (; checkboxes; checkboxes = g_slist_next (checkboxes)) {
			char *path = checkboxes->data;

			if (g_hash_table_lookup (priv->checkboxes, path))
				continue;
			path = g_strdup (path);
			g_hash_table_insert (priv->checkboxes, path, path);
		}
		changed = TRUE;
	}

	if (changed)
		e_tree_model_node_changed (priv->etree_model,
					   e_tree_model_get_root (priv->etree_model));
	else
		e_tree_model_no_change (priv->etree_model);
}

static void
essv_add_to_list (gpointer	key,
		  gpointer	value,
		  gpointer	user_data)
{
	GSList **list = user_data;

	*list = g_slist_prepend (*list, g_strdup (key));
}

GSList *
e_storage_set_view_get_checkboxes_list (EStorageSetView *storage_set_view)
{
	GSList *list = NULL;

	if (storage_set_view->priv->checkboxes) {
		g_hash_table_foreach (storage_set_view->priv->checkboxes, essv_add_to_list, &list);

		list = g_slist_reverse (list);
	}

	return list;
}


void
e_storage_set_view_set_allow_dnd (EStorageSetView *storage_set_view,
				  gboolean allow_dnd)
{
	g_return_if_fail (storage_set_view != NULL);
	g_return_if_fail (E_IS_STORAGE_SET_VIEW (storage_set_view));

	storage_set_view->priv->allow_dnd = !! allow_dnd;
}

gboolean
e_storage_set_view_get_allow_dnd (EStorageSetView *storage_set_view)
{
	g_return_val_if_fail (storage_set_view != NULL, FALSE);
	g_return_val_if_fail (E_IS_STORAGE_SET_VIEW (storage_set_view), FALSE);

	return storage_set_view->priv->allow_dnd;
}

const char *
e_storage_set_view_get_right_click_path (EStorageSetView *storage_set_view)
{
	g_return_val_if_fail (storage_set_view != NULL, NULL);
	g_return_val_if_fail (E_IS_STORAGE_SET_VIEW (storage_set_view), NULL);

	return storage_set_view->priv->right_click_row_path;
}


E_MAKE_TYPE (e_storage_set_view, "EStorageSetView", EStorageSetView, class_init, init, PARENT_TYPE)
