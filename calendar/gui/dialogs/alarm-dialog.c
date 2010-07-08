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
 *		Federico Mena-Quintero <federico@ximian.com>
 *      Miguel de Icaza <miguel@ximian.com>
 *      Seth Alves <alves@hungry.com>
 *      JP Rosevear <jpr@ximian.com>
 *      Hans Petter Jansson <hpj@ximian.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <string.h>
#include <gtk/gtk.h>
#include <glib/gi18n.h>
#include <libedataserver/e-time-utils.h>
#include "e-util/e-dialog-widgets.h"
#include <libecal/e-cal-util.h>
#include <libecal/e-cal-time-util.h>
#include "e-util/e-util.h"
#include "e-util/e-dialog-widgets.h"
#include "e-util/e-util-private.h"
#include <libebook/e-destination.h>
#include <libedataserverui/e-name-selector.h>
#include <libical/icalattach.h>
#include "../calendar-config.h"
#include "comp-editor-util.h"
#include "alarm-dialog.h"



typedef struct {
	GtkBuilder *builder;

	/* The alarm  */
	ECalComponentAlarm *alarm;

	/* The client */
	ECal *ecal;

	/* Toplevel */
	GtkWidget *toplevel;

	GtkWidget *action_combo;
	GtkWidget *interval_value;
	GtkWidget *value_units_combo;
	GtkWidget *relative_combo;
	GtkWidget *time_combo;

	/* Alarm repeat widgets */
	GtkWidget *repeat_toggle;
	GtkWidget *repeat_group;
	GtkWidget *repeat_quantity;
	GtkWidget *repeat_value;
	GtkWidget *repeat_unit_combo;

	GtkWidget *option_notebook;

	/* Display alarm widgets */
	GtkWidget *dalarm_group;
	GtkWidget *dalarm_message;
	GtkWidget *dalarm_description;

	/* Audio alarm widgets */
	GtkWidget *aalarm_group;
	GtkWidget *aalarm_sound;
	GtkWidget *aalarm_file_chooser;

	/* Mail alarm widgets */
	const gchar *email;
	GtkWidget *malarm_group;
	GtkWidget *malarm_address_group;
	GtkWidget *malarm_addresses;
	GtkWidget *malarm_addressbook;
	GtkWidget *malarm_message;
	GtkWidget *malarm_description;

	/* Procedure alarm widgets */
	GtkWidget *palarm_group;
	GtkWidget *palarm_program;
	GtkWidget *palarm_args;

	/* Addressbook name selector */
	ENameSelector *name_selector;
} Dialog;

static const gchar *section_name = "Send To";

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

/* Combo box maps */
static const gint action_map[] = {
	E_CAL_COMPONENT_ALARM_DISPLAY,
	E_CAL_COMPONENT_ALARM_AUDIO,
	E_CAL_COMPONENT_ALARM_PROCEDURE,
	E_CAL_COMPONENT_ALARM_EMAIL,
	-1
};

static const gchar *action_map_cap[] = {
	CAL_STATIC_CAPABILITY_NO_DISPLAY_ALARMS,
	CAL_STATIC_CAPABILITY_NO_AUDIO_ALARMS,
        CAL_STATIC_CAPABILITY_NO_PROCEDURE_ALARMS,
	CAL_STATIC_CAPABILITY_NO_EMAIL_ALARMS
};

static const gint value_map[] = {
	MINUTES,
	HOURS,
	DAYS,
	-1
};

static const gint relative_map[] = {
	BEFORE,
	AFTER,
	-1
};

static const gint time_map[] = {
	E_CAL_COMPONENT_ALARM_TRIGGER_RELATIVE_START,
	E_CAL_COMPONENT_ALARM_TRIGGER_RELATIVE_END,
	-1
};

enum duration_units {
	DUR_MINUTES,
	DUR_HOURS,
	DUR_DAYS
};

static const gint duration_units_map[] = {
	DUR_MINUTES,
	DUR_HOURS,
	DUR_DAYS,
	-1
};

static void populate_widgets_from_alarm (Dialog *dialog);
static void action_changed_cb (GtkWidget *action_combo, gpointer data);

