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
#include "e-shell-migrate.h"
#include "e-shell-window.h"
#include "e-shell-utils.h"

#define SET_ONLINE_TIMEOUT_SECONDS 5

struct _EShellPrivate {
	GQueue alerts;
	ESourceRegistry *registry;
	ECredentialsPrompter *credentials_prompter;
	EClientCache *client_cache;
	GtkWidget *preferences_window;
	GCancellable *cancellable;
	EColorSchemeWatcher *color_scheme_watcher;

	/* Shell Backends */
	GList *loaded_backends;              /* not referenced */
	GHashTable *backends_by_name;
	GHashTable *backends_by_scheme;

	GHashTable *auth_prompt_parents;     /* gchar *ESource::uid ~> gpointer ( only remembered, not referenced, GtkWindow * )*/

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
	gulong get_dialog_parent_full_handler_id;
	gulong credentials_required_handler_id;

	guint started : 1;
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
	guint activate_created_window : 1;
};

enum {
	PROP_0,
	PROP_CLIENT_CACHE,
	PROP_EXPRESS_MODE,
	PROP_MODULE_DIRECTORY,
	PROP_NETWORK_AVAILABLE,
	PROP_ONLINE,
	PROP_REGISTRY,
	PROP_CREDENTIALS_PROMPTER
};

enum {
	EVENT,
	HANDLE_URI,
	VIEW_URI,
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

G_DEFINE_TYPE_WITH_CODE (EShell, e_shell, GTK_TYPE_APPLICATION,
	G_ADD_PRIVATE (EShell)
	G_IMPLEMENT_INTERFACE (G_TYPE_INITABLE, e_shell_initable_init)
	G_IMPLEMENT_INTERFACE (E_TYPE_EXTENSIBLE, NULL))

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
	gboolean activate_created_window = shell->priv->activate_created_window;
	const gchar *view_name;

	shell->priv->activate_created_window = FALSE;

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
				if (activate_created_window || g_strcmp0 (active_view, view_name) == 0) {
					if (!get_current) {
						view_name = e_shell_get_canonical_name (shell, view_name);
						if (view_name)
							e_shell_window_set_active_view (E_SHELL_WINDOW (window), view_name);
					}

					gtk_window_present (window);
					return;
				} else if (get_current && active_view) {
					if (activate_created_window)
						return;

					view_name = active_view;
					break;
				}
			}

			list = g_list_next (list);
		}
	} else {
		GtkWindow *window;

		window = e_shell_get_active_window (shell);

		if (E_IS_SHELL_WINDOW (window)) {
			if (activate_created_window)
				return;
			view_name = e_shell_window_get_active_view (E_SHELL_WINDOW (window));
		}
	}

	if (!activate_created_window) {
		/* No suitable EShellWindow found, so create one. */
		e_shell_create_shell_window (shell, view_name);
	}
}

static void
shell_action_handle_uris_cb (GSimpleAction *action,
                             GVariant *parameter,
                             EShell *shell)
{
	const gchar **uris;
	gchar *change_dir = NULL;
	gboolean do_import = FALSE, do_view = FALSE;
	gint ii, skip_args = 0, did_read_args = -1;

	/* Do not use g_strfreev() here. */
	uris = g_variant_get_strv (parameter, NULL);

	/* the arguments can come in any order, they only should be at the beginning, before the URI-s */
	while (did_read_args != skip_args) {
		did_read_args = skip_args;

		if (uris && g_strcmp0 (uris[skip_args], "--use-cwd") == 0 && uris[skip_args + 1] && *uris[skip_args + 1]) {
			change_dir = g_get_current_dir ();

			if (g_chdir (uris[skip_args + 1]) != 0)
				g_warning ("%s: Failed to change directory to '%s': %s", G_STRFUNC, uris[skip_args + 1], g_strerror (errno));

			skip_args += 2;
		}

		if (uris && g_strcmp0 (uris[skip_args], "--import") == 0) {
			do_import = TRUE;
			skip_args++;
		}

		if (uris && g_strcmp0 (uris[skip_args], "--view") == 0) {
			do_view = TRUE;
			skip_args++;
		}
	}

	if (skip_args > 0) {
		for (ii = 0; uris[ii + skip_args]; ii++) {
			uris[ii] = uris[ii + skip_args];
		}

		uris[ii] = NULL;
	}

	e_shell_handle_uris (shell, uris, do_import, do_view);
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
		_("Preparing to go offline…"));

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
		_("Preparing to go online…"));

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
		_("Preparing to quit…"));

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

static gboolean
close_alert_idle_cb (gpointer user_data)
{
	GWeakRef *weak_ref = user_data;
	EAlert *alert;

	alert = g_weak_ref_get (weak_ref);
	if (alert) {
		e_alert_response (alert, GTK_RESPONSE_CLOSE);
		g_object_unref (alert);
	}

	return FALSE;
}

static void
shell_source_connection_status_notify_cb (ESource *source,
					  GParamSpec *param,
					  EAlert *alert)
{
	g_return_if_fail (E_IS_ALERT (alert));

	if (e_source_get_connection_status (source) == E_SOURCE_CONNECTION_STATUS_DISCONNECTED ||
	    e_source_get_connection_status (source) == E_SOURCE_CONNECTION_STATUS_CONNECTING ||
	    e_source_get_connection_status (source) == E_SOURCE_CONNECTION_STATUS_CONNECTED) {
		/* These notifications are received in the Source Registry thread,
		   thus schedule it for the main/UI thread. */
		g_idle_add_full (G_PRIORITY_DEFAULT_IDLE, close_alert_idle_cb,
			e_weak_ref_new (alert), (GDestroyNotify) e_weak_ref_free);
	}
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

static void
shell_wrote_ssl_trust_cb (GObject *source_object,
			  GAsyncResult *result,
			  gpointer user_data)
{
	ESource *source;
	GError *error = NULL;

	g_return_if_fail (E_IS_SOURCE (source_object));

	source = E_SOURCE (source_object);

	if (!e_source_write_finish (source, result, &error) &&
	    !g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED)) {
		g_warning ("%s: Failed to save changes to source '%s' (%s): %s", G_STRFUNC,
			e_source_get_display_name (source),
			e_source_get_uid (source),
			error ? error->message : "Unknown error");
	}

	g_clear_error (&error);
}

