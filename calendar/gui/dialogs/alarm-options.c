/* Evolution calendar - Alarm options dialog
 *
 * Copyright (C) 2001 Ximian, Inc.
 *
 * Authors: Federico Mena-Quintero <federico@ximian.com>
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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <string.h>
#include <glib.h>
#include <libgnome/gnome-i18n.h>
#include <gtk/gtkmain.h>
#include <gtk/gtkcheckbutton.h>
#include <gtk/gtksignal.h>
#include <gtk/gtkwindow.h>
#include <gtk/gtkhbox.h>
#include <gtk/gtktextbuffer.h>
#include <gtk/gtktextview.h>
#include <bonobo/bonobo-control.h>
#include <bonobo/bonobo-exception.h>
#include <bonobo/bonobo-widget.h>
#include <glade/glade.h>
#include <ebook/e-destination.h>
#include "Evolution-Addressbook-SelectNames.h"
#include "e-util/e-dialog-widgets.h"
#include "alarm-options.h"



typedef struct {
	/* Whether the dialog was accepted or canceled */
	gboolean canceled;

	/* Glade XML data */
	GladeXML *xml;

	/* Toplevel */
	GtkWidget *toplevel;

	/* Buttons */
	GtkWidget *button_ok;
	GtkWidget *button_cancel;

	/* Alarm repeat widgets */
	gboolean repeat;
	GtkWidget *repeat_toggle;
	GtkWidget *repeat_group;
	GtkWidget *repeat_quantity;
	GtkWidget *repeat_value;
	GtkWidget *repeat_unit;

	/* Display alarm widgets */
	GtkWidget *dalarm_group;
	GtkWidget *dalarm_description;

	/* Audio alarm widgets */
	GtkWidget *aalarm_group;
	GtkWidget *aalarm_attach;

	/* Mail alarm widgets */
	const char *email;
	GtkWidget *malarm_group;
	GtkWidget *malarm_address_group;
	GtkWidget *malarm_addresses;
	GtkWidget *malarm_addressbook;
	GtkWidget *malarm_description;
	GNOME_Evolution_Addressbook_SelectNames corba_select_names;

	/* Procedure alarm widgets */
	GtkWidget *palarm_group;
	GtkWidget *palarm_program;
	GtkWidget *palarm_args;
} Dialog;

#define SELECT_NAMES_OAFID "OAFIID:GNOME_Evolution_Addressbook_SelectNames"
static const char *section_name = "Send To";



/* Gets the widgets from the XML file and returns if they are all available. */
static gboolean
get_widgets (Dialog *dialog)
{
#define GW(name) glade_xml_get_widget (dialog->xml, name)

	dialog->toplevel = GW ("alarm-options-toplevel");

	dialog->button_ok = GW ("button-ok");
	dialog->button_cancel = GW ("button-cancel");

	dialog->repeat_toggle = GW ("repeat-toggle");
	dialog->repeat_group = GW ("repeat-group");
	dialog->repeat_quantity = GW ("repeat-quantity");
	dialog->repeat_value = GW ("repeat-value");
	dialog->repeat_unit = GW ("repeat-unit");

	dialog->dalarm_group = GW ("dalarm-group");
	dialog->dalarm_description = GW ("dalarm-description");

	dialog->aalarm_group = GW ("aalarm-group");
	dialog->aalarm_attach = GW ("aalarm-attach");

	dialog->malarm_group = GW ("malarm-group");
	dialog->malarm_address_group = GW ("malarm-address-group");
	dialog->malarm_addressbook = GW ("malarm-addressbook");
	dialog->malarm_description = GW ("malarm-description");
	
	dialog->palarm_group = GW ("palarm-group");
	dialog->palarm_program = GW ("palarm-program");
	dialog->palarm_args = GW ("palarm-args");

	return (dialog->toplevel
		&& dialog->button_ok
		&& dialog->button_cancel
		&& dialog->repeat_toggle
		&& dialog->repeat_group
		&& dialog->repeat_quantity
		&& dialog->repeat_value
		&& dialog->repeat_unit
		&& dialog->dalarm_group
		&& dialog->dalarm_description
		&& dialog->aalarm_group
		&& dialog->aalarm_attach
		&& dialog->malarm_group
		&& dialog->malarm_address_group
		&& dialog->malarm_addressbook
		&& dialog->malarm_description
		&& dialog->palarm_group
		&& dialog->palarm_program
		&& dialog->palarm_args);
}

