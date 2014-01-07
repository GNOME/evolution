/*
 * e-contacts-selector.h
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

#ifndef E_CONTACTS_SELECTOR_H
#define E_CONTACTS_SELECTOR_H

#include <e-util/e-util.h>

/* Standard GObject macros */
#define E_TYPE_CONTACTS_SELECTOR \
	(e_contacts_selector_get_type ())
#define E_CONTACTS_SELECTOR(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), E_TYPE_CONTACTS_SELECTOR, EContactsSelector))
#define E_CONTACTS_SELECTOR_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((cls), E_TYPE_CONTACTS_SELECTOR, EContactsSelectorClass))
#define E_IS_CONTACTS_SELECTOR(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), E_TYPE_CONTACTS_SELECTOR))
#define E_IS_CONTACTS_SELECTOR_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_TYPE \
	((cls), E_TYPE_CONTACTS_SELECTOR))
#define E_CONTACTS_SELECTOR_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS \
	((obj), E_TYPE_CONTACTS_SELECTOR, EContactsSelectorClass))

G_BEGIN_DECLS

typedef struct _EContactsSelector EContactsSelector;
typedef struct _EContactsSelectorClass EContactsSelectorClass;
typedef struct _EContactsSelectorPrivate EContactsSelectorPrivate;

struct _EContactsSelector {
	ESourceSelector parent;
	EContactsSelectorPrivate *priv;
};

struct _EContactsSelectorClass {
	ESourceSelectorClass parent_class;
};

GType		e_contacts_selector_get_type	(void);
void		e_contacts_selector_type_register
						(GTypeModule *type_module);
GtkWidget *	e_contacts_selector_new		(ESourceRegistry *registry);

G_END_DECLS

#endif /* E_CONTACTS_SELECTOR_H */
