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

static const gchar * parser_mime_types[] = { "multipart/mixed",
					    "multipart/report",
					    "multipart/*",
					    NULL };

static GSList *
empe_mp_mixed_parse (EMailParserExtension *extension,
                     EMailParser *parser,
                     CamelMimePart *part,
                     GString *part_id,
                     GCancellable *cancellable)
{
	CamelMultipart *mp;
	gint i, nparts, len;
	GSList *parts;

	if (g_cancellable_is_cancelled (cancellable))
		return NULL;

	mp = (CamelMultipart *) camel_medium_get_content ((CamelMedium *) part);

	if (!CAMEL_IS_MULTIPART (mp)) {
		parts = e_mail_parser_parse_part_as (
				parser, part, part_id,
				"application/vnd.evolution.source", cancellable);
		return parts;
	}

	len = part_id->len;
	parts = NULL;
	nparts = camel_multipart_get_number (mp);
	for (i = 0; i < nparts; i++) {
		CamelMimePart *subpart;
		CamelContentType *ct;
		GSList *new_parts;

		subpart = camel_multipart_get_part (mp, i);

		g_string_append_printf (part_id, ".mixed.%d", i);

		new_parts = e_mail_parser_parse_part (
				parser, subpart, part_id, cancellable);

		ct = camel_mime_part_get_content_type (subpart);

		/* Display parts with CID as attachments (unless they already are
		 * attachments) */
		if (new_parts && new_parts->data &&
			(E_MAIL_PART (new_parts->data)->cid != NULL) &&
			!E_MAIL_PART (new_parts->data)->is_attachment) {

			parts = g_slist_concat (
				parts,
				e_mail_parser_wrap_as_attachment (
					parser, subpart, new_parts,
					part_id, cancellable));

			/* Force messages to be expandable */
		} else if (!new_parts ||
		    (camel_content_type_is (ct, "message", "rfc822") &&
		     new_parts && new_parts->data &&
		     !E_MAIL_PART (new_parts->data)->is_attachment)) {

			parts = g_slist_concat (
				parts,
				e_mail_parser_wrap_as_attachment (
					parser, subpart, new_parts,
					part_id, cancellable));
			if (parts && parts->data)
				E_MAIL_PART (parts->data)->force_inline = TRUE;
		} else {
			parts = g_slist_concat (parts, new_parts);
		}

		g_string_truncate (part_id, len);
	}

	return parts;
}

static guint32
empe_mp_mixed_get_flags (EMailParserExtension *extension)
{
	return E_MAIL_PARSER_EXTENSION_COMPOUND_TYPE;
}

static const gchar **
empe_mp_mixed_mime_types (EMailExtension *extension)
{
	return parser_mime_types;
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
	iface->mime_types = empe_mp_mixed_mime_types;
}

static void
e_mail_parser_multipart_mixed_init (EMailParserMultipartMixed *parser)
{
}
