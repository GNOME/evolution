/*
 * SPDX-FileCopyrightText: (C) 1999-2008 Novell, Inc. (www.novell.com)
 * SPDX-License-Identifier: LGPL-2.1-or-later
 * SPDX-FileContributor: Michael Meeks <michael@ximian.com>
 * SPDX-FileContributor: Ettore Perazzoli <ettore@ximian.com>
 */

#include "evolution-config.h"

#include "e-dialog-utils.h"

/**
 * e_notice:
 * @parent: the dialog's parent window, or %NULL
 * @type: the type of dialog (%GTK_MESSAGE_INFO, %GTK_MESSAGE_WARNING,
 * or %GTK_MESSAGE_ERROR)
 * @format: printf-style format string, followed by arguments
 *
 * Convenience function to show a dialog with a message and an "OK"
 * button.
 **/
void
e_notice (gpointer parent,
          GtkMessageType type,
          const gchar *format,
          ...)
{
	GtkWidget *dialog;
	va_list args;
	gchar *str;

	va_start (args, format);
	str = g_strdup_vprintf (format, args);

	dialog = gtk_message_dialog_new (
		NULL, GTK_DIALOG_DESTROY_WITH_PARENT,
		type, GTK_BUTTONS_OK, "%s", str);
	va_end (args);
	g_free (str);

	if (parent && !gtk_widget_is_toplevel (parent))
		parent = gtk_widget_get_toplevel (parent);
	if (parent)
		gtk_window_set_transient_for (GTK_WINDOW (dialog), parent);

	gtk_dialog_run (GTK_DIALOG (dialog));
	gtk_widget_destroy (dialog);
}
