/*
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
 */

#include "evolution-config.h"

#include <glib/gi18n-lib.h>
#include <gtk/gtk.h>

#include <e-util/e-util.h>

#include "calendar-config.h"
#include "comp-util.h"
#include "e-comp-editor.h"
#include "e-comp-editor-page.h"
#include "e-comp-editor-page-attachments.h"
#include "e-comp-editor-page-general.h"
#include "e-comp-editor-page-recurrence.h"
#include "e-comp-editor-page-reminders.h"
#include "e-comp-editor-property-part.h"
#include "e-comp-editor-property-parts.h"

#include "e-comp-editor-task.h"

struct _ECompEditorTaskPrivate {
	ECompEditorPage *page_general;
	ECompEditorPage *recurrence_page;
	ECompEditorPage *reminders_page;
	ECompEditorPropertyPart *categories;
	ECompEditorPropertyPart *dtstart;
	ECompEditorPropertyPart *due_date;
	ECompEditorPropertyPart *completed_date;
	ECompEditorPropertyPart *percentcomplete;
	ECompEditorPropertyPart *status;
	ECompEditorPropertyPart *estimated_duration;
	ECompEditorPropertyPart *timezone;
	ECompEditorPropertyPart *description;

	gpointer in_the_past_alert;
	gpointer insensitive_info_alert;
	gboolean dtstart_is_unset;
	gboolean due_is_unset;
};

G_DEFINE_TYPE_WITH_PRIVATE (ECompEditorTask, e_comp_editor_task, E_TYPE_COMP_EDITOR)

static ICalTimezone *
ece_task_get_timezone_from_property (ECompEditor *comp_editor,
				     ICalProperty *property)
{
	ECalClient *client;
	ICalParameter *param;
	ICalTimezone *zone = NULL;
	const gchar *tzid;

	g_return_val_if_fail (E_IS_COMP_EDITOR (comp_editor), NULL);

	if (!property)
		return NULL;

	param = i_cal_property_get_first_parameter (property, I_CAL_TZID_PARAMETER);
	if (!param)
		return NULL;

	tzid = i_cal_parameter_get_tzid (param);
	if (!tzid || !*tzid) {
		g_clear_object (&param);
		return NULL;
	}

	if (g_ascii_strcasecmp (tzid, "UTC") == 0) {
		g_clear_object (&param);
		return i_cal_timezone_get_utc_timezone ();
	}

	client = e_comp_editor_get_source_client (comp_editor);
	/* It should be already fetched for the UI, thus this should be non-blocking. */
	if (client && e_cal_client_get_timezone_sync (client, tzid, &zone, NULL, NULL) && zone) {
		g_clear_object (&param);
		return zone;
	}

	zone = i_cal_timezone_get_builtin_timezone_from_tzid (tzid);
	if (!zone)
		zone = i_cal_timezone_get_builtin_timezone (tzid);

	g_clear_object (&param);

	return zone;
}

static ICalTime *
ece_task_get_completed (ICalComponent *comp)
{
	ICalProperty *prop;
	ICalTime *tt;

	g_return_val_if_fail (I_CAL_IS_COMPONENT (comp), NULL);

	prop = i_cal_component_get_first_property (comp, I_CAL_COMPLETED_PROPERTY);
	if (!prop)
		return NULL;

	tt = i_cal_property_get_completed (prop);

	g_object_unref (prop);

	return tt;
}

static void
ece_task_update_timezone (ECompEditorTask *task_editor,
			  gboolean *force_allday)
{
	struct _props_data {
		ICalPropertyKind kind;
		ICalTime * (*get_func) (ICalComponent *comp);
	} properties[] = {
		{ I_CAL_DTSTART_PROPERTY, i_cal_component_get_dtstart },
		{ I_CAL_DUE_PROPERTY, i_cal_component_get_due },
		{ I_CAL_COMPLETED_PROPERTY, ece_task_get_completed }
	};
	ECompEditor *comp_editor;
	ICalComponent *component;
	ICalTimezone *zone = NULL;
	gboolean has_property = FALSE;
	gint ii;

	g_return_if_fail (E_IS_COMP_EDITOR_TASK (task_editor));

	if (force_allday)
		*force_allday = FALSE;

	comp_editor = E_COMP_EDITOR (task_editor);

	component = e_comp_editor_get_component (comp_editor);
	if (!component)
		return;

	for (ii = 0; !has_property && ii < G_N_ELEMENTS (properties); ii++) {
		if (e_cal_util_component_has_property (component, properties[ii].kind)) {
			ICalTime *dt;

			has_property = TRUE;

			dt = properties[ii].get_func (component);
			if (dt && i_cal_time_is_valid_time (dt)) {
				if (force_allday && i_cal_time_is_date (dt))
					*force_allday = TRUE;

				if (i_cal_time_is_utc (dt)) {
					zone = i_cal_timezone_get_utc_timezone ();
				} else {
					ICalProperty *prop;

					prop = i_cal_component_get_first_property (component, properties[ii].kind);
					zone = ece_task_get_timezone_from_property (comp_editor, prop);
					g_clear_object (&prop);
				}
			}

			g_clear_object (&dt);
		}
	}

	if (has_property) {
		GtkWidget *edit_widget;
		ICalTimezone *cfg_zone;

		edit_widget = e_comp_editor_property_part_get_edit_widget (task_editor->priv->timezone);

		e_timezone_entry_set_timezone (E_TIMEZONE_ENTRY (edit_widget), zone);

		cfg_zone = calendar_config_get_icaltimezone ();

		if (zone && cfg_zone && zone != cfg_zone &&
		    (g_strcmp0 (i_cal_timezone_get_location (zone), i_cal_timezone_get_location (cfg_zone)) != 0 ||
		     g_strcmp0 (i_cal_timezone_get_tzid (zone), i_cal_timezone_get_tzid (cfg_zone)) != 0)) {
			/* Show timezone part */
			EUIAction *action;

			action = e_comp_editor_get_action (comp_editor, "view-timezone");
			e_ui_action_set_active (action, TRUE);
		}
	}
}

