/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */

/* Evolution calendar - Alarm page of the calendar component dialogs
 *
 * Copyright (C) 2001-2003 Ximian, Inc.
 *
 * Authors: Federico Mena-Quintero <federico@ximian.com>
 *          Miguel de Icaza <miguel@ximian.com>
 *          Seth Alves <alves@hungry.com>
 *          JP Rosevear <jpr@ximian.com>
 *          Hans Petter Jansson <hpj@ximian.com>
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
#include <gtk/gtklabel.h>
#include <gtk/gtkcellrenderertext.h>
#include <gtk/gtkdialog.h>
#include <gtk/gtknotebook.h>
#include <gtk/gtksignal.h>
#include <gtk/gtktreeview.h>
#include <gtk/gtktreeselection.h>
#include <gtk/gtkoptionmenu.h>
#include <gtk/gtktextbuffer.h>
#include <gtk/gtktextview.h>
#include <gtk/gtktogglebutton.h>
#include <libgnome/gnome-i18n.h>
#include <bonobo/bonobo-control.h>
#include <bonobo/bonobo-exception.h>
#include <bonobo/bonobo-widget.h>
#include <glade/glade.h>
#include "e-util/e-dialog-widgets.h"
#include "e-util/e-time-utils.h"
#include <libecal/e-cal-util.h>
#include <libecal/e-cal-time-util.h>
#include "e-util/e-dialog-widgets.h"
#include "e-util/e-icon-factory.h"
#include <addressbook/util/e-destination.h>
#include "Evolution-Addressbook-SelectNames.h"
#include "../calendar-config.h"
#include "comp-editor-util.h"
#include "alarm-dialog.h"



typedef struct {
	/* Glade XML data */
	GladeXML *xml;

	/* The alarm  */
	ECalComponentAlarm *alarm;

	/* The client */
	ECal *ecal;
	
	/* Toplevel */
	GtkWidget *toplevel;

	GtkWidget *action;
	GtkWidget *interval_value;
	GtkWidget *value_units;
	GtkWidget *relative;
	GtkWidget *time;

	/* Alarm repeat widgets */
	GtkWidget *repeat_toggle;
	GtkWidget *repeat_group;
	GtkWidget *repeat_quantity;
	GtkWidget *repeat_value;
	GtkWidget *repeat_unit;

	GtkWidget *option_notebook;
	
	/* Display alarm widgets */
	GtkWidget *dalarm_group;
	GtkWidget *dalarm_message;
	GtkWidget *dalarm_description;

	/* Audio alarm widgets */
	GtkWidget *aalarm_group;
	GtkWidget *aalarm_sound;
	GtkWidget *aalarm_attach;

	/* Mail alarm widgets */
	const char *email;
	GtkWidget *malarm_group;
	GtkWidget *malarm_address_group;
	GtkWidget *malarm_addresses;
	GtkWidget *malarm_addressbook;
	GtkWidget *malarm_message;
	GtkWidget *malarm_description;
	GNOME_Evolution_Addressbook_SelectNames corba_select_names;

	/* Procedure alarm widgets */
	GtkWidget *palarm_group;
	GtkWidget *palarm_program;
	GtkWidget *palarm_args;
} Dialog;

#define SELECT_NAMES_OAFID "OAFIID:GNOME_Evolution_Addressbook_SelectNames:" BASE_VERSION
static const char *section_name = "Send To";

/* "relative" types */
enum {
	BEFORE,
	AFTER
};

/* Time units */
enum {
	MINUTES,
	HOURS,
	DAYS
};

/* Option menu maps */
static const int action_map[] = {
	E_CAL_COMPONENT_ALARM_DISPLAY,
	E_CAL_COMPONENT_ALARM_AUDIO,
	E_CAL_COMPONENT_ALARM_PROCEDURE,
	E_CAL_COMPONENT_ALARM_EMAIL,
	-1
};

