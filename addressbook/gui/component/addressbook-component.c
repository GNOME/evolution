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

#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>

#include <bonobo/bonobo-generic-factory.h>

#include "evolution-shell-component.h"
#include "evolution-shell-component-dnd.h"
#include "evolution-storage.h"

#include "ebook/e-book.h"
#include "ebook/e-card.h"

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

static void
remove_folder (EvolutionShellComponent *shell_component,
	       const char *physical_uri,
	       const GNOME_Evolution_ShellComponentListener listener,
	       void *closure)
{
	CORBA_Environment ev;
	char *addressbook_db_path, *subdir_path;
	struct stat sb;
	int rv;

	g_print ("should remove %s\n", physical_uri);

	CORBA_exception_init(&ev);

	if (!strncmp (physical_uri, "ldap://", 7)) {
		GNOME_Evolution_ShellComponentListener_notifyResult (listener,
								     GNOME_Evolution_ShellComponentListener_UNSUPPORTED_OPERATION,
								     &ev);
		CORBA_exception_free(&ev);
		return;
	}
	if (strncmp (physical_uri, "file://", 7)) {
		GNOME_Evolution_ShellComponentListener_notifyResult (listener,
								     GNOME_Evolution_ShellComponentListener_INVALID_URI,
								     &ev);
		CORBA_exception_free(&ev);
		return;
	}

	subdir_path = g_concat_dir_and_file (physical_uri + 7, "subfolders");
	rv = stat (subdir_path, &sb);
	g_free (subdir_path);
	if (rv != -1) {
		GNOME_Evolution_ShellComponentListener_notifyResult (listener,
								     GNOME_Evolution_ShellComponentListener_HAS_SUBFOLDERS,
								     &ev);
		CORBA_exception_free(&ev);
		return;
	}

	addressbook_db_path = g_concat_dir_and_file (physical_uri + 7, "addressbook.db");
	rv = unlink (addressbook_db_path);
	g_free (addressbook_db_path);
	if (rv == 0) {
		GNOME_Evolution_ShellComponentListener_notifyResult (listener,
								     GNOME_Evolution_ShellComponentListener_OK,
								     &ev);
	}
	else {
		if (errno == EACCES || errno == EPERM)
			GNOME_Evolution_ShellComponentListener_notifyResult (listener,
							     GNOME_Evolution_ShellComponentListener_PERMISSION_DENIED,
							     &ev);
		else
			GNOME_Evolution_ShellComponentListener_notifyResult (listener,
							     GNOME_Evolution_ShellComponentListener_INVALID_URI, /*XXX*/
							     &ev);
	}
	CORBA_exception_free(&ev);
}

static void
xfer_folder (EvolutionShellComponent *shell_component,
	     const char *source_physical_uri,
	     const char *destination_physical_uri,
	     gboolean remove_source,
	     const GNOME_Evolution_ShellComponentListener listener,
	     void *closure)
{
	CORBA_Environment ev;
	char *source_path;
	char *destination_path;
	
	g_print ("should transfer %s to %s, %s source\n", source_physical_uri,
		 destination_physical_uri, remove_source ? "removing" : "not removing");

	if (!strncmp (source_physical_uri, "ldap://", 7)
	    || !strncmp (destination_physical_uri, "ldap://", 7)) {
		GNOME_Evolution_ShellComponentListener_notifyResult (listener,
							     GNOME_Evolution_ShellComponentListener_UNSUPPORTED_OPERATION,
							     &ev);
		CORBA_exception_free(&ev);
		return;
	}
	if (strncmp (source_physical_uri, "file://", 7)
	    || strncmp (destination_physical_uri, "file://", 7)) {
		GNOME_Evolution_ShellComponentListener_notifyResult (listener,
							     GNOME_Evolution_ShellComponentListener_INVALID_URI,
							     &ev);
		CORBA_exception_free(&ev);
		return;
	}

	/* strip the 'file://' from the beginning of each uri and add addressbook.db */
	source_path = g_concat_dir_and_file (source_physical_uri + 7, "addressbook.db");
	destination_path = g_concat_dir_and_file (destination_physical_uri + 7, "addressbook.db");

	if (remove_source) {
		g_print ("rename %s %s\n", source_path, destination_path);
	}
	else {
		g_print ("copy %s %s\n", source_path, destination_path);
	}

	CORBA_exception_init (&ev);

	/* XXX always fail for now, until the above stuff is written */
	GNOME_Evolution_ShellComponentListener_notifyResult (listener, GNOME_Evolution_ShellComponentListener_PERMISSION_DENIED, &ev);

	g_free (source_path);
	g_free (destination_path);
	CORBA_exception_free (&ev);
}

