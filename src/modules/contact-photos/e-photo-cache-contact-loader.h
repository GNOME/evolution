/*
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#ifndef E_PHOTO_CACHE_CONTACT_LOADER_H
#define E_PHOTO_CACHE_CONTACT_LOADER_H

#include <e-util/e-util.h>

/* Standard GObject macros */
#define E_TYPE_PHOTO_CACHE_CONTACT_LOADER \
	(e_photo_cache_contact_loader_get_type ())
#define E_PHOTO_CACHE_CONTACT_LOADER(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), E_TYPE_PHOTO_CACHE_CONTACT_LOADER, EPhotoCacheContactLoader))
#define E_PHOTO_CACHE_CONTACT_LOADER_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((cls), E_TYPE_PHOTO_CACHE_CONTACT_LOADER, EPhotoCacheContactLoaderClass))
#define E_IS_PHOTO_CACHE_CONTACT_LOADER(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), E_TYPE_PHOTO_CACHE_CONTACT_LOADER))
#define E_IS_PHOTO_CACHE_CONTACT_LOADER_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_TYPE \
	((cls), E_TYPE_PHOTO_CACHE_CONTACT_LOADER))
#define E_PHOTO_CACHE_CONTACT_LOADER_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS \
	((obj), E_TYPE_PHOTO_CACHE_CONTACT_LOADER, EPhotoCacheContactLoaderClass))

G_BEGIN_DECLS

typedef struct _EPhotoCacheContactLoader EPhotoCacheContactLoader;
typedef struct _EPhotoCacheContactLoaderClass EPhotoCacheContactLoaderClass;
typedef struct _EPhotoCacheContactLoaderPrivate EPhotoCacheContactLoaderPrivate;

struct _EPhotoCacheContactLoader {
	EExtension parent;
	EPhotoCacheContactLoaderPrivate *priv;
};

struct _EPhotoCacheContactLoaderClass {
	EExtensionClass parent_class;
};

GType		e_photo_cache_contact_loader_get_type
						(void) G_GNUC_CONST;
void		e_photo_cache_contact_loader_type_register
						(GTypeModule *type_module);

G_END_DECLS

#endif /* E_PHOTO_CACHE_CONTACT_LOADER_H */

