/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* evolution-test-component.c
 *
 * Copyright (C) 2001, 2002  Ximian, Inc.
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
#include "evolution-storage.h"

#include <bonobo-activation/bonobo-activation.h>

#include <bonobo/bonobo-exception.h>
#include <bonobo/bonobo-generic-factory.h>
#include <bonobo/bonobo-main.h>
#include <bonobo/bonobo-widget.h>

#include <libgnomeui/gnome-ui-init.h>

#include <gtk/gtkdialog.h>
#include <gtk/gtkeventbox.h>
#include <gtk/gtklabel.h>
#include <gtk/gtkmain.h>
#include <gtk/gtkstock.h>
#include <gtk/gtkvbox.h>

#include <gdk-pixbuf/gdk-pixbuf.h>

#include <stdlib.h>
#include <string.h>


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


/* TEST #1: Configuration Control.  */

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
	configuration_control_factory = bonobo_generic_factory_new (CONFIGURATION_CONTROL_FACTORY_ID,
								    configuration_control_factory_fn,
								    NULL);

	if (configuration_control_factory == NULL)
		g_warning ("Cannot register configuration control factory!");
}


/* TEST #2: The ::Shortcut interface.  */

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
		g_warning ("Exception getting the groups: %s", BONOBO_EX_REPOID (&ev));
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


/* TEST #4: The multiple folder selector.  */

static void
dialog_clicked_callback (GtkDialog *dialog,
			 int button_num,
			 void *data)
{
	GNOME_Evolution_StorageSetView storage_set_view_iface;
	CORBA_Environment ev;
	GNOME_Evolution_FolderList *folder_list;

	if (button_num == 1) {
		/* Close.  */
		gtk_widget_destroy (GTK_WIDGET (dialog));
		return;
	}

	CORBA_exception_init (&ev);

	storage_set_view_iface = (GNOME_Evolution_StorageSetView) data;

	folder_list = GNOME_Evolution_StorageSetView__get_checkedFolders (storage_set_view_iface, &ev);
	if (BONOBO_EX (&ev)) {
		g_warning ("Cannot get checkedFolders -- %s", BONOBO_EX_REPOID (&ev));
	} else {
		int i;

		for (i = 0; i < folder_list->_length; i ++) {
#define PRINT(s) g_print ("\t" #s ": %s\n", folder_list->_buffer[i].s);
			g_print ("Folder #%d:\n", i + 1);
			PRINT (type);
			PRINT (description);
			PRINT (displayName);
			PRINT (physicalUri);
			PRINT (evolutionUri);
#undef PRINT

			g_print ("\tunreadCount: %d\n", folder_list->_buffer[i].unreadCount);
		}
	}

	CORBA_exception_free (&ev);
}

static void
dialog_weak_notify (void *data,
		    GObject *where_the_object_was)
{
	GNOME_Evolution_StorageSetView storage_set_view_iface;
	CORBA_Environment ev;

	CORBA_exception_init (&ev);

	storage_set_view_iface = (GNOME_Evolution_StorageSetView) data;
	Bonobo_Unknown_unref (storage_set_view_iface, &ev);

	CORBA_exception_free (&ev);
}

static void
create_new_folder_selector (EvolutionShellComponent *shell_component)
{
	EvolutionShellClient *shell_client;
	GNOME_Evolution_Shell corba_shell;
	GNOME_Evolution_StorageSetView storage_set_view_iface;
	GtkWidget *dialog;
	GtkWidget *control_widget;
	CORBA_Environment ev;

	CORBA_exception_init (&ev);

	shell_client = evolution_shell_component_get_owner (shell_component);
	g_assert (shell_client != NULL);
	corba_shell = evolution_shell_client_corba_objref (shell_client);

	control_widget = evolution_shell_client_create_storage_set_view (shell_client,
									 CORBA_OBJECT_NIL,
									 NULL,
									 &storage_set_view_iface,
									 &ev);
	if (control_widget == NULL) {
		g_warning ("Can't create the StorageSetView control -- %s", BONOBO_EX_REPOID (&ev));
		CORBA_exception_free (&ev);
		return;
	}

	dialog = gtk_dialog_new_with_buttons ("Test the Selector here.", NULL,
					      GTK_DIALOG_MODAL,
					      GTK_STOCK_APPLY, GTK_RESPONSE_ACCEPT,
					      GTK_STOCK_CLOSE, GTK_RESPONSE_CLOSE,
					      NULL);
	gtk_window_set_default_size (GTK_WINDOW (dialog), 200, 400);

	gtk_container_add (GTK_CONTAINER (GTK_DIALOG (dialog)->vbox), control_widget);

	GNOME_Evolution_StorageSetView__set_showCheckboxes (storage_set_view_iface, TRUE, &ev);
	if (BONOBO_EX (&ev))
		g_warning ("Cannot show checkboxes -- %s", BONOBO_EX_REPOID (&ev));

	g_signal_connect (dialog, "clicked",
			  G_CALLBACK (dialog_clicked_callback), storage_set_view_iface);

	/* This is necessary to unref the StorageSetView iface once we are done
	   with it.  */
	g_object_weak_ref (G_OBJECT (dialog), dialog_weak_notify, storage_set_view_iface);

	gtk_widget_show (control_widget);
	gtk_widget_show (dialog);

	CORBA_exception_free (&ev);
}


