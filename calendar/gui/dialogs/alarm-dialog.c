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
#include <gtk/gtksignal.h>
#include <gtk/gtktreeview.h>
#include <gtk/gtktreeselection.h>
#include <gtk/gtkoptionmenu.h>
#include <libgnome/gnome-i18n.h>
#include <glade/glade.h>
#include "e-util/e-dialog-widgets.h"
#include "e-util/e-time-utils.h"
#include <libecal/e-cal-util.h>
#include <libecal/e-cal-time-util.h>
#include "e-util/e-dialog-widgets.h"
#include "e-util/e-icon-factory.h"
#include "../calendar-config.h"
#include "comp-editor-util.h"
#include "alarm-options.h"
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

	GtkWidget *button_options;
} Dialog;

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
}

/* fill_widgets handler for the alarm page */
static void
alarm_to_dialog (Dialog *dialog)
{
	GtkWidget *menu;
	GList *l;
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
}

/* fill_component handler for the alarm page */
static void
dialog_to_alarm (Dialog *dialog)
{
	ECalComponentAlarmTrigger trigger;
	ECalComponentAlarmAction action;
	
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
	if (action == E_CAL_COMPONENT_ALARM_EMAIL && !e_cal_component_alarm_has_attendees (dialog->alarm)) {
		char *email;

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

	dialog->button_options = GW ("button-options");

#undef GW

	return (dialog->action
		&& dialog->interval_value
		&& dialog->value_units
		&& dialog->relative
		&& dialog->time
		&& dialog->button_options);
}

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

/* Hooks the widget signals */
static void
init_widgets (Dialog *dialog)
{

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
  
 keep_alive:
	response_id = gtk_dialog_run (GTK_DIALOG (dialog.toplevel));
	switch (response_id) {
	case GTK_RESPONSE_APPLY:
		show_options (&dialog);
		goto keep_alive;

	case GTK_RESPONSE_OK:
		gtk_widget_hide (dialog.toplevel);
		dialog_to_alarm (&dialog);
		break;
		
	default:
		break;
	}

	gtk_widget_destroy (dialog.toplevel);
	g_object_unref (dialog.xml);

	return response_id == GTK_RESPONSE_OK ? TRUE : FALSE;
}