/* Fills the widgets with default values */
static void
clear_widgets (Dialog *dialog)
{
	/* Sane defaults */
	e_dialog_combo_box_set (dialog->action_combo, E_CAL_COMPONENT_ALARM_DISPLAY, action_map);
	gtk_spin_button_set_value (
		GTK_SPIN_BUTTON (dialog->interval_value), 15);
	e_dialog_combo_box_set (dialog->value_units_combo, MINUTES, value_map);
	e_dialog_combo_box_set (dialog->relative_combo, BEFORE, relative_map);
	e_dialog_combo_box_set (dialog->time_combo, E_CAL_COMPONENT_ALARM_TRIGGER_RELATIVE_START, time_map);

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
	GtkTreeModel *model;
	GtkTreeIter iter;
	gboolean valid;
	gboolean repeat;
	ECalComponentAlarmAction action;
	gchar *email;
	gint i;

	/* Clean the page */
	clear_widgets (dialog);

	/* Alarm types */
	model = gtk_combo_box_get_model (GTK_COMBO_BOX (dialog->action_combo));
	valid = gtk_tree_model_get_iter_first (model, &iter);
	for (i = 0; valid && action_map[i] != -1; i++) {
		gtk_list_store_set (
			GTK_LIST_STORE (model), &iter,
			1, !e_cal_get_static_capability (dialog->ecal, action_map_cap[i]),
			-1);

		valid = gtk_tree_model_iter_next (model, &iter);
	}

	/* Set a default address if possible */
	if (!e_cal_get_static_capability (dialog->ecal, CAL_STATIC_CAPABILITY_NO_EMAIL_ALARMS)
		&& !e_cal_component_alarm_has_attendees (dialog->alarm)
	    && e_cal_get_alarm_email_address (dialog->ecal, &email, NULL)) {
		ECalComponentAttendee *a;
		GSList attendee_list;

		a = g_new0 (ECalComponentAttendee, 1);
		a->value = email;
		a->cutype = ICAL_CUTYPE_INDIVIDUAL;
		a->status = ICAL_PARTSTAT_NEEDSACTION;
		a->role = ICAL_ROLE_REQPARTICIPANT;
		attendee_list.data = a;
		attendee_list.next = NULL;
		e_cal_component_alarm_set_attendee_list (dialog->alarm, &attendee_list);
		g_free (email);
		g_free (a);
	}

	/* If we can repeat */
	repeat = !e_cal_get_static_capability (dialog->ecal, CAL_STATIC_CAPABILITY_NO_ALARM_REPEAT);
	gtk_widget_set_sensitive (dialog->repeat_toggle, repeat);

	/* if we are editing a exiting alarm */
	e_cal_component_alarm_get_action (dialog->alarm, &action);

	if (action)
		populate_widgets_from_alarm (dialog);
}

static void
alarm_to_repeat_widgets (Dialog *dialog, ECalComponentAlarm *alarm)
{
	ECalComponentAlarmRepeat repeat;

	e_cal_component_alarm_get_repeat (dialog->alarm, &repeat);

	if (repeat.repetitions) {
		gtk_toggle_button_set_active (
			GTK_TOGGLE_BUTTON (dialog->repeat_toggle), TRUE);
		gtk_spin_button_set_value (
			GTK_SPIN_BUTTON (dialog->repeat_quantity),
			repeat.repetitions);
	} else
		return;

	if (repeat.duration.minutes) {
		e_dialog_combo_box_set (dialog->repeat_unit_combo, DUR_MINUTES, duration_units_map);
		gtk_spin_button_set_value (
			GTK_SPIN_BUTTON (dialog->repeat_value),
			repeat.duration.minutes);
	}

	if (repeat.duration.hours) {
		e_dialog_combo_box_set (dialog->repeat_unit_combo, DUR_HOURS, duration_units_map);
		gtk_spin_button_set_value (
			GTK_SPIN_BUTTON (dialog->repeat_value),
			repeat.duration.hours);
	}

	if (repeat.duration.days) {
		e_dialog_combo_box_set (dialog->repeat_unit_combo, DUR_DAYS, duration_units_map);
		gtk_spin_button_set_value (
			GTK_SPIN_BUTTON (dialog->repeat_value),
			repeat.duration.days);
	}
}

