/*
 * e-mail-parser-multipart-encrypted.c
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

typedef EMailParserExtension EMailParserMultipartEncrypted;
typedef EMailParserExtensionClass EMailParserMultipartEncryptedClass;

GType e_mail_parser_multipart_encrypted_get_type (void);

G_DEFINE_TYPE (
	EMailParserMultipartEncrypted,
	e_mail_parser_multipart_encrypted,
	E_TYPE_MAIL_PARSER_EXTENSION)

static const gchar *parser_mime_types[] = {
	"multipart/encrypted",
	NULL
};

static gboolean
empe_mp_encrypted_parse (EMailParserExtension *extension,
                         EMailParser *parser,
                         CamelMimePart *part,
                         GString *part_id,
                         GCancellable *cancellable,
                         GQueue *out_mail_parts)
{
	CamelCipherContext *context;
	const gchar *protocol;
	CamelMimePart *opart;
	CamelCipherValidity *valid;
	CamelContentType *content_type;
	CamelMultipartEncrypted *mpe;
	GQueue work_queue = G_QUEUE_INIT;
	GList *head, *link;
	GError *local_error = NULL;
	gint len;

	content_type = camel_mime_part_get_content_type (part);

	/* When it's a guessed type, then rather not interpret it as an encrypted message */
	if (g_strcmp0 (camel_content_type_param (content_type, E_MAIL_PART_X_EVOLUTION_GUESSED), "1") == 0) {
		e_mail_parser_wrap_as_non_expandable_attachment (parser, part, part_id, out_mail_parts);
		return TRUE;
	}

	mpe = (CamelMultipartEncrypted *) camel_medium_get_content ((CamelMedium *) part);
	if (!CAMEL_IS_MULTIPART_ENCRYPTED (mpe)) {
		e_mail_parser_error (
			parser, out_mail_parts,
			_("Could not parse MIME message. "
			"Displaying as source."));
		e_mail_parser_parse_part_as (
			parser, part, part_id,
			"application/vnd.evolution/source",
			cancellable, out_mail_parts);

		return TRUE;
	}

	/* Currently we only handle RFC2015-style PGP encryption. */
	protocol = camel_content_type_param (camel_data_wrapper_get_mime_type_field (CAMEL_DATA_WRAPPER (mpe)), "protocol");
	if (!protocol || g_ascii_strcasecmp (protocol, "application/pgp-encrypted") != 0) {
		e_mail_parser_error (
			parser, out_mail_parts,
			_("Unsupported encryption type for multipart/encrypted"));
		e_mail_parser_parse_part_as (
			parser, part, part_id, "multipart/mixed",
			cancellable, out_mail_parts);

		return TRUE;
	}

	context = camel_gpg_context_new (e_mail_parser_get_session (parser));

	opart = camel_mime_part_new ();
	valid = camel_cipher_context_decrypt_sync (
		context, part, opart, cancellable, &local_error);

	e_mail_part_preserve_charset_in_content_type (part, opart);

	if (local_error != NULL) {
		e_mail_parser_error (
			parser, out_mail_parts,
			_("Could not parse PGP/MIME message: %s"),
			local_error->message);
		e_mail_parser_parse_part_as (
			parser, part, part_id, "multipart/mixed",
			cancellable, out_mail_parts);

		g_object_unref (opart);
		g_object_unref (context);
		g_error_free (local_error);

		return TRUE;
	}

	len = part_id->len;
	g_string_append (part_id, ".encrypted-pgp");

	e_mail_parser_utils_check_protected_headers (parser, opart, cancellable);

	g_warn_if_fail (e_mail_parser_parse_part (
		parser, opart, part_id, cancellable, &work_queue));

	g_string_truncate (part_id, len);

	head = g_queue_peek_head_link (&work_queue);

	/* Update validity of all encrypted sub-parts */
	for (link = head; link != NULL; link = g_list_next (link)) {
		EMailPart *mail_part = link->data;

		e_mail_part_update_validity (
			mail_part, valid,
			E_MAIL_PART_VALIDITY_ENCRYPTED |
			E_MAIL_PART_VALIDITY_PGP);

		/* Do not traverse sub-messages */
		if (g_str_has_suffix (e_mail_part_get_id (mail_part), ".rfc822"))
			link = e_mail_formatter_find_rfc822_end_iter (link);
	}

	e_queue_transfer (&work_queue, out_mail_parts);

	/* Add a widget with details about the encryption, but only when
	 * the decrypted part isn't itself secured, in that case it has
	 * created the button itself. */
	if (!e_mail_part_is_secured (opart)) {
		EMailPart *mail_part;

		g_string_append (part_id, ".encrypted-pgp.button");

		e_mail_parser_parse_part_as (
			parser, part, part_id,
			"application/vnd.evolution.secure-button",
			cancellable, &work_queue);

		mail_part = g_queue_peek_head (&work_queue);

		if (mail_part != NULL)
			e_mail_part_update_validity (
				mail_part, valid,
				E_MAIL_PART_VALIDITY_ENCRYPTED |
				E_MAIL_PART_VALIDITY_PGP);

		e_queue_transfer (&work_queue, out_mail_parts);

		g_string_truncate (part_id, len);
	}

	camel_cipher_validity_free (valid);

	/* TODO: Make sure when we finalize this part, it is zero'd out */
	g_object_unref (opart);
	g_object_unref (context);

	return TRUE;
}

static void
e_mail_parser_multipart_encrypted_class_init (EMailParserExtensionClass *class)
{
	class->mime_types = parser_mime_types;
	class->priority = G_PRIORITY_LOW;
	class->parse = empe_mp_encrypted_parse;
}

static void
e_mail_parser_multipart_encrypted_init (EMailParserExtension *extension)
{
}
