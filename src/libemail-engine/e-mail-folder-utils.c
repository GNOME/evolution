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

/* User-Agent header value */
#define USER_AGENT ("Evolution " VERSION VERSION_SUBSTRING " " VERSION_COMMENT)

typedef struct _AsyncContext AsyncContext;
typedef struct _BuildAttachmentResult BuildAttachmentResult;

struct _AsyncContext {
	CamelMimeMessage *message;
	CamelMessageInfo *info;
	GPtrArray *ptr_array;
	GFile *destination;
};

struct _BuildAttachmentResult {
	CamelMimePart *part;
	gchar *orig_subject;
};

static void
async_context_free (AsyncContext *context)
{
	g_clear_pointer (&context->ptr_array, g_ptr_array_unref);
	g_clear_object (&context->message);
	g_clear_object (&context->info);
	g_clear_object (&context->destination);

	g_slice_free (AsyncContext, context);
}

static void
build_attachment_result_free (gpointer data)
{
	BuildAttachmentResult *res = data;

	g_clear_object (&res->part);
	g_clear_pointer (&res->orig_subject, g_free);

	g_free (res);
}

static void
mail_folder_append_message_thread (GTask *task,
                                   gpointer source_object,
                                   gpointer task_data,
                                   GCancellable *cancellable)
{
	AsyncContext *context;
	GError *error = NULL;
	gchar *message_uid = NULL;

	context = task_data;

	e_mail_folder_append_message_sync (
		CAMEL_FOLDER (source_object), context->message,
		context->info, &message_uid,
		cancellable, &error);

	if (error != NULL)
		g_task_return_error (task, g_steal_pointer (&error));
	else
		g_task_return_pointer (task, g_steal_pointer (&message_uid), g_free);
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
		_("Saving message to folder “%s”"),
		full_display_name ? full_display_name : camel_folder_get_display_name (folder));
	g_free (full_display_name);

	if (!camel_medium_get_header (medium, "X-Evolution-Is-Redirect")) {
		if (camel_medium_get_header (medium, "User-Agent") == NULL)
			camel_medium_set_header (medium, "User-Agent", USER_AGENT);

		camel_mime_message_set_date (message, CAMEL_MESSAGE_DATE_CURRENT, 0);
	}

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
	GTask *task;
	AsyncContext *context;

	g_return_if_fail (CAMEL_IS_FOLDER (folder));
	g_return_if_fail (CAMEL_IS_MIME_MESSAGE (message));

	context = g_slice_new0 (AsyncContext);
	context->message = g_object_ref (message);

	if (info != NULL)
		context->info = g_object_ref (info);

	task = g_task_new (folder, cancellable, callback, user_data);
	g_task_set_source_tag (task, e_mail_folder_append_message);
	g_task_set_priority (task, io_priority);
	g_task_set_task_data (task, g_steal_pointer (&context), (GDestroyNotify) async_context_free);

	g_task_run_in_thread (task, mail_folder_append_message_thread);

	g_object_unref (task);
}

gboolean
e_mail_folder_append_message_finish (CamelFolder *folder,
                                     GAsyncResult *result,
                                     gchar **appended_uid,
                                     GError **error)
{
	gchar *message_uid;

	g_return_val_if_fail (g_task_is_valid (result, folder), FALSE);
	g_return_val_if_fail (g_async_result_is_tagged (result, e_mail_folder_append_message), FALSE);

	message_uid = g_task_propagate_pointer (G_TASK (result), error);
	if (!message_uid)
		return FALSE;

	if (appended_uid != NULL)
		*appended_uid = g_steal_pointer (&message_uid);

	g_clear_pointer (&message_uid, g_free);
	return TRUE;
}

static void
mail_folder_expunge_thread (GTask *task,
                            gpointer source_object,
                            gpointer task_data,
                            GCancellable *cancellable)
{
	GError *error = NULL;
	gboolean res;

	res = e_mail_folder_expunge_sync (
		CAMEL_FOLDER (source_object), cancellable, &error);

	if (error != NULL)
		g_task_return_error (task, g_steal_pointer (&error));
	else
		g_task_return_boolean (task, res);
}