static void
ece_task_notify_source_client_cb (GObject *object,
				  GParamSpec *param,
				  gpointer user_data)
{
	g_return_if_fail (E_IS_COMP_EDITOR_TASK (object));

	ece_task_update_timezone (E_COMP_EDITOR_TASK (object), NULL);
}

static void
ece_task_notify_target_client_cb (GObject *object,
				  GParamSpec *param,
				  gpointer user_data)
{
	ECompEditorTask *task_editor;
	ECompEditor *comp_editor;
	ECalClient *cal_client;
	EUIAction *action;
	GtkWidget *edit_widget;
	gboolean date_only;
	gboolean was_allday;
	gboolean can_recur;
	gboolean can_reminders;
	gboolean can_estimated_duration;

	g_return_if_fail (E_IS_COMP_EDITOR_TASK (object));

	task_editor = E_COMP_EDITOR_TASK (object);
	comp_editor = E_COMP_EDITOR (task_editor);
	cal_client = e_comp_editor_get_target_client (comp_editor);

	action = e_comp_editor_get_action (comp_editor, "all-day-task");
	was_allday = e_ui_action_get_active (action);

	date_only = !cal_client || e_client_check_capability (E_CLIENT (cal_client), E_CAL_STATIC_CAPABILITY_TASK_DATE_ONLY);

	e_comp_editor_property_part_datetime_set_date_only (E_COMP_EDITOR_PROPERTY_PART_DATETIME (task_editor->priv->dtstart), date_only);
	e_comp_editor_property_part_datetime_set_date_only (E_COMP_EDITOR_PROPERTY_PART_DATETIME (task_editor->priv->due_date), date_only);
	e_comp_editor_property_part_datetime_set_date_only (E_COMP_EDITOR_PROPERTY_PART_DATETIME (task_editor->priv->completed_date), date_only);

	edit_widget = e_comp_editor_property_part_get_edit_widget (task_editor->priv->timezone);
	gtk_widget_set_sensitive (edit_widget, !date_only);

	action = e_comp_editor_get_action (comp_editor, "view-timezone");
	e_ui_action_set_sensitive (action, !date_only);

	action = e_comp_editor_get_action (comp_editor, "all-day-task");
	e_ui_action_set_visible (action, !date_only);

	if (was_allday) {
		action = e_comp_editor_get_action (comp_editor, "all-day-task");
		e_ui_action_set_active (action, TRUE);
	}

	can_reminders = !cal_client || !e_client_check_capability (E_CLIENT (cal_client), E_CAL_STATIC_CAPABILITY_TASK_NO_ALARM);
	gtk_widget_set_visible (GTK_WIDGET (task_editor->priv->reminders_page), can_reminders);

	can_recur = !cal_client || e_client_check_capability (E_CLIENT (cal_client), E_CAL_STATIC_CAPABILITY_TASK_CAN_RECUR);
	gtk_widget_set_visible (GTK_WIDGET (task_editor->priv->recurrence_page), can_recur);

	can_estimated_duration = !cal_client || e_client_check_capability (E_CLIENT (cal_client), E_CAL_STATIC_CAPABILITY_TASK_ESTIMATED_DURATION);
	e_comp_editor_property_part_set_visible (task_editor->priv->estimated_duration, can_estimated_duration);
}

static void
ece_task_check_dates_in_the_past (ECompEditorTask *task_editor)
{
	guint32 flags;

	g_return_if_fail (E_IS_COMP_EDITOR_TASK (task_editor));

	flags = e_comp_editor_get_flags (E_COMP_EDITOR (task_editor));

	if (task_editor->priv->in_the_past_alert)
		e_alert_response (task_editor->priv->in_the_past_alert, GTK_RESPONSE_OK);

	if ((flags & E_COMP_EDITOR_FLAG_IS_NEW) != 0) {
		GString *message = NULL;
		ICalTime *dtstart_itt, *due_date_itt;

		dtstart_itt = e_comp_editor_property_part_datetime_get_value (
			E_COMP_EDITOR_PROPERTY_PART_DATETIME (task_editor->priv->dtstart));
		due_date_itt = e_comp_editor_property_part_datetime_get_value (
			E_COMP_EDITOR_PROPERTY_PART_DATETIME (task_editor->priv->due_date));

		if (cal_comp_util_compare_time_with_today (dtstart_itt) < 0)
			message = g_string_new (_("Task’s start date is in the past"));

		if (cal_comp_util_compare_time_with_today (due_date_itt) < 0) {
			if (message)
				g_string_append_c (message, '\n');
			else
				message = g_string_new ("");

			g_string_append (message, _("Task’s due date is in the past"));
		}

		if (message) {
			EAlert *alert;

			alert = e_comp_editor_add_warning (E_COMP_EDITOR (task_editor), message->str, NULL);

			task_editor->priv->in_the_past_alert = alert;

			if (alert)
				g_object_add_weak_pointer (G_OBJECT (alert), &task_editor->priv->in_the_past_alert);

			g_string_free (message, TRUE);
			g_clear_object (&alert);
		}

		g_clear_object (&dtstart_itt);
		g_clear_object (&due_date_itt);
	}
}

