/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* e-corba-storage.c
 *
 * Copyright (C) 2000, 2001 Ximian, Inc.
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

#include "e-corba-storage.h"

#include "e-shell-constants.h"

#include "Evolution.h"

#include <glib.h>
#include <gal/util/e-util.h>

#include <bonobo/bonobo-main.h>
#include <bonobo/bonobo-exception.h>
#include <bonobo/bonobo-listener.h>

#include <gdk/gdkx.h>
#include <gtk/gtkmain.h>

#include <string.h>


#define PARENT_TYPE E_TYPE_STORAGE
static EStorageClass *parent_class = NULL;

typedef struct _StorageListenerServant StorageListenerServant;

struct _ECorbaStoragePrivate {
	GNOME_Evolution_Storage storage_interface;

	/* The Evolution::StorageListener interface we expose.  */
	GNOME_Evolution_StorageListener storage_listener_interface;
	StorageListenerServant *storage_listener_servant;

	GList *pending_opens;
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

	g_object_ref (corba_storage);
	servant->storage = E_STORAGE (corba_storage);

	return servant;
}

static void
storage_listener_servant_free (StorageListenerServant *servant)
{
	g_object_unref (servant->storage);

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
impl_StorageListener_notifyFolderCreated (PortableServer_Servant servant,
					  const CORBA_char *path,
					  const GNOME_Evolution_Folder *folder,
					  CORBA_Environment *ev)
{
	StorageListenerServant *storage_listener_servant;
	EStorage *storage;
	EFolder *e_folder;

	storage_listener_servant = (StorageListenerServant *) servant;
	storage = storage_listener_servant->storage;

	e_folder = e_folder_new (folder->displayName, folder->type, folder->description);

	e_folder_set_physical_uri     (e_folder, folder->physicalUri);
	e_folder_set_unread_count     (e_folder, folder->unreadCount);
	e_folder_set_can_sync_offline (e_folder, folder->canSyncOffline);
	e_folder_set_sorting_priority (e_folder, folder->sortingPriority);

	if (folder->customIconName[0] != '\0')
		e_folder_set_custom_icon (e_folder, folder->customIconName);

	if (! e_storage_new_folder (storage, path, e_folder)) {
		g_warning ("Cannot register folder -- %s %s\n", path, folder->displayName);
		CORBA_exception_set (ev,
				     CORBA_USER_EXCEPTION,
				     ex_GNOME_Evolution_StorageListener_Exists,
				     NULL);
		g_object_unref (e_folder);
		return;
	}
}

static void
impl_StorageListener_notifyFolderUpdated (PortableServer_Servant servant,
					  const CORBA_char *path,
					  CORBA_long unread_count,
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

	e_folder_set_unread_count (e_folder, unread_count);
}

static void
impl_StorageListener_notifyFolderRemoved (PortableServer_Servant servant,
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

static void
impl_StorageListener_notifyHasSubfolders (PortableServer_Servant servant,
					  const CORBA_char *path,
					  const CORBA_char *message,
					  CORBA_Environment *ev)
{
	StorageListenerServant *storage_listener_servant;
	EStorage *storage;

	storage_listener_servant = (StorageListenerServant *) servant;
	storage = storage_listener_servant->storage;

	if (! e_storage_declare_has_subfolders (storage, path, message)) {
		g_warning ("Cannot register subfolder tree -- %s\n", path);
		CORBA_exception_set (ev,
				     CORBA_USER_EXCEPTION,
				     ex_GNOME_Evolution_StorageListener_Exists,
				     NULL);
	}
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


/* GObject methods.  */

static void
impl_dispose (GObject *object)
{
	CORBA_Environment ev;
	ECorbaStorage *corba_storage;
	ECorbaStoragePrivate *priv;

	corba_storage = E_CORBA_STORAGE (object);
	priv = corba_storage->priv;

	CORBA_exception_init (&ev);

	if (priv->storage_interface != CORBA_OBJECT_NIL) {
		Bonobo_Unknown_unref (priv->storage_interface, &ev);
		CORBA_Object_release (priv->storage_interface, &ev);
		priv->storage_interface = CORBA_OBJECT_NIL;
	}

	if (priv->storage_listener_interface != CORBA_OBJECT_NIL) {
		CORBA_Object_release (priv->storage_listener_interface, &ev);
		priv->storage_listener_interface = CORBA_OBJECT_NIL;
	}

	if (priv->storage_listener_servant != NULL) {
		PortableServer_ObjectId *object_id;

		object_id = PortableServer_POA_servant_to_id (bonobo_poa (), priv->storage_listener_servant,
							      &ev);
		PortableServer_POA_deactivate_object (bonobo_poa (), object_id, &ev);

		CORBA_free (object_id);

		priv->storage_listener_servant = NULL;
	}

	CORBA_exception_free (&ev);

	if (priv->pending_opens != NULL) {
		g_warning ("destroying ECorbaStorage with pending async ops");
		priv->pending_opens = NULL;
	}

	(* G_OBJECT_CLASS (parent_class)->dispose) (object);
}

static void
impl_finalize (GObject *object)
{
	ECorbaStorage *corba_storage;

	corba_storage = E_CORBA_STORAGE (object);

	g_free (corba_storage->priv);

	(* G_OBJECT_CLASS (parent_class)->finalize) (object);
}


/* EStorage methods.  */

static void
get_folder_cb (EStorage *storage, EStorageResult result,
	       const char *path, gpointer data)
{
	gboolean *done = data;

	*done = TRUE;
}

static EFolder *
get_folder (EStorage *storage, const char *path)
{
	EFolder *folder;
	char *path_dup, *p;

	folder = (* E_STORAGE_CLASS (parent_class)->get_folder) (storage, path);
	if (folder)
		return folder;

	/* If @path points to a part of the storage that hasn't been
	 * opened yet, do that.
	 */
	path_dup = g_strdup (path);
	p = strchr (path_dup + 1, '/');
	while (p) {
		*p = '\0';
		if (e_storage_get_has_subfolders (storage, path_dup)) {
			gboolean done = FALSE;

			e_storage_async_open_folder (storage, path_dup,
						     get_folder_cb, &done);
			while (!done)
				gtk_main_iteration ();
		}
		*p = '/';
		p = strchr (p + 1, '/');
	}

	return (* E_STORAGE_CLASS (parent_class)->get_folder) (storage, path);
}

struct async_folder_closure {
	EStorageResultCallback callback;
	EStorage *storage;
	void *data;
};

static void
async_folder_cb (BonoboListener *listener,
		 const char *event_name, 
		 const CORBA_any *any,
		 CORBA_Environment *ev,
		 gpointer user_data)
{
	struct async_folder_closure *closure = user_data;
	GNOME_Evolution_Storage_Result *corba_result;
	EStorageResult result;

	corba_result = any->_value;
	result = e_corba_storage_corba_result_to_storage_result (*corba_result);

	(* closure->callback) (closure->storage, result, closure->data);
	bonobo_object_unref (BONOBO_OBJECT (listener));

	g_object_unref (closure->storage);
	g_free (closure);
}


struct async_create_open_closure {
	EStorage *storage;
	char *path, *type, *description;
	EStorageResultCallback callback;
	void *data;
};

static void
async_create_open_cb (EStorage *storage, EStorageResult result,
		      const char *path, void *data)
{
	struct async_create_open_closure *closure = data;

	if (result != E_STORAGE_OK) {
		(* closure->callback) (closure->storage, result,
				       closure->data);
	} else {
		e_storage_async_create_folder (closure->storage,
					       closure->path, closure->type,
					       closure->description,
					       closure->callback,
					       closure->data);
	}

	g_object_unref (closure->storage);
	g_free (closure->path);
	g_free (closure->type);
	g_free (closure->description);
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
		EFolder *parent;
		char *parent_path;

		parent_path = g_strndup (path, p - path);
		parent = e_storage_get_folder (storage, parent_path);
		parent_uri = e_folder_get_physical_uri (parent);

		if (e_folder_get_has_subfolders (parent)) {
			struct async_create_open_closure *open_closure;

			/* Force the parent folder to resolve its
			 * children before creating the new folder.
			 */
			open_closure = g_new (struct async_create_open_closure, 1);
			open_closure->storage = storage;
			g_object_ref (storage);
			open_closure->path = g_strdup (path);
			open_closure->type = g_strdup (type);
			open_closure->description = g_strdup (description);
			open_closure->callback = callback;
			open_closure->data = data;
			e_storage_async_open_folder (storage, parent_path,
						     async_create_open_cb,
						     open_closure);
			return;
		}
	} else
		parent_uri = "";

	closure = g_new (struct async_folder_closure, 1);
	closure->callback = callback;
	closure->storage = storage;
	g_object_ref (storage);
	closure->data = data;
	listener = bonobo_listener_new (async_folder_cb, closure);
	corba_listener = bonobo_object_corba_objref (BONOBO_OBJECT (listener));

	CORBA_exception_init (&ev);
	GNOME_Evolution_Storage_asyncCreateFolder (priv->storage_interface,
						   path, type, description,
						   parent_uri,
						   corba_listener, &ev);

	if (ev._major != CORBA_NO_EXCEPTION) {
		(* callback) (storage, E_STORAGE_GENERICERROR, data);
		bonobo_object_unref (BONOBO_OBJECT (listener));
		g_object_unref (storage);
		g_free (closure);
	}
	CORBA_exception_free (&ev);
}

static void
async_remove_folder (EStorage *storage,
		     const char *path,
		     EStorageResultCallback callback,
		     void *data)
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
	if (e_folder_get_is_stock (folder)) {
		(* callback) (storage, E_STORAGE_CANTCHANGESTOCKFOLDER, data);
		return;
	}
	if (e_folder_get_physical_uri (folder) == NULL) {
		(* callback) (storage, E_STORAGE_GENERICERROR, data);
		return;
	}

	closure = g_new (struct async_folder_closure, 1);
	closure->callback = callback;
	closure->storage = storage;
	g_object_ref (storage);
	closure->data = data;
	listener = bonobo_listener_new (async_folder_cb, closure);
	corba_listener = bonobo_object_corba_objref (BONOBO_OBJECT (listener));

	CORBA_exception_init (&ev);
	GNOME_Evolution_Storage_asyncRemoveFolder (priv->storage_interface,
						   path,
						   e_folder_get_physical_uri (folder),
						   corba_listener, &ev);

	if (ev._major != CORBA_NO_EXCEPTION) {
		(* callback) (storage, E_STORAGE_GENERICERROR, data);
		bonobo_object_unref (BONOBO_OBJECT (listener));
		g_object_unref (storage);
		g_free (closure);
	}
	CORBA_exception_free (&ev);
}

static void
async_xfer_folder (EStorage *storage,
		   const char *source_path,
		   const char *destination_path,
		   gboolean remove_source,
		   EStorageResultCallback callback,
		   void *data)
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

	folder = e_storage_get_folder (storage, source_path);
	if (e_folder_get_is_stock (folder) && remove_source)
		(* callback) (storage, E_STORAGE_CANTCHANGESTOCKFOLDER, data);

	closure = g_new (struct async_folder_closure, 1);
	closure->callback = callback;
	closure->storage = storage;
	g_object_ref (storage);
	closure->data = data;
	listener = bonobo_listener_new (async_folder_cb, closure);
	corba_listener = bonobo_object_corba_objref (BONOBO_OBJECT (listener));

	CORBA_exception_init (&ev);
	GNOME_Evolution_Storage_asyncXferFolder (priv->storage_interface,
						 source_path, destination_path,
						 remove_source, corba_listener, &ev);

	if (ev._major != CORBA_NO_EXCEPTION) {
		(* callback) (storage, E_STORAGE_GENERICERROR, data);
		bonobo_object_unref (BONOBO_OBJECT (listener));
		g_object_unref (storage);
		g_free (closure);
	}
	CORBA_exception_free (&ev);
}

