/*
 * Creates an HTML rendering for this month
 * Copyright (C) 1999 the Free Software Foundation
 *
 * Authors:
 *          Miguel de Icaza (miguel@kernel.org)
 */
#include <config.h>
#include <gnome.h>
#include <pwd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <string.h>
#include <ctype.h>
#include "calendar.h"
#include "alarm.h"
#include "eventedit.h"
#include "gnome-cal.h"
#include "main.h"
#include "timeutil.h"

static void
make_html_header (GnomeCalendar *gcal, GString *s)
{
	g_string_sprintf (s, 
		 "<html>\n"
		 "  <head>\n"
		 "    <title>%s</title>\n"
		 "  </head>\n"
		 "  <body>\n",
		 gcal->cal->title);
}

static void
make_html_footer (GString *s)
{
	g_string_sprintf (s, "</html>");
}

static void
make_days_headers (GString *s)
{
	g_string_append (s, 
			 "<p><table border=1>\n"
			 "<tr>\n"
			 "  <td></td>\n"
			 "  <td>MONDAY</td>\n"
			 "  <td>TUESDAY</td>\n"
			 "  <td>WEDNESDAY</td>\n"
			 "  <td>THURSDAY</td>\n"
			 "  <td>FRIDAY</td>\n"
			 "</tr>\n");
}

static void
make_days (GnomeCalendar *gcal, GString *s)
{
	struct tm tm, month;
	time_t month_start;
	int day;
	time_t now = time (NULL);
	
	make_days_headers (s);
	tm = *localtime (&now);
	tm.tm_hour = 0;
	tm.tm_min = 0;
	tm.tm_sec = 0;
	tm.tm_mday = 1;
	month_start = mktime (&tm);
	month = *localtime (&month_start);

	for (day = 0; day < month.tm_mday; day++){
		
	}
#if 0
	day = 0;
	for (y = 0; y < 5; y++){
		for (x = 0; x < 7; x++){
			if (month.tm_mday < day
		}
	}
#endif
}

void
make_month_html (GnomeCalendar *gcal, char *output)
{
	FILE *f;
	GString *s;
	
	g_return_if_fail (gcal != NULL);
	g_return_if_fail (GNOME_IS_CALENDAR (gcal));

	f = fopen (output, "w");
	if (!f){
		g_warning ("Add nice error message here");
		return;
	}

	s = g_string_new ("");
	
	make_html_header (gcal, s);
	make_days (gcal, s);
	make_html_footer (s);

	fwrite (s->str, strlen (s->str), 1, f);

	g_string_free (s, TRUE);
	fclose (f);
}
