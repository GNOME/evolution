/*
 * e-mail-backend.c
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
 * Authors:
 *   Jonathon Jongsma <jonathon.jongsma@collabora.co.uk>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 * Copyright (C) 2009 Intel Corporation
 *
 */

#include "evolution-config.h"

#include <errno.h>
#include <string.h>
#include <glib/gstdio.h>
#include <glib/gi18n-lib.h>

#include "shell/e-shell.h"

#include "e-mail-folder-tweaks.h"
#include "e-mail-migrate.h"
#include "e-mail-ui-session.h"
#include "em-event.h"
#include "em-folder-tree-model.h"
#include "em-utils.h"
#include "mail-autofilter.h"
#include "mail-send-recv.h"
#include "mail-vfolder-ui.h"

#include "e-mail-backend.h"

#define QUIT_POLL_INTERVAL 1  /* seconds */

struct _EMailBackendPrivate {
	EMailSession *session;
	GHashTable *jobs;
	EMailSendAccountOverride *send_account_override;
	EMailRemoteContent *remote_content;
	EMailProperties *mail_properties;
};

enum {
	PROP_0,
	PROP_SESSION,
	PROP_SEND_ACCOUNT_OVERRIDE,
	PROP_REMOTE_CONTENT,
	PROP_MAIL_PROPERTIES
};

G_DEFINE_ABSTRACT_TYPE_WITH_PRIVATE (EMailBackend, e_mail_backend, E_TYPE_SHELL_BACKEND)

static const gchar *
mail_shell_backend_get_data_dir (EShellBackend *backend)
{
	return mail_session_get_data_dir ();
}

static const gchar *
mail_shell_backend_get_config_dir (EShellBackend *backend)
{
	return mail_session_get_config_dir ();
}

static gchar *
mail_backend_uri_to_evname (const gchar *uri,
                            const gchar *prefix)
{
	const gchar *data_dir;
	gchar *basename;
	gchar *filename;
	gchar *safe;

	/* Converts a folder URI to a GalView filename. */

	data_dir = mail_session_get_data_dir ();

	safe = g_strdup (uri);
	e_util_make_safe_filename (safe);
	basename = g_strdup_printf ("%s%s.xml", prefix, safe);
	filename = g_build_filename (data_dir, basename, NULL);
	g_free (basename);
	g_free (safe);

	return filename;
}

static gboolean
mail_backend_any_store_requires_downsync (EMailAccountStore *account_store)
{
	GQueue queue = G_QUEUE_INIT;

	g_return_val_if_fail (E_IS_MAIL_ACCOUNT_STORE (account_store), FALSE);

	e_mail_account_store_queue_enabled_services (account_store, &queue);

	while (!g_queue_is_empty (&queue)) {
		CamelService *service;
		CamelOfflineStore *offline_store;

		service = g_queue_pop_head (&queue);

		if (!CAMEL_IS_OFFLINE_STORE (service))
			continue;

		offline_store = CAMEL_OFFLINE_STORE (service);

		if (camel_offline_store_requires_downsync (offline_store))
			return TRUE;
	}

	return FALSE;
}

/* Callback for various asynchronous CamelStore operations where
 * the EActivity's reference count is used as a counting semaphore. */
static void
mail_backend_store_operation_done_cb (CamelStore *store,
                                      GAsyncResult *result,
                                      EActivity *activity)
{
	/* FIXME Not checking result for error.  To fix this, we need
	 *       separate callbacks to call different finish functions
	 *       and then submit an EAlert on error. */

	g_object_unref (activity);
}

static void
mail_backend_store_go_online_done_cb (CamelStore *store,
				      GAsyncResult *result,
				      EActivity *activity)
{
	CamelService *service;

	service = CAMEL_SERVICE (store);

	if (e_mail_store_go_online_finish (store, result, NULL) &&
	    camel_service_get_connection_status (service) == CAMEL_SERVICE_CONNECTED) {
		CamelSession *session;

		session = camel_service_ref_session (service);

		if (E_IS_MAIL_SESSION (session) && camel_session_get_online (session)) {
			ESourceRegistry *registry;
			ESource *source;
			GSettings *settings;
			gboolean all_on_start;

			settings = e_util_ref_settings ("org.gnome.evolution.mail");
			all_on_start = g_settings_get_boolean (settings, "send-recv-all-on-start");
			g_object_unref (settings);

			registry = e_mail_session_get_registry (E_MAIL_SESSION (session));
			source = e_source_registry_ref_source (registry, camel_service_get_uid (service));

			if (source && e_source_has_extension (source, E_SOURCE_EXTENSION_REFRESH) &&
			    (all_on_start || e_source_refresh_get_enabled (e_source_get_extension (source, E_SOURCE_EXTENSION_REFRESH)))) {
				e_source_refresh_force_timeout (source);
			}

			g_clear_object (&source);
		}

		g_clear_object (&session);
	}

	g_object_unref (activity);
}

static void
mail_backend_local_trash_expunge_done_cb (GObject *source_object,
                                          GAsyncResult *result,
                                          gpointer user_data)
{
	CamelFolder *folder = CAMEL_FOLDER (source_object);
	EActivity *activity = user_data;
	GError *local_error = NULL;

	e_mail_folder_expunge_finish (folder, result, &local_error);

	if (local_error != NULL) {
		g_warning (
			"%s: Failed to expunge local trash: %s",
			G_STRFUNC, local_error->message);
		g_error_free (local_error);
	}

	g_object_unref (activity);
}

static void
mail_backend_set_session_offline_cb (gpointer user_data,
                                     GObject *object)
{
	CamelSession *session = user_data;

	g_return_if_fail (CAMEL_IS_SESSION (session));

	camel_session_set_online (session, FALSE);
	g_object_unref (session);
}

