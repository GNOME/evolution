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

typedef EMailFormatterExtension EMailFormatterVCardInline;
typedef EMailFormatterExtensionClass EMailFormatterVCardInlineClass;

typedef EExtension EMailFormatterVCardInlineLoader;
typedef EExtensionClass EMailFormatterVCardInlineLoaderClass;

GType e_mail_formatter_vcard_inline_get_type (void);

G_DEFINE_DYNAMIC_TYPE (
	EMailFormatterVCardInline,
	e_mail_formatter_vcard_inline,
	E_TYPE_MAIL_FORMATTER_EXTENSION)

static const gchar *formatter_mime_types[] = {
	"text/vcard",
	"text/x-vcard",
	"text/directory",
	NULL
};

static gboolean
emfe_vcard_inline_format (EMailFormatterExtension *extension,
                          EMailFormatter *formatter,
                          EMailFormatterContext *context,
                          EMailPart *part,
                          CamelStream *stream,
                          GCancellable *cancellable)
{
	EMailPartVCardInline *vcard_part;

	g_return_val_if_fail (E_IS_MAIL_PART_VCARD (part), FALSE);

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
		const gchar *default_charset, *charset;
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
		default_charset = e_mail_formatter_get_default_charset (formatter);
		charset = e_mail_formatter_get_charset (formatter);

		if (!default_charset)
			default_charset = "";
		if (!charset)
			charset = "";

		if (vcard_part->message_uid == NULL && message_uid != NULL)
			vcard_part->message_uid = g_strdup (message_uid);

		if (vcard_part->folder == NULL && folder != NULL)
			vcard_part->folder = g_object_ref (folder);

		uri = e_mail_part_build_uri (
			folder, message_uid,
			"part_id", G_TYPE_STRING, e_mail_part_get_id (part),
			"mode", G_TYPE_INT, E_MAIL_FORMATTER_MODE_RAW,
			"formatter_default_charset", G_TYPE_STRING, default_charset,
			"formatter_charset", G_TYPE_STRING, charset,
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
			"<div id=\"%s\">",
			e_mail_part_get_id (part));
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
			e_mail_part_get_id (part),
			access_key, html_label, uri,
			e_mail_part_get_id (part));
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

static void
e_mail_formatter_vcard_inline_class_init (EMailFormatterExtensionClass *class)
{
	class->display_name = _("Addressbook Contact");
	class->description = _("Display the part as an addressbook contact");
	class->mime_types = formatter_mime_types;
	class->format = emfe_vcard_inline_format;
}

static void
e_mail_formatter_vcard_inline_class_finalize (EMailFormatterExtensionClass *class)
{
}

static void
e_mail_formatter_vcard_inline_init (EMailFormatterExtension *extension)
{
}

void
e_mail_formatter_vcard_inline_type_register (GTypeModule *type_module)
{
	e_mail_formatter_vcard_inline_register_type (type_module);
}

