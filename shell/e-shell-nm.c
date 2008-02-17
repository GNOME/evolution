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
#include <e-shell-window.h>
#include <Evolution.h>
#include <dbus/dbus.h>
#include <dbus/dbus-glib-lowlevel.h>
#include <dbus/dbus-glib.h>
#include <NetworkManager/NetworkManager.h>

typedef enum _ShellLineStatus {
	E_SHELL_LINE_DOWN,
	E_SHELL_LINE_UP
} ShellLineStatus;


static gboolean init_dbus (EShellWindow *window);

static DBusConnection *dbus_connection = NULL;


static gboolean
reinit_dbus (gpointer user_data)
{
	if (init_dbus (user_data))
		return FALSE;

	/* keep trying to re-establish dbus connection */

	return TRUE;
}

static DBusHandlerResult
e_shell_network_monitor (DBusConnection *connection G_GNUC_UNUSED,
			 DBusMessage *message, void *user_data)
{
	DBusError error;
	const char *object;
	ShellLineStatus status;
	EShellWindow *window = NULL;
	EShell *shell = NULL;
	GNOME_Evolution_ShellState shell_state;
	EShellLineStatus line_status;

 	if (!user_data || !E_IS_SHELL_WINDOW (user_data))
 		return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
 
 	window = E_SHELL_WINDOW (user_data);
 	shell = e_shell_window_peek_shell (window);

	dbus_error_init (&error);
	object = dbus_message_get_path (message);

	if (dbus_message_is_signal (message, DBUS_INTERFACE_LOCAL, "Disconnected") &&
		 strcmp (dbus_message_get_path (message), DBUS_PATH_LOCAL) == 0) {
		dbus_connection_unref (dbus_connection);
		dbus_connection = NULL;

		g_timeout_add (3000, reinit_dbus, window);

		return DBUS_HANDLER_RESULT_HANDLED;
	}

	if (dbus_message_is_signal (message, NM_DBUS_INTERFACE, "DeviceNoLongerActive"))
		status = E_SHELL_LINE_DOWN;
	else if (dbus_message_is_signal (message, NM_DBUS_INTERFACE, "DeviceNowActive"))
		status = E_SHELL_LINE_UP;
	else
		return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;

	if (!dbus_message_get_args (message, &error, DBUS_TYPE_OBJECT_PATH,
				    &object, DBUS_TYPE_INVALID))
		return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;

	line_status = e_shell_get_line_status (shell);

	if (line_status == E_SHELL_LINE_STATUS_ONLINE && status == E_SHELL_LINE_DOWN) {
		  shell_state = GNOME_Evolution_FORCED_OFFLINE;
		  e_shell_go_offline (shell, window, shell_state);
	} else if (line_status == E_SHELL_LINE_STATUS_OFFLINE && status == E_SHELL_LINE_UP) {
		  shell_state = GNOME_Evolution_USER_ONLINE;
		  e_shell_go_online (shell, window, shell_state);
	}

	return DBUS_HANDLER_RESULT_HANDLED;
}

static gboolean
init_dbus (EShellWindow *window)
{
	DBusError error;

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

	if (!dbus_connection_add_filter (dbus_connection, e_shell_network_monitor, window, NULL))
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

int e_shell_dbus_initialise (EShellWindow *window)
{
	g_type_init ();

	return init_dbus (window);
}

void e_shell_dbus_dispose (EShellWindow *window)
{
	//FIXME
	return;
}

