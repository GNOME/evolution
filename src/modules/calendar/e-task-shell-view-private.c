/*
 * e-task-shell-view-private.c
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

#include "e-util/e-util-private.h"
#include <calendar/gui/e-cal-ops.h>

#include "e-task-shell-view-private.h"

static gboolean
task_shell_view_process_completed_tasks_cb (gpointer user_data)
{
	ETaskShellContent *task_shell_content;
	ETaskShellView *task_shell_view;
	ETaskTable *task_table;

	task_shell_view = E_TASK_SHELL_VIEW (user_data);

	task_shell_view->priv->update_completed_timeout = 0;

	task_shell_content = task_shell_view->priv->task_shell_content;
	task_table = e_task_shell_content_get_task_table (task_shell_content);

	e_task_table_process_completed_tasks (task_table, TRUE);

	/* Search query takes whether to show completed tasks into account,
	 * so if the preference has changed we need to update the query. */
	e_shell_view_execute_search (E_SHELL_VIEW (task_shell_view));

	return FALSE;
}

static void
task_shell_view_process_completed_tasks (ETaskShellView *task_shell_view)
{
	guint source_id;

	source_id = task_shell_view->priv->update_completed_timeout;

	if (source_id > 0)
		g_source_remove (source_id);

	source_id = e_named_timeout_add_seconds (
		1, task_shell_view_process_completed_tasks_cb,
		task_shell_view);

	task_shell_view->priv->update_completed_timeout = source_id;
}

static void
task_shell_view_hide_completed_tasks_changed_cb (GSettings *settings,
                                                 const gchar *key,
                                                 ETaskShellView *task_shell_view)
{
	GVariant *new_value, *old_value;

	new_value = g_settings_get_value (settings, key);
	old_value = g_hash_table_lookup (task_shell_view->priv->old_settings, key);

	if (!new_value || !old_value || !g_variant_equal (new_value, old_value)) {
		if (new_value)
			g_hash_table_insert (task_shell_view->priv->old_settings, g_strdup (key), new_value);
		else
			g_hash_table_remove (task_shell_view->priv->old_settings, key);

		task_shell_view_process_completed_tasks (task_shell_view);
	} else if (new_value) {
		g_variant_unref (new_value);
	}
}

static void
task_shell_view_hide_cancelled_tasks_changed_cb (GSettings *settings,
                                                 const gchar *key,
                                                 ETaskShellView *task_shell_view)
{
	GVariant *new_value, *old_value;

	new_value = g_settings_get_value (settings, key);
	old_value = g_hash_table_lookup (task_shell_view->priv->old_settings, key);

	if (!new_value || !old_value || !g_variant_equal (new_value, old_value)) {
		if (new_value)
			g_hash_table_insert (task_shell_view->priv->old_settings, g_strdup (key), new_value);
		else
			g_hash_table_remove (task_shell_view->priv->old_settings, key);

		e_shell_view_execute_search (E_SHELL_VIEW (task_shell_view));
	} else if (new_value) {
		g_variant_unref (new_value);
	}
}

static void
task_shell_view_table_open_component_cb (ETaskShellView *task_shell_view,
					 ECalModelComponent *comp_data)
{
	g_return_if_fail (E_IS_TASK_SHELL_VIEW (task_shell_view));
	g_return_if_fail (E_IS_CAL_MODEL_COMPONENT (comp_data));

	e_task_shell_view_open_task (task_shell_view, comp_data, FALSE);
}

static void
task_shell_view_table_popup_event_cb (EShellView *shell_view,
                                      GdkEvent *button_event)
{
	e_cal_base_shell_view_show_popup_menu (shell_view, "task-popup", button_event, NULL);
}

static gboolean
task_shell_view_selector_popup_event_cb (EShellView *shell_view,
                                         ESource *clicked_source,
                                         GdkEvent *button_event)
{
	e_cal_base_shell_view_show_popup_menu (shell_view, "task-list-popup", button_event, clicked_source);

	return TRUE;
}

static gboolean
task_shell_view_update_timeout_cb (gpointer user_data)
{
	ETaskShellView *task_shell_view;
	ETaskShellContent *task_shell_content;
	ETaskTable *task_table;
	ECalModel *model;

	task_shell_view = E_TASK_SHELL_VIEW (user_data);
	task_shell_content = task_shell_view->priv->task_shell_content;
	task_table = e_task_shell_content_get_task_table (task_shell_content);
	model = e_task_table_get_model (task_table);

	e_task_table_process_completed_tasks (task_table, FALSE);
	e_cal_model_tasks_update_due_tasks (E_CAL_MODEL_TASKS (model));

	return TRUE;
}