static const char *action_map_cap[] = {
	CAL_STATIC_CAPABILITY_NO_DISPLAY_ALARMS,
	CAL_STATIC_CAPABILITY_NO_AUDIO_ALARMS,
        CAL_STATIC_CAPABILITY_NO_PROCEDURE_ALARMS,
	CAL_STATIC_CAPABILITY_NO_EMAIL_ALARMS
};

static const int value_map[] = {
	MINUTES,
	HOURS,
	DAYS,
	-1
};

static const int relative_map[] = {
	BEFORE,
	AFTER,
	-1
};

static const int time_map[] = {
	E_CAL_COMPONENT_ALARM_TRIGGER_RELATIVE_START,
	E_CAL_COMPONENT_ALARM_TRIGGER_RELATIVE_END,
	-1
};

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

/* Fills the widgets with default values */
static void
clear_widgets (Dialog *dialog)
{
	/* Sane defaults */
	e_dialog_option_menu_set (dialog->action, E_CAL_COMPONENT_ALARM_DISPLAY, action_map);
	e_dialog_spin_set (dialog->interval_value, 15);
	e_dialog_option_menu_set (dialog->value_units, MINUTES, value_map);
	e_dialog_option_menu_set (dialog->relative, BEFORE, relative_map);
	e_dialog_option_menu_set (dialog->time, E_CAL_COMPONENT_ALARM_TRIGGER_RELATIVE_START, time_map);

	gtk_widget_set_sensitive (dialog->repeat_group, FALSE);
	gtk_widget_set_sensitive (dialog->dalarm_group, FALSE);
	gtk_widget_set_sensitive (dialog->aalarm_group, FALSE);
	gtk_widget_set_sensitive (dialog->malarm_group, FALSE);

	gtk_notebook_set_current_page (GTK_NOTEBOOK (dialog->option_notebook), 0);
}

/* fill_widgets handler for the alarm page */
static void
alarm_to_dialog (Dialog *dialog)
{
	GtkWidget *menu;
	GList *l;
	gboolean repeat;
	char *email;
	int i;	

	/* Clean the page */
	clear_widgets (dialog);

	/* Alarm types */
	menu = gtk_option_menu_get_menu (GTK_OPTION_MENU (dialog->action));
	for (i = 0, l = GTK_MENU_SHELL (menu)->children; action_map[i] != -1; i++, l = l->next) {
		if (e_cal_get_static_capability (dialog->ecal, action_map_cap[i]))
			gtk_widget_set_sensitive (l->data, FALSE);
		else
			gtk_widget_set_sensitive (l->data, TRUE);
	}

	/* Set a default address if possible */
	if (!e_cal_get_static_capability (dialog->ecal, CAL_STATIC_CAPABILITY_NO_EMAIL_ALARMS)
	    && e_cal_get_alarm_email_address (dialog->ecal, &email, NULL)) {
		ECalComponentAttendee *a;
		GSList attendee_list;
		
		a = g_new0 (ECalComponentAttendee, 1);
		a->value = email;
		attendee_list.data = a;
		attendee_list.next = NULL;
		e_cal_component_alarm_set_attendee_list (dialog->alarm, &attendee_list);
		g_free (email);
		g_free (a);
	}

	/* If we can repeat */
	repeat = !e_cal_get_static_capability (dialog->ecal, CAL_STATIC_CAPABILITY_NO_ALARM_REPEAT);
	gtk_widget_set_sensitive (dialog->repeat_toggle, repeat);
}

