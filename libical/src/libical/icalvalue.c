/* -*- Mode: C -*- */
/*======================================================================
  FILE: icalvalue.c
  CREATOR: eric 02 May 1999
  
  $Id$

 (C) COPYRIGHT 2000, Eric Busboom, http://www.softwarestudio.org

 This program is free software; you can redistribute it and/or modify
 it under the terms of either: 

    The LGPL as published by the Free Software Foundation, version
    2.1, available at: http://www.fsf.org/copyleft/lesser.html

  Or:

    The Mozilla Public License Version 1.0. You may obtain a copy of
    the License at http://www.mozilla.org/MPL/

  The original code is icalvalue.c

  Contributions from:
     Graham Davison (g.m.davison@computer.org)


======================================================================*/

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif


#include "ical.h"
#include "icalerror.h"
#include "icalmemory.h"
#include "icalparser.h"
#include "icalenums.h"

#include <stdlib.h> /* for malloc */
#include <stdio.h> /* for sprintf */
#include <string.h> /* For memset, others */
#include <stddef.h> /* For offsetof() macro */
#include <errno.h>
#include <time.h> /* for mktime */
#include <stdlib.h> /* for atoi and atof */
#include <limits.h> /* for SHRT_MAX */

#if _MAC_OS_
#include "icalmemory_strdup.h"
#endif

#define TMP_BUF_SIZE 1024

void print_datetime_to_string(char* str,  struct icaltimetype *data);
void print_date_to_string(char* str,  struct icaltimetype *data);
void print_time_to_string(char* str,  struct icaltimetype *data);
void print_recur_to_string(char* str,  struct icaltimetype *data);

struct icalvalue_impl {
	icalvalue_kind kind;
	char id[5];
	int size;
	icalproperty* parent;

	union data {
		struct icalattachtype v_attach;		
		/* void *v_binary; */ /* use v_attach */
		char *v_string;
		/*char *v_text;*/
		/*char *v_caladdress;*/
		/*char *v_uri;*/
		float v_float;
		int v_int;
		/*int v_boolean;*/
		/*int v_integer;*/
		struct icaldurationtype v_duration;
		/*int v_utcoffset;*/

		struct icalperiodtype v_period;
		/*struct icalperiodtype v_datetimeperiod;*/
		struct icalgeotype v_geo;
		/*time_t v_time;*/
		struct icaltimetype v_time;
		/*struct icaltimetype v_date;*/
		/*struct icaltimetype v_datetime;*/
		/*struct icaltimetype v_datetimedate;*/

		/* struct icalrecurrencetype was once included
                   directly ( not referenced ) in this union, but it
                   contributes 2000 bytes to every value, so now it is
                   a reference*/

		struct icalrecurrencetype *v_recur;
		union icaltriggertype v_trigger;
		icalproperty_method v_method;

	} data;
};

struct icalvalue_impl*  icalvalue_new_impl(icalvalue_kind kind){

    struct icalvalue_impl* v;

    if ( ( v = (struct icalvalue_impl*)
	   malloc(sizeof(struct icalvalue_impl))) == 0) {
	icalerror_set_errno(ICAL_NEWFAILED_ERROR);
	return 0;
    }
    
    strcpy(v->id,"val");
    
    v->kind = kind;
    v->size = 0;
    v->parent = 0;
    memset(&(v->data),0,sizeof(v->data));
    
    return v;

}



icalvalue*
icalvalue_new (icalvalue_kind kind)
{
    return (icalvalue*)icalvalue_new_impl(kind);
}

icalvalue* icalvalue_new_clone(icalvalue* value){

    struct icalvalue_impl* new;
    struct icalvalue_impl* old = (struct icalvalue_impl*)value;

    new = icalvalue_new_impl(old->kind);

    if (new == 0){
	return 0;
    }

    
    strcpy(new->id, old->id);
    new->kind = old->kind;
    new->size = old->size;

    switch (new->kind){

	/* The contents of the attach value may or may not be owned by the 
	 * library. */
	case ICAL_ATTACH_VALUE: 
	case ICAL_BINARY_VALUE: 
	{
	    /* HACK ugh. I don't feel like impleenting this */
	}

	case ICAL_STRING_VALUE:
	case ICAL_TEXT_VALUE:
	case ICAL_CALADDRESS_VALUE:
	case ICAL_URI_VALUE:
	{
	    if (old->data.v_string != 0) { 
		new->data.v_string=icalmemory_strdup(old->data.v_string);

		if ( new->data.v_string == 0 ) {
		    return 0;
		}		    

	    }
	    break;
	}
	case ICAL_RECUR_VALUE:
	{
	    if(old->data.v_recur != 0){
		new->data.v_recur = malloc(sizeof(struct icalrecurrencetype));

		if(new->data.v_recur == 0){
		    return 0;
		}

		memcpy(	new->data.v_recur, old->data.v_recur,
			sizeof(struct icalrecurrencetype));	
	    }
	    break;
	}

	default:
	{
	    /* all of the other types are stored as values, not
               pointers, so we can just copy the whole structure. */

	    new->data = old->data;
	}
    }

    return new;
}

char* icalmemory_strdup_and_dequote(char* str)
{
    char* p;
    char* out = (char*)malloc(sizeof(char) * strlen(str) +1);
    char* pout;

    if (out == 0){
	return 0;
    }

    pout = out;

    for (p = str; *p!=0; p++){
	
	if( *p == '\\')
	{
	    p++;
	    switch(*p){
		case 0:
		{
		    break;
		    *pout = '\0';
		}
		case 'n':
		{
		    *pout = '\n';
		    break;
		}
		case 'N':
		{
		    *pout = '\n';
		    break;
		}
		case '\\':
		case ',':
		case ';':
		{
		    *pout = *p;
		    break;
		}
		default:
		{
		    *pout = ' ';
		}		
	    }
	} else {
	    *pout = *p;
	}

	pout++;
	
    }

    *pout = '\0';

    return out;
}

