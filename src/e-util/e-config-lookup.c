/*
 * Copyright (C) 2017 Red Hat, Inc. (www.redhat.com)
 *
 * This library is free software: you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE. See the GNU Lesser General Public License
 * for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

/**
 * SECTION: e-config-lookup
 * @include: e-util/e-util.h
 * @short_description: Configuration lookup
 *
 * #EConfigLookup is used to search for configuration of an account,
 * which is identified by an e-mail address, server address or such.
 * It is an #EExtensible object, where the extensions connect to
 * the #EConfigLookup::run signal to run the configuration lookup.
 **/

#include "evolution-config.h"

#include <glib/gi18n-lib.h>
#include <camel/camel.h>
#include <libedataserver/libedataserver.h>

#include "e-config-lookup-result.h"
#include "e-config-lookup-worker.h"
#include "e-simple-async-result.h"
#include "e-util-enumtypes.h"

#include "e-config-lookup.h"

struct _EConfigLookupPrivate {
	ESourceRegistry *registry;

	GMutex property_lock;
	GSList *workers; /* EConfigLookupWorker * */
	GSList *results; /* EConfigLookupResult * */

	ESimpleAsyncResult *run_result;
	GCancellable *run_cancellable;
	GSList *worker_cancellables; /* CamelOperation * */

	GThreadPool *pool;
};

enum {
	PROP_0,
	PROP_REGISTRY,
	PROP_BUSY
};

enum {
	GET_SOURCE,
	WORKER_STARTED,
	WORKER_FINISHED,
	RESULT_ADDED,
	LAST_SIGNAL
};

static guint signals[LAST_SIGNAL];

G_DEFINE_TYPE_WITH_CODE (EConfigLookup, e_config_lookup, G_TYPE_OBJECT,
	G_ADD_PRIVATE (EConfigLookup)
	G_IMPLEMENT_INTERFACE (E_TYPE_EXTENSIBLE, NULL))

enum {
	EMIT_BUSY		= 1 << 0,
	EMIT_WORKER_STARTED	= 1 << 1,
	EMIT_WORKER_FINISHED	= 1 << 2
};

typedef struct _EmitData {
	EConfigLookup *config_lookup;
	EConfigLookupWorker *worker;
	guint32 flags;
	GCancellable *cancellable;
	ENamedParameters *params;
	GError *error;
} EmitData;

static void
emit_data_free (gpointer ptr)
{
	EmitData *ed = ptr;

	if (ed) {
		e_named_parameters_free (ed->params);
		g_clear_object (&ed->config_lookup);
		g_clear_object (&ed->worker);
		g_clear_object (&ed->cancellable);
		g_clear_error (&ed->error);
		g_slice_free (EmitData, ed);
	}
}

static gboolean
config_lookup_emit_idle_cb (gpointer user_data)
{
	EmitData *ed = user_data;

	g_return_val_if_fail (ed != NULL, FALSE);
	g_return_val_if_fail (E_IS_CONFIG_LOOKUP (ed->config_lookup), FALSE);

	if ((ed->flags & EMIT_WORKER_STARTED) != 0)
		g_signal_emit (ed->config_lookup, signals[WORKER_STARTED], 0, ed->worker, ed->cancellable);

	if ((ed->flags & EMIT_WORKER_FINISHED) != 0)
		g_signal_emit (ed->config_lookup, signals[WORKER_FINISHED], 0, ed->worker, ed->params, ed->error);

	if ((ed->flags & EMIT_BUSY) != 0)
		g_object_notify (G_OBJECT (ed->config_lookup), "busy");

	return FALSE;
}

