/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 *  Authors: Jeffrey Stedfast <fejj@ximian.com>
 *	     Michael Zucchi <NotZed@Ximian.com>
 *
 *  Copyright 2000 Ximian, Inc. (www.ximian.com)
 *  Copyright 2001 Ximian Inc. (www.ximian.com)
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of version 2 of the GNU General Public
 * License as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 */

/* (from glibc headers:
   POSIX says that <sys/types.h> must be included (by the caller) before <regex.h>.  */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <glib.h>
#include <sys/types.h>
#include <regex.h>
#include <string.h>
#include <ctype.h>
#include <stdio.h>

#include "camel-exception.h"
#include "camel-mime-message.h"
#include "camel-multipart.h"
#include "camel-stream-mem.h"
#include "e-util/e-sexp.h"
#include "camel-search-private.h"
#include "camel-i18n.h"

#include <glib/gunicode.h>

#define d(x)

static inline guint32
camel_utf8_getc(const unsigned char **ptr)
{
	register unsigned char *p = (unsigned char *)*ptr;
	register unsigned char c, r;
	register guint32 v, m;

again:
	r = *p++;
loop:
	if (r < 0x80) {
		*ptr = p;
		v = r;
	} else if (r < 0xfe) { /* valid start char? */
		v = r;
		m = 0x7f80;	/* used to mask out the length bits */
		do {
			c = *p++;
			if ((c & 0xc0) != 0x80) {
				r = c;
				goto loop;
			}
			v = (v<<6) | (c & 0x3f);
			r<<=1;
			m<<=5;
		} while (r & 0x40);
		
		*ptr = p;

		v &= ~m;
	} else {
		goto again;
	}

	return v;
}

/* builds the regex into pattern */
/* taken from camel-folder-search, with added isregex & exception parameter */
/* Basically, we build a new regex, either based on subset regex's, or substrings,
   that can be executed once over the whoel body, to match anything suitable.
   This is more efficient than multiple searches, and probably most (naive) strstr
   implementations, over long content.

   A small issue is that case-insenstivity wont work entirely correct for utf8 strings. */
int
camel_search_build_match_regex (regex_t *pattern, camel_search_flags_t type, int argc,
				struct _ESExpResult **argv, CamelException *ex)
{
	GString *match = g_string_new("");
	int c, i, count=0, err;
	char *word;
	int flags;
	
	/* build a regex pattern we can use to match the words, we OR them together */
	if (argc>1)
		g_string_append_c (match, '(');
	for (i = 0; i < argc; i++) {
		if (argv[i]->type == ESEXP_RES_STRING) {
			if (count > 0)
				g_string_append_c (match, '|');
			
			word = argv[i]->value.string;
			if (type & CAMEL_SEARCH_MATCH_REGEX) {
				/* no need to escape because this should already be a valid regex */
				g_string_append (match, word);
			} else {
				/* escape any special chars (not sure if this list is complete) */
				if (type & CAMEL_SEARCH_MATCH_START)
					g_string_append_c (match, '^');
				while ((c = *word++)) {
					if (strchr ("*\\.()[]^$+", c) != NULL) {
						g_string_append_c (match, '\\');
					}
					g_string_append_c (match, c);
				}
				if (type & CAMEL_SEARCH_MATCH_END)
					g_string_append_c (match, '^');
			}
			count++;
		} else {
			g_warning("Invalid type passed to body-contains match function");
		}
	}
	if (argc > 1)
		g_string_append_c (match, ')');
	flags = REG_EXTENDED|REG_NOSUB;
	if (type & CAMEL_SEARCH_MATCH_ICASE)
		flags |= REG_ICASE;
	if (type & CAMEL_SEARCH_MATCH_NEWLINE)
		flags |= REG_NEWLINE;
	err = regcomp (pattern, match->str, flags);
	if (err != 0) {
		/* regerror gets called twice to get the full error string 
		   length to do proper posix error reporting */
		int len = regerror (err, pattern, 0, 0);
		char *buffer = g_malloc0 (len + 1);
		
		regerror (err, pattern, buffer, len);
		camel_exception_setv (ex, CAMEL_EXCEPTION_SYSTEM,
				      _("Regular expression compilation failed: %s: %s"),
				      match->str, buffer);
		
		regfree (pattern);
	}
	d(printf("Built regex: '%s'\n", match->str));
	g_string_free (match, TRUE);
	
