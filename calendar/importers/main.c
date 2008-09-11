/*
 * Evolution calendar importer component
 *
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
 *		Rodrigo Moya <rodrigo@ximian.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif
#include <string.h>
#include <bonobo/bonobo-shlib-factory.h>
#include <bonobo/bonobo-context.h>
#include <bonobo/bonobo-main.h>
#include "evolution-calendar-importer.h"

#define IMPORTER_FACTORY_ID   "OAFIID:GNOME_Evolution_Calendar_ImporterFactory:" BASE_VERSION
#define ICALENDAR_IMPORTER_ID "OAFIID:GNOME_Evolution_Calendar_iCalendar_Importer:" BASE_VERSION
#define VCALENDAR_IMPORTER_ID "OAFIID:GNOME_Evolution_Calendar_vCalendar_Importer:" BASE_VERSION
#define GNOME_CALENDAR_IMPORTER_ID "OAFIID:GNOME_Evolution_Gnome_Calendar_Intelligent_Importer:" BASE_VERSION

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
		g_warning ("Component not supported by this factory");

	return object;
}

BONOBO_ACTIVATION_SHLIB_FACTORY (IMPORTER_FACTORY_ID, "Evolution Calendar importer factory", importer_factory_fn, NULL)
