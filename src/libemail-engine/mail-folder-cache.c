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
 *   Peter Williams <peterw@ximian.com>
 *   Michael Zucchi <notzed@ximian.com>
 *   Jonathon Jongsma <jonathon.jongsma@collabora.co.uk>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 * Copyright (C) 2009 Intel Corporation
 *
 */

/**
 * SECTION: mail-folder-cache
 * @short_description: Stores information about open folders
 **/

#include "evolution-config.h"

#include <errno.h>
#include <string.h>
#include <time.h>

#include <glib/gi18n.h>
#include <glib/gstdio.h>

#include <libedataserver/libedataserver.h>

#include "e-util/e-util.h"

#include "mail-mt.h"
#include "mail-folder-cache.h"
#include "mail-ops.h"
#include "e-mail-utils.h"
#include "e-mail-folder-utils.h"
#include "e-mail-session.h"
#include "e-mail-store-utils.h"

#define w(x)
#define d(x)

typedef struct _StoreInfo StoreInfo;
typedef struct _FolderInfo FolderInfo;
typedef struct _AsyncContext AsyncContext;
typedef struct _UpdateClosure UpdateClosure;

struct _MailFolderCachePrivate {
	GMainContext *main_context;
	CamelWeakRefGroup *weak_ref_group;

	/* Store to storeinfo table, active stores */
	GHashTable *store_info_ht;
	GMutex store_info_ht_lock;

	/* hack for people who LIKE to have unsent count */
	gint count_sent;
	gint count_trash;

	GHashTable *local_folder_uris;
	GHashTable *remote_folder_uris;
};

enum {
	PROP_0,
	PROP_MAIN_CONTEXT,
};

enum {
	FOLDER_AVAILABLE,
	FOLDER_UNAVAILABLE,
	FOLDER_DELETED,
	FOLDER_RENAMED,
	FOLDER_UNREAD_UPDATED,
	FOLDER_CHANGED,
	LAST_SIGNAL
};

static guint signals[LAST_SIGNAL];

typedef enum {
	E_FIRST_UPDATE_RUNNING,
	E_FIRST_UPDATE_FAILED,
	E_FIRST_UPDATE_DONE
} EFirstUpdateState;

struct _StoreInfo {
	volatile gint ref_count;

	GMutex lock;

	CamelStore *store;
	gulong folder_opened_handler_id;
	gulong folder_created_handler_id;
	gulong folder_deleted_handler_id;
	gulong folder_renamed_handler_id;
	gulong folder_subscribed_handler_id;
	gulong folder_unsubscribed_handler_id;
	gulong status_handler_id;
	gulong reachable_handler_id;

	GHashTable *folder_info_ht;	/* by full_name */
	EFirstUpdateState first_update;
	GSList *pending_folder_notes;	/* Gather note_folder calls during first_update period */

	/* Hold a reference to keep them alive. */
	CamelFolder *vjunk;
	CamelFolder *vtrash;

	/* Outstanding folderinfo requests */
	GQueue folderinfo_updates;

	CamelServiceConnectionStatus last_status;
};

struct _FolderInfo {
	volatile gint ref_count;

	GMutex lock;

	CamelStore *store;
	gchar *full_name;
	CamelFolderInfoFlags flags;

	GWeakRef folder;
	gulong folder_changed_handler_id;
};

struct _AsyncContext {
	StoreInfo *store_info;
	CamelFolderInfo *info;
};

struct _UpdateClosure {
	CamelWeakRefGroup *cache_weak_ref_group;

	CamelStore *store;

	/* Signal ID for one of:
	 * AVAILABLE, DELETED, RENAMED, UNAVAILABLE */
	guint signal_id;

	gboolean new_messages;

	gchar *full_name;
	gchar *oldfull;

	gint unread;

	/* for only one new message... */
	gchar *msg_uid;
	gchar *msg_sender;
	gchar *msg_subject;
};

/* Forward Declarations */
static void	store_folder_created_cb		(CamelStore *store,
						 CamelFolderInfo *info,
						 MailFolderCache *cache);
static void	store_folder_deleted_cb		(CamelStore *store,
						 CamelFolderInfo *info,
						 MailFolderCache *cache);
static void	store_folder_opened_cb		(CamelStore *store,
						 CamelFolder *folder,
						 MailFolderCache *cache);
static void	store_folder_renamed_cb		(CamelStore *store,
						 const gchar *old_name,
						 CamelFolderInfo *info,
						 MailFolderCache *cache);
static void	store_folder_subscribed_cb	(CamelStore *store,
						 CamelFolderInfo *info,
						 MailFolderCache *cache);
static void	store_folder_unsubscribed_cb	(CamelStore *store,
						 CamelFolderInfo *info,
						 MailFolderCache *cache);

G_DEFINE_TYPE_WITH_PRIVATE (MailFolderCache, mail_folder_cache, G_TYPE_OBJECT)

static FolderInfo *
folder_info_new (CamelStore *store,
                 const gchar *full_name,
                 CamelFolderInfoFlags flags)
{
	FolderInfo *folder_info;

	folder_info = g_slice_new0 (FolderInfo);
	folder_info->ref_count = 1;
	folder_info->store = g_object_ref (store);
	folder_info->full_name = g_strdup (full_name);
	folder_info->flags = flags;

	g_mutex_init (&folder_info->lock);

	return folder_info;
}

static FolderInfo *
folder_info_ref (FolderInfo *folder_info)
{
	g_return_val_if_fail (folder_info != NULL, NULL);
	g_return_val_if_fail (folder_info->ref_count > 0, NULL);

	g_atomic_int_inc (&folder_info->ref_count);

	return folder_info;
}

static void
folder_info_clear_folder (FolderInfo *folder_info)
{
	CamelFolder *folder;

	g_return_if_fail (folder_info != NULL);

	g_mutex_lock (&folder_info->lock);

	folder = g_weak_ref_get (&folder_info->folder);

	if (folder != NULL) {
		g_signal_handler_disconnect (
			folder,
			folder_info->folder_changed_handler_id);

		g_weak_ref_set (&folder_info->folder, NULL);
		folder_info->folder_changed_handler_id = 0;

		g_object_unref (folder);
	}

	g_mutex_unlock (&folder_info->lock);
}

static void
folder_info_unref (FolderInfo *folder_info)
{
	g_return_if_fail (folder_info != NULL);
	g_return_if_fail (folder_info->ref_count > 0);

	if (g_atomic_int_dec_and_test (&folder_info->ref_count)) {
		folder_info_clear_folder (folder_info);

		g_clear_object (&folder_info->store);
		g_free (folder_info->full_name);

		g_mutex_clear (&folder_info->lock);

		g_slice_free (FolderInfo, folder_info);
	}
}

static StoreInfo *
store_info_new (CamelStore *store)
{
	StoreInfo *store_info;

	store_info = g_slice_new0 (StoreInfo);
	store_info->ref_count = 1;
	store_info->store = g_object_ref (store);
	store_info->first_update = E_FIRST_UPDATE_RUNNING;

	store_info->folder_info_ht = g_hash_table_new_full (
		(GHashFunc) g_str_hash,
		(GEqualFunc) g_str_equal,
		(GDestroyNotify) NULL,
		(GDestroyNotify) folder_info_unref);

	g_mutex_init (&store_info->lock);

	/* If these are vfolders then they need to be opened
	 * now, otherwise they won't keep track of all folders. */
	if (camel_store_get_flags (store) & CAMEL_STORE_VJUNK)
		store_info->vjunk = camel_store_get_junk_folder_sync (
			store, NULL, NULL);
	if (camel_store_get_flags (store) & CAMEL_STORE_VTRASH)
		store_info->vtrash = camel_store_get_trash_folder_sync (
			store, NULL, NULL);

	if (CAMEL_IS_NETWORK_SERVICE (store))
		store_info->last_status = camel_service_get_connection_status (CAMEL_SERVICE (store));

	return store_info;
}

static StoreInfo *
store_info_ref (StoreInfo *store_info)
{
	g_return_val_if_fail (store_info != NULL, NULL);
	g_return_val_if_fail (store_info->ref_count > 0, NULL);

	g_atomic_int_inc (&store_info->ref_count);

	return store_info;
}

static void
store_info_unref (StoreInfo *store_info)
{
	g_return_if_fail (store_info != NULL);
	g_return_if_fail (store_info->ref_count > 0);

	if (g_atomic_int_dec_and_test (&store_info->ref_count)) {

		g_warn_if_fail (
			g_queue_is_empty (
			&store_info->folderinfo_updates));

		if (store_info->folder_opened_handler_id > 0) {
			g_signal_handler_disconnect (
				store_info->store,
				store_info->folder_opened_handler_id);
		}

		if (store_info->folder_created_handler_id > 0) {
			g_signal_handler_disconnect (
				store_info->store,
				store_info->folder_created_handler_id);
		}

		if (store_info->folder_deleted_handler_id > 0) {
			g_signal_handler_disconnect (
				store_info->store,
				store_info->folder_deleted_handler_id);
		}

		if (store_info->folder_subscribed_handler_id > 0) {
			g_signal_handler_disconnect (
				store_info->store,
				store_info->folder_subscribed_handler_id);
		}

		if (store_info->folder_unsubscribed_handler_id > 0) {
			g_signal_handler_disconnect (
				store_info->store,
				store_info->folder_unsubscribed_handler_id);
		}

		if (store_info->status_handler_id > 0) {
			g_signal_handler_disconnect (
				store_info->store,
				store_info->status_handler_id);
		}

		if (store_info->reachable_handler_id > 0) {
			g_signal_handler_disconnect (
				store_info->store,
				store_info->reachable_handler_id);
		}

		g_hash_table_destroy (store_info->folder_info_ht);

		g_clear_object (&store_info->store);
		g_clear_object (&store_info->vjunk);
		g_clear_object (&store_info->vtrash);

		g_slist_free_full (store_info->pending_folder_notes, g_object_unref);

		g_mutex_clear (&store_info->lock);

		g_slice_free (StoreInfo, store_info);
	}
}

static FolderInfo *
store_info_ref_folder_info (StoreInfo *store_info,
                            const gchar *folder_name)
{
	GHashTable *folder_info_ht;
	FolderInfo *folder_info;

	g_return_val_if_fail (store_info != NULL, NULL);
	g_return_val_if_fail (folder_name != NULL, NULL);

	g_mutex_lock (&store_info->lock);

	folder_info_ht = store_info->folder_info_ht;

	folder_info = g_hash_table_lookup (folder_info_ht, folder_name);
	if (folder_info != NULL)
		folder_info_ref (folder_info);

	g_mutex_unlock (&store_info->lock);

	return folder_info;
}

