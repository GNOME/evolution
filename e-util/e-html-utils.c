/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* e-html-utils.c
 * Copyright (C) 2000  Helix Code, Inc.
 * Author: Dan Winship <danw@helixcode.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#include <ctype.h>
#include <stdio.h>
#include <string.h>
#include <glib.h>
#include <unicode.h>

#include "e-html-utils.h"

static char *
check_size (char **buffer, int *buffer_size, char *out, int len)
{
	if (out + len > *buffer + *buffer_size) {
		int index = out - *buffer;

		*buffer_size *= 2;
		*buffer = g_realloc (*buffer, *buffer_size);
		out = *buffer + index;
	}
	return out;
}

static char *
url_extract (const unsigned char **text, gboolean check)
{
	const unsigned char *end = *text, *p;
	char *out;

	while (*end && !isspace (*end) && (*end != '"') && (*end < 0x80))
		end++;

	/* Back up if we probably went too far. */
	while (end > *text && strchr (",.!?;:>)]}", *(end - 1)))
		end--;

	if (check) {
		/* Make sure we weren't fooled. */
		p = memchr (*text, ':', end - *text);
		if (!p || end - p < 4)
			return NULL;
	}

	out = g_strndup (*text, end - *text);
	*text = end;
	return out;
}

/* FIXME -- this should be smarter */
static gboolean
is_email_address (const unsigned char *c)
{
	gboolean seen_at = FALSE, seen_postat = FALSE;

	if (c == NULL)
		return FALSE;

	if (*c == '<')
		++c;

	while (*c && (isalnum ((gint) *c)
		      || *c == '-'
		      || *c == '_'
		      || *c == (seen_at ? '.' : '@'))) {
		
		if (seen_at && !seen_postat) {
			if (*c == '.')
				return FALSE;
			seen_postat = TRUE;
		}

		if (*c == '@')
			seen_at = TRUE;

		++c;
	}

	return seen_at && seen_postat && (isspace ((gint) *c) || *c == '>' || !*c);
}

static gchar *
email_address_extract (const unsigned char **text)
{
	const unsigned char *end = *text;
	char *out;

	if (end == NULL)
		return NULL;

	while (*end && !isspace (*end) && (*end != '>') && (*end < 0x80))
		++end;

	out = g_strndup (*text, end - *text);
	if (!is_email_address (out)) {
		g_free (out);
		return NULL;
	}

	*text = end;
	return out;
}

static gboolean
is_citation (const unsigned char *c, gboolean saw_citation)
{
	unicode_char_t u;
	gint i;

	/* A line that starts with a ">" is a citation, unless it's
	 * just mbox From-mangling...
	 */
	if (*c == '>') {
		const unsigned char *p;

		if (strncmp (c, ">From ", 6) != 0)
			return TRUE;

		/* If the previous line was a citation, then say this
		 * one is too.
		 */
		if (saw_citation)
			return TRUE;

		/* Same if the next line is */
		p = (const unsigned char *)strchr ((const char *)c, '\n');
		if (p && *++p == '>')
			return TRUE;

		/* Otherwise, it was just an isolated ">From" line. */
		return FALSE;
	}

	/* Check for "Rupert> " and the like... */
	for (i = 0; c && *c && *c != '\n' && i < 10; i ++, c = unicode_next_utf8 (c)) {
		unicode_get_utf8 (c, &u);
		if (u == '>')
			return TRUE;
		if (!unicode_isalnum (u))
			return FALSE;
	}
	return FALSE;
}

