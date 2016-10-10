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
 */

#include "evolution-config.h"

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

typedef EMailParserExtension EMailParserTextHighlight;
typedef EMailParserExtensionClass EMailParserTextHighlightClass;

typedef EExtension EMailParserTextHighlightLoader;
typedef EExtensionClass EMailParserTextHighlightLoaderClass;

GType e_mail_parser_text_highlight_get_type (void);

G_DEFINE_DYNAMIC_TYPE (
	EMailParserTextHighlight,
	e_mail_parser_text_highlight,
	E_TYPE_MAIL_PARSER_EXTENSION)

static gboolean
empe_text_highlight_parse (EMailParserExtension *extension,
                           EMailParser *parser,
                           CamelMimePart *part,
                           GString *part_id,
                           GCancellable *cancellable,
                           GQueue *out_mail_parts)
{
	CamelContentType *ct;
	gboolean handled;
	gint len;

	/* Prevent recursion */
	if (g_str_has_suffix (part_id->str, ".text-highlight"))
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

	handled = e_mail_parser_parse_part_as (
		parser, part, part_id, "text/plain",
		cancellable, out_mail_parts);

	g_string_truncate (part_id, len);

	return handled;
}

static void
e_mail_parser_text_highlight_class_init (EMailParserExtensionClass *class)
{
	class->mime_types = get_mime_types ();
	class->parse = empe_text_highlight_parse;
}

void
e_mail_parser_text_highlight_class_finalize (EMailParserExtensionClass *class)
{
}

static void
e_mail_parser_text_highlight_init (EMailParserExtension *extension)
{
}

void
e_mail_parser_text_highlight_type_register (GTypeModule *type_module)
{
	e_mail_parser_text_highlight_register_type (type_module);
}

