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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <string.h>
#include <time.h>

#include <glib/gi18n.h>
#include <glib/gstdio.h>

#include <libedataserver/libedataserver.h>

#include <libemail-engine/mail-mt.h>

#include "mail-folder-cache.h"
#include "e-mail-utils.h"
#include "e-mail-folder-utils.h"
#include "e-mail-session.h"
#include "e-mail-store-utils.h"

#define w(x)
#define d(x)

#define MAIL_FOLDER_CACHE_GET_PRIVATE(obj) \
	(G_TYPE_INSTANCE_GET_PRIVATE \
	((obj), MAIL_TYPE_FOLDER_CACHE, MailFolderCachePrivate))

/* This code is a mess, there is no reason it should be so complicated. */

typedef struct _StoreInfo StoreInfo;

struct _MailFolderCachePrivate {
	gpointer session;  /* weak pointer */

	/* source id for the ping timeout callback */
	guint ping_id;
	/* Store to storeinfo table, active stores */
	GHashTable *stores;
	/* mutex to protect access to the stores hash */
	GRecMutex stores_mutex;
	/* List of folder changes to be executed in gui thread */
	GQueue updates;
	/* idle source id for flushing all pending updates */
	guint update_id;
	/* hack for people who LIKE to have unsent count */
	gint count_sent;
	gint count_trash;

	GQueue local_folder_uris;
	GQueue remote_folder_uris;
};

enum {
	PROP_0,
	PROP_SESSION
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

struct _folder_info {
	StoreInfo *store_info;	/* 'parent' link */

	gchar *full_name;	/* full name of folder/folderinfo */

	guint32 flags;
	gboolean has_children;

	gpointer folder;	/* if known (weak pointer) */
};

/* pending list of updates */
struct _folder_update {
	guint remove:1;	/* removing from vfolders */
	guint delete:1;	/* deleting as well? */
	guint add:1;	/* add to vfolder */
	guint unsub:1;   /* unsubcribing? */
	guint new;     /* new mail arrived? */

	gchar *full_name;
	gchar *oldfull;

	gint unread;
	CamelStore *store;

	/* for only one new message... */
	gchar *msg_uid;     /* ... its uid ... */
	gchar *msg_sender;  /* ... its sender ... */
	gchar *msg_subject; /* ... and its subject. */
};

struct _StoreInfo {
	GHashTable *folders;	/* by full_name */
	CamelStore *store;	/* the store for these folders */
	gboolean first_update;	/* TRUE initially, then FALSE forever */

	/* Hold a reference to keep them alive. */
	CamelFolder *vjunk;
	CamelFolder *vtrash;

