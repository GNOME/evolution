/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* control-factory.c
 *
 * Copyright (C) 2000  Ximian, Inc.
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
 * Author: Ettore Perazzoli <ettore@ximian.com>
 */

#include <config.h>
#include <gtk/gtksignal.h>
#include <glade/glade.h>
#include <bonobo/bonobo-control.h>
#include <bonobo/bonobo-generic-factory.h>
#include <bonobo/bonobo-persist-file.h>
#include <bonobo/bonobo-context.h>
#include <bonobo/bonobo-property-bag.h>
#include <glade/glade.h>

#include <cal-util/timeutil.h>
#include <gui/gnome-cal.h>
#include <gui/calendar-commands.h>

#include "control-factory.h"

#define PROPERTY_CALENDAR_URI      "folder_uri"
#define PROPERTY_CALENDAR_URI_IDX  1

#define PROPERTY_CALENDAR_VIEW     "view"
#define PROPERTY_CALENDAR_VIEW_IDX 2

#define CONTROL_FACTORY_ID   "OAFIID:GNOME_Evolution_Calendar_ControlFactory"


CORBA_Environment ev;
CORBA_ORB orb;


static void
control_activate_cb (BonoboControl *control, gboolean activate, gpointer data)
{
	GnomeCalendar *gcal;

	gcal = GNOME_CALENDAR (data);

	if (activate)
		calendar_control_activate (control, gcal);
	else
		calendar_control_deactivate (control, gcal);
}

static void
get_prop (BonoboPropertyBag *bag,
	  BonoboArg         *arg,
	  guint              arg_id,
	  CORBA_Environment *ev,
	  gpointer           user_data)
{
	GnomeCalendar *gcal = user_data;
	const char *uri;

	switch (arg_id) {

	case PROPERTY_CALENDAR_URI_IDX:
		uri = cal_client_get_uri (gnome_calendar_get_cal_client (gcal));
		BONOBO_ARG_SET_STRING (arg, uri);
		break;

	case PROPERTY_CALENDAR_VIEW_IDX:
		switch (gnome_calendar_get_view (gcal)) {
		case GNOME_CAL_DAY_VIEW:
			BONOBO_ARG_SET_STRING (arg, "day");
			break;
		case GNOME_CAL_WEEK_VIEW:
			BONOBO_ARG_SET_STRING (arg, "week");
			break;
		case GNOME_CAL_WORK_WEEK_VIEW:
			BONOBO_ARG_SET_STRING (arg, "workweek");
			break;
		case GNOME_CAL_MONTH_VIEW:
			BONOBO_ARG_SET_STRING (arg, "month");
			break;
		}
		break;

	default:
		g_warning ("Unhandled arg %d\n", arg_id);
	}
}


static void
set_prop (BonoboPropertyBag *bag,
	  const BonoboArg   *arg,
	  guint              arg_id,
	  CORBA_Environment *ev,
	  gpointer           user_data)
{
	GnomeCalendar *gcal = user_data;
	char *string;
	GnomeCalendarViewType view;

	switch (arg_id) {
	case PROPERTY_CALENDAR_URI_IDX:
		string = BONOBO_ARG_GET_STRING (arg);
		if (!gnome_calendar_open (gcal, string)) {
			char *msg;

			msg = g_strdup_printf (_("Could not open the folder in '%s'"), string);
			gnome_error_dialog_parented (
				msg,
				GTK_WINDOW (gtk_widget_get_toplevel (GTK_WIDGET (gcal))));
			g_free (msg);
		}
		break;

	case PROPERTY_CALENDAR_VIEW_IDX:
		string = BONOBO_ARG_GET_STRING (arg);
		if (!g_strcasecmp (string, "week"))
			view = GNOME_CAL_WEEK_VIEW;
		else if (!g_strcasecmp (string, "workweek"))
			view = GNOME_CAL_WORK_WEEK_VIEW;
		else if (!g_strcasecmp (string, "month"))
			view = GNOME_CAL_MONTH_VIEW;
		else
			view = GNOME_CAL_DAY_VIEW;

		/* This doesn't actually work, because the GalView
		 * comes along and resets the view. FIXME.
		 */
		gnome_calendar_set_view (gcal, view, FALSE, TRUE);
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
	bonobo_property_bag_add (pbag,
				 PROPERTY_CALENDAR_VIEW,
				 PROPERTY_CALENDAR_VIEW_IDX,
				 BONOBO_ARG_STRING,
				 NULL,
				 _("The type of view to show"),
				 0);

	bonobo_control_set_properties (control, bonobo_object_corba_objref (BONOBO_OBJECT (pbag)), NULL);
	bonobo_object_unref (BONOBO_OBJECT (pbag));
}

/* Callback factory function for calendar controls */
static BonoboObject *
control_factory_fn (BonoboGenericFactory *Factory, const char *id, void *data)
{
	BonoboControl *control;

	if (strcmp (id, CONTROL_FACTORY_ID) == 0) {
		control = control_factory_new_control ();
		if (control)
			return BONOBO_OBJECT (control);
		else
			return NULL;
	} else {
		g_warning ("Unknown ID in calendar control factory -- %s", id);
		return NULL;
	}
}


void
control_factory_init (void)
{
	static BonoboGenericFactory *factory = NULL;

	if (factory != NULL)
		return;

	factory = bonobo_generic_factory_new (CONTROL_FACTORY_ID, control_factory_fn, NULL);
	bonobo_running_context_auto_exit_unref (BONOBO_OBJECT (factory));
	
	if (factory == NULL)
		g_error ("I could not register a Calendar control factory.");
}

#if 0
static int
load_calendar (BonoboPersistFile *pf, const CORBA_char *filename, CORBA_Environment *ev, void *closure)
{
	GnomeCalendar *gcal = closure;
	
	return gnome_calendar_open (gcal, filename);
}

static int
save_calendar (BonoboPersistFile *pf, const CORBA_char *filename,
	       CORBA_Environment *ev,
	       void *closure)
{
	/* Do not know how to save stuff yet */
	return -1;
}

static void
calendar_persist_init (GnomeCalendar *gcal, BonoboControl *control)
{
	BonoboPersistFile *f;

	f = bonobo_persist_file_new (load_calendar, save_calendar, gcal);
	bonobo_object_add_interface (BONOBO_OBJECT (control), BONOBO_OBJECT (f));
}
#endif

BonoboControl *
control_factory_new_control (void)
{
	BonoboControl *control;
	GnomeCalendar *gcal;

	gcal = new_calendar ();
	if (!gcal)
		return NULL;

	gtk_widget_show (GTK_WIDGET (gcal));

	control = bonobo_control_new (GTK_WIDGET (gcal));
	if (!control) {
		g_message ("control_factory_fn(): could not create the control!");
		return NULL;
	}
	gtk_object_set_data (GTK_OBJECT (gcal), "control", control);

	calendar_properties_init (gcal, control);
#if 0
	calendar_persist_init (gcal, control);
#endif
					      
	gtk_signal_connect (GTK_OBJECT (control), "activate",
			    GTK_SIGNAL_FUNC (control_activate_cb), gcal);

	return control;
}
