/* Evolution calendar utilities and types
 *
 * Copyright (C) 2000 Ximian, Inc.
 * Copyright (C) 2000 Ximian, Inc.
 *
 * Author: Federico Mena-Quintero <federico@ximian.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
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

#include <config.h>
#include <stdlib.h>
#include "cal-util.h"



/**
 * cal_obj_instance_list_free:
 * @list: List of #CalObjInstance structures.
 * 
 * Frees a list of #CalObjInstance structures.
 **/
void
cal_obj_instance_list_free (GList *list)
{
	CalObjInstance *i;
	GList *l;

	for (l = list; l; l = l->next) {
		i = l->data;

		g_assert (i != NULL);
		g_assert (i->uid != NULL);

		g_free (i->uid);
		g_free (i);
	}

	g_list_free (list);
}

/**
 * cal_obj_uid_list_free:
 * @list: List of strings with unique identifiers.
 *
 * Frees a list of unique identifiers for calendar objects.
 **/
void
cal_obj_uid_list_free (GList *list)
{
	GList *l;

	for (l = list; l; l = l->next) {
		char *uid;

		uid = l->data;

		g_assert (uid != NULL);
		g_free (uid);
	}

	g_list_free (list);
}

icalcomponent *
cal_util_new_top_level (void)
{
	icalcomponent *icalcomp;
	icalproperty *prop;
	
	icalcomp = icalcomponent_new (ICAL_VCALENDAR_COMPONENT);

	/* RFC 2445, section 4.7.1 */
	prop = icalproperty_new_calscale ("GREGORIAN");
	icalcomponent_add_property (icalcomp, prop);
	
       /* RFC 2445, section 4.7.3 */
	prop = icalproperty_new_prodid ("-//Ximian//NONSGML Evolution Calendar//EN");
	icalcomponent_add_property (icalcomp, prop);

	/* RFC 2445, section 4.7.4.  This is the iCalendar spec version, *NOT*
	 * the product version!  Do not change this!
	 */
	prop = icalproperty_new_version ("2.0");
	icalcomponent_add_property (icalcomp, prop);

	return icalcomp;
}

/* Computes the range of time in which recurrences should be generated for a
 * component in order to compute alarm trigger times.
 */
static void
compute_alarm_range (CalComponent *comp, GList *alarm_uids, time_t start, time_t end,
		     time_t *alarm_start, time_t *alarm_end)
{
	GList *l;

	*alarm_start = start;
	*alarm_end = end;

	for (l = alarm_uids; l; l = l->next) {
		const char *auid;
		CalComponentAlarm *alarm;
		CalAlarmTrigger trigger;
		struct icaldurationtype *dur;
		time_t dur_time;

		auid = l->data;
		alarm = cal_component_get_alarm (comp, auid);
		g_assert (alarm != NULL);

		cal_component_alarm_get_trigger (alarm, &trigger);
		cal_component_alarm_free (alarm);

		switch (trigger.type) {
		case CAL_ALARM_TRIGGER_NONE:
		case CAL_ALARM_TRIGGER_ABSOLUTE:
			continue;

		case CAL_ALARM_TRIGGER_RELATIVE_START:
		case CAL_ALARM_TRIGGER_RELATIVE_END:
			dur = &trigger.u.rel_duration;
  			dur_time = icaldurationtype_as_int (*dur);

			if (dur->is_neg)
				/* If the duration is negative then dur_time
				 * will be negative as well; that is why we
				 * subtract to expand the range.
				 */
				*alarm_end = MAX (*alarm_end, end - dur_time);
			else
				*alarm_start = MIN (*alarm_start, start - dur_time);

			break;

		default:
			g_assert_not_reached ();
		}
	}

	g_assert (*alarm_start <= *alarm_end);
}

/* Closure data to generate alarm occurrences */
struct alarm_occurrence_data {
	/* These are the info we have */
	GList *alarm_uids;
	time_t start;
	time_t end;

	/* This is what we compute */
	GSList *triggers;
	int n_triggers;
};

/* Callback used from cal_recur_generate_instances(); generates triggers for all
 * of a component's RELATIVE alarms.
 */
static gboolean
add_alarm_occurrences_cb (CalComponent *comp, time_t start, time_t end, gpointer data)
{
	struct alarm_occurrence_data *aod;
	GList *l;

	aod = data;

	for (l = aod->alarm_uids; l; l = l->next) {
		const char *auid;
		CalComponentAlarm *alarm;
		CalAlarmTrigger trigger;
		struct icaldurationtype *dur;
		time_t dur_time;
		time_t occur_time, trigger_time;
		CalAlarmInstance *instance;

		auid = l->data;
		alarm = cal_component_get_alarm (comp, auid);
		g_assert (alarm != NULL);

		cal_component_alarm_get_trigger (alarm, &trigger);
		cal_component_alarm_free (alarm);

		if (trigger.type != CAL_ALARM_TRIGGER_RELATIVE_START
		    && trigger.type != CAL_ALARM_TRIGGER_RELATIVE_END)
			continue;

		dur = &trigger.u.rel_duration;
  		dur_time = icaldurationtype_as_int (*dur);

		if (trigger.type == CAL_ALARM_TRIGGER_RELATIVE_START)
			occur_time = start;
		else
			occur_time = end;

		/* If dur->is_neg is true then dur_time will already be
		 * negative.  So we do not need to test for dur->is_neg here; we
		 * can simply add the dur_time value to the occur_time and get
		 * the correct result.
		 */

		trigger_time = occur_time + dur_time;

		if (trigger_time < aod->start || trigger_time >= aod->end)
			continue;

		instance = g_new (CalAlarmInstance, 1);
		instance->auid = auid;
		instance->trigger = trigger_time;
		instance->occur_start = start;
		instance->occur_end = end;

		aod->triggers = g_slist_prepend (aod->triggers, instance);
		aod->n_triggers++;
	}

	return TRUE;
}

