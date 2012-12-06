/*
 * e-mail-parser-multipart-appledouble.c
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

#include <camel/camel.h>

typedef struct _EMailParserMultipartAppleDouble {
	GObject parent;
} EMailParserMultipartAppleDouble;

typedef struct _EMailParserMultipartAppleDoubleClass {
	GObjectClass parent_class;
} EMailParserMultipartAppleDoubleClass;

static void e_mail_parser_parser_extension_interface_init (EMailParserExtensionInterface *iface);
static void e_mail_parser_mail_extension_interface_init (EMailExtensionInterface *iface);

G_DEFINE_TYPE_EXTENDED (
	EMailParserMultipartAppleDouble,
	e_mail_parser_multipart_apple_double,
	G_TYPE_OBJECT,
	0,
	G_IMPLEMENT_INTERFACE (
		E_TYPE_MAIL_EXTENSION,
		e_mail_parser_mail_extension_interface_init)
	G_IMPLEMENT_INTERFACE (
		E_TYPE_MAIL_PARSER_EXTENSION,
		e_mail_parser_parser_extension_interface_init));

static const gchar * parser_mime_types[] = { "multipart/appledouble", NULL };

static gboolean
empe_mp_appledouble_parse (EMailParserExtension *extension,
                           EMailParser *parser,
                           CamelMimePart *part,
                           GString *part_id,
                           GCancellable *cancellable,
                           GQueue *out_mail_parts)
{
	CamelMultipart *mp;

	mp = (CamelMultipart *) camel_medium_get_content ((CamelMedium *) part);

	if (!CAMEL_IS_MULTIPART (mp)) {
		e_mail_parser_parse_part_as (
			parser, part, part_id,
			"application/vnd.evolution.source",
			cancellable, out_mail_parts);
	} else {
		CamelMimePart *mime_part;
		mime_part = camel_multipart_get_part (mp, 1);

		if (mime_part) {
			gint len;
			/* try the data fork for something useful, doubtful but who knows */
			len = part_id->len;
			g_string_append_printf (part_id, ".appledouble.1");

			e_mail_parser_parse_part (
				parser, mime_part, part_id,
				cancellable, out_mail_parts);

			g_string_truncate (part_id, len);

		} else {
			e_mail_parser_parse_part_as (
				parser, part, part_id,
				"application/vnd.evolution.source",
				cancellable, out_mail_parts);
		}
	}

	return TRUE;
}

static const gchar **
empe_mp_appledouble_mime_types (EMailExtension *extension)
{
	return parser_mime_types;
}

static void
e_mail_parser_multipart_apple_double_class_init (EMailParserMultipartAppleDoubleClass *class)
{
}

static void
e_mail_parser_parser_extension_interface_init (EMailParserExtensionInterface *iface)
{
	iface->parse = empe_mp_appledouble_parse;
}

static void
e_mail_parser_mail_extension_interface_init (EMailExtensionInterface *iface)
{
	iface->mime_types = empe_mp_appledouble_mime_types;
}

static void
e_mail_parser_multipart_apple_double_init (EMailParserMultipartAppleDouble *parser)
{

}
