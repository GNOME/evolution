/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* e-tasks.c
 *
 * Copyright (C) 2001-2003  Ximian, Inc.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of version 2 of the GNU General Public
 * License as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 * Authors: Federico Mena Quintero <federico@ximian.com>
 *	    Damon Chaplin <damon@ximian.com>
 *          Rodrigo Moya <rodrigo@ximian.com>
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <gnome.h>
#include <gtk/gtk.h>
#include <libgnomevfs/gnome-vfs-ops.h>
#include <libecal/e-cal-time-util.h>
#include <gal/util/e-util.h>
#include <gtkhtml/gtkhtml.h>
#include <gtkhtml/gtkhtml-stream.h>
#include <e-util/e-time-utils.h>
#include <e-util/e-categories-config.h>
#include "calendar-config.h"
#include "e-cal-component-preview.h"

struct _ECalComponentPreviewPrivate {
	GtkWidget *html;

	icaltimezone *zone;
};

static GtkTableClass *parent_class;


static void
on_link_clicked (GtkHTML *html, const char *url, gpointer data)
{
        GError *err = NULL;

        gnome_url_show (url, &err);

	if (err) {
		g_warning ("gnome_url_show: %s", err->message);
                g_error_free (err);
        }
}

static void
on_url_cb (GtkHTML *html, const char *url, gpointer data)
{
#if 0
	char *msg;
	ECalComponentPreview *preview = data;

	if (url && *url) {
		msg = g_strdup_printf (_("Click to open %s"), url);
		e_calendar_table_set_status_message (e_tasks_get_calendar_table (tasks), msg);
		g_free (msg);
	} else
		e_calendar_table_set_status_message (e_tasks_get_calendar_table (tasks), NULL);
#endif
}

/* Callback used when the user selects a URL in the HTML widget */
static void
url_requested_cb (GtkHTML *html, const char *url, GtkHTMLStream *stream, gpointer data)
{
	if (!strncmp ("file:///", url, strlen ("file:///"))) {
		GnomeVFSHandle *handle;
		GnomeVFSResult result;
		char buffer[4096];

		if (gnome_vfs_open (&handle, url, GNOME_VFS_OPEN_READ) == GNOME_VFS_OK) {
			do {
				GnomeVFSFileSize bread;

				result = gnome_vfs_read (handle, buffer, sizeof (buffer), &bread);
				if (result == GNOME_VFS_OK)
					gtk_html_stream_write (stream, buffer, bread);
			} while (result == GNOME_VFS_OK);

			gnome_vfs_close (handle);
		}
	}
}

