/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* control-factory.c
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
 * Author: Ettore Perazzoli
 */

#include <config.h>
#include <gnome.h>
#include <glade/glade.h>
#include <bonobo.h>
#include <bonobo/bonobo-control.h>
#include <glade/glade.h>

#include <liboaf/liboaf.h>

#include <cal-util/timeutil.h>
#include <gui/alarm.h>
#include <gui/gnome-cal.h>
#include <gui/calendar-commands.h>

#include "control-factory.h"

#define PROPERTY_CALENDAR_URI "folder_uri"

#define PROPERTY_CALENDAR_URI_IDX 1

#define CONTROL_FACTORY_ID   "OAFIID:control-factory:calendar:f4f90989-0f50-4af2-ad94-8bbdf331f0bc"


CORBA_Environment ev;
CORBA_ORB orb;


static void
control_activate_cb (BonoboControl *control,
		     gboolean activate,
		     gpointer user_data)
{
	if (activate)
		calendar_control_activate (control, user_data);
	else
		calendar_control_deactivate (control);
}


static void
get_prop (BonoboPropertyBag *bag,
	  BonoboArg         *arg,
	  guint              arg_id,
	  gpointer           user_data)
{
	/*GnomeCalendar *gcal = user_data;*/

	switch (arg_id) {

	case PROPERTY_CALENDAR_URI_IDX:
		/*
		if (fb && fb->uri)
			BONOBO_ARG_SET_STRING (arg, fb->uri);
		else
			BONOBO_ARG_SET_STRING (arg, "");
		*/
		break;

	default:
		g_warning ("Unhandled arg %d\n", arg_id);
	}
}


static void
set_prop (BonoboPropertyBag *bag,
	  const BonoboArg   *arg,
	  guint              arg_id,
	  gpointer           user_data)
{
	GnomeCalendar *gcal = user_data;
	char *filename;

	switch (arg_id) {

	case PROPERTY_CALENDAR_URI_IDX:
		printf ("set_prop: '%s'\n", BONOBO_ARG_GET_STRING (arg));
		filename = g_strdup_printf ("%s/calendar.vcf",
					    BONOBO_ARG_GET_STRING (arg));
		calendar_set_uri (gcal, filename);
		g_free (filename);
		break;

	default:
		g_warning ("Unhandled arg %d\n", arg_id);
		break;
	}
}


static void
calendar_properties_init (GnomeCalendar *gcal, BonoboControl *control)
{
	BonoboPropertyBag *pbag;

	pbag = bonobo_property_bag_new (get_prop, set_prop, gcal);

	bonobo_property_bag_add (pbag,
				 PROPERTY_CALENDAR_URI,
				 PROPERTY_CALENDAR_URI_IDX,
				 BONOBO_ARG_STRING,
				 NULL,
				 _("The URI that the calendar will display"),
				 0);

	bonobo_control_set_property_bag (control, pbag);
}

/* Callback factory function for calendar controls */
static BonoboObject *
control_factory_fn (BonoboGenericFactory *Factory, void *data)
{
	BonoboControl *control;

	control = control_factory_new_control ();

	if (control)
		return BONOBO_OBJECT (control);
	else
		return NULL;
}


void
control_factory_init (void)
{
	static BonoboGenericFactory *factory = NULL;

	if (factory != NULL)
		return;

	puts ("XXXXXX - initializing calendar factory!!!");

	factory = bonobo_generic_factory_new (CONTROL_FACTORY_ID, control_factory_fn, NULL);

	if (factory == NULL)
		g_error ("I could not register a Calendar control factory.");
}


BonoboControl *
control_factory_new_control (void)
{
	BonoboControl *control;
	GnomeCalendar *gcal;

	gcal = new_calendar (full_name, NULL, 0);
	gtk_widget_show (GTK_WIDGET (gcal));

	control = bonobo_control_new (GTK_WIDGET (gcal));
	if (!control) {
		g_message ("control_factory_fn(): could not create the control!");
		return NULL;
	}

	calendar_properties_init (gcal, control);

	gtk_signal_connect (GTK_OBJECT (control), "activate",
			    GTK_SIGNAL_FUNC (control_activate_cb), gcal);

	return control;
}
