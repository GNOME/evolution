/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* e-itip-control.c
 *
 * Copyright (C) 2001  Ximian, Inc.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
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
 * Author: JP Rosevear
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <glib.h>
#include <gtk/gtkmisc.h>
#include <libgnome/gnome-defs.h>
#include <libgnome/gnome-i18n.h>
#include <libgnome/gnome-util.h>
#include <libgnomeui/gnome-stock.h>
#include <libgnomeui/gnome-dialog.h>
#include <libgnomeui/gnome-dialog-util.h>
#include <gtkhtml/gtkhtml.h>
#include <ical.h>
#include <cal-util/cal-component.h>
#include <cal-client/cal-client.h>
#include <e-util/e-time-utils.h>
#include <e-util/e-dialog-widgets.h>
#include <e-util/e-unicode-i18n.h>
#include "calendar-config.h"
#include "itip-utils.h"
#include "e-itip-control.h"

struct _EItipControlPrivate {
	GtkWidget *html;
	
	GtkWidget *count;
	GtkWidget *next;
	GtkWidget *prev;

	CalClient *event_client;
	CalClient *task_client;

	char *vcalendar;
	CalComponent *comp;
	icalcomponent *main_comp;
	icalcomponent *ical_comp;
	icalcomponent *top_level;
	icalcompiter iter;
	icalproperty_method method;

	int current;
	int total;

	GList *addresses;
	gchar *from_address;
	gchar *my_address;
};

/* HTML Strings */
#define HTML_HEADER     "<html><head><title>iCalendar Information</title></head>"
#define HTML_BODY_START "<body bgcolor=\"#ffffff\" text=\"#000000\" link=\"#336699\">"
#define HTML_SEP        "<hr color=#336699 align=\"left\" width=450>"
#define HTML_BODY_END   "</body>"
#define HTML_FOOTER     "</html>"

#define PUBLISH_OPTIONS "<form><b>Choose an action:</b>&nbsp<select NAME=\"action\" SIZE=\"1\"> \
<option VALUE=\"U\">Update</option></select>&nbsp &nbsp \
<input TYPE=Submit name=\"ok\" value=\"OK\"></form>"

#define REQUEST_OPTIONS "<form><b>Choose an action:</b>&nbsp<select NAME=\"action\" SIZE=\"1\"> \
<option VALUE=\"A\">Accept</option> \
<option VALUE=\"T\">Tentatively accept</option> \
<option VALUE=\"D\">Decline</option></select>&nbsp \
<input TYPE=\"checkbox\" name=\"rsvp\" value=\"1\" checked>RSVP&nbsp&nbsp\
<input TYPE=\"submit\" name=\"ok\" value=\"OK\"><br> \
</form>"

#define REQUEST_FB_OPTIONS "<form><b>Choose an action:</b><select NAME=\"action\" SIZE=\"1\"> \
<option VALUE=\"F\">Send Free/Busy Information</option></select>&nbsp &nbsp \
<input TYPE=Submit name=\"ok\" value=\"OK\"></form>"

#define REPLY_OPTIONS "<form><b>Choose an action:</b><select NAME=\"action\" SIZE=\"1\"> \
<option VALUE=\"R\">Update respondent status</option></select>&nbsp &nbsp \
<input TYPE=Submit name=\"ok\" value=\"OK\"></form>"

#define REFRESH_OPTIONS "<form><b>Choose an action:</b><select NAME=\"action\" SIZE=\"1\"> \
<option VALUE=\"S\">Send Latest Information</option></select>&nbsp &nbsp \
<input TYPE=Submit name=\"ok\" value=\"OK\"></form>"

#define CANCEL_OPTIONS "<form><b>Choose an action:</b><select NAME=\"action\" SIZE=\"1\"> \
<option VALUE=\"C\">Cancel</option></select>&nbsp &nbsp \
<input TYPE=Submit name=\"ok\" value=\"OK\"></form>"


static void class_init	(EItipControlClass	 *klass);
static void init	(EItipControl		 *itip);
static void destroy	(GtkObject               *obj);

static void prev_clicked_cb (GtkWidget *widget, gpointer data);
static void next_clicked_cb (GtkWidget *widget, gpointer data);
static void url_requested_cb (GtkHTML *html, const gchar *url, GtkHTMLStream *handle, gpointer data);
static void ok_clicked_cb (GtkHTML *html, const gchar *method, const gchar *url, const gchar *encoding, gpointer data);

static GtkVBoxClass *parent_class = NULL;


GtkType
e_itip_control_get_type (void)
{
	static GtkType type = 0;

	if (type == 0) {
		static const GtkTypeInfo info =
		{
			"EItipControl",
			sizeof (EItipControl),
			sizeof (EItipControlClass),
			(GtkClassInitFunc) class_init,
			(GtkObjectInitFunc) init,
			/* reserved_1 */ NULL,
			/* reserved_2 */ NULL,
			(GtkClassInitFunc) NULL,
		};

		type = gtk_type_unique (gtk_vbox_get_type (), &info);
	}

	return type;
}

static void
class_init (EItipControlClass *klass)
{
	GtkObjectClass *object_class;

	object_class = GTK_OBJECT_CLASS (klass);

	parent_class = gtk_type_class (gtk_vbox_get_type ());

	object_class->destroy = destroy;
}


/* Calendar Server routines */
static void
start_calendar_server_cb (CalClient *cal_client,
			  CalClientOpenStatus status,
			  gpointer data)
{
	gboolean *success = data;

	if (status == CAL_CLIENT_OPEN_SUCCESS)
		*success = TRUE;
	else
		*success = FALSE;

	gtk_main_quit (); /* end the sub event loop */
}

