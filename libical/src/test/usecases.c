/* -*- Mode: C -*-
  ======================================================================
  FILE: usecases.c
  CREATOR: eric 03 April 1999
  
  DESCRIPTION:
  
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
  The original code is usecases.c

    
  ======================================================================*/

#include "ical.h"
#include <assert.h>
#include <string.h> /* for strdup */
#include <stdlib.h> /* for malloc */
#include <stdio.h> /* for printf */
#include <time.h> /* for time() */
#include "icalmemory.h"

/*

  Here is the example iCal object that the examples routines in this
  file will use:
  
     BEGIN:VCALENDAR
     PRODID:-//RDU Software//NONSGML HandCal//EN
     VERSION:2.0
     BEGIN:VTIMEZONE
     BEGIN:VEVENT
     DTSTAMP:19980309T231000Z
     UID:guid-1.host1.com
     ORGANIZER;ROLE=CHAIR:MAILTO:mrbig@host.com
     ATTENDEE;RSVP=TRUE;ROLE=REQ-PARTICIPANT;CUTYPE=GROUP:
      MAILTO:employee-A@host.com
     DESCRIPTION:Project XYZ Review Meeting
     CREATED:19980309T130000Z
     SUMMARY:XYZ Project Review
     DTSTART;TZID=US-Eastern:19980312T083000
     DTEND;TZID=US-Eastern:19980312T093000
     END:VEVENT
     END:VCALENDAR

*/
char str[] = "BEGIN:VCALENDAR
PRODID:\"-//RDU Software//NONSGML HandCal//EN\"
VERSION:2.0
BEGIN:VEVENT
DTSTAMP:19980309T231000Z
UID:guid-1.host1.com
ORGANIZER;ROLE=CHAIR:MAILTO:mrbig@host.com
ATTENDEE;RSVP=TRUE;ROLE=REQ-PARTICIPANT;CUTYPE=GROUP:MAILTO:employee-A@host.com
DESCRIPTION:Project XYZ Review Meeting
CATEGORIES:MEETING
CREATED:19980309T130000Z
SUMMARY:XYZ Project Review
DTSTART;TZID=US-Eastern:19980312T083000
DTEND;TZID=US-Eastern:19980312T093000
END:VEVENT
END:VCALENDAR";

/* Creating iCal Components 

   There are two ways to create new component in libical. You can
   build the component from primitive parts, or you can create it
   from a string.

   There are two variations of the API for building the component from
   primitive parts. In the first variation, you add each parameter and
   value to a property, and then add each property to a
   component. This results in a long series of function calls. This
   style is show in create_new_component()

   The second variation uses vargs lists to nest many primitive part
   constructors, resulting in a compact, neatly formated way to create
   components. This style is shown in create_new_component_with_va_args()

  
   
*/
   