static void
ece_task_dtstart_changed_cb (EDateEdit *date_edit,
			     ECompEditorTask *task_editor)
{
	ECompEditor *comp_editor;
	gboolean was_unset;

	g_return_if_fail (E_IS_DATE_EDIT (date_edit));
	g_return_if_fail (E_IS_COMP_EDITOR_TASK (task_editor));

	was_unset = task_editor->priv->dtstart_is_unset;
	task_editor->priv->dtstart_is_unset = e_date_edit_get_time (date_edit) == (time_t) -1;

	comp_editor = E_COMP_EDITOR (task_editor);

	if (e_comp_editor_get_updating (comp_editor))
		return;

	e_comp_editor_set_updating (comp_editor, TRUE);

	e_comp_editor_ensure_start_before_end (E_COMP_EDITOR (task_editor),
		task_editor->priv->dtstart, task_editor->priv->due_date,
		TRUE);

	/* When setting DTSTART for the first time, derive the type from the DUE,
	   otherwise the DUE has changed the type to the DATE only. */
	if (was_unset) {
		e_comp_editor_ensure_same_value_type (E_COMP_EDITOR (task_editor),
			task_editor->priv->due_date, task_editor->priv->dtstart);
	} else {
		e_comp_editor_ensure_same_value_type (E_COMP_EDITOR (task_editor),
			task_editor->priv->dtstart, task_editor->priv->due_date);
	}

	e_comp_editor_set_updating (comp_editor, FALSE);

	ece_task_check_dates_in_the_past (task_editor);
}

static void
ece_task_due_date_changed_cb (EDateEdit *date_edit,
			      ECompEditorTask *task_editor)
{
	ECompEditor *comp_editor;
	gboolean was_unset;

	g_return_if_fail (E_IS_DATE_EDIT (date_edit));
	g_return_if_fail (E_IS_COMP_EDITOR_TASK (task_editor));

	was_unset = task_editor->priv->due_is_unset;
	task_editor->priv->due_is_unset = e_date_edit_get_time (date_edit) == (time_t) -1;

	comp_editor = E_COMP_EDITOR (task_editor);

	if (e_comp_editor_get_updating (comp_editor))
		return;

	e_comp_editor_set_updating (comp_editor, TRUE);

	e_comp_editor_ensure_start_before_end (E_COMP_EDITOR (task_editor),
		task_editor->priv->dtstart, task_editor->priv->due_date,
		FALSE);

	/* When setting DUE for the first time, derive the type from the DTSTART,
	   otherwise the DTSTART has changed the type to the DATE only. */
	if (was_unset) {
		e_comp_editor_ensure_same_value_type (E_COMP_EDITOR (task_editor),
			task_editor->priv->dtstart, task_editor->priv->due_date);
	} else {
		e_comp_editor_ensure_same_value_type (E_COMP_EDITOR (task_editor),
			task_editor->priv->due_date, task_editor->priv->dtstart);
	}

	e_comp_editor_set_updating (comp_editor, FALSE);

	ece_task_check_dates_in_the_past (task_editor);
}

static void
ece_task_completed_date_changed_cb (EDateEdit *date_edit,
				    ECompEditorTask *task_editor)
{
	GtkSpinButton *percent_spin;
	ECompEditor *comp_editor;
	ICalTime *itt;
	gint status;

	g_return_if_fail (E_IS_DATE_EDIT (date_edit));
	g_return_if_fail (E_IS_COMP_EDITOR_TASK (task_editor));

	comp_editor = E_COMP_EDITOR (task_editor);

	if (e_comp_editor_get_updating (comp_editor))
		return;

	e_comp_editor_set_updating (comp_editor, TRUE);

	status = e_comp_editor_property_part_picker_with_map_get_selected (
		E_COMP_EDITOR_PROPERTY_PART_PICKER_WITH_MAP (task_editor->priv->status));
	itt = e_comp_editor_property_part_datetime_get_value (
		E_COMP_EDITOR_PROPERTY_PART_DATETIME (task_editor->priv->completed_date));
	percent_spin = GTK_SPIN_BUTTON (e_comp_editor_property_part_get_edit_widget (task_editor->priv->percentcomplete));

	if (!itt || i_cal_time_is_null_time (itt)) {
		if (status == I_CAL_STATUS_COMPLETED) {
			e_comp_editor_property_part_picker_with_map_set_selected (
				E_COMP_EDITOR_PROPERTY_PART_PICKER_WITH_MAP (task_editor->priv->status),
				I_CAL_STATUS_NONE);

			gtk_spin_button_set_value (percent_spin, 0);
		}
	} else {
		if (status != I_CAL_STATUS_COMPLETED) {
			e_comp_editor_property_part_picker_with_map_set_selected (
				E_COMP_EDITOR_PROPERTY_PART_PICKER_WITH_MAP (task_editor->priv->status),
				I_CAL_STATUS_COMPLETED);
		}

		gtk_spin_button_set_value (percent_spin, 100);
	}

	e_comp_editor_set_updating (comp_editor, FALSE);

	g_clear_object (&itt);
}

