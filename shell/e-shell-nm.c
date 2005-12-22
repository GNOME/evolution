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
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 *
 * (C) Copyright 2005 Novell, Inc.
 */

#define DBUS_API_SUBJECT_TO_CHANGE 1

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdio.h>
#include <glib.h>
#include <e-shell-window.h>
#include <Evolution.h>
#include <dbus/dbus.h>
#include <dbus/dbus-glib-lowlevel.h>
#include <dbus/dbus-glib.h>
#include <NetworkManager/NetworkManager.h>

int shell_dbus_initialize (EShellWindow *window);

enum _ShellLineStatus {
    E_SHELL_LINE_DOWN,
    E_SHELL_LINE_UP
};


typedef enum _ShellLineStatus ShellLineStatus;

static DBusHandlerResult e_shell_network_monitor (DBusConnection *connection G_GNUC_UNUSED,
				      DBusMessage *message,
				     void* user_data)
{
	DBusError error;
	const char *object;
	ShellLineStatus status;
	EShellWindow *window = E_SHELL_WINDOW (user_data);
	EShell *shell = e_shell_window_peek_shell ((EShellWindow *) user_data);
	GNOME_Evolution_ShellState shell_state;
	EShellLineStatus line_status;
	
	dbus_error_init (&error);
	object = dbus_message_get_path (message);

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

int e_shell_dbus_initialise (EShellWindow *window)
{
  	DBusConnection *connection;
	DBusError error;
	
	g_type_init ();

	dbus_error_init (&error);
	connection = dbus_bus_get (DBUS_BUS_SYSTEM, &error);
	if (connection == NULL) {
		dbus_error_free (&error);
		return FALSE;
	}

	dbus_connection_setup_with_g_main (connection, NULL);

	if (!dbus_connection_add_filter (connection, e_shell_network_monitor, window, NULL))
		return FALSE;

	dbus_bus_add_match (connection,
			    "type='signal',"
			    "interface='" NM_DBUS_INTERFACE "',"
			    "sender='" NM_DBUS_SERVICE "',"
			    "path='" NM_DBUS_PATH "'", &error);
	if (dbus_error_is_set (&error)) {
		dbus_error_free (&error);
		return FALSE;
	}

	return TRUE;
}
