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

#include <pthread.h>

#include "mail-tools.h"
#include "mail-ops.h"
#include "mail-mt.h"
/*#include "mail-folder-cache.h"*/
#include "camel/camel-exception.h"
#include "camel/camel-store.h"
#include "camel/camel-session.h"
#include "libedataserver/e-account-list.h"
#include "libedataserver/e-msgport.h"
#include "e-util/e-util-private.h"

#include "em-subscribe-editor.h"

#include "mail-config.h"

#include <glade/glade.h>
#include <glib/gi18n.h>

#define d(x)

typedef struct _EMSubscribeEditor EMSubscribeEditor;
struct _EMSubscribeEditor {
	EDList stores;

	gint busy;
	guint busy_id;

	struct _EMSubscribe *current; /* the current one, if any */

	GtkDialog *dialog;
	GtkWidget *vbox;	/* where new stores are added */
	GtkWidget *combobox;
	GtkWidget *none_selected; /* 'please select a xxx' message */
	GtkWidget *progress;
};

typedef struct _EMSubscribe EMSubscribe;
struct _EMSubscribe {
	struct _EMSubscribe *next;
	struct _EMSubscribe *prev;

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

	/* list of all returns from get_folder_info, accessed by other structures */
	GSList *info_list;

	/* pending LISTs, EMSubscribeNode's */
	gint pending_id;
	EDList pending;

	/* queue of pending UN/SUBSCRIBEs, EMsg's */
	gint subscribe_id;
	EDList subscribe;

	/* working variables at runtime */
	gint selected_count;
	gint selected_subscribed_count;
	guint subscribed_state:1; /* for setting the selection*/
};

typedef struct _EMSubscribeNode EMSubscribeNode;
struct _EMSubscribeNode {
	struct _EMSubscribeNode *next;
	struct _EMSubscribeNode *prev;

	CamelFolderInfo *info;
	GtkTreePath *path;
};

typedef struct _MailMsgListNode MailMsgListNode;
struct _MailMsgListNode {
	EDListNode node;
	MailMsg *msg;
};

static void sub_editor_busy(EMSubscribeEditor *se, gint dir);
static gint sub_queue_fill_level(EMSubscribe *sub, EMSubscribeNode *node);
static void sub_selection_changed(GtkTreeSelection *selection, EMSubscribe *sub);

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
		if (sub->folders)
			g_hash_table_destroy(sub->folders);
		l = sub->info_list;
		while (l) {
			GSList *n = l->next;

			camel_store_free_folder_info(sub->store, (CamelFolderInfo *)l->data);
			g_slist_free_1(l);
			l = n;
		}
		if (sub->store)
			camel_object_unref(sub->store);
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
		camel_store_subscribe_folder (m->sub->store, m->node->info->full_name, &m->base.ex);
	else
		camel_store_unsubscribe_folder (m->sub->store, m->node->info->full_name, &m->base.ex);
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

	if (!camel_exception_is_set(&m->base.ex)) {
		if (m->subscribe)
			m->node->info->flags |= CAMEL_FOLDER_SUBSCRIBED;
		else
			m->node->info->flags &= ~CAMEL_FOLDER_SUBSCRIBED;
	}

	/* make sure the tree view matches the correct state */
	model = gtk_tree_view_get_model(m->sub->tree);
	if (gtk_tree_model_get_iter_from_string(model, &iter, m->path)) {
		issub = (m->node->info->flags & CAMEL_FOLDER_SUBSCRIBED) != 0;
		gtk_tree_model_get(model, &iter, 0, &subscribed, 2, &node, -1);
		if (node == m->node)
			gtk_tree_store_set((GtkTreeStore *)model, &iter, 0, issub, -1);
		else {
			d(printf("node mismatch, or subscribe state changed failed\n"));
		}
	}

	/* queue any further ones, or if out, update the ui */
	msgListNode = (MailMsgListNode *) e_dlist_remhead(&m->sub->subscribe);
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
		e_dlist_addtail(&sub->subscribe, (EDListNode *)msgListNode);
	}

	return id;
}

