/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* e-corba-storage-registry.c
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

#include "e-local-storage.h"
#include "e-corba-storage.h"
#include "e-corba-storage-registry.h"
#include "e-shell-constants.h"

#include "e-util/e-corba-utils.h"

#include <bonobo/bonobo-exception.h>
#include <gal/util/e-util.h>

#include <string.h>


#define PARENT_TYPE BONOBO_OBJECT_TYPE
static BonoboObjectClass *parent_class = NULL;

struct _ECorbaStorageRegistryPrivate {
	EStorageSet *storage_set;

	GSList *listeners;
};


/* CORBA interface implementation.  */

static void
listener_notify (Bonobo_Listener listener,
		 GNOME_Evolution_StorageRegistry_MessageType type,
		 const char *name)
{
	CORBA_any any;
	GNOME_Evolution_StorageRegistry_NotifyResult nr;
	CORBA_Environment ev;
	
	nr.type = type;
	nr.name = CORBA_string_dup (name ? name : "");

	any._type = (CORBA_TypeCode) TC_GNOME_Evolution_StorageRegistry_NotifyResult;
	any._value = &nr;

	CORBA_exception_init (&ev);
	Bonobo_Listener_event (listener,
			       "Evolution::StorageRegistry::NotifyResult",
			       &any, &ev);
	if (BONOBO_EX (&ev)) {
		g_warning ("Could not send notify event for %s\n%s", name,
			   CORBA_exception_id (&ev));
	}

	CORBA_exception_free (&ev);
        CORBA_free (nr.name);
}

static GNOME_Evolution_StorageListener
impl_StorageRegistry_addStorage (PortableServer_Servant servant,
				 const GNOME_Evolution_Storage storage_interface,
				 const CORBA_char *name,
				 CORBA_Environment *ev)
{
	BonoboObject *bonobo_object;
	ECorbaStorageRegistry *storage_registry;
	ECorbaStorageRegistryPrivate *priv;
	EStorage *storage;
	GNOME_Evolution_StorageListener listener_interface;
	GSList *iter;
	
	bonobo_object = bonobo_object_from_servant (servant);
	storage_registry = E_CORBA_STORAGE_REGISTRY (bonobo_object);
	priv = storage_registry->priv;

	storage = e_corba_storage_new (storage_interface, name);

	if (! e_storage_set_add_storage (priv->storage_set, storage)) {
		CORBA_exception_set (ev,
				     CORBA_USER_EXCEPTION,
				     ex_GNOME_Evolution_StorageRegistry_Exists,
				     NULL);
		return CORBA_OBJECT_NIL;
	}

	g_object_unref (storage);


       /* FIXME: if we remove a listener while looping through the list we can
        * crash. Yay CORBA reentrancy. */
	for (iter = priv->listeners; iter; iter = iter->next) {
		listener_notify (iter->data,
				 GNOME_Evolution_StorageRegistry_STORAGE_CREATED,
				 name);
	}
	
	listener_interface = CORBA_Object_duplicate (e_corba_storage_get_StorageListener
						     (E_CORBA_STORAGE (storage)), ev);

	return listener_interface;
}

static GNOME_Evolution_StorageRegistry_StorageList *
impl_StorageRegistry_getStorageList (PortableServer_Servant servant,
				     CORBA_Environment *ev)
{
	BonoboObject *bonobo_object;
	ECorbaStorageRegistry *storage_registry;
	ECorbaStorageRegistryPrivate *priv;
	GNOME_Evolution_StorageRegistry_StorageList *storage_list;
	GList *sl, *l;
	
	bonobo_object = bonobo_object_from_servant (servant);
	storage_registry = E_CORBA_STORAGE_REGISTRY (bonobo_object);
	priv = storage_registry->priv;

	sl = e_storage_set_get_storage_list (priv->storage_set);
	
	storage_list = GNOME_Evolution_StorageRegistry_StorageList__alloc ();
	storage_list->_maximum = g_list_length (sl);
	storage_list->_length = 0;
	storage_list->_buffer = CORBA_sequence_GNOME_Evolution_Storage_allocbuf (storage_list->_maximum);
	for (l = sl; l != NULL; l = l->next) {
		EStorage *storage;
		GNOME_Evolution_Storage corba_storage;
		CORBA_Environment ev2;
		
		CORBA_exception_init (&ev2);
		
		storage = l->data;
		if (E_IS_LOCAL_STORAGE (storage)) {
			corba_storage = e_local_storage_get_corba_interface (E_LOCAL_STORAGE (storage));
		} else if (E_IS_CORBA_STORAGE (storage)) {
			corba_storage = e_corba_storage_get_corba_objref (E_CORBA_STORAGE (storage));
		} else {
			continue;			
		}
		
		corba_storage = CORBA_Object_duplicate (corba_storage, &ev2);
		if (BONOBO_EX (&ev2)) {
			CORBA_exception_free (&ev2);			
			continue;
		}		
		storage_list->_buffer[storage_list->_length] = corba_storage;
		storage_list->_length++;		
	}

	CORBA_sequence_set_release (storage_list, TRUE);

	return storage_list;	
}

