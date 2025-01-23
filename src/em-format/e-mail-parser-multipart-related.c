/*
 * e-mail-parser-multipart-related.c
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

#include <string.h>

#include <e-util/e-util.h>

#include "e-mail-parser-extension.h"
#include "e-mail-part-utils.h"
#include "e-mail-part-image.h"

typedef EMailParserExtension EMailParserMultipartRelated;
typedef EMailParserExtensionClass EMailParserMultipartRelatedClass;

GType e_mail_parser_multipart_related_get_type (void);

G_DEFINE_TYPE (
	EMailParserMultipartRelated,
	e_mail_parser_multipart_related,
	E_TYPE_MAIL_PARSER_EXTENSION)

static const gchar *parser_mime_types[] = {
	"multipart/related",
	NULL
};

static gboolean
empe_mp_related_parse (EMailParserExtension *extension,
                       EMailParser *parser,
                       CamelMimePart *part,
                       GString *part_id,
                       GCancellable *cancellable,
                       GQueue *out_mail_parts)
{
	CamelMultipart *mp;
	CamelMimePart *body_part, *display_part = NULL, *may_display_part;
	CamelContentType *display_content_type;
	gchar *html_body = NULL;
	gint i, nparts, partidlen, displayid = 0;

	mp = (CamelMultipart *) camel_medium_get_content ((CamelMedium *) part);

	if (!CAMEL_IS_MULTIPART (mp))
		return e_mail_parser_parse_part_as (
			parser, part, part_id,
			"application/vnd.evolution.source",
			cancellable, out_mail_parts);

	display_part = e_mail_part_get_related_display_part (part, &displayid);

	if (display_part == NULL)
		return e_mail_parser_parse_part_as (
			parser, part, part_id, "multipart/mixed",
			cancellable, out_mail_parts);

	may_display_part = display_part;

	display_content_type = camel_mime_part_get_content_type (display_part);
	if (display_content_type && camel_content_type_is (display_content_type, "multipart", "alternative")) {
		CamelMultipart *subparts = CAMEL_MULTIPART (camel_medium_get_content ((CamelMedium *) display_part));
		if (subparts) {
			nparts = camel_multipart_get_number (subparts);
			for (i = 0; i < nparts; i++) {
				body_part = camel_multipart_get_part (subparts, i);
				display_content_type = camel_mime_part_get_content_type (body_part);
				if (display_content_type && camel_content_type_is (display_content_type, "text", "html")) {
					may_display_part = body_part;
					break;
				}
			}
		}
	}

	display_content_type = camel_mime_part_get_content_type (may_display_part);
	if (display_content_type &&
	    camel_content_type_is (display_content_type, "text", "html")) {
		CamelDataWrapper *dw;

		dw = camel_medium_get_content ((CamelMedium *) may_display_part);
		if (dw) {
			CamelStream *mem = camel_stream_mem_new ();
			GByteArray *bytes;

			camel_data_wrapper_decode_to_stream_sync (dw, mem, cancellable, NULL);
			camel_stream_close (mem, cancellable, NULL);

			bytes = camel_stream_mem_get_byte_array (CAMEL_STREAM_MEM (mem));
			if (bytes && bytes->len)
				html_body = g_strndup ((const gchar *) bytes->data, bytes->len);

			g_object_unref (mem);
		}
	}

	/* The to-be-displayed part goes first */
	partidlen = part_id->len;
	g_string_append_printf (part_id, ".related.%d", displayid);

	e_mail_parser_parse_part (
		parser, display_part, part_id, cancellable, out_mail_parts);

	g_string_truncate (part_id, partidlen);

	/* Process the related parts */
	nparts = camel_multipart_get_number (mp);
	for (i = 0; i < nparts; i++) {
		GQueue work_queue = G_QUEUE_INIT;
		GList *head, *link;
		gint subpart_index = 0;

		body_part = camel_multipart_get_part (mp, i);

		if (body_part == display_part)
			continue;

		g_string_append_printf (part_id, ".related.%d", i);

		e_mail_parser_parse_part (
			parser, body_part, part_id,
			cancellable, &work_queue);

		head = g_queue_peek_head_link (&work_queue);

		for (link = head; link != NULL; link = g_list_next (link), subpart_index++) {
			EMailPart *mail_part = link->data;
			gboolean can_be_attachment;
			gboolean allow_as_attachment = FALSE;
			const gchar *cid;

			cid = e_mail_part_get_cid (mail_part);
			can_be_attachment = cid && E_IS_MAIL_PART_IMAGE (mail_part) &&
				e_mail_part_get_is_attachment (mail_part) && mail_part->is_hidden;

			if (can_be_attachment) {
				CamelMimePart *img_part;

				img_part = e_mail_part_ref_mime_part (mail_part);
				if (img_part) {
					CamelContentType *ct;

					ct = camel_mime_part_get_content_type (img_part);
					if (ct) {
						const gchar *name = camel_content_type_param (ct, "name");

						if (name && *name)
							allow_as_attachment = TRUE;
					}
					g_clear_object (&img_part);
				}
			}

			/* Don't render the part on its own! */
			if (!allow_as_attachment && e_mail_part_utils_body_refers (html_body, cid))
				mail_part->is_hidden = TRUE;
			else if (can_be_attachment) {
				gint sub_partidlen;

				sub_partidlen = part_id->len;
				g_string_append_printf (part_id, ".subpart.%d", subpart_index);

				if (allow_as_attachment && e_mail_part_utils_body_refers (html_body, cid))
					mail_part->is_hidden = TRUE;

				e_mail_parser_wrap_as_attachment (parser, body_part, part_id,
					allow_as_attachment ? E_MAIL_PARSER_WRAP_ATTACHMENT_FLAG_IS_POSSIBLE : E_MAIL_PARSER_WRAP_ATTACHMENT_FLAG_NONE,
					&work_queue);

				g_string_truncate (part_id, sub_partidlen);
			}
		}

		g_string_truncate (part_id, partidlen);

		e_queue_transfer (&work_queue, out_mail_parts);
	}

	g_free (html_body);

	return TRUE;
}

static void
e_mail_parser_multipart_related_class_init (EMailParserExtensionClass *class)
{
	class->mime_types = parser_mime_types;
	class->priority = G_PRIORITY_LOW;
	class->parse = empe_mp_related_parse;
}

static void
e_mail_parser_multipart_related_init (EMailParserExtension *extension)
{
}
