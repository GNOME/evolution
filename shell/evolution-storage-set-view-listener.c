/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* evolution-storage-set-view-listener.c
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
#include <bonobo.h>

#include <gal/util/e-util.h>

#include "evolution-storage-set-view-listener.h"


#define PARENT_TYPE gtk_object_get_type ()
static GtkObjectClass *parent_class = NULL;

struct _EvolutionStorageSetViewListenerPrivate {
	Evolution_StorageSetViewListener corba_listener;
	EvolutionStorageSetViewListenerServant *servant;
};

enum {
	FOLDER_SELECTED,
	STORAGE_SELECTED,
	LAST_SIGNAL
};
static guint signals[LAST_SIGNAL] = { 0 };


/* Evolution::StorageSetViewListener implementation.  */

static POA_Evolution_StorageSetViewListener__vepv my_Evolution_StorageSetViewListener_vepv;

static EvolutionStorageSetViewListener *
gtk_object_from_servant (PortableServer_Servant servant)
{
	EvolutionStorageSetViewListenerServant *my_servant;

	my_servant = (EvolutionStorageSetViewListenerServant *) servant;
	return my_servant->gtk_object;
}

static void
impl_Evolution_StorageSetViewListener_folder_selected (PortableServer_Servant servant,
						       const CORBA_char *uri,
						       CORBA_Environment *ev)
{
	EvolutionStorageSetViewListener *listener;

	listener = gtk_object_from_servant (servant);

	gtk_signal_emit (GTK_OBJECT (listener), signals[FOLDER_SELECTED], uri);
}

static void
impl_Evolution_StorageSetViewListener_storage_selected (PortableServer_Servant servant,
							const CORBA_char *uri,
							CORBA_Environment *ev)
{
	EvolutionStorageSetViewListener *listener;

	listener = gtk_object_from_servant (servant);

	gtk_signal_emit (GTK_OBJECT (listener), signals[STORAGE_SELECTED], uri);
}

static EvolutionStorageSetViewListenerServant *
create_servant (EvolutionStorageSetViewListener *listener)
{
	EvolutionStorageSetViewListenerServant *servant;
	POA_Evolution_StorageSetViewListener *corba_servant;
	CORBA_Environment ev;

	CORBA_exception_init (&ev);

	servant = g_new0 (EvolutionStorageSetViewListenerServant, 1);
	corba_servant = (POA_Evolution_StorageSetViewListener *) servant;

	corba_servant->vepv = &my_Evolution_StorageSetViewListener_vepv;
	POA_Evolution_StorageSetViewListener__init ((PortableServer_Servant) corba_servant, &ev);
	if (ev._major != CORBA_NO_EXCEPTION) {
		g_free (servant);
		CORBA_exception_free (&ev);
		return NULL;
	}

	servant->gtk_object = listener;

	CORBA_exception_free (&ev);

	return servant;
}

static Evolution_StorageSetViewListener
activate_servant (EvolutionStorageSetViewListener *listener,
		  POA_Evolution_StorageSetViewListener *servant)
{
	Evolution_StorageSetViewListener corba_object;
	CORBA_Environment ev;

	CORBA_exception_init (&ev);

	CORBA_free (PortableServer_POA_activate_object (bonobo_poa (), servant, &ev));

	corba_object = PortableServer_POA_servant_to_reference (bonobo_poa(), servant, &ev);

	if (ev._major == CORBA_NO_EXCEPTION && ! CORBA_Object_is_nil (corba_object, &ev)) {
		CORBA_exception_free (&ev);
		return corba_object;
	}

	CORBA_exception_free (&ev);

	return CORBA_OBJECT_NIL;
}


static void
impl_destroy (GtkObject *object)
{
	EvolutionStorageSetViewListener *listener;
	EvolutionStorageSetViewListenerPrivate *priv;
	CORBA_Environment ev;

	listener = EVOLUTION_STORAGE_SET_VIEW_LISTENER (object);
	priv = listener->priv;

	CORBA_exception_init (&ev);

	if (priv->corba_listener != CORBA_OBJECT_NIL)
		CORBA_Object_release (priv->corba_listener, &ev);

	if (priv->servant != NULL) {
		PortableServer_ObjectId *object_id;

		object_id = PortableServer_POA_servant_to_id (bonobo_poa(), priv->servant, &ev);
		PortableServer_POA_deactivate_object (bonobo_poa (), object_id, &ev);
		CORBA_free (object_id);

		POA_Evolution_StorageSetViewListener__fini (priv->servant, &ev);
	}

	CORBA_exception_free (&ev);

	g_free (priv);

	if (GTK_OBJECT_CLASS (parent_class)->destroy != NULL)
		(* GTK_OBJECT_CLASS (parent_class)->destroy) (object);
}


