/*
 * e-mail-parser-multipart-digest.c
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
#include <e-util/e-util.h>

#include <camel/camel.h>

#include <string.h>

typedef struct _EMailParserMultipartDigest {
	GObject parent;
} EMailParserMultipartDigest;

typedef struct _EMailParserMultipartDigestClass {
	GObjectClass parent_class;
} EMailParserMultipartDigestClass;

static void e_mail_parser_parser_extension_interface_init (EMailParserExtensionInterface *iface);
static void e_mail_parser_mail_extension_interface_init (EMailExtensionInterface *iface);

G_DEFINE_TYPE_EXTENDED (
	EMailParserMultipartDigest,
	e_mail_parser_multipart_digest,
	G_TYPE_OBJECT,
	0,
	G_IMPLEMENT_INTERFACE (
		E_TYPE_MAIL_EXTENSION,
		e_mail_parser_mail_extension_interface_init)
	G_IMPLEMENT_INTERFACE (
		E_TYPE_MAIL_PARSER_EXTENSION,
		e_mail_parser_parser_extension_interface_init));

static const gchar * parser_mime_types[] = { "multipart/digest",
					    NULL };

static gboolean
empe_mp_digest_parse (EMailParserExtension *extension,
                      EMailParser *parser,
                      CamelMimePart *part,
                      GString *part_id,
                      GCancellable *cancellable,
                      GQueue *out_mail_parts)
{
	CamelMultipart *mp;
	gint i, nparts, len;

	mp = (CamelMultipart *) camel_medium_get_content ((CamelMedium *) part);

	if (!CAMEL_IS_MULTIPART (mp))
		return e_mail_parser_parse_part_as (
			parser, part, part_id,
			"application/vnd.evolution.source",
			cancellable, out_mail_parts);

	len = part_id->len;
	nparts = camel_multipart_get_number (mp);
	for (i = 0; i < nparts; i++) {
		CamelMimePart *subpart;
		CamelContentType *ct;
		gchar *cts;

		subpart = camel_multipart_get_part (mp, i);

		if (!subpart)
			continue;

		g_string_append_printf (part_id, ".digest.%d", i);

		ct = camel_mime_part_get_content_type (subpart);

		/* According to RFC this shouldn't happen, but who knows... */
		if (ct && !camel_content_type_is (ct, "message", "rfc822")) {
			cts = camel_content_type_simple (ct);

			e_mail_parser_parse_part_as (
				parser, subpart, part_id, cts,
				cancellable, out_mail_parts);

			g_free (cts);
		} else {
			GQueue work_queue = G_QUEUE_INIT;
			EMailPart *mail_part;

			e_mail_parser_parse_part_as (
				parser, subpart, part_id, "message/rfc822",
				cancellable, &work_queue);

			mail_part = g_queue_peek_head (&work_queue);

			/* Force the message to be collapsable */
			if (mail_part != NULL && !mail_part->is_attachment)
				e_mail_parser_wrap_as_attachment (
					parser, subpart, part_id, &work_queue);

			mail_part = g_queue_peek_head (&work_queue);

			/* Force the message to be expanded */
			if (mail_part != NULL)
				mail_part->force_inline = TRUE;

			e_queue_transfer (&work_queue, out_mail_parts);
		}

		g_string_truncate (part_id, len);
	}

	return TRUE;
}

static guint32
empe_mp_digest_get_flags (EMailParserExtension *extension)
{
	return E_MAIL_PARSER_EXTENSION_COMPOUND_TYPE;
}

static const gchar **
empe_mp_digest_mime_types (EMailExtension *extension)
{
	return parser_mime_types;
}

static void
e_mail_parser_multipart_digest_class_init (EMailParserMultipartDigestClass *class)
{
}

static void
e_mail_parser_parser_extension_interface_init (EMailParserExtensionInterface *iface)
{
	iface->parse = empe_mp_digest_parse;
	iface->get_flags = empe_mp_digest_get_flags;
}

static void
e_mail_parser_mail_extension_interface_init (EMailExtensionInterface *iface)
{
	iface->mime_types = empe_mp_digest_mime_types;
}

static void
e_mail_parser_multipart_digest_init (EMailParserMultipartDigest *parser)
{

}
