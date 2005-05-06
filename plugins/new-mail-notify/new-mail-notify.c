/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 *  Author: Miguel Angel Lopez Hernandez <miguel@gulev.org.mx>
 *
 *  Copyright 2004 Novell, Inc.
 *
 *  This program is free software; you can redistribute it and/or
 *  modify it under the terms of version 2 of the GNU General Public
 *  License as published by the Free Software Foundation.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Street #330, Boston, MA 02111-1307, USA.
 *
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <string.h>
#include <glib.h>
#include <gtk/gtk.h>
#include <libgnome/gnome-i18n.h>
#include <gconf/gconf-client.h>
#include <e-util/e-config.h>
#include <mail/em-config.h>
#include <mail/em-event.h>
#include <dbus/dbus-glib.h>
#include <dbus/dbus-glib-lowlevel.h>
#include <camel/camel-folder.h>

#define DBUS_PATH "/org/gnome/evolution/mail/newmail"
#define DBUS_INTERFACE "org.gnome.evolution.mail.dbus.Signal"

GtkWidget *org_gnome_new_mail_config (EPlugin *ep, EConfigHookItemFactoryData *hook_data);
void org_gnome_new_mail_notify (EPlugin *ep, EMEventTargetFolder *t);
void org_gnome_message_reading_notify (EPlugin *ep, EMEventTargetMessage *t);

static DBusConnection *bus;

static void
send_dbus_message (const char *message_name, const char *data)
{
	DBusMessage *message;

	if (bus == NULL)
		return;

	/* Create a new message on the DBUS_INTERFACE */
	message = dbus_message_new_signal (DBUS_PATH,
					   DBUS_INTERFACE,
					   message_name);

	/* Appends the data as an argument to the message */
	dbus_message_append_args (message,
#if DBUS_VERSION >= 310
				  DBUS_TYPE_STRING, &data,
#else
				  DBUS_TYPE_STRING, data,
#endif	
				  DBUS_TYPE_INVALID);

	/* Sends the message */
	dbus_connection_send (bus,
			      message,
			      NULL);

	/* Frees the message */
	dbus_message_unref (message);
}

void
org_gnome_message_reading_notify (EPlugin *ep, EMEventTargetMessage *t)
{
	send_dbus_message ("MessageReading", t->folder->name);
}

void
org_gnome_new_mail_notify (EPlugin *ep, EMEventTargetFolder *t)
{
	send_dbus_message ("Newmail", t->uri);
}

int
e_plugin_lib_enable (EPluginLib *ep, int enable)
{
	if (enable) {
		DBusError      error;

		dbus_error_init (&error);
		bus = dbus_bus_get (DBUS_BUS_SESSION, &error);
		if (!bus) {
			g_warning("Failed to connect to the D-BUS daemon: %s\n", error.message);

			/* Could not determine address of the D-BUS session bus */
			/* Plugin will be disabled */
			dbus_error_free (&error);
			return 1;
		}

		/* Set up this connection to work in a GLib event loop  */
		dbus_connection_setup_with_g_main (bus, NULL);
	}
	/* else unref the bus if set? */

	return 0;
}

