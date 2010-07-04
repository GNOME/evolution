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
 *		Michael Zucchi <notzed@ximian.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <string.h>

#include "mail-tools.h"
#include "mail-ops.h"
#include "mail-mt.h"
/*#include "mail-folder-cache.h"*/

#include "e-util/e-util.h"
#include "e-util/e-account-utils.h"
#include "e-util/e-util-private.h"

#include "em-folder-utils.h"
#include "em-subscribe-editor.h"

#include "mail-config.h"

#include <glib/gi18n.h>

#define d(x)

enum {
	COL_SUBSCRIBED = 0, /* G_TYPE_BOOLEAN */
	COL_NAME,           /* G_TYPE_STRING  */
	COL_INFO_NODE,      /* G_TYPE_POINTER */
	COL_CAN_SELECT,     /* G_TYPE_BOOLEAN */
	COL_ICON_NAME,      /* G_TYPE_STRING  */
	N_COLUMNS
};

typedef struct _EMSubscribeEditor EMSubscribeEditor;
struct _EMSubscribeEditor {
	GQueue stores;

	gint busy;
	guint busy_id;

	gboolean is_filtering; /* whether filtering is active */
	guint refilter_id;     /* source ID of a refilter action, after change in the filter edit */

	struct _EMSubscribe *current; /* the current one, if any */

	GtkDialog *dialog;
	GtkWidget *vbox;	/* where new stores are added */
	GtkWidget *combobox;
	GtkWidget *none_selected; /* 'please select a xxx' message */
	GtkWidget *progress;
	GtkWidget *filter_entry; /* when not empty, then it's filtering */
	GtkWidget *expand_button;
	GtkWidget *collapse_button;
	GtkWidget *refresh_button;
};

typedef struct _EMSubscribe EMSubscribe;
struct _EMSubscribe {
	gint ref_count;
	gint cancel;
	gint seq;		/* upped every time we refresh */

	struct _EMSubscribeEditor *editor; /* parent object*/

	gchar *store_uri;
	gint store_id;		/* looking up a store */

	CamelStore *store;
	GHashTable *folders;

	GtkWidget *widget;	/* widget to show for this store */
	GtkTreeView *tree;	/* tree, if we have it */
	GtkTreeModel *tree_store; /* a tree store, used when not filtering */
	GtkTreeModel *list_store; /* list store, used when filtering */
	GSList *all_selectable; /* list of selectable info's, stored in the tree_store, in reverse order */

	GSList *tree_expanded_paths; /* list of expanded paths in the tree model */

	/* list of all returns from get_folder_info, accessed by other structures */
	GSList *info_list;

	gint pending_id;

	/* queue of pending UN/SUBSCRIBEs, EMsg's */
	gint subscribe_id;
	GQueue subscribe;

	/* working variables at runtime */
	gint selected_count;
	gint selected_subscribed_count;
	guint subscribed_state:1; /* for setting the selection*/
};

typedef struct _EMSubscribeNode EMSubscribeNode;
struct _EMSubscribeNode {
	CamelFolderInfo *info;
	GtkTreePath *path;
};

typedef struct _MailMsgListNode MailMsgListNode;
struct _MailMsgListNode {
	MailMsg *msg;
};

static void sub_editor_busy(EMSubscribeEditor *se, gint dir);
static gint sub_queue_fill_level(EMSubscribe *sub, EMSubscribeNode *node);
static void sub_selection_changed(GtkTreeSelection *selection, EMSubscribe *sub);

static gboolean
test_contains (const gchar *where, const gchar *what)
{
	gunichar c;
	const gchar *at = what;

	if (!what || !where)
		return TRUE;

	while (c = g_utf8_get_char_validated (where, -1), c != 0 && c != (gunichar) -1 && c != (gunichar) -2) {
		if (g_utf8_get_char (at) == g_unichar_tolower (c)) {
			at = g_utf8_next_char (at);
			if (!at || !*at)
				return TRUE;
		} else {
			at = what;
		}
		where = g_utf8_next_char (where);
	}

	return FALSE;
}

static void
update_filtering_column (EMSubscribeEditor *se, struct _EMSubscribe *sub)
{
	gchar *text;
	GtkTreeIter iter;
	GtkTreeModel *list_store;
	GSList *l;

	g_return_if_fail (se != NULL);
	g_return_if_fail (sub != NULL);
	g_return_if_fail (g_utf8_validate (gtk_entry_get_text (GTK_ENTRY (se->filter_entry)), -1, NULL));

	if (!sub->tree)
		return;

	if (gtk_tree_view_get_model (sub->tree) == sub->list_store)
		gtk_tree_view_set_model (sub->tree, NULL);

	text = g_utf8_strdown (gtk_entry_get_text (GTK_ENTRY (se->filter_entry)), -1);
	list_store = sub->list_store;

	gtk_list_store_clear (GTK_LIST_STORE (list_store));
	for (l = sub->all_selectable; l; l = l->next) {
		EMSubscribeNode *node = l->data;
		gboolean bl;

		if (!node || !node->path || !node->info)
			continue;

		bl = (!text || !*text || (node && node->info && node->info->full_name && test_contains (node->info->full_name, text)));
		if (!bl)
			continue;

		gtk_list_store_prepend ((GtkListStore *)list_store, &iter);
		gtk_list_store_set (GTK_LIST_STORE (list_store), &iter,
			COL_SUBSCRIBED, (node->info->flags & CAMEL_FOLDER_SUBSCRIBED) != 0,
			COL_NAME, node->info->full_name,
			COL_INFO_NODE, node,
			COL_CAN_SELECT, TRUE,
			COL_ICON_NAME, em_folder_utils_get_icon_name (node->info->flags),
			-1);
	}

	g_free (text);

	if (!gtk_tree_view_get_model (sub->tree)) {
		gtk_tree_view_set_model (sub->tree, sub->list_store);
		gtk_tree_view_set_search_column (sub->tree, COL_NAME);
	}
}

