%{
/* -*- Mode: C -*-
  ======================================================================
  FILE: icalitip.y
  CREATOR: eric 10 June 1999
  
  DESCRIPTION:
  
  $Id: icalitip.y,v 1.1.1.2 2000/02/21 13:19:26 alves Exp $
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



  ================================b======================================*/

#include <stdlib.h>
#include <string.h> /* for strdup() */
#include <limits.h> /* for SHRT_MAX*/
#include "icalparser.h"
#include "ical.h"
#include "pvl.h"

#define YYERROR_VERBOSE
#define YYDEBUG 1 

icalvalue *icalparser_yy_value; /* Current Value */

/* Globals for UTCOFFSET values */
int utc; 
int utc_b; 
int utcsign;

/* Globals for DURATION values */
struct icaldurationtype duration;

/* Globals for RECUR values */
struct icalrecurrencetype recur;
short skiplist[367];
short skippos;

void copy_list(short* array, size_t size);
void clear_recur();
void add_prop(icalproperty_kind);
void icalparser_fill_date(struct tm* t, char* dstr);
void icalparser_fill_time(struct tm* t, char* tstr);
void set_value_type(icalvalue_kind kind);
void yyerror(char *s); /* Don't know why I need this.... */
int yylex(void); /* Or this. */
void set_parser_value_state();
struct icaltimetype fill_datetime(char* d, char* t);



/* Set the state of the lexer so it will interpret values ( iCAL
   VALUEs, that is, ) correctly. */

%}

%union {
	float v_float;
	int   v_int;
	char* v_string;

}

%token <v_string> DIGITS
%token <v_int> INTNUMBER
%token <v_float> FLOATNUMBER
%token <v_string> STRING   
%token EOL EQUALS CHARACTER COLON COMMA SEMICOLON TIMESEPERATOR 

%token TRUE FALSE

%token FREQ BYDAY BYHOUR BYMINUTE BYMONTH BYMONTHDAY BYSECOND BYSETPOS BYWEEKNO
%token BYYEARDAY DAILY MINUTELY MONTHLY SECONDLY WEEKLY HOURLY YEARLY
%token INTERVAL COUNT UNTIL WKST MO SA SU TU WE TH FR 

%token BIT8 ACCEPTED ADD AUDIO BASE64 BINARY BOOLEAN BUSY BUSYTENTATIVE
%token BUSYUNAVAILABLE CALADDRESS CANCEL CANCELLED CHAIR CHILD COMPLETED
%token CONFIDENTIAL CONFIRMED COUNTER DATE DATETIME DECLINECOUNTER DECLINED
%token DELEGATED DISPLAY DRAFT DURATION EMAIL END FINAL FLOAT FREE GREGORIAN
%token GROUP INDIVIDUAL INPROCESS INTEGER NEEDSACTION NONPARTICIPANT
%token OPAQUE OPTPARTICIPANT PARENT PERIOD PRIVATE PROCEDURE PUBLIC PUBLISH
%token RECUR REFRESH REPLY REQPARTICIPANT REQUEST RESOURCE ROOM SIBLING
%token START TENTATIVE TEXT THISANDFUTURE THISANDPRIOR TIME TRANSPAENT
%token UNKNOWN UTCOFFSET XNAME

%token ALTREP CN CUTYPE DAYLIGHT DIR ENCODING EVENT FBTYPE FMTTYPE LANGUAGE 
%token MEMBER PARTSTAT RANGE RELATED RELTYPE ROLE RSVP SENTBY STANDARD URI

%token TIME_CHAR UTC_CHAR


%%

value:
       binary_value
	| boolean_value
	| date_value
	| datetime_value
	| duration_value
	| period_value
	| recur_value
        | utcoffset_value
        | error { 
                  icalparser_yy_value = 0;
		  icalparser_clear_flex_input();
                  yyclearin;
                  }

binary_value: "unimplemented2"

boolean_value: 
        TRUE
	{ icalparser_yy_value = icalvalue_new_boolean(1); }
	| FALSE		
	{ icalparser_yy_value = icalvalue_new_boolean(0); }

date_value: DIGITS
        {
	    struct icaltimetype stm;

	    stm = fill_datetime($1,0);

	    stm.hour = -1;
	    stm.minute = -1;
	    stm.second = -1;
	    stm.is_utc = 0;
	    stm.is_date = 1;

	    icalparser_yy_value = icalvalue_new_date(stm);
	}

utc_char: 
	/*empty*/  {utc = 0;}
	| UTC_CHAR {utc = 1;}

/* This is used in the period_value, where there may be two utc characters per rule. */
utc_char_b: 
	/*empty*/  {utc_b = 0;}
	| UTC_CHAR {utc_b = 1;}

