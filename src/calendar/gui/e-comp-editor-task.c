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
#include "e-comp-editor-page-reminders.h"
#include "e-comp-editor-property-part.h"
#include "e-comp-editor-property-parts.h"

#include "e-comp-editor-task.h"

struct _ECompEditorTaskPrivate {
	ECompEditorPage *page_general;
	ECompEditorPropertyPart *categories;
	ECompEditorPropertyPart *dtstart;
	ECompEditorPropertyPart *due_date;
	ECompEditorPropertyPart *completed_date;
	ECompEditorPropertyPart *percentcomplete;
	ECompEditorPropertyPart *status;
	ECompEditorPropertyPart *timezone;

	gpointer in_the_past_alert;
	gpointer insensitive_info_alert;
};

G_DEFINE_TYPE (ECompEditorTask, e_comp_editor_task, E_TYPE_COMP_EDITOR)

static icaltimezone *
ece_task_get_timezone_from_property (ECompEditor *comp_editor,
				     icalproperty *property)
{
	ECalClient *client;
	icalparameter *param;
	icaltimezone *zone = NULL;
	const gchar *tzid;

	g_return_val_if_fail (E_IS_COMP_EDITOR (comp_editor), NULL);

	if (!property)
		return NULL;

	param = icalproperty_get_first_parameter (property, ICAL_TZID_PARAMETER);
	if (!param)
		return NULL;

	tzid = icalparameter_get_tzid (param);
	if (!tzid || !*tzid)
		return NULL;

	if (g_ascii_strcasecmp (tzid, "UTC") == 0)
		return icaltimezone_get_utc_timezone ();

	client = e_comp_editor_get_source_client (comp_editor);
	/* It should be already fetched for the UI, thus this should be non-blocking. */
	if (client && e_cal_client_get_timezone_sync (client, tzid, &zone, NULL, NULL) && zone)
		return zone;

	zone = icaltimezone_get_builtin_timezone_from_tzid (tzid);
	if (!zone)
		zone = icaltimezone_get_builtin_timezone (tzid);

	return zone;
}