static void
config_lookup_schedule_emit_idle (EConfigLookup *config_lookup,
				  guint32 emit_flags,
				  EConfigLookupWorker *worker,
				  GCancellable *cancellable,
				  const ENamedParameters *params,
				  const GError *error)
{
	EmitData *ed;

	g_return_if_fail (E_IS_CONFIG_LOOKUP (config_lookup));
	if (worker)
		g_return_if_fail (E_IS_CONFIG_LOOKUP_WORKER (worker));

	ed = g_slice_new0 (EmitData);
	ed->config_lookup = g_object_ref (config_lookup);
	ed->flags = emit_flags;
	ed->worker = worker ? g_object_ref (worker) : NULL;
	ed->cancellable = cancellable ? g_object_ref (cancellable) : NULL;
	ed->params = params ? e_named_parameters_new_clone (params) : NULL;
	ed->error = error ? g_error_copy (error) : NULL;

	g_idle_add_full (G_PRIORITY_HIGH_IDLE, config_lookup_emit_idle_cb, ed, emit_data_free);
}

typedef struct _ThreadData {
	ENamedParameters *params;
	EConfigLookupWorker *worker;
	GCancellable *cancellable;
} ThreadData;

static void
config_lookup_thread (gpointer data,
		      gpointer user_data)
{
	ThreadData *td = data;
	EConfigLookup *config_lookup = user_data;
	ESimpleAsyncResult *run_result = NULL;
	guint32 emit_flags;
	ENamedParameters *restart_params = NULL;
	GError *error = NULL;

	g_return_if_fail (td != NULL);
	g_return_if_fail (td->params != NULL);
	g_return_if_fail (E_IS_CONFIG_LOOKUP_WORKER (td->worker));
	g_return_if_fail (G_IS_CANCELLABLE (td->cancellable));
	g_return_if_fail (E_IS_CONFIG_LOOKUP (config_lookup));

	e_config_lookup_worker_run (td->worker, config_lookup, td->params, &restart_params, td->cancellable, &error);

	g_mutex_lock (&config_lookup->priv->property_lock);

	emit_flags = EMIT_WORKER_FINISHED;

	if (g_slist_find (config_lookup->priv->worker_cancellables, td->cancellable)) {
		config_lookup->priv->worker_cancellables = g_slist_remove (config_lookup->priv->worker_cancellables, td->cancellable);
		g_object_unref (td->cancellable);

		if (!config_lookup->priv->worker_cancellables)
			emit_flags |= EMIT_BUSY;
	}

	config_lookup_schedule_emit_idle (config_lookup, emit_flags, td->worker, NULL, restart_params, error);

	if ((emit_flags & EMIT_BUSY) != 0) {
		run_result = config_lookup->priv->run_result;
		config_lookup->priv->run_result = NULL;

		g_clear_object (&config_lookup->priv->run_cancellable);
	}

	g_mutex_unlock (&config_lookup->priv->property_lock);

	if (run_result) {
		e_simple_async_result_complete_idle_take (run_result);
	}

	e_named_parameters_free (restart_params);
	e_named_parameters_free (td->params);
	g_clear_object (&td->worker);
	g_clear_object (&td->cancellable);
	g_clear_error (&error);
	g_slice_free (ThreadData, td);
}

static void
config_lookup_set_registry (EConfigLookup *config_lookup,
			    ESourceRegistry *registry)
{
	g_return_if_fail (E_IS_SOURCE_REGISTRY (registry));
	g_return_if_fail (config_lookup->priv->registry == NULL);

	config_lookup->priv->registry = g_object_ref (registry);
}

