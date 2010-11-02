/*
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
 *		Damon Chaplin <damon@ximian.com>
 *		Rodrigo Moya <rodrigo@ximian.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

/*
 * calendar-config.c - functions to load/save/get/set user settings.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <time.h>
#include <string.h>
#include <e-util/e-util.h>
#include <libecal/e-cal-time-util.h>
#include <libedataserver/e-data-server-util.h>
#include <widgets/e-timezone-dialog/e-timezone-dialog.h>
#include <shell/e-shell.h>

#include "calendar-config-keys.h"
#include "calendar-config.h"

static GConfClient *config = NULL;

static void
do_cleanup (void)
{
	g_object_unref (config);
	config = NULL;
}

static void
calendar_config_init (void)
{
	if (config)
		return;

	config = gconf_client_get_default ();
	g_atexit ((GVoidFunc) do_cleanup);

	gconf_client_add_dir (config, CALENDAR_CONFIG_PREFIX, GCONF_CLIENT_PRELOAD_RECURSIVE, NULL);
}

void
calendar_config_remove_notification (guint id)
{
	calendar_config_init ();

	gconf_client_notify_remove (config, id);
}

/* Returns TRUE if the locale has 'am' and 'pm' strings defined, in which
   case the user can choose between 12 and 24-hour time formats. */
gboolean
calendar_config_locale_supports_12_hour_format (void)
{
	gchar s[16];
	time_t t = 0;

	calendar_config_init ();

	e_utf8_strftime (s, sizeof s, "%p", gmtime (&t));
	return s[0] != '\0';
}

/*
 * Calendar Settings.
 */

static gchar *
calendar_config_get_timezone_stored (void)
{
	calendar_config_init ();

	return gconf_client_get_string (config, CALENDAR_CONFIG_TIMEZONE, NULL);
}

static gchar *
calendar_config_get_timezone (void)
{
	EShell *shell;
	EShellSettings *shell_settings;
	gboolean use_system_timezone;

	shell = e_shell_get_default ();
	shell_settings = e_shell_get_shell_settings (shell);

	use_system_timezone = e_shell_settings_get_boolean (
		shell_settings, "cal-use-system-timezone");

	if (use_system_timezone)
		return e_cal_util_get_system_timezone_location ();

	return calendar_config_get_timezone_stored ();
}

icaltimezone *
calendar_config_get_icaltimezone (void)
{
	gchar *location;
	icaltimezone *zone = NULL;

	calendar_config_init ();

	location = calendar_config_get_timezone ();
	if (location) {
		zone = icaltimezone_get_builtin_timezone (location);

		g_free (location);
	}
	return zone;
}

/* Whether we use 24-hour format or 12-hour format (AM/PM). */
gboolean
calendar_config_get_24_hour_format	(void)
{
	calendar_config_init ();

	/* If the locale defines 'am' and 'pm' strings then the user has the
	   choice of 12-hour or 24-hour time format, with 12-hour as the
	   default. If the locale doesn't have 'am' and 'pm' strings we have
	   to use 24-hour format, or strftime ()/strptime () won't work. */
	if (calendar_config_locale_supports_12_hour_format ())
		return gconf_client_get_bool (config, CALENDAR_CONFIG_24HOUR, NULL);

	return TRUE;
}

/* Scroll in a month view by a week, not by a month */
gboolean
calendar_config_get_month_scroll_by_week (void)
{
	calendar_config_init ();

	return gconf_client_get_bool (config, CALENDAR_CONFIG_MONTH_SCROLL_BY_WEEK, NULL);
}

guint
calendar_config_add_notification_month_scroll_by_week (GConfClientNotifyFunc func, gpointer data)
{
	guint id;

	calendar_config_init ();

	id = gconf_client_notify_add (config, CALENDAR_CONFIG_MONTH_SCROLL_BY_WEEK, func, data, NULL, NULL);

	return id;
}

/***************************************/