	return err;
}

static unsigned char soundex_table[256] = {
	  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
	  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
	  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
	  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
	  0,  0, 49, 50, 51,  0, 49, 50,  0,  0, 50, 50, 52, 53, 53,  0,
	 49, 50, 54, 50, 51,  0, 49,  0, 50,  0, 50,  0,  0,  0,  0,  0,
	  0,  0, 49, 50, 51,  0, 49, 50,  0,  0, 50, 50, 52, 53, 53,  0,
	 49, 50, 54, 50, 51,  0, 49,  0, 50,  0, 50,  0,  0,  0,  0,  0,
	  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
	  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
	  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
	  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
	  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
	  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
	  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
	  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
};

static void
soundexify (const gchar *sound, gchar code[5])
{
	guchar *c, last = '\0';
	gint n;
	
	for (c = (guchar *) sound; *c && !isalpha (*c); c++);
	code[0] = toupper (*c);
	memset (code + 1, '0', 3);
	for (n = 1; *c && n < 5; c++) {
		guchar ch = soundex_table[*c];
		
		if (ch && ch != last) {
			code[n++] = ch;
			last = ch;
		}
	}
	code[4] = '\0';
}

static gboolean
header_soundex (const char *header, const char *match)
{
	char mcode[5], hcode[5];
	const char *p;
	char c;
	GString *word;
	int truth = FALSE;
	
	soundexify (match, mcode);
	
	/* split the header into words, and soundexify and compare each one */
	/* FIXME: Should this convert to utf8, and split based on that, and what not?
	   soundex only makes sense for us-ascii though ... */
	
	word = g_string_new("");
	p = header;
	do {
		c = *p++;
		if (c == 0 || isspace (c)) {
			if (word->len > 0) {
				soundexify (word->str, hcode);
				if (strcmp (hcode, mcode) == 0)
					truth = TRUE;
			}
			g_string_truncate (word, 0);
		} else if (isalpha (c))
			g_string_append_c (word, c);
	} while (c && !truth);
	g_string_free (word, TRUE);
	
	return truth;
}

const char *
camel_ustrstrcase (const char *haystack, const char *needle)
{
	gunichar *nuni, *puni;
	gunichar u;
	const unsigned char *p;
	
	g_return_val_if_fail (haystack != NULL, NULL);
	g_return_val_if_fail (needle != NULL, NULL);
	
	if (strlen (needle) == 0)
		return haystack;
	if (strlen (haystack) == 0)
		return NULL;
	
	puni = nuni = g_alloca (sizeof (gunichar) * strlen (needle));
	
	p = needle;
	while ((u = camel_utf8_getc(&p)))
		*puni++ = g_unichar_tolower (u);
	
	/* NULL means there was illegal utf-8 sequence */
	if (!p)
		return NULL;
	
	p = (const unsigned char *)haystack;
	while ((u = camel_utf8_getc(&p))) {
		gunichar c;
		
		c = g_unichar_tolower (u);
		/* We have valid stripped char */
		if (c == nuni[0]) {
			const unsigned char *q = p;
			gint npos = 1;
			
			while (nuni + npos < puni) {
				u = camel_utf8_getc(&q);
				if (!q || !u)
					return NULL;
				
				c = g_unichar_tolower (u);				
				if (c != nuni[npos])
					break;
				
				npos++;
			}
			
			if (nuni + npos == puni)
				return p;
		}
	}
	
	return NULL;
}

#define CAMEL_SEARCH_COMPARE(x, y, z) G_STMT_START {   \
	if ((x) == (z)) {                              \
		if ((y) == (z))                        \
			return 0;                      \
		else                                   \
			return -1;                     \
	} else if ((y) == (z))                         \
		return 1;                              \
} G_STMT_END

