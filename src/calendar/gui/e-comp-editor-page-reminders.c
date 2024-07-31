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
#include "comp-util.h"
#include "e-alarm-list.h"
#include "itip-utils.h"

#include "e-comp-editor-page-reminders.h"

#define SECTION_NAME			_("Send To")
#define X_EVOLUTION_NEEDS_DESCRIPTION	"X-EVOLUTION-NEEDS-DESCRIPTION"

#define N_PREDEFINED_ALARMS		3
#define N_MAX_PREDEFINED_USER_ALARMS	10
/* The 3 = 1 for the default alarm + 1 for None + 1 for Custom */
#define N_MAX_PREDEFINED_ALARMS		((N_PREDEFINED_ALARMS) + (N_MAX_PREDEFINED_USER_ALARMS) + 3)

/* Items below the "Custom" value, which is the separator, "add predefined" and "remove predefined" */
#define N_BOTTOM_ITEMS			3

enum {
	CUSTOM_ALARM_VALUE		= -2,
	ADD_PREDEFINED_TIME_VALUE	= -3,
	REMOVE_PREDEFINED_TIMES_VALUE	= -4
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

	GtkWidget *add_custom_time_popover;
	GtkWidget *add_custom_time_days_spin;
	GtkWidget *add_custom_time_hours_spin;
	GtkWidget *add_custom_time_minutes_spin;
	GtkWidget *add_custom_time_add_button;

	EAlarmList *alarm_list;
	/* Interval in minutes. */
	gint predefined_alarms[N_MAX_PREDEFINED_ALARMS];

	/* Addressbook name selector, created on demand */
	ENameSelector *name_selector;

	gint last_selected_alarms_combo_index;
	gboolean any_custom_reminder_set;
};

G_DEFINE_TYPE_WITH_PRIVATE (ECompEditorPageReminders, e_comp_editor_page_reminders, E_TYPE_COMP_EDITOR_PAGE)

static gint
ecep_reminders_get_alarm_index (ECompEditorPageReminders *page_reminders)
{
	GtkComboBox *combo_box;
	GtkTreeModel *model;
	gint alarm_index, n_children, n_bottom_items;

	combo_box = GTK_COMBO_BOX (page_reminders->priv->alarms_combo);

	g_return_val_if_fail (GTK_IS_COMBO_BOX (combo_box), -1);

	alarm_index = gtk_combo_box_get_active (combo_box);
	if (alarm_index == -1)
		return alarm_index;

	model = gtk_combo_box_get_model (combo_box);
	if (!model)
		return -1;

	n_children = gtk_tree_model_iter_n_children (model, NULL);
	n_bottom_items = N_BOTTOM_ITEMS - (page_reminders->priv->any_custom_reminder_set ? 0 : 1);

	/* The Custom alarm is always the last time item */
	if (alarm_index == n_children - n_bottom_items - 1)
		alarm_index = CUSTOM_ALARM_VALUE;
	else if (alarm_index == n_children - n_bottom_items - 1 + 1) /* separator */
		alarm_index = -1;
	else if (alarm_index == n_children - n_bottom_items - 1 + 2)
		alarm_index = ADD_PREDEFINED_TIME_VALUE;
	else if (page_reminders->priv->any_custom_reminder_set && alarm_index == n_children - n_bottom_items - 1 + 3)
		alarm_index = REMOVE_PREDEFINED_TIMES_VALUE;

	return alarm_index;
}

