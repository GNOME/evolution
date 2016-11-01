/*
 * e-mail-folder-utils.c
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

#include "e-mail-folder-utils.h"

#include <glib/gi18n-lib.h>

#include <libedataserver/libedataserver.h>

#include <libemail-engine/e-mail-session.h>
#include <libemail-engine/mail-tools.h>

#include "e-mail-utils.h"

/* X-Mailer header value */
#define X_MAILER ("Evolution " VERSION VERSION_SUBSTRING " " VERSION_COMMENT)

typedef struct _AsyncContext AsyncContext;

struct _AsyncContext {
	CamelMimeMessage *message;
	CamelMessageInfo *info;
	CamelMimePart *part;
	GHashTable *hash_table;
	GPtrArray *ptr_array;
	GFile *destination;
	gchar *fwd_subject;
	gchar *message_uid;
};

static void
async_context_free (AsyncContext *context)
{
	if (context->hash_table != NULL)
		g_hash_table_unref (context->hash_table);

	if (context->ptr_array != NULL)
		g_ptr_array_unref (context->ptr_array);

	g_clear_object (&context->message);
	g_clear_object (&context->info);
	g_clear_object (&context->part);
	g_clear_object (&context->destination);

	g_free (context->fwd_subject);
	g_free (context->message_uid);

	g_slice_free (AsyncContext, context);
}

static void
mail_folder_append_message_thread (GSimpleAsyncResult *simple,
                                   GObject *object,
                                   GCancellable *cancellable)
{
	AsyncContext *context;
	GError *error = NULL;

	context = g_simple_async_result_get_op_res_gpointer (simple);

	e_mail_folder_append_message_sync (
		CAMEL_FOLDER (object), context->message,
		context->info, &context->message_uid,
		cancellable, &error);

	if (error != NULL)
		g_simple_async_result_take_error (simple, error);
}

gboolean
e_mail_folder_append_message_sync (CamelFolder *folder,
                                   CamelMimeMessage *message,
                                   CamelMessageInfo *info,
                                   gchar **appended_uid,
                                   GCancellable *cancellable,
                                   GError **error)
{
	CamelMedium *medium;
	gchar *full_display_name;
	gboolean success;

	g_return_val_if_fail (CAMEL_IS_FOLDER (folder), FALSE);
	g_return_val_if_fail (CAMEL_IS_MIME_MESSAGE (message), FALSE);

	medium = CAMEL_MEDIUM (message);

	full_display_name = e_mail_folder_to_full_display_name (folder, NULL);
	camel_operation_push_message (
		cancellable,
		_("Saving message to folder '%s'"),
		full_display_name ? full_display_name : camel_folder_get_display_name (folder));
	g_free (full_display_name);

	if (camel_medium_get_header (medium, "X-Mailer") == NULL)
		camel_medium_set_header (medium, "X-Mailer", X_MAILER);

	camel_mime_message_set_date (message, CAMEL_MESSAGE_DATE_CURRENT, 0);

	success = camel_folder_append_message_sync (
		folder, message, info, appended_uid, cancellable, error);

	camel_operation_pop_message (cancellable);

	return success;
}

void
e_mail_folder_append_message (CamelFolder *folder,
                              CamelMimeMessage *message,
                              CamelMessageInfo *info,
                              gint io_priority,
                              GCancellable *cancellable,
                              GAsyncReadyCallback callback,
                              gpointer user_data)
{
	GSimpleAsyncResult *simple;
	AsyncContext *context;

	g_return_if_fail (CAMEL_IS_FOLDER (folder));
	g_return_if_fail (CAMEL_IS_MIME_MESSAGE (message));

	context = g_slice_new0 (AsyncContext);
	context->message = g_object_ref (message);

	if (info != NULL)
		context->info = g_object_ref (info);

	simple = g_simple_async_result_new (
		G_OBJECT (folder), callback, user_data,
		e_mail_folder_append_message);

	g_simple_async_result_set_check_cancellable (simple, cancellable);

	g_simple_async_result_set_op_res_gpointer (
		simple, context, (GDestroyNotify) async_context_free);

	g_simple_async_result_run_in_thread (
		simple, mail_folder_append_message_thread,
		io_priority, cancellable);

	g_object_unref (simple);
}

gboolean
e_mail_folder_append_message_finish (CamelFolder *folder,
                                     GAsyncResult *result,
                                     gchar **appended_uid,
                                     GError **error)
{
	GSimpleAsyncResult *simple;
	AsyncContext *context;

	g_return_val_if_fail (
		g_simple_async_result_is_valid (
		result, G_OBJECT (folder),
		e_mail_folder_append_message), FALSE);

	simple = G_SIMPLE_ASYNC_RESULT (result);
	context = g_simple_async_result_get_op_res_gpointer (simple);

	if (appended_uid != NULL) {
		*appended_uid = context->message_uid;
		context->message_uid = NULL;
	}

	/* Assume success unless a GError is set. */
	return !g_simple_async_result_propagate_error (simple, error);
}

static void
mail_folder_expunge_thread (GSimpleAsyncResult *simple,
                            GObject *object,
                            GCancellable *cancellable)
{
	GError *error = NULL;

	e_mail_folder_expunge_sync (
		CAMEL_FOLDER (object), cancellable, &error);

	if (error != NULL)
		g_simple_async_result_take_error (simple, error);
}

static gboolean
mail_folder_expunge_pop3_stores (CamelFolder *folder,
                                 GCancellable *cancellable,
                                 GError **error)
{
	GHashTable *expunging_uids;
	CamelStore *parent_store;
	CamelService *service;
	CamelSession *session;
	ESourceRegistry *registry;
	GPtrArray *uids;
	GList *list, *link;
	const gchar *extension_name;
	gboolean success = TRUE;
	guint ii;

	parent_store = camel_folder_get_parent_store (folder);

	service = CAMEL_SERVICE (parent_store);
	session = camel_service_ref_session (service);
	registry = e_mail_session_get_registry (E_MAIL_SESSION (session));

	uids = camel_folder_get_uids (folder);

	if (uids == NULL)
		goto exit;

	expunging_uids = g_hash_table_new_full (
		(GHashFunc) g_str_hash,
		(GEqualFunc) g_str_equal,
		(GDestroyNotify) g_free,
		(GDestroyNotify) g_free);

	for (ii = 0; ii < uids->len; ii++) {
		CamelMessageInfo *info;
		CamelMessageFlags flags = 0;
		CamelMimeMessage *message;
		const gchar *pop3_uid;
		const gchar *source_uid;

		info = camel_folder_get_message_info (
			folder, uids->pdata[ii]);

		if (info != NULL) {
			flags = camel_message_info_get_flags (info);
			g_clear_object (&info);
		}

		/* Only interested in deleted messages. */
		if ((flags & CAMEL_MESSAGE_DELETED) == 0)
			continue;

		/* because the UID in the local store doesn't
		 * match with the UID in the pop3 store */
		message = camel_folder_get_message_sync (
			folder, uids->pdata[ii], cancellable, NULL);

		if (message == NULL)
			continue;

		pop3_uid = camel_medium_get_header (
			CAMEL_MEDIUM (message), "X-Evolution-POP3-UID");
		source_uid = camel_mime_message_get_source (message);

		if (pop3_uid != NULL)
			g_hash_table_insert (
				expunging_uids,
				g_strstrip (g_strdup (pop3_uid)),
				g_strstrip (g_strdup (source_uid)));

		g_object_unref (message);
	}

	camel_folder_free_uids (folder, uids);
	uids = NULL;

	if (g_hash_table_size (expunging_uids) == 0) {
		g_hash_table_destroy (expunging_uids);
		return TRUE;
	}

	extension_name = E_SOURCE_EXTENSION_MAIL_ACCOUNT;
	list = e_source_registry_list_enabled (registry, extension_name);

	for (link = list; link != NULL; link = g_list_next (link)) {
		ESource *source = E_SOURCE (link->data);
		ESourceBackend *extension;
		CamelFolder *folder;
		CamelService *service;
		CamelSettings *settings;
		const gchar *backend_name;
		const gchar *service_uid;
		const gchar *source_uid;
		gboolean any_found = FALSE;
		gboolean delete_expunged = FALSE;
		gboolean keep_on_server = FALSE;

		source_uid = e_source_get_uid (source);

		extension = e_source_get_extension (source, extension_name);
		backend_name = e_source_backend_get_backend_name (extension);

		if (g_strcmp0 (backend_name, "pop") != 0)
			continue;

		service = camel_session_ref_service (
			CAMEL_SESSION (session), source_uid);

		service_uid = camel_service_get_uid (service);
		settings = camel_service_ref_settings (service);

		g_object_get (
			settings,
			"delete-expunged", &delete_expunged,
			"keep-on-server", &keep_on_server,
			NULL);

		g_object_unref (settings);

		if (!keep_on_server || !delete_expunged) {
			g_object_unref (service);
			continue;
		}

		folder = camel_store_get_inbox_folder_sync (
			CAMEL_STORE (service), cancellable, error);

		/* Abort the loop on error. */
		if (folder == NULL) {
			g_object_unref (service);
			success = FALSE;
			break;
		}

		uids = camel_folder_get_uids (folder);

		if (uids == NULL) {
			g_object_unref (service);
			g_object_unref (folder);
			continue;
		}

		for (ii = 0; ii < uids->len; ii++) {
			const gchar *source_uid;

			source_uid = g_hash_table_lookup (
				expunging_uids, uids->pdata[ii]);
			if (g_strcmp0 (source_uid, service_uid) == 0) {
				any_found = TRUE;
				camel_folder_delete_message (
					folder, uids->pdata[ii]);
			}
		}

		camel_folder_free_uids (folder, uids);

		if (any_found)
			success = camel_folder_synchronize_sync (
				folder, TRUE, cancellable, error);

		g_object_unref (folder);
		g_object_unref (service);

		/* Abort the loop on error. */
		if (!success)
			break;
	}

	g_list_free_full (list, (GDestroyNotify) g_object_unref);

	g_hash_table_destroy (expunging_uids);

exit:
	g_object_unref (session);

	return success;
}

