/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Evolution calendar recurrence rule functions
 *
 * Copyright (C) 2000 Helix Code, Inc.
 *
 * Author: Damon Chaplin <damon@helixcode.com>
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

#ifndef CAL_RECUR_H
#define CAL_RECUR_H

#include <libgnome/gnome-defs.h>
#include <glib.h>

BEGIN_GNOME_DECLS


/* FIXME: I've put modified versions of RecurType and Recurrence here, since
   the ones in calobj.h don't support all of iCalendar. Hopefully Seth will
   update those soon and these can be removed. */

typedef enum {
	CAL_RECUR_YEARLY,
	CAL_RECUR_MONTHLY,
	CAL_RECUR_WEEKLY,
	CAL_RECUR_DAILY,
	CAL_RECUR_HOURLY,
	CAL_RECUR_MINUTELY,
	CAL_RECUR_SECONDLY
} CalObjRecurType;

typedef struct {
	CalObjRecurType	type;

	int            interval;

	/* Specifies the end of the recurrence. No occurrences are generated
	   after this date. If it is 0, the event recurs forever. */
	time_t         enddate;

	/* WKST property - the week start day: 0 = Monday to 6 = Sunday. */
	gint	       week_start_day;


	/* NOTE: I've used GList's here, but it doesn't matter if we use
	   other data structures like arrays. The code should be easy to
	   change. So long as it is easy to see if the modifier is set. */

	/* For BYMONTH modifier. A list of GINT_TO_POINTERs, 0-11. */
	GList	      *bymonth;

	/* For BYWEEKNO modifier. A list of GINT_TO_POINTERs, [+-]1-53. */
	GList	      *byweekno;

	/* For BYYEARDAY modifier. A list of GINT_TO_POINTERs, [+-]1-366. */
	GList	      *byyearday;

	/* For BYMONTHDAY modifier. A list of GINT_TO_POINTERs, [+-]1-31. */
	GList	      *bymonthday;

	/* For BYDAY modifier. A list of GINT_TO_POINTERs, in pairs.
	   The first of each pair is the weekday, 0 = Monday to 6 = Sunday.
	   The second of each pair is the week number [+-]0-53. */
	GList	      *byday;

	/* For BYHOUR modifier. A list of GINT_TO_POINTERs, 0-23. */
	GList	      *byhour;

	/* For BYMINUTE modifier. A list of GINT_TO_POINTERs, 0-59. */
	GList	      *byminute;

	/* For BYSECOND modifier. A list of GINT_TO_POINTERs, 0-60. */
	GList	      *bysecond;

	/* For BYSETPOS modifier. A list of GINT_TO_POINTERs, +ve or -ve. */
	GList	      *bysetpos;
} CalObjRecurrence;




/* This is what we use to represent a date & time. */
typedef struct _CalObjTime CalObjTime;
struct _CalObjTime {
	guint16 year;
	guint8 month;		/* 0 - 11 */
	guint8 day;		/* 1 - 31 */
	guint8 hour;		/* 0 - 23 */
	guint8 minute;		/* 0 - 59 */
	guint8 second;		/* 0 - 59 (maybe 60 for leap second) */
};



GArray* cal_obj_expand_recurrence	(CalObjTime	*event_start,
					 CalObjRecurrence *recur,
					 CalObjTime	*interval_start,
					 CalObjTime	*interval_end);


END_GNOME_DECLS

#endif
