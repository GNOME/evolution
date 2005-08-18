/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* 
 * e-iconv.c
 * Copyright 2000, 2001, Ximian, Inc.
 *
 * Authors:
 *   Michael Zucchi <notzed@ximian.com>
 *   Jeffery Stedfast <fejj@ximian.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License, version 2, as published by the Free Software Foundation.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
 * 02111-1307, USA.
 */

#include <config.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <locale.h>
#ifdef HAVE_CODESET
#include <langinfo.h>
#endif

#include <glib.h>

#include "iconv-detect.h"
#include "e-iconv.h"

#define cd(x) 

#ifdef G_THREADS_ENABLED
static GStaticMutex lock = G_STATIC_MUTEX_INIT;
#define LOCK() g_static_mutex_lock(&lock)
#define UNLOCK() g_static_mutex_unlock(&lock)
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
	iconv_t ip;
};

struct _iconv_cache {
	struct _iconv_cache *next;
	struct _iconv_cache *prev;

	char *conv;

	EDList open;		/* stores iconv_cache_nodes, busy ones up front */
};

#define E_ICONV_CACHE_SIZE (16)

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
	{ "euc-cn",         "gb2312"     },
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
static void e_dlist_init(EDList *v)
{
        v->head = (EDListNode *)&v->tail;
        v->tail = 0;
        v->tailpred = (EDListNode *)&v->head;
}

static EDListNode *e_dlist_addhead(EDList *l, EDListNode *n)
{
        n->next = l->head;
        n->prev = (EDListNode *)&l->head;
        l->head->prev = n;
        l->head = n;
        return n;
}

static EDListNode *e_dlist_addtail(EDList *l, EDListNode *n)
{
        n->next = (EDListNode *)&l->tail;
        n->prev = l->tailpred;
        l->tailpred->next = n;
        l->tailpred = n;
        return n;
}

static EDListNode *e_dlist_remove(EDListNode *n)
{
        n->next->prev = n->prev;
        n->prev->next = n->next;
        return n;
}


/* fucking glib... */
static const char *
e_strdown (char *str)
{
	register char *s = str;
	
	while (*s) {
		if (*s >= 'A' && *s <= 'Z')
			*s += 0x20;
		s++;
	}
	
	return str;
}

static const char *
e_strup (char *str)
{
	register char *s = str;
	
	while (*s) {
		if (*s >= 'a' && *s <= 'z')
			*s -= 0x20;
		s++;
	}
	
	return str;
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
			e_strdown (lang);
			
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

/* NOTE: Owns the lock on return if keep is TRUE ! */
static void
e_iconv_init(int keep)
{
	char *from, *to, *locale;
	int i;

	LOCK();

	if (iconv_charsets != NULL) {
		if (!keep)
			UNLOCK();
		return;
	}

	iconv_charsets = g_hash_table_new(g_str_hash, g_str_equal);
	
	for (i = 0; known_iconv_charsets[i].charset != NULL; i++) {
		from = g_strdup(known_iconv_charsets[i].charset);
		to = g_strdup(known_iconv_charsets[i].iconv_name);
		e_strdown (from);
		g_hash_table_insert(iconv_charsets, from, to);
	}

	e_dlist_init(&iconv_cache_list);
	iconv_cache = g_hash_table_new(g_str_hash, g_str_equal);
	iconv_cache_open = g_hash_table_new(NULL, NULL);
	
#ifndef G_OS_WIN32
	locale = setlocale (LC_ALL, NULL);
#else
	locale = g_win32_getlocale ();
#endif
	
	if (!locale || !strcmp (locale, "C") || !strcmp (locale, "POSIX")) {
		/* The locale "C"  or  "POSIX"  is  a  portable  locale;  its
		 * LC_CTYPE  part  corresponds  to  the 7-bit ASCII character
		 * set.
		 */
		
		locale_charset = NULL;
		locale_lang = NULL;
	} else {
#ifdef G_OS_WIN32
		g_get_charset (&locale_charset);
		locale_charset = g_strdup (locale_charset);
		e_strdown (locale_charset);
#else
#ifdef HAVE_CODESET
		locale_charset = g_strdup (nl_langinfo (CODESET));
		e_strdown (locale_charset);
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
			e_strdown (locale_charset);
		} else {
			/* charset unknown */
			locale_charset = NULL;
		}
#endif		
#endif	/* !G_OS_WIN32 */
		
		/* parse the locale lang */
		locale_parse_lang (locale);

	}

#ifdef G_OS_WIN32
	g_free (locale);
#endif
	if (!keep)
		UNLOCK();
}