icalcomponent* create_new_component()
{

    /* variable definitions */
    icalcomponent* calendar;
    icalcomponent* event;
    struct icaltimetype atime = icaltimetype_from_timet( time(0),0);
    struct icalperiodtype rtime;
    icalproperty* property;

    /* Define a time type that will use as data later. */
    rtime.start = icaltimetype_from_timet( time(0),0);
    rtime.end = icaltimetype_from_timet( time(0),0);
    rtime.end.hour++;

    /* Create calendar and add properties */

    calendar = icalcomponent_new(ICAL_VCALENDAR_COMPONENT);
    
    /* Nearly every libical function call has the same general
       form. The first part of the name defines the 'class' for the
       function, and the first argument will be a pointer to a struct
       of that class. So, icalcomponent_ functions will all take
       icalcomponent* as their first argument. */

    /* The next call creates a new proeprty and immediately adds it to the 
       'calendar' component. */ 

    icalcomponent_add_property(
	calendar,
	icalproperty_new_version(strdup("2.0"))
	);

    /* Note the use of strdup() in the previous and next call. All
       properties constructors for properties with value types of
       TEXT will take control of the string you pass into them. Since
       the string '2.0' is a static string, we need to duplicate it in
       new memory before giving it to the property */
    
    /* Here is the short version of the memory rules: 

         If the routine name has "new" in it: 
	     Caller owns the returned memory. 
             If you pass in a string, the routine takes the memory. 	 

         If the routine name has "add" in it:
	     The routine takes control of the component, property, 
	     parameter or value memory.

         If the routine returns a string ( "get" and "as_ical_string" )
	     The library owns the returned memory. 

    */

    icalcomponent_add_property(
	calendar,
	icalproperty_new_prodid(strdup("-//RDU Software//NONSGML HandCal//EN"))
	);
    
    /* Add an event */

    event = icalcomponent_new(ICAL_VEVENT_COMPONENT);

    icalcomponent_add_property(
	event,
	icalproperty_new_dtstamp(atime)
	);

    /* In the previous call, atime is a struct, and it is passed in by value. 
       This is how all compound types of values are handled. */

    icalcomponent_add_property(
	event,
	icalproperty_new_uid(strdup("guid-1.host1.com"))
	);

    /* add a property that has parameters */
    property = icalproperty_new_organizer(strdup("mrbig@host.com"));
    
    icalproperty_add_parameter(
	property,
	icalparameter_new_role(ICAL_ROLE_CHAIR)
	);

    icalcomponent_add_property(event,property);

    /* In this style of component creation, you need to use an extra
       call to add parameters to properties, but the form of this
       operation is the same as adding a property to a component */

    /* add another property that has parameters */
    property = icalproperty_new_attendee(strdup("employee-A@host.com"));
    
    icalproperty_add_parameter(
	property,
	icalparameter_new_role(ICAL_ROLE_REQPARTICIPANT)
	);

    icalproperty_add_parameter(
	property,
	icalparameter_new_rsvp(1)
	);

    icalproperty_add_parameter(
	property,
	icalparameter_new_cutype(ICAL_CUTYPE_GROUP)
	);

    icalcomponent_add_property(event,property);


    /* more properties */

    icalcomponent_add_property(
	event,
	icalproperty_new_description(strdup("Project XYZ Review Meeting"))
	);

    icalcomponent_add_property(
	event,
	icalproperty_new_categories(strdup("MEETING"))
	);

    icalcomponent_add_property(
	event,
	icalproperty_new_class(strdup("PUBLIC"))
	);
    
    icalcomponent_add_property(
	event,
	icalproperty_new_created(atime)
	);

    icalcomponent_add_property(
	event,
	icalproperty_new_summary(strdup("XYZ Project Review"))
	);

    property = icalproperty_new_dtstart(atime);
    
    icalproperty_add_parameter(
	property,
	icalparameter_new_tzid(strdup("US-Eastern"))
	);

    icalcomponent_add_property(event,property);


    property = icalproperty_new_dtend(atime);
    
    icalproperty_add_parameter(
	property,
	icalparameter_new_tzid(strdup("US-Eastern"))
	);

    icalcomponent_add_property(event,property);

    icalcomponent_add_property(
	event,
	icalproperty_new_location(strdup("1CP Conference Room 4350"))
	);

    icalcomponent_add_component(calendar,event);

    return calendar;
}


/* Now, create the same component as in the previous routine, but use
the constructor style. */