static void
store_info_insert_folder_info (StoreInfo *store_info,
                               FolderInfo *folder_info)
{
	GHashTable *folder_info_ht;

	g_return_if_fail (store_info != NULL);
	g_return_if_fail (folder_info != NULL);
	g_return_if_fail (folder_info->full_name != NULL);

	g_mutex_lock (&store_info->lock);

	folder_info_ht = store_info->folder_info_ht;

	/* Replace both key and value, because the key gets freed as soon as the value */
	g_hash_table_replace (
		folder_info_ht,
		folder_info->full_name,
		folder_info_ref (folder_info));

	g_mutex_unlock (&store_info->lock);
}

static GList *
store_info_list_folder_info (StoreInfo *store_info)
{
	GList *list;

	g_return_val_if_fail (store_info != NULL, NULL);

	g_mutex_lock (&store_info->lock);

	list = g_hash_table_get_values (store_info->folder_info_ht);
	g_list_foreach (list, (GFunc) folder_info_ref, NULL);

	g_mutex_unlock (&store_info->lock);

	return list;
}

static FolderInfo *
store_info_steal_folder_info (StoreInfo *store_info,
                              const gchar *folder_name)
{
	GHashTable *folder_info_ht;
	FolderInfo *folder_info;

	g_return_val_if_fail (store_info != NULL, NULL);
	g_return_val_if_fail (folder_name != NULL, NULL);

	g_mutex_lock (&store_info->lock);

	folder_info_ht = store_info->folder_info_ht;

	folder_info = g_hash_table_lookup (folder_info_ht, folder_name);
	if (folder_info != NULL) {
		folder_info_ref (folder_info);
		g_hash_table_remove (folder_info_ht, folder_name);
	}

	g_mutex_unlock (&store_info->lock);

	return folder_info;
}

static void
async_context_free (AsyncContext *async_context)
{
	if (async_context->info != NULL)
		camel_folder_info_free (async_context->info);

	store_info_unref (async_context->store_info);

	g_slice_free (AsyncContext, async_context);
}

static UpdateClosure *
update_closure_new (MailFolderCache *cache,
                    CamelStore *store)
{
	UpdateClosure *closure;

	closure = g_slice_new0 (UpdateClosure);
	closure->cache_weak_ref_group = camel_weak_ref_group_ref (cache->priv->weak_ref_group);
	closure->store = g_object_ref (store);

	return closure;
}

static void
update_closure_free (UpdateClosure *closure)
{
	camel_weak_ref_group_unref (closure->cache_weak_ref_group);

	g_clear_object (&closure->store);

	g_free (closure->full_name);
	g_free (closure->oldfull);
	g_free (closure->msg_uid);
	g_free (closure->msg_sender);
	g_free (closure->msg_subject);

	g_slice_free (UpdateClosure, closure);
}

static void
mail_folder_cache_check_connection_status_cb (CamelStore *store,
					      GParamSpec *param,
					      gpointer user_data);

static StoreInfo *
mail_folder_cache_new_store_info (MailFolderCache *cache,
                                  CamelStore *store)
{
	StoreInfo *store_info;
	gulong handler_id;

	g_return_val_if_fail (store != NULL, NULL);

	store_info = store_info_new (store);

	handler_id = g_signal_connect (
		store, "folder-opened",
		G_CALLBACK (store_folder_opened_cb), cache);
	store_info->folder_opened_handler_id = handler_id;

	handler_id = g_signal_connect (
		store, "folder-created",
		G_CALLBACK (store_folder_created_cb), cache);
	store_info->folder_created_handler_id = handler_id;

	handler_id = g_signal_connect (
		store, "folder-deleted",
		G_CALLBACK (store_folder_deleted_cb), cache);
	store_info->folder_deleted_handler_id = handler_id;

	handler_id = g_signal_connect (
		store, "folder-renamed",
		G_CALLBACK (store_folder_renamed_cb), cache);
	store_info->folder_renamed_handler_id = handler_id;

	if (CAMEL_IS_SUBSCRIBABLE (store)) {
		handler_id = g_signal_connect (
			store, "folder-subscribed",
			G_CALLBACK (store_folder_subscribed_cb), cache);
		store_info->folder_subscribed_handler_id = handler_id;

		handler_id = g_signal_connect (
			store, "folder-unsubscribed",
			G_CALLBACK (store_folder_unsubscribed_cb), cache);
		store_info->folder_unsubscribed_handler_id = handler_id;
	}

	if (CAMEL_IS_NETWORK_SERVICE (store)) {
		store_info->status_handler_id = g_signal_connect (store, "notify::connection-status",
			G_CALLBACK (mail_folder_cache_check_connection_status_cb), cache);

		store_info->reachable_handler_id = g_signal_connect (store, "notify::host-reachable",
			G_CALLBACK (mail_folder_cache_check_connection_status_cb), cache);
	}

	g_mutex_lock (&cache->priv->store_info_ht_lock);

	g_hash_table_insert (
		cache->priv->store_info_ht,
		g_object_ref (store),
		store_info_ref (store_info));

	g_mutex_unlock (&cache->priv->store_info_ht_lock);

	return store_info;
}

static StoreInfo *
mail_folder_cache_ref_store_info (MailFolderCache *cache,
                                  CamelStore *store)
{
	GHashTable *store_info_ht;
	StoreInfo *store_info;

	g_return_val_if_fail (store != NULL, NULL);

	g_mutex_lock (&cache->priv->store_info_ht_lock);

	store_info_ht = cache->priv->store_info_ht;

	store_info = g_hash_table_lookup (store_info_ht, store);
	if (store_info != NULL)
		store_info_ref (store_info);

	g_mutex_unlock (&cache->priv->store_info_ht_lock);

	return store_info;
}

static StoreInfo *
mail_folder_cache_steal_store_info (MailFolderCache *cache,
                                    CamelStore *store)
{
	GHashTable *store_info_ht;
	StoreInfo *store_info;

	g_return_val_if_fail (store != NULL, NULL);

	g_mutex_lock (&cache->priv->store_info_ht_lock);

	store_info_ht = cache->priv->store_info_ht;

	store_info = g_hash_table_lookup (store_info_ht, store);
	if (store_info != NULL) {
		store_info_ref (store_info);
		g_hash_table_remove (store_info_ht, store);
	}

	g_mutex_unlock (&cache->priv->store_info_ht_lock);

	return store_info;
}

static FolderInfo *
mail_folder_cache_ref_folder_info (MailFolderCache *cache,
                                   CamelStore *store,
                                   const gchar *folder_name)
{
	StoreInfo *store_info;
	FolderInfo *folder_info = NULL;

	store_info = mail_folder_cache_ref_store_info (cache, store);
	if (store_info != NULL) {
		folder_info = store_info_ref_folder_info (
			store_info, folder_name);
		store_info_unref (store_info);
	}

	return folder_info;
}

static FolderInfo *
mail_folder_cache_steal_folder_info (MailFolderCache *cache,
                                     CamelStore *store,
                                     const gchar *folder_name)
{
	StoreInfo *store_info;
	FolderInfo *folder_info = NULL;

	store_info = mail_folder_cache_ref_store_info (cache, store);
	if (store_info != NULL) {
		folder_info = store_info_steal_folder_info (
			store_info, folder_name);
		store_info_unref (store_info);
	}

	return folder_info;
}

static void
mail_folder_cache_check_connection_status_cb (CamelStore *store,
					      GParamSpec *param,
					      gpointer user_data)
{
	MailFolderCache *cache = user_data;
	StoreInfo *store_info;
	gboolean was_connecting;

	g_return_if_fail (CAMEL_IS_STORE (store));
	g_return_if_fail (param != NULL);
	g_return_if_fail (MAIL_IS_FOLDER_CACHE (cache));

	store_info = mail_folder_cache_ref_store_info (cache, store);
	if (!store_info)
		return;

	was_connecting = (store_info->last_status == CAMEL_SERVICE_CONNECTING);
	store_info->last_status = camel_service_get_connection_status (CAMEL_SERVICE (store));

	if (!was_connecting && store_info->last_status == CAMEL_SERVICE_DISCONNECTED &&
	    g_strcmp0 (param->name, "host-reachable") == 0 &&
	    camel_network_service_get_host_reachable (CAMEL_NETWORK_SERVICE (store))) {
		CamelProvider *provider;

		provider = camel_service_get_provider (CAMEL_SERVICE (store));
		if (provider && (provider->flags & CAMEL_PROVIDER_IS_STORAGE) != 0) {
			CamelSession *session;

			session = camel_service_ref_session (CAMEL_SERVICE (store));

			/* Connect it, when the host is reachable */
			if (E_IS_MAIL_SESSION (session)) {
				e_mail_session_emit_connect_store (E_MAIL_SESSION (session), store);
			} else {
				e_mail_store_go_online (store, G_PRIORITY_DEFAULT, NULL, NULL, NULL);
			}

			g_clear_object (&session);
		}
	}

	store_info_unref (store_info);
}