static void
task_shell_view_backend_error_cb (EClientCache *client_cache,
                                  EClient *client,
                                  EAlert *alert,
                                  ETaskShellView *task_shell_view)
{
	ETaskShellContent *task_shell_content;
	ESource *source;
	const gchar *extension_name;

	task_shell_content = task_shell_view->priv->task_shell_content;

	source = e_client_get_source (client);
	extension_name = E_SOURCE_EXTENSION_TASK_LIST;

	/* Only submit alerts from task list backends. */
	if (e_source_has_extension (source, extension_name)) {
		EAlertSink *alert_sink;

		alert_sink = E_ALERT_SINK (task_shell_content);
		e_alert_sink_submit_alert (alert_sink, alert);
	}
}

static void
task_shell_view_notify_view_id_cb (EShellView *shell_view)
{
	GalViewInstance *view_instance;
	const gchar *view_id;

	view_id = e_shell_view_get_view_id (shell_view);
	view_instance = e_shell_view_get_view_instance (shell_view);

	/* A NULL view ID implies we're in a custom view.  But you can
	 * only get to a custom view via the "Define Views" dialog, which
	 * would have already modified the view instance appropriately.
	 * Furthermore, there's no way to refer to a custom view by ID
	 * anyway, since custom views have no IDs. */
	if (view_id == NULL)
		return;

	gal_view_instance_set_current_view_id (view_instance, view_id);
}

static void
task_shell_view_task_view_notify_state_cb (GObject *object,
					   GParamSpec *param,
					   gpointer user_data)
{
	GAction *action = G_ACTION (object);
	ETaskShellView *task_shell_view = user_data;
	ETaskShellContent *task_shell_content;
	GtkOrientable *orientable;
	GtkOrientation orientation;
	GVariant *state;

	task_shell_content = task_shell_view->priv->task_shell_content;
	orientable = GTK_ORIENTABLE (task_shell_content);
	state = g_action_get_state (action);

	switch (g_variant_get_int32 (state)) {
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
	g_clear_pointer (&state, g_variant_unref);
}

void
e_task_shell_view_private_init (ETaskShellView *task_shell_view)
{
	task_shell_view->priv->old_settings = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, (GDestroyNotify) g_variant_unref);

	e_signal_connect_notify (
		task_shell_view, "notify::view-id",
		G_CALLBACK (task_shell_view_notify_view_id_cb), NULL);
}