static void
mail_backend_prepare_for_offline_cb (EShell *shell,
                                     EActivity *activity,
                                     EMailBackend *backend)
{
	GtkWindow *window;
	EMailSession *session;
	EMailAccountStore *account_store;
	EShellBackend *shell_backend;
	GQueue queue = G_QUEUE_INIT;

	shell_backend = E_SHELL_BACKEND (backend);

	window = e_shell_get_active_window (shell);
	session = e_mail_backend_get_session (backend);
	account_store = e_mail_ui_session_get_account_store (E_MAIL_UI_SESSION (session));

	if (!e_shell_get_network_available (shell)) {
		camel_session_set_online (CAMEL_SESSION (session), FALSE);
		camel_operation_cancel_all ();
	}

	if (e_shell_backend_is_started (shell_backend)) {
		gboolean ask_to_synchronize;
		gboolean synchronize = FALSE;
		GCancellable *cancellable;

		ask_to_synchronize =
			e_shell_get_network_available (shell) &&
			mail_backend_any_store_requires_downsync (account_store);

		if (ask_to_synchronize) {
			synchronize = e_util_prompt_user (
				window, "org.gnome.evolution.mail", NULL, "mail:ask-quick-offline", NULL);
		}

		if (!synchronize) {
			e_shell_backend_cancel_all (shell_backend);
			camel_session_set_online (CAMEL_SESSION (session), FALSE);
		}

		cancellable = e_activity_get_cancellable (activity);
		if (!cancellable) {
			cancellable = camel_operation_new ();
			e_activity_set_cancellable (activity, cancellable);
			g_object_unref (cancellable);
		} else {
			/* Maybe the cancellable just got cancelled when the above
			   camel_operation_cancel_all() had been called, but we want
			   it alive for the following "go-offline" operation, thus reset it. */
			g_cancellable_reset (cancellable);
		}

		e_shell_backend_add_activity (shell_backend, activity);
	}

	g_object_weak_ref (
		G_OBJECT (activity),
		mail_backend_set_session_offline_cb,
		g_object_ref (session));

	e_mail_account_store_queue_enabled_services (account_store, &queue);
	while (!g_queue_is_empty (&queue)) {
		CamelService *service;

		service = g_queue_pop_head (&queue);

		if (!CAMEL_IS_STORE (service))
			continue;

		e_mail_store_go_offline (
			CAMEL_STORE (service), G_PRIORITY_DEFAULT,
			e_activity_get_cancellable (activity),
			(GAsyncReadyCallback) mail_backend_store_operation_done_cb,
			g_object_ref (activity));
	}
}

static void
mail_backend_prepare_for_online_cb (EShell *shell,
                                    EActivity *activity,
                                    EMailBackend *backend)
{
	EMailSession *session;
	EMailAccountStore *account_store;
	GQueue queue = G_QUEUE_INIT;
	GSettings *settings;
	gboolean with_send_recv;

	if (e_shell_backend_is_started (E_SHELL_BACKEND (backend))) {
		if (!e_activity_get_cancellable (activity)) {
			GCancellable *cancellable;

			cancellable = camel_operation_new ();
			e_activity_set_cancellable (activity, cancellable);
			g_object_unref (cancellable);
		}

		e_shell_backend_add_activity (E_SHELL_BACKEND (backend), activity);
	}

	settings = e_util_ref_settings ("org.gnome.evolution.mail");
	with_send_recv = g_settings_get_boolean (settings, "send-recv-on-start");
	g_object_unref (settings);

	session = e_mail_backend_get_session (backend);
	account_store = e_mail_ui_session_get_account_store (E_MAIL_UI_SESSION (session));

	camel_session_set_online (CAMEL_SESSION (session), TRUE);

	e_mail_account_store_queue_enabled_services (account_store, &queue);
	while (!g_queue_is_empty (&queue)) {
		CamelService *service;

		service = g_queue_pop_head (&queue);
		if (service == NULL)
			continue;

		if (CAMEL_IS_STORE (service))
			e_mail_store_go_online (
				CAMEL_STORE (service), G_PRIORITY_DEFAULT,
				e_activity_get_cancellable (activity),
				(GAsyncReadyCallback) (with_send_recv ? mail_backend_store_go_online_done_cb : mail_backend_store_operation_done_cb),
				g_object_ref (activity));
	}
}

/* Helper for mail_backend_prepare_for_quit_cb() */
static void
mail_backend_delete_junk (CamelService *service,
                          EMailBackend *backend)
{
	CamelFolder *folder;
	GPtrArray *uids;
	guint32 flags;
	guint32 mask;
	guint ii;

	/* FIXME camel_store_get_junk_folder_sync() may block. */
	folder = camel_store_get_junk_folder_sync (
		CAMEL_STORE (service), NULL, NULL);
	if (folder == NULL)
		return;

	uids = camel_folder_dup_uids (folder);
	flags = mask = CAMEL_MESSAGE_DELETED | CAMEL_MESSAGE_SEEN;

	camel_folder_freeze (folder);

	for (ii = 0; ii < uids->len; ii++) {
		const gchar *uid = uids->pdata[ii];
		camel_folder_set_message_flags (folder, uid, flags, mask);
	}

	camel_folder_thaw (folder);

	g_ptr_array_unref (uids);
	g_object_unref (folder);
}

/* Helper for mail_backend_prepare_for_quit_cb() */
static gboolean
mail_backend_poll_to_quit (gpointer user_data)
{
	return mail_msg_active ();
}

static gboolean
mail_backend_service_is_enabled (ESourceRegistry *registry,
                                 CamelService *service)
{
	const gchar *uid;
	ESource *source;
	gboolean enabled;

	g_return_val_if_fail (registry != NULL, FALSE);
	g_return_val_if_fail (service != NULL, FALSE);

	uid = camel_service_get_uid (service);
	g_return_val_if_fail (uid != NULL, FALSE);

	source = e_source_registry_ref_source (registry, uid);
	if (!source)
		return FALSE;

	enabled = e_source_registry_check_enabled (registry, source);
	g_object_unref (source);

	return enabled;
}

