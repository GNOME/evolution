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
#include <evolution-services/executive-summary-html-view.h>

#include <liboaf/liboaf.h>

enum {
	PROPERTY_TITLE,
	PROPERTY_ICON
};

struct _UserData {
	char *title;
	char *icon;
};
typedef struct _UserData UserData;

static int running_views = 0;

#define TEST_SERVICE_ID "OAFIID:GNOME_Evolution_Summary_test_ComponentFactory"

static BonoboGenericFactory *factory = NULL;

/* PersistStream callbacks */
static void
load_from_stream (BonoboPersistStream *ps,
		  Bonobo_Stream stream,
		  Bonobo_Persist_ContentType type,
		  gpointer data,
		  CORBA_Environment *ev)
{
	char *str;
	
	if (*type && g_strcasecmp (type, "application/x-test-service") != 0) {
		CORBA_exception_set (ev, CORBA_USER_EXCEPTION,
				     ex_Bonobo_Persist_WrongDataType, NULL);
		return;
	}

	bonobo_stream_client_read_string (stream, &str, ev);
	if (ev->_major != CORBA_NO_EXCEPTION || str == NULL) {
		CORBA_exception_set (ev, CORBA_USER_EXCEPTION,
				     ex_Bonobo_Persist_WrongDataType, NULL);
		return;
	}

	g_print ("Restoring with :%s\n", str);
	g_free (str);
}

static void
save_to_stream (BonoboPersistStream *ps,
		const Bonobo_Stream stream,
		Bonobo_Persist_ContentType type,
		gpointer data,
		CORBA_Environment *ev)
{

	if (*type && g_strcasecmp (type, "application/x-test-service") != 0) {
		CORBA_exception_set (ev, CORBA_USER_EXCEPTION,
				     ex_Bonobo_Persist_WrongDataType, NULL);
		return;
	}

	bonobo_stream_client_printf (stream, TRUE, ev, "Yo yo yo");
	if (ev->_major != CORBA_NO_EXCEPTION)
		return;
}

static Bonobo_Persist_ContentTypeList *
content_types (BonoboPersistStream *ps,
	       void *closure,
	       CORBA_Environment *ev)
{
	return bonobo_persist_generate_content_types (1, "application/x-test-service");
}

/* PropertyControl callback */

/* Propertybag set/get functions */
static void
set_property (BonoboPropertyBag *bag,
	      const BonoboArg *arg,
	      guint arg_id,
	      gpointer user_data)
{
	switch (arg_id) {
	case PROPERTY_TITLE:
		g_print ("Setting title.\n");
		break;

	case PROPERTY_ICON:
		g_print ("Setting icon.\n");
		break;

	default:
		break;
	}
}

static void
get_property (BonoboPropertyBag *bag,
	      BonoboArg *arg,
	      guint arg_id,
	      gpointer user_data)
{
	UserData *ud = (UserData *) user_data;

	switch (arg_id) {
	case PROPERTY_TITLE:
		BONOBO_ARG_SET_STRING (arg, ud->title);
		break;

	case PROPERTY_ICON:
		BONOBO_ARG_SET_STRING (arg, ud->icon);
		break;

	default:
		break;
	}
}

static void
component_destroyed (GtkObject *object,
		     gpointer data)
{
	UserData *ud = (UserData *) data;
	/* Free the UserData structure */
	g_free (ud->title);
	g_free (ud->icon);
	g_free (ud);

	running_views--;

	g_print ("Destroy!\n");
	if (running_views <= 0) {
		bonobo_object_unref (BONOBO_OBJECT (factory));
		gtk_main_quit ();
	}
}

static BonoboObject *
create_view (ExecutiveSummaryComponentFactory *_factory,
	     void *closure)
{
	BonoboObject *component, *view;
	BonoboPersistStream *stream;
	BonoboPropertyBag *bag;
	UserData *ud;

	/* Create the component object */
	component = executive_summary_component_new ();

	/* Create the UserData structure and fill it */
	ud = g_new (UserData, 1);
	ud->title = g_strdup ("Hello World!");
	ud->icon = g_strdup ("apple-red.png");

	gtk_signal_connect (GTK_OBJECT (component), "destroy",
			    GTK_SIGNAL_FUNC (component_destroyed), ud);

	/* Now create the aggregate objects. For a "service"
	   either a Summary::HTMLView or Bonobo::Control are required.
	   Other supported agreggate objects are 
	   PersistStream: For saving and restoring the component.
	   PropertyBag: To set the icon and title and other properties
	   PropertyControl: To produce a control to configure the service.
	
	   To aggregate the objects
	   i) Create the objects using their creation functions
	   ii) Use bonobo_object_add_interface ().
	*/
	
	/* The Summary::HTMLView interface */
	view = executive_summary_html_view_new ();
	/* Set the default HTML */
	executive_summary_html_view_set_html (EXECUTIVE_SUMMARY_HTML_VIEW (view),
					      "<B>Hello World</b>");
	
	bonobo_object_add_interface (component, view);

	/* Add the Bonobo::PropertyBag interface */
	bag = bonobo_property_bag_new (get_property, set_property, ud);
	/* Add the properties. There should be 2:
	   window_title: For the window title.
	   window_icon: For the window icon.
	*/
	bonobo_property_bag_add (bag,
				 "window_title", PROPERTY_TITLE,
				 BONOBO_ARG_STRING,
				 NULL,
				 "The title of this components window", 0);
	bonobo_property_bag_add (bag,
				 "window_icon", PROPERTY_ICON,
				 BONOBO_ARG_STRING,
				 NULL,
				 "The icon for this component's window", 0);

	/* Now add the interface */
	bonobo_object_add_interface (component, BONOBO_OBJECT(bag));

	/* Add the Bonobo::PersistStream interface */
	stream = bonobo_persist_stream_new (load_from_stream, save_to_stream,
					    NULL, content_types, NULL);
	bonobo_object_add_interface (component, BONOBO_OBJECT(stream));

	running_views++;
	/* Return the ExecutiveSummaryComponent object */
	return component;
}

static BonoboObject *
factory_fn (BonoboGenericFactory *generic_factory,
	    void *closure)
{
	BonoboObject *_factory;
	
	/* Create an executive summary component factory */
	_factory = executive_summary_component_factory_new (create_view, NULL);
	return _factory;
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

