/*
 * e-mail-parser-attachment-bar.c
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

#include "e-mail-part-attachment-bar.h"

#include <glib/gi18n-lib.h>

#include <e-util/e-util.h>

#include "e-mail-parser-extension.h"

static void
mail_part_attachment_bar_free (EMailPart *part)
{
	EMailPartAttachmentBar *empab = (EMailPartAttachmentBar *) part;

	g_clear_object (&empab->store);
}

/******************************************************************************/

typedef EMailParserExtension EMailParserAttachmentBar;
typedef EMailParserExtensionClass EMailParserAttachmentBarClass;

GType e_mail_parser_attachment_bar_get_type (void);

G_DEFINE_TYPE (
	EMailParserAttachmentBar,
	e_mail_parser_attachment_bar,
	E_TYPE_MAIL_PARSER_EXTENSION)

static const gchar *parser_mime_types[] = {
	"application/vnd.evolution.widget.attachment-bar",
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
	EMailPartAttachmentBar *empab;
	gint len;

	len = part_id->len;
	g_string_append (part_id, ".attachment-bar");
	empab = (EMailPartAttachmentBar *) e_mail_part_subclass_new (
		part, part_id->str, sizeof (EMailPartAttachmentBar),
		(GFreeFunc) mail_part_attachment_bar_free);
	empab->parent.mime_type = g_strdup ("application/vnd.evolution.widget.attachment-bar");
	empab->store = E_ATTACHMENT_STORE (e_attachment_store_new ());
	g_string_truncate (part_id, len);

	g_queue_push_tail (out_mail_parts, empab);

	return TRUE;
}

static void
e_mail_parser_attachment_bar_class_init (EMailParserExtensionClass *class)
{
	class->mime_types = parser_mime_types;
	class->parse = empe_attachment_bar_parse;
}

static void
e_mail_parser_attachment_bar_init (EMailParserExtension *extension)
{
}
