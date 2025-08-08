/*
 * e-client-cache.c
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

/**
 * SECTION: e-client-cache
 * @include: e-util/e-util.h
 * @short_description: Shared #EClient instances
 *
 * #EClientCache provides for application-wide sharing of #EClient
 * instances and centralized rebroadcasting of #EClient::backend-died,
 * #EClient::backend-error and #GObject::notify signals from cached
 * #EClient instances.
 *
 * #EClientCache automatically invalidates cache entries in response to
 * #EClient::backend-died signals.  The #EClient instance is discarded,
 * and a new instance is created on the next request.
 **/

#include "evolution-config.h"

#include <glib/gi18n-lib.h>

#include <libecal/libecal.h>
#include <libebook/libebook.h>
#include <libebackend/libebackend.h>

#include "e-simple-async-result.h"
#include "e-client-cache.h"

typedef struct _ClientData ClientData;
typedef struct _SignalClosure SignalClosure;

struct _EClientCachePrivate {
	ESourceRegistry *registry;
	gulong source_removed_handler_id;
	gulong source_disabled_handler_id;

	GHashTable *client_ht;
	GMutex client_ht_lock;

	/* For signal emissions. */
	GMainContext *main_context;
};

struct _ClientData {
	volatile gint ref_count;
	GMutex lock;
	GWeakRef client_cache;
	EClient *client;
	GQueue connecting;
	gboolean dead_backend;
	gulong backend_died_handler_id;
	gulong backend_error_handler_id;
	gulong notify_handler_id;
};

struct _SignalClosure {
	EClientCache *client_cache;
	EClient *client;
	GParamSpec *pspec;
	gchar *error_message;
};

G_DEFINE_TYPE_WITH_CODE (EClientCache, e_client_cache, G_TYPE_OBJECT,
	G_ADD_PRIVATE (EClientCache)
	G_IMPLEMENT_INTERFACE (E_TYPE_EXTENSIBLE, NULL))

enum {
	PROP_0,
	PROP_REGISTRY
};

enum {
	BACKEND_DIED,
	BACKEND_ERROR,
	CLIENT_CONNECTED,
	CLIENT_CREATED,
	CLIENT_NOTIFY,
	ALLOW_AUTH_PROMPT,
	LAST_SIGNAL
};

static guint signals[LAST_SIGNAL];

static ClientData *
client_data_new (EClientCache *client_cache)
{
	ClientData *client_data;

	client_data = g_slice_new0 (ClientData);
	client_data->ref_count = 1;
	g_mutex_init (&client_data->lock);
	g_weak_ref_set (&client_data->client_cache, client_cache);

	return client_data;
}

static ClientData *
client_data_ref (ClientData *client_data)
{
	g_return_val_if_fail (client_data != NULL, NULL);
	g_return_val_if_fail (client_data->ref_count > 0, NULL);

	g_atomic_int_inc (&client_data->ref_count);

	return client_data;
}

static void
client_data_unref (ClientData *client_data)
{
	g_return_if_fail (client_data != NULL);
	g_return_if_fail (client_data->ref_count > 0);

	if (g_atomic_int_dec_and_test (&client_data->ref_count)) {

		/* The signal handlers hold a reference on client_data,
		 * so we should not be here unless the signal handlers
		 * have already been disconnected. */
		g_warn_if_fail (client_data->backend_died_handler_id == 0);
		g_warn_if_fail (client_data->backend_error_handler_id == 0);
		g_warn_if_fail (client_data->notify_handler_id == 0);

		g_mutex_clear (&client_data->lock);
		g_clear_object (&client_data->client);
		g_weak_ref_set (&client_data->client_cache, NULL);

		/* There should be no connect() operations in progress. */
		g_warn_if_fail (g_queue_is_empty (&client_data->connecting));

		g_slice_free (ClientData, client_data);
	}
}

static void
client_data_dispose (ClientData *client_data)
{
	g_mutex_lock (&client_data->lock);

	if (client_data->client != NULL) {
		g_signal_handler_disconnect (
			client_data->client,
			client_data->backend_died_handler_id);
		client_data->backend_died_handler_id = 0;

		g_signal_handler_disconnect (
			client_data->client,
			client_data->backend_error_handler_id);
		client_data->backend_error_handler_id = 0;

		g_signal_handler_disconnect (
			client_data->client,
			client_data->notify_handler_id);
		client_data->notify_handler_id = 0;

		g_clear_object (&client_data->client);
	}

	g_mutex_unlock (&client_data->lock);

	client_data_unref (client_data);
}

static void
signal_closure_free (SignalClosure *signal_closure)
{
	g_clear_object (&signal_closure->client_cache);
	g_clear_object (&signal_closure->client);

	if (signal_closure->pspec != NULL)
		g_param_spec_unref (signal_closure->pspec);

	g_free (signal_closure->error_message);

	g_slice_free (SignalClosure, signal_closure);
}

static ClientData *
client_ht_lookup (EClientCache *client_cache,
                  ESource *source,
                  const gchar *extension_name)
{
	GHashTable *client_ht;
	GHashTable *inner_ht;
	ClientData *client_data = NULL;

	g_return_val_if_fail (E_IS_SOURCE (source), NULL);
	g_return_val_if_fail (extension_name != NULL, NULL);

	client_ht = client_cache->priv->client_ht;

	g_mutex_lock (&client_cache->priv->client_ht_lock);

	/* We pre-load the hash table with supported extension names,
	 * so lookup failures indicate an unsupported extension name. */
	inner_ht = g_hash_table_lookup (client_ht, extension_name);
	if (inner_ht != NULL) {
		client_data = g_hash_table_lookup (inner_ht, source);
		if (client_data == NULL) {
			g_object_ref (source);
			client_data = client_data_new (client_cache);
			g_hash_table_insert (inner_ht, source, client_data);
		}
		client_data_ref (client_data);
	}

	g_mutex_unlock (&client_cache->priv->client_ht_lock);

	return client_data;
}

