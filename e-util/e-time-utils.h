/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Time utility functions
 *
 * Author:
 *   Damon Chaplin (damon@ximian.com)
 *
 * (C) 2001 Ximian, Inc.
 */

#ifndef E_TIME_UTILS
#define E_TIME_UTILS

#include <time.h>
#include <glib.h>

typedef enum {
	E_TIME_PARSE_OK,
	E_TIME_PARSE_NONE,
	E_TIME_PARSE_INVALID
} ETimeParseStatus;

/* Tries to parse a string containing a date and time. */
ETimeParseStatus e_time_parse_date_and_time	(const char	*value,
						 struct tm	*result);

/* Tries to parse a string containing a date. */
ETimeParseStatus e_time_parse_date		(const char	*value,
						 struct tm	*result);

/* Tries to parse a string containing a time. */
ETimeParseStatus e_time_parse_time		(const char	*value,
						 struct tm	*result);

/* Turns a struct tm into a string like "Wed  3/12/00 12:00:00 AM". */
void e_time_format_date_and_time		(struct tm	*date_tm,
						 gboolean	 use_24_hour_format,
						 gboolean	 show_midnight,
						 gboolean	 show_zero_seconds,
						 char		*buffer,
						 int		 buffer_size);

/* Formats a time from a struct tm, e.g. "01:59 PM". */
void e_time_format_time				(struct tm	*date_tm,
						 gboolean	 use_24_hour_format,
						 gboolean	 show_zero_seconds,
						 char		*buffer,
						 int		 buffer_size);


/* Like mktime(3), but assumes UTC instead of local timezone. */
time_t e_mktime_utc (struct tm *timeptr);

/* Like localtime_r(3), but also returns an offset in minutes after UTC.
   (Calling gmtime with tt + offset would generate the same tm) */
void e_localtime_with_offset (time_t tt, struct tm *tm, int *offset);

#endif /* E_TIME_UTILS */