static gchar *
shell_extract_ssl_trust (ESource *source)
{
	gchar *ssl_trust = NULL;

	g_return_val_if_fail (E_IS_SOURCE (source), NULL);

	if (e_source_has_extension (source, E_SOURCE_EXTENSION_WEBDAV_BACKEND)) {
		ESourceWebdav *webdav_extension;

		webdav_extension = e_source_get_extension (source, E_SOURCE_EXTENSION_WEBDAV_BACKEND);
		ssl_trust = e_source_webdav_dup_ssl_trust (webdav_extension);
	}

	return ssl_trust;
}

static gboolean
shell_maybe_propagate_ssl_trust (EShell *shell,
				 ESource *source,
				 const gchar *original_ssl_trust)
{
	gchar *new_ssl_trust;
	gboolean changed;

	g_return_val_if_fail (E_IS_SHELL (shell), FALSE);
	g_return_val_if_fail (E_IS_SOURCE (source), FALSE);

	new_ssl_trust = shell_extract_ssl_trust (source);
	changed = g_strcmp0 (original_ssl_trust, new_ssl_trust) != 0;

	if (changed && new_ssl_trust && *new_ssl_trust) {
		g_object_ref (source);

		while (source && !e_source_has_extension (source, E_SOURCE_EXTENSION_COLLECTION)) {
			ESource *parent = NULL;

			if (e_source_get_parent (source))
				parent = e_source_registry_ref_source (shell->priv->registry, e_source_get_parent (source));

			g_clear_object (&source);

			source = parent;
		}

		if (source) {
			const gchar *uid;
			GList *sources, *link;
			gchar *ssl_trust;

			if (e_source_has_extension (source, E_SOURCE_EXTENSION_WEBDAV_BACKEND)) {
				ssl_trust = shell_extract_ssl_trust (source);

				if (g_strcmp0 (ssl_trust, original_ssl_trust) == 0) {
					ESourceWebdav *webdav_extension;

					webdav_extension = e_source_get_extension (source, E_SOURCE_EXTENSION_WEBDAV_BACKEND);
					e_source_webdav_set_ssl_trust (webdav_extension, new_ssl_trust);

					e_source_write (source, shell->priv->cancellable, shell_wrote_ssl_trust_cb, NULL);
				}

				g_free (ssl_trust);
			}

			uid = e_source_get_uid (source);

			sources = e_source_registry_list_sources (shell->priv->registry, NULL);

			for (link = sources; link; link = g_list_next (link)) {
				ESource *child = link->data;

				if (g_strcmp0 (uid, e_source_get_parent (child)) == 0 &&
				    e_source_has_extension (child, E_SOURCE_EXTENSION_WEBDAV_BACKEND)) {
					ssl_trust = shell_extract_ssl_trust (child);

					if (g_strcmp0 (ssl_trust, original_ssl_trust) == 0) {
						ESourceWebdav *webdav_extension;

						webdav_extension = e_source_get_extension (child, E_SOURCE_EXTENSION_WEBDAV_BACKEND);
						e_source_webdav_set_ssl_trust (webdav_extension, new_ssl_trust);

						e_source_write (child, shell->priv->cancellable, shell_wrote_ssl_trust_cb, NULL);
					}

					g_free (ssl_trust);
				}
			}

			g_list_free_full (sources, g_object_unref);
		}

		g_clear_object (&source);
	}

	g_free (new_ssl_trust);

	return changed;
}

static ETrustPromptResponse
shell_get_source_last_trust_response (ESource *source)
{
	g_return_val_if_fail (E_IS_SOURCE (source), E_TRUST_PROMPT_RESPONSE_UNKNOWN);

	if (!e_source_has_extension (source, E_SOURCE_EXTENSION_WEBDAV_BACKEND))
		return E_TRUST_PROMPT_RESPONSE_UNKNOWN;

	return e_source_webdav_get_ssl_trust_response (e_source_get_extension (source, E_SOURCE_EXTENSION_WEBDAV_BACKEND));
}

#define SOURCE_ALERT_KEY_SOURCE			"source-alert-key-source"
#define SOURCE_ALERT_KEY_CERTIFICATE_PEM	"source-alert-key-certificate-pem"
#define SOURCE_ALERT_KEY_CERTIFICATE_ERRORS	"source-alert-key-certificate-errors"
#define SOURCE_ALERT_KEY_ERROR_TEXT		"source-alert-key-error-text"

typedef struct _TrustPromptData {
	EShell *shell; /* not referenced */
	gchar *original_ssl_trust;
} TrustPromptData;

static void
trust_prompt_data_free (gpointer ptr)
{
	TrustPromptData *tpd = ptr;

	if (tpd) {
		g_free (tpd->original_ssl_trust);
		g_slice_free (TrustPromptData, tpd);
	}
}

