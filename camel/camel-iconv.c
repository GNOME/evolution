/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 *  Authors: Jeffrey Stedfast <fejj@ximian.com>
 *           Michael Zucchi <notzed@ximian.com>
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

#include <locale.h>

#ifdef HAVE_CODESET
#include <langinfo.h>
#endif

#include "libedataserver/e-memory.h"
#include "camel-charset-map.h"
#include "string-utils.h"
#include "camel-iconv.h"

#define d(x)

#ifdef G_THREADS_ENABLED
static GStaticMutex lock = G_STATIC_MUTEX_INIT;
#define LOCK() g_static_mutex_lock (&lock)
#define UNLOCK() g_static_mutex_unlock (&lock)
#else
#define LOCK()
#define UNLOCK()
#endif


typedef struct _EDListNode {
	struct _EDListNode *next;
	struct _EDListNode *prev;
} EDListNode;

typedef struct _EDList {
	struct _EDListNode *head;
	struct _EDListNode *tail;
	struct _EDListNode *tailpred;
} EDList;

#define E_DLIST_INITIALISER(l) { (EDListNode *)&l.tail, 0, (EDListNode *)&l.head }

struct _iconv_cache_node {
	struct _iconv_cache_node *next;
	struct _iconv_cache_node *prev;
	
	struct _iconv_cache *parent;
	
	int busy;
	iconv_t cd;
};

struct _iconv_cache {
	struct _iconv_cache *next;
	struct _iconv_cache *prev;
	
	char *conv;
	
	EDList open;		/* stores iconv_cache_nodes, busy ones up front */
};

#define CAMEL_ICONV_CACHE_SIZE (16)

static EDList iconv_cache_list;
static GHashTable *iconv_cache;
static GHashTable *iconv_cache_open;
static unsigned int iconv_cache_size = 0;

static GHashTable *iconv_charsets = NULL;
static char *locale_charset = NULL;
static char *locale_lang = NULL;

struct {
	char *charset;
	char *iconv_name;
} known_iconv_charsets[] = {
#if 0
	/* charset name, iconv-friendly charset name */
	{ "iso-8859-1",     "iso-8859-1" },
	{ "iso8859-1",      "iso-8859-1" },
	/* the above mostly serves as an example for iso-style charsets,
	   but we have code that will populate the iso-*'s if/when they
	   show up in e_iconv_charset_name() so I'm
	   not going to bother putting them all in here... */
	{ "windows-cp1251", "cp1251"     },
	{ "windows-1251",   "cp1251"     },
	{ "cp1251",         "cp1251"     },
	/* the above mostly serves as an example for windows-style
	   charsets, but we have code that will parse and convert them
	   to their cp#### equivalents if/when they show up in
	   e_iconv_charset_name() so I'm not going to bother
	   putting them all in here either... */
#endif
	/* charset name (lowercase!), iconv-friendly name (sometimes case sensitive) */
	{ "utf-8",          "UTF-8"      },
	
	/* 10646 is a special case, its usually UCS-2 big endian */
	/* This might need some checking but should be ok for solaris/linux */
	{ "iso-10646-1",    "UCS-2BE"    },
	{ "iso_10646-1",    "UCS-2BE"    },
	{ "iso10646-1",     "UCS-2BE"    },
	{ "iso-10646",      "UCS-2BE"    },
	{ "iso_10646",      "UCS-2BE"    },
	{ "iso10646",       "UCS-2BE"    },
	
	{ "ks_c_5601-1987", "EUC-KR"     },
	
	/* FIXME: Japanese/Korean/Chinese stuff needs checking */
	{ "euckr-0",        "EUC-KR"     },
	{ "5601",           "EUC-KR"     },
	{ "zh_TW-euc",      "EUC-TW"     },
	{ "zh_CN.euc",      "gb2312"     },
	{ "zh_TW-big5",     "BIG5"       },
	{ "big5-0",         "BIG5"       },
	{ "big5.eten-0",    "BIG5"       },
	{ "big5hkscs-0",    "BIG5HKSCS"  },
	{ "gb2312-0",       "gb2312"     },
	{ "gb2312.1980-0",  "gb2312"     },
	{ "gb-2312",        "gb2312"     },
	{ "gb18030-0",      "gb18030"    },
	{ "gbk-0",          "GBK"        },
	
	{ "eucjp-0",        "eucJP"  	 },
	{ "ujis-0",         "ujis"  	 },
	{ "jisx0208.1983-0","SJIS"       },
	{ "jisx0212.1990-0","SJIS"       },
	{ "pck",	    "SJIS"       },
	{ NULL,             NULL         }
};



