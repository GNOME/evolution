/*
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 * Copyright (C) 2015 Red Hat, Inc. (www.redhat.com)
 *
 * This program is free software: you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by
 * the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License
 * for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 *
 * Authors:
 *	Federico Mena-Quintero <federico@ximian.com>
 *	Miguel de Icaza <miguel@ximian.com>
 *	Seth Alves <alves@hungry.com>
 *	JP Rosevear <jpr@ximian.com>
 *	Hans Petter Jansson <hpj@ximian.com>
 */

#include "evolution-config.h"

#include <glib.h>
#include <glib/gi18n-lib.h>
#include <gtk/gtk.h>

#include <e-util/e-util.h>

#include "calendar-config.h"
#include "e-alarm-list.h"
#include "itip-utils.h"

#include "e-comp-editor-page-reminders.h"

#define SECTION_NAME			_("Send To")
#define X_EVOLUTION_NEEDS_DESCRIPTION	"X-EVOLUTION-NEEDS-DESCRIPTION"

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
	E_CAL_STATIC_CAPABILITY_NO_DISPLAY_ALARMS,
	E_CAL_STATIC_CAPABILITY_NO_AUDIO_ALARMS,
	E_CAL_STATIC_CAPABILITY_NO_PROCEDURE_ALARMS,
	E_CAL_STATIC_CAPABILITY_NO_EMAIL_ALARMS
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

struct _ECompEditorPageRemindersPrivate {
	GtkWidget *alarms_combo;
	GtkWidget *alarms_scrolled_window;
	GtkWidget *alarms_tree_view;
	GtkWidget *alarms_button_box;
	GtkWidget *alarms_add_button;
	GtkWidget *alarms_remove_button;

	GtkWidget *alarm_setup_hbox;
	GtkWidget *kind_combo;
	GtkWidget *time_spin;
	GtkWidget *unit_combo;
	GtkWidget *relative_time_combo;
	GtkWidget *relative_to_combo;
	GtkWidget *repeat_setup_hbox;
	GtkWidget *repeat_check;
	GtkWidget *repeat_times_spin;
	GtkWidget *repeat_every_label;
	GtkWidget *repeat_every_spin;
	GtkWidget *repeat_unit_combo;
	GtkWidget *options_label;
	GtkWidget *options_notebook;
	GtkWidget *custom_message_check;
	GtkWidget *custom_message_text_view;
	GtkWidget *custom_sound_check;
	GtkWidget *custom_sound_chooser;
	GtkWidget *custom_app_path_entry;
	GtkWidget *custom_app_args_entry;
	GtkWidget *custom_email_button;
	GtkWidget *custom_email_entry;
	GtkWidget *custom_email_message_check;
	GtkWidget *custom_email_message_text_view;

	EAlarmList *alarm_list;
	EDurationType alarm_units;
	gint alarm_interval;
	/* either with-user-time or without it */
	const gint *alarm_map;

	/* Addressbook name selector, created on demand */
	ENameSelector *name_selector;
};

G_DEFINE_TYPE (ECompEditorPageReminders, e_comp_editor_page_reminders, E_TYPE_COMP_EDITOR_PAGE)

static void
ecep_reminders_sanitize_option_widgets (ECompEditorPageReminders *page_reminders)
{
	gboolean any_selected;
	gboolean is_custom;
	gboolean sensitive;

	g_return_if_fail (E_IS_COMP_EDITOR_PAGE_REMINDERS (page_reminders));

	any_selected = gtk_tree_selection_count_selected_rows (gtk_tree_view_get_selection (
		GTK_TREE_VIEW (page_reminders->priv->alarms_tree_view))) > 0;
	is_custom = e_dialog_combo_box_get (page_reminders->priv->alarms_combo,
		page_reminders->priv->alarm_map) == ALARM_CUSTOM;

	gtk_widget_set_sensitive (page_reminders->priv->alarms_tree_view, is_custom);
	gtk_widget_set_sensitive (page_reminders->priv->alarms_add_button, is_custom);
	gtk_widget_set_sensitive (page_reminders->priv->alarms_remove_button, any_selected && is_custom);

	gtk_widget_set_visible (page_reminders->priv->alarm_setup_hbox, any_selected && is_custom);
	gtk_widget_set_visible (page_reminders->priv->repeat_setup_hbox, any_selected && is_custom);
	gtk_widget_set_visible (page_reminders->priv->options_label, any_selected && is_custom);
	gtk_widget_set_visible (page_reminders->priv->options_notebook, any_selected && is_custom);

	sensitive = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (page_reminders->priv->repeat_check));
	gtk_widget_set_sensitive (page_reminders->priv->repeat_times_spin, sensitive);
	gtk_widget_set_sensitive (page_reminders->priv->repeat_every_label, sensitive);
	gtk_widget_set_sensitive (page_reminders->priv->repeat_every_spin, sensitive);
	gtk_widget_set_sensitive (page_reminders->priv->repeat_unit_combo, sensitive);

	sensitive = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (page_reminders->priv->custom_message_check));
	gtk_widget_set_sensitive (page_reminders->priv->custom_message_text_view, sensitive);

	sensitive = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (page_reminders->priv->custom_sound_check));
	gtk_widget_set_sensitive (page_reminders->priv->custom_sound_chooser, sensitive);

	sensitive = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (page_reminders->priv->custom_email_message_check));
	gtk_widget_set_sensitive (page_reminders->priv->custom_email_message_text_view, sensitive);
}

static void
ecep_reminders_set_text_view_text (GtkWidget *text_view,
				   const gchar *text)
{
	GtkTextBuffer *text_buffer;

	g_return_if_fail (GTK_IS_TEXT_VIEW (text_view));

	text_buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (text_view));
	gtk_text_buffer_set_text (text_buffer, text ? text : "", -1);
}

static gchar *
ecep_reminders_get_text_view_text (GtkWidget *text_view)
{
	GtkTextBuffer *text_buffer;
	GtkTextIter text_iter_start, text_iter_end;

	g_return_val_if_fail (GTK_IS_TEXT_VIEW (text_view), NULL);

	text_buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (text_view));
	gtk_text_buffer_get_start_iter (text_buffer, &text_iter_start);
	gtk_text_buffer_get_end_iter (text_buffer, &text_iter_end);

	return gtk_text_buffer_get_text (text_buffer, &text_iter_start, &text_iter_end, FALSE);
}

static gboolean
ecep_reminders_remove_needs_description_property (ECalComponentAlarm *alarm)
{
	ECalComponentPropertyBag *bag;
	guint ii, sz;

	g_return_val_if_fail (alarm != NULL, FALSE);

	bag = e_cal_component_alarm_get_property_bag (alarm);
	g_return_val_if_fail (bag != NULL, FALSE);

	sz = e_cal_component_property_bag_get_count (bag);
	for (ii = 0; ii < sz; ii++) {
		ICalProperty *prop;
		const gchar *x_name;

		prop = e_cal_component_property_bag_get (bag, ii);
		if (!prop || i_cal_property_isa (prop) != I_CAL_X_PROPERTY)
			continue;

		x_name = i_cal_property_get_x_name (prop);
		if (g_str_equal (x_name, X_EVOLUTION_NEEDS_DESCRIPTION)) {
			e_cal_component_property_bag_remove (bag, ii);
			return TRUE;
		}
	}

	return FALSE;
}

static gboolean
ecep_reminders_has_needs_description_property (ECalComponentAlarm *alarm)
{
	ECalComponentPropertyBag *bag;
	guint ii, sz;

	g_return_val_if_fail (alarm != NULL, FALSE);

	bag = e_cal_component_alarm_get_property_bag (alarm);
	g_return_val_if_fail (bag != NULL, FALSE);

	sz = e_cal_component_property_bag_get_count (bag);
	for (ii = 0; ii < sz; ii++) {
		ICalProperty *prop;
		const gchar *x_name;

		prop = e_cal_component_property_bag_get (bag, ii);
		if (!prop || i_cal_property_isa (prop) != I_CAL_X_PROPERTY)
			continue;

		x_name = i_cal_property_get_x_name (prop);
		if (g_str_equal (x_name, X_EVOLUTION_NEEDS_DESCRIPTION))
			return TRUE;
	}

	return FALSE;
}

static void
ecep_reminders_add_needs_description_property (ECalComponentAlarm *alarm)
{
	ECalComponentPropertyBag *bag;
	ICalProperty *prop;

	g_return_if_fail (alarm != NULL);

	if (ecep_reminders_has_needs_description_property (alarm))
		return;

	bag = e_cal_component_alarm_get_property_bag (alarm);
	g_return_if_fail (bag != NULL);

	prop = i_cal_property_new_x ("1");
	i_cal_property_set_x_name (prop, X_EVOLUTION_NEEDS_DESCRIPTION);
	e_cal_component_property_bag_take (bag, prop);
}

static void
ecep_reminders_reset_alarm_widget (ECompEditorPageReminders *page_reminders)
{
	ECompEditorPage *page;

	g_return_if_fail (E_IS_COMP_EDITOR_PAGE_REMINDERS (page_reminders));

	page = E_COMP_EDITOR_PAGE (page_reminders);

	e_comp_editor_page_set_updating (page, TRUE);

	gtk_combo_box_set_active (GTK_COMBO_BOX (page_reminders->priv->kind_combo), 0);
	gtk_spin_button_set_value (GTK_SPIN_BUTTON (page_reminders->priv->time_spin), 15);
	gtk_combo_box_set_active (GTK_COMBO_BOX (page_reminders->priv->unit_combo), 0);
	gtk_combo_box_set_active (GTK_COMBO_BOX (page_reminders->priv->relative_time_combo), 0);
	gtk_combo_box_set_active (GTK_COMBO_BOX (page_reminders->priv->relative_to_combo), 0);
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (page_reminders->priv->repeat_check), FALSE);
	gtk_spin_button_set_value (GTK_SPIN_BUTTON (page_reminders->priv->repeat_times_spin), 1);
	gtk_spin_button_set_value (GTK_SPIN_BUTTON (page_reminders->priv->repeat_every_spin), 5);
	gtk_combo_box_set_active (GTK_COMBO_BOX (page_reminders->priv->repeat_unit_combo), 0);
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (page_reminders->priv->custom_message_check), FALSE);
	ecep_reminders_set_text_view_text (page_reminders->priv->custom_message_text_view, NULL);
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (page_reminders->priv->custom_sound_check), FALSE);
	gtk_file_chooser_unselect_all (GTK_FILE_CHOOSER (page_reminders->priv->custom_sound_chooser));
	gtk_entry_set_text (GTK_ENTRY (page_reminders->priv->custom_app_path_entry), "");
	gtk_entry_set_text (GTK_ENTRY (page_reminders->priv->custom_app_args_entry), "");
	if (page_reminders->priv->custom_email_entry)
		gtk_entry_set_text (GTK_ENTRY (page_reminders->priv->custom_email_entry), "");
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (page_reminders->priv->custom_email_message_check), FALSE);
	ecep_reminders_set_text_view_text (page_reminders->priv->custom_email_message_text_view, NULL);

	e_comp_editor_page_set_updating (page, FALSE);
}

