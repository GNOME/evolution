/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* evolution-test-component.c
 *
 * Copyright (C) 2001  Ximian, Inc.
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

/* Simple test component for the Evolution shell.  */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "evolution-shell-component.h"
#include "evolution-activity-client.h"
#include "evolution-config-control.h"

#include <bonobo/bonobo-generic-factory.h>
#include <bonobo/bonobo-main.h>

#include <gdk-pixbuf/gdk-pixbuf.h>


#define COMPONENT_ID "OAFIID:GNOME_Evolution_TestComponent_ShellComponent"

#define CONFIGURATION_CONTROL_FACTORY_ID "OAFIID:GNOME_Evolution_TestComponent_ConfigurationControlFactory"
#define CONFIGURATION_CONTROL_ID         "OAFIID:GNOME_Evolution_TestComponent_ConfigurationControl"

static const EvolutionShellComponentFolderType folder_types[] = {
	{ "test", "/usr/share/pixmaps/gnome-money.png", N_("Test"), N_("Test type"), FALSE, NULL, NULL },
	{ NULL }
};


static BonoboGenericFactory *configuration_control_factory = NULL;

static EvolutionShellClient *parent_shell = NULL;

static int timeout_id = 0;


/* Test the configuration control.  */

static BonoboObject *
create_configuration_page (void)
{
	GtkWidget *label;

	label = gtk_label_new ("This is the configuration page for the test component.");
	gtk_widget_show (label);

	return BONOBO_OBJECT (evolution_config_control_new (label));
}

static BonoboObject *
configuration_control_factory_fn (BonoboGenericFactory *factory,
				  const char *id,
				  void *closure)
{
	if (strcmp (id, CONFIGURATION_CONTROL_ID) == 0) {
		return create_configuration_page ();
	} else {
		g_warning ("Unknown ID in configuration control factory -- %s", id);
		return NULL;
	}
}

static void
register_configuration_control_factory (void)
{
	configuration_control_factory = bonobo_generic_factory_new_multi (CONFIGURATION_CONTROL_FACTORY_ID,
									  configuration_control_factory_fn,
									  NULL);

	if (configuration_control_factory == NULL)
		g_warning ("Cannot register configuration control factory!");
}


/* Test the ::Shortcut interface.  */

static void
spit_out_shortcuts (EvolutionShellClient *shell_client)
{
	GNOME_Evolution_Shortcuts shortcuts_interface;
	GNOME_Evolution_Shortcuts_GroupList *groups;
	CORBA_Environment ev;
	int i, j;

	CORBA_exception_init (&ev);

	shortcuts_interface = evolution_shell_client_get_shortcuts_interface (shell_client);
	if (CORBA_Object_is_nil (shortcuts_interface, &ev)) {
		g_warning ("No ::Shortcut interface on the shell");
		CORBA_exception_free (&ev);
		return;
	}

	groups = GNOME_Evolution_Shortcuts__get_groups (shortcuts_interface, &ev);
	if (ev._major != CORBA_NO_EXCEPTION) {
		g_warning ("Exception getting the groups: %s", ev._repo_id);
		CORBA_exception_free (&ev);
		return;
	}

	for (i = 0; i < groups->_length; i ++) {
		GNOME_Evolution_Shortcuts_Group *group;
		GNOME_Evolution_Shortcuts_ShortcutList *shortcuts;

		group = groups->_buffer + i;
		shortcuts = &group->shortcuts;

		g_print ("\tGROUP: %s\n", group->name);

		for (j = 0; j < shortcuts->_length; j ++) {
			GNOME_Evolution_Shortcuts_Shortcut *shortcut;

			shortcut = shortcuts->_buffer + j;

			g_print ("\t\tName: %s\n", shortcut->name);
			g_print ("\t\t\tType: %s\n", shortcut->type);
			g_print ("\t\t\tURI: %s\n", shortcut->uri);
		}
	}

	g_print ("** Done\n\n");

	CORBA_exception_free (&ev);
}


/* Callbacks.  */

static void
activity_client_cancel_callback (EvolutionActivityClient *client,
				 void *data)
{
	g_print ("User requested that the operation be cancelled.\n");
}

static void
activity_client_show_details_callback (EvolutionActivityClient *client,
				       void *data)
{
	g_print ("User wants to see details.\n");
}


/* Timeout #3: We are done.  */
static int
timeout_callback_3 (void *data)
{
	EvolutionActivityClient *activity_client;

	activity_client = EVOLUTION_ACTIVITY_CLIENT (data);

	gtk_object_unref (GTK_OBJECT (activity_client));

	g_print ("--> Done.\n");

	return FALSE;
}

