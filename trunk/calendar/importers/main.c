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
