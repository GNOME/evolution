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

#include <stdlib.h>
#include <string.h>
#include <cal-util/cal-recur.h>
#include <cal-util/timeutil.h>


/*
 * Introduction to The Recurrence Generation Functions:
 *
 * Note: This is pretty complicated. See the iCalendar spec (RFC 2445) for
 *       the specification of the recurrence rules and lots of examples
 *	 (sections 4.3.10 & 4.8.5). We also want to support the older
 *	 vCalendar spec, though this should be easy since it is basically a
 *	 subset of iCalendar.
 *
 * o An iCalendar event can have any number of recurrence rules specifying
 *   occurrences of the event, as well as dates & times of specific
 *   occurrences. It can also have any number of recurrence rules and
 *   specific dates & times specifying exceptions to the occurrences.
 *   So we first merge all the occurrences generated, eliminating any
 *   duplicates, then we generate all the exceptions and remove these to
 *   form the final set of occurrences.
 *
 * o There are 7 frequencies of occurrences: YEARLY, MONTHLY, WEEKLY, DAILY,
 *   HOURLY, MINUTELY & SECONDLY. The 'interval' property specifies the
 *   multiples of the frequency between each 'set' of occurrences. So for
 *   a YEARLY frequency with an interval of 3, we generate a set of occurrences
 *   for every 3rd year. We use complete years here -  any generated
 *   occurrences that occur before the event's start (or after its end)
 *   are just discarded.
 *
 * o There are 8 frequency modifiers: BYMONTH, BYWEEKNO, BYYEARDAY, BYMONTHDAY,
 *   BYDAY, BYHOUR & BYSECOND. These can either add extra occurrences or
 *   filter out occurrences. For example 'FREQ=YEARLY;BYMONTH=1,2' produces
 *   2 occurrences for each year rather than the default 1. And
 *   'FREQ=DAILY; BYMONTH=1' filters out all occurrences except those in Jan.
 *   If the modifier works on periods which are less than the recurrence
 *   frequency, then extra occurrences are added, else occurrences are
 *   filtered. So we have 2 functions for each modifier - one to expand events
 *   and the other to filter. We use a table of functions for each frequency
 *   which points to the appropriate function to use for each modifier.
 *
 * o Any number of frequency modifiers can be used in a recurrence rule
 *   (though BYWEEKNO can only be used in a YEARLY rule). They are applied in
 *   the order given above.
 *
 * o After the set of occurrences for the frequency interval are generated,
 *   the BYSETPOS property is used to select which of the occurrences are
 *   finally output. If BYSETPOS is not specified then all the occurrences are
 *   output.
 */


#define CAL_OBJ_NUM_FILTERS	8

typedef gboolean (*CalObjFindStartFn) (CalObjTime *event_start,
				       CalObjTime *event_end,
				       Recurrence *recur,
				       CalObjTime *interval_start,
				       CalObjTime *interval_end,
				       CalObjTime *cotime);
typedef gboolean (*CalObjFindNextFn)  (CalObjTime *cotime,
				       CalObjTime *event_end,
				       Recurrence *recur,
				       CalObjTime *interval_end);
typedef GArray* (*CalObjFilterFn)    (Recurrence *recur,
				      GArray     *occs);

typedef struct _CalObjRecurVTable CalObjRecurVTable;
struct _CalObjRecurVTable {
	CalObjFindStartFn find_start_position;
	CalObjFindNextFn find_next_position;
	CalObjFilterFn filters[CAL_OBJ_NUM_FILTERS];
};


static CalObjRecurVTable* cal_obj_get_vtable (Recurrence *recur);
static void cal_obj_sort_occurrences (GArray *occs);
static gint cal_obj_time_compare_func (const void *arg1,
				       const void *arg2);
static void cal_obj_remove_duplicates (GArray *occs);
static GArray* cal_obj_bysetpos_filter (GArray *occs);