/* Another copy of this trivial list implementation
   Why?  This stuff gets called a lot (potentially), should run fast,
   and g_list's are f@@#$ed up to make this a hassle */
static void
e_dlist_init (EDList *v)
{
        v->head = (EDListNode *) &v->tail;
        v->tail = 0;
        v->tailpred = (EDListNode *) &v->head;
}

static EDListNode *
e_dlist_addhead (EDList *l, EDListNode *n)
{
        n->next = l->head;
        n->prev = (EDListNode *) &l->head;
        l->head->prev = n;
        l->head = n;
        return n;
}

static EDListNode *
e_dlist_addtail (EDList *l, EDListNode *n)
{
        n->next = (EDListNode *) &l->tail;
        n->prev = l->tailpred;
        l->tailpred->next = n;
        l->tailpred = n;
        return n;
}

static EDListNode *
e_dlist_remove (EDListNode *n)
{
        n->next->prev = n->prev;
        n->prev->next = n->next;
        return n;
}


static void
locale_parse_lang (const char *locale)
{
	char *codeset, *lang;
	
	if ((codeset = strchr (locale, '.')))
		lang = g_strndup (locale, codeset - locale);
	else
		lang = g_strdup (locale);
	
	/* validate the language */
	if (strlen (lang) >= 2) {
		if (lang[2] == '-' || lang[2] == '_') {
			/* canonicalise the lang */
			camel_strdown (lang);
			
			/* validate the country code */
			if (strlen (lang + 3) > 2) {
				/* invalid country code */
				lang[2] = '\0';
			} else {
				lang[2] = '-';
				e_strup (lang + 3);
			}
		} else if (lang[2] != '\0') {
			/* invalid language */
			g_free (lang);
			lang = NULL;
		}
		
		locale_lang = lang;
	} else {
		/* invalid language */
		locale_lang = NULL;
		g_free (lang);
	}
}


/**
 * camel_iconv_init:
 *
 * Initialize Camel's iconv cache. This *MUST* be called before any
 * camel-iconv interfaces will work correctly.
 **/
static void
camel_iconv_init (int keep)
{
	char *from, *to, *locale;
	int i;
	
	LOCK ();
	
	if (iconv_charsets != NULL) {
		if (!keep)
			UNLOCK ();
		return;
	}
	
	iconv_charsets = g_hash_table_new (g_str_hash, g_str_equal);
	
	for (i = 0; known_iconv_charsets[i].charset != NULL; i++) {
		from = g_strdup (known_iconv_charsets[i].charset);
		to = g_strdup (known_iconv_charsets[i].iconv_name);
		camel_strdown (from);
		g_hash_table_insert (iconv_charsets, from, to);
	}
	
	e_dlist_init (&iconv_cache_list);
	iconv_cache = g_hash_table_new (g_str_hash, g_str_equal);
	iconv_cache_open = g_hash_table_new (NULL, NULL);
	
	locale = setlocale (LC_ALL, NULL);
	
	if (!locale || !strcmp (locale, "C") || !strcmp (locale, "POSIX")) {
		/* The locale "C"  or  "POSIX"  is  a  portable  locale;  its
		 * LC_CTYPE  part  corresponds  to  the 7-bit ASCII character
		 * set.
		 */
		
		locale_charset = NULL;
		locale_lang = NULL;
	} else {
#ifdef HAVE_CODESET
		locale_charset = g_strdup (nl_langinfo (CODESET));
		camel_strdown (locale_charset);
#else
		/* A locale name is typically of  the  form  language[_terri-
		 * tory][.codeset][@modifier],  where  language is an ISO 639
		 * language code, territory is an ISO 3166 country code,  and
		 * codeset  is  a  character  set or encoding identifier like
		 * ISO-8859-1 or UTF-8.
		 */
		char *codeset, *p;
		
		codeset = strchr (locale, '.');
		if (codeset) {
			codeset++;
			
			/* ; is a hack for debian systems and / is a hack for Solaris systems */
			for (p = codeset; *p && !strchr ("@;/", *p); p++);
			locale_charset = g_strndup (codeset, p - codeset);
			camel_strdown (locale_charset);
		} else {
			/* charset unknown */
			locale_charset = NULL;
		}
#endif		
		
		/* parse the locale lang */
		locale_parse_lang (locale);
	}
	
	if (!keep)
		UNLOCK ();
}


