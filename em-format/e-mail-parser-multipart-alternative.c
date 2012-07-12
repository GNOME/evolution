/*
 * e-mail-parser-multipart-alternative.c
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

typedef struct _EMailParserMultipartAlternative {
	GObject parent;
} EMailParserMultipartAlternative;

typedef struct _EMailParserMultipartAlternativeClass {
	GObjectClass parent_class;
} EMailParserMultipartAlternativeClass;

static void e_mail_parser_parser_extension_interface_init (EMailParserExtensionInterface *iface);
static void e_mail_parser_mail_extension_interface_init (EMailExtensionInterface *iface);

G_DEFINE_TYPE_EXTENDED (
	EMailParserMultipartAlternative,
	e_mail_parser_multipart_alternative,
	G_TYPE_OBJECT,
	0,
	G_IMPLEMENT_INTERFACE (
		E_TYPE_MAIL_EXTENSION,
		e_mail_parser_mail_extension_interface_init)
	G_IMPLEMENT_INTERFACE (
		E_TYPE_MAIL_PARSER_EXTENSION,
		e_mail_parser_parser_extension_interface_init));

static const gchar * parser_mime_types[] = { "multipart/alternative", NULL };

static gboolean
related_display_part_is_attachment (CamelMimePart *part)
{
	CamelMimePart *display_part;

	display_part = e_mail_part_get_related_display_part (part, NULL);
	return display_part && e_mail_part_is_attachment (display_part);
}

static GSList *
empe_mp_alternative_parse (EMailParserExtension *extension,
                           EMailParser *parser,
                           CamelMimePart *part,
                           GString *part_id,
                           GCancellable *cancellable)
{
	CamelMultipart *mp;
	gint i, nparts, bestid = 0;
	CamelMimePart *best = NULL;
	GSList *parts;
	EMailExtensionRegistry *reg;

	if (g_cancellable_is_cancelled (cancellable))
		return NULL;

	reg = e_mail_parser_get_extension_registry (parser);

	mp = (CamelMultipart *) camel_medium_get_content ((CamelMedium *) part);

	if (!CAMEL_IS_MULTIPART (mp)) {
		return e_mail_parser_parse_part_as (
				parser, part, part_id,
				"application/vnd.evolution.source",
				cancellable);
	}

	/* as per rfc, find the last part we know how to display */
	nparts = camel_multipart_get_number (mp);
	for (i = 0; i < nparts; i++) {
		CamelMimePart *mpart;
		CamelDataWrapper *data_wrapper;
		CamelContentType *type;
		CamelStream *null_stream;
		gchar *mime_type;
		gsize content_size;

		if (g_cancellable_is_cancelled (cancellable))
			return NULL;

		/* is it correct to use the passed in *part here? */
		mpart = camel_multipart_get_part (mp, i);

		if (mpart == NULL)
			continue;

		/* This may block even though the stream does not.
		 * XXX Pretty inefficient way to test if the MIME part
		 *     is empty.  Surely there's a quicker way? */
		null_stream = camel_stream_null_new ();
		data_wrapper = camel_medium_get_content (CAMEL_MEDIUM (mpart));
		camel_data_wrapper_decode_to_stream_sync (
			data_wrapper, null_stream, cancellable, NULL);
		content_size = CAMEL_STREAM_NULL (null_stream)->written;
		g_object_unref (null_stream);

		if (content_size == 0)
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

		parts = e_mail_parser_parse_part (
			parser, best, part_id, cancellable);

		g_string_truncate (part_id, len);
	} else {
		parts = e_mail_parser_parse_part_as (
			parser, part, part_id, "multipart/mixed", cancellable);
	}

	return parts;
}

static const gchar **
empe_mp_alternative_mime_types (EMailExtension *extension)
{
	return parser_mime_types;
}

static void
e_mail_parser_multipart_alternative_class_init (EMailParserMultipartAlternativeClass *class)
{
}

static void
e_mail_parser_parser_extension_interface_init (EMailParserExtensionInterface *iface)
{
	iface->parse = empe_mp_alternative_parse;
}

static void
e_mail_parser_mail_extension_interface_init (EMailExtensionInterface *iface)
{
	iface->mime_types = empe_mp_alternative_mime_types;
}

static void
e_mail_parser_multipart_alternative_init (EMailParserMultipartAlternative *parser)
{

}
