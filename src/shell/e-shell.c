/*
 * e-shell.c
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

/**
 * SECTION: e-shell
 * @short_description: the backbone of Evolution
 * @include: shell/e-shell.h
 **/

#include "evolution-config.h"

#include "e-shell.h"

#include <errno.h>
#include <glib/gi18n.h>
#include <glib/gstdio.h>
#include <libebackend/libebackend.h>
#include <libedataserver/libedataserver.h>

#include "e-util/e-util-private.h"

#include "e-shell-backend.h"
#include "e-shell-enumtypes.h"
#include "e-shell-window.h"
#include "e-shell-utils.h"

#define E_SHELL_GET_PRIVATE(obj) \
	(G_TYPE_INSTANCE_GET_PRIVATE \
	((obj), E_TYPE_SHELL, EShellPrivate))

#define SET_ONLINE_TIMEOUT_SECONDS 5

struct _EShellPrivate {
	GQueue alerts;
	ESourceRegistry *registry;
	ECredentialsPrompter *credentials_prompter;
	EClientCache *client_cache;
	GtkWidget *preferences_window;
	GCancellable *cancellable;

	/* Shell Backends */
	GList *loaded_backends;              /* not referenced */
	GHashTable *backends_by_name;
	GHashTable *backends_by_scheme;

	gboolean preparing_for_online;
	gpointer preparing_for_line_change;  /* weak pointer */
	gpointer preparing_for_quit;         /* weak pointer */

	gchar *geometry;
	gchar *module_directory;

	guint inhibit_cookie;
	guint set_online_timeout_id;
	guint prepare_quit_timeout_id;

	gulong backend_died_handler_id;
	gulong allow_auth_prompt_handler_id;
	gulong get_dialog_parent_handler_id;
	gulong credentials_required_handler_id;

	guint auto_reconnect : 1;
	guint express_mode : 1;
	guint modules_loaded : 1;
	guint network_available : 1;
	guint network_available_set : 1;
	guint network_available_locked : 1;
	guint online : 1;
	guint quit_cancelled : 1;
	guint ready_to_quit : 1;
	guint safe_mode : 1;
	guint requires_shutdown : 1;
};

enum {
	PROP_0,
	PROP_CLIENT_CACHE,
	PROP_EXPRESS_MODE,
	PROP_GEOMETRY,
	PROP_MODULE_DIRECTORY,
	PROP_NETWORK_AVAILABLE,
	PROP_ONLINE,
	PROP_REGISTRY,
	PROP_CREDENTIALS_PROMPTER
};

enum {
	EVENT,
	HANDLE_URI,
	PREPARE_FOR_OFFLINE,
	PREPARE_FOR_ONLINE,
	PREPARE_FOR_QUIT,
	QUIT_REQUESTED,
	LAST_SIGNAL
};

static gpointer default_shell;
static guint signals[LAST_SIGNAL];

/* Forward Declarations */
static void e_shell_initable_init (GInitableIface *iface);

G_DEFINE_TYPE_WITH_CODE (
	EShell,
	e_shell,
	GTK_TYPE_APPLICATION,
	G_IMPLEMENT_INTERFACE (
		G_TYPE_INITABLE, e_shell_initable_init)
	G_IMPLEMENT_INTERFACE (
		E_TYPE_EXTENSIBLE, NULL))

static void
shell_alert_response_cb (EShell *shell,
                         gint response_id,
                         EAlert *alert)
{
	g_signal_handlers_disconnect_by_func (
		alert, shell_alert_response_cb, shell);

	g_queue_remove (&shell->priv->alerts, alert);

	g_object_unref (alert);
}

static void
shell_notify_online_cb (EShell *shell)
{
	gboolean online;

	online = e_shell_get_online (shell);
	e_passwords_set_online (online);
}

static void
shell_window_removed_cb (EShell *shell)
{
	g_return_if_fail (E_IS_SHELL (shell));

	if (!gtk_application_get_windows (GTK_APPLICATION (shell)) &&
	    !shell->priv->ready_to_quit)
		e_shell_quit (shell, E_SHELL_QUIT_LAST_WINDOW);
}

static gboolean
shell_window_delete_event_cb (GtkWindow *window,
                              GdkEvent *event,
                              GtkApplication *application)
{
	/* If other windows are open we can safely close this one. */
	if (g_list_length (gtk_application_get_windows (application)) > 1)
		return FALSE;

	/* Otherwise we initiate application quit. */
	e_shell_quit (E_SHELL (application), E_SHELL_QUIT_LAST_WINDOW);

	return TRUE;
}

static void
shell_action_new_window_cb (GSimpleAction *action,
                            GVariant *parameter,
                            EShell *shell)
{
	GtkApplication *application;
	const gchar *view_name;

	application = GTK_APPLICATION (shell);

	view_name = parameter ? g_variant_get_string (parameter, NULL) : NULL;
	if (view_name && !*view_name)
		view_name = NULL;

	if (view_name) {
		GList *list;
		gboolean get_current = g_strcmp0 (view_name, "current") == 0;

		list = gtk_application_get_windows (application);

		/* Present the first EShellWindow showing 'view_name'. */
		while (list != NULL) {
			GtkWindow *window = GTK_WINDOW (list->data);

			if (E_IS_SHELL_WINDOW (window)) {
				const gchar *active_view;

				active_view = e_shell_window_get_active_view (
					E_SHELL_WINDOW (window));
				if (g_strcmp0 (active_view, view_name) == 0) {
					gtk_window_present (window);
					return;
				} else if (get_current && active_view) {
					view_name = active_view;
					break;
				}
			}

			list = g_list_next (list);
		}
	} else {
		GtkWindow *window;

		window = e_shell_get_active_window (shell);

		if (E_IS_SHELL_WINDOW (window))
			view_name = e_shell_window_get_active_view (E_SHELL_WINDOW (window));
	}

	/* No suitable EShellWindow found, so create one. */
	e_shell_create_shell_window (shell, view_name);
}

static void
shell_action_handle_uris_cb (GSimpleAction *action,
                             GVariant *parameter,
                             EShell *shell)
{
	const gchar **uris;
	gchar *change_dir = NULL;
	gint ii;

	/* Do not use g_strfreev() here. */
	uris = g_variant_get_strv (parameter, NULL);
	if (uris && g_strcmp0 (uris[0], "--use-cwd") == 0 && uris[1] && *uris[1]) {
		change_dir = g_get_current_dir ();

		if (g_chdir (uris[1]) != 0)
			g_warning ("%s: Failed to change directory to '%s': %s", G_STRFUNC, uris[1], g_strerror (errno));

		for (ii = 0; uris[ii + 2]; ii++) {
			uris[ii] = uris[ii + 2];
		}

		uris[ii] = NULL;
	}

	e_shell_handle_uris (shell, uris, FALSE);
	g_free (uris);

	if (change_dir) {
		if (g_chdir (change_dir) != 0)
			g_warning ("%s: Failed to return back to '%s': %s", G_STRFUNC, change_dir, g_strerror (errno));

		g_free (change_dir);
	}
}

static void
shell_action_quit_cb (GSimpleAction *action,
                      GVariant *parameter,
                      EShell *shell)
{
	e_shell_quit (shell, E_SHELL_QUIT_REMOTE_REQUEST);
}

static void
shell_add_actions (GApplication *application)
{
	GActionMap *action_map;
	GSimpleAction *action;

	action_map = G_ACTION_MAP (application);

	/* Add actions that remote instances can invoke. */

	action = g_simple_action_new ("create-from-remote", G_VARIANT_TYPE_STRING);
	g_signal_connect (
		action, "activate",
		G_CALLBACK (shell_action_new_window_cb), application);
	g_action_map_add_action (action_map, G_ACTION (action));
	g_object_unref (action);

	action = g_simple_action_new (
		"handle-uris", G_VARIANT_TYPE_STRING_ARRAY);
	g_signal_connect (
		action, "activate",
		G_CALLBACK (shell_action_handle_uris_cb), application);
	g_action_map_add_action (action_map, G_ACTION (action));
	g_object_unref (action);

	action = g_simple_action_new ("quit", NULL);
	g_signal_connect (
		action, "activate",
		G_CALLBACK (shell_action_quit_cb), application);
	g_action_map_add_action (action_map, G_ACTION (action));
	g_object_unref (action);
}

static gboolean
e_shell_set_online_cb (gpointer user_data)
{
	EShell *shell = user_data;

	g_return_val_if_fail (E_IS_SHELL (shell), FALSE);

	shell->priv->set_online_timeout_id = 0;

	e_shell_set_online (shell, TRUE);

	return FALSE;
}

static void
shell_ready_for_online_change (EShell *shell,
			       EActivity *activity,
			       gboolean is_last_ref)
{
	gboolean is_cancelled;

	if (!is_last_ref)
		return;

	/* Increment the reference count so we can safely emit
	 * a signal without triggering the toggle reference. */
	g_object_ref (activity);

	is_cancelled = e_activity_get_state (activity) == E_ACTIVITY_CANCELLED ||
		g_cancellable_is_cancelled (e_activity_get_cancellable (activity));
	e_activity_set_state (activity, is_cancelled ? E_ACTIVITY_CANCELLED : E_ACTIVITY_COMPLETED);

	g_object_remove_toggle_ref (
		G_OBJECT (activity), (GToggleNotify)
		shell_ready_for_online_change, shell);

	/* Finalize the activity. */
	g_object_unref (activity);

	if (!is_cancelled)
		shell->priv->online = shell->priv->preparing_for_online;

	g_object_notify (G_OBJECT (shell), "online");
}

static void
shell_cancel_ongoing_preparing_line_change (EShell *shell)
{
	EActivity *activity;

	activity = g_object_ref (shell->priv->preparing_for_line_change);
	shell->priv->preparing_for_line_change = NULL;

	g_object_remove_toggle_ref (G_OBJECT (activity), (GToggleNotify) shell_ready_for_online_change, shell);

	g_object_remove_weak_pointer (G_OBJECT (activity), &shell->priv->preparing_for_line_change);

	e_activity_cancel (activity);

	g_clear_object (&activity);
}

static void
shell_prepare_for_offline (EShell *shell)
{
	/* Are preparations already in progress? */
	if (shell->priv->preparing_for_line_change != NULL)
		shell_cancel_ongoing_preparing_line_change (shell);

	shell->priv->preparing_for_line_change = e_activity_new ();
	shell->priv->preparing_for_online = FALSE;

	e_activity_set_text (
		shell->priv->preparing_for_line_change,
		_("Preparing to go offline..."));

	g_object_add_toggle_ref (
		G_OBJECT (shell->priv->preparing_for_line_change),
		(GToggleNotify) shell_ready_for_online_change, shell);

	g_object_add_weak_pointer (
		G_OBJECT (shell->priv->preparing_for_line_change),
		&shell->priv->preparing_for_line_change);

	g_signal_emit (
		shell, signals[PREPARE_FOR_OFFLINE], 0,
		shell->priv->preparing_for_line_change);

	g_object_unref (shell->priv->preparing_for_line_change);
}