static gboolean
client_ht_remove (EClientCache *client_cache,
                  ESource *source)
{
	GHashTable *client_ht;
	GHashTableIter client_ht_iter;
	gpointer inner_ht;
	gboolean removed = FALSE;

	g_return_val_if_fail (E_IS_SOURCE (source), FALSE);

	client_ht = client_cache->priv->client_ht;

	g_mutex_lock (&client_cache->priv->client_ht_lock);

	g_hash_table_iter_init (&client_ht_iter, client_ht);

	while (g_hash_table_iter_next (&client_ht_iter, NULL, &inner_ht))
		removed |= g_hash_table_remove (inner_ht, source);

	g_mutex_unlock (&client_cache->priv->client_ht_lock);

	return removed;
}

static gboolean
client_cache_emit_backend_died_idle_cb (gpointer user_data)
{
	SignalClosure *signal_closure = user_data;
	ESourceRegistry *registry;
	EAlert *alert;
	ESource *source;
	const gchar *alert_id = NULL;
	const gchar *extension_name;
	gchar *display_name = NULL;

	source = e_client_get_source (signal_closure->client);
	registry = e_client_cache_ref_registry (signal_closure->client_cache);

	extension_name = E_SOURCE_EXTENSION_ADDRESS_BOOK;
	if (e_source_has_extension (source, extension_name)) {
		alert_id = "system:address-book-backend-died";
		display_name = e_source_registry_dup_unique_display_name (
			registry, source, extension_name);
	}

	extension_name = E_SOURCE_EXTENSION_CALENDAR;
	if (e_source_has_extension (source, extension_name)) {
		alert_id = "system:calendar-backend-died";
		display_name = e_source_registry_dup_unique_display_name (
			registry, source, extension_name);
	}

	extension_name = E_SOURCE_EXTENSION_MEMO_LIST;
	if (e_source_has_extension (source, extension_name)) {
		alert_id = "system:memo-list-backend-died";
		display_name = e_source_registry_dup_unique_display_name (
			registry, source, extension_name);
	}

	extension_name = E_SOURCE_EXTENSION_TASK_LIST;
	if (e_source_has_extension (source, extension_name)) {
		alert_id = "system:task-list-backend-died";
		display_name = e_source_registry_dup_unique_display_name (
			registry, source, extension_name);
	}

	g_object_unref (registry);

	g_return_val_if_fail (alert_id != NULL, FALSE);
	g_return_val_if_fail (display_name != NULL, FALSE);

	alert = e_alert_new (alert_id, display_name, NULL);

	g_signal_emit (
		signal_closure->client_cache,
		signals[BACKEND_DIED], 0,
		signal_closure->client,
		alert);

	g_object_unref (alert);

	g_free (display_name);

	return FALSE;
}

static gboolean
client_cache_emit_backend_error_idle_cb (gpointer user_data)
{
	SignalClosure *signal_closure = user_data;
	ESourceRegistry *registry;
	EAlert *alert;
	ESource *source;
	const gchar *alert_id = NULL;
	const gchar *extension_name;
	gchar *display_name = NULL;

	source = e_client_get_source (signal_closure->client);
	registry = e_client_cache_ref_registry (signal_closure->client_cache);

	extension_name = E_SOURCE_EXTENSION_ADDRESS_BOOK;
	if (e_source_has_extension (source, extension_name)) {
		alert_id = "system:address-book-backend-error";
		display_name = e_source_registry_dup_unique_display_name (
			registry, source, extension_name);
	}

	extension_name = E_SOURCE_EXTENSION_CALENDAR;
	if (e_source_has_extension (source, extension_name)) {
		alert_id = "system:calendar-backend-error";
		display_name = e_source_registry_dup_unique_display_name (
			registry, source, extension_name);
	}

	extension_name = E_SOURCE_EXTENSION_MEMO_LIST;
	if (e_source_has_extension (source, extension_name)) {
		alert_id = "system:memo-list-backend-error";
		display_name = e_source_registry_dup_unique_display_name (
			registry, source, extension_name);
	}

	extension_name = E_SOURCE_EXTENSION_TASK_LIST;
	if (e_source_has_extension (source, extension_name)) {
		alert_id = "system:task-list-backend-error";
		display_name = e_source_registry_dup_unique_display_name (
			registry, source, extension_name);
	}

	g_object_unref (registry);

	g_return_val_if_fail (alert_id != NULL, FALSE);
	g_return_val_if_fail (display_name != NULL, FALSE);

	alert = e_alert_new (
		alert_id, display_name,
		signal_closure->error_message, NULL);

	g_signal_emit (
		signal_closure->client_cache,
		signals[BACKEND_ERROR], 0,
		signal_closure->client,
		alert);

	g_object_unref (alert);

	g_free (display_name);

	return FALSE;
}

static gboolean
client_cache_emit_client_notify_idle_cb (gpointer user_data)
{
	SignalClosure *signal_closure = user_data;
	const gchar *name;

	name = g_param_spec_get_name (signal_closure->pspec);

	g_signal_emit (
		signal_closure->client_cache,
		signals[CLIENT_NOTIFY],
		g_quark_from_string (name),
		signal_closure->client,
		signal_closure->pspec);

	return FALSE;
}

static gboolean
client_cache_emit_client_created_idle_cb (gpointer user_data)
{
	SignalClosure *signal_closure = user_data;

	g_signal_emit (
		signal_closure->client_cache,
		signals[CLIENT_CREATED], 0,
		signal_closure->client);

	return FALSE;
}

static void
client_cache_backend_died_cb (EClient *client,
                              ClientData *client_data)
{
	EClientCache *client_cache;

	client_cache = g_weak_ref_get (&client_data->client_cache);

	if (client_cache != NULL) {
		GSource *idle_source;
		SignalClosure *signal_closure;

		signal_closure = g_slice_new0 (SignalClosure);
		signal_closure->client_cache = g_object_ref (client_cache);
		signal_closure->client = g_object_ref (client);

		idle_source = g_idle_source_new ();
		g_source_set_callback (
			idle_source,
			client_cache_emit_backend_died_idle_cb,
			signal_closure,
			(GDestroyNotify) signal_closure_free);
		g_source_attach (
			idle_source, client_cache->priv->main_context);
		g_source_unref (idle_source);

		g_object_unref (client_cache);
	}

	/* Discard the EClient and tag the backend as
	 * dead until we create a replacement EClient. */
	g_mutex_lock (&client_data->lock);
	g_clear_object (&client_data->client);
	client_data->dead_backend = TRUE;
	g_mutex_unlock (&client_data->lock);

}

