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

#include <libedataserver/e-source-list.h>
#include "config-data.h"
#include "save.h"



/* Whether we have initied ourselves by reading the data from the configuration engine */
static gboolean inited = FALSE;
static GConfClient *conf_client = NULL;
static ESourceList *calendar_source_list = NULL, *tasks_source_list = NULL;



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
	if (calendar_source_list) {
		g_object_unref (calendar_source_list);
		calendar_source_list = NULL;
	}

	if (tasks_source_list) {
		g_object_unref (tasks_source_list);
		tasks_source_list = NULL;
	}

	g_object_unref (conf_client);
	conf_client = NULL;

	inited = FALSE;
}

/* Ensures that the configuration values have been read */
static void
ensure_inited (void)
{
	if (inited)
		return;

	inited = TRUE;

	conf_client = gconf_client_get_default ();
	if (!GCONF_IS_CLIENT (conf_client)) {
		inited = FALSE;
		return;
	}

	g_atexit ((GVoidFunc) do_cleanup);

	/* load the sources for calendars and tasks */
	calendar_source_list = e_source_list_new_for_gconf (conf_client,
							    "/apps/evolution/calendar/sources");
	tasks_source_list = e_source_list_new_for_gconf (conf_client,
							 "/apps/evolution/tasks/sources");

}

GConfClient *
config_data_get_conf_client (void)
{
	ensure_inited ();
	return conf_client;
}

icaltimezone *
config_data_get_timezone (void)
{
	char *location;
	icaltimezone *local_timezone;

	ensure_inited ();

	location = gconf_client_get_string (conf_client, 
					    "/apps/evolution/calendar/display/timezone",
					    NULL);
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
		return gconf_client_get_bool (conf_client,
					      "/apps/evolution/calendar/display/use_24hour_format",
					      NULL);
	}

	return TRUE;
}

GPtrArray *
config_data_get_calendars_to_load (void)
{
	GPtrArray *cals;
	GSList *groups, *gl, *sources, *sl;

	ensure_inited ();

	/* create the array to be returned */
	cals = g_ptr_array_new ();

	/* process calendar sources */
	groups = e_source_list_peek_groups (calendar_source_list);
	for (gl = groups; gl != NULL; gl = gl->next) {
		sources = e_source_group_peek_sources (E_SOURCE_GROUP (gl->data));
		for (sl = sources; sl != NULL; sl = sl->next) {
			g_ptr_array_add (cals, sl->data);
		}
	}

	/* process tasks sources */
	groups = e_source_list_peek_groups (tasks_source_list);
	for (gl = groups; gl != NULL; gl = gl->next) {
		sources = e_source_group_peek_sources (E_SOURCE_GROUP (gl->data));
		for (sl = sources; sl != NULL; sl = sl->next) {
			g_ptr_array_add (cals, sl->data);
		}
	}

	return cals;
}
