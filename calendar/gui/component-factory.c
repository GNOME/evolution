/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* component-factory.c
 *
 * Copyright (C) 2000  Helix Code, Inc.
 * Copyright (C) 2000  Ximian, Inc.
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
 * Author: Ettore Perazzoli <ettore@ximian.com>
 */

#include <config.h>
#include <bonobo.h>
#include "evolution-shell-component.h"
#include <executive-summary/evolution-services/executive-summary-component.h>
#include "component-factory.h"
#include "tasks-control-factory.h"
#include "control-factory.h"
#include "calendar-config.h"
#include "calendar-summary.h"
#include "tasks-control.h"
#include "tasks-migrate.h"



#define COMPONENT_FACTORY_ID "OAFIID:GNOME_Evolution_Calendar_ShellComponentFactory"

static BonoboGenericFactory *factory = NULL;
static BonoboGenericFactory *summary_factory = NULL;
char *evolution_dir;

static const EvolutionShellComponentFolderType folder_types[] = {
	{ "calendar", "evolution-calendar.png" },
	{ "tasks", "evolution-tasks.png" },
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

	if (!g_strcasecmp (type, "calendar")) {
		control = control_factory_new_control ();
		if (!control)
			return EVOLUTION_SHELL_COMPONENT_CORBAERROR;
	} else if (!g_strcasecmp (type, "tasks")) {
		control = tasks_control_new ();
		if (!control)
			return EVOLUTION_SHELL_COMPONENT_CORBAERROR;
	} else {
		return EVOLUTION_SHELL_COMPONENT_UNSUPPORTEDTYPE;
	}

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

	CORBA_exception_init(&ev);
	/* FIXME: I don't think we have to do anything to create a calendar
	   or tasks folder - the '.ics' files are created automatically when
	   needed. But I'm not sure - Damon. */
	if (!strcmp(type, "calendar") || !strcmp(type, "tasks")) {
		GNOME_Evolution_ShellComponentListener_notifyResult(listener, GNOME_Evolution_ShellComponentListener_OK, &ev);
	} else {
		GNOME_Evolution_ShellComponentListener_notifyResult(listener, GNOME_Evolution_ShellComponentListener_UNSUPPORTED_TYPE, &ev);
	}
	CORBA_exception_free(&ev);
}

static gint owner_count = 0;

static void
owner_set_cb (EvolutionShellComponent *shell_component,
	      EvolutionShellClient *shell_client,
	      const char *evolution_homedir,
	      gpointer user_data)
{
	static gboolean migrated = FALSE;

	owner_count ++;
	evolution_dir = g_strdup (evolution_homedir);

	calendar_config_init ();

	if (!migrated) {
		tasks_migrate ();
		migrated = TRUE;
	}
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

	shell_component = evolution_shell_component_new (folder_types,
							 create_view,
							 create_folder,
							 NULL, /* remove_folder_fn */
							 NULL, /* copy_folder_fn */
							 NULL, /* populate_folder_context_menu_fn */
							 NULL, /* get_dnd_selection_fn */
							 NULL  /* closure */);

	gtk_signal_connect (GTK_OBJECT (shell_component), "owner_set",
			    GTK_SIGNAL_FUNC (owner_set_cb), NULL);
	gtk_signal_connect (GTK_OBJECT (shell_component), "owner_unset",
			    GTK_SIGNAL_FUNC (owner_unset_cb), NULL);

	return BONOBO_OBJECT (shell_component);
}



void
component_factory_init (void)
{
	if (factory != NULL && factory != NULL)
		return;

	factory = bonobo_generic_factory_new (COMPONENT_FACTORY_ID, factory_fn, NULL);

	summary_factory = calendar_summary_factory_init ();

	if (factory == NULL)
		g_error ("Cannot initialize Evolution's calendar component.");

	if (summary_factory == NULL)
		g_error ("Cannot initialize Evolution's calendar summary component.");
}
