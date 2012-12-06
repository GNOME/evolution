/*
 * e-mail-parser-multipart-mixed.c
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
#include <em-format/e-mail-part-utils.h>

#include <camel/camel.h>

#include <string.h>

typedef struct _EMailParserMultipartMixed {
	GObject parent;
} EMailParserMultipartMixed;

typedef struct _EMailParserMultipartMixedClass {
	GObjectClass parent_class;
} EMailParserMultipartMixedClass;

static void e_mail_parser_parser_extension_interface_init (EMailParserExtensionInterface *iface);
static void e_mail_parser_mail_extension_interface_init (EMailExtensionInterface *iface);

G_DEFINE_TYPE_EXTENDED (
	EMailParserMultipartMixed,
	e_mail_parser_multipart_mixed,
	G_TYPE_OBJECT,
	0,
	G_IMPLEMENT_INTERFACE (
		E_TYPE_MAIL_EXTENSION,
		e_mail_parser_mail_extension_interface_init)
	G_IMPLEMENT_INTERFACE (
		E_TYPE_MAIL_PARSER_EXTENSION,
		e_mail_parser_parser_extension_interface_init));

static const gchar *parser_mime_types[] = {
	"multipart/mixed",
	"multipart/report",
	"multipart/*",
	NULL
};

static gboolean
empe_mp_mixed_parse (EMailParserExtension *extension,
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
		GQueue work_queue = G_QUEUE_INIT;
		EMailPart *mail_part;
		CamelMimePart *subpart;
		CamelContentType *ct;

		subpart = camel_multipart_get_part (mp, i);

		g_string_append_printf (part_id, ".mixed.%d", i);

		e_mail_parser_parse_part (
			parser, subpart, part_id, cancellable, &work_queue);

		mail_part = g_queue_peek_head (&work_queue);

		ct = camel_mime_part_get_content_type (subpart);

		/* Display parts with CID as attachments
		 * (unless they already are attachments). */
		if (mail_part != NULL &&
			mail_part->cid != NULL &&
			!mail_part->is_attachment) {

			e_mail_parser_wrap_as_attachment (
				parser, subpart, part_id, &work_queue);

			/* Force messages to be expandable */
		} else if (mail_part == NULL ||
		    (camel_content_type_is (ct, "message", "rfc822") &&
		     mail_part != NULL && !mail_part->is_attachment)) {

			e_mail_parser_wrap_as_attachment (
				parser, subpart, part_id, &work_queue);

			mail_part = g_queue_peek_head (&work_queue);

			if (mail_part != NULL)
				mail_part->force_inline = TRUE;
		}

		e_queue_transfer (&work_queue, out_mail_parts);

		g_string_truncate (part_id, len);
	}

	return TRUE;
}

static guint32
empe_mp_mixed_get_flags (EMailParserExtension *extension)
{
	return E_MAIL_PARSER_EXTENSION_COMPOUND_TYPE;
}

static void
e_mail_parser_multipart_mixed_class_init (EMailParserMultipartMixedClass *class)
{
}

static void
e_mail_parser_parser_extension_interface_init (EMailParserExtensionInterface *iface)
{
	iface->parse = empe_mp_mixed_parse;
	iface->get_flags = empe_mp_mixed_get_flags;
}

static void
e_mail_parser_mail_extension_interface_init (EMailExtensionInterface *iface)
{
	iface->mime_types = parser_mime_types;
}

static void
e_mail_parser_multipart_mixed_init (EMailParserMultipartMixed *parser)
{
}
