/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 *  Authors: Jeffrey Stedfast <fejj@ximian.com>
 *
 *  Copyright 2001 Ximian, Inc. (www.ximian.com)
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

#include <stdio.h>
#include <string.h>
#include <ctype.h>

#include "camel-mime-filter-tohtml.h"

#define d(x)

static void camel_mime_filter_tohtml_class_init (CamelMimeFilterToHTMLClass *klass);
static void camel_mime_filter_tohtml_init       (CamelObject *o);
static void camel_mime_filter_tohtml_finalize   (CamelObject *o);

static CamelMimeFilterClass *camel_mime_filter_tohtml_parent;


CamelType
camel_mime_filter_tohtml_get_type (void)
{
	static CamelType type = CAMEL_INVALID_TYPE;
	
	if (type == CAMEL_INVALID_TYPE) {
		type = camel_type_register (camel_mime_filter_get_type (),
					    "CamelMimeFilterToHTML",
					    sizeof (CamelMimeFilterToHTML),
					    sizeof (CamelMimeFilterToHTMLClass),
					    (CamelObjectClassInitFunc) camel_mime_filter_tohtml_class_init,
					    NULL,
					    (CamelObjectInitFunc) camel_mime_filter_tohtml_init,
					    (CamelObjectFinalizeFunc) camel_mime_filter_tohtml_finalize);
	}
	
	return type;
}

static void
camel_mime_filter_tohtml_finalize (CamelObject *o)
{
	;
}

static void
camel_mime_filter_tohtml_init (CamelObject *o)
{
	;
}


static char *
check_size (CamelMimeFilter *filter, char *outptr, char **outend, size_t len)
{
	size_t offset;
	
	if (*outend - outptr >= len)
		return outptr;
	
	offset = outptr - filter->outbuf;
	
	camel_mime_filter_set_size (filter, filter->outsize + len, TRUE);
	
	*outend = filter->outbuf + filter->outsize;
	
	return filter->outbuf + offset;
}


static unsigned short special_chars[128] = {
	  0,  0,  0,  0,  0,  0,  0,  0,  0,  7,  7,  0,  0,  0,  0,  0,
	  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
	  7,  4,  3,  0,  0,  0,  0,  7,  3,  7,  0,  0,  7, 12, 12,  1,
	  8,  8,  8,  8,  8,  8,  8,  8,  8,  8,  5,  7,  3,  0,  7,  4,
	  1,  8,  8,  8,  8,  8,  8,  8,  8,  8,  8,  8,  8,  8,  8,  8,
	  8,  8,  8,  8,  8,  8,  8,  8,  8,  8,  8,  3,  7,  3,  0,  4,
	  7,  8,  8,  8,  8,  8,  8,  8,  8,  8,  8,  8,  8,  8,  8,  8,
	  8,  8,  8,  8,  8,  8,  8,  8,  8,  8,  8,  0,  7,  4,  0,  0,
};


#define IS_NON_ADDR   (1 << 0)
#define IS_NON_URL    (1 << 1)
#define IS_GARBAGE    (1 << 2)
#define IS_DOMAIN     (1 << 3)

#define NON_EMAIL_CHARS         "()<>@,;:\\\"/[]`'|\n\t "
#define NON_URL_CHARS           "()<>,;\\\"[]`'|\n\t "
#define TRAILING_URL_GARBAGE    ",.!?;:>)}\\`'-_|\n\t "

#define is_addr_char(c) ((unsigned char) (c) < 128 && !(special_chars[(unsigned char) (c)] & IS_NON_ADDR))
#define is_url_char(c)  ((unsigned char) (c) < 128 && !(special_chars[(unsigned char) (c)] & IS_NON_URL))
#define is_trailing_garbage(c) ((unsigned char) (c) > 127 || (special_chars[(unsigned char) (c)] & IS_GARBAGE))
#define is_domain_name_char(c) ((unsigned char) (c) < 128 && (special_chars[(unsigned char) (c)] & IS_DOMAIN))


