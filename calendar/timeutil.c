/* Miscellaneous time-related utilities
 *
 * Copyright (C) 1998 The Free Software Foundation
 *
 * Authors: Federico Mena <federico@nuclecu.unam.mx>
 *          Miguel de Icaza <miguel@nuclecu.unam.mx>
 */

#include <libgnome/libgnome.h>
#include "timeutil.h"

#define digit_at(x,y) (x [y] - '0')
	
time_t
time_from_isodate (char *str)
{
	struct tm my_tm;

	my_tm.tm_year = digit_at (str, 0) * 1000 + digit_at (str, 1) * 100 +
		digit_at (str, 2) * 10 + digit_at (str, 3);

	my_tm.tm_mon  = digit_at (str, 4) * 10 + digit_at (str, 5);
	my_tm.tm_mday = digit_at (str, 6) * 10 + digit_at (str, 7);
	my_tm.tm_hour = digit_at (str, 9) * 10 + digit_at (str, 10);
	my_tm.tm_min  = digit_at (str, 11) * 10 + digit_at (str, 12);
	my_tm.tm_sec  = digit_at (str, 13) * 10 + digit_at (str, 14);
	my_tm.tm_isdst = -1;
	
	return mktime (&my_tm);
}

char *
isodate_from_time_t (time_t t)
{
	struct tm *tm;
	static char isotime [40];

	tm = localtime (&t);
	strftime (isotime, sizeof (isotime)-1, "%Y%m%dT%H%M%sZ", tm);
	return &isotime;
}

time_t
time_from_start_duration (time_t start, char *duration)
{
	printf ("Not yet implemented\n");
	return 0;
}

char *
format_simple_hour (int hour, int use_am_pm)
{
	static char buf[256];

	/* I don't know whether this is the best way to internationalize it.
	 * Does any language use different conventions? - Federico
	 */

	if (use_am_pm)
		sprintf (buf, "%d%s",
			 (hour == 0) ? 12 : (hour > 12) ? (hour - 12) : hour,
			 (hour < 12) ? _("am") : _("pm"));
	else
		sprintf (buf, "%02d%s", hour, _("h"));

	return buf;

}
