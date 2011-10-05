/*
 * Evolution calendar - Configuration values for the alarm notification daemon
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) version 3.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with the program; if not, see <http://www.gnu.org/licenses/>
 *
 *
 * Authors:
 *		Federico Mena-Quintero <federico@ximian.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <string.h>
#include <libedataserver/e-source-list.h>
#include "config-data.h"

/* Whether we have initied ourselves by reading
 * the data from the configuration engine. */
static gboolean inited = FALSE;
static GConfClient *conf_client = NULL;
static GSetting *calendar_settings = NULL;
static ESourceList *calendar_source_list = NULL, *tasks_source_list = NULL;

/* Copied from ../calendar-config.c; returns whether the locale has 'am' and
 * 'pm' strings defined.
 */
static gboolean
locale_supports_12_hour_format (void)
{
	gchar s[16];
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

	g_object_unref (calendar_settings);
	calendar_settings = FALSE;

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

	calendar_settings = g_settings_new ("org.gnome.evolution.calendar");

	g_atexit ((GVoidFunc) do_cleanup);

	/* load the sources for calendars and tasks */
	calendar_source_list = e_source_list_new_for_gconf (conf_client,
							    "/apps/evolution/calendar/sources");
	tasks_source_list = e_source_list_new_for_gconf (conf_client,
							 "/apps/evolution/tasks/sources");

}

ESourceList *
config_data_get_calendars (const gchar *key)
{
	ESourceList *cal_sources;
	gboolean state;
	GSList *gconf_list;

	if (!inited) {
		conf_client = gconf_client_get_default ();
		calendar_settings = g_settings_new ("org.gnome.evolution.calendar");
	}

	gconf_list = gconf_client_get_list (conf_client,
					    key,
					    GCONF_VALUE_STRING,
					    NULL);
	cal_sources = e_source_list_new_for_gconf (conf_client, key);

	if (cal_sources && g_slist_length (gconf_list)) {
		g_slist_foreach (gconf_list, (GFunc) g_free, NULL);
		g_slist_free (gconf_list);
		return cal_sources;
	}

	state = g_settings_get_boolean (calendar_settings, "notify-with-tray", NULL);
	if (!state) /* Should be old client */ {
		GSList *source;

		g_settings_set_boolean (calendar_settings, "notify-with-tray", TRUE, NULL);
		source = gconf_client_get_list (conf_client,
						"/apps/evolution/calendar/sources",
						GCONF_VALUE_STRING,
						NULL);
		gconf_client_set_list (conf_client,
				       key,
				       GCONF_VALUE_STRING,
				       source,
				       NULL);
		cal_sources = e_source_list_new_for_gconf (conf_client, key);

		if (source) {
			g_slist_foreach (source, (GFunc) g_free, NULL);
			g_slist_free (source);
		}
	}

	if (gconf_list) {
		g_slist_foreach (gconf_list, (GFunc) g_free, NULL);
		g_slist_free (gconf_list);
	}

	return cal_sources;

}

void
config_data_replace_string_list (const gchar *key,
                                 const gchar *old,
                                 const gchar *new)
{
	GSList *source, *tmp;

	if (!inited)
		conf_client = gconf_client_get_default ();

	source = gconf_client_get_list (conf_client,
					key,
					GCONF_VALUE_STRING,
					NULL);

	for (tmp = source; tmp; tmp = tmp->next) {

		if (strcmp (tmp->data, old) == 0) {
			g_free (tmp->data);
			tmp->data = g_strdup ((gchar *) new);
			gconf_client_set_list (conf_client,
					       key,
					       GCONF_VALUE_STRING,
					       source,
					       NULL);
			break;
		}
	}

	if (source) {
		g_slist_foreach (source, (GFunc) g_free, NULL);
		g_slist_free (source);
	}
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
	gchar *location;
	icaltimezone *local_timezone;

	ensure_inited ();

	if (g_settings_get_boolean (calendar_settings, "use-system-timezone"))
		location = e_cal_util_get_system_timezone_location ();
	else {
		location = g_settings_get_string (calendar_settings, "timezone");
	}

	if (location && location[0])
		local_timezone = icaltimezone_get_builtin_timezone (location);
	else
		local_timezone = icaltimezone_get_utc_timezone ();

	g_free (location);

	return local_timezone;
}

