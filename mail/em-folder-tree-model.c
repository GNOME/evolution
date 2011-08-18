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

#include "em-folder-tree-model.h"

#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/stat.h>

#include "e-util/e-util.h"
#include "e-util/e-account-utils.h"

#include <glib/gi18n.h>

#include "mail-tools.h"
#include "mail-mt.h"
#include "mail-ops.h"

/* sigh, these 2 only needed for outbox total count checking - a mess */
#include "mail-folder-cache.h"

#include "em-utils.h"
#include "em-folder-utils.h"
#include "em-event.h"

#include "e-mail-folder-utils.h"
#include "e-mail-local.h"
#include "e-mail-store.h"
#include "shell/e-shell.h"

#define d(x)

struct _EMFolderTreeModelPrivate {
	/* This is set by EMailShellSidebar.  It allows new EMFolderTree
	 * instances to initialize their selection and expanded states to
	 * mimic the sidebar. */
	GtkTreeSelection *selection;  /* weak reference */

	EAccountList *accounts;
	EMailSession *session;

	/* CamelStore -> EMFolderTreeStoreInfo */
	GHashTable *store_index;

	/* URI -> GtkTreeRowReference */
	GHashTable *uri_index;

	gulong account_changed_id;
	gulong account_removed_id;
	gulong account_added_id;
};

enum {
	PROP_0,
	PROP_SELECTION,
	PROP_SESSION
};

enum {
	LOADING_ROW,
	LOADED_ROW,
	LAST_SIGNAL
};

extern CamelStore *vfolder_store;

static gpointer parent_class;
static guint signals[LAST_SIGNAL];

G_DEFINE_TYPE (EMFolderTreeModel, em_folder_tree_model, GTK_TYPE_TREE_STORE)

static void
store_info_free (EMFolderTreeModelStoreInfo *si)
{
	g_signal_handler_disconnect (si->store, si->created_id);
	g_signal_handler_disconnect (si->store, si->deleted_id);
	g_signal_handler_disconnect (si->store, si->renamed_id);

	if (si->subscribed_id > 0)
		g_signal_handler_disconnect (si->store, si->subscribed_id);
	if (si->unsubscribed_id > 0)
		g_signal_handler_disconnect (si->store, si->unsubscribed_id);

	g_object_unref (si->store);
	gtk_tree_row_reference_free (si->row);
	g_hash_table_destroy (si->full_hash);
	g_free (si);
}

static gint
folder_tree_model_sort (GtkTreeModel *model,
                        GtkTreeIter *a,
                        GtkTreeIter *b,
                        gpointer user_data)
{
	EShell *shell;
	gchar *aname, *bname;
	CamelStore *store;
	gboolean is_store;
	guint32 aflags, bflags;
	gint rv = -2;

	/* XXX Pass the EShell in as user_data. */
	shell = e_shell_get_default ();

	gtk_tree_model_get (
		model, a,
		COL_BOOL_IS_STORE, &is_store,
		COL_POINTER_CAMEL_STORE, &store,
		COL_STRING_DISPLAY_NAME, &aname,
		COL_UINT_FLAGS, &aflags, -1);

	gtk_tree_model_get (
		model, b,
		COL_STRING_DISPLAY_NAME, &bname,
		COL_UINT_FLAGS, &bflags, -1);

	if (is_store) {
		/* On This Computer is always first, and Search Folders
		 * is always last. */
		if (e_shell_get_express_mode (shell)) {
			if (!strcmp (aname, _("On This Computer")) &&
				!strcmp (bname, _("Search Folders")))
				rv = -1;
			else if (!strcmp (bname, _("On This Computer")) &&
				!strcmp (aname, _("Search Folders")))
				rv = 1;
			else if (!strcmp (aname, _("On This Computer")))
				rv = 1;
			else if (!strcmp (bname, _("On This Computer")))
				rv = -1;
				else if (!strcmp (aname, _("Search Folders")))
				rv = 1;
			else if (!strcmp (bname, _("Search Folders")))
				rv = -1;
		} else {
			if (!strcmp (aname, _("On This Computer")))
				rv = -1;
			else if (!strcmp (bname, _("On This Computer")))
				rv = 1;
			else if (!strcmp (aname, _("Search Folders")))
				rv = 1;
			else if (!strcmp (bname, _("Search Folders")))
				rv = -1;
		}
	} else if (store == vfolder_store) {
		/* UNMATCHED is always last. */
		if (aname && !strcmp (aname, _("UNMATCHED")))
			rv = 1;
		else if (bname && !strcmp (bname, _("UNMATCHED")))
			rv = -1;
	} else {
		/* Inbox is always first. */
		if ((aflags & CAMEL_FOLDER_TYPE_MASK) == CAMEL_FOLDER_TYPE_INBOX)
			rv = -1;
		else if ((bflags & CAMEL_FOLDER_TYPE_MASK) == CAMEL_FOLDER_TYPE_INBOX)
			rv = 1;
	}

	if (aname == NULL) {
		if (bname == NULL)
			rv = 0;
	} else if (bname == NULL)
		rv = 1;

	if (rv == -2)
		rv = g_utf8_collate (aname, bname);

	g_free (aname);
	g_free (bname);

	return rv;
}

