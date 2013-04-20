/*
 * e-photo-cache.c
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

/**
 * SECTION: e-photo-cache
 * @include: e-util/e-util.h
 * @short_description: Search for photos by email address
 *
 * #EPhotoCache helps search for contact photo or logo images associated
 * with an email address.
 *
 * A limited internal cache is employed to speed up searches for recently
 * searched email addresses.  The exact caching semantics are private and
 * subject to change.
 **/

#include "e-photo-cache.h"

#include <string.h>
#include <libebackend/libebackend.h>

#define E_PHOTO_CACHE_GET_PRIVATE(obj) \
	(G_TYPE_INSTANCE_GET_PRIVATE \
	((obj), E_TYPE_PHOTO_CACHE, EPhotoCachePrivate))

/* How many email addresses we track at once, regardless of whether
 * the email address has a photo.  As new cache entries are added, we
 * discard the least recently accessed entries to keep the cache size
 * within the limit. */
#define MAX_CACHE_SIZE 20

typedef struct _AsyncContext AsyncContext;
typedef struct _PhotoData PhotoData;

struct _EPhotoCachePrivate {
	EClientCache *client_cache;

	GHashTable *photo_ht;
	GQueue photo_ht_keys;
	GMutex photo_ht_lock;
};

struct _AsyncContext {
	gchar *email_address;
	GInputStream *input_stream;
};

struct _PhotoData {
	volatile gint ref_count;
	GMutex lock;
	EContactPhoto *photo;
	gboolean photo_is_set;
};

enum {
	PROP_0,
	PROP_CLIENT_CACHE
};

G_DEFINE_TYPE_WITH_CODE (
	EPhotoCache,
	e_photo_cache,
	G_TYPE_OBJECT,
	G_IMPLEMENT_INTERFACE (
		E_TYPE_EXTENSIBLE, NULL))

static void
async_context_free (AsyncContext *async_context)
{
	g_free (async_context->email_address);

	if (async_context->input_stream != NULL)
		g_object_unref (async_context->input_stream);

	g_slice_free (AsyncContext, async_context);
}

static PhotoData *
photo_data_new (void)
{
	PhotoData *photo_data;

	photo_data = g_slice_new0 (PhotoData);
	photo_data->ref_count = 1;

	g_mutex_init (&photo_data->lock);

	return photo_data;
}

static PhotoData *
photo_data_ref (PhotoData *photo_data)
{
	g_return_val_if_fail (photo_data != NULL, NULL);
	g_return_val_if_fail (photo_data->ref_count > 0, NULL);

	g_atomic_int_inc (&photo_data->ref_count);

	return photo_data;
}

static void
photo_data_unref (PhotoData *photo_data)
{
	g_return_if_fail (photo_data != NULL);
	g_return_if_fail (photo_data->ref_count > 0);

	if (g_atomic_int_dec_and_test (&photo_data->ref_count)) {
		if (photo_data->photo != NULL)
			e_contact_photo_free (photo_data->photo);

		g_mutex_clear (&photo_data->lock);

		g_slice_free (PhotoData, photo_data);
	}
}

static gboolean
photo_data_dup_photo (PhotoData *photo_data,
                      EContactPhoto **out_photo)
{
	gboolean photo_is_set;

	g_return_val_if_fail (out_photo != NULL, FALSE);

	g_mutex_lock (&photo_data->lock);

	if (photo_data->photo != NULL)
		*out_photo = e_contact_photo_copy (photo_data->photo);
	else
		*out_photo = NULL;

	photo_is_set = photo_data->photo_is_set;

	g_mutex_unlock (&photo_data->lock);

	return photo_is_set;
}

static void
photo_data_set_photo (PhotoData *photo_data,
                      EContactPhoto *photo)
{
	g_mutex_lock (&photo_data->lock);

	if (photo_data->photo != NULL) {
		e_contact_photo_free (photo_data->photo);
		photo_data->photo = NULL;
	}

	if (photo != NULL)
		photo_data->photo = e_contact_photo_copy (photo);

	photo_data->photo_is_set = TRUE;

	g_mutex_unlock (&photo_data->lock);
}

