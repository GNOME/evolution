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

#include "string-utils.h"

#include "camel-mime-filter-enriched.h"

static struct {
	char *enriched;
	char *html;
} enriched_tags[] = {
	{ "bold",        "<b>"              },
	{ "/bold",       "</b>"             },
	{ "italic",      "<i>"              },
	{ "/italic",     "</i>"             },
	{ "fixed",       "<tt>"             },
	{ "/fixed",      "</tt>"            },
	{ "smaller",     "<font size=-1>"   },
	{ "/smaller",    "</font>"          },
	{ "bigger",      "<font size=+1>"   },
	{ "/bigger",     "</font>"          },
	{ "underline",   "<u>"              },
	{ "/underline",  "</u>"             },
	{ "center",      "<p align=center>" },
	{ "/center",     "</p>"             },
	{ "flushleft",   "<p align=left>"   },
	{ "/flushleft",  "</p>"             },
	{ "flushright",  "<p align=right>"  },
	{ "/flushright", "</p>"             },
	{ "excerpt",     "<blockquote>"     },
	{ "/excerpt",    "</blockquote>"    },
	{ "paragraph",   "<p>"              },
	{ "signature",   "<address>"        },
	{ "/signature",  "</address>"       },
	{ "comment",     "<!-- "            },
	{ "/comment",    " -->"             },
	{ "param",       "<!-- "            },
	{ "/param",      " -->"             },
	{ "np",          "<hr>"             }
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
		enriched_hash = g_hash_table_new (g_strcase_hash, g_strcase_equal);
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
	
 loop:
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
			if (!(enriched->flags & IS_RICHTEXT) && enriched->nofill <= 0) {
				/* text/enriched */
				while (inptr < inend && (outptr + 4) < outend && *inptr == '\n') {
					memcpy (outptr, "<br>", 4);
					outptr += 4;
					inptr++;
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
			
			if (!strncmp (tag, "nofill>", 7)) {
				if ((outptr + 5) < outend) {
					memcpy (outptr, "<pre>", 5);
					enriched->nofill++;
					outptr += 5;
				} else {
					inptr = tag - 1;
					goto backup;
				}
			} else if (!strncmp (tag, "/nofill>", 8)) {
				if ((outptr + 6) < outend) {
					memcpy (outptr, "</pre>", 6);
					enriched->nofill--;
					outptr += 6;
				} else {
					inptr = tag - 1;
					goto backup;
				}
			} else {
				const char *html_tag;
				char *enriched_tag;
				int len;
				
				enriched_tag = g_strndup (tag, (inptr - tag));
				html_tag = g_hash_table_lookup (enriched_hash, enriched_tag);
				g_free (enriched_tag);
				if (html_tag) {
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
		
		goto loop;
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
	int i;
	
	new = (CamelMimeFilterEnriched *) camel_object_new (CAMEL_TYPE_MIME_FILTER_ENRICHED);
	new->flags = flags;
	
	return CAMEL_MIME_FILTER (new);
}
