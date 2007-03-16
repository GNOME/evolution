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
 * Based on evolution-shell-component-client.c by Ettore Perazzoli
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "evolution-importer-client.h"

#include <glib.h>
#include <bonobo/bonobo-object.h>
#include <bonobo/bonobo-main.h>
#include <bonobo/bonobo-widget.h>
#include <bonobo/bonobo-exception.h>

#include "GNOME_Evolution_Importer.h"

G_DEFINE_TYPE (EvolutionImporterClient, evolution_importer_client, G_TYPE_OBJECT)


static void
finalise (GObject *object)
{
	/* FIXME: should this unref the client->objref?? */

	(* G_OBJECT_CLASS (evolution_importer_client_parent_class)->finalize) (object);
}

static void
evolution_importer_client_class_init (EvolutionImporterClientClass *klass)
{
	GObjectClass *object_class;

	object_class = G_OBJECT_CLASS (klass);

	object_class->finalize = finalise;
}

static void
evolution_importer_client_init (EvolutionImporterClient *client)
{
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

	client = g_object_new (evolution_importer_client_get_type (), NULL);
	client->objref = objref;

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
	objref = bonobo_activation_activate_from_id ((char *) id, 0, NULL, &ev);
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
GtkWidget *
evolution_importer_client_create_control (EvolutionImporterClient *client)
{
	GNOME_Evolution_Importer corba_importer;
	GtkWidget *widget = NULL;
	Bonobo_Control control;
	CORBA_Environment ev;
	
	g_return_val_if_fail (client != NULL, FALSE);
	g_return_val_if_fail (EVOLUTION_IS_IMPORTER_CLIENT (client), FALSE);

	CORBA_exception_init (&ev);
	corba_importer = client->objref;
	GNOME_Evolution_Importer_createControl (corba_importer, &control, &ev);

	if (!BONOBO_EX (&ev)) {
		/* FIXME Pass in container? */
		widget = bonobo_widget_new_control_from_objref (control, NULL);
		gtk_widget_show (widget);
	}	
	
	CORBA_exception_free (&ev);

	return widget;	
}

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
	corba_importer = client->objref;
	result = GNOME_Evolution_Importer_supportFormat (corba_importer, 
							 filename, &ev);
	CORBA_exception_free (&ev);

	return result;
}

/**
 * evolution_importer_client_load_file:
 * @client: The EvolutionImporterClient.
 * @filename: The file to load.
 * @physical_uri: The physical URI of the folder to import data into.
 * @folder_type: The type of the folder represented by @physical_uri.
 *
 * Loads and initialises the importer.
 *
 * Returns: TRUE on sucess, FALSE on failure.
 */
gboolean
evolution_importer_client_load_file (EvolutionImporterClient *client, const char *filename)
{
	GNOME_Evolution_Importer corba_importer;
	gboolean result;
	CORBA_Environment ev;

	g_return_val_if_fail (client != NULL, FALSE);
	g_return_val_if_fail (EVOLUTION_IS_IMPORTER_CLIENT (client), FALSE);
	g_return_val_if_fail (filename != NULL, FALSE);

	CORBA_exception_init (&ev);
	corba_importer = client->objref;
	result = GNOME_Evolution_Importer_loadFile (corba_importer, filename, &ev);
	if (ev._major != CORBA_NO_EXCEPTION) {
		g_warning ("Oh there *WAS* an exception.\nIt was %s",
			   CORBA_exception_id (&ev));
		CORBA_exception_free (&ev);
		return FALSE;
	}
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

	corba_importer = client->objref;
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

	corba_importer = client->objref;

	CORBA_exception_init (&ev);
	str = GNOME_Evolution_Importer_getError (corba_importer, &ev);
	
	return str;
}