static void
repeat_widgets_to_alarm (Dialog *dialog, ECalComponentAlarm *alarm)
{
	ECalComponentAlarmRepeat repeat;

	if (!e_dialog_toggle_get (dialog->repeat_toggle)) {
		repeat.repetitions = 0;

		e_cal_component_alarm_set_repeat (alarm, repeat);
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

	e_cal_component_alarm_set_repeat (alarm, repeat);

}

/* Fills the audio alarm data with the values from the widgets */
static void
aalarm_widgets_to_alarm (Dialog *dialog, ECalComponentAlarm *alarm)
{
	char *url;
	icalattach *attach;

	if (!gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (dialog->aalarm_sound)))
		return;

	url = e_dialog_editable_get (dialog->aalarm_attach);
	attach = icalattach_new_from_url (url ? url : "");
	g_free (url);

	e_cal_component_alarm_set_attach (alarm, attach);
	icalattach_unref (attach);
}

/* Fills the display alarm data with the values from the widgets */
static void
dalarm_widgets_to_alarm (Dialog *dialog, ECalComponentAlarm *alarm)
{
	char *str;
	ECalComponentText description;
	GtkTextBuffer *text_buffer;
	GtkTextIter text_iter_start, text_iter_end;
	icalcomponent *icalcomp;
	icalproperty *icalprop;

	if (!gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (dialog->dalarm_message)))
		return;

	text_buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (dialog->dalarm_description));
	gtk_text_buffer_get_start_iter (text_buffer, &text_iter_start);
	gtk_text_buffer_get_end_iter   (text_buffer, &text_iter_end);
	str = gtk_text_buffer_get_text (text_buffer, &text_iter_start, &text_iter_end, FALSE);

	description.value = str;
	description.altrep = NULL;

	e_cal_component_alarm_set_description (alarm, &description);
	g_free (str);

	/* remove the X-EVOLUTION-NEEDS-DESCRIPTION property, so that
	 * we don't re-set the alarm's description */
	icalcomp = e_cal_component_alarm_get_icalcomponent (alarm);
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
malarm_widgets_to_alarm (Dialog *dialog, ECalComponentAlarm *alarm)
{
	char *str;
	ECalComponentText description;
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
		ECalComponentAttendee *a;

		dest = destv[i];
		
		a = g_new0 (ECalComponentAttendee, 1);
		a->value = e_destination_get_email (dest);
		a->cn = e_destination_get_name (dest);

		attendee_list = g_slist_append (attendee_list, a);
	}

	e_cal_component_alarm_set_attendee_list (alarm, attendee_list);

	e_cal_component_free_attendee_list (attendee_list);
	e_destination_freev (destv);	

	if (!gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (dialog->malarm_message)))
		return;
	
	/* Description */
	text_buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (dialog->malarm_description));
	gtk_text_buffer_get_start_iter (text_buffer, &text_iter_start);
	gtk_text_buffer_get_end_iter   (text_buffer, &text_iter_end);
	str = gtk_text_buffer_get_text (text_buffer, &text_iter_start, &text_iter_end, FALSE);

	description.value = str;
	description.altrep = NULL;

	e_cal_component_alarm_set_description (alarm, &description);
	g_free (str);

	/* remove the X-EVOLUTION-NEEDS-DESCRIPTION property, so that
	 * we don't re-set the alarm's description */
	icalcomp = e_cal_component_alarm_get_icalcomponent (alarm);
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
palarm_widgets_to_alarm (Dialog *dialog, ECalComponentAlarm *alarm)
{
	char *program;
	icalattach *attach;
	char *str;
	ECalComponentText description;
	icalcomponent *icalcomp;
	icalproperty *icalprop;

	program = e_dialog_editable_get (dialog->palarm_program);
	attach = icalattach_new_from_url (program ? program : "");
	g_free (program);

	e_cal_component_alarm_set_attach (alarm, attach);
	icalattach_unref (attach);

	str = e_dialog_editable_get (dialog->palarm_args);
	if (str && *str) {
		description.value = str;
		description.altrep = NULL;
		
		e_cal_component_alarm_set_description (alarm, &description);
	}
	g_free (str);
	
	/* remove the X-EVOLUTION-NEEDS-DESCRIPTION property, so that
	 * we don't re-set the alarm's description */
	icalcomp = e_cal_component_alarm_get_icalcomponent (alarm);
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

/* fill_component handler for the alarm page */
static void
dialog_to_alarm (Dialog *dialog)
{
	ECalComponentAlarmTrigger trigger;
	ECalComponentAlarmAction action;

	/* Fill out the alarm */
	memset (&trigger, 0, sizeof (ECalComponentAlarmTrigger));
	trigger.type = e_dialog_option_menu_get (dialog->time, time_map);
	if (e_dialog_option_menu_get (dialog->relative, relative_map) == BEFORE)
		trigger.u.rel_duration.is_neg = 1;
	else
		trigger.u.rel_duration.is_neg = 0;

	switch (e_dialog_option_menu_get (dialog->value_units, value_map)) {
	case MINUTES:
		trigger.u.rel_duration.minutes =
			e_dialog_spin_get_int (dialog->interval_value);
		break;

	case HOURS:
		trigger.u.rel_duration.hours =
			e_dialog_spin_get_int (dialog->interval_value);
		break;

	case DAYS:
		trigger.u.rel_duration.days =
			e_dialog_spin_get_int (dialog->interval_value);
		break;

	default:
		g_assert_not_reached ();
	}
	e_cal_component_alarm_set_trigger (dialog->alarm, trigger);

	action = e_dialog_option_menu_get (dialog->action, action_map);
	e_cal_component_alarm_set_action (dialog->alarm, action);

	/* Repeat stuff */
	repeat_widgets_to_alarm (dialog, dialog->alarm);

	/* Options */
	switch (action) {
	case E_CAL_COMPONENT_ALARM_NONE:
		g_assert_not_reached ();
		break;

	case E_CAL_COMPONENT_ALARM_AUDIO:
		aalarm_widgets_to_alarm (dialog, dialog->alarm);
		break;

	case E_CAL_COMPONENT_ALARM_DISPLAY:
		dalarm_widgets_to_alarm (dialog, dialog->alarm);
		break;

	case E_CAL_COMPONENT_ALARM_EMAIL:
		malarm_widgets_to_alarm (dialog, dialog->alarm);
		break;

	case E_CAL_COMPONENT_ALARM_PROCEDURE:
		palarm_widgets_to_alarm (dialog, dialog->alarm);
		break;

	case E_CAL_COMPONENT_ALARM_UNKNOWN:
		break;

	default:
		g_assert_not_reached ();
	}
}

/* Gets the widgets from the XML file and returns TRUE if they are all available. */
static gboolean
get_widgets (Dialog *dialog)
{
#define GW(name) glade_xml_get_widget (dialog->xml, name)

	dialog->toplevel = GW ("alarm-dialog");
	if (!dialog->toplevel)
		return FALSE;

	dialog->action = GW ("action");
	dialog->interval_value = GW ("interval-value");
	dialog->value_units = GW ("value-units");
	dialog->relative = GW ("relative");
	dialog->time = GW ("time");

	dialog->repeat_toggle = GW ("repeat-toggle");
	dialog->repeat_group = GW ("repeat-group");
	dialog->repeat_quantity = GW ("repeat-quantity");
	dialog->repeat_value = GW ("repeat-value");
	dialog->repeat_unit = GW ("repeat-unit");

	dialog->option_notebook = GW ("option-notebook");

	dialog->dalarm_group = GW ("dalarm-group");
	dialog->dalarm_message = GW ("dalarm-message");
	dialog->dalarm_description = GW ("dalarm-description");

	dialog->aalarm_group = GW ("aalarm-group");
	dialog->aalarm_sound = GW ("aalarm-sound");
	dialog->aalarm_attach = GW ("aalarm-attach");

	dialog->malarm_group = GW ("malarm-group");
	dialog->malarm_address_group = GW ("malarm-address-group");
	dialog->malarm_addressbook = GW ("malarm-addressbook");
	dialog->malarm_message = GW ("malarm-message");
	dialog->malarm_description = GW ("malarm-description");
	
	dialog->palarm_group = GW ("palarm-group");
	dialog->palarm_program = GW ("palarm-program");
	dialog->palarm_args = GW ("palarm-args");

#undef GW

	return (dialog->action
		&& dialog->interval_value
		&& dialog->value_units
		&& dialog->relative
		&& dialog->time
		&& dialog->repeat_toggle
		&& dialog->repeat_group
		&& dialog->repeat_quantity
		&& dialog->repeat_value
		&& dialog->repeat_unit
		&& dialog->option_notebook
		&& dialog->dalarm_group
		&& dialog->dalarm_message
		&& dialog->dalarm_description
		&& dialog->aalarm_group
		&& dialog->aalarm_sound
		&& dialog->aalarm_attach
		&& dialog->malarm_group
		&& dialog->malarm_address_group
		&& dialog->malarm_addressbook
		&& dialog->malarm_message
		&& dialog->malarm_description
		&& dialog->palarm_group
		&& dialog->palarm_program
		&& dialog->palarm_args);		
}

#if 0
/* Callback used when the alarm options button is clicked */
static void
show_options (Dialog *dialog)
{
	gboolean repeat;
	char *email;

	e_cal_component_alarm_set_action (dialog->alarm,
					e_dialog_option_menu_get (dialog->action, action_map));

	repeat = !e_cal_get_static_capability (dialog->ecal, CAL_STATIC_CAPABILITY_NO_ALARM_REPEAT);

	if (e_cal_get_static_capability (dialog->ecal, CAL_STATIC_CAPABILITY_NO_EMAIL_ALARMS)
	    || e_cal_get_alarm_email_address (dialog->ecal, &email, NULL)) {
		if (!alarm_options_dialog_run (dialog->toplevel, dialog->alarm, email, repeat))
			g_message (G_STRLOC ": not create the alarm options dialog");
	}
}
#endif

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

/* Callback used when the repeat toggle button is toggled.  We sensitize the
 * repeat group options as appropriate.
 */
static void
repeat_toggle_toggled_cb (GtkToggleButton *toggle, gpointer data)
{
	Dialog *dialog = data;
	gboolean active;

	active = gtk_toggle_button_get_active (toggle);

	gtk_widget_set_sensitive (dialog->repeat_group, active);
}

static void
check_custom_sound (Dialog *dialog)
{
	char *str;
	gboolean sens;
	
	str = e_dialog_editable_get (dialog->aalarm_attach);

	sens = e_dialog_toggle_get (dialog->aalarm_sound) ? str && *str : TRUE;
	gtk_dialog_set_response_sensitive (GTK_DIALOG (dialog->toplevel), GTK_RESPONSE_OK, sens);

	g_free (str);
}

static void
aalarm_sound_toggled_cb (GtkToggleButton *toggle, gpointer data)
{
	Dialog *dialog = data;
	gboolean active;

	active = gtk_toggle_button_get_active (toggle);

	gtk_widget_set_sensitive (dialog->aalarm_group, active);
	check_custom_sound (dialog);
}

static void
aalarm_attach_changed_cb (GtkWidget *widget, gpointer data)
{
	Dialog *dialog = data;
	
	check_custom_sound (dialog);
}

static void
check_custom_message (Dialog *dialog)
{
	char *str;
	GtkTextBuffer *text_buffer;
	GtkTextIter text_iter_start, text_iter_end;
	gboolean sens;
	
	text_buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (dialog->dalarm_description));
	gtk_text_buffer_get_start_iter (text_buffer, &text_iter_start);
	gtk_text_buffer_get_end_iter   (text_buffer, &text_iter_end);
	str = gtk_text_buffer_get_text (text_buffer, &text_iter_start, &text_iter_end, FALSE);

	sens = e_dialog_toggle_get (dialog->dalarm_message) ? str && *str : TRUE;
	gtk_dialog_set_response_sensitive (GTK_DIALOG (dialog->toplevel), GTK_RESPONSE_OK, sens);

	g_free (str);
}