static void
ece_task_status_changed_cb (GtkComboBox *combo_box,
			    ECompEditorTask *task_editor)
{
	ECompEditor *comp_editor;
	GtkSpinButton *percent_spin;
	EDateEdit *completed_date;
	gint status;

	g_return_if_fail (GTK_IS_COMBO_BOX (combo_box));
	g_return_if_fail (E_IS_COMP_EDITOR_TASK (task_editor));

	comp_editor = E_COMP_EDITOR (task_editor);

	if (e_comp_editor_get_updating (comp_editor))
		return;

	e_comp_editor_set_updating (comp_editor, TRUE);

	percent_spin = GTK_SPIN_BUTTON (e_comp_editor_property_part_get_edit_widget (task_editor->priv->percentcomplete));
	completed_date = E_DATE_EDIT (e_comp_editor_property_part_get_edit_widget (task_editor->priv->completed_date));
	status = e_comp_editor_property_part_picker_with_map_get_selected (
		E_COMP_EDITOR_PROPERTY_PART_PICKER_WITH_MAP (task_editor->priv->status));

	if (status == I_CAL_STATUS_NONE) {
		gtk_spin_button_set_value (percent_spin, 0);
		e_date_edit_set_time (completed_date, (time_t) -1);
	} else if (status == I_CAL_STATUS_INPROCESS) {
		gint percent_complete = gtk_spin_button_get_value_as_int (percent_spin);

		if (percent_complete <= 0 || percent_complete >= 100)
			gtk_spin_button_set_value (percent_spin, 50);

		e_date_edit_set_time (completed_date, (time_t) -1);
	} else if (status == I_CAL_STATUS_COMPLETED) {
		gtk_spin_button_set_value (percent_spin, 100);
		e_date_edit_set_time (completed_date, time (NULL));
	}

	e_comp_editor_set_updating (comp_editor, FALSE);
}

static void
ece_task_percentcomplete_value_changed_cb (GtkSpinButton *spin_button,
					   ECompEditorTask *task_editor)
{
	ECompEditor *comp_editor;
	GtkSpinButton *percent_spin;
	EDateEdit *completed_date;
	gint status, percent;
	time_t ctime;

	g_return_if_fail (GTK_IS_SPIN_BUTTON (spin_button));
	g_return_if_fail (E_IS_COMP_EDITOR_TASK (task_editor));

	comp_editor = E_COMP_EDITOR (task_editor);

	if (e_comp_editor_get_updating (comp_editor))
		return;

	e_comp_editor_set_updating (comp_editor, TRUE);

	percent_spin = GTK_SPIN_BUTTON (e_comp_editor_property_part_get_edit_widget (task_editor->priv->percentcomplete));
	completed_date = E_DATE_EDIT (e_comp_editor_property_part_get_edit_widget (task_editor->priv->completed_date));

	percent = gtk_spin_button_get_value_as_int (percent_spin);
	if (percent == 100) {
		ctime = time (NULL);
		status = I_CAL_STATUS_COMPLETED;
	} else {
		ctime = (time_t) -1;

		if (percent == 0)
			status = I_CAL_STATUS_NONE;
		else
			status = I_CAL_STATUS_INPROCESS;
	}

	e_comp_editor_property_part_picker_with_map_set_selected (
		E_COMP_EDITOR_PROPERTY_PART_PICKER_WITH_MAP (task_editor->priv->status), status);
	e_date_edit_set_time (completed_date, ctime);

	e_comp_editor_set_updating (comp_editor, FALSE);
}

static void
ece_task_sensitize_widgets (ECompEditor *comp_editor,
			    gboolean force_insensitive)
{
	ECompEditorTask *task_editor;
	EUIAction *action;
	gboolean is_organizer;
	guint32 flags;

	g_return_if_fail (E_IS_COMP_EDITOR_TASK (comp_editor));

	E_COMP_EDITOR_CLASS (e_comp_editor_task_parent_class)->sensitize_widgets (comp_editor, force_insensitive);

	flags = e_comp_editor_get_flags (comp_editor);
	is_organizer = (flags & (E_COMP_EDITOR_FLAG_IS_NEW | E_COMP_EDITOR_FLAG_ORGANIZER_IS_USER)) != 0;
	task_editor = E_COMP_EDITOR_TASK (comp_editor);

	action = e_comp_editor_get_action (comp_editor, "all-day-task");
	e_ui_action_set_sensitive (action, !force_insensitive);

	if (task_editor->priv->insensitive_info_alert)
		e_alert_response (task_editor->priv->insensitive_info_alert, GTK_RESPONSE_OK);

	if (force_insensitive || !is_organizer) {
		ECalClient *client;
		const gchar *message = NULL;

		client = e_comp_editor_get_target_client (comp_editor);
		if (!client)
			message = _("Task cannot be edited, because the selected task list could not be opened");
		else if (e_client_is_readonly (E_CLIENT (client)))
			message = _("Task cannot be edited, because the selected task list is read only");
		else if (!is_organizer)
			message = _("Changes made to the task will not be sent to the attendees, because you are not the organizer");

		if (message) {
			EAlert *alert;

			alert = e_comp_editor_add_information (comp_editor, message, NULL);

			task_editor->priv->insensitive_info_alert = alert;

			if (alert)
				g_object_add_weak_pointer (G_OBJECT (alert), &task_editor->priv->insensitive_info_alert);

			g_clear_object (&alert);
		}
	}

	ece_task_check_dates_in_the_past (task_editor);
}

