/* Miscellaneous time-related utilities
 *
 * Copyright (C) 1998 The Free Software Foundation
 *
 * Authors: Federico Mena <federico@nuclecu.unam.mx>
 *          Miguel de Icaza <miguel@nuclecu.unam.mx>
 */

#ifndef TIMEUTIL_H
#define TIMEUTIL_H


#include <time.h>


time_t time_from_isodate        (char *str);
time_t time_from_start_duration (time_t start, char *duration);
char   *isodate_from_time_t     (time_t t);
int    get_time_t_hour          (time_t t);
int    isodiff_to_secs          (char *str);
char   *isodiff_from_secs       (int secs);

time_t time_add_minutes (time_t time, int minutes);
time_t time_add_day (time_t time, int days);
time_t time_add_week (time_t time, int weeks);
time_t time_add_month (time_t time, int months);
time_t time_add_year (time_t time, int years);


/* Returns pointer to a statically-allocated buffer with a string of the form
 * 3am, 4am, 12pm, 08h, 17h, etc.
 * The string is internationalized, hopefully correctly.
 */
char *format_simple_hour (int hour, int use_am_pm);

/* Returns the number of days in the specified month.  Years are full years (starting from year 1).
 * Months are in [0, 11].
 */
int time_days_in_month (int year, int month);

/* Converts the specified date to a time_t at the start of the specified day.  Years are full years
 * (starting from year 1).  Months are in [0, 11].  Days are 1-based.
 */
time_t time_from_day (int year, int month, int day);

time_t time_day_hour     (time_t t, int hour);

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


time_t parse_date (char *str);
void print_time_t (time_t t);


#endif
