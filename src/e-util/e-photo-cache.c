/*
 * e-photo-cache.c
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

/**
 * SECTION: e-photo-cache
 * @include: e-util/e-util.h
 * @short_description: Search for photos by email address
 *
 * #EPhotoCache finds photos associated with an email address.
 *
 * A limited internal cache is employed to speed up frequently searched
 * email addresses.  The exact caching semantics are private and subject
 * to change.
 **/

#include "evolution-config.h"

#include <string.h>
#include <libebackend/libebackend.h>

#include <e-util/e-data-capture.h>

#include "e-simple-async-result.h"
#include "e-photo-cache.h"

/* How long (in seconds) to hold out for a hit from the highest
 * priority photo source, after which we settle for what we have. */
#define ASYNC_TIMEOUT_SECONDS 3.0

/* How many email addresses we track at once, regardless of whether
 * the email address has a photo.  As new cache entries are added, we
 * discard the least recently accessed entries to keep the cache size
 * within the limit. */
#define MAX_CACHE_SIZE 20

#define ERROR_IS_CANCELLED(error) \
	(g_error_matches ((error), G_IO_ERROR, G_IO_ERROR_CANCELLED))

typedef struct _AsyncContext AsyncContext;
typedef struct _AsyncSubtask AsyncSubtask;
typedef struct _DataCaptureClosure DataCaptureClosure;
typedef struct _PhotoData PhotoData;

struct _EPhotoCachePrivate {
	EClientCache *client_cache;
	GMainContext *main_context;

	GHashTable *photo_ht;
	GQueue photo_ht_keys;
	GMutex photo_ht_lock;

	GHashTable *sources_ht;
	GMutex sources_ht_lock;
};

struct _AsyncContext {
	GMutex lock;
	GTimer *timer;
	GHashTable *subtasks;
	GQueue results;
	GInputStream *stream;
	GConverter *data_capture;

	GCancellable *cancellable;
	gulong cancelled_handler_id;
};

struct _AsyncSubtask {
	volatile gint ref_count;
	EPhotoSource *photo_source;
	ESimpleAsyncResult *simple;
	GCancellable *cancellable;
	GInputStream *stream;
	gint priority;
	GError *error;
};

struct _DataCaptureClosure {
	GWeakRef photo_cache;
	gchar *email_address;
};

struct _PhotoData {
	volatile gint ref_count;
	GMutex lock;
	GBytes *bytes;
};

enum {
	PROP_0,
	PROP_CLIENT_CACHE
};

/* Forward Declarations */
static void	async_context_cancel_subtasks	(AsyncContext *async_context);

G_DEFINE_TYPE_WITH_CODE (EPhotoCache, e_photo_cache, G_TYPE_OBJECT,
	G_ADD_PRIVATE (EPhotoCache)
	G_IMPLEMENT_INTERFACE (E_TYPE_EXTENSIBLE, NULL))

static AsyncSubtask *
async_subtask_new (EPhotoSource *photo_source,
                   ESimpleAsyncResult *simple)
{
	AsyncSubtask *async_subtask;

	async_subtask = g_slice_new0 (AsyncSubtask);
	async_subtask->ref_count = 1;
	async_subtask->photo_source = g_object_ref (photo_source);
	async_subtask->simple = g_object_ref (simple);
	async_subtask->cancellable = g_cancellable_new ();
	async_subtask->priority = G_PRIORITY_DEFAULT;

	return async_subtask;
}

static AsyncSubtask *
async_subtask_ref (AsyncSubtask *async_subtask)
{
	g_return_val_if_fail (async_subtask != NULL, NULL);
	g_return_val_if_fail (async_subtask->ref_count > 0, NULL);

	g_atomic_int_inc (&async_subtask->ref_count);

	return async_subtask;
}

