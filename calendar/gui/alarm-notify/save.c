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

#include <bonobo/bonobo-arg.h>
#include <bonobo/bonobo-exception.h>
#include <bonobo/bonobo-moniker-util.h>
#include "evolution-calendar.h"
#include "save.h"



/* Key names for the configuration values */

#define KEY_LAST_NOTIFICATION_TIME "/Calendar/AlarmNotify/LastNotificationTime"
#define KEY_NUM_CALENDARS_TO_LOAD "/Calendar/AlarmNotify/NumCalendarsToLoad"
#define BASE_KEY_CALENDAR_TO_LOAD "/Calendar/AlarmNotify/CalendarToLoad"
#define KEY_NUM_BLESSED_PROGRAMS "/Calendar/AlarmNotify/NumBlessedPrograms"
#define BASE_KEY_BLESSED_PROGRAM "/Calendar/AlarmNotify/BlessedProgram"



/* Tries to get the config database object; returns CORBA_OBJECT_NIL on failure. */
Bonobo_ConfigDatabase
get_config_db (void)
{
	CORBA_Environment ev;
	Bonobo_ConfigDatabase db;

	CORBA_exception_init (&ev);

	db = bonobo_get_object ("wombat:", "Bonobo/ConfigDatabase", &ev);
	if (BONOBO_EX (&ev) || db == CORBA_OBJECT_NIL) {
		g_message ("get_config_db(): Could not get the config database object");
		db = CORBA_OBJECT_NIL;
	}

	CORBA_exception_free (&ev);
	return db;
}

/* Syncs a database and unrefs it */
void
discard_config_db (Bonobo_ConfigDatabase db)
{
	CORBA_Environment ev;

	CORBA_exception_init (&ev);

	Bonobo_ConfigDatabase_sync (db, &ev);
	if (BONOBO_EX (&ev))
		g_message ("discard_config_db(): Got an exception during the sync command");

	CORBA_exception_free (&ev);

	CORBA_exception_init (&ev);

	bonobo_object_release_unref (db, &ev);
	if (BONOBO_EX (&ev))
		g_message ("discard_config_db(): Could not release/unref the database");

	CORBA_exception_free (&ev);
}

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
	Bonobo_ConfigDatabase db;
	CORBA_Environment ev;

	g_return_if_fail (t != -1);

	db = get_config_db ();
	if (db == CORBA_OBJECT_NIL)
		return;

	CORBA_exception_init (&ev);

	bonobo_config_set_long (db, KEY_LAST_NOTIFICATION_TIME, (long) t, &ev);
	if (BONOBO_EX (&ev))
		g_message ("save_notification_time(): Could not save the value");

	CORBA_exception_free (&ev);

	discard_config_db (db);
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
	Bonobo_ConfigDatabase db;
	long t;

	db = get_config_db ();
	if (db == CORBA_OBJECT_NIL)
		return -1;

	t = bonobo_config_get_long_with_default (db, KEY_LAST_NOTIFICATION_TIME, -1, NULL);

	discard_config_db (db);

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
	Bonobo_ConfigDatabase db;
	CORBA_Environment ev;
	int len, i;

	g_return_if_fail (uris != NULL);

	db = get_config_db ();
	if (db == CORBA_OBJECT_NIL)
		return;

	len = uris->len;

	CORBA_exception_init (&ev);

	bonobo_config_set_long (db, KEY_NUM_CALENDARS_TO_LOAD, len, &ev);
	if (BONOBO_EX (&ev))
		g_warning ("Cannot save config key %s -- %s", KEY_NUM_CALENDARS_TO_LOAD, BONOBO_EX_ID (&ev));

	for (i = 0; i < len; i++) {
		const char *uri;
		char *key;

		uri = uris->pdata[i];

		key = g_strdup_printf ("%s%d", BASE_KEY_CALENDAR_TO_LOAD, i);
		bonobo_config_set_string (db, key, uri, &ev);
		if (BONOBO_EX (&ev))
			g_warning ("Cannot save config key %s -- %s", key, BONOBO_EX_ID (&ev));
		g_free (key);
	}

	CORBA_exception_free (&ev);

	discard_config_db (db);
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
	Bonobo_ConfigDatabase db;
	GPtrArray *uris;
	int len, i;

	db = get_config_db ();
	if (db == CORBA_OBJECT_NIL)
		return NULL;

	/* Getting the default value below is not necessarily an error, as we
	 * may not have saved the list of calendar yet.
	 */

	len = bonobo_config_get_long_with_default (db, KEY_NUM_CALENDARS_TO_LOAD, 0, NULL);

	uris = g_ptr_array_new ();
	g_ptr_array_set_size (uris, len);

	for (i = 0; i < len; i++) {
		char *key;
		gboolean used_default;

		key = g_strdup_printf ("%s%d", BASE_KEY_CALENDAR_TO_LOAD, i);
		uris->pdata[i] = bonobo_config_get_string_with_default (db, key, "", &used_default);
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
	Bonobo_ConfigDatabase db;
	CORBA_Environment ev;
	char *key;
	int len;

	g_return_if_fail (program != NULL);

	db = get_config_db ();
	if (db == CORBA_OBJECT_NIL)
		return;

	/* Up the number saved */
	len = bonobo_config_get_long_with_default (db, KEY_NUM_BLESSED_PROGRAMS, 0, NULL);
	len++;
	
	bonobo_config_set_long (db, KEY_NUM_BLESSED_PROGRAMS, len, &ev);
	if (BONOBO_EX (&ev))
		g_warning ("Cannot save config key %s -- %s", KEY_NUM_BLESSED_PROGRAMS, BONOBO_EX_ID (&ev));

	/* Save the program name */
	key = g_strdup_printf ("%s%d", BASE_KEY_BLESSED_PROGRAM, len - 1);
	bonobo_config_set_string (db, key, program, &ev);
	if (BONOBO_EX (&ev))
		g_warning ("Cannot save config key %s -- %s", key, BONOBO_EX_ID (&ev));
	g_free (key);

	CORBA_exception_free (&ev);

	discard_config_db (db);
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
	Bonobo_ConfigDatabase db;
	int len, i;

	g_return_val_if_fail (program != NULL, FALSE);

	db = get_config_db ();
	if (db == CORBA_OBJECT_NIL)
		return FALSE;

	/* Getting the default value below is not necessarily an error, as we
	 * may not have saved the list of calendar yet.
	 */

	len = bonobo_config_get_long_with_default (db, KEY_NUM_BLESSED_PROGRAMS, 0, NULL);

	for (i = 0; i < len; i++) {
		char *key, *value;
		gboolean used_default;

		key = g_strdup_printf ("%s%d", BASE_KEY_BLESSED_PROGRAM, i);
		value = bonobo_config_get_string_with_default (db, key, "", &used_default);
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
