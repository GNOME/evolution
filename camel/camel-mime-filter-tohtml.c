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

#include "camel-url-scanner.h"
#include "camel-mime-filter-tohtml.h"
#include "camel-utf8.h"

/**
 * TODO: convert common text/plain 'markup' to html. eg.:
 *
 * _word_ -> <u>_word_</u>
 * *word* -> <b>*word*</b>
 * /word/ -> <i>/word/</i>
 **/

#define d(x)

#define FOOLISHLY_UNMUNGE_FROM 0

#define CONVERT_WEB_URLS  CAMEL_MIME_FILTER_TOHTML_CONVERT_URLS
#define CONVERT_ADDRSPEC  CAMEL_MIME_FILTER_TOHTML_CONVERT_ADDRESSES

static struct {
	unsigned int mask;
	urlpattern_t pattern;
} patterns[] = {
	{ CONVERT_WEB_URLS, { "file://",   "",        camel_url_file_start,     camel_url_file_end     } },
	{ CONVERT_WEB_URLS, { "ftp://",    "",        camel_url_web_start,      camel_url_web_end      } },
	{ CONVERT_WEB_URLS, { "http://",   "",        camel_url_web_start,      camel_url_web_end      } },
	{ CONVERT_WEB_URLS, { "https://",  "",        camel_url_web_start,      camel_url_web_end      } },
	{ CONVERT_WEB_URLS, { "news://",   "",        camel_url_web_start,      camel_url_web_end      } },
	{ CONVERT_WEB_URLS, { "nntp://",   "",        camel_url_web_start,      camel_url_web_end      } },
	{ CONVERT_WEB_URLS, { "telnet://", "",        camel_url_web_start,      camel_url_web_end      } },
	{ CONVERT_WEB_URLS, { "webcal://", "",        camel_url_web_start,      camel_url_web_end      } },
	{ CONVERT_WEB_URLS, { "callto://", "",        camel_url_web_start,      camel_url_web_end      } },
	{ CONVERT_WEB_URLS, { "h323://",   "",        camel_url_web_start,      camel_url_web_end      } },
	{ CONVERT_WEB_URLS, { "www.",      "http://", camel_url_web_start,      camel_url_web_end      } },
	{ CONVERT_WEB_URLS, { "ftp.",      "ftp://",  camel_url_web_start,      camel_url_web_end      } },
	{ CONVERT_ADDRSPEC, { "@",         "mailto:", camel_url_addrspec_start, camel_url_addrspec_end } },
};

#define NUM_URL_PATTERNS (sizeof (patterns) / sizeof (patterns[0]))

static void camel_mime_filter_tohtml_class_init (CamelMimeFilterToHTMLClass *klass);
static void camel_mime_filter_tohtml_init       (CamelMimeFilterToHTML *filter);
static void camel_mime_filter_tohtml_finalize   (CamelObject *obj);

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
camel_mime_filter_tohtml_finalize (CamelObject *obj)
{
	CamelMimeFilterToHTML *filter = (CamelMimeFilterToHTML *) obj;
	
	camel_url_scanner_free (filter->scanner);
}

