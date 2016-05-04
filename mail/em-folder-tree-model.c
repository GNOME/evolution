/*
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 *
 *
 * Authors:
 *		Jeffrey Stedfast <fejj@ximian.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#include "em-folder-tree-model.h"

#include <config.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/stat.h>

#include <glib/gi18n.h>

#include <e-util/e-util.h>
#include <shell/e-shell.h>

#include "e-mail-account-store.h"
#include "e-mail-ui-session.h"
#include "em-utils.h"
#include "em-folder-utils.h"
#include "em-event.h"

#define EM_FOLDER_TREE_MODEL_GET_PRIVATE(obj) \
	(G_TYPE_INSTANCE_GET_PRIVATE \
	((obj), EM_TYPE_FOLDER_TREE_MODEL, EMFolderTreeModelPrivate))

/* See GtkCellRendererSpinner:pulse property.
 * Animation cycles over 12 frames in 750 ms. */
#define SPINNER_PULSE_INTERVAL (750 / 12)

typedef struct _StoreInfo StoreInfo;

struct _EMFolderTreeModelPrivate {
	/* This is set by EMailShellSidebar.  It allows new EMFolderTree
	 * instances to initialize their selection and expanded states to
	 * mimic the sidebar. */
	GtkTreeSelection *selection;  /* weak reference */

	EMailSession *session;
	EMailAccountStore *account_store;

	/* CamelStore -> StoreInfo */
	GHashTable *store_index;
	GMutex store_index_lock;
};

typedef struct _FolderUnreadInfo {
	guint unread;
	guint unread_last_sel;
	gboolean is_drafts;
	guint32 fi_flags;
} FolderUnreadInfo;

struct _StoreInfo {
	volatile gint ref_count;

	CamelStore *store;
	GtkTreeRowReference *row;

	/* CamelFolderInfo::full_name -> GtkTreeRowReference */
	GHashTable *full_hash;

	/* CamelFolderInfo::full_name ~> FolderUnreadInfo * - last known unread count
	   for folders which are not loaded in the tree yet */
	GHashTable *full_hash_unread;

	/* CamelStore signal handler IDs */
	gulong folder_created_handler_id;
	gulong folder_deleted_handler_id;
	gulong folder_renamed_handler_id;
	gulong folder_info_stale_handler_id;
	gulong folder_subscribed_handler_id;
	gulong folder_unsubscribed_handler_id;
	gulong connection_status_handler_id;
	gulong host_reachable_handler_id;

	/* For comparison with the current status. */
	CamelServiceConnectionStatus last_status;

	/* Spinner renderers have to be animated manually. */
	guint spinner_pulse_value;
	guint spinner_pulse_timeout_id;
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

/* Forward Declarations */
static void	folder_tree_model_folder_created_cb
						(CamelStore *store,
						 CamelFolderInfo *fi,
						 StoreInfo *si);
static void	folder_tree_model_folder_deleted_cb
						(CamelStore *store,
						 CamelFolderInfo *fi,
						 StoreInfo *si);
static void	folder_tree_model_folder_renamed_cb
						(CamelStore *store,
						 const gchar *old_name,
						 CamelFolderInfo *info,
						 StoreInfo *si);
static void	folder_tree_model_folder_info_stale_cb
						(CamelStore *store,
						 StoreInfo *si);
static void	folder_tree_model_folder_subscribed_cb
						(CamelStore *store,
						 CamelFolderInfo *fi,
						 StoreInfo *si);
static void	folder_tree_model_folder_unsubscribed_cb
						(CamelStore *store,
						 CamelFolderInfo *fi,
						 StoreInfo *si);
static void	folder_tree_model_status_notify_cb
						(CamelStore *store,
						 GParamSpec *pspec,
						 StoreInfo *si);

static guint signals[LAST_SIGNAL];

G_DEFINE_TYPE (EMFolderTreeModel, em_folder_tree_model, GTK_TYPE_TREE_STORE)

static StoreInfo *
store_info_ref (StoreInfo *si)
{
	g_return_val_if_fail (si != NULL, NULL);
	g_return_val_if_fail (si->ref_count > 0, NULL);

	g_atomic_int_inc (&si->ref_count);

	return si;
}

static void
store_info_unref (StoreInfo *si)
{
	g_return_if_fail (si != NULL);
	g_return_if_fail (si->ref_count > 0);

	if (g_atomic_int_dec_and_test (&si->ref_count)) {

		/* Check that we're fully disconnected. */
		g_warn_if_fail (si->folder_created_handler_id == 0);
		g_warn_if_fail (si->folder_deleted_handler_id == 0);
		g_warn_if_fail (si->folder_renamed_handler_id == 0);
		g_warn_if_fail (si->folder_info_stale_handler_id == 0);
		g_warn_if_fail (si->folder_subscribed_handler_id == 0);
		g_warn_if_fail (si->folder_unsubscribed_handler_id == 0);
		g_warn_if_fail (si->connection_status_handler_id == 0);
		g_warn_if_fail (si->host_reachable_handler_id == 0);
		g_warn_if_fail (si->spinner_pulse_timeout_id == 0);

		g_object_unref (si->store);
		gtk_tree_row_reference_free (si->row);
		g_hash_table_destroy (si->full_hash);
		g_hash_table_destroy (si->full_hash_unread);

		g_slice_free (StoreInfo, si);
	}
}

static StoreInfo *
store_info_new (EMFolderTreeModel *model,
                CamelStore *store)
{
	CamelService *service;
	StoreInfo *si;
	gulong handler_id;

	si = g_slice_new0 (StoreInfo);
	si->ref_count = 1;
	si->store = g_object_ref (store);

	si->full_hash = g_hash_table_new_full (
		(GHashFunc) g_str_hash,
		(GEqualFunc) g_str_equal,
		(GDestroyNotify) g_free,
		(GDestroyNotify) gtk_tree_row_reference_free);

	si->full_hash_unread = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, g_free);

	handler_id = g_signal_connect_data (
		store, "folder-created",
		G_CALLBACK (folder_tree_model_folder_created_cb),
		store_info_ref (si),
		(GClosureNotify) store_info_unref, 0);
	si->folder_created_handler_id = handler_id;

	handler_id = g_signal_connect_data (
		store, "folder-deleted",
		G_CALLBACK (folder_tree_model_folder_deleted_cb),
		store_info_ref (si),
		(GClosureNotify) store_info_unref, 0);
	si->folder_deleted_handler_id = handler_id;

	handler_id = g_signal_connect_data (
		store, "folder-renamed",
		G_CALLBACK (folder_tree_model_folder_renamed_cb),
		store_info_ref (si),
		(GClosureNotify) store_info_unref, 0);
	si->folder_renamed_handler_id = handler_id;

