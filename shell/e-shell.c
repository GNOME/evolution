/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* e-shell.c
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

#include <gnome.h>

#include "Evolution.h"

#include "e-util/e-util.h"

#include "e-component-registry.h"
#include "e-corba-storage-registry.h"
#include "e-folder-type-registry.h"
#include "e-local-storage.h"
#include "e-shell-constants.h"
#include "e-shell-folder-selection-dialog.h"
#include "e-shell-view.h"
#include "e-shortcuts.h"
#include "e-storage-set.h"

#include "e-shell.h"


#define PARENT_TYPE BONOBO_OBJECT_TYPE
static BonoboObjectClass *parent_class = NULL;

struct _EShellPrivate {
	char *local_directory;

	GList *views;

	EStorageSet *storage_set;
	EShortcuts *shortcuts;
	EFolderTypeRegistry *folder_type_registry;

	EComponentRegistry *component_registry;

	ECorbaStorageRegistry *corba_storage_registry;
};


/* Constants.  */

#define SHORTCUTS_FILE_NAME     "shortcuts.xml"
#define LOCAL_STORAGE_DIRECTORY "local"

#ifdef USING_OAF
#define MAIL_COMPONENT_ID	 "OAFIID:evolution-shell-component:evolution-mail:d3cb3ed6-a654-4337-8aa0-f443751d6d1b"
#define CALENDAR_COMPONENT_ID    "OAFIID:evolution-shell-component:evolution-calendar:2eb9eb63-d305-4918-9c35-faae5db19e51"
#define ADDRESSBOOK_COMPONENT_ID "OAFIID:evolution-shell-component:addressbook:b7a26547-7014-4bb5-98ab-2bcac2bb55ca"
#else
#define MAIL_COMPONENT_ID	 "evolution-shell-component:evolution-mail"
#define CALENDAR_COMPONENT_ID    "evolution-shell-component:evolution-calendar"
#define ADDRESSBOOK_COMPONENT_ID "evolution-shell-component:addressbook"
#endif


