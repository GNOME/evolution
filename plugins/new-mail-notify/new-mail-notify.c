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

#define GCONF_KEY "/apps/evolution/mail/notify/gen_dbus_msg"
#define DBUS_PATH "/org/gnome/evolution/mail/newmail"
#define DBUS_INTERFACE "org.gnome.evolution.mail.dbus.Signal"

GtkWidget *org_gnome_new_mail_config (EPlugin *ep, EConfigHookItemFactoryData *hook_data);
void org_gnome_new_mail_notify (EPlugin *ep, EMEventTargetFolder *t);
void org_gnome_message_reading_notify (EPlugin *ep, EMEventTargetMessage *t);

static void
toggled_cb (GtkWidget *widget, EConfig *config)
{
	EMConfigTargetPrefs *target = (EMConfigTargetPrefs *) config->target;
	
	/* Save the new setting to gconf */
	gconf_client_set_bool (target->gconf,
			       GCONF_KEY,
			       gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (widget)),
			       NULL);
}

GtkWidget *
org_gnome_new_mail_config (EPlugin *ep, EConfigHookItemFactoryData *hook_data)
{
	GtkWidget *notify;

	EMConfigTargetPrefs *target = (EMConfigTargetPrefs *) hook_data->config->target;
	
	/* Create the checkbox we will display, complete with mnemonic that is unique in the dialog */
	notify = gtk_check_button_new_with_mnemonic (_("_Generates a D-BUS message when new mail arrives"));

	/* Set the toggle button to the current gconf setting */
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (notify),
				      gconf_client_get_bool (target->gconf,
							     GCONF_KEY, NULL));

	/* Listen for the item being toggled on and off */
	g_signal_connect (GTK_TOGGLE_BUTTON (notify),
			  "toggled",
			  G_CALLBACK (toggled_cb),
			  hook_data->config);
	
	/* Pack the checkbox in the parent widget and show it */
	gtk_box_pack_start (GTK_BOX (hook_data->parent), notify, FALSE, FALSE, 0);
	gtk_widget_show (notify);
	
	return notify;
}

static void
send_dbus_message (const char *message_name, const char *data)
{
	GConfClient *client = gconf_client_get_default ();

	if (gconf_client_get_bool(client, GCONF_KEY, NULL)) {
		DBusConnection *bus;
		DBusError error;
		DBusMessage *message;

		/* Get a connection to the session bus */
		dbus_error_init (&error);
		bus = dbus_bus_get (DBUS_BUS_SESSION,
				    &error);

		if (!bus) {
			printf ("Failed to connect to the D-BUS daemon: %s\n", error.message);
			dbus_error_free (&error);
		}

		/* Set up this connection to work in a GLib event loop  */
		dbus_connection_setup_with_g_main (bus, NULL);

		/* Create a new message on the DBUS_INTERFACE */
		message = dbus_message_new_signal (DBUS_PATH,
						   DBUS_INTERFACE,
						   message_name);

		/* Appends the data as an argument to the message */
		dbus_message_append_args (message,
					  DBUS_TYPE_STRING, data,
					  DBUS_TYPE_INVALID);

		/* Sends the message */
		dbus_connection_send (bus,
				      message,
				      NULL);

		/* Frees the message */
		dbus_message_unref (message);

		/* printf("New message [%s] with arg [%s]!\n", message_name, data); */
	}

	g_object_unref (client);
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
