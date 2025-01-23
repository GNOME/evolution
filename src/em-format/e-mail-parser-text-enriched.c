/*
 * e-mail-parser-text-enriched.c
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

#include "evolution-config.h"

#include <glib/gi18n-lib.h>

#include <e-util/e-util.h>

#include "e-mail-parser-extension.h"
#include "e-mail-part-utils.h"

typedef EMailParserExtension EMailParserTextEnriched;
typedef EMailParserExtensionClass EMailParserTextEnrichedClass;

GType e_mail_parser_text_enriched_get_type (void);

G_DEFINE_TYPE (
	EMailParserTextEnriched,
	e_mail_parser_text_enriched,
	E_TYPE_MAIL_PARSER_EXTENSION)

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
	const gchar *cid;
	gint len;
	CamelContentType *ct;

	len = part_id->len;
	g_string_append (part_id, ".text_enriched");

	mail_part = e_mail_part_new (part, part_id->str);

	ct = camel_mime_part_get_content_type (part);
	if (ct != NULL) {
		gchar *mime_type;

		mime_type = camel_content_type_simple (ct);
		e_mail_part_set_mime_type (mail_part, mime_type);
		g_free (mime_type);
	} else {
		e_mail_part_set_mime_type (mail_part, "text/enriched");
	}

	cid = camel_mime_part_get_content_id (part);
	if (cid != NULL) {
		gchar *cid_uri;

		cid_uri = g_strdup_printf ("cid:%s", cid);
		e_mail_part_set_cid (mail_part, cid_uri);
		g_free (cid_uri);
	}

	g_string_truncate (part_id, len);

	g_queue_push_tail (&work_queue, mail_part);

	if (e_mail_part_is_attachment (part))
		e_mail_parser_wrap_as_attachment (parser, part, part_id, E_MAIL_PARSER_WRAP_ATTACHMENT_FLAG_NONE, &work_queue);

	e_queue_transfer (&work_queue, out_mail_parts);

	return TRUE;
}

static void
e_mail_parser_text_enriched_class_init (EMailParserExtensionClass *class)
{
	class->mime_types = parser_mime_types;
	class->priority = G_PRIORITY_LOW;
	class->parse = empe_text_enriched_parse;
}

static void
e_mail_parser_text_enriched_init (EMailParserExtension *extension)
{
}
