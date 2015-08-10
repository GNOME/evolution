/*
 * e-mail-reader-utils.c
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

/* Miscellaneous utility functions used by EMailReader actions. */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "e-mail-reader-utils.h"

#include <glib/gi18n.h>
#include <libxml/tree.h>
#include <camel/camel.h>

#include <shell/e-shell-utils.h>

#include <libemail-engine/libemail-engine.h>

#include <em-format/e-mail-parser.h>
#include <em-format/e-mail-part-utils.h>

#include <composer/e-composer-actions.h>

#include "e-mail-backend.h"
#include "e-mail-browser.h"
#include "e-mail-printer.h"
#include "e-mail-display.h"
#include "em-composer-utils.h"
#include "em-utils.h"
#include "mail-autofilter.h"
#include "mail-vfolder-ui.h"
#include "message-list.h"

#define d(x)

typedef struct _AsyncContext AsyncContext;

struct _AsyncContext {
	EActivity *activity;
	CamelFolder *folder;
	CamelMimeMessage *message;
	EMailPartList *part_list;
	EMailReader *reader;
	CamelInternetAddress *address;
	GPtrArray *uids;
	gchar *folder_name;
	gchar *message_uid;

	EMailReplyType reply_type;
	EMailReplyStyle reply_style;
	EMailForwardStyle forward_style;
	GtkPrintOperationAction print_action;
	const gchar *filter_source;
	gint filter_type;
	gboolean replace;
	gboolean keep_signature;
};

static void
async_context_free (AsyncContext *async_context)
{
	g_clear_object (&async_context->activity);
	g_clear_object (&async_context->folder);
	g_clear_object (&async_context->message);
	g_clear_object (&async_context->part_list);
	g_clear_object (&async_context->reader);
	g_clear_object (&async_context->address);

	if (async_context->uids != NULL)
		g_ptr_array_unref (async_context->uids);

	g_free (async_context->folder_name);
	g_free (async_context->message_uid);

	g_slice_free (AsyncContext, async_context);
}

static gboolean
mail_reader_is_special_local_folder (const gchar *name)
{
	return (strcmp (name, "Drafts") == 0 ||
		strcmp (name, "Inbox") == 0 ||
		strcmp (name, "Outbox") == 0 ||
		strcmp (name, "Sent") == 0 ||
		strcmp (name, "Templates") == 0);
}

gboolean
e_mail_reader_confirm_delete (EMailReader *reader)
{
	CamelFolder *folder;
	CamelStore *parent_store;
	GtkWidget *check_button;
	GtkWidget *container;
	GtkWidget *dialog;
	GtkWindow *window;
	GSettings *settings;
	const gchar *label;
	gboolean prompt_delete_in_vfolder;
	gint response = GTK_RESPONSE_OK;

	/* Remind users what deleting from a search folder does. */

	g_return_val_if_fail (E_IS_MAIL_READER (reader), FALSE);

	folder = e_mail_reader_ref_folder (reader);
	window = e_mail_reader_get_window (reader);

	settings = e_util_ref_settings ("org.gnome.evolution.mail");

	prompt_delete_in_vfolder = g_settings_get_boolean (
		settings, "prompt-on-delete-in-vfolder");

	parent_store = camel_folder_get_parent_store (folder);

	if (!CAMEL_IS_VEE_STORE (parent_store))
		goto exit;

	if (!prompt_delete_in_vfolder)
		goto exit;

	dialog = e_alert_dialog_new_for_args (
		window, "mail:ask-delete-vfolder-msg",
		camel_folder_get_full_name (folder), NULL);

	container = e_alert_dialog_get_content_area (E_ALERT_DIALOG (dialog));

	label = _("Do not warn me again");
	check_button = gtk_check_button_new_with_label (label);
	gtk_box_pack_start (GTK_BOX (container), check_button, TRUE, TRUE, 6);
	gtk_widget_show (check_button);

	response = gtk_dialog_run (GTK_DIALOG (dialog));

	if (response != GTK_RESPONSE_DELETE_EVENT)
		g_settings_set_boolean (
			settings,
			"prompt-on-delete-in-vfolder",
			!gtk_toggle_button_get_active (
			GTK_TOGGLE_BUTTON (check_button)));

	gtk_widget_destroy (dialog);

exit:
	g_clear_object (&folder);
	g_clear_object (&settings);

	return (response == GTK_RESPONSE_OK);
}

static void
mail_reader_delete_folder_cb (GObject *source_object,
                              GAsyncResult *result,
                              gpointer user_data)
{
	CamelFolder *folder;
	EActivity *activity;
	EAlertSink *alert_sink;
	AsyncContext *async_context;
	GError *local_error = NULL;

	folder = CAMEL_FOLDER (source_object);
	async_context = (AsyncContext *) user_data;

	activity = async_context->activity;
	alert_sink = e_activity_get_alert_sink (activity);

	e_mail_folder_remove_finish (folder, result, &local_error);

	if (e_activity_handle_cancellation (activity, local_error)) {
		g_error_free (local_error);

	} else if (local_error != NULL) {
		e_alert_submit (
			alert_sink, "mail:no-delete-folder",
			camel_folder_get_full_name (folder),
			local_error->message, NULL);
		g_error_free (local_error);

	} else {
		e_activity_set_state (activity, E_ACTIVITY_COMPLETED);
	}

	async_context_free (async_context);
}

void
e_mail_reader_delete_folder (EMailReader *reader,
                             CamelFolder *folder)
{
	EMailBackend *backend;
	EMailSession *session;
	EShell *shell;
	EAlertSink *alert_sink;
	CamelStore *parent_store;
	CamelProvider *provider;
	MailFolderCache *folder_cache;
	GtkWindow *parent = e_shell_get_active_window (NULL);
	GtkWidget *dialog;
	gboolean store_is_local;
	const gchar *display_name;
	const gchar *full_name;
	gchar *full_display_name;
	CamelFolderInfoFlags flags = 0;
	gboolean have_flags;

	g_return_if_fail (E_IS_MAIL_READER (reader));
	g_return_if_fail (CAMEL_IS_FOLDER (folder));

	full_name = camel_folder_get_full_name (folder);
	display_name = camel_folder_get_display_name (folder);
	full_display_name = e_mail_folder_to_full_display_name (folder, NULL);
	parent_store = camel_folder_get_parent_store (folder);
	provider = camel_service_get_provider (CAMEL_SERVICE (parent_store));

	store_is_local = (provider->flags & CAMEL_PROVIDER_IS_LOCAL) != 0;

	backend = e_mail_reader_get_backend (reader);
	session = e_mail_backend_get_session (backend);

	alert_sink = e_mail_reader_get_alert_sink (reader);
	folder_cache = e_mail_session_get_folder_cache (session);

	if (store_is_local &&
		mail_reader_is_special_local_folder (full_name)) {
		e_alert_submit (
			alert_sink, "mail:no-delete-special-folder",
			full_display_name ? full_display_name : display_name, NULL);
		g_free (full_display_name);
		return;
	}

	shell = e_shell_backend_get_shell (E_SHELL_BACKEND (backend));

	if (!store_is_local && !e_shell_get_online (shell)) {
		e_alert_submit (
			alert_sink, "mail:online-operation",
			full_display_name ? full_display_name : display_name, NULL);
		g_free (full_display_name);
		return;
	}

	have_flags = mail_folder_cache_get_folder_info_flags (
		folder_cache, parent_store, full_name, &flags);

	if (have_flags && (flags & CAMEL_FOLDER_SYSTEM)) {
		e_alert_submit (
			alert_sink, "mail:no-delete-special-folder",
			full_display_name ? full_display_name : display_name, NULL);
		g_free (full_display_name);
		return;
	}

	if (have_flags && (flags & CAMEL_FOLDER_CHILDREN)) {
		if (CAMEL_IS_VEE_STORE (parent_store))
			dialog = e_alert_dialog_new_for_args (
				parent, "mail:ask-delete-vfolder",
				full_display_name ? full_display_name : display_name, NULL);
		else
			dialog = e_alert_dialog_new_for_args (
				parent, "mail:ask-delete-folder",
				full_display_name ? full_display_name : display_name, NULL);
	} else {
		if (CAMEL_IS_VEE_STORE (parent_store))
			dialog = e_alert_dialog_new_for_args (
				parent, "mail:ask-delete-vfolder-nochild",
				full_display_name ? full_display_name : display_name, NULL);
		else
			dialog = e_alert_dialog_new_for_args (
				parent, "mail:ask-delete-folder-nochild",
				full_display_name ? full_display_name : display_name, NULL);
	}

	if (gtk_dialog_run (GTK_DIALOG (dialog)) == GTK_RESPONSE_OK) {
		EActivity *activity;
		GCancellable *cancellable;
		AsyncContext *async_context;

		activity = e_mail_reader_new_activity (reader);
		cancellable = e_activity_get_cancellable (activity);

		async_context = g_slice_new0 (AsyncContext);
		async_context->activity = g_object_ref (activity);
		async_context->reader = g_object_ref (reader);

		/* Disable the dialog until the activity finishes. */
		gtk_widget_set_sensitive (dialog, FALSE);

		/* Destroy the dialog once the activity finishes. */
		g_object_set_data_full (
			G_OBJECT (activity), "delete-dialog",
			dialog, (GDestroyNotify) gtk_widget_destroy);

		e_mail_folder_remove (
			folder,
			G_PRIORITY_DEFAULT,
			cancellable,
			mail_reader_delete_folder_cb,
			async_context);

		g_object_unref (activity);
	} else {
		gtk_widget_destroy (dialog);
	}

	g_free (full_display_name);
}

static void
mail_reader_delete_folder_name_cb (GObject *source_object,
                                   GAsyncResult *result,
                                   gpointer user_data)
{
	CamelFolder *folder;
	EActivity *activity;
	EAlertSink *alert_sink;
	AsyncContext *async_context;
	GError *local_error = NULL;

	async_context = (AsyncContext *) user_data;

	activity = async_context->activity;
	alert_sink = e_activity_get_alert_sink (activity);

	/* XXX The returned CamelFolder is a borrowed reference. */
	folder = camel_store_get_folder_finish (
		CAMEL_STORE (source_object), result, &local_error);

	/* Sanity check. */
	g_return_if_fail (
		((folder != NULL) && (local_error == NULL)) ||
		((folder == NULL) && (local_error != NULL)));

	if (e_activity_handle_cancellation (activity, local_error)) {
		g_error_free (local_error);

	} else if (local_error != NULL) {
		e_alert_submit (
			alert_sink, "mail:no-delete-folder",
			async_context->folder_name,
			local_error->message, NULL);
		g_error_free (local_error);

	} else {
		e_activity_set_state (activity, E_ACTIVITY_COMPLETED);
		e_mail_reader_delete_folder (async_context->reader, folder);
	}

	async_context_free (async_context);
}