icalcomponent* create_new_component_with_va_args()
{

    /* This is a similar set up to the last routine */
    icalcomponent* calendar;
    struct icaltimetype atime = icaltimetype_from_timet( time(0),0);
    struct icalperiodtype rtime;
    
    rtime.start = icaltimetype_from_timet( time(0),0);
    rtime.end = icaltimetype_from_timet( time(0),0);
    rtime.end.hour++;

    /* Some of these routines are the same as those in the previous
       routine, but we've also added several 'vanew' routines. These
       'vanew' routines take a list of properties, parameters or
       values and add each of them to the parent property or
       component. */

    calendar = 
	icalcomponent_vanew(
	    ICAL_VCALENDAR_COMPONENT,
	    icalproperty_new_version(strdup("2.0")),
	    icalproperty_new_prodid(strdup("-//RDU Software//NONSGML HandCal//EN")),
	    icalcomponent_vanew(
		ICAL_VEVENT_COMPONENT,
		icalproperty_new_dtstamp(atime),
		icalproperty_new_uid(strdup("guid-1.host1.com")),
		icalproperty_vanew_organizer(
		    strdup("mrbig@host.com"),
		    icalparameter_new_role(ICAL_ROLE_CHAIR),
		    0
		    ),
		icalproperty_vanew_attendee(
		    strdup("employee-A@host.com"),
		    icalparameter_new_role(ICAL_ROLE_REQPARTICIPANT),
		    icalparameter_new_rsvp(1),
		    icalparameter_new_cutype(ICAL_CUTYPE_GROUP),
		    0
		    ),
		icalproperty_new_description(strdup("Project XYZ Review Meeting")),
		icalproperty_new_categories(strdup("MEETING")),
		icalproperty_new_class(strdup("PUBLIC")),
		icalproperty_new_created(atime),
		icalproperty_new_summary(strdup("XYZ Project Review")),
		icalproperty_vanew_dtstart(
		    atime,
		    icalparameter_new_tzid(strdup("US-Eastern")),
		    0
		    ),
		icalproperty_vanew_dtend(
		    atime,
		    icalparameter_new_tzid(strdup("US-Eastern")),
		    0
		    ),
		icalproperty_new_location(strdup("1CP Conference Room 4350")),
		0
		),
	    0
	    );

   
    /* Note that properties with no parameters can use the regular
       'new' constructor, while those with parameters use the 'vanew'
       constructor. And, be sure that the last argument in the 'vanew'
       call is a zero. Without, your program will probably crash. */

    return calendar;
}


/* Now, lets try to get a particular parameter out of a
   component. This routine will return a list of strings of all
   attendees who are required. Note that this routine assumes that the
   component that we pass in is a VEVENT; the top level component we
   created in the above two routines is a VCALENDAR */
   
char *attendees[10];
#define MAX_ATTENDEES 10;

char** get_required_attendees(icalcomponent* event)
{
    icalproperty* p;
    icalparameter* parameter;
    int c=0;

    assert(event != 0);
    assert(icalcomponent_isa(event) == ICAL_VEVENT_COMPONENT);
    
    /* This loop iterates over all of the ATTENDEE properties in the
       event */
    
    /* Yes, the iteration routines save their state in the event
       struct, so the are not thread safe unless you lock the whole
       event. */

    for(
	p = icalcomponent_get_first_property(event,ICAL_ATTENDEE_PROPERTY);
	p != 0;
	p = icalcomponent_get_next_property(event,ICAL_ATTENDEE_PROPERTY)
	) {
	
	/* Get the first ROLE parameter in the property. There should
           only be one, so we wont bother to iterate over them. */

	parameter = icalproperty_get_first_parameter(p,ICAL_ROLE_PARAMETER);

	/* If the parameter indicates the participant is required, get
           the attendees name and stick a copy of it into the output
           array */

	if ( icalparameter_get_role(parameter) == ICAL_ROLE_REQPARTICIPANT) 
	{
	    attendees[c++] = strdup(icalproperty_get_attendee(p));
	}
    }

    return attendees;
}

/* Here is a similar example. If an attendee has a PARTSTAT of
   NEEDSACTION or has no PARTSTAT parameter, change it to
   TENTATIVE. */
   
void update_attendees(icalcomponent* event)
{
    icalproperty* p;
    icalparameter* parameter;

    assert(event != 0);
    assert(icalcomponent_isa(event) == ICAL_VEVENT_COMPONENT);
    
    for(
	p = icalcomponent_get_first_property(event,ICAL_ATTENDEE_PROPERTY);
	p != 0;
	p = icalcomponent_get_next_property(event,ICAL_ATTENDEE_PROPERTY)
	) {
	
	parameter = icalproperty_get_first_parameter(p,ICAL_PARTSTAT_PARAMETER);

	if (parameter == 0) {

	    /* There was no PARTSTAT parameter, so add one.  */
	    icalproperty_add_parameter(
		p,
		icalparameter_new_partstat(ICAL_PARTSTAT_TENTATIVE)
		);

	} else if (icalparameter_get_partstat(parameter) == ICAL_PARTSTAT_NEEDSACTION) {
	    /* Remove the NEEDSACTION parameter and replace it with
               TENTATIVE */
	    
	    icalproperty_remove_parameter(p,ICAL_PARTSTAT_PARAMETER);
	    
	    /* Don't forget to free it */
	    icalparameter_free(parameter);

	    /* Add a new one */
	    icalproperty_add_parameter(
		p,
		icalparameter_new_partstat(ICAL_PARTSTAT_TENTATIVE)
		);
	}

    }
}

