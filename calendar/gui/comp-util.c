/* Evolution calendar - Utilities for manipulating ECalComponent objects
 *
 * Copyright (C) 2000 Ximian, Inc.
 * Copyright (C) 2000 Ximian, Inc.
 *
 * Author: Federico Mena-Quintero <federico@ximian.com>
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

#include <string.h>
#include <time.h>
#include "calendar-config.h"
#include "comp-util.h"
#include "dialogs/delete-comp.h"



/**
 * cal_comp_util_add_exdate:
 * @comp: A calendar component object.
 * @itt: Time for the exception.
 * 
 * Adds an exception date to the current list of EXDATE properties in a calendar
 * component object.
 **/
void
cal_comp_util_add_exdate (ECalComponent *comp, time_t t, icaltimezone *zone)
{
	GSList *list;
	ECalComponentDateTime *cdt;

	g_return_if_fail (comp != NULL);
	g_return_if_fail (E_IS_CAL_COMPONENT (comp));

	e_cal_component_get_exdate_list (comp, &list);

	cdt = g_new (ECalComponentDateTime, 1);
	cdt->value = g_new (struct icaltimetype, 1);
	*cdt->value = icaltime_from_timet_with_zone (t, FALSE, zone);
	cdt->tzid = g_strdup (icaltimezone_get_tzid (zone));

	list = g_slist_append (list, cdt);
	e_cal_component_set_exdate_list (comp, list);
	e_cal_component_free_exdate_list (list);
}



/* Returns TRUE if the TZIDs are equivalent, i.e. both NULL or the same. */
static gboolean
e_cal_component_compare_tzid (const char *tzid1, const char *tzid2)
{
	gboolean retval = TRUE;

	if (tzid1) {
		if (!tzid2 || strcmp (tzid1, tzid2))
			retval = FALSE;
	} else {
		if (tzid2)
			retval = FALSE;
	}

	return retval;
}

/**
 * cal_comp_util_compare_event_timezones:
 * @comp: A calendar component object.
 * @client: A #ECal.
 * 
 * Checks if the component uses the given timezone for both the start and
 * the end time, or if the UTC offsets of the start and end times are the same
 * as in the given zone.
 *
 * Returns: TRUE if the component's start and end time are at the same UTC
 * offset in the given timezone.
 **/
gboolean
cal_comp_util_compare_event_timezones (ECalComponent *comp,
				       ECal *client,
				       icaltimezone *zone)
{
	ECalComponentDateTime start_datetime, end_datetime;
	const char *tzid;
	gboolean retval = FALSE;
	icaltimezone *start_zone, *end_zone;
	int offset1, offset2;

	tzid = icaltimezone_get_tzid (zone);

	e_cal_component_get_dtstart (comp, &start_datetime);
	e_cal_component_get_dtend (comp, &end_datetime);

	/* If either the DTSTART or the DTEND is a DATE value, we return TRUE.
	   Maybe if one was a DATE-TIME we should check that, but that should
	   not happen often. */
	if ((start_datetime.value && start_datetime.value->is_date)
	    || (end_datetime.value && end_datetime.value->is_date)) {
		retval = TRUE;
		goto out;
	}

	/* If the event uses UTC for DTSTART & DTEND, return TRUE. Outlook
	   will send single events as UTC, so we don't want to mark all of
	   these. */
	if ((!start_datetime.value || start_datetime.value->is_utc)
	    && (!end_datetime.value || end_datetime.value->is_utc)) {
		retval = TRUE;
		goto out;
	}

	/* If the event uses floating time for DTSTART & DTEND, return TRUE.
	   Imported vCalendar files will use floating times, so we don't want
	   to mark all of these. */
	if (!start_datetime.tzid && !end_datetime.tzid) {
		retval = TRUE;
		goto out;
	}

	/* FIXME: DURATION may be used instead. */
	if (e_cal_component_compare_tzid (tzid, start_datetime.tzid)
	    && e_cal_component_compare_tzid (tzid, end_datetime.tzid)) {
		/* If both TZIDs are the same as the given zone's TZID, then
		   we know the timezones are the same so we return TRUE. */
		retval = TRUE;
	} else {
		/* If the TZIDs differ, we have to compare the UTC offsets
		   of the start and end times, using their own timezones and
		   the given timezone. */
		if (!e_cal_get_timezone (client, start_datetime.tzid,
					      &start_zone, NULL))
			goto out;

		if (start_datetime.value) {
			offset1 = icaltimezone_get_utc_offset (start_zone,
							       start_datetime.value,
							       NULL);
			offset2 = icaltimezone_get_utc_offset (zone,
							       start_datetime.value,
							       NULL);
			if (offset1 != offset2)
				goto out;
		}

		if (!e_cal_get_timezone (client, end_datetime.tzid,
					      &end_zone, NULL))
			goto out;

		if (end_datetime.value) {
			offset1 = icaltimezone_get_utc_offset (end_zone,
							       end_datetime.value,
							       NULL);
			offset2 = icaltimezone_get_utc_offset (zone,
							       end_datetime.value,
							       NULL);
			if (offset1 != offset2)
				goto out;
		}

		retval = TRUE;
	}

 out:

	e_cal_component_free_datetime (&start_datetime);
	e_cal_component_free_datetime (&end_datetime);

	return retval;
}