static CalClient *
start_calendar_server (gchar *uri)
{
	CalClient *client;
	gchar *filename;
	gboolean success;
	
	client = cal_client_new ();

	/* FIX ME */
	filename = g_concat_dir_and_file (g_get_home_dir (), uri);

	gtk_signal_connect (GTK_OBJECT (client), "cal_opened",
			    start_calendar_server_cb, &success);

	if (!cal_client_open_calendar (client, filename, FALSE))
		return NULL;

	/* run a sub event loop to turn cal-client's async load
	   notification into a synchronous call */
	gtk_main ();

	if (success)
		return client;

	return NULL;
}

static void
init (EItipControl *itip)
{
	EItipControlPrivate *priv;
	GtkWidget *hbox, *scrolled_window;
	
	priv = g_new0 (EItipControlPrivate, 1);

	itip->priv = priv;

	/* Addresses */
	priv->addresses = itip_addresses_get ();

	/* Header */
	priv->prev = gnome_stock_button (GNOME_STOCK_BUTTON_PREV);
	gtk_widget_show (priv->prev);
	priv->next = gnome_stock_button (GNOME_STOCK_BUTTON_NEXT);
	gtk_widget_show (priv->next);

	hbox = gtk_hbox_new (FALSE, 0);
	gtk_box_pack_start (GTK_BOX (hbox), priv->prev, FALSE, FALSE, 4);
	gtk_box_pack_start (GTK_BOX (hbox), priv->count, TRUE, TRUE, 4);
	gtk_box_pack_start (GTK_BOX (hbox), priv->next, FALSE, FALSE, 4);
	gtk_widget_show (hbox);

	gtk_signal_connect (GTK_OBJECT (priv->prev), "clicked",
			    GTK_SIGNAL_FUNC (prev_clicked_cb), itip);
	gtk_signal_connect (GTK_OBJECT (priv->next), "clicked",
			    GTK_SIGNAL_FUNC (next_clicked_cb), itip);

	/* Get the cal clients */
	priv->event_client = start_calendar_server ("evolution/local/Calendar/calendar.ics");
	if (priv->event_client == NULL)
		g_warning ("Unable to start calendar client");
	priv->task_client = start_calendar_server ("evolution/local/Tasks/tasks.ics");
	if (priv->task_client == NULL)
		g_warning ("Unable to start calendar client");

	/* Html Widget */
	priv->html = gtk_html_new ();
	gtk_widget_show (priv->html);

	scrolled_window = gtk_scrolled_window_new (NULL, NULL);
	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scrolled_window),
					GTK_POLICY_AUTOMATIC,
					GTK_POLICY_AUTOMATIC);
	gtk_widget_show (scrolled_window);
	
	gtk_container_add (GTK_CONTAINER (scrolled_window), priv->html);
	gtk_widget_set_usize (scrolled_window, 600, 400);
	gtk_box_pack_start (GTK_BOX (itip), scrolled_window, FALSE, FALSE, 4);

	gtk_signal_connect (GTK_OBJECT (priv->html), "url_requested",
			    url_requested_cb, itip);
	gtk_signal_connect (GTK_OBJECT (priv->html), "submit",
			    ok_clicked_cb, itip);

}

static void
clean_up (EItipControl *itip) 
{
	EItipControlPrivate *priv;

	priv = itip->priv;

	g_free (priv->vcalendar);
	priv->vcalendar = NULL;

	if (priv->comp)
		gtk_object_unref (GTK_OBJECT (priv->comp));
	priv->comp = NULL;
	
	icalcomponent_free (priv->top_level);
	priv->top_level = NULL;
	icalcomponent_free (priv->main_comp);
	priv->main_comp = NULL;
	priv->ical_comp = NULL;

	priv->current = 0;
	priv->total = 0;

	priv->my_address = NULL;
	g_free (priv->from_address);
	priv->from_address = NULL;
}

static void
destroy (GtkObject *obj)
{
	EItipControl *itip = E_ITIP_CONTROL (obj);
	EItipControlPrivate *priv;

	priv = itip->priv;

	clean_up (itip);

	itip_addresses_free (priv->addresses);
	priv->addresses = NULL;
	
	gtk_object_unref (GTK_OBJECT (priv->event_client));
	gtk_object_unref (GTK_OBJECT (priv->task_client));
	
	g_free (priv);

	if (GTK_OBJECT_CLASS (parent_class)->destroy)
		(* GTK_OBJECT_CLASS (parent_class)->destroy) (obj);
}

GtkWidget *
e_itip_control_new (void)
{
	return gtk_type_new (E_TYPE_ITIP_CONTROL);
}

static void
find_my_address (EItipControl *itip, icalcomponent *ical_comp)
{
	EItipControlPrivate *priv;
	icalproperty *prop;
	const char *attendee, *text;
	icalvalue *value;
	
	priv = itip->priv;

	for (prop = icalcomponent_get_first_property (ical_comp, ICAL_ATTENDEE_PROPERTY);
	     prop != NULL;
	     prop = icalcomponent_get_next_property (ical_comp, ICAL_ATTENDEE_PROPERTY))
	{
		GList *l;
		
		value = icalproperty_get_value (prop);
		if (!value)
			continue;

		attendee = icalvalue_get_string (value);
		
		text = itip_strip_mailto (attendee);
		for (l = priv->addresses; l != NULL; l = l->next) {
			ItipAddress *a = l->data;
			
			if (!strcmp (a->address, text)) {
				priv->my_address = a->address;
				return;
			}
		}
	}
}