static void
dalarm_message_toggled_cb (GtkToggleButton *toggle, gpointer data)
{
	Dialog *dialog = data;
	gboolean active;

	active = gtk_toggle_button_get_active (toggle);

	gtk_widget_set_sensitive (dialog->dalarm_group, active);
	check_custom_message (dialog);
}

static void
dalarm_description_changed_cb (GtkWidget *widget, gpointer data)
{
	Dialog *dialog = data;
	
	check_custom_message (dialog);
}

static void
check_custom_program (Dialog *dialog)
{
	char *str;
	gboolean sens;
	
	str = e_dialog_editable_get (dialog->palarm_program);

	sens = str && *str;
	gtk_dialog_set_response_sensitive (GTK_DIALOG (dialog->toplevel), GTK_RESPONSE_OK, sens);
}

static void
palarm_program_changed_cb (GtkWidget *widget, gpointer data)
{
	Dialog *dialog = data;
	
	check_custom_program (dialog);
}

static void
check_custom_email (Dialog *dialog)
{
	char *str;
	GtkTextBuffer *text_buffer;
	GtkTextIter text_iter_start, text_iter_end;
	EDestination **destv;
	gboolean sens;

	bonobo_widget_get_property (BONOBO_WIDGET (dialog->malarm_addresses), "destinations", 
				    TC_CORBA_string, &str, NULL);
	destv = e_destination_importv (str);
	g_free (str);
	
	text_buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (dialog->malarm_description));
	gtk_text_buffer_get_start_iter (text_buffer, &text_iter_start);
	gtk_text_buffer_get_end_iter   (text_buffer, &text_iter_end);
	str = gtk_text_buffer_get_text (text_buffer, &text_iter_start, &text_iter_end, FALSE);

	sens = (destv != NULL) && (e_dialog_toggle_get (dialog->malarm_message) ? str && *str : TRUE);
	gtk_dialog_set_response_sensitive (GTK_DIALOG (dialog->toplevel), GTK_RESPONSE_OK, sens);

	e_destination_freev (destv);
}

