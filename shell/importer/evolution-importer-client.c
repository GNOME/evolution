/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* evolution-importer-client.c
 *
 * Copyright (C) 2000  Ximian, Inc.
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
 * Author: Iain Holmes  <iain@ximian.com>
 * Based on evolution-shell-component-client.c by Ettore Perazzoli
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <bonobo/bonobo-object.h>
#include <bonobo/bonobo-main.h>
#include <gal/util/e-util.h>

#include <liboaf/liboaf.h>

#include "GNOME_Evolution_Importer.h"
#include "evolution-importer-client.h"


#define PARENT_TYPE BONOBO_OBJECT_CLIENT_TYPE
static BonoboObjectClass *parent_class = NULL;


static void
destroy (GtkObject *object)
{
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
}

static void
evolution_importer_client_construct (EvolutionImporterClient *client,
				     CORBA_Object corba_object)
{
	g_return_if_fail (client != NULL);
	g_return_if_fail (EVOLUTION_IS_IMPORTER_CLIENT (client));
	g_return_if_fail (corba_object != CORBA_OBJECT_NIL);

	bonobo_object_client_construct (BONOBO_OBJECT_CLIENT (client), corba_object);
}

/**
 * evolution_importer_client_new:
 * @objref: The CORBA_Object to make a client for.
 *
 * Makes a client for @objref. @objref should be an Evolution_Importer.
 * 
 * Returns: A newly created EvolutionImporterClient.
 */
EvolutionImporterClient *
evolution_importer_client_new (const CORBA_Object objref)
{
	EvolutionImporterClient *client;

	g_return_val_if_fail (objref != CORBA_OBJECT_NIL, NULL);

	client = gtk_type_new (evolution_importer_client_get_type ());
	evolution_importer_client_construct (client, objref);

	return client;
}

/**
 * evolution_importer_client_new_from_id:
 * @id: The oafiid of the component to make a client for.
 *
 * Makes a client for the object returned by activating @id.
 * 
 * Returns: A newly created EvolutionImporterClient.
 */
EvolutionImporterClient *
evolution_importer_client_new_from_id (const char *id)
{
	CORBA_Environment ev;
	CORBA_Object objref;

	g_return_val_if_fail (id != NULL, NULL);

	CORBA_exception_init (&ev);
	objref = oaf_activate_from_id ((char *) id, 0, NULL, &ev);
	if (ev._major != CORBA_NO_EXCEPTION) {
		CORBA_exception_free (&ev);
		g_warning ("Could not start %s.", id);
		return NULL;
	}

	CORBA_exception_free (&ev);
	if (objref == CORBA_OBJECT_NIL) {
		g_warning ("Could not activate component %s", id);
		return NULL;
	}

	return evolution_importer_client_new (objref);
}

/* API */
/**
 * evolution_importer_client_support_format:
 * @client: The EvolutionImporterClient.
 * @filename: Name of the file to check.
 *
 * Checks whether @client is able to import @filename.
 *
 * Returns: TRUE if @client can import @filename, FALSE otherwise.
 */
gboolean
evolution_importer_client_support_format (EvolutionImporterClient *client,
					  const char *filename)
{
	GNOME_Evolution_Importer corba_importer;
	gboolean result;
	CORBA_Environment ev;

	g_return_val_if_fail (client != NULL, FALSE);
	g_return_val_if_fail (EVOLUTION_IS_IMPORTER_CLIENT (client), FALSE);
	g_return_val_if_fail (filename != NULL, FALSE);

	CORBA_exception_init (&ev);
	corba_importer = bonobo_object_corba_objref (BONOBO_OBJECT (client));
	result = GNOME_Evolution_Importer_supportFormat (corba_importer, 
							 filename, &ev);
	CORBA_exception_free (&ev);

	return result;
}

/**
 * evolution_importer_client_load_file:
 * @client: The EvolutionImporterClient.
 * @filename: The file to load.
 *
 * Loads and initialises the importer.
 *
 * Returns: TRUE on sucess, FALSE on failure.
 */
gboolean
evolution_importer_client_load_file (EvolutionImporterClient *client,
				     const char *filename)
{
	GNOME_Evolution_Importer corba_importer;
	gboolean result;
	CORBA_Environment ev;

	g_return_val_if_fail (client != NULL, FALSE);
	g_return_val_if_fail (EVOLUTION_IS_IMPORTER_CLIENT (client), FALSE);
	g_return_val_if_fail (filename != NULL, FALSE);

	CORBA_exception_init (&ev);
	corba_importer = bonobo_object_corba_objref (BONOBO_OBJECT (client));
	result = GNOME_Evolution_Importer_loadFile (corba_importer,
						    filename, &ev);
	CORBA_exception_free (&ev);

	return result;
}

/**
 * evolution_importer_client_process_item:
 * @client: The EvolutionImporterClient.
 * @listener: The EvolutionImporterListener.
 *
 * Starts importing the next item in the file. @listener will be notified
 * when the item has finished.
 */
void
evolution_importer_client_process_item (EvolutionImporterClient *client,
					EvolutionImporterListener *listener)
{
	GNOME_Evolution_Importer corba_importer;
	GNOME_Evolution_ImporterListener corba_listener;
	CORBA_Environment ev;

	g_return_if_fail (client != NULL);
	g_return_if_fail (EVOLUTION_IS_IMPORTER_CLIENT (client));
	g_return_if_fail (listener != NULL);
	g_return_if_fail (EVOLUTION_IS_IMPORTER_LISTENER (listener));

	CORBA_exception_init (&ev);

	corba_importer = bonobo_object_corba_objref (BONOBO_OBJECT (client));
	corba_listener = bonobo_object_corba_objref (BONOBO_OBJECT (listener));
	GNOME_Evolution_Importer_processItem (corba_importer,
					      corba_listener, &ev);
	CORBA_exception_free (&ev);
}

/**
 * evolution_importer_client_get_error:
 * @client: The EvolutionImporterClient.
 *
 * Gets the error as a string. 
 *
 * Returns: The error as a string. If there is no error NULL is returned. 
 * Importers need not support this method and if so, NULL is also returned.
 */
const char *
evolution_importer_client_get_error (EvolutionImporterClient *client)
{
	GNOME_Evolution_Importer corba_importer;
	CORBA_char *str;
	CORBA_Environment ev;

	g_return_val_if_fail (client != NULL, NULL);
	g_return_val_if_fail (EVOLUTION_IS_IMPORTER_CLIENT (client), NULL);

	corba_importer = bonobo_object_corba_objref (BONOBO_OBJECT (client));

	CORBA_exception_init (&ev);
	str = GNOME_Evolution_Importer_getError (corba_importer, &ev);
	
	return str;
}

E_MAKE_TYPE (evolution_importer_client, "EvolutionImporterClient",
	     EvolutionImporterClient, class_init, init, PARENT_TYPE)
