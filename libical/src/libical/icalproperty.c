/* -*- Mode: C -*- */
/*======================================================================
  FILE: icalproperty.c
  CREATOR: eric 28 April 1999
  
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
  The original code is icalproperty.c

======================================================================*/

#include <string.h> /* For strdup, rindex */
#include <assert.h> 
#include <stdlib.h>
#include <errno.h>
#include <stdio.h> /* for printf */
#include <stdarg.h> /* for va_list, va_start, etc. */

#include "ical.h"
#include "pvl.h"
#include "icalenums.h"
#include "icalerror.h"
#include "icalmemory.h"

/* Private routines for icalproperty */
void icalvalue_set_parent(icalvalue* value,
			     icalproperty* property);
icalproperty* icalvalue_get_parent(icalvalue* value);

void icalparameter_set_parent(icalparameter* param,
			     icalproperty* property);
icalproperty* icalparameter_get_parent(icalparameter* value);


void icalproperty_set_x_name(icalproperty* prop, char* name);

struct icalproperty_impl 
{
	char id[5];
	icalproperty_kind kind;
	char* x_name;
	pvl_list parameters;
	pvl_elem parameter_iterator;
	icalvalue* value;
	icalcomponent *parent;
};

void icalproperty_add_parameters(struct icalproperty_impl *impl,va_list args)
{

    void* vp;
    
    while((vp = va_arg(args, void*)) != 0) {

	if (icalvalue_isa_value(vp) != 0 ){
	} else if (icalparameter_isa_parameter(vp) != 0 ){

	    icalproperty_add_parameter((icalproperty*)impl,
				       (icalparameter*)vp);
	} else {
	    abort();
	}

    }
    
    
}

struct icalproperty_impl*
icalproperty_new_impl (icalproperty_kind kind)
{
    struct icalproperty_impl* prop;

    if ( ( prop = (struct icalproperty_impl*)
	   malloc(sizeof(struct icalproperty_impl))) == 0) {
	errno = ENOMEM;
	icalerror_set_errno(ICAL_NEWFAILED_ERROR);
	return 0;
    }
    
    strcpy(prop->id,"prop");

    prop->kind = kind;
    prop->parameters = pvl_newlist();
    prop->parameter_iterator = 0;
    prop->value = 0;
    prop->x_name = 0;
    prop->parent = 0;

    return prop;
}


icalproperty*
icalproperty_new (icalproperty_kind kind)
{
    icalproperty *prop = (icalproperty*)icalproperty_new_impl(kind);

    return prop;
}


icalproperty*
icalproperty_new_clone(icalproperty* prop)
{
    struct icalproperty_impl *old = (struct icalproperty_impl*)prop;
    struct icalproperty_impl *new = icalproperty_new_impl(old->kind);
    pvl_elem p;

    icalerror_check_arg_rz((prop!=0),"Prop");
    icalerror_check_arg_rz((old!=0),"old");
    icalerror_check_arg_rz((new!=0),"new");

    if (old->value !=0) {
	new->value = icalvalue_new_clone(old->value);
    }

    if (old->x_name != 0) {

	new->x_name = strdup(old->x_name);
	
	if (new->x_name == 0) {
	    icalproperty_free(new);
	    icalerror_set_errno(ICAL_NEWFAILED_ERROR);
	    return 0;
	}
    }

    for(p=pvl_head(old->parameters);p != 0; p = pvl_next(p)){
	icalparameter *param = icalparameter_new_clone(pvl_data(p));
	
	if (param == 0){
	    icalproperty_free(new);
	    icalerror_set_errno(ICAL_NEWFAILED_ERROR);
	    return 0;
	}

	pvl_push(new->parameters,param);
    
    } 

    return new;

}

/* This one works a little differently from the other *_from_string
   routines; the string input is the name of the property, not the
   data associated with the property, as it is in
   icalvalue_from_string. All of the parsing associated with
   properties is driven by routines in icalparse.c */

icalproperty* icalproperty_new_from_string(char* str)
{
    icalproperty_kind kind;

    icalerror_check_arg_rz( (str!=0),"str");

    kind = icalenum_string_to_property_kind(str);

    if (kind == ICAL_NO_PROPERTY){
	
	if( str[0] == 'X' && str[1] == '-'){
	    icalproperty *p = icalproperty_new(ICAL_X_PROPERTY);    
	    icalproperty_set_x_name(p,str);
	    return p;
	} else {
	    icalerror_set_errno(ICAL_MALFORMEDDATA_ERROR);
	    return 0;
	}

    } else {
	return icalproperty_new(kind);
    }
}

void
icalproperty_free (icalproperty* prop)
{
    struct icalproperty_impl *p;

    icalparameter* param;
    
    icalerror_check_arg_re((prop!=0),"prop",ICAL_BADARG_ERROR);

    p = (struct icalproperty_impl*)prop;

#ifdef ICAL_FREE_ON_LIST_IS_ERROR
    icalerror_assert( (p->parent ==0),"Tried to free a property that is still attached to a component. ");
    
#else
    if(p->parent !=0){
	return;
    }
#endif

    if (p->value != 0){
        icalvalue_set_parent(p->value,0);
	icalvalue_free(p->value);
    }
    
    while( (param = pvl_pop(p->parameters)) != 0){
	icalparameter_free(param);
    }
    
    pvl_free(p->parameters);
    
    if (p->x_name != 0) {
	free(p->x_name);
    }
    
    p->kind = ICAL_NO_PROPERTY;
    p->parameters = 0;
    p->parameter_iterator = 0;
    p->value = 0;
    p->x_name = 0;
    p->id[0] = 'X';
    
    free(p);

}


char*
icalproperty_as_ical_string (icalproperty* prop)
{   
    icalparameter *param;

    /* Create new buffer that we can append names, parameters and a
       value to, and reallocate as needed. Later, this buffer will be
       copied to a icalmemory_tmp_buffer, which is managed internally
       by libical, so it can be given to the caller without fear of
       the caller forgetting to free it */

    char* property_name = 0; 
    size_t buf_size = 1024;
    char* buf = icalmemory_new_buffer(buf_size);
    char* buf_ptr = buf;
    icalvalue* value;
    char *out_buf;

    struct icalproperty_impl *impl = (struct icalproperty_impl*)prop;
    
    icalerror_check_arg_rz( (prop!=0),"prop");

    /* Append property name */

    if (impl->kind == ICAL_X_PROPERTY && impl->x_name != 0){
	property_name = impl->x_name;
    } else {
	property_name = icalenum_property_kind_to_string(impl->kind);
    }

    if (property_name == 0 ) {
	icalerror_warn("Got a property of an unknown kind.");
	icalmemory_free_buffer(buf);
	return 0;
	
    }


    icalmemory_append_string(&buf, &buf_ptr, &buf_size, property_name);
    icalmemory_append_string(&buf, &buf_ptr, &buf_size, "\n");

    /* Append parameters */
    for(param = icalproperty_get_first_parameter(prop,ICAL_ANY_PARAMETER);
	param != 0; 
	param = icalproperty_get_next_parameter(prop,ICAL_ANY_PARAMETER)) {

	char* kind_string = icalparameter_as_ical_string(param); 

	if (kind_string == 0 ) {
	    char temp[1024];
	    sprintf(temp, "Got a parameter of unknown kind in %s property",property_name);
	    icalerror_warn(temp);
	    continue;
	}

	icalmemory_append_string(&buf, &buf_ptr, &buf_size, " ;");
    	icalmemory_append_string(&buf, &buf_ptr, &buf_size, kind_string);
 	icalmemory_append_string(&buf, &buf_ptr, &buf_size, "\n");

    }    

    /* Append value */

    icalmemory_append_string(&buf, &buf_ptr, &buf_size, " :");

    value = icalproperty_get_value(prop);

    if (value != 0){
	icalmemory_append_string(&buf, &buf_ptr, &buf_size, 
		      icalvalue_as_ical_string(icalproperty_get_value(prop)));
    } else {
	icalmemory_append_string(&buf, &buf_ptr, &buf_size,"ERROR: No Value"); 
	
    }
    
    icalmemory_append_string(&buf, &buf_ptr, &buf_size, "\n");

    /* Now, copy the buffer to a tmp_buffer, which is safe to give to
       the caller without worring about de-allocating it. */

    
    out_buf = icalmemory_tmp_buffer(strlen(buf)+1);
    strcpy(out_buf, buf);

    icalmemory_free_buffer(buf);

    return out_buf;
}



icalproperty_kind
icalproperty_isa (icalproperty* property)
{
    struct icalproperty_impl *p = (struct icalproperty_impl*)property;

   if(property != 0){
       return p->kind;
   }

   return ICAL_NO_PROPERTY;
}

int
icalproperty_isa_property (void* property)
{
    struct icalproperty_impl *impl = (struct icalproperty_impl*)property;

    icalerror_check_arg_rz( (property!=0), "property");

    if (strcmp(impl->id,"prop") == 0) {
	return 1;
    } else {
	return 0;
    }
}


void
icalproperty_add_parameter (icalproperty* prop,icalparameter* parameter)
{
    struct icalproperty_impl *p = (struct icalproperty_impl*)prop;
    
   icalerror_check_arg_rv( (prop!=0),"prop");
   icalerror_check_arg_rv( (parameter!=0),"parameter");
    
   pvl_push(p->parameters, parameter);

}


void
icalproperty_remove_parameter (icalproperty* prop, icalparameter_kind kind)
{
    icalerror_check_arg_rv((prop!=0),"prop");

    assert(0); /* This routine is not implemented */
}


int
icalproperty_count_parameters (icalproperty* prop)
{
    struct icalproperty_impl *p = (struct icalproperty_impl*)prop;

    if(prop != 0){
	return pvl_count(p->parameters);
    }

    icalerror_set_errno(ICAL_USAGE_ERROR);
    return -1;
}