/* TEST #5: Test custom storage.  */

static int
shared_folder_discovery_timeout_callback (void *data)
{
	GNOME_Evolution_Storage_FolderResult result;
	CORBA_Environment ev;
	Bonobo_Listener listener;
	CORBA_any any;
	EvolutionStorage *storage;

	storage = EVOLUTION_STORAGE (data);

	listener = (Bonobo_Listener) g_object_get_data (G_OBJECT (storage), "listener");

	result.result = GNOME_Evolution_Storage_OK;
	result.path = "/Shared Folders/The Public Folder";

	any._type = TC_GNOME_Evolution_Storage_FolderResult;
	any._value = &result;

	CORBA_exception_init (&ev);

	Bonobo_Listener_event (listener, "result", &any, &ev);
	if (BONOBO_EX (&ev))
		g_warning ("Cannot report result for shared folder discovery -- %s",
			   BONOBO_EX_REPOID (&ev));

	Bonobo_Unknown_unref (listener, &ev);
	CORBA_Object_release (listener, &ev);

	CORBA_exception_free (&ev);

	g_object_set_data (G_OBJECT (storage), "listener", NULL);
	g_object_set_data (G_OBJECT (storage), "timeout_id", NULL);

	return FALSE;
}

static void
storage_discover_shared_folder_callback (EvolutionStorage *storage,
					 Bonobo_Listener listener,
					 const char *user,
					 const char *folder_name,
					 void *data)
{
	CORBA_Environment ev;
	Bonobo_Listener listener_copy;

	CORBA_exception_init (&ev);
	listener_copy = CORBA_Object_duplicate (listener, &ev);
	Bonobo_Unknown_ref (listener, &ev);
	CORBA_exception_free (&ev);

	g_print ("Listener copy %p\n", listener_copy);

	timeout_id = g_timeout_add (1000, shared_folder_discovery_timeout_callback, storage);

	g_object_set_data (G_OBJECT (storage), "listener", listener_copy);
	g_object_set_data (G_OBJECT (storage), "timeout_id", GINT_TO_POINTER (timeout_id));
}

static void
storage_cancel_discover_shared_folder_callback (EvolutionStorage *storage,
						const char *user,
						const char *folder_name,
						void *data)
{
	Bonobo_Listener listener;
	CORBA_Environment ev;
	int timeout_id;

	timeout_id = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (storage), "timeout_id"));
	if (timeout_id == 0)
		return;

	g_source_remove (timeout_id);
	g_object_set_data (G_OBJECT (storage), "timeout_id", NULL);

	listener = (Bonobo_Listener) g_object_get_data (G_OBJECT (storage), "listener");

	CORBA_exception_init (&ev);
	Bonobo_Unknown_unref (listener, &ev);
	CORBA_Object_release (listener, &ev);
	CORBA_exception_free (&ev);

	g_object_set_data (G_OBJECT (storage), "listener", NULL);
}

static void
storage_show_folder_properties_callback (EvolutionStorage *storage,
					 const char *path,
					 unsigned int itemNumber,
					 unsigned long parentWindowId,
					 void *data)
{
	g_print ("Show properties #%d -- %s\n", itemNumber, path);
}

