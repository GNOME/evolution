
/*  A Bison parser, made from icalyacc.y
    by GNU Bison version 1.28  */

#define YYBISON 1  /* Identify Bison output.  */

#define	DIGITS	257
#define	INTNUMBER	258
#define	FLOATNUMBER	259
#define	STRING	260
#define	EOL	261
#define	EQUALS	262
#define	CHARACTER	263
#define	COLON	264
#define	COMMA	265
#define	SEMICOLON	266
#define	TIMESEPERATOR	267
#define	TRUE	268
#define	FALSE	269
#define	FREQ	270
#define	BYDAY	271
#define	BYHOUR	272
#define	BYMINUTE	273
#define	BYMONTH	274
#define	BYMONTHDAY	275
#define	BYSECOND	276
#define	BYSETPOS	277
#define	BYWEEKNO	278
#define	BYYEARDAY	279
#define	DAILY	280
#define	MINUTELY	281
#define	MONTHLY	282
#define	SECONDLY	283
#define	WEEKLY	284
#define	HOURLY	285
#define	YEARLY	286
#define	INTERVAL	287
#define	COUNT	288
#define	UNTIL	289
#define	WKST	290
#define	MO	291
#define	SA	292
#define	SU	293
#define	TU	294
#define	WE	295
#define	TH	296
#define	FR	297
#define	BIT8	298
#define	ACCEPTED	299
#define	ADD	300
#define	AUDIO	301
#define	BASE64	302
#define	BINARY	303
#define	BOOLEAN	304
#define	BUSY	305
#define	BUSYTENTATIVE	306
#define	BUSYUNAVAILABLE	307
#define	CALADDRESS	308
#define	CANCEL	309
#define	CANCELLED	310
#define	CHAIR	311
#define	CHILD	312
#define	COMPLETED	313
#define	CONFIDENTIAL	314
#define	CONFIRMED	315
#define	COUNTER	316
#define	DATE	317
#define	DATETIME	318
#define	DECLINECOUNTER	319
#define	DECLINED	320
#define	DELEGATED	321
#define	DISPLAY	322
#define	DRAFT	323
#define	DURATION	324
#define	EMAIL	325
#define	END	326
#define	FINAL	327
#define	FLOAT	328
#define	FREE	329
#define	GREGORIAN	330
#define	GROUP	331
#define	INDIVIDUAL	332
#define	INPROCESS	333
#define	INTEGER	334
#define	NEEDSACTION	335
#define	NONPARTICIPANT	336
#define	OPAQUE	337
#define	OPTPARTICIPANT	338
#define	PARENT	339
#define	PERIOD	340
#define	PRIVATE	341
#define	PROCEDURE	342
#define	PUBLIC	343
#define	PUBLISH	344
#define	RECUR	345
#define	REFRESH	346
#define	REPLY	347
#define	REQPARTICIPANT	348
#define	REQUEST	349
#define	RESOURCE	350
#define	ROOM	351
#define	SIBLING	352
#define	START	353
#define	TENTATIVE	354
#define	TEXT	355
#define	THISANDFUTURE	356
#define	THISANDPRIOR	357
#define	TIME	358
#define	TRANSPAENT	359
#define	UNKNOWN	360
#define	UTCOFFSET	361
#define	XNAME	362
#define	ALTREP	363
#define	CN	364
#define	CUTYPE	365
#define	DAYLIGHT	366
#define	DIR	367
#define	ENCODING	368
#define	EVENT	369
#define	FBTYPE	370
#define	FMTTYPE	371
#define	LANGUAGE	372
#define	MEMBER	373
#define	PARTSTAT	374
#define	RANGE	375
#define	RELATED	376
#define	RELTYPE	377
#define	ROLE	378
#define	RSVP	379
#define	SENTBY	380
#define	STANDARD	381
#define	URI	382
#define	TIME_CHAR	383
#define	UTC_CHAR	384

#line 1 "icalyacc.y"

/* -*- Mode: C -*-
  ======================================================================
  FILE: icalitip.y
  CREATOR: eric 10 June 1999
  
  DESCRIPTION:
  
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
void set_parser_value_state();
struct icaltimetype fill_datetime(char* d, char* t);
void ical_yy_error(char *s); /* Don't know why I need this.... */
/*int yylex(void); /* Or this. */



/* Set the state of the lexer so it will interpret values ( iCAL
   VALUEs, that is, ) correctly. */


#line 75 "icalyacc.y"
typedef union {
	float v_float;
	int   v_int;
	char* v_string;

  /* Renaming hack */
#define    yymaxdepth ical_yy_maxdepth
#define    yyparse ical_yy_parse
#define    yylex   ical_yy_lex
#define    yyerror ical_yy_error
#define    yylval  ical_yy_lval
#define    yychar  ical_yy_char
#define    yydebug ical_yy_debug
#define    yypact  ical_yy_pact
#define    yyr1    ical_yy_r1
#define    yyr2    ical_yy_r2
#define    yydef   ical_yy_def
#define    yychk   ical_yy_chk
#define    yypgo   ical_yy_pgo
#define    yyact   ical_yy_act
#define    yyexca  ical_yy_exca
#define yyerrflag ical_yy_errflag
#define yynerrs    ical_yy_nerrs
#define    yyps    ical_yy_ps
#define    yypv    ical_yy_pv
#define    yys     ical_yy_s
#define    yy_yys  ical_yy_yys
#define    yystate ical_yy_state
#define    yytmp   ical_yy_tmp
#define    yyv     ical_yy_v
#define    yy_yyv  ical_yy_yyv
#define    yyval   ical_yy_val
#define    yylloc  ical_yy_lloc
#define yyreds     ical_yy_reds
#define yytoks     ical_yy_toks
#define yylhs      ical_yy_yylhs
#define yylen      ical_yy_yylen
#define yydefred ical_yy_yydefred
#define yydgoto    ical_yy_yydgoto
#define yydefred ical_yy_yydefred
#define yydgoto    ical_yy_yydgoto
#define yysindex ical_yy_yysindex
#define yyrindex ical_yy_yyrindex
#define yygindex ical_yy_yygindex
#define yytable     ical_yy_yytable
#define yycheck     ical_yy_yycheck
#define yyname   ical_yy_yyname
#define yyrule   ical_yy_yyrule



} YYSTYPE;
#ifndef YYDEBUG
#define YYDEBUG 1
#endif

#include <stdio.h>

#ifndef __cplusplus
#ifndef __STDC__
#define const
#endif
#endif



#define	YYFINAL		135
#define	YYFLAG		-32768
#define	YYNTBASE	141

#define YYTRANSLATE(x) ((unsigned)(x) <= 385 ? yytranslate[x] : 167)