static gchar *
photo_ht_normalize_key (const gchar *email_address)
{
	gchar *lowercase_email_address;
	gchar *collation_key;

	lowercase_email_address = g_utf8_strdown (email_address, -1);
	collation_key = g_utf8_collate_key (lowercase_email_address, -1);
	g_free (lowercase_email_address);

	return collation_key;
}

static PhotoData *
photo_ht_lookup (EPhotoCache *photo_cache,
                 const gchar *email_address)
{
	GHashTable *photo_ht;
	GQueue *photo_ht_keys;
	PhotoData *photo_data;
	gchar *key;

	g_return_val_if_fail (email_address != NULL, NULL);

	photo_ht = photo_cache->priv->photo_ht;
	photo_ht_keys = &photo_cache->priv->photo_ht_keys;

	key = photo_ht_normalize_key (email_address);

	g_mutex_lock (&photo_cache->priv->photo_ht_lock);

	photo_data = g_hash_table_lookup (photo_ht, key);

	if (photo_data != NULL) {
		GList *link;

		photo_data_ref (photo_data);

		/* Move the key to the head of the MRU queue. */
		link = g_queue_find_custom (
			photo_ht_keys, key,
			(GCompareFunc) strcmp);
		if (link != NULL) {
			g_queue_unlink (photo_ht_keys, link);
			g_queue_push_head_link (photo_ht_keys, link);
		}
	} else {
		photo_data = photo_data_new ();

		g_hash_table_insert (
			photo_ht, g_strdup (key),
			photo_data_ref (photo_data));

		/* Push the key to the head of the MRU queue. */
		g_queue_push_head (photo_ht_keys, g_strdup (key));

		/* Trim the cache if necessary. */
		while (g_queue_get_length (photo_ht_keys) > MAX_CACHE_SIZE) {
			gchar *oldest_key;

			oldest_key = g_queue_pop_tail (photo_ht_keys);
			g_hash_table_remove (photo_ht, oldest_key);
			g_free (oldest_key);
		}
	}

	/* Hash table and queue sizes should be equal at all times. */
	g_warn_if_fail (
		g_hash_table_size (photo_ht) ==
		g_queue_get_length (photo_ht_keys));

	g_mutex_unlock (&photo_cache->priv->photo_ht_lock);

	g_free (key);

	return photo_data;
}

static gboolean
photo_ht_remove (EPhotoCache *photo_cache,
                 const gchar *email_address)
{
	GHashTable *photo_ht;
	GQueue *photo_ht_keys;
	gchar *key;
	gboolean removed = FALSE;

	g_return_val_if_fail (email_address != NULL, FALSE);

	photo_ht = photo_cache->priv->photo_ht;
	photo_ht_keys = &photo_cache->priv->photo_ht_keys;

	key = photo_ht_normalize_key (email_address);

	g_mutex_lock (&photo_cache->priv->photo_ht_lock);

	if (g_hash_table_remove (photo_ht, key)) {
		GList *link;

		link = g_queue_find_custom (
			photo_ht_keys, key,
			(GCompareFunc) strcmp);
		if (link != NULL) {
			g_free (link->data);
			g_queue_delete_link (photo_ht_keys, link);
			removed = TRUE;
		}
	}

	/* Hash table and queue sizes should be equal at all times. */
	g_warn_if_fail (
		g_hash_table_size (photo_ht) ==
		g_queue_get_length (photo_ht_keys));

	g_mutex_unlock (&photo_cache->priv->photo_ht_lock);

	g_free (key);

	return removed;
}

