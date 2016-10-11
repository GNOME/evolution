/*
 * e-gravatar-photo-source.h
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

#ifndef E_GRAVATAR_PHOTO_SOURCE_H
#define E_GRAVATAR_PHOTO_SOURCE_H

#include <e-util/e-util.h>

/* Standard GObject macros */
#define E_TYPE_GRAVATAR_PHOTO_SOURCE \
	(e_gravatar_photo_source_get_type ())
#define E_GRAVATAR_PHOTO_SOURCE(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), E_TYPE_GRAVATAR_PHOTO_SOURCE, EGravatarPhotoSource))
#define E_GRAVATAR_PHOTO_SOURCE_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((cls), E_TYPE_GRAVATAR_PHOTO_SOURCE, EGravatarPhotoSourceClass))
#define E_IS_GRAVATAR_PHOTO_SOURCE(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), E_TYPE_GRAVATAR_PHOTO_SOURCE))
#define E_IS_GRAVATAR_PHOTO_SOURCE_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_TYPE \
	((cls), E_TYPE_GRAVATAR_PHOTO_SOURCE))
#define E_GRAVATAR_PHOTO_SOURCE_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS \
	((obj), E_TYPE_GRAVATAR_PHOTO_SOURCE, EGravatarPhotoSourceClass))

G_BEGIN_DECLS

typedef struct _EGravatarPhotoSource EGravatarPhotoSource;
typedef struct _EGravatarPhotoSourceClass EGravatarPhotoSourceClass;
typedef struct _EGravatarPhotoSourcePrivate EGravatarPhotoSourcePrivate;

struct _EGravatarPhotoSource {
	GObject parent;
	EGravatarPhotoSourcePrivate *priv;
};

struct _EGravatarPhotoSourceClass {
	GObjectClass parent_class;
};

GType		e_gravatar_photo_source_get_type
						(void) G_GNUC_CONST;
void		e_gravatar_photo_source_type_register
						(GTypeModule *type_module);
EPhotoSource *	e_gravatar_photo_source_new	(void);
gchar *		e_gravatar_get_hash		(const gchar *email_address);
gboolean	e_gravatar_photo_source_get_enabled
						(EGravatarPhotoSource *photo_source);
void		e_gravatar_photo_source_set_enabled
						(EGravatarPhotoSource *photo_source,
						 gboolean enabled);

G_END_DECLS

#endif /* E_GRAVATAR_PHOTO_SOURCE_H */

