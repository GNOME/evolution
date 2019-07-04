/*
 * e-mail-parser-multipart-signed.c
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

#include <glib/gi18n-lib.h>

#include <libedataserver/libedataserver.h>

#include "e-mail-formatter-utils.h"
#include "e-mail-parser-extension.h"
#include "e-mail-part-utils.h"

typedef EMailParserExtension EMailParserMultipartSigned;
typedef EMailParserExtensionClass EMailParserMultipartSignedClass;

GType e_mail_parser_multipart_signed_get_type (void);

G_DEFINE_TYPE (
	EMailParserMultipartSigned,
	e_mail_parser_multipart_signed,
	E_TYPE_MAIL_PARSER_EXTENSION)

static const gchar *parser_mime_types[] = {
	"multipart/signed",
	"application/pgp-signature",
	NULL
};

static gboolean
empe_mp_signed_parse (EMailParserExtension *extension,
                      EMailParser *parser,
                      CamelMimePart *part,
                      GString *part_id,
                      GCancellable *cancellable,
                      GQueue *out_mail_parts)
{
	CamelMimePart *cpart = NULL;
	CamelMultipart *multipart;
	CamelCipherContext *cipher = NULL;
	CamelContentType *content_type;
	CamelSession *session;
	guint32 validity_type;
	CamelCipherValidity *valid;
	const gchar *protocol = NULL;
	GError *local_error = NULL;
	gint i, nparts, len;
	gboolean secured;

	/* If the part is application/pgp-signature sub-part then skip it. */
	if (!CAMEL_IS_MULTIPART (part)) {
		gboolean is_guessed;

		content_type = camel_mime_part_get_content_type (part);

		/* When it's a guessed type, then rather not interpret it as a signed message */
		is_guessed = g_strcmp0 (camel_content_type_param (content_type, E_MAIL_PART_X_EVOLUTION_GUESSED), "1") == 0;

		if (is_guessed || camel_content_type_is (content_type, "application", "pgp-signature")) {
			gboolean add_as_attachment = is_guessed;

			if (!add_as_attachment) {
				EMailPartList *part_list;

				part_list = e_mail_parser_ref_part_list_for_operation (parser, cancellable);
				if (part_list) {
					CamelMimePart *parent_part;

					parent_part = e_mail_part_utils_find_parent_part (e_mail_part_list_get_message (part_list), part);
					if (parent_part) {
						content_type = camel_mime_part_get_content_type (parent_part);
						add_as_attachment = !camel_content_type_is (content_type, "multipart", "signed");
					}

					g_object_unref (part_list);
				}
			}

			if (add_as_attachment)
				e_mail_parser_wrap_as_non_expandable_attachment (parser, part, part_id, out_mail_parts);

			return TRUE;
		}
	}

	multipart = (CamelMultipart *) camel_medium_get_content ((CamelMedium *) part);
	if (CAMEL_IS_MULTIPART_SIGNED (multipart)) {
		cpart = camel_multipart_get_part (
			multipart, CAMEL_MULTIPART_SIGNED_CONTENT);
	}

	if (cpart == NULL) {
		e_mail_parser_error (
			parser, out_mail_parts,
			_("Could not parse MIME message. "
			"Displaying as source."));
		e_mail_parser_parse_part_as (
			parser, part, part_id,
			"application/vnd.evolution.source",
			cancellable, out_mail_parts);

		return TRUE;
	}

	content_type = camel_data_wrapper_get_mime_type_field (
		CAMEL_DATA_WRAPPER (multipart));
	if (content_type != NULL)
		protocol = camel_content_type_param (content_type, "protocol");

	session = e_mail_parser_get_session (parser);
	/* FIXME: Should be done via a plugin interface */
	/* FIXME: duplicated in em-format-html-display.c */
	if (protocol != NULL) {
#ifdef ENABLE_SMIME
		if (g_ascii_strcasecmp ("application/pkcs7-signature", protocol) == 0 ||
		    g_ascii_strcasecmp ("application/xpkcs7signature", protocol) == 0 ||
		    g_ascii_strcasecmp ("application/xpkcs7-signature", protocol) == 0 ||
		    g_ascii_strcasecmp ("application/x-pkcs7-signature", protocol) == 0) {
			cipher = camel_smime_context_new (session);
			validity_type = E_MAIL_PART_VALIDITY_SMIME;
		} else {
#endif
			if (g_ascii_strcasecmp ("application/pgp-signature", protocol) == 0) {
				cipher = camel_gpg_context_new (session);
				validity_type = E_MAIL_PART_VALIDITY_PGP;
			}
#ifdef ENABLE_SMIME
		}
#endif
	}

	if (cipher == NULL) {
		e_mail_parser_error (
			parser, out_mail_parts,
			_("Unsupported signature format"));
		e_mail_parser_parse_part_as (
			parser, part, part_id, "multipart/mixed",
			cancellable, out_mail_parts);

		return TRUE;
	}

	valid = camel_cipher_context_verify_sync (
		cipher, part, cancellable, &local_error);

	if (local_error != NULL) {
		e_mail_parser_error (
			parser, out_mail_parts,
			_("Error verifying signature: %s"),
			local_error->message);
		e_mail_parser_parse_part_as (
			parser, part, part_id, "multipart/mixed",
			cancellable, out_mail_parts);

		g_object_unref (cipher);
		g_error_free (local_error);

		return TRUE;
	}

	nparts = camel_multipart_get_number (multipart);
	secured = FALSE;
	len = part_id->len;
	for (i = 0; i < nparts; i++) {
		GQueue work_queue = G_QUEUE_INIT;
		GList *head, *link;
		CamelMimePart *subpart;

		subpart = camel_multipart_get_part (multipart, i);

		g_string_append_printf (part_id, ".signed.%d", i);

		g_warn_if_fail (e_mail_parser_parse_part (
			parser, subpart, part_id, cancellable, &work_queue));

		g_string_truncate (part_id, len);

		if (!secured)
			secured = e_mail_part_is_secured (subpart);

		head = g_queue_peek_head_link (&work_queue);

		for (link = head; link != NULL; link = g_list_next (link)) {
			EMailPart *mail_part = link->data;

			e_mail_part_update_validity (
				mail_part, valid,
				validity_type | E_MAIL_PART_VALIDITY_SIGNED);

			/* Do not traverse sub-messages */
			if (g_str_has_suffix (e_mail_part_get_id (mail_part), ".rfc822"))
				link = e_mail_formatter_find_rfc822_end_iter (link);
		}

		e_queue_transfer (&work_queue, out_mail_parts);
	}

	/* Add a widget with details about the encryption, but only when
	 * the encrypted isn't itself secured, in that case it has created
	 * the button itself. */
	if (!secured) {
		GQueue work_queue = G_QUEUE_INIT;
		EMailPart *mail_part;

		g_string_append (part_id, ".signed.button");

		e_mail_parser_parse_part_as (
			parser, part, part_id,
			"application/vnd.evolution.secure-button",
			cancellable, &work_queue);

		mail_part = g_queue_peek_head (&work_queue);

		if (mail_part != NULL)
			e_mail_part_update_validity (
				mail_part, valid,
				validity_type | E_MAIL_PART_VALIDITY_SIGNED);

		e_queue_transfer (&work_queue, out_mail_parts);

		g_string_truncate (part_id, len);
	}

	camel_cipher_validity_free (valid);

	g_object_unref (cipher);

	return TRUE;
}

static void
e_mail_parser_multipart_signed_class_init (EMailParserExtensionClass *class)
{
	class->mime_types = parser_mime_types;
	class->priority = G_PRIORITY_LOW;
	class->parse = empe_mp_signed_parse;
}

static void
e_mail_parser_multipart_signed_init (EMailParserExtension *extension)
{
}
