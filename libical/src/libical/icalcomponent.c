/*======================================================================
  FILE: icalcomponent.c
  CREATOR: eric 28 April 1999
  
  $Id$

 (C) COPYRIGHT 2000, Eric Busboom, http://www.softwarestudio.org

 This program is free software; you can redistribute it and/or modify
 it under the terms of either: 

    The LGPL as published by the Free Software Foundation, version
    2.1, available at: http://www.fsf.org/copyleft/lesser.html

  Or:

    The Mozilla Public License Version 1.0. You may obtain a copy of
    the License at http://www.mozilla.org/MPL/

  The original code is icalcomponent.c

======================================================================*/


#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "ical.h"
#include "pvl.h" /* "Pointer-to-void list" */
#include <stdlib.h>  /* for malloc */
#include <stdarg.h> /* for va_list, etc */
#include <errno.h>
#include "icalerror.h"
#include <assert.h>
#include <stdio.h> /* for fprintf */
#include "icalmemory.h"
#include "icalenums.h"

#define MAX_TMP 1024


/* icalproperty functions that only components get to use */
void icalproperty_set_parent(icalproperty* property,
			     icalcomponent* component);

icalcomponent* icalproperty_get_parent(icalproperty* property);



struct icalcomponent_impl 
{
	char id[5];
	icalcomponent_kind kind;
	char* x_name;
	pvl_list properties;
	pvl_elem property_iterator;
	pvl_list components;
	pvl_elem component_iterator;
	icalcomponent* parent;
};

void icalcomponent_add_children(struct icalcomponent_impl *impl,va_list args)
{
    void* vp;
    
    while((vp = va_arg(args, void*)) != 0) {

	assert (icalcomponent_isa_component(vp) != 0 ||
		icalproperty_isa_property(vp) != 0 ) ;

	if (icalcomponent_isa_component(vp) != 0 ){

	    icalcomponent_add_component((icalcomponent*)impl,
				       (icalcomponent*)vp);

	} else if (icalproperty_isa_property(vp) != 0 ){

	    icalcomponent_add_property((icalcomponent*)impl,
				       (icalproperty*)vp);
	}
    }    
}

icalcomponent*
icalcomponent_new_impl (icalcomponent_kind kind)
{
    struct icalcomponent_impl* comp;

    if ( ( comp = (struct icalcomponent_impl*)
	   malloc(sizeof(struct icalcomponent_impl))) == 0) {
	icalerror_set_errno(ICAL_NEWFAILED_ERROR);
	return 0;
    }
    
    strcpy(comp->id,"comp");

    comp->kind = kind;
    comp->properties = pvl_newlist();
    comp->property_iterator = 0;
    comp->components = pvl_newlist();
    comp->component_iterator = 0;
    comp->x_name = 0;
    comp->parent = 0;

    return comp;
}

icalcomponent*
icalcomponent_new (icalcomponent_kind kind)
{
   return (icalcomponent*)icalcomponent_new_impl(kind);
}

icalcomponent*
icalcomponent_vanew (icalcomponent_kind kind, ...)
{
   va_list args;

   struct icalcomponent_impl *impl = icalcomponent_new_impl(kind);

    if (impl == 0){
	return 0;
    }

   va_start(args,kind);
   icalcomponent_add_children(impl, args);
   va_end(args);

   return (icalcomponent*) impl;
}

icalcomponent* icalcomponent_new_from_string(char* str)
{
    icalcomponent_kind kind;

    icalerror_check_arg_rz( (str!=0), "str");

    kind = icalenum_string_to_component_kind(str);

    if (kind == ICAL_NO_COMPONENT){
	return 0;
    }
    
    return icalcomponent_new(kind);
}

icalcomponent* icalcomponent_new_clone(icalcomponent* component)
{
    struct icalcomponent_impl *old = (struct icalcomponent_impl*)component;
    struct icalcomponent_impl *new;
    icalproperty *p;
    icalcomponent *c;
    pvl_elem itr;

    icalerror_check_arg_rv( (component!=0), "component");

    new = icalcomponent_new_impl(old->kind);

    if (new == 0){
	return 0;
    }

    
    for( itr = pvl_head(old->properties);
	 itr != 0;
	 itr = pvl_next(itr))
    {	
	p = (icalproperty*)pvl_data(itr);
	icalcomponent_add_property(new,icalproperty_new_clone(p));
    }
   
   
    for( itr = pvl_head(old->components);
	 itr != 0;
	 itr = pvl_next(itr))
    {	
	c = (icalcomponent*)pvl_data(itr);
	icalcomponent_add_component(new,icalcomponent_new_clone(c));
    }

   return new;

}


