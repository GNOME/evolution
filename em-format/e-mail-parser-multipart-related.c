/*
 * e-mail-parser-multipart-related.c
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

#include <camel/camel.h>

#include <string.h>

typedef struct _EMailParserMultipartRelated {
	GObject parent;
} EMailParserMultipartRelated;

typedef struct _EMailParserMultipartRelatedClass {
	GObjectClass parent_class;
} EMailParserMultipartRelatedClass;

static void e_mail_parser_parser_extension_interface_init (EMailParserExtensionInterface *iface);
static void e_mail_parser_mail_extension_interface_init (EMailExtensionInterface *iface);

G_DEFINE_TYPE_EXTENDED (
	EMailParserMultipartRelated,
	e_mail_parser_multipart_related,
	G_TYPE_OBJECT,
	0,
	G_IMPLEMENT_INTERFACE (
		E_TYPE_MAIL_EXTENSION,
		e_mail_parser_mail_extension_interface_init)
	G_IMPLEMENT_INTERFACE (
		E_TYPE_MAIL_PARSER_EXTENSION,
		e_mail_parser_parser_extension_interface_init));

static const gchar * parser_mime_types[] = { "multipart/related",
					    NULL };

static GSList *
empe_mp_related_parse (EMailParserExtension *extension,
                       EMailParser *parser,
                       CamelMimePart *part,
                       GString *part_id,
                       GCancellable *cancellable)
{
	CamelMultipart *mp;
	CamelMimePart *body_part, *display_part = NULL;
	CamelContentType *display_content_type;
	gchar *html_body = NULL;
	gint i, nparts, partidlen, displayid = 0;
	GSList *parts;

	if (g_cancellable_is_cancelled (cancellable))
		return NULL;

	mp = (CamelMultipart *) camel_medium_get_content ((CamelMedium *) part);

	if (!CAMEL_IS_MULTIPART (mp)) {
		return e_mail_parser_parse_part_as (
				parser, part, part_id,
				"application/vnd.evolution.source", cancellable);
	}

	display_part = e_mail_part_get_related_display_part (part, &displayid);

	if (display_part == NULL) {
		return e_mail_parser_parse_part_as (
				parser, part, part_id, "multipart/mixed",
				cancellable);
	}

	display_content_type = camel_mime_part_get_content_type (display_part);
	if (display_content_type &&
	    camel_content_type_is (display_content_type, "text", "html")) {
		CamelDataWrapper *dw;

		dw = camel_medium_get_content ((CamelMedium *) display_part);
		if (dw) {
			CamelStream *mem = camel_stream_mem_new ();
			GByteArray *bytes;

			camel_data_wrapper_decode_to_stream_sync (dw, mem, cancellable, NULL);
			camel_stream_close (mem, cancellable, NULL);

			bytes = camel_stream_mem_get_byte_array	(CAMEL_STREAM_MEM (mem));
			if (bytes && bytes->len)
				html_body = g_strndup ((const gchar *) bytes->data, bytes->len);

			g_object_unref (mem);
		}
	}

	/* The to-be-displayed part goes first */
	partidlen = part_id->len;
	g_string_append_printf (part_id, ".related.%d", displayid);

	parts = e_mail_parser_parse_part (
			parser, display_part, part_id, cancellable);

	g_string_truncate (part_id, partidlen);

	/* Process the related parts */
	nparts = camel_multipart_get_number (mp);
	for (i = 0; i < nparts; i++) {
		GSList *list, *iter;
		body_part = camel_multipart_get_part (mp, i);
		list = NULL;

		if (body_part == display_part)
			continue;

		g_string_append_printf (part_id, ".related.%d", i);

		list = e_mail_parser_parse_part (
				parser, body_part, part_id, cancellable);

		g_string_truncate (part_id, partidlen);

		for (iter = list; iter; iter = iter->next) {
			EMailPart *mail_part;

			mail_part = iter->data;
			if (!mail_part)
				continue;

			/* Don't render the part on it's own! */
			if (e_mail_part_utils_body_refers (html_body, mail_part->cid))
				mail_part->is_hidden = TRUE;
		}

		parts = g_slist_concat (parts, list);
	}

	g_free (html_body);

	return parts;
}

static const gchar **
empe_mp_related_mime_types (EMailExtension *extension)
{
	return parser_mime_types;
}

static void
e_mail_parser_multipart_related_class_init (EMailParserMultipartRelatedClass *class)
{
}

static void
e_mail_parser_parser_extension_interface_init (EMailParserExtensionInterface *iface)
{
	iface->parse = empe_mp_related_parse;
}

static void
e_mail_parser_mail_extension_interface_init (EMailExtensionInterface *iface)
{
	iface->mime_types = empe_mp_related_mime_types;
}

static void
e_mail_parser_multipart_related_init (EMailParserMultipartRelated *parser)
{

}