gboolean
config_data_get_24_hour_format (void)
{
	ensure_inited ();

	if (locale_supports_12_hour_format ()) {
		return g_settings_get_boolean (calendar_client, "use-24hour-format");
	}

	return TRUE;
}

gboolean
config_data_get_notify_with_tray (void)
{
	ensure_inited ();

	return g_settings_get_boolean (calendar_client, "notify-with-tray");
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
config_data_set_last_notification_time (ECalClient *cal,
                                        time_t t)
{
	time_t current_t, now = time (NULL);

	g_return_if_fail (t != -1);

	if (cal) {
		ESource *source = e_client_get_source (E_CLIENT (cal));
		if (source) {
			const gchar *prop_str;
			GTimeVal curr_tv = {0};

			prop_str = e_source_get_property (source, "last-notified");
			if (!prop_str || !g_time_val_from_iso8601 (prop_str, &curr_tv))
				curr_tv.tv_sec = 0;

			if (t > (time_t) curr_tv.tv_sec || (time_t) curr_tv.tv_sec > now) {
				GTimeVal tmval = {0};
				gchar *as_text;

				tmval.tv_sec = (glong) t;
				as_text = g_time_val_to_iso8601 (&tmval);

				if (as_text) {
					e_source_set_property (source, "last-notified", as_text);
					g_free (as_text);
					/* pass through, thus the global last notification time is also changed */
				}
			}
		}
	}

	/* we only store the new notification time if it is bigger
	 * than the already stored one */
	current_t = g_settings_get_int (calendar_settings, "last-notification-time");
	if (t > current_t || current_t > now)
		g_settings_set_int (calendar_settings "last-notification-time", t);
}

/**
 * config_data_get_last_notification_time:
 *
 * Queries the last saved value for alarm notification times.
 *
 * Return value: The last saved value, or -1 if no value had been saved before.
 **/
time_t
config_data_get_last_notification_time (ECalClient *cal)
{
	time_t value, now;

	if (cal) {
		ESource *source = e_client_get_source (E_CLIENT (cal));
		if (source) {
			const gchar *last_notified;

			GTimeVal tmval = {0};

			last_notified = e_source_get_property (
				source, "last-notified");

			if (last_notified && *last_notified &&
				g_time_val_from_iso8601 (last_notified, &tmval)) {
				time_t now = time (NULL), val = (time_t) tmval.tv_sec;

				if (val > now)
					val = now;
				return val;
			}
		}
	}

	value = g_settings_get_int (calendar_settings, "last-notification-time");
	now = time (NULL);
	if (val > now)
		val = now;

	return val;
}

/**
 * config_data_save_blessed_program:
 * @program: a program name
 *
 * Saves a program name as "blessed"
 **/
void
config_data_save_blessed_program (const gchar *program)
{
	gchar **list;
	gint i;
	GArray *array = g_array_new (TRUE, FALSE, sizeof (gchar *));

	list = g_settings_get_strv (calendar_settings, "notify-programs");
	for (i = 0; i < g_strv_length (list); i++)
		g_array_append_val (array, list[i]);

	g_array_append_val (array, program);
	g_settings_set_strv (calendar_settings, "notify-programs", (const gchar *const *) array->data);

	g_strfreev (list);
	g_array_free (array, TRUE);
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
config_data_is_blessed_program (const gchar *program)
{
	gchar **list;
	gint i = 0;
	gboolean found = FALSE;

	list = g_settings_get_strv (calendar_settings, "notify-programs");
	if (!list)
		return FALSE;

	while (list[i] != NULL) {
		if (!found)
			found = strcmp ((gchar *) list[i], program) == 0;
		i++;
	}

	g_strfreev (list);

	return found;
}

static gboolean can_debug = FALSE;
static GStaticRecMutex rec_mutex = G_STATIC_REC_MUTEX_INIT;

void
config_data_init_debugging (void)
{
	can_debug = g_getenv ("ALARMS_DEBUG") != NULL;
}

/* returns whether started debugging;
 * call config_data_stop_debugging() when started and you are done with it
 */
gboolean
config_data_start_debugging (void)
{
	g_static_rec_mutex_lock (&rec_mutex);

	if (can_debug)
		return TRUE;

	g_static_rec_mutex_unlock (&rec_mutex);

	return FALSE;
}

void
config_data_stop_debugging (void)
{
	g_static_rec_mutex_unlock (&rec_mutex);
}
