/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 *  Authors: Jeffrey Stedfast <fejj@ximian.com>
 *
 *  Copyright 2003 Ximian, Inc. (www.ximian.com)
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Street #330, Boston, MA 02111-1307, USA.
 *
 */


#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>

#include <gtk/gtk.h>
#include <gdk-pixbuf/gdk-pixbuf.h>

#include <camel/camel-session.h>
#include <camel/camel-store.h>
#include <camel/camel-folder.h>
#include <camel/camel-vtrash-folder.h>
#include <camel/camel-stream-mem.h>
#include <camel/camel-file-utils.h>

#include "e-util/e-mktemp.h"
#include "e-util/e-request.h"
#include "e-util/e-dialog-utils.h"

#include "mail-mt.h"
#include "mail-ops.h"
#include "mail-tools.h"
#include "mail-config.h"
#include "mail-component.h"

#include "em-utils.h"
#include "em-popup.h"
#include "em-marshal.h"
#include "em-folder-tree.h"
#include "em-folder-selector.h"
#include "em-folder-selection.h"
#include "em-folder-properties.h"

#define d(x) x


struct _EMFolderTreePrivate {
	GtkTreeView *treeview;
	EMFolderTreeModel *model;
	
	char *selected_uri;
	char *selected_path;

	gboolean do_multiselect;
	/* when doing a multiselect, folders that we didn't find */
	GList *lost_folders;
	
	guint save_state_id;
	
	guint loading_row_id;
	
	GtkTreeRowReference *drag_row;
};

enum {
	FOLDER_SELECTED,
	LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = { 0 };

extern CamelSession *session;


static void em_folder_tree_class_init (EMFolderTreeClass *klass);
static void em_folder_tree_init (EMFolderTree *emft);
static void em_folder_tree_destroy (GtkObject *obj);
static void em_folder_tree_finalize (GObject *obj);

static gboolean emft_save_state (EMFolderTree *emft);
static void emft_queue_save_state (EMFolderTree *emft);

static void emft_tree_row_collapsed (GtkTreeView *treeview, GtkTreeIter *root, GtkTreePath *path, EMFolderTree *emft);
static void emft_tree_row_expanded (GtkTreeView *treeview, GtkTreeIter *root, GtkTreePath *path, EMFolderTree *emft);
static gboolean emft_tree_button_press (GtkWidget *treeview, GdkEventButton *event, EMFolderTree *emft);
static void emft_tree_selection_changed (GtkTreeSelection *selection, EMFolderTree *emft);

struct _emft_selection_data {
	GtkTreeModel *model;
	GtkTreeIter *iter;
	gboolean set;
};


static GtkVBoxClass *parent_class = NULL;

GType
em_folder_tree_get_type (void)
{
	static GType type = 0;
	
	if (!type) {
		static const GTypeInfo info = {
			sizeof (EMFolderTreeClass),
			NULL, /* base_class_init */
			NULL, /* base_class_finalize */
			(GClassInitFunc) em_folder_tree_class_init,
			NULL, /* class_finalize */
			NULL, /* class_data */
			sizeof (EMFolderTree),
			0,    /* n_preallocs */
			(GInstanceInitFunc) em_folder_tree_init,
		};
		
		type = g_type_register_static (GTK_TYPE_VBOX, "EMFolderTree", &info, 0);
	}
	
	return type;
}

static void
em_folder_tree_class_init (EMFolderTreeClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	GtkObjectClass *gtk_object_class = GTK_OBJECT_CLASS (klass);
	
	parent_class = g_type_class_ref (GTK_TYPE_VBOX);
	
	object_class->finalize = em_folder_tree_finalize;
	gtk_object_class->destroy = em_folder_tree_destroy;
	
	signals[FOLDER_SELECTED] =
		g_signal_new ("folder-selected",
			      G_OBJECT_CLASS_TYPE (object_class),
			      G_SIGNAL_RUN_FIRST,
			      G_STRUCT_OFFSET (EMFolderTreeClass, folder_selected),
			      NULL, NULL,
			      em_marshal_VOID__STRING_STRING,
			      G_TYPE_NONE, 2,
			      G_TYPE_STRING,
			      G_TYPE_STRING);
}


static gboolean
subdirs_contain_unread (GtkTreeModel *model, GtkTreeIter *root)
{
	unsigned int unread;
	GtkTreeIter iter;
	
	if (!gtk_tree_model_iter_children (model, &iter, root))
		return FALSE;
	
	do {
		gtk_tree_model_get (model, &iter, COL_UINT_UNREAD, &unread, -1);
		if (unread)
			return TRUE;
		
		if (gtk_tree_model_iter_has_child (model, &iter))
			if (subdirs_contain_unread (model, &iter))
				return TRUE;
	} while (gtk_tree_model_iter_next (model, &iter));
	
	return FALSE;
}


enum {
	FOLDER_ICON_NORMAL,
	FOLDER_ICON_INBOX,
	FOLDER_ICON_OUTBOX,
	FOLDER_ICON_TRASH,
	FOLDER_ICON_JUNK,
	FOLDER_ICON_LAST
};

static GdkPixbuf *folder_icons[FOLDER_ICON_LAST];

static void
render_pixbuf (GtkTreeViewColumn *column, GtkCellRenderer *renderer,
	       GtkTreeModel *model, GtkTreeIter *iter, gpointer user_data)
{
	static gboolean initialised = FALSE;
	GdkPixbuf *pixbuf = NULL;
	gboolean is_store;
	char *path;
	
	if (!initialised) {
		folder_icons[FOLDER_ICON_NORMAL] = gdk_pixbuf_new_from_file (EVOLUTION_ICONSDIR "/folder-mini.png", NULL);
		folder_icons[FOLDER_ICON_INBOX] = gdk_pixbuf_new_from_file (EVOLUTION_ICONSDIR "/inbox-mini.png", NULL);
		folder_icons[FOLDER_ICON_OUTBOX] = gdk_pixbuf_new_from_file (EVOLUTION_ICONSDIR "/outbox-mini.png", NULL);
		folder_icons[FOLDER_ICON_TRASH] = gdk_pixbuf_new_from_file (EVOLUTION_ICONSDIR "/evolution-trash-mini.png", NULL);
		folder_icons[FOLDER_ICON_JUNK] = gdk_pixbuf_new_from_file (EVOLUTION_ICONSDIR "/evolution-junk-mini.png", NULL);
		initialised = TRUE;
	}
	
	gtk_tree_model_get (model, iter, COL_STRING_FOLDER_PATH, &path,
			    COL_BOOL_IS_STORE, &is_store, -1);
	
	if (!is_store && path != NULL) {
		if (!strcasecmp (path, "/Inbox"))
			pixbuf = folder_icons[FOLDER_ICON_INBOX];
		else if (!strcasecmp (path, "/Outbox"))
			pixbuf = folder_icons[FOLDER_ICON_OUTBOX];
		else if (*path == '/' && !strcasecmp (path + 1, CAMEL_VTRASH_NAME))
			pixbuf = folder_icons[FOLDER_ICON_TRASH];
		else if (*path == '/' && !strcasecmp (path + 1, CAMEL_VJUNK_NAME))
			pixbuf = folder_icons[FOLDER_ICON_JUNK];
		else
			pixbuf = folder_icons[FOLDER_ICON_NORMAL];
	}
	
	g_object_set (renderer, "pixbuf", pixbuf, "visible", !is_store, NULL);
}

static void
render_display_name (GtkTreeViewColumn *column, GtkCellRenderer *renderer,
		     GtkTreeModel *model, GtkTreeIter *iter, gpointer user_data)
{
	gboolean is_store, bold;
	unsigned int unread;
	char *display;
	char *name;
	
	gtk_tree_model_get (model, iter, COL_STRING_DISPLAY_NAME, &name,
			    COL_BOOL_IS_STORE, &is_store,
			    COL_UINT_UNREAD, &unread, -1);
	
	if (!(bold = is_store || unread)) {
		if (gtk_tree_model_iter_has_child (model, iter))
			bold = subdirs_contain_unread (model, iter);
	}
	
	if (!is_store && unread)
		display = g_strdup_printf ("%s (%u)", name, unread);
	else
		display = g_strdup (name);
	
	g_object_set (renderer, "text", display,
		      "weight", bold ? PANGO_WEIGHT_BOLD : PANGO_WEIGHT_NORMAL,
		      NULL);
	
	g_free (display);
}

static void
em_folder_tree_init (EMFolderTree *emft)
{
	struct _EMFolderTreePrivate *priv;
	
	priv = g_new0 (struct _EMFolderTreePrivate, 1);
	priv->lost_folders = NULL;
	priv->selected_uri = NULL;
	priv->selected_path = NULL;
	priv->treeview = NULL;
	priv->model = NULL;
	priv->drag_row = NULL;
	
	emft->priv = priv;
}

static void
em_folder_tree_finalize (GObject *obj)
{
	EMFolderTree *emft = (EMFolderTree *) obj;

	/* clear list of lost uris */
	if (emft->priv->lost_folders) {
		g_list_foreach (emft->priv->lost_folders, (GFunc) g_free, NULL);
		g_list_free (emft->priv->lost_folders);
		emft->priv->lost_folders = NULL;
	}
	
	g_free (emft->priv->selected_uri);
	g_free (emft->priv->selected_path);
	g_free (emft->priv);
	
	G_OBJECT_CLASS (parent_class)->finalize (obj);
}

static void
em_folder_tree_destroy (GtkObject *obj)
{
	EMFolderTree *emft = (EMFolderTree *) obj;
	struct _EMFolderTreePrivate *priv = emft->priv;
	
	if (priv->loading_row_id != 0) {
		g_signal_handler_disconnect (priv->model, priv->loading_row_id);
		priv->loading_row_id = 0;
	}
	
	if (priv->save_state_id != 0) {
		g_source_remove (priv->save_state_id);
		emft_save_state (emft);
	}

	priv->treeview = NULL;
	priv->model = NULL;
	
	GTK_OBJECT_CLASS (parent_class)->destroy (obj);
}


static GtkTreeView *
folder_tree_new (EMFolderTreeModel *model)
{
	GtkTreeSelection *selection;
	GtkTreeViewColumn *column;
	GtkCellRenderer *renderer;
	GtkWidget *tree;
	
	tree = gtk_tree_view_new_with_model ((GtkTreeModel *) model);
	GTK_WIDGET_SET_FLAGS(tree, GTK_CAN_FOCUS);
	
	column = gtk_tree_view_column_new ();
	gtk_tree_view_append_column ((GtkTreeView *) tree, column);
	
	renderer = gtk_cell_renderer_pixbuf_new ();
	gtk_tree_view_column_pack_start (column, renderer, FALSE);
	gtk_tree_view_column_set_cell_data_func (column, renderer, render_pixbuf, NULL, NULL);
	
	renderer = gtk_cell_renderer_text_new ();
	gtk_tree_view_column_pack_start (column, renderer, TRUE);
	gtk_tree_view_column_set_cell_data_func (column, renderer, render_display_name, NULL, NULL);
	
	selection = gtk_tree_view_get_selection ((GtkTreeView *) tree);
	gtk_tree_selection_set_mode (selection, GTK_SELECTION_SINGLE);
	
	gtk_tree_view_set_headers_visible ((GtkTreeView *) tree, FALSE);
	
	return (GtkTreeView *) tree;
}

static void
em_folder_tree_construct (EMFolderTree *emft, EMFolderTreeModel *model)
{
	struct _EMFolderTreePrivate *priv = emft->priv;
	GtkTreeSelection *selection;
	GtkWidget *scrolled;
	
	scrolled = gtk_scrolled_window_new (NULL, NULL);
	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scrolled),
					GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
	gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (scrolled), GTK_SHADOW_IN);
	
	priv->model = model;
	priv->treeview = folder_tree_new (model);
	gtk_widget_show ((GtkWidget *) priv->treeview);
	
	g_signal_connect (priv->treeview, "row-expanded", G_CALLBACK (emft_tree_row_expanded), emft);
	g_signal_connect (priv->treeview, "row-collapsed", G_CALLBACK (emft_tree_row_collapsed), emft);
	g_signal_connect (priv->treeview, "button-press-event", G_CALLBACK (emft_tree_button_press), emft);
	
	selection = gtk_tree_view_get_selection ((GtkTreeView *) priv->treeview);
	g_signal_connect (selection, "changed", G_CALLBACK (emft_tree_selection_changed), emft);
	
	gtk_container_add ((GtkContainer *) scrolled, (GtkWidget *) priv->treeview);
	gtk_widget_show (scrolled);
	
	gtk_box_pack_start ((GtkBox *) emft, scrolled, TRUE, TRUE, 0);
}