static void
shell_trust_prompt_done_cb (GObject *source_object,
			    GAsyncResult *result,
			    gpointer user_data)
{
	ESource *source;
	ETrustPromptResponse response = E_TRUST_PROMPT_RESPONSE_UNKNOWN;
	TrustPromptData *tpd = user_data;
	GError *error = NULL;

	g_return_if_fail (E_IS_SOURCE (source_object));
	g_return_if_fail (tpd != NULL);

	source = E_SOURCE (source_object);

	if (!e_trust_prompt_run_for_source_finish (source, result, &response, &error)) {
		/* Can be cancelled only if the shell is disposing/disposed */
		if (error && !g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED)) {
			EAlert *alert;
			gchar *display_name;

			g_return_if_fail (E_IS_SHELL (tpd->shell));

			display_name = e_util_get_source_full_name (tpd->shell->priv->registry, source);
			alert = e_alert_new ("shell:source-trust-prompt-failed",
				display_name,
				error->message,
				NULL);
			e_shell_submit_alert (tpd->shell, alert);
			g_object_unref (alert);
			g_free (display_name);
		}

		g_clear_error (&error);
		trust_prompt_data_free (tpd);
		return;
	}

	g_return_if_fail (E_IS_SHELL (tpd->shell));

	if (response == E_TRUST_PROMPT_RESPONSE_UNKNOWN) {
		e_credentials_prompter_set_auto_prompt_disabled_for (tpd->shell->priv->credentials_prompter, source, TRUE);
		trust_prompt_data_free (tpd);
		return;
	}

	/* If a credentials prompt is required, then it'll be shown immediately. */
	e_credentials_prompter_set_auto_prompt_disabled_for (tpd->shell->priv->credentials_prompter, source, FALSE);

	if (shell_maybe_propagate_ssl_trust (tpd->shell, source, tpd->original_ssl_trust)) {
		/* NULL credentials to retry with those used the last time */
		e_source_invoke_authenticate (source, NULL, tpd->shell->priv->cancellable,
			shell_source_invoke_authenticate_cb, tpd->shell);
	}

	trust_prompt_data_free (tpd);
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
	TrustPromptData *tpd;

	g_return_if_fail (E_IS_SHELL (shell));

	if (response_id != GTK_RESPONSE_APPLY)
		return;

	source = g_object_get_data (G_OBJECT (alert), SOURCE_ALERT_KEY_SOURCE);
	certificate_pem = g_object_get_data (G_OBJECT (alert), SOURCE_ALERT_KEY_CERTIFICATE_PEM);
	certificate_errors = GPOINTER_TO_UINT (g_object_get_data (G_OBJECT (alert), SOURCE_ALERT_KEY_CERTIFICATE_ERRORS));
	error_text = g_object_get_data (G_OBJECT (alert), SOURCE_ALERT_KEY_ERROR_TEXT);

	g_return_if_fail (E_IS_SOURCE (source));

	g_object_set_data_full (G_OBJECT (source), SOURCE_ALERT_KEY_CERTIFICATE_PEM, g_strdup (certificate_pem), g_free);

	tpd = g_slice_new0 (TrustPromptData);
	tpd->shell = shell;
	tpd->original_ssl_trust = shell_extract_ssl_trust (source);

	e_trust_prompt_run_for_source (gtk_application_get_active_window (GTK_APPLICATION (shell)),
		source, certificate_pem, certificate_errors, error_text, TRUE,
		shell->priv->cancellable, shell_trust_prompt_done_cb, tpd);
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
		if (shell_get_source_last_trust_response (source) != E_TRUST_PROMPT_RESPONSE_REJECT) {
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
				TrustPromptData *tpd;

				g_object_set_data_full (G_OBJECT (source), SOURCE_ALERT_KEY_CERTIFICATE_PEM, g_strdup (certificate_pem), g_free);

				tpd = g_slice_new0 (TrustPromptData);
				tpd->shell = shell;
				tpd->original_ssl_trust = shell_extract_ssl_trust (source);

				e_trust_prompt_run_for_source (gtk_application_get_active_window (GTK_APPLICATION (shell)),
					source, certificate_pem, certificate_errors, op_error ? op_error->message : NULL, TRUE,
					shell->priv->cancellable, shell_trust_prompt_done_cb, tpd);
			}
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
shell_get_dialog_parent_full_cb (ECredentialsPrompter *prompter,
				 ESource *auth_source,
				 EShell *shell)
{
	GList *windows, *link;
	GtkWindow *override = NULL, *adept = NULL;

	g_return_val_if_fail (E_IS_SHELL (shell), NULL);

	if (auth_source)
		override = g_hash_table_lookup (shell->priv->auth_prompt_parents, e_source_get_uid (auth_source));

	windows = gtk_application_get_windows (GTK_APPLICATION (shell));
	for (link = windows; link; link = g_list_next (link)) {
		GtkWindow *window = link->data;

		if (!adept && E_IS_SHELL_WINDOW (window))
			adept = window;

		if (override == window || (!override && adept))
			return window;
	}

	return adept;
}

static GtkWindow *
shell_get_dialog_parent_cb (ECredentialsPrompter *prompter,
			    EShell *shell)
{
	return shell_get_dialog_parent_full_cb (prompter, NULL, shell);
}

