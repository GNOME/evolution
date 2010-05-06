/*
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
 *
 * Authors:
 *		Federico Mena Quintero <federico@ximian.com>
 *	    Damon Chaplin <damon@ximian.com>
 *      Rodrigo Moya <rodrigo@ximian.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#include "e-cal-component-preview.h"

#include <string.h>
#include <gtk/gtk.h>
#include <glib/gi18n.h>
#include <camel/camel.h>
#include <libecal/e-cal-time-util.h>
#include <libedataserver/e-categories.h>
#include <libedataserver/e-time-utils.h>
#include <e-util/e-util.h>
#include <e-util/e-categories-config.h>
#include "calendar-config.h"

#define E_CAL_COMPONENT_PREVIEW_GET_PRIVATE(obj) \
	(G_TYPE_INSTANCE_GET_PRIVATE \
	((obj), E_TYPE_CAL_COMPONENT_PREVIEW, ECalComponentPreviewPrivate))

struct _ECalComponentPreviewPrivate {
	icaltimezone *zone;

	/* information about currently showing component in a preview;
	   if it didn't change then the preview is not updated */
	gchar *cal_uid;
	gchar *comp_uid;
	struct icaltimetype comp_last_modified;
	gint comp_sequence;
};

static gpointer parent_class;

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
}

/* Stores information about actually shown component and
   returns whether component in the preview changed */
static gboolean
update_comp_info (ECalComponentPreview *preview,
                  ECal *ecal,
                  ECalComponent *comp)
{
	ECalComponentPreviewPrivate *priv;
	gboolean changed;

	g_return_val_if_fail (preview != NULL, TRUE);
	g_return_val_if_fail (E_IS_CAL_COMPONENT_PREVIEW (preview), TRUE);

	priv = preview->priv;

	if (!E_IS_CAL_COMPONENT (comp) || !E_IS_CAL (ecal)) {
		changed = !priv->cal_uid;
		clear_comp_info (preview);
	} else {
		const gchar *uid;
		gchar *cal_uid;
		gchar *comp_uid;
		struct icaltimetype comp_last_modified, *itm = NULL;
		gint *sequence = NULL;
		gint comp_sequence;

		cal_uid = g_strdup (e_source_peek_uid (e_cal_get_source (ecal)));
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
	}

	return changed;
}

/* Converts a time_t to a string, relative to the specified timezone */
static gchar *
timet_to_str_with_zone (ECalComponentDateTime *dt,
                        ECal *ecal,
                        icaltimezone *default_zone)
{
	struct icaltimetype itt;
	icaltimezone *zone;
        struct tm tm;
        gchar buf[256];

	if (dt->tzid) {
		/* If we can't find the zone, we'll guess its "local" */
		if (!e_cal_get_timezone (ecal, dt->tzid, &zone, NULL))
			zone = NULL;
	} else if (dt->value->is_utc) {
		zone = icaltimezone_get_utc_timezone ();
	} else {
		zone = NULL;
	}

	itt = *dt->value;
	if (zone)
		icaltimezone_convert_time (&itt, zone, default_zone);
        tm = icaltimetype_to_tm (&itt);

        e_time_format_date_and_time (&tm, calendar_config_get_24_hour_format (),
                                     FALSE, FALSE, buf, sizeof (buf));

	return g_locale_to_utf8 (buf, -1, NULL, NULL, NULL);
}

