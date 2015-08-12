/*
 * Evolution calendar - Main page of the event editor dialog
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
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

#include <errno.h>
#include <string.h>
#include <gtk/gtk.h>
#include <glib/gi18n.h>
#include <gdk/gdkkeysyms.h>

#include "e-util/e-util.h"

#include "../e-alarm-list.h"
#include "../e-meeting-attendee.h"
#include "../e-meeting-list-view.h"
#include "../e-meeting-store.h"
#include "../e-timezone-entry.h"

#include "alarm-list-dialog.h"
#include "comp-editor-util.h"
#include "comp-editor.h"
#include "e-send-options-utils.h"
#include "event-page.h"

#define EVENT_PAGE_GET_PRIVATE(obj) \
	(G_TYPE_INSTANCE_GET_PRIVATE \
	((obj), TYPE_EVENT_PAGE, EventPagePrivate))

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
	GtkBuilder *builder;

	/* Widgets from the UI file */
	GtkWidget *main;

	/* Generic informative messages placeholder */
	GtkWidget *info_hbox;
	GtkWidget *info_icon;
	GtkWidget *info_string;

	GtkWidget *summary;
	GtkWidget *summary_label;
	GtkWidget *location;
	GtkWidget *location_label;
	GtkEntryCompletion *location_completion;

	gchar **address_strings;
	gchar *fallback_address;
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

	GtkWidget *client_combo_box;

	/* Meeting related items */
	GtkWidget *list_box;
	GtkWidget *organizer_table;
	GtkWidget *organizer;
	GtkWidget *add;
	GtkWidget *remove;
	GtkWidget *edit;
	GtkWidget *invite;
	GtkWidget *invite_label;

	/* ListView stuff */
	EMeetingStore *meeting_store;
	EMeetingListView *list_view;
	gint row;

	/* For handling who the organizer is */
	gboolean user_org;
	gboolean existing;

	EAlarmList *alarm_list_store;

	gboolean sendoptions_shown;

	ESendOptionsDialog *sod;
	gchar *old_summary;
	EDurationType alarm_units;
	gint alarm_interval;

	/* This is TRUE if both the start & end timezone are the same. If the
	 * start timezone is then changed, we updated the end timezone to the
	 * same value, since 99% of events start and end in one timezone. */
	gboolean sync_timezones;
	gboolean is_meeting;

	GtkWidget *alarm_list_dlg_widget;

	/* either with-user-time or without it */
	const gint *alarm_map;

	GCancellable *connect_cancellable;
};

static void event_page_set_dates (CompEditorPage *page, CompEditorPageDates *dates);
static void notify_dates_changed (EventPage *epage, struct icaltimetype *start_tt, struct icaltimetype *end_tt);
static gboolean check_start_before_end (struct icaltimetype *start_tt, icaltimezone *start_zone,
					struct icaltimetype *end_tt, icaltimezone *end_zone, gboolean adjust_end_time);
static void set_attendees (ECalComponent *comp, const GPtrArray *attendees);
static void hour_sel_changed (GtkSpinButton *widget, EventPage *epage);
static void minute_sel_changed (GtkSpinButton *widget, EventPage *epage);
static void hour_minute_changed (EventPage *epage);
static void update_end_time_combo (EventPage *epage);
static void event_page_select_organizer (EventPage *epage, const gchar *backend_address);
static void set_subscriber_info_string (EventPage *epage, const gchar *backend_address);

G_DEFINE_TYPE (EventPage, event_page, TYPE_COMP_EDITOR_PAGE)

static gboolean
get_current_identity (EventPage *page,
                      gchar **name,
                      gchar **mailto)
{
	EShell *shell;
	CompEditor *editor;
	ESourceRegistry *registry;
	GList *list, *iter;
	GtkWidget *entry;
	const gchar *extension_name;
	const gchar *text;
	gboolean match = FALSE;

	entry = gtk_bin_get_child (GTK_BIN (page->priv->organizer));
	text = gtk_entry_get_text (GTK_ENTRY (entry));

	if (text == NULL || *text == '\0')
		return FALSE;

	editor = comp_editor_page_get_editor (COMP_EDITOR_PAGE (page));
	shell = comp_editor_get_shell (editor);

	registry = e_shell_get_registry (shell);
	extension_name = E_SOURCE_EXTENSION_MAIL_IDENTITY;

	list = e_source_registry_list_sources (registry, extension_name);

	for (iter = list; !match && iter != NULL; iter = g_list_next (iter)) {
		ESource *source = E_SOURCE (iter->data);
		ESourceMailIdentity *extension;
		const gchar *id_name;
		const gchar *id_address;
		gchar *identity;

		extension = e_source_get_extension (source, extension_name);

		id_name = e_source_mail_identity_get_name (extension);
		id_address = e_source_mail_identity_get_address (extension);

		if (id_name == NULL || id_address == NULL)
			continue;

		identity = g_strdup_printf ("%s <%s>", id_name, id_address);
		match = (g_ascii_strcasecmp (text, identity) == 0);
		g_free (identity);

		if (match && name != NULL)
			*name = g_strdup (id_name);

		if (match && mailto != NULL)
			*mailto = g_strdup_printf ("MAILTO:%s", id_address);
	}

	g_list_free_full (list, (GDestroyNotify) g_object_unref);

	return match;
}

static void
set_all_day_event_menu (EventPage *epage,
                        gboolean active)
{
	CompEditor *editor;
	GtkAction *action;

	editor = comp_editor_page_get_editor (COMP_EDITOR_PAGE (epage));
	action = comp_editor_get_action (editor, "all-day-event");
	gtk_toggle_action_set_active (GTK_TOGGLE_ACTION (action), active);
}

/* Sets the 'All Day Event' flag to the given value (without emitting signals),
 * and shows or hides the widgets as appropriate. */
static void
set_all_day (EventPage *epage,
             gboolean all_day)
{
	set_all_day_event_menu (epage, all_day);

	/* TODO implement for in end time selector */
	if (all_day)
		gtk_combo_box_set_active (
			GTK_COMBO_BOX (epage->priv->end_time_combo), 1);
	gtk_widget_set_sensitive (epage->priv->end_time_combo, !all_day);

	e_date_edit_set_show_time (
		E_DATE_EDIT (epage->priv->start_time), !all_day);
	e_date_edit_set_show_time (
		E_DATE_EDIT (epage->priv->end_time), !all_day);
}

static void
enable_busy_time_menu (EventPage *epage,
                       gboolean sensitive)
{
	CompEditor *editor;
	GtkAction *action;

	editor = comp_editor_page_get_editor (COMP_EDITOR_PAGE (epage));
	action = comp_editor_get_action (editor, "show-time-busy");
	gtk_action_set_sensitive (action, sensitive);
}

static void
set_busy_time_menu (EventPage *epage,
                    gboolean active)
{
	CompEditor *editor;
	GtkAction *action;

	editor = comp_editor_page_get_editor (COMP_EDITOR_PAGE (epage));
	action = comp_editor_get_action (editor, "show-time-busy");
	gtk_toggle_action_set_active (GTK_TOGGLE_ACTION (action), active);
}

static void
clear_widgets (EventPage *epage)
{
	EventPagePrivate *priv = epage->priv;
	CompEditor *editor;

	editor = comp_editor_page_get_editor (COMP_EDITOR_PAGE (epage));

	/* Summary, description */
	gtk_entry_set_text (GTK_ENTRY (priv->summary), "");
	gtk_entry_set_text (GTK_ENTRY (priv->location), "");
	gtk_text_buffer_set_text (gtk_text_view_get_buffer (GTK_TEXT_VIEW (priv->description)), "", 0);
	e_buffer_tagger_update_tags (GTK_TEXT_VIEW (priv->description));

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
	gtk_entry_set_text (GTK_ENTRY (priv->categories), "");
}

static gboolean
is_custom_alarm (ECalComponentAlarm *ca,
                 gchar *old_summary,
                 EDurationType user_units,
                 gint user_interval,
                 gint *alarm_type)
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
		case E_DURATION_DAYS:
			if (trigger.u.rel_duration.days == user_interval
			    && trigger.u.rel_duration.hours == 0
			    && trigger.u.rel_duration.minutes == 0) {
				if (alarm_type)
					*alarm_type = ALARM_USER_TIME;
				return FALSE;
			}
			break;

		case E_DURATION_HOURS:
			if (trigger.u.rel_duration.days == 0
			    && trigger.u.rel_duration.hours == user_interval
			    && trigger.u.rel_duration.minutes == 0) {
				if (alarm_type)
					*alarm_type = ALARM_USER_TIME;
				return FALSE;
			}
			break;

		case E_DURATION_MINUTES:
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
is_custom_alarm_store (EAlarmList *alarm_list_store,
                       gchar *old_summary,
                       EDurationType user_units,
                       gint user_interval,
                       gint *alarm_type)
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
	if (is_custom_alarm ((ECalComponentAlarm *) alarm, old_summary, user_units, user_interval, alarm_type))
		return TRUE;

	valid_iter = gtk_tree_model_iter_next (model, &iter);
	if (valid_iter)
		return TRUE;

	return FALSE;
}

static gboolean
is_custom_alarm_uid_list (ECalComponent *comp,
                          GList *alarms,
                          gchar *old_summary,
                          EDurationType user_units,
                          gint user_interval,
                          gint *alarm_type)
{
	ECalComponentAlarm *ca;
	gboolean result;

	if (g_list_length (alarms) > 1)
		return TRUE;

	ca = e_cal_component_get_alarm (comp, alarms->data);
	result = is_custom_alarm (
		ca, old_summary, user_units, user_interval, alarm_type);
	e_cal_component_alarm_free (ca);

	return result;
}

/* returns whether changed info text */
static gboolean
check_starts_in_the_past (EventPage *epage)
{
	EventPagePrivate *priv;
	struct icaltimetype start_tt = icaltime_null_time ();
	gboolean date_set;

	if ((comp_editor_get_flags (comp_editor_page_get_editor (COMP_EDITOR_PAGE (epage))) & COMP_EDITOR_NEW_ITEM) == 0)
		return FALSE;

	priv = epage->priv;
	date_set = e_date_edit_get_date (E_DATE_EDIT (priv->start_time), &start_tt.year, &start_tt.month, &start_tt.day);

	g_return_val_if_fail (date_set, FALSE);

	if (priv->all_day_event) {
		start_tt.is_date = TRUE;
	} else {
		e_date_edit_get_time_of_day (E_DATE_EDIT (priv->start_time), &start_tt.hour, &start_tt.minute);
		start_tt.zone = e_timezone_entry_get_timezone (E_TIMEZONE_ENTRY (priv->start_timezone));
	}

	if (comp_editor_test_time_in_the_past (start_tt)) {
		gchar *tmp = g_strconcat ("<b>", _("Event's start time is in the past"), "</b>", NULL);
		event_page_set_info_string (epage, "dialog-warning", tmp);
		g_free (tmp);
	} else {
		event_page_set_info_string (epage, NULL, NULL);
	}

	return TRUE;
}

static void
alarm_image_button_clicked_cb (GtkWidget *button,
                               EventPage *epage)
{
	CompEditor *editor;
	GtkAction *action;

	g_return_if_fail (IS_EVENT_PAGE (epage));

	editor = comp_editor_page_get_editor (COMP_EDITOR_PAGE (epage));
	action = comp_editor_get_action (editor, "alarms");
	gtk_action_activate (action);
}