static void
photo_ht_remove_all (EPhotoCache *photo_cache)
{
	GHashTable *photo_ht;
	GQueue *photo_ht_keys;

	photo_ht = photo_cache->priv->photo_ht;
	photo_ht_keys = &photo_cache->priv->photo_ht_keys;

	g_mutex_lock (&photo_cache->priv->photo_ht_lock);

	g_hash_table_remove_all (photo_ht);

	while (!g_queue_is_empty (photo_ht_keys))
		g_free (g_queue_pop_head (photo_ht_keys));

	g_mutex_unlock (&photo_cache->priv->photo_ht_lock);
}

static EContactPhoto *
photo_cache_extract_photo (EContact *contact)
{
	EContactPhoto *photo;

	photo = e_contact_get (contact, E_CONTACT_PHOTO);
	if (photo == NULL)
		photo = e_contact_get (contact, E_CONTACT_LOGO);

	return photo;
}

static gboolean
photo_cache_find_contacts (EPhotoCache *photo_cache,
                           const gchar *email_address,
                           GCancellable *cancellable,
                           GQueue *out_contacts,
                           GError **error)
{
	EClientCache *client_cache;
	ESourceRegistry *registry;
	EBookQuery *book_query;
	GList *list, *link;
	const gchar *extension_name;
	gchar *book_query_string;
	gboolean success = TRUE;

	book_query = e_book_query_field_test (
		E_CONTACT_EMAIL, E_BOOK_QUERY_IS, email_address);
	book_query_string = e_book_query_to_string (book_query);
	e_book_query_unref (book_query);

	client_cache = e_photo_cache_ref_client_cache (photo_cache);
	registry = e_client_cache_ref_registry (client_cache);

	extension_name = E_SOURCE_EXTENSION_ADDRESS_BOOK;
	list = e_source_registry_list_enabled (registry, extension_name);

	for (link = list; link != NULL; link = g_list_next (link)) {
		ESource *source = E_SOURCE (link->data);
		EClient *client;
		GSList *contact_list = NULL;
		GError *local_error = NULL;

		client = e_client_cache_get_client_sync (
			client_cache,
			source, extension_name,
			cancellable, &local_error);

		if (local_error != NULL) {
			g_warn_if_fail (client == NULL);
			if (g_queue_is_empty (out_contacts)) {
				g_propagate_error (error, local_error);
				success = FALSE;
			} else {
				/* Clear the error if we already
				 * have matching contacts queued. */
				g_clear_error (&local_error);
			}
			break;
		}

		e_book_client_get_contacts_sync (
			E_BOOK_CLIENT (client), book_query_string,
			&contact_list, cancellable, &local_error);

		g_object_unref (client);

		if (local_error != NULL) {
			g_warn_if_fail (contact_list == NULL);
			if (g_queue_is_empty (out_contacts)) {
				g_propagate_error (error, local_error);
				success = FALSE;
			} else {
				/* Clear the error if we already
				 * have matching contacts queued. */
				g_clear_error (&local_error);
			}
			break;
		}

		while (contact_list != NULL) {
			EContact *contact;

			/* Transfer ownership to queue. */
			contact = E_CONTACT (contact_list->data);
			g_queue_push_tail (out_contacts, contact);

			contact_list = g_slist_delete_link (
				contact_list, contact_list);
		}
	}

	g_list_free_full (list, (GDestroyNotify) g_object_unref);

	g_object_unref (client_cache);
	g_object_unref (registry);

	g_free (book_query_string);

	return success;
}

static GInputStream *
photo_cache_new_stream_from_photo (EContactPhoto *photo,
                                   GCancellable *cancellable,
                                   GError **error)
{
	GInputStream *stream = NULL;

	/* Stream takes ownership of the inlined data. */
	if (photo->type == E_CONTACT_PHOTO_TYPE_INLINED) {
		stream = g_memory_input_stream_new_from_data (
			photo->data.inlined.data,
			photo->data.inlined.length,
			(GDestroyNotify) g_free);
		photo->data.inlined.data = NULL;
		photo->data.inlined.length = 0;

	} else {
		GFileInputStream *file_stream;
		GFile *file;

		file = g_file_new_for_uri (photo->data.uri);

		/* XXX Return type should have been GInputStream. */
		file_stream = g_file_read (file, cancellable, error);
		if (file_stream != NULL)
			stream = G_INPUT_STREAM (file_stream);

		g_object_unref (file);
	}

	return stream;
}

