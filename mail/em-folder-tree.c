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
#include <camel/camel-stream-mem.h>

#include "e-util/e-mktemp.h"
#include "e-util/e-request.h"
#include "e-util/e-dialog-utils.h"

#include "mail-mt.h"
#include "mail-ops.h"
#include "mail-tools.h"

#include "em-utils.h"
#include "em-popup.h"
#include "em-marshal.h"
#include "em-folder-tree.h"
#include "em-folder-selector.h"
#include "em-folder-selection.h"


#define d(x) x

enum {
	COL_STRING_DISPLAY_NAME,  /* string that appears in the tree */
	COL_POINTER_CAMEL_STORE,  /* CamelStore object */
	COL_STRING_FOLDER_PATH,   /* if node is a folder, the full path of the folder */
	COL_STRING_URI,           /* the uri to get the store or
				   * folder object */
	COL_UINT_UNREAD,          /* unread count */
	
	COL_BOOL_IS_STORE,        /* toplevel store node? */
	COL_BOOL_LOAD_SUBDIRS,    /* %TRUE only if the store/folder
				   * has subfolders which have not yet
				   * been added to the tree */
	NUM_COLUMNS
};

static GType col_types[] = {
	G_TYPE_STRING,   /* display name */
	G_TYPE_POINTER,  /* store object */
	G_TYPE_STRING,   /* path */
	G_TYPE_STRING,   /* uri */
	G_TYPE_UINT,     /* unread count */
	G_TYPE_BOOLEAN,  /* is a store node */
	G_TYPE_BOOLEAN,  /* has not-yet-loaded subfolders */
};

struct _emft_store_info {
	CamelStore *store;
	GtkTreeRowReference *row;
	GHashTable *path_hash;  /* maps CamelFolderInfo::path's to GtkTreeRowReferences */
	
	char *display_name;
	
	unsigned int created_id;
	unsigned int deleted_id;
	unsigned int renamed_id;
	unsigned int subscribed_id;
	unsigned int unsubscribed_id;
};

struct _EMFolderTreePrivate {
	GtkTreeView *treeview;
	
	GHashTable *store_hash;  /* maps CamelStore's to store-info's */
	GHashTable *uri_hash;    /* maps URI's to GtkTreeRowReferences */
	
	char *selected_uri;
	char *selected_path;
	
	/* dnd signal ids */
	guint ddr, rdp, rd, ddg, ddd;
};

enum {
	FOLDER_SELECTED,
	LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = { 0 };


#define MESSAGE_RFC822_TYPE   "message/rfc822"
#define TEXT_URI_LIST_TYPE    "text/uri-list"
#define UID_LIST_TYPE         "x-uid-list"
#define FOLDER_TYPE           "x-folder"

/* Drag & Drop types */
enum DndDragType {
	DND_DRAG_TYPE_FOLDER,          /* drag an evo folder */
	DND_DRAG_TYPE_TEXT_URI_LIST,   /* drag to an mbox file */
};

enum DndDropType {
	DND_DROP_TYPE_UID_LIST,        /* drop a list of message uids */
	DND_DROP_TYPE_FOLDER,          /* drop an evo folder */
	DND_DROP_TYPE_MESSAGE_RFC822,  /* drop a message/rfc822 stream */
	DND_DROP_TYPE_TEXT_URI_LIST,   /* drop an mbox file */
};

static GtkTargetEntry drag_types[] = {
	{ UID_LIST_TYPE,       0, DND_DRAG_TYPE_FOLDER         },
	{ TEXT_URI_LIST_TYPE,  0, DND_DRAG_TYPE_TEXT_URI_LIST  },
};

static const int num_drag_types = sizeof (drag_types) / sizeof (drag_types[0]);

static GtkTargetEntry drop_types[] = {
	{ UID_LIST_TYPE,       0, DND_DROP_TYPE_UID_LIST       },
	{ FOLDER_TYPE,         0, DND_DROP_TYPE_FOLDER         },
	{ MESSAGE_RFC822_TYPE, 0, DND_DROP_TYPE_MESSAGE_RFC822 },
	{ TEXT_URI_LIST_TYPE,  0, DND_DROP_TYPE_TEXT_URI_LIST  },
};

static const int num_drop_types = sizeof (drop_types) / sizeof (drop_types[0]);


extern CamelSession *session;


static void em_folder_tree_class_init (EMFolderTreeClass *klass);
static void em_folder_tree_init (EMFolderTree *emft);
static void em_folder_tree_destroy (GtkObject *obj);
static void em_folder_tree_finalize (GObject *obj);

static void tree_row_expanded (GtkTreeView *treeview, GtkTreeIter *root, GtkTreePath *path, EMFolderTree *emft);
static gboolean tree_button_press (GtkWidget *treeview, GdkEventButton *event, EMFolderTree *emft);
static void tree_selection_changed (GtkTreeSelection *selection, EMFolderTree *emft);


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
	FOLDER_ICON_TRASH
};

static GdkPixbuf *folder_icons[4];

static void
render_pixbuf (GtkTreeViewColumn *column, GtkCellRenderer *renderer,
	       GtkTreeModel *model, GtkTreeIter *iter, gpointer user_data)
{
	static gboolean initialised = FALSE;
	GdkPixbuf *pixbuf = NULL;
	gboolean is_store;
	char *path;
	
	if (!initialised) {
		folder_icons[0] = gdk_pixbuf_new_from_file (EVOLUTION_ICONSDIR "/folder-mini.png", NULL);
		folder_icons[1] = gdk_pixbuf_new_from_file (EVOLUTION_ICONSDIR "/inbox-mini.png", NULL);
		folder_icons[2] = gdk_pixbuf_new_from_file (EVOLUTION_ICONSDIR "/outbox-mini.png", NULL);
		folder_icons[3] = gdk_pixbuf_new_from_file (EVOLUTION_ICONSDIR "/evolution-trash-mini.png", NULL);
		initialised = TRUE;
	}
	
	gtk_tree_model_get (model, iter, COL_STRING_FOLDER_PATH, &path,
			    COL_BOOL_IS_STORE, &is_store, -1);
	
	if (!is_store && path != NULL) {
		if (!strcasecmp (path, "/Inbox"))
			pixbuf = folder_icons[FOLDER_ICON_INBOX];
		else if (!strcasecmp (path, "/Outbox"))
			pixbuf = folder_icons[FOLDER_ICON_OUTBOX];
		else if (!strcasecmp (path, "/Trash"))
			pixbuf = folder_icons[FOLDER_ICON_TRASH];
		else
			pixbuf = folder_icons[FOLDER_ICON_NORMAL];
	}
	
	g_object_set (renderer, "pixbuf", pixbuf, NULL);
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
		      "foreground_set", unread ? TRUE : FALSE,
		      "foreground", unread ? "#0000ff" : "#000000", NULL);
	
	g_free (display);
}

static void
em_folder_tree_init (EMFolderTree *emft)
{
	struct _EMFolderTreePrivate *priv;
	
	priv = g_new0 (struct _EMFolderTreePrivate, 1);
	priv->store_hash = g_hash_table_new (g_direct_hash, g_direct_equal);
	priv->uri_hash = g_hash_table_new (g_str_hash, g_str_equal);
	priv->selected_uri = NULL;
	priv->selected_path = NULL;
	priv->treeview = NULL;
	emft->priv = priv;
}

static void
path_hash_free (gpointer key, gpointer value, gpointer user_data)
{
	g_free (key);
	gtk_tree_row_reference_free (value);
}