void
icalcomponent_free (icalcomponent* component)
{
    icalproperty* prop;
    icalcomponent* comp;
    struct icalcomponent_impl *c = (struct icalcomponent_impl*)component;

    icalerror_check_arg_rv( (component!=0), "component");

#ifdef ICAL_FREE_ON_LIST_IS_ERROR
    icalerror_assert( (c->parent ==0),"Tried to free a component that is still attached to a parent component");
#else
    if(c->parent != 0){
	return;
    }
#endif

    if(component != 0 ){
       
       while( (prop=pvl_pop(c->properties)) != 0){
	   assert(prop != 0);
           icalproperty_set_parent(prop,0);
	   icalproperty_free(prop);
       }
       
       pvl_free(c->properties);

       while( (comp=pvl_data(pvl_head(c->components))) != 0){
	   assert(comp!=0);
	   icalcomponent_remove_component(component,comp);
	   icalcomponent_free(comp);
       }
       
       pvl_free(c->components);

	if (c->x_name != 0) {
	    free(c->x_name);
	}

	c->kind = ICAL_NO_COMPONENT;
	c->properties = 0;
	c->property_iterator = 0;
	c->components = 0;
	c->component_iterator = 0;
	c->x_name = 0;	
	c->id[0] = 'X';

	free(c);
    }
}

char*
icalcomponent_as_ical_string (icalcomponent* component)
{
   char* buf, *out_buf;
   char* tmp_buf;
   size_t buf_size = 1024;
   char* buf_ptr = 0;
    pvl_elem itr;
    struct icalcomponent_impl *impl = (struct icalcomponent_impl*)component;

#ifdef ICAL_UNIX_NEWLINE    
    char newline[] = "\n";
#else
    char newline[] = "\r\n";
#endif
   
   icalcomponent *c;
   icalproperty *p;
   icalcomponent_kind kind = icalcomponent_isa(component);

   char* kind_string;

   buf = icalmemory_new_buffer(buf_size);
   buf_ptr = buf; 

   icalerror_check_arg_rz( (component!=0), "component");
   icalerror_check_arg_rz( (kind!=ICAL_NO_COMPONENT), "component kind is ICAL_NO_COMPONENT");
   
   kind_string  = icalenum_component_kind_to_string(kind);

   icalerror_check_arg_rz( (kind_string!=0),"Unknown kind of component");

   icalmemory_append_string(&buf, &buf_ptr, &buf_size, "BEGIN:");
   icalmemory_append_string(&buf, &buf_ptr, &buf_size, kind_string);
   icalmemory_append_string(&buf, &buf_ptr, &buf_size, newline);
   


   for( itr = pvl_head(impl->properties);
	 itr != 0;
	 itr = pvl_next(itr))
    {	
	p = (icalproperty*)pvl_data(itr);
	
	icalerror_assert((p!=0),"Got a null property");
	tmp_buf = icalproperty_as_ical_string(p);
	
	icalmemory_append_string(&buf, &buf_ptr, &buf_size, tmp_buf);
    }
   
   
   for( itr = pvl_head(impl->components);
	itr != 0;
	itr = pvl_next(itr))
   {	
       c = (icalcomponent*)pvl_data(itr);
       
       tmp_buf = icalcomponent_as_ical_string(c);
       
       icalmemory_append_string(&buf, &buf_ptr, &buf_size, tmp_buf);
       
   }
   
   icalmemory_append_string(&buf, &buf_ptr, &buf_size, "END:");
   icalmemory_append_string(&buf, &buf_ptr, &buf_size, 
			    icalenum_component_kind_to_string(kind));
   icalmemory_append_string(&buf, &buf_ptr, &buf_size, newline);

   out_buf = icalmemory_tmp_copy(buf);
   free(buf);

   return out_buf;
}