static gboolean
mail_folder_cache_update_idle_cb (gpointer user_data)
{
	MailFolderCache *cache;
	UpdateClosure *closure;

	closure = (UpdateClosure *) user_data;

	/* Sanity checks. */
	g_return_val_if_fail (closure->full_name != NULL, FALSE);

	cache = camel_weak_ref_group_get (closure->cache_weak_ref_group);

	if (cache != NULL) {
		if (closure->signal_id == signals[FOLDER_DELETED]) {
			g_signal_emit (
				cache,
				closure->signal_id, 0,
				closure->store,
				closure->full_name);
		}

		if (closure->signal_id == signals[FOLDER_UNAVAILABLE]) {
			g_signal_emit (
				cache,
				closure->signal_id, 0,
				closure->store,
				closure->full_name);
		}

		if (closure->signal_id == signals[FOLDER_AVAILABLE]) {
			g_signal_emit (
				cache,
				closure->signal_id, 0,
				closure->store,
				closure->full_name);
		}

		if (closure->signal_id == signals[FOLDER_RENAMED]) {
			g_signal_emit (
				cache,
				closure->signal_id, 0,
				closure->store,
				closure->oldfull,
				closure->full_name);
		}

		/* update unread counts */
		g_signal_emit (
			cache,
			signals[FOLDER_UNREAD_UPDATED], 0,
			closure->store,
			closure->full_name,
			closure->unread);

		/* XXX The old code excluded this on FOLDER_RENAMED.
		 *     Not sure if that was intentional (if so it was
		 *     very subtle!) but we'll preserve the behavior.
		 *     If it turns out to be a bug then just remove
		 *     the signal_id check. */
		if (closure->signal_id != signals[FOLDER_RENAMED]) {
			g_signal_emit (
				cache,
				signals[FOLDER_CHANGED], 0,
				closure->store,
				closure->full_name,
				closure->new_messages,
				closure->msg_uid,
				closure->msg_sender,
				closure->msg_subject);
		}

		if (CAMEL_IS_VEE_STORE (closure->store) &&
		   (closure->signal_id == signals[FOLDER_AVAILABLE] ||
		    closure->signal_id == signals[FOLDER_RENAMED])) {
			/* Normally the vfolder store takes care of the
			 * folder_opened event itself, but we add folder to
			 * the noting system later, thus we do not know about
			 * search folders to update them in a tree, thus
			 * ensure their changes will be tracked correctly. */
			CamelFolder *folder;

			/* FIXME camel_store_get_folder_sync() may block. */
			folder = camel_store_get_folder_sync (
				closure->store,
				closure->full_name,
				0, NULL, NULL);

			if (folder != NULL) {
				mail_folder_cache_note_folder (cache, folder);
				g_object_unref (folder);
			}
		}

		g_object_unref (cache);
	}

	return FALSE;
}

static void
mail_folder_cache_submit_update (UpdateClosure *closure)
{
	GMainContext *main_context;
	MailFolderCache *cache;
	GSource *idle_source;

	g_return_if_fail (closure != NULL);

	cache = camel_weak_ref_group_get (closure->cache_weak_ref_group);
	g_return_if_fail (cache != NULL);

	main_context = mail_folder_cache_ref_main_context (cache);

	idle_source = g_idle_source_new ();
	g_source_set_callback (
		idle_source,
		mail_folder_cache_update_idle_cb,
		closure,
		(GDestroyNotify) update_closure_free);
	g_source_attach (idle_source, main_context);
	g_source_unref (idle_source);

	g_main_context_unref (main_context);

	g_object_unref (cache);
}

/* This is how unread counts work (and don't work):
 *
 * camel_folder_unread_message_count() only gives a correct answer if
 * the store is paying attention to the folder. (Some stores always
 * pay attention to all folders, but IMAP can only pay attention to
 * one folder at a time.) But it doesn't have any way to know when
 * it's lying, so it's only safe to call it when you know for sure
 * that the store is paying attention to the folder, such as when it's
 * just been created, or you get a folder_changed signal on it.
 *
 * camel_store_get_folder_info() always gives correct answers for the
 * folders it checks, but it can also return -1 for a folder, meaning
 * it didn't check, and so you should stick with your previous answer.
 *
 * update_1folder is called from three places: with info != NULL when
 * the folder is created (or get_folder_info), with info == NULL when
 * a folder changed event is emitted.
 *
 * So if info is NULL, camel_folder_unread_message_count is correct,
 * and if it's not NULL and its unread_message_count isn't -1, then
 * it's correct.  */

static void
update_1folder (MailFolderCache *cache,
                FolderInfo *folder_info,
                gint new_messages,
                const gchar *msg_uid,
                const gchar *msg_sender,
                const gchar *msg_subject,
                CamelFolderInfo *info)
{
	ESourceRegistry *registry;
	CamelService *service;
	CamelSession *session;
	CamelFolder *folder;
	gint unread = -1;
	gint deleted;

	/* XXX This is a dirty way to obtain the ESourceRegistry,
	 *     but it avoids MailFolderCache requiring it up front
	 *     in mail_folder_cache_new(), which just complicates
	 *     application startup even more. */
	service = CAMEL_SERVICE (folder_info->store);
	session = camel_service_ref_session (service);
	registry = e_mail_session_get_registry (E_MAIL_SESSION (session));
	g_object_unref (session);

	g_return_if_fail (E_IS_SOURCE_REGISTRY (registry));

	folder = g_weak_ref_get (&folder_info->folder);

	if (folder != NULL) {
		CamelFolderSummary *summary;
		gboolean folder_is_sent;
		gboolean folder_is_drafts;
		gboolean folder_is_outbox;
		gboolean folder_is_vtrash;
		gboolean special_case;

		summary = camel_folder_get_folder_summary (folder);

		folder_is_sent = em_utils_folder_is_sent (registry, folder);
		folder_is_drafts = em_utils_folder_is_drafts (registry, folder);
		folder_is_outbox = em_utils_folder_is_outbox (registry, folder);
		folder_is_vtrash = CAMEL_IS_VTRASH_FOLDER (folder);

		special_case =
			(cache->priv->count_trash && folder_is_vtrash) ||
			(cache->priv->count_sent && folder_is_sent) ||
			folder_is_drafts || folder_is_outbox;

		if (special_case) {
			d (printf (" total count\n"));
			unread = camel_folder_get_message_count (folder);
			if (folder_is_drafts || folder_is_outbox) {
				guint32 junked = 0;

				deleted = camel_folder_summary_get_deleted_count (summary);
				if (deleted > 0)
					unread -= deleted;

				junked = camel_folder_summary_get_junk_count (summary);
				if (junked > 0)
					unread -= junked;
			}
		} else {
			d (printf (" unread count\n"));
			if (info)
				unread = info->unread;
			else
				unread = camel_folder_summary_get_unread_count (summary);
		}

		g_object_unref (folder);
	}

	d (printf (
		"folder updated: unread %d: '%s'\n",
		unread, folder_info->full_name));

	if (unread >= 0) {
		UpdateClosure *up;

		up = update_closure_new (cache, folder_info->store);
		up->full_name = g_strdup (folder_info->full_name);
		up->unread = unread;
		up->new_messages = new_messages;
		up->msg_uid = g_strdup (msg_uid);
		up->msg_sender = g_strdup (msg_sender);
		up->msg_subject = g_strdup (msg_subject);

		mail_folder_cache_submit_update (up);
	}
}

#define IGNORE_THREAD_VALUE_TODO	GINT_TO_POINTER (1)
#define IGNORE_THREAD_VALUE_IN_PROGRESS	GINT_TO_POINTER (2)
#define IGNORE_THREAD_VALUE_DONE	GINT_TO_POINTER (3)

static gboolean
folder_cache_check_ignore_thread (CamelFolder *folder,
				  CamelMessageInfo *info,
				  GHashTable *added_uids, /* gchar *uid ~> IGNORE_THREAD_VALUE_... */
				  GCancellable *cancellable,
				  GError **error)
{
	GArray *references;
	gboolean has_ignore_thread = FALSE, first_ignore_thread = FALSE, found_first_msgid = FALSE;
	guint64 first_msgid;
	GString *expr = NULL;
	guint ii;

	g_return_val_if_fail (CAMEL_IS_FOLDER (folder), FALSE);
	g_return_val_if_fail (info != NULL, FALSE);
	g_return_val_if_fail (added_uids != NULL, FALSE);
	g_return_val_if_fail (camel_message_info_get_uid (info) != NULL, FALSE);

	if (g_hash_table_lookup (added_uids, camel_message_info_get_uid (info)) == IGNORE_THREAD_VALUE_DONE)
		return camel_message_info_get_user_flag (info, "ignore-thread");

	references = camel_message_info_dup_references (info);
	if (!references || references->len <= 0) {
		if (references)
			g_array_unref (references);
		return FALSE;
	}

	first_msgid = g_array_index (references, guint64, 0);

	for (ii = 0; ii < references->len; ii++) {
		CamelSummaryMessageID msgid;

		msgid.id.id = g_array_index (references, guint64, ii);
		if (!msgid.id.id)
			continue;

		if (!expr)
			expr = g_string_new ("(or ");

		g_string_append_printf (expr, "(header-matches \"x-camel-msgid\" \"%lu %lu\")",
			(gulong) msgid.id.part.hi,
			(gulong) msgid.id.part.lo);
	}

	if (expr) {
		GPtrArray *uids = NULL;

		g_string_append (expr, "))");

		if (camel_folder_search_sync (folder, expr->str, &uids, cancellable, error) && uids) {
			for (ii = 0; ii < uids->len; ii++) {
				const gchar *refruid = uids->pdata[ii];
				CamelMessageInfo *refrinfo;
				gpointer cached_value;

				refrinfo = camel_folder_get_message_info (folder, refruid);
				if (!refrinfo)
					continue;

				/* This is for cases when a subthread is received and the order of UIDs
				   doesn't match the order in the thread (parent before child). */
				cached_value = g_hash_table_lookup (added_uids, refruid);
				if (cached_value == IGNORE_THREAD_VALUE_TODO) {
					GError *local_error = NULL;

					/* To avoid infinite recursion */
					g_hash_table_insert (added_uids, (gpointer) camel_pstring_strdup (refruid), IGNORE_THREAD_VALUE_IN_PROGRESS);

					if (folder_cache_check_ignore_thread (folder, refrinfo, added_uids, cancellable, &local_error))
						camel_message_info_set_user_flag (refrinfo, "ignore-thread", TRUE);

					if (local_error) {
						g_clear_error (&local_error);
					} else {
						cached_value = IGNORE_THREAD_VALUE_DONE;
						g_hash_table_insert (added_uids, (gpointer) camel_pstring_strdup (refruid), IGNORE_THREAD_VALUE_DONE);
					}
				}

				if (!cached_value)
					cached_value = IGNORE_THREAD_VALUE_DONE;

				if (first_msgid && camel_message_info_get_message_id (refrinfo) == first_msgid) {
					/* The first msgid in the references is In-Reply-To, which is the master;
					   the rest is just a guess. */
					first_ignore_thread = camel_message_info_get_user_flag (refrinfo, "ignore-thread");
					found_first_msgid = first_ignore_thread || cached_value == IGNORE_THREAD_VALUE_DONE;

					if (found_first_msgid) {
						g_clear_object (&refrinfo);
						break;
					}
				}

				has_ignore_thread = has_ignore_thread || camel_message_info_get_user_flag (refrinfo, "ignore-thread");

				g_clear_object (&refrinfo);
			}

			g_ptr_array_unref (uids);
		}

		g_string_free (expr, TRUE);
	}

	g_array_unref (references);

	return (found_first_msgid && first_ignore_thread) || (!found_first_msgid && has_ignore_thread);
}