static void
mail_backend_prepare_for_quit_cb (EShell *shell,
                                  EActivity *activity,
                                  EMailBackend *backend)
{
	EMailSession *session;
	ESourceRegistry *registry;
	GList *list, *link;
	GCancellable *cancellable;
	gboolean delete_junk;
	gboolean empty_trash;

	session = e_mail_backend_get_session (backend);
	registry = e_shell_get_registry (shell);

	delete_junk = e_mail_backend_delete_junk_policy_decision (backend);
	empty_trash = e_mail_backend_empty_trash_policy_decision (backend);

	camel_application_is_exiting = TRUE;

	camel_operation_cancel_all ();
	mail_vfolder_shutdown ();

	cancellable = e_activity_get_cancellable (activity);
	if (cancellable) {
		/* Maybe the cancellable just got cancelled when the above
		   camel_operation_cancel_all() had been called, but we want
		   it alive for the following operations, thus reset it. */
		g_cancellable_reset (cancellable);
	}

	list = camel_session_list_services (CAMEL_SESSION (session));

	if (delete_junk) {
		for (link = list; link != NULL; link = g_list_next (link)) {
			CamelService *service;

			service = CAMEL_SERVICE (link->data);

			if (!CAMEL_IS_STORE (service) ||
			    !mail_backend_service_is_enabled (registry, service))
				continue;

			mail_backend_delete_junk (service, backend);
		}
	}

	for (link = list; link != NULL; link = g_list_next (link)) {
		CamelService *service;
		gboolean store_is_local;
		const gchar *uid;

		service = CAMEL_SERVICE (link->data);

		if (!CAMEL_IS_STORE (service))
			continue;

		if (!mail_backend_service_is_enabled (registry, service))
			continue;

		uid = camel_service_get_uid (service);
		store_is_local = g_strcmp0 (uid, E_MAIL_SESSION_LOCAL_UID);

		if (empty_trash && store_is_local) {
			/* local trash requires special handling,
			 * due to POP3's "delete-expunged" option */
			CamelFolder *local_trash;

			/* This should be lightning-fast since
			 * it's just the local trash folder. */
			local_trash = camel_store_get_trash_folder_sync (
				CAMEL_STORE (service), cancellable, NULL);

			if (local_trash != NULL) {
				e_mail_folder_expunge (
					local_trash,
					G_PRIORITY_DEFAULT, cancellable,
					mail_backend_local_trash_expunge_done_cb,
					g_object_ref (activity));

				g_object_unref (local_trash);
			}
		} else {
			/* FIXME Not passing a GCancellable. */
			/* FIXME This operation should be queued. */
			camel_store_synchronize (
				CAMEL_STORE (service),
				empty_trash, G_PRIORITY_DEFAULT,
				NULL, (GAsyncReadyCallback)
				mail_backend_store_operation_done_cb,
				g_object_ref (activity));
		}
	}

	g_list_free_full (list, (GDestroyNotify) g_object_unref);

	/* Now we poll until all activities are actually cancelled or finished.
	 * Reffing the activity delays quitting; the reference count
	 * acts like a counting semaphore. */
	if (mail_msg_active ()) {
		e_named_timeout_add_seconds_full (
			G_PRIORITY_DEFAULT,
			QUIT_POLL_INTERVAL,
			mail_backend_poll_to_quit,
			g_object_ref (activity),
			(GDestroyNotify) g_object_unref);
	}
}

static void
mail_backend_quit_requested_cb (EShell *shell,
                                EShellQuitReason reason,
                                EShellBackend *mail_shell_backend)
{
	EMailBackend *backend;
	EMailSession *session;
	CamelFolder *folder;
	GtkWindow *window;
	GList *app_windows;
	gint response;

	window = e_shell_get_active_window (shell);

	/* We can quit immediately if offline. */
	if (!e_shell_get_online (shell))
		return;

	/* Or if another Evolution process asked us to. */
	if (reason == E_SHELL_QUIT_REMOTE_REQUEST)
		return;

	if (!e_shell_backend_is_started (mail_shell_backend))
		return;

	/* Check Outbox for any unsent messages. */

	backend = E_MAIL_BACKEND (mail_shell_backend);
	session = e_mail_backend_get_session (backend);

	folder = e_mail_session_get_local_folder (
		session, E_MAIL_LOCAL_FOLDER_OUTBOX);
	if (folder == NULL)
		return;

	if (camel_folder_summary_get_visible_count (camel_folder_get_folder_summary (folder)) == 0)
		return;

	app_windows = gtk_application_get_windows (GTK_APPLICATION (shell));
	while (app_windows) {
		if (E_IS_SHELL_WINDOW (app_windows->data))
			break;

		app_windows = g_list_next (app_windows);
	}

	/* Either there is any EShellWindow available, then the quit can be
	   truly cancelled, or there is none and the question is useless. */
	if (!app_windows)
		return;

	response = e_alert_run_dialog_for_args (
		window, "mail:exit-unsent-question", NULL);

	if (response == GTK_RESPONSE_YES)
		return;

	e_shell_cancel_quit (shell);
}

