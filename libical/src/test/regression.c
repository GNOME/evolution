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
#include "icalstore.h"
#include "icalcluster.h"
#include "icalerror.h"
#include "icalrestriction.h"
#include "icalcalendar.h"

/* This example creates and minipulates the ical object that appears
 * in rfc 2445, page 137 */

char str[] = "BEGIN:VCALENDAR\
PRODID:\"-//RDU Software//NONSGML HandCal//EN\"\
VERSION:2.0\
BEGIN:VTIMEZONE\
TZID:US-Eastern\
BEGIN:STANDARD\
DTSTART:19981025T020000\
RDATE:19981025T020000\
TZOFFSETFROM:-0400\
TZOFFSETTO:-0500\
TZNAME:EST\
END:STANDARD\
BEGIN:DAYLIGHT\
DTSTART:19990404T020000\
RDATE:19990404T020000\
TZOFFSETFROM:-0500\
TZOFFSETTO:-0400\
TZNAME:EDT\
END:DAYLIGHT\
END:VTIMEZONE\
BEGIN:VEVENT\
DTSTAMP:19980309T231000Z\
UID:guid-1.host1.com\
ORGANIZER;ROLE=CHAIR:MAILTO:mrbig@host.com\
ATTENDEE;RSVP=TRUE;ROLE=REQ-PARTICIPANT;CUTYPE=GROUP:MAILTO:employee-A@host.com\
DESCRIPTION:Project XYZ Review Meeting\
CATEGORIES:MEETING\
CLASS:PUBLIC\
CREATED:19980309T130000Z\
SUMMARY:XYZ Project Review\
DTSTART;TZID=US-Eastern:19980312T083000\
DTEND;TZID=US-Eastern:19980312T093000\
LOCATION:1CP Conference Room 4350\
END:VEVENT\
BEGIN:BOOGA\
DTSTAMP:19980309T231000Z\
X-LIC-FOO:Booga\
DTSTOMP:19980309T231000Z\
UID:guid-1.host1.com\
END:BOOGA\
END:VCALENDAR";


icalcomponent* create_simple_component()
{

    icalcomponent* calendar;
    struct icalperiodtype rtime;

    rtime.start = icaltimetype_from_timet( time(0),0);
    rtime.end = icaltimetype_from_timet( time(0),0);

    rtime.end.hour++;



    /* Create calendar and add properties */
    calendar = icalcomponent_new(ICAL_VCALENDAR_COMPONENT);

    
    icalcomponent_add_property(
	calendar,
	icalproperty_new_version("2.0")
	);

    printf("%s\n",icalcomponent_as_ical_string(calendar));
 
    return calendar;
	   
}

