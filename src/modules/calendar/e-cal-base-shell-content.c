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

#include <string.h>
#include <glib/gi18n-lib.h>

#include "e-cal-base-shell-sidebar.h"
#include "e-cal-base-shell-view.h"
#include "e-cal-base-shell-content.h"

struct _ECalBaseShellContentPrivate {
	ECalDataModel *data_model;
	ECalModel *model;
	gulong object_created_id;
	gulong view_state_changed_id;
};

enum {
	PROP_0,
	PROP_DATA_MODEL,
	PROP_MODEL
};

G_DEFINE_ABSTRACT_TYPE_WITH_PRIVATE (ECalBaseShellContent, e_cal_base_shell_content, E_TYPE_SHELL_CONTENT)

static void
cal_base_shell_content_client_opened_cb (ECalBaseShellSidebar *cal_base_shell_sidebar,
					 ECalClient *client,
					 ECalBaseShellContent *shell_content)
{
	g_return_if_fail (E_IS_CAL_CLIENT (client));
	g_return_if_fail (E_IS_CAL_BASE_SHELL_CONTENT (shell_content));

	e_cal_data_model_add_client (shell_content->priv->data_model, client);
}

static void
cal_base_shell_content_client_closed_cb (ECalBaseShellSidebar *cal_base_shell_sidebar,
					 ESource *source,
					 ECalBaseShellContent *shell_content)
{
	g_return_if_fail (E_IS_SOURCE (source));
	g_return_if_fail (E_IS_CAL_BASE_SHELL_CONTENT (shell_content));

	e_cal_data_model_remove_client (shell_content->priv->data_model, e_source_get_uid (source));
}

static void
cal_base_shell_content_primary_selection_changed_cb (ESourceSelector *selector,
						     GParamSpec *param,
						     ECalBaseShellContent *shell_content)
{
	ESource *source;

	g_return_if_fail (E_IS_SOURCE_SELECTOR (selector));
	g_return_if_fail (E_IS_CAL_BASE_SHELL_CONTENT (shell_content));

	source = e_source_selector_ref_primary_selection (selector);
	if (source)
		e_cal_model_set_default_source_uid (shell_content->priv->model, e_source_get_uid (source));

	g_clear_object (&source);
}

static void
cal_base_shell_content_object_created_cb (ECalBaseShellContent *cal_base_shell_content,
					  ECalClient *client,
					  ECalModel *model)
{
	EShellView *shell_view;
	EShellSidebar *shell_sidebar;
	ESourceSelector *selector;

	g_return_if_fail (E_IS_CAL_BASE_SHELL_CONTENT (cal_base_shell_content));
	g_return_if_fail (E_IS_CAL_CLIENT (client));

	shell_view = e_shell_content_get_shell_view (E_SHELL_CONTENT (cal_base_shell_content));
	g_return_if_fail (E_IS_SHELL_VIEW (shell_view));

	shell_sidebar = e_shell_view_get_shell_sidebar (shell_view);
	g_return_if_fail (E_IS_SHELL_SIDEBAR (shell_sidebar));

	selector = e_cal_base_shell_sidebar_get_selector (E_CAL_BASE_SHELL_SIDEBAR (shell_sidebar));
	e_source_selector_select_source (selector, e_client_get_source (E_CLIENT (client)));
}

