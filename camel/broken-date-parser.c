/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 *  Authors: Jeffrey Stedfast <fejj@ximian.com>
 *
 *  Copyright 2000 Ximian, Inc. (www.ximian.com)
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of version 2 of the GNU General Public
 * License as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 */


#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <glib.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <sys/time.h>
#include <time.h>

#include "broken-date-parser.h"
#include "libedataserver/e-time-utils.h"

#define d(x)

#define NUMERIC_CHARS          "1234567890"
#define WEEKDAY_CHARS          "SundayMondayTuesdayWednesdayThursdayFridaySaturday"
#define MONTH_CHARS            "JanuaryFebruaryMarchAprilMayJuneJulyAugustSeptemberOctoberNovemberDecember"
#define TIMEZONE_ALPHA_CHARS   "UTCGMTESTEDTCSTCDTMSTPSTPDTZAMNY()"
#define TIMEZONE_NUMERIC_CHARS "-+1234567890"
#define TIME_CHARS             "1234567890:"

#define DATE_TOKEN_NON_NUMERIC          (1 << 0)
#define DATE_TOKEN_NON_WEEKDAY          (1 << 1)
#define DATE_TOKEN_NON_MONTH            (1 << 2)
#define DATE_TOKEN_NON_TIME             (1 << 3)
#define DATE_TOKEN_HAS_COLON            (1 << 4)
#define DATE_TOKEN_NON_TIMEZONE_ALPHA   (1 << 5)
#define DATE_TOKEN_NON_TIMEZONE_NUMERIC (1 << 6)
#define DATE_TOKEN_HAS_SIGN             (1 << 7)

static unsigned char datetok_table[256] = {
        128,111,111,111,111,111,111,111,111,111,111,111,111,111,111,111,
        111,111,111,111,111,111,111,111,111,111,111,111,111,111,111,111,
        111,111,111,111,111,111,111,111, 79, 79,111,175,111,175,111,111,
         38, 38, 38, 38, 38, 38, 38, 38, 38, 38,119,111,111,111,111,111,
        111, 75,111, 79, 75, 79,105, 79,111,111,107,111,111, 73, 75,107,
         79,111,111, 73, 77, 79,111,109,111, 79, 79,111,111,111,111,111,
        111,105,107,107,109,105,111,107,105,105,111,111,107,107,105,105,
        107,111,105,105,105,105,107,111,111,105,111,111,111,111,111,111,
        111,111,111,111,111,111,111,111,111,111,111,111,111,111,111,111,
        111,111,111,111,111,111,111,111,111,111,111,111,111,111,111,111,
        111,111,111,111,111,111,111,111,111,111,111,111,111,111,111,111,
        111,111,111,111,111,111,111,111,111,111,111,111,111,111,111,111,
        111,111,111,111,111,111,111,111,111,111,111,111,111,111,111,111,
        111,111,111,111,111,111,111,111,111,111,111,111,111,111,111,111,
        111,111,111,111,111,111,111,111,111,111,111,111,111,111,111,111,
        111,111,111,111,111,111,111,111,111,111,111,111,111,111,111,111,
};

/* hrm, is there a library for this shit? */
static struct {
	char *name;
	int offset;
} tz_offsets [] = {
	{ "UT", 0 },
	{ "GMT", 0 },
	{ "EST", -500 },	/* these are all US timezones.  bloody yanks */
	{ "EDT", -400 },
	{ "CST", -600 },
	{ "CDT", -500 },
	{ "MST", -700 },
	{ "MDT", -600 },
	{ "PST", -800 },
	{ "PDT", -700 },
	{ "Z", 0 },
	{ "A", -100 },
	{ "M", -1200 },
	{ "N", 100 },
	{ "Y", 1200 },
};

static char *tm_months[] = {
	"Jan", "Feb", "Mar", "Apr", "May", "Jun",
	"Jul", "Aug", "Sep", "Oct", "Nov", "Dec"
};

static char *tm_days[] = {
	"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"
};


struct _date_token {
	struct _date_token *next;
	const unsigned char *start;
	unsigned int len;
	unsigned int mask;
};

/* This is where it gets ugly... */
static struct _date_token *
datetok (const char *date)
{
	struct _date_token *tokens = NULL, *token, *tail = (struct _date_token *) &tokens;
	const unsigned char *start, *end;
	unsigned int mask;
	
	start = date;
	while (*start) {
		/* kill leading whitespace */
		while (*start && isspace ((int) *start))
			start++;
		
		if (*start == '\0')
			break;
		
		mask = datetok_table[*start];
		
		/* find the end of this token */
		end = start + 1;
		while (*end && !strchr ("-/,\t\r\n ", *end))
			mask |= datetok_table[*end++];
		
		if (end != start) {
			token = g_malloc (sizeof (struct _date_token));
			token->next = NULL;
			token->start = start;
			token->len = end - start;
			token->mask = mask;
			
			tail->next = token;
			tail = token;
		}
		
		if (*end)
			start = end + 1;
		else
			break;
	}
	
	return tokens;
}

