/*
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
 *
 * Authors:
 *		Federico Mena Quintero <federico@ximian.com>
 *	    Damon Chaplin <damon@ximian.com>
 *      Rodrigo Moya <rodrigo@ximian.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#include "evolution-config.h"

#include <string.h>
#include <gtk/gtk.h>
#include <glib/gi18n.h>
#include <camel/camel.h>

#include "shell/e-shell-utils.h"

#include "calendar-config.h"
#include "comp-util.h"
#include "itip-utils.h"

#include "e-cal-component-preview.h"

#define E_CAL_COMPONENT_PREVIEW_GET_PRIVATE(obj) \
	(G_TYPE_INSTANCE_GET_PRIVATE \
	((obj), E_TYPE_CAL_COMPONENT_PREVIEW, ECalComponentPreviewPrivate))

G_DEFINE_TYPE (
	ECalComponentPreview,
	e_cal_component_preview,
	E_TYPE_WEB_VIEW)

struct _ECalComponentPreviewPrivate {
	/* information about currently showing component in a preview;
	 * if it didn't change then the preview is not updated */
	gchar *cal_uid;
	gchar *comp_uid;
	ICalTime *comp_last_modified;
	gint comp_sequence;

	ECalClient *client;
	ECalComponent *comp;
	ICalTimezone *timezone;
	gboolean use_24_hour_format;
};

#define HTML_HEADER "<!doctype html public \"-//W3C//DTD HTML 4.0 TRANSITIONAL//EN\">\n<html>\n" \
                    "<head>\n<meta name=\"generator\" content=\"Evolution Calendar Component\">\n" \
		    "<meta name=\"color-scheme\" content=\"light dark\">\n" \
		    "<link type=\"text/css\" rel=\"stylesheet\" href=\"evo-file://$EVOLUTION_WEBKITDATADIR/webview.css\">\n" \
		    "<style>\n" \
		    ".description { font-family: monospace; font-size: 1em; }\n" \
		    "</style>\n" \
		    "</head>"

static void
clear_comp_info (ECalComponentPreview *preview)
{
	ECalComponentPreviewPrivate *priv;

	g_return_if_fail (E_IS_CAL_COMPONENT_PREVIEW (preview));

	priv = preview->priv;

	g_free (priv->cal_uid);
	priv->cal_uid = NULL;
	g_free (priv->comp_uid);
	priv->comp_uid = NULL;
	priv->comp_sequence = -1;

	g_clear_object (&priv->comp_last_modified);
	g_clear_object (&priv->client);
	g_clear_object (&priv->comp);
	g_clear_object (&priv->timezone);
}

/* Stores information about actually shown component and
 * returns whether component in the preview changed */
static gboolean
update_comp_info (ECalComponentPreview *preview,
		  ECalClient *client,
		  ECalComponent *comp,
		  ICalTimezone *zone,
		  gboolean use_24_hour_format)
{
	ECalComponentPreviewPrivate *priv;
	gboolean changed;

	g_return_val_if_fail (preview != NULL, TRUE);
	g_return_val_if_fail (E_IS_CAL_COMPONENT_PREVIEW (preview), TRUE);

	priv = preview->priv;

	if (!E_IS_CAL_COMPONENT (comp) || !E_IS_CAL_CLIENT (client)) {
		changed = !priv->cal_uid;
		clear_comp_info (preview);
	} else {
		ESource *source;
		const gchar *uid;
		gchar *cal_uid;
		gchar *comp_uid;
		ICalTime *comp_last_modified;
		gint comp_sequence;

		source = e_client_get_source (E_CLIENT (client));
		cal_uid = g_strdup (e_source_get_uid (source));
		uid = e_cal_component_get_uid (comp);
		comp_uid = g_strdup (uid);
		comp_last_modified = e_cal_component_get_last_modified (comp);
		comp_sequence = e_cal_component_get_sequence (comp);
		if (comp_sequence < 0)
			comp_sequence = 0;

		changed = !priv->cal_uid || !priv->comp_uid || !cal_uid || !comp_uid ||
			  !g_str_equal (priv->cal_uid, cal_uid) ||
			  !g_str_equal (priv->comp_uid, comp_uid) ||
			  priv->comp_sequence != comp_sequence;

		if (comp_last_modified && priv->comp_last_modified)
			changed = changed || i_cal_time_compare (priv->comp_last_modified, comp_last_modified) != 0;
		else
			changed = changed || comp_last_modified != priv->comp_last_modified;

		clear_comp_info (preview);

		priv->cal_uid = cal_uid;
		priv->comp_uid = comp_uid;
		priv->comp_sequence = comp_sequence;
		priv->comp_last_modified = comp_last_modified;

		priv->comp = g_object_ref (comp);
		priv->client = g_object_ref (client);
		priv->timezone = i_cal_timezone_copy (zone);
		priv->use_24_hour_format = use_24_hour_format;
	}

	return changed;
}

