/*
 * e-contact-photo-source.c
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

#include "evolution-config.h"

#include "e-util/e-util.h"
#include "e-contact-photo-source.h"

typedef struct _AsyncContext AsyncContext;

struct _EContactPhotoSourcePrivate {
	EClientCache *client_cache;
	ESource *source;
};

struct _AsyncContext {
	EBookClient *client;
	gchar *query_string;
	GInputStream *stream;
	GCancellable *cancellable;
	gint priority;
};

enum {
	PROP_0,
	PROP_CLIENT_CACHE,
	PROP_SOURCE
};

/* Forward Declarations */
static void	e_contact_photo_source_interface_init
					(EPhotoSourceInterface *iface);

G_DEFINE_DYNAMIC_TYPE_EXTENDED (EContactPhotoSource, e_contact_photo_source, G_TYPE_OBJECT, 0,
	G_ADD_PRIVATE_DYNAMIC (EContactPhotoSource)
	G_IMPLEMENT_INTERFACE_DYNAMIC (E_TYPE_PHOTO_SOURCE, e_contact_photo_source_interface_init))

static void
async_context_free (AsyncContext *async_context)
{
	g_clear_object (&async_context->client);
	g_free (async_context->query_string);
	g_clear_object (&async_context->stream);
	g_clear_object (&async_context->cancellable);

	g_slice_free (AsyncContext, async_context);
}

static EContactPhoto *
contact_photo_source_extract_photo (EContact *contact,
                                    gint *out_priority)
{
	EContactPhoto *photo;

	photo = e_contact_get (contact, E_CONTACT_PHOTO);
	*out_priority = G_PRIORITY_HIGH;

	if (photo == NULL) {
		photo = e_contact_get (contact, E_CONTACT_LOGO);
		*out_priority = G_PRIORITY_LOW;
	}

	return photo;
}

static void
contact_photo_source_get_photo_thread (ESimpleAsyncResult *simple,
                                       gpointer source_object,
                                       GCancellable *cancellable)
{
	AsyncContext *async_context;
	GSList *slist = NULL;
	GSList *slink;
	GError *error = NULL;

	async_context = e_simple_async_result_get_op_pointer (simple);

	e_book_client_get_contacts_sync (
		async_context->client,
		async_context->query_string,
		&slist, cancellable, &error);

	if (error != NULL) {
		g_warn_if_fail (slist == NULL);
		e_simple_async_result_take_error (simple, error);
		return;
	}

	/* See if any of the contacts have a photo. */
	for (slink = slist; slink != NULL; slink = g_slist_next (slink)) {
		EContact *contact = E_CONTACT (slink->data);
		GInputStream *stream = NULL;
		EContactPhoto *photo;

		photo = contact_photo_source_extract_photo (
			contact, &async_context->priority);

		if (photo == NULL)
			continue;

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

			/* Disregard errors and proceed as
			 * though the contact has no photo. */

			/* XXX Return type should have been GInputStream. */
			file_stream = g_file_read (file, cancellable, NULL);
			if (file_stream != NULL)
				stream = G_INPUT_STREAM (file_stream);

			g_object_unref (file);
		}

		e_contact_photo_free (photo);

		/* Stop on the first input stream. */
		if (stream != NULL) {
			async_context->stream = g_object_ref (stream);
			g_object_unref (stream);
			break;
		}
	}

	g_slist_free_full (slist, (GDestroyNotify) g_object_unref);
}

static void
contact_photo_source_get_client_cb (GObject *source_object,
                                    GAsyncResult *result,
                                    gpointer user_data)
{
	ESimpleAsyncResult *simple;
	AsyncContext *async_context;
	EClient *client;
	GError *error = NULL;

	simple = E_SIMPLE_ASYNC_RESULT (user_data);
	async_context = e_simple_async_result_get_op_pointer (simple);

	client = e_client_cache_get_client_finish (
		E_CLIENT_CACHE (source_object), result, &error);

	/* Sanity check. */
	g_return_if_fail (
		((client != NULL) && (error == NULL)) ||
		((client == NULL) && (error != NULL)));

	if (client != NULL) {
		async_context->client = E_BOOK_CLIENT (g_object_ref (client));

		/* The rest of the operation we can run from a
		 * worker thread to keep the logic flow simple. */
		e_simple_async_result_run_in_thread (
			simple, G_PRIORITY_LOW,
			contact_photo_source_get_photo_thread, async_context->cancellable);

		g_object_unref (client);

	} else {
		e_simple_async_result_take_error (simple, error);
		e_simple_async_result_complete_idle (simple);
	}

	g_object_unref (simple);
}

