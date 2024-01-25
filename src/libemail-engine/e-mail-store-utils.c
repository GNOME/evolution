/*
 * e-mail-store-utils.c
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
#include "e-mail-utils.h"

#include "e-mail-store-utils.h"

#include <glib/gi18n-lib.h>

gboolean
e_mail_store_create_folder_sync (CamelStore *store,
                                 const gchar *full_name,
                                 GCancellable *cancellable,
                                 GError **error)
{
	CamelFolderInfo *folder_info;
	gchar *copied_full_name;
	gchar *display_name;
	const gchar *parent;
	gboolean success = TRUE;

	g_return_val_if_fail (CAMEL_IS_STORE (store), FALSE);
	g_return_val_if_fail (full_name != NULL, FALSE);

	copied_full_name = g_strdup (full_name);
	display_name = strrchr (copied_full_name, '/');
	if (display_name == NULL) {
		display_name = copied_full_name;
		parent = "";
	} else {
		*display_name++ = '\0';
		parent = copied_full_name;
	}

	folder_info = camel_store_create_folder_sync (
		store, parent, display_name, cancellable, error);

	g_free (copied_full_name);

	if (folder_info == NULL)
		return FALSE;

	if (CAMEL_IS_SUBSCRIBABLE (store))
		success = camel_subscribable_subscribe_folder_sync (
			CAMEL_SUBSCRIBABLE (store),
			full_name, cancellable, error);

	camel_folder_info_free (folder_info);

	return success;
}

/* Helper for e_mail_store_create_folder() */
static void
mail_store_create_folder_thread (GTask *task,
                                 gpointer source_object,
                                 gpointer task_data,
                                 GCancellable *cancellable)
{
	const gchar *full_name;
	GError *local_error = NULL;
	gboolean res;

	full_name = task_data;

	res = e_mail_store_create_folder_sync (
		CAMEL_STORE (source_object),
		full_name,
		cancellable, &local_error);

	if (local_error != NULL)
		g_task_return_error (task, g_steal_pointer (&local_error));
	else
		g_task_return_boolean (task, res);
}

void
e_mail_store_create_folder (CamelStore *store,
                            const gchar *full_name,
                            gint io_priority,
                            GCancellable *cancellable,
                            GAsyncReadyCallback callback,
                            gpointer user_data)
{
	GTask *task;

	g_return_if_fail (CAMEL_IS_STORE (store));
	g_return_if_fail (full_name != NULL);

	task = g_task_new (store, cancellable, callback, user_data);
	g_task_set_source_tag (task, e_mail_store_create_folder);
	g_task_set_priority (task, io_priority);
	g_task_set_task_data (task, g_strdup (full_name), g_free);

	g_task_run_in_thread (task, mail_store_create_folder_thread);

	g_object_unref (task);
}

gboolean
e_mail_store_create_folder_finish (CamelStore *store,
                                   GAsyncResult *result,
                                   GError **error)
{
	g_return_val_if_fail (g_task_is_valid (result, store), FALSE);
	g_return_val_if_fail (g_async_result_is_tagged (result, e_mail_store_create_folder), FALSE);

	return g_task_propagate_boolean (G_TASK (result), error);
}

/* Helper for e_mail_store_go_offline() */
static void
mail_store_go_offline_thread (GTask *task,
                              gpointer source_object,
                              gpointer task_data,
                              GCancellable *cancellable)
{
	GError *local_error = NULL;
	gboolean res;

	res = e_mail_store_go_offline_sync (
		CAMEL_STORE (source_object), cancellable, &local_error);

	if (local_error != NULL)
		g_task_return_error (task, g_steal_pointer (&local_error));
	else
		g_task_return_boolean (task, res);
}

