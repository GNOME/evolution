/*
 * e-client-utils.c
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
 * Copyright (C) 2011 Red Hat, Inc. (www.redhat.com)
 *
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <glib/gi18n-lib.h>
#include <gtk/gtk.h>
#include <libsoup/soup.h>

#include <libebook/libebook.h>
#include <libecal/libecal.h>

#include "e-client-utils.h"

/**
 * e_client_utils_new:
 *
 * Proxy function for e_book_client_utils_new() and e_cal_client_utils_new().
 *
 * Since: 3.2
 **/
EClient	*
e_client_utils_new (ESource *source,
                    EClientSourceType source_type,
                    GError **error)
{
	EClient *res = NULL;

	g_return_val_if_fail (source != NULL, NULL);
	g_return_val_if_fail (E_IS_SOURCE (source), NULL);

	switch (source_type) {
	case E_CLIENT_SOURCE_TYPE_CONTACTS:
		res = E_CLIENT (e_book_client_new (source, error));
		break;
	case E_CLIENT_SOURCE_TYPE_EVENTS:
		res = E_CLIENT (e_cal_client_new (source, E_CAL_CLIENT_SOURCE_TYPE_EVENTS, error));
		break;
	case E_CLIENT_SOURCE_TYPE_MEMOS:
		res = E_CLIENT (e_cal_client_new (source, E_CAL_CLIENT_SOURCE_TYPE_MEMOS, error));
		break;
	case E_CLIENT_SOURCE_TYPE_TASKS:
		res = E_CLIENT (e_cal_client_new (source, E_CAL_CLIENT_SOURCE_TYPE_TASKS, error));
		break;
	default:
		g_return_val_if_reached (NULL);
		break;
	}

	return res;
}

typedef struct _EClientUtilsAsyncOpData {
	GAsyncReadyCallback callback;
	gpointer user_data;
	GCancellable *cancellable;
	ESource *source;
	EClient *client;
	gboolean open_finished;
	GError *opened_cb_error;
	guint retry_open_id;
	gboolean only_if_exists;
	guint pending_properties_count;
} EClientUtilsAsyncOpData;

static void
free_client_utils_async_op_data (EClientUtilsAsyncOpData *async_data)
{
	g_return_if_fail (async_data != NULL);
	g_return_if_fail (async_data->cancellable != NULL);
	g_return_if_fail (async_data->client != NULL);

	if (async_data->retry_open_id)
		g_source_remove (async_data->retry_open_id);

	g_signal_handlers_disconnect_matched (async_data->cancellable, G_SIGNAL_MATCH_DATA, 0, 0, NULL, NULL, async_data);
	g_signal_handlers_disconnect_matched (async_data->client, G_SIGNAL_MATCH_DATA, 0, 0, NULL, NULL, async_data);

	if (async_data->opened_cb_error)
		g_error_free (async_data->opened_cb_error);
	g_object_unref (async_data->cancellable);
	g_object_unref (async_data->client);
	g_object_unref (async_data->source);
	g_free (async_data);
}

static gboolean
complete_async_op_in_idle_cb (gpointer user_data)
{
	GSimpleAsyncResult *simple = user_data;
	gint run_main_depth;

	g_return_val_if_fail (simple != NULL, FALSE);

	run_main_depth = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (simple), "run-main-depth"));
	if (run_main_depth < 1)
		run_main_depth = 1;

	/* do not receive in higher level than was initially run */
	if (g_main_depth () > run_main_depth) {
		return TRUE;
	}

	g_simple_async_result_complete (simple);
	g_object_unref (simple);

	return FALSE;
}

#define return_async_error_if_fail(expr, callback, user_data, src, source_tag) G_STMT_START {	\
	if (G_LIKELY ((expr))) { } else {								\
		GError *error;										\
													\
		error = g_error_new (E_CLIENT_ERROR, E_CLIENT_ERROR_INVALID_ARG,			\
				"%s: assertion '%s' failed", G_STRFUNC, #expr);				\
													\
		return_async_error (error, callback, user_data, src, source_tag);		\
		g_error_free (error);									\
		return;											\
	}												\
	} G_STMT_END

static void
return_async_error (const GError *error,
                    GAsyncReadyCallback callback,
                    gpointer user_data,
                    ESource *source,
                    gpointer source_tag)
{
	GSimpleAsyncResult *simple;

	g_return_if_fail (error != NULL);
	g_return_if_fail (source_tag != NULL);

	simple = g_simple_async_result_new (G_OBJECT (source), callback, user_data, source_tag);
	g_simple_async_result_set_from_error (simple, error);

	g_object_set_data (G_OBJECT (simple), "run-main-depth", GINT_TO_POINTER (g_main_depth ()));
	g_idle_add (complete_async_op_in_idle_cb, simple);
}