/**
 * camel_iconv_charset_name:
 * @charset: charset name
 *
 * Maps charset names to the names that iconv_open() is more
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
	camel_strdown (name);
	
	camel_iconv_init (TRUE);
	if ((iname = g_hash_table_lookup (iconv_charsets, name)) != NULL) {
		UNLOCK ();
		return iname;
	}
	
	/* Unknown, try canonicalise some basic charset types to something that should work */
	if (strncmp (name, "iso", 3) == 0) {
		/* Convert iso-####-# or iso####-# or iso_####-# to iso-####-# or iso####-# */
		int iso, codepage;
		char *p;
		
		tmp = name + 3;
		if (*tmp == '-' || *tmp == '_')
			tmp++;
		
		iso = strtoul (tmp, &p, 10);
		
		if (iso == 10646) {
			/* they all become ICONV_10646 */
			iname = g_strdup (ICONV_10646);
		} else {
			tmp = p;
			if (*tmp == '-' || *tmp == '_')
				tmp++;
			
			codepage = strtoul (tmp, &p, 10);
			
			if (p > tmp) {
				/* codepage is numeric */
#ifdef __aix__
				if (codepage == 13)
					iname = g_strdup ("IBM-921");
				else
#endif /* __aix__ */
					iname = g_strdup_printf (ICONV_ISO_D_FORMAT, iso, codepage);
			} else {
				/* codepage is a string - probably iso-2022-jp or something */
				iname = g_strdup_printf (ICONV_ISO_S_FORMAT, iso, p);
			}
		}
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
		/* Just assume its ok enough as is, case and all */
		iname = g_strdup (charset);
	}
	
	g_hash_table_insert (iconv_charsets, g_strdup (name), iname);
	UNLOCK ();
	
	return iname;
}

static void
flush_entry (struct _iconv_cache *ic)
{
	struct _iconv_cache_node *in, *nn;
	
	in = (struct _iconv_cache_node *) ic->open.head;
	nn = in->next;
	while (nn) {
		if (in->cd != (iconv_t) -1) {
			g_hash_table_remove (iconv_cache_open, in->cd);
			iconv_close (in->cd);
		}
		
		g_free (in);
		in = nn;
		nn = in->next;
	}
	
	g_free (ic->conv);
	g_free (ic);
}