/* Converts a time_t to a string, relative to the specified timezone */
static gchar *
timet_to_str_with_zone (ECalComponentDateTime *dt,
			ECalClient *client,
			ICalTimezone *default_zone)
{
	ICalTime *itt;
	ICalTimezone *zone = NULL;
	struct tm tm;

	if (!dt)
		return NULL;

	itt = e_cal_component_datetime_get_value (dt);

	if (e_cal_component_datetime_get_tzid (dt)) {
		if (!e_cal_client_get_timezone_sync (client, e_cal_component_datetime_get_tzid (dt), &zone, NULL, NULL))
			zone = NULL;
	} else if (i_cal_time_is_utc (itt)) {
		zone = i_cal_timezone_get_utc_timezone ();
	}

	if (zone != NULL)
		i_cal_time_convert_timezone (itt, zone, default_zone);
	tm = e_cal_util_icaltime_to_tm (itt);

	return e_datetime_format_format_tm ("calendar", "table", i_cal_time_is_date (itt) ? DTFormatKindDate : DTFormatKindDateTime, &tm);
}

static void
cal_component_preview_add_table_line (GString *buffer,
				      const gchar *header,
				      const gchar *value)
{
	gchar *markup_header, *markup_value;

	g_return_if_fail (buffer != NULL);

	if (!value || !*value)
		return;

	markup_header = header ? g_markup_escape_text (header, -1) : NULL;
	markup_value = g_markup_escape_text (value, -1);

	g_string_append_printf (buffer,
		"<tr><th>%s</th><td>%s</td></tr>",
		markup_header ? markup_header : "",
		markup_value);

	g_free (markup_header);
	g_free (markup_value);
}

