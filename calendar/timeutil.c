/* Miscellaneous time-related utilities
 *
 * Copyright (C) 1998 The Free Software Foundation
 *
 * Authors: Federico Mena <federico@nuclecu.unam.mx>
 *          Miguel de Icaza <miguel@nuclecu.unam.mx>
 */

#include <libgnome/libgnome.h>
#include "timeutil.h"

#define digit_at(x,y) (x [y] - '0')
	
time_t
time_from_isodate (char *str)
{
	struct tm my_tm;
	time_t t;

	my_tm.tm_year = (digit_at (str, 0) * 1000 + digit_at (str, 1) * 100 +
		digit_at (str, 2) * 10 + digit_at (str, 3)) - 1900;

	my_tm.tm_mon  = digit_at (str, 4) * 10 + digit_at (str, 5) - 1;
	my_tm.tm_mday = digit_at (str, 6) * 10 + digit_at (str, 7);
	my_tm.tm_hour = digit_at (str, 9) * 10 + digit_at (str, 10);
	my_tm.tm_min  = digit_at (str, 11) * 10 + digit_at (str, 12);
	my_tm.tm_sec  = digit_at (str, 13) * 10 + digit_at (str, 14);
	my_tm.tm_isdst = -1;
	
	t = mktime (&my_tm);
	return t;
}

void
print_time_t (time_t t)
{
	struct tm *tm = localtime (&t);
	
	printf ("TIEMPO: %d/%d/%d %d:%d:%d\n",
		tm->tm_mon+1, tm->tm_mday, tm->tm_year,
		tm->tm_hour, tm->tm_min, tm->tm_sec);
}

int
get_time_t_hour (time_t t)
{
	struct tm *tm;

	tm = localtime (&t);
	return tm->tm_hour;
}

char *
isodate_from_time_t (time_t t)
{
	struct tm *tm;
	static char isotime [40];

	tm = localtime (&t);
	strftime (isotime, sizeof (isotime)-1, "%Y%m%dT%H%M%sZ", tm);
	return isotime;
}

time_t
time_from_start_duration (time_t start, char *duration)
{
	printf ("Not yet implemented\n");
	return 0;
}

char *
format_simple_hour (int hour, int use_am_pm)
{
	static char buf[256];

	/* I don't know whether this is the best way to internationalize it.
	 * Does any language use different conventions? - Federico
	 */

	if (use_am_pm)
		sprintf (buf, "%d%s",
			 (hour == 0) ? 12 : (hour > 12) ? (hour - 12) : hour,
			 (hour < 12) ? _("am") : _("pm"));
	else
		sprintf (buf, "%02d%s", hour, _("h"));

	return buf;

}

time_t
time_add_day (time_t time, int days)
{
	struct tm *tm = localtime (&time);
	time_t new_time;

	tm->tm_mday += days;
	if ((new_time = mktime (tm)) == -1){
		g_warning ("mktime could not handling adding a day with\n");
		print_time_t (time);
		return time;
	}
	return new_time;
}

time_t
time_add_year (time_t time, int years)
{
	struct tm *tm = localtime (&time);
	time_t new_time;
	
	tm->tm_year += years;
	if ((new_time = mktime (tm)) == -1){
		g_warning ("mktime could not handling adding a year with\n");
		print_time_t (time);
		return time;
	}
	return new_time;
}

time_t
time_day_hour (time_t t, int hour)
{
	struct tm tm;
	
	tm = *localtime (&t);
	tm.tm_hour = hour;
	tm.tm_min  = 0;
	tm.tm_sec  = 0;

	return mktime (&tm);
}


time_t
time_start_of_day (time_t t)
{
	struct tm tm;
	
	tm = *localtime (&t);
	tm.tm_hour = 0;
	tm.tm_min  = 0;
	tm.tm_sec  = 0;

	return mktime (&tm);
}

time_t
time_end_of_day (time_t t)
{
	struct tm tm;
	
	tm = *localtime (&t);
	tm.tm_hour = 0;
	tm.tm_min  = 0;
	tm.tm_sec  = 0;
	tm.tm_mday++;
	
	return mktime (&tm);
}

time_t
time_year_begin (int year)
{
	struct tm tm;
	time_t retval;
	
	tm.tm_hour = 0;
	tm.tm_min  = 0;
	tm.tm_sec  = 0;
	tm.tm_year = year;
	tm.tm_mon  = 0;
	tm.tm_mday = 1;
	tm.tm_isdst = -1;
	
	retval = mktime (&tm);
	return retval;
}

time_t
time_year_end (int year)
{
	struct tm tm;
	
	tm.tm_hour = 23;
	tm.tm_min  = 59;
	tm.tm_sec  = 59;
	tm.tm_year = year;
	tm.tm_mon  = 11;
	tm.tm_mday = 31;
	tm.tm_isdst = -1;
	
	return mktime (&tm);
}

time_t
time_week_begin   (time_t t)
{
	struct tm tm;
	time_t retval;

	tm = *localtime (&t);
	tm.tm_mday -= tm.tm_wday;
	return mktime (&tm);
}

