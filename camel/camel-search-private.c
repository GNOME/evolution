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

#include <gal/widgets/e-unicode.h>

#include "camel-exception.h"
#include "camel-mime-message.h"
#include "camel-multipart.h"
#include "camel-stream-mem.h"
#include "e-util/e-sexp.h"

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
		return strcasecmp(value, match) == 0;
	case CAMEL_SEARCH_MATCH_CONTAINS:
		return e_utf8_strstrcase(value, match) != NULL;
	case CAMEL_SEARCH_MATCH_STARTS:
		return strncasecmp(value, match, strlen(match)) == 0;
	case CAMEL_SEARCH_MATCH_ENDS:
		return strcasecmp(value+strlen(value)-strlen(match), match) == 0;
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

