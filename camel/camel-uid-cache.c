/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* camel-uid-cache.c: UID caching code. */

/* 
 * Authors:
 *  Dan Winship <danw@ximian.com>
 *
 * Copyright 2000 Ximian, Inc. (www.ximian.com)
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

#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "camel-uid-cache.h"
#include "camel-file-utils.h"

struct _uid_state {
	int level;
	gboolean save;
};


/**
 * camel_uid_cache_new:
 * @filename: path to load the cache from
 *
 * Creates a new UID cache, initialized from @filename. If @filename
 * doesn't already exist, the UID cache will be empty. Otherwise, if
 * it does exist but can't be read, the function will return %NULL.
 *
 * Return value: a new UID cache, or %NULL
 **/
CamelUIDCache *
camel_uid_cache_new (const char *filename)
{
	CamelUIDCache *cache;
	struct stat st;
	char *dirname, *buf, **uids;
	int fd, i;
	
	dirname = g_path_get_dirname (filename);
	if (camel_mkdir (dirname, 0777) == -1) {
		g_free (dirname);
		return NULL;
	}
	
	g_free (dirname);
	
	if ((fd = open (filename, O_RDONLY | O_CREAT, 0666)) == -1)
		return NULL;
	
	if (fstat (fd, &st) == -1) {
		close (fd);
		return NULL;
	}
	
	buf = g_malloc (st.st_size + 1);
	
	if (st.st_size > 0 && camel_read (fd, buf, st.st_size) == -1) {
		close (fd);
		g_free (buf);
		return NULL;
	}
	
	buf[st.st_size] = '\0';
	
	close (fd);
	
	cache = g_new (CamelUIDCache, 1);
	cache->uids = g_hash_table_new (g_str_hash, g_str_equal);
	cache->filename = g_strdup (filename);
	cache->level = 1;
	cache->expired = 0;
	cache->size = 0;
	cache->fd = -1;
	
	uids = g_strsplit (buf, "\n", 0);
	g_free (buf);
	for (i = 0; uids[i]; i++) {
		struct _uid_state *state;
		
		state = g_new (struct _uid_state, 1);
		state->level = cache->level;
		state->save = TRUE;
		
		g_hash_table_insert (cache->uids, uids[i], state);
	}
	
	g_free (uids);
	
	return cache;
}


static void
maybe_write_uid (gpointer key, gpointer value, gpointer data)
{
	CamelUIDCache *cache = data;
	struct _uid_state *state = value;
	
	if (cache->fd == -1)
		return;
	
	if (state && state->level == cache->level && state->save) {
		if (camel_write (cache->fd, key, strlen (key)) == -1 || 
		    camel_write (cache->fd, "\n", 1) == -1) {
			cache->fd = -1;
		} else {
			cache->size += strlen (key) + 1;
		}
	} else {
		/* keep track of how much space the expired uids would
		 * have taken up in the cache */
		cache->expired += strlen (key) + 1;
	}
}


/**
 * camel_uid_cache_save:
 * @cache: a CamelUIDCache
 *
 * Attempts to save @cache back to disk.
 *
 * Return value: success or failure
 **/