void
e_task_shell_view_private_constructed (ETaskShellView *task_shell_view)
{
	ETaskShellViewPrivate *priv = task_shell_view->priv;
	EShellBackend *shell_backend;
	EShellContent *shell_content;
	EShellSidebar *shell_sidebar;
	EShellWindow *shell_window;
	EShellView *shell_view;
	EShell *shell;
	EPreviewPane *preview_pane;
	EShellSearchbar *searchbar;
	EWebView *web_view;
	EUIAction *action;
	GSettings *settings;
	gulong handler_id;

	shell_view = E_SHELL_VIEW (task_shell_view);
	shell_backend = e_shell_view_get_shell_backend (shell_view);
	shell_content = e_shell_view_get_shell_content (shell_view);
	shell_sidebar = e_shell_view_get_shell_sidebar (shell_view);
	shell_window = e_shell_view_get_shell_window (shell_view);
	shell = e_shell_window_get_shell (shell_window);

	/* Cache these to avoid lots of awkward casting. */
	priv->task_shell_backend = E_TASK_SHELL_BACKEND (g_object_ref (shell_backend));
	priv->task_shell_content = E_TASK_SHELL_CONTENT (g_object_ref (shell_content));
	priv->task_shell_sidebar = E_CAL_BASE_SHELL_SIDEBAR (g_object_ref (shell_sidebar));

	priv->settings = e_util_ref_settings ("org.gnome.evolution.calendar");

	/* Keep our own reference to this so we can
	 * disconnect our signal handlers in dispose(). */
	priv->client_cache = e_shell_get_client_cache (shell);
	g_object_ref (priv->client_cache);

	handler_id = g_signal_connect (
		priv->client_cache, "backend-error",
		G_CALLBACK (task_shell_view_backend_error_cb),
		task_shell_view);
	priv->backend_error_handler_id = handler_id;

	/* Keep our own reference to this so we can
	 * disconnect our signal handlers in dispose(). */
	priv->task_table = e_task_shell_content_get_task_table (
		E_TASK_SHELL_CONTENT (shell_content));
	g_object_ref (priv->task_table);

	handler_id = g_signal_connect_swapped (
		priv->task_table, "open-component",
		G_CALLBACK (task_shell_view_table_open_component_cb),
		task_shell_view);
	priv->open_component_handler_id = handler_id;

	handler_id = g_signal_connect_swapped (
		priv->task_table, "popup-event",
		G_CALLBACK (task_shell_view_table_popup_event_cb),
		task_shell_view);
	priv->popup_event_handler_id = handler_id;

	handler_id = g_signal_connect_swapped (
		priv->task_table, "selection-change",
		G_CALLBACK (e_task_shell_view_update_sidebar),
		task_shell_view);
	priv->selection_change_1_handler_id = handler_id;

	handler_id = g_signal_connect_swapped (
		priv->task_table, "selection-change",
		G_CALLBACK (e_shell_view_update_actions_in_idle),
		task_shell_view);
	priv->selection_change_2_handler_id = handler_id;

	/* Keep our own reference to this so we can
	 * disconnect our signal handlers in dispose(). */
	priv->model = e_task_table_get_model (priv->task_table);
	g_object_ref (priv->model);

	handler_id = g_signal_connect_swapped (
		priv->model, "model-changed",
		G_CALLBACK (e_task_shell_view_update_sidebar),
		task_shell_view);
	priv->model_changed_handler_id = handler_id;

	handler_id = g_signal_connect_swapped (
		priv->model, "model-rows-deleted",
		G_CALLBACK (e_task_shell_view_update_sidebar),
		task_shell_view);
	priv->model_rows_deleted_handler_id = handler_id;

	handler_id = g_signal_connect_swapped (
		priv->model, "model-rows-inserted",
		G_CALLBACK (e_task_shell_view_update_sidebar),
		task_shell_view);
	priv->model_rows_inserted_handler_id = handler_id;

	handler_id = g_signal_connect_swapped (
		priv->model, "row-appended",
		G_CALLBACK (e_cal_base_shell_view_model_row_appended),
		task_shell_view);
	priv->rows_appended_handler_id = handler_id;

	/* Keep our own reference to this so we can
	 * disconnect our signal handlers in dispose(). */
	priv->selector = e_cal_base_shell_sidebar_get_selector (
		E_CAL_BASE_SHELL_SIDEBAR (shell_sidebar));
	g_object_ref (priv->selector);

	handler_id = g_signal_connect_swapped (
		priv->selector, "popup-event",
		G_CALLBACK (task_shell_view_selector_popup_event_cb),
		task_shell_view);
	priv->selector_popup_event_handler_id = handler_id;

	handler_id = g_signal_connect_swapped (
		priv->selector, "primary-selection-changed",
		G_CALLBACK (e_shell_view_update_actions),
		task_shell_view);
	priv->primary_selection_changed_handler_id = handler_id;

	e_categories_add_change_hook (
		(GHookFunc) e_task_shell_view_update_search_filter,
		task_shell_view);

	/* Listen for configuration changes. */
	g_settings_bind (
		priv->settings, "confirm-purge",
		shell_view, "confirm-purge",
		G_SETTINGS_BIND_DEFAULT);

	/* Hide Completed Tasks (enable/units/value) */
	handler_id = g_signal_connect (
		priv->settings, "changed::hide-completed-tasks",
		G_CALLBACK (task_shell_view_hide_completed_tasks_changed_cb),
		task_shell_view);
	priv->settings_hide_completed_tasks_handler_id = handler_id;
	handler_id = g_signal_connect (
		priv->settings, "changed::hide-completed-tasks-units",
		G_CALLBACK (task_shell_view_hide_completed_tasks_changed_cb),
		task_shell_view);
	priv->settings_hide_completed_tasks_units_handler_id = handler_id;
	handler_id = g_signal_connect (
		priv->settings, "changed::hide-completed-tasks-value",
		G_CALLBACK (task_shell_view_hide_completed_tasks_changed_cb),
		task_shell_view);
	priv->settings_hide_completed_tasks_value_handler_id = handler_id;

	/* Hide/show cancelled tasks */
	handler_id = g_signal_connect (
		priv->settings, "changed::hide-cancelled-tasks",
		G_CALLBACK (task_shell_view_hide_cancelled_tasks_changed_cb),
		task_shell_view);
	priv->settings_hide_cancelled_tasks_handler_id = handler_id;

	preview_pane = e_task_shell_content_get_preview_pane (task_shell_view->priv->task_shell_content);
	web_view = e_preview_pane_get_web_view (preview_pane);
	e_web_view_set_open_proxy (web_view, ACTION (TASK_OPEN));
	e_web_view_set_print_proxy (web_view, ACTION (TASK_PRINT));
	e_web_view_set_save_as_proxy (web_view, ACTION (TASK_SAVE_AS));

	/* Advanced Search Action */
	action = ACTION (TASK_SEARCH_ADVANCED_HIDDEN);
	e_ui_action_set_visible (action, FALSE);
	searchbar = e_task_shell_content_get_searchbar (task_shell_view->priv->task_shell_content);
	e_shell_searchbar_set_search_option (searchbar, action);

	e_task_shell_view_update_sidebar (task_shell_view);
	e_task_shell_view_update_search_filter (task_shell_view);

	/* Call this when everything is ready, like actions in
	 * action groups and such. */
	task_shell_view_update_timeout_cb (task_shell_view);
	priv->update_timeout = e_named_timeout_add_seconds_full (
		G_PRIORITY_LOW, 60,
		task_shell_view_update_timeout_cb,
		task_shell_view, NULL);

	/* Bind GObject properties to settings keys. */

	settings = e_util_ref_settings ("org.gnome.evolution.calendar");

	action = ACTION (TASK_PREVIEW);

	g_settings_bind (
		settings, "show-task-preview",
		action, "active",
		G_SETTINGS_BIND_DEFAULT | G_SETTINGS_BIND_NO_SENSITIVITY);

	e_binding_bind_property (
		action, "active",
		priv->task_shell_content, "preview-visible",
		G_BINDING_SYNC_CREATE);

	/* use the "classic" action, because it's the first in the group and
	   the group is not set yet, due to the UI manager being frozen */
	action = ACTION (TASK_VIEW_CLASSIC);

	g_settings_bind_with_mapping (
		settings, "task-layout",
		action, "state",
		G_SETTINGS_BIND_DEFAULT | G_SETTINGS_BIND_NO_SENSITIVITY,
		e_shell_view_util_layout_to_state_cb,
		e_shell_view_util_state_to_layout_cb, NULL, NULL);

	g_object_unref (settings);

	g_signal_connect_object (action, "notify::state",
		G_CALLBACK (task_shell_view_task_view_notify_state_cb), task_shell_view, 0);

	/* to propagate the loaded state */
	task_shell_view_task_view_notify_state_cb (G_OBJECT (action), NULL, task_shell_view);
}

