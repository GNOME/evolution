/*
 * e-memo-shell-view-private.c
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

#include "e-memo-shell-view-private.h"

static void
memo_shell_view_table_popup_event_cb (EShellView *shell_view,
                                      GdkEvent *button_event)
{
	const gchar *widget_path;

	widget_path = "/memo-popup";
	e_shell_view_show_popup_menu (shell_view, widget_path, button_event);
}

static gboolean
memo_shell_view_selector_popup_event_cb (EShellView *shell_view,
                                         ESource *primary_source,
                                         GdkEvent *button_event)
{
	const gchar *widget_path;

	widget_path = "/memo-list-popup";
	e_shell_view_show_popup_menu (shell_view, widget_path, button_event);

	return TRUE;
}

static void
memo_shell_view_backend_error_cb (EClientCache *client_cache,
                                  EClient *client,
                                  EAlert *alert,
                                  EMemoShellView *memo_shell_view)
{
	EMemoShellContent *memo_shell_content;
	ESource *source;
	const gchar *extension_name;

	memo_shell_content = memo_shell_view->priv->memo_shell_content;

	source = e_client_get_source (client);
	extension_name = E_SOURCE_EXTENSION_MEMO_LIST;

	/* Only submit alerts from memo list backends. */
	if (e_source_has_extension (source, extension_name)) {
		EAlertSink *alert_sink;

		alert_sink = E_ALERT_SINK (memo_shell_content);
		e_alert_sink_submit_alert (alert_sink, alert);
	}
}

static void
memo_shell_view_notify_view_id_cb (EShellView *shell_view)
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

void
e_memo_shell_view_private_init (EMemoShellView *memo_shell_view)
{
	e_signal_connect_notify (
		memo_shell_view, "notify::view-id",
		G_CALLBACK (memo_shell_view_notify_view_id_cb), NULL);
}

void
e_memo_shell_view_private_constructed (EMemoShellView *memo_shell_view)
{
	EMemoShellViewPrivate *priv = memo_shell_view->priv;
	EShellBackend *shell_backend;
	EShellContent *shell_content;
	EShellSidebar *shell_sidebar;
	EShellWindow *shell_window;
	EShellView *shell_view;
	EShell *shell;
	gulong handler_id;

	shell_view = E_SHELL_VIEW (memo_shell_view);
	shell_backend = e_shell_view_get_shell_backend (shell_view);
	shell_content = e_shell_view_get_shell_content (shell_view);
	shell_sidebar = e_shell_view_get_shell_sidebar (shell_view);
	shell_window = e_shell_view_get_shell_window (shell_view);
	shell = e_shell_window_get_shell (shell_window);

	e_shell_window_add_action_group (shell_window, "memos");
	e_shell_window_add_action_group (shell_window, "memos-filter");

	/* Cache these to avoid lots of awkward casting. */
	priv->memo_shell_backend = g_object_ref (shell_backend);
	priv->memo_shell_content = g_object_ref (shell_content);
	priv->memo_shell_sidebar = g_object_ref (shell_sidebar);

	/* Keep our own reference to this so we can
	 * disconnect our signal handlers in dispose(). */
	priv->client_cache = e_shell_get_client_cache (shell);
	g_object_ref (priv->client_cache);

	handler_id = g_signal_connect (
		priv->client_cache, "backend-error",
		G_CALLBACK (memo_shell_view_backend_error_cb),
		memo_shell_view);
	priv->backend_error_handler_id = handler_id;

	/* Keep our own reference to this so we can
	 * disconnect our signal handlers in dispose(). */
	priv->memo_table = e_memo_shell_content_get_memo_table (
		E_MEMO_SHELL_CONTENT (shell_content));
	g_object_ref (priv->memo_table);

	handler_id = g_signal_connect_swapped (
		priv->memo_table, "open-component",
		G_CALLBACK (e_memo_shell_view_open_memo),
		memo_shell_view);
	priv->open_component_handler_id = handler_id;

	handler_id = g_signal_connect_swapped (
		priv->memo_table, "popup-event",
		G_CALLBACK (memo_shell_view_table_popup_event_cb),
		memo_shell_view);
	priv->popup_event_handler_id = handler_id;

	handler_id = g_signal_connect_swapped (
		priv->memo_table, "selection-change",
		G_CALLBACK (e_memo_shell_view_update_sidebar),
		memo_shell_view);
	priv->selection_change_1_handler_id = handler_id;

	handler_id = g_signal_connect_swapped (
		priv->memo_table, "selection-change",
		G_CALLBACK (e_shell_view_update_actions),
		memo_shell_view);
	priv->selection_change_2_handler_id = handler_id;

	/* Keep our own reference to this so we can
	 * disconnect our signal handlers in dispose(). */
	priv->model = e_memo_table_get_model (priv->memo_table);
	g_object_ref (priv->model);

	handler_id = g_signal_connect_swapped (
		priv->model, "model-changed",
		G_CALLBACK (e_memo_shell_view_update_sidebar),
		memo_shell_view);
	priv->model_changed_handler_id = handler_id;

	handler_id = g_signal_connect_swapped (
		priv->model, "model-rows-deleted",
		G_CALLBACK (e_memo_shell_view_update_sidebar),
		memo_shell_view);
	priv->model_rows_deleted_handler_id = handler_id;

	handler_id = g_signal_connect_swapped (
		priv->model, "model-rows-inserted",
		G_CALLBACK (e_memo_shell_view_update_sidebar),
		memo_shell_view);
	priv->model_rows_inserted_handler_id = handler_id;

	handler_id = g_signal_connect_swapped (
		priv->model, "row-appended",
		G_CALLBACK (e_cal_base_shell_view_model_row_appended),
		memo_shell_view);
	priv->row_appended_handler_id = handler_id;

	/* Keep our own reference to this so we can
	 * disconnect our signal handlers in dispose(). */
	priv->selector = e_cal_base_shell_sidebar_get_selector (
		E_CAL_BASE_SHELL_SIDEBAR (shell_sidebar));
	g_object_ref (priv->selector);

	handler_id = g_signal_connect_swapped (
		priv->selector, "popup-event",
		G_CALLBACK (memo_shell_view_selector_popup_event_cb),
		memo_shell_view);
	priv->selector_popup_event_handler_id = handler_id;

	handler_id = g_signal_connect_swapped (
		priv->selector, "primary-selection-changed",
		G_CALLBACK (e_shell_view_update_actions),
		memo_shell_view);
	priv->primary_selection_changed_handler_id = handler_id;

	e_categories_add_change_hook (
		(GHookFunc) e_memo_shell_view_update_search_filter,
		memo_shell_view);

	e_memo_shell_view_actions_init (memo_shell_view);
	e_memo_shell_view_update_sidebar (memo_shell_view);
	e_memo_shell_view_update_search_filter (memo_shell_view);
}

