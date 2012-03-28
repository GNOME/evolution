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
#include <gtk/gtk.h>

#include "em-format-html-print.h"
#include "em-format-html-display.h"
#include "e-mail-attachment-bar.h"
#include <e-util/e-print.h>
#include <e-util/e-util.h>
#include <widgets/misc/e-attachment-store.h>
#include <libemail-engine/mail-ops.h>

#include "em-format-html-print.h"

#define d(x)

static gpointer parent_class = NULL;

struct _EMFormatHTMLPrintPrivate {

	EMFormatHTML *original_formatter;
	EMFormatPURI *top_level_puri;

        /* List of attachment PURIs */
        GList *attachments;

};

enum {
	PROP_0,
	PROP_ORIGINAL_FORMATTER
};

static void efhp_write_print_layout	(EMFormat *emf, EMFormatPURI *puri, CamelStream *stream, EMFormatWriterInfo *info, GCancellable *cancellable);
static void efhp_write_headers		(EMFormat *emf, EMFormatPURI *puri, CamelStream *stream, EMFormatWriterInfo *info, GCancellable *cancellable);
static void efhp_write_inline_attachment (EMFormat *emf, EMFormatPURI *puri, CamelStream *stream, EMFormatWriterInfo *info, GCancellable *cancellable);

static void
efhp_write_attachments_list (EMFormatHTMLPrint *efhp,
                             CamelStream *stream,
                             EMFormatWriterInfo *info,
                             GCancellable *cancellable)
{
	GString *str;
	GList *iter;

	if (!efhp->priv->attachments)
		return;

	str = g_string_new (
                "<table border=\"0\" cellspacing=\"5\" cellpadding=\"0\" "
                       "class=\"attachments-list\" >\n");
	g_string_append_printf (str,
                "<tr><th colspan=\"2\"><h1>%s</h1></td></tr>\n"
                "<tr><th>%s</th><th>%s</th></tr>\n",
                _("Attachments"), _("Name"), _("Size"));

	for (iter = efhp->priv->attachments; iter; iter = iter->next) {
		EMFormatPURI *puri = iter->data;
		EAttachment *attachment;
		GFileInfo *fi;
		gchar *name, *size;
		GByteArray *ba;
		CamelDataWrapper *dw;

		attachment = ((EMFormatAttachmentPURI *) puri)->attachment;
		fi = e_attachment_get_file_info (attachment);
		if (!fi)
			continue;

		if (e_attachment_get_description (attachment) &&
                    *e_attachment_get_description (attachment)) {
                        name = g_strdup_printf ("%s (%s)",
				e_attachment_get_description (attachment),
				g_file_info_get_display_name (fi));
		} else {
			name = g_strdup (g_file_info_get_display_name (fi));
		}

		dw = camel_medium_get_content ((CamelMedium *) puri->part);
		ba = camel_data_wrapper_get_byte_array (dw);
		size = g_format_size (ba->len);

                g_string_append_printf (str, "<tr><td>%s</td><td>%s</td></tr>\n",
			name, size);

		g_free (name);
		g_free (size);
	}

        g_string_append (str, "</table>\n");

	camel_stream_write_string (stream, str->str, cancellable, NULL);
	g_string_free (str, TRUE);
}

