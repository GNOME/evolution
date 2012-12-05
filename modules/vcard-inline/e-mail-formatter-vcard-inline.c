/*
 * e-mail-formatter-vcard-inline.c
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

#include "e-mail-formatter-vcard-inline.h"
#include "e-mail-part-vcard-inline.h"

#include <glib/gi18n-lib.h>

#include <libebackend/libebackend.h>

#include <em-format/e-mail-formatter-extension.h>
#include <em-format/e-mail-formatter.h>
#include <em-format/e-mail-formatter-utils.h>
#include <em-format/e-mail-part-utils.h>

#include <camel/camel.h>

#define d(x)

typedef struct _EMailFormatterVCardInline {
	EExtension parent;
} EMailFormatterVCardInline;

typedef struct _EMailFormatterVCardInlineClass {
	EExtensionClass parent_class;
} EMailFormatterVCardInlineClass;

GType e_mail_formatter_vcard_inline_get_type (void);
static void e_mail_formatter_formatter_extension_interface_init (EMailFormatterExtensionInterface *iface);
static void e_mail_formatter_mail_extension_interface_init (EMailExtensionInterface *iface);

G_DEFINE_DYNAMIC_TYPE_EXTENDED (
	EMailFormatterVCardInline,
	e_mail_formatter_vcard_inline,
	E_TYPE_EXTENSION,
	0,
	G_IMPLEMENT_INTERFACE_DYNAMIC (
		E_TYPE_MAIL_EXTENSION,
		e_mail_formatter_mail_extension_interface_init)
	G_IMPLEMENT_INTERFACE_DYNAMIC (
		E_TYPE_MAIL_FORMATTER_EXTENSION,
		e_mail_formatter_formatter_extension_interface_init));

static const gchar * formatter_mime_types[] = { "text/vcard", "text/x-vcard",
					       "text/directory", NULL };

static gboolean
emfe_vcard_inline_format (EMailFormatterExtension *extension,
                          EMailFormatter *formatter,
                          EMailFormatterContext *context,
                          EMailPart *part,
                          CamelStream *stream,
                          GCancellable *cancellable)
{
	EMailPartVCardInline *vcard_part;

	g_return_val_if_fail (E_MAIL_PART_IS (part, EMailPartVCardInline), FALSE);
	vcard_part = (EMailPartVCardInline *) part;

	if (context->mode == E_MAIL_FORMATTER_MODE_RAW)  {

		EContact *contact;

		if (vcard_part->contact_list != NULL)
			contact = E_CONTACT (vcard_part->contact_list->data);
		else
			contact = NULL;

		eab_contact_formatter_format_contact_sync (
			vcard_part->formatter, contact, stream, cancellable);

	} else {
		CamelFolder *folder;
		const gchar *message_uid;
		gchar *str, *uri;
		gint length;
		const gchar *label = NULL;
		EABContactDisplayMode mode;
		const gchar *info = NULL;
		gchar *html_label, *access_key;

		length = g_slist_length (vcard_part->contact_list);
		if (length < 1)
			return FALSE;

		folder = e_mail_part_list_get_folder (context->part_list);
		message_uid = e_mail_part_list_get_message_uid (context->part_list);

		if (vcard_part->message_uid == NULL && message_uid != NULL)
			vcard_part->message_uid = g_strdup (message_uid);

		if (vcard_part->folder == NULL && folder != NULL)
			vcard_part->folder = g_object_ref (folder);

		uri = e_mail_part_build_uri (
			folder, message_uid,
			"part_id", G_TYPE_STRING, part->id,
			"mode", G_TYPE_INT, E_MAIL_FORMATTER_MODE_RAW,
			NULL);

		mode = eab_contact_formatter_get_display_mode (vcard_part->formatter);
		if (mode == EAB_CONTACT_DISPLAY_RENDER_COMPACT) {
			mode = EAB_CONTACT_DISPLAY_RENDER_NORMAL;
			label = _("Show F_ull vCard");
		} else {
			mode = EAB_CONTACT_DISPLAY_RENDER_COMPACT;
			label = _("Show Com_pact vCard");
		}

		str = g_strdup_printf (
			"<div id=\"%s\">", part->id);
		camel_stream_write_string (stream, str, cancellable, NULL);
		g_free (str);

		html_label = e_mail_formatter_parse_html_mnemonics (
				label, &access_key);
		str = g_strdup_printf (
			"<button type=\"button\" "
				"name=\"set-display-mode\" "
				"class=\"org-gnome-vcard-inline-display-mode-button\" "
				"value=\"%d\" "
				"accesskey=\"%s\">%s</button>",
			mode, access_key, html_label);
		camel_stream_write_string (stream, str, cancellable, NULL);
		g_free (str);
		g_free (html_label);
		if (access_key)
			g_free (access_key);

		html_label = e_mail_formatter_parse_html_mnemonics (
				_("Save _To Addressbook"), &access_key);
		str = g_strdup_printf (
			"<button type=\"button\" "
				"name=\"save-to-addressbook\" "
				"class=\"org-gnome-vcard-inline-save-button\" "
				"value=\"%s\" "
				"accesskey=\"%s\">%s</button><br>"
			"<iframe width=\"100%%\" height=\"auto\" frameborder=\"0\""
				"src=\"%s\" name=\"%s\"></iframe>"
			"</div>",
			part->id, access_key, html_label,
			uri, part->id);
		camel_stream_write_string (stream, str, cancellable, NULL);
		g_free (str);
		g_free (html_label);
		if (access_key)
			g_free (access_key);

		if (length == 2) {

			info = _("There is one other contact.");

		} else if (length > 2) {

			/* Translators: This will always be two or more. */
			info = g_strdup_printf (ngettext (
				"There is %d other contact.",
				"There are %d other contacts.",
				length - 1), length - 1);
		}

		if (info) {

			str = g_strdup_printf (
				"<div class=\"attachment-info\">%s</div>",
				info);

			camel_stream_write_string (stream, str, cancellable, NULL);

			g_free (str);
		}

		g_free (uri);
	}

	return TRUE;
}