static icalproperty *
find_attendee (icalcomponent *ical_comp, const char *address)
{
	icalproperty *prop;
	const char *attendee, *text;
	icalvalue *value;
	
	g_return_val_if_fail (address != NULL, NULL);

	for (prop = icalcomponent_get_first_property (ical_comp, ICAL_ATTENDEE_PROPERTY);
	     prop != NULL;
	     prop = icalcomponent_get_next_property (ical_comp, ICAL_ATTENDEE_PROPERTY))
	{
		value = icalproperty_get_value (prop);
		if (!value)
			continue;

		attendee = icalvalue_get_string (value);

		text = itip_strip_mailto (attendee);
		if (strstr (text, address))
			break;
	}
			
	return prop;
}

static icalparameter_partstat
find_attendee_partstat (icalcomponent *ical_comp, const char *address) 
{
	icalproperty *prop;

	prop = find_attendee (ical_comp, address);
	if (prop != NULL) {
		icalparameter *param;
		
		param = icalproperty_get_first_parameter (prop, ICAL_PARTSTAT_PARAMETER);
		if (param != NULL)
			return icalparameter_get_partstat (param);
	}
	
	return ICAL_PARTSTAT_NONE;	
}

static void
set_label (EItipControl *itip) 
{
	EItipControlPrivate *priv;
	gchar *text;
	
	priv = itip->priv;
	
	text = g_strdup_printf ("%d of %d", priv->current, priv->total);
	gtk_label_set_text (GTK_LABEL (priv->count), text);

}

static void
set_button_status (EItipControl *itip) 
{
	EItipControlPrivate *priv;

	priv = itip->priv;

	if (priv->current == priv->total)
		gtk_widget_set_sensitive (priv->next, FALSE);
	else
		gtk_widget_set_sensitive (priv->next, TRUE);

	if (priv->current == 1)
		gtk_widget_set_sensitive (priv->prev, FALSE);
	else
		gtk_widget_set_sensitive (priv->prev, TRUE);
}

static void
write_label_piece (time_t t, char *buffer, int size, char *stext, char *etext)
{
	struct tm *tmp_tm;
	int len;
	
	/* FIXME: Convert to an appropriate timezone. */
	tmp_tm = localtime (&t);
	if (stext != NULL)
		strcat (buffer, stext);

	len = strlen (buffer);
	e_time_format_date_and_time (tmp_tm,
				     calendar_config_get_24_hour_format (), 
				     FALSE, FALSE,
				     &buffer[len], size - len);
	if (etext != NULL)
		strcat (buffer, etext);
}

static void
set_date_label (GtkHTML *html, GtkHTMLStream *html_stream, CalComponent *comp)
{
	CalComponentDateTime datetime;
	time_t start = 0, end = 0, complete = 0, due = 0;
	static char buffer[1024];
	gboolean wrote = FALSE;
	CalComponentVType type;
	
	type = cal_component_get_vtype (comp);

	/* FIXME: timezones. */
	buffer[0] = '\0';
	cal_component_get_dtstart (comp, &datetime);	
	if (datetime.value) {
		start = icaltime_as_timet (*datetime.value);
		switch (type) {
		case CAL_COMPONENT_EVENT:
			write_label_piece (start, buffer, 1024,
					   U_("Meeting begins: <b>"),
					   "</b><br>");
			break;			
		case CAL_COMPONENT_TODO:
			write_label_piece (start, buffer, 1024,
					   U_("Task begins: <b>"),
					   "</b><br>");
			break;			
		case CAL_COMPONENT_FREEBUSY:
			write_label_piece (start, buffer, 1024,
					   U_("Free/Busy info begins: <b>"), 
					   "</b><br>");
			break;			
		default:
			write_label_piece (start, buffer, 1024, U_("Begins: <b>"), "</b><br>");	
		}
		gtk_html_write (html, html_stream, buffer, strlen(buffer));
		wrote = TRUE;
	}
	cal_component_free_datetime (&datetime);	

	buffer[0] = '\0';
	cal_component_get_dtend (comp, &datetime);
	if (datetime.value){
		end = icaltime_as_timet (*datetime.value);
		switch (type) {
		case CAL_COMPONENT_EVENT:
			write_label_piece (end, buffer, 1024, "Meeting ends: <b>", "</b><br>");
			break;			
		case CAL_COMPONENT_FREEBUSY:
			write_label_piece (start, buffer, 1024, "Free/Busy info ends: <b>", 
					   "</b><br>");
			break;			
		default:
			write_label_piece (start, buffer, 1024, "Ends: <b>", "</b><br>");	
		}
		gtk_html_write (html, html_stream, buffer, strlen (buffer));
		wrote = TRUE;
	}
	cal_component_free_datetime (&datetime);	

	buffer[0] = '\0';
	datetime.tzid = NULL;
	cal_component_get_completed (comp, &datetime.value);
	if (type == CAL_COMPONENT_TODO && datetime.value) {
		complete = icaltime_as_timet (*datetime.value);
		write_label_piece (complete, buffer, 1024, "Task Completed: <b>", "</b><br>");
		gtk_html_write (html, html_stream, buffer, strlen (buffer));
		wrote = TRUE;
	}
	cal_component_free_datetime (&datetime);

	buffer[0] = '\0';
	cal_component_get_due (comp, &datetime);
	if (type == CAL_COMPONENT_TODO && complete == 0 && datetime.value) {
		due = icaltime_as_timet (*datetime.value);
		write_label_piece (due, buffer, 1024, "Task Due: <b>", "</b><br>");
		gtk_html_write (html, html_stream, buffer, strlen (buffer));
		wrote = TRUE;
	}
	cal_component_free_datetime (&datetime);

	if (wrote)
		gtk_html_write (html, html_stream, "<br>", 8);
}

static void
set_message (GtkHTML *html, GtkHTMLStream *html_stream, gchar *message, gboolean err) 
{
	char *buffer;
	
	if (message == NULL)
		return;

	if (err) {
		buffer = g_strdup_printf ("<b><font color=\"#ff0000\">%s</font></b><br><br>", message);
	} else {
		buffer = g_strdup_printf ("<b>%s</b><br><br>", message);
	}
	gtk_html_write (GTK_HTML (html), html_stream, buffer, strlen (buffer));
	g_free (buffer);		
}