GtkWidget *
em_folder_tree_new (void)
{
	EMFolderTreeModel *model;
	EMFolderTree *emft;
	
	model = em_folder_tree_model_new (mail_component_peek_base_directory (mail_component_peek ()));
	emft = (EMFolderTree *) em_folder_tree_new_with_model (model);
	g_object_unref (model);
	
	return (GtkWidget *) emft;
}


struct _gsbn {
	struct _EMFolderTreeModelStoreInfo *si;
	const char *name;
};

static void
emft_get_store_by_name (CamelStore *store, struct _EMFolderTreeModelStoreInfo *si, struct _gsbn *gsbn)
{
	if (!strcmp (si->display_name, gsbn->name))
		gsbn->si = si;
}

static void
emft_expand_node (const char *key, gpointer value, EMFolderTree *emft)
{
	struct _EMFolderTreePrivate *priv = emft->priv;
	struct _EMFolderTreeModelStoreInfo *si;
	GtkTreeRowReference *row;
	GtkTreePath *path;
	EAccount *account;
	const char *p;
	char *id;
	
	if (!(p = strchr (key, ':')))
		return;
	
	id = g_strndup (key, p - key);
	if ((account = mail_config_get_account_by_uid (id)) && account->enabled) {
		CamelException ex;
		CamelStore *store;
		
		camel_exception_init (&ex);
		store = (CamelStore *) camel_session_get_service (session, account->source->url, CAMEL_PROVIDER_STORE, &ex);
		camel_exception_clear (&ex);
		
		if (store == NULL || !(si = g_hash_table_lookup (priv->model->store_hash, store))) {
			if (store)
				camel_object_unref (store);
			g_free (id);
			return;
		}
	} else {
		struct _gsbn gsbn;
		
		gsbn.si = NULL;
		gsbn.name = id;
		
		g_hash_table_foreach (priv->model->store_hash, (GHFunc) emft_get_store_by_name, &gsbn);
		if (!(si = gsbn.si)) {
			g_free (id);
			return;
		}
	}
	
	g_free (id);
	
	p++;
	if (!strcmp (p, "/"))
		row = si->row;
	else if (!(row = g_hash_table_lookup (si->path_hash, p)))
		return;
	
	path = gtk_tree_row_reference_get_path (row);
	gtk_tree_view_expand_to_path (priv->treeview, path);
	gtk_tree_path_free (path);
}


