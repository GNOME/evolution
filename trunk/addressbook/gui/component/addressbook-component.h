/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* addressbook-component.h
 *
 * Copyright (C) 2003  Ximian, Inc.
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
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 * Author: Ettore Perazzoli <ettore@ximian.com>
 */

#ifndef _ADDRESSBOOK_COMPONENT_H_
#define _ADDRESSBOOK_COMPONENT_H_

#include <bonobo/bonobo-object.h>

#include "Evolution.h"
#include "e-activity-handler.h"
#include <libedataserver/e-source-list.h>

#define ADDRESSBOOK_TYPE_COMPONENT			(addressbook_component_get_type ())
#define ADDRESSBOOK_COMPONENT(obj)			(G_TYPE_CHECK_INSTANCE_CAST ((obj), ADDRESSBOOK_TYPE_COMPONENT, AddressbookComponent))
#define ADDRESSBOOK_COMPONENT_CLASS(klass)		(G_TYPE_CHECK_CLASS_CAST ((klass), ADDRESSBOOK_TYPE_COMPONENT, AddressbookComponentClass))
#define ADDRESSBOOK_IS_COMPONENT(obj)			(G_TYPE_CHECK_INSTANCE_TYPE ((obj), ADDRESSBOOK_TYPE_COMPONENT))
#define ADDRESSBOOK_IS_COMPONENT_CLASS(klass)		(G_TYPE_CHECK_CLASS_TYPE ((obj), ADDRESSBOOK_TYPE_COMPONENT))


typedef struct _AddressbookComponent        AddressbookComponent;
typedef struct _AddressbookComponentPrivate AddressbookComponentPrivate;
typedef struct _AddressbookComponentClass   AddressbookComponentClass;

struct _AddressbookComponent {
	BonoboObject parent;

	AddressbookComponentPrivate *priv;
};

struct _AddressbookComponentClass {
	BonoboObjectClass parent_class;

	POA_GNOME_Evolution_Component__epv epv;
};


GType addressbook_component_get_type (void);

AddressbookComponent *addressbook_component_peek (void);

GConfClient       *addressbook_component_peek_gconf_client     (AddressbookComponent *component);
const char        *addressbook_component_peek_base_directory   (AddressbookComponent *component);

#endif /* _ADDRESSBOOK_COMPONENT_H_ */