/* ********************************************************************** */
static void
sub_fill_level(EMSubscribe *sub, CamelFolderInfo *info,  GtkTreeIter *parent, gint pending)
{
	CamelFolderInfo *fi;
	GtkTreeStore *treestore;
	GtkTreeIter iter;
	EMSubscribeNode *node;

	treestore = (GtkTreeStore *)gtk_tree_view_get_model(sub->tree);

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
			gtk_tree_store_set(treestore, &iter, 0, state, 1, fi->name, 2, node, -1);
			if ((fi->flags & CAMEL_FOLDER_NOINFERIORS) == 0)
				node->path = gtk_tree_model_get_path((GtkTreeModel *)treestore, &iter);
			g_hash_table_insert(sub->folders, fi->full_name, node);
		} else if (node->path) {
			gtk_tree_model_get_iter(gtk_tree_view_get_model(sub->tree), &iter, node->path);
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
				sub_fill_level(sub, fi->child, &iter, FALSE);
			} else if (!(fi->flags & CAMEL_FOLDER_NOCHILDREN)) {
				GtkTreeIter new_iter;
				d(printf("flags: CAMEL_FOLDER_NOCHILDREN is not set '%s', known:%d\n", fi->full_name, known?1:0));
				if (!known) {
					gtk_tree_store_append(treestore, &new_iter, &iter);
					gtk_tree_store_set(treestore, &new_iter, 0, 0, 1, "Loading...", 2, NULL, -1);
				}
			}
			else {
				if (pending)
					e_dlist_addtail(&sub->pending, (EDListNode *)node);
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
	gchar *pub_full_name=NULL;

	if (m->seq == m->sub->seq) {
		camel_operation_register(m->base.cancel);
		m->info = camel_store_get_folder_info(m->sub->store, m->node?m->node->info->full_name:pub_full_name,
						      CAMEL_STORE_FOLDER_INFO_NO_VIRTUAL | CAMEL_STORE_FOLDER_INFO_SUBSCRIPTION_LIST, &m->base.ex);
		camel_operation_unregister(m->base.cancel);
	}
}

static void
sub_folderinfo_done (struct _emse_folderinfo_msg *m)
{
	EMSubscribeNode *node;

	m->sub->pending_id = -1;
	if (m->sub->cancel || m->seq != m->sub->seq)
		return;

	if (camel_exception_is_set (&m->base.ex)) {
		g_warning ("Error getting folder info from store: %s",
			   camel_exception_get_description (&m->base.ex));
	}

	if (m->info) {
		if (m->node) {
			GtkTreeIter iter;

			gtk_tree_model_get_iter(gtk_tree_view_get_model(m->sub->tree), &iter, m->node->path);
			sub_fill_level(m->sub, m->info, &iter, FALSE);
		} else {
			sub_fill_level(m->sub, m->info, NULL, TRUE);
		}
	}

	/* check for more to do */
	node = (EMSubscribeNode *)e_dlist_remhead(&m->sub->pending);
	if (node)
		sub_queue_fill_level(m->sub, node);
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
		gtk_tree_model_get(model, &iter, 0, &subscribed, 2, &node, -1);
		subscribed = !subscribed;
		d(printf("new state is %s\n", subscribed?"subscribed":"not subscribed"));
		gtk_tree_store_set((GtkTreeStore *)model, &iter, 0, subscribed, -1);
		sub_subscribe_folder(sub, node, subscribed, spath);
	}
}

static void sub_do_changed(GtkTreeModel *model, GtkTreePath *path, GtkTreeIter *iter, gpointer data)
{
	EMSubscribe *sub = data;
	EMSubscribeNode *node;
	gboolean subscribed;

	gtk_tree_model_get(model, iter, 0, &subscribed, 2, &node, -1);

	if (subscribed)
		sub->selected_subscribed_count++;
	sub->selected_count++;
}