/* The working days of the week, a bit-wise combination of flags. */
CalWeekdays
calendar_config_get_working_days	(void)
{
	calendar_config_init ();

	return gconf_client_get_int (config, CALENDAR_CONFIG_WORKING_DAYS, NULL);
}

/* Settings to hide completed tasks. */
gboolean
calendar_config_get_hide_completed_tasks	(void)
{
	calendar_config_init ();

	return gconf_client_get_bool (config, CALENDAR_CONFIG_TASKS_HIDE_COMPLETED, NULL);
}

static EDurationType
calendar_config_get_hide_completed_tasks_units	(void)
{
	gchar *units;
	EDurationType cu;

	calendar_config_init ();

	units = gconf_client_get_string (config, CALENDAR_CONFIG_TASKS_HIDE_COMPLETED_UNITS, NULL);

	if (units && !strcmp (units, "minutes"))
		cu = E_DURATION_MINUTES;
	else if (units && !strcmp (units, "hours"))
		cu = E_DURATION_HOURS;
	else
		cu = E_DURATION_DAYS;

	g_free (units);

	return cu;
}

/**
 * calendar_config_get_hide_completed_tasks_sexp:
 *
 * @get_completed: Whether to form subexpression that
 * gets completed or not completed tasks.
 * Returns the subexpression to use to filter out completed tasks according
 * to the config settings. The returned sexp should be freed.
 **/
gchar *
calendar_config_get_hide_completed_tasks_sexp (gboolean get_completed)
{
	gchar *sexp = NULL;

	if (calendar_config_get_hide_completed_tasks ()) {
		EDurationType units;
		gint value;

		units = calendar_config_get_hide_completed_tasks_units ();
		value = gconf_client_get_int (config, CALENDAR_CONFIG_TASKS_HIDE_COMPLETED_VALUE, NULL);

		if (value == 0) {
			/* If the value is 0, we want to hide completed tasks
			   immediately, so we filter out all complete/incomplete tasks.*/
			if (!get_completed)
				sexp = g_strdup ("(not is-completed?)");
			else
				sexp = g_strdup ("(is-completed?)");
		} else {
			gchar *isodate;
			icaltimezone *zone;
			struct icaltimetype tt;
			time_t t;

			/* Get the current time, and subtract the appropriate
			   number of days/hours/minutes. */
			zone = calendar_config_get_icaltimezone ();
			tt = icaltime_current_time_with_zone (zone);

			switch (units) {
			case E_DURATION_DAYS:
				icaltime_adjust (&tt, -value, 0, 0, 0);
				break;
			case E_DURATION_HOURS:
				icaltime_adjust (&tt, 0, -value, 0, 0);
				break;
			case E_DURATION_MINUTES:
				icaltime_adjust (&tt, 0, 0, -value, 0);
				break;
			default:
				g_return_val_if_reached (NULL);
			}

			t = icaltime_as_timet_with_zone (tt, zone);

			/* Convert the time to an ISO date string, and build
			   the query sub-expression. */
			isodate = isodate_from_time_t (t);
			if (!get_completed)
				sexp = g_strdup_printf ("(not (completed-before? (make-time \"%s\")))", isodate);
			else
				sexp = g_strdup_printf ("(completed-before? (make-time \"%s\"))", isodate);
			g_free (isodate);
		}
	}

	return sexp;
}

void
calendar_config_set_dir_path (const gchar *path)
{
	calendar_config_init ();

	gconf_client_set_string (config, CALENDAR_CONFIG_SAVE_DIR, path, NULL);
}

gchar *
calendar_config_get_dir_path (void)
{
	gchar *path;

	calendar_config_init ();

	path = gconf_client_get_string (config, CALENDAR_CONFIG_SAVE_DIR, NULL);

	return path;
}

/* contains list of strings, locations, recently used as the second timezone in a day view.
   Free with calendar_config_free_day_second_zones. */