static void
account_changed_cb (EAccountList *accounts,
                    EAccount *account,
                    EMFolderTreeModel *model)
{
	EMailSession *session;
	CamelService *service;

	session = em_folder_tree_model_get_session (model);

	service = camel_session_get_service (
		CAMEL_SESSION (session), account->uid);

	if (!CAMEL_IS_STORE (service))
		return;

	em_folder_tree_model_remove_store (model, CAMEL_STORE (service));

	/* check if store needs to be added at all*/
	if (!account->enabled)
		return;

	em_folder_tree_model_add_store (model, CAMEL_STORE (service));
}

static void
account_removed_cb (EAccountList *accounts,
                    EAccount *account,
                    EMFolderTreeModel *model)
{
	EMailSession *session;
	CamelService *service;

	session = em_folder_tree_model_get_session (model);

	service = camel_session_get_service (
		CAMEL_SESSION (session), account->uid);

	if (!CAMEL_IS_STORE (service))
		return;

	em_folder_tree_model_remove_store (model, CAMEL_STORE (service));
}

/* HACK: FIXME: the component should listen to the account object directly */
static void
account_added_cb (EAccountList *accounts,
                  EAccount *account,
                  EMFolderTreeModel *model)
{
	EMailSession *session;

	session = em_folder_tree_model_get_session (model);
	e_mail_store_add_by_account (session, account);
}

static void
folder_tree_model_selection_finalized_cb (EMFolderTreeModel *model)
{
	model->priv->selection = NULL;

	g_object_notify (G_OBJECT (model), "selection");
}