static gboolean cal_obj_yearly_find_start_position (CalObjTime *event_start,
						    CalObjTime *event_end,
						    Recurrence *recur,
						    CalObjTime *interval_start,
						    CalObjTime *interval_end,
						    CalObjTime *cotime);
static gboolean cal_obj_yearly_find_next_position  (CalObjTime *cotime,
						    CalObjTime *event_end,
						    Recurrence *recur,
						    CalObjTime *interval_end);

static gboolean cal_obj_monthly_find_start_position (CalObjTime *event_start,
						     CalObjTime *event_end,
						     Recurrence *recur,
						     CalObjTime *interval_start,
						     CalObjTime *interval_end,
						     CalObjTime *cotime);
static gboolean cal_obj_monthly_find_next_position  (CalObjTime *cotime,
						     CalObjTime *event_end,
						     Recurrence *recur,
						     CalObjTime *interval_end);

static gboolean cal_obj_weekly_find_start_position (CalObjTime *event_start,
						    CalObjTime *event_end,
						    Recurrence *recur,
						    CalObjTime *interval_start,
						    CalObjTime *interval_end,
						    CalObjTime *cotime);
static gboolean cal_obj_weekly_find_next_position  (CalObjTime *cotime,
						    CalObjTime *event_end,
						    Recurrence *recur,
						    CalObjTime *interval_end);

static gboolean cal_obj_daily_find_start_position  (CalObjTime *event_start,
						    CalObjTime *event_end,
						    Recurrence *recur,
						    CalObjTime *interval_start,
						    CalObjTime *interval_end,
						    CalObjTime *cotime);
static gboolean cal_obj_daily_find_next_position   (CalObjTime *cotime,
						    CalObjTime *event_end,
						    Recurrence *recur,
						    CalObjTime *interval_end);

static gboolean cal_obj_hourly_find_start_position (CalObjTime *event_start,
						    CalObjTime *event_end,
						    Recurrence *recur,
						    CalObjTime *interval_start,
						    CalObjTime *interval_end,
						    CalObjTime *cotime);
static gboolean cal_obj_hourly_find_next_position  (CalObjTime *cotime,
						    CalObjTime *event_end,
						    Recurrence *recur,
						    CalObjTime *interval_end);

static gboolean cal_obj_minutely_find_start_position (CalObjTime *event_start,
						      CalObjTime *event_end,
						      Recurrence *recur,
						      CalObjTime *interval_start,
						      CalObjTime *interval_end,
						      CalObjTime *cotime);
static gboolean cal_obj_minutely_find_next_position  (CalObjTime *cotime,
						      CalObjTime *event_end,
						      Recurrence *recur,
						      CalObjTime *interval_end);

static gboolean cal_obj_secondly_find_start_position (CalObjTime *event_start,
						      CalObjTime *event_end,
						      Recurrence *recur,
						      CalObjTime *interval_start,
						      CalObjTime *interval_end,
						      CalObjTime *cotime);
static gboolean cal_obj_secondly_find_next_position  (CalObjTime *cotime,
						      CalObjTime *event_end,
						      Recurrence *recur,
						      CalObjTime *interval_end);

static GArray* cal_obj_bymonth_expand		(Recurrence *recur,
						 GArray     *occs);
static GArray* cal_obj_bymonth_filter		(Recurrence *recur,
						 GArray     *occs);
static GArray* cal_obj_byweekno_expand		(Recurrence *recur,
						 GArray     *occs);
#if 0
/* This isn't used at present. */
static GArray* cal_obj_byweekno_filter		(Recurrence *recur,
						 GArray     *occs);
#endif
static GArray* cal_obj_byyearday_expand		(Recurrence *recur,
						 GArray     *occs);
static GArray* cal_obj_byyearday_filter		(Recurrence *recur,
						 GArray     *occs);
static GArray* cal_obj_bymonthday_expand	(Recurrence *recur,
						 GArray     *occs);
