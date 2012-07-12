/*
 * e-mail-parser-inlinepgp-signed.c
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
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "e-mail-format-extensions.h"

#include <em-format/e-mail-parser-extension.h>
#include <em-format/e-mail-parser.h>
#include <em-format/e-mail-part-utils.h>
#include <e-util/e-util.h>

#include <glib/gi18n-lib.h>
#include <camel/camel.h>

#include <string.h>

typedef struct _EMailParserInlinePGPSigned {
	GObject parent;
} EMailParserInlinePGPSigned;

typedef struct _EMailParserInlinePGPSignedClass {
	GObjectClass parent_class;
} EMailParserInlinePGPSignedClass;

static void e_mail_parser_parser_extension_interface_init (EMailParserExtensionInterface *iface);
static void e_mail_parser_mail_extension_interface_init (EMailExtensionInterface *iface);

G_DEFINE_TYPE_EXTENDED (
	EMailParserInlinePGPSigned,
	e_mail_parser_inline_pgp_signed,
	G_TYPE_OBJECT,
	0,
	G_IMPLEMENT_INTERFACE (
		E_TYPE_MAIL_EXTENSION,
		e_mail_parser_mail_extension_interface_init)
	G_IMPLEMENT_INTERFACE (
		E_TYPE_MAIL_PARSER_EXTENSION,
		e_mail_parser_parser_extension_interface_init));

static const gchar * parser_mime_types[] = { "application/x-inlinepgp-signed",
					    NULL };

static GSList *
empe_inlinepgp_signed_parse (EMailParserExtension *extension,
                             EMailParser *parser,
                             CamelMimePart *part,
                             GString *part_id,
                             GCancellable *cancellable)
{
	CamelStream *filtered_stream;
	CamelMimeFilterPgp *pgp_filter;
	CamelContentType *content_type;
	CamelCipherContext *cipher;
	CamelCipherValidity *valid;
	CamelDataWrapper *dw;
	CamelMimePart *opart;
	CamelStream *ostream;
	gchar *type;
	gint len;
	GError *local_error = NULL;
	GByteArray *ba;
	GSList *parts, *iter;

	if (g_cancellable_is_cancelled (cancellable))
		return NULL;

	cipher = camel_gpg_context_new (e_mail_parser_get_session (parser));

	/* Verify the signature of the message */
	valid = camel_cipher_context_verify_sync (
		cipher, part, cancellable, &local_error);
	if (!valid) {
		parts = e_mail_parser_error (
				parser, cancellable,
				_("Error verifying signature: %s"),
				local_error->message ?
					local_error->message :
					_("Unknown error"));

		g_clear_error (&local_error);

		parts = g_slist_concat (parts,
				e_mail_parser_parse_part_as (
					parser, part, part_id,
					"application/vnd.evolution.source",
					cancellable));

		g_object_unref (cipher);
		return parts;
	}

	/* Setup output stream */
	ostream = camel_stream_mem_new ();
	filtered_stream = camel_stream_filter_new (ostream);

	/* Add PGP header / footer filter */
	pgp_filter = (CamelMimeFilterPgp *) camel_mime_filter_pgp_new ();
	camel_stream_filter_add (
		CAMEL_STREAM_FILTER (filtered_stream),
		CAMEL_MIME_FILTER (pgp_filter));
	g_object_unref (pgp_filter);

	/* Pass through the filters that have been setup */
	dw = camel_medium_get_content ((CamelMedium *) part);
	camel_data_wrapper_decode_to_stream_sync (
		dw, (CamelStream *) filtered_stream, cancellable, NULL);
	camel_stream_flush ((CamelStream *) filtered_stream, cancellable, NULL);
	g_object_unref (filtered_stream);

	/* Create a new text/plain MIME part containing the signed
	 * content preserving the original part's Content-Type params. */
	content_type = camel_mime_part_get_content_type (part);
	type = camel_content_type_format (content_type);
	content_type = camel_content_type_decode (type);
	g_free (type);

	g_free (content_type->type);
	content_type->type = g_strdup ("text");
	g_free (content_type->subtype);
	content_type->subtype = g_strdup ("plain");
	type = camel_content_type_format (content_type);
	camel_content_type_unref (content_type);

	ba = camel_stream_mem_get_byte_array ((CamelStreamMem *) ostream);
	opart = camel_mime_part_new ();
	camel_mime_part_set_content (opart, (gchar *) ba->data, ba->len, type);
	g_free (type);

	len = part_id->len;
	g_string_append (part_id, ".inlinepgp_signed");

	parts = e_mail_parser_parse_part (
			parser, opart, part_id, cancellable);

	for (iter = parts; iter; iter = iter->next) {
		EMailPart *mail_part;

		mail_part = iter->data;
		if (!mail_part)
			continue;

		e_mail_part_update_validity (mail_part, valid,
			E_MAIL_PART_VALIDITY_SIGNED |
			E_MAIL_PART_VALIDITY_PGP);
	}

	g_string_truncate (part_id, len);

	/* Add a widget with details about the encryption, but only when
	 * the encrypted isn't itself secured, in that case it has created
	 * the button itself */
	if (!e_mail_part_is_secured (opart)) {
		GSList *button;
		EMailPart *mail_part;
		g_string_append (part_id, ".inlinepgp_signed.button");

		button = e_mail_parser_parse_part_as (
				parser, part, part_id,
				"application/vnd.evolution.widget.secure-button",
				cancellable);
		if (button && button->data) {
			mail_part = button->data;

			e_mail_part_update_validity (mail_part, valid,
				E_MAIL_PART_VALIDITY_SIGNED |
				E_MAIL_PART_VALIDITY_PGP);
		}

		parts = g_slist_concat (parts, button);

		g_string_truncate (part_id, len);
	}

	/* Clean Up */
	camel_cipher_validity_free (valid);
	g_object_unref (opart);
	g_object_unref (ostream);
	g_object_unref (cipher);

	return parts;
}

static const gchar **
empe_inlinepgp_signed_mime_types (EMailExtension *extension)
{
	return parser_mime_types;
}

static void
e_mail_parser_inline_pgp_signed_class_init (EMailParserInlinePGPSignedClass *class)
{
}

static void
e_mail_parser_parser_extension_interface_init (EMailParserExtensionInterface *iface)
{
	iface->parse = empe_inlinepgp_signed_parse;
}

static void
e_mail_parser_mail_extension_interface_init (EMailExtensionInterface *iface)
{
	iface->mime_types = empe_inlinepgp_signed_mime_types;
}

static void
e_mail_parser_inline_pgp_signed_init (EMailParserInlinePGPSigned *parser)
{

}
