/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* main.c
 * Test Service that counts the number of seconds since it was started.
 *
 * Authors: Iain Holmes <iain@helixcode.com>
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
#include <bonobo.h>

#include <evolution-services/executive-summary-component.h>
#include <evolution-services/executive-summary-component-view.h>

#include <liboaf/liboaf.h>

static int running_views = 0;

struct userdata {
	int i;
};

#define TEST_SERVICE_ID "OAFIID:evolution-summary-component-factory:test-service:0ea887d5-622b-4b8c-b525-18aa1cbe18a6"

static BonoboGenericFactory *factory = NULL;

/* The "do something interesting" function */
int
add_one (ExecutiveSummaryComponentView *view) 
{
	char *html;
	struct userdata *ud;
	
	/* Get the user data from the view */
	ud = gtk_object_get_data (GTK_OBJECT (view), "timer-data");
	if (ud == NULL) {
		g_warning ("No user data");
		return FALSE;
	}
	
	/* Generate the new html */
	html = g_strdup_printf ("Since you started this service<br>"
				"<center>%d</center><br>seconds have passed.", ud->i);
	
	/* Change the html on the view
	   which will tell the Executive Summary that something needs updating */
	executive_summary_component_view_set_html (view, html);
	
	/* executive_summary_component_view_set_html () makes a copy of the HTML
	   passed into it, so we don't need to keep it around */
	g_free (html);
	
	/* Do something "fun" */
	ud->i++;
	
	return TRUE;
}

void
view_destroyed (GtkObject *object,
		gpointer data)
{
	ExecutiveSummaryComponentView *view;
	struct userdata *ud;
	int id;
	
	/* Free the user data for this view*/
	ud = gtk_object_get_data (object, "timer-data");
	gtk_object_set_data (object, "timer-data", NULL);
	g_free (ud);
	
	/* Remove one running view */
	running_views--;
	
	/* If there are no running views left, quit */
	if (running_views <= 0)
		gtk_main_quit ();
}

/* Create the view:
   HTML only */
static void
create_view (ExecutiveSummaryComponent *component,
	     ExecutiveSummaryComponentView *view,
	     void *closure)
{
	char *html = "Since you started this service<br><center>0</center><br>seconds have passed.";
	struct userdata *ud;
	
	/* Create the userdata structure */
	ud = g_new (struct userdata, 1);
	
	ud->i = 1;
	executive_summary_component_view_construct (view, component, NULL,
						    html, "The Magic Counter",
						    "gnome-clock.png");
	/* Set the user data on the object */
	gtk_object_set_data (GTK_OBJECT (view), "timer-data", ud);
	
	/* Connect the the destroyed signal to find out 
	   when the view is destroyed */
	gtk_signal_connect (GTK_OBJECT (view), "destroy",
			    GTK_SIGNAL_FUNC (view_destroyed), NULL);
	
	/* Increase the number of running views */
	running_views++;
	
	/* Do something "interesting" once a second */
	gtk_timeout_add (1000, add_one, view);
}

static void
configure (ExecutiveSummaryComponent *component,
	   void *closure)
{
	GtkWidget *window, *label;
	
	window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
	label = gtk_label_new ("This is a configuration dialog.\nNo it really is");
	
	gtk_container_add (GTK_CONTAINER (window), label);
	gtk_widget_show_all (window);
}

static BonoboObject *
factory_fn (BonoboGenericFactory *_factory,
	    void *closure)
{
	ExecutiveSummaryComponent *component;
	
	/* Create an executive summary component for this factory */
	component = executive_summary_component_new (create_view,
						     configure,
						     NULL);
	return BONOBO_OBJECT (component);
}

void
test_service_factory_init (void)
{
	if (factory != NULL)
		return;
	
	/* Register the factory creation function and the IID */
	factory = bonobo_generic_factory_new (TEST_SERVICE_ID, factory_fn, NULL);
	if (factory == NULL) {
		g_warning ("Cannot initialize test service");
		exit (0);
	}
}

int
main (int argc, char **argv)
{
	CORBA_ORB orb;
	
	/* Init GNOME, oaf and bonobo */
	gnome_init_with_popt_table ("Test service", VERSION,
				    argc, argv, oaf_popt_options, 0, NULL);
	orb = oaf_init (argc, argv);
	
	if (bonobo_init (orb, CORBA_OBJECT_NIL, CORBA_OBJECT_NIL) == FALSE) {
		g_error ("Could not initialize Bonobo");
	}
	
	/* Register the factory */
	test_service_factory_init ();
	
	/* Enter main */
	bonobo_main ();
	
	return 0;
}