static void
ece_task_fill_widgets (ECompEditor *comp_editor,
		       ICalComponent *component)
{
	gboolean force_allday = FALSE;

	g_return_if_fail (E_IS_COMP_EDITOR_TASK (comp_editor));
	g_return_if_fail (I_CAL_IS_COMPONENT (component));

	ece_task_update_timezone (E_COMP_EDITOR_TASK (comp_editor), &force_allday);

	E_COMP_EDITOR_CLASS (e_comp_editor_task_parent_class)->fill_widgets (comp_editor, component);

	if (force_allday) {
		EUIAction *action;

		action = e_comp_editor_get_action (comp_editor, "all-day-task");
		e_ui_action_set_active (action, TRUE);
	}
}

static gboolean
ece_task_fill_component (ECompEditor *comp_editor,
			 ICalComponent *component)
{
	ECompEditorTask *task_editor;
	ICalTime *itt;

	g_return_val_if_fail (E_IS_COMP_EDITOR_TASK (comp_editor), FALSE);

	task_editor = E_COMP_EDITOR_TASK (comp_editor);

	if (!e_comp_editor_property_part_datetime_check_validity (
		E_COMP_EDITOR_PROPERTY_PART_DATETIME (task_editor->priv->dtstart), NULL, NULL)) {

		e_comp_editor_set_validation_error (comp_editor,
			task_editor->priv->page_general,
			e_comp_editor_property_part_get_edit_widget (task_editor->priv->dtstart),
			_("Start date is not a valid date"));

		return FALSE;
	}

	if (!e_comp_editor_property_part_datetime_check_validity (
		E_COMP_EDITOR_PROPERTY_PART_DATETIME (task_editor->priv->due_date), NULL, NULL)) {

		e_comp_editor_set_validation_error (comp_editor,
			task_editor->priv->page_general,
			e_comp_editor_property_part_get_edit_widget (task_editor->priv->due_date),
			_("Due date is not a valid date"));

		return FALSE;
	}

	if (!e_comp_editor_property_part_datetime_check_validity (
		E_COMP_EDITOR_PROPERTY_PART_DATETIME (task_editor->priv->completed_date), NULL, NULL)) {

		e_comp_editor_set_validation_error (comp_editor,
			task_editor->priv->page_general,
			e_comp_editor_property_part_get_edit_widget (task_editor->priv->completed_date),
			_("Completed date is not a valid date"));

		return FALSE;
	}

	itt = e_comp_editor_property_part_datetime_get_value (
		E_COMP_EDITOR_PROPERTY_PART_DATETIME (task_editor->priv->completed_date));
	if (cal_comp_util_compare_time_with_today (itt) > 0) {
		e_comp_editor_set_validation_error (comp_editor,
			task_editor->priv->page_general,
			e_comp_editor_property_part_get_edit_widget (task_editor->priv->completed_date),
			_("Completed date cannot be in the future"));

		g_clear_object (&itt);

		return FALSE;
	}

	g_clear_object (&itt);

	itt = e_comp_editor_property_part_datetime_get_value (E_COMP_EDITOR_PROPERTY_PART_DATETIME (task_editor->priv->dtstart));

	if (itt && i_cal_time_is_valid_time (itt) && !i_cal_time_is_null_time (itt)) {
		ICalTime *due;

		due = e_comp_editor_property_part_datetime_get_value (E_COMP_EDITOR_PROPERTY_PART_DATETIME (task_editor->priv->due_date));

		if (due && i_cal_time_is_valid_time (due) && !i_cal_time_is_null_time (due)) {
			gboolean same;

			if (i_cal_time_is_date (itt))
				same = i_cal_time_compare_date_only (itt, due) == 0;
			else
				same = i_cal_time_compare (itt, due) == 0;

			if (same) {
				e_comp_editor_set_validation_error (comp_editor,
					task_editor->priv->page_general,
					e_comp_editor_property_part_get_edit_widget (task_editor->priv->due_date),
					_("Due date cannot be the same as the Start date"));

				g_clear_object (&itt);
				g_clear_object (&due);

				return FALSE;
			}
		}

		g_clear_object (&due);
	}

	g_clear_object (&itt);

	if (!E_COMP_EDITOR_CLASS (e_comp_editor_task_parent_class)->fill_component (comp_editor, component))
		return FALSE;

	if (e_cal_util_component_has_recurrences (component)) {
		ECalClient *cal_client;
		ICalTime *dtstart;

		/* Check this only after the recurrence page updates the component,
		   like when the component is not recurring anymore. */
		dtstart = e_comp_editor_property_part_datetime_get_value (E_COMP_EDITOR_PROPERTY_PART_DATETIME (task_editor->priv->dtstart));

		if (!dtstart || i_cal_time_is_null_time (dtstart) || !i_cal_time_is_valid_time (dtstart)) {
			e_comp_editor_set_validation_error (comp_editor,
				task_editor->priv->page_general,
				e_comp_editor_property_part_get_edit_widget (task_editor->priv->dtstart),
				_("Start date is required for recurring tasks"));

			g_clear_object (&dtstart);

			return FALSE;
		}

		g_clear_object (&dtstart);

		cal_client = e_comp_editor_get_source_client (comp_editor);
		if (!cal_client)
			cal_client = e_comp_editor_get_target_client (comp_editor);

		if (cal_client) {
			if ((e_comp_editor_get_flags (comp_editor) & E_COMP_EDITOR_FLAG_IS_NEW) != 0) {
				e_cal_util_init_recur_task_sync	(component, cal_client, NULL, NULL);
			} else if (e_cal_util_component_has_property (component, I_CAL_COMPLETED_PROPERTY)) {
				e_cal_util_mark_task_complete_sync (component, (time_t) -1, cal_client, NULL, NULL);
			} else if (!e_cal_util_component_has_property (component, I_CAL_DUE_PROPERTY)) {
				e_cal_util_init_recur_task_sync	(component, cal_client, NULL, NULL);
			}
		}
	}

	return TRUE;
}

