/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 *  Authors: Jeffrey Stedfast <fejj@ximian.com>
 *
 *  Copyright 2003 Ximian, Inc. (www.ximian.com)
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
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

#include <string.h>

#include <camel/camel-stream.h>
#include <camel/camel-stream-filter.h>
#include <camel/camel-mime-filter-tohtml.h>
#include <camel/camel-mime-filter-enriched.h>
#include "em-format-quote.h"

struct _EMFormatQuotePrivate {
	int dummy;
};

static void emfq_format_clone(EMFormat *, CamelMedium *, EMFormat *);
static void emfq_format_error(EMFormat *emf, CamelStream *stream, const char *txt);
static void emfq_format_message(EMFormat *, CamelStream *, CamelMedium *);
static void emfq_format_source(EMFormat *, CamelStream *, CamelMimePart *);
static void emfq_format_attachment(EMFormat *, CamelStream *, CamelMimePart *, const char *, const EMFormatHandler *);

static void emfq_builtin_init(EMFormatQuoteClass *efhc);

static EMFormatClass *emfq_parent;

static void
emfq_init(GObject *o)
{
	EMFormatQuote *emfq =(EMFormatQuote *) o;
	
	emfq->priv = g_malloc0(sizeof(*emfq->priv));
	
	/* we want to convert url's etc */
	emfq->text_html_flags = CAMEL_MIME_FILTER_TOHTML_PRE | CAMEL_MIME_FILTER_TOHTML_CONVERT_URLS
		| CAMEL_MIME_FILTER_TOHTML_CONVERT_ADDRESSES;
}

static void
emfq_finalise(GObject *o)
{
	EMFormatQuote *emfq =(EMFormatQuote *) o;

	if (emfq->stream)
		camel_object_unref(emfq->stream);
	g_free(emfq->credits);
	g_free(emfq->priv);
	
	((GObjectClass *) emfq_parent)->finalize(o);
}

static void
emfq_base_init(EMFormatQuoteClass *emfqklass)
{
	emfq_builtin_init(emfqklass);
}

static void
emfq_class_init(GObjectClass *klass)
{
	((EMFormatClass *) klass)->format_clone = emfq_format_clone;
	((EMFormatClass *) klass)->format_error = emfq_format_error;
	((EMFormatClass *) klass)->format_message = emfq_format_message;
	((EMFormatClass *) klass)->format_source = emfq_format_source;
	((EMFormatClass *) klass)->format_attachment = emfq_format_attachment;
	
	klass->finalize = emfq_finalise;
}

GType
em_format_quote_get_type(void)
{
	static GType type = 0;
	
	if (type == 0) {
		static const GTypeInfo info = {
			sizeof(EMFormatQuoteClass),
			(GBaseInitFunc)emfq_base_init, NULL,
			(GClassInitFunc)emfq_class_init,
			NULL, NULL,
			sizeof(EMFormatQuote), 0,
			(GInstanceInitFunc) emfq_init
		};
		
		emfq_parent = g_type_class_ref(em_format_get_type());
		type = g_type_register_static(em_format_get_type(), "EMFormatQuote", &info, 0);
	}
	
	return type;
}

EMFormatQuote *
em_format_quote_new(const char *credits, CamelStream *stream, guint32 flags)
{
	EMFormatQuote *emfq;
	
	emfq = (EMFormatQuote *)g_object_new(em_format_quote_get_type(), NULL);

	emfq->credits = g_strdup(credits);
	emfq->stream = stream;
	camel_object_ref(stream);
	emfq->flags = flags;
	
	return emfq;
}

static void
emfq_format_clone(EMFormat *emf, CamelMedium *part, EMFormat *src)
{
#define emfq ((EMFormatQuote *)emf)

	((EMFormatClass *)emfq_parent)->format_clone(emf, part, src);

	camel_stream_reset(emfq->stream);
	em_format_format_message(emf, emfq->stream, part);
	camel_stream_flush(emfq->stream);

	g_signal_emit_by_name(emf, "complete");
#undef emfq
}

static void
emfq_format_error(EMFormat *emf, CamelStream *stream, const char *txt)
{
	/* FIXME: should we even bother writign error text for quoting? probably not... */
}

static void
emfq_format_message(EMFormat *emf, CamelStream *stream, CamelMedium *part)
{
	EMFormatQuote *emfq =(EMFormatQuote *) emf;
	
	if (emfq->credits)
		camel_stream_printf(stream, "%s", emfq->credits);


	if (emfq->flags & EM_FORMAT_QUOTE_CITE)
		camel_stream_printf(stream, "<!--+GtkHTML:<DATA class=\"ClueFlow\" key=\"orig\" value=\"1\">-->\n"
				    "<blockquote type=cite>\n"
				    "<font color=\"#%06x\">\n",
				    emfq->citation_colour & 0xffffff);

	if (emfq->flags & EM_FORMAT_QUOTE_HEADERS) {
		camel_stream_printf(stream, "<b>To: </b> Header goes here<br>");
	}

	em_format_part(emf, stream, (CamelMimePart *)part);

	if (emfq->flags & EM_FORMAT_QUOTE_CITE)
		camel_stream_write_string(stream, "</blockquote></font><!--+GtkHTML:<DATA class=\"ClueFlow\" clear=\"orig\">-->");
}

