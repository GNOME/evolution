/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 *  Authors: Jeffrey Stedfast <fejj@ximian.com>
 *
 *  Copyright 2003 Ximian, Inc. (www.ximian.com)
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Street #330, Boston, MA 02111-1307, USA.
 *
 */


#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <glib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>

#include "e-util/e-memory.h"
#include "camel/camel-charset-map.h"


#define ICONV_CACHE_SIZE   (16)

struct _iconv_cache_bucket {
	struct _iconv_cache_bucket *next;
	struct _iconv_cache_bucket *prev;
	guint32 refcount;
	gboolean used;
	iconv_t cd;
	char *key;
};


static EMemChunk *cache_chunk;
static struct _iconv_cache_bucket *iconv_cache_buckets;
static GHashTable *iconv_cache;
static GHashTable *iconv_open_hash;
static unsigned int iconv_cache_size = 0;

#ifdef G_THREADS_ENABLED
static GStaticMutex iconv_cache_lock = G_STATIC_MUTEX_INIT;
#define ICONV_CACHE_LOCK()   g_static_mutex_lock (&iconv_cache_lock)
#define ICONV_CACHE_UNLOCK() g_static_mutex_unlock (&iconv_cache_lock)
#else
#define ICONV_CACHE_LOCK()
#define ICONV_CACHE_UNLOCK()
#endif /* G_THREADS_ENABLED */


/* caller *must* hold the iconv_cache_lock to call any of the following functions */


/**
 * iconv_cache_bucket_new:
 * @key: cache key
 * @cd: iconv descriptor
 *
 * Creates a new cache bucket, inserts it into the cache and
 * increments the cache size.
 *
 * Returns a pointer to the newly allocated cache bucket.
 **/
static struct _iconv_cache_bucket *
iconv_cache_bucket_new (const char *key, iconv_t cd)
{
	struct _iconv_cache_bucket *bucket;
	
	bucket = e_memchunk_alloc (cache_chunk);
	bucket->next = NULL;
	bucket->prev = NULL;
	bucket->key = g_strdup (key);
	bucket->refcount = 1;
	bucket->used = TRUE;
	bucket->cd = cd;
	
	g_hash_table_insert (iconv_cache, bucket->key, bucket);
	
	/* FIXME: Since iconv_cache_expire_unused() traverses the list
	   from head to tail, perhaps it might be better to append new
	   nodes rather than prepending? This way older cache buckets
	   expire first? */
	bucket->next = iconv_cache_buckets;
	iconv_cache_buckets = bucket;
	
	iconv_cache_size++;
	
	return bucket;
}


/**
 * iconv_cache_bucket_expire:
 * @bucket: cache bucket
 *
 * Expires a single cache bucket @bucket. This should only ever be
 * called on a bucket that currently has no used iconv descriptors
 * open.
 **/
static void
iconv_cache_bucket_expire (struct _iconv_cache_bucket *bucket)
{
	g_hash_table_remove (iconv_cache, bucket->key);
	
	if (bucket->prev) {
		bucket->prev->next = bucket->next;
		if (bucket->next)
			bucket->next->prev = bucket->prev;
	} else {
		iconv_cache_buckets = bucket->next;
		if (bucket->next)
			bucket->next->prev = NULL;
	}
	
	g_free (bucket->key);
	iconv_close (bucket->cd);
	e_memchunk_free (cache_chunk, bucket);
	
	iconv_cache_size--;
}


/**
 * iconv_cache_expire_unused:
 *
 * Expires as many unused cache buckets as it needs to in order to get
 * the total number of buckets < ICONV_CACHE_SIZE.
 **/
static void
iconv_cache_expire_unused (void)
{
	struct _iconv_cache_bucket *bucket, *next;
	
	bucket = iconv_cache_buckets;
	while (bucket && iconv_cache_size >= ICONV_CACHE_SIZE) {
		next = bucket->next;
		
		if (bucket->refcount == 0)
			iconv_cache_bucket_expire (bucket);
		
		bucket = next;
	}
}


void
camel_iconv_shutdown (void)
{
	struct _iconv_cache_bucket *bucket, *next;
	
	bucket = iconv_cache_buckets;
	while (bucket) {
		next = bucket->next;
		
		g_free (bucket->key);
		g_iconv_close (bucket->cd);
		e_memchunk_free (cache_chunk, bucket);
		
		bucket = next;
	}
	
	g_hash_table_destroy (iconv_cache);
	g_hash_table_destroy (iconv_open_hash);
	
	e_memchunk_destroy (cache_chunk);
}


/**
 * camel_iconv_init:
 *
 * Initialize Camel's iconv cache. This *MUST* be called before any
 * camel-iconv interfaces will work correctly.
 **/
