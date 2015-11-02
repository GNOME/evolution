/*
 * e-task-shell-view-actions.c
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

#include "e-cal-base-shell-view.h"
#include "e-task-shell-view-private.h"
#include "e-cal-shell-view.h"

static void
action_task_assign_cb (GtkAction *action,
                       ETaskShellView *task_shell_view)
{
	ETaskShellContent *task_shell_content;
	ECalModelComponent *comp_data;
	ETaskTable *task_table;
	GSList *list;

	task_shell_content = task_shell_view->priv->task_shell_content;
	task_table = e_task_shell_content_get_task_table (task_shell_content);

	list = e_task_table_get_selected (task_table);
	g_return_if_fail (list != NULL);
	comp_data = list->data;
	g_slist_free (list);

	/* XXX We only open the first selected task. */
	e_task_shell_view_open_task (task_shell_view, comp_data);

	/* FIXME Need to actually assign the task. */
}

static void
action_task_delete_cb (GtkAction *action,
                       ETaskShellView *task_shell_view)
{
	ETaskShellContent *task_shell_content;
	ETaskTable *task_table;

	task_shell_content = task_shell_view->priv->task_shell_content;
	task_table = e_task_shell_content_get_task_table (task_shell_content);

	e_selectable_delete_selection (E_SELECTABLE (task_table));
}

static void
action_task_find_cb (GtkAction *action,
                     ETaskShellView *task_shell_view)
{
	ETaskShellContent *task_shell_content;
	EPreviewPane *preview_pane;

	task_shell_content = task_shell_view->priv->task_shell_content;
	preview_pane = e_task_shell_content_get_preview_pane (task_shell_content);

	e_preview_pane_show_search_bar (preview_pane);
}

static void
action_task_forward_cb (GtkAction *action,
                        ETaskShellView *task_shell_view)
{
	ETaskShellContent *task_shell_content;
	ECalModelComponent *comp_data;
	ETaskTable *task_table;
	ECalComponent *comp;
	GSList *list;

	task_shell_content = task_shell_view->priv->task_shell_content;
	task_table = e_task_shell_content_get_task_table (task_shell_content);

	list = e_task_table_get_selected (task_table);
	g_return_if_fail (list != NULL);
	comp_data = list->data;
	g_slist_free (list);

	/* XXX We only forward the first selected task. */
	comp = e_cal_component_new_from_icalcomponent (icalcomponent_new_clone (comp_data->icalcomp));
	g_return_if_fail (comp != NULL);

	itip_send_component_with_model (e_task_table_get_model (task_table),
		E_CAL_COMPONENT_METHOD_PUBLISH, comp,
		comp_data->client, NULL, NULL, NULL, TRUE, FALSE, TRUE);

	g_object_unref (comp);
}

static void
action_task_list_copy_cb (GtkAction *action,
			  EShellView *shell_view)
{
	e_cal_base_shell_view_copy_calendar (shell_view);
}

static void
action_task_list_delete_cb (GtkAction *action,
                            ETaskShellView *task_shell_view)
{
	ECalBaseShellSidebar *task_shell_sidebar;
	EShellWindow *shell_window;
	EShellView *shell_view;
	ESource *source;
	ESourceSelector *selector;
	gint response;

	shell_view = E_SHELL_VIEW (task_shell_view);
	shell_window = e_shell_view_get_shell_window (shell_view);

	task_shell_sidebar = task_shell_view->priv->task_shell_sidebar;
	selector = e_cal_base_shell_sidebar_get_selector (task_shell_sidebar);

	source = e_source_selector_ref_primary_selection (selector);
	g_return_if_fail (source != NULL);

	if (e_source_get_remote_deletable (source)) {
		response = e_alert_run_dialog_for_args (
			GTK_WINDOW (shell_window),
			"calendar:prompt-delete-remote-task-list",
			e_source_get_display_name (source), NULL);

		if (response == GTK_RESPONSE_YES)
			e_shell_view_remote_delete_source (shell_view, source);

	} else {
		response = e_alert_run_dialog_for_args (
			GTK_WINDOW (shell_window),
			"calendar:prompt-delete-task-list",
			e_source_get_display_name (source), NULL);

		if (response == GTK_RESPONSE_YES)
			e_shell_view_remove_source (shell_view, source);
	}

	g_object_unref (source);
}