static void
repeat_widgets_to_alarm (Dialog *dialog, ECalComponentAlarm *alarm)
{
	ECalComponentAlarmRepeat repeat;

	if (!gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (dialog->repeat_toggle))) {
		repeat.repetitions = 0;

		e_cal_component_alarm_set_repeat (alarm, repeat);
		return;
	}

	repeat.repetitions = gtk_spin_button_get_value_as_int (
		GTK_SPIN_BUTTON (dialog->repeat_quantity));

	memset (&repeat.duration, 0, sizeof (repeat.duration));
	switch (e_dialog_combo_box_get (dialog->repeat_unit_combo, duration_units_map)) {
	case DUR_MINUTES:
		repeat.duration.minutes = gtk_spin_button_get_value_as_int (
			GTK_SPIN_BUTTON (dialog->repeat_value));
		break;

	case DUR_HOURS:
		repeat.duration.hours = gtk_spin_button_get_value_as_int (
			GTK_SPIN_BUTTON (dialog->repeat_value));
		break;

	case DUR_DAYS:
		repeat.duration.days = gtk_spin_button_get_value_as_int (
			GTK_SPIN_BUTTON (dialog->repeat_value));
		break;

	default:
		g_return_if_reached ();
	}

	e_cal_component_alarm_set_repeat (alarm, repeat);

}

/* Fills the audio alarm data with the values from the widgets */
static void
aalarm_widgets_to_alarm (Dialog *dialog, ECalComponentAlarm *alarm)
{
	gchar *url;
	icalattach *attach;

	if (!gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (dialog->aalarm_sound)))
		return;

	url = gtk_file_chooser_get_uri (
		GTK_FILE_CHOOSER (dialog->aalarm_file_chooser));
	attach = icalattach_new_from_url (url ? url : "");
	g_free (url);

	e_cal_component_alarm_set_attach (alarm, attach);
	icalattach_unref (attach);
}

/* Fills the widgets with audio alarm data */
static void
alarm_to_aalarm_widgets (Dialog *dialog, ECalComponentAlarm *alarm)
{
	const gchar *url;
	icalattach *attach;

	e_cal_component_alarm_get_attach (alarm, (&attach));
	url = icalattach_get_url (attach);
	icalattach_unref (attach);

	if (!(url && *url))
		return;

	gtk_toggle_button_set_active (
		GTK_TOGGLE_BUTTON (dialog->aalarm_sound), TRUE);
	gtk_file_chooser_set_uri (
		GTK_FILE_CHOOSER (dialog->aalarm_file_chooser), url);
}

/* Fills the widgets with display alarm data */
static void
alarm_to_dalarm_widgets (Dialog *dialog, ECalComponentAlarm *alarm )
{
	ECalComponentText description;
	GtkTextBuffer *text_buffer;

	e_cal_component_alarm_get_description (alarm, &description);

	if (description.value) {
		gtk_toggle_button_set_active (
			GTK_TOGGLE_BUTTON (dialog->dalarm_message), TRUE);
		text_buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (dialog->dalarm_description));
		gtk_text_buffer_set_text (text_buffer, description.value, -1);
	}
}

/* Fills the display alarm data with the values from the widgets */
static void
dalarm_widgets_to_alarm (Dialog *dialog, ECalComponentAlarm *alarm)
{
	gchar *str;
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
		const gchar *x_name;

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
	gchar *str;
	ECalComponentText description;
	GSList *attendee_list = NULL;
	GtkTextBuffer *text_buffer;
	GtkTextIter text_iter_start, text_iter_end;
	ENameSelectorModel *name_selector_model;
	EDestinationStore *destination_store;
	GList *destinations;
	icalcomponent *icalcomp;
	icalproperty *icalprop;
	GList *l;

	/* Attendees */
	name_selector_model = e_name_selector_peek_model (dialog->name_selector);
	e_name_selector_model_peek_section (name_selector_model, section_name, NULL, &destination_store);
	destinations = e_destination_store_list_destinations (destination_store);

	for (l = destinations; l; l = g_list_next (l)) {
		EDestination *dest;
		ECalComponentAttendee *a;

		dest = l->data;

		a = g_new0 (ECalComponentAttendee, 1);
		a->value = e_destination_get_email (dest);
		a->cn = e_destination_get_name (dest);
		a->cutype = ICAL_CUTYPE_INDIVIDUAL;
		a->status = ICAL_PARTSTAT_NEEDSACTION;
		a->role = ICAL_ROLE_REQPARTICIPANT;

		attendee_list = g_slist_append (attendee_list, a);
	}

	e_cal_component_alarm_set_attendee_list (alarm, attendee_list);

	e_cal_component_free_attendee_list (attendee_list);
	g_list_free (destinations);

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
		const gchar *x_name;

		x_name = icalproperty_get_x_name (icalprop);
		if (!strcmp (x_name, "X-EVOLUTION-NEEDS-DESCRIPTION")) {
			icalcomponent_remove_property (icalcomp, icalprop);
			break;
		}

		icalprop = icalcomponent_get_next_property (icalcomp, ICAL_X_PROPERTY);
	}
}

