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

#include <config.h>
#include <stdlib.h>
#include <string.h>
#include <cal-util/calobj.h>
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
 *   multiples of the frequency which we step by. We generate a 'set' of
 *   occurrences for each period defined by the frequency & interval.
 *   So for a YEARLY frequency with an interval of 3, we generate a set of
 *   occurrences for every 3rd year. We use complete years here -  any
 *   generated occurrences that occur before the event's start (or after its
 *   end) are just discarded.
 *
 * o There are 8 frequency modifiers: BYMONTH, BYWEEKNO, BYYEARDAY, BYMONTHDAY,
 *   BYDAY, BYHOUR, BYMINUTE & BYSECOND. These can either add extra occurrences
 *   or filter out occurrences. For example 'FREQ=YEARLY;BYMONTH=1,2' produces
 *   2 occurrences for each year rather than the default 1. And
 *   'FREQ=DAILY;BYMONTH=1' filters out all occurrences except those in Jan.
 *   If the modifier works on periods which are less than the recurrence
 *   frequency, then extra occurrences are added, otherwise occurrences are
 *   filtered. So we have 2 functions for each modifier - one to expand events
 *   and the other to filter. We use a table of functions for each frequency
 *   which points to the appropriate function to use for each modifier.
 *
 * o Any number of frequency modifiers can be used in a recurrence rule.
 *   (Though the iCalendar spec says that BYWEEKNO can only be used in a YEARLY
 *   rule, and some modifiers aren't appropriate for some frequencies - e.g.
 *   BYMONTHDAY is not really useful in a WEEKLY frequency, and BYYEARDAY is
 *   not useful in a MONTHLY or WEEKLY frequency).
 *   The frequency modifiers are applied in the order given above. The first 5
 *   modifier rules (BYMONTH, BYWEEKNO, BYYEARDAY, BYMONTHDAY & BYDAY) all
 *   produce the days on which the occurrences take place, and so we have to
 *   compute some of these in parallel rather than sequentially, or we may end
 *   up with too many days.
 *
 * o Note that some expansion functions may produce days which are invalid,
 *   e.g. 31st September, 30th Feb. These invalid days are removed before the
 *   BYHOUR, BYMINUTE & BYSECOND modifier functions are applied.
 *
 * o After the set of occurrences for the frequency interval are generated,
 *   the BYSETPOS property is used to select which of the occurrences are
 *   finally output. If BYSETPOS is not specified then all the occurrences are
 *   output.
 */


/* This is what we use to pass to all the filter functions. */
typedef struct _RecurData RecurData;
struct _RecurData {
	CalObjRecurrence *recur;

	/* This is used for the WEEKLY frequency. */
	gint weekday;

	/* This is used for fast lookup in BYMONTH filtering. */
	guint8 months[12];

	/* This is used for fast lookup in BYYEARDAY filtering. */
	guint8 yeardays[367], neg_yeardays[367]; /* Days are 1 - 366. */

	/* This is used for fast lookup in BYMONTHDAY filtering. */
	guint8 monthdays[32], neg_monthdays[32]; /* Days are 1 to 31. */

	/* This is used for fast lookup in BYDAY filtering. */
	guint8 weekdays[7];

	/* This is used for fast lookup in BYHOUR filtering. */
	guint8 hours[24];

	/* This is used for fast lookup in BYMINUTE filtering. */
	guint8 minutes[60];

	/* This is used for fast lookup in BYSECOND filtering. */
	guint8 seconds[61];
};



typedef gboolean (*CalObjFindStartFn) (CalObjTime *event_start,
				       CalObjTime *event_end,
				       RecurData  *recur_data,
				       CalObjTime *interval_start,
				       CalObjTime *interval_end,
				       CalObjTime *cotime);
typedef gboolean (*CalObjFindNextFn)  (CalObjTime *cotime,
				       CalObjTime *event_end,
				       RecurData  *recur_data,
				       CalObjTime *interval_end);
typedef GArray*	 (*CalObjFilterFn)    (RecurData  *recur_data,
				       GArray     *occs);

typedef struct _CalObjRecurVTable CalObjRecurVTable;
struct _CalObjRecurVTable {
	CalObjFindStartFn find_start_position;
	CalObjFindNextFn find_next_position;

	CalObjFilterFn bymonth_filter;
	CalObjFilterFn byweekno_filter;
	CalObjFilterFn byyearday_filter;
	CalObjFilterFn bymonthday_filter;
	CalObjFilterFn byday_filter;
	CalObjFilterFn byhour_filter;
	CalObjFilterFn byminute_filter;
	CalObjFilterFn bysecond_filter;
};


/* This is used to specify which parts of the CalObjTime to compare in
   cal_obj_time_compare(). */
typedef enum {
	CALOBJ_YEAR,
	CALOBJ_MONTH,
	CALOBJ_DAY,
	CALOBJ_HOUR,
	CALOBJ_MINUTE,
	CALOBJ_SECOND
} CalObjTimeComparison;

static GArray*	cal_obj_generate_set_yearly	(RecurData	*recur_data,
						 CalObjRecurVTable *vtable,
						 CalObjTime	*occ);
static GArray*	cal_obj_generate_set_monthly	(RecurData	*recur_data,
						 CalObjRecurVTable *vtable,
						 CalObjTime	*occ);
static GArray*	cal_obj_generate_set_default	(RecurData	*recur_data,
						 CalObjRecurVTable *vtable,
						 CalObjTime	*occ);


static CalObjRecurVTable* cal_obj_get_vtable	(CalObjRecurType	 recur_type);
static void	cal_obj_initialize_recur_data	(RecurData	*recur_data,
						 CalObjRecurrence	*recur,
						 CalObjTime	*event_start);
static void	cal_obj_sort_occurrences	(GArray		*occs);
static gint	cal_obj_time_compare_func	(const void	*arg1,
						 const void	*arg2);
static void	cal_obj_remove_duplicates_and_invalid_dates (GArray	*occs);
static GArray*	cal_obj_bysetpos_filter		(CalObjRecurrence	*recur,
						 GArray		*occs);


static gboolean cal_obj_yearly_find_start_position (CalObjTime *event_start,
						    CalObjTime *event_end,
						    RecurData  *recur_data,
						    CalObjTime *interval_start,
						    CalObjTime *interval_end,
						    CalObjTime *cotime);
static gboolean cal_obj_yearly_find_next_position  (CalObjTime *cotime,
						    CalObjTime *event_end,
						    RecurData  *recur_data,
						    CalObjTime *interval_end);

static gboolean cal_obj_monthly_find_start_position (CalObjTime *event_start,
						     CalObjTime *event_end,
						     RecurData  *recur_data,
						     CalObjTime *interval_start,
						     CalObjTime *interval_end,
						     CalObjTime *cotime);
static gboolean cal_obj_monthly_find_next_position  (CalObjTime *cotime,
						     CalObjTime *event_end,
						     RecurData  *recur_data,
						     CalObjTime *interval_end);

static gboolean cal_obj_weekly_find_start_position (CalObjTime *event_start,
						    CalObjTime *event_end,
						    RecurData  *recur_data,
						    CalObjTime *interval_start,
						    CalObjTime *interval_end,
						    CalObjTime *cotime);
static gboolean cal_obj_weekly_find_next_position  (CalObjTime *cotime,
						    CalObjTime *event_end,
						    RecurData  *recur_data,
						    CalObjTime *interval_end);

static gboolean cal_obj_daily_find_start_position  (CalObjTime *event_start,
						    CalObjTime *event_end,
						    RecurData  *recur_data,
						    CalObjTime *interval_start,
						    CalObjTime *interval_end,
						    CalObjTime *cotime);
static gboolean cal_obj_daily_find_next_position   (CalObjTime *cotime,
						    CalObjTime *event_end,
						    RecurData  *recur_data,
						    CalObjTime *interval_end);

static gboolean cal_obj_hourly_find_start_position (CalObjTime *event_start,
						    CalObjTime *event_end,
						    RecurData  *recur_data,
						    CalObjTime *interval_start,
						    CalObjTime *interval_end,
						    CalObjTime *cotime);
static gboolean cal_obj_hourly_find_next_position  (CalObjTime *cotime,
						    CalObjTime *event_end,
						    RecurData  *recur_data,
						    CalObjTime *interval_end);

static gboolean cal_obj_minutely_find_start_position (CalObjTime *event_start,
						      CalObjTime *event_end,
						      RecurData  *recur_data,
						      CalObjTime *interval_start,
						      CalObjTime *interval_end,
						      CalObjTime *cotime);
static gboolean cal_obj_minutely_find_next_position  (CalObjTime *cotime,
						      CalObjTime *event_end,
						      RecurData  *recur_data,
						      CalObjTime *interval_end);

static gboolean cal_obj_secondly_find_start_position (CalObjTime *event_start,
						      CalObjTime *event_end,
						      RecurData  *recur_data,
						      CalObjTime *interval_start,
						      CalObjTime *interval_end,
						      CalObjTime *cotime);
static gboolean cal_obj_secondly_find_next_position  (CalObjTime *cotime,
						      CalObjTime *event_end,
						      RecurData  *recur_data,
						      CalObjTime *interval_end);

static GArray* cal_obj_bymonth_expand		(RecurData  *recur_data,
						 GArray     *occs);
static GArray* cal_obj_bymonth_filter		(RecurData  *recur_data,
						 GArray     *occs);
static GArray* cal_obj_byweekno_expand		(RecurData  *recur_data,
						 GArray     *occs);
#if 0
/* This isn't used at present. */
static GArray* cal_obj_byweekno_filter		(RecurData  *recur_data,
						 GArray     *occs);
