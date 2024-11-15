/*
 * e-cal-shell-view-taskpad.c
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
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#include "evolution-config.h"

#include "calendar/gui/e-cal-ops.h"
#include "calendar/gui/itip-utils.h"

#include "e-cal-shell-view-private.h"

/* Much of this file is based on e-task-shell-view-actions.c. */

static void
action_calendar_taskpad_assign_cb (EUIAction *action,
				   GVariant *parameter,
				   gpointer user_data)
{
	ECalShellView *cal_shell_view = user_data;
	ECalShellContent *cal_shell_content;
	ECalModelComponent *comp_data;
	ECalModel *model;
	ETaskTable *task_table;
	EShellContent *shell_content;
	GSList *list;

	g_return_if_fail (E_IS_CAL_SHELL_VIEW (cal_shell_view));

	cal_shell_content = cal_shell_view->priv->cal_shell_content;
	task_table = e_cal_shell_content_get_task_table (cal_shell_content);

	list = e_task_table_get_selected (task_table);
	g_return_if_fail (list != NULL);
	comp_data = list->data;
	g_slist_free (list);

	g_return_if_fail (E_IS_CAL_MODEL_COMPONENT (comp_data));

	/* XXX We only open the first selected task. */
	shell_content = e_shell_view_get_shell_content (E_SHELL_VIEW (cal_shell_view));
	model = e_cal_base_shell_content_get_model (E_CAL_BASE_SHELL_CONTENT (shell_content));
	e_cal_ops_open_component_in_editor_sync	(model, comp_data->client, comp_data->icalcomp, TRUE);
}

static void
action_calendar_taskpad_forward_cb (EUIAction *action,
				    GVariant *parameter,
				    gpointer user_data)
{
	ECalShellView *cal_shell_view = user_data;
	ECalShellContent *cal_shell_content;
	ECalModelComponent *comp_data;
	ETaskTable *task_table;
	ECalComponent *comp;
	GSList *list;

	cal_shell_content = cal_shell_view->priv->cal_shell_content;
	task_table = e_cal_shell_content_get_task_table (cal_shell_content);

	list = e_task_table_get_selected (task_table);
	g_return_if_fail (list != NULL);
	comp_data = list->data;
	g_slist_free (list);

	/* XXX We only forward the first selected task. */
	comp = e_cal_component_new_from_icalcomponent (i_cal_component_clone (comp_data->icalcomp));
	g_return_if_fail (comp != NULL);

	itip_send_component_with_model (e_cal_model_get_data_model (e_task_table_get_model (task_table)),
		I_CAL_METHOD_PUBLISH, comp, comp_data->client,
		NULL, NULL, NULL, E_ITIP_SEND_COMPONENT_FLAG_STRIP_ALARMS | E_ITIP_SEND_COMPONENT_FLAG_ENSURE_MASTER_OBJECT);

	g_object_unref (comp);
}

static void
action_calendar_taskpad_mark_complete_cb (EUIAction *action,
					  GVariant *parameter,
					  gpointer user_data)
{
	ECalShellView *cal_shell_view = user_data;
	ECalShellContent *cal_shell_content;
	ETaskTable *task_table;
	ECalModel *model;
	GSList *list, *iter;

	cal_shell_content = cal_shell_view->priv->cal_shell_content;
	task_table = e_cal_shell_content_get_task_table (cal_shell_content);
	list = e_task_table_get_selected (task_table);
	model = e_task_table_get_model (task_table);

	for (iter = list; iter != NULL; iter = iter->next) {
		ECalModelComponent *comp_data = iter->data;
		e_cal_model_tasks_mark_comp_complete (
			E_CAL_MODEL_TASKS (model), comp_data);
	}

	g_slist_free (list);
}

static void
action_calendar_taskpad_mark_incomplete_cb (EUIAction *action,
					    GVariant *parameter,
					    gpointer user_data)
{
	ECalShellView *cal_shell_view = user_data;
	ECalShellContent *cal_shell_content;
	ETaskTable *task_table;
	ECalModel *model;
	GSList *list, *iter;

	cal_shell_content = cal_shell_view->priv->cal_shell_content;
	task_table = e_cal_shell_content_get_task_table (cal_shell_content);
	list = e_task_table_get_selected (task_table);
	model = e_task_table_get_model (task_table);

	for (iter = list; iter != NULL; iter = iter->next) {
		ECalModelComponent *comp_data = iter->data;
		e_cal_model_tasks_mark_comp_incomplete (
			E_CAL_MODEL_TASKS (model), comp_data);
	}

	g_slist_free (list);
}