static void
emft_loading_row_cb (EMFolderTreeModel *model, GtkTreePath *tree_path, GtkTreeIter *iter, EMFolderTree *emft)
{
	struct _EMFolderTreeModelStoreInfo *si;
	CamelStore *store;
	EAccount *account;
	char *path, *key;
	
	gtk_tree_model_get ((GtkTreeModel *) model, iter,
			    COL_STRING_FOLDER_PATH, &path,
			    COL_POINTER_CAMEL_STORE, &store,
			    -1);
	
	si = g_hash_table_lookup (model->store_hash, store);
	if ((account = mail_config_get_account_by_name (si->display_name))) {
	        key = g_strdup_printf ("%s:%s", account->uid, path);
	} else {
		key = g_strdup_printf ("%s:%s", si->display_name, path);
	}
	
	if (em_folder_tree_model_get_expanded (model, key))
		gtk_tree_view_expand_to_path (emft->priv->treeview, tree_path);
	
	g_free (key);
}


GtkWidget *
em_folder_tree_new_with_model (EMFolderTreeModel *model)
{
	EMFolderTree *emft;
	
	emft = g_object_new (EM_TYPE_FOLDER_TREE, NULL);
	em_folder_tree_construct (emft, model);
	g_object_ref (model);
	
	/* FIXME: this sucks... */
	g_hash_table_foreach (model->expanded, (GHFunc) emft_expand_node, emft);
	
	emft->priv->loading_row_id = g_signal_connect (model, "loading-row", G_CALLBACK (emft_loading_row_cb), emft);
	
	return (GtkWidget *) emft;
}


static void
tree_drag_begin (GtkWidget *widget, GdkDragContext *context, EMFolderTree *emft)
{
	struct _EMFolderTreePrivate *priv = emft->priv;
	GtkTreeSelection *selection;
	GtkTreeModel *model;
	GtkTreePath *path;
	GtkTreeIter iter;
	
	selection = gtk_tree_view_get_selection ((GtkTreeView *) widget);
	if (!gtk_tree_selection_get_selected (selection, &model, &iter))
		return;
	
	path = gtk_tree_model_get_path (model, &iter);
	priv->drag_row = gtk_tree_row_reference_new (model, path);
	gtk_tree_path_free (path);
	
	/* FIXME: set a drag icon? */
}

static void
tree_drag_data_delete (GtkWidget *widget, GdkDragContext *context, EMFolderTree *emft)
{
	struct _EMFolderTreePrivate *priv = emft->priv;
	GtkTreePath *path;
	
	if (!priv->drag_row || (path = gtk_tree_row_reference_get_path (priv->drag_row)))
		return;
	
	em_folder_tree_model_drag_data_delete (priv->model, path);
	gtk_tree_path_free (path);
}

static void
tree_drag_data_get (GtkWidget *widget, GdkDragContext *context, GtkSelectionData *selection, guint info, guint time, EMFolderTree *emft)
{
	struct _EMFolderTreePrivate *priv = emft->priv;
	GtkTreePath *path;
	
	if (!priv->drag_row || !(path = gtk_tree_row_reference_get_path (priv->drag_row)))
		return;
	
	em_folder_tree_model_drag_data_get (priv->model, path, selection, info);
	gtk_tree_path_free (path);
}

static void
tree_drag_data_received (GtkWidget *widget, GdkDragContext *context, int x, int y, GtkSelectionData *selection,
			 guint info, guint time, EMFolderTree *emft)
{
	struct _EMFolderTreePrivate *priv = emft->priv;
	GtkTreeViewDropPosition pos;
	GtkTreePath *path;
	gboolean success;
	gboolean moved;
	gboolean move;
	
	if (!gtk_tree_view_get_dest_row_at_pos (priv->treeview, x, y, &path, &pos))
		return;
	
	move = context->action == GDK_ACTION_MOVE;
	success = em_folder_tree_model_drag_data_received (priv->model, path, selection, info, move, &moved);
	
	gtk_drag_finish (context, success, success && move && !moved, time);
}

static gboolean
tree_drag_drop (GtkWidget *widget, GdkDragContext *context, int x, int y, guint time, EMFolderTree *emft)
{
	struct _EMFolderTreePrivate *priv = emft->priv;
	GtkTreeViewColumn *column;
	int cell_x, cell_y;
	GtkTreePath *path;
	GdkAtom target;
	
	if (!gtk_tree_view_get_path_at_pos (priv->treeview, x, y, &path, &column, &cell_x, &cell_y))
		return FALSE;
	
	target = em_folder_tree_model_row_drop_target (priv->model, path, context->targets);
	gtk_tree_path_free (path);
	if (target == GDK_NONE)
		return FALSE;
	
	return TRUE;
}

static void
tree_drag_end (GtkWidget *widget, GdkDragContext *context, EMFolderTree *emft)
{
	struct _EMFolderTreePrivate *priv = emft->priv;
	
	if (priv->drag_row) {
		gtk_tree_row_reference_free (priv->drag_row);
		priv->drag_row = NULL;
	}
	
	/* FIXME: undo anything done in drag-begin */
}

static void
tree_drag_leave (GtkWidget *widget, GdkDragContext *context, guint time, EMFolderTree *emft)
{
	/* FIXME: unhighlight target row? */
}

static gboolean
tree_drag_motion (GtkWidget *widget, GdkDragContext *context, int x, int y, guint time, EMFolderTree *emft)
{
	struct _EMFolderTreePrivate *priv = emft->priv;
	GtkTreeViewDropPosition pos;
	GtkTreePath *path;
	GdkDragAction action;
	
	if (!gtk_tree_view_get_dest_row_at_pos (priv->treeview, x, y, &path, &pos))
		return FALSE;
	
	/* FIXME: highlight target row? */
	
	action = em_folder_tree_model_row_drop_possible (priv->model, path, context->targets);
	gtk_tree_path_free (path);
	
	gdk_drag_status (context, action, time);
	
	return action;
}


void
em_folder_tree_enable_drag_and_drop (EMFolderTree *emft)
{
	struct _EMFolderTreePrivate *priv;
	
	g_return_if_fail (EM_IS_FOLDER_TREE (emft));
	
	priv = emft->priv;
	
	em_folder_tree_model_set_drag_drop_types (priv->model, (GtkWidget *) priv->treeview);
	
	g_signal_connect (priv->treeview, "drag-begin", G_CALLBACK (tree_drag_begin), emft);
	g_signal_connect (priv->treeview, "drag-data-delete", G_CALLBACK (tree_drag_data_delete), emft);
	g_signal_connect (priv->treeview, "drag-data-get", G_CALLBACK (tree_drag_data_get), emft);
	g_signal_connect (priv->treeview, "drag-data-received", G_CALLBACK (tree_drag_data_received), emft);
	g_signal_connect (priv->treeview, "drag-drop", G_CALLBACK (tree_drag_drop), emft);
	g_signal_connect (priv->treeview, "drag-end", G_CALLBACK (tree_drag_end), emft);
	g_signal_connect (priv->treeview, "drag-leave", G_CALLBACK (tree_drag_leave), emft);
	g_signal_connect (priv->treeview, "drag-motion", G_CALLBACK (tree_drag_motion), emft);
}


void
em_folder_tree_set_multiselect (EMFolderTree *tree, gboolean mode)
{
	GtkTreeSelection *sel = gtk_tree_view_get_selection ((GtkTreeView *) tree->priv->treeview);
	
	tree->priv->do_multiselect = mode;
	gtk_tree_selection_set_mode (sel, mode ? GTK_SELECTION_MULTIPLE : GTK_SELECTION_SINGLE);
}

static void
get_selected_uris_iterate (GtkTreeModel *model, GtkTreePath *path, GtkTreeIter *iter, gpointer data)
{
	GList **list = (GList **) data;
	char *uri;
	
	gtk_tree_model_get (model, iter, /*COL_STRING_FOLDER_PATH, &path,*/
			    COL_STRING_URI, &uri, -1);
	*list = g_list_append (*list, g_strdup (uri));
}