gboolean
e_mail_store_go_offline_sync (CamelStore *store,
                              GCancellable *cancellable,
                              GError **error)
{
	CamelService *service;
	const gchar *display_name;
	gboolean success = TRUE;

	g_return_val_if_fail (CAMEL_IS_STORE (store), FALSE);

	service = CAMEL_SERVICE (store);

	display_name = camel_service_get_display_name (service);
	if (display_name == NULL || *display_name == '\0')
		display_name = G_OBJECT_TYPE_NAME (service);

	camel_operation_push_message (
		cancellable, _("Disconnecting from “%s”"), display_name);

	if (CAMEL_IS_OFFLINE_STORE (store)) {
		success = camel_offline_store_set_online_sync (
			CAMEL_OFFLINE_STORE (store),
			FALSE, cancellable, error);

	} else {
		success = camel_service_disconnect_sync (
			service, TRUE, cancellable, error);
	}

	camel_operation_pop_message (cancellable);

	return success;
}

void
e_mail_store_go_offline (CamelStore *store,
                         gint io_priority,
                         GCancellable *cancellable,
                         GAsyncReadyCallback callback,
                         gpointer user_data)
{
	GTask *task;

	g_return_if_fail (CAMEL_IS_STORE (store));

	task = g_task_new (store, cancellable, callback, user_data);
	g_task_set_source_tag (task, e_mail_store_go_offline);
	g_task_set_priority (task, io_priority);

	g_task_run_in_thread (task, mail_store_go_offline_thread);

	g_object_unref (task);
}

gboolean
e_mail_store_go_offline_finish (CamelStore *store,
                                GAsyncResult *result,
                                GError **error)
{
	g_return_val_if_fail (g_task_is_valid (result, store), FALSE);
	g_return_val_if_fail (g_async_result_is_tagged (result, e_mail_store_go_offline), FALSE);

	return g_task_propagate_boolean (G_TASK (result), error);
}

gboolean
e_mail_store_go_online_sync (CamelStore *store,
                             GCancellable *cancellable,
                             GError **error)
{
	CamelService *service;
	const gchar *display_name;
	gboolean success = TRUE;

	g_return_val_if_fail (CAMEL_IS_STORE (store), FALSE);

	service = CAMEL_SERVICE (store);

	display_name = camel_service_get_display_name (service);
	if (display_name == NULL || *display_name == '\0')
		display_name = G_OBJECT_TYPE_NAME (service);

	camel_operation_push_message (
		cancellable, _("Reconnecting to “%s”"), display_name);

	if (CAMEL_IS_OFFLINE_STORE (store))
		success = camel_offline_store_set_online_sync (
			CAMEL_OFFLINE_STORE (store),
			TRUE, cancellable, error);

	camel_operation_pop_message (cancellable);

	return success;
}

/* Helper for e_mail_store_go_online() */
static void
mail_store_go_online_thread (GTask *task,
                             gpointer source_object,
                             gpointer task_data,
                             GCancellable *cancellable)
{
	GError *local_error = NULL;
	gboolean res;

	res = e_mail_store_go_online_sync (
		CAMEL_STORE (source_object), cancellable, &local_error);

	if (local_error != NULL)
		g_task_return_error (task, g_steal_pointer (&local_error));
	else
		g_task_return_boolean (task, res);
}

void
e_mail_store_go_online (CamelStore *store,
                        gint io_priority,
                        GCancellable *cancellable,
                        GAsyncReadyCallback callback,
                        gpointer user_data)
{
	GTask *task;

	g_return_if_fail (CAMEL_IS_STORE (store));

	task = g_task_new (store, cancellable, callback, user_data);
	g_task_set_source_tag (task, e_mail_store_go_online);
	g_task_set_priority (task, io_priority);

	g_task_run_in_thread (task, mail_store_go_online_thread);

	g_object_unref (task);
}

gboolean
e_mail_store_go_online_finish (CamelStore *store,
                               GAsyncResult *result,
                               GError **error)
{
	g_return_val_if_fail (g_task_is_valid (result, store), FALSE);
	g_return_val_if_fail (g_async_result_is_tagged (result, e_mail_store_go_online), FALSE);

	return g_task_propagate_boolean (G_TASK (result), error);
}