static void
config_lookup_set_property (GObject *object,
			    guint property_id,
			    const GValue *value,
			    GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_REGISTRY:
			config_lookup_set_registry (
				E_CONFIG_LOOKUP (object),
				g_value_get_object (value));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
config_lookup_get_property (GObject *object,
			    guint property_id,
			    GValue *value,
			    GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_BUSY:
			g_value_set_boolean (
				value,
				e_config_lookup_get_busy (
				E_CONFIG_LOOKUP (object)));
			return;

		case PROP_REGISTRY:
			g_value_set_object (
				value,
				e_config_lookup_get_registry (
				E_CONFIG_LOOKUP (object)));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
config_lookup_constructed (GObject *object)
{
	/* Chain up to parent's method. */
	G_OBJECT_CLASS (e_config_lookup_parent_class)->constructed (object);

	e_extensible_load_extensions (E_EXTENSIBLE (object));
}

static void
config_lookup_dispose (GObject *object)
{
	EConfigLookup *config_lookup = E_CONFIG_LOOKUP (object);
	gboolean had_running_workers;

	e_config_lookup_cancel_all (config_lookup);

	if (config_lookup->priv->pool) {
		g_thread_pool_free (config_lookup->priv->pool, TRUE, TRUE);
		config_lookup->priv->pool = NULL;
	}

	g_mutex_lock (&config_lookup->priv->property_lock);

	g_clear_object (&config_lookup->priv->run_cancellable);

	g_slist_free_full (config_lookup->priv->workers, g_object_unref);
	config_lookup->priv->workers = NULL;

	had_running_workers = config_lookup->priv->worker_cancellables != NULL;
	g_slist_free_full (config_lookup->priv->worker_cancellables, g_object_unref);
	config_lookup->priv->worker_cancellables = NULL;

	g_mutex_unlock (&config_lookup->priv->property_lock);

	if (had_running_workers)
		g_object_notify (object, "busy");

	g_clear_object (&config_lookup->priv->registry);

	/* Chain up to parent's method. */
	G_OBJECT_CLASS (e_config_lookup_parent_class)->dispose (object);
}

static void
config_lookup_finalize (GObject *object)
{
	EConfigLookup *config_lookup = E_CONFIG_LOOKUP (object);

	g_slist_free_full (config_lookup->priv->results, g_object_unref);
	g_mutex_clear (&config_lookup->priv->property_lock);

	/* Chain up to parent's method. */
	G_OBJECT_CLASS (e_config_lookup_parent_class)->finalize (object);
}

static void
e_config_lookup_class_init (EConfigLookupClass *klass)
{
	GObjectClass *object_class;

	object_class = G_OBJECT_CLASS (klass);
	object_class->set_property = config_lookup_set_property;
	object_class->get_property = config_lookup_get_property;
	object_class->constructed = config_lookup_constructed;
	object_class->dispose = config_lookup_dispose;
	object_class->finalize = config_lookup_finalize;

	/**
	 * EConfigLookup:registry:
	 *
	 * The #ESourceRegistry manages #ESource instances.
	 *
	 * Since: 3.26
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
	 * EConfigLookup:busy:
	 *
	 * Whether the EConfigLookup has any running workers.
	 *
	 * Since: 3.28
	 **/
	g_object_class_install_property (
		object_class,
		PROP_BUSY,
		g_param_spec_boolean (
			"busy",
			"Busy",
			NULL,
			FALSE,
			G_PARAM_READABLE |
			G_PARAM_STATIC_STRINGS));

	/**
	 * EConfigLookup::get-source:
	 * @kind: an #EConfigLookupSourceKind
	 *
	 * Emitted to get an #ESource of the given @kind. Return %NULL, when not available.
	 *
	 * Since: 3.26
	 **/
	signals[GET_SOURCE] = g_signal_new (
		"get-source",
		G_TYPE_FROM_CLASS (klass),
		G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
		G_STRUCT_OFFSET (EConfigLookupClass, get_source),
		NULL, NULL,
		NULL,
		G_TYPE_POINTER, 1,
		E_TYPE_CONFIG_LOOKUP_SOURCE_KIND);

	/**
	 * EConfigLookup::worker-started:
	 * @worker: an #EConfigLookupWorker
	 * @cancellable: associated #GCancellable for this worker run
	 *
	 * Emitted when the @worker is about to start running.
	 * Corresponding @EConfigLookup::worker-finished is emitted when
	 * the run is finished.
	 *
	 * Note that this signal is always emitted in the main thread.
	 *
	 * Since: 3.28
	 **/
	signals[WORKER_STARTED] = g_signal_new (
		"worker-started",
		G_TYPE_FROM_CLASS (klass),
		G_SIGNAL_RUN_LAST,
		G_STRUCT_OFFSET (EConfigLookupClass, worker_started),
		NULL, NULL,
		NULL,
		G_TYPE_NONE, 2,
		E_TYPE_CONFIG_LOOKUP_WORKER,
		G_TYPE_CANCELLABLE);

	/**
	 * EConfigLookup::worker-finished:
	 * @worker: an #EConfigLookupWorker
	 * @restart_params: an optional #ENamedParameters to use when the @worker might be restarted
	 * @error: an optional #GError with an overall result of the run
	 *
	 * Emitted when the @worker finished its running.
	 *
	 * Note that this signal is always emitted in the main thread.
	 *
	 * Since: 3.28
	 **/
	signals[WORKER_FINISHED] = g_signal_new (
		"worker-finished",
		G_TYPE_FROM_CLASS (klass),
		G_SIGNAL_RUN_LAST,
		G_STRUCT_OFFSET (EConfigLookupClass, worker_finished),
		NULL, NULL,
		NULL,
		G_TYPE_NONE, 3,
		E_TYPE_CONFIG_LOOKUP_WORKER,
		E_TYPE_NAMED_PARAMETERS,
		G_TYPE_ERROR);

	/**
	 * EConfigLookup::result-added:
	 * @result: an #EConfigLookupResult
	 *
	 * Emitted when a new @result is added to the config lookup.
	 *
	 * Note that this signal can be emitted in a worker's dedicated thread.
	 *
	 * Since: 3.28
	 **/
	signals[RESULT_ADDED] = g_signal_new (
		"result-added",
		G_TYPE_FROM_CLASS (klass),
		G_SIGNAL_RUN_LAST,
		G_STRUCT_OFFSET (EConfigLookupClass, result_added),
		NULL, NULL,
		NULL,
		G_TYPE_NONE, 1,
		E_TYPE_CONFIG_LOOKUP_RESULT);
}

static void
e_config_lookup_init (EConfigLookup *config_lookup)
{
	config_lookup->priv = e_config_lookup_get_instance_private (config_lookup);

	g_mutex_init (&config_lookup->priv->property_lock);
	config_lookup->priv->pool = g_thread_pool_new (config_lookup_thread, config_lookup, 10, FALSE, NULL);
}

/**
 * e_config_lookup_new:
 * @registry: an #ESourceRegistry
 *
 * Creates a new #EConfigLookup instance.
 *
 * Returns: (transfer full): a new #EConfigLookup
 *
 * Since: 3.26
 **/
EConfigLookup *
e_config_lookup_new (ESourceRegistry *registry)
{
	g_return_val_if_fail (E_IS_SOURCE_REGISTRY (registry), NULL);

	return g_object_new (E_TYPE_CONFIG_LOOKUP,
		"registry", registry,
		NULL);
}

/**
 * e_config_lookup_get_registry:
 * @config_lookup: an #EConfigLookup
 *
 * Returns the #ESourceRegistry passed to e_config_lookup_new().
 *
 * Returns: (transfer none): an #ESourceRegistry
 *
 * Since: 3.26
 **/
ESourceRegistry *
e_config_lookup_get_registry (EConfigLookup *config_lookup)
{
	g_return_val_if_fail (E_IS_CONFIG_LOOKUP (config_lookup), NULL);

	return config_lookup->priv->registry;
}

/**
 * e_config_lookup_get_source:
 * @config_lookup: an #EConfigLookup
 * @kind: one of #EConfigLookupSourceKind, except of the %E_CONFIG_LOOKUP_SOURCE_UNKNOWN
 *
 * Emits the #EConfigLookup::get-source signal and any listener can provide
 * the source. The function can return %NULL, when there are no listeners
 * or when such source is not available.
 *
 * Returns: (transfer none) (nullable): an #ESource of the given @kind, or %NULL, if not found
 *
 * Since: 3.26
 **/
ESource *
e_config_lookup_get_source (EConfigLookup *config_lookup,
			    EConfigLookupSourceKind kind)
{
	ESource *source = NULL;

	g_return_val_if_fail (E_IS_CONFIG_LOOKUP (config_lookup), NULL);

	g_signal_emit (config_lookup, signals[GET_SOURCE], 0, kind, &source);

	return source;
}

/**
 * e_config_lookup_get_busy:
 * @config_lookup: an #EConfigLookup
 *
 * Returns whether there's any running worker. They can be cancelled
 * with e_config_lookup_cancel_all().
 *
 * Returns: whether there's any running worker
 *
 * Since: 3.28
 **/
gboolean
e_config_lookup_get_busy (EConfigLookup *config_lookup)
{
	gboolean busy;

	g_return_val_if_fail (E_IS_CONFIG_LOOKUP (config_lookup), FALSE);

	g_mutex_lock (&config_lookup->priv->property_lock);
	busy = config_lookup->priv->worker_cancellables != NULL;
	g_mutex_unlock (&config_lookup->priv->property_lock);

	return busy;
}

/**
 * e_config_lookup_cancel_all:
 * @config_lookup: an #EConfigLookup
 *
 * Cancels all pending workers.
 *
 * Since: 3.28
 **/
void
e_config_lookup_cancel_all (EConfigLookup *config_lookup)
{
	GSList *cancellables;
	GCancellable *run_cancellable;

	g_return_if_fail (E_IS_CONFIG_LOOKUP (config_lookup));

	g_mutex_lock (&config_lookup->priv->property_lock);
	cancellables = g_slist_copy_deep (config_lookup->priv->worker_cancellables, (GCopyFunc) g_object_ref, NULL);
	run_cancellable = config_lookup->priv->run_cancellable ? g_object_ref (config_lookup->priv->run_cancellable) : NULL;
	g_mutex_unlock (&config_lookup->priv->property_lock);

	g_slist_foreach (cancellables, (GFunc) g_cancellable_cancel, NULL);
	g_slist_free_full (cancellables, g_object_unref);

	if (run_cancellable) {
		g_cancellable_cancel (run_cancellable);
		g_object_unref (run_cancellable);
	}
}

/**
 * e_config_lookup_register_worker:
 * @config_lookup: an #EConfigLookup
 * @worker: an #EConfigLookupWorker
 *
 * Registers a @worker as a worker, which can be run as part of e_config_lookup_run().
 * The function adds its own reference to @worker.
 *
 * Since: 3.28
 **/
void
e_config_lookup_register_worker (EConfigLookup *config_lookup,
				 EConfigLookupWorker *worker)
{
	GSList *existing_worker;

	g_return_if_fail (E_IS_CONFIG_LOOKUP (config_lookup));
	g_return_if_fail (E_IS_CONFIG_LOOKUP_WORKER (worker));

	g_mutex_lock (&config_lookup->priv->property_lock);

	existing_worker = g_slist_find (config_lookup->priv->workers, worker);

	g_warn_if_fail (existing_worker == NULL);

	if (!existing_worker)
		config_lookup->priv->workers = g_slist_prepend (config_lookup->priv->workers, g_object_ref (worker));

	g_mutex_unlock (&config_lookup->priv->property_lock);
}

/**
 * e_config_lookup_unregister_worker:
 * @config_lookup: an #EConfigLookup
 * @worker: an #EConfigLookupWorker
 *
 * Removes a @worker previously registered with e_config_lookup_register_worker().
 *
 * Since: 3.28
 **/
void
e_config_lookup_unregister_worker (EConfigLookup *config_lookup,
				   EConfigLookupWorker *worker)
{
	GSList *existing_worker;

	g_return_if_fail (E_IS_CONFIG_LOOKUP (config_lookup));
	g_return_if_fail (E_IS_CONFIG_LOOKUP_WORKER (worker));

	g_mutex_lock (&config_lookup->priv->property_lock);

	existing_worker = g_slist_find (config_lookup->priv->workers, worker);

	g_warn_if_fail (existing_worker != NULL);

	if (existing_worker) {
		config_lookup->priv->workers = g_slist_remove (config_lookup->priv->workers, worker);
		g_object_unref (worker);
	}

	g_mutex_unlock (&config_lookup->priv->property_lock);
}

/**
 * e_config_lookup_dup_registered_workers:
 * @config_lookup: an #EConfigLookup
 *
 * Returns a list of all registered #EConfigLookupWorker objects.
 *
 * The returned #GSList should be freed with
 * g_slist_free_full (workers, g_object_unref);
 * when no longer needed.
 *
 * Returns: (transfer full) (element-type EConfigLookupWorker): a #GSList with all
 *    workers registered with e_config_lookup_register_worker().
 *
 * Since: 3.28
 **/
GSList *
e_config_lookup_dup_registered_workers (EConfigLookup *config_lookup)
{
	GSList *workers;

	g_return_val_if_fail (E_IS_CONFIG_LOOKUP (config_lookup), NULL);

	g_mutex_lock (&config_lookup->priv->property_lock);
	workers = g_slist_copy_deep (config_lookup->priv->workers, (GCopyFunc) g_object_ref, NULL);
	g_mutex_unlock (&config_lookup->priv->property_lock);

	return workers;
}

/**
 * e_config_lookup_run:
 * @config_lookup: an #EConfigLookup
 * @params: an #ENamedParameters with lookup parameters
 * @cancellable: an optional #GCancellable, or %NULL
 * @callback: a callback to call, when the run is finished
 * @user_data: user data for the @callback
 *
 * Runs configuration lookup asynchronously. Once the run is done, the @callback is called,
 * and the call can be finished with e_config_lookup_run_finish(). The @callback is always
 * called from the main thread.
 *
 * Workers can be run individually using e_config_lookup_run_worker().
 *
 * Note that there cannot be run two lookups at the same time, thus if it
 * happens, then the @callback is called immediately with a %NULL result.
 *
 * Since: 3.26
 **/
void
e_config_lookup_run (EConfigLookup *config_lookup,
		     const ENamedParameters *params,
		     GCancellable *cancellable,
		     GAsyncReadyCallback callback,
		     gpointer user_data)
{
	GSList *workers, *link;

	g_return_if_fail (E_IS_CONFIG_LOOKUP (config_lookup));
	g_return_if_fail (params != NULL);

	g_mutex_lock (&config_lookup->priv->property_lock);

	if (config_lookup->priv->run_result) {
		g_mutex_unlock (&config_lookup->priv->property_lock);

		if (callback)
			callback (G_OBJECT (config_lookup), NULL, user_data);
		return;
	}

	g_slist_free_full (config_lookup->priv->results, g_object_unref);
	config_lookup->priv->results = NULL;

	if (cancellable)
		g_object_ref (cancellable);
	else
		cancellable = g_cancellable_new ();

	config_lookup->priv->run_result = e_simple_async_result_new (G_OBJECT (config_lookup), callback, user_data, e_config_lookup_run);
	config_lookup->priv->run_cancellable = cancellable;

	workers = g_slist_copy_deep (config_lookup->priv->workers, (GCopyFunc) g_object_ref, NULL);

	g_mutex_unlock (&config_lookup->priv->property_lock);

	if (workers) {
		for (link = workers; link; link = g_slist_next (link)) {
			EConfigLookupWorker *worker = link->data;

			e_config_lookup_run_worker (config_lookup, worker, params, cancellable);
		}

		g_slist_free_full (workers, g_object_unref);
	} else {
		ESimpleAsyncResult *run_result;

		g_mutex_lock (&config_lookup->priv->property_lock);

		run_result = config_lookup->priv->run_result;
		config_lookup->priv->run_result = NULL;

		g_clear_object (&config_lookup->priv->run_cancellable);

		g_mutex_unlock (&config_lookup->priv->property_lock);

		if (run_result) {
			e_simple_async_result_complete_idle_take (run_result);
		}
	}
}

/**
 * e_config_lookup_run_finish:
 * @config_lookup: an #EConfigLookup
 * @result: result of the operation
 *
 * Finishes the configuration lookup previously run by e_config_lookup_run().
 * It's expected that the extensions may fail, thus it doesn't return
 * anything and is provided mainly for consistency with asynchronous API.
 *
 * Since: 3.26
 **/
void
e_config_lookup_run_finish (EConfigLookup *config_lookup,
			    GAsyncResult *result)
{
	g_return_if_fail (E_IS_CONFIG_LOOKUP (config_lookup));
	g_return_if_fail (G_IS_ASYNC_RESULT (result));
	g_return_if_fail (g_async_result_is_tagged (result, e_config_lookup_run));
}

/**
 * e_config_lookup_run_worker:
 * @config_lookup: an #EConfigLookup
 * @worker: an #EConfigLookupWorker to run in a dedicated thread
 * @params: an #ENamedParameters with lookup parameters
 * @cancellable: an optional #GCancellable, or %NULL
 *
 * Creates a new thread and runs @worker in it. When the @cancellable is %NULL,
 * then there's creates a new #CamelOperation, which either proxies currently
 * running lookup or the newly created cancellable is completely independent.
 *
 * This function can be called while there's an ongoing configuration lookup, but
 * also when the @worker is restarted.
 *
 * Since: 3.28
 **/
void
e_config_lookup_run_worker (EConfigLookup *config_lookup,
			    EConfigLookupWorker *worker,
			    const ENamedParameters *params,
			    GCancellable *cancellable)
{
	ThreadData *td;

	g_return_if_fail (E_IS_CONFIG_LOOKUP (config_lookup));
	g_return_if_fail (E_IS_CONFIG_LOOKUP_WORKER (worker));
	g_return_if_fail (params != NULL);

	td = g_slice_new0 (ThreadData);
	td->params = e_named_parameters_new_clone (params);
	td->worker = g_object_ref (worker);

	g_mutex_lock (&config_lookup->priv->property_lock);

	if (cancellable)
		td->cancellable = camel_operation_new_proxy (cancellable);
	else if (config_lookup->priv->run_cancellable)
		td->cancellable = camel_operation_new_proxy (config_lookup->priv->run_cancellable);
	else
		td->cancellable = camel_operation_new ();

	camel_operation_push_message (td->cancellable, "%s", _("Running…"));
	config_lookup->priv->worker_cancellables = g_slist_prepend (config_lookup->priv->worker_cancellables, g_object_ref (td->cancellable));

	config_lookup_schedule_emit_idle (config_lookup, EMIT_WORKER_STARTED |
		(!config_lookup->priv->worker_cancellables->next ? EMIT_BUSY : 0),
		worker, td->cancellable, NULL, NULL);

	g_thread_pool_push (config_lookup->priv->pool, td, NULL);

	g_mutex_unlock (&config_lookup->priv->property_lock);
}

/**
 * e_config_lookup_add_result:
 * @config_lookup: an #EConfigLookup
 * @result: (transfer full): an #EConfigLookupResult
 *
 * Adds a new @result in a list of known configuration lookup results.
 * The @config_lookup assumes ownership of the @result and frees it
 * when no longer needed.
 *
 * The list of results can be obtained with e_config_lookup_dup_results().
 *
 * Since: 3.26
 **/
void
e_config_lookup_add_result (EConfigLookup *config_lookup,
			    EConfigLookupResult *result)
{
	g_return_if_fail (E_IS_CONFIG_LOOKUP (config_lookup));
	g_return_if_fail (E_IS_CONFIG_LOOKUP_RESULT (result));

	g_mutex_lock (&config_lookup->priv->property_lock);

	config_lookup->priv->results = g_slist_prepend (config_lookup->priv->results, result);

	g_mutex_unlock (&config_lookup->priv->property_lock);

	g_signal_emit (config_lookup, signals[RESULT_ADDED], 0, result);
}

/**
 * e_config_lookup_count_results:
 * @config_lookup: an #EConfigLookup
 *
 * Returns: how many results had been added already.
 *
 * Since: 3.28
 **/
gint
e_config_lookup_count_results (EConfigLookup *config_lookup)
{
	gint n_results;

	g_return_val_if_fail (E_IS_CONFIG_LOOKUP (config_lookup), -1);

	g_mutex_lock (&config_lookup->priv->property_lock);

	n_results = g_slist_length (config_lookup->priv->results);

	g_mutex_unlock (&config_lookup->priv->property_lock);

	return n_results;
}

/**
 * e_config_lookup_dup_results:
 * @config_lookup: an #EConfigLookup
 * @kind: an #EConfigLookupResultKind to filter the results with
 * @protocol: (nullable): optional protocol to filter the results with, or %NULL
 *
 * Returns a #GSList with #EConfigLookupResult objects satisfying
 * the @kind and @protocol filtering conditions. To receive all
 * gathered results use %E_CONFIG_LOOKUP_RESULT_UNKNOWN for @kind
 * and %NULL for the @protocol.
 *
 * Free the returned #GSList with
 * g_slist_free_full (results, g_object_unref);
 * when no longer needed.
 *
 * Returns: (transfer full) (element-type EConfigLookupResult): a #GSList
 *    with results satisfying the @kind and @protocol filtering conditions.
 *
 * Since: 3.28
 **/
GSList *
e_config_lookup_dup_results (EConfigLookup *config_lookup,
			     EConfigLookupResultKind kind,
			     const gchar *protocol)
{
	GSList *results = NULL, *link;

	g_return_val_if_fail (E_IS_CONFIG_LOOKUP (config_lookup), NULL);

	g_mutex_lock (&config_lookup->priv->property_lock);

	for (link = config_lookup->priv->results; link; link = g_slist_next (link)) {
		EConfigLookupResult *result = link->data;

		if (!E_IS_CONFIG_LOOKUP_RESULT (result))
			continue;

		if (kind != E_CONFIG_LOOKUP_RESULT_UNKNOWN &&
		    kind != e_config_lookup_result_get_kind (result))
			continue;

		if (protocol &&
		    g_strcmp0 (protocol, e_config_lookup_result_get_protocol (result)) != 0)
			continue;

		results = g_slist_prepend (results, g_object_ref (result));
	}

	g_mutex_unlock (&config_lookup->priv->property_lock);

	return results;
}

/**
 * e_config_lookup_clear_results:
 * @config_lookup: an #EConfigLookup
 *
 * Frees all gathered results. This might be usually called before
 * starting new custom lookup. The e_config_lookup_run() frees
 * all results automatically.
 *
 * Since: 3.28
 **/
void
e_config_lookup_clear_results (EConfigLookup *config_lookup)
{
	g_return_if_fail (E_IS_CONFIG_LOOKUP (config_lookup));

	g_mutex_lock (&config_lookup->priv->property_lock);

	g_slist_free_full (config_lookup->priv->results, g_object_unref);
	config_lookup->priv->results = NULL;

	g_mutex_unlock (&config_lookup->priv->property_lock);
}

/**
 * e_config_lookup_encode_certificate_trust:
 * @response: an #ETrustPromptResponse to encode
 *
 * Encodes @response to a string. This can be decoded back to enum
 * with e_config_lookup_decode_certificate_trust().
 *
 * Returns: string representation of @response.
 *
 * Since: 3.28
 **/
const gchar *
e_config_lookup_encode_certificate_trust (ETrustPromptResponse response)
{
	return e_enum_to_string (E_TYPE_TRUST_PROMPT_RESPONSE, response);
}

/**
 * e_config_lookup_decode_certificate_trust:
 * @value: a text value to decode
 *
 * Decodes text @value to #ETrustPromptResponse, previously encoded
 * with e_config_lookup_encode_certificate_trust().
 *
 * Returns: an #ETrustPromptResponse corresponding to @value.
 *
 * Since: 3.28
 **/
ETrustPromptResponse
e_config_lookup_decode_certificate_trust (const gchar *value)
{
	gint decoded;

	if (!value ||
	    !e_enum_from_string (E_TYPE_TRUST_PROMPT_RESPONSE, value, &decoded))
		decoded = E_TRUST_PROMPT_RESPONSE_UNKNOWN;

	return decoded;
}