static void
async_subtask_unref (AsyncSubtask *async_subtask)
{
	g_return_if_fail (async_subtask != NULL);
	g_return_if_fail (async_subtask->ref_count > 0);

	if (g_atomic_int_dec_and_test (&async_subtask->ref_count)) {

		/* Ignore cancellations. */
		if (ERROR_IS_CANCELLED (async_subtask->error))
			g_clear_error (&async_subtask->error);

		/* Leave a breadcrumb on the console
		 * about unpropagated subtask errors. */
		if (async_subtask->error != NULL) {
			g_warning (
				"%s: Unpropagated error in %s subtask: %s",
				__FILE__,
				G_OBJECT_TYPE_NAME (
				async_subtask->photo_source),
				async_subtask->error->message);
			g_error_free (async_subtask->error);
		}

		g_clear_object (&async_subtask->photo_source);
		g_clear_object (&async_subtask->simple);
		g_clear_object (&async_subtask->cancellable);
		g_clear_object (&async_subtask->stream);

		g_slice_free (AsyncSubtask, async_subtask);
	}
}

static gboolean
async_subtask_cancel_idle_cb (gpointer user_data)
{
	AsyncSubtask *async_subtask = user_data;

	g_cancellable_cancel (async_subtask->cancellable);

	return FALSE;
}

static gint
async_subtask_compare (gconstpointer a,
                       gconstpointer b)
{
	const AsyncSubtask *subtask_a = a;
	const AsyncSubtask *subtask_b = b;

	/* Without error is always less than with error. */

	if (subtask_a->error != NULL && subtask_b->error != NULL)
		return 0;

	if (subtask_a->error == NULL && subtask_b->error != NULL)
		return -1;

	if (subtask_a->error != NULL && subtask_b->error == NULL)
		return 1;

	if (subtask_a->priority == subtask_b->priority)
		return 0;

	return (subtask_a->priority < subtask_b->priority) ? -1 : 1;
}

static void
async_subtask_complete (AsyncSubtask *async_subtask)
{
	ESimpleAsyncResult *simple;
	AsyncContext *async_context;
	gboolean cancel_subtasks = FALSE;
	gdouble seconds_elapsed;

	simple = async_subtask->simple;
	async_context = e_simple_async_result_get_op_pointer (simple);

	g_mutex_lock (&async_context->lock);

	seconds_elapsed = g_timer_elapsed (async_context->timer, NULL);

	/* Discard successfully completed subtasks with no match found.
	 * Keep failed subtasks around so we have a GError to propagate
	 * if we need one, but those go on the end of the queue. */

	if (async_subtask->stream != NULL) {
		g_queue_insert_sorted (
			&async_context->results,
			async_subtask_ref (async_subtask),
			(GCompareDataFunc) async_subtask_compare,
			NULL);

		/* If enough seconds have elapsed, just take the highest
		 * priority input stream we have.  Cancel the unfinished
		 * subtasks and let them complete with an error. */
		if (seconds_elapsed > ASYNC_TIMEOUT_SECONDS)
			cancel_subtasks = TRUE;

	} else if (async_subtask->error != NULL) {
		g_queue_push_tail (
			&async_context->results,
			async_subtask_ref (async_subtask));
	}

	g_hash_table_remove (async_context->subtasks, async_subtask);

	if (g_hash_table_size (async_context->subtasks) > 0) {
		/* Let the remaining subtasks finish. */
		goto exit;
	}

	/* The queue should be ordered now such that subtasks
	 * with input streams are before subtasks with errors.
	 * So just evaluate the first subtask on the queue. */

	async_subtask = g_queue_pop_head (&async_context->results);

	if (async_subtask != NULL) {
		if (async_subtask->stream != NULL) {
			async_context->stream =
				g_converter_input_stream_new (
					async_subtask->stream,
					async_context->data_capture);
		}

		if (async_subtask->error != NULL) {
			e_simple_async_result_take_error (
				simple, async_subtask->error);
			async_subtask->error = NULL;
		}

		async_subtask_unref (async_subtask);
	}

	e_simple_async_result_complete_idle (simple);

exit:
	g_mutex_unlock (&async_context->lock);

	if (cancel_subtasks) {
		/* Call this after the mutex is unlocked. */
		async_context_cancel_subtasks (async_context);
	}
}