	/* Outstanding folderinfo requests */
	GQueue folderinfo_updates;
};

struct _update_data {
	NoteDoneFunc done;
	gpointer data;
	MailFolderCache *cache;
	GCancellable *cancellable;
};

G_DEFINE_TYPE (MailFolderCache, mail_folder_cache, G_TYPE_OBJECT)

static void
free_update (struct _folder_update *up)
{
	g_free (up->full_name);
	if (up->store)
		g_object_unref (up->store);
	g_free (up->oldfull);
	g_free (up->msg_uid);
	g_free (up->msg_sender);
	g_free (up->msg_subject);
	g_free (up);
}

static void
free_folder_info (struct _folder_info *mfi)
{
	g_free (mfi->full_name);
	g_free (mfi);
}

static StoreInfo *
store_info_new (CamelStore *store)
{
	StoreInfo *info;
	GHashTable *folders;

	folders = g_hash_table_new_full (
		(GHashFunc) g_str_hash,
		(GEqualFunc) g_str_equal,
		(GDestroyNotify) NULL,
		(GDestroyNotify) free_folder_info);

	info = g_slice_new0 (StoreInfo);
	info->folders = folders;
	info->store = g_object_ref (store);
	info->first_update = TRUE;

	/* If these are vfolders then they need to be opened
	 * now, otherwise they won't keep track of all folders. */
	if (store->flags & CAMEL_STORE_VJUNK)
		info->vjunk = camel_store_get_junk_folder_sync (
			store, NULL, NULL);
	if (store->flags & CAMEL_STORE_VTRASH)
		info->vtrash = camel_store_get_trash_folder_sync (
			store, NULL, NULL);

	g_queue_init (&info->folderinfo_updates);

	return info;
}

static void
store_info_free (StoreInfo *info)
{
	struct _update_data *ud;

	while (!g_queue_is_empty (&info->folderinfo_updates)) {
		ud = g_queue_pop_head (&info->folderinfo_updates);
		g_cancellable_cancel (ud->cancellable);
	}

	g_hash_table_destroy (info->folders);
	g_object_unref (info->store);

	if (info->vjunk != NULL)
		g_object_unref (info->vjunk);

	if (info->vtrash != NULL)
		g_object_unref (info->vtrash);

	g_slice_free (StoreInfo, info);
}

static gboolean
flush_updates_idle_cb (MailFolderCache *cache)
{
	struct _folder_update *up;

	g_rec_mutex_lock (&cache->priv->stores_mutex);
	while ((up = g_queue_pop_head (&cache->priv->updates)) != NULL) {
		g_rec_mutex_unlock (&cache->priv->stores_mutex);

		if (up->remove) {
			if (up->delete) {
				g_signal_emit (
					cache, signals[FOLDER_DELETED], 0,
					up->store, up->full_name);
			} else
				g_signal_emit (
					cache, signals[FOLDER_UNAVAILABLE], 0,
					up->store, up->full_name);
		} else {
			if (up->oldfull && up->add) {
				g_signal_emit (
					cache, signals[FOLDER_RENAMED], 0,
					up->store, up->oldfull, up->full_name);
			}

			if (!up->oldfull && up->add)
				g_signal_emit (
					cache, signals[FOLDER_AVAILABLE], 0,
					up->store, up->full_name);
		}

		/* update unread counts */
		g_signal_emit (
			cache, signals[FOLDER_UNREAD_UPDATED], 0,
			up->store, up->full_name, up->unread);

		/* indicate that the folder has changed (new mail received, etc) */
		if (up->store != NULL && up->full_name != NULL) {
			g_signal_emit (
				cache, signals[FOLDER_CHANGED], 0, up->store,
				up->full_name, up->new, up->msg_uid,
				up->msg_sender, up->msg_subject);
		}

		if (CAMEL_IS_VEE_STORE (up->store) && !up->remove) {
			/* Normally the vfolder store takes care of the
			 * folder_opened event itself, but we add folder to
			 * the noting system later, thus we do not know about
			 * search folders to update them in a tree, thus
			 * ensure their changes will be tracked correctly. */
			CamelFolder *folder;

			/* FIXME camel_store_get_folder_sync() may block. */
			folder = camel_store_get_folder_sync (
				up->store, up->full_name, 0, NULL, NULL);

			if (folder) {
				mail_folder_cache_note_folder (cache, folder);
				g_object_unref (folder);
			}
		}

		free_update (up);

		g_rec_mutex_lock (&cache->priv->stores_mutex);
	}
	cache->priv->update_id = 0;
	g_rec_mutex_unlock (&cache->priv->stores_mutex);

	return FALSE;
}

static void
flush_updates (MailFolderCache *cache)
{
	if (cache->priv->update_id > 0)
		return;

	if (g_queue_is_empty (&cache->priv->updates))
		return;

	cache->priv->update_id = g_idle_add (
		(GSourceFunc) flush_updates_idle_cb, cache);
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
                struct _folder_info *mfi,
                gint new,
                const gchar *msg_uid,
                const gchar *msg_sender,
                const gchar *msg_subject,
                CamelFolderInfo *info)
{
	EMailSession *session;
	ESourceRegistry *registry;
	struct _folder_update *up;
	CamelFolder *folder;
	gint unread = -1;
	gint deleted;

	session = mail_folder_cache_get_session (cache);
	registry = e_mail_session_get_registry (session);

	folder = mfi->folder;
	if (folder) {
		gboolean folder_is_sent;
		gboolean folder_is_drafts;
		gboolean folder_is_outbox;
		gboolean folder_is_vtrash;
		gboolean special_case;

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

				deleted = camel_folder_get_deleted_message_count (folder);
				if (deleted > 0)
					unread -= deleted;

				junked = camel_folder_summary_get_junk_count (folder->summary);
				if (junked > 0)
					unread -= junked;
			}
		} else {
			d (printf (" unread count\n"));
			if (info)
				unread = info->unread;
			else
				unread = camel_folder_get_unread_message_count (folder);
		}
	}

	d (printf ("folder updated: unread %d: '%s'\n", unread, mfi->full_name));

	if (unread == -1)
		return;

	up = g_malloc0 (sizeof (*up));
	up->full_name = g_strdup (mfi->full_name);
	up->unread = unread;
	up->new = new;
	up->store = g_object_ref (mfi->store_info->store);
	up->msg_uid = g_strdup (msg_uid);
	up->msg_sender = g_strdup (msg_sender);
	up->msg_subject = g_strdup (msg_subject);
	g_queue_push_tail (&cache->priv->updates, up);
	flush_updates (cache);
}

static void
folder_changed_cb (CamelFolder *folder,
                   CamelFolderChangeInfo *changes,
                   MailFolderCache *cache)
{
	static GHashTable *last_newmail_per_folder = NULL;
	time_t latest_received, new_latest_received;
	CamelFolder *local_drafts;
	CamelFolder *local_outbox;
	CamelFolder *local_sent;
	CamelSession *session;
	CamelStore *parent_store;
	CamelMessageInfo *info;
	StoreInfo *si;
	struct _folder_info *mfi;
	const gchar *full_name;
	gint new = 0;
	gint i;
	guint32 flags;
	gchar *uid = NULL, *sender = NULL, *subject = NULL;

	full_name = camel_folder_get_full_name (folder);
	parent_store = camel_folder_get_parent_store (folder);
	session = camel_service_get_session (CAMEL_SERVICE (parent_store));

	if (!last_newmail_per_folder)
		last_newmail_per_folder = g_hash_table_new (g_direct_hash, g_direct_equal);

	/* it's fine to hash them by folder pointer here */
	latest_received = GPOINTER_TO_INT (
		g_hash_table_lookup (last_newmail_per_folder, folder));
	new_latest_received = latest_received;

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
		/* for each added message, check to see that it is
		 * brand new, not junk and not already deleted */
		for (i = 0; i < changes->uid_added->len; i++) {
			info = camel_folder_get_message_info (
				folder, changes->uid_added->pdata[i]);
			if (info) {
				flags = camel_message_info_flags (info);
				if (((flags & CAMEL_MESSAGE_SEEN) == 0) &&
				    ((flags & CAMEL_MESSAGE_JUNK) == 0) &&
				    ((flags & CAMEL_MESSAGE_DELETED) == 0) &&
				    (camel_message_info_date_received (info) > latest_received)) {
					if (camel_message_info_date_received (info) > new_latest_received)
						new_latest_received = camel_message_info_date_received (info);
					new++;
					if (new == 1) {
						uid = g_strdup (camel_message_info_uid (info));
						sender = g_strdup (camel_message_info_from (info));
						subject = g_strdup (camel_message_info_subject (info));
					} else {
						g_free (uid);
						g_free (sender);
						g_free (subject);

						uid = NULL;
						sender = NULL;
						subject = NULL;
					}
				}
				camel_folder_free_message_info (folder, info);
			}
		}
	}

	if (new > 0)
		g_hash_table_insert (
			last_newmail_per_folder, folder,
			GINT_TO_POINTER (new_latest_received));

	g_rec_mutex_lock (&cache->priv->stores_mutex);
	if (cache->priv->stores != NULL
	    && (si = g_hash_table_lookup (cache->priv->stores, parent_store)) != NULL
	    && (mfi = g_hash_table_lookup (si->folders, full_name)) != NULL
	    && mfi->folder == folder) {
		update_1folder (cache, mfi, new, uid, sender, subject, NULL);
	}
	g_rec_mutex_unlock (&cache->priv->stores_mutex);

	g_free (uid);
	g_free (sender);
	g_free (subject);
}