static void
malarm_addresses_changed_cb  (BonoboListener    *listener,
			      const char        *event_name,
			      const CORBA_any   *arg,
			      CORBA_Environment *ev,
			      gpointer           data)
{
	Dialog *dialog = data;
	
	check_custom_email (dialog);
}

static void
malarm_message_toggled_cb (GtkToggleButton *toggle, gpointer data)
{
	Dialog *dialog = data;
	gboolean active;

	active = gtk_toggle_button_get_active (toggle);

	gtk_widget_set_sensitive (dialog->malarm_group, active);
	check_custom_email (dialog);
}

static void
malarm_description_changed_cb (GtkWidget *widget, gpointer data)
{
	Dialog *dialog = data;
	
	check_custom_email (dialog);
}

static void
action_selection_done_cb (GtkMenuShell *menu_shell, gpointer data)
{
	Dialog *dialog = data;
	ECalComponentAlarmAction action;
	int page = 0, i;
	
	action = e_dialog_option_menu_get (dialog->action, action_map);
	for (i = 0; action_map[i] != -1 ; i++) {
		if (action == action_map[i]) {
			page = i;
			break;
		}
	}
	
	gtk_notebook_set_page (GTK_NOTEBOOK (dialog->option_notebook), page);

	switch (action) {	
	case E_CAL_COMPONENT_ALARM_AUDIO:
		check_custom_sound (dialog);
		break;

	case E_CAL_COMPONENT_ALARM_DISPLAY:
		check_custom_message (dialog);
		break;

	case E_CAL_COMPONENT_ALARM_EMAIL:
		check_custom_email (dialog);
		break;

	case E_CAL_COMPONENT_ALARM_PROCEDURE:
		check_custom_program (dialog);
		break;
	default:
		g_assert_not_reached ();
		return;
	}
}

