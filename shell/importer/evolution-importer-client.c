/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* evolution-importer-listener.c
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
 * Author: Iain Holmes  <iain@helixcode.com>
 * Based on evolution-shell-component-client.c by Ettore Perazzoli
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <bonobo/bonobo-object.h>
#include <bonobo/bonobo-main.h>
#include <gal/util/e-util.h>

#include "GNOME_Evolution_Importer.h"
#include "evolution-importer-client.h"


#define PARENT_TYPE BONOBO_OBJECT_CLIENT_TYPE
static BonoboObjectClass *parent_class = NULL;

struct _EvolutionImporterClientPrivate {
	EvolutionImporterClientCallback callback;
	void *closure;

	GNOME_Evolution_ImporterListener listener_interface;
	PortableServer_Servant listener_servant;
};


static PortableServer_ServantBase__epv Listener_base_epv;
static POA_GNOME_Evolution_ImporterListener__epv Listener_epv;
static POA_GNOME_Evolution_ImporterListener__vepv Listener_vepv;
static gboolean Listener_vepv_initialized = FALSE;

struct _ImporterListenerServant {
	POA_GNOME_Evolution_ImporterListener servant;
	EvolutionImporterClient *component_client;
};
typedef struct _ImporterListenerServant ImporterListenerServant;



static void
dispatch_callback (EvolutionImporterClient *client,
		   EvolutionImporterResult result,
		   gboolean more_items)
{
	EvolutionImporterClientPrivate *priv;
	EvolutionImporterClientCallback callback;
	PortableServer_ObjectId *oid;
	void *closure;
	CORBA_Environment ev;

	priv = client->private;

	g_return_if_fail (priv->callback != NULL);
	g_return_if_fail (priv->listener_servant != NULL);

	CORBA_exception_init (&ev);

	oid = PortableServer_POA_servant_to_id (bonobo_poa (), priv->listener_servant, &ev);
	PortableServer_POA_deactivate_object (bonobo_poa (), oid, &ev);
	POA_GNOME_Evolution_ImporterListener__fini (priv->listener_servant, &ev);
	CORBA_free (oid);

	CORBA_Object_release (priv->listener_interface, &ev);
	CORBA_exception_free (&ev);

	priv->listener_servant = NULL;
	priv->listener_interface = CORBA_OBJECT_NIL;

	callback = priv->callback;
	closure = priv->closure;

	priv->callback = NULL;
	priv->closure = NULL;

	(* callback) (client, result, more_items, closure);
}

static EvolutionImporterClient *
component_client_from_ImporterListener_servant (PortableServer_Servant servant)
{
	ImporterListenerServant *listener_servant;

	listener_servant = (ImporterListenerServant *) servant;
	return listener_servant->component_client;
}

static EvolutionImporterResult
result_from_async_corba_result (GNOME_Evolution_ImporterListener_ImporterResult corba_result)
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
	default:
		return EVOLUTION_IMPORTER_UNKNOWN_ERROR;
	}
}

static void
impl_ImporterListener_notifyResult (PortableServer_Servant servant,
				    const GNOME_Evolution_ImporterListener_ImporterResult result,
				    const CORBA_boolean more_items,
				    CORBA_Environment *ev)
{
	EvolutionImporterClient *client;

	client = component_client_from_ImporterListener_servant (servant);
	dispatch_callback (client, result_from_async_corba_result (result), more_items);
}

static void
ImporterListener_vepv_initialize (void)
{
	Listener_base_epv._private = NULL;
	Listener_base_epv.finalize = NULL;
	Listener_base_epv.default_POA = NULL;

	Listener_epv.notifyResult = impl_ImporterListener_notifyResult;
	
	Listener_vepv._base_epv = &Listener_base_epv;
	Listener_vepv.GNOME_Evolution_ImporterListener_epv = &Listener_epv;
	
	Listener_vepv_initialized = TRUE;
}

static PortableServer_Servant *
create_listener_servant (EvolutionImporterClient *client)
{
	ImporterListenerServant *servant;

	if (!Listener_vepv_initialized)
		ImporterListener_vepv_initialize ();

	servant = g_new0 (ImporterListenerServant, 1);
	servant->servant.vepv = &Listener_vepv;
	servant->component_client = client;

	return (PortableServer_Servant) servant;
}

static void
free_listener_servant (PortableServer_Servant servant)
{
	g_free (servant);
}

