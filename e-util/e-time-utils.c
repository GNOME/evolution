/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Time utility functions
 *
 * Author:
 *   Damon Chaplin (damon@ximian.com)
 *
 * (C) 2001 Ximian, Inc.
 */

#include <config.h>

/* We need this for strptime. */
#define _XOPEN_SOURCE 500
#define __USE_XOPEN
#include <time.h>
#include <sys/time.h>
#undef _XOPEN_SOURCE
#undef __USE_XOPEN

#include <string.h>
#include <ctype.h>
#include <glib.h>
#include <libgnome/gnome-defs.h>
#include <libgnome/gnome-i18n.h>
#include "e-time-utils.h"

static gboolean string_is_empty (const char *value);


/*
 * Parses a string containing a date and a time. The date is expected to be
 * in a format something like "Wed 3/13/00 14:20:00", though we use gettext
 * to support the appropriate local formats and we try to accept slightly
 * different formats, e.g. the weekday can be skipped and we can accept 12-hour
 * formats with an am/pm string.
 *
 * Returns E_TIME_PARSE_OK if it could not be parsed, E_TIME_PARSE_NONE if it
 * was empty, or E_TIME_PARSE_INVALID if it couldn't be parsed.
 */
ETimeParseStatus
e_time_parse_date_and_time		(const char	*value,
					 struct tm	*result)
{
	struct tm discard_tm, time_tm;
	struct tm *today_tm;
	time_t t;
	const char *pos, *parse_end;
	char *format[4];
	gboolean parsed_date = FALSE, parsed_time = FALSE;
	gint i;

	if (string_is_empty (value))
		return E_TIME_PARSE_NONE;

	pos = value;

	/* Skip everything up to the first digit. */
	while (!isdigit (*pos))
		pos++;

	memset (result, 0, sizeof (*result));
	/* strptime format for a date. */
	parse_end = strptime (pos, _("%m/%d/%Y"), result);
	if (parse_end) {
		pos = parse_end;
		parsed_date = TRUE;
	}

	/* FIXME: Skip everything up to the first digit. I hope the am/pm flag
	   never gets entered first - it does in Japanese, so fix this. */
	while (!isdigit (*pos))
		pos++;


	/* strptime format for a time of day, in 12-hour format. */
	format[0] = _("%I:%M:%S %p");

	/* strptime format for a time of day, in 24-hour format. */
	format[1] = _("%H:%M:%S");

	/* strptime format for time of day, without seconds, 12-hour format. */
	format[2] = _("%I:%M %p");

	/* strptime format for time of day, without seconds 24-hour format. */
	format[3] = _("%H:%M");

	for (i = 0; i < sizeof (format) / sizeof (format[0]); i++) {
		memset (&time_tm, 0, sizeof (time_tm));
		parse_end = strptime (pos, format[i], &time_tm);
		if (parse_end) {
			pos = parse_end;
			parsed_time = TRUE;
			break;
		}
	}

	/* Skip any whitespace. */
	while (isspace (*pos))
		pos++;

	/* If we haven't already parsed a date, try again. */
	if (!parsed_date) {
		memset (result, 0, sizeof (*result));
		/* strptime format for a date. */
		parse_end = strptime (pos, _("%m/%d/%Y"), result);
		if (parse_end) {
			pos = parse_end;
			parsed_date = TRUE;
		}
	}

	/* If we don't have a date or a time it must be invalid. */
	if (!parsed_date && !parsed_time)
		return E_TIME_PARSE_INVALID;

	if (parsed_date) {
		/* If a 2-digit year was used we use the current century. */
		if (result->tm_year < 0) {
			t = time (NULL);
			today_tm = localtime (&t);

			/* This should convert it into a value from 0 to 99. */
			result->tm_year += 1900;

			/* Now add on the century. */
			result->tm_year += today_tm->tm_year
				- (today_tm->tm_year % 100);
		}
	} else {
		/* If we didn't get a date we use the current day. */
		t = time (NULL);
		today_tm = localtime (&t);
		result->tm_mday = today_tm->tm_mday;
		result->tm_mon  = today_tm->tm_mon;
		result->tm_year = today_tm->tm_year;
	}

	if (parsed_time) {
		result->tm_hour = time_tm.tm_hour;
		result->tm_min = time_tm.tm_min;
		result->tm_sec = time_tm.tm_sec;
	} else {
		result->tm_hour = 0;
		result->tm_min = 0;
		result->tm_sec = 0;
	}

	result->tm_isdst = -1;

	return E_TIME_PARSE_OK;
}


/*
 * Parses a string containing a time. It is expected to be in a format
 * something like "14:20:00", though we use gettext to support the appropriate
 * local formats and we try to accept slightly different formats, e.g. we can
 * accept 12-hour formats with an am/pm string.
 *
 * Returns E_TIME_PARSE_OK if it could not be parsed, E_TIME_PARSE_NONE if it
 * was empty, or E_TIME_PARSE_INVALID if it couldn't be parsed.
 */
