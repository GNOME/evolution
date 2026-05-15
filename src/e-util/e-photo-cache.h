/*
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#if !defined (__E_UTIL_H_INSIDE__) && !defined (LIBEUTIL_COMPILATION)
#error "Only <e-util/e-util.h> should be included directly."
#endif

#ifndef E_PHOTO_CACHE_H
#define E_PHOTO_CACHE_H

#include <libebook/libebook.h>
#include <e-util/e-client-cache.h>
#include <e-util/e-photo-source.h>

/* Standard GObject macros */
#define E_TYPE_PHOTO_CACHE \
	(e_photo_cache_get_type ())
#define E_PHOTO_CACHE(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), E_TYPE_PHOTO_CACHE, EPhotoCache))
#define E_PHOTO_CACHE_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((cls), E_TYPE_PHOTO_CACHE, EPhotoCacheClass))
#define E_IS_PHOTO_CACHE(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), E_TYPE_PHOTO_CACHE))
#define E_IS_PHOTO_CACHE_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_TYPE \
	((cls), E_TYPE_PHOTO_CACHE))
#define E_PHOTO_CACHE_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS \
	((obj), E_TYPE_PHOTO_CACHE, EPhotoCacheClass))

G_BEGIN_DECLS

typedef struct _EPhotoCache EPhotoCache;
typedef struct _EPhotoCacheClass EPhotoCacheClass;
typedef struct _EPhotoCachePrivate EPhotoCachePrivate;

/**
 * EPhotoCache:
 *
 * Contains only private data that should be read and manipulated using the
 * functions below.
 **/
struct _EPhotoCache {
	GObject parent;
	EPhotoCachePrivate *priv;
};

struct _EPhotoCacheClass {
	GObjectClass parent_class;
};

GType		e_photo_cache_get_type		(void) G_GNUC_CONST;
EPhotoCache *	e_photo_cache_new		(EClientCache *client_cache);
EClientCache *	e_photo_cache_ref_client_cache	(EPhotoCache *photo_cache);
void		e_photo_cache_add_photo_source	(EPhotoCache *photo_cache,
						 EPhotoSource *photo_source);
GList *		e_photo_cache_list_photo_sources
						(EPhotoCache *photo_cache);
gboolean	e_photo_cache_remove_photo_source
						(EPhotoCache *photo_cache,
						 EPhotoSource *photo_source);
void		e_photo_cache_add_photo		(EPhotoCache *photo_cache,
						 const gchar *email_address,
						 GBytes *bytes);
gboolean	e_photo_cache_remove_photo	(EPhotoCache *photo_cache,
						 const gchar *email_address);
gboolean	e_photo_cache_get_photo_sync	(EPhotoCache *photo_cache,
						 const gchar *email_address,
						 GCancellable *cancellable,
						 GInputStream **out_stream,
						 GError **error);
void		e_photo_cache_get_photo		(EPhotoCache *photo_cache,
						 const gchar *email_address,
						 GCancellable *cancellable,
						 GAsyncReadyCallback callback,
						 gpointer user_data);
gboolean	e_photo_cache_get_photo_finish	(EPhotoCache *photo_cache,
						 GAsyncResult *result,
						 GInputStream **out_stream,
						 GError **error);

G_END_DECLS

#endif /* E_PHOTO_CACHE_H */

