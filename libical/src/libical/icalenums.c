/* -*- Mode: C -*- */
/*======================================================================
  FILE: icalenum.c
  CREATOR: eric 29 April 1999
  
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
  The original code is icalenum.c

  ======================================================================*/

#include "icalenums.h"

struct icalproperty_kind_map {
	icalproperty_kind kind;
	char name[20];
};

static struct icalproperty_kind_map property_map[] = 
{
    { ICAL_ACTION_PROPERTY, "ACTION"},
    { ICAL_ATTACH_PROPERTY, "ATTACH"},
    { ICAL_ATTENDEE_PROPERTY, "ATTENDEE"},
    { ICAL_CALSCALE_PROPERTY, "CALSCALE"},
    { ICAL_CATEGORIES_PROPERTY, "CATEGORIES"},
    { ICAL_CLASS_PROPERTY, "CLASS"},
    { ICAL_COMMENT_PROPERTY, "COMMENT"},
    { ICAL_COMPLETED_PROPERTY, "COMPLETED"},
    { ICAL_CONTACT_PROPERTY, "CONTACT"},
    { ICAL_CREATED_PROPERTY, "CREATED"},
    { ICAL_DESCRIPTION_PROPERTY, "DESCRIPTION"},
    { ICAL_DTEND_PROPERTY, "DTEND"},
    { ICAL_DTSTAMP_PROPERTY, "DTSTAMP"},
    { ICAL_DTSTART_PROPERTY, "DTSTART"},
    { ICAL_DUE_PROPERTY, "DUE"},
    { ICAL_DURATION_PROPERTY, "DURATION"},
    { ICAL_EXDATE_PROPERTY, "EXDATE"},
    { ICAL_EXRULE_PROPERTY, "EXRULE"},
    { ICAL_FREEBUSY_PROPERTY, "FREEBUSY"},
    { ICAL_GEO_PROPERTY, "GEO"},
    { ICAL_LASTMODIFIED_PROPERTY, "LAST-MODIFIED"},
    { ICAL_LOCATION_PROPERTY, "LOCATION"},
    { ICAL_METHOD_PROPERTY, "METHOD"},
    { ICAL_ORGANIZER_PROPERTY, "ORGANIZER"},
    { ICAL_PERCENTCOMPLETE_PROPERTY, "PERCENT-COMPLETE"},
    { ICAL_PRIORITY_PROPERTY, "PRIORITY"},
    { ICAL_PRODID_PROPERTY, "PRODID"},
    { ICAL_RDATE_PROPERTY, "RDATE"},
    { ICAL_RECURRENCEID_PROPERTY, "RECURRENCE-ID"},
    { ICAL_RELATEDTO_PROPERTY, "RELATED-TO"},
    { ICAL_REPEAT_PROPERTY, "REPEAT"},
    { ICAL_REQUESTSTATUS_PROPERTY, "REQUEST-STATUS"},
    { ICAL_RESOURCES_PROPERTY, "RESOURCES"},
    { ICAL_RRULE_PROPERTY, "RRULE"},
    { ICAL_SEQUENCE_PROPERTY, "SEQUENCE"},
    { ICAL_STATUS_PROPERTY, "STATUS"},
    { ICAL_SUMMARY_PROPERTY, "SUMMARY"},
    { ICAL_TRANSP_PROPERTY, "TRANSP"},
    { ICAL_TRIGGER_PROPERTY, "TRIGGER"},
    { ICAL_TZID_PROPERTY, "TZID"},
    { ICAL_TZNAME_PROPERTY, "TZNAME"},
    { ICAL_TZOFFSETFROM_PROPERTY, "TZOFFSETFROM"},
    { ICAL_TZOFFSETTO_PROPERTY, "TZOFFSETTO"},
    { ICAL_TZURL_PROPERTY, "TZURL"},
    { ICAL_UID_PROPERTY, "UID"},
    { ICAL_URL_PROPERTY, "URL"},
    { ICAL_VERSION_PROPERTY, "VERSION"},
    { ICAL_X_PROPERTY,"X_PROPERTY"},

    /* CAP Object Properties */

    { ICAL_SCOPE_PROPERTY, "SCOPE"},
    { ICAL_MAXRESULTS_PROPERTY, "MAXRESULTS"},
    { ICAL_MAXRESULTSSIZE_PROPERTY, "MAXRESULTSSIZE"},
    { ICAL_QUERY_PROPERTY, "QUERY" },
    { ICAL_QUERYNAME_PROPERTY, "QUERYNAME" },
    { ICAL_TARGET_PROPERTY, "TARGET"},

