/* Evolution calendar - alarm notification dialog
 *
 * Copyright (C) 2000 Ximian, Inc.
 * Copyright (C) 2001 Ximian, Inc.
 *
 * Author: Federico Mena-Quintero <federico@ximian.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
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
#include <gtk/gtkwindow.h>
#include <libgnome/gnome-defs.h>
#include <libgnome/gnome-i18n.h>
#include <libgnomeui/gnome-winhints.h>
#include <libgnomeui/gnome-window-icon.h>
#include <glade/glade.h>
#include <e-util/e-time-utils.h>
#include <e-util/e-unicode-i18n.h>
#include <gal/widgets/e-unicode.h>
#include <gal/widgets/e-scroll-frame.h>
#include <gtkhtml/gtkhtml.h>
#include <gtkhtml/gtkhtml-stream.h>
#include "alarm-notify-dialog.h"


GtkWidget *make_html_display (gchar *widget_name, char *s1, char *s2, int scroll, int shadow);

/* The useful contents of the alarm notify dialog */
typedef struct {
	GladeXML *xml;

	GtkWidget *dialog;
	GtkWidget *close;
	GtkWidget *snooze;
	GtkWidget *edit;
	GtkWidget *heading;
	GtkWidget *message;
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
	gtk_object_unref (GTK_OBJECT (an->xml));
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
	GtkWidget *html, *frame;

	gtk_widget_push_visual(gdk_rgb_get_visual());
	gtk_widget_push_colormap(gdk_rgb_get_cmap());
	
	html = gtk_html_new();

	gtk_html_set_default_content_type (GTK_HTML (html),
					   "charset=utf-8");
	gtk_html_load_empty (GTK_HTML (html));

	gtk_signal_connect (GTK_OBJECT (html), "url_requested",
			    GTK_SIGNAL_FUNC (url_requested_cb),
			    NULL);

	gtk_widget_pop_colormap();  
	gtk_widget_pop_visual();

	frame = e_scroll_frame_new(NULL, NULL);
    
	e_scroll_frame_set_policy(E_SCROLL_FRAME(frame),
				  GTK_POLICY_AUTOMATIC,
				  GTK_POLICY_AUTOMATIC);
	
	
	e_scroll_frame_set_shadow_type (E_SCROLL_FRAME (frame),
					GTK_SHADOW_IN);

	gtk_widget_set_usize (frame, 300, 200);

	gtk_container_add(GTK_CONTAINER (frame), html);
	
	gtk_widget_show_all(frame);
	
	gtk_object_set_user_data(GTK_OBJECT (frame), html);
	return frame;        
}

static void
write_times (GtkHTMLStream *stream, char *start, char *end)
{
	if (start)
		gtk_html_stream_printf (stream, "<b>%s</b> %s<br>", U_("Starting:"), start);
	if (end)
		gtk_html_stream_printf (stream, "<b>%s</b> %s<br>", U_("Ending:"), end);

}

/* Creates a heading for the alarm notification dialog */
static void
write_html_heading (GtkHTMLStream *stream, const char *message, CalComponentVType vtype, time_t occur_start, time_t occur_end)
{
	char *buf;
	char s[128], e[128];
	char *start = NULL, *end = NULL;
	char *bg_path = "file://" EVOLUTION_ICONSDIR "/bcg.png";
	char *image_path = "file://" EVOLUTION_ICONSDIR "/alarm.png";

	if (occur_start != -1) {
		struct tm tm;

		tm = *localtime (&occur_start);
		strftime (s, sizeof (s), "%A %b %d %Y %H:%M", &tm);
		start = e_utf8_from_locale_string (s);
	}

	if (occur_end != -1) {
		struct tm tm;

		tm = *localtime (&occur_end);
		strftime (e, sizeof (e), "%A %b %d %Y %H:%M", &tm);
		end = e_utf8_from_locale_string (e);
	}

	/* I love combinatorial explosion */
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
				U_("Evolution Alarm"));

	gtk_html_stream_printf (stream, "<br><br><font size=\"+2\">%s</font><br><br>", message);

	switch (vtype) {
	case CAL_COMPONENT_EVENT:
		/* gtk_html_stream_printf (stream, "%s<br>", U_("Notification about your appointment")); */
		write_times (stream, start, end);
		break;
	case CAL_COMPONENT_TODO:
		/* gtk_html_stream_printf (stream, "%s<br>", U_("Notification about your task")); */
		write_times (stream, start, end);
		break;
	default:
		/* Only VEVENTs and VTODOs can have alarms */
		g_assert_not_reached ();
		buf = NULL;
		break;
	}

	g_free (start);
	g_free (end);
	return;
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
 * Return value: TRUE on success, FALSE if the dialog could not be created.
 **/
