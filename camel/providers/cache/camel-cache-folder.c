/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* camel-cache-folder.c : class for a cache folder */

/* 
 * Authors:
 *   Dan Winship <danw@helixcode.com>
 *
 * Copyright (C) 2000 Helix Code, Inc. (www.helixcode.com)
 *
 * This program is free software; you can redistribute it and/or 
 * modify it under the terms of the GNU General Public License as 
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307
 * USA
 */


/*
 * Notes on the cache provider:
 *
 * We require that the remote folder have persistent UIDs, and nothing
 * else. We require that the local store folder have persistent UIDs
 * and summary capability.
 *
 * If the remote folder does not have summary capability, we will need
 * to sync any new messages over to the local folder when the folder
 * is opened or when it changes. If the remote folder does have
 * summary capability, we can be more relaxed about doing this.
 *
 * If the remote folder has search capability, we will use it, at
 * least when the folder isn't synced. Otherwise if the local folder
 * has search capability, we will use that (but it will require
 * syncing the remote folder locally to use). Otherwise the cache
 * folder won't have search capability.
 *
 * CamelCacheFolder UIDs are remote UIDs, because we need to be able
 * to return a complete list of them at get_uids time, and the
 * messages might not all be present in the local folder, and we can't
 * predict what UIDs will be assigned to them when they are cached
 * there. We keep hash tables mapping remote to local UIDs and vice
 * versa, and a map file to cache this information between sessions.
 * The maps must always be 100% accurate.
 *
 * The messages in the local folder may not be in the same order as
 * the messages in the remote folder.
 *
 *
 * Many operations on the local folder are done with a NULL
 * CamelException, because having them fail only results in efficiency
 * problems, not actual permanent failures. (Eg, get_message will
 * try to append the message to the local folder, but doesn't check
 * for failure, because it already has the message to pass back to the
 * user.)
 */

#include "camel-cache-folder.h"
#include "camel-cache-store.h"
#include <camel/camel-exception.h>
#include <camel/camel-medium.h>

static CamelFolderClass *parent_class;
#define CF_CLASS(so) CAMEL_FOLDER_CLASS (GTK_OBJECT(so)->klass)

static void init (CamelFolder *folder, CamelStore *parent_store,
		  CamelFolder *parent_folder, const gchar *name,
		  gchar *separator, gboolean path_begins_with_sep,
		  CamelException *ex);

static void cache_sync (CamelFolder *folder, gboolean expunge, 
			CamelException *ex);

static void expunge (CamelFolder *folder, CamelException *ex);

static gint get_message_count (CamelFolder *folder);

static void append_message (CamelFolder *folder, CamelMimeMessage *message, 
			    guint32 flags, CamelException *ex);

static guint32 get_message_flags (CamelFolder *folder, const char *uid);
static void set_message_flags (CamelFolder *folder, const char *uid,
			       guint32 flags, guint32 set);
static gboolean get_message_user_flag (CamelFolder *folder, const char *uid,
				       const char *name);
static void set_message_user_flag (CamelFolder *folder, const char *uid,
				   const char *name, gboolean value);

static CamelMimeMessage *get_message (CamelFolder *folder, 
				      const gchar *uid,
				      CamelException *ex);

static GPtrArray *get_uids (CamelFolder *folder);
static GPtrArray *get_summary (CamelFolder *folder);
static GPtrArray *get_subfolder_names (CamelFolder *folder);
static void free_subfolder_names (CamelFolder *folder, GPtrArray *subfolders);

static GPtrArray *search_by_expression (CamelFolder *folder,
					const char *expression,
					CamelException *ex);

static const CamelMessageInfo *get_message_info (CamelFolder *folder,
						 const char *uid);

static void finalize (GtkObject *object);

static void
camel_cache_folder_class_init (CamelCacheFolderClass *camel_cache_folder_class)
{
	CamelFolderClass *camel_folder_class =
		CAMEL_FOLDER_CLASS (camel_cache_folder_class);
	GtkObjectClass *gtk_object_class =
		GTK_OBJECT_CLASS (camel_cache_folder_class);

	parent_class = gtk_type_class (camel_folder_get_type ());

	/* virtual method overload */
	camel_folder_class->init = init;
	camel_folder_class->sync = cache_sync;
	camel_folder_class->expunge = expunge;
	camel_folder_class->get_message_count = get_message_count;
	camel_folder_class->append_message = append_message;
	camel_folder_class->get_message_flags = get_message_flags;
	camel_folder_class->set_message_flags = set_message_flags;
	camel_folder_class->get_message_user_flag = get_message_user_flag;
	camel_folder_class->set_message_user_flag = set_message_user_flag;
	camel_folder_class->get_message = get_message;
	camel_folder_class->get_uids = get_uids;
	camel_folder_class->free_uids = camel_folder_free_nop;
	camel_folder_class->get_summary = get_summary;
	camel_folder_class->free_summary = camel_folder_free_nop;
	camel_folder_class->get_subfolder_names = get_subfolder_names;
	camel_folder_class->free_subfolder_names = free_subfolder_names;
	camel_folder_class->search_by_expression = search_by_expression;
	camel_folder_class->get_message_info = get_message_info;

	gtk_object_class->finalize = finalize;
}

