/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* e-corba-storage.c
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

#include <bonobo/bonobo-main.h>
#include <gal/util/e-util.h>

#include "Evolution.h"

#include "e-corba-storage.h"


#define PARENT_TYPE E_TYPE_STORAGE
static EStorageClass *parent_class = NULL;

typedef struct _StorageListenerServant StorageListenerServant;

struct _ECorbaStoragePrivate {
	char *name;

	GNOME_Evolution_Storage storage_interface;

	/* The Evolution::StorageListener interface we expose.  */

	GNOME_Evolution_StorageListener storage_listener_interface;
	StorageListenerServant *storage_listener_servant;
};


/* Implementation of the CORBA Evolution::StorageListener interface.  */

static POA_GNOME_Evolution_StorageListener__vepv storage_listener_vepv;

struct _StorageListenerServant {
	POA_GNOME_Evolution_StorageListener servant;
	EStorage *storage;
};

static StorageListenerServant *
storage_listener_servant_new (ECorbaStorage *corba_storage)
{
	StorageListenerServant *servant;

	servant = g_new0 (StorageListenerServant, 1);

	servant->servant.vepv = &storage_listener_vepv;

	gtk_object_ref (GTK_OBJECT (corba_storage));
	servant->storage = E_STORAGE (corba_storage);

	return servant;
}

static void
storage_listener_servant_free (StorageListenerServant *servant)
{
	gtk_object_unref (GTK_OBJECT (servant->storage));

	g_free (servant);
}

#if 0
static void
impl_StorageListener_destroy (PortableServer_Servant servant,
			      CORBA_Environment *ev)
{
	/* FIXME */
}
#endif

static void
impl_StorageListener_new_folder (PortableServer_Servant servant,
				 const CORBA_char *path,
				 const GNOME_Evolution_Folder *folder,
				 CORBA_Environment *ev)
{
	StorageListenerServant *storage_listener_servant;
	EStorage *storage;
	EFolder *e_folder;

	storage_listener_servant = (StorageListenerServant *) servant;
	storage = storage_listener_servant->storage;

	e_folder = e_folder_new (folder->display_name,
				 folder->type,
				 folder->description);

	e_folder_set_physical_uri (e_folder, folder->physical_uri);
	e_folder_set_highlighted (e_folder, folder->highlighted);

	if (! e_storage_new_folder (storage, path, e_folder)) {
		g_print ("Cannot register folder -- %s %s\n", path, folder->display_name);
		CORBA_exception_set (ev,
				     CORBA_USER_EXCEPTION,
				     ex_GNOME_Evolution_StorageListener_Exists,
				     NULL);
		gtk_object_unref (GTK_OBJECT (e_folder));
		return;
	}

	g_print ("Folder registered successfully -- %s %s\n", path, folder->display_name);
}

static void
impl_StorageListener_update_folder (PortableServer_Servant servant,
				    const CORBA_char *path,
				    const CORBA_char *display_name,
				    CORBA_boolean highlighted,
				    CORBA_Environment *ev)
{
	StorageListenerServant *storage_listener_servant;
	EStorage *storage;
	EFolder *e_folder;

	storage_listener_servant = (StorageListenerServant *) servant;
	storage = storage_listener_servant->storage;

	e_folder = e_storage_get_folder (storage, path);
	if (e_folder == NULL) {
		CORBA_exception_set (ev,
				     CORBA_USER_EXCEPTION,
				     ex_GNOME_Evolution_StorageListener_NotFound,
				     NULL);
		return;
	}

	e_folder_set_name (e_folder, display_name);
	e_folder_set_highlighted (e_folder, highlighted);
}

static void
impl_StorageListener_removed_folder (PortableServer_Servant servant,
				     const CORBA_char *path,
				     CORBA_Environment *ev)
{
	StorageListenerServant *storage_listener_servant;
	EStorage *storage;

	storage_listener_servant = (StorageListenerServant *) servant;
	storage = storage_listener_servant->storage;

	if (! e_storage_removed_folder (storage, path))
		CORBA_exception_set (ev,
				     CORBA_USER_EXCEPTION,
				     ex_GNOME_Evolution_StorageListener_NotFound,
				     NULL);
}