static void
cal_component_preview_write_html (ECalComponentPreview *preview,
                                  GString *buffer)
{
	ECalClient *client;
	ECalComponent *comp;
	ICalTimezone *default_zone;
	ECalComponentText *text;
	ECalComponentDateTime *dt;
	gchar *str;
	GString *string;
	GSList *list, *iter;
	ICalComponent *icomp;
	ICalProperty *prop;
	ICalPropertyStatus status;
	const gchar *tmp;
	gchar *location, *url;
	gint priority;
	gchar *markup;

	client = preview->priv->client;
	comp = preview->priv->comp;
	default_zone = preview->priv->timezone;

	/* write document header */
	text = e_cal_component_get_summary (comp);

	g_string_append (buffer, HTML_HEADER);
	g_string_append (buffer, "<body class=\"-e-web-view-background-color -e-web-view-text-color calpreview\">");

	markup = g_markup_escape_text (text && e_cal_component_text_get_value (text) ? e_cal_component_text_get_value (text) : _("Untitled"), -1);
	if (text && e_cal_component_text_get_value (text))
		g_string_append_printf (buffer, "<h2>%s</h2>", markup);
	else
		g_string_append_printf (buffer, "<h2><i>%s</i></h2>", markup);
	e_cal_component_text_free (text);
	g_free (markup);

	g_string_append (buffer, "<table border=\"0\" cellspacing=\"5\">");

	/* write icons for the categories */
	string = g_string_new (NULL);
	list = e_cal_component_get_categories_list (comp);
	if (list != NULL) {
		markup = g_markup_escape_text (_("Categories:"), -1);
		g_string_append_printf (buffer, "<tr><th>%s</th><td>", markup);
		g_free (markup);
	}
	for (iter = list; iter != NULL; iter = iter->next) {
		const gchar *category = iter->data;
		gchar *icon_file;

		icon_file = e_categories_dup_icon_file_for (category);
		if (icon_file && g_file_test (icon_file, G_FILE_TEST_EXISTS)) {
			gchar *uri;

			uri = g_filename_to_uri (icon_file, NULL, NULL);
			g_string_append_printf (
				buffer, "<img alt=\"%s\" src=\"evo-%s\">",
				category, uri);
			g_free (uri);
		} else {
			if (iter != list)
				g_string_append_len (string, ", ", 2);

			markup = g_markup_escape_text (category, -1);
			g_string_append (string, markup);
			g_free (markup);
		}

		g_free (icon_file);
	}
	if (string->len > 0)
		g_string_append_printf (buffer, "%s", string->str);
	if (list != NULL)
		g_string_append (buffer, "</td></tr>");
	g_slist_free_full (list, g_free);
	g_string_free (string, TRUE);

	/* write location */
	location = e_cal_component_get_location (comp);
	if (location && *location) {
		markup = g_markup_escape_text (_("Location:"), -1);
		g_string_append_printf (buffer, "<tr><th>%s</th>", markup);
		g_free (markup);

		markup = camel_text_to_html (location,
			CAMEL_MIME_FILTER_TOHTML_CONVERT_NL |
			CAMEL_MIME_FILTER_TOHTML_CONVERT_SPACES |
			CAMEL_MIME_FILTER_TOHTML_CONVERT_URLS |
			CAMEL_MIME_FILTER_TOHTML_CONVERT_ADDRESSES, 0);
		g_string_append_printf (buffer, "<td>%s</td></tr>", markup);
		g_free (markup);
	}
	g_free (location);

	/* write start date */
	dt = e_cal_component_get_dtstart (comp);
	if (dt && e_cal_component_datetime_get_value (dt)) {
		str = timet_to_str_with_zone (dt, client, default_zone);
		cal_component_preview_add_table_line (buffer, _("Start Date:"), str);
		g_free (str);
	}
	e_cal_component_datetime_free (dt);

	/* write end date */
	dt = e_cal_component_get_vtype (comp) == E_CAL_COMPONENT_EVENT ? e_cal_component_get_dtend (comp) : NULL;
	if (dt && e_cal_component_datetime_get_value (dt)) {
		str = timet_to_str_with_zone (dt, client, default_zone);
		cal_component_preview_add_table_line (buffer, _("End Date:"), str);
		g_free (str);
	}
	e_cal_component_datetime_free (dt);

	icomp = e_cal_component_get_icalcomponent (comp);

	if (e_cal_util_component_has_recurrences (icomp)) {
		str = e_cal_recur_describe_recurrence_ex (icomp,
			calendar_config_get_week_start_day (),
			E_CAL_RECUR_DESCRIBE_RECURRENCE_FLAG_NONE,
			cal_comp_util_format_itt);

		if (str) {
			cal_component_preview_add_table_line (buffer, _("Recurs:"), str);
			g_free (str);
		}
	}

	/* write Due Date */
	dt = e_cal_component_get_vtype (comp) == E_CAL_COMPONENT_TODO ? e_cal_component_get_due (comp) : NULL;
	if (dt && e_cal_component_datetime_get_value (dt)) {
		str = timet_to_str_with_zone (dt, client, default_zone);
		cal_component_preview_add_table_line (buffer, _("Due Date:"), str);
		g_free (str);
	}
	e_cal_component_datetime_free (dt);

	prop = i_cal_component_get_first_property (icomp, I_CAL_ESTIMATEDDURATION_PROPERTY);
	if (prop) {
		ICalDuration *duration;

		duration = i_cal_property_get_estimatedduration (prop);

		if (duration) {
			gint seconds;

			seconds = i_cal_duration_as_int (duration);
			if (seconds > 0) {
				str = e_cal_util_seconds_to_string (seconds);
				cal_component_preview_add_table_line (buffer, _("Estimated duration:"), str);
				g_free (str);
			}
		}

		g_clear_object (&duration);
		g_object_unref (prop);
	}

	/* write status */
	prop = i_cal_component_get_first_property (icomp, I_CAL_STATUS_PROPERTY);
	if (prop) {
		status = e_cal_component_get_status (comp);
		tmp = cal_comp_util_status_to_localized_string (i_cal_component_isa (icomp), status);

		if (tmp)
			cal_component_preview_add_table_line (buffer, _("Status:"), tmp);

		g_object_unref (prop);
	}

	/* write priority */
	priority = e_cal_component_get_priority (comp);
	if (priority > 0) {
		if (priority <= 4)
			tmp = _("High");
		else if (priority == 5)
			tmp = _("Normal");
		else
			tmp = _("Low");

		cal_component_preview_add_table_line (buffer, _("Priority:"), tmp);
	}

	prop = i_cal_component_get_first_property (icomp, I_CAL_CLASS_PROPERTY);
	if (prop) {
		switch (i_cal_property_get_class (prop)) {
		case I_CAL_CLASS_PRIVATE:
			tmp = _("Private");
			break;
		case I_CAL_CLASS_CONFIDENTIAL:
			tmp = _("Confidential");
			break;
		default:
			tmp = NULL;
			break;
		}

		if (tmp)
			cal_component_preview_add_table_line (buffer, _("Classification:"), tmp);

		g_object_unref (prop);
	}

	if (e_cal_component_has_organizer (comp)) {
		ECalComponentOrganizer *organizer;
		const gchar *organizer_email;

		organizer = e_cal_component_get_organizer (comp);
		organizer_email = cal_comp_util_get_organizer_email (organizer);

		if (organizer_email) {
			markup = g_markup_escape_text (_("Organizer:"), -1);
			g_string_append_printf (buffer, "<tr><th>%s</th>", markup);
			g_free (markup);
			if (e_cal_component_organizer_get_cn (organizer) && e_cal_component_organizer_get_cn (organizer)[0]) {
				gchar *html;

				str = g_strconcat (e_cal_component_organizer_get_cn (organizer), " <", organizer_email, ">", NULL);
				html = camel_text_to_html (str,
					CAMEL_MIME_FILTER_TOHTML_CONVERT_NL |
					CAMEL_MIME_FILTER_TOHTML_CONVERT_SPACES |
					CAMEL_MIME_FILTER_TOHTML_CONVERT_ADDRESSES, 0);
				g_string_append_printf (buffer, "<td>%s</td></tr>", html);
				g_free (html);
				g_free (str);
			} else {
				str = camel_text_to_html (organizer_email,
					CAMEL_MIME_FILTER_TOHTML_CONVERT_NL |
					CAMEL_MIME_FILTER_TOHTML_CONVERT_SPACES |
					CAMEL_MIME_FILTER_TOHTML_CONVERT_ADDRESSES, 0);
				g_string_append_printf (buffer, "<td>%s</td></tr>", str);
				g_free (str);
			}
		}

		e_cal_component_organizer_free (organizer);
	}

	if (e_cal_component_has_attendees (comp)) {
		GSList *attendees = NULL, *a;
		gboolean have = FALSE;

		attendees = e_cal_component_get_attendees (comp);

		for (a = attendees; a; a = a->next) {
			ECalComponentAttendee *attnd = a->data;
			const gchar *email = cal_comp_util_get_attendee_email (attnd);

			if (!attnd || !email || !*email)
				continue;

			if (!have) {
				markup = g_markup_escape_text (_("Attendees:"), -1);
				g_string_append_printf (buffer, "<tr><th>%s</th><td>", markup);
				g_free (markup);
			} else {
				g_string_append (buffer, "<br>");
			}

			if (!email)
				email = "";

			if (e_cal_component_attendee_get_cn (attnd) && e_cal_component_attendee_get_cn (attnd)[0]) {
				gchar *html;

				str = g_strconcat (e_cal_component_attendee_get_cn (attnd), " <", email, ">", NULL);
				html = camel_text_to_html (str,
					CAMEL_MIME_FILTER_TOHTML_CONVERT_NL |
					CAMEL_MIME_FILTER_TOHTML_CONVERT_URLS |
					CAMEL_MIME_FILTER_TOHTML_CONVERT_SPACES |
					CAMEL_MIME_FILTER_TOHTML_CONVERT_ADDRESSES, 0);
				g_string_append (buffer, html);
				g_free (html);
				g_free (str);
			} else {
				str = camel_text_to_html (email,
					CAMEL_MIME_FILTER_TOHTML_CONVERT_NL |
					CAMEL_MIME_FILTER_TOHTML_CONVERT_URLS |
					CAMEL_MIME_FILTER_TOHTML_CONVERT_SPACES |
					CAMEL_MIME_FILTER_TOHTML_CONVERT_ADDRESSES, 0);
				g_string_append (buffer, str);
				g_free (str);
			}

			have = TRUE;
		}

		if (have)
			g_string_append (buffer, "</td></tr>");

		g_slist_free_full (attendees, e_cal_component_attendee_free);
	}

	/* URL */
	url = e_cal_component_get_url (comp);
	if (url) {
		gchar *scheme, *header_markup;
		const gchar *href = url;

		str = NULL;

		scheme = g_uri_parse_scheme (url);
		if (!scheme || !*scheme) {
			str = g_strconcat ("http://", url, NULL);
			href = str;
		}

		g_free (scheme);

		if (strchr (href, '\"')) {
			markup = g_markup_escape_text (href, -1);
			g_free (str);
			str = markup;
			href = str;
		}

		header_markup = g_markup_escape_text (_("Web Page:"), -1);
		markup = g_markup_escape_text (url, -1);

		g_string_append_printf (
			buffer, "<tr><th>%s</th><td><a href=\"%s\">%s</a></td></tr>",
			header_markup, href, markup);

		g_free (header_markup);
		g_free (markup);
		g_free (str);
		g_free (url);
	}

	g_string_append (buffer, "<tr><td colspan=\"2\"><hr></td></tr>");

	/* Write description as the last, using full width */

	list = e_cal_component_get_descriptions (comp);
	if (list) {
		GSList *node;
		gboolean has_header = FALSE;

		for (node = list; node != NULL; node = node->next) {
			gchar *html;

			text = node->data;
			if (!text || !e_cal_component_text_get_value (text))
				continue;

			html = camel_text_to_html (
				e_cal_component_text_get_value (text),
				CAMEL_MIME_FILTER_TOHTML_CONVERT_NL |
				CAMEL_MIME_FILTER_TOHTML_CONVERT_SPACES |
				CAMEL_MIME_FILTER_TOHTML_CONVERT_URLS |
				CAMEL_MIME_FILTER_TOHTML_CONVERT_ADDRESSES, 0);

			if (html) {
				if (!has_header) {
					has_header = TRUE;

					g_string_append (buffer, "<tr><td colspan=\"2\" class=\"description\">");
				}

				g_string_append_printf (buffer, "%s", html);
			}

			g_free (html);
		}

		if (has_header)
			g_string_append (buffer, "</td></tr>");

		g_slist_free_full (list, e_cal_component_text_free);
	}

	g_string_append (buffer, "</table>");

	/* close document */
	g_string_append (buffer, "</body></html>");
}