static int
camel_ustrcasecmp (const char *s1, const char *s2)
{
	gunichar u1, u2 = 0;
	
	CAMEL_SEARCH_COMPARE (s1, s2, NULL);
	
	u1 = camel_utf8_getc((const unsigned char **)&s1);
	u2 = camel_utf8_getc((const unsigned char **)&s2);
	while (u1 && u2) {
		u1 = g_unichar_tolower (u1);
		u2 = g_unichar_tolower (u2);
		if (u1 < u2)
			return -1;
		else if (u1 > u2)
			return 1;
		
		u1 = camel_utf8_getc((const unsigned char **)&s1);
		u2 = camel_utf8_getc((const unsigned char **)&s2);
	}
	
	/* end of one of the strings ? */
	CAMEL_SEARCH_COMPARE (u1, u2, 0);
	
	/* if we have invalid utf8 sequence ?  */
	CAMEL_SEARCH_COMPARE (s1, s2, NULL);
	
	return 0;
}

static int
camel_ustrncasecmp (const char *s1, const char *s2, size_t len)
{
	gunichar u1, u2 = 0;
	
	CAMEL_SEARCH_COMPARE (s1, s2, NULL);
	
	u1 = camel_utf8_getc((const unsigned char **)&s1);
	u2 = camel_utf8_getc((const unsigned char **)&s2);
	while (len > 0 && u1 && u2) {
		u1 = g_unichar_tolower (u1);
		u2 = g_unichar_tolower (u2);
		if (u1 < u2)
			return -1;
		else if (u1 > u2)
			return 1;
		
		len--;
		u1 = camel_utf8_getc((const unsigned char **)&s1);
		u2 = camel_utf8_getc((const unsigned char **)&s2);
	}
	
	if (len == 0)
		return 0;
	
	/* end of one of the strings ? */
	CAMEL_SEARCH_COMPARE (u1, u2, 0);
	
	/* if we have invalid utf8 sequence ?  */
	CAMEL_SEARCH_COMPARE (s1, s2, NULL);
	
	return 0;
}

/* value is the match value suitable for exact match if required */
static int
header_match(const char *value, const char *match, camel_search_match_t how)
{
	const unsigned char *p;
	int vlen, mlen;
	
	if (how == CAMEL_SEARCH_MATCH_SOUNDEX)
		return header_soundex (value, match);

	vlen = strlen(value);
	mlen = strlen(match);
	if (vlen < mlen)
		return FALSE;
	
	/* from dan the man, if we have mixed case, perform a case-sensitive match,
	   otherwise not */
	p = (const unsigned char *)match;
	while (*p) {
		if (isupper(*p)) {
			switch (how) {
			case CAMEL_SEARCH_MATCH_EXACT:
				return strcmp(value, match) == 0;
			case CAMEL_SEARCH_MATCH_CONTAINS:
				return strstr(value, match) != NULL;
			case CAMEL_SEARCH_MATCH_STARTS:
				return strncmp(value, match, mlen) == 0;
			case CAMEL_SEARCH_MATCH_ENDS:
				return strcmp(value + vlen - mlen, match) == 0;
			default:
				break;
			}
			return FALSE;
		}
		p++;
	}
	
	switch (how) {
	case CAMEL_SEARCH_MATCH_EXACT:
		return camel_ustrcasecmp(value, match) == 0;
	case CAMEL_SEARCH_MATCH_CONTAINS:
		return camel_ustrstrcase(value, match) != NULL;
	case CAMEL_SEARCH_MATCH_STARTS:
		return camel_ustrncasecmp(value, match, mlen) == 0;
	case CAMEL_SEARCH_MATCH_ENDS:
		return camel_ustrcasecmp(value + vlen - mlen, match) == 0;
	default:
		break;
	}
	
	return FALSE;
}

/* searhces for match inside value, if match is mixed case, hten use case-sensitive,
   else insensitive */
