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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <glib.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <Evolution.h>
#include <NetworkManager/libnm_glib.h>
#include <e-shell-window.h>


static void e_shell_glib_network_monitor (libnm_glib_ctx *ctx, gpointer user_data)
{
	libnm_glib_state	state;
	EShellLineStatus line_status;
	EShellWindow *window = E_SHELL_WINDOW (user_data);
	EShell *shell = e_shell_window_peek_shell (window);
	GNOME_Evolution_ShellState shell_state;

	g_return_if_fail (ctx != NULL);

	state = libnm_glib_get_network_state (ctx);
	line_status = e_shell_get_line_status (shell);
	
	if (line_status == E_SHELL_LINE_STATUS_ONLINE && state == LIBNM_NO_NETWORK_CONNECTION) {
	   	 shell_state = GNOME_Evolution_FORCED_OFFLINE;
		 e_shell_go_offline (shell, window, shell_state);   
	} else if (line_status == E_SHELL_LINE_STATUS_OFFLINE && state == LIBNM_ACTIVE_NETWORK_CONNECTION) {
	       	 shell_state = GNOME_Evolution_USER_ONLINE;
		 e_shell_go_online (shell, window, shell_state);
	} else  		
		return LIBNM_INVALID_CONTEXT;

	return FALSE;
}


int e_shell_nm_glib_initialise (EShellWindow *window )
{
	libnm_glib_ctx *ctx;
	guint id;

	ctx = libnm_glib_init ();
	if (!ctx)
	{
		fprintf (stderr, "Could not initialize libnm.\n");
		return FALSE;
	}

	id = libnm_glib_register_callback (ctx, e_shell_glib_network_monitor, window, NULL);
	libnm_glib_unregister_callback (ctx, id);

	id = libnm_glib_register_callback (ctx, e_shell_glib_network_monitor, window, NULL);
	
	return TRUE;
}