struct async_open_closure {
	EStorageDiscoveryCallback callback;
	EStorage *storage;
	void *data;
	char *path;
};

static void
async_open_cb (BonoboListener *listener, const char *event_name, 
	       const CORBA_any *any, CORBA_Environment *ev,
	       gpointer user_data)
{
	struct async_open_closure *orig_closure = user_data, *closure;
	GNOME_Evolution_Storage_Result *corba_result;
	ECorbaStoragePrivate *priv;
	EStorageResult result;
	GList *p;

	corba_result = any->_value;
	result = e_corba_storage_corba_result_to_storage_result (*corba_result);
	bonobo_object_unref (BONOBO_OBJECT (listener));

	priv = E_CORBA_STORAGE (orig_closure->storage)->priv;
	p = priv->pending_opens;
	while (p) {
		closure = p->data;
		if (!strcmp (closure->path, orig_closure->path)) {
			(* closure->callback) (closure->storage, result,
					       closure->path, closure->data);
			if (closure != orig_closure) {
				g_free (closure->path);
				g_free (closure);
			}
			priv->pending_opens = g_list_remove (priv->pending_opens, p->data);
			p = priv->pending_opens;
		} else
			p = p->next;
	}

	g_object_unref (orig_closure->storage);
	g_free (orig_closure->path);
	g_free (orig_closure);
}

