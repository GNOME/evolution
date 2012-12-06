/*
 * e-mail-parser-image.c
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
#include <em-format/e-mail-part-utils.h>
#include <em-format/e-mail-parser.h>
#include <e-util/e-util.h>

#include <glib/gi18n-lib.h>
#include <camel/camel.h>

typedef struct _EMailParserImage {
	GObject parent;
} EMailParserImage;

typedef struct _EMailParserImageClass {
	GObjectClass parent_class;
} EMailParserImageClass;

static void e_mail_parser_parser_extension_interface_init (EMailParserExtensionInterface *iface);
static void e_mail_parser_mail_extension_interface_init (EMailExtensionInterface *iface);

G_DEFINE_TYPE_EXTENDED (
	EMailParserImage,
	e_mail_parser_image,
	G_TYPE_OBJECT,
	0,
	G_IMPLEMENT_INTERFACE (
		E_TYPE_MAIL_EXTENSION,
		e_mail_parser_mail_extension_interface_init)
	G_IMPLEMENT_INTERFACE (
		E_TYPE_MAIL_PARSER_EXTENSION,
		e_mail_parser_parser_extension_interface_init));

static const gchar *parser_mime_types[] = { "image/gif",
					    "image/jpeg",
					    "image/png",
					    "image/x-png",
					    "image/x-bmp",
					    "image/bmp",
					    "image/svg",
					    "image/x-cmu-raster",
					    "image/x-ico",
					    "image/x-portable-anymap",
					    "image/x-portable-bitmap",
					    "image/x-portable-graymap",
					    "image/x-portable-pixmap",
					    "image/x-xpixmap",
					    "image/jpg",
					    "image/pjpeg",
					    NULL };

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
	const gchar *tmp;
	gchar *cid;
	gint len;
	CamelContentType *ct;

	tmp = camel_mime_part_get_content_id (part);
	if (tmp) {
		cid = g_strdup_printf ("cid:%s", tmp);
	} else {
		cid = NULL;
	}

	len = part_id->len;
	g_string_append (part_id, ".image");

	ct = camel_mime_part_get_content_type (part);

	mail_part = e_mail_part_new (part, part_id->str);
	mail_part->is_attachment = TRUE;
	mail_part->cid = cid;
	mail_part->mime_type = ct ? camel_content_type_simple (ct) : g_strdup ("image/*");
	mail_part->is_hidden = (cid != NULL);

	g_string_truncate (part_id, len);

	g_queue_push_tail (&work_queue, mail_part);

	if (cid == NULL)
		e_mail_parser_wrap_as_attachment (
			parser, part, part_id, &work_queue);

	e_queue_transfer (&work_queue, out_mail_parts);

	return TRUE;
}

static const gchar **
empe_image_mime_types (EMailExtension *extension)
{
	return parser_mime_types;
}

static void
e_mail_parser_image_class_init (EMailParserImageClass *class)
{
}

static void
e_mail_parser_parser_extension_interface_init (EMailParserExtensionInterface *iface)
{
	iface->parse = empe_image_parse;
}

static void
e_mail_parser_mail_extension_interface_init (EMailExtensionInterface *iface)
{
	iface->mime_types = empe_image_mime_types;
}

static void
e_mail_parser_image_init (EMailParserImage *parser)
{

}