static void
ecep_reminders_selected_to_widgets (ECompEditorPageReminders *page_reminders)
{
	ECalComponentAlarmTrigger *trigger;
	ECalComponentAlarmAction action;
	ECalComponentAlarmRepeat *repeat;
	ECalComponentAlarm *alarm;
	ICalDuration *duration;
	GtkTreeSelection *selection;
	GtkTreeIter iter;

	g_return_if_fail (E_IS_COMP_EDITOR_PAGE_REMINDERS (page_reminders));

	selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (page_reminders->priv->alarms_tree_view));
	g_return_if_fail (gtk_tree_selection_get_selected (selection, NULL, &iter));

	alarm = (ECalComponentAlarm *) e_alarm_list_get_alarm (page_reminders->priv->alarm_list, &iter);
	g_return_if_fail (alarm != NULL);

	action = e_cal_component_alarm_get_action (alarm);
	trigger = e_cal_component_alarm_get_trigger (alarm);

	e_comp_editor_page_set_updating (E_COMP_EDITOR_PAGE (page_reminders), TRUE);

	if (action == E_CAL_COMPONENT_ALARM_NONE) {
		ecep_reminders_reset_alarm_widget (page_reminders);

		e_comp_editor_page_set_updating (E_COMP_EDITOR_PAGE (page_reminders), FALSE);
		return;
	}

	/* Alarm Types */
	switch (e_cal_component_alarm_trigger_get_kind (trigger)) {
	case E_CAL_COMPONENT_ALARM_TRIGGER_RELATIVE_START:
		e_dialog_combo_box_set (page_reminders->priv->relative_to_combo, E_CAL_COMPONENT_ALARM_TRIGGER_RELATIVE_START, time_map);
		break;

	case E_CAL_COMPONENT_ALARM_TRIGGER_RELATIVE_END:
		e_dialog_combo_box_set (page_reminders->priv->relative_to_combo, E_CAL_COMPONENT_ALARM_TRIGGER_RELATIVE_END, time_map);
		break;
	default:
		g_warning ("%s: Unexpected alarm trigger type (%d)", G_STRLOC, e_cal_component_alarm_trigger_get_kind (trigger));
	}

	duration = e_cal_component_alarm_trigger_get_duration (trigger);
	switch (i_cal_duration_is_neg (duration)) {
	case 1:
		e_dialog_combo_box_set (page_reminders->priv->relative_time_combo, BEFORE, relative_map);
		break;

	case 0:
		e_dialog_combo_box_set (page_reminders->priv->relative_time_combo, AFTER, relative_map);
		break;
	}

	if (i_cal_duration_get_days (duration)) {
		e_dialog_combo_box_set (page_reminders->priv->unit_combo, DAYS, value_map);
		gtk_spin_button_set_value (GTK_SPIN_BUTTON (page_reminders->priv->time_spin),
			i_cal_duration_get_days (duration));
	} else if (i_cal_duration_get_hours (duration)) {
		e_dialog_combo_box_set (page_reminders->priv->unit_combo, HOURS, value_map);
		gtk_spin_button_set_value (GTK_SPIN_BUTTON (page_reminders->priv->time_spin),
			i_cal_duration_get_hours (duration));
	} else if (i_cal_duration_get_minutes (duration)) {
		e_dialog_combo_box_set (page_reminders->priv->unit_combo, MINUTES, value_map);
		gtk_spin_button_set_value (GTK_SPIN_BUTTON (page_reminders->priv->time_spin),
			i_cal_duration_get_minutes (duration));
	} else {
		e_dialog_combo_box_set (page_reminders->priv->unit_combo, MINUTES, value_map);
		gtk_spin_button_set_value (GTK_SPIN_BUTTON (page_reminders->priv->time_spin), 0);
	}

	/* Repeat options */
	repeat = e_cal_component_alarm_get_repeat (alarm);

	if (e_cal_component_alarm_repeat_get_repetitions (repeat)) {
		gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (page_reminders->priv->repeat_check), TRUE);
		gtk_spin_button_set_value (GTK_SPIN_BUTTON (page_reminders->priv->repeat_times_spin),
			e_cal_component_alarm_repeat_get_repetitions (repeat));

		duration = e_cal_component_alarm_repeat_get_interval (repeat);

		if (i_cal_duration_get_minutes (duration)) {
			e_dialog_combo_box_set (page_reminders->priv->repeat_unit_combo, DUR_MINUTES, duration_units_map);
			gtk_spin_button_set_value (GTK_SPIN_BUTTON (page_reminders->priv->repeat_every_spin),
				i_cal_duration_get_minutes (duration));
		}

		if (i_cal_duration_get_hours (duration)) {
			e_dialog_combo_box_set (page_reminders->priv->repeat_unit_combo, DUR_HOURS, duration_units_map);
			gtk_spin_button_set_value (GTK_SPIN_BUTTON (page_reminders->priv->repeat_every_spin),
				i_cal_duration_get_hours (duration));
		}

		if (i_cal_duration_get_days (duration)) {
			e_dialog_combo_box_set (page_reminders->priv->repeat_unit_combo, DUR_DAYS, duration_units_map);
			gtk_spin_button_set_value (GTK_SPIN_BUTTON (page_reminders->priv->repeat_every_spin),
				i_cal_duration_get_days (duration));
		}
	} else {
		gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (page_reminders->priv->repeat_check), FALSE);
		gtk_spin_button_set_value (GTK_SPIN_BUTTON (page_reminders->priv->repeat_times_spin), 1);
		gtk_spin_button_set_value (GTK_SPIN_BUTTON (page_reminders->priv->repeat_every_spin), 5);
	}

	/* Alarm options */
	e_dialog_combo_box_set (page_reminders->priv->kind_combo, action, action_map);

	switch (action) {
	case E_CAL_COMPONENT_ALARM_AUDIO: {
		GSList *attachments;
		const gchar *url = NULL;

		attachments = e_cal_component_alarm_get_attachments (alarm);
		/* Audio alarm can have only one attachment, the file to play */
		if (attachments && !attachments->next) {
			ICalAttach *attach = attachments->data;

			url = attach ? i_cal_attach_get_url (attach) : NULL;
		}

		if (url && *url) {
			gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (page_reminders->priv->custom_sound_check), TRUE);
			gtk_file_chooser_set_uri (GTK_FILE_CHOOSER (page_reminders->priv->custom_sound_chooser), url);
		} else {
			gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (page_reminders->priv->custom_sound_check), FALSE);
			gtk_file_chooser_unselect_all (GTK_FILE_CHOOSER (page_reminders->priv->custom_sound_chooser));
		}
		} break;

	case E_CAL_COMPONENT_ALARM_DISPLAY: {
		ECalComponentText* description;

		description = e_cal_component_alarm_get_description (alarm);

		if (description && e_cal_component_text_get_value (description) && e_cal_component_text_get_value (description)[0]) {
			gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (page_reminders->priv->custom_message_check), TRUE);
			ecep_reminders_set_text_view_text (page_reminders->priv->custom_message_text_view, e_cal_component_text_get_value (description));
		} else {
			gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (page_reminders->priv->custom_message_check), FALSE);
			ecep_reminders_set_text_view_text (page_reminders->priv->custom_message_text_view, NULL);
		}
		} break;

	case E_CAL_COMPONENT_ALARM_EMAIL: {
		ENameSelectorModel *name_selector_model;
		EDestinationStore *destination_store;
		ECalComponentText *description;
		GSList *attendees, *link;

		/* Attendees */
		name_selector_model = e_name_selector_peek_model (page_reminders->priv->name_selector);
		e_name_selector_model_peek_section (name_selector_model, SECTION_NAME, NULL, &destination_store);

		attendees = e_cal_component_alarm_get_attendees (alarm);
		for (link = attendees; link; link = g_slist_next (link)) {
			ECalComponentAttendee *att = link->data;
			EDestination *dest;

			dest = e_destination_new ();

			if (att && e_cal_component_attendee_get_cn (att) && e_cal_component_attendee_get_cn (att)[0])
				e_destination_set_name (dest, e_cal_component_attendee_get_cn (att));

			if (att && e_cal_component_attendee_get_value (att) && e_cal_component_attendee_get_value (att)[0])
				e_destination_set_email (dest, itip_strip_mailto (e_cal_component_attendee_get_value (att)));

			e_destination_store_append_destination (destination_store, dest);

			g_object_unref (dest);
		}

		/* Description */
		description = e_cal_component_alarm_get_description (alarm);
		if (description && e_cal_component_text_get_value (description) && e_cal_component_text_get_value (description)[0]) {
			gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (page_reminders->priv->custom_email_message_check), TRUE);
			ecep_reminders_set_text_view_text (page_reminders->priv->custom_email_message_text_view, e_cal_component_text_get_value (description));
		} else {
			gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (page_reminders->priv->custom_email_message_check), FALSE);
			ecep_reminders_set_text_view_text (page_reminders->priv->custom_email_message_text_view, NULL);
		}
		} break;

	case E_CAL_COMPONENT_ALARM_PROCEDURE: {
		const gchar *url = NULL;
		GSList *attachments;

		attachments = e_cal_component_alarm_get_attachments (alarm);
		if (attachments && !attachments->next) {
			ICalAttach *attach = attachments->data;
			url = attach ? i_cal_attach_get_url (attach) : NULL;
		}

		if (url && *url) {
			ECalComponentText *description;

			description = e_cal_component_alarm_get_description (alarm);

			gtk_entry_set_text (GTK_ENTRY (page_reminders->priv->custom_app_path_entry), url);
			gtk_entry_set_text (GTK_ENTRY (page_reminders->priv->custom_app_args_entry),
				(description && e_cal_component_text_get_value (description)) ? e_cal_component_text_get_value (description) : "");
		} else {
			gtk_entry_set_text (GTK_ENTRY (page_reminders->priv->custom_app_path_entry), "");
			gtk_entry_set_text (GTK_ENTRY (page_reminders->priv->custom_app_args_entry), "");
		}
		} break;
	default:
		g_warning ("%s: Unexpected alarm action (%d)", G_STRLOC, action);
	}

	e_comp_editor_page_set_updating (E_COMP_EDITOR_PAGE (page_reminders), FALSE);
}