const char *e_iconv_charset_name(const char *charset)
{
	char *name, *ret, *tmp;

	if (charset == NULL)
		return NULL;

	name = g_alloca (strlen (charset) + 1);
	strcpy (name, charset);
	e_strdown (name);
	
	e_iconv_init(TRUE);
	ret = g_hash_table_lookup(iconv_charsets, name);
	if (ret != NULL) {
		UNLOCK();
		return ret;
	}

	/* Unknown, try canonicalise some basic charset types to something that should work */
	if (strncmp(name, "iso", 3) == 0) {
		/* Convert iso-nnnn-n or isonnnn-n or iso_nnnn-n to iso-nnnn-n or isonnnn-n */
		int iso, codepage;
		char *p;
		
		tmp = name + 3;
		if (*tmp == '-' || *tmp == '_')
			tmp++;
		
		iso = strtoul (tmp, &p, 10);
		
		if (iso == 10646) {
			/* they all become ICONV_10646 */
			ret = g_strdup (ICONV_10646);
		} else {
			tmp = p;
			if (*tmp == '-' || *tmp == '_')
				tmp++;
			
			codepage = strtoul (tmp, &p, 10);
			
			if (p > tmp) {
				/* codepage is numeric */
#ifdef __aix__
				if (codepage == 13)
					ret = g_strdup ("IBM-921");
				else
#endif /* __aix__ */
					ret = g_strdup_printf (ICONV_ISO_D_FORMAT, iso, codepage);
			} else {
				/* codepage is a string - probably iso-2022-jp or something */
				ret = g_strdup_printf (ICONV_ISO_S_FORMAT, iso, p);
			}
		}
	} else if (strncmp(name, "windows-", 8) == 0) {
		/* Convert windows-nnnnn or windows-cpnnnnn to cpnnnn */
		tmp = name+8;
		if (!strncmp(tmp, "cp", 2))
			tmp+=2;
		ret = g_strdup_printf("CP%s", tmp);
	} else if (strncmp(name, "microsoft-", 10) == 0) {
		/* Convert microsoft-nnnnn or microsoft-cpnnnnn to cpnnnn */
		tmp = name+10;
		if (!strncmp(tmp, "cp", 2))
			tmp+=2;
		ret = g_strdup_printf("CP%s", tmp);	
	} else {
		/* Just assume its ok enough as is, case and all */
		ret = g_strdup(charset);
	}

	g_hash_table_insert(iconv_charsets, g_strdup(name), ret);
	UNLOCK();

	return ret;
}

static void
flush_entry(struct _iconv_cache *ic)
{
	struct _iconv_cache_node *in, *nn;

	in = (struct _iconv_cache_node *)ic->open.head;
	nn = in->next;
	while (nn) {
		if (in->ip != (iconv_t)-1) {
			g_hash_table_remove(iconv_cache_open, in->ip);
			iconv_close(in->ip);
		}
		g_free(in);
		in = nn;
		nn = in->next;
	}
	g_free(ic->conv);
	g_free(ic);
}

