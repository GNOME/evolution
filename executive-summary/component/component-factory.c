/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* component-factory.c
 *
 * Authors: Ettore Perazzoli <ettore@helixcode.com>
 *          Iain Holmes <iain@helixcode.com>
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
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <bonobo/bonobo-object.h>
#include <bonobo/bonobo-generic-factory.h>

#include "Evolution.h"
#include "evolution-storage.h"

#include "evolution-shell-component.h"
#include <gal/widgets/e-gui-utils.h>

#include "component-factory.h"
#include "e-summary-factory.h"

#define COMPONENT_FACTORY_IID "OAFIID:GNOME_Evolution_Summary_ShellComponentFactory"

static BonoboGenericFactory *factory = NULL;
static gint running_objects = 0;

static const EvolutionShellComponentFolderType folder_types[] = {
	{ "executive-summary", "evolution-today.png" },
	{ NULL, NULL }
};

char *evolution_dir;

/* EvolutionShellComponent methods and signals */

static EvolutionShellComponentResult
create_view (EvolutionShellComponent *shell_component,
	     const char *physical_uri,
	     const char *folder_type,
	     BonoboControl **control_return,
	     void *closure)
{
	EvolutionShellClient *shell_client;
	GNOME_Evolution_Shell corba_shell;
	BonoboControl *control;
	
	if (g_strcasecmp (folder_type, "executive-summary") != 0)
		return EVOLUTION_SHELL_COMPONENT_UNSUPPORTEDTYPE;
	
	shell_client = evolution_shell_component_get_owner (shell_component);
	corba_shell = bonobo_object_corba_objref (BONOBO_OBJECT (shell_client));
	
	control = e_summary_factory_new_control (physical_uri, corba_shell);
	if (!control)
		return EVOLUTION_SHELL_COMPONENT_NOTFOUND;
	
	*control_return = control;
	
	return EVOLUTION_SHELL_COMPONENT_OK;
}

static void
owner_set_cb (EvolutionShellComponent *shell_component,
	      EvolutionShellClient *shell_client,
	      const char *evolution_homedir,
	      gpointer user_data)
{
	GNOME_Evolution_Shell corba_shell;
	
	evolution_dir = g_strdup (evolution_homedir);
	
	corba_shell = bonobo_object_corba_objref (BONOBO_OBJECT (shell_client));
}

static void
owner_unset_cb (EvolutionShellComponent *shell_component,
		gpointer user_data)
{
	gtk_main_quit ();
}

static void
factory_destroy (BonoboObject *component,
		 gpointer dummy)
{
	running_objects--;
	
	if (running_objects > 0)
		return;
	
	if (factory)
		bonobo_object_unref (BONOBO_OBJECT (factory));
	else
		g_warning ("Serious ref counting error");
	factory = NULL;
	
	gtk_main_quit ();
}

static BonoboObject *
factory_fn (BonoboGenericFactory *factory, 
	    void *closure)
{
	EvolutionShellComponent *shell_component;
	
	running_objects++;
	
	shell_component = evolution_shell_component_new (folder_types,
							 create_view,
							 NULL, NULL, NULL, NULL, NULL,
							 NULL);
	gtk_signal_connect (GTK_OBJECT (shell_component), "destroy",
			    GTK_SIGNAL_FUNC (factory_destroy), NULL);
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
	
	factory = bonobo_generic_factory_new (COMPONENT_FACTORY_IID, 
					      factory_fn, NULL);
	
	if (factory == NULL) {
		e_notice (NULL, GNOME_MESSAGE_BOX_ERROR,
			  _("Cannot initialize Evolution's Executive Summary component."));
		exit (1);
	}
}