GList *
em_folder_tree_get_selected_uris (EMFolderTree *emft)
{
	GtkTreeSelection *selection = gtk_tree_view_get_selection (emft->priv->treeview);
	GList *lost = emft->priv->lost_folders;
	GList *list = NULL;
	
	/* at first, add lost uris */
	while (lost) {
		list = g_list_append (list, g_strdup (lost->data));
		lost = g_list_next (lost);
	}
	
	gtk_tree_selection_selected_foreach (selection, get_selected_uris_iterate, &list);
	
	return list;
}

static void
get_selected_uris_path_iterate (GtkTreeModel *model, GtkTreePath *treepath, GtkTreeIter *iter, gpointer data)
{
	GList **list = (GList **) data;
	char *path;
	
	gtk_tree_model_get (model, iter, COL_STRING_FOLDER_PATH, &path, -1);
	*list = g_list_append (*list, g_strdup (path));
}

GList *
em_folder_tree_get_selected_paths (EMFolderTree *emft)
{
	GtkTreeSelection *selection = gtk_tree_view_get_selection (emft->priv->treeview);
	GList *list = NULL;
	
	gtk_tree_selection_selected_foreach (selection, get_selected_uris_path_iterate, &list);
	
	return list;
}

void
em_folder_tree_set_selected_list (EMFolderTree *emft, GList *list)
{
	struct _EMFolderTreePrivate *priv = emft->priv;
	
	/* clear list of lost uris */
	if (priv->lost_folders) {
		g_list_foreach (priv->lost_folders, (GFunc)g_free, NULL);
		g_list_free (priv->lost_folders);
		priv->lost_folders = NULL;
	}
	
	while (list) {
		em_folder_tree_set_selected (emft, list->data);
		list = g_list_next (list);
	}
}


#if 0
static void
dump_fi (CamelFolderInfo *fi, int depth)
{
	int i;
	
	while (fi != NULL) {
		for (i = 0; i < depth; i++)
			fputs ("  ", stdout);
		
		printf ("path='%s'; full_name='%s'\n", fi->path, fi->full_name);
		
		if (fi->child)
			dump_fi (fi->child, depth + 1);
		
		fi = fi->sibling;
	}
}
#endif

struct _EMFolderTreeGetFolderInfo {
	struct _mail_msg msg;
	
	/* input data */
	GtkTreeRowReference *root;
	EMFolderTree *emft;
	CamelStore *store;
	guint32 flags;
	char *top;
	
	/* output data */
	CamelFolderInfo *fi;
	
	/* uri to select if any after the op is done */
	char *select_uri;
};

static void
emft_get_folder_info__get (struct _mail_msg *mm)
{
	struct _EMFolderTreeGetFolderInfo *m = (struct _EMFolderTreeGetFolderInfo *) mm;
	guint32 flags = m->flags;
	
	if (camel_store_supports_subscriptions (m->store))
		flags |= CAMEL_STORE_FOLDER_INFO_SUBSCRIBED;
	
	m->fi = camel_store_get_folder_info (m->store, m->top, flags, &mm->ex);
}

static void
emft_get_folder_info__got (struct _mail_msg *mm)
{
	struct _EMFolderTreeGetFolderInfo *m = (struct _EMFolderTreeGetFolderInfo *) mm;
	struct _EMFolderTreePrivate *priv = m->emft->priv;
	struct _EMFolderTreeModelStoreInfo *si;
	GtkTreeIter root, iter;
	CamelFolderInfo *fi;
	GtkTreeStore *model;
	GtkTreePath *path;
	gboolean load;
	
	/* check that we haven't been destroyed */
	if (priv->treeview == NULL)
		return;
	
	/* check that our parent folder hasn't been deleted/unsubscribed */
	if (!gtk_tree_row_reference_valid (m->root))
		return;
	
	if (!(si = g_hash_table_lookup (priv->model->store_hash, m->store))) {
		/* store has been removed in the interim - do nothing */
		return;
	}
	
	model = (GtkTreeStore *) gtk_tree_view_get_model (priv->treeview);
	
	path = gtk_tree_row_reference_get_path (m->root);
	gtk_tree_model_get_iter ((GtkTreeModel *) model, &root, path);
	gtk_tree_path_free (path);
	
	/* make sure we still need to load the tree subfolders... */
	gtk_tree_model_get ((GtkTreeModel *) model, &root,
			    COL_BOOL_LOAD_SUBDIRS, &load,
			    -1);
	if (!load) {
		if (priv->do_multiselect && m->select_uri)
			priv->lost_folders = g_list_append (priv->lost_folders, g_strdup (m->select_uri));
		return;
	}
	
	/* get the first child (which will be a dummy node) */
	gtk_tree_model_iter_children ((GtkTreeModel *) model, &iter, &root);
	
	/* FIXME: camel's IMAP code is totally on crack here, @top's
	 * folder info should be @fi and fi->child should be what we
	 * want to fill our tree with... *sigh* */
	if (m->top && m->fi && !strcmp (m->fi->full_name, m->top)) {
		if (!(fi = m->fi->child))
			fi = m->fi->sibling;
	} else
		fi = m->fi;
	
	if (fi == NULL) {
		/* no children afterall... remove the "Loading..." placeholder node */
		gtk_tree_store_remove (model, &iter);
	} else {
		do {
			em_folder_tree_model_set_folder_info (priv->model, &iter, si, fi);
			
			if ((fi = fi->sibling) != NULL)
				gtk_tree_store_append (model, &iter, &root);
		} while (fi != NULL);
	}
	
	gtk_tree_store_set (model, &root, COL_BOOL_LOAD_SUBDIRS, FALSE, -1);
	
	if (m->select_uri)
		em_folder_tree_set_selected (m->emft, m->select_uri);
	
	emft_queue_save_state (m->emft);
}

static void
emft_get_folder_info__free (struct _mail_msg *mm)
{
	struct _EMFolderTreeGetFolderInfo *m = (struct _EMFolderTreeGetFolderInfo *) mm;
	
	camel_store_free_folder_info (m->store, m->fi);
	
	gtk_tree_row_reference_free (m->root);
	camel_object_unref (m->store);
	g_free (m->select_uri);
	g_free (m->top);
}

static struct _mail_msg_op get_folder_info_op = {
	NULL,
	emft_get_folder_info__get,
	emft_get_folder_info__got,
	emft_get_folder_info__free,
};

static void
emft_update_model_expanded_state (struct _EMFolderTreePrivate *priv, GtkTreeIter *iter, gboolean expanded)
{
	struct _EMFolderTreeModelStoreInfo *si;
	CamelStore *store;
	EAccount *account;
	char *path, *key;
	
	gtk_tree_model_get ((GtkTreeModel *) priv->model, iter,
			    COL_STRING_FOLDER_PATH, &path,
			    COL_POINTER_CAMEL_STORE, &store,
			    -1);
	
	si = g_hash_table_lookup (priv->model->store_hash, store);
	if ((account = mail_config_get_account_by_name (si->display_name))) {
	        key = g_strdup_printf ("%s:%s", account->uid, path);
	} else {
		key = g_strdup_printf ("%s:%s", si->display_name, path);
	}
	
	em_folder_tree_model_set_expanded (priv->model, key, expanded);
	g_free (key);
}