static void
folder_cache_process_folder_changes_thread (CamelFolder *folder,
					    CamelFolderChangeInfo *changes,
					    GCancellable *cancellable,
					    GError **error,
					    gpointer user_data)
{
	static GHashTable *last_newmail_per_folder = NULL;
	static GMutex last_newmail_per_folder_mutex;
	MailFolderCache *cache = user_data;
	time_t latest_received, new_latest_received;
	CamelFolder *local_drafts;
	CamelFolder *local_outbox;
	CamelFolder *local_sent;
	CamelSession *session;
	CamelStore *parent_store;
	CamelMessageInfo *info;
	FolderInfo *folder_info;
	const gchar *full_name;
	gint new = 0;
	gint i;
	guint32 flags;
	gchar *uid = NULL, *sender = NULL, *subject = NULL;

	g_return_if_fail (CAMEL_IS_FOLDER (folder));
	g_return_if_fail (changes != NULL);
	g_return_if_fail (MAIL_IS_FOLDER_CACHE (cache));

	full_name = camel_folder_get_full_name (folder);
	parent_store = camel_folder_get_parent_store (folder);
	session = camel_service_ref_session (CAMEL_SERVICE (parent_store));

	g_mutex_lock (&last_newmail_per_folder_mutex);
	if (last_newmail_per_folder == NULL)
		last_newmail_per_folder = g_hash_table_new (
			g_direct_hash, g_direct_equal);

	/* it's fine to hash them by folder pointer here */
	latest_received = GPOINTER_TO_INT (
		g_hash_table_lookup (last_newmail_per_folder, folder));
	new_latest_received = latest_received;
	g_mutex_unlock (&last_newmail_per_folder_mutex);

	local_drafts = e_mail_session_get_local_folder (
		E_MAIL_SESSION (session), E_MAIL_LOCAL_FOLDER_DRAFTS);
	local_outbox = e_mail_session_get_local_folder (
		E_MAIL_SESSION (session), E_MAIL_LOCAL_FOLDER_OUTBOX);
	local_sent = e_mail_session_get_local_folder (
		E_MAIL_SESSION (session), E_MAIL_LOCAL_FOLDER_SENT);

	if (!CAMEL_IS_VEE_FOLDER (folder)
	    && folder != local_drafts
	    && folder != local_outbox
	    && folder != local_sent
	    && changes && (changes->uid_added->len > 0)) {
		GHashTable *added_uids; /* gchar *uid ~> IGNORE_THREAD_VALUE_... */

		/* The messages can be received in a wrong order (by UID), the same as the In-Reply-To
		   message can be a new message here, in which case it might not be already updated,
		   thus remember which messages are added and eventually update them when needed. */
		added_uids = g_hash_table_new_full (g_str_hash, g_str_equal, (GDestroyNotify) camel_pstring_free, NULL);

		for (i = 0; i < changes->uid_added->len; i++) {
			const gchar *tmp_uid = changes->uid_added->pdata[i];

			if (tmp_uid)
				g_hash_table_insert (added_uids, (gpointer) camel_pstring_strdup (tmp_uid), IGNORE_THREAD_VALUE_TODO);
		}

		/* for each added message, check to see that it is
		 * brand new, not junk and not already deleted */
		for (i = 0; i < changes->uid_added->len && !g_cancellable_is_cancelled (cancellable); i++) {
			info = camel_folder_get_message_info (
				folder, changes->uid_added->pdata[i]);
			if (info) {
				GError *local_error = NULL;

				flags = camel_message_info_get_flags (info);
				if (((flags & CAMEL_MESSAGE_SEEN) == 0) &&
				    ((flags & CAMEL_MESSAGE_DELETED) == 0) &&
				    folder_cache_check_ignore_thread (folder, info, added_uids, cancellable, &local_error)) {
					camel_message_info_set_flags (info, CAMEL_MESSAGE_SEEN, CAMEL_MESSAGE_SEEN);
					camel_message_info_set_user_flag (info, "ignore-thread", TRUE);
					flags = flags | CAMEL_MESSAGE_SEEN;
				}

				if (((flags & CAMEL_MESSAGE_SEEN) == 0) &&
				    ((flags & CAMEL_MESSAGE_JUNK) == 0) &&
				    ((flags & CAMEL_MESSAGE_DELETED) == 0) &&
				    (camel_message_info_get_date_received (info) > latest_received)) {
					if (camel_message_info_get_date_received (info) > new_latest_received)
						new_latest_received = camel_message_info_get_date_received (info);
					new++;
					if (new == 1) {
						uid = g_strdup (camel_message_info_get_uid (info));
						sender = g_strdup (camel_message_info_get_from (info));
						subject = g_strdup (camel_message_info_get_subject (info));
					} else {
						g_free (uid);
						g_free (sender);
						g_free (subject);

						uid = NULL;
						sender = NULL;
						subject = NULL;
					}
				}

				g_clear_object (&info);

				if (local_error) {
					g_propagate_error (error, local_error);
					break;
				}
			}
		}

		g_hash_table_destroy (added_uids);
	}

	if (new > 0) {
		g_mutex_lock (&last_newmail_per_folder_mutex);
		g_hash_table_insert (
			last_newmail_per_folder, folder,
			GINT_TO_POINTER (new_latest_received));
		g_mutex_unlock (&last_newmail_per_folder_mutex);
	}

	folder_info = mail_folder_cache_ref_folder_info (
		cache, parent_store, full_name);
	if (folder_info != NULL) {
		update_1folder (
			cache, folder_info, new,
			uid, sender, subject, NULL);
		folder_info_unref (folder_info);
	}

	g_free (uid);
	g_free (sender);
	g_free (subject);

	g_object_unref (session);
}

#undef IGNORE_THREAD_VALUE_TODO
#undef IGNORE_THREAD_VALUE_IN_PROGRESS
#undef IGNORE_THREAD_VALUE_DONE

static void
folder_changed_cb (CamelFolder *folder,
                   CamelFolderChangeInfo *changes,
                   MailFolderCache *cache)
{
	if (!changes)
		return;

	mail_process_folder_changes (folder, changes,
		folder_cache_process_folder_changes_thread,
		g_object_unref, g_object_ref (cache));
}

static void
unset_folder_info (MailFolderCache *cache,
                   FolderInfo *folder_info,
                   gint delete)
{
	d (printf ("unset folderinfo '%s'\n", folder_info->uri));

	folder_info_clear_folder (folder_info);

	if ((folder_info->flags & CAMEL_FOLDER_NOSELECT) == 0) {
		UpdateClosure *up;

		up = update_closure_new (cache, folder_info->store);
		up->full_name = g_strdup (folder_info->full_name);

		if (delete)
			up->signal_id = signals[FOLDER_DELETED];
		else
			up->signal_id = signals[FOLDER_UNAVAILABLE];

		mail_folder_cache_submit_update (up);
	}
}

static void
setup_folder (MailFolderCache *cache,
              CamelFolderInfo *fi,
              StoreInfo *store_info)
{
	FolderInfo *folder_info;

	folder_info = store_info_ref_folder_info (store_info, fi->full_name);
	if (folder_info != NULL) {
		update_1folder (cache, folder_info, 0, NULL, NULL, NULL, fi);
		folder_info_unref (folder_info);
	} else {
		UpdateClosure *up;

		folder_info = folder_info_new (
			store_info->store,
			fi->full_name,
			fi->flags);

		store_info_insert_folder_info (store_info, folder_info);

		up = update_closure_new (cache, store_info->store);
		up->full_name = g_strdup (fi->full_name);
		up->unread = fi->unread;

		if ((fi->flags & CAMEL_FOLDER_NOSELECT) == 0)
			up->signal_id = signals[FOLDER_AVAILABLE];

		mail_folder_cache_submit_update (up);

		folder_info_unref (folder_info);
	}
}

static void
create_folders (MailFolderCache *cache,
                CamelFolderInfo *fi,
                StoreInfo *store_info)
{
	while (fi) {
		setup_folder (cache, fi, store_info);

		if (fi->child)
			create_folders (cache, fi->child, store_info);

		fi = fi->next;
	}
}

static void
store_folder_subscribed_cb (CamelStore *store,
                            CamelFolderInfo *info,
                            MailFolderCache *cache)
{
	StoreInfo *store_info;

	store_info = mail_folder_cache_ref_store_info (cache, store);
	if (store_info != NULL) {
		setup_folder (cache, info, store_info);
		store_info_unref (store_info);
	}
}

static void
store_folder_created_cb (CamelStore *store,
                         CamelFolderInfo *info,
                         MailFolderCache *cache)
{
	/* We only want created events to do more work
	 * if we don't support subscriptions. */
	if (!CAMEL_IS_SUBSCRIBABLE (store))
		store_folder_subscribed_cb (store, info, cache);
}

static void
store_folder_opened_cb (CamelStore *store,
                        CamelFolder *folder,
                        MailFolderCache *cache)
{
	mail_folder_cache_note_folder (cache, folder);
}

static void
store_folder_unsubscribed_cb (CamelStore *store,
                              CamelFolderInfo *info,
                              MailFolderCache *cache)
{
	FolderInfo *folder_info;

	folder_info = mail_folder_cache_steal_folder_info (
		cache, store, info->full_name);
	if (folder_info != NULL) {
		unset_folder_info (cache, folder_info, TRUE);
		folder_info_unref (folder_info);
	}
}

static void
store_folder_deleted_cb (CamelStore *store,
                         CamelFolderInfo *info,
                         MailFolderCache *cache)
{
	/* We only want deleted events to do more work
	 * if we don't support subscriptions. */
	if (!CAMEL_IS_SUBSCRIBABLE (store))
		store_folder_unsubscribed_cb (store, info, cache);
}