#endif
static GArray* cal_obj_byyearday_expand		(RecurData  *recur_data,
						 GArray     *occs);
static GArray* cal_obj_byyearday_filter		(RecurData  *recur_data,
						 GArray     *occs);
static GArray* cal_obj_bymonthday_expand	(RecurData  *recur_data,
						 GArray     *occs);
static GArray* cal_obj_bymonthday_filter	(RecurData  *recur_data,
						 GArray     *occs);
static GArray* cal_obj_byday_expand_yearly	(RecurData  *recur_data,
						 GArray     *occs);
static GArray* cal_obj_byday_expand_monthly	(RecurData  *recur_data,
						 GArray     *occs);
static GArray* cal_obj_byday_expand_weekly	(RecurData  *recur_data,
						 GArray     *occs);
static GArray* cal_obj_byday_filter		(RecurData  *recur_data,
						 GArray     *occs);
static GArray* cal_obj_byhour_expand		(RecurData  *recur_data,
						 GArray     *occs);
static GArray* cal_obj_byhour_filter		(RecurData  *recur_data,
						 GArray     *occs);
static GArray* cal_obj_byminute_expand		(RecurData  *recur_data,
						 GArray     *occs);
static GArray* cal_obj_byminute_filter		(RecurData  *recur_data,
						 GArray     *occs);
static GArray* cal_obj_bysecond_expand		(RecurData  *recur_data,
						 GArray     *occs);
static GArray* cal_obj_bysecond_filter		(RecurData  *recur_data,
						 GArray     *occs);

static void cal_obj_time_add_months		(CalObjTime *cotime,
						 gint	     months);
static void cal_obj_time_add_days		(CalObjTime *cotime,
						 gint	     days);
static void cal_obj_time_subtract_days		(CalObjTime *cotime,
						 gint	     days);
static void cal_obj_time_add_hours		(CalObjTime *cotime,
						 gint	     hours);
static void cal_obj_time_add_minutes		(CalObjTime *cotime,
						 gint	     minutes);
static void cal_obj_time_add_seconds		(CalObjTime *cotime,
						 gint	     seconds);
static gint cal_obj_time_compare		(CalObjTime *cotime1,
						 CalObjTime *cotime2,
						 CalObjTimeComparison type);
static gint cal_obj_time_weekday		(CalObjTime *cotime,
						 CalObjRecurrence *recur);
static gint cal_obj_time_day_of_year		(CalObjTime *cotime);
static void cal_obj_time_find_first_week	(CalObjTime *cotime,
						 RecurData  *recur_data);


CalObjRecurVTable cal_obj_yearly_vtable = {
	cal_obj_yearly_find_start_position,
	cal_obj_yearly_find_next_position,

	cal_obj_bymonth_expand,
	cal_obj_byweekno_expand,
	cal_obj_byyearday_expand,
	cal_obj_bymonthday_expand,
	cal_obj_byday_expand_yearly,
	cal_obj_byhour_expand,
	cal_obj_byminute_expand,
	cal_obj_bysecond_expand
};

CalObjRecurVTable cal_obj_monthly_vtable = {
	cal_obj_monthly_find_start_position,
	cal_obj_monthly_find_next_position,

	cal_obj_bymonth_filter,
	NULL, /* BYWEEKNO is only applicable to YEARLY frequency. */
	NULL, /* BYYEARDAY is not useful in a MONTHLY frequency. */
	cal_obj_bymonthday_expand,
	cal_obj_byday_expand_monthly,
	cal_obj_byhour_expand,
	cal_obj_byminute_expand,
	cal_obj_bysecond_expand
};

CalObjRecurVTable cal_obj_weekly_vtable = {
	cal_obj_weekly_find_start_position,
	cal_obj_weekly_find_next_position,

	cal_obj_bymonth_filter,
	NULL, /* BYWEEKNO is only applicable to YEARLY frequency. */
	NULL, /* BYYEARDAY is not useful in a WEEKLY frequency. */
	NULL, /* BYMONTHDAY is not useful in a WEEKLY frequency. */
	cal_obj_byday_expand_weekly,
	cal_obj_byhour_expand,
	cal_obj_byminute_expand,
	cal_obj_bysecond_expand
};

CalObjRecurVTable cal_obj_daily_vtable = {
	cal_obj_daily_find_start_position,
	cal_obj_daily_find_next_position,

	cal_obj_bymonth_filter,
	NULL, /* BYWEEKNO is only applicable to YEARLY frequency. */
	cal_obj_byyearday_filter,
	cal_obj_bymonthday_filter,
	cal_obj_byday_filter,
	cal_obj_byhour_expand,
	cal_obj_byminute_expand,
	cal_obj_bysecond_expand
};

CalObjRecurVTable cal_obj_hourly_vtable = {
	cal_obj_hourly_find_start_position,
	cal_obj_hourly_find_next_position,

	cal_obj_bymonth_filter,
	NULL, /* BYWEEKNO is only applicable to YEARLY frequency. */
	cal_obj_byyearday_filter,
	cal_obj_bymonthday_filter,
	cal_obj_byday_filter,
	cal_obj_byhour_filter,
	cal_obj_byminute_expand,
	cal_obj_bysecond_expand
};

CalObjRecurVTable cal_obj_minutely_vtable = {
	cal_obj_minutely_find_start_position,
	cal_obj_minutely_find_next_position,

	cal_obj_bymonth_filter,
	NULL, /* BYWEEKNO is only applicable to YEARLY frequency. */
	cal_obj_byyearday_filter,
	cal_obj_bymonthday_filter,
	cal_obj_byday_filter,
	cal_obj_byhour_filter,
	cal_obj_byminute_filter,
	cal_obj_bysecond_expand
};

CalObjRecurVTable cal_obj_secondly_vtable = {
	cal_obj_secondly_find_start_position,
	cal_obj_secondly_find_next_position,

	cal_obj_bymonth_filter,
	NULL, /* BYWEEKNO is only applicable to YEARLY frequency. */
	cal_obj_byyearday_filter,
	cal_obj_bymonthday_filter,
	cal_obj_byday_filter,
	cal_obj_byhour_filter,
	cal_obj_byminute_filter,
	cal_obj_bysecond_filter
};



/*
 * Calls the given callback function for each occurrence of the event between
 * the given start and end times. If end is 0 it continues until the event
 * ends or forever if the event has an infinite recurrence rule.
 * If the callback routine return 0 the occurrence generation stops.
 *
 * NOTE: This could replace ical_object_generate_events() eventually.
 */
void
cal_object_generate_events (iCalObject	*ico,
			    time_t	 start,
			    time_t	 end,
			    calendarfn   cb,
			    void	*closure)
{

	/* FIXME: The iCalObject should have a list of RRULES & RDATES and a
	   list of EXRULES & EXDATES. */


	/* If there is no recurrence, just call the callback if the event
	   intersects the given interval. */
	if (!ico->recur) {
		if ((end && (ico->dtstart < end) && (ico->dtend > start))
		    || ((end == 0) && (ico->dtend > start))) {
			(* cb) (ico, ico->dtstart, ico->dtend, closure);
		}
		return;
	}


	/* Expand each of the recurrence rules. */



	/* Add on specific occurrence dates. */


	/* Expand each of the exception rules. */


	/* Add on specific exception dates. */


	/* Sort both arrays. */


	/* Create the final array, by removing the exceptions from the
	   occurrences, and removing any duplicates. */


}



/* Returns an unsorted GArray of CalObjTime's resulting from expanding the
   given recurrence rule within the given interval. */
GArray*
cal_obj_expand_recurrence		(CalObjTime	*event_start,
					 CalObjRecurrence *recur,
					 CalObjTime	*interval_start,
					 CalObjTime	*interval_end)
{
	CalObjRecurVTable *vtable;
	CalObjTime *event_end = NULL, event_end_cotime;
	RecurData recur_data;
	CalObjTime occ;
	GArray *all_occs, *occs;
	struct tm *event_end_tm;

	vtable = cal_obj_get_vtable (recur->type);

	/* This is the resulting array of CalObjTime elements. */
	all_occs = g_array_new (FALSE, FALSE, sizeof (CalObjTime));

	/* Calculate some useful data such as some fast lookup tables. */
	cal_obj_initialize_recur_data (&recur_data, recur, event_start);

	/* Compute the event_end, if the recur's enddate is set. */
	if (recur->enddate) {
		event_end_tm = localtime (&recur->enddate);
		event_end_cotime.year   = event_end_tm->tm_year + 1900;
		event_end_cotime.month  = event_end_tm->tm_mon;
		event_end_cotime.day    = event_end_tm->tm_mday;
		event_end_cotime.hour   = event_end_tm->tm_hour;
		event_end_cotime.minute = event_end_tm->tm_min;
		event_end_cotime.second = event_end_tm->tm_sec;

		event_end = &event_end_cotime;
	}

	/* Get the first period based on the frequency and the interval that
	   intersects the interval between start and end. */
	if ((*vtable->find_start_position) (event_start, event_end,
					    &recur_data,
					    interval_start, interval_end,
					    &occ))
		return all_occs;

	/* Loop until the event ends or we go past the end of the required
	   interval. */
	for (;;) {
		/* Generate the set of occurrences for this period. */
		/* FIXME: We shouldn't allow multiple expansion filters to be
		   used to expand the days. They should be done separately and
		   combined after, if needed. */
		/* FIXME: Remove invalid days before HOURLY/MINUTELY/SECONDLY
		   rules? Problems with WEEKLY as well? */
		switch (recur->type) {
		case CAL_RECUR_YEARLY:
			occs = cal_obj_generate_set_yearly (&recur_data,
							    vtable, &occ);
			break;
		case CAL_RECUR_MONTHLY:
			occs = cal_obj_generate_set_monthly (&recur_data,
							     vtable, &occ);
			break;
		default:
			occs = cal_obj_generate_set_default (&recur_data,
							     vtable, &occ);
			break;
		}

		/* Sort the occurrences and remove duplicates. */
		cal_obj_sort_occurrences (occs);
		cal_obj_remove_duplicates_and_invalid_dates (occs);

		/* Apply the BYSETPOS property. */
		occs = cal_obj_bysetpos_filter (recur, occs);

		/* Add the occurrences onto the main array. */
		g_array_append_vals (all_occs, occs->data, occs->len);

		/* Skip to the next period, or exit the loop if finished. */
		if ((*vtable->find_next_position) (&occ, event_end,
						   &recur_data, interval_end))
			break;
	}

	return all_occs;
}