icalparameter*
icalproperty_get_first_parameter (icalproperty* prop, icalparameter_kind kind)
{
   struct icalproperty_impl *p = (struct icalproperty_impl*)prop;

   icalerror_check_arg_rz( (prop!=0),"prop");
   
   p->parameter_iterator = pvl_head(p->parameters);

   if (p->parameter_iterator == 0) {
       return 0;
   }

   return (icalparameter*) pvl_data(p->parameter_iterator);
}


icalparameter*
icalproperty_get_next_parameter (icalproperty* prop, icalparameter_kind kind)
{
   struct icalproperty_impl *p = (struct icalproperty_impl*)prop;
   icalerror_check_arg_rz( (prop!=0),"prop");

   if (p->parameter_iterator == 0 ) {
       return 0;
   }

   p->parameter_iterator = pvl_next(p->parameter_iterator);

   if (p->parameter_iterator == 0 ) {
       return 0;
   }

   return  (icalparameter*) pvl_data(p->parameter_iterator);
}

void
icalproperty_set_value (icalproperty* prop, icalvalue* value)
{
    struct icalproperty_impl *p = (struct icalproperty_impl*)prop;

    icalerror_check_arg_rv((prop !=0),"prop");
    icalerror_check_arg_rv((value !=0),"value");
    
    if (p->value != 0){
	icalvalue_set_parent(p->value,0);
	icalvalue_free(p->value);
	p->value = 0;
    }

    p->value = value;
    
    icalvalue_set_parent(value,prop);
}


icalvalue*
icalproperty_get_value (icalproperty* prop)
{
    struct icalproperty_impl *p = (struct icalproperty_impl*)prop;
    
    icalerror_check_arg_rz( (prop!=0),"prop");
    
    return p->value;
}


void icalproperty_set_x_name(icalproperty* prop, char* name)
{
    struct icalproperty_impl *impl = (struct icalproperty_impl*)prop;

    icalerror_check_arg_rv( (name!=0),"name");
    icalerror_check_arg_rv( (prop!=0),"prop");

    if (impl->x_name != 0) {
        free(impl->x_name);
    }

    impl->x_name = strdup(name);

    if(impl->x_name == 0){
	icalerror_set_errno(ICAL_ALLOCATION_ERROR);
    }

}
                              
char* icalproperty_get_x_name(icalproperty* prop){

    struct icalproperty_impl *impl = (struct icalproperty_impl*)prop;

    icalerror_check_arg_rz( (prop!=0),"prop");

    return impl->x_name;
}


void icalproperty_set_parent(icalproperty* property,
			     icalcomponent* component)
{
    struct icalproperty_impl *impl = (struct icalproperty_impl*)property;

    icalerror_check_arg_rv( (property!=0),"property");
    
    impl->parent = component;
}

icalcomponent* icalproperty_get_parent(icalproperty* property)
{
    struct icalproperty_impl *impl = (struct icalproperty_impl*)property;
 
    icalerror_check_arg_rv( (property!=0),"property");

    return impl->parent;
}


/* Everything below this line is machine generated. Do not edit. */

/* METHOD */

icalproperty* icalproperty_new_method(icalproperty_method v)
{
   struct icalproperty_impl *impl = icalproperty_new_impl(ICAL_METHOD_PROPERTY);  
   

   icalproperty_set_method((icalproperty*)impl,v);

   return (icalproperty*)impl;
}

icalproperty* icalproperty_vanew_method(icalproperty_method v, ...)
{
   va_list args;
   struct icalproperty_impl *impl = icalproperty_new_impl(ICAL_METHOD_PROPERTY);  
   

   icalproperty_set_method((icalproperty*)impl,v);

   va_start(args,v);
   icalproperty_add_parameters(impl, args);
   va_end(args);

   return (icalproperty*)impl;
}
 
void icalproperty_set_method(icalproperty* prop, icalproperty_method v)
{
    icalvalue *value;
   
    
    icalerror_check_arg_rv( (prop!=0),"prop");

    value = icalvalue_new_method(v);

    icalproperty_set_value(prop,value);

}

icalproperty_method icalproperty_get_method(icalproperty* prop)
{
    icalvalue *value;
    icalerror_check_arg( (prop!=0),"prop");

    value = icalproperty_get_value(prop);

    return icalvalue_get_method(value);
}

/* LAST-MODIFIED */

icalproperty* icalproperty_new_lastmodified(struct icaltimetype v)
{
   struct icalproperty_impl *impl = icalproperty_new_impl(ICAL_LASTMODIFIED_PROPERTY);  
   

   icalproperty_set_lastmodified((icalproperty*)impl,v);

   return (icalproperty*)impl;
}

icalproperty* icalproperty_vanew_lastmodified(struct icaltimetype v, ...)
{
   va_list args;
   struct icalproperty_impl *impl = icalproperty_new_impl(ICAL_LASTMODIFIED_PROPERTY);  
   

   icalproperty_set_lastmodified((icalproperty*)impl,v);

   va_start(args,v);
   icalproperty_add_parameters(impl, args);
   va_end(args);

   return (icalproperty*)impl;
}
 
void icalproperty_set_lastmodified(icalproperty* prop, struct icaltimetype v)
{
    icalvalue *value;
   
    
    icalerror_check_arg_rv( (prop!=0),"prop");

    value = icalvalue_new_datetime(v);

    icalproperty_set_value(prop,value);

}

struct icaltimetype icalproperty_get_lastmodified(icalproperty* prop)
{
    icalvalue *value;
    icalerror_check_arg( (prop!=0),"prop");

    value = icalproperty_get_value(prop);

    return icalvalue_get_datetime(value);
}

/* UID */

icalproperty* icalproperty_new_uid(char* v)
{
   struct icalproperty_impl *impl = icalproperty_new_impl(ICAL_UID_PROPERTY);  
   icalerror_check_arg_rz( (v!=0),"v");


   icalproperty_set_uid((icalproperty*)impl,v);

   return (icalproperty*)impl;
}

icalproperty* icalproperty_vanew_uid(char* v, ...)
{
   va_list args;
   struct icalproperty_impl *impl = icalproperty_new_impl(ICAL_UID_PROPERTY);  
   icalerror_check_arg_rz( (v!=0),"v");


   icalproperty_set_uid((icalproperty*)impl,v);

   va_start(args,v);
   icalproperty_add_parameters(impl, args);
   va_end(args);

   return (icalproperty*)impl;
}
 
void icalproperty_set_uid(icalproperty* prop, char* v)
{
    icalvalue *value;
   
    icalerror_check_arg_rv( (v!=0),"v");

    icalerror_check_arg_rv( (prop!=0),"prop");

    value = icalvalue_new_text(v);

    icalproperty_set_value(prop,value);

}

char* icalproperty_get_uid(icalproperty* prop)
{
    icalvalue *value;
    icalerror_check_arg( (prop!=0),"prop");

    value = icalproperty_get_value(prop);

    return icalvalue_get_text(value);
}

/* PRODID */

icalproperty* icalproperty_new_prodid(char* v)
{
   struct icalproperty_impl *impl = icalproperty_new_impl(ICAL_PRODID_PROPERTY);  
   icalerror_check_arg_rz( (v!=0),"v");


   icalproperty_set_prodid((icalproperty*)impl,v);

   return (icalproperty*)impl;
}

icalproperty* icalproperty_vanew_prodid(char* v, ...)
{
   va_list args;
   struct icalproperty_impl *impl = icalproperty_new_impl(ICAL_PRODID_PROPERTY);  
   icalerror_check_arg_rz( (v!=0),"v");


   icalproperty_set_prodid((icalproperty*)impl,v);

   va_start(args,v);
   icalproperty_add_parameters(impl, args);
   va_end(args);

   return (icalproperty*)impl;
}
 
void icalproperty_set_prodid(icalproperty* prop, char* v)
{
    icalvalue *value;
   
    icalerror_check_arg_rv( (v!=0),"v");

    icalerror_check_arg_rv( (prop!=0),"prop");

    value = icalvalue_new_text(v);

    icalproperty_set_value(prop,value);

}

char* icalproperty_get_prodid(icalproperty* prop)
{
    icalvalue *value;
    icalerror_check_arg( (prop!=0),"prop");

    value = icalproperty_get_value(prop);

    return icalvalue_get_text(value);
}

/* STATUS */

icalproperty* icalproperty_new_status(char* v)
{
   struct icalproperty_impl *impl = icalproperty_new_impl(ICAL_STATUS_PROPERTY);  
   icalerror_check_arg_rz( (v!=0),"v");


   icalproperty_set_status((icalproperty*)impl,v);

   return (icalproperty*)impl;
}

icalproperty* icalproperty_vanew_status(char* v, ...)
{
   va_list args;
   struct icalproperty_impl *impl = icalproperty_new_impl(ICAL_STATUS_PROPERTY);  
   icalerror_check_arg_rz( (v!=0),"v");


   icalproperty_set_status((icalproperty*)impl,v);

   va_start(args,v);
   icalproperty_add_parameters(impl, args);
   va_end(args);

   return (icalproperty*)impl;
}
 
void icalproperty_set_status(icalproperty* prop, char* v)
{
    icalvalue *value;
   
    icalerror_check_arg_rv( (v!=0),"v");

    icalerror_check_arg_rv( (prop!=0),"prop");

    value = icalvalue_new_text(v);

    icalproperty_set_value(prop,value);

}

char* icalproperty_get_status(icalproperty* prop)
{
    icalvalue *value;
    icalerror_check_arg( (prop!=0),"prop");

    value = icalproperty_get_value(prop);

    return icalvalue_get_text(value);
}

/* DESCRIPTION */

icalproperty* icalproperty_new_description(char* v)
{
   struct icalproperty_impl *impl = icalproperty_new_impl(ICAL_DESCRIPTION_PROPERTY);  
   icalerror_check_arg_rz( (v!=0),"v");


   icalproperty_set_description((icalproperty*)impl,v);

   return (icalproperty*)impl;
}

