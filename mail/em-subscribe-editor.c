/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 * em-subscribe-editor.c *
 *
 * Authors: Michael Zucchi <notzed@ximian.com>
 *
 * Copyright 2003 Ximian, Inc. (www.ximian.com)
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
#include "e-util/e-account-list.h"

#include "em-subscribe-editor.h"

#include "mail-config.h"

#include <glade/glade.h>
#include <libgnome/gnome-i18n.h>

#include <gtk/gtkdialog.h>
#include <gtk/gtkscrolledwindow.h>
#include <gtk/gtkbox.h>
#include <gtk/gtklabel.h>
#include <gtk/gtktreeview.h>
#include <gtk/gtktreestore.h>
#include <gtk/gtktreeselection.h>
#include <gtk/gtkcellrenderertoggle.h>
#include <gtk/gtkcellrenderertext.h>
#include <gtk/gtkoptionmenu.h>
#include <gtk/gtkprogressbar.h>
#include <gtk/gtkmenuitem.h>

#define d(x) 

typedef struct _EMSubscribeEditor EMSubscribeEditor;
struct _EMSubscribeEditor {
	EDList stores;

	int busy;
	guint busy_id;

	struct _EMSubscribe *current; /* the current one, if any */

	GtkDialog *dialog;
	GtkWidget *vbox;	/* where new stores are added */
	GtkWidget *optionmenu;
	GtkWidget *none_selected; /* 'please select a xxx' message */
	GtkWidget *none_selected_item;
	GtkWidget *subscribe_button;
	GtkWidget *unsubscribe_button;
	GtkWidget *progress;
};

typedef struct _EMSubscribe EMSubscribe;
struct _EMSubscribe {
	struct _EMSubscribe *next;
	struct _EMSubscribe *prev;

	int ref_count;
	int cancel;

	struct _EMSubscribeEditor *editor; /* parent object*/

	char *store_uri;
	int store_id;		/* looking up a store */

	CamelStore *store;
	GHashTable *folders;

	GtkWidget *widget;	/* widget to show for this store */
	GtkTreeView *tree;	/* tree, if we have it */

	/* list of all returns from get_folder_info, accessed by other structures */
	GSList *info_list;

	/* pending LISTs, EMSubscribeNode's */
	int pending_id;
	EDList pending;
	
	/* queue of pending UN/SUBSCRIBEs, EMsg's */
	int subscribe_id;
	EDList subscribe;

	/* working variables at runtime */
	int selected_count;
	int selected_subscribed_count;
	gboolean subscribed_state:1; /* for setting the selection*/
};

typedef struct _EMSubscribeNode EMSubscribeNode;
struct _EMSubscribeNode {
	struct _EMSubscribeNode *next;
	struct _EMSubscribeNode *prev;

	CamelFolderInfo *info;
	GtkTreePath *path;
};

static void sub_editor_busy(EMSubscribeEditor *se, int dir);
static int sub_queue_fill_level(EMSubscribe *sub, EMSubscribeNode *node);
static void sub_selection_changed(GtkTreeSelection *selection, EMSubscribe *sub);

static void
sub_node_free(char *key, EMSubscribeNode *node, EMSubscribe *sub)
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
		if (sub->folders) {
			g_hash_table_foreach(sub->folders, (GHFunc)sub_node_free, sub);
			g_hash_table_destroy(sub->folders);
		}
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
	struct _mail_msg msg;

	EMSubscribe *sub;
	EMSubscribeNode *node;
	int subscribe;
	char *path;
};

static void 
sub_folder_subscribe (struct _mail_msg *mm)
{
	struct _zsubscribe_msg *m = (struct _zsubscribe_msg *) mm;

	if (m->subscribe)
		camel_store_subscribe_folder (m->sub->store, m->node->info->full_name, &mm->ex);
	else
		camel_store_unsubscribe_folder (m->sub->store, m->node->info->full_name, &mm->ex);
}

