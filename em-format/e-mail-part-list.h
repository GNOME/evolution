/*
 * e-mail-part-list.h
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) version 3.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with the program; if not, see <http://www.gnu.org/licenses/>
 *
 */

#ifndef E_MAIL_PART_LIST_H_
#define E_MAIL_PART_LIST_H_

#include <camel/camel.h>
#include <em-format/e-mail-part.h>

/* Standard GObject macros */
#define E_TYPE_MAIL_PART_LIST \
	(e_mail_part_list_get_type ())
#define E_MAIL_PART_LIST(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), E_TYPE_MAIL_PART_LIST, EMailPartList))
#define E_MAIL_PART_LIST_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((cls), E_TYPE_MAIL_PART_LIST, EMailPartListClass))
#define E_IS_MAIL_PART_LIST(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), E_TYPE_MAIL_PART_LIST))
#define E_IS_MAIL_PART_LIST_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_TYPE \
	((cls), E_TYPE_MAIL_PART_LIST))
#define E_MAIL_PART_LIST_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS \
	((obj), E_TYPE_MAIL_PART_LIST, EMailPartListClass))

G_BEGIN_DECLS

typedef struct _EMailPartList EMailPartList;
typedef struct _EMailPartListClass EMailPartListClass;

struct _EMailPartList {
	GObject parent;

	CamelMimeMessage *message;
	CamelFolder *folder;
	gchar *message_uid;

	/* GSList of EMailPart's */
	GSList *list;
};

struct _EMailPartListClass {
	GObjectClass parent_class;
};

EMailPartList *	e_mail_part_list_new		();

GType		e_mail_part_list_get_type	();

EMailPart *	e_mail_part_list_find_part	(EMailPartList *part_list,
						 const gchar *id);

GSList *		e_mail_part_list_get_iter	(GSList *list,
						 const gchar *id);

G_END_DECLS

#endif /* E_MAIL_PART_LIST_H_ */ 