gboolean
e_mail_folder_expunge_sync (CamelFolder *folder,
                            GCancellable *cancellable,
                            GError **error)
{
	CamelFolder *local_inbox;
	CamelStore *parent_store;
	CamelService *service;
	CamelSession *session;
	gboolean is_local_inbox;
	gboolean is_local_trash;
	gboolean store_is_local;
	gboolean success = TRUE;
	const gchar *uid;

	g_return_val_if_fail (CAMEL_IS_FOLDER (folder), FALSE);

	parent_store = camel_folder_get_parent_store (folder);

	service = CAMEL_SERVICE (parent_store);
	session = camel_service_ref_session (service);

	uid = camel_service_get_uid (service);
	store_is_local = (g_strcmp0 (uid, E_MAIL_SESSION_LOCAL_UID) == 0);

	local_inbox = e_mail_session_get_local_folder (
		E_MAIL_SESSION (session), E_MAIL_LOCAL_FOLDER_INBOX);
	is_local_inbox = (folder == local_inbox);
	is_local_trash = FALSE;

	if (store_is_local && !is_local_inbox) {
		CamelFolder *local_trash;

		local_trash = camel_store_get_trash_folder_sync (
			parent_store, cancellable, error);

		if (local_trash != NULL) {
			is_local_trash = (folder == local_trash);
			g_object_unref (local_trash);
		} else {
			success = FALSE;
			goto exit;
		}
	}

	/* Expunge all POP3 accounts when expunging
	 * the local Inbox or Trash folder. */
	if (is_local_inbox || is_local_trash)
		success = mail_folder_expunge_pop3_stores (
			folder, cancellable, error);

	if (success)
		success = camel_folder_expunge_sync (
			folder, cancellable, error);

exit:
	g_object_unref (session);

	return success;
}

void
e_mail_folder_expunge (CamelFolder *folder,
                       gint io_priority,
                       GCancellable *cancellable,
                       GAsyncReadyCallback callback,
                       gpointer user_data)
{
	GSimpleAsyncResult *simple;

	g_return_if_fail (CAMEL_IS_FOLDER (folder));

	simple = g_simple_async_result_new (
		G_OBJECT (folder), callback,
		user_data, e_mail_folder_expunge);

	g_simple_async_result_set_check_cancellable (simple, cancellable);

	g_simple_async_result_run_in_thread (
		simple, mail_folder_expunge_thread,
		io_priority, cancellable);

	g_object_unref (simple);
}

gboolean
e_mail_folder_expunge_finish (CamelFolder *folder,
                              GAsyncResult *result,
                              GError **error)
{
	GSimpleAsyncResult *simple;

	g_return_val_if_fail (
		g_simple_async_result_is_valid (
		result, G_OBJECT (folder),
		e_mail_folder_expunge), FALSE);

	simple = G_SIMPLE_ASYNC_RESULT (result);

	/* Assume success unless a GError is set. */
	return !g_simple_async_result_propagate_error (simple, error);
}

static void
mail_folder_build_attachment_thread (GSimpleAsyncResult *simple,
                                     GObject *object,
                                     GCancellable *cancellable)
{
	AsyncContext *context;
	GError *error = NULL;

	context = g_simple_async_result_get_op_res_gpointer (simple);

	context->part = e_mail_folder_build_attachment_sync (
		CAMEL_FOLDER (object), context->ptr_array,
		&context->fwd_subject, cancellable, &error);

	if (error != NULL)
		g_simple_async_result_take_error (simple, error);
}

CamelMimePart *
e_mail_folder_build_attachment_sync (CamelFolder *folder,
                                     GPtrArray *message_uids,
                                     gchar **fwd_subject,
                                     GCancellable *cancellable,
                                     GError **error)
{
	GHashTable *hash_table;
	CamelMimeMessage *message;
	CamelMimePart *part;
	const gchar *uid;

	g_return_val_if_fail (CAMEL_IS_FOLDER (folder), NULL);
	g_return_val_if_fail (message_uids != NULL, NULL);

	/* Need at least one message UID to make an attachment. */
	g_return_val_if_fail (message_uids->len > 0, NULL);

	hash_table = e_mail_folder_get_multiple_messages_sync (
		folder, message_uids, cancellable, error);

	if (hash_table == NULL)
		return NULL;

	/* Create the forward subject from the first message. */

	uid = g_ptr_array_index (message_uids, 0);
	g_return_val_if_fail (uid != NULL, NULL);

	message = g_hash_table_lookup (hash_table, uid);
	g_return_val_if_fail (message != NULL, NULL);

	if (fwd_subject != NULL)
		*fwd_subject = mail_tool_generate_forward_subject (message);

	if (message_uids->len == 1) {
		part = mail_tool_make_message_attachment (message);

	} else {
		CamelMultipart *multipart;
		guint ii;

		multipart = camel_multipart_new ();
		camel_data_wrapper_set_mime_type (
			CAMEL_DATA_WRAPPER (multipart), "multipart/digest");
		camel_multipart_set_boundary (multipart, NULL);

		for (ii = 0; ii < message_uids->len; ii++) {
			uid = g_ptr_array_index (message_uids, ii);
			g_return_val_if_fail (uid != NULL, NULL);

			message = g_hash_table_lookup (hash_table, uid);
			g_return_val_if_fail (message != NULL, NULL);

			part = mail_tool_make_message_attachment (message);
			camel_multipart_add_part (multipart, part);
			g_object_unref (part);
		}

		part = camel_mime_part_new ();

		camel_medium_set_content (
			CAMEL_MEDIUM (part),
			CAMEL_DATA_WRAPPER (multipart));

		camel_mime_part_set_description (
			part, _("Forwarded messages"));

		g_object_unref (multipart);
	}

	g_hash_table_unref (hash_table);

	return part;
}