GtkType
camel_cache_folder_get_type (void)
{
	static GtkType camel_cache_folder_type = 0;

	if (!camel_cache_folder_type) {
		GtkTypeInfo camel_cache_folder_info =	
		{
			"CamelCacheFolder",
			sizeof (CamelCacheFolder),
			sizeof (CamelCacheFolderClass),
			(GtkClassInitFunc) camel_cache_folder_class_init,
			(GtkObjectInitFunc) NULL,
				/* reserved_1 */ NULL,
				/* reserved_2 */ NULL,
			(GtkClassInitFunc) NULL,
		};

		camel_cache_folder_type = gtk_type_unique (CAMEL_FOLDER_TYPE, &camel_cache_folder_info);
	}

	return camel_cache_folder_type;
}


static void
cache_free_summary (CamelCacheFolder *cache_folder)
{
	if (cache_folder->remote_summary) {
		camel_folder_free_summary (cache_folder->remote,
					   cache_folder->summary);
	} else {
		int i;

		for (i = 0; i < cache_folder->summary->len; i++) {
			camel_message_info_free (
				cache_folder->summary->pdata[i]);
		}
		g_ptr_array_free (cache_folder->summary, TRUE);
		g_hash_table_destroy (cache_folder->summary_uids);
	}
}

static void
finalize (GtkObject *object)
{
	CamelCacheFolder *cache_folder = CAMEL_CACHE_FOLDER (object);

	if (cache_folder->uids) {
		camel_folder_free_uids (cache_folder->remote,
					cache_folder->uids);
	}
	if (cache_folder->summary)
		cache_free_summary (cache_folder);

	if (cache_folder->uidmap)
		camel_cache_map_destroy (cache_folder->uidmap);

	gtk_object_unref (GTK_OBJECT (cache_folder->local));
	gtk_object_unref (GTK_OBJECT (cache_folder->remote));

	g_free (cache_folder->mapfile);

	GTK_OBJECT_CLASS (parent_class)->finalize (object);
}


static void
update (CamelCacheFolder *cache_folder, CamelException *ex)
{
	if (cache_folder->uids) {
		camel_folder_free_uids (cache_folder->remote,
					cache_folder->uids);
	}
	cache_folder->uids = camel_folder_get_uids (cache_folder->remote);

	if (cache_folder->summary)
		cache_free_summary (cache_folder);

	if (cache_folder->remote_summary) {
		cache_folder->summary =
			camel_folder_get_summary (cache_folder->remote);
	} else {
		CamelMessageInfo *mi;
		GPtrArray *lsummary;
		const char *ruid;
		int i;

		if (!cache_folder->is_synced) {
			camel_cache_folder_sync (cache_folder, ex);
			if (camel_exception_is_set (ex))
				return;
		}

		cache_folder->summary = g_ptr_array_new ();
		cache_folder->summary_uids = g_hash_table_new (g_str_hash,
							       g_str_equal);

		lsummary = camel_folder_get_summary (cache_folder->local);

		/* For each local message, duplicate its info, replace
		 * the uid with the remote one, and add it to the
		 * uid->info cache.
		 */
		for (i = 0; i < lsummary->len; i++) {
			mi = lsummary->pdata[i];
			ruid = camel_cache_map_get_remote (cache_folder->uidmap, mi->uid);
			if (!ruid) {
				/* Stale message. Delete it from cache. */
				camel_folder_delete_message (
					cache_folder->local, mi->uid);
				continue;
			}

			mi = g_new (CamelMessageInfo, 1);
			camel_message_info_dup_to (lsummary->pdata[i], mi);
			g_free (mi->uid);
			mi->uid = g_strdup (ruid);
			g_hash_table_insert (cache_folder->summary_uids,
					     mi->uid, mi);
		}
		camel_folder_free_summary (cache_folder->local, lsummary);

		/* Now build the summary array in remote UID order. */
		for (i = 0; i < cache_folder->uids->len; i++) {
			mi = g_hash_table_lookup (cache_folder->summary_uids,
						  cache_folder->uids->pdata[i]);
			g_ptr_array_add (cache_folder->summary, mi);
		}
	}
	
}

