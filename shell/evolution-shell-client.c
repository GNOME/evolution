/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* evolution-shell-client.c
 *
 * Copyright (C) 2000  Ximian, Inc.
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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <gdk/gdkx.h>
#include <gtk/gtkmain.h>

#include <bonobo/bonobo-exception.h>
#include <bonobo/bonobo-main.h>
#include <bonobo/bonobo-object.h>
#include <bonobo/bonobo-widget.h>

#include <gal/util/e-util.h>

#include "evolution-shell-client.h"
#include "e-shell-corba-icon-utils.h"


struct _EvolutionShellClientPrivate {
	GNOME_Evolution_Activity activity_interface;
	GNOME_Evolution_Shortcuts shortcuts_interface;
	GNOME_Evolution_StorageRegistry storage_registry_interface;
	GHashTable *icons;
};

#define PARENT_TYPE bonobo_object_client_get_type ()
static BonoboObjectClientClass *parent_class = NULL;


/* Easy-to-use wrapper for Evolution::user_select_folder.  */

static PortableServer_ServantBase__epv FolderSelectionListener_base_epv;
static POA_GNOME_Evolution_FolderSelectionListener__epv FolderSelectionListener_epv;
static POA_GNOME_Evolution_FolderSelectionListener__vepv FolderSelectionListener_vepv;
static gboolean FolderSelectionListener_vtables_initialized = FALSE;

struct _FolderSelectionListenerServant {
	POA_GNOME_Evolution_FolderSelectionListener servant;
	GNOME_Evolution_Folder **folder_return;
};
typedef struct _FolderSelectionListenerServant FolderSelectionListenerServant;


/* Helper functions.  */

static CORBA_Object
query_shell_interface (EvolutionShellClient *shell_client,
		       const char *interface_name)
{
	CORBA_Environment ev;
	CORBA_Object interface_object;
	EvolutionShellClientPrivate *priv;

	priv = shell_client->priv;

	CORBA_exception_init (&ev);

 	interface_object = Bonobo_Unknown_queryInterface (bonobo_object_corba_objref (BONOBO_OBJECT (shell_client)),
							  interface_name, &ev);

	if (ev._major != CORBA_NO_EXCEPTION) {
		g_warning ("EvolutionShellClient: Error querying interface %s on %p -- %s",
			   interface_name, shell_client, ev._repo_id);
		interface_object = CORBA_OBJECT_NIL;
	} else if (CORBA_Object_is_nil (interface_object, &ev)) {
		g_warning ("No interface %s for ShellClient %p", interface_name, shell_client);
	}

	CORBA_exception_free (&ev);

	return interface_object;
}


static void
impl_FolderSelectionListener_selected (PortableServer_Servant servant,
				       const GNOME_Evolution_Folder *folder,
				       CORBA_Environment *ev)
{
	FolderSelectionListenerServant *listener_servant;

	listener_servant = (FolderSelectionListenerServant *) servant;

	if (listener_servant->folder_return != NULL) {
		GNOME_Evolution_Folder *ret_folder =
			GNOME_Evolution_Folder__alloc ();
		ret_folder->type = CORBA_string_dup (folder->type);
		ret_folder->description = CORBA_string_dup (folder->description);
		ret_folder->displayName = CORBA_string_dup (folder->displayName);
		ret_folder->physicalUri = CORBA_string_dup (folder->physicalUri);
		ret_folder->evolutionUri = CORBA_string_dup (folder->evolutionUri);
		ret_folder->unreadCount = folder->unreadCount;
		* (listener_servant->folder_return) = ret_folder;
	}

	gtk_main_quit ();
}

static void
impl_FolderSelectionListener_cancel (PortableServer_Servant servant,
				     CORBA_Environment *ev)
{
	FolderSelectionListenerServant *listener_servant;

	listener_servant = (FolderSelectionListenerServant *) servant;

	if (listener_servant->folder_return != NULL)
		* (listener_servant->folder_return) = NULL;

	gtk_main_quit ();
}	

