/* -*- Mode: C -*- */
/*======================================================================
 FILE: icalcalendar.h
 CREATOR: eric 23 December 1999


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
 
 The Original Code is eric. The Initial Developer of the Original
 Code is Eric Busboom


======================================================================*/

#ifndef ICALCALENDAR_H
#define ICALCALENDAR_H

#include "ical.h"
#include "icalstore.h"
#include "icalcluster.h"

/* icalcalendar
 * Routines for storing calendar data in a file system. The calendar 
 * has two icalstores, one for incoming components and one for booked
 * components. It also has interfaces to access the free/busy list
 * and a list of calendar properties */

typedef  void icalcalendar;

icalcalendar* icalcalendar_new(char* dir);

void icalcalendar_free(icalcalendar* calendar);

int icalcalendar_lock(icalcalendar* calendar);

int icalcalendar_unlock(icalcalendar* calendar);

int icalcalendar_islocked(icalcalendar* calendar);

int icalcalendar_ownlock(icalcalendar* calendar);

icalstore* icalcalendar_get_booked(icalcalendar* calendar);

icalcluster* icalcalendar_get_incoming(icalcalendar* calendar);

icalcluster* icalcalendar_get_properties(icalcalendar* calendar);

icalcluster* icalcalendar_get_freebusy(icalcalendar* calendar);


#endif /* !ICALCALENDAR_H */