static void
write_error_html (EItipControl *itip, gchar *itip_err)
{
	EItipControlPrivate *priv;
	GtkHTMLStream *html_stream;
	gchar *html;
	
	priv = itip->priv;

	/* Html widget */
	html_stream = gtk_html_begin (GTK_HTML (priv->html));
	gtk_html_write (GTK_HTML (priv->html), html_stream, 
			HTML_HEADER, strlen(HTML_HEADER));
	gtk_html_write (GTK_HTML (priv->html), html_stream,
			HTML_BODY_START, strlen(HTML_BODY_START));

	/* The table */
	html = g_strdup ("<table width=450 cellspacing=\"0\" cellpadding=\"4\" border=\"0\">");
	gtk_html_write (GTK_HTML (priv->html), html_stream, html, strlen(html));
	g_free (html);

	/* The column for the image */
	html = g_strdup ("<tr><td width=48 align=\"center\" valign=\"top\" rowspan=\"8\">");
	gtk_html_write (GTK_HTML (priv->html), html_stream, html, strlen(html));
	g_free (html);

	/* The image */
	html = g_strdup ("<img src=\"/meeting-request.png\"></td>");
	gtk_html_write (GTK_HTML (priv->html), html_stream, html, strlen(html));
	g_free (html);

	html = g_strdup ("<td align=\"left\" valign=\"top\">");
	gtk_html_write (GTK_HTML (priv->html), html_stream, html, strlen(html));
	g_free (html);

	/* Title */
	set_message (GTK_HTML (priv->html), html_stream, "iCalendar Error", TRUE);

	/* Error */
	gtk_html_write (GTK_HTML (priv->html), html_stream, itip_err, strlen(itip_err));

	/* Clean up */
	html = g_strdup ("</td></tr></table>");
	gtk_html_write (GTK_HTML (priv->html), html_stream, html, strlen(html));
	g_free (html);

	gtk_html_write (GTK_HTML (priv->html), html_stream,
			HTML_BODY_END, strlen(HTML_BODY_END));
	gtk_html_write (GTK_HTML (priv->html), html_stream,
			HTML_FOOTER, strlen(HTML_FOOTER));

	gtk_html_end (GTK_HTML (priv->html), html_stream, GTK_HTML_STREAM_OK);
}

static void
write_html (EItipControl *itip, gchar *itip_desc, gchar *itip_title, gchar *options) 
{
	EItipControlPrivate *priv;
	GtkHTMLStream *html_stream;
	CalComponentText text;
	CalComponentOrganizer organizer;
	CalComponentAttendee *attendee;
	GSList *attendees, *l = NULL;
	gchar *html;
	
	priv = itip->priv;

	/* Html widget */
	html_stream = gtk_html_begin (GTK_HTML (priv->html));
	gtk_html_write (GTK_HTML (priv->html), html_stream, 
			HTML_HEADER, strlen(HTML_HEADER));
	gtk_html_write (GTK_HTML (priv->html), html_stream,
			HTML_BODY_START, strlen(HTML_BODY_START));

	/* The table */
	html = g_strdup ("<table width=450 cellspacing=\"0\" cellpadding=\"4\" border=\"0\">");
	gtk_html_write (GTK_HTML (priv->html), html_stream, html, strlen(html));
	g_free (html);

	/* The column for the image */
	html = g_strdup ("<tr><td width=48 align=\"center\" valign=\"top\" rowspan=\"8\">");
	gtk_html_write (GTK_HTML (priv->html), html_stream, html, strlen(html));
	g_free (html);

	/* The image */
	html = g_strdup ("<img src=\"/meeting-request.png\"></td>");
	gtk_html_write (GTK_HTML (priv->html), html_stream, html, strlen(html));
	g_free (html);

	html = g_strdup ("<td align=\"left\" valign=\"top\">");
	gtk_html_write (GTK_HTML (priv->html), html_stream, html, strlen(html));
	g_free (html);

	switch (priv->method) {
	case ICAL_METHOD_REFRESH:
	case ICAL_METHOD_REPLY:
		/* An attendee sent this */
		cal_component_get_attendee_list (priv->comp, &attendees);
		if (attendees != NULL) {			
			attendee = attendees->data;		
			html = g_strdup_printf (itip_desc, 
						attendee->cn ? 
						attendee->cn : 
						itip_strip_mailto (attendee->value));
		} else {
			html = g_strdup_printf (itip_desc, "An unknown person");
		}
		break;
	case ICAL_METHOD_PUBLISH:
	case ICAL_METHOD_REQUEST:
	case ICAL_METHOD_ADD:
	case ICAL_METHOD_CANCEL:
	default:
		/* The organizer sent this */	
		cal_component_get_organizer (priv->comp, &organizer);
		if (organizer.value != NULL)
			html = g_strdup_printf (itip_desc,
						organizer.cn ? 
						organizer.cn : 
						itip_strip_mailto (organizer.value));
		else
			html = g_strdup_printf (itip_desc, "An unknown person");
		break;
	}
	gtk_html_write (GTK_HTML (priv->html), html_stream, html, strlen(html));
	g_free (html);
	
	/* Describe what the user can do */
	html = U_("<br> Please review the following information, "
	          "and then select an action from the menu below.");
	gtk_html_write (GTK_HTML (priv->html), html_stream, html, strlen(html));
	
	/* Separator */
	gtk_html_write (GTK_HTML (priv->html), html_stream, HTML_SEP, strlen (HTML_SEP));

	/* Title */
	set_message (GTK_HTML (priv->html), html_stream, itip_title, FALSE);

	/* Date information */
	set_date_label (GTK_HTML (priv->html), html_stream, priv->comp);

	/* Summary */
	cal_component_get_summary (priv->comp, &text);
	html = g_strdup_printf (U_("<b>Summary:</b> %s<br><br>"), text.value ? text.value : U_("<i>None</i>"));
	gtk_html_write (GTK_HTML (priv->html), html_stream, html, strlen(html));
	g_free (html);

	/* Description */
	cal_component_get_description_list (priv->comp, &l);
	if (l)
		text = *((CalComponentText *)l->data);

	if (l && text.value) {
		html = g_strdup_printf (U_("<b>Description:</b> %s"), text.value);
		gtk_html_write (GTK_HTML (priv->html), html_stream, html, strlen(html));
		g_free (html);
	}
	cal_component_free_text_list (l);

	/* Separator */
	gtk_html_write (GTK_HTML (priv->html), html_stream, HTML_SEP, strlen (HTML_SEP));

	/* Options */
	if (options != NULL)
		gtk_html_write (GTK_HTML (priv->html), html_stream, options, strlen (options));

	html = g_strdup ("</td></tr></table>");
	gtk_html_write (GTK_HTML (priv->html), html_stream, html, strlen(html));
	g_free (html);

	gtk_html_write (GTK_HTML (priv->html), html_stream,
			HTML_BODY_END, strlen(HTML_BODY_END));
	gtk_html_write (GTK_HTML (priv->html), html_stream,
			HTML_FOOTER, strlen(HTML_FOOTER));

	gtk_html_end (GTK_HTML (priv->html), html_stream, GTK_HTML_STREAM_OK);
}


