/*
 * Evolution calendar - Send calendar component dialog
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 *
 *
 * Authors:
 *		JP Rosevear <jpr@ximian.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <gtk/gtk.h>
#include <glib/gi18n.h>
#include "delete-error.h"

/**
 * delete_error_dialog:
 *
 * Shows any applicable error messages as the result of deleting and object
 *
 **/
void
delete_error_dialog (const GError *error,
                     ECalComponentVType vtype)
{
	GtkWidget *dialog;
	const gchar *str;
	const gchar *icon_name = NULL;

	if (!error || error->domain != E_CLIENT_ERROR)
		return;

	switch (error->code) {
	case E_CLIENT_ERROR_DBUS_ERROR:
		switch (vtype) {
		case E_CAL_COMPONENT_EVENT:
			/* Translators: The '%s' is replaced with a detailed error message */
			str = _("The event could not be deleted due to a dbus error: %s");
			break;
		case E_CAL_COMPONENT_TODO:
			/* Translators: The '%s' is replaced with a detailed error message */
			str = _("The task could not be deleted due to a dbus error: %s");
			break;
		case E_CAL_COMPONENT_JOURNAL:
			/* Translators: The '%s' is replaced with a detailed error message */
			str = _("The memo could not be deleted due to a dbus error: %s");
			break;
		default:
			/* Translators: The '%s' is replaced with a detailed error message */
			str = _("The item could not be deleted due to a dbus error: %s");
			break;
		}
		break;
	case E_CLIENT_ERROR_PERMISSION_DENIED:
		switch (vtype) {
		case E_CAL_COMPONENT_EVENT:
			str = _("The event could not be deleted because permission was denied");
			break;
		case E_CAL_COMPONENT_TODO:
			str = _("The task could not be deleted because permission was denied");
			break;
		case E_CAL_COMPONENT_JOURNAL:
			str = _("The memo could not be deleted because permission was denied");
			break;
		default:
			str = _("The item could not be deleted because permission was denied");
			break;
		}
		break;
	case E_CLIENT_ERROR_OTHER_ERROR:
		switch (vtype) {
		case E_CAL_COMPONENT_EVENT:
			/* Translators: The '%s' is replaced with a detailed error message */
			str = _("The event could not be deleted due to an error: %s");
			break;
		case E_CAL_COMPONENT_TODO:
			/* Translators: The '%s' is replaced with a detailed error message */
			str = _("The task could not be deleted due to an error: %s");
			break;
		case E_CAL_COMPONENT_JOURNAL:
			/* Translators: The '%s' is replaced with a detailed error message */
			str = _("The memo could not be deleted due to an error: %s");
			break;
		default:
			/* Translators: The '%s' is replaced with a detailed error message */
			str = _("The item could not be deleted due to an error: %s");
			break;
		}
		break;
	default:
		/* If not found, we don't care - its gone anyhow */
		return;
	}

	dialog = gtk_message_dialog_new (
		NULL, GTK_DIALOG_MODAL,
		GTK_MESSAGE_ERROR,
		GTK_BUTTONS_OK, str, error->message);
	if (vtype == E_CAL_COMPONENT_EVENT)
		icon_name = "x-office-calendar";
	else if (vtype == E_CAL_COMPONENT_TODO)
		icon_name = "stock_todo";

	if (icon_name)
		gtk_window_set_icon_name (GTK_WINDOW (dialog), icon_name);

	gtk_dialog_run (GTK_DIALOG (dialog));
	gtk_widget_destroy (dialog);
}
