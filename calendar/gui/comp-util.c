/* Evolution calendar - Utilities for manipulating CalComponent objects
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

#include "comp-util.h"



/**
 * cal_comp_util_add_exdate:
 * @comp: A calendar component object.
 * @itt: Time for the exception.
 * 
 * Adds an exception date to the current list of EXDATE properties in a calendar
 * component object.
 **/
void
cal_comp_util_add_exdate (CalComponent *comp, time_t t, icaltimezone *zone)
{
	GSList *list;
	CalComponentDateTime *cdt;

	g_return_if_fail (comp != NULL);
	g_return_if_fail (IS_CAL_COMPONENT (comp));

	cal_component_get_exdate_list (comp, &list);

	cdt = g_new (CalComponentDateTime, 1);
	cdt->value = g_new (struct icaltimetype, 1);
	*cdt->value = icaltime_from_timet_with_zone (t, FALSE, zone);
	cdt->tzid = g_strdup (icaltimezone_get_tzid (zone));

	list = g_slist_append (list, cdt);
	cal_component_set_exdate_list (comp, list);
	cal_component_free_exdate_list (list);
}



/* Returns TRUE if the TZIDs are equivalent, i.e. both NULL or the same. */
static gboolean
cal_component_compare_tzid (const char *tzid1, const char *tzid2)
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
 * @client: A #CalClient.
 * 
 * Checks if the component uses the given timezone for both the start and
 * the end time, or if the UTC offsets of the start and end times are the same
 * as in the given zone.
 *
 * Returns: TRUE if the component's start and end time are at the same UTC
 * offset in the given timezone.
 **/
gboolean
cal_comp_util_compare_event_timezones (CalComponent *comp,
				       CalClient *client,
				       icaltimezone *zone)
{
	CalClientGetStatus status;
	CalComponentDateTime start_datetime, end_datetime;
	const char *tzid;
	gboolean retval = FALSE;
	icaltimezone *start_zone, *end_zone;
	int offset1, offset2;

	tzid = icaltimezone_get_tzid (zone);

	cal_component_get_dtstart (comp, &start_datetime);
	cal_component_get_dtend (comp, &end_datetime);

	/* If either the DTSTART or the DTEND is a DATE value, we return TRUE.
	   Maybe if one was a DATE-TIME we should check that, but that should
	   not happen often. */
	if (start_datetime.value->is_date || end_datetime.value->is_date) {
		retval = TRUE;
		goto out;
	}

	/* FIXME: DURATION may be used instead. */
	if (cal_component_compare_tzid (tzid, start_datetime.tzid)
	    && cal_component_compare_tzid (tzid, end_datetime.tzid)) {
		/* If both TZIDs are the same as the given zone's TZID, then
		   we know the timezones are the same so we return TRUE. */
		retval = TRUE;
	} else {
		/* If the TZIDs differ, we have to compare the UTC offsets
		   of the start and end times, using their own timezones and
		   the given timezone. */
		status = cal_client_get_timezone (client,
						  start_datetime.tzid,
						  &start_zone);
		if (status != CAL_CLIENT_GET_SUCCESS)
			goto out;

		offset1 = icaltimezone_get_utc_offset (start_zone,
						       start_datetime.value,
						       NULL);
		offset2 = icaltimezone_get_utc_offset (zone,
						       start_datetime.value,
						       NULL);
		if (offset1 == offset2) {
			status = cal_client_get_timezone (client,
							  end_datetime.tzid,
							  &end_zone);
			if (status != CAL_CLIENT_GET_SUCCESS)
				goto out;

			offset1 = icaltimezone_get_utc_offset (end_zone,
							       end_datetime.value,
							       NULL);
			offset2 = icaltimezone_get_utc_offset (zone,
							       end_datetime.value,
							       NULL);
			if (offset1 == offset2)
				retval = TRUE;
		}
	}

 out:

	cal_component_free_datetime (&start_datetime);
	cal_component_free_datetime (&end_datetime);

	return retval;
}
