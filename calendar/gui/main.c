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

#include <glib.h>
#include <libgnome/gnome-defs.h>
#include <libgnome/gnome-i18n.h>
#include <libgnomeui/gnome-init.h>
#include <glade/glade.h>

#include <liboaf/liboaf.h>
#include <bonobo/bonobo-main.h>

#include <gal/widgets/e-cursors.h>

#include "alarm-notify/alarm.h"
#include "calendar-commands.h"
#include "calendar-config.h"
#include "component-factory.h"
#include "control-factory.h"
#include "e-itip-control.h"
#include "tasks-control-factory.h"

static void
init_bonobo (int argc, char **argv)
{
	if (gnome_init_with_popt_table ("evolution-calendar", VERSION, argc, argv,
					oaf_popt_options, 0, NULL) != 0)
		g_error (_("Could not initialize GNOME"));

	oaf_init (argc, argv);

	if (bonobo_init (CORBA_OBJECT_NIL, CORBA_OBJECT_NIL, CORBA_OBJECT_NIL) == FALSE)
		g_error (_("Could not initialize Bonobo"));
}


int
main (int argc, char **argv)
{
	bindtextdomain(PACKAGE, EVOLUTION_LOCALEDIR);
	textdomain(PACKAGE);

	init_bonobo (argc, argv);
	glade_gnome_init ();
	alarm_init ();
	e_cursors_init ();

#if 0
	//g_log_set_always_fatal ((GLogLevelFlags) 0xFFFF);
	g_log_set_always_fatal (G_LOG_LEVEL_ERROR |
				G_LOG_LEVEL_CRITICAL |
				G_LOG_LEVEL_WARNING);
#endif

	control_factory_init ();
	component_factory_init ();
	e_itip_control_factory_init ();
	tasks_control_factory_init ();

	bonobo_main ();

	alarm_done ();
	calendar_config_write_on_exit ();

	return 0;
}
