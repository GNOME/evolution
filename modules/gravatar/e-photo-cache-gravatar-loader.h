/*
 * e-photo-cache-gravatar-loader.h
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

#ifndef E_PHOTO_CACHE_GRAVATAR_LOADER_H
#define E_PHOTO_CACHE_GRAVATAR_LOADER_H

#include <e-util/e-util.h>

/* Standard GObject macros */
#define E_TYPE_PHOTO_CACHE_GRAVATAR_LOADER \
	(e_photo_cache_gravatar_loader_get_type ())
#define E_PHOTO_CACHE_GRAVATAR_LOADER(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), E_TYPE_PHOTO_CACHE_GRAVATAR_LOADER, EPhotoCacheGravatarLoader))
#define E_PHOTO_CACHE_GRAVATAR_LOADER_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((cls), E_TYPE_PHOTO_CACHE_GRAVATAR_LOADER, EPhotoCacheGravatarLoaderClass))
#define E_IS_PHOTO_CACHE_GRAVATAR_LOADER(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), E_TYPE_PHOTO_CACHE_GRAVATAR_LOADER))
#define E_IS_PHOTO_CACHE_GRAVATAR_LOADER_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_TYPE \
	((cls), E_TYPE_PHOTO_CACHE_GRAVATAR_LOADER))
#define E_PHOTO_CACHE_GRAVATAR_LOADER_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS \
	((obj), E_TYPE_PHOTO_CACHE_GRAVATAR_LOADER, EPhotoCacheGravatarLoaderClass))

G_BEGIN_DECLS

typedef struct _EPhotoCacheGravatarLoader EPhotoCacheGravatarLoader;
typedef struct _EPhotoCacheGravatarLoaderClass EPhotoCacheGravatarLoaderClass;
typedef struct _EPhotoCacheGravatarLoaderPrivate EPhotoCacheGravatarLoaderPrivate;

struct _EPhotoCacheGravatarLoader {
	EExtension parent;
	EPhotoCacheGravatarLoaderPrivate *priv;
};

struct _EPhotoCacheGravatarLoaderClass {
	EExtensionClass parent_class;
};

GType		e_photo_cache_gravatar_loader_get_type
						(void) G_GNUC_CONST;
void		e_photo_cache_gravatar_loader_type_register
						(GTypeModule *type_module);

G_END_DECLS

#endif /* E_PHOTO_CACHE_GRAVATAR_LOADER_H */

