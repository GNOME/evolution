/* Miscellaneous time-related utilities
 *
 * Copyright (C) 1998 The Free Software Foundation
 * Copyright (C) 2000 Helix Code, Inc.
 * Copyright (C) 2000 Ximian, Inc.
 *
 * Authors: Federico Mena <federico@ximian.com>
 *          Miguel de Icaza <miguel@ximian.com>
 */

#include <string.h>
#include <ctype.h>
#include <glib.h>
#include "timeutil.h"



void
print_time_t (time_t t)
{
	struct tm *tm = localtime (&t);
	
	printf ("%d/%02d/%02d %02d:%02d:%02d",
		1900 + tm->tm_year, tm->tm_mon+1, tm->tm_mday,
		tm->tm_hour, tm->tm_min, tm->tm_sec);
}

/**
 * isodate_from_time_t:
 * @t: A time value.
 * 
 * Creates an ISO 8601 local time representation from a time value.
 * 
 * Return value: String with the ISO 8601 representation of the local time.
 **/
char *
isodate_from_time_t (time_t t)
{
	struct tm *tm;
	char isotime[40];

	tm = localtime (&t);
	strftime (isotime, sizeof (isotime)-1, "%Y%m%dT%H%M%S", tm);
	return g_strdup (isotime);
}

/**
 * time_from_isodate:
 * @str: Date/time value in ISO 8601 format.
 * 
 * Converts an ISO 8601 time string into a time_t value.
 * 
 * Return value: Time_t corresponding to the specified ISO string.
 **/
time_t
time_from_isodate (const char *str)
{
	int len;
	struct tm my_tm;
	time_t t;
	int i;

	g_return_val_if_fail (str != NULL, -1);

	/* yyyymmdd[Thhmmss[Z]] */

	len = strlen (str);

	if (!(len == 8 || len == 15 || len == 16))
		return -1;

	for (i = 0; i < len; i++)
		if (!((i != 8 && i != 15 && isdigit (str[i]))
		      || (i == 8 && str[i] == 'T')
		      || (i == 15 && str[i] == 'Z')))
			return -1;

	memset (&my_tm, 0, sizeof (my_tm));

#define digit_at(x,y) (x[y] - '0')

	my_tm.tm_year = (digit_at (str, 0) * 1000 + digit_at (str, 1) * 100 +
			 digit_at (str, 2) * 10 + digit_at (str, 3)) - 1900;

	my_tm.tm_mon  = digit_at (str, 4) * 10 + digit_at (str, 5) - 1;
	my_tm.tm_mday = digit_at (str, 6) * 10 + digit_at (str, 7);

	if (len > 8) {
		my_tm.tm_hour = digit_at (str, 9) * 10 + digit_at (str, 10);
		my_tm.tm_min  = digit_at (str, 11) * 10 + digit_at (str, 12);
		my_tm.tm_sec  = digit_at (str, 13) * 10 + digit_at (str, 14);
	}

	my_tm.tm_isdst = -1;

	t = mktime (&my_tm);

	if (len == 16) {
#if defined(HAVE_TM_GMTOFF)
		t += my_tm.tm_gmtoff;
#elif defined(HAVE_TIMEZONE)
		t -= timezone;
#endif
	}
	    
	return t;
}

time_t
time_add_minutes (time_t time, int minutes)
{
	struct tm *tm = localtime (&time);
	time_t new_time;

	tm->tm_min += minutes;
	if ((new_time = mktime (tm)) == -1) {
		g_message ("time_add_minutes(): mktime() could not handle "
			   "adding %d minutes with\n", minutes);
		print_time_t (time);
		printf ("\n");
		return time;
	}
	return new_time;
}

/* Adds a day onto the time, using local time.
   Note that if clocks go forward due to daylight savings time, there are
   some non-existent local times, so the hour may be changed to make it a
   valid time. This also means that it may not be wise to keep calling
   time_add_day() to step through a certain period - if the hour gets changed
   to make it valid time, any further calls to time_add_day() will also return
   this hour, which may not be what you want. */
time_t
time_add_day (time_t time, int days)
{
	struct tm *tm = localtime (&time);
	time_t new_time;
#if 0
	int dst_flag = tm->tm_isdst;
#endif

	tm->tm_mday += days;
	tm->tm_isdst = -1;

	if ((new_time = mktime (tm)) == -1) {
		g_message ("time_add_day(): mktime() could not handling adding %d days with\n",
			   days);
		print_time_t (time);
		printf ("\n");
		return time;
	}

#if 0
	/* I don't know what this is for. See also time_day_begin() and
	   time_day_end(). - Damon. */
	if (dst_flag > tm->tm_isdst) {
		tm->tm_hour++;
		new_time += 3600;
	} else if (dst_flag < tm->tm_isdst) {
		tm->tm_hour--;
		new_time -= 3600;
	}
#endif

	return new_time;
}

time_t
time_add_week (time_t time, int weeks)
{
	return time_add_day (time, weeks * 7);
}

