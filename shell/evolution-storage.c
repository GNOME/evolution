/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* evolution-storage.c
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

#include "Evolution.h"

#include "e-util/e-util.h"

#include "e-folder-tree.h"

#include "evolution-storage.h"


#define PARENT_TYPE BONOBO_OBJECT_TYPE
static BonoboObjectClass *parent_class = NULL;

struct _EvolutionStoragePrivate {
	char *name;

	EFolderTree *folder_tree;

	GList *corba_storage_listeners;
};


/* Utility functions.  */

static void
list_through_listener_foreach (EFolderTree *tree,
			       const char *path,
			       void *data,
			       void *closure)
{
	const Evolution_Folder *corba_folder;
	Evolution_StorageListener corba_listener;
	CORBA_Environment ev;

	corba_folder = (Evolution_Folder *) data;
	corba_listener = (Evolution_StorageListener) closure;

	/* The root folder has no data.  */
	if (corba_folder == NULL)
		return;
	
	CORBA_exception_init (&ev);
	Evolution_StorageListener_new_folder (corba_listener, path, corba_folder, &ev);
	CORBA_exception_free (&ev);
}

static void
list_through_listener (EvolutionStorage *storage,
		       Evolution_StorageListener listener,
		       CORBA_Environment *ev)
{
	EvolutionStoragePrivate *priv;

	priv = storage->priv;

	e_folder_tree_foreach (priv->folder_tree,
			       list_through_listener_foreach,
			       listener);
}

static void
add_listener (EvolutionStorage *storage,
	      const Evolution_StorageListener listener)
{
	EvolutionStoragePrivate *priv;
	Evolution_StorageListener listener_copy;
	CORBA_Environment ev;

	priv = storage->priv;

	CORBA_exception_init (&ev);

	listener_copy = CORBA_Object_duplicate (listener, &ev);
	if (ev._major != CORBA_NO_EXCEPTION) {
		/* Panic.  */
		g_warning ("EvolutionStorage -- Cannot duplicate listener.");
		CORBA_exception_free (&ev);
		return;
	}

	priv->corba_storage_listeners = g_list_prepend (priv->corba_storage_listeners,
							listener_copy);

	list_through_listener (storage, listener_copy, &ev);

	CORBA_exception_free (&ev);
}


/* Functions for the EFolderTree in the storage.  */

static void
folder_destroy_notify (EFolderTree *tree,
		       const char *path,
		       void *data,
		       void *closure)
{
	Evolution_Folder *corba_folder;

	corba_folder = (Evolution_Folder *) data;
	CORBA_free (data);
}


/* CORBA interface implementation.  */

static POA_Evolution_Storage__vepv Storage_vepv;

static POA_Evolution_Storage *
create_servant (void)
{
	POA_Evolution_Storage *servant;
	CORBA_Environment ev;

	servant = (POA_Evolution_Storage *) g_new0 (BonoboObjectServant, 1);
	servant->vepv = &Storage_vepv;

	CORBA_exception_init (&ev);

	POA_Evolution_Storage__init ((PortableServer_Servant) servant, &ev);
	if (ev._major != CORBA_NO_EXCEPTION) {
		g_free (servant);
		CORBA_exception_free (&ev);
		return NULL;
	}

	CORBA_exception_free (&ev);

	return servant;
}

static CORBA_char *
impl_Storage__get_name (PortableServer_Servant servant,
			CORBA_Environment *ev)
{
	BonoboObject *bonobo_object;
	EvolutionStorage *storage;
	EvolutionStoragePrivate *priv;

	bonobo_object = bonobo_object_from_servant (servant);
	storage = EVOLUTION_STORAGE (bonobo_object);
	priv = storage->priv;

	return CORBA_string_dup (priv->name);
}

static void
impl_Storage_add_listener (PortableServer_Servant servant,
			   const Evolution_StorageListener listener,
			   CORBA_Environment *ev)
{
	BonoboObject *bonobo_object;
	EvolutionStorage *storage;

	bonobo_object = bonobo_object_from_servant (servant);
	storage = EVOLUTION_STORAGE (bonobo_object);

	add_listener (storage, listener);
}