static void
sub_node_free(EMSubscribeNode *node)
{
	d(printf("sub node free '%s'\n", node->info?node->info->full_name:"<unknown>"));
	if (node->path)
		gtk_tree_path_free(node->path);
	g_free(node);
}

static void
sub_ref(EMSubscribe *sub)
{
	sub->ref_count++;
}

static void
sub_unref(EMSubscribe *sub)
{
	GSList *l;

	sub->ref_count--;
	if (sub->ref_count == 0) {
		d(printf("subscribe object finalised\n"));
		/* we dont have to delete the "subscribe" task list, as it must be empty,
		   otherwise we wouldn't be unreffed (intentional circular reference) */
		if (sub->tree_store)
			g_object_unref (sub->tree_store);
		if (sub->list_store)
			g_object_unref (sub->list_store);
		if (sub->folders)
			g_hash_table_destroy(sub->folders);
		g_slist_free (sub->all_selectable);
		g_slist_foreach (sub->tree_expanded_paths, (GFunc) gtk_tree_path_free, NULL);
		g_slist_free (sub->tree_expanded_paths);
		l = sub->info_list;
		while (l) {
			GSList *n = l->next;

			camel_store_free_folder_info(sub->store, (CamelFolderInfo *)l->data);
			g_slist_free_1(l);
			l = n;
		}
		if (sub->store)
			g_object_unref (sub->store);
		g_free(sub->store_uri);
		g_free(sub);
	}
}

/* ** Subscribe folder operation **************************************** */

struct _zsubscribe_msg {
	MailMsg base;

	EMSubscribe *sub;
	EMSubscribeNode *node;
	gint subscribe;
	gchar *path;
};

static void
sub_folder_exec (struct _zsubscribe_msg *m)
{
	if (m->subscribe)
		camel_store_subscribe_folder (
			m->sub->store, m->node->info->full_name,
			&m->base.error);
	else
		camel_store_unsubscribe_folder (
			m->sub->store, m->node->info->full_name,
			&m->base.error);
}

static void
sub_folder_done (struct _zsubscribe_msg *m)
{
	struct _zsubscribe_msg *next;
	GtkTreeIter iter;
	GtkTreeModel *model;
	EMSubscribeNode *node;
	gboolean subscribed, issub;
	MailMsgListNode *msgListNode;

	m->sub->subscribe_id = -1;
	if (m->sub->cancel)
		return;

	if (m->base.error == NULL) {
		if (m->subscribe)
			m->node->info->flags |= CAMEL_FOLDER_SUBSCRIBED;
		else
			m->node->info->flags &= ~CAMEL_FOLDER_SUBSCRIBED;
	}

	/* make sure the tree view matches the correct state */
	/* all actions are done on tree store, synced to list store */
	model = m->sub->tree_store;
	if (gtk_tree_model_get_iter_from_string(model, &iter, m->path)) {
		issub = (m->node->info->flags & CAMEL_FOLDER_SUBSCRIBED) != 0;
		gtk_tree_model_get(model, &iter, COL_SUBSCRIBED, &subscribed, COL_INFO_NODE, &node, -1);
		if (node == m->node) {
			gtk_tree_store_set ((GtkTreeStore *)model, &iter, COL_SUBSCRIBED, issub, -1);
		} else {
			d(printf("node mismatch, or subscribe state changed failed\n"));
		}
	}

	/* queue any further ones, or if out, update the ui */
	msgListNode = g_queue_pop_head (&m->sub->subscribe);
	if (msgListNode) {
		next = (struct _zsubscribe_msg *) msgListNode->msg;
		/* Free the memory of the MailMsgListNode which won't be used anymore. */
		g_free(msgListNode);
		next->sub->subscribe_id = next->base.seq;
                mail_msg_unordered_push (next);
	} else {
		/* should it go off the model instead? */
		sub_selection_changed(gtk_tree_view_get_selection(m->sub->tree), m->sub);
	}
}

static void
sub_folder_free (struct _zsubscribe_msg *m)
{
	g_free(m->path);
	sub_unref(m->sub);
}

static MailMsgInfo sub_subscribe_folder_info = {
	sizeof (struct _zsubscribe_msg),
	(MailMsgDescFunc) NULL,
	(MailMsgExecFunc) sub_folder_exec,
	(MailMsgDoneFunc) sub_folder_done,
	(MailMsgFreeFunc) sub_folder_free
};

