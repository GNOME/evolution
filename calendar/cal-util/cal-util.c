/* Evolution calendar utilities and types
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

#include <config.h>
#include <stdlib.h>
#include <string.h>
#include <glib.h>
#include <libgnome/gnome-i18n.h>
#include <libgnome/gnome-util.h>
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

static char *
get_line_fn (char *buf, size_t size, void *file)
{
	return fgets (buf, size, file);
}

icalcomponent *
cal_util_parse_ics_file (const char *filename)
{
	icalparser *parser;
	icalcomponent *icalcomp;
	FILE *file;

	file = fopen (filename, "r");
	if (!file)
		return NULL;

	parser = icalparser_new ();
	icalparser_set_gen_data (parser, file);

	icalcomp = icalparser_parse (parser, get_line_fn);
	icalparser_free (parser);
	fclose (file);

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
	time_t repeat_time;

	*alarm_start = start;
	*alarm_end = end;

	repeat_time = 0;

	for (l = alarm_uids; l; l = l->next) {
		const char *auid;
		CalComponentAlarm *alarm;
		CalAlarmTrigger trigger;
		struct icaldurationtype *dur;
		time_t dur_time;
		CalAlarmRepeat repeat;

		auid = l->data;
		alarm = cal_component_get_alarm (comp, auid);
		g_assert (alarm != NULL);

		cal_component_alarm_get_trigger (alarm, &trigger);
		cal_component_alarm_get_repeat (alarm, &repeat);
		cal_component_alarm_free (alarm);

		switch (trigger.type) {
		case CAL_ALARM_TRIGGER_NONE:
		case CAL_ALARM_TRIGGER_ABSOLUTE:
			break;

		case CAL_ALARM_TRIGGER_RELATIVE_START:
		case CAL_ALARM_TRIGGER_RELATIVE_END:
			dur = &trigger.u.rel_duration;
  			dur_time = icaldurationtype_as_int (*dur);

			if (repeat.repetitions != 0) {
				int rdur;

				rdur = repeat.repetitions * icaldurationtype_as_int (repeat.duration);
				repeat_time = MAX (repeat_time, rdur);
			}

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

	*alarm_start -= repeat_time;

	g_assert (*alarm_start <= *alarm_end);
}

/* Closure data to generate alarm occurrences */
struct alarm_occurrence_data {
	/* These are the info we have */
	GList *alarm_uids;
	time_t start;
	time_t end;
	CalAlarmAction *omit;
	
	/* This is what we compute */
	GSList *triggers;
	int n_triggers;
};

static void
add_trigger (struct alarm_occurrence_data *aod, const char *auid, time_t trigger,
	     time_t occur_start, time_t occur_end)
{
	CalAlarmInstance *instance;

	instance = g_new (CalAlarmInstance, 1);
	instance->auid = auid;
	instance->trigger = trigger;
	instance->occur_start = occur_start;
	instance->occur_end = occur_end;

	aod->triggers = g_slist_prepend (aod->triggers, instance);
	aod->n_triggers++;
}

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
		CalAlarmAction action;
		CalAlarmTrigger trigger;
		CalAlarmRepeat repeat;
		struct icaldurationtype *dur;
		time_t dur_time;
		time_t occur_time, trigger_time;
		int i;
		
		auid = l->data;
		alarm = cal_component_get_alarm (comp, auid);
		g_assert (alarm != NULL);

		cal_component_alarm_get_action (alarm, &action);
		cal_component_alarm_get_trigger (alarm, &trigger);
		cal_component_alarm_get_repeat (alarm, &repeat);
		cal_component_alarm_free (alarm);

		for (i = 0; aod->omit[i] != -1; i++) {
			if (aod->omit[i] == action)
				break;
		}
		if (aod->omit[i] != -1)
			continue;
		
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

		/* Add repeating alarms */

		if (repeat.repetitions != 0) {
			int i;
			time_t repeat_time;

			repeat_time = icaldurationtype_as_int (repeat.duration);

			for (i = 0; i < repeat.repetitions; i++) {
				time_t t;

				t = trigger_time + (i + 1) * repeat_time;

				if (t >= aod->start && t < aod->end)
					add_trigger (aod, auid, t, start, end);
			}
		}

		/* Add the trigger itself */

		if (trigger_time >= aod->start && trigger_time < aod->end)
			add_trigger (aod, auid, trigger_time, start, end);
	}

	return TRUE;
}

