/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 *  Authors: Jeffrey Stedfast <fejj@ximian.com>
 *
 *  Copyright 2002 Ximian, Inc. (www.ximian.com)
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

#include <camel/camel-string-utils.h>

#include "camel-mime-filter-enriched.h"

/* text/enriched is rfc1896 */

typedef char * (*EnrichedParamParser) (const char *inptr, int inlen);

static char *param_parse_colour (const char *inptr, int inlen);
static char *param_parse_font (const char *inptr, int inlen);
static char *param_parse_lang (const char *inptr, int inlen);

static struct {
	char *enriched;
	char *html;
	gboolean needs_param;
	EnrichedParamParser parse_param; /* parses *and* validates the input */
} enriched_tags[] = {
	{ "bold",        "<b>",                 FALSE, NULL               },
	{ "/bold",       "</b>",                FALSE, NULL               },
	{ "italic",      "<i>",                 FALSE, NULL               },
	{ "/italic",     "</i>",                FALSE, NULL               },
	{ "fixed",       "<tt>",                FALSE, NULL               },
	{ "/fixed",      "</tt>",               FALSE, NULL               },
	{ "smaller",     "<font size=-1>",      FALSE, NULL               },
	{ "/smaller",    "</font>",             FALSE, NULL               },
	{ "bigger",      "<font size=+1>",      FALSE, NULL               },
	{ "/bigger",     "</font>",             FALSE, NULL               },
	{ "underline",   "<u>",                 FALSE, NULL               },
	{ "/underline",  "</u>",                FALSE, NULL               },
	{ "center",      "<p align=center>",    FALSE, NULL               },
	{ "/center",     "</p>",                FALSE, NULL               },
	{ "flushleft",   "<p align=left>",      FALSE, NULL               },
	{ "/flushleft",  "</p>",                FALSE, NULL               },
	{ "flushright",  "<p align=right>",     FALSE, NULL               },
	{ "/flushright", "</p>",                FALSE, NULL               },
	{ "excerpt",     "<blockquote>",        FALSE, NULL               },
	{ "/excerpt",    "</blockquote>",       FALSE, NULL               },
	{ "paragraph",   "<p>",                 FALSE, NULL               },
	{ "signature",   "<address>",           FALSE, NULL               },
	{ "/signature",  "</address>",          FALSE, NULL               },
	{ "comment",     "<!-- ",               FALSE, NULL               },
	{ "/comment",    " -->",                FALSE, NULL               },
	{ "np",          "<hr>",                FALSE, NULL               },
	{ "fontfamily",  "<font face=\"%s\">",  TRUE,  param_parse_font   },
	{ "/fontfamily", "</font>",             FALSE, NULL               },
	{ "color",       "<font color=\"%s\">", TRUE,  param_parse_colour },
	{ "/color",      "</font>",             FALSE, NULL               },
	{ "lang",        "<span lang=\"%s\">",  TRUE,  param_parse_lang   },
	{ "/lang",       "</span>",             FALSE, NULL               },
	
	/* don't handle this tag yet... */
	{ "paraindent",  "<!-- ",               /* TRUE */ FALSE, NULL    },
	{ "/paraindent", " -->",                FALSE, NULL               },
	
	/* as soon as we support all the tags that can have a param
	 * tag argument, these should be unnecessary, but we'll keep
	 * them anyway just in case? */
	{ "param",       "<!-- ",               FALSE, NULL               },
	{ "/param",      " -->",                FALSE, NULL               },
};

#define NUM_ENRICHED_TAGS (sizeof (enriched_tags) / sizeof (enriched_tags[0]))

static GHashTable *enriched_hash = NULL;


static void camel_mime_filter_enriched_class_init (CamelMimeFilterEnrichedClass *klass);
static void camel_mime_filter_enriched_init       (CamelMimeFilterEnriched *filter);
static void camel_mime_filter_enriched_finalize   (CamelObject *obj);

static void filter_filter (CamelMimeFilter *filter, char *in, size_t len, size_t prespace,
			   char **out, size_t *outlen, size_t *outprespace);
static void filter_complete (CamelMimeFilter *filter, char *in, size_t len, size_t prespace,
			     char **out, size_t *outlen, size_t *outprespace);
static void filter_reset (CamelMimeFilter *filter);


static CamelMimeFilterClass *parent_class = NULL;