void
e_memo_shell_view_private_dispose (EMemoShellView *memo_shell_view)
{
	EMemoShellViewPrivate *priv = memo_shell_view->priv;

	if (priv->backend_error_handler_id > 0) {
		g_signal_handler_disconnect (
			priv->client_cache,
			priv->backend_error_handler_id);
		priv->backend_error_handler_id = 0;
	}

	if (priv->open_component_handler_id > 0) {
		g_signal_handler_disconnect (
			priv->memo_table,
			priv->open_component_handler_id);
		priv->open_component_handler_id = 0;
	}

	if (priv->popup_event_handler_id > 0) {
		g_signal_handler_disconnect (
			priv->memo_table,
			priv->popup_event_handler_id);
		priv->popup_event_handler_id = 0;
	}

	if (priv->selection_change_1_handler_id > 0) {
		g_signal_handler_disconnect (
			priv->memo_table,
			priv->selection_change_1_handler_id);
		priv->selection_change_1_handler_id = 0;
	}

	if (priv->selection_change_2_handler_id > 0) {
		g_signal_handler_disconnect (
			priv->memo_table,
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

	if (priv->row_appended_handler_id > 0) {
		g_signal_handler_disconnect (
			priv->model,
			priv->row_appended_handler_id);
		priv->row_appended_handler_id = 0;
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

	g_clear_object (&priv->memo_shell_backend);
	g_clear_object (&priv->memo_shell_content);
	g_clear_object (&priv->memo_shell_sidebar);

	g_clear_object (&priv->client_cache);
	g_clear_object (&priv->memo_table);
	g_clear_object (&priv->model);
	g_clear_object (&priv->selector);
}

void
e_memo_shell_view_private_finalize (EMemoShellView *memo_shell_view)
{
	/* XXX Nothing to do? */
}

void
e_memo_shell_view_open_memo (EMemoShellView *memo_shell_view,
                             ECalModelComponent *comp_data)
{
	EShellContent *shell_content;
	ECalModel *model;

	g_return_if_fail (E_IS_MEMO_SHELL_VIEW (memo_shell_view));
	g_return_if_fail (E_IS_CAL_MODEL_COMPONENT (comp_data));

	shell_content = e_shell_view_get_shell_content (E_SHELL_VIEW (memo_shell_view));
	model = e_cal_base_shell_content_get_model (E_CAL_BASE_SHELL_CONTENT (shell_content));

	e_cal_ops_open_component_in_editor_sync	(model, comp_data->client, comp_data->icalcomp, FALSE);
}

void
e_memo_shell_view_update_sidebar (EMemoShellView *memo_shell_view)
{
	EMemoShellContent *memo_shell_content;
	EShellView *shell_view;
	EShellSidebar *shell_sidebar;
	EMemoTable *memo_table;
	ECalModel *model;
	GString *string;
	const gchar *format;
	gint n_rows;
	gint n_selected;

	shell_view = E_SHELL_VIEW (memo_shell_view);
	shell_sidebar = e_shell_view_get_shell_sidebar (shell_view);

	memo_shell_content = memo_shell_view->priv->memo_shell_content;
	memo_table = e_memo_shell_content_get_memo_table (memo_shell_content);

	model = e_memo_table_get_model (memo_table);

	n_rows = e_table_model_row_count (E_TABLE_MODEL (model));
	n_selected = e_table_selected_count (E_TABLE (memo_table));

	string = g_string_sized_new (64);

	format = ngettext ("%d memo", "%d memos", n_rows);
	g_string_append_printf (string, format, n_rows);

	if (n_selected > 0) {
		format = _("%d selected");
		g_string_append_len (string, ", ", 2);
		g_string_append_printf (string, format, n_selected);
	}

	e_shell_sidebar_set_secondary_text (shell_sidebar, string->str);

	g_string_free (string, TRUE);
}
