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

#include "evolution-config.h"

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

#include "em-folder-tree-model.h"

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

	EMailFolderTweaks *folder_tweaks;
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

	gboolean loaded;

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
	FOLDER_CUSTOM_ICON,
	COMPARE_FOLDERS,
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

G_DEFINE_TYPE_WITH_CODE (EMFolderTreeModel, em_folder_tree_model, GTK_TYPE_TREE_STORE,
	G_ADD_PRIVATE (EMFolderTreeModel)
	G_IMPLEMENT_INTERFACE (E_TYPE_EXTENSIBLE, NULL))

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
	si->loaded = FALSE;

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
	guint sort_order_a = 0, sort_order_b = 0;
	gint rv = -2;

	folder_tree_model = EM_FOLDER_TREE_MODEL (model);

	gtk_tree_model_get (
		model, a,
		COL_BOOL_IS_STORE, &a_is_store,
		COL_OBJECT_CAMEL_STORE, &service_a,
		COL_STRING_DISPLAY_NAME, &aname,
		COL_UINT_FLAGS, &flags_a,
		COL_UINT_SORT_ORDER, &sort_order_a,
		-1);

	gtk_tree_model_get (
		model, b,
		COL_BOOL_IS_STORE, &b_is_store,
		COL_OBJECT_CAMEL_STORE, &service_b,
		COL_STRING_DISPLAY_NAME, &bname,
		COL_UINT_FLAGS, &flags_b,
		COL_UINT_SORT_ORDER, &sort_order_b,
		-1);

	if (CAMEL_IS_SERVICE (service_a))
		store_uid = camel_service_get_uid (service_a);

	if (!a_is_store && !b_is_store && (sort_order_a || sort_order_b)) {
		if (sort_order_a && sort_order_b)
			rv = sort_order_a < sort_order_b ? -1 : (sort_order_a > sort_order_b ? 1 : 0);
		else if (sort_order_a)
			rv = -1;
		else
			rv = 1;
	} else if (a_is_store && b_is_store) {
		rv = e_mail_account_store_compare_services (
			folder_tree_model->priv->account_store,
			service_a, service_b);

	} else {
		/* Inbox is always first. */
		if ((flags_a & CAMEL_FOLDER_TYPE_MASK) == CAMEL_FOLDER_TYPE_INBOX)
			rv = -1;
		else if ((flags_b & CAMEL_FOLDER_TYPE_MASK) == CAMEL_FOLDER_TYPE_INBOX)
			rv = 1;
	}

	if (rv == -2 && !a_is_store && !b_is_store)
		g_signal_emit (model, signals[COMPARE_FOLDERS], 0, store_uid, a, b, &rv);

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
	EMailFolderTweaks *tweaks;
	gchar *top_folder_uri;

	em_folder_tree_model_remove_store (
		folder_tree_model, CAMEL_STORE (service));

	top_folder_uri = e_mail_folder_uri_build (CAMEL_STORE (service), "");
	tweaks = em_folder_tree_model_get_folder_tweaks (folder_tree_model);

	e_mail_folder_tweaks_remove_for_folders (tweaks, top_folder_uri);

	g_free (top_folder_uri);
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

static gboolean
em_folder_tree_model_update_tweaks_foreach_cb (GtkTreeModel *model,
					       GtkTreePath *path,
					       GtkTreeIter *iter,
					       gpointer user_data)
{
	const gchar *given_folder_uri = user_data;
	gchar *stored_folder_uri = NULL;

	gtk_tree_model_get (model, iter, COL_STRING_FOLDER_URI, &stored_folder_uri, -1);

	if (!stored_folder_uri ||
	    g_strcmp0 (stored_folder_uri, given_folder_uri) != 0) {
		g_free (stored_folder_uri);
		return FALSE;
	}

	g_free (stored_folder_uri);

	em_folder_tree_model_update_row_tweaks (EM_FOLDER_TREE_MODEL (model), iter);

	return TRUE;
}

