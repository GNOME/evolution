/*
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
 *		Chenthill Palanisamy (pchenthill@novell.com)
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <string.h>
#include <glib/gi18n.h>
#include <gtk/gtk.h>
#include <libecal/e-cal.h>

#include <e-util/e-alert-dialog.h>
#include <shell/e-shell-view.h>

#include <calendar/gui/e-calendar-view.h>
#include <calendar/gui/itip-utils.h>
#include <calendar/gui/gnome-cal.h>

#include "gw-ui.h"

typedef struct {
	ECal *ecal;
	icalcomponent *icalcomp;
} ReceiveData;

static void
finalize_receive_data (ReceiveData *r_data)
{
	if (r_data->ecal) {
		g_object_unref (r_data->ecal);
		r_data->ecal = NULL;
	}

	if (r_data->ecal) {
		icalcomponent_free (r_data->icalcomp);
		r_data->icalcomp = NULL;
	}

	g_free (r_data);
}

static gboolean
receive_objects (gpointer data)
{
	GError *error = NULL;
	ReceiveData *r_data = data;

	icalcomponent_set_method (r_data->icalcomp, ICAL_METHOD_REQUEST);

	if (!e_cal_receive_objects (r_data->ecal, r_data->icalcomp, &error)) {
		/* FIXME show an error dialog */
		g_error_free (error);
	}

	finalize_receive_data (r_data);
	return TRUE;
}

static icalproperty *
find_attendee (icalcomponent *ical_comp, const gchar *address)
{
	icalproperty *prop;

	if (address == NULL)
		return NULL;

	for (prop = icalcomponent_get_first_property (ical_comp, ICAL_ATTENDEE_PROPERTY);
	     prop != NULL;
	     prop = icalcomponent_get_next_property (ical_comp, ICAL_ATTENDEE_PROPERTY)) {
		icalvalue *value;
		const gchar *attendee;
		gchar *text;

		value = icalproperty_get_value (prop);
		if (!value)
			continue;

		attendee = icalvalue_get_string (value);

		text = g_strdup (itip_strip_mailto (attendee));
		text = g_strstrip (text);
		if (!g_ascii_strcasecmp (address, text)) {
			g_free (text);
			break;
		}
		g_free (text);
	}

	return prop;
}
static void
change_status (icalcomponent *ical_comp, const gchar *address, icalparameter_partstat status)
{
	icalproperty *prop;

	prop = find_attendee (ical_comp, address);
	if (prop) {
		icalparameter *param;

		icalproperty_remove_parameter (prop, ICAL_PARTSTAT_PARAMETER);
		param = icalparameter_new_partstat (status);
		icalproperty_add_parameter (prop, param);
	} else {
		icalparameter *param;

		prop = icalproperty_new_attendee (address);
		icalcomponent_add_property (ical_comp, prop);

		param = icalparameter_new_role (ICAL_ROLE_OPTPARTICIPANT);
		icalproperty_add_parameter (prop, param);

		param = icalparameter_new_partstat (status);
		icalproperty_add_parameter (prop, param);
	}
}

static void
process_meeting (ECalendarView *cal_view, icalparameter_partstat status)
{
	GList *selected;
	icalcomponent *clone;

	selected = e_calendar_view_get_selected_events (cal_view);
	if (selected) {
		ECalendarViewEvent *event = (ECalendarViewEvent *) selected->data;
		ECalComponent *comp;
		ReceiveData *r_data;
		gboolean recurring = FALSE;
		GThread *thread = NULL;
		GError *error = NULL;
		gchar *address = NULL;

		if (!is_comp_data_valid (event))
			return;

		comp = e_cal_component_new ();
		r_data = g_new0 (ReceiveData, 1);

		e_cal_component_set_icalcomponent (comp, icalcomponent_new_clone (event->comp_data->icalcomp));
		address = itip_get_comp_attendee (comp, event->comp_data->client);

		if (e_cal_component_has_recurrences (comp) || e_cal_component_is_instance (comp))
			recurring = TRUE;

		/* Free comp */
		g_object_unref (comp);
		comp = NULL;

		clone = icalcomponent_new_clone (event->comp_data->icalcomp);
		change_status (clone, address, status);

		r_data->ecal = g_object_ref (event->comp_data->client);
		r_data->icalcomp = clone;

		if (recurring) {
			gint response;
			const gchar *msg;

			if (status == ICAL_PARTSTAT_ACCEPTED || status == ICAL_PARTSTAT_TENTATIVE)
				msg = "org.gnome.evolution.process_meeting:recurrence-accept";
			else
				msg = "org.gnome.evolution.process_meeting:recurrence-decline";

			response = e_alert_run_dialog_for_args (GTK_WINDOW (gtk_widget_get_toplevel ((GtkWidget *)cal_view)),
								msg, NULL);
			if (response == GTK_RESPONSE_YES) {
				icalproperty *prop;
				const gchar *uid = icalcomponent_get_uid (r_data->icalcomp);

				prop = icalproperty_new_x ("All");
				icalproperty_set_x_name (prop, "X-GW-RECUR-INSTANCES-MOD-TYPE");
				icalcomponent_add_property (r_data->icalcomp, prop);

				prop = icalproperty_new_x (uid);
				icalproperty_set_x_name (prop, "X-GW-RECURRENCE-KEY");
				icalcomponent_add_property (r_data->icalcomp, prop);

			} else if (response == GTK_RESPONSE_CANCEL) {
				finalize_receive_data (r_data);
				return;
			}
		}

		thread = g_thread_create ((GThreadFunc) receive_objects, r_data , FALSE, &error);
		if (!thread) {
			g_warning (G_STRLOC ": %s", error->message);
			g_error_free (error);
		}
	}
}

