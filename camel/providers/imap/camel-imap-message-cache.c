/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* camel-imap-message-cache.c: Class for an IMAP message cache */

/* 
 * Author: 
 *   Dan Winship <danw@ximian.com>
 *
 * Copyright (C) 2001 Ximian, Inc. (www.ximian.com)
 *
 * This program is free software; you can redistribute it and/or 
 * modify it under the terms of version 2 of the GNU General Public 
 * License as published by the Free Software Foundation.
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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <sys/types.h>
#include <dirent.h>
#include <errno.h>
#include <string.h>
#include <ctype.h>

#include "camel-imap-message-cache.h"
#include "camel-data-wrapper.h"
#include "camel-exception.h"
#include "camel-stream-fs.h"
#include "camel-i18n.h"

static void finalize (CamelImapMessageCache *cache);
static void stream_finalize (CamelObject *stream, gpointer event_data, gpointer user_data);


CamelType
camel_imap_message_cache_get_type (void)
{
	static CamelType camel_imap_message_cache_type = CAMEL_INVALID_TYPE;
	
	if (camel_imap_message_cache_type == CAMEL_INVALID_TYPE) {
		camel_imap_message_cache_type = camel_type_register (
			CAMEL_OBJECT_TYPE, "CamelImapMessageCache",
			sizeof (CamelImapMessageCache),
			sizeof (CamelImapMessageCacheClass),
			NULL,
			NULL,
			NULL,
			(CamelObjectFinalizeFunc) finalize);
	}

	return camel_imap_message_cache_type;
}

static void
free_part (gpointer key, gpointer value, gpointer data)
{
	if (value) {
		if (strchr (key, '.')) {
			camel_object_unhook_event (value, "finalize",
						   stream_finalize, data);
			camel_object_unref (value);
		} else
			g_ptr_array_free (value, TRUE);
	}
	g_free (key);
}

static void
finalize (CamelImapMessageCache *cache)
{
	if (cache->path)
		g_free (cache->path);
	if (cache->parts) {
		g_hash_table_foreach (cache->parts, free_part, cache);
		g_hash_table_destroy (cache->parts);
	}
	if (cache->cached)
		g_hash_table_destroy (cache->cached);
}

static void
cache_put (CamelImapMessageCache *cache, const char *uid, const char *key,
	   CamelStream *stream)
{
	char *hash_key;
	GPtrArray *subparts;
	gpointer okey, ostream;
	guint32 uidval;

	uidval = strtoul (uid, NULL, 10);
	if (uidval > cache->max_uid)
		cache->max_uid = uidval;

	subparts = g_hash_table_lookup (cache->parts, uid);
	if (!subparts) {
		subparts = g_ptr_array_new ();
		g_hash_table_insert (cache->parts, g_strdup (uid), subparts);
	}

	if (g_hash_table_lookup_extended (cache->parts, key, &okey, &ostream)) {
		if (ostream) {
			camel_object_unhook_event (ostream, "finalize",
						   stream_finalize, cache);
			g_hash_table_remove (cache->cached, ostream);
			camel_object_unref (ostream);
		}
		hash_key = okey;
	} else {
		hash_key = g_strdup (key);
		g_ptr_array_add (subparts, hash_key);
	}

	g_hash_table_insert (cache->parts, hash_key, stream);
	g_hash_table_insert (cache->cached, stream, hash_key);

	if (stream) {
		camel_object_hook_event (CAMEL_OBJECT (stream), "finalize",
					 stream_finalize, cache);
	}
}

/**
 * camel_imap_message_cache_new:
 * @path: directory to use for storage
 * @summary: CamelFolderSummary for the folder we are caching
 * @ex: a CamelException
 *
 * Return value: a new CamelImapMessageCache object using @path for
 * storage. If cache files already exist in @path, then any that do not
 * correspond to messages in @summary will be deleted.
 **/