static GtkWidget *
create_alarm_image_button (const gchar *image_text,
                           const gchar *tip_text,
                           EventPage *epage)
{
	GtkWidget *image, *button;

	button = gtk_button_new ();
	gtk_button_set_relief (GTK_BUTTON (button), GTK_RELIEF_NONE);
	gtk_widget_set_can_focus (button, FALSE);

	image = gtk_image_new_from_icon_name (image_text, GTK_ICON_SIZE_MENU);

	gtk_container_add ((GtkContainer *) button, image);
	gtk_widget_show_all (button);
	gtk_widget_set_tooltip_text (button, tip_text);

	g_signal_connect (
		button, "clicked",
		G_CALLBACK (alarm_image_button_clicked_cb), epage);

	return button;
}

static void
sensitize_widgets (EventPage *epage)
{
	ECalClient *client;
	EShell *shell;
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
	shell = comp_editor_get_shell (editor);

	priv = epage->priv;
	if (flags & COMP_EDITOR_MEETING)
		sens = flags & COMP_EDITOR_USER_ORG;

	read_only = e_client_is_readonly (E_CLIENT (client));

	delegate = flags & COMP_EDITOR_DELEGATE;

	sensitize = !read_only && sens;

	if (read_only) {
		gchar *tmp = g_strconcat ("<b>", _("Event cannot be edited, because the selected calendar is read only"), "</b>", NULL);
		event_page_set_info_string (epage, "dialog-information", tmp);
		g_free (tmp);
	} else if (!sens) {
		gchar *tmp = g_strconcat ("<b>", _("Event cannot be fully edited, because you are not the organizer"), "</b>", NULL);
		event_page_set_info_string (epage, "dialog-information", tmp);
		g_free (tmp);
	} else if (!check_starts_in_the_past (epage)) {
		event_page_set_info_string (epage, NULL, NULL);
	}

	alarm = e_dialog_combo_box_get (priv->alarm_time_combo, priv->alarm_map) != ALARM_NONE;
	custom = is_custom_alarm_store (priv->alarm_list_store, priv->old_summary, priv->alarm_units, priv->alarm_interval, NULL) ||
		 e_dialog_combo_box_get (priv->alarm_time_combo, priv->alarm_map) == ALARM_CUSTOM ? TRUE : FALSE;

	if (alarm && !priv->alarm_icon) {
		priv->alarm_icon = create_alarm_image_button ("stock_bell", _("This event has reminders"), epage);
		gtk_box_pack_start ((GtkBox *) priv->status_icons, priv->alarm_icon, FALSE, FALSE, 6);
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
		gtk_widget_set_sensitive (priv->client_combo_box, FALSE);
	}

	gtk_widget_set_sensitive (priv->organizer, !read_only);
	gtk_widget_set_sensitive (priv->add, (!read_only && sens) || delegate);
	gtk_widget_set_sensitive (priv->edit, (!read_only && sens) || delegate);
	e_meeting_list_view_set_editable (priv->list_view, (!read_only && sens) || delegate);
	gtk_widget_set_sensitive (priv->remove, (!read_only && sens) || delegate);
	gtk_widget_set_sensitive (priv->invite, (!read_only && sens) || delegate);
	gtk_widget_set_sensitive (GTK_WIDGET (priv->list_view), !read_only);

	action_group = comp_editor_get_action_group (editor, "editable");
	gtk_action_group_set_sensitive (action_group, !read_only);

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
		gtk_label_set_mnemonic_widget ((GtkLabel *) priv->org_cal_label, priv->client_combo_box);
	} else {
		gtk_widget_show (priv->calendar_label);
		gtk_widget_show (priv->list_box);
		if (!e_shell_get_express_mode (shell))
			gtk_widget_show (priv->attendee_box);
		gtk_widget_show (priv->organizer);
		gtk_label_set_text_with_mnemonic ((GtkLabel *) priv->org_cal_label, _("Or_ganizer:"));
	}
}