static void
efhp_write_headers (EMFormat *emf,
                    EMFormatPURI *puri,
                    CamelStream *stream,
                    EMFormatWriterInfo *info,
                    GCancellable *cancellable)
{
	struct _camel_header_raw raw_header;
	GString *str, *tmp;
	gchar *subject;
	const gchar *buf;
	EMFormatPURI *p;
	GList *iter;
	gint attachments_count;
	gchar *puri_prefix;

	buf = camel_medium_get_header (CAMEL_MEDIUM (puri->part), "subject");
	subject = camel_header_decode_string (buf, "UTF-8");
	str = g_string_new ("<table border=\"0\" cellspacing=\"5\" " \
                            "cellpadding=\"0\" class=\"printing-header\">\n");
	g_string_append_printf (
		str,
                "<tr class=\"header-item\">"
                "<td colspan=\"2\"><h1>%s</h1></td>"
                "</tr>\n",
		subject);
	g_free (subject);

	for (iter = g_queue_peek_head_link (&emf->header_list); iter; iter = iter->next) {

		EMFormatHeader *header = iter->data;
		raw_header.name = header->name;

		/* Skip 'Subject' header, it's already displayed. */
		if (g_ascii_strncasecmp (header->name, "Subject", 7) == 0)
			continue;

		if (header->value && *header->value) {
			raw_header.value = header->value;
			em_format_html_format_header (emf, str,
				CAMEL_MEDIUM (puri->part), &raw_header,
				header->flags | EM_FORMAT_HTML_HEADER_NOLINKS,
				"UTF-8");
		} else {
			raw_header.value = g_strdup (camel_medium_get_header (
				CAMEL_MEDIUM (emf->message), header->name));

			if (raw_header.value && *raw_header.value) {
				em_format_html_format_header (emf, str,
					CAMEL_MEDIUM (puri->part), &raw_header,
					header->flags | EM_FORMAT_HTML_HEADER_NOLINKS,
					"UTF-8");
			}

			if (raw_header.value)
				g_free (raw_header.value);
		}
	}

        /* Get prefix of this PURI */
        puri_prefix = g_strndup (puri->uri, g_strrstr (puri->uri, ".") - puri->uri);

	/* Add encryption/signature header */
	raw_header.name = _("Security");
	tmp = g_string_new ("");
	/* Find first secured part. */
	for (iter = emf->mail_part_list, puri; iter; iter = iter->next) {

		p = iter->data;

		if (p->validity_type == 0)
			continue;

		if (!g_str_has_prefix (p->uri, puri_prefix))
			continue;

		if ((p->validity_type & EM_FORMAT_VALIDITY_FOUND_PGP) &&
		    (p->validity_type & EM_FORMAT_VALIDITY_FOUND_SIGNED)) {
			g_string_append (tmp, _("GPG signed"));
		}
		if ((p->validity_type & EM_FORMAT_VALIDITY_FOUND_PGP) &&
		    (p->validity_type & EM_FORMAT_VALIDITY_FOUND_ENCRYPTED)) {
			if (tmp->len > 0) g_string_append (tmp, ", ");
			g_string_append (tmp, _("GPG encrpyted"));
		}
		if ((p->validity_type & EM_FORMAT_VALIDITY_FOUND_SMIME) &&
		    (p->validity_type & EM_FORMAT_VALIDITY_FOUND_SIGNED)) {

			if (tmp->len > 0) g_string_append (tmp, ", ");
			g_string_append (tmp, _("S/MIME signed"));
		}
		if ((p->validity_type & EM_FORMAT_VALIDITY_FOUND_SMIME) &&
		    (p->validity_type & EM_FORMAT_VALIDITY_FOUND_ENCRYPTED)) {

			if (tmp->len > 0) g_string_append (tmp, ", ");
			g_string_append (tmp, _("S/MIME encrpyted"));
		}

		break;
	}

	if (tmp->len > 0) {
		raw_header.value = tmp->str;
		em_format_html_format_header (emf, str, CAMEL_MEDIUM (p->part),
			&raw_header, EM_FORMAT_HEADER_BOLD | EM_FORMAT_HTML_HEADER_NOLINKS, "UTF-8");
	}
	g_string_free (tmp, TRUE);

	/* Count attachments and display the number as a header */
	attachments_count = 0;

	for (iter = emf->mail_part_list; iter; iter = iter ? iter->next : iter) {

		p = iter->data;

		if (!g_str_has_prefix (p->uri, puri_prefix))
			continue;

		if ((p->is_attachment || g_str_has_suffix(p->uri, ".attachment")) &&
		    (!p->cid)) {
			attachments_count++;
                        /* EFHD sometimes creates two PURIs per attachment! */
			if (iter->next && iter->next->data) {
				EMFormatPURI *p2 = iter->next->data;
				if (g_str_has_prefix (p2->uri, p->uri))
					iter = iter->next;
			}
		}
	}
	if (attachments_count > 0) {
		raw_header.name = _("Attachments");
		raw_header.value = g_strdup_printf ("%d", attachments_count);
		em_format_html_format_header (emf, str, CAMEL_MEDIUM (puri->part),
			&raw_header, EM_FORMAT_HEADER_BOLD | EM_FORMAT_HTML_HEADER_NOLINKS, "UTF-8");
		g_free (raw_header.value);
	}

	g_string_append (str, "</table>");

	camel_stream_write_string (stream, str->str, cancellable, NULL);
	g_string_free (str, TRUE);
	g_free (puri_prefix);
}