static ECalendarView *
get_calendar_view (EShellView *shell_view)
{
	EShellContent *shell_content;
	GnomeCalendar *gcal = NULL;
	GnomeCalendarViewType view_type;

	g_return_val_if_fail (shell_view != NULL, NULL);

	shell_content = e_shell_view_get_shell_content (shell_view);

	g_object_get (shell_content, "calendar", &gcal, NULL);

	view_type = gnome_calendar_get_view (gcal);

	return gnome_calendar_get_calendar_view (gcal, view_type);
}

void
gw_meeting_accept_cb (GtkAction *action, EShellView *shell_view)
{
	ECalendarView *cal_view = get_calendar_view (shell_view);
	g_return_if_fail (cal_view != NULL);

	process_meeting (cal_view, ICAL_PARTSTAT_ACCEPTED);
}

void
gw_meeting_accept_tentative_cb (GtkAction *action, EShellView *shell_view)
{
	ECalendarView *cal_view = get_calendar_view (shell_view);
	g_return_if_fail (cal_view != NULL);

	process_meeting (cal_view, ICAL_PARTSTAT_TENTATIVE);
}

void
gw_meeting_decline_cb (GtkAction *action, EShellView *shell_view)
{
	ECalendarView *cal_view = get_calendar_view (shell_view);
	g_return_if_fail (cal_view != NULL);

	process_meeting (cal_view, ICAL_PARTSTAT_DECLINED);
}

typedef struct {
	ECal *client;
	ECalComponent *comp;
	CalObjModType mod;
} ThreadData;

static void
add_retract_data (ECalComponent *comp, const gchar *retract_comment, CalObjModType mod)
{
	icalcomponent *icalcomp = NULL;
	icalproperty *icalprop = NULL;

	icalcomp = e_cal_component_get_icalcomponent (comp);
	if (retract_comment && *retract_comment)
		icalprop = icalproperty_new_x (retract_comment);
	else
		icalprop = icalproperty_new_x ("0");
	icalproperty_set_x_name (icalprop, "X-EVOLUTION-RETRACT-COMMENT");
	icalcomponent_add_property (icalcomp, icalprop);

	if (mod == CALOBJ_MOD_ALL)
		icalprop = icalproperty_new_x ("All");
	else
		icalprop = icalproperty_new_x ("This");
	icalproperty_set_x_name (icalprop, "X-EVOLUTION-RECUR-MOD");
	icalcomponent_add_property (icalcomp, icalprop);
}

static void
free_thread_data (ThreadData *data)
{
	if (data == NULL)
		return;

	if (data->client)
		g_object_unref (data->client);

	if (data->comp)
		g_object_unref (data->comp);

	g_free (data);
}

