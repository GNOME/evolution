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
#include <camel/camel-string-utils.h>
#include <camel/camel-mime-message.h>
#include <camel/camel-url.h>

#include <gal/util/e-iconv.h>
#include <libgnome/gnome-i18n.h>

#include "em-stripsig-filter.h"
#include "em-format-quote.h"

struct _EMFormatQuotePrivate {
	int dummy;
};

static void emfq_format_clone(EMFormat *, CamelFolder *, const char *, CamelMimeMessage *, EMFormat *);
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
emfq_format_clone(EMFormat *emf, CamelFolder *folder, const char *uid, CamelMimeMessage *msg, EMFormat *src)
{
	EMFormatQuote *emfq = (EMFormatQuote *) emf;
	
	((EMFormatClass *)emfq_parent)->format_clone(emf, folder, uid, msg, src);

	camel_stream_reset(emfq->stream);
	em_format_format_message(emf, emfq->stream, (CamelMedium *)msg);
	camel_stream_flush(emfq->stream);

	g_signal_emit_by_name(emf, "complete");
}

static void
emfq_format_error(EMFormat *emf, CamelStream *stream, const char *txt)
{
	/* FIXME: should we even bother writing error text for quoting? probably not... */
}

static void
emfq_format_text_header (EMFormatQuote *emfq, CamelStream *stream, const char *label, const char *value, guint32 flags, int is_html)
{
	const char *fmt, *html;
	char *mhtml = NULL;
	
	if (value == NULL)
		return;
	
	while (*value == ' ')
		value++;
	
	if (!is_html)
		html = mhtml = camel_text_to_html (value, 0, 0);
	else
		html = value;
	
	if (flags & EM_FORMAT_HEADER_BOLD)
		fmt = "<b>%s</b>: %s<br>";
	else
		fmt = "%s: %s<br>";
	
	camel_stream_printf (stream, fmt, label, html);
	g_free (mhtml);
}

static char *addrspec_hdrs[] = {
	"sender", "from", "reply-to", "to", "cc", "bcc",
	"resent-sender", "resent-from", "resent-reply-to",
	"resent-to", "resent-cc", "resent-bcc", NULL
};

#if 0
/* FIXME: include Sender and Resent-* headers too? */
/* For Translators only: The following strings are used in the header table in the preview pane */
static char *i18n_hdrs[] = {
	N_("From"), N_("Reply-To"), N_("To"), N_("Cc"), N_("Bcc")
};
#endif

static void
emfq_format_address (GString *out, struct _camel_header_address *a)
{
	guint32 flags = CAMEL_MIME_FILTER_TOHTML_CONVERT_SPACES;
	char *name, *mailto, *addr;
	
	while (a) {
		if (a->name)
			name = camel_text_to_html (a->name, flags, 0);
		else
			name = NULL;
		
		switch (a->type) {
		case CAMEL_HEADER_ADDRESS_NAME:
			if (name && *name) {
				char *real, *mailaddr;
				
				g_string_append_printf (out, "%s &lt;", name);
				/* rfc2368 for mailto syntax and url encoding extras */
				if ((real = camel_header_encode_phrase (a->name))) {
					mailaddr = g_strdup_printf ("%s <%s>", real, a->v.addr);
					g_free (real);
					mailto = camel_url_encode (mailaddr, "?=&()");
					g_free (mailaddr);
				} else {
					mailto = camel_url_encode (a->v.addr, "?=&()");
				}
			} else {
				mailto = camel_url_encode (a->v.addr, "?=&()");
			}
			addr = camel_text_to_html (a->v.addr, flags, 0);
			g_string_append_printf (out, "<a href=\"mailto:%s\">%s</a>", mailto, addr);
			g_free (mailto);
			g_free (addr);
			
			if (name && *name)
				g_string_append (out, "&gt;");
			break;
		case CAMEL_HEADER_ADDRESS_GROUP:
			g_string_append_printf (out, "%s: ", name);
			emfq_format_address (out, a->v.members);
			g_string_append_printf (out, ";");
			break;
		default:
			g_warning ("Invalid address type");
			break;
		}
		
		g_free (name);
		
		a = a->next;
		if (a)
			g_string_append (out, ", ");
	}
}

static void
emfq_format_header (EMFormat *emf, CamelStream *stream, CamelMedium *part, const char *namein, guint32 flags, const char *charset)
{
	CamelMimeMessage *msg = (CamelMimeMessage *) part;
	EMFormatQuote *emfq = (EMFormatQuote *) emf;
	char *name, *value = NULL, *p;
	const char *txt, *label;
	int addrspec = 0, i;
	int is_html = FALSE;
	
	name = g_alloca (strlen (namein) + 1);
	strcpy (name, namein);
	camel_strdown (name);
	
	for (i = 0; addrspec_hdrs[i]; i++) {
		if (!strcmp (name, addrspec_hdrs[i])) {
			addrspec = 1;
			break;
		}
	}
	
	if (addrspec) {
		struct _camel_header_address *addrs;
		GString *html;
		
		if (!(txt = camel_medium_get_header (part, name)))
			return;
		
		if (!(addrs = camel_header_address_decode (txt, emf->charset ? emf->charset : emf->default_charset)))
			return;
		
		/* canonicalise the header name... first letter is
		 * capitalised and any letter following a '-' also gets
		 * capitalised */
		p = name;
		*p -= 0x20;
		do {
			p++;
			if (p[-1] == '-' && *p >= 'a' && *p <= 'z')
				*p -= 0x20;
		} while (*p);
		
		label = _(name);
		
		html = g_string_new ("");
		emfq_format_address (html, addrs);
		camel_header_address_unref (addrs);
		txt = value = html->str;
		g_string_free (html, FALSE);
		flags |= EM_FORMAT_HEADER_BOLD;
		is_html = TRUE;
	} else if (!strcmp (name, "subject")) {
		txt = camel_mime_message_get_subject (msg);
		label = _("Subject");
		flags |= EM_FORMAT_HEADER_BOLD;
	} else if (!strcmp (name, "x-evolution-mailer")) { /* pseudo-header */
		if (!(txt = camel_medium_get_header (part, "x-mailer")))
			if (!(txt = camel_medium_get_header (part, "user-agent")))
				return;
		
		txt = value = camel_header_format_ctext (txt, charset);
		
		label = _("Mailer");
		flags |= EM_FORMAT_HEADER_BOLD;
	} else if (!strcmp (name, "date") || !strcmp (name, "resent-date")) {
		if (!(txt = camel_medium_get_header (part, name)))
			return;
		
		if (!strcmp (name, "date"))
			label = _("Date");
		else
			label = "Resent-Date";
		
		flags |= EM_FORMAT_HEADER_BOLD;
	} else {
		txt = camel_medium_get_header (part, name);
		value = camel_header_decode_string (txt, charset);
		txt = value;
		label = namein;
	}
	
	emfq_format_text_header (emfq, stream, label, txt, flags, is_html);
	
	g_free (value);
}