static GArray*
cal_obj_generate_set_yearly	(RecurData *recur_data,
				 CalObjRecurVTable *vtable,
				 CalObjTime *occ)
{
	CalObjRecurrence *recur = recur_data->recur;
	GArray *occs_arrays[4], *occs, *occs2;
	gint num_occs_arrays = 0, i;

	/* This is a bit complicated, since the iCalendar spec says that
	   several BYxxx modifiers can be used simultaneously. So we have to
	   be quite careful when determining the days of the occurrences.
	   The BYHOUR, BYMINUTE & BYSECOND modifiers are no problem at all.

	   The modifiers we have to worry about are: BYMONTH, BYWEEKNO,
	   BYYEARDAY, BYMONTHDAY & BYDAY. We can't do these sequentially
	   since each filter will mess up the results of the previous one.
	   But they aren't all completely independant, e.g. BYMONTHDAY and
	   BYDAY are related to BYMONTH, and BYDAY is related to BYWEEKNO.

	   BYDAY can also be applied independently, which makes it worse.
	   So we assume that if BYMONTH or BYWEEKNO is used, then the BYDAY
	   modifier applies to those, else it is applied independantly.

	   We expand the occurrences in parallel into the occs_arrays[] array,
	   and then merge them all into one GArray before expanding BYHOUR,
	   BYMINUTE & BYSECOND. */
	   
	if (recur->bymonth) {
		occs = g_array_new (FALSE, FALSE, sizeof (CalObjTime));
		g_array_append_vals (occs, occ, 1);

		occs = (*vtable->bymonth_filter) (recur_data, occs);

		/* If BYMONTHDAY & BYDAY are both set we need to expand them
		   in parallel and add the results. */
		if (recur->bymonthday && recur->byday) {
			/* Copy the occs array. */
			occs2 = g_array_new (FALSE, FALSE,
					     sizeof (CalObjTime));
			g_array_append_vals (occs2, occs->data, occs->len);

			occs = (*vtable->bymonthday_filter) (recur_data, occs);
			/* Note that we explicitly call the monthly version
			   of the BYDAY expansion filter. */
			occs2 = cal_obj_byday_expand_monthly (recur_data,
							      occs2);

			/* Add the 2 resulting arrays together. */
			g_array_append_vals (occs, occs2->data, occs2->len);
			g_array_free (occs2, TRUE);
		} else {
			occs = (*vtable->bymonthday_filter) (recur_data, occs);
			/* Note that we explicitly call the monthly version
			   of the BYDAY expansion filter. */
			occs = cal_obj_byday_expand_monthly (recur_data, occs);
		}

		occs_arrays[num_occs_arrays++] = occs;
	}

	if (recur->byweekno) {
		occs = g_array_new (FALSE, FALSE, sizeof (CalObjTime));
		g_array_append_vals (occs, occ, 1);

		/* FIXME: We may have an invalid date here. What to do?? */

		occs = (*vtable->byweekno_filter) (recur_data, occs);
		/* Note that we explicitly call the weekly version of the
		   BYDAY expansion filter. */
		occs = cal_obj_byday_expand_weekly (recur_data, occs);

		occs_arrays[num_occs_arrays++] = occs;
	}

	if (recur->byyearday) {
		occs = g_array_new (FALSE, FALSE, sizeof (CalObjTime));
		g_array_append_vals (occs, occ, 1);

		occs = (*vtable->byyearday_filter) (recur_data, occs);

		occs_arrays[num_occs_arrays++] = occs;
	}

	/* If BYDAY is set, and BYMONTH and BYWEEKNO are not set, we need to
	   expand BYDAY independantly. */
	if (recur->byday && !recur->bymonth && !recur->byweekno) {
		occs = g_array_new (FALSE, FALSE, sizeof (CalObjTime));
		g_array_append_vals (occs, occ, 1);

		occs = (*vtable->byday_filter) (recur_data, occs);

		occs_arrays[num_occs_arrays++] = occs;
	}

	/* Add all the arrays together. */
	occs = occs_arrays[0];
	for (i = 1; i < num_occs_arrays; i++) {
		occs2 = occs_arrays[i];
		g_array_append_vals (occs, occs2->data, occs2->len);
		g_array_free (occs2, TRUE);
	}

	/* Now expand BYHOUR, BYMINUTE & BYSECOND. */
	occs = (*vtable->byhour_filter) (recur_data, occs);
	occs = (*vtable->byminute_filter) (recur_data, occs);
	occs = (*vtable->bysecond_filter) (recur_data, occs);

	return occs;
}


static GArray*
cal_obj_generate_set_monthly	(RecurData *recur_data,
				 CalObjRecurVTable *vtable,
				 CalObjTime *occ)
{
	GArray *occs, *occs2;

	/* We start with just the one time in each set. */
	occs = g_array_new (FALSE, FALSE, sizeof (CalObjTime));
	g_array_append_vals (occs, occ, 1);

	occs = (*vtable->bymonth_filter) (recur_data, occs);

	/* We need to combine the output of BYMONTHDAY & BYDAY, by doing them
	   in parallel rather than sequentially. If we did them sequentially
	   then we would lose the occurrences generated by BYMONTHDAY, and
	   instead have repetitions of the occurrences from BYDAY. */
	if (recur_data->recur->bymonthday && recur_data->recur->byday) {
		occs2 = g_array_new (FALSE, FALSE, sizeof (CalObjTime));
		g_array_append_vals (occs2, occs->data, occs->len);

		occs = (*vtable->bymonthday_filter) (recur_data, occs);
		occs2 = (*vtable->byday_filter) (recur_data, occs2);

		g_array_append_vals (occs, occs2->data, occs2->len);
		g_array_free (occs2, TRUE);
	} else {
		occs = (*vtable->bymonthday_filter) (recur_data, occs);
		occs = (*vtable->byday_filter) (recur_data, occs);
	}

	occs = (*vtable->byhour_filter) (recur_data, occs);
	occs = (*vtable->byminute_filter) (recur_data, occs);
	occs = (*vtable->bysecond_filter) (recur_data, occs);

	return occs;
}


static GArray*
cal_obj_generate_set_default	(RecurData *recur_data,
				 CalObjRecurVTable *vtable,
				 CalObjTime *occ)
{
	GArray *occs;

	/* We start with just the one time in the set. */
	occs = g_array_new (FALSE, FALSE, sizeof (CalObjTime));
	g_array_append_vals (occs, occ, 1);

	occs = (*vtable->bymonth_filter) (recur_data, occs);
	if (vtable->byweekno_filter)
		occs = (*vtable->byweekno_filter) (recur_data, occs);
	if (vtable->byyearday_filter)
		occs = (*vtable->byyearday_filter) (recur_data, occs);
	if (vtable->bymonthday_filter)
		occs = (*vtable->bymonthday_filter) (recur_data, occs);
	occs = (*vtable->byday_filter) (recur_data, occs);

	occs = (*vtable->byhour_filter) (recur_data, occs);
	occs = (*vtable->byminute_filter) (recur_data, occs);
	occs = (*vtable->bysecond_filter) (recur_data, occs);

	return occs;
}



/* Returns the function table corresponding to the recurrence frequency. */
static CalObjRecurVTable*
cal_obj_get_vtable (CalObjRecurType recur_type)
{
	switch (recur_type) {
	case CAL_RECUR_YEARLY:
		return &cal_obj_yearly_vtable;
	case CAL_RECUR_MONTHLY:
		return &cal_obj_monthly_vtable;
	case CAL_RECUR_WEEKLY:
		return &cal_obj_weekly_vtable;
	case CAL_RECUR_DAILY:
		return &cal_obj_daily_vtable;
	case CAL_RECUR_HOURLY:
		return &cal_obj_hourly_vtable;
	case CAL_RECUR_MINUTELY:
		return &cal_obj_minutely_vtable;
	case CAL_RECUR_SECONDLY:
		return &cal_obj_secondly_vtable;
	}
	return NULL;
}


/* This creates a number of fast lookup tables used when filtering with the
   modifier properties BYMONTH, BYYEARDAY etc. */