static void
rename_folders (MailFolderCache *cache,
                StoreInfo *store_info,
                const gchar *oldbase,
                const gchar *newbase,
                CamelFolderInfo *fi)
{
	gchar *old, *olduri, *oldfile, *newuri, *newfile;
	FolderInfo *old_folder_info;
	FolderInfo *new_folder_info;
	UpdateClosure *up;
	const gchar *config_dir;

	up = update_closure_new (cache, store_info->store);
	up->signal_id = signals[FOLDER_AVAILABLE];

	/* Form what was the old name, and try and look it up */
	old = g_strdup_printf ("%s%s", oldbase, fi->full_name + strlen (newbase));
	old_folder_info = store_info_steal_folder_info (store_info, old);
	if (old_folder_info != NULL) {
		up->oldfull = g_strdup (old_folder_info->full_name);
		up->signal_id = signals[FOLDER_RENAMED];
		folder_info_unref (old_folder_info);
	}

	new_folder_info = folder_info_new (
		store_info->store,
		fi->full_name,
		fi->flags);

	store_info_insert_folder_info (store_info, new_folder_info);

	folder_info_unref (new_folder_info);

	up->full_name = g_strdup (fi->full_name);
	up->unread = fi->unread==-1 ? 0 : fi->unread;

	/* No signal emission for NOSELECT folders. */
	if ((fi->flags & CAMEL_FOLDER_NOSELECT) != 0)
		up->signal_id = 0;

	mail_folder_cache_submit_update (up);

	/* rename the meta-data we maintain ourselves */
	config_dir = mail_session_get_config_dir ();
	olduri = e_mail_folder_uri_build (store_info->store, old);
	e_util_make_safe_filename (olduri);
	newuri = e_mail_folder_uri_build (store_info->store, fi->full_name);
	e_util_make_safe_filename (newuri);
	oldfile = g_strdup_printf ("%s/custom_view-%s.xml", config_dir, olduri);
	newfile = g_strdup_printf ("%s/custom_view-%s.xml", config_dir, newuri);
	if (g_rename (oldfile, newfile) == -1 && errno != ENOENT) {
		g_warning (
			"%s: Failed to rename '%s' to '%s': %s", G_STRFUNC,
			oldfile, newfile, g_strerror (errno));
	}
	g_free (oldfile);
	g_free (newfile);
	oldfile = g_strdup_printf ("%s/current_view-%s.xml", config_dir, olduri);
	newfile = g_strdup_printf ("%s/current_view-%s.xml", config_dir, newuri);
	if (g_rename (oldfile, newfile) == -1 && errno != ENOENT) {
		g_warning (
			"%s: Failed to rename '%s' to '%s': %s", G_STRFUNC,
			oldfile, newfile, g_strerror (errno));
	}
	g_free (oldfile);
	g_free (newfile);
	g_free (olduri);
	g_free (newuri);

	g_free (old);
}

static void
get_folders (CamelFolderInfo *fi,
             GPtrArray *folders)
{
	while (fi) {
		g_ptr_array_add (folders, fi);

		if (fi->child)
			get_folders (fi->child, folders);

		fi = fi->next;
	}
}

static gint
folder_cmp (gconstpointer ap,
            gconstpointer bp)
{
	const CamelFolderInfo *a = ((CamelFolderInfo **) ap)[0];
	const CamelFolderInfo *b = ((CamelFolderInfo **) bp)[0];

	return strcmp (a->full_name, b->full_name);
}

static void
store_folder_renamed_cb (CamelStore *store,
                         const gchar *old_name,
                         CamelFolderInfo *info,
                         MailFolderCache *cache)
{
	StoreInfo *store_info;

	store_info = mail_folder_cache_ref_store_info (cache, store);
	if (store_info != NULL) {
		GPtrArray *folders = g_ptr_array_new ();
		CamelFolderInfo *top;
		gint ii;

		/* Ok, so for some reason the folderinfo we have comes in all
		 * messed up from imap, should find out why ... this makes it
		 * workable.
		 * XXX This refers to the old IMAP backend, not IMAPX, and so
		 *     this may not be needed anymore. */
		get_folders (info, folders);
		g_ptr_array_sort (folders, (GCompareFunc) folder_cmp);

		top = folders->pdata[0];
		for (ii = 0; ii < folders->len; ii++) {
			rename_folders (
				cache, store_info, old_name,
				top->full_name, folders->pdata[ii]);
		}

		g_ptr_array_free (folders, TRUE);

		store_info_unref (store_info);
	}
}

static gboolean
store_has_folder_hierarchy (CamelStore *store)
{
	CamelProvider *provider;

	g_return_val_if_fail (store != NULL, FALSE);

	provider = camel_service_get_provider (CAMEL_SERVICE (store));
	g_return_val_if_fail (provider != NULL, FALSE);

	if (provider->flags & CAMEL_PROVIDER_IS_STORAGE)
		return TRUE;

	if (provider->flags & CAMEL_PROVIDER_IS_EXTERNAL)
		return TRUE;

	return FALSE;
}

