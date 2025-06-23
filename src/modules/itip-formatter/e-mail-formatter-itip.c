/*
 * e-mail-formatter-itip.c
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

#include <glib/gi18n-lib.h>

#include <em-format/e-mail-formatter-extension.h>
#include <em-format/e-mail-formatter.h>
#include <em-format/e-mail-part-utils.h>
#include <libebackend/libebackend.h>

#include "itip-view.h"
#include "e-mail-part-itip.h"

#include "e-mail-formatter-itip.h"

#define d(x)

typedef EMailFormatterExtension EMailFormatterItip;
typedef EMailFormatterExtensionClass EMailFormatterItipClass;

GType e_mail_formatter_itip_get_type (void);
GType e_mail_formatter_itip_loader_get_type (void);

G_DEFINE_DYNAMIC_TYPE (
	EMailFormatterItip,
	e_mail_formatter_itip,
	E_TYPE_MAIL_FORMATTER_EXTENSION)

static const gchar *formatter_mime_types[] = {
	"text/calendar",
	"application/ics",
	NULL
};

static gboolean
emfe_itip_get_use_alternative_html (const gchar *uri)
{
	GUri *guri;
	gboolean res = FALSE;

	if (!uri)
		return FALSE;

	guri = g_uri_parse (uri, SOUP_HTTP_URI_FLAGS | G_URI_FLAGS_PARSE_RELAXED, NULL);
	if (guri) {
		GHashTable *query;

		query = g_uri_get_query (guri) ? soup_form_decode (g_uri_get_query (guri)) : NULL;
		res = query && g_strcmp0 (g_hash_table_lookup (query, "e-itip-view-alternative-html"), "1") == 0;
		if (query)
			g_hash_table_destroy (query);
		g_uri_unref (guri);
	}

	return res;
}

static gboolean
emfe_itip_format (EMailFormatterExtension *extension,
                  EMailFormatter *formatter,
                  EMailFormatterContext *context,
                  EMailPart *part,
                  GOutputStream *stream,
                  GCancellable *cancellable)
{
	GString *buffer;
	EMailPartItip *itip_part;
	gboolean use_alternative_html;

	/* This can be called with attachment parts too, thus
	   return silently in that case */
	if (!E_IS_MAIL_PART_ITIP (part))
		return FALSE;

	use_alternative_html = emfe_itip_get_use_alternative_html (context->uri);

	if (use_alternative_html && context->mode != E_MAIL_FORMATTER_MODE_RAW)
		return TRUE;

	itip_part = (EMailPartItip *) part;

	if (context->mode == E_MAIL_FORMATTER_MODE_PRINTING) {
		ItipView *itip_view;

		buffer = g_string_sized_new (1024);

		itip_view = itip_view_new (e_mail_part_get_id (part),
			itip_part,
			itip_part->folder,
			itip_part->message_uid,
			itip_part->message,
			itip_part->itip_mime_part,
			itip_part->vcalendar,
			itip_part->cancellable);
		itip_view_init_view (itip_view);
		itip_view_write_for_printing (itip_view, buffer);

	} else if (context->mode == E_MAIL_FORMATTER_MODE_RAW) {
		GSettings *settings;
		gboolean show_day_agenda;

		if (use_alternative_html) {
			if (itip_part->alternative_html) {
				g_output_stream_write_all (stream,
					itip_part->alternative_html,
					strlen (itip_part->alternative_html),
					NULL, cancellable, NULL);
			}

			return TRUE;
		}

		buffer = g_string_sized_new (2048);

		settings = e_util_ref_settings ("org.gnome.evolution.plugin.itip");
		show_day_agenda = g_settings_get_boolean (settings, "show-day-agenda");
		g_clear_object (&settings);

		if (show_day_agenda && itip_part->vcalendar) {
			ICalComponent *vcalendar;
			ICalComponent *icomp;

			vcalendar = i_cal_component_new_from_string (itip_part->vcalendar);
			/* only events can show the day agenda, hide it otherwise */
			icomp = vcalendar ? i_cal_component_get_first_component (vcalendar, I_CAL_VEVENT_COMPONENT) : NULL;

			show_day_agenda = icomp != NULL;

			g_clear_object (&icomp);
			g_clear_object (&vcalendar);
		}

		itip_view_write (itip_part, formatter, buffer, show_day_agenda);
	} else {
		CamelFolder *folder, *old_folder;
		CamelMimeMessage *message, *old_message;
		const gchar *message_uid;
		const gchar *default_charset, *charset;
		gchar *uri, *old_message_uid;

		folder = e_mail_part_list_get_folder (context->part_list);
		message = e_mail_part_list_get_message (context->part_list);
		message_uid = e_mail_part_list_get_message_uid (context->part_list);

		/* mark message as containing calendar, thus it will show the
		 * icon in message list now on */
		if (message_uid != NULL && folder != NULL) {
			CamelMessageInfo *nfo = camel_folder_get_message_info (folder, message_uid);

			if (nfo && !camel_message_info_get_user_flag (nfo, "$has_cal"))
				camel_message_info_set_user_flag (nfo, "$has_cal", TRUE);

			g_clear_object (&nfo);
		}

		old_folder = itip_part->folder;
		old_message = itip_part->message;
		old_message_uid = itip_part->message_uid;

		itip_part->folder = folder ? g_object_ref (folder) : NULL;
		itip_part->message = g_object_ref (message);
		itip_part->message_uid = g_strdup (message_uid);
		g_clear_pointer (&itip_part->alternative_html, g_free);

		g_clear_object (&old_folder);
		g_clear_object (&old_message);
		g_free (old_message_uid);

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
			NULL);

		buffer = g_string_sized_new (256);
		g_string_append_printf (
			buffer,
			"<div class=\"part-container\" "
			"style=\"border: none; background: none;\">"
			"<iframe width=\"100%%\" height=\"auto\""
			" frameborder=\"0\" src=\"%s\" name=\"%s\" id=\"%s\"></iframe>"
			"</div>",
			uri,
			e_mail_part_get_id (part),
			e_mail_part_get_id (part));

		g_free (uri);
	}

	g_output_stream_write_all (
		stream, buffer->str, buffer->len, NULL, cancellable, NULL);

	g_string_free (buffer, TRUE);

	return TRUE;
}

static void
e_mail_formatter_itip_class_init (EMailFormatterExtensionClass *class)
{
	class->display_name = _("ITIP");
	class->description = _("Display part as an invitation");
	class->mime_types = formatter_mime_types;
	class->format = emfe_itip_format;
}

static void
e_mail_formatter_itip_class_finalize (EMailFormatterExtensionClass *class)
{
}

static void
e_mail_formatter_itip_init (EMailFormatterExtension *extension)
{
}

void
e_mail_formatter_itip_type_register (GTypeModule *type_module)
{
	e_mail_formatter_itip_register_type (type_module);
}