/* Generates the absolute triggers for a component */
static void
generate_absolute_triggers (CalComponent *comp, struct alarm_occurrence_data *aod,
			    CalRecurResolveTimezoneFn resolve_tzid,
			    gpointer user_data,
			    icaltimezone *default_timezone)
{
	GList *l;
	CalComponentDateTime dt_start, dt_end;

	cal_component_get_dtstart (comp, &dt_start);
	cal_component_get_dtend (comp, &dt_end);

	for (l = aod->alarm_uids; l; l = l->next) {
		const char *auid;
		CalComponentAlarm *alarm;
		CalAlarmAction action;
		CalAlarmRepeat repeat;
		CalAlarmTrigger trigger;
		time_t abs_time;
		time_t occur_start, occur_end;
		icaltimezone *zone;
		int i;
		
		auid = l->data;
		alarm = cal_component_get_alarm (comp, auid);
		g_assert (alarm != NULL);

		cal_component_alarm_get_action (alarm, &action);
		cal_component_alarm_get_trigger (alarm, &trigger);
		cal_component_alarm_get_repeat (alarm, &repeat);
		cal_component_alarm_free (alarm);

		for (i = 0; aod->omit[i] != -1; i++) {
			if (aod->omit[i] == action)
				break;
		}
		if (aod->omit[i] != -1)
			continue;

		if (trigger.type != CAL_ALARM_TRIGGER_ABSOLUTE)
			continue;

		/* Absolute triggers are always in UTC; see RFC 2445 section 4.8.6.3 */
		zone = icaltimezone_get_utc_timezone ();

		abs_time = icaltime_as_timet_with_zone (trigger.u.abs_time, zone);

		/* No particular occurrence, so just use the times from the component */

		if (dt_start.value) {
			if (dt_start.tzid && !dt_start.value->is_date)
				zone = (* resolve_tzid) (dt_start.tzid, user_data);
			else
				zone = default_timezone;

			occur_start = icaltime_as_timet_with_zone (*dt_start.value, zone);
		} else
			occur_start = -1;

		if (dt_end.value) {
			if (dt_end.tzid && !dt_end.value->is_date)
				zone = (* resolve_tzid) (dt_end.tzid, user_data);
			else
				zone = default_timezone;

			occur_end = icaltime_as_timet_with_zone (*dt_end.value, zone);
		} else
			occur_end = -1;

		/* Add repeating alarms */

		if (repeat.repetitions != 0) {
			int i;
			time_t repeat_time;

			repeat_time = icaldurationtype_as_int (repeat.duration);

			for (i = 0; i < repeat.repetitions; i++) {
				time_t t;

				t = abs_time + (i + 1) * repeat_time;

				if (t >= aod->start && t < aod->end)
					add_trigger (aod, auid, t, occur_start, occur_end);
			}
		}

		/* Add the trigger itself */

		if (abs_time >= aod->start && abs_time < aod->end)
			add_trigger (aod, auid, abs_time, occur_start, occur_end);
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
 * @default_timezone: the timezone used to resolve DATE and floating DATE-TIME
 * values.
 *
 * Generates alarm instances for a calendar component.  Returns the instances
 * structure, or NULL if no alarm instances occurred in the specified time
 * range.
 */
CalComponentAlarms *
cal_util_generate_alarms_for_comp (CalComponent *comp,
				   time_t start,
				   time_t end,
				   CalAlarmAction *omit,
				   CalRecurResolveTimezoneFn resolve_tzid,
				   gpointer user_data,
				   icaltimezone *default_timezone)
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
	aod.omit = omit;
	aod.triggers = NULL;
	aod.n_triggers = 0;

	cal_recur_generate_instances (comp, alarm_start, alarm_end,
				      add_alarm_occurrences_cb, &aod,
				      resolve_tzid, user_data,
				      default_timezone);

	/* We add the ABSOLUTE triggers separately */
	generate_absolute_triggers (comp, &aod, resolve_tzid, user_data, default_timezone);

	if (aod.n_triggers == 0)
		return NULL;

	/* Create the component alarm instances structure */

	alarms = g_new (CalComponentAlarms, 1);
	alarms->comp = comp;
	g_object_ref (G_OBJECT (alarms->comp));
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
 * @default_timezone: the timezone used to resolve DATE and floating DATE-TIME
 * values.
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
				   CalAlarmAction *omit,
				   GSList **comp_alarms,
				   CalRecurResolveTimezoneFn resolve_tzid,
				   gpointer user_data,
				   icaltimezone *default_timezone)
{
	GList *l;
	int n;

	n = 0;

	for (l = comps; l; l = l->next) {
		CalComponent *comp;
		CalComponentAlarms *alarms;

		comp = CAL_COMPONENT (l->data);
		alarms = cal_util_generate_alarms_for_comp (comp, start, end, omit, resolve_tzid, user_data, default_timezone);

		if (alarms) {
			*comp_alarms = g_slist_prepend (*comp_alarms, alarms);
			n++;
		}
	}

	return n;
}


/* Converts an iCalendar PRIORITY value to a translated string. Any unknown
   priority value (i.e. not 0-9) will be returned as "" (undefined). */
char *
cal_util_priority_to_string (int priority)
{
	char *retval;

	if (priority <= 0)
		retval = "";
	else if (priority <= 4)
		retval = _("High");
	else if (priority == 5)
		retval = _("Normal");
	else if (priority <= 9)
		retval = _("Low");
	else
		retval = "";

	return retval;
}


/* Converts a translated priority string to an iCalendar priority value.
   Returns -1 if the priority string is not valid. */
int
cal_util_priority_from_string (const char *string)
{
	int priority;

	/* An empty string is the same as 'None'. */
	if (!string || !string[0] || !g_strcasecmp (string, _("Undefined")))
		priority = 0;
	else if (!g_strcasecmp (string, _("High")))
		priority = 3;
	else if (!g_strcasecmp (string, _("Normal")))
		priority = 5;
	else if (!g_strcasecmp (string, _("Low")))
		priority = 7;
	else
		priority = -1;

	return priority;
}

char *
cal_util_expand_uri (char *uri, gboolean tasks)
{
	return g_strdup (uri);
}

/* callback for icalcomponent_foreach_tzid */
typedef struct {
	icalcomponent *vcal_comp;
	icalcomponent *icalcomp;
} ForeachTzidData;

static void
add_timezone_cb (icalparameter *param, void *data)
{
	icaltimezone *tz;
	const char *tzid;
	icalcomponent *vtz_comp;
	ForeachTzidData *f_data = (ForeachTzidData *) data;

	tzid = icalparameter_get_tzid (param);
	if (!tzid)
		return;

	tz = icalcomponent_get_timezone (f_data->vcal_comp, tzid);
	if (tz)
		return;

	tz = icalcomponent_get_timezone (f_data->icalcomp, tzid);
	if (!tz) {
		tz = icaltimezone_get_builtin_timezone_from_tzid (tzid);
		if (!tz)
			return;
	}

	vtz_comp = icaltimezone_get_component (tz);
	if (!vtz_comp)
		return;

	icalcomponent_add_component (f_data->vcal_comp,
				     icalcomponent_new_clone (vtz_comp));
}

/* Adds VTIMEZONE components to a VCALENDAR for all tzid's
 * in the given CalComponent. */
void
cal_util_add_timezones_from_component (icalcomponent *vcal_comp,
				       icalcomponent *icalcomp)
{
	ForeachTzidData f_data;

	g_return_if_fail (vcal_comp != NULL);
	g_return_if_fail (icalcomp != NULL);;

	f_data.vcal_comp = vcal_comp;
	f_data.icalcomp = icalcomp;
	icalcomponent_foreach_tzid (icalcomp, add_timezone_cb, &f_data);
}

gboolean
cal_util_component_is_instance (icalcomponent *icalcomp)
{
	icalproperty *prop;

	g_return_val_if_fail (icalcomp != NULL, FALSE);

	prop = icalcomponent_get_first_property (icalcomp, ICAL_RECURRENCEID_PROPERTY);
	return prop ? TRUE : FALSE;
}

gboolean
cal_util_component_has_alarms (icalcomponent *icalcomp)
{
	icalcomponent *alarm;

	g_return_val_if_fail (icalcomp != NULL, FALSE);

	alarm = icalcomponent_get_first_component (icalcomp, ICAL_VALARM_COMPONENT);
	return alarm ? TRUE : FALSE;
}

gboolean
cal_util_component_has_organizer (icalcomponent *icalcomp)
{
	icalproperty *prop;

	g_return_val_if_fail (icalcomp != NULL, FALSE);

	prop = icalcomponent_get_first_property (icalcomp, ICAL_ORGANIZER_PROPERTY);
	return prop ? TRUE : FALSE;
}

gboolean
cal_util_component_has_recurrences (icalcomponent *icalcomp)
{
	g_return_val_if_fail (icalcomp != NULL, FALSE);

	return cal_util_component_has_rdates (icalcomp) || cal_util_component_has_rrules (icalcomp);
}

gboolean
cal_util_component_has_rdates (icalcomponent *icalcomp)
{
	icalproperty *prop;

	g_return_val_if_fail (icalcomp != NULL, FALSE);

	prop = icalcomponent_get_first_property (icalcomp, ICAL_RDATE_PROPERTY);
	return prop ? TRUE : FALSE;
}

gboolean
cal_util_component_has_rrules (icalcomponent *icalcomp)
{
	icalproperty *prop;

	g_return_val_if_fail (icalcomp != NULL, FALSE);

	prop = icalcomponent_get_first_property (icalcomp, ICAL_RRULE_PROPERTY);
	return prop ? TRUE : FALSE;
}

gboolean
cal_util_event_dates_match (icalcomponent *icalcomp1, icalcomponent *icalcomp2)
{
	struct icaltimetype c1_dtstart, c1_dtend, c2_dtstart, c2_dtend;

	g_return_val_if_fail (icalcomp1 != NULL, FALSE);
	g_return_val_if_fail (icalcomp2 != NULL, FALSE);

	c1_dtstart = icalcomponent_get_dtstart (icalcomp1);
	c1_dtend = icalcomponent_get_dtend (icalcomp1);
	c2_dtstart = icalcomponent_get_dtstart (icalcomp2);
	c2_dtend = icalcomponent_get_dtend (icalcomp2);

	/* if either value is NULL, they must both be NULL to match */
	if (icaltime_is_valid_time (c1_dtstart) || icaltime_is_valid_time (c2_dtstart)) {
		if (!(icaltime_is_valid_time (c1_dtstart) && icaltime_is_valid_time (c2_dtstart)))
			return FALSE;
	} else {
		if (icaltime_compare (c1_dtstart, c2_dtstart))
			return FALSE;
	}

	if (icaltime_is_valid_time (c1_dtend) || icaltime_is_valid_time (c2_dtend)) {
		if (!(icaltime_is_valid_time (c1_dtend) && icaltime_is_valid_time (c2_dtend)))
			return FALSE;
	} else {
		if (icaltime_compare (c1_dtend, c2_dtend))
			return FALSE;
	}

	

	/* now match the timezones */
	if (!(!c1_dtstart.zone && !c2_dtstart.zone) ||
	    (c1_dtstart.zone && c2_dtstart.zone &&
	     !strcmp (icaltimezone_get_tzid ((icaltimezone *) c1_dtstart.zone),
		      icaltimezone_get_tzid ((icaltimezone *) c2_dtstart.zone))))
		return FALSE;

	if (!(!c1_dtend.zone && !c2_dtend.zone) ||
	    (c1_dtend.zone && c2_dtend.zone &&
	     !strcmp (icaltimezone_get_tzid ((icaltimezone *) c1_dtend.zone),
		      icaltimezone_get_tzid ((icaltimezone *) c2_dtend.zone))))
		return FALSE;

	return TRUE;
}