static void
init (CamelFolder *folder, CamelStore *parent_store,
      CamelFolder *parent_folder, const gchar *name, gchar *separator,
      gboolean path_begins_with_sep, CamelException *ex)
{
	CamelCacheFolder *cache_folder = (CamelCacheFolder *)folder;
	char *path;

	CF_CLASS (folder)->init (folder, parent_store, parent_folder,
				 name, separator, path_begins_with_sep, ex);
	if (camel_exception_is_set (ex))
		return;

	folder->permanent_flags =
		camel_folder_get_permanent_flags (cache_folder->local);
	folder->can_hold_folders = cache_folder->remote->can_hold_folders;
	folder->can_hold_messages = cache_folder->remote->can_hold_messages;
	folder->has_summary_capability = TRUE;
	folder->has_search_capability =
		camel_folder_has_search_capability (cache_folder->local) ||
		camel_folder_has_search_capability (cache_folder->remote);

	cache_folder->remote_summary =
		camel_folder_has_summary_capability (cache_folder->remote);

	/* Load UIDs, summary, etc. */
	path = CAMEL_SERVICE (cache_folder->local->parent_store)->url->path;
	cache_folder->mapfile = g_strdup_printf ("%s/%s.map", path, name);
	cache_folder->uidmap = camel_cache_map_new ();
	camel_cache_map_read (cache_folder->uidmap, cache_folder->mapfile, ex);
	if (camel_exception_is_set (ex))
		return;
	update (cache_folder, ex);

	return;
}

/* If the remote folder changes, cache the new messages if necessary,
 * update the summary, and propagate the signal.
 */
static void
remote_folder_changed (CamelFolder *remote_folder, int type, gpointer data)
{
	CamelCacheFolder *cache_folder = data;

	update (cache_folder, NULL);
	gtk_signal_emit_by_name (GTK_OBJECT (cache_folder), "folder_changed",
				 type);
}

/* If the local folder changes, it's because we just cached a message
 * or expunged messages. Look for new messages and update the UID maps.
 */
static void
local_folder_changed (CamelFolder *local_folder, int type, gpointer data)
{
	CamelCacheFolder *cache_folder = data;
	CamelMimeMessage *msg;
	GPtrArray *new_luids;
	char *luid;
	const char *ruid;
	int i;

	/* Get the updated list of local UIDs. For any that we didn't
	 * already know about, figure out the corresponding remote
	 * UID.
	 */
	new_luids = camel_folder_get_uids (local_folder);
	for (i = 0; i < new_luids->len; i++) {
		luid = new_luids->pdata[i];
		if (!camel_cache_map_get_remote (cache_folder->uidmap, luid)) {
			msg = camel_folder_get_message (local_folder,
							luid, NULL);
			if (!msg)
				continue; /* Hrmph. */
			ruid = camel_medium_get_header (CAMEL_MEDIUM (msg),
							"X-Evolution-Remote-UID");
			if (!ruid) {
				/* How'd that get here? */
				camel_folder_delete_message (local_folder,
							     luid);
				continue;
			}

			camel_cache_map_update (cache_folder->uidmap,
						luid, ruid);
		}
	}
	camel_folder_free_uids (local_folder, new_luids);

	/* FIXME: the uidmaps contain bad data now. */
}

/* DONE */
static void
cache_sync (CamelFolder *folder, gboolean expunge, CamelException *ex)
{
	CamelCacheFolder *cache_folder = (CamelCacheFolder *)folder;

	camel_folder_sync (cache_folder->remote, expunge, ex);
	if (!camel_exception_is_set (ex)) {
		camel_folder_sync (cache_folder->local, expunge, NULL);
		camel_cache_map_write (cache_folder->uidmap,
				       cache_folder->mapfile, ex);
	}
}

/* DONE */
static void
expunge (CamelFolder *folder, CamelException *ex)
{
	CamelCacheFolder *cache_folder = (CamelCacheFolder *)folder;

	camel_folder_expunge (cache_folder->remote, ex);
	if (!camel_exception_is_set (ex))
		camel_folder_expunge (cache_folder->local, NULL);
}

