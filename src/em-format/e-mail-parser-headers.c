/*
 * e-mail-parser-headers.c
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

#include <string.h>
#include <glib/gi18n-lib.h>

#include <e-util/e-util.h>
#include <libemail-engine/libemail-engine.h>

#include "e-mail-parser-extension.h"
#include "e-mail-part-headers.h"

typedef EMailParserExtension EMailParserHeaders;
typedef EMailParserExtensionClass EMailParserHeadersClass;

GType e_mail_parser_headers_get_type (void);

G_DEFINE_TYPE (
	EMailParserHeaders,
	e_mail_parser_headers,
	E_TYPE_MAIL_PARSER_EXTENSION)

static const gchar *parser_mime_types[] = {
	E_MAIL_PART_HEADERS_MIME_TYPE,
	NULL
};

static gboolean
empe_headers_parse (EMailParserExtension *extension,
                    EMailParser *parser,
                    CamelMimePart *part,
                    GString *part_id,
                    GCancellable *cancellable,
                    GQueue *out_mail_parts)
{
	EMailPart *mail_part;
	gint len;

	len = part_id->len;
	g_string_append (part_id, ".headers");

	mail_part = e_mail_part_headers_new (part, part_id->str);
	g_queue_push_tail (out_mail_parts, mail_part);

	g_string_truncate (part_id, len);

	return TRUE;
}

static void
e_mail_parser_headers_class_init (EMailParserExtensionClass *class)
{
	class->mime_types = parser_mime_types;
	class->priority = G_PRIORITY_LOW;
	class->parse = empe_headers_parse;
}

static void
e_mail_parser_headers_init (EMailParserExtension *extension)
{
}