static void
show_current_event (EItipControl *itip) 
{
	EItipControlPrivate *priv;
	gchar *itip_title, *itip_desc, *options;
	
	priv = itip->priv;

	switch (priv->method) {
	case ICAL_METHOD_PUBLISH:
		itip_desc = _("<b>%s</b> has published meeting information.");
		itip_title = _("Meeting Information");
		options = PUBLISH_OPTIONS;
		break;
	case ICAL_METHOD_REQUEST:
		itip_desc = _("<b>%s</b> requests your presence at a meeting.");
		itip_title = _("Meeting Proposal");
		options = REQUEST_OPTIONS;
		break;
	case ICAL_METHOD_ADD:
		itip_desc = _("<b>%s</b> wishes to add to an existing meeting.");
		itip_title = _("Meeting Update");
		options = PUBLISH_OPTIONS;
		break;
	case ICAL_METHOD_REFRESH:
		itip_desc = _("<b>%s</b> wishes to receive the latest meeting information.");
		itip_title = _("Meeting Update Request");
		options = PUBLISH_OPTIONS;
		break;
	case ICAL_METHOD_REPLY:
		itip_desc = _("<b>%s</b> has replied to a meeting request.");
		itip_title = _("Meeting Reply");
		options = REPLY_OPTIONS;
		break;
	case ICAL_METHOD_CANCEL:
		itip_desc = _("<b>%s</b> has cancelled a meeting.");
		itip_title = _("Meeting Cancellation");
		options = CANCEL_OPTIONS;
		break;
	default:
		itip_desc = _("<b>%s</b> has sent an unintelligible message.");
		itip_title = _("Bad Meeting Message");
		options = NULL;
	}

	write_html (itip, itip_desc, itip_title, options);
}

static void
show_current_todo (EItipControl *itip) 
{
	EItipControlPrivate *priv;
	gchar *itip_title, *itip_desc, *options;
	
	priv = itip->priv;

	switch (priv->method) {
	case ICAL_METHOD_PUBLISH:
		itip_desc = _("<b>%s</b> has published task information.");
		itip_title = _("Task Information");
		options = PUBLISH_OPTIONS;
		break;
	case ICAL_METHOD_REQUEST:
		itip_desc = _("<b>%s</b> requests you perform a task.");
		itip_title = _("Task Proposal");
		options = REQUEST_OPTIONS;
		break;
	case ICAL_METHOD_ADD:
		itip_desc = _("<b>%s</b> wishes to add to an existing task.");
		itip_title = _("Task Update");
		options = PUBLISH_OPTIONS;
		break;
	case ICAL_METHOD_REFRESH:
		itip_desc = _("<b>%s</b> wishes to receive the latest task information.");
		itip_title = _("Task Update Request");
		options = PUBLISH_OPTIONS;
		break;
	case ICAL_METHOD_REPLY:
		itip_desc = _("<b>%s</b> has replied to a task assignment.");
		itip_title = _("Task Reply");
		options = REPLY_OPTIONS;
		break;
	case ICAL_METHOD_CANCEL:
		itip_desc = _("<b>%s</b> has cancelled a task.");
		itip_title = _("Task Cancellation");
		options = CANCEL_OPTIONS;
		break;
	default:
		itip_desc = _("<b>%s</b> has sent an unintelligible message.");
		itip_title = _("Bad Task Message");
		options = NULL;
	}

	write_html (itip, itip_desc, itip_title, options);
}

static void
show_current_freebusy (EItipControl *itip) 
{
	EItipControlPrivate *priv;
	gchar *itip_title, *itip_desc, *options;
	
	priv = itip->priv;

	switch (priv->method) {
	case ICAL_METHOD_PUBLISH:
		itip_desc = _("<b>%s</b> has published free/busy information.");
		itip_title = _("Free/Busy Information");
		options = NULL;
		break;
	case ICAL_METHOD_REQUEST:
		itip_desc = _("<b>%s</b> requests your free/busy information.");
		itip_title = _("Free/Busy Request");
		options = REQUEST_FB_OPTIONS;
		break;
	case ICAL_METHOD_REPLY:
		itip_desc = _("<b>%s</b> has replied to a free/busy request.");
		itip_title = _("Free/Busy Reply");
		options = NULL;
		break;
	default:
		itip_desc = _("<b>%s</b> has sent an unintelligible message.");
		itip_title = _("Bad Free/Busy Message");
		options = NULL;
	}

	write_html (itip, itip_desc, itip_title, options);
}