	handler_id = g_signal_connect_data (
		store, "folder-info-stale",
		G_CALLBACK (folder_tree_model_folder_info_stale_cb),
		store_info_ref (si),
		(GClosureNotify) store_info_unref, 0);
	si->folder_info_stale_handler_id = handler_id;

	if (CAMEL_IS_SUBSCRIBABLE (store)) {
		handler_id = g_signal_connect_data (
			store, "folder-subscribed",
			G_CALLBACK (folder_tree_model_folder_subscribed_cb),
			store_info_ref (si),
			(GClosureNotify) store_info_unref, 0);
		si->folder_subscribed_handler_id = handler_id;

		handler_id = g_signal_connect_data (
			store, "folder-unsubscribed",
			G_CALLBACK (folder_tree_model_folder_unsubscribed_cb),
			store_info_ref (si),
			(GClosureNotify) store_info_unref, 0);
		si->folder_unsubscribed_handler_id = handler_id;
	}

	if (CAMEL_IS_NETWORK_SERVICE (store)) {
		handler_id = g_signal_connect_data (
			store, "notify::connection-status",
			G_CALLBACK (folder_tree_model_status_notify_cb),
			store_info_ref (si),
			(GClosureNotify) store_info_unref, 0);
		si->connection_status_handler_id = handler_id;

		handler_id = g_signal_connect_data (
			store, "notify::host-reachable",
			G_CALLBACK (folder_tree_model_status_notify_cb),
			store_info_ref (si),
			(GClosureNotify) store_info_unref, 0);
		si->host_reachable_handler_id = handler_id;
	}

	service = CAMEL_SERVICE (store);
	si->last_status = camel_service_get_connection_status (service);

	return si;
}

static void
store_info_dispose (StoreInfo *si)
{
	g_return_if_fail (si != NULL);

	/* Disconnect all signal handlers and whatever
	 * else might be holding a StoreInfo reference. */

	if (si->folder_created_handler_id > 0) {
		g_signal_handler_disconnect (
			si->store,
			si->folder_created_handler_id);
		si->folder_created_handler_id = 0;
	}

	if (si->folder_deleted_handler_id > 0) {
		g_signal_handler_disconnect (
			si->store,
			si->folder_deleted_handler_id);
		si->folder_deleted_handler_id = 0;
	}

	if (si->folder_renamed_handler_id > 0) {
		g_signal_handler_disconnect (
			si->store,
			si->folder_renamed_handler_id);
		si->folder_renamed_handler_id = 0;
	}

	if (si->folder_info_stale_handler_id > 0) {
		g_signal_handler_disconnect (
			si->store,
			si->folder_info_stale_handler_id);
		si->folder_info_stale_handler_id = 0;
	}

	if (si->folder_subscribed_handler_id > 0) {
		g_signal_handler_disconnect (
			si->store,
			si->folder_subscribed_handler_id);
		si->folder_subscribed_handler_id = 0;
	}

	if (si->folder_unsubscribed_handler_id > 0) {
		g_signal_handler_disconnect (
			si->store,
			si->folder_unsubscribed_handler_id);
		si->folder_unsubscribed_handler_id = 0;
	}

	if (si->connection_status_handler_id > 0) {
		g_signal_handler_disconnect (
			si->store,
			si->connection_status_handler_id);
		si->connection_status_handler_id = 0;
	}

	if (si->host_reachable_handler_id > 0) {
		g_signal_handler_disconnect (
			si->store,
			si->host_reachable_handler_id);
		si->host_reachable_handler_id = 0;
	}

	if (si->spinner_pulse_timeout_id > 0) {
		g_source_remove (si->spinner_pulse_timeout_id);
		si->spinner_pulse_timeout_id = 0;
	}

	store_info_unref (si);
}

static StoreInfo *
folder_tree_model_store_index_lookup (EMFolderTreeModel *model,
                                      CamelStore *store)
{
	StoreInfo *si = NULL;

	g_return_val_if_fail (CAMEL_IS_STORE (store), NULL);

	g_mutex_lock (&model->priv->store_index_lock);

	si = g_hash_table_lookup (model->priv->store_index, store);
	if (si != NULL)
		store_info_ref (si);

	g_mutex_unlock (&model->priv->store_index_lock);

	return si;
}

static void
folder_tree_model_store_index_insert (EMFolderTreeModel *model,
                                      StoreInfo *si)
{
	g_return_if_fail (si != NULL);

	g_mutex_lock (&model->priv->store_index_lock);

	g_hash_table_insert (
		model->priv->store_index,
		si->store, store_info_ref (si));

	g_mutex_unlock (&model->priv->store_index_lock);
}

static gboolean
folder_tree_model_store_index_remove (EMFolderTreeModel *model,
                                      StoreInfo *si)
{
	gboolean removed;

	g_return_val_if_fail (si != NULL, FALSE);

	g_mutex_lock (&model->priv->store_index_lock);

	removed = g_hash_table_remove (model->priv->store_index, si->store);

	g_mutex_unlock (&model->priv->store_index_lock);

	return removed;
}

static gint
folder_tree_model_sort (GtkTreeModel *model,
                        GtkTreeIter *a,
                        GtkTreeIter *b,
                        gpointer unused)
{
	EMFolderTreeModel *folder_tree_model;
	gchar *aname, *bname;
	CamelService *service_a;
	CamelService *service_b;
	gboolean a_is_store;
	gboolean b_is_store;
	const gchar *store_uid = NULL;
	guint32 flags_a, flags_b;
	gint rv = -2;

	folder_tree_model = EM_FOLDER_TREE_MODEL (model);

	gtk_tree_model_get (
		model, a,
		COL_BOOL_IS_STORE, &a_is_store,
		COL_OBJECT_CAMEL_STORE, &service_a,
		COL_STRING_DISPLAY_NAME, &aname,
		COL_UINT_FLAGS, &flags_a,
		-1);

	gtk_tree_model_get (
		model, b,
		COL_BOOL_IS_STORE, &b_is_store,
		COL_OBJECT_CAMEL_STORE, &service_b,
		COL_STRING_DISPLAY_NAME, &bname,
		COL_UINT_FLAGS, &flags_b,
		-1);

	if (CAMEL_IS_SERVICE (service_a))
		store_uid = camel_service_get_uid (service_a);

	if (a_is_store && b_is_store) {
		rv = e_mail_account_store_compare_services (
			folder_tree_model->priv->account_store,
			service_a, service_b);

	} else if (g_strcmp0 (store_uid, E_MAIL_SESSION_VFOLDER_UID) == 0) {
		/* UNMATCHED is always last. */
		if (g_strcmp0 (aname, _("UNMATCHED")) == 0)
			rv = 1;
		else if (g_strcmp0 (bname, _("UNMATCHED")) == 0)
			rv = -1;

	} else {
		/* Inbox is always first. */
		if ((flags_a & CAMEL_FOLDER_TYPE_MASK) == CAMEL_FOLDER_TYPE_INBOX)
			rv = -1;
		else if ((flags_b & CAMEL_FOLDER_TYPE_MASK) == CAMEL_FOLDER_TYPE_INBOX)
			rv = 1;
	}

	if (rv == -2) {
		if (aname != NULL && bname != NULL)
			rv = g_utf8_collate (aname, bname);
		else if (aname == bname)
			rv = 0;
		else if (aname == NULL)
			rv = -1;
		else
			rv = 1;
	}

	g_free (aname);
	g_free (bname);

	g_clear_object (&service_a);
	g_clear_object (&service_b);

	return rv;
}

