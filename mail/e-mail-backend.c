/*
 * e-mail-backend.c
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) version 3.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with the program; if not, see <http://www.gnu.org/licenses/>
 *
 * Authors:
 *   Jonathon Jongsma <jonathon.jongsma@collabora.co.uk>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 * Copyright (C) 2009 Intel Corporation
 *
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "e-mail-backend.h"

#include <string.h>
#include <libedataserver/e-data-server-util.h>

#include "e-util/e-account-utils.h"
#include "e-util/e-alert-dialog.h"
#include "e-util/e-alert-sink.h"

#include "misc/e-account-combo-box.h"

#include "shell/e-shell.h"

#include "mail/e-mail-folder-utils.h"
#include "mail/e-mail-local.h"
#include "mail/e-mail-migrate.h"
#include "mail/e-mail-session.h"
#include "mail/e-mail-store.h"
#include "mail/e-mail-store-utils.h"
#include "mail/em-event.h"
#include "mail/em-folder-tree-model.h"
#include "mail/em-utils.h"
#include "mail/mail-autofilter.h"
#include "mail/mail-config.h"
#include "mail/mail-folder-cache.h"
#include "mail/mail-ops.h"
#include "mail/mail-vfolder.h"

#define E_MAIL_BACKEND_GET_PRIVATE(obj) \
	(G_TYPE_INSTANCE_GET_PRIVATE \
	((obj), E_TYPE_MAIL_BACKEND, EMailBackendPrivate))

#define QUIT_POLL_INTERVAL 1  /* seconds */

struct _EMailBackendPrivate {
	EMailSession *session;
	GHashTable *jobs;
};

enum {
	PROP_0,
	PROP_SESSION
};

/* FIXME Kill this thing.  It's a horrible hack. */
extern gint camel_application_is_exiting;

G_DEFINE_ABSTRACT_TYPE (
	EMailBackend,
	e_mail_backend,
	E_TYPE_SHELL_BACKEND)

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

/* Helper for mail_backend_prepare_for_offline_cb() */
static void
mail_store_prepare_for_offline_cb (CamelService *service,
                                   gpointer unused,
                                   EActivity *activity)
{
	/* FIXME Not passing a GCancellable. */
	e_mail_store_go_offline (
		CAMEL_STORE (service), G_PRIORITY_DEFAULT, NULL,
		(GAsyncReadyCallback) mail_backend_store_operation_done_cb,
		g_object_ref (activity));
}

static void
mail_backend_prepare_for_offline_cb (EShell *shell,
                                     EActivity *activity,
                                     EMailBackend *backend)
{
	GtkWindow *window;
	EMailSession *session;
	gboolean synchronize = FALSE;

	window = e_shell_get_active_window (shell);
	session = e_mail_backend_get_session (backend);

	if (e_shell_get_network_available (shell) &&
		e_shell_backend_is_started (E_SHELL_BACKEND (backend)))
		synchronize = em_utils_prompt_user (
			window, NULL, "mail:ask-quick-offline", NULL);

	if (!synchronize) {
		mail_cancel_all ();
		camel_session_set_network_available (
			CAMEL_SESSION (session), FALSE);
	}

	e_mail_store_foreach (
		(GHFunc) mail_store_prepare_for_offline_cb, activity);
}

/* Helper for mail_backend_prepare_for_online_cb() */
static void
mail_store_prepare_for_online_cb (CamelService *service,
                                  gpointer unused,
                                  EActivity *activity)
{
	/* FIXME Not passing a GCancellable. */
	e_mail_store_go_online (
		CAMEL_STORE (service), G_PRIORITY_DEFAULT, NULL,
		(GAsyncReadyCallback) mail_backend_store_operation_done_cb,
		g_object_ref (activity));
}

static void
mail_backend_prepare_for_online_cb (EShell *shell,
                                    EActivity *activity,
                                    EMailBackend *backend)
{
	EMailSession *session;

	session = e_mail_backend_get_session (backend);
	camel_session_set_online (CAMEL_SESSION (session), TRUE);

	e_mail_store_foreach (
		(GHFunc) mail_store_prepare_for_online_cb, activity);
}

