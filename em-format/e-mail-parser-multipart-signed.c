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
#include <libedataserver/libedataserver.h>

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

static gboolean
empe_mp_signed_parse (EMailParserExtension *extension,
                      EMailParser *parser,
                      CamelMimePart *part,
                      GString *part_id,
                      GCancellable *cancellable,
                      GQueue *out_mail_parts)
{
	CamelMimePart *cpart;
	CamelMultipartSigned *mps;
	CamelCipherContext *cipher = NULL;
	CamelSession *session;
	guint32 validity_type;
	CamelCipherValidity *valid;
	GError *local_error = NULL;
	gint i, nparts, len;
	gboolean secured;

	/* If the part is application/pgp-signature sub-part then skip it. */
	if (!CAMEL_IS_MULTIPART (part)) {
		CamelContentType *ct;
		ct = camel_mime_part_get_content_type (CAMEL_MIME_PART (part));
		if (camel_content_type_is (ct, "application", "pgp-signature")) {
			return TRUE;
		}
	}

	mps = (CamelMultipartSigned *) camel_medium_get_content ((CamelMedium *) part);
	if (!CAMEL_IS_MULTIPART_SIGNED (mps)
		|| (
		cpart = camel_multipart_get_part (
			(CamelMultipart *) mps,
		CAMEL_MULTIPART_SIGNED_CONTENT)) == NULL) {
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

	nparts = camel_multipart_get_number (CAMEL_MULTIPART (mps));
	secured = FALSE;
	len = part_id->len;
	for (i = 0; i < nparts; i++) {
		GQueue work_queue = G_QUEUE_INIT;
		GList *head, *link;
		CamelMimePart *subpart;

		subpart = camel_multipart_get_part (CAMEL_MULTIPART (mps), i);

		g_string_append_printf (part_id, ".signed.%d", i);

		e_mail_parser_parse_part (
			parser, subpart, part_id, cancellable, &work_queue);

		g_string_truncate (part_id, len);

		if (!secured)
			secured = e_mail_part_is_secured (subpart);

		head = g_queue_peek_head_link (&work_queue);

		for (link = head; link != NULL; link = g_list_next (link)) {
			EMailPart *mail_part = link->data;

			e_mail_part_update_validity (
				mail_part, valid,
				validity_type | E_MAIL_PART_VALIDITY_SIGNED);
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
			"application/vnd.evolution.widget.secure-button",
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