void
e_mail_reader_delete_folder_name (EMailReader *reader,
                                  CamelStore *store,
                                  const gchar *folder_name)
{
	EActivity *activity;
	GCancellable *cancellable;
	AsyncContext *async_context;

	g_return_if_fail (E_IS_MAIL_READER (reader));
	g_return_if_fail (CAMEL_IS_STORE (store));
	g_return_if_fail (folder_name != NULL);

	activity = e_mail_reader_new_activity (reader);
	cancellable = e_activity_get_cancellable (activity);

	async_context = g_slice_new0 (AsyncContext);
	async_context->activity = g_object_ref (activity);
	async_context->reader = g_object_ref (reader);
	async_context->folder_name = g_strdup (folder_name);

	camel_store_get_folder (
		store, folder_name,
		CAMEL_STORE_FOLDER_INFO_FAST,
		G_PRIORITY_DEFAULT, cancellable,
		mail_reader_delete_folder_name_cb,
		async_context);

	g_object_unref (activity);
}

/* Helper for e_mail_reader_expunge_folder() */
static void
mail_reader_expunge_folder_cb (GObject *source_object,
                               GAsyncResult *result,
                               gpointer user_data)
{
	CamelFolder *folder;
	EActivity *activity;
	EAlertSink *alert_sink;
	AsyncContext *async_context;
	GError *local_error = NULL;

	folder = CAMEL_FOLDER (source_object);
	async_context = (AsyncContext *) user_data;

	activity = async_context->activity;
	alert_sink = e_activity_get_alert_sink (activity);

	e_mail_folder_expunge_finish (folder, result, &local_error);

	if (e_activity_handle_cancellation (activity, local_error)) {
		g_error_free (local_error);

	} else if (local_error != NULL) {
		gchar *full_display_name;

		full_display_name = e_mail_folder_to_full_display_name (folder, NULL);

		e_alert_submit (
			alert_sink, "mail:no-expunge-folder",
			full_display_name ? full_display_name : camel_folder_get_display_name (folder),
			local_error->message, NULL);

		g_free (full_display_name);
		g_error_free (local_error);

	} else {
		e_activity_set_state (activity, E_ACTIVITY_COMPLETED);
	}

	async_context_free (async_context);
}

void
e_mail_reader_expunge_folder (EMailReader *reader,
                              CamelFolder *folder)
{
	GtkWindow *window;
	const gchar *display_name;
	gchar *full_display_name;
	gboolean proceed;

	g_return_if_fail (E_IS_MAIL_READER (reader));
	g_return_if_fail (CAMEL_IS_FOLDER (folder));

	window = e_mail_reader_get_window (reader);
	display_name = camel_folder_get_display_name (folder);
	full_display_name = e_mail_folder_to_full_display_name (folder, NULL);

	proceed = e_util_prompt_user (
		window, "org.gnome.evolution.mail", "prompt-on-expunge",
		"mail:ask-expunge", full_display_name ? full_display_name : display_name, NULL);

	g_free (full_display_name);

	if (proceed) {
		EActivity *activity;
		GCancellable *cancellable;
		AsyncContext *async_context;

		activity = e_mail_reader_new_activity (reader);
		cancellable = e_activity_get_cancellable (activity);

		async_context = g_slice_new0 (AsyncContext);
		async_context->activity = g_object_ref (activity);
		async_context->reader = g_object_ref (reader);

		e_mail_folder_expunge (
			folder,
			G_PRIORITY_DEFAULT, cancellable,
			mail_reader_expunge_folder_cb,
			async_context);

		g_object_unref (activity);
	}
}

/* Helper for e_mail_reader_expunge_folder_name() */
static void
mail_reader_expunge_folder_name_cb (GObject *source_object,
                                    GAsyncResult *result,
                                    gpointer user_data)
{
	CamelFolder *folder;
	EActivity *activity;
	EAlertSink *alert_sink;
	AsyncContext *async_context;
	GError *local_error = NULL;

	async_context = (AsyncContext *) user_data;

	activity = async_context->activity;
	alert_sink = e_activity_get_alert_sink (activity);

	/* XXX The returned CamelFolder is a borrowed reference. */
	folder = camel_store_get_folder_finish (
		CAMEL_STORE (source_object), result, &local_error);

	if (e_activity_handle_cancellation (activity, local_error)) {
		g_error_free (local_error);

	} else if (local_error != NULL) {
		e_alert_submit (
			alert_sink, "mail:no-expunge-folder",
			async_context->folder_name,
			local_error->message, NULL);
		g_error_free (local_error);

	} else {
		e_activity_set_state (activity, E_ACTIVITY_COMPLETED);
		e_mail_reader_expunge_folder (async_context->reader, folder);
	}

	async_context_free (async_context);
}

void
e_mail_reader_expunge_folder_name (EMailReader *reader,
                                   CamelStore *store,
                                   const gchar *folder_name)
{
	EActivity *activity;
	GCancellable *cancellable;
	AsyncContext *async_context;

	g_return_if_fail (E_IS_MAIL_READER (reader));
	g_return_if_fail (CAMEL_IS_STORE (store));
	g_return_if_fail (folder_name != NULL);

	activity = e_mail_reader_new_activity (reader);
	cancellable = e_activity_get_cancellable (activity);

	async_context = g_slice_new0 (AsyncContext);
	async_context->activity = g_object_ref (activity);
	async_context->reader = g_object_ref (reader);
	async_context->folder_name = g_strdup (folder_name);

	camel_store_get_folder (
		store, folder_name,
		CAMEL_STORE_FOLDER_INFO_FAST,
		G_PRIORITY_DEFAULT, cancellable,
		mail_reader_expunge_folder_name_cb,
		async_context);

	g_object_unref (activity);
}

struct _process_autoarchive_msg {
	MailMsg base;

	AsyncContext *async_context;
};

static gchar *
process_autoarchive_desc (struct _process_autoarchive_msg *m)
{
	gchar *desc, *full_display_name;

	full_display_name = e_mail_folder_to_full_display_name (m->async_context->folder, NULL);

	desc = g_strdup_printf (
		_("Refreshing folder '%s'"),
		full_display_name ? full_display_name : camel_folder_get_display_name (m->async_context->folder));

	g_free (full_display_name);

	return desc;
}

static void
process_autoarchive_exec (struct _process_autoarchive_msg *m,
			  GCancellable *cancellable,
			  GError **error)
{
	gchar *folder_uri;

	folder_uri = e_mail_folder_uri_from_folder (m->async_context->folder);

	em_utils_process_autoarchive_sync (
		e_mail_reader_get_backend (m->async_context->reader),
		m->async_context->folder, folder_uri, cancellable, error);

	g_free (folder_uri);
}

static void
process_autoarchive_done (struct _process_autoarchive_msg *m)
{
	EActivity *activity;
	EAlertSink *alert_sink;

	activity = m->async_context->activity;
	alert_sink = e_activity_get_alert_sink (activity);

	if (e_activity_handle_cancellation (activity, m->base.error)) {
	} else if (m->base.error != NULL) {
		gchar *full_display_name;

		full_display_name = e_mail_folder_to_full_display_name (m->async_context->folder, NULL);

		e_alert_submit (
			alert_sink, "mail:no-refresh-folder",
			full_display_name ? full_display_name : camel_folder_get_display_name (m->async_context->folder),
			m->base.error->message, NULL);

		g_free (full_display_name);
	} else {
		e_activity_set_state (activity, E_ACTIVITY_COMPLETED);
	}
}

static void
process_autoarchive_free (struct _process_autoarchive_msg *m)
{
	async_context_free (m->async_context);
}

static MailMsgInfo process_autoarchive_info = {
	sizeof (struct _process_autoarchive_msg),
	(MailMsgDescFunc) process_autoarchive_desc,
	(MailMsgExecFunc) process_autoarchive_exec,
	(MailMsgDoneFunc) process_autoarchive_done,
	(MailMsgFreeFunc) process_autoarchive_free
};

/* Helper for e_mail_reader_refresh_folder() */
static void
mail_reader_refresh_folder_cb (GObject *source_object,
                               GAsyncResult *result,
                               gpointer user_data)
{
	CamelFolder *folder;
	EActivity *activity;
	EAlertSink *alert_sink;
	AsyncContext *async_context;
	GError *local_error = NULL;

	folder = CAMEL_FOLDER (source_object);
	if (!camel_folder_refresh_info_finish (folder, result, &local_error) && !local_error)
		local_error = g_error_new_literal (CAMEL_ERROR, CAMEL_ERROR_GENERIC, _("Unknown error"));

	async_context = (AsyncContext *) user_data;

	activity = async_context->activity;
	alert_sink = e_activity_get_alert_sink (activity);

	if (e_activity_handle_cancellation (activity, local_error)) {
		g_error_free (local_error);

	} else if (local_error != NULL) {
		gchar *full_display_name;

		full_display_name = e_mail_folder_to_full_display_name (folder, NULL);

		e_alert_submit (
			alert_sink, "mail:no-refresh-folder",
			full_display_name ? full_display_name : camel_folder_get_display_name (folder),
			local_error->message, NULL);

		g_free (full_display_name);
		g_error_free (local_error);

	} else {
		struct _process_autoarchive_msg *m;

		g_warn_if_fail (async_context->folder == NULL);

		async_context->folder = g_object_ref (folder);

		m = mail_msg_new (&process_autoarchive_info);
		m->async_context = async_context;

		mail_msg_unordered_push (m);

		async_context = NULL;
	}

	if (async_context)
		async_context_free (async_context);
}

void
e_mail_reader_refresh_folder (EMailReader *reader,
                              CamelFolder *folder)
{
	EActivity *activity;
	GCancellable *cancellable;
	AsyncContext *async_context;

	g_return_if_fail (E_IS_MAIL_READER (reader));
	g_return_if_fail (CAMEL_IS_FOLDER (folder));

	activity = e_mail_reader_new_activity (reader);
	cancellable = e_activity_get_cancellable (activity);

	async_context = g_slice_new0 (AsyncContext);
	async_context->activity = g_object_ref (activity);
	async_context->reader = g_object_ref (reader);

	camel_folder_refresh_info (
		folder,
		G_PRIORITY_DEFAULT, cancellable,
		mail_reader_refresh_folder_cb,
		async_context);

	g_object_unref (activity);
}

