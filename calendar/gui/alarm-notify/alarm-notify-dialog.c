/* Evolution calendar - alarm notification dialog
 *
 * Copyright (C) 2000 Ximian, Inc.
 * Copyright (C) 2001 Ximian, Inc.
 *
 * Author: Federico Mena-Quintero <federico@ximian.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of version 2 of the GNU General Public
 * License as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307, USA.
 */

#include <config.h>
#include <stdio.h>
#include <string.h>
#include <gtk/gtklabel.h>
#include <gtk/gtkspinbutton.h>
#include <gtk/gtksignal.h>
#include <gtk/gtkscrolledwindow.h>
#include <gtk/gtkwindow.h>
#include <libgnome/gnome-i18n.h>
#if 0 
#  include <libgnomeui/gnome-winhints.h>
#endif
#include <glade/glade.h>
#include <e-util/e-time-utils.h>
#include <gtkhtml/gtkhtml.h>
#include <gtkhtml/gtkhtml-stream.h>
#include <libecal/e-cal-time-util.h>
#include "alarm-notify-dialog.h"
#include "config-data.h"
#include "util.h"
#include <e-util/e-icon-factory.h>


GtkWidget *make_html_display (gchar *widget_name, char *s1, char *s2, int scroll, int shadow);

/* The useful contents of the alarm notify dialog */
typedef struct {
	GladeXML *xml;

	GtkWidget *dialog;
	GtkWidget *close;
	GtkWidget *snooze;
	GtkWidget *edit;
	GtkWidget *snooze_time;
	GtkWidget *html;

	AlarmNotifyFunc func;
	gpointer func_data;
} AlarmNotify;



/* Callback used when the notify dialog is destroyed */
static void
dialog_destroy_cb (GtkObject *object, gpointer data)
{
	AlarmNotify *an;

	an = data;
	g_object_unref (an->xml);
	g_free (an);
}

/* Delete_event handler for the alarm notify dialog */
static gint
delete_event_cb (GtkWidget *widget, GdkEvent *event, gpointer data)
{
	AlarmNotify *an;

	an = data;
	g_assert (an->func != NULL);

	(* an->func) (ALARM_NOTIFY_CLOSE, -1, an->func_data);

	gtk_widget_destroy (widget);
	return TRUE;
}

/* Callback for the close button */
static void
close_clicked_cb (GtkWidget *widget, gpointer data)
{
	AlarmNotify *an;

	an = data;
	g_assert (an->func != NULL);

	(* an->func) (ALARM_NOTIFY_CLOSE, -1, an->func_data);

	gtk_widget_destroy (an->dialog);
}

/* Callback for the snooze button */
static void
snooze_clicked_cb (GtkWidget *widget, gpointer data)
{
	AlarmNotify *an;
	int snooze_time;

	an = data;
	g_assert (an->func != NULL);

	snooze_time = gtk_spin_button_get_value_as_int (GTK_SPIN_BUTTON (an->snooze_time));
	(* an->func) (ALARM_NOTIFY_SNOOZE, snooze_time, an->func_data);

	gtk_widget_destroy (an->dialog);
}

/* Callback for the edit button */
static void
edit_clicked_cb (GtkWidget *widget, gpointer data)
{
	AlarmNotify *an;

	an = data;
	g_assert (an->func != NULL);

	(* an->func) (ALARM_NOTIFY_EDIT, -1, an->func_data);

	gtk_widget_destroy (an->dialog);
}

static void
url_requested_cb (GtkHTML *html, const char *url, GtkHTMLStream *stream, gpointer data)
{

	if (!strncmp ("file:///", url, strlen ("file:///"))) {
		FILE *fp;
		const char *filename = url + strlen ("file://");
		char buf[4096];
		size_t len;

		fp = fopen (filename, "r");

		if (fp == NULL) {
			g_warning ("Error opening image: %s\n", url);
			gtk_html_stream_close (stream, GTK_HTML_STREAM_ERROR);
			return;
		}

		while ((len = fread (buf, 1, sizeof(buf), fp)) > 0)
			gtk_html_stream_write (stream, buf, len);

		if (feof (fp)) {
			fclose (fp);
			gtk_html_stream_close (stream, GTK_HTML_STREAM_OK);
			return;
		}

		fclose (fp);
	}

	g_warning ("Error loading image");
	gtk_html_stream_close (stream, GTK_HTML_STREAM_ERROR);
	return;
}

