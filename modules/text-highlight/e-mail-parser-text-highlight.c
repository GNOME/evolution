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

typedef GObject EMailParserTextHighlight;
typedef GObjectClass EMailParserTextHighlightClass;

typedef EExtension EMailParserTextHighlightLoader;
typedef EExtensionClass EMailParserTextHighlightLoaderClass;

GType e_mail_parser_text_highlight_get_type (void);
GType e_mail_parser_text_highlight_loader_get_type (void);
static void e_mail_parser_parser_extension_interface_init (EMailParserExtensionInterface *iface);

G_DEFINE_DYNAMIC_TYPE_EXTENDED (
	EMailParserTextHighlight,
	e_mail_parser_text_highlight,
	G_TYPE_OBJECT,
	0,
	G_IMPLEMENT_INTERFACE_DYNAMIC (
		E_TYPE_MAIL_PARSER_EXTENSION,
		e_mail_parser_parser_extension_interface_init));

G_DEFINE_DYNAMIC_TYPE (
	EMailParserTextHighlightLoader,
	e_mail_parser_text_highlight_loader,
	E_TYPE_EXTENSION)

static gboolean
empe_text_highlight_parse (EMailParserExtension *extension,
                           EMailParser *parser,
                           CamelMimePart *part,
                           GString *part_id,
                           GCancellable *cancellable,
                           GQueue *out_mail_parts)
{
	CamelContentType *ct;
	gint len;

	/* Prevent recursion */
	if (strstr (part_id->str, ".text-highlight") != NULL)
		return FALSE;

	/* Don't parse text/html if it's not an attachment */
	ct = camel_mime_part_get_content_type (part);
	if (camel_content_type_is (ct, "text", "html")) {
		const CamelContentDisposition *disp;

		disp = camel_mime_part_get_content_disposition (part);
		if (!disp || (g_strcmp0 (disp->disposition, "attachment") != 0))
			return FALSE;
	}

	len = part_id->len;
	g_string_append (part_id, ".text-highlight");

	/* All source codes and scripts are in general plain texts,
	 * so let text/plain parser handle it. */

	e_mail_parser_parse_part_as (
		parser, part, part_id, "text/plain",
		cancellable, out_mail_parts);

	g_string_truncate (part_id, len);

	return TRUE;
}

static void
e_mail_parser_parser_extension_interface_init (EMailParserExtensionInterface *iface)
{
	iface->mime_types = get_mime_types ();
	iface->parse = empe_text_highlight_parse;
}

static void
e_mail_parser_text_highlight_class_init (EMailParserTextHighlightClass *class)
{
}

void
e_mail_parser_text_highlight_class_finalize (EMailParserTextHighlightClass *class)
{
}

static void
e_mail_parser_text_highlight_init (EMailParserTextHighlight *parser)
{
}

static void
mail_parser_text_highlight_loader_constructed (GObject *object)
{
	EExtensible *extensible;

	extensible = e_extension_get_extensible (E_EXTENSION (object));

	e_mail_extension_registry_add_extension (
		E_MAIL_EXTENSION_REGISTRY (extensible),
		get_mime_types (),
		e_mail_parser_text_highlight_get_type ());
}

static void
e_mail_parser_text_highlight_loader_class_init (EExtensionClass *class)
{
	GObjectClass *object_class;

	object_class = G_OBJECT_CLASS (class);
	object_class->constructed = mail_parser_text_highlight_loader_constructed;

	class->extensible_type = E_TYPE_MAIL_PARSER_EXTENSION_REGISTRY;
}

static void
e_mail_parser_text_highlight_loader_class_finalize (EExtensionClass *class)
{
}

static void
e_mail_parser_text_highlight_loader_init (EExtension *extension)
{
}

void
e_mail_parser_text_highlight_type_register (GTypeModule *type_module)
{
	e_mail_parser_text_highlight_register_type (type_module);
	e_mail_parser_text_highlight_loader_register_type (type_module);
}