static void
emft_tree_row_expanded (GtkTreeView *treeview, GtkTreeIter *root, GtkTreePath *tree_path, EMFolderTree *emft)
{
	struct _EMFolderTreePrivate *priv = emft->priv;
	struct _EMFolderTreeGetFolderInfo *m;
	GtkTreeModel *model;
	CamelStore *store;
	const char *top;
	gboolean load;
	char *path;
	
	model = gtk_tree_view_get_model (treeview);
	
	gtk_tree_model_get (model, root,
			    COL_STRING_FOLDER_PATH, &path,
			    COL_POINTER_CAMEL_STORE, &store,
			    COL_BOOL_LOAD_SUBDIRS, &load,
			    -1);
	
	emft_update_model_expanded_state (priv, root, TRUE);
	
	if (!load) {
		emft_queue_save_state (emft);
		return;
	}
	
	if (!path || !strcmp (path, "/"))
		top = NULL;
	else
		top = path + 1;
	
	m = mail_msg_new (&get_folder_info_op, NULL, sizeof (struct _EMFolderTreeGetFolderInfo));
	m->root = gtk_tree_row_reference_new (model, tree_path);
	camel_object_ref (store);
	m->store = store;
	m->emft = emft;
	m->top = g_strdup (top);
	m->flags = CAMEL_STORE_FOLDER_INFO_RECURSIVE;
	m->select_uri = NULL;
	
	e_thread_put (mail_thread_new, (EMsg *) m);
}

static void
emft_tree_row_collapsed (GtkTreeView *treeview, GtkTreeIter *root, GtkTreePath *tree_path, EMFolderTree *emft)
{
	emft_update_model_expanded_state (emft->priv, root, FALSE);
	emft_queue_save_state (emft);
}

#if 0
static void
emft_popup_view (GtkWidget *item, EMFolderTree *emft)
{

}

static void
emft_popup_open_new (GtkWidget *item, EMFolderTree *emft)
{
}
#endif


struct _EMCopyFolders {
	struct _mail_msg msg;
	
	/* input data */
	CamelStore *fromstore;
	CamelStore *tostore;
	
	char *frombase;
	char *tobase;
	
	int delete;
};

static void
emft_copy_folders__copy (struct _mail_msg *mm)
{
	struct _EMCopyFolders *m = (struct _EMCopyFolders *) mm;
	guint32 flags = CAMEL_STORE_FOLDER_INFO_FAST | CAMEL_STORE_FOLDER_INFO_RECURSIVE;
	GList *pending = NULL, *deleting = NULL, *l;
	GString *fromname, *toname;
	CamelFolderInfo *fi;
	const char *tmp;
	int fromlen;
	
	if (camel_store_supports_subscriptions (m->fromstore))
		flags |= CAMEL_STORE_FOLDER_INFO_SUBSCRIBED;
	
	if (!(fi = camel_store_get_folder_info (m->fromstore, m->frombase, flags, &mm->ex)))
		return;
	
	pending = g_list_append (pending, fi);
	
	toname = g_string_new ("");
	fromname = g_string_new ("");
	
	tmp = strrchr (m->frombase, '/');
	if (tmp == NULL)
		fromlen = 0;
	else
		fromlen = tmp - m->frombase + 1;
	
	d(printf ("top name is '%s'\n", fi->full_name));
	
	while (pending) {
		CamelFolderInfo *info = pending->data;
		
		pending = g_list_remove_link (pending, pending);
		while (info) {
			CamelFolder *fromfolder, *tofolder;
			GPtrArray *uids;
			
			if (info->child)
				pending = g_list_append (pending, info->child);
			
			if (m->tobase[0])
				g_string_printf (toname, "%s/%s", m->tobase, info->full_name + fromlen);
			else
				g_string_printf (toname, "%s", info->full_name + fromlen);
			
			d(printf ("Copying from '%s' to '%s'\n", info->full_name, toname->str));
			
			/* This makes sure we create the same tree, e.g. from a nonselectable source */
			/* Not sure if this is really the 'right thing', e.g. for spool stores, but it makes the ui work */
			if ((info->flags & CAMEL_FOLDER_NOSELECT) == 0) {
				d(printf ("this folder is selectable\n"));
				if (!(fromfolder = camel_store_get_folder (m->fromstore, info->full_name, 0, &mm->ex)))
					goto exception;
				
				if (!(tofolder = camel_store_get_folder (m->tostore, toname->str, CAMEL_STORE_FOLDER_CREATE, &mm->ex))) {
					camel_object_unref (fromfolder);
					goto exception;
				}
				
				if (camel_store_supports_subscriptions (m->tostore)
				    && !camel_store_folder_subscribed (m->tostore, toname->str))
					camel_store_subscribe_folder (m->tostore, toname->str, NULL);
				
				uids = camel_folder_get_uids (fromfolder);
				camel_folder_transfer_messages_to (fromfolder, uids, tofolder, NULL, m->delete, &mm->ex);
				camel_folder_free_uids (fromfolder, uids);
				
				camel_object_unref (fromfolder);
				camel_object_unref (tofolder);
			}
			
			if (camel_exception_is_set (&mm->ex))
				goto exception;
			else if (m->delete)
				deleting = g_list_prepend (deleting, info);
			
			info = info->sibling;
		}
	}
	
	/* delete the folders in reverse order from how we copyied them, if we are deleting any */
	l = deleting;
	while (l) {
		CamelFolderInfo *info = l->data;
		
		d(printf ("deleting folder '%s'\n", info->full_name));
		
		if (camel_store_supports_subscriptions (m->fromstore))
			camel_store_unsubscribe_folder (m->fromstore, info->full_name, NULL);
		
		camel_store_delete_folder (m->fromstore, info->full_name, NULL);
		l = l->next;
	}
	
 exception:
	
	camel_store_free_folder_info (m->fromstore, fi);
	g_list_free (deleting);
	
	g_string_free (toname, TRUE);
	g_string_free (fromname, TRUE);
}

static void
emft_copy_folders__free (struct _mail_msg *mm)
{
	struct _EMCopyFolders *m = (struct _EMCopyFolders *) mm;
	
	camel_object_unref (m->fromstore);
	camel_object_unref (m->tostore);
	
	g_free (m->frombase);
	g_free (m->tobase);
}

static struct _mail_msg_op copy_folders_op = {
	NULL,
	emft_copy_folders__copy,
	NULL,
	emft_copy_folders__free,
};

static void
emft_copy_folders (CamelStore *tostore, const char *tobase, CamelStore *fromstore, const char *frombase, int delete)
{
	struct _EMCopyFolders *m;
	
	m = mail_msg_new (&copy_folders_op, NULL, sizeof (struct _EMCopyFolders));
	camel_object_ref (fromstore);
	m->fromstore = fromstore;
	camel_object_ref (tostore);
	m->tostore = tostore;
	m->frombase = g_strdup (frombase);
	m->tobase = g_strdup (tobase);
	m->delete = delete;
	
	e_thread_put (mail_thread_new, (EMsg *) m);
}

struct _copy_folder_data {
	EMFolderTree *emft;
	gboolean delete;
};

static void
emft_popup_copy_folder_selected (const char *uri, void *data)
{
	struct _copy_folder_data *cfd = data;
	struct _EMFolderTreePrivate *priv;
	CamelStore *fromstore, *tostore;
	char *tobase, *frombase;
	CamelException ex;
	GtkWidget *dialog;
	CamelURL *url;
	
	if (uri == NULL) {
		g_free (cfd);
		return;
	}
	
	priv = cfd->emft->priv;
	
	d(printf ("copying folder '%s' to '%s'\n", priv->selected_path, uri));
	
	camel_exception_init (&ex);
	if (!(fromstore = camel_session_get_store (session, priv->selected_uri, &ex)))
		goto exception;
	
	frombase = priv->selected_path + 1;
	
	if (!(tostore = camel_session_get_store (session, uri, &ex))) {
		camel_object_unref (fromstore);
		goto exception;
	}
	
	url = camel_url_new (uri, NULL);
	if (url->fragment)
		tobase = url->fragment;
	else if (url->path && url->path[0])
		tobase = url->path + 1;
	else
		tobase = "";
	
	emft_copy_folders (tostore, tobase, fromstore, frombase, cfd->delete);
	
	camel_url_free (url);
	g_free (cfd);
	
	return;
	
 exception:
	
	dialog = gtk_message_dialog_new ((GtkWindow *) cfd->emft, GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
					 GTK_MESSAGE_ERROR, GTK_BUTTONS_CLOSE, _("%s"), ex.desc);
	g_signal_connect (dialog, "response", G_CALLBACK (gtk_widget_destroy), dialog);
	camel_exception_clear (&ex);
	gtk_widget_show (dialog);
	g_free (cfd);
}

