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

#ifdef __linux__
/* We need this to get a prototype for strptime. */
#define _GNU_SOURCE
#endif /* __linux__ */

#include <time.h>
#include <sys/time.h>
#include <gal/widgets/e-unicode.h>

#ifdef __linux__
#undef _GNU_SOURCE
#endif /* __linux__ */

#include <string.h>
#include <ctype.h>
#include <glib.h>
#include <libgnome/gnome-i18n.h>
#include <gal/util/e-util.h>
#include "e-time-utils.h"


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


/* Takes a number of format strings for strptime() and attempts to parse a
 * string with them.
 */
static ETimeParseStatus
parse_with_strptime (const char *value, struct tm *result, const char **formats, int n_formats)
{
	const char *parse_end = NULL, *pos;
	gchar *locale_str;
	gchar *format_str;
	ETimeParseStatus parse_ret;
	gboolean parsed = FALSE;
	int i;

	if (string_is_empty (value)) {
		memset (result, 0, sizeof (*result));
		result->tm_isdst = -1;
		return E_TIME_PARSE_NONE;
	}
	
	locale_str = e_utf8_to_locale_string (value);

	pos = (const char *) locale_str;

	/* Skip whitespace */
	while (isspace (*pos))
		pos++;

	/* Try each of the formats in turn */

	for (i = 0; i < n_formats; i++) {
		memset (result, 0, sizeof (*result));
		format_str = e_utf8_to_locale_string (formats[i]);
		parse_end = strptime (pos, format_str, result);
		g_free (format_str);
		if (parse_end) {
			parsed = TRUE;
			break;
		}
	}

	result->tm_isdst = -1;

	parse_ret =  E_TIME_PARSE_INVALID;

	/* If we parsed something, make sure we parsed the entire string. */
	if (parsed) {
		/* Skip whitespace */
		while (isspace (*parse_end))
			parse_end++;

		if (*parse_end == '\0')
			parse_ret = E_TIME_PARSE_OK;
	}

	g_free (locale_str);

	return (parse_ret);

}


/* Returns TRUE if the locale has 'am' and 'pm' strings defined, in which
   case the user can choose between 12 and 24-hour time formats. */
static gboolean
locale_supports_12_hour_format (void)
{  
	struct tm tmp_tm = { 0 };
	char s[16];

	e_utf8_strftime (s, sizeof (s), "%p", &tmp_tm);
	return s[0] != '\0';
}


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
	struct tm *today_tm;
	time_t t;
	const char *format[16];
	int num_formats = 0;
	gboolean use_12_hour_formats = locale_supports_12_hour_format ();
	ETimeParseStatus status;

	if (string_is_empty (value)) {
		memset (result, 0, sizeof (*result));
		result->tm_isdst = -1;
		return E_TIME_PARSE_NONE;
	}

	/* We'll parse the whole date and time in one go, otherwise we get
	   into i18n problems. We attempt to parse with several formats,
	   longest first. Note that we only use the '%p' specifier if the
	   locale actually has 'am' and 'pm' strings defined, otherwise we
	   will get incorrect results. Note also that we try to use exactly
	   the same strings as in e_time_format_date_and_time(), to try to
	   avoid i18n problems. We also use cut-down versions, so users don't
	   have to type in the weekday or the seconds, for example.
	   Note that all these formats include the full date, and the time
	   will be set to 00:00:00 before parsing, so we don't need to worry
	   about filling in any missing fields after parsing. */

	/*
	 * Try the full times, with the weekday. Then try without seconds,
	 * and without minutes, and finally with no time at all.
	 */
	if (use_12_hour_formats) {
		/* strptime format of a weekday, a date and a time,
		   in 12-hour format. */
		format[num_formats++] = _("%a %m/%d/%Y %I:%M:%S %p");
	}

	/* strptime format of a weekday, a date and a time, 
	   in 24-hour format. */
	format[num_formats++] = _("%a %m/%d/%Y %H:%M:%S");

	if (use_12_hour_formats) {
		/* strptime format of a weekday, a date and a time,
		   in 12-hour format, without seconds. */
		format[num_formats++] = _("%a %m/%d/%Y %I:%M %p");
	}

	/* strptime format of a weekday, a date and a time,
	   in 24-hour format, without seconds. */
	format[num_formats++] = _("%a %m/%d/%Y %H:%M");

	if (use_12_hour_formats) {
		/* strptime format of a weekday, a date and a time,
		   in 12-hour format, without minutes or seconds. */
		format[num_formats++] = _("%a %m/%d/%Y %I %p");
	}

	/* strptime format of a weekday, a date and a time,
	   in 24-hour format, without minutes or seconds. */
	format[num_formats++] = _("%a %m/%d/%Y %H");

	/* strptime format of a weekday and a date. */
	format[num_formats++] = _("%a %m/%d/%Y");


	/*
	 * Now try all the above formats again, but without the weekday.
	 */
	if (use_12_hour_formats) {
		/* strptime format of a date and a time, in 12-hour format. */
		format[num_formats++] = _("%m/%d/%Y %I:%M:%S %p");
	}

	/* strptime format of a date and a time, in 24-hour format. */
	format[num_formats++] = _("%m/%d/%Y %H:%M:%S");

	if (use_12_hour_formats) {
		/* strptime format of a date and a time, in 12-hour format,
		   without seconds. */
		format[num_formats++] = _("%m/%d/%Y %I:%M %p");
	}

	/* strptime format of a date and a time, in 24-hour format,
	   without seconds. */
	format[num_formats++] = _("%m/%d/%Y %H:%M");

	if (use_12_hour_formats) {
		/* strptime format of a date and a time, in 12-hour format,
		   without minutes or seconds. */
		format[num_formats++] = _("%m/%d/%Y %I %p");
	}

	/* strptime format of a date and a time, in 24-hour format,
	   without minutes or seconds. */
	format[num_formats++] = _("%m/%d/%Y %H");

	/* strptime format of a weekday and a date. */
	format[num_formats++] = _("%m/%d/%Y");


	status = parse_with_strptime (value, result, format, num_formats);
	/* Note that we checked if it was empty already, so it is either OK
	   or INVALID here. */
	if (status == E_TIME_PARSE_OK) {
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
		/* Now we try to just parse a time, assuming the current day.*/
		status = e_time_parse_time (value, result);
		if (status == E_TIME_PARSE_OK) {
			/* We fill in the current day. */
			t = time (NULL);
			today_tm = localtime (&t);
			result->tm_mday = today_tm->tm_mday;
			result->tm_mon  = today_tm->tm_mon;
			result->tm_year = today_tm->tm_year;
		}
	}

	return status;
}