/* Create a new component */
icalcomponent* create_new_component()
{

    icalcomponent* calendar;
    icalcomponent* timezone;
    icalcomponent* tzc;
    icalcomponent* event;
    struct icaltimetype atime = icaltimetype_from_timet( time(0),0);
    struct icalperiodtype rtime;
    icalproperty* property;

    rtime.start = icaltimetype_from_timet( time(0),0);
    rtime.end = icaltimetype_from_timet( time(0),0);

    rtime.end.hour++;



    /* Create calendar and add properties */
    calendar = icalcomponent_new(ICAL_VCALENDAR_COMPONENT);

    
    icalcomponent_add_property(
	calendar,
	icalproperty_new_version("2.0")
	);
    
    icalcomponent_add_property(
	calendar,
	icalproperty_new_prodid("-//RDU Software//NONSGML HandCal//EN")
	);
    
    /* Create a timezone object and add it to the calendar */

    timezone = icalcomponent_new(ICAL_VTIMEZONE_COMPONENT);

    icalcomponent_add_property(
	timezone,
	icalproperty_new_tzid("US_Eastern")
	);

    /* Add a sub-component of the timezone */
    tzc = icalcomponent_new(ICAL_XDAYLIGHT_COMPONENT);

    icalcomponent_add_property(
	tzc, 
	icalproperty_new_dtstart(atime)
	);

    icalcomponent_add_property(
	tzc, 
	icalproperty_new_rdate(rtime)
	);
	    
    icalcomponent_add_property(
	tzc, 
	icalproperty_new_tzoffsetfrom(-4.0)
	);

    icalcomponent_add_property(
	tzc, 
	icalproperty_new_tzoffsetto(-5.0)
	);

    icalcomponent_add_property(
	tzc, 
	icalproperty_new_tzname("EST")
	);

    icalcomponent_add_component(timezone,tzc);

    icalcomponent_add_component(calendar,timezone);

    /* Add a second subcomponent */
    tzc = icalcomponent_new(ICAL_XSTANDARD_COMPONENT);

    icalcomponent_add_property(
	tzc, 
	icalproperty_new_dtstart(atime)
	);

    icalcomponent_add_property(
	tzc, 
	icalproperty_new_rdate(rtime)
	);
	    
    icalcomponent_add_property(
	tzc, 
	icalproperty_new_tzoffsetfrom(-4.0)
	);

    icalcomponent_add_property(
	tzc, 
	icalproperty_new_tzoffsetto(-5.0)
	);

    icalcomponent_add_property(
	tzc, 
	icalproperty_new_tzname("EST")
	);

    icalcomponent_add_component(timezone,tzc);

    /* Add an event */

    event = icalcomponent_new(ICAL_VEVENT_COMPONENT);

    icalcomponent_add_property(
	event,
	icalproperty_new_dtstamp(atime)
	);

    icalcomponent_add_property(
	event,
	icalproperty_new_uid("guid-1.host1.com")
	);

    /* add a property that has parameters */
    property = icalproperty_new_organizer("mrbig@host.com");
    
    icalproperty_add_parameter(
	property,
	icalparameter_new_role(ICAL_ROLE_CHAIR)
	);

    icalcomponent_add_property(event,property);

    /* add another property that has parameters */
    property = icalproperty_new_attendee("employee-A@host.com");
    
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
	icalproperty_new_description("Project XYZ Review Meeting")
	);

    icalcomponent_add_property(
	event,
	icalproperty_new_categories("MEETING")
	);

    icalcomponent_add_property(
	event,
	icalproperty_new_class("PUBLIC")
	);
    
    icalcomponent_add_property(
	event,
	icalproperty_new_created(atime)
	);

    icalcomponent_add_property(
	event,
	icalproperty_new_summary("XYZ Project Review")
	);


    property = icalproperty_new_dtstart(atime);
    
    icalproperty_add_parameter(
	property,
	icalparameter_new_tzid("US-Eastern")
	);

    icalcomponent_add_property(event,property);


    property = icalproperty_new_dtend(atime);
    
    icalproperty_add_parameter(
	property,
	icalparameter_new_tzid("US-Eastern")
	);

    icalcomponent_add_property(event,property);

    icalcomponent_add_property(
	event,
	icalproperty_new_location("1CP Conference Room 4350")
	);

    icalcomponent_add_component(calendar,event);

    printf("%s\n",icalcomponent_as_ical_string(calendar));

    icalcomponent_free(calendar);

    return 0;
}


/* Create a new component, using the va_args list */

icalcomponent* create_new_component_with_va_args()
{

    icalcomponent* calendar;
    struct icaltimetype atime = icaltimetype_from_timet( time(0),0);
    struct icalperiodtype rtime;
    
    rtime.start = icaltimetype_from_timet( time(0),0);
    rtime.end = icaltimetype_from_timet( time(0),0);

    rtime.end.hour++;

    calendar = 
	icalcomponent_vanew(
	    ICAL_VCALENDAR_COMPONENT,
	    icalproperty_new_version("2.0"),
	    icalproperty_new_prodid("-//RDU Software//NONSGML HandCal//EN"),
	    icalcomponent_vanew(
		ICAL_VTIMEZONE_COMPONENT,
		icalproperty_new_tzid("US_Eastern"),
		icalcomponent_vanew(
		    ICAL_XDAYLIGHT_COMPONENT,
		    icalproperty_new_dtstart(atime),
		    icalproperty_new_rdate(rtime),
		    icalproperty_new_tzoffsetfrom(-4.0),
		    icalproperty_new_tzoffsetto(-5.0),
		    icalproperty_new_tzname("EST"),
		    0
		    ),
		icalcomponent_vanew(
		    ICAL_XSTANDARD_COMPONENT,
		    icalproperty_new_dtstart(atime),
		    icalproperty_new_rdate(rtime),
		    icalproperty_new_tzoffsetfrom(-5.0),
		    icalproperty_new_tzoffsetto(-4.0),
		    icalproperty_new_tzname("EST"),
		    0
		    ),
		0
		),
	    icalcomponent_vanew(
		ICAL_VEVENT_COMPONENT,
		icalproperty_new_dtstamp(atime),
		icalproperty_new_uid("guid-1.host1.com"),
		icalproperty_vanew_organizer(
		    "mrbig@host.com",
		    icalparameter_new_role(ICAL_ROLE_CHAIR),
		    0
		    ),
		icalproperty_vanew_attendee(
		    "employee-A@host.com",
		    icalparameter_new_role(ICAL_ROLE_REQPARTICIPANT),
		    icalparameter_new_rsvp(1),
		    icalparameter_new_cutype(ICAL_CUTYPE_GROUP),
		    0
		    ),
		icalproperty_new_description("Project XYZ Review Meeting"),
		icalproperty_new_categories("MEETING"),
		icalproperty_new_class("PUBLIC"),
		icalproperty_new_created(atime),
		icalproperty_new_summary("XYZ Project Review"),
		icalproperty_vanew_dtstart(
		    atime,
		    icalparameter_new_tzid("US-Eastern"),
		    0
		    ),
		icalproperty_vanew_dtend(
		    atime,
		    icalparameter_new_tzid("US-Eastern"),
		    0
		    ),
		icalproperty_new_location("1CP Conference Room 4350"),
		0
		),
	    0
	    );
	
    printf("%s\n",icalcomponent_as_ical_string(calendar));
    

    icalcomponent_free(calendar);

    return 0;
}