static void
unset_folder_info (MailFolderCache *cache,
                   struct _folder_info *mfi,
                   gint delete,
                   gint unsub)
{
	struct _folder_update *up;

	d (printf ("unset folderinfo '%s'\n", mfi->uri));

	if (mfi->folder) {
		CamelFolder *folder = mfi->folder;

		g_signal_handlers_disconnect_by_func (
			folder, folder_changed_cb, cache);

		g_object_remove_weak_pointer (
			G_OBJECT (mfi->folder), &mfi->folder);
	}

	if ((mfi->flags & CAMEL_FOLDER_NOSELECT) == 0) {
		up = g_malloc0 (sizeof (*up));

		up->remove = TRUE;
		up->delete = delete;
		up->unsub = unsub;
		up->store = g_object_ref (mfi->store_info->store);
		up->full_name = g_strdup (mfi->full_name);

		g_queue_push_tail (&cache->priv->updates, up);
		flush_updates (cache);
	}
}

static void
setup_folder (MailFolderCache *cache,
              CamelFolderInfo *fi,
              StoreInfo *si)
{
	struct _folder_info *mfi;
	struct _folder_update *up;

	mfi = g_hash_table_lookup (si->folders, fi->full_name);
	if (mfi) {
		update_1folder (cache, mfi, 0, NULL, NULL, NULL, fi);
	} else {
		mfi = g_malloc0 (sizeof (*mfi));
		mfi->full_name = g_strdup (fi->full_name);
		mfi->store_info = si;
		mfi->flags = fi->flags;
		mfi->has_children = fi->child != NULL;

		g_hash_table_insert (si->folders, mfi->full_name, mfi);

		up = g_malloc0 (sizeof (*up));
		up->full_name = g_strdup (mfi->full_name);
		up->unread = fi->unread;
		up->store = g_object_ref (si->store);

		if ((fi->flags & CAMEL_FOLDER_NOSELECT) == 0)
			up->add = TRUE;

		g_queue_push_tail (&cache->priv->updates, up);
		flush_updates (cache);
	}
}

static void
create_folders (MailFolderCache *cache,
                CamelFolderInfo *fi,
                StoreInfo *si)
{
	while (fi) {
		setup_folder (cache, fi, si);

		if (fi->child)
			create_folders (cache, fi->child, si);

		fi = fi->next;
	}
}

static void
store_folder_subscribed_cb (CamelStore *store,
                            CamelFolderInfo *info,
                            MailFolderCache *cache)
{
	StoreInfo *si;

	g_rec_mutex_lock (&cache->priv->stores_mutex);
	si = g_hash_table_lookup (cache->priv->stores, store);
	if (si)
		setup_folder (cache, info, si);
	g_rec_mutex_unlock (&cache->priv->stores_mutex);
}

static void
store_folder_created_cb (CamelStore *store,
                         CamelFolderInfo *info,
                         MailFolderCache *cache)
{
	/* We only want created events to do more work
	 * if we dont support subscriptions. */
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
	StoreInfo *si;
	struct _folder_info *mfi;

	g_rec_mutex_lock (&cache->priv->stores_mutex);
	si = g_hash_table_lookup (cache->priv->stores, store);
	if (si) {
		mfi = g_hash_table_lookup (si->folders, info->full_name);
		if (mfi) {
			unset_folder_info (cache, mfi, TRUE, TRUE);
			g_hash_table_remove (si->folders, mfi->full_name);
		}
	}
	g_rec_mutex_unlock (&cache->priv->stores_mutex);
}

static void
store_folder_deleted_cb (CamelStore *store,
                         CamelFolderInfo *info,
                         MailFolderCache *cache)
{
	/* We only want deleted events to do more work
	 * if we dont support subscriptions. */
	if (!CAMEL_IS_SUBSCRIBABLE (store))
		store_folder_unsubscribed_cb (store, info, cache);
}

