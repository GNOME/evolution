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
#include <bonobo/bonobo-control.h>

#include <e-book.h>

#include <importer/evolution-intelligent-importer.h>
#include <importer/GNOME_Evolution_Importer.h>
#include <importer/evolution-importer-listener.h>

#define COMPONENT_FACTORY_IID "OAFIID:GNOME_Evolution_GnomeCard_Intelligent_ImporterFactory"
#define VCARD_IMPORTER_IID "OAFIID:GNOME_Evolution_Addressbook_VCard_Importer"

typedef struct {
	GNOME_Evolution_Importer importer;
	EvolutionImporterListener *listener;

	GtkWidget *addresses;
	gboolean do_addresses;

	GtkWidget *ask;
	gboolean ask_again;
} GnomeCardImporter;

static void
gnomecard_store_settings (GnomeCardImporter *importer)
{
	char *evolution_dir, *key;

	evolution_dir = gnome_util_prepend_user_home ("evolution");
	key = g_strdup_printf ("=%s/config/Gnomecard-Importer=/settings/",
			       evolution_dir);
	g_free (evolution_dir);

	gnome_config_push_prefix (key);
	g_free (key);

	gnome_config_set_bool ("address", importer->do_addresses);

	gnome_config_set_bool ("ask-again", importer->ask_again);
	gnome_config_pop_prefix ();
}

static void
gnomecard_restore_settings (GnomeCardImporter *importer)
{
	char *evolution_dir, *key;

	evolution_dir = gnome_util_prepend_user_home ("evolution");
	key = g_strdup_printf ("=%s/config/Gnomecard-Importer=/settings/",
			       evolution_dir);
	g_free (evolution_dir);

	gnome_config_push_prefix (key);
	g_free (key);

	importer->do_addresses = gnome_config_get_bool ("address=True");

	importer->ask_again = gnome_config_get_bool ("ask-again=False");
	gnome_config_pop_prefix ();
}

static gboolean
gnomecard_can_import (EvolutionIntelligentImporter *ii,
		      void *closure)
{
	GnomeCardImporter *importer = closure;
	char *evolution_dir;
	char *gnomecard;
	char *key;
	gboolean result, address;

	evolution_dir = gnome_util_prepend_user_home ("evolution");
	key = g_strdup_printf ("=%s/config/Importers=/gnomecard-importers/", evolution_dir);
	g_free (evolution_dir);

	gnome_config_push_prefix (key);
	g_free (key);

	address = gnome_config_get_bool ("address-imported");
	if (address == TRUE) {
		gnome_config_pop_prefix ();
		return FALSE;
	}
	gnome_config_pop_prefix ();

	if (importer->ask_again == TRUE) {
		return FALSE;
	}

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

	/* Reference our object so when the shell release_unrefs us
	   we will still exist and not go byebye */
	bonobo_object_ref (BONOBO_OBJECT (ii));

	gnomecard_store_settings (gci);
	if (gci->do_addresses == TRUE) {

		CORBA_exception_init (&ev);
		result = GNOME_Evolution_Importer_loadFile (gci->importer, 
							    gnomecard, 
							    "", &ev);
		if (ev._major != CORBA_NO_EXCEPTION || result == FALSE) {
			g_warning ("Exception here: %s",
				   CORBA_exception_id (&ev));
			CORBA_Object_release (gci->importer, &ev);
			CORBA_exception_free (&ev);
			return;
		}

		gci->listener = evolution_importer_listener_new (importer_cb, 
								 gci);
		objref = bonobo_object_corba_objref (BONOBO_OBJECT (gci->listener));
		GNOME_Evolution_Importer_processItem (gci->importer, objref, &ev);
		if (ev._major != CORBA_NO_EXCEPTION) {
			g_warning ("Exception: %s", CORBA_exception_id (&ev));
			return;
		}

		CORBA_exception_free (&ev);

		return;
	} else {
		bonobo_object_unref (BONOBO_OBJECT (ii));
		return;
	}
}

static void
gnomecard_destroy_cb (GtkObject *object,
		      GnomeCardImporter *importer)
{
	/* save the state of the checkboxes */
	g_print ("\n---------Settings-------\n");
	g_print ("Addressbook - %s\n", importer->do_addresses? "Yes" : "No");

	gnomecard_store_settings (importer);
	gtk_main_quit ();
}

/* Fun with aggregation */
static void
checkbox_toggle_cb (GtkToggleButton *tb,
		    gboolean *do_item)
{
	*do_item = gtk_toggle_button_get_active (tb);
}

static BonoboControl *
create_checkboxes_control (GnomeCardImporter *importer)
{
	GtkWidget *container, *vbox, *sep;
	BonoboControl *control;

	container = gtk_frame_new (_("Import"));
	vbox = gtk_vbox_new (FALSE, 2);
	gtk_container_set_border_width (GTK_CONTAINER (container), 2);
	gtk_container_add (GTK_CONTAINER (container), vbox);

	importer->addresses = gtk_check_button_new_with_label (_("Addressbook"));
	gtk_signal_connect (GTK_OBJECT (importer->addresses), "toggled",
			    GTK_SIGNAL_FUNC (checkbox_toggle_cb),
			    &importer->do_addresses);

	sep = gtk_hseparator_new ();

	importer->ask = gtk_check_button_new_with_label (_("Don't ask me again"));
	gtk_signal_connect (GTK_OBJECT (importer->ask), "toggled",
			    GTK_SIGNAL_FUNC (checkbox_toggle_cb),
			    &importer->ask_again);

	gtk_box_pack_start (GTK_BOX (vbox), importer->addresses, FALSE, FALSE, 0);
	gtk_box_pack_start (GTK_BOX (vbox), sep, FALSE, FALSE, 0);
	gtk_box_pack_start (GTK_BOX (vbox), importer->ask, FALSE, FALSE, 0);

	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (importer->addresses),
				      importer->do_addresses);
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (importer->ask),
				      importer->ask_again);

	gtk_widget_show_all (container);
	control = bonobo_control_new (container);
	return control;
}

static BonoboObject *
factory_fn (BonoboGenericFactory *_factory,
	    void *closure)
{
	EvolutionIntelligentImporter *importer;
	GnomeCardImporter *gci;
	char *message = N_("Evolution has found GnomeCard files.\n"
			   "Would you like them to be imported into Evolution?");
	CORBA_Environment ev;
	BonoboControl *control;

	gci = g_new (GnomeCardImporter, 1);
	gnomecard_restore_settings (gci);

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
			    GTK_SIGNAL_FUNC (gnomecard_destroy_cb), gci);

	control = create_checkboxes_control (gci);
	bonobo_object_add_interface (BONOBO_OBJECT (importer),
				     BONOBO_OBJECT (control));
	
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