static void
action_calendar_taskpad_new_cb (EUIAction *action,
				GVariant *parameter,
				gpointer user_data)
{
	ECalShellView *cal_shell_view = user_data;
	EShellView *shell_view;
	EShellWindow *shell_window;
	ECalShellContent *cal_shell_content;
	ECalModelComponent *comp_data;
	ETaskTable *task_table;
	GSList *list;

	shell_view = E_SHELL_VIEW (cal_shell_view);
	shell_window = e_shell_view_get_shell_window (shell_view);

	cal_shell_content = cal_shell_view->priv->cal_shell_content;
	task_table = e_cal_shell_content_get_task_table (cal_shell_content);

	list = e_task_table_get_selected (task_table);
	g_return_if_fail (list != NULL);
	comp_data = list->data;
	g_slist_free (list);

	e_cal_ops_new_component_editor (shell_window, E_CAL_CLIENT_SOURCE_TYPE_TASKS,
		e_source_get_uid (e_client_get_source (E_CLIENT (comp_data->client))), FALSE);
}

static void
action_calendar_taskpad_open_cb (EUIAction *action,
				 GVariant *parameter,
				 gpointer user_data)
{
	ECalShellView *cal_shell_view = user_data;
	ECalShellContent *cal_shell_content;
	ECalModelComponent *comp_data;
	ETaskTable *task_table;
	GSList *list;

	cal_shell_content = cal_shell_view->priv->cal_shell_content;
	task_table = e_cal_shell_content_get_task_table (cal_shell_content);

	list = e_task_table_get_selected (task_table);
	g_return_if_fail (list != NULL);
	comp_data = list->data;
	g_slist_free (list);

	/* XXX We only open the first selected task. */
	e_cal_shell_view_taskpad_open_task (cal_shell_view, comp_data);
}

static void
action_calendar_taskpad_open_url_cb (EUIAction *action,
				     GVariant *parameter,
				     gpointer user_data)
{
	ECalShellView *cal_shell_view = user_data;
	EShellView *shell_view;
	EShellWindow *shell_window;
	ECalShellContent *cal_shell_content;
	ECalModelComponent *comp_data;
	ETaskTable *task_table;
	ICalProperty *prop;
	const gchar *uri;
	GSList *list;

	shell_view = E_SHELL_VIEW (cal_shell_view);
	shell_window = e_shell_view_get_shell_window (shell_view);

	cal_shell_content = cal_shell_view->priv->cal_shell_content;
	task_table = e_cal_shell_content_get_task_table (cal_shell_content);

	list = e_task_table_get_selected (task_table);
	g_return_if_fail (list != NULL);
	comp_data = list->data;

	/* XXX We only open the URI of the first selected task. */
	prop = i_cal_component_get_first_property (comp_data->icalcomp, I_CAL_URL_PROPERTY);
	g_return_if_fail (prop != NULL);

	uri = i_cal_property_get_url (prop);
	e_show_uri (GTK_WINDOW (shell_window), uri);
	g_object_unref (prop);
}

static void
action_calendar_taskpad_print_cb (EUIAction *action,
				  GVariant *parameter,
				  gpointer user_data)
{
	ECalShellView *cal_shell_view = user_data;
	ECalShellContent *cal_shell_content;
	ECalModelComponent *comp_data;
	ETaskTable *task_table;
	ECalComponent *comp;
	ECalModel *model;
	GSList *list;

	cal_shell_content = cal_shell_view->priv->cal_shell_content;
	task_table = e_cal_shell_content_get_task_table (cal_shell_content);
	model = e_task_table_get_model (task_table);

	list = e_task_table_get_selected (task_table);
	g_return_if_fail (list != NULL);
	comp_data = list->data;
	g_slist_free (list);

	/* XXX We only print the first selected task. */
	comp = e_cal_component_new_from_icalcomponent (i_cal_component_clone (comp_data->icalcomp));

	print_comp (
		comp, comp_data->client,
		e_cal_model_get_timezone (model),
		e_cal_model_get_use_24_hour_format (model),
		GTK_PRINT_OPERATION_ACTION_PRINT_DIALOG);

	g_object_unref (comp);
}

