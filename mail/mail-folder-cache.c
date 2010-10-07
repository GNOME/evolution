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
#include "config.h"
#endif

#include <string.h>
#include <time.h>

#include <glib.h>
#include <glib/gi18n.h>
#include <glib-object.h>
#include <glib/gstdio.h>

#include <libedataserver/e-data-server-util.h>
#include "e-util/e-marshal.h"
#include "e-util/e-util.h"

#include "mail-mt.h"
#include "mail-folder-cache.h"
#include "mail-ops.h"
#include "mail-session.h"
#include "mail-tools.h"

#include "em-utils.h"
#include "e-mail-local.h"

#define w(x)
#define d(x)

/* This code is a mess, there is no reason it should be so complicated. */

#define MAIL_FOLDER_CACHE_GET_PRIVATE(obj) \
	(G_TYPE_INSTANCE_GET_PRIVATE \
	((obj), MAIL_TYPE_FOLDER_CACHE, MailFolderCachePrivate))

struct _MailFolderCachePrivate {
	/* source id for the ping timeout callback */
	guint ping_id;
	/* Store to storeinfo table, active stores */
	GHashTable *stores;
	/* mutex to protect access to the stores hash */
	GMutex *stores_mutex;
	/* List of folder changes to be executed in gui thread */
	GQueue updates;
	/* event id for the async event of flushing all pending updates */
	gint update_id;
	/* hack for people who LIKE to have unsent count */
	gint count_sent;
	gint count_trash;
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
	struct _store_info *store_info;	/* 'parent' link */

	gchar *full_name;	/* full name of folder/folderinfo */
	gchar *uri;		/* uri of folder */

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
	gchar *uri;
	gchar *oldfull;
	gchar *olduri;

	gint unread;
	CamelStore *store;

	/* for only one new message... */
	gchar *msg_uid;     /* ... its uid ... */
	gchar *msg_sender;  /* ... its sender ... */
	gchar *msg_subject; /* ... and its subject. */
};

struct _store_info {
	GHashTable *folders;	/* by full_name */
	GHashTable *folders_uri; /* by uri */

	CamelStore *store;	/* the store for these folders */

	/* Outstanding folderinfo requests */
	GQueue folderinfo_updates;
};

G_DEFINE_TYPE (MailFolderCache, mail_folder_cache, G_TYPE_OBJECT)

static void
free_update(struct _folder_update *up)
{
	g_free (up->full_name);
	g_free (up->uri);
	if (up->store)
		g_object_unref (up->store);
	g_free (up->oldfull);
	g_free (up->olduri);
	g_free (up->msg_uid);
	g_free (up->msg_sender);
	g_free (up->msg_subject);
	g_free (up);
}

static void
real_flush_updates (gpointer o, gpointer event_data, gpointer data)
{
	struct _folder_update *up;
	MailFolderCache *self = (MailFolderCache*) o;

	g_mutex_lock (self->priv->stores_mutex);
	while ((up = g_queue_pop_head (&self->priv->updates)) != NULL) {
		g_mutex_unlock (self->priv->stores_mutex);

		if (up->remove) {
			if (up->delete) {
				g_signal_emit (self, signals[FOLDER_DELETED], 0, up->store, up->uri);
			} else
				g_signal_emit (self, signals[FOLDER_UNAVAILABLE], 0, up->store, up->uri);
		} else {
			if (up->olduri && up->add) {
				g_signal_emit (self, signals[FOLDER_RENAMED], 0, up->store, up->olduri, up->uri);
			}

			if (!up->olduri && up->add)
				g_signal_emit (self, signals[FOLDER_AVAILABLE], 0, up->store, up->uri);
		}

		/* update unread counts */
		g_signal_emit (self, signals[FOLDER_UNREAD_UPDATED], 0,
			       up->store, up->full_name, up->unread);

		/* indicate that the folder has changed (new mail received, etc) */
		if (up->uri) {
			g_signal_emit (
				self, signals[FOLDER_CHANGED], 0, up->store,
				up->uri, up->full_name, up->new, up->msg_uid,
				up->msg_sender, up->msg_subject);
		}

		if (CAMEL_IS_VEE_STORE (up->store) && !up->remove) {
			/* Normally the vfolder store takes care of the folder_opened event itself,
			   but we add folder to the noting system later, thus we do not know about
			   search folders to update them in a tree, thus ensure their changes will
			   be tracked correctly. */
			CamelFolder *folder = camel_store_get_folder (up->store, up->full_name, 0, NULL);

			if (folder) {
				mail_folder_cache_note_folder (self, folder);
				g_object_unref (folder);
			}
		}

		free_update(up);

		g_mutex_lock (self->priv->stores_mutex);
	}
	self->priv->update_id = -1;
	g_mutex_unlock (self->priv->stores_mutex);
}