static void
efhp_write_inline_attachment (EMFormat *emf,
                              EMFormatPURI *puri,
                              CamelStream *stream,
                              EMFormatWriterInfo *info,
                              GCancellable *cancellable)
{
	gchar *name;
	EMFormatAttachmentPURI *att_puri = (EMFormatAttachmentPURI *) puri;
	EAttachment *attachment;
	GFileInfo *fi;

	attachment = att_puri->attachment;
	fi = e_attachment_get_file_info (attachment);

	if (e_attachment_get_description (attachment) &&
            *e_attachment_get_description (attachment)) {
                name = g_strdup_printf ("<h2>Attachment: %s (%s)</h2>\n",
			e_attachment_get_description (attachment),
			g_file_info_get_display_name (fi));
	} else {
                name = g_strdup_printf ("<h2>Attachment: %s</h2>\n",
			g_file_info_get_display_name (fi));
	}

	camel_stream_write_string (stream, name, cancellable, NULL);
	g_free (name);
}

static void
efhp_write_print_layout (EMFormat *emf,
                         EMFormatPURI *puri,
                         CamelStream *stream,
                         EMFormatWriterInfo *info,
                         GCancellable *cancellable)
{
	GList *iter;
	EMFormatWriterInfo print_info = {
		EM_FORMAT_WRITE_MODE_PRINTING, FALSE, FALSE };
	EMFormatHTMLPrint *efhp = EM_FORMAT_HTML_PRINT (emf);

	g_list_free (efhp->priv->attachments);
	efhp->priv->attachments = NULL;

	camel_stream_write_string (stream,
		"<!DOCTYPE HTML>\n<html>\n"
		"<head>\n<meta name=\"generator\" content=\"Evolution Mail Component\" />\n"
		"<title>Evolution Mail Display</title>\n"
		"<link type=\"text/css\" rel=\"stylesheet\" media=\"print\" "
                      "href=\"evo-file://" EVOLUTION_PRIVDATADIR "/theme/webview-print.css\" />\n"
		"</head>\n"
		"<body style=\"background: #FFF; color: #000;\">",
		cancellable, NULL);

	for (iter = emf->mail_part_list; iter != NULL; iter = iter ? iter->next : iter) {

		EMFormatPURI *puri = iter->data;

		if (g_str_has_suffix (puri->uri, "print_layout"))
			continue;

		/* To late to change .headers writer_func, do it manually. */
		if (g_str_has_suffix (puri->uri, ".headers")) {
			efhp_write_headers (emf, puri, stream, &print_info, cancellable);
			continue;
		}

		if (g_str_has_suffix (puri->uri, ".rfc822")) {

			puri->write_func (emf, puri, stream, &print_info, cancellable);

                        while (iter && !g_str_has_suffix (puri->uri, ".rfc822.end")) {

				iter = iter->next;
				if (iter)
					puri = iter->data;
			}

			if (!iter)
				break;

			continue;

		}

		if (puri->is_attachment || g_str_has_suffix (puri->uri, ".attachment")) {
			const EMFormatHandler *handler;
			CamelContentType *ct;
			gchar *mime_type;

                        if (puri->cid && g_ascii_strncasecmp (puri->cid, "cid:", 4) == 0)
				continue;

			ct = camel_mime_part_get_content_type (puri->part);
			mime_type = camel_content_type_simple (ct);

			handler = em_format_find_handler (puri->emf, mime_type);
                        d(printf("Handler for PURI %s (%s): %s\n", puri->uri, mime_type,
                                 handler ? handler->mime_type : "(null)"));
			g_free (mime_type);

			efhp->priv->attachments =
				g_list_append (efhp->priv->attachments, puri);

			/* If we can't inline this attachment, skip it */
			if (handler && puri->write_func) {
				efhp_write_inline_attachment (puri->emf, puri,
					stream, &print_info, cancellable);

				if (iter->next && iter->next->data) {
					EMFormatPURI *p;
					p = iter->next->data;

                                        /* Has the next PURI the same prefix? */
					if (p->write_func &&
					    g_str_has_prefix (p->uri, puri->uri)) {
						p->write_func (emf, p, stream,
						       &print_info, cancellable);
						iter = iter->next;
					} else {
						if (puri->write_func) {
							puri->write_func (emf, puri,
								stream, &print_info,
								cancellable);
						}
					}
				}
			}

			continue;
		}

		/* Ignore widget parts and unwritable non-attachment parts */
		if (puri->write_func == NULL)
			continue;

                /* Passed all tests, probably a regular part - display it */
		puri->write_func (puri->emf, puri, stream, &print_info, cancellable);

	}

	efhp_write_attachments_list (efhp, stream, &print_info, cancellable);

	camel_stream_write_string (stream, "</body></html>", cancellable, NULL);
}

