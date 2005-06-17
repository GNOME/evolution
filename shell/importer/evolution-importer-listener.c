/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* evolution-importer-client.c
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
 * Author: Iain Holmes  <iain@ximian.com>
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "evolution-importer-listener.h"

#include <bonobo/bonobo-object.h>
#include <e-util/e-util.h>

#include "GNOME_Evolution_Importer.h"
#define PARENT_TYPE BONOBO_OBJECT_TYPE

static BonoboObjectClass *parent_class = NULL;

struct _EvolutionImporterListenerPrivate {
	EvolutionImporterListenerCallback callback;

	void *closure;
};

#if 0
static POA_GNOME_Evolution_ImporterListener__vepv Listener_vepv;

static POA_GNOME_Evolution_ImporterListener *
create_servant (void)
{
	POA_GNOME_Evolution_ImporterListener *servant;
	CORBA_Environment ev;

	servant = (POA_GNOME_Evolution_ImporterListener *) g_new0 (BonoboObjectServant, 1);
	servant->vepv = &Listener_vepv;

	CORBA_exception_init (&ev);
	POA_GNOME_Evolution_ImporterListener__init ((PortableServer_Servant) servant, &ev);
	if (ev._major != CORBA_NO_EXCEPTION) {
		g_free (servant);
		CORBA_exception_free (&ev);
		return NULL;
	}

	CORBA_exception_free (&ev);

	return servant;
}
#endif

static EvolutionImporterResult
corba_result_to_evolution (GNOME_Evolution_ImporterListener_ImporterResult corba_result)
{
	switch (corba_result) {
	case GNOME_Evolution_ImporterListener_OK:
		return EVOLUTION_IMPORTER_OK;
	case GNOME_Evolution_ImporterListener_UNSUPPORTED_OPERATION:
		return EVOLUTION_IMPORTER_UNSUPPORTED_OPERATION;
	case GNOME_Evolution_ImporterListener_UNKNOWN_DATA:
		return EVOLUTION_IMPORTER_UNKNOWN_DATA;
	case GNOME_Evolution_ImporterListener_BAD_DATA:
		return EVOLUTION_IMPORTER_BAD_DATA;
	case GNOME_Evolution_ImporterListener_BAD_FILE:
		return EVOLUTION_IMPORTER_BAD_FILE;
	case GNOME_Evolution_ImporterListener_NOT_READY:
		return EVOLUTION_IMPORTER_NOT_READY;
	case GNOME_Evolution_ImporterListener_BUSY:
		return EVOLUTION_IMPORTER_BUSY;
	default:
		return EVOLUTION_IMPORTER_UNKNOWN_ERROR;
	}
}

static inline EvolutionImporterListener *
evolution_importer_listener_from_servant (PortableServer_Servant servant)
{
	return EVOLUTION_IMPORTER_LISTENER (bonobo_object_from_servant (servant));
}

static void
impl_GNOME_Evolution_ImporterListener_notifyResult (PortableServer_Servant servant,
						    GNOME_Evolution_ImporterListener_ImporterResult result,
						    CORBA_boolean more_items,
						    CORBA_Environment *ev)
{
	EvolutionImporterListener *listener;
	EvolutionImporterListenerPrivate *priv;
	EvolutionImporterResult out_result;

	listener = evolution_importer_listener_from_servant (servant);
	priv = listener->priv;

	out_result = corba_result_to_evolution (result);
	if (priv->callback) {
		(priv->callback) (listener, out_result, more_items, 
				  priv->closure);
	}

	return;
}	


/* GObject methods */
static void
finalise (GObject *object)
{
	EvolutionImporterListener *listener;
	EvolutionImporterListenerPrivate *priv;

	listener = EVOLUTION_IMPORTER_LISTENER (object);
	priv = listener->priv;

	if (priv == NULL)
		return;

	g_free (priv);
	listener->priv = NULL;

	G_OBJECT_CLASS (parent_class)->finalize(object);
}

#if 0
static void
corba_class_init (void)
{
	POA_GNOME_Evolution_ImporterListener__vepv *vepv;
	POA_GNOME_Evolution_ImporterListener__epv *epv;
	PortableServer_ServantBase__epv *base_epv;

	base_epv = g_new0 (PortableServer_ServantBase__epv, 1);
	base_epv->_private = NULL;
	base_epv->finalize = NULL;
	base_epv->default_POA = NULL;

	epv = g_new0 (POA_GNOME_Evolution_ImporterListener__epv, 1);
	epv->notifyResult = impl_GNOME_Evolution_ImporterListener_notifyResult;
	
	vepv = &Listener_vepv;
	vepv->_base_epv = base_epv;
	vepv->Bonobo_Unknown_epv = bonobo_object_get_epv ();
	vepv->GNOME_Evolution_ImporterListener_epv = epv;
}
#endif

static void
evolution_importer_listener_class_init (EvolutionImporterListenerClass *klass)
{
	GObjectClass *object_class;
	POA_GNOME_Evolution_ImporterListener__epv *epv = &klass->epv;
	
	object_class = G_OBJECT_CLASS (klass);
	object_class->finalize = finalise;

	parent_class = g_type_class_ref(PARENT_TYPE);
	epv->notifyResult = impl_GNOME_Evolution_ImporterListener_notifyResult;
}

static void
evolution_importer_listener_init (EvolutionImporterListener *listener)
{
	EvolutionImporterListenerPrivate *priv;

	priv = g_new0 (EvolutionImporterListenerPrivate, 1);
	listener->priv = priv;
}

static void
evolution_importer_listener_construct (EvolutionImporterListener *listener,
				       EvolutionImporterListenerCallback callback,
				       void *closure)
{
	EvolutionImporterListenerPrivate *priv;

	g_return_if_fail (listener != NULL);
	g_return_if_fail (EVOLUTION_IS_IMPORTER_LISTENER (listener));
	g_return_if_fail (callback != NULL);

	priv = listener->priv;
	priv->callback = callback;
	priv->closure = closure;
}

/**
 * evolution_importer_listener_new
 * @callback: The #EvolutionImporterListenerCallback callback.
 * @closure: The data that will be passed to that callback.
 *
 * Creates a new #EvolutionImporterListener object which calls @callback when
 * something happens.
 * Returns: A newly allocated #EvolutionImporterListener.
 */
EvolutionImporterListener *
evolution_importer_listener_new (EvolutionImporterListenerCallback callback,
				 void *closure)
{
	EvolutionImporterListener *listener;

	listener = g_object_new (evolution_importer_listener_get_type (), NULL);

	evolution_importer_listener_construct (listener, callback, closure);
	return listener;
}

BONOBO_TYPE_FUNC_FULL (EvolutionImporterListener,
		       GNOME_Evolution_ImporterListener,
		       PARENT_TYPE, evolution_importer_listener);