icalvalue* icalvalue_new_from_string_with_error(icalvalue_kind kind,char* str,icalproperty** error)
{

    icalvalue *value = 0;
    
    icalerror_check_arg_rz(str!=0,"str");

    if (error != 0){
	*error = 0;
    }

    switch (kind){
	
        case ICAL_ATTACH_VALUE:
	{
	    /* HACK */
	    value = 0;

	    if (error != 0){
		char temp[TMP_BUF_SIZE];
		sprintf(temp,"ATTACH Values are not implemented"); 
		*error = icalproperty_vanew_xlicerror( 
		    temp, 
		    icalparameter_new_xlicerrortype( 
			ICAL_XLICERRORTYPE_VALUEPARSEERROR), 
		    0); 
	    }

	    icalerror_warn("Parsing ATTACH properties is unimplmeneted");
	    break;
	}

	case ICAL_BINARY_VALUE:
	{
	    /* HACK */
	    value = 0;

	    if (error != 0){
		char temp[TMP_BUF_SIZE];
		sprintf(temp,"BINARY Values are not implemented"); 
		*error = icalproperty_vanew_xlicerror( 
		    temp, 
		    icalparameter_new_xlicerrortype( 
			ICAL_XLICERRORTYPE_VALUEPARSEERROR), 
		    0); 
	    }

	    icalerror_warn("Parsing BINARY values is unimplmeneted");
	    break;
	}

	case ICAL_BOOLEAN_VALUE:
	{
	    /* HACK */
	    value = 0;

	    if (error != 0){
		char temp[TMP_BUF_SIZE];
		sprintf(temp,"BOOLEAN Values are not implemented"); 
		*error = icalproperty_vanew_xlicerror( 
		    temp, 
		    icalparameter_new_xlicerrortype( 
			ICAL_XLICERRORTYPE_VALUEPARSEERROR), 
		    0); 
	    }

	    icalerror_warn("Parsing BOOLEAN values is unimplmeneted");
	    break;
	}

	case ICAL_INTEGER_VALUE:
	{
	    value = icalvalue_new_integer(atoi(str));
	    break;
	}

	case ICAL_FLOAT_VALUE:
	{
	    value = icalvalue_new_float(atof(str));
	    break;
	}

	case ICAL_UTCOFFSET_VALUE:
	{
	    value = icalparser_parse_value(kind,str,(icalcomponent*)0);
	    break;
	}

	case ICAL_TEXT_VALUE:
	{
	    char* dequoted_str = icalmemory_strdup_and_dequote(str);
	    value = icalvalue_new_text(dequoted_str);
	    free(dequoted_str);
	    break;
	}


	case ICAL_STRING_VALUE:
	{
	    value = icalvalue_new_string(str);
	    break;
	}

	case ICAL_CALADDRESS_VALUE:
	{
	    value = icalvalue_new_caladdress(str);
	    break;
	}

	case ICAL_URI_VALUE:
	{
	    value = icalvalue_new_uri(str);
	    break;
	}

	case ICAL_METHOD_VALUE:
	{
	    icalproperty_method method = icalenum_string_to_method(str);

	    if(method == ICAL_METHOD_NONE){
		value = 0;
	    } else {
		value = icalvalue_new_method(method);
	    }

	    break; 

	}
	case ICAL_GEO_VALUE:
	{
	    value = 0;
	    /* HACK */

	    if (error != 0){
		char temp[TMP_BUF_SIZE];
		sprintf(temp,"GEO Values are not implemented"); 
		*error = icalproperty_vanew_xlicerror( 
		    temp, 
		    icalparameter_new_xlicerrortype( 
			ICAL_XLICERRORTYPE_VALUEPARSEERROR), 
		    0); 
	    }

	    /*icalerror_warn("Parsing GEO properties is unimplmeneted");*/

	    break;
	}

	case ICAL_RECUR_VALUE:
	case ICAL_DATE_VALUE:
	case ICAL_DATETIME_VALUE:
	case ICAL_DATETIMEDATE_VALUE:
	case ICAL_DATETIMEPERIOD_VALUE:
	case ICAL_TIME_VALUE:
	case ICAL_DURATION_VALUE:
	case ICAL_PERIOD_VALUE:
	case ICAL_TRIGGER_VALUE:
	{
	    value = icalparser_parse_value(kind,str,error);
	    break;
	}

	default:
	{

	    if (error != 0 ){
		char temp[TMP_BUF_SIZE];

                snprintf(temp,TMP_BUF_SIZE,"Unknown type for \'%s\'",str);
			    
		*error = icalproperty_vanew_xlicerror( 
		    temp, 
		    icalparameter_new_xlicerrortype( 
			ICAL_XLICERRORTYPE_VALUEPARSEERROR), 
		    0); 
	    }

	    icalerror_warn("icalvalue_new_from_string got an unknown value type");
            value=0;
	}
    }


    if (error != 0 && *error == 0 && value == 0){
	char temp[TMP_BUF_SIZE];
	
        snprintf(temp,TMP_BUF_SIZE,"Failed to parse value: \'%s\'",str);
	
	*error = icalproperty_vanew_xlicerror( 
	    temp, 
	    icalparameter_new_xlicerrortype( 
		ICAL_XLICERRORTYPE_VALUEPARSEERROR), 
	    0); 
    }


    return value;

}

icalvalue* icalvalue_new_from_string(icalvalue_kind kind,char* str)
{
    return icalvalue_new_from_string_with_error(kind,str,(icalproperty*)0);
}



void
icalvalue_free (icalvalue* value)
{
    struct icalvalue_impl* v = (struct icalvalue_impl*)value;

    icalerror_check_arg_rv((value != 0),"value");

#ifdef ICAL_FREE_ON_LIST_IS_ERROR
    icalerror_assert( (v->parent ==0),"This value is still attached to a property");
    
#else
    if(v->parent !=0){
	return;
    }
#endif


    switch (v->kind){
	case ICAL_BINARY_VALUE: 
	case ICAL_ATTACH_VALUE: {
	    /* HACK ugh. This will be tough to implement */
	}
	case ICAL_TEXT_VALUE:
	case ICAL_CALADDRESS_VALUE:
	case ICAL_URI_VALUE:
	{
	    if (v->data.v_string != 0) { 
		free(v->data.v_string);
		v->data.v_string = 0;
	    }
	    break;
	}
	case ICAL_RECUR_VALUE:
	{
	    if(v->data.v_recur != 0){
		free(v->data.v_recur);
		v->data.v_recur = 0;
	    }
	    break;
	}

	default:
	{
	    /* Nothing to do */
	}
    }

    v->kind = ICAL_NO_VALUE;
    v->size = 0;
    v->parent = 0;
    memset(&(v->data),0,sizeof(v->data));
    v->id[0] = 'X';
    free(v);
}

int
icalvalue_is_valid (icalvalue* value)
{
    /*struct icalvalue_impl* v = (struct icalvalue_impl*)value;*/
    
    if(value == 0){
	return 0;
    }
    
    return 1;
}

char* icalvalue_binary_as_ical_string(icalvalue* value) {

    char* data;
    char* str;
    icalerror_check_arg_rz( (value!=0),"value");
    data = icalvalue_get_binary(value);

    str = (char*)icalmemory_tmp_buffer(60);
    sprintf(str,"icalvalue_binary_as_ical_string is not implemented yet");

    return str;
}


char* icalvalue_int_as_ical_string(icalvalue* value) {
    
    int data;
    char* str = (char*)icalmemory_tmp_buffer(2);
    icalerror_check_arg_rz( (value!=0),"value");
    data = icalvalue_get_integer(value);
	
    sprintf(str,"%d",data);

    return str;
}

char* icalvalue_utcoffset_as_ical_string(icalvalue* value)
{    
    int data,h,m,s;
    char sign;
    char* str = (char*)icalmemory_tmp_buffer(9);
    icalerror_check_arg_rz( (value!=0),"value");
    data = icalvalue_get_utcoffset(value);

    if (abs(data) == data){
	sign = '+';
    } else {
	sign = '-';
    }

    h = data/3600;
    m = (data - (h*3600))/ 60;
    s = (data - (h*3600) - (m*60));

    sprintf(str,"%c%02d%02d%02d",sign,abs(h),abs(m),abs(s));

    return str;
}

char* icalvalue_string_as_ical_string(icalvalue* value) {

    char* data;
    char* str = 0;
    icalerror_check_arg_rz( (value!=0),"value");
    data = ((struct icalvalue_impl*)value)->data.v_string;

    str = (char*)icalmemory_tmp_buffer(strlen(data)+1);   

    strcpy(str,data);

    return str;
}