static void
emft_popup_copy (GtkWidget *item, EMFolderTree *emft)
{
	struct _copy_folder_data *cfd;
	
	cfd = g_malloc (sizeof (*cfd));
	cfd->emft = emft;
	cfd->delete = FALSE;
	
	em_select_folder (NULL, _("Select folder"),
			  NULL, emft_popup_copy_folder_selected, cfd);
}

static void
emft_popup_move (GtkWidget *item, EMFolderTree *emft)
{
	struct _copy_folder_data *cfd;
	
	cfd = g_malloc (sizeof (*cfd));
	cfd->emft = emft;
	cfd->delete = TRUE;
	
	em_select_folder (NULL, _("Select folder"),
			  NULL, emft_popup_copy_folder_selected, cfd);
}


gboolean
em_folder_tree_create_folder (EMFolderTree *emft, const char *path, const char *uri)
{
	struct _EMFolderTreePrivate *priv = emft->priv;
	struct _EMFolderTreeModelStoreInfo *si;
	const char *parent, *full_name;
	char *name, *namebuf = NULL;
	GtkWidget *dialog;
	CamelStore *store;
	CamelException ex;
	
	d(printf ("Creating folder: %s (%s)\n", path, uri));
	
	camel_exception_init (&ex);
	if (!(store = (CamelStore *) camel_session_get_service (session, uri, CAMEL_PROVIDER_STORE, &ex)))
		goto exception;
	
	if (!(si = g_hash_table_lookup (priv->model->store_hash, store))) {
		camel_object_unref (store);
		g_assert_not_reached ();
		goto exception;
	}
	
	camel_object_unref (store);
	
	full_name = path[0] == '/' ? path + 1 : path;
	namebuf = g_strdup (full_name);
	if (!(name = strrchr (namebuf, '/'))) {
		name = namebuf;
		parent = "";
	} else {
		*name++ = '\0';
		parent = namebuf;
	}
	
	d(printf ("creating folder parent='%s' name='%s' path='%s'\n", parent, name, path));
	
	camel_store_create_folder (si->store, parent, name, &ex);
	if (camel_exception_is_set (&ex)) {
		goto exception;
	} else if (camel_store_supports_subscriptions (si->store)) {
		camel_store_subscribe_folder (si->store, full_name, &ex);
		if (camel_exception_is_set (&ex))
			goto exception;
	}
	
	g_free (namebuf);
	
	return TRUE;
	
 exception:
	
	dialog = gtk_message_dialog_new ((GtkWindow *) emft, GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
					 GTK_MESSAGE_ERROR, GTK_BUTTONS_CLOSE, _("%s"), ex.desc);
	g_signal_connect (dialog, "response", G_CALLBACK (gtk_widget_destroy), dialog);
	camel_exception_clear (&ex);
	g_free (namebuf);
	
	gtk_widget_show (dialog);
	
	return FALSE;
}

static void
emft_popup_new_folder_response (EMFolderSelector *emfs, int response, EMFolderTree *emft)
{
	const char *uri, *path;
	
	if (response != GTK_RESPONSE_OK) {
		gtk_widget_destroy ((GtkWidget *) emfs);
		return;
	}
	
	uri = em_folder_selector_get_selected_uri (emfs);
	path = em_folder_selector_get_selected_path (emfs);
	
	if (em_folder_tree_create_folder (emfs->emft, path, uri))
		gtk_widget_destroy ((GtkWidget *) emfs);
}

static void
emft_popup_new_folder (GtkWidget *item, EMFolderTree *emft)
{
	EMFolderTree *folder_tree;
	GtkWidget *dialog;
	
	folder_tree = (EMFolderTree *) em_folder_tree_new_with_model (emft->priv->model);
	
	dialog = em_folder_selector_create_new (folder_tree, 0, _("Create folder"), _("Specify where to create the folder:"));
	em_folder_selector_set_selected ((EMFolderSelector *) dialog, emft->priv->selected_uri);
	g_signal_connect (dialog, "response", G_CALLBACK (emft_popup_new_folder_response), emft);
	gtk_widget_show (dialog);
	/* FIXME: what is this crap, this should go in the folder selector */
	gtk_widget_grab_focus ((GtkWidget *) ((EMFolderSelector *)dialog)->name_entry);
}

static void
emft_popup_delete_rec (CamelStore *store, CamelFolderInfo *fi, CamelException *ex)
{
	while (fi) {
		CamelFolder *folder;
		
		if (fi->child) {
			emft_popup_delete_rec (store, fi->child, ex);
			if (camel_exception_is_set (ex))
				return;
		}
		
		d(printf ("deleting folder '%s'\n", fi->full_name));
		
		/* shouldn't camel do this itself? */
		if (camel_store_supports_subscriptions (store))
			camel_store_unsubscribe_folder (store, fi->full_name, NULL);
		
		if (!(folder = camel_store_get_folder (store, fi->full_name, 0, ex)))
			return;
		
		if (!CAMEL_IS_VEE_FOLDER (folder)) {
			GPtrArray *uids = camel_folder_get_uids (folder);
			int i;
			
			camel_folder_freeze (folder);
			for (i = 0; i < uids->len; i++)
				camel_folder_delete_message (folder, uids->pdata[i]);
			
			camel_folder_free_uids (folder, uids);
			
			camel_folder_sync (folder, TRUE, NULL);
			camel_folder_thaw (folder);
		}
		
		camel_store_delete_folder (store, fi->full_name, ex);
		if (camel_exception_is_set (ex))
			return;
		
		fi = fi->sibling;
	}
}

static void
emft_popup_delete_folders (CamelStore *store, const char *path, CamelException *ex)
{
	guint32 flags = CAMEL_STORE_FOLDER_INFO_RECURSIVE | CAMEL_STORE_FOLDER_INFO_FAST;
	const char *full_name;
	CamelFolderInfo *fi;
	
	if (camel_store_supports_subscriptions (store))
		flags |= CAMEL_STORE_FOLDER_INFO_SUBSCRIBED;
	
	full_name = path[0] == '/' ? path + 1 : path;
	fi = camel_store_get_folder_info (store, full_name, flags, ex);
	if (camel_exception_is_set (ex))
		return;
	
	emft_popup_delete_rec (store, fi, ex);
	camel_store_free_folder_info (store, fi);
}

static void
selfunc (GtkTreeModel *model, GtkTreePath *path, GtkTreeIter *iter, gpointer data)
{
	struct _emft_selection_data *dat = (struct _emft_selection_data *) data;
	
	dat->model = model;
	if (!dat->set)
		*(dat->iter) = *iter;
	dat->set = TRUE;
}

static gboolean
emft_selection_get_selected (GtkTreeSelection *selection, GtkTreeModel **model, GtkTreeIter *iter)
{
	struct _emft_selection_data dat = { NULL, iter, FALSE };
	
	if (gtk_tree_selection_get_mode (selection) == GTK_SELECTION_MULTIPLE) {
		gtk_tree_selection_selected_foreach (selection, selfunc, &dat);
		if (model)
			*model = dat.model;
		return dat.set;
	} else {
		return gtk_tree_selection_get_selected (selection, model, iter);
	}
}

