/* -*- Mode: C -*-
  ======================================================================
  FILE: icalparser.c
  CREATOR: eric 04 August 1999
  
  $Id$
  $Locker$
    
 The contents of this file are subject to the Mozilla Public License
 Version 1.0 (the "License"); you may not use this file except in
 compliance with the License. You may obtain a copy of the License at
 http://www.mozilla.org/MPL/
 
 Software distributed under the License is distributed on an "AS IS"
 basis, WITHOUT WARRANTY OF ANY KIND, either express or implied. See
 the License for the specific language governing rights and
 limitations under the License.
 
 The Original Code is eric. The Initial Developer of the Original
 Code is 

 (C) COPYRIGHT 1999 The Software Studio. 
 http://www.softwarestudio.org

 ======================================================================*/
#include "ical.h"
#include "pvl.h"
#include "icalparser.h"
#include "icalmemory.h"
#include <string.h> /* For strncpy */
#include <stdio.h> /* For FILE and fgets and sprintf */
#include <stdlib.h> /* for free */


extern icalvalue* icalparser_yy_value;
void set_parser_value_state(icalvalue_kind kind);
int icalparser_yyparse(void);

char* icalparser_get_next_char(char c, char *str);
char* icalparser_get_next_parameter(char* line,char** end);
char* icalparser_get_next_value(char* line, char **end, icalvalue_kind kind);
char* icalparser_get_prop_name(char* line, char** end);
char* icalparser_get_param_name(char* line, char **end);

icalvalue* icalvalue_new_from_string_with_error(icalvalue_kind kind, char* str, icalproperty **error);



char* icalparser_get_next_char(char c, char *str)
{
    int quote_mode = 0;
    char* p;
    

    for(p=str; *p!=0; p++){
	
	if ( quote_mode == 0 && *p=='"' && *(p-1) != '\\' ){
	    quote_mode =1;
	    continue;
	}

	if ( quote_mode == 1 && *p=='"' && *(p-1) != '\\' ){
	    quote_mode =0;
	    continue;
	}

	if (quote_mode == 0 &&  *p== c  && *(p-1) != '\\' ){
	    return p;
	}

    }

    return 0;
}

/* make a new tmp buffer out of a substring */
char* make_segment(char* start, char* end)
{
    char *buf;
    size_t size = (size_t)end - (size_t)start;
    
    buf = icalmemory_tmp_buffer(size+1);
    

    strncpy(buf,start,size);
    *(buf+size) = 0;
    
    return buf;
    
}

char* input_buffer;
char* input_buffer_p;
#define min(a,b) ((a) < (b) ? (a) : (b))   

int icalparser_flex_input(char* buf, int max_size)
{
    int n = min(max_size,strlen(input_buffer_p));

    if (n > 0){
	memcpy(buf, input_buffer_p, n);
	input_buffer_p += n;
	return n;
    } else {
	return 0;
    }
}

void icalparser_clear_flex_input()
{
    input_buffer_p = input_buffer+strlen(input_buffer);
}

/* Cal the flex parser to parse a complex value */

icalvalue*  icalparser_parse_value(icalvalue_kind kind,
				       char* str, icalproperty** error)
{
    int r;
    input_buffer_p = input_buffer = str;

    set_parser_value_state(kind);
    icalparser_yy_value = 0;

    r = icalparser_yyparse();

    /* Error. Parse failed */
    if( icalparser_yy_value == 0 || r != 0){

	if(icalparser_yy_value !=0){
	    icalvalue_free(icalparser_yy_value);
	    icalparser_yy_value = 0;
	}

	return 0;
    }

    if (error != 0){
	*error = 0;
    }

    return icalparser_yy_value;
}

char* icalparser_get_prop_name(char* line, char** end)
{
    char* p;
    char* v;
    char *str;

    p = icalparser_get_next_char(';',line); 
    v = icalparser_get_next_char(':',line); 
    if (p== 0 && v == 0) {
	return 0;
    }

    /* There is no ';' or, it is after the ';' that marks the beginning of
       the value */
    if (v!=0 && ( p == 0 || p > v)){
	str = make_segment(line,v);
	*end = v+1;
    } else {
	str = make_segment(line,p);
	*end = p+1;
    }

    return str;
}

char* icalparser_get_param_name(char* line, char **end)
{
    
    char* next; 
    char *str;

    next = icalparser_get_next_char('=',line);

    if (next == 0) {
       return 0;
    }

    str = make_segment(line,next);
    *end = next+1;
    return str;
   
}

