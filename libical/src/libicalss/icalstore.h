/* -*- Mode: C -*- */
/*======================================================================
 FILE: icalstore.h
 CREATOR: eric 28 November 1999


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

#ifndef ICALSTORE_H
#define ICALSTORE_H

#include "ical.h"
#include "icalerror.h"
typedef void icalstore;

/* icalstore Routines for storing, fetching, and searching for ical
 * objects in a database */

icalstore* icalstore_new(char* dir);

void icalstore_free(icalstore* store);

/* Add a new component to the store */
icalerrorenum icalstore_add_component(icalstore* store, icalstore* comp);

/* Remove a component from the store */
icalerrorenum icalstore_remove_component(icalstore* store, icalstore* comp);

/* Restrict the component returned by icalstore_first, _next to those
   that pass the gauge */
icalerrorenum icalstore_select(icalstore* store, icalcomponent* gauge);

/* Return true if a component passes the gauge */
int icalstore_test(icalcomponent* comp, icalcomponent* gauge);

/* Clear the restrictions set by icalstore_select */
void icalstore_clear(icalstore* store);

/* Get a single component by uid */
icalcomponent* icalstore_fetch(icalstore* store, char* uid);

/* Return true of the store has an object with the given UID */
int icalstore_has_uid(icalstore* store, char* uid);

/* Return the first component in the store, or first that passes the gauge.*/
icalcomponent* icalstore_get_first_component(icalstore* store);

/* Return the next component in the store, or next that passes the gauge.*/
icalcomponent* icalstore_get_next_component(icalstore* store);

	
int icalstore_next_uid_number(icalstore* store);


#endif /* !ICALSTORE_H */