static void
rename_folders (MailFolderCache *cache,
                StoreInfo *si,
                const gchar *oldbase,
                const gchar *newbase,
                CamelFolderInfo *fi)
{
	gchar *old, *olduri, *oldfile, *newuri, *newfile;
	struct _folder_info *mfi;
	struct _folder_update *up;
	const gchar *config_dir;

	up = g_malloc0 (sizeof (*up));

	/* Form what was the old name, and try and look it up */
	old = g_strdup_printf ("%s%s", oldbase, fi->full_name + strlen (newbase));
	mfi = g_hash_table_lookup (si->folders, old);
	if (mfi) {
		up->oldfull = mfi->full_name;

		/* Be careful not to invoke the destroy function. */
		g_hash_table_steal (si->folders, mfi->full_name);

		/* Its a rename op */
		mfi->full_name = g_strdup (fi->full_name);
		mfi->flags = fi->flags;
		mfi->has_children = fi->child != NULL;

		g_hash_table_insert (si->folders, mfi->full_name, mfi);
	} else {
		/* Its a new op */
		mfi = g_malloc0 (sizeof (*mfi));
		mfi->full_name = g_strdup (fi->full_name);
		mfi->store_info = si;
		mfi->flags = fi->flags;
		mfi->has_children = fi->child != NULL;

		g_hash_table_insert (si->folders, mfi->full_name, mfi);
	}

	up->full_name = g_strdup (mfi->full_name);
	up->unread = fi->unread==-1 ? 0 : fi->unread;
	up->store = g_object_ref (si->store);

	if ((fi->flags & CAMEL_FOLDER_NOSELECT) == 0)
		up->add = TRUE;

	g_queue_push_tail (&cache->priv->updates, up);
	flush_updates (cache);

	/* rename the meta-data we maintain ourselves */
	config_dir = mail_session_get_config_dir ();
	olduri = e_mail_folder_uri_build (si->store, old);
	e_filename_make_safe (olduri);
	newuri = e_mail_folder_uri_build (si->store, fi->full_name);
	e_filename_make_safe (newuri);
	oldfile = g_strdup_printf ("%s/custom_view-%s.xml", config_dir, olduri);
	newfile = g_strdup_printf ("%s/custom_view-%s.xml", config_dir, newuri);
	g_rename (oldfile, newfile);
	g_free (oldfile);
	g_free (newfile);
	oldfile = g_strdup_printf ("%s/current_view-%s.xml", config_dir, olduri);
	newfile = g_strdup_printf ("%s/current_view-%s.xml", config_dir, newuri);
	g_rename (oldfile, newfile);
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
	StoreInfo *si;

	g_rec_mutex_lock (&cache->priv->stores_mutex);
	si = g_hash_table_lookup (cache->priv->stores, store);
	if (si) {
		GPtrArray *folders = g_ptr_array_new ();
		CamelFolderInfo *top;
		gint i;

		/* Ok, so for some reason the folderinfo we have comes in all messed up from
		 * imap, should find out why ... this makes it workable */
		get_folders (info, folders);
		qsort (folders->pdata, folders->len, sizeof (folders->pdata[0]), folder_cmp);

		top = folders->pdata[0];
		for (i = 0; i < folders->len; i++) {
			rename_folders (cache, si, old_name, top->full_name, folders->pdata[i]);
		}

		g_ptr_array_free (folders, TRUE);

	}
	g_rec_mutex_unlock (&cache->priv->stores_mutex);
}

static void
unset_folder_info_hash (gchar *path,
                        struct _folder_info *mfi,
                        gpointer data)
{
	MailFolderCache *cache = (MailFolderCache *) data;
	unset_folder_info (cache, mfi, FALSE, FALSE);
}

static void
mail_folder_cache_first_update (MailFolderCache *cache,
                                StoreInfo *info)
{
	EMailSession *session;
	const gchar *uid;

	session = mail_folder_cache_get_session (cache);
	uid = camel_service_get_uid (CAMEL_SERVICE (info->store));

	if (info->vjunk != NULL)
		mail_folder_cache_note_folder (cache, info->vjunk);

	if (info->vtrash != NULL)
		mail_folder_cache_note_folder (cache, info->vtrash);

	/* Some extra work for the "On This Computer" store. */
	if (g_strcmp0 (uid, E_MAIL_SESSION_LOCAL_UID) == 0) {
		CamelFolder *folder;
		gint ii;

		for (ii = 0; ii < E_MAIL_NUM_LOCAL_FOLDERS; ii++) {
			folder = e_mail_session_get_local_folder (session, ii);
			mail_folder_cache_note_folder (cache, folder);
		}
	}
}

static void
update_folders (CamelStore *store,
                GAsyncResult *result,
                struct _update_data *ud)
{
	CamelFolderInfo *fi;
	StoreInfo *si;
	GError *error = NULL;
	gboolean free_fi = TRUE;

	fi = camel_store_get_folder_info_finish (store, result, &error);

	/* Silently ignore cancellation errors. */
	if (g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED)) {
		g_error_free (error);

	} else if (error != NULL) {
		g_warning ("%s", error->message);
		g_error_free (error);
	}

	g_rec_mutex_lock (&ud->cache->priv->stores_mutex);
	si = g_hash_table_lookup (ud->cache->priv->stores, store);
	if (si && !g_cancellable_is_cancelled (ud->cancellable)) {
		/* The 'si' is still there, so we can remove ourselves from
		 * its list.  Or else its not, and we're on our own and free
		 * anyway. */
		g_queue_remove (&si->folderinfo_updates, ud);

		if (fi != NULL)
			create_folders (ud->cache, fi, si);
	}
	g_rec_mutex_unlock (&ud->cache->priv->stores_mutex);

	/* Do some extra work for the first update. */
	if (si != NULL && si->first_update) {
		mail_folder_cache_first_update (ud->cache, si);
		si->first_update = FALSE;
	}

	if (ud->done != NULL)
		free_fi = ud->done (ud->cache, store, fi, ud->data);
	if (fi && free_fi)
		camel_store_free_folder_info (store, fi);

	if (ud->cancellable != NULL)
		g_object_unref (ud->cancellable);

	g_free (ud);
}

struct _ping_store_msg {
	MailMsg base;
	CamelStore *store;
};

static gchar *
ping_store_desc (struct _ping_store_msg *m)
{
	gchar *service_name;
	gchar *msg;

	service_name = camel_service_get_name (CAMEL_SERVICE (m->store), TRUE);
	msg = g_strdup_printf (_("Pinging %s"), service_name);
	g_free (service_name);

	return msg;
}

static void
ping_store_exec (struct _ping_store_msg *m,
                 GCancellable *cancellable,
                 GError **error)
{
	CamelServiceConnectionStatus status;
	CamelService *service;
	gboolean online = FALSE;

	service = CAMEL_SERVICE (m->store);
	status = camel_service_get_connection_status (service);

	if (status == CAMEL_SERVICE_CONNECTED) {
		if (CAMEL_IS_DISCO_STORE (m->store) &&
			camel_disco_store_status (
			CAMEL_DISCO_STORE (m->store)) !=CAMEL_DISCO_STORE_OFFLINE)
			online = TRUE;
		else if (CAMEL_IS_OFFLINE_STORE (m->store) &&
			camel_offline_store_get_online (
			CAMEL_OFFLINE_STORE (m->store)))
			online = TRUE;
	}
	if (online)
		camel_store_noop_sync (m->store, cancellable, error);
}