CamelImapMessageCache *
camel_imap_message_cache_new (const char *path, CamelFolderSummary *summary,
			      CamelException *ex)
{
	CamelImapMessageCache *cache;
	DIR *dir;
	struct dirent *d;
	char *uid, *p;
	GPtrArray *deletes;
	CamelMessageInfo *info;

	dir = opendir (path);
	if (!dir) {
		camel_exception_setv (ex, CAMEL_EXCEPTION_SYSTEM,
				      _("Could not open cache directory: %s"),
				      g_strerror (errno));
		return NULL;
	}

	cache = (CamelImapMessageCache *)camel_object_new (CAMEL_IMAP_MESSAGE_CACHE_TYPE);
	cache->path = g_strdup (path);

	cache->parts = g_hash_table_new (g_str_hash, g_str_equal);
	cache->cached = g_hash_table_new (NULL, NULL);
	deletes = g_ptr_array_new ();
	while ((d = readdir (dir))) {
		if (!isdigit (d->d_name[0]))
			continue;

		p = strchr (d->d_name, '.');
		if (p)
			uid = g_strndup (d->d_name, p - d->d_name);
		else
			uid = g_strdup (d->d_name);

		info = camel_folder_summary_uid (summary, uid);
		if (info) {
			camel_message_info_free(info);
			cache_put (cache, uid, d->d_name, NULL);
		} else
			g_ptr_array_add (deletes, g_strdup_printf ("%s/%s", cache->path, d->d_name));
		g_free (uid);
	}
	closedir (dir);

	while (deletes->len) {
		unlink (deletes->pdata[0]);
		g_free (deletes->pdata[0]);
		g_ptr_array_remove_index_fast (deletes, 0);
	}
	g_ptr_array_free (deletes, TRUE);

	return cache;
}

/**
 * camel_imap_message_cache_max_uid:
 * @cache: the cache
 *
 * Return value: the largest (real) UID in the cache.
 **/
guint32
camel_imap_message_cache_max_uid (CamelImapMessageCache *cache)
{
	return cache->max_uid;
}

/**
 * camel_imap_message_cache_set_path:
 * @cache: 
 * @path: 
 * 
 * Set the path used for the message cache.
 **/
void
camel_imap_message_cache_set_path (CamelImapMessageCache *cache, const char *path)
{
	g_free(cache->path);
	cache->path = g_strdup(path);
}

static void
stream_finalize (CamelObject *stream, gpointer event_data, gpointer user_data)
{
	CamelImapMessageCache *cache = user_data;
	char *key;

	key = g_hash_table_lookup (cache->cached, stream);
	if (!key)
		return;
	g_hash_table_remove (cache->cached, stream);
	g_hash_table_insert (cache->parts, key, NULL);
}


static CamelStream *
insert_setup (CamelImapMessageCache *cache, const char *uid, const char *part_spec,
	      char **path, char **key, CamelException *ex)
{
	CamelStream *stream;
	int fd;
	
	*path = g_strdup_printf ("%s/%s.%s", cache->path, uid, part_spec);
	*key = strrchr (*path, '/') + 1;
	stream = g_hash_table_lookup (cache->parts, *key);
	if (stream)
		camel_object_unref (CAMEL_OBJECT (stream));
	
	fd = open (*path, O_RDWR | O_CREAT | O_TRUNC, 0600);
	if (fd == -1) {
		camel_exception_setv (ex, CAMEL_EXCEPTION_SYSTEM,
				      _("Failed to cache message %s: %s"),
				      uid, g_strerror (errno));
		g_free (*path);
		return NULL;
	}
	
	return camel_stream_fs_new_with_fd (fd);
}

static CamelStream *
insert_abort (char *path, CamelStream *stream)
{
	unlink (path);
	g_free (path);
	camel_object_unref (CAMEL_OBJECT (stream));
	return NULL;
}