static const gchar *
emfe_vcard_inline_get_display_name (EMailFormatterExtension *extension)
{
	return _("Addressbook Contact");
}

static const gchar *
emfe_vcard_inline_get_description (EMailFormatterExtension *extension)
{
	return _("Display the part as an addressbook contact");
}

static const gchar **
emfe_vcard_inline_mime_types (EMailExtension *extension)
{
	return formatter_mime_types;
}

static void
e_mail_formatter_vcard_inline_constructed (GObject *object)
{
	EExtensible *extensible;
	EMailExtensionRegistry *reg;

	extensible = e_extension_get_extensible (E_EXTENSION (object));
	reg = E_MAIL_EXTENSION_REGISTRY (extensible);

	e_mail_extension_registry_add_extension (reg, E_MAIL_EXTENSION (object));
}

static void
e_mail_formatter_vcard_inline_class_init (EMailFormatterVCardInlineClass *class)
{
	GObjectClass *object_class;
	EExtensionClass *extension_class;

	object_class = G_OBJECT_CLASS (class);
	object_class->constructed = e_mail_formatter_vcard_inline_constructed;

	extension_class = E_EXTENSION_CLASS (class);
	extension_class->extensible_type = E_TYPE_MAIL_FORMATTER_EXTENSION_REGISTRY;
}

static void
e_mail_formatter_formatter_extension_interface_init (EMailFormatterExtensionInterface *iface)
{
	iface->format = emfe_vcard_inline_format;
	iface->get_display_name = emfe_vcard_inline_get_display_name;
	iface->get_description = emfe_vcard_inline_get_description;
}

static void
e_mail_formatter_mail_extension_interface_init (EMailExtensionInterface *iface)
{
	iface->mime_types = emfe_vcard_inline_mime_types;
}

static void
e_mail_formatter_vcard_inline_init (EMailFormatterVCardInline *formatter)
{

}

void
e_mail_formatter_vcard_inline_type_register (GTypeModule *type_module)
{
	e_mail_formatter_vcard_inline_register_type (type_module);
}

static void
e_mail_formatter_vcard_inline_class_finalize (EMailFormatterVCardInlineClass *class)
{

}