static GArray* cal_obj_bymonthday_filter	(Recurrence *recur,
						 GArray     *occs);
static GArray* cal_obj_byday_expand		(Recurrence *recur,
						 GArray     *occs);
static GArray* cal_obj_byday_filter		(Recurrence *recur,
						 GArray     *occs);
static GArray* cal_obj_byhour_expand		(Recurrence *recur,
						 GArray     *occs);
static GArray* cal_obj_byhour_filter		(Recurrence *recur,
						 GArray     *occs);
static GArray* cal_obj_byminute_expand		(Recurrence *recur,
						 GArray     *occs);
static GArray* cal_obj_byminute_filter		(Recurrence *recur,
						 GArray     *occs);
static GArray* cal_obj_bysecond_expand		(Recurrence *recur,
						 GArray     *occs);
static GArray* cal_obj_bysecond_filter		(Recurrence *recur,
						 GArray     *occs);

static void cal_obj_time_add_days (CalObjTime *cotime,
				   gint	       days);


CalObjRecurVTable cal_obj_yearly_vtable = {
	cal_obj_yearly_find_start_position,
	cal_obj_yearly_find_next_position,
	{
		cal_obj_bymonth_expand,
		cal_obj_byweekno_expand,
		cal_obj_byyearday_expand,
		cal_obj_bymonthday_expand,
		cal_obj_byday_expand,
		cal_obj_byhour_expand,
		cal_obj_byminute_expand,
		cal_obj_bysecond_expand
	},
};

CalObjRecurVTable cal_obj_monthly_vtable = {
	cal_obj_monthly_find_start_position,
	cal_obj_monthly_find_next_position,
	{
		cal_obj_bymonth_filter,
		NULL,
		cal_obj_byyearday_filter,
		cal_obj_bymonthday_expand,
		cal_obj_byday_expand,
		cal_obj_byhour_expand,
		cal_obj_byminute_expand,
		cal_obj_bysecond_expand
	},
};

CalObjRecurVTable cal_obj_weekly_vtable = {
	cal_obj_weekly_find_start_position,
	cal_obj_weekly_find_next_position,
	{
		cal_obj_bymonth_filter,
		NULL,
		cal_obj_byyearday_filter,
		cal_obj_bymonthday_filter,
		cal_obj_byday_expand,
		cal_obj_byhour_expand,
		cal_obj_byminute_expand,
		cal_obj_bysecond_expand
	},
};

CalObjRecurVTable cal_obj_daily_vtable = {
	cal_obj_daily_find_start_position,
	cal_obj_daily_find_next_position,
	{
		cal_obj_bymonth_filter,
		NULL,
		cal_obj_byyearday_filter,
		cal_obj_bymonthday_filter,
		cal_obj_byday_filter,
		cal_obj_byhour_expand,
		cal_obj_byminute_expand,
		cal_obj_bysecond_expand
	},
};

CalObjRecurVTable cal_obj_hourly_vtable = {
	cal_obj_hourly_find_start_position,
	cal_obj_hourly_find_next_position,
	{
		cal_obj_bymonth_filter,
		NULL,
		cal_obj_byyearday_filter,
		cal_obj_bymonthday_filter,
		cal_obj_byday_filter,
		cal_obj_byhour_filter,
		cal_obj_byminute_expand,
		cal_obj_bysecond_expand
	},
};

CalObjRecurVTable cal_obj_minutely_vtable = {
	cal_obj_minutely_find_start_position,
	cal_obj_minutely_find_next_position,
	{
		cal_obj_bymonth_filter,
		NULL,
		cal_obj_byyearday_filter,
		cal_obj_bymonthday_filter,
		cal_obj_byday_filter,
		cal_obj_byhour_filter,
		cal_obj_byminute_filter,
		cal_obj_bysecond_expand
	},
};

