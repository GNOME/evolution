/* -*- Mode: C -*- */
/*======================================================================
  FILE: icalrestriction.h
  CREATOR: eric 24 April 1999
  
  $Id$


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
  The original code is icalrestriction.h

  Contributions from:
     Graham Davison (g.m.davison@computer.org)


======================================================================*/

#include "ical.h"

#ifndef ICALRESTRICTION_H
#define ICALRESTRICTION_H

/* These must stay in this order for icalrestriction_compare to work */
typedef enum icalrestriction_kind {
    ICAL_RESTRICTION_NONE=0,		/* 0 */
    ICAL_RESTRICTION_ZERO,		/* 1 */
    ICAL_RESTRICTION_ONE,		/* 2 */
    ICAL_RESTRICTION_ZEROPLUS,		/* 3 */
    ICAL_RESTRICTION_ONEPLUS,		/* 4 */
    ICAL_RESTRICTION_ZEROORONE,		/* 5 */
    ICAL_RESTRICTION_ONEEXCLUSIVE,	/* 6 */
    ICAL_RESTRICTION_ONEMUTUAL,		/* 7 */
    ICAL_RESTRICTION_UNKNOWN		/* 8 */
} icalrestriction_kind;

int 
icalrestriction_compare(icalrestriction_kind restr, int count);

icalrestriction_kind
icalrestriction_get_property_restriction(icalproperty_method method,
					 icalcomponent_kind component,
					 icalproperty_kind property);

icalrestriction_kind
icalrestriction_get_component_restriction(icalproperty_method method,
					 icalcomponent_kind component,
					  icalcomponent_kind subcomponent);

int
icalrestriction_is_parameter_allowed(icalproperty_kind property,
                                       icalparameter_kind parameter);

int icalrestriction_check(icalcomponent* comp);


#endif /* !ICALRESTRICTION_H */



