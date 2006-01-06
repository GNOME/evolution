/* Evolution calendar - Main page of the event editor dialog
 *
 * Copyright (C) 2001 Ximian, Inc.
 *
 * Authors: Federico Mena-Quintero <federico@ximian.com>
 *          Miguel de Icaza <miguel@ximian.com>
 *          Seth Alves <alves@hungry.com>
 *          JP Rosevear <jpr@ximian.com>
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
#include <gtk/gtksignal.h>
#include <gtk/gtktextview.h>
#include <gtk/gtktogglebutton.h>
#include <gtk/gtkmessagedialog.h>
#include <gdk/gdkkeysyms.h>
#include <libgnome/gnome-i18n.h>
#include <glade/glade.h>
#include <libedataserverui/e-source-option-menu.h>
#include "common/authentication.h"
#include "e-util/e-categories-config.h"
#include "e-util/e-dialog-widgets.h"
#include "e-util/e-popup.h"
#include "misc/e-dateedit.h"
#include "misc/e-send-options.h"
#include <libecal/e-cal-time-util.h>
#include "../calendar-config.h"
#include "../e-timezone-entry.h"
#include <e-util/e-dialog-utils.h>
#include <e-util/e-dialog-widgets.h>
#include <e-util/e-icon-factory.h>
#include <e-util/e-util-private.h>

#include "../e-meeting-attendee.h"
#include "../e-meeting-store.h"
#include "../e-meeting-list-view.h"
#include "../e-cal-popup.h"
#include "comp-editor.h"
#include "comp-editor-util.h"
#include "../e-alarm-list.h"
#include "alarm-list-dialog.h"
#include "event-page.h"
#include "e-send-options-utils.h"



/* Private part of the EventPage structure */
struct _EventPagePrivate {
	/* Glade XML data */
	GladeXML *xml;
	/* Widgets from the Glade file */

	GtkWidget *main;
	BonoboUIComponent *uic;

	GtkWidget *summary;
	GtkWidget *summary_label;
	GtkWidget *location;
	GtkWidget *location_label;

	EAccountList *accounts;
	EMeetingAttendee *ia;	
	char *default_address;
	char *user_add;
	ECalComponent *comp;

	/* For meeting/event */
	GtkWidget *calendar_label;
	GtkWidget *org_cal_label;
	GtkWidget *attendee_box;

	/* Lists of attendees */
	GPtrArray *deleted_attendees;
	
	GtkWidget *start_time;
	GtkWidget *end_time;
	GtkWidget *end_time_selector;
	GtkWidget *time_hour;
	GtkWidget *hour_selector;
	GtkWidget *minute_selector;
	GtkWidget *start_timezone;
	GtkWidget *end_timezone;
	GtkWidget *timezone_label;
	gboolean   all_day_event;
	GtkWidget *status_icons;
	GtkWidget *alarm_icon;
	GtkWidget *recur_icon;

	GtkWidget *description;

	ECalComponentClassification classification;
	
	gboolean  show_time_as_busy;

	GtkWidget *alarm_dialog;
	GtkWidget *alarm;
	GtkWidget *alarm_time;
	GtkWidget *alarm_warning;
	GtkWidget *alarm_custom;

	GtkWidget *categories_btn;
	GtkWidget *categories;

	GtkWidget *source_selector;
	
	/* Meeting related items */
	GtkWidget *list_box;
	GtkWidget *organizer_table;
	GtkWidget *organizer;
	GtkWidget *add;
	GtkWidget *remove;
	GtkWidget *edit;
	GtkWidget *invite;
	GtkWidget *attendees_label;

	/* ListView stuff */
	EMeetingStore *model;
	ECal	  *client;
	EMeetingListView *list_view;
	gint row;

	/* For handling who the organizer is */
	gboolean user_org;
	gboolean existing;
        gboolean updating;

	EAlarmList *alarm_list_store;
	
	gboolean sendoptions_shown;

	ESendOptionsDialog *sod;
	char *old_summary;
	CalUnits alarm_units;
	int alarm_interval;
	
	/* This is TRUE if both the start & end timezone are the same. If the
	   start timezone is then changed, we updated the end timezone to the
	   same value, since 99% of events start and end in one timezone. */
	gboolean sync_timezones;
	gboolean is_meeting;
};



static void event_page_finalize (GObject *object);

static GtkWidget *event_page_get_widget (CompEditorPage *page);
static void event_page_focus_main_widget (CompEditorPage *page);
static gboolean event_page_fill_widgets (CompEditorPage *page, ECalComponent *comp);
static gboolean event_page_fill_component (CompEditorPage *page, ECalComponent *comp);
static gboolean event_page_fill_timezones (CompEditorPage *page, GHashTable *timezones);
static void event_page_set_summary (CompEditorPage *page, const char *summary);
static void event_page_set_dates (CompEditorPage *page, CompEditorPageDates *dates);
static void notify_dates_changed (EventPage *epage, struct icaltimetype *start_tt, struct icaltimetype *end_tt);
static gboolean check_start_before_end (struct icaltimetype *start_tt, icaltimezone *start_zone, 
					struct icaltimetype *end_tt, icaltimezone *end_zone, gboolean adjust_end_time);
static void set_attendees (ECalComponent *comp, const GPtrArray *attendees);
static void hour_sel_changed ( GtkSpinButton *widget, EventPage *epage);
static void minute_sel_changed ( GtkSpinButton *widget, EventPage *epage);
static void hour_minute_changed ( EventPage *epage);
static void update_end_time_selector( EventPage *epage);
G_DEFINE_TYPE (EventPage, event_page, TYPE_COMP_EDITOR_PAGE);

/* Class initialization function for the event page */
static void
event_page_class_init (EventPageClass *class)
{
	CompEditorPageClass *editor_page_class;
	GObjectClass *object_class;

	editor_page_class = (CompEditorPageClass *) class;
	object_class = (GObjectClass *) class;

	editor_page_class->get_widget = event_page_get_widget;
	editor_page_class->focus_main_widget = event_page_focus_main_widget;
	editor_page_class->fill_widgets = event_page_fill_widgets;
	editor_page_class->fill_component = event_page_fill_component;
	editor_page_class->fill_timezones = event_page_fill_timezones;
	editor_page_class->set_summary = event_page_set_summary;
	editor_page_class->set_dates = event_page_set_dates;

	object_class->finalize = event_page_finalize;
}

/* Object initialization function for the event page */
static void
event_page_init (EventPage *epage)
{
	EventPagePrivate *priv;

	priv = g_new0 (EventPagePrivate, 1);
	epage->priv = priv;

	priv->xml = NULL;
	priv->uic = NULL;

	priv->main = NULL;
	priv->summary = NULL;
	priv->summary_label = NULL;
	priv->location = NULL;
	priv->location_label = NULL;
	priv->start_time = NULL;
	priv->end_time = NULL;
	priv->start_timezone = NULL;
	priv->end_timezone = NULL;
	priv->timezone_label = NULL;
	priv->all_day_event = FALSE;
	priv->status_icons = NULL;
	priv->alarm_icon = NULL;
	priv->recur_icon = NULL;
	priv->description = NULL;
	priv->classification = E_CAL_COMPONENT_CLASS_NONE;
	priv->show_time_as_busy = FALSE;
	priv->alarm_dialog = NULL;
	priv->alarm = NULL;
	priv->alarm_time = NULL;
	priv->alarm_custom = NULL;
	priv->categories_btn = NULL;
	priv->categories = NULL;
	priv->sod = NULL;

	priv->deleted_attendees = g_ptr_array_new ();

	priv->comp = NULL;

	priv->accounts = NULL;
	priv->ia = NULL;
	priv->default_address = NULL;
	priv->invite = NULL;
	
	priv->model = NULL;
	priv->list_view = NULL;
	
	priv->updating = FALSE;
	
	priv->alarm_interval =  -1;
	
	priv->sendoptions_shown = FALSE;
	priv->is_meeting = FALSE;
	priv->sync_timezones = FALSE;

	priv->default_address = NULL;
}

static void
cleanup_attendees (GPtrArray *attendees)
{
	int i;
	
	for (i = 0; i < attendees->len; i++)
		g_object_unref (g_ptr_array_index (attendees, i));
}

/* Destroy handler for the event page */
static void
event_page_finalize (GObject *object)
{
	EventPage *epage;
	EventPagePrivate *priv;

	g_return_if_fail (object != NULL);
	g_return_if_fail (IS_EVENT_PAGE (object));

	epage = EVENT_PAGE (object);
	priv = epage->priv;
	
	if (priv->comp != NULL)
		g_object_unref (priv->comp);

	cleanup_attendees (priv->deleted_attendees);
	g_ptr_array_free (priv->deleted_attendees, TRUE);
	
	if (priv->main)
		gtk_widget_unref (priv->main);

	if (priv->xml) {
		g_object_unref (priv->xml);
		priv->xml = NULL;
	}

	if (priv->alarm_list_store) {
		g_object_unref (priv->alarm_list_store);
		priv->alarm_list_store = NULL;
	}

	if (priv->sod) {
		g_object_unref (priv->sod);
		priv->sod = NULL;
	}
	g_free (priv->old_summary);
	
	g_free (priv);
	epage->priv = NULL;

	if (G_OBJECT_CLASS (event_page_parent_class)->finalize)
		(* G_OBJECT_CLASS (event_page_parent_class)->finalize) (object);
}



static const int classification_map[] = {
	E_CAL_COMPONENT_CLASS_PUBLIC,
	E_CAL_COMPONENT_CLASS_PRIVATE,
	E_CAL_COMPONENT_CLASS_CONFIDENTIAL,
	-1
};

enum {
	ALARM_15_MINUTES,
	ALARM_1_HOUR,
	ALARM_1_DAY,
	ALARM_USER_TIME
};

static const int alarm_map[] = {
	ALARM_15_MINUTES,
	ALARM_1_HOUR,
	ALARM_1_DAY,
	ALARM_USER_TIME,
	-1
};

static void
set_classification_menu (EventPage *epage, gint class)
{
	bonobo_ui_component_freeze (epage->priv->uic, NULL);
	switch (class) {
		case E_CAL_COMPONENT_CLASS_PUBLIC:
			bonobo_ui_component_set_prop (
				epage->priv->uic, "/commands/ActionClassPublic",
				"state", "1", NULL);
			break;
		case E_CAL_COMPONENT_CLASS_CONFIDENTIAL:
			bonobo_ui_component_set_prop (
				epage->priv->uic, "/commands/ActionClassConfidential",
				"state", "1", NULL);
			break;
		case E_CAL_COMPONENT_CLASS_PRIVATE:
			bonobo_ui_component_set_prop (
				epage->priv->uic, "/commands/ActionClassPrivate",
				"state", "1", NULL);
			break;
	}
	bonobo_ui_component_thaw (epage->priv->uic, NULL);
}

static void
set_busy_time_menu (EventPage *epage, gboolean status)
{
	bonobo_ui_component_set_prop (
		epage->priv->uic, "/commands/ActionShowTimeBusy",
		"state", status ? "1" : "0", NULL);
}

static void
enable_busy_time_menu (EventPage *epage, gboolean state)
{
	bonobo_ui_component_set_prop (
		epage->priv->uic, "/commands/ActionShowTimeBusy",
		"sensitive", state ? "1" : "0", NULL);
}

static void
set_all_day_event_menu (EventPage *epage, gboolean status)
{
	bonobo_ui_component_set_prop (
		epage->priv->uic, "/commands/ActionAllDayEvent",
		"state", status ? "1" : "0", NULL);
}

/* get_widget handler for the event page */
static GtkWidget *
event_page_get_widget (CompEditorPage *page)
{
	EventPage *epage;
	EventPagePrivate *priv;

	epage = EVENT_PAGE (page);
	priv = epage->priv;

	return priv->main;
}

/* focus_main_widget handler for the event page */
static void
event_page_focus_main_widget (CompEditorPage *page)
{
	EventPage *epage;
	EventPagePrivate *priv;

	epage = EVENT_PAGE (page);
	priv = epage->priv;

	gtk_widget_grab_focus (priv->summary);
}

/* Sets the 'All Day Event' flag to the given value (without emitting signals),
 * and shows or hides the widgets as appropriate. */
static void
set_all_day (EventPage *epage, gboolean all_day)
{
	EventPagePrivate *priv;

	priv = epage->priv;

	set_all_day_event_menu (epage, all_day);
	
	/* TODO implement for in end time selector */
	if (all_day)
		gtk_option_menu_set_history (GTK_OPTION_MENU (priv->end_time_selector), 1);
	gtk_widget_set_sensitive (priv->end_time_selector, !all_day);

	e_date_edit_set_show_time (E_DATE_EDIT (priv->start_time), !all_day);
	e_date_edit_set_show_time (E_DATE_EDIT (priv->end_time), !all_day);

}