CalObjRecurVTable cal_obj_secondly_vtable = {
	cal_obj_secondly_find_start_position,
	cal_obj_secondly_find_next_position,
	{
		cal_obj_bymonth_filter,
		NULL,
		cal_obj_byyearday_filter,
		cal_obj_bymonthday_filter,
		cal_obj_byday_filter,
		cal_obj_byhour_filter,
		cal_obj_byminute_filter,
		cal_obj_bysecond_filter
	},
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
					 CalObjTime	*interval_end)
{
	CalObjRecurVTable *vtable;
	CalObjTime occ;
	GArray *all_occs, *occs;
	gint filter;

	vtable = cal_obj_get_vtable (recur);

	/* This is the resulting array of CalObjTime elements. */
	all_occs = g_array_new (FALSE, FALSE, sizeof (CalObjTime));

	/* Get the first period based on the frequency and the interval that
	   intersects the interval between start and end. */
	if ((*vtable->find_start_position) (event_start, event_end, recur,
					    interval_start, interval_end,
					    &occ))
		return all_occs;

	/* Loop until the event ends or we go past the end of the required
	   interval. */
	for (;;) {

		/* We start with just the one time in the set. */
		occs = g_array_new (FALSE, FALSE, sizeof (CalObjTime));
		g_array_append_val (occs, occ);

		/* Generate the set of occurrences for this period. */
		for (filter = 0; filter < CAL_OBJ_NUM_FILTERS; filter++) {
			if (vtable->filters[filter])
				occs = (*vtable->filters[filter]) (recur,
								   occs);
		}

		/* Sort the occurrences and remove duplicates. */
		cal_obj_sort_occurrences (occs);
		cal_obj_remove_duplicates (occs);

		/* Apply the BYSETPOS property. */
		occs = cal_obj_bysetpos_filter (occs);

		/* Add the occurrences onto the main array. */
		g_array_append_vals (all_occs, occs->data, occs->len);

		/* Skip to the next period, or exit the loop if finished. */
		if ((*vtable->find_next_position) (&occ, event_end, recur,
						   interval_end))
			break;
	}

	return all_occs;
}


/* Returns the function table corresponding to the recurrence frequency. */
static CalObjRecurVTable*
cal_obj_get_vtable (Recurrence *recur)
{
	switch (recur->type) {
	case RECUR_YEARLY:
		return &cal_obj_yearly_vtable;
	case RECUR_MONTHLY:
		return &cal_obj_monthly_vtable;
	case RECUR_WEEKLY:
		return &cal_obj_weekly_vtable;
	case RECUR_DAILY:
		return &cal_obj_daily_vtable;
	case RECUR_HOURLY:
		return &cal_obj_hourly_vtable;
	case RECUR_MINUTELY:
		return &cal_obj_minutely_vtable;
	case RECUR_SECONDLY:
		return &cal_obj_secondly_vtable;
	}
	return NULL;
}


static void
cal_obj_sort_occurrences (GArray *occs)
{
	qsort (occs->data, occs->len, sizeof (CalObjTime),
	       cal_obj_time_compare_func);
}


static gint
cal_obj_time_compare_func (const void *arg1,
			   const void *arg2)
{
	CalObjTime *cotime1, *cotime2;

	cotime1 = (CalObjTime*) arg1;
	cotime2 = (CalObjTime*) arg2;

	if (cotime1->year < cotime2->year)
		return -1;
	if (cotime1->year > cotime2->year)
		return 1;

	if (cotime1->month < cotime2->month)
		return -1;
	if (cotime1->month > cotime2->month)
		return 1;

	if (cotime1->day < cotime2->day)
		return -1;
	if (cotime1->day > cotime2->day)
		return 1;

	if (cotime1->hour < cotime2->hour)
		return -1;
	if (cotime1->hour > cotime2->hour)
		return 1;

	if (cotime1->minute < cotime2->minute)
		return -1;
	if (cotime1->minute > cotime2->minute)
		return 1;

	if (cotime1->second < cotime2->second)
		return -1;
	if (cotime1->second > cotime2->second)
		return 1;

	return 0;
}


