/*
 * SPDX-FileCopyrightText: (C) 2017 Red Hat, Inc. (www.redhat.com)
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

/**
 * SECTION: e-config-lookup-worker
 * @include: e-util/e-util.h
 * @short_description: Configuration lookup worker interface
 *
 * #EConfigLookupWorker is an interface which runs the configuration look up.
 **/

#include "evolution-config.h"

#include <libedataserver/libedataserver.h>

#include "e-config-lookup.h"
#include "e-util-enums.h"

#include "e-config-lookup-worker.h"

G_DEFINE_QUARK (e-config-lookup-worker-error-quark, e_config_lookup_worker_error)

G_DEFINE_INTERFACE (EConfigLookupWorker, e_config_lookup_worker, G_TYPE_OBJECT)

static void
e_config_lookup_worker_default_init (EConfigLookupWorkerInterface *iface)
{
	iface->get_display_name = NULL;
	iface->run = NULL;
}

/**
 * e_config_lookup_worker_get_display_name:
 * @lookup_worker: an #EConfigLookupWorker
 *
 * Returns: human readable display name of this @lookup_worker
 *
 * Since: 3.28
 **/
const gchar *
e_config_lookup_worker_get_display_name (EConfigLookupWorker *lookup_worker)
{
	EConfigLookupWorkerInterface *iface;

	g_return_val_if_fail (E_IS_CONFIG_LOOKUP_WORKER (lookup_worker), NULL);

	iface = E_CONFIG_LOOKUP_WORKER_GET_INTERFACE (lookup_worker);
	g_return_val_if_fail (iface != NULL, NULL);
	g_return_val_if_fail (iface->get_display_name != NULL, NULL);

	return iface->get_display_name (lookup_worker);
}

/**
 * e_config_lookup_worker_run:
 * @lookup_worker: an #EConfigLookupWorker
 * @config_lookup: an #EConfigLookup
 * @params: an #ENamedParameters with additional parameters
 * @out_restart_params: (out): optional #ENamedParameters, used to pass when restart is requested
 * @cancellable: optional #GCancellable object, or %NULL
 * @error: return location for a #GError, or %NULL
 *
 * Runs actual configuration look up. This method is called
 * from a dedicated thread.
 *
 * When @out_restart_params is not %NULL at the end of the call,
 * then it's saved for later re-run of the look up, in case
 * the @error indicates it can be restarted, by using appropriate
 * #EConfigLookupWorkerError.
 *
 * Since: 3.28
 **/
void
e_config_lookup_worker_run (EConfigLookupWorker *lookup_worker,
			    EConfigLookup *config_lookup,
			    const ENamedParameters *params,
			    ENamedParameters **out_restart_params,
			    GCancellable *cancellable,
			    GError **error)
{
	EConfigLookupWorkerInterface *iface;

	g_return_if_fail (E_IS_CONFIG_LOOKUP_WORKER (lookup_worker));
	g_return_if_fail (E_IS_CONFIG_LOOKUP (config_lookup));

	iface = E_CONFIG_LOOKUP_WORKER_GET_INTERFACE (lookup_worker);
	g_return_if_fail (iface != NULL);
	g_return_if_fail (iface->run != NULL);

	iface->run (lookup_worker, config_lookup, params, out_restart_params, cancellable, error);
}