static void
update_time (EventPage *epage, ECalComponentDateTime *start_date, ECalComponentDateTime *end_date)
{
	EventPagePrivate *priv;
	struct icaltimetype *start_tt, *end_tt, implied_tt;
	icaltimezone *start_zone = NULL;
	gboolean all_day_event;

	priv = epage->priv;

	/* Note that if we are creating a new event, the timezones may not be
	   on the server, so we try to get the builtin timezone with the TZID
	   first. */
	start_zone = icaltimezone_get_builtin_timezone_from_tzid (start_date->tzid);
	if (!start_zone) {
		/* FIXME: Handle error better. */
		if (!e_cal_get_timezone (COMP_EDITOR_PAGE (epage)->client,
					      start_date->tzid, &start_zone, NULL)) {
			g_warning ("Couldn't get timezone from server: %s",
				   start_date->tzid ? start_date->tzid : "");			
		}
	}

	/* If both times are DATE values, we set the 'All Day Event' checkbox.
	   Also, if DTEND is after DTSTART, we subtract 1 day from it. */
	all_day_event = FALSE;
	start_tt = start_date->value;
	end_tt = end_date->value;
	if (!end_tt && start_tt->is_date) {
		end_tt = &implied_tt;
		*end_tt = *start_tt;
		icaltime_adjust (end_tt, 1, 0, 0, 0);
	}
	
	if (start_tt->is_date && end_tt->is_date) {
		all_day_event = TRUE;
		if (icaltime_compare_date_only (*end_tt, *start_tt) > 0) {
			icaltime_adjust (end_tt, -1, 0, 0, 0);
		}
	}

	epage->priv->all_day_event = all_day_event;
	set_all_day (epage, all_day_event);

	/* If it is an all day event, we set both timezones to the current
	   timezone, so that if the user toggles the 'All Day Event' checkbox
	   the event uses the current timezone rather than none at all. */
	if (all_day_event)
		start_zone = calendar_config_get_icaltimezone ();

	gtk_signal_handler_block_by_data (GTK_OBJECT (priv->start_time),
					  epage);
	g_signal_handlers_block_matched (priv->end_time, G_SIGNAL_MATCH_DATA, 0, 0, NULL, NULL, epage);

	e_date_edit_set_date (E_DATE_EDIT (priv->start_time), start_tt->year,
			      start_tt->month, start_tt->day);
	e_date_edit_set_time_of_day (E_DATE_EDIT (priv->start_time),
				     start_tt->hour, start_tt->minute);

	e_date_edit_set_date (E_DATE_EDIT (priv->end_time), end_tt->year,
			      end_tt->month, end_tt->day);
	e_date_edit_set_time_of_day (E_DATE_EDIT (priv->end_time),
				     end_tt->hour, end_tt->minute);

	gtk_signal_handler_unblock_by_data (GTK_OBJECT (priv->start_time),
					    epage);
	gtk_signal_handler_unblock_by_data (GTK_OBJECT (priv->end_time),
					    epage);

	/* Set the timezones, and set sync_timezones to TRUE if both timezones
	   are the same. */
	gtk_signal_handler_block_by_data (GTK_OBJECT (priv->start_timezone),
					  epage);
	g_signal_handlers_block_matched (priv->end_timezone, G_SIGNAL_MATCH_DATA, 0, 0, NULL, NULL, epage);
	
	e_timezone_entry_set_timezone (E_TIMEZONE_ENTRY (priv->start_timezone),
				       start_zone);
	event_page_set_show_timezone (epage, calendar_config_get_show_timezone() & !all_day_event);

	gtk_signal_handler_unblock_by_data (GTK_OBJECT (priv->start_timezone),
					    epage);

	priv->sync_timezones = TRUE; 
}

/* Fills the widgets with default values */
static void
clear_widgets (EventPage *epage)
{
	EventPagePrivate *priv;

	priv = epage->priv;

	/* Summary, description */
	e_dialog_editable_set (priv->summary, NULL);
	e_dialog_editable_set (priv->location, NULL);
	gtk_text_buffer_set_text (gtk_text_view_get_buffer (GTK_TEXT_VIEW (priv->description)), "", 0);

	/* Start and end times */
	gtk_signal_handler_block_by_data (GTK_OBJECT (priv->start_time),
					  epage);
	g_signal_handlers_block_matched (priv->end_time, G_SIGNAL_MATCH_DATA, 0, 0, NULL, NULL, epage);

	e_date_edit_set_time (E_DATE_EDIT (priv->start_time), 0);
	e_date_edit_set_time (E_DATE_EDIT (priv->end_time), 0);

	gtk_signal_handler_unblock_by_data (GTK_OBJECT (priv->start_time),
					    epage);
	gtk_signal_handler_unblock_by_data (GTK_OBJECT (priv->end_time),
					    epage);

	epage->priv->all_day_event = FALSE;
	set_all_day (epage, FALSE);

	/* Classification */
	priv->classification = E_CAL_COMPONENT_CLASS_PRIVATE;
	set_classification_menu (epage, priv->classification);

	/* Show Time As (Transparency) */
	priv->show_time_as_busy = TRUE;
	set_busy_time_menu (epage, TRUE);

	/* Alarm */
	e_dialog_toggle_set (priv->alarm, FALSE);
	e_dialog_option_menu_set (priv->alarm_time, ALARM_15_MINUTES, alarm_map);
	
	/* Categories */
	e_dialog_editable_set (priv->categories, NULL);

	gtk_entry_set_text (GTK_ENTRY (GTK_COMBO (priv->organizer)->entry), priv->default_address);
	
}

static gboolean
is_custom_alarm (ECalComponentAlarm *ca, char *old_summary, CalUnits user_units, int user_interval, int *alarm_type) 
{
	ECalComponentAlarmTrigger trigger;
	ECalComponentAlarmRepeat repeat;
	ECalComponentAlarmAction action;
	ECalComponentText desc;
	icalcomponent *icalcomp;
	icalproperty *icalprop;
	icalattach *attach;
	gboolean needs_desc = FALSE;
	
	e_cal_component_alarm_get_action (ca, &action);
	if (action != E_CAL_COMPONENT_ALARM_DISPLAY)
		return TRUE;

	e_cal_component_alarm_get_attach (ca, &attach);
	if (attach)
		return TRUE;

	icalcomp = e_cal_component_alarm_get_icalcomponent (ca);
	icalprop = icalcomponent_get_first_property (icalcomp, ICAL_X_PROPERTY);
	while (icalprop) {
		const char *x_name;

		x_name = icalproperty_get_x_name (icalprop);
		if (!strcmp (x_name, "X-EVOLUTION-NEEDS-DESCRIPTION"))
			needs_desc = TRUE;

		icalprop = icalcomponent_get_next_property (icalcomp, ICAL_X_PROPERTY);
	}

	if (!needs_desc) {	
		e_cal_component_alarm_get_description (ca, &desc);
		if (!desc.value || !old_summary || strcmp (desc.value, old_summary))
			return TRUE;
	}

	e_cal_component_alarm_get_repeat (ca, &repeat);
	if (repeat.repetitions != 0)
		return TRUE;
	
	if (e_cal_component_alarm_has_attendees (ca))
		return TRUE;
	
	e_cal_component_alarm_get_trigger (ca, &trigger);
	if (trigger.type != E_CAL_COMPONENT_ALARM_TRIGGER_RELATIVE_START)
		return TRUE;
	
	if (trigger.u.rel_duration.is_neg != 1)
		return TRUE;

	if (trigger.u.rel_duration.weeks != 0)
		return TRUE;

	if (trigger.u.rel_duration.seconds != 0)
		return TRUE;

	if (trigger.u.rel_duration.days == 1
	    && trigger.u.rel_duration.hours == 0
	    && trigger.u.rel_duration.minutes == 0) {
		if (alarm_type)
			*alarm_type = ALARM_1_DAY;
		return FALSE;
	}
	
	if (trigger.u.rel_duration.days == 0
	    && trigger.u.rel_duration.hours == 1
	    && trigger.u.rel_duration.minutes == 0) {
		if (alarm_type)
			*alarm_type = ALARM_1_HOUR;
		return FALSE;
	}
	
	if (trigger.u.rel_duration.days == 0
	    && trigger.u.rel_duration.hours == 0
	    && trigger.u.rel_duration.minutes == 15) {
		if (alarm_type)
			*alarm_type = ALARM_15_MINUTES;		
		return FALSE;
	}

	if (user_interval != -1) {
		switch (user_units) {
		case CAL_DAYS:		
			if (trigger.u.rel_duration.days == user_interval
			    && trigger.u.rel_duration.hours == 0
			    && trigger.u.rel_duration.minutes == 0) {
				if (alarm_type)
					*alarm_type = ALARM_USER_TIME;		
				return FALSE;
			}
			break;
			
		case CAL_HOURS:
			if (trigger.u.rel_duration.days == 0
			    && trigger.u.rel_duration.hours == user_interval
			    && trigger.u.rel_duration.minutes == 0) {
				if (alarm_type)
					*alarm_type = ALARM_USER_TIME;		
				return FALSE;
			}
			break;
			
		case CAL_MINUTES:
			if (trigger.u.rel_duration.days == 0
			    && trigger.u.rel_duration.hours == 0
			    && trigger.u.rel_duration.minutes == user_interval) {
				if (alarm_type)
					*alarm_type = ALARM_USER_TIME;		
				return FALSE;
			}
			break;
		}		
	}

	return TRUE;
}

static gboolean
is_custom_alarm_uid_list (ECalComponent *comp, GList *alarms, char *old_summary, CalUnits user_units, int user_interval, int *alarm_type)
{
	ECalComponentAlarm *ca;
	gboolean result;
	
	if (g_list_length (alarms) > 1)
		return TRUE;

	ca = e_cal_component_get_alarm (comp, alarms->data);
	result = is_custom_alarm (ca, old_summary, user_units, user_interval, alarm_type);
	e_cal_component_alarm_free (ca);

	return result;
}

static gboolean
is_custom_alarm_store (EAlarmList *alarm_list_store, char *old_summary,  CalUnits user_units, int user_interval, int *alarm_type) 
{
	const ECalComponentAlarm *alarm;
	GtkTreeModel *model;
	GtkTreeIter iter;
	gboolean valid_iter;
	
	model = GTK_TREE_MODEL (alarm_list_store);

	valid_iter = gtk_tree_model_get_iter_first (model, &iter);
	if (!valid_iter)
		return FALSE;

	alarm = e_alarm_list_get_alarm (alarm_list_store, &iter);
	if (is_custom_alarm ((ECalComponentAlarm *)alarm, old_summary, user_units, user_interval, alarm_type))
		return TRUE;

	valid_iter = gtk_tree_model_iter_next (model, &iter);
	if (valid_iter)
		return TRUE;
	
	return FALSE;
}

void
event_page_set_view_role (EventPage *epage, gboolean state)
{
	EventPagePrivate *priv = epage->priv;

	e_meeting_list_view_column_set_visible (priv->list_view, "Role", state);
}

void
event_page_set_view_status (EventPage *epage, gboolean state)
{
	EventPagePrivate *priv = epage->priv;

	e_meeting_list_view_column_set_visible (priv->list_view, "Status", state);
}

void
event_page_set_view_type (EventPage *epage, gboolean state)
{
	EventPagePrivate *priv = epage->priv;

	e_meeting_list_view_column_set_visible (priv->list_view, "Type", state);
}

void
event_page_set_view_rsvp (EventPage *epage, gboolean state)
{
	EventPagePrivate *priv = epage->priv;

	e_meeting_list_view_column_set_visible (priv->list_view, "RSVP", state);
}

void 
event_page_set_classification (EventPage *epage, ECalComponentClassification class)
{
	epage->priv->classification = class;
}

static GtkWidget *
create_image_event_box (const char *image_text, const char *tip_text)
{
	GtkWidget *image, *box;
	GtkTooltips *tip;
	
	box = gtk_event_box_new ();
	tip = gtk_tooltips_new ();
	image = e_icon_factory_get_image (image_text, E_ICON_SIZE_MENU);

	gtk_container_add ((GtkContainer *) box, image);
	gtk_widget_show_all (box);
	gtk_tooltips_set_tip (tip, box, tip_text, NULL);

	return box;	
}