datetime_value: 
	DIGITS TIME_CHAR DIGITS utc_char
        {
	    struct  icaltimetype stm;
	    stm = fill_datetime($1, $3);
	    stm.is_utc = utc;
	    stm.is_date = 0;

	    icalparser_yy_value = 
		icalvalue_new_datetime(stm);
	}


/* Duration */


dur_date: dur_day
	| dur_day dur_time

dur_week: DIGITS 'W'
	{
	    duration.weeks = atoi($1);
	}

dur_time: TIME_CHAR dur_hour 
	{
	}
	| TIME_CHAR dur_minute
	{
	}
	| TIME_CHAR dur_second
	{
	}

dur_hour: DIGITS 'H'
	{
	    duration.hours = atoi($1);
	}
	| DIGITS 'H' dur_minute
	{
	    duration.hours = atoi($1);
	}

dur_minute: DIGITS 'M'
	{
	    duration.minutes = atoi($1);
	}
	| DIGITS 'M' dur_second
	{
	    duration.minutes = atoi($1);
	}

dur_second: DIGITS 'S'
	{
	    duration.seconds = atoi($1);
	}

dur_day: DIGITS 'D'
	{
	    duration.days = atoi($1);
	}

dur_prefix: /* empty */
	{
	} 
	| '+'
	{
	}
	| '-'
	{
	}

duration_value: dur_prefix 'P' dur_date
	{ 
	    icalparser_yy_value = icalvalue_new_duration(duration); 
	    memset(&duration,0, sizeof(duration));
	}
	| dur_prefix 'P' dur_time
	{ 
	    icalparser_yy_value = icalvalue_new_duration(duration); 
	    memset(&duration,0, sizeof(duration));
	}
	| dur_prefix 'P' dur_week
	{ 
	    icalparser_yy_value = icalvalue_new_duration(duration); 
	    memset(&duration,0, sizeof(duration));
	}


/* Period */

period_value:  DIGITS TIME_CHAR DIGITS utc_char '/'  DIGITS TIME_CHAR DIGITS utc_char_b
	{
            struct icalperiodtype p;
        
	    p.start = fill_datetime($1,$3);
	    p.start.is_utc = utc;
	    p.start.is_date = 0;


	    p.end = fill_datetime($6,$8);
	    p.end.is_utc = utc_b;
	    p.end.is_date = 0;
		
	    p.duration.days = -1;
	    p.duration.weeks = -1;
	    p.duration.hours = -1;
	    p.duration.minutes = -1;
	    p.duration.seconds = -1;

	    icalparser_yy_value = icalvalue_new_period(p);
	}
	| DIGITS TIME_CHAR DIGITS utc_char '/'  duration_value
	{
            struct icalperiodtype p;
	    
	    p.start = fill_datetime($1,$3);
	    p.start.is_utc = utc;
	    p.start.is_date = 0;

	    p.end.year = -1;
	    p.end.month = -1;
	    p.end.day = -1;
	    p.end.hour = -1;
	    p.end.minute = -1;
	    p.end.second = -1;
		   
	    /* The duration_value rule setes the global 'duration'
               variable, but it also creates a new value in
               icalparser_yy_value. So, free that, then copy
               'duration' into the icalperiodtype struct. */

	    p.duration = icalvalue_get_duration(icalparser_yy_value);
	    icalvalue_free(icalparser_yy_value);
	    icalparser_yy_value = 0;

	    icalparser_yy_value = icalvalue_new_period(p);

	}



/* Recur */

recur_start:
	FREQ EQUALS SECONDLY {clear_recur();recur.freq = ICAL_SECONDLY_RECURRENCE;}
        | FREQ EQUALS MINUTELY {clear_recur();recur.freq = ICAL_MINUTELY_RECURRENCE;}
	| FREQ EQUALS HOURLY {clear_recur();recur.freq = ICAL_HOURLY_RECURRENCE;}
	| FREQ EQUALS DAILY {clear_recur();recur.freq = ICAL_DAILY_RECURRENCE;}
	| FREQ EQUALS WEEKLY {clear_recur();recur.freq = ICAL_WEEKLY_RECURRENCE;}
	| FREQ EQUALS MONTHLY {clear_recur();recur.freq = ICAL_MONTHLY_RECURRENCE;}
	| FREQ EQUALS YEARLY {clear_recur();recur.freq = ICAL_YEARLY_RECURRENCE;}
	;


weekday:
	SU { skiplist[skippos]=ICAL_SUNDAY_WEEKDAY; if( skippos<8) skippos++;}
	| MO { skiplist[skippos]=ICAL_MONDAY_WEEKDAY;if( skippos<8) skippos++;}
	| TU { skiplist[skippos]=ICAL_TUESDAY_WEEKDAY;if( skippos<8) skippos++;}
	| WE { skiplist[skippos]=ICAL_WEDNESDAY_WEEKDAY;if( skippos<8) skippos++;}
	| TH { skiplist[skippos]=ICAL_THURSDAY_WEEKDAY;if( skippos<8) skippos++;}
	| FR { skiplist[skippos]=ICAL_FRIDAY_WEEKDAY;if( skippos<8) skippos++;}
	| SA { skiplist[skippos]=ICAL_SATURDAY_WEEKDAY;if( skippos<8) skippos++;}
	;


