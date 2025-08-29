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

#include "evolution-config.h"

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
	GSList *hidden_parts; /* EMailPart, to have set ::is_hidden = FALSE; */
};

static void
async_context_free (AsyncContext *async_context)
{
	GSList *link;

	g_clear_object (&async_context->activity);
	g_clear_object (&async_context->folder);
	g_clear_object (&async_context->message);
	g_clear_object (&async_context->reader);
	g_clear_object (&async_context->address);

	if (async_context->uids != NULL)
		g_ptr_array_unref (async_context->uids);

	g_free (async_context->folder_name);
	g_free (async_context->message_uid);

	for (link = async_context->hidden_parts; link; link = g_slist_next (link)) {
		EMailPart *part = link->data;

		part->is_hidden = FALSE;
	}

	g_slist_free_full (async_context->hidden_parts, g_object_unref);
	async_context->hidden_parts = NULL;

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
		camel_folder_get_full_display_name (folder), NULL);

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
			camel_folder_get_full_display_name (folder),
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
		0, G_PRIORITY_DEFAULT, cancellable,
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
		0, G_PRIORITY_DEFAULT, cancellable,
		mail_reader_expunge_folder_name_cb,
		async_context);

	g_object_unref (activity);
}

static void
mail_reader_empty_junk_thread (EAlertSinkThreadJobData *job_data,
			       gpointer user_data,
			       GCancellable *cancellable,
			       GError **error)
{
	AsyncContext *async_context = user_data;
	CamelFolder *folder;
	CamelFolderSummary *summary;
	GPtrArray *uids;
	guint ii;

	g_return_if_fail (async_context != NULL);

	folder = async_context->folder;

	g_return_if_fail (CAMEL_IS_FOLDER (folder));

	camel_folder_freeze (folder);

	summary = camel_folder_get_folder_summary (folder);
	if (summary)
		camel_folder_summary_prepare_fetch_all (summary, NULL);

	uids = camel_folder_dup_uids (folder);
	if (uids) {
		for (ii = 0; ii < uids->len; ii++) {
			CamelMessageInfo *nfo;

			nfo = camel_folder_get_message_info (folder, uids->pdata[ii]);
			if (nfo) {
				camel_message_info_set_flags (nfo, CAMEL_MESSAGE_DELETED | CAMEL_MESSAGE_SEEN, CAMEL_MESSAGE_DELETED | CAMEL_MESSAGE_SEEN);
				g_object_unref (nfo);
			}
		}

		if (uids->len > 0)
			camel_folder_synchronize_sync (folder, FALSE, cancellable, error);

		g_ptr_array_unref (uids);
	}

	camel_folder_thaw (folder);
}

void
e_mail_reader_empty_junk_folder (EMailReader *reader,
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
	if (full_display_name)
		display_name = full_display_name;

	proceed = e_util_prompt_user (window, "org.gnome.evolution.mail", "prompt-on-empty-junk",
		"mail:ask-empty-junk", display_name, NULL);

	if (proceed) {
		AsyncContext *async_context;
		EAlertSink *alert_sink;
		EActivity *activity;
		gchar *description;

		alert_sink = e_mail_reader_get_alert_sink (reader);

		async_context = g_slice_new0 (AsyncContext);
		async_context->reader = g_object_ref (reader);
		async_context->folder = g_object_ref (folder);

		description = g_strdup_printf (_("Deleting messages in Junk folder “%s”…"), display_name);

		activity = e_alert_sink_submit_thread_job (alert_sink,
			description, "mail:failed-empty-junk", display_name,
			mail_reader_empty_junk_thread, async_context,
			(GDestroyNotify) async_context_free);

		g_clear_object (&activity);
		g_free (description);
	}

	g_free (full_display_name);
}

static void
mail_reader_empty_junk_folder_name_cb (GObject *source_object,
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

	folder = camel_store_get_folder_finish (CAMEL_STORE (source_object), result, &local_error);

	if (e_activity_handle_cancellation (activity, local_error)) {
		g_error_free (local_error);

	} else if (local_error != NULL) {
		e_alert_submit (
			alert_sink, "mail:failed-empty-junk",
			async_context->folder_name,
			local_error->message, NULL);
		g_error_free (local_error);

	} else {
		e_activity_set_state (activity, E_ACTIVITY_COMPLETED);
		e_mail_reader_empty_junk_folder (async_context->reader, folder);
	}

	async_context_free (async_context);
	g_clear_object (&folder);
}