static GNOME_Evolution_Storage
impl_StorageRegistry_getStorageByName (PortableServer_Servant servant,
				       const CORBA_char *name,
				       CORBA_Environment *ev)
{
	BonoboObject *bonobo_object;
	ECorbaStorageRegistry *storage_registry;
	ECorbaStorageRegistryPrivate *priv;
	GNOME_Evolution_Storage corba_storage;
	EStorage *storage;

	bonobo_object = bonobo_object_from_servant (servant);
	storage_registry = E_CORBA_STORAGE_REGISTRY (bonobo_object);
	priv = storage_registry->priv;

	storage = e_storage_set_get_storage (priv->storage_set, name);
	if (storage == NULL) {
		CORBA_exception_set (ev, CORBA_USER_EXCEPTION,
				     ex_GNOME_Evolution_StorageRegistry_NotFound,
				     NULL);
		return CORBA_OBJECT_NIL;
	}

	corba_storage = e_corba_storage_get_corba_objref (E_CORBA_STORAGE (storage));

	return bonobo_object_dup_ref (corba_storage, NULL);
}

static void
impl_StorageRegistry_removeStorageByName (PortableServer_Servant servant,
					  const CORBA_char *name,
					  CORBA_Environment *ev)
{
	BonoboObject *bonobo_object;
	ECorbaStorageRegistry *storage_registry;
	ECorbaStorageRegistryPrivate *priv;
	EStorage *storage;

	bonobo_object = bonobo_object_from_servant (servant);
	storage_registry = E_CORBA_STORAGE_REGISTRY (bonobo_object);
	priv = storage_registry->priv;

	storage = e_storage_set_get_storage (priv->storage_set, name);
	if (storage == NULL) {
		CORBA_exception_set (ev, CORBA_USER_EXCEPTION,
				     ex_GNOME_Evolution_StorageRegistry_NotFound,
				     NULL);
		return;
	}

	/* FIXME: Yucky to get the storage by name and then remove it.  */
	/* FIXME: Check failure.  */
	e_storage_set_remove_storage (priv->storage_set, storage);
}

static void
storage_set_foreach (EStorageSet *set,
		     Bonobo_Listener listener)
{
	GList *storage_list, *p;

	storage_list = e_storage_set_get_storage_list (set);
	for (p = storage_list; p; p = p->next) {
		const char *name;

		name = e_storage_get_name (E_STORAGE (p->data));

		listener_notify (listener, GNOME_Evolution_StorageRegistry_STORAGE_CREATED, name);
		g_object_unref (p->data);
	}
	
	g_list_free (storage_list);
}

static GSList *
find_listener (GSList *p,
	       Bonobo_Listener listener)
{
	CORBA_Environment ev;

	CORBA_exception_init (&ev);
	for (; p; p = p->next) {
		if (CORBA_Object_is_equivalent (p->data, listener, &ev) == TRUE) {
			CORBA_exception_free (&ev);
			return p;
		}
	}

	CORBA_exception_free (&ev);
	return NULL;
}