static void
sensitize_widgets (EventPage *epage)
{
	gboolean read_only, custom, alarm, sens = TRUE, sensitize;
	EventPagePrivate *priv;
	gboolean delegate;	
	
	priv = epage->priv;
	if (COMP_EDITOR_PAGE (epage)->flags & COMP_EDITOR_MEETING)
	 	sens = COMP_EDITOR_PAGE (epage)->flags & COMP_EDITOR_PAGE_USER_ORG;

	if (!e_cal_is_read_only (COMP_EDITOR_PAGE (epage)->client, &read_only, NULL))
		read_only = TRUE;
	
	delegate = COMP_EDITOR_PAGE (epage)->flags & COMP_EDITOR_PAGE_DELEGATE;
	
	sensitize = !read_only && sens;

	custom = is_custom_alarm_store (priv->alarm_list_store, priv->old_summary, priv->alarm_units, priv->alarm_interval, NULL);
	alarm = e_dialog_toggle_get (priv->alarm);

	if (alarm && !priv->alarm_icon) {
		priv->alarm_icon = create_image_event_box ("stock_bell", "This event has alarms");
		gtk_box_pack_start ((GtkBox *)priv->status_icons, priv->alarm_icon, FALSE, FALSE, 3);
	}
	
	gtk_entry_set_editable (GTK_ENTRY (priv->summary), sensitize);
	gtk_entry_set_editable (GTK_ENTRY (priv->location), sensitize);
	gtk_widget_set_sensitive (priv->start_time, sensitize);
	gtk_widget_set_sensitive (priv->start_timezone, sensitize);
	gtk_widget_set_sensitive (priv->end_time, sensitize);
	gtk_widget_set_sensitive (priv->end_timezone, sensitize);
	gtk_widget_set_sensitive (priv->description, sensitize);
	gtk_widget_set_sensitive (priv->alarm, !read_only);
	gtk_widget_set_sensitive (priv->alarm_time, !read_only && !custom && alarm);
	gtk_widget_set_sensitive (priv->alarm_custom, alarm);
	gtk_widget_set_sensitive (priv->categories_btn, sensitize);
	/*TODO implement the for portion of the end time selector */
	if ( (COMP_EDITOR_PAGE(epage)->flags) & COMP_EDITOR_PAGE_NEW_ITEM ) {
		if (priv->all_day_event)
			gtk_option_menu_set_history (GTK_OPTION_MENU (priv->end_time_selector), 1);
		else 
			gtk_option_menu_set_history (GTK_OPTION_MENU (priv->end_time_selector), 0);
        } else 
		gtk_option_menu_set_history (GTK_OPTION_MENU (priv->end_time_selector), 1);


	gtk_entry_set_editable (GTK_ENTRY (priv->categories), sensitize);

	if (delegate) {
		gtk_widget_set_sensitive (priv->source_selector, FALSE);
	}

	gtk_widget_set_sensitive (priv->organizer, !read_only);
	gtk_widget_set_sensitive (priv->add, (!read_only &&  sens) || delegate);
	gtk_widget_set_sensitive (priv->remove, (!read_only &&  sens) || delegate);
	gtk_widget_set_sensitive (priv->invite, (!read_only &&  sens) || delegate);
	gtk_widget_set_sensitive (GTK_WIDGET (priv->list_view), !read_only);	

	bonobo_ui_component_set_prop (priv->uic, "/commands/InsertAttachments", "sensitive", sensitize ? "1" : "0"
			, NULL);
	bonobo_ui_component_set_prop (priv->uic, "/commands/ViewTimeZone", "sensitive", sensitize ? "1" : "0"
			, NULL);
	bonobo_ui_component_set_prop (priv->uic, "/commands/ActionAllDayEvent", "sensitive", sensitize ? "1" : "0"
			, NULL);
	bonobo_ui_component_set_prop (priv->uic, "/commands/ActionRecurrence", "sensitive", sensitize ? "1" : "0"
			, NULL);
	bonobo_ui_component_set_prop (priv->uic, "/commands/ActionShowTimeBusy", "sensitive", !read_only ? "1" : "0"
			, NULL);
	bonobo_ui_component_set_prop (priv->uic, "/commands/ActionAlarm", "sensitive", !read_only ? "1" : "0"
			, NULL);
	bonobo_ui_component_set_prop (priv->uic, "/commands/ActionClassPublic", "sensitive", sensitize ? "1" : "0"
			, NULL);
	bonobo_ui_component_set_prop (priv->uic, "/commands/ActionClassPrivate", "sensitive", sensitize ? "1" : "0"
			, NULL);
	bonobo_ui_component_set_prop (priv->uic, "/commands/ActionClassConfidential", "sensitive",
		       	sensitize ? "1" : "0", NULL);
	bonobo_ui_component_set_prop (priv->uic, "/commands/ViewCategories", "sensitive", sensitize ? "1" : "0"
			, NULL);
	bonobo_ui_component_set_prop (priv->uic, "/commands/InsertSendOptions", "sensitive", sensitize ? "1" : "0"
			, NULL);

	if (!priv->is_meeting) {
		gtk_widget_hide (priv->calendar_label);
		gtk_widget_hide (priv->list_box);
		gtk_widget_hide (priv->attendee_box);
		gtk_widget_hide (priv->organizer);
		gtk_label_set_text_with_mnemonic ((GtkLabel *) priv->org_cal_label, _("Cale_ndar"));
	} else {
		gtk_widget_show (priv->calendar_label);
		gtk_widget_show (priv->list_box);
		gtk_widget_show (priv->attendee_box);
		gtk_widget_show (priv->organizer);
		gtk_label_set_text_with_mnemonic ((GtkLabel *) priv->org_cal_label, _("Or_ganizer"));		
	}
	
}

void
event_page_hide_options (EventPage *page)
{
	g_return_if_fail (IS_EVENT_PAGE (page));

	bonobo_ui_component_set_prop (page->priv->uic, "/commands/InsertSendOptions", "hidden", "1", NULL);
	page->priv->sendoptions_shown = FALSE;
}

void
event_page_show_options (EventPage *page)
{
	g_return_if_fail (IS_EVENT_PAGE (page));

	bonobo_ui_component_set_prop (page->priv->uic, "/commands/InsertSendOptions", "hidden", "0", NULL);
	page->priv->sendoptions_shown = TRUE;
}

void 
event_page_set_meeting (EventPage *page, gboolean set)
{
	g_return_if_fail (IS_EVENT_PAGE (page));

	page->priv->is_meeting = set;
        sensitize_widgets (page);
}

void 
event_page_set_delegate (EventPage *page, gboolean set)
{
	g_return_if_fail (IS_EVENT_PAGE (page));

	if (set)
		gtk_label_set_text_with_mnemonic ((GtkLabel *)page->priv->attendees_label, _("_Delegatees"));
	else
		gtk_label_set_text_with_mnemonic ((GtkLabel *)page->priv->attendees_label, _("Atte_ndees"));
}

static EAccount *
get_current_account (EventPage *epage)
{	
	EventPagePrivate *priv;
	EIterator *it;
	const char *str;
	
	priv = epage->priv;

	str = gtk_entry_get_text (GTK_ENTRY (GTK_COMBO (priv->organizer)->entry));
	if (!str)
		return NULL;
	
	for (it = e_list_get_iterator((EList *)priv->accounts); e_iterator_is_valid(it); e_iterator_next(it)) {
		EAccount *a = (EAccount *)e_iterator_get(it);
		char *full = g_strdup_printf("%s <%s>", a->id->name, a->id->address);

		if (!strcmp (full, str)) {
			g_free (full);
			g_object_unref (it);

			return a;
		}
	
		g_free (full);
	}
	g_object_unref (it);
	
	return NULL;	
}

/* fill_widgets handler for the event page */
static gboolean
event_page_fill_widgets (CompEditorPage *page, ECalComponent *comp)
{
	EventPage *epage;
	EventPagePrivate *priv;
	ECalComponentText text;
	ECalComponentClassification cl;
	ECalComponentTransparency transparency;
	ECalComponentDateTime start_date, end_date;
	const char *location, *uid = NULL;
	const char *categories;
	ESource *source;
	GSList *l;
	gboolean validated = TRUE;
	
	g_return_val_if_fail (page->client != NULL, FALSE);

	epage = EVENT_PAGE (page);
	priv = epage->priv;

	if (!e_cal_component_has_organizer (comp)) 
		page->flags |= COMP_EDITOR_PAGE_USER_ORG;

	/* Don't send off changes during this time */
	priv->updating = TRUE;
	
	/* Clean out old data */
	if (priv->comp != NULL)
		g_object_unref (priv->comp);
	priv->comp = NULL;
	
	cleanup_attendees (priv->deleted_attendees);
	g_ptr_array_set_size (priv->deleted_attendees, 0);
	
	/* Component for cancellation */
	priv->comp = e_cal_component_clone (comp);

	/* Clean the page */
	clear_widgets (epage);

	/* Summary, location, description(s) */

	/* Component for cancellation */
	priv->comp = e_cal_component_clone (comp);
	
	e_cal_component_get_summary (comp, &text);
	e_dialog_editable_set (priv->summary, text.value);
	priv->old_summary = g_strdup (text.value);
	
	e_cal_component_get_location (comp, &location);
	e_dialog_editable_set (priv->location, location);

	e_cal_component_get_description_list (comp, &l);
	if (l && l->data) {
		ECalComponentText *dtext;
		
		dtext = l->data;
		gtk_text_buffer_set_text (gtk_text_view_get_buffer (GTK_TEXT_VIEW (priv->description)),
					  dtext->value ? dtext->value : "", -1);
	}
	e_cal_component_free_text_list (l);

	if (priv->is_meeting) {
		ECalComponentOrganizer organizer;	
		
		priv->user_add = itip_get_comp_attendee (comp, COMP_EDITOR_PAGE (epage)->client);	

		/* If there is an existing organizer show it properly */
		if (e_cal_component_has_organizer (comp)) {
			e_cal_component_get_organizer (comp, &organizer);
			if (organizer.value != NULL) {
				const gchar *strip = itip_strip_mailto (organizer.value);
				gchar *string;
				if (itip_organizer_is_user (comp, page->client)) {
					if (e_cal_get_static_capability (
								page->client,
								CAL_STATIC_CAPABILITY_ORGANIZER_NOT_EMAIL_ADDRESS))
						priv->user_org = TRUE;
				} else {
					if (e_cal_get_static_capability (
								page->client,
								CAL_STATIC_CAPABILITY_ORGANIZER_NOT_EMAIL_ADDRESS))
					gtk_widget_set_sensitive (priv->invite, FALSE);
					gtk_widget_set_sensitive (priv->add, FALSE);
					gtk_widget_set_sensitive (priv->remove, FALSE);
					priv->user_org = FALSE;
				}

				if (e_cal_get_static_capability (COMP_EDITOR_PAGE (epage)->client, CAL_STATIC_CAPABILITY_NO_ORGANIZER) && (COMP_EDITOR_PAGE (epage)->flags & COMP_EDITOR_PAGE_DELEGATE))
					string = g_strdup (priv->user_add);
				else if ( organizer.cn != NULL)
					string = g_strdup_printf ("%s <%s>", organizer.cn, strip);
				else
					string = g_strdup (strip);

				g_free (string);
				priv->existing = TRUE;
			}
		} else {
			EAccount *a;

			a = get_current_account (epage);
			if (a != NULL) {
				CompEditorPage *page = (CompEditorPage *) epage;
				priv->ia = e_meeting_store_add_attendee_with_defaults (priv->model);
				g_object_ref (priv->ia);

				e_meeting_attendee_set_address (priv->ia, g_strdup_printf ("MAILTO:%s", a->id->address));
				e_meeting_attendee_set_cn (priv->ia, g_strdup (a->id->name));
				if (page->client && e_cal_get_organizer_must_accept (page->client))
					e_meeting_attendee_set_status (priv->ia, ICAL_PARTSTAT_NEEDSACTION);
				else
					e_meeting_attendee_set_status (priv->ia, ICAL_PARTSTAT_ACCEPTED);
				e_meeting_list_view_add_attendee_to_name_selector (E_MEETING_LIST_VIEW (priv->list_view), priv->ia);
			}
		}
	}

	/* Start and end times */
	e_cal_component_get_dtstart (comp, &start_date);
	e_cal_component_get_dtend (comp, &end_date);
	if (!start_date.value) {
		comp_editor_page_display_validation_error (page, _("Event with no start date"), priv->start_time);
		validated = FALSE;
	} else if (!end_date.value) {
		comp_editor_page_display_validation_error (page, _("Event with no end date"), priv->end_time);
		validated = FALSE;
	} else
		update_time (epage, &start_date, &end_date);
	
	e_cal_component_free_datetime (&start_date);
	e_cal_component_free_datetime (&end_date);

	update_end_time_selector (epage);
	/* Classification */
	e_cal_component_get_classification (comp, &cl);
	switch (cl) {
	case E_CAL_COMPONENT_CLASS_PUBLIC:
	case E_CAL_COMPONENT_CLASS_PRIVATE:
	case E_CAL_COMPONENT_CLASS_CONFIDENTIAL:
		break;
	default:
		cl = E_CAL_COMPONENT_CLASS_PUBLIC;
		break;
	}
	set_classification_menu (epage, cl);

	/* Show Time As (Transparency) */
	e_cal_component_get_transparency (comp, &transparency);
	switch (transparency) {
	case E_CAL_COMPONENT_TRANSP_TRANSPARENT:
		set_busy_time_menu (epage, FALSE);
		break;

	default:
		set_busy_time_menu (epage, TRUE);
		break;
	}

	if (e_cal_get_static_capability (page->client, CAL_STATIC_CAPABILITY_NO_TRANSPARENCY))
		enable_busy_time_menu (epage, FALSE);
	else
		enable_busy_time_menu (epage, TRUE);

	/* Alarms */
	g_signal_handlers_block_matched (priv->alarm, G_SIGNAL_MATCH_DATA, 0, 0, NULL, NULL, epage);
	if (e_cal_component_has_alarms (comp)) {
		GList *alarms, *l;
		int alarm_type;
		
		e_dialog_toggle_set (priv->alarm, TRUE);

		alarms = e_cal_component_get_alarm_uids (comp);
		if (!is_custom_alarm_uid_list (comp, alarms, priv->old_summary, priv->alarm_units, priv->alarm_interval, &alarm_type))
			e_dialog_option_menu_set (priv->alarm_time, alarm_type, alarm_map);

		for (l = alarms; l != NULL; l = l->next) {
			ECalComponentAlarm *ca;
			
			ca = e_cal_component_get_alarm (comp, l->data);
			e_alarm_list_append (priv->alarm_list_store, NULL, ca);			
			e_cal_component_alarm_free (ca);
		}

		cal_obj_uid_list_free (alarms);
	} else {
		e_dialog_toggle_set (priv->alarm, FALSE);
		e_dialog_option_menu_set (priv->alarm_time, priv->alarm_interval == -1 ? ALARM_15_MINUTES : ALARM_USER_TIME, alarm_map);
	}
	g_signal_handlers_unblock_matched (priv->alarm, G_SIGNAL_MATCH_DATA, 0, 0, NULL, NULL, epage);

	/* Categories */
	e_cal_component_get_categories (comp, &categories);
	e_dialog_editable_set (priv->categories, categories);
	
	/* Source */
	source = e_cal_get_source (page->client);
	e_source_option_menu_select (E_SOURCE_OPTION_MENU (priv->source_selector), source);

	e_cal_component_get_uid (comp, &uid);
	if (!(COMP_EDITOR_PAGE (epage)->flags & COMP_EDITOR_PAGE_DELEGATE) 
			&& !(COMP_EDITOR_PAGE (epage)->flags && COMP_EDITOR_PAGE_NEW_ITEM)) {
		event_page_hide_options (epage);
	}

	priv->updating = FALSE;

	sensitize_widgets (epage);

	return validated;
}