static void
em_folder_tree_model_folder_tweaks_changed_cb (EMailFolderTweaks *tweaks,
					       const gchar *folder_uri,
					       gpointer user_data)
{
	EMFolderTreeModel *model = user_data;

	g_return_if_fail (EM_IS_FOLDER_TREE_MODEL (model));
	g_return_if_fail (folder_uri != NULL);

	gtk_tree_model_foreach (GTK_TREE_MODEL (model),
		em_folder_tree_model_update_tweaks_foreach_cb, (gpointer) folder_uri);
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

static const gchar *
em_folder_tree_model_get_icon_name_for_folder_uri (EMFolderTreeModel *model,
						   const gchar *folder_uri,
						   CamelStore *store,
						   const gchar *full_name,
						   guint32 *inout_folder_flags)
{
	const gchar *icon_name = "folder";
	CamelFolder *folder;
	EMailSession *session;
	MailFolderCache *folder_cache;
	guint32 folder_flags;
	gboolean folder_is_drafts = FALSE;
	gboolean folder_is_templates = FALSE;
	gboolean folder_is_archive = FALSE;

	g_return_val_if_fail (EM_IS_FOLDER_TREE_MODEL (model), icon_name);
	g_return_val_if_fail (CAMEL_IS_STORE (store), icon_name);
	g_return_val_if_fail (folder_uri != NULL, icon_name);
	g_return_val_if_fail (inout_folder_flags != NULL, icon_name);

	session = em_folder_tree_model_get_session (model);
	if (!session)
		return icon_name;

	folder_flags = *inout_folder_flags;
	folder_cache = e_mail_session_get_folder_cache (session);

	folder_is_archive = e_mail_session_is_archive_folder (session, folder_uri);

	folder = mail_folder_cache_ref_folder (folder_cache, store, full_name);
	if (folder) {
		folder_is_drafts = em_utils_folder_is_drafts (e_mail_session_get_registry (session), folder);

		g_object_unref (folder);
	}

	if (g_strcmp0 (camel_service_get_uid (CAMEL_SERVICE (store)), E_MAIL_SESSION_LOCAL_UID) == 0) {
		if (strcmp (full_name, "Drafts") == 0) {
			folder_is_drafts = TRUE;
		} else if (strcmp (full_name, "Templates") == 0) {
			folder_is_templates = TRUE;
		} else if (strcmp (full_name, "Inbox") == 0) {
			folder_flags = (folder_flags & ~CAMEL_FOLDER_TYPE_MASK) | CAMEL_FOLDER_TYPE_INBOX;
		} else if (strcmp (full_name, "Outbox") == 0) {
			folder_flags = (folder_flags & ~CAMEL_FOLDER_TYPE_MASK) | CAMEL_FOLDER_TYPE_OUTBOX;
		} else if (strcmp (full_name, "Sent") == 0) {
			folder_flags = (folder_flags & ~CAMEL_FOLDER_TYPE_MASK) | CAMEL_FOLDER_TYPE_SENT;
		}
	}

	if ((folder_flags & CAMEL_FOLDER_TYPE_MASK) == 0) {
		gchar *drafts_folder_uri;
		gchar *templates_folder_uri;
		gchar *sent_folder_uri;

		folder_tree_model_get_special_folders_uri (e_mail_session_get_registry (session), store,
			&drafts_folder_uri, &templates_folder_uri, &sent_folder_uri);

		if (!folder_is_drafts && drafts_folder_uri != NULL) {
			folder_is_drafts = e_mail_folder_uri_equal (CAMEL_SESSION (session), folder_uri, drafts_folder_uri);

			if (folder_is_drafts)
				folder_flags |= CAMEL_FOLDER_TYPE_DRAFTS;
		}

		if (!folder_is_templates && templates_folder_uri != NULL) {
			folder_is_templates = e_mail_folder_uri_equal (
				CAMEL_SESSION (session),
				folder_uri, templates_folder_uri);
		}

		if (sent_folder_uri && !(folder_flags & CAMEL_FOLDER_TYPE_MASK)) {
			if (e_mail_folder_uri_equal (CAMEL_SESSION (session), folder_uri, sent_folder_uri)) {
				folder_flags |= CAMEL_FOLDER_TYPE_SENT;
			}
		}

		g_free (drafts_folder_uri);
		g_free (templates_folder_uri);
		g_free (sent_folder_uri);
	}

	/* Choose an icon name for the folder. */
	icon_name = em_folder_utils_get_icon_name (folder_flags);

	if (g_str_equal (icon_name, "folder")) {
		if (folder_is_drafts)
			icon_name = "accessories-text-editor";
		else if (folder_is_templates)
			icon_name = "folder-templates";
		else if (folder_is_archive)
			icon_name = "mail-archive";
	}

	*inout_folder_flags = folder_flags;

	return icon_name;
}

static void
em_folder_tree_model_update_folder_icon (EMFolderTreeModel *model,
					 const gchar *folder_uri)
{
	EMailSession *session;
	CamelStore *store = NULL;
	gchar *full_name = NULL;
	GtkTreeRowReference *row;

	g_return_if_fail (EM_IS_FOLDER_TREE_MODEL (model));
	g_return_if_fail (folder_uri != NULL);

	session = em_folder_tree_model_get_session (model);
	if (!session)
		return;

	if (!e_mail_folder_uri_parse (CAMEL_SESSION (session), folder_uri, &store, &full_name, NULL))
		return;

	row = em_folder_tree_model_get_row_reference (model, store, full_name);
	if (row) {
		EMEventTargetCustomIcon *target;
		GtkTreeModel *tree_model;
		GtkTreePath *path;
		GtkTreeIter iter;
		gchar *old_icon_name = NULL;
		const gchar *new_icon_name;
		guint32 folder_flags = 0;

		tree_model = GTK_TREE_MODEL (model);

		path = gtk_tree_row_reference_get_path (row);
		gtk_tree_model_get_iter (tree_model, &iter, path);
		gtk_tree_path_free (path);

		gtk_tree_model_get (tree_model, &iter,
			COL_UINT_FLAGS, &folder_flags,
			COL_STRING_ICON_NAME, &old_icon_name,
			-1);

		new_icon_name = em_folder_tree_model_get_icon_name_for_folder_uri (model, folder_uri, store, full_name, &folder_flags);

		if (g_strcmp0 (old_icon_name, new_icon_name) != 0) {
			gtk_tree_store_set (
				GTK_TREE_STORE (model), &iter,
				COL_STRING_ICON_NAME, new_icon_name, -1);
		}

		g_free (old_icon_name);

		target = em_event_target_new_custom_icon (
			em_event_peek (), GTK_TREE_STORE (model), &iter,
			full_name, EM_EVENT_CUSTOM_ICON);
		e_event_emit (
			(EEvent *) em_event_peek (), "folder.customicon",
			(EEventTarget *) target);

		g_signal_emit (model, signals[FOLDER_CUSTOM_ICON], 0,
			&iter, store, full_name);
	}

	g_clear_object (&store);
	g_clear_pointer (&full_name, g_free);
}

static void
em_folder_tree_model_archive_folder_changed_cb (EMailSession *session,
						const gchar *service_uid,
						const gchar *old_folder_uri,
						const gchar *new_folder_uri,
						EMFolderTreeModel *model)
{
	g_return_if_fail (EM_IS_FOLDER_TREE_MODEL (model));

	if (old_folder_uri && *old_folder_uri)
		em_folder_tree_model_update_folder_icon (model, old_folder_uri);

	if (new_folder_uri && *new_folder_uri)
		em_folder_tree_model_update_folder_icon (model, new_folder_uri);
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
	EMFolderTreeModel *self = EM_FOLDER_TREE_MODEL (object);

	if (self->priv->selection != NULL) {
		g_object_weak_unref (
			G_OBJECT (self->priv->selection), (GWeakNotify)
			folder_tree_model_selection_finalized_cb, object);
		self->priv->selection = NULL;
	}

	if (self->priv->session != NULL) {
		MailFolderCache *folder_cache;

		folder_cache = e_mail_session_get_folder_cache (self->priv->session);
		g_signal_handlers_disconnect_by_data (folder_cache, object);

		g_signal_handlers_disconnect_by_data (self->priv->session, object);

		g_clear_object (&self->priv->session);
	}

	if (self->priv->account_store != NULL) {
		g_signal_handlers_disconnect_matched (
			self->priv->account_store, G_SIGNAL_MATCH_DATA,
			0, 0, NULL, NULL, object);
		g_clear_object (&self->priv->account_store);
	}

	g_signal_handlers_disconnect_by_func (self->priv->folder_tweaks,
		em_folder_tree_model_folder_tweaks_changed_cb, object);

	/* Chain up to parent's dispose() method. */
	G_OBJECT_CLASS (em_folder_tree_model_parent_class)->dispose (object);
}

static void
folder_tree_model_finalize (GObject *object)
{
	EMFolderTreeModel *self = EM_FOLDER_TREE_MODEL (object);

	g_hash_table_destroy (self->priv->store_index);
	g_mutex_clear (&self->priv->store_index_lock);
	g_clear_object (&self->priv->folder_tweaks);

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
		G_TYPE_STRING,    /* COL_STRING_FOLDER_URI */
		G_TYPE_ICON,      /* COL_GICON_CUSTOM_ICON */
		GDK_TYPE_RGBA,    /* COL_RGBA_FOREGROUND_RGBA */
		G_TYPE_UINT,      /* COL_UINT_SORT_ORDER */
		G_TYPE_UINT       /* COL_UINT_STATUS_CODE */
	};

	g_warn_if_fail (G_N_ELEMENTS (col_types) == NUM_COLUMNS);

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

	e_extensible_load_extensions (E_EXTENSIBLE (object));
}