static icalcomponent *
get_next (icalcompiter *iter) 
{
	icalcomponent *ret = NULL;
	icalcomponent_kind kind = ICAL_NO_COMPONENT;
	
	while (kind != ICAL_VEVENT_COMPONENT 
	       && kind != ICAL_VTODO_COMPONENT
	       && kind != ICAL_VFREEBUSY_COMPONENT) {
		icalcompiter_next (iter);	
		ret = icalcompiter_deref (iter);
		kind = icalcomponent_isa (ret);
	}

	return ret;
}

static icalcomponent *
get_prev (icalcompiter *iter)
{
	icalcomponent *ret = NULL;
	icalcomponent_kind kind = ICAL_NO_COMPONENT;
	
	while (kind != ICAL_VEVENT_COMPONENT 
	       && kind != ICAL_VTODO_COMPONENT
	       && kind != ICAL_VFREEBUSY_COMPONENT) {
		icalcompiter_prior (iter);	
		ret = icalcompiter_deref (iter);
		kind = icalcomponent_isa (ret);
	}

	return ret;
}

static void
show_current (EItipControl *itip) 
{
	EItipControlPrivate *priv;
	CalComponentVType type;
	icalcomponent *alarm_comp;
	icalcompiter alarm_iter;
	
	priv = itip->priv;

	set_label (itip);
	set_button_status (itip);

	if (priv->comp)
		gtk_object_unref (GTK_OBJECT (priv->comp));
	
	/* Strip out alarms for security purposes */
	alarm_iter = icalcomponent_begin_component (priv->ical_comp, ICAL_VALARM_COMPONENT);
	while ((alarm_comp = icalcompiter_deref (&alarm_iter)) != NULL) {
		icalcomponent_remove_component (priv->ical_comp, alarm_comp);

		icalcompiter_next (&alarm_iter);
	}

	priv->comp = cal_component_new ();
	if (!cal_component_set_icalcomponent (priv->comp, priv->ical_comp)) {
		write_error_html (itip, _("The message does not appear to be properly formed"));
		gtk_object_unref (GTK_OBJECT (priv->comp));
		priv->comp = NULL;
		return;
	};

	type = cal_component_get_vtype (priv->comp);

	switch (type) {
	case CAL_COMPONENT_EVENT:
		show_current_event (itip);
		break;
	case CAL_COMPONENT_TODO:
		show_current_todo (itip);
		break;
	case CAL_COMPONENT_FREEBUSY:
		show_current_freebusy (itip);
		break;
	default:
		write_error_html (itip, _("The message contains only unsupported requests."));
	}

	find_my_address (itip, priv->ical_comp);
}

void
e_itip_control_set_data (EItipControl *itip, const gchar *text) 
{
	EItipControlPrivate *priv;
	icalproperty *prop;
	icalcomponent_kind kind = ICAL_NO_COMPONENT;
	icalcomponent *tz_comp;
	icalcompiter tz_iter;
	
	priv = itip->priv;

	clean_up (itip);

	priv->comp = NULL;
	priv->total = 0;
	priv->current = 0;
	
	priv->vcalendar = g_strdup (text);
	priv->top_level = cal_util_new_top_level ();

	priv->main_comp = icalparser_parse_string (priv->vcalendar);
	if (priv->main_comp == NULL) {
		write_error_html (itip, _("The attachment does not contain a valid calendar message"));
		return;
	}

	prop = icalcomponent_get_first_property (priv->main_comp, ICAL_METHOD_PROPERTY);
	if (prop == NULL) {		
		write_error_html (itip, _("The attachment does not contain a valid calendar message"));
		return;
	}
	
	priv->method = icalproperty_get_method (prop);

	tz_iter = icalcomponent_begin_component (priv->main_comp, ICAL_VTIMEZONE_COMPONENT);
	while ((tz_comp = icalcompiter_deref (&tz_iter)) != NULL) {
		icalcomponent *clone;
		
		clone = icalcomponent_new_clone (tz_comp);
		icalcomponent_add_component (priv->top_level, clone);

		icalcompiter_next (&tz_iter);
	}

	priv->iter = icalcomponent_begin_component (priv->main_comp, ICAL_ANY_COMPONENT);
	priv->ical_comp = icalcompiter_deref (&priv->iter);
	kind = icalcomponent_isa (priv->ical_comp);
	if (kind != ICAL_VEVENT_COMPONENT 
	    && kind != ICAL_VTODO_COMPONENT
	    && kind != ICAL_VFREEBUSY_COMPONENT)
		priv->ical_comp = get_next (&priv->iter);
	
	priv->total = icalcomponent_count_components (priv->main_comp, ICAL_VEVENT_COMPONENT);
	priv->total += icalcomponent_count_components (priv->main_comp, ICAL_VTODO_COMPONENT);
	priv->total += icalcomponent_count_components (priv->main_comp, ICAL_VFREEBUSY_COMPONENT);
	
	if (priv->total > 0)
		priv->current = 1;
	else
		priv->current = 0;

	show_current (itip);
}

gchar *
e_itip_control_get_data (EItipControl *itip) 
{
	EItipControlPrivate *priv;

	priv = itip->priv;
	
	return g_strdup (priv->vcalendar);
}

gint
e_itip_control_get_data_size (EItipControl *itip) 
{
	EItipControlPrivate *priv;

	priv = itip->priv;
	
	if (priv->vcalendar == NULL)
		return 0;
	
	return strlen (priv->vcalendar);
}

