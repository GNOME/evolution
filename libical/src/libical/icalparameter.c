/* -*- Mode: C -*-
  ======================================================================
  FILE: icalderivedparameters.{c,h}
  CREATOR: eric 09 May 1999
  
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
  The original code is icalderivedparameters.{c,h}

  Contributions from:
     Graham Davison (g.m.davison@computer.org)

 ======================================================================*/

#include "ical.h"
#include "icalerror.h"
#include <stdlib.h> /* for malloc() */
#include <errno.h>
#include <string.h> /* for memset() */
#include "icalmemory.h"

struct icalparameter_impl
{
	icalparameter_kind kind;
	char id[5];
	int size;
	char* string;
	char* x_name;
	icalproperty* parent;

	union data {
		int v_int;
		int v_rsvp;
		icalparameter_cutype v_cutype;
		icalparameter_encoding v_encoding;
		icalparameter_fbtype v_fbtype;
		icalparameter_partstat v_partstat;
		icalparameter_range v_range;
		icalparameter_related v_related;
		icalparameter_reltype v_reltype;
		icalparameter_role v_role;
		icalparameter_value v_value;
		icalparameter_xlicerrortype v_xlicerrortype;
		icalparameter_xliccomparetype v_xliccomparetype;
	} data;
};

struct icalparameter_impl* icalparameter_new_impl(icalparameter_kind kind)
{
    struct icalparameter_impl* v;

    if ( ( v = (struct icalparameter_impl*)
	   malloc(sizeof(struct icalparameter_impl))) == 0) {
	icalerror_set_errno(ICAL_NEWFAILED_ERROR);
	errno = ENOMEM;
	return 0;
    }
    
    strcpy(v->id,"para");

    v->kind = kind;
    v->size = 0;
    v->string = 0;
    v->x_name = 0;
    v->parent = 0;
    memset(&(v->data),0,sizeof(v->data));

    return v;
}

icalparameter*
icalparameter_new (icalparameter_kind kind)
{
    struct icalparameter_impl* v = icalparameter_new_impl(kind);

    return (icalparameter*) v;

}

icalparameter* 
icalparameter_new_clone(icalparameter* param)
{
    struct icalparameter_impl *old;
    struct icalparameter_impl *new;

    old = (struct icalparameter_impl *)param;
    new = icalparameter_new_impl(old->kind);

    icalerror_check_arg_rz((param!=0),"param");

    if (new == 0){
	return 0;
    }

    memcpy(new,old,sizeof(struct icalparameter_impl));

    if (old->string != 0){
	new->string = strdup(old->string);
	if (new->string == 0){
	    icalparameter_free(new);
	    return 0;
	}
    }

    if (old->x_name != 0){
	new->x_name = strdup(old->x_name);
	if (new->x_name == 0){
	    icalparameter_free(new);
	    return 0;
	}
    }

    return new;
}