GtkWidget *
make_html_display (gchar *widget_name, char *s1, char *s2, int scroll, int shadow)
{
	GtkWidget *html, *scrolled_window;

	gtk_widget_push_colormap (gdk_rgb_get_colormap ());

	html = gtk_html_new();

	gtk_html_set_default_content_type (GTK_HTML (html),
					   "charset=utf-8");
	gtk_html_load_empty (GTK_HTML (html));

	g_signal_connect (html, "url_requested",
			  G_CALLBACK (url_requested_cb),
			  NULL);

	gtk_widget_pop_colormap();

	scrolled_window = gtk_scrolled_window_new(NULL, NULL);

	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolled_window),
				       GTK_POLICY_AUTOMATIC,
				       GTK_POLICY_AUTOMATIC);


	gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (scrolled_window),
					     GTK_SHADOW_IN);

	gtk_widget_set_size_request (scrolled_window, 300, 200);

	gtk_container_add(GTK_CONTAINER (scrolled_window), html);

	gtk_widget_show_all(scrolled_window);

	g_object_set_data (G_OBJECT (scrolled_window), "html", html);
	return scrolled_window;
}

static void
write_times (GtkHTMLStream *stream, char *start, char *end)
{
	if (start)
		gtk_html_stream_printf (stream, "<b>%s</b> %s<br>", _("Starting:"), start);
	if (end)
		gtk_html_stream_printf (stream, "<b>%s</b> %s<br>", _("Ending:"), end);

}

/* Creates a heading for the alarm notification dialog */
static void
write_html_heading (GtkHTMLStream *stream, const char *message,
		    ECalComponentVType vtype, time_t occur_start, time_t occur_end)
{
	char *buf;
	char *start, *end;
	char *bg_path = "file://" EVOLUTION_IMAGESDIR "/bcg.png";
	gchar *image_path;
	gchar *icon_path;
	icaltimezone *current_zone;

	icon_path = e_icon_factory_get_icon_filename ("stock_alarm", 48);
	image_path = g_strdup_printf ("file://%s", icon_path);
	g_free (icon_path);

	/* Stringize the times */

	current_zone = config_data_get_timezone ();

	buf = timet_to_str_with_zone (occur_start, current_zone);
	start = g_locale_to_utf8 (buf, -1, NULL, NULL, NULL);
	g_free (buf);

	buf = timet_to_str_with_zone (occur_end, current_zone);
	end = g_locale_to_utf8 (buf, -1, NULL, NULL, NULL);
	g_free (buf);

	/* Write the header */

	gtk_html_stream_printf (stream,
				"<HTML><BODY background=\"%s\">"
				"<TABLE WIDTH=\"100%%\">"
				"<TR>"
				"<TD><IMG SRC=\"%s\" ALIGN=\"top\" BORDER=\"0\"></TD>"
				"<TD><H1>%s</H1></TD>"
				"</TR>"
				"</TABLE>",
				bg_path,
				image_path,
				_("Evolution Alarm"));

	gtk_html_stream_printf (stream, "<br><br><font size=\"+2\">%s</font><br><br>", message);

	/* Write the times */

	switch (vtype) {
	case E_CAL_COMPONENT_EVENT:
		write_times (stream, start, end);
		break;

	case E_CAL_COMPONENT_TODO:
		write_times (stream, start, end);
		break;

	default:
		/* Only VEVENTs and VTODOs can have alarms */
		g_assert_not_reached ();
		break;
	}

	g_free (start);
	g_free (end);
	g_free (image_path);
}

/**
 * alarm_notify_dialog:
 * @trigger: Trigger time for the alarm.
 * @occur_start: Start of occurrence time for the event.
 * @occur_end: End of occurrence time for the event.
 * @vtype: Type of the component which corresponds to the alarm.
 * @message; Message to display in the dialog; usually comes from the component.
 * @func: Function to be called when a dialog action is invoked.
 * @func_data: Closure data for @func.
 *
 * Runs the alarm notification dialog.  The specified @func will be used to
 * notify the client about result of the actions in the dialog.
 *
 * Return value: a pointer to the dialog structure if successful or NULL if an error occurs.
 **/