/* Return a list of all attendees who are required. */
   
char** get_required_attendees(icalproperty* event)
{
    icalproperty* p;
    icalparameter* parameter;

    char **attendees;
    int max = 10;
    int c = 0;

    attendees = malloc(max * (sizeof (char *)));

    assert(event != 0);
    assert(icalcomponent_isa(event) == ICAL_VEVENT_COMPONENT);
    
    for(
	p = icalcomponent_get_first_property(event,ICAL_ATTENDEE_PROPERTY);
	p != 0;
	p = icalcomponent_get_next_property(event,ICAL_ATTENDEE_PROPERTY)
	) {
	
	parameter = icalproperty_get_first_parameter(p,ICAL_ROLE_PARAMETER);

	if ( icalparameter_get_role(parameter) == ICAL_ROLE_REQPARTICIPANT) 
	{
	    attendees[c++] = strdup(icalproperty_get_attendee(p));

            if (c >= max) {
                max *= 2; 
                attendees = realloc(attendees, max * (sizeof (char *)));
            }

	}
    }

    return attendees;
}

/* If an attendee has a PARTSTAT of NEEDSACTION or has no PARTSTAT
   parameter, change it to TENTATIVE. */
   
void update_attendees(icalproperty* event)
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

	    icalproperty_add_parameter(
		p,
		icalparameter_new_partstat(ICAL_PARTSTAT_TENTATIVE)
		);

	} else if (icalparameter_get_partstat(parameter) == ICAL_PARTSTAT_NEEDSACTION) {

	    icalproperty_remove_parameter(p,ICAL_PARTSTAT_PARAMETER);
	    
	    icalparameter_free(parameter);

	    icalproperty_add_parameter(
		p,
		icalparameter_new_partstat(ICAL_PARTSTAT_TENTATIVE)
		);
	}

    }
}


void test_values()
{
    icalvalue *v; 
    icalvalue *copy; 

    v = icalvalue_new_caladdress("cap://value/1");
    printf("caladdress 1: %s\n",icalvalue_get_caladdress(v));
    icalvalue_set_caladdress(v,"cap://value/2");
    printf("caladdress 2: %s\n",icalvalue_get_caladdress(v));
    printf("String: %s\n",icalvalue_as_ical_string(v));
    
    copy = icalvalue_new_clone(v);
    printf("Clone: %s\n",icalvalue_as_ical_string(v));
    icalvalue_free(v);
    icalvalue_free(copy);


    v = icalvalue_new_boolean(1);
    printf("caladdress 1: %d\n",icalvalue_get_boolean(v));
    icalvalue_set_boolean(v,2);
    printf("caladdress 2: %d\n",icalvalue_get_boolean(v));
    printf("String: %s\n",icalvalue_as_ical_string(v));

    copy = icalvalue_new_clone(v);
    printf("Clone: %s\n",icalvalue_as_ical_string(v));
    icalvalue_free(v);
    icalvalue_free(copy);


    v = icalvalue_new_date(icaltimetype_from_timet( time(0),0));
    printf("date 1: %s\n",icalvalue_as_ical_string(v));
    icalvalue_set_date(v,icaltimetype_from_timet( time(0)+3600,0));
    printf("date 2: %s\n",icalvalue_as_ical_string(v));

    copy = icalvalue_new_clone(v);
    printf("Clone: %s\n",icalvalue_as_ical_string(v));
    icalvalue_free(v);
    icalvalue_free(copy);


    v = icalvalue_new(-1);

    printf("Invalid type: %p\n",v);

    if (v!=0) icalvalue_free(v);


    /*    v = icalvalue_new_caladdress(0);

    printf("Bad string: %p\n",v);

    if (v!=0) icalvalue_free(v); */

}

void test_properties()
{
    icalproperty *prop;
    icalparameter *param;

    icalproperty *clone;

    prop = icalproperty_vanew_comment(
	"Another Comment",
	icalparameter_new_cn("A Common Name 1"),
	icalparameter_new_cn("A Common Name 2"),
	icalparameter_new_cn("A Common Name 3"),
       	icalparameter_new_cn("A Common Name 4"),
	0); 

    for(param = icalproperty_get_first_parameter(prop,ICAL_ANY_PARAMETER);
	param != 0; 
	param = icalproperty_get_next_parameter(prop,ICAL_ANY_PARAMETER)) {
						
	printf("Prop parameter: %s\n",icalparameter_get_cn(param));
    }    

    printf("Prop value: %s\n",icalproperty_get_comment(prop));


    printf("As iCAL string:\n %s\n",icalproperty_as_ical_string(prop));
    
    clone = icalproperty_new_clone(prop);

    printf("Clone:\n %s\n",icalproperty_as_ical_string(prop));
    
    icalproperty_free(clone);
    icalproperty_free(prop);

    prop = icalproperty_new(-1);

    printf("Invalid type: %p\n",prop);

    if (prop!=0) icalproperty_free(prop);

    /*
    prop = icalproperty_new_method(0);

    printf("Bad string: %p\n",prop);
   

    if (prop!=0) icalproperty_free(prop);
    */
}

