/* -*- Mode: C -*-
  ======================================================================
  FILE: icaltime.c
  CREATOR: eric 02 June 2000
  
  $Id$
  $Locker$
    
 (C) COPYRIGHT 2000, Eric Busboom, http://www.softwarestudio.org

 This program is free software; you can redistribute it and/or modify
 it under the terms of either: 

    The LGPL as published by the Free Software Foundation, version
    2.1, available at: http://www.fsf.org/copyleft/lesser.html

  Or:

    The Mozilla Public License Version 1.0. You may obtain a copy of
    the License at http://www.mozilla.org/MPL/

 The Original Code is eric. The Initial Developer of the Original
 Code is Eric Busboom


 ======================================================================*/

#include "icaltime.h"
#include <assert.h>

struct icaltimetype 
icaltime_from_timet(time_t tm, int is_date, int is_utc)
{
    struct icaltimetype tt;
    struct tm t;

    if(is_utc == 1){
	t = *(gmtime(&tm));
    } else {
	t = *(localtime(&tm));
    }
    tt.second = t.tm_sec;
    tt.minute = t.tm_min;
    tt.hour = t.tm_hour;
    tt.day = t.tm_mday;
    tt.month = t.tm_mon + 1;
    tt.year = t.tm_year+ 1900;
    
    tt.is_utc = is_utc;
    tt.is_date = is_date; 

    return tt;
}

time_t icaltime_as_timet(struct icaltimetype tt)
{
    struct tm stm;
    time_t tut;

    memset(&stm,0,sizeof( struct tm));

    stm.tm_sec = tt.second;
    stm.tm_min = tt.minute;
    stm.tm_hour = tt.hour;
    stm.tm_mday = tt.day;
    stm.tm_mon = tt.month-1;
    stm.tm_year = tt.year-1900;
    stm.tm_isdst = -1; /* prevents mktime from changing hour based on
			  daylight savings */

    tut = mktime(&stm);

    return tut;
}

short days_in_month[] = {0,31,28,31,30,31,30,31,31,30,31,30,31};

short icaltime_days_in_month(short month,short year)
{
    int is_leap =0;
    int days = days_in_month[month];

    assert(month > 0);
    assert(month <= 12);

    if( (year % 4 == 0 && year % 100 != 0) ||
	year % 400 == 0){
	is_leap =1;
    }

    if( month == 2){
	days += is_leap;
    }

    return days;
}

/* 1-> Sunday, 7->Saturday */
short icaltime_day_of_week(struct icaltimetype t){

    time_t tt = icaltime_as_timet(t);
    struct tm *tm;

    tm = gmtime(&tt);

    return tm->tm_wday+1;
}

short icaltime_start_doy_of_week(struct icaltimetype t){
    time_t tt = icaltime_as_timet(t);
    time_t start_tt;
    struct tm *stm;

    stm = gmtime(&tt);

    start_tt = tt - stm->tm_wday*(60*60*24);

    stm = gmtime(&start_tt);

    return stm->tm_yday;
}

short icaltime_day_of_year(struct icaltimetype t){
    time_t tt = icaltime_as_timet(t);
    struct tm *stm;

    stm = gmtime(&tt);

    return stm->tm_yday;
    
}

struct icaltimetype icaltime_from_day_of_year(short doy,  short year)
{
    struct tm stm; 
    time_t tt;

    /* Get the time of january 1 of this year*/
    memset(&stm,0,sizeof(struct tm)); 
    stm.tm_year = year-1900;
    stm.tm_mday = 1;

    tt = mktime(&stm);

    /* Now add in the days */

    tt += doy *60*60*24;

    return icaltime_from_timet(tt, 1, 1);
}

struct icaltimetype icaltime_null_time()
{
    struct icaltimetype t;
    memset(&t,0,sizeof(struct icaltimetype));

    return t;
}
int icaltime_is_null_time(struct icaltimetype t)
{
    if (t.second +t.minute+t.hour+t.day+t.month+t.year == 0){
	return 1;
    }

    return 0;

}

int icaltime_compare(struct icaltimetype a,struct icaltimetype b)
{
    time_t t1 = icaltime_as_timet(a);
    time_t t2 = icaltime_as_timet(b);

    if (t1 > t2) { 
	return 1; 
    } else if (t1 < t2) { 
	return -1;
    } else { 
	return 0; 
    }

}

int icaltime_compare_date_only(struct icaltimetype a, struct icaltimetype b)
{
    time_t t1 = icaltime_as_timet(a);
    time_t t2 = icaltime_as_timet(b);

    if (a.year == b.year && a.month == b.month && a.day == b.day) {
        return 0;
    }

    if (t1 > t2) { 
	return 1; 
    } else if (t1 < t2) { 
	return -1;
    }
}

