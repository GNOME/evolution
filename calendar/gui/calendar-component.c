/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* component-factory.c
 *
 * Copyright (C) 2000  Helix Code, Inc.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
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

#include <bonobo.h>
#include "evolution-shell-component.h"
#include "component-factory.h"
#include "control-factory.h"


#ifdef USING_OAF
#define COMPONENT_FACTORY_ID "OAFIID:evolution-shell-component-factory:evolution-calendar:cba77062-1466-4aac-8ce7-b019eaf2e921"
#else
#define COMPONENT_FACTORY_ID "evolution-shell-component-factory:evolution-calendar"
#endif

static BonoboGenericFactory *factory = NULL;

static const EvolutionShellComponentFolderType folder_types[] = {
	{ "calendar", "evolution-calendar.png" },
	{ NULL, NULL }
};


/* EvolutionShellComponent methods and signals.  */

static EvolutionShellComponentResult
create_view (EvolutionShellComponent *shell_component,
	     const char *physical_uri,
	     const char *type,
	     BonoboControl **control_return,
	     void *closure)
{
	BonoboControl *control;

	if (g_strcasecmp (type, "calendar") != 0)
		return EVOLUTION_SHELL_COMPONENT_UNSUPPORTEDTYPE;

	control = control_factory_new_control ();
	bonobo_control_set_property (control, "folder_uri", physical_uri, NULL);

	*control_return = control;

	return EVOLUTION_SHELL_COMPONENT_OK;
}

static gint owner_count = 0;

static void
owner_set_cb (EvolutionShellComponent *shell_component,
	      Evolution_Shell shell_interface,
	      const char *evolution_homedir)
{
	owner_count ++;
}

static void
owner_unset_cb (EvolutionShellComponent *shell_component,
		Evolution_Shell shell_interface,
		EvolutionShellClient shell_client,
		void *data)
{
	owner_count --;
	if (owner_count <= 0)
		gtk_main_quit();
}


/* The factory function.  */

static BonoboObject *
factory_fn (BonoboGenericFactory *factory,
	    void *closure)
{
	EvolutionShellComponent *shell_component;

	shell_component = evolution_shell_component_new (folder_types, create_view, NULL, NULL, NULL, NULL);

	gtk_signal_connect (GTK_OBJECT (shell_component), "owner_set",
			    GTK_SIGNAL_FUNC (owner_set_cb), NULL);
	gtk_signal_connect (GTK_OBJECT (shell_component), "owner_unset",
			    GTK_SIGNAL_FUNC (owner_unset_cb), NULL);

	return BONOBO_OBJECT (shell_component);
}


void
component_factory_init (void)
{
	if (factory != NULL)
		return;

	factory = bonobo_generic_factory_new (COMPONENT_FACTORY_ID, factory_fn, NULL);

	if (factory == NULL)
		g_error ("Cannot initialize Evolution's calendar component.");
}