static void
ece_task_update_timezone (ECompEditorTask *task_editor,
			  gboolean *force_allday)
{
	const gint properties[] = {
		ICAL_DTSTART_PROPERTY,
		ICAL_DUE_PROPERTY,
		ICAL_COMPLETED_PROPERTY
	};
	ECompEditor *comp_editor;
	icalcomponent *component;
	icaltimezone *zone = NULL;
	gint ii;

	g_return_if_fail (E_IS_COMP_EDITOR_TASK (task_editor));

	if (force_allday)
		*force_allday = FALSE;

	comp_editor = E_COMP_EDITOR (task_editor);

	component = e_comp_editor_get_component (comp_editor);
	if (!component)
		return;

	for (ii = 0; !zone && ii < G_N_ELEMENTS (properties); ii++) {
		struct icaltimetype dt;
		if (icalcomponent_get_first_property (component, properties[ii])) {
			dt = icalcomponent_get_dtstart (component);
			if (icaltime_is_valid_time (dt)) {
				if (force_allday && dt.is_date)
					*force_allday = TRUE;

				if (dt.is_utc)
					zone = icaltimezone_get_utc_timezone ();
				else
					zone = ece_task_get_timezone_from_property (comp_editor,
						icalcomponent_get_first_property (component, properties[ii]));
			}
		}
	}

	if (zone) {
		GtkWidget *edit_widget;

		edit_widget = e_comp_editor_property_part_get_edit_widget (task_editor->priv->timezone);

		e_timezone_entry_set_timezone (E_TIMEZONE_ENTRY (edit_widget), zone);

		if (zone != calendar_config_get_icaltimezone ()) {
			/* Show timezone part */
			GtkAction *action;

			action = e_comp_editor_get_action (comp_editor, "view-timezone");
			gtk_toggle_action_set_active (GTK_TOGGLE_ACTION (action), TRUE);
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
	GtkWidget *edit_widget;
	GtkAction *action;
	gboolean date_only;
	gboolean was_allday;

	g_return_if_fail (E_IS_COMP_EDITOR_TASK (object));

	task_editor = E_COMP_EDITOR_TASK (object);
	comp_editor = E_COMP_EDITOR (task_editor);
	cal_client = e_comp_editor_get_target_client (comp_editor);

	action = e_comp_editor_get_action (comp_editor, "all-day-task");
	was_allday = gtk_toggle_action_get_active (GTK_TOGGLE_ACTION (action));

	date_only = !cal_client || e_client_check_capability (E_CLIENT (cal_client), CAL_STATIC_CAPABILITY_TASK_DATE_ONLY);

	e_comp_editor_property_part_datetime_set_date_only (E_COMP_EDITOR_PROPERTY_PART_DATETIME (task_editor->priv->dtstart), date_only);
	e_comp_editor_property_part_datetime_set_date_only (E_COMP_EDITOR_PROPERTY_PART_DATETIME (task_editor->priv->due_date), date_only);
	e_comp_editor_property_part_datetime_set_date_only (E_COMP_EDITOR_PROPERTY_PART_DATETIME (task_editor->priv->completed_date), date_only);

	edit_widget = e_comp_editor_property_part_get_edit_widget (task_editor->priv->timezone);
	gtk_widget_set_sensitive (edit_widget, !date_only);

	action = e_comp_editor_get_action (comp_editor, "view-timezone");
	gtk_action_set_sensitive (action, !date_only);

	action = e_comp_editor_get_action (comp_editor, "all-day-task");
	gtk_action_set_visible (action, !date_only);

	if (was_allday) {
		action = e_comp_editor_get_action (comp_editor, "all-day-task");
		gtk_toggle_action_set_active (GTK_TOGGLE_ACTION (action), TRUE);
	}
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
		struct icaltimetype dtstart_itt, due_date_itt;

		dtstart_itt = e_comp_editor_property_part_datetime_get_value (
			E_COMP_EDITOR_PROPERTY_PART_DATETIME (task_editor->priv->dtstart));
		due_date_itt = e_comp_editor_property_part_datetime_get_value (
			E_COMP_EDITOR_PROPERTY_PART_DATETIME (task_editor->priv->due_date));

		if (cal_comp_util_compare_time_with_today (dtstart_itt) < 0)
			message = g_string_new (_("Task’s start date is in the past"));

		if (cal_comp_util_compare_time_with_today (due_date_itt) < 0) {
			if (message)
				g_string_append (message, "\n");
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
	}
}

static void
ece_task_dtstart_changed_cb (EDateEdit *date_edit,
			     ECompEditorTask *task_editor)
{
	ECompEditor *comp_editor;

	g_return_if_fail (E_IS_DATE_EDIT (date_edit));
	g_return_if_fail (E_IS_COMP_EDITOR_TASK (task_editor));

	comp_editor = E_COMP_EDITOR (task_editor);

	if (e_comp_editor_get_updating (comp_editor))
		return;

	e_comp_editor_set_updating (comp_editor, TRUE);

	e_comp_editor_ensure_start_before_end (E_COMP_EDITOR (task_editor),
		task_editor->priv->dtstart, task_editor->priv->due_date,
		TRUE);

	e_comp_editor_set_updating (comp_editor, FALSE);

	ece_task_check_dates_in_the_past (task_editor);
}

static void
ece_task_due_date_changed_cb (EDateEdit *date_edit,
			      ECompEditorTask *task_editor)
{
	ECompEditor *comp_editor;

	g_return_if_fail (E_IS_DATE_EDIT (date_edit));
	g_return_if_fail (E_IS_COMP_EDITOR_TASK (task_editor));

	comp_editor = E_COMP_EDITOR (task_editor);

	if (e_comp_editor_get_updating (comp_editor))
		return;

	e_comp_editor_set_updating (comp_editor, TRUE);

	e_comp_editor_ensure_start_before_end (E_COMP_EDITOR (task_editor),
		task_editor->priv->dtstart, task_editor->priv->due_date,
		FALSE);

	e_comp_editor_set_updating (comp_editor, FALSE);

	ece_task_check_dates_in_the_past (task_editor);
}

static void
ece_task_completed_date_changed_cb (EDateEdit *date_edit,
				    ECompEditorTask *task_editor)
{
	GtkSpinButton *percent_spin;
	ECompEditor *comp_editor;
	struct icaltimetype itt;
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

	if (icaltime_is_null_time (itt)) {
		if (status == ICAL_STATUS_COMPLETED) {
			e_comp_editor_property_part_picker_with_map_set_selected (
				E_COMP_EDITOR_PROPERTY_PART_PICKER_WITH_MAP (task_editor->priv->status),
				ICAL_STATUS_NONE);

			gtk_spin_button_set_value (percent_spin, 0);
		}
	} else {
		if (status != ICAL_STATUS_COMPLETED) {
			e_comp_editor_property_part_picker_with_map_set_selected (
				E_COMP_EDITOR_PROPERTY_PART_PICKER_WITH_MAP (task_editor->priv->status),
				ICAL_STATUS_COMPLETED);
		}

		gtk_spin_button_set_value (percent_spin, 100);
	}

	e_comp_editor_set_updating (comp_editor, FALSE);
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

	if (status == ICAL_STATUS_NONE) {
		gtk_spin_button_set_value (percent_spin, 0);
		e_date_edit_set_time (completed_date, (time_t) -1);
	} else if (status == ICAL_STATUS_INPROCESS) {
		gint percent_complete = gtk_spin_button_get_value_as_int (percent_spin);

		if (percent_complete <= 0 || percent_complete >= 100)
			gtk_spin_button_set_value (percent_spin, 50);

		e_date_edit_set_time (completed_date, (time_t) -1);
	} else if (status == ICAL_STATUS_COMPLETED) {
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
		status = ICAL_STATUS_COMPLETED;
	} else {
		ctime = (time_t) -1;

		if (percent == 0)
			status = ICAL_STATUS_NONE;
		else
			status = ICAL_STATUS_INPROCESS;
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
	GtkAction *action;
	gboolean is_organizer;
	guint32 flags;

	g_return_if_fail (E_IS_COMP_EDITOR_TASK (comp_editor));

	E_COMP_EDITOR_CLASS (e_comp_editor_task_parent_class)->sensitize_widgets (comp_editor, force_insensitive);

	flags = e_comp_editor_get_flags (comp_editor);
	is_organizer = (flags & (E_COMP_EDITOR_FLAG_IS_NEW | E_COMP_EDITOR_FLAG_ORGANIZER_IS_USER)) != 0;
	task_editor = E_COMP_EDITOR_TASK (comp_editor);

	action = e_comp_editor_get_action (comp_editor, "all-day-task");
	gtk_action_set_sensitive (action, !force_insensitive && is_organizer);

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
			message = _("Task cannot be fully edited, because you are not the organizer");

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
		       icalcomponent *component)
{
	gboolean force_allday = FALSE;

	g_return_if_fail (E_IS_COMP_EDITOR_TASK (comp_editor));
	g_return_if_fail (component != NULL);

	E_COMP_EDITOR_CLASS (e_comp_editor_task_parent_class)->fill_widgets (comp_editor, component);

	ece_task_update_timezone (E_COMP_EDITOR_TASK (comp_editor), &force_allday);

	if (force_allday) {
		GtkAction *action;

		action = e_comp_editor_get_action (comp_editor, "all-day-task");
		gtk_toggle_action_set_active (GTK_TOGGLE_ACTION (action), TRUE);
	}
}

static gboolean
ece_task_fill_component (ECompEditor *comp_editor,
			 icalcomponent *component)
{
	ECompEditorTask *task_editor;
	struct icaltimetype itt;

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

		return FALSE;
	}

	return E_COMP_EDITOR_CLASS (e_comp_editor_task_parent_class)->fill_component (comp_editor, component);
}

static void
ece_task_setup_ui (ECompEditorTask *task_editor)
{
	const gchar *ui =
		"<ui>"
		"  <menubar action='main-menu'>"
		"    <menu action='view-menu'>"
		"      <placeholder name='parts'>"
		"        <menuitem action='view-timezone'/>"
		"        <menuitem action='view-categories'/>"
		"      </placeholder>"
		"    </menu>"
		"    <menu action='options-menu'>"
		"      <placeholder name='toggles'>"
		"        <menuitem action='all-day-task'/>"
		"      </placeholder>"
		"    </menu>"
		"  </menubar>"
		"  <toolbar name='main-toolbar'>"
		"    <placeholder name='content'>\n"
		"      <toolitem action='all-day-task'/>\n"
		"    </placeholder>"
		"  </toolbar>"
		"</ui>";

	const GtkToggleActionEntry view_actions[] = {
		{ "view-categories",
		  NULL,
		  N_("_Categories"),
		  NULL,
		  N_("Toggles whether to display categories"),
		  NULL,
		  FALSE },

		{ "view-timezone",
		  "stock_timezone",
		  N_("Time _Zone"),
		  NULL,
		  N_("Toggles whether the time zone is displayed"),
		  NULL,
		  FALSE },

		{ "all-day-task",
		  "stock_new-24h-appointment",
		  N_("All _Day Task"),
		  NULL,
		  N_("Toggles whether to have All Day Task"),
		  NULL,
		  FALSE }
	};

	ECompEditor *comp_editor;
	GSettings *settings;
	GtkUIManager *ui_manager;
	GtkAction *action;
	GtkActionGroup *action_group;
	GtkWidget *edit_widget;
	GError *error = NULL;

	g_return_if_fail (E_IS_COMP_EDITOR_TASK (task_editor));

	comp_editor = E_COMP_EDITOR (task_editor);
	settings = e_comp_editor_get_settings (comp_editor);
	ui_manager = e_comp_editor_get_ui_manager (comp_editor);
	action_group = e_comp_editor_get_action_group (comp_editor, "individual");

	gtk_action_group_add_toggle_actions (action_group,
		view_actions, G_N_ELEMENTS (view_actions), task_editor);

	gtk_ui_manager_add_ui_from_string (ui_manager, ui, -1, &error);

	e_plugin_ui_register_manager (ui_manager, "org.gnome.evolution.task-editor", task_editor);
	e_plugin_ui_enable_manager (ui_manager, "org.gnome.evolution.task-editor");

	if (error) {
		g_critical ("%s: %s", G_STRFUNC, error->message);
		g_error_free (error);
	}

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
}

static void
e_comp_editor_task_constructed (GObject *object)
{
	ECompEditorTask *task_editor;
	ECompEditor *comp_editor;
	ECompEditorPage *page;
	ECompEditorPropertyPart *part, *summary;
	EFocusTracker *focus_tracker;
	GtkWidget *edit_widget;

	G_OBJECT_CLASS (e_comp_editor_task_parent_class)->constructed (object);

	task_editor = E_COMP_EDITOR_TASK (object);
	comp_editor = E_COMP_EDITOR (task_editor);
	focus_tracker = e_comp_editor_get_focus_tracker (comp_editor);

	page = e_comp_editor_page_general_new (comp_editor,
		_("_List:"), E_SOURCE_EXTENSION_TASK_LIST,
		NULL, FALSE, 3);

	part = e_comp_editor_property_part_summary_new (focus_tracker);
	e_comp_editor_page_add_property_part (page, part, 0, 2, 4, 1);
	summary = part;

	part = e_comp_editor_property_part_dtstart_new (C_("ECompEditor", "Sta_rt date:"), TRUE, TRUE);
	e_comp_editor_page_add_property_part (page, part, 0, 3, 2, 1);
	task_editor->priv->dtstart = part;

	edit_widget = e_comp_editor_property_part_get_edit_widget (part);
	g_signal_connect (edit_widget, "changed", G_CALLBACK (ece_task_dtstart_changed_cb), task_editor);

	part = e_comp_editor_property_part_status_new ();
	e_comp_editor_page_add_property_part (page, part, 2, 3, 2, 1);
	task_editor->priv->status = part;

	edit_widget = e_comp_editor_property_part_get_edit_widget (part);
	g_signal_connect (edit_widget, "changed", G_CALLBACK (ece_task_status_changed_cb), task_editor);

	part = e_comp_editor_property_part_due_new (TRUE, TRUE);
	e_comp_editor_page_add_property_part (page, part, 0, 4, 2, 1);
	task_editor->priv->due_date = part;

	edit_widget = e_comp_editor_property_part_get_edit_widget (part);
	g_signal_connect (edit_widget, "changed", G_CALLBACK (ece_task_due_date_changed_cb), task_editor);

	part = e_comp_editor_property_part_priority_new ();
	e_comp_editor_page_add_property_part (page, part, 2, 4, 2, 1);

	part = e_comp_editor_property_part_completed_new (TRUE, TRUE);
	e_comp_editor_page_add_property_part (page, part, 0, 5, 2, 1);
	task_editor->priv->completed_date = part;

	edit_widget = e_comp_editor_property_part_get_edit_widget (part);
	g_signal_connect (edit_widget, "changed", G_CALLBACK (ece_task_completed_date_changed_cb), task_editor);

	part = e_comp_editor_property_part_percentcomplete_new ();
	e_comp_editor_page_add_property_part (page, part, 2, 5, 2, 1);
	task_editor->priv->percentcomplete = part;

	edit_widget = e_comp_editor_property_part_get_edit_widget (part);
	g_signal_connect (edit_widget, "value-changed", G_CALLBACK (ece_task_percentcomplete_value_changed_cb), task_editor);

	part = e_comp_editor_property_part_url_new (focus_tracker);
	e_comp_editor_page_add_property_part (page, part, 0, 6, 2, 1);

	edit_widget = e_comp_editor_property_part_get_edit_widget (part);
	gtk_widget_set_hexpand (edit_widget, TRUE);

	part = e_comp_editor_property_part_classification_new ();
	e_comp_editor_page_add_property_part (page, part, 2, 6, 2, 1);

	part = e_comp_editor_property_part_timezone_new ();
	e_comp_editor_page_add_property_part (page, part, 0, 7, 4, 1);
	task_editor->priv->timezone = part;

	part = e_comp_editor_property_part_categories_new (focus_tracker);
	e_comp_editor_page_add_property_part (page, part, 0, 8, 4, 1);
	task_editor->priv->categories = part;

	part = e_comp_editor_property_part_description_new (focus_tracker);
	e_comp_editor_page_add_property_part (page, part, 0, 9, 4, 1);

	e_comp_editor_add_page (comp_editor, C_("ECompEditorPage", "General"), page);
	task_editor->priv->page_general = page;

	edit_widget = e_comp_editor_property_part_get_edit_widget (task_editor->priv->timezone);
	e_comp_editor_property_part_datetime_attach_timezone_entry (
		E_COMP_EDITOR_PROPERTY_PART_DATETIME (task_editor->priv->dtstart),
		E_TIMEZONE_ENTRY (edit_widget));
	e_comp_editor_property_part_datetime_attach_timezone_entry (
		E_COMP_EDITOR_PROPERTY_PART_DATETIME (task_editor->priv->due_date),
		E_TIMEZONE_ENTRY (edit_widget));
	e_comp_editor_property_part_datetime_attach_timezone_entry (
		E_COMP_EDITOR_PROPERTY_PART_DATETIME (task_editor->priv->completed_date),
		E_TIMEZONE_ENTRY (edit_widget));

	e_comp_editor_set_time_parts (comp_editor, task_editor->priv->dtstart, task_editor->priv->due_date);

	page = e_comp_editor_page_reminders_new (comp_editor);
	e_comp_editor_add_page (comp_editor, C_("ECompEditorPage", "Reminders"), page);

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
}

static void
e_comp_editor_task_init (ECompEditorTask *task_editor)
{
	task_editor->priv = G_TYPE_INSTANCE_GET_PRIVATE (task_editor, E_TYPE_COMP_EDITOR_TASK, ECompEditorTaskPrivate);
}

static void
e_comp_editor_task_class_init (ECompEditorTaskClass *klass)
{
	GObjectClass *object_class;
	ECompEditorClass *comp_editor_class;

	g_type_class_add_private (klass, sizeof (ECompEditorTaskPrivate));

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