static gboolean
mail_folder_expunge_pop3_stores (CamelFolder *folder,
                                 GCancellable *cancellable,
                                 GError **error)
{
	GHashTable *expunging_uids;
	CamelStore *parent_store;
	CamelSession *session;
	ESourceRegistry *registry;
	GPtrArray *uids;
	GList *list, *link;
	const gchar *extension_name;
	gboolean success = TRUE;
	guint ii;

	parent_store = camel_folder_get_parent_store (folder);

	session = camel_service_ref_session (CAMEL_SERVICE (parent_store));
	registry = e_mail_session_get_registry (E_MAIL_SESSION (session));

	uids = camel_folder_dup_uids (folder);

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

	g_ptr_array_unref (uids);
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
		CamelFolder *inbox_folder;
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

		inbox_folder = camel_store_get_inbox_folder_sync (
			CAMEL_STORE (service), cancellable, error);

		/* Abort the loop on error. */
		if (inbox_folder == NULL) {
			g_object_unref (service);
			success = FALSE;
			break;
		}

		uids = camel_folder_dup_uids (inbox_folder);

		if (uids == NULL) {
			g_object_unref (service);
			g_object_unref (inbox_folder);
			continue;
		}

		for (ii = 0; ii < uids->len; ii++) {
			source_uid = g_hash_table_lookup (
				expunging_uids, uids->pdata[ii]);
			if (g_strcmp0 (source_uid, service_uid) == 0) {
				any_found = TRUE;
				camel_folder_delete_message (
					inbox_folder, uids->pdata[ii]);
			}
		}

		g_ptr_array_unref (uids);

		if (any_found)
			success = camel_folder_synchronize_sync (
				inbox_folder, TRUE, cancellable, error);

		g_object_unref (inbox_folder);
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
	GTask *task;

	g_return_if_fail (CAMEL_IS_FOLDER (folder));

	task = g_task_new (folder, cancellable, callback, user_data);
	g_task_set_source_tag (task, e_mail_folder_expunge);
	g_task_set_priority (task, io_priority);

	g_task_run_in_thread (task, mail_folder_expunge_thread);

	g_object_unref (task);
}

gboolean
e_mail_folder_expunge_finish (CamelFolder *folder,
                              GAsyncResult *result,
                              GError **error)
{
	g_return_val_if_fail (g_task_is_valid (result, folder), FALSE);
	g_return_val_if_fail (g_async_result_is_tagged (result, e_mail_folder_expunge), FALSE);

	return g_task_propagate_boolean (G_TASK (result), error);
}

static void
mail_folder_build_attachment_thread (GTask *task,
                                     gpointer source_object,
                                     gpointer task_data,
                                     GCancellable *cancellable)
{
	BuildAttachmentResult *res;
	GPtrArray *ptr_array;
	GError *error = NULL;

	ptr_array = task_data;
	res = g_new0 (BuildAttachmentResult, 1);

	res->part = e_mail_folder_build_attachment_sync (
		CAMEL_FOLDER (source_object), ptr_array,
		&res->orig_subject, cancellable, &error);

	if (error != NULL)
		g_task_return_error (task, g_steal_pointer (&error));
	else
		g_task_return_pointer (task, g_steal_pointer (&res), build_attachment_result_free);

	g_clear_pointer (&res, build_attachment_result_free);
}

CamelMimePart *
e_mail_folder_build_attachment_sync (CamelFolder *folder,
                                     GPtrArray *message_uids,
                                     gchar **orig_subject,
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

	if (orig_subject != NULL)
		*orig_subject = g_strdup (camel_mime_message_get_subject (message));

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
	GTask *task;

	g_return_if_fail (CAMEL_IS_FOLDER (folder));
	g_return_if_fail (message_uids != NULL);

	/* Need at least one message UID to make an attachment. */
	g_return_if_fail (message_uids->len > 0);

	task = g_task_new (folder, cancellable, callback, user_data);
	g_task_set_source_tag (task, e_mail_folder_build_attachment);
	g_task_set_priority (task, io_priority);
	g_task_set_task_data (task, g_ptr_array_ref (message_uids), (GDestroyNotify) g_ptr_array_unref);

	g_task_run_in_thread (task, mail_folder_build_attachment_thread);

	g_object_unref (task);
}

CamelMimePart *
e_mail_folder_build_attachment_finish (CamelFolder *folder,
                                       GAsyncResult *result,
                                       gchar **orig_subject,
                                       GError **error)
{
	BuildAttachmentResult *res;
	CamelMimePart *part;

	g_return_val_if_fail (g_task_is_valid (result, folder), NULL);
	g_return_val_if_fail (g_async_result_is_tagged (result, e_mail_folder_build_attachment), NULL);

	res = g_task_propagate_pointer (G_TASK (result), error);
	if (!res)
		return NULL;

	if (orig_subject != NULL)
		*orig_subject = g_steal_pointer (&res->orig_subject);

	part = g_steal_pointer (&res->part);
	g_clear_pointer (&res, build_attachment_result_free);
	return part;
}