void test_parameters()
{
    icalparameter *p;

    p = icalparameter_new_cn("A Common Name");

    printf("Common Name: %s\n",icalparameter_get_cn(p));

    printf("As String: %s\n",icalparameter_as_ical_string(p));

    icalparameter_free(p);
}


void test_components()
{

    icalcomponent* c;
    icalcomponent* child;

    c = icalcomponent_vanew(
	ICAL_VCALENDAR_COMPONENT,
	icalproperty_new_version("2.0"),
	icalproperty_new_prodid("-//RDU Software//NONSGML HandCal//EN"),
	icalproperty_vanew_comment(
	    "A Comment",
	    icalparameter_new_cn("A Common Name 1"),
	    0),
	icalcomponent_vanew(
	    ICAL_VEVENT_COMPONENT,
	    icalproperty_new_version("2.0"),
	    icalproperty_new_description("This is an event"),
	    icalproperty_vanew_comment(
		"Another Comment",
		icalparameter_new_cn("A Common Name 1"),
		icalparameter_new_cn("A Common Name 2"),
		icalparameter_new_cn("A Common Name 3"),
		icalparameter_new_cn("A Common Name 4"),
		0),
	    icalproperty_vanew_xlicerror(
		"This is only a test",
		icalparameter_new_xlicerrortype(ICAL_XLICERRORTYPE_COMPONENTPARSEERROR),
		0),
	    
	    0
	    ),
	0
	);

    printf("Original Component:\n%s\n\n",icalcomponent_as_ical_string(c));

    child = icalcomponent_get_first_component(c,ICAL_VEVENT_COMPONENT);

    printf("Child Component:\n%s\n\n",icalcomponent_as_ical_string(child));
    
    icalcomponent_free(c);

}

void test_memory()
{
    size_t bufsize = 256;
    char *p;

    char S1[] = "1) When in the Course of human events, ";
    char S2[] = "2) it becomes necessary for one people to dissolve the political bands which have connected them with another, ";
    char S3[] = "3) and to assume among the powers of the earth, ";
    char S4[] = "4) the separate and equal station to which the Laws of Nature and of Nature's God entitle them, ";
    char S5[] = "5) a decent respect to the opinions of mankind requires that they ";
    char S6[] = "6) should declare the causes which impel them to the separation. ";
    char S7[] = "7) We hold these truths to be self-evident, ";
    char S8[] = "8) that all men are created equal, ";

/*    char S9[] = "9) that they are endowed by their Creator with certain unalienable Rights, ";
    char S10[] = "10) that among these are Life, Liberty, and the pursuit of Happiness. ";
    char S11[] = "11) That to secure these rights, Governments are instituted among Men, ";
    char S12[] = "12) deriving their just powers from the consent of the governed. "; 
*/


    char *f, *b1, *b2, *b3, *b4, *b5, *b6, *b7, *b8;

    #define BUFSIZE 1024


    f = icalmemory_new_buffer(bufsize);
    p = f;
    b1 = icalmemory_tmp_buffer(BUFSIZE);
    strcpy(b1, S1);
    icalmemory_append_string(&f, &p, &bufsize, b1);

    b2 = icalmemory_tmp_buffer(BUFSIZE);
    strcpy(b2, S2);
    icalmemory_append_string(&f, &p, &bufsize, b2);

    b3 = icalmemory_tmp_buffer(BUFSIZE);
    strcpy(b3, S3);
    icalmemory_append_string(&f, &p, &bufsize, b3);

    b4 = icalmemory_tmp_buffer(BUFSIZE);
    strcpy(b4, S4);
    icalmemory_append_string(&f, &p, &bufsize, b4);

    b5 = icalmemory_tmp_buffer(BUFSIZE);
    strcpy(b5, S5);
    icalmemory_append_string(&f, &p, &bufsize, b5);

    b6 = icalmemory_tmp_buffer(BUFSIZE);
    strcpy(b6, S6);
    icalmemory_append_string(&f, &p, &bufsize, b6);

    b7 = icalmemory_tmp_buffer(BUFSIZE);
    strcpy(b7, S7);
    icalmemory_append_string(&f, &p, &bufsize, b7);

    b8 = icalmemory_tmp_buffer(BUFSIZE);
    strcpy(b8, S8);
    icalmemory_append_string(&f, &p, &bufsize, b8);


    printf("1: %p %s \n",b1,b1);
    printf("2: %p %s\n",b2,b2);
    printf("3: %p %s\n",b3,b3);
    printf("4: %p %s\n",b4,b4);
    printf("5: %p %s\n",b5,b5);
    printf("6: %p %s\n",b6,b6);
    printf("7: %p %s\n",b7,b7);
    printf("8: %p %s\n",b8,b8);

    
    printf("Final: %s\n", f);

    printf("Final buffer size: %d\n",bufsize);

    free(f);
    
    bufsize = 4;
    f = icalmemory_new_buffer(bufsize);
    p = f;

    icalmemory_append_char(&f, &p, &bufsize, 'a');
    icalmemory_append_char(&f, &p, &bufsize, 'b');
    icalmemory_append_char(&f, &p, &bufsize, 'c');
    icalmemory_append_char(&f, &p, &bufsize, 'd');
    icalmemory_append_char(&f, &p, &bufsize, 'e');
    icalmemory_append_char(&f, &p, &bufsize, 'f');
    icalmemory_append_char(&f, &p, &bufsize, 'g');
    icalmemory_append_char(&f, &p, &bufsize, 'h');
    icalmemory_append_char(&f, &p, &bufsize, 'i');
    icalmemory_append_char(&f, &p, &bufsize, 'j');
    icalmemory_append_char(&f, &p, &bufsize, 'a');
    icalmemory_append_char(&f, &p, &bufsize, 'b');
    icalmemory_append_char(&f, &p, &bufsize, 'c');
    icalmemory_append_char(&f, &p, &bufsize, 'd');
    icalmemory_append_char(&f, &p, &bufsize, 'e');
    icalmemory_append_char(&f, &p, &bufsize, 'f');
    icalmemory_append_char(&f, &p, &bufsize, 'g');
    icalmemory_append_char(&f, &p, &bufsize, 'h');
    icalmemory_append_char(&f, &p, &bufsize, 'i');
    icalmemory_append_char(&f, &p, &bufsize, 'j');

    printf("Char-by-Char buffer: %s\n", f);

}