/* fill_component handler for the event page */
static gboolean
event_page_fill_component (CompEditorPage *page, ECalComponent *comp)
{
	EventPage *epage;
	EventPagePrivate *priv;
	ECalComponentDateTime start_date, end_date;
	struct icaltimetype start_tt, end_tt;
	gboolean all_day_event, start_date_set, end_date_set, busy;
	char *cat, *str;
	GtkTextBuffer *text_buffer;
	GtkTextIter text_iter_start, text_iter_end;

	epage = EVENT_PAGE (page);
	priv = epage->priv;
	text_buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (priv->description));

	/* Summary */

	str = e_dialog_editable_get (priv->summary);
	if (!str || strlen (str) == 0)
		e_cal_component_set_summary (comp, NULL);
	else {
		ECalComponentText text;

		text.value = str;
		text.altrep = NULL;

		e_cal_component_set_summary (comp, &text);
	}

	if (str)
		g_free (str);

	/* Location */

	str = e_dialog_editable_get (priv->location);
	if (!str || strlen (str) == 0)
		e_cal_component_set_location (comp, NULL);
	else
		e_cal_component_set_location (comp, str);

	if (str)
		g_free (str);

	/* Description */

	gtk_text_buffer_get_start_iter (text_buffer, &text_iter_start);
	gtk_text_buffer_get_end_iter   (text_buffer, &text_iter_end);
	str = gtk_text_buffer_get_text (text_buffer, &text_iter_start, &text_iter_end, FALSE);

	if (!str || strlen (str) == 0)
		e_cal_component_set_description_list (comp, NULL);
	else {
		GSList l;
		ECalComponentText text;

		text.value = str;
		text.altrep = NULL;
		l.data = &text;
		l.next = NULL;

		e_cal_component_set_description_list (comp, &l);
	}

	if (str)
		g_free (str);

	/* Dates */

	start_tt = icaltime_null_time ();
	start_date.value = &start_tt;
	start_date.tzid = NULL;

	end_tt = icaltime_null_time ();
	end_date.value = &end_tt;
	end_date.tzid = NULL;

	if (!e_date_edit_date_is_valid (E_DATE_EDIT (priv->start_time))) {
		comp_editor_page_display_validation_error (page, _("Start date is wrong"), priv->start_time);
		return FALSE;
	}
	start_date_set = e_date_edit_get_date (E_DATE_EDIT (priv->start_time),
					       &start_tt.year,
					       &start_tt.month,
					       &start_tt.day);
	g_assert (start_date_set);

	if (!e_date_edit_date_is_valid (E_DATE_EDIT (priv->end_time))) {
		comp_editor_page_display_validation_error (page, _("End date is wrong"), priv->end_time);
		return FALSE;
	}
	end_date_set = e_date_edit_get_date (E_DATE_EDIT (priv->end_time),
					     &end_tt.year,
					     &end_tt.month,
					     &end_tt.day);
	g_assert (end_date_set);

	/* If the all_day toggle is set, we use DATE values for DTSTART and
	   DTEND. If not, we fetch the hour & minute from the widgets. */
	all_day_event = priv->all_day_event;

	if (all_day_event) {
		start_tt.is_date = TRUE;
		end_tt.is_date = TRUE;

		/* We have to add 1 day to DTEND, as it is not inclusive. */
		icaltime_adjust (&end_tt, 1, 0, 0, 0);
	} else {
		icaltimezone *start_zone;

		if (!e_date_edit_time_is_valid (E_DATE_EDIT (priv->start_time))) {
			comp_editor_page_display_validation_error (page, _("Start time is wrong"), priv->start_time);
			return FALSE;
		}
		e_date_edit_get_time_of_day (E_DATE_EDIT (priv->start_time),
					     &start_tt.hour,
					     &start_tt.minute);
		if (!e_date_edit_time_is_valid (E_DATE_EDIT (priv->end_time))) {
			comp_editor_page_display_validation_error (page, _("End time is wrong"), priv->end_time);
			return FALSE;
		}
		e_date_edit_get_time_of_day (E_DATE_EDIT (priv->end_time),
					     &end_tt.hour,
					     &end_tt.minute);
		start_zone = e_timezone_entry_get_timezone (E_TIMEZONE_ENTRY (priv->start_timezone));
		start_date.tzid = icaltimezone_get_tzid (start_zone);
		end_date.tzid = icaltimezone_get_tzid (start_zone);
	}

	e_cal_component_set_dtstart (comp, &start_date);
	e_cal_component_set_dtend (comp, &end_date);


	/* Categories */

	cat = e_dialog_editable_get (priv->categories);
	str = comp_editor_strip_categories (cat);
	if (cat)
		g_free (cat);

	e_cal_component_set_categories (comp, str);

	if (str)
		g_free (str);

	/* Classification */
	e_cal_component_set_classification (comp, priv->classification);

	/* Show Time As (Transparency) */
	busy = priv->show_time_as_busy;
	e_cal_component_set_transparency (comp, busy ? E_CAL_COMPONENT_TRANSP_OPAQUE : E_CAL_COMPONENT_TRANSP_TRANSPARENT);

	/* send options */
	if (priv->sendoptions_shown && priv->sod) 
		e_sendoptions_utils_fill_component (priv->sod, comp);

	/* Alarm */
	e_cal_component_remove_all_alarms (comp);
	if (e_dialog_toggle_get (priv->alarm)) {
		if (is_custom_alarm_store (priv->alarm_list_store, priv->old_summary, priv->alarm_units, priv->alarm_interval, NULL)) {
			GtkTreeModel *model;
			GtkTreeIter iter;
			gboolean valid_iter;

			model = GTK_TREE_MODEL (priv->alarm_list_store);
			
			for (valid_iter = gtk_tree_model_get_iter_first (model, &iter); valid_iter;
			     valid_iter = gtk_tree_model_iter_next (model, &iter)) {
				ECalComponentAlarm *alarm, *alarm_copy;
				icalcomponent *icalcomp;
				icalproperty *icalprop;
				
				alarm = (ECalComponentAlarm *) e_alarm_list_get_alarm (priv->alarm_list_store, &iter);
				g_assert (alarm != NULL);
				
				/* We set the description of the alarm if it's got
				 * the X-EVOLUTION-NEEDS-DESCRIPTION property.
				 */
				icalcomp = e_cal_component_alarm_get_icalcomponent (alarm);
				icalprop = icalcomponent_get_first_property (icalcomp, ICAL_X_PROPERTY);
				while (icalprop) {
					const char *x_name;
					ECalComponentText summary;

					x_name = icalproperty_get_x_name (icalprop);
					if (!strcmp (x_name, "X-EVOLUTION-NEEDS-DESCRIPTION")) {
						e_cal_component_get_summary (comp, &summary);
						e_cal_component_alarm_set_description (alarm, &summary);

						icalcomponent_remove_property (icalcomp, icalprop);
						break;
					}

					icalprop = icalcomponent_get_next_property (icalcomp, ICAL_X_PROPERTY);
				}

				/* We clone the alarm to maintain the invariant that the alarm
				 * structures in the list did *not* come from the component.
				 */

				alarm_copy = e_cal_component_alarm_clone (alarm);
				e_cal_component_add_alarm (comp, alarm_copy);
				e_cal_component_alarm_free (alarm_copy);
			}
		} else {
			ECalComponentAlarm *ca;
			ECalComponentText summary;
			ECalComponentAlarmTrigger trigger;
			int alarm_type;

			ca = e_cal_component_alarm_new ();
			
			e_cal_component_get_summary (comp, &summary);
			e_cal_component_alarm_set_description (ca, &summary);
		
			e_cal_component_alarm_set_action (ca, E_CAL_COMPONENT_ALARM_DISPLAY);

			memset (&trigger, 0, sizeof (ECalComponentAlarmTrigger));
			trigger.type = E_CAL_COMPONENT_ALARM_TRIGGER_RELATIVE_START;		
			trigger.u.rel_duration.is_neg = 1;
		
			alarm_type = e_dialog_option_menu_get (priv->alarm_time, alarm_map);
			switch (alarm_type) {
			case ALARM_15_MINUTES:
				trigger.u.rel_duration.minutes = 15;
				break;
			
			case ALARM_1_HOUR:
				trigger.u.rel_duration.hours = 1;
				break;
			
			case ALARM_1_DAY:
				trigger.u.rel_duration.days = 1;
				break;

			case ALARM_USER_TIME:
				switch (calendar_config_get_default_reminder_units ()) {
				case CAL_DAYS:		
					trigger.u.rel_duration.days = priv->alarm_interval;
					break;
		
				case CAL_HOURS:
					trigger.u.rel_duration.hours = priv->alarm_interval;
					break;
		
				case CAL_MINUTES:
					trigger.u.rel_duration.minutes = priv->alarm_interval;
					break;
				}
				break;
				
			default:
				break;
			}		
			e_cal_component_alarm_set_trigger (ca, trigger);

			e_cal_component_add_alarm (comp, ca);
		}
	}
	
	if (priv->is_meeting) {
		ECalComponentOrganizer organizer = {NULL, NULL, NULL, NULL};

		if (!priv->existing) {
			EAccount *a;
			gchar *addr = NULL;

			/* Find the identity for the organizer or sentby field */
			a = get_current_account (epage);

			/* Sanity Check */
			if (a == NULL) {
				e_notice (page, GTK_MESSAGE_ERROR,
						_("The organizer selected no longer has an account."));
				return FALSE;			
			}

			if (a->id->address == NULL || strlen (a->id->address) == 0) {
				e_notice (page, GTK_MESSAGE_ERROR,
						_("An organizer is required."));
				return FALSE;
			} 

			addr = g_strdup_printf ("MAILTO:%s", a->id->address);

			organizer.value = addr;
			organizer.cn = a->id->name;
			e_cal_component_set_organizer (comp, &organizer);

			g_free (addr);
		}

		if (e_meeting_store_count_actual_attendees (priv->model) < 1) {
			e_notice (page, GTK_MESSAGE_ERROR,
					_("At least one attendee is required."));
			return FALSE;
		}


		if (COMP_EDITOR_PAGE (epage)->flags & COMP_EDITOR_PAGE_DELEGATE ) {
			GSList *attendee_list, *l;
			int i;
			const GPtrArray *attendees = e_meeting_store_get_attendees (priv->model);

			e_cal_component_get_attendee_list (priv->comp, &attendee_list);

			for (i = 0; i < attendees->len; i++) {
				EMeetingAttendee *ia = g_ptr_array_index (attendees, i);
				ECalComponentAttendee *ca;

				/* Remove the duplicate user from the component if present */
				if (e_meeting_attendee_is_set_delto (ia)) {
					for (l = attendee_list; l; l = l->next) {
						ECalComponentAttendee *a = l->data;

						if (g_str_equal (a->value, e_meeting_attendee_get_address (ia))) {
							attendee_list = g_slist_remove (attendee_list, l->data);
							break;
						}
					}
				}

				ca = e_meeting_attendee_as_e_cal_component_attendee (ia);

				attendee_list = g_slist_append (attendee_list, ca);
			}
			e_cal_component_set_attendee_list (comp, attendee_list);
			e_cal_component_free_attendee_list (attendee_list);
		} else 
			set_attendees (comp, e_meeting_store_get_attendees (priv->model));
	}

	
	return TRUE;
}

/* fill_timezones handler for the event page */
static gboolean
event_page_fill_timezones (CompEditorPage *page, GHashTable *timezones)
{
	EventPage *epage;
	EventPagePrivate *priv;
	icaltimezone *zone;

	epage = EVENT_PAGE (page);
	priv = epage->priv;

	/* add start date timezone */
	zone = e_timezone_entry_get_timezone (E_TIMEZONE_ENTRY (priv->start_timezone));
	if (zone) {
		if (!g_hash_table_lookup (timezones, icaltimezone_get_tzid (zone)))
			g_hash_table_insert (timezones, icaltimezone_get_tzid (zone), zone);
	}

	/* add end date timezone */
	zone = e_timezone_entry_get_timezone (E_TIMEZONE_ENTRY (priv->end_timezone));
	if (zone) {
		if (!g_hash_table_lookup (timezones, icaltimezone_get_tzid (zone)))
			g_hash_table_insert (timezones, icaltimezone_get_tzid (zone), zone);
	}

	return TRUE;
}

/* set_summary handler for the event page */
static void
event_page_set_summary (CompEditorPage *page, const char *summary)
{
	/* nothing */
}

static void
event_page_set_dates (CompEditorPage *page, CompEditorPageDates *dates)
{	
	update_time (EVENT_PAGE (page), dates->start, dates->end);
}



