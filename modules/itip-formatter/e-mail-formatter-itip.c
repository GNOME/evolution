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

typedef struct _EMailFormatterItip {
	EExtension parent;
} EMailFormatterItip;

typedef struct _EMailFormatterItipClass {
	EExtensionClass parent_class;
} EMailFormatterItipClass;

GType e_mail_formatter_itip_get_type (void);
static void e_mail_formatter_formatter_extension_interface_init (EMailFormatterExtensionInterface *iface);
static void e_mail_formatter_mail_extension_interface_init (EMailExtensionInterface *iface);

G_DEFINE_DYNAMIC_TYPE_EXTENDED (
	EMailFormatterItip,
	e_mail_formatter_itip,
	E_TYPE_EXTENSION,
	0,
	G_IMPLEMENT_INTERFACE_DYNAMIC (
		E_TYPE_MAIL_EXTENSION,
		e_mail_formatter_mail_extension_interface_init)
	G_IMPLEMENT_INTERFACE_DYNAMIC (
		E_TYPE_MAIL_FORMATTER_EXTENSION,
		e_mail_formatter_formatter_extension_interface_init));

static const gchar * formatter_mime_types[] = { "text/calendar",
						"application/ics",
						NULL };

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

		folder = context->part_list->folder;
		message = context->part_list->message;
		message_uid = context->part_list->message_uid;

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

static const gchar **
emfe_itip_mime_types (EMailExtension *extension)
{
	return formatter_mime_types;
}

static void
e_mail_formatter_itip_constructed (GObject *object)
{
	EExtensible *extensible;
	EMailExtensionRegistry *reg;

	extensible = e_extension_get_extensible (E_EXTENSION (object));
	reg = E_MAIL_EXTENSION_REGISTRY (extensible);

	e_mail_extension_registry_add_extension (reg, E_MAIL_EXTENSION (object));
}

static void
e_mail_formatter_itip_finalize (GObject *object)
{
	EExtensible *extensible;
	EMailExtensionRegistry *reg;

	extensible = e_extension_get_extensible (E_EXTENSION (object));
	reg = E_MAIL_EXTENSION_REGISTRY (extensible);

	e_mail_extension_registry_remove_extension (reg, E_MAIL_EXTENSION (object));
}

static void
e_mail_formatter_itip_class_init (EMailFormatterItipClass *class)
{
	GObjectClass *object_class;
	EExtensionClass *extension_class;

	object_class = G_OBJECT_CLASS (class);
	object_class->constructed = e_mail_formatter_itip_constructed;
	object_class->finalize = e_mail_formatter_itip_finalize;

	extension_class = E_EXTENSION_CLASS (class);
	extension_class->extensible_type = E_TYPE_MAIL_FORMATTER_EXTENSION_REGISTRY;
}

static void
e_mail_formatter_formatter_extension_interface_init (EMailFormatterExtensionInterface *iface)
{
	iface->format = emfe_itip_format;
	iface->get_display_name = emfe_itip_get_display_name;
	iface->get_description = emfe_itip_get_description;
}

static void
e_mail_formatter_mail_extension_interface_init (EMailExtensionInterface *iface)
{
	iface->mime_types = emfe_itip_mime_types;
}

static void
e_mail_formatter_itip_init (EMailFormatterItip *formatter)
{
}

void
e_mail_formatter_itip_type_register (GTypeModule *type_module)
{
	e_mail_formatter_itip_register_type (type_module);
}

static void
e_mail_formatter_itip_class_finalize (EMailFormatterItipClass *class)
{

}
