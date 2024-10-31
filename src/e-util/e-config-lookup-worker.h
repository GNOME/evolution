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

#if !defined (__E_UTIL_H_INSIDE__) && !defined (LIBEUTIL_COMPILATION)
#error "Only <e-util/e-util.h> should be included directly."
#endif

#ifndef E_CONFIG_LOOKUP_WORKER_H
#define E_CONFIG_LOOKUP_WORKER_H

#include <libedataserver/libedataserver.h>

#include <e-util/e-util-enums.h>

/* Standard GObject macros */
#define E_TYPE_CONFIG_LOOKUP_WORKER \
	(e_config_lookup_worker_get_type ())
#define E_CONFIG_LOOKUP_WORKER(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), E_TYPE_CONFIG_LOOKUP_WORKER, EConfigLookupWorker))
#define E_CONFIG_LOOKUP_WORKER_INTERFACE(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((cls), E_TYPE_CONFIG_LOOKUP_WORKER, EConfigLookupWorkerInterface))
#define E_IS_CONFIG_LOOKUP_WORKER(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), E_TYPE_CONFIG_LOOKUP_WORKER))
#define E_IS_CONFIG_LOOKUP_WORKER_INTERFACE(cls) \
	(G_TYPE_CHECK_CLASS_TYPE \
	((cls), E_TYPE_CONFIG_LOOKUP_WORKER))
#define E_CONFIG_LOOKUP_WORKER_GET_INTERFACE(obj) \
	(G_TYPE_INSTANCE_GET_INTERFACE \
	((obj), E_TYPE_CONFIG_LOOKUP_WORKER, EConfigLookupWorkerInterface))

/**
 * E_CONFIG_LOOKUP_WORKER_ERROR:
 *
 * An #EConfigLookupWorker error domain, which can be used
 * in e_config_lookup_worker_run().
 *
 * Since: 3.28
 **/
#define E_CONFIG_LOOKUP_WORKER_ERROR (e_config_lookup_worker_error_quark ())

G_BEGIN_DECLS

/**
 * EConfigLookupWorkerError:
 * @E_CONFIG_LOOKUP_WORKER_ERROR_REQUIRES_PASSWORD: a password is required to continue
 * @E_CONFIG_LOOKUP_WORKER_ERROR_CERTIFICATE: there is an issue with server certificate; the error
 *    message contains description of the issue, the out_restart_params contains E_CONFIG_LOOKUP_PARAM_CERTIFICATE_PEM
 *    with the offending certificate and when the user sets a trust on the certificate it is passed
 *    back to the caller in the E_CONFIG_LOOKUP_PARAM_CERTIFICATE_TRUST parameters. Decode the trust
 *    value with e_config_lookup_decode_certificate_trust()
 *
 * A set of possible errors in #E_CONFIG_LOOKUP_WORKER_ERROR domain.
 *
 * Since: 3.28
 **/
typedef enum {
	E_CONFIG_LOOKUP_WORKER_ERROR_REQUIRES_PASSWORD,
	E_CONFIG_LOOKUP_WORKER_ERROR_CERTIFICATE
} EConfigLookupWorkerError;

struct _EConfigLookup;

typedef struct _EConfigLookupWorker EConfigLookupWorker;
typedef struct _EConfigLookupWorkerInterface EConfigLookupWorkerInterface;

struct _EConfigLookupWorkerInterface {
	GTypeInterface parent_interface;

	const gchar *	(* get_display_name)	(EConfigLookupWorker *lookup_worker);
	void		(* run)			(EConfigLookupWorker *lookup_worker,
						 struct _EConfigLookup *config_lookup,
						 const ENamedParameters *params,
						 ENamedParameters **out_restart_params,
						 GCancellable *cancellable,
						 GError **error);
};

GQuark		e_config_lookup_worker_error_quark	(void) G_GNUC_CONST;
GType		e_config_lookup_worker_get_type		(void);
const gchar *	e_config_lookup_worker_get_display_name	(EConfigLookupWorker *lookup_worker);
void		e_config_lookup_worker_run		(EConfigLookupWorker *lookup_worker,
							 struct _EConfigLookup *config_lookup,
							 const ENamedParameters *params,
							 ENamedParameters **out_restart_params,
							 GCancellable *cancellable,
							 GError **error);

G_END_DECLS

#endif /* E_CONFIG_LOOKUP_WORKER_H */
