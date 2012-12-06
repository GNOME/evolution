/*
 * e-mail-parser-text-html.c
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
#include <e-util/e-util.h>

#include <glib/gi18n-lib.h>
#include <camel/camel.h>

#include <string.h>

typedef struct _EMailParserTextHTML {
	GObject parent;
} EMailParserTextHTML;

typedef struct _EMailParserTextHTMLClass {
	GObjectClass parent_class;
} EMailParserTextHTMLClass;

static void e_mail_parser_parser_extension_interface_init (EMailParserExtensionInterface *iface);
static void e_mail_parser_mail_extension_interface_init (EMailExtensionInterface *iface);

G_DEFINE_TYPE_EXTENDED (
	EMailParserTextHTML,
	e_mail_parser_text_html,
	G_TYPE_OBJECT,
	0,
	G_IMPLEMENT_INTERFACE (
		E_TYPE_MAIL_EXTENSION,
		e_mail_parser_mail_extension_interface_init)
	G_IMPLEMENT_INTERFACE (
		E_TYPE_MAIL_PARSER_EXTENSION,
		e_mail_parser_parser_extension_interface_init));

static const gchar *parser_mime_types[] = { "text/html", NULL };

static gboolean
empe_text_html_parse (EMailParserExtension *extension,
                      EMailParser *parser,
                      CamelMimePart *part,
                      GString *part_id,
                      GCancellable *cancellable,
                      GQueue *out_mail_parts)
{
	GQueue work_queue = G_QUEUE_INIT;
	EMailPart *mail_part;
	const gchar *location;
	gchar *cid = NULL;
	const gchar *base;
	gint len;

	cid = NULL;
	base = camel_medium_get_header (CAMEL_MEDIUM (part), "content-base");
	location = camel_mime_part_get_content_location (part);
	if (location != NULL) {
		if (strchr (location, ':') == NULL && base != NULL) {
			CamelURL *uri;
			CamelURL *base_url = camel_url_new (base, NULL);

			uri = camel_url_new_with_base (base_url, location);
			cid = camel_url_to_string (uri, 0);
			camel_url_free (uri);
			camel_url_free (base_url);
		} else {
			cid = g_strdup (location);
		}
	}

	len = part_id->len;
	g_string_append (part_id, ".text_html");

	mail_part = e_mail_part_new (part, part_id->str);
	mail_part->mime_type = g_strdup ("text/html");
	mail_part->cid = cid;
	g_string_truncate (part_id, len);

	g_queue_push_head (&work_queue, mail_part);

	if (e_mail_part_is_attachment (part))
		e_mail_parser_wrap_as_attachment (
			parser, part, part_id, &work_queue);

	e_queue_transfer (&work_queue, out_mail_parts);

	return TRUE;
}

static const gchar **
empe_text_html_mime_types (EMailExtension *extension)
{
	return parser_mime_types;
}

static void
e_mail_parser_text_html_class_init (EMailParserTextHTMLClass *class)
{
}

static void
e_mail_parser_parser_extension_interface_init (EMailParserExtensionInterface *iface)
{
	iface->parse = empe_text_html_parse;
}

static void
e_mail_parser_mail_extension_interface_init (EMailExtensionInterface *iface)
{
	iface->mime_types = empe_text_html_mime_types;
}

static void
e_mail_parser_text_html_init (EMailParserTextHTML *parser)
{

}
