/*
 * e-mail-folder-utils.c
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
 */

#include "e-mail-folder-utils.h"

#include <config.h>
#include <glib/gi18n-lib.h>

/* X-Mailer header value */
#define X_MAILER ("Evolution " VERSION SUB_VERSION " " VERSION_COMMENT)

typedef struct _AsyncContext AsyncContext;

struct _AsyncContext {
	CamelMimeMessage *message;
	CamelMessageInfo *info;
	gchar *message_uid;
};

static void
async_context_free (AsyncContext *context)
{
	if (context->message != NULL)
		g_object_unref (context->message);

	if (context->info != NULL)
		camel_message_info_free (context->info);

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

	if (error != NULL) {
		g_simple_async_result_set_from_error (simple, error);
		g_error_free (error);
	}
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
	gboolean success;

	g_return_val_if_fail (CAMEL_IS_FOLDER (folder), FALSE);
	g_return_val_if_fail (CAMEL_IS_MIME_MESSAGE (message), FALSE);

	medium = CAMEL_MEDIUM (message);

	camel_operation_push_message (
		cancellable,
		_("Saving message to folder '%s'"),
		camel_folder_get_full_name (folder));

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
		context->info = camel_message_info_ref (info);

	simple = g_simple_async_result_new (
		G_OBJECT (folder), callback, user_data,
		e_mail_folder_append_message);

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
	encoded_name = camel_url_encode (folder_name, "#");

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
			gchar *uid = g_strdup (url->host);
			camel_url_decode (uid);
			service = camel_session_get_service (session, uid);
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
			service = camel_session_get_service (session, uid);
			g_free (uid);
		}

		if (url->path != NULL && *url->path == '/')
			folder_name = g_strdup (url->path + 1);

	/* CamelFolderInfo URIs used to embed the store's URI, so the
	 * folder name is appended as either a path part or a fragment
	 * part, depending whether the store's URI used the path part.
	 * To determine which it is, you have to check the provider
	 * flags for CAMEL_URL_FRAGMENT_IS_PATH. */
	} else {
		service = camel_session_get_service_by_url (
			session, url, CAMEL_PROVIDER_STORE);

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
	g_return_val_if_fail (class->compare_folder_name != NULL, FALSE);

	equal = class->compare_folder_name (folder_name_a, folder_name_b);

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