/* Helper for e_mail_reader_refresh_folder_name() */
static void
mail_reader_refresh_folder_name_cb (GObject *source_object,
                                    GAsyncResult *result,
                                    gpointer user_data)
{
	CamelFolder *folder;
	EActivity *activity;
	EAlertSink *alert_sink;
	AsyncContext *async_context;
	GError *local_error = NULL;

	async_context = (AsyncContext *) user_data;

	activity = async_context->activity;
	alert_sink = e_activity_get_alert_sink (activity);

	/* XXX The returned CamelFolder is a borrowed reference. */
	folder = camel_store_get_folder_finish (
		CAMEL_STORE (source_object), result, &local_error);

	if (e_activity_handle_cancellation (activity, local_error)) {
		g_error_free (local_error);

	} else if (local_error != NULL) {
		gchar *full_display_name;

		full_display_name = g_strdup_printf ("%s : %s",
			camel_service_get_display_name (CAMEL_SERVICE (source_object)),
			async_context->folder_name);

		e_alert_submit (
			alert_sink, "mail:no-refresh-folder",
			full_display_name,
			local_error->message, NULL);

		g_free (full_display_name);
		g_error_free (local_error);

	} else {
		e_activity_set_state (activity, E_ACTIVITY_COMPLETED);
		e_mail_reader_refresh_folder (async_context->reader, folder);
	}

	async_context_free (async_context);
}

void
e_mail_reader_refresh_folder_name (EMailReader *reader,
                                   CamelStore *store,
                                   const gchar *folder_name)
{
	EActivity *activity;
	GCancellable *cancellable;
	AsyncContext *async_context;

	g_return_if_fail (E_IS_MAIL_READER (reader));
	g_return_if_fail (CAMEL_IS_STORE (store));
	g_return_if_fail (folder_name != NULL);

	activity = e_mail_reader_new_activity (reader);
	cancellable = e_activity_get_cancellable (activity);

	async_context = g_slice_new0 (AsyncContext);
	async_context->activity = g_object_ref (activity);
	async_context->reader = g_object_ref (reader);
	async_context->folder_name = g_strdup (folder_name);

	camel_store_get_folder (
		store, folder_name,
		CAMEL_STORE_FOLDER_INFO_FAST,
		G_PRIORITY_DEFAULT, cancellable,
		mail_reader_refresh_folder_name_cb,
		async_context);

	g_object_unref (activity);
}

/* Helper for e_mail_reader_unsubscribe_folder_name() */
static void
mail_reader_unsubscribe_folder_name_cb (GObject *source_object,
                                        GAsyncResult *result,
                                        gpointer user_data)
{
	EActivity *activity;
	EAlertSink *alert_sink;
	AsyncContext *async_context;
	GError *local_error = NULL;

	async_context = (AsyncContext *) user_data;

	activity = async_context->activity;
	alert_sink = e_activity_get_alert_sink (activity);

	camel_subscribable_unsubscribe_folder_finish (
		CAMEL_SUBSCRIBABLE (source_object), result, &local_error);

	if (e_activity_handle_cancellation (activity, local_error)) {
		g_error_free (local_error);

	} else if (local_error != NULL) {
		e_alert_submit (
			alert_sink, "mail:folder-unsubscribe",
			async_context->folder_name,
			local_error->message, NULL);
		g_error_free (local_error);

	} else {
		e_activity_set_state (activity, E_ACTIVITY_COMPLETED);
	}

	async_context_free (async_context);
}

void
e_mail_reader_unsubscribe_folder_name (EMailReader *reader,
                                       CamelStore *store,
                                       const gchar *folder_name)
{
	EActivity *activity;
	GCancellable *cancellable;
	AsyncContext *async_context;

	g_return_if_fail (E_IS_MAIL_READER (reader));
	g_return_if_fail (CAMEL_IS_SUBSCRIBABLE (store));
	g_return_if_fail (folder_name != NULL);

	activity = e_mail_reader_new_activity (reader);
	cancellable = e_activity_get_cancellable (activity);

	async_context = g_slice_new0 (AsyncContext);
	async_context->activity = g_object_ref (activity);
	async_context->reader = g_object_ref (reader);
	async_context->folder_name = g_strdup (folder_name);

	camel_subscribable_unsubscribe_folder (
		CAMEL_SUBSCRIBABLE (store), folder_name,
		G_PRIORITY_DEFAULT, cancellable,
		mail_reader_unsubscribe_folder_name_cb,
		async_context);

	g_object_unref (activity);
}

guint
e_mail_reader_mark_selected (EMailReader *reader,
                             guint32 mask,
                             guint32 set)
{
	CamelFolder *folder;
	guint ii = 0;

	g_return_val_if_fail (E_IS_MAIL_READER (reader), 0);

	folder = e_mail_reader_ref_folder (reader);

	if (folder != NULL) {
		GPtrArray *uids;

		camel_folder_freeze (folder);

		uids = e_mail_reader_get_selected_uids (reader);

		for (ii = 0; ii < uids->len; ii++)
			camel_folder_set_message_flags (
				folder, uids->pdata[ii], mask, set);

		/* This function is called on user interaction, thus make sure the message list
		   will scroll to the selected message, which can eventually change due to
		   view filters on the folder. */
		if (uids->len > 0) {
			GtkWidget *message_list = e_mail_reader_get_message_list (reader);

			if (message_list)
				e_tree_show_cursor_after_reflow (E_TREE (message_list));
		}

		g_ptr_array_unref (uids);

		camel_folder_thaw (folder);

		g_object_unref (folder);
	}

	return ii;
}

static guint
summary_msgid_hash (gconstpointer key)
{
	const CamelSummaryMessageID *id = (const CamelSummaryMessageID *) key;

	return id->id.part.lo;
}

static gboolean
summary_msgid_equal (gconstpointer a,
		     gconstpointer b)
{
	return ((const CamelSummaryMessageID *) a)->id.id == ((const CamelSummaryMessageID *) b)->id.id;
}

typedef struct {
	CamelFolder *folder;
	GSList *uids;
	EIgnoreThreadKind kind;
} MarkIgnoreThreadData;

static void
mark_ignore_thread_data_free (gpointer ptr)
{
	MarkIgnoreThreadData *mit = ptr;

	if (mit) {
		g_clear_object (&mit->folder);
		g_slist_free_full (mit->uids, (GDestroyNotify) camel_pstring_free);
		g_free (mit);
	}
}

static void
insert_to_checked_msgids (GHashTable *checked_msgids,
			  const CamelSummaryMessageID *msgid)
{
	CamelSummaryMessageID *msgid_copy;

	if (!msgid)
		return;

	msgid_copy = g_new0 (CamelSummaryMessageID, 1);
	memcpy (msgid_copy, msgid, sizeof (CamelSummaryMessageID));

	g_hash_table_insert (checked_msgids, msgid_copy, GINT_TO_POINTER (1));
}

static gboolean
mark_ignore_thread_traverse_uids (CamelFolder *folder,
				  const gchar *uid,
				  GHashTable *checked_uids,
				  GHashTable *checked_msgids,
				  gboolean whole_thread,
				  gboolean ignore_thread,
				  GCancellable *cancellable,
				  GError **error)
{
	GSList *to_check;
	GPtrArray *uids;
	gint ii;
	gboolean success;

	success = !g_cancellable_set_error_if_cancelled (cancellable, error);
	if (!success)
		return success;

	if (g_hash_table_contains (checked_uids, uid))
		return success;

	to_check = g_slist_prepend (NULL, (gpointer) camel_pstring_strdup (uid));

	while (to_check != NULL && !g_cancellable_set_error_if_cancelled (cancellable, error)) {
		CamelMessageInfo *mi;
		const CamelSummaryMessageID *msgid;
		const CamelSummaryReferences *references;
		const gchar *uid = to_check->data;
		gchar *sexp;
		GError *local_error = NULL;

		to_check = g_slist_remove (to_check, uid);

		if (!uid || g_hash_table_contains (checked_uids, uid)) {
			camel_pstring_free (uid);
			continue;
		}

		g_hash_table_insert (checked_uids, (gpointer) camel_pstring_strdup (uid), GINT_TO_POINTER (1));

		mi = camel_folder_get_message_info (folder, uid);
		if (!mi || !camel_message_info_message_id (mi)) {
			if (mi)
				camel_message_info_unref (mi);
			camel_pstring_free (uid);
			continue;
		}

		camel_message_info_set_user_flag (mi, "ignore-thread", ignore_thread);

		msgid = camel_message_info_message_id (mi);
		insert_to_checked_msgids (checked_msgids, msgid);

		if (whole_thread) {
			/* Search for parents */
			references = camel_message_info_references (mi);
			if (references) {
				GString *expr = NULL;

				for (ii = 0; ii < references->size; ii++) {
					if (references->references[ii].id.id == 0 ||
					    g_hash_table_contains (checked_msgids, &references->references[ii]))
						continue;

					insert_to_checked_msgids (checked_msgids, &references->references[ii]);

					if (!expr)
						expr = g_string_new ("(match-all (or ");

					g_string_append_printf (expr, "(= \"msgid\" \"%lu %lu\")",
						(gulong) references->references[ii].id.part.hi,
						(gulong) references->references[ii].id.part.lo);
				}

				if (expr) {
					g_string_append (expr, "))");

					uids = camel_folder_search_by_expression (folder, expr->str, cancellable, &local_error);
					if (uids) {
						for (ii = 0; ii < uids->len; ii++) {
							const gchar *refruid = uids->pdata[ii];

							if (refruid && !g_hash_table_contains (checked_uids, refruid))
								to_check = g_slist_prepend (to_check, (gpointer) camel_pstring_strdup (refruid));
						}

						camel_folder_search_free (folder, uids);
					}

					g_string_free (expr, TRUE);

					if (local_error) {
						g_propagate_error (error, local_error);
						camel_message_info_unref (mi);
						camel_pstring_free (uid);
						success = FALSE;
						break;
					}
				}
			}
		}

		/* Search for children */
		sexp = g_strdup_printf ("(match-all (= \"references\" \"%lu %lu\"))", (gulong) msgid->id.part.hi, (gulong) msgid->id.part.lo);
		uids = camel_folder_search_by_expression (folder, sexp, cancellable, &local_error);
		if (uids) {
			for (ii = 0; ii < uids->len; ii++) {
				const gchar *refruid = uids->pdata[ii];

				if (refruid && !g_hash_table_contains (checked_uids, refruid)) {
					CamelMessageInfo *refrmi = camel_folder_get_message_info (folder, refruid);

					if (refrmi && camel_message_info_message_id (refrmi) &&
					    !g_hash_table_contains (checked_msgids, camel_message_info_message_id (refrmi))) {
						/* The 'references' filter search can return false positives */
						references = camel_message_info_references (refrmi);
						if (references) {
							gint jj;

							for (jj = 0; jj < references->size; jj++) {
								if (references->references[jj].id.id == msgid->id.id) {
									to_check = g_slist_prepend (to_check, (gpointer) camel_pstring_strdup (refruid));
									break;
								}
							}
						}
					}

					if (refrmi)
						camel_message_info_unref (refrmi);
				}
			}

			camel_folder_search_free (folder, uids);
		}
		g_free (sexp);

		camel_message_info_unref (mi);
		camel_pstring_free (uid);

		if (local_error) {
			g_propagate_error (error, local_error);
			success = FALSE;
			break;
		}
	}

	g_slist_free_full (to_check, (GDestroyNotify) camel_pstring_free);

	return success;
}