static void
mail_folder_cache_get_property (GObject *object,
                                guint property_id,
                                GValue *value,
                                GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_MAIN_CONTEXT:
			g_value_take_boxed (
				value,
				mail_folder_cache_ref_main_context (
				MAIL_FOLDER_CACHE (object)));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
mail_folder_cache_dispose (GObject *object)
{
	MailFolderCache *self = MAIL_FOLDER_CACHE (object);

	g_hash_table_remove_all (self->priv->store_info_ht);

	/* Chain up to parent's dispose() method. */
	G_OBJECT_CLASS (mail_folder_cache_parent_class)->dispose (object);
}

static void
mail_folder_cache_finalize (GObject *object)
{
	MailFolderCache *self = MAIL_FOLDER_CACHE (object);

	g_main_context_unref (self->priv->main_context);
	camel_weak_ref_group_unref (self->priv->weak_ref_group);

	g_hash_table_destroy (self->priv->store_info_ht);
	g_hash_table_destroy (self->priv->local_folder_uris);
	g_hash_table_destroy (self->priv->remote_folder_uris);
	g_mutex_clear (&self->priv->store_info_ht_lock);

	/* Chain up to parent's finalize() method. */
	G_OBJECT_CLASS (mail_folder_cache_parent_class)->finalize (object);
}

static void
mail_folder_cache_folder_available (MailFolderCache *cache,
                                    CamelStore *store,
                                    const gchar *folder_name)
{
	CamelService *service;
	CamelSession *session;
	CamelProvider *provider;
	GHashTable *uris_table;
	gchar *folder_uri;

	/* Disregard virtual stores. */
	if (CAMEL_IS_VEE_STORE (store))
		return;

	/* Disregard virtual Junk folders. */
	if (camel_store_get_flags (store) & CAMEL_STORE_VJUNK)
		if (g_strcmp0 (folder_name, CAMEL_VJUNK_NAME) == 0)
			return;

	/* Disregard virtual Trash folders. */
	if (camel_store_get_flags (store) & CAMEL_STORE_VTRASH)
		if (g_strcmp0 (folder_name, CAMEL_VTRASH_NAME) == 0)
			return;

	service = CAMEL_SERVICE (store);
	session = camel_service_ref_session (service);
	provider = camel_service_get_provider (service);

	/* Reuse the store info lock just because it's handy. */
	g_mutex_lock (&cache->priv->store_info_ht_lock);

	folder_uri = e_mail_folder_uri_build (store, folder_name);

	if (provider->flags & CAMEL_PROVIDER_IS_REMOTE)
		uris_table = cache->priv->remote_folder_uris;
	else
		uris_table = cache->priv->local_folder_uris;

	g_hash_table_add (uris_table, folder_uri);

	g_mutex_unlock (&cache->priv->store_info_ht_lock);

	g_object_unref (session);
}

static void
mail_folder_cache_folder_unavailable (MailFolderCache *cache,
                                      CamelStore *store,
                                      const gchar *folder_name)
{
	CamelService *service;
	CamelSession *session;
	CamelProvider *provider;
	GHashTable *uris_table;
	gchar *folder_uri;

	/* Disregard virtual stores. */
	if (CAMEL_IS_VEE_STORE (store))
		return;

	/* Disregard virtual Junk folders. */
	if (camel_store_get_flags (store) & CAMEL_STORE_VJUNK)
		if (g_strcmp0 (folder_name, CAMEL_VJUNK_NAME) == 0)
			return;

	/* Disregard virtual Trash folders. */
	if (camel_store_get_flags (store) & CAMEL_STORE_VTRASH)
		if (g_strcmp0 (folder_name, CAMEL_VTRASH_NAME) == 0)
			return;

	service = CAMEL_SERVICE (store);
	session = camel_service_ref_session (service);
	provider = camel_service_get_provider (service);

	/* Reuse the store info lock just because it's handy. */
	g_mutex_lock (&cache->priv->store_info_ht_lock);

	folder_uri = e_mail_folder_uri_build (store, folder_name);

	if (provider->flags & CAMEL_PROVIDER_IS_REMOTE)
		uris_table = cache->priv->remote_folder_uris;
	else
		uris_table = cache->priv->local_folder_uris;

	g_hash_table_remove (uris_table, folder_uri);

	g_free (folder_uri);

	g_mutex_unlock (&cache->priv->store_info_ht_lock);

	g_object_unref (session);
}

static void
mail_folder_cache_folder_deleted (MailFolderCache *cache,
                                  CamelStore *store,
                                  const gchar *folder_name)
{
	CamelService *service;
	CamelSession *session;
	gchar *folder_uri;

	/* Disregard virtual stores. */
	if (CAMEL_IS_VEE_STORE (store))
		return;

	/* Disregard virtual Junk folders. */
	if (camel_store_get_flags (store) & CAMEL_STORE_VJUNK)
		if (g_strcmp0 (folder_name, CAMEL_VJUNK_NAME) == 0)
			return;

	/* Disregard virtual Trash folders. */
	if (camel_store_get_flags (store) & CAMEL_STORE_VTRASH)
		if (g_strcmp0 (folder_name, CAMEL_VTRASH_NAME) == 0)
			return;

	service = CAMEL_SERVICE (store);
	session = camel_service_ref_session (service);

	/* Reuse the store info lock just because it's handy. */
	g_mutex_lock (&cache->priv->store_info_ht_lock);

	folder_uri = e_mail_folder_uri_build (store, folder_name);

	g_hash_table_remove (cache->priv->local_folder_uris, folder_uri);
	g_hash_table_remove (cache->priv->remote_folder_uris, folder_uri);

	g_free (folder_uri);

	g_mutex_unlock (&cache->priv->store_info_ht_lock);

	g_object_unref (session);
}

static void
mail_folder_cache_class_init (MailFolderCacheClass *class)
{
	GObjectClass *object_class;

	object_class = G_OBJECT_CLASS (class);
	object_class->get_property = mail_folder_cache_get_property;
	object_class->dispose = mail_folder_cache_dispose;
	object_class->finalize = mail_folder_cache_finalize;

	class->folder_available = mail_folder_cache_folder_available;
	class->folder_unavailable = mail_folder_cache_folder_unavailable;
	class->folder_deleted = mail_folder_cache_folder_deleted;

	g_object_class_install_property (
		object_class,
		PROP_MAIN_CONTEXT,
		g_param_spec_boxed (
			"main-context",
			"Main Context",
			"The main loop context on "
			"which to attach event sources",
			G_TYPE_MAIN_CONTEXT,
			G_PARAM_READABLE |
			G_PARAM_STATIC_STRINGS));

	/**
	 * MailFolderCache::folder-available
	 * @store: the #CamelStore containing the folder
	 * @folder_name: the name of the folder
	 *
	 * Emitted when a folder becomes available
	 **/
	signals[FOLDER_AVAILABLE] = g_signal_new (
		"folder-available",
		G_OBJECT_CLASS_TYPE (object_class),
		G_SIGNAL_RUN_FIRST,
		G_STRUCT_OFFSET (MailFolderCacheClass, folder_available),
		NULL, NULL, NULL,
		G_TYPE_NONE, 2,
		CAMEL_TYPE_STORE,
		G_TYPE_STRING);

	/**
	 * MailFolderCache::folder-unavailable
	 * @store: the #CamelStore containing the folder
	 * @folder_name: the name of the folder
	 *
	 * Emitted when a folder becomes unavailable.  This represents a
	 * transient condition.  See MailFolderCache::folder-deleted to be
	 * notified when a folder is permanently removed.
	 **/
	signals[FOLDER_UNAVAILABLE] = g_signal_new (
		"folder-unavailable",
		G_OBJECT_CLASS_TYPE (object_class),
		G_SIGNAL_RUN_FIRST,
		G_STRUCT_OFFSET (MailFolderCacheClass, folder_unavailable),
		NULL, NULL, NULL,
		G_TYPE_NONE, 2,
		CAMEL_TYPE_STORE,
		G_TYPE_STRING);

	/**
	 * MailFolderCache::folder-deleted
	 * @store: the #CamelStore containing the folder
	 * @folder_name: the name of the folder
	 *
	 * Emitted when a folder is deleted
	 **/
	signals[FOLDER_DELETED] = g_signal_new (
		"folder-deleted",
		G_OBJECT_CLASS_TYPE (object_class),
		G_SIGNAL_RUN_FIRST,
		G_STRUCT_OFFSET (MailFolderCacheClass, folder_deleted),
		NULL, NULL, /* accumulator */
		NULL,
		G_TYPE_NONE, 2,
		CAMEL_TYPE_STORE,
		G_TYPE_STRING);

	/**
	 * MailFolderCache::folder-renamed
	 * @store: the #CamelStore containing the folder
	 * @old_folder_name: the old name of the folder
	 * @new_folder_name: the new name of the folder
	 *
	 * Emitted when a folder is renamed
	 **/
	signals[FOLDER_RENAMED] = g_signal_new (
		"folder-renamed",
		G_OBJECT_CLASS_TYPE (object_class),
		G_SIGNAL_RUN_FIRST,
		G_STRUCT_OFFSET (MailFolderCacheClass, folder_renamed),
		NULL, NULL, NULL,
		G_TYPE_NONE, 3,
		CAMEL_TYPE_STORE,
		G_TYPE_STRING,
		G_TYPE_STRING);

	/**
	 * MailFolderCache::folder-unread-updated
	 * @store: the #CamelStore containing the folder
	 * @folder_name: the name of the folder
	 * @unread: the number of unread mails in the folder
	 *
	 * Emitted when a we receive an update to the unread count for a folder
	 **/
	signals[FOLDER_UNREAD_UPDATED] = g_signal_new (
		"folder-unread-updated",
		G_OBJECT_CLASS_TYPE (object_class),
		G_SIGNAL_RUN_FIRST,
		G_STRUCT_OFFSET (MailFolderCacheClass, folder_unread_updated),
		NULL, NULL, NULL,
		G_TYPE_NONE, 3,
		CAMEL_TYPE_STORE,
		G_TYPE_STRING,
		G_TYPE_INT);

	/**
	 * MailFolderCache::folder-changed
	 * @store: the #CamelStore containing the folder
	 * @folder_name: the name of the folder
	 * @new_messages: the number of new messages for the folder
	 * @msg_uid: uid of the new message, or NULL
	 * @msg_sender: sender of the new message, or NULL
	 * @msg_subject: subject of the new message, or NULL
	 *
	 * Emitted when a folder has changed.  If @new_messages is not
	 * exactly 1, @msg_uid, @msg_sender, and @msg_subject will be NULL.
	 **/
	signals[FOLDER_CHANGED] = g_signal_new (
		"folder-changed",
		G_OBJECT_CLASS_TYPE (object_class),
		G_SIGNAL_RUN_FIRST,
		G_STRUCT_OFFSET (MailFolderCacheClass, folder_changed),
		NULL, NULL, NULL,
		G_TYPE_NONE, 6,
		CAMEL_TYPE_STORE,
		G_TYPE_STRING,
		G_TYPE_INT,
		G_TYPE_STRING,
		G_TYPE_STRING,
		G_TYPE_STRING);
}

static void
mail_folder_cache_init (MailFolderCache *cache)
{
	GHashTable *store_info_ht;

	store_info_ht = g_hash_table_new_full (
		(GHashFunc) g_direct_hash,
		(GEqualFunc) g_direct_equal,
		(GDestroyNotify) g_object_unref,
		(GDestroyNotify) store_info_unref);

	cache->priv = mail_folder_cache_get_instance_private (cache);
	cache->priv->main_context = g_main_context_ref_thread_default ();
	cache->priv->weak_ref_group = camel_weak_ref_group_new ();

	cache->priv->store_info_ht = store_info_ht;
	g_mutex_init (&cache->priv->store_info_ht_lock);

	cache->priv->count_sent = getenv ("EVOLUTION_COUNT_SENT") != NULL;
	cache->priv->count_trash = getenv ("EVOLUTION_COUNT_TRASH") != NULL;

	/* these URIs always come from e_mail_folder_uri_build(), thus no need to engage
	   slow e_mail_folder_uri_equal() */
	cache->priv->local_folder_uris = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, NULL);
	cache->priv->remote_folder_uris = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, NULL);

	camel_weak_ref_group_set (cache->priv->weak_ref_group, cache);
}

MailFolderCache *
mail_folder_cache_new (void)
{
	return g_object_new (MAIL_TYPE_FOLDER_CACHE, NULL);
}

/**
 * mail_folder_cache_ref_main_context:
 *
 * Returns the #GMainContext on which event sources for @cache are to be
 * attached.
 *
 * The returned #GMainContext is referenced for thread-safety and should
 * be unreferenced with g_main_context_unref() when finished with it.
 *
 * Returns: a #GMainContext
 **/
GMainContext *
mail_folder_cache_ref_main_context (MailFolderCache *cache)
{
	g_return_val_if_fail (MAIL_IS_FOLDER_CACHE (cache), NULL);

	return g_main_context_ref (cache->priv->main_context);
}

static ESource *
mail_folder_cache_ref_related_source (ESourceRegistry *registry,
				      ESource *account_source,
				      ESource *collection_source,
				      const gchar *extension_name)
{
	ESource *found_source = NULL;
	GList *sources, *link;
	const gchar *parent1, *parent2;

	g_return_val_if_fail (E_IS_SOURCE_REGISTRY (registry), NULL);
	g_return_val_if_fail (E_IS_SOURCE (account_source), NULL);
	if (collection_source)
		g_return_val_if_fail (E_IS_SOURCE (collection_source), NULL);
	g_return_val_if_fail (extension_name != NULL, NULL);

	parent1 = e_source_get_uid (account_source);
	parent2 = collection_source ? e_source_get_uid (collection_source) : NULL;

	sources = e_source_registry_list_sources (registry, extension_name);
	for (link = sources; link; link = g_list_next (link)) {
		ESource *source = link->data;
		const gchar *parent;

		if (!source)
			continue;

		parent = e_source_get_parent (source);
		if (!parent)
			continue;

		if (g_strcmp0 (parent, parent1) == 0 ||
		    g_strcmp0 (parent, parent2) == 0) {
			found_source = g_object_ref (source);
			break;
		}
	}

	g_list_free_full (sources, g_object_unref);

	return found_source;
}

static gboolean
mail_folder_cache_store_save_setup_sync (CamelService *service,
					 ESourceRegistry *registry,
					 ESource *account_source,
					 GHashTable *save_setup,
					 GCancellable *cancellable,
					 GError **error)
{
	ESource *collection_source = NULL;
	ESource *submission_source = NULL;
	ESource *transport_source = NULL;
	gboolean success = TRUE;

	/* The sources are either:
		Account
		 - Submission
		 - Transport
	or
		Collection
		 - Account
		 - Submission
		 - Transport
	*/

	g_return_val_if_fail (CAMEL_IS_STORE (service), FALSE);
	g_return_val_if_fail (E_IS_SOURCE_REGISTRY (registry), FALSE);
	g_return_val_if_fail (E_IS_SOURCE (account_source), FALSE);
	g_return_val_if_fail (save_setup != NULL, FALSE);

	if (!g_hash_table_size (save_setup))
		return TRUE;

	if (e_source_get_parent (account_source)) {
		collection_source = e_source_registry_ref_source (registry, e_source_get_parent (account_source));
		if (!collection_source || !e_source_has_extension (collection_source, E_SOURCE_EXTENSION_COLLECTION)) {
			g_clear_object (&collection_source);
		}
	}

	submission_source = mail_folder_cache_ref_related_source (registry, account_source,
		collection_source, E_SOURCE_EXTENSION_MAIL_SUBMISSION);
	transport_source = mail_folder_cache_ref_related_source (registry, account_source,
		collection_source, E_SOURCE_EXTENSION_MAIL_TRANSPORT);

	success = e_mail_store_save_initial_setup_sync (CAMEL_STORE (service), save_setup,
		collection_source, account_source, submission_source, transport_source,
		TRUE, cancellable, error);

	g_clear_object (&collection_source);
	g_clear_object (&submission_source);
	g_clear_object (&transport_source);

	return success;
}