void
camel_iconv_init (void)
{
	static int initialized = FALSE;
	
	if (initialized)
		return;
	
	iconv_cache_buckets = NULL;
	iconv_cache = g_hash_table_new (g_str_hash, g_str_equal);
	iconv_open_hash = g_hash_table_new (g_direct_hash, g_direct_equal);
	
	cache_chunk = e_memchunk_new (ICONV_CACHE_SIZE, sizeof (struct _iconv_cache_bucket));
	
	initialized = TRUE;
}


/**
 * camel_iconv_open:
 * @to: charset to convert to
 * @from: charset to convert from
 *
 * Allocates a coversion descriptor suitable for converting byte
 * sequences from charset @from to charset @to. The resulting
 * descriptor can be used with iconv (or the camel_iconv wrapper) any
 * number of times until closed using camel_iconv_close.
 *
 * Returns a new conversion descriptor for use with iconv on success
 * or (iconv_t) -1 on fail as well as setting an appropriate errno
 * value.
 **/
iconv_t
camel_iconv_open (const char *to, const char *from)
{
	struct _iconv_cache_bucket *bucket;
	iconv_t cd;
	char *key;
	
	if (from == NULL || to == NULL) {
		errno = EINVAL;
		return (iconv_t) -1;
	}
	
	if (!strcasecmp (from, "x-unknown"))
		from = camel_charset_locale_name ();
	
	/* Even tho g_iconv_open will find the appropriate charset
	 * format(s) for the to/from charset strings, we still convert
	 * them to their canonical format here so that our key is in a
	 * standard format */
	from = camel_charset_canonical_name (from);
	to = camel_charset_canonical_name (to);
	key = g_alloca (strlen (from) + strlen (to) + 2);
	sprintf (key, "%s:%s", from, to);
	
	ICONV_CACHE_LOCK ();
	
	bucket = g_hash_table_lookup (iconv_cache, key);
	if (bucket) {
		if (bucket->used) {
			cd = g_iconv_open (to, from);
			if (cd == (iconv_t) -1)
				goto exception;
		} else {
			/* Apparently iconv on Solaris <= 7 segfaults if you pass in
			 * NULL for anything but inbuf; work around that. (NULL outbuf
			 * or NULL *outbuf is allowed by Unix98.)
			 */
			size_t inleft = 0, outleft = 0;
			char *outbuf = NULL;
			
			cd = bucket->cd;
			bucket->used = TRUE;
			
			/* reset the descriptor */
			g_iconv (cd, NULL, &inleft, &outbuf, &outleft);
		}
		
		bucket->refcount++;
	} else {
		cd = g_iconv_open (to, from);
		if (cd == (iconv_t) -1)
			goto exception;
		
		iconv_cache_expire_unused ();
		
		bucket = iconv_cache_bucket_new (key, cd);
	}
	
	g_hash_table_insert (iconv_open_hash, cd, bucket->key);
	
	ICONV_CACHE_UNLOCK ();
	
	return cd;
	
 exception:
	
	ICONV_CACHE_UNLOCK ();
	
	if (errno == EINVAL)
		g_warning ("Conversion from '%s' to '%s' is not supported", from, to);
	else
		g_warning ("Could not open converter from '%s' to '%s': %s",
			   from, to, g_strerror (errno));
	
	return cd;
}


/**
 * camel_iconv:
 * @cd: conversion descriptor
 * @inbuf: address of input buffer
 * @inleft: input bytes left
 * @outbuf: address of output buffer
 * @outleft: output bytes left
 *
 * Read `man 3 iconv`
 **/
size_t
camel_iconv (iconv_t cd, const char **inbuf, size_t *inleft, char **outbuf, size_t *outleft)
{
	return iconv (cd, (ICONV_CONST char **) inbuf, inleft, outbuf, outleft);
}


/**
 * camel_iconv_close:
 * @cd: iconv conversion descriptor
 *
 * Closes the iconv descriptor @cd.
 *
 * Returns 0 on success or -1 on fail as well as setting an
 * appropriate errno value.
 **/
int
camel_iconv_close (iconv_t cd)
{
	struct _iconv_cache_bucket *bucket;
	const char *key;
	
	if (cd == (iconv_t) -1)
		return 0;
	
	ICONV_CACHE_LOCK ();
	
	key = g_hash_table_lookup (iconv_open_hash, cd);
	if (key) {
		g_hash_table_remove (iconv_open_hash, cd);
		
		bucket = g_hash_table_lookup (iconv_cache, key);
		g_assert (bucket);
		
		bucket->refcount--;
		
		if (cd == bucket->cd)
			bucket->used = FALSE;
		else
			g_iconv_close (cd);
		
		if (!bucket->refcount && iconv_cache_size > ICONV_CACHE_SIZE) {
			/* expire this cache bucket */
			iconv_cache_bucket_expire (bucket);
		}
	} else {
		ICONV_CACHE_UNLOCK ();
		
		g_warning ("This iconv context wasn't opened using camel_iconv_open()");
		
		return g_iconv_close (cd);
	}
	
	ICONV_CACHE_UNLOCK ();
	
	return 0;
}
