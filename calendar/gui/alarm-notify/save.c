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

#include <bonobo/bonobo-exception.h>
#include <bonobo/bonobo-moniker-util.h>
#include <bonobo-conf/bonobo-config-database.h>
#include "save.h"



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

	bonobo_config_set_long (db, "/Calendar/AlarmNotify/LastNotificationTime", (long) t, &ev);
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

	t = bonobo_config_get_long_with_default (db, "/Calendar/AlarmNotify/LastNotificationTime",
						 -1, NULL);

	discard_config_db (db);

	return (time_t) t;
}