static void
ece_task_all_day_notify_active_cb (GObject *object,
				   GParamSpec *param,
				   gpointer user_data)
{
	ECompEditorTask *task_editor = user_data;
	gboolean active = FALSE, visible = FALSE;

	g_object_get (object,
		"active", &active,
		"visible", &visible,
		NULL);

	if (!active && visible) {
		EDateEdit *dtstart_date_edit;

		dtstart_date_edit = E_DATE_EDIT (e_comp_editor_property_part_get_edit_widget (task_editor->priv->dtstart));

		if (e_date_edit_get_time (dtstart_date_edit) != (time_t) -1) {
			EDateEdit *due_date_edit;

			due_date_edit = E_DATE_EDIT (e_comp_editor_property_part_get_edit_widget (task_editor->priv->due_date));

			if (e_date_edit_get_time (due_date_edit) != (time_t) -1) {
				gint hour, minute;

				if (e_date_edit_get_time_of_day (dtstart_date_edit, &hour, &minute) !=
				    e_date_edit_get_time_of_day (due_date_edit, &hour, &minute)) {
					if (e_date_edit_get_time_of_day (dtstart_date_edit, &hour, &minute))
						e_date_edit_set_time_of_day (due_date_edit, hour, minute);
					else
						e_date_edit_set_time_of_day (due_date_edit, -1, -1);
				}
			}
		}
	}
}

static void
ece_task_setup_ui (ECompEditorTask *task_editor)
{
	static const gchar *eui =
		"<eui>"
		  "<menu id='main-menu'>"
		    "<submenu action='view-menu'>"
		      "<placeholder id='parts'>"
			"<item action='view-timezone' text_only='true'/>"
			"<item action='view-categories' text_only='true'/>"
		      "</placeholder>"
		    "</submenu>"
		    "<submenu action='options-menu'>"
		      "<placeholder id='toggles'>"
			"<item action='all-day-task' text_only='true'/>"
		      "</placeholder>"
		    "</submenu>"
		  "</menu>"
		  "<toolbar id='toolbar-with-headerbar'>"
		    "<placeholder id='content'>"
		      "<item action='all-day-task'/>"
		    "</placeholder>"
		  "</toolbar>"
		  "<toolbar id='toolbar-without-headerbar'>"
		    "<placeholder id='content'>"
		      "<item action='all-day-task'/>"
		    "</placeholder>"
		  "</toolbar>"
		"</eui>";

	static const EUIActionEntry view_actions[] = {
		{ "view-categories",
		  NULL,
		  N_("_Categories"),
		  NULL,
		  N_("Toggles whether to display categories"),
		  NULL, NULL, "false", (EUIActionFunc) e_ui_action_set_state },

		{ "view-timezone",
		  "stock_timezone",
		  N_("Time _Zone"),
		  NULL,
		  N_("Toggles whether the time zone is displayed"),
		  NULL, NULL, "false", (EUIActionFunc) e_ui_action_set_state },

		{ "all-day-task",
		  "stock_new-24h-appointment",
		  N_("All _Day Task"),
		  "<Control>Y",
		  N_("Toggles whether to have All Day Task"),
		  NULL, NULL, "false", (EUIActionFunc) e_ui_action_set_state }
	};

	ECompEditor *comp_editor;
	GSettings *settings;
	EUIManager *ui_manager;
	EUIAction *action;
	GtkWidget *edit_widget;

	g_return_if_fail (E_IS_COMP_EDITOR_TASK (task_editor));

	comp_editor = E_COMP_EDITOR (task_editor);
	settings = e_comp_editor_get_settings (comp_editor);
	ui_manager = e_comp_editor_get_ui_manager (comp_editor);

	e_ui_manager_add_actions_with_eui_data (ui_manager, "individual", GETTEXT_PACKAGE,
		view_actions, G_N_ELEMENTS (view_actions), task_editor, eui);

	action = e_comp_editor_get_action (comp_editor, "view-timezone");
	e_binding_bind_property (
		task_editor->priv->timezone, "visible",
		action, "active",
		G_BINDING_SYNC_CREATE | G_BINDING_BIDIRECTIONAL);
	g_settings_bind (
		settings, "editor-show-timezone",
		action, "active",
		G_SETTINGS_BIND_DEFAULT);

	action = e_comp_editor_get_action (comp_editor, "view-categories");
	e_binding_bind_property (
		task_editor->priv->categories, "visible",
		action, "active",
		G_BINDING_SYNC_CREATE | G_BINDING_BIDIRECTIONAL);
	g_settings_bind (
		settings, "editor-show-categories",
		action, "active",
		G_SETTINGS_BIND_DEFAULT);

	action = e_comp_editor_get_action (comp_editor, "all-day-task");

	edit_widget = e_comp_editor_property_part_get_edit_widget (task_editor->priv->dtstart);
	e_binding_bind_property (
		action, "active",
		edit_widget, "show-time",
		G_BINDING_INVERT_BOOLEAN | G_BINDING_BIDIRECTIONAL);

	edit_widget = e_comp_editor_property_part_get_edit_widget (task_editor->priv->due_date);
	e_binding_bind_property (
		action, "active",
		edit_widget, "show-time",
		G_BINDING_INVERT_BOOLEAN);

	edit_widget = e_comp_editor_property_part_get_edit_widget (task_editor->priv->completed_date);
	e_binding_bind_property (
		action, "active",
		edit_widget, "show-time",
		G_BINDING_INVERT_BOOLEAN);

	e_signal_connect_notify (action, "notify::active",
		G_CALLBACK (ece_task_all_day_notify_active_cb), task_editor);
}