static void
async_context_cancelled_cb (GCancellable *cancellable,
                            AsyncContext *async_context)
{
	async_context_cancel_subtasks (async_context);
}

static AsyncContext *
async_context_new (EDataCapture *data_capture,
                   GCancellable *cancellable)
{
	AsyncContext *async_context;

	async_context = g_slice_new0 (AsyncContext);
	g_mutex_init (&async_context->lock);
	async_context->timer = g_timer_new ();

	async_context->subtasks = g_hash_table_new_full (
		(GHashFunc) g_direct_hash,
		(GEqualFunc) g_direct_equal,
		(GDestroyNotify) async_subtask_unref,
		(GDestroyNotify) NULL);

	async_context->data_capture = G_CONVERTER (g_object_ref (data_capture));

	if (G_IS_CANCELLABLE (cancellable)) {
		gulong handler_id;

		async_context->cancellable = g_object_ref (cancellable);

		handler_id = g_cancellable_connect (
			async_context->cancellable,
			G_CALLBACK (async_context_cancelled_cb),
			async_context,
			(GDestroyNotify) NULL);
		async_context->cancelled_handler_id = handler_id;
	}

	return async_context;
}

static void
async_context_free (AsyncContext *async_context)
{
	/* Do this first so the callback won't fire
	 * while we're dismantling the AsyncContext. */
	if (async_context->cancelled_handler_id > 0)
		g_cancellable_disconnect (
			async_context->cancellable,
			async_context->cancelled_handler_id);

	g_mutex_clear (&async_context->lock);
	g_timer_destroy (async_context->timer);

	g_hash_table_destroy (async_context->subtasks);

	g_clear_object (&async_context->stream);
	g_clear_object (&async_context->data_capture);
	g_clear_object (&async_context->cancellable);

	g_slice_free (AsyncContext, async_context);
}

static void
async_context_cancel_subtasks (AsyncContext *async_context)
{
	GMainContext *main_context;
	GList *list, *link;

	main_context = g_main_context_ref_thread_default ();

	g_mutex_lock (&async_context->lock);

	list = g_hash_table_get_keys (async_context->subtasks);

	/* XXX Cancel subtasks from idle callbacks to make sure we don't
	 *     finalize the ESimpleAsyncResult during a "cancelled" signal
	 *     emission from the main task's GCancellable.  That will make
	 *     g_cancellable_disconnect() in async_context_free() deadlock. */
	for (link = list; link != NULL; link = g_list_next (link)) {
		AsyncSubtask *async_subtask = link->data;
		GSource *idle_source;

		idle_source = g_idle_source_new ();
		g_source_set_priority (idle_source, G_PRIORITY_HIGH_IDLE);
		g_source_set_callback (
			idle_source,
			async_subtask_cancel_idle_cb,
			async_subtask_ref (async_subtask),
			(GDestroyNotify) async_subtask_unref);
		g_source_attach (idle_source, main_context);
		g_source_unref (idle_source);
	}

	g_list_free (list);

	g_mutex_unlock (&async_context->lock);

	g_main_context_unref (main_context);
}

static DataCaptureClosure *
data_capture_closure_new (EPhotoCache *photo_cache,
                          const gchar *email_address)
{
	DataCaptureClosure *closure;

	closure = g_slice_new0 (DataCaptureClosure);
	g_weak_ref_set (&closure->photo_cache, photo_cache);
	closure->email_address = g_strdup (email_address);

	return closure;
}

static void
data_capture_closure_free (DataCaptureClosure *closure)
{
	g_weak_ref_set (&closure->photo_cache, NULL);
	g_free (closure->email_address);

	g_slice_free (DataCaptureClosure, closure);
}

