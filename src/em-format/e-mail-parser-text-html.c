/*
 * e-mail-parser-text-html.c
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

#include "e-mail-parser-extension.h"
#include "e-mail-part-utils.h"

typedef EMailParserExtension EMailParserTextHTML;
typedef EMailParserExtensionClass EMailParserTextHTMLClass;

GType e_mail_parser_text_html_get_type (void);

G_DEFINE_TYPE (
	EMailParserTextHTML,
	e_mail_parser_text_html,
	E_TYPE_MAIL_PARSER_EXTENSION)

static const gchar *parser_mime_types[] = {
	"text/html",
	NULL
};

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
	e_mail_part_set_mime_type (mail_part, "text/html");
	e_mail_part_set_cid (mail_part, cid);
	g_string_truncate (part_id, len);

	g_queue_push_head (&work_queue, mail_part);

	if (e_mail_part_is_attachment (part))
		e_mail_parser_wrap_as_attachment (parser, part, part_id, E_MAIL_PARSER_WRAP_ATTACHMENT_FLAG_NONE, &work_queue);

	e_queue_transfer (&work_queue, out_mail_parts);

	g_free (cid);

	return TRUE;
}

static void
e_mail_parser_text_html_class_init (EMailParserExtensionClass *class)
{
	class->mime_types = parser_mime_types;
	class->priority = G_PRIORITY_LOW;
	class->parse = empe_text_html_parse;
}

static void
e_mail_parser_text_html_init (EMailParserExtension *extension)
{
}