static void
mail_folder_find_duplicate_messages_thread (GTask *task,
                                            gpointer source_object,
                                            gpointer task_data,
                                            GCancellable *cancellable)
{
	GHashTable *hash_table;
	GPtrArray *ptr_array;
	GError *error = NULL;

	ptr_array = task_data;

	hash_table = e_mail_folder_find_duplicate_messages_sync (
		CAMEL_FOLDER (source_object), ptr_array,
		cancellable, &error);

	if (error != NULL)
		g_task_return_error (task, g_steal_pointer (&error));
	else
		g_task_return_pointer (task, g_steal_pointer (&hash_table), (GDestroyNotify) g_hash_table_unref);
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
	GTask *task;

	g_return_if_fail (CAMEL_IS_FOLDER (folder));
	g_return_if_fail (message_uids != NULL);

	task = g_task_new (folder, cancellable, callback, user_data);
	g_task_set_source_tag (task, e_mail_folder_find_duplicate_messages);
	g_task_set_priority (task, io_priority);
	g_task_set_task_data (task, g_ptr_array_ref (message_uids), (GDestroyNotify) g_ptr_array_unref);

	g_task_run_in_thread (task, mail_folder_find_duplicate_messages_thread);

	g_object_unref (task);
}

GHashTable *
e_mail_folder_find_duplicate_messages_finish (CamelFolder *folder,
                                              GAsyncResult *result,
                                              GError **error)
{
	g_return_val_if_fail (g_task_is_valid (result, folder), NULL);
	g_return_val_if_fail (g_async_result_is_tagged (result, e_mail_folder_find_duplicate_messages), NULL);

	return g_task_propagate_pointer (G_TASK (result), error);
}

static void
mail_folder_get_multiple_messages_thread (GTask *task,
                                          gpointer source_object,
                                          gpointer task_data,
                                          GCancellable *cancellable)
{
	GHashTable *hash_table;
	GPtrArray *ptr_array;
	GError *error = NULL;

	ptr_array = task_data;

	hash_table = e_mail_folder_get_multiple_messages_sync (
		CAMEL_FOLDER (source_object), ptr_array,
		cancellable, &error);

	if (error != NULL)
		g_task_return_error (task, g_steal_pointer (&error));
	else
		g_task_return_pointer (task, g_steal_pointer (&hash_table), (GDestroyNotify) g_hash_table_unref);
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
	GTask *task;

	g_return_if_fail (CAMEL_IS_FOLDER (folder));
	g_return_if_fail (message_uids != NULL);

	task = g_task_new (folder, cancellable, callback, user_data);
	g_task_set_source_tag (task, e_mail_folder_get_multiple_messages);
	g_task_set_priority (task, io_priority);
	g_task_set_task_data (task, g_ptr_array_ref (message_uids), (GDestroyNotify) g_ptr_array_unref);

	g_task_run_in_thread (task, mail_folder_get_multiple_messages_thread);

	g_object_unref (task);
}

GHashTable *
e_mail_folder_get_multiple_messages_finish (CamelFolder *folder,
                                            GAsyncResult *result,
                                            GError **error)
{
	g_return_val_if_fail (g_task_is_valid (result, folder), NULL);
	g_return_val_if_fail (g_async_result_is_tagged (result, e_mail_folder_get_multiple_messages), NULL);

	return g_task_propagate_pointer (G_TASK (result), error);
}

