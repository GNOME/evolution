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

#include "Evolution.h"
#include "evolution-storage.h"

#include "evolution-shell-component.h"
#include "folder-browser.h"
#include "mail.h"		/* YUCK FIXME */

#include "filter/filter-driver.h"
#include "component-factory.h"

static void create_test_storage (EvolutionShellComponent *shell_component);


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

	/* dum de dum, hack to let the folder browser know the storage its in */
	gtk_object_set_data((GtkObject *)folder_browser_widget, "e-storage",
			    gtk_object_get_data((GtkObject *)shell_component, "e-storage"));

	/* FIXME: This never fails.  :-/  */
	folder_browser_set_uri (FOLDER_BROWSER (folder_browser_widget), physical_uri);

	return control;
}

static void
owner_set_cb (EvolutionShellComponent *shell_component,
	      Evolution_Shell shell_interface,
	      gpointer user_data)
{
	g_print ("evolution-mail: Yeeeh! We have an owner!\n");	/* FIXME */

	create_test_storage (shell_component);
}

static void
owner_unset_cb (EvolutionShellComponent *shell_component, gpointer user_data)
{
	gtk_main_quit ();
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

	if (factory == NULL) {
		e_notice (NULL, GNOME_MESSAGE_BOX_ERROR,
			  _("Cannot initialize Evolution's mail component."));
		exit (1);
	}
}

static void
create_test_storage (EvolutionShellComponent *shell_component)
{
	Evolution_Shell corba_shell;
	EvolutionStorage *storage;
	
	corba_shell = evolution_shell_component_get_owner (shell_component);
	if (corba_shell == CORBA_OBJECT_NIL) {
		g_warning ("We have no shell!?");
		return;
	}
    
	storage = evolution_storage_new ("VFolders");
	if (evolution_storage_register_on_shell (storage, corba_shell)
	    != EVOLUTION_STORAGE_OK) {
		g_warning ("Cannot register storage");
		return;
	}

	/* save the storage for later */
	gtk_object_set_data((GtkObject *)shell_component, "e-storage", storage);

	/* this is totally not the way we want to do this - but the
	   filter stuff needs work before we can remove it */
	{
		FilterDriver *fe;
		int i, count;
		char *user, *system;
		extern char *evolution_dir;

		fe = filter_driver_new();
		user = g_strdup_printf ("%s/vfolders.xml", evolution_dir);
		system = g_strdup_printf("%s/evolution/vfoldertypes.xml", EVOLUTION_DATADIR);
		filter_driver_set_rules(fe, system, user);
		g_free(user);
		g_free(system);
		count = filter_driver_rule_count(fe);

		for (i=0;i<count;i++) {
			struct filter_option *fo;
			GString *query;
			struct filter_desc *desc = NULL;
			char *desctext, descunknown[64];
			char *name;

			fo = filter_driver_rule_get(fe, i);
			if (fo == NULL)
				continue;
			query = g_string_new("");
			if (fo->description)
				desc = fo->description->data;
			if (desc)
				desctext = desc->data;
			else {
				sprintf(descunknown, "volder-%p", fo);
				desctext = descunknown;
			}
			g_string_sprintf(query, "vfolder:%s/vfolder/%s?", evolution_dir, desctext);
			filter_driver_expand_option(fe, query, NULL, fo);
			name = g_strdup_printf("/%s", desctext);
			printf("Adding new vfolder: %s\n", query->str);
			evolution_storage_new_folder (storage, name,
						      "mail",
						      query->str,
						      desctext);
			g_string_free(query, TRUE);
			g_free(name);
		}
		gtk_object_unref((GtkObject *)fe);
	}
}