gboolean
alarm_notify_dialog (time_t trigger, time_t occur_start, time_t occur_end,
		     CalComponentVType vtype, const char *message,
		     AlarmNotifyFunc func, gpointer func_data)
{
	AlarmNotify *an;
	char buf[256];
	struct tm tm_trigger;
	GtkHTMLStream *stream;

	g_return_val_if_fail (trigger != -1, FALSE);

	/* Only VEVENTs or VTODOs can have alarms */
	g_return_val_if_fail (vtype == CAL_COMPONENT_EVENT || vtype == CAL_COMPONENT_TODO, FALSE);
	g_return_val_if_fail (message != NULL, FALSE);
	g_return_val_if_fail (func != NULL, FALSE);

	an = g_new0 (AlarmNotify, 1);

	an->func = func;
	an->func_data = func_data;

	an->xml = glade_xml_new (EVOLUTION_GLADEDIR "/alarm-notify.glade", NULL);
	if (!an->xml) {
		g_message ("alarm_notify_dialog(): Could not load the Glade XML file!");
		g_free (an);
		return FALSE;
	}

	an->dialog = glade_xml_get_widget (an->xml, "alarm-notify");
	an->close = glade_xml_get_widget (an->xml, "close");
	an->snooze = glade_xml_get_widget (an->xml, "snooze");
	an->edit = glade_xml_get_widget (an->xml, "edit");
	an->heading = glade_xml_get_widget (an->xml, "heading");
	an->message = glade_xml_get_widget (an->xml, "message");
	an->snooze_time = glade_xml_get_widget (an->xml, "snooze-time");
	an->html = gtk_object_get_user_data (GTK_OBJECT (glade_xml_get_widget (an->xml, "frame")));

	if (!(an->dialog && an->close && an->snooze && an->edit && an->heading && an->message
	      && an->snooze_time)) {
		g_message ("alarm_notify_dialog(): Could not find all widgets in Glade file!");
		gtk_object_unref (GTK_OBJECT (an->xml));
		g_free (an);
		return FALSE;
	}

	gtk_object_set_data (GTK_OBJECT (an->dialog), "alarm-notify", an);
	gtk_signal_connect (GTK_OBJECT (an->dialog), "destroy",
			    GTK_SIGNAL_FUNC (dialog_destroy_cb), an);

	/* Title */
	tm_trigger = *localtime (&trigger);
	strftime (buf, sizeof (buf), _("Alarm on %A %b %d %Y %H:%M"), &tm_trigger);
	gtk_window_set_title (GTK_WINDOW (an->dialog), buf);

	/* html heading */
	stream = gtk_html_begin (GTK_HTML (an->html));
	write_html_heading (stream, message, vtype, occur_start, occur_end);
	gtk_html_stream_close (stream, GTK_HTML_STREAM_OK);

	/* Connect actions */

	gtk_signal_connect (GTK_OBJECT (an->dialog), "delete_event",
			    GTK_SIGNAL_FUNC (delete_event_cb),
			    an);
			    
	gtk_signal_connect (GTK_OBJECT (an->close), "clicked",
			    GTK_SIGNAL_FUNC (close_clicked_cb),
			    an);

	gtk_signal_connect (GTK_OBJECT (an->snooze), "clicked",
			    GTK_SIGNAL_FUNC (snooze_clicked_cb),
			    an);

	gtk_signal_connect (GTK_OBJECT (an->edit), "clicked",
			    GTK_SIGNAL_FUNC (edit_clicked_cb),
			    an);

	/* Run! */

	if (!GTK_WIDGET_REALIZED (an->dialog))
		gtk_widget_realize (an->dialog);

	gnome_win_hints_set_state (an->dialog, WIN_STATE_STICKY);
	gnome_window_icon_set_from_file (GTK_WINDOW (an->dialog), EVOLUTION_ICONSDIR "/alarm.png");

	gtk_widget_show (an->dialog);
	return TRUE;
}