static CamelStream *
insert_finish (CamelImapMessageCache *cache, const char *uid, char *path,
	       char *key, CamelStream *stream)
{
	camel_stream_flush (stream);
	camel_stream_reset (stream);
	cache_put (cache, uid, key, stream);
	g_free (path);

	return stream;
}

/**
 * camel_imap_message_cache_insert:
 * @cache: the cache
 * @uid: UID of the message data to cache
 * @part_spec: the IMAP part_spec of the data
 * @data: the data
 * @len: length of @data
 *
 * Caches the provided data into @cache.
 *
 * Return value: a CamelStream containing the cached data, which the
 * caller must unref.
 **/
CamelStream *
camel_imap_message_cache_insert (CamelImapMessageCache *cache, const char *uid,
				 const char *part_spec, const char *data,
				 int len, CamelException *ex)
{
	char *path, *key;
	CamelStream *stream;
	
	stream = insert_setup (cache, uid, part_spec, &path, &key, ex);
	if (!stream)
		return NULL;
	
	if (camel_stream_write (stream, data, len) == -1) {
		camel_exception_setv (ex, CAMEL_EXCEPTION_SYSTEM,
				      _("Failed to cache message %s: %s"),
				      uid, g_strerror (errno));
		return insert_abort (path, stream);
	}
	
	return insert_finish (cache, uid, path, key, stream);
}

/**
 * camel_imap_message_cache_insert_stream:
 * @cache: the cache
 * @uid: UID of the message data to cache
 * @part_spec: the IMAP part_spec of the data
 * @data_stream: the stream to cache
 *
 * Caches the provided data into @cache.
 **/
void
camel_imap_message_cache_insert_stream (CamelImapMessageCache *cache,
					const char *uid, const char *part_spec,
					CamelStream *data_stream, CamelException *ex)
{
	char *path, *key;
	CamelStream *stream;
	
	stream = insert_setup (cache, uid, part_spec, &path, &key, ex);
	if (!stream)
		return;
	
	if (camel_stream_write_to_stream (data_stream, stream) == -1) {
		camel_exception_setv (ex, CAMEL_EXCEPTION_SYSTEM,
				      _("Failed to cache message %s: %s"),
				      uid, g_strerror (errno));
		insert_abort (path, stream);
	} else {
		insert_finish (cache, uid, path, key, stream);
		camel_object_unref (CAMEL_OBJECT (stream));
	}
}

/**
 * camel_imap_message_cache_insert_wrapper:
 * @cache: the cache
 * @uid: UID of the message data to cache
 * @part_spec: the IMAP part_spec of the data
 * @wrapper: the wrapper to cache
 *
 * Caches the provided data into @cache.
 **/
void
camel_imap_message_cache_insert_wrapper (CamelImapMessageCache *cache,
					 const char *uid, const char *part_spec,
					 CamelDataWrapper *wrapper, CamelException *ex)
{
	char *path, *key;
	CamelStream *stream;

	stream = insert_setup (cache, uid, part_spec, &path, &key, ex);
	if (!stream)
		return;
	
	if (camel_data_wrapper_write_to_stream (wrapper, stream) == -1) {
		camel_exception_setv (ex, CAMEL_EXCEPTION_SYSTEM,
				      _("Failed to cache message %s: %s"),
				      uid, g_strerror (errno));
		insert_abort (path, stream);
	} else {
		insert_finish (cache, uid, path, key, stream);
		camel_object_unref (CAMEL_OBJECT (stream));
	}
}


/**
 * camel_imap_message_cache_get:
 * @cache: the cache
 * @uid: the UID of the data to get
 * @part_spec: the part_spec of the data to get
 * @ex: exception
 *
 * Return value: a CamelStream containing the cached data (which the
 * caller must unref), or %NULL if that data is not cached.
 **/