/* Here are some examples of manipulating properties */

void test_properties()
{
    icalproperty *prop;
    icalparameter *param;
    icalvalue *value;

    icalproperty *clone;

    /* Create a new property */
    prop = icalproperty_vanew_comment(
	strdup("Another Comment"),
	icalparameter_new_cn("A Common Name 1"),
	icalparameter_new_cn("A Common Name 2"),
	icalparameter_new_cn("A Common Name 3"),
       	icalparameter_new_cn("A Common Name 4"),
	0); 

    /* Iterate through all of the parameters in the property */
    for(param = icalproperty_get_first_parameter(prop,ICAL_ANY_PROPERTY);
	param != 0; 
	param = icalproperty_get_next_parameter(prop,ICAL_ANY_PROPERTY)) {
						
	printf("Prop parameter: %s\n",icalparameter_get_cn(param));
    }    

    /* Get a string representation of the property's value */
    printf("Prop value: %s\n",icalproperty_get_comment(prop));

    /* Spit out the property in its RFC 2445 representation */
    printf("As iCAL string:\n %s\n",icalproperty_as_ical_string(prop));
    
    /* Make a copy of the property. Caller owns the memory */
    clone = icalproperty_new_clone(prop);

    /* Get a reference to the value within the clone property */
    value = icalproperty_get_value(clone);

    printf("Value: %s",icalvalue_as_ical_string(value));

    /* Free the original and the clone */
    icalproperty_free(clone);
    icalproperty_free(prop);

}



/* Here are some ways to work with values. */
void test_values()
{
    icalvalue *v; 
    icalvalue *copy; 

    v = icalvalue_new_caladdress(strdup("cap://value/1"));
    printf("caladdress 1: %s\n",icalvalue_get_caladdress(v));

    icalvalue_set_caladdress(v,strdup("cap://value/2"));
    printf("caladdress 2: %s\n",icalvalue_get_caladdress(v));
    printf("String: %s\n",icalvalue_as_ical_string(v));
    
    copy = icalvalue_new_clone(v);
    printf("Clone: %s\n",icalvalue_as_ical_string(v));
    icalvalue_free(v);
    icalvalue_free(copy);


}

void test_parameters()
{
    icalparameter *p;

    p = icalparameter_new_cn("A Common Name");

    printf("Common Name: %s\n",icalparameter_get_cn(p));

    printf("As String: %s\n",icalparameter_as_ical_string(p));
}


int test_parser()
{


    icalcomponent *c = icalparser_parse_string(str);
    printf("%s\n",icalcomponent_as_ical_string(c));
    icalcomponent_free(c);
    icalmemory_free_ring();
    return 1;
}


int main(int argc, char *argv[])
{
    icalcomponent *c1;
    icalcomponent *c2;
    icalcomponent *vevent;
    char **attendees;

    c1 = create_new_component();
    c2 = create_new_component_with_va_args();

    /* Extract the VEVENT component from the component */

    vevent = icalcomponent_get_first_component(c1,ICAL_VEVENT_COMPONENT);

    attendees = get_required_attendees(vevent);

    printf("Attendees: %s\n",attendees[0]);

    /* Now print out the component as a string. Remember that the
       library retains control of the memory returned by
       icalcomponent_as_ical_string. Do not sotre references to it or
       try to free it. It is stored on an internal ring buffer,and the
       library will eventuall reclaim it. */

    printf("%s\n",icalcomponent_as_ical_string(c1));

    return 0;

}



