/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* evolution-test-component.c
 *
 * Copyright (C) 2001  Ximian, Inc.
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

/* Simple test component for the Evolution shell.  */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "evolution-shell-component.h"
#include "evolution-activity-client.h"

#include <bonobo/bonobo-generic-factory.h>
#include <bonobo/bonobo-main.h>

#include <gdk-pixbuf/gdk-pixbuf.h>


#define COMPONENT_FACTORY_ID "OAFIID:GNOME_Evolution_TestComponent_ShellComponentFactory"
#define COMPONENT_ID         "OAFIID:GNOME_Evolution_TestComponent_ShellComponent"

static const EvolutionShellComponentFolderType folder_types[] = {
	{ "test", "/usr/share/pixmaps/gnome-money.png", NULL, NULL },
	{ NULL }
};


static EvolutionShellClient *parent_shell = NULL;
static EvolutionActivityClient *activity_client;

static int timeout_id = 0;
static int progress = -1;


/* Callbacks.  */

static void
activity_client_clicked_callback (EvolutionActivityClient *client,
				  void *data)
{
	g_warning ("Task bar button clicked!");
}


/* Timeout #3: We are done.  */
static int
timeout_callback_3 (void *data)
{
	gtk_object_unref (GTK_OBJECT (activity_client));

	g_print ("--> Done.\n");

	return FALSE;
}

/* Timeout #2: Update the progress until it reaches 100%.  */
static int
timeout_callback_2 (void *data)
{
	if (progress < 0)
		progress = 0;

	g_print ("--> Updating %d\n", progress);

	if (! evolution_activity_client_update (activity_client, "Operation Foo in progress",
						(float) progress / 100.0)) {
		g_warning ("Error when updating operation");
		return FALSE;
	}

	progress ++;
	if (progress > 100) {
		gtk_timeout_add (200, timeout_callback_3, NULL);
		return FALSE;
	}

	return TRUE;
}

/* Timeout #1: Set busy.  */
static int
timeout_callback_1 (void *data)
{
	gboolean suggest_display;
	GdkPixbuf *animated_icon[2];

	animated_icon[0] = gdk_pixbuf_new_from_file (gnome_pixmap_file ("gnome-money.png"));
	animated_icon[1] = NULL;

	g_assert (animated_icon[0] != NULL);

	activity_client = evolution_activity_client_new (parent_shell, COMPONENT_ID,
							 animated_icon,
							 "Operation Foo started!",
							 FALSE,
							 &suggest_display);
	if (activity_client == CORBA_OBJECT_NIL) {
		g_warning ("Cannot create EvolutionActivityClient object");
		return FALSE;
	}

	gtk_signal_connect (GTK_OBJECT (activity_client), "clicked",
			    GTK_SIGNAL_FUNC (activity_client_clicked_callback), NULL);

	g_print ("Component becoming busy -- %s\n", COMPONENT_ID);
	if (suggest_display)
		g_print (" --> Could display dialog box.\n");

	gtk_timeout_add (100, timeout_callback_2, NULL);

	return FALSE;
}


static EvolutionShellComponentResult
create_view_fn (EvolutionShellComponent *shell_component,
		const char *physical_uri,
		const char *folder_type,
		BonoboControl **control_return,
		void *closure)
{
	GtkWidget *vbox;
	GtkWidget *label_1, *label_2;
	GtkWidget *event_box_1, *event_box_2;

	label_1 = gtk_label_new ("This is just a test component, displaying the following URI:");
	label_2 = gtk_label_new (physical_uri);

	event_box_1 = gtk_event_box_new ();
	event_box_2 = gtk_event_box_new ();

	vbox = gtk_vbox_new (FALSE, 5);

	gtk_box_pack_start (GTK_BOX (vbox), event_box_1, TRUE, TRUE, 0);
	gtk_box_pack_start (GTK_BOX (vbox), label_1, FALSE, TRUE, 0);
	gtk_box_pack_start (GTK_BOX (vbox), label_2, FALSE, TRUE, 0);
	gtk_box_pack_start (GTK_BOX (vbox), event_box_2, TRUE, TRUE, 0);

	gtk_widget_show (label_1);
	gtk_widget_show (label_2);
	gtk_widget_show (event_box_1);
	gtk_widget_show (event_box_2);

	gtk_widget_show (vbox);

	*control_return = bonobo_control_new (vbox);

	g_assert (timeout_id == 0);
	timeout_id = gtk_timeout_add (2000, timeout_callback_1, NULL);

	return EVOLUTION_SHELL_COMPONENT_OK;
}


/* Callbacks.  */

static void
owner_set_callback (EvolutionShellComponent *shell_component,
		    EvolutionShellClient *shell_client,
		    const char *evolution_homedir)
{
	g_assert (parent_shell == NULL);

	g_print ("We have an owner -- home directory is `%s'\n", evolution_homedir);

	parent_shell = shell_client;

	if (evolution_shell_client_get_activity_interface (parent_shell) == CORBA_OBJECT_NIL)
		g_warning ("Shell doesn't have a ::Activity interface -- weird!");
}

static int
owner_unset_idle_callback (void *data)
{
	gtk_main_quit ();
	return FALSE;
}

static void
owner_unset_callback (EvolutionShellComponent *shell_component,
		      void *data)
{
	g_idle_add_full (G_PRIORITY_LOW, owner_unset_idle_callback, NULL, NULL);
}


static BonoboObject *
factory_fn (BonoboGenericFactory *factory,
	    void *closure)
{
	EvolutionShellComponent *shell_component;

	shell_component = evolution_shell_component_new (folder_types,
							 create_view_fn,
							 NULL, NULL, NULL, NULL, NULL, NULL);

	gtk_signal_connect (GTK_OBJECT (shell_component), "owner_set",
			    GTK_SIGNAL_FUNC (owner_set_callback), NULL);
	gtk_signal_connect (GTK_OBJECT (shell_component), "owner_unset",
			    GTK_SIGNAL_FUNC (owner_unset_callback), NULL);

	return BONOBO_OBJECT (shell_component);
}

static void
component_factory_init (void)
{
	BonoboGenericFactory *factory;

	factory = bonobo_generic_factory_new (COMPONENT_FACTORY_ID, factory_fn, NULL);

	if (factory == NULL)
		g_error ("Cannot initialize test component.");
}


int
main (int argc, char **argv)
{
	CORBA_ORB orb;

	bindtextdomain (PACKAGE, EVOLUTION_LOCALEDIR);
	textdomain (PACKAGE);

	gnome_init_with_popt_table ("evolution-test-component", VERSION,
				    argc, argv, oaf_popt_options, 0, NULL);

	orb = oaf_init (argc, argv);

	if (bonobo_init (orb, CORBA_OBJECT_NIL, CORBA_OBJECT_NIL) == FALSE)
		g_error ("Cannot initialize the test component.");

	component_factory_init ();

	bonobo_main ();

	return 0;
}
