/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* evolution-storage.c
 *
 * Copyright (C) 2000, 2001 Ximian, Inc.
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

#include <gtk/gtksignal.h>
#include <bonobo/bonobo-object.h>

#include <gal/util/e-util.h>

#include "Evolution.h"

#include "e-util/e-corba-utils.h"

#include "e-folder-tree.h"

#include "evolution-storage.h"


#define PARENT_TYPE BONOBO_OBJECT_TYPE
static BonoboObjectClass *parent_class = NULL;

struct _EvolutionStoragePrivate {
	/* Name of the storage.  */
	char *name;

	/* URI for the toplevel node of the storage.  */
	char *toplevel_node_uri;

	/* Type for the toplevel node of the storage.  */
	char *toplevel_node_type;

	/* The set of folders we have in this storage.  */
	EFolderTree *folder_tree;

	/* Mappings from URIs to folder tree paths.  */
	GHashTable *uri_to_path;

	/* The listener registered on this storage.  */
	GList *corba_storage_listeners;
};


enum {
	CREATE_FOLDER,
	REMOVE_FOLDER,
	LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = { 0 };


/* Utility functions.  */

static void
list_through_listener_foreach (EFolderTree *tree,
			       const char *path,
			       void *data,
			       void *closure)
{
	const GNOME_Evolution_Folder *corba_folder;
	GNOME_Evolution_StorageListener corba_listener;
	CORBA_Environment ev;

	corba_folder = (GNOME_Evolution_Folder *) data;
	corba_listener = (GNOME_Evolution_StorageListener) closure;

	/* The root folder has no data.  */
	if (corba_folder == NULL)
		return;
	
	CORBA_exception_init (&ev);
	GNOME_Evolution_StorageListener_notifyFolderCreated (corba_listener, path, corba_folder, &ev);
	CORBA_exception_free (&ev);
}

static void
list_through_listener (EvolutionStorage *storage,
		       GNOME_Evolution_StorageListener listener,
		       CORBA_Environment *ev)
{
	EvolutionStoragePrivate *priv;

	priv = storage->priv;

	e_folder_tree_foreach (priv->folder_tree,
			       list_through_listener_foreach,
			       listener);
}

static GList *
find_listener_in_list (const GNOME_Evolution_StorageListener listener,
		       GList *list)
{
	CORBA_Environment ev;
	GList *p;

	CORBA_exception_init (&ev);

	for (p = list; p != NULL; p = p->next) {
		GNOME_Evolution_StorageListener listener_item;

		listener_item = (GNOME_Evolution_StorageListener) p->data;

		if (CORBA_Object_is_equivalent (listener_item, listener, &ev) && ev._major == CORBA_NO_EXCEPTION)
			return p;
	}

	CORBA_exception_free (&ev);

	return NULL;
}

static gboolean
add_listener (EvolutionStorage *storage,
	      const GNOME_Evolution_StorageListener listener)
{
	EvolutionStoragePrivate *priv;
	GNOME_Evolution_StorageListener listener_copy;
	CORBA_Environment ev;

	priv = storage->priv;

	if (find_listener_in_list (listener, priv->corba_storage_listeners) != NULL)
		return FALSE;

	CORBA_exception_init (&ev);

	listener_copy = CORBA_Object_duplicate (listener, &ev);
	if (ev._major != CORBA_NO_EXCEPTION) {
		/* Panic.  */
		g_warning ("EvolutionStorage -- Cannot duplicate listener.");
		CORBA_exception_free (&ev);

		/* FIXME this will cause the ::add_listener implementation to
                   incorrectly raise `AlreadyListening' */
		return FALSE;
	}

	priv->corba_storage_listeners = g_list_prepend (priv->corba_storage_listeners,
							listener_copy);

	list_through_listener (storage, listener_copy, &ev);

	CORBA_exception_free (&ev);

	return TRUE;
}

static gboolean
remove_listener (EvolutionStorage *storage,
		 const GNOME_Evolution_StorageListener listener)
{
	EvolutionStoragePrivate *priv;
	CORBA_Environment ev;
	GList *p;

	priv = storage->priv;

	p = find_listener_in_list (listener, priv->corba_storage_listeners);
	if (p == NULL)
		return FALSE;

	CORBA_exception_init (&ev);
	CORBA_Object_release ((CORBA_Object) p->data, &ev);
	CORBA_exception_free (&ev);

	priv->corba_storage_listeners = g_list_remove_link (priv->corba_storage_listeners, p);

	return TRUE;
}


/* Functions for the EFolderTree in the storage.  */

static void
folder_destroy_notify (EFolderTree *tree,
		       const char *path,
		       void *data,
		       void *closure)
{
	GNOME_Evolution_Folder *corba_folder;

	corba_folder = (GNOME_Evolution_Folder *) data;
	CORBA_free (data);
}


/* CORBA interface implementation.  */

static POA_GNOME_Evolution_Storage__vepv Storage_vepv;

static POA_GNOME_Evolution_Storage *
create_servant (void)
{
	POA_GNOME_Evolution_Storage *servant;
	CORBA_Environment ev;

	servant = (POA_GNOME_Evolution_Storage *) g_new0 (BonoboObjectServant, 1);
	servant->vepv = &Storage_vepv;

	CORBA_exception_init (&ev);

	POA_GNOME_Evolution_Storage__init ((PortableServer_Servant) servant, &ev);
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

static GNOME_Evolution_Storage_Result
storage_gtk_to_corba_result (EvolutionStorageResult result)
{
	switch (result) {
	case EVOLUTION_STORAGE_OK:
		return GNOME_Evolution_Storage_OK;
	case EVOLUTION_STORAGE_ERROR_UNSUPPORTED_OPERATION:
		return GNOME_Evolution_Storage_UNSUPPORTED_OPERATION;
	case EVOLUTION_STORAGE_ERROR_UNSUPPORTED_TYPE:
		return GNOME_Evolution_Storage_UNSUPPORTED_TYPE;
	case EVOLUTION_STORAGE_ERROR_INVALID_URI:
		return GNOME_Evolution_Storage_INVALID_URI;
	case EVOLUTION_STORAGE_ERROR_ALREADY_EXISTS:
		return GNOME_Evolution_Storage_ALREADY_EXISTS;
	case EVOLUTION_STORAGE_ERROR_DOES_NOT_EXIST:
		return GNOME_Evolution_Storage_DOES_NOT_EXIST;
	case EVOLUTION_STORAGE_ERROR_PERMISSION_DENIED:
		return GNOME_Evolution_Storage_PERMISSION_DENIED;
	case EVOLUTION_STORAGE_ERROR_NO_SPACE:
		return GNOME_Evolution_Storage_NO_SPACE;
	case EVOLUTION_STORAGE_ERROR_NOT_EMPTY:
		return GNOME_Evolution_Storage_NOT_EMPTY;
	default:
		return GNOME_Evolution_Storage_GENERIC_ERROR;
	}
}

static void
impl_Storage_async_create_folder (PortableServer_Servant servant,
				  const CORBA_char *path,
				  const CORBA_char *type,
				  const CORBA_char *description,
				  const CORBA_char *parent_physical_uri,
				  const Bonobo_Listener listener,
				  CORBA_Environment *ev)
{
	BonoboObject *bonobo_object;
	EvolutionStorage *storage;

	bonobo_object = bonobo_object_from_servant (servant);
	storage = EVOLUTION_STORAGE (bonobo_object);

	gtk_signal_emit (GTK_OBJECT (storage), signals[CREATE_FOLDER],
			 listener, path, type, description, parent_physical_uri);
}

static void
impl_Storage_async_remove_folder (PortableServer_Servant servant,
				  const CORBA_char *path,
				  const CORBA_char *physical_uri,
				  const Bonobo_Listener listener,
				  CORBA_Environment *ev)
{
	BonoboObject *bonobo_object;
	EvolutionStorage *storage;
	int int_result;
	CORBA_any any;
	GNOME_Evolution_Storage_Result corba_result;

	bonobo_object = bonobo_object_from_servant (servant);
	storage = EVOLUTION_STORAGE (bonobo_object);

	int_result = GNOME_Evolution_Storage_UNSUPPORTED_OPERATION;
	gtk_signal_emit (GTK_OBJECT (storage), signals[REMOVE_FOLDER],
			 path, physical_uri, &int_result);

	corba_result = storage_gtk_to_corba_result (int_result);
	any._type = TC_GNOME_Evolution_Storage_Result;
	any._value = &corba_result;

	Bonobo_Listener_event (listener, "result", &any, ev);
}

static void
impl_Storage_async_xfer_folder (PortableServer_Servant servant,
				const CORBA_char *source_path,
				const CORBA_char *destination_path,
				const CORBA_boolean remove_source,
				const Bonobo_Listener listener,
				CORBA_Environment *ev)
{
	g_print ("FIXME: impl_Storage_async_xfer_folder -- implement me!\n");
}

static void
impl_Storage_add_listener (PortableServer_Servant servant,
			   const GNOME_Evolution_StorageListener listener,
			   CORBA_Environment *ev)
{
	BonoboObject *bonobo_object;
	EvolutionStorage *storage;

	bonobo_object = bonobo_object_from_servant (servant);
	storage = EVOLUTION_STORAGE (bonobo_object);

	if (! add_listener (storage, listener))
		CORBA_exception_set (ev, CORBA_USER_EXCEPTION, ex_GNOME_Evolution_Storage_AlreadyListening, NULL);
}

static void
impl_Storage_remove_listener (PortableServer_Servant servant,
			      const GNOME_Evolution_StorageListener listener,
			      CORBA_Environment *ev)
{
	BonoboObject *bonobo_object;
	EvolutionStorage *storage;

	bonobo_object = bonobo_object_from_servant (servant);
	storage = EVOLUTION_STORAGE (bonobo_object);

	if (! remove_listener (storage, listener))
		CORBA_exception_set (ev, CORBA_USER_EXCEPTION, ex_GNOME_Evolution_Storage_NotFound, NULL);
}


/* GtkObject methods.  */

static void
free_mapping (gpointer key, gpointer value, gpointer user_data)
{
	g_free (key);
	g_free (value);
}

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
	g_free (priv->toplevel_node_uri);
	g_free (priv->toplevel_node_type);
	if (priv->folder_tree != NULL)
		e_folder_tree_destroy (priv->folder_tree);
	if (priv->uri_to_path != NULL) {
		g_hash_table_foreach (priv->uri_to_path, free_mapping, NULL);
		g_hash_table_destroy (priv->uri_to_path);
	}

	CORBA_exception_init (&ev);

	for (p = priv->corba_storage_listeners; p != NULL; p = p->next) {
		GNOME_Evolution_StorageListener listener;

		listener = p->data;

		GNOME_Evolution_StorageListener_notifyDestroyed (listener, &ev);

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
	POA_GNOME_Evolution_Storage__vepv *vepv;

	PortableServer_ServantBase__epv *base_epv;

	base_epv = g_new0 (PortableServer_ServantBase__epv, 1);
	base_epv->_private    = NULL;
	base_epv->finalize    = NULL;
	base_epv->default_POA = NULL;

	vepv = &Storage_vepv;
	vepv->_base_epv = base_epv;
	vepv->Bonobo_Unknown_epv = bonobo_object_get_epv ();
	vepv->GNOME_Evolution_Storage_epv = evolution_storage_get_epv ();
}

/* The worst signal marshaller in Scotland */
typedef void (*GtkSignal_NONE__POINTER_POINTER_POINTER_POINTER_POINTER) (GtkObject *,
									 gpointer, gpointer, gpointer, gpointer, gpointer,
									 gpointer user_data);

static void
e_marshal_NONE__POINTER_POINTER_POINTER_POINTER_POINTER (GtkObject *object,
							 GtkSignalFunc func,
							 gpointer func_data,
							 GtkArg *args)
{
	GtkSignal_NONE__POINTER_POINTER_POINTER_POINTER_POINTER rfunc;
	
	rfunc = (GtkSignal_NONE__POINTER_POINTER_POINTER_POINTER_POINTER) func;
	(*rfunc) (object,
		  GTK_VALUE_POINTER (args[0]),
		  GTK_VALUE_POINTER (args[1]),
		  GTK_VALUE_POINTER (args[2]),
		  GTK_VALUE_POINTER (args[3]),
		  GTK_VALUE_POINTER (args[4]),
		  func_data);
}

static void
class_init (EvolutionStorageClass *klass)
{
	GtkObjectClass *object_class;

	object_class = GTK_OBJECT_CLASS (klass);
	object_class->destroy = destroy;

	parent_class = gtk_type_class (bonobo_object_get_type ());

	signals[CREATE_FOLDER] = gtk_signal_new ("create_folder",
						 GTK_RUN_LAST,
						 object_class->type,
						 GTK_SIGNAL_OFFSET (EvolutionStorageClass,
								    create_folder),
						 e_marshal_NONE__POINTER_POINTER_POINTER_POINTER_POINTER,
						 GTK_TYPE_INT, 4,
						 GTK_TYPE_STRING,
						 GTK_TYPE_STRING,
						 GTK_TYPE_STRING,
						 GTK_TYPE_STRING);

	signals[REMOVE_FOLDER] = gtk_signal_new ("remove_folder",
						 GTK_RUN_LAST,
						 object_class->type,
						 GTK_SIGNAL_OFFSET (EvolutionStorageClass,
								    remove_folder),
						 e_marshal_INT__POINTER_POINTER,
						 GTK_TYPE_INT, 2,
						 GTK_TYPE_STRING,
						 GTK_TYPE_STRING);

	gtk_object_class_add_signals (object_class, signals, LAST_SIGNAL);

	corba_class_init ();
}

static void
init (EvolutionStorage *storage)
{
	EvolutionStoragePrivate *priv;

	priv = g_new (EvolutionStoragePrivate, 1);
	priv->name                    = NULL;
	priv->toplevel_node_uri       = NULL;
	priv->toplevel_node_type      = NULL;
	priv->folder_tree             = e_folder_tree_new (folder_destroy_notify, storage);
	priv->uri_to_path                = g_hash_table_new (g_str_hash, g_str_equal);
	priv->corba_storage_listeners = NULL;

	storage->priv = priv;
}


POA_GNOME_Evolution_Storage__epv *
evolution_storage_get_epv (void)
{
	POA_GNOME_Evolution_Storage__epv *epv;

	epv = g_new0 (POA_GNOME_Evolution_Storage__epv, 1);
	epv->_get_name         = impl_Storage__get_name;
	epv->asyncCreateFolder = impl_Storage_async_create_folder;
	epv->asyncRemoveFolder = impl_Storage_async_remove_folder;
	epv->asyncXferFolder   = impl_Storage_async_xfer_folder;
	epv->addListener       = impl_Storage_add_listener;
	epv->removeListener    = impl_Storage_remove_listener;

	return epv;
}

void
evolution_storage_construct (EvolutionStorage *storage,
			     GNOME_Evolution_Storage corba_object,
			     const char *name,
			     const char *toplevel_node_uri,
			     const char *toplevel_node_type)
{
	EvolutionStoragePrivate *priv;
	CORBA_Environment ev;

	g_return_if_fail (storage != NULL);
	g_return_if_fail (EVOLUTION_IS_STORAGE (storage));
	g_return_if_fail (corba_object != CORBA_OBJECT_NIL);
	g_return_if_fail (name != NULL);
	g_return_if_fail (name[0] != '\0');

	CORBA_exception_init (&ev);

	bonobo_object_construct (BONOBO_OBJECT (storage), corba_object);

	priv = storage->priv;
	priv->name               = g_strdup (name);
	priv->toplevel_node_uri  = g_strdup (toplevel_node_uri);
	priv->toplevel_node_type = g_strdup (toplevel_node_type);

	CORBA_exception_free (&ev);
}

EvolutionStorage *
evolution_storage_new (const char *name,
		       const char *toplevel_node_uri,
		       const char *toplevel_node_type)
{
	EvolutionStorage *new;
	POA_GNOME_Evolution_Storage *servant;
	GNOME_Evolution_Storage corba_object;

	g_return_val_if_fail (name != NULL, NULL);
	g_return_val_if_fail (name[0] != '\0', NULL);

	servant = create_servant ();
	if (servant == NULL)
		return NULL;

	new = gtk_type_new (evolution_storage_get_type ());

	corba_object = bonobo_object_activate_servant (BONOBO_OBJECT (new), servant);
	evolution_storage_construct (new, corba_object, name, toplevel_node_uri, toplevel_node_type);

	return new;
}

EvolutionStorageResult
evolution_storage_register (EvolutionStorage *evolution_storage,
			    GNOME_Evolution_StorageRegistry corba_storage_registry)
{
	EvolutionStorageResult result;
	GNOME_Evolution_StorageListener corba_storage_listener;
	GNOME_Evolution_Storage corba_storage;
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
	corba_storage_listener = GNOME_Evolution_StorageRegistry_addStorage (corba_storage_registry,
									     corba_storage,
									     priv->name,
									     e_safe_corba_string (priv->toplevel_node_uri),
									     e_safe_corba_string (priv->toplevel_node_type),
									     &ev);

	if (ev._major == CORBA_NO_EXCEPTION) {
		add_listener (evolution_storage, corba_storage_listener);
		result = EVOLUTION_STORAGE_OK;
	} else {
		if (ev._major != CORBA_USER_EXCEPTION)
			result = EVOLUTION_STORAGE_ERROR_CORBA;
		else if (strcmp (CORBA_exception_id (&ev), ex_GNOME_Evolution_StorageRegistry_Exists) == 0)
			result = EVOLUTION_STORAGE_ERROR_EXISTS;
		else
			result = EVOLUTION_STORAGE_ERROR_GENERIC;
	}

	CORBA_exception_free (&ev);

	return result;
}

EvolutionStorageResult
evolution_storage_register_on_shell (EvolutionStorage *evolution_storage,
				     GNOME_Evolution_Shell corba_shell)
{
	GNOME_Evolution_StorageRegistry corba_storage_registry;
	EvolutionStorageResult result;
	CORBA_Environment ev;

	g_return_val_if_fail (evolution_storage != NULL,
			      EVOLUTION_STORAGE_ERROR_INVALIDPARAMETER);
	g_return_val_if_fail (EVOLUTION_IS_STORAGE (evolution_storage),
			      EVOLUTION_STORAGE_ERROR_INVALIDPARAMETER);
	g_return_val_if_fail (corba_shell != CORBA_OBJECT_NIL,
			      EVOLUTION_STORAGE_ERROR_INVALIDPARAMETER);

	CORBA_exception_init (&ev);

	corba_storage_registry = Bonobo_Unknown_queryInterface (corba_shell,
								 "IDL:GNOME/Evolution/StorageRegistry:1.0",
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
			      const char *description,
			      gboolean highlighted)
{
	EvolutionStorageResult   result;
	EvolutionStoragePrivate *priv;
	GNOME_Evolution_Folder  *corba_folder;
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

	corba_folder = GNOME_Evolution_Folder__alloc ();
	corba_folder->display_name = CORBA_string_dup (display_name);
	corba_folder->description  = CORBA_string_dup (description);
	corba_folder->type         = CORBA_string_dup (type);
	corba_folder->physical_uri = CORBA_string_dup (physical_uri);
	corba_folder->highlighted  = highlighted;

	if (! e_folder_tree_add (priv->folder_tree, path, corba_folder)) {
		CORBA_free (corba_folder);
		return EVOLUTION_STORAGE_ERROR_EXISTS;
	}
	g_hash_table_insert (priv->uri_to_path, g_strdup (physical_uri), g_strdup (path));

	result = EVOLUTION_STORAGE_OK;

	for (p = priv->corba_storage_listeners; p != NULL; p = p->next) {
		GNOME_Evolution_StorageListener listener;

		listener = p->data;
		GNOME_Evolution_StorageListener_notifyFolderCreated (listener, path, corba_folder, &ev);

		if (ev._major == CORBA_NO_EXCEPTION)
			continue;

		if (ev._major != CORBA_USER_EXCEPTION)
			result = EVOLUTION_STORAGE_ERROR_CORBA;
		else if (strcmp (CORBA_exception_id (&ev), ex_GNOME_Evolution_StorageListener_Exists) == 0)
			result = EVOLUTION_STORAGE_ERROR_EXISTS;
		else
			result = EVOLUTION_STORAGE_ERROR_GENERIC;

		break;
	}

	CORBA_exception_free (&ev);

	return result;
}

EvolutionStorageResult
evolution_storage_update_folder (EvolutionStorage *evolution_storage,
				 const char *path, const char *display_name,
				 gboolean highlighted)
{
	EvolutionStorageResult result;
	EvolutionStoragePrivate *priv;
	CORBA_Environment ev;
	GList *p;
	GNOME_Evolution_Folder *corba_folder;

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
		GNOME_Evolution_StorageListener listener;

		listener = p->data;
		GNOME_Evolution_StorageListener_notifyFolderUpdated (listener, path, display_name, highlighted, &ev);

		if (ev._major != CORBA_NO_EXCEPTION)
			continue;

		if (ev._major != CORBA_USER_EXCEPTION)
			result = EVOLUTION_STORAGE_ERROR_CORBA;
		else if (strcmp (CORBA_exception_id (&ev), ex_GNOME_Evolution_StorageListener_NotFound) == 0)
			result = EVOLUTION_STORAGE_ERROR_NOTFOUND;
		else
			result = EVOLUTION_STORAGE_ERROR_GENERIC;

		break;
	}

	CORBA_exception_free (&ev);

	if (result == EVOLUTION_STORAGE_OK) {
		corba_folder = e_folder_tree_get_folder (priv->folder_tree, path);
		if (corba_folder != NULL) {
			CORBA_free (corba_folder->display_name);
			corba_folder->display_name = CORBA_string_dup (display_name);
			corba_folder->highlighted = highlighted;
		} else
			result = EVOLUTION_STORAGE_ERROR_NOTFOUND;
	}

	return result;
}

EvolutionStorageResult
evolution_storage_update_folder_by_uri (EvolutionStorage *evolution_storage,
					const char *physical_uri,
					const char *display_name,
					gboolean highlighted)
{
	EvolutionStoragePrivate *priv;
	char *path;

	g_return_val_if_fail (evolution_storage != NULL,
			      EVOLUTION_STORAGE_ERROR_INVALIDPARAMETER);
	g_return_val_if_fail (EVOLUTION_IS_STORAGE (evolution_storage),
			      EVOLUTION_STORAGE_ERROR_INVALIDPARAMETER);
	g_return_val_if_fail (physical_uri != NULL, EVOLUTION_STORAGE_ERROR_INVALIDPARAMETER);

	priv = evolution_storage->priv;

	path = g_hash_table_lookup (priv->uri_to_path, physical_uri);
	return evolution_storage_update_folder (evolution_storage, path, display_name, highlighted);
}

EvolutionStorageResult
evolution_storage_removed_folder (EvolutionStorage *evolution_storage,
				  const char *path)
{
	EvolutionStorageResult result;
	EvolutionStoragePrivate *priv;
	CORBA_Environment ev;
	GNOME_Evolution_Folder *corba_folder;
	gpointer key, value;
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

	corba_folder = e_folder_tree_get_folder (priv->folder_tree, path);
	if (corba_folder == NULL)
		return EVOLUTION_STORAGE_ERROR_NOTFOUND;
	if (g_hash_table_lookup_extended (priv->uri_to_path, corba_folder->physical_uri, &key, &value)) {
		g_hash_table_remove (priv->uri_to_path, key);
		g_free (key);
		g_free (value);
	}
	e_folder_tree_remove (priv->folder_tree, path);

	CORBA_exception_init (&ev);

	result = EVOLUTION_STORAGE_OK;

	for (p = priv->corba_storage_listeners; p != NULL; p = p->next) {
		GNOME_Evolution_StorageListener listener;

		listener = p->data;
		GNOME_Evolution_StorageListener_notifyFolderRemoved (listener, path, &ev);

		if (ev._major != CORBA_NO_EXCEPTION)
			continue;

		if (ev._major != CORBA_USER_EXCEPTION)
			result = EVOLUTION_STORAGE_ERROR_CORBA;
		else if (strcmp (CORBA_exception_id (&ev), ex_GNOME_Evolution_StorageListener_NotFound) == 0)
			result = EVOLUTION_STORAGE_ERROR_NOTFOUND;
		else
			result = EVOLUTION_STORAGE_ERROR_GENERIC;

		break;
	}

	CORBA_exception_free (&ev);

	return result;
}

gboolean
evolution_storage_folder_exists (EvolutionStorage *evolution_storage,
				 const char *path)
{
	EvolutionStoragePrivate *priv;

	g_return_val_if_fail (EVOLUTION_IS_STORAGE (evolution_storage),
			      EVOLUTION_STORAGE_ERROR_INVALIDPARAMETER);
	g_return_val_if_fail (path != NULL, EVOLUTION_STORAGE_ERROR_INVALIDPARAMETER);
	g_return_val_if_fail (g_path_is_absolute (path), EVOLUTION_STORAGE_ERROR_INVALIDPARAMETER);

	priv = evolution_storage->priv;

	return e_folder_tree_get_folder (priv->folder_tree, path) != NULL;
}


E_MAKE_TYPE (evolution_storage, "EvolutionStorage", EvolutionStorage, class_init, init, PARENT_TYPE)