static void
folder_tree_model_remove_folders (EMFolderTreeModel *folder_tree_model,
                                  StoreInfo *si,
                                  GtkTreeIter *toplevel)
{
	GtkTreeModel *model;
	GtkTreeIter iter;
	gchar *full_name;
	gboolean is_store;
	gboolean iter_valid;

	model = GTK_TREE_MODEL (folder_tree_model);

	iter_valid = gtk_tree_model_iter_children (model, &iter, toplevel);

	while (iter_valid) {
		GtkTreeIter next = iter;

		/* Recursing will invalidate the current tree model
		 * iterator, so we have to fetch the next one first. */
		iter_valid = gtk_tree_model_iter_next (model, &next);
		folder_tree_model_remove_folders (folder_tree_model, si, &iter);
		iter = next;
	}

	gtk_tree_model_get (
		model, toplevel,
		COL_STRING_FULL_NAME, &full_name,
		COL_BOOL_IS_STORE, &is_store, -1);

	if (full_name != NULL) {
		g_hash_table_remove (si->full_hash, full_name);
		g_hash_table_remove (si->full_hash_unread, full_name);
	}

	gtk_tree_store_remove (GTK_TREE_STORE (model), toplevel);

	if (is_store)
		folder_tree_model_store_index_remove (folder_tree_model, si);

	g_free (full_name);
}

static void
folder_tree_model_service_removed (EMailAccountStore *account_store,
                                   CamelService *service,
                                   EMFolderTreeModel *folder_tree_model)
{
	em_folder_tree_model_remove_store (
		folder_tree_model, CAMEL_STORE (service));
}

static void
folder_tree_model_service_enabled (EMailAccountStore *account_store,
                                   CamelService *service,
                                   EMFolderTreeModel *folder_tree_model)
{
	em_folder_tree_model_add_store (
		folder_tree_model, CAMEL_STORE (service));
}

static void
folder_tree_model_service_disabled (EMailAccountStore *account_store,
                                    CamelService *service,
                                    EMFolderTreeModel *folder_tree_model)
{
	em_folder_tree_model_remove_store (
		folder_tree_model, CAMEL_STORE (service));
}

static void
folder_tree_model_services_reordered (EMailAccountStore *account_store,
                                      gboolean default_restored,
                                      EMFolderTreeModel *folder_tree_model)
{
	/* This forces the tree store to re-sort. */
	gtk_tree_sortable_set_default_sort_func (
		GTK_TREE_SORTABLE (folder_tree_model),
		folder_tree_model_sort, NULL, NULL);
}

static gboolean
folder_tree_model_spinner_pulse_cb (gpointer user_data)
{
	GtkTreeModel *model;
	GtkTreePath *path;
	GtkTreeIter iter;
	StoreInfo *si;

	si = (StoreInfo *) user_data;

	if (!gtk_tree_row_reference_valid (si->row))
		return G_SOURCE_REMOVE;

	path = gtk_tree_row_reference_get_path (si->row);
	model = gtk_tree_row_reference_get_model (si->row);
	gtk_tree_model_get_iter (model, &iter, path);
	gtk_tree_path_free (path);

	gtk_tree_store_set (
		GTK_TREE_STORE (model), &iter,
		COL_STATUS_SPINNER_PULSE,
		si->spinner_pulse_value++,
		-1);

	if (si->spinner_pulse_value == G_MAXUINT)
		si->spinner_pulse_value = 0;

	return G_SOURCE_CONTINUE;
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

	priv = EM_FOLDER_TREE_MODEL_GET_PRIVATE (object);

	if (priv->selection != NULL) {
		g_object_weak_unref (
			G_OBJECT (priv->selection), (GWeakNotify)
			folder_tree_model_selection_finalized_cb, object);
		priv->selection = NULL;
	}

	if (priv->session != NULL) {
		MailFolderCache *folder_cache;

		folder_cache = e_mail_session_get_folder_cache (priv->session);
		g_signal_handlers_disconnect_matched (
			folder_cache, G_SIGNAL_MATCH_DATA,
			0, 0, NULL, NULL, object);

		g_object_unref (priv->session);
		priv->session = NULL;
	}

	if (priv->account_store != NULL) {
		g_signal_handlers_disconnect_matched (
			priv->account_store, G_SIGNAL_MATCH_DATA,
			0, 0, NULL, NULL, object);
		g_object_unref (priv->account_store);
		priv->account_store = NULL;
	}

	/* Chain up to parent's dispose() method. */
	G_OBJECT_CLASS (em_folder_tree_model_parent_class)->dispose (object);
}

static void
folder_tree_model_finalize (GObject *object)
{
	EMFolderTreeModelPrivate *priv;

	priv = EM_FOLDER_TREE_MODEL_GET_PRIVATE (object);

	g_hash_table_destroy (priv->store_index);
	g_mutex_clear (&priv->store_index_lock);

	/* Chain up to parent's finalize() method. */
	G_OBJECT_CLASS (em_folder_tree_model_parent_class)->finalize (object);
}

static void
folder_tree_model_constructed (GObject *object)
{
	GType col_types[] = {
		G_TYPE_STRING,    /* display name */
		CAMEL_TYPE_STORE, /* CamelStore */
		G_TYPE_STRING,    /* full name */
		G_TYPE_STRING,    /* icon name */
		G_TYPE_UINT,      /* unread count */
		G_TYPE_UINT,      /* flags */
		G_TYPE_BOOLEAN,   /* is a store node */
		G_TYPE_BOOLEAN,   /* is a folder node */
		G_TYPE_BOOLEAN,   /* has not-yet-loaded subfolders */
		G_TYPE_UINT,      /* last known unread count */
		G_TYPE_BOOLEAN,   /* folder is a draft folder */
		G_TYPE_ICON,      /* status GIcon */
		G_TYPE_BOOLEAN,   /* status icon visible */
		G_TYPE_UINT,      /* status spinner pulse */
		G_TYPE_BOOLEAN,   /* status spinner visible */
	};

	gtk_tree_store_set_column_types (
		GTK_TREE_STORE (object), NUM_COLUMNS, col_types);
	gtk_tree_sortable_set_default_sort_func (
		GTK_TREE_SORTABLE (object),
		folder_tree_model_sort, NULL, NULL);
	gtk_tree_sortable_set_sort_column_id (
		GTK_TREE_SORTABLE (object),
		GTK_TREE_SORTABLE_DEFAULT_SORT_COLUMN_ID,
		GTK_SORT_ASCENDING);

	/* Chain up to parent's constructed() method. */
	G_OBJECT_CLASS (em_folder_tree_model_parent_class)->constructed (object);
}