static void
cal_component_preview_write_html (GString *buffer,
                                  ECal *ecal,
                                  ECalComponent *comp,
                                  icaltimezone *default_zone)
{
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

	g_return_if_fail (E_IS_CAL_COMPONENT (comp));

	/* write document header */
	e_cal_component_get_summary (comp, &text);

	if (text.value)
		g_string_append_printf (
			buffer, "<HTML><BODY><H1>%s</H1>",
			text.value);
	else
		g_string_append_printf (
			buffer, "<HTML><BODY><H1><I>%s</I></H1>",
			_("Untitled"));

	/* write icons for the categories */
	string = g_string_new (NULL);
	e_cal_component_get_categories_list (comp, &list);
	if (list != NULL)
		g_string_append_printf (buffer, "<H3>%s ", _("Categories:"));
	for (iter = list; iter != NULL; iter = iter->next) {
		const gchar *category = iter->data;
		const gchar *icon_file;

		icon_file = e_categories_get_icon_file_for (category);
		if (icon_file && g_file_test (icon_file, G_FILE_TEST_EXISTS)) {
			gchar *uri;

			uri = g_filename_to_uri (icon_file, NULL, NULL);
			g_string_append_printf (
				buffer, "<IMG ALT=\"%s\" SRC=\"%s\">",
				category, uri);
			g_free (uri);
		} else {
			if (iter != list)
				g_string_append_len (string, ", ", 2);
			g_string_append (string, category);
		}
	}
	if (string->len > 0)
		g_string_append_printf (buffer, "%s", string->str);
	if (list != NULL)
		g_string_append (buffer, "</H3>");
	e_cal_component_free_categories_list (list);
	g_string_free (string, TRUE);

	/* Start table */
	g_string_append (
		buffer, "<TABLE BORDER=\"0\" WIDTH=\"80%%\">"
		"<TR><TD VALIGN=\"TOP\" ALIGN=\"RIGHT\" WIDTH=\"15%%\">"
		"</TD></TR>");

	/* write location */
	e_cal_component_get_location (comp, &location);
	if (location)
		g_string_append_printf (
			buffer, "<TR><TD VALIGN=\"TOP\" ALIGN=\"RIGHT\" "
			"WIDTH=\"15%%\"><B>%s</B></TD><TD>%s</TD></TR>",
			_("Summary:"), text.value);

	/* write start date */
	e_cal_component_get_dtstart (comp, &dt);
	if (dt.value != NULL) {
		str = timet_to_str_with_zone (&dt, ecal, default_zone);
		g_string_append_printf (
			buffer, "<TR><TD VALIGN=\"TOP\" ALIGN=\"RIGHT\">"
			"<B>%s</B></TD><TD>%s</TD></TR>",
			_("Start Date:"), str);

		g_free (str);
	}
	e_cal_component_free_datetime (&dt);

	/* write end date */
	e_cal_component_get_dtend (comp, &dt);
	if (dt.value != NULL) {
		str = timet_to_str_with_zone (&dt, ecal, default_zone);
		g_string_append_printf (
			buffer, "<TR><TD VALIGN=\"TOP\" ALIGN=\"RIGHT\">"
			"<B>%s</B></TD><TD>%s</TD></TR>",
			_("Start Date:"), str);

		g_free (str);
	}
	e_cal_component_free_datetime (&dt);

	/* write Due Date */
	e_cal_component_get_due (comp, &dt);
	if (dt.value != NULL) {
		str = timet_to_str_with_zone (&dt, ecal, default_zone);
		g_string_append_printf (
			buffer, "<TR><TD VALIGN=\"TOP\" ALIGN=\"RIGHT\">"
			"<B>%s</B></TD><TD>%s</TD></TR>",
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
			buffer, "<TR><TD VALIGN=\"TOP\" ALIGN=\"RIGHT\">"
			"<B>%s</B></TD>", _("Status:"));
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

		g_string_append_printf (buffer, "<TD>%s</TD></TR>", str);
		g_free (str);
	}

	/* write priority */
	e_cal_component_get_priority (comp, &priority_value);
	if (priority_value && *priority_value != 0) {
		g_string_append_printf (
			buffer, "<TR><TD VALIGN=\"TOP\" ALIGN=\"RIGHT\">"
			"<B>%s</B></TD>", _("Priority:"));
		if (*priority_value <= 4)
			str = g_strdup (_("High"));
		else if (*priority_value == 5)
			str = g_strdup (_("Normal"));
		else
			str = g_strdup (_("Low"));

		g_string_append_printf (buffer, "<TD>%s</TD></TR>", str);

		g_free (str);
	}

	if (priority_value)
		e_cal_component_free_priority (priority_value);

	/* write description and URL */
	g_string_append (buffer, "<TR><TD COLSPAN=\"2\"><HR></TD></TR>");

	e_cal_component_get_description_list (comp, &list);
	if (list) {
		GSList *node;

		g_string_append_printf (
			buffer, "<TR><TD VALIGN=\"TOP\" ALIGN=\"RIGHT\">"
			"<B>%s</B></TD>", _("Description:"));

		g_string_append (buffer, "<TD><TT>");

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

		g_string_append (buffer, "</TT></TD></TR>");

		e_cal_component_free_text_list (list);
	}

	/* URL */
	e_cal_component_get_url (comp, (const gchar **) &str);
	if (str) {
		g_string_append_printf (
			buffer, "<TR><TD VALIGN=\"TOP\" ALIGN=\"RIGHT\">"
			"<B>%s</B></TD>", _("Web Page:"));
		g_string_append_printf (
			buffer, "<TD><A HREF=\"%s\">%s</A></TD></TR>",
			str, str);
	}

	g_string_append (buffer, "</TABLE>");

	/* close document */
	g_string_append (buffer, "</BODY></HTML>");
}

