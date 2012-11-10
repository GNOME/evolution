/*
 * e-mail-parser-application-xpkcs7mime.c
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

#include <glib-object.h>
#include <glib/gi18n-lib.h>

#include <em-format/e-mail-parser-extension.h>
#include <em-format/e-mail-parser.h>
#include <em-format/e-mail-part-utils.h>
#include <e-util/e-util.h>

#include <camel/camel.h>

#include <string.h>

typedef struct _EMailParserApplicationSMIME {
	GObject parent;
} EMailParserApplicationSMIME;

typedef struct _EMailParserAppplicationSMIMEClass {
	GObjectClass parent_class;
} EMailParserApplicationSMIMEClass;

static void e_mail_parser_parser_extension_interface_init (EMailParserExtensionInterface *iface);
static void e_mail_parser_mail_extension_interface_init (EMailExtensionInterface *iface);

G_DEFINE_TYPE_EXTENDED (
	EMailParserApplicationSMIME,
	e_mail_parser_application_smime,
	G_TYPE_OBJECT,
	0,
	G_IMPLEMENT_INTERFACE (
		E_TYPE_MAIL_EXTENSION,
		e_mail_parser_mail_extension_interface_init)
	G_IMPLEMENT_INTERFACE (
		E_TYPE_MAIL_PARSER_EXTENSION,
		e_mail_parser_parser_extension_interface_init));

static const gchar * parser_mime_types[] = { "application/xpkcs7mime",
					    "application/x-pkcs7-mime",
					    "application/pkcs7-mime",
					    "application/pkcs7-signature",
					    "application/xpkcs7-signature",
					    "application/x-pkcs7-signature",
					    NULL };

static GSList *
empe_app_smime_parse (EMailParserExtension *extension,
                       EMailParser *parser,
                       CamelMimePart *part,
                       GString *part_id,
                       GCancellable *cancellable)
{
	CamelCipherContext *context;
	CamelMimePart *opart;
	CamelCipherValidity *valid;
	GError *local_error = NULL;
	GSList *parts, *iter;
	CamelContentType *ct;

	if (g_cancellable_is_cancelled (cancellable))
		return NULL;

	ct = camel_mime_part_get_content_type (part);
	if (camel_content_type_is (ct, "application", "pkcs7-signature") ||
	    camel_content_type_is (ct, "application", "xpkcs7-signature") ||
	    camel_content_type_is (ct, "application", "x-pkcs7-signature")) {
		return g_slist_alloc ();
	}

	context = camel_smime_context_new (e_mail_parser_get_session (parser));

	opart = camel_mime_part_new ();
	valid = camel_cipher_context_decrypt_sync (
		context, part, opart,
		cancellable, &local_error);

	e_mail_part_preserve_charset_in_content_type (part, opart);

	if (local_error != NULL) {
		parts = e_mail_parser_error (
			parser, cancellable,
			_("Could not parse S/MIME message: %s"),
			local_error->message);
		g_error_free (local_error);

	} else {
		gint len = part_id->len;

		g_string_append (part_id, ".encrypted");

		parts = e_mail_parser_parse_part (
			parser, opart, part_id, cancellable);

		g_string_truncate (part_id, len);

		/* Update validity flags of all the involved subp-arts */
		for (iter = parts; iter; iter = iter->next) {

			EMailPart *mail_part = iter->data;
			if (!mail_part)
				continue;

			e_mail_part_update_validity (
				mail_part, valid,
				E_MAIL_PART_VALIDITY_ENCRYPTED |
				E_MAIL_PART_VALIDITY_SMIME);

		}

		/* Add a widget with details about the encryption, but only when
		 * the encrypted isn't itself secured, in that case it has created
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

				e_mail_part_update_validity (
					mail_part, valid,
					E_MAIL_PART_VALIDITY_ENCRYPTED |
					E_MAIL_PART_VALIDITY_SMIME);
			}

			parts = g_slist_concat (parts, button);

			g_string_truncate (part_id, len);
		}

		camel_cipher_validity_free (valid);
	}

	g_object_unref (opart);
	g_object_unref (context);

	return parts;
}

static guint32
empe_app_smime_get_flags (EMailParserExtension *extension)
{
	return E_MAIL_PARSER_EXTENSION_INLINE;
}

static const gchar **
empe_application_smime_mime_types (EMailExtension *extension)
{
	return parser_mime_types;
}

static void
e_mail_parser_application_smime_class_init (EMailParserApplicationSMIMEClass *class)
{
}

static void
e_mail_parser_parser_extension_interface_init (EMailParserExtensionInterface *interface)
{
	interface->parse = empe_app_smime_parse;
	interface->get_flags = empe_app_smime_get_flags;
}

static void
e_mail_parser_mail_extension_interface_init (EMailExtensionInterface *interface)
{
	interface->mime_types = empe_application_smime_mime_types;
}

static void
e_mail_parser_application_smime_init (EMailParserApplicationSMIME *parser)
{

}