static void
addressbook_clicked_cb (GtkWidget *widget, gpointer data)
{
	Dialog *dialog = data;
	CORBA_Environment ev;
	
	CORBA_exception_init (&ev);

	GNOME_Evolution_Addressbook_SelectNames_activateDialog (dialog->corba_select_names, 
								section_name, &ev);
	
	CORBA_exception_free (&ev);
}

static gboolean
setup_select_names (Dialog *dialog)
{
	Bonobo_Control corba_control;
	CORBA_Environment ev;
	
	CORBA_exception_init (&ev);
	
	dialog->corba_select_names = bonobo_activation_activate_from_id (SELECT_NAMES_OAFID, 0, NULL, &ev);
	if (BONOBO_EX (&ev))
		return FALSE;
	
	GNOME_Evolution_Addressbook_SelectNames_addSection (dialog->corba_select_names, 
							    section_name, section_name, &ev);
	if (BONOBO_EX (&ev))
		return FALSE;

	corba_control = GNOME_Evolution_Addressbook_SelectNames_getEntryBySection (dialog->corba_select_names, 
										   section_name, &ev);

	if (BONOBO_EX (&ev))
		return FALSE;
	
	CORBA_exception_free (&ev);

	dialog->malarm_addresses = bonobo_widget_new_control_from_objref (corba_control, CORBA_OBJECT_NIL);
	gtk_widget_show (dialog->malarm_addresses);
	gtk_box_pack_end_defaults (GTK_BOX (dialog->malarm_address_group), dialog->malarm_addresses);

	gtk_signal_connect (GTK_OBJECT (dialog->malarm_addressbook), "clicked",
			    GTK_SIGNAL_FUNC (addressbook_clicked_cb), dialog);

	return TRUE;
}

/* Closes the dialog by terminating its main loop */
static void
close_dialog (Dialog *dialog, gboolean canceled)
{
	dialog->canceled = canceled;
	gtk_main_quit ();
}

/* Callback used when the toplevel window is deleted */
static guint
toplevel_delete_event_cb (GtkWidget *widget, GdkEventAny *event, gpointer data)
{
	Dialog *dialog;

	dialog = data;
	close_dialog (dialog, TRUE);
	return TRUE;
}

/* Callback used when the OK button is clicked */
static void
button_ok_clicked_cb (GtkWidget *button, gpointer data)
{
	Dialog *dialog;

	dialog = data;
	close_dialog (dialog, FALSE);
}

/* Callback used when the Cancel button is clicked */
static void
button_cancel_clicked_cb (GtkWidget *button, gpointer data)
{
	Dialog *dialog;

	dialog = data;
	close_dialog (dialog, TRUE);
}

/* Callback used when the repeat toggle button is toggled.  We sensitize the
 * repeat group options as appropriate.
 */
static void
repeat_toggle_toggled_cb (GtkToggleButton *toggle, gpointer data)
{
	Dialog *dialog;
	gboolean active;

	dialog = data;

	active = gtk_toggle_button_get_active (toggle);

	gtk_widget_set_sensitive (dialog->repeat_group, active);
}

/* Hooks the widget signals */
static void
init_widgets (Dialog *dialog)
{
	/* Toplevel, buttons */

	dialog->canceled = TRUE;

	g_signal_connect((dialog->toplevel), "delete_event",
			    G_CALLBACK (toplevel_delete_event_cb), dialog);

	g_signal_connect((dialog->button_ok), "clicked",
			    G_CALLBACK (button_ok_clicked_cb), dialog);

	g_signal_connect((dialog->button_cancel), "clicked",
			    G_CALLBACK (button_cancel_clicked_cb), dialog);

	/* Alarm repeat */

	g_signal_connect((dialog->repeat_toggle), "toggled",
			    G_CALLBACK (repeat_toggle_toggled_cb), dialog);
}

/* Fills the audio alarm widgets with the values from the alarm component */
static void
alarm_to_aalarm_widgets (Dialog *dialog, CalComponentAlarm *alarm)
{
	icalattach *attach;
	const char *url;

	cal_component_alarm_get_attach (alarm, &attach);

	if (!attach) {
		e_dialog_editable_set (dialog->aalarm_attach, NULL);
		return;
	}

	/* FIXME: this does not support inline data */

	url = NULL;

	if (icalattach_get_is_url (attach))
		url = icalattach_get_url (attach);
	else
		g_message ("alarm_to_aalarm_widgets(): FIXME: we don't support inline data yet");

	e_dialog_editable_set (dialog->aalarm_attach, url);

	icalattach_unref (attach);
}

