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
#include <libgnome/gnome-i18n.h>
#include <glade/glade.h>
#include <gal/widgets/e-categories.h>
#include <libedataserverui/e-source-option-menu.h>
#include "common/authentication.h"
#include "e-util/e-categories-config.h"
#include "e-util/e-dialog-widgets.h"
#include "widgets/misc/e-dateedit.h"
#include <libecal/e-cal-time-util.h>
#include "../calendar-config.h"
#include "../e-timezone-entry.h"
#include "comp-editor.h"
#include "comp-editor-util.h"
#include "../e-alarm-list.h"
#include "alarm-list-dialog.h"
#include "event-page.h"



/* Private part of the EventPage structure */
struct _EventPagePrivate {
	/* Glade XML data */
	GladeXML *xml;

	/* Widgets from the Glade file */

	GtkWidget *main;

	GtkWidget *summary;
	GtkWidget *summary_label;
	GtkWidget *location;
	GtkWidget *location_label;

	GtkWidget *start_time;
	GtkWidget *end_time;
	GtkWidget *start_timezone;
	GtkWidget *end_timezone;
	GtkWidget *all_day_event;

	GtkWidget *description;

	GtkWidget *classification;
	
	GtkWidget *show_time_as_busy;

	GtkWidget *alarm;
	GtkWidget *alarm_time;
	GtkWidget *alarm_warning;
	GtkWidget *alarm_custom;

	GtkWidget *categories_btn;
	GtkWidget *categories;

	GtkWidget *source_selector;

	EAlarmList *alarm_list_store;
	
	gboolean updating;

	char *old_summary;
	CalUnits alarm_units;
	int alarm_interval;
	
	/* This is TRUE if both the start & end timezone are the same. If the
	   start timezone is then changed, we updated the end timezone to the
	   same value, since 99% of events start and end in one timezone. */
	gboolean sync_timezones;
};



static void event_page_finalize (GObject *object);

static GtkWidget *event_page_get_widget (CompEditorPage *page);
static void event_page_focus_main_widget (CompEditorPage *page);
static gboolean event_page_fill_widgets (CompEditorPage *page, ECalComponent *comp);
static gboolean event_page_fill_component (CompEditorPage *page, ECalComponent *comp);
static gboolean event_page_fill_timezones (CompEditorPage *page, GHashTable *timezones);
static void event_page_set_summary (CompEditorPage *page, const char *summary);
static void event_page_set_dates (CompEditorPage *page, CompEditorPageDates *dates);

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

	priv->main = NULL;
	priv->summary = NULL;
	priv->summary_label = NULL;
	priv->location = NULL;
	priv->location_label = NULL;
	priv->start_time = NULL;
	priv->end_time = NULL;
	priv->start_timezone = NULL;
	priv->end_timezone = NULL;
	priv->all_day_event = NULL;
	priv->description = NULL;
	priv->classification = NULL;
	priv->show_time_as_busy = NULL;
	priv->alarm = NULL;
	priv->alarm_time = NULL;
	priv->alarm_custom = NULL;
	priv->categories_btn = NULL;
	priv->categories = NULL;

	priv->alarm_interval =  -1;
	
	priv->updating = FALSE;
	priv->sync_timezones = FALSE;
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

	gtk_signal_handler_block_by_data (GTK_OBJECT (priv->all_day_event),
					  epage);
	e_dialog_toggle_set (priv->all_day_event, all_day);
	gtk_signal_handler_unblock_by_data (GTK_OBJECT (priv->all_day_event),
					    epage);

	e_date_edit_set_show_time (E_DATE_EDIT (priv->start_time), !all_day);
	e_date_edit_set_show_time (E_DATE_EDIT (priv->end_time), !all_day);

	/* DATE values do not have timezones, so we hide the fields. */
	if (all_day) {
		gtk_widget_hide (priv->start_timezone);
		gtk_widget_hide (priv->end_timezone);
	} else {
		gtk_widget_show (priv->start_timezone);
		gtk_widget_show (priv->end_timezone);
	}
}

