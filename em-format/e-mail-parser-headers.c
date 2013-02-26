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

#include <string.h>
#include <glib/gi18n-lib.h>

#include <e-util/e-util.h>
#include <libemail-engine/e-mail-utils.h>

#include "e-mail-parser-extension.h"

typedef EMailParserExtension EMailParserHeaders;
typedef EMailParserExtensionClass EMailParserHeadersClass;

GType e_mail_parser_headers_get_type (void);

G_DEFINE_TYPE (
	EMailParserHeaders,
	e_mail_parser_headers,
	E_TYPE_MAIL_PARSER_EXTENSION)

static const gchar *parser_mime_types[] = {
	"application/vnd.evolution.headers",
	NULL
};

static void
empe_headers_bind_dom (EMailPart *part,
                       WebKitDOMElement *element)
{
	WebKitDOMDocument *document;
	WebKitDOMElement *photo;
	gchar *addr, *uri;

	document = webkit_dom_node_get_owner_document (WEBKIT_DOM_NODE (element));
	photo = webkit_dom_document_get_element_by_id (document, "__evo-contact-photo");

	/* Contact photos disabled, the <img> tag is not there */
	if (!photo)
		return;

	addr = webkit_dom_element_get_attribute (photo, "data-mailaddr");
	uri = g_strdup_printf ("mail://contact-photo?mailaddr=%s", addr);

	webkit_dom_html_image_element_set_src (
		WEBKIT_DOM_HTML_IMAGE_ELEMENT (photo), uri);

	g_free (addr);
	g_free (uri);
}

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

	mail_part = e_mail_part_new (part, part_id->str);
	mail_part->mime_type = g_strdup ("application/vnd.evolution.headers");
	mail_part->bind_func = empe_headers_bind_dom;
	g_string_truncate (part_id, len);

	g_queue_push_tail (out_mail_parts, mail_part);

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
