/* -*- Mode: C -*-
  ======================================================================
  FILE: icaltypes.c
  CREATOR: eric 16 May 1999
  
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

  The original code is icaltypes.c

 ======================================================================*/
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "icaltypes.h"
#include "icalerror.h"
#include "icalmemory.h"
#include "icalvalueimpl.h"
#include <stdlib.h> /* for malloc and abs() */
#include <errno.h> /* for errno */
#include <string.h> /* for icalmemory_strdup */
#include <assert.h>

#define TEMP_MAX 1024

icalattach *
icalattach_new_from_url (const char *url)
{
    icalattach *attach;
    char *url_copy;

    icalerror_check_arg_rz ((url != NULL), "url");

    if ((attach = malloc (sizeof (icalattach))) == NULL) {
	errno = ENOMEM;
	return NULL;
    }

    if ((url_copy = strdup (url)) == NULL) {
	free (attach);
	errno = ENOMEM;
	return NULL;
    }

    attach->refcount = 1;
    attach->is_url = 1;
    attach->u.url.url = url_copy;

    return attach;
}

icalattach *
icalattach_new_from_data (const unsigned char *data, icalattach_free_fn_t free_fn,
			  void *free_fn_data)
{
    icalattach *attach;

    icalerror_check_arg_rz ((data != NULL), "data");

    if ((attach = malloc (sizeof (icalattach))) == NULL) {
	errno = ENOMEM;
	return NULL;
    }

    attach->refcount = 1;
    attach->is_url = 0;
    attach->u.data.data = (unsigned char *) data;
    attach->u.data.free_fn = free_fn;
    attach->u.data.free_fn_data = free_fn_data;

    return attach;
}

void
icalattach_ref (icalattach *attach)
{
    icalerror_check_arg_rv ((attach != NULL), "attach");
    icalerror_check_arg_rv ((attach->refcount > 0), "attach->refcount > 0");

    attach->refcount++;
}

void
icalattach_unref (icalattach *attach)
{
    icalerror_check_arg_rv ((attach != NULL), "attach");
    icalerror_check_arg_rv ((attach->refcount > 0), "attach->refcount > 0");

    attach->refcount--;

    if (attach->refcount != 0)
	return;

    if (attach->is_url)
	free (attach->u.url.url);
    else if (attach->u.data.free_fn)
	(* attach->u.data.free_fn) (attach->u.data.data, attach->u.data.free_fn_data);

    free (attach);
}

int
icalattach_get_is_url (icalattach *attach)
{
    icalerror_check_arg_rz ((attach != NULL), "attach");

    return attach->is_url ? 1 : 0;
}

const char *
icalattach_get_url (icalattach *attach)
{
    icalerror_check_arg_rz ((attach != NULL), "attach");
    icalerror_check_arg_rz ((attach->is_url), "attach->is_url");

    return attach->u.url.url;
}

unsigned char *
icalattach_get_data (icalattach *attach)
{
    icalerror_check_arg_rz ((attach != NULL), "attach");
    icalerror_check_arg_rz ((!attach->is_url), "!attach->is_url");

    return attach->u.data.data;
}


struct icaltriggertype icaltriggertype_from_string(const char* str)
{

    
    struct icaltriggertype tr, null_tr;
    int old_ieaf = icalerror_errors_are_fatal;

    tr.time= icaltime_null_time();
    tr.duration = icaldurationtype_from_int(0);

    null_tr = tr;

    if(str == 0) goto error;


    icalerror_errors_are_fatal = 0;

    tr.time = icaltime_from_string(str);

    icalerror_errors_are_fatal = old_ieaf;

    if (icaltime_is_null_time(tr.time)){

	tr.duration = icaldurationtype_from_string(str);

	if(icaldurationtype_as_int(tr.duration) == 0) goto error;
    } 

    return tr;

 error:
    icalerror_set_errno(ICAL_MALFORMEDDATA_ERROR);
    return null_tr;

}


struct icalreqstattype icalreqstattype_from_string(char* str)
{
  char *p1,*p2;
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
/*    icalerror_set_errno(ICAL_BADARG_ERROR);*/
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
