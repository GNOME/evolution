/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* Evolution calendar importer component
 *
 * Authors: Rodrigo Moya <rodrigo@ximian.com>
 *
 * Copyright (C) 2001  Ximian, Inc.
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
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif
#include <bonobo/bonobo-generic-factory.h>
#include <bonobo/bonobo-context.h>
#include <bonobo/bonobo-main.h>
#include "evolution-calendar-importer.h"

#define IMPORTER_FACTORY_ID   "OAFIID:GNOME_Evolution_Calendar_ImporterFactory"
#define ICALENDAR_IMPORTER_ID "OAFIID:GNOME_Evolution_Calendar_iCalendar_Importer"
#define VCALENDAR_IMPORTER_ID "OAFIID:GNOME_Evolution_Calendar_vCalendar_Importer"
#define GNOME_CALENDAR_IMPORTER_ID "OAFIID:GNOME_Evolution_Gnome_Calendar_Intelligent_Importer"

static BonoboObject *
importer_factory_fn (BonoboGenericFactory *factory, const char *id, void *closure)
{
	BonoboObject *object = NULL;

	g_return_val_if_fail (id != NULL, NULL);

	if (!strcmp (id, ICALENDAR_IMPORTER_ID))
		object = ical_importer_new ();
	else if (!strcmp (id, VCALENDAR_IMPORTER_ID))
		object = vcal_importer_new ();
	else if (!strcmp (id, GNOME_CALENDAR_IMPORTER_ID))
		object = gnome_calendar_importer_new ();
	else
		g_warning ("Component not supporte by this factory");

	return object;
}

static void
init_importer (void)
{
	BonoboGenericFactory *factory;

	factory = bonobo_generic_factory_new_multi (IMPORTER_FACTORY_ID,
						    importer_factory_fn, NULL);
	if (factory == NULL) {
		g_error ("Unable to create factory");
		exit (0);
	}

	bonobo_running_context_auto_exit_unref (BONOBO_OBJECT (factory));
}

int
main (int argc, char *argv[])
{
	CORBA_ORB orb;

	bindtextdomain(PACKAGE, EVOLUTION_LOCALEDIR);
	textdomain(PACKAGE);
	
	gnome_init_with_popt_table ("evolution-calendar-importer",
				    VERSION, argc, argv, oaf_popt_options, 0,
				    NULL);
	orb = oaf_init (argc, argv);
	if (bonobo_init (orb, CORBA_OBJECT_NIL, CORBA_OBJECT_NIL) == FALSE) {
		g_error ("Could not initialize Bonobo.");
		exit (0);
	}

	init_importer ();
	bonobo_main ();

	return 0;
}