static PhotoData *
photo_data_new (GBytes *bytes)
{
	PhotoData *photo_data;

	photo_data = g_slice_new0 (PhotoData);
	photo_data->ref_count = 1;
	g_mutex_init (&photo_data->lock);

	if (bytes != NULL)
		photo_data->bytes = g_bytes_ref (bytes);

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
		g_mutex_clear (&photo_data->lock);
		if (photo_data->bytes != NULL)
			g_bytes_unref (photo_data->bytes);
		g_slice_free (PhotoData, photo_data);
	}
}

static GBytes *
photo_data_ref_bytes (PhotoData *photo_data)
{
	GBytes *bytes = NULL;

	g_mutex_lock (&photo_data->lock);

	if (photo_data->bytes != NULL)
		bytes = g_bytes_ref (photo_data->bytes);

	g_mutex_unlock (&photo_data->lock);

	return bytes;
}

static void
photo_data_set_bytes (PhotoData *photo_data,
                      GBytes *bytes)
{
	g_mutex_lock (&photo_data->lock);

	g_clear_pointer (&photo_data->bytes, g_bytes_unref);

	if (bytes != NULL)
		photo_data->bytes = g_bytes_ref (bytes);

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

static void
photo_ht_insert (EPhotoCache *photo_cache,
                 const gchar *email_address,
                 GBytes *bytes)
{
	GHashTable *photo_ht;
	GQueue *photo_ht_keys;
	PhotoData *photo_data;
	gchar *key;

	g_return_if_fail (email_address != NULL);

	photo_ht = photo_cache->priv->photo_ht;
	photo_ht_keys = &photo_cache->priv->photo_ht_keys;

	key = photo_ht_normalize_key (email_address);

	g_mutex_lock (&photo_cache->priv->photo_ht_lock);

	photo_data = g_hash_table_lookup (photo_ht, key);

	if (photo_data != NULL) {
		GList *link;

		/* Replace the old photo data if we have new photo
		 * data, otherwise leave the old photo data alone. */
		if (bytes != NULL)
			photo_data_set_bytes (photo_data, bytes);

		/* Move the key to the head of the MRU queue. */
		link = g_queue_find_custom (
			photo_ht_keys, key,
			(GCompareFunc) strcmp);
		if (link != NULL) {
			g_queue_unlink (photo_ht_keys, link);
			g_queue_push_head_link (photo_ht_keys, link);
		}
	} else {
		photo_data = photo_data_new (bytes);

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

		photo_data_unref (photo_data);
	}

	/* Hash table and queue sizes should be equal at all times. */
	g_warn_if_fail (
		g_hash_table_size (photo_ht) ==
		g_queue_get_length (photo_ht_keys));

	g_mutex_unlock (&photo_cache->priv->photo_ht_lock);

	g_free (key);
}

static gboolean
photo_ht_lookup (EPhotoCache *photo_cache,
                 const gchar *email_address,
                 GInputStream **out_stream)
{
	GHashTable *photo_ht;
	PhotoData *photo_data;
	gboolean found = FALSE;
	gchar *key;

	g_return_val_if_fail (email_address != NULL, FALSE);
	g_return_val_if_fail (out_stream != NULL, FALSE);

	photo_ht = photo_cache->priv->photo_ht;

	key = photo_ht_normalize_key (email_address);

	g_mutex_lock (&photo_cache->priv->photo_ht_lock);

	photo_data = g_hash_table_lookup (photo_ht, key);

	if (photo_data != NULL) {
		GBytes *bytes;

		bytes = photo_data_ref_bytes (photo_data);
		if (bytes != NULL) {
			*out_stream =
				g_memory_input_stream_new_from_bytes (bytes);
			g_bytes_unref (bytes);
		} else {
			*out_stream = NULL;
		}
		found = TRUE;
	}

	g_mutex_unlock (&photo_cache->priv->photo_ht_lock);

	g_free (key);

	return found;
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

static void
photo_cache_data_captured_cb (EDataCapture *data_capture,
                              GBytes *bytes,
                              DataCaptureClosure *closure)
{
	EPhotoCache *photo_cache;

	photo_cache = g_weak_ref_get (&closure->photo_cache);

	if (photo_cache != NULL) {
		e_photo_cache_add_photo (
			photo_cache, closure->email_address, bytes);
		g_object_unref (photo_cache);
	}
}

static void
photo_cache_async_subtask_done_cb (GObject *source_object,
                                   GAsyncResult *result,
                                   gpointer user_data)
{
	AsyncSubtask *async_subtask = user_data;

	e_photo_source_get_photo_finish (
		E_PHOTO_SOURCE (source_object),
		result,
		&async_subtask->stream,
		&async_subtask->priority,
		&async_subtask->error);

	async_subtask_complete (async_subtask);
	async_subtask_unref (async_subtask);
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
	EPhotoCache *self = E_PHOTO_CACHE (object);

	g_clear_object (&self->priv->client_cache);

	photo_ht_remove_all (self);

	/* Chain up to parent's dispose() method. */
	G_OBJECT_CLASS (e_photo_cache_parent_class)->dispose (object);
}

static void
photo_cache_finalize (GObject *object)
{
	EPhotoCache *self = E_PHOTO_CACHE (object);

	g_main_context_unref (self->priv->main_context);

	g_hash_table_destroy (self->priv->photo_ht);
	g_hash_table_destroy (self->priv->sources_ht);

	g_mutex_clear (&self->priv->photo_ht_lock);
	g_mutex_clear (&self->priv->sources_ht_lock);

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
	GHashTable *sources_ht;

	photo_ht = g_hash_table_new_full (
		(GHashFunc) g_str_hash,
		(GEqualFunc) g_str_equal,
		(GDestroyNotify) g_free,
		(GDestroyNotify) photo_data_unref);

	sources_ht = g_hash_table_new_full (
		(GHashFunc) g_direct_hash,
		(GEqualFunc) g_direct_equal,
		(GDestroyNotify) g_object_unref,
		(GDestroyNotify) NULL);

	photo_cache->priv = e_photo_cache_get_instance_private (photo_cache);
	photo_cache->priv->main_context = g_main_context_ref_thread_default ();
	photo_cache->priv->photo_ht = photo_ht;
	photo_cache->priv->sources_ht = sources_ht;

	g_mutex_init (&photo_cache->priv->photo_ht_lock);
	g_mutex_init (&photo_cache->priv->sources_ht_lock);
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
 * e_photo_cache_add_photo_source:
 * @photo_cache: an #EPhotoCache
 * @photo_source: an #EPhotoSource
 *
 * Adds @photo_source as a potential source of photos.
 **/
void
e_photo_cache_add_photo_source (EPhotoCache *photo_cache,
                                EPhotoSource *photo_source)
{
	GHashTable *sources_ht;

	g_return_if_fail (E_IS_PHOTO_CACHE (photo_cache));
	g_return_if_fail (E_IS_PHOTO_SOURCE (photo_source));

	sources_ht = photo_cache->priv->sources_ht;

	g_mutex_lock (&photo_cache->priv->sources_ht_lock);

	g_hash_table_add (sources_ht, g_object_ref (photo_source));

	g_mutex_unlock (&photo_cache->priv->sources_ht_lock);
}

/**
 * e_photo_cache_list_photo_sources:
 * @photo_cache: an #EPhotoCache
 *
 * Returns a list of photo sources for @photo_cache.
 *
 * The sources returned in the list are referenced for thread-safety.
 * They must each be unreferenced with g_object_unref() when finished
 * with them.  Free the returned list itself with g_list_free().
 *
 * An easy way to free the list property in one step is as follows:
 *
 * |[
 *   g_list_free_full (list, g_object_unref);
 * ]|
 *
 * Returns: a sorted list of photo sources
 **/
GList *
e_photo_cache_list_photo_sources (EPhotoCache *photo_cache)
{
	GHashTable *sources_ht;
	GList *list;

	g_return_val_if_fail (E_IS_PHOTO_CACHE (photo_cache), NULL);

	sources_ht = photo_cache->priv->sources_ht;

	g_mutex_lock (&photo_cache->priv->sources_ht_lock);

	list = g_hash_table_get_keys (sources_ht);
	g_list_foreach (list, (GFunc) g_object_ref, NULL);

	g_mutex_unlock (&photo_cache->priv->sources_ht_lock);

	return list;
}

/**
 * e_photo_cache_remove_photo_source:
 * @photo_cache: an #EPhotoCache
 * @photo_source: an #EPhotoSource
 *
 * Removes @photo_source as a potential source of photos.
 *
 * Returns: %TRUE if @photo_source was found and removed, %FALSE if not
 **/
gboolean
e_photo_cache_remove_photo_source (EPhotoCache *photo_cache,
                                   EPhotoSource *photo_source)
{
	GHashTable *sources_ht;
	gboolean removed;

	g_return_val_if_fail (E_IS_PHOTO_CACHE (photo_cache), FALSE);
	g_return_val_if_fail (E_IS_PHOTO_SOURCE (photo_source), FALSE);

	sources_ht = photo_cache->priv->sources_ht;

	g_mutex_lock (&photo_cache->priv->sources_ht_lock);

	removed = g_hash_table_remove (sources_ht, photo_source);

	g_mutex_unlock (&photo_cache->priv->sources_ht_lock);

	return removed;
}

/**
 * e_photo_cache_add_photo:
 * @photo_cache: an #EPhotoCache
 * @email_address: an email address
 * @bytes: a #GBytes containing photo data, or %NULL
 *
 * Adds a cache entry for @email_address, such that subsequent photo requests
 * for @email_address will yield a #GMemoryInputStream loaded with @bytes
 * without consulting available photo sources.
 *
 * The @bytes argument can also be %NULL to indicate no photo is available for
 * @email_address.  Subsequent photo requests for @email_address will yield no
 * input stream.
 *
 * The entry may be removed without notice however, subject to @photo_cache's
 * internal caching policy.
 **/
void
e_photo_cache_add_photo (EPhotoCache *photo_cache,
                         const gchar *email_address,
                         GBytes *bytes)
{
	g_return_if_fail (E_IS_PHOTO_CACHE (photo_cache));
	g_return_if_fail (email_address != NULL);

	photo_ht_insert (photo_cache, email_address, bytes);
}

/**
 * e_photo_cache_remove_photo:
 * @photo_cache: an #EPhotoCache
 * @email_address: an email address
 *
 * Removes the cache entry for @email_address, if such an entry exists.
 *
 * Returns: %TRUE if a cache entry was found and removed
 **/
gboolean
e_photo_cache_remove_photo (EPhotoCache *photo_cache,
                            const gchar *email_address)
{
	g_return_val_if_fail (E_IS_PHOTO_CACHE (photo_cache), FALSE);
	g_return_val_if_fail (email_address != NULL, FALSE);

	return photo_ht_remove (photo_cache, email_address);
}

/**
 * e_photo_cache_get_photo_sync:
 * @photo_cache: an #EPhotoCache
 * @email_address: an email address
 * @cancellable: optional #GCancellable object, or %NULL
 * @out_stream: return location for a #GInputStream, or %NULL
 * @error: return location for a #GError, or %NULL
 *
 * Searches available photo sources for a photo associated with
 * @email_address.
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
	EAsyncClosure *closure;
	GAsyncResult *result;
	gboolean success;

	if (g_cancellable_set_error_if_cancelled (cancellable, error))
		return FALSE;

	closure = e_async_closure_new ();

	e_photo_cache_get_photo (
		photo_cache, email_address, cancellable,
		e_async_closure_callback, closure);

	result = e_async_closure_wait (closure);

	success = e_photo_cache_get_photo_finish (
		photo_cache, result, out_stream, error);

	e_async_closure_free (closure);

	return success;
}

/**
 * e_photo_cache_get_photo:
 * @photo_cache: an #EPhotoCache
 * @email_address: an email address
 * @cancellable: optional #GCancellable object, or %NULL
 * @callback: a #GAsyncReadyCallback to call when the request is satisfied
 * @user_data: data to pass to the callback function
 *
 * Asynchronously searches available photo sources for a photo associated
 * with @email_address.
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
	ESimpleAsyncResult *simple;
	AsyncContext *async_context;
	EDataCapture *data_capture;
	GInputStream *stream = NULL;
	GList *list, *link;

	g_return_if_fail (E_IS_PHOTO_CACHE (photo_cache));
	g_return_if_fail (email_address != NULL);

	/* This will be used to eavesdrop on the resulting input stream
	 * for the purpose of adding the photo data to the photo cache. */
	data_capture = e_data_capture_new (photo_cache->priv->main_context);

	g_signal_connect_data (
		data_capture, "finished",
		G_CALLBACK (photo_cache_data_captured_cb),
		data_capture_closure_new (photo_cache, email_address),
		(GClosureNotify) data_capture_closure_free, 0);

	async_context = async_context_new (data_capture, cancellable);

	simple = e_simple_async_result_new (
		G_OBJECT (photo_cache), callback,
		user_data, e_photo_cache_get_photo);

	e_simple_async_result_set_check_cancellable (simple, cancellable);

	e_simple_async_result_set_op_pointer (
		simple, async_context, (GDestroyNotify) async_context_free);

	/* Check if we have this email address already cached. */
	if (photo_ht_lookup (photo_cache, email_address, &stream)) {
		async_context->stream = stream;  /* takes ownership */
		e_simple_async_result_complete_idle (simple);
		goto exit;
	}

	list = e_photo_cache_list_photo_sources (photo_cache);

	if (list == NULL) {
		e_simple_async_result_complete_idle (simple);
		goto exit;
	}

	g_mutex_lock (&async_context->lock);

	/* Dispatch a subtask for each photo source. */
	for (link = list; link != NULL; link = g_list_next (link)) {
		EPhotoSource *photo_source;
		AsyncSubtask *async_subtask;

		photo_source = E_PHOTO_SOURCE (link->data);
		async_subtask = async_subtask_new (photo_source, simple);

		g_hash_table_add (
			async_context->subtasks,
			async_subtask_ref (async_subtask));

		e_photo_source_get_photo (
			photo_source, email_address,
			async_subtask->cancellable,
			photo_cache_async_subtask_done_cb,
			async_subtask_ref (async_subtask));

		async_subtask_unref (async_subtask);
	}

	g_mutex_unlock (&async_context->lock);

	g_list_free_full (list, (GDestroyNotify) g_object_unref);

	/* Check if we were cancelled while dispatching subtasks. */
	if (g_cancellable_is_cancelled (cancellable))
		async_context_cancel_subtasks (async_context);

exit:
	g_object_unref (simple);
	g_object_unref (data_capture);
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
	ESimpleAsyncResult *simple;
	AsyncContext *async_context;

	g_return_val_if_fail (
		e_simple_async_result_is_valid (
		result, G_OBJECT (photo_cache),
		e_photo_cache_get_photo), FALSE);

	simple = E_SIMPLE_ASYNC_RESULT (result);
	async_context = e_simple_async_result_get_op_pointer (simple);

	if (e_simple_async_result_propagate_error (simple, error))
		return FALSE;

	if (out_stream != NULL) {
		if (async_context->stream != NULL)
			*out_stream = g_object_ref (async_context->stream);
		else
			*out_stream = NULL;
	}

	return TRUE;
}