static void
init_FolderSelectionListener_vtables (void)
{
	FolderSelectionListener_base_epv._private    = NULL;
	FolderSelectionListener_base_epv.finalize    = NULL;
	FolderSelectionListener_base_epv.default_POA = NULL;

	FolderSelectionListener_epv.notifySelected = impl_FolderSelectionListener_selected;
	FolderSelectionListener_epv.notifyCanceled = impl_FolderSelectionListener_cancel;

	FolderSelectionListener_vepv._base_epv                             = &FolderSelectionListener_base_epv;
	FolderSelectionListener_vepv.GNOME_Evolution_FolderSelectionListener_epv = &FolderSelectionListener_epv;
		
	FolderSelectionListener_vtables_initialized = TRUE;
}

static GNOME_Evolution_FolderSelectionListener
create_folder_selection_listener_interface (char **result,
					    GNOME_Evolution_Folder **folder_return)
{
	GNOME_Evolution_FolderSelectionListener corba_interface;
	CORBA_Environment ev;
	FolderSelectionListenerServant *servant;
	PortableServer_Servant listener_servant;

	if (! FolderSelectionListener_vtables_initialized)
		init_FolderSelectionListener_vtables ();

	servant = g_new0 (FolderSelectionListenerServant, 1);
	servant->servant.vepv         = &FolderSelectionListener_vepv;
	servant->folder_return        = folder_return;

	listener_servant = (PortableServer_Servant) servant;

	CORBA_exception_init (&ev);

	POA_GNOME_Evolution_FolderSelectionListener__init (listener_servant, &ev);
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
count_string_items (const char **list)
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
		    GtkWindow *parent,
		    const char *title,
		    const char *default_folder,
		    const char **possible_types,
		    GNOME_Evolution_Folder **folder_return)
{
	GNOME_Evolution_FolderSelectionListener listener_interface;
	GNOME_Evolution_Shell corba_shell;
	CORBA_Environment ev;
	GNOME_Evolution_Shell_FolderTypeNameList corba_type_name_list;
	CORBA_long_long parent_xid;
	int num_possible_types;
	char *result;

	result = NULL;

	listener_interface = create_folder_selection_listener_interface (&result,
									 folder_return);
	if (listener_interface == CORBA_OBJECT_NIL)
		return;

	CORBA_exception_init (&ev);

	corba_shell = bonobo_object_corba_objref (BONOBO_OBJECT (shell_client));

	num_possible_types = count_string_items (possible_types);

	corba_type_name_list._length  = num_possible_types;
	corba_type_name_list._maximum = num_possible_types;
	corba_type_name_list._buffer  = (CORBA_char **) possible_types;

	parent_xid = (CORBA_long_long) GDK_WINDOW_XWINDOW (GTK_WIDGET (parent)->window);

	GNOME_Evolution_Shell_selectUserFolder (corba_shell, parent_xid, listener_interface,
						title, default_folder, &corba_type_name_list,
						"", &ev);

	if (ev._major != CORBA_NO_EXCEPTION) {
		CORBA_exception_free (&ev);
		return;
	}

	gtk_main();

	CORBA_Object_release (listener_interface, &ev);

	CORBA_exception_free (&ev);
}


/* GtkObject methods.  */

static void
unref_pixbuf (gpointer name, gpointer pixbuf, gpointer data)
{
	g_free (name);
	gdk_pixbuf_unref (pixbuf);
}