static void
store_info_free (struct _emft_store_info *si)
{
	camel_object_remove_event (si->store, si->created_id);
	camel_object_remove_event (si->store, si->deleted_id);
	camel_object_remove_event (si->store, si->renamed_id);
	camel_object_remove_event (si->store, si->subscribed_id);
	camel_object_remove_event (si->store, si->unsubscribed_id);
	
	g_free (si->display_name);
	camel_object_unref (si->store);
	gtk_tree_row_reference_free (si->row);
	g_hash_table_foreach (si->path_hash, path_hash_free, NULL);
	g_free (si);
}

static void
store_hash_free (gpointer key, gpointer value, gpointer user_data)
{
	struct _emft_store_info *si = value;
	
	store_info_free (si);
}

static void
uri_hash_free (gpointer key, gpointer value, gpointer user_data)
{
	g_free (key);
	gtk_tree_row_reference_free (value);
}

static void
em_folder_tree_finalize (GObject *obj)
{
	EMFolderTree *emft = (EMFolderTree *) obj;
	
	g_hash_table_foreach (emft->priv->store_hash, store_hash_free, NULL);
	g_hash_table_destroy (emft->priv->store_hash);
	
	g_hash_table_foreach (emft->priv->uri_hash, uri_hash_free, NULL);
	g_hash_table_destroy (emft->priv->uri_hash);
	
	g_free (emft->priv->selected_uri);
	g_free (emft->priv->selected_path);
	g_free (emft->priv);
	
	G_OBJECT_CLASS (parent_class)->finalize (obj);
}

static void
em_folder_tree_destroy (GtkObject *obj)
{
	struct _EMFolderTreePrivate *priv = ((EMFolderTree *) obj)->priv;
	
	if (priv->ddr != 0) {
		g_signal_handler_disconnect (obj, priv->ddr);
		priv->ddr = 0;
	}
	
	if (priv->rdp != 0) {
		g_signal_handler_disconnect (obj, priv->rdp);
		priv->rdp = 0;
	}
	
	if (priv->rd != 0) {
		g_signal_handler_disconnect (obj, priv->rd);
		priv->rd = 0;
	}
	
	if (priv->ddg != 0) {
		g_signal_handler_disconnect (obj, priv->ddg);
		priv->ddg = 0;
	}
	
	if (priv->ddd != 0) {
		g_signal_handler_disconnect (obj, priv->ddd);
		priv->ddd = 0;
	}
	
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
	
	column = gtk_tree_view_column_new ();
	gtk_tree_view_append_column ((GtkTreeView *) tree, column);
	
	renderer = gtk_cell_renderer_pixbuf_new ();
	gtk_tree_view_column_pack_start (column, renderer, FALSE);
	gtk_tree_view_column_set_cell_data_func (column, renderer, render_pixbuf, NULL, NULL);
	
	renderer = gtk_cell_renderer_text_new ();
	gtk_tree_view_column_pack_start (column, renderer, TRUE);
	gtk_tree_view_column_set_cell_data_func (column, renderer, render_display_name, NULL, NULL);
	/*gtk_tree_view_insert_column_with_attributes ((GtkTreeView *) tree, -1, "",
	  renderer, "text", 0, NULL);*/
	
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
	
	priv->treeview = folder_tree_new (model);
	gtk_widget_show ((GtkWidget *) priv->treeview);
	
	g_signal_connect (priv->treeview, "row-expanded", G_CALLBACK (tree_row_expanded), emft);
	g_signal_connect (priv->treeview, "button-press-event", G_CALLBACK (tree_button_press), emft);
	
	selection = gtk_tree_view_get_selection ((GtkTreeView *) priv->treeview);
	g_signal_connect (selection, "changed", G_CALLBACK (tree_selection_changed), emft);
	
	gtk_container_add ((GtkContainer *) scrolled, (GtkWidget *) priv->treeview);
	gtk_widget_show (scrolled);
	
	gtk_box_pack_start ((GtkBox *) emft, scrolled, TRUE, TRUE, 0);
}



static void
drop_uid_list (EMFolderTree *emft, CamelFolder *dest, gboolean move, GtkSelectionData *selection, CamelException *ex)
{
	CamelFolder *src;
	GPtrArray *uids;
	char *src_uri;
	
	em_utils_selection_get_uidlist (selection, &src_uri, &uids);
	
	if (!(src = mail_tool_uri_to_folder (src_uri, 0, ex))) {
		em_utils_uids_free (uids);
		g_free (src_uri);
		return;
	}
	
	g_free (src_uri);
	
	camel_folder_transfer_messages_to (src, uids, dest, NULL, move, ex);
	em_utils_uids_free (uids);
	camel_object_unref (src);
}

static void
drop_folder (EMFolderTree *emft, CamelFolder *dest, gboolean move, GtkSelectionData *selection, CamelException *ex)
{
	CamelFolder *src;
	
	/* get the folder being dragged */
	if (!(src = mail_tool_uri_to_folder (selection->data, 0, ex)))
		return;
	
	if (src->parent_store == dest->parent_store && move) {
		/* simple rename() action */
		char *old_name, *new_name;
		
		old_name = g_strdup (src->full_name);
		new_name = g_strdup_printf ("%s/%s", dest->full_name, src->name);
		
		camel_store_rename_folder (dest->parent_store, old_name, new_name, ex);
		
		g_free (old_name);
		g_free (new_name);
	} else {
		/* copy the folder to the new location */
		CamelFolder *folder;
		char *path;
		
		path = g_strdup_printf ("%s/%s", dest->full_name, src->name);
		if ((folder = camel_store_get_folder (dest->parent_store, path, CAMEL_STORE_FOLDER_CREATE, ex))) {
			GPtrArray *uids;
			
			uids = camel_folder_get_uids (src);
			camel_folder_transfer_messages_to (src, uids, folder, NULL, FALSE, ex);
			camel_folder_free_uids (src, uids);
			
			camel_object_unref (folder);
		}
		
		g_free (path);
	}
	
	camel_object_unref (src);
}

static gboolean
import_message_rfc822 (CamelFolder *dest, CamelStream *stream, gboolean scan_from, CamelException *ex)
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
drop_message_rfc822 (EMFolderTree *emft, CamelFolder *dest, GtkSelectionData *selection, CamelException *ex)
{
	CamelStream *stream;
	gboolean scan_from;
	
	scan_from = selection->length > 5 && !strncmp (selection->data, "From ", 5);
	stream = camel_stream_mem_new_with_buffer (selection->data, selection->length);
	
	import_message_rfc822 (dest, stream, scan_from, ex);
	
	camel_object_unref (stream);
}

static void
drop_text_uri_list (EMFolderTree *emft, CamelFolder *dest, GtkSelectionData *selection, CamelException *ex)
{
	char **urls, *url, *tmp;
	CamelStream *stream;
	CamelURL *uri;
	int fd, i;
	
	tmp = g_strndup (selection->data, selection->length);
	urls = g_strsplit (tmp, "\n", 0);
	g_free (tmp);
	
	for (i = 0; urls[i] != NULL; i++) {
		/* get the path component */
		url = g_strstrip (urls[i]);
		uri = camel_url_new (url, NULL);
		g_free (url);
		
		if (!uri || strcmp (uri->protocol, "file") != 0) {
			camel_url_free (uri);
			continue;
		}
		
		url = uri->path;
		uri->path = NULL;
		camel_url_free (uri);
		
		if ((fd = open (url, O_RDONLY)) == -1) {
			g_free (url);
			continue;
		}
		
		stream = camel_stream_fs_new_with_fd (fd);
		if (!import_message_rfc822 (dest, stream, TRUE, ex)) {
			/* FIXME: should we abort now? or continue? */
			/* for now lets just continue... */
			camel_exception_clear (ex);
		}
		
		camel_object_unref (stream);
		g_free (url);
	}
	
	g_free (urls);
}