static void
mail_backend_folder_deleted_cb (MailFolderCache *folder_cache,
                                CamelStore *store,
                                const gchar *folder_name,
                                EMailBackend *backend)
{
	EShell *shell;
	CamelStoreClass *class;
	ESourceRegistry *registry;
	EShellBackend *shell_backend;
	EMailFolderTweaks *tweaks;
	EMailSession *session;
	EAlertSink *alert_sink;
	GList *list, *link;
	const gchar *extension_name;
	const gchar *local_drafts_folder_uri;
	const gchar *local_sent_folder_uri;
	gchar *uri;

	/* Check whether the deleted folder was a designated Drafts or
	 * Sent folder for any mail account, and if so revert the setting
	 * to the equivalent local folder, which is always present. */

	shell_backend = E_SHELL_BACKEND (backend);
	shell = e_shell_backend_get_shell (shell_backend);
	registry = e_shell_get_registry (shell);

	class = CAMEL_STORE_GET_CLASS (store);
	g_return_if_fail (class->equal_folder_name != NULL);

	session = e_mail_backend_get_session (backend);
	alert_sink = e_mail_backend_get_alert_sink (backend);

	local_drafts_folder_uri =
		e_mail_session_get_local_folder_uri (
		session, E_MAIL_LOCAL_FOLDER_DRAFTS);

	local_sent_folder_uri =
		e_mail_session_get_local_folder_uri (
		session, E_MAIL_LOCAL_FOLDER_SENT);

	uri = e_mail_folder_uri_build (store, folder_name);

	extension_name = E_SOURCE_EXTENSION_MAIL_COMPOSITION;
	list = e_source_registry_list_sources (registry, extension_name);

	for (link = list; link != NULL; link = g_list_next (link)) {
		ESource *source = E_SOURCE (link->data);
		ESourceExtension *extension;
		const gchar *drafts_folder_uri;

		extension = e_source_get_extension (source, extension_name);

		drafts_folder_uri =
			e_source_mail_composition_get_drafts_folder (
			E_SOURCE_MAIL_COMPOSITION (extension));

		if (!drafts_folder_uri)
			continue;

		if (class->equal_folder_name (drafts_folder_uri, uri)) {
			GError *error = NULL;

			e_source_mail_composition_set_drafts_folder (
				E_SOURCE_MAIL_COMPOSITION (extension),
				local_drafts_folder_uri);

			/* FIXME This is a blocking D-Bus method call. */
			if (!e_source_write_sync (source, NULL, &error)) {
				g_warning ("%s", error->message);
				g_error_free (error);
			}
		}
	}

	g_list_free_full (list, (GDestroyNotify) g_object_unref);

	extension_name = E_SOURCE_EXTENSION_MAIL_SUBMISSION;
	list = e_source_registry_list_sources (registry, extension_name);

	for (link = list; link != NULL; link = g_list_next (link)) {
		ESource *source = E_SOURCE (link->data);
		ESourceMailSubmission *extension;
		const gchar *sent_folder_uri;

		extension = e_source_get_extension (source, extension_name);

		sent_folder_uri = e_source_mail_submission_get_sent_folder (extension);

		if (sent_folder_uri == NULL)
			continue;

		if (class->equal_folder_name (sent_folder_uri, uri)) {
			GError *error = NULL;

			e_source_mail_submission_set_sent_folder (extension, local_sent_folder_uri);

			/* FIXME This is a blocking D-Bus method call. */
			if (!e_source_write_sync (source, NULL, &error)) {
				g_warning ("%s", error->message);
				g_error_free (error);
			}
		}
	}

	g_list_free_full (list, (GDestroyNotify) g_object_unref);

	tweaks = e_mail_folder_tweaks_new ();
	e_mail_folder_tweaks_folder_deleted (tweaks, uri);
	g_clear_object (&tweaks);

	g_free (uri);

	/* This does something completely different.
	 * XXX Make it a separate signal handler? */
	mail_filter_delete_folder (store, folder_name, alert_sink);
}

static void
mail_backend_folder_renamed_cb (MailFolderCache *folder_cache,
                                CamelStore *store,
                                const gchar *old_folder_name,
                                const gchar *new_folder_name,
                                EMailBackend *backend)
{
	EShell *shell;
	CamelStoreClass *class;
	ESourceRegistry *registry;
	EShellBackend *shell_backend;
	EMailFolderTweaks *tweaks;
	GList *list, *link;
	const gchar *extension_name;
	gchar *old_uri;
	gchar *new_uri;
	gint ii;

	const gchar *cachenames[] = {
		"views/current_view-",
		"views/custom_view-"
	};

	/* Check whether the renamed folder was a designated Drafts or
	 * Sent folder for any mail account, and if so update the setting
	 * to the new folder name. */

	shell_backend = E_SHELL_BACKEND (backend);
	shell = e_shell_backend_get_shell (shell_backend);
	registry = e_shell_get_registry (shell);

	class = CAMEL_STORE_GET_CLASS (store);
	g_return_if_fail (class->equal_folder_name != NULL);

	old_uri = e_mail_folder_uri_build (store, old_folder_name);
	new_uri = e_mail_folder_uri_build (store, new_folder_name);

	extension_name = E_SOURCE_EXTENSION_MAIL_COMPOSITION;
	list = e_source_registry_list_sources (registry, extension_name);

	for (link = list; link != NULL; link = g_list_next (link)) {
		ESource *source = E_SOURCE (link->data);
		ESourceExtension *extension;
		const gchar *drafts_folder_uri;
		gboolean need_update;

		extension = e_source_get_extension (source, extension_name);

		drafts_folder_uri =
			e_source_mail_composition_get_drafts_folder (
			E_SOURCE_MAIL_COMPOSITION (extension));

		need_update =
			(drafts_folder_uri != NULL) &&
			class->equal_folder_name (drafts_folder_uri, old_uri);

		if (need_update) {
			GError *error = NULL;

			e_source_mail_composition_set_drafts_folder (
				E_SOURCE_MAIL_COMPOSITION (extension),
				new_uri);

			/* FIXME This is a blocking D-Bus method call. */
			if (!e_source_write_sync (source, NULL, &error)) {
				g_warning ("%s", error->message);
				g_error_free (error);
			}
		}
	}

	g_list_free_full (list, (GDestroyNotify) g_object_unref);

	extension_name = E_SOURCE_EXTENSION_MAIL_SUBMISSION;
	list = e_source_registry_list_sources (registry, extension_name);

	for (link = list; link != NULL; link = g_list_next (link)) {
		ESource *source = E_SOURCE (link->data);
		ESourceMailSubmission *extension;
		const gchar *sent_folder_uri;
		gboolean need_update;

		extension = e_source_get_extension (source, extension_name);

		sent_folder_uri = e_source_mail_submission_get_sent_folder (extension);

		need_update =
			(sent_folder_uri != NULL) &&
			class->equal_folder_name (sent_folder_uri, old_uri);

		if (need_update) {
			GError *error = NULL;

			e_source_mail_submission_set_sent_folder (extension, new_uri);

			/* FIXME This is a blocking D-Bus method call. */
			if (!e_source_write_sync (source, NULL, &error)) {
				g_warning ("%s", error->message);
				g_error_free (error);
			}
		}
	}

	g_list_free_full (list, (GDestroyNotify) g_object_unref);

	/* Rename GalView files. */

	for (ii = 0; ii < G_N_ELEMENTS (cachenames); ii++) {
		gchar *oldname;
		gchar *newname;

		oldname = mail_backend_uri_to_evname (old_uri, cachenames[ii]);
		newname = mail_backend_uri_to_evname (new_uri, cachenames[ii]);

		/* Ignore errors; doesn't matter. */
		if (g_rename (oldname, newname) == -1 && errno != ENOENT) {
			g_warning (
				"%s: Failed to rename '%s' to '%s': %s",
				G_STRFUNC,
				oldname, newname,
				g_strerror (errno));
		}

		g_free (oldname);
		g_free (newname);
	}

	tweaks = e_mail_folder_tweaks_new ();
	e_mail_folder_tweaks_folder_renamed (tweaks, old_uri, new_uri);
	g_clear_object (&tweaks);

	g_free (old_uri);
	g_free (new_uri);

	/* This does something completely different.
	 * XXX Make it a separate signal handler? */
	mail_filter_rename_folder (
		store, old_folder_name, new_folder_name);
}

