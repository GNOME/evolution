/* -*- Mode: C -*- */
/*======================================================================
 FILE: icaltypes.h
 CREATOR: eric 20 March 1999


 (C) COPYRIGHT 2000, Eric Busboom, http://www.softwarestudio.org

 This program is free software; you can redistribute it and/or modify
 it under the terms of either: 

    The LGPL as published by the Free Software Foundation, version
    2.1, available at: http://www.fsf.org/copyleft/lesser.html

  Or:

    The Mozilla Public License Version 1.0. You may obtain a copy of
    the License at http://www.mozilla.org/MPL/

  The original code is icaltypes.h

======================================================================*/

#ifndef ICALTYPES_H
#define ICALTYPES_H

#include <time.h>
#include "icalenums.h" /* for recurrence enums */
#include "icaltime.h"

/* This type type should probably be an opaque type... */
struct icalattachtype
{
	void* binary;
	int owns_binary; 

	char* base64;
	int owns_base64;

	char* url;

	int refcount; 

};

/* converts base64 to binary, fetches url and stores as binary, or
   just returns data */

struct icalattachtype* icalattachtype_new();
void  icalattachtype_add_reference(struct icalattachtype* v);
void icalattachtype_free(struct icalattachtype* v);

void icalattachtype_set_url(struct icalattachtype* v, char* url);
char* icalattachtype_get_url(struct icalattachtype* v);

void icalattachtype_set_base64(struct icalattachtype* v, char* base64,
				int owns);
char* icalattachtype_get_base64(struct icalattachtype* v);

void icalattachtype_set_binary(struct icalattachtype* v, char* binary,
				int owns);
void* icalattachtype_get_binary(struct icalattachtype* v);

struct icalgeotype 
{
	float lat;
	float lon;
};

					   

/* See RFC 2445 Section 4.3.10, RECUR Value, for an explaination of
   the values and fields in struct icalrecurrencetype */


struct icalrecurrencetype 
{
	icalrecurrencetype_frequency freq;


	/* until and count are mutually exclusive. */
       	struct icaltimetype until;
	int count;

	short interval;
	
	icalrecurrencetype_weekday week_start;
	
	/* The BY* parameters can each take a list of values. Here I
	 * assume that the list of values will not be larger than the
	 * range of the value -- that is, the client will not name a
	 * value more than once. 
	 
	 * Each of the lists is terminated with the value SHRT_MAX
	 * unless the the list is full. */

	short by_second[61];
	short by_minute[61];
	short by_hour[25];
	short by_day[8]; /* Encoded value, see below */
	short by_month_day[32];
	short by_year_day[367];
	short by_week_no[54];
	short by_month[13];
	short by_set_pos[367];
};


void icalrecurrencetype_clear(struct icalrecurrencetype *r);

/* The 'day' element of icalrecurrencetype_weekday is encoded to allow
reporesentation of both the day of the week ( Monday, Tueday), but
also the Nth day of the week ( First tuesday of the month, last
thursday of the year) These routines decode the day values */

/* 1 == Monday, etc. */
enum icalrecurrencetype_weekday icalrecurrencetype_day_day_of_week(short day);

/* 0 == any of day of week. 1 == first, 2 = second, -2 == second to last, etc */
short icalrecurrencetype_day_position(short day);

struct icaldurationtype
{
	unsigned int days;
	unsigned int weeks;
	unsigned int hours;
	unsigned int minutes;
	unsigned int seconds;
};

struct icaldurationtype icaldurationtype_from_timet(time_t t);
time_t icaldurationtype_as_timet(struct icaldurationtype duration);

/* Return the next occurance of 'r' after the time specified by 'after' */
struct icaltimetype icalrecurrencetype_next_occurance(
    struct icalrecurrencetype *r,
    struct icaltimetype *after);


struct icalperiodtype 
{
	struct icaltimetype start; /* Must be absolute */	
	struct icaltimetype end; /* Must be absolute */
	struct icaldurationtype duration;
};

time_t icalperiodtype_duration(struct icalperiodtype period);
time_t icalperiodtype_end(struct icalperiodtype period);

union icaltriggertype 
{
	struct icaltimetype time; 
	struct icaldurationtype duration;
};


/* struct icalreqstattype. This struct contains two string pointers,
but don't try to free either of them. The "desc" string is a pointer
to a static table inside the library.  Don't try to free it. The
"debug" string is a pointer into the string that the called passed
into to icalreqstattype_from_string. Don't try to free it either, and
don't use it after the original string has been freed.

BTW, you would get that original string from
*icalproperty_get_requeststatus() or icalvalue_get_text(), when
operating on a the value of a request_status property. */

struct icalreqstattype {

	icalrequeststatus code;
  char* desc;
  char* debug;
};

struct icalreqstattype icalreqstattype_from_string(char* str);
char* icalreqstattype_as_string(struct icalreqstattype);

#endif /* !ICALTYPES_H */