static const short yytranslate[] = {     0,
     2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
     2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
     2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
     2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
     2,     2,   137,     2,   138,     2,   140,     2,     2,     2,
     2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
     2,     2,     2,     2,     2,     2,     2,   136,     2,     2,
     2,   133,     2,     2,     2,     2,   134,     2,     2,   139,
     2,     2,   135,     2,     2,     2,   132,     2,     2,     2,
     2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
     2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
     2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
     2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
     2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
     2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
     2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
     2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
     2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
     2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
     2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
     2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
     2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
     2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
     2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
     2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
     2,     2,     2,     2,     2,     1,     3,     4,     5,     6,
     7,     8,     9,    10,    11,    12,    13,    14,    15,    16,
    17,    18,    19,    20,    21,    22,    23,    24,    25,    26,
    27,    28,    29,    30,    31,    32,    33,    34,    35,    36,
    37,    38,    39,    40,    41,    42,    43,    44,    45,    46,
    47,    48,    49,    50,    51,    52,    53,    54,    55,    56,
    57,    58,    59,    60,    61,    62,    63,    64,    65,    66,
    67,    68,    69,    70,    71,    72,    73,    74,    75,    76,
    77,    78,    79,    80,    81,    82,    83,    84,    85,    86,
    87,    88,    89,    90,    91,    92,    93,    94,    95,    96,
    97,    98,    99,   100,   101,   102,   103,   104,   105,   106,
   107,   108,   109,   110,   111,   112,   113,   114,   115,   116,
   117,   118,   119,   120,   121,   122,   123,   124,   125,   126,
   127,   128,   129,   130,   131
};

#if YYDEBUG != 0
static const short yyprhs[] = {     0,
     0,     2,     4,     6,     8,    10,    12,    14,    16,    18,
    20,    22,    24,    26,    27,    29,    30,    32,    37,    39,
    42,    45,    48,    51,    54,    57,    61,    64,    68,    71,
    74,    75,    77,    79,    83,    87,    91,   101,   108,   112,
   116,   120,   124,   128,   132,   136,   138,   140,   142,   144,
   146,   148,   150,   152,   155,   159,   161,   165,   169,   173,
   177,   181,   185,   189,   193,   197,   201,   205,   209,   213,
   217,   221,   225,   229,   233,   237,   241,   245,   246,   250,
   253,   255,   257,   261
};

static const short yyrhs[] = {   142,
     0,   143,     0,   144,     0,   147,     0,   156,     0,   157,
     0,   164,     0,   166,     0,     1,     0,   131,     0,    14,
     0,    15,     0,     3,     0,     0,   130,     0,     0,   130,
     0,     3,   129,     3,   145,     0,   154,     0,   154,   150,
     0,     3,   132,     0,   129,   151,     0,   129,   152,     0,
   129,   153,     0,     3,   133,     0,     3,   133,   152,     0,
     3,   134,     0,     3,   134,   153,     0,     3,   135,     0,
     3,   136,     0,     0,   137,     0,   138,     0,   155,   139,
   148,     0,   155,   139,   150,     0,   155,   139,   149,     0,
     3,   129,     3,   145,   140,     3,   129,     3,   146,     0,
     3,   129,     3,   145,   140,   156,     0,    16,     8,    29,
     0,    16,     8,    27,     0,    16,     8,    31,     0,    16,
     8,    26,     0,    16,     8,    30,     0,    16,     8,    28,
     0,    16,     8,    32,     0,    39,     0,    37,     0,    40,
     0,    41,     0,    42,     0,    43,     0,    38,     0,   159,
     0,     3,   159,     0,   160,    11,   159,     0,     3,     0,
   161,    11,     3,     0,    33,     8,     3,     0,    36,     8,
    39,     0,    36,     8,    37,     0,    36,     8,    40,     0,
    36,     8,    41,     0,    36,     8,    42,     0,    36,     8,
    43,     0,    36,     8,    38,     0,    22,     8,   161,     0,
    19,     8,   161,     0,    18,     8,   161,     0,    17,     8,
   160,     0,    20,     8,   161,     0,    21,     8,   161,     0,
    25,     8,   161,     0,    24,     8,   161,     0,    23,     8,
   161,     0,    35,     8,   147,     0,    35,     8,   144,     0,
    34,     8,     3,     0,     0,   163,    12,   162,     0,   158,
   163,     0,   137,     0,   138,     0,   165,     4,     4,     0,
   165,     4,     4,     4,     0
};

#endif

#if YYDEBUG != 0
static const short yyrline[] = { 0,
   158,   160,   161,   162,   163,   164,   165,   166,   167,   173,
   175,   178,   181,   196,   198,   201,   203,   205,   221,   222,
   224,   229,   232,   235,   239,   243,   248,   252,   257,   262,
   267,   270,   273,   277,   282,   287,   296,   317,   349,   351,
   352,   353,   354,   355,   356,   360,   362,   363,   364,   365,
   366,   367,   371,   373,   374,   377,   379,   382,   384,   385,
   386,   387,   388,   389,   390,   391,   392,   393,   394,   395,
   396,   397,   398,   399,   400,   403,   406,   410,   412,   414,
   422,   423,   425,   431
};
#endif


#if YYDEBUG != 0 || defined (YYERROR_VERBOSE)

static const char * const yytname[] = {   "$","error","$undefined.","DIGITS",
"INTNUMBER","FLOATNUMBER","STRING","EOL","EQUALS","CHARACTER","COLON","COMMA",
"SEMICOLON","TIMESEPERATOR","TRUE","FALSE","FREQ","BYDAY","BYHOUR","BYMINUTE",
"BYMONTH","BYMONTHDAY","BYSECOND","BYSETPOS","BYWEEKNO","BYYEARDAY","DAILY",
"MINUTELY","MONTHLY","SECONDLY","WEEKLY","HOURLY","YEARLY","INTERVAL","COUNT",
"UNTIL","WKST","MO","SA","SU","TU","WE","TH","FR","BIT8","ACCEPTED","ADD","AUDIO",
"BASE64","BINARY","BOOLEAN","BUSY","BUSYTENTATIVE","BUSYUNAVAILABLE","CALADDRESS",
"CANCEL","CANCELLED","CHAIR","CHILD","COMPLETED","CONFIDENTIAL","CONFIRMED",
"COUNTER","DATE","DATETIME","DECLINECOUNTER","DECLINED","DELEGATED","DISPLAY",
"DRAFT","DURATION","EMAIL","END","FINAL","FLOAT","FREE","GREGORIAN","GROUP",
"INDIVIDUAL","INPROCESS","INTEGER","NEEDSACTION","NONPARTICIPANT","OPAQUE","OPTPARTICIPANT",
"PARENT","PERIOD","PRIVATE","PROCEDURE","PUBLIC","PUBLISH","RECUR","REFRESH",
"REPLY","REQPARTICIPANT","REQUEST","RESOURCE","ROOM","SIBLING","START","TENTATIVE",
"TEXT","THISANDFUTURE","THISANDPRIOR","TIME","TRANSPAENT","UNKNOWN","UTCOFFSET",
"XNAME","ALTREP","CN","CUTYPE","DAYLIGHT","DIR","ENCODING","EVENT","FBTYPE",
"FMTTYPE","LANGUAGE","MEMBER","PARTSTAT","RANGE","RELATED","RELTYPE","ROLE",
"RSVP","SENTBY","STANDARD","URI","TIME_CHAR","UTC_CHAR","\"unimplemented2\"",
"'W'","'H'","'M'","'S'","'D'","'+'","'-'","'P'","'/'","value","binary_value",
"boolean_value","date_value","utc_char","utc_char_b","datetime_value","dur_date",
"dur_week","dur_time","dur_hour","dur_minute","dur_second","dur_day","dur_prefix",
"duration_value","period_value","recur_start","weekday","weekday_list","recur_list",
"recur_skip","recur_skip_list","recur_value","plusminus","utcoffset_value", NULL
};
#endif