/* Timeout #2: Update the progress until it reaches 100%.  */
static int
timeout_callback_2 (void *data)
{
	EvolutionActivityClient *activity_client;
	int progress;

	activity_client = EVOLUTION_ACTIVITY_CLIENT (data);
	progress = GPOINTER_TO_INT (gtk_object_get_data (GTK_OBJECT (activity_client), "my_progress"));

	if (progress < 0)
		progress = 0;

	g_print ("--> Updating %d\n", progress);

	if (! evolution_activity_client_update (activity_client, "Operation Foo in progress",
						(float) progress / 100.0)) {
		g_warning ("Error when updating operation");
		return FALSE;
	}

	progress ++;
	gtk_object_set_data (GTK_OBJECT (activity_client), "my_progress", GINT_TO_POINTER (progress));

	if (progress > 100) {
		gtk_timeout_add (200, timeout_callback_3, activity_client);
		return FALSE;
	}

	return TRUE;
}

/* Timeout #1: Set busy.  */
static int
timeout_callback_1 (void *data)
{
	EvolutionActivityClient *activity_client;
	gboolean suggest_display;
	GdkPixbuf *animated_icon[2];
	static int count = 0;

#define NUM_ACTIVITIES 10

	animated_icon[0] = gdk_pixbuf_new_from_file (gnome_pixmap_file ("gnome-money.png"));
	animated_icon[1] = NULL;

	g_assert (animated_icon[0] != NULL);

	activity_client = evolution_activity_client_new (parent_shell, COMPONENT_ID,
							 animated_icon,
							 "Operation Foo started!",
							 TRUE,
							 &suggest_display);
	if (activity_client == CORBA_OBJECT_NIL) {
		g_warning ("Cannot create EvolutionActivityClient object");
		return FALSE;
	}

	gtk_object_set_data (GTK_OBJECT (activity_client), "my_progress", GINT_TO_POINTER (-1));

	gtk_signal_connect (GTK_OBJECT (activity_client), "cancel",
			    GTK_SIGNAL_FUNC (activity_client_cancel_callback), NULL);
	gtk_signal_connect (GTK_OBJECT (activity_client), "show_details",
			    GTK_SIGNAL_FUNC (activity_client_show_details_callback), NULL);

	g_print ("Component becoming busy -- %s\n", COMPONENT_ID);
	if (suggest_display)
		g_print (" --> Could display dialog box.\n");

	gtk_timeout_add (100, timeout_callback_2, activity_client);

	if (count < NUM_ACTIVITIES) {
		count ++;
		gtk_timeout_add ((rand () % 5 + 1) * 500, timeout_callback_1, NULL);
	}

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

	spit_out_shortcuts (shell_client);
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

static void
user_create_new_item_callback (EvolutionShellComponent *shell_component,
			       const char *id,
			       const char *parent_folder_physical_uri,
			       const char *parent_folder_type)
{
	g_print ("\n*** Should create -- %s\n", id);
	g_print ("\n\tType %s, URI %s\n", parent_folder_type, parent_folder_physical_uri);
}


static void
register_component (void)
{
	EvolutionShellComponent *shell_component;
	int result;

	shell_component = evolution_shell_component_new (folder_types,
							 NULL,
							 create_view_fn,
							 NULL, NULL, NULL, NULL, NULL, NULL);

	gtk_signal_connect (GTK_OBJECT (shell_component), "owner_set",
			    GTK_SIGNAL_FUNC (owner_set_callback), NULL);
	gtk_signal_connect (GTK_OBJECT (shell_component), "owner_unset",
			    GTK_SIGNAL_FUNC (owner_unset_callback), NULL);

	evolution_shell_component_add_user_creatable_item (shell_component, "Stuff",
							   "New Stuff", "New _Stuff", '\0', NULL);
	evolution_shell_component_add_user_creatable_item (shell_component, "MoreStuff",
							   "New More Stuff", "New _More Stuff", 'n', NULL);

	gtk_signal_connect (GTK_OBJECT (shell_component), "user_create_new_item",
			    GTK_SIGNAL_FUNC (user_create_new_item_callback), NULL);

	result = oaf_active_server_register (COMPONENT_ID,
					     bonobo_object_corba_objref (BONOBO_OBJECT (shell_component)));

	if (result == OAF_REG_ERROR)
		g_error ("Cannot register active server into OAF");
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

	register_configuration_control_factory ();

	register_component ();

	bonobo_main ();

	return 0;
}
