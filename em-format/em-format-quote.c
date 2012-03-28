/*
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) version 3.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with the program; if not, see <http://www.gnu.org/licenses/>
 *
 *
 * Authors:
 *		Jeffrey Stedfast <fejj@ximian.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <string.h>

#include <glib/gi18n.h>

#include "em-inline-filter.h"
#include "em-stripsig-filter.h"
#include "em-format-quote.h"

#define EM_FORMAT_QUOTE_GET_PRIVATE(obj) \
	(G_TYPE_INSTANCE_GET_PRIVATE \
	((obj), EM_TYPE_FORMAT_QUOTE, EMFormatQuotePrivate))

struct _EMFormatQuotePrivate {
	gchar *credits;
	EMFormatQuoteFlags flags;
	guint32 text_html_flags;
};

static void emfq_builtin_init (EMFormatQuoteClass *efhc);

static CamelMimePart * decode_inline_parts (CamelMimePart *part, GCancellable *cancellable);

static void emfq_parse_text_plain       (EMFormat *emf, CamelMimePart *part, GString *part_id, EMFormatParserInfo *info, GCancellable *cancellable);
static void emfq_parse_text_enriched    (EMFormat *emf, CamelMimePart *part, GString *part_id, EMFormatParserInfo *info, GCancellable *cancellable);
static void emfq_parse_text_html        (EMFormat *emf, CamelMimePart *part, GString *part_id, EMFormatParserInfo *info, GCancellable *cancellable);
static void emfq_parse_attachment       (EMFormat *emf, CamelMimePart *part, GString *part_id, EMFormatParserInfo *info, GCancellable *cancellable);

static void emfq_write_text_plain	(EMFormat *emf, EMFormatPURI *puri, CamelStream *stream, EMFormatWriterInfo *info, GCancellable *cancellable);
static void emfq_write_text_enriched	(EMFormat *emf, EMFormatPURI *puri, CamelStream *stream, EMFormatWriterInfo *info, GCancellable *cancellable);
static void emfq_write_text_html	(EMFormat *emf, EMFormatPURI *puri, CamelStream *stream, EMFormatWriterInfo *info, GCancellable *cancellable);

static gpointer parent_class;

/* Decodes inline encoded parts of 'part'. The returned pointer,
 * if not NULL, should be unreffed with g_object_unref(). */
static CamelMimePart *
decode_inline_parts (CamelMimePart *part,
                     GCancellable *cancellable)
{
	CamelMultipart *mp;
	CamelStream *null;
	CamelStream *filtered_stream;
	EMInlineFilter *inline_filter;

	g_return_val_if_fail (part != NULL, NULL);

	null = camel_stream_null_new ();
	filtered_stream = camel_stream_filter_new (null);
	g_object_unref (null);

	inline_filter = em_inline_filter_new (
		camel_mime_part_get_encoding (part),
		camel_mime_part_get_content_type (part));
	camel_stream_filter_add (
		CAMEL_STREAM_FILTER (filtered_stream),
		CAMEL_MIME_FILTER (inline_filter));
	camel_data_wrapper_decode_to_stream_sync (
		camel_medium_get_content (CAMEL_MEDIUM (part)),
		filtered_stream, cancellable, NULL);
	camel_stream_close (filtered_stream, cancellable, NULL);
	g_object_unref (filtered_stream);

	if (!em_inline_filter_found_any (inline_filter)) {
		g_object_unref (inline_filter);
		return NULL;
	}

	mp = em_inline_filter_get_multipart (inline_filter);

	g_object_unref (inline_filter);

	if (mp) {
		part = camel_mime_part_new ();
		camel_medium_set_content (
			CAMEL_MEDIUM (part), CAMEL_DATA_WRAPPER (mp));
		g_object_unref (mp);
	} else {
		g_object_ref (part);
	}

	return part;
}

static void
emfq_format_text_header (EMFormatQuote *emfq,
                         GString *buffer,
                         const gchar *label,
                         const gchar *value,
                         guint32 flags,
                         gint is_html)
{
	const gchar *html;
	gchar *mhtml = NULL;

	if (value == NULL)
		return;

	while (*value == ' ')
		value++;

	if (!is_html)
		html = mhtml = camel_text_to_html (value, 0, 0);
	else
		html = value;

	if (flags & EM_FORMAT_HEADER_BOLD)
		g_string_append_printf (
                        buffer, "<b>%s</b>: %s<br>", label, html);
	else
		g_string_append_printf (
                        buffer, "%s: %s<br>", label, html);

	g_free (mhtml);
}

