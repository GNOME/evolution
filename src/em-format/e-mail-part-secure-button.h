/*
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

#ifndef E_MAIL_PART_SECURE_BUTTON_H
#define E_MAIL_PART_SECURE_BUTTON_H

#include <em-format/e-mail-part.h>

/* Standard GObject macros */
#define E_TYPE_MAIL_PART_SECURE_BUTTON \
	(e_mail_part_secure_button_get_type ())
#define E_MAIL_PART_SECURE_BUTTON(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), E_TYPE_MAIL_PART_SECURE_BUTTON, EMailPartSecureButton))
#define E_MAIL_PART_SECURE_BUTTON_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((cls), E_TYPE_MAIL_PART_SECURE_BUTTON, EMailPartSecureButtonClass))
#define E_IS_MAIL_PART_SECURE_BUTTON(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), E_TYPE_MAIL_PART_SECURE_BUTTON))
#define E_IS_MAIL_PART_SECURE_BUTTON_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_TYPE \
	((cls), E_TYPE_MAIL_PART_SECURE_BUTTON))
#define E_MAIL_PART_SECURE_BUTTON_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS \
	((obj), E_TYPE_MAIL_PART_SECURE_BUTTON, EMailPartSecureButtonClass))

G_BEGIN_DECLS

typedef struct _EMailPartSecureButton EMailPartSecureButton;
typedef struct _EMailPartSecureButtonClass EMailPartSecureButtonClass;
typedef struct _EMailPartSecureButtonPrivate EMailPartSecureButtonPrivate;

struct _EMailPartSecureButton {
	EMailPart parent;
	EMailPartSecureButtonPrivate *priv;
};

struct _EMailPartSecureButtonClass {
	EMailPartClass parent_class;
};

GType		e_mail_part_secure_button_get_type	(void) G_GNUC_CONST;
EMailPart *	e_mail_part_secure_button_new		(CamelMimePart *mime_part,
							 const gchar *id);

G_END_DECLS

#endif /* E_MAIL_PART_SECURE_BUTTON_H */
