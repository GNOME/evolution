// -*- Mode: C -*-
/*======================================================================
 FILE: icaltime.h
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

#ifndef ICALTIME_H
#define ICALTIME_H

#include <time.h>

struct icaltimetype
{
	int year;
	int month;
	int day;
	int hour;
	int minute;
	int second;

	int is_utc; /* 1-> time is in UTC timezone */

	int is_date; /* 1 -> interpret this as date. */
};	

struct icaltimetype icaltime_null_time();

int icaltime_is_null_time(struct icaltimetype t);

struct icaltimetype icaltime_normalize(struct icaltimetype t);

short icaltime_day_of_year(struct icaltimetype t);
struct icaltimetype icaltime_from_day_of_year(short doy,  short year);

short icaltime_day_of_week(struct icaltimetype t);
short icaltime_start_doy_of_week(struct icaltimetype t);

struct icaltimetype icaltime_from_timet(time_t v, int is_date, int is_utc);
time_t icaltime_as_timet(struct icaltimetype);

short icaltime_week_number(short day_of_month, short month, short year);

struct icaltimetype icaltime_from_week_number(short week_number, short year);

int icaltime_compare(struct icaltimetype a,struct icaltimetype b);
int icaltime_compare_date_only(struct icaltimetype a, struct icaltimetype b);

short icaltime_days_in_month(short month,short year);

#endif /* !ICALTIME_H */