static void
update_time (EventPage *epage,
             ECalComponentDateTime *start_date,
             ECalComponentDateTime *end_date)
{
	CompEditor *editor;
	ECalClient *client;
	GtkAction *action;
	struct icaltimetype *start_tt, *end_tt, implied_tt;
	icaltimezone *start_zone = NULL, *def_zone = NULL;
	gboolean all_day_event, homezone = TRUE;
	gboolean show_timezone;

	editor = comp_editor_page_get_editor (COMP_EDITOR_PAGE (epage));
	client = comp_editor_get_client (editor);

	if (start_date->tzid) {
		/* Note that if we are creating a new event, the timezones may not be
		 * on the server, so we try to get the builtin timezone with the TZID
		 * first. */
		start_zone = icaltimezone_get_builtin_timezone_from_tzid (start_date->tzid);
		if (!start_zone) {
			/* FIXME: Handle error better. */
			GError *error = NULL;

			e_cal_client_get_timezone_sync (
				client, start_date->tzid,
				&start_zone, NULL, &error);

			if (error != NULL) {
				g_warning (
					"Couldn't get timezone '%s' from server: %s",
					start_date->tzid ? start_date->tzid : "",
					error->message);
				g_error_free (error);
			}
		}
	}

	/* If both times are DATE values, we set the 'All Day Event' checkbox.
	 * Also, if DTEND is after DTSTART, we subtract 1 day from it. */
	all_day_event = FALSE;
	start_tt = start_date->value;
	end_tt = end_date->value;
	if (!end_tt && start_tt->is_date) {
		end_tt = &implied_tt;
		*end_tt = *start_tt;
		icaltime_adjust (end_tt, 1, 0, 0, 0);
	} else if (!end_tt) {
		end_tt = &implied_tt;
		*end_tt = *start_tt;
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
	 * timezone, so that if the user toggles the 'All Day Event' checkbox
	 * the event uses the current timezone rather than none at all. */
	if (all_day_event)
		start_zone = e_meeting_store_get_timezone (
			epage->priv->meeting_store);

	g_signal_handlers_block_matched (
		epage->priv->start_time, G_SIGNAL_MATCH_DATA,
		0, 0, NULL, NULL, epage);
	g_signal_handlers_block_matched (
		epage->priv->end_time, G_SIGNAL_MATCH_DATA,
		0, 0, NULL, NULL, epage);

	e_date_edit_set_date (
		E_DATE_EDIT (epage->priv->start_time),
		start_tt->year, start_tt->month, start_tt->day);
	e_date_edit_set_time_of_day (
		E_DATE_EDIT (epage->priv->start_time),
		start_tt->hour, start_tt->minute);

	e_date_edit_set_date (
		E_DATE_EDIT (epage->priv->end_time),
		end_tt->year, end_tt->month, end_tt->day);
	e_date_edit_set_time_of_day (
		E_DATE_EDIT (epage->priv->end_time),
		end_tt->hour, end_tt->minute);

	g_signal_handlers_unblock_matched (
		epage->priv->start_time, G_SIGNAL_MATCH_DATA,
		0, 0, NULL, NULL, epage);
	g_signal_handlers_unblock_matched (
		epage->priv->end_time, G_SIGNAL_MATCH_DATA,
		0, 0, NULL, NULL, epage);

	/* Set the timezones, and set sync_timezones to TRUE if both timezones
	 * are the same. */
	g_signal_handlers_block_matched (
		epage->priv->start_timezone, G_SIGNAL_MATCH_DATA,
		0, 0, NULL, NULL, epage);
	g_signal_handlers_block_matched (
		epage->priv->end_timezone, G_SIGNAL_MATCH_DATA,
		0, 0, NULL, NULL, epage);

	if (start_zone)
		e_timezone_entry_set_timezone (
			E_TIMEZONE_ENTRY (epage->priv->start_timezone),
			start_zone);
	def_zone = e_meeting_store_get_timezone (epage->priv->meeting_store);
	if (!def_zone || !start_zone || strcmp (icaltimezone_get_tzid (def_zone), icaltimezone_get_tzid (start_zone)))
		 homezone = FALSE;

	action = comp_editor_get_action (editor, "view-time-zone");
	show_timezone = gtk_toggle_action_get_active (GTK_TOGGLE_ACTION (action));
	event_page_set_show_timezone (epage, (show_timezone || !homezone) & !all_day_event);

	/*unblock the endtimezone widget*/
	g_signal_handlers_unblock_matched (
		epage->priv->end_timezone, G_SIGNAL_MATCH_DATA,
		0, 0, NULL, NULL, epage);
	g_signal_handlers_unblock_matched (
		epage->priv->start_timezone, G_SIGNAL_MATCH_DATA,
		0, 0, NULL, NULL, epage);

	epage->priv->sync_timezones = TRUE;

	update_end_time_combo (epage);
}

static void
organizer_changed_cb (GtkEntry *entry,
                      EventPage *epage)
{
	gchar *name;
	gchar *mailto;

	g_return_if_fail (GTK_IS_ENTRY (entry));
	g_return_if_fail (IS_EVENT_PAGE (epage));

	if (!epage->priv->ia)
		return;

	if (!get_current_identity (epage, &name, &mailto))
		return;

	/* XXX EMeetingAttendee takes ownership of the strings. */
	e_meeting_attendee_set_cn (epage->priv->ia, name);
	e_meeting_attendee_set_address (epage->priv->ia, mailto);
}

static void
event_page_dispose (GObject *object)
{
	EventPagePrivate *priv;

	priv = EVENT_PAGE_GET_PRIVATE (object);

	if (priv->connect_cancellable != NULL) {
		g_cancellable_cancel (priv->connect_cancellable);
		g_object_unref (priv->connect_cancellable);
		priv->connect_cancellable = NULL;
	}

	if (priv->location_completion != NULL) {
		g_object_unref (priv->location_completion);
		priv->location_completion = NULL;
	}

	if (priv->comp != NULL) {
		g_object_unref (priv->comp);
		priv->comp = NULL;
	}

	if (priv->main != NULL) {
		g_object_unref (priv->main);
		priv->main = NULL;
	}

	if (priv->builder != NULL) {
		g_object_unref (priv->builder);
		priv->builder = NULL;
	}

	if (priv->alarm_time_combo) {
		g_signal_handlers_disconnect_matched (priv->alarm_time_combo, G_SIGNAL_MATCH_DATA, 0, 0, NULL, NULL, object);
	}

	if (priv->alarm_list_store != NULL) {
		g_signal_handlers_disconnect_matched (priv->alarm_list_store, G_SIGNAL_MATCH_DATA, 0, 0, NULL, NULL, object);

		g_object_unref (priv->alarm_list_store);
		priv->alarm_list_store = NULL;
	}

	if (priv->sod != NULL) {
		g_object_unref (priv->sod);
		priv->sod = NULL;
	}

	if (priv->alarm_dialog) {
		gtk_widget_destroy (priv->alarm_dialog);
		priv->alarm_dialog = NULL;
	}

	/* Chain up to parent's dispose() method. */
	G_OBJECT_CLASS (event_page_parent_class)->dispose (object);
}

static void
event_page_finalize (GObject *object)
{
	EventPagePrivate *priv;

	priv = EVENT_PAGE_GET_PRIVATE (object);

	g_strfreev (priv->address_strings);
	g_free (priv->fallback_address);

	g_ptr_array_foreach (
		priv->deleted_attendees, (GFunc) g_object_unref, NULL);
	g_ptr_array_free (priv->deleted_attendees, TRUE);

	g_free (priv->old_summary);
	g_free (priv->user_add);

	priv->alarm_list_dlg_widget = NULL;

	/* Chain up to parent's finalize() method. */
	G_OBJECT_CLASS (event_page_parent_class)->finalize (object);
}

static GtkWidget *
event_page_get_widget (CompEditorPage *page)
{
	EventPage *event_page = EVENT_PAGE (page);

	return event_page->priv->main;
}

static void
event_page_focus_main_widget (CompEditorPage *page)
{
	EventPage *event_page = EVENT_PAGE (page);

	gtk_widget_grab_focus (event_page->priv->summary);
}

static void
event_page_load_locations_list (CompEditorPage *page,
                                ECalComponent *comp)
{
	EShell *shell;
	EShellBackend *backend;
	EventPagePrivate *priv;
	CompEditor *editor;
	GtkListStore *store;
	GError *error;

	const gchar *cache_dir;
	gchar *file_name, *contents;
	gchar **locations;
	gint row;

	priv = EVENT_PAGE (page)->priv;
	editor = comp_editor_page_get_editor (page);

	shell = comp_editor_get_shell (editor);
	backend = e_shell_get_backend_by_name (shell, "calendar");
	cache_dir = e_shell_backend_get_config_dir (backend);
	file_name = g_build_filename (cache_dir, "locations", NULL);

	if (!g_file_test (file_name, G_FILE_TEST_EXISTS)) {
		g_free (file_name);
		return;
	}

	error = NULL;
	if (!g_file_get_contents (file_name, &contents, NULL, &error)) {
		if (error != NULL) {
			g_warning (
				"%s: Failed to load locations list: %s",
				G_STRFUNC, error->message);
			g_error_free (error);
		}

		g_free (file_name);
		return;
	}

	locations = g_strsplit (contents, "\n", 0);
	if (!locations) {
		g_free (contents);
		g_free (file_name);
		return;
	}

	row = 0;
	store = GTK_LIST_STORE (gtk_entry_completion_get_model (priv->location_completion));
	while (locations[row] && *locations[row]) {
		GtkTreeIter iter;
		gtk_list_store_append (store, &iter);
		gtk_list_store_set (store, &iter, 0, locations[row], -1);
		row++;
	}

	g_strfreev (locations);
	g_free (contents);
	g_free (file_name);
}

static void
event_page_save_locations_list (CompEditorPage *page,
                                ECalComponent *comp)
{
	EShell *shell;
	EShellBackend *backend;
	EventPagePrivate *priv;
	CompEditor *editor;
	GError *error;
	GtkTreeModel *model;
	GtkTreeIter iter;

	const gchar *cache_dir;
	const gchar *current_location;
	gchar *file_name;
	GString *contents;

	priv = EVENT_PAGE (page)->priv;
	editor = comp_editor_page_get_editor (page);

	shell = comp_editor_get_shell (editor);
	backend = e_shell_get_backend_by_name (shell, "calendar");
	cache_dir = e_shell_backend_get_config_dir (backend);
	file_name = g_build_filename (cache_dir, "locations", NULL);

	if (!g_file_test (cache_dir, G_FILE_TEST_EXISTS | G_FILE_TEST_IS_DIR)) {
		gint r = g_mkdir_with_parents (cache_dir, 0700);
		if (r < 0) {
			g_warning ("%s: Failed to create %s: %s", G_STRFUNC, cache_dir, g_strerror (errno));
			g_free (file_name);
			return;
		}
	}

	current_location = gtk_entry_get_text (GTK_ENTRY (priv->location));

	/* Put current locatin on the very top of the list */
	contents = g_string_new (current_location);
	g_string_append_c (contents, '\n');

	model = gtk_entry_completion_get_model (priv->location_completion);
	if (gtk_tree_model_get_iter_first (model, &iter)) {
		gint i = 0;
		do {
			gchar *str;

			gtk_tree_model_get (model, &iter, 0, &str, -1);

			/* Skip the current location */
			if (str && *str && g_ascii_strcasecmp (str, current_location) != 0)
				g_string_append_printf (contents, "%s\n", str);

			g_free (str);

			i++;

		} while (gtk_tree_model_iter_next (model, &iter) && (i < 20));
	}

	error = NULL;
	g_file_set_contents (file_name, contents->str, -1, &error);
	if (error != NULL) {
		g_warning (
			"%s: Failed to save locations: %s",
			G_STRFUNC, error->message);
		g_error_free (error);
	}

	g_string_free (contents, TRUE);
	g_free (file_name);
}

static gboolean
event_page_fill_widgets (CompEditorPage *page,
                         ECalComponent *comp)
{
	ECalClient *client;
	CompEditor *editor;
	CompEditorFlags flags;
	EventPage *epage;
	EventPagePrivate *priv;
	ECalComponentText text;
	ECalComponentClassification cl;
	ECalComponentTransparency transparency;
	ECalComponentDateTime start_date, end_date;
	ESourceRegistry *registry;
	EShell *shell;
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
	shell = comp_editor_get_shell (editor);

	registry = e_shell_get_registry (shell);

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
	if (text.value != NULL)
		gtk_entry_set_text (GTK_ENTRY (priv->summary), text.value);
	else
		gtk_entry_set_text (GTK_ENTRY (priv->summary), "");
	priv->old_summary = g_strdup (text.value);

	e_cal_component_get_location (comp, &location);
	if (location != NULL)
		gtk_entry_set_text (GTK_ENTRY (priv->location), location);
	else
		gtk_entry_set_text (GTK_ENTRY (priv->location), "");
	event_page_load_locations_list (page, comp);

	e_cal_component_get_description_list (comp, &l);
	if (l && l->data) {
		ECalComponentText *dtext;

		dtext = l->data;
		gtk_text_buffer_set_text (
			gtk_text_view_get_buffer (GTK_TEXT_VIEW (priv->description)),
			dtext->value ? dtext->value : "", -1);
	} else {
		gtk_text_buffer_set_text (
			gtk_text_view_get_buffer (GTK_TEXT_VIEW (priv->description)),
			"", 0);
	}
	e_cal_component_free_text_list (l);
	e_buffer_tagger_update_tags (GTK_TEXT_VIEW (priv->description));

	e_client_get_backend_property_sync (E_CLIENT (client), CAL_BACKEND_PROPERTY_CAL_EMAIL_ADDRESS, &backend_addr, NULL, NULL);
	set_subscriber_info_string (epage, backend_addr);

	if (priv->is_meeting) {
		ECalComponentOrganizer organizer;
		gchar *name = NULL;
		gchar *mailto = NULL;

		priv->user_add = itip_get_comp_attendee (
			registry, comp, client);

		/* Organizer strings */
		event_page_select_organizer (epage, backend_addr);

		/* If there is an existing organizer show it properly */
		if (e_cal_component_has_organizer (comp)) {
			e_cal_component_get_organizer (comp, &organizer);
			if (organizer.value != NULL) {
				const gchar *strip = itip_strip_mailto (organizer.value);
				gchar *string;

				if (itip_organizer_is_user (registry, comp, client) ||
				    itip_sentby_is_user (registry, comp, client)) {
					priv->user_org = TRUE;
				} else {
					if (e_client_check_capability (
								E_CLIENT (client),
								CAL_STATIC_CAPABILITY_ORGANIZER_NOT_EMAIL_ADDRESS))
					gtk_widget_set_sensitive (priv->invite, FALSE);
					gtk_widget_set_sensitive (priv->add, FALSE);
					gtk_widget_set_sensitive (priv->edit, FALSE);
					gtk_widget_set_sensitive (priv->remove, FALSE);
					priv->user_org = FALSE;
				}

				if (e_client_check_capability (E_CLIENT (client), CAL_STATIC_CAPABILITY_NO_ORGANIZER) && (flags & COMP_EDITOR_DELEGATE))
					string = g_strdup (backend_addr);
				else if (organizer.cn != NULL)
					string = g_strdup_printf ("%s <%s>", organizer.cn, strip);
				else
					string = g_strdup (strip);

				g_signal_handlers_block_by_func (gtk_bin_get_child (GTK_BIN (priv->organizer)), organizer_changed_cb, epage);

				if (!priv->user_org) {
					GtkComboBox *combo_box;
					GtkListStore *list_store;
					GtkTreeModel *model;
					GtkTreeIter iter;

					combo_box = GTK_COMBO_BOX (priv->organizer);
					model = gtk_combo_box_get_model (combo_box);
					list_store = GTK_LIST_STORE (model);

					gtk_list_store_clear (list_store);
					gtk_list_store_append (list_store, &iter);
					gtk_list_store_set (list_store, &iter, 0, string, -1);
					gtk_combo_box_set_active (combo_box, 0);
					gtk_editable_set_editable (GTK_EDITABLE (gtk_bin_get_child (GTK_BIN (priv->organizer))), FALSE);
				} else {
					gtk_entry_set_text (GTK_ENTRY (gtk_bin_get_child (GTK_BIN (priv->organizer))), string);
				}

				g_signal_handlers_unblock_by_func (gtk_bin_get_child (GTK_BIN (priv->organizer)), organizer_changed_cb, epage);

				g_free (string);
				priv->existing = TRUE;
			}
		} else if (get_current_identity (epage, &name, &mailto)) {
			EMeetingAttendee *attendee;
			gchar *backend_mailto = NULL;

			if (backend_addr != NULL && *backend_addr != '\0') {
				backend_mailto = g_strdup_printf (
					"MAILTO:%s", backend_addr);
				if (g_ascii_strcasecmp (backend_mailto, mailto) == 0) {
					g_free (backend_mailto);
					backend_mailto = NULL;
				}
			}

			attendee =
				e_meeting_store_add_attendee_with_defaults (
				priv->meeting_store);
			priv->ia = g_object_ref (attendee);

			if (backend_mailto == NULL) {
				e_meeting_attendee_set_cn (attendee, name);
				e_meeting_attendee_set_address (attendee, mailto);
				name = mailto = NULL;
			} else {
				e_meeting_attendee_set_address (attendee, backend_mailto);
				e_meeting_attendee_set_sentby (attendee, mailto);
				backend_mailto = mailto = NULL;
			}

			if (client && e_cal_client_check_organizer_must_accept (client))
				e_meeting_attendee_set_status (
					attendee, ICAL_PARTSTAT_NEEDSACTION);
			else
				e_meeting_attendee_set_status (
					attendee, ICAL_PARTSTAT_ACCEPTED);

			e_meeting_list_view_add_attendee_to_name_selector (
				E_MEETING_LIST_VIEW (priv->list_view), attendee);

			g_free (backend_mailto);
		}

		g_free (mailto);
		g_free (name);
	}

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

	if (e_client_check_capability (E_CLIENT (client), CAL_STATIC_CAPABILITY_NO_TRANSPARENCY))
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
	if (categories != NULL)
		gtk_entry_set_text (GTK_ENTRY (priv->categories), categories);
	else
		gtk_entry_set_text (GTK_ENTRY (priv->categories), "");

	/* Source */
	e_source_combo_box_set_active (
		E_SOURCE_COMBO_BOX (priv->client_combo_box),
		e_client_get_source (E_CLIENT (client)));

	e_cal_component_get_uid (comp, &uid);
	if (!(flags & COMP_EDITOR_DELEGATE)
			&& !(flags && COMP_EDITOR_NEW_ITEM)) {
		event_page_hide_options (epage);
	}

	sensitize_widgets (epage);

	e_widget_undo_reset (priv->summary);
	e_widget_undo_reset (priv->location);
	e_widget_undo_reset (priv->categories);
	e_widget_undo_reset (priv->description);

	return validated;
}

static gboolean
event_page_fill_component (CompEditorPage *page,
                           ECalComponent *comp)
{
	CompEditor *editor;
	CompEditorFlags flags;
	ECalClient *client;
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

	str = gtk_editable_get_chars (GTK_EDITABLE (priv->summary), 0, -1);
	if (str == NULL || *str == '\0')
		e_cal_component_set_summary (comp, NULL);
	else {
		ECalComponentText text;

		text.value = str;
		text.altrep = NULL;

		e_cal_component_set_summary (comp, &text);
	}

	g_free (str);

	/* Location */

	str = gtk_editable_get_chars (GTK_EDITABLE (priv->location), 0, -1);
	if (str == NULL || *str == '\0')
		e_cal_component_set_location (comp, NULL);
	else {
		e_cal_component_set_location (comp, str);
		event_page_save_locations_list (page, comp);
	}

	g_free (str);

	/* Description */

	gtk_text_buffer_get_start_iter (text_buffer, &text_iter_start);
	gtk_text_buffer_get_end_iter   (text_buffer, &text_iter_end);
	str = gtk_text_buffer_get_text (text_buffer, &text_iter_start, &text_iter_end, FALSE);

	if (str == NULL || *str == '\0')
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
	start_date_set = e_date_edit_get_date (
		E_DATE_EDIT (priv->start_time),
		&start_tt.year,
		&start_tt.month,
		&start_tt.day);
	g_return_val_if_fail (start_date_set, FALSE);

	if (!e_date_edit_date_is_valid (E_DATE_EDIT (priv->end_time))) {
		comp_editor_page_display_validation_error (page, _("End date is wrong"), priv->end_time);
		return FALSE;
	}
	end_date_set = e_date_edit_get_date (
		E_DATE_EDIT (priv->end_time),
		&end_tt.year,
		&end_tt.month,
		&end_tt.day);
	g_return_val_if_fail (end_date_set, FALSE);

	/* If the all_day toggle is set, we use DATE values for DTSTART and
	 * DTEND. If not, we fetch the hour & minute from the widgets. */
	all_day_event = priv->all_day_event;

	if (all_day_event) {
		start_tt.is_date = TRUE;
		end_tt.is_date = TRUE;

		/* We have to add 1 day to DTEND, as it is not inclusive. */
		icaltime_adjust (&end_tt, 1, 0, 0, 0);

		if (e_client_check_capability (E_CLIENT (client), CAL_STATIC_CAPABILITY_ALL_DAY_EVENT_AS_TIME)) {
			icaltimezone *start_zone;

			start_tt.is_date = FALSE;
			start_tt.hour = 0;
			start_tt.minute = 0;
			start_tt.second = 0;

			end_tt.is_date = FALSE;
			end_tt.hour = 0;
			end_tt.minute = 0;
			end_tt.second = 0;

			start_zone = e_timezone_entry_get_timezone (E_TIMEZONE_ENTRY (priv->start_timezone));
			start_date.tzid = icaltimezone_get_tzid (start_zone);
			end_date.tzid = icaltimezone_get_tzid (start_zone);
		}
	} else {
		icaltimezone *start_zone;

		if (!e_date_edit_time_is_valid (E_DATE_EDIT (priv->start_time))) {
			comp_editor_page_display_validation_error (page, _("Start time is wrong"), priv->start_time);
			return FALSE;
		}
		e_date_edit_get_time_of_day (
			E_DATE_EDIT (priv->start_time),
			&start_tt.hour,
			&start_tt.minute);
		if (!e_date_edit_time_is_valid (E_DATE_EDIT (priv->end_time))) {
			comp_editor_page_display_validation_error (page, _("End time is wrong"), priv->end_time);
			return FALSE;
		}
		e_date_edit_get_time_of_day (
			E_DATE_EDIT (priv->end_time),
			&end_tt.hour,
			&end_tt.minute);
		start_zone = e_timezone_entry_get_timezone (E_TIMEZONE_ENTRY (priv->start_timezone));
		start_date.tzid = icaltimezone_get_tzid (start_zone);
		end_date.tzid = icaltimezone_get_tzid (start_zone);
	}

	e_cal_component_set_dtstart (comp, &start_date);
	e_cal_component_set_dtend (comp, &end_date);

	/* Categories */

	cat = gtk_editable_get_chars (GTK_EDITABLE (priv->categories), 0, -1);
	str = comp_editor_strip_categories (cat);
	g_free (cat);

	e_cal_component_set_categories (comp, str);

	g_free (str);

	/* Classification */
	classification = comp_editor_get_classification (editor);
	e_cal_component_set_classification (comp, classification);

	/* Show Time As (Transparency) */
	busy = priv->show_time_as_busy;
	e_cal_component_set_transparency (comp, busy ? E_CAL_COMPONENT_TRANSP_OPAQUE : E_CAL_COMPONENT_TRANSP_TRANSPARENT);

	/* send options */
	if (priv->sendoptions_shown && priv->sod) {
		icaltimezone *zone = comp_editor_get_timezone (editor);
		e_send_options_utils_fill_component (priv->sod, comp, zone);
	}

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
				switch (e_meeting_store_get_default_reminder_units (priv->meeting_store)) {
				case E_DURATION_DAYS:
					trigger.u.rel_duration.days = priv->alarm_interval;
					break;

				case E_DURATION_HOURS:
					trigger.u.rel_duration.hours = priv->alarm_interval;
					break;

				case E_DURATION_MINUTES:
					trigger.u.rel_duration.minutes = priv->alarm_interval;
					break;
				}
				break;

			default:
				break;
			}
			e_cal_component_alarm_set_trigger (ca, trigger);

			e_cal_component_add_alarm (comp, ca);
			e_cal_component_alarm_free (ca);
		}
	}

	if (priv->is_meeting) {
		ECalComponentOrganizer organizer = {NULL, NULL, NULL, NULL};

		if (!priv->existing) {
			gchar *backend_addr = NULL;
			gchar *backend_mailto = NULL;
			gchar *name;
			gchar *mailto;

			e_client_get_backend_property_sync (E_CLIENT (client), CAL_BACKEND_PROPERTY_CAL_EMAIL_ADDRESS, &backend_addr, NULL, NULL);

			/* Find the identity for the organizer or sentby field */
			if (!get_current_identity (epage, &name, &mailto)) {
				e_notice (
					priv->main, GTK_MESSAGE_ERROR,
					_("An organizer is required."));
				return FALSE;
			}

			/* Prefer the backend address if we have one. */
			if (backend_addr != NULL && *backend_addr != '\0') {
				backend_mailto = g_strdup_printf (
					"MAILTO:%s", backend_addr);
				if (g_ascii_strcasecmp (backend_mailto, mailto) == 0) {
					g_free (backend_mailto);
					backend_mailto = NULL;
				}
			}

			if (backend_mailto == NULL) {
				organizer.cn = name;
				organizer.value = mailto;
				name = mailto = NULL;
			} else {
				organizer.value = backend_mailto;
				organizer.sentby = mailto;
				backend_mailto = mailto = NULL;
			}

			e_cal_component_set_organizer (comp, &organizer);

			g_free (backend_addr);
			g_free (backend_mailto);
			g_free (name);
			g_free (mailto);
		}

		if (e_meeting_store_count_actual_attendees (priv->meeting_store) < 1) {
			e_notice (
				priv->main, GTK_MESSAGE_ERROR,
				_("At least one attendee is required."));
			return FALSE;
		}

		if (flags & COMP_EDITOR_DELEGATE) {
			GSList *attendee_list, *l;
			gint i;
			const GPtrArray *attendees = e_meeting_store_get_attendees (priv->meeting_store);

			e_cal_component_get_attendee_list (priv->comp, &attendee_list);

			for (i = 0; i < attendees->len; i++) {
				EMeetingAttendee *ia = g_ptr_array_index (attendees, i);
				ECalComponentAttendee *ca;

				/* Remove the duplicate user from the component if present */
				if (e_meeting_attendee_is_set_delfrom (ia) || e_meeting_attendee_is_set_delto (ia)) {
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
			set_attendees (comp, e_meeting_store_get_attendees (priv->meeting_store));
	}

	return TRUE;
}

static gboolean
event_page_fill_timezones (CompEditorPage *page,
                           GHashTable *timezones)
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
event_page_set_dates (CompEditorPage *page,
                      CompEditorPageDates *dates)
{
	update_time (EVENT_PAGE (page), dates->start, dates->end);
}

static void
event_page_add_attendee (CompEditorPage *page,
                         EMeetingAttendee *attendee)
{
	CompEditor *editor;
	EventPagePrivate *priv;

	priv = EVENT_PAGE_GET_PRIVATE (page);
	editor = comp_editor_page_get_editor (page);

	if ((comp_editor_get_flags (editor) & COMP_EDITOR_DELEGATE) != 0) {
		gchar *delfrom;

		/* EMeetingAttendee takes ownership of the string. */
		delfrom = g_strdup_printf ("MAILTO:%s", priv->user_add);
		e_meeting_attendee_set_delfrom (attendee, delfrom);
	}

	e_meeting_store_add_attendee (priv->meeting_store, attendee);
	e_meeting_list_view_add_attendee_to_name_selector (
		E_MEETING_LIST_VIEW (priv->list_view), attendee);
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
	editor_page_class->add_attendee = event_page_add_attendee;
}

static void
event_page_init (EventPage *epage)
{
	epage->priv = EVENT_PAGE_GET_PRIVATE (epage);
	epage->priv->deleted_attendees = g_ptr_array_new ();
	epage->priv->alarm_interval = -1;
	epage->priv->alarm_map = alarm_map_with_user_time;
	epage->priv->location_completion = gtk_entry_completion_new ();
}

void
event_page_set_view_role (EventPage *epage,
                          gboolean state)
{
	e_meeting_list_view_column_set_visible (
		epage->priv->list_view, E_MEETING_STORE_ROLE_COL, state);
}

void
event_page_set_view_status (EventPage *epage,
                            gboolean state)
{
	e_meeting_list_view_column_set_visible (
		epage->priv->list_view, E_MEETING_STORE_STATUS_COL, state);
}

void
event_page_set_view_type (EventPage *epage,
                          gboolean state)
{
	e_meeting_list_view_column_set_visible (
		epage->priv->list_view, E_MEETING_STORE_TYPE_COL, state);
}

void
event_page_set_view_rsvp (EventPage *epage,
                          gboolean state)
{
	e_meeting_list_view_column_set_visible (
		epage->priv->list_view, E_MEETING_STORE_RSVP_COL, state);
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
event_page_set_meeting (EventPage *page,
                        gboolean set)
{
	g_return_if_fail (IS_EVENT_PAGE (page));

	page->priv->is_meeting = set;
	if (page->priv->comp)
		sensitize_widgets (page);
}

void
event_page_set_delegate (EventPage *page,
                         gboolean set)
{
	g_return_if_fail (IS_EVENT_PAGE (page));

	if (set)
		gtk_button_set_label (GTK_BUTTON (page->priv->invite), _("_Delegatees"));
	else
		gtk_button_set_label (GTK_BUTTON (page->priv->invite), _("Atte_ndees"));
}

static void
time_sel_changed (GtkComboBox *combo,
                  EventPage *epage)
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

		update_end_time_combo (epage);
	}
}

static
void update_end_time_combo (EventPage *epage)
{
	EventPagePrivate *priv;
	struct icaltimetype start_tt = icaltime_null_time ();
	struct icaltimetype end_tt = icaltime_null_time ();
	time_t start_timet,end_timet;
	gint hours,minutes;

	priv = epage->priv;

	e_date_edit_get_date (
		E_DATE_EDIT (priv->start_time),
		&start_tt.year,
		&start_tt.month,
		&start_tt.day);
	e_date_edit_get_time_of_day (
		E_DATE_EDIT (priv->start_time),
		&start_tt.hour,
		&start_tt.minute);
	e_date_edit_get_date (
		E_DATE_EDIT (priv->end_time),
		&end_tt.year,
		&end_tt.month,
		&end_tt.day);
	e_date_edit_get_time_of_day (
		E_DATE_EDIT (priv->end_time),
		&end_tt.hour,
		&end_tt.minute);

	end_timet = icaltime_as_timet (end_tt);
	start_timet = icaltime_as_timet (start_tt);

	end_timet -= start_timet;
	hours = end_timet / (60 * 60);
	minutes = (end_timet / 60) - (hours * 60);

	gtk_spin_button_set_value (GTK_SPIN_BUTTON (priv->hour_selector), hours);
	gtk_spin_button_set_value (GTK_SPIN_BUTTON (priv->minute_selector), minutes);
}

static void
hour_sel_changed (GtkSpinButton *widget,
                  EventPage *epage)
{
	hour_minute_changed (epage);
}

static void
minute_sel_changed (GtkSpinButton *widget,
                    EventPage *epage)
{
	hour_minute_changed (epage);
}

static gboolean
minute_sel_focus_out (GtkSpinButton *widget,
                      GdkEvent *event,
                      EventPage *epage)
{
	const gchar *text;
	gint hours, minutes;

	g_return_val_if_fail (widget != NULL, FALSE);
	g_return_val_if_fail (epage != NULL, FALSE);

	text = gtk_entry_get_text (GTK_ENTRY (widget));
	minutes = g_strtod (text, NULL);

	if (minutes >= 60) {
		hours = minutes / 60;
		minutes = minutes % 60;

		gtk_spin_button_set_value (GTK_SPIN_BUTTON (epage->priv->hour_selector), hours);
		gtk_spin_button_set_value (GTK_SPIN_BUTTON (epage->priv->minute_selector), minutes);
	}

	return FALSE;
}

static void
hour_minute_changed (EventPage *epage)
{
	EventPagePrivate *priv;
	gint for_hours, for_minutes;
	struct icaltimetype end_tt = icaltime_null_time ();

	priv = epage->priv;

	e_date_edit_get_date (
		E_DATE_EDIT (priv->start_time),
		&end_tt.year,
		&end_tt.month,
		&end_tt.day);
	e_date_edit_get_time_of_day (
		E_DATE_EDIT (priv->start_time),
		&end_tt.hour,
		&end_tt.minute);

	for_hours = gtk_spin_button_get_value (GTK_SPIN_BUTTON (priv->hour_selector));
	for_minutes = gtk_spin_button_get_value (GTK_SPIN_BUTTON (priv->minute_selector));

	icaltime_adjust (&end_tt, 0, for_hours, for_minutes, 0);

	e_date_edit_set_date_and_time_of_day (
		E_DATE_EDIT (priv->end_time),
		end_tt.year,
		end_tt.month,
		end_tt.day,
		end_tt.hour,
		end_tt.minute);
}

static void
edit_clicked_cb (GtkButton *btn,
                 EventPage *epage)
{
	EventPagePrivate *priv;
	GtkTreePath *path = NULL;
	GtkTreeViewColumn *focus_col;

	priv = epage->priv;

	gtk_tree_view_get_cursor (
		GTK_TREE_VIEW (priv->list_view), &path, NULL);
	g_return_if_fail (path != NULL);

	gtk_tree_view_get_cursor (
		GTK_TREE_VIEW (priv->list_view), &path, &focus_col);
	gtk_tree_view_set_cursor (
		GTK_TREE_VIEW (priv->list_view), path, focus_col, TRUE);
	gtk_tree_path_free (path);
}

static void
add_clicked_cb (GtkButton *btn,
                EventPage *epage)
{
	CompEditor *editor;
	CompEditorFlags flags;
	EMeetingAttendee *attendee;

	editor = comp_editor_page_get_editor (COMP_EDITOR_PAGE (epage));
	flags = comp_editor_get_flags (editor);

	attendee = e_meeting_store_add_attendee_with_defaults (epage->priv->meeting_store);

	if (flags & COMP_EDITOR_DELEGATE) {
		e_meeting_attendee_set_delfrom (attendee, g_strdup_printf ("MAILTO:%s", epage->priv->user_add));
	}

	e_meeting_list_view_edit (epage->priv->list_view, attendee);
}

static gboolean
existing_attendee (EMeetingAttendee *ia,
                   ECalComponent *comp)
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
remove_attendee (EventPage *epage,
                 EMeetingAttendee *ia)
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
	 * assume they no longer want the organizer showing up */
	if (ia == priv->ia) {
		g_object_unref (priv->ia);
		priv->ia = NULL;
	}

	/* If this was a delegatee, no longer delegate */
	if (e_meeting_attendee_is_set_delfrom (ia)) {
		EMeetingAttendee *ib;

		ib = e_meeting_store_find_attendee (priv->meeting_store, e_meeting_attendee_get_delfrom (ia), &pos);
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
			ib = e_meeting_store_find_attendee (priv->meeting_store, e_meeting_attendee_get_delto (ia), NULL);

		comp_editor_manage_new_attendees (priv->comp, ia, FALSE);
		e_meeting_list_view_remove_attendee_from_name_selector (priv->list_view, ia);
		e_meeting_store_remove_attendee (priv->meeting_store, ia);

		ia = ib;
	}

	sensitize_widgets (epage);
}