static void
em_folder_tree_model_class_init (EMFolderTreeModelClass *class)
{
	GObjectClass *object_class;

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

	signals[FOLDER_CUSTOM_ICON] = g_signal_new (
		"folder-custom-icon",
		G_OBJECT_CLASS_TYPE (object_class),
		G_SIGNAL_RUN_FIRST,
		G_STRUCT_OFFSET (EMFolderTreeModelClass, folder_custom_icon),
		NULL, NULL, NULL,
		G_TYPE_NONE, 3,
		G_TYPE_POINTER,
		CAMEL_TYPE_STORE,
		G_TYPE_STRING);

	/* Return -2 for "default sort order", otherwise expects only -1, 0 or 1. */
	signals[COMPARE_FOLDERS] = g_signal_new (
		"compare-folders",
		G_OBJECT_CLASS_TYPE (object_class),
		G_SIGNAL_RUN_FIRST,
		G_STRUCT_OFFSET (EMFolderTreeModelClass, compare_folders),
		NULL, NULL, NULL,
		G_TYPE_INT, 3,
		G_TYPE_STRING,
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
	gboolean unread_increased = FALSE, is_drafts = FALSE;

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

			unread_increased = unread > saved_fu_info->unread;

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
			} else {
				fu_info->is_drafts = em_utils_folder_name_is_drafts (e_mail_session_get_registry (model->priv->session), store, full);
			}

			if (!mail_folder_cache_get_folder_info_flags (folder_cache, store, full, &flags))
				flags = 0;

			fu_info->fi_flags = flags;
		}

		is_drafts = fu_info->is_drafts;

		g_hash_table_insert (si->full_hash_unread, g_strdup (full), fu_info);

		goto exit;
	}

	path = gtk_tree_row_reference_get_path (reference);
	gtk_tree_model_get_iter (tree_model, &iter, path);
	gtk_tree_path_free (path);

	gtk_tree_model_get (
		tree_model, &iter,
		COL_UINT_UNREAD_LAST_SEL, &old_unread,
		COL_BOOL_IS_DRAFT, &is_drafts,
		-1);

	unread_increased = unread > old_unread;

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
	if (unread_increased && !is_drafts && gtk_tree_row_reference_valid (si->row)) {
		path = gtk_tree_row_reference_get_path (si->row);
		gtk_tree_model_get_iter (tree_model, &iter, path);
		gtk_tree_path_free (path);

		gtk_tree_store_set (
			GTK_TREE_STORE (model), &iter,
			COL_UINT_UNREAD, 0,
			COL_UINT_UNREAD_LAST_SEL, 1,
			-1);
	}

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

	model->priv = em_folder_tree_model_get_instance_private (model);
	model->priv->store_index = store_index;
	model->priv->folder_tweaks = e_mail_folder_tweaks_new ();

	g_mutex_init (&model->priv->store_index_lock);

	g_signal_connect (model->priv->folder_tweaks, "changed",
		G_CALLBACK (em_folder_tree_model_folder_tweaks_changed_cb), model);
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