/* Hooks the widget signals */
static void
init_widgets (Dialog *dialog)
{
	GtkWidget *menu;
	GtkTextBuffer *text_buffer;
	BonoboControlFrame *cf;
	Bonobo_PropertyBag pb = CORBA_OBJECT_NIL;
	
	menu = gtk_option_menu_get_menu (GTK_OPTION_MENU (dialog->action));
	g_signal_connect (menu, "selection_done",
			  G_CALLBACK (action_selection_done_cb),
			  dialog);

	g_signal_connect (G_OBJECT (dialog->repeat_toggle), "toggled",
			  G_CALLBACK (repeat_toggle_toggled_cb), dialog);

	/* Handle custom sounds */
	g_signal_connect (G_OBJECT (dialog->aalarm_sound), "toggled",
			  G_CALLBACK (aalarm_sound_toggled_cb), dialog);
	g_signal_connect (G_OBJECT (dialog->aalarm_attach), "changed",
			  G_CALLBACK (aalarm_attach_changed_cb), dialog);

	/* Handle custom messages */
	g_signal_connect (G_OBJECT (dialog->dalarm_message), "toggled",
			  G_CALLBACK (dalarm_message_toggled_cb), dialog);
	text_buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (dialog->dalarm_description));
	g_signal_connect (G_OBJECT (text_buffer), "changed",
			  G_CALLBACK (dalarm_description_changed_cb), dialog);

	/* Handle program */
	g_signal_connect (G_OBJECT (dialog->palarm_program), "changed",
			  G_CALLBACK (palarm_program_changed_cb), dialog);

	/* Handle custom email */
	g_signal_connect (G_OBJECT (dialog->malarm_message), "toggled",
			  G_CALLBACK (malarm_message_toggled_cb), dialog);
	text_buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (dialog->malarm_description));
	g_signal_connect (G_OBJECT (text_buffer), "changed",
			  G_CALLBACK (malarm_description_changed_cb), dialog);

	cf = bonobo_widget_get_control_frame (BONOBO_WIDGET (dialog->malarm_addresses));
	pb = bonobo_control_frame_get_control_property_bag (cf, NULL);
	
	bonobo_event_source_client_add_listener (pb, malarm_addresses_changed_cb,
						 "Bonobo/Property:change:entry_changed",
						 NULL, dialog);
}

