/* -*- Mode: C -*-
  ======================================================================
  FILE: icaltestparser.c
  CREATOR: eric 20 June 1999
  
  $Id$
  $Locker$
    
 The contents of this file are subject to the Mozilla Public License
 Version 1.0 (the "License"); you may not use this file except in
 compliance with the License. You may obtain a copy of the License at
 http://www.mozilla.org/MPL/
 
 Software distributed under the License is distributed on an "AS IS"
 basis, WITHOUT WARRANTY OF ANY KIND, either express or implied. See
 the License for the specific language governing rights and
 limitations under the License.

  The original author is Eric Busboom
  The original code is icaltestparser.c

 
 (C) COPYRIGHT 1999 The Software Studio. 
 http://www.softwarestudio.org

 ======================================================================*/

#include <stdio.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include "ical.h"

#include <stdlib.h>

char str[] = "BEGIN:VCALENDAR
PRODID:\"-//RDU Software//NONSGML HandCal//EN\"
VERSION:2.0
BEGIN:VTIMEZONE
TZID:US-Eastern
BEGIN:STANDARD
DTSTART:19990404T020000
RDATE:19990u404xT020000
TZOFFSETFROM:-0500
TZOFFSETTO:-0400
END:STANDARD
BEGIN:DAYLIGHT
DTSTART:19990404T020000
RDATE:19990404T020000
TZOFFSETFROM:-0500
TZOFFSETTO:-0400
TZNAME:EDT
Dkjhgri:derhvnv;
BEGIN:dfkjh
END:dfdfkjh
END:DAYLIGHT
END:VTIMEZONE
BEGIN:VEVENT
GEO:Bongo
DTSTAMP:19980309T231000Z
UID:guid-1.host1.com
ORGANIZER;ROLE=CHAIR:MAILTO:mrbig@host.com
ATTENDEE;RSVP=TRUE;ROLE=REQ-PARTICIPANT;CUTYPE=GROUP
 :MAILTO:employee-A@host.com
DESCRIPTION:Project XYZ Review Meeting
CATEGORIES:MEETING
CLASS:PUBLIC
CREATED:19980309T130000Z
SUMMARY:XYZ Project Review
DTSTART;TZID=US-Eastern:19980312T083000
DTEND;TZID=US-Eastern:19980312T093000
LOCATION:1CP Conference Room 4350
END:VEVENT
END:VCALENDAR
";

extern int yydebug;

/* Have the parser fetch data from stdin */

char* read_stdin(char *s, size_t size, void *d)
{
  char *c = fgets(s,size, stdin);

  return c;

}

int main()
{

  /* This is how we would have the parser parse a string */
  /*    icalcomponent *c = icalparser_parse_string(str);*/

   icalcomponent *c = icalparser_parse(read_stdin);

    printf("%s\n",icalcomponent_as_ical_string(c));

    /* Strip errors and spit it out again 
    printf("\n%d Errors in Component\n",icalcomponent_count_errors(c));
    icalcomponent_strip_errors(c);
    printf("%s\n",icalcomponent_as_ical_string(c));    
    */

    icalmemory_free_ring();
    icalcomponent_free(c);

    return 1;
}

