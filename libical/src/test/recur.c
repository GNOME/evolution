/* -*- Mode: C -*-
  ======================================================================
  FILE: recur.c
  CREATOR: ebusboom 8jun00
  
  DESCRIPTION:

  Test program for expanding recurrences. Run as:

     ./recur ../../test-data/recur.txt

  
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
  The original code is usecases.c

    
  ======================================================================*/

#include "ical.h"
#include <assert.h>
#include <string.h> /* for strdup */
#include <stdlib.h> /* for malloc */
#include <stdio.h> /* for printf */
#include <time.h> /* for time() */
#include "icalmemory.h"
#include "icaldirset.h"
#include "icalfileset.h"

int main(int argc, char *argv[])
{
    icalfileset *cin;
    struct icaltimetype start, next;
    icalcomponent *itr;
    icalproperty *desc, *dtstart, *rrule;
    struct icalrecurrencetype recur;
    icalrecur_iterator* ritr;
    time_t tt;

    cin = icalfileset_new(argv[1]);
    assert(cin != 0);

    for (itr = icalfileset_get_first_component(cin,
                                               ICAL_ANY_COMPONENT);
         itr != 0;
         itr = icalfileset_get_next_component(cin,
                                              ICAL_ANY_COMPONENT)){ 

	desc = icalcomponent_get_first_property(itr,ICAL_DESCRIPTION_PROPERTY);
	assert(desc !=0);

	dtstart = icalcomponent_get_first_property(itr,ICAL_DTSTART_PROPERTY);
	assert(dtstart !=0);

	rrule = icalcomponent_get_first_property(itr,ICAL_RRULE_PROPERTY);
	assert(rrule !=0);


	recur = icalproperty_get_rrule(rrule);
	start = icalproperty_get_dtstart(dtstart);
	
	ritr = icalrecur_iterator_new(recur,start);
	
	tt = icaltime_as_timet(start);

	printf("\n\n#### %s\n",icalproperty_get_description(desc));
	printf("#### %s\n",icalvalue_as_ical_string(icalproperty_get_value(rrule)));
	printf("#### %s\n",ctime(&tt ));

	for(ritr = icalrecur_iterator_new(recur,start),
		next = icalrecur_iterator_next(ritr); 
	    !icaltime_is_null_time(next);
	    next = icalrecur_iterator_next(ritr)){
	    
	    tt = icaltime_as_timet(next);
	    
	    printf("  %s",ctime(&tt ));		
	    
	}

    }

    return 0;
}
