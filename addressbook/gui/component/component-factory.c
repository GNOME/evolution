/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* component-factory.c - Factory for Evolution's Addressbook component.
 *
 * Copyright (C) 2002 Ximian, Inc.
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

#include <config.h>

#include <string.h>
#include "addressbook.h"
#include "addressbook-component.h"
#include "addressbook-config.h"
#include "e-address-popup.h"
#include "e-address-widget.h"
#include "e-minicard-control.h"
#include "select-names/e-select-names-bonobo.h"

#include <bonobo/bonobo-shlib-factory.h>


#define FACTORY_ID "OAFIID:GNOME_Evolution_Addressbook_Factory_2"

#define MINICARD_CONTROL_ID            "OAFIID:GNOME_Evolution_Addressbook_MiniCard_Control"
#define ADDRESSBOOK_CONTROL_ID         "OAFIID:GNOME_Evolution_Addressbook_Control"
#define COMPONENT_ID                   "OAFIID:GNOME_Evolution_Addressbook_Component"
#define ADDRESS_WIDGET_ID              "OAFIID:GNOME_Evolution_Addressbook_AddressWidget"
#define ADDRESS_POPUP_ID               "OAFIID:GNOME_Evolution_Addressbook_AddressPopup"
#define SELECT_NAMES_ID                "OAFIID:GNOME_Evolution_Addressbook_SelectNames"
#define LDAP_STORAGE_CONFIG_CONTROL_ID "OAFIID:GNOME_Evolution_LDAPStorage_ConfigControl"


static BonoboObject *
factory (BonoboGenericFactory *factory,
	 const char *component_id,
	 void *closure)
{
	if (strcmp (component_id, MINICARD_CONTROL_ID) == 0)
		return BONOBO_OBJECT (e_minicard_control_new ());
	if (strcmp (component_id, ADDRESSBOOK_CONTROL_ID) == 0)
		return BONOBO_OBJECT (addressbook_new_control ());
	if (strcmp (component_id, COMPONENT_ID) == 0) {
		BonoboObject *object = BONOBO_OBJECT (addressbook_component_peek ());
		bonobo_object_ref (object);
		return object;
	} else if (strcmp (component_id, ADDRESS_WIDGET_ID) == 0)
		return BONOBO_OBJECT (e_address_widget_new_control ());
	if (strcmp (component_id, ADDRESS_POPUP_ID) == 0)
		return BONOBO_OBJECT (e_address_popup_new_control ());
	if (strcmp (component_id, LDAP_STORAGE_CONFIG_CONTROL_ID) == 0)
		return BONOBO_OBJECT (addressbook_config_control_new ());
	if (strcmp (component_id, SELECT_NAMES_ID) == 0)
		return BONOBO_OBJECT (e_select_names_bonobo_new ());

	g_warning (FACTORY_ID ": Don't know what to do with %s", component_id);
	return NULL;
}

BONOBO_ACTIVATION_SHLIB_FACTORY (FACTORY_ID, "Evolution Addressbook component factory", factory, NULL)
