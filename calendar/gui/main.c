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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <gnome.h>
#include <glade/glade.h>
#include <bonobo.h>
#include <bonobo/bonobo-control.h>
#include <glade/glade.h>

#ifdef USING_OAF
#include <liboaf/liboaf.h>
#else
#include <libgnorba/gnorba.h>
#endif

#include <cal-util/timeutil.h>
#include <gui/alarm.h>
#include <gui/eventedit.h>
#include <gui/gnome-cal.h>
#include <gui/calendar-commands.h>


static void
init_bonobo (int *argc, char **argv)
{
#ifdef USING_OAF
	/* FIXME: VERSION instead of "0.0".  */
	gnome_init_with_popt_table ("evolution-calendar", "0.0",
				    *argc, argv, oaf_popt_options,
				    0, NULL);
	oaf_init (*argc, argv);
#else
	CORBA_Environment ev;

	CORBA_exception_init (&ev);
	gnome_CORBA_init_with_popt_table (
		"evolution-calendar", "0.0",
		argc, argv, NULL, 0, NULL, GNORBA_INIT_SERVER_FUNC, &ev);
	CORBA_exception_free (&ev);
#endif

	if (bonobo_init (CORBA_OBJECT_NIL, CORBA_OBJECT_NIL, CORBA_OBJECT_NIL) == FALSE)
		g_error (_("Could not initialize Bonobo"));

	glade_gnome_init ();
}


int
main (int argc, char **argv)
{
	init_bonobo (&argc, argv);
	glade_gnome_init ();
	alarm_init ();

	init_calendar ();

	//g_log_set_always_fatal ((GLogLevelFlags) 0xFFFF);
	g_log_set_always_fatal (G_LOG_LEVEL_ERROR |
				G_LOG_LEVEL_CRITICAL |
				G_LOG_LEVEL_WARNING);

	control_factory_init ();
	component_factory_init ();

	bonobo_main ();

	return 0;
}