/* Generates the absolute triggers for a component */
static void
generate_absolute_triggers (CalComponent *comp, struct alarm_occurrence_data *aod)
{
	GList *l;
	CalComponentDateTime dt_start, dt_end;

	cal_component_get_dtstart (comp, &dt_start);
	cal_component_get_dtend (comp, &dt_end);

	for (l = aod->alarm_uids; l; l = l->next) {
		const char *auid;
		CalComponentAlarm *alarm;
		CalAlarmTrigger trigger;
		time_t abs_time;
		CalAlarmInstance *instance;

		auid = l->data;
		alarm = cal_component_get_alarm (comp, auid);
		g_assert (alarm != NULL);

		cal_component_alarm_get_trigger (alarm, &trigger);
		cal_component_alarm_free (alarm);

		if (trigger.type != CAL_ALARM_TRIGGER_ABSOLUTE)
			continue;

		abs_time = icaltime_as_timet (trigger.u.abs_time);

		if (abs_time < aod->start || abs_time >= aod->end)
			continue;

		instance = g_new (CalAlarmInstance, 1);
		instance->auid = auid;
		instance->trigger = abs_time;

		/* No particular occurrence, so just use the times from the component */

		if (dt_start.value)
			instance->occur_start = icaltime_as_timet (*dt_start.value);
		else
			instance->occur_start = -1;

		if (dt_end.value)
			instance->occur_end = icaltime_as_timet (*dt_end.value);
		else
			instance->occur_end = -1;

		aod->triggers = g_slist_prepend (aod->triggers, instance);
		aod->n_triggers++;
	}

	cal_component_free_datetime (&dt_start);
	cal_component_free_datetime (&dt_end);
}

/* Compares two alarm instances; called from g_slist_sort() */
static gint
compare_alarm_instance (gconstpointer a, gconstpointer b)
{
	const CalAlarmInstance *aia, *aib;

	aia = a;
	aib = b;

	if (aia->trigger < aib->trigger)
		return -1;
	else if (aia->trigger > aib->trigger)
		return 1;
	else
		return 0;
}

/**
 * cal_util_generate_alarms_for_comp
 * @comp: the CalComponent to generate alarms from
 * @start: start time
 * @end: end time
 * @resolve_tzid: callback for resolving timezones
 * @user_data: data to be passed to the resolve_tzid callback
 *
 * Generates alarm instances for a calendar component.  Returns the instances
 * structure, or NULL if no alarm instances occurred in the specified time
 * range.
 */
CalComponentAlarms *
cal_util_generate_alarms_for_comp (CalComponent *comp,
				   time_t start,
				   time_t end,
				   CalRecurResolveTimezoneFn resolve_tzid,
				   gpointer user_data)
{
	GList *alarm_uids;
	time_t alarm_start, alarm_end;
	struct alarm_occurrence_data aod;
	CalComponentAlarms *alarms;

	if (!cal_component_has_alarms (comp))
		return NULL;

	alarm_uids = cal_component_get_alarm_uids (comp);
	compute_alarm_range (comp, alarm_uids, start, end, &alarm_start, &alarm_end);

	aod.alarm_uids = alarm_uids;
	aod.start = start;
	aod.end = end;
	aod.triggers = NULL;
	aod.n_triggers = 0;

	cal_recur_generate_instances (comp, alarm_start, alarm_end,
				      add_alarm_occurrences_cb, &aod,
				      resolve_tzid, user_data);

	/* We add the ABSOLUTE triggers separately */
	generate_absolute_triggers (comp, &aod);

	if (aod.n_triggers == 0)
		return NULL;

	/* Create the component alarm instances structure */

	alarms = g_new (CalComponentAlarms, 1);
	alarms->comp = comp;
	gtk_object_ref (GTK_OBJECT (alarms->comp));
	alarms->alarms = g_slist_sort (aod.triggers, compare_alarm_instance);

	return alarms;
}

/**
 * cal_util_generate_alarms_for_list
 * @comps: list of CalComponent's
 * @start: start time
 * @end: end time
 * @comp_alarms: list to be returned
 * @resolve_tzid: callback for resolving timezones
 * @user_data: data to be passed to the resolve_tzid callback
 *
 * Iterates through all the components in the comps list and generates alarm
 * instances for them; putting them in the comp_alarms list.
 *
 * Returns: the number of elements it added to that list.
 */
int
cal_util_generate_alarms_for_list (GList *comps,
				   time_t start,
				   time_t end,
				   GSList **comp_alarms,
				   CalRecurResolveTimezoneFn resolve_tzid,
				   gpointer user_data)
{
	GList *l;
	int n;

	n = 0;

	for (l = comps; l; l = l->next) {
		CalComponent *comp;
		CalComponentAlarms *alarms;

		comp = CAL_COMPONENT (l->data);
		alarms = cal_util_generate_alarms_for_comp (comp, start, end, resolve_tzid, user_data);

		if (alarms) {
			*comp_alarms = g_slist_prepend (*comp_alarms, alarms);
			n++;
		}
	}

	return n;
}
