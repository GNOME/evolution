/*
 * e-photo-cache-contact-loader.c
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

#include "e-photo-cache-contact-loader.h"

#include "e-contact-photo-source.h"

struct _EPhotoCacheContactLoaderPrivate {
	ESourceRegistry *registry;
	gulong source_added_handler_id;
	gulong source_removed_handler_id;

	/* ESource -> EPhotoSource */
	GHashTable *photo_sources;
};

G_DEFINE_DYNAMIC_TYPE_EXTENDED (EPhotoCacheContactLoader, e_photo_cache_contact_loader, E_TYPE_EXTENSION, 0,
	G_ADD_PRIVATE_DYNAMIC (EPhotoCacheContactLoader))

static EPhotoCache *
photo_cache_contact_loader_get_photo_cache (EPhotoCacheContactLoader *loader)
{
	EExtensible *extensible;

	extensible = e_extension_get_extensible (E_EXTENSION (loader));

	return E_PHOTO_CACHE (extensible);
}

static void
photo_cache_contact_loader_add_source (EPhotoCacheContactLoader *loader,
                                       ESource *source)
{
	EPhotoCache *photo_cache;
	EPhotoSource *photo_source;
	EClientCache *client_cache;

	photo_cache = photo_cache_contact_loader_get_photo_cache (loader);
	client_cache = e_photo_cache_ref_client_cache (photo_cache);

	photo_source = e_contact_photo_source_new (client_cache, source);
	g_hash_table_insert (
		loader->priv->photo_sources,
		g_object_ref (source),
		g_object_ref (photo_source));
	e_photo_cache_add_photo_source (photo_cache, photo_source);
	g_object_unref (photo_source);

	g_object_unref (client_cache);
}

static void
photo_cache_contact_loader_remove_source (EPhotoCacheContactLoader *loader,
                                          ESource *source)
{
	EPhotoCache *photo_cache;
	EPhotoSource *photo_source;
	GHashTable *hash_table;

	photo_cache = photo_cache_contact_loader_get_photo_cache (loader);

	hash_table = loader->priv->photo_sources;
	photo_source = g_hash_table_lookup (hash_table, source);
	if (photo_source != NULL) {
		e_photo_cache_remove_photo_source (photo_cache, photo_source);
		g_hash_table_remove (hash_table, source);
	}
}

static void
photo_cache_contact_loader_source_added_cb (ESourceRegistry *registry,
                                            ESource *source,
                                            EPhotoCacheContactLoader *loader)
{
	const gchar *extension_name = E_SOURCE_EXTENSION_ADDRESS_BOOK;

	if (e_source_has_extension (source, extension_name))
		photo_cache_contact_loader_add_source (loader, source);
}

static void
photo_cache_contact_loader_source_removed_cb (ESourceRegistry *registry,
                                              ESource *source,
                                              EPhotoCacheContactLoader *loader)
{
	const gchar *extension_name = E_SOURCE_EXTENSION_ADDRESS_BOOK;

	if (e_source_has_extension (source, extension_name))
		photo_cache_contact_loader_remove_source (loader, source);
}

static void
photo_cache_contact_loader_dispose (GObject *object)
{
	EPhotoCacheContactLoader *self = E_PHOTO_CACHE_CONTACT_LOADER (object);

	if (self->priv->source_added_handler_id > 0) {
		g_signal_handler_disconnect (
			self->priv->registry,
			self->priv->source_added_handler_id);
		self->priv->source_added_handler_id = 0;
	}

	if (self->priv->source_removed_handler_id > 0) {
		g_signal_handler_disconnect (
			self->priv->registry,
			self->priv->source_removed_handler_id);
		self->priv->source_removed_handler_id = 0;
	}

	g_clear_object (&self->priv->registry);

	g_hash_table_remove_all (self->priv->photo_sources);

	/* Chain up to parent's dispose() method. */
	G_OBJECT_CLASS (e_photo_cache_contact_loader_parent_class)->dispose (object);
}

static void
photo_cache_contact_loader_finalize (GObject *object)
{
	EPhotoCacheContactLoader *self = E_PHOTO_CACHE_CONTACT_LOADER (object);

	g_hash_table_destroy (self->priv->photo_sources);

	/* Chain up to parent's finalize() method. */
	G_OBJECT_CLASS (e_photo_cache_contact_loader_parent_class)->finalize (object);
}

static void
photo_cache_contact_loader_constructed (GObject *object)
{
	EPhotoCacheContactLoader *loader;
	EPhotoCache *photo_cache;
	EClientCache *client_cache;
	ESourceRegistry *registry;
	GList *list, *link;
	const gchar *extension_name;
	gulong handler_id;

	/* Chain up to parent's constructed() method. */
	G_OBJECT_CLASS (e_photo_cache_contact_loader_parent_class)->constructed (object);

	loader = E_PHOTO_CACHE_CONTACT_LOADER (object);
	photo_cache = photo_cache_contact_loader_get_photo_cache (loader);

	client_cache = e_photo_cache_ref_client_cache (photo_cache);
	registry = e_client_cache_ref_registry (client_cache);

	extension_name = E_SOURCE_EXTENSION_ADDRESS_BOOK;
	list = e_source_registry_list_sources (registry, extension_name);

	for (link = list; link != NULL; link = g_list_next (link)) {
		ESource *source = E_SOURCE (link->data);
		photo_cache_contact_loader_add_source (loader, source);
	}

	g_list_free_full (list, (GDestroyNotify) g_object_unref);

	loader->priv->registry = g_object_ref (registry);

	handler_id = g_signal_connect (
		registry, "source-added",
		G_CALLBACK (photo_cache_contact_loader_source_added_cb),
		loader);
	loader->priv->source_added_handler_id = handler_id;

	handler_id = g_signal_connect (
		registry, "source-removed",
		G_CALLBACK (photo_cache_contact_loader_source_removed_cb),
		loader);
	loader->priv->source_removed_handler_id = handler_id;

	g_object_unref (client_cache);
	g_object_unref (registry);
}

static void
e_photo_cache_contact_loader_class_init (EPhotoCacheContactLoaderClass *class)
{
	GObjectClass *object_class;
	EExtensionClass *extension_class;

	object_class = G_OBJECT_CLASS (class);
	object_class->dispose = photo_cache_contact_loader_dispose;
	object_class->finalize = photo_cache_contact_loader_finalize;
	object_class->constructed = photo_cache_contact_loader_constructed;

	extension_class = E_EXTENSION_CLASS (class);
	extension_class->extensible_type = E_TYPE_PHOTO_CACHE;
}

static void
e_photo_cache_contact_loader_class_finalize (EPhotoCacheContactLoaderClass *class)
{
}

static void
e_photo_cache_contact_loader_init (EPhotoCacheContactLoader *loader)
{
	GHashTable *photo_sources;

	photo_sources = g_hash_table_new_full (
		(GHashFunc) e_source_hash,
		(GEqualFunc) e_source_equal,
		(GDestroyNotify) g_object_unref,
		(GDestroyNotify) g_object_unref);

	loader->priv = e_photo_cache_contact_loader_get_instance_private (loader);
	loader->priv->photo_sources = photo_sources;
}

void
e_photo_cache_contact_loader_type_register (GTypeModule *type_module)
{
	/* XXX G_DEFINE_DYNAMIC_TYPE declares a static type registration
	 *     function, so we have to wrap it with a public function in
	 *     order to register types from a separate compilation unit. */
	e_photo_cache_contact_loader_register_type (type_module);
}