static void
e_setup_theme_icons_theme_changed_cb (GtkSettings *gtk_settings)
{
	EPreferSymbolicIcons prefer_symbolic_icons;
	GtkCssProvider *symbolic_icons_css_provider;
	GtkIconTheme *icon_theme;
	GSettings *g_settings;
	gboolean use_symbolic_icons = FALSE;
	gboolean using_symbolic_icons;
	gchar *icon_theme_name = NULL;

	g_settings = e_util_ref_settings ("org.gnome.evolution.shell");
	prefer_symbolic_icons = g_settings_get_enum (g_settings, "prefer-symbolic-icons");
	g_clear_object (&g_settings);

	icon_theme = gtk_icon_theme_get_default ();

	switch (prefer_symbolic_icons) {
	case E_PREFER_SYMBOLIC_ICONS_NO:
		use_symbolic_icons = FALSE;
		break;
	case E_PREFER_SYMBOLIC_ICONS_YES:
		use_symbolic_icons = TRUE;
		break;
	case E_PREFER_SYMBOLIC_ICONS_AUTO:
	default:
		if (!gtk_settings)
			return;

		g_object_get (gtk_settings,
			"gtk-icon-theme-name", &icon_theme_name,
			NULL);

		if (g_strcmp0 (icon_theme_name, "HighContrast") == 0 ||
		    g_strcmp0 (icon_theme_name, "ContrastHigh") == 0) {
			use_symbolic_icons = TRUE;
		} else {
			/* pick few common action icons and check whether they are
			   only symbolic in the current theme */
			const gchar *sample_icon_names[][3] = {
				{ "appointment-new", "appointment-new-symbolic", NULL },
				{ "edit-cut", "edit-cut-symbolic", NULL },
				{ "edit-copy", "edit-copy-symbolic", NULL },
				{ "mail-reply-sender", "mail-reply-sender-symbolic", NULL }
			};
			guint n_symbolic = 0, n_non_symbolic = 0;
			guint ii;

			for (ii = 0; ii < G_N_ELEMENTS (sample_icon_names); ii++) {
				GtkIconInfo *icon_info;

				icon_info = gtk_icon_theme_choose_icon (icon_theme, sample_icon_names[ii], 32, 0);
				if (icon_info) {
					if (gtk_icon_info_is_symbolic (icon_info))
						n_symbolic++;
					else
						n_non_symbolic++;

					g_clear_object (&icon_info);
				}
			}

			use_symbolic_icons = n_symbolic > n_non_symbolic;
		}

		g_free (icon_theme_name);
		break;
	}

	/* using the same key on both objects, to save one quark */
	#define KEY_NAME "e-symbolic-icons-css-provider"

	symbolic_icons_css_provider = g_object_get_data (G_OBJECT (icon_theme), KEY_NAME);
	using_symbolic_icons = symbolic_icons_css_provider && GINT_TO_POINTER (
		g_object_get_data (G_OBJECT (symbolic_icons_css_provider), KEY_NAME)) != 0;

	if (prefer_symbolic_icons != E_PREFER_SYMBOLIC_ICONS_AUTO && !symbolic_icons_css_provider) {
		symbolic_icons_css_provider = gtk_css_provider_new ();
		g_object_set_data_full (G_OBJECT (icon_theme), KEY_NAME, symbolic_icons_css_provider, g_object_unref);

		gtk_style_context_add_provider_for_screen (gdk_screen_get_default (),
			GTK_STYLE_PROVIDER (symbolic_icons_css_provider),
			GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
	}

	if (use_symbolic_icons && !using_symbolic_icons) {
		if (!symbolic_icons_css_provider) {
			symbolic_icons_css_provider = gtk_css_provider_new ();
			g_object_set_data_full (G_OBJECT (icon_theme), KEY_NAME, symbolic_icons_css_provider, g_object_unref);

			gtk_style_context_add_provider_for_screen (gdk_screen_get_default (),
				GTK_STYLE_PROVIDER (symbolic_icons_css_provider),
				GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
		}
		gtk_css_provider_load_from_data (symbolic_icons_css_provider, "* { -gtk-icon-style:symbolic; }", -1, NULL);
		g_object_set_data (G_OBJECT (symbolic_icons_css_provider), KEY_NAME, GINT_TO_POINTER (1));
	} else if (!use_symbolic_icons && (using_symbolic_icons || prefer_symbolic_icons == E_PREFER_SYMBOLIC_ICONS_NO)) {
		gtk_css_provider_load_from_data (symbolic_icons_css_provider,
			prefer_symbolic_icons == E_PREFER_SYMBOLIC_ICONS_NO ? "* { -gtk-icon-style:regular; }" : "", -1, NULL);
		g_object_set_data (G_OBJECT (symbolic_icons_css_provider), KEY_NAME, GINT_TO_POINTER (0));
	}

	#undef KEY_NAME

	e_icon_factory_set_prefer_symbolic_icons (use_symbolic_icons);
}

static void
e_setup_theme_icons (void)
{
	GtkSettings *gtk_settings = gtk_settings_get_default ();
	GSettings *g_settings;

	e_signal_connect_notify (gtk_settings, "notify::gtk-icon-theme-name",
		G_CALLBACK (e_setup_theme_icons_theme_changed_cb), NULL);

	g_settings = e_util_ref_settings ("org.gnome.evolution.shell");
	g_signal_connect_swapped (g_settings, "changed::prefer-symbolic-icons",
		G_CALLBACK (e_setup_theme_icons_theme_changed_cb), gtk_settings);
	g_clear_object (&g_settings);

	e_setup_theme_icons_theme_changed_cb (gtk_settings);
}

static void
categories_icon_theme_hack (void)
{
	GList *categories, *link;
	GtkIconTheme *icon_theme;
	GHashTable *dirnames;
	const gchar *category_name;
	gchar *filename;
	gchar *dirname;

	icon_theme = gtk_icon_theme_get_default ();
	dirnames = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, NULL);

	/* Get the icon file for some default category.  Doesn't matter
	 * which, so long as it has an icon.  We're just interested in
	 * the directory components. */
	categories = e_categories_dup_list ();

	for (link = categories; link; link = g_list_next (link)) {
		category_name = link->data;

		filename = e_categories_dup_icon_file_for (category_name);
		if (filename && *filename) {
			/* Extract the directory components. */
			dirname = g_path_get_dirname (filename);

			if (dirname && !g_hash_table_contains (dirnames, dirname)) {
				/* Add it to the icon theme's search path.  This relies on
				 * GtkIconTheme's legacy feature of using image files found
				 * directly in the search path. */
				gtk_icon_theme_append_search_path (icon_theme, dirname);
				g_hash_table_insert (dirnames, dirname, NULL);
			} else {
				g_free (dirname);
			}
		}

		g_free (filename);
	}

	g_list_free_full (categories, g_free);
	g_hash_table_destroy (dirnames);
}

static void
shell_set_express_mode (EShell *shell,
                        gboolean express_mode)
{
	shell->priv->express_mode = express_mode;
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
	EShell *self = E_SHELL (object);
	EAlert *alert;

	if (self->priv->set_online_timeout_id > 0) {
		g_source_remove (self->priv->set_online_timeout_id);
		self->priv->set_online_timeout_id = 0;
	}

	if (self->priv->prepare_quit_timeout_id) {
		g_source_remove (self->priv->prepare_quit_timeout_id);
		self->priv->prepare_quit_timeout_id = 0;
	}

	if (self->priv->cancellable) {
		g_cancellable_cancel (self->priv->cancellable);
		g_clear_object (&self->priv->cancellable);
	}

	while ((alert = g_queue_pop_head (&self->priv->alerts)) != NULL) {
		g_signal_handlers_disconnect_by_func (
			alert, shell_alert_response_cb, object);
		g_object_unref (alert);
	}

	if (self->priv->backend_died_handler_id > 0) {
		g_signal_handler_disconnect (
			self->priv->client_cache,
			self->priv->backend_died_handler_id);
		self->priv->backend_died_handler_id = 0;
	}

	if (self->priv->allow_auth_prompt_handler_id > 0) {
		g_signal_handler_disconnect (
			self->priv->client_cache,
			self->priv->allow_auth_prompt_handler_id);
		self->priv->allow_auth_prompt_handler_id = 0;
	}

	if (self->priv->credentials_required_handler_id > 0) {
		g_signal_handler_disconnect (
			self->priv->registry,
			self->priv->credentials_required_handler_id);
		self->priv->credentials_required_handler_id = 0;
	}

	if (self->priv->get_dialog_parent_handler_id > 0) {
		g_signal_handler_disconnect (
			self->priv->credentials_prompter,
			self->priv->get_dialog_parent_handler_id);
		self->priv->get_dialog_parent_handler_id = 0;
	}

	if (self->priv->get_dialog_parent_full_handler_id > 0) {
		g_signal_handler_disconnect (
			self->priv->credentials_prompter,
			self->priv->get_dialog_parent_full_handler_id);
		self->priv->get_dialog_parent_full_handler_id = 0;
	}

	g_clear_object (&self->priv->registry);
	g_clear_object (&self->priv->credentials_prompter);
	g_clear_object (&self->priv->client_cache);
	g_clear_object (&self->priv->color_scheme_watcher);

	g_clear_pointer (&self->priv->preferences_window, gtk_widget_destroy);

	if (self->priv->preparing_for_line_change != NULL) {
		g_object_remove_weak_pointer (
			G_OBJECT (self->priv->preparing_for_line_change),
			&self->priv->preparing_for_line_change);
	}

	/* Chain up to parent's dispose() method. */
	G_OBJECT_CLASS (e_shell_parent_class)->dispose (object);
}

static void
shell_finalize (GObject *object)
{
	EShell *self = E_SHELL (object);

	g_warn_if_fail (self->priv->inhibit_cookie == 0);

	g_hash_table_destroy (self->priv->backends_by_name);
	g_hash_table_destroy (self->priv->backends_by_scheme);
	g_hash_table_destroy (self->priv->auth_prompt_parents);

	g_list_free_full (self->priv->loaded_backends, g_object_unref);

	g_free (self->priv->geometry);
	g_free (self->priv->module_directory);

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
	g_return_if_fail (E_IS_SHELL (application));

	e_file_lock_create ();

	/* Destroy the lock file when the EShell is finalized
	 * to indicate a clean shut down to the next session. */
	g_object_weak_ref (
		G_OBJECT (application),
		(GWeakNotify) e_file_lock_destroy, NULL);

	/* Chain up to parent's startup() method. */
	G_APPLICATION_CLASS (e_shell_parent_class)->startup (application);
}

static void
shell_activate (GApplication *application)
{
	EShell *shell = E_SHELL (application);
	GList *list;

	/* Do not chain up.  Default method just emits a warning. */

	if (!shell->priv->preferences_window) {
		GtkIconTheme *icon_theme;

		shell->priv->preferences_window = e_preferences_window_new (shell);
		shell->priv->color_scheme_watcher = e_color_scheme_watcher_new ();

		/* Add our icon directory to the theme's search path
		 * here instead of in main() so Anjal picks it up. */
		icon_theme = gtk_icon_theme_get_default ();
		gtk_icon_theme_append_search_path (icon_theme, EVOLUTION_ICONDIR);
		gtk_icon_theme_append_search_path (icon_theme, E_DATA_SERVER_ICONDIR);

		e_shell_load_modules (shell);

		/* Attempt migration -after- loading all modules and plugins,
		 * as both shell backends and certain plugins hook into this. */
		e_shell_migrate_attempt (shell);

		categories_icon_theme_hack ();
		e_setup_theme_icons ();

		e_shell_event (shell, "ready-to-start", NULL);
	}

	shell->priv->activate_created_window = FALSE;

	if (!shell->priv->started)
		return;

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

	shell->priv->activate_created_window = TRUE;

	/* No EShellWindow found, so create one. */
	e_shell_create_shell_window (E_SHELL (application), NULL);
}

static void
shell_shutdown (GApplication *application)
{
	EShell *shell = E_SHELL (application);

	if (shell->priv->inhibit_cookie > 0) {
		gtk_application_uninhibit (GTK_APPLICATION (application), shell->priv->inhibit_cookie);
		shell->priv->inhibit_cookie = 0;
	}

	/* Chain up to parent's method. */
	G_APPLICATION_CLASS (e_shell_parent_class)->shutdown (application);
}

static void
shell_window_added (GtkApplication *application,
                    GtkWindow *window)
{
	gchar *role;

	/* Chain up to parent's window_added() method. */
	GTK_APPLICATION_CLASS (e_shell_parent_class)->
		window_added (application, window);

	/* Register a callback to properly quit when the window is closed, but
	 * run it last so other handlers also have a chance to run beforehand. */
	g_signal_connect_after (
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

G_GNUC_NORETURN static gboolean
option_version_cb (const gchar *option_name,
                   const gchar *option_value,
                   gpointer data,
                   GError **error)
{
	g_print ("%s %s%s %s\n", PACKAGE, VERSION, VERSION_SUBSTRING, VERSION_COMMENT);

	exit (0);
}

/* Command-line options.  */
#ifdef G_OS_WIN32
static gboolean register_handlers = FALSE;
static gboolean reinstall = FALSE;
static gboolean show_icons = FALSE;
static gboolean hide_icons = FALSE;
static gboolean unregister_handlers = FALSE;
#endif /* G_OS_WIN32 */
static gboolean force_online = FALSE;
static gboolean start_online = FALSE;
static gboolean start_offline = FALSE;
static gboolean setup_only = FALSE;
static gboolean force_shutdown = FALSE;
static gboolean disable_eplugin = FALSE;
static gboolean disable_preview = FALSE;
static gboolean import_uris = FALSE;
static gboolean view_uris = FALSE;
static gboolean quit = FALSE;

static gchar *geometry = NULL;
static gchar *requested_view = NULL;
static gchar **remaining_args;

static GOptionEntry app_options[] = {
#ifdef G_OS_WIN32
	{ "register-handlers", '\0', G_OPTION_FLAG_HIDDEN,
	  G_OPTION_ARG_NONE, &register_handlers, NULL, NULL },
	{ "reinstall", '\0', G_OPTION_FLAG_HIDDEN, G_OPTION_ARG_NONE, &reinstall,
	  NULL, NULL },
	{ "show-icons", '\0', G_OPTION_FLAG_HIDDEN, G_OPTION_ARG_NONE, &show_icons,
	  NULL, NULL },
	{ "hide-icons", '\0', G_OPTION_FLAG_HIDDEN, G_OPTION_ARG_NONE, &hide_icons,
	  NULL, NULL },
	{ "unregister-handlers", '\0', G_OPTION_FLAG_HIDDEN,
	  G_OPTION_ARG_NONE, &unregister_handlers, NULL, NULL },
#endif /* G_OS_WIN32 */
	{ "component", 'c', 0, G_OPTION_ARG_STRING, &requested_view,
	/* Translators: Do NOT translate the five component
	 * names, they MUST remain in English! */
	  N_("Start Evolution showing the specified component. "
	     "Available options are “mail”, “calendar”, “contacts”, "
	     "“tasks”, and “memos”"), "COMPONENT" },
	{ "geometry", 'g', 0, G_OPTION_ARG_STRING, &geometry,
	  N_("Apply the given geometry to the main window"), "GEOMETRY" },
	{ "offline", '\0', 0, G_OPTION_ARG_NONE, &start_offline,
	  N_("Start in offline mode"), NULL },
	{ "online", '\0', 0, G_OPTION_ARG_NONE, &start_online,
	  N_("Start in online mode"), NULL },
	{ "force-online", '\0', 0, G_OPTION_ARG_NONE, &force_online,
	  N_("Ignore network availability"), NULL },
#ifndef G_OS_WIN32
	{ "force-shutdown", '\0', 0, G_OPTION_ARG_NONE, &force_shutdown,
	  N_("Forcibly shut down Evolution and background Evolution-Data-Server processes"), NULL },
#endif
	{ "disable-eplugin", '\0', 0, G_OPTION_ARG_NONE, &disable_eplugin,
	  N_("Disable loading of any plugins."), NULL },
	{ "disable-preview", '\0', 0, G_OPTION_ARG_NONE, &disable_preview,
	  N_("Disable preview pane of Mail, Contacts and Tasks."), NULL },
	{ "setup-only", '\0', G_OPTION_FLAG_HIDDEN, G_OPTION_ARG_NONE,
	  &setup_only, NULL, NULL },
	{ "import", 'i', 0, G_OPTION_ARG_NONE, &import_uris,
	  N_("Import URIs or filenames given as rest of arguments."), NULL },
	{ "view", '\0', 0, G_OPTION_ARG_NONE, &view_uris,
	  N_("View URIs or filenames given as rest of arguments."), NULL },
	{ "quit", 'q', 0, G_OPTION_ARG_NONE, &quit,
	  N_("Request a running Evolution process to quit"), NULL },
	{ "version", 'v', G_OPTION_FLAG_HIDDEN | G_OPTION_FLAG_NO_ARG,
	  G_OPTION_ARG_CALLBACK, option_version_cb, NULL, NULL },
	{ G_OPTION_REMAINING, 0, 0, G_OPTION_ARG_STRING_ARRAY,
	  &remaining_args, NULL, NULL },
	{ NULL }
};

static gboolean
handle_options_idle_cb (gpointer user_data)
{
	const gchar * const *uris = (const gchar * const *) user_data;
	EShell *shell;

	shell = e_shell_get_default ();

	/* These calls do the right thing when another Evolution
	 * process is running. */
	if (uris != NULL && *uris != NULL) {
		if (e_shell_handle_uris (shell, uris, import_uris, view_uris) == 0)
			g_application_quit (G_APPLICATION (shell));
	} else {
		e_shell_create_shell_window (shell, requested_view);
	}

	shell->priv->started = TRUE;
	g_application_release (G_APPLICATION (shell));

	/* If another Evolution process is running, we're done. */
	if (g_application_get_is_remote (G_APPLICATION (shell)))
		g_application_quit (G_APPLICATION (shell));

	return FALSE;
}

static gint
e_shell_handle_local_options_cb (GApplication *application,
				 GVariantDict *options,
				 gpointer user_data)
{
	EShell *shell = E_SHELL (application);
	GSettings *settings;
	gboolean online = TRUE;

	settings = e_util_ref_settings ("org.gnome.evolution.shell");

	/* Requesting online or offline mode from the command-line
	 * should be persistent, just like selecting it in the UI. */

	if (start_online || force_online) {
		online = TRUE;
		g_settings_set_boolean (settings, "start-offline", FALSE);
	} else if (start_offline) {
		online = FALSE;
		g_settings_set_boolean (settings, "start-offline", TRUE);
	} else {
		online = !g_settings_get_boolean (settings, "start-offline");
	}

	shell->priv->online = online;

	g_clear_object (&settings);

	g_clear_pointer (&shell->priv->geometry, g_free);
	shell->priv->geometry = g_strdup (geometry);

#ifdef G_OS_WIN32
	if (register_handlers || reinstall || show_icons) {
		_e_win32_register_mailer ();
		_e_win32_register_addressbook ();
	}

	if (register_handlers)
		return 0;

	if (reinstall) {
		_e_win32_set_default_mailer ();
		return 0;
	}

	if (show_icons) {
		_e_win32_set_default_mailer ();
		return 0;
	}

	if (hide_icons) {
		_e_win32_unset_default_mailer ();
		return 0;
	}

	if (unregister_handlers) {
		_e_win32_unregister_mailer ();
		_e_win32_unregister_addressbook ();
		return 0;
	}

	if (!is_any_gettext_catalog_installed ()) {
		/* No message catalog installed for the current locale
		 * language, so don't bother with the localisations
		 * provided by other things then either. Reset thread
		 * locale to "en-US" and C library locale to "C". */
		SetThreadLocale (
			MAKELCID (MAKELANGID (LANG_ENGLISH, SUBLANG_ENGLISH_US),
			SORT_DEFAULT));
		setlocale (LC_ALL, "C");
	}
#endif

	if (start_online && start_offline) {
		g_printerr (
			_("%s: --online and --offline cannot be used "
			"together.\n  Run “%s --help” for more "
			"information.\n"), g_get_prgname (), g_get_prgname ());
		return 1;
	} else if (force_online && start_offline) {
		g_printerr (
			_("%s: --force-online and --offline cannot be used "
			"together.\n  Run “%s --help” for more "
			"information.\n"), g_get_prgname (), g_get_prgname ());
		return 1;
	}

	if (force_shutdown) {
		gchar *filename;

		filename = g_build_filename (EVOLUTION_TOOLSDIR, "killev", NULL);
		execl (filename, "killev", NULL);

		return 2;
	}

	if (disable_preview) {
		settings = e_util_ref_settings ("org.gnome.evolution.mail");
		g_settings_set_boolean (settings, "safe-list", TRUE);
		g_object_unref (settings);

		settings = e_util_ref_settings ("org.gnome.evolution.addressbook");
		g_settings_set_boolean (settings, "show-preview", FALSE);
		g_object_unref (settings);

		settings = e_util_ref_settings ("org.gnome.evolution.calendar");
		g_settings_set_boolean (settings, "show-memo-preview", FALSE);
		g_settings_set_boolean (settings, "show-task-preview", FALSE);
		g_settings_set_boolean (settings, "year-show-preview", FALSE);
		g_object_unref (settings);
	}

	if (setup_only)
		return 0;

	if (quit) {
		e_shell_quit (E_SHELL (application), E_SHELL_QUIT_OPTION);
		return 0;
	}

	if (g_application_get_is_remote (application)) {
		g_application_activate (application);

		if (remaining_args && *remaining_args)
			e_shell_handle_uris (E_SHELL (application), (const gchar * const *) remaining_args, import_uris, view_uris);

		/* This will be redirected to the previously run instance,
		   because this instance is remote. */
		if (requested_view && *requested_view)
			e_shell_create_shell_window (E_SHELL (application), requested_view);

		return 0;
	}

	if (force_online)
		e_shell_lock_network_available (shell);

	/* to not have the papplication shutdown due to no window being created */
	g_application_hold (G_APPLICATION (shell));

	g_idle_add (handle_options_idle_cb, (gpointer) remaining_args);

	if (!disable_eplugin) {
		/* Register built-in plugin hook types. */
		g_type_ensure (E_TYPE_IMPORT_HOOK);
		g_type_ensure (E_TYPE_PLUGIN_UI_HOOK);

		/* All EPlugin and EPluginHook subclasses should be
		 * registered in GType now, so load plugins now. */
		e_plugin_load_plugins ();
	}

	return -1;
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

	shell->priv->get_dialog_parent_full_handler_id = g_signal_connect (
		shell->priv->credentials_prompter, "get-dialog-parent-full",
		G_CALLBACK (shell_get_dialog_parent_full_cb), shell);

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

	if (!e_util_get_use_header_bar ()) {
		/* Forbid header bars in stock GTK+ dialogs.
		 * They look very out of place in Evolution. */
		g_object_set (
			gtk_settings_get_default (),
			"gtk-dialogs-use-header", FALSE, NULL);
	}

	return TRUE;
}

static void
e_shell_class_init (EShellClass *class)
{
	GObjectClass *object_class;
	GApplicationClass *application_class;
	GtkApplicationClass *gtk_application_class;

	object_class = G_OBJECT_CLASS (class);
	object_class->set_property = shell_set_property;
	object_class->get_property = shell_get_property;
	object_class->dispose = shell_dispose;
	object_class->finalize = shell_finalize;
	object_class->constructed = shell_constructed;

	application_class = G_APPLICATION_CLASS (class);
	application_class->startup = shell_startup;
	application_class->activate = shell_activate;
	application_class->shutdown = shell_shutdown;

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
	 * EShell::view-uri
	 * @shell: the #EShell which emitted the signal
	 * @uri: the URI to be viewed
	 *
	 * Emitted when @shell receives a URI to be viewed, usually by
	 * way of a command-line argument.  An #EShellBackend should listen
	 * for this signal and try to show the URI, usually by opening an
	 * viewer window for the identified resource.
	 *
	 * Returns: %TRUE if the URI could be viewed, %FALSE otherwise
	 *
	 * Since: 3.54
	 **/
	signals[VIEW_URI] = g_signal_new (
		"view-uri",
		G_OBJECT_CLASS_TYPE (object_class),
		G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
		0 /* G_STRUCT_OFFSET (EShellClass, view_uri) */,
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

	shell->priv = e_shell_get_instance_private (shell);

	backends_by_name = g_hash_table_new (g_str_hash, g_str_equal);
	backends_by_scheme = g_hash_table_new (g_str_hash, g_str_equal);

	g_queue_init (&shell->priv->alerts);

	shell->priv->cancellable = g_cancellable_new ();
	shell->priv->backends_by_name = backends_by_name;
	shell->priv->backends_by_scheme = backends_by_scheme;
	shell->priv->auth_prompt_parents = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, NULL);
	shell->priv->safe_mode = e_file_lock_exists ();

	e_signal_connect_notify (
		shell, "notify::online",
		G_CALLBACK (shell_notify_online_cb), NULL);

	g_signal_connect (
		shell, "handle-local-options",
		G_CALLBACK (e_shell_handle_local_options_cb), NULL);

	g_application_add_main_option_entries (G_APPLICATION (shell), app_options);
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
	GList *list;

	g_return_if_fail (E_IS_SHELL (shell));

	if (shell->priv->modules_loaded)
		return;

	/* Process shell backends. */

	list = g_list_sort (
		e_extensible_list_extensions (
		E_EXTENSIBLE (shell), E_TYPE_SHELL_BACKEND),
		(GCompareFunc) e_shell_backend_compare);
	g_list_foreach (list, (GFunc) shell_process_backend, shell);
	shell->priv->loaded_backends = list;

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
	gboolean source_enabled;

	g_return_if_fail (E_IS_SHELL (shell));
	g_return_if_fail (E_IS_SOURCE (source));

	source_enabled = e_source_registry_check_enabled (shell->priv->registry, source);

	e_credentials_prompter_set_auto_prompt_disabled_for (shell->priv->credentials_prompter, source, !source_enabled);

	if (!source_enabled)
		return;

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
 * @do_view: request a view of the URIs
 *
 * Emits the #EShell::handle-uri signal for each URI.
 *
 * Returns: the number of URIs successfully handled
 **/
guint
e_shell_handle_uris (EShell *shell,
                     const gchar * const *uris,
                     gboolean do_import,
		     gboolean do_view)
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

			if (do_view) {
				g_signal_emit (
					shell, signals[VIEW_URI],
					0, uris[ii], &handled);
			} else {
				g_signal_emit (
					shell, signals[HANDLE_URI],
					0, uris[ii], &handled);
			}
			n_handled += handled ? 1 : 0;
		}

		if (n_handled == 0 && !do_view)
			n_handled = e_shell_utils_import_uris (shell, uris);
	}

	return n_handled;

remote:  /* Send a message to the other Evolution process. */

	cwd = g_get_current_dir ();
	args = g_ptr_array_sized_new (g_strv_length ((gchar **) uris) + 2);

	g_ptr_array_add (args, (gchar *) "--use-cwd");
	g_ptr_array_add (args, cwd);

	if (do_import)
		g_ptr_array_add (args, (gchar *) "--import");
	if (do_view)
		g_ptr_array_add (args, (gchar *) "--view");

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
 * In the first phase, the @shell emits an #EShell::quit-requested signal
 * to potentially give the user a chance to cancel shutdown.  If the user
 * cancels shutdown, the function returns %FALSE.  Otherwise it proceeds
 * into the second phase.
 *
 * In the second phase, the @shell emits an #EShell::prepare-for-quit
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
 * Note: This function has no effect during an #EShell::prepare-for-quit
 * signal emission.
 **/
void
e_shell_cancel_quit (EShell *shell)
{
	g_return_if_fail (E_IS_SHELL (shell));

	shell->priv->quit_cancelled = TRUE;

	g_signal_stop_emission (shell, signals[QUIT_REQUESTED], 0);
}

/**
 * e_shell_set_auth_prompt_parent:
 * @shell: an #EShell
 * @source: an #ESource
 * @parent: (nullable): a #GtkWindow
 *
 * Sets an override for a credential prompt parent window.
 *
 * Since: 3.42
 **/
void
e_shell_set_auth_prompt_parent (EShell *shell,
				ESource *source,
				GtkWindow *parent)
{
	g_return_if_fail (E_IS_SHELL (shell));
	g_return_if_fail (E_IS_SOURCE (source));
	g_return_if_fail (e_source_get_uid (source));

	if (parent) {
		g_hash_table_insert (shell->priv->auth_prompt_parents, g_strdup (e_source_get_uid (source)), parent);
	} else {
		g_hash_table_remove (shell->priv->auth_prompt_parents, e_source_get_uid (source));
	}
}
