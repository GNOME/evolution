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

#include <config.h>
#include <bonobo.h>
#include "evolution-shell-component.h"
#ifdef WANT_THE_EXECUTIVE_SUMMARY
#include <executive-summary/evolution-services/executive-summary-component.h>
#endif
#include "component-factory.h"
#include "control-factory.h"
#include "calendar-config.h"
#if WANT_THE_EXECUTIVE_SUMMARY
#include "calendar-summary.h"
#endif


#define COMPONENT_FACTORY_ID "OAFIID:GNOME_Evolution_Calendar_ShellComponentFactory"
#define SUMMARY_FACTORY_ID   "OAFIID:GNOME_Evolution_Calendar_ExecutiveSummaryComponentFactory"

static BonoboGenericFactory *factory = NULL;
#ifdef WANT_THE_EXECUTIVE_SUMMARY
static BonoboGenericFactory *summary_factory = NULL;
#endif
char *evolution_dir;

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
	      EvolutionShellClient *shell_client,
	      const char *evolution_homedir,
	      gpointer user_data)
{
	evolution_dir = g_strdup (evolution_homedir);
	calendar_config_init ();
	owner_count ++;
}

static void
owner_unset_cb (EvolutionShellComponent *shell_component,
		gpointer user_data)
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

#ifdef WANT_THE_EXECUTIVE_SUMMARY
static BonoboObject *
summary_fn (BonoboGenericFactory *factory, 
	    void *closure)
{
	ExecutiveSummaryComponent *summary_component;

	summary_component = executive_summary_component_new (NULL,
							     create_summary_view,
							     NULL,
							     evolution_dir);
	return BONOBO_OBJECT (summary_component);
}
#endif


void
component_factory_init (void)
{
	if (factory != NULL && factory != NULL)
		return;

	factory = bonobo_generic_factory_new (COMPONENT_FACTORY_ID, factory_fn, NULL);

#ifdef WANT_THE_EXECUTIVE_SUMMARY
	summary_factory = bonobo_generic_factory_new (SUMMARY_FACTORY_ID, summary_fn, NULL);
#endif
	if (factory == NULL)
		g_error ("Cannot initialize Evolution's calendar component.");

#ifdef WANT_THE_EXECUTIVE_SUMMARY
	if (summary_factory == NULL)
		g_error ("Cannot initialize Evolution's calendar summary component.");
#endif
}
