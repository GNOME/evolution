/*
 * e-mail-parser-multipart-encrypted.c
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

#include <glib/gi18n-lib.h>
#include <camel/camel.h>

typedef struct _EMailParserMultipartEncrypted {
	GObject parent;
} EMailParserMultipartEncrypted;

typedef struct _EMailParserMultipartEncryptedClass {
	GObjectClass parent_class;
} EMailParserMultipartEncryptedClass;

static void e_mail_parser_parser_extension_interface_init (EMailParserExtensionInterface *iface);
static void e_mail_parser_mail_extension_interface_init (EMailExtensionInterface *iface);

G_DEFINE_TYPE_EXTENDED (
	EMailParserMultipartEncrypted,
	e_mail_parser_multipart_encrypted,
	G_TYPE_OBJECT,
	0,
	G_IMPLEMENT_INTERFACE (
		E_TYPE_MAIL_EXTENSION,
		e_mail_parser_mail_extension_interface_init)
	G_IMPLEMENT_INTERFACE (
		E_TYPE_MAIL_PARSER_EXTENSION,
		e_mail_parser_parser_extension_interface_init))

static const gchar * parser_mime_types[] = { "multipart/encrypted", NULL };

static GSList *
empe_mp_encrypted_parse (EMailParserExtension *extension,
                         EMailParser *parser,
                         CamelMimePart *part,
                         GString *part_id,
                         GCancellable *cancellable)
{
	CamelCipherContext *context;
	const gchar *protocol;
	CamelMimePart *opart;
	CamelCipherValidity *valid;
	CamelMultipartEncrypted *mpe;
	GError *local_error = NULL;
	GSList *parts;
	gint len;
	GSList *iter;

	if (g_cancellable_is_cancelled (cancellable))
		return NULL;

	mpe = (CamelMultipartEncrypted *) camel_medium_get_content ((CamelMedium *) part);
	if (!CAMEL_IS_MULTIPART_ENCRYPTED (mpe)) {
		parts = e_mail_parser_error (
				parser, cancellable,
				_("Could not parse MIME message. "
				  "Displaying as source."));
		parts = g_slist_concat (
				parts,
				e_mail_parser_parse_part_as (
					parser, part, part_id,
					"application/vnd.evolution/source",
					cancellable));

		return parts;
	}

	/* Currently we only handle RFC2015-style PGP encryption. */
	protocol = camel_content_type_param (
		((CamelDataWrapper *) mpe)->mime_type, "protocol");
	if (!protocol || g_ascii_strcasecmp (protocol, "application/pgp-encrypted") != 0) {
		parts = e_mail_parser_error (
				parser, cancellable,
			       _("Unsupported encryption type for multipart/encrypted"));

		parts = g_slist_concat (
				parts,
				e_mail_parser_parse_part_as (
					parser, part, part_id,
					"multipart/mixed", cancellable));
		return parts;
	}

	context = camel_gpg_context_new (e_mail_parser_get_session (parser));

	opart = camel_mime_part_new ();
	valid = camel_cipher_context_decrypt_sync (
			context, part, opart, cancellable, &local_error);

	e_mail_part_preserve_charset_in_content_type (part, opart);
	if (valid == NULL) {
		parts = e_mail_parser_error (
				parser, cancellable,
				_("Could not parse PGP/MIME message: %s"),
				local_error->message ?
					local_error->message :
					_("Unknown error"));

		g_clear_error (&local_error);

		parts = g_slist_concat (parts,
				e_mail_parser_parse_part_as (
					parser, part, part_id,
					"multipart/mixed", cancellable));

		g_object_unref (opart);
		g_object_unref (context);

		return parts;
	}

	len = part_id->len;
	g_string_append (part_id, ".encrypted");

	parts = e_mail_parser_parse_part (
			parser, opart, part_id, cancellable);

	g_string_truncate (part_id, len);

	/* Update validity of all encrypted sub-parts */
	for (iter = parts; iter; iter = iter->next) {
		EMailPart *mail_part;

		mail_part = iter->data;
		if (!mail_part)
			continue;

		e_mail_part_update_validity (mail_part, valid,
			E_MAIL_PART_VALIDITY_ENCRYPTED |
			E_MAIL_PART_VALIDITY_PGP);
	}

	/* Add a widget with details about the encryption, but only when
		* the decrypted part isn't itself secured, in that case it has created
		* the button itself */
	if (!e_mail_part_is_secured (opart)) {
		GSList *button;
		EMailPart *mail_part;
		g_string_append (part_id, ".encrypted.button");

		button = e_mail_parser_parse_part_as (
				parser, part, part_id,
				"application/vnd.evolution.widget.secure-button",
				cancellable);
		if (button && button->data) {
			mail_part = button->data;

			e_mail_part_update_validity (mail_part, valid,
				E_MAIL_PART_VALIDITY_ENCRYPTED |
				E_MAIL_PART_VALIDITY_PGP);
		}

		parts = g_slist_concat (parts, button);

		g_string_truncate (part_id, len);
	}

	camel_cipher_validity_free (valid);

	/* TODO: Make sure when we finalize this part, it is zero'd out */
	g_object_unref (opart);
	g_object_unref (context);

	return parts;
}

static const gchar **
empe_mp_encrypted_mime_types (EMailExtension *extension)
{
	return parser_mime_types;
}

static void
e_mail_parser_multipart_encrypted_class_init (EMailParserMultipartEncryptedClass *class)
{
}

static void
e_mail_parser_parser_extension_interface_init (EMailParserExtensionInterface *iface)
{
	iface->parse = empe_mp_encrypted_parse;
}

static void
e_mail_parser_mail_extension_interface_init (EMailExtensionInterface *iface)
{
	iface->mime_types = empe_mp_encrypted_mime_types;
}

static void
e_mail_parser_multipart_encrypted_init (EMailParserMultipartEncrypted *parser)
{

}
