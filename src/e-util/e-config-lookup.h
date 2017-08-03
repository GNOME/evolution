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

#if !defined (__E_UTIL_H_INSIDE__) && !defined (LIBEUTIL_COMPILATION)
#error "Only <e-util/e-util.h> should be included directly."
#endif

#ifndef E_CONFIG_LOOKUP_H
#define E_CONFIG_LOOKUP_H

#include <libedataserver/libedataserver.h>

#include <e-util/e-activity.h>
#include <e-util/e-util-enums.h>
#include <e-util/e-config-lookup-result.h>

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
#define E_CONFIG_LOOKUP_PARAM_EMAIL_ADDRESS	"email-address"
#define E_CONFIG_LOOKUP_PARAM_SERVERS		"servers"

G_BEGIN_DECLS

typedef struct _EConfigLookup EConfigLookup;
typedef struct _EConfigLookupClass EConfigLookupClass;
typedef struct _EConfigLookupPrivate EConfigLookupPrivate;

/**
 * EConfigLookupThreadFunc:
 * @config_lookup: an #EConfigLookup
 * @params: an #ENamedParameters with additional parameters
 * @user_data: user data passed to e_config_lookup_create_thread()
 * @cancellable: a #GCancellable
 *
 * A function executed in a dedicated thread.
 *
 * Since: 3.26
 **/
typedef void (* EConfigLookupThreadFunc) (EConfigLookup *config_lookup,
					  const ENamedParameters *params,
					  gpointer user_data,
					  GCancellable *cancellable);

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
	void		(* run)		(EConfigLookup *config_lookup,
					 const ENamedParameters *params,
					 EActivity *activity);
	ESource *	(* get_source)	(EConfigLookup *config_lookup,
					 EConfigLookupSourceKind kind);
};

GType		e_config_lookup_get_type		(void) G_GNUC_CONST;
EConfigLookup *	e_config_lookup_new			(ESourceRegistry *registry);
ESourceRegistry *
		e_config_lookup_get_registry		(EConfigLookup *config_lookup);
ESource *	e_config_lookup_get_source		(EConfigLookup *config_lookup,
							 EConfigLookupSourceKind kind);
void		e_config_lookup_run			(EConfigLookup *config_lookup,
							 const ENamedParameters *params,
							 GCancellable *cancellable,
							 GAsyncReadyCallback callback,
							 gpointer user_data);
void		e_config_lookup_run_finish		(EConfigLookup *config_lookup,
							 GAsyncResult *result);
void		e_config_lookup_create_thread		(EConfigLookup *config_lookup,
							 const ENamedParameters *params,
							 EActivity *activity,
							 EConfigLookupThreadFunc thread_func,
							 gpointer user_data,
							 GDestroyNotify user_data_free);
void		e_config_lookup_add_result		(EConfigLookup *config_lookup,
							 EConfigLookupResult *result);
GSList *	e_config_lookup_get_results		(EConfigLookup *config_lookup,
							 EConfigLookupResultKind kind,
							 const gchar *protocol);

G_END_DECLS

#endif /* E_CONFIG_LOOKUP_H */