void
e_mail_folder_build_attachment (CamelFolder *folder,
                                GPtrArray *message_uids,
                                gint io_priority,
                                GCancellable *cancellable,
                                GAsyncReadyCallback callback,
                                gpointer user_data)
{
	GSimpleAsyncResult *simple;
	AsyncContext *context;

	g_return_if_fail (CAMEL_IS_FOLDER (folder));
	g_return_if_fail (message_uids != NULL);

	/* Need at least one message UID to make an attachment. */
	g_return_if_fail (message_uids->len > 0);

	context = g_slice_new0 (AsyncContext);
	context->ptr_array = g_ptr_array_ref (message_uids);

	simple = g_simple_async_result_new (
		G_OBJECT (folder), callback, user_data,
		e_mail_folder_build_attachment);

	g_simple_async_result_set_check_cancellable (simple, cancellable);

	g_simple_async_result_set_op_res_gpointer (
		simple, context, (GDestroyNotify) async_context_free);

	g_simple_async_result_run_in_thread (
		simple, mail_folder_build_attachment_thread,
		io_priority, cancellable);

	g_object_unref (simple);
}

CamelMimePart *
e_mail_folder_build_attachment_finish (CamelFolder *folder,
                                       GAsyncResult *result,
                                       gchar **fwd_subject,
                                       GError **error)
{
	GSimpleAsyncResult *simple;
	AsyncContext *context;

	g_return_val_if_fail (
		g_simple_async_result_is_valid (
		result, G_OBJECT (folder),
		e_mail_folder_build_attachment), NULL);

	simple = G_SIMPLE_ASYNC_RESULT (result);
	context = g_simple_async_result_get_op_res_gpointer (simple);

	if (g_simple_async_result_propagate_error (simple, error))
		return NULL;

	if (fwd_subject != NULL) {
		*fwd_subject = context->fwd_subject;
		context->fwd_subject = NULL;
	}

	g_return_val_if_fail (CAMEL_IS_MIME_PART (context->part), NULL);

	return g_object_ref (context->part);
}

static void
mail_folder_find_duplicate_messages_thread (GSimpleAsyncResult *simple,
                                            GObject *object,
                                            GCancellable *cancellable)
{
	AsyncContext *context;
	GError *error = NULL;

	context = g_simple_async_result_get_op_res_gpointer (simple);

	context->hash_table = e_mail_folder_find_duplicate_messages_sync (
		CAMEL_FOLDER (object), context->ptr_array,
		cancellable, &error);

	if (error != NULL)
		g_simple_async_result_take_error (simple, error);
}

static GHashTable *
emfu_get_messages_hash_sync (CamelFolder *folder,
                             GPtrArray *message_uids,
                             GCancellable *cancellable,
                             GError **error)
{
	GHashTable *hash_table;
	CamelMimeMessage *message;
	guint ii;

	g_return_val_if_fail (CAMEL_IS_FOLDER (folder), NULL);
	g_return_val_if_fail (message_uids != NULL, NULL);

	camel_operation_push_message (
		cancellable,
		ngettext (
			"Retrieving %d message",
			"Retrieving %d messages",
			message_uids->len),
		message_uids->len);

	hash_table = g_hash_table_new_full (
		(GHashFunc) g_str_hash,
		(GEqualFunc) g_str_equal,
		(GDestroyNotify) g_free,
		(GDestroyNotify) g_free);

	/* This is an all or nothing operation.  Destroy the
	 * hash table if we fail to retrieve any message. */

	for (ii = 0; ii < message_uids->len; ii++) {
		const gchar *uid;
		gint percent;

		uid = g_ptr_array_index (message_uids, ii);
		percent = ((ii + 1) * 100) / message_uids->len;

		message = camel_folder_get_message_sync (
			folder, uid, cancellable, error);

		camel_operation_progress (cancellable, percent);

		if (CAMEL_IS_MIME_MESSAGE (message)) {
			CamelDataWrapper *content;
			gchar *digest = NULL;

			/* Generate a digest string from the message's content. */
			content = camel_medium_get_content (CAMEL_MEDIUM (message));

			if (content != NULL) {
				CamelStream *stream;
				GByteArray *buffer;
				gssize n_bytes;

				stream = camel_stream_mem_new ();

				n_bytes = camel_data_wrapper_decode_to_stream_sync (
					content, stream, cancellable, error);

				if (n_bytes >= 0) {
					guint data_len;

					/* The CamelStreamMem owns the buffer. */
					buffer = camel_stream_mem_get_byte_array (
						CAMEL_STREAM_MEM (stream));
					g_return_val_if_fail (buffer != NULL, NULL);

					data_len = buffer->len;

					/* Strip trailing white-spaces and empty lines */
					while (data_len > 0 && g_ascii_isspace (buffer->data[data_len - 1]))
						data_len--;

					if (data_len > 0)
						digest = g_compute_checksum_for_data (G_CHECKSUM_SHA256, buffer->data, data_len);
				}

				g_object_unref (stream);
			}

			g_hash_table_insert (
				hash_table, g_strdup (uid), digest);
			g_object_unref (message);
		} else {
			g_hash_table_destroy (hash_table);
			hash_table = NULL;
			break;
		}
	}

	camel_operation_pop_message (cancellable);

	return hash_table;
}

GHashTable *
e_mail_folder_find_duplicate_messages_sync (CamelFolder *folder,
                                            GPtrArray *message_uids,
                                            GCancellable *cancellable,
                                            GError **error)
{
	GQueue trash = G_QUEUE_INIT;
	GHashTable *hash_table;
	GHashTable *unique_ids;
	GHashTableIter iter;
	gpointer key, value;

	g_return_val_if_fail (CAMEL_IS_FOLDER (folder), NULL);
	g_return_val_if_fail (message_uids != NULL, NULL);

	/* hash_table = { MessageUID : digest-as-string } */
	hash_table = emfu_get_messages_hash_sync (
		folder, message_uids, cancellable, error);

	if (hash_table == NULL)
		return NULL;

	camel_operation_push_message (
		cancellable, _("Scanning messages for duplicates"));

	unique_ids = g_hash_table_new_full (
		(GHashFunc) g_int64_hash,
		(GEqualFunc) g_int64_equal,
		(GDestroyNotify) g_free,
		(GDestroyNotify) g_free);

	g_hash_table_iter_init (&iter, hash_table);

	while (g_hash_table_iter_next (&iter, &key, &value)) {
		CamelSummaryMessageID message_id;
		CamelMessageFlags flags;
		CamelMessageInfo *info;
		gboolean duplicate;
		const gchar *digest;

		info = camel_folder_get_message_info (folder, key);
		if (!info)
			continue;

		message_id.id.id = camel_message_info_get_message_id (info);
		flags = camel_message_info_get_flags (info);

		/* Skip messages marked for deletion. */
		if (flags & CAMEL_MESSAGE_DELETED) {
			g_queue_push_tail (&trash, key);
			g_clear_object (&info);
			continue;
		}

		digest = value;

		if (digest == NULL) {
			g_queue_push_tail (&trash, key);
			g_clear_object (&info);
			continue;
		}

		/* Determine if the message a duplicate. */

		value = g_hash_table_lookup (unique_ids, &message_id.id.id);
		duplicate = (value != NULL) && g_str_equal (digest, value);

		if (!duplicate) {
			gint64 *v_int64;

			/* XXX Might be better to create a GArray
			 *     of 64-bit integers and have the hash
			 *     table keys point to array elements. */
			v_int64 = g_new0 (gint64, 1);
			*v_int64 = (gint64) message_id.id.id;

			g_hash_table_insert (unique_ids, v_int64, g_strdup (digest));
			g_queue_push_tail (&trash, key);
		}

		g_clear_object (&info);
	}

	/* Delete all non-duplicate messages from the hash table. */
	while ((key = g_queue_pop_head (&trash)) != NULL)
		g_hash_table_remove (hash_table, key);

	camel_operation_pop_message (cancellable);

	g_hash_table_destroy (unique_ids);

	return hash_table;
}