/* Helper for mail_backend_prepare_for_quit_cb() */
static void
mail_backend_delete_junk (CamelStore *store,
                          gpointer unused,
                          EMailBackend *backend)
{
	CamelFolder *folder;
	GPtrArray *uids;
	guint32 flags;
	guint32 mask;
	guint ii;

	/* FIXME camel_store_get_junk_folder_sync() may block. */
	folder = camel_store_get_junk_folder_sync (store, NULL, NULL);
	if (folder == NULL)
		return;

	uids = camel_folder_get_uids (folder);
	flags = mask = CAMEL_MESSAGE_DELETED | CAMEL_MESSAGE_SEEN;

	camel_folder_freeze (folder);

	for (ii = 0; ii < uids->len; ii++) {
		const gchar *uid = uids->pdata[ii];
		camel_folder_set_message_flags (folder, uid, flags, mask);
	}

	camel_folder_thaw (folder);

	camel_folder_free_uids (folder, uids);
}

/* Helper for mail_backend_prepare_for_quit_cb() */
static void
mail_backend_final_sync (CamelStore *store,
                         gpointer unused,
                         gpointer user_data)
{
	struct {
		EActivity *activity;
		gboolean empty_trash;
	} *sync_data = user_data;

	/* FIXME Not passing a GCancellable. */
	/* FIXME This operation should be queued. */
	camel_store_synchronize (
		store, sync_data->empty_trash, G_PRIORITY_DEFAULT, NULL,
		(GAsyncReadyCallback) mail_backend_store_operation_done_cb,
		g_object_ref (sync_data->activity));
}

/* Helper for mail_backend_prepare_for_quit_cb() */
static gboolean
mail_backend_poll_to_quit (EActivity *activity)
{
	return mail_msg_active ();
}

/* Helper for mail_backend_prepare_for_quit_cb() */
static void
mail_backend_ready_to_quit (EActivity *activity)
{
	emu_free_mail_cache ();

	/* Do this last.  It may terminate the process. */
	g_object_unref (activity);
}

static void
mail_backend_prepare_for_quit_cb (EShell *shell,
                                  EActivity *activity,
                                  EMailBackend *backend)
{
	EAccountList *account_list;
	gboolean delete_junk;
	gboolean empty_trash;

	struct {
		EActivity *activity;
		gboolean empty_trash;
	} sync_data;

	delete_junk = e_mail_backend_delete_junk_policy_decision (backend);
	empty_trash = e_mail_backend_empty_trash_policy_decision (backend);

	camel_application_is_exiting = TRUE;

	account_list = e_get_account_list ();
	e_account_list_prune_proxies (account_list);

	mail_vfolder_shutdown ();

	/* Cancel all pending activities. */
	mail_cancel_all ();

	if (delete_junk)
		e_mail_store_foreach (
			(GHFunc) mail_backend_delete_junk, backend);

	sync_data.activity = activity;
	sync_data.empty_trash = empty_trash;

	e_mail_store_foreach ((GHFunc) mail_backend_final_sync, &sync_data);

	/* Now we poll until all activities are actually cancelled or finished.
	 * Reffing the activity delays quitting; the reference count
	 * acts like a counting semaphore. */
	if (mail_msg_active ())
		g_timeout_add_seconds_full (
			G_PRIORITY_DEFAULT, QUIT_POLL_INTERVAL,
			(GSourceFunc) mail_backend_poll_to_quit,
			g_object_ref (activity),
			(GDestroyNotify) mail_backend_ready_to_quit);
	else
		mail_backend_ready_to_quit (g_object_ref (activity));
}

static void
mail_backend_quit_requested_cb (EShell *shell,
                                EShellQuitReason reason,
                                EShellBackend *mail_shell_backend)
{
	CamelFolder *folder;
	GtkWindow *window;
	gint response;

	window = e_shell_get_active_window (shell);

	/* We can quit immediately if offline. */
	if (!e_shell_get_online (shell))
		return;

	/* Or if another Evolution process asked us to. */
	if (reason == E_SHELL_QUIT_REMOTE_REQUEST)
		return;

	/* In express mode, don't raise mail request in non mail window. */
	if (e_shell_get_express_mode (shell) &&
		strcmp(e_shell_window_get_active_view((EShellWindow *)window), "mail") != 0)
		return;

	if (!e_shell_backend_is_started (mail_shell_backend))
		return;

	/* Check Outbox for any unsent messages. */

	folder = e_mail_local_get_folder (E_MAIL_LOCAL_FOLDER_OUTBOX);
	if (folder == NULL)
		return;

	if (folder->summary->visible_count == 0)
		return;

	response = e_alert_run_dialog_for_args (
		window, "mail:exit-unsaved", NULL);

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
	mail_filter_delete_folder (backend, store, folder_name);
}