#if 0
static void
table_init (void)
{
	int max, ch, i;
	char *c;
	
	memset (special_chars, 0, sizeof (special_chars));
	for (c = NON_EMAIL_CHARS; *c; c++)
		special_chars[(int) *c] |= IS_NON_ADDR;
	for (c = NON_URL_CHARS; *c; c++)
		special_chars[(int) *c] |= IS_NON_URL;
	for (c = TRAILING_URL_GARBAGE; *c; c++)
		special_chars[(int) *c] |= IS_GARBAGE;
	
#define is_ascii_alpha(c) (((c) >= 'A' && (c) <= 'Z') || ((c) >= 'a' && (c) <= 'z'))
	
	for (ch = 0; ch < 128; ch++) {
		if (is_ascii_alpha (ch) || isdigit (ch) || ch == '.' || ch == '-')
			special_chars[ch] |= IS_DOMAIN;
	}
	
	max = sizeof (special_chars) / sizeof (special_chars[0]);
	printf ("static unsigned short special_chars[%d] = {", max);
	for (i = 0; i < max; i++) {
		if (i % 16 == 0)
			printf ("\n\t");
		printf ("%3d,", special_chars[i]);
	}
	printf ("\n};\n");
}
#endif

static char *
url_extract (char **in, int inlen, gboolean check, gboolean *backup)
{
	unsigned char *inptr, *inend, *p;
	char *url;
	
	inptr = (unsigned char *) *in;
	inend = inptr + inlen;
	
	while (inptr < inend && is_url_char (*inptr))
		inptr++;
	
	if ((char *) inptr == *in)
		return NULL;
	
	/* back up if we probably went too far. */
	while (inptr > (unsigned char *) *in && is_trailing_garbage (*(inptr - 1)))
		inptr--;
	
	if (check) {
		/* make sure we weren't fooled. */
		p = memchr (*in, ':', (char *) inptr - *in);
		if (!p)
			return NULL;
	}
	
	if (inptr == inend && backup) {
		*backup = TRUE;
		return NULL;
	}
	
	url = g_strndup (*in, (char *) inptr - *in);
	*in = inptr;
	
	return url;
}

static char *
email_address_extract (char **in, char *inend, char *start, char **outptr, gboolean *backup)
{
	char *addr, *pre, *end, *dot;
	
	/* *in points to the '@'. Look backward for a valid local-part */
	pre = *in;
	while (pre - 1 >= start && is_addr_char (*(pre - 1)))
		pre--;
	
	if (pre == *in)
		return NULL;
	
	/* Now look forward for a valid domain part */
	for (end = *in + 1, dot = NULL; end < inend && is_domain_name_char (*end); end++) {
		if (*end == '.' && !dot)
			dot = end;
	}
	
	if (end >= inend && backup) {
		*backup = TRUE;
		*outptr -= (*in - pre);
		*in = pre;
		return NULL;
	}
	
	if (!dot)
		return NULL;
	
	/* Remove trailing garbage */
	while (end > *in && is_trailing_garbage (*(end - 1)))
		end--;
	if (dot > end)
		return NULL;
	
	addr = g_strndup (pre, end - pre);
	*outptr -= (*in - pre);
	*in = end;
	
	return addr;
}

static gboolean
is_citation (char *inptr, char *inend, gboolean saw_citation, gboolean *backup)
{
	if (*inptr != '>')
		return FALSE;
	
	if (inend - inptr >= 6) {
		/* make sure this isn't just mbox From-magling... */
		if (strncmp (inptr, ">From ", 6) != 0)
			return TRUE;
	} else if (backup) {
		/* we don't have enough data to tell, so return */
		*backup = TRUE;
		return saw_citation;
	}
	
	/* if the previous line was a citation, then say this one is too */
	if (saw_citation)
		return TRUE;
	
	/* otherwise it was just an isolated ">From " line */
	return FALSE;
}

static gboolean
is_protocol (char *inptr, char *inend, gboolean *backup)
{
	if (inend - inptr >= 8) {
		if (!strncasecmp (inptr, "http://", 7) ||
		    !strncasecmp (inptr, "https://", 8) ||
		    !strncasecmp (inptr, "ftp://", 6) ||
		    !strncasecmp (inptr, "nntp://", 7) ||
		    !strncasecmp (inptr, "mailto:", 7) ||
		    !strncasecmp (inptr, "news:", 5) ||
		    !strncasecmp (inptr, "file:", 5))
			return TRUE;
	} else if (backup) {
		*backup = TRUE;
		return FALSE;
	}
	
	return FALSE;
}