/* GtkObject methods.  */

static void
destroy (GtkObject *object)
{
	EvolutionStorage *storage;
	EvolutionStoragePrivate *priv;
	CORBA_Environment ev;
	GList *p;

	storage = EVOLUTION_STORAGE (object);
	priv = storage->priv;

	g_free (priv->name);

	if (priv->folder_tree != NULL)
		e_folder_tree_destroy (priv->folder_tree);

	CORBA_exception_init (&ev);

	for (p = priv->corba_storage_listeners; p != NULL; p = p->next) {
		Evolution_StorageListener listener;

		listener = p->data;

		Evolution_StorageListener_destroyed (listener, &ev);

		/* (This is not a Bonobo object, so no unref.)  */
		CORBA_Object_release (listener, &ev);
	}

	g_list_free (priv->corba_storage_listeners);

	CORBA_exception_free (&ev);

	g_free (priv);

	(* GTK_OBJECT_CLASS (parent_class)->destroy) (object);
}


static void
corba_class_init (void)
{
	POA_Evolution_Storage__vepv *vepv;

	PortableServer_ServantBase__epv *base_epv;

	base_epv = g_new0 (PortableServer_ServantBase__epv, 1);
	base_epv->_private    = NULL;
	base_epv->finalize    = NULL;
	base_epv->default_POA = NULL;

	vepv = &Storage_vepv;
	vepv->Bonobo_Unknown_epv = bonobo_object_get_epv ();
	vepv->Evolution_Storage_epv = evolution_storage_get_epv ();
}

static void
class_init (EvolutionStorageClass *klass)
{
	GtkObjectClass *object_class;

	object_class = GTK_OBJECT_CLASS (klass);
	object_class->destroy = destroy;

	parent_class = gtk_type_class (bonobo_object_get_type ());

	corba_class_init ();
}

static void
init (EvolutionStorage *storage)
{
	EvolutionStoragePrivate *priv;

	priv = g_new (EvolutionStoragePrivate, 1);
	priv->folder_tree             = e_folder_tree_new (folder_destroy_notify, storage);
	priv->name                    = NULL;
	priv->corba_storage_listeners = NULL;

	storage->priv = priv;
}


POA_Evolution_Storage__epv *
evolution_storage_get_epv (void)
{
	POA_Evolution_Storage__epv *epv;

	epv = g_new0 (POA_Evolution_Storage__epv, 1);
	epv->_get_name    = impl_Storage__get_name;
	epv->add_listener = impl_Storage_add_listener;

	return epv;
}

void
evolution_storage_construct (EvolutionStorage *storage,
			     Evolution_Storage corba_object,
			     const char *name)
{
	EvolutionStoragePrivate *priv;

	g_return_if_fail (storage != NULL);
	g_return_if_fail (EVOLUTION_IS_STORAGE (storage));
	g_return_if_fail (corba_object != CORBA_OBJECT_NIL);
	g_return_if_fail (name != NULL);
	g_return_if_fail (name[0] != '\0');

	bonobo_object_construct (BONOBO_OBJECT (storage), corba_object);

	priv = storage->priv;
	priv->name = g_strdup (name);
}

EvolutionStorage *
evolution_storage_new (const char *name)
{
	EvolutionStorage *new;
	POA_Evolution_Storage *servant;
	Evolution_Storage corba_object;

	g_return_val_if_fail (name != NULL, NULL);
	g_return_val_if_fail (name[0] != '\0', NULL);

	servant = create_servant ();
	if (servant == NULL)
		return NULL;

	new = gtk_type_new (evolution_storage_get_type ());

	corba_object = bonobo_object_activate_servant (BONOBO_OBJECT (new), servant);
	evolution_storage_construct (new, corba_object, name);

	return new;
}