static gboolean
async_open_folder_idle (gpointer data)
{
	struct async_open_closure *closure = data, *old_closure;
	EStorage *storage = closure->storage;
	ECorbaStorage *corba_storage;
	ECorbaStoragePrivate *priv;
	BonoboListener *listener;
	Bonobo_Listener corba_listener;
	CORBA_Environment ev;
	GList *p;

	corba_storage = E_CORBA_STORAGE (storage);
	priv = corba_storage->priv;

	for (p = priv->pending_opens; p; p = p->next) {
		old_closure = p->data;
		if (!strcmp (closure->path, old_closure->path)) {
			priv->pending_opens = g_list_prepend (priv->pending_opens, closure);
			return FALSE;
		}
	}

	listener = bonobo_listener_new (async_open_cb, closure);
	corba_listener = bonobo_object_corba_objref (BONOBO_OBJECT (listener));

	CORBA_exception_init (&ev);
	GNOME_Evolution_Storage_asyncOpenFolder (priv->storage_interface,
						 closure->path,
						 corba_listener, &ev);
	if (ev._major != CORBA_NO_EXCEPTION) {
		(* closure->callback) (storage, E_STORAGE_GENERICERROR,
				       closure->path, closure->data);
		bonobo_object_unref (BONOBO_OBJECT (listener));
		g_object_unref (closure->storage);
		g_free (closure->path);
		g_free (closure);
	}
	CORBA_exception_free (&ev);

	priv->pending_opens = g_list_prepend (priv->pending_opens, closure);
	return FALSE;
}