static void
action_task_list_manage_groups_cb (GtkAction *action,
				   ETaskShellView *task_shell_view)
{
	EShellView *shell_view;
	ESourceSelector *selector;

	shell_view = E_SHELL_VIEW (task_shell_view);
	selector = e_cal_base_shell_sidebar_get_selector (task_shell_view->priv->task_shell_sidebar);

	if (e_source_selector_manage_groups (selector) &&
	    e_source_selector_save_groups_setup (selector, e_shell_view_get_state_key_file (shell_view)))
		e_shell_view_set_state_dirty (shell_view);
}


static void
action_task_list_new_cb (GtkAction *action,
                         ETaskShellView *task_shell_view)
{
	EShell *shell;
	EShellView *shell_view;
	EShellWindow *shell_window;
	ESourceRegistry *registry;
	ECalClientSourceType source_type;
	GtkWidget *config;
	GtkWidget *dialog;
	const gchar *icon_name;

	shell_view = E_SHELL_VIEW (task_shell_view);
	shell_window = e_shell_view_get_shell_window (shell_view);
	shell = e_shell_window_get_shell (shell_window);

	registry = e_shell_get_registry (shell);
	source_type = E_CAL_CLIENT_SOURCE_TYPE_TASKS;
	config = e_cal_source_config_new (registry, NULL, source_type);

	dialog = e_source_config_dialog_new (E_SOURCE_CONFIG (config));

	gtk_window_set_transient_for (
		GTK_WINDOW (dialog), GTK_WINDOW (shell_window));

	icon_name = gtk_action_get_icon_name (action);
	gtk_window_set_icon_name (GTK_WINDOW (dialog), icon_name);

	gtk_window_set_title (GTK_WINDOW (dialog), _("New Task List"));

	gtk_widget_show (dialog);
}

static void
action_task_list_print_cb (GtkAction *action,
                           ETaskShellView *task_shell_view)
{
	ETaskShellContent *task_shell_content;
	ETaskTable *task_table;

	task_shell_content = task_shell_view->priv->task_shell_content;
	task_table = e_task_shell_content_get_task_table (task_shell_content);

	print_table (
		E_TABLE (task_table), _("Print Tasks"), _("Tasks"),
		GTK_PRINT_OPERATION_ACTION_PRINT_DIALOG);
}

static void
action_task_list_print_preview_cb (GtkAction *action,
                                   ETaskShellView *task_shell_view)
{
	ETaskShellContent *task_shell_content;
	ETaskTable *task_table;

	task_shell_content = task_shell_view->priv->task_shell_content;
	task_table = e_task_shell_content_get_task_table (task_shell_content);

	print_table (
		E_TABLE (task_table), _("Print Tasks"), _("Tasks"),
		GTK_PRINT_OPERATION_ACTION_PREVIEW);
}

static void
action_task_list_properties_cb (GtkAction *action,
                                ETaskShellView *task_shell_view)
{
	EShellView *shell_view;
	EShellWindow *shell_window;
	ECalBaseShellSidebar *task_shell_sidebar;
	ECalClientSourceType source_type;
	ESource *source;
	ESourceSelector *selector;
	ESourceRegistry *registry;
	GtkWidget *config;
	GtkWidget *dialog;
	const gchar *icon_name;

	shell_view = E_SHELL_VIEW (task_shell_view);
	shell_window = e_shell_view_get_shell_window (shell_view);

	task_shell_sidebar = task_shell_view->priv->task_shell_sidebar;
	selector = e_cal_base_shell_sidebar_get_selector (task_shell_sidebar);
	source = e_source_selector_ref_primary_selection (selector);
	g_return_if_fail (source != NULL);

	source_type = E_CAL_CLIENT_SOURCE_TYPE_TASKS;
	registry = e_source_selector_get_registry (selector);
	config = e_cal_source_config_new (registry, source, source_type);

	g_object_unref (source);

	dialog = e_source_config_dialog_new (E_SOURCE_CONFIG (config));

	gtk_window_set_transient_for (
		GTK_WINDOW (dialog), GTK_WINDOW (shell_window));

	icon_name = gtk_action_get_icon_name (action);
	gtk_window_set_icon_name (GTK_WINDOW (dialog), icon_name);

	gtk_window_set_title (GTK_WINDOW (dialog), _("Task List Properties"));

	gtk_widget_show (dialog);
}