static void
ecep_reminders_sanitize_option_widgets (ECompEditorPageReminders *page_reminders)
{
	gboolean any_selected;
	gboolean is_custom;
	gboolean sensitive;
	gboolean can_only_one = FALSE;
	gint n_defined;

	g_return_if_fail (E_IS_COMP_EDITOR_PAGE_REMINDERS (page_reminders));

	any_selected = gtk_tree_selection_count_selected_rows (gtk_tree_view_get_selection (
		GTK_TREE_VIEW (page_reminders->priv->alarms_tree_view))) > 0;
	is_custom = ecep_reminders_get_alarm_index (page_reminders) == CUSTOM_ALARM_VALUE;
	n_defined = gtk_tree_model_iter_n_children (gtk_tree_view_get_model (
		GTK_TREE_VIEW (page_reminders->priv->alarms_tree_view)), NULL);

	if (n_defined >= 1) {
		ECompEditor *comp_editor;

		comp_editor = e_comp_editor_page_ref_editor (E_COMP_EDITOR_PAGE (page_reminders));

		if (comp_editor) {
			ECalClient *target_client;

			target_client = e_comp_editor_get_target_client (comp_editor);

			if (target_client)
				can_only_one = e_cal_client_check_one_alarm_only (target_client);

			g_object_unref (comp_editor);
		}
	}

	gtk_widget_set_sensitive (page_reminders->priv->alarms_tree_view, is_custom);
	gtk_widget_set_sensitive (page_reminders->priv->alarms_add_button, n_defined < 1 || !can_only_one);
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

static gboolean
ecep_reminders_description_differs (ECompEditorPageReminders *page_reminders,
				    const ECalComponentText *description)
{
	ECompEditor *editor;
	gboolean differ = TRUE;

	editor = e_comp_editor_page_ref_editor (E_COMP_EDITOR_PAGE (page_reminders));

	if (editor) {
		ICalComponent *icomp;

		icomp = e_comp_editor_get_component (editor);
		differ = description && g_strcmp0 (e_cal_component_text_get_value (description), i_cal_component_get_summary (icomp)) != 0;

		g_clear_object (&editor);
	}

	return differ;
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
	gint duration_minutes = 0;

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
	if (!duration || i_cal_duration_is_neg (duration))
		e_dialog_combo_box_set (page_reminders->priv->relative_time_combo, BEFORE, relative_map);
	else
		e_dialog_combo_box_set (page_reminders->priv->relative_time_combo, AFTER, relative_map);

	if (duration) {
		duration_minutes = i_cal_duration_as_int (duration) / 60;

		if (duration_minutes < 0)
			duration_minutes *= -1;
	}

	if (duration_minutes && !(duration_minutes % (24 * 60))) {
		e_dialog_combo_box_set (page_reminders->priv->unit_combo, DAYS, value_map);
		gtk_spin_button_set_value (GTK_SPIN_BUTTON (page_reminders->priv->time_spin), duration_minutes / (24 * 60));
	} else if (duration_minutes && !(duration_minutes % 60)) {
		e_dialog_combo_box_set (page_reminders->priv->unit_combo, HOURS, value_map);
		gtk_spin_button_set_value (GTK_SPIN_BUTTON (page_reminders->priv->time_spin), duration_minutes / 60);
	} else if (duration_minutes) {
		e_dialog_combo_box_set (page_reminders->priv->unit_combo, MINUTES, value_map);
		gtk_spin_button_set_value (GTK_SPIN_BUTTON (page_reminders->priv->time_spin), duration_minutes);
	} else {
		e_dialog_combo_box_set (page_reminders->priv->unit_combo, MINUTES, value_map);
		gtk_spin_button_set_value (GTK_SPIN_BUTTON (page_reminders->priv->time_spin), 0);
	}

	/* Repeat options */
	repeat = e_cal_component_alarm_get_repeat (alarm);

	if (repeat && e_cal_component_alarm_repeat_get_repetitions (repeat)) {
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
		ECalComponentText *description;
		gboolean differ;

		description = e_cal_component_alarm_get_description (alarm);
		differ = ecep_reminders_description_differs (page_reminders, description);

		if (differ && description && e_cal_component_text_get_value (description) && e_cal_component_text_get_value (description)[0]) {
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
		gboolean differ;

		/* Attendees */
		name_selector_model = e_name_selector_peek_model (page_reminders->priv->name_selector);
		e_name_selector_model_peek_section (name_selector_model, SECTION_NAME, NULL, &destination_store);

		attendees = e_cal_component_alarm_get_attendees (alarm);
		for (link = attendees; link; link = g_slist_next (link)) {
			ECalComponentAttendee *att = link->data;
			EDestination *dest;
			const gchar *att_email;

			dest = e_destination_new ();

			if (att && e_cal_component_attendee_get_cn (att) && e_cal_component_attendee_get_cn (att)[0])
				e_destination_set_name (dest, e_cal_component_attendee_get_cn (att));

			att_email = e_cal_util_get_attendee_email (att);
			if (att_email)
				e_destination_set_email (dest, att_email);

			e_destination_store_append_destination (destination_store, dest);

			g_object_unref (dest);
		}

		/* Description */
		description = e_cal_component_alarm_get_description (alarm);
		differ = ecep_reminders_description_differs (page_reminders, description);

		if (differ && description && e_cal_component_text_get_value (description) && e_cal_component_text_get_value (description)[0]) {
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
			} else {
				ecep_reminders_add_needs_description_property (alarm);
			}

			g_free (text);
		} else {
			ecep_reminders_add_needs_description_property (alarm);
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
			} else {
				ecep_reminders_add_needs_description_property (alarm);
			}

			g_free (text);
		} else {
			ecep_reminders_add_needs_description_property (alarm);
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

	if (gtk_tree_selection_get_selected (selection, NULL, NULL))
		ecep_reminders_selected_to_widgets (page_reminders);

	ecep_reminders_sanitize_option_widgets (page_reminders);
}

static gint
ecep_reminders_interval_to_int (gint days,
				gint hours,
				gint minutes)
{
	return (days * 24 * 60) + (hours * 60) + minutes;
}

static void
ecep_reminders_int_to_interval (gint value,
				gint *out_days,
				gint *out_hours,
				gint *out_minutes)
{
	*out_days = value / (24 * 60);
	*out_hours = (value / 60) % 24;
	*out_minutes = value % 60;
}

static void ecep_reminders_add_custom_time_clicked (ECompEditorPageReminders *page_reminders);
static void ecep_reminders_remove_custom_times_clicked (ECompEditorPageReminders *page_reminders);

static void
ecep_reminders_alarms_combo_changed_cb (GtkComboBox *combo_box,
					ECompEditorPageReminders *page_reminders)
{
	ECalComponentAlarm *alarm;
	ICalDuration *duration;
	gint alarm_index;

	g_return_if_fail (E_IS_COMP_EDITOR_PAGE_REMINDERS (page_reminders));

	if (!e_comp_editor_page_get_updating (E_COMP_EDITOR_PAGE (page_reminders)))
		e_comp_editor_page_emit_changed (E_COMP_EDITOR_PAGE (page_reminders));

	alarm_index = ecep_reminders_get_alarm_index (page_reminders);
	if (alarm_index == -1 || alarm_index == 0) {
		page_reminders->priv->last_selected_alarms_combo_index = 0;
		e_alarm_list_clear (page_reminders->priv->alarm_list);

		ecep_reminders_sanitize_option_widgets (page_reminders);
		return;
	}

	if (alarm_index == ADD_PREDEFINED_TIME_VALUE || alarm_index == REMOVE_PREDEFINED_TIMES_VALUE) {
		g_signal_handlers_block_by_func (page_reminders->priv->alarms_combo, ecep_reminders_alarms_combo_changed_cb, page_reminders);
		gtk_combo_box_set_active (GTK_COMBO_BOX (page_reminders->priv->alarms_combo), page_reminders->priv->last_selected_alarms_combo_index);
		g_signal_handlers_unblock_by_func (page_reminders->priv->alarms_combo, ecep_reminders_alarms_combo_changed_cb, page_reminders);

		if (alarm_index == ADD_PREDEFINED_TIME_VALUE)
			ecep_reminders_add_custom_time_clicked (page_reminders);
		else
			ecep_reminders_remove_custom_times_clicked (page_reminders);

		return;
	}

	if (alarm_index == CUSTOM_ALARM_VALUE) {
		GtkTreeSelection *selection;
		GtkTreeModel *model;

		model = gtk_combo_box_get_model (combo_box);

		if (model) {
			page_reminders->priv->last_selected_alarms_combo_index =
				gtk_tree_model_iter_n_children (model, NULL) - N_BOTTOM_ITEMS - 1 +
				(page_reminders->priv->any_custom_reminder_set ? 0 : 1);
		}

		selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (page_reminders->priv->alarms_tree_view));

		if (!gtk_tree_selection_get_selected (selection, NULL, NULL)) {
			GtkTreeIter iter;

			if (gtk_tree_model_get_iter_first (GTK_TREE_MODEL (page_reminders->priv->alarm_list), &iter))
				gtk_tree_selection_select_iter (selection, &iter);
		}

		ecep_reminders_sanitize_option_widgets (page_reminders);
		return;
	}

	page_reminders->priv->last_selected_alarms_combo_index = alarm_index;

	e_alarm_list_clear (page_reminders->priv->alarm_list);

	alarm = e_cal_component_alarm_new ();

	e_cal_component_alarm_set_action (alarm, E_CAL_COMPONENT_ALARM_DISPLAY);

	duration = i_cal_duration_new_null_duration ();

	i_cal_duration_set_is_neg (duration, TRUE);

	if (alarm_index >= 1 && alarm_index < N_MAX_PREDEFINED_ALARMS) {
		gint ii;

		for (ii = 0; ii < alarm_index - 1 && page_reminders->priv->predefined_alarms[ii] != -1; ii++) {
		}

		g_warn_if_fail (ii == alarm_index - 1);

		if (ii == alarm_index - 1) {
			gint days = 0, hours = 0, minutes = 0;

			ecep_reminders_int_to_interval (page_reminders->priv->predefined_alarms[alarm_index - 1], &days, &hours, &minutes);

			i_cal_duration_set_days (duration, days);
			i_cal_duration_set_hours (duration, hours);
			i_cal_duration_set_minutes (duration, minutes);
		}
	}

	e_cal_component_alarm_take_trigger (alarm,
		e_cal_component_alarm_trigger_new_relative (E_CAL_COMPONENT_ALARM_TRIGGER_RELATIVE_START, duration));
	ecep_reminders_add_needs_description_property (alarm);
	e_alarm_list_append (page_reminders->priv->alarm_list, NULL, alarm);
	e_cal_component_alarm_free (alarm);
	g_object_unref (duration);

	ecep_reminders_sanitize_option_widgets (page_reminders);
}

static void
ecep_reminders_alarms_add_clicked_cb (GtkButton *button,
				      ECompEditorPageReminders *page_reminders)
{
	GtkComboBox *combo_box;
	GtkTreeSelection *selection;
	GtkTreeIter iter;
	ECalComponentAlarm *alarm;
	ICalDuration *duration;

	g_return_if_fail (E_IS_COMP_EDITOR_PAGE_REMINDERS (page_reminders));

	combo_box = GTK_COMBO_BOX (page_reminders->priv->alarms_combo);

	/* Ensure the reminder is always set to Custom */
	if (ecep_reminders_get_alarm_index (page_reminders) != CUSTOM_ALARM_VALUE) {
		GtkTreeModel *model;

		model = gtk_combo_box_get_model (combo_box);

		if (model)
			gtk_combo_box_set_active (combo_box, gtk_tree_model_iter_n_children (model, NULL) - N_BOTTOM_ITEMS - 1 +
				(page_reminders->priv->any_custom_reminder_set ? 0 : 1));
	}

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
ecep_reminders_alarm_description_differs (ECalComponentAlarm *ca,
					  const gchar *old_summary)
{
	if (!ecep_reminders_has_needs_description_property (ca)) {
		ECalComponentText *desc;

		desc = e_cal_component_alarm_get_description (ca);
		if (!desc || !e_cal_component_text_get_value (desc) ||
		    !old_summary || strcmp (e_cal_component_text_get_value (desc), old_summary))
			return TRUE;

		ecep_reminders_add_needs_description_property (ca);
	}

	return FALSE;
}

static gboolean
ecep_reminders_is_custom_alarm (ECompEditorPageReminders *page_reminders,
				ECalComponentAlarm *ca,
				const gchar *old_summary,
				gint *alarm_index)
{
	ECalComponentAlarmTrigger *trigger;
	ECalComponentAlarmRepeat *repeat;
	ECalComponentAlarmAction action;
	ICalDuration *duration;
	GSList *attachments;
	gint ii, value;

	action = e_cal_component_alarm_get_action (ca);
	if (action != E_CAL_COMPONENT_ALARM_DISPLAY)
		return TRUE;

	attachments = e_cal_component_alarm_get_attachments (ca);
	if (attachments)
		return TRUE;

	if (ecep_reminders_alarm_description_differs (ca, old_summary))
		return TRUE;

	repeat = e_cal_component_alarm_get_repeat (ca);
	if (repeat && e_cal_component_alarm_repeat_get_repetitions (repeat) != 0)
		return TRUE;

	if (e_cal_component_alarm_has_attendees (ca))
		return TRUE;

	trigger = e_cal_component_alarm_get_trigger (ca);
	if (!trigger || e_cal_component_alarm_trigger_get_kind (trigger) != E_CAL_COMPONENT_ALARM_TRIGGER_RELATIVE_START)
		return TRUE;

	duration = e_cal_component_alarm_trigger_get_duration (trigger);
	if (!duration || (!i_cal_duration_is_neg (duration) && i_cal_duration_as_int (duration) != 0))
		return TRUE;

	if (i_cal_duration_get_seconds (duration) != 0)
		return TRUE;

	value = i_cal_duration_as_int (duration) / 60;

	if (value < 0)
		value *= -1;

	for (ii = 0; ii < N_MAX_PREDEFINED_ALARMS && page_reminders->priv->predefined_alarms[ii] != -1; ii++) {
		if (value == page_reminders->priv->predefined_alarms[ii]) {
			if (alarm_index)
				*alarm_index = ii + 1;

			return FALSE;
		}
	}

	return TRUE;
}

static gboolean
ecep_reminders_is_custom_alarm_uid_list (ECompEditorPageReminders *page_reminders,
					 ECalComponent *comp,
					 GSList *alarm_uids,
					 const gchar *old_summary,
					 gint *alarm_index)
{
	ECalComponentAlarm *ca;
	gboolean result;

	if (!alarm_uids)
		return FALSE;

	if (alarm_uids->next)
		return TRUE;

	ca = e_cal_component_get_alarm (comp, alarm_uids->data);
	result = ecep_reminders_is_custom_alarm (page_reminders, ca, old_summary, alarm_index);
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
		gtk_combo_box_set_active (GTK_COMBO_BOX (page_reminders->priv->alarms_combo), 0);
		return;
	}

	g_object_unref (valarm);

	comp = e_cal_component_new_from_icalcomponent (i_cal_component_clone (component));
	if (comp && e_cal_component_has_alarms (comp)) {
		GSList *alarms, *link;
		const gchar *summary;
		gint alarm_index = 0;

		summary = i_cal_component_get_summary (component);
		alarms = e_cal_component_get_alarm_uids (comp);

		if (ecep_reminders_is_custom_alarm_uid_list (page_reminders, comp, alarms, summary, &alarm_index)) {
			GtkTreeModel *model;

			model = gtk_combo_box_get_model (GTK_COMBO_BOX (page_reminders->priv->alarms_combo));
			alarm_index = gtk_tree_model_iter_n_children (model, NULL) - N_BOTTOM_ITEMS - 1 +
				(page_reminders->priv->any_custom_reminder_set ? 0 : 1);
		}

		if (alarm_index < 0)
			alarm_index = 0;

		gtk_combo_box_set_active (GTK_COMBO_BOX (page_reminders->priv->alarms_combo), alarm_index);

		e_alarm_list_clear (page_reminders->priv->alarm_list);

		for (link = alarms; link; link = g_slist_next (link)) {
			ECalComponentAlarm *ca;
			const gchar *uid = link->data;

			ca = e_cal_component_get_alarm (comp, uid);
			ecep_reminders_alarm_description_differs (ca, summary);
			e_alarm_list_append (page_reminders->priv->alarm_list, NULL, ca);
			e_cal_component_alarm_free (ca);
		}

		g_slist_free_full (alarms, g_free);

		if (ecep_reminders_get_alarm_index (page_reminders) == CUSTOM_ALARM_VALUE) {
			GtkTreeSelection *selection;
			GtkTreeIter iter;

			selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (page_reminders->priv->alarms_tree_view));
			if (gtk_tree_model_get_iter_first (GTK_TREE_MODEL (page_reminders->priv->alarm_list), &iter))
				gtk_tree_selection_select_iter (selection, &iter);
		}
	} else {
		gtk_combo_box_set_active (GTK_COMBO_BOX (page_reminders->priv->alarms_combo), 0);
	}

	g_clear_object (&comp);
}