static void
sub_folder_subscribed (struct _mail_msg *mm)
{
	struct _zsubscribe_msg *m = (struct _zsubscribe_msg *)mm, *next;
	GtkTreeIter iter;
	GtkTreeModel *model;
	EMSubscribeNode *node;
	gboolean subscribed, issub;

	m->sub->subscribe_id = -1;
	if (m->sub->cancel)
		return;

	if (!camel_exception_is_set(&mm->ex)) {
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
		else
			d(printf("node mismatch, or subscribe state changed failed\n"));
	}

	/* queue any further ones, or if out, update the ui */
	next = (struct _zsubscribe_msg *)e_dlist_remhead(&m->sub->subscribe);
	if (next) {
		next->sub->subscribe_id = next->msg.seq;
		e_thread_put(mail_thread_new, (EMsg *)next);
	} else {
		/* should it go off the model instead? */
		sub_selection_changed(gtk_tree_view_get_selection(m->sub->tree), m->sub);
	}
}

static void 
sub_folder_free (struct _mail_msg *mm)
{
	struct _zsubscribe_msg *m = (struct _zsubscribe_msg *) mm;

	g_free(m->path);
	sub_unref(m->sub);
}

static struct _mail_msg_op sub_subscribe_folder_op = {
	NULL, /*subscribe_folder_desc,*/
	sub_folder_subscribe,
	sub_folder_subscribed,
	sub_folder_free,
};

/* spath is tree path in string form */
static int
sub_subscribe_folder (EMSubscribe *sub, EMSubscribeNode *node, int state, const char *spath)
{
	struct _zsubscribe_msg *m;
	int id;

	m = mail_msg_new (&sub_subscribe_folder_op, NULL, sizeof(*m));
	m->sub = sub;
	sub_ref(sub);
	m->node = node;
	m->subscribe = state;
	m->path = g_strdup(spath);

	id = m->msg.seq;
	if (sub->subscribe_id == -1) {
		sub->subscribe_id = id;
		d(printf("running subscribe folder '%s'\n", spath));
		e_thread_put (mail_thread_new, (EMsg *)m);
	} else {
		d(printf("queueing subscribe folder '%s'\n", spath));
		e_dlist_addtail(&sub->subscribe, (EDListNode *)m);
	}

	return id;
}

/* ********************************************************************** */
static void
sub_fill_level(EMSubscribe *sub, CamelFolderInfo *info,  GtkTreeIter *parent, int pending)
{
	CamelFolderInfo *fi;
	GtkTreeStore *treestore;
	GtkTreeIter iter;
	EMSubscribeNode *node;

	treestore = (GtkTreeStore *)gtk_tree_view_get_model(sub->tree);

	/* first, fill a level up */
	fi = info;
	while (fi) {
		if (g_hash_table_lookup(sub->folders, fi->full_name) == NULL) {
			gboolean state;

			gtk_tree_store_append(treestore, &iter, parent);
			node = g_malloc0(sizeof(*node));
			node->info = fi;
			/* FIXME: CAMEL_FOLDER_SUBSCRIBED not implemented properly in imap */
			state = camel_store_folder_subscribed(sub->store, fi->full_name);
			/* state = (fi->flags & CAMEL_FOLDER_SUBSCRIBED) != 0; */
			gtk_tree_store_set(treestore, &iter, 0, state, 1, fi->name, 2, node, -1);
			if ((fi->flags & CAMEL_FOLDER_NOINFERIORS) == 0) {
				node->path = gtk_tree_model_get_path((GtkTreeModel *)treestore, &iter);
				if (node->path) {
					/* save time, if we have any children alread, dont re-scan */
					if (fi->child) {
						d(printf("scanning child '%s'\n", fi->child->full_name));
						sub_fill_level(sub, fi->child, &iter, FALSE);
					} else {
						if (pending)
							e_dlist_addtail(&sub->pending, (EDListNode *)node);
					}
				}
			}
			g_hash_table_insert(sub->folders, fi->full_name, node);
		}
		fi = fi->next;
	}
}