static void
mail_folder_remove_thread (GTask *task,
                           gpointer source_object,
                           gpointer task_data,
                           GCancellable *cancellable)
{
	GError *error = NULL;
	gboolean res;

	res = e_mail_folder_remove_sync (
		CAMEL_FOLDER (source_object), cancellable, &error);

	if (error != NULL)
		g_task_return_error (task, g_steal_pointer (&error));
	else
		g_task_return_boolean (task, res);
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

			uids = camel_folder_dup_uids (folder);

			for (ii = 0; ii < uids->len; ii++)
				camel_folder_delete_message (
					folder, uids->pdata[ii]);

			g_ptr_array_unref (uids);

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
	camel_operation_push_message (cancellable, _("Removing folder “%s”"),
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
	GTask *task;

	g_return_if_fail (CAMEL_IS_FOLDER (folder));

	task = g_task_new (folder, cancellable, callback, user_data);
	g_task_set_source_tag (task, e_mail_folder_remove);
	g_task_set_priority (task, io_priority);

	g_task_run_in_thread (task, mail_folder_remove_thread);

	g_object_unref (task);
}

gboolean
e_mail_folder_remove_finish (CamelFolder *folder,
                             GAsyncResult *result,
                             GError **error)
{
	g_return_val_if_fail (g_task_is_valid (result, folder), FALSE);
	g_return_val_if_fail (g_async_result_is_tagged (result, e_mail_folder_remove), FALSE);

	return g_task_propagate_boolean (G_TASK (result), error);
}

static void
mail_folder_remove_attachments_thread (GTask *task,
                                       gpointer source_object,
                                       gpointer task_data,
                                       GCancellable *cancellable)
{
	GPtrArray *ptr_array;
	GError *error = NULL;
	gboolean res;

	ptr_array = task_data;

	res = e_mail_folder_remove_attachments_sync (
		CAMEL_FOLDER (source_object), ptr_array,
		cancellable, &error);

	if (error != NULL)
		g_task_return_error (task, g_steal_pointer (&error));
	else
		g_task_return_boolean (task, res);
}

static gboolean
mail_folder_strip_message_level (CamelMimePart *in_part,
				 GCancellable *cancellable)
{
	CamelDataWrapper *content;
	CamelMultipart *multipart;
	CamelMultipart *new_multipart = NULL;
	gboolean modified = FALSE;
	gboolean is_signed;
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

	is_signed = CAMEL_IS_MULTIPART_SIGNED (multipart);

	if (is_signed) {
		CamelDataWrapper *in_datawrapper, *new_datawrapper;

		in_datawrapper = CAMEL_DATA_WRAPPER (multipart);

		new_multipart = camel_multipart_new ();
		new_datawrapper = CAMEL_DATA_WRAPPER (new_multipart);

		camel_multipart_set_boundary (new_multipart, camel_multipart_get_boundary (multipart));
		camel_multipart_set_preface (new_multipart, camel_multipart_get_preface (multipart));
		camel_multipart_set_postface (new_multipart, camel_multipart_get_postface (multipart));

		camel_data_wrapper_set_encoding	(new_datawrapper, camel_data_wrapper_get_encoding (in_datawrapper));
		camel_data_wrapper_set_mime_type_field (new_datawrapper, camel_data_wrapper_get_mime_type_field (in_datawrapper));
	}

	/* Replace MIME parts with "attachment" or "inline" dispositions
	 * with a small "text/plain" part saying the file was removed. */
	for (ii = 0; ii < n_parts && !g_cancellable_is_cancelled (cancellable); ii++) {
		CamelMimePart *mime_part;
		const gchar *disposition;
		gboolean is_attachment;

		mime_part = camel_multipart_get_part (multipart, ii);
		disposition = camel_mime_part_get_disposition (mime_part);

		is_attachment = (!is_signed || ii != 1) && (
			(g_strcmp0 (disposition, "attachment") == 0) ||
			(g_strcmp0 (disposition, "inline") == 0));

		if (is_attachment) {
			const gchar *filename;
			const gchar *content_type;
			gchar *str_content;

			disposition = "inline";
			content_type = "text/plain";
			filename = camel_mime_part_get_filename (mime_part);

			if (filename != NULL && *filename != '\0')
				str_content = g_strdup_printf (
					_("File “%s” has been removed."),
					filename);
			else
				str_content = g_strdup (
					_("File has been removed."));

			camel_mime_part_set_content (
				mime_part, str_content,
				strlen (str_content), content_type);
			camel_mime_part_set_content_type (
				mime_part, content_type);
			camel_mime_part_set_disposition (
				mime_part, disposition);

			g_free (str_content);

			modified = TRUE;
		} else {
			modified = mail_folder_strip_message_level (mime_part, cancellable) || modified;
		}

		if (new_multipart)
			camel_multipart_add_part (new_multipart, mime_part);
	}

	if (new_multipart && modified)
		camel_medium_set_content (CAMEL_MEDIUM (in_part), CAMEL_DATA_WRAPPER (new_multipart));

	g_clear_object (&new_multipart);

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
		const CamelNameValueArray *headers;

		headers = camel_medium_get_headers (CAMEL_MEDIUM (message));
		orig_info = camel_folder_get_message_info (folder, message_uid);
		copy_info = camel_message_info_new_from_headers (NULL, headers);

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
	GTask *task;

	g_return_if_fail (CAMEL_IS_FOLDER (folder));
	g_return_if_fail (message_uids != NULL);

	task = g_task_new (folder, cancellable, callback, user_data);
	g_task_set_source_tag (task, e_mail_folder_remove_attachments);
	g_task_set_priority (task, io_priority);
	g_task_set_task_data (task, g_ptr_array_ref (message_uids), (GDestroyNotify) g_ptr_array_unref);

	g_task_run_in_thread (task, mail_folder_remove_attachments_thread);

	g_object_unref (task);
}

gboolean
e_mail_folder_remove_attachments_finish (CamelFolder *folder,
                                         GAsyncResult *result,
                                         GError **error)
{
	g_return_val_if_fail (g_task_is_valid (result, folder), FALSE);
	g_return_val_if_fail (g_async_result_is_tagged (result, e_mail_folder_remove_attachments), FALSE);

	return g_task_propagate_boolean (G_TASK (result), error);
}

static void
mail_folder_save_messages_thread (GTask *task,
                                  gpointer source_object,
                                  gpointer task_data,
                                  GCancellable *cancellable)
{
	AsyncContext *context;
	GError *error = NULL;
	gboolean res;

	context = task_data;

	res = e_mail_folder_save_messages_sync (
		CAMEL_FOLDER (source_object), context->ptr_array,
		context->destination, cancellable, &error);

	if (error != NULL)
		g_task_return_error (task, g_steal_pointer (&error));
	else
		g_task_return_boolean (task, res);
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

static gssize
mail_folder_utils_splice_to_stream (CamelStream *stream,
				    GInputStream *input_stream,
				    GCancellable *cancellable,
				    GError **error)
{
	gssize n_read;
	gsize bytes_copied, n_written;
	gchar buffer[8192];
	goffset file_offset;
	gboolean res;

	g_return_val_if_fail (CAMEL_IS_STREAM (stream), -1);
	g_return_val_if_fail (G_IS_INPUT_STREAM (input_stream), -1);

	if (g_cancellable_set_error_if_cancelled (cancellable, error))
		return -1;

	file_offset = 0;
	bytes_copied = 0;
	res = TRUE;
	do {
		n_read = g_input_stream_read (input_stream, buffer, sizeof (buffer), cancellable, error);
		if (n_read == -1) {
			res = FALSE;
			break;
		}

		if (n_read == 0)
			break;

		if ((n_written = camel_stream_write (stream, buffer, n_read, cancellable, error)) == -1) {
			res = FALSE;
			break;
		}

		file_offset += n_read;
		bytes_copied += n_written;
		if (bytes_copied > G_MAXSSIZE)
			bytes_copied = G_MAXSSIZE;
	} while (res);

	if (res)
		return bytes_copied;

	return -1;
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
	gboolean is_mbox = TRUE;
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

	if (message_uids->len == 1 && g_file_peek_path (destination)) {
		const gchar *path = g_file_peek_path (destination);
		gsize len = strlen (path);

		if (len > 4 && g_ascii_strncasecmp (path + len - 4, ".eml", 4) == 0)
			is_mbox = FALSE;
	}

	byte_array = g_byte_array_new ();

	for (ii = 0; ii < message_uids->len; ii++) {
		CamelMimeMessage *message;
		const gchar *uid;
		gchar *filename;
		GFileInputStream *file_input_stream = NULL;
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

		message = camel_folder_get_message_sync (folder, uid, cancellable, error);
		if (message == NULL) {
			success = FALSE;
			goto exit;
		}

		filename = camel_folder_get_filename (folder, uid, NULL);
		if (filename) {
			GFile *file;

			file = g_file_new_for_path (filename);
			if (file) {
				file_input_stream = g_file_read (file, cancellable, NULL);

				g_clear_object (&file);
			}

			g_clear_pointer (&filename, g_free);
		}

		if (!file_input_stream)
			mail_folder_save_prepare_part (CAMEL_MIME_PART (message));

		if (is_mbox) {
			gchar *from_line;

			from_line = camel_mime_message_build_mbox_from (message);
			g_return_val_if_fail (from_line != NULL, FALSE);

			success = g_output_stream_write_all (
				G_OUTPUT_STREAM (file_output_stream),
				from_line, strlen (from_line), NULL,
				cancellable, error);

			g_free (from_line);
		}

		if (!success) {
			g_object_unref (message);
			g_clear_object (&file_input_stream);
			goto exit;
		}

		if (is_mbox) {
			CamelMimeFilter *filter;
			CamelStream *stream;

			filter = camel_mime_filter_from_new ();
			stream = camel_stream_filter_new (base_stream);
			camel_stream_filter_add (CAMEL_STREAM_FILTER (stream), filter);

			if (file_input_stream) {
				retval = mail_folder_utils_splice_to_stream (stream, G_INPUT_STREAM (file_input_stream), cancellable, error);
			} else {
				retval = camel_data_wrapper_write_to_stream_sync (
					CAMEL_DATA_WRAPPER (message),
					stream, cancellable, error);
			}

			g_object_unref (filter);
			g_object_unref (stream);
		} else {
			if (file_input_stream) {
				retval = g_output_stream_splice (G_OUTPUT_STREAM (file_output_stream), G_INPUT_STREAM (file_input_stream),
					G_OUTPUT_STREAM_SPLICE_NONE, cancellable, error);
			} else {
				retval = camel_data_wrapper_write_to_output_stream_sync (CAMEL_DATA_WRAPPER (message),
					G_OUTPUT_STREAM (file_output_stream), cancellable, error);
			}
		}

		g_clear_object (&file_input_stream);
		g_clear_object (&message);

		if (retval == -1)
			goto exit;

		if (is_mbox && ii + 1 < message_uids->len)
			g_byte_array_append (byte_array, (guint8 *) "\n", 1);

		if (byte_array->len > 0) {
			success = g_output_stream_write_all (
				G_OUTPUT_STREAM (file_output_stream),
				byte_array->data, byte_array->len,
				NULL, cancellable, error);

			if (!success)
				goto exit;

			/* Reset the byte array for the next message. */
			g_byte_array_set_size (byte_array, 0);
		}

		percent = ((ii + 1) * 100) / message_uids->len;
		camel_operation_progress (cancellable, percent);
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
	GTask *task;
	AsyncContext *context;

	g_return_if_fail (CAMEL_IS_FOLDER (folder));
	g_return_if_fail (message_uids != NULL);
	g_return_if_fail (G_IS_FILE (destination));

	/* Need at least one message UID to save. */
	g_return_if_fail (message_uids->len > 0);

	context = g_slice_new0 (AsyncContext);
	context->ptr_array = g_ptr_array_ref (message_uids);
	context->destination = g_object_ref (destination);

	task = g_task_new (folder, cancellable, callback, user_data);
	g_task_set_source_tag (task, e_mail_folder_save_messages);
	g_task_set_priority (task, io_priority);
	g_task_set_task_data (task, g_steal_pointer (&context), (GDestroyNotify) async_context_free);

	g_task_run_in_thread (task, mail_folder_save_messages_thread);

	g_object_unref (task);
}

gboolean
e_mail_folder_save_messages_finish (CamelFolder *folder,
                                    GAsyncResult *result,
                                    GError **error)
{
	g_return_val_if_fail (g_task_is_valid (result, folder), FALSE);
	g_return_val_if_fail (g_async_result_is_tagged (result, e_mail_folder_save_messages), FALSE);

	return g_task_propagate_boolean (G_TASK (result), error);
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
	}

	if (CAMEL_IS_STORE (service) && folder_name != NULL) {
		if (out_store != NULL)
			*out_store = CAMEL_STORE (g_object_ref (service));

		if (out_folder_name != NULL) {
			*out_folder_name = folder_name;
			folder_name = NULL;
		}

		success = TRUE;
	} else {
		g_set_error (
			error, CAMEL_FOLDER_ERROR,
			CAMEL_FOLDER_ERROR_INVALID,
			_("Invalid folder URI “%s”"),
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

		if (!folder_name || !strchr (folder_name, '/') || (
		    CAMEL_IS_VEE_FOLDER (folder) && (
		    g_strcmp0 (folder_name, CAMEL_VTRASH_NAME) == 0 ||
		    g_strcmp0 (folder_name, CAMEL_VJUNK_NAME) == 0))) {
			full_display_name = g_strdup_printf ("%s : %s", service_display_name, camel_folder_get_display_name (folder));
		} else {
			full_display_name = g_strdup_printf ("%s : %s", service_display_name, camel_folder_get_full_display_name (folder));
		}

		g_free (folder_name);
	}

	g_clear_object (&session);
	g_free (folder_uri);

	return full_display_name;
}