/* Fills the display alarm widgets with the values from the alarm component */
static void
alarm_to_dalarm_widgets (Dialog *dialog, CalComponentAlarm *alarm)
{
	CalComponentText description;
	GtkTextBuffer *text_buffer;
	
	cal_component_alarm_get_description (alarm, &description);

	text_buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (dialog->dalarm_description));
	gtk_text_buffer_set_text (text_buffer, description.value ? description.value : "", -1);
}

/* Fills the mail alarm widgets with the values from the alarm component */
static void
alarm_to_malarm_widgets (Dialog *dialog, CalComponentAlarm *alarm)
{
	CalComponentText description;
	GtkTextBuffer *text_buffer;	
	GSList *attendee_list, *l;
	EDestination **destv;
	int len, i;
	
	/* Recipients */
	cal_component_alarm_get_attendee_list (alarm, &attendee_list);
	len = g_slist_length (attendee_list);

	if (len <= 0) {
		destv = g_new0 (EDestination *, 2);
		destv[0] = e_destination_new ();
		e_destination_set_email (destv[0], dialog->email);
		destv[1] = NULL;
		len = 1;
	} else {
		destv = g_new0 (EDestination *, len + 1);
		for (l = attendee_list, i = 0; l != NULL; l = l->next, i++) {
			CalComponentAttendee *a = l->data;
			EDestination *dest;
			
			dest = e_destination_new ();
			if (a->cn != NULL && *a->cn)
				e_destination_set_name (dest, a->cn);
			if (a->value != NULL && *a->value)
				e_destination_set_email (dest, a->value);
			
			destv[i] = dest;
		}
		destv[i] = NULL;
	}
	
	bonobo_widget_set_property (BONOBO_WIDGET (dialog->malarm_addresses), 
				    "destinations", e_destination_exportv (destv), NULL);

	for (i = 0; i < len; i++)
		g_object_unref (GTK_OBJECT (destv[i]));
	g_free (destv);

	cal_component_free_attendee_list (attendee_list);

	/* Description */
	cal_component_alarm_get_description (alarm, &description);

	text_buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (dialog->malarm_description));
	gtk_text_buffer_set_text (text_buffer, description.value ? description.value : "", -1);
}

/* Fills the procedure alarm widgets with the values from the alarm component */
static void
alarm_to_palarm_widgets (Dialog *dialog, CalComponentAlarm *alarm)
{
	icalattach *attach;
	CalComponentText description;

	cal_component_alarm_get_attach (alarm, &attach);
	cal_component_alarm_get_description (alarm, &description);

	if (attach) {
		const char *url;

		if (icalattach_get_is_url (attach)) {
			url = icalattach_get_url (attach);
			e_dialog_editable_set (dialog->palarm_program, url);
		} else
			g_message ("alarm_to_palarm_widgets(): Don't know what to do with non-URL "
				   "attachments");

		icalattach_unref (attach);
	}

	e_dialog_editable_set (dialog->palarm_args, description.value);
}

enum duration_units {
	DUR_MINUTES,
	DUR_HOURS,
	DUR_DAYS
};

static const int duration_units_map[] = {
	DUR_MINUTES,
	DUR_HOURS,
	DUR_DAYS,
	-1
};

/* Sigh.  Takes an overcomplicated duration value and reduces it to its lowest
 * common denominator.
 */
static void
normalize_duration (struct icaldurationtype dur, int *value, enum duration_units *units)
{
	if (dur.seconds != 0 || dur.minutes != 0) {
		*value = ((((dur.weeks * 7 + dur.days) * 24 + dur.hours) * 60) + dur.minutes
			  + dur.seconds / 60 + ((dur.seconds % 60) >= 30 ? 1 : 0));
		*units = DUR_MINUTES;
	} else if (dur.hours) {
		*value = ((dur.weeks * 7) + dur.days) * 24 + dur.hours;
		*units = DUR_HOURS;
	} else if (dur.days != 0 || dur.weeks != 0) {
		*value = dur.weeks * 7 + dur.days;
		*units = DUR_DAYS;
	} else {
		*value = 0;
		*units = DUR_MINUTES;
	}
}