static void
shell_prepare_for_online (EShell *shell)
{
	/* Are preparations already in progress? */
	if (shell->priv->preparing_for_line_change != NULL)
		shell_cancel_ongoing_preparing_line_change (shell);

	shell->priv->preparing_for_line_change = e_activity_new ();
	shell->priv->preparing_for_online = TRUE;

	e_activity_set_text (
		shell->priv->preparing_for_line_change,
		_("Preparing to go online..."));

	g_object_add_toggle_ref (
		G_OBJECT (shell->priv->preparing_for_line_change),
		(GToggleNotify) shell_ready_for_online_change, shell);

	g_object_add_weak_pointer (
		G_OBJECT (shell->priv->preparing_for_line_change),
		&shell->priv->preparing_for_line_change);

	g_signal_emit (
		shell, signals[PREPARE_FOR_ONLINE], 0,
		shell->priv->preparing_for_line_change);

	g_object_unref (shell->priv->preparing_for_line_change);
}

static void
shell_ready_for_quit (EShell *shell,
                      EActivity *activity,
                      gboolean is_last_ref)
{
	GtkApplication *application;
	GList *list;

	g_return_if_fail (E_IS_SHELL (shell));

	if (!is_last_ref)
		return;

	shell->priv->ready_to_quit = TRUE;

	application = GTK_APPLICATION (shell);

	/* Increment the reference count so we can safely emit
	 * a signal without triggering the toggle reference. */
	g_object_ref (activity);

	e_activity_set_state (activity, E_ACTIVITY_COMPLETED);

	g_object_remove_toggle_ref (
		G_OBJECT (activity), (GToggleNotify)
		shell_ready_for_quit, shell);

	/* Finalize the activity. */
	g_object_unref (activity);

	/* XXX Inhibiting session manager actions currently only
	 *     works on GNOME, so check that we obtained a valid
	 *     inhibit cookie before attempting to uninhibit. */
	if (shell->priv->inhibit_cookie > 0) {
		gtk_application_uninhibit (
			application, shell->priv->inhibit_cookie);
		shell->priv->inhibit_cookie = 0;
	}

	if (shell->priv->prepare_quit_timeout_id) {
		g_source_remove (shell->priv->prepare_quit_timeout_id);
		shell->priv->prepare_quit_timeout_id = 0;
	}

	/* Destroy all watched windows.  Note, we iterate over a -copy-
	 * of the watched windows list because the act of destroying a
	 * watched window will modify the watched windows list, which
	 * would derail the iteration. */
	list = g_list_copy (gtk_application_get_windows (application));
	g_list_foreach (list, (GFunc) gtk_widget_destroy, NULL);
	g_list_free (list);

	if (gtk_main_level () > 0)
		gtk_main_quit ();
}

static gboolean
shell_ask_quit_with_pending_activities (EShell *shell)
{
	GList *windows;

	windows = gtk_application_get_windows (GTK_APPLICATION (shell));

	return e_alert_run_dialog_for_args (windows ? windows->data : NULL,
		"shell:ask-quit-with-pending", NULL) == GTK_RESPONSE_OK;
}

static gboolean
shell_prepare_for_quit_timeout_cb (gpointer user_data);

static void
shell_prepare_for_quit (EShell *shell)
{
	GtkApplication *application;
	GList *list, *iter;

	/* Are preparations already in progress? */
	if (shell->priv->preparing_for_quit != NULL) {
		if (shell_ask_quit_with_pending_activities (shell)) {
			e_activity_cancel (shell->priv->preparing_for_quit);
			camel_operation_cancel_all ();

			shell_ready_for_quit (shell, shell->priv->preparing_for_quit, TRUE);
		}
		return;
	}

	application = GTK_APPLICATION (shell);

	shell->priv->inhibit_cookie = gtk_application_inhibit (
		application, NULL,
		GTK_APPLICATION_INHIBIT_SWITCH |
		GTK_APPLICATION_INHIBIT_LOGOUT |
		GTK_APPLICATION_INHIBIT_SUSPEND,
		_("Preparing to quit"));

	shell->priv->preparing_for_quit = e_activity_new ();

	e_activity_set_text (
		shell->priv->preparing_for_quit,
		_("Preparing to quit..."));

	g_object_add_toggle_ref (
		G_OBJECT (shell->priv->preparing_for_quit),
		(GToggleNotify) shell_ready_for_quit, shell);

	g_object_add_weak_pointer (
		G_OBJECT (shell->priv->preparing_for_quit),
		&shell->priv->preparing_for_quit);

	g_signal_emit (
		shell, signals[PREPARE_FOR_QUIT], 0,
		shell->priv->preparing_for_quit);

	shell->priv->prepare_quit_timeout_id =
		e_named_timeout_add_seconds (60, shell_prepare_for_quit_timeout_cb, shell);

	g_object_unref (shell->priv->preparing_for_quit);

	/* Desensitize all watched windows to prevent user action. */
	list = gtk_application_get_windows (application);
	for (iter = list; iter != NULL; iter = iter->next)
		gtk_widget_set_sensitive (GTK_WIDGET (iter->data), FALSE);
}

static gboolean
shell_prepare_for_quit_timeout_cb (gpointer user_data)
{
	EShell *shell = user_data;

	g_return_val_if_fail (E_IS_SHELL (shell), FALSE);
	g_return_val_if_fail (shell->priv->preparing_for_quit != 0, FALSE);

	shell->priv->prepare_quit_timeout_id = 0;

	/* This asks whether to quit or wait and does all the work */
	shell_prepare_for_quit (shell);

	return FALSE;
}

static gboolean
shell_request_quit (EShell *shell,
                    EShellQuitReason reason)
{
	/* Are preparations already in progress? */
	if (shell->priv->preparing_for_quit != NULL)
		return TRUE;

	/* Give the application a chance to cancel quit. */
	shell->priv->quit_cancelled = FALSE;
	g_signal_emit (shell, signals[QUIT_REQUESTED], 0, reason);

	return !shell->priv->quit_cancelled;
}

/* Helper for shell_add_backend() */
static void
shell_split_and_insert_items (GHashTable *hash_table,
                              const gchar *items,
                              EShellBackend *shell_backend)
{
	gpointer key;
	gchar **strv;
	gint ii;

	strv = g_strsplit_set (items, ":", -1);

	for (ii = 0; strv[ii] != NULL; ii++) {
		key = (gpointer) g_intern_string (strv[ii]);
		g_hash_table_insert (hash_table, key, shell_backend);
	}

	g_strfreev (strv);
}

static void
shell_process_backend (EShellBackend *shell_backend,
                       EShell *shell)
{
	EShellBackendClass *class;
	GHashTable *backends_by_name;
	GHashTable *backends_by_scheme;
	const gchar *string;

	class = E_SHELL_BACKEND_GET_CLASS (shell_backend);
	backends_by_name = shell->priv->backends_by_name;
	backends_by_scheme = shell->priv->backends_by_scheme;

	if ((string = class->name) != NULL)
		g_hash_table_insert (
			backends_by_name, (gpointer)
			g_intern_string (string), shell_backend);

	if ((string = class->aliases) != NULL)
		shell_split_and_insert_items (
			backends_by_name, string, shell_backend);

	if ((string = class->schemes) != NULL)
		shell_split_and_insert_items (
			backends_by_scheme, string, shell_backend);
}

static void
shell_backend_died_cb (EClientCache *client_cache,
                       EClient *client,
                       EAlert *alert,
                       EShell *shell)
{
	/* Backend crashes are quite serious.
	 * Post the alert to all shell views. */
	e_shell_submit_alert (shell, alert);
}

static void
shell_allow_auth_prompt_cb (EClientCache *client_cache,
			    ESource *source,
			    EShell *shell)
{
	g_return_if_fail (E_IS_SOURCE (source));
	g_return_if_fail (E_IS_SHELL (shell));

	e_shell_allow_auth_prompt_for (shell, source);
}

static void
shell_source_connection_status_notify_cb (ESource *source,
					  GParamSpec *param,
					  EAlert *alert)
{
	g_return_if_fail (E_IS_ALERT (alert));

	if (e_source_get_connection_status (source) == E_SOURCE_CONNECTION_STATUS_DISCONNECTED ||
	    e_source_get_connection_status (source) == E_SOURCE_CONNECTION_STATUS_CONNECTING ||
	    e_source_get_connection_status (source) == E_SOURCE_CONNECTION_STATUS_CONNECTED)
		e_alert_response (alert, GTK_RESPONSE_CLOSE);
}

static void
shell_submit_source_connection_alert (EShell *shell,
				      ESource *source,
				      EAlert *alert)
{
	g_return_if_fail (E_IS_SHELL (shell));
	g_return_if_fail (E_IS_SOURCE (source));
	g_return_if_fail (E_IS_ALERT (alert));

	e_signal_connect_notify_object (source, "notify::connection-status",
		G_CALLBACK (shell_source_connection_status_notify_cb), alert, 0);

	e_shell_submit_alert (shell, alert);
}

static void
shell_source_invoke_authenticate_cb (GObject *source_object,
				     GAsyncResult *result,
				     gpointer user_data)
{
	ESource *source;
	EShell *shell = user_data;
	GError *error = NULL;

	g_return_if_fail (E_IS_SOURCE (source_object));

	source = E_SOURCE (source_object);

	if (!e_source_invoke_authenticate_finish (source, result, &error)) {
		/* Can be cancelled only if the shell is disposing/disposed */
		if (error && !g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED)) {
			EAlert *alert;
			gchar *display_name;

			g_return_if_fail (E_IS_SHELL (shell));

			display_name = e_util_get_source_full_name (shell->priv->registry, source);
			alert = e_alert_new ("shell:source-invoke-authenticate-failed",
				display_name,
				error->message,
				NULL);
			e_shell_submit_alert (shell, alert);
			g_object_unref (alert);
			g_free (display_name);
		}

		g_clear_error (&error);
	}
}

#define SOURCE_ALERT_KEY_SOURCE			"source-alert-key-source"
#define SOURCE_ALERT_KEY_CERTIFICATE_PEM	"source-alert-key-certificate-pem"
#define SOURCE_ALERT_KEY_CERTIFICATE_ERRORS	"source-alert-key-certificate-errors"
#define SOURCE_ALERT_KEY_ERROR_TEXT		"source-alert-key-error-text"

static void
shell_trust_prompt_done_cb (GObject *source_object,
			    GAsyncResult *result,
			    gpointer user_data)
{
	ESource *source;
	EShell *shell = user_data;
	ETrustPromptResponse response = E_TRUST_PROMPT_RESPONSE_UNKNOWN;
	GError *error = NULL;

	g_return_if_fail (E_IS_SOURCE (source_object));

	source = E_SOURCE (source_object);

	if (!e_trust_prompt_run_for_source_finish (source, result, &response, &error)) {
		/* Can be cancelled only if the shell is disposing/disposed */
		if (error && !g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED)) {
			EAlert *alert;
			gchar *display_name;

			g_return_if_fail (E_IS_SHELL (shell));

			display_name = e_util_get_source_full_name (shell->priv->registry, source);
			alert = e_alert_new ("shell:source-trust-prompt-failed",
				display_name,
				error->message,
				NULL);
			e_shell_submit_alert (shell, alert);
			g_object_unref (alert);
			g_free (display_name);
		}

		g_clear_error (&error);
		return;
	}

	g_return_if_fail (E_IS_SHELL (shell));

	if (response == E_TRUST_PROMPT_RESPONSE_UNKNOWN) {
		e_credentials_prompter_set_auto_prompt_disabled_for (shell->priv->credentials_prompter, source, TRUE);
		return;
	}

	/* If a credentials prompt is required, then it'll be shown immediately. */
	e_credentials_prompter_set_auto_prompt_disabled_for (shell->priv->credentials_prompter, source, FALSE);

	/* NULL credentials to retry with those used the last time */
	e_source_invoke_authenticate (source, NULL, shell->priv->cancellable,
		shell_source_invoke_authenticate_cb, shell);
}

