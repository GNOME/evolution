/* -*- Mode: C -*-
  ======================================================================
  FILE: icaltypes.c
  CREATOR: eric 16 May 1999
  
  $Id$
  $Locker$
    

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
  The original code is icaltypes.c

 ======================================================================*/

#include "icaltypes.h"
#include "icalerror.h"
#include <stdlib.h> /* for malloc */
#include <errno.h> /* for errno */
#include <string.h> /* for strdup */
#include <assert.h>
#include <limits.h> /* for SHRT_MAX */

void*
icalattachtype_get_data (struct icalattachtype* type);

struct icalattachtype*
icalattachtype_new()
{
    struct icalattachtype* v;

    if ( ( v = (struct icalattachtype*)
	   malloc(sizeof(struct icalattachtype))) == 0) {
	errno = ENOMEM;
	return 0;
    }

    v->refcount = 1;

    v->binary = 0;
    v->owns_binary = 0;

    v->base64 = 0;
    v->owns_base64 = 0;

    v->url = 0; 

    return v;
}


void
icalattachtype_free(struct icalattachtype* v)
{
    icalerror_check_arg( (v!=0),"v");
    
    v->refcount--;

    if (v->refcount <= 0){
	
	if (v->base64 != 0 && v->owns_base64 != 0){
	    free(v->base64);
	}

	if (v->binary != 0 && v->owns_binary != 0){
	    free(v->binary);
	}
	
	if (v->url != 0){
	    free(v->url);
	}

	free(v);
    }
}

void  icalattachtype_add_reference(struct icalattachtype* v)
{
    icalerror_check_arg( (v!=0),"v");
    v->refcount++;
}

void icalattachtype_set_url(struct icalattachtype* v, char* url)
{
    icalerror_check_arg( (v!=0),"v");

    if (v->url != 0){
	free (v->url);
    }

    v->url = strdup(url);

    /* HACK This routine should do something if strdup returns NULL */

}

char* icalattachtype_get_url(struct icalattachtype* v)
{
    icalerror_check_arg( (v!=0),"v");
    return v->url;
}

void icalattachtype_set_base64(struct icalattachtype* v, char* base64,
				int owns)
{
    icalerror_check_arg( (v!=0),"v");

    v->base64 = base64;
    v->owns_base64 = !(owns != 0 );
    
}

char* icalattachtype_get_base64(struct icalattachtype* v)
{
    icalerror_check_arg( (v!=0),"v");
    return v->base64;
}

void icalattachtype_set_binary(struct icalattachtype* v, char* binary,
				int owns)
{
    icalerror_check_arg( (v!=0),"v");

    v->binary = binary;
    v->owns_binary = !(owns != 0 );

}

void* icalattachtype_get_binary(struct icalattachtype* v)
{
    icalerror_check_arg( (v!=0),"v");
    return v->binary;
}



time_t
icalperiodtype_duration (struct icalperiodtype period);


time_t
icalperiodtype_end (struct icalperiodtype period);

struct icaltimetype 
icaltimetype_from_timet(time_t v, int date)
{
    struct icaltimetype tt;
    struct tm t;
    time_t tm = time(&v);

/* HACK Does not properly consider timezone */
    t = *(gmtime(&tm));

    tt.second = t.tm_sec;
    tt.minute = t.tm_min;
    tt.hour = t.tm_hour;
    tt.day = t.tm_mday;
    tt.month = t.tm_mon + 1;
    tt.year = t.tm_year+ 1900;
    
    tt.is_utc = 1;
    tt.is_date = date; 

    return tt;
}

/* From Russel Steinthal */
time_t icaldurationtype_as_timet(struct icaldurationtype dur)
{
        return (time_t) (dur.seconds +
                         (60 * dur.minutes) +
                         (60 * 60 * dur.hours) +
                         (60 * 60 * 24 * dur.days) +
                         (60 * 60 * 24 * 7 * dur.weeks));
} 


void icalrecurrencetype_clear(struct icalrecurrencetype *recur)
{
    memset(recur,ICAL_RECURRENCE_ARRAY_MAX_BYTE,
	   sizeof(struct icalrecurrencetype));

    recur->week_start = ICAL_NO_WEEKDAY;
    recur->freq = ICAL_NO_RECURRENCE;
    recur->interval = 0;
    recur->until.year = 0;
    recur->count = 0;
}