static const short yyr1[] = {     0,
   141,   141,   141,   141,   141,   141,   141,   141,   141,   142,
   143,   143,   144,   145,   145,   146,   146,   147,   148,   148,
   149,   150,   150,   150,   151,   151,   152,   152,   153,   154,
   155,   155,   155,   156,   156,   156,   157,   157,   158,   158,
   158,   158,   158,   158,   158,   159,   159,   159,   159,   159,
   159,   159,   160,   160,   160,   161,   161,   162,   162,   162,
   162,   162,   162,   162,   162,   162,   162,   162,   162,   162,
   162,   162,   162,   162,   162,   162,   162,   163,   163,   164,
   165,   165,   166,   166
};

static const short yyr2[] = {     0,
     1,     1,     1,     1,     1,     1,     1,     1,     1,     1,
     1,     1,     1,     0,     1,     0,     1,     4,     1,     2,
     2,     2,     2,     2,     2,     3,     2,     3,     2,     2,
     0,     1,     1,     3,     3,     3,     9,     6,     3,     3,
     3,     3,     3,     3,     3,     1,     1,     1,     1,     1,
     1,     1,     1,     2,     3,     1,     3,     3,     3,     3,
     3,     3,     3,     3,     3,     3,     3,     3,     3,     3,
     3,     3,     3,     3,     3,     3,     3,     0,     3,     2,
     1,     1,     3,     4
};

static const short yydefact[] = {     0,
     9,    13,    11,    12,     0,    10,    32,    33,     1,     2,
     3,     4,     0,     5,     6,    78,     7,     0,     8,     0,
     0,     0,    80,     0,    14,    42,    40,    44,    39,    43,
    41,    45,     0,     0,    34,    36,    35,    19,     0,    83,
    15,    18,    21,    30,     0,    22,    23,    24,    20,     0,
     0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     0,     0,    79,    84,    31,    25,    27,    29,     0,     0,
     0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     0,     0,    32,    33,    38,     0,    26,     0,    28,     0,
    47,    52,    46,    48,    49,    50,    51,    53,    69,    56,
    68,    67,    70,    71,    66,    74,    73,    72,    58,    77,
    13,    76,    75,    60,    65,    59,    61,    62,    63,    64,
     0,    54,     0,     0,     0,    16,    55,    57,    14,    17,
    37,    18,     0,     0,     0
};

static const short yydefgoto[] = {   133,
     9,    10,    11,    42,   131,    12,    35,    36,    37,    46,
    47,    48,    38,    13,    14,    15,    16,    98,    99,   101,
    63,    23,    17,    18,    19
};

static const short yypact[] = {    -1,
-32768,  -123,-32768,-32768,     1,-32768,     3,     6,-32768,-32768,
-32768,-32768,  -127,-32768,-32768,-32768,-32768,    64,-32768,    66,
   -10,    -2,    58,    67,   -58,-32768,-32768,-32768,-32768,-32768,
-32768,-32768,  -128,    70,-32768,-32768,-32768,   -55,    28,    71,
-32768,   -64,-32768,-32768,   -68,-32768,-32768,-32768,-32768,    69,
    72,    73,    74,    75,    76,    77,    78,    79,    80,    81,
    82,    83,-32768,-32768,     2,    89,    90,-32768,     0,    91,
    91,    91,    91,    91,    91,    91,    91,    92,    93,    94,
   -14,   -51,-32768,-32768,-32768,   -36,-32768,   -56,-32768,    -7,
-32768,-32768,-32768,-32768,-32768,-32768,-32768,-32768,    88,-32768,
    95,    95,    95,    95,    95,    95,    95,    95,-32768,-32768,
   -29,-32768,-32768,-32768,-32768,-32768,-32768,-32768,-32768,-32768,
    98,-32768,    -7,    99,   100,   -26,-32768,-32768,   -58,-32768,
-32768,-32768,   105,   107,-32768
};

static const short yypgoto[] = {-32768,
-32768,-32768,    29,   -21,-32768,    30,-32768,-32768,    84,-32768,
    45,    46,-32768,-32768,    47,-32768,-32768,   -79,-32768,   -17,
-32768,-32768,-32768,-32768,-32768
};


#define	YYLAST		140


static const short yytable[] = {     1,
    33,     2,    90,    43,    82,    20,   -81,    44,    21,   -82,
   122,    22,     3,     4,     5,    26,    27,    28,    29,    30,
    31,    32,   114,   115,   116,   117,   118,   119,   120,    91,
    92,    93,    94,    95,    96,    97,    91,    92,    93,    94,
    95,    96,    97,   127,    50,    51,    52,    53,    54,    55,
    56,    57,    58,   102,   103,   104,   105,   106,   107,   108,
    59,    60,    61,    62,    66,    67,    68,    24,    25,    39,
    40,    41,    45,    34,    64,    65,    69,   121,    68,    70,
    71,    72,    73,    74,    75,    76,    77,    78,    79,    80,
    81,    86,    88,   100,   109,   110,   111,    67,   123,   125,
   126,   128,   129,   130,   134,   124,   135,   132,   112,   113,
    87,    85,    89,     0,     0,     0,     0,     0,     0,     0,
     0,    49,     0,     0,     0,     0,    34,     0,     0,     6,
     0,     0,     0,     0,     0,     7,     8,   -31,    83,    84
};

static const short yycheck[] = {     1,
     3,     3,     3,   132,     3,   129,     4,   136,     8,     4,
    90,   139,    14,    15,    16,    26,    27,    28,    29,    30,
    31,    32,    37,    38,    39,    40,    41,    42,    43,    37,
    38,    39,    40,    41,    42,    43,    37,    38,    39,    40,
    41,    42,    43,   123,    17,    18,    19,    20,    21,    22,
    23,    24,    25,    71,    72,    73,    74,    75,    76,    77,
    33,    34,    35,    36,   133,   134,   135,     4,     3,    12,
     4,   130,     3,   129,     4,   140,     8,   129,   135,     8,
     8,     8,     8,     8,     8,     8,     8,     8,     8,     8,
     8,     3,     3,     3,     3,     3,     3,   134,    11,   129,
     3,     3,     3,   130,     0,    11,     0,   129,    80,    80,
    66,    65,    67,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
    -1,    38,    -1,    -1,    -1,    -1,   129,    -1,    -1,   131,
    -1,    -1,    -1,    -1,    -1,   137,   138,   139,   137,   138
};
/* -*-C-*-  Note some compilers choke on comments on `#line' lines.  */
#line 3 "/usr/lib/bison.simple"
/* This file comes from bison-1.28.  */

