/*
 * e-gravatar-photo-source.c
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

#include <libsoup/soup.h>
#include <libedataserver/libedataserver.h>

#include "e-util/e-util.h"
#include "e-gravatar-photo-source.h"

#define AVATAR_BASE_URI "https://seccdn.libravatar.org/avatar/"

struct _EGravatarPhotoSourcePrivate {
	gboolean enabled;
};

enum {
	PROP_0,
	PROP_ENABLED
};

typedef struct _AsyncContext AsyncContext;

struct _AsyncContext {
	gchar *email_address;
	GInputStream *stream;
};

/* Forward Declarations */
static void	e_gravatar_photo_source_interface_init
					(EPhotoSourceInterface *iface);

G_DEFINE_DYNAMIC_TYPE_EXTENDED (EGravatarPhotoSource, e_gravatar_photo_source, G_TYPE_OBJECT, 0,
	G_ADD_PRIVATE_DYNAMIC (EGravatarPhotoSource)
	G_IMPLEMENT_INTERFACE_DYNAMIC (E_TYPE_PHOTO_SOURCE, e_gravatar_photo_source_interface_init))

static void
async_context_free (AsyncContext *async_context)
{
	g_free (async_context->email_address);
	g_clear_object (&async_context->stream);

	g_slice_free (AsyncContext, async_context);
}

static void
gravatar_photo_source_get_photo_thread (ESimpleAsyncResult *simple,
                                        gpointer source_object,
                                        GCancellable *cancellable)
{
	AsyncContext *async_context;
	SoupMessage *message;
	SoupSession *session;
	GInputStream *stream = NULL;
	gchar *hash;
	gchar *uri;
	GError *local_error = NULL;

	g_return_if_fail (E_IS_GRAVATAR_PHOTO_SOURCE (source_object));

	if (!e_gravatar_photo_source_get_enabled (E_GRAVATAR_PHOTO_SOURCE (source_object)))
		return;

	async_context = e_simple_async_result_get_op_pointer (simple);

	hash = e_gravatar_get_hash (async_context->email_address);
	uri = g_strdup_printf ("%s%s?d=404", AVATAR_BASE_URI, hash);

	g_debug ("Requesting avatar for %s", async_context->email_address);
	g_debug ("%s", uri);

	session = soup_session_new ();

	/* We control the URI so there should be no error. */
	message = soup_message_new (SOUP_METHOD_GET, uri);
	g_return_if_fail (message != NULL);

	stream = soup_session_send (session, message, cancellable, &local_error);

	/* Sanity check. */
	g_return_if_fail (
		((stream != NULL) && (local_error == NULL)) ||
		((stream == NULL) && (local_error != NULL)));

	/* XXX soup_request_send() returns a stream on HTTP errors.
	 *     We need to check the status code on the SoupMessage
	 *     to make sure the we're not getting an error message. */
	if (stream != NULL) {
		if (SOUP_STATUS_IS_SUCCESSFUL (soup_message_get_status (message))) {
			async_context->stream = g_object_ref (stream);

		} else if (soup_message_get_status (message) != SOUP_STATUS_NOT_FOUND) {
			local_error = g_error_new_literal (
				E_SOUP_SESSION_ERROR,
				soup_message_get_status (message),
				soup_message_get_reason_phrase (message));
		}

		g_object_unref (stream);
	}

	if (local_error != NULL) {
		const gchar *domain;

		domain = g_quark_to_string (local_error->domain);
		g_debug ("Error: %s (%s)", local_error->message, domain);
		e_simple_async_result_take_error (simple, local_error);
	}

	g_debug ("Request complete");

	g_clear_object (&message);
	g_clear_object (&session);

	g_free (hash);
	g_free (uri);
}

static void
gravatar_photo_source_get_photo (EPhotoSource *photo_source,
                                 const gchar *email_address,
                                 GCancellable *cancellable,
                                 GAsyncReadyCallback callback,
                                 gpointer user_data)
{
	ESimpleAsyncResult *simple;
	AsyncContext *async_context;

	async_context = g_slice_new0 (AsyncContext);
	async_context->email_address = g_strdup (email_address);

	simple = e_simple_async_result_new (
		G_OBJECT (photo_source), callback,
		user_data, gravatar_photo_source_get_photo);

	e_simple_async_result_set_check_cancellable (simple, cancellable);

	e_simple_async_result_set_op_pointer (
		simple, async_context, (GDestroyNotify) async_context_free);

	e_simple_async_result_run_in_thread (
		simple, G_PRIORITY_LOW,
		gravatar_photo_source_get_photo_thread, cancellable);

	g_object_unref (simple);
}