char* icalvalue_recur_as_ical_string(icalvalue* value) 
{
    char* str;
    char *str_p;
    size_t buf_sz = 200;
    char temp[20];
    int i,j;
    struct icalvalue_impl *impl = (struct icalvalue_impl*)value;
    struct icalrecurrencetype *recur = impl->data.v_recur;

    struct { char* str;size_t offset; short limit;  } recurmap[] = 
      {
        {";BYSECOND=",offsetof(struct icalrecurrencetype,by_second),60},
        {";BYMINUTE=",offsetof(struct icalrecurrencetype,by_minute),60},
        {";BYHOUR=",offsetof(struct icalrecurrencetype,by_hour),24},
        {";BYDAY=",offsetof(struct icalrecurrencetype,by_day),7},
        {";BYMONTHDAY=",offsetof(struct icalrecurrencetype,by_month_day),31},
        {";BYYEARDAY=",offsetof(struct icalrecurrencetype,by_year_day),366},
        {";BYWEEKNO=",offsetof(struct icalrecurrencetype,by_week_no),52},
        {";BYMONTH=",offsetof(struct icalrecurrencetype,by_month),12},
        {";BYSETPOS=",offsetof(struct icalrecurrencetype,by_set_pos),366},
        {0,0,0},
      };



    icalerror_check_arg_rz((value != 0),"value");

    if(recur->freq == ICAL_NO_RECURRENCE){
	return 0;
    }

    str = (char*)icalmemory_tmp_buffer(buf_sz);
    str_p = str;

    icalmemory_append_string(&str,&str_p,&buf_sz,"FREQ=");
    icalmemory_append_string(&str,&str_p,&buf_sz,
			     icalenum_recurrence_to_string(recur->freq));

    if(recur->until.year != 0){
	
	temp[0] = 0;
	print_datetime_to_string(temp,&(recur->until));
	
	icalmemory_append_string(&str,&str_p,&buf_sz,";UNTIL=");
	icalmemory_append_string(&str,&str_p,&buf_sz, temp);
    }

    if(recur->count != 0){
	sprintf(temp,"%d",recur->count);
	icalmemory_append_string(&str,&str_p,&buf_sz,";COUNT=");
	icalmemory_append_string(&str,&str_p,&buf_sz, temp);
    }

    if(recur->interval != 0){
	sprintf(temp,"%d",recur->interval);
	icalmemory_append_string(&str,&str_p,&buf_sz,";INTERVAL=");
	icalmemory_append_string(&str,&str_p,&buf_sz, temp);
    }
    
    for(j =0; recurmap[j].str != 0; j++){
	short* array = (short*)(recurmap[j].offset+ (size_t)recur);
	short limit = recurmap[j].limit;

	/* Skip unused arrays */
	if( array[0] != ICAL_RECURRENCE_ARRAY_MAX ) {

	    icalmemory_append_string(&str,&str_p,&buf_sz,recurmap[j].str);
	    
	    for(i=0; i< limit  && array[i] != ICAL_RECURRENCE_ARRAY_MAX;
		i++){
		if (j == 3) { /* BYDAY */
		    short dow = icalrecurrencetype_day_day_of_week(array[i]);
		    char *daystr = icalenum_weekday_to_string(dow);

		    /* HACK, does not correctly handle the integer value */
		    icalmemory_append_string(&str,&str_p,&buf_sz,daystr);
		} else {
		    sprintf(temp,"%d",array[i]);
		    icalmemory_append_string(&str,&str_p,&buf_sz, temp);
		}
		
		if( (i+1)<limit &&array[i+1] 
		    != ICAL_RECURRENCE_ARRAY_MAX){
		    icalmemory_append_char(&str,&str_p,&buf_sz,',');
		}
	    }	 
	}   
    }

    return  str;
}

char* icalvalue_text_as_ical_string(icalvalue* value) {

    char *str;
    char *str_p;
    char *rtrn;
    char *p;
    size_t buf_sz;
    int line_length;

    line_length = 0;

    buf_sz = strlen(((struct icalvalue_impl*)value)->data.v_string)+1;

    str_p = str = (char*)icalmemory_new_buffer(buf_sz);

    if (str_p == 0){
      return 0;
    }

    for(p=((struct icalvalue_impl*)value)->data.v_string; *p!=0; p++){

	switch(*p){
	    case '\n': {
		icalmemory_append_string(&str,&str_p,&buf_sz,"\\n");
		line_length+=3;
		break;
	    }

	    case '\t': {
		icalmemory_append_string(&str,&str_p,&buf_sz,"\\t");
		line_length+=3;
		break;
	    }
	    case '\r': {
		icalmemory_append_string(&str,&str_p,&buf_sz,"\\r");
		line_length+=3;
		break;
	    }
	    case '\b': {
		icalmemory_append_string(&str,&str_p,&buf_sz,"\\b");
		line_length+=3;
		break;
	    }
	    case '\f': {
		icalmemory_append_string(&str,&str_p,&buf_sz,"\\f");
		line_length+=3;
		break;
	    }

	    case ';':
	    case ',':{
		icalmemory_append_char(&str,&str_p,&buf_sz,'\\');
		icalmemory_append_char(&str,&str_p,&buf_sz,*p);
		line_length+=3;
		break;
	    }

	    case '"':{
		icalmemory_append_char(&str,&str_p,&buf_sz,'\\');
		icalmemory_append_char(&str,&str_p,&buf_sz,*p);
		line_length+=3;
		break;
	    }

	    default: {
		icalmemory_append_char(&str,&str_p,&buf_sz,*p);
		line_length++;
	    }
	}

	if (line_length > 65 && *p == ' '){
	    icalmemory_append_string(&str,&str_p,&buf_sz,"\n ");
	    line_length=0;
	}


	if (line_length > 75){
	    icalmemory_append_string(&str,&str_p,&buf_sz,"\n ");
	    line_length=0;
	}

    }

    /* Assume the last character is not a '\0' and add one. We could
       check *str_p != 0, but that would be an uninitialized memory
       read. */


    icalmemory_append_char(&str,&str_p,&buf_sz,'\0');

    rtrn = icalmemory_tmp_copy(str);

    icalmemory_free_buffer(str);

    return rtrn;
}


char* icalvalue_attach_as_ical_string(icalvalue* value) {

    struct icalattachtype a;
    char * str;

    icalerror_check_arg_rz( (value!=0),"value");

    a = icalvalue_get_attach(value);

    if (a.binary != 0) {
	return  icalvalue_binary_as_ical_string(value);
    } else if (a.base64 != 0) {
	str = (char*)icalmemory_tmp_buffer(strlen(a.base64)+1);
	strcpy(str,a.base64);
	return str;
    } else if (a.url != 0){
	return icalvalue_string_as_ical_string(value);
    } else {
	icalerrno = ICAL_MALFORMEDDATA_ERROR;
	return 0;
    }
}

void append_duration_segment(char** buf, char** buf_ptr, size_t* buf_size, 
			     char* sep, unsigned int value) {

    char temp[TMP_BUF_SIZE];

    sprintf(temp,"%d",value);

    icalmemory_append_string(buf, buf_ptr, buf_size, temp);
    icalmemory_append_string(buf, buf_ptr, buf_size, sep);
    
}