static void
cal_obj_remove_duplicates (GArray *occs)
{
	CalObjTime *occ, *prev_occ = NULL;
	gint len, i, j = 0;

	len = occs->len;
	for (i = 0; i < len; i++) {
		occ = &g_array_index (occs, CalObjTime, i);

		if (!prev_occ
		    || cal_obj_time_compare_func (occ, prev_occ) != 0) {
			if (i != j)
				g_array_index (occs, CalObjTime, j)
					= g_array_index (occs, CalObjTime, i);
			j++;
		}

		prev_occ = occ;
	}

	g_array_set_size (occs, j);
}


static GArray*
cal_obj_bysetpos_filter (GArray *occs)
{

	return occs;
}




static gboolean
cal_obj_yearly_find_start_position (CalObjTime *event_start,
				    CalObjTime *event_end,
				    Recurrence *recur,
				    CalObjTime *interval_start,
				    CalObjTime *interval_end,
				    CalObjTime *cotime)
{


	return FALSE;
}


static gboolean
cal_obj_yearly_find_next_position (CalObjTime *cotime,
				   CalObjTime *event_end,
				   Recurrence *recur,
				   CalObjTime *interval_end)
{
	/* NOTE: The day may now be invalid, e.g. 29th Feb.
	   Make sure we remove these eventually. */
	cotime->year += recur->interval;

	if (cotime->year > event_end->year
	    || cotime->year > interval_end->year)
		return TRUE;

	return FALSE;
}



static gboolean
cal_obj_monthly_find_start_position (CalObjTime *event_start,
				     CalObjTime *event_end,
				     Recurrence *recur,
				     CalObjTime *interval_start,
				     CalObjTime *interval_end,
				     CalObjTime *cotime)
{


	return FALSE;
}


static gboolean
cal_obj_monthly_find_next_position (CalObjTime *cotime,
				    CalObjTime *event_end,
				    Recurrence *recur,
				    CalObjTime *interval_end)
{
	cotime->month += recur->interval;
	cotime->year += cotime->month / 12;
	cotime->month %= 12;

	if (cotime->year > event_end->year
	    || cotime->year > interval_end->year
	    || (cotime->year == event_end->year
		&& cotime->month > event_end->month)
	    || (cotime->year == interval_end->year
		&& cotime->month > interval_end->month))
		return TRUE;

	return FALSE;
}



static gboolean
cal_obj_weekly_find_start_position (CalObjTime *event_start,
				    CalObjTime *event_end,
				    Recurrence *recur,
				    CalObjTime *interval_start,
				    CalObjTime *interval_end,
				    CalObjTime *cotime)
{


	return FALSE;
}


static gboolean
cal_obj_weekly_find_next_position (CalObjTime *cotime,
				   CalObjTime *event_end,
				   Recurrence *recur,
				   CalObjTime *interval_end)
{
	cal_obj_time_add_days (cotime, recur->interval);



	return FALSE;
}


static gboolean
cal_obj_daily_find_start_position  (CalObjTime *event_start,
				    CalObjTime *event_end,
				    Recurrence *recur,
				    CalObjTime *interval_start,
				    CalObjTime *interval_end,
				    CalObjTime *cotime)
{


	return FALSE;
}


static gboolean
cal_obj_daily_find_next_position  (CalObjTime *cotime,
				   CalObjTime *event_end,
				   Recurrence *recur,
				   CalObjTime *interval_end)
{

	cal_obj_time_add_days (cotime, recur->interval);


	return FALSE;
}


static gboolean
cal_obj_hourly_find_start_position (CalObjTime *event_start,
				    CalObjTime *event_end,
				    Recurrence *recur,
				    CalObjTime *interval_start,
				    CalObjTime *interval_end,
				    CalObjTime *cotime)
{


	return FALSE;
}