icalparameter* icalparameter_new_from_string(icalparameter_kind kind, char* val)
{

    icalparameter* param=0;

    icalerror_check_arg_rz((val!=0),"val");

    switch (kind) {
	case ICAL_ALTREP_PARAMETER:
	{
	    param = icalparameter_new_altrep(val);

	    break;
	}
	case ICAL_CN_PARAMETER:
	{
	    param = icalparameter_new_cn(val);

	    break;
	}
	case ICAL_CUTYPE_PARAMETER:
	{
	    if(strcmp(val,"INDIVIDUAL") == 0){ 
		param = icalparameter_new_cutype(ICAL_CUTYPE_INDIVIDUAL);
	    }
	    else if(strcmp(val,"GROUP") == 0){ 
		param = icalparameter_new_cutype(ICAL_CUTYPE_GROUP);
	    }
	    else if(strcmp(val,"RESOURCE") == 0){ 
		param = icalparameter_new_cutype(ICAL_CUTYPE_RESOURCE);
	    }
	    else if(strcmp(val,"ROOM") == 0){ 
		param = icalparameter_new_cutype(ICAL_CUTYPE_ROOM);
	    }
	    else if(strcmp(val,"UNKNOWN") == 0){ 
		param = icalparameter_new_cutype(ICAL_CUTYPE_UNKNOWN);
	    }
	    else {
		param = icalparameter_new_cutype(ICAL_CUTYPE_XNAME);
		icalparameter_set_xvalue(param,val);
	    } 
	    break;
	}

	case ICAL_DELEGATEDFROM_PARAMETER:
	{
	    param = icalparameter_new_delegatedfrom(val);

	    break;
	}
	case ICAL_DELEGATEDTO_PARAMETER:
	{
	    param = icalparameter_new_delegatedto(val);

	    break;
	}
	case ICAL_DIR_PARAMETER:
	{
	    param = icalparameter_new_dir(val);

	    break;
	}
	case ICAL_ENCODING_PARAMETER:
	{
	    if(strcmp(val,"BIT8") == 0){ 
		param = icalparameter_new_encoding(ICAL_ENCODING_8BIT);
	    }
	    else if(strcmp(val,"BASE64") == 0){ 
		param = icalparameter_new_encoding(ICAL_ENCODING_BASE64);
	    }
	    else {
		param = icalparameter_new_encoding(ICAL_ENCODING_XNAME);
		icalparameter_set_xvalue(param,val);
	    } 
	    break;
	}
	case ICAL_FBTYPE_PARAMETER:
	{
	    if(strcmp(val,"FREE") == 0){ 
		param = icalparameter_new_fbtype(ICAL_FBTYPE_FREE);
	    }
	    else if(strcmp(val,"BUSY") == 0){ 
		param = icalparameter_new_fbtype(ICAL_FBTYPE_BUSY);
	    }
	    else if(strcmp(val,"BUSYUNAVAILABLE") == 0){ 
		param = icalparameter_new_fbtype(ICAL_FBTYPE_BUSYUNAVAILABLE);
	    }
	    else if(strcmp(val,"BUSYTENTATIVE") == 0){ 
		param = icalparameter_new_fbtype(ICAL_FBTYPE_BUSYTENTATIVE);
	    }
	    else {
		param = icalparameter_new_fbtype(ICAL_FBTYPE_XNAME);
		icalparameter_set_xvalue(param,val);
	    } 
	    break;
	}
	case ICAL_FMTTYPE_PARAMETER:
	{
	    param = icalparameter_new_fmttype(val);
	    break;
	}
	case ICAL_LANGUAGE_PARAMETER:
	{
	    param = icalparameter_new_language(val);

	    break;
	}
	case ICAL_MEMBER_PARAMETER:
	{
	    param = icalparameter_new_member(val);

	    break;
	}
	case ICAL_PARTSTAT_PARAMETER:
	{
	    if(strcmp(val,"NEEDSACTION") == 0){ 
		param = icalparameter_new_partstat(ICAL_PARTSTAT_NEEDSACTION);
	    }
	    else if(strcmp(val,"ACCEPTED") == 0){ 
		param = icalparameter_new_partstat(ICAL_PARTSTAT_ACCEPTED);
	    }
	    else if(strcmp(val,"DECLINED") == 0){ 
		param = icalparameter_new_partstat(ICAL_PARTSTAT_DECLINED);
	    }
	    else if(strcmp(val,"TENTATIVE") == 0){ 
		param = icalparameter_new_partstat(ICAL_PARTSTAT_TENTATIVE);
	    }
	    else if(strcmp(val,"DELEGATED") == 0){ 
		param = icalparameter_new_partstat(ICAL_PARTSTAT_DELEGATED);
	    }
	    else if(strcmp(val,"COMPLETED") == 0){ 
		param = icalparameter_new_partstat(ICAL_PARTSTAT_COMPLETED);
	    }
	    else if(strcmp(val,"INPROCESS") == 0){ 
		param = icalparameter_new_partstat(ICAL_PARTSTAT_INPROCESS);
	    }
	    else {
		param = icalparameter_new_partstat(ICAL_PARTSTAT_XNAME);
		icalparameter_set_xvalue(param,val);
	    } 
	    break;
	}
	case ICAL_RANGE_PARAMETER:
	{
	    if(strcmp(val,"THISANDFUTURE") == 0){ 
		param = icalparameter_new_range(ICAL_RANGE_THISANDFUTURE);
	    }
	    else if(strcmp(val,"THISANDPRIOR") == 0){ 
		param = icalparameter_new_range(ICAL_RANGE_THISANDPRIOR);
	    }

	    break;
	}
	case ICAL_RELATED_PARAMETER:
	{
	    if(strcmp(val,"START") == 0){ 
		param = icalparameter_new_related(ICAL_RELATED_START);
	    }
	    else if(strcmp(val,"END") == 0){ 
		param = icalparameter_new_related(ICAL_RELATED_END);
	    }

	    break;
	}
	case ICAL_RELTYPE_PARAMETER:
	{
	    if(strcmp(val,"PARENT") == 0){ 
		param = icalparameter_new_reltype(ICAL_RELTYPE_PARENT);
	    }
	    else if(strcmp(val,"CHILD") == 0){ 
		param = icalparameter_new_reltype(ICAL_RELTYPE_CHILD);
	    }
	    else if(strcmp(val,"SIBLING") == 0){ 
		param = icalparameter_new_reltype(ICAL_RELTYPE_SIBLING);
	    }
	    else {
		param = icalparameter_new_reltype(ICAL_RELTYPE_XNAME);
		icalparameter_set_xvalue(param,val);
	    } 
	    break;
	}
	case ICAL_ROLE_PARAMETER:
	{
	    if(strcmp(val,"CHAIR") == 0){ 
		param = icalparameter_new_role(ICAL_ROLE_CHAIR);
	    }
	    else if(strcmp(val,"REQ-PARTICIPANT") == 0){ 
		param = icalparameter_new_role(ICAL_ROLE_REQPARTICIPANT);
	    }
	    else if(strcmp(val,"OPT-PARTICIPANT") == 0){ 
		param = icalparameter_new_role(ICAL_ROLE_OPTPARTICIPANT);
	    }
	    else if(strcmp(val,"NON-PARTICIPANT") == 0){ 
		param = icalparameter_new_role(ICAL_ROLE_NONPARTICIPANT);
	    }
	    else {
		param = icalparameter_new_role(ICAL_ROLE_XNAME);
		icalparameter_set_xvalue(param,val);
	    } 
	    break;
	}
	case ICAL_RSVP_PARAMETER:
	{
	    if(strcmp(val,"TRUE") == 0){ 
		param = icalparameter_new_rsvp(1);
	    }
	    else if(strcmp(val,"FALSE") == 0){ 
		param = icalparameter_new_rsvp(0);
	    }

	    break;
	}
	case ICAL_SENTBY_PARAMETER:
	{
	    param = icalparameter_new_sentby(val);

	    break;
	}
	case ICAL_TZID_PARAMETER:
	{
	    param = icalparameter_new_tzid(val);

	    break;
	}
	case ICAL_VALUE_PARAMETER:
	{
	    if(strcmp(val,"BINARY") == 0){ 
		param = icalparameter_new_value(ICAL_VALUE_BINARY);
	    }
	    else if(strcmp(val,"BOOLEAN") == 0){ 
		param = icalparameter_new_value(ICAL_VALUE_BOOLEAN);
	    }
	    else if(strcmp(val,"CAL-ADDRESS") == 0){ 
		param = icalparameter_new_value(ICAL_VALUE_CALADDRESS);
	    }
	    else if(strcmp(val,"DATE") == 0){ 
		param = icalparameter_new_value(ICAL_VALUE_DATE);
	    }
	    else if(strcmp(val,"DATE-TIME") == 0){ 
		param = icalparameter_new_value(ICAL_VALUE_DATETIME);
	    }
	    else if(strcmp(val,"DURATION") == 0){ 
		param = icalparameter_new_value(ICAL_VALUE_DURATION);
	    }
	    else if(strcmp(val,"FLOAT") == 0){ 
		param = icalparameter_new_value(ICAL_VALUE_FLOAT);
	    }
	    else if(strcmp(val,"INTEGER") == 0){ 
		param = icalparameter_new_value(ICAL_VALUE_INTEGER);
	    }
	    else if(strcmp(val,"PERIOD") == 0){ 
		param = icalparameter_new_value(ICAL_VALUE_PERIOD);
	    }
	    else if(strcmp(val,"RECUR") == 0){ 
		param = icalparameter_new_value(ICAL_VALUE_RECUR);
	    }
	    else if(strcmp(val,"TEXT") == 0){ 
		param = icalparameter_new_value(ICAL_VALUE_TEXT);
	    }
	    else if(strcmp(val,"TIME") == 0){ 
		param = icalparameter_new_value(ICAL_VALUE_TIME);
	    }
	    else if(strcmp(val,"URI") == 0){ 
		param = icalparameter_new_value(ICAL_VALUE_URI);
	    }
	    else if(strcmp(val,"UTC-OFFSET") == 0){ 
		param = icalparameter_new_value(ICAL_VALUE_UTCOFFSET);
	    }
	    else {
		param = 0;
	    } 
	    break;
	}
	case ICAL_XLICERRORTYPE_PARAMETER:
	{

	    if(strcmp(val,"COMPONENT_PARSE_ERROR") == 0){ 
		param = icalparameter_new_xlicerrortype(ICAL_XLICERRORTYPE_COMPONENTPARSEERROR);
	    }
	    else if(strcmp(val,"PROPERTY_PARSE_ERROR") == 0){ 
		param = icalparameter_new_xlicerrortype(ICAL_XLICERRORTYPE_PROPERTYPARSEERROR);
	    }
	    else if(strcmp(val,"PARAMETER_PARSE_ERROR") == 0){ 
		param = icalparameter_new_xlicerrortype(ICAL_XLICERRORTYPE_PARAMETERPARSEERROR);
	    }
	    else if(strcmp(val,"VALUE_PARSE_ERROR") == 0){ 
		param = icalparameter_new_xlicerrortype(ICAL_XLICERRORTYPE_VALUEPARSEERROR);
	    }
	    else if(strcmp(val,"INVALID_ITIP") == 0){ 
		param = icalparameter_new_xlicerrortype(ICAL_XLICERRORTYPE_INVALIDITIP);
	    }
	    break;
	}
	case ICAL_X_PARAMETER:
	{
		param = icalparameter_new(ICAL_FBTYPE_PARAMETER);
		icalparameter_set_xvalue(param,val);
	    break;
	}

	case ICAL_NO_PARAMETER: 
	default:
	{
	    return 0;
	}
	

    }
    
    return param;
}

