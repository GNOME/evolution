/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* camel-message-cache.c: Class for a Camel cache.
 *
 * Authors: Michael Zucchi <notzed@ximian.com>
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
#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#ifdef HAVE_ALLOCA_H
#include <alloca.h>
#endif

#include "camel-i18n.h"
#include "camel-data-cache.h"
#include "camel-exception.h"
#include "camel-stream-fs.h"
#include "camel-stream-mem.h"
#include "camel-file-utils.h"

extern int camel_verbose_debug;
#define dd(x) (camel_verbose_debug?(x):0)
#define d(x)

/* how many 'bits' of hash are used to key the toplevel directory */
#define CAMEL_DATA_CACHE_BITS (6)
#define CAMEL_DATA_CACHE_MASK ((1<<CAMEL_DATA_CACHE_BITS)-1)

/* timeout before a cache dir is checked again for expired entries,
   once an hour should be enough */
#define CAMEL_DATA_CACHE_CYCLE_TIME (60*60)

struct _CamelDataCachePrivate {
	CamelObjectBag *busy_bag;

	int expire_inc;
	time_t expire_last[1<<CAMEL_DATA_CACHE_BITS];
};

static CamelObject *camel_data_cache_parent;

static void data_cache_class_init(CamelDataCacheClass *klass)
{
	camel_data_cache_parent = (CamelObject *)camel_object_get_type ();

#if 0
	klass->add = data_cache_add;
	klass->get = data_cache_get;
	klass->close = data_cache_close;
	klass->remove = data_cache_remove;
	klass->clear = data_cache_clear;
#endif
}

static void data_cache_init(CamelDataCache *cdc, CamelDataCacheClass *klass)
{
	struct _CamelDataCachePrivate *p;

	p = cdc->priv = g_malloc0(sizeof(*cdc->priv));
	p->busy_bag = camel_object_bag_new(g_str_hash, g_str_equal, (CamelCopyFunc)g_strdup, g_free);
}

static void data_cache_finalise(CamelDataCache *cdc)
{
	struct _CamelDataCachePrivate *p;

	p = cdc->priv;
	camel_object_bag_destroy(p->busy_bag);
	g_free(p);
	
	g_free (cdc->path);
}

CamelType
camel_data_cache_get_type(void)
{
	static CamelType camel_data_cache_type = CAMEL_INVALID_TYPE;
	
	if (camel_data_cache_type == CAMEL_INVALID_TYPE) {
		camel_data_cache_type = camel_type_register(
			CAMEL_OBJECT_TYPE, "CamelDataCache",
			sizeof (CamelDataCache),
			sizeof (CamelDataCacheClass),
			(CamelObjectClassInitFunc) data_cache_class_init,
			NULL,
			(CamelObjectInitFunc) data_cache_init,
			(CamelObjectFinalizeFunc) data_cache_finalise);
	}

	return camel_data_cache_type;
}

/**
 * camel_data_cache_new:
 * @path: Base path of cache, subdirectories will be created here.
 * @flags: Open flags, none defined.
 * @ex: 
 * 
 * Create a new data cache.
 * 
 * Return value: A new cache object, or NULL if the base path cannot
 * be written to.
 **/
CamelDataCache *
camel_data_cache_new(const char *path, guint32 flags, CamelException *ex)
{
	CamelDataCache *cdc;

	if (camel_mkdir (path, 0700) == -1) {
		camel_exception_setv(ex, CAMEL_EXCEPTION_SYSTEM,
				     _("Unable to create cache path"));
		return NULL;
	}

	cdc = (CamelDataCache *)camel_object_new(CAMEL_DATA_CACHE_TYPE);

	cdc->path = g_strdup(path);
	cdc->flags = flags;
	cdc->expire_age = -1;
	cdc->expire_access = -1;

	return cdc;
}

/**
 * camel_data_cache_set_expire_age:
 * @cdc: 
 * @when: Timeout for age expiry, or -1 to disable.
 * 
 * Set the cache expiration policy for aged entries.
 * 
 * Items in the cache older than @when seconds may be
 * flushed at any time.  Items are expired in a lazy
 * manner, so it is indeterminate when the items will
 * physically be removed.
 *
 * Note you can set both an age and an access limit.  The
 * age acts as a hard limit on cache entries.
 **/