char* icalvalue_duration_as_ical_string(icalvalue* value) {

    struct icaldurationtype data;
    char *buf, *output_line;
    size_t buf_size = 256;
    char* buf_ptr = 0;

    icalerror_check_arg_rz( (value!=0),"value");
    data = icalvalue_get_duration(value);

    buf = (char*)icalmemory_new_buffer(buf_size);
    buf_ptr = buf;
    
    icalmemory_append_string(&buf, &buf_ptr, &buf_size, "P");
    
    
    if (data.weeks != 0 ) {
	append_duration_segment(&buf, &buf_ptr, &buf_size, "W", data.weeks);
    }

    if (data.days != 0 ) {
	append_duration_segment(&buf, &buf_ptr, &buf_size, "D", data.days);
    }

    if (data.hours != 0 || data.minutes != 0 || data.seconds != 0) {

	icalmemory_append_string(&buf, &buf_ptr, &buf_size, "T");

	if (data.hours != 0 ) {
	    append_duration_segment(&buf, &buf_ptr, &buf_size, "H", data.hours);
	}
	if (data.minutes != 0 ) {
	    append_duration_segment(&buf, &buf_ptr, &buf_size, "M", data.minutes);
	}
	if (data.seconds != 0 ) {
	    append_duration_segment(&buf, &buf_ptr, &buf_size, "S", data.seconds);
	}

    }
 
    output_line = icalmemory_tmp_copy(buf);
    icalmemory_free_buffer(buf);

    return output_line;

    
}

void print_time_to_string(char* str,  struct icaltimetype *data)
{
    char temp[20];

    if (data->is_utc == 1){
	sprintf(temp,"%02d%02d%02dZ",data->hour,data->minute,data->second);
    } else {
	sprintf(temp,"%02d%02d%02d",data->hour,data->minute,data->second);
    }   

    strcat(str,temp);
}

 
char* icalvalue_time_as_ical_string(icalvalue* value) {

    struct icaltimetype data;
    char* str;
    icalerror_check_arg_rz( (value!=0),"value");
    data = icalvalue_get_time(value);
    
    str = (char*)icalmemory_tmp_buffer(8);

    str[0] = 0;
    print_time_to_string(str,&data);

    return str;
}

void print_date_to_string(char* str,  struct icaltimetype *data)
{
    char temp[20];

    sprintf(temp,"%04d%02d%02d",data->year,data->month,data->day);

    strcat(str,temp);
}

char* icalvalue_date_as_ical_string(icalvalue* value) {

    struct icaltimetype data;
    char* str;
    icalerror_check_arg_rz( (value!=0),"value");
    data = icalvalue_get_date(value);

    str = (char*)icalmemory_tmp_buffer(9);
 
    str[0] = 0;
    print_date_to_string(str,&data);
   
    return str;
}

void print_datetime_to_string(char* str,  struct icaltimetype *data)
{
    print_date_to_string(str,data);
    strcat(str,"T");
    print_time_to_string(str,data);

}

char* icalvalue_datetime_as_ical_string(icalvalue* value) {
    
    struct icaltimetype data;
    char* str;
    icalerror_check_arg_rz( (value!=0),"value");
    data = icalvalue_get_date(value);

    str = (char*)icalmemory_tmp_buffer(20);
 
    str[0] = 0;

    print_datetime_to_string(str,&data);
   
    return str;

}


char* icalvalue_datetimedate_as_ical_string(icalvalue* value) {

    struct icaltimetype data;
    icalerror_check_arg_rz( (value!=0),"value");
    data = icalvalue_get_datetime(value);

    if (data.is_date == 1){
	return icalvalue_date_as_ical_string(value);
    } else {
	return icalvalue_datetime_as_ical_string(value);
    }
}


char* icalvalue_float_as_ical_string(icalvalue* value) {

    float data;
    char* str;
    icalerror_check_arg_rz( (value!=0),"value");
    data = icalvalue_get_float(value);

    str = (char*)icalmemory_tmp_buffer(15);

    sprintf(str,"%f",data);

    return str;
}

char* icalvalue_geo_as_ical_string(icalvalue* value) {

    struct icalgeotype data;
    char* str;
    icalerror_check_arg_rz( (value!=0),"value");

    data = icalvalue_get_geo(value);

    str = (char*)icalmemory_tmp_buffer(25);

    sprintf(str,"%f;%f",data.lat,data.lon);

    return str;
}

char* icalvalue_datetimeperiod_as_ical_string(icalvalue* value) {

    struct icalperiodtype data;
    char* str;
    icalerror_check_arg_rz( (value!=0),"value");
    data = icalvalue_get_datetimeperiod(value);

    str = (char*)icalmemory_tmp_buffer(60);

    if( data.end.second == -1){
	/* This is a DATE-TIME value, since there is no end value */
	icalvalue *v= icalvalue_new_datetime(data.start);

	strcpy(str,icalvalue_datetime_as_ical_string(v));

	free(v);

    } else {
	icalvalue *v1 = icalvalue_new_datetime(data.start);
	icalvalue *v2 = icalvalue_new_datetime(data.end);

	sprintf(str,"%s/%s",
		icalvalue_datetime_as_ical_string(v1),
		icalvalue_datetime_as_ical_string(v2)
	    );

	free(v1);
	free(v2);

    }

    return str;
}

char* icalvalue_period_as_ical_string(icalvalue* value) {

    struct icalperiodtype data;
    char* str;
    icalvalue *s,*e;
    
    icalerror_check_arg_rz( (value!=0),"value");
    data = icalvalue_get_period(value);

    str = (char*)icalmemory_tmp_buffer(60);

    s = icalvalue_new_datetime(data.start);

    if (data.end.second != -1){
	/* use the end date */
	e = icalvalue_new_datetime(data.end);
	
	sprintf(str,"%s/%s",
		icalvalue_datetime_as_ical_string(s),
		icalvalue_datetime_as_ical_string(e)
	    );
	

    } else {
	/* use the duration */
	e = icalvalue_new_duration(data.duration);
	
	sprintf(str,"%s/%s",
		icalvalue_datetime_as_ical_string(s),
		icalvalue_duration_as_ical_string(e)
	    );
	
    }

    icalvalue_free(e);
    icalvalue_free(s);
    return str;
}

char* icalvalue_trigger_as_ical_string(icalvalue* value) {

    union icaltriggertype data;
    char* str;
    icalerror_check_arg_rz( (value!=0),"value");
    data = icalvalue_get_trigger(value);

    str = (char*)icalmemory_tmp_buffer(60);
    sprintf(str,"icalvalue_trigger_as_ical_string is not implemented yet");

    return str;
}