static void
em_folder_tree_model_class_init (EMFolderTreeModelClass *class)
{
	GObjectClass *object_class;

	g_type_class_add_private (class, sizeof (EMFolderTreeModelPrivate));

	object_class = G_OBJECT_CLASS (class);
	object_class->set_property = folder_tree_model_set_property;
	object_class->get_property = folder_tree_model_get_property;
	object_class->dispose = folder_tree_model_dispose;
	object_class->finalize = folder_tree_model_finalize;
	object_class->constructed = folder_tree_model_constructed;

	g_object_class_install_property (
		object_class,
		PROP_SESSION,
		g_param_spec_object (
			"session",
			NULL,
			NULL,
			E_TYPE_MAIL_SESSION,
			G_PARAM_READWRITE));

	g_object_class_install_property (
		object_class,
		PROP_SELECTION,
		g_param_spec_object (
			"selection",
			"Selection",
			NULL,
			GTK_TYPE_TREE_SELECTION,
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
                                    gint unread,
				    MailFolderCache *folder_cache)
{
	GtkTreeRowReference *reference;
	GtkTreeModel *tree_model;
	GtkTreePath *path;
	GtkTreeIter parent;
	GtkTreeIter iter;
	StoreInfo *si;
	guint old_unread = 0;

	g_return_if_fail (EM_IS_FOLDER_TREE_MODEL (model));
	g_return_if_fail (CAMEL_IS_STORE (store));
	g_return_if_fail (full != NULL);

	if (unread < 0)
		return;

	si = folder_tree_model_store_index_lookup (model, store);
	if (si == NULL)
		return;

	tree_model = GTK_TREE_MODEL (model);

	reference = g_hash_table_lookup (si->full_hash, full);
	if (!gtk_tree_row_reference_valid (reference)) {
		FolderUnreadInfo *fu_info;

		fu_info = g_new0 (FolderUnreadInfo, 1);
		fu_info->unread = unread;
		fu_info->unread_last_sel = unread;
		fu_info->is_drafts = FALSE;

		if (g_hash_table_contains (si->full_hash_unread, full)) {
			FolderUnreadInfo *saved_fu_info;

			saved_fu_info = g_hash_table_lookup (si->full_hash_unread, full);

			fu_info->unread_last_sel = MIN (saved_fu_info->unread_last_sel, unread);
			fu_info->is_drafts = saved_fu_info->is_drafts;
			fu_info->fi_flags = saved_fu_info->fi_flags;
		} else {
			CamelFolder *folder;
			CamelFolderInfoFlags flags;

			fu_info->unread_last_sel = unread;

			folder = mail_folder_cache_ref_folder (folder_cache, store, full);
			if (folder) {
				fu_info->is_drafts = em_utils_folder_is_drafts (e_mail_session_get_registry (model->priv->session), folder);
				g_object_unref (folder);
			}

			if (!mail_folder_cache_get_folder_info_flags (folder_cache, store, full, &flags))
				flags = 0;

			fu_info->fi_flags = flags;
		}

		g_hash_table_insert (si->full_hash_unread, g_strdup (full), fu_info);

		goto exit;
	}

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

exit:
	store_info_unref (si);
}

static void
em_folder_tree_model_init (EMFolderTreeModel *model)
{
	GHashTable *store_index;

	store_index = g_hash_table_new_full (
		(GHashFunc) g_direct_hash,
		(GEqualFunc) g_direct_equal,
		(GDestroyNotify) NULL,
		(GDestroyNotify) store_info_dispose);

	model->priv = EM_FOLDER_TREE_MODEL_GET_PRIVATE (model);
	model->priv->store_index = store_index;

	g_mutex_init (&model->priv->store_index_lock);
}

EMFolderTreeModel *
em_folder_tree_model_new (void)
{
	return g_object_new (EM_TYPE_FOLDER_TREE_MODEL, NULL);
}

static EMFolderTreeModel *
em_folder_tree_manage_default (gboolean do_create)
{
	static EMFolderTreeModel *default_folder_tree_model = NULL;

	if (do_create && G_UNLIKELY (default_folder_tree_model == NULL)) {
		default_folder_tree_model = em_folder_tree_model_new ();
	} else if (!do_create && G_UNLIKELY (default_folder_tree_model != NULL)) {
		/* This is necessary, due to circular dependency between stored GtkTreeRwoReference
		   and the model itself. */
		g_mutex_lock (&default_folder_tree_model->priv->store_index_lock);
		g_hash_table_remove_all (default_folder_tree_model->priv->store_index);
		g_mutex_unlock (&default_folder_tree_model->priv->store_index_lock);

		g_object_unref (default_folder_tree_model);
		default_folder_tree_model = NULL;
	}

	return default_folder_tree_model;
}

EMFolderTreeModel *
em_folder_tree_model_get_default (void)
{
	return em_folder_tree_manage_default (TRUE);
}

void
em_folder_tree_model_free_default (void)
{
	em_folder_tree_manage_default (FALSE);
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

	if (model->priv->selection == selection)
		return;

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

	if (model->priv->session == session)
		return;

	if (session != NULL) {
		g_return_if_fail (E_IS_MAIL_SESSION (session));
		g_object_ref (session);
	}

	if (model->priv->session != NULL) {
		MailFolderCache *folder_cache;

		folder_cache = e_mail_session_get_folder_cache (model->priv->session);
		g_signal_handlers_disconnect_matched (
			folder_cache, G_SIGNAL_MATCH_DATA,
			0, 0, NULL, NULL, model);

		g_object_unref (model->priv->session);
	}

	model->priv->session = session;

	/* FIXME Technically we should be disconnecting this signal
	 *       when replacing an old session with a new session,
	 *       but at present this function is only called once. */
	if (session != NULL) {
		EMailAccountStore *account_store;
		MailFolderCache *folder_cache;

		folder_cache = e_mail_session_get_folder_cache (session);
		account_store = e_mail_ui_session_get_account_store (
			E_MAIL_UI_SESSION (session));

		/* Keep our own reference since we connect to its signals. */
		g_warn_if_fail (model->priv->account_store == NULL);
		model->priv->account_store = g_object_ref (account_store);

		/* No need to connect to "service-added" emissions since it's
		 * always immediately followed by either "service-enabled" or
		 * "service-disabled". */

		g_signal_connect (
			account_store, "service-removed",
			G_CALLBACK (folder_tree_model_service_removed),
			model);

		g_signal_connect (
			account_store, "service-enabled",
			G_CALLBACK (folder_tree_model_service_enabled),
			model);

		g_signal_connect (
			account_store, "service-disabled",
			G_CALLBACK (folder_tree_model_service_disabled),
			model);

		g_signal_connect (
			account_store, "services-reordered",
			G_CALLBACK (folder_tree_model_services_reordered),
			model);

		g_signal_connect_swapped (
			folder_cache, "folder-unread-updated",
			G_CALLBACK (folder_tree_model_set_unread_count),
			model);
	}

	g_object_notify (G_OBJECT (model), "session");
}

static void
folder_tree_model_get_special_folders_uri (ESourceRegistry *registry,
					   CamelStore *store,
					   gchar **drafts_folder_uri,
					   gchar **templates_folder_uri,
					   gchar **sent_folder_uri)
{
	ESource *source;
	const gchar *extension_name;

	/* In case we fail... */
	*drafts_folder_uri = NULL;
	*templates_folder_uri = NULL;
	*sent_folder_uri = NULL;

	source = em_utils_ref_mail_identity_for_store (registry, store);
	if (source == NULL)
		return;

	extension_name = E_SOURCE_EXTENSION_MAIL_COMPOSITION;
	if (e_source_has_extension (source, extension_name)) {
		ESourceMailComposition *extension;

		extension = e_source_get_extension (source, extension_name);

		*drafts_folder_uri = e_source_mail_composition_dup_drafts_folder (extension);
		*templates_folder_uri = e_source_mail_composition_dup_templates_folder (extension);
	}

	extension_name = E_SOURCE_EXTENSION_MAIL_SUBMISSION;
	if (e_source_has_extension (source, extension_name)) {
		ESourceMailSubmission *extension;

		extension = e_source_get_extension (source, extension_name);

		*sent_folder_uri = e_source_mail_submission_dup_sent_folder (extension);
	}

	g_object_unref (source);
}

void
em_folder_tree_model_set_folder_info (EMFolderTreeModel *model,
                                      GtkTreeIter *iter,
                                      CamelStore *store,
                                      CamelFolderInfo *fi,
                                      gint fully_loaded)
{
	GtkTreeRowReference *path_row;
	GtkTreeStore *tree_store;
	MailFolderCache *folder_cache;
	ESourceRegistry *registry;
	EMailSession *session;
	guint unread;
	GtkTreePath *path;
	GtkTreeIter sub;
	CamelFolder *folder;
	StoreInfo *si;
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
	gboolean store_is_local;
	gchar *uri;

	g_return_if_fail (EM_IS_FOLDER_TREE_MODEL (model));
	g_return_if_fail (iter != NULL);
	g_return_if_fail (CAMEL_IS_STORE (store));
	g_return_if_fail (fi != NULL);

	si = folder_tree_model_store_index_lookup (model, store);
	g_return_if_fail (si != NULL);

	/* Make sure we don't already know about it. */
	if (g_hash_table_lookup (si->full_hash, fi->full_name)) {
		store_info_unref (si);
		return;
	}

	tree_store = GTK_TREE_STORE (model);

	session = em_folder_tree_model_get_session (model);
	folder_cache = e_mail_session_get_folder_cache (session);
	registry = e_mail_session_get_registry (session);

	uid = camel_service_get_uid (CAMEL_SERVICE (store));
	store_is_local = (g_strcmp0 (uid, E_MAIL_SESSION_LOCAL_UID) == 0);

	if (!fully_loaded)
		load = (fi->child == NULL) && !(fi->flags &
			(CAMEL_FOLDER_NOCHILDREN | CAMEL_FOLDER_NOINFERIORS));

	path = gtk_tree_model_get_path (GTK_TREE_MODEL (model), iter);
	path_row = gtk_tree_row_reference_new (GTK_TREE_MODEL (model), path);
	gtk_tree_path_free (path);

	uri = e_mail_folder_uri_build (store, fi->full_name);

	g_hash_table_insert (
		si->full_hash, g_strdup (fi->full_name), path_row);

	g_hash_table_remove (si->full_hash_unread, fi->full_name);

	store_info_unref (si);
	si = NULL;

	/* XXX If we have the folder, and its the Outbox folder, we need
	 *     the total count, not unread.  We do the same for Drafts. */

	/* XXX This is duplicated in mail-folder-cache too, should perhaps
	 *     be functionised. */
	unread = fi->unread;
	folder = mail_folder_cache_ref_folder (
		folder_cache, store, fi->full_name);
	if (folder != NULL) {
		folder_is_drafts = em_utils_folder_is_drafts (registry, folder);
		folder_is_outbox = em_utils_folder_is_outbox (registry, folder);

		if (folder_is_drafts || folder_is_outbox) {
			gint total;
			gint deleted;

			total = camel_folder_get_message_count (folder);
			deleted = camel_folder_get_deleted_message_count (folder);

			if (total > 0 && deleted != -1)
				total -= deleted;

			unread = MAX (total, 0);
		}

		g_object_unref (folder);
	}

	flags = fi->flags;
	display_name = fi->display_name;

	if (store_is_local) {
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

	if ((flags & CAMEL_FOLDER_TYPE_MASK) == 0) {
		gchar *drafts_folder_uri;
		gchar *templates_folder_uri;
		gchar *sent_folder_uri;

		folder_tree_model_get_special_folders_uri (registry, store,
			&drafts_folder_uri, &templates_folder_uri, &sent_folder_uri);

		if (!folder_is_drafts && drafts_folder_uri != NULL) {
			folder_is_drafts = e_mail_folder_uri_equal (
				CAMEL_SESSION (session),
				uri, drafts_folder_uri);
		}

		if (!folder_is_templates && templates_folder_uri != NULL) {
			folder_is_templates = e_mail_folder_uri_equal (
				CAMEL_SESSION (session),
				uri, templates_folder_uri);
		}

		if (sent_folder_uri != NULL) {
			if (e_mail_folder_uri_equal (
				CAMEL_SESSION (session),
				uri, sent_folder_uri)) {
				add_flags = CAMEL_FOLDER_TYPE_SENT;
			}
		}

		g_free (drafts_folder_uri);
		g_free (templates_folder_uri);
		g_free (sent_folder_uri);
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
		COL_OBJECT_CAMEL_STORE, store,
		COL_STRING_FULL_NAME, fi->full_name,
		COL_STRING_ICON_NAME, icon_name,
		COL_UINT_FLAGS, flags,
		COL_BOOL_IS_STORE, FALSE,
		COL_BOOL_IS_FOLDER, TRUE,
		COL_BOOL_LOAD_SUBDIRS, load,
		COL_UINT_UNREAD_LAST_SEL, 0,
		COL_BOOL_IS_DRAFT, folder_is_drafts,
		-1);

	g_free (uri);
	uri = NULL;

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
			COL_OBJECT_CAMEL_STORE, store,
			COL_STRING_FULL_NAME, NULL,
			COL_STRING_ICON_NAME, NULL,
			COL_BOOL_LOAD_SUBDIRS, FALSE,
			COL_BOOL_IS_STORE, FALSE,
			COL_BOOL_IS_FOLDER, FALSE,
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
				model, &sub, store, fi, fully_loaded);
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
folder_tree_model_folder_created_cb (CamelStore *store,
                                     CamelFolderInfo *fi,
                                     StoreInfo *si)
{
	/* We only want created events to do more
	 * work if we don't support subscriptions. */
	if (CAMEL_IS_SUBSCRIBABLE (store))
		return;

	if (g_hash_table_size (si->full_hash) > 0)
		folder_tree_model_folder_subscribed_cb (store, fi, si);
}

static void
folder_tree_model_folder_deleted_cb (CamelStore *store,
                                     CamelFolderInfo *fi,
                                     StoreInfo *si)
{
	/* We only want deleted events to do more
	 * work if we don't support subscriptions. */
	if (CAMEL_IS_SUBSCRIBABLE (store))
		return;

	folder_tree_model_folder_unsubscribed_cb (store, fi, si);
}

static void
folder_tree_model_folder_renamed_cb (CamelStore *store,
                                     const gchar *old_name,
                                     CamelFolderInfo *info,
                                     StoreInfo *si)
{
	GtkTreeRowReference *reference;
	GtkTreeModel *model;
	GtkTreeIter root, iter;
	GtkTreePath *path;
	gchar *parent, *p;

	reference = g_hash_table_lookup (si->full_hash, old_name);
	if (!gtk_tree_row_reference_valid (reference))
		return;

	path = gtk_tree_row_reference_get_path (reference);
	model = gtk_tree_row_reference_get_model (reference);
	gtk_tree_model_get_iter (model, &iter, path);
	gtk_tree_path_free (path);

	folder_tree_model_remove_folders (
		EM_FOLDER_TREE_MODEL (model), si, &iter);

	/* Make sure we don't already have the new folder name. */
	reference = g_hash_table_lookup (si->full_hash, info->full_name);
	if (gtk_tree_row_reference_valid (reference))
		return;

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
	gtk_tree_model_get_iter (model, &root, path);
	gtk_tree_path_free (path);

	gtk_tree_store_append (GTK_TREE_STORE (model), &iter, &root);
	em_folder_tree_model_set_folder_info (
		EM_FOLDER_TREE_MODEL (model), &iter, store, info, TRUE);
}

static void
folder_tree_model_folder_info_stale_cb (CamelStore *store,
                                        StoreInfo *si)
{
	GtkTreeModel *model;

	if (!gtk_tree_row_reference_valid (si->row))
		return;

	model = gtk_tree_row_reference_get_model (si->row);

	/* Re-add the store.  The StoreInfo instance will be
	 * discarded and the folder tree will be reconstructed. */
	em_folder_tree_model_add_store (EM_FOLDER_TREE_MODEL (model), store);
}

static void
folder_tree_model_folder_subscribed_cb (CamelStore *store,
                                        CamelFolderInfo *fi,
                                        StoreInfo *si)
{
	GtkTreeRowReference *reference;
	GtkTreeModel *model;
	GtkTreeIter parent, iter;
	GtkTreePath *path;
	gboolean load;
	gchar *dirname, *p;

	/* Make sure we don't already know about it? */
	if (g_hash_table_contains (si->full_hash, fi->full_name))
		return;

	/* Get our parent folder's path. */
	dirname = g_alloca (strlen (fi->full_name) + 1);
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
	model = gtk_tree_row_reference_get_model (reference);
	gtk_tree_model_get_iter (model, &parent, path);
	gtk_tree_path_free (path);

	/* Make sure parent's subfolders have already been loaded. */
	gtk_tree_model_get (
		model, &parent,
		COL_BOOL_LOAD_SUBDIRS, &load, -1);
	if (load)
		return;

	gtk_tree_store_append (GTK_TREE_STORE (model), &iter, &parent);

	em_folder_tree_model_set_folder_info (
		EM_FOLDER_TREE_MODEL (model), &iter, store, fi,
		(fi->flags & (CAMEL_FOLDER_NOINFERIORS | CAMEL_FOLDER_NOCHILDREN)) != 0);
}

static void
folder_tree_model_folder_unsubscribed_cb (CamelStore *store,
                                          CamelFolderInfo *fi,
                                          StoreInfo *si)
{
	GtkTreeRowReference *reference;
	GtkTreeModel *model;
	GtkTreePath *path;
	GtkTreeIter iter;

	reference = g_hash_table_lookup (si->full_hash, fi->full_name);
	if (!gtk_tree_row_reference_valid (reference))
		return;

	path = gtk_tree_row_reference_get_path (reference);
	model = gtk_tree_row_reference_get_model (reference);
	gtk_tree_model_get_iter (model, &iter, path);
	gtk_tree_path_free (path);

	folder_tree_model_remove_folders (
		EM_FOLDER_TREE_MODEL (model), si, &iter);
}

static void
folder_tree_model_update_status_icon (StoreInfo *si)
{
	CamelService *service;
	CamelServiceConnectionStatus status;
	GtkTreeModel *model;
	GtkTreePath *path;
	GtkTreeIter iter;
	GIcon *icon = NULL;
	const gchar *icon_name = NULL;
	gboolean was_connecting;
	gboolean host_reachable;

	g_return_if_fail (si != NULL);

	if (!gtk_tree_row_reference_valid (si->row))
		return;

	service = CAMEL_SERVICE (si->store);
	status = camel_service_get_connection_status (service);
	was_connecting = (si->last_status == CAMEL_SERVICE_CONNECTING);
	si->last_status = status;

	host_reachable = camel_network_service_get_host_reachable (
		CAMEL_NETWORK_SERVICE (service));

	switch (status) {
		case CAMEL_SERVICE_DISCONNECTED:
			if (!host_reachable)
				icon_name = "network-no-route-symbolic";
			else if (was_connecting)
				icon_name = "network-error-symbolic";
			else
				icon_name = "network-offline-symbolic";
			break;

		case CAMEL_SERVICE_CONNECTING:
			icon_name = NULL;
			break;

		case CAMEL_SERVICE_CONNECTED:
			icon_name = "network-idle-symbolic";
			break;

		case CAMEL_SERVICE_DISCONNECTING:
			icon_name = NULL;
			break;
	}

	if (icon_name == NULL && si->spinner_pulse_timeout_id == 0) {
		si->spinner_pulse_timeout_id = g_timeout_add_full (
			G_PRIORITY_DEFAULT,
			SPINNER_PULSE_INTERVAL,
			folder_tree_model_spinner_pulse_cb,
			store_info_ref (si),
			(GDestroyNotify) store_info_unref);
	}

	if (icon_name != NULL && si->spinner_pulse_timeout_id > 0) {
		g_source_remove (si->spinner_pulse_timeout_id);
		si->spinner_pulse_timeout_id = 0;
	}

	path = gtk_tree_row_reference_get_path (si->row);
	model = gtk_tree_row_reference_get_model (si->row);
	gtk_tree_model_get_iter (model, &iter, path);
	gtk_tree_path_free (path);

	if (icon_name != NULL) {
		/* Use fallbacks if symbolic icons are not available. */
		icon = g_themed_icon_new_with_default_fallbacks (icon_name);
	}

	gtk_tree_store_set (
		GTK_TREE_STORE (model), &iter,
		COL_STATUS_ICON, icon,
		COL_STATUS_ICON_VISIBLE, (icon_name != NULL),
		COL_STATUS_SPINNER_VISIBLE, (icon_name == NULL),
		-1);

	g_clear_object (&icon);

}

static void
folder_tree_model_status_notify_cb (CamelStore *store,
                                    GParamSpec *pspec,
                                    StoreInfo *si)
{
	/* Even though this is a GObject::notify signal, CamelService
	 * always emits it from its GMainContext on the "main" thread,
	 * so it's safe to modify the GtkTreeStore from here. */

	folder_tree_model_update_status_icon (si);
}

void
em_folder_tree_model_add_store (EMFolderTreeModel *model,
                                CamelStore *store)
{
	GtkTreeRowReference *reference;
	GtkTreeStore *tree_store;
	GtkTreeIter root, iter;
	GtkTreePath *path;
	CamelService *service;
	CamelProvider *provider;
	StoreInfo *si;
	const gchar *display_name;

	g_return_if_fail (EM_IS_FOLDER_TREE_MODEL (model));
	g_return_if_fail (CAMEL_IS_STORE (store));

	tree_store = GTK_TREE_STORE (model);

	service = CAMEL_SERVICE (store);
	provider = camel_service_get_provider (service);
	display_name = camel_service_get_display_name (service);

	/* Ignore stores that should not be added to the tree model. */

	if (provider == NULL)
		return;

	if ((provider->flags & CAMEL_PROVIDER_IS_STORAGE) == 0)
		return;

	if (em_utils_is_local_delivery_mbox_file (service))
		return;

	si = folder_tree_model_store_index_lookup (model, store);
	if (si != NULL) {
		em_folder_tree_model_remove_store (model, store);
		store_info_unref (si);
	}

	/* Add the store to the tree. */
	gtk_tree_store_append (tree_store, &iter, NULL);
	gtk_tree_store_set (
		tree_store, &iter,
		COL_STRING_DISPLAY_NAME, display_name,
		COL_OBJECT_CAMEL_STORE, store,
		COL_STRING_FULL_NAME, NULL,
		COL_BOOL_LOAD_SUBDIRS, TRUE,
		COL_BOOL_IS_STORE, TRUE,
		-1);

	path = gtk_tree_model_get_path (GTK_TREE_MODEL (model), &iter);
	reference = gtk_tree_row_reference_new (GTK_TREE_MODEL (model), path);

	si = store_info_new (model, store);
	si->row = reference;  /* takes ownership */

	folder_tree_model_store_index_insert (model, si);

	/* Each store has folders, but we don't load them until
	 * the user demands them. */
	root = iter;
	gtk_tree_store_append (tree_store, &iter, &root);
	gtk_tree_store_set (
		tree_store, &iter,
		COL_STRING_DISPLAY_NAME, _("Loading..."),
		COL_OBJECT_CAMEL_STORE, store,
		COL_STRING_FULL_NAME, NULL,
		COL_BOOL_LOAD_SUBDIRS, FALSE,
		COL_BOOL_IS_STORE, FALSE,
		COL_BOOL_IS_FOLDER, FALSE,
		COL_UINT_UNREAD, 0,
		COL_UINT_UNREAD_LAST_SEL, 0,
		COL_BOOL_IS_DRAFT, FALSE,
		-1);

	if (CAMEL_IS_NETWORK_SERVICE (store))
		folder_tree_model_update_status_icon (si);

	g_signal_emit (model, signals[LOADED_ROW], 0, path, &root);
	gtk_tree_path_free (path);

	store_info_unref (si);
}

void
em_folder_tree_model_remove_store (EMFolderTreeModel *model,
                                   CamelStore *store)
{
	GtkTreePath *path;
	GtkTreeIter iter;
	StoreInfo *si;

	g_return_if_fail (EM_IS_FOLDER_TREE_MODEL (model));
	g_return_if_fail (CAMEL_IS_STORE (store));

	si = folder_tree_model_store_index_lookup (model, store);
	if (si == NULL)
		return;

	path = gtk_tree_row_reference_get_path (si->row);
	gtk_tree_model_get_iter (GTK_TREE_MODEL (model), &iter, path);
	gtk_tree_path_free (path);

	/* recursively remove subfolders and finally the toplevel store */
	folder_tree_model_remove_folders (model, si, &iter);

	store_info_unref (si);
}

/* This is necessary, because of circular dependency between the model
   and its row references */
void
em_folder_tree_model_remove_all_stores (EMFolderTreeModel *model)
{
	GList *list, *link;

	g_return_if_fail (EM_IS_FOLDER_TREE_MODEL (model));

	g_mutex_lock (&model->priv->store_index_lock);
	list = g_hash_table_get_keys (model->priv->store_index);
	g_list_foreach (list, (GFunc) g_object_ref, NULL);
	g_mutex_unlock (&model->priv->store_index_lock);

	for (link = list; link; link = g_list_next (link)) {
		CamelStore *store = link->data;

		em_folder_tree_model_remove_store (model, store);
	}

	g_list_free_full (list, g_object_unref);
}

GList *
em_folder_tree_model_list_stores (EMFolderTreeModel *model)
{
	GList *list;

	g_return_val_if_fail (EM_IS_FOLDER_TREE_MODEL (model), NULL);

	g_mutex_lock (&model->priv->store_index_lock);

	list = g_hash_table_get_keys (model->priv->store_index);

	/* FIXME Listed CamelStores should be referenced here. */

	g_mutex_unlock (&model->priv->store_index_lock);

	return list;
}

gboolean
em_folder_tree_model_is_type_inbox (EMFolderTreeModel *model,
                                    CamelStore *store,
                                    const gchar *full)
{
	GtkTreeRowReference *reference;
	StoreInfo *si;
	guint32 flags = 0;

	g_return_val_if_fail (EM_IS_FOLDER_TREE_MODEL (model), FALSE);
	g_return_val_if_fail (CAMEL_IS_STORE (store), FALSE);
	g_return_val_if_fail (full != NULL, FALSE);

	si = folder_tree_model_store_index_lookup (model, store);
	if (si == NULL)
		return FALSE;

	reference = g_hash_table_lookup (si->full_hash, full);

	if (gtk_tree_row_reference_valid (reference)) {
		GtkTreePath *path;
		GtkTreeIter iter;

		path = gtk_tree_row_reference_get_path (reference);
		gtk_tree_model_get_iter (GTK_TREE_MODEL (model), &iter, path);
		gtk_tree_path_free (path);

		gtk_tree_model_get (
			GTK_TREE_MODEL (model), &iter,
			COL_UINT_FLAGS, &flags, -1);
	}

	store_info_unref (si);

	return ((flags & CAMEL_FOLDER_TYPE_MASK) == CAMEL_FOLDER_TYPE_INBOX);
}

gchar *
em_folder_tree_model_get_folder_name (EMFolderTreeModel *model,
                                      CamelStore *store,
                                      const gchar *full)
{
	GtkTreeRowReference *reference;
	GtkTreePath *path;
	GtkTreeIter iter;
	StoreInfo *si;
	gchar *name = NULL;

	g_return_val_if_fail (EM_IS_FOLDER_TREE_MODEL (model), NULL);
	g_return_val_if_fail (CAMEL_IS_STORE (store), NULL);
	g_return_val_if_fail (full != NULL, NULL);

	si = folder_tree_model_store_index_lookup (model, store);
	if (si == NULL) {
		name = g_strdup (full);
		goto exit;
	}

	reference = g_hash_table_lookup (si->full_hash, full);
	if (!gtk_tree_row_reference_valid (reference)) {
		name = g_strdup (full);
		goto exit;
	}

	path = gtk_tree_row_reference_get_path (reference);
	gtk_tree_model_get_iter (GTK_TREE_MODEL (model), &iter, path);
	gtk_tree_path_free (path);

	gtk_tree_model_get (
		GTK_TREE_MODEL (model), &iter,
		COL_STRING_DISPLAY_NAME, &name, -1);

exit:
	if (si != NULL)
		store_info_unref (si);

	return name;
}

/**
 * em_folder_tree_model_get_row_reference:
 * @model: an #EMFolderTreeModel
 * @store: a #CamelStore
 * @folder_name: a folder name, or %NULL
 *
 * Returns the #GtkTreeRowReference for the folder described by @store and
 * @folder_name.  If @folder_name is %NULL, returns the #GtkTreeRowReference
 * for the @store itself.  If no matching row is found, the function returns
 * %NULL.
 *
 * Returns: a valid #GtkTreeRowReference, or %NULL
 **/
GtkTreeRowReference *
em_folder_tree_model_get_row_reference (EMFolderTreeModel *model,
                                        CamelStore *store,
                                        const gchar *folder_name)
{
	GtkTreeRowReference *reference = NULL;
	StoreInfo *si;

	g_return_val_if_fail (EM_IS_FOLDER_TREE_MODEL (model), NULL);
	g_return_val_if_fail (CAMEL_IS_STORE (store), NULL);

	si = folder_tree_model_store_index_lookup (model, store);
	if (si == NULL)
		return NULL;

	if (folder_name != NULL)
		reference = g_hash_table_lookup (si->full_hash, folder_name);
	else
		reference = si->row;

	if (!gtk_tree_row_reference_valid (reference))
		reference = NULL;

	store_info_unref (si);

	return reference;
}

void
em_folder_tree_model_user_marked_unread (EMFolderTreeModel *model,
                                         CamelFolder *folder,
                                         guint n_marked)
{
	GtkTreeRowReference *reference;
	GtkTreePath *path;
	GtkTreeIter iter;
	CamelStore *parent_store;
	const gchar *folder_name;
	guint unread;

	/* The user marked messages in the given folder as unread.
	 * Update our unread counts so we don't misinterpret this
	 * event as new mail arriving. */

	g_return_if_fail (EM_IS_FOLDER_TREE_MODEL (model));
	g_return_if_fail (CAMEL_IS_FOLDER (folder));

	parent_store = camel_folder_get_parent_store (folder);
	folder_name = camel_folder_get_full_name (folder);

	reference = em_folder_tree_model_get_row_reference (
		model, parent_store, folder_name);

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

static gboolean
folder_tree_model_eval_children_has_unread_mismatch (GtkTreeModel *model,
						     GtkTreeIter *root)
{
	guint unread, unread_last_sel;
	GtkTreeIter iter;

	if (!gtk_tree_model_iter_children (model, &iter, root))
		return FALSE;

	do {
		gtk_tree_model_get (model, &iter,
			COL_UINT_UNREAD, &unread,
			COL_UINT_UNREAD_LAST_SEL, &unread_last_sel,
			-1);

		if (unread != ~0 && unread > unread_last_sel)
			return TRUE;

		if (gtk_tree_model_iter_has_child (model, &iter))
			if (folder_tree_model_eval_children_has_unread_mismatch (model, &iter))
				return TRUE;
	} while (gtk_tree_model_iter_next (model, &iter));

	return FALSE;
}

gboolean
em_folder_tree_model_has_unread_mismatch (GtkTreeModel *model,
					  GtkTreeIter *store_iter)
{
	StoreInfo *si;
	CamelStore *store = NULL;
	gboolean is_store = FALSE;
	gboolean has_unread_mismatch = FALSE;

	g_return_val_if_fail (EM_IS_FOLDER_TREE_MODEL (model), FALSE);
	g_return_val_if_fail (store_iter != NULL, FALSE);

	gtk_tree_model_get (model, store_iter,
		COL_BOOL_IS_STORE, &is_store,
		COL_OBJECT_CAMEL_STORE, &store,
		-1);

	if (is_store) {
		si = folder_tree_model_store_index_lookup (EM_FOLDER_TREE_MODEL (model), store);
		if (si) {
			GHashTableIter hash_iter;
			gpointer value;

			g_hash_table_iter_init (&hash_iter, si->full_hash_unread);
			while (g_hash_table_iter_next (&hash_iter, NULL, &value)) {
				FolderUnreadInfo *fu_info = value;

				if (fu_info && !fu_info->is_drafts && (fu_info->fi_flags & CAMEL_FOLDER_VIRTUAL) == 0 &&
				    fu_info->unread > fu_info->unread_last_sel) {
					has_unread_mismatch = TRUE;
					break;
				}
			}

			store_info_unref (si);
		}

		has_unread_mismatch = has_unread_mismatch ||
			folder_tree_model_eval_children_has_unread_mismatch (model, store_iter);
	}

	g_clear_object (&store);

	return has_unread_mismatch;
}