static void
mail_backend_folder_changed_cb (MailFolderCache *folder_cache,
                                CamelStore *store,
                                const gchar *folder_name,
                                gint new_messages,
                                const gchar *msg_uid,
                                const gchar *msg_sender,
                                const gchar *msg_subject,
                                EMailBackend *mail_backend)
{
	EMEvent *event = em_event_peek ();
	EMEventTargetFolder *target;
	EMFolderTreeModel *model;
	CamelFolder *folder;
	gchar *folder_uri;
	gint folder_type;
	CamelFolderInfoFlags flags = 0;

	folder_uri = e_mail_folder_uri_build (store, folder_name);

	mail_folder_cache_get_folder_info_flags (
		folder_cache, store, folder_name, &flags);

	target = em_event_target_new_folder (
		event, store, folder_uri, new_messages,
		msg_uid, msg_sender, msg_subject);

	g_free (folder_uri);

	folder_type = (flags & CAMEL_FOLDER_TYPE_MASK);
	target->is_inbox = (folder_type == CAMEL_FOLDER_TYPE_INBOX);

	model = em_folder_tree_model_get_default ();
	target->display_name = em_folder_tree_model_get_folder_name (
		model, store, folder_name);

	folder = mail_folder_cache_ref_folder (folder_cache, store, folder_name);
	if (folder) {
		target->full_display_name = e_mail_folder_to_full_display_name (folder, NULL);
		g_clear_object (&folder);
	}

	if (target->new > 0) {
		EShell *shell;
		EShellBackend *shell_backend;

		shell_backend = E_SHELL_BACKEND (mail_backend);
		shell = e_shell_backend_get_shell (shell_backend);
		e_shell_event (shell, "mail-icon", (gpointer) "mail-unread");
	}

	/**
	 * @Event: folder.changed
	 * @Title: Folder changed
	 * @Target: EMEventTargetFolder
	 *
	 * folder.changed is emitted whenever a folder changes.  There is no
	 * detail on how the folder has changed.
	 *
	 * UPDATE: We tell the number of new UIDs added rather than the new
	 * mails received.
	 */
	e_event_emit (
		(EEvent *) event, "folder.changed",
		(EEventTarget *) target);
}

static void
mail_backend_folder_unread_updated_cb (MailFolderCache *folder_cache,
                                       CamelStore *store,
                                       const gchar *folder_name,
                                       gint unread_messages,
                                       EMailBackend *mail_backend)
{
	EMEvent *event = em_event_peek ();
	EMEventTargetFolderUnread *target;
	gchar *folder_uri;
	gint folder_type;
	CamelFolderInfoFlags flags = 0;

	folder_uri = e_mail_folder_uri_build (store, folder_name);

	mail_folder_cache_get_folder_info_flags (
		folder_cache, store, folder_name, &flags);

	target = em_event_target_new_folder_unread (
		event, store, folder_uri, unread_messages);

	g_free (folder_uri);

	folder_type = (flags & CAMEL_FOLDER_TYPE_MASK);
	target->is_inbox = (folder_type == CAMEL_FOLDER_TYPE_INBOX);

	/**
	 * @Event: folder.unread-updated
	 * @Title: Folder unread updated
	 * @Target: EMEventTargetFolder
	 *
	 * folder.unread-updated is emitted whenever the number of unread messages
	 * in a folder changes.
	 */
	e_event_emit (
		(EEvent *) event, "folder.unread-updated",
		(EEventTarget *) target);
}

static void
mail_backend_job_started_cb (CamelSession *session,
                             GCancellable *cancellable,
                             EShellBackend *shell_backend)
{
	EMailBackend *self;
	EActivity *activity;

	self = E_MAIL_BACKEND (shell_backend);

	activity = e_activity_new ();
	e_activity_set_cancellable (activity, cancellable);
	e_shell_backend_add_activity (shell_backend, activity);

	/* The hash table takes ownership of the activity. */
	g_hash_table_insert (self->priv->jobs, cancellable, activity);
}

static void
mail_backend_job_finished_cb (CamelSession *session,
                              GCancellable *cancellable,
                              const GError *error,
                              EShellBackend *shell_backend)
{
	EMailBackend *self;
	EShellBackendClass *class;
	EActivity *activity;
	const gchar *description;

	self = E_MAIL_BACKEND (shell_backend);
	class = E_SHELL_BACKEND_GET_CLASS (shell_backend);

	activity = g_hash_table_lookup (self->priv->jobs, cancellable);
	description = e_activity_get_text (activity);

	if (e_activity_handle_cancellation (activity, error)) {
		/* nothing to do */

	} else if (error != NULL) {
		EShell *shell;
		GtkApplication *application;
		GList *list, *iter;

		shell = e_shell_backend_get_shell (shell_backend);

		application = GTK_APPLICATION (shell);
		list = gtk_application_get_windows (application);

		/* Submit the error to an appropriate EAlertSink. */
		for (iter = list; iter != NULL; iter = g_list_next (iter)) {
			EShellView *shell_view;
			EShellContent *shell_content;
			gchar *tmp = NULL;

			if (!E_IS_SHELL_WINDOW (iter->data))
				continue;

			shell_view = e_shell_window_peek_shell_view (
				E_SHELL_WINDOW (iter->data), class->name);

			if (!E_IS_SHELL_VIEW (shell_view))
				continue;

			shell_content = e_shell_view_get_shell_content (shell_view);

			if (!description || !*description) {
				tmp = camel_operation_dup_message (cancellable);
				description = tmp;
			}

			if (description != NULL && *description != '\0')
				e_alert_submit (
					E_ALERT_SINK (shell_content),
					"mail:async-error", description,
					error->message, NULL);
			else
				e_alert_submit (
					E_ALERT_SINK (shell_content),
					"mail:async-error-nodescribe",
					error->message, NULL);

			g_free (tmp);

			break;
		}
	}

	g_hash_table_remove (self->priv->jobs, cancellable);
}