void
e_mail_folder_find_duplicate_messages (CamelFolder *folder,
                                       GPtrArray *message_uids,
                                       gint io_priority,
                                       GCancellable *cancellable,
                                       GAsyncReadyCallback callback,
                                       gpointer user_data)
{
	GSimpleAsyncResult *simple;
	AsyncContext *context;

	g_return_if_fail (CAMEL_IS_FOLDER (folder));
	g_return_if_fail (message_uids != NULL);

	context = g_slice_new0 (AsyncContext);
	context->ptr_array = g_ptr_array_ref (message_uids);

	simple = g_simple_async_result_new (
		G_OBJECT (folder), callback, user_data,
		e_mail_folder_find_duplicate_messages);

	g_simple_async_result_set_check_cancellable (simple, cancellable);

	g_simple_async_result_set_op_res_gpointer (
		simple, context, (GDestroyNotify) async_context_free);

	g_simple_async_result_run_in_thread (
		simple, mail_folder_find_duplicate_messages_thread,
		io_priority, cancellable);

	g_object_unref (simple);
}

GHashTable *
e_mail_folder_find_duplicate_messages_finish (CamelFolder *folder,
                                              GAsyncResult *result,
                                              GError **error)
{
	GSimpleAsyncResult *simple;
	AsyncContext *context;

	g_return_val_if_fail (
		g_simple_async_result_is_valid (
		result, G_OBJECT (folder),
		e_mail_folder_find_duplicate_messages), NULL);

	simple = G_SIMPLE_ASYNC_RESULT (result);
	context = g_simple_async_result_get_op_res_gpointer (simple);

	if (g_simple_async_result_propagate_error (simple, error))
		return NULL;

	return g_hash_table_ref (context->hash_table);
}

static void
mail_folder_get_multiple_messages_thread (GSimpleAsyncResult *simple,
                                          GObject *object,
                                          GCancellable *cancellable)
{
	AsyncContext *context;
	GError *error = NULL;

	context = g_simple_async_result_get_op_res_gpointer (simple);

	context->hash_table = e_mail_folder_get_multiple_messages_sync (
		CAMEL_FOLDER (object), context->ptr_array,
		cancellable, &error);

	if (error != NULL)
		g_simple_async_result_take_error (simple, error);
}

GHashTable *
e_mail_folder_get_multiple_messages_sync (CamelFolder *folder,
                                          GPtrArray *message_uids,
                                          GCancellable *cancellable,
                                          GError **error)
{
	GHashTable *hash_table;
	CamelMimeMessage *message;
	guint ii;

	g_return_val_if_fail (CAMEL_IS_FOLDER (folder), NULL);
	g_return_val_if_fail (message_uids != NULL, NULL);

	camel_operation_push_message (
		cancellable,
		ngettext (
			"Retrieving %d message",
			"Retrieving %d messages",
			message_uids->len),
		message_uids->len);

	hash_table = g_hash_table_new_full (
		(GHashFunc) g_str_hash,
		(GEqualFunc) g_str_equal,
		(GDestroyNotify) g_free,
		(GDestroyNotify) g_object_unref);

	/* This is an all or nothing operation.  Destroy the
	 * hash table if we fail to retrieve any message. */

	for (ii = 0; ii < message_uids->len; ii++) {
		const gchar *uid;
		gint percent;

		uid = g_ptr_array_index (message_uids, ii);
		percent = ((ii + 1) * 100) / message_uids->len;

		message = camel_folder_get_message_sync (
			folder, uid, cancellable, error);

		camel_operation_progress (cancellable, percent);

		if (CAMEL_IS_MIME_MESSAGE (message)) {
			g_hash_table_insert (
				hash_table, g_strdup (uid), message);
		} else {
			g_hash_table_destroy (hash_table);
			hash_table = NULL;
			break;
		}
	}

	camel_operation_pop_message (cancellable);

	return hash_table;
}

void
e_mail_folder_get_multiple_messages (CamelFolder *folder,
                                     GPtrArray *message_uids,
                                     gint io_priority,
                                     GCancellable *cancellable,
                                     GAsyncReadyCallback callback,
                                     gpointer user_data)
{
	GSimpleAsyncResult *simple;
	AsyncContext *context;

	g_return_if_fail (CAMEL_IS_FOLDER (folder));
	g_return_if_fail (message_uids != NULL);

	context = g_slice_new0 (AsyncContext);
	context->ptr_array = g_ptr_array_ref (message_uids);

	simple = g_simple_async_result_new (
		G_OBJECT (folder), callback, user_data,
		e_mail_folder_get_multiple_messages);

	g_simple_async_result_set_check_cancellable (simple, cancellable);

	g_simple_async_result_set_op_res_gpointer (
		simple, context, (GDestroyNotify) async_context_free);

	g_simple_async_result_run_in_thread (
		simple, mail_folder_get_multiple_messages_thread,
		io_priority, cancellable);

	g_object_unref (simple);
}

GHashTable *
e_mail_folder_get_multiple_messages_finish (CamelFolder *folder,
                                            GAsyncResult *result,
                                            GError **error)
{
	GSimpleAsyncResult *simple;
	AsyncContext *context;

	g_return_val_if_fail (
		g_simple_async_result_is_valid (
		result, G_OBJECT (folder),
		e_mail_folder_get_multiple_messages), NULL);

	simple = G_SIMPLE_ASYNC_RESULT (result);
	context = g_simple_async_result_get_op_res_gpointer (simple);

	if (g_simple_async_result_propagate_error (simple, error))
		return NULL;

	return g_hash_table_ref (context->hash_table);
}

static void
mail_folder_remove_thread (GSimpleAsyncResult *simple,
                           GObject *object,
                           GCancellable *cancellable)
{
	GError *error = NULL;

	e_mail_folder_remove_sync (
		CAMEL_FOLDER (object), cancellable, &error);

	if (error != NULL)
		g_simple_async_result_take_error (simple, error);
}

static gboolean
mail_folder_remove_recursive (CamelStore *store,
                              CamelFolderInfo *folder_info,
                              GCancellable *cancellable,
                              GError **error)
{
	gboolean success = TRUE;

	while (folder_info != NULL) {
		CamelFolder *folder;

		if (folder_info->child != NULL) {
			success = mail_folder_remove_recursive (
				store, folder_info->child, cancellable, error);
			if (!success)
				break;
		}

		folder = camel_store_get_folder_sync (
			store, folder_info->full_name, 0, cancellable, error);
		if (folder == NULL) {
			success = FALSE;
			break;
		}

		if (!CAMEL_IS_VEE_FOLDER (folder)) {
			GPtrArray *uids;
			guint ii;

			/* Delete every message in this folder,
			 * then expunge it. */

			camel_folder_freeze (folder);

			uids = camel_folder_get_uids (folder);

			for (ii = 0; ii < uids->len; ii++)
				camel_folder_delete_message (
					folder, uids->pdata[ii]);

			camel_folder_free_uids (folder, uids);

			success = camel_folder_synchronize_sync (
				folder, TRUE, cancellable, error);

			camel_folder_thaw (folder);
		}

		g_object_unref (folder);

		if (!success)
			break;

		/* If the store supports subscriptions,
		 * then unsubscribe from this folder. */
		if (CAMEL_IS_SUBSCRIBABLE (store)) {
			success = camel_subscribable_unsubscribe_folder_sync (
				CAMEL_SUBSCRIBABLE (store),
				folder_info->full_name,
				cancellable, error);
			if (!success)
				break;
		}

		success = camel_store_delete_folder_sync (
			store, folder_info->full_name, cancellable, error);
		if (!success)
			break;

		folder_info = folder_info->next;
	}

	return success;
}

