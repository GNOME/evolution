/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) version 3.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with the program; if not, see <http://www.gnu.org/licenses/>
 *
 *
 * Authors:
 *		Jeffrey Stedfast <fejj@ximian.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
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

#include <glib/gi18n.h>
#include <gdk/gdkkeysyms.h>
#include <glib/gi18n.h>

#include <camel/camel-session.h>
#include <camel/camel-store.h>
#include <camel/camel-folder.h>
#include <camel/camel-vee-store.h>
#include <camel/camel-vtrash-folder.h>
#include <camel/camel-stream-mem.h>
#include <camel/camel-file-utils.h>
#include <camel/camel-stream-fs.h>

#include "e-util/e-account-utils.h"
#include "e-util/e-mktemp.h"
#include "e-util/e-request.h"
#include "e-util/e-icon-factory.h"
#include "e-util/e-error.h"
#include "e-util/e-util.h"

#include "em-vfolder-rule.h"

#include "mail-mt.h"
#include "mail-ops.h"
#include "mail-tools.h"
#include "mail-config.h"
#include "mail-send-recv.h"
#include "mail-vfolder.h"

#include "em-utils.h"
#include "em-folder-tree.h"
#include "em-folder-utils.h"
#include "em-folder-selector.h"
#include "em-folder-selection.h"
#include "em-folder-properties.h"
#include "em-event.h"

#include "e-mail-local.h"

#define d(x)

#define EM_FOLDER_TREE_GET_PRIVATE(obj) \
	(G_TYPE_INSTANCE_GET_PRIVATE \
	((obj), EM_TYPE_FOLDER_TREE, EMFolderTreePrivate))

struct _selected_uri {
	gchar *key;		/* store:path or account/path */
	gchar *uri;
	CamelStore *store;
	gchar *path;
};

struct _EMFolderTreePrivate {
	GSList *select_uris;	/* selected_uri structures of each path pending selection. */
	GHashTable *select_uris_table; /*Removed as they're encountered, so use this to find uri's not presnet but selected */

	guint32 excluded;
	gboolean (*excluded_func)(EMFolderTree *emft, GtkTreeModel *model, GtkTreeIter *iter, gpointer data);
	gpointer excluded_data;

	guint cursor_set:1;	/* set to TRUE means we or something
				 * else has set the cursor, otherwise
				 * we need to set it when we set the
				 * selection */

	guint autoscroll_id;
	guint autoexpand_id;
	GtkTreeRowReference *autoexpand_row;

	guint loading_row_id;
	guint loaded_row_id;

	GtkTreeRowReference *drag_row;
	gboolean skip_double_click;
};

enum {
	FOLDER_ACTIVATED,  /* aka double-clicked or user hit enter */
	FOLDER_SELECTED,
	POPUP_EVENT,
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
	{ (gchar *) "x-folder",         0, DND_DRAG_TYPE_FOLDER         },
	{ (gchar *) "text/uri-list",    0, DND_DRAG_TYPE_TEXT_URI_LIST  },
};

static GtkTargetEntry drop_types[] = {
	{ (gchar *) "x-uid-list" ,      0, DND_DROP_TYPE_UID_LIST       },
	{ (gchar *) "x-folder",         0, DND_DROP_TYPE_FOLDER         },
	{ (gchar *) "message/rfc822",   0, DND_DROP_TYPE_MESSAGE_RFC822 },
	{ (gchar *) "text/uri-list",    0, DND_DROP_TYPE_TEXT_URI_LIST  },
};

static GdkAtom drag_atoms[NUM_DRAG_TYPES];
static GdkAtom drop_atoms[NUM_DROP_TYPES];

static guint signals[LAST_SIGNAL] = { 0 };

extern CamelSession *session;
extern CamelStore *vfolder_store;

struct _emft_selection_data {
	GtkTreeModel *model;
	GtkTreeIter *iter;
	gboolean set;
};

static gpointer parent_class = NULL;

struct _EMFolderTreeGetFolderInfo {
	MailMsg base;

	/* input data */
	GtkTreeRowReference *root;
	EMFolderTree *emft;
	CamelStore *store;
	guint32 flags;
	gchar *top;

	/* output data */
	CamelFolderInfo *fi;
};

static gchar *
emft_get_folder_info__desc (struct _EMFolderTreeGetFolderInfo *m)
{
	gchar *ret, *name;

	name = camel_service_get_name((CamelService *)m->store, TRUE);
	ret = g_strdup_printf(_("Scanning folders in \"%s\""), name);
	g_free(name);
	return ret;
}

static void
emft_get_folder_info__exec (struct _EMFolderTreeGetFolderInfo *m)
{
	guint32 flags = m->flags | CAMEL_STORE_FOLDER_INFO_SUBSCRIBED;

	m->fi = camel_store_get_folder_info (m->store, m->top, flags, &m->base.ex);
}

static void
emft_get_folder_info__done (struct _EMFolderTreeGetFolderInfo *m)
{
	struct _EMFolderTreeModelStoreInfo *si;
	GtkTreeIter root, iter, titer;
	CamelFolderInfo *fi;
	GtkTreeView *tree_view;
	GtkTreeModel *model;
	GtkTreePath *path;
	gboolean is_store;

	/* check that we haven't been destroyed */
	g_return_if_fail (GTK_IS_TREE_VIEW (m->emft));

	/* check that our parent folder hasn't been deleted/unsubscribed */
	if (!gtk_tree_row_reference_valid (m->root))
		return;

	tree_view = GTK_TREE_VIEW (m->emft);
	model = gtk_tree_view_get_model (tree_view);

	si = em_folder_tree_model_lookup_store_info (
		EM_FOLDER_TREE_MODEL (model), m->store);
	if (si == NULL) {
		/* store has been removed in the interim - do nothing */
		return;
	}

	path = gtk_tree_row_reference_get_path (m->root);
	gtk_tree_model_get_iter (model, &root, path);

	/* if we had an error, then we need to re-set the load subdirs state and collapse the node */
	if (!m->fi && camel_exception_is_set(&m->base.ex)) {
		gtk_tree_store_set(
			GTK_TREE_STORE (model), &root,
			COL_BOOL_LOAD_SUBDIRS, TRUE, -1);
		gtk_tree_view_collapse_row (tree_view, path);
		gtk_tree_path_free (path);
		return;
	}

	gtk_tree_path_free (path);

	/* make sure we still need to load the tree subfolders... */
	gtk_tree_model_get (model, &root, COL_BOOL_IS_STORE, &is_store, -1);

	/* get the first child (which will be a dummy node) */
	gtk_tree_model_iter_children (model, &iter, &root);

	/* Traverse to the last valid iter */
	titer = iter;
	while (gtk_tree_model_iter_next (model, &iter))
		titer = iter; /* Preserve the last valid iter */

	iter = titer;

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
		gtk_tree_store_remove (GTK_TREE_STORE (model), &iter);

		if (is_store) {
			path = gtk_tree_model_get_path (model, &root);
			gtk_tree_view_collapse_row (tree_view, path);
			gtk_tree_path_free (path);
			return;
		}
	} else {
		gint fully_loaded = (m->flags & CAMEL_STORE_FOLDER_INFO_RECURSIVE) ? TRUE : FALSE;

		do {
			gboolean known = g_hash_table_lookup (si->full_hash, fi->full_name) != NULL;

			if (!known)
				em_folder_tree_model_set_folder_info (
					EM_FOLDER_TREE_MODEL (model),
					&iter, si, fi, fully_loaded);

			if ((fi = fi->next) != NULL && !known)
				gtk_tree_store_append (GTK_TREE_STORE (model), &iter, &root);
		} while (fi != NULL);
	}

	gtk_tree_store_set (
		GTK_TREE_STORE (model), &root,
		COL_BOOL_LOAD_SUBDIRS, FALSE, -1);
}

static void
emft_get_folder_info__free (struct _EMFolderTreeGetFolderInfo *m)
{
	camel_store_free_folder_info (m->store, m->fi);

	gtk_tree_row_reference_free (m->root);
	g_object_unref(m->emft);
	camel_object_unref (m->store);
	g_free (m->top);
}

static MailMsgInfo get_folder_info_info = {
	sizeof (struct _EMFolderTreeGetFolderInfo),
	(MailMsgDescFunc) emft_get_folder_info__desc,
	(MailMsgExecFunc) emft_get_folder_info__exec,
	(MailMsgDoneFunc) emft_get_folder_info__done,
	(MailMsgFreeFunc) emft_get_folder_info__free
};

static void
folder_tree_emit_popup_event (EMFolderTree *emft,
                              GdkEvent *event)
{
	g_signal_emit (emft, signals[POPUP_EVENT], 0, event);
}

