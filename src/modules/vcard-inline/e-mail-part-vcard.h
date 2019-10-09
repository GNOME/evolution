/*
 * e-mail-part-vcard.h
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

#ifndef E_MAIL_PART_VCARD_H
#define E_MAIL_PART_VCARD_H

#include <em-format/e-mail-part.h>

#include <addressbook/gui/widgets/eab-contact-formatter.h>

/* Standard GObject macros */
#define E_TYPE_MAIL_PART_VCARD \
	(e_mail_part_vcard_get_type ())
#define E_MAIL_PART_VCARD(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), E_TYPE_MAIL_PART_VCARD, EMailPartVCard))
#define E_MAIL_PART_VCARD_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((cls), E_TYPE_MAIL_PART_VCARD, EMailPartVCardClass))
#define E_IS_MAIL_PART_VCARD(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), E_TYPE_MAIL_PART_VCARD))
#define E_IS_MAIL_PART_VCARD_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_TYPE \
	((cls), E_TYPE_MAIL_PART_VCARD))
#define E_MAIL_PART_VCARD_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS \
	((obj), E_TYPE_MAIL_PART_VCARD, EMailPartVCardClass))

G_BEGIN_DECLS

typedef struct _EMailPartVCard EMailPartVCard;
typedef struct _EMailPartVCardClass EMailPartVCardClass;
typedef struct _EMailPartVCardPrivate EMailPartVCardPrivate;

struct _EMailPartVCard {
	EMailPart parent;
	EMailPartVCardPrivate *priv;
};

struct _EMailPartVCardClass {
	EMailPartClass parent_class;
};

GType		e_mail_part_vcard_get_type	(void) G_GNUC_CONST;
void		e_mail_part_vcard_type_register	(GTypeModule *type_module);
EMailPartVCard *
		e_mail_part_vcard_new		(CamelMimePart *mime_part,
						 const gchar *id);
void		e_mail_part_vcard_take_contacts	(EMailPartVCard *vcard_part,
						 GSList *contacts);
const GSList *	e_mail_part_vcard_get_contacts	(EMailPartVCard *vcard_part);

G_END_DECLS

#endif /* E_MAIL_PART_VCARD_H */

