/* Evolution calendar - Send calendar component dialog
 *
 * Copyright (C) 2001 Ximian, Inc.
 *
 * Author: JP Rosevear <jpr@ximian.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of version 2 of the GNU General Public
 * License as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307, USA.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <glib.h>
#include <gtk/gtkmessagedialog.h>
#include <libgnome/gnome-i18n.h>
#include <libgnomeui/gnome-uidefs.h>
#include <gal/widgets/e-unicode.h>
#include "delete-error.h"



/**
 * delete_error_dialog:
 * 
 * Shows any applicable error messages as the result of deleting and object
 * 
 **/
void
delete_error_dialog (CalClientResult result, CalComponentVType vtype)
{
	GtkWidget *dialog;
	const char *str;

	switch (result) {
	case CAL_CLIENT_RESULT_CORBA_ERROR:
		switch (vtype) {
		case CAL_COMPONENT_EVENT:
			str = _("The event could not be deleted due to a corba error");
			break;
		case CAL_COMPONENT_TODO:
			str = _("The task could not be deleted due to a corba error");
			break;
		case CAL_COMPONENT_JOURNAL:
			str = _("The journal entry could not be deleted due to a corba error");
			break;
		default:
			str = _("The item could not be deleted due to a corba error");
			break;
		}
		break;
	case CAL_CLIENT_RESULT_PERMISSION_DENIED:
		switch (vtype) {
		case CAL_COMPONENT_EVENT:
			str = _("The event could not be deleted because permission was denied");
			break;
		case CAL_COMPONENT_TODO:
			str = _("The task could not be deleted because permission was denied");
			break;
		case CAL_COMPONENT_JOURNAL:
			str = _("The journal entry could not be deleted because permission was denied");
			break;
		default:
			str = _("The item could not be deleted because permission was denied");
			break;
		}
		break;
	case CAL_CLIENT_RESULT_INVALID_OBJECT:
		switch (vtype) {
		case CAL_COMPONENT_EVENT:
			str = _("The event could not be deleted because it was invalid");
			break;
		case CAL_COMPONENT_TODO:
			str = _("The task could not be deleted because it was invalid");
			break;
		case CAL_COMPONENT_JOURNAL:
			str = _("The journal entry could not be deleted because it was invalid");
			break;
		default:
			str = _("The item could not be deleted because it was invalid");
			break;
		}
		break;
	case CAL_CLIENT_RESULT_SUCCESS:
	case CAL_CLIENT_RESULT_NOT_FOUND:
	default:		
		/* If not found, we don't care - its gone anyhow */
		return;
	}
	
	dialog = gtk_message_dialog_new (NULL, GTK_DIALOG_MODAL,
					 GTK_MESSAGE_ERROR,
					 GTK_BUTTONS_OK, str);
	gtk_dialog_run (GTK_DIALOG (dialog));
	gtk_widget_destroy (dialog);
}
