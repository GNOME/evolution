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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "icaltime.h"
#include <assert.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#ifdef ICAL_NO_LIBICAL
#define icalerror_set_errno(x)
#define  icalerror_check_arg_rv(x,y)
#define  icalerror_check_arg_re(x,y,z)
#else
#include "icalerror.h"
#include "icalmemory.h"
#endif




struct icaltimetype 
icaltime_from_timet(time_t tm, int is_date)
{
    struct icaltimetype tt = icaltime_null_time();
    struct tm t;

    t = *(gmtime(&tm));
     
    if(is_date == 0){ 
	tt.second = t.tm_sec;
	tt.minute = t.tm_min;
	tt.hour = t.tm_hour;
    } else {
	tt.second = tt.minute =tt.hour = 0 ;
    }

    tt.day = t.tm_mday;
    tt.month = t.tm_mon + 1;
    tt.year = t.tm_year+ 1900;
    
    tt.is_utc = 1;
    tt.is_date = is_date; 

    return tt;
}

/* Structure used by set_tz to hold an old value of TZ, and the new
   value, which is in memory we will have to free in unset_tz */
/* This will hold the last "TZ=XXX" string we used with putenv(). After we
   call putenv() again to set a new TZ string, we can free the previous one.
   As far as I know, no libc implementations actually free the memory used in
   the environment variables (how could they know if it is a static string or
   a malloc'ed string?), so we have to free it ourselves. */
static char* saved_tz = NULL;


/* If you use set_tz(), you must call unset_tz() some time later to restore the
   original TZ. Pass unset_tz() the string that set_tz() returns. */
char* set_tz(const char* tzid)
{
    char *old_tz, *old_tz_copy = NULL, *new_tz;

    /* Get the old TZ setting and save a copy of it to return. */
    old_tz = getenv("TZ");
    if(old_tz){
	old_tz_copy = (char*)malloc(strlen (old_tz) + 4);

	if(old_tz_copy == 0){
	    icalerror_set_errno(ICAL_NEWFAILED_ERROR);
	    return 0;
	}

	strcpy (old_tz_copy, "TZ=");
	strcpy (old_tz_copy + 3, old_tz);
    }

    /* Create the new TZ string. */
    new_tz = (char*)malloc(strlen (tzid) + 4);

    if(new_tz == 0){
	icalerror_set_errno(ICAL_NEWFAILED_ERROR);
	return 0;
    }

    strcpy (new_tz, "TZ=");
    strcpy (new_tz + 3, tzid);

    /* Add the new TZ to the environment. */
    putenv(new_tz); 

    /* Free any previous TZ environment string we have used. */
    if (saved_tz)
      free (saved_tz);

    /* Save a pointer to the TZ string we just set, so we can free it later. */
    saved_tz = new_tz;

    return old_tz_copy; /* This will be zero if the TZ env var was not set */
}

void unset_tz(char *tzstr)
{
    /* restore the original environment */

    if(tzstr!=0){
	putenv(tzstr);
    } else {
	putenv("TZ"); /* Delete from environment */
    } 

    /* Free any previous TZ environment string we have used. */
    if (saved_tz)
      free (saved_tz);

    /* Save a pointer to the TZ string we just set, so we can free it later.
       (This can possibly be NULL if there was no TZ to restore.) */
    saved_tz = tzstr;
}

time_t icaltime_as_timet(struct icaltimetype tt)
{
    struct tm stm;
    time_t t;

    memset(&stm,0,sizeof( struct tm));

    if(icaltime_is_null_time(tt)) {
	return 0;
    }

    stm.tm_sec = tt.second;
    stm.tm_min = tt.minute;
    stm.tm_hour = tt.hour;
    stm.tm_mday = tt.day;
    stm.tm_mon = tt.month-1;
    stm.tm_year = tt.year-1900;
    stm.tm_isdst = -1;

    if(tt.is_utc == 1 || tt.is_date == 1){
        char *old_tz = set_tz("UTC");
	t = mktime(&stm);
	unset_tz(old_tz);
    } else {
	t = mktime(&stm);
    }

    return t;

}

char* icaltime_as_ical_string(struct icaltimetype tt)
{
    size_t size = 17;
    char* buf = icalmemory_new_buffer(size);

    if(tt.is_date){
	snprintf(buf, size,"%04d%02d%02d",tt.year,tt.month,tt.day);
    } else {
	char* fmt;
	if(tt.is_utc){
	    fmt = "%04d%02d%02dT%02d%02d%02dZ";
	} else {
	    fmt = "%04d%02d%02dT%02d%02d%02d";
	}
	snprintf(buf, size,fmt,tt.year,tt.month,tt.day,
		 tt.hour,tt.minute,tt.second);
    }
    
    icalmemory_add_tmp_buffer(buf);

    return buf;

}