/* Helper for e_mail_store_prepare_for_offline() */
static void
mail_store_prepare_for_offline_thread (GTask *task,
                                       gpointer source_object,
                                       gpointer task_data,
                                       GCancellable *cancellable)
{
	CamelService *service;
	const gchar *display_name;
	GError *local_error = NULL;

	service = CAMEL_SERVICE (source_object);

	display_name = camel_service_get_display_name (service);
	if (display_name == NULL || *display_name == '\0')
		display_name = G_OBJECT_TYPE_NAME (service);

	camel_operation_push_message (
		cancellable, _("Preparing account “%s” for offline"),
		display_name);

	if (CAMEL_IS_OFFLINE_STORE (service))
		camel_offline_store_prepare_for_offline_sync (
			CAMEL_OFFLINE_STORE (service),
			cancellable, &local_error);

	camel_operation_pop_message (cancellable);

	if (local_error != NULL)
		g_task_return_error (task, g_steal_pointer (&local_error));
	else
		g_task_return_boolean (task, TRUE);
}

void
e_mail_store_prepare_for_offline (CamelStore *store,
                                  gint io_priority,
                                  GCancellable *cancellable,
                                  GAsyncReadyCallback callback,
                                  gpointer user_data)
{
	GTask *task;

	g_return_if_fail (CAMEL_IS_STORE (store));

	task = g_task_new (store, cancellable, callback, user_data);
	g_task_set_source_tag (task, e_mail_store_prepare_for_offline);
	g_task_set_priority (task, io_priority);

	g_task_run_in_thread (task, mail_store_prepare_for_offline_thread);

	g_object_unref (task);
}

gboolean
e_mail_store_prepare_for_offline_finish (CamelStore *store,
                                         GAsyncResult *result,
                                         GError **error)
{
	g_return_val_if_fail (g_task_is_valid (result, store), FALSE);
	g_return_val_if_fail (g_async_result_is_tagged (result, e_mail_store_prepare_for_offline), FALSE);

	return g_task_propagate_boolean (G_TASK (result), error);
}

static gboolean
mail_store_save_setup_key (CamelStore *store,
			   ESource *source,
			   const gchar *extension_name,
			   const gchar *property_name,
			   const gchar *type_id,
			   const gchar *value)
{
	gpointer extension;
	GObjectClass *klass;

	g_return_val_if_fail (CAMEL_IS_STORE (store), FALSE);
	if (source)
		g_return_val_if_fail (E_IS_SOURCE (source), FALSE);
	g_return_val_if_fail (extension_name != NULL, FALSE);
	g_return_val_if_fail (property_name != NULL, FALSE);
	g_return_val_if_fail (value != NULL, FALSE);

	if (!source)
		return FALSE;

	extension = e_source_get_extension (source, extension_name);
	if (!extension) {
		g_warning ("%s: Cannot find extension '%s'", G_STRFUNC, extension_name);
		return FALSE;
	}

	klass = G_OBJECT_GET_CLASS (extension);
	g_return_val_if_fail (klass != NULL, FALSE);

	if (!g_object_class_find_property (klass, property_name)) {
		g_warning ("%s: Extension '%s' doesn't have property '%s'", G_STRFUNC, extension_name, property_name);
		return FALSE;
	}

	if (!type_id || g_str_equal (type_id, "s")) {
		g_object_set (extension, property_name, value, NULL);
	} else if (g_str_equal (type_id, "b")) {
		gboolean val;

		val = g_strcmp0 (value, "false") != 0 && g_strcmp0 (value, "0") != 0;

		g_object_set (extension, property_name, val, NULL);
	} else if (g_str_equal (type_id, "i")) {
		gint val;

		val = (gint) g_ascii_strtoll (value, NULL, 10);

		g_object_set (extension, property_name, val, NULL);
	} else if (g_str_equal (type_id, "f")) {
		gchar *folder_uri;

		folder_uri = e_mail_folder_uri_build (store, value);
		g_object_set (extension, property_name, folder_uri, NULL);
		g_free (folder_uri);
	} else {
		g_warning ("%s: Unknown type identifier '%s' provided", G_STRFUNC, type_id);
		return FALSE;
	}

	return TRUE;
}

