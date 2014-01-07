/*
 * e-photo-source.h
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

#if !defined (__E_UTIL_H_INSIDE__) && !defined (LIBEUTIL_COMPILATION)
#error "Only <e-util/e-util.h> should be included directly."
#endif

#ifndef E_PHOTO_SOURCE_H
#define E_PHOTO_SOURCE_H

#include <gio/gio.h>

/* Standard GObject macros */
#define E_TYPE_PHOTO_SOURCE \
	(e_photo_source_get_type ())
#define E_PHOTO_SOURCE(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), E_TYPE_PHOTO_SOURCE, EPhotoSource))
#define E_IS_PHOTO_SOURCE(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), E_TYPE_PHOTO_SOURCE))
#define E_PHOTO_SOURCE_GET_INTERFACE(obj) \
	(G_TYPE_INSTANCE_GET_INTERFACE \
	((obj), E_TYPE_PHOTO_SOURCE, EPhotoSourceInterface))

G_BEGIN_DECLS

typedef struct _EPhotoSource EPhotoSource;
typedef struct _EPhotoSourceInterface EPhotoSourceInterface;

struct _EPhotoSourceInterface {
	GTypeInterface parent_interface;

	void		(*get_photo)		(EPhotoSource *photo_source,
						 const gchar *email_address,
						 GCancellable *cancellable,
						 GAsyncReadyCallback callback,
						 gpointer user_data);
	gboolean	(*get_photo_finish)	(EPhotoSource *photo_source,
						 GAsyncResult *result,
						 GInputStream **out_stream,
						 gint *out_priority,
						 GError **error);
};

GType		e_photo_source_get_type		(void) G_GNUC_CONST;
void		e_photo_source_get_photo	(EPhotoSource *photo_source,
						 const gchar *email_address,
						 GCancellable *cancellable,
						 GAsyncReadyCallback callback,
						 gpointer user_data);
gboolean	e_photo_source_get_photo_finish	(EPhotoSource *photo_source,
						 GAsyncResult *result,
						 GInputStream **out_stream,
						 gint *out_priority,
						 GError **error);

G_END_DECLS

#endif /* E_PHOTO_SOURCE_H */

