/* -*- Mode: C -*- */
/*======================================================================
 FILE: icalcomponent.h
 CREATOR: eric 20 March 1999


 (C) COPYRIGHT 2000, Eric Busboom, http://www.softwarestudio.org

 This program is free software; you can redistribute it and/or modify
 it under the terms of either: 

    The LGPL as published by the Free Software Foundation, version
    2.1, available at: http://www.fsf.org/copyleft/lesser.html

  Or:

    The Mozilla Public License Version 1.0. You may obtain a copy of
    the License at http://www.mozilla.org/MPL/

  The original code is icalcomponent.h

======================================================================*/

#ifndef ICALCOMPONENT_H
#define ICALCOMPONENT_H

#include "icalproperty.h"
#include "icalvalue.h"
#include "icalenums.h" /* defines icalcomponent_kind */
#include "pvl.h"

typedef void icalcomponent;

/* This is exposed so that callers will not have to allocate and
   deallocate iterators. Pretend that you can't see it. */
typedef struct icalcompiter
{
	icalcomponent_kind kind;
	pvl_elem iter;

} icalcompiter;

icalcomponent* icalcomponent_new(icalcomponent_kind kind);
icalcomponent* icalcomponent_new_clone(icalcomponent* component);
icalcomponent* icalcomponent_new_from_string(char* str);
icalcomponent* icalcomponent_vanew(icalcomponent_kind kind, ...);
void icalcomponent_free(icalcomponent* component);

char* icalcomponent_as_ical_string(icalcomponent* component);

int icalcomponent_is_valid(icalcomponent* component);

icalcomponent_kind icalcomponent_isa(icalcomponent* component);

int icalcomponent_isa_component (void* component);

/* 
 * Working with properties
 */

void icalcomponent_add_property(icalcomponent* component,
				icalproperty* property);

void icalcomponent_remove_property(icalcomponent* component,
				   icalproperty* property);

int icalcomponent_count_properties(icalcomponent* component,
				   icalproperty_kind kind);

/* Iterate through the properties */
icalproperty* icalcomponent_get_current_property(icalcomponent* component);

icalproperty* icalcomponent_get_first_property(icalcomponent* component,
					      icalproperty_kind kind);
icalproperty* icalcomponent_get_next_property(icalcomponent* component,
					      icalproperty_kind kind);

/* Return a null-terminated array of icalproperties*/

icalproperty** icalcomponent_get_properties(icalcomponent* component,
					      icalproperty_kind kind);


/* 
 * Working with components
 */ 


void icalcomponent_add_component(icalcomponent* parent,
				icalcomponent* child);

void icalcomponent_remove_component(icalcomponent* parent,
				icalcomponent* child);

int icalcomponent_count_components(icalcomponent* component,
				   icalcomponent_kind kind);

/* Iteration Routines. There are two forms of iterators, internal and
external. The internal ones came first, and are almost completely
sufficient, but they fail badly when you want to construct a loop that
removes components from the container.

The internal iterators are deprecated. */

/* Using external iterators */
icalcompiter icalcomponent_begin_component(icalcomponent* component,
					   icalcomponent_kind kind);

icalcompiter icalcomponent_end_component(icalcomponent* component,
					 icalcomponent_kind kind);

/* Iterate through components */
icalcomponent* icalcomponent_get_current_component (icalcomponent* component);

icalcomponent* icalcomponent_get_first_component(icalcomponent* component,
					      icalcomponent_kind kind);
icalcomponent* icalcomponent_get_next_component(icalcomponent* component,
					      icalcomponent_kind kind);



/* Return a null-terminated array of icalproperties*/
icalproperty** icalcomponent_get_component(icalcomponent* component,
					      icalproperty_kind kind);

/* Working with embedded error properties */

int icalcomponent_count_errors(icalcomponent* component);

/* Remove all X-LIC-ERROR properties*/
void icalcomponent_strip_errors(icalcomponent* component);

/* Convert some X-LIC-ERROR properties into RETURN-STATUS properties*/
void icalcomponent_convert_errors(icalcomponent* component);

/* Internal operations. You don't see these... */
icalcomponent* icalcomponent_get_parent(icalcomponent* component);
void icalcomponent_set_parent(icalcomponent* component, 
			      icalcomponent* parent);



/* External component iterator */
icalcomponent* icalcompiter_next(icalcompiter* i);
icalcomponent* icalcompiter_prior(icalcompiter* i);
icalcomponent* icalcompiter_deref(icalcompiter* i);


#endif /* !ICALCOMPONENT_H */