    /* libical private properties */
    { ICAL_XLICERROR_PROPERTY,"X-LIC-ERROR"},
    { ICAL_XLICCLUSTERCOUNT_PROPERTY,"X-LIC-CLUSTERCOUNT"},

    /* End of the list */
    { ICAL_NO_PROPERTY, ""}
};


char* icalenum_property_kind_to_string(icalproperty_kind kind)
{
    int i;

    for (i=0; property_map[i].kind != ICAL_NO_PROPERTY; i++) {
	if (property_map[i].kind == kind) {
	    return property_map[i].name;
	}
    }

    return 0;

}

icalproperty_kind icalenum_string_to_property_kind(char* string)
{
    int i;

    if (string ==0 ) { 
	return ICAL_NO_PROPERTY;
    }

    for (i=0; property_map[i].kind  != ICAL_NO_PROPERTY; i++) {
	if (strcmp(property_map[i].name, string) == 0) {
	    return property_map[i].kind;
	}
    }

    return ICAL_NO_PROPERTY;
}




struct icalparameter_kind_map {
	icalparameter_kind kind;
	char name[20];
};

static struct icalparameter_kind_map parameter_map[] = 
{
    { ICAL_ALTREP_PARAMETER, "ALTREP"},
    { ICAL_CN_PARAMETER, "CN"},
    { ICAL_CUTYPE_PARAMETER, "CUTYPE"},
    { ICAL_DELEGATEDFROM_PARAMETER, "DELEGATED-FROM"},
    { ICAL_DELEGATEDTO_PARAMETER, "DELEGATED-TO"},
    { ICAL_DIR_PARAMETER, "DIR"},
    { ICAL_ENCODING_PARAMETER, "ENCODING"},
    { ICAL_FBTYPE_PARAMETER, "FBTYPE"},
    { ICAL_FMTTYPE_PARAMETER, "FMTTYPE"},
    { ICAL_LANGUAGE_PARAMETER, "LANGUAGE"},
    { ICAL_MEMBER_PARAMETER, "MEMBER"},
    { ICAL_PARTSTAT_PARAMETER, "PARTSTAT"},
    { ICAL_RANGE_PARAMETER, "RANGE"},
    { ICAL_RELATED_PARAMETER, "RELATED"},
    { ICAL_RELTYPE_PARAMETER, "RELTYPE"},
    { ICAL_ROLE_PARAMETER, "ROLE"},
    { ICAL_RSVP_PARAMETER, "RSVP"},
    { ICAL_SENTBY_PARAMETER, "SENT-BY"},
    { ICAL_TZID_PARAMETER, "TZID"},
    { ICAL_VALUE_PARAMETER, "VALUE"},

    /* CAP parameters */

    /* libical private parameters */
    { ICAL_XLICERRORTYPE_PARAMETER, "X-LIC-ERRORTYPE"},
    { ICAL_XLICCOMPARETYPE_PARAMETER, "X-LIC-COMPARETYPE"},

    /* End of list */
    { ICAL_NO_PARAMETER, ""}
};

char* icalenum_parameter_kind_to_string(icalparameter_kind kind)
{
    int i;

    for (i=0; parameter_map[i].kind != ICAL_NO_PARAMETER; i++) {
	if (parameter_map[i].kind == kind) {
	    return parameter_map[i].name;
	}
    }

    return 0;

}

icalparameter_kind icalenum_string_to_parameter_kind(char* string)
{
    int i;

    if (string ==0 ) { 
	return ICAL_NO_PARAMETER;
    }

    for (i=0; parameter_map[i].kind  != ICAL_NO_PARAMETER; i++) {
	if (strcmp(parameter_map[i].name, string) == 0) {
	    return parameter_map[i].kind;
	}
    }

    return ICAL_NO_PARAMETER;
}

struct icalvalue_kind_map {
	icalvalue_kind kind;
	char name[20];
};