int test_store()
{

    icalcomponent *c, *gauge;
    icalerrorenum error;
    icalcomponent *next, *itr;
    icalcluster* cluster;
    struct icalperiodtype rtime;
    icalstore *s = icalstore_new("store");
    int i;

    rtime.start = icaltimetype_from_timet( time(0),0);

    cluster = icalcluster_new("clusterin.vcd");

    if (cluster == 0){
	printf("Failed to create cluster: %s\n",icalerror_strerror(icalerrno));
	return 0;
    }

#define NUMCOMP 4

    /* Duplicate every component in the cluster NUMCOMP times */

    icalerror_clear_errno();

    for (i = 1; i<NUMCOMP+1; i++){

	/*rtime.start.month = i%12;*/
	rtime.start.month = i;
	rtime.end = rtime.start;
	rtime.end.hour++;
	
	for (itr = icalcluster_get_first_component(cluster,
						   ICAL_ANY_COMPONENT);
	     itr != 0;
	     itr = icalcluster_get_next_component(cluster,
						  ICAL_ANY_COMPONENT)){
	    icalcomponent *clone;
	    icalproperty *p;

	    
	    if(icalcomponent_isa(itr) != ICAL_VEVENT_COMPONENT){
		continue;
	    }

	    assert(itr != 0);
	    
	    /* Change the dtstart and dtend times in the component
               pointed to by Itr*/

	    clone = icalcomponent_new_clone(itr);
	    assert(icalerrno == ICAL_NO_ERROR);
	    assert(clone !=0);

	    /* DTSTART*/
	    p = icalcomponent_get_first_property(clone,ICAL_DTSTART_PROPERTY);
	    assert(icalerrno  == ICAL_NO_ERROR);

	    if (p == 0){
		p = icalproperty_new_dtstart(rtime.start);
		icalcomponent_add_property(clone,p);
	    } else {
		icalproperty_set_dtstart(p,rtime.start);
	    }
	    assert(icalerrno  == ICAL_NO_ERROR);

	    /* DTEND*/
	    p = icalcomponent_get_first_property(clone,ICAL_DTEND_PROPERTY);
	    assert(icalerrno  == ICAL_NO_ERROR);

	    if (p == 0){
		p = icalproperty_new_dtstart(rtime.end);
		icalcomponent_add_property(clone,p);
	    } else {
		icalproperty_set_dtstart(p,rtime.end);
	    }
	    assert(icalerrno  == ICAL_NO_ERROR);
	    
	    printf("\n----------\n%s\n---------\n",icalcomponent_as_ical_string(clone));

	    error = icalstore_add_component(s,clone);
	    
	    assert(icalerrno  == ICAL_NO_ERROR);

	}

    }
    
    gauge = 
	icalcomponent_vanew(
	    ICAL_VCALENDAR_COMPONENT,
	    icalcomponent_vanew(
		ICAL_VEVENT_COMPONENT,  
		icalproperty_vanew_summary(
		    "Submit Income Taxes",
		    icalparameter_new_xliccomparetype(ICAL_XLICCOMPARETYPE_EQUAL),
		    0),
		0),
	    icalcomponent_vanew(
		ICAL_VEVENT_COMPONENT,  
		icalproperty_vanew_summary(
		    "Bastille Day Party",
		    icalparameter_new_xliccomparetype(ICAL_XLICCOMPARETYPE_EQUAL),
		    0),
		0),
	    0);

#if 0


    icalstore_select(s,gauge);

    for(c = icalstore_first(s); c != 0; c = icalstore_next(s)){
	
	printf("Got one! (%d)\n", count++);
	
	if (c != 0){
	    printf("%s", icalcomponent_as_ical_string(c));;
	    if (icalstore_store(s2,c) == 0){
		printf("Failed to write!\n");
	    }
	    icalcomponent_free(c);
	} else {
	    printf("Failed to get component\n");
	}
    }


    icalstore_free(s2);
#endif


    for(c = icalstore_get_first_component(s); 
	c != 0; 
	c =  next){
	
	next = icalstore_get_next_component(s);

	if (c != 0){
	    /*icalstore_remove_component(s,c);*/
	    printf("%s", icalcomponent_as_ical_string(c));;
	} else {
	    printf("Failed to get component\n");
	}


    }

    icalstore_free(s);
    return 0;
}

