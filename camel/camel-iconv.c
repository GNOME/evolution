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
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include "e-util/e-memory.h"
#include "camel/camel-charset-map.h"
#include "camel-iconv.h"


#define ICONV_CACHE_SIZE   (16)

struct _iconv_cache_bucket {
	struct _iconv_cache_bucket *next;
	struct _iconv_cache_bucket *prev;
	guint32 refcount;
	gboolean used;
	iconv_t cd;
	char *key;
};


/* a useful website on charset alaises:
 * http://www.li18nux.org/subgroups/sa/locnameguide/v1.1draft/CodesetAliasTable-V11.html */

struct {
	char *charset;
	char *iconv_name;
} known_iconv_charsets[] = {
#if 0
	/* charset name, iconv-friendly charset name */
	{ "iso-8859-1",      "iso-8859-1" },
	{ "iso8859-1",       "iso-8859-1" },
	/* the above mostly serves as an example for iso-style charsets,
	   but we have code that will populate the iso-*'s if/when they
	   show up in camel_iconv_charset_name() so I'm
	   not going to bother putting them all in here... */
	{ "windows-cp1251",  "cp1251"     },
	{ "windows-1251",    "cp1251"     },
	{ "cp1251",          "cp1251"     },
	/* the above mostly serves as an example for windows-style
	   charsets, but we have code that will parse and convert them
	   to their cp#### equivalents if/when they show up in
	   camel_iconv_charset_name() so I'm not going to bother
	   putting them all in here either... */
#endif
	/* charset name (lowercase!), iconv-friendly name (sometimes case sensitive) */
	{ "utf-8",           "UTF-8"      },
	{ "utf8",            "UTF-8"      },
	
	/* 10646 is a special case, its usually UCS-2 big endian */
	/* This might need some checking but should be ok for solaris/linux */
	{ "iso-10646-1",     "UCS-2BE"    },
	{ "iso_10646-1",     "UCS-2BE"    },
	{ "iso10646-1",      "UCS-2BE"    },
	{ "iso-10646",       "UCS-2BE"    },
	{ "iso_10646",       "UCS-2BE"    },
	{ "iso10646",        "UCS-2BE"    },
	
	/* "ks_c_5601-1987" seems to be the most common of this lot */
	{ "ks_c_5601-1987",  "EUC-KR"     },
	{ "5601",            "EUC-KR"     },
	{ "ksc-5601",        "EUC-KR"     },
	{ "ksc-5601-1987",   "EUC-KR"     },
	{ "ksc-5601_1987",   "EUC-KR"     },
	
	/* FIXME: Japanese/Korean/Chinese stuff needs checking */
	{ "euckr-0",         "EUC-KR"     },
	{ "5601",            "EUC-KR"     },
	{ "big5-0",          "BIG5"       },
	{ "big5.eten-0",     "BIG5"       },
	{ "big5hkscs-0",     "BIG5HKCS"   },
	{ "gb2312-0",        "gb2312"     },
	{ "gb2312.1980-0",   "gb2312"     },
	{ "euc-cn",          "gb2312"     },
	{ "gb18030-0",       "gb18030"    },
	{ "gbk-0",           "GBK"        },
	
	{ "eucjp-0",         "eucJP"  	  },  /* should this map to "EUC-JP" instead? */
	{ "ujis-0",          "ujis"  	  },  /* we might want to map this to EUC-JP */
	{ "jisx0208.1983-0", "SJIS"       },
	{ "jisx0212.1990-0", "SJIS"       },
	{ "pck",	     "SJIS"       },
	{ NULL,              NULL         }
};


static GHashTable *iconv_charsets;

static EMemChunk *cache_chunk;
static struct _iconv_cache_bucket *iconv_cache_buckets;
static GHashTable *iconv_cache;
static GHashTable *iconv_open_hash;
static unsigned int iconv_cache_size = 0;

