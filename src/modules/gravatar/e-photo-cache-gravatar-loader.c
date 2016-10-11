/*
 * e-photo-cache-gravatar-loader.c
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

#include "e-photo-cache-gravatar-loader.h"

#include "e-gravatar-photo-source.h"

G_DEFINE_DYNAMIC_TYPE (
	EPhotoCacheGravatarLoader,
	e_photo_cache_gravatar_loader,
	E_TYPE_EXTENSION)

static EPhotoCache *
photo_cache_gravatar_loader_get_photo_cache (EPhotoCacheGravatarLoader *loader)
{
	EExtensible *extensible;

	extensible = e_extension_get_extensible (E_EXTENSION (loader));

	return E_PHOTO_CACHE (extensible);
}

static void
photo_cache_gravatar_loader_constructed (GObject *object)
{
	EPhotoCacheGravatarLoader *loader;
	EPhotoCache *photo_cache;
	EPhotoSource *photo_source;

	/* Chain up to parent's constructed() method. */
	G_OBJECT_CLASS (e_photo_cache_gravatar_loader_parent_class)->constructed (object);

	loader = E_PHOTO_CACHE_GRAVATAR_LOADER (object);
	photo_cache = photo_cache_gravatar_loader_get_photo_cache (loader);

	photo_source = e_gravatar_photo_source_new ();
	e_photo_cache_add_photo_source (photo_cache, photo_source);
	g_object_unref (photo_source);
}

static void
e_photo_cache_gravatar_loader_class_init (EPhotoCacheGravatarLoaderClass *class)
{
	GObjectClass *object_class;
	EExtensionClass *extension_class;

	object_class = G_OBJECT_CLASS (class);
	object_class->constructed = photo_cache_gravatar_loader_constructed;

	extension_class = E_EXTENSION_CLASS (class);
	extension_class->extensible_type = E_TYPE_PHOTO_CACHE;
}

static void
e_photo_cache_gravatar_loader_class_finalize (EPhotoCacheGravatarLoaderClass *class)
{
}

static void
e_photo_cache_gravatar_loader_init (EPhotoCacheGravatarLoader *loader)
{
}

void
e_photo_cache_gravatar_loader_type_register (GTypeModule *type_module)
{
	/* XXX G_DEFINE_DYNAMIC_TYPE declares a static type registration
	 *     function, so we have to wrap it with a public function in
	 *     order to register types from a separate compilation unit. */
	e_photo_cache_gravatar_loader_register_type (type_module);
}