static void
follow_cancel_cb (GCancellable *cancellable,
                  GCancellable *transparent_cancellable)
{
	g_cancellable_cancel (transparent_cancellable);
}

gboolean
e_mail_folder_remove_sync (CamelFolder *folder,
                           GCancellable *cancellable,
                           GError **error)
{
	CamelFolderInfo *folder_info;
	CamelFolderInfo *to_remove;
	CamelFolderInfo *next = NULL;
	CamelStore *parent_store;
	const gchar *full_name;
	gchar *full_display_name;
	gboolean success = TRUE;
	GCancellable *transparent_cancellable = NULL;
	gulong cbid = 0;

	g_return_val_if_fail (CAMEL_IS_FOLDER (folder), FALSE);

	full_name = camel_folder_get_full_name (folder);
	parent_store = camel_folder_get_parent_store (folder);

	full_display_name = e_mail_folder_to_full_display_name (folder, NULL);
	camel_operation_push_message (cancellable, _("Removing folder '%s'"),
		full_display_name ? full_display_name : camel_folder_get_display_name (folder));
	g_free (full_display_name);

	if (cancellable) {
		transparent_cancellable = g_cancellable_new ();
		cbid = g_cancellable_connect (
			cancellable, G_CALLBACK (follow_cancel_cb),
			transparent_cancellable, NULL);
	}

	if ((camel_store_get_flags (parent_store) & CAMEL_STORE_CAN_DELETE_FOLDERS_AT_ONCE) != 0) {
		success = camel_store_delete_folder_sync (
			parent_store, full_name, transparent_cancellable, error);
	} else {
		folder_info = camel_store_get_folder_info_sync (
			parent_store, full_name,
			CAMEL_STORE_FOLDER_INFO_RECURSIVE |
			CAMEL_STORE_FOLDER_INFO_SUBSCRIBED,
			cancellable, error);

		if (folder_info == NULL) {
			success = FALSE;
			goto exit;
		}

		to_remove = folder_info;

		/* For cases when the top-level folder_info contains siblings,
		 * such as when full_name contains a wildcard letter, compare
		 * the folder name against folder_info->full_name to avoid
		 * removing more folders than requested. */
		if (folder_info->next != NULL) {
			while (to_remove != NULL) {
				if (g_strcmp0 (to_remove->full_name, full_name) == 0)
					break;
				to_remove = to_remove->next;
			}

			/* XXX Should we set a GError and return FALSE here? */
			if (to_remove == NULL) {
				g_warning ("%s: Failed to find folder '%s'", G_STRFUNC, full_name);
				camel_folder_info_free (folder_info);
				success = TRUE;
				goto exit;
			}

			/* Prevent iterating over siblings. */
			next = to_remove->next;
			to_remove->next = NULL;
		}

		success = mail_folder_remove_recursive (
				parent_store, to_remove, transparent_cancellable, error);

		/* Restore the folder_info tree to its original
		 * state so we don't leak folder_info nodes. */
		to_remove->next = next;

		camel_folder_info_free (folder_info);
	}

exit:
	if (transparent_cancellable) {
		g_cancellable_disconnect (cancellable, cbid);
		g_object_unref (transparent_cancellable);
	}

	camel_operation_pop_message (cancellable);

	return success;
}

void
e_mail_folder_remove (CamelFolder *folder,
                      gint io_priority,
                      GCancellable *cancellable,
                      GAsyncReadyCallback callback,
                      gpointer user_data)
{
	GSimpleAsyncResult *simple;

	g_return_if_fail (CAMEL_IS_FOLDER (folder));

	simple = g_simple_async_result_new (
		G_OBJECT (folder), callback,
		user_data, e_mail_folder_remove);

	g_simple_async_result_set_check_cancellable (simple, cancellable);

	g_simple_async_result_run_in_thread (
		simple, mail_folder_remove_thread,
		io_priority, cancellable);

	g_object_unref (simple);
}

gboolean
e_mail_folder_remove_finish (CamelFolder *folder,
                             GAsyncResult *result,
                             GError **error)
{
	GSimpleAsyncResult *simple;

	g_return_val_if_fail (
		g_simple_async_result_is_valid (
		result, G_OBJECT (folder),
		e_mail_folder_remove), FALSE);

	simple = G_SIMPLE_ASYNC_RESULT (result);

	/* Assume success unless a GError is set. */
	return !g_simple_async_result_propagate_error (simple, error);
}

static void
mail_folder_remove_attachments_thread (GSimpleAsyncResult *simple,
                                       GObject *object,
                                       GCancellable *cancellable)
{
	AsyncContext *context;
	GError *error = NULL;

	context = g_simple_async_result_get_op_res_gpointer (simple);

	e_mail_folder_remove_attachments_sync (
		CAMEL_FOLDER (object), context->ptr_array,
		cancellable, &error);

	if (error != NULL)
		g_simple_async_result_take_error (simple, error);
}

static gboolean
mail_folder_strip_message_level (CamelMimePart *in_part,
				 GCancellable *cancellable)
{
	CamelDataWrapper *content;
	CamelMultipart *multipart;
	gboolean modified = FALSE;
	guint ii, n_parts;

	g_return_val_if_fail (CAMEL_IS_MIME_PART (in_part), FALSE);

	content = camel_medium_get_content (CAMEL_MEDIUM (in_part));

	if (CAMEL_IS_MIME_MESSAGE (content)) {
		return mail_folder_strip_message_level (CAMEL_MIME_PART (content), cancellable);
	}

	if (!CAMEL_IS_MULTIPART (content))
		return FALSE;

	multipart = CAMEL_MULTIPART (content);
	n_parts = camel_multipart_get_number (multipart);

	/* Replace MIME parts with "attachment" or "inline" dispositions
	 * with a small "text/plain" part saying the file was removed. */
	for (ii = 0; ii < n_parts && !g_cancellable_is_cancelled (cancellable); ii++) {
		CamelMimePart *mime_part;
		const gchar *disposition;
		gboolean is_attachment;

		mime_part = camel_multipart_get_part (multipart, ii);
		disposition = camel_mime_part_get_disposition (mime_part);

		is_attachment =
			(g_strcmp0 (disposition, "attachment") == 0) ||
			(g_strcmp0 (disposition, "inline") == 0);

		if (is_attachment) {
			const gchar *filename;
			const gchar *content_type;
			gchar *content;

			disposition = "inline";
			content_type = "text/plain";
			filename = camel_mime_part_get_filename (mime_part);

			if (filename != NULL && *filename != '\0')
				content = g_strdup_printf (
					_("File \"%s\" has been removed."),
					filename);
			else
				content = g_strdup (
					_("File has been removed."));

			camel_mime_part_set_content (
				mime_part, content,
				strlen (content), content_type);
			camel_mime_part_set_content_type (
				mime_part, content_type);
			camel_mime_part_set_disposition (
				mime_part, disposition);

			modified = TRUE;
		} else {
			modified = mail_folder_strip_message_level (mime_part, cancellable) || modified;
		}
	}

	return modified;
}

/* Helper for e_mail_folder_remove_attachments_sync() */
static gboolean
mail_folder_strip_message (CamelFolder *folder,
                           CamelMimeMessage *message,
                           const gchar *message_uid,
                           GCancellable *cancellable,
                           GError **error)
{
	gboolean modified;
	gboolean success = TRUE;

	modified = mail_folder_strip_message_level (CAMEL_MIME_PART (message), cancellable);

	/* Append the modified message with removed attachments to
	 * the folder and mark the original message for deletion. */
	if (modified) {
		CamelMessageInfo *orig_info;
		CamelMessageInfo *copy_info;
		CamelMessageFlags flags;

		orig_info =
			camel_folder_get_message_info (folder, message_uid);
		copy_info =
			camel_message_info_new_from_header (
			NULL, CAMEL_MIME_PART (message)->headers);

		flags = camel_folder_get_message_flags (folder, message_uid);
		camel_message_info_set_flags (copy_info, flags, flags);

		success = camel_folder_append_message_sync (
			folder, message, copy_info, NULL, cancellable, error);
		if (success)
			camel_message_info_set_flags (
				orig_info,
				CAMEL_MESSAGE_DELETED,
				CAMEL_MESSAGE_DELETED);

		g_clear_object (&orig_info);
		g_clear_object (&copy_info);
	}

	return success;
}