static gboolean
setup_storage_listener (ECorbaStorage *corba_storage)
{
	StorageListenerServant *servant;
	ECorbaStoragePrivate *priv;
	GNOME_Evolution_StorageListener storage_listener_interface;
	CORBA_Environment ev;

	priv = corba_storage->priv;

	servant = storage_listener_servant_new (corba_storage);

	CORBA_exception_init (&ev);

	POA_GNOME_Evolution_StorageListener__init (servant, &ev);
	if (ev._major != CORBA_NO_EXCEPTION)
		goto error;

	CORBA_free (PortableServer_POA_activate_object (bonobo_poa (), servant, &ev));
	if (ev._major != CORBA_NO_EXCEPTION)
		goto error;

	storage_listener_interface = PortableServer_POA_servant_to_reference (bonobo_poa (),
									      servant, &ev);
	if (ev._major != CORBA_NO_EXCEPTION)
		goto error;

	priv->storage_listener_interface = storage_listener_interface;
	priv->storage_listener_servant = servant;

	return TRUE;

 error:
	storage_listener_servant_free (servant);
	CORBA_exception_free (&ev);
	return FALSE;
}


/* GtkObject methods.  */

static void
destroy (GtkObject *object)
{
	CORBA_Environment ev;
	ECorbaStorage *corba_storage;
	ECorbaStoragePrivate *priv;

	corba_storage = E_CORBA_STORAGE (object);
	priv = corba_storage->priv;

	g_free (priv->name);

	CORBA_exception_init (&ev);

	if (priv->storage_interface != CORBA_OBJECT_NIL) {
		Bonobo_Unknown_unref (priv->storage_interface, &ev);
		CORBA_Object_release (priv->storage_interface, &ev);
	}

	if (priv->storage_listener_interface != CORBA_OBJECT_NIL)
		CORBA_Object_release (priv->storage_listener_interface, &ev);

	if (priv->storage_listener_servant != NULL) {
		PortableServer_ObjectId *object_id;

		object_id = PortableServer_POA_servant_to_id (bonobo_poa (), priv->storage_listener_servant,
							      &ev);
		PortableServer_POA_deactivate_object (bonobo_poa (), object_id, &ev);

		POA_GNOME_Evolution_StorageListener__fini (priv->storage_listener_servant, &ev);
		CORBA_free (object_id);
	}

	CORBA_exception_free (&ev);

	g_free (priv);

	(* GTK_OBJECT_CLASS (parent_class)->destroy) (object);
}


/* EStorage methods.  */

static const char *
get_name (EStorage *storage)
{
	ECorbaStorage *corba_storage;
	ECorbaStoragePrivate *priv;

	corba_storage = E_CORBA_STORAGE (storage);
	priv = corba_storage->priv;

	return priv->name;
}

struct async_folder_closure {
	EStorageResultCallback callback;
	EStorage *storage;
	void *data;
};