static void
mail_backend_allow_auth_prompt_cb (EMailSession *session,
				   ESource *source,
				   EShell *shell)
{
	g_return_if_fail (E_IS_SOURCE (source));
	g_return_if_fail (E_IS_SHELL (shell));

	e_shell_allow_auth_prompt_for (shell, source);
}

static void
mail_backend_connect_store_cb (EMailSession *session,
			       CamelStore *store,
			       gpointer user_data)
{
	EMailBackend *mail_backend = user_data;
	GCancellable *cancellable;
	EActivity *activity;
	GSettings *settings;
	gboolean with_send_recv;
	gchar *description;

	g_return_if_fail (E_IS_MAIL_SESSION (session));
	g_return_if_fail (E_IS_MAIL_BACKEND (mail_backend));
	g_return_if_fail (CAMEL_IS_STORE (store));

	settings = e_util_ref_settings ("org.gnome.evolution.mail");
	with_send_recv = g_settings_get_boolean (settings, "send-recv-on-start");
	g_object_unref (settings);

	cancellable = camel_operation_new ();
	description = g_strdup_printf (_("Reconnecting to “%s”"), camel_service_get_display_name (CAMEL_SERVICE (store)));

	activity = e_activity_new ();
	e_activity_set_cancellable (activity, cancellable);
	e_activity_set_text (activity, description);

	if (E_IS_MAIL_UI_SESSION (session))
		e_mail_ui_session_add_activity (E_MAIL_UI_SESSION (session), activity);

	e_mail_store_go_online (
		store, G_PRIORITY_DEFAULT,
		e_activity_get_cancellable (activity),
		(GAsyncReadyCallback) (with_send_recv ? mail_backend_store_go_online_done_cb : mail_backend_store_operation_done_cb),
		activity); /* Takes ownership of 'activity' */

	g_object_unref (cancellable);
	g_free (description);
}