static void
impl_StorageRegistry_addListener (PortableServer_Servant servant,
				  Bonobo_Listener listener,
				  CORBA_Environment *ev)
{
	BonoboObject *bonobo_object;
	ECorbaStorageRegistry *storage_registry;
	ECorbaStorageRegistryPrivate *priv;
	Bonobo_Listener listener_copy;
	CORBA_Environment ev2;
	
	bonobo_object = bonobo_object_from_servant (servant);
	storage_registry = E_CORBA_STORAGE_REGISTRY (bonobo_object);
	priv = storage_registry->priv;

	if (find_listener (priv->listeners, listener) != NULL) {
		CORBA_exception_set (ev, CORBA_USER_EXCEPTION,
				     ex_GNOME_Evolution_StorageRegistry_AlreadyListening,
				     NULL);
		return;
	}

	CORBA_exception_init (&ev2);
	listener_copy = CORBA_Object_duplicate ((CORBA_Object) listener, &ev2);
	if (BONOBO_EX (&ev2)) {
		g_warning ("EvolutionStorageRegistry -- Cannot duplicate listener.");
		CORBA_exception_free (&ev2);
		return;
	}

	CORBA_exception_free (&ev2);
	priv->listeners = g_slist_prepend (priv->listeners, listener_copy);
	storage_set_foreach (priv->storage_set, listener_copy);
}

static void
impl_StorageRegistry_removeListener (PortableServer_Servant servant,
				     Bonobo_Listener listener,
				     CORBA_Environment *ev)
{
	BonoboObject *bonobo_object;
	ECorbaStorageRegistry *storage_registry;
	ECorbaStorageRegistryPrivate *priv;
	CORBA_Environment ev2;
	GSList *p;

	bonobo_object = bonobo_object_from_servant (servant);
	storage_registry = E_CORBA_STORAGE_REGISTRY (bonobo_object);
	priv = storage_registry->priv;

	p = find_listener (priv->listeners, listener);
	if (p == NULL) {
		CORBA_exception_set (ev, CORBA_USER_EXCEPTION,
				     ex_GNOME_Evolution_StorageRegistry_NotFound,
				     NULL);
		return;
	}

	CORBA_exception_init (&ev2);
	CORBA_Object_release ((CORBA_Object) p->data, &ev2);
	CORBA_exception_free (&ev2);
	
	priv->listeners = g_slist_remove_link (priv->listeners, p);
	g_slist_free (p);
}

static GNOME_Evolution_Folder *
impl_StorageRegistry_getFolderByUri (PortableServer_Servant servant,
				     const CORBA_char *uri,
				     CORBA_Environment *ev)
{
	BonoboObject *bonobo_object;
	ECorbaStorageRegistry *storage_registry;
	GNOME_Evolution_Folder *corba_folder;
	EStorageSet *storage_set;
	EFolder *folder;
	char *path;
	CORBA_char *corba_evolution_uri;

	bonobo_object = bonobo_object_from_servant (servant);
	storage_registry = E_CORBA_STORAGE_REGISTRY (bonobo_object);
	storage_set = storage_registry->priv->storage_set;

	if (!strncmp (uri, E_SHELL_URI_PREFIX, E_SHELL_URI_PREFIX_LEN)) {
		corba_evolution_uri = CORBA_string_dup (uri);
		path = (char *)uri + E_SHELL_URI_PREFIX_LEN;
		folder = e_storage_set_get_folder (storage_set, path);
	} else {
		path = e_storage_set_get_path_for_physical_uri (storage_set, uri);
		if (path) {
			corba_evolution_uri = CORBA_string_alloc (strlen (path) + E_SHELL_URI_PREFIX_LEN + 1);
			strcpy (corba_evolution_uri, E_SHELL_URI_PREFIX);
			strcat (corba_evolution_uri, path);
			folder = e_storage_set_get_folder (storage_set, path);
			g_free (path);
		} else {
			corba_evolution_uri = NULL;
			folder = NULL;
		}
	}

	if (!folder) {
		CORBA_exception_set (ev, CORBA_USER_EXCEPTION,
				     ex_GNOME_Evolution_StorageRegistry_NotFound,
				     NULL);
		return CORBA_OBJECT_NIL;
	}

	corba_folder = GNOME_Evolution_Folder__alloc ();

	corba_folder->displayName = CORBA_string_dup (e_folder_get_name (folder));

	corba_folder->description     = CORBA_string_dup (e_safe_corba_string (e_folder_get_description (folder)));
	corba_folder->type            = CORBA_string_dup (e_folder_get_type_string (folder));
	corba_folder->physicalUri     = CORBA_string_dup (e_safe_corba_string (e_folder_get_physical_uri (folder)));
	corba_folder->customIconName  = CORBA_string_dup (e_safe_corba_string (e_folder_get_custom_icon_name (folder)));
	corba_folder->evolutionUri    = corba_evolution_uri;
	corba_folder->unreadCount     = e_folder_get_unread_count (folder);
	corba_folder->sortingPriority = e_folder_get_sorting_priority (folder);

	return corba_folder;
}


