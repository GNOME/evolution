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

#include <libxml/tree.h>

#include <gtk/gtk.h>
#include <gdk-pixbuf/gdk-pixbuf.h>

#include <camel/camel-session.h>
#include <camel/camel-store.h>
#include <camel/camel-folder.h>
#include <camel/camel-vee-store.h>
#include <camel/camel-vtrash-folder.h>
#include <camel/camel-stream-mem.h>
#include <camel/camel-file-utils.h>
#include <camel/camel-stream-fs.h>

#include "e-util/e-mktemp.h"
#include "e-util/e-request.h"
#include "e-util/e-icon-factory.h"

#include "widgets/misc/e-error.h"

#include "filter/vfolder-rule.h"

#include "mail-mt.h"
#include "mail-ops.h"
#include "mail-tools.h"
#include "mail-config.h"
#include "mail-component.h"
#include "mail-vfolder.h"

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

	guint32 excluded;

	gboolean do_multiselect;
	/* when doing a multiselect, folders that we didn't find */
	GList *lost_folders;
	
	guint save_state_id;
	
	guint autoscroll_id;
	guint autoexpand_id;
	GtkTreeRowReference *autoexpand_row;
	
	guint loading_row_id;
	guint loaded_row_id;
	
	GtkTreeRowReference *drag_row;
};

enum {
	FOLDER_ACTIVATED,  /* aka double-clicked or user hit enter */
	FOLDER_SELECTED,
	LAST_SIGNAL
};

/* Drag & Drop types */
enum DndDragType {
	DND_DRAG_TYPE_FOLDER,          /* drag an evo folder */
	DND_DRAG_TYPE_TEXT_URI_LIST,   /* drag to an mbox file */
	NUM_DRAG_TYPES
};

enum DndDropType {
	DND_DROP_TYPE_UID_LIST,        /* drop a list of message uids */
	DND_DROP_TYPE_FOLDER,          /* drop an evo folder */
	DND_DROP_TYPE_MESSAGE_RFC822,  /* drop a message/rfc822 stream */
	DND_DROP_TYPE_TEXT_URI_LIST,   /* drop an mbox file */
	NUM_DROP_TYPES
};

static GtkTargetEntry drag_types[] = {
	{ "x-folder",         0, DND_DRAG_TYPE_FOLDER         },
	{ "text/uri-list",    0, DND_DRAG_TYPE_TEXT_URI_LIST  },
};

static GtkTargetEntry drop_types[] = {
	{ "x-uid-list" ,      0, DND_DROP_TYPE_UID_LIST       },
	{ "x-folder",         0, DND_DROP_TYPE_FOLDER         },
	{ "message/rfc822",   0, DND_DROP_TYPE_MESSAGE_RFC822 },
	{ "text/uri-list",    0, DND_DROP_TYPE_TEXT_URI_LIST  },
};

static GdkAtom drag_atoms[NUM_DRAG_TYPES];
static GdkAtom drop_atoms[NUM_DROP_TYPES];

static guint signals[LAST_SIGNAL] = { 0 };

extern CamelSession *session;

static void em_folder_tree_class_init (EMFolderTreeClass *klass);
static void em_folder_tree_init (EMFolderTree *emft);
static void em_folder_tree_destroy (GtkObject *obj);
static void em_folder_tree_finalize (GObject *obj);

static gboolean emft_save_state (EMFolderTree *emft);
static void emft_queue_save_state (EMFolderTree *emft);

static void emft_update_model_expanded_state (struct _EMFolderTreePrivate *priv, GtkTreeIter *iter, gboolean expanded);

static void emft_tree_row_activated (GtkTreeView *treeview, GtkTreePath *path, GtkTreeViewColumn *column, EMFolderTree *emft);
static void emft_tree_row_collapsed (GtkTreeView *treeview, GtkTreeIter *root, GtkTreePath *path, EMFolderTree *emft);
static void emft_tree_row_expanded (GtkTreeView *treeview, GtkTreeIter *root, GtkTreePath *path, EMFolderTree *emft);
static gboolean emft_tree_button_press (GtkTreeView *treeview, GdkEventButton *event, EMFolderTree *emft);
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
			      em_marshal_VOID__STRING_STRING_UINT,
			      G_TYPE_NONE, 3,
			      G_TYPE_STRING,
			      G_TYPE_STRING,
			      G_TYPE_UINT);
	
	signals[FOLDER_ACTIVATED] =
		g_signal_new ("folder-activated",
			      G_OBJECT_CLASS_TYPE (object_class),
			      G_SIGNAL_RUN_FIRST,
			      G_STRUCT_OFFSET (EMFolderTreeClass, folder_activated),
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
		folder_icons[FOLDER_ICON_NORMAL] = e_icon_factory_get_icon ("stock_folder", E_ICON_SIZE_MENU);
		folder_icons[FOLDER_ICON_INBOX] = e_icon_factory_get_icon ("stock_inbox", E_ICON_SIZE_MENU);
		folder_icons[FOLDER_ICON_OUTBOX] = e_icon_factory_get_icon ("stock_outbox", E_ICON_SIZE_MENU);
		folder_icons[FOLDER_ICON_TRASH] = e_icon_factory_get_icon ("stock_delete", E_ICON_SIZE_MENU);
		folder_icons[FOLDER_ICON_JUNK] = e_icon_factory_get_icon ("stock_spam", E_ICON_SIZE_MENU);
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

static gboolean
emft_select_func(GtkTreeSelection *selection, GtkTreeModel *model, GtkTreePath *path, gboolean selected, gpointer data)
{
	EMFolderTree *emft = data;
	gboolean is_store;
	guint32 flags;
	GtkTreeIter iter;

	/* NB: This will be called with selection==NULL from tree_row_activated */

	if (emft->priv->excluded == 0)
		return TRUE;

	if (!gtk_tree_model_get_iter(model, &iter, path))
		return TRUE;

	gtk_tree_model_get(model, &iter, COL_UINT_FLAGS, &flags, COL_BOOL_IS_STORE, &is_store, -1);
	if (is_store)
		flags |= CAMEL_FOLDER_NOSELECT;

	return (flags & emft->priv->excluded) == 0;
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
	
	if (priv->loaded_row_id != 0) {
		g_signal_handler_disconnect (priv->model, priv->loaded_row_id);
		priv->loaded_row_id = 0;
	}
	
	if (priv->save_state_id != 0) {
		g_source_remove (priv->save_state_id);
		emft_save_state (emft);
	}
	
	if (priv->autoscroll_id != 0) {
		g_source_remove (priv->autoscroll_id);
		priv->autoscroll_id = 0;
	}
	
	if (priv->autoexpand_id != 0) {
		gtk_tree_row_reference_free (priv->autoexpand_row);
		priv->autoexpand_row = NULL;
		
		g_source_remove (priv->autoexpand_id);
		priv->autoexpand_id = 0;
	}
	
	priv->treeview = NULL;
	priv->model = NULL;
	
	GTK_OBJECT_CLASS (parent_class)->destroy (obj);
}

static GtkTreeView *
folder_tree_new (EMFolderTree *emft, EMFolderTreeModel *model)
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
	gtk_tree_selection_set_select_function(selection, emft_select_func, emft, NULL);
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
	priv->treeview = folder_tree_new (emft, model);
	gtk_widget_show ((GtkWidget *) priv->treeview);
	
	g_signal_connect (priv->treeview, "row-expanded", G_CALLBACK (emft_tree_row_expanded), emft);
	g_signal_connect (priv->treeview, "row-collapsed", G_CALLBACK (emft_tree_row_collapsed), emft);
	g_signal_connect (priv->treeview, "row-activated", G_CALLBACK (emft_tree_row_activated), emft);
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

static void
emft_expand_node (EMFolderTreeModel *model, const char *key, EMFolderTree *emft)
{
	struct _EMFolderTreePrivate *priv = emft->priv;
	struct _EMFolderTreeModelStoreInfo *si;
	extern CamelStore *vfolder_store;
	GtkTreeRowReference *row;
	GtkTreePath *path;
	EAccount *account;
	CamelStore *store;
	const char *p;
	char *uid;
	size_t n;
	
	if (!(p = strchr (key, '/')))
		n = strlen (key);
	else
		n = (p - key);
	
	uid = g_alloca (n + 1);
	memcpy (uid, key, n);
	uid[n] = '\0';
	
	if ((account = mail_config_get_account_by_uid (uid)) && account->enabled) {
		CamelException ex;
		
		camel_exception_init (&ex);
		store = (CamelStore *) camel_session_get_service (session, account->source->url, CAMEL_PROVIDER_STORE, &ex);
		camel_exception_clear (&ex);
		
		if (store == NULL)
			return;
	} else if (!strcmp (uid, "vfolder")) {
		if (!(store = vfolder_store))
			return;
		
		camel_object_ref (store);
	} else if (!strcmp (uid, "local")) {
		if (!(store = mail_component_peek_local_store (NULL)))
			return;
		
		camel_object_ref (store);
	} else {
		return;
	}
	
	if (!(si = g_hash_table_lookup (priv->model->store_hash, store))) {
		camel_object_unref (store);
		return;
	}
	
	camel_object_unref (store);
	
	if (p != NULL) {
		if (!(row = g_hash_table_lookup (si->path_hash, p)))
			return;
	} else
		row = si->row;
	
	path = gtk_tree_row_reference_get_path (row);
	gtk_tree_view_expand_row (priv->treeview, path, FALSE);
	gtk_tree_path_free (path);
}

static void
emft_maybe_expand_row (EMFolderTreeModel *model, GtkTreePath *tree_path, GtkTreeIter *iter, EMFolderTree *emft)
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
	        key = g_strdup_printf ("%s%s", account->uid, path);
	} else if (CAMEL_IS_VEE_STORE (store)) {
		/* vfolder store */
		key = g_strdup_printf ("vfolder%s", path);
	} else {
		/* local store */
		key = g_strdup_printf ("local%s", path);
	}
	
	if (em_folder_tree_model_get_expanded (model, key)) {
		gtk_tree_view_expand_to_path (emft->priv->treeview, tree_path);
		gtk_tree_view_expand_row (emft->priv->treeview, tree_path, FALSE);
	}
	
	g_free (key);
}

