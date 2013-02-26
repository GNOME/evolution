/*
 * e-settings-photo-cache.h
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) version 3.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with the program; if not, see <http://www.gnu.org/licenses/>
 *
 */

#ifndef E_SETTINGS_PHOTO_CACHE_H
#define E_SETTINGS_PHOTO_CACHE_H

#include <e-util/e-util.h>

/* Standard GObject macros */
#define E_TYPE_SETTINGS_PHOTO_CACHE \
	(e_settings_photo_cache_get_type ())
#define E_SETTINGS_PHOTO_CACHE(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), E_TYPE_SETTINGS_PHOTO_CACHE, ESettingsPhotoCache))
#define E_SETTINGS_PHOTO_CACHE_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((cls), E_TYPE_SETTINGS_PHOTO_CACHE, ESettingsPhotoCacheClass))
#define E_IS_SETTINGS_PHOTO_CACHE(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), E_TYPE_SETTINGS_PHOTO_CACHE))
#define E_IS_SETTINGS_PHOTO_CACHE_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_TYPE \
	((cls), E_TYPE_SETTINGS_PHOTO_CACHE))
#define E_SETTINGS_PHOTO_CACHE_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS \
	((obj), E_TYPE_SETTINGS_PHOTO_CACHE, ESettingsPhotoCacheClass))

G_BEGIN_DECLS

typedef struct _ESettingsPhotoCache ESettingsPhotoCache;
typedef struct _ESettingsPhotoCacheClass ESettingsPhotoCacheClass;
typedef struct _ESettingsPhotoCachePrivate ESettingsPhotoCachePrivate;

struct _ESettingsPhotoCache {
	EExtension parent;
	ESettingsPhotoCachePrivate *priv;
};

struct _ESettingsPhotoCacheClass {
	EExtensionClass parent_class;
};

GType		e_settings_photo_cache_get_type	(void) G_GNUC_CONST;
void		e_settings_photo_cache_type_register
						(GTypeModule *type_module);

G_END_DECLS

#endif /* E_SETTINGS_PHOTO_CACHE_H */