/**
 * cal_comp_confirm_delete_empty_comp:
 * @comp: A calendar component.
 * @client: Calendar client where the component purportedly lives.
 * @widget: Widget to be used as the basis for UTF8 conversion.
 * 
 * Assumming a calendar component with an empty SUMMARY property (as per
 * string_is_empty()), asks whether the user wants to delete it based on
 * whether the appointment is on the calendar server or not.  If the
 * component is on the server, this function will present a confirmation
 * dialog and delete the component if the user tells it to.  If the component
 * is not on the server it will just return TRUE.
 * 
 * Return value: A result code indicating whether the component
 * was not on the server and is to be deleted locally, whether it
 * was on the server and the user deleted it, or whether the
 * user cancelled the deletion.
 **/
gboolean
cal_comp_is_on_server (ECalComponent *comp, ECal *client)
{
	const char *uid;
	icalcomponent *icalcomp;
	GError *error = NULL;

	g_return_val_if_fail (comp != NULL, FALSE);
	g_return_val_if_fail (E_IS_CAL_COMPONENT (comp), FALSE);
	g_return_val_if_fail (client != NULL, FALSE);
	g_return_val_if_fail (E_IS_CAL (client), FALSE);

	/* See if the component is on the server.  If it is not, then it likely
	 * means that the appointment is new, only in the day view, and we
	 * haven't added it yet to the server.  In that case, we don't need to
	 * confirm and we can just delete the event.  Otherwise, we ask
	 * the user.
	 */
	e_cal_component_get_uid (comp, &uid);

	if (e_cal_get_object (client, uid, NULL, &icalcomp, &error)) {
		icalcomponent_free (icalcomp);

		return TRUE;
	}

	if (error->code != E_CALENDAR_STATUS_OBJECT_NOT_FOUND)
		g_warning (G_STRLOC ": %s", error->message);

	g_clear_error (&error);

	return FALSE;
}

/**
 * cal_comp_event_new_with_defaults:
 * 
 * Creates a new VEVENT component and adds any default alarms to it as set in
 * the program's configuration values.
 * 
 * Return value: A newly-created calendar component.
 **/