void
icalparameter_free (icalparameter* parameter)
{
    struct icalparameter_impl * impl;

    impl = (struct icalparameter_impl*)parameter;

/*  HACK. This always triggers, even when parameter is non-zero
    icalerror_check_arg_rv((parameter==0),"parameter");*/


#ifdef ICAL_FREE_ON_LIST_IS_ERROR
    icalerror_assert( (impl->parent ==0),"Tried to free a parameter that is still attached to a component. ");
    
#else
    if(impl->parent !=0){
	return;
    }
#endif

    
    if (impl->string != 0){
	free (impl->string);
    }
    
    if (impl->x_name != 0){
	free (impl->x_name);
    }
    
    memset(impl,0,sizeof(impl));

    impl->parent = 0;
    impl->id[0] = 'X';
    free(impl);
}


char no_parameter[]="Error: No Parameter";
char*
icalparameter_as_ical_string (icalparameter* parameter)
{
    struct icalparameter_impl* impl;
    size_t buf_size = 1024;
    char* buf; 
    char* buf_ptr;
    char *out_buf;
    char *kind_string;

    char tend[1024]; /* HACK . Should be using memory buffer ring */

    icalerror_check_arg_rz( (parameter!=0), "parameter");

    /* Create new buffer that we can append names, parameters and a
       value to, and reallocate as needed. Later, this buffer will be
       copied to a icalmemory_tmp_buffer, which is managed internally
       by libical, so it can be given to the caller without fear of
       the caller forgetting to free it */

    buf = icalmemory_new_buffer(buf_size);
    buf_ptr = buf;
    impl = (struct icalparameter_impl*)parameter;

    kind_string = icalenum_parameter_kind_to_string(impl->kind);

    if (impl->kind == ICAL_NO_PARAMETER || 
	impl->kind == ICAL_ANY_PARAMETER || 
	kind_string == 0)
    {
	icalerror_set_errno(ICAL_BADARG_ERROR);
	return 0;
    }
    
    /* Put the parameter name into the string */
    icalmemory_append_string(&buf, &buf_ptr, &buf_size, kind_string);
    icalmemory_append_string(&buf, &buf_ptr, &buf_size, "=");
    
    switch (impl->kind) {
	case ICAL_CUTYPE_PARAMETER:
	{
	    switch (impl->data.v_cutype) {
		case ICAL_CUTYPE_INDIVIDUAL: {
		    strcpy(tend,"INDIVIDUAL");break;
		}
		case ICAL_CUTYPE_GROUP:{
		    strcpy(tend,"GROUP");break;
		}
		case ICAL_CUTYPE_RESOURCE: {
		    strcpy(tend,"RESOURCE");break;
		}
		case ICAL_CUTYPE_ROOM:{
		    strcpy(tend,"ROOM");break;
		}
		case ICAL_CUTYPE_UNKNOWN:{
		    strcpy(tend,"UNKNOWN");break;
		}
		case ICAL_CUTYPE_XNAME:{
		    if (impl->string == 0){ return no_parameter;}
		    strcpy(tend,impl->string);break;
		}		
		default:{
		    icalerror_set_errno(ICAL_BADARG_ERROR);break;
		}
	    }
	    break;

	}
	case ICAL_ENCODING_PARAMETER:
	{
	    switch (impl->data.v_encoding) {
		case ICAL_ENCODING_8BIT: {
		    strcpy(tend,"8BIT");break;
		}
		case ICAL_ENCODING_BASE64:{
		    strcpy(tend,"BASE64");break;
		}
		default:{
		    icalerror_set_errno(ICAL_BADARG_ERROR);break;
		}
	    }
	    break;
	}

	case ICAL_FBTYPE_PARAMETER:
	{
	    switch (impl->data.v_fbtype) {
		case ICAL_FBTYPE_FREE:{
		    strcpy(tend,"FREE");break;
		}
		case ICAL_FBTYPE_BUSY: {
		    strcpy(tend,"BUSY");break;
		}
		case ICAL_FBTYPE_BUSYUNAVAILABLE:{
		    strcpy(tend,"BUSYUNAVAILABLE");break;
		}
		case ICAL_FBTYPE_BUSYTENTATIVE:{
		    strcpy(tend,"BUSYTENTATIVE");break;
		}
		case ICAL_FBTYPE_XNAME:{
		    if (impl->string == 0){ return no_parameter;}
		    strcpy(tend,impl->string);break;
		}
		default:{
		    icalerror_set_errno(ICAL_BADARG_ERROR);break;
		}
	    }
	    break;

	}
	case ICAL_PARTSTAT_PARAMETER:
	{
	    switch (impl->data.v_partstat) {
		case ICAL_PARTSTAT_NEEDSACTION: {
		    strcpy(tend,"NEEDSACTION");break;
		}
		case ICAL_PARTSTAT_ACCEPTED: {
		    strcpy(tend,"ACCEPTED");break;
		}
		case ICAL_PARTSTAT_DECLINED:{
		    strcpy(tend,"DECLINED");break;
		}
		case ICAL_PARTSTAT_TENTATIVE:{
		    strcpy(tend,"TENTATIVE");break;
		}
		case ICAL_PARTSTAT_DELEGATED:{
		    strcpy(tend,"DELEGATED");break;
		}
		case ICAL_PARTSTAT_COMPLETED:{
		    strcpy(tend,"COMPLETED");break;
		}
		case ICAL_PARTSTAT_INPROCESS:{
		    strcpy(tend,"INPROCESS");break;
		}
		case ICAL_PARTSTAT_XNAME:{
		    if (impl->string == 0){ return no_parameter;}
		    strcpy(tend,impl->string);break;
		}
		default:{
		    icalerror_set_errno(ICAL_BADARG_ERROR);break;
		}
	    }
	    break;

	}
	case ICAL_RANGE_PARAMETER:
	{
	    switch (impl->data.v_range) {
		case ICAL_RANGE_THISANDPRIOR: {
		    strcpy(tend,"THISANDPRIOR");break;
		}
		case ICAL_RANGE_THISANDFUTURE: {
		    strcpy(tend,"THISANDFUTURE");break;
		}
		default:{
		    icalerror_set_errno(ICAL_BADARG_ERROR);break;
		}
	    }
	    break;
	}
	case ICAL_RELATED_PARAMETER:
	{
	    switch (impl->data.v_related) {
		case ICAL_RELATED_START: {
		    strcpy(tend,"START");break;
		}
		case ICAL_RELATED_END: {
		    strcpy(tend,"END");break;
		}
		default:{
		    icalerror_set_errno(ICAL_BADARG_ERROR);break;
		}
	    }
	    break;
	}
	case ICAL_RELTYPE_PARAMETER:
	{
	    switch (impl->data.v_reltype) {
		case ICAL_RELTYPE_PARENT: {
		    strcpy(tend,"PARENT");break;
		}
		case ICAL_RELTYPE_CHILD:{
		    strcpy(tend,"CHILD");break;
		}
		case ICAL_RELTYPE_SIBLING:{
		    strcpy(tend,"SIBLING");break;
		}
		case ICAL_RELTYPE_XNAME:{
		    if (impl->string == 0){ return no_parameter;}
		    strcpy(tend,impl->string);break;
		}
		default:{
		    icalerror_set_errno(ICAL_BADARG_ERROR);break;
		}
	    }
	    break;
	}
	case ICAL_ROLE_PARAMETER:
	{
	    switch (impl->data.v_role) {
		case ICAL_ROLE_CHAIR: {
		    strcpy(tend,"CHAIR");break;
		}
		case ICAL_ROLE_REQPARTICIPANT: {
		    strcpy(tend,"REQ-PARTICIPANT");break;
		}
		case ICAL_ROLE_OPTPARTICIPANT:  {
		    strcpy(tend,"OPT-PARTICIPANT");break;
		}
		case ICAL_ROLE_NONPARTICIPANT: {
		    strcpy(tend,"NON-PARTICIPANT");break;
		}
		case ICAL_ROLE_XNAME:{
		    if (impl->string == 0){ return no_parameter;}
		    strcpy(tend,impl->string);break;
		}
		default:{
		    icalerror_set_errno(ICAL_BADARG_ERROR);break;
		}
	    }
	    break;
	}
	case ICAL_RSVP_PARAMETER:
	{
	    switch (impl->data.v_rsvp) {
		case 1: {
		    strcpy(tend,"TRUE");break;
		}
		case 0: {
		    strcpy(tend,"FALSE");break;
		}
		default:{
		    icalerror_set_errno(ICAL_BADARG_ERROR);break;
		}
	    }
	    break;
	}
	case ICAL_VALUE_PARAMETER:
	{
	    switch (impl->data.v_value) {
		case ICAL_VALUE_BINARY:  {
		    strcpy(tend,"BINARY");break;
		}
		case ICAL_VALUE_BOOLEAN:  {
		    strcpy(tend,"BOOLEAN");break;
		}
		case ICAL_VALUE_CALADDRESS:  {
		    strcpy(tend,"CAL-ADDRESS");break;
		}
		case ICAL_VALUE_DATE:  {
		    strcpy(tend,"DATE");break;
		}
		case ICAL_VALUE_DATETIME:  {
		    strcpy(tend,"DATE-TIME");break;
		}
		case ICAL_VALUE_DURATION:  {
		    strcpy(tend,"DURATION");break;
		}
		case ICAL_VALUE_FLOAT:  {
		    strcpy(tend,"FLOAT");break;
		}
		case ICAL_VALUE_INTEGER:  {
		    strcpy(tend,"INTEGER");break;
		}
		case ICAL_VALUE_PERIOD:  {
		    strcpy(tend,"PERIOD");break;
		}
		case ICAL_VALUE_RECUR:  {
		    strcpy(tend,"RECUR");break;
		}
		case ICAL_VALUE_TEXT:  {
		    strcpy(tend,"TEXT");break;
		}
		case ICAL_VALUE_TIME:  {
		    strcpy(tend,"TIME");break;
		}
		case ICAL_VALUE_URI:  {
		    strcpy(tend,"URI");break;
		}
		case ICAL_VALUE_UTCOFFSET: {
		    strcpy(tend,"UTC-OFFSET");break;
		}
		case ICAL_VALUE_XNAME: {
		    if (impl->string == 0){ return no_parameter;}
		    strcpy(tend,impl->string);break;
		}
		default:{
		    strcpy(tend,"ERROR");break;
		    icalerror_set_errno(ICAL_BADARG_ERROR);break;
		}
	    }
	    break;
	}


	case ICAL_XLICERRORTYPE_PARAMETER:
	{
	    switch (impl->data.v_xlicerrortype) {
		case ICAL_XLICERRORTYPE_COMPONENTPARSEERROR:
		{
		    strcpy(tend,"COMPONENT_PARSE_ERROR");break;
		}
		case ICAL_XLICERRORTYPE_PROPERTYPARSEERROR:
		{
		    strcpy(tend,"PROPERTY_PARSE_ERROR");break;
		}
		case ICAL_XLICERRORTYPE_PARAMETERPARSEERROR:
		{
		    strcpy(tend,"PARAMETER_PARSE_ERROR");break;
		}
		case ICAL_XLICERRORTYPE_VALUEPARSEERROR:
		{
		    strcpy(tend,"VALUE_PARSE_ERROR");break;
		}
		case ICAL_XLICERRORTYPE_INVALIDITIP:
		{
		    strcpy(tend,"INVALID_ITIP");break;
		}
	    }
	    break;
	}
	
	case ICAL_XLICCOMPARETYPE_PARAMETER:
	{
	    switch (impl->data.v_xliccomparetype) {
		case ICAL_XLICCOMPARETYPE_EQUAL:
		{
		    strcpy(tend,"EQUAL");break;
		}
		case ICAL_XLICCOMPARETYPE_NOTEQUAL:
		{
		    strcpy(tend,"NOTEQUAL");break;
		}
		case ICAL_XLICCOMPARETYPE_LESS:
		{
		    strcpy(tend,"LESS");break;
		}
		case ICAL_XLICCOMPARETYPE_GREATER:
		{
		    strcpy(tend,"GREATER");break;
		}
		case ICAL_XLICCOMPARETYPE_LESSEQUAL:
		{
		    strcpy(tend,"LESSEQUAL");break;
		}
		case ICAL_XLICCOMPARETYPE_GREATEREQUAL:
		{
		    strcpy(tend,"GREATEREQUAL");break;
		}
		case ICAL_XLICCOMPARETYPE_REGEX:
		{
		    strcpy(tend,"REGEX");break;
		}
		break;
	    }

	    default:{
		icalerror_set_errno(ICAL_BADARG_ERROR);break;
	    }
	    break;
	}


	case ICAL_SENTBY_PARAMETER:
	case ICAL_TZID_PARAMETER:
	case ICAL_X_PARAMETER:
	case ICAL_FMTTYPE_PARAMETER:
	case ICAL_LANGUAGE_PARAMETER:
	case ICAL_MEMBER_PARAMETER:
	case ICAL_DELEGATEDFROM_PARAMETER:
	case ICAL_DELEGATEDTO_PARAMETER:
	case ICAL_DIR_PARAMETER:
	case ICAL_ALTREP_PARAMETER:
	case ICAL_CN_PARAMETER:
	{
	    if (impl->string == 0){ return no_parameter;}
	    strcpy(tend,impl->string);break;
	    break;
	}

	case ICAL_NO_PARAMETER:
	case ICAL_ANY_PARAMETER:
	{
	    /* These are actually handled before the case/switch
               clause */
	}

    }

    icalmemory_append_string(&buf, &buf_ptr, &buf_size, tend); 

    /* Now, copy the buffer to a tmp_buffer, which is safe to give to
       the caller without worring about de-allocating it. */

    
    out_buf = icalmemory_tmp_buffer(strlen(buf));
    strcpy(out_buf, buf);

    icalmemory_free_buffer(buf);

    return out_buf;

}


