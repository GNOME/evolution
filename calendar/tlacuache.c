/* Tlacuache - personal calendar server main module
 *
 * Copyright (C) 2000 Helix Code, Inc.
 *
 * Author: Federico Mena-Quintero <federico@helixcode.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307, USA.
 */

#include <config.h>
#include <libgnorba/gnorba.h>
#include <bonobo/gnome-bonobo.h>
#include "cal-factory.h"



/* The calendar factory */
static CalFactory *factory;

/* The alarms code needs this */
int debug_alarms = FALSE;



/* Creates and registers the calendar factory */
static gboolean
create_cal_factory (void)
{
	CORBA_Object object;
	CORBA_Environment ev;
	int result;

	factory = cal_factory_new ();
	if (!factory) {
		g_message ("create_cal_factory(): could not create the calendar factory!");
		return FALSE;
	}

	object = gnome_object_corba_objref (GNOME_OBJECT (factory));

	CORBA_exception_init (&ev);
	result = goad_server_register (CORBA_OBJECT_NIL,
				       object,
				       "calendar:cal-factory",
				       "object",
				       &ev);

	if (ev._major != CORBA_NO_EXCEPTION || result == -1) {
		g_message ("create_cal_factory(): could not register the calendar factory");
		CORBA_exception_free (&ev);
		return FALSE;
	} else if (result == -2) {
		g_message ("create_cal_factory(): a calendar factory is already registered");
		CORBA_exception_free (&ev);
		return FALSE;
	}

	CORBA_exception_free (&ev);
	return TRUE;
}

int
main (int argc, char **argv)
{
	CORBA_Environment ev;

	bindtextdomain (PACKAGE, GNOMELOCALEDIR);
	textdomain (PACKAGE);

	CORBA_exception_init (&ev);
	gnome_CORBA_init ("tlacuache", VERSION, &argc, argv, GNORBA_INIT_SERVER_FUNC, &ev);
	if (ev._major != CORBA_NO_EXCEPTION) {
		g_message ("main(): could not initialize the ORB");
		CORBA_exception_free (&ev);
		exit (1);
	}
	CORBA_exception_free (&ev);

	if (!bonobo_init (CORBA_OBJECT_NIL, CORBA_OBJECT_NIL, CORBA_OBJECT_NIL)) {
		g_message ("main(): could not initialize Bonobo");
		exit (1);
	}

	if (!create_cal_factory ())
		exit (1);

	bonobo_main ();
	return 0;
}