static void
remove_clicked_cb (GtkButton *btn,
                   EventPage *epage)
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
	model = GTK_TREE_MODEL (priv->meeting_store);
	if (!(paths = gtk_tree_selection_get_selected_rows (selection, &model))) {
		g_warning ("Could not get a selection to delete.");
		return;
	}
	paths = g_list_reverse (paths);

	for (tmp = paths; tmp; tmp = tmp->next) {
		path = tmp->data;

		gtk_tree_model_get_iter (GTK_TREE_MODEL (priv->meeting_store), &iter, path);

		gtk_tree_model_get (GTK_TREE_MODEL (priv->meeting_store), &iter, E_MEETING_STORE_ADDRESS_COL, &address, -1);
		ia = e_meeting_store_find_attendee (priv->meeting_store, address, NULL);
		g_free (address);
		if (!ia) {
			g_warning ("Cannot delete attendee\n");
			continue;
		} else if (e_meeting_attendee_get_edit_level (ia) != E_MEETING_ATTENDEE_EDIT_FULL) {
			g_warning ("Not enough rights to delete attendee: %s\n", e_meeting_attendee_get_address (ia));
			continue;
		}

		remove_attendee (epage, ia);
	}

	/* Select closest item after removal */
	valid_iter = gtk_tree_model_get_iter (GTK_TREE_MODEL (priv->meeting_store), &iter, path);
	if (!valid_iter) {
		gtk_tree_path_prev (path);
		valid_iter = gtk_tree_model_get_iter (GTK_TREE_MODEL (priv->meeting_store), &iter, path);
	}

	if (valid_iter) {
		gtk_tree_selection_unselect_all (selection);
		gtk_tree_selection_select_iter (selection, &iter);
	}

	g_list_foreach (paths, (GFunc) gtk_tree_path_free, NULL);
	g_list_free (paths);
}

