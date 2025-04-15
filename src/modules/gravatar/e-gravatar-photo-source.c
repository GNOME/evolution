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

/* Forward Declarations */
static void	e_gravatar_photo_source_interface_init
					(EPhotoSourceInterface *iface);

G_DEFINE_DYNAMIC_TYPE_EXTENDED (EGravatarPhotoSource, e_gravatar_photo_source, G_TYPE_OBJECT, 0,
	G_ADD_PRIVATE_DYNAMIC (EGravatarPhotoSource)
	G_IMPLEMENT_INTERFACE_DYNAMIC (E_TYPE_PHOTO_SOURCE, e_gravatar_photo_source_interface_init))

static void
gravatar_photo_source_get_photo_thread (GTask *task,
					gpointer source_object,
					gpointer task_data,
					GCancellable *cancellable)
{
	const gchar *email_address = task_data;
	SoupMessage *message;
	SoupSession *session;
	GInputStream *stream = NULL;
	gchar *hash;
	gchar *uri;
	GError *local_error = NULL;

	g_return_if_fail (E_IS_GRAVATAR_PHOTO_SOURCE (source_object));

	if (!e_gravatar_photo_source_get_enabled (E_GRAVATAR_PHOTO_SOURCE (source_object)))
		return;

	hash = e_gravatar_get_hash (email_address);
	uri = g_strdup_printf ("%s%s?d=404", AVATAR_BASE_URI, hash);

	g_debug ("Requesting avatar for %s", email_address);
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
			g_task_return_pointer (task, g_steal_pointer (&stream), g_object_unref);

		} else if (soup_message_get_status (message) != SOUP_STATUS_NOT_FOUND) {
			local_error = g_error_new_literal (
				E_SOUP_SESSION_ERROR,
				soup_message_get_status (message),
				soup_message_get_reason_phrase (message));
		} else {
			g_task_return_pointer (task, NULL, NULL);
		}

		g_clear_object (&stream);
	}

	if (local_error != NULL) {
		const gchar *domain;

		domain = g_quark_to_string (local_error->domain);
		g_debug ("Error: %s (%s)", local_error->message, domain);
		g_task_return_error (task, g_steal_pointer (&local_error));
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
	GTask *task;

	task = g_task_new (photo_source, cancellable, callback, user_data);
	g_task_set_source_tag (task, gravatar_photo_source_get_photo);
	g_task_set_priority (task, G_PRIORITY_LOW);
	g_task_set_task_data (task, g_strdup (email_address), g_free);

	g_task_run_in_thread (task, gravatar_photo_source_get_photo_thread);

	g_object_unref (task);
}

static gboolean
gravatar_photo_source_get_photo_finish (EPhotoSource *photo_source,
                                        GAsyncResult *result,
                                        GInputStream **out_stream,
                                        gint *out_priority,
                                        GError **error)
{
	GError *local_error = NULL;

	g_return_val_if_fail (E_IS_PHOTO_SOURCE (photo_source), FALSE);
	g_return_val_if_fail (g_task_is_valid (result, photo_source), FALSE);
	g_return_val_if_fail (g_async_result_is_tagged (result, gravatar_photo_source_get_photo), FALSE);

	*out_stream = g_task_propagate_pointer (G_TASK (result), &local_error);
	if (local_error) {
		g_propagate_error (error, g_steal_pointer (&local_error));
		return FALSE;
	}

	if (out_priority != NULL)
		*out_priority = G_PRIORITY_DEFAULT;

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