EMailFolderTweaks *
em_folder_tree_model_get_folder_tweaks (EMFolderTreeModel *model)
{
	g_return_val_if_fail (EM_IS_FOLDER_TREE_MODEL (model), NULL);

	return model->priv->folder_tweaks;
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
		g_signal_handlers_disconnect_by_data (folder_cache, model);

		g_signal_handlers_disconnect_by_data (model->priv->session, model);

		g_object_unref (model->priv->session);
	}

	model->priv->session = session;

	/* FIXME Technically we should be disconnecting this signal
	 *       when replacing an old session with a new session,
	 *       but at present this function is only called once. */
	if (session != NULL) {
		EMailAccountStore *account_store;
		MailFolderCache *folder_cache;

		g_signal_connect (model->priv->session, "archive-folder-changed",
			G_CALLBACK (em_folder_tree_model_archive_folder_changed_cb), model);

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

gboolean
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
	guint32 flags;
	EMEventTargetCustomIcon *target;
	gboolean store_is_local;
	gboolean load = FALSE;
	gboolean folder_is_drafts = FALSE;
	gboolean folder_is_outbox = FALSE;
	gboolean folder_is_sent = FALSE;
	gchar *uri;

	g_return_val_if_fail (EM_IS_FOLDER_TREE_MODEL (model), FALSE);
	g_return_val_if_fail (iter != NULL, FALSE);
	g_return_val_if_fail (CAMEL_IS_STORE (store), FALSE);
	g_return_val_if_fail (fi != NULL, FALSE);

	si = folder_tree_model_store_index_lookup (model, store);
	g_return_val_if_fail (si != NULL, FALSE);

	/* Make sure we don't already know about it. */
	if (g_hash_table_lookup (si->full_hash, fi->full_name)) {
		store_info_unref (si);
		return FALSE;
	}

	if (!si->loaded)
		si->loaded = TRUE;

	tree_store = GTK_TREE_STORE (model);

	session = em_folder_tree_model_get_session (model);
	folder_cache = e_mail_session_get_folder_cache (session);
	registry = e_mail_session_get_registry (session);

	uid = camel_service_get_uid (CAMEL_SERVICE (store));
	store_is_local = (g_strcmp0 (uid, E_MAIL_SESSION_LOCAL_UID) == 0);

	if (!fully_loaded)
		load = (fi->child == NULL) && !(fi->flags &
			(CAMEL_FOLDER_NOCHILDREN | CAMEL_FOLDER_NOINFERIORS));
	else
		load = !fi->child && (fi->flags & CAMEL_FOLDER_CHILDREN) != 0;

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
		folder_is_sent = em_utils_folder_is_sent (registry, folder);

		if (folder_is_drafts || folder_is_outbox) {
			gint total;
			gint deleted;

			total = camel_folder_get_message_count (folder);
			deleted = camel_folder_summary_get_deleted_count (camel_folder_get_folder_summary (folder));

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
			display_name = _("Templates");
		} else if (strcmp (fi->full_name, "Inbox") == 0) {
			flags = (flags & ~CAMEL_FOLDER_TYPE_MASK) |
				CAMEL_FOLDER_TYPE_INBOX;
			display_name = _("Inbox");
			folder_is_drafts = FALSE;
			folder_is_sent = FALSE;
		} else if (strcmp (fi->full_name, "Outbox") == 0) {
			flags = (flags & ~CAMEL_FOLDER_TYPE_MASK) |
				CAMEL_FOLDER_TYPE_OUTBOX;
			display_name = _("Outbox");
			folder_is_drafts = FALSE;
			folder_is_sent = FALSE;
		} else if (strcmp (fi->full_name, "Sent") == 0) {
			folder_is_sent = TRUE;
			display_name = _("Sent");
		}
	}

	if ((flags & CAMEL_FOLDER_TYPE_MASK) != CAMEL_FOLDER_TYPE_INBOX) {
		if (folder_is_drafts)
			flags = (flags & ~CAMEL_FOLDER_TYPE_MASK) | CAMEL_FOLDER_TYPE_DRAFTS;
		if (folder_is_sent)
			flags = (flags & ~CAMEL_FOLDER_TYPE_MASK) | CAMEL_FOLDER_TYPE_SENT;
	}

	/* Choose an icon name for the folder. */
	icon_name = em_folder_tree_model_get_icon_name_for_folder_uri (model, uri, store, fi->full_name, &flags);

	if (!store_is_local) {
		struct _special_folders {
			guint32 folder_type;
			const gchar *bare_name;
		} special_folders[] = {
			{ CAMEL_FOLDER_TYPE_INBOX, N_("Inbox") },
			{ CAMEL_FOLDER_TYPE_TRASH, N_("Trash") },
			{ CAMEL_FOLDER_TYPE_JUNK, N_("Junk") },
			{ CAMEL_FOLDER_TYPE_SENT, N_("Sent") },
			{ CAMEL_FOLDER_TYPE_DRAFTS, N_("Drafts") }
		};
		gint ii;

		for (ii = 0; ii < G_N_ELEMENTS (special_folders); ii++) {
			if ((flags & CAMEL_FOLDER_TYPE_MASK) == special_folders[ii].folder_type &&
			    g_strcmp0 (fi->display_name, special_folders[ii].bare_name) == 0) {
				display_name = _(special_folders[ii].bare_name);
				break;
			}
		}
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
		COL_STRING_FOLDER_URI, uri,
		-1);

	em_folder_tree_model_update_row_tweaks (model, iter);

	g_free (uri);
	uri = NULL;

	target = em_event_target_new_custom_icon (
		em_event_peek (), tree_store, iter,
		fi->full_name, EM_EVENT_CUSTOM_ICON);
	e_event_emit (
		(EEvent *) em_event_peek (), "folder.customicon",
		(EEventTarget *) target);

	g_signal_emit (model, signals[FOLDER_CUSTOM_ICON], 0,
		iter, store, fi->full_name);

	if (unread != ~0)
		gtk_tree_store_set (
			tree_store, iter, COL_UINT_UNREAD, unread,
			COL_UINT_UNREAD_LAST_SEL, unread, -1);

	if (load) {
		/* create a placeholder node for our subfolders... */
		gtk_tree_store_append (tree_store, &sub, iter);
		gtk_tree_store_set (
			tree_store, &sub,
			COL_STRING_DISPLAY_NAME, _("Loading…"),
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
		g_signal_emit (model, signals[LOADED_ROW], 0, path, iter);
		g_signal_emit (model, signals[LOADING_ROW], 0, path, iter);
		gtk_tree_path_free (path);
		return TRUE;
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

			if (!em_folder_tree_model_set_folder_info (model, &sub, store, fi, fully_loaded))
				gtk_tree_store_remove (tree_store, &sub);

			fi = fi->next;
		} while (fi);
	}

	if (!emitted) {
		path = gtk_tree_model_get_path (GTK_TREE_MODEL (model), iter);
		g_signal_emit (model, signals[LOADED_ROW], 0, path, iter);
		gtk_tree_path_free (path);
	}

	return TRUE;
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

	if (si->loaded)
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

	if (!em_folder_tree_model_set_folder_info (EM_FOLDER_TREE_MODEL (model), &iter, store, info, TRUE))
		gtk_tree_store_remove (GTK_TREE_STORE (model), &iter);
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

	if (!em_folder_tree_model_set_folder_info (EM_FOLDER_TREE_MODEL (model), &iter, store, fi,
		(fi->flags & (CAMEL_FOLDER_NOINFERIORS | CAMEL_FOLDER_NOCHILDREN)) != 0))
		gtk_tree_store_remove (GTK_TREE_STORE (model), &iter);
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
	guint status_code = EMFT_STATUS_CODE_UNKNOWN;

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
			if (!host_reachable) {
				icon_name = "network-no-route-symbolic";
				status_code = EMFT_STATUS_CODE_NO_ROUTE;
			} else if (was_connecting) {
				icon_name = "network-error-symbolic";
				status_code = EMFT_STATUS_CODE_OTHER_ERROR;
			} else {
				icon_name = "network-offline-symbolic";
				status_code = EMFT_STATUS_CODE_DISCONNECTED;
			}
			break;

		case CAMEL_SERVICE_CONNECTING:
			icon_name = NULL;
			break;

		case CAMEL_SERVICE_CONNECTED:
			icon_name = "network-idle-symbolic";
			status_code = EMFT_STATUS_CODE_CONNECTED;
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
		COL_UINT_STATUS_CODE, status_code,
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
		COL_STRING_DISPLAY_NAME, _("Loading…"),
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

