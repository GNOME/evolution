/* -*- Mode: C -*- */
/*======================================================================
 FILE: icalvcal.h
 CREATOR: eric 13 January 2000


 $Id$
 $Locker$

 (C) COPYRIGHT 2000 Eric Busboom
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

#ifndef ICALVCAL_H
#define ICALVCAL_H

VCalObject* icalvcal_new_vcal_from_ical(icalcomponent* component);
icalcomponent* icalvcal_new_ical_from_vcal(VCalObject* vcal);


#endif /* !ICALVCAL_H */