static void
async_open_folder (EStorage *storage,
		   const char *path,
		   EStorageDiscoveryCallback callback,
		   void *data)
{
	struct async_open_closure *closure;

	closure = g_new (struct async_open_closure, 1);
	closure->callback = callback;
	closure->storage = storage;
	g_object_ref (storage);
	closure->data = data;
	closure->path = g_strdup (path);

	g_idle_add (async_open_folder_idle, closure);
}


/* Shared folders.  */

static gboolean
supports_shared_folders (EStorage *storage)
{
	GNOME_Evolution_Storage storage_iface;
	CORBA_boolean has_shared_folders;
	CORBA_Environment ev;

	CORBA_exception_init (&ev);

	storage_iface = e_corba_storage_get_corba_objref (E_CORBA_STORAGE (storage));
	g_assert (storage_iface != CORBA_OBJECT_NIL);

	has_shared_folders = GNOME_Evolution_Storage__get_hasSharedFolders (storage_iface, &ev);
	if (BONOBO_EX (&ev))
		has_shared_folders = FALSE;

	CORBA_exception_free (&ev);

	return has_shared_folders;
}

static void
async_folder_discovery_cb (BonoboListener *listener,
			   const char *event_name, 
			   const CORBA_any *any,
			   CORBA_Environment *ev,
			   gpointer user_data)
{
	struct async_folder_closure *closure = user_data;
	GNOME_Evolution_Storage_FolderResult *corba_result;
	EStorageDiscoveryCallback callback;
	EStorageResult result;
	char *path;

	corba_result = any->_value;
	result = e_corba_storage_corba_result_to_storage_result (corba_result->result);
	if (result == E_STORAGE_OK)
		path = corba_result->path;
	else
		path = NULL;

	callback = (EStorageDiscoveryCallback)closure->callback;
	(* callback) (closure->storage, result, path, closure->data);

	bonobo_object_unref (BONOBO_OBJECT (listener));
	g_object_unref (closure->storage);
	g_free (closure);
}