static void
camel_mime_filter_tohtml_init (CamelMimeFilterToHTML *filter)
{
	filter->scanner = camel_url_scanner_new ();
	
	filter->flags = 0;
	filter->colour = 0;
	filter->column = 0;
	filter->pre_open = FALSE;
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

static int
citation_depth (const char *in)
{
	register const char *inptr = in;
	int depth = 1;
	
	if (*inptr++ != '>')
		return 0;
	
#if FOOLISHLY_UNMUNGE_FROM
	/* check that it isn't an escaped From line */
	if (!strncmp (inptr, "From", 4))
		return 0;
#endif
	
	while (*inptr != '\n') {
		if (*inptr == ' ')
			inptr++;
		
		if (*inptr++ != '>')
			break;
		
		depth++;
	}
	
	return depth;
}

static char *
writeln (CamelMimeFilter *filter, const char *in, const char *inend, char *outptr, char **outend)
{
	CamelMimeFilterToHTML *html = (CamelMimeFilterToHTML *) filter;
	const char *inptr = in;

	while (inptr < inend) {
		guint32 u;

		outptr = check_size (filter, outptr, outend, 16);

		u = camel_utf8_getc_limit ((const unsigned char **) &inptr, inend);
		switch (u) {
		case 0xffff:
			g_warning("Truncated utf8 buffer");
			return outptr;
		case '<':
			outptr = g_stpcpy (outptr, "&lt;");
			html->column++;
			break;
		case '>':
			outptr = g_stpcpy (outptr, "&gt;");
			html->column++;
			break;
		case '&':
			outptr = g_stpcpy (outptr, "&amp;");
			html->column++;
			break;
		case '"':
			outptr = g_stpcpy (outptr, "&quot;");
			html->column++;
			break;
		case '\t':
			if (html->flags & (CAMEL_MIME_FILTER_TOHTML_CONVERT_SPACES)) {
				do {
					outptr = check_size (filter, outptr, outend, 7);
					outptr = g_stpcpy (outptr, "&nbsp;");
					html->column++;
				} while (html->column % 8);
				break;
			}
			/* otherwise, FALL THROUGH */
		case ' ':
			if (html->flags & CAMEL_MIME_FILTER_TOHTML_CONVERT_SPACES
			    && ((inptr == (in + 1) || *inptr == ' ' || *inptr == '\t'))) {
				outptr = g_stpcpy (outptr, "&nbsp;");
				html->column++;
				break;
			}
			/* otherwise, FALL THROUGH */
		default:
			if (u >= 20 && u <0x80)
				*outptr++ = u;
			else {
				if (html->flags & CAMEL_MIME_FILTER_TOHTML_ESCAPE_8BIT)
					*outptr++ = '?';
				else
					outptr += sprintf(outptr, "&#%u;", u);
			}
			html->column++;
			break;
		}
	}
	
	return outptr;
}

static void
html_convert (CamelMimeFilter *filter, char *in, size_t inlen, size_t prespace,
	      char **out, size_t *outlen, size_t *outprespace, gboolean flush)
{
	CamelMimeFilterToHTML *html = (CamelMimeFilterToHTML *) filter;
	register char *inptr, *outptr;
	char *start, *outend;
	const char *inend;
	int depth;

	if (inlen == 0) {
		if (html->pre_open) {
			/* close the pre-tag */
			outend = filter->outbuf + filter->outsize;
			outptr = check_size (filter, filter->outbuf, &outend, 10);
			outptr = g_stpcpy (outptr, "</pre>");
			html->pre_open = FALSE;

			*out = filter->outbuf;
			*outlen = outptr - filter->outbuf;
			*outprespace = filter->outpre;
		} else {
			*out = in;
			*outlen = 0;
			*outprespace = 0;
		}

		return;
	}
	
	camel_mime_filter_set_size (filter, inlen * 2 + 6, FALSE);
	
	inptr = in;
	inend = in + inlen;
	outptr = filter->outbuf;
	outend = filter->outbuf + filter->outsize;
	
	if (html->flags & CAMEL_MIME_FILTER_TOHTML_PRE && !html->pre_open) {
		outptr = g_stpcpy (outptr, "<pre>");
		html->pre_open = TRUE;
	}

	start = inptr;
	do {
		while (inptr < inend && *inptr != '\n')
			inptr++;

		if (inptr >= inend && !flush)
			break;

		html->column = 0;
		depth = 0;
		
		if (html->flags & CAMEL_MIME_FILTER_TOHTML_MARK_CITATION) {
			if ((depth = citation_depth (start)) > 0) {
				/* FIXME: we could easily support multiple colour depths here */
				
				outptr = check_size (filter, outptr, &outend, 25);
				outptr += sprintf(outptr, "<font color=\"#%06x\">", (html->colour & 0xffffff));
			}
#if FOOLISHLY_UNMUNGE_FROM
			else if (*start == '>') {
				/* >From line */
				start++;
			}
#endif
		} else if (html->flags & CAMEL_MIME_FILTER_TOHTML_CITE) {
			outptr = check_size (filter, outptr, &outend, 6);
			outptr = g_stpcpy (outptr, "&gt; ");
			html->column += 2;
		}
		
#define CONVERT_URLS (CAMEL_MIME_FILTER_TOHTML_CONVERT_URLS | CAMEL_MIME_FILTER_TOHTML_CONVERT_ADDRESSES)
		if (html->flags & CONVERT_URLS) {
			size_t matchlen, buflen, len;
			urlmatch_t match;
			
			len = inptr - start;
			
			do {
				if (camel_url_scanner_scan (html->scanner, start, len, &match)) {
					/* write out anything before the first regex match */
					outptr = writeln (filter, start, start + match.um_so,
							  outptr, &outend);
					
					start += match.um_so;
					len -= match.um_so;
					
					matchlen = match.um_eo - match.um_so;
					
					buflen = 20 + strlen (match.prefix) + matchlen + matchlen;
					outptr = check_size (filter, outptr, &outend, buflen);
					
					/* write out the href tag */
					outptr = g_stpcpy (outptr, "<a href=\"");
					outptr = g_stpcpy (outptr, match.prefix);
					memcpy (outptr, start, matchlen);
					outptr += matchlen;
					outptr = g_stpcpy (outptr, "\">");
					
					/* now write the matched string */
					memcpy (outptr, start, matchlen);
					html->column += matchlen;
					outptr += matchlen;
					start += matchlen;
					len -= matchlen;
					
					/* close the href tag */
					outptr = g_stpcpy (outptr, "</a>");
				} else {
					/* nothing matched so write out the remainder of this line buffer */
					outptr = writeln (filter, start, start + len, outptr, &outend);
					break;
				}
			} while (len > 0);
		} else {
			outptr = writeln (filter, start, inptr, outptr, &outend);
		}
		
		if ((html->flags & CAMEL_MIME_FILTER_TOHTML_MARK_CITATION) && depth > 0) {
			outptr = check_size (filter, outptr, &outend, 8);
			outptr = g_stpcpy (outptr, "</font>");
		}
		
		if (inptr < inend) {
			if (html->flags & CAMEL_MIME_FILTER_TOHTML_CONVERT_NL) {
				outptr = check_size (filter, outptr, &outend, 5);
				outptr = g_stpcpy (outptr, "<br>");
			}
			
			*outptr++ = '\n';
		}
		
		start = ++inptr;
	} while (inptr < inend);
	
	if (flush) {
		/* flush the rest of our input buffer */
		if (start < inend)
			outptr = writeln (filter, start, inend, outptr, &outend);
		
		if (html->pre_open) {
			/* close the pre-tag */
			outptr = check_size (filter, outptr, &outend, 10);
			outptr = g_stpcpy (outptr, "</pre>");
		}
	} else if (start < inend) {
		/* backup */
		camel_mime_filter_backup (filter, start, (unsigned) (inend - start));
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
	int i;
	
	new = CAMEL_MIME_FILTER_TOHTML (camel_object_new (camel_mime_filter_tohtml_get_type ()));
	
	new->flags = flags;
	new->colour = colour;
	
	for (i = 0; i < NUM_URL_PATTERNS; i++) {
		if (patterns[i].mask & flags)
			camel_url_scanner_add (new->scanner, &patterns[i].pattern);
	}
	
	return CAMEL_MIME_FILTER (new);
}


char *
camel_text_to_html (const char *in, guint32 flags, guint32 colour)
{
	CamelMimeFilter *filter;
	size_t outlen, outpre;
	char *outbuf;
	
	g_return_val_if_fail (in != NULL, NULL);
	
	filter = camel_mime_filter_tohtml_new (flags, colour);
	
	camel_mime_filter_complete (filter, (char *) in, strlen (in), 0,
				    &outbuf, &outlen, &outpre);
	
	outbuf = g_strndup (outbuf, outlen);
	
	camel_object_unref (filter);
	
	return outbuf;
}