gpointer
alarm_notify_dialog (time_t trigger, time_t occur_start, time_t occur_end,
		     ECalComponentVType vtype, const char *message,
		     AlarmNotifyFunc func, gpointer func_data)
{
	AlarmNotify *an;
	GtkHTMLStream *stream;
	icaltimezone *current_zone;
	char *buf, *title;
	GList *icon_list;

	g_return_val_if_fail (trigger != -1, NULL);

	/* Only VEVENTs or VTODOs can have alarms */
	g_return_val_if_fail (vtype == E_CAL_COMPONENT_EVENT || vtype == E_CAL_COMPONENT_TODO, NULL);
	g_return_val_if_fail (message != NULL, NULL);
	g_return_val_if_fail (func != NULL, NULL);

	an = g_new0 (AlarmNotify, 1);

	an->func = func;
	an->func_data = func_data;

	an->xml = glade_xml_new (EVOLUTION_GLADEDIR "/alarm-notify.glade", NULL, NULL);
	if (!an->xml) {
		g_message ("alarm_notify_dialog(): Could not load the Glade XML file!");
		g_free (an);
		return NULL;
	}

	an->dialog = glade_xml_get_widget (an->xml, "alarm-notify");
	an->close = glade_xml_get_widget (an->xml, "close");
	an->snooze = glade_xml_get_widget (an->xml, "snooze");
	an->edit = glade_xml_get_widget (an->xml, "edit");
	an->snooze_time = glade_xml_get_widget (an->xml, "snooze-time");
	an->html = g_object_get_data (G_OBJECT (glade_xml_get_widget (an->xml, "frame")), "html");

	if (!(an->dialog && an->close && an->snooze && an->edit
	      && an->snooze_time)) {
		g_message ("alarm_notify_dialog(): Could not find all widgets in Glade file!");
		g_object_unref (an->xml);
		g_free (an);
		return NULL;
	}

	g_signal_connect (G_OBJECT (an->dialog), "destroy",
			  G_CALLBACK (dialog_destroy_cb),
			  an);

	/* Title */

	current_zone = config_data_get_timezone ();

	buf = timet_to_str_with_zone (trigger, current_zone);
	title = g_strdup_printf (_("Alarm on %s"), buf);
	g_free (buf);

	gtk_window_set_title (GTK_WINDOW (an->dialog), title);
	g_free (title);

	/* html heading */
	stream = gtk_html_begin (GTK_HTML (an->html));
	write_html_heading (stream, message, vtype, occur_start, occur_end);
	gtk_html_stream_close (stream, GTK_HTML_STREAM_OK);

	/* Connect actions */

	g_signal_connect (an->dialog, "delete_event",
			  G_CALLBACK (delete_event_cb),
			  an);

	g_signal_connect (an->close, "clicked",
			  G_CALLBACK (close_clicked_cb),
			  an);

	g_signal_connect (an->snooze, "clicked",
			  G_CALLBACK (snooze_clicked_cb),
			  an);

	g_signal_connect (an->edit, "clicked",
			  G_CALLBACK (edit_clicked_cb),
			  an);

	/* Run! */

	if (!GTK_WIDGET_REALIZED (an->dialog))
		gtk_widget_realize (an->dialog);

	icon_list = e_icon_factory_get_icon_list ("stock_alarm");
	if (icon_list) {
		gtk_window_set_icon_list (GTK_WINDOW (an->dialog), icon_list);
		g_list_foreach (icon_list, (GFunc) g_object_unref, NULL);
		g_list_free (icon_list);
	}

	gtk_widget_show (an->dialog);
	return an;
}

void
alarm_notify_dialog_disable_buttons (gpointer dialog)
{
	AlarmNotify *an = dialog;

	gtk_widget_set_sensitive (an->snooze, FALSE);
	gtk_widget_set_sensitive (an->edit, FALSE);
}