static gboolean
cal_obj_hourly_find_next_position (CalObjTime *cotime,
				   CalObjTime *event_end,
				   Recurrence *recur,
				   CalObjTime *interval_end)
{


	return FALSE;
}


static gboolean
cal_obj_minutely_find_start_position (CalObjTime *event_start,
				      CalObjTime *event_end,
				      Recurrence *recur,
				      CalObjTime *interval_start,
				      CalObjTime *interval_end,
				      CalObjTime *cotime)
{


	return FALSE;
}


static gboolean
cal_obj_minutely_find_next_position (CalObjTime *cotime,
				     CalObjTime *event_end,
				     Recurrence *recur,
				     CalObjTime *interval_end)
{


	return FALSE;
}


static gboolean
cal_obj_secondly_find_start_position (CalObjTime *event_start,
				      CalObjTime *event_end,
				      Recurrence *recur,
				      CalObjTime *interval_start,
				      CalObjTime *interval_end,
				      CalObjTime *cotime)
{


	return FALSE;
}


static gboolean
cal_obj_secondly_find_next_position (CalObjTime *cotime,
				     CalObjTime *event_end,
				     Recurrence *recur,
				     CalObjTime *interval_end)
{


	return FALSE;
}





/* If the BYMONTH rule is specified it expands each occurrence in occs, by
   using each of the months in the bymonth list. */
static GArray*
cal_obj_bymonth_expand		(Recurrence *recur,
				 GArray     *occs)
{
	GArray *new_occs;
	CalObjTime *occ;
	GList *elem;
	gint len, i;

	/* If BYMONTH has not been specified, or the array is empty, just
	   return the array. */
	if (!recur->bymonth || occs->len == 0)
		return occs;

	new_occs = g_array_new (FALSE, FALSE, sizeof (CalObjTime));

	len = occs->len;
	for (i = 0; i < len; i++) {
		occ = &g_array_index (occs, CalObjTime, i);

		elem = recur->bymonth;
		while (elem) {
			/* NOTE: The day may now be invalid, e.g. 31st Feb.
			   Make sure we remove these eventually. */
			occ->month = GPOINTER_TO_INT (elem->data);
			g_array_append_vals (new_occs, occ, 1);
			elem = elem->next;
		}
	}

	g_array_free (occs, TRUE);

	return new_occs;
}


/* If the BYMONTH rule is specified it filters out all occurrences in occs
   which do not match one of the months in the bymonth list. */
static GArray*
cal_obj_bymonth_filter		(Recurrence *recur,
				 GArray     *occs)
{
	GArray *new_occs;
	CalObjTime *occ;
	guint8 months[12];
	gint mon, len, i;
	GList *elem;

	/* If BYMONTH has not been specified, or the array is empty, just
	   return the array. */
	elem = recur->bymonth;
	if (!elem || occs->len == 0)
		return occs;

	/* Create an array of months from bymonths for fast lookup. */
	memset (&months, 0, sizeof (months));
	while (elem) {
		mon = GPOINTER_TO_INT (elem->data);
		months[mon] = 1;
		elem = elem->next;
	}

	new_occs = g_array_new (FALSE, FALSE, sizeof (CalObjTime));

	len = occs->len;
	for (i = 0; i < len; i++) {
		occ = &g_array_index (occs, CalObjTime, i);
		if (months[occ->month])
			g_array_append_vals (new_occs, occ, 1);
	}

	g_array_free (occs, TRUE);

	return new_occs;
}



static GArray*
cal_obj_byweekno_expand		(Recurrence *recur,
				 GArray     *occs)
{

	return occs;
}


#if 0
/* This isn't used at present. */
static GArray*
cal_obj_byweekno_filter		(Recurrence *recur,
				 GArray     *occs)
{

	return occs;
}
#endif


