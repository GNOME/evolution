%{
/* -*- Mode: C -*-
  ======================================================================
  FILE: icalssyacc.y
  CREATOR: eric 08 Aug 2000
  
  DESCRIPTION:
  
  $Id: icalssyacc.y,v 1.1 2000/12/11 22:06:18 federico Exp $
  $Locker:  $

(C) COPYRIGHT 2000, Eric Busboom, http://www.softwarestudio.org

 This program is free software; you can redistribute it and/or modify
 it under the terms of either: 

    The LGPL as published by the Free Software Foundation, version
    2.1, available at: http://www.fsf.org/copyleft/lesser.html

  Or:

    The Mozilla Public License Version 1.0. You may obtain a copy of
    the License at http://www.mozilla.org/MPL/

 The Original Code is eric. The Initial Developer of the Original
 Code is Eric Busboom

  ======================================================================*/

#include <stdlib.h>
#include <string.h> /* for strdup() */
#include <limits.h> /* for SHRT_MAX*/
#include "ical.h"
#include "pvl.h"
#include "icalgaugeimpl.h"


extern struct icalgauge_impl *icalss_yy_gauge;

void ssyacc_add_where(struct icalgauge_impl* impl, char* str1, 
	enum icalparameter_xliccomparetype compare , char* str2);
void ssyacc_add_select(struct icalgauge_impl* impl, char* str1);
void ssyacc_add_from(struct icalgauge_impl* impl, char* str1);
void move_where(int w);
void sserror(char *s); /* Don't know why I need this.... */



%}

%union {
	char* v_string;
}


%token <v_string> STRING
%token SELECT FROM WHERE COMMA EQUALS NOTEQUALS  LESS GREATER LESSEQUALS
%token GREATEREQUALS AND OR EOL END

%%

query_min: SELECT select_list FROM from_list WHERE where_list
	   | error { 
		 icalparser_clear_flex_input();
                 yyclearin;
           }	
	   ;	

select_list:
	STRING {ssyacc_add_select(icalss_yy_gauge,$1);}
	| select_list COMMA STRING {ssyacc_add_select(icalss_yy_gauge,$3);}
	;


from_list:
	STRING {ssyacc_add_from(icalss_yy_gauge,$1);}
	| from_list COMMA STRING {ssyacc_add_from(icalss_yy_gauge,$3);}
	;

where_clause:
	/* Empty */
	| STRING EQUALS STRING {ssyacc_add_where(icalss_yy_gauge,$1,ICAL_XLICCOMPARETYPE_EQUAL,$3); }
	
	| STRING NOTEQUALS STRING {ssyacc_add_where(icalss_yy_gauge,$1,ICAL_XLICCOMPARETYPE_NOTEQUAL,$3); }
	| STRING LESS STRING {ssyacc_add_where(icalss_yy_gauge,$1,ICAL_XLICCOMPARETYPE_LESS,$3); }
	| STRING GREATER STRING {ssyacc_add_where(icalss_yy_gauge,$1,ICAL_XLICCOMPARETYPE_GREATER,$3); }
	| STRING LESSEQUALS STRING {ssyacc_add_where(icalss_yy_gauge,$1,ICAL_XLICCOMPARETYPE_LESSEQUAL,$3); }
	| STRING GREATEREQUALS STRING {ssyacc_add_where(icalss_yy_gauge,$1,ICAL_XLICCOMPARETYPE_GREATEREQUAL,$3); }
	;

where_list:
	where_clause {move_where(1);}
	| where_list AND where_clause {move_where(2);} 
	| where_list OR where_clause {move_where(3);}
	;


%%

void ssyacc_add_where(struct icalgauge_impl* impl, char* str1, 
	enum icalparameter_xliccomparetype compare , char* str2)
{
    icalproperty *p;
    icalvalue *v;
    icalproperty_kind kind;
    
    kind = icalenum_string_to_property_kind(str1);
    
    if(kind == ICAL_NO_PROPERTY){
	assert(0);
    }

    p = icalproperty_new(kind);

    v = icalvalue_new_text(str2);

    if(v == 0){
	assert(0);
    }

    icalproperty_set_value(p,v);

    icalproperty_add_parameter(p,icalparameter_new_xliccomparetype(compare));

    icalcomponent_add_property(impl->where,p);
}

void ssyacc_add_select(struct icalgauge_impl* impl, char* str1)
{
    icalproperty *p;
    icalproperty_kind pkind;
    icalcomponent_kind ckind = ICAL_NO_COMPONENT;
    char* c;
    char* compstr;
    char* propstr;
    
    /* Is there a period in str1 ? If so, the string specified both a
       component and a property*/
    if( (c = strrchr(str1,'.')) != 0){
	compstr = str1;
	propstr = c+1;
	*c = '\0';
    } else {
	compstr = 0;
	propstr = str1;
    }


    /* Handle the case where a component was specified */
    if(compstr != 0){
	ckind = icalenum_string_to_component_kind(compstr);
	
	if(ckind == ICAL_NO_COMPONENT){
	    assert(0);
	}
    } else {
	ckind = ICAL_NO_COMPONENT;
    }


    /* If the property was '*', then accept all properties */
    if(strcmp("*",propstr) == 0) {
	pkind = ICAL_ANY_PROPERTY; 	    
    } else {
	pkind = icalenum_string_to_property_kind(str1);    
    }
    

    if(pkind == ICAL_NO_PROPERTY){
	assert(0);
    }


    if(ckind == ICAL_NO_COMPONENT){
	p = icalproperty_new(pkind);
	assert(p!=0);
	icalcomponent_add_property(impl->select,p);

    } else {
	icalcomponent *comp = 
	    icalcomponent_new(ckind);
	p = icalproperty_new(pkind);

	assert(p!=0);

	icalcomponent_add_property(comp,p);
	icalcomponent_add_component(impl->select,comp);
	
    }
}

void ssyacc_add_from(struct icalgauge_impl* impl, char* str1)
{
    icalcomponent *c;
    icalcomponent_kind ckind;

    ckind = icalenum_string_to_component_kind(str1);

    if(ckind == ICAL_NO_COMPONENT){
	assert(0);
    }

    c = icalcomponent_new(ckind);

    icalcomponent_add_component(impl->from,c);

}

void move_where(int w)
{
}

void sserror(char *s){
    fprintf(stderr,"Parse error \'%s\'\n", s);
}
