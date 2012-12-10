/*
 * e-alert-sink.c
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
 */

/**
 * SECTION: e-alert-sink
 * @short_description: an interface to handle alerts
 * @include: e-util/e-util.h
 *
 * A widget that implements #EAlertSink means it can handle #EAlerts,
 * usually by displaying them to the user.
 **/

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "e-alert-sink.h"

#include "e-alert-dialog.h"

G_DEFINE_INTERFACE (
	EAlertSink,
	e_alert_sink,
	GTK_TYPE_WIDGET)

static void
alert_sink_fallback (GtkWidget *widget,
                     EAlert *alert)
{
	GtkWidget *dialog;
	gpointer parent;

	parent = gtk_widget_get_toplevel (widget);
	parent = gtk_widget_is_toplevel (parent) ? parent : NULL;

	dialog = e_alert_dialog_new (parent, alert);
	gtk_dialog_run (GTK_DIALOG (dialog));
	gtk_widget_destroy (dialog);
}

static void
alert_sink_submit_alert (EAlertSink *alert_sink,
                         EAlert *alert)
{
	/* This is just a lame fallback handler.  Implementors
	 * are strongly encouraged to override this method. */
	alert_sink_fallback (GTK_WIDGET (alert_sink), alert);
}

static void
e_alert_sink_default_init (EAlertSinkInterface *interface)
{
	interface->submit_alert = alert_sink_submit_alert;
}

/**
 * e_alert_sink_submit_alert:
 * @alert_sink: an #EAlertSink
 * @alert: an #EAlert
 *
 * This function is a place to pass #EAlert objects.  Beyond that it has no
 * well-defined behavior.  It's up to the widget implementing the #EAlertSink
 * interface to decide what to do with them.
 **/
void
e_alert_sink_submit_alert (EAlertSink *alert_sink,
                           EAlert *alert)
{
	EAlertSinkInterface *interface;

	g_return_if_fail (E_IS_ALERT_SINK (alert_sink));
	g_return_if_fail (E_IS_ALERT (alert));

	interface = E_ALERT_SINK_GET_INTERFACE (alert_sink);
	g_return_if_fail (interface->submit_alert != NULL);

	interface->submit_alert (alert_sink, alert);
}