static void
cal_base_shell_content_view_state_changed_cb (ECalDataModel *data_model,
					      ECalClientView *view,
					      ECalDataModelViewState state,
					      guint percent,
					      const gchar *message,
					      const GError *error,
					      ECalBaseShellContent *cal_base_shell_content)
{
	EShellView *shell_view;
	EShellSidebar *shell_sidebar;
	ESourceSelector *selector;
	ESource *source;
	ECalClient *client;

	shell_view = e_shell_content_get_shell_view (E_SHELL_CONTENT (cal_base_shell_content));
	g_return_if_fail (E_IS_SHELL_VIEW (shell_view));

	shell_sidebar = e_shell_view_get_shell_sidebar (shell_view);
	g_return_if_fail (E_IS_SHELL_SIDEBAR (shell_sidebar));

	selector = e_cal_base_shell_sidebar_get_selector (E_CAL_BASE_SHELL_SIDEBAR (shell_sidebar));
	client = e_cal_client_view_ref_client (view);
	/* Can be NULL when the corresponding source had been removed or disabled */
	if (!client)
		return;

	source = e_client_get_source (E_CLIENT (client));
	g_clear_object (&client);

	if (state == E_CAL_DATA_MODEL_VIEW_STATE_START ||
	    state == E_CAL_DATA_MODEL_VIEW_STATE_PROGRESS) {
		e_source_selector_set_source_is_busy (selector, source, state == E_CAL_DATA_MODEL_VIEW_STATE_START || (message && *message) || percent > 0);

		if (message && *message) {
			gchar *tooltip = NULL;

			if (percent > 0) {
				/* Translators: This is a running activity whose percent complete is known. */
				tooltip = g_strdup_printf (_("%s (%d%% complete)"), message, percent);
			}

			e_source_selector_set_source_tooltip (selector, source, tooltip ? tooltip : message);

			g_free (tooltip);
		} else {
			e_source_selector_set_source_tooltip (selector, source, NULL);
		}
	} else {
		e_source_selector_set_source_is_busy (selector, source, FALSE);
		e_source_selector_set_source_tooltip (selector, source, NULL);
	}
}

static void
cal_base_shell_content_view_created_cb (EShellWindow *shell_window,
					EShellView *shell_view,
					ECalBaseShellContent *cal_base_shell_content)
{
	EShellSidebar *shell_sidebar;
	ECalBaseShellContentClass *klass;
	ESourceSelector *selector;

	g_signal_handlers_disconnect_by_func (
		shell_window,
		cal_base_shell_content_view_created_cb, cal_base_shell_content);

	g_return_if_fail (E_IS_CAL_BASE_SHELL_CONTENT (cal_base_shell_content));

	shell_view = e_shell_content_get_shell_view (E_SHELL_CONTENT (cal_base_shell_content));
	g_return_if_fail (E_IS_SHELL_VIEW (shell_view));

	shell_sidebar = e_shell_view_get_shell_sidebar (shell_view);
	g_return_if_fail (E_IS_SHELL_SIDEBAR (shell_sidebar));

	g_signal_connect (shell_sidebar, "client-opened",
		G_CALLBACK (cal_base_shell_content_client_opened_cb), cal_base_shell_content);
	g_signal_connect (shell_sidebar, "client-closed",
		G_CALLBACK (cal_base_shell_content_client_closed_cb), cal_base_shell_content);

	cal_base_shell_content->priv->object_created_id = g_signal_connect_swapped (
		cal_base_shell_content->priv->model, "object-created",
		G_CALLBACK (cal_base_shell_content_object_created_cb), cal_base_shell_content);

	selector = e_cal_base_shell_sidebar_get_selector (E_CAL_BASE_SHELL_SIDEBAR (shell_sidebar));
	g_signal_connect (selector, "notify::primary-selection",
		G_CALLBACK (cal_base_shell_content_primary_selection_changed_cb), cal_base_shell_content);

	cal_base_shell_content->priv->view_state_changed_id = g_signal_connect (
		cal_base_shell_content->priv->data_model, "view-state-changed",
		G_CALLBACK (cal_base_shell_content_view_state_changed_cb), cal_base_shell_content);

	e_cal_base_shell_sidebar_ensure_sources_open (E_CAL_BASE_SHELL_SIDEBAR (shell_sidebar));

	klass = E_CAL_BASE_SHELL_CONTENT_GET_CLASS (cal_base_shell_content);
	g_return_if_fail (klass != NULL);

	if (klass->view_created)
		klass->view_created (cal_base_shell_content);
}

static GCancellable *
cal_base_shell_content_submit_data_model_thread_job (GObject *responder,
						     const gchar *description,
						     const gchar *alert_ident,
						     const gchar *alert_arg_0,
						     EAlertSinkThreadJobFunc func,
						     gpointer user_data,
						     GDestroyNotify free_user_data)
{
	EShellView *shell_view;
	EActivity *activity;
	GCancellable *cancellable = NULL;

	g_return_val_if_fail (E_IS_CAL_BASE_SHELL_CONTENT (responder), NULL);

	shell_view = e_shell_content_get_shell_view (E_SHELL_CONTENT (responder));
	activity = e_shell_view_submit_thread_job (shell_view, description,
		alert_ident, alert_arg_0, func, user_data, free_user_data);

	if (activity) {
		cancellable = e_activity_get_cancellable (activity);
		if (cancellable)
			g_object_ref (cancellable);
		g_object_unref (activity);
	}

	return cancellable;
}