/* convert tt, of timezone tzid, into a utc time */
struct icaltimetype icaltime_as_utc(struct icaltimetype tt,const char* tzid)
{
    int tzid_offset;

    if(tt.is_utc == 1 || tt.is_date == 1){
	return tt;
    }

    tzid_offset = icaltime_utc_offset(tt,tzid);

    tt.second -= tzid_offset;

    tt.is_utc = 1;

    return icaltime_normalize(tt);
}

/* convert tt, a time in UTC, into a time in timezone tzid */
struct icaltimetype icaltime_as_zone(struct icaltimetype tt,const char* tzid)
{
    int tzid_offset;

    tzid_offset = icaltime_utc_offset(tt,tzid);

    tt.second += tzid_offset;

    tt.is_utc = 0;

    return icaltime_normalize(tt);

}


/* Return the offset of the named zone as seconds. tt is a time
   indicating the date for which you want the offset */
int icaltime_utc_offset(struct icaltimetype ictt, const char* tzid)
{

    time_t tt = icaltime_as_timet(ictt);
    time_t offset_tt;
    struct tm gtm;

    char *tz_str = 0;

    if(tzid != 0){
	tz_str = set_tz(tzid);
    }

    /* Mis-interpret a UTC broken out time as local time */
    gtm = *(gmtime(&tt));
    gtm.tm_isdst = localtime(&tt)->tm_isdst;    
    offset_tt = mktime(&gtm);
    
    if(tzid != 0){
	unset_tz(tz_str);
    }

    return tt-offset_tt;
}



/* Normalize by converting from localtime to utc and back to local
   time. This uses localtime because localtime and mktime are inverses
   of each other */

struct icaltimetype icaltime_normalize(struct icaltimetype tt)
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

    stm = *(localtime(&tut));

    tt.second = stm.tm_sec;
    tt.minute = stm.tm_min;
    tt.hour = stm.tm_hour;
    tt.day = stm.tm_mday;
    tt.month = stm.tm_mon +1;
    tt.year = stm.tm_year+1900;

    return tt;
}


#ifndef ICAL_NO_LIBICAL
#include "icalvalue.h"

struct icaltimetype icaltime_from_string(const char* str)
{
    struct icaltimetype tt = icaltime_null_time();
    int size;

    icalerror_check_arg_re(str!=0,"str",icaltime_null_time());

    size = strlen(str);
    
    if(size == 15) { /* floating time */
	tt.is_utc = 0;
	tt.is_date = 0;
    } else if (size == 16) { /* UTC time, ends in 'Z'*/
	tt.is_utc = 1;
	tt.is_date = 0;

	if(str[15] != 'Z'){
	    icalerror_set_errno(ICAL_MALFORMEDDATA_ERROR);
	    return icaltime_null_time();
	}
	    
    } else if (size == 8) { /* A DATE */
	tt.is_utc = 1;
	tt.is_date = 1;
    } else { /* error */
	icalerror_set_errno(ICAL_MALFORMEDDATA_ERROR);
	return icaltime_null_time();
    }

    if(tt.is_date == 1){
	sscanf(str,"%04d%02d%02d",&tt.year,&tt.month,&tt.day);
    } else {
	char tsep;
	sscanf(str,"%04d%02d%02d%c%02d%02d%02d",&tt.year,&tt.month,&tt.day,
	       &tsep,&tt.hour,&tt.minute,&tt.second);

	if(tsep != 'T'){
	    icalerror_set_errno(ICAL_MALFORMEDDATA_ERROR);
	    return icaltime_null_time();
	}

    }

    return tt;    
}
#endif

char ctime_str[20];
char* icaltime_as_ctime(struct icaltimetype t)
{
    time_t tt;
 
    tt = icaltime_as_timet(t);
    sprintf(ctime_str,"%s",ctime(&tt));

    ctime_str[strlen(ctime_str)-1] = 0;

    return ctime_str;
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
    struct tm stm;

    stm.tm_year = t.year - 1900;
    stm.tm_mon = t.month - 1;
    stm.tm_mday = t.day;
    stm.tm_hour = 0;
    stm.tm_min = 0;
    stm.tm_sec = 0;
    stm.tm_isdst = -1;

    mktime (&stm);

    return stm.tm_wday + 1;
}

/* Day of the year that the first day of the week (Sunday) is on  */
short icaltime_start_doy_of_week(struct icaltimetype t){
    time_t tt = icaltime_as_timet(t);
    time_t start_tt;
    struct tm *stm;
    int syear;

    stm = gmtime(&tt);
    syear = stm->tm_year;

    start_tt = tt - stm->tm_wday*(60*60*24);

    stm = gmtime(&start_tt);
    
    if(syear == stm->tm_year){
	return stm->tm_yday+1;
    } else {
	/* return negative to indicate that start of week is in
           previous year. */
	int is_leap = 0;
	int year = stm->tm_year;

	if( (year % 4 == 0 && year % 100 != 0) ||
	    year % 400 == 0){
	    is_leap =1;
	}

	return (stm->tm_yday+1)-(365+is_leap);
    }
    
}