void
e_task_shell_view_private_dispose (ETaskShellView *task_shell_view)
{
	ETaskShellViewPrivate *priv = task_shell_view->priv;

	if (priv->backend_error_handler_id > 0) {
		g_signal_handler_disconnect (
			priv->client_cache,
			priv->backend_error_handler_id);
		priv->backend_error_handler_id = 0;
	}

	if (priv->open_component_handler_id > 0) {
		g_signal_handler_disconnect (
			priv->task_table,
			priv->open_component_handler_id);
		priv->open_component_handler_id = 0;
	}

	if (priv->popup_event_handler_id > 0) {
		g_signal_handler_disconnect (
			priv->task_table,
			priv->popup_event_handler_id);
		priv->popup_event_handler_id = 0;
	}

	if (priv->selection_change_1_handler_id > 0) {
		g_signal_handler_disconnect (
			priv->task_table,
			priv->selection_change_1_handler_id);
		priv->selection_change_1_handler_id = 0;
	}

	if (priv->selection_change_2_handler_id > 0) {
		g_signal_handler_disconnect (
			priv->task_table,
			priv->selection_change_2_handler_id);
		priv->selection_change_2_handler_id = 0;
	}

	if (priv->model_changed_handler_id > 0) {
		g_signal_handler_disconnect (
			priv->model,
			priv->model_changed_handler_id);
		priv->model_changed_handler_id = 0;
	}

	if (priv->model_rows_deleted_handler_id > 0) {
		g_signal_handler_disconnect (
			priv->model,
			priv->model_rows_deleted_handler_id);
		priv->model_rows_deleted_handler_id = 0;
	}

	if (priv->model_rows_inserted_handler_id > 0) {
		g_signal_handler_disconnect (
			priv->model,
			priv->model_rows_inserted_handler_id);
		priv->model_rows_inserted_handler_id = 0;
	}

	if (priv->rows_appended_handler_id > 0) {
		g_signal_handler_disconnect (
			priv->model,
			priv->rows_appended_handler_id);
		priv->rows_appended_handler_id = 0;
	}

	if (priv->selector_popup_event_handler_id > 0) {
		g_signal_handler_disconnect (
			priv->selector,
			priv->selector_popup_event_handler_id);
		priv->selector_popup_event_handler_id = 0;
	}

	if (priv->primary_selection_changed_handler_id > 0) {
		g_signal_handler_disconnect (
			priv->selector,
			priv->primary_selection_changed_handler_id);
		priv->primary_selection_changed_handler_id = 0;
	}

	if (priv->settings_hide_completed_tasks_handler_id > 0) {
		g_signal_handler_disconnect (
			priv->settings,
			priv->settings_hide_completed_tasks_handler_id);
		priv->settings_hide_completed_tasks_handler_id = 0;
	}

	if (priv->settings_hide_completed_tasks_units_handler_id > 0) {
		g_signal_handler_disconnect (
			priv->settings,
			priv->settings_hide_completed_tasks_units_handler_id);
		priv->settings_hide_completed_tasks_units_handler_id = 0;
	}

	if (priv->settings_hide_completed_tasks_value_handler_id > 0) {
		g_signal_handler_disconnect (
			priv->settings,
			priv->settings_hide_completed_tasks_value_handler_id);
		priv->settings_hide_completed_tasks_value_handler_id = 0;
	}

	if (priv->settings_hide_cancelled_tasks_handler_id > 0) {
		g_signal_handler_disconnect (
			priv->settings,
			priv->settings_hide_cancelled_tasks_handler_id);
		priv->settings_hide_cancelled_tasks_handler_id = 0;
	}

	g_clear_object (&priv->task_shell_backend);
	g_clear_object (&priv->task_shell_content);
	g_clear_object (&priv->task_shell_sidebar);

	g_clear_object (&priv->client_cache);
	g_clear_object (&priv->task_table);
	g_clear_object (&priv->model);
	g_clear_object (&priv->selector);
	g_clear_object (&priv->settings);

	if (priv->update_timeout > 0) {
		g_source_remove (priv->update_timeout);
		priv->update_timeout = 0;
	}

	if (priv->update_completed_timeout > 0) {
		g_source_remove (priv->update_completed_timeout);
		priv->update_completed_timeout = 0;
	}
}

