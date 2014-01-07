/*
 * e-mail-parser-attachment-bar.c
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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "e-mail-part-attachment-bar.h"

#include <glib/gi18n-lib.h>

#include <e-util/e-util.h>

#include "e-mail-parser-extension.h"

typedef EMailParserExtension EMailParserAttachmentBar;
typedef EMailParserExtensionClass EMailParserAttachmentBarClass;

GType e_mail_parser_attachment_bar_get_type (void);

G_DEFINE_TYPE (
	EMailParserAttachmentBar,
	e_mail_parser_attachment_bar,
	E_TYPE_MAIL_PARSER_EXTENSION)

static const gchar *parser_mime_types[] = {
	E_MAIL_PART_ATTACHMENT_BAR_MIME_TYPE,
	NULL
};

static gboolean
empe_attachment_bar_parse (EMailParserExtension *extension,
                           EMailParser *parser,
                           CamelMimePart *part,
                           GString *part_id,
                           GCancellable *cancellable,
                           GQueue *out_mail_parts)
{
	EMailPart *mail_part;
	gint len;

	len = part_id->len;
	g_string_append (part_id, ".attachment-bar");
	mail_part = e_mail_part_attachment_bar_new (part, part_id->str);
	e_mail_part_set_mime_type (mail_part, parser_mime_types[0]);
	g_string_truncate (part_id, len);

	g_queue_push_tail (out_mail_parts, mail_part);

	return TRUE;
}

static void
e_mail_parser_attachment_bar_class_init (EMailParserExtensionClass *class)
{
	class->mime_types = parser_mime_types;
	class->priority = G_PRIORITY_LOW;
	class->parse = empe_attachment_bar_parse;
}

static void
e_mail_parser_attachment_bar_init (EMailParserExtension *extension)
{
}