static void
mail_backend_folder_renamed_cb (MailFolderCache *folder_cache,
                                CamelStore *store,
                                const gchar *old_folder_name,
                                const gchar *new_folder_name,
                                EMailBackend *backend)
{
	mail_filter_rename_folder (
		backend, store, old_folder_name, new_folder_name);
}

static void
mail_backend_folder_changed_cb (MailFolderCache *folder_cache,
                                CamelStore *store,
                                const gchar *folder_name,
                                gint new_messages,
                                const gchar *msg_uid,
                                const gchar *msg_sender,
                                const gchar *msg_subject,
                                EShell *shell)
{
	CamelFolder *folder = NULL;
	EMEvent *event = em_event_peek ();
	EMEventTargetFolder *target;
	EMFolderTreeModel *model;
	gchar *folder_uri;
	gint folder_type;
	CamelFolderInfoFlags flags = 0;

	folder_uri = e_mail_folder_uri_build (store, folder_name);

	if (mail_folder_cache_get_folder_from_uri (
			folder_cache, folder_uri, &folder))
		if (folder && !mail_folder_cache_get_folder_info_flags (
				folder_cache, folder, &flags))
			g_return_if_reached ();

	g_free (folder_uri);

	target = em_event_target_new_folder (
		event, store, folder_name, new_messages,
		msg_uid, msg_sender, msg_subject);

	folder_type = (flags & CAMEL_FOLDER_TYPE_MASK);
	target->is_inbox = (folder_type == CAMEL_FOLDER_TYPE_INBOX);

	model = em_folder_tree_model_get_default ();
	target->display_name = em_folder_tree_model_get_folder_name (
		model, store, folder_name);

	if (target->new > 0)
		e_shell_event (shell, "mail-icon", (gpointer) "mail-unread");

	/** @Event: folder.changed
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
mail_backend_job_started_cb (CamelSession *session,
                             GCancellable *cancellable,
                             EShellBackend *shell_backend)
{
	EMailBackendPrivate *priv;
	EActivity *activity;

	priv = E_MAIL_BACKEND_GET_PRIVATE (shell_backend);

	activity = e_activity_new ();
	e_activity_set_cancellable (activity, cancellable);
	e_shell_backend_add_activity (shell_backend, activity);

	/* The hash table takes ownership of the activity. */
	g_hash_table_insert (priv->jobs, cancellable, activity);
}

static void
mail_backend_job_finished_cb (CamelSession *session,
                              GCancellable *cancellable,
                              const GError *error,
                              EShellBackend *shell_backend)
{
	EMailBackendPrivate *priv;
	EShellBackendClass *class;
	EActivity *activity;
	const gchar *description;

	priv = E_MAIL_BACKEND_GET_PRIVATE (shell_backend);
	class = E_SHELL_BACKEND_GET_CLASS (shell_backend);

	activity = g_hash_table_lookup (priv->jobs, cancellable);
	description = e_activity_get_text (activity);

	if (e_activity_handle_cancellation (activity, error)) {
		/* nothing to do */

	} else if (error != NULL) {
		EShell *shell;
		GList *list, *iter;

		shell = e_shell_backend_get_shell (shell_backend);
		list = e_shell_get_watched_windows (shell);

		/* Submit the error to an appropriate EAlertSink. */
		for (iter = list; iter != NULL; iter = g_list_next (iter)) {
			EShellView *shell_view;
			EShellContent *shell_content;

			if (!E_IS_SHELL_WINDOW (iter->data))
				continue;

			shell_view = e_shell_window_peek_shell_view (
				E_SHELL_WINDOW (iter->data), class->name);

			if (!E_IS_SHELL_VIEW (shell_view))
				continue;

			shell_content =
				e_shell_view_get_shell_content (shell_view);

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

			break;
		}
	}

	g_hash_table_remove (priv->jobs, cancellable);
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
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
mail_backend_dispose (GObject *object)
{
	EMailBackendPrivate *priv;

	priv = E_MAIL_BACKEND_GET_PRIVATE (object);

	if (priv->session != NULL) {
		g_signal_handlers_disconnect_matched (
			priv->session, G_SIGNAL_MATCH_DATA,
			0, 0, NULL, NULL, object);
		g_object_unref (priv->session);
		priv->session = NULL;
	}

	/* There should be no unfinished jobs left. */
	g_warn_if_fail (g_hash_table_size (priv->jobs) == 0);

	/* Chain up to parent's dispose() method. */
	G_OBJECT_CLASS (e_mail_backend_parent_class)->dispose (object);
}

static void
mail_backend_finalize (GObject *object)
{
	EMailBackendPrivate *priv;

	priv = E_MAIL_BACKEND_GET_PRIVATE (object);

	g_hash_table_destroy (priv->jobs);

	/* Chain up to parent's finalize() method. */
	G_OBJECT_CLASS (e_mail_backend_parent_class)->finalize (object);

	camel_shutdown ();
}

static void
mail_backend_constructed (GObject *object)
{
	EMailBackendPrivate *priv;
	EShell *shell;
	EShellBackend *shell_backend;
	EMFolderTreeModel *folder_tree_model;
	MailFolderCache *folder_cache;

	priv = E_MAIL_BACKEND_GET_PRIVATE (object);

	shell_backend = E_SHELL_BACKEND (object);
	shell = e_shell_backend_get_shell (shell_backend);

	if (camel_init (e_get_user_data_dir (), TRUE) != 0)
		exit (0);

	camel_provider_init ();

	priv->session = e_mail_session_new ();
	folder_cache = e_mail_session_get_folder_cache (priv->session);

	g_object_bind_property (
		shell, "online",
		priv->session, "online",
		G_BINDING_SYNC_CREATE);

	g_signal_connect (
		priv->session, "job-started",
		G_CALLBACK (mail_backend_job_started_cb),
		shell_backend);

	g_signal_connect (
		priv->session, "job-finished",
		G_CALLBACK (mail_backend_job_finished_cb),
		shell_backend);

	/* FIXME This is an evil hack that needs to die.
	 *       Give EAccountComboBox a CamelSession property. */
	e_account_combo_box_set_session (CAMEL_SESSION (priv->session));

	/* FIXME EMailBackend should own the default EMFolderTreeModel. */
	folder_tree_model = em_folder_tree_model_get_default ();
	em_folder_tree_model_set_session (folder_tree_model, priv->session);

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
		G_CALLBACK (mail_backend_folder_changed_cb), shell);

	mail_config_init (priv->session);
	mail_msg_init ();

	/* Chain up to parent's constructed() method. */
	G_OBJECT_CLASS (e_mail_backend_parent_class)->constructed (object);
}