static void
client_utils_get_backend_property_cb (GObject *source_object,
                                      GAsyncResult *result,
                                      gpointer user_data)
{
	EClient *client = E_CLIENT (source_object);
	EClientUtilsAsyncOpData *async_data = user_data;
	GSimpleAsyncResult *simple;

	g_return_if_fail (async_data != NULL);
	g_return_if_fail (async_data->client != NULL);
	g_return_if_fail (async_data->client == client);

	if (result) {
		gchar *prop_value = NULL;

		if (e_client_get_backend_property_finish (client, result, &prop_value, NULL))
			g_free (prop_value);

		async_data->pending_properties_count--;
		if (async_data->pending_properties_count)
			return;
	}

	simple = g_simple_async_result_new (G_OBJECT (async_data->source), async_data->callback, async_data->user_data, e_client_utils_open_new);
	g_simple_async_result_set_op_res_gpointer (simple, g_object_ref (async_data->client), g_object_unref);

	g_object_set_data (G_OBJECT (simple), "run-main-depth", GINT_TO_POINTER (g_main_depth ()));
	g_idle_add (complete_async_op_in_idle_cb, simple);

	free_client_utils_async_op_data (async_data);
}

static void
client_utils_capabilities_retrieved_cb (GObject *source_object,
                                        GAsyncResult *result,
                                        gpointer user_data)
{
	EClient *client = E_CLIENT (source_object);
	EClientUtilsAsyncOpData *async_data = user_data;
	gchar *capabilities = NULL;
	gboolean caps_res;

	g_return_if_fail (async_data != NULL);
	g_return_if_fail (async_data->client != NULL);
	g_return_if_fail (async_data->client == client);

	caps_res = e_client_retrieve_capabilities_finish (client, result, &capabilities, NULL);
	g_free (capabilities);

	if (caps_res) {
		async_data->pending_properties_count = 1;

		/* precache backend properties */
		if (E_IS_CAL_CLIENT (client)) {
			async_data->pending_properties_count += 3;

			e_client_get_backend_property (async_data->client, CAL_BACKEND_PROPERTY_CAL_EMAIL_ADDRESS, async_data->cancellable, client_utils_get_backend_property_cb, async_data);
			e_client_get_backend_property (async_data->client, CAL_BACKEND_PROPERTY_ALARM_EMAIL_ADDRESS, async_data->cancellable, client_utils_get_backend_property_cb, async_data);
			e_client_get_backend_property (async_data->client, CAL_BACKEND_PROPERTY_DEFAULT_OBJECT, async_data->cancellable, client_utils_get_backend_property_cb, async_data);
		} else if (E_IS_BOOK_CLIENT (client)) {
			async_data->pending_properties_count += 2;

			e_client_get_backend_property (async_data->client, BOOK_BACKEND_PROPERTY_REQUIRED_FIELDS, async_data->cancellable, client_utils_get_backend_property_cb, async_data);
			e_client_get_backend_property (async_data->client, BOOK_BACKEND_PROPERTY_SUPPORTED_FIELDS, async_data->cancellable, client_utils_get_backend_property_cb, async_data);
		} else {
			g_warn_if_reached ();
			client_utils_get_backend_property_cb (source_object, NULL, async_data);
			return;
		}

		e_client_get_backend_property (async_data->client, CLIENT_BACKEND_PROPERTY_CACHE_DIR, async_data->cancellable, client_utils_get_backend_property_cb, async_data);
	} else {
		client_utils_get_backend_property_cb (source_object, NULL, async_data);
	}
}

static void
client_utils_open_new_done (EClientUtilsAsyncOpData *async_data)
{
	g_return_if_fail (async_data != NULL);
	g_return_if_fail (async_data->client != NULL);

	/* retrieve capabilities just to have them cached on #EClient for later use */
	e_client_retrieve_capabilities (async_data->client, async_data->cancellable, client_utils_capabilities_retrieved_cb, async_data);
}

static void client_utils_opened_cb (EClient *client, const GError *error, EClientUtilsAsyncOpData *async_data);

static void
finish_or_retry_open (EClientUtilsAsyncOpData *async_data,
                      const GError *error)
{
	g_return_if_fail (async_data != NULL);

	if (error) {
		return_async_error (error, async_data->callback, async_data->user_data, async_data->source, e_client_utils_open_new);
		free_client_utils_async_op_data (async_data);
	} else {
		client_utils_open_new_done (async_data);
	}
}

static void
client_utils_opened_cb (EClient *client,
                        const GError *error,
                        EClientUtilsAsyncOpData *async_data)
{
	g_return_if_fail (client != NULL);
	g_return_if_fail (async_data != NULL);
	g_return_if_fail (client == async_data->client);

	g_signal_handlers_disconnect_by_func (client, G_CALLBACK (client_utils_opened_cb), async_data);

	if (!async_data->open_finished) {
		/* there can happen that the "opened" signal is received
		 * before the e_client_open () is finished, thus keep detailed
		 * error for later use, if any */
		if (error)
			async_data->opened_cb_error = g_error_copy (error);
	} else {
		finish_or_retry_open (async_data, error);
	}
}