/* This should run pretty quick, its called a lot */
iconv_t e_iconv_open(const char *oto, const char *ofrom)
{
	const char *to, *from;
	char *tofrom;
	struct _iconv_cache *ic;
	struct _iconv_cache_node *in;
	int errnosav;
	iconv_t ip;

	if (oto == NULL || ofrom == NULL) {
		errno = EINVAL;
		return (iconv_t) -1;
	}
	
	to = e_iconv_charset_name (oto);
	from = e_iconv_charset_name (ofrom);
	tofrom = g_alloca (strlen (to) + strlen (from) + 2);
	sprintf(tofrom, "%s%%%s", to, from);

	LOCK();

	ic = g_hash_table_lookup(iconv_cache, tofrom);
	if (ic) {
		e_dlist_remove((EDListNode *)ic);
	} else {
		struct _iconv_cache *last = (struct _iconv_cache *)iconv_cache_list.tailpred;
		struct _iconv_cache *prev;

		prev = last->prev;
		while (prev && iconv_cache_size > E_ICONV_CACHE_SIZE) {
			in = (struct _iconv_cache_node *)last->open.head;
			if (in->next && !in->busy) {
				cd(printf("Flushing iconv converter '%s'\n", last->conv));
				e_dlist_remove((EDListNode *)last);
				g_hash_table_remove(iconv_cache, last->conv);
				flush_entry(last);
				iconv_cache_size--;
			}
			last = prev;
			prev = last->prev;
		}

		iconv_cache_size++;
		
		ic = g_malloc(sizeof(*ic));
		e_dlist_init(&ic->open);
		ic->conv = g_strdup(tofrom);
		g_hash_table_insert(iconv_cache, ic->conv, ic);

		cd(printf("Creating iconv converter '%s'\n", ic->conv));
	}
	e_dlist_addhead(&iconv_cache_list, (EDListNode *)ic);

	/* If we have a free iconv, use it */
	in = (struct _iconv_cache_node *)ic->open.tailpred;
	if (in->prev && !in->busy) {
		cd(printf("using existing iconv converter '%s'\n", ic->conv));
		ip = in->ip;
		if (ip != (iconv_t)-1) {
			/* work around some broken iconv implementations 
			 * that die if the length arguments are NULL 
			 */
			size_t buggy_iconv_len = 0;
			char *buggy_iconv_buf = NULL;

			/* resets the converter */
			iconv(ip, &buggy_iconv_buf, &buggy_iconv_len, &buggy_iconv_buf, &buggy_iconv_len);
			in->busy = TRUE;
			e_dlist_remove((EDListNode *)in);
			e_dlist_addhead(&ic->open, (EDListNode *)in);
		}
	} else {
		cd(printf("creating new iconv converter '%s'\n", ic->conv));
		ip = iconv_open(to, from);
		in = g_malloc(sizeof(*in));
		in->ip = ip;
		in->parent = ic;
		e_dlist_addhead(&ic->open, (EDListNode *)in);
		if (ip != (iconv_t)-1) {
			g_hash_table_insert(iconv_cache_open, ip, in);
			in->busy = TRUE;
		} else {
			errnosav = errno;
			g_warning("Could not open converter for '%s' to '%s' charset", from, to);
			in->busy = FALSE;
			errno = errnosav;
		}
	}

	UNLOCK();

	return ip;
}

size_t e_iconv(iconv_t cd, const char **inbuf, size_t *inbytesleft, char ** outbuf, size_t *outbytesleft)
{
	return iconv(cd, (char **) inbuf, inbytesleft, outbuf, outbytesleft);
}

void
e_iconv_close(iconv_t ip)
{
	struct _iconv_cache_node *in;

	if (ip == (iconv_t)-1)
		return;

	LOCK();
	in = g_hash_table_lookup(iconv_cache_open, ip);
	if (in) {
		cd(printf("closing iconv converter '%s'\n", in->parent->conv));
		e_dlist_remove((EDListNode *)in);
		in->busy = FALSE;
		e_dlist_addtail(&in->parent->open, (EDListNode *)in);
	} else {
		g_warning("trying to close iconv i dont know about: %p", ip);
		iconv_close(ip);
	}
	UNLOCK();

}

const char *e_iconv_locale_charset(void)
{
	e_iconv_init(FALSE);

	return locale_charset;
}


const char *
e_iconv_locale_language (void)
{
	e_iconv_init (FALSE);
	
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
e_iconv_charset_language (const char *charset)
{
	int i;
	
	if (!charset)
		return NULL;
	
	charset = e_iconv_charset_name (charset);
	for (i = 0; i < NUM_CJKR_LANGS; i++) {
		if (!strcasecmp (cjkr_lang_map[i].charset, charset))
			return cjkr_lang_map[i].lang;
	}
	
	return NULL;
}