static void
cal_obj_initialize_recur_data (RecurData  *recur_data,
			       CalObjRecurrence *recur,
			       CalObjTime *event_start)
{
	GList *elem;
	gint month, yearday, monthday, weekday, week_num, hour, minute, second;

	/* Clear the entire RecurData. */
	memset (recur_data, 0, sizeof (RecurData));

	recur_data->recur = recur;

	/* Set the weekday, used for the WEEKLY frequency and the BYWEEKNO
	   modifier. */
	recur_data->weekday = cal_obj_time_weekday (event_start, recur);

	/* Create an array of months from bymonths for fast lookup. */
	elem = recur->bymonth;
	while (elem) {
		month = GPOINTER_TO_INT (elem->data);
		recur_data->months[month] = 1;
		elem = elem->next;
	}

	/* Create an array of yeardays from byyearday for fast lookup.
	   We create a second array to handle the negative values. The first
	   element there corresponds to the last day of the year. */
	elem = recur->byyearday;
	while (elem) {
		yearday = GPOINTER_TO_INT (elem->data);
		if (yearday >= 0)
			recur_data->yeardays[yearday] = 1;
		else
			recur_data->neg_yeardays[-yearday] = 1;
		elem = elem->next;
	}

	/* Create an array of monthdays from bymonthday for fast lookup.
	   We create a second array to handle the negative values. The first
	   element there corresponds to the last day of the month. */
	elem = recur->bymonthday;
	while (elem) {
		monthday = GPOINTER_TO_INT (elem->data);
		if (monthday >= 0)
			recur_data->monthdays[monthday] = 1;
		else
			recur_data->neg_monthdays[-monthday] = 1;
		elem = elem->next;
	}

	/* Create an array of weekdays from byday for fast lookup. */
	elem = recur->byday;
	while (elem) {
		weekday = GPOINTER_TO_INT (elem->data);
		elem = elem->next;
		/* The week number is not used when filtering. */
		week_num = GPOINTER_TO_INT (elem->data);
		elem = elem->next;

		recur_data->weekdays[weekday] = 1;
	}

	/* Create an array of hours from byhour for fast lookup. */
	elem = recur->byhour;
	while (elem) {
		hour = GPOINTER_TO_INT (elem->data);
		recur_data->hours[hour] = 1;
		elem = elem->next;
	}

	/* Create an array of minutes from byminutes for fast lookup. */
	elem = recur->byminute;
	while (elem) {
		minute = GPOINTER_TO_INT (elem->data);
		recur_data->minutes[minute] = 1;
		elem = elem->next;
	}

	/* Create an array of seconds from byseconds for fast lookup. */
	elem = recur->bysecond;
	while (elem) {
		second = GPOINTER_TO_INT (elem->data);
		recur_data->seconds[second] = 1;
		elem = elem->next;
	}
}


static void
cal_obj_sort_occurrences (GArray *occs)
{
	qsort (occs->data, occs->len, sizeof (CalObjTime),
	       cal_obj_time_compare_func);
}