static void
efhp_finalize (GObject *object)
{
	EMFormatHTMLPrint *efhp = EM_FORMAT_HTML_PRINT (object);

	if (efhp->priv->original_formatter) {
		g_object_unref (efhp->priv->original_formatter);
		efhp->priv->original_formatter = NULL;
	}

	if (efhp->priv->top_level_puri) {
		em_format_puri_free (efhp->priv->top_level_puri);
		efhp->priv->top_level_puri = NULL;
	}

	if (efhp->priv->attachments) {
		g_list_free (efhp->priv->attachments);
		efhp->priv->attachments = NULL;
	}

	/* Chain up to parent's finalize() method. */
	G_OBJECT_CLASS (parent_class)->finalize (object);
}

static gboolean
efhp_is_inline (EMFormat *emf,
                const gchar *part_id,
                CamelMimePart *mime_part,
                const EMFormatHandler *handle)
{
	/* When printing, inline any part that has a handler. */
	return (handle != NULL);
}

static void
efhp_set_orig_formatter (EMFormatHTMLPrint *efhp,
                             EMFormat *formatter)
{
	EMFormat *emfp, *emfs;
	EMFormatPURI *puri;
	GHashTableIter iter;
	gpointer key, value;

	efhp->priv->original_formatter = g_object_ref (formatter);

	emfp = EM_FORMAT (efhp);
	emfs = EM_FORMAT (formatter);

	emfp->mail_part_list = g_list_copy (emfs->mail_part_list);

	/* Make a shallow copy of the table. This table will NOT destroy
	 * the PURIs when free'd! */
	if (emfp->mail_part_table)
		g_hash_table_unref (emfp->mail_part_table);

	emfp->mail_part_table = g_hash_table_new (g_str_hash, g_str_equal);
	g_hash_table_iter_init (&iter, emfs->mail_part_table);
	while (g_hash_table_iter_next (&iter, &key, &value))
		g_hash_table_insert (emfp->mail_part_table, key, value);

	if (emfs->folder)
		emfp->folder = g_object_ref (emfs->folder);
	emfp->message_uid = g_strdup (emfs->message_uid);
	emfp->message = g_object_ref (emfs->message);

	/* Add a generic PURI that will write a HTML layout
	 * for all the parts */
	puri = em_format_puri_new (EM_FORMAT (efhp),
		sizeof (EMFormatPURI), NULL, "print_layout");
	puri->write_func = efhp_write_print_layout;
	puri->mime_type = g_strdup ("text/html");
	em_format_add_puri (EM_FORMAT (efhp), puri);
	efhp->priv->top_level_puri = puri;
}