void
e_mail_reader_empty_junk_folder_name (EMailReader *reader,
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
		0, G_PRIORITY_DEFAULT, cancellable,
		mail_reader_empty_junk_folder_name_cb,
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
		_("Refreshing folder “%s”"),
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
		CAMEL_STORE_FOLDER_INFO_FAST | CAMEL_STORE_FOLDER_INFO_REFRESH,
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

		uids = e_mail_reader_get_selected_uids_with_collapsed_threads (reader);

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

typedef struct {
	CamelFolder *folder;
	GPtrArray *uids;
	EIgnoreThreadKind kind;
	gboolean thread_subject;
} MarkIgnoreThreadData;

static void
mark_ignore_thread_data_free (gpointer ptr)
{
	MarkIgnoreThreadData *mit = ptr;

	if (mit) {
		g_clear_object (&mit->folder);
		g_clear_pointer (&mit->uids, g_ptr_array_unref);
		g_slice_free (MarkIgnoreThreadData, mit);
	}
}

static void
mail_reader_utils_mark_ignore_thread_thread (EAlertSinkThreadJobData *job_data,
					     gpointer user_data,
					     GCancellable *cancellable,
					     GError **error)
{
	MarkIgnoreThreadData *mit = user_data;
	GHashTable *covered_uids; /* gchar * (UID) ~> UID */
	GPtrArray *res_uids = NULL;
	GString *expr;
	gboolean ignore_thread, whole_thread, fetched_all;
	guint ii;

	g_return_if_fail (mit != NULL);

	camel_folder_freeze (mit->folder);

	whole_thread = mit->kind == E_IGNORE_THREAD_WHOLE_SET || mit->kind == E_IGNORE_THREAD_WHOLE_UNSET;
	ignore_thread = mit->kind == E_IGNORE_THREAD_WHOLE_SET || mit->kind == E_IGNORE_THREAD_SUBSET_SET;

	fetched_all = mit->uids->len > 50;

	if (fetched_all) {
		CamelFolderSummary *folder_summary = camel_folder_get_folder_summary (mit->folder);

		if (folder_summary)
			camel_folder_summary_prepare_fetch_all (folder_summary, NULL);
	}

	covered_uids = g_hash_table_new (g_str_hash, g_str_equal);

	expr = g_string_sized_new (128);
	g_string_append_printf (expr, "(match-threads \"%s%s\" (uid",
		mit->thread_subject ? "" : "no-subject,",
		whole_thread ? "all" : "replies");

	for (ii = 0; ii < mit->uids->len; ii++) {
		const gchar *uid = g_ptr_array_index (mit->uids, ii);
		CamelMessageInfo *mi;

		g_string_append_printf (expr, " \"%s\"", uid);

		if (g_hash_table_contains (covered_uids, uid))
			continue;

		g_hash_table_add (covered_uids, (gpointer) uid);

		mi = camel_folder_get_message_info (mit->folder, uid);
		if (mi)
			camel_message_info_set_user_flag (mi, "ignore-thread", ignore_thread);

		g_clear_object (&mi);
	}

	g_string_append (expr, "))");

	if (camel_folder_search_sync (mit->folder, expr->str, &res_uids, cancellable, error) && res_uids) {
		if (!fetched_all && res_uids->len > 50) {
			CamelFolderSummary *folder_summary = camel_folder_get_folder_summary (mit->folder);

			if (folder_summary)
				camel_folder_summary_prepare_fetch_all (folder_summary, NULL);

			fetched_all = TRUE;
		}

		for (ii = 0; ii < res_uids->len; ii++) {
			const gchar *uid = g_ptr_array_index (res_uids, ii);
			CamelMessageInfo *mi;

			if (g_hash_table_contains (covered_uids, uid))
				continue;

			g_hash_table_add (covered_uids, (gpointer) uid);

			mi = camel_folder_get_message_info (mit->folder, uid);
			if (mi)
				camel_message_info_set_user_flag (mi, "ignore-thread", ignore_thread);

			g_clear_object (&mi);
		}
	}

	camel_folder_thaw (mit->folder);

	g_hash_table_destroy (covered_uids);
	/* the covered_uids has borrowed the uids from the res_uids, thus free it after the covered_uids */
	g_clear_pointer (&res_uids, g_ptr_array_unref);
	g_string_free (expr, TRUE);
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

		uids = e_mail_reader_get_selected_uids_with_collapsed_threads (reader);
		if (uids && uids->len > 0) {
			MarkIgnoreThreadData *mit;
			GtkWidget *message_list;
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

			message_list = e_mail_reader_get_message_list (reader);

			mit = g_slice_new0 (MarkIgnoreThreadData);
			mit->folder = g_object_ref (folder);
			mit->kind = kind;
			mit->thread_subject = message_list_get_thread_subject (MESSAGE_LIST (message_list));
			mit->uids = g_ptr_array_ref (uids);

			alert_sink = e_mail_reader_get_alert_sink (reader);

			activity = e_alert_sink_submit_thread_job (alert_sink, description, alert_id,
				camel_folder_get_full_display_name (folder), mail_reader_utils_mark_ignore_thread_thread,
				mit, mark_ignore_thread_data_free);


			if (activity)
				e_shell_backend_add_activity (E_SHELL_BACKEND (e_mail_reader_get_backend (reader)), activity);

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
	gboolean prefer_existing;

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

	prefer_existing = !E_IS_MAIL_BROWSER (window);

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

		g_clear_object (&info);
	}

	for (ii = 0; ii < views->len; ii++) {
		const gchar *uid = views->pdata[ii];
		GtkWidget *browser;
		EMailReader *browser_reader;
		MessageList *ml;

		if (prefer_existing) {
			EMailBrowser *mail_browser;

			mail_browser = em_utils_find_message_window (E_MAIL_FORMATTER_MODE_NORMAL, folder, uid);

			if (mail_browser) {
				gtk_window_present (GTK_WINDOW (mail_browser));
				continue;
			}
		}

		browser = e_mail_browser_new (backend, E_MAIL_FORMATTER_MODE_NORMAL);
		browser_reader = E_MAIL_READER (browser);

		ml = MESSAGE_LIST (e_mail_reader_get_message_list (browser_reader));
		message_list_freeze (ml);

		e_mail_reader_set_folder (browser_reader, folder);
		e_mail_reader_set_message (browser_reader, uid);

		copy_tree_state (reader, browser_reader);
		e_mail_reader_set_group_by_threads (browser_reader,
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

	em_utils_print_part_list_finish (source_object, result, &local_error);

	if (e_activity_handle_cancellation (activity, local_error)) {
		g_error_free (local_error);

	} else if (local_error != NULL) {
		e_alert_submit (
			alert_sink, "mail:printing-failed",
			local_error->message, NULL);
		g_error_free (local_error);

	} else {
		/* Set activity as completed, and keep it displayed for a few
		 * seconds so that user can actually see the printing was
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
	EActivity *activity;
	GCancellable *cancellable;
	EMailPartList *part_list;
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

	e_activity_set_text (activity, _("Printing"));

	em_utils_print_part_list (part_list, mail_display, async_context->print_action,
		cancellable, mail_reader_print_message_cb, async_context);

	g_clear_object (&part_list);
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
			"Folder “%s” contains %u duplicate message. "
			"Are you sure you want to delete it?",
			"Folder “%s” contains %u duplicate messages. "
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

	uids = e_mail_reader_get_selected_uids_with_collapsed_threads (reader);
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

static gboolean
emr_utils_get_skip_insecure_parts (EMailReader *reader)
{
	if (!reader)
		return TRUE;

	return e_mail_display_get_skip_insecure_parts (e_mail_reader_get_mail_display (reader));
}

typedef struct _CreateComposerData {
	EMailReader *reader;
	CamelFolder *folder;
	CamelMimeMessage *message;
	const gchar *message_uid; /* Allocated on the string pool, use camel_pstring_strdup/free */
	gboolean keep_signature;
	gboolean replace;

	EMailPartList *part_list;
	EMailReplyType reply_type;
	EMailReplyStyle reply_style;
	CamelInternetAddress *address;
	EMailPartValidityFlags validity_pgp_sum;
	EMailPartValidityFlags validity_smime_sum;
	gboolean is_selection;
	gboolean skip_insecure_parts;

	EMailForwardStyle forward_style;

	CamelMimePart *attached_part;
	gchar *attached_subject;
	GPtrArray *attached_uids;
} CreateComposerData;

static CreateComposerData *
create_composer_data_new (void)
{
	return g_slice_new0 (CreateComposerData);
}

static void
create_composer_data_free (CreateComposerData *ccd)
{
	if (ccd) {
		if (ccd->attached_uids)
			g_ptr_array_unref (ccd->attached_uids);

		g_clear_object (&ccd->reader);
		g_clear_object (&ccd->folder);
		g_clear_object (&ccd->message);
		g_clear_object (&ccd->part_list);
		g_clear_object (&ccd->address);
		g_clear_object (&ccd->attached_part);
		camel_pstring_free (ccd->message_uid);
		g_free (ccd->attached_subject);
		g_slice_free (CreateComposerData, ccd);
	}
}

static void
mail_reader_edit_messages_composer_created_cb (GObject *source_object,
					       GAsyncResult *result,
					       gpointer user_data)
{
	CreateComposerData *ccd = user_data;
	EMsgComposer *composer;
	GError *error = NULL;

	g_return_if_fail (ccd != NULL);

	composer = e_msg_composer_new_finish (result, &error);
	if (error) {
		g_warning ("%s: Failed to create msg composer: %s", G_STRFUNC, error->message);
		g_clear_error (&error);
	} else {
		camel_medium_remove_header (CAMEL_MEDIUM (ccd->message), "User-Agent");
		camel_medium_remove_header (CAMEL_MEDIUM (ccd->message), "X-Mailer");
		camel_medium_remove_header (CAMEL_MEDIUM (ccd->message), "X-Newsreader");
		camel_medium_remove_header (CAMEL_MEDIUM (ccd->message), "X-MimeOLE");

		em_utils_edit_message (
			composer, ccd->folder, ccd->message, ccd->message_uid,
			ccd->keep_signature, ccd->replace);

		e_mail_reader_composer_created (
			ccd->reader, composer, ccd->message);
	}

	create_composer_data_free (ccd);
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
	gboolean skip_insecure_parts;
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
	skip_insecure_parts = emr_utils_get_skip_insecure_parts (async_context->reader);

	/* Open each message in its own composer window. */

	g_hash_table_iter_init (&iter, hash_table);

	while (g_hash_table_iter_next (&iter, &key, &value)) {
		CreateComposerData *ccd;

		ccd = create_composer_data_new ();
		ccd->reader = g_object_ref (async_context->reader);
		ccd->folder = g_object_ref (folder);
		ccd->message = g_object_ref (CAMEL_MIME_MESSAGE (value));
		ccd->message_uid = camel_pstring_strdup ((const gchar *) key);
		ccd->keep_signature = async_context->keep_signature;
		ccd->replace = async_context->replace;
		ccd->skip_insecure_parts = skip_insecure_parts;

		e_msg_composer_new (shell, mail_reader_edit_messages_composer_created_cb, ccd);
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
mail_reader_forward_attached_composer_created_cb (GObject *source_object,
						  GAsyncResult *result,
						  gpointer user_data)
{
	CreateComposerData *ccd = user_data;
	EMsgComposer *composer;
	GError *error = NULL;

	composer = e_msg_composer_new_finish (result, &error);
	if (error) {
		g_warning ("%s: Failed to create msg composer: %s", G_STRFUNC, error->message);
		g_clear_error (&error);
	} else {
		CamelDataWrapper *content;

		em_utils_forward_attachment (composer, ccd->attached_part, ccd->attached_subject, ccd->folder, ccd->attached_uids);

		content = camel_medium_get_content (CAMEL_MEDIUM (ccd->attached_part));
		if (CAMEL_IS_MIME_MESSAGE (content)) {
			e_mail_reader_composer_created (ccd->reader, composer, CAMEL_MIME_MESSAGE (content));
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
			e_mail_reader_composer_created (ccd->reader, composer, NULL);
		}
	}

	create_composer_data_free (ccd);
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
	EShell *shell;
	CamelMimePart *part;
	CreateComposerData *ccd;
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

	ccd = create_composer_data_new ();
	ccd->reader = g_object_ref (async_context->reader);
	ccd->folder = g_object_ref (folder);
	ccd->attached_part = part;
	ccd->attached_subject = subject;
	ccd->attached_uids = async_context->uids ? g_ptr_array_ref (async_context->uids) : NULL;
	ccd->skip_insecure_parts = emr_utils_get_skip_insecure_parts (async_context->reader);

	backend = e_mail_reader_get_backend (async_context->reader);
	shell = e_shell_backend_get_shell (E_SHELL_BACKEND (backend));

	e_msg_composer_new (shell, mail_reader_forward_attached_composer_created_cb, ccd);

	e_activity_set_state (activity, E_ACTIVITY_COMPLETED);

exit:
	async_context_free (async_context);
}

static void
mail_reader_forward_message_composer_created_cb (GObject *source_object,
						 GAsyncResult *result,
						 gpointer user_data)
{
	CreateComposerData *ccd = user_data;
	EMsgComposer *composer;
	GError *error = NULL;

	g_return_if_fail (ccd != NULL);

	composer = e_msg_composer_new_finish (result, &error);
	if (error) {
		g_warning ("%s: Failed to create msg composer: %s", G_STRFUNC, error->message);
		g_clear_error (&error);
	} else {
		em_utils_forward_message (
			composer, ccd->message,
			ccd->forward_style,
			ccd->folder, ccd->message_uid,
			ccd->skip_insecure_parts);

		e_mail_reader_composer_created (
			ccd->reader, composer, ccd->message);
	}

	create_composer_data_free (ccd);
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
	EShell *shell;
	GHashTable *hash_table;
	GHashTableIter iter;
	gpointer key, value;
	gboolean skip_insecure_parts;
	AsyncContext *async_context;
	GError *local_error = NULL;

	folder = CAMEL_FOLDER (source_object);
	async_context = (AsyncContext *) user_data;

	activity = async_context->activity;
	alert_sink = e_activity_get_alert_sink (activity);

	backend = e_mail_reader_get_backend (async_context->reader);
	shell = e_shell_backend_get_shell (E_SHELL_BACKEND (backend));

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

	skip_insecure_parts = emr_utils_get_skip_insecure_parts (async_context->reader);

	/* Create a new composer window for each message. */

	g_hash_table_iter_init (&iter, hash_table);

	while (g_hash_table_iter_next (&iter, &key, &value)) {
		CreateComposerData *ccd;
		CamelMimeMessage *message;
		const gchar *message_uid;

		message_uid = (const gchar *) key;
		message = CAMEL_MIME_MESSAGE (value);

		ccd = create_composer_data_new ();
		ccd->reader = g_object_ref (async_context->reader);
		ccd->folder = g_object_ref (folder);
		ccd->message = g_object_ref (message);
		ccd->message_uid = camel_pstring_strdup (message_uid);
		ccd->forward_style = async_context->forward_style;
		ccd->skip_insecure_parts = skip_insecure_parts;

		e_msg_composer_new (shell, mail_reader_forward_message_composer_created_cb, ccd);
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

static gboolean
plaintext_contains_nonwhitespace (const gchar *text,
				  gint len)
{
	const gchar *cp;
	gunichar uc = 0;

	if (!text || len <= 0)
		return FALSE;

	cp = text;

	while (cp != NULL && cp - text < len) {
		uc = g_utf8_get_char (cp);
		if (uc == 0)
			break;

		if (!g_unichar_isspace (uc))
			break;

		cp = g_utf8_next_char (cp);
	}

	return cp - text < len - 1 && uc != 0;
}

static void
maybe_mangle_plaintext_signature_delimiter (gchar **inout_text)
{
	GString *text;

	g_return_if_fail (inout_text != NULL);

	if (!*inout_text || (!strstr (*inout_text, "\n-- \n") && g_ascii_strncasecmp (*inout_text, "-- \n", 4)  != 0))
		return;

	text = e_str_replace_string (*inout_text, "\n-- \n", "\n--\n");
	if (!text)
		return;

	if (text->len >= 4 && g_ascii_strncasecmp (text->str, "-- \n", 4)  == 0) {
		/* Remove the space at the third byte */
		g_string_erase (text, 2, 1);
	}

	g_free (*inout_text);
	*inout_text = g_string_free (text, FALSE);
}

static void
mail_reader_reply_to_message_composer_created_cb (GObject *source_object,
						  GAsyncResult *result,
						  gpointer user_data)
{
	CreateComposerData *ccd = user_data;
	EMsgComposer *composer;
	GError *error = NULL;

	g_return_if_fail (ccd != NULL);

	composer = e_msg_composer_new_finish (result, &error);
	if (error) {
		g_warning ("%s: failed to create msg composer: %s", G_STRFUNC, error->message);
		g_clear_error (&error);
	} else {
		guint32 add_flags = 0;

		if (!ccd->is_selection && ccd->skip_insecure_parts)
			add_flags = E_MAIL_REPLY_FLAG_SKIP_INSECURE_PARTS;

		em_utils_reply_to_message (
			composer, ccd->message, ccd->folder, ccd->message_uid,
			ccd->reply_type, ccd->reply_style, ccd->is_selection ? NULL : ccd->part_list, ccd->address,
			(ccd->reply_type == E_MAIL_REPLY_TO_SENDER ? E_MAIL_REPLY_FLAG_FORCE_SENDER_REPLY : E_MAIL_REPLY_FLAG_NONE) | add_flags);

		em_composer_utils_update_security (composer, ccd->validity_pgp_sum, ccd->validity_smime_sum);

		e_mail_reader_composer_created (ccd->reader, composer, ccd->message);
	}

	create_composer_data_free (ccd);
}

static gboolean
extract_parts_with_content_id_cb (CamelMimeMessage *message,
				  CamelMimePart *part,
				  CamelMimePart *parent_part,
				  gpointer user_data)
{
	GPtrArray *cid_parts = user_data;
	const gchar *cid;

	cid = camel_mime_part_get_content_id (part);
	if (cid && *cid)
		g_ptr_array_add (cid_parts, g_object_ref (part));

	return TRUE;
}

static CamelMimeMessage *
e_mail_reader_utils_selection_as_message (CamelMimeMessage *src_message,
					  const gchar *selection,
					  gboolean selection_is_html)
{
	const CamelNameValueArray *headers;
	CamelMimeMessage *new_message;
	CamelMultipart *multipart_body = NULL;
	guint ii, len;
	gint length;

	if (!selection || !*selection)
		return NULL;

	length = strlen (selection);
	if ((selection_is_html && !html_contains_nonwhitespace (selection, length)) ||
	    (!selection_is_html && !plaintext_contains_nonwhitespace (selection, length))) {
		return NULL;
	}

	new_message = camel_mime_message_new ();

	if (src_message) {
		/* Filter out "content-*" headers. */
		headers = camel_medium_get_headers (CAMEL_MEDIUM (src_message));
		len = camel_name_value_array_get_length (headers);
		for (ii = 0; ii < len; ii++) {
			const gchar *header_name = NULL, *header_value = NULL;

			if (camel_name_value_array_get (headers, ii, &header_name, &header_value) &&
			    header_name &&
			    g_ascii_strncasecmp (header_name, "content-", 8) != 0) {
				camel_medium_add_header (CAMEL_MEDIUM (new_message), header_name, header_value);
			}
		}
	}

	camel_medium_add_header (
		CAMEL_MEDIUM (new_message),
		"X-Evolution-Content-Source", "selection");


	if (src_message && selection_is_html) {
		/* the HTML can contain embedded images, thus take them too; they are not
		   necessarily all of them, but parsing the HTML to know which they are
		   is more trouble than just adding all parts with Content-ID header */
		GPtrArray *cid_parts; /* CamelMimePart * */

		cid_parts = g_ptr_array_new_with_free_func (g_object_unref);
		camel_mime_message_foreach_part (src_message, extract_parts_with_content_id_cb, cid_parts);

		if (cid_parts->len > 0) {
			CamelMimePart *part;

			multipart_body = camel_multipart_new ();
			camel_data_wrapper_set_mime_type (CAMEL_DATA_WRAPPER (multipart_body), "multipart/related");
			camel_multipart_set_boundary (multipart_body, NULL);

			part = camel_mime_part_new ();
			camel_mime_part_set_encoding (part, CAMEL_TRANSFER_ENCODING_8BIT);
			camel_mime_part_set_content (part, selection, length,
				selection_is_html ? "text/html; charset=utf-8" : "text/plain; charset=utf-8");
			camel_multipart_add_part (multipart_body, part);
			g_clear_object (&part);

			for (ii = 0; ii < cid_parts->len; ii++) {
				part = g_ptr_array_index (cid_parts, ii);

				camel_multipart_add_part (multipart_body, part);
			}

			camel_medium_set_content (CAMEL_MEDIUM (new_message), CAMEL_DATA_WRAPPER (multipart_body));
		}

		g_ptr_array_unref (cid_parts);
	}

	if (!multipart_body) {
		camel_mime_part_set_encoding (CAMEL_MIME_PART (new_message), CAMEL_TRANSFER_ENCODING_8BIT);
		camel_mime_part_set_content (CAMEL_MIME_PART (new_message), selection, length,
			selection_is_html ? "text/html; charset=utf-8" : "text/plain; charset=utf-8");
	}

	g_clear_object (&multipart_body);

	return new_message;
}

typedef struct _SelectionOrMessageData {
	GTask *task; /* not referenced */
	EActivity *activity;
	CamelFolder *folder;
	CamelMimeMessage *preloaded_message;
	CamelMimeMessage *message;
	EMailPartList *part_list;
	EMailPartValidityFlags orig_validity_pgp_sum;
	EMailPartValidityFlags orig_validity_smime_sum;
	const gchar *message_uid; /* Allocated on the string pool, use camel_pstring_strdup/free */
	gboolean is_selection;
	gboolean selection_is_html;
	gboolean skip_insecure_parts;
} SelectionOrMessageData;

static void
selection_or_message_data_free (gpointer ptr)
{
	SelectionOrMessageData *smd = ptr;

	if (smd) {
		g_clear_object (&smd->activity);
		g_clear_object (&smd->folder);
		g_clear_object (&smd->preloaded_message);
		g_clear_object (&smd->message);
		g_clear_object (&smd->part_list);
		camel_pstring_free (smd->message_uid);
		g_slice_free (SelectionOrMessageData, smd);
	}
}

static void
selection_or_message_message_parsed_cb (GObject *object,
					GAsyncResult *result,
					gpointer user_data)
{
	SelectionOrMessageData *smd = user_data;
	GError *local_error = NULL;

	smd->part_list = e_mail_reader_parse_message_finish (E_MAIL_READER (object), result, &local_error);

	if (local_error) {
		g_warn_if_fail (g_error_matches (local_error, G_IO_ERROR, G_IO_ERROR_CANCELLED));
		g_task_return_error (smd->task, local_error);
	} else {
		if (!smd->orig_validity_pgp_sum && !smd->orig_validity_smime_sum)
			e_mail_part_list_sum_validity (smd->part_list, &smd->orig_validity_pgp_sum, &smd->orig_validity_smime_sum);

		g_task_return_boolean (smd->task, TRUE);
	}

	g_clear_object (&smd->task);
}

static void
selection_or_message_got_message_cb (GObject *source_object,
				     GAsyncResult *result,
				     gpointer user_data)
{
	SelectionOrMessageData *smd = user_data;
	EActivity *activity;
	EAlertSink *alert_sink;
	GCancellable *cancellable;
	GError *local_error = NULL;

	activity = smd->activity;
	alert_sink = e_activity_get_alert_sink (activity);
	cancellable = e_activity_get_cancellable (activity);

	g_warn_if_fail (smd->message == NULL);
	smd->message = camel_folder_get_message_finish (CAMEL_FOLDER (source_object), result, &local_error);

	/* Sanity check. */
	g_return_if_fail (
		((smd->message != NULL) && (local_error == NULL)) ||
		((smd->message == NULL) && (local_error != NULL)));

	if (e_activity_handle_cancellation (activity, local_error)) {
		g_task_return_error (smd->task, local_error);
		g_clear_object (&smd->task);
		return;

	} else if (local_error != NULL) {
		e_alert_submit (
			alert_sink, "mail:no-retrieve-message",
			local_error->message, NULL);
		g_task_return_error (smd->task, local_error);
		g_clear_object (&smd->task);
		return;
	}

	e_mail_reader_parse_message (
		E_MAIL_READER (g_task_get_source_object (smd->task)),
		smd->folder,
		smd->message_uid,
		smd->message,
		cancellable,
		selection_or_message_message_parsed_cb,
		smd);
}

static void
selection_or_message_get_message (EMailReader *reader,
				  SelectionOrMessageData *smd)
{
	CamelObjectBag *registry;
	GCancellable *cancellable;
	gchar *mail_uri;

	g_return_if_fail (E_IS_MAIL_READER (reader));
	g_return_if_fail (smd != NULL);

	smd->is_selection = FALSE;

	registry = e_mail_part_list_get_registry ();
	mail_uri = e_mail_part_build_uri (smd->folder, smd->message_uid, NULL, NULL);
	smd->part_list = camel_object_bag_get (registry, mail_uri);
	g_free (mail_uri);

	if (smd->part_list) {
		g_warn_if_fail (smd->message == NULL);
		if (smd->preloaded_message)
			smd->message = smd->preloaded_message;
		else
			smd->message = e_mail_part_list_get_message (smd->part_list);

		if (smd->message)
			g_object_ref (smd->message);
		else
			g_clear_object (&smd->part_list);

		if (smd->message) {
			e_mail_part_list_sum_validity (smd->part_list, &smd->orig_validity_pgp_sum, &smd->orig_validity_smime_sum);

			g_task_return_boolean (smd->task, TRUE);
			g_clear_object (&smd->task);
			return;
		}
	}

	cancellable = g_task_get_cancellable (smd->task);

	smd->activity = e_mail_reader_new_activity (reader);
	e_activity_set_cancellable (smd->activity, cancellable);

	if (smd->preloaded_message) {
		g_warn_if_fail (smd->message == NULL);
		smd->message = g_object_ref (smd->preloaded_message);

		e_mail_reader_parse_message (
			reader,
			smd->folder,
			smd->message_uid,
			smd->message,
			cancellable,
			selection_or_message_message_parsed_cb,
			smd);
	} else {
		camel_folder_get_message (
			smd->folder,
			smd->message_uid,
			G_PRIORITY_DEFAULT,
			cancellable,
			selection_or_message_got_message_cb,
			smd);
	}
}

static void
selection_or_message_got_selection_jsc_cb (GObject *source_object,
					   GAsyncResult *result,
					   gpointer user_data)
{
	SelectionOrMessageData *smd = user_data;
	CamelMimeMessage *new_message;
	gchar *selection;
	GSList *texts = NULL;
	GError *error = NULL;

	g_return_if_fail (smd != NULL);
	g_return_if_fail (E_IS_WEB_VIEW (source_object));

	if (!e_web_view_jsc_get_selection_finish (WEBKIT_WEB_VIEW (source_object), result, &texts, &error)) {
		texts = NULL;
		g_warning ("%s: Failed to get view selection: %s", G_STRFUNC, error ? error->message : "Unknown error");
	}

	g_clear_error (&error);

	selection = texts ? texts->data : NULL;

	if (selection && !smd->selection_is_html) {
		maybe_mangle_plaintext_signature_delimiter (&selection);
		texts->data = selection;
	}

	new_message = e_mail_reader_utils_selection_as_message (smd->message, selection, smd->selection_is_html);

	smd->is_selection = new_message != NULL;

	if (new_message) {
		g_clear_object (&smd->message);
		smd->message = new_message;
	}

	g_task_return_boolean (smd->task, TRUE);
	g_clear_object (&smd->task);

	g_slist_free_full (texts, g_free);
}

void
e_mail_reader_utils_get_selection_or_message (EMailReader *reader,
					      CamelMimeMessage *preloaded_message,
					      GCancellable *cancellable,
					      GAsyncReadyCallback callback,
					      gpointer user_data)
{
	SelectionOrMessageData *smd;
	EMailDisplay *display;
	EWebView *web_view;
	GtkWidget *message_list;
	const gchar *uid;

	message_list = e_mail_reader_get_message_list (reader);

	uid = MESSAGE_LIST (message_list)->cursor_uid;
	g_return_if_fail (uid != NULL);

	smd = g_slice_new0 (SelectionOrMessageData);
	smd->task = g_task_new (reader, cancellable, callback, user_data);
	g_task_set_source_tag (smd->task, e_mail_reader_utils_get_selection_or_message);
	g_task_set_task_data (smd->task, smd, selection_or_message_data_free);

	display = e_mail_reader_get_mail_display (reader);
	web_view = E_WEB_VIEW (display);

	smd->message_uid = camel_pstring_strdup (uid);
	smd->folder = e_mail_reader_ref_folder (reader);

	if (preloaded_message)
		smd->preloaded_message = g_object_ref (preloaded_message);

	if (gtk_widget_is_visible (GTK_WIDGET (web_view)) &&
	    e_web_view_has_selection (web_view)) {
		EMailPartList *part_list;
		CamelMimeMessage *message = NULL;

		part_list = e_mail_display_get_part_list (display);
		if (part_list)
			message = e_mail_part_list_get_message (part_list);

		if (message) {
			CamelContentType *ct;

			e_mail_part_list_sum_validity (part_list, &smd->orig_validity_pgp_sum, &smd->orig_validity_smime_sum);

			smd->message = g_object_ref (message);
			smd->part_list = g_object_ref (part_list);

			ct = camel_mime_part_get_content_type (CAMEL_MIME_PART (message));

			if (camel_content_type_is (ct, "text", "plain")) {
				smd->selection_is_html = FALSE;
				e_web_view_jsc_get_selection (WEBKIT_WEB_VIEW (web_view), E_TEXT_FORMAT_PLAIN, NULL,
					selection_or_message_got_selection_jsc_cb, smd);
			} else {
				smd->selection_is_html = TRUE;
				e_web_view_jsc_get_selection (WEBKIT_WEB_VIEW (web_view), E_TEXT_FORMAT_HTML, NULL,
					selection_or_message_got_selection_jsc_cb, smd);
			}

			return;
		}
	}

	selection_or_message_get_message (reader, smd);
}

CamelMimeMessage *
e_mail_reader_utils_get_selection_or_message_finish (EMailReader *reader,
						     GAsyncResult *result,
						     gboolean *out_is_selection,
						     CamelFolder **out_folder,
						     const gchar **out_message_uid, /* free with camel_pstring_free() */
						     EMailPartList **out_part_list,
						     EMailPartValidityFlags *out_orig_validity_pgp_sum,
						     EMailPartValidityFlags *out_orig_validity_smime_sum,
						     GError **error)
{
	SelectionOrMessageData *smd;
	CamelMimeMessage *message;
	GTask *task;

	g_return_val_if_fail (E_IS_MAIL_READER (reader), NULL);
	g_return_val_if_fail (g_task_is_valid (result, reader), NULL);
	g_return_val_if_fail (g_async_result_is_tagged (result, e_mail_reader_utils_get_selection_or_message), NULL);

	task = G_TASK (result);

	smd = g_task_get_task_data (task);
	g_return_val_if_fail (smd != NULL, NULL);

	if (g_task_propagate_boolean (task, error)) {
		message = g_steal_pointer (&smd->message);

		if (out_is_selection)
			*out_is_selection = smd->is_selection;

		if (out_folder)
			*out_folder = g_steal_pointer (&smd->folder);

		if (out_message_uid)
			*out_message_uid = g_steal_pointer (&smd->message_uid);

		if (out_part_list)
			*out_part_list = g_steal_pointer (&smd->part_list);

		if (out_orig_validity_pgp_sum)
			*out_orig_validity_pgp_sum = smd->orig_validity_pgp_sum;

		if (out_orig_validity_smime_sum)
			*out_orig_validity_smime_sum = smd->orig_validity_smime_sum;
	} else {
		message = NULL;
	}

	return message;
}

static void
reply_to_message_got_message_cb (GObject *source_object,
				 GAsyncResult *result,
				 gpointer user_data)
{
	EMailReplyType reply_type = GPOINTER_TO_INT (user_data);
	EMailReader *reader = E_MAIL_READER (source_object);
	EShell *shell;
	CreateComposerData *ccd;
	GError *error = NULL;

	ccd = create_composer_data_new ();
	ccd->reader = g_object_ref (reader);
	ccd->reply_type = reply_type;
	ccd->reply_style = e_mail_reader_get_reply_style (reader);
	ccd->skip_insecure_parts = emr_utils_get_skip_insecure_parts (reader);

	ccd->message = e_mail_reader_utils_get_selection_or_message_finish (reader, result,
			&ccd->is_selection, &ccd->folder, &ccd->message_uid, &ccd->part_list,
			&ccd->validity_pgp_sum, &ccd->validity_smime_sum, &error);

	if (!ccd->message) {
		if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
			g_warning ("%s: Failed to get message: %s", G_STRFUNC, error ? error->message : "Unknown error");
		g_clear_error (&error);
		create_composer_data_free (ccd);
		return;
	}

	if (reply_type == E_MAIL_REPLY_TO_RECIPIENT) {
		EMailDisplay *display;
		const gchar *uri;

		display = e_mail_reader_get_mail_display (reader);
		uri = e_web_view_get_selected_uri (E_WEB_VIEW (display));

		if (uri) {
			CamelURL *curl;

			curl = camel_url_new (uri, NULL);

			if (curl && curl->path && *curl->path) {
				ccd->address = camel_internet_address_new ();
				if (camel_address_decode (CAMEL_ADDRESS (ccd->address), curl->path) < 0) {
					g_clear_object (&ccd->address);
				}
			}

			if (curl)
				camel_url_free (curl);
		}
	}

	shell = e_shell_backend_get_shell (E_SHELL_BACKEND (e_mail_reader_get_backend (reader)));

	e_msg_composer_new (shell, mail_reader_reply_to_message_composer_created_cb, ccd);
}

void
e_mail_reader_reply_to_message (EMailReader *reader,
                                CamelMimeMessage *src_message,
                                EMailReplyType reply_type)
{
	g_return_if_fail (E_IS_MAIL_READER (reader));

	e_mail_reader_utils_get_selection_or_message (reader, src_message, NULL,
		reply_to_message_got_message_cb, GINT_TO_POINTER (reply_type));
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

static void
emru_file_chooser_filter_changed_cb (GtkFileChooser *file_chooser,
				     GParamSpec *param,
				     gpointer user_data)
{
	GtkFileFilter *filter;
	const gchar *extension = NULL;
	gchar *current_name;
	GtkFileFilterInfo file_info = { 0, };

	g_return_if_fail (GTK_IS_FILE_CHOOSER (file_chooser));

	filter = gtk_file_chooser_get_filter (file_chooser);
	if (!filter)
		return;

	file_info.contains = GTK_FILE_FILTER_FILENAME | GTK_FILE_FILTER_MIME_TYPE;
	file_info.filename = "message.eml";
	file_info.mime_type = "message/rfc822";

	if (gtk_file_filter_filter (filter, &file_info))
		extension = ".eml";

	if (!extension) {
		file_info.filename = "message.mbox";
		file_info.mime_type = "application/mbox";

		if (gtk_file_filter_filter (filter, &file_info))
			extension = ".mbox";
	}

	if (!extension)
		return;

	current_name = gtk_file_chooser_get_current_name (file_chooser);
	if (!current_name)
		return;

	if (!g_str_has_suffix (current_name, extension) &&
	    (g_str_has_suffix (current_name, ".eml") ||
	     g_str_has_suffix (current_name, ".mbox"))) {
		gchar *ptr, *tmp;

		ptr = strrchr (current_name, '.');
		*ptr = '\0';

		tmp = g_strconcat (current_name, extension, NULL);

		gtk_file_chooser_set_current_name (file_chooser, tmp);

		g_free (tmp);
	}

	g_free (current_name);
}

static void
emru_setup_filters (GtkFileChooserNative *file_chooser_native,
		    gpointer user_data)
{
	const gchar *default_extension = user_data;
	GtkFileChooser *file_chooser;

	file_chooser = GTK_FILE_CHOOSER (file_chooser_native);

	if (g_strcmp0 (default_extension, ".eml") == 0) {
		GSList *filters, *link;
		GtkFileFilterInfo file_info = { 0, };

		file_info.contains = GTK_FILE_FILTER_FILENAME | GTK_FILE_FILTER_MIME_TYPE;
		file_info.filename = "message.eml";
		file_info.mime_type = "message/rfc822";

		filters = gtk_file_chooser_list_filters (file_chooser);

		for (link = filters; link; link = g_slist_next (link)) {
			GtkFileFilter *filter = link->data;

			if (gtk_file_filter_filter (filter, &file_info)) {
				gtk_file_chooser_set_filter (file_chooser, filter);
				break;
			}
		}

		g_slist_free (filters);
	}

	/* This does not seem to work for GtkFileChooserNative */
	g_signal_connect (file_chooser, "notify::filter",
		G_CALLBACK (emru_file_chooser_filter_changed_cb), NULL);
}

typedef enum _EMailReaderSaveToFileFormat {
	E_MAIL_READER_SAVE_TO_FILE_FORMAT_MBOX = 0,
	E_MAIL_READER_SAVE_TO_FILE_FORMAT_EML
} EMailReaderSaveToFileFormat;

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
	const gchar *default_extension;
	gchar *suggestion = NULL;
	EMailReaderSaveToFileFormat file_format;

	folder = e_mail_reader_ref_folder (reader);
	backend = e_mail_reader_get_backend (reader);

	uids = e_mail_reader_get_selected_uids (reader);
	g_return_if_fail (uids != NULL && uids->len > 0);

	if (uids->len > 1) {
		GtkWidget *message_list;

		message_list = e_mail_reader_get_message_list (reader);
		message_list_sort_uids (MESSAGE_LIST (message_list), uids);
		file_format = E_MAIL_READER_SAVE_TO_FILE_FORMAT_MBOX;
	} else {
		GSettings *settings;

		settings = e_util_ref_settings ("org.gnome.evolution.mail");
		file_format = g_settings_get_enum (settings, "save-format");
		g_clear_object (&settings);
	}

	if (file_format == E_MAIL_READER_SAVE_TO_FILE_FORMAT_EML)
		default_extension = ".eml";
	else
		default_extension = ".mbox";

	message_uid = g_ptr_array_index (uids, 0);
	title = ngettext ("Save Message", "Save Messages", uids->len);

	/* Suggest as a filename the subject of the first message. */
	info = camel_folder_get_message_info (folder, message_uid);
	if (info != NULL) {
		const gchar *subject;

		subject = camel_message_info_get_subject (info);
		if (subject != NULL)
			suggestion = g_strconcat (subject, default_extension, NULL);
		g_clear_object (&info);
	}

	if (suggestion == NULL) {
		const gchar *basename;

		/* Translators: This is part of a suggested file name
		 * used when saving a message or multiple messages to
		 * mbox format, when the first message doesn't have a
		 * subject.  The extension ".mbox" is appended to the
		 * string; for example "Message.mbox". */
		basename = ngettext ("Message", "Messages", uids->len);
		suggestion = g_strconcat (basename, default_extension, NULL);
	}

	shell_backend = E_SHELL_BACKEND (backend);
	shell = e_shell_backend_get_shell (shell_backend);

	destination = e_shell_run_save_dialog (
		shell, title, suggestion,
		uids->len > 1 ? "*.mbox:application/mbox,message/rfc822" :
		"*.mbox:application/mbox;*.eml:message/rfc822",
		uids->len > 1 ? NULL : emru_setup_filters, (gpointer) default_extension);

	if (destination == NULL)
		goto exit;

	if (uids->len == 1 && g_file_peek_path (destination)) {
		const gchar *path = g_file_peek_path (destination);
		gsize len = strlen (path);

		if (len > 4) {
			EMailReaderSaveToFileFormat chosen_file_format;

			if (g_ascii_strncasecmp (path + len - 4, ".eml", 4) == 0)
				chosen_file_format = E_MAIL_READER_SAVE_TO_FILE_FORMAT_EML;
			else
				chosen_file_format = E_MAIL_READER_SAVE_TO_FILE_FORMAT_MBOX;

			if (file_format != chosen_file_format) {
				GSettings *settings;

				settings = e_util_ref_settings ("org.gnome.evolution.mail");
				g_settings_set_enum (settings, "save-format", chosen_file_format);
				g_clear_object (&settings);
			}
		}
	}

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

void
e_mail_reader_select_previous_message (EMailReader *reader,
				       gboolean or_else_next)
{
	GtkWidget *message_list;
	gboolean hide_deleted;
	gboolean success;

	g_return_if_fail (E_IS_MAIL_READER (reader));

	hide_deleted = e_mail_reader_get_hide_deleted (reader);
	message_list = e_mail_reader_get_message_list (reader);

	success = message_list_select (
		MESSAGE_LIST (message_list),
		MESSAGE_LIST_SELECT_PREVIOUS, 0, 0);

	if (!success && (hide_deleted || or_else_next))
		message_list_select (
			MESSAGE_LIST (message_list),
			MESSAGE_LIST_SELECT_NEXT, 0, 0);
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
mail_reader_parse_message_run (GTask *task,
                               gpointer source_object,
                               gpointer task_data,
                               GCancellable *cancellable)
{
	EMailReader *reader = E_MAIL_READER (source_object);
	CamelObjectBag *registry;
	EMailPartList *part_list;
	EMailDisplay *mail_display;
	AsyncContext *async_context = task_data;
	gchar *mail_uri;
	gboolean is_source;

	mail_display = e_mail_reader_get_mail_display (reader);
	is_source = e_mail_display_get_mode (mail_display) == E_MAIL_FORMATTER_MODE_SOURCE;

	registry = e_mail_part_list_get_registry ();

	mail_uri = e_mail_part_build_uri (
		async_context->folder,
		async_context->message_uid, NULL, NULL);

	part_list = camel_object_bag_reserve (registry, mail_uri);

	if (!part_list && is_source) {
		EMailPart *mail_part;

		part_list = e_mail_part_list_new (async_context->message, async_context->message_uid, async_context->folder);
		mail_part = e_mail_part_new (CAMEL_MIME_PART (async_context->message), ".message");
		e_mail_part_list_add_part (part_list, mail_part);
		g_object_unref (mail_part);

		/* Do not store it, it'll be taken from EMailDisplay */
		camel_object_bag_abort (registry, mail_uri);
	}

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

	if (!g_task_return_error_if_cancelled (task))
		g_task_return_pointer (task, g_steal_pointer (&part_list), g_object_unref);
	else
		g_clear_object (&part_list);
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
	GTask *task;
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

	task = g_task_new (reader, cancellable, callback, user_data);
	g_task_set_source_tag (task, e_mail_reader_parse_message);
	g_task_set_task_data (task, async_context, (GDestroyNotify) async_context_free);

	g_task_run_in_thread (task, mail_reader_parse_message_run);

	g_object_unref (task);
	g_object_unref (activity);
}

EMailPartList *
e_mail_reader_parse_message_finish (EMailReader *reader,
                                    GAsyncResult *result,
                                    GError **error)
{
	g_return_val_if_fail (g_task_is_valid (result, reader), NULL);
	g_return_val_if_fail (g_async_result_is_tagged (result, e_mail_reader_parse_message), NULL);

	return g_task_propagate_pointer (G_TASK (result), error);
}

gboolean
e_mail_reader_utils_get_mark_seen_setting (EMailReader *reader,
					   gint *out_timeout_interval)
{
	CamelFolder *folder;
	GSettings *settings;
	gboolean mark_seen = FALSE;

	g_return_val_if_fail (E_IS_MAIL_READER (reader), FALSE);

	folder = e_mail_reader_ref_folder (reader);

	if (CAMEL_IS_VEE_FOLDER (folder)) {
		GtkWidget *message_list_widget;

		message_list_widget = e_mail_reader_get_message_list (reader);

		if (IS_MESSAGE_LIST (message_list_widget)) {
			MessageList *message_list;

			message_list = MESSAGE_LIST (message_list_widget);

			if (message_list->cursor_uid) {
				CamelMessageInfo *nfo;

				nfo = camel_folder_get_message_info (folder, message_list->cursor_uid);

				if (nfo && CAMEL_IS_VEE_MESSAGE_INFO (nfo)) {
					CamelFolder *real_folder;

					real_folder = camel_vee_folder_get_location (CAMEL_VEE_FOLDER (folder),
						CAMEL_VEE_MESSAGE_INFO (nfo), NULL);

					if (real_folder) {
						g_object_ref (real_folder);
						g_object_unref (folder);
						folder = real_folder;
					}
				}

				g_clear_object (&nfo);
			}
		}
	}

	if (folder) {
		CamelStore *store;
		CamelThreeState cts_value;

		cts_value = camel_folder_get_mark_seen (folder);
		if (cts_value == CAMEL_THREE_STATE_OFF || cts_value == CAMEL_THREE_STATE_ON) {
			if (out_timeout_interval)
				*out_timeout_interval = camel_folder_get_mark_seen_timeout (folder);

			g_clear_object (&folder);

			return cts_value == CAMEL_THREE_STATE_ON;
		}

		store = camel_folder_get_parent_store (folder);
		if (store) {
			ESourceRegistry *registry;
			ESource *source;
			EThreeState ets_value = E_THREE_STATE_INCONSISTENT;

			registry = e_mail_session_get_registry (e_mail_backend_get_session (e_mail_reader_get_backend (reader)));
			source = e_source_registry_ref_source (registry, camel_service_get_uid (CAMEL_SERVICE (store)));

			if (source && e_source_has_extension (source, E_SOURCE_EXTENSION_MAIL_ACCOUNT)) {
				ESourceMailAccount *account_ext;

				account_ext = e_source_get_extension (source, E_SOURCE_EXTENSION_MAIL_ACCOUNT);
				ets_value = e_source_mail_account_get_mark_seen (account_ext);

				if (out_timeout_interval && ets_value != E_THREE_STATE_INCONSISTENT)
					*out_timeout_interval = e_source_mail_account_get_mark_seen_timeout (account_ext);
			}

			g_clear_object (&source);

			if (ets_value == E_THREE_STATE_OFF || ets_value == E_THREE_STATE_ON) {
				g_clear_object (&folder);

				return ets_value == E_THREE_STATE_ON;
			}
		}

		g_clear_object (&folder);
	}

	settings = e_util_ref_settings ("org.gnome.evolution.mail");

	mark_seen = g_settings_get_boolean (settings, "mark-seen");

	if (out_timeout_interval)
		*out_timeout_interval = g_settings_get_int (settings, "mark-seen-timeout");

	g_object_unref (settings);

	return mark_seen;
}