/* Fills the repeat widgets with the values from the alarm component */
static void
alarm_to_repeat_widgets (Dialog *dialog, CalComponentAlarm *alarm)
{
	CalAlarmRepeat repeat;
	int value;
	enum duration_units units;

	cal_component_alarm_get_repeat (alarm, &repeat);

	/* Sensitivity */

	if (!dialog->repeat || repeat.repetitions == 0) {
		gtk_widget_set_sensitive (dialog->repeat_toggle, dialog->repeat);
		gtk_widget_set_sensitive (dialog->repeat_group, FALSE);
		e_dialog_toggle_set (dialog->repeat_toggle, FALSE);
		return;
	}

	gtk_widget_set_sensitive (dialog->repeat_group, TRUE);
	e_dialog_toggle_set (dialog->repeat_toggle, TRUE);

	/* Repetitions */
	e_dialog_spin_set (dialog->repeat_quantity, repeat.repetitions);

	/* Duration */

	normalize_duration (repeat.duration, &value, &units);

	e_dialog_spin_set (dialog->repeat_value, value);
	e_dialog_option_menu_set (dialog->repeat_unit, units, duration_units_map);
}

/* Fills the widgets with the values from the alarm component */
static void
alarm_to_dialog (Dialog *dialog, CalComponentAlarm *alarm)
{
	CalAlarmAction action;

	alarm_to_repeat_widgets (dialog, alarm);

	cal_component_alarm_get_action (alarm, &action);

	switch (action) {
	case CAL_ALARM_NONE:
		g_assert_not_reached ();
		return;

	case CAL_ALARM_AUDIO:
		gtk_window_set_title (GTK_WINDOW (dialog->toplevel), _("Audio Alarm Options"));
		gtk_widget_show (dialog->aalarm_group);
		gtk_widget_hide (dialog->dalarm_group);
		gtk_widget_hide (dialog->malarm_group);
		gtk_widget_hide (dialog->palarm_group);
		alarm_to_aalarm_widgets (dialog, alarm);
		break;

	case CAL_ALARM_DISPLAY:
		gtk_window_set_title (GTK_WINDOW (dialog->toplevel), _("Message Alarm Options"));
		gtk_widget_hide (dialog->aalarm_group);
		gtk_widget_show (dialog->dalarm_group);
		gtk_widget_hide (dialog->malarm_group);
		gtk_widget_hide (dialog->palarm_group);
		alarm_to_dalarm_widgets (dialog, alarm);
		break;

	case CAL_ALARM_EMAIL:
		gtk_window_set_title (GTK_WINDOW (dialog->toplevel), _("Email Alarm Options"));
		gtk_widget_hide (dialog->aalarm_group);
		gtk_widget_hide (dialog->dalarm_group);
		gtk_widget_show (dialog->malarm_group);
		gtk_widget_hide (dialog->palarm_group);
		alarm_to_malarm_widgets (dialog, alarm);
		break;

	case CAL_ALARM_PROCEDURE:
		gtk_window_set_title (GTK_WINDOW (dialog->toplevel), _("Program Alarm Options"));
		gtk_widget_hide (dialog->aalarm_group);
		gtk_widget_hide (dialog->dalarm_group);
		gtk_widget_hide (dialog->malarm_group);
		gtk_widget_show (dialog->palarm_group);
		alarm_to_palarm_widgets (dialog, alarm);
		break;

	case CAL_ALARM_UNKNOWN:
		gtk_window_set_title (GTK_WINDOW (dialog->toplevel), _("Unknown Alarm Options"));
		break;

	default:
		g_assert_not_reached ();
		return;
	}
}



/* Fills the alarm data with the values from the repeat/duration widgets */
static void
repeat_widgets_to_alarm (Dialog *dialog, CalComponentAlarm *alarm)
{
	CalAlarmRepeat repeat;

	if (!e_dialog_toggle_get (dialog->repeat_toggle)) {
		repeat.repetitions = 0;

		cal_component_alarm_set_repeat (alarm, repeat);
		return;
	}

	repeat.repetitions = e_dialog_spin_get_int (dialog->repeat_quantity);

	memset (&repeat.duration, 0, sizeof (repeat.duration));
	switch (e_dialog_option_menu_get (dialog->repeat_unit, duration_units_map)) {
	case DUR_MINUTES:
		repeat.duration.minutes = e_dialog_spin_get_int (dialog->repeat_value);
		break;

	case DUR_HOURS:
		repeat.duration.hours = e_dialog_spin_get_int (dialog->repeat_value);
		break;

	case DUR_DAYS:
		repeat.duration.days = e_dialog_spin_get_int (dialog->repeat_value);
		break;

	default:
		g_assert_not_reached ();
	}

	cal_component_alarm_set_repeat (alarm, repeat);

}