/* Fills the widgets from mail alarm data */
static void
alarm_to_malarm_widgets (Dialog *dialog, ECalComponentAlarm *alarm )
{
    ENameSelectorModel *name_selector_model;
    EDestinationStore *destination_store;
    ECalComponentText description;
    GtkTextBuffer *text_buffer;
    GSList *attendee_list, *l;
    gint len;

    /* Attendees */
    name_selector_model = e_name_selector_peek_model (dialog->name_selector);
    e_name_selector_model_peek_section (name_selector_model, section_name, NULL, &destination_store);

    e_cal_component_alarm_get_attendee_list (alarm, &attendee_list);
    len = g_slist_length (attendee_list);
    if (len > 0) {
        for (l = attendee_list; l; l = g_slist_next (l)) {
            ECalComponentAttendee *a = l->data;
            EDestination *dest;

            dest = e_destination_new ();
            if (a->cn != NULL && *a->cn)
                e_destination_set_name (dest, a->cn);
            if (a->value != NULL && *a->value) {
                if (!strncasecmp (a->value, "MAILTO:", 7))
                    e_destination_set_email (dest, a->value + 7);
                else
                    e_destination_set_email (dest, a->value);
            }
            e_destination_store_append_destination (destination_store, dest);
            g_object_unref(GTK_OBJECT (dest));
        }
        e_cal_component_free_attendee_list (attendee_list);
    }

    /* Description */
    e_cal_component_alarm_get_description (alarm, &description);
    if (description.value) {
        gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (dialog->malarm_message), TRUE);
        text_buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (dialog->malarm_description));
        gtk_text_buffer_set_text (text_buffer, description.value, -1);
    }
}

/* Fills the widgets from procedure alarm data */
static void
alarm_to_palarm_widgets (Dialog *dialog, ECalComponentAlarm *alarm)
{
	ECalComponentText description;
	const gchar *url;
	icalattach *attach;

	e_cal_component_alarm_get_attach (alarm, (&attach));
	url = icalattach_get_url (attach);
	icalattach_unref (attach);

	if (!(url && *url))
		return;

	e_dialog_editable_set (dialog->palarm_program, url);
	e_cal_component_alarm_get_description (alarm, &description);

	e_dialog_editable_set (dialog->palarm_args, description.value);
}

/* Fills the procedure alarm data with the values from the widgets */
static void
palarm_widgets_to_alarm (Dialog *dialog, ECalComponentAlarm *alarm)
{
	gchar *program;
	icalattach *attach;
	gchar *str;
	ECalComponentText description;
	icalcomponent *icalcomp;
	icalproperty *icalprop;

	program = e_dialog_editable_get (dialog->palarm_program);
	attach = icalattach_new_from_url (program ? program : "");
	g_free (program);

	e_cal_component_alarm_set_attach (alarm, attach);
	icalattach_unref (attach);

	str = e_dialog_editable_get (dialog->palarm_args);

		description.value = str;
		description.altrep = NULL;

		e_cal_component_alarm_set_description (alarm, &description);

	g_free (str);

	/* remove the X-EVOLUTION-NEEDS-DESCRIPTION property, so that
	 * we don't re-set the alarm's description */
	icalcomp = e_cal_component_alarm_get_icalcomponent (alarm);
	icalprop = icalcomponent_get_first_property (icalcomp, ICAL_X_PROPERTY);
	while (icalprop) {
		const gchar *x_name;

		x_name = icalproperty_get_x_name (icalprop);
		if (!strcmp (x_name, "X-EVOLUTION-NEEDS-DESCRIPTION")) {
			icalcomponent_remove_property (icalcomp, icalprop);
			break;
		}

		icalprop = icalcomponent_get_next_property (icalcomp, ICAL_X_PROPERTY);
	}
}

