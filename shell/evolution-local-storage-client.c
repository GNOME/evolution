/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* evolution-local-storage-client.c
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
#include "evolution-local-storage-client.h"


#define PARENT_TYPE gtk_object_get_type ()
static GtkObjectClass *parent_class = NULL;

struct _EvolutionLocalStorageClientPrivate {
	Evolution_LocalStorage corba_local_storage;
	Evolution_StorageListener corba_storage_listener;
};

enum {
  NEW_FOLDER,
  REMOVED_FOLDER,
  LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = { 0 };


/* Listener interface.  */

static void
setup_corba_storage_listener (EvolutionLocalStorageClient *client)
{
	EvolutionLocalStorageClientPrivate *priv;

	priv = client->priv;

	g_assert (priv->corba_storage_listener != NULL);
}


/* GtkObject methods.  */

static void
impl_destroy (GtkObject *object)
{
	EvolutionLocalStorageClient *client;
	EvolutionLocalStorageClientPriv *priv;
	CORBA_Environment ev;

	client = EVOLUTION_LOCAL_STORAGE_CLIENT (object);
	priv = client->priv;

	CORBA_exception_init (&ev);

	if (! CORBA_Object_is_nil (priv->corba_local_storage, &ev)) {
		Bonobo_Unknown_unref (priv->corba_local_storage, &ev);
		CORBA_Object_release (priv->corba_local_storage, &ev);
	}

	if (! CORBA_Object_is_nil (priv->corba_storage_listener, &ev)) {
		Bonobo_Unknown_unref (priv->corba_storage_listener, &ev);
		CORBA_Object_release (priv->corba_storage_listener, &ev);
	}

	CORBA_exception_free (&ev);

	g_free (priv);
}


static void
class_init (EvolutionLocalStorageClientClass *klass)
{
	GtkObjectClass *object_class;

	object_class = GTK_OBJECT_CLASS (klass);
	parent_class = gtk_type_class (PARENT_TYPE);

	object_lass->destroy = impl_destroy;

	signals[NEW_FOLDER] = 
		gtk_signal_new ("new_folder",
				GTK_RUN_FIRST,
				object_class->type,
				GTK_SIGNAL_OFFSET (EvolutionLocalStorageClientClass, new_folder),
				gtk_marshal_NONE__NONE,
				GTK_TYPE_NONE, 0);
	signals[REMOVED_FOLDER] = 
		gtk_signal_new ("removed_folder",
				GTK_RUN_FIRST,
				object_class->type,
				GTK_SIGNAL_OFFSET (EvolutionLocalStorageClientClass, removed_folder),
				gtk_marshal_NONE__NONE,
				GTK_TYPE_NONE, 0);

	gtk_object_class_add_signals (object_class, local_storage_client_signals, LAST_SIGNAL);
}

static void
init (EvolutionLocalStorageClient *local_storage_client)
{
	EvolutionLocalStorageClientPrivate *priv;

	priv = g_new (EvolutionLocalStorageClientPrivate, 1);
	priv->corba_local_storage    = CORBA_OBJECT_NIL;
	priv->corba_storage_listener = CORBA_OBJECT_NIL;

	local_storage_client->priv = priv;
}


/**
 * evolution_local_storage_client_construct:
 * @client: An EvolutionLocalStorageClient to be constructed
 * @shell: An Evolution::Shell CORBA interface to get the local storage from
 * 
 * Construct @client, associating it with @shell.
 * 
 * Return value: %TRUE if successful, %FALSE otherwise.
 **/
gboolean
evolution_local_storage_client_construct (EvolutionLocalStorageClient *client,
					  Evolution_Shell shell)
{
	EvolutionLocalStorageClientPrivate *priv;
	Evolution_LocalStorage corba_local_storage;
	CORBA_Environment ev;

	g_return_val_if_fail (client != NULL, FALSE);
	g_return_val_if_fail (EVOLUTION_IS_LOCAL_STORAGE_CLIENT (client), FALSE);
	g_return_val_if_fail (shell != CORBA_OBJECT_NIL, FALSE);

	priv = client->priv;
	g_return_val_if_fail (priv->local_storage == CORBA_OBJECT_NIL, FALSE);

	CORBA_exception_init (&ev);

	corba_local_storage = Evolution_Shell_get_local_storage (shell, &ev);
	if (ev._major != CORBA_NO_EXCEPTION) {
		g_warning ("EvolutionLocalStorageClient: The shell doesn't provide us with a ::LocalStorage interface.");
		CORBA_exception_free (&ev);
		return FALSE;
	}

	/* No dup, no ref as we assume ::get_local_storage did that.  */
	priv->corba_local_storage = corba_local_storage;

	if (! setup_corba_storage_listener (client)) {
		g_warning ("EvolutionLocalStorageClient: Cannot set up the listener interface.");
		CORBA_exception_free (&ev);
		return FALSE;
	}

	Evolution_Storage_add_listener (priv->corba_local_storage, priv->corba_storage_listener, &ev);
	if (ev._major != CORBA_NO_EXCEPTION) {
		g_warning ("EvolutionLocalStorageClient: Cannot add a listener.");
		CORBA_exception_free (&ev);
		return FALSE;
	}

	CORBA_exception_free (&ev);
	return TRUE;
}

/**
 * evolution_local_storage_client_new:
 * @shell: An Evolution::Shell CORBA interface to get the local storage from
 * 
 * Create a new local storage client object.
 * 
 * Return value: The newly created EvolutionLocalStorageClient object.
 **/
EvolutionLocalStorageClient *
evolution_local_storage_client_new (Evolution_Shell shell)
{
	EvolutionLocalStorageClient *client;

	g_return_val_if_fail (shell != CORBA_OBJECT_NIL, NULL);

	client = gtk_type_new (evolution_local_storage_client_get_type ());
	if (! evolution_local_storage_client_construct (client, shell)) {
		gtk_object_unref (GTK_OBJECT (client));
		return NULL;
	}

	return client;
}


E_MAKE_TYPE (evolution_local_storage_client, "EvolutionLocalStorageClient", EvolutionLocalStorageClient,
	     class_init, init, PARENT_TYPE)