static void
time_sel_changed (GtkOptionMenu *widget, EventPage *epage)
{
	EventPagePrivate *priv;
	int selection = gtk_option_menu_get_history (widget);

	priv = epage->priv;

	if (selection == 1) {
		gtk_widget_hide (priv->time_hour);
		gtk_widget_show (priv->end_time);
		hour_sel_changed (GTK_SPIN_BUTTON (priv->hour_selector), epage);
		minute_sel_changed (GTK_SPIN_BUTTON (priv->minute_selector), epage);
	} else if (!selection){
		gtk_widget_show (priv->time_hour);
		gtk_widget_hide (priv->end_time);

		update_end_time_selector ( epage);
	}
}

static
void update_end_time_selector (EventPage *epage)
{
	EventPagePrivate *priv;
	struct icaltimetype start_tt = icaltime_null_time();
	struct icaltimetype end_tt = icaltime_null_time();
	time_t start_timet,end_timet;
	gint hours,minutes;

	priv = epage->priv;

	e_date_edit_get_date (E_DATE_EDIT (priv->start_time),
                             &start_tt.year,
                             &start_tt.month,
                             &start_tt.day);
        e_date_edit_get_time_of_day (E_DATE_EDIT (priv->start_time),
       	                             &start_tt.hour,
               	                     &start_tt.minute);
	e_date_edit_get_date (E_DATE_EDIT (priv->end_time),
                             &end_tt.year,
                             &end_tt.month,
                             &end_tt.day);
	e_date_edit_get_time_of_day (E_DATE_EDIT (priv->end_time),
               	                     &end_tt.hour,
                       	             &end_tt.minute);
	
	end_timet = icaltime_as_timet (end_tt);
	start_timet = icaltime_as_timet (start_tt);

	end_timet -= start_timet;
	hours = end_timet / ( 60 * 60 );
	minutes = (end_timet/60) - ( hours * 60 );

	gtk_spin_button_set_value (GTK_SPIN_BUTTON (priv->hour_selector), hours);
	gtk_spin_button_set_value (GTK_SPIN_BUTTON (priv->minute_selector), minutes);
}

void
static hour_sel_changed (GtkSpinButton *widget, EventPage *epage)
{
	hour_minute_changed(epage);
}

void
static minute_sel_changed (GtkSpinButton *widget, EventPage *epage)
{
	hour_minute_changed ( epage);
}

void
static hour_minute_changed ( EventPage *epage)
{
	EventPagePrivate *priv;
	gint for_hours, for_minutes;
	struct icaltimetype end_tt = icaltime_null_time();

	priv = epage->priv;

	e_date_edit_get_date (E_DATE_EDIT (priv->start_time),
                              &end_tt.year,
                              &end_tt.month,
                              &end_tt.day);
        e_date_edit_get_time_of_day (E_DATE_EDIT (priv->start_time),
                                     &end_tt.hour,
                                     &end_tt.minute);

	for_hours = gtk_spin_button_get_value (GTK_SPIN_BUTTON (priv->hour_selector));
	for_minutes = gtk_spin_button_get_value (GTK_SPIN_BUTTON (priv->minute_selector));

	icaltime_adjust (&end_tt, 0, for_hours, for_minutes, 0);

	e_date_edit_set_date_and_time_of_day (E_DATE_EDIT (priv->end_time),
				  	      end_tt.year,
					      end_tt.month,
					      end_tt.day,
			      		      end_tt.hour,
					      end_tt.minute);
}

static void
add_clicked_cb (GtkButton *btn, EventPage *epage)
{
	EMeetingAttendee *attendee;

	attendee = e_meeting_store_add_attendee_with_defaults (epage->priv->model);

	if (COMP_EDITOR_PAGE (epage)->flags & COMP_EDITOR_PAGE_DELEGATE) {
		e_meeting_attendee_set_delfrom (attendee, g_strdup_printf ("MAILTO:%s", epage->priv->user_add));
	}

	e_meeting_list_view_edit (epage->priv->list_view, attendee);
}

static gboolean
existing_attendee (EMeetingAttendee *ia, ECalComponent *comp) 
{
	GSList *attendees, *l;
	const gchar *ia_address;
	
	ia_address = itip_strip_mailto (e_meeting_attendee_get_address (ia));
	if (!ia_address)
		return FALSE;
	
	e_cal_component_get_attendee_list (comp, &attendees);

	for (l = attendees; l; l = l->next) {
		ECalComponentAttendee *attendee = l->data;
		const char *address;
		
		address = itip_strip_mailto (attendee->value);
		if (address && !g_strcasecmp (ia_address, address)) {
			e_cal_component_free_attendee_list (attendees);
			return TRUE;
		}
	}
	
	e_cal_component_free_attendee_list (attendees);
	
	return FALSE;
}

static void
remove_attendee (EventPage *epage, EMeetingAttendee *ia) 
{
	EventPagePrivate *priv;
	int pos = 0;
	gboolean delegate = (COMP_EDITOR_PAGE (epage)->flags & COMP_EDITOR_PAGE_DELEGATE);
	
	priv = epage->priv;

	/* If the user deletes the organizer attendee explicitly,
	   assume they no longer want the organizer showing up */
	if (ia == priv->ia) {
		g_object_unref (priv->ia);
		priv->ia = NULL;
	}	
		
	/* If this was a delegatee, no longer delegate */
	if (e_meeting_attendee_is_set_delfrom (ia)) {
		EMeetingAttendee *ib;
		
		ib = e_meeting_store_find_attendee (priv->model, e_meeting_attendee_get_delfrom (ia), &pos);
		if (ib != NULL) {
			e_meeting_attendee_set_delto (ib, NULL);
			
			if (!delegate) 
				e_meeting_attendee_set_edit_level (ib,  E_MEETING_ATTENDEE_EDIT_FULL);
		}		
	}
	
	/* Handle deleting all attendees in the delegation chain */	
	while (ia != NULL) {
		EMeetingAttendee *ib = NULL;

		if (existing_attendee (ia, priv->comp)) {
			g_object_ref (ia);
			g_ptr_array_add (priv->deleted_attendees, ia);
		}
		
		if (e_meeting_attendee_get_delto (ia) != NULL)
			ib = e_meeting_store_find_attendee (priv->model, e_meeting_attendee_get_delto (ia), NULL);
		
		e_meeting_list_view_remove_attendee_from_name_selector (priv->list_view, ia);
		e_meeting_store_remove_attendee (priv->model, ia);

		ia = ib;
	}
	
	sensitize_widgets (epage);
}

static void
remove_clicked_cb (GtkButton *btn, EventPage *epage)
{
	EventPagePrivate *priv;
	EMeetingAttendee *ia;
	GtkTreeSelection *selection;
	GList *paths = NULL, *tmp;
	GtkTreeIter iter;
	GtkTreePath *path = NULL;
	gboolean valid_iter;
	char *address;
	
	priv = epage->priv;

	selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (priv->list_view));
	if (!(paths = gtk_tree_selection_get_selected_rows (selection, (GtkTreeModel **) &(priv->model)))) {
		g_warning ("Could not get a selection to delete.");
		return;
	}
	paths = g_list_reverse (paths);
	
	for (tmp = paths; tmp; tmp=tmp->next) {
		path = tmp->data;
		
		gtk_tree_model_get_iter (GTK_TREE_MODEL(priv->model), &iter, path);

		gtk_tree_model_get (GTK_TREE_MODEL (priv->model), &iter, E_MEETING_STORE_ADDRESS_COL, &address, -1);
		ia = e_meeting_store_find_attendee (priv->model, address, NULL);
		g_free (address);
		if (!ia) {
			g_warning ("Cannot delete attendee\n");
			continue;
		} else if (e_meeting_attendee_get_edit_level (ia) != E_MEETING_ATTENDEE_EDIT_FULL) {
			g_warning("Not enough rights to delete attendee: %s\n", e_meeting_attendee_get_address(ia));	
			continue;
		}
		
		remove_attendee (epage, ia);
	}
	
	/* Select closest item after removal */
	valid_iter = gtk_tree_model_get_iter (GTK_TREE_MODEL (priv->model), &iter, path);
	if (!valid_iter) {
		gtk_tree_path_prev (path);
		valid_iter = gtk_tree_model_get_iter (GTK_TREE_MODEL (priv->model), &iter, path);
	}
	
	if (valid_iter) {
		gtk_tree_selection_unselect_all (selection);
		gtk_tree_selection_select_iter (selection, &iter);
	}
	
	g_list_foreach (paths, (GFunc) gtk_tree_path_free, NULL);
	g_list_free (paths);
}

static void
invite_cb (GtkWidget *widget, gpointer data) 
{
	EventPage *page;
	EventPagePrivate *priv;
	
	page = EVENT_PAGE (data);
	priv = page->priv;

	e_meeting_list_view_invite_others_dialog (priv->list_view);
}

static void
attendee_added_cb (EMeetingListView *emlv, EMeetingAttendee *ia, gpointer user_data)
{
   EventPage *epage = EVENT_PAGE (user_data);	
   EventPagePrivate *priv;
   gboolean delegate = (COMP_EDITOR_PAGE (epage)->flags & COMP_EDITOR_PAGE_DELEGATE);

   priv = epage->priv;

   if (delegate) {
	   if (existing_attendee (ia, priv->comp))
		   e_meeting_store_remove_attendee (priv->model, ia);
	   else {
		   if (!e_cal_get_static_capability (COMP_EDITOR_PAGE(epage)->client, 
					   CAL_STATIC_CAPABILITY_DELEGATE_TO_MANY)) {
			   const char *delegator_id = e_meeting_attendee_get_delfrom (ia);
			   EMeetingAttendee *delegator;

			   delegator = e_meeting_store_find_attendee (priv->model, delegator_id, NULL);
			   e_meeting_attendee_set_delto (delegator, 
					   g_strdup (e_meeting_attendee_get_address (ia)));

			   gtk_widget_set_sensitive (priv->invite, FALSE);
			   gtk_widget_set_sensitive (priv->add, FALSE);
		   }
	   }
   }

}

/* Callbacks for list view*/
static void
popup_add_cb (EPopup *ep, EPopupItem *pitem, void *data)
{
	EventPage *epage = data;

	add_clicked_cb (NULL, epage);
}

static void
popup_delete_cb (EPopup *ep, EPopupItem *pitem, void *data)
{
	EventPage *epage = data;
	EventPagePrivate *priv;
	
	priv = epage->priv;

	remove_clicked_cb (NULL, epage);
}

enum {
	ATTENDEE_CAN_DELEGATE = 1<<1,
	ATTENDEE_CAN_DELETE = 1<<2,
	ATTENDEE_CAN_ADD = 1<<3,
	ATTENDEE_LAST = 1<<4,
};

static EPopupItem context_menu_items[] = {
	{ E_POPUP_ITEM, "10.delete", N_("_Remove"), popup_delete_cb, NULL, GTK_STOCK_REMOVE, ATTENDEE_CAN_DELETE },
	{ E_POPUP_ITEM, "15.add", N_("_Add "), popup_add_cb, NULL, GTK_STOCK_ADD },	
};

static void
context_popup_free(EPopup *ep, GSList *items, void *data)
{
	g_slist_free(items);
}

static gint
button_press_event (GtkWidget *widget, GdkEventButton *event, EventPage *epage)
{
	EventPagePrivate *priv;
	GtkMenu *menu;
	EMeetingAttendee *ia;
	GtkTreePath *path;
	GtkTreeIter iter;
	char *address;
	guint32 disable_mask = ~0;
	GSList *menus = NULL;
	ECalPopup *ep;
	int i;

	priv = epage->priv;

	/* only process right-clicks */
	if (event->button != 3 || event->type != GDK_BUTTON_PRESS)
		return FALSE;

	/* only if we right-click on an attendee */
	if (gtk_tree_view_get_path_at_pos (GTK_TREE_VIEW (priv->list_view), event->x, event->y, &path, NULL, NULL, NULL)) {
		GtkTreeSelection *selection;

		if (gtk_tree_model_get_iter (GTK_TREE_MODEL (priv->model), &iter, path)) {
		
			gtk_tree_model_get (GTK_TREE_MODEL (priv->model), &iter, E_MEETING_STORE_ADDRESS_COL, &address, -1);
			ia = e_meeting_store_find_attendee (priv->model, address, &priv->row);
			g_free (address);

			if (ia) {
				selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (priv->list_view));
				gtk_tree_selection_unselect_all (selection);
				gtk_tree_selection_select_path (selection, path);
		
 				if (e_meeting_attendee_get_edit_level (ia) == E_MEETING_ATTENDEE_EDIT_FULL)
 					disable_mask &= ~ATTENDEE_CAN_DELETE;
			}
		}
	}

	if (GTK_WIDGET_IS_SENSITIVE(priv->add))
		disable_mask &= ~ATTENDEE_CAN_ADD;
	else if (COMP_EDITOR_PAGE (epage)->flags & COMP_EDITOR_PAGE_USER_ORG)
		disable_mask &= ~ATTENDEE_CAN_ADD;

	ep = e_cal_popup_new("org.gnome.evolution.calendar.meeting.popup");

	for (i=0;i<sizeof(context_menu_items)/sizeof(context_menu_items[0]);i++)
		menus = g_slist_prepend(menus, &context_menu_items[i]);
	
	e_popup_add_items((EPopup *)ep, menus, NULL, context_popup_free, epage);
	menu = e_popup_create_menu_once((EPopup *)ep, NULL, disable_mask);
	gtk_menu_popup (menu, NULL, NULL, NULL, NULL, event->button, event->time);

	return TRUE;
}