static void
create_listener_interface (EvolutionImporterClient *client)
{
	EvolutionImporterClientPrivate *priv;
	PortableServer_Servant listener_servant;
	GNOME_Evolution_ImporterListener corba_interface;
	CORBA_Environment ev;

	priv = client->private;

	listener_servant = create_listener_servant (client);
	CORBA_exception_init (&ev);

	POA_GNOME_Evolution_ImporterListener__init (listener_servant, &ev);

	if (ev._major != CORBA_NO_EXCEPTION) {
		free_listener_servant (listener_servant);
		return;
	}

	CORBA_free (PortableServer_POA_activate_object (bonobo_poa (), 
							listener_servant, &ev));
	corba_interface = PortableServer_POA_servant_to_reference (bonobo_poa (),
								   listener_servant, &ev);
	if (ev._major != CORBA_NO_EXCEPTION) {
		corba_interface = CORBA_OBJECT_NIL;
			free_listener_servant (listener_servant);
	}

	CORBA_exception_free (&ev);

	priv->listener_servant = listener_servant;
	priv->listener_interface = corba_interface;
}



static void
destroy (GtkObject *object)
{
	EvolutionImporterClient *client;
	EvolutionImporterClientPrivate *priv;

	client = EVOLUTION_IMPORTER_CLIENT (object);
	priv = client->private;

	if (priv->callback != NULL)
		dispatch_callback (client, EVOLUTION_IMPORTER_INTERRUPTED, FALSE);

	g_free (priv);

	(* GTK_OBJECT_CLASS (parent_class)->destroy) (object);
}

static void
class_init (EvolutionImporterClientClass *klass)
{
	GtkObjectClass *object_class;

	object_class = GTK_OBJECT_CLASS (klass);
	parent_class = gtk_type_class (PARENT_TYPE);

	object_class->destroy = destroy;
}

static void
init (EvolutionImporterClient *client)
{
	EvolutionImporterClientPrivate *priv;

	priv = g_new (EvolutionImporterClientPrivate, 1);
	priv->listener_interface = CORBA_OBJECT_NIL;
	priv->listener_servant = NULL;
	priv->callback = NULL;
	priv->closure = NULL;

	client->private = priv;
}

void
evolution_importer_client_construct (EvolutionImporterClient *client,
				     CORBA_Object corba_object)
{
	g_return_if_fail (client != NULL);
	g_return_if_fail (EVOLUTION_IS_IMPORTER_CLIENT (client));
	g_return_if_fail (corba_object != CORBA_OBJECT_NIL);

	bonobo_object_client_construct (BONOBO_OBJECT_CLIENT (client), corba_object);
}

EvolutionImporterClient *
evolution_importer_client_new (const CORBA_Object objref)
{
	EvolutionImporterClient *client;

	g_return_val_if_fail (objref != CORBA_OBJECT_NIL, NULL);

	client = gtk_type_new (evolution_importer_client_get_type ());
	evolution_importer_client_construct (client, objref);

	return client;
}

/* API */
void
evolution_importer_client_process_item (EvolutionImporterClient *client,
					EvolutionImporterClientCallback callback,
					void *closure)
{
	EvolutionImporterClientPrivate *priv;
	GNOME_Evolution_Importer corba_importer;
	CORBA_Environment ev;

	g_return_if_fail (client != NULL);
	g_return_if_fail (EVOLUTION_IS_IMPORTER_CLIENT (client));
	g_return_if_fail (callback != NULL);

	priv = client->private;

	if (priv->callback != NULL) {
		(* callback) (client, EVOLUTION_IMPORTER_BUSY, FALSE, closure);
		return;
	}
	
	create_listener_interface (client);

	CORBA_exception_init (&ev);

	corba_importer = bonobo_object_corba_objref (BONOBO_OBJECT (client));
	priv->callback = callback;
	priv->closure = closure;

	GNOME_Evolution_Importer_processItem (corba_importer,
					      priv->listener_interface, &ev);
	CORBA_exception_free (&ev);
}

const char *
evolution_importer_client_get_error (EvolutionImporterClient *client)
{
	EvolutionImporterClientPrivate *priv;
	GNOME_Evolution_Importer corba_importer;
	CORBA_char *str;
	CORBA_Environment ev;

	g_return_val_if_fail (client != NULL, NULL);
	g_return_val_if_fail (EVOLUTION_IS_IMPORTER_CLIENT (client), NULL);

	priv = client->private;
	corba_importer = bonobo_object_corba_objref (BONOBO_OBJECT (client));

	CORBA_exception_init (&ev);
	str = GNOME_Evolution_Importer_getError (corba_importer, &ev);
	
	return str;
}

E_MAKE_TYPE (evolution_importer_client, "EvolutionImporterClient",
	     EvolutionImporterClient, class_init, init, PARENT_TYPE)
