/*
 * calc.c Calculations to work out what day it is etc for the Calendar
 *
 * Most of this stuff was taken from the gcal source by Thomas Esken.
 * <esken@uni-muenster.de> 
 * gcal is a text-based calendar program
 */

#include <time.h>
#include <glib.h>
#include <ctype.h>
#include "calcs.h"

#include <config.h>

#ifndef HAVE_STRCASECMP
int strcasecmp(const char * /*s1*/, const char * /*s2*/);
#endif

/* Number of days in a month */
static const int dvec[] = { 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
/* Number of past days of a month */
static const int mvec[] = { 0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334};
Greg_struct greg_reform_date[6] = {
/* {int year, int month, int f_day, int l_day} */
   { 1582, 10,  5, 14 },
   { 1700,  2, 19, 28 },
   { 1752,  9,  3, 13 },
   { 1753,  2, 18, 28 },
/* must be left with all zeroes */
   { 0,0,0,0 }
};
Greg_struct *greg=greg_reform_date;



/*
 * Computes the number of days in February and returns them,
 */
int days_of_february(const int year)
{
	return((year&3) ? 28 : (!(year%100)&&(year%400)) ? 28 : 29);
}

int is_leap_year(const int year)
{
	return (days_of_february(year) == 29);
}

/*
 * Check wether a given date is calid.
 */
int valid_date(const int day, const int month, const int year)
{
	if (   day < 1
		|| month < MONTH_MIN
		|| month > MONTH_MAX
		|| (   (month != 2)
			&& (day > dvec[month-1]))
		|| (   (month == 2)
			&& (day > days_of_february (year))))
	        return(FALSE);

	return(TRUE);
}

/*
 * Set a date back one day (to yesterday's date)
 */
void prev_date(int *day, int *month, int *year)
{
	(*day)--;
	if ( !*day || !valid_date(*day, *month, *year)) {
		(*month)--;
		if (*month < MONTH_MIN) {
			*month = MONTH_MAX;
			(*year)--;
		}
		if (*month ==2)
			*day = days_of_february(*year);
		else
			*day = dvec[*month-1];
	}
} /* prev_date */

/*
 * Set a date forward one day (to tomorrow's date)
 */
void next_date(int *day, int *month, int *year)
{
	(*day)++;
	if (!valid_date(*day, *month, *year)) {
		*day = DAY_MIN;
		if (*month == MONTH_MAX) {
			*month = MONTH_MIN;
			(*year)++;
		} else
			(*month)++;
	}
} /* next_date */

/*
 * Get date from the system 
 */
void get_system_date(int *day, int *month, int *year)
{
	auto struct tm         *sys_date;
	auto time_t   sys_time;


	sys_time  = time((time_t *)NULL);
	sys_date  = localtime(&sys_time);
	*day   = sys_date->tm_mday;
	*month = sys_date->tm_mon + 1;
	*year  = sys_date->tm_year;
	if (*year < CENTURY)
		*year += CENTURY;
} /* get_system_date */


/*
 * Given a string with the name of a month, return 1..12 or 0 if not found
 */
int month_atoi(const char *string)
{
	int i;
	for (i = MONTH_MIN; i <= MONTH_MAX; i++)
		if (strcasecmp(string, (char *)month_name(i)) == 0)
			return i;
	return 0;
}

int day_atoi(const char *string)
{
	int i;
	for (i = DAY_MIN; i <= DAY_MAX; i++)
		if (strcasecmp(string, (char *)day_name(i)) == 0)
			return i;
	return 0;
}

/*
 * Returns ordinal suffix (st, nd, rd, th) for a day
 */
const char *day_suffix(int day)
{
	static const char *suffix[]={"th", "st", "nd", "rd"};
	register int i;

	i = 0;

	if (day > 100)
		day %= 100;
	if (day < 11 || day > 13)
		i = day % 10;
	if (i > 3)
		i = 0;

	return(suffix[i]);
} /* day_suffix */

/* 
 * Returns the short name of the day of week, format "%-3s"
 */
const char *short3_day_name(const int day)
{
	static const char *name[]={"invalid day", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat", "Sun"};

	return(((day<DAY_MIN)||(day>DAY_MAX)) ? name[0] : name[day]);
} /* short3_day_name */

/*
 * Returns the short name of day of week
 */
const char *short_day_name(const int day)
{
	static const char *name[]={"invalid day", "Mo", "Tu", "We", "Th", "Fr", "Sa", "Su"};

	return(((day<DAY_MIN)||(day>DAY_MAX)) ? name[0] : name[day]);
} /* short_day_name */

/*
 * Returns the complete name of the day
 */
const char *day_name(const int day)
{
	static const char *name[]={"invalid day", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday", "Sunday"};

	return(((day<DAY_MIN)||(day>DAY_MAX)) ? name[0] : name[day]);
} /* day_name */

/*
 * Returns the short name of the month
 */
const char *short_month_name(const int month)
{
	static const char *name[]={ "invalid month", "Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};

	return(((month<MONTH_MIN)||(month>MONTH_MAX)) ? name[0] : name[month]);
} /* short_month_name() */

/*
 * Returns the name of the month
 */
const char *month_name(const int month)
{
	static const char *name[]={ "invalid month", "January", "February", "March", "April", "May", "June", "July", "August", "September", "October", "November", "December"};

	return(((month<MONTH_MIN)||(month>MONTH_MAX)) ? name[0] : name[month]);
} /* month_name() */

/*
 * Compute the absolute number of days of the given date since 1 Jan 0001
 * respecting the missing period of the Gregorian Reformation
 * I am glad someone else worked this one out!! - cs
 */
unsigned long int date2num(const int day, const int month, const int year)
{
	auto unsigned long int julian_days;

	julian_days = (unsigned long int)((year-1)*(unsigned long int)(DAY_LAST)+((year-1)>>2));

	if (year > greg->year
		|| (   (year == greg->year)
		    && (   month > greg->month
		        || (   (month == greg->month)
		            && (day > greg->last_day)))))
		julian_days -= (unsigned long int)(greg->last_day - greg->first_day + 1);
	if (year > greg->year) {
		julian_days += (((year-1) / 400) - (greg->year / 400));
		julian_days -= (((year-1) / 100) - (greg->year / 100));
		if (!(greg->year % 100) && (greg->year % 400))
			julian_days--;
	}
	julian_days += (unsigned long int)mvec[month-1];
	julian_days += day;
	if ( (days_of_february(year) == 29) && (month > 2))
		julian_days++;

	return(julian_days);
} /* date2num */

/*
 * Computes the weekday of a Gregorian/Julian calendar date
 * (month must be 1..12) returns 1..7 (mo..su)
 */
int weekday_of_date(const int day, const int month, const int year)
{
	auto unsigned long int julian_days=date2num(day, month,year)%DAY_MAX;

	return((julian_days>2) ? (int)julian_days-2 : (int)julian_days+5);
} /* weekday_of_date() */