static void
cal_component_preview_finalize (GObject *object)
{
	clear_comp_info (E_CAL_COMPONENT_PREVIEW (object));

	/* Chain up to parent's finalize() method. */
	G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
cal_component_preview_class_init (ECalComponentPreviewClass *class)
{
	GObjectClass *object_class;

	parent_class = g_type_class_peek_parent (class);
	g_type_class_add_private (class, sizeof (ECalComponentPreviewPrivate));

	object_class = G_OBJECT_CLASS (class);
	object_class->finalize = cal_component_preview_finalize;
}

static void
cal_component_preview_init (ECalComponentPreview *preview)
{
	preview->priv = E_CAL_COMPONENT_PREVIEW_GET_PRIVATE (preview);

	preview->priv->zone = icaltimezone_get_utc_timezone ();
}

GType
e_cal_component_preview_get_type (void)
{
	static GType type = 0;

	if (G_UNLIKELY (type == 0)) {
		static const GTypeInfo type_info = {
			sizeof (ECalComponentPreviewClass),
			(GBaseInitFunc) NULL,
			(GBaseFinalizeFunc) NULL,
			(GClassInitFunc) cal_component_preview_class_init,
			(GClassFinalizeFunc) NULL,
			NULL,  /* class_data */
			sizeof (ECalComponentPreview),
			0,     /* n_preallocs */
			(GInstanceInitFunc) cal_component_preview_init,
			NULL   /* value_table */
		};

		type = g_type_register_static (
			E_TYPE_WEB_VIEW, "ECalComponentPreview",
			&type_info, 0);
	}

	return type;
}

GtkWidget *
e_cal_component_preview_new (void)
{
	return g_object_new (E_TYPE_CAL_COMPONENT_PREVIEW, NULL);
}

icaltimezone *
e_cal_component_preview_get_default_timezone (ECalComponentPreview *preview)
{
	g_return_val_if_fail (E_IS_CAL_COMPONENT_PREVIEW (preview), NULL);

	return preview->priv->zone;
}

void
e_cal_component_preview_set_default_timezone (ECalComponentPreview *preview,
                                              icaltimezone *zone)
{
	g_return_if_fail (E_IS_CAL_COMPONENT_PREVIEW (preview));
	g_return_if_fail (zone != NULL);

	preview->priv->zone = zone;
}

void
e_cal_component_preview_display (ECalComponentPreview *preview,
                                 ECal *ecal,
                                 ECalComponent *comp)
{
	GString *buffer;

	g_return_if_fail (E_IS_CAL_COMPONENT_PREVIEW (preview));
	g_return_if_fail (E_IS_CAL_COMPONENT (comp));

	/* do not update preview when setting the same component as last time,
	   which even didn't change */
	if (!update_comp_info (preview, ecal, comp))
		return;

	/* XXX The initial buffer size is arbitrary.  Tune it. */

	buffer = g_string_sized_new (4096);
	cal_component_preview_write_html (
		buffer, ecal, comp, preview->priv->zone);
	e_web_view_load_string (E_WEB_VIEW (preview), buffer->str);
	g_string_free (buffer, TRUE);
}

void
e_cal_component_preview_clear (ECalComponentPreview *preview)
{
	g_return_if_fail (E_IS_CAL_COMPONENT_PREVIEW (preview));

	clear_comp_info (preview);
	e_web_view_clear (E_WEB_VIEW (preview));
}
