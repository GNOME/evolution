/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* evolution-shell-client.c
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

#include "e-util/e-util.h"

#include "evolution-shell-client.h"


struct _EvolutionShellClientPrivate {
	int dummy;
};

#define PARENT_TYPE bonobo_object_client_get_type ()
static BonoboObjectClientClass *parent_class = NULL;


/* Easy-to-use wrapper for Evolution::user_select_folder.  */

static PortableServer_ServantBase__epv FolderSelectionListener_base_epv;
static POA_Evolution_FolderSelectionListener__epv FolderSelectionListener_epv;
static POA_Evolution_FolderSelectionListener__vepv FolderSelectionListener_vepv;
static gboolean FolderSelectionListener_vtables_initialized = FALSE;

struct _FolderSelectionListenerServant {
	POA_Evolution_FolderSelectionListener servant;
	GMainLoop *main_loop;
	char **uri_return;
	char **physical_uri_return;
};
typedef struct _FolderSelectionListenerServant FolderSelectionListenerServant;

static void
impl_FolderSelectionListener_selected (PortableServer_Servant servant,
				       const CORBA_char *uri,
				       const CORBA_char *physical_uri,
				       CORBA_Environment *ev)
{
	FolderSelectionListenerServant *listener_servant;

	listener_servant = (FolderSelectionListenerServant *) servant;

	if (listener_servant->uri_return != NULL)
		* (listener_servant->uri_return) = g_strdup (uri);

	if (listener_servant->physical_uri_return != NULL)
		* (listener_servant->physical_uri_return) = g_strdup (physical_uri);

	g_main_quit (listener_servant->main_loop);
}

static void
impl_FolderSelectionListener_cancel (PortableServer_Servant servant,
				     CORBA_Environment *ev)
{
	FolderSelectionListenerServant *listener_servant;

	listener_servant = (FolderSelectionListenerServant *) servant;

	if (listener_servant->uri_return != NULL)
		* (listener_servant->uri_return) = NULL;

	if (listener_servant->physical_uri_return != NULL)
		* (listener_servant->physical_uri_return) = NULL;

	g_main_quit (listener_servant->main_loop);
}	

static void
init_FolderSelectionListener_vtables (void)
{
	FolderSelectionListener_base_epv._private    = NULL;
	FolderSelectionListener_base_epv.finalize    = NULL;
	FolderSelectionListener_base_epv.default_POA = NULL;

	FolderSelectionListener_epv.selected = impl_FolderSelectionListener_selected;
	FolderSelectionListener_epv.cancel = impl_FolderSelectionListener_cancel;

	FolderSelectionListener_vepv._base_epv                             = &FolderSelectionListener_base_epv;
	FolderSelectionListener_vepv.Evolution_FolderSelectionListener_epv = &FolderSelectionListener_epv;
		
	FolderSelectionListener_vtables_initialized = TRUE;
}

static Evolution_FolderSelectionListener
create_folder_selection_listener_interface (char **result,
					    GMainLoop *main_loop,
					    char **uri_return,
					    char **physical_uri_return)
{
	Evolution_FolderSelectionListener corba_interface;
	CORBA_Environment ev;
	FolderSelectionListenerServant *servant;
	PortableServer_Servant listener_servant;

	if (! FolderSelectionListener_vtables_initialized)
		init_FolderSelectionListener_vtables ();

	servant = g_new0 (FolderSelectionListenerServant, 1);
	servant->servant.vepv        = &FolderSelectionListener_vepv;
	servant->main_loop           = main_loop;
	servant->uri_return          = uri_return;
	servant->physical_uri_return = physical_uri_return;

	listener_servant = (PortableServer_Servant) servant;

	CORBA_exception_init (&ev);

	POA_Evolution_FolderSelectionListener__init (listener_servant, &ev);
	if (ev._major != CORBA_NO_EXCEPTION) {
		g_free(servant);
		return CORBA_OBJECT_NIL;
	}

	CORBA_free (PortableServer_POA_activate_object (bonobo_poa (), listener_servant, &ev));

	corba_interface = PortableServer_POA_servant_to_reference (bonobo_poa (), listener_servant, &ev);
	if (ev._major != CORBA_NO_EXCEPTION)
		corba_interface = CORBA_OBJECT_NIL;

	CORBA_exception_free (&ev);

	return corba_interface;
}

static int
count_string_items (const char *list[])
{
	int i;

	if (list == NULL)
		return 0;

	for (i = 0; list[i] != NULL; i++)
		;

	return i;
}

