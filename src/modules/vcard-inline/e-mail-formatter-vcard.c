/*
 * e-mail-formatter-vcard.c
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

#include "e-mail-formatter-vcard.h"
#include "e-mail-part-vcard.h"

#include <glib/gi18n-lib.h>

#include <libebackend/libebackend.h>

#include <em-format/e-mail-formatter-extension.h>
#include <em-format/e-mail-formatter.h>
#include <em-format/e-mail-formatter-utils.h>
#include <em-format/e-mail-part-utils.h>

#include <camel/camel.h>

#define d(x)

typedef EMailFormatterExtension EMailFormatterVCard;
typedef EMailFormatterExtensionClass EMailFormatterVCardClass;

typedef EExtension EMailFormatterVCardLoader;
typedef EExtensionClass EMailFormatterVCardLoaderClass;

GType e_mail_formatter_vcard_get_type (void);

G_DEFINE_DYNAMIC_TYPE (
	EMailFormatterVCard,
	e_mail_formatter_vcard,
	E_TYPE_MAIL_FORMATTER_EXTENSION)

static const gchar *formatter_mime_types[] = {
	"text/vcard",
	"text/x-vcard",
	"text/directory",
	NULL
};

static gboolean
mail_formatter_vcard_format (EMailFormatterExtension *extension,
                             EMailFormatter *formatter,
                             EMailFormatterContext *context,
                             EMailPart *part,
                             GOutputStream *stream,
                             GCancellable *cancellable)
{
	EMailPartVCard *vcard_part;
	const GSList *contacts;

	g_return_val_if_fail (E_IS_MAIL_PART_VCARD (part), FALSE);

	vcard_part = (EMailPartVCard *) part;
	contacts = e_mail_part_vcard_get_contacts (vcard_part);

	if (!contacts)
		return FALSE;

	if (context->mode == E_MAIL_FORMATTER_MODE_RAW)  {
		EABContactFormatter *vcard_formatter;
		EABContactDisplayMode display_mode = EAB_CONTACT_DISPLAY_RENDER_COMPACT;
		EContact *contact;
		GString *buffer;

		contact = E_CONTACT (contacts->data);

		buffer = g_string_sized_new (1024);

		if (context && context->uri) {
			if (camel_strstrcase (context->uri, "vcard-format=normal"))
				display_mode = EAB_CONTACT_DISPLAY_RENDER_NORMAL;
			else if (camel_strstrcase (context->uri, "vcard-format=compact"))
				display_mode = EAB_CONTACT_DISPLAY_RENDER_COMPACT;
		}

		vcard_formatter = g_object_new (EAB_TYPE_CONTACT_FORMATTER,
			"display-mode", display_mode,
			"render-maps", FALSE,
			NULL);

		eab_contact_formatter_format_contact (vcard_formatter, contact, buffer);

		g_output_stream_write_all (
			stream, buffer->str, buffer->len,
			NULL, cancellable, NULL);

		g_string_free (buffer, TRUE);
		g_object_unref (vcard_formatter);
	} else {
		CamelFolder *folder;
		const gchar *message_uid;
		const gchar *default_charset, *charset;
		gchar *str, *uri, *button_iframe_uri;
		gint length;
		const gchar *label = NULL;
		const gchar *info = NULL;
		gchar *access_key = NULL;
		gchar *html_label;

		length = g_slist_length ((GSList *) contacts);

		folder = e_mail_part_list_get_folder (context->part_list);
		message_uid = e_mail_part_list_get_message_uid (context->part_list);
		default_charset = e_mail_formatter_get_default_charset (formatter);
		charset = e_mail_formatter_get_charset (formatter);

		if (!default_charset)
			default_charset = "";
		if (!charset)
			charset = "";

		uri = e_mail_part_build_uri (
			folder, message_uid,
			"part_id", G_TYPE_STRING, e_mail_part_get_id (part),
			"mode", G_TYPE_INT, E_MAIL_FORMATTER_MODE_RAW,
			"formatter_default_charset", G_TYPE_STRING, default_charset,
			"formatter_charset", G_TYPE_STRING, charset,
			"vcard-format", G_TYPE_STRING, "compact",
			NULL);

		str = g_strdup_printf (
			"<div id=\"%s\">",
			e_mail_part_get_id (part));
		g_output_stream_write_all (
			stream, str, strlen (str), NULL, cancellable, NULL);
		g_free (str);

		button_iframe_uri = e_mail_part_build_uri (
			folder, message_uid,
			"part_id", G_TYPE_STRING, e_mail_part_get_id (part),
			"mode", G_TYPE_INT, E_MAIL_FORMATTER_MODE_RAW,
			"formatter_default_charset", G_TYPE_STRING, default_charset,
			"formatter_charset", G_TYPE_STRING, charset,
			"vcard-format", G_TYPE_STRING, "normal",
			NULL);

		label = _("Show F_ull vCard");
		html_label = e_mail_formatter_parse_html_mnemonics (label, &access_key);
		str = g_strdup_printf (
			"<button type=\"button\" "
				"name=\"set-display-mode-normal\" "
				"id=\"%s\" "
				"class=\"org-gnome-vcard-display-mode-button\" "
				"value=\"%d\" "
				"evo-iframe-uri=\"%s\" "
				"style=\"margin-left: 0px\""
				"accesskey=\"%s\">%s</button>",
			e_mail_part_get_id (part),
			EAB_CONTACT_DISPLAY_RENDER_NORMAL, button_iframe_uri, access_key,
			html_label);
		g_output_stream_write_all (
			stream, str, strlen (str), NULL, cancellable, NULL);
		g_free (str);
		g_free (html_label);
		g_free (button_iframe_uri);
		g_clear_pointer (&access_key, g_free);

		button_iframe_uri = e_mail_part_build_uri (
			folder, message_uid,
			"part_id", G_TYPE_STRING, e_mail_part_get_id (part),
			"mode", G_TYPE_INT, E_MAIL_FORMATTER_MODE_RAW,
			"formatter_default_charset", G_TYPE_STRING, default_charset,
			"formatter_charset", G_TYPE_STRING, charset,
			"vcard-format", G_TYPE_STRING, "compact",
			NULL);

		label = _("Show Com_pact vCard");
		html_label = e_mail_formatter_parse_html_mnemonics (label, &access_key);
		str = g_strdup_printf (
			"<button type=\"button\" "
				"name=\"set-display-mode-compact\" "
				"id=\"%s\" "
				"class=\"org-gnome-vcard-display-mode-button\" "
				"value=\"%d\" "
				"evo-iframe-uri=\"%s\" "
				"style=\"margin-left: 0px\""
				"accesskey=\"%s\" hidden>%s</button>",
			e_mail_part_get_id (part),
			EAB_CONTACT_DISPLAY_RENDER_COMPACT, button_iframe_uri, access_key,
			html_label);
		g_output_stream_write_all (
			stream, str, strlen (str), NULL, cancellable, NULL);
		g_free (str);
		g_free (html_label);
		g_free (button_iframe_uri);
		g_clear_pointer (&access_key, g_free);

		html_label = e_mail_formatter_parse_html_mnemonics (
				_("Save _To Addressbook"), &access_key);
		str = g_strdup_printf (
			"<button type=\"button\" "
				"name=\"save-to-addressbook\" "
				"class=\"org-gnome-vcard-save-button\" "
				"value=\"%s\" "
				"accesskey=\"%s\">%s</button><br>"
				"<iframe width=\"100%%\" height=\"auto\" "
				" class=\"-e-mail-formatter-frame-color -e-web-view-background-color\" "
				" style=\"border: 1px solid;\""
				" src=\"%s\" id=\"%s\" name=\"%s\"></iframe>"
			"</div>",
			e_mail_part_get_id (part),
			access_key, html_label, uri,
			e_mail_part_get_id (part),
			e_mail_part_get_id (part));
		g_output_stream_write_all (
			stream, str, strlen (str), NULL, cancellable, NULL);
		g_free (str);
		g_free (html_label);
		g_clear_pointer (&access_key, g_free);

		if (length == 2) {
			info = _("There is one other contact.");

		} else if (length > 2) {
			info = g_strdup_printf (ngettext (
				/* Translators: This will always be two or more. */
				"There is %d other contact.",
				"There are %d other contacts.",
				length - 1), length - 1);
		}

		if (info) {
			str = g_strdup_printf (
				"<div class=\"attachment-info\">%s</div>",
				info);

			g_output_stream_write_all (
				stream, str, strlen (str),
				NULL, cancellable, NULL);

			g_free (str);
		}

		g_free (uri);
	}

	return TRUE;
}

static void
e_mail_formatter_vcard_class_init (EMailFormatterExtensionClass *class)
{
	class->display_name = _("Addressbook Contact");
	class->description = _("Display the part as an addressbook contact");
	class->mime_types = formatter_mime_types;
	class->format = mail_formatter_vcard_format;
}

static void
e_mail_formatter_vcard_class_finalize (EMailFormatterExtensionClass *class)
{
}

static void
e_mail_formatter_vcard_init (EMailFormatterExtension *extension)
{
}

void
e_mail_formatter_vcard_type_register (GTypeModule *type_module)
{
	e_mail_formatter_vcard_register_type (type_module);
}