gboolean
camel_search_header_match (const char *value, const char *match, camel_search_match_t how, camel_search_t type, const char *default_charset)
{
	const char *name, *addr;
	int truth = FALSE, i;
	CamelInternetAddress *cia;
	char *v, *vdom, *mdom;

	while (*value && isspace (*value))
		value++;
	
	switch(type) {
	case CAMEL_SEARCH_TYPE_ENCODED:
		v = camel_header_decode_string(value, default_charset); /* FIXME: Find header charset */
		truth = header_match(v, match, how);
		g_free(v);
		break;
	case CAMEL_SEARCH_TYPE_MLIST:
		/* Special mailing list old-version domain hack
		   If one of the mailing list names doesn't have an @ in it, its old-style, so
		   only match against the pre-domain part, which should be common */

		vdom = strchr(value, '@');
		mdom = strchr(match, '@');
		if (mdom == NULL && vdom != NULL) {
			v = g_alloca(vdom-value+1);
			memcpy(v, value, vdom-value);
			v[vdom-value] = 0;
			value = (char *)v;
		} else if (mdom != NULL && vdom == NULL) {
			v = g_alloca(mdom-match+1);
			memcpy(v, match, mdom-match);
			v[mdom-match] = 0;
			match = (char *)v;
		}
		/* Falls through */
	case CAMEL_SEARCH_TYPE_ASIS:
		truth = header_match(value, match, how);
		break;
	case CAMEL_SEARCH_TYPE_ADDRESS_ENCODED:
	case CAMEL_SEARCH_TYPE_ADDRESS:
		/* possible simple case to save some work if we can */
		if (header_match(value, match, how))
			return TRUE;

		/* Now we decode any addresses, and try asis matches on name and address parts */
		cia = camel_internet_address_new();
		if (type == CAMEL_SEARCH_TYPE_ADDRESS_ENCODED)
			camel_address_decode((CamelAddress *)cia, value);
		else
			camel_address_unformat((CamelAddress *)cia, value);

		for (i=0; !truth && camel_internet_address_get(cia, i, &name, &addr);i++)
			truth = (name && header_match(name, match, how)) || (addr && header_match(addr, match, how));
		
		camel_object_unref (cia);
		break;
	}

	return truth;
}

/* performs a 'slow' content-based match */
/* there is also an identical copy of this in camel-filter-search.c */
gboolean
camel_search_message_body_contains (CamelDataWrapper *object, regex_t *pattern)
{
	CamelDataWrapper *containee;
	int truth = FALSE;
	int parts, i;
	
	containee = camel_medium_get_content_object (CAMEL_MEDIUM (object));
	
	if (containee == NULL)
		return FALSE;
	
	/* using the object types is more accurate than using the mime/types */
	if (CAMEL_IS_MULTIPART (containee)) {
		parts = camel_multipart_get_number (CAMEL_MULTIPART (containee));
		for (i = 0; i < parts && truth == FALSE; i++) {
			CamelDataWrapper *part = (CamelDataWrapper *)camel_multipart_get_part (CAMEL_MULTIPART (containee), i);
			if (part)
				truth = camel_search_message_body_contains (part, pattern);
		}
	} else if (CAMEL_IS_MIME_MESSAGE (containee)) {
		/* for messages we only look at its contents */
		truth = camel_search_message_body_contains ((CamelDataWrapper *)containee, pattern);
	} else if (camel_content_type_is(CAMEL_DATA_WRAPPER (containee)->mime_type, "text", "*")) {
		/* for all other text parts, we look inside, otherwise we dont care */
		CamelStreamMem *mem = (CamelStreamMem *)camel_stream_mem_new ();
		
		camel_data_wrapper_write_to_stream (containee, CAMEL_STREAM (mem));
		camel_stream_write (CAMEL_STREAM (mem), "", 1);
		truth = regexec (pattern, mem->buffer->data, 0, NULL, 0) == 0;
		camel_object_unref (mem);
	}
	
	return truth;
}

