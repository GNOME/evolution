/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* control-factory.c
 *
 * Copyright (C) 2000, 2001, 2002, 2003  Ximian, Inc.
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
#include <string.h>
#include <gtk/gtksignal.h>
#include <glade/glade.h>
#include <bonobo/bonobo-control.h>
#include <bonobo/bonobo-generic-factory.h>
#include <bonobo/bonobo-persist-file.h>
#include <bonobo/bonobo-context.h>
#include <bonobo/bonobo-property-bag.h>
#include <glade/glade.h>
#include <libgnomeui/gnome-dialog-util.h>

#include <libecal/e-cal-time-util.h>
#include <gui/gnome-cal.h>
#include <gui/calendar-commands.h>
#include <gui/calendar-config.h>

#include "control-factory.h"

#define PROPERTY_CALENDAR_URI      "folder_uri"
#define PROPERTY_CALENDAR_URI_IDX  1

#define PROPERTY_CALENDAR_VIEW     "view"
#define PROPERTY_CALENDAR_VIEW_IDX 2


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
	GnomeCalendar *gcal;
	const char *uri;
	BonoboControl *control = user_data;

	gcal = (GnomeCalendar *) bonobo_control_get_widget (control);

	switch (arg_id) {

	case PROPERTY_CALENDAR_URI_IDX:
		uri = e_cal_get_uri (e_cal_model_get_default_client (gnome_calendar_get_calendar_model (gcal)));
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
		case GNOME_CAL_LIST_VIEW:
			BONOBO_ARG_SET_STRING (arg, "list");
			break;
		default:
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
	GnomeCalendar *gcal;
	char *string;
	GnomeCalendarViewType view;
	ESource *source;
	ESourceGroup *group;
	BonoboControl *control = user_data;

	gcal = (GnomeCalendar *) bonobo_control_get_widget (control);

	switch (arg_id) {
	case PROPERTY_CALENDAR_URI_IDX:
		string = BONOBO_ARG_GET_STRING (arg);

		group = e_source_group_new ("", string);
		source = e_source_new ("", "");
		e_source_set_group (source, group);

		if (gnome_calendar_add_event_source (gcal, source)) {
			calendar_control_sensitize_calendar_commands (control, gcal, TRUE);
		} else {
			char *msg;

			msg = g_strdup_printf (_("Could not open the folder in '%s'"), string);
			gnome_error_dialog_parented (
				msg,
				GTK_WINDOW (gtk_widget_get_toplevel (GTK_WIDGET (gcal))));
			g_free (msg);
		}

		g_object_unref (source);
		g_object_unref (group);

		break;

	case PROPERTY_CALENDAR_VIEW_IDX:
		string = BONOBO_ARG_GET_STRING (arg);
		if (!strcmp (string, "week"))
			view = GNOME_CAL_WEEK_VIEW;
		else if (!strcmp (string, "workweek"))
			view = GNOME_CAL_WORK_WEEK_VIEW;
		else if (!strcmp (string, "month"))
			view = GNOME_CAL_MONTH_VIEW;
		else if (!strcmp (string, "day"))
			view = GNOME_CAL_DAY_VIEW;
		else
			view = calendar_config_get_default_view ();

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

	pbag = bonobo_property_bag_new (get_prop, set_prop, control);

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

BonoboControl *
control_factory_new_control (void)
{
	BonoboControl *control;
	GnomeCalendar *gcal;

	gcal = GNOME_CALENDAR (gnome_calendar_new ());
	if (!gcal)
		return NULL;

	gtk_widget_show (GTK_WIDGET (gcal));

	control = bonobo_control_new (GTK_WIDGET (gcal));
	if (!control) {
		g_message ("control_factory_fn(): could not create the control!");
		return NULL;
	}
	g_object_set_data (G_OBJECT (gcal), "control", control);

	calendar_properties_init (gcal, control);
					      
	g_signal_connect (control, "activate", G_CALLBACK (control_activate_cb), gcal);

	return control;
}