/* DONE */
static gint
get_message_count (CamelFolder *folder)
{
	CamelCacheFolder *cache_folder = (CamelCacheFolder *)folder;

	return cache_folder->summary->len;
}

/* DONE */
static void
append_message (CamelFolder *folder, CamelMimeMessage *message, 
		guint32 flags, CamelException *ex)
{
	CamelCacheFolder *cache_folder = (CamelCacheFolder *)folder;

	/* We'd like to cache this locally as well, but we have no
	 * 100% reliable way to determine the UID assigned to the
	 * remote message, so we can't.
	 */
	camel_folder_append_message (cache_folder->remote, message, flags, ex);
}

/* DONE */
static guint32
get_message_flags (CamelFolder *folder, const char *uid)
{
	const CamelMessageInfo *mi;

	mi = get_message_info (folder, uid);
	g_return_val_if_fail (mi != NULL, 0);
	return mi->flags;
}

/* DONE */
static void
set_message_flags (CamelFolder *folder, const char *uid,
		   guint32 flags, guint32 set)
{
	CamelCacheFolder *cache_folder = (CamelCacheFolder *)folder;
	const char *luid;

	luid = camel_cache_map_get_local (cache_folder->uidmap, uid);
	if (luid) {
		camel_folder_set_message_flags (cache_folder->local, luid,
						flags, set);
	}
	camel_folder_set_message_flags (cache_folder->remote, uid, flags, set);
}

/* DONE */
static gboolean
get_message_user_flag (CamelFolder *folder, const char *uid, const char *name)
{
	const CamelMessageInfo *mi;

	mi = get_message_info (folder, uid);
	g_return_val_if_fail (mi != NULL, 0);
	return camel_flag_get ((CamelFlag **)&mi->user_flags, name);
}

/* DONE */
static void
set_message_user_flag (CamelFolder *folder, const char *uid,
		       const char *name, gboolean value)
{
	CamelCacheFolder *cache_folder = (CamelCacheFolder *)folder;
	const char *luid;

	luid = camel_cache_map_get_local (cache_folder->uidmap, uid);
	if (luid) {
		camel_folder_set_message_user_flag (cache_folder->local, luid,
						    name, value);
	}
	camel_folder_set_message_user_flag (cache_folder->remote, uid,
					    name, value);
}


/* DONE */
static CamelMimeMessage *
get_message (CamelFolder *folder, const gchar *uid, CamelException *ex)
{
	CamelCacheFolder *cache_folder = (CamelCacheFolder *)folder;
	CamelMimeMessage *msg;
	guint32 flags;
	const char *luid;

	/* Check if we have it cached first. */
	luid = camel_cache_map_get_local (cache_folder->uidmap, uid);
	if (luid) {
		msg = camel_folder_get_message (cache_folder->local,
						luid, NULL);
		if (msg)
			return msg;

		/* Hm... Oh well. Update the map and try for real. */
		camel_cache_map_remove (cache_folder->uidmap, NULL, uid);
	}

	/* OK. It's not cached. Get the remote message. */
	msg = camel_folder_get_message (cache_folder->remote, uid, ex);
	if (!msg)
		return NULL;
	flags = camel_folder_get_message_flags (cache_folder->remote, uid);

	/* Add a header giving the remote UID and append it to the
	 * local folder. (This should eventually invoke
	 * local_folder_changed(), which will take care of updating
	 * the uidmaps.)
	 */
	camel_medium_add_header (CAMEL_MEDIUM (msg), "X-Evolution-Remote-UID",
				 uid);
	camel_folder_append_message (cache_folder->local, msg, flags, NULL);

	return msg;
}

/* DONE */
static GPtrArray *
get_uids (CamelFolder *folder)
{
	CamelCacheFolder *cache_folder = (CamelCacheFolder *)folder;

	return cache_folder->uids;
}

/* DONE */
static GPtrArray *
get_summary (CamelFolder *folder)
{
	CamelCacheFolder *cache_folder = (CamelCacheFolder *)folder;

	return cache_folder->summary;
}

/* DONE */
static GPtrArray *
get_subfolder_names (CamelFolder *folder)
{
	CamelCacheFolder *cache_folder = (CamelCacheFolder *)folder;

	return camel_folder_get_subfolder_names (cache_folder->remote);
}

/* DONE */
static void
free_subfolder_names (CamelFolder *folder, GPtrArray *subfolders)
{
	CamelCacheFolder *cache_folder = (CamelCacheFolder *)folder;

	camel_folder_free_subfolder_names (cache_folder->remote, subfolders);
}