static void
action_task_list_refresh_cb (GtkAction *action,
                             ETaskShellView *task_shell_view)
{
	ECalBaseShellSidebar *task_shell_sidebar;
	ESourceSelector *selector;
	EClient *client = NULL;
	ESource *source;

	task_shell_sidebar = task_shell_view->priv->task_shell_sidebar;
	selector = e_cal_base_shell_sidebar_get_selector (task_shell_sidebar);

	source = e_source_selector_ref_primary_selection (selector);

	if (source != NULL) {
		client = e_client_selector_ref_cached_client (
			E_CLIENT_SELECTOR (selector), source);
		g_object_unref (source);
	}

	if (client == NULL)
		return;

	g_return_if_fail (e_client_check_refresh_supported (client));

	e_cal_base_shell_view_allow_auth_prompt_and_refresh (E_SHELL_VIEW (task_shell_view), client);

	g_object_unref (client);
}

static void
action_task_list_rename_cb (GtkAction *action,
                            ETaskShellView *task_shell_view)
{
	ECalBaseShellSidebar *task_shell_sidebar;
	ESourceSelector *selector;

	task_shell_sidebar = task_shell_view->priv->task_shell_sidebar;
	selector = e_cal_base_shell_sidebar_get_selector (task_shell_sidebar);

	e_source_selector_edit_primary_selection (selector);
}

static void
action_task_list_select_one_cb (GtkAction *action,
                                ETaskShellView *task_shell_view)
{
	ECalBaseShellSidebar *task_shell_sidebar;
	ESourceSelector *selector;
	ESource *primary;

	task_shell_sidebar = task_shell_view->priv->task_shell_sidebar;
	selector = e_cal_base_shell_sidebar_get_selector (task_shell_sidebar);

	primary = e_source_selector_ref_primary_selection (selector);
	g_return_if_fail (primary != NULL);

	e_source_selector_select_exclusive (selector, primary);

	g_object_unref (primary);
}

