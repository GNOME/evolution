/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 *  Authors: Jeffrey Stedfast <fejj@helixcode.com>
 *
 *  Copyright 2000 Helix Code, Inc. (www.helixcode.com)
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

#include <ctype.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#include <gtk/gtk.h>
#include "camel-imap-utils.h"
#include "string-utils.h"
#include <e-sexp.h>
#include "camel/camel-folder-summary.h"

#define d(x) x

char *
imap_next_word (const char *buf)
{
	char *word;
	
	/* skip over current word */
	for (word = (char *)buf; *word && *word != ' '; word++);
	
	/* skip over white space */
	for ( ; *word && *word == ' '; word++);
	
	return word;
}

/**
 * imap_parse_list_response:
 * @buf: the LIST or LSUB response
 * @flags: a pointer to a variable to store the flags in, or %NULL
 * @sep: a pointer to a variable to store the hierarchy separator in, or %NULL
 * @folder: a pointer to a variable to store the folder name in, or %NULL
 *
 * Parses a LIST or LSUB response and returns the desired parts of it.
 * If @folder is non-%NULL, its value must be freed by the caller.
 *
 * Return value: whether or not the response was successfully parsed.
 **/
gboolean
imap_parse_list_response (const char *buf, int *flags, char *sep, char **folder)
{
	char *word;
	int len;

	if (*buf != '*')
		return FALSE;

	word = imap_next_word (buf);
	if (g_strncasecmp (word, "LIST", 4) && g_strncasecmp (word, "LSUB", 4))
		return FALSE;

	/* get the flags */
	word = imap_next_word (word);
	if (*word != '(')
		return FALSE;

	if (flags)
		*flags = 0;

	word++;
	while (*word != ')') {
		len = strcspn (word, " )");
		if (flags) {
			if (!g_strncasecmp (word, "\\Noinferiors", len))
				*flags |= IMAP_LIST_FLAG_NOINFERIORS;
			else if (!g_strncasecmp (word, "\\Noselect", len))
				*flags |= IMAP_LIST_FLAG_NOSELECT;
			else if (!g_strncasecmp (word, "\\Marked", len))
				*flags |= IMAP_LIST_FLAG_MARKED;
			else if (!g_strncasecmp (word, "\\Unmarked", len))
				*flags |= IMAP_LIST_FLAG_UNMARKED;
		}

		word += len;
		while (*word == ' ')
			word++;
	}

	/* get the directory separator */
	word = imap_next_word (word);
	if (!strncmp (word, "NIL", 3)) {
		if (sep)
			*sep = '\0';
	} else if (*word++ == '"') {
		if (*word == '\\')
			word++;
		if (sep)
			*sep = *word;
		word++;
		if (*word++ != '"')
			return FALSE;
	} else
		return FALSE;

	if (folder) {
		/* get the folder name */
		word = imap_next_word (word);
		*folder = imap_parse_astring (&word, &len);
		return *folder != NULL;
	}

	return TRUE;
}

char *
imap_create_flag_list (guint32 flags)
{
	GString *gstr;
	char *flag_list;
	
	gstr = g_string_new ("(");
	
	if (flags & CAMEL_MESSAGE_ANSWERED)
		g_string_append (gstr, "\\Answered ");
	if (flags & CAMEL_MESSAGE_DELETED)
		g_string_append (gstr, "\\Deleted ");
	if (flags & CAMEL_MESSAGE_DRAFT)
		g_string_append (gstr, "\\Draft ");
	if (flags & CAMEL_MESSAGE_FLAGGED)
		g_string_append (gstr, "\\Flagged ");
	if (flags & CAMEL_MESSAGE_SEEN)
		g_string_append (gstr, "\\Seen ");
	
	if (gstr->str[gstr->len - 1] == ' ')
		gstr->str[gstr->len - 1] = ')';
	else
		g_string_append_c (gstr, ')');
	
	flag_list = gstr->str;
	g_string_free (gstr, FALSE);
	return flag_list;
}