/**
 * e_text_to_html_full:
 * @input: a NUL-terminated input buffer
 * @flags: some combination of the E_TEXT_TO_HTML_* flags defined
 * in e-html-utils.h
 * @color: color for citation highlighting
 *
 * This takes a buffer of text as input and produces a buffer of
 * "equivalent" HTML, subject to certain transformation rules.
 *
 * The set of possible flags is:
 *
 *   - E_TEXT_TO_HTML_PRE: wrap the output HTML in <PRE> and </PRE>.
 *     Should only be used if @input is the entire buffer to be
 *     converted. If e_text_to_html is being called with small pieces
 *     of data, you should wrap the entire result in <PRE> yourself.
 *
 *   - E_TEXT_TO_HTML_CONVERT_NL: convert "\n" to "<BR>\n" on output.
 *     (should not be used with E_TEXT_TO_HTML_PRE, since that would
 *     result in double-newlines).
 *
 *   - E_TEXT_TO_HTML_CONVERT_SPACES: convert a block of N spaces
 *     into N-1 non-breaking spaces and one normal space. A space
 *     at the start of the buffer is always converted to a
 *     non-breaking space, regardless of the following character,
 *     which probably means you don't want to use this flag on
 *     pieces of data that aren't delimited by at least line breaks.
 *
 *     If E_TEXT_TO_HTML_CONVERT_NL and E_TEXT_TO_HTML_CONVERT_SPACES
 *     are both defined, then TABs will also be converted to spaces.
 *
 *   - E_TEXT_TO_HTML_CONVERT_URLS: wrap <a href="..."> </a> around
 *     strings that look like URLs.
 *
 *   - E_TEXT_TO_HTML_CONVERT_ADDRESSES: wrap <a href="mailto:..."> </a> around
 *     strings that look like mail addresses.
 *
 *   - E_TEXT_TO_HTML_MARK_CITATION: wrap <font color="..."> </font> around
 *     citations (lines beginning with "> ", etc).
 **/
