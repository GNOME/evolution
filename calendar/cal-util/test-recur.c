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
static GList* build_list (gint first, ...);
static gchar* time_to_string (CalObjTime *cotime);
static void do_test (gchar		  *description,
		     CalObjTime	  *event_start,
		     CalObjRecurrence *recur,
		     CalObjTime	  *interval_start,
		     CalObjTime	  *interval_end);

#define LIST_END	999

static void
test_yearly ()
{
	CalObjTime event_start, interval_start, interval_end;
	CalObjRecurrence recur;

	set_time (&event_start,    2000, 0, 1, 0, 0, 0);

	/* We set the interval to a wide range so we just test the event. */
	set_time (&interval_start, 2000, 0, 1, 0, 0, 0);
	set_time (&interval_end,   2010, 0, 1, 0, 0, 0);

	memset (&recur, 0, sizeof (recur));
	recur.type = CAL_RECUR_YEARLY;
	recur.interval = 3;
	recur.byweekno = build_list (3, 9, 24, LIST_END);
	recur.byday = build_list (3, 0, 5, 0, LIST_END);
	do_test ("YEARLY every 3 years in weeks 3, 9, 24 on Thu/Sat",
		 &event_start, &recur, &interval_start, &interval_end);


	set_time (&interval_end,   2002, 0, 1, 0, 0, 0);
	memset (&recur, 0, sizeof (recur));
	recur.type = CAL_RECUR_YEARLY;
	recur.interval = 1;
	recur.bymonth = build_list (0, 6, LIST_END);
	recur.byday = build_list (0, 0, 6, 0, LIST_END);
	do_test ("YEARLY every year in Jan/Jul on Mon/Sun",
		 &event_start, &recur, &interval_start, &interval_end);


	memset (&recur, 0, sizeof (recur));
	recur.type = CAL_RECUR_YEARLY;
	recur.interval = 1;
	recur.bymonthday = build_list (3, 7, LIST_END);
	do_test ("YEARLY every year on 3rd & 7th of the month",
		 &event_start, &recur, &interval_start, &interval_end);



	memset (&recur, 0, sizeof (recur));
	recur.type = CAL_RECUR_YEARLY;
	recur.interval = 1;
	recur.byyearday = build_list (15, 126, 360, LIST_END);
	do_test ("YEARLY every year on 15th, 126th & 360th day of the year",
		 &event_start, &recur, &interval_start, &interval_end);

}


static void
test_monthly ()
{
	CalObjTime event_start, interval_start, interval_end;
	CalObjRecurrence recur;

	set_time (&event_start,    2000, 0, 1, 0, 0, 0);

	/* We set the interval to a wide range so we just test the event. */
	set_time (&interval_start, 2000, 0, 1, 0, 0, 0);
	set_time (&interval_end,   2002, 0, 1, 0, 0, 0);

	memset (&recur, 0, sizeof (recur));
	recur.type = CAL_RECUR_MONTHLY;
	recur.interval = 1;
	do_test ("MONTHLY every month",
		 &event_start, &recur, &interval_start, &interval_end);

}

static void
test_weekly ()
{
	CalObjTime event_start, interval_start, interval_end;
	CalObjRecurrence recur;

	set_time (&event_start,    2000, 0, 1, 0, 0, 0);

	/* We set the interval to a wide range so we just test the event. */
	set_time (&interval_start, 2000, 0, 1, 0, 0, 0);
	set_time (&interval_end,   2002, 0, 1, 0, 0, 0);

	memset (&recur, 0, sizeof (recur));
	recur.type = CAL_RECUR_WEEKLY;
	recur.interval = 1;
	do_test ("WEEKLY every week",
		 &event_start, &recur, &interval_start, &interval_end);

}

static void
test_daily ()
{
	CalObjTime event_start, interval_start, interval_end;
	CalObjRecurrence recur;

	set_time (&event_start,    2000, 0, 1, 0, 0, 0);

	/* We set the interval to a wide range so we just test the event. */
	set_time (&interval_start, 2000, 0, 1, 0, 0, 0);
	set_time (&interval_end,   2000, 6, 1, 0, 0, 0);

	memset (&recur, 0, sizeof (recur));
	recur.type = CAL_RECUR_DAILY;
	recur.interval = 1;
	do_test ("DAILY every day",
		 &event_start, &recur, &interval_start, &interval_end);

}

