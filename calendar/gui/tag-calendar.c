/* Evolution calendar - Utilities for tagging ECalendar widgets
 *
 * Copyright (C) 2000 Helix Code, Inc.
 *
 * Author: Federico Mena-Quintero <federico@helixcode.com>
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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <cal-util/timeutil.h>
#include "tag-calendar.h"



struct calendar_tag_closure {
	ECalendarItem *calitem;
	time_t start_time;
	time_t end_time;
};

/* Clears all the tags in a calendar and fills a closure structure with the
 * necessary information for iterating over occurrences.
 */
static void
prepare_tag (ECalendar *ecal, struct calendar_tag_closure *c)
{
	gint start_year, start_month, start_day;
	gint end_year, end_month, end_day;
	struct tm start_tm = { 0 }, end_tm = { 0 };

	e_calendar_item_clear_marks (ecal->calitem);

	e_calendar_item_get_date_range	(ecal->calitem,
					 &start_year, &start_month, &start_day,
					 &end_year, &end_month, &end_day);

	start_tm.tm_year = start_year - 1900;
	start_tm.tm_mon = start_month;
	start_tm.tm_mday = start_day;
	start_tm.tm_hour = 0;
	start_tm.tm_min = 0;
	start_tm.tm_sec = 0;
	start_tm.tm_isdst = -1;

	end_tm.tm_year = end_year - 1900;
	end_tm.tm_mon = end_month;
	end_tm.tm_mday = end_day;
	end_tm.tm_hour = 0;
	end_tm.tm_min = 0;
	end_tm.tm_sec = 0;
	end_tm.tm_isdst = -1;

	c->calitem = ecal->calitem;
	c->start_time = mktime (&start_tm);
	c->end_time = mktime (&end_tm);
}

/* Marks the specified range in an ECalendar; called from cal_client_generate_instances() */
static gboolean
tag_calendar_cb (CalComponent *comp,
		 time_t istart,
		 time_t iend,
		 gpointer data)
{
	struct calendar_tag_closure *c = data;
	time_t t;

	t = time_day_begin (istart);

	do {
		struct tm tm;

		tm = *localtime (&t);

		e_calendar_item_mark_day (c->calitem, tm.tm_year + 1900,
					  tm.tm_mon, tm.tm_mday,
					  E_CALENDAR_ITEM_MARK_BOLD);

		t = time_day_end (t);
	} while (t < iend);

	return TRUE;
}

/**
 * tag_calendar_by_client:
 * @ecal: Calendar widget to tag.
 * @client: A calendar client object.
 * 
 * Tags an #ECalendar widget with the events that occur in its current time
 * range.  The occurrences are extracted from the specified calendar @client.
 **/
void
tag_calendar_by_client (ECalendar *ecal, CalClient *client)
{
	struct calendar_tag_closure c;

	g_return_if_fail (ecal != NULL);
	g_return_if_fail (E_IS_CALENDAR (ecal));
	g_return_if_fail (client != NULL);
	g_return_if_fail (IS_CAL_CLIENT (client));

	/* If the ECalendar isn't visible, we just return. */
	if (!GTK_WIDGET_VISIBLE (ecal))
		return;

	prepare_tag (ecal, &c);

	if (!cal_client_is_loaded (client))
		return;

	cal_client_generate_instances (client, CALOBJ_TYPE_EVENT,
				       c.start_time, c.end_time,
				       tag_calendar_cb, &c);
}

/**
 * tag_calendar_by_comp:
 * @ecal: Calendar widget to tag.
 * @comp: A calendar component object.
 * 
 * Tags an #ECalendar widget with any occurrences of a specific calendar
 * component that occur within the calendar's current time range.
 **/
void
tag_calendar_by_comp (ECalendar *ecal, CalComponent *comp)
{
	struct calendar_tag_closure c;

	g_return_if_fail (ecal != NULL);
	g_return_if_fail (E_IS_CALENDAR (ecal));
	g_return_if_fail (comp != NULL);
	g_return_if_fail (IS_CAL_COMPONENT (comp));

	/* If the ECalendar isn't visible, we just return. */
	if (!GTK_WIDGET_VISIBLE (ecal))
		return;

	prepare_tag (ecal, &c);
	cal_recur_generate_instances (comp, c.start_time, c.end_time, tag_calendar_cb, &c);
}