static void
shell_credentials_prompt_done_cb (GObject *source_object,
				  GAsyncResult *result,
				  gpointer user_data)
{
	EShell *shell = user_data;
	ESource *source = NULL;
	ENamedParameters *credentials = NULL;
	GError *error = NULL;

	g_return_if_fail (E_IS_SHELL (shell));

	if (e_credentials_prompter_prompt_finish (E_CREDENTIALS_PROMPTER (source_object), result, &source, &credentials, &error)) {
		e_source_invoke_authenticate (source, credentials, shell->priv->cancellable,
			shell_source_invoke_authenticate_cb, shell);
	} else if (error && !g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED)) {
		EAlert *alert;
		gchar *display_name;

		g_return_if_fail (E_IS_SHELL (shell));

		display_name = e_util_get_source_full_name (shell->priv->registry, source);
		alert = e_alert_new ("shell:source-credentials-prompt-failed",
			display_name,
			error->message,
			NULL);
		e_shell_submit_alert (shell, alert);
		g_object_unref (alert);
		g_free (display_name);
	}

	e_named_parameters_free (credentials);
	g_clear_object (&source);
	g_clear_object (&shell);
	g_clear_error (&error);
}

static void
shell_connection_error_alert_response_cb (EAlert *alert,
					  gint response_id,
					  EShell *shell)
{
	ESource *source;

	g_return_if_fail (E_IS_SHELL (shell));

	if (response_id != GTK_RESPONSE_APPLY)
		return;

	source = g_object_get_data (G_OBJECT (alert), SOURCE_ALERT_KEY_SOURCE);
	g_return_if_fail (E_IS_SOURCE (source));

	e_credentials_prompter_set_auto_prompt_disabled_for (shell->priv->credentials_prompter, source, FALSE);

	e_credentials_prompter_prompt (shell->priv->credentials_prompter, source, NULL,
		E_CREDENTIALS_PROMPTER_PROMPT_FLAG_ALLOW_STORED_CREDENTIALS,
		shell_credentials_prompt_done_cb, g_object_ref (shell));
}

static void
shell_connect_error_open_settings_goa_clicked_cb (GtkButton *button,
						  EAlert *alert)
{
	const gchar *account_id;
	gchar *command_line, *control_center_path;
	GError *error = NULL;

	/* The SOURCE_ALERT_KEY_SOURCE is not an ESource here */
	account_id = g_object_get_data (G_OBJECT (button), SOURCE_ALERT_KEY_SOURCE);

	control_center_path = g_find_program_in_path ("gnome-control-center");
	command_line = g_strjoin (
		" ",
		control_center_path,
		"online-accounts",
		account_id,
		NULL);

	g_spawn_command_line_async (command_line, &error);

	g_free (command_line);
	g_free (control_center_path);

	if (error != NULL) {
		g_warning ("%s: %s", G_STRFUNC, error->message);
		g_error_free (error);
	}
}

static void
shell_connect_trust_error_alert_response_cb (EAlert *alert,
					     gint response_id,
					     EShell *shell)
{
	ESource *source;
	const gchar *certificate_pem;
	GTlsCertificateFlags certificate_errors;
	const gchar *error_text;

	g_return_if_fail (E_IS_SHELL (shell));

	if (response_id != GTK_RESPONSE_APPLY)
		return;

	source = g_object_get_data (G_OBJECT (alert), SOURCE_ALERT_KEY_SOURCE);
	certificate_pem = g_object_get_data (G_OBJECT (alert), SOURCE_ALERT_KEY_CERTIFICATE_PEM);
	certificate_errors = GPOINTER_TO_UINT (g_object_get_data (G_OBJECT (alert), SOURCE_ALERT_KEY_CERTIFICATE_ERRORS));
	error_text = g_object_get_data (G_OBJECT (alert), SOURCE_ALERT_KEY_ERROR_TEXT);

	g_return_if_fail (E_IS_SOURCE (source));

	g_object_set_data_full (G_OBJECT (source), SOURCE_ALERT_KEY_CERTIFICATE_PEM, g_strdup (certificate_pem), g_free);

	e_trust_prompt_run_for_source (gtk_application_get_active_window (GTK_APPLICATION (shell)),
		source, certificate_pem, certificate_errors, error_text, TRUE,
		shell->priv->cancellable, shell_trust_prompt_done_cb, shell);
}

static void
shell_maybe_add_connect_error_goa_button (EAlert *alert,
					  ESource *source,
					  ESourceRegistry *registry)
{
	gchar *account_id = NULL;

	g_return_if_fail (E_IS_ALERT (alert));
	g_return_if_fail (E_IS_SOURCE (source));
	g_return_if_fail (E_IS_SOURCE_REGISTRY (registry));

	if (e_source_has_extension (source, E_SOURCE_EXTENSION_GOA)) {
		account_id = e_source_goa_dup_account_id (e_source_get_extension (source, E_SOURCE_EXTENSION_GOA));
	} else if (e_source_get_parent (source)) {
		ESource *parent;

		parent = e_source_registry_ref_source (registry, e_source_get_parent (source));
		if (parent && e_source_has_extension (parent, E_SOURCE_EXTENSION_GOA))
			account_id = e_source_goa_dup_account_id (e_source_get_extension (parent, E_SOURCE_EXTENSION_GOA));

		g_clear_object (&parent);
	}

	if (account_id) {
		gchar *control_center_path;

		control_center_path = g_find_program_in_path ("gnome-control-center");

		if (!control_center_path || !*control_center_path) {
			g_free (account_id);
			account_id = NULL;
		}

		g_free (control_center_path);
	}

	if (account_id) {
		GtkWidget *button;

		button = gtk_button_new_with_mnemonic (_("Open _Settings"));
		/* The SOURCE_ALERT_KEY_SOURCE is not an ESource here */
		g_object_set_data_full (G_OBJECT (button), SOURCE_ALERT_KEY_SOURCE, g_strdup (account_id), g_free);
		gtk_widget_show (button);

		g_signal_connect (button, "clicked",
			G_CALLBACK (shell_connect_error_open_settings_goa_clicked_cb), alert);

		e_alert_add_widget (alert, button);
	}

	g_free (account_id);
}

static const gchar *
shell_get_connection_error_tag_for_source (ESource *source)
{
	const gchar *tag = "shell:source-connection-error";
	const gchar *override_tag = NULL;

	g_return_val_if_fail (E_IS_SOURCE (source), tag);

	if (e_source_has_extension (source, E_SOURCE_EXTENSION_ADDRESS_BOOK)) {
		override_tag = "shell:addressbook-connection-error";
	}

	if (e_source_has_extension (source, E_SOURCE_EXTENSION_CALENDAR)) {
		if (!override_tag)
			override_tag = "shell:calendar-connection-error";
		else
			override_tag = "";
	}

	if (e_source_has_extension (source, E_SOURCE_EXTENSION_MAIL_ACCOUNT) ||
	    e_source_has_extension (source, E_SOURCE_EXTENSION_MAIL_TRANSPORT)) {
		if (!override_tag)
			override_tag = "shell:mail-connection-error";
		else
			override_tag = "";
	}

	if (e_source_has_extension (source, E_SOURCE_EXTENSION_MEMO_LIST)) {
		if (!override_tag)
			override_tag = "shell:memo-list-connection-error";
		else
			override_tag = "";
	}

	if (e_source_has_extension (source, E_SOURCE_EXTENSION_TASK_LIST)) {
		if (!override_tag)
			override_tag = "shell:task-list-connection-error";
		else
			override_tag = "";
	}

	if (override_tag && *override_tag)
		return override_tag;

	return tag;
}

static const gchar *
shell_get_connection_trust_error_tag_for_source (ESource *source)
{
	const gchar *tag = "shell:source-connection-trust-error";
	const gchar *override_tag = NULL;

	g_return_val_if_fail (E_IS_SOURCE (source), tag);

	if (e_source_has_extension (source, E_SOURCE_EXTENSION_ADDRESS_BOOK)) {
		override_tag = "shell:addressbook-connection-trust-error";
	}

	if (e_source_has_extension (source, E_SOURCE_EXTENSION_CALENDAR)) {
		if (!override_tag)
			override_tag = "shell:calendar-connection-trust-error";
		else
			override_tag = "";
	}

	if (e_source_has_extension (source, E_SOURCE_EXTENSION_MAIL_ACCOUNT) ||
	    e_source_has_extension (source, E_SOURCE_EXTENSION_MAIL_TRANSPORT)) {
		if (!override_tag)
			override_tag = "shell:mail-connection-trust-error";
		else
			override_tag = "";
	}

	if (e_source_has_extension (source, E_SOURCE_EXTENSION_MEMO_LIST)) {
		if (!override_tag)
			override_tag = "shell:memo-list-connection-trust-error";
		else
			override_tag = "";
	}

	if (e_source_has_extension (source, E_SOURCE_EXTENSION_TASK_LIST)) {
		if (!override_tag)
			override_tag = "shell:task-list-connection-trust-error";
		else
			override_tag = "";
	}

	if (override_tag && *override_tag)
		return override_tag;

	return tag;
}