char* icalparser_get_next_paramvalue(char* line, char **end)
{
    
    char* next; 
    char *str;

    next = icalparser_get_next_char(',',line);

    if (next == 0){
	next = (char*)(size_t)line+(size_t)strlen(line);\
    }

    if (next == line){
	return 0;
    } else {
	str = make_segment(line,next);
	*end = next+1;
	return str;
    }
   
}

/* A property may have multiple values, if the values are seperated by
   commas in the content line. This routine will look for the next
   comma after line and will set the next place to start searching in
   end. */

char* icalparser_get_next_value(char* line, char **end, icalvalue_kind kind)
{
    
    char* next;
    char *p;
    char *str;
    size_t length = strlen(line);

    p = line;
    while(1){

      next = icalparser_get_next_char(',',p);

      /* Unforunately, RFC2445 says that for the RECUR value, COMMA
         can both seperate digits in a list, and it can seperate
         multiple recurrence specifications. This is not a friendly
         part of the spec. This weirdness tries to
         distinguish the two uses. it is probably a HACK*/
      
      if( kind == ICAL_RECUR_VALUE ) {
	  if ( next != 0 &&
	       (*end+length) > next+5 &&
	       strncmp(next,"FREQ",4) == 0
	      ) {
	      /* The COMMA was followed by 'FREQ', is it a real seperator*/
	      /* Fall through */
	      printf("%s\n",next);
	  } else if (next != 0){
	      /* Not real, get the next COMMA */
	      p = next+1;
	      next = 0;
	      continue;
	  } 
      }

      /* If the comma is preceeded by a '\', then it is a literal and
	 not a value seperator*/  
      
      if ( (next!=0 && *(next-1) == '\\') ||
	   (next!=0 && *(next-3) == '\\')
	  ) 
	      /*second clause for '/' is on prev line. HACK may be out of bounds */
      {
	  p = next+1;
      } else {
	  break;
      }

    }

    if (next == 0){
	next = (char*)(size_t)line+length;
        *end = next;
    } else {
      *end = next+1;
    }

    if (next == line){
	return 0;
    } 
	

    str = make_segment(line,next);
    return str;
   
}

char* icalparser_get_next_parameter(char* line,char** end)
{
    char *next;
    char *v;
    char *str;

    v = icalparser_get_next_char(':',line); 
    next = icalparser_get_next_char(';', line);
    
    /* There is no ';' or, it is after the ':' that marks the beginning of
       the value */

    if (next == 0 || next > v) {
	next = icalparser_get_next_char(':', line);
    }

    if (next != 0) {
	str = make_segment(line,next);
	*end = next+1;
	return str;
    } else {
	*end = line;
	return 0;
    }
}

/* HACK. This is not threadsafe */
int buffer_full=0;
size_t tmp_buf_size = 80; 
char temp[80];


/* Get a single property line, from the property name through the
   final new line, and include any continuation lines */

char* icalparser_get_line(char* (*line_gen_func)(char *s, size_t size, void *d), int *lineno)
{
    char *line;
    char *line_p;
    size_t buf_size = tmp_buf_size;
  

    line_p = line = icalmemory_new_buffer(buf_size);
    line[0] = '\0';
   
    while(1) {

	/* The buffer is not clear, so transfer the data in it to the
	   output. This may be left over from a previous call */
	if (temp[0] != '\0' ) {
	    if (temp[tmp_buf_size-1] == 0){
		buffer_full = 1;
	    } else {
		buffer_full = 0;
	    }
	    icalmemory_append_string(&line,&line_p,&buf_size,temp);     
	    temp[0] = '\0' ;
	}
	
	temp[tmp_buf_size-1] = 1; /* Mark end of buffer */

	if ((*line_gen_func)(temp,tmp_buf_size,0)==0){/* Get more data */
	    if (temp[0] == '\0'){
		if(line[0] != '\0'){
		    break;
		} else {
		    free(line);
		    return 0;
		}
	    }
	}

	if  ( line_p > line+1 && *(line_p-1) == '\n' && temp[0] == ' ') {

	    /* If the output line ends in a '\n' and the temp buffer
	       begins with a ' ', then the buffer holds a continuation
	       line, so keep reading.  */

	    /* back up the pointer to erase the continuation characters */
	    line_p--;

	    /* shift the temp buffer down to eliminate the leading space*/
	    memmove(&(temp[0]),&(temp[1]),tmp_buf_size);
	    temp[tmp_buf_size-1] = temp[tmp_buf_size-2];

	} else if ( buffer_full == 1 ) {
	    
	    /* The buffer was filled on the last read, so read again */

	} else {

	    /* Looks like the end of this content line, so break */
	    break;
	}

	
    }

    /* Erase the final newline */
    if ( line_p > line+1 && *(line_p-1) == '\n') {

	*(line_p-1) = '\0';
    } else {
	*(line_p) = '\0';
    }

    return line;

}

