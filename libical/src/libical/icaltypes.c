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
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "icaltypes.h"
#include "icalerror.h"
#include "icalmemory.h"
#include <stdlib.h> /* for malloc */
#include <errno.h> /* for errno */
#include <string.h> /* for strdup */
#include <assert.h>
#include <limits.h> /* for SHRT_MAX */

#define TEMP_MAX 1024

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


struct icalreqstattype icalreqstattype_from_string(char* str)
{
  char *p1,*p2;
  size_t len; 
  struct icalreqstattype stat;
  int major, minor;

  icalerror_check_arg((str != 0),"str");

  stat.code = ICAL_UNKNOWN_STATUS;
  stat.debug = 0; 

   stat.desc = 0;

  /* Get the status numbers */

  sscanf(str, "%d.%d",&major, &minor);

  if (major <= 0 || minor < 0){
    icalerror_set_errno(ICAL_BADARG_ERROR);
    return stat;
  }

  stat.code = icalenum_num_to_reqstat(major, minor);

  if (stat.code == ICAL_UNKNOWN_STATUS){
    icalerror_set_errno(ICAL_BADARG_ERROR);
    return stat;
  }
  

  p1 = strchr(str,';');

  if (p1 == 0){
    icalerror_set_errno(ICAL_BADARG_ERROR);
    return stat;
  }

  /* Just ignore the second clause; it will be taken from inside the library 
   */



  p2 = strchr(p1+1,';');
  if (p2 != 0 && *p2 != 0){
    stat.debug = p2+1;
  } 

  return stat;
  
}

char* icalreqstattype_as_string(struct icalreqstattype stat)
{
  char format[20];
  char *temp;

  temp = (char*)icalmemory_tmp_buffer(TEMP_MAX);

  icalerror_check_arg_rz((stat.code != ICAL_UNKNOWN_STATUS),"Status");
  
  if (stat.desc == 0){
    stat.desc = icalenum_reqstat_desc(stat.code);
  }
  
  if(stat.debug != 0){
    snprintf(temp,TEMP_MAX,"%d.%d;%s;%s", icalenum_reqstat_major(stat.code),
             icalenum_reqstat_minor(stat.code),
             stat.desc, stat.debug);
    
  } else {
    snprintf(temp,TEMP_MAX,"%d.%d;%s", icalenum_reqstat_major(stat.code),
             icalenum_reqstat_minor(stat.code),
             stat.desc);
  }

  return temp;
}
