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

#include <gtk/gtkmessagedialog.h>
#include "widgets/misc/e-error.h"
#include "send-comp.h"



/**
 * send_component_dialog:
 * 
 * Pops up a dialog box asking the user whether he wants to send a
 * iTip/iMip message
 * 
 * Return value: TRUE if the user clicked Yes, FALSE otherwise.
 **/
gboolean
send_component_dialog (GtkWindow *parent, ECal *client, ECalComponent *comp, gboolean new)
{
	ECalComponentVType vtype;
	const char *id;
	
	if (e_cal_get_save_schedules (client))
		return FALSE;
	
	vtype = e_cal_component_get_vtype (comp);

	switch (vtype) {
	case E_CAL_COMPONENT_EVENT:
		if (new)
			id = "calendar:prompt-meeting-invite";
		else
			id = "calendar:prompt-send-updated-meeting-info";
		break;

	case E_CAL_COMPONENT_TODO:
		if (new)
			id = "calendar:prompt-send-task";
		else
			id = "calendar:prompt-send-updated-task-info";
		break;

	default:
		g_message ("send_component_dialog(): "
			   "Cannot handle object of type %d", vtype);
		return FALSE;
	}
	
	if (e_error_run (parent, id, NULL) == GTK_RESPONSE_YES)
		return TRUE;
	else
		return FALSE;
}

gboolean
send_component_prompt_subject (GtkWindow *parent, ECal *client, ECalComponent *comp)
{
	ECalComponentVType vtype;
	const char *id;
	
	vtype = e_cal_component_get_vtype (comp);

	switch (vtype) {
	case E_CAL_COMPONENT_EVENT:
			id = "calendar:prompt-send-no-subject-calendar";
		break;

	case E_CAL_COMPONENT_TODO:
			id = "calendar:prompt-send-no-subject-task";
		break;

	default:
		g_message ("send_component_prompt_subject(): "
			   "Cannot handle object of type %d", vtype);
		return FALSE;
	}
	
	if (e_error_run (parent, id, NULL) == GTK_RESPONSE_YES)
		return TRUE;
	else
		return FALSE;
}