static void
invite_cb (GtkWidget *widget,
           EventPage *page)
{
	e_meeting_list_view_invite_others_dialog (page->priv->list_view);
}

static void
attendee_added_cb (EMeetingListView *emlv,
                   EMeetingAttendee *ia,
                   EventPage *epage)
{
	EventPagePrivate *priv = epage->priv;
	CompEditor *editor;
	CompEditorFlags flags;
	ECalClient *client;

	editor = comp_editor_page_get_editor (COMP_EDITOR_PAGE (epage));
	client = comp_editor_get_client (editor);
	flags = comp_editor_get_flags (editor);

	if (!(flags & COMP_EDITOR_DELEGATE)) {
		comp_editor_manage_new_attendees (priv->comp, ia, TRUE);
		return;
	}

	/* do not remove here, it did EMeetingListView already */
	e_meeting_attendee_set_delfrom (ia, g_strdup_printf ("MAILTO:%s", priv->user_add ? priv->user_add : ""));

	if (!e_client_check_capability (E_CLIENT (client), CAL_STATIC_CAPABILITY_DELEGATE_TO_MANY)) {
		EMeetingAttendee *delegator;

		gtk_widget_set_sensitive (priv->invite, FALSE);
		gtk_widget_set_sensitive (priv->add, FALSE);
		gtk_widget_set_sensitive (priv->edit, FALSE);

		delegator = e_meeting_store_find_attendee (priv->meeting_store, priv->user_add, NULL);
		g_return_if_fail (delegator != NULL);

		e_meeting_attendee_set_delto (delegator, g_strdup (e_meeting_attendee_get_address (ia)));
	}
}

static gboolean
list_view_event (EMeetingListView *list_view,
                 GdkEvent *event,
                 EventPage *epage)
{
	EventPagePrivate *priv = epage->priv;
	CompEditor *editor;
	CompEditorFlags flags;

	editor = comp_editor_page_get_editor (COMP_EDITOR_PAGE (epage));
	flags = comp_editor_get_flags (editor);

	if (event->type == GDK_2BUTTON_PRESS && flags & COMP_EDITOR_USER_ORG) {
		EMeetingAttendee *attendee;

		attendee = e_meeting_store_add_attendee_with_defaults (priv->meeting_store);

		if (flags & COMP_EDITOR_DELEGATE) {
			e_meeting_attendee_set_delfrom (attendee, g_strdup_printf ("MAILTO:%s", epage->priv->user_add));
		}

		e_meeting_list_view_edit (epage->priv->list_view, attendee);
		return TRUE;
	}

	return FALSE;
}

static gboolean
list_key_press (EMeetingListView *list_view,
                GdkEventKey *event,
                EventPage *epage)
{
	if (event->keyval == GDK_KEY_Delete) {

		remove_clicked_cb (NULL, epage);

		return TRUE;
	} else if (event->keyval == GDK_KEY_Insert) {
		add_clicked_cb (NULL, epage);

		return TRUE;
	}

	return FALSE;
}

