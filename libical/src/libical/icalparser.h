/* -*- Mode: C -*- */
/*======================================================================
  FILE: icalparser.h
  CREATOR: eric 20 April 1999
  
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
  The original code is icalparser.h

======================================================================*/


#ifndef ICALPARSER_H
#define ICALPARSER_H

#include "ical.h"
#include <stdio.h> /* For FILE* */

typedef void* icalparser;
typedef enum icalparser_state {
    ICALPARSER_ERROR,
    ICALPARSER_SUCCESS,
    ICALPARSER_BEGIN_COMP,
    ICALPARSER_END_COMP,
    ICALPARSER_IN_PROGRESS
} icalparser_state;


/***********************************************************************
 * Message oriented parsing.  icalparser_parse takes a string that
 * holds the text ( in RFC 2445 format ) and returns a pointer to an
 * icalcomponent. The caller owns the memory. line_gen_func is a
 * pointer to a function that returns one content line per invocation
 **********************************************************************/

icalcomponent* icalparser_parse(icalparser *parser,
				char* (*line_gen_func)(char *s, size_t size, void *d));

/* A simple, and incorrect interface - can only return one component*/
icalcomponent* icalparser_parse_string(char* str);


/***********************************************************************
 * Line-oriented parsing. 
 * 
 * Create a new parser via icalparse_new_parser, then add ines one at
 * a time with icalparse_add_line(). icalparser_add_line() will return
 * non-zero when it has finished with a component.
 ***********************************************************************/

icalparser* icalparser_new();
void icalparser_set_gen_data(icalparser* parser, void* data);
icalcomponent* icalparser_add_line(icalparser* parser, char* str );
icalcomponent* icalparser_claim(icalparser* parser);
icalcomponent* icalparser_clean(icalparser* parser);
icalparser_state icalparser_get_state(icalparser* parser);
void icalparser_free(icalparser* parser);

/***********************************************************************
 * Parser support functions
 ***********************************************************************/

/* Use the flex/bison parser to turn a string into a value type */
icalvalue*  icalparser_parse_value(icalvalue_kind kind, char* str, icalcomponent** errors);

/* Given a line generator function, return a single iCal content line.*/
char* icalparser_get_line(icalparser* parser, char* (*line_gen_func)(char *s, size_t size, void *d));


/* a line_gen_function that returns lines from a string. To use it,
   set string_line_generator_str to point to the input string, and set
   string_line_generator_pos to 0. These globals make the routine not
   thead-safe.  */

extern char* string_line_generator_str;
extern char* string_line_generator_pos;
char* string_line_generator(char *out, size_t buf_size, void *d);

#endif /* !ICALPARSE_H */