static void
mail_backend_get_property (GObject *object,
                           guint property_id,
                           GValue *value,
                           GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_SESSION:
			g_value_set_object (
				value,
				e_mail_backend_get_session (
				E_MAIL_BACKEND (object)));
			return;

		case PROP_SEND_ACCOUNT_OVERRIDE:
			g_value_set_object (
				value,
				e_mail_backend_get_send_account_override (
				E_MAIL_BACKEND (object)));
			return;

		case PROP_REMOTE_CONTENT:
			g_value_set_object (
				value,
				e_mail_backend_get_remote_content (
				E_MAIL_BACKEND (object)));
			return;

		case PROP_MAIL_PROPERTIES:
			g_value_set_object (
				value,
				e_mail_backend_get_mail_properties (
				E_MAIL_BACKEND (object)));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
mail_backend_dispose (GObject *object)
{
	EMailBackend *self = E_MAIL_BACKEND (object);

	if (self->priv->session != NULL) {
		em_folder_tree_model_free_default ();

		g_signal_handlers_disconnect_matched (
			self->priv->session, G_SIGNAL_MATCH_DATA,
			0, 0, NULL, NULL, object);
		camel_session_remove_services (
			CAMEL_SESSION (self->priv->session));
		g_clear_object (&self->priv->session);
	}

	/* There should be no unfinished jobs left. */
	g_warn_if_fail (g_hash_table_size (self->priv->jobs) == 0);

	/* Chain up to parent's dispose() method. */
	G_OBJECT_CLASS (e_mail_backend_parent_class)->dispose (object);
}

static void
mail_backend_finalize (GObject *object)
{
	EMailBackend *self = E_MAIL_BACKEND (object);

	g_hash_table_destroy (self->priv->jobs);
	g_clear_object (&self->priv->send_account_override);
	g_clear_object (&self->priv->remote_content);
	g_clear_object (&self->priv->mail_properties);

	/* Chain up to parent's finalize() method. */
	G_OBJECT_CLASS (e_mail_backend_parent_class)->finalize (object);

	camel_shutdown ();
}

static void
mail_backend_add_store (EMailSession *session,
                        CamelStore *store,
                        EMailBackend *backend)
{
	EMFolderTreeModel *model;

	model = em_folder_tree_model_get_default ();
	em_folder_tree_model_add_store (model, store);
}

static void
mail_backend_remove_store (EMailSession *session,
                           CamelStore *store,
                           EMailBackend *backend)
{
	EMFolderTreeModel *model;

	model = em_folder_tree_model_get_default ();
	em_folder_tree_model_remove_store (model, store);
}

#define SET_ACTIVITY(cancellable, activity) \
	g_object_set_data (G_OBJECT (cancellable), "e-activity", activity)
#define GET_ACTIVITY(cancellable) \
        g_object_get_data (G_OBJECT (cancellable), "e-activity")

static void
mail_mt_create_activity (GCancellable *cancellable)
{
	EActivity *activity;

	activity = e_activity_new ();
	e_activity_set_percent (activity, 0.0);
	e_activity_set_cancellable (activity, cancellable);
	SET_ACTIVITY (cancellable, activity);
}

static void
mail_mt_submit_activity (GCancellable *cancellable)
{
	EShell *shell;
	EShellBackend *shell_backend;
	EActivity *activity;

	shell = e_shell_get_default ();
	shell_backend = e_shell_get_backend_by_name (
		shell, "mail");

	activity = GET_ACTIVITY (cancellable);
	if (activity)
		e_shell_backend_add_activity (shell_backend, activity);

}

static void
mail_mt_free_activity (GCancellable *cancellable)
{
	EActivity *activity = GET_ACTIVITY (cancellable);

	if (activity)
		g_object_unref (activity);
}

static void
mail_mt_complete_activity (GCancellable *cancellable)
{
	EActivity *activity = GET_ACTIVITY (cancellable);

	if (activity)
		e_activity_set_state (activity, E_ACTIVITY_COMPLETED);
}

static void
mail_mt_cancel_activity (GCancellable *cancellable)
{
	EActivity *activity = GET_ACTIVITY (cancellable);

	if (activity)
		e_activity_set_state (activity, E_ACTIVITY_CANCELLED);
}

static void
mail_mt_alert_error (GCancellable *cancellable,
                     const gchar *what,
                     const gchar *message)
{
	EShell *shell;
	EShellView *shell_view;
	EShellWindow *shell_window = NULL;
	EShellContent *shell_content;
	GList *list, *iter;
	GtkApplication *application;

	shell = e_shell_get_default ();
	application = GTK_APPLICATION (shell);
	list = gtk_application_get_windows (application);

	/* Find the most recently used EShellWindow. */
	for (iter = list; iter != NULL; iter = g_list_next (iter)) {
		if (E_IS_SHELL_WINDOW (iter->data)) {
			shell_window = E_SHELL_WINDOW (iter->data);
			break;
		}
	}

	/* If we can't find an EShellWindow then... well, screw it. */
	if (shell_window == NULL)
		return;

	shell_view = e_shell_window_get_shell_view (
		shell_window, "mail");
	shell_content = e_shell_view_get_shell_content (shell_view);

	if (what) {
		e_alert_submit (
			E_ALERT_SINK (shell_content),
			"mail:async-error", what,
			message, NULL);
	} else
		e_alert_submit (
			E_ALERT_SINK (shell_content),
			"mail:async-error-nodescribe",
			message, NULL);
}

static EAlertSink *
mail_mt_get_alert_sink (void)
{
	EShell *shell;
	EShellBackend *shell_backend;

	shell = e_shell_get_default ();
	shell_backend = e_shell_get_backend_by_name (
		shell, "mail");

	return e_mail_backend_get_alert_sink (E_MAIL_BACKEND (shell_backend));
}

static void
mail_backend_constructed (GObject *object)
{
	EMailBackend *self = E_MAIL_BACKEND (object);
	EShell *shell;
	EShellBackend *shell_backend;
	MailFolderCache *folder_cache;
	ESourceRegistry *registry;
	gchar *config_filename;
	GList *providers;

	shell_backend = E_SHELL_BACKEND (object);
	shell = e_shell_backend_get_shell (shell_backend);

	if (camel_init (e_get_user_data_dir (), TRUE) != 0)
		exit (0);

	providers = camel_provider_list (TRUE);
	if (!providers) {
		g_warning ("%s: No camel providers loaded, exiting...", G_STRFUNC);
		exit (1);
	}

	g_list_free (providers);

	registry = e_shell_get_registry (shell);
	self->priv->session = e_mail_ui_session_new (registry);

	g_signal_connect (
		self->priv->session, "allow-auth-prompt",
		G_CALLBACK (mail_backend_allow_auth_prompt_cb), shell);

	g_signal_connect (
		self->priv->session, "flush-outbox",
		G_CALLBACK (mail_send), self->priv->session);

	g_signal_connect (
		self->priv->session, "connect-store",
		G_CALLBACK (mail_backend_connect_store_cb), object);

	/* Propagate "activity-added" signals from
	 * the mail session to the shell backend. */
	g_signal_connect_swapped (
		self->priv->session, "activity-added",
		G_CALLBACK (e_shell_backend_add_activity),
		shell_backend);

	g_signal_connect (
		self->priv->session, "job-started",
		G_CALLBACK (mail_backend_job_started_cb),
		shell_backend);

	g_signal_connect (
		self->priv->session, "job-finished",
		G_CALLBACK (mail_backend_job_finished_cb),
		shell_backend);

	g_signal_connect (
		self->priv->session, "store-added",
		G_CALLBACK (mail_backend_add_store),
		shell_backend);

	g_signal_connect (
		self->priv->session, "store-removed",
		G_CALLBACK (mail_backend_remove_store),
		shell_backend);

	g_signal_connect (
		shell, "prepare-for-offline",
		G_CALLBACK (mail_backend_prepare_for_offline_cb),
		shell_backend);

	g_signal_connect (
		shell, "prepare-for-online",
		G_CALLBACK (mail_backend_prepare_for_online_cb),
		shell_backend);

	g_signal_connect (
		shell, "prepare-for-quit",
		G_CALLBACK (mail_backend_prepare_for_quit_cb),
		shell_backend);

	g_signal_connect (
		shell, "quit-requested",
		G_CALLBACK (mail_backend_quit_requested_cb),
		shell_backend);

	folder_cache = e_mail_session_get_folder_cache (self->priv->session);

	g_signal_connect (
		folder_cache, "folder-deleted",
		G_CALLBACK (mail_backend_folder_deleted_cb),
		shell_backend);

	g_signal_connect (
		folder_cache, "folder-renamed",
		G_CALLBACK (mail_backend_folder_renamed_cb),
		shell_backend);

	g_signal_connect (
		folder_cache, "folder-changed",
		G_CALLBACK (mail_backend_folder_changed_cb), shell_backend);

	g_signal_connect (
		folder_cache, "folder-unread-updated",
		G_CALLBACK (mail_backend_folder_unread_updated_cb),
		shell_backend);

	mail_config_init (self->priv->session);

	mail_msg_register_activities (
		mail_mt_create_activity,
		mail_mt_submit_activity,
		mail_mt_free_activity,
		mail_mt_complete_activity,
		mail_mt_cancel_activity,
		mail_mt_alert_error,
		mail_mt_get_alert_sink);

	/* Chain up to parent's constructed() method. */
	G_OBJECT_CLASS (e_mail_backend_parent_class)->constructed (object);

	config_filename = g_build_filename (e_shell_backend_get_config_dir (shell_backend), "send-overrides.ini", NULL);
	self->priv->send_account_override = e_mail_send_account_override_new (config_filename);
	g_free (config_filename);

	config_filename = g_build_filename (e_shell_backend_get_config_dir (shell_backend), "remote-content.db", NULL);
	self->priv->remote_content = e_mail_remote_content_new (config_filename);
	g_free (config_filename);

	config_filename = g_build_filename (e_shell_backend_get_config_dir (shell_backend), "properties.db", NULL);
	self->priv->mail_properties = e_mail_properties_new (config_filename);
	g_free (config_filename);
}

static void
e_mail_backend_class_init (EMailBackendClass *class)
{
	GObjectClass *object_class;
	EShellBackendClass *shell_backend_class;

	object_class = G_OBJECT_CLASS (class);
	object_class->get_property = mail_backend_get_property;
	object_class->dispose = mail_backend_dispose;
	object_class->finalize = mail_backend_finalize;
	object_class->constructed = mail_backend_constructed;

	shell_backend_class = E_SHELL_BACKEND_CLASS (class);
	shell_backend_class->migrate = e_mail_migrate;
	shell_backend_class->get_data_dir = mail_shell_backend_get_data_dir;
	shell_backend_class->get_config_dir = mail_shell_backend_get_config_dir;

	g_object_class_install_property (
		object_class,
		PROP_SESSION,
		g_param_spec_object (
			"session",
			NULL,
			NULL,
			E_TYPE_MAIL_SESSION,
			G_PARAM_READABLE));

	g_object_class_install_property (
		object_class,
		PROP_SEND_ACCOUNT_OVERRIDE,
		g_param_spec_object (
			"send-account-override",
			NULL,
			NULL,
			E_TYPE_MAIL_SEND_ACCOUNT_OVERRIDE,
			G_PARAM_READABLE));

	g_object_class_install_property (
		object_class,
		PROP_REMOTE_CONTENT,
		g_param_spec_object (
			"remote-content",
			NULL,
			NULL,
			E_TYPE_MAIL_REMOTE_CONTENT,
			G_PARAM_READABLE));

	g_object_class_install_property (
		object_class,
		PROP_MAIL_PROPERTIES,
		g_param_spec_object (
			"mail-properties",
			NULL,
			NULL,
			E_TYPE_MAIL_PROPERTIES,
			G_PARAM_READABLE));
}

static void
e_mail_backend_init (EMailBackend *backend)
{
	backend->priv = e_mail_backend_get_instance_private (backend);

	backend->priv->jobs = g_hash_table_new_full (
		(GHashFunc) g_direct_hash,
		(GEqualFunc) g_direct_equal,
		(GDestroyNotify) NULL,
		(GDestroyNotify) g_object_unref);
}

EMailSession *
e_mail_backend_get_session (EMailBackend *backend)
{
	g_return_val_if_fail (E_IS_MAIL_BACKEND (backend), NULL);

	return backend->priv->session;
}

EAlertSink *
e_mail_backend_get_alert_sink (EMailBackend *backend)
{
	EShell *shell;
	EShellView *shell_view;
	EShellBackend *shell_backend;
	EShellContent *shell_content;
	EShellWindow *shell_window = NULL;
	EShellBackendClass *class;
	GtkApplication *application;
	GList *list, *link;

	/* XXX This is meant to be a convenient but temporary hack.
	 *     It digs through the list of available EShellWindows
	 *     to find a suitable EAlertSink. */

	g_return_val_if_fail (E_IS_MAIL_BACKEND (backend), NULL);

	shell_backend = E_SHELL_BACKEND (backend);
	shell = e_shell_backend_get_shell (shell_backend);

	application = GTK_APPLICATION (shell);
	list = gtk_application_get_windows (application);

	/* Find the most recently used EShellWindow. */
	for (link = list; link != NULL; link = g_list_next (link)) {
		if (E_IS_SHELL_WINDOW (link->data)) {
			shell_window = E_SHELL_WINDOW (link->data);
			break;
		}
	}

	g_return_val_if_fail (shell_window != NULL, NULL);

	class = E_SHELL_BACKEND_GET_CLASS (shell_backend);
	shell_view = e_shell_window_get_shell_view (shell_window, class->name);
	shell_content = e_shell_view_get_shell_content (shell_view);

	return E_ALERT_SINK (shell_content);
}

gboolean
e_mail_backend_delete_junk_policy_decision (EMailBackend *backend)
{
	EMailBackendClass *class;

	g_return_val_if_fail (E_IS_MAIL_BACKEND (backend), FALSE);

	class = E_MAIL_BACKEND_GET_CLASS (backend);
	g_return_val_if_fail (class != NULL, FALSE);

	if (class->delete_junk_policy_decision == NULL)
		return FALSE;

	return class->delete_junk_policy_decision (backend);
}

gboolean
e_mail_backend_empty_trash_policy_decision (EMailBackend *backend)
{
	EMailBackendClass *class;

	g_return_val_if_fail (E_IS_MAIL_BACKEND (backend), FALSE);

	class = E_MAIL_BACKEND_GET_CLASS (backend);
	g_return_val_if_fail (class != NULL, FALSE);

	if (class->empty_trash_policy_decision == NULL)
		return FALSE;

	return class->empty_trash_policy_decision (backend);
}

EMailSendAccountOverride *
e_mail_backend_get_send_account_override (EMailBackend *backend)
{
	g_return_val_if_fail (E_IS_MAIL_BACKEND (backend), NULL);

	return backend->priv->send_account_override;
}

EMailRemoteContent *
e_mail_backend_get_remote_content (EMailBackend *backend)
{
	g_return_val_if_fail (E_IS_MAIL_BACKEND (backend), NULL);

	return backend->priv->remote_content;
}

EMailProperties *
e_mail_backend_get_mail_properties (EMailBackend *backend)
{
	g_return_val_if_fail (E_IS_MAIL_BACKEND (backend), NULL);

	return backend->priv->mail_properties;
}
