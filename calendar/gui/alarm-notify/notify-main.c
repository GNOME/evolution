/* Evolution calendar - Alarm notification service main file
 *
 * Copyright (C) 2000 Ximian, Inc.
 * Copyright (C) 2003 Novell, Inc.
 *
 * Authors: Federico Mena-Quintero <federico@ximian.com>
 *          Rodrigo Moya <rodrigo@ximian.com>
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

#include <string.h>
#include <glib.h>
#include <gtk/gtkmain.h>
#include <libgnome/gnome-i18n.h>
#include <libgnome/gnome-init.h>
#include <libgnome/gnome-sound.h>
#include <libgnomeui/gnome-client.h>
#include <libgnomeui/gnome-ui-init.h>
#include <libgnomevfs/gnome-vfs-init.h>
#include <glade/glade.h>
#include <bonobo/bonobo-main.h>
#include <bonobo/bonobo-generic-factory.h>
#include <bonobo-activation/bonobo-activation.h>
#include <libedataserver/e-source.h>
#include "e-util/e-passwords.h"
#include "e-util/e-icon-factory.h"
#include "alarm.h"
#include "alarm-queue.h"
#include "alarm-notify.h"
#include "config-data.h"



static BonoboGenericFactory *factory;

static AlarmNotify *alarm_notify_service = NULL;


/* Callback for the master client's "die" signal.  We must terminate the daemon
 * since the session is ending.
 */
static void
client_die_cb (GnomeClient *client)
{
	bonobo_main_quit ();
}

static gint
save_session_cb (GnomeClient *client, GnomeSaveStyle save_style, gint shutdown,
		 GnomeInteractStyle interact_style, gint fast, gpointer user_data)
{
	char *args[2];

	args[0] = EVOLUTION_LIBEXECDIR "/evolution-alarm-notify";
	args[1] = NULL;
	gnome_client_set_restart_command (client, 1, args);

	return TRUE;
}

/* Sees if a session manager is present.  If so, it tells the SM how to restart
 * the daemon when the session starts.  It also sets the die callback so that
 * the daemon can terminate properly when the session ends.
 */
static void
init_session (void)
{
	GnomeClient *master_client;

	master_client = gnome_master_client ();

	g_signal_connect (G_OBJECT (master_client), "die",
			  G_CALLBACK (client_die_cb), NULL);
	g_signal_connect (G_OBJECT (master_client), "save_yourself",
			  G_CALLBACK (save_session_cb), NULL);

	/* The daemon should always be started up by the session manager when
	 * the session starts.  The daemon will take care of loading whatever
	 * calendars it was told to load.
	 */
	gnome_client_set_restart_style (master_client, GNOME_RESTART_IF_RUNNING);
}

/* Factory function for the alarm notify service; just creates and references a
 * singleton service object.
 */
static BonoboObject *
alarm_notify_factory_fn (BonoboGenericFactory *factory, const char *component_id, void *data)
{
	if (!alarm_notify_service) {
		alarm_notify_service = alarm_notify_new ();
		g_assert (alarm_notify_service != NULL);
	}

	bonobo_object_ref (BONOBO_OBJECT (alarm_notify_service));

	return BONOBO_OBJECT (alarm_notify_service);
}

/* Creates the alarm notifier */
static gboolean
init_alarm_service (gpointer user_data)
{
	if (!alarm_notify_service) {
		alarm_notify_service = alarm_notify_new ();
		g_assert (alarm_notify_service != NULL);
	}
	
	return FALSE;
}

int
main (int argc, char **argv)
{
	bindtextdomain (GETTEXT_PACKAGE, EVOLUTION_LOCALEDIR);
	bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
	textdomain (GETTEXT_PACKAGE);

	gnome_program_init ("evolution-alarm-notify", VERSION, LIBGNOMEUI_MODULE, argc, argv, NULL);

	if (bonobo_init_full (&argc, argv, bonobo_activation_orb_get (),
			      CORBA_OBJECT_NIL, CORBA_OBJECT_NIL) == FALSE)
		g_error (_("Could not initialize Bonobo"));

	glade_init ();

	gnome_sound_init ("localhost");

	e_icon_factory_init ();

	factory = bonobo_generic_factory_new ("OAFIID:GNOME_Evolution_Calendar_AlarmNotify_Factory:" BASE_VERSION,
					      (BonoboFactoryCallback) alarm_notify_factory_fn, NULL);
	if (!factory)
		g_error (_("Could not create the alarm notify service factory"));

	init_session ();

	g_idle_add ((GSourceFunc) init_alarm_service, NULL);

	bonobo_main ();

	bonobo_object_unref (BONOBO_OBJECT (factory));
	factory = NULL;

	alarm_queue_done ();
	alarm_done ();

	if (alarm_notify_service)
		bonobo_object_unref (BONOBO_OBJECT (alarm_notify_service));

	e_passwords_shutdown ();
	gnome_sound_shutdown ();

	return 0;
}
