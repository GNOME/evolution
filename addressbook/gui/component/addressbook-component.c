/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* addressbook-component.c
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

#include <bonobo/bonobo-generic-factory.h>

#include "evolution-shell-component.h"
#include "evolution-storage.h"

#include "addressbook-storage.h"
#include "addressbook-component.h"
#include "addressbook.h"



#define GNOME_EVOLUTION_ADDRESSBOOK_COMPONENT_FACTORY_ID "OAFIID:GNOME_Evolution_Addressbook_ShellComponentFactory"

EvolutionShellClient *global_shell_client;

EvolutionShellClient *
addressbook_component_get_shell_client  (void)
{
	return global_shell_client;
}

static BonoboGenericFactory *factory = NULL;

static char *accepted_dnd_types[] = {
	"text/x-vcard",
	NULL
};

static const EvolutionShellComponentFolderType folder_types[] = {
	{ "contacts", "evolution-contacts.png", accepted_dnd_types },
	{ NULL, NULL, NULL, NULL }
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

	if (g_strcasecmp (type, "contacts") != 0)
		return EVOLUTION_SHELL_COMPONENT_UNSUPPORTEDTYPE;

	control = addressbook_factory_new_control ();
	bonobo_control_set_property (control, "folder_uri", physical_uri, NULL);

	*control_return = control;

	return EVOLUTION_SHELL_COMPONENT_OK;
}

static void
create_folder (EvolutionShellComponent *shell_component,
	       const char *physical_uri,
	       const char *type,
	       const GNOME_Evolution_ShellComponentListener listener,
	       void *closure)
{
	CORBA_Environment ev;
	GNOME_Evolution_ShellComponentListener_Result result;

	if (g_strcasecmp (type, "contacts") != 0)
		result = GNOME_Evolution_ShellComponentListener_UNSUPPORTED_TYPE;
	else 
		result = GNOME_Evolution_ShellComponentListener_OK;

	CORBA_exception_init(&ev);
	GNOME_Evolution_ShellComponentListener_notifyResult(listener, result, &ev);
	CORBA_exception_free(&ev);
}

#if 0
static void
remove_folder (EvolutionShellComponent *shell_component,
	       const char *physical_uri,
	       const GNOME_Evolution_ShellComponentListener listener,
	       void *closure)
{
	printf ("should remove %s\n", physical_uri);
}

static void
xfer_folder (EvolutionShellComponent *shell_component,
	     const char *source_physical_uri,
	     const char *destination_physical_uri,
	     gboolean remove_source,
	     const GNOME_Evolution_ShellComponentListener listener,
	     void *closure)
{
	printf ("should transfer %s to %s, %s source\n", source_physical_uri,
		destination_physical_uri, remove_source ? "removing" : "not removing");
}

static void
populate_context_menu (EvolutionShellComponent *shell_component,
		       BonoboUIComponent *uic,
		       const char *physical_uri,
		       const char *type,
		       void *closure)
{
	printf ("should populate context menu for %s (%s)\n", physical_uri, type);
}

static char*
get_dnd_selection (EvolutionShellComponent *shell_component,
		   const char *physical_uri,
		   int type,
		   int *format_return,
		   const char **selection_return,
		   int *selection_length_return,
		   void *closure)
{
	printf ("should get dnd selection for %s\n", physical_uri);
	return NULL;
}
#endif

static int owner_count = 0;

static void
owner_set_cb (EvolutionShellComponent *shell_component,
	      EvolutionShellClient *shell_client,
	      const char *evolution_homedir,
	      gpointer user_data)
{
	owner_count ++;

	if (global_shell_client == NULL)
		global_shell_client = shell_client;

	addressbook_storage_setup (shell_component, evolution_homedir);
}

static void
owner_unset_cb (EvolutionShellComponent *shell_component,
		GNOME_Evolution_Shell shell_interface,
		gpointer user_data)
{
	owner_count --;
	if (owner_count == 0)
		gtk_main_quit();
}


/* The factory function.  */

static BonoboObject *
factory_fn (BonoboGenericFactory *factory,
	    void *closure)
{
	EvolutionShellComponent *shell_component;

	shell_component = evolution_shell_component_new (folder_types, create_view, create_folder,
							 NULL, NULL, NULL, NULL,
#if 0
							 remove_folder, xfer_folder, 
							 populate_context_menu, get_dnd_selection,
#endif
							 NULL);

	gtk_signal_connect (GTK_OBJECT (shell_component), "owner_set",
			    GTK_SIGNAL_FUNC (owner_set_cb), NULL);

	gtk_signal_connect (GTK_OBJECT (shell_component), "owner_unset",
			    GTK_SIGNAL_FUNC (owner_unset_cb), NULL);

	return BONOBO_OBJECT (shell_component);
}


void
addressbook_component_factory_init (void)
{
	if (factory != NULL)
		return;

	factory = bonobo_generic_factory_new (GNOME_EVOLUTION_ADDRESSBOOK_COMPONENT_FACTORY_ID, factory_fn, NULL);

	if (factory == NULL)
		g_error ("Cannot initialize the Evolution addressbook factory.");
}

