/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* e-corba-storage.c
 *
 * Copyright (C) 2000 Helix Code, Inc.
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

#include "e-util/e-util.h"

#include "Evolution.h"

#include "e-corba-storage.h"


#define PARENT_TYPE E_TYPE_STORAGE
static EStorageClass *parent_class = NULL;

typedef struct _StorageListenerServant StorageListenerServant;

struct _ECorbaStoragePrivate {
	char *name;

	Evolution_Storage storage_interface;

	/* The Evolution::StorageListener interface we expose.  */

	Evolution_StorageListener storage_listener_interface;
	StorageListenerServant *storage_listener_servant;
};


/* Implementation of the CORBA Evolution::StorageListener interface.  */

static POA_Evolution_StorageListener__vepv storage_listener_vepv;

struct _StorageListenerServant {
	POA_Evolution_StorageListener servant;
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
				 const Evolution_Folder *folder,
				 CORBA_Environment *ev)
{
	StorageListenerServant *storage_listener_servant;
	EStorage *storage;
	EFolder *e_folder;

	storage_listener_servant = (StorageListenerServant *) servant;
	storage = storage_listener_servant->storage;

	e_folder = e_folder_new (folder->name,
				 folder->type,
				 folder->description);

	if (! e_storage_new_folder (storage, path, e_folder)) {
		CORBA_exception_set (ev,
				     CORBA_USER_EXCEPTION,
				     ex_Evolution_StorageListener_Exists,
				     NULL);
		gtk_object_unref (GTK_OBJECT (e_folder));
	}
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

	if (! e_storage_remove_folder (storage, path))
		CORBA_exception_set (ev,
				     CORBA_USER_EXCEPTION,
				     ex_Evolution_StorageListener_NotFound,
				     NULL);
}


static gboolean
setup_storage_listener (ECorbaStorage *corba_storage)
{
	StorageListenerServant *servant;
	ECorbaStoragePrivate *priv;
	Evolution_StorageListener storage_listener_interface;
	CORBA_Environment ev;

	priv = corba_storage->priv;

	servant = storage_listener_servant_new (corba_storage);

	CORBA_exception_init (&ev);

	POA_Evolution_StorageListener__init (servant, &ev);
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

		POA_Evolution_StorageListener__fini (priv->storage_listener_servant, &ev);
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


static void
corba_class_init (void)
{
	POA_Evolution_StorageListener__vepv *vepv;
	POA_Evolution_StorageListener__epv *epv;
	PortableServer_ServantBase__epv *base_epv;

	base_epv = g_new0 (PortableServer_ServantBase__epv, 1);
	base_epv->_private    = NULL;
	base_epv->finalize    = NULL;
	base_epv->default_POA = NULL;

	epv = g_new0 (POA_Evolution_StorageListener__epv, 1);
	epv->new_folder     = impl_StorageListener_new_folder;
	epv->removed_folder = impl_StorageListener_removed_folder;

	vepv = &storage_listener_vepv;
	vepv->_base_epv                     = base_epv;
	vepv->Evolution_StorageListener_epv = epv;
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
			   const Evolution_Storage storage_interface,
			   const char *name)
{
	ECorbaStoragePrivate *priv;
	CORBA_Environment ev;

	g_return_if_fail (corba_storage != NULL);
	g_return_if_fail (E_IS_CORBA_STORAGE (corba_storage));
	g_return_if_fail (storage_interface != CORBA_OBJECT_NIL);
	g_return_if_fail (name != NULL);

	e_storage_construct (E_STORAGE (corba_storage));

	priv = corba_storage->priv;

	priv->name = g_strdup (name);

	CORBA_exception_init (&ev);

	Bonobo_Unknown_ref (storage_interface, &ev);
	if (ev._major != CORBA_NO_EXCEPTION) {
		g_warning ("%s -- Cannot reference Bonobo object", __FUNCTION__);
	} else {
		priv->storage_interface = CORBA_Object_duplicate (storage_interface, &ev);
	}

	CORBA_exception_free (&ev);

	setup_storage_listener (corba_storage);
}

EStorage *
e_corba_storage_new (const Evolution_Storage storage_interface,
		     const char *name)
{
	EStorage *new;

	g_return_val_if_fail (storage_interface != CORBA_OBJECT_NIL, NULL);
	g_return_val_if_fail (name != NULL, NULL);

	new = gtk_type_new (e_corba_storage_get_type ());

	e_corba_storage_construct (E_CORBA_STORAGE (new), storage_interface, name);

	return new;
}


const Evolution_StorageListener
e_corba_storage_get_StorageListener (ECorbaStorage *corba_storage)
{
	g_return_val_if_fail (corba_storage != NULL, NULL);
	g_return_val_if_fail (E_IS_CORBA_STORAGE (corba_storage), NULL);

	return corba_storage->priv->storage_listener_interface;
}


E_MAKE_TYPE (e_corba_storage, "ECorbaStorage", ECorbaStorage, class_init, init, PARENT_TYPE)