static void
mail_reader_utils_mark_ignore_thread_thread (EAlertSinkThreadJobData *job_data,
					     gpointer user_data,
					     GCancellable *cancellable,
					     GError **error)
{
	MarkIgnoreThreadData *mit = user_data;
	GHashTable *checked_uids; /* gchar * (UID) ~> 1 */
	GHashTable *checked_msgids; /* CamelSummaryMessageID * ~> 1 */
	gboolean ignore_thread, whole_thread;
	GSList *link;

	g_return_if_fail (mit != NULL);

	camel_folder_freeze (mit->folder);

	whole_thread = mit->kind == E_IGNORE_THREAD_WHOLE_SET || mit->kind == E_IGNORE_THREAD_WHOLE_UNSET;
	ignore_thread = mit->kind == E_IGNORE_THREAD_WHOLE_SET || mit->kind == E_IGNORE_THREAD_SUBSET_SET;

	checked_uids = g_hash_table_new_full (g_str_hash, g_str_equal, (GDestroyNotify) camel_pstring_free, NULL);
	checked_msgids = g_hash_table_new_full (summary_msgid_hash, summary_msgid_equal, g_free, NULL);

	for (link = mit->uids; link; link = g_slist_next (link)) {
		if (!mark_ignore_thread_traverse_uids (mit->folder, link->data, checked_uids, checked_msgids,
			whole_thread, ignore_thread, cancellable, error)) {
			break;
		}
	}

	camel_folder_thaw (mit->folder);

	g_hash_table_destroy (checked_msgids);
	g_hash_table_destroy (checked_uids);
}

void
e_mail_reader_mark_selected_ignore_thread (EMailReader *reader,
					   EIgnoreThreadKind kind)
{
	CamelFolder *folder;

	g_return_if_fail (E_IS_MAIL_READER (reader));

	folder = e_mail_reader_ref_folder (reader);

	if (folder != NULL) {
		GPtrArray *uids;
		guint ii;

		uids = e_mail_reader_get_selected_uids (reader);
		if (uids && uids->len > 0) {
			MarkIgnoreThreadData *mit;
			EAlertSink *alert_sink;
			EActivity *activity;
			const gchar *description = NULL, *alert_id = NULL;

			switch (kind) {
			case E_IGNORE_THREAD_WHOLE_SET:
				description = _("Marking thread to be ignored");
				alert_id = "mail:failed-mark-ignore-thread";
				break;
			case E_IGNORE_THREAD_WHOLE_UNSET:
				description = _("Unmarking thread from being ignored");
				alert_id = "mail:failed-mark-unignore-thread";
				break;
			case E_IGNORE_THREAD_SUBSET_SET:
				description = _("Marking subthread to be ignored");
				alert_id = "mail:failed-mark-ignore-subthread";
				break;
			case E_IGNORE_THREAD_SUBSET_UNSET:
				description = _("Unmarking subthread from being ignored");
				alert_id = "mail:failed-mark-unignore-subthread";
				break;
			}

			mit = g_new0 (MarkIgnoreThreadData, 1);
			mit->folder = g_object_ref (folder);
			mit->kind = kind;

			for (ii = 0; ii < uids->len; ii++) {
				mit->uids = g_slist_prepend (mit->uids, (gpointer) camel_pstring_strdup (uids->pdata[ii]));
			}

			alert_sink = e_mail_reader_get_alert_sink (reader);

			activity = e_alert_sink_submit_thread_job (alert_sink, description, alert_id,
				camel_folder_get_full_name (folder), mail_reader_utils_mark_ignore_thread_thread,
				mit, mark_ignore_thread_data_free);

			g_clear_object (&activity);
		}

		g_ptr_array_unref (uids);
		g_object_unref (folder);
	}
}

static void
copy_tree_state (EMailReader *src_reader,
                 EMailReader *des_reader)
{
	GtkWidget *src_mlist, *des_mlist;
	ETableState *state;

	g_return_if_fail (src_reader != NULL);
	g_return_if_fail (des_reader != NULL);

	src_mlist = e_mail_reader_get_message_list (src_reader);
	if (!src_mlist)
		return;

	des_mlist = e_mail_reader_get_message_list (des_reader);
	if (!des_mlist)
		return;

	state = e_tree_get_state_object (E_TREE (src_mlist));
	e_tree_set_state_object (E_TREE (des_mlist), state);
	g_object_unref (state);

	message_list_set_search (MESSAGE_LIST (des_mlist), MESSAGE_LIST (src_mlist)->search);
}

guint
e_mail_reader_open_selected (EMailReader *reader)
{
	EShell *shell;
	EMailBackend *backend;
	ESourceRegistry *registry;
	CamelFolder *folder;
	GtkWindow *window;
	GPtrArray *views;
	GPtrArray *uids;
	guint ii = 0;

	g_return_val_if_fail (E_IS_MAIL_READER (reader), 0);

	backend = e_mail_reader_get_backend (reader);
	shell = e_shell_backend_get_shell (E_SHELL_BACKEND (backend));
	registry = e_shell_get_registry (shell);

	folder = e_mail_reader_ref_folder (reader);
	uids = e_mail_reader_get_selected_uids (reader);
	window = e_mail_reader_get_window (reader);

	if (!em_utils_ask_open_many (window, uids->len))
		goto exit;

	if (em_utils_folder_is_drafts (registry, folder) ||
		em_utils_folder_is_outbox (registry, folder) ||
		em_utils_folder_is_templates (registry, folder)) {

		e_mail_reader_edit_messages (reader, folder, uids, TRUE, TRUE);

		ii = uids->len;

		goto exit;
	}

	views = g_ptr_array_new ();

	/* For vfolders we need to edit the original, not the vfolder copy. */
	for (ii = 0; ii < uids->len; ii++) {
		const gchar *uid = uids->pdata[ii];
		CamelFolder *real_folder;
		CamelMessageInfo *info;
		gchar *real_uid;

		if (!CAMEL_IS_VEE_FOLDER (folder)) {
			g_ptr_array_add (views, g_strdup (uid));
			continue;
		}

		info = camel_folder_get_message_info (folder, uid);
		if (info == NULL)
			continue;

		real_folder = camel_vee_folder_get_location (
			CAMEL_VEE_FOLDER (folder),
			(CamelVeeMessageInfo *) info, &real_uid);

		if (em_utils_folder_is_drafts (registry, real_folder) ||
			em_utils_folder_is_outbox (registry, real_folder)) {
			GPtrArray *edits;

			edits = g_ptr_array_new ();
			g_ptr_array_add (edits, real_uid);
			e_mail_reader_edit_messages (
				reader, real_folder, edits, TRUE, TRUE);
			g_ptr_array_unref (edits);
		} else {
			g_free (real_uid);
			g_ptr_array_add (views, g_strdup (uid));
		}

		camel_message_info_unref (info);
	}

	for (ii = 0; ii < views->len; ii++) {
		const gchar *uid = views->pdata[ii];
		GtkWidget *browser;
		MessageList *ml;

		browser = e_mail_browser_new (backend, E_MAIL_FORMATTER_MODE_NORMAL);

		ml = MESSAGE_LIST (e_mail_reader_get_message_list (
			E_MAIL_READER (browser)));
		message_list_freeze (ml);

		e_mail_reader_set_folder (E_MAIL_READER (browser), folder);
		e_mail_reader_set_message (E_MAIL_READER (browser), uid);

		copy_tree_state (reader, E_MAIL_READER (browser));
		e_mail_reader_set_group_by_threads (
			E_MAIL_READER (browser),
			e_mail_reader_get_group_by_threads (reader));

		message_list_thaw (ml);
		gtk_widget_show (browser);
	}

	g_ptr_array_foreach (views, (GFunc) g_free, NULL);
	g_ptr_array_free (views, TRUE);

exit:
	g_clear_object (&folder);
	g_ptr_array_unref (uids);

	return ii;
}

static void
mail_reader_print_message_cb (GObject *source_object,
                              GAsyncResult *result,
                              gpointer user_data)
{
	EActivity *activity;
	EAlertSink *alert_sink;
	AsyncContext *async_context;
	GError *local_error = NULL;

	async_context = (AsyncContext *) user_data;

	activity = async_context->activity;
	alert_sink = e_activity_get_alert_sink (activity);

	e_mail_printer_print_finish (
		E_MAIL_PRINTER (source_object), result, &local_error);

	if (e_activity_handle_cancellation (activity, local_error)) {
		g_error_free (local_error);

	} else if (local_error != NULL) {
		e_alert_submit (
			alert_sink, "mail:printing-failed",
			local_error->message, NULL);
		g_error_free (local_error);

	} else {
		/* Set activity as completed, and keep it displayed for a few
		 * seconds so that user can actually see the the printing was
		 * successfully finished. */
		e_activity_set_state (activity, E_ACTIVITY_COMPLETED);
	}

	async_context_free (async_context);
}

static void
mail_reader_print_parse_message_cb (GObject *source_object,
                                    GAsyncResult *result,
                                    gpointer user_data)
{
	EMailReader *reader;
	EMailDisplay *mail_display;
	EMailFormatter *formatter;
	EActivity *activity;
	GCancellable *cancellable;
	EMailPrinter *printer;
	EMailPartList *part_list;
	EMailRemoteContent *remote_content;
	AsyncContext *async_context;
	GError *local_error = NULL;

	reader = E_MAIL_READER (source_object);
	async_context = (AsyncContext *) user_data;

	activity = async_context->activity;
	cancellable = e_activity_get_cancellable (activity);

	part_list = e_mail_reader_parse_message_finish (reader, result, &local_error);

	if (local_error) {
		g_warn_if_fail (g_error_matches (local_error, G_IO_ERROR, G_IO_ERROR_CANCELLED));

		/* coverity[check_return] */
		e_activity_handle_cancellation (activity, local_error);
		g_clear_error (&local_error);
		async_context_free (async_context);

		return;
	}

	mail_display = e_mail_reader_get_mail_display (reader);
	formatter = e_mail_display_get_formatter (mail_display);
	remote_content = e_mail_display_ref_remote_content (mail_display);

	printer = e_mail_printer_new (part_list, remote_content);

	g_clear_object (&remote_content);

	e_activity_set_text (activity, _("Printing"));

	e_mail_printer_print (
		printer,
		async_context->print_action,
		formatter,
		cancellable,
		mail_reader_print_message_cb,
		async_context);

	g_object_unref (printer);
}