static EMFormatHandler type_builtin_table[] = {
        { (gchar *) "x-evolution/message/headers", 0, efhp_write_headers, },
};

static void
efhp_builtin_init (EMFormatHTMLPrintClass *efhc)
{
	EMFormatClass *emfc;
	gint ii;

	emfc = (EMFormatClass *) efhc;

	for (ii = 0; ii < G_N_ELEMENTS (type_builtin_table); ii++)
		em_format_class_add_handler (
			emfc, &type_builtin_table[ii]);
}

static void
efhp_set_property (GObject *object,
                   guint prop_id,
                   const GValue *value,
                   GParamSpec *pspec)
{
	switch (prop_id) {

		case PROP_ORIGINAL_FORMATTER:
			efhp_set_orig_formatter (
				EM_FORMAT_HTML_PRINT (object),
				(EMFormat *) g_value_get_object (value));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
}

static void
efhp_get_property (GObject *object,
                   guint prop_id,
                   GValue *value,
                   GParamSpec *pspec)
{
	EMFormatHTMLPrintPrivate *priv;

	priv = EM_FORMAT_HTML_PRINT (object)->priv;

	switch (prop_id) {

		case PROP_ORIGINAL_FORMATTER:
			g_value_set_pointer (value,
				priv->original_formatter);
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
}

static void
em_format_html_print_base_init (EMFormatHTMLPrintClass *klass)
{
	efhp_builtin_init (klass);
}

static void
em_format_html_print_class_init (EMFormatHTMLPrintClass *klass)
{
	GObjectClass *object_class;
	EMFormatClass *format_class;

	parent_class = g_type_class_peek_parent (klass);
	g_type_class_add_private (klass, sizeof (EMFormatHTMLPrintPrivate));

	object_class = G_OBJECT_CLASS (klass);
	object_class->finalize = efhp_finalize;
	object_class->set_property = efhp_set_property;
	object_class->get_property = efhp_get_property;

	format_class = EM_FORMAT_CLASS (klass);
	format_class->is_inline = efhp_is_inline;

	g_object_class_install_property (
		object_class,
		PROP_ORIGINAL_FORMATTER,
		g_param_spec_object (
			"original-formatter",
			NULL,
			NULL,
			EM_TYPE_FORMAT,
			G_PARAM_WRITABLE |
			G_PARAM_CONSTRUCT_ONLY));
}

static void
em_format_html_print_init (EMFormatHTMLPrint *efhp)
{
	efhp->priv = G_TYPE_INSTANCE_GET_PRIVATE (
		efhp, EM_TYPE_FORMAT_HTML_PRINT, EMFormatHTMLPrintPrivate);

	efhp->priv->attachments = NULL;
	efhp->export_filename = NULL;
}

GType
em_format_html_print_get_type (void)
{
	static GType type = 0;

	if (G_UNLIKELY (type == 0)) {
		static const GTypeInfo type_info = {
			sizeof (EMFormatHTMLPrintClass),
			(GBaseInitFunc) em_format_html_print_base_init,
			(GBaseFinalizeFunc) NULL,
			(GClassInitFunc) em_format_html_print_class_init,
			(GClassFinalizeFunc) NULL,
			NULL,  /* class_data */
			sizeof (EMFormatHTMLPrint),
			0,     /* n_preallocs */
			(GInstanceInitFunc) em_format_html_print_init,
			NULL   /* value_table */
		};

		type = g_type_register_static (
			em_format_html_get_type(), "EMFormatHTMLPrint",
			&type_info, 0);
	}

	return type;
}

EMFormatHTMLPrint *
em_format_html_print_new (EMFormatHTML *source)
{
	EMFormatHTMLPrint *efhp;

	efhp = g_object_new (EM_TYPE_FORMAT_HTML_PRINT,
		"original-formatter", source,
		NULL);

	return efhp;
}
