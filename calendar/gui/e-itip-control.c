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

#include <glib.h>
#include <gtk/gtkmisc.h>
#include <libgnome/gnome-defs.h>
#include <libgnome/gnome-i18n.h>
#include <libgnome/gnome-util.h>
#include <libgnomeui/gnome-stock.h>
#include <libgnomeui/gnome-dialog.h>
#include <libgnomeui/gnome-dialog-util.h>
#include <ical.h>
#include <cal-util/cal-component.h>
#include <cal-client/cal-client.h>
#include <e-util/e-time-utils.h>
#include <e-util/e-dialog-widgets.h>
#include "calendar-config.h"
#include "itip-utils.h"
#include "e-itip-control.h"

struct _EItipControlPrivate {
	GtkWidget *summary;
	GtkWidget *datetime;
	GtkWidget *message;
	GtkWidget *count;
	GtkWidget *options;
	GtkWidget *ok;
	GtkWidget *next;
	GtkWidget *prev;

	CalClient *event_client;
	CalClient *task_client;

	char *vcalendar;
	CalComponent *comp;
	icalcomponent *main_comp;
	icalcomponent *ical_comp;
	icalcompiter iter;
	icalproperty_method method;

	const int *map;
	int current;
	int total;

	gchar *from_address;
	gchar *my_address;
};

/* Option menu maps */
enum { 
	UPDATE_CALENDAR
};

enum { 
	ACCEPT_TO_CALENDAR_RSVP,
	ACCEPT_TO_CALENDAR,
	TENTATIVE_TO_CALENDAR_RSVP,
	TENTATIVE_TO_CALENDAR,
	DECLINE_TO_CALENDAR_RSVP,
	DECLINE_TO_CALENDAR
};

enum { 
	SEND_FREEBUSY
};

enum { 
	CANCEL_CALENDAR
};

static const int publish_map[] = {
	UPDATE_CALENDAR,
	-1
};

static const char *publish_text_map[] = {
	"Update Calendar",
	NULL
};

static const int request_map[] = {
	ACCEPT_TO_CALENDAR_RSVP,
	ACCEPT_TO_CALENDAR,
	TENTATIVE_TO_CALENDAR_RSVP,
	TENTATIVE_TO_CALENDAR,
	DECLINE_TO_CALENDAR_RSVP,
	DECLINE_TO_CALENDAR,
	-1
};

static const char *request_text_map[] = {
	"Accept and RSVP",
	"Accept and do not RSVP",
	"Tentatively accept and RSVP",
	"Tentatively accept and do not RSVP",
	"Decline and RSVP",
	"Decline and do not RSVP",
	NULL
};

static const int request_fb_map[] = {
	SEND_FREEBUSY,
	-1
};

static const char *request_fb_text_map[] = {
	"Send Free/Busy Information",
	NULL
};

static const int reply_map[] = {
	UPDATE_CALENDAR,
	-1
};

static const char *reply_text_map[] = {
	"Update Calendar",
	NULL
};

static const int cancel_map[] = {
	CANCEL_CALENDAR,
	-1
};

static const char *cancel_text_map[] = {
	"Cancel",
	NULL
};

static void class_init	(EItipControlClass	 *klass);
static void init	(EItipControl		 *itip);
static void destroy	(GtkObject               *obj);

