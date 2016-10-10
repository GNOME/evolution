/*
 * e-mail-parser-multipart-appledouble.c
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

#include "e-mail-parser-extension.h"

typedef EMailParserExtension EMailParserMultipartAppleDouble;
typedef EMailParserExtensionClass EMailParserMultipartAppleDoubleClass;

GType e_mail_parser_multipart_apple_double_get_type (void);

G_DEFINE_TYPE (
	EMailParserMultipartAppleDouble,
	e_mail_parser_multipart_apple_double,
	E_TYPE_MAIL_PARSER_EXTENSION)

static const gchar *parser_mime_types[] = {
	"multipart/appledouble",
	NULL
};

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

static void
e_mail_parser_multipart_apple_double_class_init (EMailParserExtensionClass *class)
{
	class->mime_types = parser_mime_types;
	class->priority = G_PRIORITY_LOW;
	class->parse = empe_mp_appledouble_parse;
}

static void
e_mail_parser_multipart_apple_double_init (EMailParserExtension *extension)
{
}
