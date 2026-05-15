/*
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include "evolution-config.h"

#include <glib/gi18n-lib.h>

#include <e-util/e-util.h>

#include "e-mail-parser-extension.h"
#include "e-mail-part-image.h"
#include "e-mail-part-utils.h"

typedef EMailParserExtension EMailParserImage;
typedef EMailParserExtensionClass EMailParserImageClass;

GType e_mail_parser_image_get_type (void);

G_DEFINE_TYPE (
	EMailParserImage,
	e_mail_parser_image,
	E_TYPE_MAIL_PARSER_EXTENSION)

static const gchar *parser_mime_types[] = {
	"image/*",
	NULL
};

static gboolean
empe_image_parse (EMailParserExtension *extension,
                  EMailParser *parser,
                  CamelMimePart *part,
                  GString *part_id,
                  GCancellable *cancellable,
                  GQueue *out_mail_parts)
{
	GQueue work_queue = G_QUEUE_INIT;
	EMailPart *mail_part;
	gint len;

	len = part_id->len;
	g_string_append (part_id, ".image");

	mail_part = e_mail_part_image_new (part, part_id->str);

	g_string_truncate (part_id, len);

	g_queue_push_tail (&work_queue, mail_part);

	if (!mail_part->is_hidden)
		e_mail_parser_wrap_as_attachment (parser, part, part_id, E_MAIL_PARSER_WRAP_ATTACHMENT_FLAG_NONE, &work_queue);

	e_queue_transfer (&work_queue, out_mail_parts);

	return TRUE;
}

static void
e_mail_parser_image_class_init (EMailParserExtensionClass *class)
{
	class->mime_types = parser_mime_types;
	class->priority = G_PRIORITY_LOW;
	class->parse = empe_image_parse;
}

static void
e_mail_parser_image_init (EMailParserExtension *extension)
{
}