icalproperty* icalproperty_vanew_description(char* v, ...)
{
   va_list args;
   struct icalproperty_impl *impl = icalproperty_new_impl(ICAL_DESCRIPTION_PROPERTY);  
   icalerror_check_arg_rz( (v!=0),"v");


   icalproperty_set_description((icalproperty*)impl,v);

   va_start(args,v);
   icalproperty_add_parameters(impl, args);
   va_end(args);

   return (icalproperty*)impl;
}
 
void icalproperty_set_description(icalproperty* prop, char* v)
{
    icalvalue *value;
   
    icalerror_check_arg_rv( (v!=0),"v");

    icalerror_check_arg_rv( (prop!=0),"prop");

    value = icalvalue_new_text(v);

    icalproperty_set_value(prop,value);

}

char* icalproperty_get_description(icalproperty* prop)
{
    icalvalue *value;
    icalerror_check_arg( (prop!=0),"prop");

    value = icalproperty_get_value(prop);

    return icalvalue_get_text(value);
}

/* DURATION */

icalproperty* icalproperty_new_duration(struct icaldurationtype v)
{
   struct icalproperty_impl *impl = icalproperty_new_impl(ICAL_DURATION_PROPERTY);  
   

   icalproperty_set_duration((icalproperty*)impl,v);

   return (icalproperty*)impl;
}

icalproperty* icalproperty_vanew_duration(struct icaldurationtype v, ...)
{
   va_list args;
   struct icalproperty_impl *impl = icalproperty_new_impl(ICAL_DURATION_PROPERTY);  
   

   icalproperty_set_duration((icalproperty*)impl,v);

   va_start(args,v);
   icalproperty_add_parameters(impl, args);
   va_end(args);

   return (icalproperty*)impl;
}
 
void icalproperty_set_duration(icalproperty* prop, struct icaldurationtype v)
{
    icalvalue *value;
   
    
    icalerror_check_arg_rv( (prop!=0),"prop");

    value = icalvalue_new_duration(v);

    icalproperty_set_value(prop,value);

}

struct icaldurationtype icalproperty_get_duration(icalproperty* prop)
{
    icalvalue *value;
    icalerror_check_arg( (prop!=0),"prop");

    value = icalproperty_get_value(prop);

    return icalvalue_get_duration(value);
}

/* CATEGORIES */

icalproperty* icalproperty_new_categories(char* v)
{
   struct icalproperty_impl *impl = icalproperty_new_impl(ICAL_CATEGORIES_PROPERTY);  
   icalerror_check_arg_rz( (v!=0),"v");


   icalproperty_set_categories((icalproperty*)impl,v);

   return (icalproperty*)impl;
}

icalproperty* icalproperty_vanew_categories(char* v, ...)
{
   va_list args;
   struct icalproperty_impl *impl = icalproperty_new_impl(ICAL_CATEGORIES_PROPERTY);  
   icalerror_check_arg_rz( (v!=0),"v");


   icalproperty_set_categories((icalproperty*)impl,v);

   va_start(args,v);
   icalproperty_add_parameters(impl, args);
   va_end(args);

   return (icalproperty*)impl;
}
 
void icalproperty_set_categories(icalproperty* prop, char* v)
{
    icalvalue *value;
   
    icalerror_check_arg_rv( (v!=0),"v");

    icalerror_check_arg_rv( (prop!=0),"prop");

    value = icalvalue_new_text(v);

    icalproperty_set_value(prop,value);

}

char* icalproperty_get_categories(icalproperty* prop)
{
    icalvalue *value;
    icalerror_check_arg( (prop!=0),"prop");

    value = icalproperty_get_value(prop);

    return icalvalue_get_text(value);
}

/* VERSION */

icalproperty* icalproperty_new_version(char* v)
{
   struct icalproperty_impl *impl = icalproperty_new_impl(ICAL_VERSION_PROPERTY);  
   icalerror_check_arg_rz( (v!=0),"v");


   icalproperty_set_version((icalproperty*)impl,v);

   return (icalproperty*)impl;
}

icalproperty* icalproperty_vanew_version(char* v, ...)
{
   va_list args;
   struct icalproperty_impl *impl = icalproperty_new_impl(ICAL_VERSION_PROPERTY);  
   icalerror_check_arg_rz( (v!=0),"v");


   icalproperty_set_version((icalproperty*)impl,v);

   va_start(args,v);
   icalproperty_add_parameters(impl, args);
   va_end(args);

   return (icalproperty*)impl;
}
 
void icalproperty_set_version(icalproperty* prop, char* v)
{
    icalvalue *value;
   
    icalerror_check_arg_rv( (v!=0),"v");

    icalerror_check_arg_rv( (prop!=0),"prop");

    value = icalvalue_new_text(v);

    icalproperty_set_value(prop,value);

}

char* icalproperty_get_version(icalproperty* prop)
{
    icalvalue *value;
    icalerror_check_arg( (prop!=0),"prop");

    value = icalproperty_get_value(prop);

    return icalvalue_get_text(value);
}

/* TZOFFSETFROM */

icalproperty* icalproperty_new_tzoffsetfrom(int v)
{
   struct icalproperty_impl *impl = icalproperty_new_impl(ICAL_TZOFFSETFROM_PROPERTY);  
   

   icalproperty_set_tzoffsetfrom((icalproperty*)impl,v);

   return (icalproperty*)impl;
}

icalproperty* icalproperty_vanew_tzoffsetfrom(int v, ...)
{
   va_list args;
   struct icalproperty_impl *impl = icalproperty_new_impl(ICAL_TZOFFSETFROM_PROPERTY);  
   

   icalproperty_set_tzoffsetfrom((icalproperty*)impl,v);

   va_start(args,v);
   icalproperty_add_parameters(impl, args);
   va_end(args);

   return (icalproperty*)impl;
}
 
void icalproperty_set_tzoffsetfrom(icalproperty* prop, int v)
{
    icalvalue *value;
   
    
    icalerror_check_arg_rv( (prop!=0),"prop");

    value = icalvalue_new_utcoffset(v);

    icalproperty_set_value(prop,value);

}

int icalproperty_get_tzoffsetfrom(icalproperty* prop)
{
    icalvalue *value;
    icalerror_check_arg( (prop!=0),"prop");

    value = icalproperty_get_value(prop);

    return icalvalue_get_utcoffset(value);
}

/* RRULE */

icalproperty* icalproperty_new_rrule(struct icalrecurrencetype v)
{
   struct icalproperty_impl *impl = icalproperty_new_impl(ICAL_RRULE_PROPERTY);  
   

   icalproperty_set_rrule((icalproperty*)impl,v);

   return (icalproperty*)impl;
}

icalproperty* icalproperty_vanew_rrule(struct icalrecurrencetype v, ...)
{
   va_list args;
   struct icalproperty_impl *impl = icalproperty_new_impl(ICAL_RRULE_PROPERTY);  
   

   icalproperty_set_rrule((icalproperty*)impl,v);

   va_start(args,v);
   icalproperty_add_parameters(impl, args);
   va_end(args);

   return (icalproperty*)impl;
}
 
void icalproperty_set_rrule(icalproperty* prop, struct icalrecurrencetype v)
{
    icalvalue *value;
   
    
    icalerror_check_arg_rv( (prop!=0),"prop");

    value = icalvalue_new_recur(v);

    icalproperty_set_value(prop,value);

}

struct icalrecurrencetype icalproperty_get_rrule(icalproperty* prop)
{
    icalvalue *value;
    icalerror_check_arg( (prop!=0),"prop");

    value = icalproperty_get_value(prop);

    return icalvalue_get_recur(value);
}

/* ATTENDEE */

icalproperty* icalproperty_new_attendee(char* v)
{
   struct icalproperty_impl *impl = icalproperty_new_impl(ICAL_ATTENDEE_PROPERTY);  
   icalerror_check_arg_rz( (v!=0),"v");


   icalproperty_set_attendee((icalproperty*)impl,v);

   return (icalproperty*)impl;
}

icalproperty* icalproperty_vanew_attendee(char* v, ...)
{
   va_list args;
   struct icalproperty_impl *impl = icalproperty_new_impl(ICAL_ATTENDEE_PROPERTY);  
   icalerror_check_arg_rz( (v!=0),"v");


   icalproperty_set_attendee((icalproperty*)impl,v);

   va_start(args,v);
   icalproperty_add_parameters(impl, args);
   va_end(args);

   return (icalproperty*)impl;
}
 
void icalproperty_set_attendee(icalproperty* prop, char* v)
{
    icalvalue *value;
   
    icalerror_check_arg_rv( (v!=0),"v");

    icalerror_check_arg_rv( (prop!=0),"prop");

    value = icalvalue_new_caladdress(v);

    icalproperty_set_value(prop,value);

}

char* icalproperty_get_attendee(icalproperty* prop)
{
    icalvalue *value;
    icalerror_check_arg( (prop!=0),"prop");

    value = icalproperty_get_value(prop);

    return icalvalue_get_caladdress(value);
}

/* CONTACT */

icalproperty* icalproperty_new_contact(char* v)
{
   struct icalproperty_impl *impl = icalproperty_new_impl(ICAL_CONTACT_PROPERTY);  
   icalerror_check_arg_rz( (v!=0),"v");


   icalproperty_set_contact((icalproperty*)impl,v);

   return (icalproperty*)impl;
}

icalproperty* icalproperty_vanew_contact(char* v, ...)
{
   va_list args;
   struct icalproperty_impl *impl = icalproperty_new_impl(ICAL_CONTACT_PROPERTY);  
   icalerror_check_arg_rz( (v!=0),"v");


   icalproperty_set_contact((icalproperty*)impl,v);

   va_start(args,v);
   icalproperty_add_parameters(impl, args);
   va_end(args);

   return (icalproperty*)impl;
}
 
