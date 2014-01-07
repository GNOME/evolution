/*
 * e-source-contacts.h
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

#ifndef E_SOURCE_CONTACTS_H
#define E_SOURCE_CONTACTS_H

#include <libedataserver/libedataserver.h>

/* Standard GObject macros */
#define E_TYPE_SOURCE_CONTACTS \
	(e_source_contacts_get_type ())
#define E_SOURCE_CONTACTS(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), E_TYPE_SOURCE_CONTACTS, ESourceContacts))
#define E_SOURCE_CONTACTS_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((cls), E_TYPE_SOURCE_CONTACTS, ESourceContactsClass))
#define E_IS_SOURCE_CONTACTS(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), E_TYPE_SOURCE_CONTACTS))
#define E_IS_SOURCE_CONTACTS_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_TYPE \
	((cls), E_TYPE_SOURCE_CONTACTS))
#define E_SOURCE_CONTACTS_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS \
	((obj), E_TYPE_SOURCE_CONTACTS, ESourceContactsClass))

#define E_SOURCE_EXTENSION_CONTACTS_BACKEND "Contacts Backend"

G_BEGIN_DECLS

typedef struct _ESourceContacts ESourceContacts;
typedef struct _ESourceContactsClass ESourceContactsClass;
typedef struct _ESourceContactsPrivate ESourceContactsPrivate;

struct _ESourceContacts {
	ESourceExtension parent;
	ESourceContactsPrivate *priv;
};

struct _ESourceContactsClass {
	ESourceExtensionClass parent_class;
};

GType		e_source_contacts_get_type	(void);
void		e_source_contacts_type_register	(GTypeModule *type_module);
gboolean	e_source_contacts_get_include_me
						(ESourceContacts *extension);
void		e_source_contacts_set_include_me
						(ESourceContacts *extension,
						 gboolean include_me);

G_END_DECLS

#endif /* E_SOURCE_CONTACTS_H */