static void
mail_reader_print_get_message_cb (GObject *source_object,
                                  GAsyncResult *result,
                                  gpointer user_data)
{
	EActivity *activity;
	EAlertSink *alert_sink;
	CamelMimeMessage *message;
	GCancellable *cancellable;
	AsyncContext *async_context;
	GError *local_error = NULL;

	async_context = (AsyncContext *) user_data;

	activity = async_context->activity;
	alert_sink = e_activity_get_alert_sink (activity);
	cancellable = e_activity_get_cancellable (activity);

	message = camel_folder_get_message_finish (
		CAMEL_FOLDER (source_object), result, &local_error);

	/* Sanity check. */
	g_return_if_fail (
		((message != NULL) && (local_error == NULL)) ||
		((message == NULL) && (local_error != NULL)));

	if (e_activity_handle_cancellation (activity, local_error)) {
		async_context_free (async_context);
		g_error_free (local_error);
		return;

	} else if (local_error != NULL) {
		e_alert_submit (
			alert_sink, "mail:no-retrieve-message",
			local_error->message, NULL);
		async_context_free (async_context);
		g_error_free (local_error);
		return;
	}

	e_activity_set_text (activity, "");

	e_mail_reader_parse_message (
		async_context->reader,
		async_context->folder,
		async_context->message_uid,
		message,
		cancellable,
		mail_reader_print_parse_message_cb,
		async_context);

	g_object_unref (message);
}

void
e_mail_reader_print (EMailReader *reader,
                     GtkPrintOperationAction action)
{
	EActivity *activity;
	GCancellable *cancellable;
	MessageList *message_list;
	AsyncContext *async_context;

	g_return_if_fail (E_IS_MAIL_READER (reader));

	message_list = MESSAGE_LIST (e_mail_reader_get_message_list (reader));

	activity = e_mail_reader_new_activity (reader);
	cancellable = e_activity_get_cancellable (activity);

	async_context = g_slice_new0 (AsyncContext);
	async_context->activity = g_object_ref (activity);
	async_context->folder = e_mail_reader_ref_folder (reader);
	async_context->reader = g_object_ref (reader);
	async_context->message_uid = g_strdup (message_list->cursor_uid);
	async_context->print_action = action;

	camel_folder_get_message (
		async_context->folder,
		async_context->message_uid,
		G_PRIORITY_DEFAULT, cancellable,
		mail_reader_print_get_message_cb,
		async_context);

	g_object_unref (activity);
}

static void
mail_reader_remove_attachments_cb (GObject *source_object,
                                   GAsyncResult *result,
                                   gpointer user_data)
{
	EActivity *activity;
	EAlertSink *alert_sink;
	AsyncContext *async_context;
	GError *local_error = NULL;

	async_context = (AsyncContext *) user_data;

	activity = async_context->activity;
	alert_sink = e_activity_get_alert_sink (activity);

	e_mail_folder_remove_attachments_finish (
		CAMEL_FOLDER (source_object), result, &local_error);

	if (e_activity_handle_cancellation (activity, local_error)) {
		g_error_free (local_error);

	} else if (local_error != NULL) {
		e_alert_submit (
			alert_sink,
			"mail:remove-attachments",
			local_error->message, NULL);
		g_error_free (local_error);
	}

	async_context_free (async_context);
}

void
e_mail_reader_remove_attachments (EMailReader *reader)
{
	EActivity *activity;
	AsyncContext *async_context;
	GCancellable *cancellable;
	CamelFolder *folder;
	GPtrArray *uids;

	g_return_if_fail (E_IS_MAIL_READER (reader));

	uids = e_mail_reader_get_selected_uids (reader);
	g_return_if_fail (uids != NULL);

	/* Remove attachments asynchronously. */

	activity = e_mail_reader_new_activity (reader);
	cancellable = e_activity_get_cancellable (activity);

	async_context = g_slice_new0 (AsyncContext);
	async_context->activity = g_object_ref (activity);
	async_context->reader = g_object_ref (reader);

	folder = e_mail_reader_ref_folder (reader);

	e_mail_folder_remove_attachments (
		folder, uids,
		G_PRIORITY_DEFAULT,
		cancellable,
		mail_reader_remove_attachments_cb,
		async_context);

	g_object_unref (folder);

	g_object_unref (activity);

	g_ptr_array_unref (uids);
}

static void
mail_reader_remove_duplicates_cb (GObject *source_object,
                                  GAsyncResult *result,
                                  gpointer user_data)
{
	EActivity *activity;
	EAlertSink *alert_sink;
	CamelFolder *folder;
	GHashTable *duplicates;
	GtkWindow *parent_window;
	guint n_duplicates;
	gchar *full_display_name;
	AsyncContext *async_context;
	GError *local_error = NULL;

	folder = CAMEL_FOLDER (source_object);
	async_context = (AsyncContext *) user_data;

	activity = async_context->activity;
	alert_sink = e_activity_get_alert_sink (activity);

	parent_window = e_mail_reader_get_window (async_context->reader);

	duplicates = e_mail_folder_find_duplicate_messages_finish (
		folder, result, &local_error);

	/* Sanity check. */
	g_return_if_fail (
		((duplicates != NULL) && (local_error == NULL)) ||
		((duplicates == NULL) && (local_error != NULL)));

	if (e_activity_handle_cancellation (activity, local_error)) {
		async_context_free (async_context);
		g_error_free (local_error);
		return;

	} else if (local_error != NULL) {
		e_alert_submit (
			alert_sink,
			"mail:find-duplicate-messages",
			local_error->message, NULL);
		async_context_free (async_context);
		g_error_free (local_error);
		return;
	}

	/* Finalize the activity here so we don't leave a message in
	 * the task bar while prompting the user for confirmation. */
	e_activity_set_state (activity, E_ACTIVITY_COMPLETED);
	g_clear_object (&async_context->activity);

	n_duplicates = g_hash_table_size (duplicates);
	full_display_name = e_mail_folder_to_full_display_name (folder, NULL);

	if (n_duplicates == 0) {
		e_util_prompt_user (
			parent_window, "org.gnome.evolution.mail", NULL,
			"mail:info-no-remove-duplicates",
			full_display_name ? full_display_name : camel_folder_get_display_name (folder), NULL);
	} else {
		gchar *confirmation;
		gboolean proceed;

		confirmation = g_strdup_printf (ngettext (
			/* Translators: %s is replaced with a folder
			 * name %u with count of duplicate messages. */
			"Folder '%s' contains %u duplicate message. "
			"Are you sure you want to delete it?",
			"Folder '%s' contains %u duplicate messages. "
			"Are you sure you want to delete them?",
			n_duplicates),
			full_display_name ? full_display_name : camel_folder_get_display_name (folder),
			n_duplicates);

		proceed = e_util_prompt_user (
			parent_window, "org.gnome.evolution.mail", NULL,
			"mail:ask-remove-duplicates",
			confirmation, NULL);

		if (proceed) {
			GHashTableIter iter;
			gpointer key;

			camel_folder_freeze (folder);

			g_hash_table_iter_init (&iter, duplicates);

			/* Mark duplicate messages for deletion. */
			while (g_hash_table_iter_next (&iter, &key, NULL))
				camel_folder_delete_message (folder, key);

			camel_folder_thaw (folder);
		}

		g_free (confirmation);
	}

	g_hash_table_destroy (duplicates);
	g_free (full_display_name);

	async_context_free (async_context);
}

void
e_mail_reader_remove_duplicates (EMailReader *reader)
{
	EActivity *activity;
	GCancellable *cancellable;
	AsyncContext *async_context;
	CamelFolder *folder;
	GPtrArray *uids;

	g_return_if_fail (E_IS_MAIL_READER (reader));

	uids = e_mail_reader_get_selected_uids (reader);
	g_return_if_fail (uids != NULL);

	/* Find duplicate messages asynchronously. */

	activity = e_mail_reader_new_activity (reader);
	cancellable = e_activity_get_cancellable (activity);

	async_context = g_slice_new0 (AsyncContext);
	async_context->activity = g_object_ref (activity);
	async_context->reader = g_object_ref (reader);

	folder = e_mail_reader_ref_folder (reader);

	e_mail_folder_find_duplicate_messages (
		folder, uids,
		G_PRIORITY_DEFAULT,
		cancellable,
		mail_reader_remove_duplicates_cb,
		async_context);

	g_object_unref (folder);

	g_object_unref (activity);

	g_ptr_array_unref (uids);
}

static void
mail_reader_edit_messages_cb (GObject *source_object,
                              GAsyncResult *result,
                              gpointer user_data)
{
	CamelFolder *folder;
	EShell *shell;
	EMailBackend *backend;
	EActivity *activity;
	EAlertSink *alert_sink;
	GHashTable *hash_table;
	GHashTableIter iter;
	gpointer key, value;
	AsyncContext *async_context;
	GError *local_error = NULL;

	folder = CAMEL_FOLDER (source_object);
	async_context = (AsyncContext *) user_data;

	activity = async_context->activity;
	alert_sink = e_activity_get_alert_sink (activity);

	hash_table = e_mail_folder_get_multiple_messages_finish (
		folder, result, &local_error);

	/* Sanity check. */
	g_return_if_fail (
		((hash_table != NULL) && (local_error == NULL)) ||
		((hash_table == NULL) && (local_error != NULL)));

	if (e_activity_handle_cancellation (activity, local_error)) {
		g_error_free (local_error);
		goto exit;

	} else if (local_error != NULL) {
		e_alert_submit (
			alert_sink,
			"mail:get-multiple-messages",
			local_error->message, NULL);
		g_error_free (local_error);
		goto exit;
	}

	backend = e_mail_reader_get_backend (async_context->reader);
	shell = e_shell_backend_get_shell (E_SHELL_BACKEND (backend));

	/* Open each message in its own composer window. */

	g_hash_table_iter_init (&iter, hash_table);

	while (g_hash_table_iter_next (&iter, &key, &value)) {
		EMsgComposer *composer;
		CamelMimeMessage *message;
		const gchar *message_uid = NULL;

		if (async_context->replace)
			message_uid = (const gchar *) key;

		message = CAMEL_MIME_MESSAGE (value);

		camel_medium_remove_header (
			CAMEL_MEDIUM (message), "X-Mailer");

		composer = em_utils_edit_message (
			shell, folder, message, message_uid,
			async_context->keep_signature);

		e_mail_reader_composer_created (
			async_context->reader, composer, message);
	}

	g_hash_table_unref (hash_table);

	e_activity_set_state (activity, E_ACTIVITY_COMPLETED);

exit:
	async_context_free (async_context);
}

void
e_mail_reader_edit_messages (EMailReader *reader,
                             CamelFolder *folder,
                             GPtrArray *uids,
                             gboolean replace,
                             gboolean keep_signature)
{
	EActivity *activity;
	GCancellable *cancellable;
	AsyncContext *async_context;

	g_return_if_fail (E_IS_MAIL_READER (reader));
	g_return_if_fail (CAMEL_IS_FOLDER (folder));
	g_return_if_fail (uids != NULL);

	activity = e_mail_reader_new_activity (reader);
	cancellable = e_activity_get_cancellable (activity);

	async_context = g_slice_new0 (AsyncContext);
	async_context->activity = g_object_ref (activity);
	async_context->reader = g_object_ref (reader);
	async_context->replace = replace;
	async_context->keep_signature = keep_signature;

	e_mail_folder_get_multiple_messages (
		folder, uids,
		G_PRIORITY_DEFAULT,
		cancellable,
		mail_reader_edit_messages_cb,
		async_context);

	g_object_unref (activity);
}