guint32
imap_parse_flag_list (char **flag_list_p)
{
	char *flag_list = *flag_list_p;
	guint32 flags = 0;
	int len;
	
	if (*flag_list++ != '(') {
		*flag_list_p = NULL;
		return 0;
	}
	
	while (*flag_list && *flag_list != ')') {
		len = strcspn (flag_list, " )");
		if (!g_strncasecmp (flag_list, "\\Answered", len))
			flags |= CAMEL_MESSAGE_ANSWERED;
		else if (!g_strncasecmp (flag_list, "\\Deleted", len))
			flags |= CAMEL_MESSAGE_DELETED;
		else if (!g_strncasecmp (flag_list, "\\Draft", len))
			flags |= CAMEL_MESSAGE_DRAFT;
		else if (!g_strncasecmp (flag_list, "\\Flagged", len))
			flags |= CAMEL_MESSAGE_FLAGGED;
		else if (!g_strncasecmp (flag_list, "\\Seen", len))
			flags |= CAMEL_MESSAGE_SEEN;
		
		flag_list += len;
		if (*flag_list == ' ')
			flag_list++;
	}

	if (*flag_list++ != ')') {
		*flag_list_p = NULL;
		return 0;
	}

	*flag_list_p = flag_list;
	return flags;
}

static char imap_atom_specials[128] = {
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	1, 0, 1, 0, 0, 1, 0, 0, 1, 1, 1, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0,
};
#define imap_is_atom_char(ch) (isascii (ch) && !imap_atom_specials[ch])

/**
 * imap_parse_string_generic:
 * @str_p: a pointer to a string
 * @len: a pointer to an int to return the length in
 * @type: type of string (#IMAP_STRING, #IMAP_ASTRING, or #IMAP_NSTRING)
 * to parse.
 *
 * This parses an IMAP "string" (quoted string or literal), "nstring"
 * (NIL or string), or "astring" (atom or string) starting at *@str_p.
 * On success, *@str_p will point to the first character after the end
 * of the string, and *@len will contain the length of the returned
 * string. On failure, *@str_p will be set to %NULL.
 *
 * This assumes that the string is in the form returned by
 * camel_imap_command(): that line breaks are indicated by LF rather
 * than CRLF.
 *
 * Return value: the parsed string, or %NULL if a NIL or no string
 * was parsed. (In the former case, *@str_p will be %NULL; in the
 * latter, it will point to the character after the NIL.)
 **/
char *
imap_parse_string_generic (char **str_p, int *len, int type)
{
	char *str = *str_p;
	char *out;

	if (!str)
		return NULL;
	else if (*str == '"') {
		char *p;
		int size;

		str++;
		size = strcspn (str, "\"") + 1;
		p = out = g_malloc (size);

		while (*str && *str != '"') {
			if (*str == '\\')
				str++;
			*p++ = *str++;
			if (p - out == size) {
				out = g_realloc (out, size * 2);
				p = out + size;
				size *= 2;
			}
		}
		if (*str != '"') {
			*str_p = NULL;
			g_free (out);
			return NULL;
		}
		*p = '\0';
		*str_p = str + 1;
		*len = strlen (out);
		return out;
	} else if (*str == '{') {
		*len = strtoul (str + 1, (char **)&str, 10);
		if (*str++ != '}' || *str++ != '\n' || strlen (str) < *len) {
			*str_p = NULL;
			return NULL;
		}
		
		out = g_strndup (str, *len);
		*str_p = str + *len;
		return out;
	} else if (type == IMAP_NSTRING && !g_strncasecmp (str, "nil", 3)) {
		*str_p += 3;
		*len = 0;
		return NULL;
	} else if (type == IMAP_ASTRING &&
		   imap_is_atom_char ((unsigned char)*str)) {
		while (imap_is_atom_char ((unsigned char)*str))
			str++;

		*len = str - *str_p;
		str = g_strndup (*str_p, *len);
		*str_p += *len;
		return str;
	} else {
		*str_p = NULL;
		return NULL;
	}
}

/**
 * imap_quote_string:
 * @str: the string to quote, which must not contain CR or LF
 *
 * Return value: an IMAP "quoted" corresponding to the string, which
 * the caller must free.
 **/
char *
imap_quote_string (const char *str)
{
	const char *p;
	char *quoted, *q;
	int len;

	len = strlen (str);
	p = str;
	while ((p = strpbrk (p, "\"\\"))) {
		len++;
		p++;
	}

	quoted = q = g_malloc (len + 3);
	*q++ = '"';
	while ((p = strpbrk (str, "\"\\"))) {
		memcpy (q, str, p - str);
		q += p - str;
		*q++ = '\\';
		*q++ = *p++;
		str = p;
	}
	sprintf (q, "%s\"", str);

	return quoted;
}