static void
html_convert (CamelMimeFilter *filter, char *in, size_t inlen, size_t prespace,
	      char **out, size_t *outlen, size_t *outprespace, gboolean flush)
{
	CamelMimeFilterToHTML *html = (CamelMimeFilterToHTML *) filter;
	char *inptr, *inend, *outptr, *outend, *start;
	gboolean backup = FALSE;
	
	camel_mime_filter_set_size (filter, inlen * 2 + 6, FALSE);
	
	inptr = start = in;
	inend = in + inlen;
	outptr = filter->outbuf;
	outend = filter->outbuf + filter->outsize;
	
	if (html->flags & CAMEL_MIME_FILTER_TOHTML_PRE && !html->pre_open) {
		outptr += sprintf (outptr, "%s", "<pre>");
		html->pre_open = TRUE;
	}
	
	while (inptr < inend) {
		unsigned char u;
		
		if (html->flags & CAMEL_MIME_FILTER_TOHTML_MARK_CITATION && html->column == 0) {
			html->saw_citation = is_citation (inptr, inend, html->saw_citation,
							  flush ? &backup : NULL);
			if (backup)
				break;
			
			if (html->saw_citation) {
				if (!html->coloured) {
					char font[25];
					
					g_snprintf (font, 25, "<font color=\"#%06x\">", html->colour);
					
					outptr = check_size (filter, outptr, &outend, 25);
					outptr += sprintf (outptr, "%s", font);
					html->coloured = TRUE;
				}
			} else if (html->coloured) {
				outptr = check_size (filter, outptr, &outend, 10);
				outptr += sprintf (outptr, "%s", "</font>");
				html->coloured = FALSE;
			}
			
			/* display mbox-mangled ">From " as "From " */
			if (*inptr == '>' && !html->saw_citation)
				inptr++;
		} else if (html->flags & CAMEL_MIME_FILTER_TOHTML_CITE && html->column == 0) {
			outptr = check_size (filter, outptr, &outend, 6);
			outptr += sprintf (outptr, "%s", "&gt; ");
		}
		
		if (html->flags & CAMEL_MIME_FILTER_TOHTML_CONVERT_URLS && isalpha ((int) *inptr)) {
			char *refurl = NULL, *dispurl = NULL;
			
			if (is_protocol (inptr, inend, flush ? &backup : NULL)) {
				dispurl = url_extract (&inptr, inend - inptr, TRUE,
						       flush ? &backup : NULL);
				if (backup)
					break;
				
				if (dispurl)
					refurl = g_strdup (dispurl);
			} else {
				if (backup)
					break;
				
				if (!strncasecmp (inptr, "www.", 4) && ((unsigned char) inptr[4]) < 0x80
				    && isalnum ((int) inptr[4])) {
					dispurl = url_extract (&inptr, inend - inptr, FALSE,
							      flush ? &backup : NULL);
					if (backup)
						break;
					
					if (dispurl)
						refurl = g_strdup_printf ("http://%s", dispurl);
				}
			}
			
			if (dispurl) {
				outptr = check_size (filter, outptr, &outend,
						     strlen (refurl) +
						     strlen (dispurl) + 15);
				outptr += sprintf (outptr, "<a href=\"%s\">%s</a>",
						   refurl, dispurl);
				html->column += strlen (dispurl);
				g_free (refurl);
				g_free (dispurl);
			}
			
			if (inptr >= inend)
				break;
		}
		
		if (*inptr == '@' && (html->flags & CAMEL_MIME_FILTER_TOHTML_CONVERT_ADDRESSES)) {
			char *addr, *outaddr;
			
			addr = email_address_extract (&inptr, inend, start, &outptr,
						      flush ? &backup : NULL);
			if (backup)
				break;
			
			if (addr) {
				outaddr = g_strdup_printf ("<a href=\"mailto:%s\">%s</a>",
							   addr, addr);
				outptr = check_size (filter, outptr, &outend, strlen (outaddr));
				outptr += sprintf (outptr, "%s", outaddr);
				html->column += strlen (addr);
				g_free (addr);
				g_free (outaddr);
			}
		}
		
		outptr = check_size (filter, outptr, &outend, 32);
		
		switch ((u = (unsigned char) *inptr++)) {
		case '<':
			outptr += sprintf (outptr, "%s", "&lt;");
			html->column++;
			break;
			
		case '>':
			outptr += sprintf (outptr, "%s", "&gt;");
			html->column++;
			break;
			
		case '&':
			outptr += sprintf (outptr, "%s", "&amp;");
			html->column++;
			break;
			
		case '"':
			outptr += sprintf (outptr, "%s", "&quot;");
			html->column++;
			break;
			
		case '\n':
			if (html->flags & CAMEL_MIME_FILTER_TOHTML_CONVERT_NL)
				outptr += sprintf (outptr, "%s", "<br>");
			
			*outptr++ = '\n';
			start = inptr;
			html->column = 0;
			break;
			
		case '\t':
			if (html->flags & (CAMEL_MIME_FILTER_TOHTML_CONVERT_SPACES)) {
				do {
					outptr = check_size (filter, outptr, &outend, 7);
					outptr += sprintf (outptr, "%s", "&nbsp;");
					html->column++;
				} while (html->column % 8);
				break;
			}
			/* otherwise, FALL THROUGH */
			
		case ' ':
			if (html->flags & CAMEL_MIME_FILTER_TOHTML_CONVERT_SPACES) {
				if (inptr == in || (inptr < inend && (*(inptr + 1) == ' ' ||
								      *(inptr + 1) == '\t' ||
								      *(inptr - 1) == '\n'))) {
					outptr += sprintf (outptr, "%s", "&nbsp;");
					html->column++;
					break;
				}
			}
			/* otherwise, FALL THROUGH */
			
		default:
			if ((u >= 0x20 && u < 0x80) ||
			    (u == '\r' || u == '\t') || html->flags & CAMEL_MIME_FILTER_TOHTML_PRESERVE_8BIT) {
				/* Default case, just copy. */
				*outptr++ = (char) u;
			} else {
				if (html->flags & CAMEL_MIME_FILTER_TOHTML_ESCAPE_8BIT)
					*outptr++ = '?';
				else
					outptr += g_snprintf (outptr, 9, "&#%d;", (int) u);
			}
			html->column++;
			break;
		}
	}
	
	if (inptr < inend)
		camel_mime_filter_backup (filter, inptr, inend - inptr);
	
	if (flush && html->pre_open) {
		outptr = check_size (filter, outptr, &outend, 10);
		outptr += sprintf (outptr, "%s", "</pre>");
		html->pre_open = FALSE;
	}
	
	*out = filter->outbuf;
	*outlen = outptr - filter->outbuf;
	*outprespace = filter->outpre;
}