enum {
	NO_VIEWS_LEFT,
	LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = { 0 };


/* Callback for the folder selection dialog.  */

static void
folder_selection_dialog_clicked_cb (GnomeDialog *dialog,
				    int button_number,
				    void *data)
{
	EShellFolderSelectionDialog *folder_selection_dialog;
	Evolution_FolderSelectionListener listener;
	EShell *shell;
	EStorageSet *storage_set;
	EFolder *folder;
	CORBA_Environment ev;
	const char *path;
	char *uri;
	const char *physical_uri;

	folder_selection_dialog = E_SHELL_FOLDER_SELECTION_DIALOG (dialog);
	shell = E_SHELL (data);
	listener = gtk_object_get_data (GTK_OBJECT (dialog), "corba_listener");

	storage_set = e_shell_get_storage_set (shell);
	path = e_shell_folder_selection_dialog_get_selected_path (folder_selection_dialog),
	folder = e_storage_set_get_folder (storage_set, path);

	uri = g_strconcat (E_SHELL_URI_PREFIX, path, NULL);

	if (folder == NULL)	/* Uh? */
		physical_uri = "";
	else
		physical_uri = e_folder_get_physical_uri (folder);

	CORBA_exception_init (&ev);

	Evolution_FolderSelectionListener_selected (listener, uri, physical_uri, &ev);

	CORBA_Object_release (listener, &ev);

	CORBA_exception_free (&ev);

	g_free (uri);
}


/* CORBA interface implementation.  */

static POA_Evolution_Shell__vepv shell_vepv;

static POA_Evolution_Shell *
create_servant (void)
{
	POA_Evolution_Shell *servant;
	CORBA_Environment ev;

	servant = (POA_Evolution_Shell *) g_new0 (BonoboObjectServant, 1);
	servant->vepv = &shell_vepv;

	CORBA_exception_init (&ev);

	POA_Evolution_Shell__init ((PortableServer_Servant) servant, &ev);
	if (ev._major != CORBA_NO_EXCEPTION) {
		g_free (servant);
		CORBA_exception_free (&ev);
		return NULL;
	}

	CORBA_exception_free (&ev);

	return servant;
}

static Evolution_ShellComponent
impl_Shell_get_component_for_type (PortableServer_Servant servant,
				   const CORBA_char *type,
				   CORBA_Environment *ev)
{
	BonoboObject *bonobo_object;
	EvolutionShellComponentClient *handler;
	EFolderTypeRegistry *folder_type_registry;
	Evolution_ShellComponent corba_component;
	EShell *shell;

	bonobo_object = bonobo_object_from_servant (servant);
	shell = E_SHELL (bonobo_object);
	folder_type_registry = shell->priv->folder_type_registry;

	handler = e_folder_type_registry_get_handler_for_type (folder_type_registry, type);

	if (handler == NULL) {
		CORBA_exception_set (ev, CORBA_USER_EXCEPTION, ex_Evolution_Shell_NotFound, NULL);
		return CORBA_OBJECT_NIL;
	}

	corba_component = bonobo_object_corba_objref (BONOBO_OBJECT (handler));
	Bonobo_Unknown_ref (corba_component, ev);

	return CORBA_Object_duplicate (corba_component, ev);
}

static void
impl_Shell_user_select_folder (PortableServer_Servant servant,
			       const Evolution_FolderSelectionListener listener,
			       const CORBA_char *title,
			       const CORBA_char *default_folder,
			       CORBA_Environment *ev)
{
	GtkWidget *folder_selection_dialog;
	BonoboObject *bonobo_object;
	Evolution_FolderSelectionListener listener_duplicate;
	EShell *shell;

	bonobo_object = bonobo_object_from_servant (servant);
	shell = E_SHELL (bonobo_object);

	folder_selection_dialog = e_shell_folder_selection_dialog_new (shell, title, default_folder);

	listener_duplicate = CORBA_Object_duplicate (listener, ev);
	gtk_object_set_data (GTK_OBJECT (folder_selection_dialog), "corba_listener", shell);

	gtk_signal_connect (GTK_OBJECT (folder_selection_dialog), "clicked",
			    GTK_SIGNAL_FUNC (folder_selection_dialog_clicked_cb), shell);

	gtk_widget_show (folder_selection_dialog);
}


/* Initialization of the storages.  */

static gboolean
setup_corba_storages (EShell *shell)
{
	EShellPrivate *priv;

	priv = shell->priv;

	g_assert (priv->storage_set != NULL);
	priv->corba_storage_registry = e_corba_storage_registry_new (priv->storage_set);

	if (priv->corba_storage_registry == NULL)
		return FALSE;

	bonobo_object_add_interface (BONOBO_OBJECT (shell),
				     BONOBO_OBJECT (priv->corba_storage_registry));

	return TRUE;
}

static gboolean
setup_local_storage (EShell *shell)
{
	EStorage *local_storage;
	EShellPrivate *priv;
	gchar *local_storage_path;

	priv = shell->priv;

	local_storage_path = g_concat_dir_and_file (priv->local_directory,
						    LOCAL_STORAGE_DIRECTORY);
	local_storage = e_local_storage_open (priv->folder_type_registry,
					      local_storage_path);
	if (local_storage == NULL) {
		g_warning (_("Cannot set up local storage -- %s"), local_storage_path);
		g_free (local_storage_path);
		return FALSE;
	}
	g_free (local_storage_path);

	g_assert (shell->priv->folder_type_registry);

	e_storage_set_add_storage (priv->storage_set, local_storage);

	gtk_object_unref (GTK_OBJECT (local_storage));

	return TRUE;
}


/* Initialization of the components.  */

static void
setup_components (EShell *shell)
{
	EShellPrivate *priv;

	priv = shell->priv;

	priv->component_registry = e_component_registry_new (shell);

	/* FIXME: Hardcoded for now.  */

	if (! e_component_registry_register_component (priv->component_registry, MAIL_COMPONENT_ID))
		g_warning ("Cannot activate mail component -- %s", MAIL_COMPONENT_ID);

	if (! e_component_registry_register_component (priv->component_registry, CALENDAR_COMPONENT_ID))
		g_warning ("Cannot activate calendar component -- %s", CALENDAR_COMPONENT_ID);

	if (! e_component_registry_register_component (priv->component_registry, ADDRESSBOOK_COMPONENT_ID))
		g_warning ("Cannot activate addressbook component -- %s", ADDRESSBOOK_COMPONENT_ID);
}


/* EShellView destruction callback.  */

static void
view_destroy_cb (GtkObject *object,
		 gpointer data)
{
	EShell *shell;

	g_assert (E_IS_SHELL_VIEW (object));

	shell = E_SHELL (data);
	shell->priv->views = g_list_remove (shell->priv->views, object);

	if (shell->priv->views == NULL)
		gtk_signal_emit (GTK_OBJECT (shell), signals[NO_VIEWS_LEFT]);
}


/* GtkObject methods.  */

static void
destroy (GtkObject *object)
{
	EShell *shell;
	EShellPrivate *priv;
	GList *p;

	shell = E_SHELL (object);
	priv = shell->priv;

	g_free (priv->local_directory);

	if (priv->storage_set != NULL)
		gtk_object_unref (GTK_OBJECT (priv->storage_set));

	if (priv->shortcuts != NULL)
		gtk_object_unref (GTK_OBJECT (priv->shortcuts));

	if (priv->folder_type_registry != NULL)
		gtk_object_unref (GTK_OBJECT (priv->folder_type_registry));

	if (priv->component_registry != NULL)
		gtk_object_unref (GTK_OBJECT (priv->component_registry));

	for (p = priv->views; p != NULL; p = p->next) {
		EShellView *view;

		view = E_SHELL_VIEW (p->data);

		gtk_signal_disconnect_by_func (GTK_OBJECT (view),
					       GTK_SIGNAL_FUNC (view_destroy_cb), shell);
		gtk_object_destroy (GTK_OBJECT (view));
	}

	g_list_free (priv->views);

	if (priv->corba_storage_registry != NULL)
		bonobo_object_unref (BONOBO_OBJECT (priv->corba_storage_registry));

	g_free (priv);

	(* GTK_OBJECT_CLASS (parent_class)->destroy) (object);
}


/* Initialization.  */

static void
corba_class_init (void)
{
	POA_Evolution_Shell__vepv *vepv;
	POA_Evolution_Shell__epv *epv;
	PortableServer_ServantBase__epv *base_epv;

	base_epv = g_new0 (PortableServer_ServantBase__epv, 1);
	base_epv->_private    = NULL;
	base_epv->finalize    = NULL;
	base_epv->default_POA = NULL;

	epv = g_new0 (POA_Evolution_Shell__epv, 1);
	epv->get_component_for_type = impl_Shell_get_component_for_type;
	epv->user_select_folder     = impl_Shell_user_select_folder;

	vepv = &shell_vepv;
	vepv->Bonobo_Unknown_epv = bonobo_object_get_epv ();
	vepv->Evolution_Shell_epv = epv;
}

static void
class_init (EShellClass *klass)
{
	GtkObjectClass *object_class;

	parent_class = gtk_type_class (PARENT_TYPE);

	object_class = GTK_OBJECT_CLASS (klass);
	object_class->destroy = destroy;

	signals[NO_VIEWS_LEFT] =
		gtk_signal_new ("no_views_left",
				GTK_RUN_LAST,
				object_class->type,
				GTK_SIGNAL_OFFSET (EShellClass, no_views_left),
				gtk_marshal_NONE__NONE,
				GTK_TYPE_NONE, 0);

	gtk_object_class_add_signals (object_class, signals, LAST_SIGNAL);

	corba_class_init ();
}

static void
init (EShell *shell)
{
	EShellPrivate *priv;

	priv = g_new (EShellPrivate, 1);

	priv->views = NULL;

	priv->local_directory        = NULL;
	priv->storage_set            = NULL;
	priv->shortcuts              = NULL;
	priv->component_registry     = NULL;
	priv->folder_type_registry  = NULL;
	priv->corba_storage_registry = NULL;

	shell->priv = priv;
}


void
e_shell_construct (EShell *shell,
		   Evolution_Shell corba_object,
		   const char *local_directory)
{
	EShellPrivate *priv;
	gchar *shortcut_path;

	g_return_if_fail (shell != NULL);
	g_return_if_fail (E_IS_SHELL (shell));
	g_return_if_fail (corba_object != CORBA_OBJECT_NIL);
	g_return_if_fail (local_directory != NULL);
	g_return_if_fail (g_path_is_absolute (local_directory));

	bonobo_object_construct (BONOBO_OBJECT (shell), corba_object);

	priv = shell->priv;

	priv->local_directory      = g_strdup (local_directory);
	priv->folder_type_registry = e_folder_type_registry_new ();
	priv->storage_set          = e_storage_set_new (shell->priv->folder_type_registry);

	/* CORBA storages must be set up before the components, because otherwise components
           cannot register their own storages.  */
	if (! setup_corba_storages (shell))
		return;

	setup_components (shell);

	/* The local storage depends on the component registry.  */
	setup_local_storage (shell);

	shortcut_path = g_concat_dir_and_file (local_directory, "shortcuts.xml");
	priv->shortcuts = e_shortcuts_new (priv->storage_set,
					   priv->folder_type_registry,
					   shortcut_path);

	if (priv->shortcuts == NULL) {
		gtk_object_unref (GTK_OBJECT (priv->shortcuts));
		priv->shortcuts = NULL;

		g_warning ("Cannot load shortcuts -- %s", shortcut_path);
	}

	g_free (shortcut_path);
}

EShell *
e_shell_new (const char *local_directory)
{
	EShell *new;
	EShellPrivate *priv;
	Evolution_Shell corba_object;
	POA_Evolution_Shell *servant;

	g_return_val_if_fail (local_directory != NULL, NULL);
	g_return_val_if_fail (*local_directory != '\0', NULL);

	servant = create_servant ();
	if (servant == NULL)
		return NULL;

	new = gtk_type_new (e_shell_get_type ());

	corba_object = bonobo_object_activate_servant (BONOBO_OBJECT (new), servant);
	e_shell_construct (new, corba_object, local_directory);

	priv = new->priv;

	if (priv->shortcuts == NULL || priv->storage_set == NULL) {
		bonobo_object_unref (BONOBO_OBJECT (new));
		return NULL;
	}

	return new;
}


GtkWidget *
e_shell_new_view (EShell *shell,
		  const char *uri)
{
	GtkWidget *view;

	g_return_val_if_fail (shell != NULL, NULL);
	g_return_val_if_fail (E_IS_SHELL (shell), NULL);

	view = e_shell_view_new (shell);

	gtk_widget_show (view);
	gtk_signal_connect (GTK_OBJECT (view), "destroy", GTK_SIGNAL_FUNC (view_destroy_cb), shell);

	if (uri != NULL)
		e_shell_view_display_uri (E_SHELL_VIEW (view), uri);

	shell->priv->views = g_list_prepend (shell->priv->views, view);

	return view;
}


EShortcuts *
e_shell_get_shortcuts (EShell *shell)
{
	g_return_val_if_fail (shell != NULL, NULL);
	g_return_val_if_fail (E_IS_SHELL (shell), NULL);

	return shell->priv->shortcuts;
}

EStorageSet *
e_shell_get_storage_set (EShell *shell)
{
	g_return_val_if_fail (shell != NULL, NULL);
	g_return_val_if_fail (E_IS_SHELL (shell), NULL);

	return shell->priv->storage_set;
}

EFolderTypeRegistry *
e_shell_get_folder_type_registry (EShell *shell)
{
	g_return_val_if_fail (shell != NULL, NULL);
	g_return_val_if_fail (E_IS_SHELL (shell), NULL);

	return shell->priv->folder_type_registry;
}


void
e_shell_quit (EShell *shell)
{
	EShellPrivate *priv;
	GList *p;

	g_return_if_fail (shell != NULL);
	g_return_if_fail (E_IS_SHELL (shell));

	priv = shell->priv;

	for (p = priv->views; p != NULL; p = p->next) {
		EShellView *shell_view;

		shell_view = E_SHELL_VIEW (p->data);
		gtk_signal_disconnect_by_func (GTK_OBJECT (shell_view),
					       GTK_SIGNAL_FUNC (view_destroy_cb), shell);
		gtk_widget_destroy (GTK_WIDGET (shell_view));
	}

	g_list_free (priv->views);
	priv->views = NULL;

	bonobo_object_unref (BONOBO_OBJECT (priv->corba_storage_registry));

	priv->corba_storage_registry = NULL;

	e_storage_set_remove_all_storages (priv->storage_set);
	gtk_object_unref (GTK_OBJECT (priv->storage_set));

	gtk_object_unref (GTK_OBJECT (priv->shortcuts));
	gtk_object_unref (GTK_OBJECT (priv->folder_type_registry));
	gtk_object_unref (GTK_OBJECT (priv->component_registry));

	priv->storage_set          = NULL;
	priv->shortcuts            = NULL;
	priv->folder_type_registry = NULL;
	priv->component_registry   = NULL;

	bonobo_object_unref (BONOBO_OBJECT (shell));
}


E_MAKE_TYPE (e_shell, "EShell", EShell, class_init, init, PARENT_TYPE)