void
em_folder_tree_model_mark_store_loaded (EMFolderTreeModel *model,
					CamelStore *store)
{
	StoreInfo *si;

	g_return_if_fail (EM_IS_FOLDER_TREE_MODEL (model));
	g_return_if_fail (CAMEL_IS_STORE (store));

	si = folder_tree_model_store_index_lookup (model, store);

	if (si) {
		si->loaded = TRUE;
		store_info_unref (si);
	}
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

void
em_folder_tree_model_update_row_tweaks (EMFolderTreeModel *model,
					GtkTreeIter *iter)
{
	GIcon *custom_icon = NULL;
	GdkRGBA *foreground = NULL, rgba;
	gchar *folder_uri = NULL, *icon_filename;
	guint sort_order;

	g_return_if_fail (EM_IS_FOLDER_TREE_MODEL (model));
	g_return_if_fail (iter != NULL);

	gtk_tree_model_get (GTK_TREE_MODEL (model), iter,
		COL_STRING_FOLDER_URI, &folder_uri,
		-1);

	if (!folder_uri)
		return;

	if (e_mail_folder_tweaks_get_color (model->priv->folder_tweaks, folder_uri, &rgba))
		foreground = &rgba;

	icon_filename = e_mail_folder_tweaks_dup_icon_filename (model->priv->folder_tweaks, folder_uri);
	if (icon_filename &&
	    g_file_test (icon_filename, G_FILE_TEST_EXISTS | G_FILE_TEST_IS_REGULAR)) {
		GFile *file;

		file = g_file_new_for_path (icon_filename);
		custom_icon = g_file_icon_new (file);

		g_clear_object (&file);
	}

	sort_order = e_mail_folder_tweaks_get_sort_order (model->priv->folder_tweaks, folder_uri);

	gtk_tree_store_set (GTK_TREE_STORE (model), iter,
		COL_GICON_CUSTOM_ICON, custom_icon,
		COL_RGBA_FOREGROUND_RGBA, foreground,
		COL_UINT_SORT_ORDER, sort_order,
		-1);

	g_clear_object (&custom_icon);
	g_free (icon_filename);
	g_free (folder_uri);
}

void
em_folder_tree_model_update_folder_icons_for_store (EMFolderTreeModel *model,
						    CamelStore *store)
{
	GtkTreeModel *tree_model;
	GHashTableIter iter;
	gpointer value;
	StoreInfo *si;

	g_return_if_fail (EM_IS_FOLDER_TREE_MODEL (model));
	g_return_if_fail (CAMEL_IS_STORE (store));

	si = folder_tree_model_store_index_lookup (model, store);
	if (!si)
		return;

	tree_model = GTK_TREE_MODEL (model);

	g_hash_table_iter_init (&iter, si->full_hash);

	while (g_hash_table_iter_next (&iter, NULL, &value)) {
		GtkTreeRowReference *row = value;

		if (gtk_tree_row_reference_valid (row)) {
			gchar *folder_uri = NULL;
			GtkTreePath *path;
			GtkTreeIter titer;

			path = gtk_tree_row_reference_get_path (row);
			gtk_tree_model_get_iter (tree_model, &titer, path);
			gtk_tree_path_free (path);

			gtk_tree_model_get (tree_model, &titer, COL_STRING_FOLDER_URI, &folder_uri, -1);
			if (folder_uri)
				em_folder_tree_model_update_folder_icon (model, folder_uri);
			g_free (folder_uri);
		}
	}

	store_info_unref (si);
}
