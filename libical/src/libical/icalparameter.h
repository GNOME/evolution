/* -*- Mode: C -*- */
/*======================================================================
  FILE: icalparam.h
  CREATOR: eric 20 March 1999


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
  The original code is icalparam.h

  ======================================================================*/

#ifndef ICALPARAM_H
#define ICALPARAM_H

#include "icalenums.h" /* defined icalparameter_kind and other enums */

typedef void icalparameter;

icalparameter* icalparameter_new(icalparameter_kind kind);
icalparameter* icalparameter_new_clone(icalparameter* p);
icalparameter* icalparameter_new_from_string(icalparameter_kind kind, char* value);

void icalparameter_free(icalparameter* parameter);

char* icalparameter_as_ical_string(icalparameter* parameter);

int icalparameter_is_valid(icalparameter* parameter);

icalparameter_kind icalparameter_isa(icalparameter* parameter);

int icalparameter_isa_parameter(void* param);

/* Acess the name of an X parameer */
void icalparameter_set_xname (icalparameter* param, char* v);
char* icalparameter_get_xname(icalparameter* param);
void icalparameter_set_xvalue (icalparameter* param, char* v);
char* icalparameter_get_xvalue(icalparameter* param);


/* Everything below this line is machine generated. Do not edit. */
/* ALTREP */
icalparameter* icalparameter_new_altrep(char* v);
char* icalparameter_get_altrep(icalparameter* value);
void icalparameter_set_altrep(icalparameter* value, char* v);

/* CN */
icalparameter* icalparameter_new_cn(char* v);
char* icalparameter_get_cn(icalparameter* value);
void icalparameter_set_cn(icalparameter* value, char* v);

/* CUTYPE */
icalparameter* icalparameter_new_cutype(icalparameter_cutype v);
icalparameter_cutype icalparameter_get_cutype(icalparameter* value);
void icalparameter_set_cutype(icalparameter* value, icalparameter_cutype v);

/* DELEGATED-FROM */
icalparameter* icalparameter_new_delegatedfrom(char* v);
char* icalparameter_get_delegatedfrom(icalparameter* value);
void icalparameter_set_delegatedfrom(icalparameter* value, char* v);

/* DELEGATED-TO */
icalparameter* icalparameter_new_delegatedto(char* v);
char* icalparameter_get_delegatedto(icalparameter* value);
void icalparameter_set_delegatedto(icalparameter* value, char* v);

/* DIR */
icalparameter* icalparameter_new_dir(char* v);
char* icalparameter_get_dir(icalparameter* value);
void icalparameter_set_dir(icalparameter* value, char* v);

/* ENCODING */
icalparameter* icalparameter_new_encoding(icalparameter_encoding v);
icalparameter_encoding icalparameter_get_encoding(icalparameter* value);
void icalparameter_set_encoding(icalparameter* value, icalparameter_encoding v);

/* FBTYPE */
icalparameter* icalparameter_new_fbtype(icalparameter_fbtype v);
icalparameter_fbtype icalparameter_get_fbtype(icalparameter* value);
void icalparameter_set_fbtype(icalparameter* value, icalparameter_fbtype v);

/* FMTTYPE */
icalparameter* icalparameter_new_fmttype(char* v);
char* icalparameter_get_fmttype(icalparameter* value);
void icalparameter_set_fmttype(icalparameter* value, char* v);

/* LANGUAGE */
icalparameter* icalparameter_new_language(char* v);
char* icalparameter_get_language(icalparameter* value);
void icalparameter_set_language(icalparameter* value, char* v);

/* MEMBER */
icalparameter* icalparameter_new_member(char* v);
char* icalparameter_get_member(icalparameter* value);
void icalparameter_set_member(icalparameter* value, char* v);

/* PARTSTAT */
icalparameter* icalparameter_new_partstat(icalparameter_partstat v);
icalparameter_partstat icalparameter_get_partstat(icalparameter* value);
void icalparameter_set_partstat(icalparameter* value, icalparameter_partstat v);

/* RANGE */
icalparameter* icalparameter_new_range(icalparameter_range v);
icalparameter_range icalparameter_get_range(icalparameter* value);
void icalparameter_set_range(icalparameter* value, icalparameter_range v);

/* RELATED */
icalparameter* icalparameter_new_related(icalparameter_related v);
icalparameter_related icalparameter_get_related(icalparameter* value);
void icalparameter_set_related(icalparameter* value, icalparameter_related v);

/* RELTYPE */
icalparameter* icalparameter_new_reltype(icalparameter_reltype v);
icalparameter_reltype icalparameter_get_reltype(icalparameter* value);
void icalparameter_set_reltype(icalparameter* value, icalparameter_reltype v);

/* ROLE */
icalparameter* icalparameter_new_role(icalparameter_role v);
icalparameter_role icalparameter_get_role(icalparameter* value);
void icalparameter_set_role(icalparameter* value, icalparameter_role v);

/* RSVP */
icalparameter* icalparameter_new_rsvp(int v);
int icalparameter_get_rsvp(icalparameter* value);
void icalparameter_set_rsvp(icalparameter* value, int v);

/* SENT-BY */
icalparameter* icalparameter_new_sentby(char* v);
char* icalparameter_get_sentby(icalparameter* value);
void icalparameter_set_sentby(icalparameter* value, char* v);

/* TZID */
icalparameter* icalparameter_new_tzid(char* v);
char* icalparameter_get_tzid(icalparameter* value);
void icalparameter_set_tzid(icalparameter* value, char* v);

/* VALUE */
icalparameter* icalparameter_new_value(icalparameter_value v);
icalparameter_value icalparameter_get_value(icalparameter* value);
void icalparameter_set_value(icalparameter* value, icalparameter_value v);

/* X */
icalparameter* icalparameter_new_x(char* v);
char* icalparameter_get_x(icalparameter* value);
void icalparameter_set_x(icalparameter* value, char* v);

/* X-LIC-ERRORTYPE */
icalparameter* icalparameter_new_xlicerrortype(icalparameter_xlicerrortype v);
icalparameter_xlicerrortype icalparameter_get_xlicerrortype(icalparameter* value);
void icalparameter_set_xlicerrortype(icalparameter* value, icalparameter_xlicerrortype v);

/* X-LIC-COMPARETYPE */
icalparameter* icalparameter_new_xliccomparetype(icalparameter_xliccomparetype v);
icalparameter_xliccomparetype icalparameter_get_xliccomparetype(icalparameter* value);
void icalparameter_set_xliccomparetype(icalparameter* value, icalparameter_xliccomparetype v);

#endif ICALPARAMETER_H
