/*
 * Copyright (C) 2014 Red Hat, Inc. (www.redhat.com)
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE. See the GNU Lesser General Public License
 * for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 *
 * Authors: Milan Crha <mcrha@redhat.com>
 */

#include "evolution-config.h"

#include <gtk/gtk.h>
#include <glib/gi18n-lib.h>

#include <camel/camel.h>

#include <calendar/gui/e-cal-dialogs.h>

#include "e-cal-base-shell-content.h"
#include "e-cal-base-shell-sidebar.h"

#include "e-cal-base-shell-view.h"

struct _ECalBaseShellViewPrivate {
	EShell *shell;
	guint prepare_for_quit_handler_id;
	ESource *clicked_source;
};

enum {
	PROP_0,
	PROP_CLICKED_SOURCE
};

G_DEFINE_ABSTRACT_TYPE_WITH_PRIVATE (ECalBaseShellView, e_cal_base_shell_view, E_TYPE_SHELL_VIEW)

static void
cal_base_shell_view_prepare_for_quit_cb (EShell *shell,
					 EActivity *activity,
					 ECalBaseShellView *cal_base_shell_view)
{
	EShellContent *shell_content;

	g_return_if_fail (E_IS_CAL_BASE_SHELL_VIEW (cal_base_shell_view));

	/* Stop running searches, if any; the activity tight
	 * on the search prevents application from quitting. */
	/*e_cal_base_shell_view_search_stop (cal_base_shell_view);*/

	shell_content = e_shell_view_get_shell_content (E_SHELL_VIEW (cal_base_shell_view));
	e_cal_base_shell_content_prepare_for_quit (E_CAL_BASE_SHELL_CONTENT (shell_content), activity);
}

