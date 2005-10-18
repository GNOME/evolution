/*
 * evolution-ipod-sync.c - Evolution->Ipod synchronisation
 *
 * (C)2004 Justin Wake <jwake@iinet.net.au>
 *
 * Licensed under the GNU GPL v2. See COPYING.
 *
 */

#include "config.h"
#include "evolution-ipod-sync.h"

#include <gnome.h>
#include <glade/glade.h>
#include <libhal.h>

char *  mount_point = NULL;
LibHalContext *ctx;

gboolean
ipod_check_status (gboolean silent)
{
	LibHalContext *ctx;
	DBusConnection *conn;
	
	if (check_hal () == FALSE)
	{
		if (!silent) {
			GtkWidget *message = gtk_message_dialog_new_with_markup (
											NULL, 0, GTK_MESSAGE_ERROR, GTK_BUTTONS_OK,
											"<span weight=\"bold\" size=\"larger\">"
											"Hardware Abstraction Layer not loaded"
											"</span>\n\n"
											"The \"hald\" service is required but not currently "
											"running. Please enable the service and rerun this "
											"program, or contact your system administrator.");

			gtk_dialog_run (GTK_DIALOG (message));
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
			GtkWidget *message = gtk_message_dialog_new_with_markup (
											NULL, 0, GTK_MESSAGE_ERROR, GTK_BUTTONS_OK,
											"<span weight=\"bold\" size=\"larger\">"
											"Search for a iPod failed"
											"</span>\n\n"
											"Evolution could not find a iPod to synchronize with."
											"Either it is not connected to the system or it is "
											"not powered on.");

			gtk_dialog_run (GTK_DIALOG (message));
			gtk_widget_destroy (message);
		}

		return FALSE;
	}

	return TRUE;
}

char *
ipod_get_mount ()
{
	return mount_point;
}