/* Skeleton output parser for bison,
   Copyright (C) 1984, 1989, 1990 Free Software Foundation, Inc.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place - Suite 330,
   Boston, MA 02111-1307, USA.  */

/* As a special exception, when this file is copied by Bison into a
   Bison output file, you may use that output file without restriction.
   This special exception was added by the Free Software Foundation
   in version 1.24 of Bison.  */

/* This is the parser code that is written into each bison parser
  when the %semantic_parser declaration is not specified in the grammar.
  It was written by Richard Stallman by simplifying the hairy parser
  used when %semantic_parser is specified.  */

#ifndef YYSTACK_USE_ALLOCA
#ifdef alloca
#define YYSTACK_USE_ALLOCA
#else /* alloca not defined */
#ifdef __GNUC__
#define YYSTACK_USE_ALLOCA
#define alloca __builtin_alloca
#else /* not GNU C.  */
#if (!defined (__STDC__) && defined (sparc)) || defined (__sparc__) || defined (__sparc) || defined (__sgi) || (defined (__sun) && defined (__i386))
#define YYSTACK_USE_ALLOCA
#include <alloca.h>
#else /* not sparc */
/* We think this test detects Watcom and Microsoft C.  */
/* This used to test MSDOS, but that is a bad idea
   since that symbol is in the user namespace.  */
#if (defined (_MSDOS) || defined (_MSDOS_)) && !defined (__TURBOC__)
#if 0 /* No need for malloc.h, which pollutes the namespace;
	 instead, just don't use alloca.  */
#include <malloc.h>
#endif
#else /* not MSDOS, or __TURBOC__ */
#if defined(_AIX)
/* I don't know what this was needed for, but it pollutes the namespace.
   So I turned it off.   rms, 2 May 1997.  */
/* #include <malloc.h>  */
 #pragma alloca
#define YYSTACK_USE_ALLOCA
#else /* not MSDOS, or __TURBOC__, or _AIX */
#if 0
#ifdef __hpux /* haible@ilog.fr says this works for HPUX 9.05 and up,
		 and on HPUX 10.  Eventually we can turn this on.  */
#define YYSTACK_USE_ALLOCA
#define alloca __builtin_alloca
#endif /* __hpux */
#endif
#endif /* not _AIX */
#endif /* not MSDOS, or __TURBOC__ */
#endif /* not sparc */
#endif /* not GNU C */
#endif /* alloca not defined */
#endif /* YYSTACK_USE_ALLOCA not defined */

#ifdef YYSTACK_USE_ALLOCA
#define YYSTACK_ALLOC alloca
#else
#define YYSTACK_ALLOC malloc
#endif

/* Note: there must be only one dollar sign in this file.
   It is replaced by the list of actions, each action
   as one case of the switch.  */

#define yyerrok		(yyerrstatus = 0)
#define yyclearin	(yychar = YYEMPTY)
#define YYEMPTY		-2
#define YYEOF		0
#define YYACCEPT	goto yyacceptlab
#define YYABORT 	goto yyabortlab
#define YYERROR		goto yyerrlab1
/* Like YYERROR except do call yyerror.
   This remains here temporarily to ease the
   transition to the new meaning of YYERROR, for GCC.
   Once GCC version 2 has supplanted version 1, this can go.  */
#define YYFAIL		goto yyerrlab
#define YYRECOVERING()  (!!yyerrstatus)
#define YYBACKUP(token, value) \
do								\
  if (yychar == YYEMPTY && yylen == 1)				\
    { yychar = (token), yylval = (value);			\
      yychar1 = YYTRANSLATE (yychar);				\
      YYPOPSTACK;						\
      goto yybackup;						\
    }								\
  else								\
    { yyerror ("syntax error: cannot back up"); YYERROR; }	\
while (0)

#define YYTERROR	1
#define YYERRCODE	256

#ifndef YYPURE
#define YYLEX		yylex()
#endif

#ifdef YYPURE
#ifdef YYLSP_NEEDED
#ifdef YYLEX_PARAM
#define YYLEX		yylex(&yylval, &yylloc, YYLEX_PARAM)
#else
#define YYLEX		yylex(&yylval, &yylloc)
#endif
#else /* not YYLSP_NEEDED */
#ifdef YYLEX_PARAM
#define YYLEX		yylex(&yylval, YYLEX_PARAM)
#else
#define YYLEX		yylex(&yylval)
#endif
#endif /* not YYLSP_NEEDED */
#endif

/* If nonreentrant, generate the variables here */

#ifndef YYPURE

int	yychar;			/*  the lookahead symbol		*/
YYSTYPE	yylval;			/*  the semantic value of the		*/
				/*  lookahead symbol			*/

#ifdef YYLSP_NEEDED
YYLTYPE yylloc;			/*  location data for the lookahead	*/
				/*  symbol				*/
#endif

int yynerrs;			/*  number of parse errors so far       */
#endif  /* not YYPURE */

#if YYDEBUG != 0
int yydebug;			/*  nonzero means print parse trace	*/
/* Since this is uninitialized, it does not stop multiple parsers
   from coexisting.  */
#endif

/*  YYINITDEPTH indicates the initial size of the parser's stacks	*/

#ifndef	YYINITDEPTH
#define YYINITDEPTH 200
#endif

/*  YYMAXDEPTH is the maximum size the stacks can grow to
    (effective only if the built-in stack extension method is used).  */

#if YYMAXDEPTH == 0
#undef YYMAXDEPTH
#endif

#ifndef YYMAXDEPTH
#define YYMAXDEPTH 10000
#endif

/* Define __yy_memcpy.  Note that the size argument
   should be passed with type unsigned int, because that is what the non-GCC
   definitions require.  With GCC, __builtin_memcpy takes an arg
   of type size_t, but it can handle unsigned int.  */

#if __GNUC__ > 1		/* GNU C and GNU C++ define this.  */
#define __yy_memcpy(TO,FROM,COUNT)	__builtin_memcpy(TO,FROM,COUNT)
#else				/* not GNU C or C++ */
#ifndef __cplusplus

/* This is the most reliable way to avoid incompatibilities
   in available built-in functions on various systems.  */
static void
__yy_memcpy (to, from, count)
     char *to;
     char *from;
     unsigned int count;
{
  register char *f = from;
  register char *t = to;
  register int i = count;

  while (i-- > 0)
    *t++ = *f++;
}