CamelType
camel_mime_filter_enriched_get_type (void)
{
	static CamelType type = CAMEL_INVALID_TYPE;
	
	if (type == CAMEL_INVALID_TYPE) {
		type = camel_type_register (camel_mime_filter_get_type (),
					    "CamelMimeFilterEnriched",
					    sizeof (CamelMimeFilterEnriched),
					    sizeof (CamelMimeFilterEnrichedClass),
					    (CamelObjectClassInitFunc) camel_mime_filter_enriched_class_init,
					    NULL,
					    (CamelObjectInitFunc) camel_mime_filter_enriched_init,
					    (CamelObjectFinalizeFunc) camel_mime_filter_enriched_finalize);
	}
	
	return type;
}

static void
camel_mime_filter_enriched_class_init (CamelMimeFilterEnrichedClass *klass)
{
	CamelMimeFilterClass *filter_class = (CamelMimeFilterClass *) klass;
	int i;
	
	parent_class = CAMEL_MIME_FILTER_CLASS (camel_mime_filter_get_type ());
	
	filter_class->reset = filter_reset;
	filter_class->filter = filter_filter;
	filter_class->complete = filter_complete;
	
	if (!enriched_hash) {
		enriched_hash = g_hash_table_new (camel_strcase_hash, camel_strcase_equal);
		for (i = 0; i < NUM_ENRICHED_TAGS; i++)
			g_hash_table_insert (enriched_hash, enriched_tags[i].enriched,
					     enriched_tags[i].html);
	}
}

static void
camel_mime_filter_enriched_finalize (CamelObject *obj)
{
	;
}

static void
camel_mime_filter_enriched_init (CamelMimeFilterEnriched *filter)
{
	filter->flags = 0;
	filter->nofill = 0;
}


#if 0
static gboolean
enriched_tag_needs_param (const char *tag)
{
	int i;
	
	for (i = 0; i < NUM_ENRICHED_TAGS; i++)
		if (!g_ascii_strcasecmp (tag, enriched_tags[i].enriched))
			return enriched_tags[i].needs_param;
	
	return FALSE;
}
#endif

static gboolean
html_tag_needs_param (const char *tag)
{
	return strstr (tag, "%s") != NULL;
}

static const char *valid_colours[] = {
	"red", "green", "blue", "yellow", "cyan", "magenta", "black", "white"
};

#define NUM_VALID_COLOURS  (sizeof (valid_colours) / sizeof (valid_colours[0]))

static char *
param_parse_colour (const char *inptr, int inlen)
{
	const char *inend, *end;
	guint32 rgb = 0;
	guint v;
	int i;
	
	for (i = 0; i < NUM_VALID_COLOURS; i++) {
		if (!strncasecmp (inptr, valid_colours[i], inlen))
			return g_strdup (valid_colours[i]);
	}
	
	/* check for numeric r/g/b in the format: ####,####,#### */
	if (inptr[4] != ',' || inptr[9] != ',') {
		/* okay, mailer must have used a string name that
		 * rfc1896 did not specify? do some simple scanning
		 * action, a colour name MUST be [a-zA-Z] */
		end = inptr;
		inend = inptr + inlen;
		while (end < inend && ((*end >= 'a' && *end <= 'z') || (*end >= 'A' && *end <= 'Z')))
			end++;
		
		return g_strndup (inptr, end - inptr);
	}
	
	for (i = 0; i < 3; i++) {
		v = strtoul (inptr, (char **) &end, 16);
		if (end != inptr + 4)
			goto invalid_format;
		
		v >>= 8;
		rgb = (rgb << 8) | (v & 0xff);
		
		inptr += 5;
	}
	
	return g_strdup_printf ("#%.6X", rgb);
	
 invalid_format:
	
	/* default colour? */
	return g_strdup ("black");
}

static char *
param_parse_font (const char *fontfamily, int inlen)
{
	register const char *inptr = fontfamily;
	const char *inend = inptr + inlen;
	
	/* don't allow any of '"', '<', nor '>' */
	while (inptr < inend && *inptr != '"' && *inptr != '<' && *inptr != '>')
		inptr++;
	
	return g_strndup (fontfamily, inptr - fontfamily);
}

static char *
param_parse_lang (const char *lang, int inlen)
{
	register const char *inptr = lang;
	const char *inend = inptr + inlen;
	
	/* don't allow any of '"', '<', nor '>' */
	while (inptr < inend && *inptr != '"' && *inptr != '<' && *inptr != '>')
		inptr++;
	
	return g_strndup (lang, inptr - lang);
}

