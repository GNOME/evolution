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
#include "cancel-comp.h"



/**
 * cancel_component_dialog:
 * 
 * Pops up a dialog box asking the user whether he wants to send a
 * cancel and delete an iTip/iMip message
 * 
 * Return value: TRUE if the user clicked Yes, FALSE otherwise.
 **/
gboolean
cancel_component_dialog (GtkWindow *parent, ECal *client, ECalComponent *comp, gboolean deleting)
{
	GtkWidget *dialog;
	ECalComponentVType vtype;
	char *str;
	gint response;

	if (deleting && e_cal_get_save_schedules (client))
		return TRUE;

	vtype = e_cal_component_get_vtype (comp);

	switch (vtype) {
	case E_CAL_COMPONENT_EVENT:
		if (deleting)
			str = g_strdup_printf (_("The event being deleted is a meeting, "
						 "would you like to send a cancellation notice?"));
		else
			str = g_strdup_printf (_("Are you sure you want to cancel "
						 "and delete this meeting?"));
		break;

	case E_CAL_COMPONENT_TODO:
		if (deleting)
			str = g_strdup_printf (_("The task being deleted is assigned, "
						 "would you like to send a cancellation notice?"));
		else
			str = g_strdup_printf (_("Are you sure you want to cancel "
						 "and delete this task?"));
		break;

	case E_CAL_COMPONENT_JOURNAL:
		if (deleting)
			str = g_strdup_printf (_("The journal entry being deleted is published, "
						 "would you like to send a cancellation notice?"));
		else
			str = g_strdup_printf (_("Are you sure you want to cancel "
						 "and delete this journal entry?"));
		break;

	default:
		g_message ("cancel_component_dialog(): "
			   "Cannot handle object of type %d", vtype);
		return FALSE;
	}
	
	dialog = gtk_message_dialog_new (parent, GTK_DIALOG_MODAL,
					 GTK_MESSAGE_QUESTION,
					 GTK_BUTTONS_YES_NO, str);

	response = gtk_dialog_run (GTK_DIALOG (dialog));
	gtk_widget_destroy (dialog);

	if (response == GTK_RESPONSE_YES)
		return TRUE;
	else
		return FALSE;
}