static void
ecep_reminders_widgets_to_selected (ECompEditorPageReminders *page_reminders)
{
	ECalComponentAlarmTrigger *trigger;
	ECalComponentAlarmAction action;
	ECalComponentAlarmRepeat *repeat = NULL;
	ECalComponentAlarm *alarm;
	ICalDuration *duration;
	GtkTreeSelection *selection;
	GtkTreeIter iter;

	g_return_if_fail (E_IS_COMP_EDITOR_PAGE_REMINDERS (page_reminders));

	if (e_comp_editor_page_get_updating (E_COMP_EDITOR_PAGE (page_reminders)))
		return;

	selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (page_reminders->priv->alarms_tree_view));
	if (!gtk_tree_selection_get_selected (selection, NULL, &iter))
		return;

	alarm = e_cal_component_alarm_new ();

	duration = i_cal_duration_new_null_duration ();

	if (e_dialog_combo_box_get (page_reminders->priv->relative_time_combo, relative_map) == BEFORE)
		i_cal_duration_set_is_neg (duration, TRUE);
	else
		i_cal_duration_set_is_neg (duration, FALSE);

	switch (e_dialog_combo_box_get (page_reminders->priv->unit_combo, value_map)) {
	case MINUTES:
		i_cal_duration_set_minutes (duration, gtk_spin_button_get_value_as_int (
			GTK_SPIN_BUTTON (page_reminders->priv->time_spin)));
		break;

	case HOURS:
		i_cal_duration_set_hours (duration, gtk_spin_button_get_value_as_int (
			GTK_SPIN_BUTTON (page_reminders->priv->time_spin)));
		break;

	case DAYS:
		i_cal_duration_set_days (duration, gtk_spin_button_get_value_as_int (
			GTK_SPIN_BUTTON (page_reminders->priv->time_spin)));
		break;

	default:
		g_return_if_reached ();
	}

	trigger = e_cal_component_alarm_trigger_new_relative (
		e_dialog_combo_box_get (page_reminders->priv->relative_to_combo, time_map),
		duration);

	g_object_unref (duration);

	e_cal_component_alarm_take_trigger (alarm, trigger);

	action = e_dialog_combo_box_get (page_reminders->priv->kind_combo, action_map);
	e_cal_component_alarm_set_action (alarm, action);

	/* Repeat stuff */
	if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (page_reminders->priv->repeat_check))) {
		duration = i_cal_duration_new_null_duration ();

		switch (e_dialog_combo_box_get (page_reminders->priv->repeat_unit_combo, duration_units_map)) {
		case DUR_MINUTES:
			i_cal_duration_set_minutes (duration, gtk_spin_button_get_value_as_int (
				GTK_SPIN_BUTTON (page_reminders->priv->repeat_every_spin)));
			break;

		case DUR_HOURS:
			i_cal_duration_set_hours (duration, gtk_spin_button_get_value_as_int (
				GTK_SPIN_BUTTON (page_reminders->priv->repeat_every_spin)));
			break;

		case DUR_DAYS:
			i_cal_duration_set_days (duration, gtk_spin_button_get_value_as_int (
				GTK_SPIN_BUTTON (page_reminders->priv->repeat_every_spin)));
			break;

		default:
			g_return_if_reached ();
		}

		repeat = e_cal_component_alarm_repeat_new (gtk_spin_button_get_value_as_int (
			GTK_SPIN_BUTTON (page_reminders->priv->repeat_times_spin)), duration);

		g_object_unref (duration);
	}

	e_cal_component_alarm_take_repeat (alarm, repeat);

	/* Options */
	switch (action) {
	case E_CAL_COMPONENT_ALARM_NONE:
		g_return_if_reached ();
		break;

	case E_CAL_COMPONENT_ALARM_AUDIO:
		if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (page_reminders->priv->custom_sound_check))) {
			gchar *url;

			url = gtk_file_chooser_get_uri (GTK_FILE_CHOOSER (page_reminders->priv->custom_sound_chooser));

			if (url && *url) {
				ICalAttach *attach;
				GSList *attachments;

				attach = i_cal_attach_new_from_url (url);
				attachments = g_slist_prepend (NULL, attach);
				e_cal_component_alarm_take_attachments (alarm, attachments);
			}

			g_free (url);

			url = gtk_file_chooser_get_filename (GTK_FILE_CHOOSER (page_reminders->priv->custom_sound_chooser));
			if (url && *url) {
				gchar *path;

				path = g_path_get_dirname (url);
				if (path && *path) {
					calendar_config_set_dir_path (path);
				}

				g_free (path);
			}

			g_free (url);
		}
		break;

	case E_CAL_COMPONENT_ALARM_DISPLAY:
		if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (page_reminders->priv->custom_message_check))) {
			gchar *text;

			text = ecep_reminders_get_text_view_text (page_reminders->priv->custom_message_text_view);
			if (text && *text) {
				ECalComponentText *description;

				description = e_cal_component_text_new (text, NULL);

				e_cal_component_alarm_take_description (alarm, description);

				ecep_reminders_remove_needs_description_property (alarm);
			}

			g_free (text);
		}
		break;

	case E_CAL_COMPONENT_ALARM_EMAIL: {
		GSList *attendees = NULL;
		ENameSelectorModel *name_selector_model;
		EDestinationStore *destination_store;
		GList *destinations, *link;

		/* Attendees */
		name_selector_model = e_name_selector_peek_model (page_reminders->priv->name_selector);
		e_name_selector_model_peek_section (name_selector_model, SECTION_NAME, NULL, &destination_store);
		destinations = e_destination_store_list_destinations (destination_store);

		for (link = destinations; link; link = g_list_next (link)) {
			EDestination *dest = link->data;
			ECalComponentAttendee *att;
			gchar *mailto;

			mailto = g_strconcat ("mailto:", e_destination_get_email (dest), NULL);
			att = e_cal_component_attendee_new ();
			e_cal_component_attendee_set_value (att, mailto);
			e_cal_component_attendee_set_cn (att, e_destination_get_name (dest));
			e_cal_component_attendee_set_cutype (att, I_CAL_CUTYPE_INDIVIDUAL);
			e_cal_component_attendee_set_partstat (att, I_CAL_PARTSTAT_NEEDSACTION);
			e_cal_component_attendee_set_role (att, I_CAL_ROLE_REQPARTICIPANT);

			attendees = g_slist_prepend (attendees, att);

			g_free (mailto);
		}

		e_cal_component_alarm_take_attendees (alarm, g_slist_reverse (attendees));

		g_list_free (destinations);

		if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (page_reminders->priv->custom_email_message_check))) {
			gchar *text;

			text = ecep_reminders_get_text_view_text (page_reminders->priv->custom_email_message_text_view);
			if (text && *text) {
				ECalComponentText *description;

				description = e_cal_component_text_new (text, NULL);
				e_cal_component_alarm_take_description (alarm, description);

				ecep_reminders_remove_needs_description_property (alarm);
			}

			g_free (text);
		}
		} break;

	case E_CAL_COMPONENT_ALARM_PROCEDURE: {
		ECalComponentText *description;
		GSList *attachments;
		const gchar *text;

		text = gtk_entry_get_text (GTK_ENTRY (page_reminders->priv->custom_app_path_entry));

		attachments = g_slist_prepend (NULL, i_cal_attach_new_from_url (text ? text : ""));
		e_cal_component_alarm_take_attachments (alarm, attachments);

		text = gtk_entry_get_text (GTK_ENTRY (page_reminders->priv->custom_app_args_entry));

		description = e_cal_component_text_new (text, NULL);

		e_cal_component_alarm_take_description (alarm, description);
		ecep_reminders_remove_needs_description_property (alarm);
		} break;

	case E_CAL_COMPONENT_ALARM_UNKNOWN:
		break;

	default:
		g_return_if_reached ();
	}

	e_alarm_list_set_alarm (page_reminders->priv->alarm_list, &iter, alarm);

	e_cal_component_alarm_free (alarm);

	e_comp_editor_page_emit_changed (E_COMP_EDITOR_PAGE (page_reminders));
}

static void
ecep_reminders_alarms_selection_changed_cb (GtkTreeSelection *selection,
					    ECompEditorPageReminders *page_reminders)
{
	g_return_if_fail (GTK_IS_TREE_SELECTION (selection));
	g_return_if_fail (E_IS_COMP_EDITOR_PAGE_REMINDERS (page_reminders));

	ecep_reminders_sanitize_option_widgets (page_reminders);

	if (gtk_tree_selection_get_selected (selection, NULL, NULL))
		ecep_reminders_selected_to_widgets (page_reminders);
}

static void
ecep_reminders_alarms_combo_changed_cb (GtkComboBox *combo_box,
					ECompEditorPageReminders *page_reminders)
{
	ECalComponentAlarm *alarm;
	ICalDuration *duration;
	gint alarm_type;

	g_return_if_fail (E_IS_COMP_EDITOR_PAGE_REMINDERS (page_reminders));

	ecep_reminders_sanitize_option_widgets (page_reminders);

	if (!e_comp_editor_page_get_updating (E_COMP_EDITOR_PAGE (page_reminders)))
		e_comp_editor_page_emit_changed (E_COMP_EDITOR_PAGE (page_reminders));

	alarm_type = e_dialog_combo_box_get (page_reminders->priv->alarms_combo, page_reminders->priv->alarm_map);
	if (alarm_type == ALARM_NONE) {
		e_alarm_list_clear (page_reminders->priv->alarm_list);
		return;
	}

	if (alarm_type == ALARM_CUSTOM) {
		GtkTreeSelection *selection;

		selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (page_reminders->priv->alarms_tree_view));

		if (!gtk_tree_selection_get_selected (selection, NULL, NULL)) {
			GtkTreeIter iter;

			if (gtk_tree_model_get_iter_first (GTK_TREE_MODEL (page_reminders->priv->alarm_list), &iter))
				gtk_tree_selection_select_iter (selection, &iter);
		}

		return;
	}

	e_alarm_list_clear (page_reminders->priv->alarm_list);

	alarm = e_cal_component_alarm_new ();

	e_cal_component_alarm_set_action (alarm, E_CAL_COMPONENT_ALARM_DISPLAY);

	duration = i_cal_duration_new_null_duration ();

	i_cal_duration_set_is_neg (duration, TRUE);

	switch (alarm_type) {
	case ALARM_15_MINUTES:
		i_cal_duration_set_minutes (duration, 15);
		break;

	case ALARM_1_HOUR:
		i_cal_duration_set_hours (duration, 1);
		break;

	case ALARM_1_DAY:
		i_cal_duration_set_days (duration, 1);
		break;

	case ALARM_USER_TIME:
		switch (page_reminders->priv->alarm_units) {
		case E_DURATION_DAYS:
			i_cal_duration_set_days (duration, page_reminders->priv->alarm_interval);
			break;

		case E_DURATION_HOURS:
			i_cal_duration_set_hours (duration, page_reminders->priv->alarm_interval);
			break;

		case E_DURATION_MINUTES:
			i_cal_duration_set_minutes (duration, page_reminders->priv->alarm_interval);
			break;
		}
		break;

	default:
		break;
	}

	e_cal_component_alarm_take_trigger (alarm,
		e_cal_component_alarm_trigger_new_relative (E_CAL_COMPONENT_ALARM_TRIGGER_RELATIVE_START, duration));
	ecep_reminders_add_needs_description_property (alarm);
	e_alarm_list_append (page_reminders->priv->alarm_list, NULL, alarm);
	e_cal_component_alarm_free (alarm);
	g_object_unref (duration);
}

static void
ecep_reminders_alarms_add_clicked_cb (GtkButton *button,
				      ECompEditorPageReminders *page_reminders)
{
	GtkTreeSelection *selection;
	GtkTreeIter iter;
	ECalComponentAlarm *alarm;
	ICalDuration *duration;

	g_return_if_fail (E_IS_COMP_EDITOR_PAGE_REMINDERS (page_reminders));

	alarm = e_cal_component_alarm_new ();

	ecep_reminders_add_needs_description_property (alarm);

	duration = i_cal_duration_new_null_duration ();
	i_cal_duration_set_is_neg (duration, TRUE);
	i_cal_duration_set_minutes (duration, 15);

	e_cal_component_alarm_set_action (alarm, E_CAL_COMPONENT_ALARM_DISPLAY);
	e_cal_component_alarm_take_trigger (alarm,
		e_cal_component_alarm_trigger_new_relative (E_CAL_COMPONENT_ALARM_TRIGGER_RELATIVE_START, duration));
	g_object_unref (duration);

	e_alarm_list_append (page_reminders->priv->alarm_list, &iter, alarm);

	selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (page_reminders->priv->alarms_tree_view));
	gtk_tree_selection_select_iter (selection, &iter);

	ecep_reminders_sanitize_option_widgets (page_reminders);
}

static void
ecep_reminders_alarms_remove_clicked_cb (GtkButton *button,
					 ECompEditorPageReminders *page_reminders)
{
	GtkTreeSelection *selection;
	GtkTreeIter iter;
	GtkTreePath *path;
	GtkTreeModel *model;
	gboolean valid_iter;

	g_return_if_fail (E_IS_COMP_EDITOR_PAGE_REMINDERS (page_reminders));

	selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (page_reminders->priv->alarms_tree_view));
	g_return_if_fail (gtk_tree_selection_get_selected (selection, &model, &iter));

	path = gtk_tree_model_get_path (model, &iter);

	e_alarm_list_remove (page_reminders->priv->alarm_list, &iter);

	/* Select closest item after removal */
	valid_iter = gtk_tree_model_get_iter (model, &iter, path);
	if (!valid_iter && gtk_tree_path_prev (path)) {
		valid_iter = gtk_tree_model_get_iter (model, &iter, path);
	}

	if (valid_iter)
		gtk_tree_selection_select_iter (selection, &iter);

	e_comp_editor_page_emit_changed (E_COMP_EDITOR_PAGE (page_reminders));
}