static gpointer
retract_object (gpointer val)
{
	ThreadData *data = val;
	icalcomponent *icalcomp = NULL, *mod_comp = NULL;
	GList *users = NULL;
	gchar *rid = NULL;
	const gchar *uid;
	GError *error = NULL;

	add_retract_data (data->comp, NULL, data->mod);

	icalcomp = e_cal_component_get_icalcomponent (data->comp);
	icalcomponent_set_method (icalcomp, ICAL_METHOD_CANCEL);

	if (!e_cal_send_objects (data->client, icalcomp, &users,
						&mod_comp, &error))	{
		/* FIXME report error  */
		g_warning ("Unable to retract the meeting \n");
		g_clear_error (&error);
		return GINT_TO_POINTER (1);
	}

	if (mod_comp)
		icalcomponent_free (mod_comp);

	if (users) {
		g_list_foreach (users, (GFunc) g_free, NULL);
		g_list_free (users);
	}

	rid = e_cal_component_get_recurid_as_string (data->comp);
	e_cal_component_get_uid (data->comp, &uid);

	if (!e_cal_remove_object_with_mod (data->client, uid,
				rid, data->mod, &error)) {
		g_warning ("Unable to remove the item \n");
		g_clear_error (&error);
		return GINT_TO_POINTER (1);
	}
	g_free (rid);

	free_thread_data (data);
	return GINT_TO_POINTER (0);
}

static void
object_created_cb (CompEditor *ce, gpointer data)
{
	GThread *thread = NULL;
	gint response;
	GError *error = NULL;

	gtk_widget_hide (GTK_WIDGET (ce));

	response = e_alert_run_dialog_for_args (GTK_WINDOW (gtk_widget_get_toplevel ((GtkWidget *)ce)),
						"org.gnome.evolution.process_meeting:resend-retract",
						NULL);
	if (response == GTK_RESPONSE_NO) {
		free_thread_data (data);
		return;
	}

	thread = g_thread_create ((GThreadFunc) retract_object, data , FALSE, &error);
	if (!thread) {
		g_warning (G_STRLOC ": %s", error->message);
		g_error_free (error);
	}
}

void
gw_resend_meeting_cb (GtkAction *action, EShellView *shell_view)
{
	GList *selected;
	ECalendarView *cal_view = get_calendar_view (shell_view);

	g_return_if_fail (cal_view != NULL);

	selected = e_calendar_view_get_selected_events (cal_view);
	if (selected) {
		ECalendarViewEvent *event = (ECalendarViewEvent *) selected->data;
		ECalComponent *comp;
		ECalComponent *new_comp = NULL;
		gboolean recurring = FALSE;
		CalObjModType mod = CALOBJ_MOD_THIS;
		ThreadData *data = NULL;
		gint response;
		const gchar *msg;
		/* inserting the boolean to share the code between resend and retract */
		gboolean resend = TRUE;

		if (!is_comp_data_valid (event))
			return;

		comp = e_cal_component_new ();
		e_cal_component_set_icalcomponent (comp, icalcomponent_new_clone (event->comp_data->icalcomp));
		if (e_cal_component_has_recurrences (comp) || e_cal_component_is_instance (comp))
			recurring = TRUE;

		if (recurring == TRUE)
			msg = "org.gnome.evolution.process_meeting:resend-recurrence";
		else
			msg = "org.gnome.evolution.process_meeting:resend";

		response = e_alert_run_dialog_for_args (GTK_WINDOW (e_shell_view_get_shell_window (shell_view)),
							msg, NULL);
		if (response == GTK_RESPONSE_YES) {
			mod = CALOBJ_MOD_ALL;
		} else if (response == GTK_RESPONSE_CANCEL) {
			g_object_unref (comp);
			return;
		}

		data = g_new0 (ThreadData, 1);
		data->client = g_object_ref (event->comp_data->client);
		data->comp = comp;
		data->mod = mod;

		if (resend)
		{
			guint flags = 0;
			gchar *new_uid = NULL;
			CompEditor *ce;
			icalcomponent *icalcomp;

			flags |= COMP_EDITOR_NEW_ITEM;
			flags |= COMP_EDITOR_MEETING;
			flags |= COMP_EDITOR_USER_ORG;

			new_comp = e_cal_component_clone (comp);
			new_uid = e_cal_component_gen_uid ();
			e_cal_component_set_recurid (new_comp, NULL);
			e_cal_component_set_uid (new_comp, new_uid);
			icalcomp = icalcomponent_new_clone (e_cal_component_get_icalcomponent (new_comp));
			ce = e_calendar_view_open_event_with_flags (cal_view, data->client, icalcomp, flags);

			g_signal_connect (ce, "object_created", G_CALLBACK (object_created_cb), data);
			g_object_unref (new_comp);
			g_free (new_uid);
		}
	}
}