static gboolean
ecep_reminders_fill_component (ECompEditorPage *page,
			       ICalComponent *component)
{
	ECompEditorPageReminders *page_reminders;
	ECalComponent *comp;
	ICalComponent *changed_comp, *alarm_comp;
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

		alarm = (ECalComponentAlarm *) e_alarm_list_get_alarm (page_reminders->priv->alarm_list, &iter);
		if (!alarm) {
			g_warning ("alarm is NULL\n");
			continue;
		}

		/* We clone the alarm to maintain the invariant that the alarm
		 * structures in the list did *not* come from the component.
		 */

		alarm_copy = e_cal_component_alarm_copy (alarm);

		/* We set the description of the alarm if it's got
		 * the X-EVOLUTION-NEEDS-DESCRIPTION property.
		 */
		if (ecep_reminders_remove_needs_description_property (alarm_copy)) {
			ECalComponentText *summary;

			summary = e_cal_component_get_summary (comp);
			e_cal_component_alarm_take_description (alarm_copy, summary);
		}

		action = e_cal_component_alarm_get_action (alarm_copy);

		if (action == E_CAL_COMPONENT_ALARM_EMAIL) {
			ECalComponentText *summary;

			summary = e_cal_component_get_summary (comp);
			e_cal_component_alarm_take_summary (alarm_copy, summary);
		} else {
			e_cal_component_alarm_set_summary (alarm_copy, NULL);
		}

		if (action == E_CAL_COMPONENT_ALARM_EMAIL || action == E_CAL_COMPONENT_ALARM_DISPLAY) {
			if (!e_cal_component_alarm_get_description (alarm_copy)) {
				const gchar *description;

				description = i_cal_component_get_description (e_cal_component_get_icalcomponent (comp));
				if (!description || !*description)
					description = i_cal_component_get_summary (e_cal_component_get_icalcomponent (comp));

				if (description && *description)
					e_cal_component_alarm_take_description (alarm_copy, e_cal_component_text_new (description, NULL));
				else
					e_cal_component_alarm_set_description (alarm_copy, NULL);
			}
		} else {
			e_cal_component_alarm_set_description (alarm_copy, NULL);
		}

		e_cal_component_add_alarm (comp, alarm_copy);
		e_cal_component_alarm_free (alarm_copy);
	}

	while (alarm_comp = i_cal_component_get_first_component (component, I_CAL_VALARM_COMPONENT), alarm_comp) {
		i_cal_component_remove_component (component, alarm_comp);
		g_object_unref (alarm_comp);
	}

	changed_comp = e_cal_component_get_icalcomponent (comp);
	if (changed_comp) {
		/* Move all VALARM components into the right 'component' */
		while (alarm_comp = i_cal_component_get_first_component (changed_comp, I_CAL_VALARM_COMPONENT), alarm_comp) {
			i_cal_component_remove_component (changed_comp, alarm_comp);
			i_cal_component_add_component (component, alarm_comp);
			g_object_unref (alarm_comp);
		}
	} else {
		g_warn_if_reached ();
	}

	g_clear_object (&comp);

	return TRUE;
}

