/*
 * Evolution calendar - Configuration values for the alarm notification daemon
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
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
#include <e-util/e-util.h>
#include "config-data.h"

/* Whether we have initied ourselves by reading
 * the data from the configuration engine. */
static GSettings *calendar_settings = NULL;

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

void
config_data_cleanup (void)
{
	if (calendar_settings)
		g_object_unref (calendar_settings);
	calendar_settings = NULL;
}

/* Ensures that the configuration values have been read */
static void
ensure_inited (void)
{
	if (calendar_settings)
		return;

	calendar_settings = e_util_ref_settings ("org.gnome.evolution.calendar");
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
		return g_settings_get_boolean (calendar_settings, "use-24hour-format");
	}

	return TRUE;
}

gboolean
config_data_get_notify_with_tray (void)
{
	ensure_inited ();

	return g_settings_get_boolean (calendar_settings, "notify-with-tray");
}

gint
config_data_get_default_snooze_minutes (void)
{
	ensure_inited ();

	return g_settings_get_int (calendar_settings, "default-snooze-minutes");
}

static void
source_written_cb (GObject *source_object,
                   GAsyncResult *result,
                   gpointer user_data)
{
	GError *error = NULL;

	e_source_write_finish (E_SOURCE (source_object), result, &error);

	if (error != NULL) {
		g_warning (
			"Failed to write source changes: %s",
			error->message);
		g_error_free (error);
	}
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

	if (cal != NULL) {
		ESource *source;
		ESourceAlarms *extension;
		GTimeVal tv = {0};
		const gchar *extension_name;
		gchar *iso8601;

		source = e_client_get_source (E_CLIENT (cal));
		extension_name = E_SOURCE_EXTENSION_ALARMS;
		extension = e_source_get_extension (source, extension_name);

		iso8601 = (gchar *) e_source_alarms_get_last_notified (extension);
		if (iso8601 != NULL)
			g_time_val_from_iso8601 (iso8601, &tv);

		if (t > (time_t) tv.tv_sec || (time_t) tv.tv_sec > now) {
			tv.tv_sec = (glong) t;
			iso8601 = g_time_val_to_iso8601 (&tv);
			e_source_alarms_set_last_notified (extension, iso8601);
			g_free (iso8601);

			e_source_write (source, NULL, source_written_cb, NULL);
		}
	}

	/* we only store the new notification time if it is bigger
	 * than the already stored one */
	current_t = g_settings_get_int (calendar_settings, "last-notification-time");
	if (t > current_t || current_t > now)
		g_settings_set_int (calendar_settings, "last-notification-time", t);
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

	if (cal != NULL) {
		ESource *source;
		ESourceAlarms *extension;
		GTimeVal tmval = {0};
		const gchar *extension_name;
		const gchar *last_notified;
		time_t now, val;

		source = e_client_get_source (E_CLIENT (cal));
		extension_name = E_SOURCE_EXTENSION_ALARMS;

		if (!e_source_has_extension (source, extension_name))
			goto skip;

		extension = e_source_get_extension (source, extension_name);
		last_notified = e_source_alarms_get_last_notified (extension);

		if (last_notified == NULL || *last_notified == '\0')
			goto skip;

		if (!g_time_val_from_iso8601 (last_notified, &tmval))
			goto skip;

		now = time (NULL);
		val = (time_t) tmval.tv_sec;

		if (val > now)
			val = now;

		return val;
	}

skip:
	value = g_settings_get_int (calendar_settings, "last-notification-time");
	now = time (NULL);
	if (value > now)
		value = now;

	return value;
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
	GPtrArray *array = g_ptr_array_new ();

	list = g_settings_get_strv (calendar_settings, "notify-programs");
	for (i = 0; i < g_strv_length (list); i++)
		g_ptr_array_add (array, list[i]);

	g_ptr_array_add (array, (gpointer) program);
	g_ptr_array_add (array, NULL);

	g_settings_set_strv (
		calendar_settings, "notify-programs",
		(const gchar *const *) array->pdata);

	g_strfreev (list);
	g_ptr_array_free (array, TRUE);
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
static GRecMutex rec_mutex;

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
	g_rec_mutex_lock (&rec_mutex);

	if (can_debug)
		return TRUE;

	g_rec_mutex_unlock (&rec_mutex);

	return FALSE;
}

void
config_data_stop_debugging (void)
{
	g_rec_mutex_unlock (&rec_mutex);
}