void
event_page_set_all_day_event (EventPage *epage,
                              gboolean all_day)
{
	EventPagePrivate *priv = epage->priv;
	struct icaltimetype start_tt = icaltime_null_time ();
	struct icaltimetype end_tt = icaltime_null_time ();
	CompEditor *editor;
	GtkAction *action;
	gboolean date_set;
	gboolean active;

	editor = comp_editor_page_get_editor (COMP_EDITOR_PAGE (epage));

	epage->priv->all_day_event = all_day;
	e_date_edit_set_show_time (E_DATE_EDIT (priv->start_time), !all_day);
	e_date_edit_set_show_time (E_DATE_EDIT (priv->end_time), !all_day);

	date_set = e_date_edit_get_date (
		E_DATE_EDIT (priv->start_time),
		&start_tt.year,
		&start_tt.month,
		&start_tt.day);
	e_date_edit_get_time_of_day (
		E_DATE_EDIT (priv->start_time),
		&start_tt.hour,
		&start_tt.minute);
	g_return_if_fail (date_set);

	date_set = e_date_edit_get_date (
		E_DATE_EDIT (priv->end_time),
		&end_tt.year,
		&end_tt.month,
		&end_tt.day);
	e_date_edit_get_time_of_day (
		E_DATE_EDIT (priv->end_time),
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
		start_tt.minute = 0;
		start_tt.second = 0;
		start_tt.is_date = TRUE;

		/* Round down to the start of the day, or the start of the
		 * previous day if it is midnight. */
		icaltime_adjust (&end_tt, 0, 0, 0, -1);
		end_tt.hour = 0;
		end_tt.minute = 0;
		end_tt.second = 0;
		end_tt.is_date = TRUE;
	} else {
		icaltimezone *start_zone;

		if (end_tt.year == start_tt.year
		    && end_tt.month == start_tt.month
		    && end_tt.day == start_tt.day) {
			/* The event is within one day, so we set the event
			 * start to the start of the working day, and the end
			 * to one hour later. */
			start_tt.hour =
				comp_editor_get_work_day_start_hour (editor);
			start_tt.minute =
				comp_editor_get_work_day_start_minute (editor);
			start_tt.second = 0;

			end_tt = start_tt;
			icaltime_adjust (&end_tt, 0, 1, 0, 0);
		} else {
			/* The event is longer than 1 day, so we keep exactly
			 * the same times, just using DATE-TIME rather than
			 * DATE. */
			icaltime_adjust (&end_tt, 1, 0, 0, 0);
		}

		/* Make sure that end > start using the timezones. */
		start_zone = e_timezone_entry_get_timezone (E_TIMEZONE_ENTRY (priv->start_timezone));
		check_start_before_end (&start_tt, start_zone,
					&end_tt, start_zone,
					TRUE);
	}

	action = comp_editor_get_action (editor, "view-time-zone");
	active = gtk_toggle_action_get_active (GTK_TOGGLE_ACTION (action));
	event_page_set_show_timezone (epage, active & !all_day);

	g_signal_handlers_block_matched (priv->start_time, G_SIGNAL_MATCH_DATA, 0, 0, NULL, NULL, epage);
	g_signal_handlers_block_matched (priv->end_time, G_SIGNAL_MATCH_DATA, 0, 0, NULL, NULL, epage);

	e_date_edit_set_date (
		E_DATE_EDIT (priv->start_time), start_tt.year,
		start_tt.month, start_tt.day);
	e_date_edit_set_time_of_day (
		E_DATE_EDIT (priv->start_time),
		start_tt.hour, start_tt.minute);

	e_date_edit_set_date (
		E_DATE_EDIT (priv->end_time), end_tt.year,
		end_tt.month, end_tt.day);
	e_date_edit_set_time_of_day (
		E_DATE_EDIT (priv->end_time),
		end_tt.hour, end_tt.minute);

	g_signal_handlers_unblock_matched (priv->start_time, G_SIGNAL_MATCH_DATA, 0, 0, NULL, NULL, epage);
	g_signal_handlers_unblock_matched (priv->end_time, G_SIGNAL_MATCH_DATA, 0, 0, NULL, NULL, epage);

	/* Notify upstream */
	notify_dates_changed (epage, &start_tt, &end_tt);

	comp_editor_page_changed (COMP_EDITOR_PAGE (epage));
}

void
event_page_set_show_time_busy (EventPage *epage,
                               gboolean state)
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
event_page_set_show_timezone (EventPage *epage,
                              gboolean state)
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
event_page_set_show_categories (EventPage *epage,
                                gboolean state)
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
event_page_set_info_string (EventPage *epage,
                            const gchar *icon,
                            const gchar *msg)
{
	EventPagePrivate *priv;

	priv = epage->priv;

	gtk_image_set_from_icon_name (GTK_IMAGE (priv->info_icon), icon, GTK_ICON_SIZE_BUTTON);
	gtk_label_set_markup (GTK_LABEL (priv->info_string), msg);

	if (msg && icon)
		gtk_widget_show (priv->info_hbox);
	else
		gtk_widget_hide (priv->info_hbox);
}

/* Gets the widgets from the XML file and returns if they are all available. */
static gboolean
get_widgets (EventPage *epage)
{
	EShell *shell;
	EClientCache *client_cache;
	CompEditor *editor;
	CompEditorPage *page = COMP_EDITOR_PAGE (epage);
	GtkEntryCompletion *completion;
	EventPagePrivate *priv;
	GSList *accel_groups;
	GtkAction *action;
	GtkWidget *parent;
	GtkWidget *toplevel;
	GtkWidget *sw;

	priv = epage->priv;

#define GW(name) e_builder_get_widget (priv->builder, name)

	editor = comp_editor_page_get_editor (page);
	shell = comp_editor_get_shell (editor);
	client_cache = e_shell_get_client_cache (shell);

	priv->main = GW ("event-page");
	if (!priv->main)
		return FALSE;

	/* Get the GtkAccelGroup from the toplevel window, so we can install
	 * it when the notebook page is mapped. */
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

	action = comp_editor_get_action (editor, "view-time-zone");
	if (!gtk_toggle_action_get_active (GTK_TOGGLE_ACTION (action))) {
		gtk_widget_hide (priv->timezone_label);
		gtk_widget_hide (priv->start_timezone);
	} else {
		gtk_widget_show (priv->timezone_label);
		gtk_widget_show_all (priv->start_timezone);
	}

	g_object_ref (priv->main);
	parent = gtk_widget_get_parent (priv->main);
	gtk_container_remove (GTK_CONTAINER (parent), priv->main);

	priv->categories = GW ("categories");
	priv->categories_btn = GW ("categories-button");

	priv->organizer = GW ("organizer");
	gtk_list_store_clear (GTK_LIST_STORE (gtk_combo_box_get_model (GTK_COMBO_BOX (priv->organizer))));
	gtk_combo_box_set_entry_text_column (GTK_COMBO_BOX (priv->organizer), 0);

	priv->summary = GW ("summary");
	priv->summary_label = GW ("summary-label");
	priv->location = GW ("location");
	priv->location_label = GW ("location-label");

	priv->info_hbox = GW ("generic-info");
	priv->info_icon = GW ("generic-info-image");
	priv->info_string = GW ("generic-info-msgs");

	priv->invite = GW ("invite");
	priv->invite_label = GW ("invite-label");
	if (e_shell_get_express_mode (shell))
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

	priv->list_view = e_meeting_list_view_new (priv->meeting_store);

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

	priv->client_combo_box = GW ("client-combo-box");
	e_client_combo_box_set_client_cache (
		E_CLIENT_COMBO_BOX (priv->client_combo_box), client_cache);

	completion = e_category_completion_new ();
	gtk_entry_set_completion (GTK_ENTRY (priv->categories), completion);
	g_object_unref (completion);

	return (priv->summary
		&& priv->location
		&& priv->start_time
		&& priv->end_time
		&& priv->description);
}

static void
summary_changed_cb (GtkEntry *entry,
                    CompEditorPage *page)
{
	CompEditor *editor;
	const gchar *text;

	if (comp_editor_page_get_updating (page))
		return;

	editor = comp_editor_page_get_editor (page);
	text = gtk_entry_get_text (entry);
	comp_editor_set_summary (editor, text);
}

/* Note that this assumes that the start_tt and end_tt passed to it are the
 * dates visible to the user. For DATE values, we have to add 1 day to the
 * end_tt before emitting the signal. */
