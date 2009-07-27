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
component_has_new_attendees (ECalComponent *comp)
{
	g_return_val_if_fail (comp != NULL, FALSE);

	if (!e_cal_component_has_attendees (comp))
		return FALSE;

	return g_object_get_data (G_OBJECT (comp), "new-attendees") != NULL;
}

static gboolean
have_nonprocedural_alarm (ECalComponent *comp)
{
	GList *uids, *l;

	g_return_val_if_fail (comp != NULL, FALSE);

	uids = e_cal_component_get_alarm_uids (comp);

	for (l = uids; l; l = l->next) {
		ECalComponentAlarm *alarm;
		ECalComponentAlarmAction action = E_CAL_COMPONENT_ALARM_UNKNOWN;

		alarm = e_cal_component_get_alarm (comp, (const gchar *)l->data);
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

static GtkWidget *
add_checkbox (GtkBox *where, const gchar *caption)
{
	GtkWidget *checkbox, *align;

	g_return_val_if_fail (where != NULL, NULL);
	g_return_val_if_fail (caption != NULL, NULL);

	checkbox = gtk_check_button_new_with_mnemonic (caption);
	align = gtk_alignment_new (0.0, 0.5, 0.0, 0.0);
	gtk_alignment_set_padding (GTK_ALIGNMENT (align), 0, 0, 12, 12);
	gtk_container_add (GTK_CONTAINER (align), checkbox);
	gtk_widget_show (checkbox);
	gtk_box_pack_start (where, align, TRUE, TRUE, 2);
	gtk_widget_show (align);

	return checkbox;
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
send_component_dialog (GtkWindow *parent, ECal *client, ECalComponent *comp, gboolean new, gboolean *strip_alarms, gboolean *only_new_attendees)
{
	ECalComponentVType vtype;
	const gchar *id;
	GtkWidget *dialog, *sa_checkbox = NULL, *ona_checkbox = NULL;
	gboolean res;

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

	if (only_new_attendees && !component_has_new_attendees (comp)) {
		/* do not show the check if there is no new attendee and
		   set as all attendees are required to be notified */
		*only_new_attendees = FALSE;

		/* pretend it as being passed NULL to simplify code below */
		only_new_attendees = NULL;
	}

	if (strip_alarms && !have_nonprocedural_alarm (comp)) {
		/* pretend it as being passed NULL to simplify code below */
		strip_alarms = NULL;
	}

	dialog = e_error_new (parent, id, NULL);

	if (strip_alarms)
		sa_checkbox = add_checkbox (GTK_BOX (GTK_DIALOG (dialog)->vbox), _("Send my alarms with this event"));
	if (only_new_attendees)
		ona_checkbox = add_checkbox (GTK_BOX (GTK_DIALOG (dialog)->vbox), _("Notify new attendees _only"));

	res = gtk_dialog_run (GTK_DIALOG (dialog)) == GTK_RESPONSE_YES;

	if (res && strip_alarms)
		*strip_alarms = !gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (sa_checkbox));
	if (only_new_attendees)
		*only_new_attendees = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (ona_checkbox));

	gtk_widget_destroy (GTK_WIDGET (dialog));

	return res;
}

gboolean
send_component_prompt_subject (GtkWindow *parent, ECal *client, ECalComponent *comp)
{
	ECalComponentVType vtype;
	const gchar *id;

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
