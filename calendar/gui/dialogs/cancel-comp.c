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
#include <e-util/e-icon-factory.h>
#include "widgets/misc/e-error.h"
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
	ECalComponentVType vtype;
	const char *id;
	
	if (deleting && e_cal_get_save_schedules (client))
		return TRUE;
	
	vtype = e_cal_component_get_vtype (comp);
	
	switch (vtype) {
	case E_CAL_COMPONENT_EVENT:
		if (deleting)
			id = "calendar:prompt-delete-meeting";
		else
			id = "calendar:prompt-cancel-meeting";
		break;

	case E_CAL_COMPONENT_TODO:
		if (deleting)
			id = "calendar:prompt-delete-task";
		else
			id = "calendar:prompt-cancel-task";
		break;

	case E_CAL_COMPONENT_JOURNAL:
		if (deleting)
			id = "calendar:prompt-delete-journal";
		else
			id = "calendar:prompt-cancel-journal";
		break;

	default:
		g_message ("cancel_component_dialog(): "
			   "Cannot handle object of type %d", vtype);
		return FALSE;
	}
	
	if (e_error_run (parent, id, NULL) == GTK_RESPONSE_YES)
		return TRUE;
	else
		return FALSE;
}
