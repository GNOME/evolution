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

#include <gtk/gtkobject.h>
#include <gtk/gtktypeutils.h>

#include "Evolution.h"

#include "e-util/e-util.h"

#include "e-corba-storage-registry.h"
#include "e-folder-type-repository.h"
#include "e-local-storage.h"
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
	EFolderTypeRepository *folder_type_repository;

	ECorbaStorageRegistry *corba_storage_registry;
};

#define SHORTCUTS_FILE_NAME     "shortcuts.xml"
#define LOCAL_STORAGE_DIRECTORY "local"

enum {
	NO_VIEWS_LEFT,
	LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = { 0 };


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

static void
impl_Shell_dummy_method (PortableServer_Servant servant,
			 CORBA_Environment *ev)
{
	g_print ("Evolution::Shell::dummy_method invoked!\n");
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
setup_storages (EShell *shell)
{
	EStorage *local_storage;
	EShellPrivate *priv;
	gchar *local_storage_path;

	priv = shell->priv;

	local_storage_path = g_concat_dir_and_file (priv->local_directory,
						    LOCAL_STORAGE_DIRECTORY);
	local_storage = e_local_storage_open (local_storage_path);
	if (local_storage == NULL) {
		g_warning (_("Cannot set up local storage -- %s"), local_storage_path);
		g_free (local_storage_path);
		return FALSE;
	}
	g_free (local_storage_path);

	g_assert (shell->priv->folder_type_repository);

	priv->storage_set = e_storage_set_new (shell->priv->folder_type_repository);
	e_storage_set_add_storage (priv->storage_set, local_storage);

	return setup_corba_storages (shell);
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

	if (priv->storage_set != NULL)
		gtk_object_unref (GTK_OBJECT (priv->storage_set));

	if (priv->shortcuts != NULL)
		gtk_object_unref (GTK_OBJECT (priv->shortcuts));

	if (priv->folder_type_repository != NULL)
		gtk_object_unref (GTK_OBJECT (priv->folder_type_repository));

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
	epv->dummy_method = impl_Shell_dummy_method;

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
	priv->folder_type_repository = NULL;
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
	g_return_if_fail (local_directory != NULL);
	g_return_if_fail (g_path_is_absolute (local_directory));

	bonobo_object_construct (BONOBO_OBJECT (shell), corba_object);

	priv = shell->priv;

	priv->local_directory = g_strdup (local_directory);
	priv->folder_type_repository = e_folder_type_repository_new ();

	if (! setup_storages (shell))
		return;

	priv->shortcuts = e_shortcuts_new (priv->storage_set, priv->folder_type_repository);

	shortcut_path = g_concat_dir_and_file (local_directory, "shortcuts.xml");

	if (! e_shortcuts_load (priv->shortcuts, shortcut_path)) {
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

EFolderTypeRepository *
e_shell_get_folder_type_repository (EShell *shell)
{
	g_return_val_if_fail (shell != NULL, NULL);
	g_return_val_if_fail (E_IS_SHELL (shell), NULL);

	return shell->priv->folder_type_repository;
}


void
e_shell_quit (EShell *shell)
{
	g_return_if_fail (shell != NULL);
	g_return_if_fail (E_IS_SHELL (shell));

	bonobo_object_unref (BONOBO_OBJECT (shell));
}


E_MAKE_TYPE (e_shell, "EShell", EShell, class_init, init, PARENT_TYPE)
