/*
 * Evolution->Ipod synchronisation
 *
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
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 * (C)2004 Justin Wake <jwake@iinet.net.au>
 *
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "evolution-ipod-sync.h"

#include <glib/gi18n-lib.h>
#include <gtk/gtk.h>
#include <glade/glade.h>

gchar *  mount_point = NULL;
LibHalContext *ctx;

gboolean
ipod_check_status (gboolean silent)
{
	LibHalContext *ctx;
	DBusConnection *conn;

	if (check_hal () == FALSE)
	{
		if (!silent) {
			GtkWidget *message;
			gchar *msg1;
			msg1 = g_strdup_printf("<span weight=\"bold\" size=\"larger\">%s</span>\n\n", _("Hardware Abstraction Layer not loaded"));

			message = gtk_message_dialog_new_with_markup (NULL, 0, GTK_MESSAGE_ERROR, GTK_BUTTONS_OK,
				"%s%s", msg1, _("The \"hald\" service is required but not currently "
						"running. Please enable the service and rerun this "
						"program, or contact your system administrator."));

			gtk_dialog_run (GTK_DIALOG (message));

			g_free(msg1);
			gtk_widget_destroy (message);
		}
		return FALSE;

	}

	conn = dbus_bus_get (DBUS_BUS_SYSTEM, NULL);

	ctx = libhal_ctx_new ();
	libhal_ctx_set_dbus_connection (ctx, conn);
	if (!libhal_ctx_init(ctx, NULL))
		return FALSE;

	mount_point = find_ipod_mount_point (ctx);

	if (mount_point == NULL) {
		/* Either the iPod wasn't mounted when we started, or
		 * it wasn't plugged in. Either way, we want to umount
		 * the iPod when we finish syncing. */
		if (!silent) {
			GtkWidget *message;
			gchar *msg1;
			msg1 = g_strdup_printf("<span weight=\"bold\" size=\"larger\">%s</span>\n\n", _("Search for an iPod failed"));

			message = gtk_message_dialog_new_with_markup (NULL, 0, GTK_MESSAGE_ERROR, GTK_BUTTONS_OK,
				"%s%s", msg1, _("Evolution could not find an iPod to synchronize with. "
						"Either the iPod is not connected to the system or it "
						"is not powered on."));

			gtk_dialog_run (GTK_DIALOG (message));

			g_free(msg1);
			gtk_widget_destroy (message);
		}

		return FALSE;
	}

	return TRUE;
}

gchar *
ipod_get_mount (void)
{
	return mount_point;
}