void
e_itip_control_set_from_address (EItipControl *itip, const gchar *address)
{
	EItipControlPrivate *priv;

	priv = itip->priv;

	if (priv->from_address)
		g_free (priv->from_address);
	
	priv->from_address = g_strdup (address);
}

const gchar *
e_itip_control_get_from_address (EItipControl *itip)
{
	EItipControlPrivate *priv;

	priv = itip->priv;

	return priv->from_address;
}


static void
change_status (icalcomponent *ical_comp, const char *address, icalparameter_partstat status)
{
	icalproperty *prop;

	prop = find_attendee (ical_comp, address);
	if (prop) {
		icalparameter *param;

		icalproperty_remove_parameter (prop, ICAL_PARTSTAT_PARAMETER);
		param = icalparameter_new_partstat (status);
		icalproperty_add_parameter (prop, param);
	}
}

static void
update_item (EItipControl *itip) 
{
	EItipControlPrivate *priv;
	icalcomponent *clone;
	CalClient *client;
	CalComponentVType type;
	GtkWidget *dialog;
	
	priv = itip->priv;

	type = cal_component_get_vtype (priv->comp);
	if (type == CAL_COMPONENT_TODO)
		client = priv->task_client;
	else 
		client = priv->event_client;

	clone = icalcomponent_new_clone (priv->ical_comp);
	icalcomponent_add_component (priv->top_level, clone);
	
	if (!cal_client_update_objects (client, priv->top_level))
		dialog = gnome_warning_dialog (_("Calendar file could not be updated!\n"));
	else
		dialog = gnome_ok_dialog (_("Update complete\n"));
	gnome_dialog_run_and_close (GNOME_DIALOG (dialog));

	icalcomponent_remove_component (priv->top_level, clone);
}

static void
update_attendee_status (EItipControl *itip)
{
	EItipControlPrivate *priv;
	CalClient *client;
	CalClientGetStatus status;
	CalComponent *comp;	
	CalComponentVType type;
	const char *uid;
	GtkWidget *dialog;
	
	priv = itip->priv;

	type = cal_component_get_vtype (priv->comp);
	if (type == CAL_COMPONENT_TODO)
		client = priv->task_client;
	else 
		client = priv->event_client;

	/* Obtain our version */
	cal_component_get_uid (priv->comp, &uid);	
	status = cal_client_get_object (client, uid, &comp);

	if (status == CAL_CLIENT_GET_SUCCESS) {
		GSList *attendees;
		
		cal_component_get_attendee_list (priv->comp, &attendees);
		if (attendees != NULL) {
			CalComponentAttendee *a = attendees->data;
			icalparameter_partstat partstat;
			
			partstat = find_attendee_partstat (priv->ical_comp, itip_strip_mailto (a->value));
			
			if (partstat != ICAL_PARTSTAT_NONE) {
				change_status (cal_component_get_icalcomponent (comp),
					       itip_strip_mailto (a->value),
					       partstat);
			} else {				
				dialog = gnome_warning_dialog (_("Attendee status could "
								 "not be updated because "
								 "of an invalid status!\n"));
				goto cleanup;				
			}
		}
		
		if (!cal_client_update_object (client, comp))
			dialog = gnome_warning_dialog (_("Attendee status ould not be updated!\n"));
		else
			dialog = gnome_ok_dialog (_("Attendee status updated\n"));
	} else {
		dialog = gnome_warning_dialog (_("Attendee status can not be updated " 
						 "because the item no longer exists"));
	}

 cleanup:
	gtk_object_unref (GTK_OBJECT (comp));
	gnome_dialog_run_and_close (GNOME_DIALOG (dialog));
}

static void
remove_item (EItipControl *itip) 
{
	EItipControlPrivate *priv;
	CalClient *client;
	CalComponentVType type;
	const char *uid;
	GtkWidget *dialog;
	
	priv = itip->priv;

	type = cal_component_get_vtype (priv->comp);
	if (type == CAL_COMPONENT_TODO)
		client = priv->task_client;
	else 
		client = priv->event_client;

	cal_component_get_uid (priv->comp, &uid);
	if (!cal_client_remove_object (client, uid))
		dialog = gnome_warning_dialog (_("I couldn't remove the item from your calendar file!\n"));
	else
		dialog = gnome_ok_dialog (_("Removal Complete"));
	gnome_dialog_run_and_close (GNOME_DIALOG (dialog));
}

static void
send_item (EItipControl *itip)
{
	EItipControlPrivate *priv;
	CalComponent *comp;
	CalComponentVType type;
	const char *uid;
	CalClientGetStatus status;
	GtkWidget *dialog;

	priv = itip->priv;
	
	type = cal_component_get_vtype (priv->comp);
	cal_component_get_uid (priv->comp, &uid);

	switch (type) {
	case CAL_COMPONENT_EVENT:
		status = cal_client_get_object (priv->event_client, uid, &comp);
		break;
	case CAL_COMPONENT_TODO:
		status = cal_client_get_object (priv->task_client, uid, &comp);
		break;
	default:
		status = CAL_CLIENT_GET_NOT_FOUND;
	}

	if (status == CAL_CLIENT_GET_SUCCESS) {
		itip_send_comp (CAL_COMPONENT_METHOD_PUBLISH, comp);
		dialog = gnome_ok_dialog (_("Item sent!\n"));
	} else {
		dialog = gnome_warning_dialog (_("The item could not be sent!\n"));
	}
	gnome_dialog_run_and_close (GNOME_DIALOG (dialog));
}