static void
destroy (GtkObject *object)
{
	EvolutionShellClient *shell_client;
	EvolutionShellClientPrivate *priv;
	CORBA_Environment ev;

	shell_client = EVOLUTION_SHELL_CLIENT (object);
	priv = shell_client->priv;

	CORBA_exception_init (&ev);

	if (priv->activity_interface != CORBA_OBJECT_NIL) {
		Bonobo_Unknown_unref (priv->activity_interface, &ev);
		if (ev._major != CORBA_NO_EXCEPTION)
			g_warning ("EvolutionShellClient::destroy: "
				   "Error unreffing the ::Activity interface -- %s\n",
				   ev._repo_id);
		CORBA_Object_release (priv->activity_interface, &ev);
	}

	if (priv->shortcuts_interface != CORBA_OBJECT_NIL) {
		Bonobo_Unknown_unref (priv->shortcuts_interface, &ev);
		if (ev._major != CORBA_NO_EXCEPTION)
			g_warning ("EvolutionShellClient::destroy: "
				   "Error unreffing the ::Shortcuts interface -- %s\n",
				   ev._repo_id);
		CORBA_Object_release (priv->shortcuts_interface, &ev);
	}

	if (priv->storage_registry_interface != CORBA_OBJECT_NIL) {
		Bonobo_Unknown_unref (priv->storage_registry_interface, &ev);
		if (ev._major != CORBA_NO_EXCEPTION)
			g_warning ("EvolutionShellClient::destroy: "
				   "Error unreffing the ::StorageRegistry interface -- %s\n",
				   ev._repo_id);
		CORBA_Object_release (priv->storage_registry_interface, &ev);
	}

	CORBA_exception_free (&ev);

	g_hash_table_foreach (priv->icons, unref_pixbuf, NULL);
	g_hash_table_destroy (priv->icons);

	g_free (priv);

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
	priv->activity_interface         = CORBA_OBJECT_NIL;
	priv->shortcuts_interface        = CORBA_OBJECT_NIL;
	priv->storage_registry_interface = CORBA_OBJECT_NIL;
	priv->icons = g_hash_table_new (g_str_hash, g_str_equal);

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
				  GNOME_Evolution_Shell corba_shell)
{
	EvolutionShellClientPrivate *priv;

	g_return_if_fail (shell_client != NULL);
	g_return_if_fail (EVOLUTION_IS_SHELL_CLIENT (shell_client));
	g_return_if_fail (corba_shell != CORBA_OBJECT_NIL);

	bonobo_object_construct (BONOBO_OBJECT (shell_client), (CORBA_Object) corba_shell);

	priv = shell_client->priv;
	g_return_if_fail (priv->activity_interface == CORBA_OBJECT_NIL);

	priv->activity_interface = query_shell_interface (shell_client, "IDL:GNOME/Evolution/Activity:1.0");
	priv->shortcuts_interface = query_shell_interface (shell_client, "IDL:GNOME/Evolution/Shortcuts:1.0");
	priv->storage_registry_interface = query_shell_interface (shell_client, "IDL:GNOME/Evolution/StorageRegistry:1.0");
}

/**
 * evolution_shell_client_new:
 * @corba_shell: A pointer to the CORBA Evolution::Shell interface.
 * 
 * Create a new client object for @corba_shell. The shell client will
 * free @corba_shell when it is destroyed.
 * 
 * Return value: A pointer to the Evolution::Shell client BonoboObject.
 **/
EvolutionShellClient *
evolution_shell_client_new (GNOME_Evolution_Shell corba_shell)
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
 * @parent: Parent window for the dialog (must be realized when invoking)
 * @title: The title for the folder selection dialog
 * @default_folder: URI (physical or evolution:) of the folder initially selected on the dialog
 * @folder_return: 
 * 
 * Pop up the shell's folder selection dialog with the specified
 * @title and @default_folder as the initially selected folder. On
 * return, set *@folder_return to the folder structure for the
 * selected folder (or %NULL if the user cancelled the dialog). (The
 * dialog is modal.)
 **/
void
evolution_shell_client_user_select_folder (EvolutionShellClient *shell_client,
					   GtkWindow *parent,
					   const char *title,
					   const char *default_folder,
					   const char **possible_types,
					   GNOME_Evolution_Folder **folder_return)
{
	/* Do this first so it can be checked as a return value, even
	 * if we g_return_if_fail.
	 */
	if (folder_return)
		*folder_return = CORBA_OBJECT_NIL;

	g_return_if_fail (shell_client != NULL);
	g_return_if_fail (EVOLUTION_IS_SHELL_CLIENT (shell_client));
	g_return_if_fail (title != NULL);
	g_return_if_fail (default_folder != NULL);
	g_return_if_fail (parent == NULL || GTK_WIDGET_REALIZED (parent));

	user_select_folder (shell_client, parent, title, default_folder,
			    possible_types, folder_return);
}


