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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "e-cal-component-preview.h"

#include <string.h>
#include <gtk/gtk.h>
#include <glib/gi18n.h>
#include <camel/camel.h>

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
	struct icaltimetype comp_last_modified;
	gint comp_sequence;

	ECalClient *client;
	ECalComponent *comp;
	icaltimezone *timezone;
	gboolean use_24_hour_format;
};

#define HTML_HEADER "<!doctype html public \"-//W3C//DTD HTML 4.0 TRANSITIONAL//EN\">\n<html>\n" \
                    "<head>\n<meta name=\"generator\" content=\"Evolution Calendar Component\">\n" \
		    "<link type=\"text/css\" rel=\"stylesheet\" href=\"evo-file://" EVOLUTION_PRIVDATADIR "/theme/webview.css\">\n" \
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
	priv->comp_last_modified = icaltime_null_time ();
	priv->comp_sequence = -1;

	g_clear_object (&priv->client);
	g_clear_object (&priv->comp);
	if (priv->timezone) {
		icaltimezone_free (priv->timezone, 1);
		priv->timezone = NULL;
	}
}

/* Stores information about actually shown component and
 * returns whether component in the preview changed */
static gboolean
update_comp_info (ECalComponentPreview *preview,
                  ECalClient *client,
                  ECalComponent *comp,
                  icaltimezone *zone,
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
		struct icaltimetype comp_last_modified, *itm = NULL;
		gint *sequence = NULL;
		gint comp_sequence;

		source = e_client_get_source (E_CLIENT (client));
		cal_uid = g_strdup (e_source_get_uid (source));
		e_cal_component_get_uid (comp, &uid);
		comp_uid = g_strdup (uid);
		e_cal_component_get_last_modified (comp, &itm);
		if (itm) {
			comp_last_modified = *itm;
			e_cal_component_free_icaltimetype (itm);
		} else
			comp_last_modified = icaltime_null_time ();
		e_cal_component_get_sequence (comp, &sequence);
		if (sequence) {
			comp_sequence = *sequence;
			e_cal_component_free_sequence (sequence);
		} else
			comp_sequence = 0;

		changed = !priv->cal_uid || !priv->comp_uid || !cal_uid || !comp_uid ||
			  !g_str_equal (priv->cal_uid, cal_uid) ||
			  !g_str_equal (priv->comp_uid, comp_uid) ||
			  priv->comp_sequence != comp_sequence ||
			  icaltime_compare (priv->comp_last_modified, comp_last_modified) != 0;

		clear_comp_info (preview);

		priv->cal_uid = cal_uid;
		priv->comp_uid = comp_uid;
		priv->comp_sequence = comp_sequence;
		priv->comp_last_modified = comp_last_modified;

		priv->comp = g_object_ref (comp);
		priv->client = g_object_ref (client);
		priv->timezone = icaltimezone_copy (zone);
		priv->use_24_hour_format = use_24_hour_format;
	}

	return changed;
}

/* Converts a time_t to a string, relative to the specified timezone */
static gchar *
timet_to_str_with_zone (ECalComponentDateTime *dt,
                        ECalClient *client,
                        icaltimezone *default_zone)
{
	struct icaltimetype itt;
	icaltimezone *zone = NULL;
	struct tm tm;

	if (dt->tzid != NULL) {
		e_cal_client_get_timezone_sync (
			client, dt->tzid, &zone, NULL, NULL);
	} else if (dt->value->is_utc) {
		zone = icaltimezone_get_utc_timezone ();
	}

	itt = *dt->value;
	if (zone != NULL)
		icaltimezone_convert_time (&itt, zone, default_zone);
	tm = icaltimetype_to_tm (&itt);

	return e_datetime_format_format_tm ("calendar", "table", itt.is_date ? DTFormatKindDate : DTFormatKindDateTime, &tm);
}

