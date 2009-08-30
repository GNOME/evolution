/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* e-addressbook-selector.h
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of version 2 of the GNU General Public
 * License as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#ifndef E_ADDRESSBOOK_SELECTOR_H
#define E_ADDRESSBOOK_SELECTOR_H

#include <libedataserver/e-source-list.h>
#include <libedataserverui/e-source-selector.h>
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
	ESourceSelector parent;
	EAddressbookSelectorPrivate *priv;
};

struct _EAddressbookSelectorClass {
	ESourceSelectorClass parent_class;
};

GType		e_addressbook_selector_get_type	(void);
GtkWidget *	e_addressbook_selector_new	(ESourceList *source_list);
EAddressbookView *
		e_addressbook_selector_get_current_view
						(EAddressbookSelector *selector);
void		e_addressbook_selector_set_current_view
						(EAddressbookSelector *selector,
						 EAddressbookView *current_view);

G_END_DECLS

#endif /* E_ADDRESSBOOK_SELECTOR_H */