ETimeParseStatus
e_time_parse_time			(const char	*value,
					 struct tm	*result)
{
	const char *pos, *parse_end;
	char *format[4];
	gboolean parsed_time = FALSE;
	gint i;

	if (string_is_empty (value)) {
		memset (result, 0, sizeof (*result));
		result->tm_isdst = -1;
		return E_TIME_PARSE_NONE;
	}

	pos = value;

	/* Skip any whitespace. */
	while (isspace (*pos))
		pos++;

	/* strptime format for a time of day, in 12-hour format.
	   If it is not appropriate in the locale set to an empty string. */
	format[0] = _("%I:%M:%S %p");

	/* strptime format for a time of day, in 24-hour format. */
	format[1] = _("%H:%M:%S");

	/* strptime format for time of day, without seconds, 12-hour format.
	   If it is is not appropriate in the locale set to an empty string. */
	format[2] = _("%I:%M %p");

	/* strptime format for time of day, without seconds 24-hour format. */
	format[3] = _("%H:%M");

	for (i = 0; i < sizeof (format) / sizeof (format[0]); i++) {
		memset (result, 0, sizeof (*result));
		parse_end = strptime (pos, format[i], result);
		if (parse_end) {
			pos = parse_end;
			parsed_time = TRUE;
			break;
		}
	}

	result->tm_isdst = -1;

	/* If we don't have a date or a time it must be invalid. */
	if (!parsed_time)
		return E_TIME_PARSE_INVALID;

	return E_TIME_PARSE_OK;
}


/* Returns whether a string is NULL, empty, or full of whitespace */
static gboolean
string_is_empty (const char *value)
{
	const char *p;
	gboolean empty = TRUE;

	if (value) {
		p = value;
		while (*p) {
			if (!isspace (*p)) {
				empty = FALSE;
				break;
			}
			p++;
		}
	}
	return empty;
}


/* Creates a string representation of a time value and stores it in buffer.
   buffer_size should be about 64 to be safe. If show_midnight is FALSE, and
   the time is midnight, then we just show the date. If show_zero_seconds
   is FALSE, then if the time has zero seconds only the hour and minute are
   shown. */
void
e_time_format_date_and_time		(struct tm	*date_tm,
					 gboolean	 use_24_hour_format,
					 gboolean	 show_midnight,
					 gboolean	 show_zero_seconds,
					 char		*buffer,
					 int		 buffer_size)
{
	char *format;

	if (!show_midnight && date_tm->tm_hour == 0
	    && date_tm->tm_min == 0 && date_tm->tm_sec == 0) {
		/* strftime format of a weekday and a date. */
		format = _("%a %m/%d/%Y");
	} else if (use_24_hour_format) {
		if (!show_zero_seconds && date_tm->tm_sec == 0)
			/* strftime format of a weekday, a date and a
			   time, in 24-hour format, without seconds. */
			format = _("%a %m/%d/%Y %H:%M");
		else
			/* strftime format of a weekday, a date and a
			   time, in 24-hour format. */
			format = _("%a %m/%d/%Y %H:%M:%S");
	} else {
		if (!show_zero_seconds && date_tm->tm_sec == 0)
			/* strftime format of a weekday, a date and a
			   time, in 12-hour format, without seconds. */
			format = _("%a %m/%d/%Y %I:%M %p");
		else
			/* strftime format of a weekday, a date and a
			   time, in 12-hour format. */
			format = _("%a %m/%d/%Y %I:%M:%S %p");
	}

	/* strftime returns 0 if the string doesn't fit, and leaves the buffer
	   undefined, so we set it to the empty string in that case. */
	if (strftime (buffer, buffer_size, format, date_tm) == 0)
		buffer[0] = '\0';
}


/* Creates a string representation of a time value and stores it in buffer.
   buffer_size should be about 64 to be safe. */
void
e_time_format_time			(struct tm	*date_tm,
					 gboolean	 use_24_hour_format,
					 gboolean	 show_zero_seconds,
					 char		*buffer,
					 int		 buffer_size)
{
	char *format;

	if (use_24_hour_format) {
		if (!show_zero_seconds && date_tm->tm_sec == 0)
			/* strftime format of a time in 24-hour format,
			   without seconds. */
			format = _("%H:%M");
		else
			/* strftime format of a time in 24-hour format. */
			format = _("%H:%M:%S");
	} else {
		if (!show_zero_seconds && date_tm->tm_sec == 0)
			/* strftime format of a time in 12-hour format,
			   without seconds. */
			format = _("%I:%M %p");
		else
			/* strftime format of a time in 12-hour format. */
			format = _("%I:%M:%S %p");
	}
			
	/* strftime returns 0 if the string doesn't fit, and leaves the buffer
	   undefined, so we set it to the empty string in that case. */
	if (strftime (buffer, buffer_size, format, date_tm) == 0)
		buffer[0] = '\0';
}
