/* e-addressbook-selector.h
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
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
 */

#ifndef E_ADDRESSBOOK_SELECTOR_H
#define E_ADDRESSBOOK_SELECTOR_H

#include "e-addressbook-view.h"

/* Standard GObject macros */
#define E_TYPE_ADDRESSBOOK_SELECTOR \
	(e_addressbook_selector_get_type ())
#define E_ADDRESSBOOK_SELECTOR(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), E_TYPE_ADDRESSBOOK_SELECTOR, EAddressbookSelector))
#define E_ADDRESSBOOK_SELECTOR_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((cls), E_TYPE_ADDRESSBOOK_SELECTOR, EAddressbookSelectorClass))
#define E_IS_ADDRESSBOOK_SELECTOR(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), E_TYPE_ADDRESSBOOK_SELECTOR))
#define E_IS_ADDRESSBOOK_SELECTOR_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_TYPE \
	((cls), E_TYPE_ADDRESSBOOK_SELECTOR))
#define E_ADDRESSBOOK_SELECTOR_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS \
	((obj), E_TYPE_ADDRESSBOOK_SELECTOR, EAddressbookSelectorClass))

G_BEGIN_DECLS

typedef struct _EAddressbookSelector EAddressbookSelector;
typedef struct _EAddressbookSelectorClass EAddressbookSelectorClass;
typedef struct _EAddressbookSelectorPrivate EAddressbookSelectorPrivate;

struct _EAddressbookSelector {
	EClientSelector parent;
	EAddressbookSelectorPrivate *priv;
};

struct _EAddressbookSelectorClass {
	EClientSelectorClass parent_class;
};

GType		e_addressbook_selector_get_type	(void);
GtkWidget *	e_addressbook_selector_new	(EClientCache *client_cache);
EAddressbookView *
		e_addressbook_selector_get_current_view
						(EAddressbookSelector *selector);
void		e_addressbook_selector_set_current_view
						(EAddressbookSelector *selector,
						 EAddressbookView *current_view);
gchar *		e_addressbook_selector_dup_selected_category
						(EAddressbookSelector *selector);

G_END_DECLS

#endif /* E_ADDRESSBOOK_SELECTOR_H */