static void
populate_context_menu (EvolutionShellComponent *shell_component,
		       BonoboUIComponent *uic,
		       const char *physical_uri,
		       const char *type,
		       void *closure)
{
	static char popup_xml[] = 
		"<menuitem name=\"BorkBorkBork\" verb=\"ActivateView\" _label=\"_Foooo\" _tip=\"FooFooFoo\"/>\n";
	bonobo_ui_component_set_translate (uic, EVOLUTION_SHELL_COMPONENT_POPUP_PLACEHOLDER,
					   popup_xml, NULL);
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
	g_print ("should get dnd selection for %s\n", physical_uri);
	return NULL;
}

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


/* Destination side DnD */

static CORBA_boolean
destination_folder_handle_motion (EvolutionShellComponentDndDestinationFolder *folder,
				  const char *physical_uri,
				  const GNOME_Evolution_ShellComponentDnd_DestinationFolder_Context * destination_context,
				  GNOME_Evolution_ShellComponentDnd_Action * suggested_action_return,
				  gpointer user_data)
{
	g_print ("in destination_folder_handle_motion (%s)\n", physical_uri);
	*suggested_action_return = GNOME_Evolution_ShellComponentDnd_ACTION_MOVE;
	return TRUE;
}

static void
dnd_drop_book_open_cb (EBook *book, EBookStatus status, GList *card_list)
{
	GList *l;

	for (l = card_list; l; l = l->next) {
		ECard *card = l->data;

		e_book_add_card (book, card, NULL /* XXX */, NULL);
	}
}

static CORBA_boolean
destination_folder_handle_drop (EvolutionShellComponentDndDestinationFolder *folder,
				const char *physical_uri,
				const GNOME_Evolution_ShellComponentDnd_DestinationFolder_Context * destination_context,
				const GNOME_Evolution_ShellComponentDnd_Action action,
				const GNOME_Evolution_ShellComponentDnd_Data * data,
				gpointer user_data)
{
	EBook *book;
	GList *card_list;
	char *expanded_uri;

	if (action == GNOME_Evolution_ShellComponentDnd_ACTION_LINK)
		return FALSE; /* we can't create links in our addressbook format */

	g_print ("in destination_folder_handle_drop (%s)\n", physical_uri);

	card_list = e_card_load_cards_from_string (data->bytes._buffer);

	expanded_uri = addressbook_expand_uri (physical_uri);

	book = e_book_new ();
	e_book_load_uri (book, expanded_uri,
			 (EBookCallback)dnd_drop_book_open_cb, card_list);

	g_free (expanded_uri);

	return TRUE;
}


/* The factory function.  */

static BonoboObject *
factory_fn (BonoboGenericFactory *factory,
	    void *closure)
{
	EvolutionShellComponent *shell_component;
	EvolutionShellComponentDndDestinationFolder *destination_interface;

	shell_component = evolution_shell_component_new (folder_types, create_view, create_folder,
							 remove_folder, xfer_folder,
							 populate_context_menu,
							 get_dnd_selection,
							 NULL);

	destination_interface = evolution_shell_component_dnd_destination_folder_new (destination_folder_handle_motion,
										      destination_folder_handle_drop,
										      shell_component);

	bonobo_object_add_interface (BONOBO_OBJECT (shell_component),
				     BONOBO_OBJECT (destination_interface));

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