static void
update_time (EventPage *epage, ECalComponentDateTime *start_date, ECalComponentDateTime *end_date)
{
	EventPagePrivate *priv;
	struct icaltimetype *start_tt, *end_tt, implied_tt;
	icaltimezone *start_zone = NULL, *end_zone = NULL;
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

	end_zone = icaltimezone_get_builtin_timezone_from_tzid (end_date->tzid);
	if (!end_zone) {
		if (!e_cal_get_timezone (COMP_EDITOR_PAGE (epage)->client,
					      end_date->tzid, &end_zone, NULL)) {
			/* FIXME: Handle error better. */
			g_warning ("Couldn't get timezone from server: %s",
				   end_date->tzid ? end_date->tzid : "");
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

	set_all_day (epage, all_day_event);

	/* If it is an all day event, we set both timezones to the current
	   timezone, so that if the user toggles the 'All Day Event' checkbox
	   the event uses the current timezone rather than none at all. */
	if (all_day_event)
		start_zone = end_zone = calendar_config_get_icaltimezone ();

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
	e_timezone_entry_set_timezone (E_TIMEZONE_ENTRY (priv->end_timezone),
				       end_zone);

	gtk_signal_handler_unblock_by_data (GTK_OBJECT (priv->start_timezone),
					    epage);
	gtk_signal_handler_unblock_by_data (GTK_OBJECT (priv->end_timezone),
					    epage);

	priv->sync_timezones = (start_zone == end_zone) ? TRUE : FALSE;

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

	set_all_day (epage, FALSE);

	/* Classification */
	e_dialog_option_menu_set (priv->classification, E_CAL_COMPONENT_CLASS_PRIVATE, classification_map);

	/* Show Time As (Transparency) */
	e_dialog_toggle_set (priv->show_time_as_busy, TRUE);

	/* Alarm */
	e_dialog_toggle_set (priv->alarm, FALSE);
	e_dialog_option_menu_set (priv->alarm_time, ALARM_15_MINUTES, alarm_map);
	
	/* Categories */
	e_dialog_editable_set (priv->categories, NULL);
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

static void
sensitize_widgets (EventPage *epage)
{
	gboolean read_only, custom, alarm;
	EventPagePrivate *priv;
	
	priv = epage->priv;

	if (!e_cal_is_read_only (COMP_EDITOR_PAGE (epage)->client, &read_only, NULL))
		read_only = TRUE;

	custom = is_custom_alarm_store (priv->alarm_list_store, priv->old_summary, priv->alarm_units, priv->alarm_interval, NULL);
	alarm = e_dialog_toggle_get (priv->alarm);
	
	gtk_widget_set_sensitive (priv->summary_label, !read_only);
	gtk_entry_set_editable (GTK_ENTRY (priv->summary), !read_only);
	gtk_widget_set_sensitive (priv->location_label, !read_only);
	gtk_entry_set_editable (GTK_ENTRY (priv->location), !read_only);
	gtk_widget_set_sensitive (priv->start_time, !read_only);
	gtk_widget_set_sensitive (priv->start_timezone, !read_only);
	gtk_widget_set_sensitive (priv->end_time, !read_only);
	gtk_widget_set_sensitive (priv->end_timezone, !read_only);
	gtk_widget_set_sensitive (priv->all_day_event, !read_only);
	gtk_widget_set_sensitive (priv->description, !read_only);
	gtk_widget_set_sensitive (priv->classification, !read_only);
	gtk_widget_set_sensitive (priv->show_time_as_busy, !read_only);
	gtk_widget_set_sensitive (priv->alarm, !read_only);
	gtk_widget_set_sensitive (priv->alarm_time, !read_only && !custom && alarm);
	gtk_widget_set_sensitive (priv->alarm_custom, alarm);
	if (custom)
		gtk_widget_show (priv->alarm_warning);
	else
		gtk_widget_hide (priv->alarm_warning);
	gtk_widget_set_sensitive (priv->categories_btn, !read_only);
	gtk_entry_set_editable (GTK_ENTRY (priv->categories), !read_only);
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
	const char *location;
	const char *categories;
	ESource *source;
	GSList *l;
	gboolean validated = TRUE;
	
	g_return_val_if_fail (page->client != NULL, FALSE);

	epage = EVENT_PAGE (page);
	priv = epage->priv;

	/* Don't send off changes during this time */
	priv->updating = TRUE;

	/* Clean the page */
	clear_widgets (epage);

	/* Summary, location, description(s) */

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
	e_dialog_option_menu_set (priv->classification, cl, classification_map);

	/* Show Time As (Transparency) */
	e_cal_component_get_transparency (comp, &transparency);
	switch (transparency) {
	case E_CAL_COMPONENT_TRANSP_TRANSPARENT:
		e_dialog_toggle_set (priv->show_time_as_busy, FALSE);
		break;

	default:
		e_dialog_toggle_set (priv->show_time_as_busy, TRUE);
		break;
	}
	if (e_cal_get_static_capability (page->client, CAL_STATIC_CAPABILITY_NO_TRANSPARENCY))
		gtk_widget_hide (priv->show_time_as_busy);
	else
		gtk_widget_show (priv->show_time_as_busy);

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
	ECalComponentClassification classif;
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
	all_day_event = e_dialog_toggle_get (priv->all_day_event);

	if (all_day_event) {
		start_tt.is_date = TRUE;
		end_tt.is_date = TRUE;

		/* We have to add 1 day to DTEND, as it is not inclusive. */
		icaltime_adjust (&end_tt, 1, 0, 0, 0);
	} else {
		icaltimezone *start_zone, *end_zone;

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
		end_zone = e_timezone_entry_get_timezone (E_TIMEZONE_ENTRY (priv->end_timezone));
		end_date.tzid = icaltimezone_get_tzid (end_zone);
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
	classif = e_dialog_option_menu_get (priv->classification, classification_map);
	e_cal_component_set_classification (comp, classif);

	/* Show Time As (Transparency) */
	busy = e_dialog_toggle_get (priv->show_time_as_busy);
	e_cal_component_set_transparency (comp, busy ? E_CAL_COMPONENT_TRANSP_OPAQUE : E_CAL_COMPONENT_TRANSP_TRANSPARENT);

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



/* Gets the widgets from the XML file and returns if they are all available. */
static gboolean
get_widgets (EventPage *epage)
{
	CompEditorPage *page = COMP_EDITOR_PAGE (epage);
	EventPagePrivate *priv;
	GSList *accel_groups;
	GtkWidget *toplevel;

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

	gtk_widget_ref (priv->main);
	gtk_container_remove (GTK_CONTAINER (priv->main->parent), priv->main);

	priv->summary = GW ("summary");
	priv->summary_label = GW ("summary-label");
	priv->location = GW ("location");
	priv->location_label = GW ("location-label");

	/* Glade's visibility flag doesn't seem to work for custom widgets */
	priv->start_time = GW ("start-time");
	gtk_widget_show (priv->start_time);
	priv->end_time = GW ("end-time");
	gtk_widget_show (priv->end_time);

	priv->start_timezone = GW ("start-timezone");
	priv->end_timezone = GW ("end-timezone");
	priv->all_day_event = GW ("all-day-event");

	priv->description = GW ("description");

	priv->classification = GW ("classification");

	priv->show_time_as_busy = GW ("show-time-as-busy");

	priv->alarm = GW ("alarm");
	priv->alarm_time = GW ("alarm-time");
	priv->alarm_warning = GW ("alarm-warning");
	priv->alarm_custom = GW ("alarm-custom");

	priv->categories_btn = GW ("categories-button");
	priv->categories = GW ("categories");

	priv->source_selector = GW ("source");

#undef GW

	return (priv->summary
		&& priv->location
		&& priv->start_time
		&& priv->end_time
		&& priv->start_timezone
		&& priv->end_timezone
		&& priv->all_day_event
		&& priv->description
		&& priv->classification
		&& priv->show_time_as_busy
		&& priv->alarm
		&& priv->alarm_time
		&& priv->alarm_warning
		&& priv->alarm_custom
		&& priv->categories_btn
		&& priv->categories);
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
	icaltimezone *start_zone = NULL, *end_zone = NULL;

	priv = epage->priv;
	
	all_day_event = e_dialog_toggle_get (priv->all_day_event);

	start_dt.value = start_tt;
	end_dt.value = end_tt;

	if (all_day_event) {
		/* The actual DTEND is 1 day after the displayed date for
		   DATE values. */
		icaltime_adjust (end_tt, 1, 0, 0, 0);
	} else {
		start_zone = e_timezone_entry_get_timezone (E_TIMEZONE_ENTRY (priv->start_timezone));
		end_zone = e_timezone_entry_get_timezone (E_TIMEZONE_ENTRY (priv->end_timezone));
	}

	start_dt.tzid = start_zone ? icaltimezone_get_tzid (start_zone) : NULL;
	end_dt.tzid = end_zone ? icaltimezone_get_tzid (end_zone) : NULL;

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
	icaltimezone *start_zone, *end_zone;
	
	priv = epage->priv;

	if (priv->updating)
		return;

	/* Fetch the start and end times and timezones from the widgets. */
	all_day_event = e_dialog_toggle_get (priv->all_day_event);

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
		end_zone = e_timezone_entry_get_timezone (E_TIMEZONE_ENTRY (priv->end_timezone));

		if (check_start_before_end (&start_tt, start_zone,
					    &end_tt, end_zone,
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

/* Callback used when the start or end date widgets change.  We check that the
 * start date < end date and we set the "all day event" button as appropriate.
 */
static void
date_changed_cb (GtkWidget *dedit, gpointer data)
{
	EventPage *epage;
	
	epage = EVENT_PAGE (data);

	times_updated (epage, dedit == epage->priv->start_time);
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
		priv->updating = FALSE;
	}

	times_updated (epage, TRUE);
}


/* Callback used when the end timezone is changed. It checks if the end
 * timezone is the same as the start timezone and sets sync_timezones if so.
 */
static void
end_timezone_changed_cb (GtkWidget *widget, gpointer data)
{
	EventPage *epage;
	EventPagePrivate *priv;
	icaltimezone *start_zone, *end_zone;
	
	epage = EVENT_PAGE (data);
	priv = epage->priv;

	start_zone = e_timezone_entry_get_timezone (E_TIMEZONE_ENTRY (priv->start_timezone));
	end_zone = e_timezone_entry_get_timezone (E_TIMEZONE_ENTRY (priv->end_timezone));

	priv->sync_timezones = (start_zone == end_zone) ? TRUE : FALSE;

	times_updated (epage, TRUE);
}

/* Callback: all day event button toggled.
 * Note that this should only be called when the user explicitly toggles the
 * button. Be sure to block this handler when the toggle button's state is set
 * within the code.
 */
static void
all_day_event_toggled_cb (GtkWidget *toggle, gpointer data)
{
	EventPage *epage;
	EventPagePrivate *priv;
	gboolean all_day;
	struct icaltimetype start_tt = icaltime_null_time();
	struct icaltimetype end_tt = icaltime_null_time();
	gboolean date_set;

	epage = EVENT_PAGE (data);
	priv = epage->priv;

	/* When the all_day toggle is turned on, the start date is
	 * rounded down to the start of the day, and end date is
	 * rounded down to the start of the day on which the event
	 * ends. The event is then taken to be inclusive of the days
	 * between the start and end days.  Note that if the event end
	 * is at midnight, we round it down to the previous day, so the
	 * event times stay the same.
	 *
	 * When the all_day_toggle is turned off, then if the event is within
	 * one day, we set the event start to the start of the working day,
	 * and set the event end to one hour after it. If the event is longer
	 * than one day, we set the event end to the end of the day it is on,
	 * so that the actual event times remain the same.
	 *
	 * This may need tweaking to work well with different timezones used
	 * in the event start & end.
	 */
	all_day = GTK_TOGGLE_BUTTON (toggle)->active;

	set_all_day (epage, all_day);

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

	if (all_day) {
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
		icaltimezone *start_zone, *end_zone;

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
		end_zone = e_timezone_entry_get_timezone (E_TIMEZONE_ENTRY (priv->end_timezone));
		check_start_before_end (&start_tt, start_zone,
					&end_tt, end_zone,
					TRUE);
	}

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
			icaltimezone *zone;
			
			zone = calendar_config_get_icaltimezone ();
			e_cal_set_default_timezone (client, zone, NULL);

			comp_editor_notify_client_changed (
				COMP_EDITOR (gtk_widget_get_toplevel (priv->main)),
				client);
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
	} else {
		e_alarm_list_clear (priv->alarm_list_store);
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
			    G_CALLBACK (date_changed_cb), epage);
	g_signal_connect((priv->end_time), "changed",
			    G_CALLBACK (date_changed_cb), epage);

	g_signal_connect((priv->start_timezone), "changed",
			    G_CALLBACK (start_timezone_changed_cb), epage);
	g_signal_connect((priv->end_timezone), "changed",
			    G_CALLBACK (end_timezone_changed_cb), epage);

	g_signal_connect((priv->all_day_event), "toggled",
			    G_CALLBACK (all_day_event_toggled_cb), epage);

	/* Categories button */
	g_signal_connect((priv->categories_btn), "clicked",
			    G_CALLBACK (categories_clicked_cb), epage);

	/* Source selector */
	g_signal_connect((priv->source_selector), "source_selected",
			    G_CALLBACK (source_changed_cb), epage);

	/* Alarms */
	priv->alarm_list_store = e_alarm_list_new ();

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
			  "toggled", G_CALLBACK (alarm_changed_cb),
			  epage);
	g_signal_connect (priv->alarm_custom, "clicked",
			  G_CALLBACK (alarm_custom_clicked_cb), epage);

	/* Connect the default signal handler to use to make sure we notify
	 * upstream of changes to the widget values.
	 */

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
	g_signal_connect((priv->start_timezone), "changed",
			    G_CALLBACK (field_changed_cb), epage);
	g_signal_connect((priv->end_timezone), "changed",
			    G_CALLBACK (field_changed_cb), epage);
	g_signal_connect((priv->all_day_event), "toggled",
			    G_CALLBACK (field_changed_cb), epage);
	g_signal_connect((priv->classification),
			    "changed", G_CALLBACK (field_changed_cb),
			    epage);
	g_signal_connect((priv->show_time_as_busy),
			    "toggled", G_CALLBACK (field_changed_cb),
			    epage);
	g_signal_connect((priv->alarm),
			    "toggled", G_CALLBACK (field_changed_cb),
			    epage);
	g_signal_connect((priv->alarm_time),
			    "changed", G_CALLBACK (field_changed_cb),
			    epage);
	g_signal_connect((priv->categories), "changed",
			    G_CALLBACK (field_changed_cb), epage);

	/* Set the default timezone, so the timezone entry may be hidden. */
	zone = calendar_config_get_icaltimezone ();
	e_timezone_entry_set_default_timezone (E_TIMEZONE_ENTRY (priv->start_timezone), zone);
	e_timezone_entry_set_default_timezone (E_TIMEZONE_ENTRY (priv->end_timezone), zone);

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
event_page_construct (EventPage *epage)
{
	EventPagePrivate *priv;

	priv = epage->priv;

	priv->xml = glade_xml_new (EVOLUTION_GLADEDIR "/event-page.glade", 
				   NULL, NULL);
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
event_page_new (void)
{
	EventPage *epage;

	epage = g_object_new (TYPE_EVENT_PAGE, NULL);
	if (!event_page_construct (epage)) {
		g_object_unref (epage);
		return NULL;
	}

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