static void
async_folder_cb (BonoboListener *listener, char *event_name, 
		 CORBA_any *any, CORBA_Environment *ev,
		 gpointer user_data)
{
	struct async_folder_closure *closure = user_data;
	GNOME_Evolution_Storage_Result *corba_result;
	EStorageResult result;

	corba_result = any->_value;
	switch (*corba_result) {
	case GNOME_Evolution_Storage_OK:
		result = E_STORAGE_OK;
		break;
	case GNOME_Evolution_Storage_UNSUPPORTED_OPERATION:
		result = E_STORAGE_UNSUPPORTEDOPERATION;
		break;
	case GNOME_Evolution_Storage_UNSUPPORTED_TYPE:
		result = E_STORAGE_UNSUPPORTEDTYPE;
		break;
	case GNOME_Evolution_Storage_ALREADY_EXISTS:
		result = E_STORAGE_EXISTS;
		break;
	case GNOME_Evolution_Storage_DOES_NOT_EXIST:
		result = E_STORAGE_NOTFOUND;
		break;
	case GNOME_Evolution_Storage_PERMISSION_DENIED:
		result = E_STORAGE_PERMISSIONDENIED;
		break;
	case GNOME_Evolution_Storage_NO_SPACE:
		result = E_STORAGE_NOSPACE;
		break;
	case GNOME_Evolution_Storage_INVALID_URI:
	case GNOME_Evolution_Storage_NOT_EMPTY:
	case GNOME_Evolution_Storage_GENERIC_ERROR:
	default:
		result = E_STORAGE_GENERICERROR;
		break;
	}

	closure->callback (closure->storage, result, closure->data);
	bonobo_object_unref (BONOBO_OBJECT (listener));
	g_free (closure);
}

static void
async_create_folder (EStorage *storage, const char *path,
		     const char *type, const char *description,
		     EStorageResultCallback callback, void *data)
{
	ECorbaStorage *corba_storage;
	ECorbaStoragePrivate *priv;
	const char *parent_uri;
	char *p;
	BonoboListener *listener;
	Bonobo_Listener corba_listener;
	CORBA_Environment ev;
	struct async_folder_closure *closure;

	corba_storage = E_CORBA_STORAGE (storage);
	priv = corba_storage->priv;

	p = strrchr (path, '/');
	if (p && p != path) {
		char *parent_path;
		EFolder *parent;

		parent_path = g_strndup (path, p - path);
		parent = e_storage_get_folder (storage, parent_path);
		parent_uri = e_folder_get_physical_uri (parent);
	} else
		parent_uri = "";

	closure = g_new (struct async_folder_closure, 1);
	closure->callback = callback;
	closure->storage = storage;
	closure->data = data;
	listener = bonobo_listener_new (async_folder_cb, closure);
	corba_listener = bonobo_object_corba_objref (BONOBO_OBJECT (listener));

	CORBA_exception_init (&ev);
	GNOME_Evolution_Storage_asyncCreateFolder (priv->storage_interface,
						   path, type, description,
						   parent_uri,
						   corba_listener, &ev);

	if (ev._major != CORBA_NO_EXCEPTION) {
		callback (storage, E_STORAGE_GENERICERROR, data);
		bonobo_object_unref (BONOBO_OBJECT (listener));
		g_free (closure);
	}
	CORBA_exception_free (&ev);
}

static void
async_remove_folder (EStorage *storage, const char *path,
		     EStorageResultCallback callback, void *data)
{
	ECorbaStorage *corba_storage;
	ECorbaStoragePrivate *priv;
	EFolder *folder;
	BonoboListener *listener;
	Bonobo_Listener corba_listener;
	CORBA_Environment ev;
	struct async_folder_closure *closure;

	corba_storage = E_CORBA_STORAGE (storage);
	priv = corba_storage->priv;

	folder = e_storage_get_folder (storage, path);

	closure = g_new (struct async_folder_closure, 1);
	closure->callback = callback;
	closure->storage = storage;
	closure->data = data;
	listener = bonobo_listener_new (async_folder_cb, closure);
	corba_listener = bonobo_object_corba_objref (BONOBO_OBJECT (listener));

	CORBA_exception_init (&ev);
	GNOME_Evolution_Storage_asyncRemoveFolder (priv->storage_interface,
						   path, e_folder_get_physical_uri (folder),
						   corba_listener, &ev);

	if (ev._major != CORBA_NO_EXCEPTION) {
		callback (storage, E_STORAGE_GENERICERROR, data);
		bonobo_object_unref (BONOBO_OBJECT (listener));
		g_free (closure);
	}
	CORBA_exception_free (&ev);
}