void
e_task_shell_view_private_finalize (ETaskShellView *task_shell_view)
{
	g_clear_pointer (&task_shell_view->priv->old_settings, g_hash_table_destroy);
}

void
e_task_shell_view_open_task (ETaskShellView *task_shell_view,
                             ECalModelComponent *comp_data,
			     gboolean force_attendees)
{
	EShellContent *shell_content;
	ECalModel *model;

	g_return_if_fail (E_IS_TASK_SHELL_VIEW (task_shell_view));
	g_return_if_fail (E_IS_CAL_MODEL_COMPONENT (comp_data));

	shell_content = e_shell_view_get_shell_content (E_SHELL_VIEW (task_shell_view));
	model = e_cal_base_shell_content_get_model (E_CAL_BASE_SHELL_CONTENT (shell_content));

	e_cal_ops_open_component_in_editor_sync	(model, comp_data->client, comp_data->icalcomp, force_attendees);
}

void
e_task_shell_view_delete_completed (ETaskShellView *task_shell_view)
{
	ETaskShellContent *task_shell_content;
	ECalModel *model;

	g_return_if_fail (E_IS_TASK_SHELL_VIEW (task_shell_view));

	task_shell_content = task_shell_view->priv->task_shell_content;
	model = e_cal_base_shell_content_get_model (E_CAL_BASE_SHELL_CONTENT (task_shell_content));

	e_cal_ops_delete_completed_tasks (model);
}

void
e_task_shell_view_update_sidebar (ETaskShellView *task_shell_view)
{
	ETaskShellContent *task_shell_content;
	EShellView *shell_view;
	EShellSidebar *shell_sidebar;
	ETaskTable *task_table;
	ECalModel *model;
	GString *string;
	const gchar *format;
	gint n_rows;
	gint n_selected;

	shell_view = E_SHELL_VIEW (task_shell_view);
	shell_sidebar = e_shell_view_get_shell_sidebar (shell_view);

	task_shell_content = task_shell_view->priv->task_shell_content;
	task_table = e_task_shell_content_get_task_table (task_shell_content);

	model = e_task_table_get_model (task_table);

	n_rows = e_table_model_row_count (E_TABLE_MODEL (model));
	n_selected = e_table_selected_count (E_TABLE (task_table));

	string = g_string_sized_new (64);

	format = ngettext ("%d task", "%d tasks", n_rows);
	g_string_append_printf (string, format, n_rows);

	if (n_selected > 0) {
		format = _("%d selected");
		g_string_append_len (string, ", ", 2);
		g_string_append_printf (string, format, n_selected);
	}

	e_shell_sidebar_set_secondary_text (shell_sidebar, string->str);

	g_string_free (string, TRUE);
}