/* Fills the audio alarm data with the values from the widgets */
static void
aalarm_widgets_to_alarm (Dialog *dialog, CalComponentAlarm *alarm)
{
	char *url;
	icalattach *attach;

	url = e_dialog_editable_get (dialog->aalarm_attach);
	attach = icalattach_new_from_url (url ? url : "");
	g_free (url);

	cal_component_alarm_set_attach (alarm, attach);
	icalattach_unref (attach);
}

/* Fills the display alarm data with the values from the widgets */
static void
dalarm_widgets_to_alarm (Dialog *dialog, CalComponentAlarm *alarm)
{
	char *str;
	CalComponentText description;
	GtkTextBuffer *text_buffer;
	GtkTextIter text_iter_start, text_iter_end;
	icalcomponent *icalcomp;
	icalproperty *icalprop;

	text_buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (dialog->dalarm_description));
	gtk_text_buffer_get_start_iter (text_buffer, &text_iter_start);
	gtk_text_buffer_get_end_iter   (text_buffer, &text_iter_end);
	str = gtk_text_buffer_get_text (text_buffer, &text_iter_start, &text_iter_end, FALSE);

	description.value = str;
	description.altrep = NULL;

	cal_component_alarm_set_description (alarm, &description);
	g_free (str);

	/* remove the X-EVOLUTION-NEEDS-DESCRIPTION property, so that
	 * we don't re-set the alarm's description */
	icalcomp = cal_component_alarm_get_icalcomponent (alarm);
	icalprop = icalcomponent_get_first_property (icalcomp, ICAL_X_PROPERTY);
	while (icalprop) {
		const char *x_name;

		x_name = icalproperty_get_x_name (icalprop);
		if (!strcmp (x_name, "X-EVOLUTION-NEEDS-DESCRIPTION")) {
			icalcomponent_remove_property (icalcomp, icalprop);
			break;
		}

		icalprop = icalcomponent_get_next_property (icalcomp, ICAL_X_PROPERTY);
	}
}

/* Fills the mail alarm data with the values from the widgets */
static void
malarm_widgets_to_alarm (Dialog *dialog, CalComponentAlarm *alarm)
{
	char *str;
	CalComponentText description;
	GSList *attendee_list = NULL;
	EDestination **destv;
	GtkTextBuffer *text_buffer;
	GtkTextIter text_iter_start, text_iter_end;
	icalcomponent *icalcomp;
	icalproperty *icalprop;
	int i;
	
	/* Attendees */
	bonobo_widget_get_property (BONOBO_WIDGET (dialog->malarm_addresses), "destinations", 
				    TC_CORBA_string, &str, NULL);
	destv = e_destination_importv (str);
	g_free (str);
	
	for (i = 0; destv[i] != NULL; i++) {
		EDestination *dest;
		CalComponentAttendee *a;

		dest = destv[i];
		
		a = g_new0 (CalComponentAttendee, 1);
		a->value = e_destination_get_email (dest);
		a->cn = e_destination_get_name (dest);

		attendee_list = g_slist_append (attendee_list, a);
	}

	cal_component_alarm_set_attendee_list (alarm, attendee_list);

	cal_component_free_attendee_list (attendee_list);
	e_destination_freev (destv);	

	/* Description */
	text_buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (dialog->dalarm_description));
	gtk_text_buffer_get_start_iter (text_buffer, &text_iter_start);
	gtk_text_buffer_get_end_iter   (text_buffer, &text_iter_end);
	str = gtk_text_buffer_get_text (text_buffer, &text_iter_start, &text_iter_end, FALSE);

	description.value = str;
	description.altrep = NULL;

	cal_component_alarm_set_description (alarm, &description);
	g_free (str);

	/* remove the X-EVOLUTION-NEEDS-DESCRIPTION property, so that
	 * we don't re-set the alarm's description */
	icalcomp = cal_component_alarm_get_icalcomponent (alarm);
	icalprop = icalcomponent_get_first_property(icalcomp, ICAL_X_PROPERTY);
	while (icalprop) {
		const char *x_name;

		x_name = icalproperty_get_x_name (icalprop);
		if (!strcmp (x_name, "X-EVOLUTION-NEEDS-DESCRIPTION")) {
			icalcomponent_remove_property (icalcomp, icalprop);
			break;
		}

		icalprop = icalcomponent_get_next_property (icalcomp, ICAL_X_PROPERTY);
	}
}