static struct icalvalue_kind_map value_map[] = 
{
    { ICAL_BINARY_VALUE, "BINARY"},
    { ICAL_BOOLEAN_VALUE, "BOOLEAN"},
    { ICAL_CALADDRESS_VALUE, "CAL-ADDRESS"},
    { ICAL_DATE_VALUE, "DATE"},
    { ICAL_DATETIME_VALUE, "DATE-TIME"},
    { ICAL_DURATION_VALUE, "DURATION"},
    { ICAL_FLOAT_VALUE, "FLOAT"},
    { ICAL_INTEGER_VALUE, "INTEGER"},
    { ICAL_PERIOD_VALUE, "PERIOD"},
    { ICAL_RECUR_VALUE, "RECUR"},
    { ICAL_TEXT_VALUE, "TEXT"},
    { ICAL_TIME_VALUE, "TIME"},
    { ICAL_URI_VALUE, "URI"},
    { ICAL_UTCOFFSET_VALUE, "UTC-OFFSET"},
    { ICAL_GEO_VALUE, "FLOAT"}, /* Not an RFC2445 type */
    { ICAL_ATTACH_VALUE, "XATTACH"}, /* Not an RFC2445 type */
    { ICAL_DATETIMEDATE_VALUE, "XDATETIMEDATE"}, /* Not an RFC2445 type */
    { ICAL_DATETIMEPERIOD_VALUE, "XDATETIMEPERIOD"}, /* Not an RFC2445 type */
    { ICAL_QUERY_VALUE, "QUERY"},
    { ICAL_NO_VALUE, ""},
};

char* icalenum_value_kind_to_string(icalvalue_kind kind)
{
    int i;

    for (i=0; value_map[i].kind != ICAL_NO_VALUE; i++) {
	if (value_map[i].kind == kind) {
	    return value_map[i].name;
	}
    }

    return 0;

}

icalvalue_kind icalenum_value_kind_by_prop(icalproperty_kind kind)
{

    return ICAL_NO_VALUE;
}


struct icalcomponent_kind_map {
	icalcomponent_kind kind;
	char name[20];
};

  

static struct icalcomponent_kind_map component_map[] = 
{
    { ICAL_VEVENT_COMPONENT, "VEVENT" },
    { ICAL_VTODO_COMPONENT, "VTODO" },
    { ICAL_VJOURNAL_COMPONENT, "VJOURNAL" },
    { ICAL_VCALENDAR_COMPONENT, "VCALENDAR" },
    { ICAL_VFREEBUSY_COMPONENT, "VFREEBUSY" },
    { ICAL_VTIMEZONE_COMPONENT, "VTIMEZONE" },
    { ICAL_VALARM_COMPONENT, "VALARM" },
    { ICAL_XSTANDARD_COMPONENT, "STANDARD" }, /*These are part of RFC2445 */
    { ICAL_XDAYLIGHT_COMPONENT, "DAYLIGHT" }, /*but are not really components*/
    { ICAL_X_COMPONENT, "X" },
    { ICAL_VSCHEDULE_COMPONENT, "SCHEDULE" },

    /* CAP components */
    { ICAL_VQUERY_COMPONENT, "VQUERY" },  
    { ICAL_VCAR_COMPONENT, "VCAR" },  
    { ICAL_VCOMMAND_COMPONENT, "VCOMMAND" },  

    /* libical private components */
    { ICAL_XLICINVALID_COMPONENT, "X-LIC-UNKNOWN" },  
    { ICAL_XROOT_COMPONENT, "ROOT" },  

    /* End of list */
    { ICAL_NO_COMPONENT, "" },
};

char* icalenum_component_kind_to_string(icalcomponent_kind kind)
{
    int i;

    for (i=0; component_map[i].kind != ICAL_NO_COMPONENT; i++) {
	if (component_map[i].kind == kind) {
	    return component_map[i].name;
	}
    }

    return 0;

}

icalcomponent_kind icalenum_string_to_component_kind(char* string)
{
    int i;

    if (string ==0 ) { 
	return ICAL_NO_COMPONENT;
    }

    for (i=0; component_map[i].kind  != ICAL_NO_COMPONENT; i++) {
	if (strcmp(component_map[i].name, string) == 0) {
	    return component_map[i].kind;
	}
    }

    return ICAL_NO_COMPONENT;
}

struct  icalproperty_kind_value_map {
	icalproperty_kind prop;
	icalvalue_kind value;
};