char *
e_text_to_html_full (const char *input, unsigned int flags, guint32 color)
{
	const unsigned char *cur = input;
	char *buffer = NULL;
	char *out = NULL;
	int buffer_size = 0, col;
	gboolean colored = FALSE, saw_citation = FALSE;

	/* Allocate a translation buffer.  */
	buffer_size = strlen (input) * 2 + 5;
	buffer = g_malloc (buffer_size);

	out = buffer;
	if (flags & E_TEXT_TO_HTML_PRE)
		out += sprintf (out, "<PRE>\n");

	col = 0;

	for (cur = input; cur && *cur; cur = unicode_next_utf8 (cur)) {
		unicode_char_t u;

		if (flags & E_TEXT_TO_HTML_MARK_CITATION && col == 0) {
			saw_citation = is_citation (cur, saw_citation);
			if (saw_citation) {
				if (!colored) {
					gchar font [25];

					g_snprintf (font, 25, "<FONT COLOR=\"#%06x\">", color);

					check_size (&buffer, &buffer_size, out, 25);
					out += sprintf (out, "%s", font);
					colored = TRUE;
				}
			} else if (colored) {
				gchar *no_font = "</FONT>";

				check_size (&buffer, &buffer_size, out, 9);
				out += sprintf (out, "%s", no_font);
				colored = FALSE;
			}

			/* Display mbox-mangled ">From" as "From" */
			if (*cur == '>' && !saw_citation)
				cur++;
		}

		unicode_get_utf8 (cur, &u);
		if (unicode_isalpha (u) &&
		    (flags & E_TEXT_TO_HTML_CONVERT_URLS)) {
			char *tmpurl = NULL, *refurl = NULL, *dispurl = NULL;

			if (!strncasecmp (cur, "http://", 7) ||
			    !strncasecmp (cur, "https://", 8) ||
			    !strncasecmp (cur, "ftp://", 6) ||
			    !strncasecmp (cur, "nntp://", 7) ||
			    !strncasecmp (cur, "mailto:", 7) ||
			    !strncasecmp (cur, "news:", 5)) {
				tmpurl = url_extract (&cur, TRUE);
				if (tmpurl) {
					refurl = e_text_to_html (tmpurl, 0);
					dispurl = g_strdup (refurl);
				}
			} else if (!strncasecmp (cur, "www.", 4) &&
				   (*(cur + 4) < 0x80) &&
				   unicode_isalnum (*(cur + 4))) {
				tmpurl = url_extract (&cur, FALSE);
				dispurl = e_text_to_html (tmpurl, 0);
				refurl = g_strdup_printf ("http://%s",
							  dispurl);
			}

			if (tmpurl) {
				out = check_size (&buffer, &buffer_size, out,
						  strlen (refurl) +
						  strlen (dispurl) + 15);
				out += sprintf (out,
						"<a href=\"%s\">%s</a>",
						refurl, dispurl);
				col += strlen (tmpurl);
				g_free (tmpurl);
				g_free (refurl);
				g_free (dispurl);
			}

			if (!*cur)
				break;
			unicode_get_utf8 (cur, &u);
		}

		if (unicode_isalpha (u)
		    && (flags & E_TEXT_TO_HTML_CONVERT_ADDRESSES)
		    && is_email_address (cur)) {
			gchar *addr = NULL, *dispaddr = NULL;

			addr = email_address_extract (&cur);
			dispaddr = e_text_to_html (addr, 0);
			
			if (addr) {
				gchar *outaddr = g_strdup_printf ("<a href=\"mailto:%s\">%s</a>",
								  addr, dispaddr);
				out = check_size (&buffer, &buffer_size, out, strlen(outaddr));
				out += sprintf (out, "%s", outaddr);
				col += strlen (addr);
				g_free (addr);
				g_free (dispaddr);
				g_free (outaddr);
			}

			if (!*cur)
				break;
			unicode_get_utf8 (cur, &u);
			
		}

		if (u == (unicode_char_t)-1) {
			/* Sigh. Someone sent undeclared 8-bit data.
			 * Assume it's iso-8859-1.
			 */
			u = *cur;
		}

		out = check_size (&buffer, &buffer_size, out, 10);

		switch (u) {
		case '<':
			strcpy (out, "&lt;");
			out += 4;
			col++;
			break;

		case '>':
			strcpy (out, "&gt;");
			out += 4;
			col++;
			break;

		case '&':
			strcpy (out, "&amp;");
			out += 5;
			col++;
			break;

		case '"':
			strcpy (out, "&quot;");
			out += 6;
			col++;
			break;

		case '\n':
			if (flags & E_TEXT_TO_HTML_CONVERT_NL) {
				strcpy (out, "<br>");
				out += 4;
			}
			*out++ = *cur;
			col = 0;
			break;

		case '\t':
			if (flags & (E_TEXT_TO_HTML_CONVERT_SPACES |
				     E_TEXT_TO_HTML_CONVERT_NL)) {
				do {
					out = check_size (&buffer, &buffer_size,
						    out, 7);
					strcpy (out, "&nbsp;");
					out += 6;
					col++;
				} while (col % 8);
				break;
			}
			/* otherwise, FALL THROUGH */

		case ' ':
			if (flags & E_TEXT_TO_HTML_CONVERT_SPACES) {
				if (cur == (const unsigned char *)input ||
				    *(cur + 1) == ' ' || *(cur + 1) == '\t') {
					strcpy (out, "&nbsp;");
					out += 6;
					col++;
					break;
				}
			}
			/* otherwise, FALL THROUGH */

		default:
			if ((u >= 0x20 && u < 0x80) ||
			    (u == '\r' || u == '\t')) {
				/* Default case, just copy. */
				*out++ = u;
			} else {
				out += g_snprintf(out, 9, "&#%d;", u);
			}
			col++;
			break;
		}
	}

	out = check_size (&buffer, &buffer_size, out, 7);
	if (flags & E_TEXT_TO_HTML_PRE)
		strcpy (out, "</PRE>");
	else
		*out = '\0';

	return buffer;
}

char *
e_text_to_html (const char *input, unsigned int flags)
{
	return e_text_to_html_full (input, flags, 0);
}
