/* Evolution calendar - Configuration values for the alarm notification daemon
 *
 * Copyright (C) 2001 Ximian, Inc.
 *
 * Authors: Federico Mena-Quintero <federico@ximian.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307, USA.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "config-data.h"
#include "save.h"



/* Whether we have initied ourselves by reading the data from the configuration engine */
static gboolean inited;

/* Configuration values */
static icaltimezone *local_timezone;
static gboolean use_24_hour_format;



/* Copied from ../calendar-config.c; returns whether the locale has 'am' and
 * 'pm' strings defined.
 */
static gboolean
locale_supports_12_hour_format (void)
{  
	char s[16];
	time_t t = 0;

	strftime (s, sizeof s, "%p", gmtime (&t));
	return s[0] != '\0';
}

/* Ensures that the configuration values have been read */
static void
ensure_inited (void)
{
	Bonobo_ConfigDatabase db;
	char *location;

	if (inited)
		return;

	inited = TRUE;

	db = get_config_db ();
	if (db == CORBA_OBJECT_NIL) {
		/* This sucks */
		local_timezone = icaltimezone_get_utc_timezone ();

		/* This sucks as well */
		use_24_hour_format = TRUE;

		return;
	}

	location = bonobo_config_get_string (db, "/Calendar/Display/Timezone", NULL);
	if (location) {
		local_timezone = icaltimezone_get_builtin_timezone (location);
		g_free (location);
	} else
		local_timezone = icaltimezone_get_utc_timezone ();

	if (locale_supports_12_hour_format ()) {
		/* Wasn't the whole point of a configuration engine *NOT* to
		 * have apps specify their own stupid defaults everywhere, but
		 * just in a schema file?
		 */
		use_24_hour_format = bonobo_config_get_boolean_with_default (
			db,
			"/Calendar/Display/Use24HourFormat", FALSE, NULL);
	} else
		use_24_hour_format = TRUE;

	discard_config_db (db);
}

icaltimezone *
config_data_get_timezone (void)
{
	ensure_inited ();

	return local_timezone;
}

gboolean
config_data_get_24_hour_format (void)
{
	ensure_inited ();

	return use_24_hour_format;
}