static void
notify_dates_changed (EventPage *epage,
                      struct icaltimetype *start_tt,
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
		 * DATE values. */
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

	check_starts_in_the_past (epage);
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
	 * we need to modify one of the times. */
	cmp = icaltime_compare (*start_tt, end_tt_copy);
	if (cmp > 0) {
		if (adjust_end_time) {
			/* Modify the end time, to be the start + 1 hour. */
			*end_tt = *start_tt;
			icaltime_adjust (end_tt, 0, 1, 0, 0);
			icaltimezone_convert_time (
				end_tt, start_zone,
				end_zone);
		} else {
			/* Modify the start time, to be the end - 1 hour. */
			*start_tt = *end_tt;
			icaltime_adjust (start_tt, 0, -1, 0, 0);
			icaltimezone_convert_time (
				start_tt, end_zone,
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
times_updated (EventPage *epage,
               gboolean adjust_end_time)
{
	EventPagePrivate *priv;
	struct icaltimetype start_tt = icaltime_null_time ();
	struct icaltimetype end_tt = icaltime_null_time ();
	gboolean date_set, all_day_event;
	gboolean set_start_date = FALSE, set_end_date = FALSE;
	icaltimezone *start_zone;

	priv = epage->priv;

	if (comp_editor_page_get_updating (COMP_EDITOR_PAGE (epage)))
		return;

	/* Fetch the start and end times and timezones from the widgets. */
	all_day_event = priv->all_day_event;

	date_set = e_date_edit_get_date (
		E_DATE_EDIT (priv->start_time),
		&start_tt.year,
		&start_tt.month,
		&start_tt.day);
	g_return_if_fail (date_set);

	date_set = e_date_edit_get_date (
		E_DATE_EDIT (priv->end_time),
		&end_tt.year,
		&end_tt.month,
		&end_tt.day);
	g_return_if_fail (date_set);

	if (all_day_event) {
		/* All Day Events are simple. We just compare the dates and if
		 * start > end we copy one of them to the other. */
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
		 * timezone before comparing. */
		e_date_edit_get_time_of_day (
			E_DATE_EDIT (priv->start_time),
			&start_tt.hour,
			&start_tt.minute);
		e_date_edit_get_time_of_day (
			E_DATE_EDIT (priv->end_time),
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
		e_date_edit_set_date (
			E_DATE_EDIT (priv->start_time),
			start_tt.year, start_tt.month,
			start_tt.day);
		e_date_edit_set_time_of_day (
			E_DATE_EDIT (priv->start_time),
			start_tt.hour, start_tt.minute);
		g_signal_handlers_unblock_matched (priv->start_time, G_SIGNAL_MATCH_DATA, 0, 0, NULL, NULL, epage);
	}

	if (set_end_date) {
		g_signal_handlers_block_matched (priv->end_time, G_SIGNAL_MATCH_DATA, 0, 0, NULL, NULL, epage);
		e_date_edit_set_date (
			E_DATE_EDIT (priv->end_time),
			end_tt.year, end_tt.month, end_tt.day);
		e_date_edit_set_time_of_day (
			E_DATE_EDIT (priv->end_time),
			end_tt.hour, end_tt.minute);
		g_signal_handlers_unblock_matched (priv->end_time, G_SIGNAL_MATCH_DATA, 0, 0, NULL, NULL, epage);
	}

	/* Notify upstream */
	notify_dates_changed (epage, &start_tt, &end_tt);
}

static gboolean
safe_to_process_date_changed_signal (GtkWidget *dedit_widget)
{
	EDateEdit *dedit;
	GtkWidget *entry;

	g_return_val_if_fail (dedit_widget != NULL, FALSE);

	dedit = E_DATE_EDIT (dedit_widget);
	g_return_val_if_fail (dedit != NULL, FALSE);

	entry = e_date_edit_get_entry (dedit);

	return !entry || !gtk_widget_has_focus (entry);
}

/* Callback used when the start date widget change.  We check that the
 * start date < end date and we set the "all day event" button as appropriate.
 */
static void
start_date_changed_cb (GtkWidget *dedit,
                       EventPage *epage)
{
	if (!safe_to_process_date_changed_signal (dedit))
		return;

	hour_minute_changed (epage);
	times_updated (epage, TRUE);
}

/* Callback used when the end date widget change.  We check that the
 * start date < end date and we set the "all day event" button as appropriate.
 */
static void
end_date_changed_cb (GtkWidget *dedit,
                     EventPage *epage)
{
	if (!safe_to_process_date_changed_signal (dedit)) {
		return;
	}

	times_updated (epage, FALSE);
}

/* Callback used when the start timezone is changed. If sync_timezones is set,
 * we set the end timezone to the same value. It also updates the start time
 * labels on the other notebook pages.
 */
static void
start_timezone_changed_cb (GtkWidget *widget,
                           EventPage *epage)
{
	EventPagePrivate *priv = epage->priv;

	if (priv->sync_timezones) {
		comp_editor_page_set_updating (COMP_EDITOR_PAGE (epage), TRUE);
		/*the earlier method caused an infinite recursion*/
		priv->end_timezone = priv->start_timezone;
		gtk_widget_show_all (priv->end_timezone);
		comp_editor_page_set_updating (COMP_EDITOR_PAGE (epage), FALSE);
	}

	times_updated (epage, TRUE);
}

/* Callback used when the categories button is clicked; we must bring up the
 * category list dialog.
 */
static void
categories_clicked_cb (GtkWidget *button,
                       EventPage *epage)
{
	GtkEntry *entry;

	entry = GTK_ENTRY (epage->priv->categories);
	e_categories_config_open_dialog_for_entry (entry);
}

void
event_page_send_options_clicked_cb (EventPage *epage)
{
	EventPagePrivate *priv;
	CompEditor *editor;
	GtkWidget *toplevel;
	ESource *source;
	ECalClient *client;

	priv = epage->priv;
	editor = comp_editor_page_get_editor (COMP_EDITOR_PAGE (epage));
	client = comp_editor_get_client (editor);

	if (!priv->sod) {
		priv->sod = e_send_options_dialog_new ();
		source = e_source_combo_box_ref_active (
			E_SOURCE_COMBO_BOX (priv->client_combo_box));
		e_send_options_utils_set_default_data (
			priv->sod, source, "calendar");
		priv->sod->data->initialized = TRUE;
		g_object_unref (source);
	}

	if (e_client_check_capability (E_CLIENT (client), CAL_STATIC_CAPABILITY_NO_GEN_OPTIONS)) {
		e_send_options_set_need_general_options (priv->sod, FALSE);
	}

	toplevel = gtk_widget_get_toplevel (priv->main);
	e_send_options_dialog_run (priv->sod, toplevel, E_ITEM_CALENDAR);
}

static void
epage_get_client_cb (GObject *source_object,
                     GAsyncResult *result,
                     gpointer user_data)
{
	EClient *client;
	EClientComboBox *combo_box;
	EventPage *epage = user_data;
	EventPagePrivate *priv;
	CompEditor *editor;
	GError *error = NULL;

	combo_box = E_CLIENT_COMBO_BOX (source_object);

	client = e_client_combo_box_get_client_finish (
		combo_box, result, &error);

	/* Sanity check. */
	g_return_if_fail (
		((client != NULL) && (error == NULL)) ||
		((client == NULL) && (error != NULL)));

	if (g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED)) {
		g_clear_error (&error);
		return;
	}

	editor = comp_editor_page_get_editor (COMP_EDITOR_PAGE (epage));
	priv = epage->priv;

	if (error != NULL) {
		GtkWidget *dialog;
		ECalClient *old_client;

		old_client = comp_editor_get_client (editor);

		e_source_combo_box_set_active (
			E_SOURCE_COMBO_BOX (combo_box),
			e_client_get_source (E_CLIENT (old_client)));

		dialog = gtk_message_dialog_new (
			NULL, GTK_DIALOG_MODAL,
			GTK_MESSAGE_WARNING, GTK_BUTTONS_OK,
			"%s", error->message);
		gtk_dialog_run (GTK_DIALOG (dialog));
		gtk_widget_destroy (dialog);

		g_clear_error (&error);
	} else {
		gchar *backend_addr = NULL;
		icaltimezone *zone;
		ECalClient *cal_client = E_CAL_CLIENT (client);

		g_return_if_fail (cal_client != NULL);

		zone = e_meeting_store_get_timezone (priv->meeting_store);
		e_cal_client_set_default_timezone (cal_client, zone);

		comp_editor_set_client (editor, cal_client);
		if (e_client_check_capability (client, CAL_STATIC_CAPABILITY_REQ_SEND_OPTIONS) && priv->is_meeting)
			event_page_show_options (epage);
		else
			event_page_hide_options (epage);

		e_client_get_backend_property_sync (client, CAL_BACKEND_PROPERTY_CAL_EMAIL_ADDRESS, &backend_addr, NULL, NULL);

		if (priv->is_meeting)
			event_page_select_organizer (epage, backend_addr);

		set_subscriber_info_string (epage, backend_addr);
		g_free (backend_addr);

		sensitize_widgets (epage);

		alarm_list_dialog_set_client (
			priv->alarm_list_dlg_widget, cal_client);
	}
}

static void
combo_box_changed_cb (ESourceComboBox *combo_box,
                      EventPage *epage)
{
	EventPagePrivate *priv = epage->priv;
	ESource *source;

	if (comp_editor_page_get_updating (COMP_EDITOR_PAGE (epage)))
		return;

	source = e_source_combo_box_ref_active (combo_box);
	/* This is valid when the 'combo_box' is rebuilding its content. */
	if (!source)
		return;

	if (priv->connect_cancellable != NULL) {
		g_cancellable_cancel (priv->connect_cancellable);
		g_object_unref (priv->connect_cancellable);
	}
	priv->connect_cancellable = g_cancellable_new ();

	e_client_combo_box_get_client (
		E_CLIENT_COMBO_BOX (combo_box),
		source, priv->connect_cancellable,
		epage_get_client_cb, epage);

	g_object_unref (source);
}

static void
set_subscriber_info_string (EventPage *epage,
                            const gchar *backend_address)
{
	if (!check_starts_in_the_past (epage))
		event_page_set_info_string (epage, NULL, NULL);
}

static void
alarm_changed_cb (GtkWidget *widget,
                  EventPage *epage)
{
	EventPagePrivate *priv = epage->priv;

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
			switch (e_meeting_store_get_default_reminder_units (priv->meeting_store)) {
			case E_DURATION_DAYS:
				trigger.u.rel_duration.days = priv->alarm_interval;
				break;

			case E_DURATION_HOURS:
				trigger.u.rel_duration.hours = priv->alarm_interval;
				break;

			case E_DURATION_MINUTES:
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
			priv->alarm_icon = create_alarm_image_button ("stock_bell", _("This event has reminders"), epage);
			gtk_box_pack_start ((GtkBox *) priv->status_icons, priv->alarm_icon, FALSE, FALSE, 6);
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
	ECalClient *client;

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

static gboolean
alarm_dialog_delete_event_cb (GtkWidget *alarm_dialog)
{
	gtk_widget_hide (alarm_dialog);

	/* stop processing other handlers */
	return TRUE;
}

/* Hooks the widget signals */
static gboolean
init_widgets (EventPage *epage)
{
	EventPagePrivate *priv = epage->priv;
	EShell *shell;
	CompEditor *editor;
	EClientCache *client_cache;
	GtkTextBuffer *text_buffer;
	icaltimezone *zone;
	gchar *combo_label = NULL;
	GtkAction *action;
	GtkTreeSelection *selection;
	gboolean active;
	ECalClient *client;
	GtkTreeIter iter;
	GtkListStore *store;

	editor = comp_editor_page_get_editor (COMP_EDITOR_PAGE (epage));

	shell = comp_editor_get_shell (editor);
	client = comp_editor_get_client (editor);

	client_cache = e_shell_get_client_cache (shell);

	/* Make sure the EDateEdit widgets use our timezones to get the
	 * current time. */
	e_date_edit_set_get_time_callback (
		E_DATE_EDIT (priv->start_time),
		(EDateEditGetTimeCallback) comp_editor_get_current_time,
		g_object_ref (editor),
		(GDestroyNotify) g_object_unref);
	e_date_edit_set_get_time_callback (
		E_DATE_EDIT (priv->end_time),
		(EDateEditGetTimeCallback) comp_editor_get_current_time,
		g_object_ref (editor),
		(GDestroyNotify) g_object_unref);

	/* Generic informative messages */
	gtk_widget_hide (priv->info_hbox);

	/* Summary */
	g_signal_connect (
		priv->summary, "changed",
		G_CALLBACK (summary_changed_cb), epage);

	/* Description */
	text_buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (priv->description));

	gtk_text_view_set_wrap_mode (GTK_TEXT_VIEW (priv->description), GTK_WRAP_WORD);

	e_buffer_tagger_connect (GTK_TEXT_VIEW (priv->description));

	/* Start and end times */
	g_signal_connect (
		priv->start_time, "changed",
		G_CALLBACK (start_date_changed_cb), epage);
	g_signal_connect (
		priv->end_time, "changed",
		G_CALLBACK (end_date_changed_cb), epage);

	/* Categories */
	g_signal_connect (
		priv->categories_btn, "clicked",
		G_CALLBACK (categories_clicked_cb), epage);

	/* Client selector */
	g_signal_connect (
		priv->client_combo_box, "changed",
		G_CALLBACK (combo_box_changed_cb), epage);

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
	g_signal_connect (
		priv->start_timezone, "changed",
		G_CALLBACK (start_timezone_changed_cb), epage);

	e_meeting_list_view_column_set_visible (
		priv->list_view, E_MEETING_STORE_ATTENDEE_COL, TRUE);

	action = comp_editor_get_action (editor, "view-role");
	active = gtk_toggle_action_get_active (GTK_TOGGLE_ACTION (action));
	e_meeting_list_view_column_set_visible (
		priv->list_view, E_MEETING_STORE_ROLE_COL, active);

	action = comp_editor_get_action (editor, "view-rsvp");
	active = gtk_toggle_action_get_active (GTK_TOGGLE_ACTION (action));
	e_meeting_list_view_column_set_visible (
		priv->list_view, E_MEETING_STORE_RSVP_COL, active);

	action = comp_editor_get_action (editor, "view-status");
	active = gtk_toggle_action_get_active (GTK_TOGGLE_ACTION (action));
	e_meeting_list_view_column_set_visible (
		priv->list_view, E_MEETING_STORE_STATUS_COL, active);

	action = comp_editor_get_action (editor, "view-type");
	active = gtk_toggle_action_get_active (GTK_TOGGLE_ACTION (action));
	e_meeting_list_view_column_set_visible (
		priv->list_view, E_MEETING_STORE_TYPE_COL, active);

	selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (priv->list_view));
	gtk_tree_selection_set_mode (selection, GTK_SELECTION_MULTIPLE);

	g_signal_connect (
		priv->list_view, "event",
		G_CALLBACK (list_view_event), epage);
	g_signal_connect (
		priv->list_view, "key_press_event",
		G_CALLBACK (list_key_press), epage);

	/* Add attendee button */
	g_signal_connect (
		priv->add, "clicked",
		G_CALLBACK (add_clicked_cb), epage);

	/* Remove attendee button */
	g_signal_connect (
		priv->remove, "clicked",
		G_CALLBACK (remove_clicked_cb), epage);

	/* Edit attendee button */
	g_signal_connect (
		priv->edit, "clicked",
		G_CALLBACK (edit_clicked_cb), epage);

	/* Contacts button */
	g_signal_connect (
		priv->invite, "clicked",
		G_CALLBACK (invite_cb), epage);

	/* Alarm dialog */
	g_signal_connect (
		priv->alarm_dialog, "response",
		G_CALLBACK (gtk_widget_hide), priv->alarm_dialog);
	g_signal_connect (
		priv->alarm_dialog, "delete-event",
		G_CALLBACK (alarm_dialog_delete_event_cb), priv->alarm_dialog);
	priv->alarm_list_dlg_widget = alarm_list_dialog_peek (
		client_cache, client, priv->alarm_list_store);
	gtk_widget_reparent (priv->alarm_list_dlg_widget, priv->alarm_box);
	gtk_widget_show_all (priv->alarm_list_dlg_widget);
	gtk_widget_hide (priv->alarm_dialog);
	gtk_window_set_modal (GTK_WINDOW (priv->alarm_dialog), TRUE);

	/* Meeting List View */
	g_signal_connect (
		priv->list_view, "attendee_added",
		G_CALLBACK (attendee_added_cb), epage);

	gtk_widget_show (GTK_WIDGET (priv->list_view));

	/* categories */
	action = comp_editor_get_action (editor, "view-categories");
	if (!gtk_toggle_action_get_active (GTK_TOGGLE_ACTION (action))) {
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
	g_signal_connect (
		priv->end_time_combo, "changed",
		G_CALLBACK (time_sel_changed), epage);
	update_end_time_combo (epage);

	/* Hour and Minute selector */
	gtk_spin_button_set_range ((GtkSpinButton *) priv->hour_selector, 0, G_MAXINT);
	g_signal_connect (
		priv->hour_selector, "value-changed",
		G_CALLBACK (hour_sel_changed), epage);
	g_signal_connect (
		priv->minute_selector, "value-changed",
		G_CALLBACK (minute_sel_changed), epage);

	g_signal_connect (
		priv->minute_selector, "focus-out-event",
		G_CALLBACK (minute_sel_focus_out), epage);

	/* Add the user defined time if necessary */
	priv->alarm_units =
		e_meeting_store_get_default_reminder_units (
		priv->meeting_store);
	priv->alarm_interval =
		e_meeting_store_get_default_reminder_interval (
		priv->meeting_store);

	combo_label = NULL;
	switch (priv->alarm_units) {
	case E_DURATION_DAYS:
		if (priv->alarm_interval != 1) {
			combo_label = g_strdup_printf (ngettext ("%d day before appointment", "%d days before appointment", priv->alarm_interval), priv->alarm_interval);
		}
		break;

	case E_DURATION_HOURS:
		if (priv->alarm_interval != 1) {
			combo_label = g_strdup_printf (ngettext ("%d hour before appointment", "%d hours before appointment", priv->alarm_interval), priv->alarm_interval);
		}
		break;

	case E_DURATION_MINUTES:
		if (priv->alarm_interval != 15) {
			combo_label = g_strdup_printf (ngettext ("%d minute before appointment", "%d minutes before appointment", priv->alarm_interval), priv->alarm_interval);
		}
		break;
	}

	store = GTK_LIST_STORE (gtk_combo_box_get_model (GTK_COMBO_BOX (priv->alarm_time_combo)));
	if (combo_label) {
		gtk_list_store_append (store, &iter);
		gtk_list_store_set (
			store, &iter,
			0, combo_label,
			-1);
		g_free (combo_label);
		priv->alarm_map = alarm_map_with_user_time;
	} else {
		priv->alarm_map = alarm_map_without_user_time;
	}

	gtk_list_store_append (store, &iter);
	gtk_list_store_set (
		store, &iter,
		0, _("Custom"),
		-1);

	gtk_list_store_insert (store, &iter, 0);
	gtk_list_store_set (
		store, &iter,
		/* Translators: "None" for "No reminder set" */
		0, C_("cal-reminders", "None"),
		-1);

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
		priv->client_combo_box, "changed",
		G_CALLBACK (comp_editor_page_changed), epage);
	g_signal_connect_swapped (
		priv->start_timezone, "changed",
		G_CALLBACK (comp_editor_page_changed), epage);

	/* Set the default timezone, so the timezone entry may be hidden. */
	zone = e_meeting_store_get_timezone (priv->meeting_store);
	e_timezone_entry_set_default_timezone (E_TIMEZONE_ENTRY (priv->start_timezone), zone);
	e_timezone_entry_set_default_timezone (E_TIMEZONE_ENTRY (priv->end_timezone), zone);

	action = comp_editor_get_action (editor, "view-time-zone");
	active = gtk_toggle_action_get_active (GTK_TOGGLE_ACTION (action));
	event_page_set_show_timezone (epage, active);

	return TRUE;
}

static void
event_page_select_organizer (EventPage *epage,
                             const gchar *backend_address)
{
	EventPagePrivate *priv = epage->priv;
	const gchar *default_address;
	gint ii;

	/* Treat an empty backend address as NULL. */
	if (backend_address != NULL && *backend_address == '\0')
		backend_address = NULL;

	default_address = priv->fallback_address;

	if (backend_address != NULL) {
		for (ii = 0; priv->address_strings[ii] != NULL; ii++) {
			if (g_strrstr (priv->address_strings[ii], backend_address) != NULL) {
				default_address = priv->address_strings[ii];
				break;
			}
		}
	}

	if (default_address != NULL) {
		if (!priv->comp || !e_cal_component_has_organizer (priv->comp)) {
			GtkEntry *entry = GTK_ENTRY (gtk_bin_get_child (GTK_BIN (priv->organizer)));

			g_signal_handlers_block_by_func (entry, organizer_changed_cb, epage);
			gtk_entry_set_text (entry, default_address);
			g_signal_handlers_unblock_by_func (entry, organizer_changed_cb, epage);
		}
	} else
		g_warning ("No potential organizers!");
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
event_page_construct (EventPage *epage,
                      EMeetingStore *meeting_store)
{
	EventPagePrivate *priv;
	EShell *shell;
	CompEditor *editor;
	ESourceRegistry *registry;
	EFocusTracker *focus_tracker;
	GtkComboBox *combo_box;
	GtkListStore *list_store;
	GtkTreeModel *model;
	GtkTreeIter iter;
	gint ii;

	editor = comp_editor_page_get_editor (COMP_EDITOR_PAGE (epage));
	shell = comp_editor_get_shell (editor);
	focus_tracker = comp_editor_get_focus_tracker (editor);

	priv = epage->priv;
	priv->meeting_store = g_object_ref (meeting_store);

	/* Make sure our custom widget classes are registered with
	 * GType before we load the GtkBuilder definition file. */
	g_type_ensure (E_TYPE_CLIENT_COMBO_BOX);
	g_type_ensure (E_TYPE_DATE_EDIT);
	g_type_ensure (E_TYPE_TIMEZONE_ENTRY);
	g_type_ensure (E_TYPE_SPELL_ENTRY);

	priv->builder = gtk_builder_new ();
	e_load_ui_builder_definition (priv->builder, "event-page.ui");

	if (!get_widgets (epage)) {
		g_message (
			"event_page_construct(): "
			"Could not find all widgets in the XML file!");
		return NULL;
	}

	e_spell_text_view_attach (GTK_TEXT_VIEW (priv->description));
	e_widget_undo_attach (priv->summary, focus_tracker);
	e_widget_undo_attach (priv->location, focus_tracker);
	e_widget_undo_attach (priv->categories, focus_tracker);
	e_widget_undo_attach (priv->description, focus_tracker);

	/* Create entry completion and attach it to the entry */
	priv->location_completion = gtk_entry_completion_new ();
	gtk_entry_set_completion (
		GTK_ENTRY (priv->location),
		priv->location_completion);

	/* Initialize completino model */
	list_store = gtk_list_store_new (1, G_TYPE_STRING);
	gtk_entry_completion_set_model (
		priv->location_completion,
		GTK_TREE_MODEL (list_store));
	gtk_entry_completion_set_text_column (priv->location_completion, 0);

	combo_box = GTK_COMBO_BOX (priv->organizer);
	model = gtk_combo_box_get_model (combo_box);
	list_store = GTK_LIST_STORE (model);

	registry = e_shell_get_registry (shell);
	priv->address_strings = itip_get_user_identities (registry);
	priv->fallback_address = itip_get_fallback_identity (registry);

	/* FIXME Could we just use a GtkComboBoxText? */
	for (ii = 0; priv->address_strings[ii] != NULL; ii++) {
		gtk_list_store_append (list_store, &iter);
		gtk_list_store_set (
			list_store, &iter,
			0, priv->address_strings[ii], -1);
	}

	gtk_combo_box_set_active (combo_box, 0);

	g_signal_connect (
		gtk_bin_get_child (GTK_BIN (priv->organizer)), "changed",
		G_CALLBACK (organizer_changed_cb), epage);

	if (!init_widgets (epage)) {
		g_message (
			"event_page_construct(): "
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
event_page_new (EMeetingStore *meeting_store,
                CompEditor *editor)
{
	EventPage *epage;

	epage = g_object_new (TYPE_EVENT_PAGE, "editor", editor, NULL);
	if (!event_page_construct (epage, meeting_store)) {
		g_object_unref (epage);
		g_return_val_if_reached (NULL);
	}

	return epage;
}

static void
set_attendees (ECalComponent *comp,
               const GPtrArray *attendees)
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
 * event_page_remove_all_attendees
 * @epage: an #EventPage
 *
 * Removes all attendees from the meeting store and name selector.
 **/
void
event_page_remove_all_attendees (EventPage *epage)
{
	EventPagePrivate *priv;

	g_return_if_fail (epage != NULL);
	g_return_if_fail (IS_EVENT_PAGE (epage));

	priv = epage->priv;

	e_meeting_store_remove_all_attendees (priv->meeting_store);
	e_meeting_list_view_remove_all_attendees_from_name_selector (E_MEETING_LIST_VIEW (priv->list_view));
}

GtkWidget *
event_page_get_alarm_page (EventPage *epage)
{
	EventPagePrivate *priv;
	GtkWidget *alarm_page, *tmp;

	g_return_val_if_fail (epage != NULL, NULL);
	g_return_val_if_fail (IS_EVENT_PAGE (epage), NULL);

	priv = epage->priv;

	tmp = GW ("dialog-vbox1");
	alarm_page = GW ("vbox2");
	g_object_ref (alarm_page);
	gtk_container_remove ((GtkContainer *) tmp, alarm_page);

	return alarm_page;
}

GtkWidget *
event_page_get_attendee_page (EventPage *epage)
{
	EventPagePrivate *priv;
	GtkWidget *apage;

	g_return_val_if_fail (epage != NULL, NULL);
	g_return_val_if_fail (IS_EVENT_PAGE (epage), NULL);

	priv = epage->priv;

	apage = priv->list_box;
	g_object_ref (apage);
	gtk_container_remove ((GtkContainer *) gtk_widget_get_parent (apage), apage);
	gtk_widget_hide (priv->attendee_box);

	return apage;
}

#undef GW