static void
emfq_format_source(EMFormat *emf, CamelStream *stream, CamelMimePart *part)
{
	CamelStreamFilter *filtered_stream;
	CamelMimeFilter *html_filter;
	CamelDataWrapper *dw = (CamelDataWrapper *)part;

	filtered_stream = camel_stream_filter_new_with_stream ((CamelStream *) stream);
	html_filter = camel_mime_filter_tohtml_new (CAMEL_MIME_FILTER_TOHTML_CONVERT_NL
						    | CAMEL_MIME_FILTER_TOHTML_CONVERT_SPACES
						    | CAMEL_MIME_FILTER_TOHTML_ESCAPE_8BIT, 0);
	camel_stream_filter_add(filtered_stream, html_filter);
	camel_object_unref(html_filter);
	
	em_format_format_text(emf, (CamelStream *)filtered_stream, dw);
	camel_object_unref(filtered_stream);
}

static void
emfq_format_attachment(EMFormat *emf, CamelStream *stream, CamelMimePart *part, const char *mime_type, const EMFormatHandler *handle)
{
	;
}

#include <camel/camel-medium.h>
#include <camel/camel-mime-part.h>
#include <camel/camel-multipart.h>
#include <camel/camel-url.h>

static void
emfq_text_plain(EMFormatQuote *emfq, CamelStream *stream, CamelMimePart *part, EMFormatHandler *info)
{
	CamelStreamFilter *filtered_stream;
	CamelMimeFilter *html_filter;
	CamelContentType *type;
	const char *format;
	guint32 rgb = 0x737373, flags;

	flags = emfq->text_html_flags;
	
	/* Check for RFC 2646 flowed text. */
	type = camel_mime_part_get_content_type(part);
	if (camel_content_type_is (type, "text", "plain")
	    && (format = camel_content_type_param (type, "format"))
	    && !g_ascii_strcasecmp(format, "flowed"))
		flags |= CAMEL_MIME_FILTER_TOHTML_FORMAT_FLOWED;
	
	html_filter = camel_mime_filter_tohtml_new(flags, rgb);
	filtered_stream = camel_stream_filter_new_with_stream(stream);
	camel_stream_filter_add(filtered_stream, html_filter);
	camel_object_unref(html_filter);
	
	em_format_format_text((EMFormat *)emfq, (CamelStream *)filtered_stream, camel_medium_get_content_object((CamelMedium *)part));
	camel_stream_flush((CamelStream *)filtered_stream);
	camel_object_unref(filtered_stream);
}

static void
emfq_text_enriched(EMFormatQuote *emfq, CamelStream *stream, CamelMimePart *part, EMFormatHandler *info)
{
	CamelStreamFilter *filtered_stream;
	CamelMimeFilter *enriched;
	CamelDataWrapper *dw;
	guint32 flags = 0;
	
	dw = camel_medium_get_content_object((CamelMedium *)part);
	
	if (!strcmp(info->mime_type, "text/richtext")) {
		flags = CAMEL_MIME_FILTER_ENRICHED_IS_RICHTEXT;
		camel_stream_write_string( stream, "\n<!-- text/richtext -->\n");
	} else {
		camel_stream_write_string( stream, "\n<!-- text/enriched -->\n");
	}
	
	enriched = camel_mime_filter_enriched_new(flags);
	filtered_stream = camel_stream_filter_new_with_stream (stream);
	camel_stream_filter_add(filtered_stream, enriched);
	camel_object_unref(enriched);

	camel_stream_write_string(stream, "<br><hr><br>");
	em_format_format_text((EMFormat *)emfq, (CamelStream *)filtered_stream, dw);
	camel_object_unref(filtered_stream);
}

static void
emfq_text_html(EMFormat *emf, CamelStream *stream, CamelMimePart *part, EMFormatHandler *info)
{
	camel_stream_write_string(stream, "\n<!-- text/html -->\n");
	em_format_format_text(emf, stream, camel_medium_get_content_object((CamelMedium *)part));
}

static const char *type_remove_table[] = {
	"message/external-body",
	"multipart/appledouble",
};

static EMFormatHandler type_builtin_table[] = {
	{ "text/plain",(EMFormatFunc)emfq_text_plain },
	{ "text/enriched",(EMFormatFunc)emfq_text_enriched },
	{ "text/richtext",(EMFormatFunc)emfq_text_enriched },
	{ "text/html",(EMFormatFunc)emfq_text_html },
/*	{ "multipart/related",(EMFormatFunc)emfq_multipart_related },*/
};

static void
emfq_builtin_init(EMFormatQuoteClass *efhc)
{
	int i;
	
	for (i = 0; i < sizeof(type_remove_table) / sizeof(type_remove_table[0]); i++)
		em_format_class_remove_handler((EMFormatClass *) efhc, type_remove_table[i]);

	for (i=0;i<sizeof(type_builtin_table)/sizeof(type_builtin_table[0]);i++)
		em_format_class_add_handler((EMFormatClass *)efhc, &type_builtin_table[i]);
}