int
icalparameter_is_valid (icalparameter* parameter);


icalparameter_kind
icalparameter_isa (icalparameter* parameter)
{
    if(parameter == 0){
	return ICAL_NO_PARAMETER;
    }

    return ((struct icalparameter_impl *)parameter)->kind;
}


int
icalparameter_isa_parameter (void* parameter)
{
    struct icalparameter_impl *impl = (struct icalparameter_impl *)parameter;

    if (parameter == 0){
	return 0;
    }

    if (strcmp(impl->id,"para") == 0) {
	return 1;
    } else {
	return 0;
    }
}


void
icalparameter_set_xname (icalparameter* param, char* v)
{
    struct icalparameter_impl *impl = (struct icalparameter_impl*)param;
    icalerror_check_arg_rv( (param!=0),"param");
    icalerror_check_arg_rv( (v!=0),"v");

    if (impl->x_name != 0){
	free(impl->x_name);
    }

    impl->x_name = strdup(v);

    if (impl->x_name == 0){
	errno = ENOMEM;
    }

}

char*
icalparameter_get_xname (icalparameter* param)
{
    struct icalparameter_impl *impl = (struct icalparameter_impl*)param;
    icalerror_check_arg_rz( (param!=0),"param");

    return impl->x_name;
}

void
icalparameter_set_xvalue (icalparameter* param, char* v)
{
    struct icalparameter_impl *impl = (struct icalparameter_impl*)param;

    icalerror_check_arg_rv( (param!=0),"param");
    icalerror_check_arg_rv( (v!=0),"v");

    if (impl->string != 0){
	free(impl->string);
    }

    impl->string = strdup(v);

    if (impl->string == 0){
	errno = ENOMEM;
    }

}