#ifdef G_THREADS_ENABLED
static GStaticMutex iconv_cache_lock = G_STATIC_MUTEX_INIT;
static GStaticMutex iconv_charset_lock = G_STATIC_MUTEX_INIT;
#define ICONV_CACHE_LOCK()   g_static_mutex_lock (&iconv_cache_lock)
#define ICONV_CACHE_UNLOCK() g_static_mutex_unlock (&iconv_cache_lock)
#define ICONV_CHARSET_LOCK() g_static_mutex_lock (&iconv_charset_lock)
#define ICONV_CHARSET_UNLOCK() g_static_mutex_unlock (&iconv_charset_lock)
#else
#define ICONV_CACHE_LOCK()
#define ICONV_CACHE_UNLOCK()
#define ICONV_CHARSET_LOCK()
#define ICONV_CHARSET_UNLOCK()
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


static void
iconv_charset_free (char *name, char *iname, gpointer user_data)
{
	g_free (name);
	g_free (iname);
}

void
camel_iconv_shutdown (void)
{
	struct _iconv_cache_bucket *bucket, *next;
	
	g_hash_table_foreach (iconv_charsets, (GHFunc) iconv_charset_free, NULL);
	g_hash_table_destroy (iconv_charsets);
	
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
	char *from, *to;
	int i;
	
	if (initialized)
		return;
	
	iconv_charsets = g_hash_table_new (g_str_hash, g_str_equal);
	
	for (i = 0; known_iconv_charsets[i].charset != NULL; i++) {
		from = g_strdup (known_iconv_charsets[i].charset);
		to = g_strdup (known_iconv_charsets[i].iconv_name);
		g_ascii_strdown (from, -1);
		
		g_hash_table_insert (iconv_charsets, from, to);
	}
	
	iconv_cache_buckets = NULL;
	iconv_cache = g_hash_table_new (g_str_hash, g_str_equal);
	iconv_open_hash = g_hash_table_new (g_direct_hash, g_direct_equal);
	
	cache_chunk = e_memchunk_new (ICONV_CACHE_SIZE, sizeof (struct _iconv_cache_bucket));
	
	initialized = TRUE;
}


/**
 * camel_iconv_charset_name:
 * @charset: charset name
 *
 * Maps charset names to the names that glib's g_iconv_open() is more
 * likely able to handle.
 *
 * Returns an iconv-friendly name for @charset.
 **/
const char *
camel_iconv_charset_name (const char *charset)
{
	char *name, *iname, *tmp;
	
	if (charset == NULL)
		return NULL;
	
	name = g_alloca (strlen (charset) + 1);
	strcpy (name, charset);
	g_ascii_strdown (name, -1);
	
	ICONV_CHARSET_LOCK ();
	if ((iname = g_hash_table_lookup (iconv_charsets, name)) != NULL) {
		ICONV_CHARSET_UNLOCK ();
		return iname;
	}
	
	/* Unknown, try to convert some basic charset types to something that should work */
	if (!strncmp (name, "iso", 3)) {
		/* camel_charset_canonical_name() can handle this case */
		ICONV_CHARSET_UNLOCK ();
		return camel_charset_canonical_name (charset);
	} else if (strncmp (name, "windows-", 8) == 0) {
		/* Convert windows-#### or windows-cp#### to cp#### */
		tmp = name + 8;
		if (!strncmp (tmp, "cp", 2))
			tmp += 2;
		iname = g_strdup_printf ("CP%s", tmp);
	} else if (strncmp (name, "microsoft-", 10) == 0) {
		/* Convert microsoft-#### or microsoft-cp#### to cp#### */
		tmp = name + 10;
		if (!strncmp (tmp, "cp", 2))
			tmp += 2;
		iname = g_strdup_printf ("CP%s", tmp);	
	} else {
		/* Just assume its ok enough as is, case and all - let g_iconv_open() handle this */
		iname = g_strdup (charset);
	}
	
	g_hash_table_insert (iconv_charsets, g_strdup (name), iname);
	ICONV_CHARSET_UNLOCK ();
	
	return iname;
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
	 * format(s) for the to/from charset strings (hahaha, yea
	 * right), we still convert them to their canonical format
	 * here so that our key is in a standard format */
	from = camel_iconv_charset_name (from);
	to = camel_iconv_charset_name (to);
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
	return iconv (cd, inbuf, inleft, outbuf, outleft);
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