gboolean
camel_uid_cache_save (CamelUIDCache *cache)
{
	char *filename;
	int errnosav;
	int fd;
	
	filename = g_strdup_printf ("%s~", cache->filename);
	if ((fd = open (filename, O_WRONLY | O_CREAT | O_TRUNC, 0666)) == -1) {
		g_free (filename);
		return FALSE;
	}
	
	cache->fd = fd;
	cache->size = 0;
	cache->expired = 0;
	g_hash_table_foreach (cache->uids, maybe_write_uid, cache);
	
	if (cache->fd == -1)
		goto exception;
	
	if (fsync (fd) == -1)
		goto exception;
	
	close (fd);
	fd = -1;
	
	if (rename (filename, cache->filename) == -1)
		goto exception;
	
	g_free (filename);
	
	return TRUE;
	
 exception:
	
	errnosav = errno;
	
#ifdef ENABLE_SPASMOLYTIC
	if (fd != -1) {
		/**
		 * If our new cache size is larger than the old cache,
		 * even if we haven't finished writing it out
		 * successfully, we should still attempt to replace
		 * the old cache with the new cache because it will at
		 * least avoid re-downloading a few extra messages
		 * than if we just kept the old cache.
		 *
		 * Similarly, even if the new cache size is smaller
		 * than the old cache size, but we've expired enough
		 * uids to make up for the difference in size (or
		 * more), then we should replace the old cache with
		 * the new cache as well.
		 **/
		
		if (stat (cache->filename, &st) == 0 &&
		    (cache->size > st.st_size || cache->size + cache->expired > st.st_size)) {
			if (ftruncate (fd, (off_t) cache->size) != -1) {
				cache->size = 0;
				cache->expired = 0;
				goto overwrite;
			}
		}
		
		close (fd);
	}
#endif
	
	unlink (filename);
	g_free (filename);
	
	errno = errnosav;
	
	return FALSE;
}


static void
free_uid (gpointer key, gpointer value, gpointer data)
{
	g_free (key);
	g_free (value);
}


/**
 * camel_uid_cache_destroy:
 * @cache: a CamelUIDCache
 *
 * Destroys @cache and frees its data.
 **/
void
camel_uid_cache_destroy (CamelUIDCache *cache)
{
	g_hash_table_foreach (cache->uids, free_uid, NULL);
	g_hash_table_destroy (cache->uids);
	g_free (cache->filename);
	g_free (cache);
}


/**
 * camel_uid_cache_get_new_uids:
 * @cache: a CamelUIDCache
 * @uids: an array of UIDs
 *
 * Returns an array of UIDs from @uids that are not in @cache, and
 * removes UIDs from @cache that aren't in @uids.
 *
 * Return value: an array of new UIDs, which must be freed with
 * camel_uid_cache_free_uids().
 **/
GPtrArray *
camel_uid_cache_get_new_uids (CamelUIDCache *cache, GPtrArray *uids)
{
	GPtrArray *new_uids;
	gpointer old_uid;
	char *uid;
	int i;
	
	new_uids = g_ptr_array_new ();
	cache->level++;

	for (i = 0; i < uids->len; i++) {
		struct _uid_state *state;
		
		uid = uids->pdata[i];
		if (g_hash_table_lookup_extended (cache->uids, uid, (void **)&old_uid, (void **)&state)) {
			g_hash_table_remove (cache->uids, uid);
			g_free (old_uid);
		} else {
			g_ptr_array_add (new_uids, g_strdup (uid));
			state = g_new (struct _uid_state, 1);
			state->save = FALSE;
		}
		
		state->level = cache->level;
		g_hash_table_insert (cache->uids, g_strdup (uid), state);
	}
	
	return new_uids;
}


/**
 * camel_uid_cache_save_uid:
 * @cache: a CamelUIDCache
 * @uid: a uid to save
 *
 * Marks a uid for saving.
 **/
void
camel_uid_cache_save_uid (CamelUIDCache *cache, const char *uid)
{
	struct _uid_state *state;
	gpointer old_uid;
	
	g_return_if_fail (uid != NULL);

	if (g_hash_table_lookup_extended (cache->uids, uid, (void **)&old_uid, (void **)&state)) {
		state->save = TRUE;
		state->level = cache->level;
	} else {
		state = g_new (struct _uid_state, 1);
		state->save = TRUE;
		state->level = cache->level;

		g_hash_table_insert (cache->uids, g_strdup (uid), state);
	}
}


/**
 * camel_uid_cache_free_uids:
 * @uids: an array returned from camel_uid_cache_get_new_uids()
 *
 * Frees the array of UIDs.
 **/
void
camel_uid_cache_free_uids (GPtrArray *uids)
{
	int i;
	
	for (i = 0; i < uids->len; i++)
		g_free (uids->pdata[i]);
	g_ptr_array_free (uids, TRUE);
}