/* DONE */
static GPtrArray *
search_by_expression (CamelFolder *folder, const char *expression,
		      CamelException *ex)
{
	CamelCacheFolder *cache_folder = (CamelCacheFolder *)folder;

	/* Search on the remote folder if we're not synced. */
	if (!cache_folder->is_synced &&
	    camel_folder_has_search_capability (cache_folder->remote)) {
		return camel_folder_search_by_expression (cache_folder->remote,
							  expression, ex);
	} else {
		GPtrArray *matches;
		const char *ruid;
		int i;

		if (!cache_folder->is_synced)
			camel_cache_folder_sync (cache_folder, ex);
		if (camel_exception_is_set (ex))
			return NULL;
		matches = search_by_expression (cache_folder->local,
						expression, ex);
		if (camel_exception_is_set (ex))
			return NULL;

		/* Convert local uids to remote. */
		for (i = 0; i < matches->len; i++) {
			ruid = camel_cache_map_get_remote (cache_folder->uidmap,
							   matches->pdata[i]);
			g_free (matches->pdata[i]);
			matches->pdata[i] = g_strdup (ruid);
		}

		return matches;
	}
}

/* DONE */
static const CamelMessageInfo *
get_message_info (CamelFolder *folder, const char *uid)
{
	CamelCacheFolder *cache_folder = (CamelCacheFolder *)folder;

	if (cache_folder->remote_summary) {
		return camel_folder_get_message_info (cache_folder->remote,
						      uid);
	} else
		return g_hash_table_lookup (cache_folder->summary_uids, uid);
}


CamelFolder *
camel_cache_folder_new (CamelStore *store, CamelFolder *parent,
			CamelFolder *remote, CamelFolder *local)
{
	CamelCacheFolder *cache_folder;
	CamelFolder *folder;

	cache_folder = gtk_type_new (CAMEL_CACHE_FOLDER_TYPE);
	folder = (CamelFolder *)cache_folder;

	cache_folder->local = local;
	gtk_object_ref (GTK_OBJECT (local));
	gtk_signal_connect (GTK_OBJECT (local), "folder_changed",
			    GTK_SIGNAL_FUNC (local_folder_changed),
			    cache_folder);

	cache_folder->remote = remote;
	gtk_object_ref (GTK_OBJECT (remote));
	gtk_signal_connect (GTK_OBJECT (remote), "folder_changed",
			    GTK_SIGNAL_FUNC (remote_folder_changed),
			    cache_folder);

	/* XXX */

	return folder;
}

void
camel_cache_folder_sync (CamelCacheFolder *cache_folder, CamelException *ex)
{
	CamelMimeMessage *msg;
	const char *ruid, *luid;
	int lsize, i;
	guint32 flags;

	lsize = camel_folder_get_message_count (cache_folder->local);

	camel_folder_freeze (cache_folder->local);
	for (i = 0; i < cache_folder->uids->len; i++) {
		ruid = cache_folder->uids->pdata[i];
		luid = camel_cache_map_get_local (cache_folder->uidmap, ruid);

		/* Don't re-copy messages we already have. */
		if (luid &&
		    camel_folder_get_message_info (cache_folder->local, luid))
			continue;

		msg = camel_folder_get_message (cache_folder->remote,
						ruid, ex);
		if (camel_exception_is_set (ex))
			return;
		flags = camel_folder_get_message_flags (cache_folder->remote,
							ruid);

		camel_medium_add_header (CAMEL_MEDIUM (msg),
					 "X-Evolution-Remote-UID", ruid);
		camel_folder_append_message (cache_folder->local, msg,
					     flags, ex);
		if (camel_exception_is_set (ex))
			return;
	}
	camel_folder_thaw (cache_folder->local);
}

static void
get_mappings (CamelCacheFolder *cache_folder, int first, CamelException *ex)
{
	GPtrArray *uids;
	CamelMimeMessage *msg;
	const char *ruid;
	int i;

	uids = camel_folder_get_uids (cache_folder->local);
	for (i = first; i < uids->len; i++) {
		msg = camel_folder_get_message (cache_folder->local,
						uids->pdata[i], ex);
		if (!msg)
			break;
		ruid = camel_medium_get_header (CAMEL_MEDIUM (msg),
						"X-Evolution-Remote-UID");

		camel_cache_map_add (cache_folder->uidmap,
				     uids->pdata[i], ruid);
	}
	camel_folder_free_uids (cache_folder->local, uids);
}