static struct icalproperty_kind_value_map propval_map[] = 
{
    { ICAL_CALSCALE_PROPERTY, ICAL_TEXT_VALUE }, 
    { ICAL_METHOD_PROPERTY, ICAL_TEXT_VALUE }, 
    { ICAL_PRODID_PROPERTY, ICAL_TEXT_VALUE }, 
    { ICAL_VERSION_PROPERTY, ICAL_TEXT_VALUE }, 
    { ICAL_CATEGORIES_PROPERTY, ICAL_TEXT_VALUE }, 
    { ICAL_CLASS_PROPERTY, ICAL_TEXT_VALUE }, 
    { ICAL_COMMENT_PROPERTY, ICAL_TEXT_VALUE }, 
    { ICAL_DESCRIPTION_PROPERTY, ICAL_TEXT_VALUE }, 
    { ICAL_LOCATION_PROPERTY, ICAL_TEXT_VALUE }, 
    { ICAL_PERCENTCOMPLETE_PROPERTY, ICAL_INTEGER_VALUE }, 
    { ICAL_PRIORITY_PROPERTY, ICAL_INTEGER_VALUE }, 
    { ICAL_RESOURCES_PROPERTY, ICAL_TEXT_VALUE }, 
    { ICAL_STATUS_PROPERTY, ICAL_TEXT_VALUE }, 
    { ICAL_SUMMARY_PROPERTY, ICAL_TEXT_VALUE }, 
    { ICAL_COMPLETED_PROPERTY, ICAL_DATETIME_VALUE }, 
    { ICAL_FREEBUSY_PROPERTY, ICAL_PERIOD_VALUE }, 
    { ICAL_TRANSP_PROPERTY, ICAL_TEXT_VALUE }, 
    { ICAL_TZNAME_PROPERTY, ICAL_TEXT_VALUE }, 
    { ICAL_TZOFFSETFROM_PROPERTY, ICAL_UTCOFFSET_VALUE }, 
    { ICAL_TZOFFSETTO_PROPERTY, ICAL_UTCOFFSET_VALUE }, 
    { ICAL_TZURL_PROPERTY, ICAL_URI_VALUE }, 
    { ICAL_TZID_PROPERTY, ICAL_TEXT_VALUE }, 
    { ICAL_ATTENDEE_PROPERTY, ICAL_CALADDRESS_VALUE }, 
    { ICAL_CONTACT_PROPERTY, ICAL_TEXT_VALUE }, 
    { ICAL_ORGANIZER_PROPERTY, ICAL_CALADDRESS_VALUE }, 
    { ICAL_RELATEDTO_PROPERTY, ICAL_TEXT_VALUE }, 
    { ICAL_URL_PROPERTY, ICAL_URI_VALUE }, 
    { ICAL_UID_PROPERTY, ICAL_TEXT_VALUE }, 
    { ICAL_EXRULE_PROPERTY, ICAL_RECUR_VALUE }, 
    { ICAL_RRULE_PROPERTY, ICAL_RECUR_VALUE }, 
    { ICAL_ACTION_PROPERTY, ICAL_TEXT_VALUE }, 
    { ICAL_REPEAT_PROPERTY, ICAL_INTEGER_VALUE }, 
    { ICAL_CREATED_PROPERTY, ICAL_DATETIME_VALUE }, 
    { ICAL_DTSTAMP_PROPERTY, ICAL_DATETIME_VALUE }, 
    { ICAL_LASTMODIFIED_PROPERTY, ICAL_DATETIME_VALUE }, 
    { ICAL_SEQUENCE_PROPERTY, ICAL_INTEGER_VALUE }, 
    { ICAL_X_PROPERTY, ICAL_TEXT_VALUE }, 
    { ICAL_REQUESTSTATUS_PROPERTY, ICAL_TEXT_VALUE }, 
    { ICAL_ATTACH_PROPERTY, ICAL_URI_VALUE }, 
    { ICAL_GEO_PROPERTY, ICAL_GEO_VALUE }, 
    { ICAL_DTEND_PROPERTY, ICAL_DATETIME_VALUE }, 
    { ICAL_DUE_PROPERTY, ICAL_DATETIME_VALUE }, 
    { ICAL_DTSTART_PROPERTY, ICAL_DATETIME_VALUE }, 
    { ICAL_RECURRENCEID_PROPERTY, ICAL_DATETIME_VALUE }, 
    { ICAL_EXDATE_PROPERTY, ICAL_DATETIME_VALUE }, 
    { ICAL_RDATE_PROPERTY, ICAL_DATETIME_VALUE }, 
    { ICAL_TRIGGER_PROPERTY, ICAL_DURATION_VALUE }, 
    { ICAL_DURATION_PROPERTY, ICAL_DURATION_VALUE }, 