int
icalcomponent_is_valid (icalcomponent* component)
{
    struct icalcomponent_impl *impl = (struct icalcomponent_impl *)component;

	
    if ( (strcmp(impl->id,"comp") == 0) &&
	 impl->kind != ICAL_NO_COMPONENT){
	return 1;
    } else {
	return 0;
    }

}


icalcomponent_kind
icalcomponent_isa (icalcomponent* component)
{
    struct icalcomponent_impl *impl = (struct icalcomponent_impl *)component;
    icalerror_check_arg_rz( (component!=0), "component");

   if(component != 0)
   {
       return impl->kind;
   }

   return ICAL_NO_COMPONENT;
}


int
icalcomponent_isa_component (void* component)
{
    struct icalcomponent_impl *impl = (struct icalcomponent_impl *)component;

    icalerror_check_arg_rz( (component!=0), "component");

    if (strcmp(impl->id,"comp") == 0) {
	return 1;
    } else {
	return 0;
    }

}

int icalcomponent_property_sorter(void *a, void *b)
{
    icalproperty_kind kinda, kindb;
    char *ksa, *ksb;

    kinda = icalproperty_isa((icalproperty*)a);
    kindb = icalproperty_isa((icalproperty*)b);

    ksa = icalenum_property_kind_to_string(kinda);
    ksb = icalenum_property_kind_to_string(kindb);

    return strcmp(ksa,ksb);
}


void
icalcomponent_add_property (icalcomponent* component, icalproperty* property)
{
    struct icalcomponent_impl *impl;

    icalerror_check_arg_rv( (component!=0), "component");
    icalerror_check_arg_rv( (property!=0), "property");

     impl = (struct icalcomponent_impl*)component;

    icalerror_assert( (!icalproperty_get_parent(property)),"The property has already been added to a component. Remove the property with icalcomponent_remove_property before calling icalcomponent_add_property");

    icalproperty_set_parent(property,component);

#ifdef ICAL_INSERT_ORDERED
    pvl_insert_ordered(impl->properties,
		       icalcomponent_property_sorter,property);
#else
    pvl_push(impl->properties,property);
#endif

}


void
icalcomponent_remove_property (icalcomponent* component, icalproperty* property)
{
    struct icalcomponent_impl *impl;
    pvl_elem itr, next_itr;
    struct icalproperty_impl *pimpl;

    icalerror_check_arg_rv( (component!=0), "component");
    icalerror_check_arg_rv( (property!=0), "property");
    
    impl = (struct icalcomponent_impl*)component;

    pimpl = (struct icalproperty_impl*)property;

    icalerror_assert( (icalproperty_get_parent(property)),"The property is not a member of a component");

    
    for( itr = pvl_head(impl->properties);
	 itr != 0;
	 itr = next_itr)
    {
	next_itr = pvl_next(itr);
	
	if( pvl_data(itr) == (void*)property ){

	   if (impl->property_iterator == itr){
	       impl->property_iterator = pvl_next(itr);
	   }

	   pvl_remove( impl->properties, itr); 
	  icalproperty_set_parent(property,0);
	}
    }	
}

int
icalcomponent_count_properties (icalcomponent* component, 
				icalproperty_kind kind)
{
    int count=0;
    pvl_elem itr;
    struct icalcomponent_impl *impl = (struct icalcomponent_impl*)component;

    icalerror_check_arg_rz( (component!=0), "component");

    for( itr = pvl_head(impl->properties);
	 itr != 0;
	 itr = pvl_next(itr))
    {	
	if(kind == icalproperty_isa((icalproperty*)pvl_data(itr)) ||
	    kind == ICAL_ANY_PROPERTY){
	    count++;
	}
    }


    return count;

}

icalproperty* icalcomponent_get_current_property (icalcomponent* component)
{

   struct icalcomponent_impl *c = (struct icalcomponent_impl*)component;
   icalerror_check_arg_rz( (component!=0),"component");

   if ((c->property_iterator==0)){
       return 0;
   }

   return (icalproperty*) pvl_data(c->property_iterator);

}