static void
sub_selection_changed(GtkTreeSelection *selection, EMSubscribe *sub)
{
	gint dosub = TRUE, dounsub = TRUE;

	sub->selected_count = 0;
	sub->selected_subscribed_count = 0;
	gtk_tree_selection_selected_foreach(selection, sub_do_changed, sub);

	if (sub->selected_count == 0) {
		dosub = FALSE;
		dounsub = FALSE;
	} else if (sub->selected_subscribed_count == sub->selected_count)
		dosub = FALSE;
	else if (sub->selected_subscribed_count == 0)
		dounsub = FALSE;

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
sub_row_expanded(GtkTreeView *tree, GtkTreeIter *iter, GtkTreePath *path, EMSubscribe *sub)
{
	EMSubscribeNode *node;
	GtkTreeIter child;
	GtkTreeModel *model = (GtkTreeModel *)gtk_tree_view_get_model(tree);
	gchar *row_name;

	gtk_tree_model_get(model, iter, 1, &row_name, -1);
	d(printf("%s:%s: row-expanded '%s'\n", G_STRLOC, G_STRFUNC,
		 row_name?row_name:"<root>"));

	/* Do we really need to fetch the children for this row? */
	if (gtk_tree_model_iter_n_children(model, iter) > 1) {
		gtk_tree_model_get(model, iter, 2, &node, -1);
		if (node->path) {
			/* Mark it as already-processed path */
			gtk_tree_path_free(node->path);
			node->path=NULL;
		}
		return;
	} else {
		gtk_tree_model_iter_children(model, &child, iter);
		gtk_tree_model_get(model, &child, 2, &node, -1);
		if (!node) {
			/* This is the place holder node, delete it and fire-up a pending */
			gtk_tree_store_remove((GtkTreeStore *)model, &child);
			gtk_tree_model_get(model, iter, 2, &node, -1);
		} else {
			gtk_tree_model_get(model, iter, 2, &node, -1);
			if (node->path) {
				/* Mark it as already-processed path */
				gtk_tree_path_free(node->path);
				node->path=NULL;
			}
			return;
		}
	}

	e_dlist_addhead(&sub->pending, (EDListNode *)node);

	if (sub->pending_id == -1
	    && (node = (EMSubscribeNode *)e_dlist_remtail(&sub->pending)))
		sub_queue_fill_level(sub, node);
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

	while ( (msgListNode = (MailMsgListNode *)e_dlist_remhead(&sub->subscribe))) {
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
	e_dlist_init(&sub->pending);
	sub->subscribe_id = -1;
	e_dlist_init(&sub->subscribe);
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
		GtkCellRenderer *renderer;
		GtkTreeStore *model;

		sub->store = store;
		camel_object_ref(store);
		sub->folders = g_hash_table_new_full (
			g_str_hash, g_str_equal,
			(GDestroyNotify) NULL,
			(GDestroyNotify) sub_node_free);

		model = gtk_tree_store_new (3, G_TYPE_BOOLEAN, G_TYPE_STRING, G_TYPE_POINTER);
		sub->tree = (GtkTreeView *) gtk_tree_view_new_with_model ((GtkTreeModel *) model);
		g_object_unref (model);
		gtk_widget_show ((GtkWidget *)sub->tree);

		sub->widget = gtk_scrolled_window_new (NULL, NULL);
		gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (sub->widget), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
		gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (sub->widget), GTK_SHADOW_IN);
		gtk_container_add((GtkContainer *)sub->widget, (GtkWidget *)sub->tree);
		gtk_widget_show(sub->widget);

		renderer = gtk_cell_renderer_toggle_new ();
		g_object_set(renderer, "activatable", TRUE, NULL);
		gtk_tree_view_insert_column_with_attributes (sub->tree, -1, _("Subscribed"), renderer, "active", 0, NULL);
		g_signal_connect(renderer, "toggled", G_CALLBACK(sub_subscribe_toggled), sub);

		renderer = gtk_cell_renderer_text_new ();
		gtk_tree_view_insert_column_with_attributes (sub->tree, -1, _("Folder"), renderer, "text", 1, NULL);
		gtk_tree_view_set_expander_column(sub->tree, gtk_tree_view_get_column(sub->tree, 1));

		selection = gtk_tree_view_get_selection (sub->tree);
		gtk_tree_selection_set_mode (selection, GTK_SELECTION_MULTIPLE);
		gtk_tree_view_set_headers_visible (sub->tree, FALSE);

		g_signal_connect(sub->tree, "row-expanded", G_CALLBACK(sub_row_expanded), sub);
		g_signal_connect(sub->tree, "row-activated", G_CALLBACK(sub_row_activated), sub);
		g_signal_connect(sub->tree, "destroy", G_CALLBACK(sub_destroy), sub);

		sub_selection_changed(selection, sub);
		g_signal_connect(selection, "changed", G_CALLBACK(sub_selection_changed), sub);

		sub_queue_fill_level(sub, NULL);
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

	gtk_tree_store_clear((GtkTreeStore *)gtk_tree_view_get_model(sub->tree));

	e_dlist_init(&sub->pending);

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
	struct _EMSubscribe *sub;

	d(printf("combobox changed\n"));

	i = 1;
	n = gtk_combo_box_get_active (GTK_COMBO_BOX (se->combobox));
	if (n == 0) {
		gtk_widget_show (se->none_selected);
	} else {
		GtkTreeIter iter;
		GtkTreeModel *model;

		gtk_widget_hide (se->none_selected);

		model = gtk_combo_box_get_model (GTK_COMBO_BOX (se->combobox));
		if (gtk_tree_model_get_iter_first (model, &iter)) {
			/* hide the first item */
			gtk_list_store_set (
				GTK_LIST_STORE (model), &iter,
				1, FALSE,
				-1);
		}
	}

	se->current = NULL;
	sub = (struct _EMSubscribe *)se->stores.head;
	while (sub->next) {
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
		sub = sub->next;
	}
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

GtkDialog *em_subscribe_editor_new(void)
{
	EMSubscribeEditor *se;
	EAccountList *accounts;
	EIterator *iter;
	GladeXML *xml;
	GtkWidget *w;
	GtkCellRenderer *cell;
	GtkListStore *store;
	GtkTreeIter gtiter;
	gchar *gladefile;

	se = g_malloc0(sizeof(*se));
	e_dlist_init(&se->stores);

	gladefile = g_build_filename (EVOLUTION_GLADEDIR,
				      "mail-dialogs.glade",
				      NULL);
	xml = glade_xml_new (gladefile, "subscribe_dialog", NULL);
	g_free (gladefile);

	if (xml == NULL) {
		/* ?? */
		return NULL;
	}
	se->dialog = (GtkDialog *)glade_xml_get_widget (xml, "subscribe_dialog");
	g_signal_connect(se->dialog, "destroy", G_CALLBACK(sub_editor_destroy), se);

	gtk_widget_ensure_style ((GtkWidget *)se->dialog);
	gtk_container_set_border_width ((GtkContainer *) ((GtkDialog *)se->dialog)->action_area, 12);
	gtk_container_set_border_width ((GtkContainer *) ((GtkDialog *)se->dialog)->vbox, 0);

	se->vbox = glade_xml_get_widget(xml, "tree_box");

	/* FIXME: This is just to get the shadow, is there a better way? */
	w = gtk_label_new(_("Please select a server."));
	se->none_selected = gtk_viewport_new(NULL, NULL);
	gtk_viewport_set_shadow_type((GtkViewport *)se->none_selected, GTK_SHADOW_IN);
	gtk_container_add((GtkContainer *)se->none_selected, w);
	gtk_widget_show(w);

	gtk_box_pack_start((GtkBox *)se->vbox, se->none_selected, TRUE, TRUE, 0);
	gtk_widget_show(se->none_selected);

	se->progress = glade_xml_get_widget(xml, "progress_bar");
	gtk_widget_hide(se->progress);

	w = glade_xml_get_widget(xml, "close_button");
	g_signal_connect(w, "clicked", G_CALLBACK(sub_editor_close), se);

	w = glade_xml_get_widget(xml, "refresh_button");
	g_signal_connect(w, "clicked", G_CALLBACK(sub_editor_refresh), se);

	/* setup stores combobox */
	se->combobox = glade_xml_get_widget (xml, "store_combobox");
	store = gtk_list_store_new (2, G_TYPE_STRING, G_TYPE_BOOLEAN);
	gtk_combo_box_set_model (GTK_COMBO_BOX (se->combobox), GTK_TREE_MODEL (store));
	g_object_unref (store);

	gtk_cell_layout_clear (GTK_CELL_LAYOUT (se->combobox));

	cell = gtk_cell_renderer_text_new ();
	gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (se->combobox), cell, TRUE);
	gtk_cell_layout_set_attributes (GTK_CELL_LAYOUT (se->combobox), cell,
                                  "text", 0,
                                  "visible", 1,
                                  NULL);

	gtk_list_store_append (store, &gtiter);
	gtk_list_store_set (
		store, &gtiter,
		0, _("No server has been selected"),
		1, TRUE,
		-1);

	accounts = mail_config_get_accounts ();
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
			e_dlist_addtail(&se->stores, (EDListNode *)subscribe_new(se, account->source->url));
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

	return se->dialog;
}