static gboolean
list_view_event (EMeetingListView *list_view, GdkEvent *event, EventPage *epage) {
	
	EventPagePrivate *priv= epage->priv;
	
	if (event->type == GDK_2BUTTON_PRESS && COMP_EDITOR_PAGE (epage)->flags & COMP_EDITOR_PAGE_USER_ORG) {
		EMeetingAttendee *attendee;

		attendee = e_meeting_store_add_attendee_with_defaults (priv->model);

		if (COMP_EDITOR_PAGE (epage)->flags & COMP_EDITOR_PAGE_DELEGATE) {
			e_meeting_attendee_set_delfrom (attendee, g_strdup_printf ("MAILTO:%s", epage->priv->user_add));
		}

		e_meeting_list_view_edit (epage->priv->list_view, attendee);
		return TRUE;
	}

	return FALSE;	
}


static gboolean
list_key_press (EMeetingListView *list_view, GdkEventKey *event, EventPage *epage)
{
	if (event->keyval == GDK_Delete) {
		EventPagePrivate *priv;
	
		priv = epage->priv;
		remove_clicked_cb (NULL, epage);

		return TRUE;
	} else if (event->keyval == GDK_Insert) {
		add_clicked_cb (NULL, epage);

		return TRUE;
	}

	return FALSE;
}

void
event_page_set_all_day_event (EventPage *epage, gboolean all_day)
{
	EventPagePrivate *priv = epage->priv;
	struct icaltimetype start_tt = icaltime_null_time();
	struct icaltimetype end_tt = icaltime_null_time();
	gboolean date_set;

	epage->priv->all_day_event = all_day;
	e_date_edit_set_show_time (E_DATE_EDIT (priv->start_time), !all_day);
	e_date_edit_set_show_time (E_DATE_EDIT (priv->end_time), !all_day);

	date_set = e_date_edit_get_date (E_DATE_EDIT (priv->start_time),
					 &start_tt.year,
					 &start_tt.month,
					 &start_tt.day);
	e_date_edit_get_time_of_day (E_DATE_EDIT (priv->start_time),
				     &start_tt.hour,
				     &start_tt.minute);
	g_assert (date_set);

	date_set = e_date_edit_get_date (E_DATE_EDIT (priv->end_time),
					 &end_tt.year,
					 &end_tt.month,
					 &end_tt.day);
	e_date_edit_get_time_of_day (E_DATE_EDIT (priv->end_time),
				     &end_tt.hour,
				     &end_tt.minute);
	g_assert (date_set);

	/* TODO implement the for portion in end time selector */
	gtk_widget_set_sensitive (priv->end_time_selector, !all_day);
	if (all_day) 
		gtk_option_menu_set_history (GTK_OPTION_MENU (priv->end_time_selector), 1);
	else 
		gtk_option_menu_set_history (GTK_OPTION_MENU (priv->end_time_selector), 0);
	
	if (all_day) {
		bonobo_ui_component_set_prop (epage->priv->uic, "/commands/ViewTimeZone", "sensitive", "0", NULL);
		
		/* Round down to the start of the day. */
		start_tt.hour = 0;
		start_tt.minute  = 0;
		start_tt.second  = 0;
		start_tt.is_date = TRUE;

		/* Round down to the start of the day, or the start of the
		   previous day if it is midnight. */
		icaltime_adjust (&end_tt, 0, 0, 0, -1);
		end_tt.hour = 0;
		end_tt.minute  = 0;
		end_tt.second  = 0;
		end_tt.is_date = TRUE;
	} else {
		icaltimezone *start_zone;

		bonobo_ui_component_set_prop (epage->priv->uic, "/commands/ViewTimeZone", "sensitive", "1", NULL);
		
		if (end_tt.year == start_tt.year
		    && end_tt.month == start_tt.month
		    && end_tt.day == start_tt.day) {
			/* The event is within one day, so we set the event
			   start to the start of the working day, and the end
			   to one hour later. */
			start_tt.hour = calendar_config_get_day_start_hour ();
			start_tt.minute  = calendar_config_get_day_start_minute ();
			start_tt.second  = 0;

			end_tt = start_tt;
			icaltime_adjust (&end_tt, 0, 1, 0, 0);
		} else {
			/* The event is longer than 1 day, so we keep exactly
			   the same times, just using DATE-TIME rather than
			   DATE. */
			icaltime_adjust (&end_tt, 1, 0, 0, 0);
		}

		/* Make sure that end > start using the timezones. */
		start_zone = e_timezone_entry_get_timezone (E_TIMEZONE_ENTRY (priv->start_timezone));
		check_start_before_end (&start_tt, start_zone,
					&end_tt, start_zone,
					TRUE);
	}
	
	event_page_set_show_timezone (epage, calendar_config_get_show_timezone() & !all_day);
	gtk_signal_handler_block_by_data (GTK_OBJECT (priv->start_time),
					  epage);
	gtk_signal_handler_block_by_data (GTK_OBJECT (priv->end_time),
					  epage);

	e_date_edit_set_date (E_DATE_EDIT (priv->start_time), start_tt.year,
			      start_tt.month, start_tt.day);
	e_date_edit_set_time_of_day (E_DATE_EDIT (priv->start_time),
				     start_tt.hour, start_tt.minute);

	e_date_edit_set_date (E_DATE_EDIT (priv->end_time), end_tt.year,
			      end_tt.month, end_tt.day);
	e_date_edit_set_time_of_day (E_DATE_EDIT (priv->end_time),
				     end_tt.hour, end_tt.minute);

	gtk_signal_handler_unblock_by_data (GTK_OBJECT (priv->start_time),
					    epage);
	gtk_signal_handler_unblock_by_data (GTK_OBJECT (priv->end_time),
					    epage);

	/* Notify upstream */
	notify_dates_changed (epage, &start_tt, &end_tt);

        if (!priv->updating)
		comp_editor_page_notify_changed (COMP_EDITOR_PAGE (epage));

}

void
event_page_set_show_time_busy (EventPage *epage, gboolean state)
{
	epage->priv->show_time_as_busy = state;
	if (!epage->priv->updating)
		comp_editor_page_notify_changed (COMP_EDITOR_PAGE (epage));

}

void
event_page_show_alarm (EventPage *epage)
{
	gtk_widget_show (epage->priv->alarm_dialog);
}

void 
event_page_set_show_timezone (EventPage *epage, gboolean state)
{
	if (state) {
		gtk_widget_show_all (epage->priv->start_timezone);
		gtk_widget_show (epage->priv->timezone_label);	
	} else {
		gtk_widget_hide (epage->priv->start_timezone);
		gtk_widget_hide (epage->priv->timezone_label);
	}
		
}

void
event_page_set_show_categories (EventPage *epage, gboolean state)
{
	if (state) {
		gtk_widget_show (epage->priv->categories_btn);
		gtk_widget_show (epage->priv->categories);	
	} else {
		gtk_widget_hide (epage->priv->categories_btn);
		gtk_widget_hide (epage->priv->categories);
	}
}

/* Gets the widgets from the XML file and returns if they are all available. */
static gboolean
get_widgets (EventPage *epage)
{
	CompEditorPage *page = COMP_EDITOR_PAGE (epage);
	EventPagePrivate *priv;
	GSList *accel_groups;
	GtkWidget *toplevel;
	GtkWidget *sw;

	priv = epage->priv;

#define GW(name) glade_xml_get_widget (priv->xml, name)

	priv->main = GW ("event-page");
	if (!priv->main)
		return FALSE;

	/* Get the GtkAccelGroup from the toplevel window, so we can install
	   it when the notebook page is mapped. */
	toplevel = gtk_widget_get_toplevel (priv->main);
	accel_groups = gtk_accel_groups_from_object (G_OBJECT (toplevel));
	if (accel_groups) {
		page->accel_group = accel_groups->data;
		gtk_accel_group_ref (page->accel_group);
	}
	priv->alarm_dialog = GW ("alarm-dialog");
	priv->alarm = GW ("alarm");
	priv->alarm_time = GW ("alarm-time");
	priv->alarm_custom = GW ("alarm-custom");

	priv->timezone_label = GW ("timezone-label");
	priv->start_timezone = GW ("start-timezone");
	priv->end_timezone = priv->start_timezone;
	priv->status_icons = GW ("status-icons");

	gtk_widget_show (priv->status_icons);

	if (!calendar_config_get_show_timezone()) {
		gtk_widget_hide (priv->timezone_label);
		gtk_widget_hide (priv->start_timezone);
	} else {
		gtk_widget_show (priv->timezone_label);
		gtk_widget_show_all (priv->start_timezone);
	}
	priv->attendees_label = GW ("attendees-label");

	gtk_widget_ref (priv->main);
	gtk_container_remove (GTK_CONTAINER (priv->main->parent), priv->main);

	priv->categories = GW ("categories");	
	priv->categories_btn = GW ("categories-button");
	priv->organizer = GW ("organizer");
	priv->summary = GW ("summary");
	priv->summary_label = GW ("summary-label");
	priv->location = GW ("location");
	priv->location_label = GW ("location-label");

	priv->invite = GW ("invite");
	priv->add = GW ("add-attendee");
	priv->remove = GW ("remove-attendee");
	priv->list_box = GW ("list-box");
	
	priv->calendar_label = GW ("calendar-label");
	priv->attendee_box = GW ("attendee-box");
	priv->org_cal_label = GW ("org-cal-label");

	priv->list_view = e_meeting_list_view_new (priv->model);
	
	sw = gtk_scrolled_window_new (NULL, NULL);
	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (sw), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
	gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (sw), GTK_SHADOW_IN);
	gtk_widget_show (sw);
	gtk_container_add (GTK_CONTAINER (sw), GTK_WIDGET (priv->list_view));
	gtk_box_pack_start (GTK_BOX (priv->list_box), sw, TRUE, TRUE, 0);
	

	/* Glade's visibility flag doesn't seem to work for custom widgets */
	priv->start_time = GW ("start-time");
	gtk_widget_show (priv->start_time);
	
	priv->time_hour = GW ("time-hour");
	priv->hour_selector = GW ("hour_selector");
	priv->minute_selector = GW ("minute_selector");
	priv->end_time_selector = GW ("end-time-selector");
	
	priv->end_time = GW ("end-time");
	gtk_widget_show_all (priv->time_hour);
	gtk_widget_hide (priv->end_time);

	priv->description = GW ("description");

	priv->source_selector = GW ("source");

#undef GW

	return (priv->summary
		&& priv->location
		&& priv->start_time
		&& priv->end_time
		&& priv->description );
}

/* Callback used when the summary changes; we emit the notification signal. */
static void
summary_changed_cb (GtkEditable *editable, gpointer data)
{
	EventPage *epage;
	EventPagePrivate *priv;
	gchar *summary;
	
	epage = EVENT_PAGE (data);
	priv = epage->priv;
	
	if (priv->updating)
		return;
	
	summary = e_dialog_editable_get (GTK_WIDGET (editable));
	comp_editor_page_notify_summary_changed (COMP_EDITOR_PAGE (epage),
						 summary);
	g_free (summary);
}


/* Note that this assumes that the start_tt and end_tt passed to it are the
   dates visible to the user. For DATE values, we have to add 1 day to the
   end_tt before emitting the signal. */
static void
notify_dates_changed (EventPage *epage, struct icaltimetype *start_tt,
		      struct icaltimetype *end_tt)
{
	EventPagePrivate *priv;
	CompEditorPageDates dates;
	ECalComponentDateTime start_dt, end_dt;
	gboolean all_day_event;
	icaltimezone *start_zone = NULL;
	priv = epage->priv;
	
	all_day_event = priv->all_day_event;

	start_dt.value = start_tt;
	end_dt.value = end_tt;

	if (all_day_event) {
		/* The actual DTEND is 1 day after the displayed date for
		   DATE values. */
		icaltime_adjust (end_tt, 1, 0, 0, 0);
	} else {
		start_zone = e_timezone_entry_get_timezone (E_TIMEZONE_ENTRY (priv->start_timezone));
	}

	start_dt.tzid = start_zone ? icaltimezone_get_tzid (start_zone) : NULL;
	end_dt.tzid = start_zone ? icaltimezone_get_tzid (start_zone) : NULL;

	dates.start = &start_dt;
	dates.end = &end_dt;

	dates.due = NULL;
	dates.complete = NULL;

	comp_editor_page_notify_dates_changed (COMP_EDITOR_PAGE (epage),
					       &dates);
}


static gboolean
check_start_before_end (struct icaltimetype *start_tt,
			icaltimezone *start_zone,
			struct icaltimetype *end_tt,
			icaltimezone *end_zone,
			gboolean adjust_end_time)
{
	struct icaltimetype end_tt_copy;
	int cmp;

	/* Convert the end time to the same timezone as the start time. */
	end_tt_copy = *end_tt;
	icaltimezone_convert_time (&end_tt_copy, end_zone, start_zone);

	/* Now check if the start time is after the end time. If it is,
	   we need to modify one of the times. */
	cmp = icaltime_compare (*start_tt, end_tt_copy);
	if (cmp > 0) {
		if (adjust_end_time) {
			/* Modify the end time, to be the start + 1 hour. */
			*end_tt = *start_tt;
			icaltime_adjust (end_tt, 0, 1, 0, 0);
			icaltimezone_convert_time (end_tt, start_zone,
						   end_zone);
		} else {
			/* Modify the start time, to be the end - 1 hour. */
			*start_tt = *end_tt;
			icaltime_adjust (start_tt, 0, -1, 0, 0);
			icaltimezone_convert_time (start_tt, end_zone,
						   start_zone);
		}
		return TRUE;
	}

	return FALSE;
}


