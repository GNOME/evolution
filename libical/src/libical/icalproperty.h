/* -*- Mode: C -*-
  ======================================================================
  FILE: icalderivedproperties.{c,h}
  CREATOR: eric 09 May 1999
  
  $Id$
    
 (C) COPYRIGHT 2000, Eric Busboom, http://www.softwarestudio.org
 ======================================================================*/


#ifndef ICALPROPERTY_H
#define ICALPROPERTY_H

#include <time.h>

typedef void icalproperty;

icalproperty* icalproperty_new(icalproperty_kind kind);

icalproperty* icalproperty_new_clone(icalproperty * prop);

icalproperty* icalproperty_new_from_string(char* str);

char* icalproperty_as_ical_string(icalproperty* prop);

void  icalproperty_free(icalproperty* prop);

icalproperty_kind icalproperty_isa(icalproperty* property);
int icalproperty_isa_property(void* property);

void icalproperty_add_parameter(icalproperty* prop,icalparameter* parameter);

void icalproperty_remove_parameter(icalproperty* prop,
				   icalparameter_kind kind);

int icalproperty_count_parameters(icalproperty* prop);

/* Iterate through the parameters */
icalparameter* icalproperty_get_first_parameter(icalproperty* prop,
						icalparameter_kind kind);
icalparameter* icalproperty_get_next_parameter(icalproperty* prop,
						icalparameter_kind kind);
/* Access the value of the property */
void icalproperty_set_value(icalproperty* prop, icalvalue* value);
icalvalue* icalproperty_get_value(icalproperty* prop);

/* Deal with X properties */

void icalproperty_set_x_name(icalproperty* prop, char* name);
char* icalproperty_get_x_name(icalproperty* prop);

/* Everything below this line is machine generated. Do not edit. */

/* METHOD */
icalproperty* icalproperty_new_method(icalproperty_method v);
icalproperty* icalproperty_vanew_method(icalproperty_method v, ...);
void icalproperty_set_method(icalproperty* prop, icalproperty_method v);
icalproperty_method icalproperty_get_method(icalproperty* prop);

/* X-LIC-MIMECID */
icalproperty* icalproperty_new_xlicmimecid(char* v);
icalproperty* icalproperty_vanew_xlicmimecid(char* v, ...);
void icalproperty_set_xlicmimecid(icalproperty* prop, char* v);
char* icalproperty_get_xlicmimecid(icalproperty* prop);

/* LAST-MODIFIED */
icalproperty* icalproperty_new_lastmodified(struct icaltimetype v);
icalproperty* icalproperty_vanew_lastmodified(struct icaltimetype v, ...);
void icalproperty_set_lastmodified(icalproperty* prop, struct icaltimetype v);
struct icaltimetype icalproperty_get_lastmodified(icalproperty* prop);

/* UID */
icalproperty* icalproperty_new_uid(char* v);
icalproperty* icalproperty_vanew_uid(char* v, ...);
void icalproperty_set_uid(icalproperty* prop, char* v);
char* icalproperty_get_uid(icalproperty* prop);

/* PRODID */
icalproperty* icalproperty_new_prodid(char* v);
icalproperty* icalproperty_vanew_prodid(char* v, ...);
void icalproperty_set_prodid(icalproperty* prop, char* v);
char* icalproperty_get_prodid(icalproperty* prop);

/* STATUS */
icalproperty* icalproperty_new_status(char* v);
icalproperty* icalproperty_vanew_status(char* v, ...);
void icalproperty_set_status(icalproperty* prop, char* v);
char* icalproperty_get_status(icalproperty* prop);

/* DESCRIPTION */
icalproperty* icalproperty_new_description(char* v);
icalproperty* icalproperty_vanew_description(char* v, ...);
void icalproperty_set_description(icalproperty* prop, char* v);
char* icalproperty_get_description(icalproperty* prop);