static void prev_clicked_cb (GtkWidget *widget, gpointer data);
static void next_clicked_cb (GtkWidget *widget, gpointer data);
static void ok_clicked_cb (GtkWidget *widget, gpointer data);

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
	GtkWidget *hbox, *table, *lbl;
	
	priv = g_new0 (EItipControlPrivate, 1);

	itip->priv = priv;

	/* Header */
	priv->count = gtk_label_new ("0 of 0");
	gtk_widget_show (priv->count);
	priv->prev = gnome_stock_button (GNOME_STOCK_BUTTON_PREV);
	gtk_widget_show (priv->prev);
	priv->next = gnome_stock_button (GNOME_STOCK_BUTTON_NEXT);
	gtk_widget_show (priv->next);

	hbox = gtk_hbox_new (FALSE, 0);
	gtk_box_pack_start (GTK_BOX (hbox), priv->prev, FALSE, FALSE, 4);
	gtk_box_pack_start (GTK_BOX (hbox), priv->count, TRUE, TRUE, 4);
	gtk_box_pack_start (GTK_BOX (hbox), priv->next, FALSE, FALSE, 4);
	gtk_widget_show (hbox);

	gtk_box_pack_start (GTK_BOX (itip), hbox, FALSE, FALSE, 4);

	gtk_signal_connect (GTK_OBJECT (priv->prev), "clicked",
			    GTK_SIGNAL_FUNC (prev_clicked_cb), itip);
	gtk_signal_connect (GTK_OBJECT (priv->next), "clicked",
			    GTK_SIGNAL_FUNC (next_clicked_cb), itip);

	/* Information */
	table = gtk_table_new (1, 3, FALSE);
	gtk_widget_show (table);

	lbl = gtk_label_new (_("Summary:"));
	gtk_misc_set_alignment (GTK_MISC (lbl), 0.0, 0.0);
	gtk_widget_show (lbl);
	gtk_table_attach (GTK_TABLE (table), lbl, 0, 1, 0, 1,
			  GTK_EXPAND & GTK_FILL, GTK_SHRINK, 4, 0);
	priv->summary = gtk_label_new ("");
	gtk_misc_set_alignment (GTK_MISC (priv->summary), 0.0, 0.5);
	gtk_widget_show (priv->summary);
	gtk_table_attach (GTK_TABLE (table), priv->summary, 1, 2, 0, 1,
			  GTK_EXPAND & GTK_FILL, GTK_EXPAND, 4, 0);

	lbl = gtk_label_new (_("Date/Time:"));
	gtk_misc_set_alignment (GTK_MISC (lbl), 0.0, 0.5);
	gtk_widget_show (lbl);
	gtk_table_attach (GTK_TABLE (table), lbl, 0, 1, 1, 2, 
			  GTK_EXPAND, GTK_SHRINK, 4, 0);
	priv->datetime = gtk_label_new ("");
	gtk_misc_set_alignment (GTK_MISC (priv->datetime), 0.0, 0.5);
	gtk_widget_show (priv->datetime);
	gtk_table_attach (GTK_TABLE (table), priv->datetime, 1, 2, 1, 2,
			  GTK_EXPAND & GTK_FILL, GTK_EXPAND, 4, 0);

	priv->message = gtk_label_new ("");
	gtk_label_set_line_wrap (GTK_LABEL (priv->message), TRUE);
	gtk_widget_show (priv->message);
	gtk_table_attach (GTK_TABLE (table), priv->message, 0, 2, 2, 3, 
			  GTK_EXPAND & GTK_FILL, GTK_EXPAND & GTK_FILL, 4, 0);

	gtk_box_pack_start (GTK_BOX (itip), table, FALSE, FALSE, 4);

	/* Actions */
	priv->options = gtk_option_menu_new ();
	gtk_widget_show (priv->options);
	priv->ok = gnome_stock_button (GNOME_STOCK_BUTTON_OK);
	gtk_widget_show (priv->ok);

	hbox = hbox = gtk_hbox_new (FALSE, 0);
	gtk_box_pack_start (GTK_BOX (hbox), priv->options, TRUE, TRUE, 4);
	gtk_box_pack_start (GTK_BOX (hbox), priv->ok, FALSE, FALSE, 4);
	gtk_widget_show (hbox);

	gtk_signal_connect (GTK_OBJECT (priv->ok), "clicked",
			    GTK_SIGNAL_FUNC (ok_clicked_cb), itip);

	gtk_box_pack_start (GTK_BOX (itip), hbox, FALSE, FALSE, 4);

	/* Get the cal clients */
	priv->event_client = start_calendar_server ("evolution/local/Calendar/calendar.ics");
	if (priv->event_client == NULL)
		g_warning ("Unable to start calendar client");
	priv->task_client = start_calendar_server ("evolution/local/Tasks/tasks.ics");
		g_warning ("Unable to start calendar client");
}

