/*
 * e-mail-session-utils.c
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
 */

#include "evolution-config.h"

#include "e-mail-session-utils.h"

#include <glib/gi18n-lib.h>
#include <libedataserver/libedataserver.h>

#include <libemail-engine/e-mail-folder-utils.h>
#include <libemail-engine/e-mail-utils.h>
#include <libemail-engine/mail-tools.h>

/* User-Agent header value */
#define USER_AGENT ("Evolution " VERSION VERSION_SUBSTRING " " VERSION_COMMENT)

/* FIXME: Temporary - remove this after we move filter/ to eds */
#define E_FILTER_SOURCE_OUTGOING "outgoing"

typedef struct _AsyncContext AsyncContext;
typedef struct _FccForMsgResult FccForMsgResult;

struct _AsyncContext {
	CamelFolder *folder;

	CamelMimeMessage *message;
	CamelMessageInfo *info;

	CamelAddress *from;
	CamelAddress *recipients;

	CamelFilterDriver *driver;

	CamelService *transport;

	GCancellable *cancellable;
	gint io_priority;

	/* X-Evolution headers */
	CamelNameValueArray *xev_headers;

	GPtrArray *post_to_uris;

	EMailLocalFolder local_id;

	gchar *folder_uri;

	gboolean use_sent_folder;
	gboolean request_dsn;
};

struct _FccForMsgResult {
	CamelFolder *folder;
	gboolean use_sent_folder;
};

static void
async_context_free (AsyncContext *context)
{
	g_clear_object (&context->folder);
	g_clear_object (&context->message);
	g_clear_object (&context->info);
	g_clear_object (&context->from);
	g_clear_object (&context->recipients);
	g_clear_object (&context->driver);
	g_clear_object (&context->transport);

	if (context->cancellable != NULL) {
		camel_operation_pop_message (context->cancellable);
		g_object_unref (context->cancellable);
	}

	camel_name_value_array_free (context->xev_headers);

	if (context->post_to_uris != NULL) {
		g_ptr_array_foreach (
			context->post_to_uris, (GFunc) g_free, NULL);
		g_ptr_array_free (context->post_to_uris, TRUE);
	}

	g_free (context->folder_uri);

	g_slice_free (AsyncContext, context);
}

static void
fcc_for_msg_result_free (gpointer data)
{
	FccForMsgResult *result = data;
	g_clear_object (&result->folder);

	g_free (result);
}

GQuark
e_mail_error_quark (void)
{
	static GQuark quark = 0;

	if (G_UNLIKELY (quark == 0)) {
		const gchar *string = "e-mail-error-quark";
		quark = g_quark_from_static_string (string);
	}

	return quark;
}

static void
mail_session_append_to_local_folder_thread (GTask *task,
                                            gpointer source_object,
                                            gpointer task_data,
                                            GCancellable *cancellable)
{
	AsyncContext *context;
	GError *error = NULL;
	gchar *message_uid = NULL;

	context = task_data;

	e_mail_session_append_to_local_folder_sync (
		E_MAIL_SESSION (source_object),
		context->local_id, context->message,
		context->info, &message_uid,
		cancellable, &error);

	if (error != NULL)
		g_task_return_error (task, g_steal_pointer (&error));
	else
		g_task_return_pointer (task, g_steal_pointer (&message_uid), g_free);
}

gboolean
e_mail_session_append_to_local_folder_sync (EMailSession *session,
                                            EMailLocalFolder local_id,
                                            CamelMimeMessage *message,
                                            CamelMessageInfo *info,
                                            gchar **appended_uid,
                                            GCancellable *cancellable,
                                            GError **error)
{
	CamelFolder *folder;
	const gchar *folder_uri;
	gboolean success = FALSE;

	g_return_val_if_fail (E_IS_MAIL_SESSION (session), FALSE);
	g_return_val_if_fail (CAMEL_IS_MIME_MESSAGE (message), FALSE);

	folder_uri = e_mail_session_get_local_folder_uri (session, local_id);
	g_return_val_if_fail (folder_uri != NULL, FALSE);

	folder = e_mail_session_uri_to_folder_sync (
		session, folder_uri, CAMEL_STORE_FOLDER_CREATE,
		cancellable, error);

	if (folder != NULL) {
		success = e_mail_folder_append_message_sync (
			folder, message, info, appended_uid,
			cancellable, error);
		g_object_unref (folder);
	}

	return success;
}

void
e_mail_session_append_to_local_folder (EMailSession *session,
                                       EMailLocalFolder local_id,
                                       CamelMimeMessage *message,
                                       CamelMessageInfo *info,
                                       gint io_priority,
                                       GCancellable *cancellable,
                                       GAsyncReadyCallback callback,
                                       gpointer user_data)
{
	GTask *task;
	AsyncContext *context;

	g_return_if_fail (E_IS_MAIL_SESSION (session));
	g_return_if_fail (CAMEL_IS_MIME_MESSAGE (message));

	context = g_slice_new0 (AsyncContext);
	context->local_id = local_id;
	context->message = g_object_ref (message);

	if (info != NULL)
		context->info = g_object_ref (info);

	task = g_task_new (session, cancellable, callback, user_data);
	g_task_set_source_tag (task, e_mail_session_append_to_local_folder);
	g_task_set_priority (task, io_priority);
	g_task_set_task_data (task, g_steal_pointer (&context), (GDestroyNotify) async_context_free);

	g_task_run_in_thread (task, mail_session_append_to_local_folder_thread);

	g_object_unref (task);
}

gboolean
e_mail_session_append_to_local_folder_finish (EMailSession *session,
                                              GAsyncResult *result,
                                              gchar **appended_uid,
                                              GError **error)
{
	gchar *message_uid;
	gboolean res;

	g_return_val_if_fail (g_task_is_valid (result, session), FALSE);
	g_return_val_if_fail (g_async_result_is_tagged (result, e_mail_session_append_to_local_folder), FALSE);

	message_uid = g_task_propagate_pointer (G_TASK (result), error);
	res = message_uid != NULL;
	if (appended_uid != NULL)
		*appended_uid = g_steal_pointer (&message_uid);

	g_clear_pointer (&message_uid, g_free);
	return res;
}

static void
mail_session_handle_draft_headers_thread (GTask *task,
                                          gpointer source_object,
                                          gpointer task_data,
                                          GCancellable *cancellable)
{
	EMailSession *session;
	CamelMimeMessage *message;
	GError *error = NULL;
	gboolean res;

	session = E_MAIL_SESSION (source_object);
	message = CAMEL_MIME_MESSAGE (task_data);

	res = e_mail_session_handle_draft_headers_sync (session, message, cancellable, &error);

	if (error != NULL)
		g_task_return_error (task, g_steal_pointer (&error));
	else
		g_task_return_boolean (task, res);
}