/* DURATION */
icalproperty* icalproperty_new_duration(struct icaldurationtype v);
icalproperty* icalproperty_vanew_duration(struct icaldurationtype v, ...);
void icalproperty_set_duration(icalproperty* prop, struct icaldurationtype v);
struct icaldurationtype icalproperty_get_duration(icalproperty* prop);

/* CATEGORIES */
icalproperty* icalproperty_new_categories(char* v);
icalproperty* icalproperty_vanew_categories(char* v, ...);
void icalproperty_set_categories(icalproperty* prop, char* v);
char* icalproperty_get_categories(icalproperty* prop);

/* VERSION */
icalproperty* icalproperty_new_version(char* v);
icalproperty* icalproperty_vanew_version(char* v, ...);
void icalproperty_set_version(icalproperty* prop, char* v);
char* icalproperty_get_version(icalproperty* prop);

/* TZOFFSETFROM */
icalproperty* icalproperty_new_tzoffsetfrom(int v);
icalproperty* icalproperty_vanew_tzoffsetfrom(int v, ...);
void icalproperty_set_tzoffsetfrom(icalproperty* prop, int v);
int icalproperty_get_tzoffsetfrom(icalproperty* prop);

/* RRULE */
icalproperty* icalproperty_new_rrule(struct icalrecurrencetype v);
icalproperty* icalproperty_vanew_rrule(struct icalrecurrencetype v, ...);
void icalproperty_set_rrule(icalproperty* prop, struct icalrecurrencetype v);
struct icalrecurrencetype icalproperty_get_rrule(icalproperty* prop);

/* ATTENDEE */
icalproperty* icalproperty_new_attendee(char* v);
icalproperty* icalproperty_vanew_attendee(char* v, ...);
void icalproperty_set_attendee(icalproperty* prop, char* v);
char* icalproperty_get_attendee(icalproperty* prop);

/* CONTACT */
icalproperty* icalproperty_new_contact(char* v);
icalproperty* icalproperty_vanew_contact(char* v, ...);
void icalproperty_set_contact(icalproperty* prop, char* v);
char* icalproperty_get_contact(icalproperty* prop);

/* X-LIC-MIMECONTENTTYPE */
icalproperty* icalproperty_new_xlicmimecontenttype(char* v);
icalproperty* icalproperty_vanew_xlicmimecontenttype(char* v, ...);
void icalproperty_set_xlicmimecontenttype(icalproperty* prop, char* v);
char* icalproperty_get_xlicmimecontenttype(icalproperty* prop);

/* X-LIC-MIMEOPTINFO */
icalproperty* icalproperty_new_xlicmimeoptinfo(char* v);
icalproperty* icalproperty_vanew_xlicmimeoptinfo(char* v, ...);
void icalproperty_set_xlicmimeoptinfo(icalproperty* prop, char* v);
char* icalproperty_get_xlicmimeoptinfo(icalproperty* prop);

/* RELATED-TO */
icalproperty* icalproperty_new_relatedto(char* v);
icalproperty* icalproperty_vanew_relatedto(char* v, ...);
void icalproperty_set_relatedto(icalproperty* prop, char* v);
char* icalproperty_get_relatedto(icalproperty* prop);

/* ORGANIZER */
icalproperty* icalproperty_new_organizer(char* v);
icalproperty* icalproperty_vanew_organizer(char* v, ...);
void icalproperty_set_organizer(icalproperty* prop, char* v);
char* icalproperty_get_organizer(icalproperty* prop);

/* COMMENT */
icalproperty* icalproperty_new_comment(char* v);
icalproperty* icalproperty_vanew_comment(char* v, ...);
void icalproperty_set_comment(icalproperty* prop, char* v);
char* icalproperty_get_comment(icalproperty* prop);

/* X-LIC-ERROR */
icalproperty* icalproperty_new_xlicerror(char* v);
icalproperty* icalproperty_vanew_xlicerror(char* v, ...);
void icalproperty_set_xlicerror(icalproperty* prop, char* v);
char* icalproperty_get_xlicerror(icalproperty* prop);

