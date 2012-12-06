/*
 * e-mail-parser-text-enriched.c
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
#include <em-format/e-mail-parser.h>
#include <em-format/e-mail-part-utils.h>
#include <e-util/e-util.h>

#include <glib/gi18n-lib.h>
#include <camel/camel.h>

typedef struct _EMailParserTextEnriched {
	GObject parent;
} EMailParserTextEnriched;

typedef struct _EMailParserTextEnrichedClass {
	GObjectClass parent_class;
} EMailParserTextEnrichedClass;

static void e_mail_parser_parser_extension_interface_init (EMailParserExtensionInterface *iface);
static void e_mail_parser_mail_extension_interface_init (EMailExtensionInterface *iface);

G_DEFINE_TYPE_EXTENDED (
	EMailParserTextEnriched,
	e_mail_parser_text_enriched,
	G_TYPE_OBJECT,
	0,
	G_IMPLEMENT_INTERFACE (
		E_TYPE_MAIL_EXTENSION,
		e_mail_parser_mail_extension_interface_init)
	G_IMPLEMENT_INTERFACE (
		E_TYPE_MAIL_PARSER_EXTENSION,
		e_mail_parser_parser_extension_interface_init));

static const gchar *parser_mime_types[] = {
	"text/richtext",
	"text/enriched",
	NULL
};

static gboolean
empe_text_enriched_parse (EMailParserExtension *extension,
                          EMailParser *parser,
                          CamelMimePart *part,
                          GString *part_id,
                          GCancellable *cancellable,
                          GQueue *out_mail_parts)
{
	GQueue work_queue = G_QUEUE_INIT;
	EMailPart *mail_part;
	const gchar *tmp;
	gint len;
	CamelContentType *ct;

	len = part_id->len;
	g_string_append (part_id, ".text_enriched");

	ct = camel_mime_part_get_content_type (part);

	mail_part = e_mail_part_new (part, part_id->str);
	mail_part->mime_type = ct ? camel_content_type_simple (ct) : g_strdup ("text/enriched");
	tmp = camel_mime_part_get_content_id (part);
	if (!tmp) {
		mail_part->cid = NULL;
	} else {
		mail_part->cid = g_strdup_printf ("cid:%s", tmp);
	}

	g_string_truncate (part_id, len);

	g_queue_push_tail (&work_queue, mail_part);

	if (e_mail_part_is_attachment (part))
		e_mail_parser_wrap_as_attachment (
			parser, part, part_id, &work_queue);

	e_queue_transfer (&work_queue, out_mail_parts);

	return TRUE;
}

static void
e_mail_parser_text_enriched_class_init (EMailParserTextEnrichedClass *class)
{
}

static void
e_mail_parser_parser_extension_interface_init (EMailParserExtensionInterface *iface)
{
	iface->parse = empe_text_enriched_parse;
}

static void
e_mail_parser_mail_extension_interface_init (EMailExtensionInterface *iface)
{
	iface->mime_types = parser_mime_types;
}

static void
e_mail_parser_text_enriched_init (EMailParserTextEnriched *parser)
{

}