static void
e_comp_editor_task_constructed (GObject *object)
{
	ECompEditorTask *task_editor;
	ECompEditor *comp_editor;
	ECompEditorPage *page;
	ECompEditorPropertyPart *part, *summary;
	EFocusTracker *focus_tracker;
	EUIManager *ui_manager;
	GtkWidget *edit_widget;

	G_OBJECT_CLASS (e_comp_editor_task_parent_class)->constructed (object);

	task_editor = E_COMP_EDITOR_TASK (object);
	comp_editor = E_COMP_EDITOR (task_editor);
	ui_manager = e_comp_editor_get_ui_manager (comp_editor);
	focus_tracker = e_comp_editor_get_focus_tracker (comp_editor);

	e_ui_manager_freeze (ui_manager);

	page = e_comp_editor_page_general_new (comp_editor,
		_("_List:"), E_SOURCE_EXTENSION_TASK_LIST,
		NULL, FALSE, 3);

	part = e_comp_editor_property_part_summary_new (focus_tracker);
	e_comp_editor_page_add_property_part (page, part, 0, 2, 4, 1);
	summary = part;

	part = e_comp_editor_property_part_location_new (focus_tracker);
	e_comp_editor_page_add_property_part (page, part, 0, 3, 4, 1);

	part = e_comp_editor_property_part_dtstart_new (C_("ECompEditor", "Sta_rt date:"), TRUE, TRUE, FALSE);
	e_comp_editor_page_add_property_part (page, part, 0, 4, 2, 1);
	task_editor->priv->dtstart = part;

	edit_widget = e_comp_editor_property_part_get_edit_widget (part);
	g_signal_connect (edit_widget, "changed", G_CALLBACK (ece_task_dtstart_changed_cb), task_editor);

	part = e_comp_editor_property_part_status_new (I_CAL_VTODO_COMPONENT);
	e_comp_editor_page_add_property_part (page, part, 2, 4, 2, 1);
	task_editor->priv->status = part;

	edit_widget = e_comp_editor_property_part_get_edit_widget (part);
	g_signal_connect (edit_widget, "changed", G_CALLBACK (ece_task_status_changed_cb), task_editor);

	part = e_comp_editor_property_part_due_new (TRUE, TRUE);
	e_comp_editor_page_add_property_part (page, part, 0, 5, 2, 1);
	task_editor->priv->due_date = part;

	edit_widget = e_comp_editor_property_part_get_edit_widget (part);
	g_signal_connect (edit_widget, "changed", G_CALLBACK (ece_task_due_date_changed_cb), task_editor);

	part = e_comp_editor_property_part_priority_new ();
	e_comp_editor_page_add_property_part (page, part, 2, 5, 2, 1);

	part = e_comp_editor_property_part_completed_new (TRUE, TRUE);
	e_comp_editor_page_add_property_part (page, part, 0, 6, 2, 1);
	task_editor->priv->completed_date = part;

	edit_widget = e_comp_editor_property_part_get_edit_widget (part);
	g_signal_connect (edit_widget, "changed", G_CALLBACK (ece_task_completed_date_changed_cb), task_editor);

	part = e_comp_editor_property_part_percentcomplete_new ();
	e_comp_editor_page_add_property_part (page, part, 2, 6, 2, 1);
	task_editor->priv->percentcomplete = part;

	edit_widget = e_comp_editor_property_part_get_edit_widget (part);
	g_signal_connect (edit_widget, "value-changed", G_CALLBACK (ece_task_percentcomplete_value_changed_cb), task_editor);

	part = e_comp_editor_property_part_url_new (focus_tracker);
	e_comp_editor_page_add_property_part (page, part, 0, 7, 2, 1);

	edit_widget = e_comp_editor_property_part_get_edit_widget (part);
	gtk_widget_set_hexpand (edit_widget, TRUE);

	part = e_comp_editor_property_part_classification_new ();
	e_comp_editor_page_add_property_part (page, part, 2, 7, 2, 1);

	part = e_comp_editor_property_part_estimated_duration_new ();
	e_comp_editor_page_add_property_part (page, part, 0, 8, 4, 1);
	task_editor->priv->estimated_duration = part;

	part = e_comp_editor_property_part_timezone_new ();
	e_comp_editor_page_add_property_part (page, part, 0, 9, 4, 1);
	task_editor->priv->timezone = part;

	part = e_comp_editor_property_part_categories_new (focus_tracker);
	e_comp_editor_page_add_property_part (page, part, 0, 10, 4, 1);
	task_editor->priv->categories = part;

	part = e_comp_editor_property_part_description_new (focus_tracker);
	e_comp_editor_page_add_property_part (page, part, 0, 11, 4, 1);
	task_editor->priv->description = part;

	e_comp_editor_add_page (comp_editor, C_("ECompEditorPage", "General"), page);
	task_editor->priv->page_general = page;

	edit_widget = e_comp_editor_property_part_get_edit_widget (task_editor->priv->timezone);
	e_comp_editor_property_part_datetime_attach_timezone_entry (
		E_COMP_EDITOR_PROPERTY_PART_DATETIME (task_editor->priv->dtstart),
		E_TIMEZONE_ENTRY (edit_widget));
	g_signal_connect_swapped (task_editor->priv->dtstart, "lookup-timezone",
		G_CALLBACK (e_comp_editor_lookup_timezone), task_editor);
	e_comp_editor_property_part_datetime_attach_timezone_entry (
		E_COMP_EDITOR_PROPERTY_PART_DATETIME (task_editor->priv->due_date),
		E_TIMEZONE_ENTRY (edit_widget));
	g_signal_connect_swapped (task_editor->priv->due_date, "lookup-timezone",
		G_CALLBACK (e_comp_editor_lookup_timezone), task_editor);
	e_comp_editor_property_part_datetime_attach_timezone_entry (
		E_COMP_EDITOR_PROPERTY_PART_DATETIME (task_editor->priv->completed_date),
		E_TIMEZONE_ENTRY (edit_widget));
	g_signal_connect_swapped (task_editor->priv->completed_date, "lookup-timezone",
		G_CALLBACK (e_comp_editor_lookup_timezone), task_editor);

	e_comp_editor_set_time_parts (comp_editor, task_editor->priv->dtstart, task_editor->priv->due_date);

	page = e_comp_editor_page_reminders_new (comp_editor);
	e_comp_editor_add_page (comp_editor, C_("ECompEditorPage", "Reminders"), page);
	task_editor->priv->reminders_page = page;

	page = e_comp_editor_page_recurrence_new (comp_editor);
	e_comp_editor_add_page (comp_editor, C_("ECompEditorPage", "Recurrence"), page);
	task_editor->priv->recurrence_page = page;

	page = e_comp_editor_page_attachments_new (comp_editor);
	e_comp_editor_add_page (comp_editor, C_("ECompEditorPage", "Attachments"), page);

	ece_task_setup_ui (task_editor);

	edit_widget = e_comp_editor_property_part_get_edit_widget (summary);
	e_binding_bind_property (edit_widget, "text", comp_editor, "title-suffix", 0);
	gtk_widget_grab_focus (edit_widget);

	g_signal_connect (comp_editor, "notify::source-client",
		G_CALLBACK (ece_task_notify_source_client_cb), NULL);
	g_signal_connect (comp_editor, "notify::target-client",
		G_CALLBACK (ece_task_notify_target_client_cb), NULL);

	e_extensible_load_extensions (E_EXTENSIBLE (comp_editor));

	e_ui_manager_thaw (ui_manager);
}

static void
e_comp_editor_task_init (ECompEditorTask *task_editor)
{
	task_editor->priv = e_comp_editor_task_get_instance_private (task_editor);
}

static void
e_comp_editor_task_class_init (ECompEditorTaskClass *klass)
{
	GObjectClass *object_class;
	ECompEditorClass *comp_editor_class;

	object_class = G_OBJECT_CLASS (klass);
	object_class->constructed = e_comp_editor_task_constructed;

	comp_editor_class = E_COMP_EDITOR_CLASS (klass);
	comp_editor_class->help_section = "tasks-usage";
	comp_editor_class->title_format_with_attendees = _("Assigned Task — %s");
	comp_editor_class->title_format_without_attendees = _("Task — %s");
	comp_editor_class->icon_name = "stock_task";
	comp_editor_class->sensitize_widgets = ece_task_sensitize_widgets;
	comp_editor_class->fill_widgets = ece_task_fill_widgets;
	comp_editor_class->fill_component = ece_task_fill_component;
}