static void
emft_popup_delete_response (GtkWidget *dialog, guint response, EMFolderTree *emft)
{
	struct _EMFolderTreePrivate *priv = emft->priv;
	GtkTreeSelection *selection;
	GtkTreeModel *model;
	CamelStore *store;
	CamelException ex;
	GtkTreeIter iter;
	char *path;
	
	gtk_widget_destroy (dialog);
	if (response != GTK_RESPONSE_OK)
		return;
	
	selection = gtk_tree_view_get_selection (priv->treeview);
	emft_selection_get_selected (selection, &model, &iter);
	gtk_tree_model_get (model, &iter, COL_STRING_FOLDER_PATH, &path,
			    COL_POINTER_CAMEL_STORE, &store, -1);
	
	camel_exception_init (&ex);
	emft_popup_delete_folders (store, path, &ex);
	if (camel_exception_is_set (&ex)) {
		e_notice (NULL, GTK_MESSAGE_ERROR, _("Could not delete folder: %s"), ex.desc);
		camel_exception_clear (&ex);
	}
}

static void
emft_popup_delete_folder (GtkWidget *item, EMFolderTree *emft)
{
	struct _EMFolderTreePrivate *priv = emft->priv;
	GtkTreeSelection *selection;
	GtkTreeModel *model;
	GtkTreeIter iter;
	GtkWidget *dialog;
	char *title, *path;
	
	selection = gtk_tree_view_get_selection (priv->treeview);
	emft_selection_get_selected (selection, &model, &iter);
	gtk_tree_model_get (model, &iter, COL_STRING_FOLDER_PATH, &path, -1);
	
	dialog = gtk_message_dialog_new (NULL, GTK_DIALOG_DESTROY_WITH_PARENT | GTK_DIALOG_MODAL,
					 GTK_MESSAGE_QUESTION, GTK_BUTTONS_NONE,
					 _("Really delete folder \"%s\" and all of its subfolders?"),
					 path);
	
	gtk_dialog_add_button ((GtkDialog *) dialog, GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL);
	gtk_dialog_add_button ((GtkDialog *) dialog, GTK_STOCK_DELETE, GTK_RESPONSE_OK);
	
	gtk_dialog_set_default_response ((GtkDialog *) dialog, GTK_RESPONSE_OK);
	gtk_container_set_border_width ((GtkContainer *) dialog, 6); 
	gtk_box_set_spacing ((GtkBox *) ((GtkDialog *) dialog)->vbox, 6);
	
	title = g_strdup_printf (_("Delete \"%s\""), path);
	gtk_window_set_title ((GtkWindow *) dialog, title);
	g_free (title);
	
	g_signal_connect (dialog, "response", G_CALLBACK (emft_popup_delete_response), emft);
	gtk_widget_show (dialog);
}

static void
emft_popup_rename_folder (GtkWidget *item, EMFolderTree *emft)
{
	struct _EMFolderTreePrivate *priv = emft->priv;
	char *prompt, *folder_path, *name, *new_name, *uri;
	GtkTreeSelection *selection;
	const char *full_name, *p;
	gboolean done = FALSE;
	GtkTreeModel *model;
	CamelStore *store;
	GtkTreeIter iter;
	size_t base_len;
	
	selection = gtk_tree_view_get_selection (priv->treeview);
	emft_selection_get_selected (selection, &model, &iter);
	gtk_tree_model_get (model, &iter, COL_STRING_FOLDER_PATH, &folder_path,
			    COL_STRING_DISPLAY_NAME, &name,
			    COL_POINTER_CAMEL_STORE, &store,
			    COL_STRING_URI, &uri, -1);
	
	full_name = folder_path[0] == '/' ? folder_path + 1 : folder_path;
	if ((p = strrchr (full_name, '/')))
		base_len = (size_t) (p - full_name);
	else
		base_len = 0;
	
	prompt = g_strdup_printf (_("Rename the \"%s\" folder to:"), name);
	while (!done) {
		new_name = e_request_string (NULL, _("Rename Folder"), prompt, name);
		if (new_name == NULL || !strcmp (name, new_name)) {
			/* old name == new name */
			done = TRUE;
		} else {
			CamelFolderInfo *fi;
			CamelException ex;
			char *path, *p;
			
			if (base_len > 0) {
				path = g_malloc (base_len + strlen (new_name) + 2);
				memcpy (path, full_name, base_len);
				p = path + base_len;
				*p++ = '/';
				strcpy (p, new_name);
			} else {
				path = g_strdup (new_name);
			}
			
			camel_exception_init (&ex);
			if ((fi = camel_store_get_folder_info (store, path, CAMEL_STORE_FOLDER_INFO_FAST, &ex)) != NULL) {
				camel_store_free_folder_info (store, fi);
				
				e_notice (NULL, GTK_MESSAGE_ERROR,
					  _("A folder named \"%s\" already exists. Please use a different name."),
					  new_name);
			} else {
				const char *oldpath, *newpath;
				
				oldpath = full_name;
				newpath = path;
				
				d(printf ("renaming %s to %s\n", oldpath, newpath));
				
				camel_exception_clear (&ex);
				camel_store_rename_folder (store, oldpath, newpath, &ex);
				if (camel_exception_is_set (&ex)) {
					e_notice (NULL, GTK_MESSAGE_ERROR, _("Could not rename folder: %s"), ex.desc);
					camel_exception_clear (&ex);
				}
				
				done = TRUE;
			}
			
			g_free (path);
		}
		
		g_free (new_name);
	}
}


static void
emft_popup_properties (GtkWidget *item, EMFolderTree *emft)
{
	struct _EMFolderTreePrivate *priv = emft->priv;
	GtkTreeSelection *selection;
	GtkTreeModel *model;
	GtkTreeIter iter;
	char *uri;
	
	selection = gtk_tree_view_get_selection (priv->treeview);
	emft_selection_get_selected (selection, &model, &iter);
	gtk_tree_model_get (model, &iter, COL_STRING_URI, &uri, -1);

	em_folder_properties_show(NULL, NULL, uri);
}

static EMPopupItem emft_popup_menu[] = {
#if 0
	{ EM_POPUP_ITEM, "00.emc.00", N_("_View"), G_CALLBACK (emft_popup_view), NULL, NULL, EM_POPUP_FOLDER_SELECT },
	{ EM_POPUP_ITEM, "00.emc.01", N_("Open in _New Window"), G_CALLBACK (emft_popup_open_new), NULL, NULL, EM_POPUP_FOLDER_SELECT },

	{ EM_POPUP_BAR, "10.emc" },
#endif
	{ EM_POPUP_ITEM, "10.emc.00", N_("_Copy"), G_CALLBACK (emft_popup_copy), NULL, "folder-copy-16.png", EM_POPUP_FOLDER_FOLDER|EM_POPUP_FOLDER_SELECT },
	{ EM_POPUP_ITEM, "10.emc.01", N_("_Move"), G_CALLBACK (emft_popup_move), NULL, "folder-move-16.png", EM_POPUP_FOLDER_FOLDER|EM_POPUP_FOLDER_DELETE },
	
	{ EM_POPUP_BAR, "20.emc" },
	/* FIXME: need to disable for nochildren folders */
	{ EM_POPUP_ITEM, "20.emc.00", N_("_New Folder..."), G_CALLBACK (emft_popup_new_folder), NULL, "folder-mini.png", EM_POPUP_FOLDER_INFERIORS },
	/* FIXME: need to disable for undeletable folders */
	{ EM_POPUP_ITEM, "20.emc.01", N_("_Delete"), G_CALLBACK (emft_popup_delete_folder), NULL, "evolution-trash-mini.png", EM_POPUP_FOLDER_FOLDER|EM_POPUP_FOLDER_DELETE },
	{ EM_POPUP_ITEM, "20.emc.01", N_("_Rename"), G_CALLBACK (emft_popup_rename_folder), NULL, NULL, EM_POPUP_FOLDER_FOLDER|EM_POPUP_FOLDER_DELETE },
	
	{ EM_POPUP_BAR, "80.emc" },
	{ EM_POPUP_ITEM, "80.emc.00", N_("_Properties..."), G_CALLBACK (emft_popup_properties), NULL, "configure_16_folder.xpm", EM_POPUP_FOLDER_FOLDER|EM_POPUP_FOLDER_SELECT },
};