static void
populate_widgets_from_alarm (Dialog *dialog)
{
	ECalComponentAlarmTrigger *trigger;
	ECalComponentAlarmAction *action;

	action = g_new0 (ECalComponentAlarmAction, 1);
	e_cal_component_alarm_get_action (dialog->alarm, action);
	g_return_if_fail ( action != NULL );

	trigger = g_new0 (ECalComponentAlarmTrigger, 1);
	e_cal_component_alarm_get_trigger (dialog->alarm, trigger);
	g_return_if_fail ( trigger != NULL );

	if (*action == E_CAL_COMPONENT_ALARM_NONE)
		return;

	gtk_window_set_title (GTK_WINDOW (dialog->toplevel),_("Edit Alarm"));

	/* Alarm Types */
	switch (trigger->type) {
	case E_CAL_COMPONENT_ALARM_TRIGGER_RELATIVE_START:
		e_dialog_combo_box_set (dialog->time_combo, E_CAL_COMPONENT_ALARM_TRIGGER_RELATIVE_START, time_map);
		break;

	case E_CAL_COMPONENT_ALARM_TRIGGER_RELATIVE_END:
		e_dialog_combo_box_set (dialog->time_combo, E_CAL_COMPONENT_ALARM_TRIGGER_RELATIVE_END, time_map);
		break;
        default:
                g_warning ("%s: Unexpected alarm type (%d)", G_STRLOC, trigger->type);
	}

	switch (trigger->u.rel_duration.is_neg) {
	case 1:
		e_dialog_combo_box_set (dialog->relative_combo, BEFORE, relative_map);
		break;

	case 0:
		e_dialog_combo_box_set (dialog->relative_combo, AFTER, relative_map);
		break;
	}

	if (trigger->u.rel_duration.days) {
		e_dialog_combo_box_set (dialog->value_units_combo, DAYS, value_map);
		gtk_spin_button_set_value (
			GTK_SPIN_BUTTON (dialog->interval_value),
			trigger->u.rel_duration.days);
	} else if (trigger->u.rel_duration.hours) {
		e_dialog_combo_box_set (dialog->value_units_combo, HOURS, value_map);
		gtk_spin_button_set_value (
			GTK_SPIN_BUTTON (dialog->interval_value),
			trigger->u.rel_duration.hours);
	} else if (trigger->u.rel_duration.minutes) {
		e_dialog_combo_box_set (dialog->value_units_combo, MINUTES, value_map);
		gtk_spin_button_set_value (
			GTK_SPIN_BUTTON (dialog->interval_value),
			trigger->u.rel_duration.minutes);
	} else {
		e_dialog_combo_box_set (dialog->value_units_combo, MINUTES, value_map);
		gtk_spin_button_set_value (
			GTK_SPIN_BUTTON (dialog->interval_value), 0);
	}

	/* Repeat options */
	alarm_to_repeat_widgets (dialog, dialog->alarm);

	/* Alarm options */
	e_dialog_combo_box_set (dialog->action_combo, *action, action_map);
	action_changed_cb (dialog->action_combo, dialog);

	switch (*action) {
	case E_CAL_COMPONENT_ALARM_AUDIO:
		alarm_to_aalarm_widgets (dialog, dialog->alarm);
		break;

	case E_CAL_COMPONENT_ALARM_DISPLAY:
		alarm_to_dalarm_widgets (dialog, dialog->alarm);
		break;

	case E_CAL_COMPONENT_ALARM_EMAIL:
		alarm_to_malarm_widgets (dialog, dialog->alarm);
		break;

	case E_CAL_COMPONENT_ALARM_PROCEDURE:
		alarm_to_palarm_widgets (dialog, dialog->alarm);
		break;
        default:
                g_warning ("%s: Unexpected alarm action (%d)", G_STRLOC, *action);
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
	trigger.type = e_dialog_combo_box_get (dialog->time_combo, time_map);
	if (e_dialog_combo_box_get (dialog->relative_combo, relative_map) == BEFORE)
		trigger.u.rel_duration.is_neg = 1;
	else
		trigger.u.rel_duration.is_neg = 0;

	switch (e_dialog_combo_box_get (dialog->value_units_combo, value_map)) {
	case MINUTES:
		trigger.u.rel_duration.minutes =
			gtk_spin_button_get_value_as_int (
			GTK_SPIN_BUTTON (dialog->interval_value));
		break;

	case HOURS:
		trigger.u.rel_duration.hours =
			gtk_spin_button_get_value_as_int (
			GTK_SPIN_BUTTON (dialog->interval_value));
		break;

	case DAYS:
		trigger.u.rel_duration.days =
			gtk_spin_button_get_value_as_int (
			GTK_SPIN_BUTTON (dialog->interval_value));
		break;

	default:
		g_return_if_reached ();
	}
	e_cal_component_alarm_set_trigger (dialog->alarm, trigger);

	action = e_dialog_combo_box_get (dialog->action_combo, action_map);
	e_cal_component_alarm_set_action (dialog->alarm, action);

	/* Repeat stuff */
	repeat_widgets_to_alarm (dialog, dialog->alarm);

	/* Options */
	switch (action) {
	case E_CAL_COMPONENT_ALARM_NONE:
		g_return_if_reached ();
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
		g_return_if_reached ();
	}
}

/* Gets the widgets from the XML file and returns TRUE if they are all available. */
static gboolean
get_widgets (Dialog *dialog)
{
	dialog->toplevel = e_builder_get_widget (dialog->builder, "alarm-dialog");
	if (!dialog->toplevel)
		return FALSE;

	dialog->action_combo = e_builder_get_widget (dialog->builder, "action-combobox");
	dialog->interval_value = e_builder_get_widget (dialog->builder, "interval-value");
	dialog->value_units_combo = e_builder_get_widget (dialog->builder, "value-units-combobox");
	dialog->relative_combo = e_builder_get_widget (dialog->builder, "relative-combobox");
	dialog->time_combo = e_builder_get_widget (dialog->builder, "time-combobox");

	dialog->repeat_toggle = e_builder_get_widget (dialog->builder, "repeat-toggle");
	dialog->repeat_group = e_builder_get_widget (dialog->builder, "repeat-group");
	dialog->repeat_quantity = e_builder_get_widget (dialog->builder, "repeat-quantity");
	dialog->repeat_value = e_builder_get_widget (dialog->builder, "repeat-value");
	dialog->repeat_unit_combo = e_builder_get_widget (dialog->builder, "repeat-unit-combobox");

	dialog->option_notebook = e_builder_get_widget (dialog->builder, "option-notebook");

	dialog->dalarm_group = e_builder_get_widget (dialog->builder, "dalarm-group");
	dialog->dalarm_message = e_builder_get_widget (dialog->builder, "dalarm-message");
	dialog->dalarm_description = e_builder_get_widget (dialog->builder, "dalarm-description");

	dialog->aalarm_group = e_builder_get_widget (dialog->builder, "aalarm-group");
	dialog->aalarm_sound = e_builder_get_widget (dialog->builder, "aalarm-sound");
	dialog->aalarm_file_chooser = e_builder_get_widget (dialog->builder, "aalarm-file-chooser");

	dialog->malarm_group = e_builder_get_widget (dialog->builder, "malarm-group");
	dialog->malarm_address_group = e_builder_get_widget (dialog->builder, "malarm-address-group");
	dialog->malarm_addressbook = e_builder_get_widget (dialog->builder, "malarm-addressbook");
	dialog->malarm_message = e_builder_get_widget (dialog->builder, "malarm-message");
	dialog->malarm_description = e_builder_get_widget (dialog->builder, "malarm-description");

	dialog->palarm_group = e_builder_get_widget (dialog->builder, "palarm-group");
	dialog->palarm_program = e_builder_get_widget (dialog->builder, "palarm-program");
	dialog->palarm_args = e_builder_get_widget (dialog->builder, "palarm-args");

	if (dialog->action_combo) {
		const gchar *actions[] = {
			N_("Pop up an alert"),
			N_("Play a sound"),
			N_("Run a program"),
			N_("Send an email")
		};

		GtkComboBox *combo = (GtkComboBox*)dialog->action_combo;
		GtkCellRenderer *cell;
		GtkListStore *store;
		gint i;

		g_return_val_if_fail (combo != NULL, FALSE);
		g_return_val_if_fail (GTK_IS_COMBO_BOX (combo), FALSE);

		store = gtk_list_store_new (2, G_TYPE_STRING, G_TYPE_BOOLEAN);
		gtk_combo_box_set_model (combo, GTK_TREE_MODEL (store));
		g_object_unref (store);

		gtk_cell_layout_clear (GTK_CELL_LAYOUT (combo));

		cell = gtk_cell_renderer_text_new ();
		gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (combo), cell, TRUE);
		gtk_cell_layout_set_attributes (GTK_CELL_LAYOUT (combo), cell,
				"text", 0,
				"sensitive", 1,
				NULL);

		for (i = 0; i < G_N_ELEMENTS (actions); i++) {
			GtkTreeIter iter;

			gtk_list_store_append (store, &iter);
			gtk_list_store_set (
				store, &iter,
				0, _(actions[i]),
				1, TRUE,
				-1);
		}
	}

	return (dialog->action_combo
		&& dialog->interval_value
		&& dialog->value_units_combo
		&& dialog->relative_combo
		&& dialog->time_combo
		&& dialog->repeat_toggle
		&& dialog->repeat_group
		&& dialog->repeat_quantity
		&& dialog->repeat_value
		&& dialog->repeat_unit_combo
		&& dialog->option_notebook
		&& dialog->dalarm_group
		&& dialog->dalarm_message
		&& dialog->dalarm_description
		&& dialog->aalarm_group
		&& dialog->aalarm_sound
		&& dialog->aalarm_file_chooser
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
	gchar *email;

	e_cal_component_alarm_set_action (dialog->alarm,
					e_dialog_combo_box_get (dialog->action_combo, action_map));

	repeat = !e_cal_get_static_capability (dialog->ecal, CAL_STATIC_CAPABILITY_NO_ALARM_REPEAT);

	if (e_cal_get_static_capability (dialog->ecal, CAL_STATIC_CAPABILITY_NO_EMAIL_ALARMS)
	    || e_cal_get_alarm_email_address (dialog->ecal, &email, NULL)) {
		if (!alarm_options_dialog_run (dialog->toplevel, dialog->alarm, email, repeat))
			g_message (G_STRLOC ": not create the alarm options dialog");
	}
}
#endif

static void
addressbook_clicked_cb (GtkWidget *widget, Dialog *dialog)
{
	e_name_selector_show_dialog (dialog->name_selector, dialog->toplevel);
}

static void
addressbook_response_cb (GtkWidget *widget, gint response, gpointer data)
{
	Dialog *dialog = data;
	ENameSelectorDialog *name_selector_dialog;

	name_selector_dialog = e_name_selector_peek_dialog (dialog->name_selector);
	gtk_widget_hide (GTK_WIDGET (name_selector_dialog));
}

static gboolean
setup_select_names (Dialog *dialog)
{
	ENameSelectorModel *name_selector_model;
	ENameSelectorDialog *name_selector_dialog;

	dialog->name_selector = e_name_selector_new ();
	name_selector_model = e_name_selector_peek_model (dialog->name_selector);

	e_name_selector_model_add_section (name_selector_model, section_name, section_name, NULL);

	dialog->malarm_addresses =
		GTK_WIDGET (e_name_selector_peek_section_entry (dialog->name_selector, section_name));
	gtk_widget_show (dialog->malarm_addresses);
	gtk_box_pack_end (GTK_BOX (dialog->malarm_address_group), dialog->malarm_addresses, TRUE, TRUE, 0);

	g_signal_connect (G_OBJECT (dialog->malarm_addressbook), "clicked",
			  G_CALLBACK (addressbook_clicked_cb), dialog);

	name_selector_dialog = e_name_selector_peek_dialog (dialog->name_selector);
	g_signal_connect (name_selector_dialog, "response",
			  G_CALLBACK (addressbook_response_cb), dialog);

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
	gchar *str, *dir;
	gboolean sens;

	str = gtk_file_chooser_get_filename (
		GTK_FILE_CHOOSER (dialog->aalarm_file_chooser));

	if (str && *str) {
		dir = g_path_get_dirname (str);
		if (dir && *dir) {
			calendar_config_set_dir_path (dir);
		}
	}

	sens = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (dialog->aalarm_sound)) ? str && *str : TRUE;
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
	gchar *str;
	GtkTextBuffer *text_buffer;
	GtkTextIter text_iter_start, text_iter_end;
	gboolean sens;

	text_buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (dialog->dalarm_description));
	gtk_text_buffer_get_start_iter (text_buffer, &text_iter_start);
	gtk_text_buffer_get_end_iter   (text_buffer, &text_iter_end);
	str = gtk_text_buffer_get_text (text_buffer, &text_iter_start, &text_iter_end, FALSE);

	sens = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (dialog->dalarm_message)) ? str && *str : TRUE;
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
	gchar *str;
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
	gchar *str;
	GtkTextBuffer *text_buffer;
	GtkTextIter text_iter_start, text_iter_end;
	ENameSelectorModel *name_selector_model;
	EDestinationStore *destination_store;
	GList *destinations;
	gboolean sens;

	name_selector_model = e_name_selector_peek_model (dialog->name_selector);
	e_name_selector_model_peek_section (name_selector_model, section_name, NULL, &destination_store);
	destinations = e_destination_store_list_destinations (destination_store);

	text_buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (dialog->malarm_description));
	gtk_text_buffer_get_start_iter (text_buffer, &text_iter_start);
	gtk_text_buffer_get_end_iter   (text_buffer, &text_iter_end);
	str = gtk_text_buffer_get_text (text_buffer, &text_iter_start, &text_iter_end, FALSE);

	sens = (destinations != NULL) && (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (dialog->malarm_message)) ? str && *str : TRUE);
	gtk_dialog_set_response_sensitive (GTK_DIALOG (dialog->toplevel), GTK_RESPONSE_OK, sens);

	g_list_free (destinations);
}