void icalproperty_set_contact(icalproperty* prop, char* v)
{
    icalvalue *value;
   
    icalerror_check_arg_rv( (v!=0),"v");

    icalerror_check_arg_rv( (prop!=0),"prop");

    value = icalvalue_new_text(v);

    icalproperty_set_value(prop,value);

}

char* icalproperty_get_contact(icalproperty* prop)
{
    icalvalue *value;
    icalerror_check_arg( (prop!=0),"prop");

    value = icalproperty_get_value(prop);

    return icalvalue_get_text(value);
}

/* RELATED-TO */

icalproperty* icalproperty_new_relatedto(char* v)
{
   struct icalproperty_impl *impl = icalproperty_new_impl(ICAL_RELATEDTO_PROPERTY);  
   icalerror_check_arg_rz( (v!=0),"v");


   icalproperty_set_relatedto((icalproperty*)impl,v);

   return (icalproperty*)impl;
}

icalproperty* icalproperty_vanew_relatedto(char* v, ...)
{
   va_list args;
   struct icalproperty_impl *impl = icalproperty_new_impl(ICAL_RELATEDTO_PROPERTY);  
   icalerror_check_arg_rz( (v!=0),"v");


   icalproperty_set_relatedto((icalproperty*)impl,v);

   va_start(args,v);
   icalproperty_add_parameters(impl, args);
   va_end(args);

   return (icalproperty*)impl;
}
 
void icalproperty_set_relatedto(icalproperty* prop, char* v)
{
    icalvalue *value;
   
    icalerror_check_arg_rv( (v!=0),"v");

    icalerror_check_arg_rv( (prop!=0),"prop");

    value = icalvalue_new_text(v);

    icalproperty_set_value(prop,value);

}

char* icalproperty_get_relatedto(icalproperty* prop)
{
    icalvalue *value;
    icalerror_check_arg( (prop!=0),"prop");

    value = icalproperty_get_value(prop);

    return icalvalue_get_text(value);
}

/* ORGANIZER */

icalproperty* icalproperty_new_organizer(char* v)
{
   struct icalproperty_impl *impl = icalproperty_new_impl(ICAL_ORGANIZER_PROPERTY);  
   icalerror_check_arg_rz( (v!=0),"v");


   icalproperty_set_organizer((icalproperty*)impl,v);

   return (icalproperty*)impl;
}

icalproperty* icalproperty_vanew_organizer(char* v, ...)
{
   va_list args;
   struct icalproperty_impl *impl = icalproperty_new_impl(ICAL_ORGANIZER_PROPERTY);  
   icalerror_check_arg_rz( (v!=0),"v");


   icalproperty_set_organizer((icalproperty*)impl,v);

   va_start(args,v);
   icalproperty_add_parameters(impl, args);
   va_end(args);

   return (icalproperty*)impl;
}
 
void icalproperty_set_organizer(icalproperty* prop, char* v)
{
    icalvalue *value;
   
    icalerror_check_arg_rv( (v!=0),"v");

    icalerror_check_arg_rv( (prop!=0),"prop");

    value = icalvalue_new_caladdress(v);

    icalproperty_set_value(prop,value);

}

char* icalproperty_get_organizer(icalproperty* prop)
{
    icalvalue *value;
    icalerror_check_arg( (prop!=0),"prop");

    value = icalproperty_get_value(prop);

    return icalvalue_get_caladdress(value);
}

/* COMMENT */

icalproperty* icalproperty_new_comment(char* v)
{
   struct icalproperty_impl *impl = icalproperty_new_impl(ICAL_COMMENT_PROPERTY);  
   icalerror_check_arg_rz( (v!=0),"v");


   icalproperty_set_comment((icalproperty*)impl,v);

   return (icalproperty*)impl;
}

icalproperty* icalproperty_vanew_comment(char* v, ...)
{
   va_list args;
   struct icalproperty_impl *impl = icalproperty_new_impl(ICAL_COMMENT_PROPERTY);  
   icalerror_check_arg_rz( (v!=0),"v");


   icalproperty_set_comment((icalproperty*)impl,v);

   va_start(args,v);
   icalproperty_add_parameters(impl, args);
   va_end(args);

   return (icalproperty*)impl;
}
 
void icalproperty_set_comment(icalproperty* prop, char* v)
{
    icalvalue *value;
   
    icalerror_check_arg_rv( (v!=0),"v");

    icalerror_check_arg_rv( (prop!=0),"prop");

    value = icalvalue_new_text(v);

    icalproperty_set_value(prop,value);

}

char* icalproperty_get_comment(icalproperty* prop)
{
    icalvalue *value;
    icalerror_check_arg( (prop!=0),"prop");

    value = icalproperty_get_value(prop);

    return icalvalue_get_text(value);
}

/* TRIGGER */

icalproperty* icalproperty_new_trigger(union icaltriggertype v)
{
   struct icalproperty_impl *impl = icalproperty_new_impl(ICAL_TRIGGER_PROPERTY);  
   

   icalproperty_set_trigger((icalproperty*)impl,v);

   return (icalproperty*)impl;
}

icalproperty* icalproperty_vanew_trigger(union icaltriggertype v, ...)
{
   va_list args;
   struct icalproperty_impl *impl = icalproperty_new_impl(ICAL_TRIGGER_PROPERTY);  
   

   icalproperty_set_trigger((icalproperty*)impl,v);

   va_start(args,v);
   icalproperty_add_parameters(impl, args);
   va_end(args);

   return (icalproperty*)impl;
}
 
void icalproperty_set_trigger(icalproperty* prop, union icaltriggertype v)
{
    icalvalue *value;
   
    
    icalerror_check_arg_rv( (prop!=0),"prop");

    value = icalvalue_new_trigger(v);

    icalproperty_set_value(prop,value);

}

union icaltriggertype icalproperty_get_trigger(icalproperty* prop)
{
    icalvalue *value;
    icalerror_check_arg( (prop!=0),"prop");

    value = icalproperty_get_value(prop);

    return icalvalue_get_trigger(value);
}

/* X-LIC-ERROR */

icalproperty* icalproperty_new_xlicerror(char* v)
{
   struct icalproperty_impl *impl = icalproperty_new_impl(ICAL_XLICERROR_PROPERTY);  
   icalerror_check_arg_rz( (v!=0),"v");


   icalproperty_set_xlicerror((icalproperty*)impl,v);

   return (icalproperty*)impl;
}

icalproperty* icalproperty_vanew_xlicerror(char* v, ...)
{
   va_list args;
   struct icalproperty_impl *impl = icalproperty_new_impl(ICAL_XLICERROR_PROPERTY);  
   icalerror_check_arg_rz( (v!=0),"v");


   icalproperty_set_xlicerror((icalproperty*)impl,v);

   va_start(args,v);
   icalproperty_add_parameters(impl, args);
   va_end(args);

   return (icalproperty*)impl;
}
 
void icalproperty_set_xlicerror(icalproperty* prop, char* v)
{
    icalvalue *value;
   
    icalerror_check_arg_rv( (v!=0),"v");

    icalerror_check_arg_rv( (prop!=0),"prop");

    value = icalvalue_new_text(v);

    icalproperty_set_value(prop,value);

}

char* icalproperty_get_xlicerror(icalproperty* prop)
{
    icalvalue *value;
    icalerror_check_arg( (prop!=0),"prop");

    value = icalproperty_get_value(prop);

    return icalvalue_get_text(value);
}

/* CLASS */

icalproperty* icalproperty_new_class(char* v)
{
   struct icalproperty_impl *impl = icalproperty_new_impl(ICAL_CLASS_PROPERTY);  
   icalerror_check_arg_rz( (v!=0),"v");


   icalproperty_set_class((icalproperty*)impl,v);

   return (icalproperty*)impl;
}

icalproperty* icalproperty_vanew_class(char* v, ...)
{
   va_list args;
   struct icalproperty_impl *impl = icalproperty_new_impl(ICAL_CLASS_PROPERTY);  
   icalerror_check_arg_rz( (v!=0),"v");


   icalproperty_set_class((icalproperty*)impl,v);

   va_start(args,v);
   icalproperty_add_parameters(impl, args);
   va_end(args);

   return (icalproperty*)impl;
}
 
void icalproperty_set_class(icalproperty* prop, char* v)
{
    icalvalue *value;
   
    icalerror_check_arg_rv( (v!=0),"v");

    icalerror_check_arg_rv( (prop!=0),"prop");

    value = icalvalue_new_text(v);

    icalproperty_set_value(prop,value);

}

char* icalproperty_get_class(icalproperty* prop)
{
    icalvalue *value;
    icalerror_check_arg( (prop!=0),"prop");

    value = icalproperty_get_value(prop);

    return icalvalue_get_text(value);
}

/* TZOFFSETTO */

icalproperty* icalproperty_new_tzoffsetto(int v)
{
   struct icalproperty_impl *impl = icalproperty_new_impl(ICAL_TZOFFSETTO_PROPERTY);  
   

   icalproperty_set_tzoffsetto((icalproperty*)impl,v);

   return (icalproperty*)impl;
}

icalproperty* icalproperty_vanew_tzoffsetto(int v, ...)
{
   va_list args;
   struct icalproperty_impl *impl = icalproperty_new_impl(ICAL_TZOFFSETTO_PROPERTY);  
   

   icalproperty_set_tzoffsetto((icalproperty*)impl,v);

   va_start(args,v);
   icalproperty_add_parameters(impl, args);
   va_end(args);

   return (icalproperty*)impl;
}
 
void icalproperty_set_tzoffsetto(icalproperty* prop, int v)
{
    icalvalue *value;
   
    
    icalerror_check_arg_rv( (prop!=0),"prop");

    value = icalvalue_new_utcoffset(v);

    icalproperty_set_value(prop,value);

}