static void
action_calendar_taskpad_save_as_cb (EUIAction *action,
				    GVariant *parameter,
				    gpointer user_data)
{
	ECalShellView *cal_shell_view = user_data;
	EShell *shell;
	EShellView *shell_view;
	EShellWindow *shell_window;
	EShellBackend *shell_backend;
	ECalShellContent *cal_shell_content;
	ECalModelComponent *comp_data;
	ETaskTable *task_table;
	EActivity *activity;
	GSList *list;
	GFile *file;
	gchar *string;

	shell_view = E_SHELL_VIEW (cal_shell_view);
	shell_window = e_shell_view_get_shell_window (shell_view);
	shell_backend = e_shell_view_get_shell_backend (shell_view);
	shell = e_shell_window_get_shell (shell_window);

	cal_shell_content = cal_shell_view->priv->cal_shell_content;
	task_table = e_cal_shell_content_get_task_table (cal_shell_content);

	list = e_task_table_get_selected (task_table);
	g_return_if_fail (list != NULL);
	comp_data = list->data;
	g_slist_free (list);

	/* Translators: Default filename part saving a task to a file when
	 * no summary is filed, the '.ics' extension is concatenated to it. */
	string = comp_util_suggest_filename (comp_data->icalcomp, _("task"));
	file = e_shell_run_save_dialog (
		shell, _("Save as iCalendar"), string,
		"*.ics:text/calendar", NULL, NULL);
	g_free (string);
	if (file == NULL)
		return;

	string = e_cal_client_get_component_as_string (
		comp_data->client, comp_data->icalcomp);
	if (string == NULL) {
		g_warning ("Could not convert task to a string");
		g_object_unref (file);
		return;
	}

	/* XXX No callback means errors are discarded. */
	activity = e_file_replace_contents_async (
		file, string, strlen (string), NULL, FALSE,
		G_FILE_CREATE_NONE, (GAsyncReadyCallback) NULL, NULL);
	e_shell_backend_add_activity (shell_backend, activity);

	/* Free the string when the activity is finalized. */
	g_object_set_data_full (
		G_OBJECT (activity),
		"file-content", string,
		(GDestroyNotify) g_free);

	g_object_unref (file);
}

void
e_cal_shell_view_taskpad_actions_init (ECalShellView *self)
{
	static const EUIActionEntry calendar_taskpad_entries[] = {

		{ "calendar-taskpad-assign",
		  "stock_task-assigned-to",
		  N_("_Assign Task"),
		  NULL,
		  NULL,
		  action_calendar_taskpad_assign_cb, NULL, NULL, NULL },

		{ "calendar-taskpad-forward",
		  "mail-forward",
		  N_("_Forward as iCalendar…"),
		  NULL,
		  NULL,
		  action_calendar_taskpad_forward_cb, NULL, NULL, NULL },

		{ "calendar-taskpad-mark-complete",
		  NULL,
		  N_("_Mark as Complete"),
		  NULL,
		  N_("Mark selected tasks as complete"),
		  action_calendar_taskpad_mark_complete_cb, NULL, NULL, NULL },

		{ "calendar-taskpad-mark-incomplete",
		  NULL,
		  N_("_Mark as Incomplete"),
		  NULL,
		  N_("Mark selected tasks as incomplete"),
		  action_calendar_taskpad_mark_incomplete_cb, NULL, NULL, NULL },

		{ "calendar-taskpad-new",
		  "stock_task",
		  N_("New _Task"),
		  NULL,
		  N_("Create a new task"),
		  action_calendar_taskpad_new_cb, NULL, NULL, NULL },

		{ "calendar-taskpad-open",
		  "document-open",
		  N_("_Open Task"),
		  "<Control>o",
		  N_("View the selected task"),
		  action_calendar_taskpad_open_cb, NULL, NULL, NULL },

		{ "calendar-taskpad-open-url",
		  "applications-internet",
		  N_("Open _Web Page"),
		  NULL,
		  NULL,
		  action_calendar_taskpad_open_url_cb, NULL, NULL, NULL },
	};

	static const EUIActionEntry lockdown_printing_entries[] = {

		{ "calendar-taskpad-print",
		  "document-print",
		  N_("Print…"),
		  NULL,
		  N_("Print the selected task"),
		  action_calendar_taskpad_print_cb, NULL, NULL, NULL }
	};

	static const EUIActionEntry lockdown_save_to_disk_entries[] = {

		{ "calendar-taskpad-save-as",
		  "document-save-as",
		  N_("_Save as iCalendar…"),
		  NULL,
		  NULL,
		  action_calendar_taskpad_save_as_cb, NULL, NULL, NULL }
	};

	EShellView *shell_view;
	EUIManager *ui_manager;

	shell_view = E_SHELL_VIEW (self);
	ui_manager = e_shell_view_get_ui_manager (shell_view);

	/* Calendar Actions */
	e_ui_manager_add_actions (ui_manager, "calendar", NULL,
		calendar_taskpad_entries, G_N_ELEMENTS (calendar_taskpad_entries), self);

	/* Lockdown Printing Actions */
	e_ui_manager_add_actions (ui_manager, "lockdown-printing", NULL,
		lockdown_printing_entries, G_N_ELEMENTS (lockdown_printing_entries), self);

	/* Lockdown Save-to-Disk Actions */
	e_ui_manager_add_actions (ui_manager, "lockdown-save-to-disk", NULL,
		lockdown_save_to_disk_entries, G_N_ELEMENTS (lockdown_save_to_disk_entries), self);
}

