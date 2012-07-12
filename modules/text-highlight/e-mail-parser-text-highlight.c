/*
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
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <string.h>
#include <gtk/gtk.h>
#include <glib/gi18n.h>
#include <camel/camel.h>

#include "e-mail-parser-text-highlight.h"
#include "languages.h"

#include <em-format/e-mail-extension-registry.h>
#include <em-format/e-mail-parser-extension.h>
#include <em-format/e-mail-part.h>
#include <em-format/e-mail-part-utils.h>

#include <libebackend/libebackend.h>

#define d(x)

typedef struct _EMailParserTextHighlight {
	EExtension parent;
} EMailParserTextHighlight;

typedef struct _EMailParserTextHighlightClass {
	EExtensionClass parent_class;
} EMailParserTextHighlightClass;

GType e_mail_parser_text_highlight_get_type (void);
static void e_mail_parser_mail_extension_interface_init (EMailExtensionInterface *iface);
static void e_mail_parser_parser_extension_interface_init (EMailParserExtensionInterface *iface);

G_DEFINE_DYNAMIC_TYPE_EXTENDED (
	EMailParserTextHighlight,
	e_mail_parser_text_highlight,
	E_TYPE_EXTENSION,
	0,
	G_IMPLEMENT_INTERFACE_DYNAMIC (
		E_TYPE_MAIL_EXTENSION,
		e_mail_parser_mail_extension_interface_init)
	G_IMPLEMENT_INTERFACE_DYNAMIC (
		E_TYPE_MAIL_PARSER_EXTENSION,
		e_mail_parser_parser_extension_interface_init));

static GSList *
empe_text_highlight_parse (EMailParserExtension *extension,
                           EMailParser *parser,
                           CamelMimePart *part,
                           GString *part_id,
                           GCancellable *cancellable)
{
	GSList *parts;
	gint len;
	CamelContentType *ct;

	/* Prevent recursion */
	if (strstr (part_id->str, ".text-highlight") != NULL) {
		return NULL;
	}

	/* Don't parse text/html if it's not an attachment */
	ct = camel_mime_part_get_content_type (part);
	if (camel_content_type_is (ct, "text", "html")) {
		const CamelContentDisposition *disp;

		disp = camel_mime_part_get_content_disposition (part);
		if (!disp || (g_strcmp0 (disp->disposition, "attachment") != 0)) {
			return NULL;
		}
	}

	len = part_id->len;
	g_string_append (part_id, ".text-highlight");

	/* All source codes and scripts are in general plain texts,
	 * so let text/plain parser handle it. */
	parts = e_mail_parser_parse_part_as (
			parser, part, part_id, "text/plain", cancellable);

	g_string_truncate (part_id, len);

	return parts;
}

static const gchar **
empe_mime_types (EMailExtension *extension)
{
	return get_mime_types ();
}

void
e_mail_parser_text_highlight_type_register (GTypeModule *type_module)
{
	e_mail_parser_text_highlight_register_type (type_module);
}

static void
e_mail_parser_mail_extension_interface_init (EMailExtensionInterface *iface)
{
	iface->mime_types = empe_mime_types;
}

static void
e_mail_parser_parser_extension_interface_init (EMailParserExtensionInterface *iface)
{
	iface->parse = empe_text_highlight_parse;
}

static void
e_mail_parser_text_highlight_constructed (GObject *object)
{
	EExtensible *extensible;
	EMailExtensionRegistry *reg;

	extensible = e_extension_get_extensible (E_EXTENSION (object));
	reg = E_MAIL_EXTENSION_REGISTRY (extensible);

	e_mail_extension_registry_add_extension (reg, E_MAIL_EXTENSION (object));
}

static void
e_mail_parser_text_highlight_class_init (EMailParserTextHighlightClass *class)
{
	GObjectClass *object_class;
	EExtensionClass *extension_class;

	object_class = G_OBJECT_CLASS (class);
	object_class->constructed = e_mail_parser_text_highlight_constructed;

	extension_class = E_EXTENSION_CLASS (class);
	extension_class->extensible_type = E_TYPE_MAIL_PARSER_EXTENSION_REGISTRY;
}

void
e_mail_parser_text_highlight_class_finalize (EMailParserTextHighlightClass *class)
{
}

static void
e_mail_parser_text_highlight_init (EMailParserTextHighlight *parser)
{

}
