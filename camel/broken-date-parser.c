/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 *  Authors: Jeffrey Stedfast <fejj@helixcode.com>
 *
 *  Copyright 2000 Helix Code, Inc. (www.helixcode.com)
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Street #330, Boston, MA 02111-1307, USA.
 *
 */

#include "broken-date-parser.h"

/* prototypes for functions dealing with broken date formats */
static GList *datetok (const gchar *date);
static gint get_days_in_month (gint mon, gint year);
static gint get_weekday (gchar *str);
static gint get_month (gchar *str);

static char *tz_months [] = {
	"Jan", "Feb", "Mar", "Apr", "May", "Jun",
	"Jul", "Aug", "Sep", "Oct", "Nov", "Dec"
};

/*****************************************************************************
 * The following functions are here in the case of badly broken date formats *
 *                                                                           *
 * -- fejj@helixcode.com                                                     *
 *****************************************************************************/

typedef struct {
	gchar dow[6];   /* day of week (should only need 4 chars) */
	gint day;
	gint mon;       /* 1->12 or 0 if invalid */
	gint year;
	gint hour;
	gint min;
	gint sec;
	gchar zone[6];  /* time zone */
} date_t;

static
GList *datetok (const gchar *date)
{
	GList *tokens = NULL;
	gchar *token, *start, *end;
	
	start = (gchar *) date;
	while (*start) {
		/* find the end of this token */
		for (end = start; *end && *end != ' '; end++);
		
		token = g_strndup (start, (end - start));
		
		if (token && *token)
			tokens = g_list_append (tokens, token);
		else
			g_free (token);

		if (*end)
			start = end + 1;
		else
			break;
	}

	return tokens;
}

static gint
get_days_in_month (gint mon, gint year)
{
	switch (mon) {
	case 1: case 3: case 5: case 7: case 8: case 10: case 12:
		return 31;
	case 4: case 6: case 9: case 11:
		return 30;
	case 2:
		if ((year % 4 == 0 && year % 100 != 0) || year % 400 == 0)
			return 29;
		return 28;
	default:
		return 30;
	}
}

static gint
get_weekday (gchar *str)
{
	g_return_val_if_fail ((str != NULL), 0);

	if (strncmp (str, "Mon", 3) == 0) {
		return 1;
	} else if (strncmp (str, "Tue", 3) == 0) {
		return 2;
	} else if (strncmp (str, "Wed", 3) == 0) {
		return 3;
	} else if (strncmp (str, "Thu", 3) == 0) {
		return 4;
	} else if (strncmp (str, "Fri", 3) == 0) {
		return 5;
	} else if (strncmp (str, "Sat", 3) == 0) {
		return 6;
	} else if (strncmp (str, "Sun", 3) == 0) {
		return 7;
	}

	return 0;  /* unknown week day */
}

static gint
get_month (gchar *str)
{
	g_return_val_if_fail (str != NULL, 0);
    
	if (strncmp (str, "Jan", 3) == 0) {
		return 1;
	} else if (strncmp (str, "Feb", 3) == 0) {
		return 2;
	} else if (strncmp (str, "Mar", 3) == 0) {
		return 3;
	} else if (strncmp (str, "Apr", 3) == 0) {
		return 4;
	} else if (strncmp (str, "May", 3) == 0) {
		return 5;
	} else if (strncmp (str, "Jun", 3) == 0) {
		return 6;
	} else if (strncmp (str, "Jul", 3) == 0) {
		return 7;
	} else if (strncmp (str, "Aug", 3) == 0) {
		return 8;
	} else if (strncmp (str, "Sep", 3) == 0) {
		return 9;
	} else if (strncmp (str, "Oct", 3) == 0) {
		return 10;
	} else if (strncmp (str, "Nov", 3) == 0) {
		return 11;
	} else if (strncmp (str, "Dec", 3) == 0) {
		return 12;
	}
    
	return 0;  /* unknown month */
}

