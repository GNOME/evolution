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
#include <gconf/gconf-client.h>



/* Key names for the configuration values */

#define KEY_LAST_NOTIFICATION_TIME "/apps/evolution/calendar/notify/last_notification_time"
#define KEY_CALENDARS "/apps/evolution/calendar/notify/calendars"
#define KEY_PROGRAMS "/apps/evolution/calendar/notify/programs"



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
	int i;
	GConfClient *gconf = gconf_client_get_default();
	GSList *l = NULL;

	g_return_if_fail (uris != NULL);

	for (i=0;i<uris->len;i++)
		l = g_slist_append(l, uris->pdata[i]);

	gconf_client_set_list(gconf, KEY_CALENDARS, GCONF_VALUE_STRING, l, NULL);

	g_slist_free(l);

	g_object_unref (gconf);
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
	GConfClient *gconf = gconf_client_get_default();
	GSList *l, *n;
	GPtrArray *uris;

	/* Getting the default value below is not necessarily an error, as we
	 * may not have saved the list of calendar yet.
	 */

	l = gconf_client_get_list (gconf, KEY_CALENDARS, GCONF_VALUE_STRING, NULL);
	uris = g_ptr_array_new ();
	while (l) {
		n = l->next;
		g_ptr_array_add (uris, l->data);
		g_slist_free_1(l);
		l = n;
	}

	g_object_unref (gconf);
	
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
	GConfClient *gconf = gconf_client_get_default();
	GSList *l;

	l = gconf_client_get_list(gconf, KEY_PROGRAMS, GCONF_VALUE_STRING, NULL);
	l = g_slist_append(l, g_strdup(program));
	gconf_client_set_list(gconf, KEY_PROGRAMS, GCONF_VALUE_STRING, l, NULL);
	g_slist_foreach(l, (GFunc)g_free, NULL);
	g_slist_free(l);
	g_object_unref (gconf);
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
	GConfClient *gconf = gconf_client_get_default();
	GSList *l, *n;
	gboolean found = FALSE;

	l = gconf_client_get_list(gconf, KEY_PROGRAMS, GCONF_VALUE_STRING, NULL);
	while (l) {
		n = l->next;
		if (!found)
			found = strcmp((char *)l->data, program) == 0;
		g_free(l->data);
		g_slist_free_1(l);
		l = n;
	}

	g_object_unref (gconf);
	
	return found;
}
