/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* mail-local-storage.h
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

/* This handles the interfacing with the shell's local storage.  */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <glib.h>
#include <gtk/gtk.h>
#include <orb/orbit.h>

#include "e-folder-tree.h"
#include "evolution-storage-listener.h"

#include "mail-local-storage.h"


/* Static stuff.  Sigh, it sucks, but it fits in the way the whole mail
   compnent is written.  */

/* The interface to the local storage.  */
static Evolution_LocalStorage corba_local_storage = CORBA_OBJECT_NIL;

/* The listener on our side.  We get notified of things happening in the local
   storage through this.  */
static EvolutionStorageListener *local_storage_listener = NULL;

/* The folder set.  The folder data is an Evolution_Folder, allocated through
   CORBA normally.  */
static EFolderTree *folder_tree = NULL;


/* Folder destroy notification function for the `folder_tree'.  */

static void
folder_tree_folder_notify_cb (EFolderTree *tree,
			      const char *path,
			      void *data,
			      void *closure)
{
	Evolution_Folder *corba_folder;

	corba_folder = (Evolution_Folder *) data;
	CORBA_free (corba_folder);
}


/* Callbacks for the EvolutionStorageListner signals.  */

static void
local_storage_destroyed_cb (EvolutionStorageListener *storage_listener,
			    void *data)
{
	/* FIXME: Dunno how to handle this yet.  */
	g_warning ("%s -- The LocalStorage has gone?!", __FILE__);
}

static void
local_storage_new_folder_cb (EvolutionStorageListener *storage_listener,
			     const char *path,
			     const Evolution_Folder *folder,
			     void *data)
{
	Evolution_Folder *copy_of_folder;
	CORBA_Environment ev;

	if (strcmp (folder->type, "mail") != 0)
		return;

	CORBA_exception_init (&ev);

#if 0
	/* This is how we could do to display extra information about the
           folder.  */
	display_name = g_strconcat (folder->display_name, _(" (XXX unread)"), NULL);
	Evolution_LocalStorage_set_display_name (corba_local_storage, path, display_name, &ev);
#endif

	copy_of_folder = Evolution_Folder__alloc ();
	copy_of_folder->type         = CORBA_string_dup (folder->type);
	copy_of_folder->description  = CORBA_string_dup (folder->description);
	copy_of_folder->display_name = CORBA_string_dup (folder->display_name);
	copy_of_folder->physical_uri = CORBA_string_dup (folder->physical_uri);

	e_folder_tree_add (folder_tree, path, copy_of_folder);

	CORBA_exception_free (&ev);
}

static void
local_storage_removed_folder_cb (EvolutionStorageListener *storage_listener,
				 const char *path,
				 void *data)
{
	/* Prevent a warning from `e_folder_tree_remove()'.  */
	if (e_folder_tree_get_folder (folder_tree, path) == NULL)
		return;

	e_folder_tree_remove (folder_tree, path);
}


gboolean
mail_local_storage_startup (EvolutionShellClient *shell_client)
{
	Evolution_StorageListener corba_local_storage_listener;

	CORBA_Environment ev;

	CORBA_exception_init (&ev);

	corba_local_storage = evolution_shell_client_get_local_storage (shell_client);
	if (corba_local_storage == CORBA_OBJECT_NIL) {
		CORBA_exception_free (&ev);
		return FALSE;
	}

	local_storage_listener = evolution_storage_listener_new ();
	corba_local_storage_listener = evolution_storage_listener_corba_objref (local_storage_listener);

	gtk_signal_connect (GTK_OBJECT (local_storage_listener), "destroyed",
			    GTK_SIGNAL_FUNC (local_storage_destroyed_cb), NULL);
	gtk_signal_connect (GTK_OBJECT (local_storage_listener), "new_folder",
			    GTK_SIGNAL_FUNC (local_storage_new_folder_cb), NULL);
	gtk_signal_connect (GTK_OBJECT (local_storage_listener), "removed_folder",
			    GTK_SIGNAL_FUNC (local_storage_removed_folder_cb), NULL);

	Evolution_Storage_add_listener (corba_local_storage, corba_local_storage_listener, &ev);
	if (ev._major != CORBA_NO_EXCEPTION) {
		g_warning ("%s -- Cannot add a listener to the Local Storage.", __FILE__);

		gtk_object_unref (GTK_OBJECT (local_storage_listener));

		Bonobo_Unknown_unref (corba_local_storage, &ev);
		CORBA_Object_release (corba_local_storage, &ev);

		CORBA_exception_free (&ev);
		return FALSE;
	}

	CORBA_exception_free (&ev);

	folder_tree = e_folder_tree_new (folder_tree_folder_notify_cb, NULL);

	return TRUE;
}

void
mail_local_storage_shutdown (void)
{
	CORBA_Environment ev;

	CORBA_exception_init (&ev);

	Bonobo_Unknown_unref (corba_local_storage, &ev);
	CORBA_Object_release (corba_local_storage, &ev);
	corba_local_storage = CORBA_OBJECT_NIL;

	gtk_object_unref (GTK_OBJECT (local_storage_listener));
	local_storage_listener = NULL;

	gtk_object_unref (GTK_OBJECT (folder_tree));
	folder_tree = NULL;

	CORBA_exception_free (&ev);
}
