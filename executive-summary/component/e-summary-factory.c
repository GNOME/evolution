/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* e-summary-factory.c
 *
 * Authors: Ettore Perazzoli <ettore@helixcode.com>
 *          Iain Holmes <iain@helixcode.com>
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
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <bonobo/bonobo-control.h>
#include <bonobo/bonobo-main.h>
#include <bonobo/bonobo-object.h>
#include <bonobo/bonobo-generic-factory.h>
#include <bonobo/bonobo-ui-util.h>

#include <liboaf/liboaf.h>

#include <gdk-pixbuf/gdk-pixbuf.h>
#include <gal/util/e-util.h>
#include "e-util/e-gui-utils.h"

#include "e-summary-factory.h"

#include "e-summary.h"
#include "e-summary-util.h"
#include "e-summary-callbacks.h"
#include "Evolution.h"

#include <evolution-services/Executive-Summary.h>
#include <evolution-services/executive-summary-component.h>
#include <evolution-services/executive-summary-component-factory-client.h>

static GList *control_list = NULL;

BonoboUIVerb verbs[] = {
	BONOBO_UI_UNSAFE_VERB ("AddService", embed_service),
	BONOBO_UI_UNSAFE_VERB ("NewMail", new_mail),
	BONOBO_UI_UNSAFE_VERB ("ESummarySettings", configure_summary),
	BONOBO_UI_VERB_END
};

static EPixmap pixmaps [] = {
	E_PIXMAP ("/Toolbar/AddService", "buttons/add-service.png"),
	E_PIXMAP ("/Toolbar/NewMail", "buttons/compose-message.png"),
	E_PIXMAP_END
};

static void
control_activate (BonoboControl     *control,
		  BonoboUIComponent *ui_component,
		  ESummary          *esummary)
{
	Bonobo_UIContainer container;

	container = bonobo_control_get_remote_ui_container (control);
	bonobo_ui_component_set_container (ui_component, container);
	bonobo_object_release_unref (container, NULL);

	bonobo_ui_component_add_verb_list_with_data (ui_component, verbs, esummary);
	
	bonobo_ui_component_freeze (ui_component, NULL);

	bonobo_ui_util_set_ui (ui_component, EVOLUTION_DATADIR,
			       "evolution-executive-summary.xml", 
			       "evolution-executive-summary");

	e_pixmaps_update (ui_component, pixmaps);

	bonobo_ui_component_thaw (ui_component, NULL);
}

static void
control_deactivate (BonoboControl     *control,
		    BonoboUIComponent *ui_component,
		    ESummary          *esummary)
{
	e_summary_unset_message (esummary); 
	bonobo_ui_component_unset_container (ui_component);
}

static void
control_activate_cb (BonoboControl *control,
		     gboolean activate,
		     gpointer user_data)
{
	ESummary *summary;
	BonoboUIComponent *ui_component;
	Bonobo_ControlFrame control_frame;
	GNOME_Evolution_ShellView shell_view_interface;
	CORBA_Environment ev;

	ui_component = bonobo_control_get_ui_component (control);
	g_assert (ui_component != NULL);

	if (gtk_object_get_data (GTK_OBJECT (control), "shell_view_interface") == NULL) {
		control_frame = bonobo_control_get_control_frame (control);
		if (control_frame == NULL) {
			goto out;
		}

		CORBA_exception_init (&ev);
		shell_view_interface = Bonobo_Unknown_queryInterface (control_frame,
								      "IDL:GNOME/Evolution/ShellView:1.0",
								      &ev);
		CORBA_exception_free (&ev);

		if (shell_view_interface != CORBA_OBJECT_NIL) {
			gtk_object_set_data (GTK_OBJECT (control),
					     "shell_view_interface",
					     shell_view_interface);
		} else {
			g_warning ("Control frame doesn't have Evolution/ShellView.");
		}
									       
		summary = E_SUMMARY (user_data);
		e_summary_set_shell_view_interface (summary, 
						    shell_view_interface);
	}

 out:
	if (activate)
		control_activate (control, ui_component, user_data);
	else
		control_deactivate (control, ui_component, user_data);
}

static void
control_destroy_cb (BonoboControl *control,
		    gpointer user_data)
{
	GtkWidget *esummary = user_data;

	control_list = g_list_remove (control_list, control);

	gtk_object_destroy (GTK_OBJECT (esummary));
}

BonoboControl *
e_summary_factory_new_control (const char *uri,
			       const GNOME_Evolution_Shell shell)
{
  BonoboControl *control;
  GtkWidget *esummary;

  esummary = e_summary_new (shell);
  if (esummary == NULL)
    return NULL;

  gtk_widget_show (esummary);

  control = bonobo_control_new (esummary);

  if (control == NULL) {
    gtk_object_destroy (GTK_OBJECT (esummary));
    return NULL;
  }

  gtk_signal_connect (GTK_OBJECT (control), "activate",
		      control_activate_cb, esummary);

  gtk_signal_connect (GTK_OBJECT (control), "destroy",
		      control_destroy_cb, esummary);

  control_list = g_list_prepend (control_list, control);

  return control;
}