/* Converts a time_t to a string, relative to the specified timezone */
static char *
timet_to_str_with_zone (ECalComponentDateTime *dt, ECal *ecal, icaltimezone *default_zone)
{
	struct icaltimetype itt;
	icaltimezone *zone;
        struct tm tm;
        char buf[256];                                                                                              

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
write_html (GtkHTMLStream *stream, ECal *ecal, ECalComponent *comp, icaltimezone *default_zone)
{
	ECalComponentText text;
	ECalComponentDateTime dt;
	gchar *str;
	GSList *l;
	icalproperty_status status;
	const char *location;
	int *priority_value;

	g_return_if_fail (E_IS_CAL_COMPONENT (comp));

	/* write document header */
	e_cal_component_get_summary (comp, &text);

	if (text.value)
		gtk_html_stream_printf (stream,
					"<HTML><BODY><H1>%s</H1>",
					text.value);
	else
		gtk_html_stream_printf (stream,
					"<HTML><BODY><H1><I>%s</I></H1>",
					_("Untitled"));


	/* write icons for the categories */
	e_cal_component_get_categories_list (comp, &l);
	if (l) {
		GSList *node;

		for (node = l; node != NULL; node = node->next) {
			const char *icon_file;

			icon_file = e_categories_config_get_icon_file_for ((const char *) node->data);
			if (icon_file) {
				gtk_html_stream_printf (stream, "<IMG ALT=\"%s\" SRC=\"file://%s\">",
							(const char *) node->data, icon_file);
			}
		}

		e_cal_component_free_categories_list (l);

		gtk_html_stream_printf (stream, "<BR><BR><BR>");
	}

	/* Start table */
	gtk_html_stream_printf (stream, "<TABLE BORDER=\"0\" WIDTH=\"80%%\">"
				"<TR><TD VALIGN=\"TOP\" ALIGN=\"RIGHT\" WIDTH=\"15%%\"></TD></TR>");	

	/* write location */
	e_cal_component_get_location (comp, &location);
	if (location)
		gtk_html_stream_printf (stream, "<TR><TD VALIGN=\"TOP\" ALIGN=\"RIGHT\" WIDTH=\"15%%\"><B>%s</B></TD><TD>%s</TD></TR>",
					_("Summary:"), text.value);

	/* write start date */
	e_cal_component_get_dtstart (comp, &dt);
	if (dt.value != NULL) {
		str = timet_to_str_with_zone (&dt, ecal, default_zone);
		gtk_html_stream_printf (stream, "<TR><TD VALIGN=\"TOP\" ALIGN=\"RIGHT\"><B>%s</B></TD><TD>%s</TD></TR>",
					_("Start Date:"), str);

		e_cal_component_free_datetime (&dt);
		g_free (str);
	}

	/* write end date */
	e_cal_component_get_dtend (comp, &dt);
	if (dt.value != NULL) {
		str = timet_to_str_with_zone (&dt, ecal, default_zone);
		gtk_html_stream_printf (stream, "<TR><TD VALIGN=\"TOP\" ALIGN=\"RIGHT\"><B>%s</B></TD><TD>%s</TD></TR>",
					_("Start Date:"), str);

		e_cal_component_free_datetime (&dt);
		g_free (str);
	}
	
	/* write Due Date */
	e_cal_component_get_due (comp, &dt);
	if (dt.value != NULL) {
		str = timet_to_str_with_zone (&dt, ecal, default_zone);
		gtk_html_stream_printf (stream, "<TR><TD VALIGN=\"TOP\" ALIGN=\"RIGHT\"><B>%s</B></TD><TD>%s</TD></TR>",
					_("Due Date:"), str);

		e_cal_component_free_datetime (&dt);
		g_free (str);
	}
	
	/* write status */
	gtk_html_stream_printf (stream, "<TR><TD VALIGN=\"TOP\" ALIGN=\"RIGHT\"><B>%s</B></TD>", _("Status:"));
	e_cal_component_get_status (comp, &status);
	switch (status) {
	case ICAL_STATUS_INPROCESS :
		str = g_strdup (_("In Progress"));
		break;
	case ICAL_STATUS_COMPLETED :
		str = g_strdup (_("Completed"));
		break;
	case ICAL_STATUS_CANCELLED :
		str = g_strdup (_("Cancelled"));
		break;
	case ICAL_STATUS_NONE :
	default :
		str = g_strdup (_("Not Started"));
		break;
	}

	gtk_html_stream_printf (stream, "<TD>%s</TD></TR>", str);
	g_free (str);

	/* write priority */
	e_cal_component_get_priority (comp, &priority_value);
	if (priority_value && *priority_value != 0) {
		gtk_html_stream_printf (stream, "<TR><TD VALIGN=\"TOP\" ALIGN=\"RIGHT\"><B>%s</B></TD>", _("Priority:"));
		if (*priority_value <= 4)
			str = g_strdup (_("High"));
		else if (*priority_value == 5)
			str = g_strdup (_("Normal"));
		else
			str = g_strdup (_("Low"));

		gtk_html_stream_printf (stream, "<TD>%s</TD></TR>", str);

		g_free (str);
		e_cal_component_free_priority (priority_value);
	}

	/* write description and URL */
	gtk_html_stream_printf (stream, "<TR><TD COLSPAN=\"2\"><HR></TD></TR>");

	e_cal_component_get_description_list (comp, &l);
	if (l) {
		GSList *node;

		gtk_html_stream_printf (stream, "<TR><TD VALIGN=\"TOP\" ALIGN=\"RIGHT\"><B>%s</B></TD>", _("Description:"));

		gtk_html_stream_printf (stream, "<TD>");

		for (node = l; node != NULL; node = node->next) {
			gint i;
			GString *str = g_string_new ("");;

			text = * (ECalComponentText *) node->data;
			for (i = 0; i < strlen (text.value ? text.value : 0); i++) {
				if (text.value[i] == '\n')
					str = g_string_append (str, "<BR>");
				else if (text.value[i] == '<')
					str = g_string_append (str, "&lt;");
				else if (text.value[i] == '>')
					str = g_string_append (str, "&gt;");
				else
					str = g_string_append_c (str, text.value[i]);
			}

			gtk_html_stream_printf (stream, str->str);
			g_string_free (str, TRUE);
		}

		gtk_html_stream_printf (stream, "</TD></TR>");

		e_cal_component_free_text_list (l);
	}

	/* URL */
	e_cal_component_get_url (comp, (const char **) &str);
	if (str) {
		gtk_html_stream_printf (stream, "<TR><TD VALIGN=\"TOP\" ALIGN=\"RIGHT\"><B>%s</B></TD>", _("Web Page:"));
		gtk_html_stream_printf (stream, "<TD><A HREF=\"%s\">%s</A></TD></TR>", str, str);
	}
	
	gtk_html_stream_printf (stream, "</TABLE>");

	/* close document */
	gtk_html_stream_printf (stream, "</BODY></HTML>");
}

static void
e_cal_component_preview_init (ECalComponentPreview *preview)
{
	ECalComponentPreviewPrivate *priv;
	GtkWidget *scroll;
	
	priv = g_new0 (ECalComponentPreviewPrivate, 1);
	preview->priv = priv;

	priv->html = gtk_html_new ();
	gtk_html_set_default_content_type (GTK_HTML (priv->html), "charset=utf-8");
	gtk_html_load_empty (GTK_HTML (priv->html));

	g_signal_connect (G_OBJECT (priv->html), "url_requested",
			  G_CALLBACK (url_requested_cb), NULL);
	g_signal_connect (G_OBJECT (priv->html), "link_clicked",
			  G_CALLBACK (on_link_clicked), preview);
	g_signal_connect (G_OBJECT (priv->html), "on_url",
			  G_CALLBACK (on_url_cb), preview);

	scroll = gtk_scrolled_window_new (NULL, NULL);
	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scroll),
					GTK_POLICY_AUTOMATIC,
					GTK_POLICY_AUTOMATIC);
	gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (scroll), GTK_SHADOW_IN);

	gtk_container_add (GTK_CONTAINER (scroll), priv->html);
	gtk_container_add (GTK_CONTAINER (preview), scroll);
	gtk_widget_show_all (scroll);
	
	priv->zone = icaltimezone_get_utc_timezone ();
}