static void
emfq_format_headers (EMFormatQuote *emfq, CamelStream *stream, CamelMedium *part)
{
	EMFormat *emf = (EMFormat *) emfq;
	CamelContentType *ct;
	const char *charset;
	EMFormatHeader *h;
	
	ct = camel_mime_part_get_content_type ((CamelMimePart *) part);
	charset = camel_content_type_param (ct, "charset");
	charset = e_iconv_charset_name (charset);	
	
	/* dump selected headers */
	h = (EMFormatHeader *) emf->header_list.head;
	while (h->next) {
		emfq_format_header (emf, stream, part, h->name, h->flags, charset);
		h = h->next;
	}
}

static void
emfq_format_message(EMFormat *emf, CamelStream *stream, CamelMedium *part)
{
	EMFormatQuote *emfq = (EMFormatQuote *) emf;

	if (emfq->credits)
		camel_stream_printf(stream, "%s<br>\n", emfq->credits);

	if (emfq->flags & EM_FORMAT_QUOTE_CITE)
		camel_stream_printf(stream, "<!--+GtkHTML:<DATA class=\"ClueFlow\" key=\"orig\" value=\"1\">-->\n"
				    "<blockquote type=cite>\n"
				    "<font color=\"#%06x\">\n",
				    emfq->citation_colour & 0xffffff);
	
	if (emfq->flags & EM_FORMAT_QUOTE_HEADERS)
		emfq_format_headers (emfq, stream, part);
	
	em_format_part (emf, stream, (CamelMimePart *) part);
	
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
	if (handle && em_format_is_inline(emf, emf->part_id->str, part, handle)) {
		char *text, *html;

		camel_stream_write_string(stream,
					  "<table border=1 cellspacing=0 cellpadding=0><tr><td><font size=-1>\n");

		/* output some info about it */
		text = em_format_describe_part(part, mime_type);
		html = camel_text_to_html(text, ((EMFormatQuote *)emf)->text_html_flags & CAMEL_MIME_FILTER_TOHTML_CONVERT_URLS, 0);
		camel_stream_write_string(stream, html);
		g_free(html);
		g_free(text);

		camel_stream_write_string(stream, "</font></td></tr></table>");

		handle->handler(emf, stream, part, handle);
	}
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
	CamelMimeFilter *sig_strip;
	CamelContentType *type;
	const char *format;
	guint32 rgb = 0x737373, flags;
	
	flags = emfq->text_html_flags;
	
	/* Check for RFC 2646 flowed text. */
	type = camel_mime_part_get_content_type(part);
	if (camel_content_type_is(type, "text", "plain")
	    && (format = camel_content_type_param(type, "format"))
	    && !g_ascii_strcasecmp(format, "flowed"))
		flags |= CAMEL_MIME_FILTER_TOHTML_FORMAT_FLOWED;
	
	filtered_stream = camel_stream_filter_new_with_stream(stream);
	
	if (emfq->flags != 0) {
		sig_strip = em_stripsig_filter_new ();
		camel_stream_filter_add (filtered_stream, sig_strip);
		camel_object_unref (sig_strip);
	}
	
	html_filter = camel_mime_filter_tohtml_new(flags, rgb);
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
		camel_stream_write_string(stream, "\n<!-- text/richtext -->\n");
	} else {
		camel_stream_write_string(stream, "\n<!-- text/enriched -->\n");
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

static void
emfq_ignore(EMFormat *emf, CamelStream *stream, CamelMimePart *part, EMFormatHandler *info)
{
	/* NOOP */
}

static EMFormatHandler type_builtin_table[] = {
	{ "text/plain",(EMFormatFunc)emfq_text_plain },
	{ "text/enriched",(EMFormatFunc)emfq_text_enriched },
	{ "text/richtext",(EMFormatFunc)emfq_text_enriched },
	{ "text/html",(EMFormatFunc)emfq_text_html },
/*	{ "multipart/related",(EMFormatFunc)emfq_multipart_related },*/
	{ "message/external-body", (EMFormatFunc)emfq_ignore },
	{ "multipart/appledouble", (EMFormatFunc)emfq_ignore },
};

static void
emfq_builtin_init(EMFormatQuoteClass *efhc)
{
	int i;
	
	for (i=0;i<sizeof(type_builtin_table)/sizeof(type_builtin_table[0]);i++)
		em_format_class_add_handler((EMFormatClass *)efhc, &type_builtin_table[i]);
}
