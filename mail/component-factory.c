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
#include "folder-browser.h"
#include "mail.h"		/* YUCK FIXME */

#include "component-factory.h"


#ifdef USING_OAF
#define COMPONENT_FACTORY_ID "OAFIID:evolution-shell-component-factory:evolution-mail:0ea887d5-622b-4b8c-b525-18aa1cbe18a6"
#else
#define COMPONENT_FACTORY_ID "evolution-shell-component-factory:evolution-mail"
#endif

static BonoboGenericFactory *factory = NULL;

static const EvolutionShellComponentFolderType folder_types[] = {
	{ "mail", "evolution-inbox.png" },
	{ NULL, NULL }
};


/* EvolutionShellComponent methods and signals.  */

static BonoboControl *
create_view (EvolutionShellComponent *shell_component,
	     const char *physical_uri,
	     void *closure)
{
	BonoboControl *control;
	GtkWidget *folder_browser_widget;

	control = folder_browser_factory_new_control ();

	folder_browser_widget = bonobo_control_get_widget (control);

	g_assert (folder_browser_widget != NULL);
	g_assert (IS_FOLDER_BROWSER (folder_browser_widget));

	/* FIXME: This never fails.  :-/  */
	folder_browser_set_uri (FOLDER_BROWSER (folder_browser_widget), physical_uri);

	return control;
}

static void
owner_set_cb (EvolutionShellComponent *shell_component,
	      Evolution_Shell shell_interface)
{
	g_print ("evolution-mail: Yeeeh! We have an owner!\n");	/* FIXME */
}


/* The factory function.  */

static BonoboObject *
factory_fn (BonoboGenericFactory *factory,
	    void *closure)
{
	EvolutionShellComponent *shell_component;

	shell_component = evolution_shell_component_new (folder_types, create_view, NULL);

	gtk_signal_connect (GTK_OBJECT (shell_component), "owner_set",
			    GTK_SIGNAL_FUNC (owner_set_cb), NULL);

	return BONOBO_OBJECT (shell_component);
}


void
component_factory_init (void)
{
	if (factory != NULL)
		return;

	factory = bonobo_generic_factory_new (COMPONENT_FACTORY_ID, factory_fn, NULL);

	if (factory == NULL) {
		e_notice (NULL, GNOME_MESSAGE_BOX_ERROR,
			  _("Cannot initialize Evolution's mail component."));
		exit (1);
	}
}