static void
async_discover_shared_folder (EStorage *storage,
			      const char *owner,
			      const char *folder_name,
			      EStorageDiscoveryCallback callback,
			      void *data)
{
	ECorbaStorage *corba_storage;
	ECorbaStoragePrivate *priv;
	BonoboListener *listener;
	Bonobo_Listener corba_listener;
	CORBA_Environment ev;
	struct async_folder_closure *closure;

	corba_storage = E_CORBA_STORAGE (storage);
	priv = corba_storage->priv;

	closure = g_new (struct async_folder_closure, 1);
	closure->callback = (EStorageResultCallback)callback;
	closure->storage = storage;
	g_object_ref (storage);
	closure->data = data;
	listener = bonobo_listener_new (async_folder_discovery_cb, closure);
	corba_listener = bonobo_object_corba_objref (BONOBO_OBJECT (listener));

	CORBA_exception_init (&ev);
	GNOME_Evolution_Storage_asyncDiscoverSharedFolder (priv->storage_interface,
							   owner, folder_name,
							   corba_listener, &ev);
	if (ev._major != CORBA_NO_EXCEPTION) {
		(* callback) (storage, E_STORAGE_GENERICERROR, NULL, data);
		bonobo_object_unref (BONOBO_OBJECT (listener));
		g_object_unref (storage);
		g_free (closure);
	}
	CORBA_exception_free (&ev);
}

static void
cancel_discover_shared_folder (EStorage *storage,
			       const char *owner,
			       const char *folder_name)
{
	ECorbaStorage *corba_storage;
	ECorbaStoragePrivate *priv;
	CORBA_Environment ev;

	corba_storage = E_CORBA_STORAGE (storage);
	priv = corba_storage->priv;

	CORBA_exception_init (&ev);
	GNOME_Evolution_Storage_cancelDiscoverSharedFolder (priv->storage_interface,
							    owner, folder_name, &ev);
	if (BONOBO_EX (&ev))
		g_warning ("Error invoking cancelDiscoverSharedFolder -- %s", BONOBO_EX_REPOID (&ev));
	CORBA_exception_free (&ev);
}

