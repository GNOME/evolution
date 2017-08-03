/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
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

#include <libedataserver/libedataserver.h>

#include "e-activity.h"
#include "e-config-lookup-result.h"
#include "e-simple-async-result.h"
#include "e-util-enumtypes.h"

#include "e-config-lookup.h"

struct _EConfigLookupPrivate {
	ESourceRegistry *registry;

	GMutex property_lock;
	GSList *results; /* EConfigLookupResult * */

	ESimpleAsyncResult *run_result;
	GCancellable *run_cancellable;

	GThreadPool *pool;
};

enum {
	PROP_0,
	PROP_REGISTRY
};

enum {
	RUN,
	GET_SOURCE,
	LAST_SIGNAL
};

static guint signals[LAST_SIGNAL];

G_DEFINE_TYPE_WITH_CODE (EConfigLookup, e_config_lookup, G_TYPE_OBJECT,
	G_IMPLEMENT_INTERFACE (E_TYPE_EXTENSIBLE, NULL))

typedef struct _ThreadData {
	ENamedParameters *params;
	EActivity *activity;
	EConfigLookupThreadFunc thread_func;
	gpointer user_data;
	GDestroyNotify user_data_free;
} ThreadData;

static void
config_lookup_thread (gpointer data,
		      gpointer user_data)
{
	ThreadData *td = data;
	EConfigLookup *config_lookup = user_data;

	g_return_if_fail (td != NULL);
	g_return_if_fail (td->params != NULL);
	g_return_if_fail (E_IS_ACTIVITY (td->activity));
	g_return_if_fail (td->thread_func != NULL);
	g_return_if_fail (E_IS_CONFIG_LOOKUP (config_lookup));

	td->thread_func (config_lookup, td->params, td->user_data, e_activity_get_cancellable (td->activity));

	if (td->user_data_free)
		td->user_data_free (td->user_data);

	e_named_parameters_free (td->params);
	g_object_unref (td->activity);
	g_free (td);
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

	g_mutex_lock (&config_lookup->priv->property_lock);

	if (config_lookup->priv->run_cancellable) {
		g_cancellable_cancel (config_lookup->priv->run_cancellable);
		g_clear_object (&config_lookup->priv->run_cancellable);
	}

	g_mutex_unlock (&config_lookup->priv->property_lock);

	g_clear_object (&config_lookup->priv->registry);

	/* Chain up to parent's method. */
	G_OBJECT_CLASS (e_config_lookup_parent_class)->dispose (object);
}

static void
config_lookup_finalize (GObject *object)
{
	EConfigLookup *config_lookup = E_CONFIG_LOOKUP (object);

	g_slist_free_full (config_lookup->priv->results, g_object_unref);
	g_thread_pool_free (config_lookup->priv->pool, TRUE, FALSE);
	g_mutex_clear (&config_lookup->priv->property_lock);

	/* Chain up to parent's method. */
	G_OBJECT_CLASS (e_config_lookup_parent_class)->finalize (object);
}

static void
e_config_lookup_class_init (EConfigLookupClass *klass)
{
	GObjectClass *object_class;

	g_type_class_add_private (klass, sizeof (EConfigLookupPrivate));

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
	 * EConfigLookup::run:
	 * @params: an #ENamedParameters with additional parameters
	 * @activity: an #EActivity
	 *
	 * Emitted to run config lookup by each extension. The extensions
	 * run their code asynchronously and claim the result by
	 * e_config_lookup_add_result(). The extension also references the @activity
	 * for the whole run, because it's used to know when all extensions
	 * are finished with searching. Extensions can use e_config_lookup_create_thread(),
	 * which does necessary things for it.
	 *
	 * The signal is emitted from the main/GUI thread, but the @activity can be
	 * unreffed in another thread too.
	 *
	 * Since: 3.26
	 **/
	signals[RUN] = g_signal_new (
		"run",
		G_TYPE_FROM_CLASS (klass),
		G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
		G_STRUCT_OFFSET (EConfigLookupClass, run),
		NULL, NULL,
		NULL,
		G_TYPE_NONE, 2,
		E_TYPE_NAMED_PARAMETERS,
		E_TYPE_ACTIVITY);

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
}