static void
user_select_folder (EvolutionShellClient *shell_client,
		    const char *title,
		    const char *default_folder,
		    const char *possible_types[],
		    char **uri_return,
		    char **physical_uri_return)
{
	Evolution_FolderSelectionListener listener_interface;
	Evolution_Shell corba_shell;
	GMainLoop *main_loop;
	CORBA_Environment ev;
	Evolution_Shell_FolderTypeList corba_type_list;
	int num_possible_types;
	char *result;

	result = NULL;
	main_loop = g_main_new (FALSE);

	listener_interface = create_folder_selection_listener_interface (&result, main_loop,
									 uri_return, physical_uri_return);
	if (listener_interface == CORBA_OBJECT_NIL) {
		g_main_destroy (main_loop);
		return;
	}

	CORBA_exception_init (&ev);

	corba_shell = bonobo_object_corba_objref (BONOBO_OBJECT (shell_client));

	num_possible_types = count_string_items (possible_types);

	corba_type_list._length  = num_possible_types;
	corba_type_list._maximum = num_possible_types;
	corba_type_list._buffer  = (CORBA_char **) possible_types;

	Evolution_Shell_user_select_folder (corba_shell, listener_interface,
					    title, default_folder, &corba_type_list,
					    &ev);

	if (ev._major != CORBA_NO_EXCEPTION) {
		CORBA_exception_free (&ev);

		if (uri_return != NULL)
			*uri_return = NULL;
		if (physical_uri_return != NULL)
			*physical_uri_return = NULL;

		return;
	}

	g_main_run (main_loop);

	CORBA_Object_release (listener_interface, &ev);

	CORBA_exception_free (&ev);
}


/* GtkObject methods.  */

static void
destroy (GtkObject *object)
{
	EvolutionShellClient *shell_client;
	EvolutionShellClientPrivate *priv;

	shell_client = EVOLUTION_SHELL_CLIENT (object);
	priv = shell_client->priv;

	/* Nothing to do here.  */

	(* GTK_OBJECT_CLASS (parent_class)->destroy) (object);
}


static void
class_init (EvolutionShellClientClass *klass)
{
	GtkObjectClass *object_class;

	parent_class = gtk_type_class (bonobo_object_get_type ());

	object_class = GTK_OBJECT_CLASS (klass);

	object_class->destroy = destroy;
}

static void
init (EvolutionShellClient *shell_client)
{
	EvolutionShellClientPrivate *priv;

	priv = g_new (EvolutionShellClientPrivate, 1);
	priv->dummy = 0;

	shell_client->priv = priv;
}


/**
 * evolution_shell_client_construct:
 * @shell_client: 
 * @corba_shell: 
 * 
 * Construct @shell_client associating it to @corba_shell.
 **/
void
evolution_shell_client_construct (EvolutionShellClient *shell_client,
				  Evolution_Shell corba_shell)
{
	g_return_if_fail (shell_client != NULL);
	g_return_if_fail (EVOLUTION_IS_SHELL_CLIENT (shell_client));
	g_return_if_fail (corba_shell != CORBA_OBJECT_NIL);

	bonobo_object_construct (BONOBO_OBJECT (shell_client), (CORBA_Object) corba_shell);
}

/**
 * evolution_shell_client_new:
 * @corba_shell: A pointer to the CORBA Evolution::Shell interface.
 * 
 * Create a new client object for @corba_shell.
 * 
 * Return value: A pointer to the Evolution::Shell client BonoboObject.
 **/
EvolutionShellClient *
evolution_shell_client_new (Evolution_Shell corba_shell)
{
	EvolutionShellClient *shell_client;

	shell_client = gtk_type_new (evolution_shell_client_get_type ());

	evolution_shell_client_construct (shell_client, corba_shell);

	if (bonobo_object_corba_objref (BONOBO_OBJECT (shell_client)) == CORBA_OBJECT_NIL) {
		bonobo_object_unref (BONOBO_OBJECT (shell_client));
		return NULL;
	}

	return shell_client;
}


/**
 * evolution_shell_client_user_select_folder:
 * @shell_client: A EvolutionShellClient object
 * @title: The title for the folder selection dialog
 * @default_folder: The folder initially selected on the dialog
 * @uri_return: 
 * @physical_uri_return: 
 * 
 * Pop up the shell's folder selection dialog with the specified @title and
 * @default_folder as the initially selected folder.  On return, set *@uri and
 * *@physical_uri to the evolution: URI and the physical URI of the selected
 * folder (or %NULL if the user cancelled the dialog).  (The dialog is modal.)
 **/
void
evolution_shell_client_user_select_folder (EvolutionShellClient *shell_client,
					   const char *title,
					   const char *default_folder,
					   const char *possible_types[],
					   char **uri_return,
					   char **physical_uri_return)
{
	g_return_if_fail (shell_client != NULL);
	g_return_if_fail (EVOLUTION_IS_SHELL_CLIENT (shell_client));
	g_return_if_fail (title != NULL);
	g_return_if_fail (default_folder != NULL);

	user_select_folder (shell_client, title, default_folder, possible_types,
			    uri_return, physical_uri_return);
}


E_MAKE_TYPE (evolution_shell_client, "EvolutionShellClient", EvolutionShellClient, class_init, init, PARENT_TYPE)