#if 0
char* icalparser_old_get_line(char* (*line_gen_func)(char *s, size_t size, void *d), int *lineno)
{
    char *line, *output_line;
    char *line_p;
    char last_line = 0;
    size_t buf_size = tmp_buf_size;
    int break_flag = 0;

    line_p = line = icalmemory_new_buffer(buf_size);
    
    /* If the hold buffer is empty, read a line. Otherwise, use the data that
     is still in 'temp'*/
    if (hold == 1){
	/* Do nothing */
    } else {
	temp[tmp_buf_size-1] = 1; /* Mark end of buffer */
	(*line_gen_func)(temp,tmp_buf_size,0);
    }

    /* Append the hold buffer or new line into the output */
    icalmemory_append_string(&line,&line_p,&buf_size,temp);
    
    if ( temp[tmp_buf_size-1] == 0 ) { /* Check if mark was overwritten */
	buffer_full = 1;
    } else {
	buffer_full = 0;
    }

    /* Try to suck up any continuation lines */
    while(last_line == 0){

	temp[tmp_buf_size-1] = 1; /* Mark end of buffer */
	if ((*line_gen_func)(temp,tmp_buf_size,0) == 0){
	    /* No more lines -- we are finished */
	    if (hold == 1) {
		hold = 0;
		break_flag = 1;
	    } else {
		icalmemory_free_buffer(line);
		return 0;
	    }
	} else {
	    if ( temp[tmp_buf_size-1] == 0 ) {
		buffer_full = 1;
	    } else {
		buffer_full = 0;
	    }	
	}

	/* keep track of line numbers */
	if (lineno != 0){ 
	    (*lineno)++;
	}


	/* Determine wether to copy the line to the output, or
	   save it in the hold buffer */

	if  ( line_p > line+1 && *(line_p-1) == '\n' && temp[0] == ' ') {

	    /* If the last line ( in the 'line' string ) ends in a '\n'
	       and the current line begins with a ' ', then the current line
	       is a continuation line, so append it. */

	    /* back up the pointer to erase the continuation characters */
	    line_p--;
	    icalmemory_append_string(&line,&line_p,&buf_size,&(temp[1]));

	    hold = 0;
	    buffer_full= 0;
	    
	} else if (buffer_full == 1) {
	    
	    /* The last line that was read filled up the read
               buffer. Append it and read again */

	    icalmemory_append_string(&line,&line_p,&buf_size,temp);

	    hold = 0;

	} else if (break_flag != 1 ){
	    /* Nope -- the line was not a continuation line. 
	       Save the line for the next call */

	    hold =1;
	    break; 
	} else {
	    break;
	}
    } 

    /* Erase the final newline */
    if ( line_p > line+1 && *(line_p-1) == '\n') {

	*(line_p-1) = '\0';
    }

    output_line = icalmemory_tmp_copy(line);
    icalmemory_free_buffer(line);

   return output_line;
}
#endif

void insert_error(icalcomponent* comp, char* text, 
		  char* message, icalparameter_xlicerrortype type)
{
    char temp[1024];
    
    if (strlen(text) > 256) {
	sprintf(temp,"%s: \'%256s...\'",message,text);
    } else {
	sprintf(temp,"%s: \'%s\'",message,text);
    }	
    
    icalcomponent_add_property
	(comp,
	 icalproperty_vanew_xlicerror(
	     temp,
	     icalparameter_new_xlicerrortype(type),
	     0));   
}

