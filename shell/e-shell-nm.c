/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Shreyas Srinivasan <sshreyas@novell.com>
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * (C) Copyright 2005 Novell, Inc.
 */

#define DBUS_API_SUBJECT_TO_CHANGE 1

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdio.h>
#include <string.h>
#include <glib.h>
#include <e-shell.h>
#include <dbus/dbus.h>
#include <dbus/dbus-glib-lowlevel.h>
#include <dbus/dbus-glib.h>
#include <NetworkManager/NetworkManager.h>

static DBusConnection *dbus_connection;

/* Forward Declaration */
gboolean e_shell_dbus_initialize (EShell *shell);

static gboolean
reinit_dbus (EShell *shell)
{
	return !e_shell_dbus_initialize (shell);
}

static DBusHandlerResult
e_shell_network_monitor (DBusConnection *connection G_GNUC_UNUSED,
			 DBusMessage *message, void *user_data)
{
	DBusError error;
	const gchar *object;
	EShell *shell = user_data;
	EShellLineStatus line_status;
	gboolean device_active;

	dbus_error_init (&error);
	object = dbus_message_get_path (message);

	if (dbus_message_is_signal (message, DBUS_INTERFACE_LOCAL, "Disconnected") &&
		object && !strcmp (object, DBUS_PATH_LOCAL)) {
		dbus_connection_unref (dbus_connection);
		dbus_connection = NULL;

		g_timeout_add (3000, (GSourceFunc) reinit_dbus, shell);

		return DBUS_HANDLER_RESULT_HANDLED;
	}

	if (dbus_message_is_signal (message, NM_DBUS_INTERFACE, "DeviceNoLongerActive"))
		device_active = FALSE;
	else if (dbus_message_is_signal (message, NM_DBUS_INTERFACE, "DeviceNowActive"))
		device_active = TRUE;
	else
		return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;

	if (!dbus_message_get_args (message, &error, DBUS_TYPE_OBJECT_PATH,
				    &object, DBUS_TYPE_INVALID))
		return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;

	line_status = e_shell_get_line_status (shell);

	if (line_status == E_SHELL_LINE_STATUS_ONLINE && !device_active)
		e_shell_set_line_status (shell, E_SHELL_LINE_STATUS_FORCED_OFFLINE);
	else if (line_status == E_SHELL_LINE_STATUS_FORCED_OFFLINE && device_active)
		e_shell_set_line_status (shell, E_SHELL_LINE_STATUS_ONLINE);

	return DBUS_HANDLER_RESULT_HANDLED;
}

gboolean
e_shell_dbus_initialize (EShell *shell)
{
	DBusError error;

	g_return_val_if_fail (E_IS_SHELL (shell), FALSE);

	if (dbus_connection != NULL)
		return TRUE;

	dbus_error_init (&error);
	if (!(dbus_connection = dbus_bus_get (DBUS_BUS_SYSTEM, &error))) {
		g_warning ("could not get system bus: %s\n", error.message);
		dbus_error_free (&error);
		return FALSE;
	}

	dbus_connection_setup_with_g_main (dbus_connection, NULL);
	dbus_connection_set_exit_on_disconnect (dbus_connection, FALSE);

	if (!dbus_connection_add_filter (dbus_connection, e_shell_network_monitor, shell, NULL))
		goto exception;

	dbus_bus_add_match (dbus_connection,
			    "type='signal',"
			    "interface='" NM_DBUS_INTERFACE "',"
			    "sender='" NM_DBUS_SERVICE "',"
			    "path='" NM_DBUS_PATH "'", &error);
	if (dbus_error_is_set (&error)) {
		dbus_error_free (&error);
		goto exception;
	}

	return TRUE;

exception:

	dbus_connection_unref (dbus_connection);
	dbus_connection = NULL;

	return FALSE;
}