/* This should run pretty quick, its called a lot */
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
	struct _iconv_cache *ic;
	struct _iconv_cache_node *in;
	int errnosav;
	iconv_t cd;
	char *key;
	
	if (to == NULL || from == NULL) {
		errno = EINVAL;
		return (iconv_t) -1;
	}
	
	to = camel_iconv_charset_name (to);
	from = camel_iconv_charset_name (from);
	key = g_alloca (strlen (to) + strlen (from) + 2);
	sprintf (key, "%s:%s", to, from);
	
	LOCK ();
	
	ic = g_hash_table_lookup (iconv_cache, key);
	if (ic) {
		e_dlist_remove ((EDListNode *) ic);
	} else {
		struct _iconv_cache *last = (struct _iconv_cache *) iconv_cache_list.tailpred;
		struct _iconv_cache *prev;
		
		prev = last->prev;
		while (prev && iconv_cache_size > CAMEL_ICONV_CACHE_SIZE) {
			in = (struct _iconv_cache_node *) last->open.head;
			if (in->next && !in->busy) {
				d(printf ("Flushing iconv converter '%s'\n", last->conv));
				e_dlist_remove ((EDListNode *) last);
				g_hash_table_remove (iconv_cache, last->conv);
				flush_entry (last);
				iconv_cache_size--;
			}
			last = prev;
			prev = last->prev;
		}
		
		iconv_cache_size++;
		
		ic = g_new (struct _iconv_cache);
		e_dlist_init (&ic->open);
		ic->conv = g_strdup (key);
		g_hash_table_insert (iconv_cache, ic->conv, ic);
		
		d(printf ("Creating iconv converter '%s'\n", ic->conv));
	}
	e_dlist_addhead (&iconv_cache_list, (EDListNode *) ic);
	
	/* If we have a free iconv, use it */
	in = (struct _iconv_cache_node *) ic->open.tailpred;
	if (in->prev && !in->busy) {
		d(printf ("using existing iconv converter '%s'\n", ic->conv));
		cd = in->cd;
		if (cd != (iconv_t) -1) {
			/* work around some broken iconv implementations 
			 * that die if the length arguments are NULL 
			 */
			size_t buggy_iconv_len = 0;
			char *buggy_iconv_buf = NULL;
			
			/* resets the converter */
			iconv (cd, &buggy_iconv_buf, &buggy_iconv_len, &buggy_iconv_buf, &buggy_iconv_len);
			in->busy = TRUE;
			e_dlist_remove ((EDListNode *) in);
			e_dlist_addhead (&ic->open, (EDListNode *) in);
		}
	} else {
		d(printf("creating new iconv converter '%s'\n", ic->conv));
		cd = iconv_open (to, from);
		in = g_new (struct _iconv_cache_node);
		in->cd = cd;
		in->parent = ic;
		e_dlist_addhead (&ic->open, (EDListNode *) in);
		if (cd != (iconv_t) -1) {
			g_hash_table_insert (iconv_cache_open, cd, in);
			in->busy = TRUE;
		} else {
			errnosav = errno;
			g_warning ("Could not open converter for '%s' to '%s' charset", from, to);
			in->busy = FALSE;
			errno = errnosav;
		}
	}
	
	UNLOCK ();
	
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
void
camel_iconv_close (iconv_t cd)
{
	struct _iconv_cache_node *in;
	
	if (cd == (iconv_t) -1)
		return;
	
	LOCK ();
	
	in = g_hash_table_lookup (iconv_cache_open, cd);
	if (in) {
		d(printf ("closing iconv converter '%s'\n", in->parent->conv));
		e_dlist_remove ((EDListNode *) in);
		in->busy = FALSE;
		e_dlist_addtail (&in->parent->open, (EDListNode *) in);
	} else {
		g_warning ("trying to close iconv i dont know about: %p", cd);
		iconv_close (cd);
	}
	
	UNLOCK ();
}


const char *
camel_iconv_locale_charset (void)
{
	camel_iconv_init (FALSE);
	
	return locale_charset;
}


const char *
camel_iconv_locale_language (void)
{
	camel_iconv_init (FALSE);
	
	return locale_lang;
}

/* map CJKR charsets to their language code */
/* NOTE: only support charset names that will be returned by
 * e_iconv_charset_name() so that we don't have to keep track of all
 * the aliases too. */
static struct {
	char *charset;
	char *lang;
} cjkr_lang_map[] = {
	{ "Big5",        "zh" },
	{ "BIG5HKSCS",   "zh" },
	{ "gb2312",      "zh" },
	{ "gb18030",     "zh" },
	{ "gbk",         "zh" },
	{ "euc-tw",      "zh" },
	{ "iso-2022-jp", "ja" },
	{ "sjis",        "ja" },
	{ "ujis",        "ja" },
	{ "eucJP",       "ja" },
	{ "euc-jp",      "ja" },
	{ "euc-kr",      "ko" },
	{ "koi8-r",      "ru" },
	{ "koi8-u",      "uk" }
};

#define NUM_CJKR_LANGS (sizeof (cjkr_lang_map) / sizeof (cjkr_lang_map[0]))

const char *
camel_iconv_charset_language (const char *charset)
{
	int i;
	
	if (!charset)
		return NULL;
	
	charset = camel_iconv_charset_name (charset);
	for (i = 0; i < NUM_CJKR_LANGS; i++) {
		if (!strcasecmp (cjkr_lang_map[i].charset, charset))
			return cjkr_lang_map[i].lang;
	}
	
	return NULL;
}
