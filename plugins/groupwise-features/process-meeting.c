/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 *  Authors: Chenthill Palanisamy (pchenthill@novell.com)
 *
 *  Copyright 2004 Novell, Inc. (www.novell.com)
 *
 *  This program is free software; you can redistribute it and/or
 *  modify it under the terms of version 2 of the GNU General Public
 *  License as published by the Free Software Foundation.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Street #330, Boston, MA 02111-1307, USA.
 *
 */
#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <string.h>
#include <libgnome/gnome-i18n.h>
#include <gtk/gtk.h>
#include <calendar/gui/e-cal-popup.h>
#include <calendar/gui/e-calendar-view.h>
#include <calendar/gui/itip-utils.h>
#include <e-util/e-error.h>
#include <libecal/e-cal.h>


typedef struct {
	ECal *ecal;
	icalcomponent *icalcomp;
} ReceiveData;

ECalendarView *c_view;

void org_gnome_accept(EPlugin *ep, ECalPopupTargetSelect *target);
static void on_accept_meeting (EPopup *ep, EPopupItem *pitem, void *data);
static void on_accept_meeting_tentative (EPopup *ep, EPopupItem *pitem, void *data);
static void on_decline_meeting (EPopup *ep, EPopupItem *pitem, void *data);

static EPopupItem popup_items[] = {
{ E_POPUP_ITEM, "41.accept", N_("Accept"), on_accept_meeting, NULL, GTK_STOCK_APPLY, 0, E_CAL_POPUP_SELECT_NOTEDITING | E_CAL_POPUP_SELECT_MEETING | E_CAL_POPUP_SELECT_ACCEPTABLE},
{ E_POPUP_ITEM, "42.accept", N_("Accept Tentatively"), on_accept_meeting_tentative, NULL, GTK_STOCK_DIALOG_QUESTION, 0, E_CAL_POPUP_SELECT_NOTEDITING | E_CAL_POPUP_SELECT_MEETING | E_CAL_POPUP_SELECT_ACCEPTABLE},
{ E_POPUP_ITEM, "43.decline", N_("Decline"), on_decline_meeting, NULL, GTK_STOCK_CANCEL, 0, E_CAL_POPUP_SELECT_NOTEDITING | E_CAL_POPUP_SELECT_MEETING}
};

static void 
popup_free (EPopup *ep, GSList *items, void *data)
{
	g_slist_free (items);
	items = NULL;
}

void 
org_gnome_accept (EPlugin *ep, ECalPopupTargetSelect *target)
{
	GSList *menus = NULL;
	GList *selected;
	int i = 0;
	static int first = 0;
	const char *uri = NULL;
	ECalendarView *cal_view = E_CALENDAR_VIEW (target->target.widget);

	c_view = cal_view;
	selected = e_calendar_view_get_selected_events (cal_view);
	if (selected) {
		ECalendarViewEvent *event = (ECalendarViewEvent *) selected->data;

		uri = e_cal_get_uri (event->comp_data->client);
	} else
		return;

	if (!uri)
		return;

	if (! g_strrstr (uri, "groupwise://"))
		return ;

	/* for translation*/
	if (!first) {
		popup_items[0].label =  _(popup_items[0].label);
	}

	first++;

	for (i = 0; i < sizeof (popup_items) / sizeof (popup_items[0]); i++)
		menus = g_slist_prepend (menus, &popup_items[i]);

	e_popup_add_items (target->target.popup, menus, NULL, popup_free, NULL);
}

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
find_attendee (icalcomponent *ical_comp, const char *address)
{
	icalproperty *prop;

	if (address == NULL)
		return NULL;

	for (prop = icalcomponent_get_first_property (ical_comp, ICAL_ATTENDEE_PROPERTY);
	     prop != NULL;
	     prop = icalcomponent_get_next_property (ical_comp, ICAL_ATTENDEE_PROPERTY)) {
		icalvalue *value;
		const char *attendee;
		char *text;

		value = icalproperty_get_value (prop);
		if (!value)
			continue;

		attendee = icalvalue_get_string (value);

		text = g_strdup (itip_strip_mailto (attendee));
		text = g_strstrip (text);
		if (!g_strcasecmp (address, text)) {
			g_free (text);
			break;
		}
		g_free (text);
	}
	
	return prop;
}
static void
change_status (icalcomponent *ical_comp, const char *address, icalparameter_partstat status)
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
		ECalComponent *comp = e_cal_component_new ();
		ReceiveData *r_data = g_new0 (ReceiveData, 1);
		gboolean recurring = FALSE;
		GThread *thread = NULL;
		GError *error = NULL;
		char *address = NULL;
		
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
			const char *arg;

			if (status == ICAL_PARTSTAT_ACCEPTED || status == ICAL_PARTSTAT_TENTATIVE)
				arg = "accept";
			else
				arg = "decline";

			response = e_error_run (NULL, "org.gnome.evolution.mail_shared_folder:recurrence", arg, NULL);
			if (response == GTK_RESPONSE_YES) {
				icalproperty *prop;
				const char *uid = icalcomponent_get_uid (r_data->icalcomp);

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

/*FIXME the data does not give us the ECalendarView object. 
  we should remove the global c_view variable once we get it from the data*/
static void
on_accept_meeting (EPopup *ep, EPopupItem *pitem, void *data)
{
	ECalendarView *cal_view = c_view;

	process_meeting (cal_view, ICAL_PARTSTAT_ACCEPTED);
}
static void
on_accept_meeting_tentative (EPopup *ep, EPopupItem *pitem, void *data)
{
	ECalendarView *cal_view = c_view;

	process_meeting (cal_view, ICAL_PARTSTAT_TENTATIVE);
}

static void
on_decline_meeting (EPopup *ep, EPopupItem *pitem, void *data)
{
	ECalendarView *cal_view = c_view;

	process_meeting (cal_view, ICAL_PARTSTAT_DECLINED);
}