static void
ecep_reminders_select_page_cb (EUIAction *action,
			       GVariant *parameter,
			       gpointer user_data)
{
	ECompEditorPage *page = user_data;

	g_return_if_fail (E_IS_COMP_EDITOR_PAGE_REMINDERS (page));

	e_comp_editor_page_select (page);
}

static void
ecep_reminders_setup_ui (ECompEditorPageReminders *page_reminders)
{
	static const gchar *eui =
		"<eui>"
		  "<menu id='main-menu'>"
		    "<submenu action='options-menu'>"
		      "<placeholder id='tabs'>"
			"<item action='page-reminders'/>"
		      "</placeholder>"
		    "</submenu>"
		  "</menu>"
		"</eui>";

	static const EUIActionEntry options_actions[] = {
		{ "page-reminders",
		  "appointment-soon",
		  N_("_Reminders"),
		  NULL,
		  N_("Set or unset reminders"),
		  ecep_reminders_select_page_cb, NULL, NULL, NULL }
	};

	ECompEditor *comp_editor;
	EUIManager *ui_manager;
	EUIAction *action;

	g_return_if_fail (E_IS_COMP_EDITOR_PAGE_REMINDERS (page_reminders));

	comp_editor = e_comp_editor_page_ref_editor (E_COMP_EDITOR_PAGE (page_reminders));
	ui_manager = e_comp_editor_get_ui_manager (comp_editor);

	e_ui_manager_add_actions_with_eui_data (ui_manager, "individual", GETTEXT_PACKAGE,
		options_actions, G_N_ELEMENTS (options_actions), page_reminders, eui);

	action = e_comp_editor_get_action (comp_editor, "page-reminders");
	if (action) {
		e_binding_bind_property (
			page_reminders, "visible",
			action, "visible",
			G_BINDING_SYNC_CREATE);
	}

	g_clear_object (&comp_editor);
}