int icalproperty_get_tzoffsetto(icalproperty* prop)
{
    icalvalue *value;
    icalerror_check_arg( (prop!=0),"prop");

    value = icalproperty_get_value(prop);

    return icalvalue_get_utcoffset(value);
}

/* TRANSP */

icalproperty* icalproperty_new_transp(char* v)
{
   struct icalproperty_impl *impl = icalproperty_new_impl(ICAL_TRANSP_PROPERTY);  
   icalerror_check_arg_rz( (v!=0),"v");


   icalproperty_set_transp((icalproperty*)impl,v);

   return (icalproperty*)impl;
}

icalproperty* icalproperty_vanew_transp(char* v, ...)
{
   va_list args;
   struct icalproperty_impl *impl = icalproperty_new_impl(ICAL_TRANSP_PROPERTY);  
   icalerror_check_arg_rz( (v!=0),"v");


   icalproperty_set_transp((icalproperty*)impl,v);

   va_start(args,v);
   icalproperty_add_parameters(impl, args);
   va_end(args);

   return (icalproperty*)impl;
}
 
void icalproperty_set_transp(icalproperty* prop, char* v)
{
    icalvalue *value;
   
    icalerror_check_arg_rv( (v!=0),"v");

    icalerror_check_arg_rv( (prop!=0),"prop");

    value = icalvalue_new_text(v);

    icalproperty_set_value(prop,value);

}

char* icalproperty_get_transp(icalproperty* prop)
{
    icalvalue *value;
    icalerror_check_arg( (prop!=0),"prop");

    value = icalproperty_get_value(prop);

    return icalvalue_get_text(value);
}

/* SEQUENCE */

icalproperty* icalproperty_new_sequence(int v)
{
   struct icalproperty_impl *impl = icalproperty_new_impl(ICAL_SEQUENCE_PROPERTY);  
   

   icalproperty_set_sequence((icalproperty*)impl,v);

   return (icalproperty*)impl;
}

icalproperty* icalproperty_vanew_sequence(int v, ...)
{
   va_list args;
   struct icalproperty_impl *impl = icalproperty_new_impl(ICAL_SEQUENCE_PROPERTY);  
   

   icalproperty_set_sequence((icalproperty*)impl,v);

   va_start(args,v);
   icalproperty_add_parameters(impl, args);
   va_end(args);

   return (icalproperty*)impl;
}
 
void icalproperty_set_sequence(icalproperty* prop, int v)
{
    icalvalue *value;
   
    
    icalerror_check_arg_rv( (prop!=0),"prop");

    value = icalvalue_new_integer(v);

    icalproperty_set_value(prop,value);

}

int icalproperty_get_sequence(icalproperty* prop)
{
    icalvalue *value;
    icalerror_check_arg( (prop!=0),"prop");

    value = icalproperty_get_value(prop);

    return icalvalue_get_integer(value);
}

/* LOCATION */

icalproperty* icalproperty_new_location(char* v)
{
   struct icalproperty_impl *impl = icalproperty_new_impl(ICAL_LOCATION_PROPERTY);  
   icalerror_check_arg_rz( (v!=0),"v");


   icalproperty_set_location((icalproperty*)impl,v);

   return (icalproperty*)impl;
}

icalproperty* icalproperty_vanew_location(char* v, ...)
{
   va_list args;
   struct icalproperty_impl *impl = icalproperty_new_impl(ICAL_LOCATION_PROPERTY);  
   icalerror_check_arg_rz( (v!=0),"v");


   icalproperty_set_location((icalproperty*)impl,v);

   va_start(args,v);
   icalproperty_add_parameters(impl, args);
   va_end(args);

   return (icalproperty*)impl;
}
 
void icalproperty_set_location(icalproperty* prop, char* v)
{
    icalvalue *value;
   
    icalerror_check_arg_rv( (v!=0),"v");

    icalerror_check_arg_rv( (prop!=0),"prop");

    value = icalvalue_new_text(v);

    icalproperty_set_value(prop,value);

}

char* icalproperty_get_location(icalproperty* prop)
{
    icalvalue *value;
    icalerror_check_arg( (prop!=0),"prop");

    value = icalproperty_get_value(prop);

    return icalvalue_get_text(value);
}

/* REQUEST-STATUS */

icalproperty* icalproperty_new_requeststatus(char* v)
{
   struct icalproperty_impl *impl = icalproperty_new_impl(ICAL_REQUESTSTATUS_PROPERTY);  
   icalerror_check_arg_rz( (v!=0),"v");


   icalproperty_set_requeststatus((icalproperty*)impl,v);

   return (icalproperty*)impl;
}

icalproperty* icalproperty_vanew_requeststatus(char* v, ...)
{
   va_list args;
   struct icalproperty_impl *impl = icalproperty_new_impl(ICAL_REQUESTSTATUS_PROPERTY);  
   icalerror_check_arg_rz( (v!=0),"v");


   icalproperty_set_requeststatus((icalproperty*)impl,v);

   va_start(args,v);
   icalproperty_add_parameters(impl, args);
   va_end(args);

   return (icalproperty*)impl;
}
 
void icalproperty_set_requeststatus(icalproperty* prop, char* v)
{
    icalvalue *value;
   
    icalerror_check_arg_rv( (v!=0),"v");

    icalerror_check_arg_rv( (prop!=0),"prop");

    value = icalvalue_new_text(v);

    icalproperty_set_value(prop,value);

}

char* icalproperty_get_requeststatus(icalproperty* prop)
{
    icalvalue *value;
    icalerror_check_arg( (prop!=0),"prop");

    value = icalproperty_get_value(prop);

    return icalvalue_get_text(value);
}

/* EXDATE */

icalproperty* icalproperty_new_exdate(struct icaltimetype v)
{
   struct icalproperty_impl *impl = icalproperty_new_impl(ICAL_EXDATE_PROPERTY);  
   

   icalproperty_set_exdate((icalproperty*)impl,v);

   return (icalproperty*)impl;
}

icalproperty* icalproperty_vanew_exdate(struct icaltimetype v, ...)
{
   va_list args;
   struct icalproperty_impl *impl = icalproperty_new_impl(ICAL_EXDATE_PROPERTY);  
   

   icalproperty_set_exdate((icalproperty*)impl,v);

   va_start(args,v);
   icalproperty_add_parameters(impl, args);
   va_end(args);

   return (icalproperty*)impl;
}
 
void icalproperty_set_exdate(icalproperty* prop, struct icaltimetype v)
{
    icalvalue *value;
   
    
    icalerror_check_arg_rv( (prop!=0),"prop");

    value = icalvalue_new_datetimedate(v);

    icalproperty_set_value(prop,value);

}

struct icaltimetype icalproperty_get_exdate(icalproperty* prop)
{
    icalvalue *value;
    icalerror_check_arg( (prop!=0),"prop");

    value = icalproperty_get_value(prop);

    return icalvalue_get_datetimedate(value);
}

/* TZID */

icalproperty* icalproperty_new_tzid(char* v)
{
   struct icalproperty_impl *impl = icalproperty_new_impl(ICAL_TZID_PROPERTY);  
   icalerror_check_arg_rz( (v!=0),"v");


   icalproperty_set_tzid((icalproperty*)impl,v);

   return (icalproperty*)impl;
}

icalproperty* icalproperty_vanew_tzid(char* v, ...)
{
   va_list args;
   struct icalproperty_impl *impl = icalproperty_new_impl(ICAL_TZID_PROPERTY);  
   icalerror_check_arg_rz( (v!=0),"v");


   icalproperty_set_tzid((icalproperty*)impl,v);

   va_start(args,v);
   icalproperty_add_parameters(impl, args);
   va_end(args);

   return (icalproperty*)impl;
}
 
void icalproperty_set_tzid(icalproperty* prop, char* v)
{
    icalvalue *value;
   
    icalerror_check_arg_rv( (v!=0),"v");

    icalerror_check_arg_rv( (prop!=0),"prop");

    value = icalvalue_new_text(v);

    icalproperty_set_value(prop,value);

}

char* icalproperty_get_tzid(icalproperty* prop)
{
    icalvalue *value;
    icalerror_check_arg( (prop!=0),"prop");

    value = icalproperty_get_value(prop);

    return icalvalue_get_text(value);
}

/* RESOURCES */

icalproperty* icalproperty_new_resources(char* v)
{
   struct icalproperty_impl *impl = icalproperty_new_impl(ICAL_RESOURCES_PROPERTY);  
   icalerror_check_arg_rz( (v!=0),"v");


   icalproperty_set_resources((icalproperty*)impl,v);

   return (icalproperty*)impl;
}

icalproperty* icalproperty_vanew_resources(char* v, ...)
{
   va_list args;
   struct icalproperty_impl *impl = icalproperty_new_impl(ICAL_RESOURCES_PROPERTY);  
   icalerror_check_arg_rz( (v!=0),"v");


   icalproperty_set_resources((icalproperty*)impl,v);

   va_start(args,v);
   icalproperty_add_parameters(impl, args);
   va_end(args);

   return (icalproperty*)impl;
}
 
void icalproperty_set_resources(icalproperty* prop, char* v)
{
    icalvalue *value;
   
    icalerror_check_arg_rv( (v!=0),"v");

    icalerror_check_arg_rv( (prop!=0),"prop");

    value = icalvalue_new_text(v);

    icalproperty_set_value(prop,value);

}

char* icalproperty_get_resources(icalproperty* prop)
{
    icalvalue *value;
    icalerror_check_arg( (prop!=0),"prop");

    value = icalproperty_get_value(prop);

    return icalvalue_get_text(value);
}

/* TZURL */

icalproperty* icalproperty_new_tzurl(char* v)
{
   struct icalproperty_impl *impl = icalproperty_new_impl(ICAL_TZURL_PROPERTY);  
   icalerror_check_arg_rz( (v!=0),"v");


   icalproperty_set_tzurl((icalproperty*)impl,v);

   return (icalproperty*)impl;
}