char*
icalparameter_get_xvalue (icalparameter* param)
{
    struct icalparameter_impl *impl = (struct icalparameter_impl*)param;

    icalerror_check_arg_rz( (param!=0),"param");

    return impl->string;

}

void icalparameter_set_parent(icalparameter* param,
			     icalproperty* property)
{
    struct icalparameter_impl *impl = (struct icalparameter_impl*)param;

    icalerror_check_arg_rv( (param!=0),"param");

    impl->parent = property;
}

icalproperty* icalparameter_get_parent(icalparameter* param)
{
    struct icalparameter_impl *impl = (struct icalparameter_impl*)param;

    icalerror_check_arg_rv( (param!=0),"param");

    return impl->parent;
}


/* Everything below this line is machine generated. Do not edit. */
/* ALTREP */
icalparameter* icalparameter_new_altrep(char* v)
{
   struct icalparameter_impl *impl;
   icalerror_clear_errno();
   icalerror_check_arg_rz( (v!=0),"v");
   impl = icalparameter_new_impl(ICAL_ALTREP_PARAMETER);
   if (impl == 0) {
      return 0;
   }

   icalparameter_set_altrep((icalparameter*) impl,v);
   if (icalerrno != ICAL_NO_ERROR) {
      icalparameter_free((icalparameter*) impl);
      return 0;
   }

   return (icalparameter*) impl;
}

char* icalparameter_get_altrep(icalparameter* param)
{
   icalerror_clear_errno();
    icalerror_check_arg_rz( (param!=0), "param");
    return ((struct icalparameter_impl*)param)->string;
}

void icalparameter_set_altrep(icalparameter* param, char* v)
{
   icalerror_check_arg_rz( (v!=0),"v");
   icalerror_check_arg_rv( (param!=0), "param");
   icalerror_clear_errno();
   
   ((struct icalparameter_impl*)param)->string = strdup(v);
}

/* CN */
icalparameter* icalparameter_new_cn(char* v)
{
   struct icalparameter_impl *impl;
   icalerror_clear_errno();
   icalerror_check_arg_rz( (v!=0),"v");
   impl = icalparameter_new_impl(ICAL_CN_PARAMETER);
   if (impl == 0) {
      return 0;
   }

   icalparameter_set_cn((icalparameter*) impl,v);
   if (icalerrno != ICAL_NO_ERROR) {
      icalparameter_free((icalparameter*) impl);
      return 0;
   }

   return (icalparameter*) impl;
}

char* icalparameter_get_cn(icalparameter* param)
{
   icalerror_clear_errno();
    icalerror_check_arg_rz( (param!=0), "param");
    return ((struct icalparameter_impl*)param)->string;
}

void icalparameter_set_cn(icalparameter* param, char* v)
{
   icalerror_check_arg_rz( (v!=0),"v");
   icalerror_check_arg_rv( (param!=0), "param");
   icalerror_clear_errno();
   
   ((struct icalparameter_impl*)param)->string = strdup(v);
}

/* CUTYPE */
icalparameter* icalparameter_new_cutype(icalparameter_cutype v)
{
   struct icalparameter_impl *impl;
   icalerror_clear_errno();
   
   impl = icalparameter_new_impl(ICAL_CUTYPE_PARAMETER);
   if (impl == 0) {
      return 0;
   }

   icalparameter_set_cutype((icalparameter*) impl,v);
   if (icalerrno != ICAL_NO_ERROR) {
      icalparameter_free((icalparameter*) impl);
      return 0;
   }

   return (icalparameter*) impl;
}

icalparameter_cutype icalparameter_get_cutype(icalparameter* param)
{
   icalerror_clear_errno();
    icalerror_check_arg( (param!=0), "param");
     if ( ((struct icalparameter_impl*)param)->string != 0){
        return ICAL_CUTYPE_XNAME;
        }

    return ((struct icalparameter_impl*)param)->data.v_cutype;

}

void icalparameter_set_cutype(icalparameter* param, icalparameter_cutype v)
{
   
   icalerror_check_arg_rv( (param!=0), "param");
   icalerror_clear_errno();
   
   ((struct icalparameter_impl*)param)->data.v_cutype = v;
}

/* DELEGATED-FROM */
icalparameter* icalparameter_new_delegatedfrom(char* v)
{
   struct icalparameter_impl *impl;
   icalerror_clear_errno();
   icalerror_check_arg_rz( (v!=0),"v");
   impl = icalparameter_new_impl(ICAL_DELEGATEDFROM_PARAMETER);
   if (impl == 0) {
      return 0;
   }

   icalparameter_set_delegatedfrom((icalparameter*) impl,v);
   if (icalerrno != ICAL_NO_ERROR) {
      icalparameter_free((icalparameter*) impl);
      return 0;
   }

   return (icalparameter*) impl;
}

char* icalparameter_get_delegatedfrom(icalparameter* param)
{
   icalerror_clear_errno();
    icalerror_check_arg_rz( (param!=0), "param");
    return ((struct icalparameter_impl*)param)->string;
}

void icalparameter_set_delegatedfrom(icalparameter* param, char* v)
{
   icalerror_check_arg_rz( (v!=0),"v");
   icalerror_check_arg_rv( (param!=0), "param");
   icalerror_clear_errno();
   
   ((struct icalparameter_impl*)param)->string = strdup(v);
}