static void
async_remove_shared_folder (EStorage *storage,
			    const char *path,
			    EStorageResultCallback callback,
			    void *data)
{
	ECorbaStorage *corba_storage;
	ECorbaStoragePrivate *priv;
	BonoboListener *listener;
	Bonobo_Listener corba_listener;
	CORBA_Environment ev;
	struct async_folder_closure *closure;

	corba_storage = E_CORBA_STORAGE (storage);
	priv = corba_storage->priv;

	closure = g_new (struct async_folder_closure, 1);
	closure->callback = callback;
	closure->storage = storage;
	g_object_ref (storage);
	closure->data = data;
	listener = bonobo_listener_new (async_folder_cb, closure);
	corba_listener = bonobo_object_corba_objref (BONOBO_OBJECT (listener));

	CORBA_exception_init (&ev);
	GNOME_Evolution_Storage_asyncRemoveSharedFolder (priv->storage_interface,
							 path, corba_listener,
							 &ev);
	if (ev._major != CORBA_NO_EXCEPTION) {
		(* callback) (storage, E_STORAGE_GENERICERROR, data);
		bonobo_object_unref (BONOBO_OBJECT (listener));
		g_object_unref (storage);
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
	epv->notifyFolderCreated = impl_StorageListener_notifyFolderCreated;
	epv->notifyFolderUpdated = impl_StorageListener_notifyFolderUpdated;
	epv->notifyFolderRemoved = impl_StorageListener_notifyFolderRemoved;
	epv->notifyHasSubfolders = impl_StorageListener_notifyHasSubfolders;

	vepv = &storage_listener_vepv;
	vepv->_base_epv                     = base_epv;
	vepv->GNOME_Evolution_StorageListener_epv = epv;
}

static void
class_init (ECorbaStorageClass *klass)
{
	GObjectClass *object_class;
	EStorageClass *storage_class;

	object_class = G_OBJECT_CLASS (klass);
	object_class->dispose  = impl_dispose;
	object_class->finalize = impl_finalize;

	storage_class = E_STORAGE_CLASS (klass);
	storage_class->get_folder                    = get_folder;
	storage_class->async_create_folder           = async_create_folder;
	storage_class->async_remove_folder           = async_remove_folder;
	storage_class->async_xfer_folder             = async_xfer_folder;
	storage_class->async_open_folder             = async_open_folder;
	storage_class->supports_shared_folders       = supports_shared_folders;
	storage_class->async_discover_shared_folder  = async_discover_shared_folder;
	storage_class->cancel_discover_shared_folder = cancel_discover_shared_folder;
	storage_class->async_remove_shared_folder    = async_remove_shared_folder;

	corba_class_init ();

	parent_class = g_type_class_ref(PARENT_TYPE);
}

static void
init (ECorbaStorage *corba_storage)
{
	ECorbaStoragePrivate *priv;

	priv = g_new (ECorbaStoragePrivate, 1);
	priv->storage_interface = CORBA_OBJECT_NIL;
	priv->pending_opens = NULL;

	corba_storage->priv = priv;
}  


/* FIXME: OK to have a boolean construct function?  */
void
e_corba_storage_construct (ECorbaStorage *corba_storage,
			   const GNOME_Evolution_Storage storage_interface,
			   const char *name)
{
	ECorbaStoragePrivate *priv;
	CORBA_Environment ev;
	EFolder *root_folder;

	g_return_if_fail (corba_storage != NULL);
	g_return_if_fail (E_IS_CORBA_STORAGE (corba_storage));
	g_return_if_fail (storage_interface != CORBA_OBJECT_NIL);
	g_return_if_fail (name != NULL);

	/* FIXME: Need separate name and display name. */
	root_folder = e_folder_new (name, "noselect", "");
	e_storage_construct (E_STORAGE (corba_storage), name, root_folder);

	priv = corba_storage->priv;

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
e_corba_storage_new (const GNOME_Evolution_Storage storage_interface,
		     const char *name)
{
	EStorage *new;

	g_return_val_if_fail (storage_interface != CORBA_OBJECT_NIL, NULL);
	g_return_val_if_fail (name != NULL, NULL);

	new = g_object_new (e_corba_storage_get_type (), NULL);

	e_corba_storage_construct (E_CORBA_STORAGE (new),
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

GNOME_Evolution_Storage
e_corba_storage_get_corba_objref (ECorbaStorage *corba_storage)
{
	g_return_val_if_fail (corba_storage != NULL, CORBA_OBJECT_NIL);
	g_return_val_if_fail (E_IS_CORBA_STORAGE (corba_storage), CORBA_OBJECT_NIL);

	return corba_storage->priv->storage_interface;
}


GSList *
e_corba_storage_get_folder_property_items (ECorbaStorage *corba_storage)
{
	GNOME_Evolution_Storage_FolderPropertyItemList *corba_items;
	CORBA_Environment ev;
	GSList *list;
	int i;

	g_return_val_if_fail (E_IS_CORBA_STORAGE (corba_storage), NULL);

	CORBA_exception_init (&ev);

	corba_items = GNOME_Evolution_Storage__get_folderPropertyItems (corba_storage->priv->storage_interface,
									&ev);

	if (BONOBO_EX (&ev)) {
		CORBA_exception_free (&ev);
		return NULL;
	}

	list = NULL;
	for (i = 0; i < corba_items->_length; i ++) {
		ECorbaStoragePropertyItem *item;

		item = g_new (ECorbaStoragePropertyItem, 1);
		item->label   = g_strdup (corba_items->_buffer[i].label);
		item->tooltip = g_strdup (corba_items->_buffer[i].tooltip);
		item->icon    = NULL; /* We don't care for now -- FIXME */

		list = g_slist_prepend (list, item);
	}
	list = g_slist_reverse (list);

	CORBA_free (corba_items);
	CORBA_exception_free (&ev);

	return list;
}

void
e_corba_storage_free_property_items_list (GSList *list)
{
	GSList *p;

	for (p = list; p != NULL; p = p->next) {
		ECorbaStoragePropertyItem *item;

		item = (ECorbaStoragePropertyItem *) p->data;

		if (item->icon != NULL)
			g_object_unref (item->icon);
		g_free (item->label);
		g_free (item->tooltip);
		g_free (item);
	}

	g_slist_free (list);
}

void
e_corba_storage_show_folder_properties (ECorbaStorage *corba_storage,
					const char *path,
					int property_item_id,
					GdkWindow *parent_window)
{
	CORBA_Environment ev;

	g_return_if_fail (E_IS_CORBA_STORAGE (corba_storage));
	g_return_if_fail (path != NULL && path[0] == E_PATH_SEPARATOR);

	CORBA_exception_init (&ev);

	GNOME_Evolution_Storage_showFolderProperties (corba_storage->priv->storage_interface,
						      path, property_item_id,
						      GDK_WINDOW_XWINDOW (parent_window),
						      &ev);
	if (BONOBO_EX (&ev))
		g_warning ("Error in Storage::showFolderProperties -- %s", BONOBO_EX_REPOID (&ev));

	CORBA_exception_free (&ev);
}

EStorageResult
e_corba_storage_corba_result_to_storage_result (GNOME_Evolution_Storage_Result corba_result)
{
	switch (corba_result) {
	case GNOME_Evolution_Storage_OK:
		return E_STORAGE_OK;
	case GNOME_Evolution_Storage_UNSUPPORTED_OPERATION:
		return E_STORAGE_UNSUPPORTEDOPERATION;
	case GNOME_Evolution_Storage_UNSUPPORTED_TYPE:
		return E_STORAGE_UNSUPPORTEDTYPE;
	case GNOME_Evolution_Storage_INVALID_URI:
		return E_STORAGE_INVALIDNAME;
	case GNOME_Evolution_Storage_ALREADY_EXISTS:
		return E_STORAGE_EXISTS;
	case GNOME_Evolution_Storage_DOES_NOT_EXIST:
		return E_STORAGE_NOTFOUND;
	case GNOME_Evolution_Storage_PERMISSION_DENIED:
		return E_STORAGE_PERMISSIONDENIED;
	case GNOME_Evolution_Storage_NO_SPACE:
		return E_STORAGE_NOSPACE;
	case GNOME_Evolution_Storage_NOT_EMPTY:
		return E_STORAGE_NOTEMPTY;
	case GNOME_Evolution_Storage_NOT_ONLINE:
		return E_STORAGE_NOTONLINE;
	case GNOME_Evolution_Storage_GENERIC_ERROR:
	default:
		return E_STORAGE_GENERICERROR;
	}
}


E_MAKE_TYPE (e_corba_storage, "ECorbaStorage", ECorbaStorage, class_init, init, PARENT_TYPE)