icalproperty* icalproperty_vanew_tzurl(char* v, ...)
{
   va_list args;
   struct icalproperty_impl *impl = icalproperty_new_impl(ICAL_TZURL_PROPERTY);  
   icalerror_check_arg_rz( (v!=0),"v");


   icalproperty_set_tzurl((icalproperty*)impl,v);

   va_start(args,v);
   icalproperty_add_parameters(impl, args);
   va_end(args);

   return (icalproperty*)impl;
}
 
void icalproperty_set_tzurl(icalproperty* prop, char* v)
{
    icalvalue *value;
   
    icalerror_check_arg_rv( (v!=0),"v");

    icalerror_check_arg_rv( (prop!=0),"prop");

    value = icalvalue_new_uri(v);

    icalproperty_set_value(prop,value);

}

char* icalproperty_get_tzurl(icalproperty* prop)
{
    icalvalue *value;
    icalerror_check_arg( (prop!=0),"prop");

    value = icalproperty_get_value(prop);

    return icalvalue_get_uri(value);
}

/* REPEAT */

icalproperty* icalproperty_new_repeat(int v)
{
   struct icalproperty_impl *impl = icalproperty_new_impl(ICAL_REPEAT_PROPERTY);  
   

   icalproperty_set_repeat((icalproperty*)impl,v);

   return (icalproperty*)impl;
}

icalproperty* icalproperty_vanew_repeat(int v, ...)
{
   va_list args;
   struct icalproperty_impl *impl = icalproperty_new_impl(ICAL_REPEAT_PROPERTY);  
   

   icalproperty_set_repeat((icalproperty*)impl,v);

   va_start(args,v);
   icalproperty_add_parameters(impl, args);
   va_end(args);

   return (icalproperty*)impl;
}
 
void icalproperty_set_repeat(icalproperty* prop, int v)
{
    icalvalue *value;
   
    
    icalerror_check_arg_rv( (prop!=0),"prop");

    value = icalvalue_new_integer(v);

    icalproperty_set_value(prop,value);

}

int icalproperty_get_repeat(icalproperty* prop)
{
    icalvalue *value;
    icalerror_check_arg( (prop!=0),"prop");

    value = icalproperty_get_value(prop);

    return icalvalue_get_integer(value);
}

/* PRIORITY */

icalproperty* icalproperty_new_priority(int v)
{
   struct icalproperty_impl *impl = icalproperty_new_impl(ICAL_PRIORITY_PROPERTY);  
   

   icalproperty_set_priority((icalproperty*)impl,v);

   return (icalproperty*)impl;
}

icalproperty* icalproperty_vanew_priority(int v, ...)
{
   va_list args;
   struct icalproperty_impl *impl = icalproperty_new_impl(ICAL_PRIORITY_PROPERTY);  
   

   icalproperty_set_priority((icalproperty*)impl,v);

   va_start(args,v);
   icalproperty_add_parameters(impl, args);
   va_end(args);

   return (icalproperty*)impl;
}
 
void icalproperty_set_priority(icalproperty* prop, int v)
{
    icalvalue *value;
   
    
    icalerror_check_arg_rv( (prop!=0),"prop");

    value = icalvalue_new_integer(v);

    icalproperty_set_value(prop,value);

}

int icalproperty_get_priority(icalproperty* prop)
{
    icalvalue *value;
    icalerror_check_arg( (prop!=0),"prop");

    value = icalproperty_get_value(prop);

    return icalvalue_get_integer(value);
}

/* FREEBUSY */

icalproperty* icalproperty_new_freebusy(struct icalperiodtype v)
{
   struct icalproperty_impl *impl = icalproperty_new_impl(ICAL_FREEBUSY_PROPERTY);  
   

   icalproperty_set_freebusy((icalproperty*)impl,v);

   return (icalproperty*)impl;
}

icalproperty* icalproperty_vanew_freebusy(struct icalperiodtype v, ...)
{
   va_list args;
   struct icalproperty_impl *impl = icalproperty_new_impl(ICAL_FREEBUSY_PROPERTY);  
   

   icalproperty_set_freebusy((icalproperty*)impl,v);

   va_start(args,v);
   icalproperty_add_parameters(impl, args);
   va_end(args);

   return (icalproperty*)impl;
}
 
void icalproperty_set_freebusy(icalproperty* prop, struct icalperiodtype v)
{
    icalvalue *value;
   
    
    icalerror_check_arg_rv( (prop!=0),"prop");

    value = icalvalue_new_period(v);

    icalproperty_set_value(prop,value);

}

struct icalperiodtype icalproperty_get_freebusy(icalproperty* prop)
{
    icalvalue *value;
    icalerror_check_arg( (prop!=0),"prop");

    value = icalproperty_get_value(prop);

    return icalvalue_get_period(value);
}

/* DTSTART */

icalproperty* icalproperty_new_dtstart(struct icaltimetype v)
{
   struct icalproperty_impl *impl = icalproperty_new_impl(ICAL_DTSTART_PROPERTY);  
   

   icalproperty_set_dtstart((icalproperty*)impl,v);

   return (icalproperty*)impl;
}

icalproperty* icalproperty_vanew_dtstart(struct icaltimetype v, ...)
{
   va_list args;
   struct icalproperty_impl *impl = icalproperty_new_impl(ICAL_DTSTART_PROPERTY);  
   

   icalproperty_set_dtstart((icalproperty*)impl,v);

   va_start(args,v);
   icalproperty_add_parameters(impl, args);
   va_end(args);

   return (icalproperty*)impl;
}
 
void icalproperty_set_dtstart(icalproperty* prop, struct icaltimetype v)
{
    icalvalue *value;
   
    
    icalerror_check_arg_rv( (prop!=0),"prop");

    value = icalvalue_new_datetimedate(v);

    icalproperty_set_value(prop,value);

}

struct icaltimetype icalproperty_get_dtstart(icalproperty* prop)
{
    icalvalue *value;
    icalerror_check_arg( (prop!=0),"prop");

    value = icalproperty_get_value(prop);

    return icalvalue_get_datetimedate(value);
}

/* RECURRENCE-ID */

icalproperty* icalproperty_new_recurrenceid(struct icaltimetype v)
{
   struct icalproperty_impl *impl = icalproperty_new_impl(ICAL_RECURRENCEID_PROPERTY);  
   

   icalproperty_set_recurrenceid((icalproperty*)impl,v);

   return (icalproperty*)impl;
}

icalproperty* icalproperty_vanew_recurrenceid(struct icaltimetype v, ...)
{
   va_list args;
   struct icalproperty_impl *impl = icalproperty_new_impl(ICAL_RECURRENCEID_PROPERTY);  
   

   icalproperty_set_recurrenceid((icalproperty*)impl,v);

   va_start(args,v);
   icalproperty_add_parameters(impl, args);
   va_end(args);

   return (icalproperty*)impl;
}
 
void icalproperty_set_recurrenceid(icalproperty* prop, struct icaltimetype v)
{
    icalvalue *value;
   
    
    icalerror_check_arg_rv( (prop!=0),"prop");

    value = icalvalue_new_datetimedate(v);

    icalproperty_set_value(prop,value);

}

struct icaltimetype icalproperty_get_recurrenceid(icalproperty* prop)
{
    icalvalue *value;
    icalerror_check_arg( (prop!=0),"prop");

    value = icalproperty_get_value(prop);

    return icalvalue_get_datetimedate(value);
}

/* SUMMARY */

icalproperty* icalproperty_new_summary(char* v)
{
   struct icalproperty_impl *impl = icalproperty_new_impl(ICAL_SUMMARY_PROPERTY);  
   icalerror_check_arg_rz( (v!=0),"v");


   icalproperty_set_summary((icalproperty*)impl,v);

   return (icalproperty*)impl;
}

icalproperty* icalproperty_vanew_summary(char* v, ...)
{
   va_list args;
   struct icalproperty_impl *impl = icalproperty_new_impl(ICAL_SUMMARY_PROPERTY);  
   icalerror_check_arg_rz( (v!=0),"v");


   icalproperty_set_summary((icalproperty*)impl,v);

   va_start(args,v);
   icalproperty_add_parameters(impl, args);
   va_end(args);

   return (icalproperty*)impl;
}
 
void icalproperty_set_summary(icalproperty* prop, char* v)
{
    icalvalue *value;
   
    icalerror_check_arg_rv( (v!=0),"v");

    icalerror_check_arg_rv( (prop!=0),"prop");

    value = icalvalue_new_text(v);

    icalproperty_set_value(prop,value);

}

char* icalproperty_get_summary(icalproperty* prop)
{
    icalvalue *value;
    icalerror_check_arg( (prop!=0),"prop");

    value = icalproperty_get_value(prop);

    return icalvalue_get_text(value);
}

/* DTEND */

icalproperty* icalproperty_new_dtend(struct icaltimetype v)
{
   struct icalproperty_impl *impl = icalproperty_new_impl(ICAL_DTEND_PROPERTY);  
   

   icalproperty_set_dtend((icalproperty*)impl,v);

   return (icalproperty*)impl;
}

icalproperty* icalproperty_vanew_dtend(struct icaltimetype v, ...)
{
   va_list args;
   struct icalproperty_impl *impl = icalproperty_new_impl(ICAL_DTEND_PROPERTY);  
   

   icalproperty_set_dtend((icalproperty*)impl,v);

   va_start(args,v);
   icalproperty_add_parameters(impl, args);
   va_end(args);

   return (icalproperty*)impl;
}
 
void icalproperty_set_dtend(icalproperty* prop, struct icaltimetype v)
{
    icalvalue *value;
   
    
    icalerror_check_arg_rv( (prop!=0),"prop");

    value = icalvalue_new_datetimedate(v);

    icalproperty_set_value(prop,value);

}