static void
cal_base_shell_content_get_property (GObject *object,
				     guint property_id,
				     GValue *value,
				     GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_DATA_MODEL:
			g_value_set_object (
				value, e_cal_base_shell_content_get_data_model (
				E_CAL_BASE_SHELL_CONTENT (object)));
			return;

		case PROP_MODEL:
			g_value_set_object (
				value, e_cal_base_shell_content_get_model (
				E_CAL_BASE_SHELL_CONTENT (object)));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
cal_base_shell_content_dispose (GObject *object)
{
	ECalBaseShellContent *cal_base_shell_content;

	cal_base_shell_content = E_CAL_BASE_SHELL_CONTENT (object);

	e_cal_data_model_set_disposing (cal_base_shell_content->priv->data_model, TRUE);

	if (cal_base_shell_content->priv->view_state_changed_id != 0) {
		g_signal_handler_disconnect (cal_base_shell_content->priv->data_model,
			cal_base_shell_content->priv->view_state_changed_id);
		cal_base_shell_content->priv->view_state_changed_id = 0;
	}

	if (cal_base_shell_content->priv->object_created_id != 0) {
		g_signal_handler_disconnect (cal_base_shell_content->priv->model,
			cal_base_shell_content->priv->object_created_id);
		cal_base_shell_content->priv->object_created_id = 0;
	}

	/* Chain up to parent's method. */
	G_OBJECT_CLASS (e_cal_base_shell_content_parent_class)->dispose (object);
}

static void
cal_base_shell_content_finalize (GObject *object)
{
	ECalBaseShellContent *cal_base_shell_content;

	cal_base_shell_content = E_CAL_BASE_SHELL_CONTENT (object);

	if (cal_base_shell_content->priv->model &&
	    cal_base_shell_content->priv->data_model)
		e_cal_data_model_unsubscribe (cal_base_shell_content->priv->data_model,
			E_CAL_DATA_MODEL_SUBSCRIBER (cal_base_shell_content->priv->model));

	g_clear_object (&cal_base_shell_content->priv->model);
	g_clear_object (&cal_base_shell_content->priv->data_model);

	/* Chain up to parent's method. */
	G_OBJECT_CLASS (e_cal_base_shell_content_parent_class)->finalize (object);
}

static void
cal_base_shell_content_constructed (GObject *object)
{
	EShell *shell;
	EShellView *shell_view;
	EShellWindow *shell_window;
	ECalBaseShellContent *cal_base_shell_content;
	ECalBaseShellContentClass *klass;
	ESourceRegistry *registry;
	ESource *default_source = NULL;
	GSettings *settings;
	const gchar *created_signal_name = NULL;

	/* Chain up to parent's method. */
	G_OBJECT_CLASS (e_cal_base_shell_content_parent_class)->constructed (object);

	cal_base_shell_content = E_CAL_BASE_SHELL_CONTENT (object);
	cal_base_shell_content->priv->data_model = e_cal_base_shell_content_create_new_data_model (cal_base_shell_content);

	klass = E_CAL_BASE_SHELL_CONTENT_GET_CLASS (cal_base_shell_content);
	g_return_if_fail (klass != NULL);
	g_return_if_fail (klass->new_cal_model != NULL);

	shell_view = e_shell_content_get_shell_view (E_SHELL_CONTENT (cal_base_shell_content));
	shell_window = e_shell_view_get_shell_window (shell_view);
	shell = e_shell_window_get_shell (shell_window);
	registry = e_shell_get_registry (shell);

	cal_base_shell_content->priv->model = klass->new_cal_model (
		cal_base_shell_content->priv->data_model, registry, shell);

	e_binding_bind_property (
		cal_base_shell_content->priv->model, "timezone",
		cal_base_shell_content->priv->data_model, "timezone",
		G_BINDING_SYNC_CREATE);

	switch (e_cal_base_shell_view_get_source_type (shell_view)) {
		case E_CAL_CLIENT_SOURCE_TYPE_EVENTS:
			e_cal_data_model_set_expand_recurrences (cal_base_shell_content->priv->data_model, TRUE);
			default_source = e_source_registry_ref_default_calendar (registry);
			created_signal_name = "shell-view-created::calendar";

			settings = e_util_ref_settings ("org.gnome.evolution.calendar");
			g_settings_bind (
				settings, "hide-cancelled-events",
				cal_base_shell_content->priv->data_model, "skip-cancelled",
				G_SETTINGS_BIND_GET);
			g_object_unref (settings);
			break;
		case E_CAL_CLIENT_SOURCE_TYPE_MEMOS:
			default_source = e_source_registry_ref_default_memo_list (registry);
			created_signal_name = "shell-view-created::memos";
			break;
		case E_CAL_CLIENT_SOURCE_TYPE_TASKS:
			default_source = e_source_registry_ref_default_task_list (registry);
			created_signal_name = "shell-view-created::tasks";
			break;
		case E_CAL_CLIENT_SOURCE_TYPE_LAST:
			g_warn_if_reached ();
			return;
	}

	e_cal_model_set_default_source_uid (cal_base_shell_content->priv->model, e_source_get_uid (default_source));

	g_clear_object (&default_source);

	g_signal_connect (
		shell_window, created_signal_name,
		G_CALLBACK (cal_base_shell_content_view_created_cb),
		cal_base_shell_content);
}

static void
e_cal_base_shell_content_class_init (ECalBaseShellContentClass *class)
{
	GObjectClass *object_class;

	object_class = G_OBJECT_CLASS (class);
	object_class->get_property = cal_base_shell_content_get_property;
	object_class->dispose = cal_base_shell_content_dispose;
	object_class->finalize = cal_base_shell_content_finalize;
	object_class->constructed = cal_base_shell_content_constructed;

	g_object_class_install_property (
		object_class,
		PROP_DATA_MODEL,
		g_param_spec_object (
			"data-model",
			NULL,
			NULL,
			E_TYPE_CAL_DATA_MODEL,
			G_PARAM_READABLE));

	g_object_class_install_property (
		object_class,
		PROP_MODEL,
		g_param_spec_object (
			"model",
			NULL,
			NULL,
			E_TYPE_CAL_MODEL,
			G_PARAM_READABLE));
}

static void
e_cal_base_shell_content_init (ECalBaseShellContent *cal_base_shell_content)
{
	cal_base_shell_content->priv = e_cal_base_shell_content_get_instance_private (cal_base_shell_content);
}

ECalDataModel *
e_cal_base_shell_content_get_data_model (ECalBaseShellContent *cal_base_shell_content)
{
	g_return_val_if_fail (E_IS_CAL_BASE_SHELL_CONTENT (cal_base_shell_content), NULL);

	return cal_base_shell_content->priv->data_model;
}

ECalModel *
e_cal_base_shell_content_get_model (ECalBaseShellContent *cal_base_shell_content)
{
	g_return_val_if_fail (E_IS_CAL_BASE_SHELL_CONTENT (cal_base_shell_content), NULL);

	return cal_base_shell_content->priv->model;
}

void
e_cal_base_shell_content_prepare_for_quit (ECalBaseShellContent *cal_base_shell_content,
					   EActivity *activity)
{
	ECalBaseShellContentClass *klass;

	g_return_if_fail (E_IS_CAL_BASE_SHELL_CONTENT (cal_base_shell_content));

	klass = E_CAL_BASE_SHELL_CONTENT_GET_CLASS (cal_base_shell_content);
	g_return_if_fail (klass != NULL);

	if (klass->prepare_for_quit)
		klass->prepare_for_quit (cal_base_shell_content, activity);
}

ECalDataModel *
e_cal_base_shell_content_create_new_data_model (ECalBaseShellContent *cal_base_shell_content)
{
	EShellView *shell_view;
	ESourceRegistry *registry;

	g_return_val_if_fail (E_IS_CAL_BASE_SHELL_CONTENT (cal_base_shell_content), NULL);

	shell_view = e_shell_content_get_shell_view (E_SHELL_CONTENT (cal_base_shell_content));
	registry = e_shell_get_registry (e_shell_window_get_shell (e_shell_view_get_shell_window (shell_view)));

	return e_cal_data_model_new (registry, cal_base_shell_content_submit_data_model_thread_job, G_OBJECT (cal_base_shell_content));
}