static GArray*
cal_obj_byyearday_expand	(Recurrence *recur,
				 GArray     *occs)
{

	return occs;
}


static GArray*
cal_obj_byyearday_filter	(Recurrence *recur,
				 GArray     *occs)
{

	return occs;
}



static GArray*
cal_obj_bymonthday_expand	(Recurrence *recur,
				 GArray     *occs)
{

	return occs;
}


static GArray*
cal_obj_bymonthday_filter	(Recurrence *recur,
				 GArray     *occs)
{

	return occs;
}



static GArray*
cal_obj_byday_expand		(Recurrence *recur,
				 GArray     *occs)
{

	return occs;
}


static GArray*
cal_obj_byday_filter		(Recurrence *recur,
				 GArray     *occs)
{

	return occs;
}



/* If the BYHOUR rule is specified it expands each occurrence in occs, by
   using each of the hours in the byhour list. */
static GArray*
cal_obj_byhour_expand		(Recurrence *recur,
				 GArray     *occs)
{
	GArray *new_occs;
	CalObjTime *occ;
	GList *elem;
	gint len, i;

	/* If BYHOUR has not been specified, or the array is empty, just
	   return the array. */
	if (!recur->byhour || occs->len == 0)
		return occs;

	new_occs = g_array_new (FALSE, FALSE, sizeof (CalObjTime));

	len = occs->len;
	for (i = 0; i < len; i++) {
		occ = &g_array_index (occs, CalObjTime, i);

		elem = recur->byhour;
		while (elem) {
			occ->hour = GPOINTER_TO_INT (elem->data);
			g_array_append_vals (new_occs, occ, 1);
			elem = elem->next;
		}
	}

	g_array_free (occs, TRUE);

	return new_occs;
}


/* If the BYHOUR rule is specified it filters out all occurrences in occs
   which do not match one of the hours in the byhour list. */
static GArray*
cal_obj_byhour_filter		(Recurrence *recur,
				 GArray     *occs)
{
	GArray *new_occs;
	CalObjTime *occ;
	guint8 hours[24];
	gint hour, len, i;
	GList *elem;

	/* If BYHOURUTE has not been specified, or the array is empty, just
	   return the array. */
	elem = recur->byhour;
	if (!elem || occs->len == 0)
		return occs;

	/* Create an array of hours from byhour for fast lookup. */
	memset (&hours, 0, sizeof (hours));
	while (elem) {
		hour = GPOINTER_TO_INT (elem->data);
		hours[hour] = 1;
		elem = elem->next;
	}

	new_occs = g_array_new (FALSE, FALSE, sizeof (CalObjTime));

	len = occs->len;
	for (i = 0; i < len; i++) {
		occ = &g_array_index (occs, CalObjTime, i);
		if (hours[occ->hour])
			g_array_append_vals (new_occs, occ, 1);
	}

	g_array_free (occs, TRUE);

	return new_occs;
}



/* If the BYMINUTE rule is specified it expands each occurrence in occs, by
   using each of the minutes in the byminute list. */
static GArray*
cal_obj_byminute_expand		(Recurrence *recur,
				 GArray     *occs)
{
	GArray *new_occs;
	CalObjTime *occ;
	GList *elem;
	gint len, i;

	/* If BYMINUTE has not been specified, or the array is empty, just
	   return the array. */
	if (!recur->byminute || occs->len == 0)
		return occs;

	new_occs = g_array_new (FALSE, FALSE, sizeof (CalObjTime));

	len = occs->len;
	for (i = 0; i < len; i++) {
		occ = &g_array_index (occs, CalObjTime, i);

		elem = recur->byminute;
		while (elem) {
			occ->minute = GPOINTER_TO_INT (elem->data);
			g_array_append_vals (new_occs, occ, 1);
			elem = elem->next;
		}
	}

	g_array_free (occs, TRUE);

	return new_occs;
}


