/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* camel-uid-cache.c: UID caching code. */

/* 
 * Authors:
 *  Dan Winship <danw@ximian.com>
 *
 * Copyright 2000 Ximian, Inc. (www.ximian.com)
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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "camel-uid-cache.h"

struct _uid_state {
	int level;
	gboolean save;
};

static void free_uid (gpointer key, gpointer value, gpointer data);
static void maybe_write_uid (gpointer key, gpointer value, gpointer data);

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
	char *buf, **uids;
	int fd, i;
	
	fd = open (filename, O_RDWR | O_CREAT, 0700);
	if (fd == -1)
		return NULL;
	
	if (fstat (fd, &st) != 0) {
		close (fd);
		return NULL;
	}
	buf = g_malloc (st.st_size + 1);
	
	if (read (fd, buf, st.st_size) == -1) {
		close (fd);
		g_free (buf);
		return NULL;
	}
	buf[st.st_size] = '\0';
	
	cache = g_new (CamelUIDCache, 1);
	cache->fd = fd;
	cache->level = 1;
	cache->uids = g_hash_table_new (g_str_hash, g_str_equal);
	
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
	if (lseek (cache->fd, 0, SEEK_SET) != 0)
		return FALSE;
	g_hash_table_foreach (cache->uids, maybe_write_uid, cache);
	return ftruncate (cache->fd, lseek (cache->fd, 0, SEEK_CUR)) == 0;
}

static void
maybe_write_uid (gpointer key, gpointer value, gpointer data)
{
	CamelUIDCache *cache = data;
	struct _uid_state *state = value;
	
	if (state && state->level == cache->level && state->save) {
		write (cache->fd, key, strlen (key));
		write (cache->fd, "\n", 1);
	}
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
	close (cache->fd);
	g_free (cache);
}

static void
free_uid (gpointer key, gpointer value, gpointer data)
{
	g_free (key);
	g_free (value);
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