static gboolean
emft_tree_button_press (GtkWidget *treeview, GdkEventButton *event, EMFolderTree *emft)
{
	struct _EMFolderTreePrivate *priv = emft->priv;
	GtkTreeSelection *selection;
	GtkTreeModel *model;
	GtkTreeIter iter;
	GSList *menus = NULL;
	GtkMenu *menu;
	EMPopup *emp;
	EMPopupTarget *target;
	char *uri;
	gboolean isstore;
	int i;
	
	if (event->button != 3)
		return FALSE;
	
	/* handle right-click by opening a context menu */
	emp = em_popup_new ("com.ximian.mail.storageset.popup.select");

	/* FIXME: we really need the folderinfo to build a proper menu */
	selection = gtk_tree_view_get_selection (priv->treeview);
	emft_selection_get_selected (selection, &model, &iter);
	gtk_tree_model_get (model, &iter, COL_STRING_URI, &uri, COL_BOOL_IS_STORE, &isstore, -1);
	target = em_popup_target_new_folder(uri, isstore);
	
	for (i = 0; i < sizeof (emft_popup_menu) / sizeof (emft_popup_menu[0]); i++) {
		EMPopupItem *item = &emft_popup_menu[i];
		
		item->activate_data = emft;
		menus = g_slist_prepend (menus, item);
	}
	
	em_popup_add_items (emp, menus, (GDestroyNotify) g_slist_free);

	menu = em_popup_create_menu_once (emp, target, 0, target->mask);
	
	if (event == NULL || event->type == GDK_KEY_PRESS) {
		/* FIXME: menu pos function */
		gtk_menu_popup (menu, NULL, NULL, NULL, NULL, 0, event->time);
	} else {
		gtk_menu_popup (menu, NULL, NULL, NULL, NULL, event->button, event->time);
	}
	
	return TRUE;
}


static void
emft_tree_selection_changed (GtkTreeSelection *selection, EMFolderTree *emft)
{
	struct _EMFolderTreePrivate *priv = emft->priv;
	GtkTreeModel *model;
	GtkTreeIter iter;
	char *path, *uri;
	
	if (!emft_selection_get_selected (selection, &model, &iter))
		return;
	
	gtk_tree_model_get (model, &iter, COL_STRING_FOLDER_PATH, &path,
			    COL_STRING_URI, &uri, -1);
	
	g_free (priv->selected_uri);
	priv->selected_uri = g_strdup (uri);
	
	g_free (priv->selected_path);
	priv->selected_path = g_strdup (path);
	
	g_signal_emit (emft, signals[FOLDER_SELECTED], 0, path, uri);
}


void
em_folder_tree_set_selected (EMFolderTree *emft, const char *uri)
{
	struct _EMFolderTreeModelStoreInfo *si;
	struct _EMFolderTreeGetFolderInfo *m;
	struct _EMFolderTreePrivate *priv;
	GtkTreeRowReference *row = NULL;
	GtkTreeSelection *selection;
	GtkTreePath *tree_path;
	CamelStore *store;
	CamelException ex;
	const char *top;
	char *path, *p;
	CamelURL *url;
	
	g_return_if_fail (EM_IS_FOLDER_TREE (emft));
	
	priv = emft->priv;
	
	camel_exception_init (&ex);
	if (!(store = (CamelStore *) camel_session_get_service (session, uri, CAMEL_PROVIDER_STORE, &ex))) {
		camel_exception_clear (&ex);
		return;
	}
	
	if (!(si = g_hash_table_lookup (priv->model->store_hash, store))) {
		camel_object_unref (store);
		return;
	}
	
	if (!(url = camel_url_new (uri, NULL))) {
		camel_object_unref (store);
		return;
	}
	
	if (((CamelService *) store)->provider->url_flags & CAMEL_URL_FRAGMENT_IS_PATH)
		path = g_strdup_printf ("/%s", url->fragment ? url->fragment : "");
	else
		path = g_strdup (url->path ? url->path : "/");
	
	top = path[0] == '/' ? path + 1 : path;
	camel_url_free (url);
	
	if (!strcmp (path, "/"))
		row = si->row;
	
	if (row || (row = g_hash_table_lookup (si->path_hash, path))) {
		/* the folder-info node has already been loaded */
		tree_path = gtk_tree_row_reference_get_path (row);
		gtk_tree_view_expand_to_path (priv->treeview, tree_path);
		selection = gtk_tree_view_get_selection (priv->treeview);
		gtk_tree_selection_select_path (selection, tree_path);
		gtk_tree_path_free (tree_path);
		camel_object_unref (store);
		g_free (path);
		return;
	}
	
	/* look for the first of our parent folders that has already been loaded */
	p = path + strlen (path);
	while (p > path) {
		if (*p == '/') {
			*p = '\0';
			
			if ((row = g_hash_table_lookup (si->path_hash, path)))
				break;
		}
		
		p--;
	}
	
	if (row == NULL) {
		/* none of the folders of the desired store have been loaded yet */
		row = si->row;
		top = NULL;
	}

	/* FIXME: this gets all the subfolders of our first loaded
	 * parent folder - ideally we'd only get what we needed, but
	 * it's probably not worth the effort */
	m = mail_msg_new (&get_folder_info_op, NULL, sizeof (struct _EMFolderTreeGetFolderInfo));
	m->root = gtk_tree_row_reference_copy (row);
	m->store = store;
	m->emft = emft;
	m->top = top ? g_strdup (top) : NULL;
	m->flags = CAMEL_STORE_FOLDER_INFO_FAST | CAMEL_STORE_FOLDER_INFO_RECURSIVE;
	m->select_uri = g_strdup (uri);
	
	g_free (path);
	
	e_thread_put (mail_thread_new, (EMsg *) m);
}


const char *
em_folder_tree_get_selected_uri (EMFolderTree *emft)
{
	g_return_val_if_fail (EM_IS_FOLDER_TREE (emft), NULL);
	
	return emft->priv->selected_uri;
}


const char *
em_folder_tree_get_selected_path (EMFolderTree *emft)
{
	g_return_val_if_fail (EM_IS_FOLDER_TREE (emft), NULL);
	
	return emft->priv->selected_path;
}


EMFolderTreeModel *
em_folder_tree_get_model (EMFolderTree *emft)
{
	g_return_val_if_fail (EM_IS_FOLDER_TREE (emft), NULL);
	
	return emft->priv->model;
}


static gboolean
emft_save_state (EMFolderTree *emft)
{
	struct _EMFolderTreePrivate *priv = emft->priv;
	
	em_folder_tree_model_save_expanded (priv->model);
	priv->save_state_id = 0;
	
	return FALSE;
}


static void
emft_queue_save_state (EMFolderTree *emft)
{
	struct _EMFolderTreePrivate *priv = emft->priv;
	
	if (priv->save_state_id != 0)
		return;
	
	priv->save_state_id = g_timeout_add (1000, (GSourceFunc) emft_save_state, emft);
}
