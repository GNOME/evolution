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

#ifndef E_CONFIG_LOOKUP_H
#define E_CONFIG_LOOKUP_H

#include <libedataserver/libedataserver.h>

#include <e-util/e-util-enums.h>
#include <e-util/e-config-lookup-result.h>
#include <e-util/e-config-lookup-worker.h>

/* Standard GObject macros */
#define E_TYPE_CONFIG_LOOKUP \
	(e_config_lookup_get_type ())
#define E_CONFIG_LOOKUP(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), E_TYPE_CONFIG_LOOKUP, EConfigLookup))
#define E_CONFIG_LOOKUP_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((cls), E_TYPE_CONFIG_LOOKUP, EConfigLookupClass))
#define E_IS_CONFIG_LOOKUP(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), E_TYPE_CONFIG_LOOKUP))
#define E_IS_CONFIG_LOOKUP_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_TYPE \
	((cls), E_TYPE_CONFIG_LOOKUP))
#define E_CONFIG_LOOKUP_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS \
	((obj), E_TYPE_CONFIG_LOOKUP, EConfigLookupClass))

#define E_CONFIG_LOOKUP_PARAM_USER		"user"
#define E_CONFIG_LOOKUP_PARAM_PASSWORD		"password"
#define E_CONFIG_LOOKUP_PARAM_REMEMBER_PASSWORD	"remember-password" /* Is part of params, if can remember, otherwise not in params */
#define E_CONFIG_LOOKUP_PARAM_EMAIL_ADDRESS	"email-address"
#define E_CONFIG_LOOKUP_PARAM_SERVERS		"servers"
#define E_CONFIG_LOOKUP_PARAM_CERTIFICATE_PEM	"certificate-pem"
#define E_CONFIG_LOOKUP_PARAM_CERTIFICATE_HOST	"certificate-host"
#define E_CONFIG_LOOKUP_PARAM_CERTIFICATE_TRUST	"certificate-trust"

G_BEGIN_DECLS

typedef struct _EConfigLookup EConfigLookup;
typedef struct _EConfigLookupClass EConfigLookupClass;
typedef struct _EConfigLookupPrivate EConfigLookupPrivate;

/**
 * EConfigLookup:
 *
 * Contains only private data that should be read and manipulated using
 * the functions below.
 *
 * Since: 3.26
 **/
struct _EConfigLookup {
	/*< private >*/
	GObject parent;
	EConfigLookupPrivate *priv;
};

struct _EConfigLookupClass {
	/*< private >*/
	GObjectClass parent_class;

	/* Signals */
	ESource *	(* get_source)		(EConfigLookup *config_lookup,
						 EConfigLookupSourceKind kind);
	void		(* worker_started)	(EConfigLookup *config_lookup,
						 EConfigLookupWorker *worker,
						 GCancellable *cancellable);
	void		(* worker_finished)	(EConfigLookup *config_lookup,
						 EConfigLookupWorker *worker,
						 const ENamedParameters *restart_params,
						 const GError *error);
	void		(* result_added)	(EConfigLookup *config_lookup,
						 EConfigLookupResult *result);
};

GType		e_config_lookup_get_type		(void) G_GNUC_CONST;
EConfigLookup *	e_config_lookup_new			(ESourceRegistry *registry);
ESourceRegistry *
		e_config_lookup_get_registry		(EConfigLookup *config_lookup);
ESource *	e_config_lookup_get_source		(EConfigLookup *config_lookup,
							 EConfigLookupSourceKind kind);
gboolean	e_config_lookup_get_busy		(EConfigLookup *config_lookup);
void		e_config_lookup_cancel_all		(EConfigLookup *config_lookup);
void		e_config_lookup_register_worker		(EConfigLookup *config_lookup,
							 EConfigLookupWorker *worker);
void		e_config_lookup_unregister_worker	(EConfigLookup *config_lookup,
							 EConfigLookupWorker *worker);
GSList *	e_config_lookup_dup_registered_workers	(EConfigLookup *config_lookup);
void		e_config_lookup_run			(EConfigLookup *config_lookup,
							 const ENamedParameters *params,
							 GCancellable *cancellable,
							 GAsyncReadyCallback callback,
							 gpointer user_data);
void		e_config_lookup_run_finish		(EConfigLookup *config_lookup,
							 GAsyncResult *result);
void		e_config_lookup_run_worker		(EConfigLookup *config_lookup,
							 EConfigLookupWorker *worker,
							 const ENamedParameters *params,
							 GCancellable *cancellable);
void		e_config_lookup_add_result		(EConfigLookup *config_lookup,
							 EConfigLookupResult *result);
gint		e_config_lookup_count_results		(EConfigLookup *config_lookup);
GSList *	e_config_lookup_dup_results		(EConfigLookup *config_lookup,
							 EConfigLookupResultKind kind,
							 const gchar *protocol);
void		e_config_lookup_clear_results		(EConfigLookup *config_lookup);

const gchar *	e_config_lookup_encode_certificate_trust(ETrustPromptResponse response);
ETrustPromptResponse
		e_config_lookup_decode_certificate_trust(const gchar *value);

G_END_DECLS

#endif /* E_CONFIG_LOOKUP_H */