void
camel_data_cache_set_expire_age(CamelDataCache *cdc, time_t when)
{
	cdc->expire_age = when;
}

/**
 * camel_data_cache_set_expire_access:
 * @cdc: 
 * @when: Timeout for access, or -1 to disable access expiry.
 * 
 * Set the cache expiration policy for access times.
 *
 * Items in the cache which haven't been accessed for @when
 * seconds may be expired at any time.  Items are expired in a lazy
 * manner, so it is indeterminate when the items will
 * physically be removed.
 *
 * Note you can set both an age and an access limit.  The
 * age acts as a hard limit on cache entries.
 **/
void
camel_data_cache_set_expire_access(CamelDataCache *cdc, time_t when)
{
	cdc->expire_access = when;
}

static void
data_cache_expire(CamelDataCache *cdc, const char *path, const char *keep, time_t now)
{
	DIR *dir;
	struct dirent *d;
	GString *s;
	struct stat st;
	CamelStream *stream;

	dir = opendir(path);
	if (dir == NULL)
		return;

	s = g_string_new("");
	while ( (d = readdir(dir)) ) {
		if (strcmp(d->d_name, keep) == 0)
			continue;
		
		g_string_printf (s, "%s/%s", path, d->d_name);
		dd(printf("Checking '%s' for expiry\n", s->str));
		if (stat(s->str, &st) == 0
		    && S_ISREG(st.st_mode)
		    && ((cdc->expire_age != -1 && st.st_mtime + cdc->expire_age < now)
			|| (cdc->expire_access != -1 && st.st_atime + cdc->expire_access < now))) {
			dd(printf("Has expired!  Removing!\n"));
			unlink(s->str);
			stream = camel_object_bag_get(cdc->priv->busy_bag, s->str);
			if (stream) {
				camel_object_bag_remove(cdc->priv->busy_bag, stream);
				camel_object_unref(stream);
			}
		}
	}
	g_string_free(s, TRUE);
	closedir(dir);
}

/* Since we have to stat the directory anyway, we use this opportunity to
   lazily expire old data.
   If it is this directories 'turn', and we haven't done it for CYCLE_TIME seconds,
   then we perform an expiry run */
static char *
data_cache_path(CamelDataCache *cdc, int create, const char *path, const char *key)
{
	char *dir, *real, *tmp;
	guint32 hash;

	hash = g_str_hash(key);
	hash = (hash>>5)&CAMEL_DATA_CACHE_MASK;
	dir = alloca(strlen(cdc->path) + strlen(path) + 8);
	sprintf(dir, "%s/%s/%02x", cdc->path, path, hash);
	if (access(dir, F_OK) == -1) {
		if (create)
			camel_mkdir (dir, 0700);
	} else if (cdc->priv->expire_inc == hash
		   && (cdc->expire_age != -1 || cdc->expire_access != -1)) {
		time_t now;

		dd(printf("Checking expire cycle time on dir '%s'\n", dir));

		/* This has a race, but at worst we re-run an expire cycle which is safe */
		now = time(0);
		if (cdc->priv->expire_last[hash] + CAMEL_DATA_CACHE_CYCLE_TIME < now) {
			cdc->priv->expire_last[hash] = now;
			data_cache_expire(cdc, dir, key, now);
		}
		cdc->priv->expire_inc = (cdc->priv->expire_inc + 1) & CAMEL_DATA_CACHE_MASK;
	}

	tmp = camel_file_util_safe_filename(key);
	real = g_strdup_printf("%s/%s", dir, tmp);
	g_free(tmp);

	return real;
}

/**
 * camel_data_cache_add:
 * @cdc: 
 * @path: Relative path of item to add.
 * @key: Key of item to add.
 * @ex: 
 * 
 * Add a new item to the cache.
 *
 * The key and the path combine to form a unique key used to store
 * the item.
 * 
 * Potentially, expiry processing will be performed while this call
 * is executing.
 *
 * Return value: A CamelStream (file) opened in read-write mode.
 * The caller must unref this when finished.
 **/