static gboolean
mail_folder_cache_maybe_run_initial_setup_sync (CamelService *service,
						GCancellable *cancellable,
						GError **error)
{
	CamelSession *session;
	ESourceRegistry *registry;
	ESource *source;
	ESourceMailAccount *mail_account;
	gboolean success = TRUE;

	g_return_val_if_fail (CAMEL_IS_STORE (service), FALSE);

	session = camel_service_ref_session (service);

	/* It can be NULL, in some corner cases, thus do not consider it a problem */
	if (!session)
		return TRUE;

	g_return_val_if_fail (E_IS_MAIL_SESSION (session), FALSE);

	registry = e_mail_session_get_registry (E_MAIL_SESSION (session));
	source = e_source_registry_ref_source (registry, camel_service_get_uid (service));

	if (source) {
		mail_account = e_source_get_extension (source, E_SOURCE_EXTENSION_MAIL_ACCOUNT);
		if (e_source_mail_account_get_needs_initial_setup (mail_account)) {
			CamelStore *store = CAMEL_STORE (service);
			GHashTable *save_setup = NULL;

			/* The store doesn't support the function, thus silently pretend success.
			   Still update the ESource flag, in case the store would implement
			   the function in the future. */
			if (!(camel_store_get_flags (store) & CAMEL_STORE_SUPPORTS_INITIAL_SETUP))
				success = TRUE;
			else
				success = camel_store_initial_setup_sync (store, &save_setup, cancellable, error);

			if (success) {
				e_source_mail_account_set_needs_initial_setup (mail_account, FALSE);

				if (save_setup)
					success = mail_folder_cache_store_save_setup_sync (service, registry, source, save_setup, cancellable, error);

				if (success && e_source_get_writable (source))
					success = e_source_write_sync (source, cancellable, error);
			}

			if (save_setup)
				g_hash_table_destroy (save_setup);
		}
	}

	g_clear_object (&session);
	g_clear_object (&source);

	return success;
}

/* Helper for mail_folder_cache_note_store() */
static void
mail_folder_cache_first_update (MailFolderCache *cache,
                                StoreInfo *store_info)
{
	CamelService *service;
	CamelSession *session;
	const gchar *uid;
	GSList *folders, *iter;

	service = CAMEL_SERVICE (store_info->store);
	session = camel_service_ref_session (service);
	uid = camel_service_get_uid (service);

	if (store_info->vjunk != NULL)
		mail_folder_cache_note_folder (cache, store_info->vjunk);

	if (store_info->vtrash != NULL)
		mail_folder_cache_note_folder (cache, store_info->vtrash);

	/* Some extra work for the "On This Computer" store. */
	if (g_strcmp0 (uid, E_MAIL_SESSION_LOCAL_UID) == 0) {
		CamelFolder *folder;
		gint ii;

		for (ii = 0; ii < E_MAIL_NUM_LOCAL_FOLDERS; ii++) {
			folder = e_mail_session_get_local_folder (
				E_MAIL_SESSION (session), ii);
			mail_folder_cache_note_folder (cache, folder);
		}
	}

	g_object_unref (session);

	g_mutex_lock (&store_info->lock);
	store_info->first_update = E_FIRST_UPDATE_DONE;
	folders = store_info->pending_folder_notes;
	store_info->pending_folder_notes = NULL;
	g_mutex_unlock (&store_info->lock);

	for (iter = folders; iter; iter = g_slist_next (iter)) {
		mail_folder_cache_note_folder (cache, iter->data);
	}

	g_slist_free_full (folders, g_object_unref);
}

/* Helper for mail_folder_cache_note_store() */
static void
mail_folder_cache_note_store_thread (ESimpleAsyncResult *simple,
                                     gpointer source_object,
                                     GCancellable *cancellable)
{
	MailFolderCache *cache;
	CamelService *service;
	CamelSession *session;
	StoreInfo *store_info;
	GQueue result_queue = G_QUEUE_INIT;
	AsyncContext *async_context;
	gboolean success = FALSE;
	GError *local_error = NULL;

	cache = MAIL_FOLDER_CACHE (source_object);
	async_context = e_simple_async_result_get_op_pointer (simple);
	store_info = async_context->store_info;

	service = CAMEL_SERVICE (store_info->store);
	session = camel_service_ref_session (service);

	/* We might get a race when setting up a store, such that it is
	 * still left in offline mode, after we've gone online.  This
	 * catches and fixes it up when the shell opens us.
	 *
	 * XXX This is a Bonobo-era artifact.  Do we really still need
	 *     to do this?
	 */
	if (camel_session_get_online (session)) {
		gboolean store_online = TRUE;

		if (CAMEL_IS_OFFLINE_STORE (service)) {
			store_online = camel_offline_store_get_online (CAMEL_OFFLINE_STORE (service)) &&
				camel_service_get_connection_status (service) == CAMEL_SERVICE_CONNECTED;
		}

		if (!store_online) {
			/* Ignore these errors, it can still provide folders in offline */
			store_online = e_mail_store_go_online_sync (CAMEL_STORE (service), cancellable, NULL);
		}

		if (store_online && !mail_folder_cache_maybe_run_initial_setup_sync (service, cancellable, &local_error)) {
			/* Just log on console, but keep going otherwise */
			g_warning ("%s: Failed to run initial setup for '%s': %s", G_STRFUNC, camel_service_get_display_name (service), local_error ? local_error->message : "Unknown error");
			g_clear_error (&local_error);
		}
	}

	/* No folder hierarchy means we're done. */
	if (!store_has_folder_hierarchy (store_info->store))
		goto exit;

	/* XXX This can return NULL without setting a GError if no
	 *     folders match the search criteria or the store does
	 *     not support folders.
	 *
	 *     The function signature should be changed to return a
	 *     boolean with the CamelFolderInfo returned through an
	 *     "out" parameter so it's easier to distinguish errors
	 *     from empty results.
	 */
	async_context->info = camel_store_get_folder_info_sync (
		store_info->store, NULL,
		CAMEL_STORE_FOLDER_INFO_FAST |
		CAMEL_STORE_FOLDER_INFO_RECURSIVE |
		CAMEL_STORE_FOLDER_INFO_SUBSCRIBED,
		cancellable, &local_error);

	if (local_error != NULL) {
		g_warn_if_fail (async_context->info == NULL);
		e_simple_async_result_take_error (simple, local_error);
		goto exit;
	}

	create_folders (cache, async_context->info, store_info);

	/* Do some extra work for the first update. */
	g_mutex_lock (&store_info->lock);
	if (store_info->first_update != E_FIRST_UPDATE_DONE) {
		g_mutex_unlock (&store_info->lock);
		mail_folder_cache_first_update (cache, store_info);
	} else {
		g_mutex_unlock (&store_info->lock);
	}

	success = TRUE;
exit:
	/* We don't want finish() functions being invoked while holding a
	 * locked mutex, so flush the StoreInfo's queue to a local queue. */
	g_mutex_lock (&store_info->lock);

	if (store_info->first_update != E_FIRST_UPDATE_DONE)
		store_info->first_update = success ? E_FIRST_UPDATE_DONE : E_FIRST_UPDATE_FAILED;

	e_queue_transfer (&store_info->folderinfo_updates, &result_queue);
	g_mutex_unlock (&store_info->lock);

	while (!g_queue_is_empty (&result_queue)) {
		ESimpleAsyncResult *queued_result;

		queued_result = g_queue_pop_head (&result_queue);

		/* Skip the ESimpleAsyncResult passed into this function.
		 * e_simple_async_result_run_in_thread() will complete it
		 * for us, and we don't want to complete it twice. */
		if (queued_result != simple)
			e_simple_async_result_complete_idle_take (queued_result);
		else
			g_clear_object (&queued_result);
	}

	g_object_unref (session);
}

/**
 * mail_folder_cache_note_store:
 *
 * Add a store whose folders should appear in the shell The folders are scanned
 * from the store, and/or added at runtime via the folder_created event.  The
 * @done function returns if we can free folder info.
 */
void
mail_folder_cache_note_store (MailFolderCache *cache,
                              CamelStore *store,
                              GCancellable *cancellable,
                              GAsyncReadyCallback callback,
                              gpointer user_data)
{
	StoreInfo *store_info;
	ESimpleAsyncResult *simple;
	AsyncContext *async_context;

	g_return_if_fail (MAIL_IS_FOLDER_CACHE (cache));
	g_return_if_fail (CAMEL_IS_STORE (store));

	store_info = mail_folder_cache_ref_store_info (cache, store);
	if (store_info == NULL)
		store_info = mail_folder_cache_new_store_info (cache, store);

	async_context = g_slice_new0 (AsyncContext);
	async_context->store_info = store_info_ref (store_info);

	simple = e_simple_async_result_new (
		G_OBJECT (cache), callback, user_data,
		mail_folder_cache_note_store);

	e_simple_async_result_set_op_pointer (
		simple, async_context, (GDestroyNotify) async_context_free);

	g_mutex_lock (&store_info->lock);

	if (store_info->first_update != E_FIRST_UPDATE_DONE)
		store_info->first_update = E_FIRST_UPDATE_RUNNING;

	g_queue_push_tail (
		&store_info->folderinfo_updates,
		g_object_ref (simple));

	/* Queue length > 1 means there's already an operation for
	 * this store in progress so we'll just pick up the result
	 * when it finishes. */
	if (g_queue_get_length (&store_info->folderinfo_updates) == 1)
		e_simple_async_result_run_in_thread (
			simple, G_PRIORITY_DEFAULT,
			mail_folder_cache_note_store_thread,
			cancellable);

	g_mutex_unlock (&store_info->lock);

	g_object_unref (simple);

	store_info_unref (store_info);
}

gboolean
mail_folder_cache_note_store_finish (MailFolderCache *cache,
                                     GAsyncResult *result,
                                     CamelFolderInfo **out_info,
                                     GError **error)
{
	ESimpleAsyncResult *simple;
	AsyncContext *async_context;

	g_return_val_if_fail (
		e_simple_async_result_is_valid (
		result, G_OBJECT (cache),
		mail_folder_cache_note_store), FALSE);

	simple = E_SIMPLE_ASYNC_RESULT (result);
	async_context = e_simple_async_result_get_op_pointer (simple);

	if (e_simple_async_result_propagate_error (simple, error))
		return FALSE;

	if (out_info != NULL) {
		if (async_context->info != NULL)
			*out_info = camel_folder_info_clone (
				async_context->info);
		else
			*out_info = NULL;
	}

	return TRUE;
}