gboolean
e_mail_folder_remove_attachments_sync (CamelFolder *folder,
                                       GPtrArray *message_uids,
                                       GCancellable *cancellable,
                                       GError **error)
{
	gboolean success = TRUE;
	guint ii;

	g_return_val_if_fail (CAMEL_IS_FOLDER (folder), FALSE);
	g_return_val_if_fail (message_uids != NULL, FALSE);

	camel_folder_freeze (folder);

	camel_operation_push_message (cancellable, _("Removing attachments"));

	for (ii = 0; success && ii < message_uids->len; ii++) {
		CamelMimeMessage *message;
		CamelFolder *real_folder = NULL, *use_folder;
		gchar *real_message_uid = NULL;
		const gchar *uid, *use_message_uid;
		gint percent;

		uid = g_ptr_array_index (message_uids, ii);

		em_utils_get_real_folder_and_message_uid (folder, uid, &real_folder, NULL, &real_message_uid);

		use_folder = real_folder ? real_folder : folder;
		use_message_uid = real_message_uid ? real_message_uid : uid;
		message = camel_folder_get_message_sync (use_folder, use_message_uid, cancellable, error);

		if (message == NULL) {
			g_clear_object (&real_folder);
			g_free (real_message_uid);
			success = FALSE;
			break;
		}

		success = mail_folder_strip_message (use_folder, message, use_message_uid, cancellable, error);

		percent = ((ii + 1) * 100) / message_uids->len;
		camel_operation_progress (cancellable, percent);

		g_clear_object (&real_folder);
		g_clear_object (&message);
		g_free (real_message_uid);
	}

	camel_operation_pop_message (cancellable);

	if (success)
		camel_folder_synchronize_sync (
			folder, FALSE, cancellable, error);

	camel_folder_thaw (folder);

	return success;
}

void
e_mail_folder_remove_attachments (CamelFolder *folder,
                                  GPtrArray *message_uids,
                                  gint io_priority,
                                  GCancellable *cancellable,
                                  GAsyncReadyCallback callback,
                                  gpointer user_data)
{
	GSimpleAsyncResult *simple;
	AsyncContext *context;

	g_return_if_fail (CAMEL_IS_FOLDER (folder));
	g_return_if_fail (message_uids != NULL);

	context = g_slice_new0 (AsyncContext);
	context->ptr_array = g_ptr_array_ref (message_uids);

	simple = g_simple_async_result_new (
		G_OBJECT (folder), callback, user_data,
		e_mail_folder_remove_attachments);

	g_simple_async_result_set_check_cancellable (simple, cancellable);

	g_simple_async_result_set_op_res_gpointer (
		simple, context, (GDestroyNotify) async_context_free);

	g_simple_async_result_run_in_thread (
		simple, mail_folder_remove_attachments_thread,
		io_priority, cancellable);

	g_object_unref (simple);
}

gboolean
e_mail_folder_remove_attachments_finish (CamelFolder *folder,
                                         GAsyncResult *result,
                                         GError **error)
{
	GSimpleAsyncResult *simple;

	g_return_val_if_fail (
		g_simple_async_result_is_valid (
		result, G_OBJECT (folder),
		e_mail_folder_remove_attachments), FALSE);

	simple = G_SIMPLE_ASYNC_RESULT (result);

	/* Assume success unless a GError is set. */
	return !g_simple_async_result_propagate_error (simple, error);
}

static void
mail_folder_save_messages_thread (GSimpleAsyncResult *simple,
                                  GObject *object,
                                  GCancellable *cancellable)
{
	AsyncContext *context;
	GError *error = NULL;

	context = g_simple_async_result_get_op_res_gpointer (simple);

	e_mail_folder_save_messages_sync (
		CAMEL_FOLDER (object), context->ptr_array,
		context->destination, cancellable, &error);

	if (error != NULL)
		g_simple_async_result_take_error (simple, error);
}

/* Helper for e_mail_folder_save_messages_sync() */
static void
mail_folder_save_prepare_part (CamelMimePart *mime_part)
{
	CamelDataWrapper *content;

	content = camel_medium_get_content (CAMEL_MEDIUM (mime_part));

	if (content == NULL)
		return;

	if (CAMEL_IS_MULTIPART (content)) {
		guint n_parts, ii;

		n_parts = camel_multipart_get_number (
			CAMEL_MULTIPART (content));
		for (ii = 0; ii < n_parts; ii++) {
			mime_part = camel_multipart_get_part (
				CAMEL_MULTIPART (content), ii);
			mail_folder_save_prepare_part (mime_part);
		}

	} else if (CAMEL_IS_MIME_MESSAGE (content)) {
		mail_folder_save_prepare_part (CAMEL_MIME_PART (content));

	} else {
		CamelContentType *type;

		/* Save textual parts as 8-bit, not encoded. */
		type = camel_data_wrapper_get_mime_type_field (content);
		if (camel_content_type_is (type, "text", "*"))
			camel_mime_part_set_encoding (
				mime_part, CAMEL_TRANSFER_ENCODING_8BIT);
	}
}

gboolean
e_mail_folder_save_messages_sync (CamelFolder *folder,
                                  GPtrArray *message_uids,
                                  GFile *destination,
                                  GCancellable *cancellable,
                                  GError **error)
{
	GFileOutputStream *file_output_stream;
	CamelStream *base_stream = NULL;
	GByteArray *byte_array;
	gboolean success = TRUE;
	guint ii;

	g_return_val_if_fail (CAMEL_IS_FOLDER (folder), FALSE);
	g_return_val_if_fail (message_uids != NULL, FALSE);
	g_return_val_if_fail (G_IS_FILE (destination), FALSE);

	/* Need at least one message UID to save. */
	g_return_val_if_fail (message_uids->len > 0, FALSE);

	camel_operation_push_message (
		cancellable, ngettext (
			"Saving %d message",
			"Saving %d messages",
			message_uids->len),
		message_uids->len);

	file_output_stream = g_file_replace (
		destination, NULL, FALSE,
		G_FILE_CREATE_PRIVATE |
		G_FILE_CREATE_REPLACE_DESTINATION,
		cancellable, error);

	if (file_output_stream == NULL) {
		camel_operation_pop_message (cancellable);
		return FALSE;
	}

	byte_array = g_byte_array_new ();

	for (ii = 0; ii < message_uids->len; ii++) {
		CamelMimeMessage *message;
		CamelMimeFilter *filter;
		CamelStream *stream;
		const gchar *uid;
		gchar *from_line;
		gint percent;
		gint retval;

		if (base_stream != NULL)
			g_object_unref (base_stream);

		/* CamelStreamMem does NOT take ownership of the byte
		 * array when set with camel_stream_mem_set_byte_array().
		 * This allows us to reuse the same memory slab for each
		 * message, which is slightly more efficient. */
		base_stream = camel_stream_mem_new ();
		camel_stream_mem_set_byte_array (
			CAMEL_STREAM_MEM (base_stream), byte_array);

		uid = g_ptr_array_index (message_uids, ii);

		message = camel_folder_get_message_sync (
			folder, uid, cancellable, error);
		if (message == NULL) {
			success = FALSE;
			goto exit;
		}

		mail_folder_save_prepare_part (CAMEL_MIME_PART (message));

		from_line = camel_mime_message_build_mbox_from (message);
		g_return_val_if_fail (from_line != NULL, FALSE);

		success = g_output_stream_write_all (
			G_OUTPUT_STREAM (file_output_stream),
			from_line, strlen (from_line), NULL,
			cancellable, error);

		g_free (from_line);

		if (!success) {
			g_object_unref (message);
			goto exit;
		}

		filter = camel_mime_filter_from_new ();
		stream = camel_stream_filter_new (base_stream);
		camel_stream_filter_add (CAMEL_STREAM_FILTER (stream), filter);

		retval = camel_data_wrapper_write_to_stream_sync (
			CAMEL_DATA_WRAPPER (message),
			stream, cancellable, error);

		g_object_unref (filter);
		g_object_unref (stream);

		if (retval == -1) {
			g_object_unref (message);
			goto exit;
		}

		g_byte_array_append (byte_array, (guint8 *) "\n", 1);

		success = g_output_stream_write_all (
			G_OUTPUT_STREAM (file_output_stream),
			byte_array->data, byte_array->len,
			NULL, cancellable, error);

		if (!success) {
			g_object_unref (message);
			goto exit;
		}

		percent = ((ii + 1) * 100) / message_uids->len;
		camel_operation_progress (cancellable, percent);

		/* Reset the byte array for the next message. */
		g_byte_array_set_size (byte_array, 0);

		g_object_unref (message);
	}

exit:
	if (base_stream != NULL)
		g_object_unref (base_stream);

	g_byte_array_free (byte_array, TRUE);

	g_object_unref (file_output_stream);

	camel_operation_pop_message (cancellable);

	if (!success) {
		/* Try deleting the destination file. */
		g_file_delete (destination, NULL, NULL);
	}

	return success;
}