/**
 * evolution_shell_client_get_activity_interface:
 * @shell_client: An EvolutionShellClient object
 * 
 * Get the GNOME::Evolution::Activity for the shell associated with
 * @shell_client.
 * 
 * Return value: A CORBA Object represeting the GNOME::Evolution::Activity
 * interface.
 **/
GNOME_Evolution_Activity
evolution_shell_client_get_activity_interface (EvolutionShellClient *shell_client)
{
	g_return_val_if_fail (shell_client != NULL, CORBA_OBJECT_NIL);
	g_return_val_if_fail (EVOLUTION_IS_SHELL_CLIENT (shell_client), CORBA_OBJECT_NIL);

	return shell_client->priv->activity_interface;
}

/**
 * evolution_shell_client_get_shortcuts_interface:
 * @shell_client: An EvolutionShellClient object
 * 
 * Get the GNOME::Evolution::Shortcuts for the shell associated with
 * @shell_client.
 * 
 * Return value: A CORBA Object represeting the GNOME::Evolution::Shortcuts
 * interface.
 **/
GNOME_Evolution_Shortcuts
evolution_shell_client_get_shortcuts_interface  (EvolutionShellClient *shell_client)
{
	g_return_val_if_fail (shell_client != NULL, CORBA_OBJECT_NIL);
	g_return_val_if_fail (EVOLUTION_IS_SHELL_CLIENT (shell_client), CORBA_OBJECT_NIL);

	return shell_client->priv->shortcuts_interface;
}

/**
 * evolution_shell_client_get_storage_registry_interface:
 * @shell_client: An EvolutionShellClient object
 * 
 * Get the GNOME::Evolution::StorageRegistry for the shell associated
 * with @shell_client.
 * 
 * Return value: A CORBA Object represeting the
 * GNOME::Evolution::StorageRegistry interface.
 **/
GNOME_Evolution_StorageRegistry
evolution_shell_client_get_storage_registry_interface (EvolutionShellClient *shell_client)
{
	g_return_val_if_fail (shell_client != NULL, CORBA_OBJECT_NIL);
	g_return_val_if_fail (EVOLUTION_IS_SHELL_CLIENT (shell_client), CORBA_OBJECT_NIL);

	return shell_client->priv->storage_registry_interface;
}


/**
 * evolution_shell_client_get_local_storage:
 * @shell_client: An EvolutionShellClient object
 * 
 * Retrieve the local storage interface for this shell.
 * 
 * Return value: a pointer to the CORBA object implementing the local storage
 * in the shell associated with @shell_client.
 **/
GNOME_Evolution_Storage
evolution_shell_client_get_local_storage (EvolutionShellClient *shell_client)
{
	GNOME_Evolution_Shell corba_shell;
	GNOME_Evolution_Storage corba_local_storage;
	CORBA_Environment ev;

	g_return_val_if_fail (shell_client != NULL, CORBA_OBJECT_NIL);
	g_return_val_if_fail (EVOLUTION_IS_SHELL_CLIENT (shell_client), CORBA_OBJECT_NIL);

	CORBA_exception_init (&ev);

	corba_shell = bonobo_object_corba_objref (BONOBO_OBJECT (shell_client));
	if (corba_shell == CORBA_OBJECT_NIL) {
		g_warning ("evolution_shell_client_get_local_storage() invoked on an "
			   "EvolutionShellClient that doesn't have a CORBA objref???");
		CORBA_exception_free (&ev);
		return CORBA_OBJECT_NIL;
	}

	corba_local_storage = GNOME_Evolution_Shell_getLocalStorage (corba_shell, &ev);
	if (ev._major != CORBA_NO_EXCEPTION) {
		g_warning ("evolution_shell_client_get_local_storage() failing -- %s ???", ev._repo_id);
		CORBA_exception_free (&ev);
		return CORBA_OBJECT_NIL;
	}

	CORBA_exception_free (&ev);

	return corba_local_storage;
}

