/*
 * Evolution calendar - Main page of the event editor dialog
 *
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
#include <glade/glade.h>
#include <gdk/gdkkeysyms.h>
#include <libedataserverui/e-category-completion.h>
#include <libedataserverui/e-source-combo-box.h>
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

#define EVENT_PAGE_GET_PRIVATE(obj) \
	(G_TYPE_INSTANCE_GET_PRIVATE \
	((obj), TYPE_EVENT_PAGE, EventPagePrivate))

enum {
	ALARM_NONE,
	ALARM_15_MINUTES,
	ALARM_1_HOUR,
	ALARM_1_DAY,
	ALARM_USER_TIME,
	ALARM_CUSTOM
};

static const gint alarm_map_with_user_time[] = {
	ALARM_NONE,
	ALARM_15_MINUTES,
	ALARM_1_HOUR,
	ALARM_1_DAY,
	ALARM_USER_TIME,
	ALARM_CUSTOM,
	-1
};

static const gint alarm_map_without_user_time[] = {
	ALARM_NONE,
	ALARM_15_MINUTES,
	ALARM_1_HOUR,
	ALARM_1_DAY,
	ALARM_CUSTOM,
	-1
};

/* Private part of the EventPage structure */
struct _EventPagePrivate {
	/* Glade XML data */
	GladeXML *xml;

	/* Widgets from the Glade file */
	GtkWidget *main;

	/* Generic informative messages placeholder */
	GtkWidget *info_hbox;
	GtkWidget *info_icon;
	GtkWidget *info_string;
	gchar *subscriber_info_text;

	GtkWidget *summary;
	GtkWidget *summary_label;
	GtkWidget *location;
	GtkWidget *location_label;

	EAccountList *accounts;
	GList *address_strings;
	EMeetingAttendee *ia;
	gchar *user_add;
	ECalComponent *comp;

	/* For meeting/event */
	GtkWidget *calendar_label;
	GtkWidget *org_cal_label;
	GtkWidget *attendee_box;

	/* Lists of attendees */
	GPtrArray *deleted_attendees;

	GtkWidget *start_time;
	GtkWidget *end_time;
	GtkWidget *end_time_combo;
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

	gboolean  show_time_as_busy;

	GtkWidget *alarm_dialog;
	GtkWidget *alarm_time_combo;
	GtkWidget *alarm_warning;
	GtkWidget *alarm_box;

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
	GtkWidget *invite_label;
	GtkWidget *attendees_label;

	/* ListView stuff */
	EMeetingStore *model;
	EMeetingListView *list_view;
	gint row;

	/* For handling who the organizer is */
	gboolean user_org;
	gboolean existing;

	EAlarmList *alarm_list_store;

	gboolean sendoptions_shown;

	ESendOptionsDialog *sod;
	gchar *old_summary;
	CalUnits alarm_units;
	gint alarm_interval;

	/* This is TRUE if both the start & end timezone are the same. If the
	   start timezone is then changed, we updated the end timezone to the
	   same value, since 99% of events start and end in one timezone. */
	gboolean sync_timezones;
	gboolean is_meeting;

	GtkWidget *alarm_list_dlg_widget;

	/* either with-user-time or without it */
	const gint *alarm_map;
};

static GtkWidget *event_page_get_widget (CompEditorPage *page);
static void event_page_focus_main_widget (CompEditorPage *page);
static gboolean event_page_fill_widgets (CompEditorPage *page, ECalComponent *comp);
static gboolean event_page_fill_component (CompEditorPage *page, ECalComponent *comp);
static gboolean event_page_fill_timezones (CompEditorPage *page, GHashTable *timezones);
static void event_page_set_dates (CompEditorPage *page, CompEditorPageDates *dates);
static void notify_dates_changed (EventPage *epage, struct icaltimetype *start_tt, struct icaltimetype *end_tt);
static gboolean check_start_before_end (struct icaltimetype *start_tt, icaltimezone *start_zone,
					struct icaltimetype *end_tt, icaltimezone *end_zone, gboolean adjust_end_time);
static void set_attendees (ECalComponent *comp, const GPtrArray *attendees);
static void hour_sel_changed ( GtkSpinButton *widget, EventPage *epage);
static void minute_sel_changed ( GtkSpinButton *widget, EventPage *epage);
static void hour_minute_changed ( EventPage *epage);
static void update_end_time_combo ( EventPage *epage);
static void event_page_select_organizer (EventPage *epage, const gchar *backend_address);
static void set_subscriber_info_string (EventPage *epage, const gchar *backend_address);

G_DEFINE_TYPE (EventPage, event_page, TYPE_COMP_EDITOR_PAGE)

static void
event_page_dispose (GObject *object)
{
	EventPagePrivate *priv;

	priv = EVENT_PAGE_GET_PRIVATE (object);

	if (priv->comp != NULL) {
		g_object_unref (priv->comp);
		priv->comp = NULL;
	}

	if (priv->main != NULL) {
		g_object_unref (priv->main);
		priv->main = NULL;
	}

	if (priv->xml != NULL) {
		g_object_unref (priv->xml);
		priv->xml = NULL;
	}

	if (priv->alarm_list_store != NULL) {
		g_object_unref (priv->alarm_list_store);
		priv->alarm_list_store = NULL;
	}

	if (priv->sod != NULL) {
		g_object_unref (priv->sod);
		priv->sod = NULL;
	}

	/* Chain up to parent's dispose() method. */
	G_OBJECT_CLASS (event_page_parent_class)->dispose (object);
}

static void
event_page_finalize (GObject *object)
{
	EventPagePrivate *priv;

	priv = EVENT_PAGE_GET_PRIVATE (object);

	g_list_foreach (priv->address_strings, (GFunc) g_free, NULL);
	g_list_free (priv->address_strings);

	g_ptr_array_foreach (
		priv->deleted_attendees, (GFunc) g_object_unref, NULL);
	g_ptr_array_free (priv->deleted_attendees, TRUE);

	g_free (priv->old_summary);
	g_free (priv->subscriber_info_text);

	priv->alarm_list_dlg_widget = NULL;

	/* Chain up to parent's finalize() method. */
	G_OBJECT_CLASS (event_page_parent_class)->finalize (object);
}

static void
event_page_class_init (EventPageClass *class)
{
	GObjectClass *object_class;
	CompEditorPageClass *editor_page_class;

	g_type_class_add_private (class, sizeof (EventPagePrivate));

	object_class = G_OBJECT_CLASS (class);
	object_class->dispose = event_page_dispose;
	object_class->finalize = event_page_finalize;

	editor_page_class = COMP_EDITOR_PAGE_CLASS (class);
	editor_page_class->get_widget = event_page_get_widget;
	editor_page_class->focus_main_widget = event_page_focus_main_widget;
	editor_page_class->fill_widgets = event_page_fill_widgets;
	editor_page_class->fill_component = event_page_fill_component;
	editor_page_class->fill_timezones = event_page_fill_timezones;
	editor_page_class->set_dates = event_page_set_dates;
}

static void
event_page_init (EventPage *epage)
{
	epage->priv = EVENT_PAGE_GET_PRIVATE (epage);
	epage->priv->deleted_attendees = g_ptr_array_new ();
	epage->priv->alarm_interval = -1;
	epage->priv->alarm_map = alarm_map_with_user_time;
}

static void
set_busy_time_menu (EventPage *epage, gboolean active)
{
	CompEditor *editor;
	GtkAction *action;

	editor = comp_editor_page_get_editor (COMP_EDITOR_PAGE (epage));
	action = comp_editor_get_action (editor, "show-time-busy");
	gtk_toggle_action_set_active (GTK_TOGGLE_ACTION (action), active);
}

static void
enable_busy_time_menu (EventPage *epage, gboolean sensitive)
{
	CompEditor *editor;
	GtkAction *action;

	editor = comp_editor_page_get_editor (COMP_EDITOR_PAGE (epage));
	action = comp_editor_get_action (editor, "show-time-busy");
	gtk_action_set_sensitive (action, sensitive);
}

static void
set_all_day_event_menu (EventPage *epage, gboolean active)
{
	CompEditor *editor;
	GtkAction *action;

	editor = comp_editor_page_get_editor (COMP_EDITOR_PAGE (epage));
	action = comp_editor_get_action (editor, "all-day-event");
	gtk_toggle_action_set_active (GTK_TOGGLE_ACTION (action), active);
}

/* get_widget handler for the event page */
static GtkWidget *
event_page_get_widget (CompEditorPage *page)
{
	EventPagePrivate *priv;

	priv = EVENT_PAGE_GET_PRIVATE (page);

	return priv->main;
}

/* focus_main_widget handler for the event page */
static void
event_page_focus_main_widget (CompEditorPage *page)
{
	EventPagePrivate *priv;

	priv = EVENT_PAGE_GET_PRIVATE (page);

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
		gtk_combo_box_set_active (GTK_COMBO_BOX (priv->end_time_combo), 1);
	gtk_widget_set_sensitive (priv->end_time_combo, !all_day);

	e_date_edit_set_show_time (E_DATE_EDIT (priv->start_time), !all_day);
	e_date_edit_set_show_time (E_DATE_EDIT (priv->end_time), !all_day);

}