static void
action_task_mark_complete_cb (GtkAction *action,
                              ETaskShellView *task_shell_view)
{
	ETaskShellContent *task_shell_content;
	ETaskTable *task_table;
	ECalModel *model;
	GSList *list, *iter;

	task_shell_content = task_shell_view->priv->task_shell_content;
	task_table = e_task_shell_content_get_task_table (task_shell_content);
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
action_task_mark_incomplete_cb (GtkAction *action,
                                ETaskShellView *task_shell_view)
{
	ETaskShellContent *task_shell_content;
	ETaskTable *task_table;
	ECalModel *model;
	GSList *list, *iter;

	task_shell_content = task_shell_view->priv->task_shell_content;
	task_table = e_task_shell_content_get_task_table (task_shell_content);
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
action_task_new_cb (GtkAction *action,
                    ETaskShellView *task_shell_view)
{
	EShellView *shell_view;
	EShellWindow *shell_window;
	ETaskShellContent *task_shell_content;
	ETaskTable *task_table;
	EClient *client = NULL;
	GSList *list;

	shell_view = E_SHELL_VIEW (task_shell_view);
	shell_window = e_shell_view_get_shell_window (shell_view);

	task_shell_content = task_shell_view->priv->task_shell_content;
	task_table = e_task_shell_content_get_task_table (task_shell_content);

	list = e_task_table_get_selected (task_table);
	if (list) {
		ECalModelComponent *comp_data;

		comp_data = list->data;
		client = g_object_ref (comp_data->client);
		g_slist_free (list);
	}

	e_cal_ops_new_component_editor (shell_window, E_CAL_CLIENT_SOURCE_TYPE_TASKS,
		client ? e_source_get_uid (e_client_get_source (client)) : NULL, FALSE);

	g_clear_object (&client);
}

static void
action_task_open_cb (GtkAction *action,
                     ETaskShellView *task_shell_view)
{
	ETaskShellContent *task_shell_content;
	ECalModelComponent *comp_data;
	ETaskTable *task_table;
	GSList *list;

	task_shell_content = task_shell_view->priv->task_shell_content;
	task_table = e_task_shell_content_get_task_table (task_shell_content);

	list = e_task_table_get_selected (task_table);
	g_return_if_fail (list != NULL);
	comp_data = list->data;
	g_slist_free (list);

	/* XXX We only open the first selected task. */
	e_task_shell_view_open_task (task_shell_view, comp_data);
}

static void
action_task_open_url_cb (GtkAction *action,
                         ETaskShellView *task_shell_view)
{
	EShellView *shell_view;
	EShellWindow *shell_window;
	ETaskShellContent *task_shell_content;
	ECalModelComponent *comp_data;
	ETaskTable *task_table;
	icalproperty *prop;
	const gchar *uri;
	GSList *list;

	shell_view = E_SHELL_VIEW (task_shell_view);
	shell_window = e_shell_view_get_shell_window (shell_view);

	task_shell_content = task_shell_view->priv->task_shell_content;
	task_table = e_task_shell_content_get_task_table (task_shell_content);

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
action_task_preview_cb (GtkToggleAction *action,
                        ETaskShellView *task_shell_view)
{
	ETaskShellContent *task_shell_content;
	gboolean visible;

	task_shell_content = task_shell_view->priv->task_shell_content;
	visible = gtk_toggle_action_get_active (action);
	e_task_shell_content_set_preview_visible (task_shell_content, visible);
}

static void
action_task_print_cb (GtkAction *action,
                      ETaskShellView *task_shell_view)
{
	ETaskShellContent *task_shell_content;
	ECalModelComponent *comp_data;
	ECalComponent *comp;
	ECalModel *model;
	ETaskTable *task_table;
	icalcomponent *clone;
	GSList *list;

	task_shell_content = task_shell_view->priv->task_shell_content;
	task_table = e_task_shell_content_get_task_table (task_shell_content);
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
action_task_purge_cb (GtkAction *action,
                      ETaskShellView *task_shell_view)
{
	EShellView *shell_view;
	EShellWindow *shell_window;
	GtkWidget *content_area;
	GtkWidget *dialog;
	GtkWidget *widget;
	gboolean active;
	gint response;

	shell_view = E_SHELL_VIEW (task_shell_view);
	shell_window = e_shell_view_get_shell_window (shell_view);

	if (!e_task_shell_view_get_confirm_purge (task_shell_view))
		goto purge;

	/* XXX This needs reworked.  The dialog looks like ass. */

	dialog = gtk_message_dialog_new (
		GTK_WINDOW (shell_window),
		GTK_DIALOG_DESTROY_WITH_PARENT,
		GTK_MESSAGE_WARNING,
		GTK_BUTTONS_YES_NO,
		"%s", _("This operation will permanently erase all tasks "
		"marked as completed. If you continue, you will not be able "
		"to recover these tasks.\n\nReally erase these tasks?"));

	gtk_dialog_set_default_response (GTK_DIALOG (dialog), GTK_RESPONSE_NO);

	content_area = gtk_dialog_get_content_area (GTK_DIALOG (dialog));
	widget = gtk_check_button_new_with_label (_("Do not ask me again"));
	gtk_box_pack_start (GTK_BOX (content_area), widget, TRUE, TRUE, 6);
	gtk_widget_show (widget);

	response = gtk_dialog_run (GTK_DIALOG (dialog));
	active = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (widget));
	gtk_widget_destroy (dialog);

	if (response != GTK_RESPONSE_YES)
		return;

	if (active)
		e_task_shell_view_set_confirm_purge (task_shell_view, FALSE);

purge:
	e_task_shell_view_delete_completed (task_shell_view);
}

static void
action_task_save_as_cb (GtkAction *action,
                        ETaskShellView *task_shell_view)
{
	EShell *shell;
	EShellView *shell_view;
	EShellWindow *shell_window;
	EShellBackend *shell_backend;
	ETaskShellContent *task_shell_content;
	ECalModelComponent *comp_data;
	ETaskTable *task_table;
	EActivity *activity;
	GSList *list;
	GFile *file;
	gchar *string;

	shell_view = E_SHELL_VIEW (task_shell_view);
	shell_window = e_shell_view_get_shell_window (shell_view);
	shell_backend = e_shell_view_get_shell_backend (shell_view);
	shell = e_shell_window_get_shell (shell_window);

	task_shell_content = task_shell_view->priv->task_shell_content;
	task_table = e_task_shell_content_get_task_table (task_shell_content);

	list = e_task_table_get_selected (task_table);
	g_return_if_fail (list != NULL);
	comp_data = list->data;
	g_slist_free (list);

	/* Translators: Default filename part saving a task to a file when
	 * no summary is filed, the '.ics' extension is concatenated to it */
	string = icalcomp_suggest_filename (comp_data->icalcomp, _("task"));
	file = e_shell_run_save_dialog (
		shell, _("Save as iCalendar"), string,
		"*.ics:text/calendar", NULL, NULL);
	g_free (string);
	if (file == NULL)
		return;

	/* XXX We only save the first selected task. */
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

static void
action_task_view_cb (GtkRadioAction *action,
                     GtkRadioAction *current,
                     ETaskShellView *task_shell_view)
{
	ETaskShellContent *task_shell_content;
	GtkOrientable *orientable;
	GtkOrientation orientation;

	task_shell_content = task_shell_view->priv->task_shell_content;
	orientable = GTK_ORIENTABLE (task_shell_content);

	switch (gtk_radio_action_get_current_value (action)) {
		case 0:
			orientation = GTK_ORIENTATION_VERTICAL;
			break;
		case 1:
			orientation = GTK_ORIENTATION_HORIZONTAL;
			break;
		default:
			g_return_if_reached ();
	}

	gtk_orientable_set_orientation (orientable, orientation);
}

static GtkActionEntry task_entries[] = {

	{ "task-assign",
	  NULL,
	  N_("_Assign Task"),
	  NULL,
	  NULL,  /* XXX Add a tooltip! */
	  G_CALLBACK (action_task_assign_cb) },

	{ "task-delete",
	  "edit-delete",
	  N_("_Delete Task"),
	  NULL,
	  N_("Delete selected tasks"),
	  G_CALLBACK (action_task_delete_cb) },

	{ "task-find",
	  "edit-find",
	  N_("_Find in Task..."),
	  "<Shift><Control>f",
	  N_("Search for text in the displayed task"),
	  G_CALLBACK (action_task_find_cb) },

	{ "task-forward",
	  "mail-forward",
	  N_("_Forward as iCalendar..."),
	  "<Control>f",
	  NULL,  /* XXX Add a tooltip! */
	  G_CALLBACK (action_task_forward_cb) },

	{ "task-list-copy",
	  "edit-copy",
	  N_("_Copy..."),
	  "<Control>c",
	  NULL,  /* XXX Add a tooltip! */
	  G_CALLBACK (action_task_list_copy_cb) },

	{ "task-list-delete",
	  "edit-delete",
	  N_("D_elete Task List"),
	  NULL,
	  N_("Delete the selected task list"),
	  G_CALLBACK (action_task_list_delete_cb) },

	{ "task-list-manage-groups",
	  NULL,
	  N_("_Manage Task List groups..."),
	  NULL,
	  N_("Manage task list groups order and visibility"),
	  G_CALLBACK (action_task_list_manage_groups_cb) },

	{ "task-list-new",
	  "stock_todo",
	  N_("_New Task List"),
	  NULL,
	  N_("Create a new task list"),
	  G_CALLBACK (action_task_list_new_cb) },

	{ "task-list-properties",
	  "document-properties",
	  N_("_Properties"),
	  NULL,
	  NULL,  /* XXX Add a tooltip! */
	  G_CALLBACK (action_task_list_properties_cb) },

	{ "task-list-refresh",
	  "view-refresh",
	  N_("Re_fresh"),
	  NULL,
	  N_("Refresh the selected task list"),
	  G_CALLBACK (action_task_list_refresh_cb) },

	{ "task-list-rename",
	  NULL,
	  N_("_Rename..."),
	  "F2",
	  N_("Rename the selected task list"),
	  G_CALLBACK (action_task_list_rename_cb) },

	{ "task-list-select-one",
	  "stock_check-filled",
	  N_("Show _Only This Task List"),
	  NULL,
	  NULL,  /* XXX Add a tooltip! */
	  G_CALLBACK (action_task_list_select_one_cb) },

	{ "task-mark-complete",
	  NULL,
	  N_("_Mark as Complete"),
	  "<Control>k",
	  N_("Mark selected tasks as complete"),
	  G_CALLBACK (action_task_mark_complete_cb) },

	{ "task-mark-incomplete",
	  NULL,
	  N_("Mar_k as Incomplete"),
	  NULL,
	  N_("Mark selected tasks as incomplete"),
	  G_CALLBACK (action_task_mark_incomplete_cb) },

	{ "task-new",
	  "stock_task",
	  N_("New _Task"),
	  NULL,
	  N_("Create a new task"),
	  G_CALLBACK (action_task_new_cb) },

	{ "task-open",
	  "document-open",
	  N_("_Open Task"),
	  "<Control>o",
	  N_("View the selected task"),
	  G_CALLBACK (action_task_open_cb) },

	{ "task-open-url",
	  "applications-internet",
	  N_("Open _Web Page"),
	  NULL,
	  NULL,  /* XXX Add a tooltip! */
	  G_CALLBACK (action_task_open_url_cb) },

	{ "task-purge",
	  NULL,
	  N_("Purg_e"),
	  "<Control>e",
	  N_("Delete completed tasks"),
	  G_CALLBACK (action_task_purge_cb) },

	/*** Menus ***/

	{ "task-actions-menu",
	  NULL,
	  N_("_Actions"),
	  NULL,
	  NULL,
	  NULL },

	{ "task-preview-menu",
	  NULL,
	  N_("_Preview"),
	  NULL,
	  NULL,
	  NULL }
};

static EPopupActionEntry task_popup_entries[] = {

	{ "task-list-popup-copy",
	  NULL,
	  "task-list-copy" },

	{ "task-list-popup-delete",
	  N_("_Delete"),
	  "task-list-delete" },

	{ "task-list-popup-manage-groups",
	  N_("_Manage groups..."),
	  "task-list-manage-groups" },

	{ "task-list-popup-properties",
	  NULL,
	  "task-list-properties" },

	{ "task-list-popup-refresh",
	  NULL,
	  "task-list-refresh" },

	{ "task-list-popup-rename",
	  NULL,
	  "task-list-rename" },

	{ "task-list-popup-select-one",
	  NULL,
	  "task-list-select-one" },

	{ "task-popup-assign",
	  NULL,
	  "task-assign" },

	{ "task-popup-forward",
	  NULL,
	  "task-forward" },

	{ "task-popup-mark-complete",
	  NULL,
	  "task-mark-complete" },

	{ "task-popup-mark-incomplete",
	  NULL,
	  "task-mark-incomplete" },

	{ "task-popup-open",
	  NULL,
	  "task-open" },

	{ "task-popup-open-url",
	  NULL,
	  "task-open-url" }
};

static GtkToggleActionEntry task_toggle_entries[] = {

	{ "task-preview",
	  NULL,
	  N_("Task _Preview"),
	  "<Control>m",
	  N_("Show task preview pane"),
	  G_CALLBACK (action_task_preview_cb),
	  TRUE }
};

static GtkRadioActionEntry task_view_entries[] = {

	/* This action represents the inital active memo view.
	 * It should not be visible in the UI, nor should it be
	 * possible to switch to it from another shell view. */
	{ "task-view-initial",
	  NULL,
	  NULL,
	  NULL,
	  NULL,
	  -1 },

	{ "task-view-classic",
	  NULL,
	  N_("_Classic View"),
	  NULL,
	  N_("Show task preview below the task list"),
	  0 },

	{ "task-view-vertical",
	  NULL,
	  N_("_Vertical View"),
	  NULL,
	  N_("Show task preview alongside the task list"),
	  1 }
};

static GtkRadioActionEntry task_filter_entries[] = {

	{ "task-filter-active-tasks",
	  NULL,
	  N_("Active Tasks"),
	  NULL,
	  NULL,  /* XXX Add a tooltip! */
	  TASK_FILTER_ACTIVE_TASKS },

	{ "task-filter-any-category",
	  NULL,
	  N_("Any Category"),
	  NULL,
	  NULL,  /* XXX Add a tooltip! */
	  TASK_FILTER_ANY_CATEGORY },

	{ "task-filter-completed-tasks",
	  NULL,
	  N_("Completed Tasks"),
	  NULL,
	  NULL,  /* XXX Add a tooltip! */
	  TASK_FILTER_COMPLETED_TASKS },

	{ "task-filter-next-7-days-tasks",
	  NULL,
	  N_("Next 7 Days' Tasks"),
	  NULL,
	  NULL,  /* XXX Add a tooltip! */
	  TASK_FILTER_NEXT_7_DAYS_TASKS },

	{ "task-filter-overdue-tasks",
	  NULL,
	  N_("Overdue Tasks"),
	  NULL,
	  NULL,  /* XXX Add a tooltip! */
	  TASK_FILTER_OVERDUE_TASKS },

	{ "task-filter-tasks-with-attachments",
	  NULL,
	  N_("Tasks with Attachments"),
	  NULL,
	  NULL,  /* XXX Add a tooltip! */
	  TASK_FILTER_TASKS_WITH_ATTACHMENTS },

	{ "task-filter-unmatched",
	  NULL,
	  N_("Unmatched"),
	  NULL,
	  NULL,  /* XXX Add a tooltip! */
	  TASK_FILTER_UNMATCHED }
};

static GtkRadioActionEntry task_search_entries[] = {

	{ "task-search-advanced-hidden",
	  NULL,
	  N_("Advanced Search"),
	  NULL,
	  NULL,
	  TASK_SEARCH_ADVANCED },

	{ "task-search-any-field-contains",
          NULL,
	  N_("Any field contains"),
	  NULL,
	  NULL,  /* XXX Add a tooltip! */
	  TASK_SEARCH_ANY_FIELD_CONTAINS },

	{ "task-search-description-contains",
	  NULL,
	  N_("Description contains"),
	  NULL,
	  NULL,  /* XXX Add a tooltip! */
	  TASK_SEARCH_DESCRIPTION_CONTAINS },

	{ "task-search-summary-contains",
	  NULL,
	  N_("Summary contains"),
	  NULL,
	  NULL,  /* XXX Add a tooltip! */
	  TASK_SEARCH_SUMMARY_CONTAINS }
};

static GtkActionEntry lockdown_printing_entries[] = {

	{ "task-list-print",
	  "document-print",
	  N_("Print..."),
	  "<Control>p",
	  N_("Print the list of tasks"),
	  G_CALLBACK (action_task_list_print_cb) },

	{ "task-list-print-preview",
	  "document-print-preview",
	  N_("Pre_view..."),
	  NULL,
	  N_("Preview the list of tasks to be printed"),
	  G_CALLBACK (action_task_list_print_preview_cb) },

	{ "task-print",
	  "document-print",
	  N_("Print..."),
	  NULL,
	  N_("Print the selected task"),
	  G_CALLBACK (action_task_print_cb) }
};

static EPopupActionEntry lockdown_printing_popup_entries[] = {

	{ "task-popup-print",
	  NULL,
	  "task-print" }
};

static GtkActionEntry lockdown_save_to_disk_entries[] = {

	{ "task-save-as",
	  "document-save-as",
	  N_("_Save as iCalendar..."),
	  NULL,
	  NULL,  /* XXX Add a tooltip! */
	  G_CALLBACK (action_task_save_as_cb) }
};

static EPopupActionEntry lockdown_save_to_disk_popup_entries[] = {

	{ "task-popup-save-as",
	  NULL,
	  "task-save-as" },
};

void
e_task_shell_view_actions_init (ETaskShellView *task_shell_view)
{
	ETaskShellContent *task_shell_content;
	EShellView *shell_view;
	EShellWindow *shell_window;
	EShellSearchbar *searchbar;
	EPreviewPane *preview_pane;
	EWebView *web_view;
	GtkActionGroup *action_group;
	GSettings *settings;
	GtkAction *action;

	shell_view = E_SHELL_VIEW (task_shell_view);
	shell_window = e_shell_view_get_shell_window (shell_view);

	task_shell_content = task_shell_view->priv->task_shell_content;
	searchbar = e_task_shell_content_get_searchbar (task_shell_content);
	preview_pane = e_task_shell_content_get_preview_pane (task_shell_content);
	web_view = e_preview_pane_get_web_view (preview_pane);

	/* Task Actions */
	action_group = ACTION_GROUP (TASKS);
	gtk_action_group_add_actions (
		action_group, task_entries,
		G_N_ELEMENTS (task_entries), task_shell_view);
	e_action_group_add_popup_actions (
		action_group, task_popup_entries,
		G_N_ELEMENTS (task_popup_entries));
	gtk_action_group_add_toggle_actions (
		action_group, task_toggle_entries,
		G_N_ELEMENTS (task_toggle_entries), task_shell_view);
	gtk_action_group_add_radio_actions (
		action_group, task_view_entries,
		G_N_ELEMENTS (task_view_entries), -1,
		G_CALLBACK (action_task_view_cb), task_shell_view);
	gtk_action_group_add_radio_actions (
		action_group, task_search_entries,
		G_N_ELEMENTS (task_search_entries),
		-1, NULL, NULL);

	/* Advanced Search Action */
	action = ACTION (TASK_SEARCH_ADVANCED_HIDDEN);
	gtk_action_set_visible (action, FALSE);
	e_shell_searchbar_set_search_option (
		searchbar, GTK_RADIO_ACTION (action));

	/* Lockdown Printing Actions */
	action_group = ACTION_GROUP (LOCKDOWN_PRINTING);
	gtk_action_group_add_actions (
		action_group, lockdown_printing_entries,
		G_N_ELEMENTS (lockdown_printing_entries),
		task_shell_view);
	e_action_group_add_popup_actions (
		action_group, lockdown_printing_popup_entries,
		G_N_ELEMENTS (lockdown_printing_popup_entries));

	/* Lockdown Save-to-Disk Actions */
	action_group = ACTION_GROUP (LOCKDOWN_SAVE_TO_DISK);
	gtk_action_group_add_actions (
		action_group, lockdown_save_to_disk_entries,
		G_N_ELEMENTS (lockdown_save_to_disk_entries),
		task_shell_view);
	e_action_group_add_popup_actions (
		action_group, lockdown_save_to_disk_popup_entries,
		G_N_ELEMENTS (lockdown_save_to_disk_popup_entries));

	/* Bind GObject properties to settings keys. */

	settings = e_util_ref_settings ("org.gnome.evolution.calendar");

	g_settings_bind (
		settings, "show-task-preview",
		ACTION (TASK_PREVIEW), "active",
		G_SETTINGS_BIND_DEFAULT);

	g_settings_bind (
		settings, "task-layout",
		ACTION (TASK_VIEW_VERTICAL), "current-value",
		G_SETTINGS_BIND_DEFAULT);

	g_object_unref (settings);

	/* Fine tuning. */

	e_binding_bind_property (
		ACTION (TASK_PREVIEW), "active",
		ACTION (TASK_VIEW_CLASSIC), "sensitive",
		G_BINDING_SYNC_CREATE);

	e_binding_bind_property (
		ACTION (TASK_PREVIEW), "active",
		ACTION (TASK_VIEW_VERTICAL), "sensitive",
		G_BINDING_SYNC_CREATE);

	e_web_view_set_open_proxy (web_view, ACTION (TASK_OPEN));
	e_web_view_set_print_proxy (web_view, ACTION (TASK_PRINT));
	e_web_view_set_save_as_proxy (web_view, ACTION (TASK_SAVE_AS));
}

void
e_task_shell_view_update_search_filter (ETaskShellView *task_shell_view)
{
	ETaskShellContent *task_shell_content;
	EShellView *shell_view;
	EShellWindow *shell_window;
	EShellSearchbar *searchbar;
	EActionComboBox *combo_box;
	GtkActionGroup *action_group;
	GtkRadioAction *radio_action;
	GList *list, *iter;
	GSList *group;
	gint ii;

	shell_view = E_SHELL_VIEW (task_shell_view);
	shell_window = e_shell_view_get_shell_window (shell_view);

	action_group = ACTION_GROUP (TASKS_FILTER);
	e_action_group_remove_all_actions (action_group);

	/* Add the standard filter actions.  No callback is needed
	 * because changes in the EActionComboBox are detected and
	 * handled by EShellSearchbar. */
	gtk_action_group_add_radio_actions (
		action_group, task_filter_entries,
		G_N_ELEMENTS (task_filter_entries),
		TASK_FILTER_ANY_CATEGORY, NULL, NULL);

	/* Retrieve the radio group from an action we just added. */
	list = gtk_action_group_list_actions (action_group);
	radio_action = GTK_RADIO_ACTION (list->data);
	group = gtk_radio_action_get_group (radio_action);
	g_list_free (list);

	/* Build the category actions. */

	list = e_util_dup_searchable_categories ();
	for (iter = list, ii = 0; iter != NULL; iter = iter->next, ii++) {
		const gchar *category_name = iter->data;
		gchar *filename;
		GtkAction *action;
		gchar *action_name;

		action_name = g_strdup_printf (
			"task-filter-category-%d", ii);
		radio_action = gtk_radio_action_new (
			action_name, category_name, NULL, NULL, ii);
		g_free (action_name);

		/* Convert the category icon file to a themed icon name. */
		filename = e_categories_dup_icon_file_for (category_name);
		if (filename != NULL && *filename != '\0') {
			gchar *basename;
			gchar *cp;

			basename = g_path_get_basename (filename);

			/* Lose the file extension. */
			if ((cp = strrchr (basename, '.')) != NULL)
				*cp = '\0';

			g_object_set (
				radio_action, "icon-name", basename, NULL);

			g_free (basename);
		}

		g_free (filename);

		gtk_radio_action_set_group (radio_action, group);
		group = gtk_radio_action_get_group (radio_action);

		/* The action group takes ownership of the action. */
		action = GTK_ACTION (radio_action);
		gtk_action_group_add_action (action_group, action);
		g_object_unref (radio_action);
	}
	g_list_free_full (list, g_free);

	task_shell_content = task_shell_view->priv->task_shell_content;
	searchbar = e_task_shell_content_get_searchbar (task_shell_content);
	combo_box = e_shell_searchbar_get_filter_combo_box (searchbar);

	e_shell_view_block_execute_search (shell_view);

	/* Use any action in the group; doesn't matter which. */
	e_action_combo_box_set_action (combo_box, radio_action);

	ii = TASK_FILTER_UNMATCHED;
	e_action_combo_box_add_separator_after (combo_box, ii);

	ii = TASK_FILTER_TASKS_WITH_ATTACHMENTS;
	e_action_combo_box_add_separator_after (combo_box, ii);

	e_shell_view_unblock_execute_search (shell_view);
}