static int
decode_int (const unsigned char *in, unsigned int inlen)
{
	register const unsigned char *inptr;
	const unsigned char *inend;
	int sign = 1, val = 0;
	
	inptr = in;
	inend = in + inlen;
	
	if (*inptr == '-') {
		sign = -1;
		inptr++;
	} else if (*inptr == '+')
		inptr++;
	
	for ( ; inptr < inend; inptr++) {
		if (!isdigit ((int) *inptr))
			return  -1;
		else
			val = (val * 10) + (*inptr - '0');
	}
	
	val *= sign;
	
	return val;
}

static int
get_wday (const unsigned char *in, unsigned int inlen)
{
	int wday;
	
	if (inlen < 3)
		return -1;
	
	for (wday = 0; wday < 7; wday++)
		if (!strncasecmp (in, tm_days[wday], 3))
			return wday;
	
	return -1;  /* unknown week day */
}

static int
get_mday (const unsigned char *in, unsigned int inlen)
{
	int mday;
	
	mday = decode_int (in, inlen);
	
	if (mday < 0 || mday > 31)
		mday = -1;
	
	return mday;
}

static int
get_month (const unsigned char *in, unsigned int inlen)
{
	int i;
	
	if (inlen < 3)
		return -1;
	
	for (i = 0; i < 12; i++)
		if (!strncasecmp (in, tm_months[i], 3))
			return i;
	
	return -1;  /* unknown month */
}

static int
get_year (const unsigned char *in, unsigned int inlen)
{
	int year;
	
	year = decode_int (in, inlen);
	if (year == -1)
		return -1;
	
	if (year < 100)
		year += (year < 70) ? 2000 : 1900;
	
	if (year < 1969)
		return -1;
	
	return year;
}

static gboolean
get_time (const unsigned char *in, unsigned int inlen, int *hour, int *min, int *sec)
{
	register const unsigned char *inptr;
	const unsigned char *inend;
	int *val, colons = 0;
	
	*hour = *min = *sec = 0;
	
	inend = in + inlen;
	val = hour;
	for (inptr = in; inptr < inend; inptr++) {
		if (*inptr == ':') {
			colons++;
			switch (colons) {
			case 1:
				val = min;
				break;
			case 2:
				val = sec;
				break;
			default:
				return FALSE;
			}
		} else if (!isdigit ((int) *inptr))
			return FALSE;
		else
			*val = (*val * 10) + (*inptr - '0');
	}
	
	return TRUE;
}

static int
get_tzone (struct _date_token **token)
{
	const unsigned char *inptr, *inend;
	unsigned int inlen;
	int i, t;
	
	for (i = 0; *token && i < 2; *token = (*token)->next, i++) {
		inptr = (*token)->start;
		inlen = (*token)->len;
		inend = inptr + inlen;
		
		if (*inptr == '+' || *inptr == '-') {
			t = decode_int (inptr, inlen);
			if (t < -1200 || t > 1400)
				return -1;
			
			return t;
		} else {
			if (*inptr == '(') {
				inptr++;
				if (*(inend - 1) == ')')
					inlen -= 2;
				else
					inlen--;
			}
			
			for (t = 0; t < 15; t++) {
				unsigned int len = strlen (tz_offsets[t].name);
				
				if (len != inlen)
					continue;
				
				if (!strncmp (inptr, tz_offsets[t].name, len))
					return tz_offsets[t].offset;
			}
		}
	}
	
	return -1;
}

/* This is where things get interesting... ;-) */

#define date_token_mask(t)  (((struct _date_token *) t)->mask)
#define is_numeric(t)       ((date_token_mask (t) & DATE_TOKEN_NON_NUMERIC) == 0)
#define is_weekday(t)       ((date_token_mask (t) & DATE_TOKEN_NON_WEEKDAY) == 0)
#define is_month(t)         ((date_token_mask (t) & DATE_TOKEN_NON_MONTH) == 0)
#define is_time(t)          (((date_token_mask (t) & DATE_TOKEN_NON_TIME) == 0) && (date_token_mask (t) & DATE_TOKEN_HAS_COLON))
#define is_tzone_alpha(t)   ((date_token_mask (t) & DATE_TOKEN_NON_TIMEZONE_ALPHA) == 0)
#define is_tzone_numeric(t) (((date_token_mask (t) & DATE_TOKEN_NON_TIMEZONE_NUMERIC) == 0) && (date_token_mask (t) & DATE_TOKEN_HAS_SIGN))
#define is_tzone(t)         (is_tzone_alpha (t) || is_tzone_numeric (t))