static void
update_time (EventPage *epage, ECalComponentDateTime *start_date, ECalComponentDateTime *end_date)
{
	EventPagePrivate *priv = epage->priv;
	CompEditor *editor;
	ECal *client;
	struct icaltimetype *start_tt, *end_tt, implied_tt;
	icaltimezone *start_zone = NULL, *def_zone = NULL;
	gboolean all_day_event, homezone=TRUE;

	editor = comp_editor_page_get_editor (COMP_EDITOR_PAGE (epage));
	client = comp_editor_get_client (editor);

	/* Note that if we are creating a new event, the timezones may not be
	   on the server, so we try to get the builtin timezone with the TZID
	   first. */
	start_zone = icaltimezone_get_builtin_timezone_from_tzid (start_date->tzid);
	if (!start_zone) {
		/* FIXME: Handle error better. */
		if (!e_cal_get_timezone (client, start_date->tzid, &start_zone, NULL)) {
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

	g_signal_handlers_block_matched (priv->start_time, G_SIGNAL_MATCH_DATA, 0, 0, NULL, NULL, epage);
	g_signal_handlers_block_matched (priv->end_time, G_SIGNAL_MATCH_DATA, 0, 0, NULL, NULL, epage);

	e_date_edit_set_date (E_DATE_EDIT (priv->start_time), start_tt->year,
			      start_tt->month, start_tt->day);
	e_date_edit_set_time_of_day (E_DATE_EDIT (priv->start_time),
				     start_tt->hour, start_tt->minute);

	e_date_edit_set_date (E_DATE_EDIT (priv->end_time), end_tt->year,
			      end_tt->month, end_tt->day);
	e_date_edit_set_time_of_day (E_DATE_EDIT (priv->end_time),
				     end_tt->hour, end_tt->minute);

	g_signal_handlers_unblock_matched (priv->start_time, G_SIGNAL_MATCH_DATA, 0, 0, NULL, NULL, epage);
	g_signal_handlers_unblock_matched (priv->end_time, G_SIGNAL_MATCH_DATA, 0, 0, NULL, NULL, epage);

	/* Set the timezones, and set sync_timezones to TRUE if both timezones
	   are the same. */
	g_signal_handlers_block_matched (priv->start_timezone, G_SIGNAL_MATCH_DATA, 0, 0, NULL, NULL, epage);
	g_signal_handlers_block_matched (priv->end_timezone, G_SIGNAL_MATCH_DATA, 0, 0, NULL, NULL, epage);

	if (start_zone)
		e_timezone_entry_set_timezone (E_TIMEZONE_ENTRY (priv->start_timezone),
					       start_zone);
	def_zone = calendar_config_get_icaltimezone ();
	if (!def_zone || !start_zone || strcmp (icaltimezone_get_tzid(def_zone), icaltimezone_get_tzid (start_zone)))
		 homezone = FALSE;

	event_page_set_show_timezone (epage, (calendar_config_get_show_timezone()|| !homezone) & !all_day_event);

	/*unblock the endtimezone widget*/
	g_signal_handlers_unblock_matched (priv->end_timezone, G_SIGNAL_MATCH_DATA, 0, 0, NULL, NULL, epage);
	g_signal_handlers_unblock_matched (priv->start_timezone, G_SIGNAL_MATCH_DATA, 0, 0, NULL, NULL, epage);

	priv->sync_timezones = TRUE;

	update_end_time_combo (epage);
}

/* Fills the widgets with default values */
static void
clear_widgets (EventPage *epage)
{
	EventPagePrivate *priv = epage->priv;
	CompEditor *editor;

	editor = comp_editor_page_get_editor (COMP_EDITOR_PAGE (epage));

	/* Summary, description */
	e_dialog_editable_set (priv->summary, NULL);
	e_dialog_editable_set (priv->location, NULL);
	gtk_text_buffer_set_text (gtk_text_view_get_buffer (GTK_TEXT_VIEW (priv->description)), "", 0);

	/* Start and end times */
	g_signal_handlers_block_matched (priv->start_time, G_SIGNAL_MATCH_DATA, 0, 0, NULL, NULL, epage);
	g_signal_handlers_block_matched (priv->end_time, G_SIGNAL_MATCH_DATA, 0, 0, NULL, NULL, epage);

	e_date_edit_set_time (E_DATE_EDIT (priv->start_time), 0);
	e_date_edit_set_time (E_DATE_EDIT (priv->end_time), 0);

	g_signal_handlers_unblock_matched (priv->start_time, G_SIGNAL_MATCH_DATA, 0, 0, NULL, NULL, epage);
	g_signal_handlers_unblock_matched (priv->end_time, G_SIGNAL_MATCH_DATA, 0, 0, NULL, NULL, epage);

	epage->priv->all_day_event = FALSE;
	set_all_day (epage, FALSE);

	/* Classification */
	comp_editor_set_classification (editor, E_CAL_COMPONENT_CLASS_PUBLIC);

	/* Show Time As (Transparency) */
	priv->show_time_as_busy = TRUE;
	set_busy_time_menu (epage, TRUE);

	/* Alarm */
	e_dialog_combo_box_set (priv->alarm_time_combo, ALARM_NONE, priv->alarm_map);

	/* Categories */
	e_dialog_editable_set (priv->categories, NULL);
}

static gboolean
is_custom_alarm (ECalComponentAlarm *ca, gchar *old_summary, CalUnits user_units, gint user_interval, gint *alarm_type)
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
		const gchar *x_name;

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
is_custom_alarm_uid_list (ECalComponent *comp, GList *alarms, gchar *old_summary, CalUnits user_units, gint user_interval, gint *alarm_type)
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
is_custom_alarm_store (EAlarmList *alarm_list_store, gchar *old_summary,  CalUnits user_units, gint user_interval, gint *alarm_type)
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

	e_meeting_list_view_column_set_visible (priv->list_view, E_MEETING_STORE_ROLE_COL, state);
}

void
event_page_set_view_status (EventPage *epage, gboolean state)
{
	EventPagePrivate *priv = epage->priv;

	e_meeting_list_view_column_set_visible (priv->list_view, E_MEETING_STORE_STATUS_COL, state);
}

void
event_page_set_view_type (EventPage *epage, gboolean state)
{
	EventPagePrivate *priv = epage->priv;

	e_meeting_list_view_column_set_visible (priv->list_view, E_MEETING_STORE_TYPE_COL, state);
}

void
event_page_set_view_rsvp (EventPage *epage, gboolean state)
{
	EventPagePrivate *priv = epage->priv;

	e_meeting_list_view_column_set_visible (priv->list_view, E_MEETING_STORE_RSVP_COL, state);
}

static GtkWidget *
create_image_event_box (const gchar *image_text, const gchar *tip_text)
{
	GtkWidget *image, *box;

	box = gtk_event_box_new ();
	image = gtk_image_new_from_icon_name (image_text, GTK_ICON_SIZE_MENU);

	gtk_container_add ((GtkContainer *) box, image);
	gtk_widget_show_all (box);
	gtk_widget_set_tooltip_text (box, tip_text);

	return box;
}

static void
sensitize_widgets (EventPage *epage)
{
	ECal *client;
	CompEditor *editor;
	CompEditorFlags flags;
	GtkActionGroup *action_group;
	GtkAction *action;
	gboolean read_only, custom, alarm, sens = TRUE, sensitize;
	EventPagePrivate *priv;
	gboolean delegate;

	editor = comp_editor_page_get_editor (COMP_EDITOR_PAGE (epage));
	client = comp_editor_get_client (editor);
	flags = comp_editor_get_flags (editor);

	priv = epage->priv;
	if (flags & COMP_EDITOR_MEETING)
		sens = flags & COMP_EDITOR_USER_ORG;

	if (!e_cal_is_read_only (client, &read_only, NULL))
		read_only = TRUE;

	delegate = flags & COMP_EDITOR_DELEGATE;

	sensitize = !read_only && sens;

	if (read_only) {
		gchar *tmp = g_strconcat ("<b>", _("Event cannot be edited, because the selected calendar is read only"), "</b>", NULL);
		event_page_set_info_string (epage, GTK_STOCK_DIALOG_INFO, tmp);
		g_free (tmp);
	} else if (!sens) {
		gchar *tmp = g_strconcat ("<b>", _("Event cannot be fully edited, because you are not the organizer"), "</b>", NULL);
		event_page_set_info_string (epage, GTK_STOCK_DIALOG_INFO, tmp);
		g_free (tmp);
	} else {
		event_page_set_info_string (epage, priv->subscriber_info_text ? GTK_STOCK_DIALOG_INFO : NULL, priv->subscriber_info_text);
	}

	alarm = e_dialog_combo_box_get (priv->alarm_time_combo, priv->alarm_map) != ALARM_NONE;
	custom = is_custom_alarm_store (priv->alarm_list_store, priv->old_summary, priv->alarm_units, priv->alarm_interval, NULL) ||
		 e_dialog_combo_box_get (priv->alarm_time_combo, priv->alarm_map)  == ALARM_CUSTOM ? TRUE:FALSE;

	if (alarm && !priv->alarm_icon) {
		priv->alarm_icon = create_image_event_box ("stock_bell", _("This event has alarms"));
		gtk_box_pack_start ((GtkBox *)priv->status_icons, priv->alarm_icon, FALSE, FALSE, 6);
	}

	/* The list of organizers is set to be non-editable. Otherwise any
	 * change in the displayed list causes an 'Account not found' error.
	 */
	gtk_editable_set_editable (GTK_EDITABLE (gtk_bin_get_child (GTK_BIN (priv->organizer))), FALSE);

	gtk_editable_set_editable (GTK_EDITABLE (priv->summary), !read_only);
	gtk_editable_set_editable (GTK_EDITABLE (priv->location), sensitize);
	gtk_widget_set_sensitive (priv->alarm_box, custom);
	gtk_widget_set_sensitive (priv->start_time, sensitize);
	gtk_widget_set_sensitive (priv->start_timezone, sensitize);
	gtk_widget_set_sensitive (priv->end_time, sensitize);
	gtk_widget_set_sensitive (priv->end_timezone, sensitize);
	gtk_text_view_set_editable (GTK_TEXT_VIEW (priv->description), !read_only);
	gtk_widget_set_sensitive (priv->alarm_time_combo, !read_only);
	gtk_widget_set_sensitive (priv->categories_btn, !read_only);
	/*TODO implement the for portion of the end time selector */
	if (flags & COMP_EDITOR_NEW_ITEM) {
		if (priv->all_day_event)
			gtk_combo_box_set_active (GTK_COMBO_BOX (priv->end_time_combo), 1);
		else
			gtk_combo_box_set_active (GTK_COMBO_BOX (priv->end_time_combo), 0);
        } else
		gtk_combo_box_set_active (GTK_COMBO_BOX (priv->end_time_combo), 1);

	gtk_widget_set_sensitive (priv->hour_selector, sensitize);
	gtk_widget_set_sensitive (priv->minute_selector, sensitize);

	gtk_editable_set_editable (GTK_EDITABLE (priv->categories), !read_only);

	if (delegate) {
		gtk_widget_set_sensitive (priv->source_selector, FALSE);
	}

	gtk_widget_set_sensitive (priv->organizer, !read_only);
	gtk_widget_set_sensitive (priv->add, (!read_only &&  sens) || delegate);
	gtk_widget_set_sensitive (priv->edit, (!read_only && sens) || delegate);
	e_meeting_list_view_set_editable (priv->list_view, (!read_only && sens) || delegate);
	gtk_widget_set_sensitive (priv->remove, (!read_only &&  sens) || delegate);
	gtk_widget_set_sensitive (priv->invite, (!read_only &&  sens) || delegate);
	gtk_widget_set_sensitive (GTK_WIDGET (priv->list_view), !read_only);

	action_group = comp_editor_get_action_group (editor, "individual");
	gtk_action_group_set_sensitive (action_group, sensitize);

	action = comp_editor_get_action (editor, "free-busy");
	gtk_action_set_sensitive (action, sensitize);

	if (!priv->is_meeting) {
		gtk_widget_hide (priv->calendar_label);
		gtk_widget_hide (priv->list_box);
		gtk_widget_hide (priv->attendee_box);
		gtk_widget_hide (priv->organizer);
		gtk_label_set_text_with_mnemonic ((GtkLabel *) priv->org_cal_label, _("_Calendar:"));
		gtk_label_set_mnemonic_widget ((GtkLabel *) priv->org_cal_label, priv->source_selector);
	} else {
		gtk_widget_show (priv->calendar_label);
		gtk_widget_show (priv->list_box);
		gtk_widget_show (priv->attendee_box);
		gtk_widget_show (priv->organizer);
		gtk_label_set_text_with_mnemonic ((GtkLabel *) priv->org_cal_label, _("Or_ganizer:"));
	}

}

void
event_page_hide_options (EventPage *page)
{
	CompEditor *editor;
	GtkAction *action;

	g_return_if_fail (IS_EVENT_PAGE (page));

	editor = comp_editor_page_get_editor (COMP_EDITOR_PAGE (page));
	action = comp_editor_get_action (editor, "send-options");
	gtk_action_set_visible (action, FALSE);
}

void
event_page_show_options (EventPage *page)
{
	CompEditor *editor;
	GtkAction *action;

	g_return_if_fail (IS_EVENT_PAGE (page));

	editor = comp_editor_page_get_editor (COMP_EDITOR_PAGE (page));
	action = comp_editor_get_action (editor, "send-options");
	gtk_action_set_visible (action, TRUE);
}

void
event_page_set_meeting (EventPage *page, gboolean set)
{
	g_return_if_fail (IS_EVENT_PAGE (page));

	page->priv->is_meeting = set;
	if (page->priv->comp)
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
	const gchar *str;

	priv = epage->priv;

	str = gtk_entry_get_text (GTK_ENTRY (gtk_bin_get_child (GTK_BIN (priv->organizer))));
	if (!str)
		return NULL;

	for (it = e_list_get_iterator((EList *)priv->accounts); e_iterator_is_valid(it); e_iterator_next(it)) {
		EAccount *a = (EAccount *)e_iterator_get(it);
		gchar *full = g_strdup_printf("%s <%s>", a->id->name, a->id->address);

		if (!g_ascii_strcasecmp (full, str)) {
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
	ECal *client;
	CompEditor *editor;
	CompEditorFlags flags;
	EventPage *epage;
	EventPagePrivate *priv;
	ECalComponentText text;
	ECalComponentClassification cl;
	ECalComponentTransparency transparency;
	ECalComponentDateTime start_date, end_date;
	const gchar *location, *uid = NULL;
	const gchar *categories;
	gchar *backend_addr = NULL;
	GSList *l;
	gboolean validated = TRUE;

	epage = EVENT_PAGE (page);
	priv = epage->priv;

	editor = comp_editor_page_get_editor (page);
	client = comp_editor_get_client (editor);
	flags = comp_editor_get_flags (editor);

	if (!e_cal_component_has_organizer (comp)) {
		flags |= COMP_EDITOR_USER_ORG;
		comp_editor_set_flags (editor, flags);
	}

	/* Clean out old data */
	if (priv->comp != NULL)
		g_object_unref (priv->comp);
	priv->comp = NULL;

	g_ptr_array_foreach (
		priv->deleted_attendees, (GFunc) g_object_unref, NULL);
	g_ptr_array_set_size (priv->deleted_attendees, 0);

	/* Clean the page */
	clear_widgets (epage);

	/* Summary, location, description(s) */

	/* Component for cancellation */
	priv->comp = e_cal_component_clone (comp);
	comp_editor_copy_new_attendees (priv->comp, comp);

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

	e_cal_get_cal_address (client, &backend_addr, NULL);
	set_subscriber_info_string (epage, backend_addr);

	if (priv->is_meeting) {
		ECalComponentOrganizer organizer;

		priv->user_add = itip_get_comp_attendee (comp, client);

		/* Organizer strings */
		event_page_select_organizer (epage, backend_addr);

		/* If there is an existing organizer show it properly */
		if (e_cal_component_has_organizer (comp)) {
			e_cal_component_get_organizer (comp, &organizer);
			if (organizer.value != NULL) {
				const gchar *strip = itip_strip_mailto (organizer.value);
				gchar *string;

				if (itip_organizer_is_user (comp, client) || itip_sentby_is_user (comp, client)) {
					if (e_cal_get_static_capability (
								client,
								CAL_STATIC_CAPABILITY_ORGANIZER_NOT_EMAIL_ADDRESS))
						priv->user_org = TRUE;
				} else {
					if (e_cal_get_static_capability (
								client,
								CAL_STATIC_CAPABILITY_ORGANIZER_NOT_EMAIL_ADDRESS))
					gtk_widget_set_sensitive (priv->invite, FALSE);
					gtk_widget_set_sensitive (priv->add, FALSE);
					gtk_widget_set_sensitive (priv->edit, FALSE);
					gtk_widget_set_sensitive (priv->remove, FALSE);
					priv->user_org = FALSE;
				}

				if (e_cal_get_static_capability (client, CAL_STATIC_CAPABILITY_NO_ORGANIZER) && (flags & COMP_EDITOR_DELEGATE))
					string = g_strdup (backend_addr);
				else if ( organizer.cn != NULL)
					string = g_strdup_printf ("%s <%s>", organizer.cn, strip);
				else
					string = g_strdup (strip);

				if (!priv->user_org) {
					gtk_list_store_clear (GTK_LIST_STORE (gtk_combo_box_get_model (GTK_COMBO_BOX (priv->organizer))));
					gtk_combo_box_append_text (GTK_COMBO_BOX (priv->organizer), string);
					gtk_combo_box_set_active (GTK_COMBO_BOX (priv->organizer), 0);
					gtk_editable_set_editable (GTK_EDITABLE (gtk_bin_get_child (GTK_BIN (priv->organizer))), FALSE);
				} else {
					gtk_entry_set_text (GTK_ENTRY (gtk_bin_get_child (GTK_BIN (priv->organizer))), string);
				}

				g_free (string);
				priv->existing = TRUE;
			}
		} else {
			EAccount *a;

			a = get_current_account (epage);
			if (a != NULL) {
				priv->ia = e_meeting_store_add_attendee_with_defaults (priv->model);
				g_object_ref (priv->ia);

				if (!(backend_addr && *backend_addr) || !g_ascii_strcasecmp (backend_addr, a->id->address)) {
					e_meeting_attendee_set_address (priv->ia, g_strdup_printf ("MAILTO:%s", a->id->address));
					e_meeting_attendee_set_cn (priv->ia, g_strdup (a->id->name));
				} else {
					e_meeting_attendee_set_address (priv->ia, g_strdup_printf ("MAILTO:%s", backend_addr));
					e_meeting_attendee_set_sentby (priv->ia, g_strdup_printf ("MAILTO:%s", a->id->address));
				}

				if (client && e_cal_get_organizer_must_accept (client))
					e_meeting_attendee_set_status (priv->ia, ICAL_PARTSTAT_NEEDSACTION);
				else
					e_meeting_attendee_set_status (priv->ia, ICAL_PARTSTAT_ACCEPTED);
				e_meeting_list_view_add_attendee_to_name_selector (E_MEETING_LIST_VIEW (priv->list_view), priv->ia);
			}

		}
	}

	if (backend_addr)
		g_free (backend_addr);

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

	update_end_time_combo (epage);
	/* Classification */
	e_cal_component_get_classification (comp, &cl);
	comp_editor_set_classification (editor, cl);

	/* Show Time As (Transparency) */
	e_cal_component_get_transparency (comp, &transparency);
	switch (transparency) {
	case E_CAL_COMPONENT_TRANSP_TRANSPARENT:
		priv->show_time_as_busy = FALSE;
		set_busy_time_menu (epage, FALSE);
		break;

	default:
		priv->show_time_as_busy = TRUE;
		set_busy_time_menu (epage, TRUE);
		break;
	}

	if (e_cal_get_static_capability (client, CAL_STATIC_CAPABILITY_NO_TRANSPARENCY))
		enable_busy_time_menu (epage, FALSE);
	else
		enable_busy_time_menu (epage, TRUE);

	/* Alarms */
	g_signal_handlers_block_matched (priv->alarm_time_combo, G_SIGNAL_MATCH_DATA, 0, 0, NULL, NULL, epage);
	g_signal_handlers_block_matched (priv->alarm_list_store, G_SIGNAL_MATCH_DATA, 0, 0, NULL, NULL, epage);

	if (e_cal_component_has_alarms (comp)) {
		GList *alarms, *list;
		gint alarm_type;

		alarms = e_cal_component_get_alarm_uids (comp);
		if (!is_custom_alarm_uid_list (comp, alarms, priv->old_summary, priv->alarm_units, priv->alarm_interval, &alarm_type))
			e_dialog_combo_box_set (priv->alarm_time_combo, alarm_type, priv->alarm_map);
		else
			e_dialog_combo_box_set (priv->alarm_time_combo, ALARM_CUSTOM, priv->alarm_map);

		for (list = alarms; list != NULL; list = list->next) {
			ECalComponentAlarm *ca;

			ca = e_cal_component_get_alarm (comp, list->data);
			e_alarm_list_append (priv->alarm_list_store, NULL, ca);
			e_cal_component_alarm_free (ca);
		}

		cal_obj_uid_list_free (alarms);
	} else {
		e_dialog_combo_box_set (priv->alarm_time_combo, ALARM_NONE, priv->alarm_map);
	}
	g_signal_handlers_unblock_matched (priv->alarm_time_combo, G_SIGNAL_MATCH_DATA, 0, 0, NULL, NULL, epage);
	g_signal_handlers_unblock_matched (priv->alarm_list_store, G_SIGNAL_MATCH_DATA, 0, 0, NULL, NULL, epage);

	/* Categories */
	e_cal_component_get_categories (comp, &categories);
	e_dialog_editable_set (priv->categories, categories);

	/* Source */
	e_source_combo_box_set_active (
		E_SOURCE_COMBO_BOX (priv->source_selector),
		e_cal_get_source (client));

	e_cal_component_get_uid (comp, &uid);
	if (!(flags & COMP_EDITOR_DELEGATE)
			&& !(flags && COMP_EDITOR_NEW_ITEM)) {
		event_page_hide_options (epage);
	}

	sensitize_widgets (epage);

	return validated;
}

/* fill_component handler for the event page */
static gboolean
event_page_fill_component (CompEditorPage *page, ECalComponent *comp)
{
	CompEditor *editor;
	CompEditorFlags flags;
	ECal *client;
	EventPage *epage;
	EventPagePrivate *priv;
	ECalComponentClassification classification;
	ECalComponentDateTime start_date, end_date;
	struct icaltimetype start_tt, end_tt;
	gboolean all_day_event, start_date_set, end_date_set, busy;
	gchar *cat, *str;
	GtkTextBuffer *text_buffer;
	GtkTextIter text_iter_start, text_iter_end;

	epage = EVENT_PAGE (page);
	priv = epage->priv;

	editor = comp_editor_page_get_editor (page);
	client = comp_editor_get_client (editor);
	flags = comp_editor_get_flags (editor);

	text_buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (priv->description));

	comp_editor_copy_new_attendees (comp, priv->comp);

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
	g_return_val_if_fail (start_date_set, FALSE);

	if (!e_date_edit_date_is_valid (E_DATE_EDIT (priv->end_time))) {
		comp_editor_page_display_validation_error (page, _("End date is wrong"), priv->end_time);
		return FALSE;
	}
	end_date_set = e_date_edit_get_date (E_DATE_EDIT (priv->end_time),
					     &end_tt.year,
					     &end_tt.month,
					     &end_tt.day);
	g_return_val_if_fail (end_date_set, FALSE);

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
	classification = comp_editor_get_classification (editor);
	e_cal_component_set_classification (comp, classification);

	/* Show Time As (Transparency) */
	busy = priv->show_time_as_busy;
	e_cal_component_set_transparency (comp, busy ? E_CAL_COMPONENT_TRANSP_OPAQUE : E_CAL_COMPONENT_TRANSP_TRANSPARENT);

	/* send options */
	if (priv->sendoptions_shown && priv->sod)
		e_sendoptions_utils_fill_component (priv->sod, comp);

	/* Alarm */
	e_cal_component_remove_all_alarms (comp);
	if (e_dialog_combo_box_get (priv->alarm_time_combo, priv->alarm_map) != ALARM_NONE) {
		if (e_dialog_combo_box_get (priv->alarm_time_combo, priv->alarm_map) == ALARM_CUSTOM) {
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
				if (!alarm) {
					g_warning ("alarm is NULL\n");
					continue;
				}

				/* We set the description of the alarm if it's got
				 * the X-EVOLUTION-NEEDS-DESCRIPTION property.
				 */
				icalcomp = e_cal_component_alarm_get_icalcomponent (alarm);
				icalprop = icalcomponent_get_first_property (icalcomp, ICAL_X_PROPERTY);
				while (icalprop) {
					const gchar *x_name;
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
			gint alarm_type;

			ca = e_cal_component_alarm_new ();

			e_cal_component_get_summary (comp, &summary);

			if (summary.value)
				e_cal_component_alarm_set_description (ca, &summary);

			e_cal_component_alarm_set_action (ca, E_CAL_COMPONENT_ALARM_DISPLAY);

			memset (&trigger, 0, sizeof (ECalComponentAlarmTrigger));
			trigger.type = E_CAL_COMPONENT_ALARM_TRIGGER_RELATIVE_START;
			trigger.u.rel_duration.is_neg = 1;

			alarm_type = e_dialog_combo_box_get (priv->alarm_time_combo, priv->alarm_map);
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
			gchar *backend_addr = NULL, *org_addr = NULL, *sentby = NULL;

			e_cal_get_cal_address (client, &backend_addr, NULL);

			/* Find the identity for the organizer or sentby field */
			a = get_current_account (epage);

			/* Sanity Check */
			if (a == NULL) {
				e_notice (priv->main, GTK_MESSAGE_ERROR,
						_("The organizer selected no longer has an account."));
				return FALSE;
			}

			if (a->id->address == NULL || strlen (a->id->address) == 0) {
				e_notice (priv->main, GTK_MESSAGE_ERROR,
						_("An organizer is required."));
				return FALSE;
			}

			if (!(backend_addr && *backend_addr) || !g_ascii_strcasecmp (backend_addr, a->id->address)) {
				org_addr = g_strdup_printf ("MAILTO:%s", a->id->address);
				organizer.value = org_addr;
				organizer.cn = a->id->name;
			} else {
				gchar *sentby = NULL;
				org_addr = g_strdup_printf ("MAILTO:%s", backend_addr);
				sentby = g_strdup_printf ("MAILTO:%s", a->id->address);
				organizer.value = org_addr;
				organizer.sentby = sentby;
			}

			e_cal_component_set_organizer (comp, &organizer);

			g_free (backend_addr);
			g_free (org_addr);
			g_free (sentby);
		}

		if (e_meeting_store_count_actual_attendees (priv->model) < 1) {
			e_notice (priv->main, GTK_MESSAGE_ERROR,
					_("At least one attendee is required."));
			return FALSE;
		}

		if (flags & COMP_EDITOR_DELEGATE) {
			GSList *attendee_list, *l;
			gint i;
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
			g_hash_table_insert (timezones, (gpointer) icaltimezone_get_tzid (zone), zone);
	}

	/* add end date timezone */
	zone = e_timezone_entry_get_timezone (E_TIMEZONE_ENTRY (priv->end_timezone));
	if (zone) {
		if (!g_hash_table_lookup (timezones, icaltimezone_get_tzid (zone)))
			g_hash_table_insert (timezones, (gpointer) icaltimezone_get_tzid (zone), zone);
	}

	return TRUE;
}

static void
event_page_set_dates (CompEditorPage *page, CompEditorPageDates *dates)
{
	update_time (EVENT_PAGE (page), dates->start, dates->end);
}



static void
time_sel_changed (GtkComboBox *combo, EventPage *epage)
{
	EventPagePrivate *priv;
	gint selection = gtk_combo_box_get_active (combo);

	priv = epage->priv;

	if (selection == 1) {
		gtk_widget_hide (priv->time_hour);
		gtk_widget_show (priv->end_time);
		hour_sel_changed (GTK_SPIN_BUTTON (priv->hour_selector), epage);
		minute_sel_changed (GTK_SPIN_BUTTON (priv->minute_selector), epage);
	} else if (!selection) {
		gtk_widget_show (priv->time_hour);
		gtk_widget_hide (priv->end_time);

		update_end_time_combo ( epage);
	}
}

static
void update_end_time_combo (EventPage *epage)
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

static void
hour_sel_changed (GtkSpinButton *widget, EventPage *epage)
{
	hour_minute_changed (epage);
}

static void
minute_sel_changed (GtkSpinButton *widget, EventPage *epage)
{
	hour_minute_changed (epage);
}

static void
hour_minute_changed (EventPage *epage)
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
edit_clicked_cb (GtkButton *btn, EventPage *epage)
{
	EventPagePrivate *priv;
	GtkTreePath *path = NULL;
	GtkTreeViewColumn *focus_col;
	gint row = 0;

	priv = epage->priv;

	gtk_tree_view_get_cursor (GTK_TREE_VIEW (priv->list_view), &path, NULL);
	g_return_if_fail (path != NULL);

	row = gtk_tree_path_get_indices (path)[0];

	gtk_tree_view_get_cursor (GTK_TREE_VIEW (priv->list_view), &path, &focus_col);
	gtk_tree_view_set_cursor (GTK_TREE_VIEW (priv->list_view), path, focus_col, TRUE);
	gtk_tree_path_free (path);
}

static void
add_clicked_cb (GtkButton *btn, EventPage *epage)
{
	CompEditor *editor;
	CompEditorFlags flags;
	EMeetingAttendee *attendee;

	editor = comp_editor_page_get_editor (COMP_EDITOR_PAGE (epage));
	flags = comp_editor_get_flags (editor);

	attendee = e_meeting_store_add_attendee_with_defaults (epage->priv->model);

	if (flags & COMP_EDITOR_DELEGATE) {
		e_meeting_attendee_set_delfrom (attendee, g_strdup_printf ("MAILTO:%s", epage->priv->user_add));
	}

	e_meeting_list_view_edit (epage->priv->list_view, attendee);
}

static gboolean
existing_attendee (EMeetingAttendee *ia, ECalComponent *comp)
{
	GSList *attendees, *l;
	const gchar *ia_address;
	const gchar *ia_sentby = NULL;

	ia_address = itip_strip_mailto (e_meeting_attendee_get_address (ia));
	if (!ia_address)
		return FALSE;

	if (e_meeting_attendee_is_set_sentby (ia))
		ia_sentby = itip_strip_mailto (e_meeting_attendee_get_sentby (ia));

	e_cal_component_get_attendee_list (comp, &attendees);

	for (l = attendees; l; l = l->next) {
		ECalComponentAttendee *attendee = l->data;
		const gchar *address;
		const gchar *sentby = NULL;

		address = itip_strip_mailto (attendee->value);
		if (attendee->sentby)
			sentby = itip_strip_mailto (attendee->sentby);

		if ((address && !g_ascii_strcasecmp (ia_address, address)) || (sentby && ia_sentby&& !g_ascii_strcasecmp (ia_sentby, sentby))) {
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
	EventPagePrivate *priv = epage->priv;
	CompEditor *editor;
	CompEditorFlags flags;
	gint pos = 0;
	gboolean delegate;

	editor = comp_editor_page_get_editor (COMP_EDITOR_PAGE (epage));
	flags = comp_editor_get_flags (editor);

	delegate = (flags & COMP_EDITOR_DELEGATE);

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

		/* do not add to deleted_attendees if user removed new attendee */
		if (existing_attendee (ia, priv->comp) && !comp_editor_have_in_new_attendees (priv->comp, ia)) {
			g_object_ref (ia);
			g_ptr_array_add (priv->deleted_attendees, ia);
		}

		if (e_meeting_attendee_get_delto (ia) != NULL)
			ib = e_meeting_store_find_attendee (priv->model, e_meeting_attendee_get_delto (ia), NULL);

		comp_editor_manage_new_attendees (priv->comp, ia, FALSE);
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
	GtkTreeModel *model = NULL;
	gboolean valid_iter;
	gchar *address;

	priv = epage->priv;

	selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (priv->list_view));
	model = GTK_TREE_MODEL (priv->model);
	if (!(paths = gtk_tree_selection_get_selected_rows (selection, &model ))) {
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
attendee_added_cb (EMeetingListView *emlv,
                   EMeetingAttendee *ia,
                   EventPage *epage)
{
	EventPagePrivate *priv = epage->priv;
	CompEditor *editor;
	CompEditorFlags flags;
	ECal *client;

	editor = comp_editor_page_get_editor (COMP_EDITOR_PAGE (epage));
	client = comp_editor_get_client (editor);
	flags = comp_editor_get_flags (editor);

	if (!(flags & COMP_EDITOR_DELEGATE)) {
		comp_editor_manage_new_attendees (priv->comp, ia, TRUE);
		return;
	}

	if (existing_attendee (ia, priv->comp)) {
		e_meeting_store_remove_attendee (priv->model, ia);
	} else {
		if (!e_cal_get_static_capability (client, CAL_STATIC_CAPABILITY_DELEGATE_TO_MANY)) {
			const gchar *delegator_id = e_meeting_attendee_get_delfrom (ia);
			EMeetingAttendee *delegator;

			delegator = e_meeting_store_find_attendee (priv->model, delegator_id, NULL);

			g_return_if_fail (delegator != NULL);

			e_meeting_attendee_set_delto (delegator, g_strdup (e_meeting_attendee_get_address (ia)));

			e_meeting_attendee_set_delfrom (ia, g_strdup_printf ("MAILTO:%s", delegator_id));
			gtk_widget_set_sensitive (priv->invite, FALSE);
			gtk_widget_set_sensitive (priv->add, FALSE);
			gtk_widget_set_sensitive (priv->edit, FALSE);
		}
	}
}

/* Callbacks for list view*/
static void
popup_add_cb (EPopup *ep, EPopupItem *pitem, gpointer data)
{
	EventPage *epage = data;

	add_clicked_cb (NULL, epage);
}

static void
popup_delete_cb (EPopup *ep, EPopupItem *pitem, gpointer data)
{
	EventPage *epage = data;

	remove_clicked_cb (NULL, epage);
}

enum {
	ATTENDEE_CAN_DELEGATE = 1<<1,
	ATTENDEE_CAN_DELETE = 1<<2,
	ATTENDEE_CAN_ADD = 1<<3,
	ATTENDEE_LAST = 1<<4
};

static EPopupItem context_menu_items[] = {
	{ E_POPUP_ITEM, (gchar *) "10.delete", (gchar *) N_("_Remove"), popup_delete_cb, NULL, (gchar *) GTK_STOCK_REMOVE, ATTENDEE_CAN_DELETE },
	{ E_POPUP_ITEM, (gchar *) "15.add", (gchar *) N_("_Add "), popup_add_cb, NULL, (gchar *) GTK_STOCK_ADD, ATTENDEE_CAN_ADD },
};

static void
context_popup_free(EPopup *ep, GSList *items, gpointer data)
{
	g_slist_free(items);
}

static gint
button_press_event (GtkWidget *widget, GdkEventButton *event, EventPage *epage)
{
	EventPagePrivate *priv = epage->priv;
	CompEditor *editor;
	CompEditorFlags flags;
	GtkMenu *menu;
	EMeetingAttendee *ia;
	GtkTreePath *path;
	GtkTreeIter iter;
	gchar *address;
	guint32 disable_mask = ~0;
	GSList *menus = NULL;
	ECalPopup *ep;
	gint i;

	/* only process right-clicks */
	if (event->button != 3 || event->type != GDK_BUTTON_PRESS)
		return FALSE;

	editor = comp_editor_page_get_editor (COMP_EDITOR_PAGE (epage));
	flags = comp_editor_get_flags (editor);

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
	else if (flags & COMP_EDITOR_USER_ORG)
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

	EventPagePrivate *priv = epage->priv;
	CompEditor *editor;
	CompEditorFlags flags;

	editor = comp_editor_page_get_editor (COMP_EDITOR_PAGE (epage));
	flags = comp_editor_get_flags (editor);

	if (event->type == GDK_2BUTTON_PRESS && flags & COMP_EDITOR_USER_ORG) {
		EMeetingAttendee *attendee;

		attendee = e_meeting_store_add_attendee_with_defaults (priv->model);

		if (flags & COMP_EDITOR_DELEGATE) {
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
	CompEditor *editor;
	GtkAction *action;
	gboolean date_set;

	editor = comp_editor_page_get_editor (COMP_EDITOR_PAGE (epage));

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
	g_return_if_fail (date_set);

	date_set = e_date_edit_get_date (E_DATE_EDIT (priv->end_time),
					 &end_tt.year,
					 &end_tt.month,
					 &end_tt.day);
	e_date_edit_get_time_of_day (E_DATE_EDIT (priv->end_time),
				     &end_tt.hour,
				     &end_tt.minute);
	g_return_if_fail (date_set);

	/* TODO implement the for portion in end time selector */
	gtk_widget_set_sensitive (priv->end_time_combo, !all_day);
	if (all_day)
		gtk_combo_box_set_active (GTK_COMBO_BOX (priv->end_time_combo), 1);
	else
		gtk_combo_box_set_active (GTK_COMBO_BOX (priv->end_time_combo), 0);

	action = comp_editor_get_action (editor, "view-time-zone");
	gtk_action_set_sensitive (action, !all_day);

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
		icaltimezone *start_zone;

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
	g_signal_handlers_block_matched (priv->start_time, G_SIGNAL_MATCH_DATA, 0, 0, NULL, NULL, epage);
	g_signal_handlers_block_matched (priv->end_time, G_SIGNAL_MATCH_DATA, 0, 0, NULL, NULL, epage);

	e_date_edit_set_date (E_DATE_EDIT (priv->start_time), start_tt.year,
			      start_tt.month, start_tt.day);
	e_date_edit_set_time_of_day (E_DATE_EDIT (priv->start_time),
				     start_tt.hour, start_tt.minute);

	e_date_edit_set_date (E_DATE_EDIT (priv->end_time), end_tt.year,
			      end_tt.month, end_tt.day);
	e_date_edit_set_time_of_day (E_DATE_EDIT (priv->end_time),
				     end_tt.hour, end_tt.minute);

	g_signal_handlers_unblock_matched (priv->start_time, G_SIGNAL_MATCH_DATA, 0, 0, NULL, NULL, epage);
	g_signal_handlers_unblock_matched (priv->end_time, G_SIGNAL_MATCH_DATA, 0, 0, NULL, NULL, epage);

	/* Notify upstream */
	notify_dates_changed (epage, &start_tt, &end_tt);

	comp_editor_page_changed (COMP_EDITOR_PAGE (epage));
}

void
event_page_set_show_time_busy (EventPage *epage, gboolean state)
{
	epage->priv->show_time_as_busy = state;
	comp_editor_page_changed (COMP_EDITOR_PAGE (epage));
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

/*If the msg has some value set, the icon should always be set */
void
event_page_set_info_string (EventPage *epage, const gchar *icon, const gchar *msg)
{
	EventPagePrivate *priv;

	priv = epage->priv;

	gtk_image_set_from_stock (GTK_IMAGE (priv->info_icon), icon, GTK_ICON_SIZE_BUTTON);
	gtk_label_set_markup (GTK_LABEL(priv->info_string), msg);

	if (msg && icon)
		gtk_widget_show (priv->info_hbox);
	else
		gtk_widget_hide (priv->info_hbox);
}

/* Gets the widgets from the XML file and returns if they are all available. */
static gboolean
get_widgets (EventPage *epage)
{
	CompEditorPage *page = COMP_EDITOR_PAGE (epage);
	GtkEntryCompletion *completion;
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
	if (accel_groups)
		page->accel_group = g_object_ref (accel_groups->data);
	priv->alarm_dialog = GW ("alarm-dialog");
	priv->alarm_box = GW ("custom_box");
	priv->alarm_time_combo = GW ("alarm-time-combobox");

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

	g_object_ref (priv->main);
	gtk_container_remove (GTK_CONTAINER (priv->main->parent), priv->main);

	priv->categories = GW ("categories");
	priv->categories_btn = GW ("categories-button");

	priv->organizer = GW ("organizer");
	gtk_list_store_clear (GTK_LIST_STORE (gtk_combo_box_get_model (GTK_COMBO_BOX (priv->organizer))));

	priv->summary = GW ("summary");
	priv->summary_label = GW ("summary-label");
	priv->location = GW ("location");
	priv->location_label = GW ("location-label");

	priv->info_hbox = GW ("generic-info");
	priv->info_icon = GW ("generic-info-image");
	priv->info_string = GW ("generic-info-msgs");

	priv->invite = GW ("invite");
	priv->invite_label = GW ("invite-label");
	if (comp_editor_get_lite ())
		gtk_widget_hide (priv->invite);
	else
		gtk_widget_hide (priv->invite_label);

	priv->add = GW ("add-attendee");
	priv->remove = GW ("remove-attendee");
	priv->edit = GW ("edit-attendee");
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
	priv->end_time_combo = GW ("end-time-combobox");

	priv->end_time = GW ("end-time");
	gtk_widget_show_all (priv->time_hour);
	gtk_widget_hide (priv->end_time);

	priv->description = GW ("description");

	priv->source_selector = GW ("source");

#undef GW

	completion = e_category_completion_new ();
	gtk_entry_set_completion (GTK_ENTRY (priv->categories), completion);
	g_object_unref (completion);

	return (priv->summary
		&& priv->location
		&& priv->start_time
		&& priv->end_time
		&& priv->description );
}

static void
summary_changed_cb (GtkEditable *editable,
                    CompEditorPage *page)
{
	CompEditor *editor;
	gchar *summary;

	if (comp_editor_page_get_updating (page))
		return;

	editor = comp_editor_page_get_editor (page);
	summary = e_dialog_editable_get (GTK_WIDGET (editable));
	comp_editor_set_summary (editor, summary);
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
	gint cmp;

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

	if (comp_editor_page_get_updating (COMP_EDITOR_PAGE (epage)))
		return;

	/* Fetch the start and end times and timezones from the widgets. */
	all_day_event = priv->all_day_event;

	date_set = e_date_edit_get_date (E_DATE_EDIT (priv->start_time),
					 &start_tt.year,
					 &start_tt.month,
					 &start_tt.day);
	g_return_if_fail (date_set);

	date_set = e_date_edit_get_date (E_DATE_EDIT (priv->end_time),
					 &end_tt.year,
					 &end_tt.month,
					 &end_tt.day);
	g_return_if_fail (date_set);

	if (all_day_event) {
		/* All Day Events are simple. We just compare the dates and if
		   start > end we copy one of them to the other. */
		gint cmp = icaltime_compare_date_only (start_tt, end_tt);
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
		comp_editor_page_set_updating (COMP_EDITOR_PAGE (epage), TRUE);
		/*the earlier method caused an infinite recursion*/
		priv->end_timezone=priv->start_timezone;
		gtk_widget_show_all (priv->end_timezone);
		comp_editor_page_set_updating (COMP_EDITOR_PAGE (epage), FALSE);
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
	CompEditor *editor;
	GtkWidget *toplevel;
	ESource *source;
	ECal *client;

	priv = epage->priv;
	editor = comp_editor_page_get_editor (COMP_EDITOR_PAGE (epage));
	client = comp_editor_get_client (editor);

	if (!priv->sod) {
		priv->sod = e_sendoptions_dialog_new ();
		source = e_source_combo_box_get_active (
			E_SOURCE_COMBO_BOX (priv->source_selector));
		e_sendoptions_utils_set_default_data (priv->sod, source, "calendar");
		priv->sod->data->initialized = TRUE;
	}

	if (e_cal_get_static_capability (client, CAL_STATIC_CAPABILITY_NO_GEN_OPTIONS)) {
		e_sendoptions_set_need_general_options (priv->sod, FALSE);
	}

	toplevel = gtk_widget_get_toplevel (priv->main);
	e_sendoptions_dialog_run (priv->sod, toplevel, E_ITEM_CALENDAR);
}

static void
source_changed_cb (ESourceComboBox *source_combo_box, EventPage *epage)
{
	EventPagePrivate *priv = epage->priv;
	CompEditor *editor;
	ESource *source;
	ECal *client;

	if (comp_editor_page_get_updating (COMP_EDITOR_PAGE (epage)))
		return;

	editor = comp_editor_page_get_editor (COMP_EDITOR_PAGE (epage));
	source = e_source_combo_box_get_active (source_combo_box);
	client = auth_new_cal_from_source (source, E_CAL_SOURCE_TYPE_EVENT);

	if (client) {
		icaltimezone *zone;

		zone = calendar_config_get_icaltimezone ();
		e_cal_set_default_timezone (client, zone, NULL);
	}

	if (!client || !e_cal_open (client, FALSE, NULL)) {
		GtkWidget *dialog;
		ECal *old_client;

		old_client = comp_editor_get_client (editor);

		if (client)
			g_object_unref (client);

		e_source_combo_box_set_active (
			E_SOURCE_COMBO_BOX (priv->source_selector),
			e_cal_get_source (old_client));

		dialog = gtk_message_dialog_new (NULL, GTK_DIALOG_MODAL,
						 GTK_MESSAGE_WARNING, GTK_BUTTONS_OK,
						 _("Unable to open the calendar '%s'."),
						 e_source_peek_name (source));
		gtk_dialog_run (GTK_DIALOG (dialog));
		gtk_widget_destroy (dialog);
	} else {
		comp_editor_set_client (editor, client);
		if (e_cal_get_static_capability (client, CAL_STATIC_CAPABILITY_REQ_SEND_OPTIONS) && priv->is_meeting)
			event_page_show_options (epage);
		else
			event_page_hide_options (epage);

		if (client) {
			gchar *backend_addr = NULL;

			e_cal_get_cal_address(client, &backend_addr, NULL);

			if (priv->is_meeting)
				event_page_select_organizer (epage, backend_addr);

			set_subscriber_info_string (epage, backend_addr);
			g_free (backend_addr);
		}

		sensitize_widgets (epage);

		alarm_list_dialog_set_client (priv->alarm_list_dlg_widget, client);
	}
}

static void
set_subscriber_info_string (EventPage *epage, const gchar *backend_address)
{
	CompEditor *editor;
	ECal *client;
	ESource *source;

	editor = comp_editor_page_get_editor (COMP_EDITOR_PAGE (epage));
	client = comp_editor_get_client (editor);
	source = e_cal_get_source (client);

	if (e_source_get_property (source, "subscriber")) {
		g_free (epage->priv->subscriber_info_text);
		/* Translators: This string is used when we are creating an Event
		   (meeting or appointment)  on behalf of some other user */
		epage->priv->subscriber_info_text = g_markup_printf_escaped (_("You are acting on behalf of %s"), backend_address);

		event_page_set_info_string (epage, GTK_STOCK_DIALOG_INFO, epage->priv->subscriber_info_text);
	} else {
		g_free (epage->priv->subscriber_info_text);
		epage->priv->subscriber_info_text = NULL;

		event_page_set_info_string (epage, NULL, NULL);
	}
}

static void
alarm_changed_cb (GtkWidget *widget, gpointer data)
{
	EventPage *epage;
	EventPagePrivate *priv;

	epage = EVENT_PAGE (data);
	priv = epage->priv;

	if (e_dialog_combo_box_get (priv->alarm_time_combo, priv->alarm_map) != ALARM_NONE) {
		ECalComponentAlarm *ca;
		ECalComponentAlarmTrigger trigger;
		icalcomponent *icalcomp;
		icalproperty *icalprop;
		gint alarm_type;

		ca = e_cal_component_alarm_new ();

		e_cal_component_alarm_set_action (ca, E_CAL_COMPONENT_ALARM_DISPLAY);

		memset (&trigger, 0, sizeof (ECalComponentAlarmTrigger));
		trigger.type = E_CAL_COMPONENT_ALARM_TRIGGER_RELATIVE_START;
		trigger.u.rel_duration.is_neg = 1;

		alarm_type = e_dialog_combo_box_get (priv->alarm_time_combo, priv->alarm_map);
		switch (alarm_type) {
		case ALARM_15_MINUTES:
			e_alarm_list_clear (priv->alarm_list_store);
			trigger.u.rel_duration.minutes = 15;
			break;

		case ALARM_1_HOUR:
			e_alarm_list_clear (priv->alarm_list_store);
			trigger.u.rel_duration.hours = 1;
			break;

		case ALARM_1_DAY:
			e_alarm_list_clear (priv->alarm_list_store);
			trigger.u.rel_duration.days = 1;
			break;

		case ALARM_USER_TIME:
			e_alarm_list_clear (priv->alarm_list_store);
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
		case ALARM_CUSTOM:
			gtk_widget_set_sensitive (priv->alarm_box, TRUE);

		default:
			break;
		}

		if (alarm_type != ALARM_CUSTOM) {
			e_cal_component_alarm_set_trigger (ca, trigger);

			icalcomp = e_cal_component_alarm_get_icalcomponent (ca);
			icalprop = icalproperty_new_x ("1");
			icalproperty_set_x_name (icalprop, "X-EVOLUTION-NEEDS-DESCRIPTION");
			icalcomponent_add_property (icalcomp, icalprop);

			e_alarm_list_append (priv->alarm_list_store, NULL, ca);
		}
		if (!priv->alarm_icon) {
			priv->alarm_icon = create_image_event_box ("stock_bell", _("This event has alarms"));
			gtk_box_pack_start ((GtkBox *)priv->status_icons, priv->alarm_icon, FALSE, FALSE, 6);
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

#if 0
static void
alarm_custom_clicked_cb (GtkWidget *widget,
                         EventPage *epage)
{
	EventPagePrivate *priv = epage->priv;
	EAlarmList *temp_list_store;
	CompEditor *editor;
	GtkTreeModel *model;
	GtkTreeIter iter;
	gboolean valid_iter;
	GtkWidget *toplevel;
	ECal *client;

	editor = comp_editor_page_get_editor (COMP_EDITOR_PAGE (epage));
	client = comp_editor_get_client (editor);

	/* Make a copy of the list store in case the user cancels */
	temp_list_store = e_alarm_list_new ();
	model = GTK_TREE_MODEL (priv->alarm_list_store);

	for (valid_iter = gtk_tree_model_get_iter_first (model, &iter); valid_iter;
	     valid_iter = gtk_tree_model_iter_next (model, &iter)) {
		ECalComponentAlarm *alarm;

		alarm = (ECalComponentAlarm *) e_alarm_list_get_alarm (priv->alarm_list_store, &iter);
		if (alarm == NULL) {
			g_warning ("alarm is NULL\n");
			continue;
		}

		e_alarm_list_append (temp_list_store, NULL, alarm);
	}

	toplevel = gtk_widget_get_toplevel (priv->main);
	if (alarm_list_dialog_run (toplevel, client, temp_list_store)) {
		g_object_unref (priv->alarm_list_store);
		priv->alarm_list_store = temp_list_store;

		comp_editor_set_changed (editor, TRUE);
	} else {
		g_object_unref (temp_list_store);
	}

	/* If the user erases everything, uncheck the alarm toggle */
	valid_iter = gtk_tree_model_get_iter_first (GTK_TREE_MODEL (priv->alarm_list_store), &iter);

	sensitize_widgets (epage);
}
#endif

/* Hooks the widget signals */
static gboolean
init_widgets (EventPage *epage)
{
	EventPagePrivate *priv = epage->priv;
	CompEditor *editor;
	GtkTextBuffer *text_buffer;
	icaltimezone *zone;
	gchar *combo_label = NULL;
	GtkTreeSelection *selection;
	ECal *client;

	editor = comp_editor_page_get_editor (COMP_EDITOR_PAGE (epage));
	client = comp_editor_get_client (editor);

	/* Make sure the EDateEdit widgets use our timezones to get the
	   current time. */
	e_date_edit_set_get_time_callback (E_DATE_EDIT (priv->start_time),
					   (EDateEditGetTimeCallback) comp_editor_get_current_time,
					   epage, NULL);
	e_date_edit_set_get_time_callback (E_DATE_EDIT (priv->end_time),
					   (EDateEditGetTimeCallback) comp_editor_get_current_time,
					   epage, NULL);

	/* Generic informative messages */
	gtk_widget_hide (priv->info_hbox);

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
	g_signal_connect((priv->source_selector), "changed",
			    G_CALLBACK (source_changed_cb), epage);
	/* Alarms */
	priv->alarm_list_store = e_alarm_list_new ();
	g_signal_connect_swapped (
		priv->alarm_list_store, "row-inserted",
		G_CALLBACK (comp_editor_page_changed), epage);
	g_signal_connect_swapped (
		priv->alarm_list_store, "row-deleted",
		G_CALLBACK (comp_editor_page_changed), epage);
	g_signal_connect_swapped (
		priv->alarm_list_store, "row-changed",
		G_CALLBACK (comp_editor_page_changed), epage);

	/* Timezone changed */
	g_signal_connect((priv->start_timezone), "changed",
			    G_CALLBACK (start_timezone_changed_cb), epage);

	e_meeting_list_view_column_set_visible (priv->list_view, E_MEETING_STORE_ATTENDEE_COL, TRUE);
	e_meeting_list_view_column_set_visible (priv->list_view, E_MEETING_STORE_ROLE_COL, calendar_config_get_show_role ());
	e_meeting_list_view_column_set_visible (priv->list_view, E_MEETING_STORE_RSVP_COL, calendar_config_get_show_rsvp ());
	e_meeting_list_view_column_set_visible (priv->list_view, E_MEETING_STORE_STATUS_COL, calendar_config_get_show_status ());
	e_meeting_list_view_column_set_visible (priv->list_view, E_MEETING_STORE_TYPE_COL, calendar_config_get_show_type ());

	selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (priv->list_view));
	gtk_tree_selection_set_mode (selection, GTK_SELECTION_MULTIPLE);

	g_signal_connect (G_OBJECT (priv->list_view), "button_press_event", G_CALLBACK (button_press_event), epage);
	g_signal_connect (G_OBJECT (priv->list_view), "event", G_CALLBACK (list_view_event), epage);
	g_signal_connect (priv->list_view, "key_press_event", G_CALLBACK (list_key_press), epage);

	/* Add attendee button */
	g_signal_connect (priv->add, "clicked", G_CALLBACK (add_clicked_cb), epage);

	/* Remove attendee button */
	g_signal_connect (priv->remove, "clicked", G_CALLBACK (remove_clicked_cb), epage);

	/* Edit attendee button */
	g_signal_connect (priv->edit, "clicked", G_CALLBACK (edit_clicked_cb), epage);

	/* Contacts button */
	g_signal_connect(priv->invite, "clicked", G_CALLBACK (invite_cb), epage);

	/* Alarm dialog */
	g_signal_connect (GTK_DIALOG (priv->alarm_dialog), "response", G_CALLBACK (gtk_widget_hide), priv->alarm_dialog);
	g_signal_connect (GTK_DIALOG (priv->alarm_dialog), "delete-event", G_CALLBACK (gtk_widget_hide), priv->alarm_dialog);
	priv->alarm_list_dlg_widget = alarm_list_dialog_peek (client, priv->alarm_list_store);
	gtk_widget_reparent (priv->alarm_list_dlg_widget, priv->alarm_box);
	gtk_widget_show_all (priv->alarm_list_dlg_widget);
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
	gtk_combo_box_set_active (GTK_COMBO_BOX (priv->end_time_combo), 1);
	gtk_widget_hide (priv->time_hour);
	gtk_widget_show (priv->end_time);
	g_signal_connect (priv->end_time_combo, "changed", G_CALLBACK (time_sel_changed), epage);
	update_end_time_combo ( epage);

	/* Hour and Minute selector */
	gtk_spin_button_set_range( (GtkSpinButton*) priv->hour_selector, 0, G_MAXINT);
	g_signal_connect (priv->hour_selector, "value-changed", G_CALLBACK (hour_sel_changed), epage);
	g_signal_connect (priv->minute_selector, "value-changed", G_CALLBACK (minute_sel_changed), epage);

	/* Add the user defined time if necessary */
	priv->alarm_units = calendar_config_get_default_reminder_units ();
	priv->alarm_interval = calendar_config_get_default_reminder_interval ();

	combo_label = NULL;
	switch (priv->alarm_units) {
	case CAL_DAYS:
		if (priv->alarm_interval != 1) {
			combo_label = g_strdup_printf (ngettext("%d day before appointment", "%d days before appointment", priv->alarm_interval), priv->alarm_interval);
		}
		break;

	case CAL_HOURS:
		if (priv->alarm_interval != 1) {
			combo_label = g_strdup_printf (ngettext("%d hour before appointment", "%d hours before appointment", priv->alarm_interval), priv->alarm_interval);
		}
		break;

	case CAL_MINUTES:
		if (priv->alarm_interval != 15) {
			combo_label = g_strdup_printf (ngettext("%d minute before appointment", "%d minutes before appointment", priv->alarm_interval), priv->alarm_interval);
		}
		break;
	}

	if (combo_label) {
		gtk_combo_box_append_text (GTK_COMBO_BOX (priv->alarm_time_combo), combo_label);
		g_free (combo_label);
		priv->alarm_map = alarm_map_with_user_time;
	} else {
		priv->alarm_map = alarm_map_without_user_time;
	}

	gtk_combo_box_append_text (GTK_COMBO_BOX (priv->alarm_time_combo), _("Customize"));
	gtk_combo_box_prepend_text (GTK_COMBO_BOX (priv->alarm_time_combo), _("None"));

	g_signal_connect_swapped (
		priv->alarm_time_combo, "changed",
		G_CALLBACK (comp_editor_page_changed), epage);
	g_signal_connect (
		priv->alarm_time_combo, "changed",
		G_CALLBACK (alarm_changed_cb), epage);

	/* Belongs to priv->description */
	g_signal_connect_swapped (
		text_buffer, "changed",
		G_CALLBACK (comp_editor_page_changed), epage);
	g_signal_connect_swapped (
		priv->summary, "changed",
		G_CALLBACK (comp_editor_page_changed), epage);
	g_signal_connect_swapped (
		priv->location, "changed",
		G_CALLBACK (comp_editor_page_changed), epage);
	g_signal_connect_swapped (
		priv->start_time, "changed",
		G_CALLBACK (comp_editor_page_changed), epage);
	g_signal_connect_swapped (
		priv->end_time, "changed",
		G_CALLBACK (comp_editor_page_changed), epage);
	g_signal_connect_swapped (
		priv->categories, "changed",
		G_CALLBACK (comp_editor_page_changed), epage);
	g_signal_connect_swapped (
		priv->source_selector, "changed",
		G_CALLBACK (comp_editor_page_changed), epage);
	g_signal_connect_swapped (
		priv->start_timezone, "changed",
		G_CALLBACK (comp_editor_page_changed), epage);

	/* Set the default timezone, so the timezone entry may be hidden. */
	zone = calendar_config_get_icaltimezone ();
	e_timezone_entry_set_default_timezone (E_TIMEZONE_ENTRY (priv->start_timezone), zone);
	e_timezone_entry_set_default_timezone (E_TIMEZONE_ENTRY (priv->end_timezone), zone);

	event_page_set_show_timezone (epage, calendar_config_get_show_timezone());

	return TRUE;
}



static void
event_page_select_organizer (EventPage *epage, const gchar *backend_address)
{
	EventPagePrivate *priv = epage->priv;
	CompEditor *editor;
	GList *l;
	ECal *client;
	EAccount *def_account;
	gchar *def_address = NULL;
	const gchar *default_address;
	gboolean subscribed_cal = FALSE;
	ESource *source = NULL;
	const gchar *user_addr = NULL;

	def_account = itip_addresses_get_default();
	if (def_account && def_account->enabled)
		def_address = g_strdup_printf("%s <%s>", def_account->id->name, def_account->id->address);

	editor = comp_editor_page_get_editor (COMP_EDITOR_PAGE (epage));
	client = comp_editor_get_client (editor);

	if (client)
		source = e_cal_get_source (client);
	if (source)
		user_addr = e_source_get_property (source, "subscriber");

	if (user_addr)
		subscribed_cal = TRUE;
	else
		user_addr = (backend_address && *backend_address) ? backend_address : NULL;

	default_address = NULL;
	if (user_addr)
		for (l = priv->address_strings; l != NULL; l = l->next)
			if (g_strrstr ((gchar *) l->data, user_addr) != NULL) {
				default_address = (const gchar *) l->data;
				break;
			}

	if (!default_address && def_address)
		default_address = def_address;

	if (default_address) {
		if (!priv->comp || !e_cal_component_has_organizer (priv->comp)) {
			gtk_entry_set_text (GTK_ENTRY (gtk_bin_get_child (GTK_BIN (priv->organizer))), default_address);
			gtk_widget_set_sensitive (priv->organizer, !subscribed_cal);
		}
	} else
		g_warning ("No potential organizers!");

	g_free (def_address);
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
event_page_construct (EventPage *epage, EMeetingStore *model)
{
	EventPagePrivate *priv;
	EIterator *it;
	EAccount *a;
	gchar *gladefile;

	priv = epage->priv;
	g_object_ref (model);
	priv->model = model;

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

	priv->accounts = itip_addresses_get ();
	for (it = e_list_get_iterator((EList *)priv->accounts);
	     e_iterator_is_valid(it);
	     e_iterator_next(it)) {
		gchar *full = NULL;

		a = (EAccount *)e_iterator_get(it);

		/* skip disabled accounts */
		if (!a->enabled)
			continue;

		full = g_strdup_printf("%s <%s>", a->id->name, a->id->address);

		priv->address_strings = g_list_append(priv->address_strings, full);
	}

	g_object_unref(it);

	if (priv->address_strings) {
		GList *l;

		for (l = priv->address_strings; l; l = l->next)
			gtk_combo_box_append_text (GTK_COMBO_BOX (priv->organizer), l->data);

		gtk_combo_box_set_active (GTK_COMBO_BOX (priv->organizer), 0);
	} else
		g_warning ("No potential organizers!");

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
event_page_new (EMeetingStore *model, CompEditor *editor)
{
	EventPage *epage;

	epage = g_object_new (TYPE_EVENT_PAGE, "editor", editor, NULL);
	if (!event_page_construct (epage, model)) {
		g_object_unref (epage);
		g_return_val_if_reached (NULL);
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

GtkWidget *event_page_create_source_combo_box (void);

GtkWidget *
event_page_create_source_combo_box (void)
{
	GtkWidget   *combo_box;
	GConfClient *gconf_client;
	ESourceList *source_list;

	gconf_client = gconf_client_get_default ();
	source_list = e_source_list_new_for_gconf (
		gconf_client, "/apps/evolution/calendar/sources");

	combo_box = e_source_combo_box_new (source_list);
	g_object_unref (source_list);
	g_object_unref (gconf_client);

	gtk_widget_show (combo_box);
	return combo_box;
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
	gint i;

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

ENameSelector *
event_page_get_name_selector (EventPage *epage)
{
	EventPagePrivate *priv;

	g_return_val_if_fail (epage != NULL, NULL);
	g_return_val_if_fail (IS_EVENT_PAGE (epage), NULL);

	priv = epage->priv;

	return e_meeting_list_view_get_name_selector (priv->list_view);
}

/**
 * event_page_add_attendee
 * Add attendee to meeting store and name selector.
 * @param epage EventPage.
 * @param attendee Attendee to be added.
 **/
void
event_page_add_attendee (EventPage *epage, EMeetingAttendee *attendee)
{
	EventPagePrivate *priv;

	g_return_if_fail (epage != NULL);
	g_return_if_fail (IS_EVENT_PAGE (epage));

	priv = epage->priv;

	e_meeting_store_add_attendee (priv->model, attendee);
	e_meeting_list_view_add_attendee_to_name_selector (E_MEETING_LIST_VIEW (priv->list_view), attendee);
}

/**
 * event_page_remove_all_attendees
 * Removes all attendees from the meeting store and name selector.
 * @param epage EventPage.
 **/
void
event_page_remove_all_attendees (EventPage *epage)
{
	EventPagePrivate *priv;

	g_return_if_fail (epage != NULL);
	g_return_if_fail (IS_EVENT_PAGE (epage));

	priv = epage->priv;

	e_meeting_store_remove_all_attendees (priv->model);
	e_meeting_list_view_remove_all_attendees_from_name_selector (E_MEETING_LIST_VIEW (priv->list_view));
}