gboolean
e_mail_session_handle_draft_headers_sync (EMailSession *session,
                                          CamelMimeMessage *message,
                                          GCancellable *cancellable,
                                          GError **error)
{
	CamelFolder *folder;
	CamelMedium *medium;
	const gchar *folder_uri;
	const gchar *message_uid;
	const gchar *header_name;
	gboolean success;

	g_return_val_if_fail (E_IS_MAIL_SESSION (session), FALSE);
	g_return_val_if_fail (CAMEL_IS_MIME_MESSAGE (message), FALSE);

	medium = CAMEL_MEDIUM (message);

	header_name = "X-Evolution-Draft-Folder";
	folder_uri = camel_medium_get_header (medium, header_name);

	header_name = "X-Evolution-Draft-Message";
	message_uid = camel_medium_get_header (medium, header_name);

	/* Don't report errors about missing X-Evolution-Draft
	 * headers.  These headers are optional, so their absence
	 * is handled by doing nothing. */
	if (folder_uri == NULL || message_uid == NULL)
		return TRUE;

	folder = e_mail_session_uri_to_folder_sync (
		session, folder_uri, 0, cancellable, error);

	if (folder == NULL)
		return FALSE;

	camel_folder_set_message_flags (
		folder, message_uid,
		CAMEL_MESSAGE_DELETED | CAMEL_MESSAGE_SEEN,
		CAMEL_MESSAGE_DELETED | CAMEL_MESSAGE_SEEN);

	success = camel_folder_synchronize_message_sync (
		folder, message_uid, cancellable, error);

	g_object_unref (folder);

	return success;
}

void
e_mail_session_handle_draft_headers (EMailSession *session,
                                     CamelMimeMessage *message,
                                     gint io_priority,
                                     GCancellable *cancellable,
                                     GAsyncReadyCallback callback,
                                     gpointer user_data)
{
	GTask *task;

	g_return_if_fail (E_IS_MAIL_SESSION (session));
	g_return_if_fail (CAMEL_IS_MIME_MESSAGE (message));

	task = g_task_new (session, cancellable, callback, user_data);
	g_task_set_source_tag (task, e_mail_session_handle_draft_headers);
	g_task_set_priority (task, io_priority);
	g_task_set_task_data (task, g_object_ref (message), g_object_unref);

	g_task_run_in_thread (task, mail_session_handle_draft_headers_thread);

	g_object_unref (task);
}

gboolean
e_mail_session_handle_draft_headers_finish (EMailSession *session,
                                            GAsyncResult *result,
                                            GError **error)
{
	g_return_val_if_fail (g_task_is_valid (result, session), FALSE);
	g_return_val_if_fail (g_async_result_is_tagged (result, e_mail_session_handle_draft_headers), FALSE);

	return g_task_propagate_boolean (G_TASK (result), error);
}

static void
mail_session_handle_source_headers_thread (GTask *task,
                                           gpointer source_object,
                                           gpointer task_data,
                                           GCancellable *cancellable)
{
	EMailSession *session;
	CamelMimeMessage *message;
	GError *error = NULL;
	gboolean res;

	session = E_MAIL_SESSION (source_object);
	message = CAMEL_MIME_MESSAGE (task_data);

	res = e_mail_session_handle_source_headers_sync (
		session, message, cancellable, &error);

	if (error != NULL)
		g_task_return_error (task, g_steal_pointer (&error));
	else
		g_task_return_boolean (task, res);
}

gboolean
e_mail_session_handle_source_headers_sync (EMailSession *session,
                                           CamelMimeMessage *message,
                                           GCancellable *cancellable,
                                           GError **error)
{
	CamelFolder *folder;
	CamelMedium *medium;
	CamelMessageFlags flags = 0;
	const gchar *folder_uri;
	const gchar *message_uid;
	const gchar *flag_string;
	const gchar *header_name;
	gboolean success;
	guint length, ii;
	gchar **tokens;
	gchar *string;

	g_return_val_if_fail (E_IS_MAIL_SESSION (session), FALSE);
	g_return_val_if_fail (CAMEL_IS_MIME_MESSAGE (message), FALSE);

	medium = CAMEL_MEDIUM (message);

	header_name = "X-Evolution-Source-Folder";
	folder_uri = camel_medium_get_header (medium, header_name);

	header_name = "X-Evolution-Source-Message";
	message_uid = camel_medium_get_header (medium, header_name);

	header_name = "X-Evolution-Source-Flags";
	flag_string = camel_medium_get_header (medium, header_name);

	/* Don't report errors about missing X-Evolution-Source
	 * headers.  These headers are optional, so their absence
	 * is handled by doing nothing. */
	if (folder_uri == NULL || message_uid == NULL || flag_string == NULL)
		return TRUE;

	/* Convert the flag string to CamelMessageFlags. */

	string = g_strstrip (g_strdup (flag_string));
	tokens = g_strsplit (string, " ", 0);
	g_free (string);

	/* If tokens is NULL, a length of 0 will skip the loop. */
	length = (tokens != NULL) ? g_strv_length (tokens) : 0;

	for (ii = 0; ii < length; ii++) {
		/* Note: We're only checking for flags known to
		 * be used in X-Evolution-Source-Flags headers.
		 * Add more as needed. */
		if (g_strcmp0 (tokens[ii], "ANSWERED") == 0)
			flags |= CAMEL_MESSAGE_ANSWERED;
		else if (g_strcmp0 (tokens[ii], "ANSWERED_ALL") == 0)
			flags |= CAMEL_MESSAGE_ANSWERED_ALL;
		else if (g_strcmp0 (tokens[ii], "FORWARDED") == 0)
			flags |= CAMEL_MESSAGE_FORWARDED;
		else if (g_strcmp0 (tokens[ii], "SEEN") == 0)
			flags |= CAMEL_MESSAGE_SEEN;
		else
			g_warning (
				"Unknown flag '%s' in %s",
				tokens[ii], header_name);
	}

	g_strfreev (tokens);

	folder = e_mail_session_uri_to_folder_sync (
		session, folder_uri, 0, cancellable, error);

	if (folder == NULL)
		return FALSE;

	camel_folder_set_message_flags (
		folder, message_uid, flags, flags);

	success = camel_folder_synchronize_message_sync (
		folder, message_uid, cancellable, error);

	g_object_unref (folder);

	return success;
}

