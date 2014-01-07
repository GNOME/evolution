/*
 * e-mail-part-image.h
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

#ifndef E_MAIL_PART_IMAGE_H
#define E_MAIL_PART_IMAGE_H

#include <em-format/e-mail-part.h>

/* Standard GObject macros */
#define E_TYPE_MAIL_PART_IMAGE \
	(e_mail_part_image_get_type ())
#define E_MAIL_PART_IMAGE(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), E_TYPE_MAIL_PART_IMAGE, EMailPartImage))
#define E_MAIL_PART_IMAGE_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((cls), E_TYPE_MAIL_PART_IMAGE, EMailPartImageClass))
#define E_IS_MAIL_PART_IMAGE(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), E_TYPE_MAIL_PART_IMAGE))
#define E_IS_MAIL_PART_IMAGE_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_TYPE \
	((cls), E_TYPE_MAIL_PART_IMAGE))
#define E_MAIL_PART_IMAGE_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS \
	((obj), E_TYPE_MAIL_PART_IMAGE, EMailPartImageClass))

G_BEGIN_DECLS

typedef struct _EMailPartImage EMailPartImage;
typedef struct _EMailPartImageClass EMailPartImageClass;
typedef struct _EMailPartImagePrivate EMailPartImagePrivate;

struct _EMailPartImage {
	EMailPart parent;
	EMailPartImagePrivate *priv;
};

struct _EMailPartImageClass {
	EMailPartClass parent_class;
};

GType		e_mail_part_image_get_type	(void) G_GNUC_CONST;
EMailPart *	e_mail_part_image_new		(CamelMimePart *mime_part,
						 const gchar *id);

G_END_DECLS

#endif /* E_MAIL_PART_IMAGE_H */