static gboolean
drag_data_received_cb (EMFolderTreeModel *model, GtkTreePath *dest_path, GtkSelectionData *selection, EMFolderTree *emft)
{
	CamelFolder *folder;
	CamelStore *store;
	CamelException ex;
	GtkTreeIter iter;
	char *path;
	
	/* this means we are receiving no data */
	if (!selection->data || selection->length == -1)
		return FALSE;
	
	gtk_tree_model_get_iter ((GtkTreeModel *) model, &iter, dest_path);
	gtk_tree_model_get ((GtkTreeModel *) model, &iter,
			    COL_POINTER_CAMEL_STORE, &store,
			    COL_STRING_FOLDER_PATH, &path, -1);
	
	camel_exception_init (&ex);
	if ((folder = camel_store_get_folder (store, path, 0, &ex))) {
		/* FIXME: would have been nicer if we could 'move'
		 * messages and/or folders. but alas, gtktreeview
		 * drag&drop sucks ass and doesn't give us the
		 * context->action to check for GDK_ACTION_MOVE, so we
		 * can't. Yay. */
		gboolean move = FALSE;
		
		if (selection->target == gdk_atom_intern ("x-uid-list", FALSE)) {
			/* import a list of uids from another evo folder */
			drop_uid_list (emft, folder, move, selection, &ex);
			d(printf ("* dropped a x-uid-list\n"));
		} else if (selection->target == gdk_atom_intern ("x-folder", FALSE)) {
			/* copy or move (aka rename) a folder */
			drop_folder (emft, folder, move, selection, &ex);
			d(printf ("* dropped a x-folder\n"));
		} else if (selection->target == gdk_atom_intern ("message/rfc822", FALSE)) {
			/* import a message/rfc822 stream */
			drop_message_rfc822 (emft, folder, selection, &ex);
			d(printf ("* dropped a message/rfc822\n"));
		} else if (selection->target == gdk_atom_intern ("text/uri-list", FALSE)) {
			/* import an mbox, maildir, or mh folder? */
			drop_text_uri_list (emft, folder, selection, &ex);
			d(printf ("* dropped a text/uri-list\n"));
		} else {
			g_assert_not_reached ();
		}
	}
	
	if (camel_exception_is_set (&ex)) {
		/* FIXME: error dialog? */
		camel_exception_clear (&ex);
		return FALSE;
	}
	
	return TRUE;
}

static gboolean
row_drop_possible_cb (EMFolderTreeModel *model, GtkTreePath *dest_path, GtkSelectionData *selection, EMFolderTree *emft)
{
	GtkTreeIter iter;
	gboolean is_store;
	
	gtk_tree_model_get_iter ((GtkTreeModel *) model, &iter, dest_path);
	gtk_tree_model_get ((GtkTreeModel *) model, &iter, COL_BOOL_IS_STORE, &is_store, -1);
	
	if (selection->target == gdk_atom_intern ("x-uid-list", FALSE)) {
		if (is_store)
			return FALSE;
		
		return TRUE;
	} else if (selection->target == gdk_atom_intern ("x-folder", FALSE)) {
		return TRUE;
	} else if (selection->target == gdk_atom_intern ("message/rfc822", FALSE)) {
		if (is_store)
			return FALSE;
		
		return TRUE;
	} else if (selection->target == gdk_atom_intern ("text/uri-list", FALSE)) {
		if (is_store)
			return FALSE;
		
		return TRUE;
	} else {
		g_assert_not_reached ();
		return FALSE;
	}
}

static gboolean
row_draggable_cb (EMFolderTreeModel *model, GtkTreePath *src_path, EMFolderTree *emft)
{
	GtkTreeIter iter;
	gboolean is_store;
	
	gtk_tree_model_get_iter ((GtkTreeModel *) model, &iter, src_path);
	gtk_tree_model_get ((GtkTreeModel *) model, &iter, COL_BOOL_IS_STORE, &is_store, -1);
	
	return !is_store;
}

static void
drag_text_uri_list (EMFolderTree *emft, CamelFolder *src, GtkSelectionData *selection, CamelException *ex)
{
	CamelFolder *dest;
	const char *tmpdir;
	CamelStore *store;
	GPtrArray *uids;
	char *url;
	
	if (!(tmpdir = e_mkdtemp ("drag-n-drop-XXXXXX"))) {
		camel_exception_setv (ex, CAMEL_EXCEPTION_SYSTEM,
				      _("Could not create temporary directory: %s"),
				      g_strerror (errno));
		return;
	}
	
	url = g_strdup_printf ("mbox:%s", tmpdir);
	if (!(store = camel_session_get_store (session, url, ex))) {
		camel_exception_setv (ex, CAMEL_EXCEPTION_SYSTEM,
				      _("Could not create temporary mbox store: %s"),
				      camel_exception_get_description (ex));
		g_free (url);
		
		return;
	}
	
	if (!(dest = camel_store_get_folder (store, "mbox", CAMEL_STORE_FOLDER_CREATE, ex))) {
		camel_exception_setv (ex, CAMEL_EXCEPTION_SYSTEM,
				      _("Could not create temporary mbox folder: %s"),
				      camel_exception_get_description (ex));
		
		camel_object_unref (store);
		g_free (url);
		
		return;
	}
	
	camel_object_unref (store);
	uids = camel_folder_get_uids (src);
	
	camel_folder_transfer_messages_to (src, uids, dest, NULL, FALSE, ex);
	if (camel_exception_is_set (ex)) {
		camel_exception_setv (ex, CAMEL_EXCEPTION_SYSTEM,
				      _("Could not copy messages to temporary mbox folder: %s"),
				      camel_exception_get_description (ex));
	} else {
		memcpy (url, "file", 4);
		gtk_selection_data_set (selection, selection->target, 8, url, strlen (url));
	}
	
	camel_folder_free_uids (src, uids);
	camel_object_unref (dest);
	g_free (url);
}

static gboolean
drag_data_get_cb (EMFolderTreeModel *model, GtkTreePath *src_path, GtkSelectionData *selection, EMFolderTree *emft)
{
	CamelFolder *folder;
	CamelStore *store;
	CamelException ex;
	GtkTreeIter iter;
	char *path, *uri;
	
	gtk_tree_model_get_iter ((GtkTreeModel *) model, &iter, src_path);
	gtk_tree_model_get ((GtkTreeModel *) model, &iter,
			    COL_POINTER_CAMEL_STORE, &store,
			    COL_STRING_FOLDER_PATH, &path,
			    COL_STRING_URI, &uri, -1);
	
	camel_exception_init (&ex);
	
	if (selection->target == gdk_atom_intern ("x-folder", FALSE)) {
		/* dragging to a new location in the folder tree */
		gtk_selection_data_set (selection, selection->target, 8, uri, strlen (uri) + 1);
	} else if (selection->target == gdk_atom_intern ("text/uri-list", FALSE)) {
		/* dragging to nautilus or something, probably */
		if ((folder = camel_store_get_folder (store, path, 0, &ex))) {
			drag_text_uri_list (emft, folder, selection, &ex);
			camel_object_unref (folder);
		}
	} else {
		g_assert_not_reached ();
	}
	
	if (camel_exception_is_set (&ex)) {
		/* FIXME: error dialog? */
		camel_exception_clear (&ex);
		return FALSE;
	}
	
	return TRUE;
}