static gboolean
gravatar_photo_source_get_photo_finish (EPhotoSource *photo_source,
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
		gravatar_photo_source_get_photo), FALSE);

	simple = E_SIMPLE_ASYNC_RESULT (result);
	async_context = e_simple_async_result_get_op_pointer (simple);

	if (e_simple_async_result_propagate_error (simple, error))
		return FALSE;

	if (async_context->stream != NULL) {
		*out_stream = g_object_ref (async_context->stream);
		if (out_priority != NULL)
			*out_priority = G_PRIORITY_DEFAULT;
	} else {
		*out_stream = NULL;
	}

	return TRUE;
}

static void
gravatar_photo_source_set_property (GObject *object,
				    guint property_id,
				    const GValue *value,
				    GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_ENABLED:
			e_gravatar_photo_source_set_enabled (
				E_GRAVATAR_PHOTO_SOURCE (object),
				g_value_get_boolean (value));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
gravatar_photo_source_get_property (GObject *object,
				    guint property_id,
				    GValue *value,
				    GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_ENABLED:
			g_value_set_boolean (
				value,
				e_gravatar_photo_source_get_enabled (
				E_GRAVATAR_PHOTO_SOURCE (object)));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
e_gravatar_photo_source_class_init (EGravatarPhotoSourceClass *class)
{
	GObjectClass *object_class;

	object_class = G_OBJECT_CLASS (class);
	object_class->set_property = gravatar_photo_source_set_property;
	object_class->get_property = gravatar_photo_source_get_property;

	g_object_class_install_property (
		object_class,
		PROP_ENABLED,
		g_param_spec_boolean (
			"enabled",
			"Enabled",
			"Whether can search for contact photos",
			FALSE,
			G_PARAM_READWRITE |
			G_PARAM_STATIC_STRINGS));
}

static void
e_gravatar_photo_source_class_finalize (EGravatarPhotoSourceClass *class)
{
}

static void
e_gravatar_photo_source_interface_init (EPhotoSourceInterface *iface)
{
	iface->get_photo = gravatar_photo_source_get_photo;
	iface->get_photo_finish = gravatar_photo_source_get_photo_finish;
}

static void
e_gravatar_photo_source_init (EGravatarPhotoSource *photo_source)
{
	GSettings *settings;

	photo_source->priv = e_gravatar_photo_source_get_instance_private (photo_source);

	settings = e_util_ref_settings ("org.gnome.evolution.mail");

	g_settings_bind (settings, "search-gravatar-for-photo",
			 photo_source, "enabled",
			  G_SETTINGS_BIND_DEFAULT | G_SETTINGS_BIND_NO_SENSITIVITY);

	g_object_unref (settings);
}

void
e_gravatar_photo_source_type_register (GTypeModule *type_module)
{
	/* XXX G_DEFINE_DYNAMIC_TYPE declares a static type registration
	 *     function, so we have to wrap it with a public function in
	 *     order to register types from a separate compilation unit. */
	e_gravatar_photo_source_register_type (type_module);
}

EPhotoSource *
e_gravatar_photo_source_new (void)
{
	return g_object_new (E_TYPE_GRAVATAR_PHOTO_SOURCE, NULL);
}

gchar *
e_gravatar_get_hash (const gchar *email_address)
{
	gchar *string;
	gchar *hash;

	g_return_val_if_fail (email_address != NULL, NULL);
	g_return_val_if_fail (g_utf8_validate (email_address, -1, NULL), NULL);

	string = g_strstrip (g_utf8_strdown (email_address, -1));
	hash = g_compute_checksum_for_string (G_CHECKSUM_MD5, string, -1);
	g_free (string);

	return hash;
}

gboolean
e_gravatar_photo_source_get_enabled (EGravatarPhotoSource *photo_source)
{
	g_return_val_if_fail (E_IS_GRAVATAR_PHOTO_SOURCE (photo_source), FALSE);

	return photo_source->priv->enabled;
}

void
e_gravatar_photo_source_set_enabled (EGravatarPhotoSource *photo_source,
				     gboolean enabled)
{
	g_return_if_fail (E_IS_GRAVATAR_PHOTO_SOURCE (photo_source));

	if ((photo_source->priv->enabled ? 1 : 0) == (enabled ? 1 : 0))
		return;

	photo_source->priv->enabled = enabled;

	g_object_notify (G_OBJECT (photo_source), "enabled");
}