static void
malarm_addresses_changed_cb  (GtkWidget *editable,
			      gpointer   data)
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
action_changed_cb (GtkWidget *action_combo, gpointer data)
{
	Dialog *dialog = data;
	gchar *dir;
	ECalComponentAlarmAction action;
	gint page = 0, i;

	action = e_dialog_combo_box_get (dialog->action_combo, action_map);
	for (i = 0; action_map[i] != -1; i++) {
		if (action == action_map[i]) {
			page = i;
			break;
		}
	}

	gtk_notebook_set_current_page (
		GTK_NOTEBOOK (dialog->option_notebook), page);

	switch (action) {
	case E_CAL_COMPONENT_ALARM_AUDIO:
		dir = calendar_config_get_dir_path ();
		if (dir && *dir)
			gtk_file_chooser_set_current_folder (
				GTK_FILE_CHOOSER (dialog->aalarm_file_chooser),
				dir);
		g_free (dir);
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
		g_return_if_reached ();
		return;
	}
}

/* Hooks the widget signals */
static void
init_widgets (Dialog *dialog)
{
	GtkTextBuffer *text_buffer;

	g_signal_connect (dialog->action_combo, "changed",
			  G_CALLBACK (action_changed_cb),
			  dialog);

	g_signal_connect (G_OBJECT (dialog->repeat_toggle), "toggled",
			  G_CALLBACK (repeat_toggle_toggled_cb), dialog);

	/* Handle custom sounds */
	g_signal_connect (G_OBJECT (dialog->aalarm_sound), "toggled",
			  G_CALLBACK (aalarm_sound_toggled_cb), dialog);
	g_signal_connect (G_OBJECT (dialog->aalarm_file_chooser), "selection-changed",
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

	g_signal_connect (dialog->malarm_addresses, "changed",
			  G_CALLBACK (malarm_addresses_changed_cb), dialog);
}

gboolean
alarm_dialog_run (GtkWidget *parent, ECal *ecal, ECalComponentAlarm *alarm)
{
	Dialog dialog;
	GtkWidget *container;
	gint response_id;

	g_return_val_if_fail (alarm != NULL, FALSE);

	dialog.alarm = alarm;
	dialog.ecal = ecal;

	dialog.builder = gtk_builder_new ();
	e_load_ui_builder_definition (dialog.builder, "alarm-dialog.ui");

	if (!get_widgets (&dialog)) {
		g_object_unref(dialog.builder);
		return FALSE;
	}

	if (!setup_select_names (&dialog)) {
		g_object_unref (dialog.builder);
		return FALSE;
	}

	init_widgets (&dialog);

	alarm_to_dialog (&dialog);

	gtk_widget_ensure_style (dialog.toplevel);

	container = gtk_dialog_get_action_area (GTK_DIALOG (dialog.toplevel));
	gtk_container_set_border_width (GTK_CONTAINER (container), 12);

	container = gtk_dialog_get_content_area (GTK_DIALOG (dialog.toplevel));
	gtk_container_set_border_width (GTK_CONTAINER (container), 0);

	gtk_window_set_icon_name (
		GTK_WINDOW (dialog.toplevel), "x-office-calendar");

	gtk_window_set_transient_for (GTK_WINDOW (dialog.toplevel),
				      GTK_WINDOW (parent));

	response_id = gtk_dialog_run (GTK_DIALOG (dialog.toplevel));

	if (response_id == GTK_RESPONSE_OK)
		dialog_to_alarm (&dialog);

	gtk_widget_destroy (dialog.toplevel);
	g_object_unref (dialog.builder);

	return response_id == GTK_RESPONSE_OK ? TRUE : FALSE;
}