static gboolean
ecep_reminders_add_predefined_alarm (ECompEditorPageReminders *page_reminders,
				     gint value_minutes)
{
	gint ii;

	g_return_val_if_fail (E_IS_COMP_EDITOR_PAGE_REMINDERS (page_reminders), FALSE);
	g_return_val_if_fail (value_minutes >= 0, FALSE);

	for (ii = 0; ii < N_MAX_PREDEFINED_ALARMS && page_reminders->priv->predefined_alarms[ii] != -1; ii++) {
		if (value_minutes == page_reminders->priv->predefined_alarms[ii])
			return FALSE;
	}

	if (ii < N_MAX_PREDEFINED_ALARMS) {
		page_reminders->priv->predefined_alarms[ii] = value_minutes;

		if (ii + 1 < N_MAX_PREDEFINED_ALARMS)
			page_reminders->priv->predefined_alarms[ii + 1] = -1;
	}

	return ii < N_MAX_PREDEFINED_ALARMS;
}

static gint
ecep_reminders_compare_predefined_alarm (gconstpointer data1,
					 gconstpointer data2,
					 gpointer user_data)
{
	gint value1 = * (gint *) data1;
	gint value2 = * (gint *) data2;

	return value1 - value2;
}

static void
ecep_reminders_sort_predefined_alarms (ECompEditorPageReminders *page_reminders)
{
	gint nelems;

	g_return_if_fail (E_IS_COMP_EDITOR_PAGE_REMINDERS (page_reminders));

	for (nelems = N_PREDEFINED_ALARMS; nelems < N_MAX_PREDEFINED_ALARMS && page_reminders->priv->predefined_alarms[nelems] != -1; nelems++) {
		/* Just count those filled */
	}

	g_qsort_with_data (page_reminders->priv->predefined_alarms, nelems,
		sizeof (gint), ecep_reminders_compare_predefined_alarm, NULL);
}

