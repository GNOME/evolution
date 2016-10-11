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

#define E_CAL_BASE_SHELL_VIEW_GET_PRIVATE(obj) \
	(G_TYPE_INSTANCE_GET_PRIVATE \
	((obj), E_TYPE_CAL_BASE_SHELL_VIEW, ECalBaseShellViewPrivate))

G_DEFINE_ABSTRACT_TYPE (ECalBaseShellView, e_cal_base_shell_view, E_TYPE_SHELL_VIEW)

struct _ECalBaseShellViewPrivate {
	EShell *shell;
	guint prepare_for_quit_handler_id;
};

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
cal_base_shell_view_dispose (GObject *object)
{
	ECalBaseShellView *cal_base_shell_view = E_CAL_BASE_SHELL_VIEW (object);

	if (cal_base_shell_view->priv->shell && cal_base_shell_view->priv->prepare_for_quit_handler_id) {
		g_signal_handler_disconnect (cal_base_shell_view->priv->shell,
			cal_base_shell_view->priv->prepare_for_quit_handler_id);
		cal_base_shell_view->priv->prepare_for_quit_handler_id = 0;
	}

	g_clear_object (&cal_base_shell_view->priv->shell);

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

	g_type_class_add_private (class, sizeof (ECalBaseShellViewPrivate));

	object_class = G_OBJECT_CLASS (class);
	object_class->dispose = cal_base_shell_view_dispose;
	object_class->constructed = cal_base_shell_view_constructed;

	class->source_type = E_CAL_CLIENT_SOURCE_TYPE_LAST;
}

static void
e_cal_base_shell_view_init (ECalBaseShellView *cal_base_shell_view)
{
	cal_base_shell_view->priv = E_CAL_BASE_SHELL_VIEW_GET_PRIVATE (cal_base_shell_view);
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