static void
setup_custom_storage (EvolutionShellClient *shell_client)
{
	EvolutionStorage *the_storage;
	EvolutionStorageResult result;

	the_storage = evolution_storage_new ("TestStorage", TRUE);

	g_signal_connect (the_storage, "discover_shared_folder",
			  G_CALLBACK (storage_discover_shared_folder_callback), shell_client);
	g_signal_connect (the_storage, "cancel_discover_shared_folder",
			  G_CALLBACK (storage_cancel_discover_shared_folder_callback), shell_client);

	/* Add some custom "Properties" items.  */
	evolution_storage_add_property_item (the_storage, "Sharing...",
					     "Change sharing properties for this folder", NULL); 
	evolution_storage_add_property_item (the_storage, "Permissions...",
					     "Change permissions for this folder", NULL);

	g_signal_connect (the_storage, "show_folder_properties",
			  G_CALLBACK (storage_show_folder_properties_callback), NULL);

	result = evolution_storage_register_on_shell (the_storage, evolution_shell_client_corba_objref (shell_client));
	if (result != EVOLUTION_STORAGE_OK) {
		g_warning ("Cannot register storage on the shell.");
		bonobo_object_unref (BONOBO_OBJECT (the_storage));
		return;
	}

	/* Test the sorting_priority arg here: if it was just sorting in
	   alphabetical order, FirstFolder would come before SecondFolder, but
	   we are specifying -1 sorting priority for SecondFolder and zero for
	   FirstFolder so the order is reversed.  */

	evolution_storage_new_folder (the_storage, "/FirstFolder", "FirstFolder",
				      "test", "file:///tmp/blah", "", "inbox", 0, TRUE, 0);
	evolution_storage_new_folder (the_storage, "/SecondFolder", "SecondFolder",
				      "calendar", "file:///tmp/bleh", "", NULL, 0, FALSE, -1);
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

	g_object_unref (activity_client);

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
	progress = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (activity_client), "my_progress"));

	if (progress < 0)
		progress = 0;

	g_print ("--> Updating %d\n", progress);

	if (! evolution_activity_client_update (activity_client, "Operation Foo in progress",
						(float) progress / 100.0)) {
		g_warning ("Error when updating operation");
		return FALSE;
	}

	progress ++;
	g_object_set_data (G_OBJECT (activity_client), "my_progress", GINT_TO_POINTER (progress));

	if (progress > 100) {
		g_timeout_add (200, timeout_callback_3, activity_client);
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

	animated_icon[0] = gdk_pixbuf_new_from_file (EVOLUTION_IMAGES "outbox-mini.png", NULL);
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

	g_object_set_data (G_OBJECT (activity_client), "my_progress", GINT_TO_POINTER (-1));

	g_signal_connect (activity_client, "cancel",
			  G_CALLBACK (activity_client_cancel_callback), NULL);
	g_signal_connect (activity_client, "show_details",
			  G_CALLBACK (activity_client_show_details_callback), NULL);

	g_print ("Component becoming busy -- %s\n", COMPONENT_ID);
	if (suggest_display)
		g_print (" --> Could display dialog box.\n");

	g_timeout_add (100, timeout_callback_2, activity_client);

	if (count < NUM_ACTIVITIES) {
		count ++;
		g_timeout_add ((rand () % 5 + 1) * 500, timeout_callback_1, NULL);
	}

	return FALSE;
}