icalcomponent* icalparser_parse(char* (*line_gen_func)(char *s, size_t size, void* d))
{

    char *line = 0;
    char *p; 
    char *str;
    char *end;
    int lineno = 0;

    int vcount = 0;

    icalcomponent *root_component = 0;
    icalcomponent *tail  = 0;
    icalproperty *prop;
    icalvalue *value;

    icalvalue_kind value_kind;

    pvl_list components = pvl_newlist();

    do {

	value_kind = ICAL_NO_VALUE;

	/* Get a single property line, from a property name through a
           newline */
	if (line!=0){
	    free(line);
	}

	line = icalparser_get_line(line_gen_func,&lineno);

	if (line == 0){
	    continue;
	}

	end = 0;

	str = icalparser_get_prop_name(line, &end);

	if (str == 0){
	    tail = pvl_data(pvl_tail(components));

	    if (tail){
		insert_error(tail,line,
			     "Got a data line, but could not find a property name or component begin tag",
			     ICAL_XLICERRORTYPE_COMPONENTPARSEERROR);
	    }
	    tail = 0;
	    continue; 
	}

/**********************************************************************
 * Handle begin and end of components
 **********************************************************************/								       


	/* If the property name is BEGIN or END, we are actually
           starting or ending a new component */

	if(strcmp(str,"BEGIN") == 0){
	    icalcomponent *c;      ;

	    str = icalparser_get_next_value(end,&end, value_kind);
	    
	    c  = icalcomponent_new_from_string(str);

	    if (c == 0){
		c = icalcomponent_new(ICAL_XLICINVALID_COMPONENT);
		insert_error(c,str,"Parse error in component name",
			     ICAL_XLICERRORTYPE_COMPONENTPARSEERROR);
	    }
	    
	    pvl_push(components,c);

	    continue;
	} else if (strcmp(str,"END") == 0 ) {

	    str = icalparser_get_next_value(end,&end, value_kind);

	    root_component = pvl_pop(components);

	    tail = pvl_data(pvl_tail(components));

	    if(tail != 0){
		icalcomponent_add_component(tail,root_component);
	    }

	    tail = 0;
	    continue;
	}


	/* There is no point in continuing if we have not seen a
           component yet */

	if(pvl_data(pvl_tail(components)) == 0){
	    continue;
	}


/**********************************************************************
 * Handle property names
 **********************************************************************/								       
	/* At this point, the property name really is a property name,
           (Not a component name) so make a new property and add it to
           the component */

	prop = icalproperty_new_from_string(str);

	if (prop != 0){
	    tail = pvl_data(pvl_tail(components));

	    icalcomponent_add_property(tail, prop);

	    /* Set the value kind for the default for this type of
               property. This may be re-set by a VALUE parameter */
	    value_kind = 
		icalenum_property_kind_to_value_kind(
		    icalproperty_isa(prop));
	} else {
	    icalcomponent* tail = pvl_data(pvl_tail(components));

	    insert_error(tail,str,"Parse error in property name",
			 ICAL_XLICERRORTYPE_PROPERTYPARSEERROR);
	    
	    tail = 0;
	    continue;
	}

/**********************************************************************
 * Handle parameter values
 **********************************************************************/								       

	/* Now, add any parameters to the last property */

	p = 0;
	while(1) {

	    if (*(end-1) == ':'){
		/* if the last seperator was a ":" and the value is a
                   URL, icalparser_get_next_parameter will find the
                   ':' in the URL, so better break now. */
		break;
	    }

	    str = icalparser_get_next_parameter(end,&end);

	    if (str != 0){
		char* name;
		char* pvalue;
		icalparameter *param = 0;
		icalparameter_kind kind;
		
		tail = pvl_data(pvl_tail(components));

		name = icalparser_get_param_name(str,&pvalue);

		if (name == 0){
		    insert_error(tail, str, "Can't parse parameter name",
				 ICAL_XLICERRORTYPE_PARAMETERPARSEERROR);
		    tail = 0;
		    break;
		}

		kind = icalenum_string_to_parameter_kind(name);
		if (kind != ICAL_NO_PARAMETER){
		    param = icalparameter_new_from_string(kind,pvalue);
		} else {

		    /* Error. Failed to parse the parameter*/
		    insert_error(tail, str, "Can't parse parameter name",
				 ICAL_XLICERRORTYPE_PARAMETERPARSEERROR);
		    tail = 0;
		    continue;
		}

		if (param == 0){
		    insert_error(tail,str,"Can't parse parameter value",
				 ICAL_XLICERRORTYPE_PARAMETERPARSEERROR);
		    
		    tail = 0;
		    continue;
		}

		/* If it is a VALUE parameter, set the kind of value*/
		if (icalparameter_isa(param)==ICAL_VALUE_PARAMETER){

		    value_kind = (icalvalue_kind)
			icalparameter_get_value(param);

		    if (value_kind == ICAL_NO_VALUE){

			/* Ooops, could not parse the value of the
                           parameter ( it was not one of the defined
                           values ), so reset the value_kind */
			
			icalcomponent* tail 
			    = pvl_data(pvl_tail(components));
			
			insert_error(
			    tail, str, 
			    "Got a VALUE parameter with an unknown type",
			    ICAL_XLICERRORTYPE_PARAMETERPARSEERROR);
			icalparameter_free(param);
			
			value_kind = 
			    icalenum_property_kind_to_value_kind(
				icalproperty_isa(prop));
			
			icalparameter_free(param);
			tail = 0;
			continue;
		    } 
		}

		/* Everything is OK, so add the parameter */
		icalproperty_add_parameter(prop,param);
		tail = 0;

	    } else {
		/* If we did not get a param string, go on to looking
		   for a value */
		break;
	    }
	}	    
	
/**********************************************************************
 * Handle values
 **********************************************************************/								       

	/* Look for values. If there are ',' characters in the values,
           then there are multiple values, so clone the current
           parameter and add one part of the value to each clone */

	vcount=0;
	while(1) {
	    str = icalparser_get_next_value(end,&end, value_kind);

	    if (str != 0){
		
		if (vcount > 0){
		    /* Actually, only clone after the second value */
		    icalproperty* clone = icalproperty_new_clone(prop);
		    tail = pvl_data(pvl_tail(components));
		    
		    icalcomponent_add_property(tail, clone);
		    prop = clone;		    
		    tail = 0;
		}
		
		value = icalvalue_new_from_string(value_kind, str);
		
		/* Don't add properties without value */
		if (value == 0){
		    char temp[1024];

		    icalproperty_kind prop_kind = icalproperty_isa(prop);
		    tail = pvl_data(pvl_tail(components));

		    sprintf(temp,"Can't parse as %s value in %s property. Removing entire property",
			    icalenum_value_kind_to_string(value_kind),
			    icalenum_property_kind_to_string(prop_kind));

		    insert_error(tail, str, temp,
				 ICAL_XLICERRORTYPE_VALUEPARSEERROR);

		    /* Remove the troublesome property */
		    icalcomponent_remove_property(tail,prop);
		    icalproperty_free(prop);
		    prop = 0;
		    tail = 0;
		    break;
		    
		} else {
		    vcount++;
		    icalproperty_set_value(prop, value);
		}


	    } else {
		
		break;
	    }
	}
	
    } while( !feof(stdin) && line !=0 );
    
    
    if (pvl_data(pvl_tail(components)) == 0){
	/* A nice, clean exit */
	pvl_free(components);
	free(line);
	return root_component;
    }

    /* Clear off any component that may be left in the list */
    /* This will happen if some components did not have an "END" tag*/

    while((tail=pvl_data(pvl_tail(components))) != 0){

	insert_error(tail," ",
		     "Missing END tag for this component. Closing component at end of input.",
		     ICAL_XLICERRORTYPE_COMPONENTPARSEERROR);
	

	root_component = pvl_pop(components);
	tail=pvl_data(pvl_tail(components));

	if(tail != 0){
	    icalcomponent_add_component(tail,root_component);
	}
    }

    free(line);
    return root_component;
}