static void
ping_store_free (struct _ping_store_msg *m)
{
	g_object_unref (m->store);
}

static MailMsgInfo ping_store_info = {
	sizeof (struct _ping_store_msg),
	(MailMsgDescFunc) ping_store_desc,
	(MailMsgExecFunc) ping_store_exec,
	(MailMsgDoneFunc) NULL,
	(MailMsgFreeFunc) ping_store_free
};

static void
ping_store (CamelStore *store)
{
	CamelServiceConnectionStatus status;
	CamelService *service;
	struct _ping_store_msg *m;

	service = CAMEL_SERVICE (store);
	status = camel_service_get_connection_status (service);

	if (status != CAMEL_SERVICE_CONNECTED)
		return;

	m = mail_msg_new (&ping_store_info);
	m->store = g_object_ref (store);

	mail_msg_slow_ordered_push (m);
}

static gboolean
ping_cb (MailFolderCache *cache)
{
	g_rec_mutex_lock (&cache->priv->stores_mutex);

	g_hash_table_foreach (cache->priv->stores, (GHFunc) ping_store, NULL);

	g_rec_mutex_unlock (&cache->priv->stores_mutex);

	return TRUE;
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
store_go_online_cb (CamelStore *store,
                    GAsyncResult *result,
                    struct _update_data *ud)
{
	/* FIXME Not checking result for error. */

	g_rec_mutex_lock (&ud->cache->priv->stores_mutex);

	if (g_hash_table_lookup (ud->cache->priv->stores, store) != NULL &&
		!g_cancellable_is_cancelled (ud->cancellable)) {
		/* We're already in the store update list. */
		if (store_has_folder_hierarchy (store))
			camel_store_get_folder_info (
				store, NULL,
				CAMEL_STORE_FOLDER_INFO_FAST |
				CAMEL_STORE_FOLDER_INFO_RECURSIVE |
				CAMEL_STORE_FOLDER_INFO_SUBSCRIBED,
				G_PRIORITY_DEFAULT, ud->cancellable,
				(GAsyncReadyCallback) update_folders, ud);
	} else {
		/* The store vanished, that means we were probably cancelled,
		 * or at any rate, need to clean ourselves up. */
		if (ud->cancellable != NULL)
			g_object_unref (ud->cancellable);
		g_free (ud);
	}

	g_rec_mutex_unlock (&ud->cache->priv->stores_mutex);
}

static GList *
find_folder_uri (GQueue *queue,
                 CamelSession *session,
                 const gchar *folder_uri)
{
	GList *head, *link;

	head = g_queue_peek_head_link (queue);

	for (link = head; link != NULL; link = g_list_next (link))
		if (e_mail_folder_uri_equal (session, link->data, folder_uri))
			break;

	return link;
}

struct _find_info {
	const gchar *folder_uri;
	struct _folder_info *fi;
};

static void
storeinfo_find_folder_info (CamelStore *store,
                            StoreInfo *si,
                            struct _find_info *fi)
{
	gchar *folder_name;
	gboolean success;

	if (fi->fi != NULL)
		return;

	success = e_mail_folder_uri_parse (
		camel_service_get_session (CAMEL_SERVICE (store)),
		fi->folder_uri, NULL, &folder_name, NULL);

	if (success) {
		fi->fi = g_hash_table_lookup (si->folders, folder_name);
		g_free (folder_name);
	}
}

static void
mail_folder_cache_set_session (MailFolderCache *cache,
                               EMailSession *session)
{
	g_return_if_fail (E_IS_MAIL_SESSION (session));
	g_return_if_fail (cache->priv->session == NULL);

	cache->priv->session = session;

	g_object_add_weak_pointer (
		G_OBJECT (cache->priv->session),
		&cache->priv->session);
}

static void
mail_folder_cache_set_property (GObject *object,
                                guint property_id,
                                const GValue *value,
                                GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_SESSION:
			mail_folder_cache_set_session (
				MAIL_FOLDER_CACHE (object),
				g_value_get_object (value));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
mail_folder_cache_get_property (GObject *object,
                                guint property_id,
                                GValue *value,
                                GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_SESSION:
			g_value_set_object (
				value,
				mail_folder_cache_get_session (
				MAIL_FOLDER_CACHE (object)));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
mail_folder_cache_dispose (GObject *object)
{
	MailFolderCachePrivate *priv;

	priv = MAIL_FOLDER_CACHE_GET_PRIVATE (object);

	if (priv->session != NULL) {
		g_object_remove_weak_pointer (
			G_OBJECT (priv->session), &priv->session);
		priv->session = NULL;
	}

	/* Chain up to parent's dispose() method. */
	G_OBJECT_CLASS (mail_folder_cache_parent_class)->dispose (object);
}

static void
mail_folder_cache_finalize (GObject *object)
{
	MailFolderCachePrivate *priv;

	priv = MAIL_FOLDER_CACHE_GET_PRIVATE (object);

	g_hash_table_destroy (priv->stores);
	g_rec_mutex_clear (&priv->stores_mutex);

	if (priv->ping_id > 0) {
		g_source_remove (priv->ping_id);
		priv->ping_id = 0;
	}

	if (priv->update_id > 0) {
		g_source_remove (priv->update_id);
		priv->update_id = 0;
	}

	while (!g_queue_is_empty (&priv->local_folder_uris))
		g_free (g_queue_pop_head (&priv->local_folder_uris));

	while (!g_queue_is_empty (&priv->remote_folder_uris))
		g_free (g_queue_pop_head (&priv->remote_folder_uris));

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
	GQueue *queue;
	gchar *folder_uri;

	/* Disregard virtual stores. */
	if (CAMEL_IS_VEE_STORE (store))
		return;

	/* Disregard virtual Junk folders. */
	if (store->flags & CAMEL_STORE_VJUNK)
		if (g_strcmp0 (folder_name, CAMEL_VJUNK_NAME) == 0)
			return;

	/* Disregard virtual Trash folders. */
	if (store->flags & CAMEL_STORE_VTRASH)
		if (g_strcmp0 (folder_name, CAMEL_VTRASH_NAME) == 0)
			return;

	service = CAMEL_SERVICE (store);
	session = camel_service_get_session (service);
	provider = camel_service_get_provider (service);

	/* Reuse the stores mutex just because it's handy. */
	g_rec_mutex_lock (&cache->priv->stores_mutex);

	folder_uri = e_mail_folder_uri_build (store, folder_name);

	if (provider->flags & CAMEL_PROVIDER_IS_REMOTE)
		queue = &cache->priv->remote_folder_uris;
	else
		queue = &cache->priv->local_folder_uris;

	if (find_folder_uri (queue, session, folder_uri) == NULL)
		g_queue_push_tail (queue, folder_uri);
	else
		g_free (folder_uri);

	g_rec_mutex_unlock (&cache->priv->stores_mutex);
}

static void
mail_folder_cache_folder_unavailable (MailFolderCache *cache,
                                      CamelStore *store,
                                      const gchar *folder_name)
{
	CamelService *service;
	CamelSession *session;
	CamelProvider *provider;
	GQueue *queue;
	GList *link;
	gchar *folder_uri;

	/* Disregard virtual stores. */
	if (CAMEL_IS_VEE_STORE (store))
		return;

	/* Disregard virtual Junk folders. */
	if (store->flags & CAMEL_STORE_VJUNK)
		if (g_strcmp0 (folder_name, CAMEL_VJUNK_NAME) == 0)
			return;

	/* Disregard virtual Trash folders. */
	if (store->flags & CAMEL_STORE_VTRASH)
		if (g_strcmp0 (folder_name, CAMEL_VTRASH_NAME) == 0)
			return;

	service = CAMEL_SERVICE (store);
	session = camel_service_get_session (service);
	provider = camel_service_get_provider (service);

	/* Reuse the stores mutex just because it's handy. */
	g_rec_mutex_lock (&cache->priv->stores_mutex);

	folder_uri = e_mail_folder_uri_build (store, folder_name);

	if (provider->flags & CAMEL_PROVIDER_IS_REMOTE)
		queue = &cache->priv->remote_folder_uris;
	else
		queue = &cache->priv->local_folder_uris;

	link = find_folder_uri (queue, session, folder_uri);
	if (link != NULL) {
		g_free (link->data);
		g_queue_delete_link (queue, link);
	}

	g_free (folder_uri);

	g_rec_mutex_unlock (&cache->priv->stores_mutex);
}

static void
mail_folder_cache_folder_deleted (MailFolderCache *cache,
                                  CamelStore *store,
                                  const gchar *folder_name)
{
	CamelService *service;
	CamelSession *session;
	GQueue *queue;
	GList *link;
	gchar *folder_uri;

	/* Disregard virtual stores. */
	if (CAMEL_IS_VEE_STORE (store))
		return;

	/* Disregard virtual Junk folders. */
	if (store->flags & CAMEL_STORE_VJUNK)
		if (g_strcmp0 (folder_name, CAMEL_VJUNK_NAME) == 0)
			return;

	/* Disregard virtual Trash folders. */
	if (store->flags & CAMEL_STORE_VTRASH)
		if (g_strcmp0 (folder_name, CAMEL_VTRASH_NAME) == 0)
			return;

	service = CAMEL_SERVICE (store);
	session = camel_service_get_session (service);

	/* Reuse the stores mutex just because it's handy. */
	g_rec_mutex_lock (&cache->priv->stores_mutex);

	folder_uri = e_mail_folder_uri_build (store, folder_name);

	queue = &cache->priv->local_folder_uris;
	link = find_folder_uri (queue, session, folder_uri);
	if (link != NULL) {
		g_free (link->data);
		g_queue_delete_link (queue, link);
	}

	queue = &cache->priv->remote_folder_uris;
	link = find_folder_uri (queue, session, folder_uri);
	if (link != NULL) {
		g_free (link->data);
		g_queue_delete_link (queue, link);
	}

	g_free (folder_uri);

	g_rec_mutex_unlock (&cache->priv->stores_mutex);
}

static void
mail_folder_cache_class_init (MailFolderCacheClass *class)
{
	GObjectClass *object_class;

	g_type_class_add_private (class, sizeof (MailFolderCachePrivate));

	object_class = G_OBJECT_CLASS (class);
	object_class->set_property = mail_folder_cache_set_property;
	object_class->get_property = mail_folder_cache_get_property;
	object_class->dispose = mail_folder_cache_dispose;
	object_class->finalize = mail_folder_cache_finalize;

	class->folder_available = mail_folder_cache_folder_available;
	class->folder_unavailable = mail_folder_cache_folder_unavailable;
	class->folder_deleted = mail_folder_cache_folder_deleted;

	g_object_class_install_property (
		object_class,
		PROP_SESSION,
		g_param_spec_object (
			"session",
			"Session",
			"Mail session",
			E_TYPE_MAIL_SESSION,
			G_PARAM_READWRITE |
			G_PARAM_CONSTRUCT_ONLY |
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
	const gchar *buf;
	guint timeout;

	cache->priv = MAIL_FOLDER_CACHE_GET_PRIVATE (cache);

	/* initialize values */
	cache->priv->stores = g_hash_table_new (NULL, NULL);
	g_rec_mutex_init (&cache->priv->stores_mutex);

	g_queue_init (&cache->priv->updates);
	cache->priv->count_sent = getenv ("EVOLUTION_COUNT_SENT") != NULL;
	cache->priv->count_trash = getenv ("EVOLUTION_COUNT_TRASH") != NULL;

	buf = getenv ("EVOLUTION_PING_TIMEOUT");
	timeout = buf ? strtoul (buf, NULL, 10) : 600;
	cache->priv->ping_id = g_timeout_add_seconds (
		timeout, (GSourceFunc) ping_cb, cache);

	g_queue_init (&cache->priv->local_folder_uris);
	g_queue_init (&cache->priv->remote_folder_uris);
}

MailFolderCache *
mail_folder_cache_new (EMailSession *session)
{
	g_return_val_if_fail (E_IS_MAIL_SESSION (session), NULL);

	return g_object_new (
		MAIL_TYPE_FOLDER_CACHE,
		"session", session, NULL);
}

EMailSession *
mail_folder_cache_get_session (MailFolderCache *cache)
{
	g_return_val_if_fail (MAIL_IS_FOLDER_CACHE (cache), NULL);

	return E_MAIL_SESSION (cache->priv->session);
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
                              NoteDoneFunc done,
                              gpointer data)
{
	CamelSession *session;
	StoreInfo *si;
	struct _update_data *ud;
	gint hook = 0;

	g_return_if_fail (MAIL_IS_FOLDER_CACHE (cache));
	g_return_if_fail (CAMEL_IS_STORE (store));

	session = camel_service_get_session (CAMEL_SERVICE (store));

	g_rec_mutex_lock (&cache->priv->stores_mutex);

	si = g_hash_table_lookup (cache->priv->stores, store);
	if (si == NULL) {
		si = store_info_new (store);
		g_hash_table_insert (cache->priv->stores, store, si);
		hook = TRUE;
	}

	ud = g_malloc0 (sizeof (*ud));
	ud->done = done;
	ud->data = data;
	ud->cache = cache;

	if (G_IS_CANCELLABLE (cancellable))
		ud->cancellable = g_object_ref (cancellable);

	/* We might get a race when setting up a store, such that it is
	 * still left in offline mode, after we've gone online.  This
	 * catches and fixes it up when the shell opens us. */
	if (CAMEL_IS_DISCO_STORE (store)) {
		if (camel_session_get_online (session) &&
			 camel_disco_store_status (CAMEL_DISCO_STORE (store)) ==
			CAMEL_DISCO_STORE_OFFLINE) {
			e_mail_store_go_online (
				store, G_PRIORITY_DEFAULT, cancellable,
				(GAsyncReadyCallback) store_go_online_cb, ud);
		} else {
			goto normal_setup;
		}
	} else if (CAMEL_IS_OFFLINE_STORE (store)) {
		if (camel_session_get_online (session) &&
			!camel_offline_store_get_online (
			CAMEL_OFFLINE_STORE (store))) {
			e_mail_store_go_online (
				store, G_PRIORITY_DEFAULT, cancellable,
				(GAsyncReadyCallback) store_go_online_cb, ud);
		} else {
			goto normal_setup;
		}
	} else {
	normal_setup:
		if (store_has_folder_hierarchy (store))
			camel_store_get_folder_info (
				store, NULL,
				CAMEL_STORE_FOLDER_INFO_FAST |
				CAMEL_STORE_FOLDER_INFO_RECURSIVE |
				CAMEL_STORE_FOLDER_INFO_SUBSCRIBED,
				G_PRIORITY_DEFAULT, cancellable,
				(GAsyncReadyCallback) update_folders, ud);
	}

	g_queue_push_tail (&si->folderinfo_updates, ud);

	g_rec_mutex_unlock (&cache->priv->stores_mutex);

	/* there is potential for race here, but it is safe as we check
	 * for the store anyway */
	if (hook) {
		g_signal_connect (
			store, "folder-opened",
			G_CALLBACK (store_folder_opened_cb), cache);
		g_signal_connect (
			store, "folder-created",
			G_CALLBACK (store_folder_created_cb), cache);
		g_signal_connect (
			store, "folder-deleted",
			G_CALLBACK (store_folder_deleted_cb), cache);
		g_signal_connect (
			store, "folder-renamed",
			G_CALLBACK (store_folder_renamed_cb), cache);
	}

	if (hook && CAMEL_IS_SUBSCRIBABLE (store)) {
		g_signal_connect (
			store, "folder-subscribed",
			G_CALLBACK (store_folder_subscribed_cb), cache);
		g_signal_connect (
			store, "folder-unsubscribed",
			G_CALLBACK (store_folder_unsubscribed_cb), cache);
	}
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
	StoreInfo *si;
	struct _folder_info *mfi;
	const gchar *full_name;

	g_return_if_fail (MAIL_IS_FOLDER_CACHE (cache));
	g_return_if_fail (CAMEL_IS_FOLDER (folder));

	full_name = camel_folder_get_full_name (folder);
	parent_store = camel_folder_get_parent_store (folder);

	g_rec_mutex_lock (&cache->priv->stores_mutex);
	if (cache->priv->stores == NULL
	    || (si = g_hash_table_lookup (cache->priv->stores, parent_store)) == NULL
	    || (mfi = g_hash_table_lookup (si->folders, full_name)) == NULL) {
		w (g_warning ("Noting folder before store initialised"));
		g_rec_mutex_unlock (&cache->priv->stores_mutex);
		return;
	}

	/* dont do anything if we already have this */
	if (mfi->folder == folder) {
		g_rec_mutex_unlock (&cache->priv->stores_mutex);
		return;
	}

	mfi->folder = folder;

	g_object_add_weak_pointer (G_OBJECT (folder), &mfi->folder);

	update_1folder (cache, mfi, 0, NULL, NULL, NULL, NULL);

	g_rec_mutex_unlock (&cache->priv->stores_mutex);

	g_signal_connect (
		folder, "changed",
		G_CALLBACK (folder_changed_cb), cache);
}

/**
 * mail_folder_cache_get_folder_from_uri:
 *
 * Gets the #CamelFolder for the supplied @uri.
 *
 * Returns: %TRUE if the URI is available, folderp is set to a reffed
 *          folder if the folder has also already been opened
 */
gboolean
mail_folder_cache_get_folder_from_uri (MailFolderCache *cache,
                                       const gchar *uri,
                                       CamelFolder **folderp)
{
	struct _find_info fi = { uri, NULL };

	if (cache->priv->stores == NULL)
		return FALSE;

	g_rec_mutex_lock (&cache->priv->stores_mutex);
	g_hash_table_foreach (
		cache->priv->stores, (GHFunc)
		storeinfo_find_folder_info, &fi);
	if (folderp) {
		if (fi.fi && fi.fi->folder)
			*folderp = g_object_ref (fi.fi->folder);
		else
			*folderp = NULL;
	}
	g_rec_mutex_unlock (&cache->priv->stores_mutex);

	return fi.fi != NULL;
}

gboolean
mail_folder_cache_get_folder_info_flags (MailFolderCache *cache,
                                         CamelFolder *folder,
                                         CamelFolderInfoFlags *flags)
{
	struct _find_info fi = { NULL, NULL };
	gchar *folder_uri;

	if (cache->priv->stores == NULL)
		return FALSE;

	folder_uri = e_mail_folder_uri_from_folder (folder);
	fi.folder_uri = folder_uri;

	g_rec_mutex_lock (&cache->priv->stores_mutex);
	g_hash_table_foreach (
		cache->priv->stores, (GHFunc)
		storeinfo_find_folder_info, &fi);
	if (flags) {
		if (fi.fi)
			*flags = fi.fi->flags;
		else
			*flags = 0;
	}
	g_rec_mutex_unlock (&cache->priv->stores_mutex);

	g_free (folder_uri);

	return fi.fi != NULL;
}

/* Returns whether folder 'folder' has children based on folder_info->child property.
 * If not found returns FALSE and sets 'found' to FALSE, if not NULL. */
gboolean
mail_folder_cache_get_folder_has_children (MailFolderCache *cache,
                                           CamelFolder *folder,
                                           gboolean *found)
{
	struct _find_info fi = { NULL, NULL };
	gchar *folder_uri;

	g_return_val_if_fail (MAIL_IS_FOLDER_CACHE (cache), FALSE);
	g_return_val_if_fail (CAMEL_IS_FOLDER (folder), FALSE);

	if (cache->priv->stores == NULL)
		return FALSE;

	folder_uri = e_mail_folder_uri_from_folder (folder);
	fi.folder_uri = folder_uri;

	g_rec_mutex_lock (&cache->priv->stores_mutex);
	g_hash_table_foreach (
		cache->priv->stores, (GHFunc)
		storeinfo_find_folder_info, &fi);
	if (found != NULL)
		*found = fi.fi != NULL;
	g_rec_mutex_unlock (&cache->priv->stores_mutex);

	g_free (folder_uri);

	return fi.fi != NULL && fi.fi->has_children;
}

void
mail_folder_cache_get_local_folder_uris (MailFolderCache *self,
                                         GQueue *out_queue)
{
	GList *head, *link;

	g_return_if_fail (MAIL_IS_FOLDER_CACHE (self));
	g_return_if_fail (out_queue != NULL);

	/* Reuse the stores mutex just because it's handy. */
	g_rec_mutex_lock (&self->priv->stores_mutex);

	head = g_queue_peek_head_link (&self->priv->local_folder_uris);

	for (link = head; link != NULL; link = g_list_next (link))
		g_queue_push_tail (out_queue, g_strdup (link->data));

	g_rec_mutex_unlock (&self->priv->stores_mutex);
}

void
mail_folder_cache_get_remote_folder_uris (MailFolderCache *self,
                                          GQueue *out_queue)
{
	GList *head, *link;

	g_return_if_fail (MAIL_IS_FOLDER_CACHE (self));
	g_return_if_fail (out_queue != NULL);

	/* Reuse the stores mutex just because it's handy. */
	g_rec_mutex_lock (&self->priv->stores_mutex);

	head = g_queue_peek_head_link (&self->priv->remote_folder_uris);

	for (link = head; link != NULL; link = g_list_next (link))
		g_queue_push_tail (out_queue, g_strdup (link->data));

	g_rec_mutex_unlock (&self->priv->stores_mutex);
}

void
mail_folder_cache_service_removed (MailFolderCache *cache,
                                   CamelService *service)
{
	StoreInfo *si;

	g_return_if_fail (MAIL_IS_FOLDER_CACHE (cache));
	g_return_if_fail (CAMEL_IS_SERVICE (service));

	if (cache->priv->stores == NULL)
		return;

	g_rec_mutex_lock (&cache->priv->stores_mutex);

	si = g_hash_table_lookup (cache->priv->stores, service);
	if (si != NULL) {
		g_hash_table_remove (cache->priv->stores, service);

		g_signal_handlers_disconnect_matched (
			service, G_SIGNAL_MATCH_DATA,
			0, 0, NULL, NULL, cache);

		g_hash_table_foreach (
			si->folders, (GHFunc)
			unset_folder_info_hash, cache);

		store_info_free (si);
	}

	g_rec_mutex_unlock (&cache->priv->stores_mutex);
}

void
mail_folder_cache_service_enabled (MailFolderCache *cache,
                                   CamelService *service)
{
	g_return_if_fail (MAIL_IS_FOLDER_CACHE (cache));
	g_return_if_fail (CAMEL_IS_SERVICE (service));

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