static gboolean
ecep_reminders_fill_alarms_combo (ECompEditorPageReminders *page_reminders,
				  gint select_minutes)
{
	GtkComboBoxText *text_combo;
	gint ii, select_index = 0;
	gboolean did_select = FALSE;

	g_return_val_if_fail (E_IS_COMP_EDITOR_PAGE_REMINDERS (page_reminders), FALSE);
	g_return_val_if_fail (GTK_IS_COMBO_BOX_TEXT (page_reminders->priv->alarms_combo), FALSE);

	ecep_reminders_sort_predefined_alarms (page_reminders);

	text_combo = GTK_COMBO_BOX_TEXT (page_reminders->priv->alarms_combo);

	g_signal_handlers_block_by_func (text_combo, ecep_reminders_alarms_combo_changed_cb, page_reminders);

	if (select_minutes < 0)
		select_index = gtk_combo_box_get_active (GTK_COMBO_BOX (text_combo));

	gtk_combo_box_text_remove_all (text_combo);

	/* Translators: "None" for "No reminder set" */
	gtk_combo_box_text_append_text (text_combo, C_("cal-reminders", "None"));

	for (ii = 0; ii < N_MAX_PREDEFINED_ALARMS && page_reminders->priv->predefined_alarms[ii] != -1; ii++) {
		gchar *text, *merged;

		if (page_reminders->priv->predefined_alarms[ii]) {
			text = e_cal_util_seconds_to_string (page_reminders->priv->predefined_alarms[ii] * 60);
			/* Translators: This constructs predefined reminder's description, for example "15 minutes before",
			   "1 hour before", "1 day before", but, if user has set, also more complicated strings like
			   "2 days 13 hours 1 minute before". */
			merged = g_strdup_printf (C_("cal-reminders", "%s before"), text);
			gtk_combo_box_text_append_text (text_combo, merged);
			g_free (merged);
			g_free (text);
		} else {
			gtk_combo_box_text_append_text (text_combo, C_("cal-reminders", "at the start"));
		}

		if (select_minutes >= 0 && select_minutes == page_reminders->priv->predefined_alarms[ii])
			select_index = ii + 1;
	}

	/* Translators: "Custom" for "Custom reminder set" */
	gtk_combo_box_text_append_text (text_combo, C_("cal-reminders", "Custom"));
	gtk_combo_box_text_append_text (text_combo, "-");
	gtk_combo_box_text_append_text (text_combo, _("Add predefined time…"));

	if (page_reminders->priv->any_custom_reminder_set)
		gtk_combo_box_text_append_text (text_combo, _("Remove predefined times"));

	g_signal_handlers_unblock_by_func (text_combo, ecep_reminders_alarms_combo_changed_cb, page_reminders);

	if (select_index >= 0 && select_index <= ii) {
		gtk_combo_box_set_active (GTK_COMBO_BOX (text_combo), select_index);
		did_select = select_minutes >= 0;
	} else {
		gtk_combo_box_set_active (GTK_COMBO_BOX (text_combo), 0);
	}

	return did_select;
}

static void
ecep_reminders_init_predefined_alarms (ECompEditorPageReminders *page_reminders)
{
	EDurationType alarm_units;
	gint alarm_interval, minutes;

	g_return_if_fail (E_IS_COMP_EDITOR_PAGE_REMINDERS (page_reminders));

	page_reminders->priv->predefined_alarms[0] = ecep_reminders_interval_to_int (0, 0, 15);
	page_reminders->priv->predefined_alarms[1] = ecep_reminders_interval_to_int (0, 1, 0);
	page_reminders->priv->predefined_alarms[2] = ecep_reminders_interval_to_int (1, 0, 0);
	page_reminders->priv->predefined_alarms[3] = -1;

	alarm_interval = calendar_config_get_default_reminder_interval ();
	alarm_units = calendar_config_get_default_reminder_units ();

	minutes = ecep_reminders_interval_to_int (
		alarm_units == E_DURATION_DAYS ? alarm_interval : 0,
		alarm_units == E_DURATION_HOURS ? alarm_interval : 0,
		alarm_units == E_DURATION_MINUTES ? alarm_interval : 0);

	ecep_reminders_add_predefined_alarm (page_reminders, minutes);
}

static void
ecep_reminders_add_custom_time_add_button_clicked_cb (GtkButton *button,
						      gpointer user_data)
{
	ECompEditorPageReminders *page_reminders = user_data;
	gboolean found = FALSE;
	gint new_minutes, ii;

	g_return_if_fail (E_IS_COMP_EDITOR_PAGE_REMINDERS (page_reminders));

	new_minutes = ecep_reminders_interval_to_int (
		gtk_spin_button_get_value_as_int (GTK_SPIN_BUTTON (page_reminders->priv->add_custom_time_days_spin)),
		gtk_spin_button_get_value_as_int (GTK_SPIN_BUTTON (page_reminders->priv->add_custom_time_hours_spin)),
		gtk_spin_button_get_value_as_int (GTK_SPIN_BUTTON (page_reminders->priv->add_custom_time_minutes_spin)));
	g_return_if_fail (new_minutes >= 0);

	gtk_widget_hide (page_reminders->priv->add_custom_time_popover);

	for (ii = 0; ii < N_MAX_PREDEFINED_ALARMS && page_reminders->priv->predefined_alarms[ii] != -1; ii++) {
		if (new_minutes == page_reminders->priv->predefined_alarms[ii]) {
			found = TRUE;
			gtk_combo_box_set_active (GTK_COMBO_BOX (page_reminders->priv->alarms_combo), ii + 1);
			break;
		}
	}

	if (!found) {
		GSettings *settings;
		GVariant *variant;
		gboolean any_user_alarm_added = FALSE;
		gint32 array[N_MAX_PREDEFINED_USER_ALARMS + 1] = { 0 }, narray = 0;

		settings = e_util_ref_settings ("org.gnome.evolution.calendar");
		variant = g_settings_get_value (settings, "custom-reminders-minutes");
		if (variant) {
			const gint32 *stored;
			gsize nstored = 0;

			stored = g_variant_get_fixed_array (variant, &nstored, sizeof (gint32));
			if (stored && nstored > 0) {
				/* Skip the oldest, when too many stored */
				for (ii = nstored >= N_MAX_PREDEFINED_USER_ALARMS ? 1 : 0; ii < N_MAX_PREDEFINED_USER_ALARMS && ii < nstored; ii++) {
					array[narray] = stored[ii];
					narray++;
				}
			}

			g_variant_unref (variant);
		}

		/* Add the new at the end of the array */
		array[narray] = new_minutes;
		narray++;

		variant = g_variant_new_fixed_array (G_VARIANT_TYPE_INT32, array, narray, sizeof (gint32));
		g_settings_set_value (settings, "custom-reminders-minutes", variant);

		g_object_unref (settings);

		ecep_reminders_init_predefined_alarms (page_reminders);

		for (ii = 0; ii < narray; ii++) {
			if (ecep_reminders_add_predefined_alarm (page_reminders, array[ii]))
				any_user_alarm_added = TRUE;
		}

		page_reminders->priv->any_custom_reminder_set = any_user_alarm_added;

		if (!ecep_reminders_fill_alarms_combo (page_reminders, new_minutes))
			gtk_combo_box_set_active (GTK_COMBO_BOX (page_reminders->priv->alarms_combo), 0);
	}
}

