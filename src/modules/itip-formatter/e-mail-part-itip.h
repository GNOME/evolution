/*
 * e-mail-part-itip.h
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

#ifndef E_MAIL_PART_ITIP_H
#define E_MAIL_PART_ITIP_H

#include <libecal/libecal.h>
#include <libebackend/libebackend.h>

#include <em-format/e-mail-part.h>

#include "itip-view.h"

/* Standard GObject macros */
#define E_TYPE_MAIL_PART_ITIP \
	(e_mail_part_itip_get_type ())
#define E_MAIL_PART_ITIP(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), E_TYPE_MAIL_PART_ITIP, EMailPartItip))
#define E_MAIL_PART_ITIP_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((cls), E_TYPE_MAIL_PART_ITIP, EMailPartItipClass))
#define E_IS_MAIL_PART_ITIP(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), E_TYPE_MAIL_PART_ITIP))
#define E_IS_MAIL_PART_ITIP_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_TYPE \
	((cls), E_TYPE_MAIL_PART_ITIP))
#define E_MAIL_PART_ITIP_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS \
	((obj), E_TYPE_MAIL_PART_ITIP, EMailPartItipClass))

G_BEGIN_DECLS

typedef struct _EMailPartItip EMailPartItip;
typedef struct _EMailPartItipClass EMailPartItipClass;
typedef struct _EMailPartItipPrivate EMailPartItipPrivate;

struct _EMailPartItip {
	EMailPart parent;
	EMailPartItipPrivate *priv;

	CamelFolder *folder;
	CamelMimeMessage *message;
	gchar *message_uid;
	CamelMimePart *itip_mime_part;
	gchar *vcalendar;
	gchar *alternative_html;
	gboolean alternative_html_is_from_plain_text;

	/* cancelled when freeing the puri */
	GCancellable *cancellable;
};

struct _EMailPartItipClass {
	EMailPartClass parent_class;
};

GType		e_mail_part_itip_get_type	(void) G_GNUC_CONST;
void		e_mail_part_itip_type_register	(GTypeModule *type_module);
EMailPartItip *	e_mail_part_itip_new		(CamelMimePart *mime_part,
						 const gchar *id);

G_END_DECLS

#endif /* E_MAIL_PART_ITIP_H */