static void
ecep_reminders_name_selector_dialog_response_cb (GtkWidget *widget,
						 gint response,
						 ECompEditorPageReminders *page_reminders)
{
	ENameSelectorDialog *name_selector_dialog;

	g_return_if_fail (E_IS_COMP_EDITOR_PAGE_REMINDERS (page_reminders));

	name_selector_dialog = e_name_selector_peek_dialog (page_reminders->priv->name_selector);
	gtk_widget_hide (GTK_WIDGET (name_selector_dialog));
}

static void
ecep_reminders_set_alarm_email (ECompEditorPageReminders *page_reminders)
{
	ECompEditor *comp_editor;
	ECalClient *target_client;

	g_return_if_fail (E_IS_COMP_EDITOR_PAGE_REMINDERS (page_reminders));

	if (!page_reminders->priv->name_selector)
		return;

	comp_editor = e_comp_editor_page_ref_editor (E_COMP_EDITOR_PAGE (page_reminders));
	target_client = e_comp_editor_get_target_client (comp_editor);

	if (target_client &&
	    !e_client_check_capability (E_CLIENT (target_client), E_CAL_STATIC_CAPABILITY_NO_EMAIL_ALARMS)) {
		ENameSelectorModel *selector_model;
		EDestinationStore *destination_store = NULL;
		const gchar *alarm_email;

		alarm_email = e_comp_editor_get_alarm_email_address (comp_editor);
		selector_model = e_name_selector_peek_model (page_reminders->priv->name_selector);
		if (alarm_email && *alarm_email &&
		    e_name_selector_model_peek_section (selector_model, SECTION_NAME, NULL, &destination_store) &&
		    destination_store && !gtk_tree_model_iter_n_children (GTK_TREE_MODEL (destination_store), NULL)) {
			EDestination *dest;

			dest = e_destination_new ();

			e_destination_set_email (dest, alarm_email);
			e_destination_store_append_destination (destination_store, dest);

			g_object_unref (dest);
		}
	}

	g_clear_object (&comp_editor);
}

static void
ecep_reminders_setup_name_selector (ECompEditorPageReminders *page_reminders)
{
	ECompEditor *comp_editor;
	ENameSelectorModel *name_selector_model;
	ENameSelectorDialog *name_selector_dialog;
	GtkWidget *widget, *option_grid;

	g_return_if_fail (E_IS_COMP_EDITOR_PAGE_REMINDERS (page_reminders));
	g_return_if_fail (page_reminders->priv->name_selector == NULL);
	g_return_if_fail (page_reminders->priv->custom_email_entry == NULL);

	comp_editor = e_comp_editor_page_ref_editor (E_COMP_EDITOR_PAGE (page_reminders));

	page_reminders->priv->name_selector = e_name_selector_new (e_shell_get_client_cache (e_comp_editor_get_shell (comp_editor)));

	e_name_selector_load_books (page_reminders->priv->name_selector);
	name_selector_model = e_name_selector_peek_model (page_reminders->priv->name_selector);

	e_name_selector_model_add_section (name_selector_model, SECTION_NAME, SECTION_NAME, NULL);

	option_grid = gtk_notebook_get_nth_page (GTK_NOTEBOOK (page_reminders->priv->options_notebook), 3);

	widget = GTK_WIDGET (e_name_selector_peek_section_entry (page_reminders->priv->name_selector, SECTION_NAME));
	g_object_set (G_OBJECT (widget),
		"hexpand", TRUE,
		"halign", GTK_ALIGN_FILL,
		"vexpand", FALSE,
		"valign", GTK_ALIGN_CENTER,
		"margin-start", 4,
		NULL);
	gtk_widget_show (widget);
	gtk_grid_attach (GTK_GRID (option_grid), widget, 1, 0, 1, 1);
	page_reminders->priv->custom_email_entry = widget;

	g_signal_connect_swapped (page_reminders->priv->custom_email_entry, "changed",
		G_CALLBACK (ecep_reminders_widgets_to_selected), page_reminders);

	name_selector_dialog = e_name_selector_peek_dialog (page_reminders->priv->name_selector);
	g_signal_connect (name_selector_dialog, "response",
		G_CALLBACK (ecep_reminders_name_selector_dialog_response_cb), page_reminders);

	ecep_reminders_set_alarm_email (page_reminders);

	g_clear_object (&comp_editor);
}

static void
ecep_reminders_kind_combo_changed_cb (GtkWidget *combo_box,
				      ECompEditorPageReminders *page_reminders)
{
	ECalComponentAlarmAction action;
	gint page = 0, ii;

	g_return_if_fail (E_IS_COMP_EDITOR_PAGE_REMINDERS (page_reminders));

	if (!page_reminders->priv->name_selector &&
	    e_dialog_combo_box_get (combo_box, action_map) == E_CAL_COMPONENT_ALARM_EMAIL) {
		ecep_reminders_setup_name_selector (page_reminders);
	}

	action = e_dialog_combo_box_get (page_reminders->priv->kind_combo, action_map);
	for (ii = 0; action_map[ii] != -1; ii++) {
		if (action == action_map[ii]) {
			page = ii;
			break;
		}
	}

	gtk_notebook_set_current_page (GTK_NOTEBOOK (page_reminders->priv->options_notebook), page);
}

static void
ecep_reminders_send_to_clicked_cb (GtkWidget *button,
				   ECompEditorPageReminders *page_reminders)
{
	GtkWidget *toplevel;

	g_return_if_fail (E_IS_COMP_EDITOR_PAGE_REMINDERS (page_reminders));
	g_return_if_fail (page_reminders->priv->name_selector != NULL);

	toplevel = gtk_widget_get_toplevel (button);
	if (!GTK_IS_WINDOW (toplevel))
		toplevel = NULL;

	e_name_selector_show_dialog (page_reminders->priv->name_selector, toplevel);
}

