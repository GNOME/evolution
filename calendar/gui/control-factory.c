/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) version 3.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with the program; if not, see <http://www.gnu.org/licenses/>
 *
 *
 * Authors:
 *		Ettore Perazzoli <ettore@ximian.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#include <config.h>
#include <string.h>
#include <gtk/gtk.h>
#include <glade/glade.h>
#include <bonobo/bonobo-control.h>
#include <bonobo/bonobo-generic-factory.h>
#include <bonobo/bonobo-persist-file.h>
#include <bonobo/bonobo-context.h>
#include <glade/glade.h>

#include <libecal/e-cal-time-util.h>
#include <gui/gnome-cal.h>
#include <gui/calendar-commands.h>
#include <gui/calendar-config.h>

#include "control-factory.h"

/* Are these supposed to be global or static? */
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

	g_signal_connect (control, "activate", G_CALLBACK (control_activate_cb), gcal);

	return control;
}
