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
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <gnome.h>

#include "e-util/e-util.h"
#include "e-shell-constants.h"

#include "e-storage-set-view.h"


#define PARENT_TYPE GTK_TYPE_CTREE
static GtkCTreeClass *parent_class = NULL;

struct _EStorageSetViewPrivate {
	EStorageSet *storage_set;

	/* These tables must always be kept in sync, and one cannot exist
           without the other, as they share the dynamically allocated path.  */
	GHashTable *ctree_node_to_path;
	GHashTable *path_to_ctree_node;

	/* Path of the row selected by the latest "tree_select_row" signal.  */
	const char *selected_row_path;

	/* Path of the row currently being dragged.  */
	const char *dragged_row_path;

	/* Path of the row that was selected before the latest click.  */
	const char *selected_row_path_before_click;

	/* Whether we are currently performing a drag from this view.  */
	int in_drag : 1;

	/* X/Y position for the last button click.  */
	int button_x, button_y;

	/* Button used for the drag.  This is initialized in the `button_press_event'
           handler.  */
	int drag_button;
};

#define DRAG_RESISTANCE 3


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
add_node_to_hashes (EStorageSetView *storage_set_view,
		    const char *path,
		    GtkCTreeNode *node)
{
	EStorageSetViewPrivate *priv;
	char *hash_path;

	g_return_val_if_fail (g_path_is_absolute (path), FALSE);

	priv = storage_set_view->priv;

	if (g_hash_table_lookup (priv->path_to_ctree_node, path) != NULL) {
		g_warning ("EStorageSetView: Node already existing while adding -- %s", path);
		return FALSE;
	}

	g_print ("EStorageSetView: Adding -- %s\n", path);

	hash_path = g_strdup (path);

	g_hash_table_insert (priv->path_to_ctree_node, hash_path, node);
	g_hash_table_insert (priv->ctree_node_to_path, node, hash_path);

	return TRUE;
}

static GtkCTreeNode *
remove_node_from_hashes (EStorageSetView *storage_set_view,
			 const char *path)
{
	EStorageSetViewPrivate *priv;
	GtkCTreeNode *node;
	char *hash_path;

	priv = storage_set_view->priv;

	node = g_hash_table_lookup (priv->path_to_ctree_node, path);
	if (node == NULL) {
		g_warning ("EStorageSetView: Node not found while removing -- %s", path);
		return NULL;
	}

	g_print ("EStorageSetView: Removing -- %s\n", path);

	hash_path = g_hash_table_lookup (priv->ctree_node_to_path, node);
	g_free (hash_path);

	g_hash_table_remove (priv->ctree_node_to_path, node);
	g_hash_table_remove (priv->path_to_ctree_node, path);

	return node;
}

