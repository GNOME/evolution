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

#ifndef CAL_RECURL_H
#define CAL_RECUR_H

#include <libgnome/gnome-defs.h>
#include <glib.h>

BEGIN_GNOME_DECLS


/* FIXME: I've put modified versions of RecurType and Recurrence here, since
   the ones in calobj.h don't support all of iCalendar. Hopefully Seth will
   update those soon and these can be removed. */

enum RecurType {
	RECUR_YEARLY,
	RECUR_MONTHLY,
	RECUR_WEEKLY,
	RECUR_DAILY,
	RECUR_HOURLY,
	RECUR_MINUTELY,
	RECUR_SECONDLY,
};

typedef struct {
	enum RecurType type;

	int            interval;

	int            weekday;

	int	       month_pos;

	int	       month_day;


	/* For BYMONTH modifier. A list of GINT_TO_POINTERs, 0-11. */
	GList	      *bymonth;


	/* For BYHOUR modifier. A list of GINT_TO_POINTERs, 0-23. */
	GList	      *byhour;

	/* For BYMINUTE modifier. A list of GINT_TO_POINTERs, 0-59. */
	GList	      *byminute;

	/* For BYSECOND modifier. A list of GINT_TO_POINTERs, 0-60. */
	GList	      *bysecond;

} Recurrence;



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



/* Returns an unsorted array of time_t's resulting from expanding the
   recurrence within the given interval. Each iCalendar event can have any
   number of recurrence rules specifying occurrences of the event, as well as
   any number of recurrence rules specifying exceptions. */
GArray*
cal_obj_expand_recurrence		(CalObjTime	*event_start,
					 CalObjTime	*event_end,
					 Recurrence	*recur,
					 CalObjTime	*interval_start,
					 CalObjTime	*interval_end);

END_GNOME_DECLS

#endif
