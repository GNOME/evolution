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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "e-mail-utils.h"

#include "e-mail-store-utils.h"

#include <glib/gi18n-lib.h>

typedef struct _AsyncContext AsyncContext;

struct _AsyncContext {
	gchar *full_name;
};

static void
async_context_free (AsyncContext *context)
{
	g_free (context->full_name);

	g_slice_free (AsyncContext, context);
}

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
mail_store_create_folder_thread (GSimpleAsyncResult *simple,
                                 GObject *source_object,
                                 GCancellable *cancellable)
{
	AsyncContext *context;
	GError *local_error = NULL;

	context = g_simple_async_result_get_op_res_gpointer (simple);

	e_mail_store_create_folder_sync (
		CAMEL_STORE (source_object),
		context->full_name,
		cancellable, &local_error);

	if (local_error != NULL)
		g_simple_async_result_take_error (simple, local_error);
}

void
e_mail_store_create_folder (CamelStore *store,
                            const gchar *full_name,
                            gint io_priority,
                            GCancellable *cancellable,
                            GAsyncReadyCallback callback,
                            gpointer user_data)
{
	GSimpleAsyncResult *simple;
	AsyncContext *context;

	g_return_if_fail (CAMEL_IS_STORE (store));
	g_return_if_fail (full_name != NULL);

	context = g_slice_new0 (AsyncContext);
	context->full_name = g_strdup (full_name);

	simple = g_simple_async_result_new (
		G_OBJECT (store), callback, user_data,
		e_mail_store_create_folder);

	g_simple_async_result_set_check_cancellable (simple, cancellable);

	g_simple_async_result_set_op_res_gpointer (
		simple, context, (GDestroyNotify) async_context_free);

	g_simple_async_result_run_in_thread (
		simple, mail_store_create_folder_thread,
		io_priority, cancellable);

	g_object_unref (simple);
}

gboolean
e_mail_store_create_folder_finish (CamelStore *store,
                                   GAsyncResult *result,
                                   GError **error)
{
	GSimpleAsyncResult *simple;

	g_return_val_if_fail (
		g_simple_async_result_is_valid (
		result, G_OBJECT (store),
		e_mail_store_create_folder), FALSE);

	simple = G_SIMPLE_ASYNC_RESULT (result);

	/* Assume success unless a GError is set. */
	return !g_simple_async_result_propagate_error (simple, error);
}

/* Helper for e_mail_store_go_offline() */
static void
mail_store_go_offline_thread (GSimpleAsyncResult *simple,
                              GObject *source_object,
                              GCancellable *cancellable)
{
	GError *local_error = NULL;

	e_mail_store_go_offline_sync (
		CAMEL_STORE (source_object), cancellable, &local_error);

	if (local_error != NULL)
		g_simple_async_result_take_error (simple, local_error);
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
		cancellable, _("Disconnecting from '%s'"), display_name);

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
	GSimpleAsyncResult *simple;

	g_return_if_fail (CAMEL_IS_STORE (store));

	simple = g_simple_async_result_new (
		G_OBJECT (store), callback,
		user_data, e_mail_store_go_offline);

	g_simple_async_result_set_check_cancellable (simple, cancellable);

	g_simple_async_result_run_in_thread (
		simple, mail_store_go_offline_thread,
		io_priority, cancellable);

	g_object_unref (simple);
}

gboolean
e_mail_store_go_offline_finish (CamelStore *store,
                                GAsyncResult *result,
                                GError **error)
{
	GSimpleAsyncResult *simple;

	g_return_val_if_fail (
		g_simple_async_result_is_valid (
		result, G_OBJECT (store), e_mail_store_go_offline), FALSE);

	simple = G_SIMPLE_ASYNC_RESULT (result);

	/* Assume success unless a GError is set. */
	return !g_simple_async_result_propagate_error (simple, error);
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
		cancellable, _("Reconnecting to '%s'"), display_name);

	if (CAMEL_IS_OFFLINE_STORE (store))
		success = camel_offline_store_set_online_sync (
			CAMEL_OFFLINE_STORE (store),
			TRUE, cancellable, error);

	camel_operation_pop_message (cancellable);

	return success;
}

/* Helper for e_mail_store_go_online() */
static void
mail_store_go_online_thread (GSimpleAsyncResult *simple,
                             GObject *source_object,
                             GCancellable *cancellable)
{
	GError *local_error = NULL;

	e_mail_store_go_online_sync (
		CAMEL_STORE (source_object), cancellable, &local_error);

	if (local_error != NULL)
		g_simple_async_result_take_error (simple, local_error);
}

void
e_mail_store_go_online (CamelStore *store,
                        gint io_priority,
                        GCancellable *cancellable,
                        GAsyncReadyCallback callback,
                        gpointer user_data)
{
	GSimpleAsyncResult *simple;

	g_return_if_fail (CAMEL_IS_STORE (store));

	simple = g_simple_async_result_new (
		G_OBJECT (store), callback,
		user_data, e_mail_store_go_online);

	g_simple_async_result_set_check_cancellable (simple, cancellable);

	g_simple_async_result_run_in_thread (
		simple, mail_store_go_online_thread,
		io_priority, cancellable);

	g_object_unref (simple);
}

gboolean
e_mail_store_go_online_finish (CamelStore *store,
                               GAsyncResult *result,
                               GError **error)
{
	GSimpleAsyncResult *simple;

	g_return_val_if_fail (
		g_simple_async_result_is_valid (
		result, G_OBJECT (store), e_mail_store_go_online), FALSE);

	simple = G_SIMPLE_ASYNC_RESULT (result);

	/* Assume success unless a GError is set. */
	return !g_simple_async_result_propagate_error (simple, error);
}

/* Helper for e_mail_store_prepare_for_offline() */
static void
mail_store_prepare_for_offline_thread (GSimpleAsyncResult *simple,
                                       GObject *source_object,
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
		cancellable, _("Preparing account '%s' for offline"),
		display_name);

	if (CAMEL_IS_OFFLINE_STORE (service))
		camel_offline_store_prepare_for_offline_sync (
			CAMEL_OFFLINE_STORE (service),
			cancellable, &local_error);

	if (local_error != NULL)
		g_simple_async_result_take_error (simple, local_error);

	camel_operation_pop_message (cancellable);
}

void
e_mail_store_prepare_for_offline (CamelStore *store,
                                  gint io_priority,
                                  GCancellable *cancellable,
                                  GAsyncReadyCallback callback,
                                  gpointer user_data)
{
	GSimpleAsyncResult *simple;

	g_return_if_fail (CAMEL_IS_STORE (store));

	simple = g_simple_async_result_new (
		G_OBJECT (store), callback, user_data,
		e_mail_store_prepare_for_offline);

	g_simple_async_result_set_check_cancellable (simple, cancellable);

	g_simple_async_result_run_in_thread (
		simple, mail_store_prepare_for_offline_thread,
		io_priority, cancellable);

	g_object_unref (simple);
}

gboolean
e_mail_store_prepare_for_offline_finish (CamelStore *store,
                                         GAsyncResult *result,
                                         GError **error)
{
	GSimpleAsyncResult *simple;

	g_return_val_if_fail (
		g_simple_async_result_is_valid (
		result, G_OBJECT (store),
		e_mail_store_prepare_for_offline), FALSE);

	simple = G_SIMPLE_ASYNC_RESULT (result);

	/* Assume success unless a GError is set. */
	return !g_simple_async_result_propagate_error (simple, error);
}