static void
client_cache_backend_error_cb (EClient *client,
                               const gchar *error_message,
                               ClientData *client_data)
{
	EClientCache *client_cache;

	client_cache = g_weak_ref_get (&client_data->client_cache);

	if (client_cache != NULL) {
		GSource *idle_source;
		SignalClosure *signal_closure;

		signal_closure = g_slice_new0 (SignalClosure);
		signal_closure->client_cache = g_object_ref (client_cache);
		signal_closure->client = g_object_ref (client);
		signal_closure->error_message = g_strdup (error_message);

		idle_source = g_idle_source_new ();
		g_source_set_callback (
			idle_source,
			client_cache_emit_backend_error_idle_cb,
			signal_closure,
			(GDestroyNotify) signal_closure_free);
		g_source_attach (
			idle_source, client_cache->priv->main_context);
		g_source_unref (idle_source);

		g_object_unref (client_cache);
	}
}

static void
client_cache_notify_cb (EClient *client,
                        GParamSpec *pspec,
                        ClientData *client_data)
{
	EClientCache *client_cache;

	client_cache = g_weak_ref_get (&client_data->client_cache);

	if (client_cache != NULL) {
		GSource *idle_source;
		SignalClosure *signal_closure;

		signal_closure = g_slice_new0 (SignalClosure);
		signal_closure->client_cache = g_object_ref (client_cache);
		signal_closure->client = g_object_ref (client);
		signal_closure->pspec = g_param_spec_ref (pspec);

		idle_source = g_idle_source_new ();
		g_source_set_callback (
			idle_source,
			client_cache_emit_client_notify_idle_cb,
			signal_closure,
			(GDestroyNotify) signal_closure_free);
		g_source_attach (
			idle_source, client_cache->priv->main_context);
		g_source_unref (idle_source);

		g_object_unref (client_cache);
	}
}

static void
client_cache_process_results (ClientData *client_data,
                              EClient *client,
                              const GError *error)
{
	GQueue queue = G_QUEUE_INIT;

	/* Sanity check. */
	g_return_if_fail (
		((client != NULL) && (error == NULL)) ||
		((client == NULL) && (error != NULL)));

	g_mutex_lock (&client_data->lock);

	/* Complete async operations outside the lock. */
	e_queue_transfer (&client_data->connecting, &queue);

	if (client != NULL) {
		EClientCache *client_cache;

		/* Make sure we're not leaking a reference. This can happen when
		   a synchronous and an asynchronous open are interleaving. The
		   synchronous open bypasses pending openings, thus can eventually
		   overwrite, or preset, the client.
		*/
		g_clear_object (&client_data->client);

		client_data->client = g_object_ref (client);
		client_data->dead_backend = FALSE;

		client_cache = g_weak_ref_get (&client_data->client_cache);

		/* If the EClientCache has been disposed already,
		 * there's no point in connecting signal handlers. */
		if (client_cache != NULL) {
			GSource *idle_source;
			SignalClosure *signal_closure;
			gulong handler_id;

			/* client_data_dispose() will break the
			 * reference cycles we're creating here. */

			handler_id = g_signal_connect_data (
				client, "backend-died",
				G_CALLBACK (client_cache_backend_died_cb),
				client_data_ref (client_data),
				(GClosureNotify) client_data_unref,
				0);
			client_data->backend_died_handler_id = handler_id;

			handler_id = g_signal_connect_data (
				client, "backend-error",
				G_CALLBACK (client_cache_backend_error_cb),
				client_data_ref (client_data),
				(GClosureNotify) client_data_unref,
				0);
			client_data->backend_error_handler_id = handler_id;

			handler_id = g_signal_connect_data (
				client, "notify",
				G_CALLBACK (client_cache_notify_cb),
				client_data_ref (client_data),
				(GClosureNotify) client_data_unref,
				0);
			client_data->notify_handler_id = handler_id;

			g_signal_emit (client_cache, signals[CLIENT_CONNECTED], 0, client);

			signal_closure = g_slice_new0 (SignalClosure);
			signal_closure->client_cache =
				g_object_ref (client_cache);
			signal_closure->client = g_object_ref (client);

			idle_source = g_idle_source_new ();
			g_source_set_callback (
				idle_source,
				client_cache_emit_client_created_idle_cb,
				signal_closure,
				(GDestroyNotify) signal_closure_free);
			g_source_attach (
				idle_source, client_cache->priv->main_context);
			g_source_unref (idle_source);

			g_object_unref (client_cache);
		}
	}

	g_mutex_unlock (&client_data->lock);

	while (!g_queue_is_empty (&queue)) {
		ESimpleAsyncResult *simple;

		simple = g_queue_pop_head (&queue);
		if (client != NULL)
			e_simple_async_result_set_op_pointer (
				simple, g_object_ref (client),
				(GDestroyNotify) g_object_unref);
		if (error != NULL)
			e_simple_async_result_take_error (simple, g_error_copy (error));
		e_simple_async_result_complete_idle (simple);
		g_object_unref (simple);
	}
}

static void
client_cache_book_connect_cb (GObject *source_object,
                              GAsyncResult *result,
                              gpointer user_data)
{
	ClientData *client_data = user_data;
	EClient *client;
	GError *error = NULL;

	client = e_book_client_connect_finish (result, &error);

	client_cache_process_results (client_data, client, error);

	if (client != NULL)
		g_object_unref (client);

	if (error != NULL)
		g_error_free (error);

	client_data_unref (client_data);
}