gchar *
parse_broken_date (const gchar *datestr)
{
	GList *tokens;
	date_t date;
	gchar *token, *ptr, *newdatestr;
	guint len, i, retval;
	gdouble tz = 0.0;

	memset ((void*)&date, 0, sizeof (date_t));
	g_return_val_if_fail (datestr != NULL, NULL);
	
	tokens = datetok (datestr);
	len = g_list_length (tokens);
	for (i = 0; i < len; i++) {
		token = g_list_nth_data (tokens, i);
		
		if ((retval = get_weekday (token))) {
			strncpy (date.dow, datestr, 4);
		} else if ((retval = get_month (token))) {
			date.mon = retval;
		} else if (strlen (token) <= 2) {
			/* this could be a 1 or 2 digit day of the month */
			for (retval = 1, ptr = token; *ptr; ptr++)
				if (*ptr < '0' || *ptr > '9')
					retval = 0;
			
			if (retval && atoi (token) <= 31 && !date.day)  /* probably should find a better way */
				date.day = atoi (token);
			else                                            /* fubar'd client using a 2-digit year */
				date.year = atoi (token) < 69 ? 2000 + atoi (token) : 1900 + atoi (token);
		} else if (strlen (token) == 4) {
			/* this could be the year... */
			for (retval = 1, ptr = token; *ptr; ptr++)
				if (*ptr < '0' || *ptr > '9')
					retval = 0;
			
			if (retval)
				date.year = atoi (token);
		} else if (strchr (token, ':')) {
			/* this must be the time: hh:mm:ss */
			sscanf (token, "%d:%d:%d", &date.hour, &date.min, &date.sec);
		} else if (*token == '-' || *token == '+') {
			tz = atoi (token) / 100.0;
		}
	}
	
	g_list_free (tokens);
	
	/* adjust times based on time zones */
	
	if (tz != 0) {
		/* check for time-zone shift */
		if (tz > 0) {
			/* correct for positive hours off of UCT */
			date.hour -= (tz / 100);
			tz = (gint)tz % 100;
			
			if (tz > 0) /* correct for positive minutes off of UCT */
				date.min -= (gint)(((gdouble) tz / 100.0) * 60.0);
		} else {
			if (tz < 0) {
				/* correct for negative hours off of UCT */
				tz = -tz;
				date.hour += (tz / 100);
				tz = -((gint)tz % 100);
				
				if (tz < 0)
					date.min -= (gint)(((gdouble) tz / 100.0) * 60.0);
			}
		}
		
		/* adjust seconds to proper range */
		if (date.sec > 59) {
			date.min += (date.sec / 60);
			date.sec = (date.sec % 60);
		}
		
		/* adjust minutes to proper range */
		if (date.min > 59) {
			date.hour += (date.min / 60);
			date.min = (date.min % 60);
		} else {
			if (date.min < 0) {
				date.min = -date.min;
				date.hour -= (date.min / 60) - 1;
				date.min = 60 - (date.min % 60);
			}
		}
		
		/* adjust hours to the proper randge */
		if (date.hour > 23) {
			date.day += (date.hour / 24);
			date.hour -= (date.hour % 24);
		} else {
			if (date.hour < 0) {
				date.hour = -date.hour;
				date.day -= (date.hour / 24) - 1;
				date.hour = 24 - (date.hour % 60);
			}
		}
		
		/* adjust days to the proper range */
		while (date.day > get_days_in_month (date.mon, date.year)) {
			date.day -= get_days_in_month (date.mon, date.year);
			date.mon++;
			if (date.mon > 12) {
				date.year += (date.mon / 12);
				date.mon = (date.mon % 12);
				if (date.mon == 0) {
					/* month sanity check */
					date.mon = 12;
					date.year -= 1;
				}
			}
		}
		
		while (date.day < 1) {
			date.day += get_days_in_month (date.mon, date.year);
			date.mon--;
			if (date.mon < 1) {
				date.mon = -date.mon;
				date.year -= (date.mon / 12) - 1;
				date.mon = 12 - (date.mon % 12);
			}
		}
		
		/* adjust months to the proper range */
		if (date.mon > 12) {
			date.year += (date.mon / 12);
			date.mon = (date.mon % 12);
			if (date.mon == 0) {
				/* month sanity check */
				date.mon = 12;
				date.year -= 1;
			}
		} else {
			if (date.mon < 1) {
				date.mon = -date.mon;
				date.year -= (date.mon / 12) - 1;
				date.mon = 12 - (date.mon % 12);
			}
		}
	}

	/* now lets print this date into a string with the correct format */
	newdatestr = g_strdup_printf ("%s, %d %s %d %s%d:%s%d:%s%d -0000",
				      date.dow, date.day, tz_months[date.mon-1],
				      date.year,
				      date.hour > 10 ? "" : "0", date.hour,
				      date.min > 10 ? "" : "0", date.min,
				      date.sec > 10 ? "" : "0", date.sec);
	
	return newdatestr;
}

/*****************************************************************************
 * This ends the code for the broken date parser...                          *
 *                                                                           *
 * -- fejj@helixcode.com                                                     *
 *****************************************************************************/
