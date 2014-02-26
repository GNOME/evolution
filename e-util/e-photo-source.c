/*
 * e-photo-source.c
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
 * SECTION: e-photo-source
 * @include: e-util/e-util.h
 * @short_description: A source of email address photos
 *
 * #EPhotoSource is an interface used to extend the functionality of
 * #EPhotoCache.  You can add an object implementing #EPhotoSource to an
 * #EPhotoCache with e_photo_cache_add_photo_source() and remove it with
 * e_photo_cache_remove_photo_source().  When #EPhotoCache needs a photo
 * for an email address it will invoke e_photo_source_get_photo() on all
 * available #EPhotoSource objects simultaneously and select one photo.
 **/

#include "e-photo-source.h"

G_DEFINE_INTERFACE (
	EPhotoSource,
	e_photo_source,
	G_TYPE_OBJECT)

static void
e_photo_source_default_init (EPhotoSourceInterface *iface)
{
}

/**
 * e_photo_source_get_photo:
 * @photo_source: an #EPhotoSource
 * @email_address: an email address
 * @cancellable: optional #GCancellable object, or %NULL
 * @callback: a #GAsyncReadyCallback to call when the request is satisfied
 * @user_data: data to pass to the callback function
 *
 * Asynchronously searches for a photo for @email_address.
 *
 * When the operation is finished, @callback will be called.  You can then
 * call e_photo_source_get_photo_finish() to get the result of the operation.
 **/
void
e_photo_source_get_photo (EPhotoSource *photo_source,
                          const gchar *email_address,
                          GCancellable *cancellable,
                          GAsyncReadyCallback callback,
                          gpointer user_data)
{
	EPhotoSourceInterface *iface;

	g_return_if_fail (E_IS_PHOTO_SOURCE (photo_source));
	g_return_if_fail (email_address != NULL);

	iface = E_PHOTO_SOURCE_GET_INTERFACE (photo_source);
	g_return_if_fail (iface->get_photo != NULL);

	iface->get_photo (
		photo_source, email_address,
		cancellable, callback, user_data);
}

/**
 * e_photo_source_get_photo_finish:
 * @photo_source: an #EPhotoSource
 * @result: a #GAsyncResult
 * @out_stream: return location for a #GInputStream
 * @out_priority: return location for a priority value, or %NULL
 * @error: return location for a #GError, or %NULL
 *
 * Finishes the operation started with e_photo_source_get_photo().
 *
 * If a match was found, a #GInputStream from which to read image data is
 * returned through the @out_stream return location, and a suggested priority
 * value for the match is returned through the @out_priority return location.
 *
 * You can use the @out_priority value to rank this result among other
 * #EPhotoSource results.  The value is usually @G_PRIORITY_DEFAULT, but
 * may be @G_PRIORITY_LOW if the result is a fallback image.
 *
 * If no match was found, the @out_stream return location is set to %NULL
 * (the @out_priority return location will remain unset).
 *
 * The return value indicates whether the search completed successfully,
 * not whether a match was found.  If an error occurred, the function will
 * set @error and return %FALSE.
 *
 * Returns: whether the search completed successfully
 **/
gboolean
e_photo_source_get_photo_finish (EPhotoSource *photo_source,
                                 GAsyncResult *result,
                                 GInputStream **out_stream,
                                 gint *out_priority,
                                 GError **error)
{
	EPhotoSourceInterface *iface;

	g_return_val_if_fail (E_IS_PHOTO_SOURCE (photo_source), FALSE);
	g_return_val_if_fail (G_IS_ASYNC_RESULT (result), FALSE);
	g_return_val_if_fail (out_stream != NULL, FALSE);

	iface = E_PHOTO_SOURCE_GET_INTERFACE (photo_source);
	g_return_val_if_fail (iface->get_photo_finish != NULL, FALSE);

	return iface->get_photo_finish (
		photo_source, result, out_stream, out_priority, error);
}

