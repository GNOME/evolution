/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 *  Authors: Jeffrey Stedfast <fejj@helixcode.com>
 *	     Michael Zucchi <NotZed@Ximian.com>
 *
 *  Copyright 2000 Helix Code, Inc. (www.helixcode.com)
 *  Copyright 2001 Ximian Inc. (www.ximian.com)
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

/* (from glibc headers:
   POSIX says that <sys/types.h> must be included (by the caller) before <regex.h>.  */
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
#include <unicode.h>

#include "camel-search-private.h"

#define d(x)

/* builds the regex into pattern */
/* taken from camel-folder-search, with added isregex & exception parameter */
/* Basically, we build a new regex, either based on subset regex's, or substrings,
   that can be executed once over the whoel body, to match anything suitable.
   This is more efficient than multiple searches, and probably most (naive) strstr
   implementations, over long content.

   A small issue is that case-insenstivity wont work entirely correct for utf8 strings. */
int
camel_search_build_match_regex(regex_t *pattern, camel_search_flags_t type, int argc, struct _ESExpResult **argv, CamelException *ex)
{
	GString *match = g_string_new("");
	int c, i, count=0, err;
	char *word;
	int flags;

	/* build a regex pattern we can use to match the words, we OR them together */
	if (argc>1)
		g_string_append_c(match, '(');
	for (i=0;i<argc;i++) {
		if (argv[i]->type == ESEXP_RES_STRING) {
			if (count > 0)
				g_string_append_c(match, '|');
			/* escape any special chars (not sure if this list is complete) */
			word = argv[i]->value.string;
			if (type & CAMEL_SEARCH_MATCH_REGEX) {
				g_string_append(match, word);
			} else {
				if (type & CAMEL_SEARCH_MATCH_START)
					g_string_append_c(match, '^');
				while ((c = *word++)) {
					if (strchr("*\\.()[]^$+", c) != NULL) {
						g_string_append_c(match, '\\');
					}
					g_string_append_c(match, c);
				}
				if (type & CAMEL_SEARCH_MATCH_END)
					g_string_append_c(match, '^');
			}
			count++;
		} else {
			g_warning("Invalid type passed to body-contains match function");
		}
	}
	if (argc>1)
		g_string_append_c(match, ')');
	flags = REG_EXTENDED|REG_NOSUB;
	if (type & CAMEL_SEARCH_MATCH_ICASE)
		flags |= REG_ICASE;
	err = regcomp(pattern, match->str, flags);
	if (err != 0) {
		/* regerror gets called twice to get the full error string 
		   length to do proper posix error reporting */
		int len = regerror(err, pattern, 0, 0);
		char *buffer = g_malloc0(len + 1);

		regerror(err, pattern, buffer, len);
		camel_exception_setv(ex, CAMEL_EXCEPTION_SYSTEM,
				     _("Regular expression compilation failed: %s: %s"),
				     match->str, buffer);

		regfree(pattern);
	}
	d(printf("Built regex: '%s'\n", match->str));
	g_string_free(match, TRUE);
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
header_soundex(const char *header, const char *match)
{
	char mcode[5], hcode[5];
	const char *p;
	char c;
	GString *word;
	int truth = FALSE;

	soundexify(match, mcode);

	/* split the header into words, and soundexify and compare each one */
	/* FIXME: Should this convert to utf8, and split based on that, and what not?
	   soundex only makes sense for us-ascii though ... */

	word = g_string_new("");
	p = header;
	do {
		c = *p++;
		if (c == 0 || isspace(c)) {
			if (word->len > 0) {
				soundexify(word->str, hcode);
				if (strcmp(hcode, mcode) == 0)
					truth = TRUE;
			}
			g_string_truncate(word, 0);
		} else if (isalpha(c))
			g_string_append_c(word, c);
	} while (c && !truth);
	g_string_free(word, TRUE);

	return truth;
}

static guint16 utf8_get(const char **inp)
{
	guint32 c, v = 0, s, shift;
	const unsigned char *p = *inp;

	if (p == NULL)
		return 0;

	s = *p++;
	if ((s & 0x80) == 0) {	/* 7 bit char */
		v = s;
	} else if (s>0xf7) {	/* invalid char, we can only have upto 4 bits encoded */
		p = NULL;
	} else if (s>=0xc0) {	/* valid start char */
		shift = 0;
		do {
			c = *p++;
			if ((c & 0xc0) == 0x80) {
				v = (v<<6) | (c&0x3f);
				shift += 5;
			} else {
				*inp = NULL;
				return 0;
			}
			s <<= 1;
		} while ((s & 0x80) != 0);
		v |= s << shift;
	} else {		/* invalid start char, internal char */
		p = NULL;
	}

	*inp = p;
	return v;
}

static const char *
camel_ustrstrcase(const char *haystack, const char *needle)
{
	unicode_char_t *uni, *puni, *suni, u, v;
	const char *p, *s, *l;

	if (haystack == NULL || needle == NULL)
		return NULL;

	if (needle[0] == 0)
		return haystack;

	if (haystack[0] == 0)
		return NULL;

	puni = uni = alloca(sizeof(*uni)*(strlen(needle)+1));
	p = needle;
	while ((u = utf8_get(&p)))
		*puni++ = unicode_tolower(u);

	if (p == NULL)
		return NULL;

	l = p = haystack;
	while ( (u = utf8_get(&p)) ) {
		v = unicode_tolower(u);
		if (uni[0] == v) {
			s = p;
			suni = uni+1;
			while (suni < puni) {
				u = utf8_get(&s);
				v = unicode_tolower(u);
				if (v != *suni)
					goto next;
				suni++;
			}
			return l;
		}
	next:
		l = p;
	}

	return NULL;
}

static int
camel_ustrcasecmp(const char *s1, const char *s2)
{
	guint16 u1, u2=0;

	if (s1 == NULL) {
		if (s2 == NULL)
			return 0;
		else
			return -1;
	}
	if (s2 == NULL)
		return 1;
  
	while ((u1 = utf8_get(&s1)) && (u2 = utf8_get(&s2))) {
		u1 = unicode_tolower(u1);
		u2 = unicode_tolower(u2);
		if (u1 < u2)
			return -1;
		else if (u1 > u2)
			return 1;
	}

	/* if we have invalid utf8 sequence ?  */
	if (s2 == NULL || s1 == NULL)
		return 1;

	if (u1 == 0) {
		if (u2 == 0)
			return 0;
		else
			return -1;
	}
	if (u2 == 0)
		return 1;

	return 0;
}

static int
camel_ustrncasecmp(const char *s1, const char *s2, size_t len)
{
	guint16 u1, u2=0;

	if (s1 == NULL) {
		if (s2 == NULL)
			return 0;
		else
			return -1;
	}
	if (s2 == NULL)
		return 1;
  
	while (len > 0 && (u1 = utf8_get(&s1)) && (u2 = utf8_get(&s2))) {
		u1 = unicode_tolower(u1);
		u2 = unicode_tolower(u2);
		if (u1 < u2)
			return -1;
		else if (u1 > u2)
			return 1;
		len--;
	}

	if (len == 0)
		return 0;

	/* if we have invalid utf8 sequence ?  */
	if (s2 == NULL || s1 == NULL)
		return 1;

	if (u1 == 0) {
		if (u2 == 0)
			return 0;
		else
			return -1;
	}
	if (u2 == 0)
		return 1;

	return 0;
}


/* searhces for match inside value, if match is mixed case, hten use case-sensitive,
   else insensitive */
gboolean camel_search_header_match(const char *value, const char *match, camel_search_match_t how)
{
	const char *p;

	if (how == CAMEL_SEARCH_MATCH_SOUNDEX)
		return header_soundex(value, match);

	while (*value && isspace(*value))
		value++;

	if (strlen(value) < strlen(match))
		return FALSE;

	/* from dan the man, if we have mixed case, perform a case-sensitive match,
	   otherwise not */
	p = match;
	while (*p) {
		if (isupper(*p)) {
			switch(how) {
			case CAMEL_SEARCH_MATCH_EXACT:
				return strcmp(value, match) == 0;
			case CAMEL_SEARCH_MATCH_CONTAINS:
				return strstr(value, match) != NULL;
			case CAMEL_SEARCH_MATCH_STARTS:
				return strncmp(value, match, strlen(match)) == 0;
			case CAMEL_SEARCH_MATCH_ENDS:
				return strcmp(value+strlen(value)-strlen(match), match) == 0;
			default:
				break;
			}
			return FALSE;
		}
		p++;
	}
	switch(how) {
	case CAMEL_SEARCH_MATCH_EXACT:
		return camel_ustrcasecmp(value, match) == 0;
	case CAMEL_SEARCH_MATCH_CONTAINS:
		return camel_ustrstrcase(value, match) != NULL;
	case CAMEL_SEARCH_MATCH_STARTS:
		return camel_ustrncasecmp(value, match, strlen(match)) == 0;
	case CAMEL_SEARCH_MATCH_ENDS:
		return camel_ustrcasecmp(value+strlen(value)-strlen(match), match) == 0;
	default:
		break;
	}

	return FALSE;
}

/* performs a 'slow' content-based match */
/* there is also an identical copy of this in camel-filter-search.c */
gboolean
camel_search_message_body_contains(CamelDataWrapper *object, regex_t *pattern)
{
	CamelDataWrapper *containee;
	int truth = FALSE;
	int parts, i;

	containee = camel_medium_get_content_object(CAMEL_MEDIUM(object));

	if (containee == NULL)
		return FALSE;

	/* TODO: I find it odd that get_part and get_content_object do not
	   add a reference, probably need fixing for multithreading */

	/* using the object types is more accurate than using the mime/types */
	if (CAMEL_IS_MULTIPART(containee)) {
		parts = camel_multipart_get_number(CAMEL_MULTIPART(containee));
		for (i=0;i<parts && truth==FALSE;i++) {
			CamelDataWrapper *part = (CamelDataWrapper *)camel_multipart_get_part(CAMEL_MULTIPART(containee), i);
			if (part)
				truth = camel_search_message_body_contains(part, pattern);
		}
	} else if (CAMEL_IS_MIME_MESSAGE(containee)) {
		/* for messages we only look at its contents */
		truth = camel_search_message_body_contains((CamelDataWrapper *)containee, pattern);
	} else if (header_content_type_is(CAMEL_DATA_WRAPPER(containee)->mime_type, "text", "*")) {
		/* for all other text parts, we look inside, otherwise we dont care */
		CamelStreamMem *mem = (CamelStreamMem *)camel_stream_mem_new();

		camel_data_wrapper_write_to_stream(containee, (CamelStream *)mem);
		camel_stream_write((CamelStream *)mem, "", 1);
		truth = regexec(pattern, mem->buffer->data, 0, NULL, 0) == 0;
		camel_object_unref((CamelObject *)mem);
	}
	return truth;
}

