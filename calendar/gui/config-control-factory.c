/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* config-control-factory.c
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
 * Author: Ettore Perazzoli
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "config-control-factory.h"

#include "dialogs/cal-prefs-dialog.h"

#include <bonobo/bonobo-generic-factory.h>


#define CONFIG_CONTROL_FACTORY_ID "OAFIID:GNOME_Evolution_Calendar_ConfigControlFactory"
static BonoboGenericFactory *generic_factory = NULL;


static BonoboObject *
factory_fn (BonoboGenericFactory *generic_factory,
	    void *data)
{
	EvolutionConfigControl *config_control;

	config_control = cal_prefs_dialog_new ();

	return BONOBO_OBJECT (config_control);
}

gboolean
config_control_factory_register (GNOME_Evolution_Shell shell)
{
	generic_factory = bonobo_generic_factory_new (CONFIG_CONTROL_FACTORY_ID,
						      factory_fn, shell);

	if (generic_factory == NULL) {
		g_warning ("Cannot register %s", CONFIG_CONTROL_FACTORY_ID);
		return FALSE;
	}

	return TRUE;
}
