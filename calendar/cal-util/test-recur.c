/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */

/* 
 * Author : 
 *  Damon Chaplin <damon@helixcode.com>
 *
 * Copyright 2000, Helix Code, Inc.
 *
 * This program is free software; you can redistribute it and/or 
 * modify it under the terms of the GNU General Public License as 
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307
 * USA
 */

/*
 * This tests the recurrence rule expansion functions.
 */

#include <config.h>
#include <string.h>
#include <time.h>
#include <glib.h>
#include "cal-recur.h"

static void set_time (CalObjTime *cotime, gint year, gint month, gint day,
		      gint hour, gint minute, gint second);
static void display_occs (GArray *occs);

#define LIST_ADD(list, num) \
	list = g_list_prepend (list, GINT_TO_POINTER (num));

int
main (int argc, char *argv[])
{
	CalObjTime event_start, interval_start, interval_end;
	CalObjRecurrence recur;
	GArray *occs;

	set_time (&event_start,    2000, 0, 1, 0, 0, 0);

	/* We set the interval to a wide range so we just test the event. */
	set_time (&interval_start, 1900, 0, 1, 0, 0, 0);
	set_time (&interval_end,   2100, 0, 1, 0, 0, 0);

	memset (&recur, 0, sizeof (recur));
	recur.type = CAL_RECUR_YEARLY;
	recur.interval = 1;

	LIST_ADD (recur.byweekno, 3)
	LIST_ADD (recur.byweekno, 9)
	LIST_ADD (recur.byweekno, 24)

	LIST_ADD (recur.byday, 0)
	LIST_ADD (recur.byday, 3)
	LIST_ADD (recur.byday, 0)
	LIST_ADD (recur.byday, 5)

	occs = cal_obj_expand_recurrence (&event_start, &recur,
					  &interval_start, &interval_end);

	display_occs (occs);
	g_array_free (occs, TRUE);

	return 0;
}


static void
set_time (CalObjTime *cotime, gint year, gint month, gint day,
	  gint hour, gint minute, gint second)
{
	cotime->year	= year;
	cotime->month	= month;
	cotime->day	= day;
	cotime->hour	= hour;
	cotime->minute	= minute;
	cotime->second	= second;
}


static void
display_occs (GArray *occs)
{
	CalObjTime *occ;
	gint len, i;
	struct tm t;

	len = occs->len;
	for (i = 0; i < len; i++) {
		occ = &g_array_index (occs, CalObjTime, i);

		t.tm_sec	= occ->second;
		t.tm_min	= occ->minute;
		t.tm_hour	= occ->hour;
		t.tm_mday	= occ->day;
		t.tm_mon	= occ->month;
		t.tm_year	= occ->year - 1900;
		t.tm_isdst	= -1;

		mktime (&t);

		g_print ("%s", asctime (&t));
	}
}
