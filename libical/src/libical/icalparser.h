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

/***********************************************************************
 * Message oriented parsing.  icalparser_parse takes a string that
 * holds the text ( in RFC 2445 format ) and returns a pointer to an
 * icalcomponent. The caller owns the memory. line_gen_func is a
 * pointer to a function that returns one content line per invocation
 **********************************************************************/

icalcomponent* icalparser_parse(char* (*line_gen_func)());

/* Parse directly from a string */
icalcomponent* icalparser_parse_string(char* str);

/*  icalparser_flex_input is the routine that is called from the macro
    YYINPUT in the flex lexer. */
int icalparser_flex_input(char* buf, int max_size);
void icalparser_clear_flex_input();

/***********************************************************************
 * Line-oriented parsing. 
 * 
 * Create a new parser via icalparse_new_parser, then add ines one at
 * a time with icalparse_add_line(). After adding the last line, call
 * icalparse_close() to return the parsed component.
 ***********************************************************************/

/* These are not implemented yet */
typedef void* icalparser;
icalparser icalparse_new_parser();
void icalparse_add_line(icalparser* parser );
icalcomponent* icalparse_close(icalparser* parser);

/***********************************************************************
 * Parser support functions
 ***********************************************************************/

/* Use the flex/bison parser to turn a string into a value type */
icalvalue*  icalparser_parse_value(icalvalue_kind kind, char* str, icalcomponent** errors);

char* icalparser_get_line(char* (*line_gen_func)(char *s, size_t size, void *d), int *lineno);



/* a line_gen_function that returns lines from a string */


#endif /* !ICALPARSE_H */