static void
e_cal_component_preview_destroy (GtkObject *object)
{
	ECalComponentPreview *preview;
	ECalComponentPreviewPrivate *priv;

	g_return_if_fail (object != NULL);
	g_return_if_fail (E_IS_CAL_COMPONENT_PREVIEW (object));

	preview = E_CAL_COMPONENT_PREVIEW (object);
	priv = preview->priv;

	if (priv) {

		g_free (priv);
		preview->priv = NULL;
	}

	if (GTK_OBJECT_CLASS (parent_class)->destroy)
		(* GTK_OBJECT_CLASS (parent_class)->destroy) (object);
}

static void
e_cal_component_preview_class_init (ECalComponentPreviewClass *klass)
{
	GtkObjectClass *object_class;
	
	object_class = (GtkObjectClass *) klass;
	
	parent_class = g_type_class_peek_parent (klass);
	
	object_class->destroy = e_cal_component_preview_destroy;
}

E_MAKE_TYPE (e_cal_component_preview, "ECalComponentPreview", ECalComponentPreview,
	     e_cal_component_preview_class_init, e_cal_component_preview_init,
	     GTK_TYPE_TABLE)

GtkWidget *
e_cal_component_preview_new (void)
{
	ECalComponentPreview *preview;

	preview = g_object_new (e_cal_component_preview_get_type (), NULL);

	return GTK_WIDGET (preview);
}

icaltimezone *
e_cal_component_preview_get_default_timezone (ECalComponentPreview *preview)
{
	ECalComponentPreviewPrivate *priv;
	
	g_return_val_if_fail (preview != NULL, NULL);
	g_return_val_if_fail (E_IS_CAL_COMPONENT_PREVIEW (preview), NULL);

	priv = preview->priv;

	return priv->zone;
}

void
e_cal_component_preview_set_default_timezone (ECalComponentPreview *preview, icaltimezone *zone)
{
	ECalComponentPreviewPrivate *priv;
	
	g_return_if_fail (preview != NULL);
	g_return_if_fail (E_IS_CAL_COMPONENT_PREVIEW (preview));
	g_return_if_fail (zone != NULL);

	priv = preview->priv;
	
	priv->zone = zone;
}

void
e_cal_component_preview_display (ECalComponentPreview *preview, ECal *ecal, ECalComponent *comp)
{
	ECalComponentPreviewPrivate *priv;
	GtkHTMLStream *stream;

	g_return_if_fail (preview != NULL);
	g_return_if_fail (E_IS_CAL_COMPONENT_PREVIEW (preview));
	g_return_if_fail (comp != NULL);
	g_return_if_fail (E_IS_CAL_COMPONENT (comp));

	priv = preview->priv;
	
	stream = gtk_html_begin (GTK_HTML (priv->html));	
	write_html (stream, ecal, comp, priv->zone);
	gtk_html_stream_close (stream, GTK_HTML_STREAM_OK);
}

void
e_cal_component_preview_clear (ECalComponentPreview *preview)
{
	ECalComponentPreviewPrivate *priv;

	g_return_if_fail (preview != NULL);
	g_return_if_fail (E_IS_CAL_COMPONENT_PREVIEW (preview));
	
	priv = preview->priv;
	
	gtk_html_load_empty (GTK_HTML (priv->html));
}