/* TRIGGER */
icalproperty* icalproperty_new_trigger(union icaltriggertype v);
icalproperty* icalproperty_vanew_trigger(union icaltriggertype v, ...);
void icalproperty_set_trigger(icalproperty* prop, union icaltriggertype v);
union icaltriggertype icalproperty_get_trigger(icalproperty* prop);

/* CLASS */
icalproperty* icalproperty_new_class(char* v);
icalproperty* icalproperty_vanew_class(char* v, ...);
void icalproperty_set_class(icalproperty* prop, char* v);
char* icalproperty_get_class(icalproperty* prop);

/* X */
icalproperty* icalproperty_new_x(char* v);
icalproperty* icalproperty_vanew_x(char* v, ...);
void icalproperty_set_x(icalproperty* prop, char* v);
char* icalproperty_get_x(icalproperty* prop);

/* TZOFFSETTO */
icalproperty* icalproperty_new_tzoffsetto(int v);
icalproperty* icalproperty_vanew_tzoffsetto(int v, ...);
void icalproperty_set_tzoffsetto(icalproperty* prop, int v);
int icalproperty_get_tzoffsetto(icalproperty* prop);

/* TRANSP */
icalproperty* icalproperty_new_transp(char* v);
icalproperty* icalproperty_vanew_transp(char* v, ...);
void icalproperty_set_transp(icalproperty* prop, char* v);
char* icalproperty_get_transp(icalproperty* prop);

/* X-LIC-MIMEENCODING */
icalproperty* icalproperty_new_xlicmimeencoding(char* v);
icalproperty* icalproperty_vanew_xlicmimeencoding(char* v, ...);
void icalproperty_set_xlicmimeencoding(icalproperty* prop, char* v);
char* icalproperty_get_xlicmimeencoding(icalproperty* prop);

/* SEQUENCE */
icalproperty* icalproperty_new_sequence(int v);
icalproperty* icalproperty_vanew_sequence(int v, ...);
void icalproperty_set_sequence(icalproperty* prop, int v);
int icalproperty_get_sequence(icalproperty* prop);

/* LOCATION */
icalproperty* icalproperty_new_location(char* v);
icalproperty* icalproperty_vanew_location(char* v, ...);
void icalproperty_set_location(icalproperty* prop, char* v);
char* icalproperty_get_location(icalproperty* prop);

/* REQUEST-STATUS */
icalproperty* icalproperty_new_requeststatus(char* v);
icalproperty* icalproperty_vanew_requeststatus(char* v, ...);
void icalproperty_set_requeststatus(icalproperty* prop, char* v);
char* icalproperty_get_requeststatus(icalproperty* prop);

/* EXDATE */
icalproperty* icalproperty_new_exdate(struct icaltimetype v);
icalproperty* icalproperty_vanew_exdate(struct icaltimetype v, ...);
void icalproperty_set_exdate(icalproperty* prop, struct icaltimetype v);
struct icaltimetype icalproperty_get_exdate(icalproperty* prop);

/* TZID */
icalproperty* icalproperty_new_tzid(char* v);
icalproperty* icalproperty_vanew_tzid(char* v, ...);
void icalproperty_set_tzid(icalproperty* prop, char* v);
char* icalproperty_get_tzid(icalproperty* prop);

/* RESOURCES */
icalproperty* icalproperty_new_resources(char* v);
icalproperty* icalproperty_vanew_resources(char* v, ...);
void icalproperty_set_resources(icalproperty* prop, char* v);
char* icalproperty_get_resources(icalproperty* prop);

/* TZURL */
icalproperty* icalproperty_new_tzurl(char* v);
icalproperty* icalproperty_vanew_tzurl(char* v, ...);
void icalproperty_set_tzurl(icalproperty* prop, char* v);
char* icalproperty_get_tzurl(icalproperty* prop);

