/*
 * e-settings-photo-cache.c
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

#include "e-settings-photo-cache.h"

#define E_SETTINGS_PHOTO_CACHE_GET_PRIVATE(obj) \
	(G_TYPE_INSTANCE_GET_PRIVATE \
	((obj), E_TYPE_SETTINGS_PHOTO_CACHE, ESettingsPhotoCachePrivate))

struct _ESettingsPhotoCachePrivate {
	gint placeholder;
};

G_DEFINE_DYNAMIC_TYPE (
	ESettingsPhotoCache,
	e_settings_photo_cache,
	E_TYPE_EXTENSION)

static void
settings_photo_cache_constructed (GObject *object)
{
	EExtension *extension;
	EExtensible *extensible;
	GSettings *settings;

	extension = E_EXTENSION (object);
	extensible = e_extension_get_extensible (extension);

	settings = g_settings_new ("org.gnome.evolution.mail");

	g_settings_bind (
		settings, "photo-local",
		extensible, "local-only",
		G_SETTINGS_BIND_GET);

	g_object_unref (settings);

	/* Chain up to parent's constructed() method. */
	G_OBJECT_CLASS (e_settings_photo_cache_parent_class)->
		constructed (object);
}

static void
e_settings_photo_cache_class_init (ESettingsPhotoCacheClass *class)
{
	GObjectClass *object_class;
	EExtensionClass *extension_class;

	g_type_class_add_private (class, sizeof (ESettingsPhotoCachePrivate));

	object_class = G_OBJECT_CLASS (class);
	object_class->constructed = settings_photo_cache_constructed;

	extension_class = E_EXTENSION_CLASS (class);
	extension_class->extensible_type = E_TYPE_PHOTO_CACHE;
}

static void
e_settings_photo_cache_class_finalize (ESettingsPhotoCacheClass *class)
{
}

static void
e_settings_photo_cache_init (ESettingsPhotoCache *extension)
{
	extension->priv = E_SETTINGS_PHOTO_CACHE_GET_PRIVATE (extension);
}

void
e_settings_photo_cache_type_register (GTypeModule *type_module)
{
	/* XXX G_DEFINE_DYNAMIC_TYPE declares a static type registration
	 *     function, so we have to wrap it with a public function in
	 *     order to register types from a separate compilation unit. */
	e_settings_photo_cache_register_type (type_module);
}

