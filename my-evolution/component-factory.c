/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* component-factory.c
 *
 * Copyright (C) 2001 Ximian, Inc.
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
 * Author: Iain Holmes  <iain@ximian.com>
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <string.h>

#include <bonobo/bonobo-object.h>
#include <bonobo/bonobo-generic-factory.h>
#include <bonobo/bonobo-context.h>
#include <bonobo/bonobo-shlib-factory.h>
#include <bonobo/bonobo-main.h>

#include <shell/evolution-shell-component.h>
#include <shell/Evolution.h>

#include <gal/widgets/e-gui-utils.h>

#include "e-summary-factory.h"
#include "e-summary-offline-handler.h"
#include "e-summary.h"
#include "e-summary-preferences.h"

#include "component-factory.h"

#define FACTORY_ID             "OAFIID:GNOME_Evolution_Summary_ComponentFactory"

#define COMPONENT_ID           "OAFIID:GNOME_Evolution_Summary_ShellComponent"
#define PREFERENCES_CONTROL_ID "OAFIID:GNOME_Evolution_Summary_ConfigControl"

static gint running_objects = 0;
static ESummaryPrefs *global_preferences = NULL;

static const EvolutionShellComponentFolderType folder_types[] = {
	{ "summary", "evolution-today.png", N_("Summary"), N_("Folder containing the Evolution Summary"), FALSE, NULL, NULL },
	{ NULL, NULL }
};

static char *evolution_dir = NULL;

/* EvolutionShellComponent methods and signals */

static EvolutionShellComponentResult
create_view (EvolutionShellComponent *shell,
	     const char *physical_uri,
	     const char *folder_type,
	     const char *view_info,
	     BonoboControl **control_return,
	     void *closure)
{
	EvolutionShellClient *shell_client;
	ESummaryOfflineHandler *offline_handler;
	GNOME_Evolution_Shell corba_shell;
	BonoboControl *control;

	if (g_strcasecmp (folder_type, "Summary") != 0) {
		return EVOLUTION_SHELL_COMPONENT_UNSUPPORTEDTYPE;
	}

	offline_handler = g_object_get_data (G_OBJECT (shell), "offline-handler");
	shell_client = evolution_shell_component_get_owner (shell);
	corba_shell = evolution_shell_client_corba_objref (shell_client);
	control = e_summary_factory_new_control (physical_uri, corba_shell,
						 offline_handler, global_preferences);
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
	
	if (evolution_dir != NULL) {
		evolution_dir = g_strdup (evolution_homedir);
	}

	corba_shell = evolution_shell_client_corba_objref (shell_client);
	
	e_summary_folder_init_folder_store (corba_shell);
	e_summary_preferences_init_control (corba_shell);
}

static void
owner_unset_cb (EvolutionShellComponent *shell_component,
		gpointer user_data)
{
	g_free (evolution_dir);
	evolution_dir = NULL;

	e_summary_folder_clear_folder_store ();
}

static BonoboObject *
create_shell_component (void)
{
	EvolutionShellComponent *shell_component;
	ESummaryOfflineHandler *offline_handler;

	running_objects++;

	if (global_preferences == NULL) {
		global_preferences = e_summary_preferences_init ();
	}
	
	shell_component = evolution_shell_component_new (folder_types,
							 NULL,
							 create_view,
							 NULL, NULL, 
							 NULL, NULL,
							 NULL, NULL,
							 NULL, NULL);

	g_signal_connect (shell_component, "owner_set", G_CALLBACK (owner_set_cb), NULL);
	g_signal_connect (shell_component, "owner_unset", G_CALLBACK (owner_unset_cb), NULL);

	offline_handler = e_summary_offline_handler_new ();
	g_object_set_data (G_OBJECT (shell_component), "offline-handler", offline_handler);
	bonobo_object_add_interface (BONOBO_OBJECT (shell_component), BONOBO_OBJECT (offline_handler));

	return BONOBO_OBJECT (shell_component);
}

static BonoboObject *
factory (BonoboGenericFactory *this,
	 const char *object_id,
	 void *data)
{
	if (strcmp (object_id, COMPONENT_ID) == 0)
		return create_shell_component ();

	if (strcmp (object_id, PREFERENCES_CONTROL_ID) == 0)
		return e_summary_preferences_create_control ();

	g_warning (FACTORY_ID ": Don't know anything about %s", object_id);

	return NULL;
}

BONOBO_ACTIVATION_SHLIB_FACTORY (FACTORY_ID, "Evolution Summary component factory", factory, NULL)