/* spath is tree path in string form */
static gint
sub_subscribe_folder (EMSubscribe *sub, EMSubscribeNode *node, gint state, const gchar *spath)
{
	struct _zsubscribe_msg *m;
	MailMsgListNode *msgListNode;
	gint id;

	m = mail_msg_new (&sub_subscribe_folder_info);
	m->sub = sub;
	sub_ref(sub);
	m->node = node;
	m->subscribe = state;
	m->path = g_strdup(spath);

	id = m->base.seq;
	if (sub->subscribe_id == -1) {
		sub->subscribe_id = id;
		d(printf("running subscribe folder '%s'\n", spath));
		mail_msg_unordered_push (m);
	} else {
		msgListNode = g_malloc0(sizeof(MailMsgListNode));
		msgListNode->msg = (MailMsg *) m;
		d(printf("queueing subscribe folder '%s'\n", spath));
		g_queue_push_tail (&sub->subscribe, msgListNode);
	}

	return id;
}

/* ********************************************************************** */
static void
sub_fill_levels (EMSubscribe *sub, CamelFolderInfo *info, GtkTreeIter *parent)
{
	CamelFolderInfo *fi;
	GtkTreeStore *treestore;
	GtkTreeIter iter;
	EMSubscribeNode *node;

	treestore = (GtkTreeStore *) sub->tree_store;
	g_return_if_fail (treestore != NULL);

	/* first, fill a level up */
	fi = info;
	while (fi) {
		gboolean known = FALSE;

		if ((node = g_hash_table_lookup(sub->folders, fi->full_name)) == NULL) {
			gboolean state;

			gtk_tree_store_append(treestore, &iter, parent);
			node = g_malloc0(sizeof(*node));
			node->info = fi;
			state = (fi->flags & CAMEL_FOLDER_SUBSCRIBED) != 0;
			gtk_tree_store_set (treestore, &iter,
				COL_SUBSCRIBED, state,
				COL_NAME, fi->name,
				COL_INFO_NODE, node,
				COL_CAN_SELECT, (fi->flags & CAMEL_FOLDER_NOSELECT) == 0,
				COL_ICON_NAME, em_folder_utils_get_icon_name (fi->flags),
				-1);
			if ((fi->flags & CAMEL_FOLDER_NOSELECT) == 0)
				sub->all_selectable = g_slist_prepend (sub->all_selectable, node);
			if (state) {
				GtkTreePath *path = gtk_tree_model_get_path ((GtkTreeModel *)treestore, &iter);
				gtk_tree_view_expand_to_path (sub->tree, path);
				gtk_tree_path_free (path);
			}
			if ((fi->flags & CAMEL_FOLDER_NOINFERIORS) == 0)
				node->path = gtk_tree_model_get_path((GtkTreeModel *)treestore, &iter);
			g_hash_table_insert(sub->folders, fi->full_name, node);
		} else if (node->path) {
			gtk_tree_model_get_iter (GTK_TREE_MODEL (treestore), &iter, node->path);
			known = TRUE;
		}

		d(printf("flags & CAMEL_FOLDER_NOCHILDREN=%d, f & CAMEL_FOLDER_NOINFERIORS=%d\t fi->full_name=[%s], node->path=%p\n",
			 fi->flags & CAMEL_FOLDER_NOCHILDREN, fi->flags & CAMEL_FOLDER_NOINFERIORS, fi->full_name,
			 node->path));

		if ((fi->flags & CAMEL_FOLDER_NOINFERIORS) == 0
		    && node->path) {
			/* save time, if we have any children alread, dont re-scan */
			if (fi->child) {
				d(printf("scanning child '%s'\n", fi->child->full_name));
				sub_fill_levels (sub, fi->child, &iter);
			} else if (!(fi->flags & CAMEL_FOLDER_NOCHILDREN)) {
				d(printf("flags: CAMEL_FOLDER_NOCHILDREN is not set '%s', known:%d\n", fi->full_name, known?1:0));
			}
		} else {
			d(printf("%s:%s: fi->flags & CAMEL_FOLDER_NOINFERIORS=%d\t node->path=[%p]\n",
				 G_STRLOC, G_STRFUNC, fi->flags & CAMEL_FOLDER_NOINFERIORS,
				 node->path));
		}

		fi = fi->next;
	}
}

/* async query of folderinfo */

struct _emse_folderinfo_msg {
	MailMsg base;

	gint seq;

	EMSubscribe *sub;
	EMSubscribeNode *node;
	CamelFolderInfo *info;
};

static void
sub_folderinfo_exec (struct _emse_folderinfo_msg *m)
{
	if (m->seq == m->sub->seq) {
		camel_operation_register (m->base.cancel);
		/* get the full folder tree for search ability */
		m->info = camel_store_get_folder_info (
			m->sub->store, NULL,
			CAMEL_STORE_FOLDER_INFO_NO_VIRTUAL |
			CAMEL_STORE_FOLDER_INFO_SUBSCRIPTION_LIST |
			CAMEL_STORE_FOLDER_INFO_RECURSIVE,
			&m->base.error);
		camel_operation_unregister (m->base.cancel);
	}
}

static void
sub_folderinfo_done (struct _emse_folderinfo_msg *m)
{
	m->sub->pending_id = -1;
	if (m->sub->cancel || m->seq != m->sub->seq)
		return;

	if (m->base.error != NULL)
		g_warning (
			"Error getting folder info from store: %s",
			m->base.error->message);

	if (m->info) {
		if (m->node) {
			GtkTreeIter iter;

			gtk_tree_model_get_iter (m->sub->tree_store, &iter, m->node->path);
			sub_fill_levels (m->sub, m->info, &iter);
		} else {
			sub_fill_levels (m->sub, m->info, NULL);
		}

		if (m->sub->editor->is_filtering)
			update_filtering_column (m->sub->editor, m->sub);
	}
}