    /* CAP properties */
    { ICAL_SCOPE_PROPERTY, ICAL_TEXT_VALUE },
    { ICAL_MAXRESULTS_PROPERTY,  ICAL_INTEGER_VALUE},
    { ICAL_MAXRESULTSSIZE_PROPERTY,  ICAL_INTEGER_VALUE},
    { ICAL_QUERY_PROPERTY, ICAL_QUERY_VALUE },
    { ICAL_QUERYNAME_PROPERTY, ICAL_TEXT_VALUE },
    { ICAL_TARGET_PROPERTY, ICAL_CALADDRESS_VALUE },


    /* libical private properties */
    { ICAL_XLICERROR_PROPERTY,ICAL_TEXT_VALUE},
    { ICAL_XLICCLUSTERCOUNT_PROPERTY,ICAL_INTEGER_VALUE},


    /* End of list */
    { ICAL_NO_PROPERTY, ICAL_NO_PROPERTY}
};


icalvalue_kind icalenum_property_kind_to_value_kind(icalproperty_kind kind)
{
    int i;

    for (i=0; propval_map[i].value  != ICAL_NO_VALUE; i++) {
	if ( propval_map[i].prop == kind ) {
	    return propval_map[i].value;
	}
    }

    return ICAL_NO_VALUE;
}

struct {icalrecurrencetype_weekday wd; char * str; } 
wd_map[] = {
    {ICAL_SUNDAY_WEEKDAY,"SU"},
    {ICAL_MONDAY_WEEKDAY,"MO"},
    {ICAL_TUESDAY_WEEKDAY,"TU"},
    {ICAL_WEDNESDAY_WEEKDAY,"WE"},
    {ICAL_THURSDAY_WEEKDAY,"TH"},
    {ICAL_FRIDAY_WEEKDAY,"FR"},
    {ICAL_SATURDAY_WEEKDAY,"SA"},
    {ICAL_NO_WEEKDAY,0}
};

char* icalenum_weekday_to_string(icalrecurrencetype_weekday kind)
{
    int i;

    for (i=0; wd_map[i].wd  != ICAL_NO_WEEKDAY; i++) {
	if ( wd_map[i].wd ==  kind) {
	    return wd_map[i].str;
	}
    }

    return 0;
}


struct {
	icalrecurrencetype_frequency kind;
	char* str;
} freq_map[] = {
    {ICAL_SECONDLY_RECURRENCE,"SECONDLY"},
    {ICAL_MINUTELY_RECURRENCE,"MINUTELY"},
    {ICAL_HOURLY_RECURRENCE,"HOURLY"},
    {ICAL_DAILY_RECURRENCE,"DAILY"},
    {ICAL_WEEKLY_RECURRENCE,"WEEKLY"},
    {ICAL_MONTHLY_RECURRENCE,"MONTHLY"},
    {ICAL_YEARLY_RECURRENCE,"YEARLY"},
    {ICAL_NO_RECURRENCE,0}
};

char* icalenum_recurrence_to_string(icalrecurrencetype_frequency kind)
{
    int i;

    for (i=0; freq_map[i].kind != ICAL_NO_RECURRENCE ; i++) {
	if ( freq_map[i].kind == kind ) {
	    return freq_map[i].str;
	}
    }
    return 0;
}