void
e_mail_folder_save_messages (CamelFolder *folder,
                             GPtrArray *message_uids,
                             GFile *destination,
                             gint io_priority,
                             GCancellable *cancellable,
                             GAsyncReadyCallback callback,
                             gpointer user_data)
{
	GSimpleAsyncResult *simple;
	AsyncContext *context;

	g_return_if_fail (CAMEL_IS_FOLDER (folder));
	g_return_if_fail (message_uids != NULL);
	g_return_if_fail (G_IS_FILE (destination));

	/* Need at least one message UID to save. */
	g_return_if_fail (message_uids->len > 0);

	context = g_slice_new0 (AsyncContext);
	context->ptr_array = g_ptr_array_ref (message_uids);
	context->destination = g_object_ref (destination);

	simple = g_simple_async_result_new (
		G_OBJECT (folder), callback, user_data,
		e_mail_folder_save_messages);

	g_simple_async_result_set_check_cancellable (simple, cancellable);

	g_simple_async_result_set_op_res_gpointer (
		simple, context, (GDestroyNotify) async_context_free);

	g_simple_async_result_run_in_thread (
		simple, mail_folder_save_messages_thread,
		io_priority, cancellable);

	g_object_unref (simple);
}

gboolean
e_mail_folder_save_messages_finish (CamelFolder *folder,
                                    GAsyncResult *result,
                                    GError **error)
{
	GSimpleAsyncResult *simple;

	g_return_val_if_fail (
		g_simple_async_result_is_valid (
		result, G_OBJECT (folder),
		e_mail_folder_save_messages), FALSE);

	simple = G_SIMPLE_ASYNC_RESULT (result);

	/* Assume success unless a GError is set. */
	return !g_simple_async_result_propagate_error (simple, error);
}

/**
 * e_mail_folder_uri_build:
 * @store: a #CamelStore
 * @folder_name: a folder name
 *
 * Builds a folder URI string from @store and @folder_name.
 *
 * Returns: a newly-allocated folder URI string
 **/
gchar *
e_mail_folder_uri_build (CamelStore *store,
                         const gchar *folder_name)
{
	const gchar *uid;
	gchar *encoded_name;
	gchar *encoded_uid;
	gchar *uri;

	g_return_val_if_fail (CAMEL_IS_STORE (store), NULL);
	g_return_val_if_fail (folder_name != NULL, NULL);

	/* Skip the leading slash, if present. */
	if (*folder_name == '/')
		folder_name++;

	uid = camel_service_get_uid (CAMEL_SERVICE (store));

	encoded_uid = camel_url_encode (uid, ":;@/");
	encoded_name = camel_url_encode (folder_name, ":;@?#");

	uri = g_strdup_printf ("folder://%s/%s", encoded_uid, encoded_name);

	g_free (encoded_uid);
	g_free (encoded_name);

	return uri;
}

/**
 * e_mail_folder_uri_parse:
 * @session: a #CamelSession
 * @folder_uri: a folder URI
 * @out_store: return location for a #CamelStore, or %NULL
 * @out_folder_name: return location for a folder name, or %NULL
 * @error: return location for a #GError, or %NULL
 *
 * Parses a folder URI generated by e_mail_folder_uri_build() and
 * returns the corresponding #CamelStore instance in @out_store and
 * folder name string in @out_folder_name.  If the URI is malformed
 * or no corresponding store exists, the function sets @error and
 * returns %FALSE.
 *
 * If the function is able to parse the URI, the #CamelStore instance
 * set in @out_store should be unreferenced with g_object_unref() when
 * done with it, and the folder name string set in @out_folder_name
 * should be freed with g_free().
 *
 * The function also handles older style URIs, such as ones where the
 * #CamelStore's #CamelStore::uri string was embedded directly in the
 * folder URI, and account-based URIs that used an "email://" prefix.
 *
 * Returns: %TRUE if @folder_uri could be parsed, %FALSE otherwise
 **/
gboolean
e_mail_folder_uri_parse (CamelSession *session,
                         const gchar *folder_uri,
                         CamelStore **out_store,
                         gchar **out_folder_name,
                         GError **error)
{
	CamelURL *url;
	CamelService *service = NULL;
	gchar *folder_name = NULL;
	gboolean success = FALSE;

	g_return_val_if_fail (CAMEL_IS_SESSION (session), FALSE);
	g_return_val_if_fail (folder_uri != NULL, FALSE);

	url = camel_url_new (folder_uri, error);
	if (url == NULL)
		return FALSE;

	/* Current URI Format: 'folder://' STORE_UID '/' FOLDER_PATH */
	if (g_strcmp0 (url->protocol, "folder") == 0) {

		if (url->host != NULL) {
			gchar *uid;

			if (url->user == NULL || *url->user == '\0')
				uid = g_strdup (url->host);
			else
				uid = g_strconcat (
					url->user, "@", url->host, NULL);

			service = camel_session_ref_service (session, uid);
			g_free (uid);
		}

		if (url->path != NULL && *url->path == '/')
			folder_name = camel_url_decode_path (url->path + 1);

	/* This style was used to reference accounts by UID before
	 * CamelServices themselves had UIDs.  Some examples are:
	 *
	 * Special cases:
	 *
	 *   'email://local@local/' FOLDER_PATH
	 *   'email://vfolder@local/' FOLDER_PATH
	 *
	 * General case:
	 *
	 *   'email://' ACCOUNT_UID '/' FOLDER_PATH
	 *
	 * Note: ACCOUNT_UID is now equivalent to STORE_UID, and
	 *       the STORE_UIDs for the special cases are 'local'
	 *       and 'vfolder'.
	 */
	} else if (g_strcmp0 (url->protocol, "email") == 0) {
		gchar *uid = NULL;

		/* Handle the special cases. */
		if (g_strcmp0 (url->host, "local") == 0) {
			if (g_strcmp0 (url->user, "local") == 0)
				uid = g_strdup ("local");
			if (g_strcmp0 (url->user, "vfolder") == 0)
				uid = g_strdup ("vfolder");
		}

		/* Handle the general case. */
		if (uid == NULL && url->host != NULL) {
			if (url->user == NULL)
				uid = g_strdup (url->host);
			else
				uid = g_strdup_printf (
					"%s@%s", url->user, url->host);
		}

		if (uid != NULL) {
			service = camel_session_ref_service (session, uid);
			g_free (uid);
		}

		if (url->path != NULL && *url->path == '/')
			folder_name = camel_url_decode_path (url->path + 1);

	/* CamelFolderInfo URIs used to embed the store's URI, so the
	 * folder name is appended as either a path part or a fragment
	 * part, depending whether the store's URI used the path part.
	 * To determine which it is, you have to check the provider
	 * flags for CAMEL_URL_FRAGMENT_IS_PATH. */
	} else {
		gboolean local_mbox_folder;

		/* In Evolution 2.x, the local mail store used mbox
		 * format.  camel_session_ref_service_by_url() won't
		 * match "mbox:///.../mail/local" folder URIs, since
		 * the local mail store is now Maildir format.  Test
		 * for this corner case and work around it.
		 *
		 * The folder path is kept in the fragment part of the
		 * URL which makes it easy to test the filesystem path.
		 * The suffix "evolution/mail/local" should match both
		 * the current XDG-compliant location and the old "dot
		 * folder" location (~/.evolution/mail/local). */
		local_mbox_folder =
			(g_strcmp0 (url->protocol, "mbox") == 0) &&
			(url->path != NULL) &&
			g_str_has_suffix (url->path, "evolution/mail/local");

		if (local_mbox_folder) {
			service = camel_session_ref_service (session, "local");
		} else {
			service = camel_session_ref_service_by_url (
				session, url, CAMEL_PROVIDER_STORE);
		}

		if (CAMEL_IS_STORE (service)) {
			CamelProvider *provider;

			provider = camel_service_get_provider (service);

			if (provider->url_flags & CAMEL_URL_FRAGMENT_IS_PATH)
				folder_name = g_strdup (url->fragment);
			else if (url->path != NULL && *url->path == '/')
				folder_name = g_strdup (url->path + 1);
		}
	}

	if (CAMEL_IS_STORE (service) && folder_name != NULL) {
		if (out_store != NULL)
			*out_store = g_object_ref (service);

		if (out_folder_name != NULL) {
			*out_folder_name = folder_name;
			folder_name = NULL;
		}

		success = TRUE;
	} else {
		g_set_error (
			error, CAMEL_FOLDER_ERROR,
			CAMEL_FOLDER_ERROR_INVALID,
			_("Invalid folder URI '%s'"),
			folder_uri);
	}

	if (service != NULL)
		g_object_unref (service);

	g_free (folder_name);

	camel_url_free (url);

	return success;
}