static void
destroy (GtkObject *obj)
{
	EItipControl *itip = E_ITIP_CONTROL (obj);
	EItipControlPrivate *priv;

	priv = itip->priv;

	g_free (priv);
}

GtkWidget *
e_itip_control_new (void)
{
	return gtk_type_new (E_TYPE_ITIP_CONTROL);
}

static icalproperty *
find_attendee (icalcomponent *ical_comp, char *address)
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

		/* Here I strip off the "MAILTO:" if it is present. */
		text = strchr (attendee, ':');
		if (text != NULL)
			text++;
		else
			text = attendee;

		if (!strstr (text, address))
			break;
	}
			
	return prop;
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

static GtkWidget *
create_menu (const char **map) 
{
	GtkWidget *menu, *item;
	int i;
	
	menu = gtk_menu_new ();

	for (i = 0; map[i] != NULL; i++) {
		item = gtk_menu_item_new_with_label (map[i]);
		gtk_widget_show (item);
		gtk_menu_append (GTK_MENU (menu), item);
	}
	gtk_widget_show (menu);
	
	return menu;
}

static void
set_options (EItipControl *itip)
{
	EItipControlPrivate *priv;
	gboolean sens = TRUE;
	
	priv = itip->priv;
	
	switch (priv->method) {
	case ICAL_METHOD_PUBLISH:
		priv->map = publish_map;
		gtk_option_menu_set_menu (GTK_OPTION_MENU (priv->options), 
					  create_menu (publish_text_map));
		break;
	case ICAL_METHOD_REQUEST:
		priv->map = request_map;
		gtk_option_menu_set_menu (GTK_OPTION_MENU (priv->options),
					  create_menu (request_text_map));
		break;
	case ICAL_METHOD_REPLY:
		priv->map = reply_map;
		gtk_option_menu_set_menu (GTK_OPTION_MENU (priv->options),
					  create_menu (reply_text_map));
		break;
	case ICAL_METHOD_CANCEL:
		priv->map = cancel_map;
		gtk_option_menu_set_menu (GTK_OPTION_MENU (priv->options),
					  create_menu (cancel_text_map));
		break;
	default:
		priv->map = NULL;
		gtk_option_menu_remove_menu (GTK_OPTION_MENU (priv->options));
		sens = FALSE;
	}

	gtk_widget_set_sensitive (priv->options, sens);
	gtk_widget_set_sensitive (priv->ok, sens);
}


static void
set_options_freebusy (EItipControl *itip)
{
	EItipControlPrivate *priv;
	gboolean sens = TRUE;
	
	priv = itip->priv;
	
	switch (priv->method) {
	case ICAL_METHOD_REQUEST:
		priv->map = request_fb_map;
		gtk_option_menu_set_menu (GTK_OPTION_MENU (priv->options),
					  create_menu (request_fb_text_map));
		break;
	default:
		priv->map = NULL;
		gtk_option_menu_remove_menu (GTK_OPTION_MENU (priv->options));
		sens = FALSE;
	}

	gtk_widget_set_sensitive (priv->options, sens);
	gtk_widget_set_sensitive (priv->ok, sens);
}