time_t
time_add_month (time_t time, int months)
{
	struct tm *tm = localtime (&time);
	time_t new_time;
	int mday;

	mday = tm->tm_mday;
	
	tm->tm_mon += months;
	tm->tm_isdst = -1;
	if ((new_time = mktime (tm)) == -1) {
		g_message ("time_add_month(): mktime() could not handling adding %d months with\n",
			   months);
		print_time_t (time);
		printf ("\n");
		return time;
	}
	tm = localtime (&new_time);
	if (tm->tm_mday < mday) {
		tm->tm_mon--;
		tm->tm_mday = time_days_in_month (tm->tm_year+1900, tm->tm_mon);
		return new_time = mktime (tm);
	}
	else
		return new_time;
}

time_t
time_add_year (time_t time, int years)
{
	struct tm *tm = localtime (&time);
	time_t new_time;
	
	tm->tm_year += years;
	if ((new_time = mktime (tm)) == -1) {
		g_message ("time_add_year(): mktime() could not handling adding %d years with\n",
			   years);
		print_time_t (time);
		printf ("\n");
		return time;
	}
	return new_time;
}

/* Number of days in a month, for normal and leap years */
static const int days_in_month[2][12] = {
	{ 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 },
	{ 31, 29, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 }
};

/* Returns whether the specified year is a leap year */
static int
is_leap_year (int year)
{
	if (year <= 1752)
		return !(year % 4);
	else
		return (!(year % 4) && (year % 100)) || !(year % 400);
}

int
time_days_in_month (int year, int month)
{
	g_return_val_if_fail (year >= 1900, 0);
	g_return_val_if_fail ((month >= 0) && (month < 12), 0);

	return days_in_month [is_leap_year (year)][month];
}

time_t
time_from_day (int year, int month, int day)
{
	struct tm tm;

	memset (&tm, 0, sizeof (tm));
	tm.tm_year = year - 1900;
	tm.tm_mon = month;
	tm.tm_mday = day;
	tm.tm_isdst = -1;

	return mktime (&tm);
}

time_t
time_year_begin (time_t t)
{
	struct tm tm;

	tm = *localtime (&t);
	tm.tm_hour = 0;
	tm.tm_min  = 0;
	tm.tm_sec  = 0;
	tm.tm_mon  = 0;
	tm.tm_mday = 1;
	tm.tm_isdst = -1;

	return mktime (&tm);
}

time_t
time_year_end (time_t t)
{
	struct tm tm;

	tm = *localtime (&t);
	tm.tm_hour = 0;
	tm.tm_min  = 0;
	tm.tm_sec  = 0;
	tm.tm_mon  = 0;
	tm.tm_mday = 1;
	tm.tm_year++;
	tm.tm_isdst = -1;

	return mktime (&tm);
}

time_t
time_month_begin (time_t t)
{
	struct tm tm;

	tm = *localtime (&t);
	tm.tm_hour = 0;
	tm.tm_min  = 0;
	tm.tm_sec  = 0;
	tm.tm_mday = 1;
	tm.tm_isdst = -1;

	return mktime (&tm);
}

time_t
time_month_end (time_t t)
{
	struct tm tm;

	tm = *localtime (&t);
	tm.tm_hour = 0;
	tm.tm_min  = 0;
	tm.tm_sec  = 0;
	tm.tm_mday = 1;
	tm.tm_mon++;
	tm.tm_isdst = -1;

	return mktime (&tm);
}

time_t
time_week_begin (time_t t)
{
	struct tm tm;

	/* FIXME: make it take week_starts_on_monday into account */

	tm = *localtime (&t);
	tm.tm_hour = 0;
	tm.tm_min  = 0;
	tm.tm_sec  = 0;
	tm.tm_mday -= tm.tm_wday;
	tm.tm_isdst = -1;

	return mktime (&tm);
}

time_t
time_week_end (time_t t)
{
	struct tm tm;

	/* FIXME: make it take week_starts_on_monday into account */

	tm = *localtime (&t);
	tm.tm_hour = 0;
	tm.tm_min  = 0;
	tm.tm_sec  = 0;
	tm.tm_mday += 7 - tm.tm_wday;
	tm.tm_isdst = -1;

	return mktime (&tm);
}

/* Returns the start of the day, according to the local time. */
time_t
time_day_begin (time_t t)
{
	struct tm tm;

	tm = *localtime (&t);
	tm.tm_hour = 0;
	tm.tm_min  = 0;
	tm.tm_sec  = 0;
	tm.tm_isdst = -1;

	return mktime (&tm);
}

/* Returns the end of the day, according to the local time. */
time_t
time_day_end (time_t t)
{
	struct tm tm;

	tm = *localtime (&t);
	tm.tm_mday++;
	tm.tm_hour = 0;
	tm.tm_min  = 0;
	tm.tm_sec  = 0;
	tm.tm_isdst = -1;

	return mktime (&tm);
}