static gboolean
drag_data_delete_cb (EMFolderTreeModel *model, GtkTreePath *src_path, EMFolderTree *emft)
{
	gboolean is_store;
	CamelStore *store;
	CamelException ex;
	GtkTreeIter iter;
	char *path;
	
	gtk_tree_model_get_iter ((GtkTreeModel *) model, &iter, src_path);
	gtk_tree_model_get ((GtkTreeModel *) model, &iter,
			    COL_POINTER_CAMEL_STORE, &store,
			    COL_STRING_FOLDER_PATH, &path,
			    COL_BOOL_IS_STORE, &is_store, -1);
	
	if (is_store)
		return FALSE;
	
	camel_exception_init (&ex);
	camel_store_delete_folder (store, path, &ex);
	if (camel_exception_is_set (&ex)) {
		/* FIXME: error dialog? */
		camel_exception_clear (&ex);
		return FALSE;
	}
	
	return TRUE;
}


GtkWidget *
em_folder_tree_new (void)
{
	struct _EMFolderTreePrivate *priv;
	EMFolderTreeModel *model;
	EMFolderTree *emft;
	
	model = em_folder_tree_model_new (NUM_COLUMNS, col_types);
	emft = (EMFolderTree *) em_folder_tree_new_with_model (model);
	
	priv = emft->priv;
	priv->ddr = g_signal_connect (model, "drag-data-received", G_CALLBACK (drag_data_received_cb), emft);
	priv->rdp = g_signal_connect (model, "row-drop-possible", G_CALLBACK (row_drop_possible_cb), emft);
	priv->rd = g_signal_connect (model, "row-draggable", G_CALLBACK (row_draggable_cb), emft);
	priv->ddg = g_signal_connect (model, "drag-data-get", G_CALLBACK (drag_data_get_cb), emft);
	priv->ddd = g_signal_connect (model, "drag-data-delete", G_CALLBACK (drag_data_delete_cb), emft);
	
	gtk_drag_source_set ((GtkWidget *) emft, 0, drag_types, num_drag_types, GDK_ACTION_COPY | GDK_ACTION_MOVE);
	gtk_drag_dest_set ((GtkWidget *) emft, GTK_DEST_DEFAULT_ALL, drop_types, num_drop_types, GDK_ACTION_COPY | GDK_ACTION_MOVE);
	
	return (GtkWidget *) emft;
}


GtkWidget *
em_folder_tree_new_with_model (EMFolderTreeModel *model)
{
	EMFolderTree *emft;
	
	emft = g_object_new (EM_TYPE_FOLDER_TREE, NULL);
	em_folder_tree_construct (emft, model);
	
	return (GtkWidget *) emft;
}


static void
tree_store_set_folder_info (GtkTreeStore *model, GtkTreeIter *iter,
			    struct _EMFolderTreePrivate *priv,
			    struct _emft_store_info *si,
			    CamelFolderInfo *fi)
{
	GtkTreeRowReference *uri_row, *path_row;
	unsigned int unread;
	GtkTreePath *path;
	GtkTreeIter sub;
	gboolean load;
	
	load = (fi->flags & CAMEL_FOLDER_CHILDREN) && !(fi->flags & CAMEL_FOLDER_NOINFERIORS);
	
	path = gtk_tree_model_get_path ((GtkTreeModel *) model, iter);
	uri_row = gtk_tree_row_reference_new ((GtkTreeModel *) model, path);
	path_row = gtk_tree_row_reference_copy (uri_row);
	gtk_tree_path_free (path);
	
	g_hash_table_insert (priv->uri_hash, g_strdup (fi->url), uri_row);
	g_hash_table_insert (si->path_hash, g_strdup (fi->path), path_row);
	
	unread = fi->unread_message_count == -1 ? 0 : fi->unread_message_count;
	
	gtk_tree_store_set (model, iter,
			    COL_STRING_DISPLAY_NAME, fi->name,
			    COL_POINTER_CAMEL_STORE, si->store,
			    COL_STRING_FOLDER_PATH, fi->path,
			    COL_STRING_URI, fi->url,
			    COL_UINT_UNREAD, unread,
			    COL_BOOL_IS_STORE, FALSE,
			    COL_BOOL_LOAD_SUBDIRS, load,
			    -1);
	
	if (load) {
		/* create a placeholder node for our subfolders... */
		gtk_tree_store_append (model, &sub, iter);
		gtk_tree_store_set (model, &sub,
				    COL_STRING_DISPLAY_NAME, _("Loading..."),
				    COL_POINTER_CAMEL_STORE, si->store,
				    COL_STRING_FOLDER_PATH, fi->path,
				    COL_BOOL_LOAD_SUBDIRS, TRUE,
				    COL_BOOL_IS_STORE, FALSE,
				    COL_STRING_URI, fi->url,
				    COL_UINT_UNREAD, 0,
				    -1);
	}
}