static void
ecep_reminders_add_custom_time_clicked (ECompEditorPageReminders *page_reminders)
{
	g_return_if_fail (E_IS_COMP_EDITOR_PAGE_REMINDERS (page_reminders));

	if (!page_reminders->priv->add_custom_time_popover) {
		GtkWidget *widget;
		GtkBox *vbox, *box;

		page_reminders->priv->add_custom_time_days_spin = gtk_spin_button_new_with_range (0.0, 366.0, 1.0);
		page_reminders->priv->add_custom_time_hours_spin = gtk_spin_button_new_with_range (0.0, 23.0, 1.0);
		page_reminders->priv->add_custom_time_minutes_spin = gtk_spin_button_new_with_range (0.0, 59.0, 1.0);

		g_object_set (G_OBJECT (page_reminders->priv->add_custom_time_days_spin),
			"digits", 0,
			"numeric", TRUE,
			"snap-to-ticks", TRUE,
			NULL);

		g_object_set (G_OBJECT (page_reminders->priv->add_custom_time_hours_spin),
			"digits", 0,
			"numeric", TRUE,
			"snap-to-ticks", TRUE,
			NULL);

		g_object_set (G_OBJECT (page_reminders->priv->add_custom_time_minutes_spin),
			"digits", 0,
			"numeric", TRUE,
			"snap-to-ticks", TRUE,
			NULL);

		vbox = GTK_BOX (gtk_box_new (GTK_ORIENTATION_VERTICAL, 2));

		widget = gtk_label_new (_("Set a custom predefined time to"));
		gtk_box_pack_start (vbox, widget, FALSE, FALSE, 0);

		box = GTK_BOX (gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 2));
		g_object_set (G_OBJECT (box),
			"halign", GTK_ALIGN_START,
			"hexpand", FALSE,
			"valign", GTK_ALIGN_CENTER,
			"vexpand", FALSE,
			NULL);

		gtk_box_pack_start (box, page_reminders->priv->add_custom_time_days_spin, FALSE, FALSE, 4);
		/* Translators: this is part of: "Set a custom predefined time to [nnn] days [nnn] hours [nnn] minutes", where the text in "[]" means a separate widget */
		widget = gtk_label_new_with_mnemonic (C_("cal-reminders", "da_ys"));
		gtk_label_set_mnemonic_widget (GTK_LABEL (widget), page_reminders->priv->add_custom_time_days_spin);
		gtk_box_pack_start (box, widget, FALSE, FALSE, 4);

		gtk_box_pack_start (vbox, GTK_WIDGET (box), FALSE, FALSE, 0);

		box = GTK_BOX (gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 2));
		g_object_set (G_OBJECT (box),
			"halign", GTK_ALIGN_START,
			"hexpand", FALSE,
			"valign", GTK_ALIGN_CENTER,
			"vexpand", FALSE,
			NULL);

		gtk_box_pack_start (box, page_reminders->priv->add_custom_time_hours_spin, FALSE, FALSE, 4);
		/* Translators: this is part of: "Set a custom predefined time to [nnn] days [nnn] hours [nnn] minutes", where the text in "[]" means a separate widget */
		widget = gtk_label_new_with_mnemonic (C_("cal-reminders", "_hours"));
		gtk_label_set_mnemonic_widget (GTK_LABEL (widget), page_reminders->priv->add_custom_time_hours_spin);
		gtk_box_pack_start (box, widget, FALSE, FALSE, 4);

		gtk_box_pack_start (vbox, GTK_WIDGET (box), FALSE, FALSE, 0);

		box = GTK_BOX (gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 2));
		g_object_set (G_OBJECT (box),
			"halign", GTK_ALIGN_START,
			"hexpand", FALSE,
			"valign", GTK_ALIGN_CENTER,
			"vexpand", FALSE,
			NULL);

		gtk_box_pack_start (box, page_reminders->priv->add_custom_time_minutes_spin, FALSE, FALSE, 4);
		/* Translators: this is part of: "Set a custom predefined time to [nnn] days [nnn] hours [nnn] minutes", where the text in "[]" means a separate widget */
		widget = gtk_label_new_with_mnemonic (C_("cal-reminders", "_minutes"));
		gtk_label_set_mnemonic_widget (GTK_LABEL (widget), page_reminders->priv->add_custom_time_minutes_spin);
		gtk_box_pack_start (box, widget, FALSE, FALSE, 4);

		gtk_box_pack_start (vbox, GTK_WIDGET (box), FALSE, FALSE, 0);

		page_reminders->priv->add_custom_time_add_button = gtk_button_new_with_mnemonic (_("_Add time"));
		g_object_set (G_OBJECT (page_reminders->priv->add_custom_time_add_button),
			"halign", GTK_ALIGN_CENTER,
			NULL);

		gtk_box_pack_start (vbox, page_reminders->priv->add_custom_time_add_button, FALSE, FALSE, 0);

		gtk_widget_show_all (GTK_WIDGET (vbox));

		page_reminders->priv->add_custom_time_popover = gtk_popover_new (GTK_WIDGET (page_reminders));
		gtk_popover_set_position (GTK_POPOVER (page_reminders->priv->add_custom_time_popover), GTK_POS_BOTTOM);
		gtk_container_add (GTK_CONTAINER (page_reminders->priv->add_custom_time_popover), GTK_WIDGET (vbox));
		gtk_container_set_border_width (GTK_CONTAINER (page_reminders->priv->add_custom_time_popover), 6);

		g_signal_connect (page_reminders->priv->add_custom_time_add_button, "clicked",
			G_CALLBACK (ecep_reminders_add_custom_time_add_button_clicked_cb), page_reminders);
	}

	gtk_widget_hide (page_reminders->priv->add_custom_time_popover);
	gtk_spin_button_set_value (GTK_SPIN_BUTTON (page_reminders->priv->add_custom_time_days_spin), 0.0);
	gtk_spin_button_set_value (GTK_SPIN_BUTTON (page_reminders->priv->add_custom_time_hours_spin), 0.0);
	gtk_spin_button_set_value (GTK_SPIN_BUTTON (page_reminders->priv->add_custom_time_minutes_spin), 0.0);
	gtk_popover_set_relative_to (GTK_POPOVER (page_reminders->priv->add_custom_time_popover), page_reminders->priv->alarms_combo);
	gtk_widget_show (page_reminders->priv->add_custom_time_popover);

	gtk_widget_grab_focus (page_reminders->priv->add_custom_time_days_spin);
}

