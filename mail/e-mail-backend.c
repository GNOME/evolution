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
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#include "e-mail-backend.h"

#include <camel/camel.h>

#include "e-util/e-account-utils.h"
#include "e-util/e-alert-dialog.h"

#include "shell/e-shell.h"

#include "mail/e-mail-local.h"
#include "mail/e-mail-migrate.h"
#include "mail/e-mail-store.h"
#include "mail/em-utils.h"
#include "mail/mail-ops.h"
#include "mail/mail-session.h"
#include "mail/mail-vfolder.h"

#define E_MAIL_BACKEND_GET_PRIVATE(obj) \
	(G_TYPE_INSTANCE_GET_PRIVATE \
	((obj), E_TYPE_MAIL_BACKEND, EMailBackendPrivate))

#define QUIT_POLL_INTERVAL 1  /* seconds */

struct _EMailBackendPrivate {
	gint placeholder;  /* for future expansion */
};

static gpointer parent_class;

/* FIXME Kill this thing.  It's a horrible hack. */
extern gint camel_application_is_exiting;

/* Callback for various asynchronous CamelStore operations where
 * the EActivity's reference count is used as a counting semaphore. */
static void
mail_backend_store_operation_done_cb (CamelStore *store,
                                      gpointer user_data)
{
	g_object_unref (E_ACTIVITY (user_data));
}

static void
mail_backend_notify_online_cb (EShell *shell,
                               GParamSpec *pspec,
                               EMailBackend *backend)
{
	gboolean online;

	online = e_shell_get_online (shell);
	camel_session_set_online (session, online);
}

/* Helper for mail_backend_prepare_for_offline_cb() */
static void
mail_store_prepare_for_offline_cb (CamelService *service,
                                   gpointer unused,
                                   EActivity *activity)
{
	if (CAMEL_IS_DISCO_STORE (service) || CAMEL_IS_OFFLINE_STORE (service))
		mail_store_set_offline (
			CAMEL_STORE (service), TRUE,
			mail_backend_store_operation_done_cb,
			g_object_ref (activity));
}

static void
mail_backend_prepare_for_offline_cb (EShell *shell,
                                     EActivity *activity,
                                     EMailBackend *backend)
{
	GtkWindow *window;
	gboolean synchronize = FALSE;

	window = e_shell_get_active_window (shell);

	if (e_shell_get_network_available (shell))
		synchronize = em_utils_prompt_user (
			window, NULL, "mail:ask-quick-offline", NULL);

	if (!synchronize) {
		mail_cancel_all ();
		camel_session_set_network_state (session, FALSE);
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
	if (CAMEL_IS_DISCO_STORE (service) || CAMEL_IS_OFFLINE_STORE (service))
		mail_store_set_offline (
			CAMEL_STORE (service), FALSE,
			mail_backend_store_operation_done_cb,
			g_object_ref (activity));
}

static void
mail_backend_prepare_for_online_cb (EShell *shell,
                                    EActivity *activity,
                                    EMailBackend *backend)
{
	camel_session_set_online (session, TRUE);

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

	folder = camel_store_get_junk (store, NULL);
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

	/* Reffing the activity delays quitting; the reference count
	 * acts like a counting semaphore. */
	mail_sync_store (
		store, sync_data->empty_trash,
		mail_backend_store_operation_done_cb,
		g_object_ref (sync_data->activity));
}

/* Helper for mail_backend_prepare_for_quit_cb() */
static gboolean
mail_backend_poll_to_quit (EActivity *activity)
{
	return mail_msg_active ((guint) -1);
}

/* Helper for mail_backend_prepare_for_quit_cb() */
static void
mail_backend_ready_to_quit (EActivity *activity)
{
	mail_session_shutdown ();
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

	if (delete_junk)
		e_mail_store_foreach (
			(GHFunc) mail_backend_delete_junk, backend);

	sync_data.activity = activity;
	sync_data.empty_trash = empty_trash;

	e_mail_store_foreach ((GHFunc) mail_backend_final_sync, &sync_data);

	/* Cancel all activities. */
	mail_cancel_all ();

	/* Now we poll until all activities are actually cancelled.
	 * Reffing the activity delays quitting; the reference count
	 * acts like a counting semaphore. */
	if (mail_msg_active ((guint) -1))
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
                                EShellBackend *shell_backend)
{
	CamelFolder *folder;
	GtkWindow *window;
	guint32 unsent;
	gint response;

	window = e_shell_get_active_window (shell);

	/* We can quit immediately if offline. */
	if (!e_shell_get_online (shell))
		return;

	/* Check Outbox for any unsent messages. */

	folder = e_mail_local_get_folder (E_MAIL_FOLDER_OUTBOX);
	if (folder == NULL)
		return;

	if (camel_object_get (
		folder, NULL, CAMEL_FOLDER_VISIBLE, &unsent, 0) != 0)
		return;

	if (unsent == 0)
		return;

	response = e_alert_run_dialog_for_args (
		window, "mail:exit-unsaved", NULL);

	if (response == GTK_RESPONSE_YES)
		return;

	e_shell_cancel_quit (shell);
}

static void
mail_backend_constructed (GObject *object)
{
	EShell *shell;
	EShellBackend *shell_backend;
	const gchar *data_dir;

	shell_backend = E_SHELL_BACKEND (object);
	shell = e_shell_backend_get_shell (shell_backend);

	/* This also initializes Camel, so it needs to happen early. */
	mail_session_init (shell_backend);

	g_signal_connect (
		shell, "notify::online",
		G_CALLBACK (mail_backend_notify_online_cb),
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

	mail_config_init ();
	mail_msg_init ();

	data_dir = e_shell_backend_get_data_dir (shell_backend);
	e_mail_store_init (data_dir);
}

static void
mail_backend_class_init (EMailBackendClass *class)
{
	GObjectClass *object_class;
	EShellBackendClass *shell_backend_class;

	parent_class = g_type_class_peek_parent (class);
	g_type_class_add_private (class, sizeof (EMailBackendPrivate));

	object_class = G_OBJECT_CLASS (class);
	object_class->constructed = mail_backend_constructed;

	shell_backend_class = E_SHELL_BACKEND_CLASS (class);
	shell_backend_class->migrate = e_mail_migrate;
}

static void
mail_backend_init (EMailBackend *backend)
{
	backend->priv = E_MAIL_BACKEND_GET_PRIVATE (backend);
}

GType
e_mail_backend_get_type (void)
{
	static GType type = 0;

	if (G_UNLIKELY (type == 0)) {
		static const GTypeInfo type_info = {
			sizeof (EMailBackendClass),
			(GBaseInitFunc) NULL,
			(GBaseFinalizeFunc) NULL,
			(GClassInitFunc) mail_backend_class_init,
			(GClassFinalizeFunc) NULL,
			NULL,  /* class_data */
			sizeof (EMailBackend),
			0,     /* n_preallocs */
			(GInstanceInitFunc) mail_backend_init,
			NULL   /* value_table */
		};

		type = g_type_register_static (
			E_TYPE_SHELL_BACKEND, "EMailBackend", &type_info,
			G_TYPE_FLAG_ABSTRACT);
	}

	return type;
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