EvolutionStorageResult
evolution_storage_register (EvolutionStorage *evolution_storage,
			    Evolution_StorageRegistry corba_storage_registry)
{
	EvolutionStorageResult result;
	Evolution_StorageListener corba_storage_listener;
	Evolution_Storage corba_storage;
	EvolutionStoragePrivate *priv;
	CORBA_Environment ev;

	g_return_val_if_fail (evolution_storage != NULL,
			      EVOLUTION_STORAGE_ERROR_INVALIDPARAMETER);
	g_return_val_if_fail (EVOLUTION_IS_STORAGE (evolution_storage),
			      EVOLUTION_STORAGE_ERROR_INVALIDPARAMETER);
	g_return_val_if_fail (corba_storage_registry != CORBA_OBJECT_NIL,
			      EVOLUTION_STORAGE_ERROR_INVALIDPARAMETER);

	priv = evolution_storage->priv;

	if (priv->corba_storage_listeners != NULL)
		return EVOLUTION_STORAGE_ERROR_ALREADYREGISTERED;

	CORBA_exception_init (&ev);

	corba_storage = bonobo_object_corba_objref (BONOBO_OBJECT (evolution_storage));
	corba_storage_listener = Evolution_StorageRegistry_register_storage (corba_storage_registry,
									     corba_storage,
									     priv->name, &ev);

	if (ev._major == CORBA_NO_EXCEPTION) {
		add_listener (evolution_storage, corba_storage_listener);
		result = EVOLUTION_STORAGE_OK;
	} else {
		if (ev._major != CORBA_USER_EXCEPTION)
			result = EVOLUTION_STORAGE_ERROR_CORBA;
		else if (strcmp (CORBA_exception_id (&ev), ex_Evolution_StorageRegistry_Exists) == 0)
			result = EVOLUTION_STORAGE_ERROR_EXISTS;
		else
			result = EVOLUTION_STORAGE_ERROR_GENERIC;
	}

	CORBA_exception_free (&ev);

	return result;
}

EvolutionStorageResult
evolution_storage_register_on_shell (EvolutionStorage *evolution_storage,
				     Evolution_Shell corba_shell)
{
	Evolution_StorageRegistry corba_storage_registry;
	EvolutionStorageResult result;
	CORBA_Environment ev;

	g_return_val_if_fail (evolution_storage != NULL,
			      EVOLUTION_STORAGE_ERROR_INVALIDPARAMETER);
	g_return_val_if_fail (EVOLUTION_IS_STORAGE (evolution_storage),
			      EVOLUTION_STORAGE_ERROR_INVALIDPARAMETER);
	g_return_val_if_fail (corba_shell != CORBA_OBJECT_NIL,
			      EVOLUTION_STORAGE_ERROR_INVALIDPARAMETER);

	CORBA_exception_init (&ev);

	corba_storage_registry = Bonobo_Unknown_query_interface (corba_shell,
								 "IDL:Evolution/StorageRegistry:1.0",
								 &ev);
	if (corba_storage_registry == CORBA_OBJECT_NIL || ev._major != CORBA_NO_EXCEPTION) {
		CORBA_exception_free (&ev);
		return EVOLUTION_STORAGE_ERROR_NOREGISTRY;
	}

	result = evolution_storage_register (evolution_storage, corba_storage_registry);

	Bonobo_Unknown_unref (corba_storage_registry, &ev);
	CORBA_Object_release (corba_storage_registry, &ev);

	CORBA_exception_free (&ev);

	return result;
}

