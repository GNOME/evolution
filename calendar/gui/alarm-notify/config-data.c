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

#include <string.h>
#include <libedataserver/e-source-list.h>
#include "config-data.h"



#define KEY_LAST_NOTIFICATION_TIME "/apps/evolution/calendar/notify/last_notification_time"
#define KEY_PROGRAMS "/apps/evolution/calendar/notify/programs"

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

gboolean
config_data_get_notify_with_tray (void)
{
	ensure_inited ();

	return gconf_client_get_bool (conf_client,
				      "/apps/evolution/calendar/notify/notify_with_tray",
				      NULL);
}

/**
 * config_data_set_last_notification_time:
 * @t: A time value.
 * 
 * Saves the last notification time so that it can be fetched the next time the
 * alarm daemon is run.  This way the daemon can show alarms that should have
 * triggered while it was not running.
 **/
void
config_data_set_last_notification_time (time_t t)
{
	GConfClient *conf_client;
	time_t current_t;

	g_return_if_fail (t != -1);

	if (!(conf_client = config_data_get_conf_client ()))
		return;

	/* we only store the new notification time if it is bigger
	   than the already stored one */
	current_t = gconf_client_get_int (conf_client, KEY_LAST_NOTIFICATION_TIME, NULL);
	if (t > current_t)
		gconf_client_set_int (conf_client, KEY_LAST_NOTIFICATION_TIME, t, NULL);
}

/**
 * config_data_get_last_notification_time:
 * 
 * Queries the last saved value for alarm notification times.
 * 
 * Return value: The last saved value, or -1 if no value had been saved before.
 **/
time_t
config_data_get_last_notification_time (void)
{
	GConfClient *conf_client;
	GConfValue *value;

	if (!(conf_client = config_data_get_conf_client ()))
		return -1;

	value = gconf_client_get_without_default (conf_client, KEY_LAST_NOTIFICATION_TIME, NULL);
	if (value)
		return (time_t) gconf_value_get_int (value);

	return time (NULL);
}

/**
 * config_data_save_blessed_program:
 * @program: a program name
 * 
 * Saves a program name as "blessed"
 **/
void
config_data_save_blessed_program (const char *program)
{
	GConfClient *conf_client;
	GSList *l;

	if (!(conf_client = config_data_get_conf_client ()))
		return;

	l = gconf_client_get_list (conf_client, KEY_PROGRAMS, GCONF_VALUE_STRING, NULL);
	l = g_slist_append (l, g_strdup (program));
	gconf_client_set_list (conf_client, KEY_PROGRAMS, GCONF_VALUE_STRING, l, NULL);
	g_slist_foreach (l, (GFunc) g_free, NULL);
	g_slist_free (l);
}

/**
 * config_data_is_blessed_program:
 * @program: a program name
 * 
 * Checks to see if a program is blessed
 * 
 * Return value: TRUE if program is blessed, FALSE otherwise
 **/
gboolean
config_data_is_blessed_program (const char *program)
{
	GConfClient *conf_client;
	GSList *l, *n;
	gboolean found = FALSE;

	if (!(conf_client = config_data_get_conf_client ()))
		return FALSE;

	l = gconf_client_get_list (conf_client, KEY_PROGRAMS, GCONF_VALUE_STRING, NULL);
	while (l) {
		n = l->next;
		if (!found)
			found = strcmp ((char *) l->data, program) == 0;
		g_free (l->data);
		g_slist_free_1 (l);
		l = n;
	}

	return found;
}