struct icaltimetype icalproperty_get_dtend(icalproperty* prop)
{
    icalvalue *value;
    icalerror_check_arg( (prop!=0),"prop");

    value = icalproperty_get_value(prop);

    return icalvalue_get_datetimedate(value);
}

/* TZNAME */

icalproperty* icalproperty_new_tzname(char* v)
{
   struct icalproperty_impl *impl = icalproperty_new_impl(ICAL_TZNAME_PROPERTY);  
   icalerror_check_arg_rz( (v!=0),"v");


   icalproperty_set_tzname((icalproperty*)impl,v);

   return (icalproperty*)impl;
}

icalproperty* icalproperty_vanew_tzname(char* v, ...)
{
   va_list args;
   struct icalproperty_impl *impl = icalproperty_new_impl(ICAL_TZNAME_PROPERTY);  
   icalerror_check_arg_rz( (v!=0),"v");


   icalproperty_set_tzname((icalproperty*)impl,v);

   va_start(args,v);
   icalproperty_add_parameters(impl, args);
   va_end(args);

   return (icalproperty*)impl;
}
 
void icalproperty_set_tzname(icalproperty* prop, char* v)
{
    icalvalue *value;
   
    icalerror_check_arg_rv( (v!=0),"v");

    icalerror_check_arg_rv( (prop!=0),"prop");

    value = icalvalue_new_text(v);

    icalproperty_set_value(prop,value);

}

char* icalproperty_get_tzname(icalproperty* prop)
{
    icalvalue *value;
    icalerror_check_arg( (prop!=0),"prop");

    value = icalproperty_get_value(prop);

    return icalvalue_get_text(value);
}

/* RDATE */

icalproperty* icalproperty_new_rdate(struct icalperiodtype v)
{
   struct icalproperty_impl *impl = icalproperty_new_impl(ICAL_RDATE_PROPERTY);  
   

   icalproperty_set_rdate((icalproperty*)impl,v);

   return (icalproperty*)impl;
}

icalproperty* icalproperty_vanew_rdate(struct icalperiodtype v, ...)
{
   va_list args;
   struct icalproperty_impl *impl = icalproperty_new_impl(ICAL_RDATE_PROPERTY);  
   

   icalproperty_set_rdate((icalproperty*)impl,v);

   va_start(args,v);
   icalproperty_add_parameters(impl, args);
   va_end(args);

   return (icalproperty*)impl;
}
 
void icalproperty_set_rdate(icalproperty* prop, struct icalperiodtype v)
{
    icalvalue *value;
   
    
    icalerror_check_arg_rv( (prop!=0),"prop");

    value = icalvalue_new_datetimeperiod(v);

    icalproperty_set_value(prop,value);

}

struct icalperiodtype icalproperty_get_rdate(icalproperty* prop)
{
    icalvalue *value;
    icalerror_check_arg( (prop!=0),"prop");

    value = icalproperty_get_value(prop);

    return icalvalue_get_datetimeperiod(value);
}

/* URL */

icalproperty* icalproperty_new_url(char* v)
{
   struct icalproperty_impl *impl = icalproperty_new_impl(ICAL_URL_PROPERTY);  
   icalerror_check_arg_rz( (v!=0),"v");


   icalproperty_set_url((icalproperty*)impl,v);

   return (icalproperty*)impl;
}

icalproperty* icalproperty_vanew_url(char* v, ...)
{
   va_list args;
   struct icalproperty_impl *impl = icalproperty_new_impl(ICAL_URL_PROPERTY);  
   icalerror_check_arg_rz( (v!=0),"v");


   icalproperty_set_url((icalproperty*)impl,v);

   va_start(args,v);
   icalproperty_add_parameters(impl, args);
   va_end(args);

   return (icalproperty*)impl;
}
 
void icalproperty_set_url(icalproperty* prop, char* v)
{
    icalvalue *value;
   
    icalerror_check_arg_rv( (v!=0),"v");

    icalerror_check_arg_rv( (prop!=0),"prop");

    value = icalvalue_new_uri(v);

    icalproperty_set_value(prop,value);

}

char* icalproperty_get_url(icalproperty* prop)
{
    icalvalue *value;
    icalerror_check_arg( (prop!=0),"prop");

    value = icalproperty_get_value(prop);

    return icalvalue_get_uri(value);
}

/* ATTACH */

icalproperty* icalproperty_new_attach(struct icalattachtype v)
{
   struct icalproperty_impl *impl = icalproperty_new_impl(ICAL_ATTACH_PROPERTY);  
   

   icalproperty_set_attach((icalproperty*)impl,v);

   return (icalproperty*)impl;
}

icalproperty* icalproperty_vanew_attach(struct icalattachtype v, ...)
{
   va_list args;
   struct icalproperty_impl *impl = icalproperty_new_impl(ICAL_ATTACH_PROPERTY);  
   

   icalproperty_set_attach((icalproperty*)impl,v);

   va_start(args,v);
   icalproperty_add_parameters(impl, args);
   va_end(args);

   return (icalproperty*)impl;
}
 
void icalproperty_set_attach(icalproperty* prop, struct icalattachtype v)
{
    icalvalue *value;
   
    
    icalerror_check_arg_rv( (prop!=0),"prop");

    value = icalvalue_new_attach(v);

    icalproperty_set_value(prop,value);

}

struct icalattachtype icalproperty_get_attach(icalproperty* prop)
{
    icalvalue *value;
    icalerror_check_arg( (prop!=0),"prop");

    value = icalproperty_get_value(prop);

    return icalvalue_get_attach(value);
}

/* X-LIC-CLUSTERCOUNT */

icalproperty* icalproperty_new_xlicclustercount(int v)
{
   struct icalproperty_impl *impl = icalproperty_new_impl(ICAL_XLICCLUSTERCOUNT_PROPERTY);  
   

   icalproperty_set_xlicclustercount((icalproperty*)impl,v);

   return (icalproperty*)impl;
}

icalproperty* icalproperty_vanew_xlicclustercount(int v, ...)
{
   va_list args;
   struct icalproperty_impl *impl = icalproperty_new_impl(ICAL_XLICCLUSTERCOUNT_PROPERTY);  
   

   icalproperty_set_xlicclustercount((icalproperty*)impl,v);

   va_start(args,v);
   icalproperty_add_parameters(impl, args);
   va_end(args);

   return (icalproperty*)impl;
}
 
void icalproperty_set_xlicclustercount(icalproperty* prop, int v)
{
    icalvalue *value;
   
    
    icalerror_check_arg_rv( (prop!=0),"prop");

    value = icalvalue_new_integer(v);

    icalproperty_set_value(prop,value);

}

int icalproperty_get_xlicclustercount(icalproperty* prop)
{
    icalvalue *value;
    icalerror_check_arg( (prop!=0),"prop");

    value = icalproperty_get_value(prop);

    return icalvalue_get_integer(value);
}

/* EXRULE */

icalproperty* icalproperty_new_exrule(struct icalrecurrencetype v)
{
   struct icalproperty_impl *impl = icalproperty_new_impl(ICAL_EXRULE_PROPERTY);  
   

   icalproperty_set_exrule((icalproperty*)impl,v);

   return (icalproperty*)impl;
}

icalproperty* icalproperty_vanew_exrule(struct icalrecurrencetype v, ...)
{
   va_list args;
   struct icalproperty_impl *impl = icalproperty_new_impl(ICAL_EXRULE_PROPERTY);  
   

   icalproperty_set_exrule((icalproperty*)impl,v);

   va_start(args,v);
   icalproperty_add_parameters(impl, args);
   va_end(args);

   return (icalproperty*)impl;
}
 
void icalproperty_set_exrule(icalproperty* prop, struct icalrecurrencetype v)
{
    icalvalue *value;
   
    
    icalerror_check_arg_rv( (prop!=0),"prop");

    value = icalvalue_new_recur(v);

    icalproperty_set_value(prop,value);

}

struct icalrecurrencetype icalproperty_get_exrule(icalproperty* prop)
{
    icalvalue *value;
    icalerror_check_arg( (prop!=0),"prop");

    value = icalproperty_get_value(prop);

    return icalvalue_get_recur(value);
}

/* QUERY */

icalproperty* icalproperty_new_query(char* v)
{
   struct icalproperty_impl *impl = icalproperty_new_impl(ICAL_QUERY_PROPERTY);  
   icalerror_check_arg_rz( (v!=0),"v");


   icalproperty_set_query((icalproperty*)impl,v);

   return (icalproperty*)impl;
}

icalproperty* icalproperty_vanew_query(char* v, ...)
{
   va_list args;
   struct icalproperty_impl *impl = icalproperty_new_impl(ICAL_QUERY_PROPERTY);  
   icalerror_check_arg_rz( (v!=0),"v");


   icalproperty_set_query((icalproperty*)impl,v);

   va_start(args,v);
   icalproperty_add_parameters(impl, args);
   va_end(args);

   return (icalproperty*)impl;
}
 
void icalproperty_set_query(icalproperty* prop, char* v)
{
    icalvalue *value;
   
    icalerror_check_arg_rv( (v!=0),"v");

    icalerror_check_arg_rv( (prop!=0),"prop");

    value = icalvalue_new_query(v);

    icalproperty_set_value(prop,value);

}

char* icalproperty_get_query(icalproperty* prop)
{
    icalvalue *value;
    icalerror_check_arg( (prop!=0),"prop");

    value = icalproperty_get_value(prop);

    return icalvalue_get_query(value);
}

/* PERCENT-COMPLETE */

icalproperty* icalproperty_new_percentcomplete(int v)
{
   struct icalproperty_impl *impl = icalproperty_new_impl(ICAL_PERCENTCOMPLETE_PROPERTY);  
   

   icalproperty_set_percentcomplete((icalproperty*)impl,v);

   return (icalproperty*)impl;
}

