/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* camel-cache-map.c : functions for a local<->remote uid map */

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

#include "camel-cache-map.h"
#include <camel/camel-exception.h>

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

/**
 * camel_cache_map_new:
 *
 * Return value: a new CamelCacheMap
 **/
CamelCacheMap *
camel_cache_map_new (void)
{
	CamelCacheMap *map = g_new (CamelCacheMap, 1);

	map->l2r = g_hash_table_new (g_str_hash, g_str_equal);
	map->r2l = g_hash_table_new (g_str_hash, g_str_equal);

	return map;
}

static void
free_mapping (gpointer key, gpointer value, gpointer data)
{
	g_free (key);
	g_free (data);
}

/**
 * camel_cache_map_destroy:
 * @map: a CamelCacheMap
 *
 * Frees @map and all of the data stored in it.
 **/
void
camel_cache_map_destroy (CamelCacheMap *map)
{
	g_hash_table_foreach (map->l2r, free_mapping, NULL);
	g_hash_table_destroy (map->l2r);
	g_hash_table_destroy (map->r2l);
	g_free (map);
}

/**
 * camel_cache_map_add:
 * @map: a CamelCacheMap
 * @luid: the local uid
 * @ruid: the remote uid
 *
 * Adds a mapping between @luid and @ruid. If either already exists
 * in the map, this may leak memory and result in incorrect map entries.
 * Use camel_cache_map_update() in that case instead.
 **/
void
camel_cache_map_add (CamelCacheMap *map, const char *luid, const char *ruid)
{
	char *map_luid = g_strdup (luid);
	char *map_ruid = g_strdup (ruid);

	g_hash_table_insert (map->l2r, map_luid, map_ruid);
	g_hash_table_insert (map->r2l, map_ruid, map_luid);
}

/**
 * camel_cache_map_remove:
 * @map: a CamelCacheMap
 * @luid: the local uid
 * @ruid: the remote uid
 *
 * Removes the mapping between @luid and @ruid. Either (but not both)
 * of the uids can be %NULL if they are not both known.
 **/
void
camel_cache_map_remove (CamelCacheMap *map, const char *luid, const char *ruid)
{
	gpointer map_luid, map_ruid;

	if ((luid && g_hash_table_lookup_extended (map->l2r, luid,
						   &map_luid, &map_ruid)) ||
	    (ruid && g_hash_table_lookup_extended (map->r2l, ruid,
						  &map_luid, &map_ruid))) {
		g_hash_table_remove (map->l2r, map_luid);
		g_hash_table_remove (map->r2l, map_ruid);
		g_free (map_luid);
		g_free (map_ruid);
	}
}

/**
 * camel_cache_map_update:
 * @map: a CamelCacheMap
 * @luid: the local uid
 * @ruid: the remote uid
 *
 * Updates the mappings to associate @luid with @ruid, clearing any
 * previous mappings for both of them.
 **/
void
camel_cache_map_update (CamelCacheMap *map, const char *luid, const char *ruid)
{
	camel_cache_map_remove (map, luid, ruid);
	camel_cache_map_add (map, luid, ruid);
}

/**
 * camel_cache_map_get_local
 * @map: a CamelCacheMap
 * @ruid: the remote uid
 *
 * Return value: the corresponding local uid, or %NULL
 **/
const char *
camel_cache_map_get_local (CamelCacheMap *map, const char *ruid)
{
	return g_hash_table_lookup (map->r2l, ruid);
}

/**
 * camel_cache_map_get_remote
 * @map: a CamelCacheMap
 * @luid: the local uid
 *
 * Return value: the corresponding remote uid, or %NULL
 **/
const char *
camel_cache_map_get_remote (CamelCacheMap *map, const char *luid)
{
	return g_hash_table_lookup (map->l2r, luid);
}



static void
write_mapping (gpointer key, gpointer value, gpointer user_data)
{
	int fd = *(int *)user_data;

	/* FIXME: We assume the local UID has no ':'s in it. */
	write (fd, key, strlen (key));
	write (fd, ":", 1);
	write (fd, value, strlen (value));
	write (fd, "\n", 1);
}

/**
 * camel_cache_map_write:
 * @map: a CamelCacheMap
 * @file: the filename to write the map to
 * @ex: a CamelException
 *
 * Writes @map out to @file, setting @ex if something goes wrong.
 **/
void
camel_cache_map_write (CamelCacheMap *map, const char *file,
		       CamelException *ex)
{
	int fd;
	char *tmpfile;

	tmpfile = g_strdup_printf ("%s~", file);
	fd = open (tmpfile, O_WRONLY | O_CREAT, S_IRUSR | S_IWUSR);
	if (fd == -1) {
		g_free (tmpfile);
		camel_exception_setv (ex, CAMEL_EXCEPTION_SYSTEM,
				      "Could not create cache map file: %s",
				      g_strerror (errno));
		return;
	}

	g_hash_table_foreach (map->l2r, write_mapping, &fd);

	if (close (fd) == -1 ||
	    rename (tmpfile, file) == -1) {
		camel_exception_setv (ex, CAMEL_EXCEPTION_SYSTEM,
				      "Could not save cache map file: %s",
				      g_strerror (errno));
		unlink (tmpfile);
	}
	g_free (tmpfile);
}

/**
 * camel_cache_map_read:
 * @map: a CamelCacheMap
 * @file: the filename to read the map from
 * @ex: a CamelException
 *
 * Reads @map from @file, setting @ex if something goes wrong. @map
 * should be a freshly-created CamelCacheMap.
 **/
void
camel_cache_map_read (CamelCacheMap *map, const char *file, CamelException *ex)
{
	FILE *f;
	char buf[1024], *p, *q;

	/* FIXME: lazy implementation. We could make this work with
	 * lines longer than 1024 chars. :)
	 */

	f = fopen (file, "r");
	if (!f) {
		camel_exception_setv (ex, CAMEL_EXCEPTION_SYSTEM,
				      "Could not open cache map file: %s",
				      g_strerror (errno));
		return;
	}

	while (fgets (buf, sizeof (buf), f)) {
		p = strchr (buf, ':');
		if (p)
			q = strchr (buf, '\n');
		if (!p || !q) {
			camel_exception_set (ex, CAMEL_EXCEPTION_SYSTEM,
					     "Bad cache file.");
			return;
		}
		*p++ = *q = '\0';

		/* Local uid at buf, remote at p. */
		camel_cache_map_add (map, buf, p);
	}

	fclose (f);
}