/* REPEAT */
icalproperty* icalproperty_new_repeat(int v);
icalproperty* icalproperty_vanew_repeat(int v, ...);
void icalproperty_set_repeat(icalproperty* prop, int v);
int icalproperty_get_repeat(icalproperty* prop);

/* PRIORITY */
icalproperty* icalproperty_new_priority(int v);
icalproperty* icalproperty_vanew_priority(int v, ...);
void icalproperty_set_priority(icalproperty* prop, int v);
int icalproperty_get_priority(icalproperty* prop);

/* FREEBUSY */
icalproperty* icalproperty_new_freebusy(struct icalperiodtype v);
icalproperty* icalproperty_vanew_freebusy(struct icalperiodtype v, ...);
void icalproperty_set_freebusy(icalproperty* prop, struct icalperiodtype v);
struct icalperiodtype icalproperty_get_freebusy(icalproperty* prop);

/* DTSTART */
icalproperty* icalproperty_new_dtstart(struct icaltimetype v);
icalproperty* icalproperty_vanew_dtstart(struct icaltimetype v, ...);
void icalproperty_set_dtstart(icalproperty* prop, struct icaltimetype v);
struct icaltimetype icalproperty_get_dtstart(icalproperty* prop);

/* RECURRENCE-ID */
icalproperty* icalproperty_new_recurrenceid(struct icaltimetype v);
icalproperty* icalproperty_vanew_recurrenceid(struct icaltimetype v, ...);
void icalproperty_set_recurrenceid(icalproperty* prop, struct icaltimetype v);
struct icaltimetype icalproperty_get_recurrenceid(icalproperty* prop);

/* SUMMARY */
icalproperty* icalproperty_new_summary(char* v);
icalproperty* icalproperty_vanew_summary(char* v, ...);
void icalproperty_set_summary(icalproperty* prop, char* v);
char* icalproperty_get_summary(icalproperty* prop);

/* DTEND */
icalproperty* icalproperty_new_dtend(struct icaltimetype v);
icalproperty* icalproperty_vanew_dtend(struct icaltimetype v, ...);
void icalproperty_set_dtend(icalproperty* prop, struct icaltimetype v);
struct icaltimetype icalproperty_get_dtend(icalproperty* prop);

/* TZNAME */
icalproperty* icalproperty_new_tzname(char* v);
icalproperty* icalproperty_vanew_tzname(char* v, ...);
void icalproperty_set_tzname(icalproperty* prop, char* v);
char* icalproperty_get_tzname(icalproperty* prop);

/* RDATE */
icalproperty* icalproperty_new_rdate(struct icalperiodtype v);
icalproperty* icalproperty_vanew_rdate(struct icalperiodtype v, ...);
void icalproperty_set_rdate(icalproperty* prop, struct icalperiodtype v);
struct icalperiodtype icalproperty_get_rdate(icalproperty* prop);

/* X-LIC-MIMEFILENAME */
icalproperty* icalproperty_new_xlicmimefilename(char* v);
icalproperty* icalproperty_vanew_xlicmimefilename(char* v, ...);
void icalproperty_set_xlicmimefilename(icalproperty* prop, char* v);
char* icalproperty_get_xlicmimefilename(icalproperty* prop);

/* URL */
icalproperty* icalproperty_new_url(char* v);
icalproperty* icalproperty_vanew_url(char* v, ...);
void icalproperty_set_url(icalproperty* prop, char* v);
char* icalproperty_get_url(icalproperty* prop);

/* X-LIC-CLUSTERCOUNT */
icalproperty* icalproperty_new_xlicclustercount(int v);
icalproperty* icalproperty_vanew_xlicclustercount(int v, ...);
void icalproperty_set_xlicclustercount(icalproperty* prop, int v);
int icalproperty_get_xlicclustercount(icalproperty* prop);