gboolean
alarm_dialog_run (GtkWidget *parent, ECal *ecal, ECalComponentAlarm *alarm)
{
	Dialog dialog;
	int response_id;
	GList *icon_list;
	
	g_return_val_if_fail (alarm != NULL, FALSE);

	dialog.alarm = alarm;
	dialog.ecal = ecal;
	
	dialog.xml = glade_xml_new (EVOLUTION_GLADEDIR "/alarm-dialog.glade", NULL, NULL);
	if (!dialog.xml) {
		g_message (G_STRLOC ": Could not load the Glade XML file!");
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

	alarm_to_dialog (&dialog);
	
	icon_list = e_icon_factory_get_icon_list ("stock_calendar");
	if (icon_list) {
		gtk_window_set_icon_list (GTK_WINDOW (dialog.toplevel), icon_list);
		g_list_foreach (icon_list, (GFunc) g_object_unref, NULL);
		g_list_free (icon_list);
	}

	gtk_window_set_transient_for (GTK_WINDOW (dialog.toplevel),
				      GTK_WINDOW (parent));
  
	response_id = gtk_dialog_run (GTK_DIALOG (dialog.toplevel));

	if (response_id == GTK_RESPONSE_OK)
		dialog_to_alarm (&dialog);

	gtk_widget_destroy (dialog.toplevel);
	g_object_unref (dialog.xml);

	return response_id == GTK_RESPONSE_OK ? TRUE : FALSE;
}