static void
sub_folderinfo_free (struct _emse_folderinfo_msg *m)
{
	if (m->info)
		m->sub->info_list = g_slist_prepend(m->sub->info_list, m->info);

	if (!m->sub->cancel)
		sub_editor_busy(m->sub->editor, -1);

	/* Now we just load the children on demand, so set the
	   expand state to true if m->node is not NULL
	*/
	if (m->node)
		gtk_tree_view_expand_row(m->sub->tree, m->node->path, FALSE);

	sub_unref(m->sub);
}

static MailMsgInfo sub_folderinfo_info = {
	sizeof (struct _emse_folderinfo_msg),
	(MailMsgDescFunc) NULL,
	(MailMsgExecFunc) sub_folderinfo_exec,
	(MailMsgDoneFunc) sub_folderinfo_done,
	(MailMsgFreeFunc) sub_folderinfo_free
};

static gint
sub_queue_fill_level(EMSubscribe *sub, EMSubscribeNode *node)
{
	struct _emse_folderinfo_msg *m;
	gint id;

	d(printf("%s:%s: Starting get folderinfo of '%s'\n", G_STRLOC, G_STRFUNC,
		 node?node->info->full_name:"<root>"));

	m = mail_msg_new (&sub_folderinfo_info);
	sub_ref(sub);
	m->sub = sub;
	m->node = node;
	m->seq = sub->seq;

	sub->pending_id = m->base.seq;

	sub_editor_busy(sub->editor, 1);

	id = m->base.seq;

	mail_msg_unordered_push (m);
	return id;
}

static void
update_buttons_sesitivity (EMSubscribeEditor *se)
{
	gboolean is_tree_model;

	if (!se)
		return;

	is_tree_model = se->current && se->current->tree && !se->is_filtering;

	gtk_widget_set_sensitive (se->expand_button, is_tree_model);
	gtk_widget_set_sensitive (se->collapse_button, is_tree_model);
	gtk_widget_set_sensitive (se->refresh_button, se->current && se->current->tree);
}

/* ********************************************************************** */

/* (un) subscribes the current selection */

static void
sub_subscribe_toggled(GtkCellRendererToggle *render, const gchar *spath, EMSubscribe *sub)
{
	GtkTreeIter iter;
	GtkTreeModel *model = gtk_tree_view_get_model(sub->tree);
	EMSubscribeNode *node;
	gboolean subscribed;

	d(printf("subscribe toggled?\n"));

	if (gtk_tree_model_get_iter_from_string(model, &iter, spath)) {
		gchar *free_path;

		gtk_tree_model_get(model, &iter, COL_SUBSCRIBED, &subscribed, COL_INFO_NODE, &node, -1);
		g_return_if_fail (node != NULL);
		subscribed = !subscribed;
		d(printf("new state is %s\n", subscribed?"subscribed":"not subscribed"));
		if (GTK_IS_TREE_STORE (model)) {
			gtk_tree_store_set ((GtkTreeStore *)model, &iter, COL_SUBSCRIBED, subscribed, -1);
		} else {
			/* it's a list store, convert spath to tree path and update tree store value */
			gtk_list_store_set (GTK_LIST_STORE (model), &iter, COL_SUBSCRIBED, subscribed, -1);
			if (gtk_tree_model_get_iter (sub->tree_store, &iter, node->path)) {
				gtk_tree_store_set ((GtkTreeStore *)sub->tree_store, &iter, COL_SUBSCRIBED, subscribed, -1);
			}
			free_path = gtk_tree_path_to_string (node->path);
			if (subscribed)
				sub->tree_expanded_paths = g_slist_prepend (sub->tree_expanded_paths, gtk_tree_path_copy (node->path));
			spath = free_path;
		}

		sub_subscribe_folder(sub, node, subscribed, spath);
	}
}

static void sub_do_changed(GtkTreeModel *model, GtkTreePath *path, GtkTreeIter *iter, gpointer data)
{
	EMSubscribe *sub = data;
	EMSubscribeNode *node;
	gboolean subscribed;

	gtk_tree_model_get(model, iter, COL_SUBSCRIBED, &subscribed, COL_INFO_NODE, &node, -1);

	if (subscribed)
		sub->selected_subscribed_count++;
	sub->selected_count++;
}

static void
sub_selection_changed(GtkTreeSelection *selection, EMSubscribe *sub)
{
	sub->selected_count = 0;
	sub->selected_subscribed_count = 0;
	gtk_tree_selection_selected_foreach(selection, sub_do_changed, sub);
}

/* double-clicking causes a node item to be evaluated directly */
static void sub_row_activated(GtkTreeView *tree, GtkTreePath *path, GtkTreeViewColumn *col, EMSubscribe *sub)
{
	if (!gtk_tree_view_row_expanded(tree, path))
		gtk_tree_view_expand_row(tree, path, FALSE);
	else
		gtk_tree_view_collapse_row(tree, path);

}

