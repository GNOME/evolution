/*
 * evolution-ipod-sync.c - Evolution->Ipod synchronisation
 *
 * (C)2004 Justin Wake <jwake@iinet.net.au>
 *
 * Licensed under the GNU GPL v2. See COPYING.
 *
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "evolution-ipod-sync.h"

#include <glib/gi18n-lib.h>
#include <gtk/gtk.h>
#include <glade/glade.h>

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
			gchar *msg1, *msg2;
			msg1 = g_strdup_printf("<span weight=\"bold\" size=\"larger\">%s</span>\n\n", _("Hardware Abstraction Layer not loaded"));
			msg2 = g_strdup_printf("%s%s", msg1, _("The \"hald\" service is required but not currently "
								"running. Please enable the service and rerun this "
								"program, or contact your system administrator.") );

			GtkWidget *message = gtk_message_dialog_new_with_markup (NULL, 0, GTK_MESSAGE_ERROR, GTK_BUTTONS_OK, msg2);

			gtk_dialog_run (GTK_DIALOG (message));

			g_free(msg1);
			g_free(msg2);
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
			gchar *msg1, *msg2;
			msg1 = g_strdup_printf("<span weight=\"bold\" size=\"larger\">%s</span>\n\n", _("Search for an iPod failed"));
			msg2 = g_strdup_printf("%s%s", msg1, _("Evolution could not find an iPod to synchronize with. "
								"Either the iPod is not connected to the system or it "
								"is not powered on."));

			GtkWidget *message = gtk_message_dialog_new_with_markup (NULL, 0, GTK_MESSAGE_ERROR, GTK_BUTTONS_OK, msg2);

			gtk_dialog_run (GTK_DIALOG (message));

			g_free(msg1);
			g_free(msg2);
			gtk_widget_destroy (message);
		}

		return FALSE;
	}

	return TRUE;
}

char *
ipod_get_mount (void)
{
	return mount_point;
}