icalproperty* icalproperty_vanew_percentcomplete(int v, ...)
{
   va_list args;
   struct icalproperty_impl *impl = icalproperty_new_impl(ICAL_PERCENTCOMPLETE_PROPERTY);  
   

   icalproperty_set_percentcomplete((icalproperty*)impl,v);

   va_start(args,v);
   icalproperty_add_parameters(impl, args);
   va_end(args);

   return (icalproperty*)impl;
}
 
void icalproperty_set_percentcomplete(icalproperty* prop, int v)
{
    icalvalue *value;
   
    
    icalerror_check_arg_rv( (prop!=0),"prop");

    value = icalvalue_new_integer(v);

    icalproperty_set_value(prop,value);

}

int icalproperty_get_percentcomplete(icalproperty* prop)
{
    icalvalue *value;
    icalerror_check_arg( (prop!=0),"prop");

    value = icalproperty_get_value(prop);

    return icalvalue_get_integer(value);
}

/* CALSCALE */

icalproperty* icalproperty_new_calscale(char* v)
{
   struct icalproperty_impl *impl = icalproperty_new_impl(ICAL_CALSCALE_PROPERTY);  
   icalerror_check_arg_rz( (v!=0),"v");


   icalproperty_set_calscale((icalproperty*)impl,v);

   return (icalproperty*)impl;
}

icalproperty* icalproperty_vanew_calscale(char* v, ...)
{
   va_list args;
   struct icalproperty_impl *impl = icalproperty_new_impl(ICAL_CALSCALE_PROPERTY);  
   icalerror_check_arg_rz( (v!=0),"v");


   icalproperty_set_calscale((icalproperty*)impl,v);

   va_start(args,v);
   icalproperty_add_parameters(impl, args);
   va_end(args);

   return (icalproperty*)impl;
}
 
void icalproperty_set_calscale(icalproperty* prop, char* v)
{
    icalvalue *value;
   
    icalerror_check_arg_rv( (v!=0),"v");

    icalerror_check_arg_rv( (prop!=0),"prop");

    value = icalvalue_new_text(v);

    icalproperty_set_value(prop,value);

}

char* icalproperty_get_calscale(icalproperty* prop)
{
    icalvalue *value;
    icalerror_check_arg( (prop!=0),"prop");

    value = icalproperty_get_value(prop);

    return icalvalue_get_text(value);
}

/* CREATED */

icalproperty* icalproperty_new_created(struct icaltimetype v)
{
   struct icalproperty_impl *impl = icalproperty_new_impl(ICAL_CREATED_PROPERTY);  
   

   icalproperty_set_created((icalproperty*)impl,v);

   return (icalproperty*)impl;
}

icalproperty* icalproperty_vanew_created(struct icaltimetype v, ...)
{
   va_list args;
   struct icalproperty_impl *impl = icalproperty_new_impl(ICAL_CREATED_PROPERTY);  
   

   icalproperty_set_created((icalproperty*)impl,v);

   va_start(args,v);
   icalproperty_add_parameters(impl, args);
   va_end(args);

   return (icalproperty*)impl;
}
 
void icalproperty_set_created(icalproperty* prop, struct icaltimetype v)
{
    icalvalue *value;
   
    
    icalerror_check_arg_rv( (prop!=0),"prop");

    value = icalvalue_new_datetime(v);

    icalproperty_set_value(prop,value);

}

struct icaltimetype icalproperty_get_created(icalproperty* prop)
{
    icalvalue *value;
    icalerror_check_arg( (prop!=0),"prop");

    value = icalproperty_get_value(prop);

    return icalvalue_get_datetime(value);
}

/* GEO */

icalproperty* icalproperty_new_geo(struct icalgeotype v)
{
   struct icalproperty_impl *impl = icalproperty_new_impl(ICAL_GEO_PROPERTY);  
   

   icalproperty_set_geo((icalproperty*)impl,v);

   return (icalproperty*)impl;
}

icalproperty* icalproperty_vanew_geo(struct icalgeotype v, ...)
{
   va_list args;
   struct icalproperty_impl *impl = icalproperty_new_impl(ICAL_GEO_PROPERTY);  
   

   icalproperty_set_geo((icalproperty*)impl,v);

   va_start(args,v);
   icalproperty_add_parameters(impl, args);
   va_end(args);

   return (icalproperty*)impl;
}
 
void icalproperty_set_geo(icalproperty* prop, struct icalgeotype v)
{
    icalvalue *value;
   
    
    icalerror_check_arg_rv( (prop!=0),"prop");

    value = icalvalue_new_geo(v);

    icalproperty_set_value(prop,value);

}

struct icalgeotype icalproperty_get_geo(icalproperty* prop)
{
    icalvalue *value;
    icalerror_check_arg( (prop!=0),"prop");

    value = icalproperty_get_value(prop);

    return icalvalue_get_geo(value);
}

/* COMPLETED */

icalproperty* icalproperty_new_completed(struct icaltimetype v)
{
   struct icalproperty_impl *impl = icalproperty_new_impl(ICAL_COMPLETED_PROPERTY);  
   

   icalproperty_set_completed((icalproperty*)impl,v);

   return (icalproperty*)impl;
}

icalproperty* icalproperty_vanew_completed(struct icaltimetype v, ...)
{
   va_list args;
   struct icalproperty_impl *impl = icalproperty_new_impl(ICAL_COMPLETED_PROPERTY);  
   

   icalproperty_set_completed((icalproperty*)impl,v);

   va_start(args,v);
   icalproperty_add_parameters(impl, args);
   va_end(args);

   return (icalproperty*)impl;
}
 
void icalproperty_set_completed(icalproperty* prop, struct icaltimetype v)
{
    icalvalue *value;
   
    
    icalerror_check_arg_rv( (prop!=0),"prop");

    value = icalvalue_new_datetime(v);

    icalproperty_set_value(prop,value);

}

struct icaltimetype icalproperty_get_completed(icalproperty* prop)
{
    icalvalue *value;
    icalerror_check_arg( (prop!=0),"prop");

    value = icalproperty_get_value(prop);

    return icalvalue_get_datetime(value);
}

/* DTSTAMP */

icalproperty* icalproperty_new_dtstamp(struct icaltimetype v)
{
   struct icalproperty_impl *impl = icalproperty_new_impl(ICAL_DTSTAMP_PROPERTY);  
   

   icalproperty_set_dtstamp((icalproperty*)impl,v);

   return (icalproperty*)impl;
}

icalproperty* icalproperty_vanew_dtstamp(struct icaltimetype v, ...)
{
   va_list args;
   struct icalproperty_impl *impl = icalproperty_new_impl(ICAL_DTSTAMP_PROPERTY);  
   

   icalproperty_set_dtstamp((icalproperty*)impl,v);

   va_start(args,v);
   icalproperty_add_parameters(impl, args);
   va_end(args);

   return (icalproperty*)impl;
}
 
void icalproperty_set_dtstamp(icalproperty* prop, struct icaltimetype v)
{
    icalvalue *value;
   
    
    icalerror_check_arg_rv( (prop!=0),"prop");

    value = icalvalue_new_datetime(v);

    icalproperty_set_value(prop,value);

}

struct icaltimetype icalproperty_get_dtstamp(icalproperty* prop)
{
    icalvalue *value;
    icalerror_check_arg( (prop!=0),"prop");

    value = icalproperty_get_value(prop);

    return icalvalue_get_datetime(value);
}

/* DUE */

icalproperty* icalproperty_new_due(struct icaltimetype v)
{
   struct icalproperty_impl *impl = icalproperty_new_impl(ICAL_DUE_PROPERTY);  
   

   icalproperty_set_due((icalproperty*)impl,v);

   return (icalproperty*)impl;
}

icalproperty* icalproperty_vanew_due(struct icaltimetype v, ...)
{
   va_list args;
   struct icalproperty_impl *impl = icalproperty_new_impl(ICAL_DUE_PROPERTY);  
   

   icalproperty_set_due((icalproperty*)impl,v);

   va_start(args,v);
   icalproperty_add_parameters(impl, args);
   va_end(args);

   return (icalproperty*)impl;
}
 
void icalproperty_set_due(icalproperty* prop, struct icaltimetype v)
{
    icalvalue *value;
   
    
    icalerror_check_arg_rv( (prop!=0),"prop");

    value = icalvalue_new_datetimedate(v);

    icalproperty_set_value(prop,value);

}

struct icaltimetype icalproperty_get_due(icalproperty* prop)
{
    icalvalue *value;
    icalerror_check_arg( (prop!=0),"prop");

    value = icalproperty_get_value(prop);

    return icalvalue_get_datetimedate(value);
}

/* ACTION */

icalproperty* icalproperty_new_action(char* v)
{
   struct icalproperty_impl *impl = icalproperty_new_impl(ICAL_ACTION_PROPERTY);  
   icalerror_check_arg_rz( (v!=0),"v");


   icalproperty_set_action((icalproperty*)impl,v);

   return (icalproperty*)impl;
}

icalproperty* icalproperty_vanew_action(char* v, ...)
{
   va_list args;
   struct icalproperty_impl *impl = icalproperty_new_impl(ICAL_ACTION_PROPERTY);  
   icalerror_check_arg_rz( (v!=0),"v");


   icalproperty_set_action((icalproperty*)impl,v);

   va_start(args,v);
   icalproperty_add_parameters(impl, args);
   va_end(args);

   return (icalproperty*)impl;
}
 
void icalproperty_set_action(icalproperty* prop, char* v)
{
    icalvalue *value;
   
    icalerror_check_arg_rv( (v!=0),"v");

    icalerror_check_arg_rv( (prop!=0),"prop");

    value = icalvalue_new_text(v);

    icalproperty_set_value(prop,value);

}

char* icalproperty_get_action(icalproperty* prop)
{
    icalvalue *value;
    icalerror_check_arg( (prop!=0),"prop");

    value = icalproperty_get_value(prop);

    return icalvalue_get_text(value);
}