static void
mail_reader_forward_attachment_cb (GObject *source_object,
                                   GAsyncResult *result,
                                   gpointer user_data)
{
	CamelFolder *folder;
	EMailBackend *backend;
	EActivity *activity;
	EAlertSink *alert_sink;
	CamelMimePart *part;
	CamelDataWrapper *content;
	EMsgComposer *composer;
	gchar *subject = NULL;
	AsyncContext *async_context;
	GError *local_error = NULL;

	folder = CAMEL_FOLDER (source_object);
	async_context = (AsyncContext *) user_data;

	activity = async_context->activity;
	alert_sink = e_activity_get_alert_sink (activity);

	part = e_mail_folder_build_attachment_finish (
		folder, result, &subject, &local_error);

	/* Sanity check. */
	g_return_if_fail (
		((part != NULL) && (local_error == NULL)) ||
		((part == NULL) && (local_error != NULL)));

	if (e_activity_handle_cancellation (activity, local_error)) {
		g_warn_if_fail (subject == NULL);
		g_error_free (local_error);
		goto exit;

	} else if (local_error != NULL) {
		g_warn_if_fail (subject == NULL);
		e_alert_submit (
			alert_sink,
			"mail:get-multiple-messages",
			local_error->message, NULL);
		g_error_free (local_error);
		goto exit;
	}

	backend = e_mail_reader_get_backend (async_context->reader);

	composer = em_utils_forward_attachment (
		backend, part, subject, folder, async_context->uids);

	content = camel_medium_get_content (CAMEL_MEDIUM (part));
	if (CAMEL_IS_MIME_MESSAGE (content)) {
		e_mail_reader_composer_created (
			async_context->reader, composer,
			CAMEL_MIME_MESSAGE (content));
	} else {
		/* XXX What to do for the multipart/digest case?
		 *     Extract the first message from the digest, or
		 *     change the argument type to CamelMimePart and
		 *     just pass the whole digest through?
		 *
		 *     This signal is primarily serving EMailBrowser,
		 *     which can only forward one message at a time.
		 *     So for the moment it doesn't matter, but still
		 *     something to consider. */
		e_mail_reader_composer_created (
			async_context->reader, composer, NULL);
	}

	e_activity_set_state (activity, E_ACTIVITY_COMPLETED);

	g_object_unref (part);
	g_free (subject);

exit:
	async_context_free (async_context);
}

static void
mail_reader_forward_messages_cb (GObject *source_object,
                                 GAsyncResult *result,
                                 gpointer user_data)
{
	CamelFolder *folder;
	EMailBackend *backend;
	EActivity *activity;
	EAlertSink *alert_sink;
	GHashTable *hash_table;
	GHashTableIter iter;
	gpointer key, value;
	AsyncContext *async_context;
	GError *local_error = NULL;

	folder = CAMEL_FOLDER (source_object);
	async_context = (AsyncContext *) user_data;

	activity = async_context->activity;
	alert_sink = e_activity_get_alert_sink (activity);

	backend = e_mail_reader_get_backend (async_context->reader);

	hash_table = e_mail_folder_get_multiple_messages_finish (
		folder, result, &local_error);

	/* Sanity check. */
	g_return_if_fail (
		((hash_table != NULL) && (local_error == NULL)) ||
		((hash_table == NULL) && (local_error != NULL)));

	if (e_activity_handle_cancellation (activity, local_error)) {
		g_error_free (local_error);
		goto exit;

	} else if (local_error != NULL) {
		e_alert_submit (
			alert_sink,
			"mail:get-multiple-messages",
			local_error->message, NULL);
		g_error_free (local_error);
		goto exit;
	}

	/* Create a new composer window for each message. */

	g_hash_table_iter_init (&iter, hash_table);

	while (g_hash_table_iter_next (&iter, &key, &value)) {
		EMsgComposer *composer;
		CamelMimeMessage *message;
		const gchar *message_uid;

		message_uid = (const gchar *) key;
		message = CAMEL_MIME_MESSAGE (value);

		composer = em_utils_forward_message (
			backend, message,
			async_context->forward_style,
			folder, message_uid);

		e_mail_reader_composer_created (
			async_context->reader, composer, message);
	}

	g_hash_table_unref (hash_table);

	e_activity_set_state (activity, E_ACTIVITY_COMPLETED);

exit:
	async_context_free (async_context);
}

void
e_mail_reader_forward_messages (EMailReader *reader,
                                CamelFolder *folder,
                                GPtrArray *uids,
                                EMailForwardStyle style)
{
	EActivity *activity;
	GCancellable *cancellable;
	AsyncContext *async_context;

	g_return_if_fail (E_IS_MAIL_READER (reader));
	g_return_if_fail (CAMEL_IS_FOLDER (folder));
	g_return_if_fail (uids != NULL);

	activity = e_mail_reader_new_activity (reader);
	cancellable = e_activity_get_cancellable (activity);

	async_context = g_slice_new0 (AsyncContext);
	async_context->activity = g_object_ref (activity);
	async_context->reader = g_object_ref (reader);
	async_context->uids = g_ptr_array_ref (uids);
	async_context->forward_style = style;

	switch (style) {
		case E_MAIL_FORWARD_STYLE_ATTACHED:
			e_mail_folder_build_attachment (
				folder, uids,
				G_PRIORITY_DEFAULT,
				cancellable,
				mail_reader_forward_attachment_cb,
				async_context);
			break;

		case E_MAIL_FORWARD_STYLE_INLINE:
		case E_MAIL_FORWARD_STYLE_QUOTED:
			e_mail_folder_get_multiple_messages (
				folder, uids,
				G_PRIORITY_DEFAULT,
				cancellable,
				mail_reader_forward_messages_cb,
				async_context);
			break;

		default:
			g_warn_if_reached ();
	}

	g_object_unref (activity);
}

/* Helper for e_mail_reader_reply_to_message()
 * XXX This function belongs in e-html-utils.c */
static gboolean
html_contains_nonwhitespace (const gchar *html,
                             gint len)
{
	const gchar *cp;
	gunichar uc = 0;

	if (html == NULL || len <= 0)
		return FALSE;

	cp = html;

	while (cp != NULL && cp - html < len) {
		uc = g_utf8_get_char (cp);
		if (uc == 0)
			break;

		if (uc == '<') {
			/* skip until next '>' */
			uc = g_utf8_get_char (cp);
			while (uc != 0 && uc != '>' && cp - html < len) {
				cp = g_utf8_next_char (cp);
				uc = g_utf8_get_char (cp);
			}
			if (uc == 0)
				break;
		} else if (uc == '&') {
			/* sequence '&nbsp;' is a space */
			if (g_ascii_strncasecmp (cp, "&nbsp;", 6) == 0)
				cp = cp + 5;
			else
				break;
		} else if (!g_unichar_isspace (uc))
			break;

		cp = g_utf8_next_char (cp);
	}

	return cp - html < len - 1 && uc != 0;
}

static void
mail_reader_reply_message_parsed (GObject *object,
                                  GAsyncResult *result,
                                  gpointer user_data)
{
	EShell *shell;
	EMailBackend *backend;
	EMailReader *reader = E_MAIL_READER (object);
	EMailPartList *part_list;
	EMsgComposer *composer;
	CamelMimeMessage *message;
	AsyncContext *async_context;
	GError *local_error = NULL;

	async_context = (AsyncContext *) user_data;

	part_list = e_mail_reader_parse_message_finish (reader, result, &local_error);

	if (local_error) {
		g_warn_if_fail (g_error_matches (local_error, G_IO_ERROR, G_IO_ERROR_CANCELLED));

		g_clear_error (&local_error);
		async_context_free (async_context);

		return;
	}

	message = e_mail_part_list_get_message (part_list);

	backend = e_mail_reader_get_backend (async_context->reader);
	shell = e_shell_backend_get_shell (E_SHELL_BACKEND (backend));

	composer = em_utils_reply_to_message (
		shell, message,
		async_context->folder,
		async_context->message_uid,
		async_context->reply_type,
		async_context->reply_style,
		part_list,
		async_context->address);

	e_mail_reader_composer_created (reader, composer, message);

	g_object_unref (part_list);

	async_context_free (async_context);
}

static void
mail_reader_get_message_ready_cb (GObject *source_object,
                                  GAsyncResult *result,
                                  gpointer user_data)
{
	EActivity *activity;
	EAlertSink *alert_sink;
	GCancellable *cancellable;
	CamelMimeMessage *message;
	AsyncContext *async_context;
	GError *local_error = NULL;

	async_context = (AsyncContext *) user_data;

	activity = async_context->activity;
	alert_sink = e_activity_get_alert_sink (activity);
	cancellable = e_activity_get_cancellable (activity);

	message = camel_folder_get_message_finish (
		CAMEL_FOLDER (source_object), result, &local_error);

	/* Sanity check. */
	g_return_if_fail (
		((message != NULL) && (local_error == NULL)) ||
		((message == NULL) && (local_error != NULL)));

	if (e_activity_handle_cancellation (activity, local_error)) {
		async_context_free (async_context);
		g_error_free (local_error);
		return;

	} else if (local_error != NULL) {
		e_alert_submit (
			alert_sink, "mail:no-retrieve-message",
			local_error->message, NULL);
		async_context_free (async_context);
		g_error_free (local_error);
		return;
	}

	e_mail_reader_parse_message (
		async_context->reader,
		async_context->folder,
		async_context->message_uid,
		message,
		cancellable,
		mail_reader_reply_message_parsed,
		async_context);

	g_object_unref (message);
}

