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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "calendar/gui/e-cal-ops.h"
#include "calendar/gui/itip-utils.h"

#include "e-cal-shell-view-private.h"

/* Much of this file is based on e-task-shell-view-actions.c. */

static void
action_calendar_taskpad_assign_cb (GtkAction *action,
                                   ECalShellView *cal_shell_view)
{
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

	/* FIXME Need to actually assign the task. */
}

static void
action_calendar_taskpad_forward_cb (GtkAction *action,
                                    ECalShellView *cal_shell_view)
{
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
	comp = e_cal_component_new_from_icalcomponent (icalcomponent_new_clone (comp_data->icalcomp));
	g_return_if_fail (comp != NULL);

	itip_send_component_with_model (e_task_table_get_model (task_table),
		E_CAL_COMPONENT_METHOD_PUBLISH, comp, comp_data->client,
		NULL, NULL, NULL, TRUE, FALSE, TRUE);

	g_object_unref (comp);
}

static void
action_calendar_taskpad_mark_complete_cb (GtkAction *action,
                                          ECalShellView *cal_shell_view)
{
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
action_calendar_taskpad_mark_incomplete_cb (GtkAction *action,
                                            ECalShellView *cal_shell_view)
{
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
action_calendar_taskpad_new_cb (GtkAction *action,
                                ECalShellView *cal_shell_view)
{
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
action_calendar_taskpad_open_cb (GtkAction *action,
                                 ECalShellView *cal_shell_view)
{
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
action_calendar_taskpad_open_url_cb (GtkAction *action,
                                     ECalShellView *cal_shell_view)
{
	EShellView *shell_view;
	EShellWindow *shell_window;
	ECalShellContent *cal_shell_content;
	ECalModelComponent *comp_data;
	ETaskTable *task_table;
	icalproperty *prop;
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
	prop = icalcomponent_get_first_property (
		comp_data->icalcomp, ICAL_URL_PROPERTY);
	g_return_if_fail (prop != NULL);

	uri = icalproperty_get_url (prop);
	e_show_uri (GTK_WINDOW (shell_window), uri);
}

static void
action_calendar_taskpad_print_cb (GtkAction *action,
                                  ECalShellView *cal_shell_view)
{
	ECalShellContent *cal_shell_content;
	ECalModelComponent *comp_data;
	ETaskTable *task_table;
	ECalComponent *comp;
	ECalModel *model;
	icalcomponent *clone;
	GSList *list;

	cal_shell_content = cal_shell_view->priv->cal_shell_content;
	task_table = e_cal_shell_content_get_task_table (cal_shell_content);
	model = e_task_table_get_model (task_table);

	list = e_task_table_get_selected (task_table);
	g_return_if_fail (list != NULL);
	comp_data = list->data;
	g_slist_free (list);

	/* XXX We only print the first selected task. */
	comp = e_cal_component_new ();
	clone = icalcomponent_new_clone (comp_data->icalcomp);
	e_cal_component_set_icalcomponent (comp, clone);

	print_comp (
		comp, comp_data->client,
		e_cal_model_get_timezone (model),
		e_cal_model_get_use_24_hour_format (model),
		GTK_PRINT_OPERATION_ACTION_PRINT_DIALOG);

	g_object_unref (comp);
}

static void
action_calendar_taskpad_save_as_cb (GtkAction *action,
                                    ECalShellView *cal_shell_view)
{
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
	string = icalcomp_suggest_filename (comp_data->icalcomp, _("task"));
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

static GtkActionEntry calendar_taskpad_entries[] = {

	{ "calendar-taskpad-assign",
	  NULL,
	  N_("_Assign Task"),
	  NULL,
	  NULL,  /* XXX Add a tooltip! */
	  G_CALLBACK (action_calendar_taskpad_assign_cb) },

	{ "calendar-taskpad-forward",
	  "mail-forward",
	  N_("_Forward as iCalendar..."),
	  NULL,
	  NULL,  /* XXX Add a tooltip! */
	  G_CALLBACK (action_calendar_taskpad_forward_cb) },

	{ "calendar-taskpad-mark-complete",
	  NULL,
	  N_("_Mark as Complete"),
	  NULL,
	  N_("Mark selected tasks as complete"),
	  G_CALLBACK (action_calendar_taskpad_mark_complete_cb) },

	{ "calendar-taskpad-mark-incomplete",
	  NULL,
	  N_("_Mark as Incomplete"),
	  NULL,
	  N_("Mark selected tasks as incomplete"),
	  G_CALLBACK (action_calendar_taskpad_mark_incomplete_cb) },

	{ "calendar-taskpad-new",
	  "stock_task",
	  N_("New _Task"),
	  NULL,
	  N_("Create a new task"),
	  G_CALLBACK (action_calendar_taskpad_new_cb) },

	{ "calendar-taskpad-open",
	  "document-open",
	  N_("_Open Task"),
	  "<Control>o",
	  N_("View the selected task"),
	  G_CALLBACK (action_calendar_taskpad_open_cb) },

	{ "calendar-taskpad-open-url",
	  "applications-internet",
	  N_("Open _Web Page"),
	  NULL,
	  NULL,  /* XXX Add a tooltip! */
	  G_CALLBACK (action_calendar_taskpad_open_url_cb) },
};

static GtkActionEntry lockdown_printing_entries[] = {

	{ "calendar-taskpad-print",
	  "document-print",
	  N_("Print..."),
	  NULL,
	  N_("Print the selected task"),
	  G_CALLBACK (action_calendar_taskpad_print_cb) }
};

static GtkActionEntry lockdown_save_to_disk_entries[] = {

	{ "calendar-taskpad-save-as",
	  "document-save-as",
	  N_("_Save as iCalendar..."),
	  NULL,
	  NULL,  /* XXX Add a tooltip! */
	  G_CALLBACK (action_calendar_taskpad_save_as_cb) }
};

void
e_cal_shell_view_taskpad_actions_init (ECalShellView *cal_shell_view)
{
	EShellView *shell_view;
	EShellWindow *shell_window;
	GtkActionGroup *action_group;

	shell_view = E_SHELL_VIEW (cal_shell_view);
	shell_window = e_shell_view_get_shell_window (shell_view);

	/* Calendar Actions */
	action_group = ACTION_GROUP (CALENDAR);
	gtk_action_group_add_actions (
		action_group, calendar_taskpad_entries,
		G_N_ELEMENTS (calendar_taskpad_entries), cal_shell_view);

	/* Lockdown Printing Actions */
	action_group = ACTION_GROUP (LOCKDOWN_PRINTING);
	gtk_action_group_add_actions (
		action_group, lockdown_printing_entries,
		G_N_ELEMENTS (lockdown_printing_entries), cal_shell_view);

	/* Lockdown Save-to-Disk Actions */
	action_group = ACTION_GROUP (LOCKDOWN_SAVE_TO_DISK);
	gtk_action_group_add_actions (
		action_group, lockdown_save_to_disk_entries,
		G_N_ELEMENTS (lockdown_save_to_disk_entries), cal_shell_view);
}

void
e_cal_shell_view_taskpad_actions_update (ECalShellView *cal_shell_view)
{
	ECalShellContent *cal_shell_content;
	EShellWindow *shell_window;
	EShellView *shell_view;
	ETaskTable *task_table;
	GtkAction *action;
	GSList *list, *iter;
	gboolean assignable = TRUE;
	gboolean editable = TRUE;
	gboolean has_url = FALSE;
	gboolean sensitive;
	gint n_selected;
	gint n_complete = 0;
	gint n_incomplete = 0;

	shell_view = E_SHELL_VIEW (cal_shell_view);
	shell_window = e_shell_view_get_shell_window (shell_view);

	cal_shell_content = cal_shell_view->priv->cal_shell_content;
	task_table = e_cal_shell_content_get_task_table (cal_shell_content);

	n_selected = e_table_selected_count (E_TABLE (task_table));

	list = e_task_table_get_selected (task_table);
	for (iter = list; iter != NULL; iter = iter->next) {
		ECalModelComponent *comp_data = iter->data;
		icalproperty *prop;
		const gchar *cap;
		gboolean read_only;

		read_only = e_client_is_readonly (E_CLIENT (comp_data->client));
		editable &= !read_only;

		cap = CAL_STATIC_CAPABILITY_NO_TASK_ASSIGNMENT;
		if (e_client_check_capability (E_CLIENT (comp_data->client), cap))
			assignable = FALSE;

		cap = CAL_STATIC_CAPABILITY_NO_CONV_TO_ASSIGN_TASK;
		if (e_client_check_capability (E_CLIENT (comp_data->client), cap))
			assignable = FALSE;

		prop = icalcomponent_get_first_property (
			comp_data->icalcomp, ICAL_URL_PROPERTY);
		has_url |= (prop != NULL);

		prop = icalcomponent_get_first_property (
			comp_data->icalcomp, ICAL_COMPLETED_PROPERTY);
		if (prop != NULL)
			n_complete++;
		else
			n_incomplete++;
	}
	g_slist_free (list);

	action = ACTION (CALENDAR_TASKPAD_ASSIGN);
	sensitive = (n_selected == 1) && editable && assignable;
	gtk_action_set_sensitive (action, sensitive);

	action = ACTION (CALENDAR_TASKPAD_FORWARD);
	sensitive = (n_selected == 1);
	gtk_action_set_sensitive (action, sensitive);

	action = ACTION (CALENDAR_TASKPAD_MARK_COMPLETE);
	sensitive = (n_selected > 0) && editable && (n_incomplete > 0);
	gtk_action_set_sensitive (action, sensitive);

	action = ACTION (CALENDAR_TASKPAD_MARK_INCOMPLETE);
	sensitive = (n_selected > 0) && editable && (n_complete > 0);
	gtk_action_set_sensitive (action, sensitive);

	action = ACTION (CALENDAR_TASKPAD_OPEN);
	sensitive = (n_selected == 1);
	gtk_action_set_sensitive (action, sensitive);

	action = ACTION (CALENDAR_TASKPAD_OPEN_URL);
	sensitive = (n_selected == 1) && has_url;
	gtk_action_set_sensitive (action, sensitive);

	action = ACTION (CALENDAR_TASKPAD_PRINT);
	sensitive = (n_selected == 1);
	gtk_action_set_sensitive (action, sensitive);

	action = ACTION (CALENDAR_TASKPAD_SAVE_AS);
	sensitive = (n_selected == 1);
	gtk_action_set_sensitive (action, sensitive);
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