/*
 * This is called whenever the start or end dates or timezones is changed.
 * It makes sure that the start date < end date. It also emits the notification
 * signals so the other event editor pages update their labels etc.
 *
 * If adjust_end_time is TRUE, if the start time < end time it will adjust
 * the end time. If FALSE it will adjust the start time. If the user sets the
 * start or end time, the other time is adjusted to make it valid.
 */
static void
times_updated (EventPage *epage, gboolean adjust_end_time)
{
	EventPagePrivate *priv;
	struct icaltimetype start_tt = icaltime_null_time();
	struct icaltimetype end_tt = icaltime_null_time();
	gboolean date_set, all_day_event;
	gboolean set_start_date = FALSE, set_end_date = FALSE;
	icaltimezone *start_zone;
	
	priv = epage->priv;

	if (priv->updating)
		return;

	/* Fetch the start and end times and timezones from the widgets. */
	all_day_event = priv->all_day_event;

	date_set = e_date_edit_get_date (E_DATE_EDIT (priv->start_time),
					 &start_tt.year,
					 &start_tt.month,
					 &start_tt.day);
	g_assert (date_set);

	date_set = e_date_edit_get_date (E_DATE_EDIT (priv->end_time),
					 &end_tt.year,
					 &end_tt.month,
					 &end_tt.day);
	g_assert (date_set);

	if (all_day_event) {
		/* All Day Events are simple. We just compare the dates and if
		   start > end we copy one of them to the other. */
		int cmp = icaltime_compare_date_only (start_tt, end_tt);
		if (cmp > 0) {
			if (adjust_end_time) {
				end_tt = start_tt;
				set_end_date = TRUE;
			} else {
				start_tt = end_tt;
				set_start_date = TRUE;
			}
		}

		start_tt.is_date = TRUE;
		end_tt.is_date = TRUE;
	} else {
		/* For DATE-TIME events, we have to convert to the same
		   timezone before comparing. */
		e_date_edit_get_time_of_day (E_DATE_EDIT (priv->start_time),
					     &start_tt.hour,
					     &start_tt.minute);
		e_date_edit_get_time_of_day (E_DATE_EDIT (priv->end_time),
					     &end_tt.hour,
					     &end_tt.minute);

		start_zone = e_timezone_entry_get_timezone (E_TIMEZONE_ENTRY (priv->start_timezone));

		if (check_start_before_end (&start_tt, start_zone,
					    &end_tt, start_zone,
					    adjust_end_time)) {
			if (adjust_end_time)
				set_end_date = TRUE;
			else
				set_start_date = TRUE;
		}
	}


	if (set_start_date) {
		g_signal_handlers_block_matched (priv->start_time, G_SIGNAL_MATCH_DATA, 0, 0, NULL, NULL, epage);
		e_date_edit_set_date (E_DATE_EDIT (priv->start_time),
				      start_tt.year, start_tt.month,
				      start_tt.day);
		e_date_edit_set_time_of_day (E_DATE_EDIT (priv->start_time),
					     start_tt.hour, start_tt.minute);
		g_signal_handlers_unblock_matched (priv->start_time, G_SIGNAL_MATCH_DATA, 0, 0, NULL, NULL, epage);
	}

	if (set_end_date) {
		g_signal_handlers_block_matched (priv->end_time, G_SIGNAL_MATCH_DATA, 0, 0, NULL, NULL, epage);
		e_date_edit_set_date (E_DATE_EDIT (priv->end_time),
				      end_tt.year, end_tt.month, end_tt.day);
		e_date_edit_set_time_of_day (E_DATE_EDIT (priv->end_time),
					     end_tt.hour, end_tt.minute);
		g_signal_handlers_unblock_matched (priv->end_time, G_SIGNAL_MATCH_DATA, 0, 0, NULL, NULL, epage);
	}

	/* Notify upstream */
	notify_dates_changed (epage, &start_tt, &end_tt);
}

/* Callback used when the start date widget change.  We check that the
 * start date < end date and we set the "all day event" button as appropriate.
 */
static void
start_date_changed_cb (GtkWidget *dedit, gpointer data)
{
	EventPage *epage;
	
	epage = EVENT_PAGE (data);

	hour_minute_changed (epage);

	times_updated (epage, TRUE);
}

/* Callback used when the end date widget change.  We check that the
 * start date < end date and we set the "all day event" button as appropriate.
 */
static void
end_date_changed_cb (GtkWidget *dedit, gpointer data)
{
	EventPage *epage;
	
	epage = EVENT_PAGE (data);

	times_updated (epage, FALSE);
}

/* Callback used when the start timezone is changed. If sync_timezones is set,
 * we set the end timezone to the same value. It also updates the start time
 * labels on the other notebook pages.
 */
static void
start_timezone_changed_cb (GtkWidget *widget, gpointer data)
{
	EventPage *epage;
	EventPagePrivate *priv;
	icaltimezone *zone;

	epage = EVENT_PAGE (data);
	priv = epage->priv;

	if (priv->sync_timezones) {
		zone = e_timezone_entry_get_timezone (E_TIMEZONE_ENTRY (priv->start_timezone));
		priv->updating = TRUE;
		e_timezone_entry_set_timezone (E_TIMEZONE_ENTRY (priv->end_timezone), zone);
		gtk_widget_show_all (priv->end_timezone);
		priv->updating = FALSE;
	}

	times_updated (epage, TRUE);
}

/* Callback used when the categories button is clicked; we must bring up the
 * category list dialog.
 */
static void
categories_clicked_cb (GtkWidget *button, gpointer data)
{
	EventPage *epage;
	EventPagePrivate *priv;
	GtkWidget *entry;

	epage = EVENT_PAGE (data);
	priv = epage->priv;

	entry = priv->categories;
	e_categories_config_open_dialog_for_entry (GTK_ENTRY (entry));
}

void
event_page_sendoptions_clicked_cb (EventPage *epage)
{
	EventPagePrivate *priv;
	GtkWidget *toplevel;
	ESource *source;

	priv = epage->priv;

	if (!priv->sod) {
		priv->sod = e_sendoptions_dialog_new ();
		source = e_source_option_menu_peek_selected  (E_SOURCE_OPTION_MENU (priv->source_selector));
		e_sendoptions_utils_set_default_data (priv->sod, source, "calendar");
		priv->sod->data->initialized = TRUE;
	}	

	if (e_cal_get_static_capability (COMP_EDITOR_PAGE (epage)->client, 
					 CAL_STATIC_CAPABILITY_NO_GEN_OPTIONS)) {
		e_sendoptions_set_need_general_options (priv->sod, FALSE);
	}

	toplevel = gtk_widget_get_toplevel (priv->main);
	e_sendoptions_dialog_run (priv->sod, toplevel, E_ITEM_CALENDAR);
}

/* This is called when any field is changed; it notifies upstream. */
static void
field_changed_cb (GtkWidget *widget, gpointer data)
{
	EventPage *epage;
	EventPagePrivate *priv;
	
	epage = EVENT_PAGE (data);
	priv = epage->priv;
	
	if (!priv->updating)
		comp_editor_page_notify_changed (COMP_EDITOR_PAGE (epage));
}

static void
source_changed_cb (GtkWidget *widget, ESource *source, gpointer data)
{
	EventPage *epage;
	EventPagePrivate *priv;

	epage = EVENT_PAGE (data);
	priv = epage->priv;

	if (!priv->updating) {
		ECal *client;

		client = auth_new_cal_from_source (source, E_CAL_SOURCE_TYPE_EVENT);
		if (client) {
			icaltimezone *zone;
			
			zone = calendar_config_get_icaltimezone ();
			e_cal_set_default_timezone (client, zone, NULL);
		}

		if (!client || !e_cal_open (client, FALSE, NULL)) {
			GtkWidget *dialog;

			if (client)
				g_object_unref (client);

			e_source_option_menu_select (E_SOURCE_OPTION_MENU (priv->source_selector),
						     e_cal_get_source (COMP_EDITOR_PAGE (epage)->client));

			dialog = gtk_message_dialog_new (NULL, GTK_DIALOG_MODAL,
							 GTK_MESSAGE_WARNING, GTK_BUTTONS_OK,
							 _("Unable to open the calendar '%s'."),
							 e_source_peek_name (source));
			gtk_dialog_run (GTK_DIALOG (dialog));
			gtk_widget_destroy (dialog);
		} else {
			comp_editor_notify_client_changed (
				COMP_EDITOR (gtk_widget_get_toplevel (priv->main)),
				client);
			if (e_cal_get_static_capability (client, CAL_STATIC_CAPABILITY_REQ_SEND_OPTIONS) && priv->is_meeting)
				event_page_show_options (epage);
			else
				event_page_hide_options (epage);

			sensitize_widgets (epage);
		}
	}
}

static void
alarm_changed_cb (GtkWidget *widget, gpointer data)
{
	EventPage *epage;
	EventPagePrivate *priv;
	
	epage = EVENT_PAGE (data);
	priv = epage->priv;

	if (e_dialog_toggle_get (priv->alarm)) {
		ECalComponentAlarm *ca;
		ECalComponentAlarmTrigger trigger;
		icalcomponent *icalcomp;
		icalproperty *icalprop;
		int alarm_type;
		
		ca = e_cal_component_alarm_new ();		
		
		e_cal_component_alarm_set_action (ca, E_CAL_COMPONENT_ALARM_DISPLAY);

		memset (&trigger, 0, sizeof (ECalComponentAlarmTrigger));
		trigger.type = E_CAL_COMPONENT_ALARM_TRIGGER_RELATIVE_START;		
		trigger.u.rel_duration.is_neg = 1;
		
		alarm_type = e_dialog_option_menu_get (priv->alarm_time, alarm_map);
		switch (alarm_type) {
		case ALARM_15_MINUTES:
			trigger.u.rel_duration.minutes = 15;
			break;
			
		case ALARM_1_HOUR:
			trigger.u.rel_duration.hours = 1;
			break;
			
		case ALARM_1_DAY:
			trigger.u.rel_duration.days = 1;
			break;

		case ALARM_USER_TIME:
			switch (calendar_config_get_default_reminder_units ()) {
			case CAL_DAYS:		
				trigger.u.rel_duration.days = priv->alarm_interval;
				break;
				
			case CAL_HOURS:
				trigger.u.rel_duration.hours = priv->alarm_interval;
				break;
				
			case CAL_MINUTES:
				trigger.u.rel_duration.minutes = priv->alarm_interval;
				break;
			}
			break;
			
		default:
			break;
		}		
		e_cal_component_alarm_set_trigger (ca, trigger);

		icalcomp = e_cal_component_alarm_get_icalcomponent (ca);
		icalprop = icalproperty_new_x ("1");
		icalproperty_set_x_name (icalprop, "X-EVOLUTION-NEEDS-DESCRIPTION");
		icalcomponent_add_property (icalcomp, icalprop);

		e_alarm_list_append (priv->alarm_list_store, NULL, ca);

		if (!priv->alarm_icon) {
			priv->alarm_icon = create_image_event_box ("stock_bell", "This event has alarms");
			gtk_box_pack_start ((GtkBox *)priv->status_icons, priv->alarm_icon, FALSE, FALSE, 3);
		}
	} else {
		e_alarm_list_clear (priv->alarm_list_store);
		if (priv->alarm_icon) {
			gtk_container_remove (GTK_CONTAINER (priv->status_icons), priv->alarm_icon);
			priv->alarm_icon = NULL;
		}
	}	
		
	sensitize_widgets (epage);	
}

static void
alarm_custom_clicked_cb (GtkWidget *widget, gpointer data)
{
	EventPage *epage;
	EventPagePrivate *priv;
	EAlarmList *temp_list_store;
	GtkTreeModel *model;
	GtkTreeIter iter;
	gboolean valid_iter;
	GtkWidget *toplevel;
	
	epage = EVENT_PAGE (data);
	priv = epage->priv;

	/* Make a copy of the list store in case the user cancels */
	temp_list_store = e_alarm_list_new ();
	model = GTK_TREE_MODEL (priv->alarm_list_store);
	
	for (valid_iter = gtk_tree_model_get_iter_first (model, &iter); valid_iter;
	     valid_iter = gtk_tree_model_iter_next (model, &iter)) {
		ECalComponentAlarm *alarm;
				
		alarm = (ECalComponentAlarm *) e_alarm_list_get_alarm (priv->alarm_list_store, &iter);
		g_assert (alarm != NULL);

		e_alarm_list_append (temp_list_store, NULL, alarm);
	}	
	
	toplevel = gtk_widget_get_toplevel (priv->main);
	if (alarm_list_dialog_run (toplevel, COMP_EDITOR_PAGE (epage)->client, temp_list_store)) {
		g_object_unref (priv->alarm_list_store);
		priv->alarm_list_store = temp_list_store;

		comp_editor_page_notify_changed (COMP_EDITOR_PAGE (epage));	
	} else {
		g_object_unref (temp_list_store);
	}	
	
	/* If the user erases everything, uncheck the alarm toggle */
	valid_iter = gtk_tree_model_get_iter_first (GTK_TREE_MODEL (priv->alarm_list_store), &iter);
	if (!valid_iter)
		e_dialog_toggle_set (priv->alarm, FALSE);

	sensitize_widgets (epage);
}