static void
folder_tree_model_set_property (GObject *object,
                                guint property_id,
                                const GValue *value,
                                GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_SELECTION:
			em_folder_tree_model_set_selection (
				EM_FOLDER_TREE_MODEL (object),
				g_value_get_object (value));
			return;

		case PROP_SESSION:
			em_folder_tree_model_set_session (
				EM_FOLDER_TREE_MODEL (object),
				g_value_get_object (value));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
folder_tree_model_get_property (GObject *object,
                                guint property_id,
                                GValue *value,
                                GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_SELECTION:
			g_value_set_object (
				value,
				em_folder_tree_model_get_selection (
				EM_FOLDER_TREE_MODEL (object)));
			return;

		case PROP_SESSION:
			g_value_set_object (
				value,
				em_folder_tree_model_get_session (
				EM_FOLDER_TREE_MODEL (object)));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
folder_tree_model_dispose (GObject *object)
{
	EMFolderTreeModelPrivate *priv;

	priv = EM_FOLDER_TREE_MODEL (object)->priv;

	if (priv->selection != NULL) {
		g_object_weak_unref (
			G_OBJECT (priv->selection), (GWeakNotify)
			folder_tree_model_selection_finalized_cb, object);
		priv->selection = NULL;
	}

	if (priv->session != NULL) {
		g_object_unref (priv->session);
		priv->session = NULL;
	}

	/* Chain up to parent's dispose() method. */
	G_OBJECT_CLASS (parent_class)->dispose (object);
}

static void
folder_tree_model_finalize (GObject *object)
{
	EMFolderTreeModelPrivate *priv;

	priv = EM_FOLDER_TREE_MODEL (object)->priv;

	g_hash_table_destroy (priv->store_index);
	g_hash_table_destroy (priv->uri_index);

	g_signal_handler_disconnect (
		priv->accounts, priv->account_changed_id);
	g_signal_handler_disconnect (
		priv->accounts, priv->account_removed_id);
	g_signal_handler_disconnect (
		priv->accounts, priv->account_added_id);

	/* Chain up to parent's finalize() method. */
	G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
em_folder_tree_model_class_init (EMFolderTreeModelClass *class)
{
	GObjectClass *object_class;

	parent_class = g_type_class_peek_parent (class);
	g_type_class_add_private (class, sizeof (EMFolderTreeModelPrivate));

	object_class = G_OBJECT_CLASS (class);
	object_class->set_property = folder_tree_model_set_property;
	object_class->get_property = folder_tree_model_get_property;
	object_class->dispose = folder_tree_model_dispose;
	object_class->finalize = folder_tree_model_finalize;

	g_object_class_install_property (
		object_class,
		PROP_SELECTION,
		g_param_spec_object (
			"selection",
			"Selection",
			NULL,
			GTK_TYPE_TREE_SELECTION,
			G_PARAM_READWRITE));

	g_object_class_install_property (
		object_class,
		PROP_SESSION,
		g_param_spec_object (
			"session",
			NULL,
			NULL,
			E_TYPE_MAIL_SESSION,
			G_PARAM_READWRITE));

	signals[LOADING_ROW] = g_signal_new (
		"loading-row",
		G_OBJECT_CLASS_TYPE (object_class),
		G_SIGNAL_RUN_FIRST,
		G_STRUCT_OFFSET (EMFolderTreeModelClass, loading_row),
		NULL, NULL,
		e_marshal_VOID__POINTER_POINTER,
		G_TYPE_NONE, 2,
		G_TYPE_POINTER,
		G_TYPE_POINTER);

	signals[LOADED_ROW] = g_signal_new (
		"loaded-row",
		G_OBJECT_CLASS_TYPE (object_class),
		G_SIGNAL_RUN_FIRST,
		G_STRUCT_OFFSET (EMFolderTreeModelClass, loaded_row),
		NULL, NULL,
		e_marshal_VOID__POINTER_POINTER,
		G_TYPE_NONE, 2,
		G_TYPE_POINTER,
		G_TYPE_POINTER);
}

static void
folder_tree_model_set_unread_count (EMFolderTreeModel *model,
                                    CamelStore *store,
                                    const gchar *full,
                                    gint unread)
{
	EMFolderTreeModelStoreInfo *si;
	GtkTreeRowReference *reference;
	GtkTreeModel *tree_model;
	GtkTreePath *path;
	GtkTreeIter parent;
	GtkTreeIter iter;
	guint old_unread = 0;

	g_return_if_fail (EM_IS_FOLDER_TREE_MODEL (model));
	g_return_if_fail (CAMEL_IS_STORE (store));
	g_return_if_fail (full != NULL);

	if (unread < 0)
		return;

	si = em_folder_tree_model_lookup_store_info (model, store);
	if (si == NULL)
		return;

	reference = g_hash_table_lookup (si->full_hash, full);
	if (!gtk_tree_row_reference_valid (reference))
		return;

	tree_model = GTK_TREE_MODEL (model);

	path = gtk_tree_row_reference_get_path (reference);
	gtk_tree_model_get_iter (tree_model, &iter, path);
	gtk_tree_path_free (path);

	gtk_tree_model_get (
		tree_model, &iter,
		COL_UINT_UNREAD_LAST_SEL, &old_unread, -1);

	gtk_tree_store_set (
		GTK_TREE_STORE (model), &iter,
		COL_UINT_UNREAD, unread,
		COL_UINT_UNREAD_LAST_SEL, MIN (old_unread, unread), -1);

	/* Folders are displayed with a bold weight to indicate that
	 * they contain unread messages.  We signal that parent rows
	 * have changed here to update them. */
	while (gtk_tree_model_iter_parent (tree_model, &parent, &iter)) {
		path = gtk_tree_model_get_path (tree_model, &parent);
		gtk_tree_model_row_changed (tree_model, path, &parent);
		gtk_tree_path_free (path);
		iter = parent;
	}
}

static void
em_folder_tree_model_init (EMFolderTreeModel *model)
{
	GHashTable *store_index;
	GHashTable *uri_index;

	GType col_types[] = {
		G_TYPE_STRING,   /* display name */
		G_TYPE_POINTER,  /* store object */
		G_TYPE_STRING,   /* full name */
		G_TYPE_STRING,   /* icon name */
		G_TYPE_STRING,   /* uri */
		G_TYPE_UINT,     /* unread count */
		G_TYPE_UINT,     /* flags */
		G_TYPE_BOOLEAN,  /* is a store node */
		G_TYPE_BOOLEAN,  /* is a folder node */
		G_TYPE_BOOLEAN,  /* has not-yet-loaded subfolders */
		G_TYPE_UINT,      /* last known unread count */
		G_TYPE_BOOLEAN  /* folder is a draft folder */
	};

	store_index = g_hash_table_new_full (
		g_direct_hash, g_direct_equal,
		(GDestroyNotify) NULL,
		(GDestroyNotify) store_info_free);

	uri_index = g_hash_table_new_full (
		g_str_hash, g_str_equal,
		(GDestroyNotify) g_free,
		(GDestroyNotify) gtk_tree_row_reference_free);

	model->priv = G_TYPE_INSTANCE_GET_PRIVATE (
		model, EM_TYPE_FOLDER_TREE_MODEL, EMFolderTreeModelPrivate);
	model->priv->store_index = store_index;
	model->priv->uri_index = uri_index;

	gtk_tree_store_set_column_types (
		GTK_TREE_STORE (model), NUM_COLUMNS, col_types);
	gtk_tree_sortable_set_default_sort_func (
		GTK_TREE_SORTABLE (model),
		folder_tree_model_sort, NULL, NULL);
	gtk_tree_sortable_set_sort_column_id (
		GTK_TREE_SORTABLE (model),
		GTK_TREE_SORTABLE_DEFAULT_SORT_COLUMN_ID,
		GTK_SORT_ASCENDING);

	model->priv->accounts = e_get_account_list ();
	model->priv->account_changed_id = g_signal_connect (
		model->priv->accounts, "account-changed",
		G_CALLBACK (account_changed_cb), model);
	model->priv->account_removed_id = g_signal_connect (
		model->priv->accounts, "account-removed",
		G_CALLBACK (account_removed_cb), model);
	model->priv->account_added_id = g_signal_connect (
		model->priv->accounts, "account-added",
		G_CALLBACK (account_added_cb), model);
}

EMFolderTreeModel *
em_folder_tree_model_new (void)
{
	return g_object_new (EM_TYPE_FOLDER_TREE_MODEL, NULL);
}

EMFolderTreeModel *
em_folder_tree_model_get_default (void)
{
	static EMFolderTreeModel *default_folder_tree_model;

	if (G_UNLIKELY (default_folder_tree_model == NULL))
		default_folder_tree_model = em_folder_tree_model_new ();

	return default_folder_tree_model;
}

GtkTreeSelection *
em_folder_tree_model_get_selection (EMFolderTreeModel *model)
{
	g_return_val_if_fail (EM_IS_FOLDER_TREE_MODEL (model), NULL);

	return GTK_TREE_SELECTION (model->priv->selection);
}

void
em_folder_tree_model_set_selection (EMFolderTreeModel *model,
                                    GtkTreeSelection *selection)
{
	g_return_if_fail (EM_IS_FOLDER_TREE_MODEL (model));

	if (selection != NULL)
		g_return_if_fail (GTK_IS_TREE_SELECTION (selection));

	if (model->priv->selection != NULL) {
		g_object_weak_unref (
			G_OBJECT (model->priv->selection), (GWeakNotify)
			folder_tree_model_selection_finalized_cb, model);
		model->priv->selection = NULL;
	}

	model->priv->selection = selection;

	if (model->priv->selection != NULL)
		g_object_weak_ref (
			G_OBJECT (model->priv->selection), (GWeakNotify)
			folder_tree_model_selection_finalized_cb, model);

	g_object_notify (G_OBJECT (model), "selection");
}

EMailSession *
em_folder_tree_model_get_session (EMFolderTreeModel *model)
{
	g_return_val_if_fail (EM_IS_FOLDER_TREE_MODEL (model), NULL);

	return model->priv->session;
}

void
em_folder_tree_model_set_session (EMFolderTreeModel *model,
                                  EMailSession *session)
{
	g_return_if_fail (EM_IS_FOLDER_TREE_MODEL (model));

	if (session != NULL) {
		g_return_if_fail (E_IS_MAIL_SESSION (session));
		g_object_ref (session);
	}

	if (model->priv->session != NULL)
		g_object_unref (model->priv->session);

	model->priv->session = session;

	/* FIXME Technically we should be disconnecting this signal
	 *       when replacing an old session with a new session,
	 *       but at present this function is only called once. */
	if (session != NULL) {
		MailFolderCache *folder_cache;

		folder_cache = e_mail_session_get_folder_cache (session);

		g_signal_connect_swapped (
			folder_cache, "folder-unread-updated",
			G_CALLBACK (folder_tree_model_set_unread_count),
			model);
	}

	g_object_notify (G_OBJECT (model), "session");
}

void
em_folder_tree_model_set_folder_info (EMFolderTreeModel *model,
                                      GtkTreeIter *iter,
                                      EMFolderTreeModelStoreInfo *si,
                                      CamelFolderInfo *fi,
                                      gint fully_loaded)
{
	GtkTreeRowReference *uri_row, *path_row;
	GtkTreeStore *tree_store;
	MailFolderCache *folder_cache;
	EMailSession *session;
	EAccount *account;
	guint unread;
	GtkTreePath *path;
	GtkTreeIter sub;
	CamelFolder *folder;
	gboolean emitted = FALSE;
	const gchar *uid;
	const gchar *icon_name;
	const gchar *display_name;
	guint32 flags, add_flags = 0;
	EMEventTargetCustomIcon *target;
	gboolean load = FALSE;
	gboolean folder_is_drafts = FALSE;
	gboolean folder_is_outbox = FALSE;
	gboolean folder_is_templates = FALSE;
	gchar *uri;

	/* Make sure we don't already know about it. */
	if (g_hash_table_lookup (si->full_hash, fi->full_name))
		return;

	tree_store = GTK_TREE_STORE (model);

	session = em_folder_tree_model_get_session (model);
	folder_cache = e_mail_session_get_folder_cache (session);

	uid = camel_service_get_uid (CAMEL_SERVICE (si->store));
	account = e_get_account_by_uid (uid);

	if (!fully_loaded)
		load = (fi->child == NULL) && !(fi->flags &
			(CAMEL_FOLDER_NOCHILDREN | CAMEL_FOLDER_NOINFERIORS));

	path = gtk_tree_model_get_path (GTK_TREE_MODEL (model), iter);
	uri_row = gtk_tree_row_reference_new (GTK_TREE_MODEL (model), path);
	path_row = gtk_tree_row_reference_copy (uri_row);
	gtk_tree_path_free (path);

	uri = e_mail_folder_uri_build (si->store, fi->full_name);

	/* Hash table takes ownership of the URI string. */
	g_hash_table_insert (
		model->priv->uri_index, uri, uri_row);
	g_hash_table_insert (
		si->full_hash, g_strdup (fi->full_name), path_row);

	/* XXX If we have the folder, and its the Outbox folder, we need
	 *     the total count, not unread.  We do the same for Drafts. */

	/* XXX This is duplicated in mail-folder-cache too, should perhaps
	 *     be functionised. */
	unread = fi->unread;
	if (mail_folder_cache_get_folder_from_uri (
		folder_cache, uri, &folder) && folder) {
		folder_is_drafts = em_utils_folder_is_drafts (folder);
		folder_is_outbox = em_utils_folder_is_outbox (folder);

		if (folder_is_drafts || folder_is_outbox) {
			gint total;

			if ((total = camel_folder_get_message_count (folder)) > 0) {
				gint deleted = camel_folder_get_deleted_message_count (folder);

				if (deleted != -1)
					total -= deleted;
			}

			unread = total > 0 ? total : 0;
		}

		g_object_unref (folder);
	}

	flags = fi->flags;
	display_name = fi->display_name;

	if (si->store == e_mail_local_get_store ()) {
		if (strcmp (fi->full_name, "Drafts") == 0) {
			folder_is_drafts = TRUE;
			display_name = _("Drafts");
		} else if (strcmp (fi->full_name, "Templates") == 0) {
			folder_is_templates = TRUE;
			display_name = _("Templates");
		} else if (strcmp (fi->full_name, "Inbox") == 0) {
			flags = (flags & ~CAMEL_FOLDER_TYPE_MASK) |
				CAMEL_FOLDER_TYPE_INBOX;
			display_name = _("Inbox");
		} else if (strcmp (fi->full_name, "Outbox") == 0) {
			flags = (flags & ~CAMEL_FOLDER_TYPE_MASK) |
				CAMEL_FOLDER_TYPE_OUTBOX;
			display_name = _("Outbox");
		} else if (strcmp (fi->full_name, "Sent") == 0) {
			flags = (flags & ~CAMEL_FOLDER_TYPE_MASK) |
				CAMEL_FOLDER_TYPE_SENT;
			display_name = _("Sent");
		}
	}

	if (account != NULL && (flags & CAMEL_FOLDER_TYPE_MASK) == 0) {
		if (!folder_is_drafts && account->drafts_folder_uri != NULL) {
			folder_is_drafts = e_mail_folder_uri_equal (
				CAMEL_SESSION (session), uri,
				account->drafts_folder_uri);
		}

		if (account->sent_folder_uri != NULL) {
			if (e_mail_folder_uri_equal (
				CAMEL_SESSION (session), uri,
				account->sent_folder_uri)) {
				add_flags = CAMEL_FOLDER_TYPE_SENT;
			}
		}
	}

	/* Choose an icon name for the folder. */
	icon_name = em_folder_utils_get_icon_name (flags | add_flags);

	if (g_str_equal (icon_name, "folder")) {
		if (folder_is_drafts)
			icon_name = "accessories-text-editor";
		else if (folder_is_templates)
			icon_name = "text-x-generic-template";
	}

	gtk_tree_store_set (
		tree_store, iter,
		COL_STRING_DISPLAY_NAME, display_name,
		COL_POINTER_CAMEL_STORE, si->store,
		COL_STRING_FULL_NAME, fi->full_name,
		COL_STRING_ICON_NAME, icon_name,
		COL_STRING_URI, uri,
		COL_UINT_FLAGS, flags,
		COL_BOOL_IS_STORE, FALSE,
		COL_BOOL_IS_FOLDER, TRUE,
		COL_BOOL_LOAD_SUBDIRS, load,
		COL_UINT_UNREAD_LAST_SEL, 0,
		COL_BOOL_IS_DRAFT, folder_is_drafts,
		-1);

	target = em_event_target_new_custom_icon (
		em_event_peek (), tree_store, iter,
		fi->full_name, EM_EVENT_CUSTOM_ICON);
	e_event_emit (
		(EEvent *) em_event_peek (), "folder.customicon",
		(EEventTarget *) target);

	if (unread != ~0)
		gtk_tree_store_set (
			tree_store, iter, COL_UINT_UNREAD, unread,
			COL_UINT_UNREAD_LAST_SEL, unread, -1);

	if (load) {
		/* create a placeholder node for our subfolders... */
		gtk_tree_store_append (tree_store, &sub, iter);
		gtk_tree_store_set (
			tree_store, &sub,
			COL_STRING_DISPLAY_NAME, _("Loading..."),
			COL_POINTER_CAMEL_STORE, si->store,
			COL_STRING_FULL_NAME, NULL,
			COL_STRING_ICON_NAME, NULL,
			COL_BOOL_LOAD_SUBDIRS, FALSE,
			COL_BOOL_IS_STORE, FALSE,
			COL_BOOL_IS_FOLDER, FALSE,
			COL_STRING_URI, NULL,
			COL_UINT_UNREAD, 0,
			COL_UINT_UNREAD_LAST_SEL, 0,
			COL_BOOL_IS_DRAFT, FALSE,
			-1);

		path = gtk_tree_model_get_path (GTK_TREE_MODEL (model), iter);
		g_signal_emit (model, signals[LOADING_ROW], 0, path, iter);
		gtk_tree_path_free (path);
		return;
	}

	if (fi->child) {
		fi = fi->child;

		do {
			gtk_tree_store_append (tree_store, &sub, iter);

			if (!emitted) {
				path = gtk_tree_model_get_path (
					GTK_TREE_MODEL (model), iter);
				g_signal_emit (
					model, signals[LOADED_ROW],
					0, path, iter);
				gtk_tree_path_free (path);
				emitted = TRUE;
			}

			em_folder_tree_model_set_folder_info (
				model, &sub, si, fi, fully_loaded);
			fi = fi->next;
		} while (fi);
	}

	if (!emitted) {
		path = gtk_tree_model_get_path (GTK_TREE_MODEL (model), iter);
		g_signal_emit (model, signals[LOADED_ROW], 0, path, iter);
		gtk_tree_path_free (path);
	}
}

static void
folder_subscribed_cb (CamelStore *store,
                      CamelFolderInfo *fi,
                      EMFolderTreeModel *model)
{
	EMFolderTreeModelStoreInfo *si;
	GtkTreeRowReference *reference;
	GtkTreeIter parent, iter;
	GtkTreePath *path;
	gboolean load;
	gchar *dirname, *p;

	si = em_folder_tree_model_lookup_store_info (model, store);
	if (si == NULL)
		return;

	/* Make sure we don't already know about it? */
	if (g_hash_table_lookup (si->full_hash, fi->full_name))
		return;

	/* Get our parent folder's path. */
	dirname = g_alloca (strlen (fi->full_name)+1);
	strcpy (dirname, fi->full_name);
	p = strrchr (dirname, '/');
	if (p == NULL) {
		/* User subscribed to a toplevel folder. */
		reference = si->row;
	} else {
		*p = 0;
		reference = g_hash_table_lookup (si->full_hash, dirname);
	}

	if (!gtk_tree_row_reference_valid (reference))
		return;

	path = gtk_tree_row_reference_get_path (reference);
	gtk_tree_model_get_iter (GTK_TREE_MODEL (model), &parent, path);
	gtk_tree_path_free (path);

	/* Make sure parent's subfolders have already been loaded. */
	gtk_tree_model_get (
		GTK_TREE_MODEL (model), &parent,
		COL_BOOL_LOAD_SUBDIRS, &load, -1);
	if (load)
		return;

	gtk_tree_store_append (GTK_TREE_STORE (model), &iter, &parent);

	em_folder_tree_model_set_folder_info (model, &iter, si, fi, TRUE);
}

static void
folder_unsubscribed_cb (CamelStore *store,
                        CamelFolderInfo *fi,
                        EMFolderTreeModel *model)
{
	EMFolderTreeModelStoreInfo *si;
	GtkTreeRowReference *reference;
	GtkTreePath *path;
	GtkTreeIter iter;

	si = em_folder_tree_model_lookup_store_info (model, store);
	if (si == NULL)
		return;

	reference = g_hash_table_lookup (si->full_hash, fi->full_name);
	if (!gtk_tree_row_reference_valid (reference))
		return;

	path = gtk_tree_row_reference_get_path (reference);
	gtk_tree_model_get_iter (GTK_TREE_MODEL (model), &iter, path);
	gtk_tree_path_free (path);

	em_folder_tree_model_remove_folders (model, si, &iter);
}

static void
folder_created_cb (CamelStore *store,
                   CamelFolderInfo *fi,
                   EMFolderTreeModel *model)
{
	EMFolderTreeModelStoreInfo *si;

	/* We only want created events to do more
	 * work if we don't support subscriptions. */
	if (CAMEL_IS_SUBSCRIBABLE (store))
		return;

	/* process "folder-created" event only when store already loaded */
	si = em_folder_tree_model_lookup_store_info (model, store);
	if (si == NULL || g_hash_table_size (si->full_hash) == 0)
		return;

	folder_subscribed_cb (store, fi, model);
}

static void
folder_deleted_cb (CamelStore *store,
                   CamelFolderInfo *fi,
                   EMFolderTreeModel *model)
{
	/* We only want deleted events to do more
	 * work if we don't support subscriptions. */
	if (CAMEL_IS_SUBSCRIBABLE (store))
		return;

	folder_unsubscribed_cb (store, fi, model);
}

typedef struct {
	gchar *old_base;
	CamelFolderInfo *new;
} RenameInfo;

static void
folder_renamed_cb (CamelStore *store,
                   const gchar *old_name,
                   CamelFolderInfo *info,
                   EMFolderTreeModel *model)
{
	EMFolderTreeModelStoreInfo *si;
	GtkTreeRowReference *reference;
	GtkTreeIter root, iter;
	GtkTreePath *path;
	gchar *parent, *p;

	si = em_folder_tree_model_lookup_store_info (model, store);
	if (si == NULL)
		return;

	reference = g_hash_table_lookup (si->full_hash, old_name);
	if (!gtk_tree_row_reference_valid (reference))
		return;

	path = gtk_tree_row_reference_get_path (reference);
	gtk_tree_model_get_iter (GTK_TREE_MODEL (model), &iter, path);
	gtk_tree_path_free (path);

	em_folder_tree_model_remove_folders (model, si, &iter);

	parent = g_strdup (info->full_name);
	p = strrchr (parent, '/');
	if (p)
		*p = 0;
	if (p == NULL || parent == p)
		/* renamed to a toplevel folder on the store */
		reference = si->row;
	else
		reference = g_hash_table_lookup (si->full_hash, parent);

	g_free (parent);

	if (!gtk_tree_row_reference_valid (reference))
		return;

	path = gtk_tree_row_reference_get_path (reference);
	gtk_tree_model_get_iter (GTK_TREE_MODEL (model), &root, path);
	gtk_tree_path_free (path);

	gtk_tree_store_append (GTK_TREE_STORE (model), &iter, &root);
	em_folder_tree_model_set_folder_info (model, &iter, si, info, TRUE);
}

void
em_folder_tree_model_add_store (EMFolderTreeModel *model,
                                CamelStore *store)
{
	EMFolderTreeModelStoreInfo *si;
	GtkTreeRowReference *reference;
	GtkTreeStore *tree_store;
	GtkTreeIter root, iter;
	GtkTreePath *path;
	CamelService *service;
	CamelProvider *provider;
	CamelURL *service_url;
	const gchar *display_name;
	gchar *uri;

	g_return_if_fail (EM_IS_FOLDER_TREE_MODEL (model));
	g_return_if_fail (CAMEL_IS_STORE (store));

	tree_store = GTK_TREE_STORE (model);

	service = CAMEL_SERVICE (store);
	provider = camel_service_get_provider (service);
	service_url = camel_service_get_camel_url (service);
	display_name = camel_service_get_display_name (service);

	/* Ignore stores that should not be added to the tree model. */

	if (provider == NULL)
		return;

	if ((provider->flags & CAMEL_PROVIDER_IS_STORAGE) == 0)
		return;

	if (em_utils_is_local_delivery_mbox_file (service_url))
		return;

	si = em_folder_tree_model_lookup_store_info (model, store);
	if (si != NULL)
		em_folder_tree_model_remove_store (model, store);

	uri = camel_url_to_string (service_url, CAMEL_URL_HIDE_ALL);

	/* Add the store to the tree. */
	gtk_tree_store_append (tree_store, &iter, NULL);
	gtk_tree_store_set (
		tree_store, &iter,
		COL_STRING_DISPLAY_NAME, display_name,
		COL_POINTER_CAMEL_STORE, store,
		COL_STRING_FULL_NAME, NULL,
		COL_BOOL_LOAD_SUBDIRS, TRUE,
		COL_BOOL_IS_STORE, TRUE,
		COL_STRING_URI, uri, -1);

	path = gtk_tree_model_get_path (GTK_TREE_MODEL (model), &iter);
	reference = gtk_tree_row_reference_new (GTK_TREE_MODEL (model), path);

	si = g_new0 (EMFolderTreeModelStoreInfo, 1);
	si->store = g_object_ref (store);
	si->row = gtk_tree_row_reference_copy (reference);
	si->full_hash = g_hash_table_new_full (
		g_str_hash, g_str_equal,
		(GDestroyNotify) g_free,
		(GDestroyNotify) gtk_tree_row_reference_free);
	g_hash_table_insert (model->priv->store_index, store, si);

	/* Transfer ownership of the URI and GtkTreeRowReference. */
	g_hash_table_insert (model->priv->uri_index, uri, reference);

	/* Each store has folders, but we don't load them until
	 * the user demands them. */
	root = iter;
	gtk_tree_store_append (tree_store, &iter, &root);
	gtk_tree_store_set (
		tree_store, &iter,
		COL_STRING_DISPLAY_NAME, _("Loading..."),
		COL_POINTER_CAMEL_STORE, store,
		COL_STRING_FULL_NAME, NULL,
		COL_BOOL_LOAD_SUBDIRS, FALSE,
		COL_BOOL_IS_STORE, FALSE,
		COL_BOOL_IS_FOLDER, FALSE,
		COL_STRING_URI, NULL,
		COL_UINT_UNREAD, 0,
		COL_UINT_UNREAD_LAST_SEL, 0,
		COL_BOOL_IS_DRAFT, FALSE,
		-1);

	/* Listen to store events. */
	si->created_id = g_signal_connect (
		store, "folder-created",
		G_CALLBACK (folder_created_cb), model);
	si->deleted_id = g_signal_connect (
		store, "folder-deleted",
		G_CALLBACK (folder_deleted_cb), model);
	si->renamed_id = g_signal_connect (
		store, "folder_renamed",
		G_CALLBACK (folder_renamed_cb), model);
	if (CAMEL_IS_SUBSCRIBABLE (store)) {
		si->subscribed_id = g_signal_connect (
			store, "folder_subscribed",
			G_CALLBACK (folder_subscribed_cb), model);
		si->unsubscribed_id = g_signal_connect (
			store, "folder_unsubscribed",
			G_CALLBACK (folder_unsubscribed_cb), model);
	}

	g_signal_emit (model, signals[LOADED_ROW], 0, path, &root);
	gtk_tree_path_free (path);
}

void
em_folder_tree_model_remove_store (EMFolderTreeModel *model,
                                   CamelStore *store)
{
	EMFolderTreeModelStoreInfo *si;
	GtkTreePath *path;
	GtkTreeIter iter;

	g_return_if_fail (EM_IS_FOLDER_TREE_MODEL (model));
	g_return_if_fail (CAMEL_IS_STORE (store));

	si = em_folder_tree_model_lookup_store_info (model, store);
	if (si == NULL)
		return;

	path = gtk_tree_row_reference_get_path (si->row);
	gtk_tree_model_get_iter (GTK_TREE_MODEL (model), &iter, path);
	gtk_tree_path_free (path);

	/* recursively remove subfolders and finally the toplevel store */
	em_folder_tree_model_remove_folders (model, si, &iter);
}

GList *
em_folder_tree_model_list_stores (EMFolderTreeModel *model)
{
	g_return_val_if_fail (EM_IS_FOLDER_TREE_MODEL (model), NULL);

	return g_hash_table_get_keys (model->priv->store_index);
}

void
em_folder_tree_model_remove_folders (EMFolderTreeModel *model,
                                     EMFolderTreeModelStoreInfo *si,
                                     GtkTreeIter *toplevel)
{
	gchar *uri, *full_name;
	gboolean is_store, go;
	GtkTreeIter iter;

	if (gtk_tree_model_iter_children (GTK_TREE_MODEL (model), &iter, toplevel)) {
		do {
			GtkTreeIter next = iter;

			go = gtk_tree_model_iter_next (GTK_TREE_MODEL (model), &next);
			em_folder_tree_model_remove_folders (model, si, &iter);
			iter = next;
		} while (go);
	}

	gtk_tree_model_get (
		GTK_TREE_MODEL (model), toplevel,
		COL_STRING_URI, &uri,
		COL_STRING_FULL_NAME, &full_name,
		COL_BOOL_IS_STORE, &is_store, -1);

	if (full_name != NULL)
		g_hash_table_remove (si->full_hash, full_name);

	if (uri != NULL)
		g_hash_table_remove (model->priv->uri_index, uri);

	gtk_tree_store_remove ((GtkTreeStore *) model, toplevel);

	/* Freeing the GtkTreeRowReference in the store info may finalize
	 * the model.  Keep the model alive until the store info is fully
	 * removed from the hash table. */
	if (is_store) {
		g_object_ref (model);
		g_hash_table_remove (model->priv->store_index, si->store);
		g_object_unref (model);
	}

	g_free (full_name);
	g_free (uri);
}

gboolean
em_folder_tree_model_is_type_inbox (EMFolderTreeModel *model,
                                    CamelStore *store,
                                    const gchar *full)
{
	EMFolderTreeModelStoreInfo *si;
	GtkTreeRowReference *reference;
	GtkTreePath *path;
	GtkTreeIter iter;
	guint32 flags;

	g_return_val_if_fail (EM_IS_FOLDER_TREE_MODEL (model), FALSE);
	g_return_val_if_fail (CAMEL_IS_STORE (store), FALSE);
	g_return_val_if_fail (full != NULL, FALSE);

	si = em_folder_tree_model_lookup_store_info (model, store);
	if (si == NULL)
		return FALSE;

	reference = g_hash_table_lookup (si->full_hash, full);
	if (!gtk_tree_row_reference_valid (reference))
		return FALSE;

	path = gtk_tree_row_reference_get_path (reference);
	gtk_tree_model_get_iter (GTK_TREE_MODEL (model), &iter, path);
	gtk_tree_path_free (path);

	gtk_tree_model_get (
		GTK_TREE_MODEL (model), &iter,
		COL_UINT_FLAGS, &flags, -1);

	return ((flags & CAMEL_FOLDER_TYPE_MASK) == CAMEL_FOLDER_TYPE_INBOX);
}

gchar *
em_folder_tree_model_get_folder_name (EMFolderTreeModel *model,
                                      CamelStore *store,
                                      const gchar *full)
{
	EMFolderTreeModelStoreInfo *si;
	GtkTreeRowReference *reference;
	GtkTreePath *path;
	GtkTreeIter iter;
	gchar *name = NULL;

	g_return_val_if_fail (EM_IS_FOLDER_TREE_MODEL (model), NULL);
	g_return_val_if_fail (CAMEL_IS_STORE (store), NULL);
	g_return_val_if_fail (full != NULL, NULL);

	si = em_folder_tree_model_lookup_store_info (model, store);
	if (si == NULL)
		return NULL;

	reference = g_hash_table_lookup (si->full_hash, full);
	if (!gtk_tree_row_reference_valid (reference))
		return NULL;

	path = gtk_tree_row_reference_get_path (reference);
	gtk_tree_model_get_iter (GTK_TREE_MODEL (model), &iter, path);
	gtk_tree_path_free (path);

	gtk_tree_model_get (
		GTK_TREE_MODEL (model), &iter,
		COL_STRING_DISPLAY_NAME, &name, -1);

	return name;
}

EMFolderTreeModelStoreInfo *
em_folder_tree_model_lookup_store_info (EMFolderTreeModel *model,
                                        CamelStore *store)
{
	g_return_val_if_fail (EM_IS_FOLDER_TREE_MODEL (model), NULL);
	g_return_val_if_fail (CAMEL_IS_STORE (store), NULL);

	return g_hash_table_lookup (model->priv->store_index, store);
}

GtkTreeRowReference *
em_folder_tree_model_lookup_uri (EMFolderTreeModel *model,
                                 const gchar *folder_uri)
{
	GtkTreeRowReference *reference;

	g_return_val_if_fail (EM_IS_FOLDER_TREE_MODEL (model), NULL);
	g_return_val_if_fail (folder_uri != NULL, NULL);

	reference = g_hash_table_lookup (model->priv->uri_index, folder_uri);

	return gtk_tree_row_reference_valid (reference) ? reference : NULL;
}

void
em_folder_tree_model_user_marked_unread (EMFolderTreeModel *model,
                                         CamelFolder *folder,
                                         guint n_marked)
{
	GtkTreeRowReference *reference;
	GtkTreePath *path;
	GtkTreeIter iter;
	gchar *folder_uri;
	guint unread;

	/* The user marked messages in the given folder as unread.
	 * Update our unread counts so we don't misinterpret this
	 * event as new mail arriving. */

	g_return_if_fail (EM_IS_FOLDER_TREE_MODEL (model));
	g_return_if_fail (CAMEL_IS_FOLDER (folder));

	folder_uri = e_mail_folder_uri_from_folder (folder);
	reference = em_folder_tree_model_lookup_uri (model, folder_uri);
	g_free (folder_uri);

	g_return_if_fail (gtk_tree_row_reference_valid (reference));

	path = gtk_tree_row_reference_get_path (reference);
	gtk_tree_model_get_iter (GTK_TREE_MODEL (model), &iter, path);
	gtk_tree_path_free (path);

	gtk_tree_model_get (
		GTK_TREE_MODEL (model), &iter,
		COL_UINT_UNREAD, &unread, -1);

	unread += n_marked;

	gtk_tree_store_set (
		GTK_TREE_STORE (model), &iter,
		COL_UINT_UNREAD_LAST_SEL, unread,
		COL_UINT_UNREAD, unread, -1);
}