/* Fills the procedure alarm data with the values from the widgets */
static void
palarm_widgets_to_alarm (Dialog *dialog, CalComponentAlarm *alarm)
{
	char *program;
	icalattach *attach;
	char *str;
	CalComponentText description;
	icalcomponent *icalcomp;
	icalproperty *icalprop;

	program = e_dialog_editable_get (dialog->palarm_program);
	attach = icalattach_new_from_url (program ? program : "");
	g_free (program);

	cal_component_alarm_set_attach (alarm, attach);
	icalattach_unref (attach);

	str = e_dialog_editable_get (dialog->palarm_args);
	description.value = str;
	description.altrep = NULL;

	cal_component_alarm_set_description (alarm, &description);
	g_free (str);

	/* remove the X-EVOLUTION-NEEDS-DESCRIPTION property, so that
	 * we don't re-set the alarm's description */
	icalcomp = cal_component_alarm_get_icalcomponent (alarm);
	icalprop = icalcomponent_get_first_property (icalcomp, ICAL_X_PROPERTY);
	while (icalprop) {
		const char *x_name;

		x_name = icalproperty_get_x_name (icalprop);
		if (!strcmp (x_name, "X-EVOLUTION-NEEDS-DESCRIPTION")) {
			icalcomponent_remove_property (icalcomp, icalprop);
			break;
		}

		icalprop = icalcomponent_get_next_property (icalcomp, ICAL_X_PROPERTY);
	}
}

/* Fills the alarm data with the values from the widgets */
static void
dialog_to_alarm (Dialog *dialog, CalComponentAlarm *alarm)
{
	CalAlarmAction action;

	repeat_widgets_to_alarm (dialog, alarm);

	cal_component_alarm_get_action (alarm, &action);

	switch (action) {
	case CAL_ALARM_NONE:
		g_assert_not_reached ();
		break;

	case CAL_ALARM_AUDIO:
		aalarm_widgets_to_alarm (dialog, alarm);
		break;

	case CAL_ALARM_DISPLAY:
		dalarm_widgets_to_alarm (dialog, alarm);
		break;

	case CAL_ALARM_EMAIL:
		malarm_widgets_to_alarm (dialog, alarm);
		break;

	case CAL_ALARM_PROCEDURE:
		palarm_widgets_to_alarm (dialog, alarm);
		break;

	case CAL_ALARM_UNKNOWN:
		break;

	default:
		g_assert_not_reached ();
	}
}



/**
 * alarm_options_dialog_run:
 * @alarm: Alarm that is to be edited.
 * 
 * Runs an alarm options dialog modally.
 * 
 * Return value: TRUE if the dialog could be created, FALSE otherwise.
 **/
gboolean
alarm_options_dialog_run (CalComponentAlarm *alarm, const char *email, gboolean repeat)
{
	Dialog dialog;

	g_return_val_if_fail (alarm != NULL, FALSE);

 	dialog.repeat = repeat;
 	dialog.email = email;
	dialog.xml = glade_xml_new (EVOLUTION_GLADEDIR "/alarm-options.glade", NULL, NULL);
	if (!dialog.xml) {
		g_message ("alarm_options_dialog_new(): Could not load the Glade XML file!");
		return FALSE;
	}

	if (!get_widgets (&dialog)) {
		g_object_unref(dialog.xml);
		return FALSE;
	}

	if (!setup_select_names (&dialog)) {
  		g_object_unref (dialog.xml);
  		return FALSE;
  	}

	init_widgets (&dialog);

	alarm_to_dialog (&dialog, alarm);

	gtk_widget_show (dialog.toplevel);
	gtk_main ();

	if (!dialog.canceled)
		dialog_to_alarm (&dialog, alarm);

	gtk_widget_destroy (dialog.toplevel);
	g_object_unref(dialog.xml);

	return TRUE;
}