/**
 * e_mail_folder_uri_equal:
 * @session: a #CamelSession
 * @folder_uri_a: a folder URI
 * @folder_uri_b: another folder URI
 *
 * Compares two folder URIs for equality.  If either URI is invalid,
 * the function returns %FALSE.
 *
 * Returns: %TRUE if the URIs are equal, %FALSE if not
 **/
gboolean
e_mail_folder_uri_equal (CamelSession *session,
                         const gchar *folder_uri_a,
                         const gchar *folder_uri_b)
{
	CamelStore *store_a;
	CamelStore *store_b;
	CamelStoreClass *class;
	gchar *folder_name_a;
	gchar *folder_name_b;
	gboolean success_a;
	gboolean success_b;
	gboolean equal = FALSE;

	g_return_val_if_fail (CAMEL_IS_SESSION (session), FALSE);
	g_return_val_if_fail (folder_uri_a != NULL, FALSE);
	g_return_val_if_fail (folder_uri_b != NULL, FALSE);

	success_a = e_mail_folder_uri_parse (
		session, folder_uri_a, &store_a, &folder_name_a, NULL);

	success_b = e_mail_folder_uri_parse (
		session, folder_uri_b, &store_b, &folder_name_b, NULL);

	if (!success_a || !success_b)
		goto exit;

	if (store_a != store_b)
		goto exit;

	/* Doesn't matter which store we use since they're the same. */
	class = CAMEL_STORE_GET_CLASS (store_a);
	g_return_val_if_fail (class->equal_folder_name != NULL, FALSE);

	equal = class->equal_folder_name (folder_name_a, folder_name_b);

exit:
	if (success_a) {
		g_object_unref (store_a);
		g_free (folder_name_a);
	}

	if (success_b) {
		g_object_unref (store_b);
		g_free (folder_name_b);
	}

	return equal;
}

/**
 * e_mail_folder_uri_from_folder:
 * @folder: a #CamelFolder
 *
 * Convenience function for building a folder URI from a #CamelFolder.
 * Free the returned URI string with g_free().
 *
 * Returns: a newly-allocated folder URI string
 **/
gchar *
e_mail_folder_uri_from_folder (CamelFolder *folder)
{
	CamelStore *store;
	const gchar *folder_name;

	g_return_val_if_fail (CAMEL_IS_FOLDER (folder), NULL);

	store = camel_folder_get_parent_store (folder);
	folder_name = camel_folder_get_full_name (folder);

	return e_mail_folder_uri_build (store, folder_name);
}

/**
 * e_mail_folder_uri_to_markup:
 * @session: a #CamelSession
 * @folder_uri: a folder URI
 * @error: return location for a #GError, or %NULL
 *
 * Converts @folder_uri to a markup string suitable for displaying to users.
 * The string consists of the #CamelStore display name (in bold), followed
 * by the folder path.  If the URI is malformed or no corresponding store
 * exists, the function sets @error and returns %NULL.  Free the returned
 * string with g_free().
 *
 * Returns: a newly-allocated markup string, or %NULL
 **/
gchar *
e_mail_folder_uri_to_markup (CamelSession *session,
                             const gchar *folder_uri,
                             GError **error)
{
	CamelStore *store = NULL;
	const gchar *display_name;
	gchar *folder_name = NULL;
	gchar *markup;
	gboolean success;

	g_return_val_if_fail (CAMEL_IS_SESSION (session), NULL);
	g_return_val_if_fail (folder_uri != NULL, NULL);

	success = e_mail_folder_uri_parse (
		session, folder_uri, &store, &folder_name, error);

	if (!success)
		return NULL;

	g_return_val_if_fail (CAMEL_IS_STORE (store), NULL);
	g_return_val_if_fail (folder_name != NULL, NULL);

	display_name = camel_service_get_display_name (CAMEL_SERVICE (store));

	markup = g_markup_printf_escaped (
		"<b>%s</b> : %s", display_name, folder_name);

	g_object_unref (store);
	g_free (folder_name);

	return markup;
}

/**
 * e_mail_folder_to_full_display_name:
 * @folder: a #CamelFolder
 * @error: return location for a #GError, or %NULL
 *
 * Returns similar description as e_mail_folder_uri_to_markup(), only without markup
 * and rather for a @folder, than for a folder URI. Returned pointer should be freed
 * with g_free() when no longer needed.
 *
 * Returns: a newly-allocated string, or %NULL
 *
 * Since: 3.18
 **/
gchar *
e_mail_folder_to_full_display_name (CamelFolder *folder,
				    GError **error)
{
	CamelSession *session;
	CamelStore *store;
	gchar *folder_uri, *full_display_name = NULL, *folder_name = NULL;

	g_return_val_if_fail (CAMEL_IS_FOLDER (folder), NULL);

	folder_uri = e_mail_folder_uri_from_folder (folder);
	if (!folder_uri)
		return NULL;

	store = camel_folder_get_parent_store (folder);
	if (!store) {
		g_warn_if_reached ();
		g_free (folder_uri);

		return NULL;
	}

	session = camel_service_ref_session (CAMEL_SERVICE (store));
	if (!session) {
		g_warn_if_reached ();
		g_free (folder_uri);

		return NULL;
	}

	if (e_mail_folder_uri_parse (session, folder_uri, NULL, &folder_name, error)) {
		const gchar *service_display_name;

		service_display_name = camel_service_get_display_name (CAMEL_SERVICE (store));

		full_display_name = g_strdup_printf ("%s : %s", service_display_name, folder_name);

		g_free (folder_name);
	}

	g_clear_object (&session);
	g_free (folder_uri);

	return full_display_name;
}