void
e_mail_reader_reply_to_message (EMailReader *reader,
                                CamelMimeMessage *src_message,
                                EMailReplyType reply_type)
{
	EShell *shell;
	EMailBackend *backend;
	EShellBackend *shell_backend;
	EMailDisplay *display;
	EMailPartList *part_list = NULL;
	GtkWidget *message_list;
	CamelMimeMessage *new_message;
	CamelInternetAddress *address = NULL;
	CamelFolder *folder;
	EMailReplyStyle reply_style;
	EWebView *web_view;
	struct _camel_header_raw *header;
	const gchar *uid;
	gchar *selection = NULL;
	gint length;
	gchar *mail_uri;
	CamelObjectBag *registry;
	EMsgComposer *composer;
	EMailPartValidityFlags validity_pgp_sum = 0;
	EMailPartValidityFlags validity_smime_sum = 0;

	/* This handles quoting only selected text in the reply.  If
	 * nothing is selected or only whitespace is selected, fall
	 * back to the normal em_utils_reply_to_message(). */

	g_return_if_fail (E_IS_MAIL_READER (reader));

	backend = e_mail_reader_get_backend (reader);
	display = e_mail_reader_get_mail_display (reader);
	message_list = e_mail_reader_get_message_list (reader);
	reply_style = e_mail_reader_get_reply_style (reader);

	shell_backend = E_SHELL_BACKEND (backend);
	shell = e_shell_backend_get_shell (shell_backend);

	web_view = E_WEB_VIEW (display);

	if (reply_type == E_MAIL_REPLY_TO_RECIPIENT) {
		const gchar *uri;

		uri = e_web_view_get_selected_uri (web_view);

		if (uri) {
			CamelURL *curl;

			curl = camel_url_new (uri, NULL);

			if (curl && curl->path && *curl->path) {
				address = camel_internet_address_new ();
				if (camel_address_decode (
						CAMEL_ADDRESS (address),
						curl->path) < 0) {
					g_object_unref (address);
					address = NULL;
				}
			}

			if (curl)
				camel_url_free (curl);
		}
	}

	uid = MESSAGE_LIST (message_list)->cursor_uid;
	g_return_if_fail (uid != NULL);

	folder = e_mail_reader_ref_folder (reader);

	if (!gtk_widget_get_visible (GTK_WIDGET (web_view)))
		goto whole_message;

	registry = e_mail_part_list_get_registry ();
	mail_uri = e_mail_part_build_uri (folder, uid, NULL, NULL);
	part_list = camel_object_bag_get (registry, mail_uri);
	g_free (mail_uri);

	if (!part_list) {
		goto whole_message;
	} else {
		GQueue queue = G_QUEUE_INIT;

		e_mail_part_list_queue_parts (part_list, NULL, &queue);

		while (!g_queue_is_empty (&queue)) {
			EMailPart *part = g_queue_pop_head (&queue);
			GList *head, *link;

			head = g_queue_peek_head_link (&part->validities);

			for (link = head; link != NULL; link = g_list_next (link)) {
				EMailPartValidityPair *vpair = link->data;

				if (vpair == NULL)
					continue;

				if ((vpair->validity_type & E_MAIL_PART_VALIDITY_PGP) != 0)
					validity_pgp_sum |= vpair->validity_type;
				if ((vpair->validity_type & E_MAIL_PART_VALIDITY_SMIME) != 0)
					validity_smime_sum |= vpair->validity_type;
			}

			g_object_unref (part);
		}
	}

	if (src_message == NULL) {
		src_message = e_mail_part_list_get_message (part_list);
		g_return_if_fail (src_message != NULL);
	}

	g_clear_object (&part_list);

	if (!e_web_view_is_selection_active (web_view))
		goto whole_message;

	selection = e_web_view_get_selection_html (web_view);
	if (selection == NULL || *selection == '\0')
		goto whole_message;

	length = strlen (selection);
	if (!html_contains_nonwhitespace (selection, length))
		goto whole_message;

	new_message = camel_mime_message_new ();

	/* Filter out "content-*" headers. */
	header = CAMEL_MIME_PART (src_message)->headers;
	while (header != NULL) {
		if (g_ascii_strncasecmp (header->name, "content-", 8) != 0)
			camel_medium_add_header (
				CAMEL_MEDIUM (new_message),
				header->name, header->value);

		header = header->next;
	}

	camel_medium_add_header (
		CAMEL_MEDIUM (new_message),
		"X-Evolution-Content-Source", "selection");

	camel_mime_part_set_encoding (
		CAMEL_MIME_PART (new_message),
		CAMEL_TRANSFER_ENCODING_8BIT);

	camel_mime_part_set_content (
		CAMEL_MIME_PART (new_message),
		selection, length, "text/html; charset=utf-8");

	composer = em_utils_reply_to_message (
		shell, new_message, folder, uid,
		reply_type, reply_style, NULL, address);
	if (validity_pgp_sum != 0 || validity_smime_sum != 0) {
		GtkToggleAction *action;

		if ((validity_pgp_sum & E_MAIL_PART_VALIDITY_PGP) != 0) {
			if ((validity_pgp_sum & E_MAIL_PART_VALIDITY_SIGNED) != 0) {
				action = GTK_TOGGLE_ACTION (E_COMPOSER_ACTION_PGP_SIGN (composer));
				gtk_toggle_action_set_active (action, TRUE);
			}

			if ((validity_pgp_sum & E_MAIL_PART_VALIDITY_ENCRYPTED) != 0) {
				action = GTK_TOGGLE_ACTION (E_COMPOSER_ACTION_PGP_ENCRYPT (composer));
				gtk_toggle_action_set_active (action, TRUE);
			}
		}

		if ((validity_smime_sum & E_MAIL_PART_VALIDITY_SMIME) != 0) {
			if ((validity_smime_sum & E_MAIL_PART_VALIDITY_SIGNED) != 0) {
				action = GTK_TOGGLE_ACTION (E_COMPOSER_ACTION_SMIME_SIGN (composer));
				gtk_toggle_action_set_active (action, TRUE);
			}

			if ((validity_smime_sum & E_MAIL_PART_VALIDITY_ENCRYPTED) != 0) {
				action = GTK_TOGGLE_ACTION (E_COMPOSER_ACTION_SMIME_ENCRYPT (composer));
				gtk_toggle_action_set_active (action, TRUE);
			}
		}
	}

	e_mail_reader_composer_created (reader, composer, new_message);

	g_object_unref (new_message);

	g_free (selection);

	goto exit;

whole_message:
	if (src_message == NULL) {
		EActivity *activity;
		GCancellable *cancellable;
		AsyncContext *async_context;

		activity = e_mail_reader_new_activity (reader);
		cancellable = e_activity_get_cancellable (activity);

		async_context = g_slice_new0 (AsyncContext);
		async_context->activity = g_object_ref (activity);
		async_context->folder = g_object_ref (folder);
		async_context->reader = g_object_ref (reader);
		async_context->message_uid = g_strdup (uid);
		async_context->reply_type = reply_type;
		async_context->reply_style = reply_style;

		if (address != NULL)
			async_context->address = g_object_ref (address);

		camel_folder_get_message (
			async_context->folder,
			async_context->message_uid,
			G_PRIORITY_DEFAULT,
			cancellable,
			mail_reader_get_message_ready_cb,
			async_context);

		g_object_unref (activity);

	} else {
		composer = em_utils_reply_to_message (
			shell, src_message, folder, uid,
			reply_type, reply_style, part_list, address);

		e_mail_reader_composer_created (reader, composer, src_message);
	}

exit:
	g_clear_object (&address);
	g_clear_object (&folder);
}

static void
mail_reader_save_messages_cb (GObject *source_object,
                              GAsyncResult *result,
                              gpointer user_data)
{
	EActivity *activity;
	EAlertSink *alert_sink;
	AsyncContext *async_context;
	GError *local_error = NULL;

	async_context = (AsyncContext *) user_data;

	activity = async_context->activity;
	alert_sink = e_activity_get_alert_sink (activity);

	e_mail_folder_save_messages_finish (
		CAMEL_FOLDER (source_object), result, &local_error);

	if (e_activity_handle_cancellation (activity, local_error)) {
		g_error_free (local_error);

	} else if (local_error != NULL) {
		e_alert_submit (
			alert_sink,
			"mail:save-messages",
			local_error->message, NULL);
		g_error_free (local_error);
	}

	async_context_free (async_context);
}

void
e_mail_reader_save_messages (EMailReader *reader)
{
	EShell *shell;
	EActivity *activity;
	EMailBackend *backend;
	GCancellable *cancellable;
	AsyncContext *async_context;
	EShellBackend *shell_backend;
	CamelMessageInfo *info;
	CamelFolder *folder;
	GFile *destination;
	GPtrArray *uids;
	const gchar *message_uid;
	const gchar *title;
	gchar *suggestion = NULL;

	folder = e_mail_reader_ref_folder (reader);
	backend = e_mail_reader_get_backend (reader);

	uids = e_mail_reader_get_selected_uids (reader);
	g_return_if_fail (uids != NULL && uids->len > 0);

	if (uids->len > 1) {
		GtkWidget *message_list;

		message_list = e_mail_reader_get_message_list (reader);
		message_list_sort_uids (MESSAGE_LIST (message_list), uids);
	}

	message_uid = g_ptr_array_index (uids, 0);

	title = ngettext ("Save Message", "Save Messages", uids->len);

	/* Suggest as a filename the subject of the first message. */
	info = camel_folder_get_message_info (folder, message_uid);
	if (info != NULL) {
		const gchar *subject;

		subject = camel_message_info_subject (info);
		if (subject != NULL)
			suggestion = g_strconcat (subject, ".mbox", NULL);
		camel_message_info_unref (info);
	}

	if (suggestion == NULL) {
		const gchar *basename;

		/* Translators: This is part of a suggested file name
		 * used when saving a message or multiple messages to
		 * mbox format, when the first message doesn't have a
		 * subject.  The extension ".mbox" is appended to the
		 * string; for example "Message.mbox". */
		basename = ngettext ("Message", "Messages", uids->len);
		suggestion = g_strconcat (basename, ".mbox", NULL);
	}

	shell_backend = E_SHELL_BACKEND (backend);
	shell = e_shell_backend_get_shell (shell_backend);

	destination = e_shell_run_save_dialog (
		shell, title, suggestion,
		"*.mbox:application/mbox,message/rfc822", NULL, NULL);

	if (destination == NULL)
		goto exit;

	/* Save messages asynchronously. */

	activity = e_mail_reader_new_activity (reader);
	cancellable = e_activity_get_cancellable (activity);

	async_context = g_slice_new0 (AsyncContext);
	async_context->activity = g_object_ref (activity);
	async_context->reader = g_object_ref (reader);

	e_mail_folder_save_messages (
		folder, uids,
		destination,
		G_PRIORITY_DEFAULT,
		cancellable,
		mail_reader_save_messages_cb,
		async_context);

	g_object_unref (activity);

	g_object_unref (destination);

exit:
	g_clear_object (&folder);
	g_ptr_array_unref (uids);
}

void
e_mail_reader_select_next_message (EMailReader *reader,
                                   gboolean or_else_previous)
{
	GtkWidget *message_list;
	gboolean hide_deleted;
	gboolean success;

	g_return_if_fail (E_IS_MAIL_READER (reader));

	hide_deleted = e_mail_reader_get_hide_deleted (reader);
	message_list = e_mail_reader_get_message_list (reader);

	success = message_list_select (
		MESSAGE_LIST (message_list),
		MESSAGE_LIST_SELECT_NEXT, 0, 0);

	if (!success && (hide_deleted || or_else_previous))
		message_list_select (
			MESSAGE_LIST (message_list),
			MESSAGE_LIST_SELECT_PREVIOUS, 0, 0);
}