char*
icalvalue_as_ical_string (icalvalue* value)
{
    struct icalvalue_impl* v = (struct icalvalue_impl*)value;

    v=v;

    if(value == 0){
	return 0;
    }

    switch (v->kind){

	case ICAL_ATTACH_VALUE:
	    return icalvalue_attach_as_ical_string(value);

	case ICAL_BINARY_VALUE:
	    return icalvalue_binary_as_ical_string(value);

	case ICAL_BOOLEAN_VALUE:
	case ICAL_INTEGER_VALUE:
	    return icalvalue_int_as_ical_string(value);                  

	case ICAL_UTCOFFSET_VALUE:
	    return icalvalue_utcoffset_as_ical_string(value);                  

	case ICAL_TEXT_VALUE:
	    return icalvalue_text_as_ical_string(value);

	case ICAL_STRING_VALUE:
	case ICAL_URI_VALUE:
	case ICAL_CALADDRESS_VALUE:
	    return icalvalue_string_as_ical_string(value);

	case ICAL_DATE_VALUE:
	    return icalvalue_date_as_ical_string(value);
	case ICAL_DATETIME_VALUE:
	    return icalvalue_datetime_as_ical_string(value);
	case ICAL_DATETIMEDATE_VALUE:
	    return icalvalue_datetimedate_as_ical_string(value);
	case ICAL_DURATION_VALUE:
	    return icalvalue_duration_as_ical_string(value);
	case ICAL_TIME_VALUE:
	    return icalvalue_time_as_ical_string(value);

	case ICAL_PERIOD_VALUE:
	    return icalvalue_period_as_ical_string(value);
	case ICAL_DATETIMEPERIOD_VALUE:
	    return icalvalue_datetimeperiod_as_ical_string(value);

	case ICAL_FLOAT_VALUE:
	    return icalvalue_float_as_ical_string(value);

	case ICAL_GEO_VALUE:
	    return icalvalue_geo_as_ical_string(value);

	case ICAL_RECUR_VALUE:
	    return icalvalue_recur_as_ical_string(value);

	case ICAL_TRIGGER_VALUE:
	    return icalvalue_trigger_as_ical_string(value);

	case ICAL_METHOD_VALUE:
	    return icalenum_method_to_string(v->data.v_method);

	case ICAL_NO_VALUE:
	default:
	{
	    return 0;
	}
    }
}


icalvalue_kind
icalvalue_isa (icalvalue* value)
{
    struct icalvalue_impl* v = (struct icalvalue_impl*)value;

    if(value == 0){
	return ICAL_NO_VALUE;
    }

    return v->kind;
}


int
icalvalue_isa_value (void* value)
{
    struct icalvalue_impl *impl = (struct icalvalue_impl *)value;

    icalerror_check_arg_rz( (value!=0), "value");

    if (strcmp(impl->id,"val") == 0) {
	return 1;
    } else {
	return 0;
    }
}


icalparameter_xliccomparetype
icalvalue_compare(icalvalue* a, icalvalue *b)
{
    struct icalvalue_impl *impla = (struct icalvalue_impl *)a;
    struct icalvalue_impl *implb = (struct icalvalue_impl *)b;

    icalerror_check_arg_rz( (a!=0), "a");
    icalerror_check_arg_rz( (b!=0), "b");

    /* Not the same type; they can only be unequal */
    if (icalvalue_isa(a) != icalvalue_isa(b)){
	return ICAL_XLICCOMPARETYPE_NOTEQUAL;
    }

    switch (icalvalue_isa(a)){

	case ICAL_ATTACH_VALUE:
	case ICAL_BINARY_VALUE:

	case ICAL_BOOLEAN_VALUE:
	{
	    if (icalvalue_get_boolean(a) == icalvalue_get_boolean(b)){
		return ICAL_XLICCOMPARETYPE_EQUAL;
	    } else {
		return ICAL_XLICCOMPARETYPE_NOTEQUAL;
	    }
	}

	case ICAL_FLOAT_VALUE:
	{
	    if (impla->data.v_float > implb->data.v_float){
		return ICAL_XLICCOMPARETYPE_GREATER;
	    } else if (impla->data.v_float < implb->data.v_float){
		return ICAL_XLICCOMPARETYPE_LESS;
	    } else {
		return ICAL_XLICCOMPARETYPE_EQUAL;
	    }
	}

	case ICAL_INTEGER_VALUE:
	case ICAL_UTCOFFSET_VALUE:
	{
	    if (impla->data.v_int > implb->data.v_int){
		return ICAL_XLICCOMPARETYPE_GREATER;
	    } else if (impla->data.v_int < implb->data.v_int){
		return ICAL_XLICCOMPARETYPE_LESS;
	    } else {
		return ICAL_XLICCOMPARETYPE_EQUAL;
	    }
	}

	case ICAL_TEXT_VALUE:
	case ICAL_URI_VALUE:
	case ICAL_CALADDRESS_VALUE:
	case ICAL_TRIGGER_VALUE:
	case ICAL_DATE_VALUE:
	case ICAL_DATETIME_VALUE:
	case ICAL_DATETIMEDATE_VALUE:
	case ICAL_DURATION_VALUE: /* HACK. Not correct for DURATION */
	case ICAL_TIME_VALUE:
	case ICAL_DATETIMEPERIOD_VALUE:
	{
	    int r;

	    r =  strcmp(icalvalue_as_ical_string(a),
			  icalvalue_as_ical_string(b));

	    if (r > 0) { 	
		return ICAL_XLICCOMPARETYPE_GREATER;
	    } else if (r < 0){
		return ICAL_XLICCOMPARETYPE_LESS;
	    } else {
		return 0;
	    }

		
	}

	case ICAL_METHOD_VALUE:
	{
	    if (icalvalue_get_method(a) == icalvalue_get_method(b)){
		return ICAL_XLICCOMPARETYPE_EQUAL;
	    } else {
		return ICAL_XLICCOMPARETYPE_NOTEQUAL;
	    }

	}
	case ICAL_PERIOD_VALUE:
	case ICAL_GEO_VALUE:
	case ICAL_RECUR_VALUE:
	case ICAL_NO_VALUE:
	default:
	{
	    icalerror_warn("Comparison not implemented for value type");
	    return ICAL_XLICCOMPARETYPE_REGEX+1; /* HACK */
	}
    }   

}

void icalvalue_set_parent(icalvalue* value,
			     icalproperty* property)
{
    struct icalvalue_impl* v = (struct icalvalue_impl*)value;

    v->parent = property;

}

icalproperty* icalvalue_get_parent(icalvalue* value)
{
    struct icalvalue_impl* v = (struct icalvalue_impl*)value;


    return v->parent;
}



/* Recur is a special case, so it is not auto generated. Well,
   actually, it is auto-generated, but you will have to manually
   remove the auto-generated version after each generation.  */
icalvalue*
icalvalue_new_recur (struct icalrecurrencetype v)
{
   struct icalvalue_impl* impl = icalvalue_new_impl(ICAL_RECUR_VALUE);
    
   icalvalue_set_recur((icalvalue*)impl,v);

   return (icalvalue*)impl;
}

void
icalvalue_set_recur(icalvalue* value, struct icalrecurrencetype v)
{
    struct icalvalue_impl* impl; 
    
    icalerror_check_arg_rv( (value!=0),"value");
    icalerror_check_value_type(value, ICAL_RECUR_VALUE);

    impl = (struct icalvalue_impl*)value;

    if (impl->data.v_recur != 0){
	free(impl->data.v_recur);
	impl->data.v_recur = 0;
    }

    impl->data.v_recur = malloc(sizeof(struct icalrecurrencetype));

    if (impl->data.v_recur == 0){
	icalerror_set_errno(ICAL_ALLOCATION_ERROR);
	return;
    } else {
	memcpy(impl->data.v_recur, &v, sizeof(struct icalrecurrencetype));
    }
	       
}