static char *
param_parse (const char *enriched, const char *inptr, int inlen)
{
	int i;
	
	for (i = 0; i < NUM_ENRICHED_TAGS; i++) {
		if (!g_ascii_strcasecmp (enriched, enriched_tags[i].enriched))
			return enriched_tags[i].parse_param (inptr, inlen);
	}
	
	g_assert_not_reached ();
	
	return NULL;
}

#define IS_RICHTEXT CAMEL_MIME_FILTER_ENRICHED_IS_RICHTEXT

static void
enriched_to_html (CamelMimeFilter *filter, char *in, size_t inlen, size_t prespace,
		  char **out, size_t *outlen, size_t *outprespace, gboolean flush)
{
	CamelMimeFilterEnriched *enriched = (CamelMimeFilterEnriched *) filter;
	const char *tag, *inend, *outend;
	register const char *inptr;
	register char *outptr;
	
	camel_mime_filter_set_size (filter, inlen * 2 + 6, FALSE);
	
	inptr = in;
	inend = in + inlen;
	outptr = filter->outbuf;
	outend = filter->outbuf + filter->outsize;
	
 retry:
	do {
		while (inptr < inend && outptr < outend && !strchr (" <>&\n", *inptr))
			*outptr++ = *inptr++;
		
		if (outptr == outend)
			goto backup;
		
		if ((inptr + 1) >= inend)
			break;
		
		switch (*inptr++) {
		case ' ':
			while (inptr < inend && (outptr + 7) < outend && *inptr == ' ') {
				memcpy (outptr, "&nbsp;", 6);
				outptr += 6;
				inptr++;
			}
			
			if (outptr < outend)
				*outptr++ = ' ';
			
			break;
		case '\n':
			if (!(enriched->flags & IS_RICHTEXT)) {
				/* text/enriched */
				if (enriched->nofill > 0) {
					if ((outptr + 4) < outend) {
						memcpy (outptr, "<br>", 4);
						outptr += 4;
					} else {
						inptr--;
						goto backup;
					}
				} else if (*inptr == '\n') {
					if ((outptr + 4) >= outend) {
						inptr--;
						goto backup;
					}
					
					while (inptr < inend && (outptr + 4) < outend && *inptr == '\n') {
						memcpy (outptr, "<br>", 4);
						outptr += 4;
						inptr++;
					}
				} else {
					*outptr++ = ' ';
				}
			} else {
				/* text/richtext */
				*outptr++ = ' ';
			}
			break;
		case '>':
			if ((outptr + 4) < outend) {
				memcpy (outptr, "&gt;", 4);
				outptr += 4;
			} else {
				inptr--;
				goto backup;
			}
			break;
		case '&':
			if ((outptr + 5) < outend) {
				memcpy (outptr, "&amp;", 5);
				outptr += 5;
			} else {
				inptr--;
				goto backup;
			}
			break;
		case '<':
			if (!(enriched->flags & IS_RICHTEXT)) {
				/* text/enriched */
				if (*inptr == '<') {
					if ((outptr + 4) < outend) {
						memcpy (outptr, "&lt;", 4);
						outptr += 4;
						inptr++;
						break;
					} else {
						inptr--;
						goto backup;
					}
				}
			} else {
				/* text/richtext */
				if ((inend - inptr) >= 3 && (outptr + 4) < outend) {
					if (strncmp (inptr, "lt>", 3) == 0) {
						memcpy (outptr, "&lt;", 4);
						outptr += 4;
						inptr += 3;
						break;
					} else if (strncmp (inptr, "nl>", 3) == 0) {
						memcpy (outptr, "<br>", 4);
						outptr += 4;
						inptr += 3;
						break;
					}
				} else {
					inptr--;
					goto backup;
				}
			}
			
			tag = inptr;
			while (inptr < inend && *inptr != '>')
				inptr++;
			
			if (inptr == inend) {
				inptr = tag - 1;
				goto need_input;
			}
			
			if (!strncasecmp (tag, "nofill>", 7)) {
				if ((outptr + 5) < outend) {
					enriched->nofill++;
				} else {
					inptr = tag - 1;
					goto backup;
				}
			} else if (!strncasecmp (tag, "/nofill>", 8)) {
				if ((outptr + 6) < outend) {
					enriched->nofill--;
				} else {
					inptr = tag - 1;
					goto backup;
				}
			} else {
				const char *html_tag;
				char *enriched_tag;
				int len;
				
				len = inptr - tag;
				enriched_tag = g_alloca (len + 1);
				memcpy (enriched_tag, tag, len);
				enriched_tag[len] = '\0';
				
				html_tag = g_hash_table_lookup (enriched_hash, enriched_tag);
				
				if (html_tag) {
					if (html_tag_needs_param (html_tag)) {
						const char *start;
						char *param;
						
						while (inptr < inend && *inptr != '<')
							inptr++;
						
						if (inptr == inend || (inend - inptr) <= 15) {
							inptr = tag - 1;
							goto need_input;
						}
						
						if (strncasecmp (inptr, "<param>", 7) != 0) {
							/* ignore the enriched command tag... */
							inptr -= 1;
							goto loop;
						}
						
						inptr += 7;
						start = inptr;
						
						while (inptr < inend && *inptr != '<')
							inptr++;
						
						if (inptr == inend || (inend - inptr) <= 8) {
							inptr = tag - 1;
							goto need_input;
						}
						
						if (strncasecmp (inptr, "</param>", 8) != 0) {
							/* ignore the enriched command tag... */
							inptr += 7;
							goto loop;
						}
						
						len = inptr - start;
						param = param_parse (enriched_tag, start, len);
						len = strlen (param);
						
						inptr += 7;
						
						len += strlen (html_tag);
						
						if ((outptr + len) < outend) {
							outptr += snprintf (outptr, len, html_tag, param);
							g_free (param);
						} else {
							g_free (param);
							inptr = tag - 1;
							goto backup;
						}
					} else {
						len = strlen (html_tag);
						if ((outptr + len) < outend) {
							memcpy (outptr, html_tag, len);
							outptr += len;
						} else {
							inptr = tag - 1;
							goto backup;
						}
					}
				}
			}
			
		loop:
			inptr++;
			break;
		default:
			break;
		}
	} while (inptr < inend);
	
 need_input:
	
	/* the reason we ignore @flush here is because if there isn't
           enough input to parse a tag, then there's nothing we can
           do. */
	
	if (inptr < inend)
		camel_mime_filter_backup (filter, inptr, (unsigned) (inend - inptr));
	
	*out = filter->outbuf;
	*outlen = outptr - filter->outbuf;
	*outprespace = filter->outpre;
	
	return;
	
 backup:
	
	if (flush) {
		size_t offset, grow;
		
		grow = (inend - inptr) * 2 + 20;
		offset = outptr - filter->outbuf;
		camel_mime_filter_set_size (filter, filter->outsize + grow, TRUE);
		outend = filter->outbuf + filter->outsize;
		outptr = filter->outbuf + offset;
		
		goto retry;
	} else {
		camel_mime_filter_backup (filter, inptr, (unsigned) (inend - inptr));
	}
	
	*out = filter->outbuf;
	*outlen = outptr - filter->outbuf;
	*outprespace = filter->outpre;
}