static void
photo_cache_set_client_cache (EPhotoCache *photo_cache,
                              EClientCache *client_cache)
{
	g_return_if_fail (E_IS_CLIENT_CACHE (client_cache));
	g_return_if_fail (photo_cache->priv->client_cache == NULL);

	photo_cache->priv->client_cache = g_object_ref (client_cache);
}

static void
photo_cache_set_property (GObject *object,
                          guint property_id,
                          const GValue *value,
                          GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_CLIENT_CACHE:
			photo_cache_set_client_cache (
				E_PHOTO_CACHE (object),
				g_value_get_object (value));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
photo_cache_get_property (GObject *object,
                          guint property_id,
                          GValue *value,
                          GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_CLIENT_CACHE:
			g_value_take_object (
				value,
				e_photo_cache_ref_client_cache (
				E_PHOTO_CACHE (object)));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
photo_cache_dispose (GObject *object)
{
	EPhotoCachePrivate *priv;

	priv = E_PHOTO_CACHE_GET_PRIVATE (object);

	g_clear_object (&priv->client_cache);

	photo_ht_remove_all (E_PHOTO_CACHE (object));

	/* Chain up to parent's dispose() method. */
	G_OBJECT_CLASS (e_photo_cache_parent_class)->dispose (object);
}

static void
photo_cache_finalize (GObject *object)
{
	EPhotoCachePrivate *priv;

	priv = E_PHOTO_CACHE_GET_PRIVATE (object);

	g_hash_table_destroy (priv->photo_ht);

	/* Chain up to parent's finalize() method. */
	G_OBJECT_CLASS (e_photo_cache_parent_class)->finalize (object);
}

static void
photo_cache_constructed (GObject *object)
{
	/* Chain up to parent's constructed() method. */
	G_OBJECT_CLASS (e_photo_cache_parent_class)->constructed (object);

	e_extensible_load_extensions (E_EXTENSIBLE (object));
}

static void
e_photo_cache_class_init (EPhotoCacheClass *class)
{
	GObjectClass *object_class;

	g_type_class_add_private (class, sizeof (EPhotoCachePrivate));

	object_class = G_OBJECT_CLASS (class);
	object_class->set_property = photo_cache_set_property;
	object_class->get_property = photo_cache_get_property;
	object_class->dispose = photo_cache_dispose;
	object_class->finalize = photo_cache_finalize;
	object_class->constructed = photo_cache_constructed;

	/**
	 * EPhotoCache:client-cache:
	 *
	 * Cache of shared #EClient instances.
	 **/
	g_object_class_install_property (
		object_class,
		PROP_CLIENT_CACHE,
		g_param_spec_object (
			"client-cache",
			"Client Cache",
			"Cache of shared EClient instances",
			E_TYPE_CLIENT_CACHE,
			G_PARAM_READWRITE |
			G_PARAM_CONSTRUCT_ONLY |
			G_PARAM_STATIC_STRINGS));
}

static void
e_photo_cache_init (EPhotoCache *photo_cache)
{
	GHashTable *photo_ht;

	photo_ht = g_hash_table_new_full (
		(GHashFunc) g_str_hash,
		(GEqualFunc) g_str_equal,
		(GDestroyNotify) g_free,
		(GDestroyNotify) photo_data_unref);

	photo_cache->priv = E_PHOTO_CACHE_GET_PRIVATE (photo_cache);
	photo_cache->priv->photo_ht = photo_ht;

	g_mutex_init (&photo_cache->priv->photo_ht_lock);
}

/**
 * e_photo_cache_new:
 * @client_cache: an #EClientCache
 *
 * Creates a new #EPhotoCache instance.
 *
 * Returns: an #EPhotoCache
 **/
EPhotoCache *
e_photo_cache_new (EClientCache *client_cache)
{
	g_return_val_if_fail (E_IS_CLIENT_CACHE (client_cache), NULL);

	return g_object_new (
		E_TYPE_PHOTO_CACHE,
		"client-cache", client_cache, NULL);
}

/**
 * e_photo_cache_ref_client_cache:
 * @photo_cache: an #EPhotoCache
 *
 * Returns the #EClientCache passed to e_photo_cache_new().
 *
 * The returned #EClientCache is referenced for thread-safety and must be
 * unreferenced with g_object_unref() when finished with it.
 *
 * Returns: an #EClientCache
 **/
EClientCache *
e_photo_cache_ref_client_cache (EPhotoCache *photo_cache)
{
	g_return_val_if_fail (E_IS_PHOTO_CACHE (photo_cache), NULL);

	return g_object_ref (photo_cache->priv->client_cache);
}

/**
 * e_photo_cache_get_photo_sync:
 * @photo_cache: an #EPhotoCache
 * @email_address: an email address
 * @cancellable: optional #GCancellable object, or %NULL
 * @out_stream: return location for a #GInputStream, or %NULL
 * @error: return location for a #GError, or %NULL
 *
 * Searches enabled address books for a contact photo or logo associated
 * with @email_address.
 *
 * If a match is found, a #GInputStream from which to read image data is
 * returned through the @out_stream return location.  If no match is found,
 * the @out_stream return location is set to %NULL.
 *
 * The return value indicates whether the search completed successfully,
 * not whether a match was found.  If an error occurs, the function will
 * set @error and return %FALSE.
 *
 * Returns: whether the search completed successfully
 **/
gboolean
e_photo_cache_get_photo_sync (EPhotoCache *photo_cache,
                              const gchar *email_address,
                              GCancellable *cancellable,
                              GInputStream **out_stream,
                              GError **error)
{
	EContactPhoto *photo = NULL;
	EClientCache *client_cache;
	GQueue queue = G_QUEUE_INIT;
	PhotoData *photo_data;
	gboolean success = TRUE;

	g_return_val_if_fail (E_IS_PHOTO_CACHE (photo_cache), FALSE);
	g_return_val_if_fail (email_address != NULL, FALSE);

	client_cache = e_photo_cache_ref_client_cache (photo_cache);

	/* Try the cache first. */
	photo_data = photo_ht_lookup (photo_cache, email_address);
	if (photo_data_dup_photo (photo_data, &photo))
		goto exit;

	/* Find contacts with a matching email address. */
	success = photo_cache_find_contacts (
		photo_cache, email_address,
		cancellable, &queue, error);
	if (!success) {
		g_warn_if_fail (g_queue_is_empty (&queue));
		goto exit;
	}

	/* Extract the first available photo from contacts. */
	while (!g_queue_is_empty (&queue)) {
		EContact *contact;

		contact = g_queue_pop_head (&queue);
		if (photo == NULL)
			photo = photo_cache_extract_photo (contact);
		g_object_unref (contact);
	}

	/* Passing a NULL photo here is fine.  We want to cache not
	 * only the photo itself, but whether a photo was found for
	 * this email address. */
	photo_data_set_photo (photo_data, photo);

exit:
	photo_data_unref (photo_data);

	g_object_unref (client_cache);

	/* Try opening an input stream to the photo data. */
	if (photo != NULL) {
		GInputStream *stream;

		stream = photo_cache_new_stream_from_photo (
			photo, cancellable, error);

		success = (stream != NULL);

		if (stream != NULL) {
			if (out_stream != NULL)
				*out_stream = g_object_ref (stream);
			g_object_unref (stream);
		}

		e_contact_photo_free (photo);
	}

	return success;
}

/* Helper for e_photo_cache_get_photo() */
static void
photo_cache_get_photo_thread (GSimpleAsyncResult *simple,
                              GObject *source_object,
                              GCancellable *cancellable)
{
	AsyncContext *async_context;
	GError *error = NULL;

	async_context = g_simple_async_result_get_op_res_gpointer (simple);

	e_photo_cache_get_photo_sync (
		E_PHOTO_CACHE (source_object),
		async_context->email_address,
		cancellable,
		&async_context->input_stream,
		&error);

	if (error != NULL)
		g_simple_async_result_take_error (simple, error);
}

/**
 * e_photo_cache_get_photo:
 * @photo_cache: an #EPhotoCache
 * @email_address: an email address
 * @cancellable: optional #GCancellable object, or %NULL
 * @callback: a #GAsyncReadyCallback to call when the request is satisfied
 * @user_data: data to pass to the callback function
 *
 * Asynchronously searches enabled address books for a contact photo or logo
 * associated with @email_address.
 *
 * When the operation is finished, @callback will be called.  You can then
 * call e_photo_cache_get_photo_finish() to get the result of the operation.
 **/
void
e_photo_cache_get_photo (EPhotoCache *photo_cache,
                         const gchar *email_address,
                         GCancellable *cancellable,
                         GAsyncReadyCallback callback,
                         gpointer user_data)
{
	GSimpleAsyncResult *simple;
	AsyncContext *async_context;

	g_return_if_fail (E_IS_PHOTO_CACHE (photo_cache));
	g_return_if_fail (email_address != NULL);

	async_context = g_slice_new0 (AsyncContext);
	async_context->email_address = g_strdup (email_address);

	simple = g_simple_async_result_new (
		G_OBJECT (photo_cache), callback,
		user_data, e_photo_cache_get_photo);

	g_simple_async_result_set_check_cancellable (simple, cancellable);

	g_simple_async_result_set_op_res_gpointer (
		simple, async_context, (GDestroyNotify) async_context_free);

	g_simple_async_result_run_in_thread (
		simple, photo_cache_get_photo_thread,
		G_PRIORITY_DEFAULT, cancellable);

	g_object_unref (simple);
}

/**
 * e_photo_cache_get_photo_finish:
 * @photo_cache: an #EPhotoCache
 * @result: a #GAsyncResult
 * @out_stream: return location for a #GInputStream, or %NULL
 * @error: return location for a #GError, or %NULL
 *
 * Finishes the operation started with e_photo_cache_get_photo().
 *
 * If a match was found, a #GInputStream from which to read image data is
 * returned through the @out_stream return location.  If no match was found,
 * the @out_stream return location is set to %NULL.
 *
 * The return value indicates whether the search completed successfully,
 * not whether a match was found.  If an error occurred, the function will
 * set @error and return %FALSE.
 *
 * Returns: whether the search completed successfully
 **/
gboolean
e_photo_cache_get_photo_finish (EPhotoCache *photo_cache,
                                GAsyncResult *result,
                                GInputStream **out_stream,
                                GError **error)
{
	GSimpleAsyncResult *simple;
	AsyncContext *async_context;

	g_return_val_if_fail (
		g_simple_async_result_is_valid (
		result, G_OBJECT (photo_cache),
		e_photo_cache_get_photo), FALSE);

	simple = G_SIMPLE_ASYNC_RESULT (result);
	async_context = g_simple_async_result_get_op_res_gpointer (simple);

	if (g_simple_async_result_propagate_error (simple, error))
		return FALSE;

	if (out_stream != NULL) {
		*out_stream = async_context->input_stream;
		async_context->input_stream = NULL;
	}

	return TRUE;
}

/**
 * e_photo_cache_remove:
 * @photo_cache: an #EPhotoCache
 * @email_address: an email address
 *
 * Removes the cache entry for @email_address, if such an entry exists.
 **/
gboolean
e_photo_cache_remove (EPhotoCache *photo_cache,
                      const gchar *email_address)
{
	g_return_val_if_fail (E_IS_PHOTO_CACHE (photo_cache), FALSE);
	g_return_val_if_fail (email_address != NULL, FALSE);

	return photo_ht_remove (photo_cache, email_address);
}