static void
sub_destroy(GtkWidget *w, EMSubscribe *sub)
{
	struct _zsubscribe_msg *m;
	MailMsgListNode *msgListNode;

	d(printf("subscribe closed\n"));
	sub->cancel = TRUE;

	if (sub->pending_id != -1)
		mail_msg_cancel(sub->pending_id);

	if (sub->subscribe_id != -1)
		mail_msg_cancel(sub->subscribe_id);

	while ((msgListNode = g_queue_pop_head (&sub->subscribe)) != NULL) {
		m = (struct _zsubscribe_msg *) msgListNode->msg;
		/* Free the memory of MailMsgListNode which won't be used anymore. */
		g_free(msgListNode);
		mail_msg_unref(m);
	}

	sub_unref(sub);
}

static EMSubscribe *
subscribe_new(EMSubscribeEditor *se, const gchar *uri)
{
	EMSubscribe *sub;

	sub = g_malloc0(sizeof(*sub));
	sub->store_uri = g_strdup(uri);
	sub->editor = se;
	sub->ref_count = 1;
	sub->pending_id = -1;
	sub->subscribe_id = -1;
	g_queue_init (&sub->subscribe);
	sub->store_id = -1;

	return sub;
}

static void
subscribe_set_store(EMSubscribe *sub, CamelStore *store)
{
	if (store == NULL || !camel_store_supports_subscriptions(store)) {
		GtkWidget *w = gtk_label_new(_("This store does not support subscriptions, or they are not enabled."));

		gtk_label_set_line_wrap((GtkLabel *)w, TRUE);
		sub->widget = gtk_viewport_new(NULL, NULL);
		gtk_viewport_set_shadow_type((GtkViewport *)sub->widget, GTK_SHADOW_IN);
		gtk_container_add((GtkContainer *)sub->widget, w);
		gtk_widget_show(w);
		gtk_widget_show(sub->widget);
	} else {
		GtkTreeSelection *selection;
		GtkTreeViewColumn *column;
		GtkCellRenderer *renderer;

		sub->all_selectable = NULL;
		sub->tree_expanded_paths = NULL;
		sub->store = store;
		g_object_ref (store);
		sub->folders = g_hash_table_new_full (
			g_str_hash, g_str_equal,
			(GDestroyNotify) NULL,
			(GDestroyNotify) sub_node_free);

		sub->tree_store = (GtkTreeModel *) gtk_tree_store_new (N_COLUMNS,
			G_TYPE_BOOLEAN, /* COL_SUBSCRIBED */
			G_TYPE_STRING,  /* COL_NAME       */
			G_TYPE_POINTER, /* COL_INFO_NODE  */
			G_TYPE_BOOLEAN, /* COL_CAN_SELECT */
			G_TYPE_STRING   /* COL_ICON_NAME  */
		);
		g_object_ref_sink (sub->tree_store);

		sub->list_store = (GtkTreeModel *) gtk_list_store_new (N_COLUMNS,
			G_TYPE_BOOLEAN, /* COL_SUBSCRIBED */
			G_TYPE_STRING,  /* COL_NAME       */
			G_TYPE_POINTER, /* COL_INFO_NODE  */
			G_TYPE_BOOLEAN, /* COL_CAN_SELECT */
			G_TYPE_STRING   /* COL_ICON_NAME  */
		);
		g_object_ref_sink (sub->list_store);

		sub->tree = (GtkTreeView *) gtk_tree_view_new_with_model (sub->editor->is_filtering ? sub->list_store : sub->tree_store);
		gtk_widget_show ((GtkWidget *)sub->tree);

		sub->widget = gtk_scrolled_window_new (NULL, NULL);
		gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (sub->widget), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
		gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (sub->widget), GTK_SHADOW_IN);
		gtk_container_add((GtkContainer *)sub->widget, (GtkWidget *)sub->tree);
		gtk_widget_show(sub->widget);

		renderer = gtk_cell_renderer_toggle_new ();
		g_object_set(renderer, "activatable", TRUE, NULL);
		gtk_tree_view_insert_column_with_attributes (sub->tree, -1, _("Subscribed"), renderer, "active", COL_SUBSCRIBED, "visible", COL_CAN_SELECT, NULL);
		g_signal_connect(renderer, "toggled", G_CALLBACK(sub_subscribe_toggled), sub);

		column = gtk_tree_view_column_new ();
		gtk_tree_view_column_set_title (column, _("Folder"));
		gtk_tree_view_append_column (sub->tree, column);

		renderer = gtk_cell_renderer_pixbuf_new ();
		gtk_tree_view_column_pack_start (column, renderer, FALSE);
		gtk_tree_view_column_add_attribute (
			column, renderer, "icon-name", COL_ICON_NAME);

		renderer = gtk_cell_renderer_text_new ();
		gtk_tree_view_column_pack_start (column, renderer, TRUE);
		gtk_tree_view_column_add_attribute (
			column, renderer, "text", COL_NAME);
		gtk_tree_view_set_expander_column(sub->tree, gtk_tree_view_get_column(sub->tree, 1));

		selection = gtk_tree_view_get_selection (sub->tree);
		gtk_tree_selection_set_mode (selection, GTK_SELECTION_MULTIPLE);
		gtk_tree_view_set_headers_visible (sub->tree, FALSE);

		gtk_tree_view_set_search_column (sub->tree, COL_NAME);
		gtk_tree_view_set_enable_search (sub->tree, TRUE);

		g_signal_connect(sub->tree, "row-activated", G_CALLBACK(sub_row_activated), sub);
		g_signal_connect(sub->tree, "destroy", G_CALLBACK(sub_destroy), sub);

		sub_selection_changed(selection, sub);
		g_signal_connect(selection, "changed", G_CALLBACK(sub_selection_changed), sub);

		sub_queue_fill_level(sub, NULL);

		update_buttons_sesitivity (sub->editor);
	}

	gtk_box_pack_start((GtkBox *)sub->editor->vbox, sub->widget, TRUE, TRUE, 0);
}