/* DELEGATED-TO */
icalparameter* icalparameter_new_delegatedto(char* v)
{
   struct icalparameter_impl *impl;
   icalerror_clear_errno();
   icalerror_check_arg_rz( (v!=0),"v");
   impl = icalparameter_new_impl(ICAL_DELEGATEDTO_PARAMETER);
   if (impl == 0) {
      return 0;
   }

   icalparameter_set_delegatedto((icalparameter*) impl,v);
   if (icalerrno != ICAL_NO_ERROR) {
      icalparameter_free((icalparameter*) impl);
      return 0;
   }

   return (icalparameter*) impl;
}

char* icalparameter_get_delegatedto(icalparameter* param)
{
   icalerror_clear_errno();
    icalerror_check_arg_rz( (param!=0), "param");
    return ((struct icalparameter_impl*)param)->string;
}

void icalparameter_set_delegatedto(icalparameter* param, char* v)
{
   icalerror_check_arg_rz( (v!=0),"v");
   icalerror_check_arg_rv( (param!=0), "param");
   icalerror_clear_errno();
   
   ((struct icalparameter_impl*)param)->string = strdup(v);
}

/* DIR */
icalparameter* icalparameter_new_dir(char* v)
{
   struct icalparameter_impl *impl;
   icalerror_clear_errno();
   icalerror_check_arg_rz( (v!=0),"v");
   impl = icalparameter_new_impl(ICAL_DIR_PARAMETER);
   if (impl == 0) {
      return 0;
   }

   icalparameter_set_dir((icalparameter*) impl,v);
   if (icalerrno != ICAL_NO_ERROR) {
      icalparameter_free((icalparameter*) impl);
      return 0;
   }

   return (icalparameter*) impl;
}

char* icalparameter_get_dir(icalparameter* param)
{
   icalerror_clear_errno();
    icalerror_check_arg_rz( (param!=0), "param");
    return ((struct icalparameter_impl*)param)->string;
}

void icalparameter_set_dir(icalparameter* param, char* v)
{
   icalerror_check_arg_rz( (v!=0),"v");
   icalerror_check_arg_rv( (param!=0), "param");
   icalerror_clear_errno();
   
   ((struct icalparameter_impl*)param)->string = strdup(v);
}

/* ENCODING */
icalparameter* icalparameter_new_encoding(icalparameter_encoding v)
{
   struct icalparameter_impl *impl;
   icalerror_clear_errno();
   
   impl = icalparameter_new_impl(ICAL_ENCODING_PARAMETER);
   if (impl == 0) {
      return 0;
   }

   icalparameter_set_encoding((icalparameter*) impl,v);
   if (icalerrno != ICAL_NO_ERROR) {
      icalparameter_free((icalparameter*) impl);
      return 0;
   }

   return (icalparameter*) impl;
}

icalparameter_encoding icalparameter_get_encoding(icalparameter* param)
{
   icalerror_clear_errno();
    icalerror_check_arg( (param!=0), "param");
     if ( ((struct icalparameter_impl*)param)->string != 0){
        return ICAL_ENCODING_XNAME;
        }

    return ((struct icalparameter_impl*)param)->data.v_encoding;

}

void icalparameter_set_encoding(icalparameter* param, icalparameter_encoding v)
{
   
   icalerror_check_arg_rv( (param!=0), "param");
   icalerror_clear_errno();
   
   ((struct icalparameter_impl*)param)->data.v_encoding = v;
}

/* FBTYPE */
icalparameter* icalparameter_new_fbtype(icalparameter_fbtype v)
{
   struct icalparameter_impl *impl;
   icalerror_clear_errno();
   
   impl = icalparameter_new_impl(ICAL_FBTYPE_PARAMETER);
   if (impl == 0) {
      return 0;
   }

   icalparameter_set_fbtype((icalparameter*) impl,v);
   if (icalerrno != ICAL_NO_ERROR) {
      icalparameter_free((icalparameter*) impl);
      return 0;
   }

   return (icalparameter*) impl;
}

icalparameter_fbtype icalparameter_get_fbtype(icalparameter* param)
{
   icalerror_clear_errno();
    icalerror_check_arg( (param!=0), "param");
     if ( ((struct icalparameter_impl*)param)->string != 0){
        return ICAL_FBTYPE_XNAME;
        }

    return ((struct icalparameter_impl*)param)->data.v_fbtype;

}

void icalparameter_set_fbtype(icalparameter* param, icalparameter_fbtype v)
{
   
   icalerror_check_arg_rv( (param!=0), "param");
   icalerror_clear_errno();
   
   ((struct icalparameter_impl*)param)->data.v_fbtype = v;
}

/* FMTTYPE */
icalparameter* icalparameter_new_fmttype(char* v)
{
   struct icalparameter_impl *impl;
   icalerror_clear_errno();
   icalerror_check_arg_rz( (v!=0),"v");
   impl = icalparameter_new_impl(ICAL_FMTTYPE_PARAMETER);
   if (impl == 0) {
      return 0;
   }

   icalparameter_set_fmttype((icalparameter*) impl,v);
   if (icalerrno != ICAL_NO_ERROR) {
      icalparameter_free((icalparameter*) impl);
      return 0;
   }

   return (icalparameter*) impl;
}

char* icalparameter_get_fmttype(icalparameter* param)
{
   icalerror_clear_errno();
    icalerror_check_arg_rz( (param!=0), "param");
    return ((struct icalparameter_impl*)param)->string;
}

void icalparameter_set_fmttype(icalparameter* param, char* v)
{
   icalerror_check_arg_rz( (v!=0),"v");
   icalerror_check_arg_rv( (param!=0), "param");
   icalerror_clear_errno();
   
   ((struct icalparameter_impl*)param)->string = strdup(v);
}

/* LANGUAGE */
icalparameter* icalparameter_new_language(char* v)
{
   struct icalparameter_impl *impl;
   icalerror_clear_errno();
   icalerror_check_arg_rz( (v!=0),"v");
   impl = icalparameter_new_impl(ICAL_LANGUAGE_PARAMETER);
   if (impl == 0) {
      return 0;
   }

   icalparameter_set_language((icalparameter*) impl,v);
   if (icalerrno != ICAL_NO_ERROR) {
      icalparameter_free((icalparameter*) impl);
      return 0;
   }

   return (icalparameter*) impl;
}

char* icalparameter_get_language(icalparameter* param)
{
   icalerror_clear_errno();
    icalerror_check_arg_rz( (param!=0), "param");
    return ((struct icalparameter_impl*)param)->string;
}

void icalparameter_set_language(icalparameter* param, char* v)
{
   icalerror_check_arg_rz( (v!=0),"v");
   icalerror_check_arg_rv( (param!=0), "param");
   icalerror_clear_errno();
   
   ((struct icalparameter_impl*)param)->string = strdup(v);
}

/* MEMBER */
icalparameter* icalparameter_new_member(char* v)
{
   struct icalparameter_impl *impl;
   icalerror_clear_errno();
   icalerror_check_arg_rz( (v!=0),"v");
   impl = icalparameter_new_impl(ICAL_MEMBER_PARAMETER);
   if (impl == 0) {
      return 0;
   }

   icalparameter_set_member((icalparameter*) impl,v);
   if (icalerrno != ICAL_NO_ERROR) {
      icalparameter_free((icalparameter*) impl);
      return 0;
   }

   return (icalparameter*) impl;
}