int test_compare()
{
    icalvalue *v1, *v2;
    icalcomponent *c, *gauge;

    v1 = icalvalue_new_caladdress("cap://value/1");
    v2 = icalvalue_new_clone(v1);

    printf("%d\n",icalvalue_compare(v1,v2));

    v1 = icalvalue_new_caladdress("A");
    v2 = icalvalue_new_caladdress("B");

    printf("%d\n",icalvalue_compare(v1,v2));

    v1 = icalvalue_new_caladdress("B");
    v2 = icalvalue_new_caladdress("A");

    printf("%d\n",icalvalue_compare(v1,v2));

    v1 = icalvalue_new_integer(5);
    v2 = icalvalue_new_integer(5);

    printf("%d\n",icalvalue_compare(v1,v2));

    v1 = icalvalue_new_integer(5);
    v2 = icalvalue_new_integer(10);

    printf("%d\n",icalvalue_compare(v1,v2));

    v1 = icalvalue_new_integer(10);
    v2 = icalvalue_new_integer(5);

    printf("%d\n",icalvalue_compare(v1,v2));


    gauge = 
	icalcomponent_vanew(
	    ICAL_VCALENDAR_COMPONENT,
	    icalcomponent_vanew(
		ICAL_VEVENT_COMPONENT,  
		icalproperty_vanew_comment(
		    "Comment",
		    icalparameter_new_xliccomparetype(ICAL_XLICCOMPARETYPE_EQUAL),
		    0),
		0),
	    0);

    c =	icalcomponent_vanew(
		ICAL_VEVENT_COMPONENT,  
		icalproperty_vanew_comment(
		    "Comment",
		    0),
		0);

    printf("%s",icalcomponent_as_ical_string(gauge));
		
    printf("%d\n",icalstore_test(c,gauge));

    return 0;
}