void
e_mail_session_handle_source_headers (EMailSession *session,
                                      CamelMimeMessage *message,
                                      gint io_priority,
                                      GCancellable *cancellable,
                                      GAsyncReadyCallback callback,
                                      gpointer user_data)
{
	GTask *task;

	g_return_if_fail (E_IS_MAIL_SESSION (session));
	g_return_if_fail (CAMEL_IS_MIME_MESSAGE (message));

	task = g_task_new (session, cancellable, callback, user_data);
	g_task_set_source_tag (task, e_mail_session_handle_source_headers);
	g_task_set_priority (task, io_priority);
	g_task_set_task_data (task, g_object_ref (message), g_object_unref);

	g_task_run_in_thread (task, mail_session_handle_source_headers_thread);

	g_object_unref (task);
}

gboolean
e_mail_session_handle_source_headers_finish (EMailSession *session,
                                             GAsyncResult *result,
                                             GError **error)
{
	g_return_val_if_fail (g_task_is_valid (result, session), FALSE);
	g_return_val_if_fail (g_async_result_is_tagged (result, e_mail_session_handle_source_headers), FALSE);

	return g_task_propagate_boolean (G_TASK (result), error);
}

static void
mail_session_send_to_thread (GTask *task,
                             gpointer source_object,
                             gpointer task_data,
                             GCancellable *cancellable)
{
	EMailSession *session;
	AsyncContext *context;
	CamelProvider *provider;
	CamelFolder *folder = NULL;
	CamelFolder *local_sent_folder;
	CamelServiceConnectionStatus status;
	GString *error_messages;
	gboolean copy_to_sent = TRUE;
	gboolean sent_message_saved = FALSE;
	gboolean did_connect = FALSE;
	guint ii;
	GError *error = NULL;

	session = E_MAIL_SESSION (source_object);
	context = task_data;

	if (camel_address_length (context->recipients) == 0)
		goto skip_send;

	/* Send the message to all recipients. */

	if (context->transport == NULL) {
		mail_tool_restore_xevolution_headers (context->message, context->xev_headers);
		g_task_return_new_error (
			task, CAMEL_SERVICE_ERROR,
			CAMEL_SERVICE_ERROR_UNAVAILABLE,
			_("No mail transport service available"));
		return;
	}

	if (!e_mail_session_mark_service_used_sync (session, context->transport, cancellable)) {
		g_warn_if_fail (g_cancellable_set_error_if_cancelled (cancellable, &error));
		mail_tool_restore_xevolution_headers (context->message, context->xev_headers);
		g_task_return_error (task, g_steal_pointer (&error));
		return;
	}

	status = camel_service_get_connection_status (context->transport);
	if (status != CAMEL_SERVICE_CONNECTED) {
		ESourceRegistry *registry;
		ESource *source;

		/* Make sure user will be asked for a password, in case he/she cancelled it */
		registry = e_mail_session_get_registry (session);
		source = e_source_registry_ref_source (registry, camel_service_get_uid (context->transport));

		if (source) {
			e_mail_session_emit_allow_auth_prompt (session, source);
			g_object_unref (source);
		}

		did_connect = TRUE;

		camel_service_connect_sync (context->transport, cancellable, &error);

		if (error != NULL) {
			mail_tool_restore_xevolution_headers (context->message, context->xev_headers);
			e_mail_session_unmark_service_used (session, context->transport);
			g_task_return_error (task, g_steal_pointer (&error));
			return;
		}
	}

	provider = camel_service_get_provider (context->transport);

	if (provider->flags & CAMEL_PROVIDER_DISABLE_SENT_FOLDER)
		copy_to_sent = FALSE;

	camel_transport_set_request_dsn (CAMEL_TRANSPORT (context->transport), context->request_dsn);

	camel_transport_send_to_sync (
		CAMEL_TRANSPORT (context->transport),
		context->message, context->from,
		context->recipients, &sent_message_saved, cancellable, &error);

	if (did_connect) {
		/* Disconnect regardless of error or cancellation,
		 * but be mindful of these conditions when calling
		 * camel_service_disconnect_sync(). */
		if (g_cancellable_is_cancelled (cancellable)) {
			camel_service_disconnect_sync (
				context->transport, FALSE, NULL, NULL);
		} else if (error != NULL) {
			camel_service_disconnect_sync (
				context->transport, FALSE, cancellable, NULL);
		} else {
			camel_service_disconnect_sync (
				context->transport, TRUE, cancellable, &error);
		}
	}

	e_mail_session_unmark_service_used (session, context->transport);

	if (error != NULL) {
		mail_tool_restore_xevolution_headers (context->message, context->xev_headers);
		g_task_return_error (task, g_steal_pointer (&error));
		return;
	}

skip_send:
	/* Post the message to requested folders. */
	for (ii = 0; ii < context->post_to_uris->len; ii++) {
		CamelFolder *post_folder;
		const gchar *folder_uri;

		folder_uri = g_ptr_array_index (context->post_to_uris, ii);

		post_folder = e_mail_session_uri_to_folder_sync (
			session, folder_uri, 0, cancellable, &error);

		if (error != NULL) {
			g_warn_if_fail (post_folder == NULL);
			mail_tool_restore_xevolution_headers (context->message, context->xev_headers);
			g_task_return_error (task, g_steal_pointer (&error));
			return;
		}

		g_return_if_fail (CAMEL_IS_FOLDER (post_folder));

		camel_operation_push_message (cancellable, _("Posting message to “%s”"), camel_folder_get_full_display_name (post_folder));

		camel_folder_append_message_sync (
			post_folder, context->message, context->info,
			NULL, cancellable, &error);

		camel_operation_pop_message (cancellable);

		g_object_unref (post_folder);

		if (error != NULL) {
			mail_tool_restore_xevolution_headers (context->message, context->xev_headers);
			g_task_return_error (task, g_steal_pointer (&error));
			return;
		}
	}

	/*** Post Processing ***/

	/* This accumulates error messages during post-processing. */
	error_messages = g_string_sized_new (256);

	mail_tool_restore_xevolution_headers (context->message, context->xev_headers);

	/* Run filters on the outgoing message. */
	if (context->driver != NULL) {
		CamelMessageFlags message_flags;
		const gchar *transport_uid = camel_service_get_uid (context->transport);

		camel_filter_driver_filter_message (
			context->driver, context->message, context->info,
			NULL, NULL, transport_uid, transport_uid, cancellable, &error);

		if (g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
			goto exit;

		if (error != NULL) {
			g_string_append_printf (
				error_messages,
				_("Failed to apply outgoing filters: %s"),
				error->message);
			g_clear_error (&error);
		}

		message_flags = camel_message_info_get_flags (context->info);

		if (message_flags & CAMEL_MESSAGE_DELETED)
			copy_to_sent = FALSE;
	}

	if (!copy_to_sent || sent_message_saved)
		goto cleanup;

	/* Append the sent message to a Sent folder. */

	local_sent_folder =
		e_mail_session_get_local_folder (
		session, E_MAIL_LOCAL_FOLDER_SENT);

	folder = e_mail_session_get_fcc_for_message_sync (
		session, context->message, &copy_to_sent, cancellable, &error);

	if (!copy_to_sent)
		goto cleanup;

	/* Sanity check. */
	g_return_if_fail (
		((folder != NULL) && (error == NULL)) ||
		((folder == NULL) && (error != NULL)));

	/* Append the message. */
	if (folder != NULL) {
		camel_operation_push_message (cancellable, _("Storing sent message to “%s”"), camel_folder_get_full_display_name (folder));

		camel_folder_append_message_sync (
			folder, context->message,
			context->info, NULL, cancellable, &error);

		camel_operation_pop_message (cancellable);
	}

	if (g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
		goto exit;

	if (error == NULL)
		goto cleanup;

	if (folder != NULL && folder != local_sent_folder) {
		const gchar *description;

		description = camel_folder_get_description (folder);

		if (error_messages->len > 0)
			g_string_append (error_messages, "\n\n");
		g_string_append_printf (
			error_messages,
			_("Failed to append to %s: %s\n"
			"Appending to local “Sent” folder instead."),
			description, error->message);
	}

	/* If appending to a remote Sent folder failed,
	 * try appending to the local Sent folder. */
	if (folder != local_sent_folder) {

		g_clear_error (&error);

		camel_operation_push_message (cancellable, _("Storing sent message to “%s”"), camel_folder_get_full_display_name (local_sent_folder));

		camel_folder_append_message_sync (
			local_sent_folder, context->message,
			context->info, NULL, cancellable, &error);

		camel_operation_pop_message (cancellable);
	}

	if (g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
		goto exit;

	/* We can't even append to the local Sent folder?
	 * In that case just leave the message in Outbox. */
	if (error != NULL) {
		if (error_messages->len > 0)
			g_string_append (error_messages, "\n\n");
		g_string_append_printf (
			error_messages,
			_("Failed to append to local “Sent” folder: %s"),
			error->message);
		g_clear_error (&error);
		goto exit;
	}

cleanup:

	/* The send operation was successful; ignore cleanup errors. */

	/* Mark the draft message for deletion, if present. */
	e_mail_session_handle_draft_headers_sync (
		session, context->message, cancellable, &error);
	if (error != NULL) {
		g_warning ("%s", error->message);
		g_clear_error (&error);
	}

	/* Set flags on the original source message, if present.
	 * Source message refers to the message being forwarded
	 * or replied to. */
	e_mail_session_handle_source_headers_sync (
		session, context->message, cancellable, &error);
	if (error &&
	    !g_error_matches (error, CAMEL_FOLDER_ERROR, CAMEL_FOLDER_ERROR_INVALID_UID) &&
	    !g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED)) {
		g_warning (
			"%s: Failed to handle source headers: %s",
			G_STRFUNC, error->message);
	}
	g_clear_error (&error);

exit:

	/* If we were cancelled, disregard any other errors. */
	if (g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED)) {
		g_task_return_error (task, g_steal_pointer (&error));

	/* Stuff the accumulated error messages in a GError. */
	} else if (error_messages->len > 0) {
		g_task_return_new_error (
			task, E_MAIL_ERROR,
			E_MAIL_ERROR_POST_PROCESSING,
			"%s", error_messages->str);
	} else {
		/* Synchronize the Sent folder. */
		if (folder != NULL) {
			camel_folder_synchronize_sync (
				folder, FALSE, cancellable, NULL);
			g_object_unref (folder);
		}

		if (!g_task_had_error (task))
			g_task_return_boolean (task, TRUE);
	}

	g_string_free (error_messages, TRUE);
}

void
e_mail_session_send_to (EMailSession *session,
                        CamelMimeMessage *message,
                        gint io_priority,
                        GCancellable *cancellable,
                        CamelFilterGetFolderFunc get_folder_func,
                        gpointer get_folder_data,
                        GAsyncReadyCallback callback,
                        gpointer user_data)
{
	GTask *task;
	AsyncContext *context;
	CamelAddress *from;
	CamelAddress *recipients;
	CamelMedium *medium;
	CamelMessageInfo *info;
	CamelService *transport;
	GPtrArray *post_to_uris;
	CamelNameValueArray *xev_headers;
	const gchar *resent_from;
	gboolean request_dsn;
	gsize msg_size;
	guint ii, len;
	GError *error = NULL;

	g_return_if_fail (E_IS_MAIL_SESSION (session));
	g_return_if_fail (CAMEL_IS_MIME_MESSAGE (message));

	medium = CAMEL_MEDIUM (message);

	if (!camel_medium_get_header (medium, "X-Evolution-Is-Redirect"))
		camel_medium_set_header (medium, "User-Agent", USER_AGENT);

	request_dsn = g_strcmp0 (camel_medium_get_header (medium, "X-Evolution-Request-DSN"), "1") == 0;

	/* Do this before removing "X-Evolution" headers. */
	transport = e_mail_session_ref_transport_for_message (
		session, message);

	xev_headers = mail_tool_remove_xevolution_headers (message);
	len = camel_name_value_array_get_length (xev_headers);

	/* Extract directives from X-Evolution headers. */

	post_to_uris = g_ptr_array_new ();
	for (ii = 0; ii < len; ii++) {
		const gchar *header_name = NULL, *header_value = NULL;
		gchar *folder_uri;

		if (!camel_name_value_array_get (xev_headers, ii, &header_name, &header_value) ||
		    !header_name ||
		    g_ascii_strcasecmp (header_name, "X-Evolution-PostTo") != 0)
			continue;

		folder_uri = g_strstrip (g_strdup (header_value));
		g_ptr_array_add (post_to_uris, folder_uri);
	}

	/* Collect sender and recipients from headers. */

	from = (CamelAddress *) camel_internet_address_new ();
	recipients = (CamelAddress *) camel_internet_address_new ();
	resent_from = camel_medium_get_header (medium, "Resent-From");

	if (resent_from != NULL) {
		const CamelInternetAddress *addr;
		const gchar *type;

		camel_address_decode (from, resent_from);

		type = CAMEL_RECIPIENT_TYPE_RESENT_TO;
		addr = camel_mime_message_get_recipients (message, type);
		camel_address_cat (recipients, CAMEL_ADDRESS (addr));

		type = CAMEL_RECIPIENT_TYPE_RESENT_CC;
		addr = camel_mime_message_get_recipients (message, type);
		camel_address_cat (recipients, CAMEL_ADDRESS (addr));

		type = CAMEL_RECIPIENT_TYPE_RESENT_BCC;
		addr = camel_mime_message_get_recipients (message, type);
		camel_address_cat (recipients, CAMEL_ADDRESS (addr));

	} else {
		const CamelInternetAddress *addr;
		const gchar *type;

		addr = camel_mime_message_get_from (message);
		camel_address_copy (from, CAMEL_ADDRESS (addr));

		type = CAMEL_RECIPIENT_TYPE_TO;
		addr = camel_mime_message_get_recipients (message, type);
		camel_address_cat (recipients, CAMEL_ADDRESS (addr));

		type = CAMEL_RECIPIENT_TYPE_CC;
		addr = camel_mime_message_get_recipients (message, type);
		camel_address_cat (recipients, CAMEL_ADDRESS (addr));

		type = CAMEL_RECIPIENT_TYPE_BCC;
		addr = camel_mime_message_get_recipients (message, type);
		camel_address_cat (recipients, CAMEL_ADDRESS (addr));
	}

	/* Miscellaneous preparations. */

	info = camel_message_info_new_from_headers (NULL, camel_medium_get_headers (CAMEL_MEDIUM (message)));

	msg_size = camel_data_wrapper_calculate_size_sync (CAMEL_DATA_WRAPPER (message), cancellable, NULL);
	if (msg_size != ((gsize) -1))
		camel_message_info_set_size (info, msg_size);
	camel_message_info_set_flags (info, CAMEL_MESSAGE_SEEN |
		(camel_mime_message_has_attachment (message) ? CAMEL_MESSAGE_ATTACHMENTS : 0), ~0);

	/* expand, or remove empty, group addresses */
	em_utils_expand_groups (CAMEL_INTERNET_ADDRESS (recipients));

	/* The rest of the processing happens in a thread. */

	context = g_slice_new0 (AsyncContext);
	context->message = g_object_ref (message);
	context->io_priority = io_priority;
	context->from = from;
	context->recipients = recipients;
	context->info = info;
	context->request_dsn = request_dsn;
	context->xev_headers = xev_headers;
	context->post_to_uris = post_to_uris;
	context->transport = transport;

	if (G_IS_CANCELLABLE (cancellable))
		context->cancellable = g_object_ref (cancellable);

	/* Failure here emits a runtime warning but is non-fatal. */
	context->driver = camel_session_get_filter_driver (CAMEL_SESSION (session), E_FILTER_SOURCE_OUTGOING, NULL, &error);
	if (context->driver != NULL && get_folder_func)
		camel_filter_driver_set_folder_func (
			context->driver, get_folder_func, get_folder_data);
	if (error != NULL) {
		g_warn_if_fail (context->driver == NULL);
		g_warning ("%s", error->message);
		g_error_free (error);
	}

	/* This gets popped in async_context_free(). */
	camel_operation_push_message (cancellable, _("Sending message"));

	task = g_task_new (session, cancellable, callback, user_data);
	g_task_set_source_tag (task, e_mail_session_send_to);
	g_task_set_priority (task, io_priority);
	g_task_set_task_data (task, g_steal_pointer (&context), (GDestroyNotify) async_context_free);

	g_task_run_in_thread (task, mail_session_send_to_thread);

	g_object_unref (task);
}

gboolean
e_mail_session_send_to_finish (EMailSession *session,
                               GAsyncResult *result,
                               GError **error)
{
	g_return_val_if_fail (g_task_is_valid (result, session), FALSE);
	g_return_val_if_fail (g_async_result_is_tagged (result, e_mail_session_send_to), FALSE);

	return g_task_propagate_boolean (G_TASK (result), error);
}

/* Helper for e_mail_session_get_fcc_for_message_sync() */
static CamelFolder *
mail_session_try_uri_to_folder (EMailSession *session,
                                const gchar *folder_uri,
                                GCancellable *cancellable,
                                GError **error)
{
	CamelFolder *folder;
	GError *local_error = NULL;

	folder = e_mail_session_uri_to_folder_sync (
		session, folder_uri, 0, cancellable, &local_error);

	/* Sanity check. */
	g_return_val_if_fail (
		((folder != NULL) && (local_error == NULL)) ||
		((folder == NULL) && (local_error != NULL)), NULL);

	/* Disregard specific errors. */

	/* Invalid URI. */
	if (g_error_matches (
		local_error, CAMEL_FOLDER_ERROR,
		CAMEL_FOLDER_ERROR_INVALID))
		g_clear_error (&local_error);

	/* Folder not found. */
	if (g_error_matches (
		local_error, CAMEL_STORE_ERROR,
		CAMEL_STORE_ERROR_NO_FOLDER))
		g_clear_error (&local_error);

	if (local_error != NULL)
		g_propagate_error (error, local_error);

	return folder;
}

/* Helper for e_mail_session_get_fcc_for_message_sync() */
static CamelFolder *
mail_session_ref_origin_folder (EMailSession *session,
                                CamelMimeMessage *message,
                                GCancellable *cancellable,
                                GError **error)
{
	CamelMedium *medium;
	const gchar *header_name;
	const gchar *header_value;

	medium = CAMEL_MEDIUM (message);

	/* Check that a "X-Evolution-Source-Flags" header is present
	 * and its value does not contain the substring "FORWARDED". */

	header_name = "X-Evolution-Source-Flags";
	header_value = camel_medium_get_header (medium, header_name);

	if (header_value == NULL)
		return NULL;

	/* Check that a "X-Evolution-Source-Message" header is present. */

	header_name = "X-Evolution-Source-Message";
	header_value = camel_medium_get_header (medium, header_name);

	if (header_value == NULL)
		return NULL;

	/* Check that a "X-Evolution-Source-Folder" header is present.
	 * Its value specifies the origin folder as a folder URI. */

	header_name = "X-Evolution-Source-Folder";
	header_value = camel_medium_get_header (medium, header_name);

	if (header_value == NULL)
		return NULL;

	/* This may return NULL without setting a GError. */
	return mail_session_try_uri_to_folder (
		session, header_value, cancellable, error);
}

/* Helper for e_mail_session_get_fcc_for_message_sync() */
static CamelFolder *
mail_session_ref_fcc_from_identity (EMailSession *session,
                                    ESource *source,
                                    CamelMimeMessage *message,
				    gboolean *out_use_sent_folder,
                                    GCancellable *cancellable,
                                    GError **error)
{
	ESourceRegistry *registry;
	ESourceMailSubmission *extension;
	CamelFolder *folder = NULL;
	const gchar *extension_name;
	gchar *folder_uri;
	gboolean use_sent_folder;

	registry = e_mail_session_get_registry (session);
	extension_name = E_SOURCE_EXTENSION_MAIL_SUBMISSION;

	if (source == NULL)
		return NULL;

	if (!e_source_registry_check_enabled (registry, source))
		return NULL;

	if (!e_source_has_extension (source, extension_name))
		return NULL;

	extension = e_source_get_extension (source, extension_name);
	use_sent_folder = e_source_mail_submission_get_use_sent_folder (extension);

	if (out_use_sent_folder)
		*out_use_sent_folder = use_sent_folder;

	if (!use_sent_folder)
		return NULL;

	if (e_source_mail_submission_get_replies_to_origin_folder (extension)) {
		GError *local_error = NULL;

		/* This may return NULL without setting a GError. */
		folder = mail_session_ref_origin_folder (
			session, message, cancellable, &local_error);

		if (local_error != NULL) {
			g_warn_if_fail (folder == NULL);
			g_propagate_error (error, local_error);
			return NULL;
		}
	}

	folder_uri = e_source_mail_submission_dup_sent_folder (extension);

	if (folder_uri != NULL && folder == NULL) {
		/* This may return NULL without setting a GError. */
		folder = mail_session_try_uri_to_folder (
			session, folder_uri, cancellable, error);
	}

	g_free (folder_uri);

	return folder;
}

/* Helper for e_mail_session_get_fcc_for_message_sync() */
static CamelFolder *
mail_session_ref_fcc_from_x_identity (EMailSession *session,
                                      CamelMimeMessage *message,
				      gboolean *out_use_sent_folder,
                                      GCancellable *cancellable,
                                      GError **error)
{
	ESource *source;
	ESourceRegistry *registry;
	CamelFolder *folder;
	CamelMedium *medium;
	const gchar *header_name;
	const gchar *header_value;
	gchar *uid;

	medium = CAMEL_MEDIUM (message);
	header_name = "X-Evolution-Identity";
	header_value = camel_medium_get_header (medium, header_name);

	if (header_value == NULL)
		return NULL;

	uid = g_strstrip (g_strdup (header_value));

	registry = e_mail_session_get_registry (session);
	source = e_source_registry_ref_source (registry, uid);

	/* This may return NULL without setting a GError. */
	folder = mail_session_ref_fcc_from_identity (
		session, source, message, out_use_sent_folder, cancellable, error);

	g_clear_object (&source);

	g_free (uid);

	return folder;
}

/* Helper for e_mail_session_get_fcc_for_message_sync() */
static CamelFolder *
mail_session_ref_fcc_from_x_fcc (EMailSession *session,
                                 CamelMimeMessage *message,
                                 GCancellable *cancellable,
                                 GError **error)
{
	CamelMedium *medium;
	const gchar *header_name;
	const gchar *header_value;

	medium = CAMEL_MEDIUM (message);
	header_name = "X-Evolution-Fcc";
	header_value = camel_medium_get_header (medium, header_name);

	if (header_value == NULL)
		return NULL;

	/* This may return NULL without setting a GError. */
	return mail_session_try_uri_to_folder (
		session, header_value, cancellable, error);
}

/* Helper for e_mail_session_get_fcc_for_message_sync() */
static CamelFolder *
mail_session_ref_fcc_from_default_identity (EMailSession *session,
                                            CamelMimeMessage *message,
					    gboolean *out_use_sent_folder,
                                            GCancellable *cancellable,
                                            GError **error)
{
	ESource *source;
	ESourceRegistry *registry;
	CamelFolder *folder;

	registry = e_mail_session_get_registry (session);
	source = e_source_registry_ref_default_mail_identity (registry);

	/* This may return NULL without setting a GError. */
	folder = mail_session_ref_fcc_from_identity (
		session, source, message, out_use_sent_folder, cancellable, error);

	g_clear_object (&source);

	return folder;
}

/**
 * e_mail_session_get_fcc_for_message_sync:
 * @session: an #EMailSession
 * @message: a #CamelMimeMessage
 * @out_use_sent_folder: (out) (nullable): optional return location to store
 *    corresponding use-sent-folder for the mail account, or %NULL
 * @cancellable: optional #GCancellable object, or %NULL
 * @error: return location for a #GError, or %NULL
 *
 * Obtains the preferred "carbon-copy" folder (a.k.a Fcc) for @message
 * by first checking @message for an "X-Evolution-Identity" header, and
 * then an "X-Evolution-Fcc" header.  Failing that, the function checks
 * the default mail identity (if available), and failing even that, the
 * function falls back to the Sent folder from the built-in mail store.
 *
 * Where applicable, the function attempts to honor the
 * #ESourceMailSubmission:replies-to-origin-folder preference.
 *
 * The returned #CamelFolder is referenced for thread-safety and must be
 * unreferenced with g_object_unref() when finished with it.
 *
 * If a non-recoverable error occurs, the function sets @error and returns
 * %NULL. It returns %NULL without setting @error when the mail account
 * has set to not use sent folder, in which case it indicates that
 * in @out_use_sent_folder too.
 *
 * Returns: a #CamelFolder, or %NULL
 **/
CamelFolder *
e_mail_session_get_fcc_for_message_sync (EMailSession *session,
                                         CamelMimeMessage *message,
					 gboolean *out_use_sent_folder,
                                         GCancellable *cancellable,
                                         GError **error)
{
	CamelFolder *folder = NULL;
	gboolean use_sent_folder = TRUE;

	g_return_val_if_fail (E_IS_MAIL_SESSION (session), NULL);
	g_return_val_if_fail (CAMEL_IS_MIME_MESSAGE (message), NULL);

	if (out_use_sent_folder)
		*out_use_sent_folder = TRUE;

	/* Check for "X-Evolution-Identity" header. */
	if (folder == NULL) {
		GError *local_error = NULL;

		/* This may return NULL without setting a GError. */
		folder = mail_session_ref_fcc_from_x_identity (
			session, message, &use_sent_folder, cancellable, &local_error);

		if (local_error != NULL) {
			g_warn_if_fail (folder == NULL);
			g_propagate_error (error, local_error);
			return NULL;
		}

		if (!use_sent_folder) {
			if (out_use_sent_folder)
				*out_use_sent_folder = use_sent_folder;
			return NULL;
		}
	}

	/* Check for "X-Evolution-Fcc" header. */
	if (folder == NULL) {
		GError *local_error = NULL;

		/* This may return NULL without setting a GError. */
		folder = mail_session_ref_fcc_from_x_fcc (
			session, message, cancellable, &local_error);

		if (local_error != NULL) {
			g_warn_if_fail (folder == NULL);
			g_propagate_error (error, local_error);
			return NULL;
		}
	}

	/* Check the default mail identity. */
	if (folder == NULL) {
		GError *local_error = NULL;

		/* This may return NULL without setting a GError. */
		folder = mail_session_ref_fcc_from_default_identity (
			session, message, &use_sent_folder, cancellable, &local_error);

		if (local_error != NULL) {
			g_warn_if_fail (folder == NULL);
			g_propagate_error (error, local_error);
			return NULL;
		}

		if (!use_sent_folder) {
			if (out_use_sent_folder)
				*out_use_sent_folder = use_sent_folder;
			return NULL;
		}
	}

	/* Last resort - local Sent folder. */
	if (folder == NULL) {
		folder = e_mail_session_get_local_folder (
			session, E_MAIL_LOCAL_FOLDER_SENT);
		g_object_ref (folder);
	}

	return folder;
}

/* Helper for e_mail_session_get_fcc_for_message() */
static void
mail_session_get_fcc_for_message_thread (GTask *task,
                                         gpointer source_object,
                                         gpointer task_data,
                                         GCancellable *cancellable)
{
	EMailSession *session;
	CamelMimeMessage *message;
	GError *local_error = NULL;
	FccForMsgResult *res;

	session = E_MAIL_SESSION (source_object);
	message = CAMEL_MIME_MESSAGE (task_data);

	res = g_new0 (FccForMsgResult, 1);
	res->folder = e_mail_session_get_fcc_for_message_sync (
			session,
			message,
			&res->use_sent_folder,
			cancellable, &local_error);

	if (local_error != NULL)
		g_task_return_error (task, g_steal_pointer (&local_error));
	else
		g_task_return_pointer (task, g_steal_pointer (&res), fcc_for_msg_result_free);

	g_clear_pointer (&res, fcc_for_msg_result_free);
}

/**
 * e_mail_session_get_fcc_for_message:
 * @session: an #EMailSession
 * @message: a #CamelMimeMessage
 * @io_priority: the I/O priority of the request
 * @cancellable: optional #GCancellable object, or %NULL
 * @callback: a #GAsyncReadyCallback to call when the request is satisfied
 * @user_data: data to pass to the callback function
 *
 * Asynchronously obtains the preferred "carbon-copy" folder (a.k.a Fcc) for
 * @message by first checking @message for an "X-Evolution-Identity" header,
 * and then an "X-Evolution-Fcc" header.  Failing that, the function checks
 * the default mail identity (if available), and failing even that, the
 * function falls back to the Sent folder from the built-in mail store.
 *
 * Where applicable, the function attempts to honor the
 * #ESourceMailSubmission:replies-to-origin-folder preference.
 *
 * When the operation is finished, @callback will be called.  You can then
 * call e_mail_session_get_fcc_for_message_finish() to get the result of the
 * operation.
 **/
void
e_mail_session_get_fcc_for_message (EMailSession *session,
                                    CamelMimeMessage *message,
                                    gint io_priority,
                                    GCancellable *cancellable,
                                    GAsyncReadyCallback callback,
                                    gpointer user_data)
{
	GTask *task;

	g_return_if_fail (E_IS_MAIL_SESSION (session));
	g_return_if_fail (CAMEL_IS_MIME_MESSAGE (message));

	task = g_task_new (session, cancellable, callback, user_data);
	g_task_set_source_tag (task, e_mail_session_get_fcc_for_message);
	g_task_set_priority (task, io_priority);
	g_task_set_task_data (task, g_object_ref (message), g_object_unref);

	g_task_run_in_thread (task, mail_session_get_fcc_for_message_thread);

	g_object_unref (task);
}

/**
 * e_mail_session_get_fcc_for_message_finish:
 * @session: an #EMailSession
 * @result: a #GAsyncResult
 * @out_use_sent_folder: (out) (nullable): optional return location to store
 *    corresponding use-sent-folder for the mail account, or %NULL
 * @error: return location for a #GError, or %NULL
 *
 * Finishes the operation started with e_mail_session_get_fcc_for_message().
 *
 * The returned #CamelFolder is referenced for thread-safety and must be
 * unreferenced with g_object_unref() when finished with it.
 *
 * If a non-recoverable error occurred, the function sets @error and
 * returns %NULL. It returns %NULL without setting @error when the mail account
 * has set to not use sent folder, in which case it indicates that
 * in @out_use_sent_folder too.
 *
 * Returns: a #CamelFolder, or %NULL
 **/
CamelFolder *
e_mail_session_get_fcc_for_message_finish (EMailSession *session,
                                           GAsyncResult *result,
					   gboolean *out_use_sent_folder,
                                           GError **error)
{
	FccForMsgResult *res;
	CamelFolder *folder;

	g_return_val_if_fail (g_task_is_valid (result, session), NULL);
	g_return_val_if_fail (g_async_result_is_tagged (result, e_mail_session_get_fcc_for_message), NULL);

	res = g_task_propagate_pointer (G_TASK (result), error);
	if (!res)
		return NULL;

	if (out_use_sent_folder)
		*out_use_sent_folder = res->use_sent_folder;

	if (!res->use_sent_folder) {
		g_return_val_if_fail (res->folder == NULL, NULL);
		g_clear_pointer (&res, fcc_for_msg_result_free);
		return NULL;
	}

	folder = g_steal_pointer (&res->folder);
	g_clear_pointer (&res, fcc_for_msg_result_free);
	return g_steal_pointer (&folder);
}

/**
 * e_mail_session_ref_transport:
 * @session: an #EMailSession
 * @transport_uid: the UID of a mail transport
 *
 * Returns the transport #CamelService instance for @transport_uid,
 * verifying first that the @transport_uid is indeed a mail transport and
 * that the corresponding #ESource is enabled.  If these checks fail, the
 * function returns %NULL.
 *
 * The returned #CamelService is referenced for thread-safety and must be
 * unreferenced with g_object_unref() when finished with it.
 *
 * Returns: a #CamelService, or %NULL
 **/
CamelService *
e_mail_session_ref_transport (EMailSession *session,
                              const gchar *transport_uid)
{
	ESourceRegistry *registry;
	ESource *source = NULL;
	CamelService *transport = NULL;
	const gchar *extension_name;

	g_return_val_if_fail (E_IS_MAIL_SESSION (session), NULL);
	g_return_val_if_fail (transport_uid != NULL, NULL);

	registry = e_mail_session_get_registry (session);
	extension_name = E_SOURCE_EXTENSION_MAIL_TRANSPORT;

	source = e_source_registry_ref_source (registry, transport_uid);

	if (source == NULL)
		goto exit;

	if (!e_source_registry_check_enabled (registry, source))
		goto exit;

	if (!e_source_has_extension (source, extension_name))
		goto exit;

	transport = camel_session_ref_service (
		CAMEL_SESSION (session), transport_uid);

	/* Sanity check. */
	if (transport != NULL)
		g_warn_if_fail (CAMEL_IS_TRANSPORT (transport));

exit:
	g_clear_object (&source);

	return transport;
}

/* Helper for e_mail_session_ref_default_transport()
 * and mail_session_ref_transport_from_x_identity(). */
static CamelService *
mail_session_ref_transport_for_identity (EMailSession *session,
                                         ESource *source)
{
	ESourceRegistry *registry;
	ESourceMailSubmission *extension;
	CamelService *transport = NULL;
	const gchar *extension_name;
	gchar *uid;

	registry = e_mail_session_get_registry (session);
	extension_name = E_SOURCE_EXTENSION_MAIL_SUBMISSION;

	if (source == NULL)
		return NULL;

	if (!e_source_registry_check_enabled (registry, source))
		return NULL;

	if (!e_source_has_extension (source, extension_name))
		return NULL;

	extension = e_source_get_extension (source, extension_name);
	uid = e_source_mail_submission_dup_transport_uid (extension);

	if (uid != NULL) {
		transport = e_mail_session_ref_transport (session, uid);
		g_free (uid);
	}

	return transport;
}

/**
 * e_mail_session_ref_default_transport:
 * @session: an #EMailSession
 *
 * Returns the default transport #CamelService instance according to
 * #ESourceRegistry's #ESourceRegistry:default-mail-identity setting,
 * verifying first that the #ESourceMailSubmission:transport-uid named by
 * the #ESourceRegistry:default-mail-identity is indeed a mail transport,
 * and that the corresponding #ESource is enabled.  If these checks fail,
 * the function returns %NULL.
 *
 * The returned #CamelService is referenced for thread-safety and must be
 * unreferenced with g_object_unref() when finished with it.
 *
 * Returns: a #CamelService, or %NULL
 **/
CamelService *
e_mail_session_ref_default_transport (EMailSession *session)
{
	ESource *source;
	ESourceRegistry *registry;
	CamelService *transport;

	g_return_val_if_fail (E_IS_MAIL_SESSION (session), NULL);

	registry = e_mail_session_get_registry (session);
	source = e_source_registry_ref_default_mail_identity (registry);
	transport = mail_session_ref_transport_for_identity (session, source);
	g_clear_object (&source);

	return transport;
}

/* Helper for e_mail_session_ref_transport_for_message() */
static CamelService *
mail_session_ref_transport_from_x_identity (EMailSession *session,
                                            CamelMimeMessage *message)
{
	ESource *source;
	ESourceRegistry *registry;
	CamelMedium *medium;
	CamelService *transport;
	const gchar *header_name;
	const gchar *header_value;
	gchar *uid;

	medium = CAMEL_MEDIUM (message);
	header_name = "X-Evolution-Identity";
	header_value = camel_medium_get_header (medium, header_name);

	if (header_value == NULL)
		return NULL;

	uid = g_strstrip (g_strdup (header_value));

	registry = e_mail_session_get_registry (session);
	source = e_source_registry_ref_source (registry, uid);
	transport = mail_session_ref_transport_for_identity (session, source);
	g_clear_object (&source);

	g_free (uid);

	return transport;
}

/* Helper for e_mail_session_ref_transport_for_message() */
static CamelService *
mail_session_ref_transport_from_x_transport (EMailSession *session,
                                             CamelMimeMessage *message)
{
	CamelMedium *medium;
	CamelService *transport;
	const gchar *header_name;
	const gchar *header_value;
	gchar *uid;

	medium = CAMEL_MEDIUM (message);
	header_name = "X-Evolution-Transport";
	header_value = camel_medium_get_header (medium, header_name);

	if (header_value == NULL)
		return NULL;

	uid = g_strstrip (g_strdup (header_value));

	transport = e_mail_session_ref_transport (session, uid);

	g_free (uid);

	return transport;
}

/**
 * e_mail_session_ref_transport_for_message:
 * @session: an #EMailSession
 * @message: a #CamelMimeMessage
 *
 * Returns the preferred transport #CamelService instance for @message by
 * first checking @message for an "X-Evolution-Identity" header, and then
 * an "X-Evolution-Transport" header.  Failing that, the function returns
 * the default transport #CamelService instance (if available).
 *
 * The returned #CamelService is referenced for thread-safety and must be
 * unreferenced with g_object_unref() when finished with it.
 *
 * Returns: a #CamelService, or %NULL
 **/
CamelService *
e_mail_session_ref_transport_for_message (EMailSession *session,
                                          CamelMimeMessage *message)
{
	CamelService *transport = NULL;

	g_return_val_if_fail (E_IS_MAIL_SESSION (session), NULL);
	g_return_val_if_fail (CAMEL_IS_MIME_MESSAGE (message), NULL);

	/* Check for "X-Evolution-Identity" header. */
	if (transport == NULL)
		transport = mail_session_ref_transport_from_x_identity (
			session, message);

	/* Check for "X-Evolution-Transport" header. */
	if (transport == NULL)
		transport = mail_session_ref_transport_from_x_transport (
			session, message);

	/* Fall back to the default mail transport. */
	if (transport == NULL)
		transport = e_mail_session_ref_default_transport (session);

	return transport;
}