icalproperty*
icalcomponent_get_first_property (icalcomponent* component, icalproperty_kind kind)
{
   struct icalcomponent_impl *c = (struct icalcomponent_impl*)component;
   icalerror_check_arg_rz( (component!=0),"component");
  
   for( c->property_iterator = pvl_head(c->properties);
	c->property_iterator != 0;
	c->property_iterator = pvl_next(c->property_iterator)) {
	    
       icalproperty *p =  (icalproperty*) pvl_data(c->property_iterator);
	
	   if (icalproperty_isa(p) == kind || kind == ICAL_ANY_PROPERTY) {
	       
	       return p;
	   }
   }
   return 0;
}

icalproperty*
icalcomponent_get_next_property (icalcomponent* component, icalproperty_kind kind)
{
   struct icalcomponent_impl *c = (struct icalcomponent_impl*)component;
   icalerror_check_arg_rz( (component!=0),"component");

   if (c->property_iterator == 0){
       return 0;
   }

   for( c->property_iterator = pvl_next(c->property_iterator);
	c->property_iterator != 0;
	c->property_iterator = pvl_next(c->property_iterator)) {
	    
       icalproperty *p =  (icalproperty*) pvl_data(c->property_iterator);
	   
       if (icalproperty_isa(p) == kind || kind == ICAL_ANY_PROPERTY) {
	   
	   return p;
       }
   }

   return 0;
}


icalproperty**
icalcomponent_get_properties (icalcomponent* component, icalproperty_kind kind);


void
icalcomponent_add_component (icalcomponent* parent, icalcomponent* child)
{
    struct icalcomponent_impl *impl, *cimpl;

    icalerror_check_arg_rv( (parent!=0), "parent");
    icalerror_check_arg_rv( (child!=0), "child");
    
    impl = (struct icalcomponent_impl*)parent;
    cimpl = (struct icalcomponent_impl*)child;

    icalerror_assert( (cimpl->parent ==0),"The child component has already been added to a parent component. Remove the component with icalcomponent_remove_componenet before calling icalcomponent_add_component");

    cimpl->parent = parent;

    pvl_push(impl->components,child);
}


void
icalcomponent_remove_component (icalcomponent* parent, icalcomponent* child)
{
   struct icalcomponent_impl *impl,*cimpl;
   pvl_elem itr, next_itr;

   icalerror_check_arg_rv( (parent!=0), "parent");
   icalerror_check_arg_rv( (child!=0), "child");
   
   impl = (struct icalcomponent_impl*)parent;
   cimpl = (struct icalcomponent_impl*)child;
   
   for( itr = pvl_head(impl->components);
	itr != 0;
	itr = next_itr)
   {
       next_itr = pvl_next(itr);
       
       if( pvl_data(itr) == (void*)child ){

	   if (impl->component_iterator == itr){
	       /* Don't let the current iterator become invalid */

	       /* HACK. The semantics for this are troubling. */
	       impl->component_iterator = 
		   pvl_next(impl->component_iterator);
	          
	   }
	   pvl_remove( impl->components, itr); 
	   cimpl->parent = 0;
	   break;
       }
   }	
}


int
icalcomponent_count_components (icalcomponent* component, 
				icalcomponent_kind kind)
{
    int count=0;
    pvl_elem itr;
    struct icalcomponent_impl *impl =
        (struct icalcomponent_impl*)component;

    icalerror_check_arg_rz( (component!=0), "component");

    for( itr = pvl_head(impl->components);
	 itr != 0;
	 itr = pvl_next(itr))
    {
	if(kind == icalcomponent_isa((icalcomponent*)pvl_data(itr)) ||
	    kind == ICAL_ANY_COMPONENT){
	    count++;
	}
    }

    return count;
}

icalcomponent*
icalcomponent_get_current_component(icalcomponent* component)
{
   struct icalcomponent_impl *c = (struct icalcomponent_impl*)component;

   icalerror_check_arg_rz( (component!=0),"component");

   if (c->component_iterator == 0){
       return 0;
   }

   return (icalcomponent*) pvl_data(c->component_iterator);
}

icalcomponent*
icalcomponent_get_first_component (icalcomponent* component, 
				   icalcomponent_kind kind)
{
   struct icalcomponent_impl *c = (struct icalcomponent_impl*)component;

   icalerror_check_arg_rz( (component!=0),"component");
  
   for( c->component_iterator = pvl_head(c->components);
	c->component_iterator != 0;
	c->component_iterator = pvl_next(c->component_iterator)) {
	    
       icalcomponent *p =  (icalcomponent*) pvl_data(c->component_iterator);
	
	   if (icalcomponent_isa(p) == kind || kind == ICAL_ANY_COMPONENT) {
	       
	       return p;
	   }
   }

   return 0;
}