static void
load_comp (ECalComponentPreview *preview)
{
	GString *buffer;

	if (!preview->priv->comp) {
		e_cal_component_preview_clear (preview);
		return;
	}

	buffer = g_string_sized_new (4096);
	cal_component_preview_write_html (preview, buffer);
	e_web_view_load_string (E_WEB_VIEW (preview), buffer->str);
	g_string_free (buffer, TRUE);
}

static void
cal_component_preview_web_process_crashed_cb (ECalComponentPreview *preview)
{
	EAlertSink *alert_sink;
	const gchar *tagid;

	g_return_if_fail (E_IS_CAL_COMPONENT_PREVIEW (preview));

	tagid = "system:webkit-web-process-crashed";

	if (preview->priv->comp) {
		ECalComponentVType vtype;

		vtype = e_cal_component_get_vtype (preview->priv->comp);
		if (vtype == E_CAL_COMPONENT_EVENT)
			tagid = "calendar:webkit-web-process-crashed-event";
		else if (vtype == E_CAL_COMPONENT_TODO)
			tagid = "calendar:webkit-web-process-crashed-task";
		else if (vtype == E_CAL_COMPONENT_JOURNAL)
			tagid = "calendar:webkit-web-process-crashed-memo";
	}

	/* Cannot use the EWebView, because it places the alerts inside itself */
	alert_sink = e_shell_utils_find_alternate_alert_sink (GTK_WIDGET (preview));
	if (alert_sink)
		e_alert_submit (alert_sink, tagid, NULL);
}

