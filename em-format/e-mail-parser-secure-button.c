/*
 * e-mail-parser-secure-button.c
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
#include <e-util/e-util.h>

#include <camel/camel.h>

typedef struct _EMailParserSecureButton {
	GObject parent;
} EMailParserSecureButton;

typedef struct _EMailParserSecureButtonClass {
	GObjectClass parent_class;
} EMailParserSecureButtonClass;

static void e_mail_parser_parser_extension_interface_init (EMailParserExtensionInterface *iface);

G_DEFINE_TYPE_EXTENDED (
	EMailParserSecureButton,
	e_mail_parser_secure_button,
	G_TYPE_OBJECT,
	0,
	G_IMPLEMENT_INTERFACE (
		E_TYPE_MAIL_PARSER_EXTENSION,
		e_mail_parser_parser_extension_interface_init))

static const gchar *parser_mime_types[] = {
	"application/vnd.evolution.widget.secure-button",
	NULL
};

static gboolean
empe_secure_button_parse (EMailParserExtension *extension,
                          EMailParser *parser,
                          CamelMimePart *part,
                          GString *part_id,
                          GCancellable *cancellable,
                          GQueue *out_mail_parts)
{
	EMailPart *mail_part;
	gint len;

	len = part_id->len;
	g_string_append (part_id, ".secure_button");
	mail_part = e_mail_part_new (part, part_id->str);
	mail_part->mime_type = g_strdup ("application/vnd.evolution.widget.secure-button");
	g_string_truncate (part_id, len);

	g_queue_push_tail (out_mail_parts, mail_part);

	return TRUE;
}

static void
e_mail_parser_secure_button_class_init (EMailParserSecureButtonClass *class)
{
}

static void
e_mail_parser_parser_extension_interface_init (EMailParserExtensionInterface *iface)
{
	iface->mime_types = parser_mime_types;
	iface->parse = empe_secure_button_parse;
}

static void
e_mail_parser_secure_button_init (EMailParserSecureButton *parser)
{

}
