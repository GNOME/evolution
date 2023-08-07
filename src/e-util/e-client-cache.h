/*
 * e-client-cache.h
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

#if !defined (__E_UTIL_H_INSIDE__) && !defined (LIBEUTIL_COMPILATION)
#error "Only <e-util/e-util.h> should be included directly."
#endif

#ifndef E_CLIENT_CACHE_H
#define E_CLIENT_CACHE_H

#include <e-util/e-alert.h>
#include <libedataserver/libedataserver.h>

/* Standard GObject macros */
#define E_TYPE_CLIENT_CACHE \
	(e_client_cache_get_type ())
#define E_CLIENT_CACHE(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), E_TYPE_CLIENT_CACHE, EClientCache))
#define E_CLIENT_CACHE_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((cls), E_TYPE_CLIENT_CACHE, EClientCacheClass))
#define E_IS_CLIENT_CACHE(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), E_TYPE_CLIENT_CACHE))
#define E_IS_CLIENT_CACHE_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_TYPE \
	((cls), E_TYPE_CLIENT_CACHE))
#define E_CLIENT_CACHE_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS \
	((obj), E_TYPE_CLIENT_CACHE, EClientCacheClass))

/* How long to wait for the connected state by default. Actual calls
   to open the client can change the value as they wish, this is only
   a constant, which can be used on places where it's expected to be
   connected before starting the actual operation with the client. */
#define E_DEFAULT_WAIT_FOR_CONNECTED_SECONDS 5

G_BEGIN_DECLS

typedef struct _EClientCache EClientCache;
typedef struct _EClientCacheClass EClientCacheClass;
typedef struct _EClientCachePrivate EClientCachePrivate;

/**
 * EClientCache:
 *
 * Contains only private data that should be read and manipulated using the
 * functions below.
 **/
struct _EClientCache {
	GObject parent;
	EClientCachePrivate *priv;
};

struct _EClientCacheClass {
	GObjectClass parent_class;

	/* Signals */
	void		(*backend_died)		(EClientCache *client_cache,
						 EClient *client,
						 EAlert *alert);
	void		(*backend_error)	(EClientCache *client_cache,
						 EClient *client,
						 EAlert *alert);
	void		(*client_notify)	(EClientCache *client_cache,
						 EClient *client,
						 GParamSpec *pspec);
	void		(*client_created)	(EClientCache *client_cache,
						 EClient *client);
	void		(*client_connected)	(EClientCache *client_cache,
						 EClient *client);
	void		(*allow_auth_prompt)	(EClientCache *client_cache,
						 ESource *source);
};

GType		e_client_cache_get_type		(void) G_GNUC_CONST;
EClientCache *	e_client_cache_new		(ESourceRegistry *registry);
ESourceRegistry *
		e_client_cache_ref_registry	(EClientCache *client_cache);
EClient *	e_client_cache_get_client_sync	(EClientCache *client_cache,
						 ESource *source,
						 const gchar *extension_name,
						 guint32 wait_for_connected_seconds,
						 GCancellable *cancellable,
						 GError **error);
void		e_client_cache_get_client	(EClientCache *client_cache,
						 ESource *source,
						 const gchar *extension_name,
						 guint32 wait_for_connected_seconds,
						 GCancellable *cancellable,
						 GAsyncReadyCallback callback,
						 gpointer user_data);
EClient *	e_client_cache_get_client_finish
						(EClientCache *client_cache,
						 GAsyncResult *result,
						 GError **error);
EClient *	e_client_cache_ref_cached_client
						(EClientCache *client_cache,
						 ESource *source,
						 const gchar *extension_name);
GSList * /* EClient * */
		e_client_cache_list_cached_clients
						(EClientCache *client_cache,
						 const gchar *extension_name);
gboolean	e_client_cache_is_backend_dead	(EClientCache *client_cache,
						 ESource *source,
						 const gchar *extension_name);
void		e_client_cache_emit_allow_auth_prompt
						(EClientCache *client_cache,
						 ESource *source);

G_END_DECLS

#endif /* E_CLIENT_CACHE_H */