/**
 * mail_folder_cache_note_folder:
 *
 * When a folder has been opened, notify it for watching.  The folder must have
 * already been created on the store (which has already been noted) before the
 * folder can be opened
 */
void
mail_folder_cache_note_folder (MailFolderCache *cache,
                               CamelFolder *folder)
{
	CamelStore *parent_store;
	CamelFolder *cached_folder;
	FolderInfo *folder_info;
	const gchar *full_name;

	g_return_if_fail (MAIL_IS_FOLDER_CACHE (cache));
	g_return_if_fail (CAMEL_IS_FOLDER (folder));

	full_name = camel_folder_get_full_name (folder);
	parent_store = camel_folder_get_parent_store (folder);

	folder_info = mail_folder_cache_ref_folder_info (
		cache, parent_store, full_name);

	/* XXX Not sure we should just be returning quietly here, but
	 *     the old code did.  Using g_return_if_fail() causes a few
	 *     warnings on startup which might be worth tracking down. */
	if (folder_info == NULL) {
		StoreInfo *store_info;
		gboolean retry = FALSE, renote_store = FALSE;

		store_info = mail_folder_cache_ref_store_info (cache, parent_store);
		if (!store_info)
			return;

		g_mutex_lock (&store_info->lock);
		if (store_info->first_update != E_FIRST_UPDATE_DONE) {
			/* The first update did not finish yet, thus add this as a pending
			   folder to be noted once the first update finishes */
			store_info->pending_folder_notes = g_slist_prepend (
				store_info->pending_folder_notes, g_object_ref (folder));

			if (store_info->first_update == E_FIRST_UPDATE_FAILED) {
				store_info->first_update = E_FIRST_UPDATE_RUNNING;
				renote_store = TRUE;
			}
		} else {
			/* It can be that certain threading interleaving made
			   the first store update finished before we reached
			   this place, thus retry to get the folder info */
			retry = TRUE;
		}
		g_mutex_unlock (&store_info->lock);

		store_info_unref (store_info);

		if (renote_store)
			mail_folder_cache_note_store (cache, parent_store, NULL, NULL, NULL);
		else if (retry)
			folder_info = mail_folder_cache_ref_folder_info (
				cache, parent_store, full_name);

		if (!folder_info)
			return;
	}

	g_mutex_lock (&folder_info->lock);

	cached_folder = g_weak_ref_get (&folder_info->folder);
	if (cached_folder != NULL) {
		g_signal_handler_disconnect (
			cached_folder,
			folder_info->folder_changed_handler_id);
		g_object_unref (cached_folder);
	}

	g_weak_ref_set (&folder_info->folder, folder);

	update_1folder (cache, folder_info, 0, NULL, NULL, NULL, NULL);

	folder_info->folder_changed_handler_id =
		g_signal_connect (
			folder, "changed",
			G_CALLBACK (folder_changed_cb), cache);

	g_mutex_unlock (&folder_info->lock);

	folder_info_unref (folder_info);
}

/**
 * mail_folder_cache_has_folder_info:
 * @cache: a #MailFolderCache
 * @store: a #CamelStore
 * @folder_name: a folder name
 *
 * Returns whether @cache has information about the folder described by
 * @store and @folder_name.  This does not necessarily mean it has the
 * #CamelFolder instance, but it at least has some meta-data about it.
 *
 * You can use this function as a folder existence test.
 *
 * Returns: %TRUE if @cache has folder info, %FALSE otherwise
 **/
gboolean
mail_folder_cache_has_folder_info (MailFolderCache *cache,
                                   CamelStore *store,
                                   const gchar *folder_name)
{
	FolderInfo *folder_info;
	gboolean has_info = FALSE;

	g_return_val_if_fail (MAIL_IS_FOLDER_CACHE (cache), FALSE);
	g_return_val_if_fail (CAMEL_IS_STORE (store), FALSE);
	g_return_val_if_fail (folder_name != NULL, FALSE);

	folder_info = mail_folder_cache_ref_folder_info (
		cache, store, folder_name);
	if (folder_info != NULL) {
		folder_info_unref (folder_info);
		has_info = TRUE;
	}

	return has_info;
}

/**
 * mail_folder_cache_ref_folder:
 * @cache: a #MailFolderCache
 * @store: a #CamelStore
 * @folder_name: a folder name
 *
 * Returns the #CamelFolder for @store and @folder_name if available, or
 * else %NULL if a #CamelFolder instance is not yet cached.  This function
 * does not block.
 *
 * The returned #CamelFolder is referenced for thread-safety and must be
 * unreferenced with g_object_unref() when finished with it.
 *
 * Returns: a #CamelFolder, or %NULL
 **/
CamelFolder *
mail_folder_cache_ref_folder (MailFolderCache *cache,
                              CamelStore *store,
                              const gchar *folder_name)
{
	FolderInfo *folder_info;
	CamelFolder *folder = NULL;

	g_return_val_if_fail (MAIL_IS_FOLDER_CACHE (cache), NULL);
	g_return_val_if_fail (CAMEL_IS_STORE (store), NULL);
	g_return_val_if_fail (folder_name != NULL, NULL);

	folder_info = mail_folder_cache_ref_folder_info (
		cache, store, folder_name);
	if (folder_info != NULL) {
		folder = g_weak_ref_get (&folder_info->folder);
		folder_info_unref (folder_info);
	}

	return folder;
}

/**
 * mail_folder_cache_get_folder_info_flags:
 * @cache: a #MailFolderCache
 * @store: a #CamelStore
 * @folder_name: a folder name
 * @flags: return location for #CamelFolderInfoFlags
 *
 * Obtains #CamelFolderInfoFlags for @store and @folder_name if available,
 * and returns %TRUE to indicate @flags was set.  If no folder information
 * is available for @store and @folder_name, the function returns %FALSE.
 *
 * Returns: whether @flags was set
 **/
gboolean
mail_folder_cache_get_folder_info_flags (MailFolderCache *cache,
                                         CamelStore *store,
                                         const gchar *folder_name,
                                         CamelFolderInfoFlags *flags)
{
	FolderInfo *folder_info;
	gboolean flags_set = FALSE;

	g_return_val_if_fail (MAIL_IS_FOLDER_CACHE (cache), FALSE);
	g_return_val_if_fail (CAMEL_IS_STORE (store), FALSE);
	g_return_val_if_fail (folder_name != NULL, FALSE);
	g_return_val_if_fail (flags != NULL, FALSE);

	folder_info = mail_folder_cache_ref_folder_info (
		cache, store, folder_name);
	if (folder_info != NULL) {
		*flags = folder_info->flags;
		folder_info_unref (folder_info);
		flags_set = TRUE;
	}

	return flags_set;
}

static void
mail_folder_cache_foreach_folder_uri_locked (MailFolderCache *cache,
					     GHashTable *uris,
					     MailFolderCacheForeachUriFunc func,
					     gpointer user_data)
{
	GHashTableIter iter;
	gpointer key;

	g_hash_table_iter_init (&iter, uris);
	while (g_hash_table_iter_next (&iter, &key, NULL)) {
		const gchar *uri = key;

		if (!func (uri, user_data))
			break;
	}
}

void
mail_folder_cache_foreach_local_folder_uri (MailFolderCache *cache,
					    MailFolderCacheForeachUriFunc func,
					    gpointer user_data)
{
	g_return_if_fail (MAIL_IS_FOLDER_CACHE (cache));
	g_return_if_fail (func != NULL);

	/* Reuse the store_info_ht_lock just because it's handy. */
	g_mutex_lock (&cache->priv->store_info_ht_lock);

	mail_folder_cache_foreach_folder_uri_locked (cache, cache->priv->local_folder_uris, func, user_data);

	g_mutex_unlock (&cache->priv->store_info_ht_lock);
}

void
mail_folder_cache_foreach_remote_folder_uri (MailFolderCache *cache,
					     MailFolderCacheForeachUriFunc func,
					     gpointer user_data)
{
	g_return_if_fail (MAIL_IS_FOLDER_CACHE (cache));
	g_return_if_fail (func != NULL);

	/* Reuse the store_info_ht_lock just because it's handy. */
	g_mutex_lock (&cache->priv->store_info_ht_lock);

	mail_folder_cache_foreach_folder_uri_locked (cache, cache->priv->remote_folder_uris, func, user_data);

	g_mutex_unlock (&cache->priv->store_info_ht_lock);
}

void
mail_folder_cache_service_removed (MailFolderCache *cache,
                                   CamelService *service)
{
	StoreInfo *store_info;

	g_return_if_fail (MAIL_IS_FOLDER_CACHE (cache));
	g_return_if_fail (CAMEL_IS_SERVICE (service));

	if (!CAMEL_IS_STORE (service))
		return;

	store_info = mail_folder_cache_steal_store_info (
		cache, CAMEL_STORE (service));
	if (store_info != NULL) {
		GList *list, *link;

		list = store_info_list_folder_info (store_info);

		for (link = list; link != NULL; link = g_list_next (link))
			unset_folder_info (cache, link->data, FALSE);

		g_list_free_full (list, (GDestroyNotify) folder_info_unref);

		store_info_unref (store_info);
	}
}

void
mail_folder_cache_service_enabled (MailFolderCache *cache,
                                   CamelService *service)
{
	g_return_if_fail (MAIL_IS_FOLDER_CACHE (cache));
	g_return_if_fail (CAMEL_IS_SERVICE (service));

	/* XXX This has no callback and it swallows errors.  Maybe
	 *     we don't want a service_enabled() function after all?
	 *     Call mail_folder_cache_note_store() directly instead
	 *     and handle errors appropriately. */
	mail_folder_cache_note_store (
		cache, CAMEL_STORE (service), NULL, NULL, NULL);
}

void
mail_folder_cache_service_disabled (MailFolderCache *cache,
                                    CamelService *service)
{
	g_return_if_fail (MAIL_IS_FOLDER_CACHE (cache));
	g_return_if_fail (CAMEL_IS_SERVICE (service));

	/* To the folder cache, disabling a service is the same as
	 * removing it.  We keep a separate callback function only
	 * to use as a breakpoint target in a debugger. */
	mail_folder_cache_service_removed (cache, service);
}
