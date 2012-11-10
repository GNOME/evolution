/*
 * e-mail-parser-multipart-signed.c
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

typedef struct _EMailParserMultipartSigned {
	GObject parent;
} EMailParserMultipartSigned;

typedef struct _EMailParserMultipartSignedClass {
	GObjectClass parent_class;
} EMailParserMultipartSignedClass;

static void e_mail_parser_parser_extension_interface_init (EMailParserExtensionInterface *iface);
static void e_mail_parser_mail_extension_interface_init (EMailExtensionInterface *iface);

G_DEFINE_TYPE_EXTENDED (
	EMailParserMultipartSigned,
	e_mail_parser_multipart_signed,
	G_TYPE_OBJECT,
	0,
	G_IMPLEMENT_INTERFACE (
		E_TYPE_MAIL_EXTENSION,
		e_mail_parser_mail_extension_interface_init)
	G_IMPLEMENT_INTERFACE (
		E_TYPE_MAIL_PARSER_EXTENSION,
		e_mail_parser_parser_extension_interface_init));

static const gchar * parser_mime_types[] = { "multipart/signed",
					    "application/pgp-signature",
					    NULL };

static GSList *
empe_mp_signed_parse (EMailParserExtension *extension,
                      EMailParser *parser,
                      CamelMimePart *part,
                      GString *part_id,
                      GCancellable *cancellable)
{
	CamelMimePart *cpart;
	CamelMultipartSigned *mps;
	CamelCipherContext *cipher = NULL;
	CamelSession *session;
	guint32 validity_type;
	GSList *parts;
	CamelCipherValidity *valid;
	GError *local_error = NULL;
	gint i, nparts, len;
	gboolean secured;

	if (g_cancellable_is_cancelled (cancellable))
		return NULL;

	/* If the part is application/pgp-signature sub-part then skip it. */
	if (!CAMEL_IS_MULTIPART (part)) {
		CamelContentType *ct;
		ct = camel_mime_part_get_content_type (CAMEL_MIME_PART (part));
		if (camel_content_type_is (ct, "application", "pgp-signature")) {
			return g_slist_alloc ();
		}
	}

	mps = (CamelMultipartSigned *) camel_medium_get_content ((CamelMedium *) part);
	if (!CAMEL_IS_MULTIPART_SIGNED (mps)
		|| (
		cpart = camel_multipart_get_part (
			(CamelMultipart *) mps,
		CAMEL_MULTIPART_SIGNED_CONTENT)) == NULL) {
		parts = e_mail_parser_error (
			parser, cancellable,
			_("Could not parse MIME message. "
			"Displaying as source."));

		parts = g_slist_concat (
			parts,
			e_mail_parser_parse_part_as (
				parser, part, part_id,
				"application/vnd.evolution.source",
				cancellable));
		return parts;
	}

	session = e_mail_parser_get_session (parser);
	/* FIXME: Should be done via a plugin interface */
	/* FIXME: duplicated in em-format-html-display.c */
	if (mps->protocol) {
#ifdef ENABLE_SMIME
		if (g_ascii_strcasecmp ("application/x-pkcs7-signature", mps->protocol) == 0
		    || g_ascii_strcasecmp ("application/pkcs7-signature", mps->protocol) == 0) {
			cipher = camel_smime_context_new (session);
			validity_type = E_MAIL_PART_VALIDITY_SMIME;
		} else {
#endif
			if (g_ascii_strcasecmp ("application/pgp-signature", mps->protocol) == 0) {
				cipher = camel_gpg_context_new (session);
				validity_type = E_MAIL_PART_VALIDITY_PGP;
			}
#ifdef ENABLE_SMIME
		}
#endif
	}

	if (cipher == NULL) {
		parts = e_mail_parser_error (
			parser, cancellable,
			_("Unsupported signature format"));

		parts = g_slist_concat (
			parts,
			e_mail_parser_parse_part_as (
				parser, part, part_id,
				"multipart/mixed", cancellable));

		return parts;
	}

	valid = camel_cipher_context_verify_sync (
		cipher, part, cancellable, &local_error);

	if (local_error != NULL) {
		parts = e_mail_parser_error (
			parser, cancellable,
			_("Error verifying signature: %s"),
			local_error->message);

		g_error_free (local_error);

		parts = g_slist_concat (
			parts,
			e_mail_parser_parse_part_as (
				parser, part, part_id,
				"multipart/mixed", cancellable));

		g_object_unref (cipher);
		return parts;
	}

	nparts = camel_multipart_get_number (CAMEL_MULTIPART (mps));
	secured = FALSE;
	len = part_id->len;
	parts = NULL;
	for (i = 0; i < nparts; i++) {
		CamelMimePart *subpart;
		GSList *mail_parts, *iter;
		subpart = camel_multipart_get_part (CAMEL_MULTIPART (mps), i);

		g_string_append_printf (part_id, ".signed.%d", i);

		mail_parts = e_mail_parser_parse_part (
			parser, subpart, part_id, cancellable);

		g_string_truncate (part_id, len);

		if (!secured)
			secured = e_mail_part_is_secured (subpart);

		for (iter = mail_parts; iter; iter = iter->next) {
			EMailPart *mail_part;

			mail_part = iter->data;
			if (!mail_part)
				continue;

			e_mail_part_update_validity (
				mail_part, valid,
				validity_type | E_MAIL_PART_VALIDITY_SIGNED);
		}

		parts = g_slist_concat (parts, mail_parts);
	}

	/* Add a widget with details about the encryption, but only when
		* the encrypted isn't itself secured, in that case it has created
		* the button itself */
	if (!secured) {
		GSList *button;
		EMailPart *mail_part;
		g_string_append (part_id, ".signed.button");

		button = e_mail_parser_parse_part_as (
			parser, part, part_id,
			"application/vnd.evolution.widget.secure-button",
			cancellable);
		if (button && button->data) {
			mail_part = button->data;

			e_mail_part_update_validity (
				mail_part, valid,
				validity_type | E_MAIL_PART_VALIDITY_SIGNED);
		}

		parts = g_slist_concat (parts, button);

		g_string_truncate (part_id, len);
	}

	camel_cipher_validity_free (valid);

	g_object_unref (cipher);

	return parts;
}

static const gchar **
empe_mp_signed_mime_types (EMailExtension *extension)
{
	return parser_mime_types;
}

static void
e_mail_parser_multipart_signed_class_init (EMailParserMultipartSignedClass *class)
{
}

static void
e_mail_parser_parser_extension_interface_init (EMailParserExtensionInterface *iface)
{
	iface->parse = empe_mp_signed_parse;
}

static void
e_mail_parser_mail_extension_interface_init (EMailExtensionInterface *iface)
{
	iface->mime_types = empe_mp_signed_mime_types;
}

static void
e_mail_parser_multipart_signed_init (EMailParserMultipartSigned *parser)
{
}
