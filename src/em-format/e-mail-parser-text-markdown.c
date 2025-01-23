/*
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
#include "e-mail-part-utils.h"

typedef EMailParserExtension EMailParserTextMarkdown;
typedef EMailParserExtensionClass EMailParserTextMarkdownClass;

GType e_mail_parser_text_markdown_get_type (void);

G_DEFINE_TYPE (EMailParserTextMarkdown, e_mail_parser_text_markdown, E_TYPE_MAIL_PARSER_EXTENSION)

static const gchar *parser_mime_types[] = {
	"text/markdown",
	NULL
};

static gboolean
empe_text_markdown_parse (EMailParserExtension *extension,
			  EMailParser *parser,
			  CamelMimePart *part,
			  GString *part_id,
			  GCancellable *cancellable,
			  GQueue *out_mail_parts)
{
	CamelContentType *type;
	EMailPart *mail_part;
	GQueue work_queue = G_QUEUE_INIT;
	gint s_len = part_id->len;
	gchar *mime_type;
	gboolean is_attachment;

	if (!camel_medium_get_content ((CamelMedium *) part))
		return FALSE;

	is_attachment = e_mail_part_is_attachment (part);

	type = camel_mime_part_get_content_type (part);
	if (!camel_content_type_is (type, "text", "markdown"))
		return FALSE;

	g_string_append_printf (part_id, ".markdown_text.%d", 0);

	mail_part = e_mail_part_new (part, part_id->str);

	mime_type = camel_content_type_simple (type);
	e_mail_part_set_mime_type (mail_part, mime_type);
	g_free (mime_type);

	g_string_truncate (part_id, s_len);

	g_queue_push_tail (&work_queue, mail_part);

	if (is_attachment)
		e_mail_parser_wrap_as_attachment (parser, part, part_id, E_MAIL_PARSER_WRAP_ATTACHMENT_FLAG_NONE, &work_queue);

	e_queue_transfer (&work_queue, out_mail_parts);

	return TRUE;
}

static void
e_mail_parser_text_markdown_class_init (EMailParserExtensionClass *class)
{
	class->mime_types = parser_mime_types;
	class->priority = G_PRIORITY_LOW;
	class->parse = empe_text_markdown_parse;
}

static void
e_mail_parser_text_markdown_init (EMailParserExtension *extension)
{
}
