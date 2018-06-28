/*
 * e-mail-parser-inlinepgp-signed.c
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 *
 */

#include "evolution-config.h"

#include <string.h>
#include <glib/gi18n-lib.h>

#include <e-util/e-util.h>

#include "e-mail-formatter-utils.h"
#include "e-mail-parser-extension.h"
#include "e-mail-part-utils.h"

typedef EMailParserExtension EMailParserInlinePGPSigned;
typedef EMailParserExtensionClass EMailParserInlinePGPSignedClass;

GType e_mail_parser_inline_pgp_signed_get_type (void);

G_DEFINE_TYPE (
	EMailParserInlinePGPSigned,
	e_mail_parser_inline_pgp_signed,
	E_TYPE_MAIL_PARSER_EXTENSION)

static const gchar *parser_mime_types[] = {
	"application/x-inlinepgp-signed",
	NULL
};

static gboolean
empe_inlinepgp_signed_parse (EMailParserExtension *extension,
                             EMailParser *parser,
                             CamelMimePart *part,
                             GString *part_id,
                             GCancellable *cancellable,
                             GQueue *out_mail_parts)
{
	CamelStream *filtered_stream;
	CamelMimeFilterPgp *pgp_filter;
	CamelContentType *content_type;
	CamelCipherContext *cipher;
	CamelCipherValidity *valid;
	CamelDataWrapper *dw;
	CamelMimePart *opart;
	CamelStream *ostream;
	GQueue work_queue = G_QUEUE_INIT;
	GList *head, *link;
	gchar *type;
	gint len;
	GError *local_error = NULL;
	GByteArray *ba;

	if (g_cancellable_is_cancelled (cancellable))
		return FALSE;

	cipher = camel_gpg_context_new (e_mail_parser_get_session (parser));

	/* Verify the signature of the message */
	valid = camel_cipher_context_verify_sync (
		cipher, part, cancellable, &local_error);

	if (local_error != NULL) {
		e_mail_parser_error (
			parser, out_mail_parts,
			_("Error verifying signature: %s"),
			local_error->message);

		g_error_free (local_error);

		e_mail_parser_parse_part_as (
			parser, part, part_id,
			"application/vnd.evolution.source",
			cancellable, out_mail_parts);

		g_object_unref (cipher);

		return TRUE;
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

	g_warn_if_fail (e_mail_parser_parse_part (
		parser, opart, part_id, cancellable, &work_queue));

	head = g_queue_peek_head_link (&work_queue);

	for (link = head; link != NULL; link = g_list_next (link)) {
		EMailPart *mail_part = link->data;

		e_mail_part_update_validity (
			mail_part, valid,
			E_MAIL_PART_VALIDITY_SIGNED |
			E_MAIL_PART_VALIDITY_PGP);

		/* Do not traverse sub-messages */
		if (g_str_has_suffix (e_mail_part_get_id (mail_part), ".rfc822"))
			link = e_mail_formatter_find_rfc822_end_iter (link);
	}

	e_queue_transfer (&work_queue, out_mail_parts);

	g_string_truncate (part_id, len);

	/* Add a widget with details about the encryption, but only when
	 * the encrypted isn't itself secured, in that case it has created
	 * the button itself */
	if (!e_mail_part_is_secured (opart)) {
		EMailPart *mail_part;

		g_string_append (part_id, ".inlinepgp_signed.button");

		e_mail_parser_parse_part_as (
			parser, part, part_id,
			"application/vnd.evolution.secure-button",
			cancellable, &work_queue);

		mail_part = g_queue_peek_head (&work_queue);
		if (mail_part != NULL)
			e_mail_part_update_validity (
				mail_part, valid,
				E_MAIL_PART_VALIDITY_SIGNED |
				E_MAIL_PART_VALIDITY_PGP);

		e_queue_transfer (&work_queue, out_mail_parts);

		g_string_truncate (part_id, len);
	}

	/* Clean Up */
	camel_cipher_validity_free (valid);
	g_object_unref (opart);
	g_object_unref (ostream);
	g_object_unref (cipher);

	return TRUE;
}

static void
e_mail_parser_inline_pgp_signed_class_init (EMailParserExtensionClass *class)
{
	class->mime_types = parser_mime_types;
	class->priority = G_PRIORITY_LOW;
	class->parse = empe_inlinepgp_signed_parse;
}

static void
e_mail_parser_inline_pgp_signed_init (EMailParserExtension *extension)
{
}