static gboolean
ecep_reminders_is_custom_alarm (ECalComponentAlarm *ca,
				const gchar *old_summary,
				EDurationType user_units,
				gint user_interval,
				gint *alarm_type)
{
	ECalComponentAlarmTrigger *trigger;
	ECalComponentAlarmRepeat *repeat;
	ECalComponentAlarmAction action;
	ECalComponentText *desc;
	ICalDuration *duration;
	GSList *attachments;

	action = e_cal_component_alarm_get_action (ca);
	if (action != E_CAL_COMPONENT_ALARM_DISPLAY)
		return TRUE;

	attachments = e_cal_component_alarm_get_attachments (ca);
	if (attachments)
		return TRUE;

	if (!ecep_reminders_has_needs_description_property (ca)) {
		desc = e_cal_component_alarm_get_description (ca);
		if (!desc || !e_cal_component_text_get_value (desc) ||
		    !old_summary || strcmp (e_cal_component_text_get_value (desc), old_summary))
			return TRUE;
	}

	repeat = e_cal_component_alarm_get_repeat (ca);
	if (repeat && e_cal_component_alarm_repeat_get_repetitions (repeat) != 0)
		return TRUE;

	if (e_cal_component_alarm_has_attendees (ca))
		return TRUE;

	trigger = e_cal_component_alarm_get_trigger (ca);
	if (!trigger || e_cal_component_alarm_trigger_get_kind (trigger) != E_CAL_COMPONENT_ALARM_TRIGGER_RELATIVE_START)
		return TRUE;

	duration = e_cal_component_alarm_trigger_get_duration (trigger);
	if (!duration || i_cal_duration_is_neg (duration) != 1)
		return TRUE;

	if (i_cal_duration_get_weeks (duration) != 0)
		return TRUE;

	if (i_cal_duration_get_seconds (duration) != 0)
		return TRUE;

	if (i_cal_duration_get_days (duration) == 1 &&
	    i_cal_duration_get_hours (duration) == 0 &&
	    i_cal_duration_get_minutes (duration) == 0) {
		if (alarm_type)
			*alarm_type = ALARM_1_DAY;
		return FALSE;
	}

	if (i_cal_duration_get_days (duration) == 0 &&
	    i_cal_duration_get_hours (duration) == 1 &&
	    i_cal_duration_get_minutes (duration) == 0) {
		if (alarm_type)
			*alarm_type = ALARM_1_HOUR;
		return FALSE;
	}

	if (i_cal_duration_get_days (duration) == 0 &&
	    i_cal_duration_get_hours (duration) == 0 &&
	    i_cal_duration_get_minutes (duration) == 15) {
		if (alarm_type)
			*alarm_type = ALARM_15_MINUTES;
		return FALSE;
	}

	if (user_interval != -1) {
		switch (user_units) {
		case E_DURATION_DAYS:
			if (i_cal_duration_get_days (duration) == user_interval &&
			    i_cal_duration_get_hours (duration) == 0 &&
			    i_cal_duration_get_minutes (duration) == 0) {
				if (alarm_type)
					*alarm_type = ALARM_USER_TIME;
				return FALSE;
			}
			break;

		case E_DURATION_HOURS:
			if (i_cal_duration_get_days (duration) == 0 &&
			    i_cal_duration_get_hours (duration) == user_interval &&
			    i_cal_duration_get_minutes (duration) == 0) {
				if (alarm_type)
					*alarm_type = ALARM_USER_TIME;
				return FALSE;
			}
			break;

		case E_DURATION_MINUTES:
			if (i_cal_duration_get_days (duration) == 0 &&
			    i_cal_duration_get_hours (duration) == 0 &&
			    i_cal_duration_get_minutes (duration) == user_interval) {
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
ecep_reminders_is_custom_alarm_uid_list (ECalComponent *comp,
					 GSList *alarm_uids,
					 const gchar *old_summary,
					 EDurationType user_units,
					 gint user_interval,
					 gint *alarm_type)
{
	ECalComponentAlarm *ca;
	gboolean result;

	if (!alarm_uids)
		return FALSE;

	if (alarm_uids->next)
		return TRUE;

	ca = e_cal_component_get_alarm (comp, alarm_uids->data);
	result = ecep_reminders_is_custom_alarm (ca, old_summary, user_units, user_interval, alarm_type);
	e_cal_component_alarm_free (ca);

	return result;
}

static void
ecep_reminders_init_sensitable_combo_box (GtkComboBox *combo_box,
					  const gchar *first_item,
					  ...) G_GNUC_NULL_TERMINATED;

static void
ecep_reminders_init_sensitable_combo_box (GtkComboBox *combo_box,
					  const gchar *first_item,
					  ...)
{
	GtkCellRenderer *cell;
	GtkCellLayout *cell_layout;
	GtkListStore *store;
	const gchar *item;
	va_list va;

	g_return_if_fail (GTK_IS_COMBO_BOX (combo_box));

	store = gtk_list_store_new (2, G_TYPE_STRING, G_TYPE_BOOLEAN);
	gtk_combo_box_set_model (combo_box, GTK_TREE_MODEL (store));
	g_object_unref (store);

	cell_layout = GTK_CELL_LAYOUT (combo_box);

	gtk_cell_layout_clear (cell_layout);

	cell = gtk_cell_renderer_text_new ();
	gtk_cell_layout_pack_start (cell_layout, cell, TRUE);
	gtk_cell_layout_set_attributes (
		cell_layout, cell,
		"text", 0,
		"sensitive", 1,
		NULL);

	va_start (va, first_item);

	item = first_item;
	while (item) {
		GtkTreeIter iter;

		gtk_list_store_append (store, &iter);
		gtk_list_store_set (
			store, &iter,
			0, item,
			1, TRUE,
			-1);

		item = va_arg (va, const gchar *);
	}

	va_end (va);
}

static void
ecep_reminders_sensitize_relative_time_combo_items (GtkWidget *combobox,
						    EClient *client,
						    const gint *map,
						    gint prop)
{
	GtkTreeModel *model;
	GtkTreeIter iter;
	gboolean valid;
	gboolean alarm_after_start;
	gint ii;

	alarm_after_start = !e_client_check_capability (client, E_CAL_STATIC_CAPABILITY_NO_ALARM_AFTER_START);
	model = gtk_combo_box_get_model (GTK_COMBO_BOX (combobox));
	valid = gtk_tree_model_get_iter_first (model, &iter);

	for (ii = 0; valid && map[ii] != -1; ii++) {
		gtk_list_store_set (GTK_LIST_STORE (model), &iter,
				1, alarm_after_start ? TRUE : (map[ii] == prop ? FALSE : TRUE),
				-1);
		valid = gtk_tree_model_iter_next (model, &iter);
	}
}

static void
ecep_reminders_sensitize_widgets_by_client (ECompEditorPageReminders *page_reminders,
					    ECompEditor *comp_editor,
					    EClient *target_client)
{
	GtkTreeModel *model;
	GtkTreeIter iter;
	gboolean valid;
	gint ii;

	g_return_if_fail (E_IS_COMP_EDITOR_PAGE_REMINDERS (page_reminders));
	g_return_if_fail (E_IS_CAL_CLIENT (target_client));

	/* Alarm types */
	model = gtk_combo_box_get_model (GTK_COMBO_BOX (page_reminders->priv->kind_combo));
	valid = gtk_tree_model_get_iter_first (model, &iter);

	for (ii = 0; valid && action_map[ii] != -1; ii++) {
		gtk_list_store_set (
			GTK_LIST_STORE (model), &iter,
			1, !e_client_check_capability (target_client, action_map_cap[ii]),
			-1);
		valid = gtk_tree_model_iter_next (model, &iter);
	}

	ecep_reminders_sensitize_relative_time_combo_items (page_reminders->priv->relative_time_combo,
		target_client, relative_map, AFTER);
	ecep_reminders_sensitize_relative_time_combo_items (page_reminders->priv->relative_to_combo,
		target_client, time_map, E_CAL_COMPONENT_ALARM_TRIGGER_RELATIVE_END);

	/* If the client doesn't support set alarm description, disable the related widgets */
	if (e_client_check_capability (target_client, E_CAL_STATIC_CAPABILITY_ALARM_DESCRIPTION)) {
		gtk_widget_show (page_reminders->priv->custom_message_check);
		gtk_widget_show (page_reminders->priv->custom_message_text_view);
	} else {
		gtk_widget_hide (page_reminders->priv->custom_message_check);
		gtk_widget_hide (page_reminders->priv->custom_message_text_view);
	}

	/* Set a default address if possible */
	ecep_reminders_set_alarm_email (page_reminders);

	/* If we can repeat */
	gtk_widget_set_sensitive (page_reminders->priv->repeat_check,
		!e_client_check_capability (target_client, E_CAL_STATIC_CAPABILITY_NO_ALARM_REPEAT));
}

static void
ecep_reminders_sensitize_widgets (ECompEditorPage *page,
				  gboolean force_insensitive)
{
	ECompEditorPageReminders *page_reminders;

	g_return_if_fail (E_IS_COMP_EDITOR_PAGE_REMINDERS (page));

	E_COMP_EDITOR_PAGE_CLASS (e_comp_editor_page_reminders_parent_class)->sensitize_widgets (page, force_insensitive);

	page_reminders = E_COMP_EDITOR_PAGE_REMINDERS (page);

	gtk_widget_set_sensitive (page_reminders->priv->alarms_combo, !force_insensitive);
	gtk_widget_set_sensitive (page_reminders->priv->alarms_scrolled_window, !force_insensitive);
	gtk_widget_set_sensitive (page_reminders->priv->alarms_button_box, !force_insensitive);
	gtk_widget_set_sensitive (page_reminders->priv->alarm_setup_hbox, !force_insensitive);
	gtk_widget_set_sensitive (page_reminders->priv->repeat_setup_hbox, !force_insensitive);
	gtk_widget_set_sensitive (page_reminders->priv->options_notebook, !force_insensitive);

	if (!force_insensitive) {
		ECompEditor *comp_editor;
		ECalClient *target_client;

		comp_editor = e_comp_editor_page_ref_editor (page);
		target_client = e_comp_editor_get_target_client (comp_editor);

		if (target_client)
			ecep_reminders_sensitize_widgets_by_client (page_reminders, comp_editor, E_CLIENT (target_client));

		g_clear_object (&comp_editor);
	}

	ecep_reminders_sanitize_option_widgets (page_reminders);
}

static void
ecep_reminders_fill_widgets (ECompEditorPage *page,
			     ICalComponent *component)
{
	ECompEditorPageReminders *page_reminders;
	ECalComponent *comp;
	ICalComponent *valarm;

	g_return_if_fail (E_IS_COMP_EDITOR_PAGE_REMINDERS (page));
	g_return_if_fail (I_CAL_IS_COMPONENT (component));

	E_COMP_EDITOR_PAGE_CLASS (e_comp_editor_page_reminders_parent_class)->fill_widgets (page, component);

	page_reminders = E_COMP_EDITOR_PAGE_REMINDERS (page);

	e_alarm_list_clear (page_reminders->priv->alarm_list);

	valarm = i_cal_component_get_first_component (component, I_CAL_VALARM_COMPONENT);
	if (!valarm) {
		e_dialog_combo_box_set (page_reminders->priv->alarms_combo, ALARM_NONE, page_reminders->priv->alarm_map);
		return;
	}

	g_object_unref (valarm);

	comp = e_cal_component_new_from_icalcomponent (i_cal_component_clone (component));
	if (comp && e_cal_component_has_alarms (comp)) {
		GSList *alarms, *link;
		gint alarm_type = ALARM_NONE;

		alarms = e_cal_component_get_alarm_uids (comp);

		if (ecep_reminders_is_custom_alarm_uid_list (comp, alarms, i_cal_component_get_summary (component),
			page_reminders->priv->alarm_units, page_reminders->priv->alarm_interval, &alarm_type))
			alarm_type = ALARM_CUSTOM;

		e_dialog_combo_box_set (page_reminders->priv->alarms_combo, alarm_type, page_reminders->priv->alarm_map);

		e_alarm_list_clear (page_reminders->priv->alarm_list);

		for (link = alarms; link; link = g_slist_next (link)) {
			ECalComponentAlarm *ca;
			const gchar *uid = link->data;

			ca = e_cal_component_get_alarm (comp, uid);
			e_alarm_list_append (page_reminders->priv->alarm_list, NULL, ca);
			e_cal_component_alarm_free (ca);
		}

		g_slist_free_full (alarms, g_free);

		if (e_dialog_combo_box_get (page_reminders->priv->alarms_combo, page_reminders->priv->alarm_map) == ALARM_CUSTOM) {
			GtkTreeSelection *selection;
			GtkTreeIter iter;

			selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (page_reminders->priv->alarms_tree_view));
			if (gtk_tree_model_get_iter_first (GTK_TREE_MODEL (page_reminders->priv->alarm_list), &iter))
				gtk_tree_selection_select_iter (selection, &iter);
		}
	} else {
		e_dialog_combo_box_set (page_reminders->priv->alarms_combo, ALARM_NONE, page_reminders->priv->alarm_map);
	}

	g_clear_object (&comp);
}

static gboolean
ecep_reminders_fill_component (ECompEditorPage *page,
			       ICalComponent *component)
{
	ECompEditorPageReminders *page_reminders;
	ECalComponent *comp;
	ICalComponent *changed_comp, *alarm;
	GtkTreeModel *model;
	GtkTreeIter iter;
	gboolean valid_iter;

	g_return_val_if_fail (E_IS_COMP_EDITOR_PAGE_REMINDERS (page), FALSE);
	g_return_val_if_fail (I_CAL_IS_COMPONENT (component), FALSE);

	if (!E_COMP_EDITOR_PAGE_CLASS (e_comp_editor_page_reminders_parent_class)->fill_component (page, component))
		return TRUE;

	comp = e_cal_component_new_from_icalcomponent (i_cal_component_clone (component));
	g_return_val_if_fail (comp != NULL, FALSE);

	page_reminders = E_COMP_EDITOR_PAGE_REMINDERS (page);

	e_cal_component_remove_all_alarms (comp);

	model = GTK_TREE_MODEL (page_reminders->priv->alarm_list);

	for (valid_iter = gtk_tree_model_get_iter_first (model, &iter);
	     valid_iter;
	     valid_iter = gtk_tree_model_iter_next (model, &iter)) {
		ECalComponentAlarm *alarm, *alarm_copy;
		ECalComponentAlarmAction action = E_CAL_COMPONENT_ALARM_UNKNOWN;
		ECalComponentPropertyBag *bag;

		alarm = (ECalComponentAlarm *) e_alarm_list_get_alarm (page_reminders->priv->alarm_list, &iter);
		if (!alarm) {
			g_warning ("alarm is NULL\n");
			continue;
		}

		/* We set the description of the alarm if it's got
		 * the X-EVOLUTION-NEEDS-DESCRIPTION property.
		 */
		if (ecep_reminders_remove_needs_description_property (alarm)) {
			ECalComponentText *summary;

			summary = e_cal_component_get_summary (comp);
			e_cal_component_alarm_take_description (alarm, summary);
		}

		action = e_cal_component_alarm_get_action (alarm);

		bag = e_cal_component_alarm_get_property_bag (alarm);
		if (action == E_CAL_COMPONENT_ALARM_EMAIL) {
			ECalComponentText *summary;
			const gchar *text;
			guint idx;

			summary = e_cal_component_get_summary (comp);
			text = (summary && e_cal_component_text_get_value (summary)) ? e_cal_component_text_get_value (summary) : "";

			idx = e_cal_component_property_bag_get_first_by_kind (bag, I_CAL_SUMMARY_PROPERTY);
			if (idx < e_cal_component_property_bag_get_count (bag)) {
				ICalProperty *prop;

				prop = e_cal_component_property_bag_get (bag, idx);
				i_cal_property_set_summary (prop, text);
			} else {
				e_cal_component_property_bag_take (bag, i_cal_property_new_summary (text));
			}

			e_cal_component_text_free (summary);
		} else {
			e_cal_component_property_bag_remove_by_kind (bag, I_CAL_SUMMARY_PROPERTY, TRUE);
		}

		if (action == E_CAL_COMPONENT_ALARM_EMAIL || action == E_CAL_COMPONENT_ALARM_DISPLAY) {
			if (e_cal_component_property_bag_get_first_by_kind (bag, I_CAL_DESCRIPTION_PROPERTY) >=
			    e_cal_component_property_bag_get_count (bag)) {
				const gchar *description;

				description = i_cal_component_get_description (e_cal_component_get_icalcomponent (comp));

				e_cal_component_property_bag_take (bag, i_cal_property_new_description (description ? description : ""));
			}
		} else {
			e_cal_component_property_bag_remove_by_kind (bag, I_CAL_DESCRIPTION_PROPERTY, TRUE);
		}

		/* We clone the alarm to maintain the invariant that the alarm
		 * structures in the list did *not* come from the component.
		 */

		alarm_copy = e_cal_component_alarm_copy (alarm);
		e_cal_component_add_alarm (comp, alarm_copy);
		e_cal_component_alarm_free (alarm_copy);
	}

	while (alarm = i_cal_component_get_first_component (component, I_CAL_VALARM_COMPONENT), alarm) {
		i_cal_component_remove_component (component, alarm);
		g_object_unref (alarm);
	}

	changed_comp = e_cal_component_get_icalcomponent (comp);
	if (changed_comp) {
		/* Move all VALARM components into the right 'component' */
		while (alarm = i_cal_component_get_first_component (changed_comp, I_CAL_VALARM_COMPONENT), alarm) {
			i_cal_component_remove_component (changed_comp, alarm);
			i_cal_component_add_component (component, alarm);
			g_object_unref (alarm);
		}
	} else {
		g_warn_if_reached ();
	}

	g_clear_object (&comp);

	return TRUE;
}

static void
ecep_reminders_select_page_cb (GtkAction *action,
			       ECompEditorPage *page)
{
	g_return_if_fail (E_IS_COMP_EDITOR_PAGE_REMINDERS (page));

	e_comp_editor_page_select (page);
}

static void
ecep_reminders_setup_ui (ECompEditorPageReminders *page_reminders)
{
	const gchar *ui =
		"<ui>"
		"  <menubar action='main-menu'>"
		"    <menu action='options-menu'>"
		"      <placeholder name='tabs'>"
		"        <menuitem action='page-reminders'/>"
		"      </placeholder>"
		"    </menu>"
		"  </menubar>"
		"  <toolbar name='main-toolbar'>"
		"    <placeholder name='content'>\n"
		"      <toolitem action='page-reminders'/>\n"
		"    </placeholder>"
		"  </toolbar>"
		"</ui>";

	const GtkActionEntry options_actions[] = {
		{ "page-reminders",
		  "appointment-soon",
		  N_("_Reminders"),
		  NULL,
		  N_("Set or unset reminders"),
		  G_CALLBACK (ecep_reminders_select_page_cb) }
	};

	ECompEditor *comp_editor;
	GtkUIManager *ui_manager;
	GtkActionGroup *action_group;
	GtkAction *action;
	GError *error = NULL;

	g_return_if_fail (E_IS_COMP_EDITOR_PAGE_REMINDERS (page_reminders));

	comp_editor = e_comp_editor_page_ref_editor (E_COMP_EDITOR_PAGE (page_reminders));
	ui_manager = e_comp_editor_get_ui_manager (comp_editor);
	action_group = e_comp_editor_get_action_group (comp_editor, "individual");

	gtk_action_group_add_actions (action_group,
		options_actions, G_N_ELEMENTS (options_actions), page_reminders);

	gtk_ui_manager_add_ui_from_string (ui_manager, ui, -1, &error);

	action = gtk_action_group_get_action (action_group, "page-reminders");
	if (action) {
		e_binding_bind_property (
			page_reminders, "visible",
			action, "visible",
			G_BINDING_SYNC_CREATE);
	}

	g_clear_object (&comp_editor);

	if (error) {
		g_critical ("%s: %s", G_STRFUNC, error->message);
		g_error_free (error);
	}
}

static void
ecep_reminders_constructed (GObject *object)
{
	ECompEditorPageReminders *page_reminders;
	GtkWidget *widget, *container, *label, *option_grid;
	GtkComboBoxText *text_combo;
	GtkTreeViewColumn *column;
	GtkTextBuffer *text_buffer;
	GtkCellRenderer *cell_renderer;
	PangoAttrList *bold;
	GtkGrid *grid;
	ECompEditor *comp_editor;
	EFocusTracker *focus_tracker;
	gchar *combo_label, *config_dir;

	G_OBJECT_CLASS (e_comp_editor_page_reminders_parent_class)->constructed (object);

	page_reminders = E_COMP_EDITOR_PAGE_REMINDERS (object);
	grid = GTK_GRID (page_reminders);
	comp_editor = e_comp_editor_page_ref_editor (E_COMP_EDITOR_PAGE (page_reminders));
	focus_tracker = e_comp_editor_get_focus_tracker (comp_editor);

	bold = pango_attr_list_new ();
	pango_attr_list_insert (bold, pango_attr_weight_new (PANGO_WEIGHT_BOLD));

	widget = gtk_label_new (_("Reminders"));
	g_object_set (G_OBJECT (widget),
		"hexpand", FALSE,
		"halign", GTK_ALIGN_START,
		"vexpand", FALSE,
		"valign", GTK_ALIGN_START,
		"attributes", bold,
		NULL);
	gtk_widget_show (widget);
	gtk_grid_attach (grid, widget, 0, 0, 2, 1);

	widget = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 4);
	g_object_set (G_OBJECT (widget),
		"hexpand", TRUE,
		"halign", GTK_ALIGN_FILL,
		"vexpand", FALSE,
		"valign", GTK_ALIGN_START,
		"margin-start", 12,
		"margin-bottom", 6,
		NULL);
	gtk_widget_show (widget);
	gtk_grid_attach (grid, widget, 0, 1, 2, 1);

	container = widget;

	widget = gtk_label_new_with_mnemonic (_("_Reminder"));
	g_object_set (G_OBJECT (widget),
		"hexpand", FALSE,
		"halign", GTK_ALIGN_START,
		"vexpand", FALSE,
		"valign", GTK_ALIGN_CENTER,
		NULL);
	gtk_widget_show (widget);
	gtk_box_pack_start (GTK_BOX (container), widget, FALSE, FALSE, 0);
	label = widget;

	widget = gtk_combo_box_text_new ();
	g_object_set (G_OBJECT (widget),
		"hexpand", FALSE,
		"halign", GTK_ALIGN_START,
		"vexpand", FALSE,
		"valign", GTK_ALIGN_CENTER,
		NULL);
	gtk_widget_show (widget);
	gtk_box_pack_start (GTK_BOX (container), widget, FALSE, FALSE, 0);
	page_reminders->priv->alarms_combo = widget;

	gtk_label_set_mnemonic_widget (GTK_LABEL (label), page_reminders->priv->alarms_combo);

	/* Add the user defined time if necessary */
	page_reminders->priv->alarm_interval = calendar_config_get_default_reminder_interval ();
	page_reminders->priv->alarm_units = calendar_config_get_default_reminder_units ();

	combo_label = NULL;

	switch (page_reminders->priv->alarm_units) {
	case E_DURATION_DAYS:
		if (page_reminders->priv->alarm_interval != 1) {
			combo_label = g_strdup_printf (ngettext ("%d day before", "%d days before",
				page_reminders->priv->alarm_interval), page_reminders->priv->alarm_interval);
		}
		break;

	case E_DURATION_HOURS:
		if (page_reminders->priv->alarm_interval != 1) {
			combo_label = g_strdup_printf (ngettext ("%d hour before", "%d hours before",
				page_reminders->priv->alarm_interval), page_reminders->priv->alarm_interval);
		}
		break;

	case E_DURATION_MINUTES:
		if (page_reminders->priv->alarm_interval != 15) {
			combo_label = g_strdup_printf (ngettext ("%d minute before", "%d minutes before",
				page_reminders->priv->alarm_interval), page_reminders->priv->alarm_interval);
		}
		break;
	}

	text_combo = GTK_COMBO_BOX_TEXT (widget);
	/* Translators: "None" for "No reminder set" */
	gtk_combo_box_text_append_text (text_combo, C_("cal-reminders", "None"));
        /* Translators: Predefined reminder's description */
	gtk_combo_box_text_append_text (text_combo, C_("cal-reminders", "15 minutes before"));
        /* Translators: Predefined reminder's description */
	gtk_combo_box_text_append_text (text_combo, C_("cal-reminders", "1 hour before"));
        /* Translators: Predefined reminder's description */
	gtk_combo_box_text_append_text (text_combo, C_("cal-reminders", "1 day before"));

	if (combo_label) {
		gtk_combo_box_text_append_text (text_combo, combo_label);
		g_free (combo_label);

		page_reminders->priv->alarm_map = alarm_map_with_user_time;
	} else {
		page_reminders->priv->alarm_map = alarm_map_without_user_time;
	}

	/* Translators: "Custom" for "Custom reminder set" */
	gtk_combo_box_text_append_text (text_combo, C_("cal-reminders", "Custom"));

	gtk_combo_box_set_active (GTK_COMBO_BOX (text_combo), 0);

	g_signal_connect (page_reminders->priv->alarms_combo, "changed",
		G_CALLBACK (ecep_reminders_alarms_combo_changed_cb), page_reminders);

	widget = gtk_scrolled_window_new (NULL, NULL);
	g_object_set (G_OBJECT (widget),
		"hexpand", TRUE,
		"halign", GTK_ALIGN_FILL,
		"vexpand", FALSE,
		"valign", GTK_ALIGN_FILL,
		"margin-start", 12,
		"margin-bottom", 6,
		"hscrollbar-policy", GTK_POLICY_AUTOMATIC,
		"vscrollbar-policy", GTK_POLICY_AUTOMATIC,
		"shadow-type", GTK_SHADOW_IN,
		NULL);
	gtk_widget_show (widget);
	gtk_grid_attach (grid, widget, 0, 2, 1, 1);

	page_reminders->priv->alarms_scrolled_window = widget;
	container = widget;

	page_reminders->priv->alarm_list = e_alarm_list_new ();

	widget = gtk_tree_view_new ();
	g_object_set (G_OBJECT (widget),
		"hexpand", TRUE,
		"halign", GTK_ALIGN_FILL,
		"vexpand", TRUE,
		"valign", GTK_ALIGN_FILL,
		"model", page_reminders->priv->alarm_list,
		"headers-visible", FALSE,
		NULL);
	gtk_widget_show (widget);

	gtk_container_add (GTK_CONTAINER (container), widget);
	page_reminders->priv->alarms_tree_view = widget;

	column = gtk_tree_view_column_new ();
	gtk_tree_view_column_set_title (column, "Action/Trigger");
	cell_renderer = gtk_cell_renderer_text_new ();
	gtk_tree_view_column_pack_start (column, cell_renderer, TRUE);
	gtk_tree_view_column_add_attribute (column, cell_renderer, "text", E_ALARM_LIST_COLUMN_DESCRIPTION);
	gtk_tree_view_append_column (GTK_TREE_VIEW (page_reminders->priv->alarms_tree_view), column);

	g_signal_connect (gtk_tree_view_get_selection (GTK_TREE_VIEW (page_reminders->priv->alarms_tree_view)),
		"changed", G_CALLBACK (ecep_reminders_alarms_selection_changed_cb), page_reminders);

	widget = gtk_button_box_new (GTK_ORIENTATION_VERTICAL);
	g_object_set (G_OBJECT (widget),
		"hexpand", FALSE,
		"halign", GTK_ALIGN_START,
		"vexpand", FALSE,
		"valign", GTK_ALIGN_START,
		NULL);
	gtk_widget_show (widget);
	gtk_grid_attach (grid, widget, 1, 2, 1, 1);
	page_reminders->priv->alarms_button_box = widget;

	widget = gtk_button_new_with_mnemonic (_("A_dd"));
	gtk_box_pack_start (GTK_BOX (page_reminders->priv->alarms_button_box), widget, FALSE, FALSE, 0);
	gtk_widget_show (widget);
	page_reminders->priv->alarms_add_button = widget;

	g_signal_connect (page_reminders->priv->alarms_add_button, "clicked",
		G_CALLBACK (ecep_reminders_alarms_add_clicked_cb), page_reminders);

	widget = gtk_button_new_with_mnemonic (_("Re_move"));
	gtk_box_pack_start (GTK_BOX (page_reminders->priv->alarms_button_box), widget, FALSE, FALSE, 0);
	gtk_widget_show (widget);
	page_reminders->priv->alarms_remove_button = widget;

	g_signal_connect (page_reminders->priv->alarms_remove_button, "clicked",
		G_CALLBACK (ecep_reminders_alarms_remove_clicked_cb), page_reminders);

	widget = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 4);
	g_object_set (G_OBJECT (widget),
		"hexpand", TRUE,
		"halign", GTK_ALIGN_FILL,
		"vexpand", FALSE,
		"valign", GTK_ALIGN_START,
		"margin-start", 12,
		"margin-bottom", 6,
		NULL);
	gtk_widget_show (widget);
	gtk_grid_attach (grid, widget, 0, 3, 2, 1);

	page_reminders->priv->alarm_setup_hbox = widget;
	container = widget;

	widget = gtk_combo_box_new ();
	g_object_set (G_OBJECT (widget),
		"hexpand", FALSE,
		"halign", GTK_ALIGN_START,
		"vexpand", FALSE,
		"valign", GTK_ALIGN_CENTER,
		NULL);
	gtk_widget_show (widget);
	gtk_box_pack_start (GTK_BOX (container), widget, FALSE, FALSE, 0);
	page_reminders->priv->kind_combo = widget;

	ecep_reminders_init_sensitable_combo_box (GTK_COMBO_BOX (widget),
		/* Translators: Part of: [ Pop up an alert ] [ x ] [ minute(s) ] [ before ] [ start ]*/
		C_("cal-reminders", "Pop up an alert"),
		/* Translators: Part of: [ Play a sound ] [ x ] [ minute(s) ] [ before ] [ start ]*/
		C_("cal-reminders", "Play a sound"),
		/* Translators: Part of: [ Run a program ] [ x ] [ minute(s) ] [ before ] [ start ]*/
		C_("cal-reminders", "Run a program"),
		/* Translators: Part of: [ Send an email ] [ x ] [ minute(s) ] [ before ] [ start ]*/
		C_("cal-reminders", "Send an email"),
		NULL);

	g_signal_connect (page_reminders->priv->kind_combo, "changed",
		G_CALLBACK (ecep_reminders_kind_combo_changed_cb), page_reminders);

	widget = gtk_spin_button_new_with_range (0, 999, 1);
	g_object_set (G_OBJECT (widget),
		"hexpand", FALSE,
		"halign", GTK_ALIGN_START,
		"vexpand", FALSE,
		"valign", GTK_ALIGN_CENTER,
		"digits", 0,
		NULL);
	gtk_widget_show (widget);
	gtk_box_pack_start (GTK_BOX (container), widget, FALSE, FALSE, 0);
	page_reminders->priv->time_spin = widget;

	widget = gtk_combo_box_text_new ();
	g_object_set (G_OBJECT (widget),
		"hexpand", FALSE,
		"halign", GTK_ALIGN_START,
		"vexpand", FALSE,
		"valign", GTK_ALIGN_CENTER,
		NULL);
	gtk_widget_show (widget);
	gtk_box_pack_start (GTK_BOX (container), widget, FALSE, FALSE, 0);
	page_reminders->priv->unit_combo = widget;

	text_combo = GTK_COMBO_BOX_TEXT (widget);
	/* Translators: Part of: [ Pop up an alert ] [ x ] [ minute(s) ] [ before ] [ start ]*/
	gtk_combo_box_text_append_text (text_combo, C_("cal-reminders", "minute(s)"));
	/* Translators: Part of: [ Pop up an alert ] [ x ] [ hour(s) ] [ before ] [ start ]*/
	gtk_combo_box_text_append_text (text_combo, C_("cal-reminders", "hour(s)"));
	/* Translators: Part of: [ Pop up an alert ] [ x ] [ day(s) ] [ before ] [ start ]*/
	gtk_combo_box_text_append_text (text_combo, C_("cal-reminders", "day(s)"));

	gtk_combo_box_set_active (GTK_COMBO_BOX (text_combo), 0);

	widget = gtk_combo_box_new ();
	g_object_set (G_OBJECT (widget),
		"hexpand", FALSE,
		"halign", GTK_ALIGN_START,
		"vexpand", FALSE,
		"valign", GTK_ALIGN_CENTER,
		NULL);
	gtk_widget_show (widget);
	gtk_box_pack_start (GTK_BOX (container), widget, FALSE, FALSE, 0);
	page_reminders->priv->relative_time_combo = widget;

	ecep_reminders_init_sensitable_combo_box (GTK_COMBO_BOX (widget),
		/* Translators: Part of: [ Pop up an alert ] [ x ] [ minute(s) ] [ before ] [ start ]*/
		C_("cal-reminders", "before"),
		/* Translators: Part of: [ Pop up an alert ] [ x ] [ minute(s) ] [ after ] [ start ]*/
		C_("cal-reminders", "after"),
		NULL);

	widget = gtk_combo_box_new ();
	g_object_set (G_OBJECT (widget),
		"hexpand", FALSE,
		"halign", GTK_ALIGN_START,
		"vexpand", FALSE,
		"valign", GTK_ALIGN_CENTER,
		NULL);
	gtk_widget_show (widget);
	gtk_box_pack_start (GTK_BOX (container), widget, FALSE, FALSE, 0);
	page_reminders->priv->relative_to_combo = widget;

	ecep_reminders_init_sensitable_combo_box (GTK_COMBO_BOX (widget),
		/* Translators: Part of: [ Pop up an alert ] [ x ] [ minute(s) ] [ before ] [ start ]*/
		C_("cal-reminders", "start"),
		/* Translators: Part of: [ Pop up an alert ] [ x ] [ minute(s) ] [ before ] [ end ]*/
		C_("cal-reminders", "end"),
		NULL);

	widget = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 4);
	g_object_set (G_OBJECT (widget),
		"hexpand", TRUE,
		"halign", GTK_ALIGN_FILL,
		"vexpand", FALSE,
		"valign", GTK_ALIGN_START,
		"margin-start", 12,
		"margin-bottom", 6,
		NULL);
	gtk_widget_show (widget);
	gtk_grid_attach (grid, widget, 0, 4, 2, 1);

	page_reminders->priv->repeat_setup_hbox = widget;
	container = widget;

	/* Translators: Part of: Repeat the reminder [ x ] extra times every [ y ] [ minutes ] */
	widget = gtk_check_button_new_with_mnemonic (_("Re_peat the reminder"));
	g_object_set (G_OBJECT (widget),
		"hexpand", FALSE,
		"halign", GTK_ALIGN_START,
		"vexpand", FALSE,
		"valign", GTK_ALIGN_CENTER,
		NULL);
	gtk_widget_show (widget);
	gtk_box_pack_start (GTK_BOX (container), widget, FALSE, FALSE, 0);

	page_reminders->priv->repeat_check = widget;

	widget = gtk_spin_button_new_with_range (1, 999, 1);
	g_object_set (G_OBJECT (widget),
		"hexpand", FALSE,
		"halign", GTK_ALIGN_START,
		"vexpand", FALSE,
		"valign", GTK_ALIGN_CENTER,
		"digits", 0,
		NULL);
	gtk_widget_show (widget);
	gtk_box_pack_start (GTK_BOX (container), widget, FALSE, FALSE, 0);
	page_reminders->priv->repeat_times_spin = widget;

	/* Translators: Part of: Repeat the reminder [ x ] extra times every [ y ] [ minutes ] */
	widget = gtk_label_new (C_("cal-reminders", "extra times every"));
	g_object_set (G_OBJECT (widget),
		"hexpand", FALSE,
		"halign", GTK_ALIGN_START,
		"vexpand", FALSE,
		"valign", GTK_ALIGN_CENTER,
		NULL);
	gtk_widget_show (widget);
	gtk_box_pack_start (GTK_BOX (container), widget, FALSE, FALSE, 0);

	page_reminders->priv->repeat_every_label = widget;

	widget = gtk_spin_button_new_with_range (1, 999, 1);
	g_object_set (G_OBJECT (widget),
		"hexpand", FALSE,
		"halign", GTK_ALIGN_START,
		"vexpand", FALSE,
		"valign", GTK_ALIGN_CENTER,
		"digits", 0,
		NULL);
	gtk_widget_show (widget);
	gtk_box_pack_start (GTK_BOX (container), widget, FALSE, FALSE, 0);
	page_reminders->priv->repeat_every_spin = widget;

	widget = gtk_combo_box_text_new ();
	g_object_set (G_OBJECT (widget),
		"hexpand", FALSE,
		"halign", GTK_ALIGN_START,
		"vexpand", FALSE,
		"valign", GTK_ALIGN_CENTER,
		NULL);
	gtk_widget_show (widget);
	gtk_box_pack_start (GTK_BOX (container), widget, FALSE, FALSE, 0);
	page_reminders->priv->repeat_unit_combo = widget;

	text_combo = GTK_COMBO_BOX_TEXT (widget);
	/* Translators: Part of: Repeat the reminder [ x ] extra times every [ y ] [ minutes ] */
	gtk_combo_box_text_append_text (text_combo, C_("cal-reminders", "minutes"));
	/* Translators: Part of: Repeat the reminder [ x ] extra times every [ y ] [ hours ] */
	gtk_combo_box_text_append_text (text_combo, C_("cal-reminders", "hours"));
	/* Translators: Part of: Repeat the reminder [ x ] extra times every [ y ] [ days ] */
	gtk_combo_box_text_append_text (text_combo, C_("cal-reminders", "days"));

	gtk_combo_box_set_active (GTK_COMBO_BOX (text_combo), 0);

	widget = gtk_label_new (_("Options"));
	g_object_set (G_OBJECT (widget),
		"hexpand", FALSE,
		"halign", GTK_ALIGN_START,
		"vexpand", FALSE,
		"valign", GTK_ALIGN_START,
		"attributes", bold,
		NULL);
	gtk_widget_show (widget);
	gtk_grid_attach (grid, widget, 0, 5, 2, 1);

	page_reminders->priv->options_label = widget;

	widget = gtk_notebook_new ();
	g_object_set (G_OBJECT (widget),
		"hexpand", TRUE,
		"halign", GTK_ALIGN_FILL,
		"vexpand", TRUE,
		"valign", GTK_ALIGN_FILL,
		"margin-start", 12,
		"show-tabs", FALSE,
		"show-border", FALSE,
		NULL);
	gtk_widget_show (widget);
	gtk_grid_attach (grid, widget, 0, 6, 2, 1);

	page_reminders->priv->options_notebook = widget;

	/* Custom message page */

	option_grid = gtk_grid_new ();
	gtk_widget_show (option_grid);

	widget = gtk_check_button_new_with_mnemonic (C_("cal-reminders", "Custom _message"));
	g_object_set (G_OBJECT (widget),
		"hexpand", TRUE,
		"halign", GTK_ALIGN_FILL,
		"vexpand", FALSE,
		"valign", GTK_ALIGN_START,
		NULL);
	gtk_widget_show (widget);
	gtk_grid_attach (GTK_GRID (option_grid), widget, 0, 0, 1, 1);
	page_reminders->priv->custom_message_check = widget;

	widget = gtk_scrolled_window_new (NULL, NULL);
	g_object_set (G_OBJECT (widget),
		"hexpand", TRUE,
		"halign", GTK_ALIGN_FILL,
		"vexpand", TRUE,
		"valign", GTK_ALIGN_FILL,
		"hscrollbar-policy", GTK_POLICY_AUTOMATIC,
		"vscrollbar-policy", GTK_POLICY_AUTOMATIC,
		"shadow-type", GTK_SHADOW_IN,
		NULL);
	gtk_widget_show (widget);
	gtk_grid_attach (GTK_GRID (option_grid), widget, 0, 1, 1, 1);

	container = widget;

	widget = gtk_text_view_new ();
	g_object_set (G_OBJECT (widget),
		"hexpand", TRUE,
		"halign", GTK_ALIGN_FILL,
		"vexpand", TRUE,
		"valign", GTK_ALIGN_FILL,
		NULL);
	gtk_widget_show (widget);
	gtk_container_add (GTK_CONTAINER (container), widget);
	page_reminders->priv->custom_message_text_view = widget;

	gtk_notebook_append_page (GTK_NOTEBOOK (page_reminders->priv->options_notebook), option_grid, NULL);

	/* Custom sound page */

	option_grid = gtk_grid_new ();
	gtk_widget_show (option_grid);

	widget = gtk_check_button_new_with_mnemonic (C_("cal-reminders", "Custom reminder _sound"));
	g_object_set (G_OBJECT (widget),
		"hexpand", TRUE,
		"halign", GTK_ALIGN_FILL,
		"vexpand", FALSE,
		"valign", GTK_ALIGN_START,
		NULL);
	gtk_widget_show (widget);
	gtk_grid_attach (GTK_GRID (option_grid), widget, 0, 0, 1, 1);
	page_reminders->priv->custom_sound_check = widget;

	widget = gtk_file_chooser_button_new (_("Select a sound file"), GTK_FILE_CHOOSER_ACTION_OPEN);
	g_object_set (G_OBJECT (widget),
		"hexpand", TRUE,
		"halign", GTK_ALIGN_FILL,
		"vexpand", FALSE,
		"valign", GTK_ALIGN_START,
		NULL);
	gtk_widget_show (widget);
	gtk_grid_attach (GTK_GRID (option_grid), widget, 0, 1, 1, 1);

	config_dir = calendar_config_get_dir_path ();
	if (config_dir && *config_dir)
		gtk_file_chooser_set_current_folder (GTK_FILE_CHOOSER (widget), config_dir);
	g_free (config_dir);

	page_reminders->priv->custom_sound_chooser = widget;

	gtk_notebook_append_page (GTK_NOTEBOOK (page_reminders->priv->options_notebook), option_grid, NULL);

	/* Custom program page */

	option_grid = gtk_grid_new ();
	g_object_set (G_OBJECT (option_grid),
		"column-spacing", 4,
		"row-spacing", 4,
		NULL);
	gtk_widget_show (option_grid);

	widget = gtk_label_new_with_mnemonic (_("_Program:"));
	g_object_set (G_OBJECT (widget),
		"hexpand", FALSE,
		"halign", GTK_ALIGN_END,
		"vexpand", FALSE,
		"valign", GTK_ALIGN_CENTER,
		NULL);
	gtk_widget_show (widget);
	gtk_grid_attach (GTK_GRID (option_grid), widget, 0, 0, 1, 1);

	label = widget;

	widget = gtk_entry_new ();
	g_object_set (G_OBJECT (widget),
		"hexpand", TRUE,
		"halign", GTK_ALIGN_FILL,
		"vexpand", FALSE,
		"valign", GTK_ALIGN_CENTER,
		NULL);
	gtk_widget_show (widget);
	gtk_grid_attach (GTK_GRID (option_grid), widget, 1, 0, 1, 1);
	page_reminders->priv->custom_app_path_entry = widget;

	gtk_label_set_mnemonic_widget (GTK_LABEL (label), widget);

	widget = gtk_label_new_with_mnemonic (_("_Arguments:"));
	g_object_set (G_OBJECT (widget),
		"hexpand", FALSE,
		"halign", GTK_ALIGN_END,
		"vexpand", FALSE,
		"valign", GTK_ALIGN_CENTER,
		NULL);
	gtk_widget_show (widget);
	gtk_grid_attach (GTK_GRID (option_grid), widget, 0, 1, 1, 1);

	label = widget;

	widget = gtk_entry_new ();
	g_object_set (G_OBJECT (widget),
		"hexpand", TRUE,
		"halign", GTK_ALIGN_FILL,
		"vexpand", FALSE,
		"valign", GTK_ALIGN_CENTER,
		NULL);
	gtk_widget_show (widget);
	gtk_grid_attach (GTK_GRID (option_grid), widget, 1, 1, 1, 1);
	page_reminders->priv->custom_app_args_entry = widget;

	gtk_label_set_mnemonic_widget (GTK_LABEL (label), widget);

	gtk_notebook_append_page (GTK_NOTEBOOK (page_reminders->priv->options_notebook), option_grid, NULL);

	/* Custom email page */

	option_grid = gtk_grid_new ();
	gtk_widget_show (option_grid);

	widget = gtk_button_new_with_mnemonic (_("_Send To:"));
	g_object_set (G_OBJECT (widget),
		"hexpand", FALSE,
		"halign", GTK_ALIGN_START,
		"vexpand", FALSE,
		"valign", GTK_ALIGN_CENTER,
		NULL);
	gtk_widget_show (widget);
	gtk_grid_attach (GTK_GRID (option_grid), widget, 0, 0, 1, 1);

	page_reminders->priv->custom_email_button = widget;

	g_signal_connect (page_reminders->priv->custom_email_button, "clicked",
		G_CALLBACK (ecep_reminders_send_to_clicked_cb), page_reminders);

	/* page_reminders->priv->custom_email_entry is initialized on demand */

	widget = gtk_check_button_new_with_mnemonic (C_("cal-reminders", "Custom _message"));
	g_object_set (G_OBJECT (widget),
		"hexpand", TRUE,
		"halign", GTK_ALIGN_FILL,
		"vexpand", FALSE,
		"valign", GTK_ALIGN_START,
		NULL);
	gtk_widget_show (widget);
	gtk_grid_attach (GTK_GRID (option_grid), widget, 0, 1, 2, 1);
	page_reminders->priv->custom_email_message_check = widget;

	widget = gtk_scrolled_window_new (NULL, NULL);
	g_object_set (G_OBJECT (widget),
		"hexpand", TRUE,
		"halign", GTK_ALIGN_FILL,
		"vexpand", TRUE,
		"valign", GTK_ALIGN_FILL,
		"hscrollbar-policy", GTK_POLICY_AUTOMATIC,
		"vscrollbar-policy", GTK_POLICY_AUTOMATIC,
		"shadow-type", GTK_SHADOW_IN,
		NULL);
	gtk_widget_show (widget);
	gtk_grid_attach (GTK_GRID (option_grid), widget, 0, 2, 2, 1);

	container = widget;

	widget = gtk_text_view_new ();
	g_object_set (G_OBJECT (widget),
		"hexpand", TRUE,
		"halign", GTK_ALIGN_FILL,
		"vexpand", TRUE,
		"valign", GTK_ALIGN_FILL,
		NULL);
	gtk_widget_show (widget);
	gtk_container_add (GTK_CONTAINER (container), widget);
	page_reminders->priv->custom_email_message_text_view = widget;

	gtk_notebook_append_page (GTK_NOTEBOOK (page_reminders->priv->options_notebook), option_grid, NULL);

	pango_attr_list_unref (bold);

	e_widget_undo_attach (page_reminders->priv->custom_message_text_view, focus_tracker);
	e_widget_undo_attach (page_reminders->priv->custom_email_message_text_view, focus_tracker);

	e_spell_text_view_attach (GTK_TEXT_VIEW (page_reminders->priv->custom_message_text_view));
	e_spell_text_view_attach (GTK_TEXT_VIEW (page_reminders->priv->custom_email_message_text_view));

	g_clear_object (&comp_editor);

	g_signal_connect_swapped (page_reminders->priv->repeat_check, "toggled",
		G_CALLBACK (ecep_reminders_sanitize_option_widgets), page_reminders);
	g_signal_connect_swapped (page_reminders->priv->custom_message_check, "toggled",
		G_CALLBACK (ecep_reminders_sanitize_option_widgets), page_reminders);
	g_signal_connect_swapped (page_reminders->priv->custom_sound_check, "toggled",
		G_CALLBACK (ecep_reminders_sanitize_option_widgets), page_reminders);
	g_signal_connect_swapped (page_reminders->priv->custom_email_message_check, "toggled",
		G_CALLBACK (ecep_reminders_sanitize_option_widgets), page_reminders);

	g_signal_connect_swapped (page_reminders->priv->kind_combo, "changed",
		G_CALLBACK (ecep_reminders_widgets_to_selected), page_reminders);
	g_signal_connect_swapped (page_reminders->priv->time_spin, "value-changed",
		G_CALLBACK (ecep_reminders_widgets_to_selected), page_reminders);
	g_signal_connect_swapped (page_reminders->priv->unit_combo, "changed",
		G_CALLBACK (ecep_reminders_widgets_to_selected), page_reminders);
	g_signal_connect_swapped (page_reminders->priv->relative_time_combo, "changed",
		G_CALLBACK (ecep_reminders_widgets_to_selected), page_reminders);
	g_signal_connect_swapped (page_reminders->priv->relative_to_combo, "changed",
		G_CALLBACK (ecep_reminders_widgets_to_selected), page_reminders);
	g_signal_connect_swapped (page_reminders->priv->repeat_check, "toggled",
		G_CALLBACK (ecep_reminders_widgets_to_selected), page_reminders);
	g_signal_connect_swapped (page_reminders->priv->repeat_times_spin, "value-changed",
		G_CALLBACK (ecep_reminders_widgets_to_selected), page_reminders);
	g_signal_connect_swapped (page_reminders->priv->repeat_every_spin, "value-changed",
		G_CALLBACK (ecep_reminders_widgets_to_selected), page_reminders);
	g_signal_connect_swapped (page_reminders->priv->repeat_unit_combo, "changed",
		G_CALLBACK (ecep_reminders_widgets_to_selected), page_reminders);
	g_signal_connect_swapped (page_reminders->priv->custom_message_check, "toggled",
		G_CALLBACK (ecep_reminders_widgets_to_selected), page_reminders);
	text_buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (page_reminders->priv->custom_message_text_view));
	g_signal_connect_swapped (text_buffer, "changed",
		G_CALLBACK (ecep_reminders_widgets_to_selected), page_reminders);
	g_signal_connect_swapped (page_reminders->priv->custom_sound_check, "toggled",
		G_CALLBACK (ecep_reminders_widgets_to_selected), page_reminders);
	g_signal_connect_swapped (page_reminders->priv->custom_sound_chooser, "file-set",
		G_CALLBACK (ecep_reminders_widgets_to_selected), page_reminders);
	g_signal_connect_swapped (page_reminders->priv->custom_app_path_entry, "changed",
		G_CALLBACK (ecep_reminders_widgets_to_selected), page_reminders);
	g_signal_connect_swapped (page_reminders->priv->custom_app_args_entry, "changed",
		G_CALLBACK (ecep_reminders_widgets_to_selected), page_reminders);
	g_signal_connect_swapped (page_reminders->priv->custom_email_message_check, "toggled",
		G_CALLBACK (ecep_reminders_widgets_to_selected), page_reminders);
	text_buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (page_reminders->priv->custom_email_message_text_view));
	g_signal_connect_swapped (text_buffer, "changed",
		G_CALLBACK (ecep_reminders_widgets_to_selected), page_reminders);

	ecep_reminders_setup_ui (page_reminders);
}