icalcomponent*
icalcomponent_get_next_component (icalcomponent* component, icalcomponent_kind kind)
{
   struct icalcomponent_impl *c = (struct icalcomponent_impl*)component;

   icalerror_check_arg_rz( (component!=0),"component");
  
   if (c->component_iterator == 0){
       return 0;
   }

   for( c->component_iterator = pvl_next(c->component_iterator);
	c->component_iterator != 0;
	c->component_iterator = pvl_next(c->component_iterator)) {
	    
       icalcomponent *p =  (icalcomponent*) pvl_data(c->component_iterator);
	
	   if (icalcomponent_isa(p) == kind || kind == ICAL_ANY_COMPONENT) {
	       
	       return p;
	   }
   }

   return 0;
}


icalproperty**
icalcomponent_get_component (icalcomponent* component, icalproperty_kind kind);


int icalcomponent_count_errors(icalcomponent* component)
{
    int errors = 0;
    icalproperty *p;
    icalcomponent *c;
    pvl_elem itr;
    struct icalcomponent_impl *impl = (struct icalcomponent_impl*)component;

    for( itr = pvl_head(impl->properties);
	 itr != 0;
	 itr = pvl_next(itr))
    {	
	p = (icalproperty*)pvl_data(itr);
	
	if(icalproperty_isa(p) == ICAL_XLICERROR_PROPERTY)
	{
	    errors++;
	}
    }


    for( itr = pvl_head(impl->components);
	 itr != 0;
	 itr = pvl_next(itr))
    {	
	c = (icalcomponent*)pvl_data(itr);
	
	errors += icalcomponent_count_errors(c);
	
    }

    return errors;
}


void icalcomponent_strip_errors(icalcomponent* component)
{
    icalproperty *p;
    icalcomponent *c;
    pvl_elem itr, next_itr;
    struct icalcomponent_impl *impl = (struct icalcomponent_impl*)component;

   for( itr = pvl_head(impl->properties);
	 itr != 0;
	 itr = next_itr)
    {	
	p = (icalproperty*)pvl_data(itr);
	next_itr = pvl_next(itr);

	if(icalproperty_isa(p) == ICAL_XLICERROR_PROPERTY)
	{
	    icalcomponent_remove_property(component,p);
	}
    }
    
    for( itr = pvl_head(impl->components);
	 itr != 0;
	 itr = pvl_next(itr))
    {	
	c = (icalcomponent*)pvl_data(itr);
	icalcomponent_strip_errors(c);
    }
}

/* Hack. This will change the state of the iterators */
void icalcomponent_convert_errors(icalcomponent* component)
{
    icalproperty *p, *next_p;
    icalcomponent *c;

    for(p = icalcomponent_get_first_property(component,ICAL_ANY_PROPERTY);
	p != 0;
	p = next_p){
	
	next_p = icalcomponent_get_next_property(component,ICAL_ANY_PROPERTY);

	if(icalproperty_isa(p) == ICAL_XLICERROR_PROPERTY)
	{
	    struct icalreqstattype rst;
	    icalparameter *param  = icalproperty_get_first_parameter
		(p,ICAL_XLICERRORTYPE_PARAMETER);

	    rst.code = ICAL_UNKNOWN_STATUS;
	    rst.desc = 0;

	    switch(icalparameter_get_xlicerrortype(param)){

		case  ICAL_XLICERRORTYPE_PARAMETERNAMEPARSEERROR: {
		    rst.code = ICAL_3_2_INVPARAM_STATUS;
		    break;
		}
		case  ICAL_XLICERRORTYPE_PARAMETERVALUEPARSEERROR: {
		    rst.code = ICAL_3_3_INVPARAMVAL_STATUS;
		    break;
		}
		case  ICAL_XLICERRORTYPE_PROPERTYPARSEERROR: {		    
		    rst.code = ICAL_3_0_INVPROPNAME_STATUS;
		    break;
		}
		case  ICAL_XLICERRORTYPE_VALUEPARSEERROR: {
		    rst.code = ICAL_3_1_INVPROPVAL_STATUS;
		    break;
		}
		case  ICAL_XLICERRORTYPE_COMPONENTPARSEERROR: {
		    rst.code = ICAL_3_4_INVCOMP_STATUS;
		    break;
		}

		default: {
		}
	    }
	    if (rst.code != ICAL_UNKNOWN_STATUS){
		
		rst.debug = icalproperty_get_xlicerror(p);
		icalcomponent_add_property(component,
					   icalproperty_new_requeststatus(
					       icalreqstattype_as_string(rst)
					       )
		    );
		
		icalcomponent_remove_property(component,p);
	    }
	}
    }

    for(c = icalcomponent_get_first_component(component,ICAL_ANY_COMPONENT);
	c != 0;
	c = icalcomponent_get_next_component(component,ICAL_ANY_COMPONENT)){
	
	icalcomponent_convert_errors(c);
    }
}