static void
corba_class_init (void)
{
	POA_Evolution_StorageSetViewListener__vepv *vepv;
	POA_Evolution_StorageSetViewListener__epv *epv;
	PortableServer_ServantBase__epv *base_epv;

	base_epv = g_new0 (PortableServer_ServantBase__epv, 1);
	base_epv->_private    = NULL;
	base_epv->finalize    = NULL;
	base_epv->default_POA = NULL;

	epv = g_new0 (POA_Evolution_StorageSetViewListener__epv, 1);
	epv->folder_selected = impl_Evolution_StorageSetViewListener_folder_selected;
	epv->storage_selected = impl_Evolution_StorageSetViewListener_storage_selected;

	vepv = & my_Evolution_StorageSetViewListener_vepv;
	vepv->_base_epv                            = base_epv;
	vepv->Evolution_StorageSetViewListener_epv = epv;
}

static void
class_init (EvolutionStorageSetViewListenerClass *klass)
{
	GtkObjectClass *object_class;

	object_class = GTK_OBJECT_CLASS (klass);
	object_class->destroy = impl_destroy;

	parent_class = gtk_type_class (gtk_object_get_type ());

	signals[FOLDER_SELECTED] = gtk_signal_new ("folder_selected",
						   GTK_RUN_FIRST,
						   object_class->type,
						   GTK_SIGNAL_OFFSET (EvolutionStorageSetViewListenerClass, folder_selected),
						   gtk_marshal_NONE__STRING,
						   GTK_TYPE_NONE, 1,
						   GTK_TYPE_STRING);

	signals[STORAGE_SELECTED] = gtk_signal_new ("storage_selected",
						    GTK_RUN_FIRST,
						    object_class->type,
						    GTK_SIGNAL_OFFSET (EvolutionStorageSetViewListenerClass, storage_selected),
						    gtk_marshal_NONE__STRING,
						    GTK_TYPE_NONE, 1,
						    GTK_TYPE_STRING);

	gtk_object_class_add_signals (object_class, signals, LAST_SIGNAL);

	corba_class_init ();
}

static void
init (EvolutionStorageSetViewListener *storage_set_view_listener)
{
	EvolutionStorageSetViewListenerPrivate *priv;

	priv = g_new (EvolutionStorageSetViewListenerPrivate, 1);
	priv->corba_listener = CORBA_OBJECT_NIL;

	storage_set_view_listener->priv = priv;
}


void
evolution_storage_set_view_listener_construct (EvolutionStorageSetViewListener *listener,
					       Evolution_StorageSetViewListener corba_listener)
{
	EvolutionStorageSetViewListenerPrivate *priv;

	g_return_if_fail (listener != NULL);
	g_return_if_fail (EVOLUTION_IS_STORAGE_SET_VIEW_LISTENER (listener));
	g_return_if_fail (corba_listener != CORBA_OBJECT_NIL);

	priv = listener->priv;

	g_return_if_fail (priv->corba_listener == CORBA_OBJECT_NIL);

	priv->corba_listener = corba_listener;

	GTK_OBJECT_UNSET_FLAGS (GTK_OBJECT (listener), GTK_FLOATING);
}

EvolutionStorageSetViewListener *
evolution_storage_set_view_listener_new (void)
{
	EvolutionStorageSetViewListener *new;
	EvolutionStorageSetViewListenerPrivate *priv;
	Evolution_StorageSetViewListener corba_listener;

	new = gtk_type_new (evolution_storage_set_view_listener_get_type ());
	priv = new->priv;

	priv->servant = create_servant (new);
	corba_listener = activate_servant (new, (POA_Evolution_StorageSetViewListener *) priv->servant);

	evolution_storage_set_view_listener_construct (new, corba_listener);

	return new;
}

Evolution_StorageSetViewListener
evolution_storage_set_view_listener_corba_objref (EvolutionStorageSetViewListener *listener)
{
	EvolutionStorageSetViewListenerPrivate *priv;

	g_return_val_if_fail (listener != NULL, CORBA_OBJECT_NIL);
	g_return_val_if_fail (EVOLUTION_IS_STORAGE_SET_VIEW_LISTENER (listener), CORBA_OBJECT_NIL);

	priv = listener->priv;
	return priv->corba_listener;
}


E_MAKE_TYPE (evolution_storage_set_view_listener, "EvolutionStorageSetViewListener", EvolutionStorageSetViewListener,
	     class_init, init, PARENT_TYPE)
