/* Evolution calendar - Configuration values for the alarm notification daemon
 *
 * Copyright (C) 2001 Ximian, Inc.
 *
 * Authors: Federico Mena-Quintero <federico@ximian.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of version 2 of the GNU General Public
 * License as published by the Free Software Foundation.
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

#ifdef HAVE_CONFIOH
#include <config.h>
#endif

#include "config-data.h"
#include "save.h"



/* Whether we have initied ourselves by reading the data from the configuration engine */
static gboolean inited = FALSE;
static EConfigListener *config;



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

static void
do_cleanup (void)
{
	g_object_unref (config);
	config = NULL;
	inited = FALSE;
}

/* Ensures that the configuration values have been read */
static void
ensure_inited (void)
{
	if (inited)
		return;

	inited = TRUE;

	config = e_config_listener_new ();
	if (!E_IS_CONFIG_LISTENER (config)) {
		inited = FALSE;
		return;
	}

	g_atexit ((GVoidFunc) do_cleanup);
}

EConfigListener *
config_data_get_listener (void)
{
	ensure_inited ();
	return config;
}

icaltimezone *
config_data_get_timezone (void)
{
	char *location;
	icaltimezone *local_timezone;

	ensure_inited ();

	location = e_config_listener_get_string_with_default (config, 
							      "/apps/evolution/calendar/display/timezone",
							      "UTC", NULL);
	if (location && location[0]) {
		local_timezone = icaltimezone_get_builtin_timezone (location);
	} else {
		local_timezone = icaltimezone_get_utc_timezone ();
	}

	g_free (location);

	return local_timezone;
}

gboolean
config_data_get_24_hour_format (void)
{
	ensure_inited ();

	if (locale_supports_12_hour_format ()) {
		return e_config_listener_get_boolean_with_default (
			config,
			"/apps/evolution/calendar/display/use_24hour_format", FALSE, NULL);
	}

	return TRUE;
}
