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

#include "e-util/e-util.h"

#include "e-folder-type-repository.h"
#include "e-local-storage.h"
#include "e-shell-view.h"
#include "e-shortcuts.h"
#include "e-storage-set.h"

#include "e-shell.h"


#define PARENT_TYPE GTK_TYPE_OBJECT
static GtkObjectClass *parent_class = NULL;

struct _EShellPrivate {
	char *local_directory;

	EStorageSet *storage_set;
	EShortcuts *shortcuts;
	EFolderTypeRepository *folder_type_repository;
};

#define SHORTCUTS_FILE_NAME     "shortcuts.xml"
#define LOCAL_STORAGE_DIRECTORY "local"


/* Initialization of the storages.  */

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
	g_free (local_storage_path);

	if (local_storage == NULL)
		return FALSE;

	priv->storage_set = e_storage_set_new ();
	e_storage_set_add_storage (priv->storage_set, local_storage);

	return TRUE;
}


/* GtkObject methods.  */

static void
destroy (GtkObject *object)
{
	EShell *shell;
	EShellPrivate *priv;

	shell = E_SHELL (object);
	priv = shell->priv;

	gtk_object_unref (GTK_OBJECT (priv->storage_set));
	gtk_object_unref (GTK_OBJECT (priv->shortcuts));
	gtk_object_unref (GTK_OBJECT (priv->folder_type_repository));

	g_free (priv);

	(* GTK_OBJECT_CLASS (parent_class)->destroy) (object);
}


static void
class_init (EShellClass *klass)
{
	GtkObjectClass *object_class;

	object_class = GTK_OBJECT_CLASS (klass);
	object_class->destroy = destroy;

	parent_class = gtk_type_class (gtk_object_get_type ());

}

static void
init (EShell *shell)
{
	EShellPrivate *priv;

	priv = g_new (EShellPrivate, 1);

	priv->local_directory        = NULL;
	priv->storage_set            = NULL;
	priv->shortcuts              = NULL;
	priv->folder_type_repository = NULL;

	shell->priv = priv;
}


void
e_shell_construct (EShell *shell,
		   const char *local_directory)
{
	EShellPrivate *priv;
	gchar *shortcut_path;

	g_return_if_fail (shell != NULL);
	g_return_if_fail (E_IS_SHELL (shell));
	g_return_if_fail (local_directory != NULL);
	g_return_if_fail (g_path_is_absolute (local_directory));

	GTK_OBJECT_UNSET_FLAGS (shell, GTK_FLOATING);

	priv = shell->priv;

	priv->local_directory = g_strdup (local_directory);

	if (! setup_storages (shell))
		return;

	priv->folder_type_repository = e_folder_type_repository_new ();
	priv->shortcuts              = e_shortcuts_new (priv->storage_set, priv->folder_type_repository);

	shortcut_path = g_concat_dir_and_file (local_directory, "shortcuts.xml");

	if (! e_shortcuts_load (priv->shortcuts, shortcut_path)) {
		gtk_object_unref (GTK_OBJECT (priv->shortcuts));
		priv->shortcuts = NULL;
	}

	g_free (shortcut_path);
}

EShell *
e_shell_new (const char *local_directory)
{
	EShell *new;
	EShellPrivate *priv;

	new = gtk_type_new (e_shell_get_type ());
	e_shell_construct (new, local_directory);

	priv = new->priv;

	if (priv->shortcuts == NULL || priv->storage_set == NULL) {
		gtk_object_unref (GTK_OBJECT (new));
		return NULL;
	}

	return new;
}


GtkWidget *
e_shell_new_view (EShell *shell,
		  const char *uri)
{
	GtkWidget *view_widget;
	EShellView *shell_view;

	g_return_val_if_fail (shell != NULL, NULL);
	g_return_val_if_fail (E_IS_SHELL (shell), NULL);

	view_widget = e_shell_view_new (shell, uri);
	shell_view = E_SHELL_VIEW (view_widget);

	gtk_widget_show (view_widget);

	return view_widget;
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


E_MAKE_TYPE (e_shell, "EShell", EShell, class_init, init, PARENT_TYPE)
