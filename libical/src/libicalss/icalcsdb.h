/* -*- Mode: C -*- */
/*======================================================================
 FILE: icalcsdb.h Calendar Server Database
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

#ifndef ICALCSDB_H
#define ICALCSDB_H

#include "ical.h"

typedef void icalcsdb;

icalcsdb* icalcsdb_new(char* path);
void icalcsdb_free(icalcsdb* csdb);

icalerrorenum icalcsdb_create(icalcsdb* db, char* calid);

icalerrorenum icalcsdb_delete(icalcsdb* db, char* calid);

icalerrorenum icalcsdb_move(icalcsdb* db, char* oldcalid, char* newcalid);

char* icalcsdb_generateuid(icalcsdb* db);

icalcalendar* icalcsdb_get_calendar(icalcsdb* db, char* calid);

icalcluster* icalcsdb_get_vcars(icalcsdb* db);

icalcluster* icalcsdb_get_properties(icalcsdb* db);

icalcluster* icalcsdb_get_capabilities(icalcsdb* db);

icalcluster* icalcsdb_get_timezones(icalcsdb* db);


#endif /* !ICALCSDB_H */