static void
filter_filter (CamelMimeFilter *filter, char *in, size_t len, size_t prespace,
	       char **out, size_t *outlen, size_t *outprespace)
{
	enriched_to_html (filter, in, len, prespace, out, outlen, outprespace, FALSE);
}

static void 
filter_complete (CamelMimeFilter *filter, char *in, size_t len, size_t prespace,
		 char **out, size_t *outlen, size_t *outprespace)
{
	enriched_to_html (filter, in, len, prespace, out, outlen, outprespace, TRUE);
}

static void
filter_reset (CamelMimeFilter *filter)
{
	CamelMimeFilterEnriched *enriched = (CamelMimeFilterEnriched *) filter;
	
	enriched->nofill = 0;
}


/**
 * camel_mime_filter_enriched_new:
 * @flags:
 *
 * Creates a new CamelMimeFilterEnriched object.
 *
 * Returns a new CamelMimeFilter object.
 **/
CamelMimeFilter *
camel_mime_filter_enriched_new (guint32 flags)
{
	CamelMimeFilterEnriched *new;
	
	new = (CamelMimeFilterEnriched *) camel_object_new (CAMEL_TYPE_MIME_FILTER_ENRICHED);
	new->flags = flags;
	
	return CAMEL_MIME_FILTER (new);
}

char *
camel_enriched_to_html(const char *in, guint32 flags)
{
	CamelMimeFilter *filter;
	size_t outlen, outpre;
	char *outbuf;

	if (in == NULL)
		return NULL;
	
	filter = camel_mime_filter_enriched_new(flags);
	
	camel_mime_filter_complete(filter, (char *)in, strlen(in), 0, &outbuf, &outlen, &outpre);
	outbuf = g_strndup (outbuf, outlen);
	camel_object_unref (filter);
	
	return outbuf;
}