void test_restriction()
{
    icalcomponent *comp;
    struct icaltimetype atime = icaltimetype_from_timet( time(0),0);
    int valid; 

    struct icalperiodtype rtime;

    rtime.start = icaltimetype_from_timet( time(0),0);
    rtime.end = icaltimetype_from_timet( time(0),0);

    rtime.end.hour++;
    

    /* Property restrictions */
    assert(icalrestriction_get_property_restriction(
	ICAL_METHOD_PUBLISH,
	ICAL_VEVENT_COMPONENT,
	ICAL_SEQUENCE_PROPERTY) == 5); /* ZEROORONE -> 5 */

    assert(icalrestriction_get_property_restriction(
	ICAL_METHOD_PUBLISH,
	ICAL_VEVENT_COMPONENT,
	ICAL_ATTACH_PROPERTY)==3); /* ZEROPLUS -> 3 */

    assert(icalrestriction_get_property_restriction(
	ICAL_METHOD_DECLINECOUNTER,
	ICAL_VEVENT_COMPONENT,
	ICAL_SEQUENCE_PROPERTY)==1); /* ZERO -> 1 */

    /* Component restrictions */
    assert(icalrestriction_get_component_restriction(
	ICAL_METHOD_PUBLISH,
	ICAL_VJOURNAL_COMPONENT,
	ICAL_X_COMPONENT) == 3); /* ZEROPLUS */

    assert(icalrestriction_get_component_restriction(
	ICAL_METHOD_CANCEL,
	ICAL_VJOURNAL_COMPONENT,
	ICAL_VEVENT_COMPONENT) == 1); /* ZERO */

    comp = 
	icalcomponent_vanew(
	    ICAL_VCALENDAR_COMPONENT,
	    icalproperty_new_version("2.0"),
	    icalproperty_new_prodid("-//RDU Software//NONSGML HandCal//EN"),
	    icalproperty_new_method(ICAL_METHOD_REQUEST),
	    icalcomponent_vanew(
		ICAL_VTIMEZONE_COMPONENT,
		icalproperty_new_tzid("US_Eastern"),
		icalcomponent_vanew(
		    ICAL_XDAYLIGHT_COMPONENT,
		    icalproperty_new_dtstart(atime),
		    icalproperty_new_rdate(rtime),
		    icalproperty_new_tzoffsetfrom(-4.0),
		    icalproperty_new_tzoffsetto(-5.0),
		    icalproperty_new_tzname("EST"),
		    0
		    ),
		icalcomponent_vanew(
		    ICAL_XSTANDARD_COMPONENT,
		    icalproperty_new_dtstart(atime),
		    icalproperty_new_rdate(rtime),
		    icalproperty_new_tzoffsetfrom(-5.0),
		    icalproperty_new_tzoffsetto(-4.0),
		    icalproperty_new_tzname("EST"),
		    0
		    ),
		0
		),
	    icalcomponent_vanew(
		ICAL_VEVENT_COMPONENT,
		icalproperty_new_dtstamp(atime),
		icalproperty_new_uid("guid-1.host1.com"),
		icalproperty_vanew_organizer(
		    "mrbig@host.com",
		    icalparameter_new_role(ICAL_ROLE_CHAIR),
		    0
		    ),
		icalproperty_vanew_attendee(
		    "employee-A@host.com",
		    icalparameter_new_role(ICAL_ROLE_REQPARTICIPANT),
		    icalparameter_new_rsvp(1),
		    icalparameter_new_cutype(ICAL_CUTYPE_GROUP),
		    0
		    ),
		icalproperty_new_description("Project XYZ Review Meeting"),
		icalproperty_new_categories("MEETING"),
		icalproperty_new_class("PUBLIC"),
		icalproperty_new_created(atime),
		icalproperty_new_summary("XYZ Project Review"),
/*		icalproperty_vanew_dtstart(
		    atime,
		    icalparameter_new_tzid("US-Eastern"),
		    0
		    ),*/
		icalproperty_vanew_dtend(
		    atime,
		    icalparameter_new_tzid("US-Eastern"),
		    0
		    ),
		icalproperty_new_location("1CP Conference Room 4350"),
		0
		),
	    0
	    );

    valid = icalrestriction_check(comp);

    printf("#### %d ####\n%s\n",valid, icalcomponent_as_ical_string(comp));

}

void test_calendar()
{
    icalcomponent *comp;
    icalcluster *c;
    icalstore *s;
    icalcalendar* calendar = icalcalendar_new("calendar");
    icalerrorenum error;
    struct icaltimetype atime = icaltimetype_from_timet( time(0),0);

    comp = icalcomponent_vanew(
	ICAL_VEVENT_COMPONENT,
	icalproperty_new_version("2.0"),
	icalproperty_new_description("This is an event"),
	icalproperty_new_dtstart(atime),
	icalproperty_vanew_comment(
	    "Another Comment",
	    icalparameter_new_cn("A Common Name 1"),
	    icalparameter_new_cn("A Common Name 2"),
	    icalparameter_new_cn("A Common Name 3"),
		icalparameter_new_cn("A Common Name 4"),
	    0),
	icalproperty_vanew_xlicerror(
	    "This is only a test",
	    icalparameter_new_xlicerrortype(ICAL_XLICERRORTYPE_COMPONENTPARSEERROR),
	    0),
	
	0);
	
	
    s = icalcalendar_get_booked(calendar);

    error = icalstore_add_component(s,comp);
    
    assert(error == ICAL_NO_ERROR);

    c = icalcalendar_get_properties(calendar);

    error = icalcluster_add_component(c,icalcomponent_new_clone(comp));

    assert(error == ICAL_NO_ERROR);

    icalcalendar_free(calendar);

}