/* Hooks the widget signals */
static gboolean
init_widgets (EventPage *epage)
{
	EventPagePrivate *priv;
	GtkTextBuffer *text_buffer;
	icaltimezone *zone;
	char *menu_label = NULL;
	GtkTreeSelection *selection;	
	
	priv = epage->priv;

	/* Make sure the EDateEdit widgets use our timezones to get the
	   current time. */
	e_date_edit_set_get_time_callback (E_DATE_EDIT (priv->start_time),
					   (EDateEditGetTimeCallback) comp_editor_get_current_time,
					   epage, NULL);
	e_date_edit_set_get_time_callback (E_DATE_EDIT (priv->end_time),
					   (EDateEditGetTimeCallback) comp_editor_get_current_time,
					   epage, NULL);

	/* Summary */
	g_signal_connect((priv->summary), "changed",
			    G_CALLBACK (summary_changed_cb), epage);

	/* Description */
	text_buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (priv->description));

	gtk_text_view_set_wrap_mode (GTK_TEXT_VIEW (priv->description), GTK_WRAP_WORD);

	/* Start and end times */
	g_signal_connect((priv->start_time), "changed",
			    G_CALLBACK (start_date_changed_cb), epage);
	g_signal_connect((priv->end_time), "changed",
			    G_CALLBACK (end_date_changed_cb), epage);

	/* Categories */
	g_signal_connect((priv->categories_btn), "clicked",
			    G_CALLBACK (categories_clicked_cb), epage);
	
	/* Source selector */
	g_signal_connect((priv->source_selector), "source_selected",
			    G_CALLBACK (source_changed_cb), epage);
	/* Alarms */
	priv->alarm_list_store = e_alarm_list_new ();

	/* Timezone changed */
	g_signal_connect((priv->start_timezone), "changed",
			    G_CALLBACK (start_timezone_changed_cb), epage);

	e_meeting_list_view_column_set_visible (priv->list_view, "Attendee                          ", 
			TRUE);
	e_meeting_list_view_column_set_visible (priv->list_view, "Role", calendar_config_get_show_role());
	e_meeting_list_view_column_set_visible (priv->list_view, "RSVP", calendar_config_get_show_rsvp());
	e_meeting_list_view_column_set_visible (priv->list_view, "Status", calendar_config_get_show_status());
	e_meeting_list_view_column_set_visible (priv->list_view, "Type", calendar_config_get_show_type());
	
	selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (priv->list_view));
	gtk_tree_selection_set_mode (selection, GTK_SELECTION_MULTIPLE);

	g_signal_connect (G_OBJECT (priv->list_view), "button_press_event", G_CALLBACK (button_press_event), epage);
	g_signal_connect (G_OBJECT (priv->list_view), "event", G_CALLBACK (list_view_event), epage);
	g_signal_connect (priv->list_view, "key_press_event", G_CALLBACK (list_key_press), epage);	

	/* Add attendee button */
	g_signal_connect (priv->add, "clicked", G_CALLBACK (add_clicked_cb), epage);

	/* Remove attendee button */
	g_signal_connect (priv->remove, "clicked", G_CALLBACK (remove_clicked_cb), epage);

	/* Contacts button */
	g_signal_connect(priv->invite, "clicked", G_CALLBACK (invite_cb), epage);	

	/* Alarm dialog */
	g_signal_connect (GTK_DIALOG (priv->alarm_dialog), "response", G_CALLBACK (gtk_widget_hide), priv->alarm_dialog);
	gtk_widget_hide (priv->alarm_dialog);
	gtk_window_set_modal (GTK_WINDOW (priv->alarm_dialog), TRUE);

	/* Meeting List View */
	g_signal_connect (priv->list_view, "attendee_added", G_CALLBACK (attendee_added_cb), epage);	

	gtk_widget_show (GTK_WIDGET (priv->list_view));

	/* categories */
	if (!calendar_config_get_show_categories()) {
		gtk_widget_hide (priv->categories_btn);
		gtk_widget_hide (priv->categories);
	} else {
		gtk_widget_show (priv->categories_btn);
		gtk_widget_show (priv->categories);
	}	

	/* End time selector */
	gtk_option_menu_set_history (GTK_OPTION_MENU (priv->end_time_selector), 1);
	gtk_widget_hide (priv->time_hour);
	gtk_widget_show (priv->end_time);
	g_signal_connect (priv->end_time_selector, "changed", G_CALLBACK (time_sel_changed), epage);
	update_end_time_selector ( epage);

	/* Hour and Minute selector */
	gtk_spin_button_set_range( (GtkSpinButton*) priv->hour_selector, 0, G_MAXINT);
	g_signal_connect (priv->hour_selector, "value-changed", G_CALLBACK (hour_sel_changed), epage);
	g_signal_connect (priv->minute_selector, "value-changed", G_CALLBACK (minute_sel_changed), epage);

	/* Add the user defined time if necessary */
	priv->alarm_units = calendar_config_get_default_reminder_units ();
	priv->alarm_interval = calendar_config_get_default_reminder_interval ();
	
	switch (priv->alarm_units) {
	case CAL_DAYS:
		if (priv->alarm_interval != 1) {
			menu_label = g_strdup_printf (ngettext("%d day before appointment", "%d days before appointment", priv->alarm_interval), priv->alarm_interval);
		} else {
			priv->alarm_interval = -1;
		}
		break;
		
	case CAL_HOURS:
		if (priv->alarm_interval != 1) {
			menu_label = g_strdup_printf (ngettext("%d hour before appointment", "%d hours before appointment", priv->alarm_interval), priv->alarm_interval);
		} else {
			priv->alarm_interval = -1;
		}
		break;
		
	case CAL_MINUTES:
		if (priv->alarm_interval != 15) {
			menu_label = g_strdup_printf (ngettext("%d minute before appointement", "%d minutes before appointment", priv->alarm_interval), priv->alarm_interval);
		} else {
			priv->alarm_interval = -1;
		}
		break;
	}
	
	if (menu_label) {
		GtkWidget *item, *menu;

		item = gtk_menu_item_new_with_label (menu_label);
		gtk_widget_show (item);
		menu = gtk_option_menu_get_menu (GTK_OPTION_MENU (priv->alarm_time));
		gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
	}
	
	g_signal_connect (priv->alarm,
			  "toggled", G_CALLBACK (field_changed_cb),
			  epage);
	g_signal_connect (priv->alarm_time, "changed",
			  G_CALLBACK (field_changed_cb), epage);
	g_signal_connect (priv->alarm_custom, "clicked",
			  G_CALLBACK (alarm_custom_clicked_cb), epage);

	/* Belongs to priv->description */
	g_signal_connect((text_buffer), "changed",
			    G_CALLBACK (field_changed_cb), epage);

	g_signal_connect((priv->summary), "changed",
			    G_CALLBACK (field_changed_cb), epage);
	g_signal_connect((priv->location), "changed",
			    G_CALLBACK (field_changed_cb), epage);
	g_signal_connect((priv->start_time), "changed",
			    G_CALLBACK (field_changed_cb), epage);
	g_signal_connect((priv->end_time), "changed",
			    G_CALLBACK (field_changed_cb), epage);
	g_signal_connect((priv->categories), "changed",
			    G_CALLBACK (field_changed_cb), epage);

	g_signal_connect (priv->alarm,
			  "toggled", G_CALLBACK (alarm_changed_cb),
			  epage);

	/* Set the default timezone, so the timezone entry may be hidden. */
	zone = calendar_config_get_icaltimezone ();
	e_timezone_entry_set_default_timezone (E_TIMEZONE_ENTRY (priv->start_timezone), zone);
	e_timezone_entry_set_default_timezone (E_TIMEZONE_ENTRY (priv->end_timezone), zone);

	event_page_set_show_timezone (epage, calendar_config_get_show_timezone());

	return TRUE;
}



/**
 * event_page_construct:
 * @epage: An event page.
 * 
 * Constructs an event page by loading its Glade data.
 * 
 * Return value: The same object as @epage, or NULL if the widgets could not be
 * created.
 **/
EventPage *
event_page_construct (EventPage *epage, EMeetingStore *model, ECal *client)
{
	EventPagePrivate *priv;
	char *backend_address = NULL;
	EIterator *it;
	EAccount *def_account;
	GList *address_strings = NULL, *l;
	EAccount *a;
	char *gladefile;

	priv = epage->priv;
	g_object_ref (model);
	priv->model = model;
	priv->client = client;

	gladefile = g_build_filename (EVOLUTION_GLADEDIR,
				      "event-page.glade",
				      NULL);
	priv->xml = glade_xml_new (gladefile, NULL, NULL);
	g_free (gladefile);

	if (!priv->xml) {
		g_message ("event_page_construct(): " 
			   "Could not load the Glade XML file!");
		return NULL;
	}

	if (!get_widgets (epage)) {
		g_message ("event_page_construct(): " 
			   "Could not find all widgets in the XML file!");
		return NULL;
	}

	/* Address information */
	if (!e_cal_get_cal_address (client, &backend_address, NULL))
		return NULL;

	priv->accounts = itip_addresses_get ();
	def_account = itip_addresses_get_default();
	for (it = e_list_get_iterator((EList *)priv->accounts);
	     e_iterator_is_valid(it);
	     e_iterator_next(it)) {
		a = (EAccount *)e_iterator_get(it);
		char *full;
		
		full = g_strdup_printf("%s <%s>", a->id->name, a->id->address);

		address_strings = g_list_append(address_strings, full);

		/* Note that the address specified by the backend gets
		 * precedence over the default mail address.
		 */
		if (backend_address && !strcmp (backend_address, a->id->address)) {
			if (priv->default_address)
				g_free (priv->default_address);
			
			priv->default_address = g_strdup (full);
		} else if (a == def_account && !priv->default_address) {
			priv->default_address = g_strdup (full);
		}
	}
	
	if (backend_address)
		g_free (backend_address);

	g_object_unref(it);

	if (address_strings)
		gtk_combo_set_popdown_strings (GTK_COMBO (priv->organizer), address_strings);
	else
		g_warning ("No potential organizers!");

	for (l = address_strings; l != NULL; l = l->next)
		g_free (l->data);
	g_list_free (address_strings);
	

	if (!init_widgets (epage)) {
		g_message ("event_page_construct(): " 
			   "Could not initialize the widgets!");
		return NULL;
	}

	return epage;
}

/**
 * event_page_new:
 * 
 * Creates a new event page.
 * 
 * Return value: A newly-created event page, or NULL if the page could
 * not be created.
 **/
EventPage *
event_page_new (EMeetingStore *model, ECal *client, BonoboUIComponent *uic)
{
	EventPage *epage;

	epage = g_object_new (TYPE_EVENT_PAGE, NULL);
	if (!event_page_construct (epage, model, client)) {
		g_object_unref (epage);
		return NULL;
	}

	epage->priv->uic = uic;

	return epage;
}

GtkWidget *make_date_edit (void);

GtkWidget *
make_date_edit (void)
{
	return comp_editor_new_date_edit (TRUE, TRUE, TRUE);
}

GtkWidget *make_timezone_entry (void);

GtkWidget *
make_timezone_entry (void)
{
	GtkWidget *w;

	w = e_timezone_entry_new ();
	gtk_widget_show (w);
	return w;
}

GtkWidget *event_page_create_source_option_menu (void);

GtkWidget *
event_page_create_source_option_menu (void)
{
	GtkWidget   *menu;
	GConfClient *gconf_client;
	ESourceList *source_list;

	gconf_client = gconf_client_get_default ();
	source_list = e_source_list_new_for_gconf (gconf_client, "/apps/evolution/calendar/sources");

	menu = e_source_option_menu_new (source_list);
	g_object_unref (source_list);

	gtk_widget_show (menu);
	return menu;
}

GtkWidget *make_status_icons (void);

GtkWidget *
make_status_icons (void)
{
	return gtk_hbox_new (FALSE, 2);
}

static void
set_attendees (ECalComponent *comp, const GPtrArray *attendees)
{
	GSList *comp_attendees = NULL, *l;
	int i;
	
	for (i = 0; i < attendees->len; i++) {
		EMeetingAttendee *ia = g_ptr_array_index (attendees, i);
		ECalComponentAttendee *ca;
		
		ca = e_meeting_attendee_as_e_cal_component_attendee (ia);
		
		comp_attendees = g_slist_prepend (comp_attendees, ca);
		
	}
	comp_attendees = g_slist_reverse (comp_attendees);
	
	e_cal_component_set_attendee_list (comp, comp_attendees);
	
	for (l = comp_attendees; l != NULL; l = l->next)
		g_free (l->data);	
	g_slist_free (comp_attendees);
}

ECalComponent *
event_page_get_cancel_comp (EventPage *page)
{
	EventPagePrivate *priv;

	g_return_val_if_fail (page != NULL, NULL);
	g_return_val_if_fail (IS_EVENT_PAGE (page), NULL);

	priv = page->priv;

	if (priv->deleted_attendees->len == 0)
		return NULL;
	
	set_attendees (priv->comp, priv->deleted_attendees);
	
	return e_cal_component_clone (priv->comp);
}