static void
ecep_reminders_remove_custom_times_clicked (ECompEditorPageReminders *page_reminders)
{
	GSettings *settings;

	g_return_if_fail (E_IS_COMP_EDITOR_PAGE_REMINDERS (page_reminders));

	settings = e_util_ref_settings ("org.gnome.evolution.calendar");
	g_settings_reset (settings, "custom-reminders-minutes");
	g_object_unref (settings);

	ecep_reminders_init_predefined_alarms (page_reminders);

	page_reminders->priv->any_custom_reminder_set = FALSE;

	ecep_reminders_fill_alarms_combo (page_reminders, -1);

	gtk_combo_box_set_active (GTK_COMBO_BOX (page_reminders->priv->alarms_combo), 0);
}

static gboolean
ecep_reminders_alarms_combo_separator_cb (GtkTreeModel *model,
					  GtkTreeIter *iter,
					  gpointer user_data)
{
	gchar *text = NULL;
	gboolean is_separator;

	if (!model || !iter)
		return FALSE;

	gtk_tree_model_get (model, iter, 0, &text, -1);

	is_separator = g_strcmp0 (text, "-") == 0;

	g_free (text);

	return is_separator;
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
	gint ii;
	gchar *config_dir;
	GSettings *settings;
	GVariant *variant;

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

	gtk_combo_box_set_row_separator_func (GTK_COMBO_BOX (widget),
		ecep_reminders_alarms_combo_separator_cb, NULL, NULL);

	gtk_label_set_mnemonic_widget (GTK_LABEL (label), page_reminders->priv->alarms_combo);

	ecep_reminders_init_predefined_alarms (page_reminders);

	settings = e_util_ref_settings ("org.gnome.evolution.calendar");
	variant = g_settings_get_value (settings, "custom-reminders-minutes");

	if (variant) {
		const gint32 *stored;
		gsize nstored = 0;

		stored = g_variant_get_fixed_array (variant, &nstored, sizeof (gint32));
		if (stored && nstored > 0) {
			if (nstored > N_MAX_PREDEFINED_USER_ALARMS)
				nstored = N_MAX_PREDEFINED_USER_ALARMS;

			for (ii = 0; ii < nstored; ii++) {
				if (stored[ii] >= 0 &&
				    ecep_reminders_add_predefined_alarm (page_reminders, stored[ii])) {
					page_reminders->priv->any_custom_reminder_set = TRUE;
				}
			}
		}

		g_variant_unref (variant);
	}

	g_object_unref (settings);

	ecep_reminders_fill_alarms_combo (page_reminders, -1);

	gtk_combo_box_set_active (GTK_COMBO_BOX (page_reminders->priv->alarms_combo), 0);

	g_signal_connect (page_reminders->priv->alarms_combo, "changed",
		G_CALLBACK (ecep_reminders_alarms_combo_changed_cb), page_reminders);

	widget = gtk_scrolled_window_new (NULL, NULL);
	g_object_set (G_OBJECT (widget),
		"hexpand", TRUE,
		"halign", GTK_ALIGN_FILL,
		"vexpand", FALSE,
		"valign", GTK_ALIGN_FILL,
		"height-request", 100,
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
		"height-request", 100,
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
	page_reminders->priv = e_comp_editor_page_reminders_get_instance_private (page_reminders);
}

static void
e_comp_editor_page_reminders_class_init (ECompEditorPageRemindersClass *klass)
{
	ECompEditorPageClass *page_class;
	GObjectClass *object_class;

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
