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

	/* Whether we are currently performing a drag from this view.  */
	int in_drag : 1;

	/* Button used for the drag.  This is initialized in the `button_press_event'
           handler.  */
	int drag_button;
};


enum {
	FOLDER_SELECTED,
	LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = { 0 };


#define ICON_WIDTH  24
#define ICON_HEIGHT 24


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

	(* GTK_WIDGET_CLASS (parent_class)->button_press_event) (widget, event);

	storage_set_view = E_STORAGE_SET_VIEW (widget);
	priv = storage_set_view->priv;

	if (priv->in_drag)
		return FALSE;

	priv->drag_button = event->button;

	/* KLUDGE ALERT.  So look at this.  We need to grab the pointer now, to check for
           motion events and maybe start a drag operation.  And GtkCTree seems to do it
           already in the `button_press_event'.  *But* for some reason something is very
           broken somewhere and the grab misbehaves when done by GtkCTree's
           `button_press_event'.  So we have to ungrab the pointer and re-grab it our way.
           Weee!  */

	gdk_pointer_ungrab (GDK_CURRENT_TIME);
	gdk_flush ();
	gdk_pointer_grab (GTK_CLIST (widget)->clist_window, FALSE,
			  GDK_POINTER_MOTION_MASK | GDK_BUTTON_RELEASE_MASK,
			  NULL, NULL, event->time);

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

	priv->in_drag = TRUE;

	gtk_drag_begin (widget, target_list, GDK_ACTION_MOVE,
			priv->drag_button, (GdkEvent *) event);

	return TRUE;
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

	if (! priv->in_drag && priv->selected_row_path != NULL) {
		gdk_pointer_ungrab (GDK_CURRENT_TIME);
		gdk_flush ();

		gtk_signal_emit (GTK_OBJECT (widget), signals[FOLDER_SELECTED],
				 priv->selected_row_path);
		priv->selected_row_path = NULL;
	}

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

	priv->in_drag = FALSE;
	priv->drag_button = 0;
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

	/* Avoid GtkCTree's broken focusing behavior.  FIXME: Other ways?  */
	GTK_WIDGET_UNSET_FLAGS (storage_set_view, GTK_CAN_FOCUS);

	priv = g_new (EStorageSetViewPrivate, 1);

	priv->storage_set        = NULL;
	priv->ctree_node_to_path = g_hash_table_new (g_direct_hash, g_direct_equal);
	priv->path_to_ctree_node = g_hash_table_new (g_str_hash, g_str_equal);
	priv->selected_row_path  = NULL;
	priv->in_drag            = FALSE;

	storage_set_view->priv = priv;
}


static void
get_pixmap_and_mask_for_folder (EStorageSetView *storage_set_view,
				EFolder *folder,
				GdkPixmap **pixmap_return,
				GdkBitmap **mask_return)
{
	EFolderTypeRepository *folder_type_repository;
	EStorageSet *storage_set;
	const char *type_name;
	GdkPixbuf *icon_pixbuf;
	GdkPixbuf *scaled_pixbuf;
	GdkVisual *visual;
	GdkGC *gc;

	storage_set = storage_set_view->priv->storage_set;
	folder_type_repository = e_storage_set_get_folder_type_repository (storage_set);

	type_name = e_folder_get_type_string (folder);
	icon_pixbuf = e_folder_type_repository_get_icon_for_type (folder_type_repository,
								  type_name);

	scaled_pixbuf = gdk_pixbuf_new (gdk_pixbuf_get_colorspace (icon_pixbuf),
					gdk_pixbuf_get_has_alpha (icon_pixbuf),
					gdk_pixbuf_get_bits_per_sample (icon_pixbuf),
					ICON_WIDTH, ICON_HEIGHT);

	gdk_pixbuf_scale (icon_pixbuf, scaled_pixbuf,
			  0, 0, ICON_WIDTH, ICON_HEIGHT,
			  0.0, 0.0,
			  (double) ICON_WIDTH / gdk_pixbuf_get_width (icon_pixbuf),
			  (double) ICON_HEIGHT / gdk_pixbuf_get_height (icon_pixbuf),
			  GDK_INTERP_HYPER);

	visual = gdk_rgb_get_visual ();
	*pixmap_return = gdk_pixmap_new (NULL, ICON_WIDTH, ICON_HEIGHT, visual->depth);

	gc = gdk_gc_new (*pixmap_return);
	gdk_pixbuf_render_to_drawable (scaled_pixbuf, *pixmap_return, gc, 0, 0, 0, 0,
				       ICON_WIDTH, ICON_HEIGHT,
				       GDK_RGB_DITHER_NORMAL, 0, 0);
	gdk_gc_unref (gc);

	*mask_return = gdk_pixmap_new (NULL, ICON_WIDTH, ICON_HEIGHT, 1);
	gdk_pixbuf_render_threshold_alpha (scaled_pixbuf, *mask_return, 0, 0, 0, 0,
					   ICON_WIDTH, ICON_HEIGHT, 0x7f);

	gdk_pixbuf_unref (scaled_pixbuf);
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

	/* Set up GtkCTree/GtkCList parameters.  */
	gtk_ctree_construct (ctree, 1, 0, NULL);
	gtk_ctree_set_line_style (ctree, GTK_CTREE_LINES_DOTTED);
	gtk_clist_set_selection_mode (GTK_CLIST (ctree), GTK_SELECTION_BROWSE);
	gtk_clist_set_row_height (GTK_CLIST (ctree), ICON_HEIGHT);
	
	priv = storage_set_view->priv;

	gtk_object_ref (GTK_OBJECT (storage_set));
	priv->storage_set = storage_set;
	
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
		g_hash_table_insert (priv->path_to_ctree_node, parent, path);

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
}


E_MAKE_TYPE (e_storage_set_view, "EStorageSetView", EStorageSetView, class_init, init, PARENT_TYPE)