CamelStream *
camel_imap_message_cache_get (CamelImapMessageCache *cache, const char *uid,
			      const char *part_spec, CamelException *ex)
{
	CamelStream *stream;
	char *path, *key;
	
	if (uid[0] == 0)
		return NULL;
	
	path = g_strdup_printf ("%s/%s.%s", cache->path, uid, part_spec);
	key = strrchr (path, '/') + 1;
	stream = g_hash_table_lookup (cache->parts, key);
	if (stream) {
		camel_stream_reset (CAMEL_STREAM (stream));
		camel_object_ref (CAMEL_OBJECT (stream));
		g_free (path);
		return stream;
	}
	
	stream = camel_stream_fs_new_with_name (path, O_RDONLY, 0);
	if (stream) {
		cache_put (cache, uid, key, stream);
	} else {
		camel_exception_setv (ex, CAMEL_EXCEPTION_SYSTEM,
				      _("Failed to cache %s: %s"),
				      part_spec, g_strerror (errno));
	}
	
	g_free (path);
	
	return stream;
}

/**
 * camel_imap_message_cache_remove:
 * @cache: the cache
 * @uid: UID of the data to remove
 *
 * Removes all data associated with @uid from @cache.
 **/
void
camel_imap_message_cache_remove (CamelImapMessageCache *cache, const char *uid)
{
	GPtrArray *subparts;
	char *key, *path;
	CamelObject *stream;
	int i;

	subparts = g_hash_table_lookup (cache->parts, uid);
	if (!subparts)
		return;
	for (i = 0; i < subparts->len; i++) {
		key = subparts->pdata[i];
		path = g_strdup_printf ("%s/%s", cache->path, key);
		unlink (path);
		g_free (path);
		stream = g_hash_table_lookup (cache->parts, key);
		if (stream) {
			camel_object_unhook_event (stream, "finalize",
						   stream_finalize, cache);
			camel_object_unref (stream);
			g_hash_table_remove (cache->cached, stream);
		}
		g_hash_table_remove (cache->parts, key);
		g_free (key);
	}
	g_hash_table_remove (cache->parts, uid);
	g_ptr_array_free (subparts, TRUE);
}

static void
add_uids (gpointer key, gpointer value, gpointer data)
{
	if (!strchr (key, '.'))
		g_ptr_array_add (data, key);
}

/**
 * camel_imap_message_cache_clear:
 * @cache: the cache
 *
 * Removes all cached data from @cache.
 **/
void
camel_imap_message_cache_clear (CamelImapMessageCache *cache)
{
	GPtrArray *uids;
	int i;

	uids = g_ptr_array_new ();
	g_hash_table_foreach (cache->parts, add_uids, uids);

	for (i = 0; i < uids->len; i++)
		camel_imap_message_cache_remove (cache, uids->pdata[i]);
	g_ptr_array_free (uids, TRUE);
}


/**
 * camel_imap_message_cache_copy:
 * @source: the source message cache
 * @source_uid: UID of a message in @source
 * @dest: the destination message cache
 * @dest_uid: UID of the message in @dest
 *
 * Copies all cached parts from @source_uid in @source to @dest_uid in
 * @destination.
 **/
void
camel_imap_message_cache_copy (CamelImapMessageCache *source,
			       const char *source_uid,
			       CamelImapMessageCache *dest,
			       const char *dest_uid,
			       CamelException *ex)
{
	GPtrArray *subparts;
	CamelStream *stream;
	char *part;
	int i;
	
	subparts = g_hash_table_lookup (source->parts, source_uid);
	if (!subparts || !subparts->len)
		return;
	
	for (i = 0; i < subparts->len; i++) {
		part = strchr (subparts->pdata[i], '.');
		if (!part++)
			continue;
		
		if ((stream = camel_imap_message_cache_get (source, source_uid, part, ex))) {
			camel_imap_message_cache_insert_stream (dest, dest_uid, part, stream, ex);
			camel_object_unref (CAMEL_OBJECT (stream));
		}
	}
}