#else /* __cplusplus */

/* This is the most reliable way to avoid incompatibilities
   in available built-in functions on various systems.  */
static void
__yy_memcpy (char *to, char *from, unsigned int count)
{
  register char *t = to;
  register char *f = from;
  register int i = count;

  while (i-- > 0)
    *t++ = *f++;
}

#endif
#endif

#line 217 "/usr/lib/bison.simple"

/* The user can define YYPARSE_PARAM as the name of an argument to be passed
   into yyparse.  The argument should have type void *.
   It should actually point to an object.
   Grammar actions can access the variable by casting it
   to the proper pointer type.  */

#ifdef YYPARSE_PARAM
#ifdef __cplusplus
#define YYPARSE_PARAM_ARG void *YYPARSE_PARAM
#define YYPARSE_PARAM_DECL
#else /* not __cplusplus */
#define YYPARSE_PARAM_ARG YYPARSE_PARAM
#define YYPARSE_PARAM_DECL void *YYPARSE_PARAM;
#endif /* not __cplusplus */
#else /* not YYPARSE_PARAM */
#define YYPARSE_PARAM_ARG
#define YYPARSE_PARAM_DECL
#endif /* not YYPARSE_PARAM */

/* Prevent warning if -Wstrict-prototypes.  */
#ifdef __GNUC__
#ifdef YYPARSE_PARAM
int yyparse (void *);
#else
int yyparse (void);
#endif
#endif