static void
cal_component_preview_write_html (ECalComponentPreview *preview,
                                  GString *buffer)
{
	ECalClient *client;
	ECalComponent *comp;
	icaltimezone *default_zone;
	ECalComponentText text;
	ECalComponentDateTime dt;
	gchar *str;
	GString *string;
	GSList *list, *iter;
	icalcomponent *icalcomp;
	icalproperty *icalprop;
	icalproperty_status status;
	const gchar *location;
	gint *priority_value;

	client = preview->priv->client;
	comp = preview->priv->comp;
	default_zone = preview->priv->timezone;

	/* write document header */
	e_cal_component_get_summary (comp, &text);

	g_string_append (buffer, HTML_HEADER);
	g_string_append (buffer, "<body class=\"-e-web-view-background-color -e-web-view-text-color\">");

	if (text.value)
		g_string_append_printf (buffer, "<h2>%s</h2>", text.value);
	else
		g_string_append_printf (buffer, "<h2><i>%s</i></h2>",_("Untitled"));

	g_string_append (buffer, "<table border=\"0\" cellspacing=\"5\">");

	/* write icons for the categories */
	string = g_string_new (NULL);
	e_cal_component_get_categories_list (comp, &list);
	if (list != NULL)
		g_string_append_printf (buffer, "<tr><th>%s</th><td>", _("Categories:"));
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
			g_string_append (string, category);
		}

		g_free (icon_file);
	}
	if (string->len > 0)
		g_string_append_printf (buffer, "%s", string->str);
	if (list != NULL)
		g_string_append (buffer, "</td></tr>");
	e_cal_component_free_categories_list (list);
	g_string_free (string, TRUE);

	/* write location */
	e_cal_component_get_location (comp, &location);
	if (location)
		g_string_append_printf (
			buffer, "<tr><th>%s</th><td>%s</td></tr>",
			_("Summary:"), text.value);

	/* write start date */
	e_cal_component_get_dtstart (comp, &dt);
	if (dt.value != NULL) {
		str = timet_to_str_with_zone (&dt, client, default_zone);
		g_string_append_printf (
			buffer, "<tr><th>%s</th><td>%s</td></tr>",
			_("Start Date:"), str);
		g_free (str);
	}
	e_cal_component_free_datetime (&dt);

	/* write end date */
	e_cal_component_get_dtend (comp, &dt);
	if (dt.value != NULL) {
		str = timet_to_str_with_zone (&dt, client, default_zone);
		g_string_append_printf (
			buffer,"<tr><th>%s</th><td>%s</td></tr>",
			_("End Date:"), str);
		g_free (str);
	}
	e_cal_component_free_datetime (&dt);

	/* write Due Date */
	e_cal_component_get_due (comp, &dt);
	if (dt.value != NULL) {
		str = timet_to_str_with_zone (&dt, client, default_zone);
		g_string_append_printf (
			buffer, "<tr><th>%s</th><td>%s</td></tr>",
			_("Due Date:"), str);
		g_free (str);
	}
	e_cal_component_free_datetime (&dt);

	/* write status */
	icalcomp = e_cal_component_get_icalcomponent (comp);
	icalprop = icalcomponent_get_first_property (
		icalcomp, ICAL_STATUS_PROPERTY);
	if (icalprop != NULL) {
		g_string_append_printf (
			buffer, "<tr><th>%s</th>",
			_("Status:"));
		e_cal_component_get_status (comp, &status);
		switch (status) {
		case ICAL_STATUS_INPROCESS :
			str = g_strdup (_("In Progress"));
			break;
		case ICAL_STATUS_COMPLETED :
			str = g_strdup (_("Completed"));
			break;
		case ICAL_STATUS_CANCELLED :
			str = g_strdup (_("Canceled"));
			break;
		case ICAL_STATUS_NONE :
		default :
			str = g_strdup (_("Not Started"));
			break;
		}

		g_string_append_printf (buffer, "<td>%s</td></tr>", str);
		g_free (str);
	}

	/* write priority */
	e_cal_component_get_priority (comp, &priority_value);
	if (priority_value && *priority_value != 0) {
		g_string_append_printf (
			buffer, "<tr><th>%s</th>",
			_("Priority:"));
		if (*priority_value <= 4)
			str = g_strdup (_("High"));
		else if (*priority_value == 5)
			str = g_strdup (_("Normal"));
		else
			str = g_strdup (_("Low"));

		g_string_append_printf (buffer, "<td>%s</td></tr>", str);

		g_free (str);
	}

	if (priority_value)
		e_cal_component_free_priority (priority_value);

	/* write description and URL */
	g_string_append (buffer, "<tr><td colspan=\"2\"><hr></td></tr>");

	e_cal_component_get_description_list (comp, &list);
	if (list) {
		GSList *node;

		g_string_append_printf (
			buffer, "<tr><th>%s</th>",
			_("Description:"));

		g_string_append (buffer, "<td class=\"description\">");

		for (node = list; node != NULL; node = node->next) {
			gchar *html;

			text = * (ECalComponentText *) node->data;
			html = camel_text_to_html (
				text.value ? text.value : "",
				CAMEL_MIME_FILTER_TOHTML_CONVERT_NL |
				CAMEL_MIME_FILTER_TOHTML_CONVERT_SPACES |
				CAMEL_MIME_FILTER_TOHTML_CONVERT_URLS |
				CAMEL_MIME_FILTER_TOHTML_CONVERT_ADDRESSES, 0);

			if (html)
				g_string_append_printf (buffer, "%s", html);

			g_free (html);
		}

		g_string_append (buffer, "</td></tr>");

		e_cal_component_free_text_list (list);
	}

	/* URL */
	e_cal_component_get_url (comp, (const gchar **) &str);
	if (str) {
		g_string_append_printf (
			buffer, "<tr><th>%s</th><td><a href=\"%s\">%s</a></td></tr>",
			_("Web Page:"), str, str);
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
		preview, "style-updated",
		G_CALLBACK (load_comp), NULL);
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
                                 icaltimezone *zone,
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