void
e_cal_shell_view_taskpad_actions_update (ECalShellView *cal_shell_view)
{
	ECalShellContent *cal_shell_content;
	EShellView *shell_view;
	ETaskTable *task_table;
	EUIAction *action;
	GSList *list, *iter;
	gboolean assignable = TRUE;
	gboolean editable = TRUE;
	gboolean has_url = FALSE;
	gboolean sensitive;
	gint n_selected;
	gint n_complete = 0;
	gint n_incomplete = 0;

	shell_view = E_SHELL_VIEW (cal_shell_view);

	cal_shell_content = cal_shell_view->priv->cal_shell_content;
	task_table = e_cal_shell_content_get_task_table (cal_shell_content);

	n_selected = e_table_selected_count (E_TABLE (task_table));

	list = e_task_table_get_selected (task_table);
	for (iter = list; iter != NULL; iter = iter->next) {
		ECalModelComponent *comp_data = iter->data;
		const gchar *cap;
		gboolean read_only;

		read_only = e_client_is_readonly (E_CLIENT (comp_data->client));
		editable &= !read_only;

		cap = E_CAL_STATIC_CAPABILITY_NO_TASK_ASSIGNMENT;
		if (e_client_check_capability (E_CLIENT (comp_data->client), cap))
			assignable = FALSE;

		cap = E_CAL_STATIC_CAPABILITY_NO_CONV_TO_ASSIGN_TASK;
		if (e_client_check_capability (E_CLIENT (comp_data->client), cap))
			assignable = FALSE;

		has_url |= e_cal_util_component_has_property (comp_data->icalcomp, I_CAL_URL_PROPERTY);

		if (e_cal_util_component_has_property (comp_data->icalcomp, I_CAL_COMPLETED_PROPERTY))
			n_complete++;
		else
			n_incomplete++;
	}
	g_slist_free (list);

	action = ACTION (CALENDAR_TASKPAD_ASSIGN);
	sensitive = (n_selected == 1) && editable && assignable;
	e_ui_action_set_visible (action, sensitive);

	action = ACTION (CALENDAR_TASKPAD_FORWARD);
	sensitive = (n_selected == 1);
	e_ui_action_set_visible (action, sensitive);

	action = ACTION (CALENDAR_TASKPAD_MARK_COMPLETE);
	sensitive = (n_selected > 0) && editable && (n_incomplete > 0);
	e_ui_action_set_visible (action, sensitive);

	action = ACTION (CALENDAR_TASKPAD_MARK_INCOMPLETE);
	sensitive = (n_selected > 0) && editable && (n_complete > 0);
	e_ui_action_set_visible (action, sensitive);

	action = ACTION (CALENDAR_TASKPAD_OPEN);
	sensitive = (n_selected == 1);
	e_ui_action_set_visible (action, sensitive);

	action = ACTION (CALENDAR_TASKPAD_OPEN_URL);
	sensitive = (n_selected == 1) && has_url;
	e_ui_action_set_visible (action, sensitive);

	action = ACTION (CALENDAR_TASKPAD_PRINT);
	sensitive = (n_selected == 1);
	e_ui_action_set_visible (action, sensitive);

	action = ACTION (CALENDAR_TASKPAD_SAVE_AS);
	sensitive = (n_selected == 1);
	e_ui_action_set_visible (action, sensitive);
}

void
e_cal_shell_view_taskpad_open_task (ECalShellView *cal_shell_view,
                                    ECalModelComponent *comp_data)
{
	EShellContent *shell_content;
	ECalModel *model;

	g_return_if_fail (E_IS_CAL_SHELL_VIEW (cal_shell_view));
	g_return_if_fail (E_IS_CAL_MODEL_COMPONENT (comp_data));

	shell_content = e_shell_view_get_shell_content (E_SHELL_VIEW (cal_shell_view));
	model = e_cal_base_shell_content_get_model (E_CAL_BASE_SHELL_CONTENT (shell_content));

	e_cal_ops_open_component_in_editor_sync	(model, comp_data->client, comp_data->icalcomp, FALSE);
}
