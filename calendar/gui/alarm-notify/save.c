/* Evolution calendar - Functions to save alarm notification times
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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <string.h>
#include <bonobo/bonobo-arg.h>
#include <bonobo/bonobo-exception.h>
#include <bonobo/bonobo-moniker-util.h>
#include "evolution-calendar.h"
#include "config-data.h"
#include "save.h"



/* Key names for the configuration values */

#define KEY_LAST_NOTIFICATION_TIME "/Calendar/AlarmNotify/LastNotificationTime"
#define KEY_NUM_CALENDARS_TO_LOAD "/Calendar/AlarmNotify/NumCalendarsToLoad"
#define BASE_KEY_CALENDAR_TO_LOAD "/Calendar/AlarmNotify/CalendarToLoad"
#define KEY_NUM_BLESSED_PROGRAMS "/Calendar/AlarmNotify/NumBlessedPrograms"
#define BASE_KEY_BLESSED_PROGRAM "/Calendar/AlarmNotify/BlessedProgram"



/**
 * save_notification_time:
 * @t: A time value.
 * 
 * Saves the last notification time so that it can be fetched the next time the
 * alarm daemon is run.  This way the daemon can show alarms that should have
 * triggered while it was not running.
 **/
void
save_notification_time (time_t t)
{
	EConfigListener *cl;
	time_t current_t;

	g_return_if_fail (t != -1);

	if (!(cl = config_data_get_listener ()))
		return;

	/* we only store the new notification time if it is bigger
	   than the already stored one */
	current_t = e_config_listener_get_long_with_default (cl, KEY_LAST_NOTIFICATION_TIME,
							     -1, NULL);
	if (t > current_t)
		e_config_listener_set_long (cl, KEY_LAST_NOTIFICATION_TIME, (long) t);
}

/**
 * get_saved_notification_time:
 * 
 * Queries the last saved value for alarm notification times.
 * 
 * Return value: The last saved value, or -1 if no value had been saved before.
 **/
time_t
get_saved_notification_time (void)
{
	EConfigListener *cl;
	long t;

	if (!(cl = config_data_get_listener ()))
		return -1;

	t = e_config_listener_get_long_with_default (cl, KEY_LAST_NOTIFICATION_TIME, -1, NULL);

	return (time_t) t;
}

/**
 * save_calendars_to_load:
 * @uris: A list of URIs of calendars.
 * 
 * Saves the list of calendars that should be loaded the next time the alarm
 * daemon starts up.
 **/
void
save_calendars_to_load (GPtrArray *uris)
{
	EConfigListener *cl;
	int len, i;

	g_return_if_fail (uris != NULL);

	if (!(cl = config_data_get_listener ()))
		return;

	len = uris->len;

	e_config_listener_set_long (cl, KEY_NUM_CALENDARS_TO_LOAD, len);

	for (i = 0; i < len; i++) {
		const char *uri;
		char *key;

		uri = uris->pdata[i];

		key = g_strdup_printf ("%s%d", BASE_KEY_CALENDAR_TO_LOAD, i);
		e_config_listener_set_string (cl, key, uri);
		g_free (key);
	}
}

/**
 * get_calendars_to_load:
 * 
 * Gets the list of calendars that should be loaded when the alarm daemon starts
 * up.
 * 
 * Return value: A list of URIs, or NULL if the value could not be retrieved.
 **/
GPtrArray *
get_calendars_to_load (void)
{
	EConfigListener *cl;
	GPtrArray *uris;
	int len, i;

	if (!(cl = config_data_get_listener ()))
		return NULL;

	/* Getting the default value below is not necessarily an error, as we
	 * may not have saved the list of calendar yet.
	 */

	len = e_config_listener_get_long_with_default (cl, KEY_NUM_CALENDARS_TO_LOAD, 0, NULL);

	uris = g_ptr_array_new ();
	g_ptr_array_set_size (uris, len);

	for (i = 0; i < len; i++) {
		char *key;
		gboolean used_default;

		key = g_strdup_printf ("%s%d", BASE_KEY_CALENDAR_TO_LOAD, i);
		uris->pdata[i] = e_config_listener_get_string_with_default (cl, key, "", &used_default);
		if (used_default)
			g_message ("get_calendars_to_load(): Could not read calendar name %d", i);

		g_free (key);
	}

	return uris;
}

/**
 * save_blessed_program:
 * @program: a program name
 * 
 * Saves a program name as "blessed"
 **/
void
save_blessed_program (const char *program)
{
	EConfigListener *cl;
	char *key;
	int len;

	g_return_if_fail (program != NULL);

	if (!(cl = config_data_get_listener ()))
		return;

	/* Up the number saved */
	len = e_config_listener_get_long_with_default (cl, KEY_NUM_BLESSED_PROGRAMS, 0, NULL);
	len++;
	
	e_config_listener_set_long (cl, KEY_NUM_BLESSED_PROGRAMS, len);

	/* Save the program name */
	key = g_strdup_printf ("%s%d", BASE_KEY_BLESSED_PROGRAM, len - 1);
	e_config_listener_set_string (cl, key, program);
	g_free (key);
}

/**
 * is_blessed_program:
 * @program: a program name
 * 
 * Checks to see if a program is blessed
 * 
 * Return value: TRUE if program is blessed, FALSE otherwise
 **/
gboolean
is_blessed_program (const char *program)
{
	EConfigListener *cl;
	int len, i;

	g_return_val_if_fail (program != NULL, FALSE);

	if (!(cl = config_data_get_listener ()))
		return FALSE;

	/* Getting the default value below is not necessarily an error, as we
	 * may not have saved the list of calendar yet.
	 */

	len = e_config_listener_get_long_with_default (cl, KEY_NUM_BLESSED_PROGRAMS, 0, NULL);

	for (i = 0; i < len; i++) {
		char *key, *value;
		gboolean used_default;

		key = g_strdup_printf ("%s%d", BASE_KEY_BLESSED_PROGRAM, i);
		value = e_config_listener_get_string_with_default (cl, key, "", &used_default);
		if (used_default)
			g_message ("get_calendars_to_load(): Could not read calendar name %d", i);

		if (value != NULL && !strcmp (value, program)) {
			g_free (key);
			g_free (value);
			return TRUE;
		}
		
		g_free (key);
		g_free (value);
	}

	return FALSE;
}