static void
flush_updates (MailFolderCache *self)
{
	if (self->priv->update_id == -1 && !g_queue_is_empty (&self->priv->updates))
		self->priv->update_id = mail_async_event_emit (
			mail_async_event, MAIL_ASYNC_GUI,
			(MailAsyncFunc) real_flush_updates,
			self, NULL, NULL);
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
update_1folder (MailFolderCache *self,
                struct _folder_info *mfi,
                gint new,
                const gchar *msg_uid,
                const gchar *msg_sender,
                const gchar *msg_subject,
                CamelFolderInfo *info)
{
	struct _folder_update *up;
	CamelFolder *folder;
	gint unread = -1;
	gint deleted;

	folder = mfi->folder;
	if (folder) {
		gboolean is_drafts = FALSE, is_outbox = FALSE;

		d(printf("update 1 folder '%s'\n", folder->full_name));
		if ((self->priv->count_trash && (CAMEL_IS_VTRASH_FOLDER (folder)))
		    || (is_drafts = em_utils_folder_is_drafts (
			folder, info ? info->uri : NULL))
		    || (is_outbox = em_utils_folder_is_outbox (
			folder, info ? info->uri : NULL))
		    || (self->priv->count_sent && em_utils_folder_is_sent (
			folder, info ? info->uri : NULL))) {
			d(printf(" total count\n"));
			unread = camel_folder_get_message_count (folder);
			if (is_drafts || is_outbox) {
				guint32 junked = 0;

				if ((deleted = camel_folder_get_deleted_message_count (folder)) > 0)
					unread -= deleted;

				junked = folder->summary->junk_count;
				if (junked > 0)
					unread -= junked;
			}
		} else {
			d(printf(" unread count\n"));
			if (info)
				unread = info->unread;
			else
				unread = camel_folder_get_unread_message_count (folder);
		}
	} else if (info && !em_utils_folder_is_drafts (NULL, info->uri) && !em_utils_folder_is_outbox (NULL, info->uri))
		unread = info->unread;

	d(printf("folder updated: unread %d: '%s'\n", unread, mfi->full_name));

	if (unread == -1)
		return;

	up = g_malloc0(sizeof(*up));
	up->full_name = g_strdup(mfi->full_name);
	up->unread = unread;
	up->new = new;
	up->store = g_object_ref (mfi->store_info->store);
	up->uri = g_strdup(mfi->uri);
	up->msg_uid = g_strdup (msg_uid);
	up->msg_sender = g_strdup (msg_sender);
	up->msg_subject = g_strdup (msg_subject);
	g_queue_push_tail (&self->priv->updates, up);
	flush_updates(self);
}

static void
folder_changed_cb (CamelFolder *folder,
                   CamelFolderChangeInfo *changes,
                   MailFolderCache *self)
{
	static GHashTable *last_newmail_per_folder = NULL;
	time_t latest_received;
	CamelFolder *local_drafts;
	CamelFolder *local_outbox;
	CamelFolder *local_sent;
	CamelStore *parent_store;
	CamelMessageInfo *info;
	struct _store_info *si;
	struct _folder_info *mfi;
	const gchar *full_name;
	gint new = 0;
	gint i;
	guint32 flags;
	gchar *uid = NULL, *sender = NULL, *subject = NULL;

	full_name = camel_folder_get_full_name (folder);
	parent_store = camel_folder_get_parent_store (folder);

	if (!last_newmail_per_folder)
		last_newmail_per_folder = g_hash_table_new (g_direct_hash, g_direct_equal);

	/* it's fine to hash them by folder pointer here */
	latest_received = GPOINTER_TO_INT (
		g_hash_table_lookup (last_newmail_per_folder, folder));

	local_drafts = e_mail_local_get_folder (E_MAIL_FOLDER_DRAFTS);
	local_outbox = e_mail_local_get_folder (E_MAIL_FOLDER_OUTBOX);
	local_sent = e_mail_local_get_folder (E_MAIL_FOLDER_SENT);

	if (!CAMEL_IS_VEE_FOLDER(folder)
	    && folder != local_drafts
	    && folder != local_outbox
	    && folder != local_sent
	    && changes && (changes->uid_added->len > 0)) {
		/* for each added message, check to see that it is
		   brand new, not junk and not already deleted */
		for (i = 0; i < changes->uid_added->len; i++) {
			info = camel_folder_get_message_info (folder, changes->uid_added->pdata[i]);
			if (info) {
				flags = camel_message_info_flags (info);
				if (((flags & CAMEL_MESSAGE_SEEN) == 0) &&
				    ((flags & CAMEL_MESSAGE_JUNK) == 0) &&
				    ((flags & CAMEL_MESSAGE_DELETED) == 0) &&
				    (camel_message_info_date_received (info) > latest_received)) {
					latest_received = camel_message_info_date_received (info);
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
			GINT_TO_POINTER (latest_received));

	g_mutex_lock (self->priv->stores_mutex);
	if (self->priv->stores != NULL
	    && (si = g_hash_table_lookup(self->priv->stores, parent_store)) != NULL
	    && (mfi = g_hash_table_lookup(si->folders, full_name)) != NULL
	    && mfi->folder == folder) {
		update_1folder (self, mfi, new, uid, sender, subject, NULL);
	}
	g_mutex_unlock (self->priv->stores_mutex);

	g_free (uid);
	g_free (sender);
	g_free (subject);
}

static void
unset_folder_info (MailFolderCache *self,
                   struct _folder_info *mfi,
                   gint delete,
                   gint unsub)
{
	struct _folder_update *up;

	d(printf("unset folderinfo '%s'\n", mfi->uri));

	if (mfi->folder) {
		CamelFolder *folder = mfi->folder;

		g_signal_handlers_disconnect_by_func (
			folder, folder_changed_cb, self);

		g_object_remove_weak_pointer (
			G_OBJECT (mfi->folder), &mfi->folder);
	}

	if ((mfi->flags & CAMEL_FOLDER_NOSELECT) == 0) {
		up = g_malloc0(sizeof(*up));

		up->remove = TRUE;
		up->delete = delete;
		up->unsub = unsub;
		up->store = g_object_ref (mfi->store_info->store);
		up->full_name = g_strdup (mfi->full_name);
		up->uri = g_strdup(mfi->uri);

		g_queue_push_tail (&self->priv->updates, up);
		flush_updates(self);
	}
}

static void
free_folder_info(struct _folder_info *mfi)
{
	g_free(mfi->full_name);
	g_free(mfi->uri);
	g_free(mfi);
}

static void
setup_folder(MailFolderCache *self, CamelFolderInfo *fi, struct _store_info *si)
{
	struct _folder_info *mfi;
	struct _folder_update *up;

	mfi = g_hash_table_lookup(si->folders, fi->full_name);
	if (mfi) {
		update_1folder (self, mfi, 0, NULL, NULL, NULL, fi);
	} else {
		d(printf("Adding new folder: %s (%s)\n", fi->full_name, fi->uri));
		mfi = g_malloc0(sizeof(*mfi));
		mfi->full_name = g_strdup(fi->full_name);
		mfi->uri = g_strdup(fi->uri);
		mfi->store_info = si;
		mfi->flags = fi->flags;
		mfi->has_children = fi->child != NULL;

		g_hash_table_insert(si->folders, mfi->full_name, mfi);
		g_hash_table_insert(si->folders_uri, mfi->uri, mfi);

		up = g_malloc0(sizeof(*up));
		up->full_name = g_strdup(mfi->full_name);
		up->uri = g_strdup(fi->uri);
		up->unread = fi->unread;
		up->store = g_object_ref (si->store);

		if ((fi->flags & CAMEL_FOLDER_NOSELECT) == 0)
			up->add = TRUE;

		g_queue_push_tail (&self->priv->updates, up);
		flush_updates(self);
	}
}

static void
create_folders(MailFolderCache *self, CamelFolderInfo *fi, struct _store_info *si)
{
	d(printf("Setup new folder: %s\n  %s\n", fi->uri, fi->full_name));

	while (fi) {
		setup_folder(self, fi, si);

		if (fi->child)
			create_folders(self, fi->child, si);

		fi = fi->next;
	}
}

static void
store_folder_subscribed_cb (CamelStore *store,
                            CamelFolderInfo *info,
                            MailFolderCache *self)
{
	struct _store_info *si;

	g_mutex_lock (self->priv->stores_mutex);
	si = g_hash_table_lookup (self->priv->stores, store);
	if (si)
		setup_folder (self, info, si);
	g_mutex_unlock (self->priv->stores_mutex);
}

static void
store_folder_created_cb (CamelStore *store,
                         CamelFolderInfo *info,
                         MailFolderCache *cache)
{
	/* We only want created events to do more work
	 * if we dont support subscriptions. */
	if (!camel_store_supports_subscriptions (store))
		store_folder_subscribed_cb (store, info, cache);
}

static void
store_folder_opened_cb (CamelStore *store,
                        CamelFolder *folder,
                        MailFolderCache *self)
{
	mail_folder_cache_note_folder (self, folder);
}

static void
store_folder_unsubscribed_cb (CamelStore *store,
                              CamelFolderInfo *info,
                              MailFolderCache *self)
{
	struct _store_info *si;
	struct _folder_info *mfi;

	g_mutex_lock (self->priv->stores_mutex);
	si = g_hash_table_lookup (self->priv->stores, store);
	if (si) {
		mfi = g_hash_table_lookup(si->folders, info->full_name);
		if (mfi) {
			g_hash_table_remove (si->folders, mfi->full_name);
			g_hash_table_remove (si->folders_uri, mfi->uri);
			unset_folder_info (self, mfi, TRUE, TRUE);
			free_folder_info (mfi);
		}
	}
	g_mutex_unlock (self->priv->stores_mutex);
}

static void
store_folder_deleted_cb (CamelStore *store,
                         CamelFolderInfo *info,
                         MailFolderCache *self)
{
	/* We only want deleted events to do more work
	 * if we dont support subscriptions. */
	if (!camel_store_supports_subscriptions (store))
		store_folder_unsubscribed_cb (store, info, self);
}

static gchar *
folder_to_url(CamelStore *store, const gchar *full_name)
{
	CamelURL *url;
	gchar *out;

	url = camel_url_copy(((CamelService *)store)->url);
	if (((CamelService *)store)->provider->url_flags  & CAMEL_URL_FRAGMENT_IS_PATH) {
		camel_url_set_fragment(url, full_name);
	} else {
		gchar *name = g_alloca(strlen(full_name)+2);

		sprintf(name, "/%s", full_name);
		camel_url_set_path(url, name);
	}

	out = camel_url_to_string(url, CAMEL_URL_HIDE_ALL);
	camel_url_free(url);

	return out;
}

static void
rename_folders (MailFolderCache *self,
                struct _store_info *si,
                const gchar *oldbase,
                const gchar *newbase,
                CamelFolderInfo *fi)
{
	gchar *old, *olduri, *oldfile, *newuri, *newfile;
	struct _folder_info *mfi;
	struct _folder_update *up;
	const gchar *config_dir;

	up = g_malloc0(sizeof(*up));

	d(printf("oldbase '%s' newbase '%s' new '%s'\n", oldbase, newbase, fi->full_name));

	/* Form what was the old name, and try and look it up */
	old = g_strdup_printf("%s%s", oldbase, fi->full_name + strlen(newbase));
	mfi = g_hash_table_lookup(si->folders, old);
	if (mfi) {
		d(printf("Found old folder '%s' renaming to '%s'\n", mfi->full_name, fi->full_name));

		up->oldfull = mfi->full_name;
		up->olduri = mfi->uri;

		/* Its a rename op */
		g_hash_table_remove(si->folders, mfi->full_name);
		g_hash_table_remove(si->folders_uri, mfi->uri);
		mfi->full_name = g_strdup(fi->full_name);
		mfi->uri = g_strdup(fi->uri);
		mfi->flags = fi->flags;
		mfi->has_children = fi->child != NULL;

		g_hash_table_insert(si->folders, mfi->full_name, mfi);
		g_hash_table_insert(si->folders_uri, mfi->uri, mfi);
	} else {
		d(printf("Rename found a new folder? old '%s' new '%s'\n", old, fi->full_name));
		/* Its a new op */
		mfi = g_malloc0(sizeof(*mfi));
		mfi->full_name = g_strdup(fi->full_name);
		mfi->uri = g_strdup(fi->uri);
		mfi->store_info = si;
		mfi->flags = fi->flags;
		mfi->has_children = fi->child != NULL;

		g_hash_table_insert(si->folders, mfi->full_name, mfi);
		g_hash_table_insert(si->folders_uri, mfi->uri, mfi);
	}

	up->full_name = g_strdup(mfi->full_name);
	up->uri = g_strdup(mfi->uri);
	up->unread = fi->unread==-1?0:fi->unread;
	up->store = g_object_ref (si->store);

	if ((fi->flags & CAMEL_FOLDER_NOSELECT) == 0)
		up->add = TRUE;

	g_queue_push_tail (&self->priv->updates, up);
	flush_updates(self);
#if 0
	if (fi->sibling)
		rename_folders(self, si, oldbase, newbase, fi->sibling, folders);
	if (fi->child)
		rename_folders(self, si, oldbase, newbase, fi->child, folders);
#endif

	/* rename the meta-data we maintain ourselves */
	config_dir = mail_session_get_config_dir ();
	olduri = folder_to_url(si->store, old);
	e_filename_make_safe(olduri);
	newuri = folder_to_url(si->store, fi->full_name);
	e_filename_make_safe(newuri);
	oldfile = g_strdup_printf("%s/custom_view-%s.xml", config_dir, olduri);
	newfile = g_strdup_printf("%s/custom_view-%s.xml", config_dir, newuri);
	g_rename(oldfile, newfile);
	g_free(oldfile);
	g_free(newfile);
	oldfile = g_strdup_printf("%s/current_view-%s.xml", config_dir, olduri);
	newfile = g_strdup_printf("%s/current_view-%s.xml", config_dir, newuri);
	g_rename(oldfile, newfile);
	g_free(oldfile);
	g_free(newfile);
	g_free(olduri);
	g_free(newuri);

	g_free(old);
}

static void
get_folders(CamelFolderInfo *fi, GPtrArray *folders)
{
	while (fi) {
		g_ptr_array_add(folders, fi);

		if (fi->child)
			get_folders(fi->child, folders);

		fi = fi->next;
	}
}

static gint
folder_cmp(gconstpointer ap, gconstpointer bp)
{
	const CamelFolderInfo *a = ((CamelFolderInfo **)ap)[0];
	const CamelFolderInfo *b = ((CamelFolderInfo **)bp)[0];

	return strcmp(a->full_name, b->full_name);
}

static void
store_folder_renamed_cb (CamelStore *store,
                         const gchar *old_name,
                         CamelFolderInfo *info,
                         MailFolderCache *self)
{
	struct _store_info *si;

	g_mutex_lock (self->priv->stores_mutex);
	si = g_hash_table_lookup(self->priv->stores, store);
	if (si) {
		GPtrArray *folders = g_ptr_array_new();
		CamelFolderInfo *top;
		gint i;

		/* Ok, so for some reason the folderinfo we have comes in all messed up from
		   imap, should find out why ... this makes it workable */
		get_folders(info, folders);
		qsort(folders->pdata, folders->len, sizeof(folders->pdata[0]), folder_cmp);

		top = folders->pdata[0];
		for (i=0;i<folders->len;i++) {
			rename_folders(self, si, old_name, top->full_name, folders->pdata[i]);
		}

		g_ptr_array_free(folders, TRUE);

	}
	g_mutex_unlock (self->priv->stores_mutex);
}

struct _update_data {
	gint id;			/* id for cancellation */
	guint cancel:1;		/* also tells us we're cancelled */

	gboolean (*done)(CamelStore *store, CamelFolderInfo *info, gpointer data);
	gpointer data;
	MailFolderCache *cache;
};

static void
unset_folder_info_hash(gchar *path, struct _folder_info *mfi, gpointer data)
{
	MailFolderCache *self = (MailFolderCache*) data;
	unset_folder_info(self, mfi, FALSE, FALSE);
}

static void
free_folder_info_hash(gchar *path, struct _folder_info *mfi, gpointer data)
{
	free_folder_info(mfi);
}

static gboolean
update_folders(CamelStore *store, CamelFolderInfo *fi, gpointer data)
{
	struct _update_data *ud = data;
	struct _store_info *si;
	gboolean res = TRUE;

	d(printf("Got folderinfo for store %s\n", store->parent_object.provider->protocol));

	g_mutex_lock (ud->cache->priv->stores_mutex);
	si = g_hash_table_lookup(ud->cache->priv->stores, store);
	if (si && !ud->cancel) {
		/* the 'si' is still there, so we can remove ourselves from its list */
		/* otherwise its not, and we're on our own and free anyway */
		g_queue_remove (&si->folderinfo_updates, ud);

		if (fi)
			create_folders(ud->cache, fi, si);
	}
	g_mutex_unlock (ud->cache->priv->stores_mutex);

	if (ud->done)
		res = ud->done (store, fi, ud->data);
	g_free(ud);

	return res;
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
ping_store_exec (struct _ping_store_msg *m)
{
	gboolean online = FALSE;

	if (CAMEL_SERVICE (m->store)->status == CAMEL_SERVICE_CONNECTED) {
		if (CAMEL_IS_DISCO_STORE (m->store) &&
			camel_disco_store_status (
			CAMEL_DISCO_STORE (m->store)) !=CAMEL_DISCO_STORE_OFFLINE)
			online = TRUE;
		else if (CAMEL_IS_OFFLINE_STORE (m->store) &&
			CAMEL_OFFLINE_STORE (m->store)->state !=
			CAMEL_OFFLINE_STORE_NETWORK_UNAVAIL)
			online = TRUE;
	}
	if (online)
		camel_store_noop (m->store, &m->base.error);
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
	struct _ping_store_msg *m;

	if (CAMEL_SERVICE (store)->status != CAMEL_SERVICE_CONNECTED)
		return;

	m = mail_msg_new (&ping_store_info);
	m->store = g_object_ref (store);

	mail_msg_slow_ordered_push (m);
}

static gboolean
ping_cb (MailFolderCache *self)
{
	g_mutex_lock (self->priv->stores_mutex);

	g_hash_table_foreach (self->priv->stores, (GHFunc) ping_store, NULL);

	g_mutex_unlock (self->priv->stores_mutex);

	return TRUE;
}

static void
store_online_cb (CamelStore *store, gpointer data)
{
	struct _update_data *ud = data;

	g_mutex_lock (ud->cache->priv->stores_mutex);

	if (g_hash_table_lookup(ud->cache->priv->stores, store) != NULL && !ud->cancel) {
		/* re-use the cancel id.  we're already in the store update list too */
		ud->id = mail_get_folderinfo(store, NULL, update_folders, ud);
	} else {
		/* the store vanished, that means we were probably cancelled, or at any rate,
		   need to clean ourselves up */
		g_free(ud);
	}

	g_mutex_unlock (ud->cache->priv->stores_mutex);
}

struct _find_info {
	const gchar *uri;
	struct _folder_info *fi;
	CamelURL *url;
};

/* look up on each storeinfo using proper hash function for that stores uri's */
static void
storeinfo_find_folder_info (CamelStore *store,
                            struct _store_info *si,
                            struct _find_info *fi)
{
	if (fi->fi == NULL) {
		if (((CamelService *)store)->provider->url_equal (
			fi->url, ((CamelService *)store)->url)) {
			gchar *path = fi->url->fragment?fi->url->fragment:fi->url->path;

			if (path[0] == '/')
				path++;
			fi->fi = g_hash_table_lookup(si->folders, path);
		}
	}
}

static void
mail_folder_cache_finalize (GObject *object)
{
	MailFolderCache *cache = (MailFolderCache*) object;

	g_hash_table_destroy (cache->priv->stores);
	g_mutex_free (cache->priv->stores_mutex);

	if (cache->priv->ping_id > 0) {
		g_source_remove (cache->priv->ping_id);
		cache->priv->ping_id = 0;
	}

	if (cache->priv->update_id > 0) {
		g_source_remove (cache->priv->update_id);
		cache->priv->update_id = 0;
	}

	/* Chain up to parent's finalize() method. */
	G_OBJECT_CLASS (mail_folder_cache_parent_class)->finalize (object);
}

static void
mail_folder_cache_class_init (MailFolderCacheClass *class)
{
	GObjectClass *object_class;

	g_type_class_add_private (class, sizeof (MailFolderCachePrivate));

	object_class = G_OBJECT_CLASS (class);
	object_class->finalize = mail_folder_cache_finalize;

	/**
	 * MailFolderCache::folder-available
	 * @store: the #CamelStore containing the folder
	 * @uri: the uri of the folder
	 *
	 * Emitted when a folder becomes available
	 **/
	signals[FOLDER_AVAILABLE] =
		g_signal_new ("folder-available",
			      G_OBJECT_CLASS_TYPE (object_class),
			      G_SIGNAL_RUN_FIRST,
			      0, /* struct offset */
			      NULL, NULL, /* accumulator */
			      e_marshal_VOID__OBJECT_STRING,
			      G_TYPE_NONE, 2,
			      CAMEL_TYPE_OBJECT, G_TYPE_STRING);

	/**
	 * MailFolderCache::folder-unavailable
	 * @store: the #CamelStore containing the folder
	 * @uri: the uri of the folder
	 *
	 * Emitted when a folder becomes unavailable.  This represents a
	 * transient condition.  See MailFolderCache::folder-deleted to be
	 * notified when a folder is permanently removed.
	 **/
	signals[FOLDER_UNAVAILABLE] =
		g_signal_new ("folder-unavailable",
			      G_OBJECT_CLASS_TYPE (object_class),
			      G_SIGNAL_RUN_FIRST,
			      0, /* struct offset */
			      NULL, NULL, /* accumulator */
			      e_marshal_VOID__OBJECT_STRING,
			      G_TYPE_NONE, 2,
			      CAMEL_TYPE_OBJECT, G_TYPE_STRING);

	/**
	 * MailFolderCache::folder-deleted
	 * @store: the #CamelStore containing the folder
	 * @uri: the uri of the folder
	 *
	 * Emitted when a folder is deleted
	 **/
	signals[FOLDER_DELETED] =
		g_signal_new ("folder-deleted",
			      G_OBJECT_CLASS_TYPE (object_class),
			      G_SIGNAL_RUN_FIRST,
			      0, /* struct offset */
			      NULL, NULL, /* accumulator */
			      e_marshal_VOID__OBJECT_STRING,
			      G_TYPE_NONE, 2,
			      CAMEL_TYPE_OBJECT, G_TYPE_STRING);

	/**
	 * MailFolderCache::folder-renamed
	 * @store: the #CamelStore containing the folder
	 * @old_uri: the old uri of the folder
	 * @new_uri: the new uri of the folder
	 *
	 * Emitted when a folder is renamed
	 **/
	signals[FOLDER_RENAMED] =
		g_signal_new ("folder-renamed",
			      G_OBJECT_CLASS_TYPE (object_class),
			      G_SIGNAL_RUN_FIRST,
			      0, /* struct offset */
			      NULL, NULL, /* accumulator */
			      e_marshal_VOID__OBJECT_STRING_STRING,
			      G_TYPE_NONE, 3,
			      CAMEL_TYPE_OBJECT, G_TYPE_STRING, G_TYPE_STRING);

	/**
	 * MailFolderCache::folder-unread-updated
	 * @store: the #CamelStore containing the folder
	 * @name: the name of the folder
	 * @unread: the number of unread mails in the folder
	 *
	 * Emitted when a we receive an update to the unread count for a folder
	 **/
	signals[FOLDER_UNREAD_UPDATED] =
		g_signal_new ("folder-unread-updated",
			      G_OBJECT_CLASS_TYPE (object_class),
			      G_SIGNAL_RUN_FIRST,
			      0, /* struct offset */
			      NULL, NULL, /* accumulator */
			      e_marshal_VOID__OBJECT_STRING_INT,
			      G_TYPE_NONE, 3,
			      CAMEL_TYPE_OBJECT, G_TYPE_STRING, G_TYPE_INT);

	/**
	 * MailFolderCache::folder-changed
	 * @store: the #CamelStore containing the folder
	 * @folder_uri: the uri of the folder
	 * @folder_fullname: the full name of the folder
	 * @new_messages: the number of new messages for the folder
	 * @msg_uid: uid of the new message, or NULL
	 * @msg_sender: sender of the new message, or NULL
	 * @msg_subject: subject of the new message, or NULL
	 *
	 * Emitted when a folder has changed.  If @new_messages is not exactly 1,
	 * @msgt_uid, @msg_sender, and @msg_subject will be NULL.
	 **/
	signals[FOLDER_CHANGED] =
		g_signal_new ("folder-changed",
			      G_OBJECT_CLASS_TYPE (object_class),
			      G_SIGNAL_RUN_FIRST,
			      0, /* struct offset */
			      NULL, NULL, /* accumulator */
			      e_marshal_VOID__OBJECT_STRING_STRING_INT_STRING_STRING_STRING,
			      G_TYPE_NONE, 7,
			      CAMEL_TYPE_OBJECT, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_INT,
			      G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING);
}

static void
mail_folder_cache_init (MailFolderCache *self)
{
	const gchar *buf;
	guint timeout;

	self->priv = MAIL_FOLDER_CACHE_GET_PRIVATE (self);

	/* initialize values */
	self->priv->stores = g_hash_table_new(NULL, NULL);
	self->priv->stores_mutex = g_mutex_new ();

	g_queue_init (&self->priv->updates);
	self->priv->update_id = -1;
	self->priv->count_sent = getenv("EVOLUTION_COUNT_SENT") != NULL;
	self->priv->count_trash = getenv("EVOLUTION_COUNT_TRASH") != NULL;

	buf = getenv ("EVOLUTION_PING_TIMEOUT");
	timeout = buf ? strtoul (buf, NULL, 10) : 600;
	self->priv->ping_id = g_timeout_add_seconds (
		timeout, (GSourceFunc) ping_cb, self);
}

static MailFolderCache *default_cache = NULL;
G_LOCK_DEFINE_STATIC (default_cache);

/**
 * mail_folder_cache_get_default:
 *
 * Get the default folder cache object
 */
MailFolderCache *
mail_folder_cache_get_default (void)
{
	G_LOCK (default_cache);
	if (!default_cache)
		default_cache = g_object_new (MAIL_TYPE_FOLDER_CACHE, NULL);
	G_UNLOCK (default_cache);

	return default_cache;
}

/**
 * mail_folder_cache_note_store:
 *
 * Add a store whose folders should appear in the shell The folders are scanned
 * from the store, and/or added at runtime via the folder_created event.  The
 * @done function returns if we can free folder info.
 */
void
mail_folder_cache_note_store (MailFolderCache *self,
                              CamelStore *store,
                              CamelOperation *op,
                              NoteDoneFunc done,
                              gpointer data)
{
	struct _store_info *si;
	struct _update_data *ud;
	gint hook = 0;

	g_return_if_fail (CAMEL_IS_STORE(store));
	g_return_if_fail (mail_in_main_thread());

	g_mutex_lock (self->priv->stores_mutex);

	si = g_hash_table_lookup (self->priv->stores, store);
	if (si == NULL) {
		si = g_malloc0(sizeof(*si));
		si->folders = g_hash_table_new(g_str_hash, g_str_equal);
		si->folders_uri = g_hash_table_new (
			CAMEL_STORE_CLASS (
			CAMEL_OBJECT_GET_CLASS(store))->hash_folder_name,
			CAMEL_STORE_CLASS (
			CAMEL_OBJECT_GET_CLASS(store))->compare_folder_name);
		si->store = g_object_ref (store);
		g_hash_table_insert(self->priv->stores, store, si);
		g_queue_init (&si->folderinfo_updates);
		hook = TRUE;
	}

	ud = g_malloc(sizeof(*ud));
	ud->done = done;
	ud->data = data;
	ud->cancel = 0;
	ud->cache = self;

	/* We might get a race when setting up a store, such that it is
	 * still left in offline mode, after we've gone online.  This
	 * catches and fixes it up when the shell opens us. */
	if (CAMEL_IS_DISCO_STORE (store)) {
		if (camel_session_get_online (session) &&
			 camel_disco_store_status (CAMEL_DISCO_STORE (store)) ==
			CAMEL_DISCO_STORE_OFFLINE) {
			/* Note: we use the 'id' here, even though its not the right id, its still ok */
			ud->id = mail_store_set_offline (store, FALSE, store_online_cb, ud);
		} else {
			goto normal_setup;
		}
	} else if (CAMEL_IS_OFFLINE_STORE (store)) {
		if (camel_session_get_online (session) &&
			CAMEL_OFFLINE_STORE (store)->state ==
			CAMEL_OFFLINE_STORE_NETWORK_UNAVAIL) {
			/* Note: we use the 'id' here, even though its not the right id, its still ok */
			ud->id = mail_store_set_offline (store, FALSE, store_online_cb, ud);
		} else {
			goto normal_setup;
		}
	} else {
	normal_setup:
		ud->id = mail_get_folderinfo (store, op, update_folders, ud);
	}

	g_queue_push_tail (&si->folderinfo_updates, ud);

	g_mutex_unlock (self->priv->stores_mutex);

	/* there is potential for race here, but it is safe as we check
	 * for the store anyway */
	if (hook) {
		g_signal_connect (
			store, "folder-opened",
			G_CALLBACK (store_folder_opened_cb), self);
		g_signal_connect (
			store, "folder-created",
			G_CALLBACK (store_folder_created_cb), self);
		g_signal_connect (
			store, "folder-deleted",
			G_CALLBACK (store_folder_deleted_cb), self);
		g_signal_connect (
			store, "folder-renamed",
			G_CALLBACK (store_folder_renamed_cb), self);
		g_signal_connect (
			store, "folder-subscribed",
			G_CALLBACK (store_folder_subscribed_cb), self);
		g_signal_connect (
			store, "folder-unsubscribed",
			G_CALLBACK (store_folder_unsubscribed_cb), self);
	}
}

/**
 * mail_folder_cache_note_store_remove:
 *
 * Notify the cache that the specified @store can be removed from the cache
 */
void
mail_folder_cache_note_store_remove (MailFolderCache *self,
                                     CamelStore *store)
{
	struct _store_info *si;

	g_return_if_fail (CAMEL_IS_STORE(store));

	if (self->priv->stores == NULL)
		return;

	d(printf("store removed!!\n"));
	g_mutex_lock (self->priv->stores_mutex);
	si = g_hash_table_lookup (self->priv->stores, store);
	if (si) {
		GList *link;

		g_hash_table_remove(self->priv->stores, store);

		g_signal_handlers_disconnect_matched (
			store, G_SIGNAL_MATCH_DATA,
			0, 0, NULL, NULL, self);

		g_hash_table_foreach (
			si->folders, (GHFunc)
			unset_folder_info_hash, self);

		link = g_queue_peek_head_link (&si->folderinfo_updates);

		while (link != NULL) {
			struct _update_data *ud = link->data;

			d(printf("Cancelling outstanding folderinfo update %d\n", ud->id));
			mail_msg_cancel(ud->id);
			ud->cancel = 1;

			link = g_list_next (link);
		}

		g_object_unref (si->store);
		g_hash_table_foreach(si->folders, (GHFunc)free_folder_info_hash, NULL);
		g_hash_table_destroy(si->folders);
		g_hash_table_destroy(si->folders_uri);
		g_free(si);
	}

	g_mutex_unlock (self->priv->stores_mutex);
}

/**
 * mail_folder_cache_note_folder:
 *
 * When a folder has been opened, notify it for watching.  The folder must have
 * already been created on the store (which has already been noted) before the
 * folder can be opened
 */
void
mail_folder_cache_note_folder (MailFolderCache *self,
                               CamelFolder *folder)
{
	CamelStore *parent_store;
	struct _store_info *si;
	struct _folder_info *mfi;
	const gchar *full_name;

	full_name = camel_folder_get_full_name (folder);
	parent_store = camel_folder_get_parent_store (folder);

	g_mutex_lock (self->priv->stores_mutex);
	if (self->priv->stores == NULL
	    || (si = g_hash_table_lookup(self->priv->stores, parent_store)) == NULL
	    || (mfi = g_hash_table_lookup(si->folders, full_name)) == NULL) {
		w(g_warning("Noting folder before store initialised"));
		g_mutex_unlock (self->priv->stores_mutex);
		return;
	}

	/* dont do anything if we already have this */
	if (mfi->folder == folder) {
		g_mutex_unlock (self->priv->stores_mutex);
		return;
	}

	mfi->folder = folder;

	g_object_add_weak_pointer (G_OBJECT (folder), &mfi->folder);

	update_1folder (self, mfi, 0, NULL, NULL, NULL, NULL);

	g_mutex_unlock (self->priv->stores_mutex);

	g_signal_connect (
		folder, "changed",
		G_CALLBACK (folder_changed_cb), self);
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
mail_folder_cache_get_folder_from_uri (MailFolderCache *self,
                                       const gchar *uri,
                                       CamelFolder **folderp)
{
	struct _find_info fi = { uri, NULL, NULL };

	if (self->priv->stores == NULL)
		return FALSE;

	fi.url = camel_url_new (uri, NULL);

	g_mutex_lock (self->priv->stores_mutex);
	g_hash_table_foreach (
		self->priv->stores, (GHFunc)
		storeinfo_find_folder_info, &fi);
	if (folderp) {
		if (fi.fi && fi.fi->folder)
			*folderp = g_object_ref (fi.fi->folder);
		else
			*folderp = NULL;
	}
	g_mutex_unlock (self->priv->stores_mutex);

	camel_url_free (fi.url);

	return fi.fi != NULL;
}

gboolean
mail_folder_cache_get_folder_info_flags (MailFolderCache *self,
                                         CamelFolder *folder,
                                         gint *flags)
{
	gchar *uri = mail_tools_folder_to_url (folder);
	struct _find_info fi = { uri, NULL, NULL };

	if (self->priv->stores == NULL)
		return FALSE;

	fi.url = camel_url_new (uri, NULL);

	g_mutex_lock (self->priv->stores_mutex);
	g_hash_table_foreach (
		self->priv->stores, (GHFunc)
		storeinfo_find_folder_info, &fi);
	if (flags) {
		if (fi.fi)
			*flags = fi.fi->flags;
		else
			*flags = 0;
	}
	g_mutex_unlock (self->priv->stores_mutex);

	camel_url_free (fi.url);
	g_free (uri);

	return fi.fi != NULL;
}

/* Returns whether folder 'folder' has children based on folder_info->child property.
   If not found returns FALSE and sets 'found' to FALSE, if not NULL. */
gboolean
mail_folder_cache_get_folder_has_children (MailFolderCache *self,
                                           CamelFolder *folder,
                                           gboolean *found)
{
	gchar *uri = mail_tools_folder_to_url (folder);
	struct _find_info fi = { uri, NULL, NULL };

	g_return_val_if_fail (self != NULL, FALSE);
	g_return_val_if_fail (folder != NULL, FALSE);

	if (self->priv->stores == NULL)
		return FALSE;

	fi.url = camel_url_new (uri, NULL);

	g_mutex_lock (self->priv->stores_mutex);
	g_hash_table_foreach (
		self->priv->stores, (GHFunc)
		storeinfo_find_folder_info, &fi);
	if (found)
		*found = fi.fi != NULL;
	g_mutex_unlock (self->priv->stores_mutex);

	camel_url_free (fi.url);
	g_free (uri);

	return fi.fi != NULL && fi.fi->has_children;
}
