/*
 * e-mail-formatter-itip.c
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

#include "e-mail-formatter-itip.h"

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <glib/gi18n-lib.h>

#include <em-format/e-mail-formatter-extension.h>
#include <em-format/e-mail-formatter.h>
#include <em-format/e-mail-part-utils.h>
#include <libebackend/libebackend.h>

#include "itip-view.h"
#include "e-mail-part-itip.h"

#define d(x)

typedef EMailFormatterExtension EMailFormatterItip;
typedef EMailFormatterExtensionClass EMailFormatterItipClass;

typedef EExtension EMailFormatterItipLoader;
typedef EExtensionClass EMailFormatterItipLoaderClass;

GType e_mail_formatter_itip_get_type (void);
GType e_mail_formatter_itip_loader_get_type (void);

G_DEFINE_DYNAMIC_TYPE (
	EMailFormatterItip,
	e_mail_formatter_itip,
	E_TYPE_MAIL_FORMATTER_EXTENSION)

G_DEFINE_DYNAMIC_TYPE (
	EMailFormatterItipLoader,
	e_mail_formatter_itip_loader,
	E_TYPE_EXTENSION)

static const gchar *formatter_mime_types[] = {
	"text/calendar",
	"application/ics",
	NULL
};

static gboolean
emfe_itip_format (EMailFormatterExtension *extension,
                  EMailFormatter *formatter,
                  EMailFormatterContext *context,
                  EMailPart *part,
                  CamelStream *stream,
                  GCancellable *cancellable)
{
	GString *buffer;
	EMailPartItip *itip_part;

	g_return_val_if_fail (E_MAIL_PART_IS (part, EMailPartItip), FALSE);
	itip_part = (EMailPartItip *) part;

	if (context->mode == E_MAIL_FORMATTER_MODE_PRINTING) {
		buffer = g_string_sized_new (1024);

		itip_part->view = itip_view_new (itip_part, itip_part->registry);

		itip_view_init_view (itip_part->view);
		itip_view_write_for_printing (itip_part->view, buffer);

	} else if (context->mode == E_MAIL_FORMATTER_MODE_RAW) {
		buffer = g_string_sized_new (2048);

		itip_view_write (formatter, buffer);

	} else {
		CamelFolder *folder;
		CamelMimeMessage *message;
		const gchar *message_uid;
		gchar *uri;

		folder = e_mail_part_list_get_folder (context->part_list);
		message = e_mail_part_list_get_message (context->part_list);
		message_uid = e_mail_part_list_get_message_uid (context->part_list);

		/* mark message as containing calendar, thus it will show the
		 * icon in message list now on */
		if (message_uid != NULL && folder != NULL &&
			!camel_folder_get_message_user_flag (
				folder, message_uid, "$has_cal")) {

			camel_folder_set_message_user_flag (
				folder, message_uid, "$has_cal", TRUE);
		}

		itip_part->folder = g_object_ref (folder);
		itip_part->uid = g_strdup (message_uid);
		itip_part->msg = g_object_ref (message);

		uri = e_mail_part_build_uri (
			folder, message_uid,
			"part_id", G_TYPE_STRING, part->id,
			"mode", G_TYPE_INT, E_MAIL_FORMATTER_MODE_RAW,
			NULL);

		buffer = g_string_sized_new (256);
		g_string_append_printf (
			buffer,
			"<div class=\"part-container\" "
			"style=\"border: none; background: none;\">"
			"<iframe width=\"100%%\" height=\"auto\""
			" frameborder=\"0\" src=\"%s\" name=\"%s\" id=\"%s\"></iframe>"
			"</div>",
			uri, part->id, part->id);

		g_free (uri);
	}

	camel_stream_write_string (stream, buffer->str, cancellable, NULL);

	g_string_free (buffer, TRUE);

	return TRUE;
}

static const gchar *
emfe_itip_get_display_name (EMailFormatterExtension *extension)
{
	return _("ITIP");
}

static const gchar *
emfe_itip_get_description (EMailFormatterExtension *extension)
{
	return _("Display part as an invitation");
}

static void
e_mail_formatter_itip_class_init (EMailFormatterExtensionClass *class)
{
	class->mime_types = formatter_mime_types;
	class->format = emfe_itip_format;
	class->get_display_name = emfe_itip_get_display_name;
	class->get_description = emfe_itip_get_description;
}

static void
e_mail_formatter_itip_class_finalize (EMailFormatterExtensionClass *class)
{
}

static void
e_mail_formatter_itip_init (EMailFormatterExtension *extension)
{
}

static void
mail_formatter_itip_loader_constructed (GObject *object)
{
	EExtensible *extensible;

	extensible = e_extension_get_extensible (E_EXTENSION (object));

	e_mail_extension_registry_add_extension (
		E_MAIL_EXTENSION_REGISTRY (extensible),
		formatter_mime_types,
		e_mail_formatter_itip_get_type ());
}

static void
e_mail_formatter_itip_loader_class_init (EExtensionClass *class)
{
	GObjectClass *object_class;

	object_class = G_OBJECT_CLASS (class);
	object_class->constructed = mail_formatter_itip_loader_constructed;

	class->extensible_type = E_TYPE_MAIL_FORMATTER_EXTENSION_REGISTRY;
}

static void
e_mail_formatter_itip_loader_class_finalize (EExtensionClass *class)
{
}

static void
e_mail_formatter_itip_loader_init (EExtension *extension)
{
}

void
e_mail_formatter_itip_type_register (GTypeModule *type_module)
{
	e_mail_formatter_itip_register_type (type_module);
	e_mail_formatter_itip_loader_register_type (type_module);
}
