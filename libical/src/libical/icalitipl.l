%{
/* -*- Mode: C -*-
  ======================================================================
  FILE: icalitip.y
  CREATOR: eric 10 June 1999
  
  DESCRIPTION:
  
  $Id: icalitipl.l,v 1.3 2000/05/15 06:18:17 ericb Exp $
  $Locker:  $

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
  The original code is icalitip.y



  ======================================================================*/
#include "icalitipy.h"
#include "icalparser.h"
#include "icalenums.h"
#include "icalmemory.h"
#include "assert.h"

#include <string.h> /* For strdup() */

#define ICAL_MAX_STR_CONST 1024

#undef YY_INPUT
#define YY_INPUT(b,r,ms) ( r= icalparser_flex_input(b,ms))
#undef yywrap

#undef YY_FATAL_ERROR
#define YY_FATAL_ERROR(msg) icalparser_yyerror(msg)

icalvalue_kind value_kind=ICAL_NO_VALUE;
void set_parser_value_state();
extern enum icalparser_state_enum icalparser_state;
extern int yydebug; 

void icalparser_yyerror(char *s);

void init_str_buf();

int last_state;

char *str_buf;
char *str_buf_p;
size_t buf_sz; /* = ICAL_MAX_STR_CONST;*/


%}

crlf		\x0D\x0A
nl              \n
lbreak		\x0D\x0A\x20 
ulbreak		\x0A\x20 
space		[ ]
qsafechar	[^\x00-\x1F\"]
safechar	[^\x00-\x1F\"\:\;\,]
tsafechar	[\x20-\x21\x23-\x2B\x2D-\x39\x3C-\x5B\x5D-\x7E]
valuechar	[^\x00-\x08\x10-\x1F]
xname		X-[a-zA-Z0-9\-]+
xname2          [a-zA-Z0-9\-\ ]
paramtext	{safechar}+
value		{valuechar}+
quotedstring	\"{qsafechar}+\"
digit		[0-9]

%array /* Make yytext an array. Slow, but handy. HACK */

%option caseless


%s quoted_string
%s binary_value boolean_value uri_value time_value duration_value number_value period_value recur_value text_value utcoffset_value
%s enum_param_value string_param_value stringlist_param_value keyword line_start component seperator parameter end_of_value paramtext



%%

%{
%}


<binary_value>{
.*			 { icalparser_yylval.v_string= icalmemory_tmp_copy(yytext);  
			   return STRING;}
{crlf}|\n                { return EOL;}

}

<boolean_value>{
.			 { return CHARACTER; }
{crlf}|\n                { return EOL;}

}

<uri_value>{
.*			 { icalparser_yylval.v_string= icalmemory_tmp_copy(yytext);  
			   return STRING;}
{crlf}|\n                { return EOL;}

}


<time_value>{
{digit}*		 { icalparser_yylval.v_string= icalmemory_tmp_copy(yytext);
                           return DIGITS; }
T			 { return TIME_CHAR; }
Z			 { return UTC_CHAR; }
{crlf}|\n                { return EOL;}
}

<duration_value>{
{digit}+		 { icalparser_yylval.v_string =icalmemory_tmp_copy(yytext); 
                           return DIGITS; }
T			 { return TIME_CHAR; }
[\+\-PTWHMSD]		 { return yytext[0]; }
{crlf}|\n	         { return EOL;}

}

<number_value>{
[\+\-\.0-9]+		 {  icalparser_yylval.v_int=atoi(yytext); return INTNUMBER; }
{crlf}|\n                { return EOL;}
}

<period_value>{
{digit}+		 { icalparser_yylval.v_string =icalmemory_tmp_copy(yytext) ;
                           return DIGITS; }
T			 { return TIME_CHAR; }
Z			 { return UTC_CHAR; }
[\/\+\-PWHMSD]		 { return yytext[0]; }
{crlf}|\n                { return EOL;}

}

<recur_value>{
INTERVAL                 { return INTERVAL; }
COUNT                    { return COUNT; }
UNTIL                    { return UNTIL; }
FREQ                     { return FREQ; }
BYDAY                    { return BYDAY; }
BYHOUR                   { return BYHOUR; }
BYMINUTE                 { return BYMINUTE; }
BYMONTH                  { return BYMONTH; }
BYMONTHDAY               { return BYMONTHDAY; }
BYSECOND                 { return BYSECOND; }
BYSETPOS                 { return BYSETPOS; }
BYWEEKNO                 { return BYWEEKNO; }
BYYEARDAY                { return BYYEARDAY; }
DAILY                    { return DAILY; }
SECONDLY                 { return SECONDLY; }
MINUTELY                 { return MINUTELY; }
HOURLY                   { return HOURLY; }
MONTHLY                  { return MONTHLY; }
WEEKLY                   { return WEEKLY; }
YEARLY                   { return YEARLY; }
WKST                     { return WKST; }
MO                       { return MO; }
SA                       { return SA; }
SU                       { return SU; }
TU                       { return TU; }
WE                       { return WE; }
TH                       { return TH; }
FR                       { return FR; }
=                        { return EQUALS; }
,                        { return COMMA; }
;                        { return SEMICOLON; }
[\-\+0-9]+		 { icalparser_yylval.v_string= icalmemory_tmp_copy(yytext);
                           return DIGITS; }
T			 { return TIME_CHAR; }
Z			 { return UTC_CHAR; }
{crlf}|\n                { return EOL;}
}

<utcoffset_value>{
{crlf}|\n                { return EOL;}
\-|\+			 { return yytext[0]; }
{digit}{digit}		 { icalparser_yylval.v_int=atoi(yytext); return INTNUMBER; }

}

<enum_param_value>{
.			 { return CHARACTER; }
{crlf}|\n            { return EOL;}

}

<seperator>{
,       { BEGIN(last_state); return COMMA; } 
}


%% 

int yywrap()
{
     return 1;
}


void set_parser_value_state(icalvalue_kind kind)
{

    switch (kind){

        case ICAL_ATTACH_VALUE:
	case ICAL_BINARY_VALUE:
	    {BEGIN(binary_value);break;}

	case ICAL_BOOLEAN_VALUE:
	case ICAL_INTEGER_VALUE:
	case ICAL_FLOAT_VALUE:
	    {BEGIN(number_value);break;}

	case ICAL_UTCOFFSET_VALUE:
	    {BEGIN(utcoffset_value);break;}

	case ICAL_TEXT_VALUE:
	    {BEGIN(text_value);
             init_str_buf();
             break;}

	case ICAL_CALADDRESS_VALUE:
	case ICAL_URI_VALUE:
	    {BEGIN(uri_value);break;}

	case ICAL_DATE_VALUE:
	case ICAL_DATETIME_VALUE:
	case ICAL_DATETIMEDATE_VALUE:
	case ICAL_DATETIMEPERIOD_VALUE:
	case ICAL_TIME_VALUE:
	    {BEGIN(time_value);break;}

	case ICAL_DURATION_VALUE:
             {BEGIN(duration_value);break;}

	case ICAL_PERIOD_VALUE:
            {BEGIN(period_value);break;}

	case ICAL_GEO_VALUE:
        case ICAL_QUERY_VALUE:
	    {BEGIN(text_value);break;}

	case ICAL_RECUR_VALUE:
	    {BEGIN(recur_value);break;}

	case ICAL_NO_VALUE:
	{
	  /* The value is probably actually a component name */
            {BEGIN(component); break;}
	}
	default:
	{
	   assert(1==0);
	}
    }
}

void init_str_buf()
{
   str_buf = icalmemory_tmp_buffer(ICAL_MAX_STR_CONST);
   str_buf_p = str_buf;
   buf_sz = ICAL_MAX_STR_CONST;


}