struct icalrecurrencetype
icalvalue_get_recur(icalvalue* value)
{
    icalerror_check_arg( (value!=0),"value");
    icalerror_check_value_type(value, ICAL_RECUR_VALUE);
  
    return *(((struct icalvalue_impl*)value)->data.v_recur);
}




/* The remaining interfaces are 'new', 'set' and 'get' for each of the value
   types */


/* Everything below this line is machine generated. Do not edit. */

icalvalue*
icalvalue_new_attach (struct icalattachtype v)
{
   struct icalvalue_impl* impl = icalvalue_new_impl(ICAL_ATTACH_VALUE);
 
   
   icalvalue_set_attach((icalvalue*)impl,v);

   return (icalvalue*)impl;
}

void
icalvalue_set_attach(icalvalue* value, struct icalattachtype v)
{
    struct icalvalue_impl* impl; 
    
    icalerror_check_arg_rv( (value!=0),"value");
    
    icalerror_check_value_type(value, ICAL_ATTACH_VALUE);

    impl = (struct icalvalue_impl*)value;

    impl->data.v_attach = v;
}

struct icalattachtype
icalvalue_get_attach(icalvalue* value)
{
    icalerror_check_arg( (value!=0),"value");
    icalerror_check_value_type(value, ICAL_ATTACH_VALUE);
  
    return ((struct icalvalue_impl*)value)->data.v_attach;
}


icalvalue*
icalvalue_new_binary (char* v)
{
   struct icalvalue_impl* impl = icalvalue_new_impl(ICAL_BINARY_VALUE);
 
   icalerror_check_arg_rz( (v!=0),"v");

   icalvalue_set_binary((icalvalue*)impl,v);

   return (icalvalue*)impl;
}

void
icalvalue_set_binary(icalvalue* value, char* v)
{
    struct icalvalue_impl* impl; 
    
    icalerror_check_arg_rv( (value!=0),"value");
    icalerror_check_arg_rv( (v!=0),"v");

    icalerror_check_value_type(value, ICAL_BINARY_VALUE);

    impl = (struct icalvalue_impl*)value;
    if(impl->data.v_string!=0) {free(impl->data.v_string);}

    impl->data.v_string = strdup(v);

    if (impl->data.v_string == 0){
      errno = ENOMEM;
    }

}

char*
icalvalue_get_binary(icalvalue* value)
{
    icalerror_check_arg( (value!=0),"value");
    icalerror_check_value_type(value, ICAL_BINARY_VALUE);
  
    return ((struct icalvalue_impl*)value)->data.v_string;
}


icalvalue*
icalvalue_new_boolean (int v)
{
   struct icalvalue_impl* impl = icalvalue_new_impl(ICAL_BOOLEAN_VALUE);
 
   
   icalvalue_set_boolean((icalvalue*)impl,v);

   return (icalvalue*)impl;
}

void
icalvalue_set_boolean(icalvalue* value, int v)
{
    struct icalvalue_impl* impl; 
    
    icalerror_check_arg_rv( (value!=0),"value");
    
    icalerror_check_value_type(value, ICAL_BOOLEAN_VALUE);

    impl = (struct icalvalue_impl*)value;

    impl->data.v_int = v;
}

int
icalvalue_get_boolean(icalvalue* value)
{
    icalerror_check_arg( (value!=0),"value");
    icalerror_check_value_type(value, ICAL_BOOLEAN_VALUE);
  
    return ((struct icalvalue_impl*)value)->data.v_int;
}


icalvalue*
icalvalue_new_caladdress (char* v)
{
   struct icalvalue_impl* impl = icalvalue_new_impl(ICAL_CALADDRESS_VALUE);
 
   icalerror_check_arg_rz( (v!=0),"v");

   icalvalue_set_caladdress((icalvalue*)impl,v);

   return (icalvalue*)impl;
}

void
icalvalue_set_caladdress(icalvalue* value, char* v)
{
    struct icalvalue_impl* impl; 
    
    icalerror_check_arg_rv( (value!=0),"value");
    icalerror_check_arg_rv( (v!=0),"v");

    icalerror_check_value_type(value, ICAL_CALADDRESS_VALUE);

    impl = (struct icalvalue_impl*)value;
    if(impl->data.v_string!=0) {free(impl->data.v_string);}

    impl->data.v_string = strdup(v);

    if (impl->data.v_string == 0){
      errno = ENOMEM;
    }

}

char*
icalvalue_get_caladdress(icalvalue* value)
{
    icalerror_check_arg( (value!=0),"value");
    icalerror_check_value_type(value, ICAL_CALADDRESS_VALUE);
  
    return ((struct icalvalue_impl*)value)->data.v_string;
}


icalvalue*
icalvalue_new_date (struct icaltimetype v)
{
   struct icalvalue_impl* impl = icalvalue_new_impl(ICAL_DATE_VALUE);
 
   
   icalvalue_set_date((icalvalue*)impl,v);

   return (icalvalue*)impl;
}

void
icalvalue_set_date(icalvalue* value, struct icaltimetype v)
{
    struct icalvalue_impl* impl; 
    
    icalerror_check_arg_rv( (value!=0),"value");
    
    icalerror_check_value_type(value, ICAL_DATE_VALUE);

    impl = (struct icalvalue_impl*)value;

    impl->data.v_time = v;
}

struct icaltimetype
icalvalue_get_date(icalvalue* value)
{
    icalerror_check_arg( (value!=0),"value");
    icalerror_check_value_type(value, ICAL_DATE_VALUE);
  
    return ((struct icalvalue_impl*)value)->data.v_time;
}


icalvalue*
icalvalue_new_datetime (struct icaltimetype v)
{
   struct icalvalue_impl* impl = icalvalue_new_impl(ICAL_DATETIME_VALUE);
 
   
   icalvalue_set_datetime((icalvalue*)impl,v);

   return (icalvalue*)impl;
}

void
icalvalue_set_datetime(icalvalue* value, struct icaltimetype v)
{
    struct icalvalue_impl* impl; 
    
    icalerror_check_arg_rv( (value!=0),"value");
    
    icalerror_check_value_type(value, ICAL_DATETIME_VALUE);

    impl = (struct icalvalue_impl*)value;

    impl->data.v_time = v;
}

struct icaltimetype
icalvalue_get_datetime(icalvalue* value)
{
    icalerror_check_arg( (value!=0),"value");
    icalerror_check_value_type(value, ICAL_DATETIME_VALUE);
  
    return ((struct icalvalue_impl*)value)->data.v_time;
}


icalvalue*
icalvalue_new_datetimedate (struct icaltimetype v)
{
   struct icalvalue_impl* impl = icalvalue_new_impl(ICAL_DATETIMEDATE_VALUE);
 
   
   icalvalue_set_datetimedate((icalvalue*)impl,v);

   return (icalvalue*)impl;
}

