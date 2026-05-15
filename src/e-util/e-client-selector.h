/*
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#if !defined (__E_UTIL_H_INSIDE__) && !defined (LIBEUTIL_COMPILATION)
#error "Only <e-util/e-util.h> should be included directly."
#endif

#ifndef E_CLIENT_SELECTOR_H
#define E_CLIENT_SELECTOR_H

#include <e-util/e-client-cache.h>
#include <e-util/e-source-selector.h>

/* Standard GObject macros */
#define E_TYPE_CLIENT_SELECTOR \
	(e_client_selector_get_type ())
#define E_CLIENT_SELECTOR(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), E_TYPE_CLIENT_SELECTOR, EClientSelector))
#define E_CLIENT_SELECTOR_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((cls), E_TYPE_CLIENT_SELECTOR, EClientSelectorClass))
#define E_IS_CLIENT_SELECTOR(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), E_TYPE_CLIENT_SELECTOR))
#define E_IS_CLIENT_SELECTOR_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_TYPE \
	((cls), E_TYPE_CLIENT_SELECTOR))
#define E_CLIENT_SELECTOR_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS \
	((obj), E_TYPE_CLIENT_SELECTOR, EClientSelectorClass))

G_BEGIN_DECLS

typedef struct _EClientSelector EClientSelector;
typedef struct _EClientSelectorClass EClientSelectorClass;
typedef struct _EClientSelectorPrivate EClientSelectorPrivate;

struct _EClientSelector {
	ESourceSelector parent;
	EClientSelectorPrivate *priv;
};

struct _EClientSelectorClass {
	ESourceSelectorClass parent_class;
};

GType		e_client_selector_get_type	(void) G_GNUC_CONST;
GtkWidget *	e_client_selector_new		(EClientCache *client_cache,
						 const gchar *extension_name);
EClientCache *	e_client_selector_ref_client_cache
						(EClientSelector *selector);
EClient *	e_client_selector_get_client_sync
						(EClientSelector *selector,
						 ESource *source,
						 gboolean call_allow_auth_prompt,
						 guint32 wait_for_connected_seconds,
						 GCancellable *cancellable,
						 GError **error);
void		e_client_selector_get_client	(EClientSelector *selector,
						 ESource *source,
						 gboolean call_allow_auth_prompt,
						 guint32 wait_for_connected_seconds,
						 GCancellable *cancellable,
						 GAsyncReadyCallback callback,
						 gpointer user_data);
EClient *	e_client_selector_get_client_finish
						(EClientSelector *selector,
						 GAsyncResult *result,
						 GError **error);
EClient *	e_client_selector_ref_cached_client
						(EClientSelector *selector,
						 ESource *source);
EClient *	e_client_selector_ref_cached_client_by_iter
						(EClientSelector *selector,
						 GtkTreeIter *iter);
gboolean	e_client_selector_is_backend_dead
						(EClientSelector *selector,
						 ESource *source);

G_END_DECLS

#endif /* E_CLIENT_SELECTOR_H */