short icaltime_week_number(struct icaltimetype ictt)
{
    char str[5];
    time_t t = icaltime_as_timet(ictt);
    int week_no;

    strftime(str,5,"%V", gmtime(&t));

    week_no = atoi(str);

    return week_no;

}


short icaltime_day_of_year(struct icaltimetype t){
    struct tm stm;

    stm.tm_year = t.year - 1900;
    stm.tm_mon = t.month - 1;
    stm.tm_mday = t.day;
    stm.tm_hour = 0;
    stm.tm_min = 0;
    stm.tm_sec = 0;
    stm.tm_isdst = -1;

    mktime (&stm);

    return stm.tm_yday + 1;
}

/* Jan 1 is day #1, not 0 */
struct icaltimetype icaltime_from_day_of_year(short doy,  short year)
{
    static const short days_in_year[2][13] = 
    { /* jan feb mar apr may  jun  jul  aug  sep  oct  nov  dec */
      {  0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334, 365 }, 
      {  0, 31, 60, 91, 121, 152, 182, 213, 244, 274, 305, 335, 366 }
    };
    struct icaltimetype tt = { 0 };
    int is_leap = 0, month;

    tt.year = year;
    if ((year % 4 == 0 && year % 100 != 0) || year % 400 == 0)
	is_leap = 1;

    assert(doy > 0);
    assert(doy <= days_in_year[is_leap][12]);

    for (month = 11; month >= 0; month--) {
      if (doy > days_in_year[is_leap][month]) {
	tt.month = month + 1;
	tt.day = doy - days_in_year[is_leap][month];
	return tt;
      }
    }

    /* Shouldn't reach here. */
    assert (0);
}

struct icaltimetype icaltime_null_time()
{
    struct icaltimetype t;
    memset(&t,0,sizeof(struct icaltimetype));

    return t;
}


int icaltime_is_valid_time(struct icaltimetype t){
    if(t.is_utc > 1 || t.is_utc < 0 ||
       t.year < 0 || t.year > 3000 ||
       t.is_date > 1 || t.is_date < 0){
	return 0;
    } else {
	return 1;
    }

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

int
icaltime_compare_date_only (struct icaltimetype a, struct icaltimetype b)
{
    time_t t1;
    time_t t2;

    if (a.year == b.year && a.month == b.month && a.day == b.day)
        return 0;

    t1 = icaltime_as_timet (a);
    t2 = icaltime_as_timet (b);

    if (t1 > t2) 
	return 1; 
    else if (t1 < t2)
	return -1;
    else {
	/* not reached */
	assert (0);
	return 0;
    }
}

/* These are defined in icalduration.c:
struct icaltimetype  icaltime_add(struct icaltimetype t,
				  struct icaldurationtype  d)
struct icaldurationtype  icaltime_subtract(struct icaltimetype t1,
					   struct icaltimetype t2)
*/



/* Adds (or subtracts) a time from a icaltimetype.
   NOTE: This function is exactly the same as icaltimezone_adjust_change()
   except for the type of the first parameter. */
void
icaltime_adjust				(struct icaltimetype	*tt,
					 int		 days,
					 int		 hours,
					 int		 minutes,
					 int		 seconds)
{
    int second, minute, hour, day;
    int minutes_overflow, hours_overflow, days_overflow;
    int days_in_month;

    /* Add on the seconds. */
    second = tt->second + seconds;
    tt->second = second % 60;
    minutes_overflow = second / 60;
    if (tt->second < 0) {
	tt->second += 60;
	minutes_overflow--;
    }

    /* Add on the minutes. */
    minute = tt->minute + minutes + minutes_overflow;
    tt->minute = minute % 60;
    hours_overflow = minute / 60;
    if (tt->minute < 0) {
	tt->minute += 60;
	hours_overflow--;
    }

    /* Add on the hours. */
    hour = tt->hour + hours + hours_overflow;
    tt->hour = hour % 24;
    days_overflow = hour / 24;
    if (tt->hour < 0) {
	tt->hour += 24;
	days_overflow--;
    }

    /* Add on the days. */
    day = tt->day + days + days_overflow;
    if (day > 0) {
	for (;;) {
	    days_in_month = icaltime_days_in_month (tt->month, tt->year);
	    if (day <= days_in_month)
		break;

	    tt->month++;
	    if (tt->month >= 13) {
		tt->year++;
		tt->month = 1;
	    }

	    day -= days_in_month;
	}
    } else {
	while (day <= 0) {
	    if (tt->month == 1) {
		tt->year--;
		tt->month = 12;
	    } else {
		tt->month--;
	    }

	    day += icaltime_days_in_month (tt->month, tt->year);
	}
    }
    tt->day = day;
}