gboolean
e_mail_store_save_initial_setup_sync (CamelStore *store,
				      GHashTable *save_setup,
				      ESource *collection_source,
				      ESource *account_source,
				      ESource *submission_source,
				      ESource *transport_source,
				      gboolean write_sources,
				      GCancellable *cancellable,
				      GError **error)
{
	gboolean collection_changed = FALSE;
	gboolean account_changed = FALSE;
	gboolean submission_changed = FALSE;
	gboolean transport_changed = FALSE;
	gboolean success = TRUE;
	GHashTableIter iter;
	gpointer key, value;

	g_return_val_if_fail (CAMEL_IS_STORE (store), FALSE);
	g_return_val_if_fail (save_setup != NULL, FALSE);
	g_return_val_if_fail (E_IS_SOURCE (account_source), FALSE);

	if (!g_hash_table_size (save_setup))
		return TRUE;

	/* The key name consists of up to four parts: Source:Extension:Property[:Type]
	   Source can be 'Collection', 'Account', 'Submission', 'Transport', 'Backend'
	   Extension is any extension name; it's up to the key creator to make sure
	   the extension belongs to that particular Source.
	   Property is a property name in the Extension.
	   Type is an optional letter describing the type of the value; if not set, then
	   string is used. Available values are: 'b' for boolean, 'i' for integer,
	   's' for string, 'f' for folder full path.
	   All the part values are case sensitive.
	*/

	g_hash_table_iter_init (&iter, save_setup);
	while (g_hash_table_iter_next (&iter, &key, &value)) {
		gchar **keys;

		keys = g_strsplit (key, ":", -1);
		if (g_strv_length (keys) < 3 || g_strv_length (keys) > 4) {
			g_warning ("%s: Incorrect store setup key, expects 3 or 4 parts, but %d given in '%s'",
				G_STRFUNC, g_strv_length (keys), (const gchar *) key);
		} else if (g_str_equal (keys[0], "Collection")) {
			if (mail_store_save_setup_key (store, collection_source, keys[1], keys[2], keys[3], value))
				collection_changed = TRUE;
		} else if (g_str_equal (keys[0], "Account")) {
			if (mail_store_save_setup_key (store, account_source, keys[1], keys[2], keys[3], value))
				account_changed = TRUE;
		} else if (g_str_equal (keys[0], "Submission")) {
			if (mail_store_save_setup_key (store, submission_source, keys[1], keys[2], keys[3], value))
				submission_changed = TRUE;
		} else if (g_str_equal (keys[0], "Transport")) {
			if (mail_store_save_setup_key (store, transport_source, keys[1], keys[2], keys[3], value))
				transport_changed = TRUE;
		} else if (g_str_equal (keys[0], "Backend")) {
			ESource *backend_source = NULL;

			if (collection_source && e_source_has_extension (collection_source, keys[1]))
				backend_source = collection_source;
			else if (account_source && e_source_has_extension (account_source, keys[1]))
				backend_source = account_source;

			if (mail_store_save_setup_key (store, backend_source, keys[1], keys[2], keys[3], value))
				transport_changed = TRUE;
		} else {
			g_warning ("%s: Unknown source name '%s' given in '%s'", G_STRFUNC, keys[0], (const gchar *) key);
		}

		g_strfreev (keys);
	}

	if (write_sources) {
		if (transport_changed && success && e_source_get_writable (transport_source))
			success = e_source_write_sync (transport_source, cancellable, error);
		if (submission_changed && success && e_source_get_writable (submission_source))
			success = e_source_write_sync (submission_source, cancellable, error);
		if (account_changed && success && e_source_get_writable (account_source))
			success = e_source_write_sync (account_source, cancellable, error);
		if (collection_changed && success && e_source_get_writable (collection_source))
			success = e_source_write_sync (collection_source, cancellable, error);
	}

	return success;
}
