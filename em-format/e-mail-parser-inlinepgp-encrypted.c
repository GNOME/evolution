/*
 * e-mail-parser-inlinepgp-encrypted.c
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

typedef struct _EMailParserInlinePGPEncrypted {
	GObject parent;
} EMailParserInlinePGPEncrypted;

typedef struct _EMailParserInlinePGPEncryptedClass {
	GObjectClass parent_class;
} EMailParserInlinePGPEncryptedClass;

static void e_mail_parser_parser_extension_interface_init (EMailParserExtensionInterface *iface);
static void e_mail_parser_mail_extension_interface_init (EMailExtensionInterface *iface);

G_DEFINE_TYPE_EXTENDED (
	EMailParserInlinePGPEncrypted,
	e_mail_parser_inline_pgp_encrypted,
	G_TYPE_OBJECT,
	0,
	G_IMPLEMENT_INTERFACE (
		E_TYPE_MAIL_EXTENSION,
		e_mail_parser_mail_extension_interface_init)
	G_IMPLEMENT_INTERFACE (
		E_TYPE_MAIL_PARSER_EXTENSION,
		e_mail_parser_parser_extension_interface_init));

static const gchar * parser_mime_types[] = { "application/x-inlinepgp-encrypted",
					    NULL };

static GSList *
empe_inlinepgp_encrypted_parse (EMailParserExtension *extension,
                                EMailParser *parser,
                                CamelMimePart *part,
                                GString *part_id,
                                GCancellable *cancellable)
{
	CamelCipherContext *cipher;
	CamelCipherValidity *valid;
	CamelMimePart *opart;
	CamelDataWrapper *dw;
	gchar *mime_type;
	gint len;
	GError *local_error = NULL;
	GSList *parts, *iter;

	if (g_cancellable_is_cancelled (cancellable))
		return NULL;

	cipher = camel_gpg_context_new (e_mail_parser_get_session (parser));

	opart = camel_mime_part_new ();

	/* Decrypt the message */
	valid = camel_cipher_context_decrypt_sync (
		cipher, part, opart, cancellable, &local_error);

	if (!valid) {
		parts = e_mail_parser_error (
				parser, cancellable,
				_("Could not parse PGP message: %s"),
				local_error->message ?
					local_error->message :
					_("Unknown error"));
		g_clear_error (&local_error);

		parts = g_slist_concat (parts,
				e_mail_parser_parse_part_as (parser,
					part, part_id,
					"application/vnd.evolution.source",
					cancellable));

		g_object_unref (cipher);
		g_object_unref (opart);
		return parts;
	}

	dw = camel_medium_get_content ((CamelMedium *) opart);
	mime_type = camel_data_wrapper_get_mime_type (dw);

	/* this ensures to show the 'opart' as inlined, if possible */
	if (mime_type && g_ascii_strcasecmp (mime_type, "application/octet-stream") == 0) {
		const gchar *snoop = e_mail_part_snoop_type (opart);

		if (snoop)
			camel_data_wrapper_set_mime_type (dw, snoop);
	}

	e_mail_part_preserve_charset_in_content_type (part, opart);
	g_free (mime_type);

	/* Pass it off to the real formatter */
	len = part_id->len;
	g_string_append (part_id, ".inlinepgp_encrypted");

	parts = e_mail_parser_parse_part_as (
			parser, opart, part_id,
			camel_data_wrapper_get_mime_type (dw), cancellable);

	g_string_truncate (part_id, len);

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
	 * the encrypted isn't itself secured, in that case it has created
	 * the button itself */
	if (!e_mail_part_is_secured (opart)) {
		GSList *button;
		EMailPart *mail_part;
		g_string_append (part_id, ".inlinepgp_encrypted.button");

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

	/* Clean Up */
	camel_cipher_validity_free (valid);
	g_object_unref (opart);
	g_object_unref (cipher);

	return parts;
}

static const gchar **
empe_inlinepgp_encrypted_mime_types (EMailExtension *extension)
{
	return parser_mime_types;
}

static void
e_mail_parser_inline_pgp_encrypted_class_init (EMailParserInlinePGPEncryptedClass *class)
{
}

static void
e_mail_parser_parser_extension_interface_init (EMailParserExtensionInterface *iface)
{
	iface->parse = empe_inlinepgp_encrypted_parse;
}

static void
e_mail_parser_mail_extension_interface_init (EMailExtensionInterface *iface)
{
	iface->mime_types = empe_inlinepgp_encrypted_mime_types;
}

static void
e_mail_parser_inline_pgp_encrypted_init (EMailParserInlinePGPEncrypted *parser)
{

}