static void
emft_free_select_uri (struct _selected_uri *u)
{
	g_free (u->uri);
	if (u->store)
		camel_object_unref (u->store);
	g_free (u->key);
	g_free (u->path);
	g_free (u);
}

static gboolean
emft_select_func (GtkTreeSelection *selection,
                  GtkTreeModel *model,
                  GtkTreePath *path,
                  gboolean selected)
{
	EMFolderTreePrivate *priv;
	GtkTreeView *tree_view;
	gboolean is_store;
	guint32 flags;
	GtkTreeIter iter;

	tree_view = gtk_tree_selection_get_tree_view (selection);

	priv = EM_FOLDER_TREE_GET_PRIVATE (tree_view);

	if (selected)
		return TRUE;

	if (priv->excluded == 0 && priv->excluded_func == NULL)
		return TRUE;

	if (!gtk_tree_model_get_iter (model, &iter, path))
		return TRUE;

	if (priv->excluded_func != NULL)
		return priv->excluded_func(
			EM_FOLDER_TREE (tree_view), model,
			&iter, priv->excluded_data);

	gtk_tree_model_get (
		model, &iter, COL_UINT_FLAGS, &flags,
		COL_BOOL_IS_STORE, &is_store, -1);

	if (is_store)
		flags |= CAMEL_FOLDER_NOSELECT;

	return (flags & priv->excluded) == 0;
}

static void
emft_clear_selected_list(EMFolderTree *emft)
{
	EMFolderTreePrivate *priv = emft->priv;

	g_slist_foreach(priv->select_uris, (GFunc) emft_free_select_uri, NULL);
	g_slist_free(priv->select_uris);
	g_hash_table_destroy(priv->select_uris_table);
	priv->select_uris = NULL;
	priv->select_uris_table = g_hash_table_new(g_str_hash, g_str_equal);
	priv->cursor_set = FALSE;
}

static void
emft_selection_changed_cb (EMFolderTree *emft,
                           GtkTreeSelection *selection)
{
	GtkTreeModel *model;
	GtkTreeIter iter;
	GList *list;
	guint32 flags = 0;
	guint unread = 0;
	guint old_unread = 0;
	gchar *full_name = NULL;
	gchar *uri = NULL;

	list = gtk_tree_selection_get_selected_rows (selection, &model);

	if (list == NULL)
		goto exit;

	gtk_tree_model_get_iter (model, &iter, list->data);

	gtk_tree_model_get (
		model, &iter,
		COL_STRING_FULL_NAME, &full_name,
		COL_STRING_URI, &uri, COL_UINT_FLAGS, &flags,
		COL_UINT_UNREAD, &unread, COL_UINT_UNREAD_LAST_SEL,
		&old_unread, -1);

	/* Sync unread counts to distinguish new incoming mail. */
	if (unread != old_unread)
		gtk_tree_store_set (
			GTK_TREE_STORE (model), &iter,
			COL_UINT_UNREAD_LAST_SEL, unread, -1);

exit:
	g_signal_emit (
		emft, signals[FOLDER_SELECTED], 0, full_name, uri, flags);

	g_free (full_name);
	g_free (uri);

	g_list_foreach (list, (GFunc) gtk_tree_path_free, NULL);
	g_list_free (list);
}

