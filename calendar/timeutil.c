/* Miscellaneous time-related utilities
 *
 * Copyright (C) 1998 The Free Software Foundation
 *
 * Authors: Federico Mena <federico@nuclecu.unam.mx>
 *          Miguel de Icaza <miguel@nuclecu.unam.mx>
 */

#include <libgnome/libgnome.h>
#include <string.h>
#include "timeutil.h"

#define digit_at(x,y) (x [y] - '0')
	
time_t
time_from_isodate (char *str)
{
	struct tm my_tm;
	time_t t;

	if (strlen (str) < 14)
		return -1;
	
	my_tm.tm_year = (digit_at (str, 0) * 1000 + digit_at (str, 1) * 100 +
		digit_at (str, 2) * 10 + digit_at (str, 3)) - 1900;

	my_tm.tm_mon  = digit_at (str, 4) * 10 + digit_at (str, 5) - 1;
	my_tm.tm_mday = digit_at (str, 6) * 10 + digit_at (str, 7);
	my_tm.tm_hour = digit_at (str, 9) * 10 + digit_at (str, 10);
	my_tm.tm_min  = digit_at (str, 11) * 10 + digit_at (str, 12);
	my_tm.tm_sec  = digit_at (str, 13) * 10 + digit_at (str, 14);
	my_tm.tm_isdst = -1;
	
	t = mktime (&my_tm);

	if (str [15] == 'Z')
		t -= timezone;
	    
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
	strftime (isotime, sizeof (isotime)-1, "%Y%m%dT%H%M%S", tm);
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
time_add_minutes (time_t time, int minutes)
{
	struct tm *tm = localtime (&time);
	time_t new_time;

	tm->tm_min += minutes;
	if ((new_time = mktime (tm)) == -1){
		g_warning ("mktime could not handling adding a day with\n");
		print_time_t (time);
		return time;
	}
	return new_time;
}

time_t
time_add_day (time_t time, int days)
{
	struct tm *tm = localtime (&time);
	time_t new_time;
	int dst_flag = tm->tm_isdst;

	tm->tm_mday += days;

	if ((new_time = mktime (tm)) == -1){
		g_warning ("mktime could not handling adding a day with\n");
		print_time_t (time);
		return time;
	}

	if (dst_flag > tm->tm_isdst){
		tm->tm_hour++;
		new_time += 3600;
	} else if (dst_flag < tm->tm_isdst){
		tm->tm_hour--;
		new_time -= 3600;
	}

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
	if ((new_time = mktime (tm)) == -1){
		g_warning ("mktime could not handling adding a month with\n");
		print_time_t (time);
		return time;
	}
	tm = localtime (&new_time);
	if (tm->tm_mday < mday){
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

	return mktime (&tm);
}

time_t
time_day_begin (time_t t)
{
	struct tm tm;
	time_t temp = t - 43200;
	int dstflag, dstflag2;
	
	tm = *localtime(&temp); /* one day */
	dstflag = tm.tm_isdst;

	tm = *localtime (&t);
	dstflag2 = tm.tm_isdst;

	if (dstflag < dstflag2)
		tm.tm_hour = 1;
	else
		tm.tm_hour = 0;

	tm.tm_min  = 0;
	tm.tm_sec  = 0;
	
	temp = mktime(&tm);
	if (dstflag > dstflag2){
		temp += 3600; 
	}

	return temp;
}

time_t
time_day_end (time_t t)
{
	struct tm tm;
	time_t temp;
	int dstflag, dstflag2;

	t += 10800;
	temp = t - 86400;

	tm = *localtime(&temp); /* one day */
	dstflag = tm.tm_isdst;

	tm = *localtime (&t);
	dstflag2 = tm.tm_isdst;

	if (dstflag < dstflag2)
		tm.tm_hour = 23;
	else {
		tm.tm_mday++;
		tm.tm_hour = 0;
	}
	tm.tm_min  = 0;
	tm.tm_sec  = 0;
	
	temp = mktime(&tm);
	if(dstflag > dstflag2) {
	}
	return temp;
}

static char *
pcat (char *dest, int num, char key)
{
	int c;

	c = sprintf (dest, "%d%c", num, key);
	return dest + c;
}

/* Converts secs into the ISO difftime representation */
char *
isodiff_from_secs (int secs)
{
	static char buffer [60], *p;
	int years, months, weeks, days, hours, minutes;
	
	years = months = weeks = days = hours = minutes = 0;
	
	years    = secs / (365 * 86400);
	secs    %= (365 * 86400);
	months   = secs / (30 * 86400);
	secs    %= (30 * 86400);
	weeks    = secs / (7 * 86400);
	secs    %= (7 * 86400);
	days     = secs / 86400;
	secs    %= 86400;
	hours    = secs / 3600;
	secs    %= 3600;
	minutes  = secs / 60;
	secs    %= 60;

	strcpy (buffer, "P");
	p = buffer + 1;
	if (years)
		p = pcat (p, years, 'Y');
	if (months)
		p = pcat (p, months, 'M');
	if (weeks)
		p = pcat (p, weeks, 'W');
	if (days)
		p = pcat (p, days, 'D');
	if (hours || minutes || secs){
		*p++ = 'T';
		if (hours)
			p = pcat (p, hours, 'H');
		if (minutes)
			p = pcat (p, minutes, 'M');
		if (secs)
			p = pcat (p, secs, 'S');
	}
	
	return buffer;
}

int
isodiff_to_secs (char *str)
{
	int value, time;
	int years, months, weeks, days, hours, minutes, seconds;

	value = years = months = weeks = days = hours = minutes = time = seconds = 0;
	if (*str != 'P')
		return 0;

	str++;
	while (*str){
		switch (*str){
		case '0': case '1': case '2': case '3': case '4':
		case '5': case '6': case '7': case '8': case '9':
			value = value * 10 + (*str - '0');
			break;
		case 'Y':
			years = value; value = 0;
			break;
		case 'M':
			if (time)
				minutes = value;
			else
				months = value;
			value = 0;
			break;
		case 'W':
			weeks = value; value = 0;
			break;
		case 'D':
			days = value; value = 0;
			break;
		case 'T':
			value = 0; time = 1;
			break;
		case 'H':
			hours = value; value = 0;
			break;
		case 'S':
			seconds = value; value = 0;
			break;
		}
		str++;
	}
	return seconds + (minutes * 60) + (hours * 3600) +
	       (days * 86400) + (weeks * 7 * 86400) +
	       (months * 30 * 86400) + (years * 365 * 86400);
}
