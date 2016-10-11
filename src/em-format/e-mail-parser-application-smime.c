/*
 * e-mail-parser-application-xpkcs7mime.c
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

#include "e-mail-parser-extension.h"
#include "e-mail-part-utils.h"

typedef EMailParserExtension EMailParserApplicationSMIME;
typedef EMailParserExtensionClass EMailParserApplicationSMIMEClass;

GType e_mail_parser_application_smime_get_type (void);

G_DEFINE_TYPE (
	EMailParserApplicationSMIME,
	e_mail_parser_application_smime,
	E_TYPE_MAIL_PARSER_EXTENSION)

static const gchar *parser_mime_types[] = {
	"application/xpkcs7mime",
	"application/x-pkcs7-mime",
	"application/pkcs7-mime",
	"application/pkcs7-signature",
	"application/xpkcs7-signature",
	"application/x-pkcs7-signature",
	NULL
};

static gboolean
empe_app_smime_parse (EMailParserExtension *extension,
                      EMailParser *parser,
                      CamelMimePart *part,
                      GString *part_id,
                      GCancellable *cancellable,
                      GQueue *out_mail_parts)
{
	CamelCipherContext *context;
	CamelMimePart *opart;
	CamelCipherValidity *valid;
	GError *local_error = NULL;
	CamelContentType *ct;

	ct = camel_mime_part_get_content_type (part);
	if (camel_content_type_is (ct, "application", "pkcs7-signature") ||
	    camel_content_type_is (ct, "application", "xpkcs7-signature") ||
	    camel_content_type_is (ct, "application", "x-pkcs7-signature")) {
		return TRUE;
	}

	context = camel_smime_context_new (e_mail_parser_get_session (parser));

	opart = camel_mime_part_new ();
	valid = camel_cipher_context_decrypt_sync (
		context, part, opart,
		cancellable, &local_error);

	e_mail_part_preserve_charset_in_content_type (part, opart);

	if (local_error != NULL) {
		e_mail_parser_error (
			parser, out_mail_parts,
			_("Could not parse S/MIME message: %s"),
			local_error->message);
		g_error_free (local_error);

	} else {
		GQueue work_queue = G_QUEUE_INIT;
		GList *head, *link;
		gint len = part_id->len;

		g_string_append (part_id, ".encrypted");

		e_mail_parser_parse_part (
			parser, opart, part_id, cancellable, &work_queue);

		g_string_truncate (part_id, len);

		head = g_queue_peek_head_link (&work_queue);

		/* Update validity flags of all the involved subp-arts */
		for (link = head; link != NULL; link = g_list_next (link)) {
			EMailPart *mail_part = link->data;

			e_mail_part_update_validity (
				mail_part, valid,
				E_MAIL_PART_VALIDITY_ENCRYPTED |
				E_MAIL_PART_VALIDITY_SMIME);
		}

		e_queue_transfer (&work_queue, out_mail_parts);

		/* Add a widget with details about the encryption, but only
		 * when the encrypted isn't itself secured, in that case it
		 * has created the button itself. */
		if (!e_mail_part_is_secured (opart)) {
			EMailPart *mail_part;

			g_string_append (part_id, ".encrypted.button");

			e_mail_parser_parse_part_as (
				parser, part, part_id,
				"application/vnd.evolution.secure-button",
				cancellable, &work_queue);

			mail_part = g_queue_peek_head (&work_queue);

			if (mail_part != NULL)
				e_mail_part_update_validity (
					mail_part, valid,
					E_MAIL_PART_VALIDITY_ENCRYPTED |
					E_MAIL_PART_VALIDITY_SMIME);

			e_queue_transfer (&work_queue, out_mail_parts);

			g_string_truncate (part_id, len);
		}

		camel_cipher_validity_free (valid);
	}

	g_object_unref (opart);
	g_object_unref (context);

	return TRUE;
}

static void
e_mail_parser_application_smime_class_init (EMailParserExtensionClass *class)
{
	class->mime_types = parser_mime_types;
	class->priority = G_PRIORITY_LOW;
	class->flags = E_MAIL_PARSER_EXTENSION_INLINE;
	class->parse = empe_app_smime_parse;
}

static void
e_mail_parser_application_smime_init (EMailParserExtension *extension)
{
}