GSList *
calendar_config_get_day_second_zones (void)
{
	GSList *res;

	calendar_config_init ();

	res = gconf_client_get_list (config, CALENDAR_CONFIG_DAY_SECOND_ZONES_LIST, GCONF_VALUE_STRING, NULL);

	return res;
}

/* frees list from calendar_config_get_day_second_zones */
void
calendar_config_free_day_second_zones (GSList *zones)
{
	if (zones) {
		g_slist_foreach (zones, (GFunc)g_free, NULL);
		g_slist_free (zones);
	}
}

/* keeps max 'day_second_zones_max' zones, if 'location' is already in a list, then it'll became first there */
void
calendar_config_set_day_second_zone (const gchar *location)
{
	calendar_config_init ();

	if (location && *location) {
		GSList *lst, *l;
		GError *error = NULL;
		gint max_zones;

		/* configurable max number of timezones to remember */
		max_zones = gconf_client_get_int (config, CALENDAR_CONFIG_DAY_SECOND_ZONES_MAX, &error);

		if (error) {
			g_error_free (error);
			max_zones = -1;
		}

		if (max_zones <= 0)
			max_zones = 5;

		lst = calendar_config_get_day_second_zones ();
		for (l = lst; l; l = l->next) {
			if (l->data && g_str_equal (l->data, location)) {
				if (l != lst) {
					/* isn't first in the list */
					gchar *val = l->data;

					lst = g_slist_remove (lst, val);
					lst = g_slist_prepend (lst, val);
				}
				break;
			}
		}

		if (!l) {
			/* not in the list yet */
			lst = g_slist_prepend (lst, g_strdup (location));
		}

		while (g_slist_length (lst) > max_zones) {
			l = g_slist_last (lst);
			g_free (l->data);
			lst = g_slist_delete_link (lst, l);
		}

		gconf_client_set_list (config, CALENDAR_CONFIG_DAY_SECOND_ZONES_LIST, GCONF_VALUE_STRING, lst, NULL);

		calendar_config_free_day_second_zones (lst);
	}

	gconf_client_set_string (config, CALENDAR_CONFIG_DAY_SECOND_ZONE, location ? location : "", NULL);
}

/* location of the second time zone user has selected. Free with g_free. */
gchar *
calendar_config_get_day_second_zone (void)
{
	calendar_config_init ();

	return gconf_client_get_string (config, CALENDAR_CONFIG_DAY_SECOND_ZONE, NULL);
}

void
calendar_config_select_day_second_zone (void)
{
	icaltimezone *zone = NULL;
	ETimezoneDialog *tzdlg;
	GtkWidget *dialog;
	gchar *second_location;

	second_location = calendar_config_get_day_second_zone ();
	if (second_location && *second_location)
		zone = icaltimezone_get_builtin_timezone (second_location);
	g_free (second_location);

	if (!zone)
		zone = calendar_config_get_icaltimezone ();

	tzdlg = e_timezone_dialog_new ();
	e_timezone_dialog_set_timezone (tzdlg, zone);

	dialog = e_timezone_dialog_get_toplevel (tzdlg);

	if (gtk_dialog_run (GTK_DIALOG (dialog)) == GTK_RESPONSE_ACCEPT) {
		const gchar *location = NULL;

		zone = e_timezone_dialog_get_timezone (tzdlg);
		if (zone == icaltimezone_get_utc_timezone ()) {
			location = "UTC";
		} else if (zone) {
			location = icaltimezone_get_location (zone);
		}

		calendar_config_set_day_second_zone (location);
	}

	g_object_unref (tzdlg);
}

guint
calendar_config_add_notification_day_second_zone (GConfClientNotifyFunc func, gpointer data)
{
	guint id;

	calendar_config_init ();

	id = gconf_client_notify_add (config, CALENDAR_CONFIG_DAY_SECOND_ZONE, func, data, NULL, NULL);

	return id;
}
