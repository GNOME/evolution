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

time_t time_add_week (time_t time, int weeks);
time_t time_add_day (time_t time, int days);
time_t time_add_year (time_t time, int years);


/* Returns pointer to a statically-allocated buffer with a string of the form
 * 3am, 4am, 12pm, 08h, 17h, etc.
 * The string is internationalized, hopefully correctly.
 */
char *format_simple_hour (int hour, int use_am_pm);

time_t time_start_of_day (time_t t);
time_t time_end_of_day   (time_t t);
time_t time_day_hour     (time_t t, int hour);
time_t time_year_begin   (int    year);
time_t time_year_end     (int    year);
time_t time_week_begin   (time_t t);

void print_time_t (time_t t);

#endif