static void
output_c(GString *w, guint32 c, int *type)
{
	int utf8len;
	char utf8[8];

	if (!g_unichar_isalnum(c))
		*type = CAMEL_SEARCH_WORD_COMPLEX | (*type & CAMEL_SEARCH_WORD_8BIT);
	else
		c = g_unichar_tolower(c);

	if (c > 0x80)
		*type |= CAMEL_SEARCH_WORD_8BIT;

	/* FIXME: use camel_utf8_putc */
	utf8len = g_unichar_to_utf8(c, utf8);
	utf8[utf8len] = 0;
	g_string_append(w, utf8);
}

static void
output_w(GString *w, GPtrArray *list, int type)
{
	struct _camel_search_word *word;

	if (w->len) {
		word = g_malloc0(sizeof(*word));
		word->word = g_strdup(w->str);
		word->type = type;
		g_ptr_array_add(list, word);
		g_string_truncate(w, 0);
	}
}

struct _camel_search_words *
camel_search_words_split(const unsigned char *in)
{
	int type = CAMEL_SEARCH_WORD_SIMPLE, all = 0;
	GString *w;
	struct _camel_search_words *words;
	GPtrArray *list = g_ptr_array_new();
	guint32 c;
	int inquote = 0;

	words = g_malloc0(sizeof(*words));	
	w = g_string_new("");

	do {
		c = camel_utf8_getc(&in);

		if (c == 0
		    || (inquote && c == '"')
		    || (!inquote && g_unichar_isspace(c))) {
			output_w(w, list, type);
			all |= type;
			type = CAMEL_SEARCH_WORD_SIMPLE;
			inquote = 0;
		} else {
			if (c == '\\') {
				c = camel_utf8_getc(&in);
				if (c)
					output_c(w, c, &type);
				else {
					output_w(w, list, type);
					all |= type;
				}
			} else if (c == '\"') {
				inquote = 1;
			} else {
				output_c(w, c, &type);
			}
		}
	} while (c);

	g_string_free(w, TRUE);
	words->len = list->len;
	words->words = (struct _camel_search_word **)list->pdata;
	words->type = all;
	g_ptr_array_free(list, FALSE);

	return words;
}

/* takes an existing 'words' list, and converts it to another consisting of
   only simple words, with any punctuation etc stripped */
struct _camel_search_words *
camel_search_words_simple(struct _camel_search_words *wordin)
{
	int i;
	const unsigned char *ptr, *start, *last;
	int type = CAMEL_SEARCH_WORD_SIMPLE, all = 0;
	GPtrArray *list = g_ptr_array_new();
	struct _camel_search_word *word;
	struct _camel_search_words *words;
	guint32 c;

	words = g_malloc0(sizeof(*words));	

	for (i=0;i<wordin->len;i++) {
		if ((wordin->words[i]->type & CAMEL_SEARCH_WORD_COMPLEX) == 0) {
			word = g_malloc0(sizeof(*word));
			word->type = wordin->words[i]->type;
			word->word = g_strdup(wordin->words[i]->word);
			g_ptr_array_add(list, word);
		} else {
			ptr = wordin->words[i]->word;
			start = last = ptr;
			do {
				c = camel_utf8_getc(&ptr);
				if (c == 0 || !g_unichar_isalnum(c)) {
					if (last > start) {
						word = g_malloc0(sizeof(*word));
						word->word = g_strndup(start, last-start);
						word->type = type;
						g_ptr_array_add(list, word);
						all |= type;
						type = CAMEL_SEARCH_WORD_SIMPLE;
					}
					start = ptr;
				}
				if (c > 0x80)
					type = CAMEL_SEARCH_WORD_8BIT;
				last = ptr;
			} while (c);
		}
	}

	words->len = list->len;
	words->words = (struct _camel_search_word **)list->pdata;
	words->type = all;
	g_ptr_array_free(list, FALSE);

	return words;
}

void
camel_search_words_free(struct _camel_search_words *words)
{
	int i;

	for (i=0;i<words->len;i++) {
		struct _camel_search_word *word = words->words[i];

		g_free(word->word);
		g_free(word);
	}
	g_free(words->words);
	g_free(words);
}

