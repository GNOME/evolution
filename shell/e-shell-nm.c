/*
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
 *		Shreyas Srinivasan <sshreyas@novell.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#define DBUS_API_SUBJECT_TO_CHANGE 1

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdio.h>
#include <string.h>
#include <glib.h>
#include <e-shell.h>
#include <Evolution.h>
#include <dbus/dbus.h>
#include <dbus/dbus-glib-lowlevel.h>
#include <dbus/dbus-glib.h>
#include <NetworkManager/NetworkManager.h>

gboolean e_shell_dbus_initialise (EShell *shell);

static DBusConnection *dbus_connection = NULL;

static gboolean
reinit_dbus (gpointer user_data)
{
	EShell *shell = user_data;

	if (e_shell_dbus_initialise (shell))
		return FALSE;

	/* keep trying to re-establish dbus connection */

	return TRUE;
}

static DBusHandlerResult
e_shell_network_monitor (DBusConnection *connection G_GNUC_UNUSED,
                         DBusMessage *message, gpointer user_data)
{
	const gchar *object;
	EShell *shell = user_data;
	GNOME_Evolution_ShellState shell_state;
	EShellLineStatus line_status;
	DBusError error = DBUS_ERROR_INIT;
	guint32 state;

	object = dbus_message_get_path (message);

	if (dbus_message_is_signal (message, DBUS_INTERFACE_LOCAL, "Disconnected") &&
		object && !strcmp (object, DBUS_PATH_LOCAL)) {
		dbus_connection_unref (dbus_connection);
		dbus_connection = NULL;

		g_timeout_add_seconds (3, reinit_dbus, shell);

		return DBUS_HANDLER_RESULT_HANDLED;
	}

	if (!dbus_message_is_signal (message, NM_DBUS_INTERFACE, "StateChanged"))
		return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;

	dbus_message_get_args (
		message, &error,
		DBUS_TYPE_UINT32, &state,
		DBUS_TYPE_INVALID);

	if (dbus_error_is_set (&error)) {
		g_warning ("%s", error.message);
		return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
	}

	line_status = e_shell_get_line_status (shell);

	if (line_status == E_SHELL_LINE_STATUS_ONLINE && (state == NM_STATE_ASLEEP || state == NM_STATE_DISCONNECTED)) {
		shell_state = GNOME_Evolution_FORCED_OFFLINE;
		e_shell_set_line_status (shell, shell_state);
	} else if (line_status == E_SHELL_LINE_STATUS_FORCED_OFFLINE && state == NM_STATE_CONNECTED) {
		shell_state = GNOME_Evolution_USER_ONLINE;
		e_shell_set_line_status (shell, shell_state);
	}

	return DBUS_HANDLER_RESULT_HANDLED;
}

static void
check_initial_state (EShell *shell)
{
	DBusMessage *message = NULL, *response = NULL;
	guint32 state = -1;
	DBusError error = DBUS_ERROR_INIT;

	message = dbus_message_new_method_call (NM_DBUS_SERVICE, NM_DBUS_PATH, NM_DBUS_INTERFACE, "state");

	/* assuming this should be safe to call syncronously */
	response = dbus_connection_send_with_reply_and_block (dbus_connection, message, 100, &error);

	if (response)
		dbus_message_get_args (response, &error, DBUS_TYPE_UINT32, &state, DBUS_TYPE_INVALID);
	else {
		g_warning ("%s \n", error.message);
		dbus_error_free (&error);
		return;
	}

	/* update the state only in the absence of network connection else let the old state prevail */
	if (state == NM_STATE_DISCONNECTED)
		e_shell_set_line_status (shell, GNOME_Evolution_FORCED_OFFLINE);

	dbus_message_unref (message);
	dbus_message_unref (response);
}

gboolean
e_shell_dbus_initialise (EShell *shell)
{
	DBusError error = DBUS_ERROR_INIT;

	if (dbus_connection != NULL)
		return TRUE;

	dbus_connection = dbus_bus_get (DBUS_BUS_SYSTEM, &error);
	if (dbus_connection == NULL) {
		g_warning ("%s", error.message);
		dbus_error_free (&error);
		return FALSE;
	}

	dbus_connection_setup_with_g_main (dbus_connection, NULL);
	dbus_connection_set_exit_on_disconnect (dbus_connection, FALSE);

	if (!dbus_connection_add_filter (dbus_connection, e_shell_network_monitor, shell, NULL))
		goto exception;

	check_initial_state (shell);

	dbus_bus_add_match (dbus_connection,
			    "type='signal',"
			    "interface='" NM_DBUS_INTERFACE "',"
			    "sender='" NM_DBUS_SERVICE "',"
			    "path='" NM_DBUS_PATH "'", &error);
	if (dbus_error_is_set (&error)) {
		g_warning ("%s", error.message);
		dbus_error_free (&error);
		goto exception;
	}

	return TRUE;

exception:

	dbus_connection_unref (dbus_connection);
	dbus_connection = NULL;

	return FALSE;
}