static void
contact_photo_source_set_client_cache (EContactPhotoSource *photo_source,
                                       EClientCache *client_cache)
{
	g_return_if_fail (E_IS_CLIENT_CACHE (client_cache));
	g_return_if_fail (photo_source->priv->client_cache == NULL);

	photo_source->priv->client_cache = g_object_ref (client_cache);
}

static void
contact_photo_source_set_source (EContactPhotoSource *photo_source,
                                 ESource *source)
{
	g_return_if_fail (E_IS_SOURCE (source));
	g_return_if_fail (photo_source->priv->source == NULL);

	photo_source->priv->source = g_object_ref (source);
}

static void
contact_photo_source_set_property (GObject *object,
                                   guint property_id,
                                   const GValue *value,
                                   GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_CLIENT_CACHE:
			contact_photo_source_set_client_cache (
				E_CONTACT_PHOTO_SOURCE (object),
				g_value_get_object (value));
			return;

		case PROP_SOURCE:
			contact_photo_source_set_source (
				E_CONTACT_PHOTO_SOURCE (object),
				g_value_get_object (value));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
contact_photo_source_get_property (GObject *object,
                                   guint property_id,
                                   GValue *value,
                                   GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_CLIENT_CACHE:
			g_value_take_object (
				value,
				e_contact_photo_source_ref_client_cache (
				E_CONTACT_PHOTO_SOURCE (object)));
			return;

		case PROP_SOURCE:
			g_value_take_object (
				value,
				e_contact_photo_source_ref_source (
				E_CONTACT_PHOTO_SOURCE (object)));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
contact_photo_source_dispose (GObject *object)
{
	EContactPhotoSource *self = E_CONTACT_PHOTO_SOURCE (object);

	g_clear_object (&self->priv->client_cache);
	g_clear_object (&self->priv->source);

	/* Chain up to parent's dispose() method. */
	G_OBJECT_CLASS (e_contact_photo_source_parent_class)->dispose (object);
}

static void
contact_photo_source_get_photo (EPhotoSource *photo_source,
                                const gchar *email_address,
                                GCancellable *cancellable,
                                GAsyncReadyCallback callback,
                                gpointer user_data)
{
	ESimpleAsyncResult *simple;
	AsyncContext *async_context;
	EClientCache *client_cache;
	ESourceRegistry *registry;
	EBookQuery *book_query;
	ESource *source;

	book_query = e_book_query_field_test (
		E_CONTACT_EMAIL, E_BOOK_QUERY_IS, email_address);

	async_context = g_slice_new0 (AsyncContext);
	async_context->query_string = e_book_query_to_string (book_query);

	if (G_IS_CANCELLABLE (cancellable))
		async_context->cancellable = g_object_ref (cancellable);

	e_book_query_unref (book_query);

	simple = e_simple_async_result_new (
		G_OBJECT (photo_source), callback,
		user_data, contact_photo_source_get_photo);

	e_simple_async_result_set_check_cancellable (simple, cancellable);

	e_simple_async_result_set_op_pointer (
		simple, async_context, (GDestroyNotify) async_context_free);

	client_cache = e_contact_photo_source_ref_client_cache (
		E_CONTACT_PHOTO_SOURCE (photo_source));
	registry = e_client_cache_ref_registry (client_cache);

	source = e_contact_photo_source_ref_source (
		E_CONTACT_PHOTO_SOURCE (photo_source));

	if (e_source_registry_check_enabled (registry, source)) {
		/* Obtain the EClient asynchronously.  If an instance needs
		 * to be created, it's more likely created in a thread with
		 * a main loop so signal emissions can work. */
		e_client_cache_get_client (
			client_cache, source,
			E_SOURCE_EXTENSION_ADDRESS_BOOK, (guint32) -1,
			cancellable,
			contact_photo_source_get_client_cb,
			g_object_ref (simple));
	} else {
		/* Return no result if the source is disabled. */
		e_simple_async_result_complete_idle (simple);
	}

	g_object_unref (client_cache);
	g_object_unref (registry);
	g_object_unref (source);

	g_object_unref (simple);
}

static gboolean
contact_photo_source_get_photo_finish (EPhotoSource *photo_source,
                                       GAsyncResult *result,
                                       GInputStream **out_stream,
                                       gint *out_priority,
                                       GError **error)
{
	ESimpleAsyncResult *simple;
	AsyncContext *async_context;

	g_return_val_if_fail (
		e_simple_async_result_is_valid (
		result, G_OBJECT (photo_source),
		contact_photo_source_get_photo), FALSE);

	simple = E_SIMPLE_ASYNC_RESULT (result);
	async_context = e_simple_async_result_get_op_pointer (simple);

	if (e_simple_async_result_propagate_error (simple, error))
		return FALSE;

	if (async_context->stream != NULL) {
		*out_stream = g_object_ref (async_context->stream);
		if (out_priority != NULL)
			*out_priority = async_context->priority;
	} else {
		*out_stream = NULL;
	}

	return TRUE;
}

static void
e_contact_photo_source_class_init (EContactPhotoSourceClass *class)
{
	GObjectClass *object_class;

	object_class = G_OBJECT_CLASS (class);
	object_class->set_property = contact_photo_source_set_property;
	object_class->get_property = contact_photo_source_get_property;
	object_class->dispose = contact_photo_source_dispose;

	g_object_class_install_property (
		object_class,
		PROP_CLIENT_CACHE,
		g_param_spec_object (
			"client-cache",
			"Client Cache",
			"Cache of shared EClient instances",
			E_TYPE_CLIENT_CACHE,
			G_PARAM_READWRITE |
			G_PARAM_CONSTRUCT_ONLY));

	g_object_class_install_property (
		object_class,
		PROP_SOURCE,
		g_param_spec_object (
			"source",
			"Source",
			"An address book source",
			E_TYPE_SOURCE,
			G_PARAM_READWRITE |
			G_PARAM_CONSTRUCT_ONLY));
}

static void
e_contact_photo_source_class_finalize (EContactPhotoSourceClass *class)
{
}

static void
e_contact_photo_source_interface_init (EPhotoSourceInterface *iface)
{
	iface->get_photo = contact_photo_source_get_photo;
	iface->get_photo_finish = contact_photo_source_get_photo_finish;
}

static void
e_contact_photo_source_init (EContactPhotoSource *photo_source)
{
	photo_source->priv = e_contact_photo_source_get_instance_private (photo_source);
}

void
e_contact_photo_source_type_register (GTypeModule *type_module)
{
	/* XXX G_DEFINE_DYNAMIC_TYPE declares a static type registration
	 *     function, so we have to wrap it with a public function in
	 *     order to register types from a separate compilation unit. */
	e_contact_photo_source_register_type (type_module);
}

EPhotoSource *
e_contact_photo_source_new (EClientCache *client_cache,
                            ESource *source)
{
	g_return_val_if_fail (E_IS_CLIENT_CACHE (client_cache), NULL);
	g_return_val_if_fail (E_IS_SOURCE (source), NULL);

	return g_object_new (
		E_TYPE_CONTACT_PHOTO_SOURCE,
		"client-cache", client_cache,
		"source", source,
		NULL);
}

EClientCache *
e_contact_photo_source_ref_client_cache (EContactPhotoSource *photo_source)
{
	g_return_val_if_fail (E_IS_CONTACT_PHOTO_SOURCE (photo_source), NULL);

	return g_object_ref (photo_source->priv->client_cache);
}

ESource *
e_contact_photo_source_ref_source (EContactPhotoSource *photo_source)
{
	g_return_val_if_fail (E_IS_CONTACT_PHOTO_SOURCE (photo_source), NULL);

	return g_object_ref (photo_source->priv->source);
}