static void
cal_component_preview_finalize (GObject *object)
{
	clear_comp_info (E_CAL_COMPONENT_PREVIEW (object));

	/* Chain up to parent's finalize() method. */
	G_OBJECT_CLASS (e_cal_component_preview_parent_class)->finalize (object);
}

static void
e_cal_component_preview_class_init (ECalComponentPreviewClass *class)
{
	GObjectClass *object_class;

	g_type_class_add_private (class, sizeof (ECalComponentPreviewPrivate));

	object_class = G_OBJECT_CLASS (class);
	object_class->finalize = cal_component_preview_finalize;
}

static void
e_cal_component_preview_init (ECalComponentPreview *preview)
{
	preview->priv = E_CAL_COMPONENT_PREVIEW_GET_PRIVATE (preview);

	g_signal_connect (
		preview, "web-process-crashed",
		G_CALLBACK (cal_component_preview_web_process_crashed_cb), NULL);
}

GtkWidget *
e_cal_component_preview_new (void)
{
	return g_object_new (E_TYPE_CAL_COMPONENT_PREVIEW, NULL);
}

void
e_cal_component_preview_display (ECalComponentPreview *preview,
                                 ECalClient *client,
                                 ECalComponent *comp,
                                 ICalTimezone *zone,
                                 gboolean use_24_hour_format)
{
	g_return_if_fail (E_IS_CAL_COMPONENT_PREVIEW (preview));
	g_return_if_fail (E_IS_CAL_COMPONENT (comp));

	/* do not update preview when setting the same component as last time,
	 * which even didn't change */
	if (!update_comp_info (preview, client, comp, zone, use_24_hour_format))
		return;

	load_comp (preview);
}

void
e_cal_component_preview_clear (ECalComponentPreview *preview)
{
	g_return_if_fail (E_IS_CAL_COMPONENT_PREVIEW (preview));

	clear_comp_info (preview);
	e_web_view_clear (E_WEB_VIEW (preview));
}