static void
e_mail_backend_class_init (EMailBackendClass *class)
{
	GObjectClass *object_class;
	EShellBackendClass *shell_backend_class;

	g_type_class_add_private (class, sizeof (EMailBackendPrivate));

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
}

static void
e_mail_backend_init (EMailBackend *backend)
{
	backend->priv = E_MAIL_BACKEND_GET_PRIVATE (backend);

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

gboolean
e_mail_backend_delete_junk_policy_decision (EMailBackend *backend)
{
	EMailBackendClass *class;

	g_return_val_if_fail (E_IS_MAIL_BACKEND (backend), FALSE);

	class = E_MAIL_BACKEND_GET_CLASS (backend);
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
	if (class->empty_trash_policy_decision == NULL)
		return FALSE;

	return class->empty_trash_policy_decision (backend);
}

void
e_mail_backend_submit_alert (EMailBackend *backend,
                             const gchar *tag,
                             ...)
{
	EShell *shell;
	EShellView *shell_view;
	EShellBackend *shell_backend;
	EShellContent *shell_content;
	EShellWindow *shell_window = NULL;
	EShellBackendClass *class;
	GList *list, *iter;
	va_list va;

	/* XXX This is meant to be a convenient but temporary hack.
	 *     Instead, pass alerts directly to an EShellContent.
	 *     Perhaps even take an EAlert** instead of a GError**
	 *     in some low-level functions. */

	g_return_if_fail (E_IS_MAIL_BACKEND (backend));
	g_return_if_fail (tag != NULL);

	shell_backend = E_SHELL_BACKEND (backend);
	shell = e_shell_backend_get_shell (shell_backend);

	/* Find the most recently used EShellWindow. */
	list = e_shell_get_watched_windows (shell);
	for (iter = list; iter != NULL; iter = g_list_next (iter)) {
		if (E_IS_SHELL_WINDOW (iter->data)) {
			shell_window = E_SHELL_WINDOW (iter->data);
			break;
		}
	}

	/* If we can't find an EShellWindow then... well, screw it. */
	if (shell_window == NULL)
		return;

	class = E_SHELL_BACKEND_GET_CLASS (shell_backend);
	shell_view = e_shell_window_get_shell_view (shell_window, class->name);
	shell_content = e_shell_view_get_shell_content (shell_view);

	va_start (va, tag);
	e_alert_submit_valist (E_ALERT_SINK (shell_content), tag, va);
	va_end (va);
}