/* ATTACH */
icalproperty* icalproperty_new_attach(struct icalattachtype v);
icalproperty* icalproperty_vanew_attach(struct icalattachtype v, ...);
void icalproperty_set_attach(icalproperty* prop, struct icalattachtype v);
struct icalattachtype icalproperty_get_attach(icalproperty* prop);

/* EXRULE */
icalproperty* icalproperty_new_exrule(struct icalrecurrencetype v);
icalproperty* icalproperty_vanew_exrule(struct icalrecurrencetype v, ...);
void icalproperty_set_exrule(icalproperty* prop, struct icalrecurrencetype v);
struct icalrecurrencetype icalproperty_get_exrule(icalproperty* prop);

/* QUERY */
icalproperty* icalproperty_new_query(char* v);
icalproperty* icalproperty_vanew_query(char* v, ...);
void icalproperty_set_query(icalproperty* prop, char* v);
char* icalproperty_get_query(icalproperty* prop);

/* PERCENT-COMPLETE */
icalproperty* icalproperty_new_percentcomplete(int v);
icalproperty* icalproperty_vanew_percentcomplete(int v, ...);
void icalproperty_set_percentcomplete(icalproperty* prop, int v);
int icalproperty_get_percentcomplete(icalproperty* prop);

/* CALSCALE */
icalproperty* icalproperty_new_calscale(char* v);
icalproperty* icalproperty_vanew_calscale(char* v, ...);
void icalproperty_set_calscale(icalproperty* prop, char* v);
char* icalproperty_get_calscale(icalproperty* prop);

/* CREATED */
icalproperty* icalproperty_new_created(struct icaltimetype v);
icalproperty* icalproperty_vanew_created(struct icaltimetype v, ...);
void icalproperty_set_created(icalproperty* prop, struct icaltimetype v);
struct icaltimetype icalproperty_get_created(icalproperty* prop);

/* GEO */
icalproperty* icalproperty_new_geo(struct icalgeotype v);
icalproperty* icalproperty_vanew_geo(struct icalgeotype v, ...);
void icalproperty_set_geo(icalproperty* prop, struct icalgeotype v);
struct icalgeotype icalproperty_get_geo(icalproperty* prop);

/* X-LIC-MIMECHARSET */
icalproperty* icalproperty_new_xlicmimecharset(char* v);
icalproperty* icalproperty_vanew_xlicmimecharset(char* v, ...);
void icalproperty_set_xlicmimecharset(icalproperty* prop, char* v);
char* icalproperty_get_xlicmimecharset(icalproperty* prop);

/* COMPLETED */
icalproperty* icalproperty_new_completed(struct icaltimetype v);
icalproperty* icalproperty_vanew_completed(struct icaltimetype v, ...);
void icalproperty_set_completed(icalproperty* prop, struct icaltimetype v);
struct icaltimetype icalproperty_get_completed(icalproperty* prop);

/* DTSTAMP */
icalproperty* icalproperty_new_dtstamp(struct icaltimetype v);
icalproperty* icalproperty_vanew_dtstamp(struct icaltimetype v, ...);
void icalproperty_set_dtstamp(icalproperty* prop, struct icaltimetype v);
struct icaltimetype icalproperty_get_dtstamp(icalproperty* prop);

/* DUE */
icalproperty* icalproperty_new_due(struct icaltimetype v);
icalproperty* icalproperty_vanew_due(struct icaltimetype v, ...);
void icalproperty_set_due(icalproperty* prop, struct icaltimetype v);
struct icaltimetype icalproperty_get_due(icalproperty* prop);

/* ACTION */
icalproperty* icalproperty_new_action(char* v);
icalproperty* icalproperty_vanew_action(char* v, ...);
void icalproperty_set_action(icalproperty* prop, char* v);
char* icalproperty_get_action(icalproperty* prop);
#endif ICALPROPERTY_H