/* If the BYMINUTE rule is specified it filters out all occurrences in occs
   which do not match one of the minutes in the byminute list. */
static GArray*
cal_obj_byminute_filter		(Recurrence *recur,
				 GArray     *occs)
{
	GArray *new_occs;
	CalObjTime *occ;
	guint8 minutes[60];
	gint min, len, i;
	GList *elem;

	/* If BYMINUTE has not been specified, or the array is empty, just
	   return the array. */
	elem = recur->byminute;
	if (!elem || occs->len == 0)
		return occs;

	/* Create an array of minutes from byminutes for fast lookup. */
	memset (&minutes, 0, sizeof (minutes));
	while (elem) {
		min = GPOINTER_TO_INT (elem->data);
		minutes[min] = 1;
		elem = elem->next;
	}

	new_occs = g_array_new (FALSE, FALSE, sizeof (CalObjTime));

	len = occs->len;
	for (i = 0; i < len; i++) {
		occ = &g_array_index (occs, CalObjTime, i);
		if (minutes[occ->minute])
			g_array_append_vals (new_occs, occ, 1);
	}

	g_array_free (occs, TRUE);

	return new_occs;
}



/* If the BYSECOND rule is specified it expands each occurrence in occs, by
   using each of the seconds in the bysecond list. */
static GArray*
cal_obj_bysecond_expand		(Recurrence *recur,
				 GArray     *occs)
{
	GArray *new_occs;
	CalObjTime *occ;
	GList *elem;
	gint len, i;

	/* If BYSECOND has not been specified, or the array is empty, just
	   return the array. */
	if (!recur->bysecond || occs->len == 0)
		return occs;

	new_occs = g_array_new (FALSE, FALSE, sizeof (CalObjTime));

	len = occs->len;
	for (i = 0; i < len; i++) {
		occ = &g_array_index (occs, CalObjTime, i);

		elem = recur->bysecond;
		while (elem) {
			occ->second = GPOINTER_TO_INT (elem->data);
			g_array_append_vals (new_occs, occ, 1);
			elem = elem->next;
		}
	}

	g_array_free (occs, TRUE);

	return new_occs;
}


/* If the BYSECOND rule is specified it filters out all occurrences in occs
   which do not match one of the seconds in the bysecond list. */
static GArray*
cal_obj_bysecond_filter		(Recurrence *recur,
				 GArray     *occs)
{
	GArray *new_occs;
	CalObjTime *occ;
	guint8 seconds[61];
	gint sec, len, i;
	GList *elem;

	/* If BYSECOND has not been specified, or the array is empty, just
	   return the array. */
	elem = recur->bysecond;
	if (!elem || occs->len == 0)
		return occs;

	/* Create an array of seconds from byseconds for fast lookup. */
	memset (&seconds, 0, sizeof (seconds));
	while (elem) {
		sec = GPOINTER_TO_INT (elem->data);
		seconds[sec] = 1;
		elem = elem->next;
	}

	new_occs = g_array_new (FALSE, FALSE, sizeof (CalObjTime));

	len = occs->len;
	for (i = 0; i < len; i++) {
		occ = &g_array_index (occs, CalObjTime, i);
		if (seconds[occ->second])
			g_array_append_vals (new_occs, occ, 1);
	}

	g_array_free (occs, TRUE);

	return new_occs;
}



static void
cal_obj_time_add_days (CalObjTime *cotime,
		       gint	   days)
{
	guint day, days_in_month;

	/* We use a guint to avoid overflow on the guint8. */
	day = (guint) cotime->day;
	day += days;

	for (;;) {
		days_in_month = time_days_in_month (cotime->year,
						    cotime->month);
		if (day <= days_in_month)
			break;

		cotime->month++;
		if (cotime->month >= 12) {
			cotime->year++;
			cotime->month = 0;
		}

		day -= days_in_month;
	}

	cotime->day = (guint8) day;
}