static void
e_config_lookup_init (EConfigLookup *config_lookup)
{
	config_lookup->priv = G_TYPE_INSTANCE_GET_PRIVATE (config_lookup, E_TYPE_CONFIG_LOOKUP, EConfigLookupPrivate);

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

static void
config_lookup_activity_gone (gpointer user_data,
			     GObject *object)
{
	EConfigLookup *config_lookup = user_data;
	ESimpleAsyncResult *run_result;

	g_return_if_fail (E_IS_CONFIG_LOOKUP (config_lookup));

	g_mutex_lock (&config_lookup->priv->property_lock);

	run_result = config_lookup->priv->run_result;
	config_lookup->priv->run_result = NULL;

	g_mutex_unlock (&config_lookup->priv->property_lock);

	if (run_result) {
		e_simple_async_result_complete_idle (run_result);
		g_object_unref (run_result);
	}

	g_object_unref (config_lookup);
}

/**
 * e_config_lookup_run:
 * @config_lookup: an #EConfigLookup
 * @params: an #ENamedParameters with lookup parameters
 * @cancellable: an optional #GCancellable, or %NULL
 * @callback: a callback to call, when the run is finished
 * @user_data: user data for the @callback
 *
 * Runs configuration lookup asynchronously, by emitting the #EConfigLookup::run signal.
 * Once the run is done, the @callback is called, and the call can be finished with
 * e_config_lookup_run_finish(). The @callback is always called from the main thread.
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
	EActivity *activity;

	g_return_if_fail (E_IS_CONFIG_LOOKUP (config_lookup));
	g_return_if_fail (params != NULL);
	g_return_if_fail (callback != NULL);

	g_mutex_lock (&config_lookup->priv->property_lock);

	if (config_lookup->priv->run_result) {
		g_mutex_unlock (&config_lookup->priv->property_lock);

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

	activity = e_activity_new ();
	e_activity_set_cancellable (activity, cancellable);

	g_object_weak_ref (G_OBJECT (activity), config_lookup_activity_gone, g_object_ref (config_lookup));

	g_mutex_unlock (&config_lookup->priv->property_lock);

	g_signal_emit (config_lookup, signals[RUN], 0, params, activity);

	g_object_unref (activity);
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
 * e_config_lookup_create_thread:
 * @config_lookup: an #EConfigLookup
 * @params: an #ENamedParameters with lookup parameters
 * @activity: an #EActivity
 * @thread_func: function to call in a new thread
 * @user_data: (nullable): optional user data for @thread_func, or %NULL
 * @user_data_free: (nullable): optional free function for @user_data, or %NULL
 *
 * Creates a new thread and calls @thread_func in it. It also references @activity
 * and unreferences it once the @thread_func is done.
 *
 * This function might be usually called by extensions in a signal handler
 * for the #EConfigLookup::run signal.
 *
 * Since: 3.26
 **/
void
e_config_lookup_create_thread (EConfigLookup *config_lookup,
			       const ENamedParameters *params,
			       EActivity *activity,
			       EConfigLookupThreadFunc thread_func,
			       gpointer user_data,
			       GDestroyNotify user_data_free)
{
	ThreadData *td;

	g_return_if_fail (E_IS_CONFIG_LOOKUP (config_lookup));
	g_return_if_fail (params != NULL);
	g_return_if_fail (E_IS_ACTIVITY (activity));
	g_return_if_fail (thread_func != NULL);

	td = g_new0 (ThreadData, 1);
	td->params = e_named_parameters_new_clone (params);
	td->activity = g_object_ref (activity);
	td->thread_func = thread_func;
	td->user_data = user_data;
	td->user_data_free = user_data_free;

	g_thread_pool_push (config_lookup->priv->pool, td, NULL);
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
 * The list of results can be obtained with e_config_lookup_get_results().
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
}

/**
 * e_config_lookup_get_results:
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
 * Returns: (element-type EConfigLookupResult) (transfer full): a #GSList
 *    with results satisfying the @kind and @protocol filtering conditions.
 *
 * Since: 3.26
 **/
GSList *
e_config_lookup_get_results (EConfigLookup *config_lookup,
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