EvolutionStorageResult
evolution_storage_new_folder (EvolutionStorage *evolution_storage,
			      const char *path,
			      const char *display_name,
			      const char *type,
			      const char *physical_uri,
			      const char *description)
{
	EvolutionStorageResult result;
	EvolutionStoragePrivate *priv;
	Evolution_Folder *corba_folder;
	CORBA_Environment ev;
	GList *p;

	g_return_val_if_fail (evolution_storage != NULL,
			      EVOLUTION_STORAGE_ERROR_INVALIDPARAMETER);
	g_return_val_if_fail (EVOLUTION_IS_STORAGE (evolution_storage),
			      EVOLUTION_STORAGE_ERROR_INVALIDPARAMETER);
	g_return_val_if_fail (path != NULL, EVOLUTION_STORAGE_ERROR_INVALIDPARAMETER);
	g_return_val_if_fail (g_path_is_absolute (path), EVOLUTION_STORAGE_ERROR_INVALIDPARAMETER);
	g_return_val_if_fail (type != NULL, EVOLUTION_STORAGE_ERROR_INVALIDPARAMETER);
	g_return_val_if_fail (physical_uri != NULL, EVOLUTION_STORAGE_ERROR_INVALIDPARAMETER);

	if (description == NULL)
		description = "";

	priv = evolution_storage->priv;

	CORBA_exception_init (&ev);

	corba_folder = Evolution_Folder__alloc ();
	corba_folder->display_name = CORBA_string_dup (display_name);
	corba_folder->description  = CORBA_string_dup (description);
	corba_folder->type         = CORBA_string_dup (type);
	corba_folder->physical_uri = CORBA_string_dup (physical_uri);

	result = EVOLUTION_STORAGE_OK;

	for (p = priv->corba_storage_listeners; p != NULL; p = p->next) {
		Evolution_StorageListener listener;

		listener = p->data;
		Evolution_StorageListener_new_folder (listener, path, corba_folder, &ev);

		if (ev._major == CORBA_NO_EXCEPTION)
			continue;

		if (ev._major != CORBA_USER_EXCEPTION)
			result = EVOLUTION_STORAGE_ERROR_CORBA;
		else if (strcmp (CORBA_exception_id (&ev), ex_Evolution_StorageListener_Exists) == 0)
			result = EVOLUTION_STORAGE_ERROR_EXISTS;
		else
			result = EVOLUTION_STORAGE_ERROR_GENERIC;

		break;
	}

	CORBA_exception_free (&ev);

	if (result == EVOLUTION_STORAGE_OK) {
		if (! e_folder_tree_add (priv->folder_tree, path, corba_folder))
			result = EVOLUTION_STORAGE_ERROR_EXISTS;
	}

	return result;
}

EvolutionStorageResult
evolution_storage_removed_folder (EvolutionStorage *evolution_storage,
				  const char *path)
{
	EvolutionStorageResult result;
	EvolutionStoragePrivate *priv;
	CORBA_Environment ev;
	GList *p;

	g_return_val_if_fail (evolution_storage != NULL,
			      EVOLUTION_STORAGE_ERROR_INVALIDPARAMETER);
	g_return_val_if_fail (EVOLUTION_IS_STORAGE (evolution_storage),
			      EVOLUTION_STORAGE_ERROR_INVALIDPARAMETER);
	g_return_val_if_fail (path != NULL, EVOLUTION_STORAGE_ERROR_INVALIDPARAMETER);
	g_return_val_if_fail (g_path_is_absolute (path), EVOLUTION_STORAGE_ERROR_INVALIDPARAMETER);

	priv = evolution_storage->priv;

	if (priv->corba_storage_listeners == NULL)
		return EVOLUTION_STORAGE_ERROR_NOTREGISTERED;

	CORBA_exception_init (&ev);

	result = EVOLUTION_STORAGE_OK;

	for (p = priv->corba_storage_listeners; p != NULL; p = p->next) {
		Evolution_StorageListener listener;

		listener = p->data;
		Evolution_StorageListener_removed_folder (listener, path, &ev);

		if (ev._major != CORBA_NO_EXCEPTION)
			continue;

		if (ev._major != CORBA_USER_EXCEPTION)
			result = EVOLUTION_STORAGE_ERROR_CORBA;
		else if (strcmp (CORBA_exception_id (&ev), ex_Evolution_StorageListener_NotFound) == 0)
			result = EVOLUTION_STORAGE_ERROR_NOTFOUND;
		else
			result = EVOLUTION_STORAGE_ERROR_GENERIC;

		break;
	}

	CORBA_exception_free (&ev);

	if (result == EVOLUTION_STORAGE_OK) {
		if (! e_folder_tree_remove (priv->folder_tree, path))
			result = EVOLUTION_STORAGE_ERROR_NOTFOUND;
	}

	return result;
}


E_MAKE_TYPE (evolution_storage, "EvolutionStorage", EvolutionStorage, class_init, init, PARENT_TYPE)