/**
 * e_time_parse_date:
 * @value: A date string.
 * @result: Return value for the parsed date.
 * 
 * Takes in a date string entered by the user and tries to convert it to
 * a struct tm.
 * 
 * Return value: Result code indicating whether the @value was an empty
 * string, a valid date, or an invalid date.
 **/
ETimeParseStatus
e_time_parse_date (const char *value, struct tm *result)
{
	const char *format[2];
	struct tm *today_tm;
	time_t t;
	ETimeParseStatus status;

	g_return_val_if_fail (value != NULL, E_TIME_PARSE_INVALID);
	g_return_val_if_fail (result != NULL, E_TIME_PARSE_INVALID);

	/* strptime format of a weekday and a date. */
	format[0] = _("%a %m/%d/%Y");

	/* This is the preferred date format for the locale. */
	format[1] = _("%m/%d/%Y");

	status = parse_with_strptime (value, result, format, sizeof (format) / sizeof (format[0]));
	if (status == E_TIME_PARSE_OK) {
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
	}
	
	return status;
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
e_time_parse_time (const char *value, struct tm *result)
{
	const char *format[6];
	int num_formats = 0;
	gboolean use_12_hour_formats = locale_supports_12_hour_format ();

	if (use_12_hour_formats) {
		/* strptime format for a time of day, in 12-hour format. */
		format[num_formats++] = _("%I:%M:%S %p");
	}

	/* strptime format for a time of day, in 24-hour format. */
	format[num_formats++] = _("%H:%M:%S");

	if (use_12_hour_formats) {
		/* strptime format for time of day, without seconds,
		   in 12-hour format. */
		format[num_formats++] = _("%I:%M %p");
	}

	/* strptime format for time of day, without seconds 24-hour format. */
	format[num_formats++] = _("%H:%M");

	if (use_12_hour_formats) {
		/* strptime format for hour and AM/PM, 12-hour format. */
		format[num_formats++] = _("%I %p");
	}

	/* strptime format for hour, 24-hour format. */
	format[num_formats++] = "%H";

	return parse_with_strptime (value, result, format, num_formats);
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
	if (e_utf8_strftime (buffer, buffer_size, format, date_tm) == 0)
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
	if (e_utf8_strftime (buffer, buffer_size, format, date_tm) == 0)
		buffer[0] = '\0';
}


/* Like mktime(3), but assumes UTC instead of local timezone. */
time_t
e_mktime_utc (struct tm *tm)
{
	time_t tt;

	tm->tm_isdst = -1;
	tt = mktime (tm);

#if defined (HAVE_TM_GMTOFF)
	tt += tm->tm_gmtoff;
#elif defined (HAVE_TIMEZONE)
	if (tm->tm_isdst > 0) {
  #if defined (HAVE_ALTZONE)
		tt -= altzone;
  #else /* !defined (HAVE_ALTZONE) */
		tt -= (timezone - 3600);
  #endif
	} else
		tt -= timezone;
#endif

	return tt;
}

/* Like localtime_r(3), but also returns an offset in seconds after UTC.
   (Calling gmtime with tt + offset would generate the same tm) */
void
e_localtime_with_offset (time_t tt, struct tm *tm, int *offset)
{
	localtime_r (&tt, tm);

#if defined (HAVE_TM_GMTOFF)
	*offset = tm->tm_gmtoff;
#elif defined (HAVE_TIMEZONE)
	if (tm->tm_isdst > 0) {
  #if defined (HAVE_ALTZONE)
		*offset = -altzone;
  #else /* !defined (HAVE_ALTZONE) */
		*offset = -(timezone - 3600);
  #endif
	} else
		*offset = -timezone;
#endif
}