static void
sub_editor_destroy(GtkWidget *w, EMSubscribeEditor *se)
{
	/* need to clean out pending store opens */
	d(printf("editor destroyed, freeing editor\n"));
	if (se->busy_id)
		g_source_remove(se->busy_id);
	if (se->refilter_id != 0)
		g_source_remove (se->refilter_id);
	se->refilter_id = 0;

	g_free(se);
}

static void
sub_editor_close(GtkWidget *w, EMSubscribeEditor *se)
{
	gtk_widget_destroy((GtkWidget *)se->dialog);
}

static void
sub_editor_refresh(GtkWidget *w, EMSubscribeEditor *se)
{
	EMSubscribe *sub = se->current;
	GSList *l;

	d(printf("sub editor refresh?\n"));
	if (sub == NULL || sub->store == NULL)
		return;

	sub->seq++;

	/* drop any currently pending */
	if (sub->pending_id != -1) {
		mail_msg_cancel(sub->pending_id);
		mail_msg_wait(sub->pending_id);
	}

	g_slist_free (sub->all_selectable);
	sub->all_selectable = NULL;

	g_slist_foreach (sub->tree_expanded_paths, (GFunc) gtk_tree_path_free, NULL);
	g_slist_free (sub->tree_expanded_paths);
	sub->tree_expanded_paths = NULL;

	gtk_tree_store_clear ((GtkTreeStore *)sub->tree_store);
	gtk_list_store_clear ((GtkListStore *)sub->list_store);

	if (sub->folders)
		g_hash_table_destroy(sub->folders);
	sub->folders = g_hash_table_new_full (
		g_str_hash, g_str_equal,
		(GDestroyNotify) NULL,
		(GDestroyNotify) sub_node_free);

	l = sub->info_list;
	sub->info_list = NULL;
	while (l) {
		GSList *n = l->next;

		camel_store_free_folder_info(sub->store, (CamelFolderInfo *)l->data);
		g_slist_free_1(l);
		l = n;
	}

	sub_queue_fill_level(sub, NULL);
}

static void
sub_editor_got_store(gchar *uri, CamelStore *store, gpointer data)
{
	struct _EMSubscribe *sub = data;

	if (!sub->cancel)
		subscribe_set_store(sub, store);
	sub_unref(sub);
}

static void
sub_editor_combobox_changed (GtkWidget *w, EMSubscribeEditor *se)
{
	gint i, n;
	GList *link;

	d(printf("combobox changed\n"));

	i = 0;
	n = gtk_combo_box_get_active (GTK_COMBO_BOX (se->combobox));
	if (n != -1) {
		GtkTreeIter iter;
		GtkTreeModel *model;

		gtk_widget_hide (se->none_selected);

		model = gtk_combo_box_get_model (GTK_COMBO_BOX (se->combobox));
		if (gtk_tree_model_get_iter_first (model, &iter)) {
			gboolean is_account = TRUE;

			gtk_tree_model_get (model, &iter, 1, &is_account, -1);

			if (!is_account && n > 0) {
				/* the first node it not an account node, it's the notice
				   about "select account please", thus remove it completely */
				gtk_list_store_remove (GTK_LIST_STORE (model), &iter);
				n--;
			} else if (!is_account) {
				gtk_widget_show (se->none_selected);
				i++;
			}
		}
	}

	if (se->refilter_id != 0) {
		g_source_remove (se->refilter_id);
		se->refilter_id = 0;
	}

	se->current = NULL;
	link = g_queue_peek_head_link (&se->stores);
	while (link != NULL) {
		struct _EMSubscribe *sub = link->data;

		if (i == n) {
			se->current = sub;
			if (sub->widget) {
				gtk_widget_show(sub->widget);
			} else if (sub->store_id == -1) {
				sub_ref(sub);
				sub->store_id = mail_get_store(sub->store_uri, NULL, sub_editor_got_store, sub);
			}
		} else {
			if (sub->widget)
				gtk_widget_hide(sub->widget);
		}
		i++;

		link = g_list_next (link);
	}

	update_buttons_sesitivity (se);

	if (se->current && se->is_filtering)
		update_filtering_column (se, se->current);
}

static gboolean sub_editor_timeout(EMSubscribeEditor *se)
{
	gtk_progress_bar_pulse((GtkProgressBar *)se->progress);

	return TRUE;
}

static void sub_editor_busy(EMSubscribeEditor *se, gint dir)
{
	gint was;

	was = se->busy != 0;
	se->busy += dir;
	if (was && !se->busy) {
		g_source_remove(se->busy_id);
		se->busy_id = 0;
		gtk_widget_hide(se->progress);
	} else if (!was && se->busy) {
		se->busy_id = g_timeout_add(1000/5, (GSourceFunc)sub_editor_timeout, se);
		gtk_widget_show(se->progress);
	}
}

#define DEFAULT_WIDTH  600
#define DEFAULT_HEIGHT 400

static GtkAllocation window_size = { 0, 0, 0, 0 };