static void
ecep_reminders_dispose (GObject *object)
{
	ECompEditorPageReminders *page_reminders;

	page_reminders = E_COMP_EDITOR_PAGE_REMINDERS (object);

	if (page_reminders->priv->name_selector)
		e_name_selector_cancel_loading (page_reminders->priv->name_selector);

	g_clear_object (&page_reminders->priv->alarm_list);
	g_clear_object (&page_reminders->priv->name_selector);

	G_OBJECT_CLASS (e_comp_editor_page_reminders_parent_class)->dispose (object);
}

static void
e_comp_editor_page_reminders_init (ECompEditorPageReminders *page_reminders)
{
	page_reminders->priv = G_TYPE_INSTANCE_GET_PRIVATE (page_reminders,
		E_TYPE_COMP_EDITOR_PAGE_REMINDERS,
		ECompEditorPageRemindersPrivate);
}

static void
e_comp_editor_page_reminders_class_init (ECompEditorPageRemindersClass *klass)
{
	ECompEditorPageClass *page_class;
	GObjectClass *object_class;

	g_type_class_add_private (klass, sizeof (ECompEditorPageRemindersPrivate));

	page_class = E_COMP_EDITOR_PAGE_CLASS (klass);
	page_class->sensitize_widgets = ecep_reminders_sensitize_widgets;
	page_class->fill_widgets = ecep_reminders_fill_widgets;
	page_class->fill_component = ecep_reminders_fill_component;

	object_class = G_OBJECT_CLASS (klass);
	object_class->constructed = ecep_reminders_constructed;
	object_class->dispose = ecep_reminders_dispose;
}

ECompEditorPage *
e_comp_editor_page_reminders_new (ECompEditor *editor)
{
	g_return_val_if_fail (E_IS_COMP_EDITOR (editor), NULL);

	return g_object_new (E_TYPE_COMP_EDITOR_PAGE_REMINDERS,
		"editor", editor,
		NULL);
}
