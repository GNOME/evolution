/* Evolution calendar - Functions to save alarm notification times
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

#include <bonobo/bonobo-arg.h>
#include <bonobo/bonobo-exception.h>
#include <bonobo/bonobo-moniker-util.h>
#include <bonobo-conf/bonobo-config-database.h>
#include "evolution-calendar.h"
#include "save.h"



/* Key names for the configuration values */

#define KEY_LAST_NOTIFICATION_TIME "/Calendar/AlarmNotify/LastNotificationTime"
#define KEY_CALENDARS_TO_LOAD "/Calendar/AlarmNotify/CalendarsToLoad"



/* Tries to get the config database object; returns CORBA_OBJECT_NIL on failure. */
static Bonobo_ConfigDatabase
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
static void
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
	int len, i;
	GNOME_Evolution_Calendar_StringSeq *seq;
	CORBA_Environment ev;
	CORBA_any *any; 

	g_return_if_fail (uris != NULL);

	db = get_config_db ();
	if (db == CORBA_OBJECT_NIL)
		return;

	/* Build the sequence of URIs */

	len = uris->len;

	seq = GNOME_Evolution_Calendar_StringSeq__alloc ();
	seq->_length = len;
	seq->_buffer = CORBA_sequence_CORBA_string_allocbuf (len);
	CORBA_sequence_set_release (seq, TRUE);

	for (i = 0; i < len; i++) {
		char *uri;

		uri = uris->pdata[i];
		seq->_buffer[i] = CORBA_string_dup (uri);
	}

	/* Save it */

	any = bonobo_arg_new (TC_GNOME_Evolution_Calendar_StringSeq);
	any->_value = seq;

	CORBA_exception_init (&ev);

	bonobo_config_set_value (db, KEY_CALENDARS_TO_LOAD, any, &ev);

	if (ev._major != CORBA_NO_EXCEPTION)
		g_message ("save_calendars_to_load(): Could not save the list of calendars");

	CORBA_exception_free (&ev);

	discard_config_db (db);
	bonobo_arg_release (any); /* this releases the sequence */
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
	GNOME_Evolution_Calendar_StringSeq *seq;
	CORBA_Environment ev;
	CORBA_any *any; 
	int len, i;
	GPtrArray *uris;

	db = get_config_db ();
	if (db == CORBA_OBJECT_NIL)
		return NULL;

	/* Get the value */

	CORBA_exception_init (&ev);

	any = bonobo_config_get_value (db, KEY_CALENDARS_TO_LOAD,
				       TC_GNOME_Evolution_Calendar_StringSeq,
				       &ev);
	discard_config_db (db);

	if (ev._major == CORBA_USER_EXCEPTION) {
		char *ex_id;

		ex_id = CORBA_exception_id (&ev);

		if (strcmp (ex_id, ex_Bonobo_ConfigDatabase_NotFound) == 0) {
			CORBA_exception_free (&ev);
			uris = g_ptr_array_new ();
			g_ptr_array_set_size (uris, 0);
			return uris;
		}
	}

	if (ev._major != CORBA_NO_EXCEPTION) {
		g_message ("get_calendars_to_load(): Could not get the list of calendars");
		CORBA_exception_free (&ev);
		return NULL;
	}

	CORBA_exception_free (&ev);

	/* Decode the value */

	seq = any->_value;
	len = seq->_length;

	uris = g_ptr_array_new ();
	g_ptr_array_set_size (uris, len);

	for (i = 0; i < len; i++)
		uris->pdata[i] = g_strdup (seq->_buffer[i]);

#if 0
	/* FIXME: The any and sequence are leaked.  If we release them this way,
	 * we crash inside the ORB freeing routines :(
	 */
	bonobo_arg_release (any);
#endif

	return uris;
}
