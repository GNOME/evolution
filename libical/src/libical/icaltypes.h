/* -*- Mode: C -*- */
/*======================================================================
 FILE: icaltypes.h
 CREATOR: eric 20 March 1999


  (C) COPYRIGHT 1999 Eric Busboom 
  http://www.softwarestudio.org

  The contents of this file are subject to the Mozilla Public License
  Version 1.0 (the "License"); you may not use this file except in
  compliance with the License. You may obtain a copy of the License at
  http://www.mozilla.org/MPL/
 
  Software distributed under the License is distributed on an "AS IS"
  basis, WITHOUT WARRANTY OF ANY KIND, either express or implied. See
  the License for the specific language governing rights and
  limitations under the License.

  The original author is Eric Busboom
  The original code is icaltypes.h

======================================================================*/

#ifndef ICALTYPES_H
#define ICALTYPES_H

#include <time.h>
#include "icalenums.h" /* for recurrence enums */

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

struct icaltimetype icaltimetype_from_timet(time_t v, int is_date);
					   

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
	short by_day[8];
	short by_month_day[32];
	short by_year_day[367];
	short by_week_no[54];
	short by_month[13];
	short by_set_pos[367];
};


void icalrecurrencetype_clear(struct icalrecurrencetype *r);

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

struct icalrequestsstatustype {

	short minor;
	short major; 

};


#endif /* !ICALTYPES_H */