/* GObject methods.  */

static void
impl_dispose (GObject *object)
{
	ECorbaStorageRegistry *corba_storage_registry;
	ECorbaStorageRegistryPrivate *priv;

	corba_storage_registry = E_CORBA_STORAGE_REGISTRY (object);
	priv = corba_storage_registry->priv;

	if (priv->storage_set != NULL) {
		g_object_unref (priv->storage_set);
		priv->storage_set = NULL;
	}

	(* G_OBJECT_CLASS (parent_class)->dispose) (object);
}

static void
impl_finalize (GObject *object)
{
	ECorbaStorageRegistry *corba_storage_registry;
	ECorbaStorageRegistryPrivate *priv;

	corba_storage_registry = E_CORBA_STORAGE_REGISTRY (object);
	priv = corba_storage_registry->priv;

	g_free (priv);

	(* G_OBJECT_CLASS (parent_class)->finalize) (object);
}


/* Initialization.  */

static void
e_corba_storage_registry_class_init (ECorbaStorageRegistryClass *klass)
{
	GObjectClass *object_class;
	POA_GNOME_Evolution_StorageRegistry__epv *epv;

	object_class = G_OBJECT_CLASS (klass);
	object_class->dispose  = impl_dispose;
	object_class->finalize = impl_finalize;

	epv = & klass->epv;
	epv->addStorage          = impl_StorageRegistry_addStorage;
	epv->getStorageList      = impl_StorageRegistry_getStorageList;
	epv->getStorageByName    = impl_StorageRegistry_getStorageByName;
	epv->removeStorageByName = impl_StorageRegistry_removeStorageByName;
	epv->addListener         = impl_StorageRegistry_addListener;
	epv->removeListener      = impl_StorageRegistry_removeListener;
	epv->getFolderByUri      = impl_StorageRegistry_getFolderByUri;

	parent_class = g_type_class_ref(PARENT_TYPE);
}

static void
e_corba_storage_registry_init (ECorbaStorageRegistry *corba_storage_registry)
{
	ECorbaStorageRegistryPrivate *priv;

	priv = g_new (ECorbaStorageRegistryPrivate, 1);
	priv->storage_set = NULL;
	priv->listeners = NULL;
	
	corba_storage_registry->priv = priv;
}


void
e_corba_storage_registry_construct (ECorbaStorageRegistry *corba_storage_registry,
				    EStorageSet *storage_set)
{
	ECorbaStorageRegistryPrivate *priv;

	g_return_if_fail (E_IS_CORBA_STORAGE_REGISTRY (corba_storage_registry));

	priv = corba_storage_registry->priv;

	g_object_ref (storage_set);
	priv->storage_set = storage_set;
}

ECorbaStorageRegistry *
e_corba_storage_registry_new (EStorageSet *storage_set)
{
	ECorbaStorageRegistry *corba_storage_registry;

	g_return_val_if_fail (storage_set != NULL, NULL);
	g_return_val_if_fail (E_IS_STORAGE_SET (storage_set), NULL);

	corba_storage_registry = g_object_new (e_corba_storage_registry_get_type (), NULL);

	e_corba_storage_registry_construct (corba_storage_registry, storage_set);

	return corba_storage_registry;
}


BONOBO_TYPE_FUNC_FULL (ECorbaStorageRegistry,
		       GNOME_Evolution_StorageRegistry,
		       PARENT_TYPE,
		       e_corba_storage_registry)