static void
filter_filter (CamelMimeFilter *filter, char *in, size_t len, size_t prespace,
	       char **out, size_t *outlen, size_t *outprespace)
{
	html_convert (filter, in, len, prespace, out, outlen, outprespace, FALSE);
}

static void 
filter_complete (CamelMimeFilter *filter, char *in, size_t len, size_t prespace,
		 char **out, size_t *outlen, size_t *outprespace)
{
	html_convert (filter, in, len, prespace, out, outlen, outprespace, TRUE);
}

static void
filter_reset (CamelMimeFilter *filter)
{
	CamelMimeFilterToHTML *html = (CamelMimeFilterToHTML *) filter;
	
	html->column = 0;
	html->pre_open = FALSE;
	html->saw_citation = FALSE;
	html->coloured = FALSE;
}

static void
camel_mime_filter_tohtml_class_init (CamelMimeFilterToHTMLClass *klass)
{
	CamelMimeFilterClass *filter_class = (CamelMimeFilterClass *) klass;
	
	camel_mime_filter_tohtml_parent = CAMEL_MIME_FILTER_CLASS (camel_type_get_global_classfuncs (camel_mime_filter_get_type ()));
	
	filter_class->reset = filter_reset;
	filter_class->filter = filter_filter;
	filter_class->complete = filter_complete;
}


/**
 * camel_mime_filter_tohtml_new:
 * @flags:
 * @colour:
 *
 * Creates a new CamelMimeFilterToHTML object.
 *
 * Returns a new CamelMimeFilter object.
 **/
CamelMimeFilter *
camel_mime_filter_tohtml_new (guint32 flags, guint32 colour)
{
	CamelMimeFilterToHTML *new;
	
	new = CAMEL_MIME_FILTER_TOHTML (camel_object_new (camel_mime_filter_tohtml_get_type ()));
	
	new->flags = flags;
	new->colour = colour;
	
	return CAMEL_MIME_FILTER (new);
}