weekday_list:
	weekday
	| DIGITS weekday { } /* HACK Incorectly handles int in BYDAY */
	| weekday_list COMMA weekday
	

recur_list: 
	DIGITS { skiplist[skippos] = atoi($1); skippos++;}
        | recur_list COMMA DIGITS { skiplist[skippos] = atoi($3); if (skippos<367) skippos++;}
	;

recur_skip:
	INTERVAL EQUALS DIGITS {recur.interval = atoi($3);}
	| WKST EQUALS SU {recur.week_start = ICAL_SUNDAY_WEEKDAY;}
	| WKST EQUALS MO {recur.week_start = ICAL_MONDAY_WEEKDAY;}
	| WKST EQUALS TU {recur.week_start = ICAL_TUESDAY_WEEKDAY;}
	| WKST EQUALS WE {recur.week_start = ICAL_WEDNESDAY_WEEKDAY;}
	| WKST EQUALS TH {recur.week_start = ICAL_THURSDAY_WEEKDAY;}
	| WKST EQUALS FR {recur.week_start = ICAL_FRIDAY_WEEKDAY;}
	| WKST EQUALS SA {recur.week_start = ICAL_SATURDAY_WEEKDAY;}
	| BYSECOND EQUALS recur_list{copy_list(recur.by_second,60);}
	| BYMINUTE EQUALS recur_list{copy_list(recur.by_minute,60);}
	| BYHOUR EQUALS recur_list{copy_list(recur.by_hour,24);}
	| BYDAY EQUALS weekday_list{copy_list(recur.by_day,7);}
	| BYMONTH EQUALS recur_list{copy_list(recur.by_month,12);}
	| BYMONTHDAY EQUALS recur_list{copy_list(recur.by_month_day,31);}
	| BYYEARDAY EQUALS recur_list{copy_list(recur.by_year_day,366);}
	| BYWEEKNO EQUALS recur_list{copy_list(recur.by_week_no,53);}
	| BYSETPOS EQUALS recur_list{copy_list(recur.by_set_pos,366);}
	| UNTIL EQUALS datetime_value
           { recur.until = icalvalue_get_datetime(icalparser_yy_value);
	   icalvalue_free(icalparser_yy_value); icalparser_yy_value=0;}
	| UNTIL EQUALS date_value 
           { recur.until = icalvalue_get_date(icalparser_yy_value);
	   icalvalue_free(icalparser_yy_value); icalparser_yy_value=0;}
	| COUNT EQUALS DIGITS
           { recur.count = atoi($3); }
	;

recur_skip_list:
	/* empty */
	| recur_skip_list SEMICOLON recur_skip

recur_value: 
	recur_start recur_skip_list
	  { icalparser_yy_value = icalvalue_new_recur(recur); }



/* UTC Offset */

plusminus: '+' { utcsign = 1; }
	| '-' { utcsign = -1; }

utcoffset_value: 
	plusminus INTNUMBER INTNUMBER
	{
	    icalparser_yy_value = icalvalue_new_utcoffset( utcsign * ($2*3600) + ($3*60) );
  	}

	| plusminus INTNUMBER INTNUMBER INTNUMBER
	{
	    icalparser_yy_value = icalvalue_new_utcoffset(utcsign * ($2*3600) + ($3*60) +($4));
  	}



%%


void clear_recur()
{   
    memset(&skiplist, ICAL_RECURRENCE_ARRAY_MAX_BYTE, sizeof(skiplist));
    skippos = 0;

    icalrecurrencetype_clear(&recur);
}

void copy_list(short* array, size_t size)
{ 
	memcpy(array, skiplist, size*sizeof(short));
	memset(&skiplist,ICAL_RECURRENCE_ARRAY_MAX_BYTE, sizeof(skiplist));
	skippos = 0;
}

struct icaltimetype fill_datetime(char* datestr, char* timestr)
{
	    struct icaltimetype stm;

	    memset(&stm,0,sizeof(stm));

	    if (datestr != 0){
		sscanf(datestr,"%4d%2d%2d",&(stm.year), &(stm.month), 
		       &(stm.day));
	    }

	    if (timestr != 0){
		sscanf(timestr,"%2d%2d%2d", &(stm.hour), &(stm.minute), 
		       &(stm.second));
	    }

	    return stm;

}

void yyerror(char* s)
{
    /*fprintf(stderr,"Parse error \'%s\'\n", s);*/
}