static void
window_size_allocate (GtkWidget *widget, GtkAllocation *allocation)
{
	GConfClient *gconf;

	/* save to in-memory variable for current session access */
	window_size = *allocation;

	/* save the setting across sessions */
	gconf = gconf_client_get_default ();
	gconf_client_set_int (gconf, "/apps/evolution/mail/subscribe_window/width", window_size.width, NULL);
	gconf_client_set_int (gconf, "/apps/evolution/mail/subscribe_window/height", window_size.height, NULL);
	g_object_unref (gconf);
}

static void
store_expanded_rows_cb (GtkTreeView *tree_view, GtkTreePath *path, gpointer data)
{
	GSList **slist = data;

	g_return_if_fail (path != NULL);
	g_return_if_fail (data != NULL);

	*slist = g_slist_prepend (*slist, gtk_tree_path_copy (path));
}

static void
expand_to_path_cb (GtkTreePath *path, GtkTreeView *tree_view)
{
	g_return_if_fail (path != NULL);
	g_return_if_fail (tree_view != NULL);

	gtk_tree_view_expand_to_path (tree_view, path);
}

static void
change_filtering_models (EMSubscribeEditor *se, gboolean turn_on)
{
	GList *link;

	link = g_queue_peek_head_link (&se->stores);
	while (link != NULL) {
		struct _EMSubscribe *sub = link->data;

		if (sub->widget && sub->tree) {
			if (turn_on) {
				g_slist_foreach (sub->tree_expanded_paths, (GFunc) gtk_tree_path_free, NULL);
				g_slist_free (sub->tree_expanded_paths);
				sub->tree_expanded_paths = NULL;

				gtk_tree_view_map_expanded_rows (sub->tree, store_expanded_rows_cb, &sub->tree_expanded_paths);

				gtk_list_store_clear (GTK_LIST_STORE (sub->list_store));
				gtk_tree_view_set_model (sub->tree, sub->list_store);
			} else {
				gtk_tree_view_set_model (sub->tree, sub->tree_store);

				g_slist_foreach (sub->tree_expanded_paths, (GFunc) expand_to_path_cb, sub->tree);
				g_slist_foreach (sub->tree_expanded_paths, (GFunc) gtk_tree_path_free, NULL);
				g_slist_free (sub->tree_expanded_paths);
				sub->tree_expanded_paths = NULL;
			}

			gtk_tree_view_set_search_column (sub->tree, COL_NAME);
		}

		link = g_list_next (link);
	}

	update_buttons_sesitivity (se);
}

static gboolean
update_filter_on_timeout_cb (gpointer data)
{
	EMSubscribeEditor *se = data;

	g_return_val_if_fail (se != NULL, FALSE);

	se->refilter_id = 0;
	if (se->current) {
		/* update filtering options */
		update_filtering_column (se, se->current);
	}

	return FALSE;
}

static void
clear_filter_cb (GtkEntry *entry, GtkEntryIconPosition icon_pos, GdkEvent *event, EMSubscribeEditor *se)
{
	g_return_if_fail (entry != NULL);

	gtk_entry_set_text (entry, "");
}

static void
filter_changed_cb (GtkEntry *entry, EMSubscribeEditor *se)
{
	const gchar *text;
	gboolean was_filtering;

	g_return_if_fail (entry != NULL);
	g_return_if_fail (se != NULL);

	text = gtk_entry_get_text (entry);
	was_filtering = se->is_filtering;
	se->is_filtering = text && *text;
	gtk_entry_set_icon_sensitive (GTK_ENTRY (se->filter_entry), GTK_ENTRY_ICON_SECONDARY, se->is_filtering);

	if (se->refilter_id != 0) {
		g_source_remove (se->refilter_id);
		se->refilter_id = 0;
	}

	if ((was_filtering && !se->is_filtering) || (!was_filtering && se->is_filtering)) {
		/* turn on/off filtering - change models */
		change_filtering_models (se, se->is_filtering);
	}

	if (se->is_filtering && se->current)
		se->refilter_id = g_timeout_add (333, update_filter_on_timeout_cb, se);
}

static void
expand_all_cb (GtkButton *button, EMSubscribeEditor *se)
{
	g_return_if_fail (se != NULL);
	g_return_if_fail (!se->is_filtering);
	g_return_if_fail (se->current != NULL);
	g_return_if_fail (se->current->tree != NULL);

	gtk_tree_view_expand_all (se->current->tree);
}

static void
collapse_all_cb (GtkButton *button, EMSubscribeEditor *se)
{
	g_return_if_fail (se != NULL);
	g_return_if_fail (!se->is_filtering);
	g_return_if_fail (se->current != NULL);
	g_return_if_fail (se->current->tree != NULL);

	gtk_tree_view_collapse_all (se->current->tree);
}

