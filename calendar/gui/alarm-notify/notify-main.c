/* Evolution calendar - Alarm notification service main file
 *
 * Copyright (C) 2000 Ximian, Inc.
 * Copyright (C) 2000 Ximian, Inc.
 *
 * Author: Federico Mena-Quintero <federico@ximian.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of version 2 of the GNU General Public
 * License as published by the Free Software Foundation.
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

#include <glib.h>
#include <libgnome/gnome-defs.h>
#include <libgnome/gnome-i18n.h>
#include <libgnomeui/gnome-init.h>
#include <libgnomevfs/gnome-vfs-init.h>
#include <glade/glade.h>
#include <bonobo/bonobo-main.h>
#include <bonobo/bonobo-generic-factory.h>
#include <liboaf/liboaf.h>
#include "alarm.h"
#include "alarm-queue.h"
#include "alarm-notify.h"
#include "save.h"



static GnomeClient *master_client = NULL;

static BonoboGenericFactory *factory;

static AlarmNotify *alarm_notify_service;


/* Callback for the master client's "die" signal.  We must terminate the daemon
 * since the session is ending.
 */
static void
client_die_cb (GnomeClient *client)
{
	gtk_main_quit ();
}

/* Sees if a session manager is present.  If so, it tells the SM how to restart
 * the daemon when the session starts.  It also sets the die callback so that
 * the daemon can terminate properly when the session ends.
 */
static void
set_session_parameters (char **argv)
{
	int flags;
	char *args[2];

	master_client = gnome_master_client ();
	flags = gnome_client_get_flags (master_client);

	if (!(flags & GNOME_CLIENT_IS_CONNECTED))
		return;

	/* The daemon should always be started up by the session manager when
	 * the session starts.  The daemon will take care of loading whatever
	 * calendars it was told to load.
	 */
	gnome_client_set_restart_style (master_client, GNOME_RESTART_ANYWAY);

	args[0] = argv[0];
	args[1] = NULL;

	gnome_client_set_restart_command (master_client, 1, args);

	gtk_signal_connect (GTK_OBJECT (master_client), "die",
			    GTK_SIGNAL_FUNC (client_die_cb), NULL);
}

/* Creates the singleton alarm notify service object */
static gboolean
init_alarm_notify_service (void)
{
	g_assert (alarm_notify_service == NULL);

	alarm_notify_service = alarm_notify_new ();
	return (alarm_notify_service != NULL);
}

/* Factory function for the alarm notify service; just creates and references a
 * singleton service object.
 */
static BonoboObject *
alarm_notify_factory_fn (BonoboGenericFactory *factory, void *data)
{
	g_assert (alarm_notify_service != NULL);

	bonobo_object_ref (BONOBO_OBJECT (alarm_notify_service));
	return BONOBO_OBJECT (alarm_notify_service);
}

/* Loads the calendars that the alarm daemon has been told to load in the past */
static gboolean
load_calendars (gpointer user_data)
{
	GPtrArray *uris;
	int i;

	uris = get_calendars_to_load ();
	if (!uris) {
		g_message ("load_calendars(): Could not get the list of calendars to load");
		return TRUE; /* should we continue retrying? */;
	}

	for (i = 0; i < uris->len; i++) {
		char *uri;
		CORBA_Environment ev;

		uri = uris->pdata[i];

		CORBA_exception_init (&ev);
		alarm_notify_add_calendar (alarm_notify_service, uri, FALSE, &ev);

		if (ev._major == CORBA_USER_EXCEPTION) {
			char *ex_id;
			
			ex_id = CORBA_exception_id (&ev);
			if (strcmp (ex_id, ex_GNOME_Evolution_Calendar_AlarmNotify_InvalidURI) == 0)
				g_message ("load_calendars(): Invalid URI `%s'; will not load "
					   "that calendar.", uri);
			else if (strcmp (ex_id,
					 ex_GNOME_Evolution_Calendar_AlarmNotify_BackendContactError)
				 == 0)
				g_message ("load_calendars(): Could not contact the backend "
					   "while trying to load `%s'", uri);
		} else if (ev._major != CORBA_NO_EXCEPTION)
			g_message ("load_calendars(): Exception while loading calendar `%s'", uri);

		CORBA_exception_free (&ev);

		g_free (uri);
	}

	g_ptr_array_free (uris, TRUE);

	return FALSE;
}

int
main (int argc, char **argv)
{
	free (malloc (8));

	bindtextdomain (PACKAGE, EVOLUTION_LOCALEDIR);
	textdomain (PACKAGE);

	if (gnome_init_with_popt_table ("evolution-alarm-notify", VERSION, argc, argv,
					oaf_popt_options, 0, NULL) != 0)
		g_error (_("Could not initialize GNOME"));

	oaf_init (argc, argv);

	if (bonobo_init (CORBA_OBJECT_NIL, CORBA_OBJECT_NIL, CORBA_OBJECT_NIL) == FALSE)
		g_error (_("Could not initialize Bonobo"));

	if (!gnome_vfs_init ())
		g_error (_("Could not initialize gnome-vfs"));

	glade_gnome_init ();

	alarm_queue_init ();

	if (!init_alarm_notify_service ())
		g_error (_("Could not create the alarm notify service"));

	factory = bonobo_generic_factory_new ("OAFIID:GNOME_Evolution_Calendar_AlarmNotify_Factory",
					      alarm_notify_factory_fn, NULL);
	if (!factory)
		g_error (_("Could not create the alarm notify service factory"));

	set_session_parameters (argv);

	g_idle_add ((GSourceFunc) load_calendars, NULL);

	bonobo_main ();

	bonobo_object_unref (BONOBO_OBJECT (factory));
	factory = NULL;

	/* FIXME: free the alarm_notify_service */

	alarm_queue_done ();
	alarm_done ();

	gnome_vfs_shutdown ();

	return 0;
}