void
evolution_shell_client_set_line_status (EvolutionShellClient *shell_client,
					gboolean              line_status)
{
	GNOME_Evolution_Shell corba_shell;
	CORBA_Environment ev;

	g_return_if_fail (shell_client != NULL);
	g_return_if_fail (EVOLUTION_IS_SHELL_CLIENT (shell_client));

	CORBA_exception_init (&ev);

	corba_shell = bonobo_object_corba_objref (BONOBO_OBJECT (shell_client));
	if (corba_shell == CORBA_OBJECT_NIL)
		return;

	GNOME_Evolution_Shell_setLineStatus (corba_shell, line_status, &ev);

	CORBA_exception_free (&ev);
}


GdkPixbuf *
evolution_shell_client_get_pixbuf_for_type (EvolutionShellClient *shell_client,
					    const char *folder_type,
					    gboolean mini)
{
	GNOME_Evolution_Shell corba_shell;
	CORBA_Environment ev;
	GNOME_Evolution_Icon *icon;
	GdkPixbuf *pixbuf;
	char *hash_name;

	g_return_val_if_fail (EVOLUTION_IS_SHELL_CLIENT (shell_client), NULL);

	hash_name = g_strdup_printf ("%s/%s", folder_type,
				     mini ? "mini" : "large");
	pixbuf = g_hash_table_lookup (shell_client->priv->icons, hash_name);
	if (!pixbuf) {
		corba_shell = bonobo_object_corba_objref (BONOBO_OBJECT (shell_client));
		g_return_val_if_fail (corba_shell != CORBA_OBJECT_NIL, NULL);

		CORBA_exception_init (&ev);
		icon = GNOME_Evolution_Shell_getIconByType (corba_shell,
							    folder_type, mini,
							    &ev);
		if (ev._major != CORBA_NO_EXCEPTION) {
			g_free (hash_name);
			return NULL;
		}
		CORBA_exception_free (&ev);

		pixbuf = e_new_gdk_pixbuf_from_corba_icon (icon, icon->width,
							   icon->height);
		CORBA_free (icon);

		g_hash_table_insert (shell_client->priv->icons,
				     hash_name, pixbuf);
	} else
		g_free (hash_name);

	gdk_pixbuf_ref (pixbuf);
	return pixbuf;
}


GtkWidget *
evolution_shell_client_create_storage_set_view (EvolutionShellClient *shell_client,
						Bonobo_UIComponent uic,
						Bonobo_Control *bonobo_control_iface_return,
						GNOME_Evolution_StorageSetView *storage_set_view_iface_return,
						CORBA_Environment *ev)
{
	GNOME_Evolution_Shell corba_shell;
	CORBA_Environment my_ev;
	Bonobo_Control control;
	GtkWidget *control_widget;

	g_return_val_if_fail (EVOLUTION_IS_SHELL_CLIENT (shell_client), NULL);

	CORBA_exception_init (&my_ev);
	if (ev == NULL)
		ev = &my_ev;

	corba_shell = BONOBO_OBJREF (shell_client);

	control = GNOME_Evolution_Shell_createStorageSetView (corba_shell, ev);
	if (BONOBO_EX (ev)) {
		g_warning ("Cannot create StorageSetView -- %s", BONOBO_EX_ID (ev));
		CORBA_exception_free (&my_ev);
		return NULL;
	}

	if (bonobo_control_iface_return != NULL)
		*bonobo_control_iface_return = control;

	control_widget = bonobo_widget_new_control_from_objref (control, uic);

	if (storage_set_view_iface_return != NULL) {
		*storage_set_view_iface_return = Bonobo_Unknown_queryInterface (control,
										"IDL:GNOME/Evolution/StorageSetView:1.0",
										ev);
		if (BONOBO_EX (ev))
			*storage_set_view_iface_return = NULL;
	}

	CORBA_exception_free (&my_ev);
	return control_widget;
}


E_MAKE_TYPE (evolution_shell_client, "EvolutionShellClient", EvolutionShellClient, class_init, init, PARENT_TYPE)