char* string_line_generator_pos=0;
char* string_line_generator_str;
char* string_line_generator(char *out, size_t buf_size, void *d)
{
    char *n;
    
    if(string_line_generator_pos==0){
	string_line_generator_pos=string_line_generator_str;
    }

    /* If the pointer is at the end of the string, we are done */
    if (*string_line_generator_pos ==0){
	return 0;
    }

    n = strchr(string_line_generator_pos,'\n');
    
    /* If no newline, take the rest of the string, and leave the
       pointer at the \0 */

    if (n == 0) {
	n = string_line_generator_pos + strlen(string_line_generator_pos);
    } else {
	n++;
    }

    strncpy(out,string_line_generator_pos,(n-string_line_generator_pos));
    
    *(out+(n-string_line_generator_pos)) = '\0';
    
    string_line_generator_pos = n;

    return out;
    
}

void _test_string_line_generator(char* str)
{
    char *line;
    int lineno=0; 
    string_line_generator_str = str;
    string_line_generator_pos = 0;
    
    while((line = icalparser_get_line(string_line_generator,&lineno))){
	printf("#%d: %s\n",lineno,line);
    }
    
    
    string_line_generator_pos = 0;
    string_line_generator_str = 0;
}



icalcomponent* icalparser_parse_string(char* str)
{
    
    icalcomponent *c;
    
    string_line_generator_str = str;
    string_line_generator_pos = 0;
    c = icalparser_parse(string_line_generator);
    string_line_generator_pos = 0;
    string_line_generator_str = 0;
    
    return c;

}