/* async query of folderinfo */

struct _emse_folderinfo_msg {
	struct _mail_msg msg;

	EMSubscribe *sub;
	EMSubscribeNode *node;
	CamelFolderInfo *info;
};

static void
sub_folderinfo_get (struct _mail_msg *mm)
{
	struct _emse_folderinfo_msg *m = (struct _emse_folderinfo_msg *) mm;

	camel_operation_register(mm->cancel);
	m->info = camel_store_get_folder_info (m->sub->store, m->node?m->node->info->full_name:"", CAMEL_STORE_FOLDER_INFO_FAST | CAMEL_STORE_FOLDER_INFO_NO_VIRTUAL, &mm->ex);
	camel_operation_unregister(mm->cancel);
}

static void
sub_folderinfo_got(struct _mail_msg *mm)
{
	struct _emse_folderinfo_msg *m = (struct _emse_folderinfo_msg *) mm;
	EMSubscribeNode *node;

	m->sub->pending_id = -1;
	if (m->sub->cancel)
		return;

	if (camel_exception_is_set (&mm->ex)) {
		g_warning ("Error getting folder info from store: %s",
			   camel_exception_get_description (&mm->ex));
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
sub_folderinfo_free(struct _mail_msg *mm)
{
	struct _emse_folderinfo_msg *m = (struct _emse_folderinfo_msg *) mm;

	if (m->info)
		m->sub->info_list = g_slist_prepend(m->sub->info_list, m->info);

	if (!m->sub->cancel)
		sub_editor_busy(m->sub->editor, -1);

	sub_unref(m->sub);
}

static struct _mail_msg_op sub_folderinfo_op = {
	NULL, /*sub_folderinfo_desc,  we do our own progress reporting/cancellation */
	sub_folderinfo_get,
	sub_folderinfo_got,
	sub_folderinfo_free,
};

static int
sub_queue_fill_level(EMSubscribe *sub, EMSubscribeNode *node)
{
	struct _emse_folderinfo_msg *m;
	int id;

	d(printf("Starting get folderinfo of '%s'\n", node?node->info->full_name:"<root>"));

	m = mail_msg_new (&sub_folderinfo_op, NULL, sizeof(*m));
	sub_ref(sub);
	m->sub = sub;
	m->node = node;

	sub->pending_id = m->msg.seq;

	sub_editor_busy(sub->editor, 1);

	id = m->msg.seq;

	e_thread_put (mail_thread_new, (EMsg *)m);
	return id;
}

/* ********************************************************************** */

/* (un) subscribes the current selection */
static void sub_do_subscribe(GtkTreeModel *model, GtkTreePath *path, GtkTreeIter *iter, void *data)
{
	EMSubscribe *sub = data;
	EMSubscribeNode *node;
	gboolean subscribed;

	gtk_tree_model_get(model, iter, 0, &subscribed, 2, &node, -1);
	if (sub->subscribed_state ^ subscribed) {
		char *spath;

		spath = gtk_tree_path_to_string(path);
		gtk_tree_store_set((GtkTreeStore *)model, iter, 0, subscribed, -1);
		sub_subscribe_folder(sub, node, sub->subscribed_state, spath);
		g_free(spath);
	}
}

static void
sub_subscribe(EMSubscribe *sub, gboolean subscribed)
{
	GtkTreeSelection *selection;

	if (sub->tree == NULL)
		return;

	sub->subscribed_state = subscribed;
	selection = gtk_tree_view_get_selection (sub->tree);
	gtk_tree_selection_selected_foreach(selection, sub_do_subscribe, sub);
}

static void
sub_subscribe_toggled(GtkCellRendererToggle *render, const char *spath, EMSubscribe *sub)
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

static void sub_do_changed(GtkTreeModel *model, GtkTreePath *path, GtkTreeIter *iter, void *data)
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
	int dosub = TRUE, dounsub = TRUE;

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

	gtk_widget_set_sensitive(sub->editor->subscribe_button, dosub);
	gtk_widget_set_sensitive(sub->editor->unsubscribe_button, dounsub);
}

/* double-clicking causes a node item to be evaluated directly */
static void sub_row_activated(GtkTreeView *tree, GtkTreePath *path, GtkTreeViewColumn *col, EMSubscribe *sub) {
	EMSubscribeNode *node;
	GtkTreeIter iter;
	GtkTreeModel *model = gtk_tree_view_get_model(tree);

	if (gtk_tree_model_get_iter(model, &iter, path) != TRUE) return;

	gtk_tree_model_get(model, &iter, 2, &node, -1);

	/* check whether the item is already processed */
	if (node->path == NULL)
		return;

	/* remove it from wherever in the list it is, and place it in front instead */
	e_dlist_remove((EDListNode *)node);
	e_dlist_addhead(&sub->pending, (EDListNode *)node);

	if (sub->pending_id == -1
	    && (node = (EMSubscribeNode *)e_dlist_remtail(&sub->pending)))
		sub_queue_fill_level(sub, node);
}

static void
sub_row_expanded(GtkTreeView *tree, GtkTreeIter *iter, GtkTreePath *path, EMSubscribe *sub)
{
	EMSubscribeNode *node;
	GtkTreeIter child;
	GtkTreeModel *model = (GtkTreeModel *)gtk_tree_view_get_model(tree);
	EDList list;

	gtk_tree_model_get(model, iter, 2, &node, -1);
	if (node->path == NULL) {
		d(printf("path '%s' already processed\n", node->info->full_name));
		return;
	}
	gtk_tree_path_free(node->path);
	node->path = NULL;

	e_dlist_init(&list);

	/* add all children nodes to pending, and fire off a pending */
	/* we add them to the head of the pending list, to make it more interactive */
	gtk_tree_model_iter_children(model, &child, iter);
	do {
		gtk_tree_model_get(model, &child, 2, &node, -1);
		if (node->path)
			e_dlist_addtail(&list, (EDListNode *)node);
	} while (gtk_tree_model_iter_next(model, &child));

	while ( (node = (EMSubscribeNode *)e_dlist_remtail(&list)) )
		e_dlist_addhead(&sub->pending, (EDListNode *)node);

	if (sub->pending_id == -1
	    && (node = (EMSubscribeNode *)e_dlist_remtail(&sub->pending)))
		sub_queue_fill_level(sub, node);
}

static void
sub_destroy(GtkWidget *w, EMSubscribe *sub)
{
	struct _zsubscribe_msg *m;

	d(printf("subscribe closed\n"));
	sub->cancel = TRUE;

	if (sub->pending_id != -1)
		mail_msg_cancel(sub->pending_id);

	if (sub->subscribe_id != -1)
		mail_msg_cancel(sub->subscribe_id);

	while ( (m = (struct _zsubscribe_msg *)e_dlist_remhead(&sub->subscribe)) )
		mail_msg_free(m);

	sub_unref(sub);
}

static EMSubscribe *
subscribe_new(EMSubscribeEditor *se, const char *uri)
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
		sub->folders = g_hash_table_new(g_str_hash, g_str_equal);
		
		model = gtk_tree_store_new (3, G_TYPE_BOOLEAN, G_TYPE_STRING, G_TYPE_POINTER);
		sub->tree = (GtkTreeView *) gtk_tree_view_new_with_model ((GtkTreeModel *) model);
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

	/* drop any currently pending */
	if (sub->pending_id != -1)
		mail_msg_cancel(sub->pending_id);

	gtk_tree_store_clear((GtkTreeStore *)gtk_tree_view_get_model(sub->tree));

	e_dlist_init(&sub->pending);
	if (sub->folders) {
		g_hash_table_foreach(sub->folders, (GHFunc)sub_node_free, sub);
		g_hash_table_destroy(sub->folders);
	}
	sub->folders = g_hash_table_new(g_str_hash, g_str_equal);

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
sub_editor_subscribe(GtkWidget *w, EMSubscribeEditor *se)
{
	d(printf("subscribe clicked, current = %p\n", se->current));

	if (se->current)
		sub_subscribe(se->current, TRUE);
}

static void
sub_editor_unsubscribe(GtkWidget *w, EMSubscribeEditor *se)
{
	d(printf("unsubscribe clicked\n"));

	if (se->current)
		sub_subscribe(se->current, FALSE);
}

static void
sub_editor_got_store(char *uri, CamelStore *store, void *data)
{
	struct _EMSubscribe *sub = data;

	if (!sub->cancel)
		subscribe_set_store(sub, store);
	sub_unref(sub);
}

static void
sub_editor_menu_changed(GtkWidget *w, EMSubscribeEditor *se)
{
	int i, n;
	struct _EMSubscribe *sub;

	d(printf("menu changed\n"));

	i = 1;
	n = gtk_option_menu_get_history((GtkOptionMenu *)se->optionmenu);
	if (n == 0)
		gtk_widget_show(se->none_selected);
	else {
		gtk_widget_hide(se->none_selected);
		gtk_widget_hide(se->none_selected_item);
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

static void sub_editor_busy(EMSubscribeEditor *se, int dir)
{
	int was;

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
	GtkWidget *menu, *w;

	se = g_malloc0(sizeof(*se));
	e_dlist_init(&se->stores);

	xml = glade_xml_new (EVOLUTION_GLADEDIR "/mail-dialogs.glade", "subscribe_dialog", NULL);
	if (xml == NULL) {
		/* ?? */
		return NULL;
	}
	se->dialog = (GtkDialog *)glade_xml_get_widget (xml, "subscribe_dialog");
	g_signal_connect(se->dialog, "destroy", G_CALLBACK(sub_editor_destroy), se);
	
	gtk_widget_realize ((GtkWidget *)se->dialog);
	gtk_container_set_border_width ((GtkContainer *) ((GtkDialog *)se->dialog)->action_area, 12);
	gtk_container_set_border_width ((GtkContainer *) ((GtkDialog *)se->dialog)->vbox, 0);

	se->vbox = glade_xml_get_widget(xml, "tree_box");

	se->subscribe_button = glade_xml_get_widget (xml, "subscribe_button");
	g_signal_connect(se->subscribe_button, "clicked", G_CALLBACK(sub_editor_subscribe), se);
	se->unsubscribe_button = glade_xml_get_widget (xml, "unsubscribe_button");
	g_signal_connect(se->unsubscribe_button, "clicked", G_CALLBACK(sub_editor_unsubscribe), se);

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

	/* setup stores menu */
	se->optionmenu = glade_xml_get_widget(xml, "store_menu");
	menu = gtk_menu_new();
	se->none_selected_item = w = gtk_menu_item_new_with_label(_("No server has been selected"));
	gtk_widget_show(w);
	gtk_menu_shell_append ((GtkMenuShell *)menu, w);

	accounts = mail_config_get_accounts ();
	for (iter = e_list_get_iterator ((EList *) accounts);
	     e_iterator_is_valid (iter);
	     e_iterator_next (iter)) {
		EAccount *account = (EAccount *) e_iterator_get (iter);
		
		/* setup url table, and store table? */
		if (account->enabled && account->source->url) {
			d(printf("adding account '%s'\n", account->name));
			w = gtk_menu_item_new_with_label(account->name);
			gtk_menu_shell_append ((GtkMenuShell *)menu, w);
			gtk_widget_show(w);
			e_dlist_addtail(&se->stores, (EDListNode *)subscribe_new(se, account->source->url));
		} else {
			d(printf("not adding account '%s'\n", account->name));
		}
	}
	g_object_unref(iter);

	gtk_option_menu_set_menu((GtkOptionMenu *)se->optionmenu, menu);
	g_signal_connect(se->optionmenu, "changed", G_CALLBACK(sub_editor_menu_changed), se);
	
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