int
yyparse(YYPARSE_PARAM_ARG)
     YYPARSE_PARAM_DECL
{
  register int yystate;
  register int yyn;
  register short *yyssp;
  register YYSTYPE *yyvsp;
  int yyerrstatus;	/*  number of tokens to shift before error messages enabled */
  int yychar1 = 0;		/*  lookahead token as an internal (translated) token number */

  short	yyssa[YYINITDEPTH];	/*  the state stack			*/
  YYSTYPE yyvsa[YYINITDEPTH];	/*  the semantic value stack		*/

  short *yyss = yyssa;		/*  refer to the stacks thru separate pointers */
  YYSTYPE *yyvs = yyvsa;	/*  to allow yyoverflow to reallocate them elsewhere */

#ifdef YYLSP_NEEDED
  YYLTYPE yylsa[YYINITDEPTH];	/*  the location stack			*/
  YYLTYPE *yyls = yylsa;
  YYLTYPE *yylsp;

#define YYPOPSTACK   (yyvsp--, yyssp--, yylsp--)
#else
#define YYPOPSTACK   (yyvsp--, yyssp--)
#endif

  int yystacksize = YYINITDEPTH;
  int yyfree_stacks = 0;

#ifdef YYPURE
  int yychar;
  YYSTYPE yylval;
  int yynerrs;
#ifdef YYLSP_NEEDED
  YYLTYPE yylloc;
#endif
#endif

  YYSTYPE yyval;		/*  the variable used to return		*/
				/*  semantic values from the action	*/
				/*  routines				*/

  int yylen;

#if YYDEBUG != 0
  if (yydebug)
    fprintf(stderr, "Starting parse\n");
#endif

  yystate = 0;
  yyerrstatus = 0;
  yynerrs = 0;
  yychar = YYEMPTY;		/* Cause a token to be read.  */

  /* Initialize stack pointers.
     Waste one element of value and location stack
     so that they stay on the same level as the state stack.
     The wasted elements are never initialized.  */

  yyssp = yyss - 1;
  yyvsp = yyvs;
#ifdef YYLSP_NEEDED
  yylsp = yyls;
#endif

/* Push a new state, which is found in  yystate  .  */
/* In all cases, when you get here, the value and location stacks
   have just been pushed. so pushing a state here evens the stacks.  */
yynewstate:

  *++yyssp = yystate;

  if (yyssp >= yyss + yystacksize - 1)
    {
      /* Give user a chance to reallocate the stack */
      /* Use copies of these so that the &'s don't force the real ones into memory. */
      YYSTYPE *yyvs1 = yyvs;
      short *yyss1 = yyss;
#ifdef YYLSP_NEEDED
      YYLTYPE *yyls1 = yyls;
#endif

      /* Get the current used size of the three stacks, in elements.  */
      int size = yyssp - yyss + 1;

#ifdef yyoverflow
      /* Each stack pointer address is followed by the size of
	 the data in use in that stack, in bytes.  */
#ifdef YYLSP_NEEDED
      /* This used to be a conditional around just the two extra args,
	 but that might be undefined if yyoverflow is a macro.  */
      yyoverflow("parser stack overflow",
		 &yyss1, size * sizeof (*yyssp),
		 &yyvs1, size * sizeof (*yyvsp),
		 &yyls1, size * sizeof (*yylsp),
		 &yystacksize);
#else
      yyoverflow("parser stack overflow",
		 &yyss1, size * sizeof (*yyssp),
		 &yyvs1, size * sizeof (*yyvsp),
		 &yystacksize);
#endif

      yyss = yyss1; yyvs = yyvs1;
#ifdef YYLSP_NEEDED
      yyls = yyls1;
#endif
#else /* no yyoverflow */
      /* Extend the stack our own way.  */
      if (yystacksize >= YYMAXDEPTH)
	{
	  yyerror("parser stack overflow");
	  if (yyfree_stacks)
	    {
	      free (yyss);
	      free (yyvs);
#ifdef YYLSP_NEEDED
	      free (yyls);
#endif
	    }
	  return 2;
	}
      yystacksize *= 2;
      if (yystacksize > YYMAXDEPTH)
	yystacksize = YYMAXDEPTH;
#ifndef YYSTACK_USE_ALLOCA
      yyfree_stacks = 1;
#endif
      yyss = (short *) YYSTACK_ALLOC (yystacksize * sizeof (*yyssp));
      __yy_memcpy ((char *)yyss, (char *)yyss1,
		   size * (unsigned int) sizeof (*yyssp));
      yyvs = (YYSTYPE *) YYSTACK_ALLOC (yystacksize * sizeof (*yyvsp));
      __yy_memcpy ((char *)yyvs, (char *)yyvs1,
		   size * (unsigned int) sizeof (*yyvsp));
#ifdef YYLSP_NEEDED
      yyls = (YYLTYPE *) YYSTACK_ALLOC (yystacksize * sizeof (*yylsp));
      __yy_memcpy ((char *)yyls, (char *)yyls1,
		   size * (unsigned int) sizeof (*yylsp));
#endif
#endif /* no yyoverflow */

      yyssp = yyss + size - 1;
      yyvsp = yyvs + size - 1;
#ifdef YYLSP_NEEDED
      yylsp = yyls + size - 1;
#endif

#if YYDEBUG != 0
      if (yydebug)
	fprintf(stderr, "Stack size increased to %d\n", yystacksize);
#endif

      if (yyssp >= yyss + yystacksize - 1)
	YYABORT;
    }

#if YYDEBUG != 0
  if (yydebug)
    fprintf(stderr, "Entering state %d\n", yystate);
#endif

  goto yybackup;
 yybackup:

/* Do appropriate processing given the current state.  */
/* Read a lookahead token if we need one and don't already have one.  */
/* yyresume: */

  /* First try to decide what to do without reference to lookahead token.  */

  yyn = yypact[yystate];
  if (yyn == YYFLAG)
    goto yydefault;

  /* Not known => get a lookahead token if don't already have one.  */

  /* yychar is either YYEMPTY or YYEOF
     or a valid token in external form.  */

  if (yychar == YYEMPTY)
    {
#if YYDEBUG != 0
      if (yydebug)
	fprintf(stderr, "Reading a token: ");
#endif
      yychar = YYLEX;
    }

  /* Convert token to internal form (in yychar1) for indexing tables with */

  if (yychar <= 0)		/* This means end of input. */
    {
      yychar1 = 0;
      yychar = YYEOF;		/* Don't call YYLEX any more */

#if YYDEBUG != 0
      if (yydebug)
	fprintf(stderr, "Now at end of input.\n");
#endif
    }
  else
    {
      yychar1 = YYTRANSLATE(yychar);

#if YYDEBUG != 0
      if (yydebug)
	{
	  fprintf (stderr, "Next token is %d (%s", yychar, yytname[yychar1]);
	  /* Give the individual parser a way to print the precise meaning
	     of a token, for further debugging info.  */
#ifdef YYPRINT
	  YYPRINT (stderr, yychar, yylval);
#endif
	  fprintf (stderr, ")\n");
	}
#endif
    }

  yyn += yychar1;
  if (yyn < 0 || yyn > YYLAST || yycheck[yyn] != yychar1)
    goto yydefault;

  yyn = yytable[yyn];

  /* yyn is what to do for this token type in this state.
     Negative => reduce, -yyn is rule number.
     Positive => shift, yyn is new state.
       New state is final state => don't bother to shift,
       just return success.
     0, or most negative number => error.  */

  if (yyn < 0)
    {
      if (yyn == YYFLAG)
	goto yyerrlab;
      yyn = -yyn;
      goto yyreduce;
    }
  else if (yyn == 0)
    goto yyerrlab;

  if (yyn == YYFINAL)
    YYACCEPT;

  /* Shift the lookahead token.  */

#if YYDEBUG != 0
  if (yydebug)
    fprintf(stderr, "Shifting token %d (%s), ", yychar, yytname[yychar1]);
#endif

  /* Discard the token being shifted unless it is eof.  */
  if (yychar != YYEOF)
    yychar = YYEMPTY;

  *++yyvsp = yylval;
#ifdef YYLSP_NEEDED
  *++yylsp = yylloc;
#endif

  /* count tokens shifted since error; after three, turn off error status.  */
  if (yyerrstatus) yyerrstatus--;

  yystate = yyn;
  goto yynewstate;

/* Do the default action for the current state.  */
yydefault:

  yyn = yydefact[yystate];
  if (yyn == 0)
    goto yyerrlab;

/* Do a reduction.  yyn is the number of a rule to reduce with.  */
yyreduce:
  yylen = yyr2[yyn];
  if (yylen > 0)
    yyval = yyvsp[1-yylen]; /* implement default value of the action */

#if YYDEBUG != 0
  if (yydebug)
    {
      int i;

      fprintf (stderr, "Reducing via rule %d (line %d), ",
	       yyn, yyrline[yyn]);

      /* Print the symbols being reduced, and their result.  */
      for (i = yyprhs[yyn]; yyrhs[i] > 0; i++)
	fprintf (stderr, "%s ", yytname[yyrhs[i]]);
      fprintf (stderr, " -> %s\n", yytname[yyr1[yyn]]);
    }
#endif


  switch (yyn) {

case 9:
#line 167 "icalyacc.y"
{ 
                  icalparser_yy_value = 0;
		  icalparser_clear_flex_input();
                  yyclearin;
                  ;
    break;}
case 11:
#line 177 "icalyacc.y"
{ icalparser_yy_value = icalvalue_new_boolean(1); ;
    break;}
case 12:
#line 179 "icalyacc.y"
{ icalparser_yy_value = icalvalue_new_boolean(0); ;
    break;}
case 13:
#line 182 "icalyacc.y"
{
	    struct icaltimetype stm;

	    stm = fill_datetime(yyvsp[0].v_string,0);

	    stm.hour = -1;
	    stm.minute = -1;
	    stm.second = -1;
	    stm.is_utc = 0;
	    stm.is_date = 1;

	    icalparser_yy_value = icalvalue_new_date(stm);
	;
    break;}
case 14:
#line 197 "icalyacc.y"
{utc = 0;;
    break;}
case 15:
#line 198 "icalyacc.y"
{utc = 1;;
    break;}
case 16:
#line 202 "icalyacc.y"
{utc_b = 0;;
    break;}
case 17:
#line 203 "icalyacc.y"
{utc_b = 1;;
    break;}
case 18:
#line 207 "icalyacc.y"
{
	    struct  icaltimetype stm;
	    stm = fill_datetime(yyvsp[-3].v_string, yyvsp[-1].v_string);
	    stm.is_utc = utc;
	    stm.is_date = 0;

	    icalparser_yy_value = 
		icalvalue_new_datetime(stm);
	;
    break;}
case 21:
#line 225 "icalyacc.y"
{
	    duration.weeks = atoi(yyvsp[-1].v_string);
	;
    break;}
case 22:
#line 230 "icalyacc.y"
{
	;
    break;}
case 23:
#line 233 "icalyacc.y"
{
	;
    break;}
case 24:
#line 236 "icalyacc.y"
{
	;
    break;}
case 25:
#line 240 "icalyacc.y"
{
	    duration.hours = atoi(yyvsp[-1].v_string);
	;
    break;}
case 26:
#line 244 "icalyacc.y"
{
	    duration.hours = atoi(yyvsp[-2].v_string);
	;
    break;}
case 27:
#line 249 "icalyacc.y"
{
	    duration.minutes = atoi(yyvsp[-1].v_string);
	;
    break;}
case 28:
#line 253 "icalyacc.y"
{
	    duration.minutes = atoi(yyvsp[-2].v_string);
	;
    break;}
case 29:
#line 258 "icalyacc.y"
{
	    duration.seconds = atoi(yyvsp[-1].v_string);
	;
    break;}
case 30:
#line 263 "icalyacc.y"
{
	    duration.days = atoi(yyvsp[-1].v_string);
	;
    break;}
case 31:
#line 268 "icalyacc.y"
{
	;
    break;}
case 32:
#line 271 "icalyacc.y"
{
	;
    break;}
case 33:
#line 274 "icalyacc.y"
{
	;
    break;}
case 34:
#line 278 "icalyacc.y"
{ 
	    icalparser_yy_value = icalvalue_new_duration(duration); 
	    memset(&duration,0, sizeof(duration));
	;
    break;}
case 35:
#line 283 "icalyacc.y"
{ 
	    icalparser_yy_value = icalvalue_new_duration(duration); 
	    memset(&duration,0, sizeof(duration));
	;
    break;}
case 36:
#line 288 "icalyacc.y"
{ 
	    icalparser_yy_value = icalvalue_new_duration(duration); 
	    memset(&duration,0, sizeof(duration));
	;
    break;}
case 37:
#line 297 "icalyacc.y"
{
            struct icalperiodtype p;
        
	    p.start = fill_datetime(yyvsp[-8].v_string,yyvsp[-6].v_string);
	    p.start.is_utc = utc;
	    p.start.is_date = 0;


	    p.end = fill_datetime(yyvsp[-3].v_string,yyvsp[-1].v_string);
	    p.end.is_utc = utc_b;
	    p.end.is_date = 0;
		
	    p.duration.days = -1;
	    p.duration.weeks = -1;
	    p.duration.hours = -1;
	    p.duration.minutes = -1;
	    p.duration.seconds = -1;

	    icalparser_yy_value = icalvalue_new_period(p);
	;
    break;}
case 38:
#line 318 "icalyacc.y"
{
            struct icalperiodtype p;
	    
	    p.start = fill_datetime(yyvsp[-5].v_string,yyvsp[-3].v_string);
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

	;
    break;}
case 39:
#line 350 "icalyacc.y"
{clear_recur();recur.freq = ICAL_SECONDLY_RECURRENCE;;
    break;}
case 40:
#line 351 "icalyacc.y"
{clear_recur();recur.freq = ICAL_MINUTELY_RECURRENCE;;
    break;}
case 41:
#line 352 "icalyacc.y"
{clear_recur();recur.freq = ICAL_HOURLY_RECURRENCE;;
    break;}
case 42:
#line 353 "icalyacc.y"
{clear_recur();recur.freq = ICAL_DAILY_RECURRENCE;;
    break;}
case 43:
#line 354 "icalyacc.y"
{clear_recur();recur.freq = ICAL_WEEKLY_RECURRENCE;;
    break;}
case 44:
#line 355 "icalyacc.y"
{clear_recur();recur.freq = ICAL_MONTHLY_RECURRENCE;;
    break;}
case 45:
#line 356 "icalyacc.y"
{clear_recur();recur.freq = ICAL_YEARLY_RECURRENCE;;
    break;}
case 46:
#line 361 "icalyacc.y"
{ skiplist[skippos]=ICAL_SUNDAY_WEEKDAY; if( skippos<8) skippos++;;
    break;}
case 47:
#line 362 "icalyacc.y"
{ skiplist[skippos]=ICAL_MONDAY_WEEKDAY;if( skippos<8) skippos++;;
    break;}
case 48:
#line 363 "icalyacc.y"
{ skiplist[skippos]=ICAL_TUESDAY_WEEKDAY;if( skippos<8) skippos++;;
    break;}
case 49:
#line 364 "icalyacc.y"
{ skiplist[skippos]=ICAL_WEDNESDAY_WEEKDAY;if( skippos<8) skippos++;;
    break;}
case 50:
#line 365 "icalyacc.y"
{ skiplist[skippos]=ICAL_THURSDAY_WEEKDAY;if( skippos<8) skippos++;;
    break;}
case 51:
#line 366 "icalyacc.y"
{ skiplist[skippos]=ICAL_FRIDAY_WEEKDAY;if( skippos<8) skippos++;;
    break;}
case 52:
#line 367 "icalyacc.y"
{ skiplist[skippos]=ICAL_SATURDAY_WEEKDAY;if( skippos<8) skippos++;;
    break;}
case 54:
#line 373 "icalyacc.y"
{ ;
    break;}
case 56:
#line 378 "icalyacc.y"
{ skiplist[skippos] = atoi(yyvsp[0].v_string); skippos++;;
    break;}
case 57:
#line 379 "icalyacc.y"
{ skiplist[skippos] = atoi(yyvsp[0].v_string); if (skippos<367) skippos++;;
    break;}
case 58:
#line 383 "icalyacc.y"
{recur.interval = atoi(yyvsp[0].v_string);;
    break;}
case 59:
#line 384 "icalyacc.y"
{recur.week_start = ICAL_SUNDAY_WEEKDAY;;
    break;}
case 60:
#line 385 "icalyacc.y"
{recur.week_start = ICAL_MONDAY_WEEKDAY;;
    break;}
case 61:
#line 386 "icalyacc.y"
{recur.week_start = ICAL_TUESDAY_WEEKDAY;;
    break;}
case 62:
#line 387 "icalyacc.y"
{recur.week_start = ICAL_WEDNESDAY_WEEKDAY;;
    break;}
case 63:
#line 388 "icalyacc.y"
{recur.week_start = ICAL_THURSDAY_WEEKDAY;;
    break;}
case 64:
#line 389 "icalyacc.y"
{recur.week_start = ICAL_FRIDAY_WEEKDAY;;
    break;}
case 65:
#line 390 "icalyacc.y"
{recur.week_start = ICAL_SATURDAY_WEEKDAY;;
    break;}
case 66:
#line 391 "icalyacc.y"
{copy_list(recur.by_second,60);;
    break;}
case 67:
#line 392 "icalyacc.y"
{copy_list(recur.by_minute,60);;
    break;}
case 68:
#line 393 "icalyacc.y"
{copy_list(recur.by_hour,24);;
    break;}
case 69:
#line 394 "icalyacc.y"
{copy_list(recur.by_day,7);;
    break;}
case 70:
#line 395 "icalyacc.y"
{copy_list(recur.by_month,12);;
    break;}
case 71:
#line 396 "icalyacc.y"
{copy_list(recur.by_month_day,31);;
    break;}
case 72:
#line 397 "icalyacc.y"
{copy_list(recur.by_year_day,366);;
    break;}
case 73:
#line 398 "icalyacc.y"
{copy_list(recur.by_week_no,53);;
    break;}
case 74:
#line 399 "icalyacc.y"
{copy_list(recur.by_set_pos,366);;
    break;}
case 75:
#line 401 "icalyacc.y"
{ recur.until = icalvalue_get_datetime(icalparser_yy_value);
	   icalvalue_free(icalparser_yy_value); icalparser_yy_value=0;;
    break;}
case 76:
#line 404 "icalyacc.y"
{ recur.until = icalvalue_get_date(icalparser_yy_value);
	   icalvalue_free(icalparser_yy_value); icalparser_yy_value=0;;
    break;}
case 77:
#line 407 "icalyacc.y"
{ recur.count = atoi(yyvsp[0].v_string); ;
    break;}
case 80:
#line 416 "icalyacc.y"
{ icalparser_yy_value = icalvalue_new_recur(recur); ;
    break;}
case 81:
#line 422 "icalyacc.y"
{ utcsign = 1; ;
    break;}
case 82:
#line 423 "icalyacc.y"
{ utcsign = -1; ;
    break;}
case 83:
#line 427 "icalyacc.y"
{
	    icalparser_yy_value = icalvalue_new_utcoffset( utcsign * (yyvsp[-1].v_int*3600) + (yyvsp[0].v_int*60) );
  	;
    break;}
case 84:
#line 432 "icalyacc.y"
{
	    icalparser_yy_value = icalvalue_new_utcoffset(utcsign * (yyvsp[-2].v_int*3600) + (yyvsp[-1].v_int*60) +(yyvsp[0].v_int));
  	;
    break;}
}
   /* the action file gets copied in in place of this dollarsign */
#line 543 "/usr/lib/bison.simple"

  yyvsp -= yylen;
  yyssp -= yylen;
#ifdef YYLSP_NEEDED
  yylsp -= yylen;
#endif

#if YYDEBUG != 0
  if (yydebug)
    {
      short *ssp1 = yyss - 1;
      fprintf (stderr, "state stack now");
      while (ssp1 != yyssp)
	fprintf (stderr, " %d", *++ssp1);
      fprintf (stderr, "\n");
    }
#endif

  *++yyvsp = yyval;

#ifdef YYLSP_NEEDED
  yylsp++;
  if (yylen == 0)
    {
      yylsp->first_line = yylloc.first_line;
      yylsp->first_column = yylloc.first_column;
      yylsp->last_line = (yylsp-1)->last_line;
      yylsp->last_column = (yylsp-1)->last_column;
      yylsp->text = 0;
    }
  else
    {
      yylsp->last_line = (yylsp+yylen-1)->last_line;
      yylsp->last_column = (yylsp+yylen-1)->last_column;
    }
#endif

  /* Now "shift" the result of the reduction.
     Determine what state that goes to,
     based on the state we popped back to
     and the rule number reduced by.  */

  yyn = yyr1[yyn];

  yystate = yypgoto[yyn - YYNTBASE] + *yyssp;
  if (yystate >= 0 && yystate <= YYLAST && yycheck[yystate] == *yyssp)
    yystate = yytable[yystate];
  else
    yystate = yydefgoto[yyn - YYNTBASE];

  goto yynewstate;

yyerrlab:   /* here on detecting error */

  if (! yyerrstatus)
    /* If not already recovering from an error, report this error.  */
    {
      ++yynerrs;

#ifdef YYERROR_VERBOSE
      yyn = yypact[yystate];

      if (yyn > YYFLAG && yyn < YYLAST)
	{
	  int size = 0;
	  char *msg;
	  int x, count;

	  count = 0;
	  /* Start X at -yyn if nec to avoid negative indexes in yycheck.  */
	  for (x = (yyn < 0 ? -yyn : 0);
	       x < (sizeof(yytname) / sizeof(char *)); x++)
	    if (yycheck[x + yyn] == x)
	      size += strlen(yytname[x]) + 15, count++;
	  msg = (char *) malloc(size + 15);
	  if (msg != 0)
	    {
	      strcpy(msg, "parse error");

	      if (count < 5)
		{
		  count = 0;
		  for (x = (yyn < 0 ? -yyn : 0);
		       x < (sizeof(yytname) / sizeof(char *)); x++)
		    if (yycheck[x + yyn] == x)
		      {
			strcat(msg, count == 0 ? ", expecting `" : " or `");
			strcat(msg, yytname[x]);
			strcat(msg, "'");
			count++;
		      }
		}
	      yyerror(msg);
	      free(msg);
	    }
	  else
	    yyerror ("parse error; also virtual memory exceeded");
	}
      else
#endif /* YYERROR_VERBOSE */
	yyerror("parse error");
    }

  goto yyerrlab1;
yyerrlab1:   /* here on error raised explicitly by an action */

  if (yyerrstatus == 3)
    {
      /* if just tried and failed to reuse lookahead token after an error, discard it.  */

      /* return failure if at end of input */
      if (yychar == YYEOF)
	YYABORT;

#if YYDEBUG != 0
      if (yydebug)
	fprintf(stderr, "Discarding token %d (%s).\n", yychar, yytname[yychar1]);
#endif

      yychar = YYEMPTY;
    }

  /* Else will try to reuse lookahead token
     after shifting the error token.  */

  yyerrstatus = 3;		/* Each real token shifted decrements this */

  goto yyerrhandle;

yyerrdefault:  /* current state does not do anything special for the error token. */

#if 0
  /* This is wrong; only states that explicitly want error tokens
     should shift them.  */
  yyn = yydefact[yystate];  /* If its default is to accept any token, ok.  Otherwise pop it.*/
  if (yyn) goto yydefault;
#endif

yyerrpop:   /* pop the current state because it cannot handle the error token */

  if (yyssp == yyss) YYABORT;
  yyvsp--;
  yystate = *--yyssp;
#ifdef YYLSP_NEEDED
  yylsp--;
#endif

#if YYDEBUG != 0
  if (yydebug)
    {
      short *ssp1 = yyss - 1;
      fprintf (stderr, "Error: state stack now");
      while (ssp1 != yyssp)
	fprintf (stderr, " %d", *++ssp1);
      fprintf (stderr, "\n");
    }
#endif

yyerrhandle:

  yyn = yypact[yystate];
  if (yyn == YYFLAG)
    goto yyerrdefault;

  yyn += YYTERROR;
  if (yyn < 0 || yyn > YYLAST || yycheck[yyn] != YYTERROR)
    goto yyerrdefault;

  yyn = yytable[yyn];
  if (yyn < 0)
    {
      if (yyn == YYFLAG)
	goto yyerrpop;
      yyn = -yyn;
      goto yyreduce;
    }
  else if (yyn == 0)
    goto yyerrpop;

  if (yyn == YYFINAL)
    YYACCEPT;

#if YYDEBUG != 0
  if (yydebug)
    fprintf(stderr, "Shifting error token, ");
#endif

  *++yyvsp = yylval;
#ifdef YYLSP_NEEDED
  *++yylsp = yylloc;
#endif

  yystate = yyn;
  goto yynewstate;

 yyacceptlab:
  /* YYACCEPT comes here.  */
  if (yyfree_stacks)
    {
      free (yyss);
      free (yyvs);
#ifdef YYLSP_NEEDED
      free (yyls);
#endif
    }
  return 0;

 yyabortlab:
  /* YYABORT comes here.  */
  if (yyfree_stacks)
    {
      free (yyss);
      free (yyvs);
#ifdef YYLSP_NEEDED
      free (yyls);
#endif
    }
  return 1;
}
#line 438 "icalyacc.y"



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