void test_recur()
{
    icalvalue *v;

    v = icalvalue_new_from_string(ICAL_RECUR_VALUE,
				  "FREQ=DAILY;COUNT=5;BYDAY=MO,TU,WE,TH,FR");

    printf("%s\n",icalvalue_as_ical_string(v));

    v = icalvalue_new_from_string(ICAL_RECUR_VALUE,
				  "FREQ=YEARLY;UNTIL=123456T123456;BYSETPOS=-1,2");

    printf("%s\n",icalvalue_as_ical_string(v));

    v = icalvalue_new_from_string(ICAL_RECUR_VALUE,
				  "FREQ=YEARLY;UNTIL=123456T123456;INTERVAL=2;BYMONTH=1;BYDAY=SU;BYHOUR=8,9;BYMINUTE=30");

    printf("%s\n",icalvalue_as_ical_string(v));

    v = icalvalue_new_from_string(ICAL_RECUR_VALUE,
				  "FREQ=MONTHLY;BYDAY=-1MO,TU,WE,TH,FR");

    printf("%s\n",icalvalue_as_ical_string(v));

    v = icalvalue_new_from_string(ICAL_RECUR_VALUE,
				  "FREQ=WEEKLY;INTERVAL=20;WKST=SU;BYDAY=TU");

    printf("%s\n",icalvalue_as_ical_string(v));
    
}

void test_duration()
{

    icalvalue *v;

    v = icalvalue_new_from_string(ICAL_DURATION_VALUE,
				  "PT8H30M");

    printf("%s\n",icalvalue_as_ical_string(v));

    icalvalue_free(v);
    v = icalvalue_new_from_string(ICAL_PERIOD_VALUE,
				  "19971015T050000Z/PT8H30M");

    printf("%s\n",icalvalue_as_ical_string(v));

    icalvalue_free(v);
    v = icalvalue_new_from_string(ICAL_PERIOD_VALUE,
				  "19971015T050000Z/19971015T060000Z");

    printf("%s\n",icalvalue_as_ical_string(v));
    icalvalue_free(v);


}


void test_strings(){

    icalvalue *v;

    v = icalvalue_new_text("foo;bar;bats");
    
    printf("%s\n",icalvalue_as_ical_string(v));

    icalvalue_free(v);

    v = icalvalue_new_text("foo\\;b\nar\\;ba\tts");
    
    printf("%s\n",icalvalue_as_ical_string(v));
    
    icalvalue_free(v);


}

void test_requeststat()
{
  icalrequeststatus s;
  struct icalreqstattype st, st2;
  char temp[1024];

  s = icalenum_num_to_reqstat(2,1);

  assert(s == ICAL_2_1_FALLBACK_STATUS);

  assert(icalenum_reqstat_major(s) == 2);
  assert(icalenum_reqstat_minor(s) == 1);

  printf("2.1: %s\n",icalenum_reqstat_desc(s));

  st.code = s;
  st.debug = "booga";
  st.desc = 0;

  printf("%s\n",icalreqstattype_as_string(st));

  st.desc = " A non-standard description";

  printf("%s\n",icalreqstattype_as_string(st));


  st.desc = 0;

  sprintf(temp,"%s\n",icalreqstattype_as_string(st));
  

  st2 = icalreqstattype_from_string("2.1;Success but fallback taken  on one or more property  values.;booga");

  printf("%d --  %d --  %s -- %s\n",icalenum_reqstat_major(st2.code),
         icalenum_reqstat_minor(st2.code),
         icalenum_reqstat_desc(st2.code),
         st2.debug);

  st2 = icalreqstattype_from_string("2.1;Success but fallback taken  on one or more property  values.;booga");
  printf("%s\n",icalreqstattype_as_string(st2));

  st2 = icalreqstattype_from_string("2.1;Success but fallback taken  on one or more property  values.;");
  printf("%s\n",icalreqstattype_as_string(st2));

  st2 = icalreqstattype_from_string("2.1;Success but fallback taken  on one or more property  values.");
  printf("%s\n",icalreqstattype_as_string(st2));

  st2 = icalreqstattype_from_string("2.1;");
  printf("%s\n",icalreqstattype_as_string(st2));

  st2 = icalreqstattype_from_string("2.1");
  printf("%s\n",icalreqstattype_as_string(st2));

  st2 = icalreqstattype_from_string("16.4");
  assert(st2.code == ICAL_UNKNOWN_STATUS);

  st2 = icalreqstattype_from_string("1.");
  assert(st2.code == ICAL_UNKNOWN_STATUS);

}


int main(int argc, char *argv[])
{


    printf("\n------------Test Restriction---------------\n");
    test_restriction();

    exit(0);

    printf("\n------------Test request status-------\n");
    test_requeststat();


    printf("\n------------Test strings---------------\n");
    test_strings();

    printf("\n------------Test recur---------------\n");
    test_recur();

    printf("\n------------Test duration---------------\n");
    test_duration();

    printf("\n------------Test Compare---------------\n");
    test_compare();

    printf("\n------------Test Memory---------------\n");
    test_memory();

    printf("\n------------Test Values---------------\n");
    test_values();
    
    printf("\n------------Test Parameters-----------\n");
    test_parameters();

    printf("\n------------Test Properties-----------\n");
    test_properties();

    printf("\n------------Test Components ----------\n");
    test_components();

    printf("\n------------Create Components --------\n");
    create_new_component();

    printf("\n----- Create Components with vaargs ---\n");
    create_new_component_with_va_args();

    


    return 0;
}