char* icalparameter_get_member(icalparameter* param)
{
   icalerror_clear_errno();
    icalerror_check_arg_rz( (param!=0), "param");
    return ((struct icalparameter_impl*)param)->string;
}

void icalparameter_set_member(icalparameter* param, char* v)
{
   icalerror_check_arg_rz( (v!=0),"v");
   icalerror_check_arg_rv( (param!=0), "param");
   icalerror_clear_errno();
   
   ((struct icalparameter_impl*)param)->string = strdup(v);
}

/* PARTSTAT */
icalparameter* icalparameter_new_partstat(icalparameter_partstat v)
{
   struct icalparameter_impl *impl;
   icalerror_clear_errno();
   
   impl = icalparameter_new_impl(ICAL_PARTSTAT_PARAMETER);
   if (impl == 0) {
      return 0;
   }

   icalparameter_set_partstat((icalparameter*) impl,v);
   if (icalerrno != ICAL_NO_ERROR) {
      icalparameter_free((icalparameter*) impl);
      return 0;
   }

   return (icalparameter*) impl;
}

icalparameter_partstat icalparameter_get_partstat(icalparameter* param)
{
   icalerror_clear_errno();
    icalerror_check_arg( (param!=0), "param");
     if ( ((struct icalparameter_impl*)param)->string != 0){
        return ICAL_PARTSTAT_XNAME;
        }

    return ((struct icalparameter_impl*)param)->data.v_partstat;

}

void icalparameter_set_partstat(icalparameter* param, icalparameter_partstat v)
{
   
   icalerror_check_arg_rv( (param!=0), "param");
   icalerror_clear_errno();
   
   ((struct icalparameter_impl*)param)->data.v_partstat = v;
}

/* RANGE */
icalparameter* icalparameter_new_range(icalparameter_range v)
{
   struct icalparameter_impl *impl;
   icalerror_clear_errno();
   
   impl = icalparameter_new_impl(ICAL_RANGE_PARAMETER);
   if (impl == 0) {
      return 0;
   }

   icalparameter_set_range((icalparameter*) impl,v);
   if (icalerrno != ICAL_NO_ERROR) {
      icalparameter_free((icalparameter*) impl);
      return 0;
   }

   return (icalparameter*) impl;
}

icalparameter_range icalparameter_get_range(icalparameter* param)
{
   icalerror_clear_errno();
    icalerror_check_arg( (param!=0), "param");
     if ( ((struct icalparameter_impl*)param)->string != 0){
        return ICAL_PARTSTAT_XNAME;
        }

    return ((struct icalparameter_impl*)param)->data.v_range;

}

void icalparameter_set_range(icalparameter* param, icalparameter_range v)
{
   
   icalerror_check_arg_rv( (param!=0), "param");
   icalerror_clear_errno();
   
   ((struct icalparameter_impl*)param)->data.v_range = v;
}

/* RELATED */
icalparameter* icalparameter_new_related(icalparameter_related v)
{
   struct icalparameter_impl *impl;
   icalerror_clear_errno();
   
   impl = icalparameter_new_impl(ICAL_RELATED_PARAMETER);
   if (impl == 0) {
      return 0;
   }

   icalparameter_set_related((icalparameter*) impl,v);
   if (icalerrno != ICAL_NO_ERROR) {
      icalparameter_free((icalparameter*) impl);
      return 0;
   }

   return (icalparameter*) impl;
}

icalparameter_related icalparameter_get_related(icalparameter* param)
{
   icalerror_clear_errno();
    icalerror_check_arg( (param!=0), "param");
     if ( ((struct icalparameter_impl*)param)->string != 0){
        return ICAL_PARTSTAT_XNAME;
        }

    return ((struct icalparameter_impl*)param)->data.v_related;

}

void icalparameter_set_related(icalparameter* param, icalparameter_related v)
{
   
   icalerror_check_arg_rv( (param!=0), "param");
   icalerror_clear_errno();
   
   ((struct icalparameter_impl*)param)->data.v_related = v;
}

/* RELTYPE */
icalparameter* icalparameter_new_reltype(icalparameter_reltype v)
{
   struct icalparameter_impl *impl;
   icalerror_clear_errno();
   
   impl = icalparameter_new_impl(ICAL_RELTYPE_PARAMETER);
   if (impl == 0) {
      return 0;
   }

   icalparameter_set_reltype((icalparameter*) impl,v);
   if (icalerrno != ICAL_NO_ERROR) {
      icalparameter_free((icalparameter*) impl);
      return 0;
   }

   return (icalparameter*) impl;
}

icalparameter_reltype icalparameter_get_reltype(icalparameter* param)
{
   icalerror_clear_errno();
    icalerror_check_arg( (param!=0), "param");
     if ( ((struct icalparameter_impl*)param)->string != 0){
        return ICAL_RELTYPE_XNAME;
        }

    return ((struct icalparameter_impl*)param)->data.v_reltype;

}

void icalparameter_set_reltype(icalparameter* param, icalparameter_reltype v)
{
   
   icalerror_check_arg_rv( (param!=0), "param");
   icalerror_clear_errno();
   
   ((struct icalparameter_impl*)param)->data.v_reltype = v;
}

/* ROLE */
icalparameter* icalparameter_new_role(icalparameter_role v)
{
   struct icalparameter_impl *impl;
   icalerror_clear_errno();
   
   impl = icalparameter_new_impl(ICAL_ROLE_PARAMETER);
   if (impl == 0) {
      return 0;
   }

   icalparameter_set_role((icalparameter*) impl,v);
   if (icalerrno != ICAL_NO_ERROR) {
      icalparameter_free((icalparameter*) impl);
      return 0;
   }

   return (icalparameter*) impl;
}

icalparameter_role icalparameter_get_role(icalparameter* param)
{
   icalerror_clear_errno();
    icalerror_check_arg( (param!=0), "param");
     if ( ((struct icalparameter_impl*)param)->string != 0){
        return ICAL_ROLE_XNAME;
        }

    return ((struct icalparameter_impl*)param)->data.v_role;

}

void icalparameter_set_role(icalparameter* param, icalparameter_role v)
{
   
   icalerror_check_arg_rv( (param!=0), "param");
   icalerror_clear_errno();
   
   ((struct icalparameter_impl*)param)->data.v_role = v;
}

/* RSVP */
icalparameter* icalparameter_new_rsvp(int v)
{
   struct icalparameter_impl *impl;
   icalerror_clear_errno();
   
   impl = icalparameter_new_impl(ICAL_RSVP_PARAMETER);
   if (impl == 0) {
      return 0;
   }

   icalparameter_set_rsvp((icalparameter*) impl,v);
   if (icalerrno != ICAL_NO_ERROR) {
      icalparameter_free((icalparameter*) impl);
      return 0;
   }

   return (icalparameter*) impl;
}

int icalparameter_get_rsvp(icalparameter* param)
{
   icalerror_clear_errno();
    icalerror_check_arg( (param!=0), "param");
     if ( ((struct icalparameter_impl*)param)->string != 0){
        return ICAL_ROLE_XNAME;
        }

    return ((struct icalparameter_impl*)param)->data.v_rsvp;

}

