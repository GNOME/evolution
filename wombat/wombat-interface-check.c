/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* wombat-interface-check.c
 *
 * Copyright (C) 2002  Ximian, Inc.
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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "wombat-interface-check.h"

#include <gal/util/e-util.h>


#define PARENT_TYPE bonobo_x_object_get_type ()
static BonoboXObjectClass *parent_class = NULL;


static CORBA_char *
impl__get_interfaceVersion (PortableServer_Servant servant,
			    CORBA_Environment *ev)
{
	return CORBA_string_dup (VERSION);
}


static void
class_init (WombatInterfaceCheckClass *class)
{
	parent_class = gtk_type_class (PARENT_TYPE);

	class->epv._get_interfaceVersion = impl__get_interfaceVersion;
}

static void
init (WombatInterfaceCheck *interface_check)
{
	/* (Nothing to initialize here.)  */
}


WombatInterfaceCheck *
wombat_interface_check_new (void)
{
	return gtk_type_new (WOMBAT_TYPE_INTERFACE_CHECK);
}


E_MAKE_X_TYPE (wombat_interface_check, "WombatInterfaceCheck", WombatInterfaceCheck,
	       class_init, init, PARENT_TYPE,
	       POA_GNOME_Evolution_WombatInterfaceCheck__init,
	       GTK_STRUCT_OFFSET (WombatInterfaceCheckClass, epv))
