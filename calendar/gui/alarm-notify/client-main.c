/* Evolution calendar - Command-line client for the alarm notification service
 *
 * Copyright (C) 2001 Ximian, Inc.
 *
 * Authors: Federico Mena-Quintero <federico@ximian.com>
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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <liboaf/liboaf.h>
#include <gnome.h>
#include <bonobo.h>
#include "evolution-calendar.h"



/* Requests that a calendar be added to the alarm notification service */
static void
add_calendar (GNOME_Evolution_Calendar_AlarmNotify an, const char *uri)
{
	CORBA_Environment ev;

	CORBA_exception_init (&ev);
	GNOME_Evolution_Calendar_AlarmNotify_addCalendar (an, uri, &ev);

	if (ev._major == CORBA_USER_EXCEPTION) {
		char *ex_id;

		ex_id = CORBA_exception_id (&ev);
		if (strcmp (ex_id, ex_GNOME_Evolution_Calendar_AlarmNotify_InvalidURI) == 0) {
			g_message ("add_calendar(): Invalid URI reported from the "
				   "alarm notification service");
			goto out;
		} else if (strcmp (ex_id,
				   ex_GNOME_Evolution_Calendar_AlarmNotify_BackendContactError)
			   == 0) {
			g_message ("add_calendar(): The alarm notification service could "
				   "not contact the backend");
			goto out;
		}
	}

	if (ev._major != CORBA_NO_EXCEPTION)
		g_message ("add_calendar(): Could not issue the addCalendar request");

 out:
	CORBA_exception_free (&ev);
}

/* Loads the calendars that the user has configured to be loaded */
static void
load_calendars (void)
{
	CORBA_Environment ev;
	GNOME_Evolution_Calendar_AlarmNotify an;
	char *base_uri;
	char *uri;

	CORBA_exception_init (&ev);
	an = oaf_activate_from_id ("OAFID:GNOME_Evolution_Calendar_AlarmNotify", 0, NULL, &ev);

	if (ev._major != CORBA_NO_EXCEPTION) {
		g_message ("load_calendars(): Could not activate the alarm notification service");
		CORBA_exception_free (&ev);
		exit (EXIT_FAILURE);
	}
	CORBA_exception_free (&ev);

	/* FIXME: this should be obtained from the configuration in the Wombat */

	base_uri = g_concat_dir_and_file (g_get_home_dir (), "evolution");

	uri = g_concat_dir_and_file (base_uri, "local/Calendar/calendar.ics");
	add_calendar (an, uri);
	g_free (uri);

	uri = g_concat_dir_and_file (base_uri, "local/Tasks/tasks.ics");
	add_calendar (an, uri);
	g_free (uri);

	g_free (base_uri);

	CORBA_exception_init (&ev);
	Bonobo_Unknown_unref (an, &ev);
	if (ev._major != CORBA_NO_EXCEPTION)
		g_message ("load_calendars(): Could not unref the alarm notification service");

	CORBA_exception_free (&ev);

	CORBA_exception_init (&ev);
	CORBA_Object_release (an, &ev);
	if (ev._major != CORBA_NO_EXCEPTION)
		g_message ("load_calendars(): Could not release the alarm notification service");

	CORBA_exception_free (&ev);
}

/* FIXME: handle the --die option */

int
main (int argc, char **argv)
{
	GnomeClient *client;
	int flags;
	gboolean launch_service;

	bindtextdomain (PACKAGE, EVOLUTION_LOCALEDIR);
	textdomain (PACKAGE);

	if (gnome_init_with_popt_table ("evolution-alarm-client", VERSION,
					argc, argv, oaf_popt_options, 0, NULL) != 0) {
		g_message ("main(): Could not initialize GNOME");
		exit (EXIT_FAILURE);
	}

	oaf_init (argc, argv);

	if (!bonobo_init (CORBA_OBJECT_NIL, CORBA_OBJECT_NIL, CORBA_OBJECT_NIL)) {
		g_message ("main(): Could not initialize Bonobo");
		exit (EXIT_FAILURE);
	}

	/* Ask the session manager to restart us */

	client = gnome_master_client ();
	flags = gnome_client_get_flags (client);

	if (flags & GNOME_CLIENT_IS_CONNECTED) {
		char *client_id;

		client_id = gnome_client_get_id (client);
		g_assert (client_id != NULL);

		launch_service = gnome_startup_acquire_token ("EVOLUTION_ALARM_NOTIFY",
							      client_id);

		if (launch_service) {
			char *args[3];

			args[0] = argv[0];
			args[2] = NULL;

			gnome_client_set_restart_style (client, GNOME_RESTART_ANYWAY);
			gnome_client_set_restart_command (client, 2, args);

			args[0] = argv[0];
			args[1] = "--die";
			args[2] = NULL;

			gnome_client_set_shutdown_command (client, 2, args);
		} else
			gnome_client_set_restart_style (client, GNOME_RESTART_NEVER);

		gnome_client_flush (client);
	} else
		launch_service = TRUE;

	if (!launch_service)
		return EXIT_SUCCESS;

	load_calendars ();

	return EXIT_SUCCESS;
}