void
icalvalue_set_datetimedate(icalvalue* value, struct icaltimetype v)
{
    struct icalvalue_impl* impl; 
    
    icalerror_check_arg_rv( (value!=0),"value");
    
    icalerror_check_value_type(value, ICAL_DATETIMEDATE_VALUE);

    impl = (struct icalvalue_impl*)value;

    impl->data.v_time = v;
}

struct icaltimetype
icalvalue_get_datetimedate(icalvalue* value)
{
    icalerror_check_arg( (value!=0),"value");
    icalerror_check_value_type(value, ICAL_DATETIMEDATE_VALUE);
  
    return ((struct icalvalue_impl*)value)->data.v_time;
}


icalvalue*
icalvalue_new_datetimeperiod (struct icalperiodtype v)
{
   struct icalvalue_impl* impl = icalvalue_new_impl(ICAL_DATETIMEPERIOD_VALUE);
 
   
   icalvalue_set_datetimeperiod((icalvalue*)impl,v);

   return (icalvalue*)impl;
}

void
icalvalue_set_datetimeperiod(icalvalue* value, struct icalperiodtype v)
{
    struct icalvalue_impl* impl; 
    
    icalerror_check_arg_rv( (value!=0),"value");
    
    icalerror_check_value_type(value, ICAL_DATETIMEPERIOD_VALUE);

    impl = (struct icalvalue_impl*)value;

    impl->data.v_period = v;
}

struct icalperiodtype
icalvalue_get_datetimeperiod(icalvalue* value)
{
    icalerror_check_arg( (value!=0),"value");
    icalerror_check_value_type(value, ICAL_DATETIMEPERIOD_VALUE);
  
    return ((struct icalvalue_impl*)value)->data.v_period;
}


icalvalue*
icalvalue_new_duration (struct icaldurationtype v)
{
   struct icalvalue_impl* impl = icalvalue_new_impl(ICAL_DURATION_VALUE);
 
   
   icalvalue_set_duration((icalvalue*)impl,v);

   return (icalvalue*)impl;
}

void
icalvalue_set_duration(icalvalue* value, struct icaldurationtype v)
{
    struct icalvalue_impl* impl; 
    
    icalerror_check_arg_rv( (value!=0),"value");
    
    icalerror_check_value_type(value, ICAL_DURATION_VALUE);

    impl = (struct icalvalue_impl*)value;

    impl->data.v_duration = v;
}

struct icaldurationtype
icalvalue_get_duration(icalvalue* value)
{
    icalerror_check_arg( (value!=0),"value");
    icalerror_check_value_type(value, ICAL_DURATION_VALUE);
  
    return ((struct icalvalue_impl*)value)->data.v_duration;
}


icalvalue*
icalvalue_new_float (float v)
{
   struct icalvalue_impl* impl = icalvalue_new_impl(ICAL_FLOAT_VALUE);
 
   
   icalvalue_set_float((icalvalue*)impl,v);

   return (icalvalue*)impl;
}

void
icalvalue_set_float(icalvalue* value, float v)
{
    struct icalvalue_impl* impl; 
    
    icalerror_check_arg_rv( (value!=0),"value");
    
    icalerror_check_value_type(value, ICAL_FLOAT_VALUE);

    impl = (struct icalvalue_impl*)value;

    impl->data.v_float = v;
}

float
icalvalue_get_float(icalvalue* value)
{
    icalerror_check_arg( (value!=0),"value");
    icalerror_check_value_type(value, ICAL_FLOAT_VALUE);
  
    return ((struct icalvalue_impl*)value)->data.v_float;
}


icalvalue*
icalvalue_new_geo (struct icalgeotype v)
{
   struct icalvalue_impl* impl = icalvalue_new_impl(ICAL_GEO_VALUE);
 
   
   icalvalue_set_geo((icalvalue*)impl,v);

   return (icalvalue*)impl;
}

void
icalvalue_set_geo(icalvalue* value, struct icalgeotype v)
{
    struct icalvalue_impl* impl; 
    
    icalerror_check_arg_rv( (value!=0),"value");
    
    icalerror_check_value_type(value, ICAL_GEO_VALUE);

    impl = (struct icalvalue_impl*)value;

    impl->data.v_geo = v;
}

struct icalgeotype
icalvalue_get_geo(icalvalue* value)
{
    icalerror_check_arg( (value!=0),"value");
    icalerror_check_value_type(value, ICAL_GEO_VALUE);
  
    return ((struct icalvalue_impl*)value)->data.v_geo;
}


icalvalue*
icalvalue_new_integer (int v)
{
   struct icalvalue_impl* impl = icalvalue_new_impl(ICAL_INTEGER_VALUE);
 
   
   icalvalue_set_integer((icalvalue*)impl,v);

   return (icalvalue*)impl;
}

void
icalvalue_set_integer(icalvalue* value, int v)
{
    struct icalvalue_impl* impl; 
    
    icalerror_check_arg_rv( (value!=0),"value");
    
    icalerror_check_value_type(value, ICAL_INTEGER_VALUE);

    impl = (struct icalvalue_impl*)value;

    impl->data.v_int = v;
}

int
icalvalue_get_integer(icalvalue* value)
{
    icalerror_check_arg( (value!=0),"value");
    icalerror_check_value_type(value, ICAL_INTEGER_VALUE);
  
    return ((struct icalvalue_impl*)value)->data.v_int;
}


icalvalue*
icalvalue_new_method (icalproperty_method v)
{
   struct icalvalue_impl* impl = icalvalue_new_impl(ICAL_METHOD_VALUE);
 
   
   icalvalue_set_method((icalvalue*)impl,v);

   return (icalvalue*)impl;
}

void
icalvalue_set_method(icalvalue* value, icalproperty_method v)
{
    struct icalvalue_impl* impl; 
    
    icalerror_check_arg_rv( (value!=0),"value");
    
    icalerror_check_value_type(value, ICAL_METHOD_VALUE);

    impl = (struct icalvalue_impl*)value;

    impl->data.v_method = v;
}

icalproperty_method
icalvalue_get_method(icalvalue* value)
{
    icalerror_check_arg( (value!=0),"value");
    icalerror_check_value_type(value, ICAL_METHOD_VALUE);
  
    return ((struct icalvalue_impl*)value)->data.v_method;
}


icalvalue*
icalvalue_new_period (struct icalperiodtype v)
{
   struct icalvalue_impl* impl = icalvalue_new_impl(ICAL_PERIOD_VALUE);
 
   
   icalvalue_set_period((icalvalue*)impl,v);

   return (icalvalue*)impl;
}

void
icalvalue_set_period(icalvalue* value, struct icalperiodtype v)
{
    struct icalvalue_impl* impl; 
    
    icalerror_check_arg_rv( (value!=0),"value");
    
    icalerror_check_value_type(value, ICAL_PERIOD_VALUE);

    impl = (struct icalvalue_impl*)value;

    impl->data.v_period = v;
}

struct icalperiodtype
icalvalue_get_period(icalvalue* value)
{
    icalerror_check_arg( (value!=0),"value");
    icalerror_check_value_type(value, ICAL_PERIOD_VALUE);
  
    return ((struct icalvalue_impl*)value)->data.v_period;
}

icalvalue*
icalvalue_new_string (char* v)
{
   struct icalvalue_impl* impl = icalvalue_new_impl(ICAL_STRING_VALUE);
 
   icalerror_check_arg_rz( (v!=0),"v");

   icalvalue_set_string((icalvalue*)impl,v);

   return (icalvalue*)impl;
}

