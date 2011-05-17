/*
 * e-mail-store-utils.c
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

#include "e-mail-store-utils.h"

#include <config.h>
#include <glib/gi18n-lib.h>

typedef struct _AsyncContext AsyncContext;

static void
mail_store_go_offline_thread (GSimpleAsyncResult *simple,
                              CamelStore *store,
                              GCancellable *cancellable)
{
	CamelService *service;
	gchar *service_name;
	GError *error = NULL;

	service = CAMEL_SERVICE (store);

	service_name = camel_service_get_name (service, TRUE);
	camel_operation_push_message (
		cancellable, _("Disconnecting from '%s'"), service_name);
	g_free (service_name);

	if (CAMEL_IS_DISCO_STORE (store)) {
		CamelDiscoStore *disco_store;

		disco_store = CAMEL_DISCO_STORE (store);

		if (camel_disco_store_can_work_offline (disco_store))
			camel_disco_store_set_status (
				disco_store, CAMEL_DISCO_STORE_OFFLINE,
				cancellable, &error);
		else
			camel_service_disconnect_sync (service, TRUE, &error);

	} else if (CAMEL_IS_OFFLINE_STORE (store)) {
		CamelOfflineStore *offline_store;

		offline_store = CAMEL_OFFLINE_STORE (store);

		camel_offline_store_set_online_sync (
			offline_store, FALSE, cancellable, &error);

	} else
		camel_service_disconnect_sync (service, TRUE, &error);

	if (error != NULL) {
		g_simple_async_result_set_from_error (simple, error);
		g_error_free (error);
	}

	camel_operation_pop_message (cancellable);
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

	/* Cancel any pending connect first so the set_offline_op
	 * thread won't get queued behind a hung connect op. */
	camel_service_cancel_connect (CAMEL_SERVICE (store));

	simple = g_simple_async_result_new (
		G_OBJECT (store), callback,
		user_data, e_mail_store_go_offline);

	g_simple_async_result_run_in_thread (
		simple, (GSimpleAsyncThreadFunc)
		mail_store_go_offline_thread,
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

static void
mail_store_go_online_thread (GSimpleAsyncResult *simple,
                             CamelStore *store,
                             GCancellable *cancellable)
{
	CamelService *service;
	gchar *service_name;
	GError *error = NULL;

	service = CAMEL_SERVICE (store);

	service_name = camel_service_get_name (service, TRUE);
	camel_operation_push_message (
		cancellable, _("Reconnecting to '%s'"), service_name);
	g_free (service_name);

	if (CAMEL_IS_DISCO_STORE (store))
		camel_disco_store_set_status (
			CAMEL_DISCO_STORE (store),
			CAMEL_DISCO_STORE_ONLINE,
			cancellable, &error);

	else if (CAMEL_IS_OFFLINE_STORE (store))
		camel_offline_store_set_online_sync (
			CAMEL_OFFLINE_STORE (store),
			TRUE, cancellable, &error);

	if (error != NULL) {
		g_simple_async_result_set_from_error (simple, error);
		g_error_free (error);
	}

	camel_operation_pop_message (cancellable);
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

	g_simple_async_result_run_in_thread (
		simple, (GSimpleAsyncThreadFunc)
		mail_store_go_online_thread,
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

static void
mail_store_prepare_for_offline_thread (GSimpleAsyncResult *simple,
                                       CamelStore *store,
                                       GCancellable *cancellable)
{
	CamelService *service;
	gchar *service_name;
	GError *error = NULL;

	service = CAMEL_SERVICE (store);

	service_name = camel_service_get_name (service, TRUE);
	camel_operation_push_message (
		cancellable, _("Preparing account '%s' for offline"),
		service_name);
	g_free (service_name);

	if (CAMEL_IS_DISCO_STORE (store))
		camel_disco_store_prepare_for_offline (
			CAMEL_DISCO_STORE (store), cancellable, &error);

	else if (CAMEL_IS_OFFLINE_STORE (store))
		camel_offline_store_prepare_for_offline_sync (
			CAMEL_OFFLINE_STORE (store), cancellable, &error);

	if (error != NULL) {
		g_simple_async_result_set_from_error (simple, error);
		g_error_free (error);
	}

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

	g_simple_async_result_run_in_thread (
		simple, (GSimpleAsyncThreadFunc)
		mail_store_prepare_for_offline_thread,
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