struct {
	icalrecurrencetype_frequency kind;
	int major;
	int minor;
	char* str;
} status_map[] = {
    {ICAL_2_0_SUCCESS_STATUS, 2,0,"Success."},
    {ICAL_2_1_FALLBACK_STATUS, 2,1,"Success but fallback taken  on one or more property  values."},
    {ICAL_2_2_IGPROP_STATUS, 2,2,"Success, invalid property ignored."},
    {ICAL_2_3_IGPARAM_STATUS, 2,3,"Success, invalid property parameter ignored."},
    {ICAL_2_4_IGXPROP_STATUS, 2,4,"Success, unknown non-standard property ignored."},
    {ICAL_2_5_IGXPARAM_STATUS, 2,5,"Success, unknown non standard property value  ignored."},
    {ICAL_2_6_IGCOMP_STATUS, 2,6,"Success, invalid calendar component ignored."},
    {ICAL_2_7_FORWARD_STATUS, 2,7,"Success, request forwarded to Calendar User."},
    {ICAL_2_8_ONEEVENT_STATUS, 2,8,"Success, repeating event ignored. Scheduled as a  single component."},
    {ICAL_2_9_TRUNC_STATUS, 2,9,"Success, truncated end date time to date boundary."},
    {ICAL_2_10_ONETODO_STATUS, 2,10,"Success, repeating VTODO ignored. Scheduled as a  single VTODO."},
    {ICAL_2_11_TRUNCRRULE_STATUS, 2,11,"Success, unbounded RRULE clipped at some finite  number of instances  "},
    {ICAL_3_0_INVPROPNAME_STATUS, 3,0,"Invalid property name."},
    {ICAL_3_1_INVPROPVAL_STATUS, 3,1,"Invalid property value."},
    {ICAL_3_2_INVPARAM_STATUS, 3,2,"Invalid property parameter."},
    {ICAL_3_3_INVPARAMVAL_STATUS, 3,3,"Invalid property parameter  value."},
    {ICAL_3_4_INVCOMP_STATUS, 3,4,"Invalid calendar component  sequence."},
    {ICAL_3_5_INVTIME_STATUS, 3,5,"Invalid date or time."},
    {ICAL_3_6_INVRULE_STATUS, 3,6,"Invalid rule."},
    {ICAL_3_7_INVCU_STATUS, 3,7,"Invalid Calendar User."},
    {ICAL_3_8_NOAUTH_STATUS, 3,8,"No authority."},
    {ICAL_3_9_BADVERSION_STATUS, 3,9,"Unsupported version."},
    {ICAL_3_10_TOOBIG_STATUS, 3,10,"Request entity too large."},
    {ICAL_3_11_MISSREQCOMP_STATUS, 3,11,"Required component or property missing."},
    {ICAL_3_12_UNKCOMP_STATUS, 3,12,"Unknown component or property found."},
    {ICAL_3_13_BADCOMP_STATUS, 3,13,"Unsupported component or property found"},
    {ICAL_3_14_NOCAP_STATUS, 3,14,"Unsupported capability."},
    {ICAL_4_0_BUSY_STATUS, 4,0,"Event conflict. Date/time  is busy."},
    {ICAL_5_0_MAYBE_STATUS, 5,0,"Request MAY supported."},
    {ICAL_5_1_UNAVAIL_STATUS, 5,1,"Service unavailable."},
    {ICAL_5_2_NOSERVICE_STATUS, 5,2,"Invalid calendar service."},
    {ICAL_5_3_NOSCHED_STATUS, 5,3,"No scheduling support for  user."}
};

struct {icalproperty_method method; char* str;} method_map[] = {
    {ICAL_METHOD_PUBLISH,"PUBLISH"},
    {ICAL_METHOD_REQUEST,"REQUEST"},
    {ICAL_METHOD_REPLY,"REPLY"},
    {ICAL_METHOD_ADD,"ADD"},
    {ICAL_METHOD_CANCEL,"CANCEL"},
    {ICAL_METHOD_REFRESH,"REFRESH"},
    {ICAL_METHOD_COUNTER,"CPUNTER"},
    {ICAL_METHOD_DECLINECOUNTER,"DECLINECOUNTER"},
    /* CAP Methods */
    {ICAL_METHOD_CREATE,"CREATE"},
    {ICAL_METHOD_READ,"READ"},
    {ICAL_METHOD_RESPONSE,"RESPONSE"},
    {ICAL_METHOD_MOVE,"MOVE"},
    {ICAL_METHOD_MODIFY,"MODIFY"},
    {ICAL_METHOD_GENERATEUID,"GENERATEUID"},
    {ICAL_METHOD_DELETE,"DELETE"},
    {ICAL_METHOD_NONE,"NONE"}
};


char* icalenum_method_to_string(icalproperty_method method)
{
    int i;

    for (i=0; method_map[i].method  != ICAL_METHOD_NONE; i++) {
	if ( method_map[i].method ==  method) {
	    return method_map[i].str;
	}
    }

    return 0;
}

icalproperty_method icalenum_string_to_method(char* str)
{
    int i;

    for (i=0; method_map[i].method  != ICAL_METHOD_NONE; i++) {
	if ( strcmp(method_map[i].str, str) == 0) {
	    return method_map[i].method;
	}
    }

    return ICAL_METHOD_NONE;
}
