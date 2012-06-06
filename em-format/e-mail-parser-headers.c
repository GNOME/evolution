/*
 * e-mail-parser-headers.c
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

#include <glib/gi18n-lib.h>

#include <em-format/e-mail-parser-extension.h>
#include <em-format/e-mail-parser.h>
#include <libemail-engine/e-mail-utils.h>
#include <e-util/e-util.h>

#include <camel/camel.h>

#include <string.h>

typedef struct _EMailParserHeaders {
	GObject parent;
} EMailParserHeaders;

typedef struct _EMailParserHeadersClass {
	GObjectClass parent_class;
} EMailParserHeadersClass;

static void e_mail_parser_parser_extension_interface_init (EMailParserExtensionInterface *iface);
static void e_mail_parser_mail_extension_interface_init (EMailExtensionInterface *iface);

G_DEFINE_TYPE_EXTENDED (
	EMailParserHeaders,
	e_mail_parser_headers,
	G_TYPE_OBJECT,
	0,
	G_IMPLEMENT_INTERFACE (
		E_TYPE_MAIL_EXTENSION,
		e_mail_parser_mail_extension_interface_init)
	G_IMPLEMENT_INTERFACE (
		E_TYPE_MAIL_PARSER_EXTENSION,
		e_mail_parser_parser_extension_interface_init));

static const gchar *parser_mime_types[] = { "application/vnd.evolution.headers", NULL };

static GSList *
empe_headers_parse (EMailParserExtension *extension,
                    EMailParser *parser,
                    CamelMimePart *part,
                    GString *part_id,
                    GCancellable *cancellable)
{
	EMailPart *mail_part;
	gint len;

	if (g_cancellable_is_cancelled (cancellable))
		return NULL;

	len = part_id->len;
	g_string_append (part_id, ".headers");

	mail_part = e_mail_part_new (part, part_id->str);
	mail_part->mime_type = g_strdup ("application/vnd.evolution.headers");
	g_string_truncate (part_id, len);

	return g_slist_append (NULL, mail_part);
}

static const gchar **
empe_headers_mime_types (EMailExtension *extension)
{
	return parser_mime_types;
}

static void
e_mail_parser_headers_class_init (EMailParserHeadersClass *klass)
{
	e_mail_parser_headers_parent_class = g_type_class_peek_parent (klass);
}

static void
e_mail_parser_parser_extension_interface_init (EMailParserExtensionInterface *iface)
{
	iface->parse = empe_headers_parse;
}

static void
e_mail_parser_mail_extension_interface_init (EMailExtensionInterface *iface)
{
	iface->mime_types = empe_headers_mime_types;
}

static void
e_mail_parser_headers_init (EMailParserHeaders *parser)
{

}
