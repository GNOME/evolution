/*
 * e-contact-photo-source.h
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

#ifndef E_CONTACT_PHOTO_SOURCE_H
#define E_CONTACT_PHOTO_SOURCE_H

#include <e-util/e-util.h>

/* Standard GObject macros */
#define E_TYPE_CONTACT_PHOTO_SOURCE \
	(e_contact_photo_source_get_type ())
#define E_CONTACT_PHOTO_SOURCE(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), E_TYPE_CONTACT_PHOTO_SOURCE, EContactPhotoSource))
#define E_CONTACT_PHOTO_SOURCE_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((cls), E_TYPE_CONTACT_PHOTO_SOURCE, EContactPhotoSourceClass))
#define E_IS_CONTACT_PHOTO_SOURCE(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), E_TYPE_CONTACT_PHOTO_SOURCE))
#define E_IS_CONTACT_PHOTO_SOURCE_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_TYPE \
	((cls), E_TYPE_CONTACT_PHOTO_SOURCE))
#define E_CONTACT_PHOTO_SOURCE_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS \
	((obj), E_TYPE_CONTACT_PHOTO_SOURCE, EContactPhotoSourceClass))

G_BEGIN_DECLS

typedef struct _EContactPhotoSource EContactPhotoSource;
typedef struct _EContactPhotoSourceClass EContactPhotoSourceClass;
typedef struct _EContactPhotoSourcePrivate EContactPhotoSourcePrivate;

struct _EContactPhotoSource {
	GObject parent;
	EContactPhotoSourcePrivate *priv;
};

struct _EContactPhotoSourceClass {
	GObjectClass parent_class;
};

GType		e_contact_photo_source_get_type
					(void) G_GNUC_CONST;
void		e_contact_photo_source_type_register
					(GTypeModule *type_module);
EPhotoSource *	e_contact_photo_source_new
					(EClientCache *client_cache,
					 ESource *source);
EClientCache *	e_contact_photo_source_ref_client_cache
					(EContactPhotoSource *photo_source);
ESource *	e_contact_photo_source_ref_source
					(EContactPhotoSource *photo_source);

G_END_DECLS

#endif /* E_CONTACT_PHOTO_SOURCE_H */