void
icalvalue_set_string(icalvalue* value, char* v)
{
    struct icalvalue_impl* impl; 
    
    icalerror_check_arg_rv( (value!=0),"value");
    icalerror_check_arg_rv( (v!=0),"v");

    icalerror_check_value_type(value, ICAL_STRING_VALUE);

    impl = (struct icalvalue_impl*)value;
    if(impl->data.v_string!=0) {free(impl->data.v_string);}

    impl->data.v_string = strdup(v);

    if (impl->data.v_string == 0){
      errno = ENOMEM;
    }

}

char*
icalvalue_get_string(icalvalue* value)
{
    icalerror_check_arg( (value!=0),"value");
    icalerror_check_value_type(value, ICAL_STRING_VALUE);
  
    return ((struct icalvalue_impl*)value)->data.v_string;
}


icalvalue*
icalvalue_new_text (char* v)
{
   struct icalvalue_impl* impl = icalvalue_new_impl(ICAL_TEXT_VALUE);
 
   icalerror_check_arg_rz( (v!=0),"v");

   icalvalue_set_text((icalvalue*)impl,v);

   return (icalvalue*)impl;
}

void
icalvalue_set_text(icalvalue* value, char* v)
{
    struct icalvalue_impl* impl; 
    
    icalerror_check_arg_rv( (value!=0),"value");
    icalerror_check_arg_rv( (v!=0),"v");

    icalerror_check_value_type(value, ICAL_TEXT_VALUE);

    impl = (struct icalvalue_impl*)value;
    if(impl->data.v_string!=0) {free(impl->data.v_string);}

    impl->data.v_string = strdup(v);

    if (impl->data.v_string == 0){
      errno = ENOMEM;
    }

}

char*
icalvalue_get_text(icalvalue* value)
{
    icalerror_check_arg( (value!=0),"value");
    icalerror_check_value_type(value, ICAL_TEXT_VALUE);
  
    return ((struct icalvalue_impl*)value)->data.v_string;
}


icalvalue*
icalvalue_new_time (struct icaltimetype v)
{
   struct icalvalue_impl* impl = icalvalue_new_impl(ICAL_TIME_VALUE);
 
   
   icalvalue_set_time((icalvalue*)impl,v);

   return (icalvalue*)impl;
}

void
icalvalue_set_time(icalvalue* value, struct icaltimetype v)
{
    struct icalvalue_impl* impl; 
    
    icalerror_check_arg_rv( (value!=0),"value");
    
    icalerror_check_value_type(value, ICAL_TIME_VALUE);

    impl = (struct icalvalue_impl*)value;

    impl->data.v_time = v;
}

struct icaltimetype
icalvalue_get_time(icalvalue* value)
{
    icalerror_check_arg( (value!=0),"value");
    icalerror_check_value_type(value, ICAL_TIME_VALUE);
  
    return ((struct icalvalue_impl*)value)->data.v_time;
}


icalvalue*
icalvalue_new_trigger (union icaltriggertype v)
{
   struct icalvalue_impl* impl = icalvalue_new_impl(ICAL_TRIGGER_VALUE);
 
   
   icalvalue_set_trigger((icalvalue*)impl,v);

   return (icalvalue*)impl;
}

void
icalvalue_set_trigger(icalvalue* value, union icaltriggertype v)
{
    struct icalvalue_impl* impl; 
    
    icalerror_check_arg_rv( (value!=0),"value");
    
    icalerror_check_value_type(value, ICAL_TRIGGER_VALUE);

    impl = (struct icalvalue_impl*)value;

    impl->data.v_trigger = v;
}

union icaltriggertype
icalvalue_get_trigger(icalvalue* value)
{
    icalerror_check_arg( (value!=0),"value");
    icalerror_check_value_type(value, ICAL_TRIGGER_VALUE);
  
    return ((struct icalvalue_impl*)value)->data.v_trigger;
}


icalvalue*
icalvalue_new_uri (char* v)
{
   struct icalvalue_impl* impl = icalvalue_new_impl(ICAL_URI_VALUE);
 
   icalerror_check_arg_rz( (v!=0),"v");

   icalvalue_set_uri((icalvalue*)impl,v);

   return (icalvalue*)impl;
}

void
icalvalue_set_uri(icalvalue* value, char* v)
{
    struct icalvalue_impl* impl; 
    
    icalerror_check_arg_rv( (value!=0),"value");
    icalerror_check_arg_rv( (v!=0),"v");

    icalerror_check_value_type(value, ICAL_URI_VALUE);

    impl = (struct icalvalue_impl*)value;
    if(impl->data.v_string!=0) {free(impl->data.v_string);}

    impl->data.v_string = strdup(v);

    if (impl->data.v_string == 0){
      errno = ENOMEM;
    }

}

char*
icalvalue_get_uri(icalvalue* value)
{
    icalerror_check_arg( (value!=0),"value");
    icalerror_check_value_type(value, ICAL_URI_VALUE);
  
    return ((struct icalvalue_impl*)value)->data.v_string;
}


icalvalue*
icalvalue_new_utcoffset (int v)
{
   struct icalvalue_impl* impl = icalvalue_new_impl(ICAL_UTCOFFSET_VALUE);
 
   
   icalvalue_set_utcoffset((icalvalue*)impl,v);

   return (icalvalue*)impl;
}

void
icalvalue_set_utcoffset(icalvalue* value, int v)
{
    struct icalvalue_impl* impl; 
    
    icalerror_check_arg_rv( (value!=0),"value");
    
    icalerror_check_value_type(value, ICAL_UTCOFFSET_VALUE);

    impl = (struct icalvalue_impl*)value;

    impl->data.v_int = v;
}

int
icalvalue_get_utcoffset(icalvalue* value)
{
    icalerror_check_arg( (value!=0),"value");
    icalerror_check_value_type(value, ICAL_UTCOFFSET_VALUE);
  
    return ((struct icalvalue_impl*)value)->data.v_int;
}


icalvalue*
icalvalue_new_query (char* v)
{
   struct icalvalue_impl* impl = icalvalue_new_impl(ICAL_QUERY_VALUE);
 
   icalerror_check_arg_rz( (v!=0),"v");

   icalvalue_set_query((icalvalue*)impl,v);

   return (icalvalue*)impl;
}

void
icalvalue_set_query(icalvalue* value, char* v)
{
    struct icalvalue_impl* impl; 
    
    icalerror_check_arg_rv( (value!=0),"value");
    icalerror_check_arg_rv( (v!=0),"v");

    icalerror_check_value_type(value, ICAL_QUERY_VALUE);

    impl = (struct icalvalue_impl*)value;
    if(impl->data.v_string!=0) {free(impl->data.v_string);}

    impl->data.v_string = strdup(v);

    if (impl->data.v_string == 0){
      errno = ENOMEM;
    }

}

char*
icalvalue_get_query(icalvalue* value)
{
    icalerror_check_arg( (value!=0),"value");
    icalerror_check_value_type(value, ICAL_QUERY_VALUE);
  
    return ((struct icalvalue_impl*)value)->data.v_string;
}