static void
client_cache_cal_connect_cb (GObject *source_object,
                             GAsyncResult *result,
                             gpointer user_data)
{
	ClientData *client_data = user_data;
	EClient *client;
	GError *error = NULL;

	client = e_cal_client_connect_finish (result, &error);

	client_cache_process_results (client_data, client, error);

	if (client != NULL)
		g_object_unref (client);

	if (error != NULL)
		g_error_free (error);

	client_data_unref (client_data);
}

static void
client_cache_source_removed_cb (ESourceRegistry *registry,
                                ESource *source,
                                GWeakRef *weak_ref)
{
	EClientCache *client_cache;

	client_cache = g_weak_ref_get (weak_ref);

	if (client_cache != NULL) {
		client_ht_remove (client_cache, source);
		g_object_unref (client_cache);
	}
}

static void
client_cache_source_disabled_cb (ESourceRegistry *registry,
                                 ESource *source,
                                 GWeakRef *weak_ref)
{
	EClientCache *client_cache;

	client_cache = g_weak_ref_get (weak_ref);

	if (client_cache != NULL) {
		e_client_cache_emit_allow_auth_prompt (client_cache, source);

		client_ht_remove (client_cache, source);
		g_object_unref (client_cache);
	}
}

static void
client_cache_set_registry (EClientCache *client_cache,
                           ESourceRegistry *registry)
{
	g_return_if_fail (E_IS_SOURCE_REGISTRY (registry));
	g_return_if_fail (client_cache->priv->registry == NULL);

	client_cache->priv->registry = g_object_ref (registry);
}