/* Helper for e_mail_reader_create_filter_from_selected() */
static void
mail_reader_create_filter_cb (GObject *source_object,
                              GAsyncResult *result,
                              gpointer user_data)
{
	EActivity *activity;
	EMailBackend *backend;
	EMailSession *session;
	EAlertSink *alert_sink;
	CamelMimeMessage *message;
	AsyncContext *async_context;
	GError *local_error = NULL;

	async_context = (AsyncContext *) user_data;

	activity = async_context->activity;
	alert_sink = e_activity_get_alert_sink (activity);

	message = camel_folder_get_message_finish (
		CAMEL_FOLDER (source_object), result, &local_error);

	/* Sanity check. */
	g_return_if_fail (
		((message != NULL) && (local_error == NULL)) ||
		((message == NULL) && (local_error != NULL)));

	if (e_activity_handle_cancellation (activity, local_error)) {
		async_context_free (async_context);
		g_error_free (local_error);
		return;

	} else if (local_error != NULL) {
		e_alert_submit (
			alert_sink, "mail:no-retrieve-message",
			local_error->message, NULL);
		async_context_free (async_context);
		g_error_free (local_error);
		return;
	}

	/* Finalize the activity here so we don't leave a message
	 * in the task bar while displaying the filter editor. */
	e_activity_set_state (activity, E_ACTIVITY_COMPLETED);
	g_clear_object (&async_context->activity);

	backend = e_mail_reader_get_backend (async_context->reader);
	session = e_mail_backend_get_session (backend);

	/* Switch to Incoming filter in case the message contains a Received header */
	if (g_str_equal (async_context->filter_source, E_FILTER_SOURCE_OUTGOING) &&
	    camel_medium_get_header (CAMEL_MEDIUM (message), "received"))
		async_context->filter_source = E_FILTER_SOURCE_INCOMING;

	filter_gui_add_from_message (
		session, message,
		async_context->filter_source,
		async_context->filter_type);

	g_object_unref (message);

	async_context_free (async_context);
}

void
e_mail_reader_create_filter_from_selected (EMailReader *reader,
                                           gint filter_type)
{
	EShell *shell;
	EActivity *activity;
	EMailBackend *backend;
	AsyncContext *async_context;
	GCancellable *cancellable;
	ESourceRegistry *registry;
	CamelFolder *folder;
	GPtrArray *uids;
	const gchar *filter_source;
	const gchar *message_uid;

	g_return_if_fail (E_IS_MAIL_READER (reader));

	backend = e_mail_reader_get_backend (reader);
	shell = e_shell_backend_get_shell (E_SHELL_BACKEND (backend));
	registry = e_shell_get_registry (shell);

	folder = e_mail_reader_ref_folder (reader);
	g_return_if_fail (folder != NULL);

	if (em_utils_folder_is_sent (registry, folder) ||
	    em_utils_folder_is_outbox (registry, folder))
		filter_source = E_FILTER_SOURCE_OUTGOING;
	else
		filter_source = E_FILTER_SOURCE_INCOMING;

	uids = e_mail_reader_get_selected_uids (reader);
	g_return_if_fail (uids != NULL && uids->len == 1);
	message_uid = g_ptr_array_index (uids, 0);

	activity = e_mail_reader_new_activity (reader);
	cancellable = e_activity_get_cancellable (activity);

	async_context = g_slice_new0 (AsyncContext);
	async_context->activity = g_object_ref (activity);
	async_context->reader = g_object_ref (reader);
	async_context->filter_source = filter_source;
	async_context->filter_type = filter_type;

	camel_folder_get_message (
		folder, message_uid,
		G_PRIORITY_DEFAULT,
		cancellable,
		mail_reader_create_filter_cb,
		async_context);

	g_object_unref (activity);

	g_ptr_array_unref (uids);

	g_object_unref (folder);
}

/* Helper for e_mail_reader_create_vfolder_from_selected() */
static void
mail_reader_create_vfolder_cb (GObject *source_object,
                               GAsyncResult *result,
                               gpointer user_data)
{
	EActivity *activity;
	EMailBackend *backend;
	EMailSession *session;
	EAlertSink *alert_sink;
	CamelMimeMessage *message;
	CamelFolder *use_folder;
	AsyncContext *async_context;
	GError *local_error = NULL;

	async_context = (AsyncContext *) user_data;

	activity = async_context->activity;
	alert_sink = e_activity_get_alert_sink (activity);

	message = camel_folder_get_message_finish (
		CAMEL_FOLDER (source_object), result, &local_error);

	/* Sanity check. */
	g_return_if_fail (
		((message != NULL) && (local_error == NULL)) ||
		((message == NULL) && (local_error != NULL)));

	if (e_activity_handle_cancellation (activity, local_error)) {
		async_context_free (async_context);
		g_error_free (local_error);
		return;

	} else if (local_error != NULL) {
		e_alert_submit (
			alert_sink, "mail:no-retrieve-message",
			local_error->message, NULL);
		async_context_free (async_context);
		g_error_free (local_error);
		return;
	}

	/* Finalize the activity here so we don't leave a message
	 * in the task bar while displaying the vfolder editor. */
	e_activity_set_state (activity, E_ACTIVITY_COMPLETED);
	g_clear_object (&async_context->activity);

	backend = e_mail_reader_get_backend (async_context->reader);
	session = e_mail_backend_get_session (backend);

	use_folder = async_context->folder;
	if (CAMEL_IS_VEE_FOLDER (use_folder)) {
		CamelStore *parent_store;
		CamelVeeFolder *vfolder;

		parent_store = camel_folder_get_parent_store (use_folder);
		vfolder = CAMEL_VEE_FOLDER (use_folder);

		if (CAMEL_IS_VEE_STORE (parent_store) &&
		    vfolder == camel_vee_store_get_unmatched_folder (CAMEL_VEE_STORE (parent_store))) {
			/* use source folder instead of the Unmatched folder */
			use_folder = camel_vee_folder_get_vee_uid_folder (
				vfolder, async_context->message_uid);
		}
	}

	vfolder_gui_add_from_message (
		session, message,
		async_context->filter_type,
		use_folder);

	g_object_unref (message);

	async_context_free (async_context);
}

void
e_mail_reader_create_vfolder_from_selected (EMailReader *reader,
                                            gint vfolder_type)
{
	EActivity *activity;
	GCancellable *cancellable;
	AsyncContext *async_context;
	GPtrArray *uids;
	const gchar *message_uid;

	g_return_if_fail (E_IS_MAIL_READER (reader));

	uids = e_mail_reader_get_selected_uids (reader);
	g_return_if_fail (uids != NULL && uids->len == 1);
	message_uid = g_ptr_array_index (uids, 0);

	activity = e_mail_reader_new_activity (reader);
	cancellable = e_activity_get_cancellable (activity);

	async_context = g_slice_new0 (AsyncContext);
	async_context->activity = g_object_ref (activity);
	async_context->folder = e_mail_reader_ref_folder (reader);
	async_context->reader = g_object_ref (reader);
	async_context->message_uid = g_strdup (message_uid);
	async_context->filter_type = vfolder_type;

	camel_folder_get_message (
		async_context->folder,
		async_context->message_uid,
		G_PRIORITY_DEFAULT,
		cancellable,
		mail_reader_create_vfolder_cb,
		async_context);

	g_object_unref (activity);

	g_ptr_array_unref (uids);
}

static void
mail_reader_parse_message_run (GSimpleAsyncResult *simple,
                               GObject *object,
                               GCancellable *cancellable)
{
	EMailReader *reader = E_MAIL_READER (object);
	CamelObjectBag *registry;
	EMailPartList *part_list;
	AsyncContext *async_context;
	gchar *mail_uri;
	GError *local_error = NULL;

	async_context = g_simple_async_result_get_op_res_gpointer (simple);

	registry = e_mail_part_list_get_registry ();

	mail_uri = e_mail_part_build_uri (
		async_context->folder,
		async_context->message_uid, NULL, NULL);

	part_list = camel_object_bag_reserve (registry, mail_uri);
	if (part_list == NULL) {
		EMailBackend *mail_backend;
		EMailSession *mail_session;
		EMailParser *parser;

		mail_backend = e_mail_reader_get_backend (reader);
		mail_session = e_mail_backend_get_session (mail_backend);

		parser = e_mail_parser_new (CAMEL_SESSION (mail_session));

		part_list = e_mail_parser_parse_sync (
			parser,
			async_context->folder,
			async_context->message_uid,
			async_context->message,
			cancellable);

		g_object_unref (parser);

		if (part_list == NULL)
			camel_object_bag_abort (registry, mail_uri);
		else
			camel_object_bag_add (registry, mail_uri, part_list);
	}

	g_free (mail_uri);

	async_context->part_list = part_list;

	if (g_cancellable_set_error_if_cancelled (cancellable, &local_error))
		g_simple_async_result_take_error (simple, local_error);
}

void
e_mail_reader_parse_message (EMailReader *reader,
                             CamelFolder *folder,
                             const gchar *message_uid,
                             CamelMimeMessage *message,
                             GCancellable *cancellable,
                             GAsyncReadyCallback callback,
                             gpointer user_data)
{
	GSimpleAsyncResult *simple;
	AsyncContext *async_context;
	EActivity *activity;

	g_return_if_fail (E_IS_MAIL_READER (reader));
	g_return_if_fail (CAMEL_IS_FOLDER (folder));
	g_return_if_fail (message_uid != NULL);
	g_return_if_fail (CAMEL_IS_MIME_MESSAGE (message));

	activity = e_mail_reader_new_activity (reader);
	e_activity_set_cancellable (activity, cancellable);
	e_activity_set_text (activity, _("Parsing message"));

	async_context = g_slice_new0 (AsyncContext);
	async_context->activity = g_object_ref (activity);
	async_context->folder = g_object_ref (folder);
	async_context->message_uid = g_strdup (message_uid);
	async_context->message = g_object_ref (message);

	simple = g_simple_async_result_new (
		G_OBJECT (reader), callback, user_data,
		e_mail_reader_parse_message);

	g_simple_async_result_set_check_cancellable (simple, cancellable);

	g_simple_async_result_set_op_res_gpointer (
		simple, async_context, (GDestroyNotify) async_context_free);

	g_simple_async_result_run_in_thread (
		simple, mail_reader_parse_message_run,
		G_PRIORITY_DEFAULT, cancellable);

	g_object_unref (simple);
	g_object_unref (activity);
}

EMailPartList *
e_mail_reader_parse_message_finish (EMailReader *reader,
                                    GAsyncResult *result,
				    GError **error)
{
	GSimpleAsyncResult *simple;
	AsyncContext *async_context;

	g_return_val_if_fail (
		g_simple_async_result_is_valid (
		result, G_OBJECT (reader),
		e_mail_reader_parse_message), NULL);

	simple = G_SIMPLE_ASYNC_RESULT (result);

	if (g_simple_async_result_propagate_error (simple, error))
		return NULL;

	async_context = g_simple_async_result_get_op_res_gpointer (simple);

	if (async_context->part_list != NULL)
		g_object_ref (async_context->part_list);

	return async_context->part_list;
}