static void
cal_base_shell_view_get_property (GObject *object,
				  guint property_id,
				  GValue *value,
				  GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_CLICKED_SOURCE:
			g_value_set_object (
				value, e_cal_base_shell_view_get_clicked_source (
				E_SHELL_VIEW (object)));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
cal_base_shell_view_dispose (GObject *object)
{
	ECalBaseShellView *cal_base_shell_view = E_CAL_BASE_SHELL_VIEW (object);

	if (cal_base_shell_view->priv->shell && cal_base_shell_view->priv->prepare_for_quit_handler_id) {
		g_signal_handler_disconnect (cal_base_shell_view->priv->shell,
			cal_base_shell_view->priv->prepare_for_quit_handler_id);
		cal_base_shell_view->priv->prepare_for_quit_handler_id = 0;
	}

	g_clear_object (&cal_base_shell_view->priv->shell);
	g_clear_object (&cal_base_shell_view->priv->clicked_source);

	/* Chain up to parent's dispose() method. */
	G_OBJECT_CLASS (e_cal_base_shell_view_parent_class)->dispose (object);
}

static void
cal_base_shell_view_constructed (GObject *object)
{
	EShellWindow *shell_window;
	EShellView *shell_view;
	EShell *shell;
	ECalBaseShellView *cal_base_shell_view = E_CAL_BASE_SHELL_VIEW (object);

	/* Chain up to parent's constructed() method. */
	G_OBJECT_CLASS (e_cal_base_shell_view_parent_class)->constructed (object);

	shell_view = E_SHELL_VIEW (cal_base_shell_view);
	shell_window = e_shell_view_get_shell_window (shell_view);
	shell = e_shell_window_get_shell (shell_window);

	cal_base_shell_view->priv->shell = g_object_ref (shell);
	cal_base_shell_view->priv->prepare_for_quit_handler_id = g_signal_connect (
		shell, "prepare-for-quit",
		G_CALLBACK (cal_base_shell_view_prepare_for_quit_cb),
		cal_base_shell_view);
}

static void
e_cal_base_shell_view_class_init (ECalBaseShellViewClass *class)
{
	GObjectClass *object_class;

	object_class = G_OBJECT_CLASS (class);
	object_class->get_property = cal_base_shell_view_get_property;
	object_class->dispose = cal_base_shell_view_dispose;
	object_class->constructed = cal_base_shell_view_constructed;

	class->source_type = E_CAL_CLIENT_SOURCE_TYPE_LAST;

	g_object_class_install_property (
		object_class,
		PROP_CLICKED_SOURCE,
		g_param_spec_object (
			"clicked-source",
			"Clicked Source",
			"An ESource which had been clicked in the source selector before showing context menu",
			E_TYPE_SOURCE,
			G_PARAM_READABLE));
}

static void
e_cal_base_shell_view_init (ECalBaseShellView *cal_base_shell_view)
{
	cal_base_shell_view->priv = e_cal_base_shell_view_get_instance_private (cal_base_shell_view);
}

ECalClientSourceType
e_cal_base_shell_view_get_source_type (EShellView *shell_view)
{
	ECalBaseShellViewClass *base_class;

	g_return_val_if_fail (E_IS_CAL_BASE_SHELL_VIEW (shell_view), E_CAL_CLIENT_SOURCE_TYPE_LAST);

	base_class = E_CAL_BASE_SHELL_VIEW_GET_CLASS (shell_view);
	g_return_val_if_fail (base_class != NULL, E_CAL_CLIENT_SOURCE_TYPE_LAST);

	return base_class->source_type;
}

static void
cal_base_shell_view_refresh_done_cb (GObject *source_object,
				     GAsyncResult *result,
				     gpointer user_data)
{
	EClient *client;
	EActivity *activity;
	EAlertSink *alert_sink;
	ESource *source;
	const gchar *display_name;
	GError *local_error = NULL;

	g_return_if_fail (E_IS_CAL_CLIENT (source_object));

	client = E_CLIENT (source_object);
	source = e_client_get_source (client);
	activity = user_data;
	alert_sink = e_activity_get_alert_sink (activity);
	display_name = e_source_get_display_name (source);

	e_client_refresh_finish (client, result, &local_error);

	if (e_activity_handle_cancellation (activity, local_error)) {
		g_error_free (local_error);

	} else if (local_error != NULL) {
		const gchar *error_message;

		switch (e_cal_client_get_source_type (E_CAL_CLIENT (client))) {
		default:
		case E_CAL_CLIENT_SOURCE_TYPE_EVENTS:
			error_message = "calendar:refresh-error-events";
			break;
		case E_CAL_CLIENT_SOURCE_TYPE_TASKS:
			error_message = "calendar:refresh-error-tasks";
			break;
		case E_CAL_CLIENT_SOURCE_TYPE_MEMOS:
			error_message = "calendar:refresh-error-memos";
			break;
		}
		e_alert_submit (
			alert_sink,
			error_message,
			display_name, local_error->message, NULL);
		g_error_free (local_error);

	} else {
		e_activity_set_state (activity, E_ACTIVITY_COMPLETED);
	}

	g_clear_object (&activity);
}

void
e_cal_base_shell_view_allow_auth_prompt_and_refresh (EShellView *shell_view,
						     EClient *client)
{
	EShellBackend *shell_backend;
	EShellContent *shell_content;
	EShell *shell;
	EActivity *activity;
	EAlertSink *alert_sink;
	GCancellable *cancellable;

	g_return_if_fail (E_IS_SHELL_VIEW (shell_view));
	g_return_if_fail (E_IS_CLIENT (client));

	shell_backend = e_shell_view_get_shell_backend (shell_view);
	shell_content = e_shell_view_get_shell_content (shell_view);
	shell = e_shell_backend_get_shell (shell_backend);

	alert_sink = E_ALERT_SINK (shell_content);
	activity = e_activity_new ();
	cancellable = g_cancellable_new ();

	e_activity_set_alert_sink (activity, alert_sink);
	e_activity_set_cancellable (activity, cancellable);

	e_shell_allow_auth_prompt_for (shell, e_client_get_source (client));

	e_client_refresh (client, cancellable,
		cal_base_shell_view_refresh_done_cb, activity);

	e_shell_backend_add_activity (shell_backend, activity);

	g_object_unref (cancellable);
}

void
e_cal_base_shell_view_model_row_appended (EShellView *shell_view,
					  ECalModel *model)
{
	EShellSidebar *shell_sidebar;
	ESourceSelector *selector;
	ESourceRegistry *registry;
	ESource *source;
	const gchar *source_uid;

	g_return_if_fail (E_IS_CAL_BASE_SHELL_VIEW (shell_view));
	g_return_if_fail (E_IS_CAL_MODEL (model));

	/* This is the "Click to Add" handler. */

	source_uid = e_cal_model_get_default_source_uid (model);
	g_return_if_fail (source_uid != NULL);

	registry = e_cal_model_get_registry (model);
	shell_sidebar = e_shell_view_get_shell_sidebar (shell_view);
	selector = e_cal_base_shell_sidebar_get_selector (E_CAL_BASE_SHELL_SIDEBAR (shell_sidebar));

	source = e_source_registry_ref_source (registry, source_uid);
	g_return_if_fail (source != NULL);

	/* Make sure the source where the component was added is selected,
	   thus the added component is visible in the view */
	e_source_selector_select_source (selector, source);

	g_clear_object (&source);
}

void
e_cal_base_shell_view_copy_calendar (EShellView *shell_view)
{
	EShellContent *shell_content;
	EShellSidebar *shell_sidebar;
	EShellWindow *shell_window;
	ESourceSelector *selector;
	ESource *from_source;
	ECalModel *model;

	g_return_if_fail (E_IS_SHELL_VIEW (shell_view));

	shell_content = e_shell_view_get_shell_content (shell_view);
	shell_sidebar = e_shell_view_get_shell_sidebar (shell_view);
	shell_window = e_shell_view_get_shell_window (shell_view);

	g_return_if_fail (E_IS_CAL_BASE_SHELL_CONTENT (shell_content));
	g_return_if_fail (E_IS_CAL_BASE_SHELL_SIDEBAR (shell_sidebar));

	model = e_cal_base_shell_content_get_model (E_CAL_BASE_SHELL_CONTENT (shell_content));
	selector = e_cal_base_shell_sidebar_get_selector (E_CAL_BASE_SHELL_SIDEBAR (shell_sidebar));

	from_source = e_source_selector_ref_primary_selection (selector);
	g_return_if_fail (from_source != NULL);

	e_cal_dialogs_copy_source (GTK_WINDOW (shell_window), model, from_source);

	g_clear_object (&from_source);
}

static gboolean
cal_base_shell_view_cleanup_clicked_source_idle_cb (gpointer user_data)
{
	ECalBaseShellView *cal_base_shell_view = user_data;

	g_return_val_if_fail (E_IS_CAL_BASE_SHELL_VIEW (cal_base_shell_view), FALSE);

	g_clear_object (&cal_base_shell_view->priv->clicked_source);
	g_clear_object (&cal_base_shell_view);

	return FALSE;
}

static void
cal_base_shell_view_popup_menu_hidden_cb (GObject *object,
					  GParamSpec *param,
					  gpointer user_data)
{
	ECalBaseShellView *cal_base_shell_view = user_data;

	g_return_if_fail (E_IS_CAL_BASE_SHELL_VIEW (cal_base_shell_view));

	/* Cannot do the clean up immediately, because the menu is hidden before
	   the action is executed. */
	g_idle_add (cal_base_shell_view_cleanup_clicked_source_idle_cb, cal_base_shell_view);

	g_signal_handlers_disconnect_by_func (object, cal_base_shell_view_popup_menu_hidden_cb, user_data);
}

GtkWidget *
e_cal_base_shell_view_show_popup_menu (EShellView *shell_view,
				       const gchar *widget_path,
				       GdkEvent *button_event,
				       ESource *clicked_source)
{
	ECalBaseShellView *cal_base_shell_view;
	GtkWidget *menu;

	g_return_val_if_fail (E_IS_CAL_BASE_SHELL_VIEW (shell_view), NULL);
	g_return_val_if_fail (widget_path != NULL, NULL);
	if (clicked_source)
		g_return_val_if_fail (E_IS_SOURCE (clicked_source), NULL);

	cal_base_shell_view = E_CAL_BASE_SHELL_VIEW (shell_view);

	g_clear_object (&cal_base_shell_view->priv->clicked_source);
	if (clicked_source)
		cal_base_shell_view->priv->clicked_source = g_object_ref (clicked_source);

	menu = e_shell_view_show_popup_menu (shell_view, widget_path, button_event);

	if (menu) {
		g_signal_connect (menu, "notify::visible",
			G_CALLBACK (cal_base_shell_view_popup_menu_hidden_cb), g_object_ref (shell_view));
	} else {
		g_clear_object (&cal_base_shell_view->priv->clicked_source);
	}

	return menu;
}

ESource *
e_cal_base_shell_view_get_clicked_source (EShellView *shell_view)
{
	ECalBaseShellView *cal_base_shell_view;

	g_return_val_if_fail (E_IS_CAL_BASE_SHELL_VIEW (shell_view), NULL);

	cal_base_shell_view = E_CAL_BASE_SHELL_VIEW (shell_view);

	return cal_base_shell_view->priv->clicked_source;
}

static void
cal_base_shell_view_refresh_backend_done_cb (GObject *source_object,
					     GAsyncResult *result,
					     gpointer user_data)
{
	ESourceRegistry *registry;
	EActivity *activity = user_data;
	EAlertSink *alert_sink;
	GError *local_error = NULL;

	g_return_if_fail (E_IS_SOURCE_REGISTRY (source_object));

	registry = E_SOURCE_REGISTRY (source_object);
	alert_sink = e_activity_get_alert_sink (activity);

	e_source_registry_refresh_backend_finish (registry, result, &local_error);

	if (e_activity_handle_cancellation (activity, local_error)) {
		g_error_free (local_error);

	} else if (local_error != NULL) {
		e_alert_submit (alert_sink, "system:refresh-backend-failed", local_error->message, NULL);
		g_error_free (local_error);

	} else {
		e_activity_set_state (activity, E_ACTIVITY_COMPLETED);
	}

	g_clear_object (&activity);
}

void
e_cal_base_shell_view_refresh_backend (EShellView *shell_view,
				       ESource *source)
{
	EShellBackend *shell_backend;
	EShellContent *shell_content;
	EShell *shell;
	EActivity *activity;
	EAlertSink *alert_sink;
	ESourceRegistry *registry;
	GCancellable *cancellable;

	g_return_if_fail (E_IS_CAL_BASE_SHELL_VIEW (shell_view));
	g_return_if_fail (E_IS_SOURCE (source));

	shell_backend = e_shell_view_get_shell_backend (shell_view);
	shell_content = e_shell_view_get_shell_content (shell_view);
	shell = e_shell_backend_get_shell (shell_backend);

	alert_sink = E_ALERT_SINK (shell_content);
	activity = e_activity_new ();
	cancellable = g_cancellable_new ();

	e_activity_set_alert_sink (activity, alert_sink);
	e_activity_set_cancellable (activity, cancellable);

	registry = e_shell_get_registry (shell);

	e_source_registry_refresh_backend (registry, e_source_get_uid (source), cancellable,
		cal_base_shell_view_refresh_backend_done_cb, activity);

	e_shell_backend_add_activity (shell_backend, activity);

	g_object_unref (cancellable);
}

void
e_cal_base_shell_view_preselect_source_config (EShellView *shell_view,
					       GtkWidget *source_config)
{
	ESource *clicked_source, *primary_source, *use_source = NULL;

	g_return_if_fail (E_IS_CAL_BASE_SHELL_VIEW (shell_view));
	g_return_if_fail (E_IS_SOURCE_CONFIG (source_config));

	clicked_source = e_cal_base_shell_view_get_clicked_source (shell_view);
	primary_source = e_source_selector_ref_primary_selection (e_cal_base_shell_sidebar_get_selector (
		E_CAL_BASE_SHELL_SIDEBAR (e_shell_view_get_shell_sidebar (shell_view))));

	if (clicked_source && clicked_source != primary_source)
		use_source = clicked_source;
	else if (primary_source)
		use_source = primary_source;

	if (use_source) {
		ESourceBackend *source_backend = NULL;

		if (e_source_has_extension (use_source, E_SOURCE_EXTENSION_COLLECTION))
			source_backend = e_source_get_extension (use_source, E_SOURCE_EXTENSION_COLLECTION);
		else if (e_source_has_extension (use_source, E_SOURCE_EXTENSION_CALENDAR))
			source_backend = e_source_get_extension (use_source, E_SOURCE_EXTENSION_CALENDAR);
		else if (e_source_has_extension (use_source, E_SOURCE_EXTENSION_MEMO_LIST))
			source_backend = e_source_get_extension (use_source, E_SOURCE_EXTENSION_MEMO_LIST);
		else if (e_source_has_extension (use_source, E_SOURCE_EXTENSION_TASK_LIST))
			source_backend = e_source_get_extension (use_source, E_SOURCE_EXTENSION_TASK_LIST);

		if (source_backend)
			e_source_config_set_preselect_type (E_SOURCE_CONFIG (source_config), e_source_backend_get_backend_name (source_backend));
		else if (use_source == clicked_source)
			e_source_config_set_preselect_type (E_SOURCE_CONFIG (source_config), e_source_get_uid (use_source));
	}

	g_clear_object (&primary_source);
}