static const gchar *addrspec_hdrs[] = {
        "Sender", "From", "Reply-To", "To", "Cc", "Bcc",
        "Resent-Sender", "Resent-from", "Resent-Reply-To",
        "Resent-To", "Resent-cc", "Resent-Bcc", NULL
};

#if 0
/* FIXME: include Sender and Resent-* headers too? */
/* For Translators only: The following strings are
 * used in the header table in the preview pane. */
static gchar *i18n_hdrs[] = {
        N_("From"), N_("Reply-To"), N_("To"), N_("Cc"), N_("Bcc")
};
#endif

static void
emfq_format_address (GString *out,
                     struct _camel_header_address *a)
{
	guint32 flags = CAMEL_MIME_FILTER_TOHTML_CONVERT_SPACES;
	gchar *name, *mailto, *addr;

	while (a) {
		if (a->name)
			name = camel_text_to_html (a->name, flags, 0);
		else
			name = NULL;

		switch (a->type) {
		case CAMEL_HEADER_ADDRESS_NAME:
			if (name && *name) {
				gchar *real, *mailaddr;

                                g_string_append_printf (out, "%s &lt;", name);
                                /* rfc2368 for mailto syntax and url encoding extras */
				if ((real = camel_header_encode_phrase ((guchar *) a->name))) {
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
			g_string_append_printf (
				out, "<a href=\"mailto:%s\">%s</a>",
				mailto, addr);
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
canon_header_name (gchar *name)
{
	gchar *inptr = name;

        /* canonicalise the header name... first letter is
         * capitalised and any letter following a '-' also gets
         * capitalised */

	if (g_ascii_islower (*inptr))
                *inptr = g_ascii_toupper (*inptr);

	inptr++;

	while (*inptr) {
		if (inptr[-1] == '-' && g_ascii_islower (*inptr))
                        *inptr = g_ascii_toupper (*inptr);
		else if (g_ascii_isupper (*inptr))
                        *inptr = g_ascii_tolower (*inptr);

		inptr++;
	}
}

static void
emfq_format_header (EMFormat *emf,
                    GString *buffer,
                    CamelMedium *part,
                    const gchar *namein,
                    guint32 flags,
                    const gchar *charset)
{
	CamelMimeMessage *msg = (CamelMimeMessage *) part;
	EMFormatQuote *emfq = (EMFormatQuote *) emf;
	gchar *name, *buf, *value = NULL;
	const gchar *txt, *label;
	gboolean addrspec = FALSE;
	gint is_html = FALSE;
	gint i;

	name = g_alloca (strlen (namein) + 1);
	strcpy (name, namein);
	canon_header_name (name);

        /* Never quote Bcc headers */
        if (g_str_equal (name, "Bcc") || g_str_equal (name, "Resent-Bcc"))
		return;

	for (i = 0; addrspec_hdrs[i]; i++) {
		if (!strcmp (name, addrspec_hdrs[i])) {
			addrspec = TRUE;
			break;
		}
	}

	label = _(name);

	if (addrspec) {
		struct _camel_header_address *addrs;
		GString *html;

		if (!(txt = camel_medium_get_header (part, name)))
			return;

		buf = camel_header_unfold (txt);
		addrs = camel_header_address_decode (
			txt, em_format_get_charset (emf) ?
			em_format_get_charset (emf) : em_format_get_default_charset (emf));
		if (addrs == NULL) {
			g_free (buf);
			return;
		}

		g_free (buf);

                html = g_string_new ("");
		emfq_format_address (html, addrs);
		camel_header_address_unref (addrs);
		txt = value = html->str;
		g_string_free (html, FALSE);
		flags |= EM_FORMAT_HEADER_BOLD;
		is_html = TRUE;
        } else if (!strcmp (name, "Subject")) {
		txt = camel_mime_message_get_subject (msg);
                label = _("Subject");
		flags |= EM_FORMAT_HEADER_BOLD;
        } else if (!strcmp (name, "X-Evolution-Mailer")) { /* pseudo-header */
                if (!(txt = camel_medium_get_header (part, "x-mailer")))
                        if (!(txt = camel_medium_get_header (part, "user-agent")))
                                if (!(txt = camel_medium_get_header (part, "x-newsreader")))
                                        if (!(txt = camel_medium_get_header (part, "x-mimeole")))
						return;

		txt = value = camel_header_format_ctext (txt, charset);

                label = _("Mailer");
		flags |= EM_FORMAT_HEADER_BOLD;
        } else if (!strcmp (name, "Date") || !strcmp (name, "Resent-Date")) {
		if (!(txt = camel_medium_get_header (part, name)))
			return;

		flags |= EM_FORMAT_HEADER_BOLD;
	} else {
		txt = camel_medium_get_header (part, name);
		buf = camel_header_unfold (txt);
		txt = value = camel_header_decode_string (txt, charset);
		g_free (buf);
	}

	emfq_format_text_header (emfq, buffer, label, txt, flags, is_html);

	g_free (value);
}

static void
emfq_format_headers (EMFormatQuote *emfq,
                     GString *buffer,
                     CamelMedium *part)
{
	EMFormat *emf = (EMFormat *) emfq;
	CamelContentType *ct;
	const gchar *charset;
	GList *link;

	if (!part)
		return;

	ct = camel_mime_part_get_content_type ((CamelMimePart *) part);
        charset = camel_content_type_param (ct, "charset");
	charset = camel_iconv_charset_name (charset);

        /* dump selected headers */
	link = g_queue_peek_head_link (&emf->header_list);
	while (link != NULL) {
		EMFormatHeader *h = link->data;
		emfq_format_header (
			emf, buffer, part, h->name, h->flags, charset);
		link = g_list_next (link);
	}

        g_string_append (buffer, "<br>\n");
}

static void
emfq_dispose (GObject *object)
{
	/* Chain up to parent's dispose() method. */
	G_OBJECT_CLASS (parent_class)->dispose (object);
}

static void
emfq_finalize (GObject *object)
{
	EMFormatQuotePrivate *priv;

	priv = EM_FORMAT_QUOTE_GET_PRIVATE (object);

	g_free (priv->credits);

	/* Chain up to parent's finalize() method. */
	G_OBJECT_CLASS (parent_class)->finalize (object);
}

/******************************************************************************/
static void
emfq_parse_text_plain (EMFormat * emf,
                       CamelMimePart * part,
                       GString * part_id,
                       EMFormatParserInfo * info,
                       GCancellable * cancellable)
{
	EMFormatPURI *puri;
		CamelMimePart *mp;
	gint len;

	len = part_id->len;
        g_string_append (part_id, ".text_plain");

		mp = decode_inline_parts (part, cancellable);
		if (mp) {

			if (CAMEL_IS_MULTIPART (camel_medium_get_content (CAMEL_MEDIUM (mp)))) {
				em_format_parse_part (emf, mp, part_id, info, cancellable);
			}

			g_object_unref (mp);
		}

	puri = em_format_puri_new (emf, sizeof (EMFormatPURI), part, part_id->str);
	puri->write_func = emfq_write_text_plain;
        puri->mime_type = g_strdup ("text/html");
	em_format_add_puri (emf, puri);

	g_string_truncate (part_id, len);
}

static void
emfq_parse_text_html (EMFormat * emf,
                      CamelMimePart * part,
                      GString * part_id,
                      EMFormatParserInfo * info,
                      GCancellable * cancellable)
{
	EMFormatPURI *puri;
	gint len;

	len = part_id->len;
        g_string_append (part_id, ".text_html");

	puri = em_format_puri_new (emf, sizeof (EMFormatPURI), part, part_id->str);
	puri->write_func = emfq_write_text_html;
        puri->mime_type = g_strdup ("text/html");
	em_format_add_puri (emf, puri);

	g_string_truncate (part_id, len);
}

static void
emfq_parse_text_enriched (EMFormat * emf,
                          CamelMimePart * part,
                          GString * part_id,
                          EMFormatParserInfo * info,
                          GCancellable * cancellable)
{
	EMFormatPURI *puri;
	gint len;

	len = part_id->len;
        g_string_append (part_id, ".text_enriched");

	puri = em_format_puri_new (emf, sizeof (EMFormatPURI), part, part_id->str);
	puri->write_func = emfq_write_text_enriched;
        puri->mime_type = g_strdup ("text/html");
	em_format_add_puri (emf, puri);

	g_string_truncate (part_id, len);
}

static void
emfq_parse_attachment (EMFormat * emf,
                       CamelMimePart * part,
                       GString * part_id,
                       EMFormatParserInfo * info,
                       GCancellable * cancellable)
{
	EMFormatPURI *puri;
	gint len;

	len = part_id->len;
        g_string_append (part_id, ".attachment");

	puri = em_format_puri_new (emf, sizeof (EMFormatPURI), part, part_id->str);
	puri->write_func = emfq_write_text_html;
        puri->mime_type = g_strdup ("text/html");
	puri->is_attachment = TRUE;
	em_format_add_puri (emf, puri);

	g_string_truncate (part_id, len);
}

/******************************************************************************/

static void
emfq_write_attachment (EMFormat *emf,
                       EMFormatPURI *puri,
                       CamelStream *stream,
                       EMFormatWriterInfo *info,
                       GCancellable *cancellable)
{
	EMFormatQuote *emfq = EM_FORMAT_QUOTE (emf);
	const EMFormatHandler *handler;
	gchar *text, *html;
	CamelContentType *ct;
	const gchar *mime_type;

	ct = camel_mime_part_get_content_type (puri->part);
	if (ct) {
		mime_type = camel_content_type_simple (ct);
		camel_content_type_unref (ct);
	} else {
		mime_type = "application/octet-stream";
	}

	handler = em_format_find_handler (emf, mime_type);

	if (!em_format_is_inline (emf, puri->uri, puri->part, handler))
		return;

	camel_stream_write_string (
		stream, "<table border=1 cellspacing=0 cellpadding=0>"
		"<tr><td><font size=-1>\n", cancellable, NULL);

	/* output some info about it */
	text = em_format_describe_part (puri->part, mime_type);
	html = camel_text_to_html (
		text, emfq->priv->text_html_flags &
		CAMEL_MIME_FILTER_TOHTML_CONVERT_URLS, 0);
	camel_stream_write_string (stream, html, cancellable, NULL);
	g_free (html);
	g_free (text);

	camel_stream_write_string (
		stream, "</font></td></tr></table>", cancellable, NULL);

	if (handler && handler->write_func)
		handler->write_func (emf, puri, stream, info, cancellable);
}

static void
emfq_base_init (EMFormatQuoteClass *klass)
{
	emfq_builtin_init (klass);
}

static void
emfq_class_init (EMFormatQuoteClass *klass)
{
	GObjectClass *object_class;

	parent_class = g_type_class_peek_parent (klass);
	g_type_class_add_private (klass, sizeof (EMFormatQuotePrivate));

	object_class = G_OBJECT_CLASS (klass);
	object_class->dispose = emfq_dispose;
	object_class->finalize = emfq_finalize;
}

static void
emfq_init (EMFormatQuote *emfq)
{
	emfq->priv = EM_FORMAT_QUOTE_GET_PRIVATE (emfq);

	/* we want to convert url's etc */
	emfq->priv->text_html_flags =
		CAMEL_MIME_FILTER_TOHTML_PRE |
		CAMEL_MIME_FILTER_TOHTML_CONVERT_URLS |
		CAMEL_MIME_FILTER_TOHTML_CONVERT_ADDRESSES;
}

GType
em_format_quote_get_type (void)
{
	static GType type = 0;

	if (G_UNLIKELY (type == 0)) {
		static const GTypeInfo type_info = {
			sizeof (EMFormatQuoteClass),
			(GBaseInitFunc) emfq_base_init,
			(GBaseFinalizeFunc) NULL,
			(GClassInitFunc) emfq_class_init,
			(GClassFinalizeFunc) NULL,
			NULL,  /* class_data */
			sizeof (EMFormatQuote),
			0,     /* n_preallocs */
			(GInstanceInitFunc) emfq_init,
			NULL   /* value_table */
		};

		type = g_type_register_static (
			EM_TYPE_FORMAT, "EMFormatQuote", &type_info, 0);
	}

	return type;
}

EMFormatQuote *
em_format_quote_new (const gchar *credits,
                     CamelStream *stream,
                     EMFormatQuoteFlags flags)
{
	EMFormatQuote *emfq;

	g_return_val_if_fail (CAMEL_IS_STREAM (stream), NULL);

	/* Steam must also be seekable so we can reset its position. */
	g_return_val_if_fail (G_IS_SEEKABLE (stream), NULL);

	emfq = g_object_new (EM_TYPE_FORMAT_QUOTE, NULL);

	emfq->priv->credits = g_strdup (credits);
	emfq->priv->flags = flags;

	return emfq;
}

void
em_format_quote_write (EMFormatQuote * emfq,
                       CamelStream * stream,
                       GCancellable * cancellable)
{
	EMFormat *emf;
	GSettings *settings;
	GList *iter;
	EMFormatWriterInfo info = { 0 };

	emf = (EMFormat *) emfq;

	g_seekable_seek (
		G_SEEKABLE (stream),
		0, G_SEEK_SET, NULL, NULL);

	settings = g_settings_new ("org.gnome.evolution.mail");
	if (g_settings_get_boolean (
		settings, "composer-top-signature"))
		camel_stream_write_string (
			stream, "<br>\n", cancellable, NULL);
	g_object_unref (settings);

	if (emfq->priv->credits && *emfq->priv->credits) {
                gchar *credits = g_strdup_printf ("%s<br/>", emfq->priv->credits);
		camel_stream_write_string (stream, credits, cancellable, NULL);
		g_free (credits);
	} else {
                camel_stream_write_string (stream, "<br/>", cancellable, NULL);
	}

	if (emfq->priv->flags & EM_FORMAT_QUOTE_CITE)
		camel_stream_write_string (stream,
                        "<!--+GtkHTML:<DATA class=\"ClueFlow\" "
                        "key=\"orig\" value=\"1\">-->\n"
                        "<blockquote type=cite>\n", cancellable, NULL);

	for (iter = emf->mail_part_list; iter; iter = iter->next) {
		EMFormatPURI *puri = iter->data;

		if (puri->is_attachment || !puri->write_func)
			continue;

		puri = iter->data;

		if (emfq->priv->flags & EM_FORMAT_QUOTE_HEADERS) {
                        GString *buffer = g_string_new ("");
			emfq_format_headers (emfq, buffer, (CamelMedium *) puri->part);
			camel_stream_write_string (stream, buffer->str, cancellable, NULL);
			g_string_free (buffer, TRUE);
		}

		puri->write_func (emf, puri, stream, &info, cancellable);
	}

	if (emfq->priv->flags & EM_FORMAT_QUOTE_CITE)
		camel_stream_write_string (
                        stream, "</blockquote><!--+GtkHTML:"
                        "<DATA class=\"ClueFlow\" clear=\"orig\">-->",
			cancellable, NULL);
}

static void
emfq_write_text_plain (EMFormat *emf,
                       EMFormatPURI *puri,
                       CamelStream *stream,
                       EMFormatWriterInfo *info,
                       GCancellable *cancellable)
{
	EMFormatQuote *emfq = EM_FORMAT_QUOTE (emf);
	CamelStream *filtered_stream;
	CamelMimeFilter *html_filter;
	CamelMimeFilter *sig_strip;
	CamelContentType *type;
	const gchar *format;
	guint32 rgb = 0x737373, flags;

	if (!puri->part)
		return;

	flags = emfq->priv->text_html_flags;

	/* Check for RFC 2646 flowed text. */
	type = camel_mime_part_get_content_type (puri->part);
	if (camel_content_type_is(type, "text", "plain")
	    && (format = camel_content_type_param(type, "format"))
	    && !g_ascii_strcasecmp(format, "flowed"))
		flags |= CAMEL_MIME_FILTER_TOHTML_FORMAT_FLOWED;

	filtered_stream = camel_stream_filter_new (stream);

	if ((emfq->priv->flags & EM_FORMAT_QUOTE_KEEP_SIG) == 0) {
		sig_strip = em_stripsig_filter_new (TRUE);
		camel_stream_filter_add (
			CAMEL_STREAM_FILTER (filtered_stream), sig_strip);
		g_object_unref (sig_strip);
	}

	html_filter = camel_mime_filter_tohtml_new (flags, rgb);
	camel_stream_filter_add (
		CAMEL_STREAM_FILTER (filtered_stream), html_filter);
	g_object_unref (html_filter);

	em_format_format_text (
		EM_FORMAT (emfq), filtered_stream,
		CAMEL_DATA_WRAPPER (puri->part), cancellable);

	camel_stream_flush (filtered_stream, cancellable, NULL);
	g_object_unref (filtered_stream);
}

static void
emfq_write_text_enriched (EMFormat *emf,
                          EMFormatPURI *puri,
                              CamelStream *stream,
                              EMFormatWriterInfo *info,
                              GCancellable *cancellable)
{
	CamelStream *filtered_stream;
	CamelMimeFilter *enriched;
	guint32 flags = 0;
	CamelContentType *ct;
	const gchar *mime_type = NULL;

	ct = camel_mime_part_get_content_type (puri->part);
	if (ct) {
		mime_type = camel_content_type_simple (ct);
		camel_content_type_unref (ct);
	}

	if (g_strcmp0 (mime_type, "text/richtext") == 0) {
		flags = CAMEL_MIME_FILTER_ENRICHED_IS_RICHTEXT;
		camel_stream_write_string (
			stream, "\n<!-- text/richtext -->\n",
			cancellable, NULL);
	} else {
		camel_stream_write_string (
			stream, "\n<!-- text/enriched -->\n",
			cancellable, NULL);
	}

	enriched = camel_mime_filter_enriched_new (flags);
	filtered_stream = camel_stream_filter_new (stream);
	camel_stream_filter_add (
		CAMEL_STREAM_FILTER (filtered_stream), enriched);
	g_object_unref (enriched);

	camel_stream_write_string (stream, "<br><hr><br>", cancellable, NULL);
	em_format_format_text (
		emf, filtered_stream, CAMEL_DATA_WRAPPER (puri->part), cancellable);
	camel_stream_flush (filtered_stream, cancellable, NULL);
	g_object_unref (filtered_stream);
}

static void
emfq_write_text_html (EMFormat *emf,
                      EMFormatPURI *puri,
                      CamelStream *stream,
                      EMFormatWriterInfo *info,
                      GCancellable *cancellable)
{
	EMFormatQuotePrivate *priv;

	priv = EM_FORMAT_QUOTE_GET_PRIVATE (emf);

	camel_stream_write_string (
		stream, "\n<!-- text/html -->\n", cancellable, NULL);

	if ((priv->flags & EM_FORMAT_QUOTE_KEEP_SIG) == 0) {
		CamelMimeFilter *sig_strip;
		CamelStream *filtered_stream;

		filtered_stream = camel_stream_filter_new (stream);

		sig_strip = em_stripsig_filter_new (FALSE);
		camel_stream_filter_add (
			CAMEL_STREAM_FILTER (filtered_stream), sig_strip);
		g_object_unref (sig_strip);

		em_format_format_text (
			emf, filtered_stream,
			(CamelDataWrapper *) puri->part, cancellable);
		camel_stream_flush (filtered_stream, cancellable, NULL);
		g_object_unref (filtered_stream);
	} else {
		em_format_format_text (
			emf, stream,
			(CamelDataWrapper *) puri->part, cancellable);
	}
}

/****************************************************************************/
static EMFormatHandler type_builtin_table[] = {
	{ (gchar *) "text/plain", emfq_parse_text_plain, emfq_write_text_plain, },
	{ (gchar *) "text/enriched", emfq_parse_text_enriched, emfq_write_text_enriched, },
	{ (gchar *) "text/richtext", emfq_parse_text_enriched, emfq_write_text_enriched, },
	{ (gchar *) "text/html", emfq_parse_text_html, emfq_write_text_html, },
	{ (gchar *) "text/*", emfq_parse_text_plain, emfq_write_text_plain, },
	{ (gchar *) "message/external-body", em_format_empty_parser, em_format_empty_writer, },
	{ (gchar *) "multipart/appledouble", em_format_empty_parser, em_format_empty_writer, },

	/* internal evolution types */
	{ (gchar *) "x-evolution/evolution-rss-feed", 0, emfq_write_text_html, },
	{ (gchar *) "x-evolution/message/attachment", emfq_parse_attachment, emfq_write_attachment, },
};

static void
emfq_builtin_init (EMFormatQuoteClass *efhc)
{
	gint ii;

	EMFormatClass *emfc = (EMFormatClass *) efhc;

	for (ii = 0; ii < G_N_ELEMENTS (type_builtin_table); ii++)
		em_format_class_add_handler (
			emfc, &type_builtin_table[ii]);
}
