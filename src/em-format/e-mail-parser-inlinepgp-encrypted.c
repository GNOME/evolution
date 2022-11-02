/*
 * e-mail-parser-inlinepgp-encrypted.c
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

typedef EMailParserExtension EMailParserInlinePGPEncrypted;
typedef EMailParserExtensionClass EMailParserInlinePGPEncryptedClass;

GType e_mail_parser_inline_pgp_encrypted_get_type (void);

G_DEFINE_TYPE (
	EMailParserInlinePGPEncrypted,
	e_mail_parser_inline_pgp_encrypted,
	E_TYPE_MAIL_PARSER_EXTENSION)

static const gchar *parser_mime_types[] = {
	"application/x-inlinepgp-encrypted",
	NULL
};

static gboolean
empe_inlinepgp_encrypted_parse (EMailParserExtension *extension,
                                EMailParser *parser,
                                CamelMimePart *part,
                                GString *part_id,
                                GCancellable *cancellable,
                                GQueue *out_mail_parts)
{
	CamelCipherContext *cipher;
	CamelCipherValidity *valid;
	CamelMimePart *opart;
	CamelDataWrapper *dw;
	gchar *mime_type;
	gint len;
	GQueue work_queue = G_QUEUE_INIT;
	GList *head, *link;
	GError *local_error = NULL;

	if (g_cancellable_is_cancelled (cancellable))
		return FALSE;

	cipher = camel_gpg_context_new (e_mail_parser_get_session (parser));

	opart = camel_mime_part_new ();

	/* Decrypt the message */
	valid = camel_cipher_context_decrypt_sync (
		cipher, part, opart, cancellable, &local_error);

	if (local_error != NULL) {
		e_mail_parser_error (
			parser, out_mail_parts,
			_("Could not parse PGP message: %s"),
			local_error->message);
		g_error_free (local_error);

		e_mail_parser_parse_part_as (
			parser,
			part, part_id,
			"application/vnd.evolution.source",
			cancellable, out_mail_parts);

		g_object_unref (cipher);
		g_object_unref (opart);

		return TRUE;
	}

	dw = camel_medium_get_content ((CamelMedium *) opart);
	mime_type = camel_data_wrapper_get_mime_type (dw);

	/* this ensures to show the 'opart' as inlined, if possible */
	if (mime_type && g_ascii_strcasecmp (mime_type, "application/octet-stream") == 0) {
		gchar *guessed_mime_type;

		guessed_mime_type = e_mail_part_guess_mime_type (opart);

		if (guessed_mime_type) {
			camel_data_wrapper_set_mime_type (dw, guessed_mime_type);

			/* Set the MIME type on the 'opart' itself as well.
			 * If it's "text/plain", then we want the TextPlain
			 * parser extension to treat it as "text/plain" and
			 * NOT wrap it as an attachment. */
			camel_data_wrapper_set_mime_type (CAMEL_DATA_WRAPPER (opart), guessed_mime_type);

			g_free (guessed_mime_type);
		}
	}

	e_mail_part_preserve_charset_in_content_type (part, opart);
	g_free (mime_type);

	/* Pass it off to the real formatter */
	len = part_id->len;
	g_string_append (part_id, ".inlinepgp_encrypted");

	mime_type = camel_data_wrapper_get_mime_type (dw);

	g_warn_if_fail (e_mail_parser_parse_part_as (
		parser, opart, part_id, mime_type,
		cancellable, &work_queue));

	g_free (mime_type);

	g_string_truncate (part_id, len);

	head = g_queue_peek_head_link (&work_queue);

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
	 * the encrypted isn't itself secured, in that case it has created
	 * the button itself */
	if (!e_mail_part_is_secured (opart)) {
		EMailPart *mail_part;

		g_string_append (part_id, ".inlinepgp_encrypted.button");

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

	/* Clean Up */
	camel_cipher_validity_free (valid);
	g_object_unref (opart);
	g_object_unref (cipher);

	return TRUE;
}

static void
e_mail_parser_inline_pgp_encrypted_class_init (EMailParserExtensionClass *class)
{
	class->mime_types = parser_mime_types;
	class->priority = G_PRIORITY_LOW;
	class->parse = empe_inlinepgp_encrypted_parse;
}

static void
e_mail_parser_inline_pgp_encrypted_init (EMailParserExtension *extension)
{
}
