/*
 * Evolution calendar - Send calendar component dialog
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

#include <glib/gi18n-lib.h>
#include "e-util/e-error.h"
#include "send-comp.h"



static gboolean
have_nonprocedural_alarm (ECalComponent *comp)
{
	GList *uids, *l;

	g_return_val_if_fail (comp != NULL, FALSE);

	uids = e_cal_component_get_alarm_uids (comp);

	for (l = uids; l; l = l->next) {
		ECalComponentAlarm *alarm;
		ECalComponentAlarmAction action = E_CAL_COMPONENT_ALARM_UNKNOWN;

		alarm = e_cal_component_get_alarm (comp, (const char *)l->data);
		if (alarm) {
			e_cal_component_alarm_get_action (alarm, &action);
			e_cal_component_alarm_free (alarm);

			if (action != E_CAL_COMPONENT_ALARM_NONE &&
			    action != E_CAL_COMPONENT_ALARM_PROCEDURE &&
			    action != E_CAL_COMPONENT_ALARM_UNKNOWN) {
				cal_obj_uid_list_free (uids);
				return TRUE;
			}
		}
	}

	cal_obj_uid_list_free (uids);

	return FALSE;
}

/**
 * send_component_dialog:
 *
 * Pops up a dialog box asking the user whether he wants to send a
 * iTip/iMip message
 *
 * Return value: TRUE if the user clicked Yes, FALSE otherwise.
 **/
gboolean
send_component_dialog (GtkWindow *parent, ECal *client, ECalComponent *comp, gboolean new, gboolean *strip_alarms)
{
	ECalComponentVType vtype;
	const char *id;

	if (strip_alarms)
		*strip_alarms = TRUE;

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
	case E_CAL_COMPONENT_JOURNAL:
		return TRUE;
	default:
		g_message ("send_component_dialog(): "
			   "Cannot handle object of type %d", vtype);
		return FALSE;
	}

	if (strip_alarms && have_nonprocedural_alarm (comp)) {
		GtkWidget *dialog, *checkbox, *align;
		gboolean res;

		dialog = e_error_new (parent, id, NULL);
		checkbox = gtk_check_button_new_with_label (_("Send my alarms with this event"));
		align = gtk_alignment_new (0.5, 0.5, 0.0, 0.0);
		gtk_container_add (GTK_CONTAINER (align), checkbox);
		gtk_widget_show (checkbox);
		gtk_box_pack_end (GTK_BOX (GTK_DIALOG (dialog)->vbox), align, TRUE, TRUE, 6);
		gtk_widget_show (align);

		res = gtk_dialog_run (GTK_DIALOG (dialog)) == GTK_RESPONSE_YES;

		if (res)
			*strip_alarms = !gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (checkbox));

		gtk_widget_destroy (GTK_WIDGET (dialog));

		return res;
	} else {
		if (e_error_run (parent, id, NULL) == GTK_RESPONSE_YES)
			return TRUE;
		else
			return FALSE;
	}
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
	case E_CAL_COMPONENT_JOURNAL:
			id = "calendar:prompt-send-no-subject-memo";
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
