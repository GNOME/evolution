/* -*- Mode: C -*- */
/*======================================================================
  FILE: icalvalue.h
  CREATOR: eric 20 March 1999


  $Id$
  $Locker$

  

 (C) COPYRIGHT 2000, Eric Busboom, http://www.softwarestudio.org

 This program is free software; you can redistribute it and/or modify
 it under the terms of either: 

    The LGPL as published by the Free Software Foundation, version
    2.1, available at: http://www.fsf.org/copyleft/lesser.html

  Or:

    The Mozilla Public License Version 1.0. You may obtain a copy of
    the License at http://www.mozilla.org/MPL/

  The original code is icalvalue.h

  ======================================================================*/

#ifndef ICALVALUE_H
#define ICALVALUE_H

#include <time.h>
#include "icalenums.h"
#include "icaltypes.h"

typedef void icalvalue;

icalvalue* icalvalue_new(icalvalue_kind kind);

icalvalue* icalvalue_new_clone(icalvalue* value);

icalvalue* icalvalue_new_from_string(icalvalue_kind kind, char* str);

void icalvalue_free(icalvalue* value);

int icalvalue_is_valid(icalvalue* value);

char* icalvalue_as_ical_string(icalvalue* value);

icalvalue_kind icalvalue_isa(icalvalue* value);

int icalvalue_isa_value(void*);

icalparameter_xliccomparetype
icalvalue_compare(icalvalue* a, icalvalue *b);

/* Everything below this line is machine generated. Do not edit. */
/* ATTACH # Non-std */
icalvalue* icalvalue_new_attach(struct icalattachtype v);
struct icalattachtype icalvalue_get_attach(icalvalue* value);
void icalvalue_set_attach(icalvalue* value, struct icalattachtype v);

/* BINARY  */
icalvalue* icalvalue_new_binary(char* v);
char* icalvalue_get_binary(icalvalue* value);
void icalvalue_set_binary(icalvalue* value, char* v);

/* BOOLEAN  */
icalvalue* icalvalue_new_boolean(int v);
int icalvalue_get_boolean(icalvalue* value);
void icalvalue_set_boolean(icalvalue* value, int v);

/* CAL-ADDRESS  */
icalvalue* icalvalue_new_caladdress(char* v);
char* icalvalue_get_caladdress(icalvalue* value);
void icalvalue_set_caladdress(icalvalue* value, char* v);

/* DATE  */
icalvalue* icalvalue_new_date(struct icaltimetype v);
struct icaltimetype icalvalue_get_date(icalvalue* value);
void icalvalue_set_date(icalvalue* value, struct icaltimetype v);

/* DATE-TIME  */
icalvalue* icalvalue_new_datetime(struct icaltimetype v);
struct icaltimetype icalvalue_get_datetime(icalvalue* value);
void icalvalue_set_datetime(icalvalue* value, struct icaltimetype v);

/* DATE-TIME-DATE # Non-std */
icalvalue* icalvalue_new_datetimedate(struct icaltimetype v);
struct icaltimetype icalvalue_get_datetimedate(icalvalue* value);
void icalvalue_set_datetimedate(icalvalue* value, struct icaltimetype v);

/* DATE-TIME-PERIOD # Non-std */
icalvalue* icalvalue_new_datetimeperiod(struct icalperiodtype v);
struct icalperiodtype icalvalue_get_datetimeperiod(icalvalue* value);
void icalvalue_set_datetimeperiod(icalvalue* value, struct icalperiodtype v);

/* DURATION  */
icalvalue* icalvalue_new_duration(struct icaldurationtype v);
struct icaldurationtype icalvalue_get_duration(icalvalue* value);
void icalvalue_set_duration(icalvalue* value, struct icaldurationtype v);

/* FLOAT  */
icalvalue* icalvalue_new_float(float v);
float icalvalue_get_float(icalvalue* value);
void icalvalue_set_float(icalvalue* value, float v);

/* GEO # Non-std */
icalvalue* icalvalue_new_geo(struct icalgeotype v);
struct icalgeotype icalvalue_get_geo(icalvalue* value);
void icalvalue_set_geo(icalvalue* value, struct icalgeotype v);

/* INTEGER  */
icalvalue* icalvalue_new_integer(int v);
int icalvalue_get_integer(icalvalue* value);
void icalvalue_set_integer(icalvalue* value, int v);

/* METHOD # Non-std */
icalvalue* icalvalue_new_method(icalproperty_method v);
icalproperty_method icalvalue_get_method(icalvalue* value);
void icalvalue_set_method(icalvalue* value, icalproperty_method v);

/* PERIOD  */
icalvalue* icalvalue_new_period(struct icalperiodtype v);
struct icalperiodtype icalvalue_get_period(icalvalue* value);
void icalvalue_set_period(icalvalue* value, struct icalperiodtype v);

/* RECUR  */
icalvalue* icalvalue_new_recur(struct icalrecurrencetype v);
struct icalrecurrencetype icalvalue_get_recur(icalvalue* value);
void icalvalue_set_recur(icalvalue* value, struct icalrecurrencetype v);

/* STRING # Non-std */
icalvalue* icalvalue_new_string(char* v);
char* icalvalue_get_string(icalvalue* value);
void icalvalue_set_string(icalvalue* value, char* v);

/* TEXT  */
icalvalue* icalvalue_new_text(char* v);
char* icalvalue_get_text(icalvalue* value);
void icalvalue_set_text(icalvalue* value, char* v);

/* TIME  */
icalvalue* icalvalue_new_time(struct icaltimetype v);
struct icaltimetype icalvalue_get_time(icalvalue* value);
void icalvalue_set_time(icalvalue* value, struct icaltimetype v);

/* TRIGGER # Non-std */
icalvalue* icalvalue_new_trigger(union icaltriggertype v);
union icaltriggertype icalvalue_get_trigger(icalvalue* value);
void icalvalue_set_trigger(icalvalue* value, union icaltriggertype v);

/* URI  */
icalvalue* icalvalue_new_uri(char* v);
char* icalvalue_get_uri(icalvalue* value);
void icalvalue_set_uri(icalvalue* value, char* v);

/* UTC-OFFSET  */
icalvalue* icalvalue_new_utcoffset(int v);
int icalvalue_get_utcoffset(icalvalue* value);
void icalvalue_set_utcoffset(icalvalue* value, int v);

/* QUERY  */
icalvalue* icalvalue_new_query(char* v);
char* icalvalue_get_query(icalvalue* value);
void icalvalue_set_query(icalvalue* value, char* v);

#endif ICALVALUE_H