static EvolutionShellComponentResult
create_view_fn (EvolutionShellComponent *shell_component,
		const char *physical_uri,
		const char *folder_type,
		const char *view_data,
		BonoboControl **control_return,
		void *closure)
{
	GtkWidget *vbox;
	GtkWidget *label_1, *label_2, *label_3, *label_4;
	GtkWidget *event_box_1, *event_box_2;

	label_1 = gtk_label_new ("This is just a test component, displaying the following URI:");
	label_2 = gtk_label_new (physical_uri);

	if (*view_data) {
		label_3 = gtk_label_new ("And the following view_data:");
		label_4 = gtk_label_new (view_data);
	} else
		label_3 = label_4 = NULL;

	event_box_1 = gtk_event_box_new ();
	event_box_2 = gtk_event_box_new ();

	vbox = gtk_vbox_new (FALSE, 5);

	gtk_box_pack_start (GTK_BOX (vbox), event_box_1, TRUE, TRUE, 0);
	gtk_box_pack_start (GTK_BOX (vbox), label_1, FALSE, TRUE, 0);
	gtk_box_pack_start (GTK_BOX (vbox), label_2, FALSE, TRUE, 0);
	if (label_3) {
		gtk_box_pack_start (GTK_BOX (vbox), label_3, FALSE, TRUE, 0);
		gtk_box_pack_start (GTK_BOX (vbox), label_4, FALSE, TRUE, 0);
	}
	gtk_box_pack_start (GTK_BOX (vbox), event_box_2, TRUE, TRUE, 0);

	gtk_widget_show_all (vbox);

	*control_return = bonobo_control_new (vbox);

	g_assert (timeout_id == 0);
	timeout_id = g_timeout_add (2000, timeout_callback_1, NULL);

	return EVOLUTION_SHELL_COMPONENT_OK;
}

static gboolean
request_quit_fn (EvolutionShellComponent *shell_component,
		 void *closure)
{
	GtkWidget *confirm_dialog;
	GtkWidget *label;
	int response;

	confirm_dialog = gtk_dialog_new_with_buttons ("Quit?", NULL, GTK_DIALOG_MODAL,
						      GTK_STOCK_OK, GTK_RESPONSE_ACCEPT,
						      GTK_STOCK_CANCEL, GTK_RESPONSE_REJECT,
						      NULL);
	label = gtk_label_new ("Please confirm that you want to quit now.");
	gtk_container_add (GTK_CONTAINER (GTK_DIALOG (confirm_dialog)->vbox), label);
	gtk_widget_show_all (confirm_dialog);

	response = gtk_dialog_run (GTK_DIALOG (confirm_dialog));
	gtk_widget_destroy (confirm_dialog);

	if (response == GTK_RESPONSE_ACCEPT)
		return TRUE;	/* OK */
	else
		return FALSE;	/* Cancel */
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

	setup_custom_storage (shell_client);
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

	if (strcmp (id, "FolderSelector") == 0)
		create_new_folder_selector (shell_component);
}


static void
register_component (void)
{
	EvolutionShellComponent *shell_component;
	int result;

	shell_component = evolution_shell_component_new (folder_types,
							 NULL,
							 create_view_fn,
							 NULL, NULL, NULL, NULL, NULL, NULL,
							 request_quit_fn,
							 NULL);

	g_signal_connect (shell_component, "owner_set",
			  G_CALLBACK (owner_set_callback), NULL);
	g_signal_connect (shell_component, "owner_unset",
			  G_CALLBACK (owner_unset_callback), NULL);

	evolution_shell_component_add_user_creatable_item (shell_component, "Stuff",
							   "New Stuff", "New _Stuff",
							   "Create some new stuff",
							   NULL,
							   '\0', NULL);
	evolution_shell_component_add_user_creatable_item (shell_component, "MoreStuff",
							   "New More Stuff", "New _More Stuff",
							   "Create more stuff",
							   NULL,
							   'n', NULL);
	evolution_shell_component_add_user_creatable_item (shell_component, "FolderSelector",
							   "Folder Selector", "New Folder _Selector",
							   "Show a folder selector",
							   NULL,
							   's', NULL);

	g_signal_connect (shell_component, "user_create_new_item",
			  G_CALLBACK (user_create_new_item_callback), NULL);

	result = bonobo_activation_active_server_register (COMPONENT_ID,
							   bonobo_object_corba_objref (BONOBO_OBJECT (shell_component)));

	if (result != Bonobo_ACTIVATION_REG_SUCCESS)
		g_error ("Cannot register active server into OAF");
}


int
main (int argc, char **argv)
{
	bindtextdomain (PACKAGE, EVOLUTION_LOCALEDIR);
	textdomain (PACKAGE);

	gnome_program_init (PACKAGE, VERSION, LIBGNOMEUI_MODULE, argc, argv, 
			    GNOME_PROGRAM_STANDARD_PROPERTIES,
			    GNOME_PARAM_HUMAN_READABLE_NAME, _("Evolution Test Component"),
			    NULL);

	register_configuration_control_factory ();

	register_component ();

	g_print ("Test Component up and running.\n");

	bonobo_main ();

	return 0;
}
