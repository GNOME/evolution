/*
 * e-mail-parser-multipart-alternative.c
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

typedef EMailParserExtension EMailParserMultipartAlternative;
typedef EMailParserExtensionClass EMailParserMultipartAlternativeClass;

GType e_mail_parser_multipart_alternative_get_type (void);

G_DEFINE_TYPE (
	EMailParserMultipartAlternative,
	e_mail_parser_multipart_alternative,
	E_TYPE_MAIL_PARSER_EXTENSION)

static const gchar *parser_mime_types[] = {
	"multipart/alternative",
	NULL
};

static gboolean
related_display_part_is_attachment (CamelMimePart *part)
{
	CamelMimePart *display_part;

	display_part = e_mail_part_get_related_display_part (part, NULL);
	return display_part && e_mail_part_is_attachment (display_part);
}

static gboolean
empe_mp_alternative_parse (EMailParserExtension *extension,
                           EMailParser *parser,
                           CamelMimePart *part,
                           GString *part_id,
                           GCancellable *cancellable,
                           GQueue *out_mail_parts)
{
	CamelMultipart *mp;
	gint i, nparts, bestid = 0;
	CamelMimePart *best = NULL;
	EMailExtensionRegistry *reg;

	reg = e_mail_parser_get_extension_registry (parser);

	mp = (CamelMultipart *) camel_medium_get_content ((CamelMedium *) part);

	if (!CAMEL_IS_MULTIPART (mp))
		return e_mail_parser_parse_part_as (
			parser, part, part_id,
			"application/vnd.evolution.source",
			cancellable, out_mail_parts);

	/* as per rfc, find the last part we know how to display */
	nparts = camel_multipart_get_number (mp);
	for (i = 0; i < nparts; i++) {
		CamelMimePart *mpart;
		CamelDataWrapper *data_wrapper;
		CamelContentType *type;
		gchar *mime_type;
		gsize content_size;

		if (g_cancellable_is_cancelled (cancellable))
			return TRUE;

		/* is it correct to use the passed in *part here? */
		mpart = camel_multipart_get_part (mp, i);

		if (mpart == NULL)
			continue;

		/* This may block even though the stream does not.
		 * XXX Pretty inefficient way to test if the MIME part
		 *     is empty.  Surely there's a quicker way? */
		data_wrapper = camel_medium_get_content (CAMEL_MEDIUM (mpart));
		content_size = camel_data_wrapper_calculate_decoded_size_sync (data_wrapper, cancellable, NULL);

		if (content_size == 0 || content_size == ((gsize) -1))
			continue;

		type = camel_mime_part_get_content_type (mpart);
		mime_type = camel_content_type_simple (type);

		camel_strdown (mime_type);

		if (!e_mail_part_is_attachment (mpart) &&
			 ((camel_content_type_is (type, "multipart", "related") == 0) ||
			  !related_display_part_is_attachment (mpart)) &&
		    (e_mail_extension_registry_get_for_mime_type (reg, mime_type) ||
			((best == NULL) &&
			 (e_mail_extension_registry_get_fallback (reg, mime_type)))))
		{
			best = mpart;
			bestid = i;
		}

		g_free (mime_type);
	}

	if (best) {
		gint len = part_id->len;

		g_string_append_printf (part_id, ".alternative.%d", bestid);

		e_mail_parser_parse_part (
			parser, best, part_id,
			cancellable, out_mail_parts);

		g_string_truncate (part_id, len);
	} else {
		e_mail_parser_parse_part_as (
			parser, part, part_id, "multipart/mixed",
			cancellable, out_mail_parts);
	}

	return TRUE;
}

static void
e_mail_parser_multipart_alternative_class_init (EMailParserExtensionClass *class)
{
	class->mime_types = parser_mime_types;
	class->priority = G_PRIORITY_LOW;
	class->parse = empe_mp_alternative_parse;
}

static void
e_mail_parser_multipart_alternative_init (EMailParserExtension *extension)
{
}