GtkWidget *
em_subscribe_editor_new(void)
{
	EMSubscribeEditor *se;
	EAccountList *accounts;
	EIterator *iter;
	GtkBuilder *builder;
	GtkWidget *w;
	GtkWidget *container;
	GtkCellRenderer *cell;
	GtkListStore *store;
	GtkTreeIter gtiter;

	se = g_malloc0(sizeof(*se));
	g_queue_init (&se->stores);

	/* XXX I think we're leaking the GtkBuilder. */
	builder = gtk_builder_new ();
	e_load_ui_builder_definition (builder, "mail-dialogs.ui");

	se->dialog = (GtkDialog *)e_builder_get_widget (builder, "subscribe_dialog");
	g_signal_connect(se->dialog, "destroy", G_CALLBACK(sub_editor_destroy), se);

	gtk_widget_ensure_style ((GtkWidget *)se->dialog);

	container = gtk_dialog_get_action_area (GTK_DIALOG (se->dialog));
	gtk_container_set_border_width (GTK_CONTAINER (container), 12);

	container = gtk_dialog_get_content_area (GTK_DIALOG (se->dialog));
	gtk_container_set_border_width (GTK_CONTAINER (container), 0);

	se->vbox = e_builder_get_widget(builder, "tree_box");

	/* FIXME: This is just to get the shadow, is there a better way? */
	w = gtk_label_new(_("Please select a server."));
	se->none_selected = gtk_viewport_new(NULL, NULL);
	gtk_viewport_set_shadow_type((GtkViewport *)se->none_selected, GTK_SHADOW_IN);
	gtk_container_add((GtkContainer *)se->none_selected, w);
	gtk_widget_show(w);

	gtk_box_pack_start((GtkBox *)se->vbox, se->none_selected, TRUE, TRUE, 0);
	gtk_widget_show(se->none_selected);

	se->progress = e_builder_get_widget(builder, "progress_bar");
	gtk_widget_hide(se->progress);

	se->filter_entry = e_builder_get_widget (builder, "filter_entry");
	gtk_entry_set_icon_sensitive (GTK_ENTRY (se->filter_entry), GTK_ENTRY_ICON_SECONDARY, FALSE);
	g_signal_connect (se->filter_entry, "icon-press", G_CALLBACK (clear_filter_cb), se);
	g_signal_connect (se->filter_entry, "changed", G_CALLBACK (filter_changed_cb), se);

	se->expand_button = e_builder_get_widget (builder, "expand_button");
	g_signal_connect (se->expand_button, "clicked", G_CALLBACK (expand_all_cb), se);

	se->collapse_button = e_builder_get_widget (builder, "collapse_button");
	g_signal_connect (se->collapse_button, "clicked", G_CALLBACK (collapse_all_cb), se);

	se->refresh_button = e_builder_get_widget (builder, "refresh_button");
	g_signal_connect (se->refresh_button, "clicked", G_CALLBACK (sub_editor_refresh), se);

	w = e_builder_get_widget (builder, "close_button");
	g_signal_connect (w, "clicked", G_CALLBACK (sub_editor_close), se);

	/* setup stores combobox */
	se->combobox = e_builder_get_widget (builder, "store_combobox");
	store = gtk_list_store_new (2, G_TYPE_STRING, G_TYPE_BOOLEAN);
	gtk_combo_box_set_model (GTK_COMBO_BOX (se->combobox), GTK_TREE_MODEL (store));
	g_object_unref (store);

	gtk_cell_layout_clear (GTK_CELL_LAYOUT (se->combobox));

	cell = gtk_cell_renderer_text_new ();
	gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (se->combobox), cell, TRUE);
	gtk_cell_layout_set_attributes (GTK_CELL_LAYOUT (se->combobox), cell,
                                  "text", 0,
                                  NULL);

	gtk_list_store_append (store, &gtiter);
	gtk_list_store_set (
		store, &gtiter,
		0, _("No server has been selected"),
		1, FALSE,
		-1);

	accounts = e_get_account_list ();
	for (iter = e_list_get_iterator ((EList *) accounts);
	     e_iterator_is_valid (iter);
	     e_iterator_next (iter)) {
		EAccount *account = (EAccount *) e_iterator_get (iter);

		/* setup url table, and store table? */
		if (account->enabled && account->source->url) {
			d(printf("adding account '%s'\n", account->name));
			gtk_list_store_append (store, &gtiter);
			gtk_list_store_set (
				store, &gtiter,
				0, account->name,
				1, TRUE,
				-1);
			g_queue_push_tail (
				&se->stores, subscribe_new (
				se, account->source->url));
		} else {
			d(printf("not adding account '%s'\n", account->name));
		}
	}
	g_object_unref(iter);

	gtk_combo_box_set_active (GTK_COMBO_BOX (se->combobox), 0);
	g_signal_connect(se->combobox, "changed", G_CALLBACK(sub_editor_combobox_changed), se);

	if (window_size.width == 0) {
		/* initialize @window_size with the previous session's size */
		GConfClient *gconf;
		GError *err = NULL;

		gconf = gconf_client_get_default ();

		window_size.width = gconf_client_get_int (gconf, "/apps/evolution/mail/subscribe_window/width", &err);
		if (err != NULL) {
			window_size.width = DEFAULT_WIDTH;
			g_clear_error (&err);
		}

		window_size.height = gconf_client_get_int (gconf, "/apps/evolution/mail/subscribe_window/height", &err);
		if (err != NULL) {
			window_size.height = DEFAULT_HEIGHT;
			g_clear_error (&err);
		}

		g_object_unref (gconf);
	}

	gtk_window_set_default_size ((GtkWindow *) se->dialog, window_size.width, window_size.height);
	g_signal_connect (se->dialog, "size-allocate", G_CALLBACK (window_size_allocate), NULL);

	update_buttons_sesitivity (se);

	return GTK_WIDGET (se->dialog);
}
