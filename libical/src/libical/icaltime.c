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
#include "icalvalue.h"
#include <assert.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>


struct icaltimetype 
icaltime_from_timet(time_t tm, int is_date, int is_utc)
{
    struct icaltimetype tt;
    struct tm t;

#if 0 /* This is incorrect; a time_t *is* in UTC by definition.  So we just ignore the flag. */
    if(is_utc == 0){
	tm += icaltime_local_utc_offset();
    }
#endif

    t = *(gmtime(&tm));

    tt.second = t.tm_sec;
    tt.minute = t.tm_min;
    tt.hour = t.tm_hour;
    tt.day = t.tm_mday;
    tt.month = t.tm_mon + 1;
    tt.year = t.tm_year+ 1900;
    
#if 0
    tt.is_utc = is_utc;
#endif
    tt.is_utc = 1;
    tt.is_date = is_date; 

    return tt;
}

/* Always returns time in UTC */
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

    if(tt.is_utc)
	stm.tm_sec -= icaltime_local_utc_offset();

    tut = mktime(&stm);

    return tut;
}



struct icaltimetype icaltime_from_string(const char* str)
{
    struct icaltimetype tt;
    icalvalue *v = icalvalue_new_from_string(ICAL_DATETIME_VALUE,str);
						  
    if (v == 0){
	return icaltime_null_time();
    }

    tt = icalvalue_get_datetime(v);

    icalvalue_free(v);

    return tt;
    
}

char ctime_str[20];
char* icaltime_as_ctime(struct icaltimetype t)
{
    time_t tt = icaltime_as_timet(t);

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

/* convert tt, of timezone tzid, into a utc time */
struct icaltimetype icaltime_as_utc(struct icaltimetype tt,const char* tzid)
{
    time_t offset, tm;
    struct icaltimetype utc;

    offset = icaltime_utc_offset(tt,tzid);
    tm = icaltime_as_timet(tt);

    tm += offset;

    utc = icaltime_from_timet(tm,0,0);

    return utc;
}

/* convert tt, a time in UTC, into a time in timezone tzid */
struct icaltimetype icaltime_as_zone(struct icaltimetype tt,const char* tzid)
{
    time_t offset, tm;
    struct icaltimetype zone;

    offset = icaltime_utc_offset(tt,tzid);
    tm = icaltime_as_timet(tt);

    tm -= offset;
    
    zone = icaltime_from_timet(tm,0,0);

    return zone;

}

/* Return the offset of the named zone as seconds. tt is a time
   indicating the date for which you want the offset */
time_t icaltime_utc_offset(struct icaltimetype tt, const char* tzid)
{
#ifdef HAVE_TIMEZONE
    extern long int timezone;
#endif
    time_t now;
    struct tm *stm;

    char *tzstr = 0;
    char *tmp;

    /* Put the new time zone into the environment */
    if(getenv("TZ") != 0){
	tzstr = (char*)strdup(getenv("TZ"));
    }

    tmp = (char*)malloc(1024);
    snprintf(tmp,1024,"TZ=%s",tzid);

    putenv(tmp);

    /* Get the offset */

    now = icaltime_as_timet(tt);

    stm = localtime(&now); /* This sets 'timezone'*/

    /* restore the original environment */

    if(tzstr!=0){
	putenv(tzstr);
    } else {
	putenv("TZ"); /* Delete from environment */
    }
 
#ifdef HAVE_TIMEZONE
    return timezone;
#else
    return -stm->tm_gmtoff;
#endif
}

time_t icaltime_local_utc_offset()
{
    time_t now;
    struct tm *stm;

    stm = localtime(&now); /* This sets 'timezone'*/

#ifdef HAVE_TIMEZONE
    return timezone;
#else
    return -stm->tm_gmtoff;
#endif
}






time_t
icalperiodtype_duration (struct icalperiodtype period);


time_t
icalperiodtype_end (struct icalperiodtype period);


/* From Russel Steinthal */
time_t icaldurationtype_as_timet(struct icaldurationtype dur)
{
        return (time_t) (dur.seconds +
                         (60 * dur.minutes) +
                         (60 * 60 * dur.hours) +
                         (60 * 60 * 24 * dur.days) +
                         (60 * 60 * 24 * 7 * dur.weeks));
} 

/* From Seth Alves,  <alves@hungry.com>   */
struct icaldurationtype icaldurationtype_from_timet(time_t t)
{
        struct icaldurationtype dur;
        time_t used = 0;
 
        dur.weeks = (t - used) / (60 * 60 * 24 * 7);
        used += dur.weeks * (60 * 60 * 24 * 7);
        dur.days = (t - used) / (60 * 60 * 24);
        used += dur.days * (60 * 60 * 24);
        dur.hours = (t - used) / (60 * 60);
        used += dur.hours * (60 * 60);
        dur.minutes = (t - used) / (60);
        used += dur.minutes * (60);
        dur.seconds = (t - used);
 
        return dur;
}

struct icaldurationtype icaldurationtype_from_string(const char* str)
{

    icalvalue *v = icalvalue_new_from_string(ICAL_DURATION_VALUE,str);

    if( v !=0){
	return icalvalue_get_duration(v);
    } else {
        struct icaldurationtype dur;
	memset(&dur,0,sizeof(struct icaldurationtype));
	return dur;
    }
 
}


struct icaltimetype  icaltime_add(struct icaltimetype t,
				  struct icaldurationtype  d)
{
    time_t tt = icaltime_as_timet(t);
    time_t dt = icaldurationtype_as_timet(d);

    return icaltime_from_timet(tt + dt, t.is_date, t.is_utc);

}

struct icaldurationtype  icaltime_subtract(struct icaltimetype t1,
					   struct icaltimetype t2)
{

    time_t t1t = icaltime_as_timet(t1);
    time_t t2t = icaltime_as_timet(t2);

    return icaldurationtype_from_timet(t1t-t2t);


}