GtkWidget *
em_folder_tree_new_with_model (EMFolderTreeModel *model)
{
	EMFolderTree *emft;
	
	emft = g_object_new (EM_TYPE_FOLDER_TREE, NULL);
	em_folder_tree_construct (emft, model);
	g_object_ref (model);
	
	em_folder_tree_model_expand_foreach (model, emft_expand_node, emft);
	
	emft->priv->loading_row_id = g_signal_connect (model, "loading-row", G_CALLBACK (emft_maybe_expand_row), emft);
	emft->priv->loaded_row_id = g_signal_connect (model, "loaded-row", G_CALLBACK (emft_maybe_expand_row), emft);
	
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
tree_drag_data_delete(GtkWidget *widget, GdkDragContext *context, EMFolderTree *emft)
{
	struct _EMFolderTreePrivate *priv = emft->priv;
	GtkTreePath *src_path;
	const char *full_name;
	gboolean is_store;
	CamelStore *store;
	CamelException ex;
	GtkTreeIter iter;
	char *path;
	
	if (!priv->drag_row || (src_path = gtk_tree_row_reference_get_path (priv->drag_row)))
		return;
	
	if (!gtk_tree_model_get_iter((GtkTreeModel *)priv->model, &iter, src_path))
		goto fail;

	gtk_tree_model_get((GtkTreeModel *)priv->model, &iter,
			   COL_POINTER_CAMEL_STORE, &store,
			   COL_STRING_FOLDER_PATH, &path,
			   COL_BOOL_IS_STORE, &is_store, -1);
	
	if (is_store)
		goto fail;
	
	full_name = path[0] == '/' ? path + 1 : path;
	
	camel_exception_init(&ex);
	camel_store_delete_folder(store, full_name, &ex);
	if (camel_exception_is_set(&ex))
		camel_exception_clear(&ex);
fail:
	gtk_tree_path_free(src_path);
}

static void
tree_drag_data_get(GtkWidget *widget, GdkDragContext *context, GtkSelectionData *selection, guint info, guint time, EMFolderTree *emft)
{
	struct _EMFolderTreePrivate *priv = emft->priv;
	GtkTreePath *src_path;
	const char *full_name;
	CamelFolder *folder;
	CamelStore *store;
	CamelException ex;
	GtkTreeIter iter;
	char *path, *uri;
	
	if (!priv->drag_row || !(src_path = gtk_tree_row_reference_get_path(priv->drag_row)))
		return;
	
	if (!gtk_tree_model_get_iter((GtkTreeModel *)priv->model, &iter, src_path))
		goto fail;
	
	gtk_tree_model_get((GtkTreeModel *)priv->model, &iter,
			   COL_POINTER_CAMEL_STORE, &store,
			   COL_STRING_FOLDER_PATH, &path,
			   COL_STRING_URI, &uri, -1);
	
	/* make sure user isn't trying to drag on a placeholder row */
	if (path == NULL)
		goto fail;
	
	full_name = path[0] == '/' ? path + 1 : path;
	
	camel_exception_init(&ex);
	
	switch (info) {
	case DND_DRAG_TYPE_FOLDER:
		/* dragging to a new location in the folder tree */
		gtk_selection_data_set(selection, drag_atoms[info], 8, uri, strlen (uri) + 1);
		break;
	case DND_DRAG_TYPE_TEXT_URI_LIST:
		/* dragging to nautilus or something, probably */
		if ((folder = camel_store_get_folder(store, full_name, 0, &ex))) {
			GPtrArray *uids = camel_folder_get_uids(folder);

			em_utils_selection_set_urilist(selection, folder, uids);
			camel_folder_free_uids(folder, uids);
			camel_object_unref(folder);
		}
		break;
	default:
		abort();
	}
	
	if (camel_exception_is_set(&ex))
		camel_exception_clear(&ex);
fail:
	gtk_tree_path_free(src_path);
}

/* Drop handling */
struct _DragDataReceivedAsync {
	struct _mail_msg msg;
	
	/* input data */
	GdkDragContext *context;

	union {
		CamelStreamMem *rfc822;
		char *folder;
		char **urilist;
		struct {
			char *uri;
			GPtrArray *uids;
		} uidlist;
	} selection;

	CamelStore *store;
	char *full_name;
	gboolean move;
	guint info;
	
	/* output data */
	gboolean moved;
};

static void
emft_drop_uid_list(struct _DragDataReceivedAsync *m, CamelFolder *dest)
{
	CamelFolder *src;

	d(printf(" * drop uid list from '%s'\n", m->selection.uidlist.uri));

	if (!(src = mail_tool_uri_to_folder(m->selection.uidlist.uri, 0, &m->msg.ex)))
		return;
	
	camel_folder_transfer_messages_to(src, m->selection.uidlist.uids, dest, NULL, m->move, &m->msg.ex);
	camel_object_unref(src);
	
	m->moved = m->move && !camel_exception_is_set(&m->msg.ex);
}

static void
emft_drop_folder_rec (CamelStore *store, CamelFolderInfo *fi, const char *parent_name, CamelException *ex)
{
	CamelFolder *src, *dest;
	CamelFolderInfo *nfi;
	char *new_name;
	
	while (fi != NULL) {
		if (!(src = mail_tool_uri_to_folder (fi->uri, 0, ex)))
			break;
		
		/* handles dropping to the root properly */
		if (parent_name[0])
			new_name = g_strdup_printf ("%s/%s", parent_name, src->name);
		else
			new_name = g_strdup (src->name);
		
		if ((nfi = camel_store_create_folder (store, parent_name, src->name, ex))) {
			camel_store_free_folder_info (store, nfi);
			
			if (camel_store_supports_subscriptions (store))
				camel_store_subscribe_folder (store, new_name, ex);
			
			/* copy the folder to the new location */
			if ((dest = camel_store_get_folder (store, new_name, 0, ex))) {
				GPtrArray *uids;
				
				uids = camel_folder_get_uids (src);
				camel_folder_transfer_messages_to (src, uids, dest, NULL, FALSE, ex);
				camel_folder_free_uids (src, uids);
				
				camel_object_unref (dest);
			}
		}
		
		camel_object_unref (src);
		
		if (fi->child)
			emft_drop_folder_rec (store, fi->child, new_name, ex);
		
		g_free (new_name);
		fi = fi->next;
	}
}

static void
emft_drop_folder(struct _DragDataReceivedAsync *m)
{
	CamelFolder *src;
	char *new_name;

	d(printf(" * Drop folder '%s' onto '%s'\n", m->selection.folder, m->full_name));

	if (!(src = mail_tool_uri_to_folder(m->selection.folder, 0, &m->msg.ex)))
		return;
	
	/* handles dropping to the root properly */
	if (m->full_name[0])
		new_name = g_strdup_printf("%s/%s", m->full_name, src->name);
	else
		new_name = g_strdup(src->name);
	
	if (src->parent_store == m->store && m->move) {
		/* simple case, rename */
		camel_store_rename_folder(m->store, src->full_name, new_name, &m->msg.ex);
		m->moved = !camel_exception_is_set (&m->msg.ex);
	} else {
		CamelFolderInfo *fi, *nfi;
		
		/* FIXME: should check we're not coming from a vfolder, otherwise bad stuff could happen */
		
		if ((fi = camel_store_get_folder_info (src->parent_store, src->full_name, CAMEL_STORE_FOLDER_INFO_FAST |
						       CAMEL_STORE_FOLDER_INFO_RECURSIVE, &m->msg.ex))) {
			if (!(nfi = camel_store_get_folder_info (m->store, new_name, CAMEL_STORE_FOLDER_INFO_FAST, &m->msg.ex))) {
				/* Good. The folder doesn't already exist... */
				camel_exception_clear (&m->msg.ex);
				emft_drop_folder_rec (m->store, fi, m->full_name, &m->msg.ex);
			}
			
			camel_store_free_folder_info (src->parent_store, fi);
		}
	}
	
	g_free(new_name);
	camel_object_unref(src);
}

static gboolean
emft_import_message_rfc822 (CamelFolder *dest, CamelStream *stream, gboolean scan_from, CamelException *ex)
{
	CamelMimeParser *mp;
	
	mp = camel_mime_parser_new ();
	camel_mime_parser_scan_from (mp, scan_from);
	camel_mime_parser_init_with_stream (mp, stream);
	
	while (camel_mime_parser_step (mp, 0, 0) == CAMEL_MIME_PARSER_STATE_FROM) {
		CamelMessageInfo *info;
		CamelMimeMessage *msg;
		
		msg = camel_mime_message_new ();
		if (camel_mime_part_construct_from_parser (CAMEL_MIME_PART (msg), mp) == -1) {
			camel_object_unref (msg);
			camel_object_unref (mp);
			return FALSE;
		}
		
		/* append the message to the folder... */
		info = g_new0 (CamelMessageInfo, 1);
		camel_folder_append_message (dest, msg, info, NULL, ex);
		camel_object_unref (msg);
		
		if (camel_exception_is_set (ex)) {
			camel_object_unref (mp);
			return FALSE;
		}
		
		/* skip over the FROM_END state */
		camel_mime_parser_step (mp, 0, 0);
	}
	
	camel_object_unref (mp);
	
	return TRUE;
}

static void
emft_drop_message_rfc822(struct _DragDataReceivedAsync *m, CamelFolder *dest)
{
	gboolean scan_from;

	d(printf(" * drop message/rfc822\n"));

	scan_from = m->selection.rfc822->buffer->len > 5
		&& !strncmp(m->selection.rfc822->buffer->data, "From ", 5);
	emft_import_message_rfc822(dest, (CamelStream *)m->selection.rfc822, scan_from, &m->msg.ex);
}

static void
emft_drop_text_uri_list(struct _DragDataReceivedAsync *m, CamelFolder *dest)
{
	CamelStream *stream;
	CamelURL *url;
	int fd, i, go=1;

	d(printf(" * drop uri list\n"));

	for (i = 0; go && m->selection.urilist[i] != NULL; i++) {
		d(printf("   - '%s'\n", (char *)m->selection.urilist[i]));

		url = camel_url_new(m->selection.urilist[i], NULL);
		if (url == NULL)
			continue;

		if (strcmp(url->protocol, "file") == 0
		    && (fd = open(url->path, O_RDONLY)) != -1) {
			stream = camel_stream_fs_new_with_fd(fd);
			go = emft_import_message_rfc822(dest, stream, TRUE, &m->msg.ex);
			camel_object_unref(stream);
		}
		camel_url_free(url);
	}
}

static char *
emft_drop_async_desc (struct _mail_msg *mm, int done)
{
	struct _DragDataReceivedAsync *m = (struct _DragDataReceivedAsync *) mm;
	CamelURL *url;
	char *buf;
	
	if (m->info == DND_DROP_TYPE_FOLDER) {
		url = camel_url_new (m->selection.folder, NULL);
		
		if (m->move)
			buf = g_strdup_printf (_("Moving folder %s"), url->fragment ? url->fragment : url->path + 1);
		else
			buf = g_strdup_printf (_("Copying folder %s"), url->fragment ? url->fragment : url->path + 1);
		
		camel_url_free (url);
		
		return buf;
	} else {
		if (m->move)
			return g_strdup_printf (_("Moving messages into folder %s"), m->full_name);
		else
			return g_strdup_printf (_("Copying messages into folder %s"), m->full_name);
	}
}

static void
emft_drop_async_drop (struct _mail_msg *mm)
{
	struct _DragDataReceivedAsync *m = (struct _DragDataReceivedAsync *) mm;
	CamelFolder *folder;
	
	/* for types other than folder, we can't drop to the root path */
	if (m->info == DND_DROP_TYPE_FOLDER) {
		/* copy or move (aka rename) a folder */
		emft_drop_folder(m);
	} else if (m->full_name[0] == 0) {
		camel_exception_set (&mm->ex, CAMEL_EXCEPTION_SYSTEM,
				     _("Cannot drop message(s) into toplevel store"));
	} else if ((folder = camel_store_get_folder (m->store, m->full_name, 0, &mm->ex))) {
		switch (m->info) {
		case DND_DROP_TYPE_UID_LIST:
			/* import a list of uids from another evo folder */
			emft_drop_uid_list(m, folder);
			break;
		case DND_DROP_TYPE_MESSAGE_RFC822:
			/* import a message/rfc822 stream */
			emft_drop_message_rfc822(m, folder);
			break;
		case DND_DROP_TYPE_TEXT_URI_LIST:
			/* import an mbox, maildir, or mh folder? */
			emft_drop_text_uri_list(m, folder);
			break;
		default:
			abort();
		}
		camel_object_unref(folder);
	}
}

static void
emft_drop_async_done (struct _mail_msg *mm)
{
	struct _DragDataReceivedAsync *m = (struct _DragDataReceivedAsync *) mm;
	gboolean success, delete;
	
	success = !camel_exception_is_set (&mm->ex);
	delete = success && m->move && !m->moved;
	
	gtk_drag_finish (m->context, success, delete, GDK_CURRENT_TIME);
}

static void
emft_drop_async_free (struct _mail_msg *mm)
{
	struct _DragDataReceivedAsync *m = (struct _DragDataReceivedAsync *) mm;
	
	g_object_unref(m->context);
	camel_object_unref(m->store);
	g_free(m->full_name);

	switch (m->info) {
	case DND_DROP_TYPE_FOLDER:
		g_free(m->selection.folder);
		break;
	case DND_DROP_TYPE_UID_LIST:
		g_free(m->selection.uidlist.uri);
		em_utils_uids_free(m->selection.uidlist.uids);
		break;
	case DND_DROP_TYPE_MESSAGE_RFC822:
		camel_object_unref(m->selection.rfc822);
		break;
	case DND_DROP_TYPE_TEXT_URI_LIST:
		g_strfreev(m->selection.urilist);
		break;
	default:
		abort();
	}
}

static struct _mail_msg_op emft_drop_async_op = {
	emft_drop_async_desc,
	emft_drop_async_drop,
	emft_drop_async_done,
	emft_drop_async_free,
};

static void
tree_drag_data_received(GtkWidget *widget, GdkDragContext *context, int x, int y, GtkSelectionData *selection, guint info, guint time, EMFolderTree *emft)
{
	struct _EMFolderTreePrivate *priv = emft->priv;
	GtkTreeViewDropPosition pos;
	GtkTreePath *dest_path;
	struct _DragDataReceivedAsync *m;
	const char *full_name;
	CamelStore *store;
	GtkTreeIter iter;
	char *path, *tmp;
	int i;
	
	if (!gtk_tree_view_get_dest_row_at_pos (priv->treeview, x, y, &dest_path, &pos))
		return;
	
	/* this means we are receiving no data */
	if (!selection->data || selection->length == -1) {
		gtk_drag_finish(context, FALSE, FALSE, GDK_CURRENT_TIME);
		return;
	}
	
	if (!gtk_tree_model_get_iter((GtkTreeModel *)priv->model, &iter, dest_path)) {
		gtk_drag_finish(context, FALSE, FALSE, GDK_CURRENT_TIME);
		return;
	}
	
	gtk_tree_model_get((GtkTreeModel *)priv->model, &iter,
			   COL_POINTER_CAMEL_STORE, &store,
			   COL_STRING_FOLDER_PATH, &path, -1);
	
	/* make sure user isn't try to drop on a placeholder row */
	if (path == NULL) {
		gtk_drag_finish (context, FALSE, FALSE, GDK_CURRENT_TIME);
		return;
	}

	full_name = path[0] == '/' ? path + 1 : path;
	
	m = mail_msg_new (&emft_drop_async_op, NULL, sizeof (struct _DragDataReceivedAsync));
	m->context = context;
	g_object_ref(context);
	m->store = store;
	camel_object_ref(store);
	m->full_name = g_strdup (full_name);
	m->move = context->action == GDK_ACTION_MOVE;
	m->info = info;

	switch (info) {
	case DND_DROP_TYPE_FOLDER:
		m->selection.folder = g_strdup(selection->data);
		break;
	case DND_DROP_TYPE_UID_LIST:
		em_utils_selection_get_uidlist(selection, &m->selection.uidlist.uri, &m->selection.uidlist.uids);
		break;
	case DND_DROP_TYPE_MESSAGE_RFC822:
		m->selection.rfc822 = (CamelStreamMem *)camel_stream_mem_new_with_buffer(selection->data, selection->length);
		break;
	case DND_DROP_TYPE_TEXT_URI_LIST:
		tmp = g_strndup(selection->data, selection->length);
		m->selection.urilist = g_strsplit(tmp, "\n", 0);
		g_free(tmp);
		for (i=0;m->selection.urilist[i];i++)
			g_strstrip(m->selection.urilist[i]);
		break;
	default:
		abort();
	}
	
	e_thread_put (mail_thread_new, (EMsg *) m);
}

static gboolean
is_special_local_folder (const char *name)
{
	return (!strcmp (name, "Drafts") || !strcmp (name, "Inbox") || !strcmp (name, "Outbox") || !strcmp (name, "Sent"));
}

static GdkAtom
emft_drop_target(EMFolderTree *emft, GdkDragContext *context, GtkTreePath *path)
{
	struct _EMFolderTreePrivate *p = emft->priv;
	char *uri, *folder_path, *src_uri = NULL;
	CamelStore *local, *sstore, *dstore;
	gboolean is_store;
	GtkTreeIter iter;
	GList *targets;
	
	/* This is a bit of a mess, but should handle all the cases properly */

	if (!gtk_tree_model_get_iter((GtkTreeModel *)p->model, &iter, path))
		return GDK_NONE;
	
	gtk_tree_model_get((GtkTreeModel *)p->model, &iter, COL_BOOL_IS_STORE, &is_store,
			   COL_STRING_FOLDER_PATH, &folder_path,
			   COL_POINTER_CAMEL_STORE, &dstore,
			   COL_STRING_URI, &uri, -1);
	
	local = mail_component_peek_local_store (NULL);
	
	targets = context->targets;
	
	/* Check for special destinations */
	if (uri && folder_path) {
		folder_path = folder_path[0] == '/' ? folder_path + 1 : folder_path;
		
#if 0
		/* only allow copying/moving folders (not messages) into the local Outbox */
		if (dstore == local && !strcmp (folder_path, "Outbox")) {
			GdkAtom xfolder;
			
			xfolder = drop_atoms[DND_DROP_TYPE_FOLDER];
			while (targets != NULL) {
				if (targets->data == (gpointer) xfolder)
					return xfolder;
				
				targets = targets->next;
			}
			
			return GDK_NONE;
		}
#endif
		
		/* don't allow copying/moving into the UNMATCHED vfolder */
		if (!strncmp (uri, "vfolder:", 8) && !strcmp (folder_path, CAMEL_UNMATCHED_NAME))
			return GDK_NONE;
		
		/* don't allow copying/moving into a vTrash/vJunk folder */
		if (!strcmp (folder_path, CAMEL_VTRASH_NAME)
		    || !strcmp (folder_path, CAMEL_VJUNK_NAME))
			return GDK_NONE;
	}
	
	if (p->drag_row) {
		GtkTreePath *src_path = gtk_tree_row_reference_get_path(p->drag_row);
		
		if (src_path) {
			if (gtk_tree_model_get_iter((GtkTreeModel *)p->model, &iter, src_path))
				gtk_tree_model_get((GtkTreeModel *)p->model, &iter,
						   COL_POINTER_CAMEL_STORE, &sstore,
						   COL_STRING_URI, &src_uri, -1);
			
			/* can't dnd onto itself or below itself - bad things happen,
			   no point dragging to where we were either */
			if (gtk_tree_path_compare(path, src_path) == 0
			    || gtk_tree_path_is_descendant(path, src_path)
			    || (gtk_tree_path_is_ancestor(path, src_path)
				&& gtk_tree_path_get_depth(path) == gtk_tree_path_get_depth(src_path)-1)) {
				gtk_tree_path_free(src_path);
				return GDK_NONE;
			}
			
			gtk_tree_path_free(src_path);
		}
	}
	
	/* Check for special sources, and vfolder stuff */
	if (src_uri) {
		CamelURL *url;
		char *path;
		
		/* FIXME: this is a total hack, but i think all we can do at present */
		/* Check for dragging from special folders which can't be moved/copied */
		url = camel_url_new(src_uri, NULL);
		path = url->fragment?url->fragment:url->path;
		if (path && path[0]) {
			/* don't allow moving any of the the local special folders */
			if (sstore == local && is_special_local_folder (path)) {
				GdkAtom xfolder;
				
				camel_url_free (url);
				
				/* TODO: not sure if this is legal, but it works, force copy for special local folders */
				context->suggested_action = GDK_ACTION_COPY;
				xfolder = drop_atoms[DND_DROP_TYPE_FOLDER];
				while (targets != NULL) {
					if (targets->data == (gpointer) xfolder)
						return xfolder;
					
					targets = targets->next;
				}
				
				return GDK_NONE;
			}
			
			/* don't allow copying/moving of the UNMATCHED vfolder */
			if (!strcmp (url->protocol, "vfolder") && !strcmp (path, CAMEL_UNMATCHED_NAME)) {
				camel_url_free (url);
				return GDK_NONE;
			}
			
			/* don't allow copying/moving of any vTrash/vJunk folder nor maildir 'inbox' */
			if (strcmp(path, CAMEL_VTRASH_NAME) == 0
			    || strcmp(path, CAMEL_VJUNK_NAME) == 0
			    /* Dont allow drag from maildir 'inbox' */
			    || strcmp(path, ".") == 0) {
				camel_url_free(url);
				return GDK_NONE;
			}
		}
		camel_url_free(url);
		
		/* vFolders can only be dropped into other vFolders */
		if (strncmp(src_uri, "vfolder:", 8) == 0) {
			/* TODO: not sure if this is legal, but it works, force move only for vfolders */
			context->suggested_action = GDK_ACTION_MOVE;

			if (uri && strncmp(uri, "vfolder:", 8) == 0) {
				GdkAtom xfolder;

				xfolder = drop_atoms[DND_DROP_TYPE_FOLDER];
				while (targets != NULL) {
					if (targets->data == (gpointer) xfolder)
						return xfolder;
					
					targets = targets->next;
				}
			}

			return GDK_NONE;
		}
	}

	/* can't drag anything but a vfolder into a vfolder */
	if (uri && strncmp(uri, "vfolder:", 8) == 0)
		return GDK_NONE;

	/* Now we either have a store or a normal folder */
	
	if (is_store) {
		GdkAtom xfolder;

		xfolder = drop_atoms[DND_DROP_TYPE_FOLDER];
		while (targets != NULL) {
			if (targets->data == (gpointer) xfolder)
				return xfolder;
			
			targets = targets->next;
		}
	} else {
		int i;

		while (targets != NULL) {
			for (i = 0; i < NUM_DROP_TYPES; i++) {
				if (targets->data == (gpointer) drop_atoms[i])
					return drop_atoms[i];
			}
			
			targets = targets->next;
		}
	}
	
	return GDK_NONE;
}

static gboolean
tree_drag_drop (GtkWidget *widget, GdkDragContext *context, int x, int y, guint time, EMFolderTree *emft)
{
	struct _EMFolderTreePrivate *priv = emft->priv;
	GtkTreeViewColumn *column;
	int cell_x, cell_y;
	GtkTreePath *path;
	GdkAtom target;
	
	if (priv->autoscroll_id != 0) {
		g_source_remove (priv->autoscroll_id);
		priv->autoscroll_id = 0;
	}
	
	if (priv->autoexpand_id != 0) {
		gtk_tree_row_reference_free (priv->autoexpand_row);
		priv->autoexpand_row = NULL;
		
		g_source_remove (priv->autoexpand_id);
		priv->autoexpand_id = 0;
	}
	
	if (!gtk_tree_view_get_path_at_pos (priv->treeview, x, y, &path, &column, &cell_x, &cell_y))
		return FALSE;
	
	target = emft_drop_target(emft, context, path);
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
	struct _EMFolderTreePrivate *priv = emft->priv;
	
	if (priv->autoscroll_id != 0) {
		g_source_remove (priv->autoscroll_id);
		priv->autoscroll_id = 0;
	}
	
	if (priv->autoexpand_id != 0) {
		gtk_tree_row_reference_free (priv->autoexpand_row);
		priv->autoexpand_row = NULL;
		
		g_source_remove (priv->autoexpand_id);
		priv->autoexpand_id = 0;
	}
	
	gtk_tree_view_set_drag_dest_row(emft->priv->treeview, NULL, GTK_TREE_VIEW_DROP_BEFORE);
}


#define SCROLL_EDGE_SIZE 15

static gboolean
tree_autoscroll (EMFolderTree *emft)
{
	struct _EMFolderTreePrivate *priv = emft->priv;
	GtkAdjustment *vadjustment;
	GdkRectangle rect;
	GdkWindow *window;
	int offset, y;
	float value;
	
	/* get the y pointer position relative to the treeview */
	window = gtk_tree_view_get_bin_window (priv->treeview);
	gdk_window_get_pointer (window, NULL, &y, NULL);
	
	/* rect is in coorinates relative to the scrolled window relative to the treeview */
	gtk_tree_view_get_visible_rect (priv->treeview, &rect);
	
	/* move y into the same coordinate system as rect */
	y += rect.y;
	
	/* see if we are near the top edge */
	if ((offset = y - (rect.y + 2 * SCROLL_EDGE_SIZE)) > 0) {
		/* see if we are near the bottom edge */
		if ((offset = y - (rect.y + rect.height - 2 * SCROLL_EDGE_SIZE)) < 0)
			return TRUE;
	}
	
	vadjustment = gtk_tree_view_get_vadjustment (priv->treeview);
	
	value = CLAMP (vadjustment->value + offset, 0.0, vadjustment->upper - vadjustment->page_size);
	gtk_adjustment_set_value (vadjustment, value);
	
	return TRUE;
}

static gboolean
tree_autoexpand (EMFolderTree *emft)
{
	struct _EMFolderTreePrivate *priv = emft->priv;
	GtkTreePath *path;
	
	path = gtk_tree_row_reference_get_path (priv->autoexpand_row);
	gtk_tree_view_expand_row (priv->treeview, path, FALSE);
	gtk_tree_path_free (path);
	
	return TRUE;
}

static gboolean
tree_drag_motion (GtkWidget *widget, GdkDragContext *context, int x, int y, guint time, EMFolderTree *emft)
{
	struct _EMFolderTreePrivate *priv = emft->priv;
	GtkTreeModel *model = (GtkTreeModel *) priv->model;
	GtkTreeViewDropPosition pos;
	GdkDragAction action = 0;
	GtkTreePath *path;
	GtkTreeIter iter;
	GdkAtom target;
	int i;
	
	if (!gtk_tree_view_get_dest_row_at_pos(priv->treeview, x, y, &path, &pos))
		return FALSE;
	
	if (priv->autoscroll_id == 0)
		priv->autoscroll_id = g_timeout_add (150, (GSourceFunc) tree_autoscroll, emft);
	
	gtk_tree_model_get_iter (model, &iter, path);
	
	if (gtk_tree_model_iter_has_child (model, &iter) && !gtk_tree_view_row_expanded (priv->treeview, path)) {
		if (priv->autoexpand_id != 0) {
			GtkTreePath *autoexpand_path;
			
			autoexpand_path = gtk_tree_row_reference_get_path (priv->autoexpand_row);
			if (gtk_tree_path_compare (autoexpand_path, path) != 0) {
				/* row changed, restart timer */
				gtk_tree_row_reference_free (priv->autoexpand_row);
				priv->autoexpand_row = gtk_tree_row_reference_new (model, path);
				g_source_remove (priv->autoexpand_id);
				priv->autoexpand_id = g_timeout_add (600, (GSourceFunc) tree_autoexpand, emft);
			}
			
			gtk_tree_path_free (autoexpand_path);
		} else {
			priv->autoexpand_id = g_timeout_add (600, (GSourceFunc) tree_autoexpand, emft);
			priv->autoexpand_row = gtk_tree_row_reference_new (model, path);
		}
	} else if (priv->autoexpand_id != 0) {
		gtk_tree_row_reference_free (priv->autoexpand_row);
		priv->autoexpand_row = NULL;
		
		g_source_remove (priv->autoexpand_id);
		priv->autoexpand_id = 0;
	}
	
	target = emft_drop_target(emft, context, path);
	if (target != GDK_NONE) {
		for (i=0; i<NUM_DROP_TYPES; i++) {
			if (drop_atoms[i] == target) {
				switch (i) {
				case DND_DROP_TYPE_FOLDER:
					action = context->suggested_action;
					if (context->actions & GDK_ACTION_MOVE)
						action = GDK_ACTION_MOVE;
					gtk_tree_view_set_drag_dest_row(priv->treeview, path, GTK_TREE_VIEW_DROP_INTO_OR_AFTER);
					break;
				case DND_DROP_TYPE_UID_LIST:
					action = context->suggested_action;
					if (context->actions & GDK_ACTION_MOVE)
						action = GDK_ACTION_MOVE;
					gtk_tree_view_set_drag_dest_row(priv->treeview, path, GTK_TREE_VIEW_DROP_INTO_OR_AFTER);
					break;
				default:
					gtk_tree_view_set_drag_dest_row(priv->treeview, path, GTK_TREE_VIEW_DROP_INTO_OR_AFTER);
					action = context->suggested_action;
					break;
				}
				break;
			}
		}
	}

	gtk_tree_path_free(path);
	
	gdk_drag_status(context, action, time);
	
	return action != 0;
}

void
em_folder_tree_enable_drag_and_drop (EMFolderTree *emft)
{
	struct _EMFolderTreePrivate *priv;
	static int setup = 0;
	int i;
	
	g_return_if_fail (EM_IS_FOLDER_TREE (emft));
	
	priv = emft->priv;
	if (!setup) {
		for (i=0; i<NUM_DRAG_TYPES; i++)
			drag_atoms[i] = gdk_atom_intern(drag_types[i].target, FALSE);
	
		for (i=0; i<NUM_DROP_TYPES; i++)
			drop_atoms[i] = gdk_atom_intern(drop_types[i].target, FALSE);

		setup = 1;
	}

	gtk_drag_source_set((GtkWidget *)priv->treeview, GDK_BUTTON1_MASK, drag_types, NUM_DRAG_TYPES, GDK_ACTION_COPY | GDK_ACTION_MOVE);
	gtk_drag_dest_set((GtkWidget *)priv->treeview, GTK_DEST_DEFAULT_ALL, drop_types, NUM_DROP_TYPES, GDK_ACTION_COPY | GDK_ACTION_MOVE);
	
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

void em_folder_tree_set_excluded(EMFolderTree *emft, guint32 flags)
{
	emft->priv->excluded = flags;
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
			fi = m->fi->next;
	} else
		fi = m->fi;
	
	if (fi == NULL) {
		/* no children afterall... remove the "Loading..." placeholder node */
		emft_update_model_expanded_state (priv, &root, FALSE);
		gtk_tree_store_remove (model, &iter);
	} else {
		int fully_loaded = (m->flags & CAMEL_STORE_FOLDER_INFO_RECURSIVE) ? TRUE : FALSE;
		
		do {
			em_folder_tree_model_set_folder_info (priv->model, &iter, si, fi, fully_loaded);
			
			if ((fi = fi->next) != NULL)
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
	g_object_unref(m->emft);
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
	        key = g_strdup_printf ("%s%s", account->uid, path);
	} else if (CAMEL_IS_VEE_STORE (store)) {
		/* vfolder store */
		key = g_strdup_printf ("vfolder%s", path);
	} else {
		/* local store */
		key = g_strdup_printf ("local%s", path);
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
	g_object_ref(emft);
	m->top = g_strdup (top);
	m->flags = CAMEL_STORE_FOLDER_INFO_RECURSIVE;
	m->select_uri = NULL;
	
	e_thread_put (mail_thread_new, (EMsg *) m);
}

static void
emft_tree_row_collapsed (GtkTreeView *treeview, GtkTreeIter *root, GtkTreePath *tree_path, EMFolderTree *emft)
{
	gtk_tree_view_set_cursor (treeview, tree_path, NULL, FALSE);
	
	emft_update_model_expanded_state (emft->priv, root, FALSE);
	emft_queue_save_state (emft);
}

static void
emft_tree_row_activated (GtkTreeView *treeview, GtkTreePath *tree_path, GtkTreeViewColumn *column, EMFolderTree *emft)
{
	struct _EMFolderTreePrivate *priv = emft->priv;
	GtkTreeModel *model = (GtkTreeModel *) priv->model;
	GtkTreeIter iter;
	char *path, *uri;
	guint32 flags;
	
	if (!emft_select_func(NULL, model, tree_path, FALSE, emft))
		return;
	
	if (!gtk_tree_model_get_iter (model, &iter, tree_path))
		return;
	
	gtk_tree_model_get (model, &iter, COL_STRING_FOLDER_PATH, &path,
			    COL_STRING_URI, &uri, COL_UINT_FLAGS, &flags, -1);
	
	g_free (priv->selected_uri);
	priv->selected_uri = g_strdup (uri);
	
	g_free (priv->selected_path);
	priv->selected_path = g_strdup (path);
	
	g_signal_emit (emft, signals[FOLDER_SELECTED], 0, path, uri, flags);
	g_signal_emit (emft, signals[FOLDER_ACTIVATED], 0, path, uri);
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
			int deleted = 0;
			
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
				if (m->tostore == m->fromstore && m->delete) {
					camel_store_rename_folder (m->fromstore, info->full_name, toname->str, &mm->ex);
					if (camel_exception_is_set (&mm->ex))
						goto exception;
					
					/* this folder no longer exists, unsubscribe it */
					if (camel_store_supports_subscriptions (m->fromstore))
						camel_store_unsubscribe_folder (m->fromstore, info->full_name, NULL);
					
					deleted = 1;
				} else {
					if (!(fromfolder = camel_store_get_folder (m->fromstore, info->full_name, 0, &mm->ex)))
						goto exception;
					
					if (!(tofolder = camel_store_get_folder (m->tostore, toname->str, CAMEL_STORE_FOLDER_CREATE, &mm->ex))) {
						camel_object_unref (fromfolder);
						goto exception;
					}
					
					uids = camel_folder_get_uids (fromfolder);
					camel_folder_transfer_messages_to (fromfolder, uids, tofolder, NULL, m->delete, &mm->ex);
					camel_folder_free_uids (fromfolder, uids);
					
					if (m->delete)
						camel_folder_sync(fromfolder, TRUE, NULL);
					
					camel_object_unref (fromfolder);
					camel_object_unref (tofolder);
				}
			}
			
			if (camel_exception_is_set (&mm->ex))
				goto exception;
			else if (m->delete && !deleted)
				deleting = g_list_prepend (deleting, info);
			
			/* subscribe to the new folder if appropriate */
			if (camel_store_supports_subscriptions (m->tostore)
			    && !camel_store_folder_subscribed (m->tostore, toname->str))
				camel_store_subscribe_folder (m->tostore, toname->str, NULL);
			
			info = info->next;
		}
	}
	
	/* delete the folders in reverse order from how we copyied them, if we are deleting any */
	l = deleting;
	while (l) {
		CamelFolderInfo *info = l->data;
		
		d(printf ("deleting folder '%s'\n", info->full_name));
		
		/* FIXME: we need to do something with the exception
		   since otherwise the users sees a failed operation
		   with no error message or even any warnings */
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
	CamelStore *fromstore = NULL, *tostore = NULL;
	char *tobase = NULL, *frombase;
	CamelException ex;
	CamelURL *url;

	if (uri == NULL) {
		g_free (cfd);
		return;
	}
	
	priv = cfd->emft->priv;
	
	d(printf ("%sing folder '%s' to '%s'\n", cfd->delete ? "move" : "copy", priv->selected_path, uri));
	
	camel_exception_init (&ex);
	frombase = priv->selected_path + 1;

	if (!(fromstore = camel_session_get_store (session, priv->selected_uri, &ex))) {
		e_error_run((GtkWindow *)gtk_widget_get_ancestor ((GtkWidget *) cfd->emft, GTK_TYPE_WINDOW),
			    cfd->delete?"mail:no-move-folder-notexist":"mail:no-copy-folder-notexist", frombase, uri, ex.desc, NULL);
		goto fail;
	}
	
	if (cfd->delete && fromstore == mail_component_peek_local_store (NULL) && is_special_local_folder (frombase)) {
		e_error_run((GtkWindow *)gtk_widget_get_ancestor ((GtkWidget *) cfd->emft, GTK_TYPE_WINDOW),
			    "mail:no-rename-special-folder", frombase, NULL);
		goto fail;
	}
	
	if (!(tostore = camel_session_get_store (session, uri, &ex))) {
		e_error_run((GtkWindow *)gtk_widget_get_ancestor ((GtkWidget *) cfd->emft, GTK_TYPE_WINDOW),
			    cfd->delete?"mail:no-move-folder-to-notexist":"mail:no-move-folder-to-notexist", frombase, uri, ex.desc, NULL);
		goto fail;
	}
	
	url = camel_url_new (uri, NULL);
	if (((CamelService *)tostore)->provider->url_flags & CAMEL_URL_FRAGMENT_IS_PATH)
		tobase = url->fragment;
	else if (url->path && url->path[0])
		tobase = url->path+1;
	if (tobase == NULL)
		tobase = "";

	emft_copy_folders (tostore, tobase, fromstore, frombase, cfd->delete);
	
	camel_url_free (url);
fail:
	if (fromstore)
		camel_object_unref(fromstore);
	if (tostore)
		camel_object_unref(tostore);
	camel_exception_clear (&ex);
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


struct _EMCreateFolder {
	struct _mail_msg msg;
	
	/* input data */
	CamelStore *store;
	char *full_name;
	char *parent;
	char *name;
	
	/* output data */
	CamelFolderInfo *fi;
	
	/* callback data */
	void (* done) (CamelFolderInfo *fi, void *user_data);
	void *user_data;
};

static char *
emft_create_folder__desc (struct _mail_msg *mm, int done)
{
	struct _EMCreateFolder *m = (struct _EMCreateFolder *) mm;
	
	return g_strdup_printf (_("Creating folder `%s'"), m->full_name);
}

static void
emft_create_folder__create (struct _mail_msg *mm)
{
	struct _EMCreateFolder *m = (struct _EMCreateFolder *) mm;
	
	d(printf ("creating folder parent='%s' name='%s' full_name='%s'\n", m->parent, m->name, m->full_name));
	
	if ((m->fi = camel_store_create_folder (m->store, m->parent, m->name, &mm->ex))) {
		if (camel_store_supports_subscriptions (m->store))
			camel_store_subscribe_folder (m->store, m->full_name, &mm->ex);
	}
}

static void
emft_create_folder__created (struct _mail_msg *mm)
{
	struct _EMCreateFolder *m = (struct _EMCreateFolder *) mm;
	
	if (m->done)
		m->done (m->fi, m->user_data);
}

static void
emft_create_folder__free (struct _mail_msg *mm)
{
	struct _EMCreateFolder *m = (struct _EMCreateFolder *) mm;
	
	camel_store_free_folder_info (m->store, m->fi);
	camel_object_unref (m->store);
	g_free (m->full_name);
	g_free (m->parent);
	g_free (m->name);
}

static struct _mail_msg_op create_folder_op = {
	emft_create_folder__desc,
	emft_create_folder__create,
	emft_create_folder__created,
	emft_create_folder__free,
};


static int
emft_create_folder (CamelStore *store, const char *path, void (* done) (CamelFolderInfo *fi, void *user_data), void *user_data)
{
	const char *parent, *full_name;
	char *name, *namebuf = NULL;
	struct _EMCreateFolder *m;
	int id;
	
	full_name = path[0] == '/' ? path + 1 : path;
	namebuf = g_strdup (full_name);
	if (!(name = strrchr (namebuf, '/'))) {
		name = namebuf;
		parent = "";
	} else {
		*name++ = '\0';
		parent = namebuf;
	}
	
	m = mail_msg_new (&create_folder_op, NULL, sizeof (struct _EMCreateFolder));
	camel_object_ref (store);
	m->store = store;
	m->full_name = g_strdup (full_name);
	m->parent = g_strdup (parent);
	m->name = g_strdup (name);
	m->user_data = user_data;
	m->done = done;
	
	g_free (namebuf);
	
	id = m->msg.seq;
	e_thread_put (mail_thread_new, (EMsg *) m);
	
	return id;
}

static void
created_cb (CamelFolderInfo *fi, void *user_data)
{
	gboolean *created = user_data;
	
	*created = fi ? TRUE : FALSE;
}

gboolean
em_folder_tree_create_folder (EMFolderTree *emft, const char *path, const char *uri)
{
	struct _EMFolderTreePrivate *priv = emft->priv;
	struct _EMFolderTreeModelStoreInfo *si;
	gboolean created = FALSE;
	CamelStore *store;
	CamelException ex;
	
	d(printf ("Creating folder: %s (%s)\n", path, uri));
	
	camel_exception_init (&ex);
	if (!(store = (CamelStore *) camel_session_get_service (session, uri, CAMEL_PROVIDER_STORE, &ex))) {
		e_error_run((GtkWindow *)gtk_widget_get_ancestor((GtkWidget *)emft, GTK_TYPE_WINDOW),
			    "mail:no-create-folder-nostore", path, ex.desc, NULL);
		goto fail;
	}
	
	if (!(si = g_hash_table_lookup (priv->model->store_hash, store))) {
		abort();
		camel_object_unref (store);
		goto fail;
	}
	
	camel_object_unref (store);
	
	mail_msg_wait (emft_create_folder (si->store, path, created_cb, &created));
fail:
	camel_exception_clear(&ex);
	
	return created;
}

static void
new_folder_created_cb (CamelFolderInfo *fi, void *user_data)
{
	EMFolderSelector *emfs = user_data;
	
	if (fi)
		gtk_widget_destroy ((GtkWidget *) emfs);
	
	g_object_unref (emfs);
}

static void
emft_popup_new_folder_response (EMFolderSelector *emfs, int response, EMFolderTree *emft)
{
	struct _EMFolderTreePrivate *priv = emft->priv;
	struct _EMFolderTreeModelStoreInfo *si;
	const char *uri, *path;
	CamelException ex;
	CamelStore *store;
	
	if (response != GTK_RESPONSE_OK) {
		gtk_widget_destroy ((GtkWidget *) emfs);
		return;
	}
	
	uri = em_folder_selector_get_selected_uri (emfs);
	path = em_folder_selector_get_selected_path (emfs);
	
	d(printf ("Creating new folder: %s (%s)\n", path, uri));
	
	camel_exception_init (&ex);
	if (!(store = (CamelStore *) camel_session_get_service (session, uri, CAMEL_PROVIDER_STORE, &ex))) {
		camel_exception_clear (&ex);
		return;
	}
	
	if (!(si = g_hash_table_lookup (priv->model->store_hash, store))) {
		g_assert_not_reached ();
		camel_object_unref (store);
		return;
	}

	/* HACK: we need to create vfolders using the vfolder editor */
	if (CAMEL_IS_VEE_STORE(store)) {
		VfolderRule *rule;

		rule = vfolder_rule_new();
		filter_rule_set_name((FilterRule *)rule, path);
		vfolder_gui_add_rule(rule);
	} else {
		g_object_ref (emfs);
		emft_create_folder (si->store, path, new_folder_created_cb, emfs);
	}

	camel_object_unref (store);
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
		
		fi = fi->next;
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
	if (!emft_selection_get_selected (selection, &model, &iter))
		return;
	
	gtk_tree_model_get (model, &iter, COL_STRING_FOLDER_PATH, &path,
			    COL_POINTER_CAMEL_STORE, &store, -1);
	
	camel_exception_init (&ex);
	emft_popup_delete_folders (store, path, &ex);
	if (camel_exception_is_set (&ex)) {
		e_error_run(NULL, "mail:no-delete-folder", path, ex.desc, NULL);
		camel_exception_clear (&ex);
	}
}

static void
emft_popup_delete_folder (GtkWidget *item, EMFolderTree *emft)
{
	struct _EMFolderTreePrivate *priv = emft->priv;
	GtkTreeSelection *selection;
	CamelStore *local, *store;
	GtkTreeModel *model;
	GtkTreeIter iter;
	GtkWidget *dialog;
	const char *full_name;
	char *path;
	
	selection = gtk_tree_view_get_selection (priv->treeview);
	if (!emft_selection_get_selected (selection, &model, &iter))
		return;
	
	gtk_tree_model_get (model, &iter, COL_POINTER_CAMEL_STORE, &store, COL_STRING_FOLDER_PATH, &path, -1);
	
	local = mail_component_peek_local_store (NULL);
	
	full_name = path[0] == '/' ? path + 1 : path;
	if (store == local && is_special_local_folder (full_name)) {
		e_error_run(NULL, "mail:no-delete-spethal-folder", full_name, NULL);
		return;
	}

	dialog = e_error_new(NULL, "mail:ask-delete-folder", full_name, NULL);
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
	CamelStore *local, *store;
	gboolean done = FALSE;
	GtkTreeModel *model;
	GtkTreeIter iter;
	size_t base_len;
	
	local = mail_component_peek_local_store (NULL);
	
	selection = gtk_tree_view_get_selection (priv->treeview);
	if (!emft_selection_get_selected (selection, &model, &iter))
		return;
	
	gtk_tree_model_get (model, &iter, COL_STRING_FOLDER_PATH, &folder_path,
			    COL_STRING_DISPLAY_NAME, &name,
			    COL_POINTER_CAMEL_STORE, &store,
			    COL_STRING_URI, &uri, -1);
	
	full_name = folder_path[0] == '/' ? folder_path + 1 : folder_path;
	
	/* don't allow user to rename one of the special local folders */
	if (store == local && is_special_local_folder (full_name)) {
		e_error_run(NULL, "mail:no-rename-spethal-folder", full_name, NULL);
		return;
	}
	
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
				e_error_run(NULL, "mail:no-rename-folder-exists", name, new_name, NULL);
			} else {
				const char *oldpath, *newpath;
				
				oldpath = full_name;
				newpath = path;
				
				d(printf ("renaming %s to %s\n", oldpath, newpath));
				
				camel_exception_clear (&ex);
				camel_store_rename_folder (store, oldpath, newpath, &ex);
				if (camel_exception_is_set (&ex)) {
					e_error_run(NULL, "mail:no-rename-folder", oldpath, newpath, ex.desc, NULL);
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
	if (!emft_selection_get_selected (selection, &model, &iter))
		return;
	
	gtk_tree_model_get (model, &iter, COL_STRING_URI, &uri, -1);

	em_folder_properties_show(NULL, NULL, uri);
}

static EMPopupItem emft_popup_menu[] = {
#if 0
	{ EM_POPUP_ITEM, "00.emc.00", N_("_View"), G_CALLBACK (emft_popup_view), NULL, NULL, EM_POPUP_FOLDER_SELECT },
	{ EM_POPUP_ITEM, "00.emc.01", N_("Open in _New Window"), G_CALLBACK (emft_popup_open_new), NULL, NULL, EM_POPUP_FOLDER_SELECT },

	{ EM_POPUP_BAR, "10.emc" },
#endif
	{ EM_POPUP_ITEM, "10.emc.00", N_("_Copy"), G_CALLBACK (emft_popup_copy), NULL, "stock_folder-copy", EM_POPUP_FOLDER_FOLDER|EM_POPUP_FOLDER_SELECT },
	{ EM_POPUP_ITEM, "10.emc.01", N_("_Move"), G_CALLBACK (emft_popup_move), NULL, "stock_folder-move", EM_POPUP_FOLDER_FOLDER|EM_POPUP_FOLDER_DELETE },
	
	{ EM_POPUP_BAR, "20.emc" },
	/* FIXME: need to disable for nochildren folders */
	{ EM_POPUP_ITEM, "20.emc.00", N_("_New Folder..."), G_CALLBACK (emft_popup_new_folder), NULL, "stock_folder", EM_POPUP_FOLDER_INFERIORS },
	/* FIXME: need to disable for undeletable folders */
	{ EM_POPUP_ITEM, "20.emc.01", N_("_Delete"), G_CALLBACK (emft_popup_delete_folder), NULL, "stock_delete", EM_POPUP_FOLDER_FOLDER|EM_POPUP_FOLDER_DELETE },
	{ EM_POPUP_ITEM, "20.emc.01", N_("_Rename"), G_CALLBACK (emft_popup_rename_folder), NULL, NULL, EM_POPUP_FOLDER_FOLDER|EM_POPUP_FOLDER_DELETE },
	
	{ EM_POPUP_BAR, "80.emc" },
	{ EM_POPUP_ITEM, "80.emc.00", N_("_Properties..."), G_CALLBACK (emft_popup_properties), NULL, "stock_folder-properties", EM_POPUP_FOLDER_FOLDER|EM_POPUP_FOLDER_SELECT }
};

static gboolean
emft_tree_button_press (GtkTreeView *treeview, GdkEventButton *event, EMFolderTree *emft)
{
	GtkTreeSelection *selection;
	CamelStore *local, *store;
	const char *folder_name;
	EMPopupTarget *target;
	GtkTreePath *tree_path;
	GtkTreeModel *model;
	GtkTreeIter iter;
	GSList *menus = NULL;
	guint32 info_flags = 0;
	guint32 flags = 0;
	gboolean isstore;
	char *uri, *path;
	GtkMenu *menu;
	EMPopup *emp;
	int i;
	
	if (event->button != 3 && !(event->button == 1 && event->type == GDK_2BUTTON_PRESS))
		return FALSE;
	
	if (!gtk_tree_view_get_path_at_pos (treeview, (int) event->x, (int) event->y, &tree_path, NULL, NULL, NULL))
		return FALSE;
	
	/* select/focus the row that was right-clicked or double-clicked */
	gtk_tree_view_set_cursor (treeview, tree_path, NULL, FALSE);
	
	if (event->button == 1 && event->type == GDK_2BUTTON_PRESS) {
		emft_tree_row_activated (treeview, tree_path, NULL, emft);
		gtk_tree_path_free (tree_path);
		return TRUE;
	}
	
	gtk_tree_path_free (tree_path);
	
	/* FIXME: we really need the folderinfo to build a proper menu */
	selection = gtk_tree_view_get_selection (treeview);
	if (!emft_selection_get_selected (selection, &model, &iter))
		return FALSE;
	
	gtk_tree_model_get (model, &iter, COL_POINTER_CAMEL_STORE, &store,
			    COL_STRING_URI, &uri, COL_STRING_FOLDER_PATH, &path,
			    COL_BOOL_IS_STORE, &isstore, -1);
	
	if (path == NULL)
		return FALSE;
	
	if (isstore)
		flags |= EM_POPUP_FOLDER_STORE;
	else
		flags |= EM_POPUP_FOLDER_FOLDER;
	
	local = mail_component_peek_local_store (NULL);
	
	folder_name = path[0] == '/' ? path + 1 : path;
	
	/* don't allow deletion of special local folders */
	if (!(store == local && is_special_local_folder (folder_name)))
		flags |= EM_POPUP_FOLDER_DELETE;
	
	/* hack for vTrash/vJunk */
	if (!strcmp (folder_name, CAMEL_VTRASH_NAME) || !strcmp (folder_name, CAMEL_VJUNK_NAME))
		info_flags |= CAMEL_FOLDER_VIRTUAL | CAMEL_FOLDER_NOINFERIORS;
	
	/* handle right-click by opening a context menu */
	emp = em_popup_new ("com.ximian.mail.storageset.popup.select");
	
	/* FIXME: pass valid fi->flags here */
	target = em_popup_target_new_folder (uri, info_flags, flags);
	
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
	guint32 flags;
	
	if (!emft_selection_get_selected (selection, &model, &iter))
		return;
	
	gtk_tree_model_get (model, &iter, COL_STRING_FOLDER_PATH, &path,
			    COL_STRING_URI, &uri, COL_UINT_FLAGS, &flags, -1);
	
	g_free (priv->selected_uri);
	priv->selected_uri = g_strdup (uri);
	
	g_free (priv->selected_path);
	priv->selected_path = g_strdup (path);
	
	g_signal_emit (emft, signals[FOLDER_SELECTED], 0, path, uri, flags);
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
		gtk_tree_view_scroll_to_cell (priv->treeview, tree_path, NULL, FALSE, 0.0, 0.0);
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
	g_object_ref(emft);
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