static void
corba_class_init (void)
{
	POA_GNOME_Evolution_StorageListener__vepv *vepv;
	POA_GNOME_Evolution_StorageListener__epv *epv;
	PortableServer_ServantBase__epv *base_epv;

	base_epv = g_new0 (PortableServer_ServantBase__epv, 1);
	base_epv->_private    = NULL;
	base_epv->finalize    = NULL;
	base_epv->default_POA = NULL;

	epv = g_new0 (POA_GNOME_Evolution_StorageListener__epv, 1);
	epv->notifyFolderCreated = impl_StorageListener_new_folder;
	epv->notifyFolderUpdated = impl_StorageListener_update_folder;
	epv->notifyFolderRemoved = impl_StorageListener_removed_folder;

	vepv = &storage_listener_vepv;
	vepv->_base_epv                     = base_epv;
	vepv->GNOME_Evolution_StorageListener_epv = epv;
}

static void
class_init (ECorbaStorageClass *klass)
{
	GtkObjectClass *object_class;
	EStorageClass *storage_class;

	object_class = GTK_OBJECT_CLASS (klass);
	object_class->destroy = destroy;

	storage_class = E_STORAGE_CLASS (klass);
	storage_class->get_name = get_name;
	storage_class->async_create_folder = async_create_folder;
	storage_class->async_remove_folder = async_remove_folder;

	corba_class_init ();

	parent_class = gtk_type_class (PARENT_TYPE);
}

static void
init (ECorbaStorage *corba_storage)
{
	ECorbaStoragePrivate *priv;

	priv = g_new (ECorbaStoragePrivate, 1);
	priv->name              = NULL;
	priv->storage_interface = CORBA_OBJECT_NIL;

	corba_storage->priv = priv;
}  


/* FIXME: OK to have a boolean construct function?  */
void
e_corba_storage_construct (ECorbaStorage *corba_storage,
			   const char *toplevel_node_uri,
			   const char *toplevel_node_type,
			   const GNOME_Evolution_Storage storage_interface,
			   const char *name)
{
	ECorbaStoragePrivate *priv;
	CORBA_Environment ev;

	g_return_if_fail (corba_storage != NULL);
	g_return_if_fail (E_IS_CORBA_STORAGE (corba_storage));
	g_return_if_fail (storage_interface != CORBA_OBJECT_NIL);
	g_return_if_fail (name != NULL);

	e_storage_construct (E_STORAGE (corba_storage), toplevel_node_uri, toplevel_node_type);

	priv = corba_storage->priv;

	priv->name = g_strdup (name);

	CORBA_exception_init (&ev);

	Bonobo_Unknown_ref (storage_interface, &ev);
	if (ev._major != CORBA_NO_EXCEPTION)
		g_warning ("e_corba_storage_construct() -- Cannot reference Bonobo object");
	else
		priv->storage_interface = CORBA_Object_duplicate (storage_interface, &ev);

	CORBA_exception_free (&ev);

	setup_storage_listener (corba_storage);
}

EStorage *
e_corba_storage_new (const char *toplevel_node_uri,
		     const char *toplevel_node_type,
		     const GNOME_Evolution_Storage storage_interface,
		     const char *name)
{
	EStorage *new;

	g_return_val_if_fail (storage_interface != CORBA_OBJECT_NIL, NULL);
	g_return_val_if_fail (name != NULL, NULL);

	new = gtk_type_new (e_corba_storage_get_type ());

	e_corba_storage_construct (E_CORBA_STORAGE (new),
				   toplevel_node_uri,
				   toplevel_node_type,
				   storage_interface, name);

	return new;
}


const GNOME_Evolution_StorageListener
e_corba_storage_get_StorageListener (ECorbaStorage *corba_storage)
{
	g_return_val_if_fail (corba_storage != NULL, NULL);
	g_return_val_if_fail (E_IS_CORBA_STORAGE (corba_storage), NULL);

	return corba_storage->priv->storage_listener_interface;
}


E_MAKE_TYPE (e_corba_storage, "ECorbaStorage", ECorbaStorage, class_init, init, PARENT_TYPE)