static void
tree_row_expanded (GtkTreeView *treeview, GtkTreeIter *root, GtkTreePath *tree_path, EMFolderTree *emft)
{
	/* FIXME: might be best to call get_folder_info in another thread and add the nodes to the treeview in the callback? */
	struct _EMFolderTreePrivate *priv = emft->priv;
	struct _emft_store_info *si;
	CamelFolderInfo *fi, *child;
	CamelStore *store;
	CamelException ex;
	GtkTreeStore *model;
	GtkTreeIter iter;
	gboolean load;
	char *path;
	char *top;
	
	model = (GtkTreeStore *) gtk_tree_view_get_model (treeview);
	
	gtk_tree_model_get ((GtkTreeModel *) model, root,
			    COL_STRING_FOLDER_PATH, &path,
			    COL_POINTER_CAMEL_STORE, &store,
			    COL_BOOL_LOAD_SUBDIRS, &load,
			    -1);
	if (!load)
		return;
	
	if (!(si = g_hash_table_lookup (emft->priv->store_hash, store))) {
		g_assert_not_reached ();
		return;
	}
	
	/* get the first child (which will be a dummy if we haven't loaded the child folders yet) */
	gtk_tree_model_iter_children ((GtkTreeModel *) model, &iter, root);
	
	/* FIXME: this sucks ass, need to fix camel so that using path as @top will Just Work (tm) */
	/* NOTE: CamelImapStore will handle "" as toplevel, but CamelMboxStore wants NULL */
	if (!path || !strcmp (path, "/"))
		top = NULL;
	else
		top = path + 1;
	
	/* FIXME: are there any flags we want to pass when getting folder-info's? */
	camel_exception_init (&ex);
	if (!(fi = camel_store_get_folder_info (store, top, 0, &ex))) {
		/* FIXME: report error to user? or simply re-collapse node? or both? */
		g_warning ("can't get folder-info's for store '%s' at path='%s'", si->display_name, path);
		gtk_tree_store_remove (model, &iter);
		camel_exception_clear (&ex);
		return;
	}
	
	/* FIXME: camel is totally on crack here, @top's folder info
	 * should be @fi and fi->childs should be what we want to fill
	 * our tree with... *sigh* */
	if (!strcmp (fi->path, path))
		child = fi->sibling;
	else
		child = fi;
	
	if (child == NULL) {
		/* no children afterall... remove the "Loading..." placeholder node */
		gtk_tree_store_remove (model, &iter);
	} else {
		do {
			tree_store_set_folder_info (model, &iter, priv, si, child);
			
			if ((child = child->sibling) != NULL)
				gtk_tree_store_append (model, &iter, root);
		} while (child != NULL);
	}
	
	gtk_tree_store_set (model, root, COL_BOOL_LOAD_SUBDIRS, FALSE, -1);
	
	camel_store_free_folder_info (store, fi);
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

/* FIXME: This must be done in another thread */
static void
em_copy_folders (CamelStore *tostore, const char *tobase, CamelStore *fromstore, const char *frombase, int delete)
{
	GString *toname, *fromname;
	CamelFolderInfo *fi;
	GList *pending = NULL, *deleting = NULL, *l;
	guint32 flags = CAMEL_STORE_FOLDER_INFO_RECURSIVE;
	CamelException ex;
	int fromlen;
	const char *tmp;
	
	camel_exception_init (&ex);
	
	if (camel_store_supports_subscriptions (fromstore))
		flags |= CAMEL_STORE_FOLDER_INFO_SUBSCRIBED;
	
	fi = camel_store_get_folder_info (fromstore, frombase, flags, &ex);
	if (camel_exception_is_set (&ex))
		goto done;
	
	pending = g_list_append (pending, fi);
	
	toname = g_string_new ("");
	fromname = g_string_new ("");
	
	tmp = strrchr (frombase, '/');
	if (tmp == NULL)
		fromlen = 0;
	else
		fromlen = tmp - frombase + 1;
	
	d(printf ("top name is '%s'\n", fi->full_name));
	
	while (pending) {
		CamelFolderInfo *info = pending->data;
		
		pending = g_list_remove_link (pending, pending);
		while (info) {
			CamelFolder *fromfolder, *tofolder;
			GPtrArray *uids;
			
			if (info->child)
				pending = g_list_append (pending, info->child);
			if (tobase[0])
				g_string_printf (toname, "%s/%s", tobase, info->full_name + fromlen);
			else
				g_string_printf (toname, "%s", info->full_name + fromlen);
			
			d(printf ("Copying from '%s' to '%s'\n", info->full_name, toname->str));
			
			/* This makes sure we create the same tree, e.g. from a nonselectable source */
			/* Not sure if this is really the 'right thing', e.g. for spool stores, but it makes the ui work */
			if ((info->flags & CAMEL_FOLDER_NOSELECT) == 0) {
				d(printf ("this folder is selectable\n"));
				fromfolder = camel_store_get_folder (fromstore, info->full_name, 0, &ex);
				if (fromfolder == NULL)
					goto exception;
				
				tofolder = camel_store_get_folder (tostore, toname->str, CAMEL_STORE_FOLDER_CREATE, &ex);
				if (tofolder == NULL) {
					camel_object_unref (fromfolder);
					goto exception;
				}
				
				if (camel_store_supports_subscriptions (tostore)
				    && !camel_store_folder_subscribed (tostore, toname->str))
					camel_store_subscribe_folder (tostore, toname->str, NULL);
				
				uids = camel_folder_get_uids (fromfolder);
				camel_folder_transfer_messages_to (fromfolder, uids, tofolder, NULL, delete, &ex);
				camel_folder_free_uids (fromfolder, uids);
				
				camel_object_unref (fromfolder);
				camel_object_unref (tofolder);
			}
			
			if (camel_exception_is_set (&ex))
				goto exception;
			else if (delete)
				deleting = g_list_prepend (deleting, info);
			
			info = info->sibling;
		}
	}
	
	/* delete the folders in reverse order from how we copyied them, if we are deleting any */
	l = deleting;
	while (l) {
		CamelFolderInfo *info = l->data;
		
		d(printf ("deleting folder '%s'\n", info->full_name));
		
		if (camel_store_supports_subscriptions (fromstore))
			camel_store_unsubscribe_folder (fromstore, info->full_name, NULL);
		
		camel_store_delete_folder (fromstore, info->full_name, NULL);
		l = l->next;
	}
	
 exception:
	
	camel_store_free_folder_info (fromstore, fi);
	g_list_free (deleting);
	
	g_string_free (toname, TRUE);
	g_string_free (fromname, TRUE);
	
 done:
	
	d(printf ("exception: %s\n", ex.desc ? ex.desc : "<none>"));
	camel_exception_clear (&ex);
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
	CamelURL *url;
	
	if (uri == NULL) {
		g_free (cfd);
		return;
	}
	
	priv = cfd->emft->priv;
	
	d(printf ("copying folder '%s' to '%s'\n", priv->selected_path, uri));
	
	camel_exception_init (&ex);
	if (!(fromstore = camel_session_get_store (session, priv->selected_uri, &ex))) {
		/* FIXME: error dialog? */
		camel_exception_clear (&ex);
		return;
	}
	
	frombase = priv->selected_path + 1;
	
	if (!(tostore = camel_session_get_store (session, uri, &ex))) {
		/* FIXME: error dialog? */
		camel_object_unref (fromstore);
		camel_exception_clear (&ex);
		return;
	}
	
	url = camel_url_new (uri, NULL);
	if (url->fragment)
		tobase = url->fragment;
	else if (url->path && url->path[0])
		tobase = url->path + 1;
	else
		tobase = "";
	
	em_copy_folders (tostore, tobase, fromstore, frombase, cfd->delete);
	
	camel_url_free (url);
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

static void
emft_popup_new_folder_response (EMFolderSelector *emfs, int response, EMFolderTree *emft)
{
	/* FIXME: ugh, kludge-a-licious: EMFolderSelector uses EMFolderTree so we can poke emfs->emft internals */
	struct _EMFolderTreePrivate *priv = emfs->emft->priv;
	struct _emft_store_info *si;
	const char *uri, *parent;
	CamelException ex;
	char *path, *name;
	
	if (response != GTK_RESPONSE_OK) {
		gtk_widget_destroy ((GtkWidget *) emfs);
		return;
	}
	
	uri = em_folder_selector_get_selected_uri (emfs);
	path = (char *) em_folder_selector_get_selected_path (emfs);
	d(printf ("Creating folder: %s (%s)\n", path, uri));
	
	if (!(si = g_hash_table_lookup (priv->store_hash, uri)))
		goto done;
	
	/* FIXME: camel_store_create_folder should just take full path names */
	path = g_strdup (path);
	if (!(name = strrchr (path, '/'))) {
		name = path;
		parent = "";
	} else {
		*name++ = '\0';
		parent = path;
	}
	
	d(printf ("creating folder name='%s' path='%s'\n", name, path));
	
	camel_exception_init (&ex);
	camel_store_create_folder (si->store, parent, name, &ex);
	if (camel_exception_is_set (&ex)) {
		d(printf ("Create failed: %s\n", ex.desc));
		/* FIXME: error dialog? */
	} else if (camel_store_supports_subscriptions (si->store)) {
		camel_store_subscribe_folder (si->store, path, &ex);
		if (camel_exception_is_set (&ex)) {
			d(printf ("Subscribe failed: %s\n", ex.desc));
			/* FIXME: error dialog? */
		}
	}
	
	camel_exception_clear (&ex);
	
 done:
	
	g_free (path);
	
	gtk_widget_destroy ((GtkWidget *) emfs);
}

static void
store_hash_add_store (gpointer key, gpointer value, gpointer user_data)
{
	struct _emft_store_info *si = value;
	EMFolderTree *emft = user_data;
	
	em_folder_tree_add_store (emft, si->store, si->display_name);
}

static void
emft_popup_new_folder (GtkWidget *item, EMFolderTree *emft)
{
	EMFolderTree *folder_tree;
	GtkWidget *dialog;
	
	folder_tree = (EMFolderTree *) em_folder_tree_new ();
	g_hash_table_foreach (emft->priv->store_hash, store_hash_add_store, folder_tree);
	
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
		
		folder = camel_store_get_folder (store, fi->full_name, 0, NULL);
		if (folder) {
			GPtrArray *uids = camel_folder_get_uids (folder);
			int i;
			
			camel_folder_freeze (folder);
			for (i = 0; i < uids->len; i++)
				camel_folder_delete_message (folder, uids->pdata[i]);
			camel_folder_sync (folder, TRUE, NULL);
			camel_folder_thaw (folder);
			camel_folder_free_uids (folder, uids);
		}
		
		camel_store_delete_folder (store, fi->full_name, ex);
		if (camel_exception_is_set (ex))
			return;
		
		fi = fi->sibling;
	}
}

static void
emft_popup_delete_folders (CamelStore *store, const char *base, CamelException *ex)
{
	guint32 flags = CAMEL_STORE_FOLDER_INFO_RECURSIVE;
	CamelFolderInfo *fi;
	
	if (camel_store_supports_subscriptions (store))
		flags |= CAMEL_STORE_FOLDER_INFO_SUBSCRIBED;
	
	fi = camel_store_get_folder_info (store, base, flags, ex);
	if (camel_exception_is_set (ex))
		return;
	
	emft_popup_delete_rec (store, fi, ex);
	camel_store_free_folder_info (store, fi);
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
	gtk_tree_selection_get_selected (selection, &model, &iter);
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
	gtk_tree_selection_get_selected (selection, &model, &iter);
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
	gboolean done = FALSE;
	GtkTreeModel *model;
	GtkTreeIter iter;
	CamelStore *store;
	
	selection = gtk_tree_view_get_selection (priv->treeview);
	gtk_tree_selection_get_selected (selection, &model, &iter);
	gtk_tree_model_get (model, &iter, COL_STRING_FOLDER_PATH, &folder_path,
			    COL_STRING_DISPLAY_NAME, &name,
			    COL_POINTER_CAMEL_STORE, &store,
			    COL_STRING_URI, &uri, -1);
	
	prompt = g_strdup_printf (_("Rename the \"%s\" folder to:"), name);
	while (!done) {
		const char *why;
		
		new_name = e_request_string (NULL, _("Rename Folder"), prompt, name);
		if (new_name == NULL || !strcmp (name, new_name)) {
			/* old name == new name */
			done = TRUE;
		} else {
			CamelFolderInfo *fi;
			CamelException ex;
			char *base, *path;
			
			/* FIXME: we can't use the os independent path crap here, since we want to control the format */
			base = g_path_get_dirname (folder_path);
			path = g_build_filename (base, new_name, NULL);
			
			camel_exception_init (&ex);
			if ((fi = camel_store_get_folder_info (store, path, CAMEL_STORE_FOLDER_INFO_FAST, &ex)) != NULL) {
				camel_store_free_folder_info (store, fi);
				
				e_notice (NULL, GTK_MESSAGE_ERROR,
					  _("A folder named \"%s\" already exists. Please use a different name."),
					  new_name);
			} else {
				const char *oldpath, *newpath;
				
				oldpath = folder_path + 1;
				newpath = path + 1;
				
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
			g_free (base);
		}
		
		g_free (new_name);
	}
}

struct _prop_data {
	void *object;
	CamelArgV *argv;
	GtkWidget **widgets;
};

static void
emft_popup_properties_response (GtkWidget *dialog, int response, struct _prop_data *prop_data)
{
	CamelArgV *argv = prop_data->argv;
	int i;
	
	if (response != GTK_RESPONSE_OK) {
		gtk_widget_destroy (dialog);
		return;
	}
	
	for (i = 0; i < argv->argc; i++) {
		CamelArg *arg = &argv->argv[i];
		
		switch (arg->tag & CAMEL_ARG_TYPE) {
		case CAMEL_ARG_BOO:
			arg->ca_int = gtk_toggle_button_get_active ((GtkToggleButton *) prop_data->widgets[i]);
			break;
		case CAMEL_ARG_STR:
			g_free (arg->ca_str);
			arg->ca_str = (char *) gtk_entry_get_text ((GtkEntry *) prop_data->widgets[i]);
			break;
		default:
			g_assert_not_reached ();
			break;
		}
	}
	
	camel_object_setv (prop_data->object, NULL, argv);
	gtk_widget_destroy (dialog);
}

static void
emft_popup_properties_free (void *data)
{
	struct _prop_data *prop_data = data;
	int i;
	
	for (i = 0; i < prop_data->argv->argc; i++) {
		if ((prop_data->argv->argv[i].tag & CAMEL_ARG_TYPE) == CAMEL_ARG_STR)
			g_free (prop_data->argv->argv[i].ca_str);
	}
	
	camel_object_unref (prop_data->object);
	g_free (prop_data->argv);
	g_free (prop_data);
}

static void
emft_popup_properties_got_folder (char *uri, CamelFolder *folder, void *data)
{
	GtkWidget *dialog, *w, *table, *label;
	struct _prop_data *prop_data;
	CamelArgGetV *arggetv;
	CamelArgV *argv;
	GSList *list, *l;
	gint32 count, i;
	char *name;
	int row = 1;
	
	if (folder == NULL)
		return;
	
	camel_object_get (folder, NULL, CAMEL_FOLDER_PROPERTIES, &list, CAMEL_FOLDER_NAME, &name, NULL);
	
	dialog = gtk_dialog_new_with_buttons (_("Folder properties"), NULL,
					      GTK_DIALOG_DESTROY_WITH_PARENT,
					      GTK_STOCK_OK, GTK_RESPONSE_OK,
					      NULL);
	
	/* TODO: maybe we want some basic properties here, like message counts/approximate size/etc */
	w = gtk_frame_new (_("Properties"));
	gtk_widget_show (w);
	gtk_box_pack_start ((GtkBox *) ((GtkDialog *) dialog)->vbox, w, TRUE, TRUE, 6);
	
	table = gtk_table_new (g_slist_length (list) + 1, 2, FALSE);
	gtk_widget_show (table);
	gtk_container_add ((GtkContainer *) w, table);
	
	label = gtk_label_new (_("Folder Name"));
	gtk_widget_show (label);
	gtk_misc_set_alignment ((GtkMisc *) label, 1.0, 0.5);
	gtk_table_attach ((GtkTable *) table, label, 0, 1, 0, 1, GTK_FILL | GTK_EXPAND, 0, 3, 0);
	
	label = gtk_label_new (name);
	gtk_widget_show (label);
	gtk_misc_set_alignment ((GtkMisc *) label, 0.0, 0.5);
	gtk_table_attach ((GtkTable *) table, label, 1, 2, 0, 1, GTK_FILL | GTK_EXPAND, 0, 3, 0);
	
	/* build an arggetv/argv to retrieve/store the results */
	count = g_slist_length (list);
	arggetv = g_malloc0 (sizeof (*arggetv) + (count - CAMEL_ARGV_MAX) * sizeof (arggetv->argv[0]));
	arggetv->argc = count;
	argv = g_malloc0 (sizeof (*argv) + (count - CAMEL_ARGV_MAX) * sizeof (argv->argv[0]));
	argv->argc = count;
	
	i = 0;
	l = list;
	while (l) {
		CamelProperty *prop = l->data;
		
		argv->argv[i].tag = prop->tag;
		arggetv->argv[i].tag = prop->tag;
		arggetv->argv[i].ca_ptr = &argv->argv[i].ca_ptr;
		
		l = l->next;
		i++;
	}
	
	camel_object_getv (folder, NULL, arggetv);
	g_free (arggetv);
	
	prop_data = g_malloc0 (sizeof (*prop_data));
	prop_data->widgets = g_malloc0 (sizeof (prop_data->widgets[0]) * count);
	prop_data->argv = argv;
	
	/* setup the ui with the values retrieved */
	l = list;
	i = 0;
	while (l) {
		CamelProperty *prop = l->data;
		
		switch (prop->tag & CAMEL_ARG_TYPE) {
		case CAMEL_ARG_BOO:
			w = gtk_check_button_new_with_label (prop->description);
			gtk_toggle_button_set_active ((GtkToggleButton *) w, argv->argv[i].ca_int != 0);
			gtk_widget_show (w);
			gtk_table_attach ((GtkTable *) table, w, 0, 2, row, row + 1, 0, 0, 3, 3);
			prop_data->widgets[i] = w;
			break;
		case CAMEL_ARG_STR:
			label = gtk_label_new (prop->description);
			gtk_misc_set_alignment ((GtkMisc *) label, 1.0, 0.5);
			gtk_widget_show (label);
			gtk_table_attach ((GtkTable *) table, label, 0, 1, row, row + 1, GTK_FILL | GTK_EXPAND, 0, 3, 3);
			
			w = gtk_entry_new ();
			gtk_widget_show (w);
			if (argv->argv[i].ca_str) {
				gtk_entry_set_text ((GtkEntry *) w, argv->argv[i].ca_str);
				camel_object_free (folder, argv->argv[i].tag, argv->argv[i].ca_str);
				argv->argv[i].ca_str = NULL;
			}
			gtk_table_attach ((GtkTable *) table, w, 1, 2, row, row + 1, GTK_FILL, 0, 3, 3);
			prop_data->widgets[i] = w;
			break;
		default:
			g_assert_not_reached ();
			break;
		}
		
		row++;
		l = l->next;
	}
	
	prop_data->object = folder;
	camel_object_ref (folder);
	
	camel_object_free (folder, CAMEL_FOLDER_PROPERTIES, list);
	camel_object_free (folder, CAMEL_FOLDER_NAME, name);
	
	/* we do 'apply on ok' ... since instant apply may apply some very long running tasks */
	
	g_signal_connect (dialog, "response", G_CALLBACK (emft_popup_properties_response), prop_data);
	g_object_set_data_full ((GObject *) dialog, "e-prop-data", prop_data, emft_popup_properties_free);
	gtk_widget_show (dialog);
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
	gtk_tree_selection_get_selected (selection, &model, &iter);
	gtk_tree_model_get (model, &iter, COL_STRING_URI, &uri, -1);
	
	mail_get_folder (uri, 0, emft_popup_properties_got_folder, emft, mail_thread_new);
}

static EMPopupItem emft_popup_menu[] = {
#if 0
	{ EM_POPUP_ITEM, "00.emc.00", N_("_View"), G_CALLBACK (emft_popup_view), NULL, NULL, 0 },
	{ EM_POPUP_ITEM, "00.emc.01", N_("Open in _New Window"), G_CALLBACK (emft_popup_open_new), NULL, NULL, 0 },

	{ EM_POPUP_BAR, "10.emc" },
#endif
	{ EM_POPUP_ITEM, "10.emc.00", N_("_Copy"), G_CALLBACK (emft_popup_copy), NULL, "folder-copy-16.png", 0 },
	{ EM_POPUP_ITEM, "10.emc.01", N_("_Move"), G_CALLBACK (emft_popup_move), NULL, "folder-move-16.png", 0 },
	
	{ EM_POPUP_BAR, "20.emc" },
	{ EM_POPUP_ITEM, "20.emc.00", N_("_New Folder..."), G_CALLBACK (emft_popup_new_folder), NULL, "folder-mini.png", 0 },
	{ EM_POPUP_ITEM, "20.emc.01", N_("_Delete"), G_CALLBACK (emft_popup_delete_folder), NULL, "evolution-trash-mini.png", 0 },
	{ EM_POPUP_ITEM, "20.emc.01", N_("_Rename"), G_CALLBACK (emft_popup_rename_folder), NULL, NULL, 0 },
	
	{ EM_POPUP_BAR, "80.emc" },
	{ EM_POPUP_ITEM, "80.emc.00", N_("_Properties..."), G_CALLBACK (emft_popup_properties), NULL, "configure_16_folder.xpm", 0 },
};

static gboolean
tree_button_press (GtkWidget *treeview, GdkEventButton *event, EMFolderTree *emft)
{
	GSList *menus = NULL;
	GtkMenu *menu;
	EMPopup *emp;
	int i;
	
	if (event->button != 3)
		return FALSE;
	
	/* handle right-click by opening a context menu */
	emp = em_popup_new ("com.ximian.mail.storageset.popup.select");
	
	for (i = 0; i < sizeof (emft_popup_menu) / sizeof (emft_popup_menu[0]); i++) {
		EMPopupItem *item = &emft_popup_menu[i];
		
		item->activate_data = emft;
		menus = g_slist_prepend (menus, item);
	}
	
	em_popup_add_items (emp, menus, (GDestroyNotify) g_slist_free);
	
	menu = em_popup_create_menu_once (emp, NULL, 0, 0);
	
	if (event == NULL || event->type == GDK_KEY_PRESS) {
		/* FIXME: menu pos function */
		gtk_menu_popup (menu, NULL, NULL, NULL, NULL, 0, event->time);
	} else {
		gtk_menu_popup (menu, NULL, NULL, NULL, NULL, event->button, event->time);
	}
	
	return TRUE;
}


static void
tree_selection_changed (GtkTreeSelection *selection, EMFolderTree *emft)
{
	struct _EMFolderTreePrivate *priv = emft->priv;
	GtkTreeModel *model;
	GtkTreeIter iter;
	char *path, *uri;
	
	if (!gtk_tree_selection_get_selected (selection, &model, &iter))
		return;
	
	gtk_tree_model_get (model, &iter, COL_STRING_FOLDER_PATH, &path,
			    COL_STRING_URI, &uri, -1);
	
	g_free (priv->selected_uri);
	priv->selected_uri = g_strdup (uri);
	
	g_free (priv->selected_path);
	priv->selected_path = g_strdup (path);
	
	g_signal_emit (emft, signals[FOLDER_SELECTED], 0, path, uri);
}


static void
folder_subscribed_cb (CamelStore *store, void *event_data, EMFolderTree *emft)
{
	CamelFolderInfo *fi = event_data;
	struct _emft_store_info *si;
	GtkTreeRowReference *row;
	GtkTreeIter parent, iter;
	GtkTreeModel *model;
	GtkTreePath *path;
	gboolean load;
	char *dirname;
	
	if (!(si = g_hash_table_lookup (emft->priv->store_hash, store)))
		return;
	
	/* make sure we don't already know about it? */
	if (g_hash_table_lookup (si->path_hash, fi->path))
		return;
	
	/* get our parent folder's path */
	if (!(dirname = g_path_get_dirname (fi->path)))
		return;
	
	if (!strcmp (dirname, "/")) {
		/* user subscribed to a toplevel folder */
		row = si->row;
		g_free (dirname);
	} else {
		row = g_hash_table_lookup (si->path_hash, dirname);
		g_free (dirname);
		
		/* if row is NULL, don't bother adding to the tree,
		 * when the user expands enough nodes - it will be
		 * added auto-magically */
		if (row == NULL)
			return;
	}
	
	path = gtk_tree_row_reference_get_path (row);
	model = gtk_tree_view_get_model (emft->priv->treeview);
	if (!(gtk_tree_model_get_iter (model, &parent, path))) {
		gtk_tree_path_free (path);
		return;
	}
	
	gtk_tree_path_free (path);
	
	/* make sure parent's subfolders have already been loaded */
	gtk_tree_model_get (model, &parent, COL_BOOL_LOAD_SUBDIRS, &load, -1);
	if (load)
		return;
	
	/* append a new node */
	gtk_tree_store_append ((GtkTreeStore *) model, &iter, &parent);
	
	tree_store_set_folder_info ((GtkTreeStore *) model, &iter, emft->priv, si, fi);
}

static void
remove_folders (EMFolderTree *emft, GtkTreeModel *model, struct _emft_store_info *si, GtkTreeIter *toplevel)
{
	struct _EMFolderTreePrivate *priv = emft->priv;
	GtkTreeRowReference *row;
	char *uri, *folder_path;
	gboolean is_store, go;
	GtkTreeIter iter;
	
	if (gtk_tree_model_iter_children (model, &iter, toplevel)) {
		do {
			GtkTreeIter next = iter;
			
			go = gtk_tree_model_iter_next (model, &next);
			remove_folders (emft, model, si, &iter);
			iter = next;
		} while (go);
	}
	
	gtk_tree_model_get (model, toplevel, COL_STRING_URI, &uri,
			    COL_STRING_FOLDER_PATH, &folder_path,
			    COL_BOOL_IS_STORE, &is_store, -1);
	
	if ((row = g_hash_table_lookup (si->path_hash, folder_path))) {
		g_hash_table_remove (si->path_hash, folder_path);
		gtk_tree_row_reference_free (row);
	}
	
	if ((row = g_hash_table_lookup (priv->uri_hash, uri))) {
		g_hash_table_remove (priv->uri_hash, uri);
		gtk_tree_row_reference_free (row);
	}
	
	gtk_tree_store_remove ((GtkTreeStore *) model, toplevel);
	
	if (is_store) {
		g_hash_table_remove (priv->store_hash, si->store);
		store_info_free (si);
	}
}

static void
folder_unsubscribed_cb (CamelStore *store, void *event_data, EMFolderTree *emft)
{
	CamelFolderInfo *fi = event_data;
	struct _emft_store_info *si;
	GtkTreeRowReference *row;
	GtkTreeModel *model;
	GtkTreePath *path;
	GtkTreeIter iter;
	
	if (!(si = g_hash_table_lookup (emft->priv->store_hash, store)))
		return;
	
	if (!(row = g_hash_table_lookup (si->path_hash, fi->path)))
		return;
	
	path = gtk_tree_row_reference_get_path (row);
	model = gtk_tree_view_get_model (emft->priv->treeview);
	if (!(gtk_tree_model_get_iter (model, &iter, path))) {
		gtk_tree_path_free (path);
		return;
	}
	
	remove_folders (emft, model, si, &iter);
}

static void
folder_created_cb (CamelStore *store, void *event_data, EMFolderTree *emft)
{
	/* we only want created events to do more work if we don't support subscriptions */
	if (!camel_store_supports_subscriptions (store))
		folder_subscribed_cb (store, event_data, emft);
}

static void
folder_deleted_cb (CamelStore *store, void *event_data, EMFolderTree *emft)
{
	/* we only want deleted events to do more work if we don't support subscriptions */
	if (!camel_store_supports_subscriptions (store))
		folder_unsubscribed_cb (store, event_data, emft);
}

static void
folder_renamed_cb (CamelStore *store, void *event_data, EMFolderTree *emft)
{
	/* FIXME: implement me */
}


void
em_folder_tree_add_store (EMFolderTree *emft, CamelStore *store, const char *display_name)
{
	struct _EMFolderTreePrivate *priv;
	struct _emft_store_info *si;
	GtkTreeRowReference *row;
	GtkTreeIter root, iter;
	GtkTreeStore *model;
	GtkTreePath *path;
	char *uri;
	
	g_return_if_fail (EM_IS_FOLDER_TREE (emft));
	g_return_if_fail (CAMEL_IS_STORE (store));
	g_return_if_fail (display_name != NULL);
	
	priv = emft->priv;
	model = (GtkTreeStore *) gtk_tree_view_get_model (priv->treeview);
	
	if ((si = g_hash_table_lookup (priv->store_hash, store))) {
		const char *name;
		
		path = gtk_tree_row_reference_get_path (si->row);
		gtk_tree_model_get_iter ((GtkTreeModel *) model, &iter, path);
		gtk_tree_path_free (path);
		
		gtk_tree_model_get ((GtkTreeModel *) model, &iter, COL_STRING_DISPLAY_NAME, (char **) &name, -1);
		
		g_warning ("the store `%s' is already in the folder tree as `%s'",
			   display_name, name);
		
		return;
	}
	
	uri = camel_url_to_string (((CamelService *) store)->url, CAMEL_URL_HIDE_ALL);
	
	/* add the store to the tree */
	gtk_tree_store_append (model, &iter, NULL);
	gtk_tree_store_set (model, &iter,
			    COL_STRING_DISPLAY_NAME, display_name,
			    COL_POINTER_CAMEL_STORE, store,
			    COL_STRING_FOLDER_PATH, "/",
			    COL_BOOL_LOAD_SUBDIRS, TRUE,
			    COL_BOOL_IS_STORE, TRUE,
			    COL_STRING_URI, uri, -1);
	
	path = gtk_tree_model_get_path ((GtkTreeModel *) model, &iter);
	row = gtk_tree_row_reference_new ((GtkTreeModel *) model, path);
	gtk_tree_path_free (path);
	
	si = g_new (struct _emft_store_info, 1);
	si->display_name = g_strdup (display_name);
	camel_object_ref (store);
	si->store = store;
	si->row = row;
	si->path_hash = g_hash_table_new (g_str_hash, g_str_equal);
	g_hash_table_insert (priv->store_hash, store, si);
	
	/* each store has folders... but we don't load them until the user demands them */
	root = iter;
	gtk_tree_store_append (model, &iter, &root);
	gtk_tree_store_set (model, &iter,
			    COL_STRING_DISPLAY_NAME, _("Loading..."),
			    COL_POINTER_CAMEL_STORE, store,
			    COL_STRING_FOLDER_PATH, "/",
			    COL_BOOL_LOAD_SUBDIRS, TRUE,
			    COL_BOOL_IS_STORE, FALSE,
			    COL_STRING_URI, uri,
			    COL_UINT_UNREAD, 0,
			    -1);
	
	g_free (uri);
	
	/* listen to store events */
#define CAMEL_CALLBACK(func) ((CamelObjectEventHookFunc) func)
	si->created_id = camel_object_hook_event (store, "folder_created", CAMEL_CALLBACK (folder_created_cb), emft);
	si->deleted_id = camel_object_hook_event (store, "folder_deleted", CAMEL_CALLBACK (folder_deleted_cb), emft);
	si->renamed_id = camel_object_hook_event (store, "folder_renamed", CAMEL_CALLBACK (folder_renamed_cb), emft);
	si->subscribed_id = camel_object_hook_event (store, "folder_subscribed", CAMEL_CALLBACK (folder_subscribed_cb), emft);
	si->unsubscribed_id = camel_object_hook_event (store, "folder_unsubscribed", CAMEL_CALLBACK (folder_unsubscribed_cb), emft);
}


void
em_folder_tree_remove_store (EMFolderTree *emft, CamelStore *store)
{
	struct _EMFolderTreePrivate *priv;
	struct _emft_store_info *si;
	GtkTreeModel *model;
	GtkTreePath *path;
	GtkTreeIter iter;
	
	g_return_if_fail (EM_IS_FOLDER_TREE (emft));
	g_return_if_fail (CAMEL_IS_STORE (store));
	
	priv = emft->priv;
	model = gtk_tree_view_get_model (priv->treeview);
	
	if (!(si = g_hash_table_lookup (priv->store_hash, store))) {
		g_warning ("the store `%s' is not in the folder tree", si->display_name);
		
		return;
	}
	
	path = gtk_tree_row_reference_get_path (si->row);
	gtk_tree_model_get_iter (model, &iter, path);
	gtk_tree_path_free (path);
	
	/* recursively remove subfolders and finally the toplevel store */
	remove_folders (emft, model, si, &iter);
}


void
em_folder_tree_set_selected (EMFolderTree *emft, const char *uri)
{
	g_return_if_fail (EM_IS_FOLDER_TREE (emft));
	
	/* FIXME: implement me */
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
	
	return (EMFolderTreeModel *) gtk_tree_view_get_model (emft->priv->treeview);
}