static void
client_cache_set_property (GObject *object,
                           guint property_id,
                           const GValue *value,
                           GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_REGISTRY:
			client_cache_set_registry (
				E_CLIENT_CACHE (object),
				g_value_get_object (value));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
client_cache_get_property (GObject *object,
                           guint property_id,
                           GValue *value,
                           GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_REGISTRY:
			g_value_take_object (
				value,
				e_client_cache_ref_registry (
				E_CLIENT_CACHE (object)));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
client_cache_dispose (GObject *object)
{
	EClientCache *self = E_CLIENT_CACHE (object);

	if (self->priv->source_removed_handler_id > 0) {
		g_signal_handler_disconnect (
			self->priv->registry,
			self->priv->source_removed_handler_id);
		self->priv->source_removed_handler_id = 0;
	}

	if (self->priv->source_disabled_handler_id > 0) {
		g_signal_handler_disconnect (
			self->priv->registry,
			self->priv->source_disabled_handler_id);
		self->priv->source_disabled_handler_id = 0;
	}

	g_clear_object (&self->priv->registry);

	g_hash_table_remove_all (self->priv->client_ht);

	g_clear_pointer (&self->priv->main_context, g_main_context_unref);

	/* Chain up to parent's dispose() method. */
	G_OBJECT_CLASS (e_client_cache_parent_class)->dispose (object);
}

static void
client_cache_finalize (GObject *object)
{
	EClientCache *self = E_CLIENT_CACHE (object);

	g_hash_table_destroy (self->priv->client_ht);
	g_mutex_clear (&self->priv->client_ht_lock);

	/* Chain up to parent's finalize() method. */
	G_OBJECT_CLASS (e_client_cache_parent_class)->finalize (object);
}

static void
client_cache_constructed (GObject *object)
{
	EClientCache *client_cache;
	ESourceRegistry *registry;
	gulong handler_id;

	client_cache = E_CLIENT_CACHE (object);

	/* Chain up to parent's constructed() method. */
	G_OBJECT_CLASS (e_client_cache_parent_class)->constructed (object);

	registry = e_client_cache_ref_registry (client_cache);

	handler_id = g_signal_connect_data (
		registry, "source-removed",
		G_CALLBACK (client_cache_source_removed_cb),
		e_weak_ref_new (client_cache),
		(GClosureNotify) e_weak_ref_free, 0);
	client_cache->priv->source_removed_handler_id = handler_id;

	handler_id = g_signal_connect_data (
		registry, "source-disabled",
		G_CALLBACK (client_cache_source_disabled_cb),
		e_weak_ref_new (client_cache),
		(GClosureNotify) e_weak_ref_free, 0);
	client_cache->priv->source_disabled_handler_id = handler_id;

	g_object_unref (registry);

	e_extensible_load_extensions (E_EXTENSIBLE (object));
}

static void
e_client_cache_class_init (EClientCacheClass *class)
{
	GObjectClass *object_class;

	object_class = G_OBJECT_CLASS (class);
	object_class->set_property = client_cache_set_property;
	object_class->get_property = client_cache_get_property;
	object_class->dispose = client_cache_dispose;
	object_class->finalize = client_cache_finalize;
	object_class->constructed = client_cache_constructed;

	/**
	 * EClientCache:registry:
	 *
	 * The #ESourceRegistry manages #ESource instances.
	 **/
	g_object_class_install_property (
		object_class,
		PROP_REGISTRY,
		g_param_spec_object (
			"registry",
			"Registry",
			"Data source registry",
			E_TYPE_SOURCE_REGISTRY,
			G_PARAM_READWRITE |
			G_PARAM_CONSTRUCT_ONLY |
			G_PARAM_STATIC_STRINGS));

	/**
	 * EClientCache::backend-died:
	 * @client_cache: the #EClientCache that received the signal
	 * @client: the #EClient that received the D-Bus notification
	 * @alert: an #EAlert with a user-friendly error description
	 *
	 * Rebroadcasts an #EClient::backend-died signal emitted by @client,
	 * along with a pre-formatted #EAlert.
	 *
	 * As a convenience to signal handlers, this signal is always
	 * emitted from the #GMainContext that was thread-default when
	 * the @client_cache was created.
	 **/
	signals[BACKEND_DIED] = g_signal_new (
		"backend-died",
		G_TYPE_FROM_CLASS (class),
		G_SIGNAL_RUN_LAST,
		G_STRUCT_OFFSET (EClientCacheClass, backend_died),
		NULL, NULL, NULL,
		G_TYPE_NONE, 2,
		E_TYPE_CLIENT,
		E_TYPE_ALERT);

	/**
	 * EClientCache::backend-error:
	 * @client_cache: the #EClientCache that received the signal
	 * @client: the #EClient that received the D-Bus notification
	 * @alert: an #EAlert with a user-friendly error description
	 *
	 * Rebroadcasts an #EClient::backend-error signal emitted by @client,
	 * along with a pre-formatted #EAlert.
	 *
	 * As a convenience to signal handlers, this signal is always
	 * emitted from the #GMainContext that was thread-default when
	 * the @client_cache was created.
	 **/
	signals[BACKEND_ERROR] = g_signal_new (
		"backend-error",
		G_TYPE_FROM_CLASS (class),
		G_SIGNAL_RUN_LAST,
		G_STRUCT_OFFSET (EClientCacheClass, backend_error),
		NULL, NULL, NULL,
		G_TYPE_NONE, 2,
		E_TYPE_CLIENT,
		E_TYPE_ALERT);

	/**
	 * EClientCache::client-connected:
	 * @client_cache: the #EClientCache that received the signal
	 * @client: the newly-created #EClient
	 *
	 * This signal is emitted when a call to e_client_cache_get_client()
	 * triggers the creation of a new #EClient instance, immediately after
	 * the client's opening phase is over.
	 *
	 * See the difference with EClientCache::client-created, which is
	 * called on idle.
	 **/
	signals[CLIENT_CONNECTED] = g_signal_new (
		"client-connected",
		G_TYPE_FROM_CLASS (class),
		G_SIGNAL_RUN_FIRST,
		G_STRUCT_OFFSET (EClientCacheClass, client_connected),
		NULL, NULL, NULL,
		G_TYPE_NONE, 1,
		E_TYPE_CLIENT);

	/**
	 * EClientCache::client-created:
	 * @client_cache: the #EClientCache that received the signal
	 * @client: the newly-created #EClient
	 *
	 * This signal is emitted when a call to e_client_cache_get_client()
	 * triggers the creation of a new #EClient instance, invoked in an idle
	 * callback.
	 *
	 * See the difference with EClientCache::client-connected, which is
	 * called immediately.
	 **/
	signals[CLIENT_CREATED] = g_signal_new (
		"client-created",
		G_TYPE_FROM_CLASS (class),
		G_SIGNAL_RUN_FIRST,
		G_STRUCT_OFFSET (EClientCacheClass, client_created),
		NULL, NULL, NULL,
		G_TYPE_NONE, 1,
		E_TYPE_CLIENT);

	/**
	 * EClientCache::client-notify:
	 * @client_cache: the #EClientCache that received the signal
	 * @client: the #EClient whose property changed
	 * @pspec: the #GParamSpec of the property that changed
	 *
	 * Rebroadcasts a #GObject::notify signal emitted by @client.
	 *
	 * This signal supports "::detail" appendices to the signal name
	 * just like the #GObject::notify signal, so you can connect to
	 * change notification signals for specific #EClient properties.
	 *
	 * As a convenience to signal handlers, this signal is always
	 * emitted from the #GMainContext that was thread-default when
	 * the @client_cache was created.
	 **/
	signals[CLIENT_NOTIFY] = g_signal_new (
		"client-notify",
		G_TYPE_FROM_CLASS (class),
		/* same flags as GObject::notify */
		G_SIGNAL_RUN_FIRST |
		G_SIGNAL_NO_RECURSE |
		G_SIGNAL_DETAILED |
		G_SIGNAL_NO_HOOKS |
		G_SIGNAL_ACTION,
		G_STRUCT_OFFSET (EClientCacheClass, client_notify),
		NULL, NULL, NULL,
		G_TYPE_NONE, 2,
		E_TYPE_CLIENT,
		G_TYPE_PARAM);

	/**
	 * EClientCache::allow-auth-prompt:
	 * @client_cache: an #EClientCache, which sent the signal
	 * @source: an #ESource
	 *
	 * This signal is emitted with e_client_cache_emit_allow_auth_prompt() to let
	 * any listeners know to enable credentials prompt for the given @source.
	 *
	 * Since: 3.16
	 **/
	signals[ALLOW_AUTH_PROMPT] = g_signal_new (
		"allow-auth-prompt",
		G_TYPE_FROM_CLASS (class),
		G_SIGNAL_RUN_FIRST,
		G_STRUCT_OFFSET (EClientCacheClass, allow_auth_prompt),
		NULL, NULL, NULL,
		G_TYPE_NONE, 1,
		E_TYPE_SOURCE);
}

static void
e_client_cache_init (EClientCache *client_cache)
{
	GHashTable *client_ht;
	gint ii;

	const gchar *extension_names[] = {
		E_SOURCE_EXTENSION_ADDRESS_BOOK,
		E_SOURCE_EXTENSION_CALENDAR,
		E_SOURCE_EXTENSION_MEMO_LIST,
		E_SOURCE_EXTENSION_TASK_LIST
	};

	client_ht = g_hash_table_new_full (
		(GHashFunc) g_str_hash,
		(GEqualFunc) g_str_equal,
		(GDestroyNotify) g_free,
		(GDestroyNotify) g_hash_table_unref);

	client_cache->priv = e_client_cache_get_instance_private (client_cache);

	client_cache->priv->main_context = g_main_context_ref_thread_default ();
	client_cache->priv->client_ht = client_ht;

	g_mutex_init (&client_cache->priv->client_ht_lock);

	/* Pre-load the extension names that can be used to instantiate
	 * EClients.  Then we can validate an extension name by testing
	 * for a matching hash table key. */

	for (ii = 0; ii < G_N_ELEMENTS (extension_names); ii++) {
		GHashTable *inner_ht;

		inner_ht = g_hash_table_new_full (
			(GHashFunc) e_source_hash,
			(GEqualFunc) e_source_equal,
			(GDestroyNotify) g_object_unref,
			(GDestroyNotify) client_data_dispose);

		g_hash_table_insert (
			client_ht,
			g_strdup (extension_names[ii]),
			g_hash_table_ref (inner_ht));

		g_hash_table_unref (inner_ht);
	}
}

/**
 * e_client_cache_new:
 * @registry: an #ESourceRegistry
 *
 * Creates a new #EClientCache instance.
 *
 * Returns: an #EClientCache
 **/
EClientCache *
e_client_cache_new (ESourceRegistry *registry)
{
	g_return_val_if_fail (E_IS_SOURCE_REGISTRY (registry), NULL);

	return g_object_new (
		E_TYPE_CLIENT_CACHE,
		"registry", registry, NULL);
}

/**
 * e_client_cache_ref_registry:
 * @client_cache: an #EClientCache
 *
 * Returns the #ESourceRegistry passed to e_client_cache_new().
 *
 * The returned #ESourceRegistry is referenced for thread-safety and must be
 * unreferenced with g_object_unref() when finished with it.
 *
 * Returns: an #ESourceRegistry
 **/
ESourceRegistry *
e_client_cache_ref_registry (EClientCache *client_cache)
{
	g_return_val_if_fail (E_IS_CLIENT_CACHE (client_cache), NULL);

	return g_object_ref (client_cache->priv->registry);
}

/**
 * e_client_cache_get_client_sync:
 * @client_cache: an #EClientCache
 * @source: an #ESource
 * @extension_name: an extension name
 * @wait_for_connected_seconds: timeout, in seconds, to wait for the backend to be fully connected
 * @cancellable: optional #GCancellable object, or %NULL
 * @error: return location for a #GError, or %NULL
 *
 * Obtains a shared #EClient instance for @source, or else creates a new
 * #EClient instance to be shared.
 *
 * The @extension_name determines the type of #EClient to obtain.  Valid
 * @extension_name values are:
 *
 * #E_SOURCE_EXTENSION_ADDRESS_BOOK will obtain an #EBookClient.
 *
 * #E_SOURCE_EXTENSION_CALENDAR will obtain an #ECalClient with an
 * #ECalClient:source-type of #E_CAL_CLIENT_SOURCE_TYPE_EVENTS.
 *
 * #E_SOURCE_EXTENSION_MEMO_LIST will obtain an #ECalClient with an
 * #ECalClient:source-type of #E_CAL_CLIENT_SOURCE_TYPE_MEMOS.
 *
 * #E_SOURCE_EXTENSION_TASK_LIST will obtain an #ECalClient with an
 * #ECalClient:source-type of #E_CAL_CLIENT_SOURCE_TYPE_TASKS.
 *
 * The @source must already have an #ESourceExtension by that name
 * for this function to work.  All other @extension_name values will
 * result in an error.
 *
 * The @wait_for_connected_seconds argument had been added since 3.16,
 * to let the caller decide how long to wait for the backend to fully
 * connect to its (possibly remote) data store. This is required due
 * to a change in the authentication process, which is fully asynchronous
 * and done on the client side, while not every client is supposed to
 * response to authentication requests. In case the backend will not connect
 * within the set interval, then it is opened in an offline mode. A special
 * value -1 can be used to not wait for the connected state at all.
 *
 * If a request for the same @source and @extension_name is already in
 * progress when this function is called, this request will "piggyback"
 * on the in-progress request such that they will both succeed or fail
 * simultaneously.
 *
 * Unreference the returned #EClient with g_object_unref() when finished
 * with it.  If an error occurs, the function will set @error and return
 * %NULL.
 *
 * Returns: an #EClient, or %NULL
 **/
EClient *
e_client_cache_get_client_sync (EClientCache *client_cache,
                                ESource *source,
                                const gchar *extension_name,
				guint32 wait_for_connected_seconds,
                                GCancellable *cancellable,
                                GError **error)
{
	ClientData *client_data;
	EClient *client = NULL;
	GError *local_error = NULL;

	g_return_val_if_fail (E_IS_CLIENT_CACHE (client_cache), NULL);
	g_return_val_if_fail (E_IS_SOURCE (source), NULL);
	g_return_val_if_fail (extension_name != NULL, NULL);

	client_data = client_ht_lookup (client_cache, source, extension_name);

	if (client_data == NULL) {
		g_set_error (
			error, G_IO_ERROR,
			G_IO_ERROR_INVALID_ARGUMENT,
			_("Cannot create a client object from "
			"extension name “%s”"), extension_name);
		return NULL;
	}

	g_mutex_lock (&client_data->lock);

	if (client_data->client != NULL)
		client = g_object_ref (client_data->client);

	g_mutex_unlock (&client_data->lock);

	/* If a cached EClient already exists, we're done. */
	if (client != NULL) {
		client_data_unref (client_data);
		return client;
	}

	/* Create an appropriate EClient instance for the extension
	 * name.  The client_ht_lookup() call above ensures us that
	 * one of these options will match. */

	if (g_str_equal (extension_name, E_SOURCE_EXTENSION_ADDRESS_BOOK)) {
		client = e_book_client_connect_sync (source, wait_for_connected_seconds,
			cancellable, &local_error);
	} else if (g_str_equal (extension_name, E_SOURCE_EXTENSION_CALENDAR)) {
		client = e_cal_client_connect_sync (
			source, E_CAL_CLIENT_SOURCE_TYPE_EVENTS, wait_for_connected_seconds,
			cancellable, &local_error);
	} else if (g_str_equal (extension_name, E_SOURCE_EXTENSION_MEMO_LIST)) {
		client = e_cal_client_connect_sync (
			source, E_CAL_CLIENT_SOURCE_TYPE_MEMOS, wait_for_connected_seconds,
			cancellable, &local_error);
	} else if (g_str_equal (extension_name, E_SOURCE_EXTENSION_TASK_LIST)) {
		client = e_cal_client_connect_sync (
			source, E_CAL_CLIENT_SOURCE_TYPE_TASKS, wait_for_connected_seconds,
			cancellable, &local_error);
	} else {
		g_warn_if_reached ();  /* Should never happen. */
	}

	if (client)
		client_cache_process_results (client_data, client, local_error);

	if (local_error)
		g_propagate_error (error, local_error);

	client_data_unref (client_data);

	return client;
}

/**
 * e_client_cache_get_client:
 * @client_cache: an #EClientCache
 * @source: an #ESource
 * @extension_name: an extension name
 * @wait_for_connected_seconds: timeout, in seconds, to wait for the backend to be fully connected
 * @cancellable: optional #GCancellable object, or %NULL
 * @callback: a #GAsyncReadyCallback to call when the request is satisfied
 * @user_data: data to pass to the callback function
 *
 * Asynchronously obtains a shared #EClient instance for @source, or else
 * creates a new #EClient instance to be shared.
 *
 * The @extension_name determines the type of #EClient to obtain.  Valid
 * @extension_name values are:
 *
 * #E_SOURCE_EXTENSION_ADDRESS_BOOK will obtain an #EBookClient.
 *
 * #E_SOURCE_EXTENSION_CALENDAR will obtain an #ECalClient with a
 * #ECalClient:source-type of #E_CAL_CLIENT_SOURCE_TYPE_EVENTS.
 *
 * #E_SOURCE_EXTENSION_MEMO_LIST will obtain an #ECalClient with a
 * #ECalClient:source-type of #E_CAL_CLIENT_SOURCE_TYPE_MEMOS.
 *
 * #E_SOURCE_EXTENSION_TASK_LIST will obtain an #ECalClient with a
 * #ECalClient:source-type of #E_CAL_CLIENT_SOURCE_TYPE_TASKS.
 *
 * The @source must already have an #ESourceExtension by that name
 * for this function to work.  All other @extension_name values will
 * result in an error.
 *
 * The @wait_for_connected_seconds argument had been added since 3.16,
 * to let the caller decide how long to wait for the backend to fully
 * connect to its (possibly remote) data store. This is required due
 * to a change in the authentication process, which is fully asynchronous
 * and done on the client side, while not every client is supposed to
 * response to authentication requests. In case the backend will not connect
 * within the set interval, then it is opened in an offline mode. A special
 * value -1 can be used to not wait for the connected state at all.
 *
 * If a request for the same @source and @extension_name is already in
 * progress when this function is called, this request will "piggyback"
 * on the in-progress request such that they will both succeed or fail
 * simultaneously.
 *
 * When the operation is finished, @callback will be called.  You can
 * then call e_client_cache_get_client_finish() to get the result of the
 * operation.
 **/
void
e_client_cache_get_client (EClientCache *client_cache,
                           ESource *source,
                           const gchar *extension_name,
			   guint32 wait_for_connected_seconds,
                           GCancellable *cancellable,
                           GAsyncReadyCallback callback,
                           gpointer user_data)
{
	ESimpleAsyncResult *simple;
	ClientData *client_data;
	EClient *client = NULL;
	gboolean connect_in_progress = FALSE;

	g_return_if_fail (E_IS_CLIENT_CACHE (client_cache));
	g_return_if_fail (E_IS_SOURCE (source));
	g_return_if_fail (extension_name != NULL);

	simple = e_simple_async_result_new (
		G_OBJECT (client_cache), callback,
		user_data, e_client_cache_get_client);

	e_simple_async_result_set_check_cancellable (simple, cancellable);

	client_data = client_ht_lookup (client_cache, source, extension_name);

	if (client_data == NULL) {
		e_simple_async_result_take_error (
			simple, g_error_new (G_IO_ERROR,
			G_IO_ERROR_INVALID_ARGUMENT,
			_("Cannot create a client object from "
			"extension name “%s”"), extension_name));
		e_simple_async_result_complete_idle (simple);
		goto exit;
	}

	g_mutex_lock (&client_data->lock);

	if (client_data->client != NULL) {
		client = g_object_ref (client_data->client);
	} else {
		GQueue *connecting = &client_data->connecting;
		connect_in_progress = !g_queue_is_empty (connecting);
		g_queue_push_tail (connecting, g_object_ref (simple));
	}

	g_mutex_unlock (&client_data->lock);

	/* If a cached EClient already exists, we're done. */
	if (client != NULL) {
		e_simple_async_result_set_op_pointer (
			simple, client, (GDestroyNotify) g_object_unref);
		e_simple_async_result_complete_idle (simple);
		goto exit;
	}

	/* If an EClient connection attempt is already in progress, our
	 * cache request will complete when it finishes, so now we wait. */
	if (connect_in_progress)
		goto exit;

	/* Create an appropriate EClient instance for the extension
	 * name.  The client_ht_lookup() call above ensures us that
	 * one of these options will match. */

	if (g_str_equal (extension_name, E_SOURCE_EXTENSION_ADDRESS_BOOK)) {
		e_book_client_connect (
			source, wait_for_connected_seconds, cancellable,
			client_cache_book_connect_cb,
			client_data_ref (client_data));
		goto exit;
	}

	if (g_str_equal (extension_name, E_SOURCE_EXTENSION_CALENDAR)) {
		e_cal_client_connect (
			source, E_CAL_CLIENT_SOURCE_TYPE_EVENTS, wait_for_connected_seconds,
			cancellable, client_cache_cal_connect_cb,
			client_data_ref (client_data));
		goto exit;
	}

	if (g_str_equal (extension_name, E_SOURCE_EXTENSION_MEMO_LIST)) {
		e_cal_client_connect (
			source, E_CAL_CLIENT_SOURCE_TYPE_MEMOS, wait_for_connected_seconds,
			cancellable, client_cache_cal_connect_cb,
			client_data_ref (client_data));
		goto exit;
	}

	if (g_str_equal (extension_name, E_SOURCE_EXTENSION_TASK_LIST)) {
		e_cal_client_connect (
			source, E_CAL_CLIENT_SOURCE_TYPE_TASKS, wait_for_connected_seconds,
			cancellable, client_cache_cal_connect_cb,
			client_data_ref (client_data));
		goto exit;
	}

	g_warn_if_reached ();  /* Should never happen. */

exit:
	if (client_data)
		client_data_unref (client_data);
	g_object_unref (simple);
}

/**
 * e_client_cache_get_client_finish:
 * @client_cache: an #EClientCache
 * @result: a #GAsyncResult
 * @error: return location for a #GError, or %NULL
 *
 * Finishes the operation started with e_client_cache_get_client().
 *
 * Unreference the returned #EClient with g_object_unref() when finished
 * with it.  If an error occurred, the function will set @error and return
 * %NULL.
 *
 * Returns: an #EClient, or %NULL
 **/
EClient *
e_client_cache_get_client_finish (EClientCache *client_cache,
                                  GAsyncResult *result,
                                  GError **error)
{
	ESimpleAsyncResult *simple;
	EClient *client;

	g_return_val_if_fail (
		e_simple_async_result_is_valid (
		result, G_OBJECT (client_cache),
		e_client_cache_get_client), NULL);

	simple = E_SIMPLE_ASYNC_RESULT (result);

	if (e_simple_async_result_propagate_error (simple, error))
		return NULL;

	client = e_simple_async_result_get_op_pointer (simple);
	g_return_val_if_fail (client != NULL, NULL);

	return g_object_ref (client);
}

/**
 * e_client_cache_ref_cached_client:
 * @client_cache: an #EClientCache
 * @source: an #ESource
 * @extension_name: an extension name
 *
 * Returns a shared #EClient instance for @source and @extension_name if
 * such an instance is already cached, or else %NULL.  This function does
 * not create a new #EClient instance, and therefore does not block.
 *
 * See e_client_cache_get_client() for valid @extension_name values.
 *
 * The returned #EClient is referenced for thread-safety and must be
 * unreferenced with g_object_unref() when finished with it.
 *
 * Returns: an #EClient, or %NULL
 **/
EClient *
e_client_cache_ref_cached_client (EClientCache *client_cache,
                                  ESource *source,
                                  const gchar *extension_name)
{
	ClientData *client_data;
	EClient *client = NULL;

	g_return_val_if_fail (E_IS_CLIENT_CACHE (client_cache), NULL);
	g_return_val_if_fail (E_IS_SOURCE (source), NULL);
	g_return_val_if_fail (extension_name != NULL, NULL);

	client_data = client_ht_lookup (client_cache, source, extension_name);

	if (client_data != NULL) {
		g_mutex_lock (&client_data->lock);
		if (client_data->client != NULL)
			client = g_object_ref (client_data->client);
		g_mutex_unlock (&client_data->lock);

		client_data_unref (client_data);
	}

	return client;
}

static void
e_client_cache_append_clients (GSList **pclients,
			       GHashTable *inner_ht)
{
	GHashTableIter iter;
	gpointer value;

	g_hash_table_iter_init (&iter, inner_ht);

	while (g_hash_table_iter_next (&iter, NULL, &value)) {
		ClientData *client_data = value;

		if (client_data && client_data->client)
			*pclients = g_slist_prepend (*pclients, g_object_ref (client_data->client));
	}
}

/**
 * e_client_cache_list_cached_clients:
 * @client_cache: an #EClientCache
 * @extension_name: (nullable): an extension name the client should serve, or %NULL for all
 *
 * Lists currently cached clients for extension @extension_name, or all
 * cached clients, when the @extension_name is %NULL.
 *
 * Free the returned #GSList with g_slist_free_full (slist, g_object_unref);,
 * when no longer needed.
 *
 * Returns: (transfer full) (nullable) (element-type EClient): a newly allocated #GSList
 *    with currently cached clients, or %NULL, when none is cached
 *
 * Since: 3.48
 **/
GSList * /* EClient * */
e_client_cache_list_cached_clients (EClientCache *client_cache,
				    const gchar *extension_name)
{
	GSList *clients = NULL;
	GHashTable *client_ht;
	GHashTable *inner_ht;

	g_return_val_if_fail (E_IS_CLIENT_CACHE (client_cache), NULL);

	client_ht = client_cache->priv->client_ht;

	g_mutex_lock (&client_cache->priv->client_ht_lock);

	if (extension_name) {
		inner_ht = g_hash_table_lookup (client_ht, extension_name);
		if (inner_ht)
			e_client_cache_append_clients (&clients, inner_ht);
	} else {
		GHashTableIter iter;
		gpointer value;

		g_hash_table_iter_init (&iter, client_ht);

		while (g_hash_table_iter_next (&iter, NULL, &value)) {
			inner_ht = value;
			if (inner_ht)
				e_client_cache_append_clients (&clients, inner_ht);
		}
	}

	g_mutex_unlock (&client_cache->priv->client_ht_lock);

	return clients;
}

/**
 * e_client_cache_is_backend_dead:
 * @client_cache: an #EClientCache
 * @source: an #ESource
 * @extension_name: an extension name
 *
 * Returns %TRUE if an #EClient instance for @source and @extension_name
 * was recently discarded after having emitted an #EClient::backend-died
 * signal, and a replacement #EClient instance has not yet been created.
 *
 * Returns: whether the backend for @source and @extension_name died
 **/
gboolean
e_client_cache_is_backend_dead (EClientCache *client_cache,
                                ESource *source,
                                const gchar *extension_name)
{
	ClientData *client_data;
	gboolean dead_backend = FALSE;

	g_return_val_if_fail (E_IS_CLIENT_CACHE (client_cache), FALSE);
	g_return_val_if_fail (E_IS_SOURCE (source), FALSE);
	g_return_val_if_fail (extension_name != NULL, FALSE);

	client_data = client_ht_lookup (client_cache, source, extension_name);

	if (client_data != NULL) {
		dead_backend = client_data->dead_backend;
		client_data_unref (client_data);
	}

	return dead_backend;
}

/**
 * e_client_cache_emit_allow_auth_prompt:
 * @client_cache: an #EClientCache
 * @source: an #ESource
 *
 * Emits 'allow-auth-prompt' on @client_cache for @source. This lets
 * any listeners know to enable credentials prompt for this @source.
 *
 * Since: 3.16
 **/
void
e_client_cache_emit_allow_auth_prompt (EClientCache *client_cache,
				       ESource *source)
{
	g_return_if_fail (E_IS_CLIENT_CACHE (client_cache));
	g_return_if_fail (E_IS_SOURCE (source));

	g_signal_emit (client_cache, signals[ALLOW_AUTH_PROMPT], 0, source);
}