static void
client_utils_open_new_async_cb (GObject *source_object,
                                GAsyncResult *result,
                                gpointer user_data)
{
	EClientUtilsAsyncOpData *async_data = user_data;
	GError *error = NULL;

	g_return_if_fail (source_object != NULL);
	g_return_if_fail (result != NULL);
	g_return_if_fail (async_data != NULL);
	g_return_if_fail (async_data->callback != NULL);
	g_return_if_fail (async_data->client == E_CLIENT (source_object));

	async_data->open_finished = TRUE;

	if (!e_client_open_finish (E_CLIENT (source_object), result, &error)
	    || g_cancellable_set_error_if_cancelled (async_data->cancellable, &error)) {
		finish_or_retry_open (async_data, error);
		g_error_free (error);
		return;
	}

	if (async_data->opened_cb_error) {
		finish_or_retry_open (async_data, async_data->opened_cb_error);
		return;
	}

	if (e_client_is_opened (async_data->client)) {
		client_utils_open_new_done (async_data);
		return;
	}

	/* wait for 'opened' signal, which is received in client_utils_opened_cb */
}

/**
 * e_client_utils_open_new:
 * @source: an #ESource to be opened
 * @source_type: an #EClientSourceType of the @source
 * @only_if_exists: if %TRUE, fail if this client doesn't already exist, otherwise create it first
 * @cancellable: a #GCancellable; can be %NULL
 * @callback: callback to call when a result is ready
 * @user_data: user data for the @callback
 *
 * Begins asynchronous opening of a new #EClient corresponding
 * to the @source of type @source_type. The resulting #EClient
 * is fully opened and authenticated client, ready to be used.
 * The opened client has also fetched capabilities.
 * This call is finished by e_client_utils_open_new_finish()
 * from the @callback.
 *
 * Since: 3.2
 **/
void
e_client_utils_open_new (ESource *source,
                         EClientSourceType source_type,
                         gboolean only_if_exists,
                         GCancellable *cancellable,
                         GAsyncReadyCallback callback,
                         gpointer user_data)
{
	EClient *client;
	GError *error = NULL;
	EClientUtilsAsyncOpData *async_data;

	g_return_if_fail (callback != NULL);
	return_async_error_if_fail (source != NULL, callback, user_data, source, e_client_utils_open_new);
	return_async_error_if_fail (E_IS_SOURCE (source), callback, user_data, source, e_client_utils_open_new);

	client = e_client_utils_new (source, source_type, &error);
	if (!client) {
		return_async_error (error, callback, user_data, source, e_client_utils_open_new);
		g_error_free (error);
		return;
	}

	async_data = g_new0 (EClientUtilsAsyncOpData, 1);
	async_data->callback = callback;
	async_data->user_data = user_data;
	async_data->source = g_object_ref (source);
	async_data->client = client;
	async_data->open_finished = FALSE;
	async_data->only_if_exists = only_if_exists;
	async_data->retry_open_id = 0;

	if (cancellable)
		async_data->cancellable = g_object_ref (cancellable);
	else
		async_data->cancellable = g_cancellable_new ();

	/* wait till backend notifies about its opened state */
	g_signal_connect (client, "opened", G_CALLBACK (client_utils_opened_cb), async_data);

	e_client_open (async_data->client, async_data->only_if_exists, async_data->cancellable, client_utils_open_new_async_cb, async_data);
}

/**
 * e_client_utils_open_new_finish:
 * @source: an #ESource on which the e_client_utils_open_new() was invoked
 * @result: a #GAsyncResult
 * @client: (out): Return value for an #EClient
 * @error: (out): a #GError to set an error, if any
 *
 * Finishes previous call of e_client_utils_open_new() and
 * sets @client to a fully opened and authenticated #EClient.
 * This @client, if not NULL, should be freed with g_object_unref().
 *
 * Returns: %TRUE if successful, %FALSE otherwise.
 *
 * Since: 3.2
 **/
gboolean
e_client_utils_open_new_finish (ESource *source,
                                GAsyncResult *result,
                                EClient **client,
                                GError **error)
{
	GSimpleAsyncResult *simple;

	g_return_val_if_fail (source != NULL, FALSE);
	g_return_val_if_fail (result != NULL, FALSE);
	g_return_val_if_fail (client != NULL, FALSE);
	g_return_val_if_fail (g_simple_async_result_is_valid (result, G_OBJECT (source), e_client_utils_open_new), FALSE);

	*client = NULL;
	simple = G_SIMPLE_ASYNC_RESULT (result);

	if (g_simple_async_result_propagate_error (simple, error))
		return FALSE;

	*client = g_object_ref (g_simple_async_result_get_op_res_gpointer (simple));

	return *client != NULL;
}
