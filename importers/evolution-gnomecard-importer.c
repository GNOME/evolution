/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* evolution-gnomecard-intelligent-importer.c
 *
 * Copyright (C) 2001  Ximian, Inc.
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
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif
#include <stdio.h>

#include <liboaf/liboaf.h>
#include <bonobo/bonobo-context.h>
#include <bonobo/bonobo-generic-factory.h>
#include <bonobo/bonobo-main.h>

#include <e-book.h>

#include <importer/evolution-intelligent-importer.h>
#include <importer/GNOME_Evolution_Importer.h>

#define COMPONENT_FACTORY_IID "OAFIID:GNOME_Evolution_GnomeCard_Intelligent_ImporterFactory"

#define KEY "gnomecard-imported"
typedef struct {
	GNOME_Evolution_Importer importer;
	EvolutionImporterListener *listener;
} GnomeCardImporter;

static gboolean
gnomecard_can_import (EvolutionIntelligentImporter *ii,
		      void *closure)
{
	char *evolution_dir;
	char *gnomecard;
	char *key;
	gboolean result;

	evolution_dir = gnome_util_prepend_user_home ("evolution");
	key = g_strdup_printf ("=%s/config/Importers=/importers/", evolution_dir);
	g_free (evolution_dir);

	gnome_config_push_prefix (key);
	g_free (key);

	if (gnome_config_get_bool (KEY) == TRUE) {
		gnome_config_pop_prefix ();
		return FALSE;
	}
	gnome_config_pop_prefix ();

	gnomecard = gnome_util_home_file ("GnomeCard.gcrd");
	result = g_file_exists (gnomecard);
	g_free (gnomecard);

	return result;
}

static gboolean
importer_timeout_fn (gpointer data)
{
	GnomeCardImporter *gci = (GnomeCardImporter *) data;
	CORBA_Object objref;
	CORBA_Environment ev;

	CORBA_exception_init (&ev);
	objref = bonobo_object_corba_objref (BONOBO_OBJECT (gci->listener));
	GNOME_Evolution_Importer_processItem (gci->importer, objref, &ev);
	CORBA_exception_free (&ev);

	return FALSE;
}
					      
static void
importer_cb (EvolutionImporterListener *listener,
	     EvolutionImporterResult result,
	     gboolean more_items,
	     void *data)
{
	GnomeCardImporter *gci = (GnomeCardImporter *) data;
	CORBA_Object objref;
	CORBA_Environment ev;

	if (result == EVOLUTION_IMPORTER_NOT_READY ||
	    result == EVOLUTION_IMPORTER_BUSY) {
		gtk_timeout_add (5000, importer_timeout_fn, data);
		return;
	}

	if (more_items) {
		g_idle_add_full (G_PRIORITY_LOW, importer_timeout_fn, data, NULL);
		return;
	}

	/* Quit Here */
}
				 
static void
gnomecard_import (EvolutionIntelligentImporter *ii,
		  void *closure)
{
	CORBA_boolean result;
	GnomeCardImporter *gci = closure;
	CORBA_Object objref;
	CORBA_Environment ev;
	char *gnomecard;

	gnomecard = gnome_util_home_file ("GnomeCard.gcrd");

	CORBA_exception_init (&ev);
	result = GNOME_Evolution_Importer_loadFile (gci->importer, 
						    gnomecard, NULL, &ev);
	if (ev._major != CORBA_NO_EXCEPTION || result == FALSE) {
		g_warning ("Exception here: %s", CORBA_exception_id (&ev));
		CORBA_Object_release (gci->importer, &ev);
		CORBA_exception_free (&ev);
		return FALSE;
	}

	gci->listener = evolution_importer_listener_new (importer_cb, gci);
	objref = bonobo_object_corba_objref (BONOBO_OBJECT (gci->listener));
	GNOME_Evolution_Importer_processItem (gci->importer, objref, &ev);
	if (ev._major != CORBA_NO_EXCEPTION) {
		g_warning ("Exception: %s", CORBA_exception_id (&ev));
		return FALSE;
	}

	CORBA_exception_free (&ev);

	return TRUE;
}

static BonoboObject *
factory_fn (BonoboGenericFactory *_factory,
	    void *closure)
{
	EvolutionIntelligentImporter *importer;
	GnomeCardImporter *gci;
	char *message = N_("Evolution has found GnomeCard files.\n"
			   "Would you like them to be imported into Evolution?");

	gci = g_new (GnomeCardImporter, 1);

	CORBA_exception_init (&ev);
	gci->importer = oaf_activate_from_id (VCARD_IMPORTER_IID, 0, NULL, &ev);
	if (ev._major != CORBA_NO_EXCEPTION) {
		g_warning ("Could not start VCard importer: %s",
			   CORBA_exception_id (&ev));
		CORBA_exception_free (&ev);
		return NULL;
	}
	CORBA_exception_free (&ev);

	importer = evolution_intelligent_importer_new (gnomecard_can_import, 
						       gnomecard_import, 
						       "GnomeCard",
						       _(message), gci);
	
	gtk_signal_connect (GTK_OBJECT (importer), "destroy",
			    GTK_SIGNAL_FUNC (importer_destroy_cb), gci);
	
	return BONOBO_OBJECT (importer);
}

static void
importer_init (void)
{
	BonoboObject *factory;

	factory = bonobo_generic_factory_new (COMPONENT_FACTORY_IID, 
					      factory_fn, NULL);
	if (factory == NULL) {
		g_error ("Unable to create factory");
		exit (0);
	}

	bonobo_running_context_auto_exit_unref (BONOBO_OBJECT (factory));
}

int
main (int argc,
      char **argv)
{
	CORBA_ORB orb;
	
	gnome_init_with_popt_table ("Evolution-GnomeCard-Intelligent-Importer",
				    VERSION, argc, argv, oaf_popt_options, 0,
				    NULL);
	orb = oaf_init (argc, argv);
	if (bonobo_init (orb, CORBA_OBJECT_NIL, CORBA_OBJECT_NIL) == FALSE) {
		g_error ("Could not initialize Bonobo.");
		exit (0);
	}

	importer_init ();
	bonobo_main ();

	return 0;
}