static void
test_hourly ()
{
	CalObjTime event_start, interval_start, interval_end;
	CalObjRecurrence recur;

	set_time (&event_start,    2000, 0, 1, 2, 15, 0);

	/* We set the interval to a wide range so we just test the event. */
	set_time (&interval_start, 2000, 0, 1, 0, 0, 0);
	set_time (&interval_end,   2002, 0, 1, 0, 0, 0);

	memset (&recur, 0, sizeof (recur));
	recur.type = CAL_RECUR_HOURLY;
	recur.interval = 3;
	recur.bymonth = build_list (3, 11, LIST_END);
	recur.byday = build_list (2, 0, 4, 0, LIST_END);
	do_test ("HOURLY every 3 hours in Apr/Dec on Wed & Fri",
		 &event_start, &recur, &interval_start, &interval_end);
}

static void
test_minutely ()
{
	CalObjTime event_start, interval_start, interval_end;
	CalObjRecurrence recur;

	set_time (&event_start,    2000, 0, 1, 0, 0, 0);

	/* We set the interval to a wide range so we just test the event. */
	set_time (&interval_start, 2000, 0, 1, 0, 0, 0);
	set_time (&interval_end,   2000, 0, 2, 0, 0, 0);

	memset (&recur, 0, sizeof (recur));
	recur.type = CAL_RECUR_MINUTELY;
	recur.interval = 45;
	do_test ("MINUTELY every 45 minutes",
		 &event_start, &recur, &interval_start, &interval_end);
}

static void
test_secondly ()
{
	CalObjTime event_start, interval_start, interval_end;
	CalObjRecurrence recur;

	set_time (&event_start,    2000, 0, 1, 0, 0, 0);

	/* We set the interval to a wide range so we just test the event. */
	set_time (&interval_start, 2000, 0, 1, 0, 0, 0);
	set_time (&interval_end,   2000, 0, 2, 0, 0, 0);

	memset (&recur, 0, sizeof (recur));
	recur.type = CAL_RECUR_SECONDLY;
	recur.interval = 15;
	recur.byhour = build_list (2, 4, 6, LIST_END);
	recur.byminute = build_list (0, 30, LIST_END);
	do_test ("SECONDLY every 15 seconds at 2:00,2:30,4:00,4:30,6:00,6:30",
		 &event_start, &recur, &interval_start, &interval_end);
}

int
main (int argc, char *argv[])
{

	test_yearly ();
	test_monthly ();
	test_weekly ();
	test_daily ();
	test_hourly ();
	test_minutely ();
	test_secondly ();

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


static GList*
build_list (gint first, ...)
{
  va_list args;
  GList *list;
  gint num;

  va_start (args, first);

  list = g_list_prepend (NULL, GINT_TO_POINTER (first));

  num = va_arg (args, gint);
  while (num != LIST_END) {
	  list = g_list_prepend (list, GINT_TO_POINTER (num));
	  num = va_arg (args, gint);
  }

  list = g_list_reverse (list);

  va_end (args);

  return list;
}


static void
do_test (gchar		  *description,
	 CalObjTime	  *event_start,
	 CalObjRecurrence *recur,
	 CalObjTime	  *interval_start,
	 CalObjTime	  *interval_end)
{
	GArray *occs;

	g_print ("========================================================\n");
	g_print ("%s\n", description);
	g_print ("(From %s", time_to_string (interval_start));
	g_print (" To %s)\n\n", time_to_string (interval_end));

	occs = cal_obj_expand_recurrence (event_start, recur,
					  interval_start, interval_end);
	display_occs (occs);
	g_array_free (occs, TRUE);
}


static gchar*
time_to_string (CalObjTime *cotime)
{
	static gchar buffer[64];
	gint month;
	gchar *months[] = { "Jan", "Feb", "Mar", "Apr", "May", "Jun", 
			    "Jul", "Aug", "Sep", "Oct", "Nov", "Dec", "XXX" };

	month = cotime->month;
	if (month < 0 || month > 12)
		month = 12;

	sprintf (buffer, "%s %2i %02i:%02i:%02i %4i",
		 months[month], cotime->day,
		 cotime->hour, cotime->minute, cotime->second,
		 cotime->year);

	return buffer;
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