static void
send_freebusy (EItipControl *itip)
{
	EItipControlPrivate *priv;
	CalComponent *comp;
	CalComponentDateTime datetime;
	time_t start, end;
	GtkWidget *dialog;
	GList *comp_list;

	priv = itip->priv;
	
	/* FIXME: timezones and free these. */
	cal_component_get_dtstart (priv->comp, &datetime);
	start = icaltime_as_timet (*datetime.value);
	cal_component_get_dtend (priv->comp, &datetime);
	end = icaltime_as_timet (*datetime.value);
	comp_list = cal_client_get_free_busy (priv->event_client, NULL, start, end);

	if (comp_list) {
		GList *l;

		for (l = comp_list; l; l = l->next) {
			CalComponent *comp = CAL_COMPONENT (l->data);
			itip_send_comp (CAL_COMPONENT_METHOD_REPLY, comp);

			gtk_object_unref (GTK_OBJECT (comp));
		}
		dialog = gnome_ok_dialog (_("Item sent!\n"));

		g_list_free (comp_list);
	} else {
		dialog = gnome_warning_dialog (_("The item could not be sent!\n"));
	}
	gnome_dialog_run_and_close (GNOME_DIALOG (dialog));
}

static void
prev_clicked_cb (GtkWidget *widget, gpointer data) 
{
	EItipControl *itip = E_ITIP_CONTROL (data);
	EItipControlPrivate *priv;

	priv = itip->priv;
	
	priv->current--;
	priv->ical_comp = get_prev (&priv->iter);

	show_current (itip);
}

static void
next_clicked_cb (GtkWidget *widget, gpointer data)
{
	EItipControl *itip = E_ITIP_CONTROL (data);
	EItipControlPrivate *priv;

	priv = itip->priv;
	
	priv->current++;
	priv->ical_comp = get_next (&priv->iter);

	show_current (itip);
}

static void
url_requested_cb (GtkHTML *html, const gchar *url, GtkHTMLStream *handle, gpointer data)
{	unsigned char buffer[4096];
	int len, fd;
        char *path;

	path = g_strdup_printf ("%s/%s", EVOLUTION_ICONSDIR, url);
	
	if ((fd = open (path, O_RDONLY)) == -1) {
		g_warning ("%s", g_strerror (errno));
		goto cleanup;
	}
      
       	while ((len = read (fd, buffer, 4096)) > 0) {
		gtk_html_write (html, handle, buffer, len);
	}

	if (len < 0) {
		/* check to see if we stopped because of an error */
		gtk_html_end (html, handle, GTK_HTML_STREAM_ERROR);
		g_warning ("%s", g_strerror (errno));
		goto cleanup;
	}	
	/* done with no errors */
	gtk_html_end (html, handle, GTK_HTML_STREAM_OK);
	close (fd);

 cleanup:
	g_free (path);
}

static void
ok_clicked_cb (GtkHTML *html, const gchar *method, const gchar *url, const gchar *encoding, gpointer data)
{
	EItipControl *itip = E_ITIP_CONTROL (data);
	EItipControlPrivate *priv;
	gchar **fields;
	gboolean rsvp = FALSE;
	int i;
	
	priv = itip->priv;
	
	fields = g_strsplit (encoding, "&", -1);
	for (i = 0; fields[i] != NULL; i++) {
		gchar **key_value;

		key_value = g_strsplit (fields[i], "=", 2);

		if (key_value[0] != NULL && !strcmp (key_value[0], "action")) {
			if (key_value[1] == NULL)
				break;
			
			switch (key_value[1][0]) {
			case 'U':
				update_item (itip);
				break;
			case 'A':
				change_status (priv->ical_comp, priv->my_address, ICAL_PARTSTAT_ACCEPTED);
				update_item (itip);
				break;
			case 'T':
				change_status (priv->ical_comp, priv->my_address, ICAL_PARTSTAT_TENTATIVE);
				update_item (itip);
				break;
			case 'D':
				change_status (priv->ical_comp, priv->my_address, ICAL_PARTSTAT_DECLINED);
				update_item (itip);
				break;
			case 'F':
				send_freebusy (itip);
				break;
			case 'R':
				update_attendee_status (itip);
				break;
			case 'S':
				send_item (itip);
				break;
			case 'C':
				remove_item (itip);
				break;
			}
		}		

		if (key_value[0] != NULL && !strcmp (key_value[0], "rsvp"))
			if (*key_value[1] == '1')
				rsvp = TRUE;
		
		g_strfreev (key_value);

	}
	g_strfreev (fields);

	if (rsvp) {
		CalComponent *comp = NULL;

		comp = cal_component_clone (priv->comp);
		if (comp == NULL)
			return;
		
		if (priv->my_address != NULL) {
			icalcomponent *ical_comp;
			icalproperty *prop;
			const char *attendee, *text;
			icalvalue *value;
			
			ical_comp = cal_component_get_icalcomponent (comp);

			for (prop = icalcomponent_get_first_property (ical_comp, ICAL_ATTENDEE_PROPERTY);
			     prop != NULL;
			     prop = icalcomponent_get_next_property (ical_comp, ICAL_ATTENDEE_PROPERTY))
			{
				value = icalproperty_get_value (prop);
				if (!value)
					continue;
				
				attendee = icalvalue_get_string (value);
				text = itip_strip_mailto (attendee);

				if (!strstr (text, priv->my_address)) {
					icalcomponent_remove_property (ical_comp, prop);
					icalproperty_free (prop);
				}
			}
			itip_send_comp (CAL_COMPONENT_METHOD_REPLY, comp);
		} else {
			GtkWidget *dialog;

			dialog = gnome_warning_dialog (_("Unable to find any of your identities "
							 "in the attendees list!\n"));
			gnome_dialog_run_and_close (GNOME_DIALOG (dialog));
		}
		gtk_object_unref (GTK_OBJECT (comp));
	}
}