static void
folder_tree_finalize (GObject *object)
{
	EMFolderTreePrivate *priv;

	priv = EM_FOLDER_TREE_GET_PRIVATE (object);

	if (priv->select_uris != NULL) {
		g_slist_foreach (
			priv->select_uris,
			(GFunc) emft_free_select_uri, NULL);
		g_slist_free (priv->select_uris);
		g_hash_table_destroy (priv->select_uris_table);
		priv->select_uris = NULL;
	}

	/* Chain up to parent's finalize() method. */
	G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
em_folder_tree_destroy (GtkObject *object)
{
	EMFolderTreePrivate *priv;
	GtkTreeModel *model;

	priv = EM_FOLDER_TREE_GET_PRIVATE (object);

	model = gtk_tree_view_get_model (GTK_TREE_VIEW (object));

	if (priv->loaded_row_id != 0) {
		g_signal_handler_disconnect (model, priv->loaded_row_id);
		priv->loaded_row_id = 0;
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

	/* Chain up to parent's destroy() method. */
	GTK_OBJECT_CLASS (parent_class)->destroy (object);
}

static gboolean
emft_button_press_event (GtkWidget *widget,
                         GdkEventButton *event)
{
	EMFolderTreePrivate *priv;
	GtkWidgetClass *widget_class;
	GtkTreeSelection *selection;
	GtkTreeView *tree_view;
	GtkTreePath *path;

	priv = EM_FOLDER_TREE_GET_PRIVATE (widget);

	tree_view = GTK_TREE_VIEW (widget);
	selection = gtk_tree_view_get_selection (tree_view);

	if (gtk_tree_selection_get_mode (selection) == GTK_SELECTION_SINGLE)
		emft_clear_selected_list (EM_FOLDER_TREE (widget));

	priv->cursor_set = TRUE;

	if (event->button != 3)
		goto chainup;

	if (!gtk_tree_view_get_path_at_pos (
		tree_view, event->x, event->y,
		&path, NULL, NULL, NULL))
		goto chainup;

	/* select/focus the row that was right-clicked */
	gtk_tree_selection_select_path (selection, path);
	gtk_tree_view_set_cursor (tree_view, path, NULL, FALSE);

	gtk_tree_path_free (path);

	folder_tree_emit_popup_event (
		EM_FOLDER_TREE (tree_view), (GdkEvent *) event);

chainup:

	/* Chain up to parent's button_press_event() method. */
	widget_class = GTK_WIDGET_CLASS (parent_class);
	return widget_class->button_press_event (widget, event);
}

static gboolean
emft_key_press_event (GtkWidget *widget,
                      GdkEventKey *event)
{
	EMFolderTreePrivate *priv;
	GtkWidgetClass *widget_class;
	GtkTreeSelection *selection;
	GtkTreeView *tree_view;

	priv = EM_FOLDER_TREE_GET_PRIVATE (widget);

	tree_view = GTK_TREE_VIEW (widget);
	selection = gtk_tree_view_get_selection (tree_view);

	if (gtk_tree_selection_get_mode (selection) == GTK_SELECTION_SINGLE)
		emft_clear_selected_list (EM_FOLDER_TREE (widget));

	priv->cursor_set = TRUE;

	/* Chain up to parent's key_press_event() method. */
	widget_class = GTK_WIDGET_CLASS (parent_class);
	return widget_class->key_press_event (widget, event);
}

static gboolean
emft_popup_menu (GtkWidget *widget)
{
	folder_tree_emit_popup_event (EM_FOLDER_TREE (widget), NULL);

	return TRUE;
}

static void
emft_row_activated (GtkTreeView *tree_view,
                    GtkTreePath *path,
                    GtkTreeViewColumn *column)
{
	EMFolderTreePrivate *priv;
	GtkTreeModel *model;
	gchar *full_name, *uri;
	GtkTreeIter iter;
	guint32 flags;

	priv = EM_FOLDER_TREE_GET_PRIVATE (tree_view);

	model = gtk_tree_view_get_model (tree_view);

	if (priv->skip_double_click)
		return;

	if (!gtk_tree_model_get_iter (model, &iter, path))
		return;

	gtk_tree_model_get (
		model, &iter, COL_STRING_FULL_NAME, &full_name,
		COL_STRING_URI, &uri, COL_UINT_FLAGS, &flags, -1);

	emft_clear_selected_list (EM_FOLDER_TREE (tree_view));

	g_signal_emit (
		tree_view, signals[FOLDER_SELECTED], 0, full_name, uri, flags);

	g_signal_emit (
		tree_view, signals[FOLDER_ACTIVATED], 0, full_name, uri);

	g_free (full_name);
	g_free (uri);
}

static gboolean
emft_test_collapse_row (GtkTreeView *tree_view,
                        GtkTreeIter *iter,
                        GtkTreePath *path)
{
	GtkTreeSelection *selection;
	GtkTreeModel *model;
	GtkTreeIter cursor;

	selection = gtk_tree_view_get_selection (tree_view);

	if (!gtk_tree_selection_get_selected (selection, &model, &cursor))
		goto exit;

	/* Select the collapsed node IFF it is a
	 * parent of the currently selected folder. */
	if (gtk_tree_store_is_ancestor (GTK_TREE_STORE (model), iter, &cursor))
		gtk_tree_view_set_cursor (tree_view, path, NULL, FALSE);

exit:
	return FALSE;
}

static void
emft_row_expanded (GtkTreeView *tree_view,
                   GtkTreeIter *iter,
                   GtkTreePath *path)
{
	struct _EMFolderTreeGetFolderInfo *msg;
	GtkTreeModel *model;
	CamelStore *store;
	gchar *full_name;
	gboolean load;

	model = gtk_tree_view_get_model (tree_view);

	gtk_tree_model_get (
		model, iter,
		COL_STRING_FULL_NAME, &full_name,
		COL_POINTER_CAMEL_STORE, &store,
		COL_BOOL_LOAD_SUBDIRS, &load, -1);

	if (!load) {
		g_free (full_name);
		return;
	}

	gtk_tree_store_set (
		GTK_TREE_STORE (model), iter,
		COL_BOOL_LOAD_SUBDIRS, FALSE, -1);

	msg = mail_msg_new (&get_folder_info_info);
	msg->root = gtk_tree_row_reference_new (model, path);
	camel_object_ref (store);
	msg->store = store;
	msg->emft = g_object_ref (tree_view);
	msg->top = full_name;
	msg->flags =
		CAMEL_STORE_FOLDER_INFO_RECURSIVE |
		CAMEL_STORE_FOLDER_INFO_FAST;

	mail_msg_unordered_push (msg);
}

static void
folder_tree_class_init (EMFolderTreeClass *class)
{
	GObjectClass *object_class;
	GtkObjectClass *gtk_object_class;
	GtkWidgetClass *widget_class;
	GtkTreeViewClass *tree_view_class;

	parent_class = g_type_class_peek_parent (class);
	g_type_class_add_private (class, sizeof (EMFolderTreePrivate));

	object_class = G_OBJECT_CLASS (class);
	object_class->finalize = folder_tree_finalize;

	gtk_object_class = GTK_OBJECT_CLASS (class);
	gtk_object_class->destroy = em_folder_tree_destroy;

	widget_class = GTK_WIDGET_CLASS (class);
	widget_class->button_press_event = emft_button_press_event;
	widget_class->key_press_event = emft_key_press_event;
	widget_class->popup_menu = emft_popup_menu;

	tree_view_class = GTK_TREE_VIEW_CLASS (class);
	tree_view_class->row_activated = emft_row_activated;
	tree_view_class->test_collapse_row = emft_test_collapse_row;
	tree_view_class->row_expanded = emft_row_expanded;

	signals[FOLDER_SELECTED] = g_signal_new (
		"folder-selected",
		G_OBJECT_CLASS_TYPE (object_class),
		G_SIGNAL_RUN_FIRST,
		G_STRUCT_OFFSET (EMFolderTreeClass, folder_selected),
		NULL, NULL,
		e_marshal_VOID__STRING_STRING_UINT,
		G_TYPE_NONE, 3,
		G_TYPE_STRING,
		G_TYPE_STRING,
		G_TYPE_UINT);

	signals[FOLDER_ACTIVATED] = g_signal_new (
		"folder-activated",
		G_OBJECT_CLASS_TYPE (object_class),
		G_SIGNAL_RUN_FIRST,
		G_STRUCT_OFFSET (EMFolderTreeClass, folder_activated),
		NULL, NULL,
		e_marshal_VOID__STRING_STRING,
		G_TYPE_NONE, 2,
		G_TYPE_STRING,
		G_TYPE_STRING);

	signals[POPUP_EVENT] = g_signal_new (
		"popup-event",
		G_OBJECT_CLASS_TYPE (object_class),
		G_SIGNAL_RUN_FIRST | G_SIGNAL_ACTION,
		G_STRUCT_OFFSET (EMFolderTreeClass, popup_event),
		NULL, NULL,
		g_cclosure_marshal_VOID__BOXED,
		G_TYPE_NONE, 1,
		GDK_TYPE_EVENT | G_SIGNAL_TYPE_STATIC_SCOPE);
}

static void
folder_tree_init (EMFolderTree *emft)
{
	GtkTreeSelection *selection;
	GHashTable *select_uris_table;
	EMFolderTreeModel *model;

	select_uris_table = g_hash_table_new (g_str_hash, g_str_equal);

	emft->priv = EM_FOLDER_TREE_GET_PRIVATE (emft);
	emft->priv->select_uris_table = select_uris_table;

	model = em_folder_tree_model_get_default ();
	gtk_tree_view_set_model (GTK_TREE_VIEW (emft), GTK_TREE_MODEL (model));

	selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (emft));

	g_signal_connect_swapped (
		selection, "changed",
		G_CALLBACK (emft_selection_changed_cb), emft);
}

GType
em_folder_tree_get_type (void)
{
	static GType type = 0;

	if (G_UNLIKELY (type == 0)) {
		static const GTypeInfo type_info = {
			sizeof (EMFolderTreeClass),
			(GBaseInitFunc) NULL,
			(GBaseFinalizeFunc) NULL,
			(GClassInitFunc) folder_tree_class_init,
			(GClassFinalizeFunc) NULL,
			NULL,  /* class_data */
			sizeof (EMFolderTree),
			0,     /* n_preallocs */
			(GInstanceInitFunc) folder_tree_init,
			NULL   /* value_table */
		};

		type = g_type_register_static (
			GTK_TYPE_TREE_VIEW, "EMFolderTree", &type_info, 0);
	}

	return type;
}

static gboolean
subdirs_contain_unread (GtkTreeModel *model, GtkTreeIter *root)
{
	guint unread;
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

static void
render_display_name (GtkTreeViewColumn *column, GtkCellRenderer *renderer,
		     GtkTreeModel *model, GtkTreeIter *iter, gpointer user_data)
{
	gboolean is_store, bold;
	guint unread;
	gchar *display;
	gchar *name;

	gtk_tree_model_get (model, iter, COL_STRING_DISPLAY_NAME, &name,
			    COL_BOOL_IS_STORE, &is_store,
			    COL_UINT_UNREAD, &unread, -1);

	if (!(bold = is_store || unread)) {
		if (gtk_tree_model_iter_has_child (model, iter))
			bold = subdirs_contain_unread (model, iter);
	}

	if (!is_store && unread) {
		/* Translators: This is the string used for displaying the
		 * folder names in folder trees. "%s" will be replaced by
		 * the folder's name and "%u" will be replaced with the
		 * number of unread messages in the folder.
		 *
		 * Most languages should translate this as "%s (%u)". The
		 * languages that use localized digits (like Persian) may
		 * need to replace "%u" with "%Iu". Right-to-left languages
		 * (like Arabic and Hebrew) may need to add bidirectional
		 * formatting codes to take care of the cases the folder
		 * name appears in either direction.
		 *
		 * Do not translate the "folder-display|" part. Remove it
		 * from your translation.
		 */
		display = g_strdup_printf (C_("folder-display", "%s (%u)"), name, unread);
		g_free (name);
	} else
		display = name;

	g_object_set (renderer, "text", display,
		      "weight", bold ? PANGO_WEIGHT_BOLD : PANGO_WEIGHT_NORMAL,
		      NULL);

	g_free (display);
}

static void
render_icon (GtkTreeViewColumn *column,
             GtkCellRenderer *renderer,
             GtkTreeModel *model,
             GtkTreeIter *iter)
{
	GtkTreeSelection *selection;
	GtkWidget *tree_view;
	GIcon *icon;
	guint unread;
	guint old_unread;
	gchar *icon_name;
	gboolean row_selected;

	gtk_tree_model_get (
		model, iter,
		COL_STRING_ICON_NAME, &icon_name,
		COL_UINT_UNREAD_LAST_SEL, &old_unread,
		COL_UINT_UNREAD, &unread, -1);

	if (icon_name == NULL)
		return;

	icon = g_themed_icon_new (icon_name);

	tree_view = gtk_tree_view_column_get_tree_view (column);
	selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (tree_view));
	row_selected = gtk_tree_selection_iter_is_selected (selection, iter);

	/* Show an emblem if there's new mail. */
	if (!row_selected && unread > old_unread) {
		GIcon *temp_icon;
		GEmblem *emblem;

		temp_icon = g_themed_icon_new ("emblem-new");
		emblem = g_emblem_new (temp_icon);
		g_object_unref (temp_icon);

		temp_icon = g_emblemed_icon_new (icon, emblem);
		g_object_unref (emblem);
		g_object_unref (icon);

		icon = temp_icon;
	}

	g_object_set (renderer, "gicon", icon, NULL);

	g_object_unref (icon);
}

static GtkTreeView *
folder_tree_new (EMFolderTree *emft)
{
	GtkTreeSelection *selection;
	GtkTreeViewColumn *column;
	GtkCellRenderer *renderer;
	GtkWidget *tree;
	GConfClient *gconf;

	gconf = mail_config_get_gconf_client ();

	/* FIXME Gross hack */
	tree = GTK_WIDGET (emft);
	GTK_WIDGET_SET_FLAGS(tree, GTK_CAN_FOCUS);

	column = gtk_tree_view_column_new ();
	gtk_tree_view_append_column ((GtkTreeView *) tree, column);

	renderer = gtk_cell_renderer_pixbuf_new ();
	gtk_tree_view_column_pack_start (column, renderer, FALSE);
	gtk_tree_view_column_add_attribute (
		column, renderer, "visible", COL_BOOL_IS_FOLDER);
	gtk_tree_view_column_set_cell_data_func (
		column, renderer, (GtkTreeCellDataFunc)
		render_icon, NULL, NULL);

	renderer = gtk_cell_renderer_text_new ();
	if (!gconf_client_get_bool (gconf, "/apps/evolution/mail/display/no_folder_dots", NULL))
		g_object_set (G_OBJECT (renderer), "ellipsize", PANGO_ELLIPSIZE_END, NULL);
	gtk_tree_view_column_pack_start (column, renderer, TRUE);
	gtk_tree_view_column_set_cell_data_func (column, renderer, render_display_name, NULL, NULL);

	selection = gtk_tree_view_get_selection ((GtkTreeView *) tree);
	gtk_tree_selection_set_mode (selection, GTK_SELECTION_SINGLE);
	gtk_tree_selection_set_select_function (
		selection, (GtkTreeSelectionFunc)
		emft_select_func, NULL, NULL);
	gtk_tree_view_set_headers_visible ((GtkTreeView *) tree, FALSE);

	gtk_tree_view_set_search_column((GtkTreeView *)tree, COL_STRING_DISPLAY_NAME);

	return (GtkTreeView *) tree;
}

static void
folder_tree_copy_expanded_cb (GtkTreeView *unused,
                              GtkTreePath *path,
                              GtkTreeView *tree_view)
{
	gtk_tree_view_expand_row (tree_view, path, FALSE);
}

static void
folder_tree_copy_selection_cb (GtkTreeModel *model,
                               GtkTreePath *path,
                               GtkTreeIter *iter,
                               GtkTreeView *tree_view)
{
	GtkTreeSelection *selection;

	selection = gtk_tree_view_get_selection (tree_view);
	gtk_tree_selection_select_path (selection, path);

	/* Center the tree view on the selected path. */
	gtk_tree_view_scroll_to_cell (tree_view, path, NULL, TRUE, 0.5, 0.0);
}

static void
folder_tree_copy_state (EMFolderTree *emft)
{
	GtkTreeSelection *selection;
	GtkTreeView *tree_view;
	GtkTreeModel *model;

	tree_view = GTK_TREE_VIEW (emft);
	model = gtk_tree_view_get_model (tree_view);

	selection = em_folder_tree_model_get_selection (
		EM_FOLDER_TREE_MODEL (model));
	if (selection == NULL)
		return;

	gtk_tree_view_map_expanded_rows (
		tree_view, (GtkTreeViewMappingFunc)
		folder_tree_copy_expanded_cb, emft);

	gtk_tree_selection_selected_foreach (
		selection, (GtkTreeSelectionForeachFunc)
		folder_tree_copy_selection_cb, emft);
}

static void
em_folder_tree_construct (EMFolderTree *emft)
{
	folder_tree_new (emft);
	folder_tree_copy_state (emft);
	gtk_widget_show (GTK_WIDGET (emft));
}

/* NOTE: Removes and frees the selected uri structure */
static void
emft_select_uri(EMFolderTree *emft, GtkTreePath *path, struct _selected_uri *u)
{
	EMFolderTreePrivate *priv = emft->priv;
	GtkTreeView *tree_view;
	GtkTreeSelection *selection;

	tree_view = GTK_TREE_VIEW (emft);
	selection = gtk_tree_view_get_selection (tree_view);
	gtk_tree_selection_select_path(selection, path);
	if (!priv->cursor_set) {
		gtk_tree_view_set_cursor (tree_view, path, NULL, FALSE);
		priv->cursor_set = TRUE;
	}
	gtk_tree_view_scroll_to_cell (tree_view, path, NULL, TRUE, 0.8f, 0.0f);
	g_hash_table_remove(priv->select_uris_table, u->key);
	priv->select_uris = g_slist_remove(priv->select_uris, u);
	emft_free_select_uri(u);
}

static void
emft_expand_node (const gchar *key, EMFolderTree *emft)
{
	struct _EMFolderTreeModelStoreInfo *si;
	GtkTreeRowReference *row;
	GtkTreeView *tree_view;
	GtkTreeModel *model;
	GtkTreePath *path;
	EAccount *account;
	CamelStore *store;
	const gchar *p;
	gchar *uid;
	gsize n;
	struct _selected_uri *u;

	if (!(p = strchr (key, '/')))
		n = strlen (key);
	else
		n = (p - key);

	uid = g_alloca (n + 1);
	memcpy (uid, key, n);
	uid[n] = '\0';

	tree_view = GTK_TREE_VIEW (emft);
	model = gtk_tree_view_get_model (tree_view);

	if ((account = e_get_account_by_uid (uid)) && account->enabled) {
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
		if (!(store = e_mail_local_get_store ()))
			return;

		camel_object_ref (store);
	} else {
		return;
	}

	si = em_folder_tree_model_lookup_store_info (
		EM_FOLDER_TREE_MODEL (model), store);
	if (si == NULL) {
		camel_object_unref (store);
		return;
	}

	camel_object_unref (store);

	if (p != NULL) {
		if (!(row = g_hash_table_lookup (si->full_hash, p + 1)))
			return;
	} else
		row = si->row;

	path = gtk_tree_row_reference_get_path (row);
	gtk_tree_view_expand_to_path (tree_view, path);

	u = g_hash_table_lookup(emft->priv->select_uris_table, key);
	if (u)
		emft_select_uri(emft, path, u);

	gtk_tree_path_free (path);
}

static void
emft_maybe_expand_row (EMFolderTreeModel *model, GtkTreePath *tree_path, GtkTreeIter *iter, EMFolderTree *emft)
{
	EMFolderTreePrivate *priv = emft->priv;
	struct _EMFolderTreeModelStoreInfo *si;
	GtkTreeView *tree_view;
	gboolean is_store;
	CamelStore *store;
	EAccount *account;
	gchar *full_name;
	gchar *key;
	struct _selected_uri *u;

	tree_view = GTK_TREE_VIEW (emft);

	gtk_tree_model_get ((GtkTreeModel *) model, iter,
			    COL_STRING_FULL_NAME, &full_name,
			    COL_POINTER_CAMEL_STORE, &store,
			    COL_BOOL_IS_STORE, &is_store,
			    -1);

	si = em_folder_tree_model_lookup_store_info (model, store);
	if ((account = e_get_account_by_name (si->display_name))) {
		key = g_strdup_printf ("%s/%s", account->uid, full_name ? full_name : "");
	} else if (CAMEL_IS_VEE_STORE (store)) {
		/* vfolder store */
		key = g_strdup_printf ("vfolder/%s", full_name ? full_name : "");
	} else {
		/* local store */
		key = g_strdup_printf ("local/%s", full_name ? full_name : "");
	}

	u = g_hash_table_lookup(priv->select_uris_table, key);
	if (u) {
		gchar *c = strrchr (key, '/');

		*c = '\0';
		emft_expand_node (key, emft);

		emft_select_uri(emft, tree_path, u);
	}

	g_free (full_name);
	g_free (key);
}

GtkWidget *
em_folder_tree_new (void)
{
	EMFolderTree *emft;
	GtkTreeModel *model;
	AtkObject *a11y;

	emft = g_object_new (EM_TYPE_FOLDER_TREE, NULL);
	em_folder_tree_construct (emft);

	model = gtk_tree_view_get_model (GTK_TREE_VIEW (emft));

	emft->priv->loading_row_id = g_signal_connect (
		model, "loading-row",
		G_CALLBACK (emft_maybe_expand_row), emft);
	emft->priv->loaded_row_id = g_signal_connect (
		model, "loaded-row",
		G_CALLBACK (emft_maybe_expand_row), emft);

	a11y = gtk_widget_get_accessible (GTK_WIDGET (emft));
	atk_object_set_name (a11y, _("Mail Folder Tree"));

	return (GtkWidget *) emft;
}

static void
tree_drag_begin (GtkWidget *widget, GdkDragContext *context, EMFolderTree *emft)
{
	EMFolderTreePrivate *priv = emft->priv;
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
	EMFolderTreePrivate *priv = emft->priv;
	gchar *full_name = NULL;
	GtkTreeModel *model;
	GtkTreePath *src_path;
	gboolean is_store;
	CamelStore *store;
	CamelException ex;
	GtkTreeIter iter;

	if (!priv->drag_row || (src_path = gtk_tree_row_reference_get_path (priv->drag_row)))
		return;

	model = gtk_tree_view_get_model (GTK_TREE_VIEW (emft));

	if (!gtk_tree_model_get_iter (model, &iter, src_path))
		goto fail;

	gtk_tree_model_get (
		model, &iter,
		COL_POINTER_CAMEL_STORE, &store,
		COL_STRING_FULL_NAME, &full_name,
		COL_BOOL_IS_STORE, &is_store, -1);

	if (is_store)
		goto fail;

	camel_exception_init(&ex);
	camel_store_delete_folder(store, full_name, &ex);
	if (camel_exception_is_set(&ex))
		camel_exception_clear(&ex);
fail:
	gtk_tree_path_free(src_path);
	g_free (full_name);
}

static void
tree_drag_data_get(GtkWidget *widget, GdkDragContext *context, GtkSelectionData *selection, guint info, guint time, EMFolderTree *emft)
{
	EMFolderTreePrivate *priv = emft->priv;
	gchar *full_name = NULL, *uri = NULL;
	GtkTreeModel *model;
	GtkTreePath *src_path;
	CamelFolder *folder;
	CamelStore *store;
	CamelException ex;
	GtkTreeIter iter;

	if (!priv->drag_row || !(src_path = gtk_tree_row_reference_get_path(priv->drag_row)))
		return;

	model = gtk_tree_view_get_model (GTK_TREE_VIEW (emft));

	if (!gtk_tree_model_get_iter (model, &iter, src_path))
		goto fail;

	gtk_tree_model_get (
		model, &iter,
		COL_POINTER_CAMEL_STORE, &store,
		COL_STRING_FULL_NAME, &full_name,
		COL_STRING_URI, &uri, -1);

	/* make sure user isn't trying to drag on a placeholder row */
	if (full_name == NULL)
		goto fail;

	camel_exception_init(&ex);

	switch (info) {
	case DND_DRAG_TYPE_FOLDER:
		/* dragging to a new location in the folder tree */
		gtk_selection_data_set(selection, drag_atoms[info], 8, (guchar *)uri, strlen (uri) + 1);
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
	g_free (full_name);
	g_free (uri);
}

/* TODO: Merge the drop handling code/menu's into one spot using a popup target for details */
/* Drop handling */
struct _DragDataReceivedAsync {
	MailMsg base;

	/* input data */
	GdkDragContext *context;

	/* Only selection->data and selection->length are valid */
	GtkSelectionData *selection;

	CamelStore *store;
	gchar *full_name;
	guint32 action;
	guint info;

	guint move:1;
	guint moved:1;
	guint aborted:1;
};

static void
emft_drop_folder(struct _DragDataReceivedAsync *m)
{
	CamelFolder *src;

	d(printf(" * Drop folder '%s' onto '%s'\n", m->selection->data, m->full_name));

	if (!(src = mail_tool_uri_to_folder((gchar *)m->selection->data, 0, &m->base.ex)))
		return;

	em_folder_utils_copy_folders(src->parent_store, src->full_name, m->store, m->full_name?m->full_name:"", m->move);
	camel_object_unref(src);
}

static gchar *
emft_drop_async__desc (struct _DragDataReceivedAsync *m)
{
	CamelURL *url;
	gchar *buf;

	if (m->info == DND_DROP_TYPE_FOLDER) {
		url = camel_url_new ((gchar *)m->selection->data, NULL);

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
emft_drop_async__exec (struct _DragDataReceivedAsync *m)
{
	CamelFolder *folder;

	/* for types other than folder, we can't drop to the root path */
	if (m->info == DND_DROP_TYPE_FOLDER) {
		/* copy or move (aka rename) a folder */
		emft_drop_folder(m);
	} else if (m->full_name == NULL) {
		camel_exception_set (&m->base.ex, CAMEL_EXCEPTION_SYSTEM,
				     _("Cannot drop message(s) into toplevel store"));
	} else if ((folder = camel_store_get_folder (m->store, m->full_name, 0, &m->base.ex))) {
		switch (m->info) {
		case DND_DROP_TYPE_UID_LIST:
			/* import a list of uids from another evo folder */
			em_utils_selection_get_uidlist(m->selection, folder, m->move, &m->base.ex);
			m->moved = m->move && !camel_exception_is_set(&m->base.ex);
			break;
		case DND_DROP_TYPE_MESSAGE_RFC822:
			/* import a message/rfc822 stream */
			em_utils_selection_get_message(m->selection, folder);
			break;
		case DND_DROP_TYPE_TEXT_URI_LIST:
			/* import an mbox, maildir, or mh folder? */
			em_utils_selection_get_urilist(m->selection, folder);
			break;
		default:
			abort();
		}
		camel_object_unref(folder);
	}
}

static void
emft_drop_async__free (struct _DragDataReceivedAsync *m)
{
	g_object_unref(m->context);
	camel_object_unref(m->store);
	g_free(m->full_name);

	g_free(m->selection->data);
	g_free(m->selection);
}

static MailMsgInfo emft_drop_async_info = {
	sizeof (struct _DragDataReceivedAsync),
	(MailMsgDescFunc) emft_drop_async__desc,
	(MailMsgExecFunc) emft_drop_async__exec,
	(MailMsgDoneFunc) NULL,
	(MailMsgFreeFunc) emft_drop_async__free
};

static void
tree_drag_data_action(struct _DragDataReceivedAsync *m)
{
	m->move = m->action == GDK_ACTION_MOVE;
	mail_msg_unordered_push (m);
}

static void
tree_drag_data_received(GtkWidget *widget, GdkDragContext *context, gint x, gint y, GtkSelectionData *selection, guint info, guint time, EMFolderTree *emft)
{
	GtkTreeViewDropPosition pos;
	GtkTreeModel *model;
	GtkTreeView *tree_view;
	GtkTreePath *dest_path;
	struct _DragDataReceivedAsync *m;
	gboolean is_store;
	CamelStore *store;
	GtkTreeIter iter;
	gchar *full_name;

	tree_view = GTK_TREE_VIEW (emft);
	model = gtk_tree_view_get_model (tree_view);

	if (!gtk_tree_view_get_dest_row_at_pos (tree_view, x, y, &dest_path, &pos))
		return;

	/* this means we are receiving no data */
	if (!selection->data || selection->length == -1) {
		gtk_drag_finish(context, FALSE, FALSE, GDK_CURRENT_TIME);
		return;
	}

	if (!gtk_tree_model_get_iter (model, &iter, dest_path)) {
		gtk_drag_finish(context, FALSE, FALSE, GDK_CURRENT_TIME);
		return;
	}

	gtk_tree_model_get (
		model, &iter,
		COL_POINTER_CAMEL_STORE, &store,
		COL_BOOL_IS_STORE, &is_store,
		COL_STRING_FULL_NAME, &full_name, -1);

	/* make sure user isn't try to drop on a placeholder row */
	if (full_name == NULL && !is_store) {
		gtk_drag_finish (context, FALSE, FALSE, GDK_CURRENT_TIME);
		return;
	}

	m = mail_msg_new (&emft_drop_async_info);
	m->context = context;
	g_object_ref(context);
	m->store = store;
	camel_object_ref(store);
	m->full_name = full_name;
	m->action = context->action;
	m->info = info;

	/* need to copy, goes away once we exit */
	m->selection = g_malloc0(sizeof(*m->selection));
	m->selection->data = g_malloc(selection->length);
	memcpy(m->selection->data, selection->data, selection->length);
	m->selection->length = selection->length;

	tree_drag_data_action(m);
}

static gboolean
is_special_local_folder (const gchar *name)
{
	return (!strcmp (name, "Drafts") || !strcmp (name, "Inbox") || !strcmp (name, "Outbox") || !strcmp (name, "Sent") || !strcmp (name, "Templates"));
}

static GdkAtom
emft_drop_target(EMFolderTree *emft, GdkDragContext *context, GtkTreePath *path)
{
	EMFolderTreePrivate *p = emft->priv;
	gchar *full_name = NULL, *uri = NULL, *src_uri = NULL;
	CamelStore *local, *sstore, *dstore;
	GdkAtom atom = GDK_NONE;
	gboolean is_store;
	GtkTreeModel *model;
	GtkTreeIter iter;
	GList *targets;
	guint32 flags = 0;

	model = gtk_tree_view_get_model (GTK_TREE_VIEW (emft));

	/* This is a bit of a mess, but should handle all the cases properly */

	if (!gtk_tree_model_get_iter (model, &iter, path))
		return GDK_NONE;

	gtk_tree_model_get (
		model, &iter,
		COL_BOOL_IS_STORE, &is_store,
		COL_STRING_FULL_NAME, &full_name,
		COL_UINT_FLAGS, &flags,
		COL_POINTER_CAMEL_STORE, &dstore,
		COL_STRING_URI, &uri, -1);

	local = e_mail_local_get_store ();

	targets = context->targets;

	/* Check for special destinations */
	if (uri && full_name) {
#if 0
		/* only allow copying/moving folders (not messages) into the local Outbox */
		if (dstore == local && !strcmp (full_name, "Outbox")) {
			GdkAtom xfolder;

			xfolder = drop_atoms[DND_DROP_TYPE_FOLDER];
			while (targets != NULL) {
				if (targets->data == (gpointer) xfolder) {
					atom = xfolder;
					goto done;
				}

				targets = targets->next;
			}

			goto done;
		}
#endif

		/* don't allow copying/moving into the UNMATCHED vfolder */
		if (!strncmp (uri, "vfolder:", 8) && !strcmp (full_name, CAMEL_UNMATCHED_NAME))
			goto done;

		/* don't allow copying/moving into a vTrash/vJunk folder */
		if (!strcmp (full_name, CAMEL_VTRASH_NAME)
		    || !strcmp (full_name, CAMEL_VJUNK_NAME))
			goto done;

		if (flags & CAMEL_FOLDER_NOSELECT)
			goto done;
	}

	if (p->drag_row) {
		GtkTreePath *src_path = gtk_tree_row_reference_get_path(p->drag_row);

		if (src_path) {
			if (gtk_tree_model_get_iter (model, &iter, src_path))
				gtk_tree_model_get (
					model, &iter,
					COL_POINTER_CAMEL_STORE, &sstore,
					COL_STRING_URI, &src_uri, -1);

			/* can't dnd onto itself or below itself - bad things happen,
			   no point dragging to where we were either */
			if (gtk_tree_path_compare(path, src_path) == 0
			    || gtk_tree_path_is_descendant(path, src_path)
			    || (gtk_tree_path_is_ancestor(path, src_path)
				&& gtk_tree_path_get_depth(path) == gtk_tree_path_get_depth(src_path)-1)) {
				gtk_tree_path_free(src_path);
				goto done;
			}

			gtk_tree_path_free(src_path);
		}
	}

	/* Check for special sources, and vfolder stuff */
	if (src_uri) {
		CamelURL *url;
		gchar *url_path;

		/* FIXME: this is a total hack, but i think all we can do at present */
		/* Check for dragging from special folders which can't be moved/copied */
		url = camel_url_new(src_uri, NULL);
		url_path = url->fragment?url->fragment:url->path;
		if (url_path && url_path[0]) {
			/* don't allow moving any of the the local special folders */
			if (sstore == local && is_special_local_folder (url_path)) {
				GdkAtom xfolder;

				camel_url_free (url);

				/* TODO: not sure if this is legal, but it works, force copy for special local folders */
				context->suggested_action = GDK_ACTION_COPY;
				xfolder = drop_atoms[DND_DROP_TYPE_FOLDER];
				while (targets != NULL) {
					if (targets->data == (gpointer) xfolder) {
						atom = xfolder;
						goto done;
					}

					targets = targets->next;
				}

				goto done;
			}

			/* don't allow copying/moving of the UNMATCHED vfolder */
			if (!strcmp (url->protocol, "vfolder") && !strcmp (url_path, CAMEL_UNMATCHED_NAME)) {
				camel_url_free (url);
				goto done;
			}

			/* don't allow copying/moving of any vTrash/vJunk folder nor maildir 'inbox' */
			if (strcmp(url_path, CAMEL_VTRASH_NAME) == 0
			    || strcmp(url_path, CAMEL_VJUNK_NAME) == 0
			    /* Dont allow drag from maildir 'inbox' */
			    || strcmp(url_path, ".") == 0) {
				camel_url_free(url);
				goto done;
			}
		}
		camel_url_free(url);

		/* Search Folders can only be dropped into other Search Folders */
		if (strncmp(src_uri, "vfolder:", 8) == 0) {
			/* TODO: not sure if this is legal, but it works, force move only for vfolders */
			context->suggested_action = GDK_ACTION_MOVE;

			if (uri && strncmp(uri, "vfolder:", 8) == 0) {
				GdkAtom xfolder;

				xfolder = drop_atoms[DND_DROP_TYPE_FOLDER];
				while (targets != NULL) {
					if (targets->data == (gpointer) xfolder) {
						atom = xfolder;
						goto done;
					}

					targets = targets->next;
				}
			}

			goto done;
		}
	}

	/* can't drag anything but a vfolder into a vfolder */
	if (uri && strncmp(uri, "vfolder:", 8) == 0)
		goto done;

	/* Now we either have a store or a normal folder */

	if (is_store) {
		GdkAtom xfolder;

		xfolder = drop_atoms[DND_DROP_TYPE_FOLDER];
		while (targets != NULL) {
			if (targets->data == (gpointer) xfolder) {
				atom = xfolder;
				goto done;
			}

			targets = targets->next;
		}
	} else {
		gint i;

		while (targets != NULL) {
			for (i = 0; i < NUM_DROP_TYPES; i++) {
				if (targets->data == (gpointer) drop_atoms[i]) {
					atom = drop_atoms[i];
					goto done;
				}
			}

			targets = targets->next;
		}
	}

 done:

	g_free (full_name);
	g_free (uri);

	return atom;
}

static gboolean
tree_drag_drop (GtkWidget *widget, GdkDragContext *context, gint x, gint y, guint time, EMFolderTree *emft)
{
	EMFolderTreePrivate *priv = emft->priv;
	GtkTreeViewColumn *column;
	GtkTreeView *tree_view;
	gint cell_x, cell_y;
	GtkTreePath *path;
	GdkAtom target;

	tree_view = GTK_TREE_VIEW (emft);

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

	if (!gtk_tree_view_get_path_at_pos (tree_view, x, y, &path, &column, &cell_x, &cell_y))
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
	EMFolderTreePrivate *priv = emft->priv;

	if (priv->drag_row) {
		gtk_tree_row_reference_free (priv->drag_row);
		priv->drag_row = NULL;
	}

	/* FIXME: undo anything done in drag-begin */
}

static void
tree_drag_leave (GtkWidget *widget, GdkDragContext *context, guint time, EMFolderTree *emft)
{
	EMFolderTreePrivate *priv = emft->priv;
	GtkTreeView *tree_view;

	tree_view = GTK_TREE_VIEW (emft);

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

	gtk_tree_view_set_drag_dest_row(tree_view, NULL, GTK_TREE_VIEW_DROP_BEFORE);
}

#define SCROLL_EDGE_SIZE 15

static gboolean
tree_autoscroll (EMFolderTree *emft)
{
	GtkAdjustment *vadjustment;
	GtkTreeView *tree_view;
	GdkRectangle rect;
	GdkWindow *window;
	gint offset, y;
	gfloat value;

	/* get the y pointer position relative to the treeview */
	tree_view = GTK_TREE_VIEW (emft);
	window = gtk_tree_view_get_bin_window (tree_view);
	gdk_window_get_pointer (window, NULL, &y, NULL);

	/* rect is in coorinates relative to the scrolled window relative to the treeview */
	gtk_tree_view_get_visible_rect (tree_view, &rect);

	/* move y into the same coordinate system as rect */
	y += rect.y;

	/* see if we are near the top edge */
	if ((offset = y - (rect.y + 2 * SCROLL_EDGE_SIZE)) > 0) {
		/* see if we are near the bottom edge */
		if ((offset = y - (rect.y + rect.height - 2 * SCROLL_EDGE_SIZE)) < 0)
			return TRUE;
	}

	vadjustment = gtk_tree_view_get_vadjustment (tree_view);

	value = CLAMP (vadjustment->value + offset, 0.0, vadjustment->upper - vadjustment->page_size);
	gtk_adjustment_set_value (vadjustment, value);

	return TRUE;
}

static gboolean
tree_autoexpand (EMFolderTree *emft)
{
	EMFolderTreePrivate *priv = emft->priv;
	GtkTreeView *tree_view;
	GtkTreePath *path;

	tree_view = GTK_TREE_VIEW (emft);
	path = gtk_tree_row_reference_get_path (priv->autoexpand_row);
	gtk_tree_view_expand_row (tree_view, path, FALSE);
	gtk_tree_path_free (path);

	return TRUE;
}

static gboolean
tree_drag_motion (GtkWidget *widget, GdkDragContext *context, gint x, gint y, guint time, EMFolderTree *emft)
{
	EMFolderTreePrivate *priv = emft->priv;
	GtkTreeViewDropPosition pos;
	GtkTreeView *tree_view;
	GtkTreeModel *model;
	GdkDragAction action = 0;
	GtkTreePath *path;
	GtkTreeIter iter;
	GdkAtom target;
	gint i;

	tree_view = GTK_TREE_VIEW (emft);
	model = gtk_tree_view_get_model (tree_view);

	if (!gtk_tree_view_get_dest_row_at_pos(tree_view, x, y, &path, &pos))
		return FALSE;

	if (priv->autoscroll_id == 0)
		priv->autoscroll_id = g_timeout_add (150, (GSourceFunc) tree_autoscroll, emft);

	gtk_tree_model_get_iter (model, &iter, path);

	if (gtk_tree_model_iter_has_child (model, &iter) && !gtk_tree_view_row_expanded (tree_view, path)) {
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
				case DND_DROP_TYPE_UID_LIST:
				case DND_DROP_TYPE_FOLDER:
					action = context->suggested_action;
					if (action == GDK_ACTION_COPY && (context->actions & GDK_ACTION_MOVE))
						action = GDK_ACTION_MOVE;
					gtk_tree_view_set_drag_dest_row(tree_view, path, GTK_TREE_VIEW_DROP_INTO_OR_AFTER);
					break;
				default:
					gtk_tree_view_set_drag_dest_row(tree_view, path, GTK_TREE_VIEW_DROP_INTO_OR_AFTER);
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
	EMFolderTreePrivate *priv;
	GtkTreeView *tree_view;
	static gint setup = 0;
	gint i;

	g_return_if_fail (EM_IS_FOLDER_TREE (emft));

	tree_view = GTK_TREE_VIEW (emft);

	priv = emft->priv;
	if (!setup) {
		for (i=0; i<NUM_DRAG_TYPES; i++)
			drag_atoms[i] = gdk_atom_intern(drag_types[i].target, FALSE);

		for (i=0; i<NUM_DROP_TYPES; i++)
			drop_atoms[i] = gdk_atom_intern(drop_types[i].target, FALSE);

		setup = 1;
	}

	gtk_drag_source_set((GtkWidget *)tree_view, GDK_BUTTON1_MASK, drag_types, NUM_DRAG_TYPES, GDK_ACTION_COPY | GDK_ACTION_MOVE);
	gtk_drag_dest_set((GtkWidget *)tree_view, GTK_DEST_DEFAULT_ALL, drop_types, NUM_DROP_TYPES, GDK_ACTION_COPY | GDK_ACTION_MOVE);

	g_signal_connect (tree_view, "drag-begin", G_CALLBACK (tree_drag_begin), emft);
	g_signal_connect (tree_view, "drag-data-delete", G_CALLBACK (tree_drag_data_delete), emft);
	g_signal_connect (tree_view, "drag-data-get", G_CALLBACK (tree_drag_data_get), emft);
	g_signal_connect (tree_view, "drag-data-received", G_CALLBACK (tree_drag_data_received), emft);
	g_signal_connect (tree_view, "drag-drop", G_CALLBACK (tree_drag_drop), emft);
	g_signal_connect (tree_view, "drag-end", G_CALLBACK (tree_drag_end), emft);
	g_signal_connect (tree_view, "drag-leave", G_CALLBACK (tree_drag_leave), emft);
	g_signal_connect (tree_view, "drag-motion", G_CALLBACK (tree_drag_motion), emft);
}

void
em_folder_tree_set_excluded (EMFolderTree *emft, guint32 flags)
{
	g_return_if_fail (EM_IS_FOLDER_TREE (emft));

	emft->priv->excluded = flags;
}

void
em_folder_tree_set_excluded_func (EMFolderTree *emft,
                                  EMFTExcludeFunc exclude,
                                  gpointer data)
{
	g_return_if_fail (EM_IS_FOLDER_TREE (emft));
	g_return_if_fail (exclude != NULL);

	emft->priv->excluded_func = exclude;
	emft->priv->excluded_data = data;
}

GList *
em_folder_tree_get_selected_uris (EMFolderTree *emft)
{
	GtkTreeSelection *selection;
	GtkTreeView *tree_view;
	GtkTreeModel *model;
	GList *list = NULL, *rows, *l;
	GSList *sl;

	tree_view = GTK_TREE_VIEW (emft);
	selection = gtk_tree_view_get_selection (tree_view);

	/* at first, add lost uris */
	for (sl = emft->priv->select_uris; sl; sl = g_slist_next(sl))
		list = g_list_append (list, g_strdup (((struct _selected_uri *)sl->data)->uri));

	rows = gtk_tree_selection_get_selected_rows(selection, &model);
	for (l=rows; l; l=g_list_next(l)) {
		GtkTreeIter iter;
		GtkTreePath *path = l->data;

		if (gtk_tree_model_get_iter(model, &iter, path)) {
			gchar *uri;

			gtk_tree_model_get(model, &iter, COL_STRING_URI, &uri, -1);
			list = g_list_prepend (list, uri);
		}
		gtk_tree_path_free(path);
	}
	g_list_free(rows);

	return g_list_reverse (list);
}

static void
get_selected_uris_path_iterate (GtkTreeModel *model, GtkTreePath *treepath, GtkTreeIter *iter, gpointer data)
{
	GList **list = (GList **) data;
	gchar *full_name;

	gtk_tree_model_get (model, iter, COL_STRING_FULL_NAME, &full_name, -1);
	*list = g_list_append (*list, full_name);
}

GList *
em_folder_tree_get_selected_paths (EMFolderTree *emft)
{
	GtkTreeSelection *selection;
	GtkTreeView *tree_view;
	GList *list = NULL;

	tree_view = GTK_TREE_VIEW (emft);
	selection = gtk_tree_view_get_selection (tree_view);

	gtk_tree_selection_selected_foreach (selection, get_selected_uris_path_iterate, &list);

	return list;
}

void
em_folder_tree_set_selected_list (EMFolderTree *emft, GList *list, gboolean expand_only)
{
	EMFolderTreePrivate *priv = emft->priv;
	gint id = 0;

	/* FIXME: need to remove any currently selected stuff? */
	if (!expand_only)
		emft_clear_selected_list(emft);

	for (;list;list = list->next) {
		struct _selected_uri *u = g_malloc0(sizeof(*u));
		CamelURL *url;
		CamelException ex = { 0 };

		u->uri = g_strdup(list->data);
		u->store = (CamelStore *)camel_session_get_service (session, u->uri, CAMEL_PROVIDER_STORE, &ex);
		camel_exception_clear(&ex);

		url = camel_url_new(u->uri, NULL);
		if (u->store == NULL || url == NULL) {
			if (!expand_only) {
				u->key = g_strdup_printf("dummy-%d:%s", id++, u->uri);
				g_hash_table_insert(priv->select_uris_table, u->key, u);
				priv->select_uris = g_slist_append(priv->select_uris, u);
			}
		} else {
			const gchar *path;
			gchar *expand_key, *end;
			EAccount *account;

			if (((CamelService *)u->store)->provider->url_flags & CAMEL_URL_FRAGMENT_IS_PATH)
				path = url->fragment;
			else
				path = url->path && url->path[0]=='/' ? url->path+1:url->path;
			if (path == NULL)
				path = "";

			/* This makes sure all our parents up to the root are expanded */
			/* FIXME: Why does the expanded state store this made up path rather than the euri? */
			if ( (account = mail_config_get_account_by_source_url(u->uri)) )
				expand_key = g_strdup_printf ("%s/%s", account->uid, path);
			else if (CAMEL_IS_VEE_STORE (u->store))
				expand_key = g_strdup_printf ("vfolder/%s", path);
			else
				expand_key = g_strdup_printf ("local/%s", path);

			if (!expand_only) {
				u->key = g_strdup(expand_key);

				g_hash_table_insert(priv->select_uris_table, u->key, u);
				priv->select_uris = g_slist_append(priv->select_uris, u);
			}

			end = strrchr(expand_key, '/');
			do {
				emft_expand_node(expand_key, emft);
				*end = 0;
				end = strrchr(expand_key, '/');
			} while (end);
			g_free(expand_key);
		}

		if (url)
			camel_url_free(url);
	}
}

#if 0
static void
dump_fi (CamelFolderInfo *fi, gint depth)
{
	gint i;

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

void
em_folder_tree_set_selected (EMFolderTree *emft,
                             const gchar *uri,
                             gboolean expand_only)
{
	GList *l = NULL;

	g_return_if_fail (EM_IS_FOLDER_TREE (emft));

	if (uri && uri[0])
		l = g_list_append(l, (gpointer)uri);

	em_folder_tree_set_selected_list(emft, l, expand_only);
	g_list_free(l);
}

void
em_folder_tree_select_next_path (EMFolderTree *emft, gboolean skip_read_folders)
{
	GtkTreeView *tree_view;
	GtkTreeSelection *selection;
	GtkTreeModel *model;
	GtkTreeIter iter, parent, child;
	GtkTreePath *current_path, *path = NULL;
	guint unread = 0;
	EMFolderTreePrivate *priv = emft->priv;

	g_return_if_fail (EM_IS_FOLDER_TREE (emft));

	tree_view = GTK_TREE_VIEW (emft);
	selection = gtk_tree_view_get_selection(tree_view);
	if (gtk_tree_selection_get_selected(selection, &model, &iter)) {

		current_path = gtk_tree_model_get_path (model, &iter);

		do {
		if (gtk_tree_model_iter_has_child (model, &iter)) {
			gtk_tree_model_iter_children (model, &child, &iter);
			path = gtk_tree_model_get_path (model, &child);
			iter = child;
		} else {
			while (1) {
				gboolean has_parent = gtk_tree_model_iter_parent (model, &parent, &iter);
				if (gtk_tree_model_iter_next (model, &iter)) {
					path = gtk_tree_model_get_path (model, &iter);
					break;
				} else {
					if (has_parent) {
						iter =  parent;
					} else {
						/* Reached end. Wrapup*/
						gtk_tree_model_get_iter_first (model, &iter);
						path = gtk_tree_model_get_path (model, &iter);
						break;
					}
				}
			}
		}
		gtk_tree_model_get (model, &iter, COL_UINT_UNREAD, &unread, -1);

		/* TODO : Flags here for better options */
		} while (skip_read_folders && unread <=0 && gtk_tree_path_compare (current_path, path));
	}

	if (path) {
		if (!gtk_tree_view_row_expanded (tree_view, path))
			gtk_tree_view_expand_to_path (tree_view, path);

		gtk_tree_selection_select_path(selection, path);

		if (!priv->cursor_set) {
			gtk_tree_view_set_cursor (tree_view, path, NULL, FALSE);
			priv->cursor_set = TRUE;
		}
		gtk_tree_view_scroll_to_cell (tree_view, path, NULL, TRUE, 0.5f, 0.0f);
	}
	return;
}

static GtkTreeIter
get_last_child (GtkTreeModel *model, GtkTreeIter *iter)
{
	GtkTreeIter *child = g_new0 (GtkTreeIter, 1);
	gboolean has_child = gtk_tree_model_iter_has_child (model, iter);

	if (gtk_tree_model_iter_next (model, iter)) {
		return get_last_child (model, iter);
	} else if (has_child) {
		/* Pick the last one */
		gint nchildren = gtk_tree_model_iter_n_children (model, iter);
		gtk_tree_model_iter_nth_child ( model, child, iter, nchildren-1);
		return get_last_child (model, child);
	}

	return *iter;
}

void
em_folder_tree_select_prev_path (EMFolderTree *emft, gboolean skip_read_folders)
{
	GtkTreeView *tree_view;
	GtkTreeSelection *selection;
	GtkTreeModel *model;
	GtkTreeIter iter, child;
	GtkTreePath *path = NULL, *current_path = NULL;
	guint unread = 0;
	EMFolderTreePrivate *priv = emft->priv;

	g_return_if_fail (EM_IS_FOLDER_TREE (emft));

	tree_view = GTK_TREE_VIEW (emft);
	selection = gtk_tree_view_get_selection (tree_view);

	if (gtk_tree_selection_get_selected(selection, &model, &iter)) {

		current_path = gtk_tree_model_get_path (model, &iter);
		do {
		path = gtk_tree_model_get_path (model, &iter);
		if (!gtk_tree_path_prev (path)) {
			gtk_tree_path_up (path);

			if (!gtk_tree_path_compare (gtk_tree_path_new_first (), path))
			{
				gtk_tree_model_get_iter_first (model, &iter);
				iter = get_last_child (model,&iter);
				path = gtk_tree_model_get_path (model, &iter);
			}
		} else {
			gtk_tree_model_get_iter (model, &iter, path);
			if (gtk_tree_model_iter_has_child (model, &iter)) {
				gint nchildren = gtk_tree_model_iter_n_children (model, &iter);
				gtk_tree_model_iter_nth_child ( model, &child, &iter, nchildren-1);
				path = gtk_tree_model_get_path (model, &child);
			}
		}

		/* TODO : Flags here for better options */
		gtk_tree_model_get_iter_from_string (model, &iter, gtk_tree_path_to_string (path) );

		gtk_tree_model_get (model, &iter, COL_UINT_UNREAD, &unread, -1);

		} while (skip_read_folders && unread <=0 && gtk_tree_path_compare (current_path, path));
	}

	if (path) {
		if (!gtk_tree_view_row_expanded (tree_view, path)) {
			gtk_tree_view_expand_to_path (tree_view, path);
		}

		gtk_tree_selection_select_path(selection, path);

		if (!priv->cursor_set) {
			gtk_tree_view_set_cursor (tree_view, path, NULL, FALSE);
			priv->cursor_set = TRUE;
		}
		gtk_tree_view_scroll_to_cell (tree_view, path, NULL, TRUE, 0.5f, 0.0f);
	}
	return;
}

gchar *
em_folder_tree_get_selected_uri (EMFolderTree *emft)
{
	GtkTreeView *tree_view;
	GtkTreeSelection *selection;
	GtkTreeModel *model;
	GtkTreeIter iter;
	gchar *uri = NULL;

	g_return_val_if_fail (EM_IS_FOLDER_TREE (emft), NULL);

	tree_view = GTK_TREE_VIEW (emft);
	selection = gtk_tree_view_get_selection (tree_view);

	if (!gtk_tree_selection_get_selected (selection, &model, &iter))
		return NULL;

	gtk_tree_model_get (model, &iter, COL_STRING_URI, &uri, -1);

	return uri;
}

gchar *
em_folder_tree_get_selected_path (EMFolderTree *emft)
{
	GtkTreeView *tree_view;
	GtkTreeSelection *selection;
	GtkTreeModel *model;
	GtkTreeIter iter;
	gchar *name = NULL;

	g_return_val_if_fail (EM_IS_FOLDER_TREE (emft), NULL);

	tree_view = GTK_TREE_VIEW (emft);
	selection = gtk_tree_view_get_selection (tree_view);

	if (!gtk_tree_selection_get_selected (selection, &model, &iter))
		return NULL;

	gtk_tree_model_get (model, &iter, COL_STRING_FULL_NAME, &name, -1);

	return name;
}

CamelFolder *
em_folder_tree_get_selected_folder (EMFolderTree *emft)
{
	GtkTreeView *tree_view;
	GtkTreeSelection *selection;
	GtkTreeModel *model;
	GtkTreeIter iter;
	gchar *full_name = NULL;
	CamelException ex;
	CamelStore *store = NULL;
	CamelFolder *folder = NULL;

	g_return_val_if_fail (EM_IS_FOLDER_TREE (emft), NULL);

	camel_exception_init (&ex);

	tree_view = GTK_TREE_VIEW (emft);
	selection = gtk_tree_view_get_selection (tree_view);

	if (gtk_tree_selection_get_selected(selection, &model, &iter))
		gtk_tree_model_get (model, &iter, COL_POINTER_CAMEL_STORE, &store,
				    COL_STRING_FULL_NAME, &full_name, -1);

	if (store && full_name)
		folder = camel_store_get_folder (store, full_name, CAMEL_STORE_FOLDER_INFO_FAST, &ex);

	camel_exception_clear (&ex);

	return folder;
}

CamelFolderInfo *
em_folder_tree_get_selected_folder_info (EMFolderTree *emft)
{
	GtkTreeView *tree_view;
	GtkTreeSelection *selection;
	GtkTreeModel *model;
	GtkTreeIter iter;
	gchar *full_name = NULL, *name = NULL, *uri = NULL;
	CamelException ex;
	CamelStore *store = NULL;
	CamelFolderInfo *fi = NULL;

	g_return_val_if_fail (EM_IS_FOLDER_TREE (emft), NULL);

	camel_exception_init (&ex);

	tree_view = GTK_TREE_VIEW (emft);
	selection = gtk_tree_view_get_selection (tree_view);

	if (gtk_tree_selection_get_selected(selection, &model, &iter))
		gtk_tree_model_get (model, &iter, COL_POINTER_CAMEL_STORE, &store,
				    COL_STRING_FULL_NAME, &full_name,
				    COL_STRING_DISPLAY_NAME, &name,
				    COL_STRING_URI, &uri, -1);

	fi = camel_folder_info_new ();
	fi->full_name = g_strdup (full_name);
	fi->uri = g_strdup (uri);
	fi->name = g_strdup (name);

	d(g_print ("em_folder_tree_get_selected_folder_info: fi->full_name=[%s], fi->uri=[%s], fi->name=[%s]\n",
		   fi->full_name, fi->uri, fi->name));
	d(g_print ("em_folder_tree_get_selected_folder_info: full_name=[%s], uri=[%s], name=[%s]\n",
		   full_name, uri, name));

	if (!fi->full_name)
		goto done;

	g_free (fi->name);
        if (!g_ascii_strcasecmp (fi->full_name, "INBOX"))
                fi->name = g_strdup (_("Inbox"));
        else
                fi->name = g_strdup (name);
 done:
	return fi;
}

void
em_folder_tree_set_skip_double_click (EMFolderTree *emft, gboolean skip)
{
	emft->priv->skip_double_click = skip;
}