static void
write_label_piece (time_t t, char *buffer, int size, char *stext, char *etext)
{
	struct tm *tmp_tm;
	int len;
	
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
set_date_label (GtkWidget *lbl, CalComponent *comp)
{
	CalComponentDateTime datetime;
	time_t start = 0, end = 0, complete = 0, due = 0;
	static char buffer[1024];

	cal_component_get_dtstart (comp, &datetime);
	if (datetime.value)
		start = icaltime_as_timet (*datetime.value);
	cal_component_get_dtend (comp, &datetime);
	if (datetime.value)
		end = icaltime_as_timet (*datetime.value);
	cal_component_get_due (comp, &datetime);
	if (datetime.value)
		due = icaltime_as_timet (*datetime.value);
	cal_component_get_completed (comp, &datetime.value);
	if (datetime.value)
		complete = icaltime_as_timet (*datetime.value);

	buffer[0] = '\0';

	if (start > 0)
		write_label_piece (start, buffer, 1024, NULL, NULL);

	if (end > 0 && start > 0)
		write_label_piece (end, buffer, 1024, _(" to "), NULL);

	if (complete > 0) {
		if (start > 0)
			write_label_piece (complete, buffer, 1024, _(" (Completed "), ")");
		else
			write_label_piece (complete, buffer, 1024, _("Completed "), NULL);
	}
	
	if (due > 0 && complete == 0) {
		if (start > 0)
			write_label_piece (due, buffer, 1024, _(" (Due "), ")");
		else
			write_label_piece (due, buffer, 1024, _("Due "), NULL);
	}

	gtk_label_set_text (GTK_LABEL (lbl), buffer);
}

static void
set_message (EItipControl *itip, gchar *message, gboolean err) 
{
	EItipControlPrivate *priv;
	GtkStyle *style = NULL;
	
	priv = itip->priv;

	if (err) {
		GdkColor color = {0, 65535, 0, 0};
		
		style = gtk_style_copy (gtk_widget_get_style (priv->message));
		style->fg[0] = color;
		gtk_widget_set_style (priv->message, style);
	} else {
		gtk_widget_restore_default_style (priv->message);
	}

	if (message != NULL)
		gtk_label_set_text (GTK_LABEL (priv->message), message);		
	else
		gtk_label_set_text (GTK_LABEL (priv->message), "");

	if (err) {
		gtk_style_unref (style);
	}
}

static void
show_current_event (EItipControl *itip) 
{
	EItipControlPrivate *priv;
	CalComponentText text;
	
	priv = itip->priv;

	set_options (itip);
	
	cal_component_get_summary (priv->comp, &text);
	if (text.value)
		gtk_label_set_text (GTK_LABEL (priv->summary), text.value);
	else
		gtk_label_set_text (GTK_LABEL (priv->summary), "");

	set_date_label (priv->datetime, priv->comp);

	switch (priv->method) {
	case ICAL_METHOD_PUBLISH:
		set_message (itip, _("This is an event that can be added to your calendar."), FALSE);
		break;
	case ICAL_METHOD_REQUEST:
		set_message (itip, _("This is a meeting request."), FALSE);
		break;
	case ICAL_METHOD_ADD:
		set_message (itip, _("This is one or more additions to a current meeting."), FALSE);
		break;
	case ICAL_METHOD_REFRESH:
		set_message (itip, _("This is a request for the latest event information."), FALSE);
		break;
	case ICAL_METHOD_REPLY:
		set_message (itip, _("This is a reply to a meeting request."), FALSE);
		break;
	case ICAL_METHOD_CANCEL:
		set_message (itip, _("This is an event cancellation."), FALSE);
		break;
	default:
		set_message (itip, _("The message is not understandable."), TRUE);
	}
}

static void
show_current_todo (EItipControl *itip) 
{
	EItipControlPrivate *priv;
	CalComponentText text;
	
	priv = itip->priv;

	set_options (itip);
	
	cal_component_get_summary (priv->comp, &text);
	if (text.value)
		gtk_label_set_text (GTK_LABEL (priv->summary), text.value);
	else
		gtk_label_set_text (GTK_LABEL (priv->summary), "");

	set_date_label (priv->datetime, priv->comp);

	switch (priv->method) {
	case ICAL_METHOD_PUBLISH:
		set_message (itip, _("This is an task that can be added to your calendar."), FALSE);
		break;
	case ICAL_METHOD_REQUEST:
		set_message (itip, _("This is a task request."), FALSE);
		break;
	case ICAL_METHOD_REFRESH:
		set_message (itip, _("This is a request for the latest task information."), FALSE);
		break;
	case ICAL_METHOD_REPLY:
		set_message (itip, _("This is a reply to a task request."), FALSE);
		break;
	case ICAL_METHOD_CANCEL:
		set_message (itip, _("This is an task cancellation."), FALSE);
		break;
	default:
		set_message (itip, _("The message is not understandable."), TRUE);
	}
}

static void
show_current_freebusy (EItipControl *itip) 
{
	EItipControlPrivate *priv;
	
	priv = itip->priv;

	set_options_freebusy (itip);
	
	gtk_label_set_text (GTK_LABEL (priv->summary), "");

	set_date_label (priv->datetime, priv->comp);

	switch (priv->method) {
	case ICAL_METHOD_PUBLISH:
		set_message (itip, _("This is freebusy information."), FALSE);
		break;
	case ICAL_METHOD_REQUEST:
		set_message (itip, _("This is a request for freebusy information."), FALSE);
		break;
	case ICAL_METHOD_REPLY:
		set_message (itip, _("This is a reply to a freebusy request."), FALSE);
		break;
	default:
		set_message (itip, _("The message is not understandable."), TRUE);
	}
}

static void
show_current (EItipControl *itip) 
{
	EItipControlPrivate *priv;
	CalComponentVType type;

	priv = itip->priv;

	set_label (itip);
	set_button_status (itip);

	if (priv->comp)
		gtk_object_unref (GTK_OBJECT (priv->comp));
	
	priv->comp = cal_component_new ();
	if (!cal_component_set_icalcomponent (priv->comp, priv->ical_comp)) {
		set_message (itip, _("The message does not appear to be properly formed"), TRUE);
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
		set_message (itip, _("The message contains only unsupported requests."), TRUE);
	}
}

void
e_itip_control_set_data (EItipControl *itip, const gchar *text) 
{
	EItipControlPrivate *priv;
	icalproperty *prop;
	
	priv = itip->priv;
	
	priv->vcalendar = g_strdup (text);
	
	priv->main_comp = icalparser_parse_string (priv->vcalendar);
	if (priv->main_comp == NULL) {
		set_message (itip, _("The information contained in this attachment was not valid"), TRUE);
		priv->comp = NULL;
		priv->total = 0;
		priv->current = 0;
		goto show;
		
	}

	prop = icalcomponent_get_first_property (priv->main_comp, ICAL_METHOD_PROPERTY);
	priv->method = icalproperty_get_method (prop);

	priv->iter = icalcomponent_begin_component (priv->main_comp, ICAL_ANY_COMPONENT);
	priv->ical_comp = icalcompiter_deref (&priv->iter);
	
	priv->total = icalcomponent_count_components (priv->main_comp, ICAL_VEVENT_COMPONENT);
	priv->total += icalcomponent_count_components (priv->main_comp, ICAL_VTODO_COMPONENT);
	priv->total += icalcomponent_count_components (priv->main_comp, ICAL_VFREEBUSY_COMPONENT);
	
	if (priv->total > 0)
		priv->current = 1;
	else
		priv->current = 0;

 show:	
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


void
e_itip_control_set_my_address (EItipControl *itip, const gchar *address)
{
	EItipControlPrivate *priv;

	priv = itip->priv;

	if (priv->my_address)
		g_free (priv->my_address);
	
	priv->my_address = g_strdup (address);
}

const gchar *
e_itip_control_get_my_address (EItipControl *itip)
{
	EItipControlPrivate *priv;

	priv = itip->priv;

	return priv->my_address;
}


static void
update_item (EItipControl *itip) 
{
	EItipControlPrivate *priv;
	CalClient *client;
	CalComponentVType type;
	
	priv = itip->priv;

	type = cal_component_get_vtype (priv->comp);
	if (type == CAL_COMPONENT_TODO)
		client = priv->task_client;
	else 
		client = priv->event_client;
	
	if (!cal_client_update_object (client, priv->comp)) {
		GtkWidget *dialog;
		
		dialog = gnome_warning_dialog(_("I couldn't update your calendar file!\n"));
		gnome_dialog_run (GNOME_DIALOG(dialog));
	}
}

static void
remove_item (EItipControl *itip) 
{
	EItipControlPrivate *priv;
	CalClient *client;
	CalComponentVType type;
	const char *uid;
	
	priv = itip->priv;

	type = cal_component_get_vtype (priv->comp);
	if (type == CAL_COMPONENT_TODO)
		client = priv->task_client;
	else 
		client = priv->event_client;

	cal_component_get_uid (priv->comp, &uid);
	if (!cal_client_remove_object (client, uid)) {
		GtkWidget *dialog;
		
		dialog = gnome_warning_dialog(_("I couldn't remove the item from your calendar file!\n"));
		gnome_dialog_run (GNOME_DIALOG(dialog));
	}
}

static void
send_freebusy (void) 
{
}

static void
change_status (EItipControl *itip, gchar *address, icalparameter_partstat status)
{
	EItipControlPrivate *priv;
	icalproperty *prop;

	priv = itip->priv;

	prop = find_attendee (priv->ical_comp, address);
	if (prop) {
		icalparameter *param;

		icalproperty_remove_parameter (prop, ICAL_PARTSTAT_PARAMETER);
		param = icalparameter_new_partstat (status);
		icalproperty_add_parameter (prop, param);
	}
}

static void
prev_clicked_cb (GtkWidget *widget, gpointer data) 
{
	EItipControl *itip = E_ITIP_CONTROL (data);
	EItipControlPrivate *priv;

	priv = itip->priv;
	
	priv->current--;
	priv->ical_comp = icalcompiter_prior (&priv->iter);

	show_current (itip);
}

static void
next_clicked_cb (GtkWidget *widget, gpointer data)
{
	EItipControl *itip = E_ITIP_CONTROL (data);
	EItipControlPrivate *priv;

	priv = itip->priv;
	
	priv->current++;
	priv->ical_comp = icalcompiter_next (&priv->iter);

	show_current (itip);
}

static void
ok_clicked_cb (GtkWidget *widget, gpointer data)
{
	EItipControl *itip = E_ITIP_CONTROL (data);
	EItipControlPrivate *priv;
	gint selection;
	
	priv = itip->priv;

	selection = e_dialog_option_menu_get (priv->options, priv->map);

	if (priv->map == publish_map) {
		update_item (itip);
	} else if (priv->map == request_map) {
		gboolean rsvp = FALSE;
		
		switch (selection) {
		case ACCEPT_TO_CALENDAR_RSVP:
			rsvp = TRUE;
		case ACCEPT_TO_CALENDAR:
			change_status (itip, priv->my_address, ICAL_PARTSTAT_ACCEPTED);
			break;
		case TENTATIVE_TO_CALENDAR_RSVP:
			rsvp = TRUE;
		case TENTATIVE_TO_CALENDAR:
			change_status (itip, priv->my_address, ICAL_PARTSTAT_TENTATIVE);
			break;
		case DECLINE_TO_CALENDAR_RSVP:
			rsvp = TRUE;
		case DECLINE_TO_CALENDAR:
			change_status (itip, priv->my_address, ICAL_PARTSTAT_DECLINED);
			break;
		}
		update_item (itip);
		if (rsvp)
			itip_send_comp (CAL_COMPONENT_METHOD_REPLY, priv->comp);

	} else if (priv->map == request_fb_map) {
		send_freebusy ();

	} else if (priv->map == reply_map) {
		update_item (itip);

	} else if (priv->map == cancel_map) {
		remove_item (itip);
	}
}