CamelStream *
camel_data_cache_add(CamelDataCache *cdc, const char *path, const char *key, CamelException *ex)
{
	char *real;
	CamelStream *stream;

	real = data_cache_path(cdc, TRUE, path, key);
	/* need to loop 'cause otherwise we can call bag_add/bag_abort
	 * after bag_reserve returned a pointer, which is an invalid
	 * sequence. */
	do {
		stream = camel_object_bag_reserve(cdc->priv->busy_bag, real);
		if (stream) {
			unlink(real);
			camel_object_bag_remove(cdc->priv->busy_bag, stream);
			camel_object_unref(stream);
		}
	} while (stream != NULL);

	stream = camel_stream_fs_new_with_name(real, O_RDWR|O_CREAT|O_TRUNC, 0600);
	if (stream)
		camel_object_bag_add(cdc->priv->busy_bag, real, stream);
	else
		camel_object_bag_abort(cdc->priv->busy_bag, real);

	g_free(real);

	return stream;
}

/**
 * camel_data_cache_get:
 * @cdc: 
 * @path: Path to the (sub) cache the item exists in.
 * @key: Key for the cache item.
 * @ex: 
 * 
 * Lookup an item in the cache.  If the item exists, a stream
 * is returned for the item.  The stream may be shared by
 * multiple callers, so ensure the stream is in a valid state
 * through external locking.
 * 
 * Return value: A cache item, or NULL if the cache item does not exist.
 **/
CamelStream *
camel_data_cache_get(CamelDataCache *cdc, const char *path, const char *key, CamelException *ex)
{
	char *real;
	CamelStream *stream;

	real = data_cache_path(cdc, FALSE, path, key);
	stream = camel_object_bag_reserve(cdc->priv->busy_bag, real);
	if (!stream) {
		stream = camel_stream_fs_new_with_name(real, O_RDWR, 0600);
		if (stream)
			camel_object_bag_add(cdc->priv->busy_bag, real, stream);
		else
			camel_object_bag_abort(cdc->priv->busy_bag, real);
	}
	g_free(real);

	return stream;
}

/**
 * camel_data_cache_remove:
 * @cdc: 
 * @path: 
 * @key: 
 * @ex: 
 * 
 * Remove/expire a cache item.
 * 
 * Return value: 
 **/
int
camel_data_cache_remove(CamelDataCache *cdc, const char *path, const char *key, CamelException *ex)
{
	CamelStream *stream;
	char *real;
	int ret;

	real = data_cache_path(cdc, FALSE, path, key);
	stream = camel_object_bag_get(cdc->priv->busy_bag, real);
	if (stream) {
		camel_object_bag_remove(cdc->priv->busy_bag, stream);
		camel_object_unref(stream);
	}

	/* maybe we were a mem stream */
	if (unlink (real) == -1 && errno != ENOENT) {
		camel_exception_setv (ex, CAMEL_EXCEPTION_SYSTEM,
				      _("Could not remove cache entry: %s: %s"),
				      real, g_strerror (errno));
		ret = -1;
	} else {
		ret = 0;
	}

	g_free(real);

	return ret;
}

/**
 * camel_data_cache_rename:
 * @cache: 
 * @old: 
 * @new: 
 * @ex: 
 * 
 * Rename a cache path.  All cache items accessed from the old path
 * are accessible using the new path.
 *
 * CURRENTLY UNIMPLEMENTED
 * 
 * Return value: -1 on error.
 **/
int camel_data_cache_rename(CamelDataCache *cache,
			    const char *old, const char *new, CamelException *ex)
{
	/* blah dont care yet */
	return -1;
}

/**
 * camel_data_cache_clear:
 * @cache: 
 * @path: Path to clear, or NULL to clear all items in
 * all paths.
 * @ex: 
 * 
 * Clear all items in a given cache path or all items in the cache.
 * 
 * CURRENTLY_UNIMPLEMENTED
 *
 * Return value: -1 on error.
 **/
int
camel_data_cache_clear(CamelDataCache *cache, const char *path, CamelException *ex)
{
	/* nor for this? */
	return -1;
}
