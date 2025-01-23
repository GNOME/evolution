/*
 * e-mail-parser-vcard.c
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
 */

#include "evolution-config.h"

#include <string.h>
#include <gtk/gtk.h>
#include <glib.h>
#include <glib/gi18n.h>
#include <glib/gstdio.h>

#include "e-mail-parser-vcard.h"
#include "e-mail-part-vcard.h"

#include <camel/camel.h>

#include <em-format/e-mail-extension-registry.h>
#include <em-format/e-mail-parser-extension.h>
#include <em-format/e-mail-part.h>

#include <libebook/libebook.h>
#include <libedataserver/libedataserver.h>

#include <addressbook/util/eab-book-util.h>

#include <libebackend/libebackend.h>

#define d(x)

typedef EMailParserExtension EMailParserVCard;
typedef EMailParserExtensionClass EMailParserVCardClass;

typedef EExtension EMailParserVCardLoader;
typedef EExtensionClass EMailParserVCardLoaderClass;

GType e_mail_parser_vcard_get_type (void);

G_DEFINE_DYNAMIC_TYPE (
	EMailParserVCard,
	e_mail_parser_vcard,
	E_TYPE_MAIL_PARSER_EXTENSION)

static const gchar *parser_mime_types[] = {
	"text/vcard",
	"text/x-vcard",
	"text/directory",
	NULL
};

static void
decode_vcard (EMailPartVCard *vcard_part,
              CamelMimePart *mime_part)
{
	CamelDataWrapper *data_wrapper;
	CamelMedium *medium;
	CamelStream *stream;
	GSList *contact_list;
	GByteArray *array;
	const gchar *string;
	const guint8 padding[2] = {0};

	array = g_byte_array_new ();
	medium = CAMEL_MEDIUM (mime_part);

	/* Stream takes ownership of the byte array. */
	stream = camel_stream_mem_new_with_byte_array (array);
	data_wrapper = camel_medium_get_content (medium);
	camel_data_wrapper_decode_to_stream_sync (
		data_wrapper, stream, NULL, NULL);

	/* because the result is not NULL-terminated */
	g_byte_array_append (array, padding, 2);

	string = (gchar *) array->data;
	contact_list = eab_contact_list_from_string (string);
	e_mail_part_vcard_take_contacts (vcard_part, contact_list);

	g_object_unref (mime_part);
	g_object_unref (stream);
}

static gboolean
empe_vcard_parse (EMailParserExtension *extension,
                         EMailParser *parser,
                         CamelMimePart *part,
                         GString *part_id,
                         GCancellable *cancellable,
                         GQueue *out_mail_parts)
{
	EMailPartVCard *vcard_part;
	GQueue work_queue = G_QUEUE_INIT;
	gint len;

	len = part_id->len;
	g_string_append (part_id, ".org-gnome-vcard-display");

	vcard_part = e_mail_part_vcard_new (part, part_id->str);

	g_object_ref (part);

	decode_vcard (vcard_part, part);

	g_string_truncate (part_id, len);

	g_queue_push_tail (&work_queue, vcard_part);

	e_mail_parser_wrap_as_attachment (parser, part, part_id, E_MAIL_PARSER_WRAP_ATTACHMENT_FLAG_NONE, &work_queue);

	e_queue_transfer (&work_queue, out_mail_parts);

	return TRUE;
}

static void
e_mail_parser_vcard_class_init (EMailParserExtensionClass *class)
{
	class->mime_types = parser_mime_types;
	class->flags = E_MAIL_PARSER_EXTENSION_INLINE_DISPOSITION;
	class->parse = empe_vcard_parse;
}

static void
e_mail_parser_vcard_class_finalize (EMailParserExtensionClass *class)
{
}

static void
e_mail_parser_vcard_init (EMailParserExtension *extension)
{
}

void
e_mail_parser_vcard_type_register (GTypeModule *type_module)
{
	e_mail_parser_vcard_register_type (type_module);
}