static void
cal_obj_remove_duplicates_and_invalid_dates (GArray *occs)
{
	static const int days_in_month[12] = {
		31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31
	};

	CalObjTime *occ, *prev_occ = NULL;
	gint len, i, j = 0, year, month, days;
	gboolean keep_occ;

	len = occs->len;
	for (i = 0; i < len; i++) {
		occ = &g_array_index (occs, CalObjTime, i);
		keep_occ = TRUE;

		if (prev_occ && cal_obj_time_compare_func (occ,
							   prev_occ) == 0)
			keep_occ = FALSE;

		year = occ->year;
		month = occ->month;
		days = days_in_month[occ->month];
		/* If it is february and a leap year, add a day. */
		if (month == 1 && (year % 4 == 0
				   && (year % 100 != 0
				       || year % 400 == 0)))
			days++;
		if (occ->day > days)
			keep_occ = FALSE;

		if (keep_occ) {
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
cal_obj_bysetpos_filter (CalObjRecurrence *recur,
			 GArray	    *occs)
{
	GArray *new_occs;
	CalObjTime *occ;
	GList *elem;
	gint len, pos;

	/* If BYSETPOS has not been specified, or the array is empty, just
	   return the array. */
	elem = recur->bysetpos;
	if (!elem || occs->len == 0)
		return occs;

	new_occs = g_array_new (FALSE, FALSE, sizeof (CalObjTime));

	/* Iterate over the indices given in bysetpos, adding the corresponding
	   element from occs to new_occs. */
	len = occs->len;
	while (elem) {
		pos = GPOINTER_TO_INT (elem->data);

		/* Negative values count back from the end of the array. */
		if (pos < 0)
			pos += len;

		if (pos >= 0 && pos < len) {
			occ = &g_array_index (occs, CalObjTime, pos);
			g_array_append_vals (new_occs, occ, 1);
		}
		elem = elem->next;
	}

	g_array_free (occs, TRUE);

	return new_occs;
}




/* Finds the first year from the event_start, counting in multiples of the
   recurrence interval, that intersects the given interval. It returns TRUE
   if there is no intersection. */
static gboolean
cal_obj_yearly_find_start_position (CalObjTime *event_start,
				    CalObjTime *event_end,
				    RecurData  *recur_data,
				    CalObjTime *interval_start,
				    CalObjTime *interval_end,
				    CalObjTime *cotime)
{
	*cotime = *event_start;

	/* Move on to the next interval, if the event starts before the
	   given interval. */
	if (cotime->year < interval_start->year) {
		gint years = interval_start->year - cotime->year
			+ recur_data->recur->interval - 1;
		years -= years % recur_data->recur->interval;
		/* NOTE: The day may now be invalid, e.g. 29th Feb. */
		cotime->year += years;
	}

	if ((event_end && cotime->year > event_end->year)
	    || (interval_end && cotime->year > interval_end->year))
		return TRUE;

	return FALSE;
}


static gboolean
cal_obj_yearly_find_next_position (CalObjTime *cotime,
				   CalObjTime *event_end,
				   RecurData  *recur_data,
				   CalObjTime *interval_end)
{
	/* NOTE: The day may now be invalid, e.g. 29th Feb. */
	cotime->year += recur_data->recur->interval;

	if ((event_end && cotime->year > event_end->year)
	    || (interval_end && cotime->year > interval_end->year))
		return TRUE;

	return FALSE;
}



static gboolean
cal_obj_monthly_find_start_position (CalObjTime *event_start,
				     CalObjTime *event_end,
				     RecurData  *recur_data,
				     CalObjTime *interval_start,
				     CalObjTime *interval_end,
				     CalObjTime *cotime)
{
	*cotime = *event_start;

	/* Move on to the next interval, if the event starts before the
	   given interval. */
	if (cal_obj_time_compare (cotime, interval_start, CALOBJ_MONTH) < 0) {
		gint months = (interval_start->year - cotime->year) * 12
			+ interval_start->month - cotime->month
			+ recur_data->recur->interval - 1;
		months -= months % recur_data->recur->interval;
		/* NOTE: The day may now be invalid, e.g. 31st Sep. */
		cal_obj_time_add_months (cotime, months);
	}

	if (event_end && cal_obj_time_compare (cotime, event_end,
					       CALOBJ_MONTH) > 0)
		return TRUE;
	if (interval_end && cal_obj_time_compare (cotime, interval_end,
						  CALOBJ_MONTH) > 0)
		return TRUE;

	return FALSE;
}


static gboolean
cal_obj_monthly_find_next_position (CalObjTime *cotime,
				    CalObjTime *event_end,
				    RecurData  *recur_data,
				    CalObjTime *interval_end)
{
	/* NOTE: The day may now be invalid, e.g. 31st Sep. */
	cal_obj_time_add_months (cotime, recur_data->recur->interval);

	if (event_end && cal_obj_time_compare (cotime, event_end,
					       CALOBJ_MONTH) > 0)
		return TRUE;
	if (interval_end && cal_obj_time_compare (cotime, interval_end,
						  CALOBJ_MONTH) > 0)
		return TRUE;

	return FALSE;
}



static gboolean
cal_obj_weekly_find_start_position (CalObjTime *event_start,
				    CalObjTime *event_end,
				    RecurData  *recur_data,
				    CalObjTime *interval_start,
				    CalObjTime *interval_end,
				    CalObjTime *cotime)
{
	GDate event_start_date, interval_start_date;
	guint32 event_start_julian, interval_start_julian;
	gint interval_start_weekday;
	CalObjTime week_start;

	if (event_end && cal_obj_time_compare (event_end, interval_start,
					       CALOBJ_DAY) < 0)
		return TRUE;
	if (interval_end && cal_obj_time_compare (event_start, interval_end,
						  CALOBJ_DAY) > 0)
		return TRUE;

	*cotime = *event_start;

	/* Convert the event start and interval start to GDates, so we can
	   easily find the number of days between them. */
	g_date_clear (&event_start_date, 1);
	g_date_set_dmy (&event_start_date, event_start->day,
			event_start->month + 1, event_start->year);
	g_date_clear (&interval_start_date, 1);
	g_date_set_dmy (&interval_start_date, interval_start->day,
			interval_start->month + 1, interval_start->year);

	/* Calculate the start of the weeks corresponding to the event start
	   and interval start. */
	event_start_julian = g_date_julian (&event_start_date);
	event_start_julian -= recur_data->weekday;

	interval_start_julian = g_date_julian (&interval_start_date);
	interval_start_weekday = cal_obj_time_weekday (interval_start,
						       recur_data->recur);
	interval_start_julian -= interval_start_weekday;

	/* We want to find the first full week using the recurrence interval
	   that intersects the given interval dates. */
	if (event_start_julian < interval_start_julian) {
		gint weeks = (interval_start_julian - event_start_julian) / 7;
		weeks += recur_data->recur->interval - 1;
		weeks -= weeks % recur_data->recur->interval;
		cal_obj_time_add_days (cotime, weeks * 7);
	}

	week_start = *cotime;
	cal_obj_time_subtract_days (&week_start, recur_data->weekday);

	if (event_end && cal_obj_time_compare (&week_start, event_end,
					       CALOBJ_DAY) > 0)
		return TRUE;
	if (interval_end && cal_obj_time_compare (&week_start, interval_end,
						  CALOBJ_DAY) > 0)
		return TRUE;

	return FALSE;
}


static gboolean
cal_obj_weekly_find_next_position (CalObjTime *cotime,
				   CalObjTime *event_end,
				   RecurData  *recur_data,
				   CalObjTime *interval_end)
{
	CalObjTime week_start;

	cal_obj_time_add_days (cotime, recur_data->recur->interval * 7);

	/* Return TRUE if the start of this week is after the event finishes
	   or is after the end of the required interval. */
	week_start = *cotime;
	cal_obj_time_subtract_days (&week_start, recur_data->weekday);

	if (event_end && cal_obj_time_compare (&week_start, event_end,
					       CALOBJ_DAY) > 0)
		return TRUE;
	if (interval_end && cal_obj_time_compare (&week_start, interval_end,
						  CALOBJ_DAY) > 0)
		return TRUE;

	return FALSE;
}


static gboolean
cal_obj_daily_find_start_position  (CalObjTime *event_start,
				    CalObjTime *event_end,
				    RecurData  *recur_data,
				    CalObjTime *interval_start,
				    CalObjTime *interval_end,
				    CalObjTime *cotime)
{
	GDate event_start_date, interval_start_date;
	guint32 event_start_julian, interval_start_julian, days;

	if (interval_end && cal_obj_time_compare (event_start, interval_end,
						  CALOBJ_DAY) > 0)
		return TRUE;
	if (event_end && cal_obj_time_compare (event_end, interval_start,
					       CALOBJ_DAY) < 0)
		return TRUE;

	*cotime = *event_start;

	/* Convert the event start and interval start to GDates, so we can
	   easily find the number of days between them. */
	g_date_clear (&event_start_date, 1);
	g_date_set_dmy (&event_start_date, event_start->day,
			event_start->month + 1, event_start->year);
	g_date_clear (&interval_start_date, 1);
	g_date_set_dmy (&interval_start_date, interval_start->day,
			interval_start->month + 1, interval_start->year);

	event_start_julian = g_date_julian (&event_start_date);
	interval_start_julian = g_date_julian (&interval_start_date);

	if (event_start_julian < interval_start_julian) {
		days = interval_start_julian - event_start_julian
			+ recur_data->recur->interval - 1;
		days -= days % recur_data->recur->interval;
		cal_obj_time_add_days (cotime, days);
	}

	if (event_end && cal_obj_time_compare (cotime, event_end,
					       CALOBJ_DAY) > 0)
		return TRUE;
	if (interval_end && cal_obj_time_compare (cotime, interval_end,
						  CALOBJ_DAY) > 0)
		return TRUE;

	return FALSE;
}


static gboolean
cal_obj_daily_find_next_position  (CalObjTime *cotime,
				   CalObjTime *event_end,
				   RecurData  *recur_data,
				   CalObjTime *interval_end)
{
	cal_obj_time_add_days (cotime, recur_data->recur->interval);

	if (event_end && cal_obj_time_compare (cotime, event_end,
					       CALOBJ_DAY) > 0)
		return TRUE;
	if (interval_end && cal_obj_time_compare (cotime, interval_end,
						  CALOBJ_DAY) > 0)
		return TRUE;

	return FALSE;
}


static gboolean
cal_obj_hourly_find_start_position (CalObjTime *event_start,
				    CalObjTime *event_end,
				    RecurData  *recur_data,
				    CalObjTime *interval_start,
				    CalObjTime *interval_end,
				    CalObjTime *cotime)
{
	GDate event_start_date, interval_start_date;
	guint32 event_start_julian, interval_start_julian, hours;

	if (interval_end && cal_obj_time_compare (event_start, interval_end,
						  CALOBJ_HOUR) > 0)
		return TRUE;
	if (event_end && cal_obj_time_compare (event_end, interval_start,
					       CALOBJ_HOUR) < 0)
		return TRUE;

	*cotime = *event_start;

	if (cal_obj_time_compare (event_start, interval_start,
				  CALOBJ_HOUR) < 0) {
		/* Convert the event start and interval start to GDates, so we
		   can easily find the number of days between them. */
		g_date_clear (&event_start_date, 1);
		g_date_set_dmy (&event_start_date, event_start->day,
				event_start->month + 1, event_start->year);
		g_date_clear (&interval_start_date, 1);
		g_date_set_dmy (&interval_start_date, interval_start->day,
				interval_start->month + 1,
				interval_start->year);

		event_start_julian = g_date_julian (&event_start_date);
		interval_start_julian = g_date_julian (&interval_start_date);

		hours = (interval_start_julian - event_start_julian) * 24;
		hours += interval_start->hour - event_start->hour;
		hours += recur_data->recur->interval - 1;
		hours -= hours % recur_data->recur->interval;
		cal_obj_time_add_hours (cotime, hours);
	}

	if (event_end && cal_obj_time_compare (cotime, event_end,
					       CALOBJ_HOUR) > 0)
		return TRUE;
	if (interval_end && cal_obj_time_compare (cotime, interval_end,
						  CALOBJ_HOUR) > 0)
		return TRUE;

	return FALSE;
}


static gboolean
cal_obj_hourly_find_next_position (CalObjTime *cotime,
				   CalObjTime *event_end,
				   RecurData  *recur_data,
				   CalObjTime *interval_end)
{
	cal_obj_time_add_hours (cotime, recur_data->recur->interval);

	if (event_end && cal_obj_time_compare (cotime, event_end,
					       CALOBJ_HOUR) > 0)
		return TRUE;
	if (interval_end && cal_obj_time_compare (cotime, interval_end,
						  CALOBJ_HOUR) > 0)
		return TRUE;

	return FALSE;
}


static gboolean
cal_obj_minutely_find_start_position (CalObjTime *event_start,
				      CalObjTime *event_end,
				      RecurData  *recur_data,
				      CalObjTime *interval_start,
				      CalObjTime *interval_end,
				      CalObjTime *cotime)
{
	GDate event_start_date, interval_start_date;
	guint32 event_start_julian, interval_start_julian, minutes;

	if (interval_end && cal_obj_time_compare (event_start, interval_end,
						  CALOBJ_MINUTE) > 0)
		return TRUE;
	if (event_end && cal_obj_time_compare (event_end, interval_start,
					       CALOBJ_MINUTE) < 0)
		return TRUE;

	*cotime = *event_start;

	if (cal_obj_time_compare (event_start, interval_start,
				  CALOBJ_MINUTE) < 0) {
		/* Convert the event start and interval start to GDates, so we
		   can easily find the number of days between them. */
		g_date_clear (&event_start_date, 1);
		g_date_set_dmy (&event_start_date, event_start->day,
				event_start->month + 1, event_start->year);
		g_date_clear (&interval_start_date, 1);
		g_date_set_dmy (&interval_start_date, interval_start->day,
				interval_start->month + 1,
				interval_start->year);

		event_start_julian = g_date_julian (&event_start_date);
		interval_start_julian = g_date_julian (&interval_start_date);

		minutes = (interval_start_julian - event_start_julian)
			* 24 * 60;
		minutes += (interval_start->hour - event_start->hour) * 24;
		minutes += interval_start->minute - event_start->minute;
		minutes += recur_data->recur->interval - 1;
		minutes -= minutes % recur_data->recur->interval;
		cal_obj_time_add_minutes (cotime, minutes);
	}

	if (event_end && cal_obj_time_compare (cotime, event_end,
					       CALOBJ_MINUTE) > 0)
		return TRUE;
	if (interval_end && cal_obj_time_compare (cotime, interval_end,
						  CALOBJ_MINUTE) > 0)
		return TRUE;

	return FALSE;
}


static gboolean
cal_obj_minutely_find_next_position (CalObjTime *cotime,
				     CalObjTime *event_end,
				     RecurData  *recur_data,
				     CalObjTime *interval_end)
{
	cal_obj_time_add_minutes (cotime, recur_data->recur->interval);

	if (event_end && cal_obj_time_compare (cotime, event_end,
					       CALOBJ_MINUTE) > 0)
		return TRUE;
	if (interval_end && cal_obj_time_compare (cotime, interval_end,
						  CALOBJ_MINUTE) > 0)
		return TRUE;

	return FALSE;
}


static gboolean
cal_obj_secondly_find_start_position (CalObjTime *event_start,
				      CalObjTime *event_end,
				      RecurData  *recur_data,
				      CalObjTime *interval_start,
				      CalObjTime *interval_end,
				      CalObjTime *cotime)
{
	GDate event_start_date, interval_start_date;
	guint32 event_start_julian, interval_start_julian, seconds;

	if (interval_end && cal_obj_time_compare (event_start, interval_end,
						  CALOBJ_SECOND) > 0)
		return TRUE;
	if (event_end && cal_obj_time_compare (event_end, interval_start,
					       CALOBJ_SECOND) < 0)
		return TRUE;

	*cotime = *event_start;

	if (cal_obj_time_compare (event_start, interval_start,
				  CALOBJ_SECOND) < 0) {
		/* Convert the event start and interval start to GDates, so we
		   can easily find the number of days between them. */
		g_date_clear (&event_start_date, 1);
		g_date_set_dmy (&event_start_date, event_start->day,
				event_start->month + 1, event_start->year);
		g_date_clear (&interval_start_date, 1);
		g_date_set_dmy (&interval_start_date, interval_start->day,
				interval_start->month + 1,
				interval_start->year);

		event_start_julian = g_date_julian (&event_start_date);
		interval_start_julian = g_date_julian (&interval_start_date);

		seconds = (interval_start_julian - event_start_julian)
			* 24 * 60 * 60;
		seconds += (interval_start->hour - event_start->hour)
			* 24 * 60;
		seconds += (interval_start->minute - event_start->minute) * 60;
		seconds += interval_start->second - event_start->second;
		seconds += recur_data->recur->interval - 1;
		seconds -= seconds % recur_data->recur->interval;
		cal_obj_time_add_seconds (cotime, seconds);
	}

	if (event_end && cal_obj_time_compare (cotime, event_end,
					       CALOBJ_SECOND) >= 0)
		return TRUE;
	if (interval_end && cal_obj_time_compare (cotime, interval_end,
						  CALOBJ_SECOND) >= 0)
		return TRUE;

	return FALSE;
}


static gboolean
cal_obj_secondly_find_next_position (CalObjTime *cotime,
				     CalObjTime *event_end,
				     RecurData  *recur_data,
				     CalObjTime *interval_end)
{
	cal_obj_time_add_seconds (cotime, recur_data->recur->interval);

	if (event_end && cal_obj_time_compare (cotime, event_end,
					       CALOBJ_SECOND) >= 0)
		return TRUE;
	if (interval_end && cal_obj_time_compare (cotime, interval_end,
						  CALOBJ_SECOND) >= 0)
		return TRUE;

	return FALSE;
}





/* If the BYMONTH rule is specified it expands each occurrence in occs, by
   using each of the months in the bymonth list. */
static GArray*
cal_obj_bymonth_expand		(RecurData  *recur_data,
				 GArray     *occs)
{
	GArray *new_occs;
	CalObjTime *occ;
	GList *elem;
	gint len, i;

	/* If BYMONTH has not been specified, or the array is empty, just
	   return the array. */
	if (!recur_data->recur->bymonth || occs->len == 0)
		return occs;

	new_occs = g_array_new (FALSE, FALSE, sizeof (CalObjTime));

	len = occs->len;
	for (i = 0; i < len; i++) {
		occ = &g_array_index (occs, CalObjTime, i);

		elem = recur_data->recur->bymonth;
		while (elem) {
			/* NOTE: The day may now be invalid, e.g. 31st Feb. */
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
cal_obj_bymonth_filter		(RecurData  *recur_data,
				 GArray     *occs)
{
	GArray *new_occs;
	CalObjTime *occ;
	gint len, i;

	/* If BYMONTH has not been specified, or the array is empty, just
	   return the array. */
	if (!recur_data->recur->bymonth || occs->len == 0)
		return occs;

	new_occs = g_array_new (FALSE, FALSE, sizeof (CalObjTime));

	len = occs->len;
	for (i = 0; i < len; i++) {
		occ = &g_array_index (occs, CalObjTime, i);
		if (recur_data->months[occ->month])
			g_array_append_vals (new_occs, occ, 1);
	}

	g_array_free (occs, TRUE);

	return new_occs;
}



static GArray*
cal_obj_byweekno_expand		(RecurData  *recur_data,
				 GArray     *occs)
{
	GArray *new_occs;
	CalObjTime *occ, year_start_cotime, year_end_cotime, cotime;
	GList *elem;
	gint len, i, weekno;

	/* If BYWEEKNO has not been specified, or the array is empty, just
	   return the array. */
	if (!recur_data->recur->byweekno || occs->len == 0)
		return occs;

	new_occs = g_array_new (FALSE, FALSE, sizeof (CalObjTime));

	len = occs->len;
	for (i = 0; i < len; i++) {
		occ = &g_array_index (occs, CalObjTime, i);

		/* Find the day that would correspond to week 1 (note that
		   week 1 is the first week starting from the specified week
		   start day that has 4 days in the new year). */
		year_start_cotime = *occ;
		cal_obj_time_find_first_week (&year_start_cotime,
					      recur_data);

		/* Find the day that would correspond to week 1 of the next
		   year, which we use for -ve week numbers. */
		year_end_cotime = *occ;
		year_end_cotime.year++;
		cal_obj_time_find_first_week (&year_end_cotime,
					      recur_data);

		/* Now iterate over the week numbers in byweekno, generating a
		   new occurrence for each one. */
		elem = recur_data->recur->byweekno;
		while (elem) {
			weekno = GPOINTER_TO_INT (elem->data);
			/* FIXME: Skip occurrences outside the year? */
			if (weekno > 0) {
				cotime = year_start_cotime;
				cal_obj_time_add_days (&cotime,
						       (weekno - 1) * 7);
			} else {
				cotime = year_end_cotime;
				cal_obj_time_subtract_days (&cotime,
							    -weekno * 7);
			}

			g_array_append_val (new_occs, cotime);
			elem = elem->next;
		}
	}

	g_array_free (occs, TRUE);

	return new_occs;
}


#if 0
/* This isn't used at present. */
static GArray*
cal_obj_byweekno_filter		(RecurData  *recur_data,
				 GArray     *occs)
{

	return occs;
}
#endif


static GArray*
cal_obj_byyearday_expand	(RecurData  *recur_data,
				 GArray     *occs)
{
	GArray *new_occs;
	CalObjTime *occ, year_start_cotime, year_end_cotime, cotime;
	GList *elem;
	gint len, i, dayno;

	/* If BYYEARDAY has not been specified, or the array is empty, just
	   return the array. */
	if (!recur_data->recur->byyearday || occs->len == 0)
		return occs;

	new_occs = g_array_new (FALSE, FALSE, sizeof (CalObjTime));

	len = occs->len;
	for (i = 0; i < len; i++) {
		occ = &g_array_index (occs, CalObjTime, i);

		/* Find the day that would correspond to day 1. */
		year_start_cotime = *occ;
		year_start_cotime.month = 0;
		year_start_cotime.day = 1;

		/* Find the day that would correspond to day 1 of the next
		   year, which we use for -ve day numbers. */
		year_end_cotime = *occ;
		year_end_cotime.year++;
		year_end_cotime.month = 0;
		year_end_cotime.day = 1;

		/* Now iterate over the day numbers in byyearday, generating a
		   new occurrence for each one. */
		elem = recur_data->recur->byyearday;
		while (elem) {
			dayno = GPOINTER_TO_INT (elem->data);
			if (dayno > 0) {
				cotime = year_start_cotime;
				cal_obj_time_add_days (&cotime, dayno - 1);
			} else {
				cotime = year_end_cotime;
				cal_obj_time_subtract_days (&cotime, -dayno);
			}

			/* Skip occurrences if they fall outside the year. */
			if (cotime.year == occ->year)
				g_array_append_val (new_occs, cotime);
			elem = elem->next;
		}
	}

	g_array_free (occs, TRUE);

	return new_occs;
}


/* Note: occs must not contain invalid dates, e.g. 31st September. */
static GArray*
cal_obj_byyearday_filter	(RecurData  *recur_data,
				 GArray     *occs)
{
	GArray *new_occs;
	CalObjTime *occ;
	gint yearday, len, i, days_in_year;

	/* If BYYEARDAY has not been specified, or the array is empty, just
	   return the array. */
	if (!recur_data->recur->byyearday || occs->len == 0)
		return occs;

	new_occs = g_array_new (FALSE, FALSE, sizeof (CalObjTime));

	len = occs->len;
	for (i = 0; i < len; i++) {
		occ = &g_array_index (occs, CalObjTime, i);
		yearday = cal_obj_time_day_of_year (occ);
		if (recur_data->yeardays[yearday]) {
			g_array_append_vals (new_occs, occ, 1);
		} else {
			days_in_year = g_date_is_leap_year (occ->year)
				? 366 : 365;
			if (recur_data->neg_yeardays[days_in_year + 1
						    - yearday])
				g_array_append_vals (new_occs, occ, 1);
		}
	}

	g_array_free (occs, TRUE);

	return new_occs;
}



static GArray*
cal_obj_bymonthday_expand	(RecurData  *recur_data,
				 GArray     *occs)
{
	GArray *new_occs;
	CalObjTime *occ, month_start_cotime, month_end_cotime, cotime;
	GList *elem;
	gint len, i, dayno;

	/* If BYMONTHDAY has not been specified, or the array is empty, just
	   return the array. */
	if (!recur_data->recur->bymonthday || occs->len == 0)
		return occs;

	new_occs = g_array_new (FALSE, FALSE, sizeof (CalObjTime));

	len = occs->len;
	for (i = 0; i < len; i++) {
		occ = &g_array_index (occs, CalObjTime, i);

		/* Find the day that would correspond to day 1. */
		month_start_cotime = *occ;
		month_start_cotime.day = 1;

		/* Find the day that would correspond to day 1 of the next
		   month, which we use for -ve day numbers. */
		month_end_cotime = *occ;
		month_end_cotime.month++;
		month_end_cotime.day = 1;

		/* Now iterate over the day numbers in bymonthday, generating a
		   new occurrence for each one. */
		elem = recur_data->recur->bymonthday;
		while (elem) {
			dayno = GPOINTER_TO_INT (elem->data);
			if (dayno > 0) {
				cotime = month_start_cotime;
				cal_obj_time_add_days (&cotime, dayno - 1);
			} else {
				cotime = month_end_cotime;
				cal_obj_time_subtract_days (&cotime, -dayno);
			}

			/* Skip occurrences if they fall outside the month. */
			if (cotime.month == occ->month)
				g_array_append_val (new_occs, cotime);
			elem = elem->next;
		}
	}

	g_array_free (occs, TRUE);

	return new_occs;
}


static GArray*
cal_obj_bymonthday_filter	(RecurData  *recur_data,
				 GArray     *occs)
{
	GArray *new_occs;
	CalObjTime *occ;
	gint len, i, days_in_month;

	/* If BYMONTHDAY has not been specified, or the array is empty, just
	   return the array. */
	if (!recur_data->recur->bymonthday || occs->len == 0)
		return occs;

	new_occs = g_array_new (FALSE, FALSE, sizeof (CalObjTime));

	len = occs->len;
	for (i = 0; i < len; i++) {
		occ = &g_array_index (occs, CalObjTime, i);
		if (recur_data->monthdays[occ->day]) {
			g_array_append_vals (new_occs, occ, 1);
		} else {
			days_in_month = time_days_in_month (occ->year,
							    occ->month);
			if (recur_data->neg_monthdays[days_in_month + 1
						     - occ->day])
				g_array_append_vals (new_occs, occ, 1);
		}
	}

	g_array_free (occs, TRUE);

	return new_occs;
}



static GArray*
cal_obj_byday_expand_yearly	(RecurData  *recur_data,
				 GArray     *occs)
{
	GArray *new_occs;
	CalObjTime *occ;
	GList *elem;
	gint len, i, weekday, week_num;
	gint first_weekday, last_weekday, offset;
	guint16 year;

	/* If BYDAY has not been specified, or the array is empty, just
	   return the array. */
	if (!recur_data->recur->byday || occs->len == 0)
		return occs;

	new_occs = g_array_new (FALSE, FALSE, sizeof (CalObjTime));

	len = occs->len;
	for (i = 0; i < len; i++) {
		occ = &g_array_index (occs, CalObjTime, i);

		elem = recur_data->recur->byday;
		while (elem) {
			weekday = GPOINTER_TO_INT (elem->data);
			elem = elem->next;
			week_num = GPOINTER_TO_INT (elem->data);
			elem = elem->next;

			year = occ->year;
			if (week_num == 0) {
				occ->month = 0;
				occ->day = 1;
				first_weekday = cal_obj_time_weekday (occ, recur_data->recur);
				offset = (weekday + 7 - first_weekday) % 7;
				cal_obj_time_add_days (occ, offset);

				while (occ->year == year) {
					g_array_append_vals (new_occs, occ, 1);
					cal_obj_time_add_days (occ, 7);
				}

			} else if (week_num > 0) {
				occ->month = 0;
				occ->day = 1;
				first_weekday = cal_obj_time_weekday (occ, recur_data->recur);
				offset = (weekday + 7 - first_weekday) % 7;
				offset += (week_num - 1) * 7;
				cal_obj_time_add_days (occ, offset);
				if (occ->year == year)
					g_array_append_vals (new_occs, occ, 1);

			} else {
				occ->month = 11;
				occ->day = 31;
				last_weekday = cal_obj_time_weekday (occ, recur_data->recur);
				offset = (last_weekday + 7 - weekday) % 7;
				offset += (week_num - 1) * 7;
				cal_obj_time_subtract_days (occ, offset);
				if (occ->year == year)
					g_array_append_vals (new_occs, occ, 1);
			}
		}
	}

	g_array_free (occs, TRUE);

	return new_occs;
}


static GArray*
cal_obj_byday_expand_monthly	(RecurData  *recur_data,
				 GArray     *occs)
{
	GArray *new_occs;
	CalObjTime *occ;
	GList *elem;
	gint len, i, weekday, week_num;
	gint first_weekday, last_weekday, offset;
	guint16 year;
	guint8 month;

	/* If BYDAY has not been specified, or the array is empty, just
	   return the array. */
	if (!recur_data->recur->byday || occs->len == 0)
		return occs;

	new_occs = g_array_new (FALSE, FALSE, sizeof (CalObjTime));

	len = occs->len;
	for (i = 0; i < len; i++) {
		occ = &g_array_index (occs, CalObjTime, i);

		elem = recur_data->recur->byday;
		while (elem) {
			weekday = GPOINTER_TO_INT (elem->data);
			elem = elem->next;
			week_num = GPOINTER_TO_INT (elem->data);
			elem = elem->next;

			year = occ->year;
			month = occ->month;
			if (week_num == 0) {
				occ->day = 1;
				first_weekday = cal_obj_time_weekday (occ, recur_data->recur);
				offset = (weekday + 7 - first_weekday) % 7;
				cal_obj_time_add_days (occ, offset);
				
				while (occ->year == year
				       && occ->month == month) {
					g_array_append_vals (new_occs, occ, 1);
					cal_obj_time_add_days (occ, 7);
				}
				
			} else if (week_num > 0) {
				occ->day = 1;
				first_weekday = cal_obj_time_weekday (occ, recur_data->recur);
				offset = (weekday + 7 - first_weekday) % 7;
				offset += (week_num - 1) * 7;
				cal_obj_time_add_days (occ, offset);
				if (occ->year == year && occ->month == month)
					g_array_append_vals (new_occs, occ, 1);

			} else {
				occ->day = time_days_in_month (occ->year,
							       occ->month);
				last_weekday = cal_obj_time_weekday (occ, recur_data->recur);
				offset = (last_weekday + 7 - weekday) % 7;
				offset += (week_num - 1) * 7;
				cal_obj_time_subtract_days (occ, offset);
				if (occ->year == year && occ->month == month)
					g_array_append_vals (new_occs, occ, 1);
			}
		}
	}

	g_array_free (occs, TRUE);

	return new_occs;
}


/* Note: occs must not contain invalid dates, e.g. 31st September. */
static GArray*
cal_obj_byday_expand_weekly	(RecurData  *recur_data,
				 GArray     *occs)
{
	GArray *new_occs;
	CalObjTime *occ;
	GList *elem;
	gint len, i, weekday, week_num;
	gint current_weekday;
	gint day_of_week, new_day_of_week, days_to_add;

	/* If BYDAY has not been specified, or the array is empty, just
	   return the array. */
	if (!recur_data->recur->byday || occs->len == 0)
		return occs;

	new_occs = g_array_new (FALSE, FALSE, sizeof (CalObjTime));

	len = occs->len;
	for (i = 0; i < len; i++) {
		occ = &g_array_index (occs, CalObjTime, i);

		elem = recur_data->recur->byday;
		while (elem) {
			weekday = GPOINTER_TO_INT (elem->data);
			elem = elem->next;
			week_num = GPOINTER_TO_INT (elem->data);
			elem = elem->next;

			current_weekday = cal_obj_time_weekday (occ, recur_data->recur);
			day_of_week = (current_weekday + 7
				       - recur_data->recur->week_start_day) % 7;
			new_day_of_week = (weekday + 7
					   - recur_data->recur->week_start_day) % 7;
			days_to_add = new_day_of_week - day_of_week;
			if (days_to_add > 0)
				cal_obj_time_add_days (occ, days_to_add);
			else if (days_to_add < 0)
				cal_obj_time_subtract_days (occ, -days_to_add);
			g_array_append_vals (new_occs, occ, 1);
		}
	}

	g_array_free (occs, TRUE);

	return new_occs;
}


/* Note: occs must not contain invalid dates, e.g. 31st September. */
static GArray*
cal_obj_byday_filter		(RecurData  *recur_data,
				 GArray     *occs)
{
	GArray *new_occs;
	CalObjTime *occ;
	gint len, i, weekday;

	/* If BYDAY has not been specified, or the array is empty, just
	   return the array. */
	if (!recur_data->recur->byday || occs->len == 0)
		return occs;

	new_occs = g_array_new (FALSE, FALSE, sizeof (CalObjTime));

	len = occs->len;
	for (i = 0; i < len; i++) {
		occ = &g_array_index (occs, CalObjTime, i);
		weekday = cal_obj_time_weekday (occ, recur_data->recur);

		/* See if the weekday on its own is set. */
		if (recur_data->weekdays[weekday])
			g_array_append_vals (new_occs, occ, 1);
	}

	g_array_free (occs, TRUE);

	return new_occs;
}



/* If the BYHOUR rule is specified it expands each occurrence in occs, by
   using each of the hours in the byhour list. */
static GArray*
cal_obj_byhour_expand		(RecurData  *recur_data,
				 GArray     *occs)
{
	GArray *new_occs;
	CalObjTime *occ;
	GList *elem;
	gint len, i;

	/* If BYHOUR has not been specified, or the array is empty, just
	   return the array. */
	if (!recur_data->recur->byhour || occs->len == 0)
		return occs;

	new_occs = g_array_new (FALSE, FALSE, sizeof (CalObjTime));

	len = occs->len;
	for (i = 0; i < len; i++) {
		occ = &g_array_index (occs, CalObjTime, i);

		elem = recur_data->recur->byhour;
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
cal_obj_byhour_filter		(RecurData  *recur_data,
				 GArray     *occs)
{
	GArray *new_occs;
	CalObjTime *occ;
	gint len, i;

	/* If BYHOUR has not been specified, or the array is empty, just
	   return the array. */
	if (!recur_data->recur->byhour || occs->len == 0)
		return occs;

	new_occs = g_array_new (FALSE, FALSE, sizeof (CalObjTime));

	len = occs->len;
	for (i = 0; i < len; i++) {
		occ = &g_array_index (occs, CalObjTime, i);
		if (recur_data->hours[occ->hour])
			g_array_append_vals (new_occs, occ, 1);
	}

	g_array_free (occs, TRUE);

	return new_occs;
}



/* If the BYMINUTE rule is specified it expands each occurrence in occs, by
   using each of the minutes in the byminute list. */
static GArray*
cal_obj_byminute_expand		(RecurData  *recur_data,
				 GArray     *occs)
{
	GArray *new_occs;
	CalObjTime *occ;
	GList *elem;
	gint len, i;

	/* If BYMINUTE has not been specified, or the array is empty, just
	   return the array. */
	if (!recur_data->recur->byminute || occs->len == 0)
		return occs;

	new_occs = g_array_new (FALSE, FALSE, sizeof (CalObjTime));

	len = occs->len;
	for (i = 0; i < len; i++) {
		occ = &g_array_index (occs, CalObjTime, i);

		elem = recur_data->recur->byminute;
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
cal_obj_byminute_filter		(RecurData  *recur_data,
				 GArray     *occs)
{
	GArray *new_occs;
	CalObjTime *occ;
	gint len, i;

	/* If BYMINUTE has not been specified, or the array is empty, just
	   return the array. */
	if (!recur_data->recur->byminute || occs->len == 0)
		return occs;

	new_occs = g_array_new (FALSE, FALSE, sizeof (CalObjTime));

	len = occs->len;
	for (i = 0; i < len; i++) {
		occ = &g_array_index (occs, CalObjTime, i);
		if (recur_data->minutes[occ->minute])
			g_array_append_vals (new_occs, occ, 1);
	}

	g_array_free (occs, TRUE);

	return new_occs;
}



/* If the BYSECOND rule is specified it expands each occurrence in occs, by
   using each of the seconds in the bysecond list. */
static GArray*
cal_obj_bysecond_expand		(RecurData  *recur_data,
				 GArray     *occs)
{
	GArray *new_occs;
	CalObjTime *occ;
	GList *elem;
	gint len, i;

	/* If BYSECOND has not been specified, or the array is empty, just
	   return the array. */
	if (!recur_data->recur->bysecond || occs->len == 0)
		return occs;

	new_occs = g_array_new (FALSE, FALSE, sizeof (CalObjTime));

	len = occs->len;
	for (i = 0; i < len; i++) {
		occ = &g_array_index (occs, CalObjTime, i);

		elem = recur_data->recur->bysecond;
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
cal_obj_bysecond_filter		(RecurData  *recur_data,
				 GArray     *occs)
{
	GArray *new_occs;
	CalObjTime *occ;
	gint len, i;

	/* If BYSECOND has not been specified, or the array is empty, just
	   return the array. */
	if (!recur_data->recur->bysecond || occs->len == 0)
		return occs;

	new_occs = g_array_new (FALSE, FALSE, sizeof (CalObjTime));

	len = occs->len;
	for (i = 0; i < len; i++) {
		occ = &g_array_index (occs, CalObjTime, i);
		if (recur_data->seconds[occ->second])
			g_array_append_vals (new_occs, occ, 1);
	}

	g_array_free (occs, TRUE);

	return new_occs;
}





/* Adds a positive number of months to the given CalObjTime, updating the year
   appropriately so we end up with a valid month. Note that the day may be
   invalid. */
static void
cal_obj_time_add_months		(CalObjTime *cotime,
				 gint	     months)
{
	guint month;

	/* We use a guint to avoid overflow on the guint8. */
	month = cotime->month + months;
	cotime->year += month / 12;
	cotime->month = month % 12;
}


/* Adds a positive number of days to the given CalObjTime, updating the month
   and year appropriately so we end up with a valid day. */
static void
cal_obj_time_add_days		(CalObjTime *cotime,
				 gint	     days)
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


/* Subtracts a positive number of days from the given CalObjTime, updating the
   month and year appropriately so we end up with a valid day. */
static void
cal_obj_time_subtract_days	(CalObjTime *cotime,
				 gint	     days)
{
	gint day, days_in_month;

	/* We use a gint to avoid overflow on the guint8. */
	day = (gint) cotime->day;
	day -= days;

	while (day <= 0) {
		if (cotime->month == 0) {
			cotime->year--;
			cotime->month = 11;
		} else {
			cotime->month--;
		}

		days_in_month = time_days_in_month (cotime->year,
						    cotime->month);

		day += days_in_month;
	}

	cotime->day = (guint8) day;
}


/* Adds a positive number of hours to the given CalObjTime, updating the day,
   month & year appropriately so we end up with a valid time. */
static void
cal_obj_time_add_hours		(CalObjTime *cotime,
				 gint	     hours)
{
	guint hour;

	/* We use a guint to avoid overflow on the guint8. */
	hour = cotime->hour + hours;
	cotime->hour = hour % 24;
	if (hour > 24)
		cal_obj_time_add_days (cotime, hour / 24);
}


/* Adds a positive number of minutes to the given CalObjTime, updating the
   rest of the CalObjTime appropriately. */
static void
cal_obj_time_add_minutes	(CalObjTime *cotime,
				 gint	     minutes)
{
	guint minute;

	/* We use a guint to avoid overflow on the guint8. */
	minute = cotime->minute + minutes;
	cotime->minute = minute % 60;
	if (minute > 60)
		cal_obj_time_add_hours (cotime, minute / 60);
}


/* Adds a positive number of seconds to the given CalObjTime, updating the
   rest of the CalObjTime appropriately. */
static void
cal_obj_time_add_seconds	(CalObjTime *cotime,
				 gint	     seconds)
{
	guint second;

	/* We use a guint to avoid overflow on the guint8. */
	second = cotime->second + seconds;
	cotime->second = second % 60;
	if (second > 60)
		cal_obj_time_add_minutes (cotime, second / 60);
}


/* Compares 2 CalObjTimes. Returns -1 if the cotime1 is before cotime2, 0 if
   they are the same, or 1 if cotime1 is after cotime2. The comparison type
   specifies which parts of the times we are interested in, e.g. if CALOBJ_DAY
   is used we only want to know if the days are different. */
static gint
cal_obj_time_compare		(CalObjTime *cotime1,
				 CalObjTime *cotime2,
				 CalObjTimeComparison type)
{
	if (cotime1->year < cotime2->year)
		return -1;
	if (cotime1->year > cotime2->year)
		return 1;

	if (type == CALOBJ_YEAR)
		return 0;

	if (cotime1->month < cotime2->month)
		return -1;
	if (cotime1->month > cotime2->month)
		return 1;

	if (type == CALOBJ_MONTH)
		return 0;

	if (cotime1->day < cotime2->day)
		return -1;
	if (cotime1->day > cotime2->day)
		return 1;

	if (type == CALOBJ_DAY)
		return 0;

	if (cotime1->hour < cotime2->hour)
		return -1;
	if (cotime1->hour > cotime2->hour)
		return 1;

	if (type == CALOBJ_HOUR)
		return 0;

	if (cotime1->minute < cotime2->minute)
		return -1;
	if (cotime1->minute > cotime2->minute)
		return 1;

	if (type == CALOBJ_MINUTE)
		return 0;

	if (cotime1->second < cotime2->second)
		return -1;
	if (cotime1->second > cotime2->second)
		return 1;

	return 0;
}


/* This is the same as the above function, but without the comparison type.
   It is used for qsort(). */
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


/* Returns the weekday of the given CalObjTime, from 0 - 6. The week start
   day is Monday by default, but can be set in the recurrence rule. */
static gint
cal_obj_time_weekday		(CalObjTime *cotime,
				 CalObjRecurrence *recur)
{
	GDate date;
	gint weekday;

	g_date_clear (&date, 1);
	g_date_set_dmy (&date, cotime->day, cotime->month + 1, cotime->year);

	/* This results in a value of 0 (Monday) - 6 (Sunday). */
	weekday = g_date_weekday (&date) - 1;

	/* This calculates the offset of our day from the start of the week.
	   We just add on a week (to avoid any possible negative values) and
	   then subtract the specified week start day, then convert it into a
	   value from 0-6. */
	weekday = (weekday + 7 - recur->week_start_day) % 7;

	return weekday;
}


/* Returns the day of the year of the given CalObjTime, from 1 - 366. */
static gint
cal_obj_time_day_of_year		(CalObjTime *cotime)
{
	GDate date;

	g_date_clear (&date, 1);
	g_date_set_dmy (&date, cotime->day, cotime->month + 1, cotime->year);

	return g_date_day_of_year (&date);
}


/* Finds the first week in the given CalObjTime's year, using the same weekday
   as the event start day (i.e. from the RecurData).
   The first week of the year is the first week starting from the specified
   week start day that has 4 days in the new year. It may be in the previous
   year. */
static void
cal_obj_time_find_first_week	(CalObjTime *cotime,
				 RecurData  *recur_data)
{
	GDate date;
	gint weekday, week_start_day, offset;

	/* Find out the weekday of the 1st of the year. */
	g_date_clear (&date, 1);
	g_date_set_dmy (&date, 1, 1, cotime->year);

	/* This results in a value of 0 (Monday) - 6 (Sunday). */
	weekday = g_date_weekday (&date) - 1;

	/* Calculate the first day of the year that starts a new week. */
	week_start_day = recur_data->recur->week_start_day;
	offset = (week_start_day + 7 - weekday) % 7;

	/* Now see if we have to move backwards 1 week, i.e. if the week
	   starts on or after Jan 5th (since the previous week has 4 days in
	   this year and so will be the first week of the year). */
	if (offset >= 4)
		offset -= 7;

	/* Now move to the required day. */
	offset += (recur_data->weekday + 7 - week_start_day) % 7;

	/* Now move the cotime to the appropriate day. */
	cotime->month = 0;
	cotime->day = 1;
	if (offset > 0)
		cal_obj_time_add_days (cotime, offset);
	else
		cal_obj_time_subtract_days (cotime, offset);
}