void icalparameter_set_rsvp(icalparameter* param, int v)
{
   
   icalerror_check_arg_rv( (param!=0), "param");
   icalerror_clear_errno();
   
   ((struct icalparameter_impl*)param)->data.v_rsvp = v;
}

/* SENT-BY */
icalparameter* icalparameter_new_sentby(char* v)
{
   struct icalparameter_impl *impl;
   icalerror_clear_errno();
   icalerror_check_arg_rz( (v!=0),"v");
   impl = icalparameter_new_impl(ICAL_SENTBY_PARAMETER);
   if (impl == 0) {
      return 0;
   }

   icalparameter_set_sentby((icalparameter*) impl,v);
   if (icalerrno != ICAL_NO_ERROR) {
      icalparameter_free((icalparameter*) impl);
      return 0;
   }

   return (icalparameter*) impl;
}

char* icalparameter_get_sentby(icalparameter* param)
{
   icalerror_clear_errno();
    icalerror_check_arg_rz( (param!=0), "param");
    return ((struct icalparameter_impl*)param)->string;
}

void icalparameter_set_sentby(icalparameter* param, char* v)
{
   icalerror_check_arg_rz( (v!=0),"v");
   icalerror_check_arg_rv( (param!=0), "param");
   icalerror_clear_errno();
   
   ((struct icalparameter_impl*)param)->string = strdup(v);
}

/* TZID */
icalparameter* icalparameter_new_tzid(char* v)
{
   struct icalparameter_impl *impl;
   icalerror_clear_errno();
   icalerror_check_arg_rz( (v!=0),"v");
   impl = icalparameter_new_impl(ICAL_TZID_PARAMETER);
   if (impl == 0) {
      return 0;
   }

   icalparameter_set_tzid((icalparameter*) impl,v);
   if (icalerrno != ICAL_NO_ERROR) {
      icalparameter_free((icalparameter*) impl);
      return 0;
   }

   return (icalparameter*) impl;
}

char* icalparameter_get_tzid(icalparameter* param)
{
   icalerror_clear_errno();
    icalerror_check_arg_rz( (param!=0), "param");
    return ((struct icalparameter_impl*)param)->string;
}

void icalparameter_set_tzid(icalparameter* param, char* v)
{
   icalerror_check_arg_rz( (v!=0),"v");
   icalerror_check_arg_rv( (param!=0), "param");
   icalerror_clear_errno();
   
   ((struct icalparameter_impl*)param)->string = strdup(v);
}

/* VALUE */
icalparameter* icalparameter_new_value(icalparameter_value v)
{
   struct icalparameter_impl *impl;
   icalerror_clear_errno();
   
   impl = icalparameter_new_impl(ICAL_VALUE_PARAMETER);
   if (impl == 0) {
      return 0;
   }

   icalparameter_set_value((icalparameter*) impl,v);
   if (icalerrno != ICAL_NO_ERROR) {
      icalparameter_free((icalparameter*) impl);
      return 0;
   }

   return (icalparameter*) impl;
}

icalparameter_value icalparameter_get_value(icalparameter* param)
{
   icalerror_clear_errno();
    icalerror_check_arg( (param!=0), "param");
     if ( ((struct icalparameter_impl*)param)->string != 0){
        return ICAL_VALUE_XNAME;
        }

    return ((struct icalparameter_impl*)param)->data.v_value;

}

void icalparameter_set_value(icalparameter* param, icalparameter_value v)
{
   
   icalerror_check_arg_rv( (param!=0), "param");
   icalerror_clear_errno();
   
   ((struct icalparameter_impl*)param)->data.v_value = v;
}

/* X */
icalparameter* icalparameter_new_x(char* v)
{
   struct icalparameter_impl *impl;
   icalerror_clear_errno();
   icalerror_check_arg_rz( (v!=0),"v");
   impl = icalparameter_new_impl(ICAL_X_PARAMETER);
   if (impl == 0) {
      return 0;
   }

   icalparameter_set_x((icalparameter*) impl,v);
   if (icalerrno != ICAL_NO_ERROR) {
      icalparameter_free((icalparameter*) impl);
      return 0;
   }

   return (icalparameter*) impl;
}

char* icalparameter_get_x(icalparameter* param)
{
   icalerror_clear_errno();
    icalerror_check_arg_rz( (param!=0), "param");
    return ((struct icalparameter_impl*)param)->string;
}

void icalparameter_set_x(icalparameter* param, char* v)
{
   icalerror_check_arg_rz( (v!=0),"v");
   icalerror_check_arg_rv( (param!=0), "param");
   icalerror_clear_errno();
   
   ((struct icalparameter_impl*)param)->string = strdup(v);
}

/* X-LIC-ERRORTYPE */
icalparameter* icalparameter_new_xlicerrortype(icalparameter_xlicerrortype v)
{
   struct icalparameter_impl *impl;
   icalerror_clear_errno();
   
   impl = icalparameter_new_impl(ICAL_XLICERRORTYPE_PARAMETER);
   if (impl == 0) {
      return 0;
   }

   icalparameter_set_xlicerrortype((icalparameter*) impl,v);
   if (icalerrno != ICAL_NO_ERROR) {
      icalparameter_free((icalparameter*) impl);
      return 0;
   }

   return (icalparameter*) impl;
}

icalparameter_xlicerrortype icalparameter_get_xlicerrortype(icalparameter* param)
{
   icalerror_clear_errno();
    icalerror_check_arg( (param!=0), "param");
     if ( ((struct icalparameter_impl*)param)->string != 0){
        return ICAL_VALUE_XNAME;
        }

    return ((struct icalparameter_impl*)param)->data.v_xlicerrortype;

}

void icalparameter_set_xlicerrortype(icalparameter* param, icalparameter_xlicerrortype v)
{
   
   icalerror_check_arg_rv( (param!=0), "param");
   icalerror_clear_errno();
   
   ((struct icalparameter_impl*)param)->data.v_xlicerrortype = v;
}

/* X-LIC-COMPARETYPE */
icalparameter* icalparameter_new_xliccomparetype(icalparameter_xliccomparetype v)
{
   struct icalparameter_impl *impl;
   icalerror_clear_errno();
   
   impl = icalparameter_new_impl(ICAL_XLICCOMPARETYPE_PARAMETER);
   if (impl == 0) {
      return 0;
   }

   icalparameter_set_xliccomparetype((icalparameter*) impl,v);
   if (icalerrno != ICAL_NO_ERROR) {
      icalparameter_free((icalparameter*) impl);
      return 0;
   }

   return (icalparameter*) impl;
}

icalparameter_xliccomparetype icalparameter_get_xliccomparetype(icalparameter* param)
{
   icalerror_clear_errno();
    icalerror_check_arg( (param!=0), "param");
     if ( ((struct icalparameter_impl*)param)->string != 0){
        return ICAL_VALUE_XNAME;
        }

    return ((struct icalparameter_impl*)param)->data.v_xliccomparetype;

}

void icalparameter_set_xliccomparetype(icalparameter* param, icalparameter_xliccomparetype v)
{
   
   icalerror_check_arg_rv( (param!=0), "param");
   icalerror_clear_errno();
   
   ((struct icalparameter_impl*)param)->data.v_xliccomparetype = v;
}

