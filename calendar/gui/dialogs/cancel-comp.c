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
#include <glib/gi18n.h>
#include <libgnomeui/gnome-uidefs.h>
#include <e-util/e-icon-factory.h>
#include "e-util/e-error.h"
#include "cancel-comp.h"



/* is_past_event:
 * 
 * returns TRUE if @comp is in the past, FALSE otherwise.
 * Comparision is based only on date part, time part is ignored.
 */
static gboolean
is_past_event (ECalComponent *comp)
{
	ECalComponentDateTime end_date;
	gboolean res;

	if (!comp) return TRUE;

	e_cal_component_get_dtend (comp, &end_date);
	res = icaltime_compare_date_only (*end_date.value,
					  icaltime_current_time_with_zone (icaltime_get_timezone (*end_date.value))
					 ) == -1;
	e_cal_component_free_datetime (&end_date);

	return res;
}

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
		if (is_past_event (comp)) {
			/* don't ask neither send notification to others on past events */
			return FALSE;
		}
		if (deleting)
			id = "calendar:prompt-cancel-meeting";
		else
			id = "calendar:prompt-delete-meeting";
		break;

	case E_CAL_COMPONENT_TODO:
		if (deleting)
			id = "calendar:prompt-cancel-task";
		else
			id = "calendar:prompt-delete-task";
		break;

	case E_CAL_COMPONENT_JOURNAL:
		if (deleting)
			id = "calendar:prompt-cancel-journal";
		else
			id = "calendar:prompt-delete-journal";
		break;

	default:
		g_message (G_STRLOC ": Cannot handle object of type %d", vtype);
		return FALSE;
	}
	
	if (e_error_run (parent, id, NULL) == GTK_RESPONSE_YES)
		return TRUE;
	else
		return FALSE;
}