static void
shell_process_credentials_required_errors (EShell *shell,
					   ESource *source,
					   ESourceCredentialsReason reason,
					   const gchar *certificate_pem,
					   GTlsCertificateFlags certificate_errors,
					   const GError *op_error)
{
	g_return_if_fail (E_IS_SHELL (shell));
	g_return_if_fail (E_IS_SOURCE (source));

	/* Skip disabled sources */
	if (!e_source_registry_check_enabled (shell->priv->registry, source))
		return;

	switch (reason) {
	case E_SOURCE_CREDENTIALS_REASON_UNKNOWN:
		/* This should not be here */
		g_warn_if_reached ();
		return;
	case E_SOURCE_CREDENTIALS_REASON_REQUIRED:
	case E_SOURCE_CREDENTIALS_REASON_REJECTED:
		/* These are handled by the credentials prompter, if not disabled */
		if (e_credentials_prompter_get_auto_prompt_disabled_for (shell->priv->credentials_prompter, source))
			break;

		return;
	case E_SOURCE_CREDENTIALS_REASON_SSL_FAILED:
	case E_SOURCE_CREDENTIALS_REASON_ERROR:
		break;
	}

	if (reason == E_SOURCE_CREDENTIALS_REASON_ERROR) {
		EAlert *alert;
		gchar *display_name;

		display_name = e_util_get_source_full_name (shell->priv->registry, source);
		alert = e_alert_new (shell_get_connection_error_tag_for_source (source),
				display_name,
				op_error && *(op_error->message) ? op_error->message : _("Unknown error"),
				NULL);
		g_free (display_name);

		shell_maybe_add_connect_error_goa_button (alert, source, shell->priv->registry);

		g_signal_connect (alert, "response", G_CALLBACK (shell_connection_error_alert_response_cb), shell);
		g_object_set_data_full (G_OBJECT (alert), SOURCE_ALERT_KEY_SOURCE, g_object_ref (source), g_object_unref);

		shell_submit_source_connection_alert (shell, source, alert);
		g_object_unref (alert);
	} else if (reason == E_SOURCE_CREDENTIALS_REASON_SSL_FAILED) {
		if (e_credentials_prompter_get_auto_prompt_disabled_for (shell->priv->credentials_prompter, source)) {
			/* Only show an alert */
			EAlert *alert;
			gchar *cert_errors_str;
			gchar *display_name;

			cert_errors_str = e_trust_prompt_describe_certificate_errors (certificate_errors);

			display_name = e_util_get_source_full_name (shell->priv->registry, source);
			alert = e_alert_new (shell_get_connection_trust_error_tag_for_source (source),
					display_name,
					(cert_errors_str && *cert_errors_str) ? cert_errors_str :
					op_error && *(op_error->message) ? op_error->message : _("Unknown error"),
					NULL);
			g_free (display_name);

			g_signal_connect (alert, "response", G_CALLBACK (shell_connect_trust_error_alert_response_cb), shell);

			g_object_set_data_full (G_OBJECT (alert), SOURCE_ALERT_KEY_SOURCE, g_object_ref (source), g_object_unref);
			g_object_set_data_full (G_OBJECT (alert), SOURCE_ALERT_KEY_CERTIFICATE_PEM, g_strdup (certificate_pem), g_free);
			g_object_set_data (G_OBJECT (alert), SOURCE_ALERT_KEY_CERTIFICATE_ERRORS, GUINT_TO_POINTER (certificate_errors));
			g_object_set_data_full (G_OBJECT (alert), SOURCE_ALERT_KEY_ERROR_TEXT, op_error ? g_strdup (op_error->message) : NULL, g_free);

			shell_submit_source_connection_alert (shell, source, alert);

			g_free (cert_errors_str);
			g_object_unref (alert);
		} else {
			g_object_set_data_full (G_OBJECT (source), SOURCE_ALERT_KEY_CERTIFICATE_PEM, g_strdup (certificate_pem), g_free);

			e_trust_prompt_run_for_source (gtk_application_get_active_window (GTK_APPLICATION (shell)),
				source, certificate_pem, certificate_errors, op_error ? op_error->message : NULL, TRUE,
				shell->priv->cancellable, shell_trust_prompt_done_cb, shell);
		}
	} else if (reason == E_SOURCE_CREDENTIALS_REASON_REQUIRED ||
		   reason == E_SOURCE_CREDENTIALS_REASON_REJECTED) {
		EAlert *alert;
		gchar *display_name;

		display_name = e_util_get_source_full_name (shell->priv->registry, source);
		alert = e_alert_new (shell_get_connection_error_tag_for_source (source),
				display_name,
				op_error && *(op_error->message) ? op_error->message : _("Credentials are required to connect to the destination host."),
				NULL);
		g_free (display_name);

		shell_maybe_add_connect_error_goa_button (alert, source, shell->priv->registry);

		g_signal_connect (alert, "response", G_CALLBACK (shell_connection_error_alert_response_cb), shell);
		g_object_set_data_full (G_OBJECT (alert), SOURCE_ALERT_KEY_SOURCE, g_object_ref (source), g_object_unref);

		shell_submit_source_connection_alert (shell, source, alert);
		g_object_unref (alert);
	} else {
		g_warn_if_reached ();
	}
}

static void
shell_get_last_credentials_required_arguments_cb (GObject *source_object,
						  GAsyncResult *result,
						  gpointer user_data)
{
	EShell *shell = user_data;
	ESource *source;
	ESourceCredentialsReason reason = E_SOURCE_CREDENTIALS_REASON_UNKNOWN;
	gchar *certificate_pem = NULL;
	GTlsCertificateFlags certificate_errors = 0;
	GError *op_error = NULL;
	GError *error = NULL;

	g_return_if_fail (E_IS_SOURCE (source_object));

	source = E_SOURCE (source_object);

	if (!e_source_get_last_credentials_required_arguments_finish (source, result, &reason,
		&certificate_pem, &certificate_errors, &op_error, &error)) {
		/* Can be cancelled only if the shell is disposing/disposed */
		if (error && !g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED)) {
			EAlert *alert;
			gchar *display_name;

			g_return_if_fail (E_IS_SHELL (shell));

			display_name = e_util_get_source_full_name (shell->priv->registry, source);
			alert = e_alert_new ("shell:source-get-values-failed",
				display_name,
				error->message,
				NULL);
			e_shell_submit_alert (shell, alert);
			g_object_unref (alert);
			g_free (display_name);
		}

		g_clear_error (&error);
		return;
	}

	g_return_if_fail (E_IS_SHELL (shell));

	if (reason != E_SOURCE_CREDENTIALS_REASON_UNKNOWN)
		shell_process_credentials_required_errors (shell, source, reason, certificate_pem, certificate_errors, op_error);

	g_free (certificate_pem);
	g_clear_error (&op_error);
}

static void
shell_process_failed_authentications (EShell *shell)
{
	GList *sources, *link;

	g_return_if_fail (E_IS_SHELL (shell));

	sources = e_source_registry_list_enabled (shell->priv->registry, NULL);

	for (link = sources; link; link = g_list_next (link)) {
		ESource *source = link->data;

		if (source && (
		    e_source_get_connection_status (source) == E_SOURCE_CONNECTION_STATUS_DISCONNECTED ||
		    e_source_get_connection_status (source) == E_SOURCE_CONNECTION_STATUS_SSL_FAILED)) {
			/* Only show alerts, do not open windows */
			e_credentials_prompter_set_auto_prompt_disabled_for (shell->priv->credentials_prompter, source, TRUE);

			e_source_get_last_credentials_required_arguments (source, shell->priv->cancellable,
				shell_get_last_credentials_required_arguments_cb, shell);
		}
	}

	g_list_free_full (sources, g_object_unref);
}

static void
shell_credentials_required_cb (ESourceRegistry *registry,
			       ESource *source,
			       ESourceCredentialsReason reason,
			       const gchar *certificate_pem,
			       GTlsCertificateFlags certificate_errors,
			       const GError *op_error,
			       EShell *shell)
{
	g_return_if_fail (E_IS_SHELL (shell));

	shell_process_credentials_required_errors (shell, source, reason, certificate_pem, certificate_errors, op_error);
}

static GtkWindow *
shell_get_dialog_parent_cb (ECredentialsPrompter *prompter,
			    EShell *shell)
{
	GList *windows, *link;

	g_return_val_if_fail (E_IS_SHELL (shell), NULL);

	windows = gtk_application_get_windows (GTK_APPLICATION (shell));
	for (link = windows; link; link = g_list_next (link)) {
		GtkWindow *window = link->data;

		if (E_IS_SHELL_WINDOW (window))
			return window;
	}

	return NULL;
}

static void
shell_app_menu_activate_cb (GSimpleAction *action,
			    GVariant *parameter,
			    gpointer user_data)
{
	EShell *shell = user_data;
	const gchar *name;

	g_return_if_fail (G_IS_ACTION (action));
	g_return_if_fail (E_IS_SHELL (shell));

	name = g_action_get_name (G_ACTION (action));
	g_return_if_fail (name != NULL);

	if (g_str_equal (name, "new-window")) {
		shell_action_new_window_cb (action, parameter, shell);
	} else if (g_str_equal (name, "preferences")) {
		e_shell_utils_run_preferences (shell);
	} else if (g_str_equal (name, "quick-reference")) {
		e_shell_utils_run_quick_reference (shell);
	} else if (g_str_equal (name, "help")) {
		e_shell_utils_run_help_contents (shell);
	} else if (g_str_equal (name, "about")) {
		e_shell_utils_run_help_about (shell);
	} else {
		g_warning ("%s: Unknown app-menu action '%s'", G_STRFUNC, name);
	}
}

static void
shell_create_app_menu (GtkApplication *application)
{
	const GActionEntry actions[] = {
		{ "new-window", shell_app_menu_activate_cb, NULL, NULL, NULL },
		{ "preferences", shell_app_menu_activate_cb, NULL, NULL, NULL },
		{ "quick-reference", shell_app_menu_activate_cb, NULL, NULL, NULL },
		{ "help", shell_app_menu_activate_cb, NULL, NULL, NULL },
		{ "about", shell_app_menu_activate_cb, NULL, NULL, NULL }
	};
	GMenu *app_menu, *section;

	g_return_if_fail (GTK_IS_APPLICATION (application));

	app_menu = g_menu_new ();

	section = g_menu_new ();
	g_menu_append (section, _("New _Window"), "app.new-window");
	g_menu_append_section (app_menu, NULL, G_MENU_MODEL (section));
	g_object_unref (section);

	section = g_menu_new ();
	g_menu_append (section, _("_Preferences"), "app.preferences");
	g_menu_append_section (app_menu, NULL, G_MENU_MODEL (section));
	g_object_unref (section);

	section = g_menu_new ();
	if (e_shell_utils_is_quick_reference_available (E_SHELL (application)))
		g_menu_append (section, _("Quick _Reference"), "app.quick-reference");
	g_menu_append (section, _("_Help"), "app.help");
	g_menu_append (section, _("_About"), "app.about");
	g_menu_append (section, _("_Quit"), "app.quit");
	g_menu_append_section (app_menu, NULL, G_MENU_MODEL (section));
	g_object_unref (section);

	gtk_application_set_app_menu (application, G_MENU_MODEL (app_menu));
	g_action_map_add_action_entries (G_ACTION_MAP (application), actions, G_N_ELEMENTS (actions), application);

	g_object_unref (app_menu);
}

static void
shell_sm_quit_cb (EShell *shell,
                  gpointer user_data)
{
	if (!shell->priv->ready_to_quit)
		shell_prepare_for_quit (shell);
}

static void
shell_set_express_mode (EShell *shell,
                        gboolean express_mode)
{
	shell->priv->express_mode = express_mode;
}

static void
shell_set_geometry (EShell *shell,
                    const gchar *geometry)
{
	g_return_if_fail (shell->priv->geometry == NULL);

	shell->priv->geometry = g_strdup (geometry);
}

static void
shell_set_module_directory (EShell *shell,
                            const gchar *module_directory)
{
	g_return_if_fail (shell->priv->module_directory == NULL);

	shell->priv->module_directory = g_strdup (module_directory);
}