icalcomponent* icalcomponent_get_parent(icalcomponent* component)
{
   struct icalcomponent_impl *c = (struct icalcomponent_impl*)component;

   return c->parent;
}

void icalcomponent_set_parent(icalcomponent* component, icalcomponent* parent)
{
   struct icalcomponent_impl *c = (struct icalcomponent_impl*)component;

   c->parent = parent;
}

icalcompiter icalcompiter_null = {ICAL_NO_COMPONENT,0};

icalcompiter 
icalcomponent_begin_component(icalcomponent* component,icalcomponent_kind kind)
{
    struct icalcomponent_impl *impl = (struct icalcomponent_impl*)component;
    icalcompiter itr;
    pvl_elem i;

    itr.kind = kind;

    icalerror_check_arg_re( (component!=0),"component",icalcompiter_null);

    for( i = pvl_head(impl->components); i != 0; i = pvl_next(itr.iter)) {
	
	icalcomponent *c =  (icalcomponent*) pvl_data(i);
	
	if (icalcomponent_isa(c) == kind || kind == ICAL_ANY_COMPONENT) {
	    
	    itr.iter = i;

	    return itr;
	}
    }

    return icalcompiter_null;;
}

icalcompiter
icalcomponent_end_component(icalcomponent* component,icalcomponent_kind kind)
{
    struct icalcomponent_impl *impl = (struct icalcomponent_impl*)component;
    icalcompiter itr; 
    pvl_elem i;

    itr.kind = kind;

    icalerror_check_arg_re( (component!=0),"component",icalcompiter_null);

    for( i = pvl_tail(impl->components); i != 0; i = pvl_prior(i)) {
	
	icalcomponent *c =  (icalcomponent*) pvl_data(i);
	
	if (icalcomponent_isa(c) == kind || kind == ICAL_ANY_COMPONENT) {
	    
	    itr.iter = pvl_next(i);

	    return itr;
	}
    }

    return icalcompiter_null;;
}


icalcomponent* icalcompiter_next(icalcompiter* i)
{
   if (i->iter == 0){
       return 0;
   }

   for( i->iter = pvl_next(i->iter);
	i->iter != 0;
	i->iter = pvl_next(i->iter)) {
	    
       icalcomponent *c =  (icalcomponent*) pvl_data(i->iter);
	
	   if (icalcomponent_isa(c) == i->kind 
	       || i->kind == ICAL_ANY_COMPONENT) {
	       
	       return icalcompiter_deref(i);;
	   }
   }

   return 0;

}

icalcomponent* icalcompiter_prior(icalcompiter* i)
{
   if (i->iter == 0){
       return 0;
   }

   for( i->iter = pvl_prior(i->iter);
	i->iter != 0;
	i->iter = pvl_prior(i->iter)) {
	    
       icalcomponent *c =  (icalcomponent*) pvl_data(i->iter);
	
	   if (icalcomponent_isa(c) == i->kind 
	       || i->kind == ICAL_ANY_COMPONENT) {
	       
	       return icalcompiter_deref(i);;
	   }
   }

   return 0;

}
icalcomponent* icalcompiter_deref(icalcompiter* i)
{
    if(i->iter ==0){
	return 0;
    }

    return pvl_data(i->iter);
}