static void
get_pixmap_and_mask_for_folder (EStorageSetView *storage_set_view,
				EFolder *folder,
				GdkPixmap **pixmap_return,
				GdkBitmap **mask_return)
{
	EFolderTypeRegistry *folder_type_registry;
	EStorageSet *storage_set;
	const char *type_name;
	GdkPixbuf *icon_pixbuf;
	GdkPixbuf *scaled_pixbuf;
	GdkVisual *visual;
	GdkGC *gc;

	storage_set = storage_set_view->priv->storage_set;
	folder_type_registry = e_storage_set_get_folder_type_registry (storage_set);

	type_name = e_folder_get_type_string (folder);
	icon_pixbuf = e_folder_type_registry_get_icon_for_type (folder_type_registry,
								type_name, TRUE);

	if (icon_pixbuf == NULL) {
		*pixmap_return = NULL;
		*mask_return = NULL;
		return;
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

	visual = gdk_rgb_get_visual ();
	*pixmap_return = gdk_pixmap_new (NULL,
					 E_SHELL_MINI_ICON_SIZE, E_SHELL_MINI_ICON_SIZE,
					 visual->depth);

	gc = gdk_gc_new (*pixmap_return);
	gdk_pixbuf_render_to_drawable (scaled_pixbuf, *pixmap_return, gc, 0, 0, 0, 0,
				       E_SHELL_MINI_ICON_SIZE, E_SHELL_MINI_ICON_SIZE,
				       GDK_RGB_DITHER_NORMAL, 0, 0);
	gdk_gc_unref (gc);

	*mask_return = gdk_pixmap_new (NULL, E_SHELL_MINI_ICON_SIZE, E_SHELL_MINI_ICON_SIZE, 1);
	gdk_pixbuf_render_threshold_alpha (scaled_pixbuf, *mask_return,
					   0, 0, 0, 0,
					   E_SHELL_MINI_ICON_SIZE, E_SHELL_MINI_ICON_SIZE,
					   0x7f);

	gdk_pixbuf_unref (scaled_pixbuf);
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

	/* Make sure we don't restore the previously selected row after the
           menu is popped down.  */
	priv->selected_row_path_before_click = NULL;
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
hash_foreach_free_path (gpointer key,
			gpointer value,
			gpointer data)
{
	g_free (value);
}

static void
destroy (GtkObject *object)
{
	EStorageSetView *storage_set_view;
	EStorageSetViewPrivate *priv;

	storage_set_view = E_STORAGE_SET_VIEW (object);
	priv = storage_set_view->priv;

	gtk_object_unref (GTK_OBJECT (priv->storage_set));

	g_hash_table_foreach (priv->ctree_node_to_path, hash_foreach_free_path, NULL);
	g_hash_table_destroy (priv->ctree_node_to_path);
	g_hash_table_destroy (priv->path_to_ctree_node);

	g_free (priv);

	(* GTK_OBJECT_CLASS (parent_class)->destroy) (object);
}


/* GtkWidget methods.  */

static int
button_press_event (GtkWidget *widget,
		    GdkEventButton *event)
{
	EStorageSetView *storage_set_view;
	EStorageSetViewPrivate *priv;

	storage_set_view = E_STORAGE_SET_VIEW (widget);
	priv = storage_set_view->priv;

	priv->selected_row_path_before_click = priv->selected_row_path;

	(* GTK_WIDGET_CLASS (parent_class)->button_press_event) (widget, event);

	if (priv->in_drag)
		return FALSE;

	priv->drag_button = event->button;
	priv->button_x = event->x;
	priv->button_y = event->y;

	/* KLUDGE ALERT.  So look at this.  We need to grab the pointer now, to check for
           motion events and maybe start a drag operation.  And GtkCTree seems to do it
           already in the `button_press_event'.  *But* for some reason something is very
           broken somewhere and the grab misbehaves when done by GtkCTree's
           `button_press_event'.  So we have to ungrab the pointer and re-grab it our way.
           Weee!  */

	gdk_pointer_ungrab (GDK_CURRENT_TIME);
	gdk_flush ();
	gtk_grab_remove (widget);

	gdk_pointer_grab (GTK_CLIST (widget)->clist_window, FALSE,
			  GDK_POINTER_MOTION_MASK | GDK_BUTTON_RELEASE_MASK,
			  NULL, NULL, event->time);
	gtk_grab_add (widget);

	return TRUE;
}

static int
motion_notify_event (GtkWidget *widget,
		     GdkEventMotion *event)
{
	EStorageSetView *storage_set_view;
	EStorageSetViewPrivate *priv;

	if (event->window != GTK_CLIST (widget)->clist_window)
		return (* GTK_WIDGET_CLASS (parent_class)->motion_notify_event) (widget, event);

	storage_set_view = E_STORAGE_SET_VIEW (widget);
	priv = storage_set_view->priv;

	if (priv->in_drag || priv->drag_button == 0)
		return FALSE;

	if (ABS (event->x - priv->button_x) < DRAG_RESISTANCE
	    && ABS (event->y - priv->button_y) < DRAG_RESISTANCE)
		return FALSE;

	priv->in_drag = TRUE;
	priv->dragged_row_path = priv->selected_row_path;

	gtk_drag_begin (widget, target_list, GDK_ACTION_MOVE,
			priv->drag_button, (GdkEvent *) event);

	return TRUE;
}

static void
handle_left_button_selection (EStorageSetView *storage_set_view,
			      GtkWidget *widget,
			      GdkEventButton *event)
{
	EStorageSetViewPrivate *priv;

	priv = storage_set_view->priv;

	gtk_signal_emit (GTK_OBJECT (widget), signals[FOLDER_SELECTED],
			 priv->selected_row_path);
	priv->selected_row_path = NULL;
}

static void
handle_right_button_selection (EStorageSetView *storage_set_view,
			       GtkWidget *widget,
			       GdkEventButton *event)
{
	EStorageSetViewPrivate *priv;

	priv = storage_set_view->priv;

	popup_folder_menu (storage_set_view, event);

	if (priv->selected_row_path_before_click != NULL)
		e_storage_set_view_set_current_folder (storage_set_view,
						       priv->selected_row_path_before_click);
}

static int
button_release_event (GtkWidget *widget,
		      GdkEventButton *event)
{
	EStorageSetView *storage_set_view;
	EStorageSetViewPrivate *priv;

	if (event->window != GTK_CLIST (widget)->clist_window)
		return (* GTK_WIDGET_CLASS (parent_class)->button_release_event) (widget, event);

	storage_set_view = E_STORAGE_SET_VIEW (widget);
	priv = storage_set_view->priv;

	if (! priv->in_drag) {
		gdk_pointer_ungrab (GDK_CURRENT_TIME);
		gtk_grab_remove (widget);
		gdk_flush ();

		if (priv->selected_row_path != NULL) {
			if (priv->drag_button == 1)
				handle_left_button_selection (storage_set_view, widget, event);
			else
				handle_right_button_selection (storage_set_view, widget, event);
		}
	}

	priv->selected_row_path_before_click = NULL;

	return TRUE;
}

static void
drag_end (GtkWidget *widget,
	  GdkDragContext *context)
{
	EStorageSetView *storage_set_view;
	EStorageSetViewPrivate *priv;

	storage_set_view = E_STORAGE_SET_VIEW (widget);
	priv = storage_set_view->priv;

	if (priv->dragged_row_path != NULL)
		e_storage_set_view_set_current_folder (storage_set_view,
						       priv->selected_row_path_before_click);

	priv->in_drag = FALSE;
	priv->drag_button = 0;
	priv->dragged_row_path = NULL;
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
drag_data_get (GtkWidget *widget,
	       GdkDragContext *context,
	       GtkSelectionData *selection_data,
	       guint info,
	       guint32 time)
{
	EStorageSetView *storage_set_view;

	storage_set_view = E_STORAGE_SET_VIEW (widget);

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


/* StorageSet signal handling.  */

static void
new_storage_cb (EStorageSet *storage_set,
		EStorage *storage,
		void *data)
{
	EStorageSetView *storage_set_view;
	EStorageSetViewPrivate *priv;
	GtkCTreeNode *node;
	char *text[2];
	char *path;

	storage_set_view = E_STORAGE_SET_VIEW (data);
	priv = storage_set_view->priv;

	path = g_strconcat (G_DIR_SEPARATOR_S, e_storage_get_name (storage), NULL);

	text[0] = (char *) e_storage_get_name (storage); /* Yuck.  */
	text[1] = NULL;

	node = gtk_ctree_insert_node (GTK_CTREE (storage_set_view), NULL, NULL,
				      text, 3, NULL, NULL, NULL, NULL, FALSE, TRUE);

	if (! add_node_to_hashes (storage_set_view, path, node)) {
		g_free (path);
		gtk_ctree_remove_node (GTK_CTREE (storage_set_view), node);
		return;
	}

	g_free (path);

	/* FIXME: We want a more specialized sort, e.g. the local folders should always be
           on top.  */
	gtk_ctree_sort_node (GTK_CTREE (storage_set_view), NULL);
}

static void
removed_storage_cb (EStorageSet *storage_set,
		    EStorage *storage,
		    void *data)
{
	EStorageSetView *storage_set_view;
	EStorageSetViewPrivate *priv;
	GtkCTreeNode *node;
	char *path;

	storage_set_view = E_STORAGE_SET_VIEW (data);
	priv = storage_set_view->priv;

	path = g_strconcat (G_DIR_SEPARATOR_S, e_storage_get_name (storage), NULL);
	node = remove_node_from_hashes (storage_set_view, path);
	g_free (path);

	gtk_ctree_remove_node (GTK_CTREE (storage_set_view), node);
}

static void
new_folder_cb (EStorageSet *storage_set,
	       const char *path,
	       void *data)
{
	EStorageSetView *storage_set_view;
	EStorageSetViewPrivate *priv;
	GtkCTreeNode *parent_node;
	GtkCTreeNode *node;
	GdkPixmap *pixmap;
	GdkBitmap *mask;
	char *text[2];
	const char *last_separator;
	char *parent_path;

	g_return_if_fail (g_path_is_absolute (path));

	storage_set_view = E_STORAGE_SET_VIEW (data);
	priv = storage_set_view->priv;

	last_separator = strrchr (path, G_DIR_SEPARATOR);

	parent_path = g_strndup (path, last_separator - path);
	parent_node = g_hash_table_lookup (priv->path_to_ctree_node, parent_path);
	if (parent_node == NULL) {
		g_print ("EStorageSetView: EStorageSet reported new subfolder for non-existing folder -- %s\n",
			 parent_path);
		g_free (parent_path);
		return;
	}

	g_free (parent_path);

	if (parent_node == NULL)
		return;

	text[0] = (char *) last_separator + 1; /* Yuck.  */
	text[1] = NULL;

	get_pixmap_and_mask_for_folder (storage_set_view,
					e_storage_set_get_folder (storage_set, path),
					&pixmap, &mask);
	node = gtk_ctree_insert_node (GTK_CTREE (storage_set_view),
				      parent_node, NULL,
				      text, 3,
				      pixmap, mask, pixmap, mask,
				      FALSE, TRUE);

	if (! add_node_to_hashes (storage_set_view, path, node)) {
		gtk_ctree_remove_node (GTK_CTREE (storage_set_view), node);
		return;
	}

	gtk_ctree_sort_node (GTK_CTREE (storage_set_view), parent_node);
}

static void
removed_folder_cb (EStorageSet *storage_set,
		   const char *path,
		   void *data)
{
	EStorageSetView *storage_set_view;
	GtkCTreeNode *node;

	storage_set_view = E_STORAGE_SET_VIEW (data);

	node = remove_node_from_hashes (storage_set_view, path);
	gtk_ctree_remove_node (GTK_CTREE (storage_set_view), node);
}


/* GtkCTree methods.  */

static void
tree_select_row (GtkCTree *ctree,
		 GtkCTreeNode *row,
		 gint column)
{
	EStorageSetView *storage_set_view;
	EStorageSetViewPrivate *priv;
	const char *path;

	(* GTK_CTREE_CLASS (parent_class)->tree_select_row) (ctree, row, column);

	storage_set_view = E_STORAGE_SET_VIEW (ctree);
	priv = storage_set_view->priv;

	path = g_hash_table_lookup (storage_set_view->priv->ctree_node_to_path, row);
	if (path == NULL)
		return;

	priv->selected_row_path = path;
}


static void
class_init (EStorageSetViewClass *klass)
{
	GtkObjectClass *object_class;
	GtkWidgetClass *widget_class;
	GtkCTreeClass *ctree_class;

	parent_class = gtk_type_class (gtk_ctree_get_type ());

	object_class = GTK_OBJECT_CLASS (klass);
	object_class->destroy = destroy;

	widget_class = GTK_WIDGET_CLASS (klass);
	widget_class->button_press_event   = button_press_event;
	widget_class->motion_notify_event  = motion_notify_event;
	widget_class->button_release_event = button_release_event;
	widget_class->drag_end             = drag_end;
	widget_class->drag_data_get        = drag_data_get;

	ctree_class = GTK_CTREE_CLASS (klass);
	ctree_class->tree_select_row = tree_select_row;

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
	GtkCList *clist;

	/* Avoid GtkCTree's broken focusing behavior.  FIXME: Other ways?  */
	GTK_WIDGET_UNSET_FLAGS (storage_set_view, GTK_CAN_FOCUS);

	priv = g_new (EStorageSetViewPrivate, 1);

	priv->storage_set                    = NULL;
	priv->ctree_node_to_path             = g_hash_table_new (g_direct_hash, g_direct_equal);
	priv->path_to_ctree_node             = g_hash_table_new (g_str_hash, g_str_equal);
	priv->selected_row_path              = NULL;
	priv->dragged_row_path               = NULL;
	priv->selected_row_path_before_click = NULL;
	priv->in_drag                        = FALSE;
	priv->button_x                       = 0;
	priv->button_y                       = 0;

	storage_set_view->priv = priv;

	/* Set up the right mouse button so that it also selects.  */
	clist = GTK_CLIST (storage_set_view);
	clist->button_actions[2] |= GTK_BUTTON_SELECTS;
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
		GtkCTreeNode *parent,
		EStorage *storage,
		const char *path,
		int level)
{
	EStorageSetViewPrivate *priv;
	GtkCTree *ctree;
	GtkCTreeNode *node;
	GList *folder_list;
	GList *p;
	const char *storage_name;

	ctree = GTK_CTREE (storage_set_view);
	priv = storage_set_view->priv;

	storage_name = e_storage_get_name (storage);

	folder_list = e_storage_list_folders (storage, path);
	if (folder_list == NULL)
		return;

	folder_list = g_list_sort (folder_list, folder_compare_cb);

	for (p = folder_list; p != NULL; p = p->next) {
		EFolder *folder;
		const char *folder_name;
		char *text[2];
		char *subpath;
		char *full_path;
		GdkPixmap *pixmap;
		GdkBitmap *mask;

		folder = E_FOLDER (p->data);
		folder_name = e_folder_get_name (folder);

		text[0] = (char *) folder_name;	/* Yuck.  */
		text[1] = NULL;

		get_pixmap_and_mask_for_folder (storage_set_view, folder, &pixmap, &mask);
		node = gtk_ctree_insert_node (ctree, parent, NULL,
					      text, 3,
					      pixmap, mask, pixmap, mask,
					      FALSE, TRUE);

		subpath = g_concat_dir_and_file (path, folder_name);
		insert_folders (storage_set_view, node, storage, subpath, level + 1);

		full_path = g_strconcat("/", storage_name, subpath, NULL);
		g_hash_table_insert (priv->ctree_node_to_path, node, full_path);
		g_hash_table_insert (priv->path_to_ctree_node, full_path, node);

		g_free (subpath);
	}

	e_free_object_list (folder_list);
}

void
e_storage_set_view_construct (EStorageSetView *storage_set_view,
			      EStorageSet *storage_set)
{
	EStorageSetViewPrivate *priv;
	GtkCTreeNode *parent;
	GtkCTree *ctree;
	EStorage *storage;
	GList *storage_list;
	GList *p;
	const char *name;
	char *text[2];
	char *path;

	g_return_if_fail (storage_set_view != NULL);
	g_return_if_fail (E_IS_STORAGE_SET_VIEW (storage_set_view));
	g_return_if_fail (storage_set != NULL);
	g_return_if_fail (E_IS_STORAGE_SET (storage_set));

	ctree = GTK_CTREE (storage_set_view);
	priv = storage_set_view->priv;

	/* Set up GtkCTree/GtkCList parameters.  */

	gtk_ctree_construct          (ctree, 1, 0, NULL);
	gtk_ctree_set_line_style     (ctree, GTK_CTREE_LINES_DOTTED);
	gtk_ctree_set_expander_style (ctree, GTK_CTREE_EXPANDER_SQUARE);

	gtk_clist_set_selection_mode     (GTK_CLIST (ctree), GTK_SELECTION_BROWSE);
	gtk_clist_set_row_height         (GTK_CLIST (ctree), E_SHELL_MINI_ICON_SIZE);
	gtk_clist_set_column_auto_resize (GTK_CLIST (ctree), 0, TRUE);
	
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
	
	storage_list = e_storage_set_get_storage_list (storage_set);

	text[1] = NULL;

	for (p = storage_list; p != NULL; p = p->next) {
		storage = E_STORAGE (p->data);

		name = e_storage_get_name (storage);
		text[0] = (char *) name; /* Yuck.  */

		parent = gtk_ctree_insert_node (ctree, NULL, NULL,
						text, 3,
						NULL, NULL, NULL, NULL,
						FALSE, TRUE);

		path = g_strconcat ("/", name, NULL);
		g_hash_table_insert (priv->ctree_node_to_path, parent, path);
		g_hash_table_insert (priv->path_to_ctree_node, path, parent);

		insert_folders (storage_set_view, parent, storage, "/", 1);
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
	GtkCTreeNode *node;

	g_return_if_fail (storage_set_view != NULL);
	g_return_if_fail (E_IS_STORAGE_SET_VIEW (storage_set_view));
	g_return_if_fail (path == NULL || g_path_is_absolute (path));

	priv = storage_set_view->priv;

	if (path == NULL) {
		gtk_clist_unselect_all (GTK_CLIST (storage_set_view));
		return;
	}

	node = g_hash_table_lookup (priv->path_to_ctree_node, path);
	if (node == NULL) {
		gtk_clist_unselect_all (GTK_CLIST (storage_set_view));
		return;
	}

	gtk_ctree_select (GTK_CTREE (storage_set_view), node);

	gtk_signal_emit (GTK_OBJECT (storage_set_view), signals[FOLDER_SELECTED], path);
}

const char *
e_storage_set_view_get_current_folder (EStorageSetView *storage_set_view)
{
	EStorageSetViewPrivate *priv;
	GtkCList *clist;
	GtkCTree *ctree;
	GtkCTreeRow *ctree_row;
	GtkCTreeNode *ctree_node;
	const char *path;

	g_return_val_if_fail (storage_set_view != NULL, NULL);
	g_return_val_if_fail (E_IS_STORAGE_SET_VIEW (storage_set_view), NULL);

	priv = storage_set_view->priv;

	clist = GTK_CLIST (storage_set_view);
	ctree = GTK_CTREE (storage_set_view);

	if (clist->selection == NULL)
		return NULL;

	ctree_row = GTK_CTREE_ROW (clist->selection->data);
	ctree_node = gtk_ctree_find_node_ptr (ctree, ctree_row);
	if (ctree_node == NULL)
		return NULL;	/* Mmh? */

	path = g_hash_table_lookup (priv->ctree_node_to_path, ctree_node);

	return path;
}


E_MAKE_TYPE (e_storage_set_view, "EStorageSetView", EStorageSetView, class_init, init, PARENT_TYPE)