static void
shell_set_property (GObject *object,
                    guint property_id,
                    const GValue *value,
                    GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_EXPRESS_MODE:
			shell_set_express_mode (
				E_SHELL (object),
				g_value_get_boolean (value));
			return;

		case PROP_GEOMETRY:
			shell_set_geometry (
				E_SHELL (object),
				g_value_get_string (value));
			return;

		case PROP_MODULE_DIRECTORY:
			shell_set_module_directory (
				E_SHELL (object),
				g_value_get_string (value));
			return;

		case PROP_NETWORK_AVAILABLE:
			e_shell_set_network_available (
				E_SHELL (object),
				g_value_get_boolean (value));
			return;

		case PROP_ONLINE:
			e_shell_set_online (
				E_SHELL (object),
				g_value_get_boolean (value));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
shell_get_property (GObject *object,
                    guint property_id,
                    GValue *value,
                    GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_CLIENT_CACHE:
			g_value_set_object (
				value, e_shell_get_client_cache (
				E_SHELL (object)));
			return;

		case PROP_EXPRESS_MODE:
			g_value_set_boolean (
				value, e_shell_get_express_mode (
				E_SHELL (object)));
			return;

		case PROP_MODULE_DIRECTORY:
			g_value_set_string (
				value, e_shell_get_module_directory (
				E_SHELL (object)));
			return;

		case PROP_NETWORK_AVAILABLE:
			g_value_set_boolean (
				value, e_shell_get_network_available (
				E_SHELL (object)));
			return;

		case PROP_ONLINE:
			g_value_set_boolean (
				value, e_shell_get_online (
				E_SHELL (object)));
			return;

		case PROP_REGISTRY:
			g_value_set_object (
				value, e_shell_get_registry (
				E_SHELL (object)));
			return;

		case PROP_CREDENTIALS_PROMPTER:
			g_value_set_object (
				value, e_shell_get_credentials_prompter (
				E_SHELL (object)));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
shell_dispose (GObject *object)
{
	EShellPrivate *priv;
	EAlert *alert;

	priv = E_SHELL_GET_PRIVATE (object);

	if (priv->set_online_timeout_id > 0) {
		g_source_remove (priv->set_online_timeout_id);
		priv->set_online_timeout_id = 0;
	}

	if (priv->prepare_quit_timeout_id) {
		g_source_remove (priv->prepare_quit_timeout_id);
		priv->prepare_quit_timeout_id = 0;
	}

	if (priv->cancellable) {
		g_cancellable_cancel (priv->cancellable);
		g_clear_object (&priv->cancellable);
	}

	while ((alert = g_queue_pop_head (&priv->alerts)) != NULL) {
		g_signal_handlers_disconnect_by_func (
			alert, shell_alert_response_cb, object);
		g_object_unref (alert);
	}

	while ((alert = g_queue_pop_head (&priv->alerts)) != NULL) {
		g_signal_handlers_disconnect_by_func (
			alert, shell_alert_response_cb, object);
		g_object_unref (alert);
	}

	if (priv->backend_died_handler_id > 0) {
		g_signal_handler_disconnect (
			priv->client_cache,
			priv->backend_died_handler_id);
		priv->backend_died_handler_id = 0;
	}

	if (priv->allow_auth_prompt_handler_id > 0) {
		g_signal_handler_disconnect (
			priv->client_cache,
			priv->allow_auth_prompt_handler_id);
		priv->allow_auth_prompt_handler_id = 0;
	}

	if (priv->credentials_required_handler_id > 0) {
		g_signal_handler_disconnect (
			priv->registry,
			priv->credentials_required_handler_id);
		priv->credentials_required_handler_id = 0;
	}

	if (priv->get_dialog_parent_handler_id > 0) {
		g_signal_handler_disconnect (
			priv->credentials_prompter,
			priv->get_dialog_parent_handler_id);
		priv->get_dialog_parent_handler_id = 0;
	}

	g_clear_object (&priv->registry);
	g_clear_object (&priv->credentials_prompter);
	g_clear_object (&priv->client_cache);

	if (priv->preferences_window) {
		gtk_widget_destroy (priv->preferences_window);
		priv->preferences_window = NULL;
	}

	if (priv->preparing_for_line_change != NULL) {
		g_object_remove_weak_pointer (
			G_OBJECT (priv->preparing_for_line_change),
			&priv->preparing_for_line_change);
	}

	/* Chain up to parent's dispose() method. */
	G_OBJECT_CLASS (e_shell_parent_class)->dispose (object);
}

static void
shell_finalize (GObject *object)
{
	EShellPrivate *priv;

	priv = E_SHELL_GET_PRIVATE (object);

	g_hash_table_destroy (priv->backends_by_name);
	g_hash_table_destroy (priv->backends_by_scheme);

	g_list_foreach (priv->loaded_backends, (GFunc) g_object_unref, NULL);
	g_list_free (priv->loaded_backends);

	g_free (priv->geometry);
	g_free (priv->module_directory);

	/* Chain up to parent's finalize() method. */
	G_OBJECT_CLASS (e_shell_parent_class)->finalize (object);
}

static void
shell_constructed (GObject *object)
{
	GNetworkMonitor *monitor;

	/* The first EShell instance is the default. */
	if (default_shell == NULL) {
		default_shell = object;
		g_object_add_weak_pointer (object, &default_shell);
	}

	/* Synchronize network monitoring. */

	monitor = e_network_monitor_get_default ();

	e_binding_bind_property (
		monitor, "network-available",
		object, "network-available",
		G_BINDING_SYNC_CREATE);

	/* Chain up to parent's constructed() method. */
	G_OBJECT_CLASS (e_shell_parent_class)->constructed (object);

	g_signal_connect (
		object, "window-removed",
		G_CALLBACK (shell_window_removed_cb), NULL);
}

static void
shell_startup (GApplication *application)
{
	EShell *shell;

	g_return_if_fail (E_IS_SHELL (application));

	shell = E_SHELL (application);
	g_warn_if_fail (!shell->priv->requires_shutdown);

	shell->priv->requires_shutdown = TRUE;

	e_file_lock_create ();

	/* Destroy the lock file when the EShell is finalized
	 * to indicate a clean shut down to the next session. */
	g_object_weak_ref (
		G_OBJECT (application),
		(GWeakNotify) e_file_lock_destroy, NULL);

	/* Chain up to parent's startup() method. */
	G_APPLICATION_CLASS (e_shell_parent_class)->startup (application);

	if (e_util_is_running_gnome ())
		shell_create_app_menu (GTK_APPLICATION (application));
}

static void
shell_shutdown (GApplication *application)
{
	EShell *shell;

	g_return_if_fail (E_IS_SHELL (application));

	shell = E_SHELL (application);

	g_warn_if_fail (shell->priv->requires_shutdown);

	shell->priv->requires_shutdown = FALSE;

	/* Chain up to parent's method. */
	G_APPLICATION_CLASS (e_shell_parent_class)->shutdown (application);
}

static void
shell_activate (GApplication *application)
{
	GList *list;

	/* Do not chain up.  Default method just emits a warning. */

	list = gtk_application_get_windows (GTK_APPLICATION (application));

	/* Present the first EShellWindow, if found. */
	while (list != NULL) {
		GtkWindow *window = GTK_WINDOW (list->data);

		if (E_IS_SHELL_WINDOW (window)) {
			gtk_window_present (window);
			return;
		}

		list = g_list_next (list);
	}

	/* No EShellWindow found, so create one. */
	e_shell_create_shell_window (E_SHELL (application), NULL);
}

static void
shell_window_added (GtkApplication *application,
                    GtkWindow *window)
{
	gchar *role;

	/* Chain up to parent's window_added() method. */
	GTK_APPLICATION_CLASS (e_shell_parent_class)->
		window_added (application, window);

	g_signal_connect (
		window, "delete-event",
		G_CALLBACK (shell_window_delete_event_cb), application);

	/* We use the window's own type name and memory
	 * address to form a unique window role for X11. */
	role = g_strdup_printf (
		"%s-%" G_GINTPTR_FORMAT,
		G_OBJECT_TYPE_NAME (window),
		(gintptr) window);
	gtk_window_set_role (window, role);
	g_free (role);
}

static gboolean
shell_initable_init (GInitable *initable,
                     GCancellable *cancellable,
                     GError **error)
{
	GApplication *application = G_APPLICATION (initable);
	EShell *shell = E_SHELL (initable);
	ESourceRegistry *registry;
	ESource *proxy_source;
	gulong handler_id;

	shell_add_actions (application);

	if (!g_application_register (application, cancellable, error))
		return FALSE;

	registry = e_source_registry_new_sync (cancellable, error);
	if (registry == NULL)
		return FALSE;

	shell->priv->registry = g_object_ref (registry);
	shell->priv->credentials_prompter = e_credentials_prompter_new (registry);
	shell->priv->client_cache = e_client_cache_new (registry);

	shell->priv->credentials_required_handler_id = g_signal_connect (
		shell->priv->registry, "credentials-required",
		G_CALLBACK (shell_credentials_required_cb), shell);

	shell->priv->get_dialog_parent_handler_id = g_signal_connect (
		shell->priv->credentials_prompter, "get-dialog-parent",
		G_CALLBACK (shell_get_dialog_parent_cb), shell);

	handler_id = g_signal_connect (
		shell->priv->client_cache, "backend-died",
		G_CALLBACK (shell_backend_died_cb), shell);
	shell->priv->backend_died_handler_id = handler_id;

	handler_id = g_signal_connect (
		shell->priv->client_cache, "allow-auth-prompt",
		G_CALLBACK (shell_allow_auth_prompt_cb), shell);
	shell->priv->allow_auth_prompt_handler_id = handler_id;

	/* Configure WebKit's default SoupSession. */

	proxy_source = e_source_registry_ref_builtin_proxy (registry);
/* FIXME WK2
	g_object_set (
		webkit_get_default_session (),
		SOUP_SESSION_PROXY_RESOLVER,
		G_PROXY_RESOLVER (proxy_source),
		NULL);
*/
	g_object_unref (proxy_source);
	g_object_unref (registry);

#if GTK_CHECK_VERSION(3,11,5)
	/* Forbid header bars in stock GTK+ dialogs.
	 * They look very out of place in Evolution. */
	g_object_set (
		gtk_settings_get_default (),
		"gtk-dialogs-use-header", FALSE, NULL);
#endif

	return TRUE;
}

static void
e_shell_class_init (EShellClass *class)
{
	GObjectClass *object_class;
	GApplicationClass *application_class;
	GtkApplicationClass *gtk_application_class;

	g_type_class_add_private (class, sizeof (EShellPrivate));

	object_class = G_OBJECT_CLASS (class);
	object_class->set_property = shell_set_property;
	object_class->get_property = shell_get_property;
	object_class->dispose = shell_dispose;
	object_class->finalize = shell_finalize;
	object_class->constructed = shell_constructed;

	application_class = G_APPLICATION_CLASS (class);
	application_class->startup = shell_startup;
	application_class->shutdown = shell_shutdown;
	application_class->activate = shell_activate;

	gtk_application_class = GTK_APPLICATION_CLASS (class);
	gtk_application_class->window_added = shell_window_added;

	/**
	 * EShell:client-cache:
	 *
	 * Shared #EClient instances.
	 **/
	g_object_class_install_property (
		object_class,
		PROP_CLIENT_CACHE,
		g_param_spec_object (
			"client-cache",
			"Client Cache",
			"Shared EClient instances",
			E_TYPE_CLIENT_CACHE,
			G_PARAM_READABLE |
			G_PARAM_STATIC_STRINGS));

	/**
	 * EShell:express-mode
	 *
	 * Express mode alters Evolution's user interface to be mode
	 * usable on devices with small screens.
	 **/
	g_object_class_install_property (
		object_class,
		PROP_EXPRESS_MODE,
		g_param_spec_boolean (
			"express-mode",
			"Express Mode",
			"Whether express mode is enabled",
			FALSE,
			G_PARAM_READWRITE |
			G_PARAM_CONSTRUCT_ONLY |
			G_PARAM_STATIC_STRINGS));

	/**
	 * EShell:geometry
	 *
	 * User-specified initial window geometry string to apply
	 * to the first #EShellWindow created.
	 **/
	g_object_class_install_property (
		object_class,
		PROP_GEOMETRY,
		g_param_spec_string (
			"geometry",
			"Geometry",
			"Initial window geometry string",
			NULL,
			G_PARAM_WRITABLE |
			G_PARAM_CONSTRUCT_ONLY |
			G_PARAM_STATIC_STRINGS));

	/**
	 * EShell:module-directory
	 *
	 * The directory from which to load #EModule<!-- -->s.
	 **/
	g_object_class_install_property (
		object_class,
		PROP_MODULE_DIRECTORY,
		g_param_spec_string (
			"module-directory",
			"Module Directory",
			"The directory from which to load EModules",
			NULL,
			G_PARAM_READWRITE |
			G_PARAM_CONSTRUCT_ONLY |
			G_PARAM_STATIC_STRINGS));

	/**
	 * EShell:network-available
	 *
	 * Whether the network is available.
	 **/
	g_object_class_install_property (
		object_class,
		PROP_NETWORK_AVAILABLE,
		g_param_spec_boolean (
			"network-available",
			"Network Available",
			"Whether the network is available",
			TRUE,
			G_PARAM_READWRITE |
			G_PARAM_STATIC_STRINGS));

	/**
	 * EShell:online
	 *
	 * Whether the shell is online.
	 **/
	g_object_class_install_property (
		object_class,
		PROP_ONLINE,
		g_param_spec_boolean (
			"online",
			"Online",
			"Whether the shell is online",
			FALSE,
			G_PARAM_READWRITE |
			G_PARAM_CONSTRUCT |
			G_PARAM_STATIC_STRINGS));

	/**
	 * EShell:registry
	 *
	 * The #ESourceRegistry manages #ESource instances.
	 **/
	g_object_class_install_property (
		object_class,
		PROP_REGISTRY,
		g_param_spec_object (
			"registry",
			"Registry",
			"Data source registry",
			E_TYPE_SOURCE_REGISTRY,
			G_PARAM_READABLE |
			G_PARAM_STATIC_STRINGS));

	/**
	 * EShell:credentials-prompter
	 *
	 * The #ECredentialsPrompter managing #ESource credential requests.
	 *
	 * Since: 3.16
	 **/
	g_object_class_install_property (
		object_class,
		PROP_CREDENTIALS_PROMPTER,
		g_param_spec_object (
			"credentials-prompter",
			"Credentials Prompter",
			"Credentials Prompter",
			E_TYPE_CREDENTIALS_PROMPTER,
			G_PARAM_READABLE |
			G_PARAM_STATIC_STRINGS));

	/**
	 * EShell::event
	 * @shell: the #EShell which emitted the signal
	 * @event_data: data associated with the event
	 *
	 * This signal is used to broadcast custom events to the entire
	 * application.  The nature of @event_data depends on the event
	 * being broadcast.  The signal's detail denotes the event.
	 **/
	signals[EVENT] = g_signal_new (
		"event",
		G_OBJECT_CLASS_TYPE (object_class),
		G_SIGNAL_RUN_FIRST | G_SIGNAL_DETAILED | G_SIGNAL_ACTION,
		0, NULL, NULL,
		g_cclosure_marshal_VOID__POINTER,
		G_TYPE_NONE, 1,
		G_TYPE_POINTER);

	/**
	 * EShell::handle-uri
	 * @shell: the #EShell which emitted the signal
	 * @uri: the URI to be handled
	 *
	 * Emitted when @shell receives a URI to be handled, usually by
	 * way of a command-line argument.  An #EShellBackend should listen
	 * for this signal and try to handle the URI, usually by opening an
	 * editor window for the identified resource.
	 *
	 * Returns: %TRUE if the URI could be handled, %FALSE otherwise
	 **/
	signals[HANDLE_URI] = g_signal_new (
		"handle-uri",
		G_OBJECT_CLASS_TYPE (object_class),
		G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
		G_STRUCT_OFFSET (EShellClass, handle_uri),
		g_signal_accumulator_true_handled, NULL,
		e_marshal_BOOLEAN__STRING,
		G_TYPE_BOOLEAN, 1,
		G_TYPE_STRING);

	/**
	 * EShell::prepare-for-offline
	 * @shell: the #EShell which emitted the signal
	 * @activity: the #EActivity for offline preparations
	 *
	 * Emitted when the user elects to work offline.  An #EShellBackend
	 * should listen for this signal and make preparations for working
	 * in offline mode.
	 *
	 * If preparations for working offline cannot immediately be
	 * completed (such as when synchronizing with a remote server),
	 * the #EShellBackend should reference the @activity until
	 * preparations are complete, and then unreference the @activity.
	 * This will delay Evolution from actually going to offline mode
	 * until all backends have unreferenced @activity.
	 **/
	signals[PREPARE_FOR_OFFLINE] = g_signal_new (
		"prepare-for-offline",
		G_OBJECT_CLASS_TYPE (object_class),
		G_SIGNAL_RUN_FIRST,
		G_STRUCT_OFFSET (EShellClass, prepare_for_offline),
		NULL, NULL,
		g_cclosure_marshal_VOID__OBJECT,
		G_TYPE_NONE, 1,
		E_TYPE_ACTIVITY);

	/**
	 * EShell::prepare-for-online
	 * @shell: the #EShell which emitted the signal
	 * @activity: the #EActivity for offline preparations
	 *
	 * Emitted when the user elects to work online.  An #EShellBackend
	 * should listen for this signal and make preparations for working
	 * in online mode.
	 *
	 * If preparations for working online cannot immediately be
	 * completed (such as when re-connecting to a remote server), the
	 * #EShellBackend should reference the @activity until preparations
	 * are complete, and then unreference the @activity.  This will
	 * delay Evolution from actually going to online mode until all
	 * backends have unreferenced @activity.
	 **/
	signals[PREPARE_FOR_ONLINE] = g_signal_new (
		"prepare-for-online",
		G_OBJECT_CLASS_TYPE (object_class),
		G_SIGNAL_RUN_FIRST,
		G_STRUCT_OFFSET (EShellClass, prepare_for_online),
		NULL, NULL,
		g_cclosure_marshal_VOID__OBJECT,
		G_TYPE_NONE, 1,
		E_TYPE_ACTIVITY);

	/**
	 * EShell::prepare-for-quit
	 * @shell: the #EShell which emitted the signal
	 * @activity: the #EActivity for quit preparations
	 *
	 * Emitted when the user elects to quit the application, after
	 * #EShell::quit-requested.  An #EShellBackend should listen for
	 * this signal and make preparations for shutting down.
	 *
	 * If preparations for shutting down cannot immediately be completed
	 * (such as when there are uncompleted network operations), the
	 * #EShellBackend should reference the @activity until preparations
	 * are complete, and then unreference the @activity.  This will
	 * delay Evolution from actually shutting down until all backends
	 * have unreferenced @activity.
	 **/
	signals[PREPARE_FOR_QUIT] = g_signal_new (
		"prepare-for-quit",
		G_OBJECT_CLASS_TYPE (object_class),
		G_SIGNAL_RUN_FIRST,
		G_STRUCT_OFFSET (EShellClass, prepare_for_quit),
		NULL, NULL,
		g_cclosure_marshal_VOID__OBJECT,
		G_TYPE_NONE, 1,
		E_TYPE_ACTIVITY);

	/**
	 * EShell::quit-requested
	 * @shell: the #EShell which emitted the signal
	 * @reason: the reason for quitting
	 *
	 * Emitted when the user elects to quit the application, before
	 * #EShell::prepare-for-quit.
	 *
	 * #EShellBackend<!-- -->s and editor windows can listen for
	 * this signal to prompt the user to save changes or finish
	 * scheduled operations immediately (such as sending mail in
	 * Outbox).  If the user elects to cancel, the signal handler
	 * should call e_shell_cancel_quit() to abort the quit.
	 **/
	signals[QUIT_REQUESTED] = g_signal_new (
		"quit-requested",
		G_OBJECT_CLASS_TYPE (object_class),
		G_SIGNAL_RUN_FIRST,
		G_STRUCT_OFFSET (EShellClass, quit_requested),
		NULL, NULL,
		g_cclosure_marshal_VOID__ENUM,
		G_TYPE_NONE, 1,
		E_TYPE_SHELL_QUIT_REASON);
}

static void
e_shell_initable_init (GInitableIface *iface)
{
	iface->init = shell_initable_init;
}

static void
e_shell_init (EShell *shell)
{
	GHashTable *backends_by_name;
	GHashTable *backends_by_scheme;
	GtkIconTheme *icon_theme;

	shell->priv = E_SHELL_GET_PRIVATE (shell);

	backends_by_name = g_hash_table_new (g_str_hash, g_str_equal);
	backends_by_scheme = g_hash_table_new (g_str_hash, g_str_equal);

	g_queue_init (&shell->priv->alerts);

	shell->priv->cancellable = g_cancellable_new ();
	shell->priv->preferences_window = e_preferences_window_new (shell);
	shell->priv->backends_by_name = backends_by_name;
	shell->priv->backends_by_scheme = backends_by_scheme;
	shell->priv->safe_mode = e_file_lock_exists ();
	shell->priv->requires_shutdown = FALSE;

	/* Add our icon directory to the theme's search path
	 * here instead of in main() so Anjal picks it up. */
	icon_theme = gtk_icon_theme_get_default ();
	gtk_icon_theme_append_search_path (icon_theme, EVOLUTION_ICONDIR);

	e_signal_connect_notify (
		shell, "notify::online",
		G_CALLBACK (shell_notify_online_cb), NULL);

	g_signal_connect_swapped (
		G_APPLICATION (shell), "shutdown",
		G_CALLBACK (shell_sm_quit_cb), shell);
}

/**
 * e_shell_get_default:
 *
 * Returns the #EShell created by <function>main()</function>.
 *
 * Try to obtain the #EShell from elsewhere if you can.  This function
 * is intended as a temporary workaround for when that proves difficult.
 *
 * Returns: the #EShell singleton
 **/
EShell *
e_shell_get_default (void)
{
	return default_shell;
}

/**
 * e_shell_load_modules:
 * @shell: an #EShell
 *
 * Loads all installed modules and performs some internal bookkeeping.
 * This function should be called after creating the #EShell instance
 * but before initiating migration or starting the main loop.
 **/
void
e_shell_load_modules (EShell *shell)
{
	EClientCache *client_cache;
	const gchar *module_directory;
	GList *list;

	g_return_if_fail (E_IS_SHELL (shell));

	if (shell->priv->modules_loaded)
		return;

	/* Load all shared library modules. */

	module_directory = e_shell_get_module_directory (shell);
	g_return_if_fail (module_directory != NULL);

	list = e_module_load_all_in_directory (module_directory);
	g_list_foreach (list, (GFunc) g_type_module_unuse, NULL);
	g_list_free (list);

	/* Process shell backends. */

	list = g_list_sort (
		e_extensible_list_extensions (
		E_EXTENSIBLE (shell), E_TYPE_SHELL_BACKEND),
		(GCompareFunc) e_shell_backend_compare);
	g_list_foreach (list, (GFunc) shell_process_backend, shell);
	shell->priv->loaded_backends = list;

	/* XXX The client cache needs extra help loading its extensions,
	 *     since it gets instantiated before any modules are loaded. */
	client_cache = e_shell_get_client_cache (shell);
	e_extensible_load_extensions (E_EXTENSIBLE (client_cache));

	shell->priv->modules_loaded = TRUE;
}

/**
 * e_shell_get_shell_backends:
 * @shell: an #EShell
 *
 * Returns a list of loaded #EShellBackend instances.  The list is
 * owned by @shell and should not be modified or freed.
 *
 * Returns: a list of loaded #EShellBackend instances
 **/
GList *
e_shell_get_shell_backends (EShell *shell)
{
	g_return_val_if_fail (E_IS_SHELL (shell), NULL);

	return shell->priv->loaded_backends;
}

/**
 * e_shell_get_canonical_name:
 * @shell: an #EShell
 * @name: the name or alias of an #EShellBackend
 *
 * Returns the canonical name for the #EShellBackend whose name or alias
 * is @name.
 *
 * Returns: the canonical #EShellBackend name
 **/
const gchar *
e_shell_get_canonical_name (EShell *shell,
                            const gchar *name)
{
	EShellBackend *shell_backend;

	g_return_val_if_fail (E_IS_SHELL (shell), NULL);

	/* Handle NULL or empty name arguments silently. */
	if (name == NULL || *name == '\0')
		return NULL;

	shell_backend = e_shell_get_backend_by_name (shell, name);

	if (shell_backend == NULL)
		return NULL;

	return E_SHELL_BACKEND_GET_CLASS (shell_backend)->name;
}

/**
 * e_shell_get_backend_by_name:
 * @shell: an #EShell
 * @name: the name or alias of an #EShellBackend
 *
 * Returns the corresponding #EShellBackend for the given name or alias,
 * or %NULL if @name is not recognized.
 *
 * Returns: the #EShellBackend named @name, or %NULL
 **/
EShellBackend *
e_shell_get_backend_by_name (EShell *shell,
                             const gchar *name)
{
	GHashTable *hash_table;

	g_return_val_if_fail (E_IS_SHELL (shell), NULL);
	g_return_val_if_fail (name != NULL, NULL);

	hash_table = shell->priv->backends_by_name;

	return g_hash_table_lookup (hash_table, name);
}

/**
 * e_shell_get_backend_by_scheme:
 * @shell: an #EShell
 * @scheme: a URI scheme
 *
 * Returns the #EShellBackend that implements the given URI scheme,
 * or %NULL if @scheme is not recognized.
 *
 * Returns: the #EShellBackend that implements @scheme, or %NULL
 **/
EShellBackend *
e_shell_get_backend_by_scheme (EShell *shell,
                               const gchar *scheme)
{
	GHashTable *hash_table;

	g_return_val_if_fail (E_IS_SHELL (shell), NULL);
	g_return_val_if_fail (scheme != NULL, NULL);

	hash_table = shell->priv->backends_by_scheme;

	return g_hash_table_lookup (hash_table, scheme);
}

/**
 * e_shell_get_client_cache:
 * @shell: an #EShell
 *
 * Returns the #EClientCache instance for @shell.
 *
 * Returns: the #EClientCache instance for @shell
 **/
EClientCache *
e_shell_get_client_cache (EShell *shell)
{
	g_return_val_if_fail (E_IS_SHELL (shell), NULL);

	return shell->priv->client_cache;
}

/**
 * e_shell_get_registry:
 * @shell: an #EShell
 *
 * Returns the shell's #ESourceRegistry which holds all #ESource instances.
 *
 * Returns: the #ESourceRegistry
 **/
ESourceRegistry *
e_shell_get_registry (EShell *shell)
{
	g_return_val_if_fail (E_IS_SHELL (shell), NULL);

	return shell->priv->registry;
}

/**
 * e_shell_get_credentials_prompter:
 * @shell: an #EShell
 *
 * Returns the shell's #ECredentialsPrompter which responds
 * to #ESource instances credential requests.
 *
 * Returns: the #ECredentialsPrompter
 *
 * Since: 3.16
 **/
ECredentialsPrompter *
e_shell_get_credentials_prompter (EShell *shell)
{
	g_return_val_if_fail (E_IS_SHELL (shell), NULL);

	return shell->priv->credentials_prompter;
}

/**
 * e_shell_allow_auth_prompt_for:
 * @shell: an #EShell
 * @source: an #ESource
 *
 * Allows direct credentials prompt for @source. That means,
 * when the @source will emit 'credentials-required' signal,
 * then a user will be asked accordingly. When the auth prompt
 * is disabled, aonly an #EAlert is shown.
 *
 * Since: 3.16
 **/
void
e_shell_allow_auth_prompt_for (EShell *shell,
			       ESource *source)
{
	g_return_if_fail (E_IS_SHELL (shell));
	g_return_if_fail (E_IS_SOURCE (source));

	e_credentials_prompter_set_auto_prompt_disabled_for (shell->priv->credentials_prompter, source, FALSE);

	if (e_source_get_connection_status (source) == E_SOURCE_CONNECTION_STATUS_AWAITING_CREDENTIALS) {
		e_credentials_prompter_process_source (shell->priv->credentials_prompter, source);
	} else if (e_source_get_connection_status (source) == E_SOURCE_CONNECTION_STATUS_SSL_FAILED) {
		e_source_get_last_credentials_required_arguments (source, shell->priv->cancellable,
			shell_get_last_credentials_required_arguments_cb, shell);
	}
}

/**
 * e_shell_create_shell_window:
 * @shell: an #EShell
 * @view_name: name of the initial shell view, or %NULL
 *
 * Creates a new #EShellWindow.  Use this function instead of
 * e_shell_window_new() so that @shell can properly configure
 * the window.
 *
 * Returns: a new #EShellWindow
 **/
GtkWidget *
e_shell_create_shell_window (EShell *shell,
                             const gchar *view_name)
{
	GtkWidget *shell_window;
	GList *link;
	gboolean can_change_default_view;

	g_return_val_if_fail (E_IS_SHELL (shell), NULL);

	if (g_application_get_is_remote (G_APPLICATION (shell)))
		goto remote;

	can_change_default_view = !view_name || *view_name != '*';
	view_name = e_shell_get_canonical_name (shell, can_change_default_view ? view_name : (view_name + 1));

	/* EShellWindow initializes its active view from a GSetting key,
	 * so set the key ahead of time to control the initial view. */
	if (view_name && can_change_default_view) {
		GSettings *settings;

		settings = e_util_ref_settings ("org.gnome.evolution.shell");
		g_settings_set_string (
			settings, "default-component-id", view_name);
		g_object_unref (settings);
	}

	shell_window = e_shell_window_new (
		shell,
		shell->priv->safe_mode,
		shell->priv->geometry);

	if (view_name && !can_change_default_view) {
		GSettings *settings;
		gchar *active_view;

		settings = e_util_ref_settings ("org.gnome.evolution.shell");

		/* This is ugly, but nothing better with GSettings bindings, I'm afraid. */
		active_view = g_settings_get_string (settings, "default-component-id");

		e_shell_window_set_active_view (E_SHELL_WINDOW (shell_window), view_name);

		g_settings_set_string (settings, "default-component-id", active_view);

		g_object_unref (settings);
		g_free (active_view);
	}

	/* Submit any outstanding alerts. */
	link = g_queue_peek_head_link (&shell->priv->alerts);
	while (link != NULL) {
		e_alert_sink_submit_alert (
			E_ALERT_SINK (shell_window),
			E_ALERT (link->data));
		link = g_list_next (link);
	}

	/* Clear the first-time-only options. */
	shell->priv->safe_mode = FALSE;
	g_free (shell->priv->geometry);
	shell->priv->geometry = NULL;

	gtk_widget_show (shell_window);

	if (g_list_length (gtk_application_get_windows (GTK_APPLICATION (shell))) == 1) {
		/* It's the first window, process outstanding credential requests now */
		e_credentials_prompter_process_awaiting_credentials (shell->priv->credentials_prompter);

		/* Also check alerts for failed authentications */
		shell_process_failed_authentications (shell);
	}

	return shell_window;

remote:  /* Send a message to the other Evolution process. */

	if (view_name != NULL) {
		g_action_group_activate_action (
			G_ACTION_GROUP (shell), "create-from-remote",
			g_variant_new_string (view_name));
	} else
		g_application_activate (G_APPLICATION (shell));

	return NULL;
}

/**
 * e_shell_handle_uris:
 * @shell: an #EShell
 * @uris: %NULL-terminated list of URIs
 * @do_import: request an import of the URIs
 *
 * Emits the #EShell::handle-uri signal for each URI.
 *
 * Returns: the number of URIs successfully handled
 **/
guint
e_shell_handle_uris (EShell *shell,
                     const gchar * const *uris,
                     gboolean do_import)
{
	GPtrArray *args;
	gchar *cwd;
	guint n_handled = 0;
	guint ii;

	g_return_val_if_fail (E_IS_SHELL (shell), FALSE);
	g_return_val_if_fail (uris != NULL, FALSE);

	if (g_application_get_is_remote (G_APPLICATION (shell)))
		goto remote;

	if (do_import) {
		n_handled = e_shell_utils_import_uris (shell, uris);
	} else {
		for (ii = 0; uris[ii] != NULL; ii++) {
			gboolean handled;

			g_signal_emit (
				shell, signals[HANDLE_URI],
				0, uris[ii], &handled);
			n_handled += handled ? 1 : 0;
		}

		if (n_handled == 0)
			n_handled = e_shell_utils_import_uris (shell, uris);
	}

	return n_handled;

remote:  /* Send a message to the other Evolution process. */

	cwd = g_get_current_dir ();
	args = g_ptr_array_sized_new (g_strv_length ((gchar **) uris) + 2);

	g_ptr_array_add (args, (gchar *) "--use-cwd");
	g_ptr_array_add (args, cwd);

	for (ii = 0; uris[ii]; ii++) {
		g_ptr_array_add (args, (gchar *) uris[ii]);
	}

	g_action_group_activate_action (
		G_ACTION_GROUP (shell), "handle-uris",
		g_variant_new_strv ((const gchar * const *) args->pdata, args->len));

	g_ptr_array_free (args, TRUE);
	g_free (cwd);

	/* As far as we're concerned, all URIs have been handled. */

	return g_strv_length ((gchar **) uris);
}

/**
 * e_shell_submit_alert:
 * @shell: an #EShell
 * @alert: an #EAlert
 *
 * Broadcasts @alert to all #EShellWindow<!-- -->s.  This should only
 * be used for application-wide alerts such as a network outage.  Submit
 * view-specific alerts to the appropriate #EShellContent instance.
 **/
void
e_shell_submit_alert (EShell *shell,
                      EAlert *alert)
{
	GtkApplication *application;
	GList *list, *iter;

	g_return_if_fail (E_IS_SHELL (shell));
	g_return_if_fail (E_IS_ALERT (alert));

	application = GTK_APPLICATION (shell);

	g_queue_push_tail (&shell->priv->alerts, g_object_ref (alert));

	g_signal_connect_swapped (
		alert, "response",
		G_CALLBACK (shell_alert_response_cb), shell);

	list = gtk_application_get_windows (application);

	/* Submit the alert to all available EShellWindows. */
	for (iter = list; iter != NULL; iter = g_list_next (iter))
		if (E_IS_SHELL_WINDOW (iter->data))
			e_alert_sink_submit_alert (
				E_ALERT_SINK (iter->data), alert);
}

/**
 * e_shell_get_active_window:
 * @shell: an #EShell or %NULL to use the default shell
 *
 * Returns the most recently focused watched window, according to
 * gtk_application_get_windows().  Convenient for finding a parent
 * for a transient window.
 *
 * Note the returned window is not necessarily an #EShellWindow.
 *
 * Returns: the most recently focused watched window
 **/
GtkWindow *
e_shell_get_active_window (EShell *shell)
{
	GtkApplication *application;
	GList *list;

	if (shell == NULL)
		shell = e_shell_get_default ();

	g_return_val_if_fail (E_IS_SHELL (shell), NULL);

	application = GTK_APPLICATION (shell);
	list = gtk_application_get_windows (application);

	if (list == NULL)
		return NULL;

	/* Sanity check */
	g_return_val_if_fail (GTK_IS_WINDOW (list->data), NULL);

	return GTK_WINDOW (list->data);
}

/**
 * e_shell_get_express_mode:
 * @shell: an #EShell
 *
 * Returns %TRUE if Evolution is in express mode.
 *
 * Returns: %TRUE if Evolution is in express mode
 **/
gboolean
e_shell_get_express_mode (EShell *shell)
{
	g_return_val_if_fail (E_IS_SHELL (shell), FALSE);

	return shell->priv->express_mode;
}

/**
 * e_shell_get_module_directory:
 * @shell: an #EShell
 *
 * Returns the directory from which #EModule<!-- -->s were loaded.
 *
 * Returns: the #EModule directory
 **/
const gchar *
e_shell_get_module_directory (EShell *shell)
{
	g_return_val_if_fail (E_IS_SHELL (shell), NULL);

	return shell->priv->module_directory;
}

/**
 * e_shell_get_network_available:
 * @shell: an #EShell
 *
 * Returns %TRUE if a network is available.
 *
 * Returns: %TRUE if a network is available
 **/
gboolean
e_shell_get_network_available (EShell *shell)
{
	g_return_val_if_fail (E_IS_SHELL (shell), FALSE);

	return shell->priv->network_available;
}

/**
 * e_shell_set_network_available:
 * @shell: an #EShell
 * @network_available: whether a network is available
 *
 * Sets whether a network is available.  This is usually called in
 * response to a status change signal from NetworkManager.  If the
 * network becomes unavailable while #EShell:online is %TRUE, the
 * @shell will force #EShell:online to %FALSE until the network
 * becomes available again.
 **/
void
e_shell_set_network_available (EShell *shell,
                               gboolean network_available)
{
	g_return_if_fail (E_IS_SHELL (shell));

	if (shell->priv->network_available_locked)
		return;

	/* Network availablity is in an indeterminate state until
	 * the first time this function is called.  Don't let our
	 * arbitrary default value block this from being handled. */
	if (!shell->priv->network_available_set)
		shell->priv->network_available_set = TRUE;
	else if (shell->priv->network_available == network_available)
		return;

	shell->priv->network_available = network_available;
	g_object_notify (G_OBJECT (shell), "network-available");

	/* If we're being forced offline, perhaps due to a network outage,
	 * reconnect automatically when the network becomes available. */
	if (!network_available && (shell->priv->online || shell->priv->preparing_for_line_change)) {
		g_message ("Network disconnected.  Forced offline.");

		if (shell->priv->set_online_timeout_id > 0) {
			g_source_remove (shell->priv->set_online_timeout_id);
			shell->priv->set_online_timeout_id = 0;
		}

		e_shell_set_online (shell, FALSE);
		shell->priv->auto_reconnect = TRUE;
	} else if (network_available && shell->priv->auto_reconnect) {
		g_message ("Connection established.  Going online.");

		/* Wait some seconds to give the network enough time to become
		 * fully available. */
		if (shell->priv->set_online_timeout_id > 0) {
			g_source_remove (shell->priv->set_online_timeout_id);
			shell->priv->set_online_timeout_id = 0;
		}

		shell->priv->set_online_timeout_id = e_named_timeout_add_seconds_full (
			G_PRIORITY_DEFAULT, SET_ONLINE_TIMEOUT_SECONDS, e_shell_set_online_cb,
			g_object_ref (shell), g_object_unref);

		shell->priv->auto_reconnect = FALSE;
	}
}

/**
 * e_shell_lock_network_available:
 * @shell: an #EShell
 *
 * Locks the value of #EShell:network-available to %TRUE.  Further
 * attempts to set the property will be ignored.
 *
 * This is used for the --force-online command-line option, which is
 * intended to override the network availability status as reported
 * by NetworkManager or other network monitoring software.
 **/
void
e_shell_lock_network_available (EShell *shell)
{
	g_return_if_fail (E_IS_SHELL (shell));

	e_shell_set_network_available (shell, TRUE);
	shell->priv->network_available_locked = TRUE;

	/* As this is a user choice to go online, do not wait and switch online immediately */
	if (shell->priv->set_online_timeout_id > 0) {
		g_source_remove (shell->priv->set_online_timeout_id);
		shell->priv->set_online_timeout_id = 0;

		e_shell_set_online (shell, TRUE);
	}
}

/**
 * e_shell_get_online:
 * @shell: an #EShell
 *
 * Returns %TRUE if Evolution is online, %FALSE if Evolution is offline.
 * Evolution may be offline because the user elected to work offline, or
 * because the network has become unavailable.
 *
 * Returns: %TRUE if Evolution is online
 **/
gboolean
e_shell_get_online (EShell *shell)
{
	g_return_val_if_fail (E_IS_SHELL (shell), FALSE);

	return shell->priv->online;
}

/**
 * e_shell_set_online:
 * @shell: an #EShell
 * @online: %TRUE to go online, %FALSE to go offline
 *
 * Asynchronously places Evolution in online or offline mode.
 **/
void
e_shell_set_online (EShell *shell,
                    gboolean online)
{
	g_return_if_fail (E_IS_SHELL (shell));

	if (online == shell->priv->online && !shell->priv->preparing_for_line_change)
		return;

	if (online)
		shell_prepare_for_online (shell);
	else
		shell_prepare_for_offline (shell);
}

/**
 * e_shell_get_preferences_window:
 * @shell: an #EShell
 *
 * Returns the Evolution Preferences window.
 *
 * Returns: the preferences window
 **/
GtkWidget *
e_shell_get_preferences_window (EShell *shell)
{
	g_return_val_if_fail (E_IS_SHELL (shell), NULL);

	return shell->priv->preferences_window;
}

/**
 * e_shell_event:
 * @shell: an #EShell
 * @event_name: the name of the event
 * @event_data: data associated with the event
 *
 * The #EShell::event signal acts as a cheap mechanism for broadcasting
 * events to the rest of the application, such as new mail arriving.  The
 * @event_name is used as the signal detail, and @event_data may point to
 * an object or data structure associated with the event.
 **/
void
e_shell_event (EShell *shell,
               const gchar *event_name,
               gpointer event_data)
{
	GQuark detail;

	g_return_if_fail (E_IS_SHELL (shell));
	g_return_if_fail (event_name != NULL);

	detail = g_quark_from_string (event_name);
	g_signal_emit (shell, signals[EVENT], detail, event_data);
}

/**
 * e_shell_quit:
 * @shell: an #EShell
 * @reason: the reason for quitting
 *
 * Requests an application shutdown.  This happens in two phases: the
 * first is synchronous, the second is asynchronous.
 *
 * In the first phase, the @shell emits a #EShell::quit-requested signal
 * to potentially give the user a chance to cancel shutdown.  If the user
 * cancels shutdown, the function returns %FALSE.  Otherwise it proceeds
 * into the second phase.
 *
 * In the second phase, the @shell emits a #EShell::prepare-for-quit
 * signal and immediately returns %TRUE.  Signal handlers may delay the
 * actual application shutdown while they clean up resources, but there
 * is no way to cancel shutdown at this point.
 *
 * Consult the documentation for these two signals for details on how
 * to handle them.
 *
 * Returns: %TRUE if shutdown is underway, %FALSE if it was cancelled
 **/
gboolean
e_shell_quit (EShell *shell,
              EShellQuitReason reason)
{
	g_return_val_if_fail (E_IS_SHELL (shell), FALSE);

	if (g_application_get_is_remote (G_APPLICATION (shell)))
		goto remote;

	/* Last Window reason can be used multiple times;
	   this is to ask for a forced quit before the timeout is reached. */
	if (reason == E_SHELL_QUIT_LAST_WINDOW && shell->priv->preparing_for_quit != NULL) {
		shell_prepare_for_quit (shell);
		return TRUE;
	}

	if (!shell_request_quit (shell, reason))
		return FALSE;

	shell_prepare_for_quit (shell);

	return TRUE;

remote:  /* Send a message to the other Evolution process. */

	g_action_group_activate_action (
		G_ACTION_GROUP (shell), "quit", NULL);

	return TRUE;
}

/**
 * e_shell_cancel_quit:
 * @shell: an #EShell
 *
 * This function may only be called from #EShell::quit-requested signal
 * handlers to prevent Evolution from quitting.  Calling this will stop
 * further emission of the #EShell::quit-requested signal.
 *
 * Note: This function has no effect during a #EShell::prepare-for-quit
 * signal emission.
 **/
void
e_shell_cancel_quit (EShell *shell)
{
	g_return_if_fail (E_IS_SHELL (shell));

	shell->priv->quit_cancelled = TRUE;

	g_signal_stop_emission (shell, signals[QUIT_REQUESTED], 0);
}

gboolean
e_shell_requires_shutdown (EShell *shell)
{
	g_return_val_if_fail (E_IS_SHELL (shell), FALSE);

	return shell->priv->requires_shutdown;
}