ECalComponent *
cal_comp_event_new_with_defaults (ECal *client)
{
	icalcomponent *icalcomp;
	ECalComponent *comp;
	int interval;
	CalUnits units;
	ECalComponentAlarm *alarm;
	icalproperty *icalprop;
	ECalComponentAlarmTrigger trigger;

	if (!e_cal_get_default_object (client, &icalcomp, NULL))
		return NULL;

	comp = e_cal_component_new ();
	if (!e_cal_component_set_icalcomponent (comp, icalcomp)) {
		g_object_unref (comp);
		icalcomponent_free (icalcomp);
		return NULL;
	}
	
	if (!calendar_config_get_use_default_reminder ())
		return comp;

	interval = calendar_config_get_default_reminder_interval ();
	units = calendar_config_get_default_reminder_units ();

	alarm = e_cal_component_alarm_new ();

	/* We don't set the description of the alarm; we'll copy it from the
	 * summary when it gets committed to the server. For that, we add a
	 * X-EVOLUTION-NEEDS-DESCRIPTION property to the alarm's component.
	 */
	icalcomp = e_cal_component_alarm_get_icalcomponent (alarm);
	icalprop = icalproperty_new_x ("1");
	icalproperty_set_x_name (icalprop, "X-EVOLUTION-NEEDS-DESCRIPTION");
	icalcomponent_add_property (icalcomp, icalprop);

	e_cal_component_alarm_set_action (alarm, E_CAL_COMPONENT_ALARM_DISPLAY);

	trigger.type = E_CAL_COMPONENT_ALARM_TRIGGER_RELATIVE_START;

	memset (&trigger.u.rel_duration, 0, sizeof (trigger.u.rel_duration));

	trigger.u.rel_duration.is_neg = TRUE;

	switch (units) {
	case CAL_MINUTES:
		trigger.u.rel_duration.minutes = interval;
		break;

	case CAL_HOURS:	
		trigger.u.rel_duration.hours = interval;
		break;

	case CAL_DAYS:	
		trigger.u.rel_duration.days = interval;
		break;

	default:
		g_assert_not_reached ();
	}

	e_cal_component_alarm_set_trigger (alarm, trigger);

	e_cal_component_add_alarm (comp, alarm);
	e_cal_component_alarm_free (alarm);

	return comp;
}

ECalComponent *
cal_comp_event_new_with_current_time (ECal *client, gboolean all_day) 
{
	ECalComponent *comp;
	struct icaltimetype itt;
	ECalComponentDateTime dt;
	icaltimezone *zone;

	comp = cal_comp_event_new_with_defaults (client);

	g_return_val_if_fail (comp, NULL);
	
	zone = calendar_config_get_icaltimezone ();
	if (all_day) {
		itt = icaltime_from_timet_with_zone (time (NULL), 1, zone);

		dt.value = &itt;
		dt.tzid = icaltimezone_get_tzid (zone);
		
		e_cal_component_set_dtstart (comp, &dt);
		e_cal_component_set_dtend (comp, &dt);		
	} else {
		itt = icaltime_current_time_with_zone (zone);
		icaltime_adjust (&itt, 0, 1, -itt.minute, -itt.second);
		
		dt.value = &itt;
		dt.tzid = icaltimezone_get_tzid (zone);
		
		e_cal_component_set_dtstart (comp, &dt);
		icaltime_adjust (&itt, 0, 1, 0, 0);
		e_cal_component_set_dtend (comp, &dt);
	}

	return comp;
}

ECalComponent *
cal_comp_task_new_with_defaults (ECal *client)
{
	ECalComponent *comp;
	icalcomponent *icalcomp;

	if (!e_cal_get_default_object (client, &icalcomp, NULL))
		return NULL;

	comp = e_cal_component_new ();
	if (!e_cal_component_set_icalcomponent (comp, icalcomp)) {
		g_object_unref (comp);
		icalcomponent_free (icalcomp);

		return NULL;
	}

	comp = e_cal_component_new ();
	if (!e_cal_component_set_icalcomponent (comp, icalcomp)) {
		g_object_unref (comp);
		icalcomponent_free (icalcomp);
		return NULL;
	}

	return comp;
}
