/* -*- Mode: C -*-
  ======================================================================
  FILE: icalerror.c
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
  The original code is icalerror.c

 ======================================================================*/

#include "icalerror.h"

icalerrorenum icalerrno;

int foo;
void icalerror_stop_here(void)
{
    foo++; /* Keep optimizers from removing routine */
}

void icalerror_clear_errno() {
    
    icalerrno = ICAL_NO_ERROR;
}

void icalerror_set_errno(icalerrorenum e) {

    icalerror_stop_here();
    icalerrno = e;
}


struct icalerror_string_map {
	icalerrorenum error;
	char name[160];
};

static struct icalerror_string_map string_map[] = 
{
    {ICAL_BADARG_ERROR,"Bad argumnet to function"},
    {ICAL_NEWFAILED_ERROR,"Failed to create a new object via a *_new() routine"},
    {ICAL_MALFORMEDDATA_ERROR,"An input string was not correctly formed"},
    {ICAL_PARSE_ERROR,"Failed to parse a part of an iCal componet"},
    {ICAL_INTERNAL_ERROR,"Random internal error. This indicates an error in the library code, not an error in use"}, 
    {ICAL_FILE_ERROR,"An operation on a file failed. Check errno for more detail."},
    {ICAL_ALLOCATION_ERROR,"Failed to allocate memory"},
    {ICAL_USAGE_ERROR,"The caller failed to properly sequence called to an object's interface"},
    {ICAL_NO_ERROR,"No error"},
    {ICAL_UNKNOWN_ERROR,"Unknown error type -- icalerror_strerror() was probably given bad input"}
};


char* icalerror_strerror(icalerrorenum e) {

    int i;

    for (i=0; string_map[i].error != ICAL_UNKNOWN_ERROR; i++) {
	if (string_map[i].error == e) {
	    return string_map[i].name;
	}
    }

    return string_map[i].name; /* Return string for ICAL_UNKNOWN_ERROR*/
    
}