static time_t
decode_broken_date (struct _date_token *tokens, int *tzone)
{
	gboolean got_wday, got_month, got_tzone;
	int hour, min, sec, offset, n;
	struct _date_token *token;
	struct tm tm;
	time_t time;
	
	memset ((void *) &tm, 0, sizeof (struct tm));
	got_wday = got_month = got_tzone = FALSE;
	offset = 0;
	
	token = tokens;
	while (token) {
		if (is_weekday (token) && !got_wday) {
			if ((n = get_wday (token->start, token->len)) != -1) {
				d(printf ("weekday; "));
				got_wday = TRUE;
				tm.tm_wday = n;
				goto next_token;
			}
		}
		
		if (is_month (token) && !got_month) {
			if ((n = get_month (token->start, token->len)) != -1) {
				d(printf ("month; "));
				got_month = TRUE;
				tm.tm_mon = n;
				goto next_token;
			}
		}
		
		if (is_time (token) && !tm.tm_hour && !tm.tm_min && !tm.tm_sec) {
			if (get_time (token->start, token->len, &hour, &min, &sec)) {
				d(printf ("time; "));
				tm.tm_hour = hour;
				tm.tm_min = min;
				tm.tm_sec = sec;
				goto next_token;
			}
		}
		
		if (is_tzone (token) && !got_tzone) {
			struct _date_token *t = token;
			
			if ((n = get_tzone (&t)) != -1) {
				d(printf ("tzone; "));
				got_tzone = TRUE;
				offset = n;
				goto next_token;
			}
		}
		
		if (is_numeric (token)) {
			if (token->len == 4 && !tm.tm_year) {
				if ((n = get_year (token->start, token->len)) != -1) {
					d(printf ("year; "));
					tm.tm_year = n - 1900;
					goto next_token;
				}
			} else {
				if (!got_month && !got_wday && token->next && is_numeric (token->next)) {
					d(printf ("mon; "));
					n = decode_int (token->start, token->len);
					got_month = TRUE;
					tm.tm_mon = n - 1;
					goto next_token;
				} else if (!tm.tm_mday && (n = get_mday (token->start, token->len)) != -1) {
					d(printf ("mday; "));
					tm.tm_mday = n;
					goto next_token;
				} else if (!tm.tm_year) {
					d(printf ("2-digit year; "));
					n = get_year (token->start, token->len);
					tm.tm_year = n - 1900;
					goto next_token;
				}
			}
		}
		
		d(printf ("???; "));
		
	next_token:
		
		token = token->next;
	}
	
	d(printf ("\n"));
	
	time = e_mktime_utc (&tm);
	
	/* time is now GMT of the time we want, but not offset by the timezone ... */
	
	/* this should convert the time to the GMT equiv time */
	time -= ((offset / 100) * 60 * 60) + (offset % 100) * 60;
	
	if (tzone)
		*tzone = offset;
	
	return time;
}


/**
 * parse_broken_date:
 * @datestr: input date string
 * @saveoffset:
 *
 * Decodes the rfc822/broken date string and saves the GMT offset into
 * @saveoffset if non-NULL.
 *
 * Returns the time_t representation of the date string specified by
 * @in. If 'saveoffset' is non-NULL, the value of the timezone offset
 * will be stored.
 **/
time_t
parse_broken_date (const char *datestr, int *saveoffset)
{
	struct _date_token *token, *tokens;
	time_t date;
	
	tokens = datetok (datestr);
	
	date = decode_broken_date (tokens, saveoffset);
	
	/* cleanup */
	while (tokens) {
		token = tokens;
		tokens = tokens->next;
		g_free (token);
	}
	
	return date;
}





#ifdef DATETOK_STANDALONE

static void
table_init ()
{
	int i;
	
	memset (datetok_table, 0, sizeof (datetok_table));
	
	for (i = 0; i < 256; i++) {
		if (!strchr (NUMERIC_CHARS, i))
			datetok_table[i] |= DATE_TOKEN_NON_NUMERIC;
		
		if (!strchr (WEEKDAY_CHARS, i))
			datetok_table[i] |= DATE_TOKEN_NON_WEEKDAY;
		
		if (!strchr (MONTH_CHARS, i))
			datetok_table[i] |= DATE_TOKEN_NON_MONTH;
		
		if (!strchr (TIME_CHARS, i))
			datetok_table[i] |= DATE_TOKEN_NON_TIME;
		
		if (!strchr (TIMEZONE_ALPHA_CHARS, i))
			datetok_table[i] |= DATE_TOKEN_NON_TIMEZONE_ALPHA;
		
		if (!strchr (TIMEZONE_NUMERIC_CHARS, i))
			datetok_table[i] |= DATE_TOKEN_NON_TIMEZONE_NUMERIC;
		
		if (((char) i) == ':')
			datetok_table[i] |= DATE_TOKEN_HAS_COLON;
		
		if (strchr ("+-", i))
			datetok_table[i] |= DATE_TOKEN_HAS_SIGN;
	}
	
	printf ("static unsigned int datetok_table[256] = {");
	for (i = 0; i < 256; i++) {
		if (i % 16 == 0)
			printf ("\n\t");
		printf ("%3d,", datetok_table[i]);
	}
	printf ("\n};\n");
}


int main (int argc, char **argv)
{
	time_t date;
	int offset;
	
	/*table_init ();*/
	
	date = parse_broken_date (argv[1], &offset);
	printf ("%d; %d\n", date, offset);
	
	return 0;
}

#endif /* DATETOK_STANDALONE */
