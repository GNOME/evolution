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

#include <gnome.h>
#include <bonobo/bonobo-main.h>
#include <bonobo/bonobo-object.h>
#include <bonobo/bonobo-generic-factory.h>
#include <bonobo/bonobo-control.h>

#include <liboaf/liboaf.h>

#include "gal/util/e-util.h"
#include "e-util/e-gui-utils.h"

#include "e-summary-factory.h"

#include "e-summary.h"
#include "Evolution.h"

#include <executive-summary-component-client.h>
#include <Executive-Summary.h>

#include <executive-summary.h>
#include <executive-summary-component.h>

static GList *control_list = NULL;

GtkWidget* embed_service (GtkWidget *widget,
			  ESummary *esummary);
static void update (ExecutiveSummary *summary,
		    const Evolution_SummaryComponent component,
		    const char *html,
		    ESummary *esummary);
static void set_title (ExecutiveSummary *summary,
		       const Evolution_SummaryComponent component,
		       const char *title,
		       void *closure);
static void flash (ExecutiveSummary *summary,
		   const Evolution_SummaryComponent component,
		   void *closure);

BonoboUIVerb verbs[] = {
	BONOBO_UI_UNSAFE_VERB ("AddService", embed_service),
	BONOBO_UI_VERB_END
};

static void
set_pixmap (BonoboUIComponent *component,
	    const char        *xml_path,
	    const char        *icon)
{
	char *path;
	GdkPixbuf *pixbuf;

	path = g_concat_dir_and_file (EVOLUTION_DATADIR "/images/evolution/buttons", icon);

	pixbuf = gdk_pixbuf_new_from_file (path);
	g_return_if_fail (pixbuf != NULL);

	bonobo_ui_util_set_pixbuf (component, xml_path, pixbuf);
	gdk_pixbuf_unref (pixbuf);
	g_free (path);
}

static void
update_pixmaps (BonoboUIComponent *component)
{
	set_pixmap (component, "/Toolbar/AddService", "add-service.png");
}

static void
control_activate (BonoboControl     *control,
		  BonoboUIComponent *ui_component,
		  ESummary          *esummary)
{
	Bonobo_UIContainer container;

	container = bonobo_control_get_remote_ui_handler (control);
	bonobo_ui_component_set_container (ui_component, container);
	bonobo_object_release_unref (container, NULL);

	bonobo_ui_component_add_verb_list_with_data (ui_component, verbs, esummary);
	
	bonobo_ui_component_freeze (ui_component, NULL);

	bonobo_ui_util_set_ui (ui_component, EVOLUTION_DATADIR,
			       "evolution-executive-summary.xml", 
			       "evolution-executive-summary");

	update_pixmaps (ui_component);
	bonobo_ui_component_thaw (ui_component, NULL);
}

static void
control_deactivate (BonoboControl     *control,
		    BonoboUIComponent *ui_component,
		    ESummary          *esummary)
{
	bonobo_ui_component_unset_container (ui_component);
}

static void
control_activate_cb (BonoboControl *control,
		     gboolean activate,
		     gpointer user_data)
{
	BonoboUIComponent *ui_component;
  
	ui_component = bonobo_control_get_ui_component (control);
	g_assert (ui_component != NULL);

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

static void 
update (ExecutiveSummary *summary,
	const Evolution_SummaryComponent component,
	const char *html,
	ESummary *esummary)
{
	e_summary_update_window (esummary, summary, html);
}

static void
set_title (ExecutiveSummary *summary,
	   const Evolution_SummaryComponent component,
	   const char *title,
	   void *closure)
{
	g_print ("Setting title to %s\n", title);
}

static void
flash (ExecutiveSummary *summary,
       const Evolution_SummaryComponent component,
       void *closure)
{
	g_print ("FLASH!\n");
}

/* A ********very********
   temporary function to embed something
*/
GtkWidget*
embed_service (GtkWidget *widget,
	       ESummary *esummary)
{
	ExecutiveSummaryComponentClient *client;
	ExecutiveSummary *summary;
	char *required_interfaces[2] = {"IDL:Evolution:SummaryComponent:1.0",
					NULL};
	char *obj_id;
	char *title;
	char *html;

	obj_id = bonobo_selector_select_id ("Select a service",
					    (const char **) required_interfaces);
	if (obj_id == NULL)
		return NULL;

	client = executive_summary_component_client_new (obj_id);

	if (client == NULL)
		return NULL;

	/* Set the owner */
	summary = executive_summary_new (set_title, flash, update, esummary);
	executive_summary_component_client_set_owner (client, summary);

	/* Create view */
	html = executive_summary_component_client_create_html_view (client, &title);
	e_summary_add_html_service (esummary, summary, client, html, title);
	g_free (html);

	e_summary_rebuild_page (esummary);
	return NULL;
}

BonoboControl *
e_summary_factory_new_control (const char *uri,
			       const Evolution_Shell shell)
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

  e_summary_rebuild_page (E_SUMMARY (esummary));
  return control;
}
