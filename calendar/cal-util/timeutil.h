/* Miscellaneous time-related utilities
 *
 * Copyright (C) 1998 The Free Software Foundation
 * Copyright (C) 2000 Helix Code, Inc.
 * Copyright (C) 2000 Ximian, Inc.
 *
 * Authors: Federico Mena <federico@ximian.com>
 *          Miguel de Icaza <miguel@ximian.com>
 */

#ifndef TIMEUTIL_H
#define TIMEUTIL_H


#include <time.h>
#include <ical.h>


char   *isodate_from_time_t     (time_t t);

time_t time_add_minutes (time_t time, int minutes);
time_t time_add_day (time_t time, int days);
time_t time_add_week (time_t time, int weeks);
time_t time_add_month (time_t time, int months);
time_t time_add_year (time_t time, int years);


/* Returns the number of days in the specified month.  Years are full years (starting from year 1).
 * Months are in [0, 11].
 */
int time_days_in_month (int year, int month);

/* Converts the specified date to a time_t at the start of the specified day.  Years are full years
 * (starting from year 1).  Months are in [0, 11].  Days are 1-based.
 */
time_t time_from_day (int year, int month, int day);

/* For the functions below, time ranges are considered to contain the start time, but not the end
 * time.
 */

/* These two functions take a time value and return the beginning or end of the corresponding year,
 * respectively.
 */
time_t time_year_begin (time_t t);
time_t time_year_end (time_t t);

/* These two functions take a time value and return the beginning or end of the corresponding month,
 * respectively.
 */
time_t time_month_begin (time_t t);
time_t time_month_end (time_t t);

/* These functions take a time value and return the beginning or end of the corresponding week,
 * respectively.  This takes into account the global week_starts_on_monday flag.
 */
time_t time_week_begin (time_t t);
time_t time_week_end (time_t t);

/* These two functions take a time value and return the beginning or end of the corresponding day,
 * respectively.
 */
time_t time_day_begin (time_t t);
time_t time_day_end (time_t t);

void print_time_t (time_t t);


#endif
