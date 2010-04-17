/*
 * e-cal-shell-settings.c
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
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#include "e-cal-shell-settings.h"

#include <gconf/gconf-client.h>
#include <libecal/e-cal-util.h>

#include "e-util/e-binding.h"

static gboolean
transform_string_to_icaltimezone (const GValue *src_value,
                                  GValue *dst_value,
                                  gpointer user_data)
{
	EShellSettings *shell_settings;
	gboolean use_system_timezone;
	const gchar *location = NULL;
	icaltimezone *timezone = NULL;

	shell_settings = E_SHELL_SETTINGS (user_data);

	use_system_timezone = e_shell_settings_get_boolean (
		shell_settings, "cal-use-system-timezone");

	if (use_system_timezone)
		timezone = e_cal_util_get_system_timezone ();
	else
		location = g_value_get_string (src_value);

	if (location != NULL && *location != '\0')
		timezone = icaltimezone_get_builtin_timezone (location);

	if (timezone == NULL)
		timezone = icaltimezone_get_utc_timezone ();

	g_value_set_pointer (dst_value, timezone);

	return TRUE;
}

static gboolean
transform_icaltimezone_to_string (const GValue *src_value,
                                  GValue *dst_value,
                                  gpointer user_data)
{
	const gchar *location = NULL;
	icaltimezone *timezone;

	timezone = g_value_get_pointer (src_value);

	if (timezone != NULL)
		location = icaltimezone_get_location (timezone);

	if (location == NULL)
		location = "UTC";

	g_value_set_string (dst_value, location);

	return TRUE;
}

static gboolean
transform_weekdays_gconf_to_evolution (const GValue *src_value,
                                       GValue *dst_value,
                                       gpointer user_data)
{
	GDateWeekday weekday;

	/* XXX At some point, Evolution changed its weekday numbering
	 *     from 0 = Sunday to 0 = Monday, but did not migrate the
	 *     "week_start_day" key.  Both enumerations are of course
	 *     different from GDateWeekday.  We should have saved the
	 *     weekday as a string instead. */

	/* This is purposefully verbose for better readability. */

	/* GConf numbering */
	switch (g_value_get_int (src_value)) {
		case 0:
			weekday = G_DATE_SUNDAY;
			break;
		case 1:
			weekday = G_DATE_MONDAY;
			break;
		case 2:
			weekday = G_DATE_TUESDAY;
			break;
		case 3:
			weekday = G_DATE_WEDNESDAY;
			break;
		case 4:
			weekday = G_DATE_THURSDAY;
			break;
		case 5:
			weekday = G_DATE_FRIDAY;
			break;
		case 6:
			weekday = G_DATE_SATURDAY;
			break;
		default:
			return FALSE;
	}

	/* Evolution numbering */
	switch (weekday) {
		case G_DATE_MONDAY:
			g_value_set_int (dst_value, 0);
			break;
		case G_DATE_TUESDAY:
			g_value_set_int (dst_value, 1);
			break;
		case G_DATE_WEDNESDAY:
			g_value_set_int (dst_value, 2);
			break;
		case G_DATE_THURSDAY:
			g_value_set_int (dst_value, 3);
			break;
		case G_DATE_FRIDAY:
			g_value_set_int (dst_value, 4);
			break;
		case G_DATE_SATURDAY:
			g_value_set_int (dst_value, 5);
			break;
		case G_DATE_SUNDAY:
			g_value_set_int (dst_value, 6);
			break;
		default:
			return FALSE;
	}

	return TRUE;
}

static gboolean
transform_weekdays_evolution_to_gconf (const GValue *src_value,
                                       GValue *dst_value,
                                       gpointer user_data)
{
	GDateWeekday weekday;

	/* XXX At some point, Evolution changed its weekday numbering
	 *     from 0 = Sunday to 0 = Monday, but did not migrate the
	 *     "week_start_day" key.  Both enumerations are of course
	 *     different from GDateWeekday.  We should have saved the
	 *     weekday as a string instead. */

	/* This is purposefully verbose for better readability. */

	/* GConf numbering */
	switch (g_value_get_int (src_value)) {
		case 0:
			weekday = G_DATE_MONDAY;
			break;
		case 1:
			weekday = G_DATE_TUESDAY;
			break;
		case 2:
			weekday = G_DATE_WEDNESDAY;
			break;
		case 3:
			weekday = G_DATE_THURSDAY;
			break;
		case 4:
			weekday = G_DATE_FRIDAY;
			break;
		case 5:
			weekday = G_DATE_SATURDAY;
			break;
		case 6:
			weekday = G_DATE_SUNDAY;
			break;
		default:
			return FALSE;
	}

	/* Evolution numbering */
	switch (weekday) {
		case G_DATE_MONDAY:
			g_value_set_int (dst_value, 1);
			break;
		case G_DATE_TUESDAY:
			g_value_set_int (dst_value, 2);
			break;
		case G_DATE_WEDNESDAY:
			g_value_set_int (dst_value, 3);
			break;
		case G_DATE_THURSDAY:
			g_value_set_int (dst_value, 4);
			break;
		case G_DATE_FRIDAY:
			g_value_set_int (dst_value, 5);
			break;
		case G_DATE_SATURDAY:
			g_value_set_int (dst_value, 6);
			break;
		case G_DATE_SUNDAY:
			g_value_set_int (dst_value, 0);
			break;
		default:
			return FALSE;
	}

	return TRUE;
}

/* Working day flags */
enum {
	WORKING_DAY_SUNDAY	= 1 << 0,
	WORKING_DAY_MONDAY	= 1 << 1,
	WORKING_DAY_TUESDAY	= 1 << 2,
	WORKING_DAY_WEDNESDAY	= 1 << 3,
	WORKING_DAY_THURSDAY	= 1 << 4,
	WORKING_DAY_FRIDAY	= 1 << 5,
	WORKING_DAY_SATURDAY	= 1 << 6
};

static gboolean
transform_working_days_bitset_to_sunday (const GValue *src_value,
                                         GValue *dst_value,
                                         gpointer user_data)
{
	gint bitset;
	gboolean working_day;

	bitset = g_value_get_int (src_value);
	working_day = ((bitset & WORKING_DAY_SUNDAY) != 0);
	g_value_set_boolean (dst_value, working_day);

	return TRUE;
}

static gboolean
transform_working_days_sunday_to_bitset (const GValue *src_value,
                                         GValue *dst_value,
                                         gpointer user_data)
{
	EShellSettings *shell_settings;
	gint bitset, bit;

	shell_settings = E_SHELL_SETTINGS (user_data);

	bitset = e_shell_settings_get_int (
		shell_settings, "cal-working-days-bitset");

	bit = g_value_get_boolean (src_value) ? WORKING_DAY_SUNDAY : 0;
	g_value_set_int (dst_value, (bitset & ~WORKING_DAY_SUNDAY) | bit);

	return TRUE;
}

static gboolean
transform_working_days_bitset_to_monday (const GValue *src_value,
                                         GValue *dst_value,
                                         gpointer user_data)
{
	gint bitset;
	gboolean working_day;

	bitset = g_value_get_int (src_value);
	working_day = ((bitset & WORKING_DAY_MONDAY) != 0);
	g_value_set_boolean (dst_value, working_day);

	return TRUE;
}

static gboolean
transform_working_days_monday_to_bitset (const GValue *src_value,
                                         GValue *dst_value,
                                         gpointer user_data)
{
	EShellSettings *shell_settings;
	gint bitset, bit;

	shell_settings = E_SHELL_SETTINGS (user_data);

	bitset = e_shell_settings_get_int (
		shell_settings, "cal-working-days-bitset");

	bit = g_value_get_boolean (src_value) ? WORKING_DAY_MONDAY : 0;
	g_value_set_int (dst_value, (bitset & ~WORKING_DAY_MONDAY) | bit);

	return TRUE;
}

static gboolean
transform_working_days_bitset_to_tuesday (const GValue *src_value,
                                          GValue *dst_value,
                                          gpointer user_data)
{
	gint bitset;
	gboolean working_day;

	bitset = g_value_get_int (src_value);
	working_day = ((bitset & WORKING_DAY_TUESDAY) != 0);
	g_value_set_boolean (dst_value, working_day);

	return TRUE;
}

static gboolean
transform_working_days_tuesday_to_bitset (const GValue *src_value,
                                          GValue *dst_value,
                                          gpointer user_data)
{
	EShellSettings *shell_settings;
	gint bitset, bit;

	shell_settings = E_SHELL_SETTINGS (user_data);

	bitset = e_shell_settings_get_int (
		shell_settings, "cal-working-days-bitset");

	bit = g_value_get_boolean (src_value) ? WORKING_DAY_TUESDAY : 0;
	g_value_set_int (dst_value, (bitset & ~WORKING_DAY_TUESDAY) | bit);

	return TRUE;
}

static gboolean
transform_working_days_bitset_to_wednesday (const GValue *src_value,
                                            GValue *dst_value,
                                            gpointer user_data)
{
	gint bitset;
	gboolean working_day;

	bitset = g_value_get_int (src_value);
	working_day = ((bitset & WORKING_DAY_WEDNESDAY) != 0);
	g_value_set_boolean (dst_value, working_day);

	return TRUE;
}

static gboolean
transform_working_days_wednesday_to_bitset (const GValue *src_value,
                                            GValue *dst_value,
                                            gpointer user_data)
{
	EShellSettings *shell_settings;
	gint bitset, bit;

	shell_settings = E_SHELL_SETTINGS (user_data);

	bitset = e_shell_settings_get_int (
		shell_settings, "cal-working-days-bitset");

	bit = g_value_get_boolean (src_value) ? WORKING_DAY_WEDNESDAY : 0;
	g_value_set_int (dst_value, (bitset & ~WORKING_DAY_WEDNESDAY) | bit);

	return TRUE;
}

static gboolean
transform_working_days_bitset_to_thursday (const GValue *src_value,
                                           GValue *dst_value,
                                           gpointer user_data)
{
	gint bitset;
	gboolean working_day;

	bitset = g_value_get_int (src_value);
	working_day = ((bitset & WORKING_DAY_THURSDAY) != 0);
	g_value_set_boolean (dst_value, working_day);

	return TRUE;
}

static gboolean
transform_working_days_thursday_to_bitset (const GValue *src_value,
                                           GValue *dst_value,
                                           gpointer user_data)
{
	EShellSettings *shell_settings;
	gint bitset, bit;

	shell_settings = E_SHELL_SETTINGS (user_data);

	bitset = e_shell_settings_get_int (
		shell_settings, "cal-working-days-bitset");

	bit = g_value_get_boolean (src_value) ? WORKING_DAY_THURSDAY : 0;
	g_value_set_int (dst_value, (bitset & ~WORKING_DAY_THURSDAY) | bit);

	return TRUE;
}

static gboolean
transform_working_days_bitset_to_friday (const GValue *src_value,
                                         GValue *dst_value,
                                         gpointer user_data)
{
	gint bitset;
	gboolean working_day;

	bitset = g_value_get_int (src_value);
	working_day = ((bitset & WORKING_DAY_FRIDAY) != 0);
	g_value_set_boolean (dst_value, working_day);

	return TRUE;
}

static gboolean
transform_working_days_friday_to_bitset (const GValue *src_value,
                                         GValue *dst_value,
                                         gpointer user_data)
{
	EShellSettings *shell_settings;
	gint bitset, bit;

	shell_settings = E_SHELL_SETTINGS (user_data);

	bitset = e_shell_settings_get_int (
		shell_settings, "cal-working-days-bitset");

	bit = g_value_get_boolean (src_value) ? WORKING_DAY_FRIDAY : 0;
	g_value_set_int (dst_value, (bitset & ~WORKING_DAY_FRIDAY) | bit);

	return TRUE;
}

static gboolean
transform_working_days_bitset_to_saturday (const GValue *src_value,
                                           GValue *dst_value,
                                           gpointer user_data)
{
	gint bitset;
	gboolean working_day;

	bitset = g_value_get_int (src_value);
	working_day = ((bitset & WORKING_DAY_SATURDAY) != 0);
	g_value_set_boolean (dst_value, working_day);

	return TRUE;
}

static gboolean
transform_working_days_saturday_to_bitset (const GValue *src_value,
                                           GValue *dst_value,
                                           gpointer user_data)
{
	EShellSettings *shell_settings;
	gint bitset, bit;

	shell_settings = E_SHELL_SETTINGS (user_data);

	bitset = e_shell_settings_get_int (
		shell_settings, "cal-working-days-bitset");

	bit = g_value_get_boolean (src_value) ? WORKING_DAY_SATURDAY : 0;
	g_value_set_int (dst_value, (bitset & ~WORKING_DAY_SATURDAY) | bit);

	return TRUE;
}

void
e_cal_shell_backend_init_settings (EShell *shell)
{
	EShellSettings *shell_settings;

	shell_settings = e_shell_get_shell_settings (shell);

	e_shell_settings_install_property_for_key (
		"cal-compress-weekend",
		"/apps/evolution/calendar/display/compress_weekend");

	e_shell_settings_install_property_for_key (
		"cal-confirm-delete",
		"/apps/evolution/calendar/prompts/confirm_delete");

	e_shell_settings_install_property_for_key (
		"cal-confirm-purge",
		"/apps/evolution/calendar/prompts/confirm_purge");

	e_shell_settings_install_property_for_key (
		"cal-show-week-numbers",
		"/apps/evolution/calendar/display/show_week_numbers");

	e_shell_settings_install_property_for_key (
		"cal-free-busy-template",
		"/apps/evolution/calendar/publish/template");

	e_shell_settings_install_property_for_key (
		"cal-hide-completed-tasks",
		"/apps/evolution/calendar/tasks/hide_completed");

	e_shell_settings_install_property_for_key (
		"cal-hide-completed-tasks-units",
		"/apps/evolution/calendar/tasks/hide_completed_units");

	e_shell_settings_install_property_for_key (
		"cal-hide-completed-tasks-value",
		"/apps/evolution/calendar/tasks/hide_completed_value");

	e_shell_settings_install_property_for_key (
		"cal-marcus-bains-day-view-color",
		"/apps/evolution/calendar/display/marcus_bains_color_dayview");

	e_shell_settings_install_property_for_key (
		"cal-marcus-bains-time-bar-color",
		"/apps/evolution/calendar/display/marcus_bains_color_timebar");

	e_shell_settings_install_property_for_key (
		"cal-marcus-bains-show-line",
		"/apps/evolution/calendar/display/marcus_bains_line");

	e_shell_settings_install_property_for_key (
		"cal-primary-calendar",
		"/apps/evolution/calendar/display/primary_calendar");

	e_shell_settings_install_property_for_key (
		"cal-primary-memo-list",
		"/apps/evolution/calendar/memos/primary_memos");

	e_shell_settings_install_property_for_key (
		"cal-primary-task-list",
		"/apps/evolution/calendar/tasks/primary_tasks");

	e_shell_settings_install_property_for_key (
		"cal-show-event-end-times",
		"/apps/evolution/calendar/display/show_event_end");

	e_shell_settings_install_property_for_key (
		"cal-tasks-color-due-today",
		"/apps/evolution/calendar/tasks/colors/due_today");

	e_shell_settings_install_property_for_key (
		"cal-tasks-color-overdue",
		"/apps/evolution/calendar/tasks/colors/overdue");

	e_shell_settings_install_property_for_key (
		"cal-time-divisions",
		"/apps/evolution/calendar/display/time_divisions");

	/* Do not bind to this.  Use "cal-timezone" instead. */
	e_shell_settings_install_property_for_key (
		"cal-timezone-string",
		"/apps/evolution/calendar/display/timezone");

	e_shell_settings_install_property_for_key (
		"cal-use-24-hour-format",
		"/apps/evolution/calendar/display/use_24hour_format");

	e_shell_settings_install_property_for_key (
		"cal-use-system-timezone",
		"/apps/evolution/calendar/display/use_system_timezone");

	/* Do not bind to this.  Use "cal-week-start-day" instead. */
	e_shell_settings_install_property_for_key (
		"cal-week-start-day-gconf",
		"/apps/evolution/calendar/display/week_start_day");

	e_shell_settings_install_property_for_key (
		"cal-work-day-end-hour",
		"/apps/evolution/calendar/display/day_end_hour");

	e_shell_settings_install_property_for_key (
		"cal-work-day-end-minute",
		"/apps/evolution/calendar/display/day_end_minute");

	e_shell_settings_install_property_for_key (
		"cal-work-day-start-hour",
		"/apps/evolution/calendar/display/day_start_hour");

	e_shell_settings_install_property_for_key (
		"cal-work-day-start-minute",
		"/apps/evolution/calendar/display/day_start_minute");

	e_shell_settings_install_property_for_key (
		"cal-working-days-bitset",
		"/apps/evolution/calendar/display/working_days");

	/* These properties use transform functions to convert
	 * GConf values to forms more useful to Evolution.  We
	 * have to use separate properties because GConfBridge
	 * does not support transform functions.  Much of this
	 * is backward-compatibility cruft for poorly designed
	 * GConf schemas. */

	e_shell_settings_install_property (
		g_param_spec_pointer (
			"cal-timezone",
			NULL,
			NULL,
			G_PARAM_READWRITE));

	e_mutual_binding_new_full (
		shell_settings, "cal-timezone-string",
		shell_settings, "cal-timezone",
		transform_string_to_icaltimezone,
		transform_icaltimezone_to_string,
		(GDestroyNotify) g_object_unref,
		g_object_ref (shell_settings));

	e_shell_settings_install_property (
		g_param_spec_int (
			"cal-week-start-day",
			NULL,
			NULL,
			0,  /* Monday */
			6,  /* Sunday */
			0,
			G_PARAM_READWRITE));

	e_mutual_binding_new_full (
		shell_settings, "cal-week-start-day-gconf",
		shell_settings, "cal-week-start-day",
		transform_weekdays_gconf_to_evolution,
		transform_weekdays_evolution_to_gconf,
		(GDestroyNotify) NULL, NULL);

	/* XXX These are my favorite.  Storing a bit array in GConf
	 *     instead of separate boolean keys.  Brilliant move. */

	e_shell_settings_install_property (
		g_param_spec_boolean (
			"cal-working-days-sunday",
			NULL,
			NULL,
			FALSE,
			G_PARAM_READWRITE));

	e_mutual_binding_new_full (
		shell_settings, "cal-working-days-bitset",
		shell_settings, "cal-working-days-sunday",
		transform_working_days_bitset_to_sunday,
		transform_working_days_sunday_to_bitset,
		(GDestroyNotify) NULL, shell_settings);

	e_shell_settings_install_property (
		g_param_spec_boolean (
			"cal-working-days-monday",
			NULL,
			NULL,
			TRUE,
			G_PARAM_READWRITE));

	e_mutual_binding_new_full (
		shell_settings, "cal-working-days-bitset",
		shell_settings, "cal-working-days-monday",
		transform_working_days_bitset_to_monday,
		transform_working_days_monday_to_bitset,
		(GDestroyNotify) NULL, shell_settings);

	e_shell_settings_install_property (
		g_param_spec_boolean (
			"cal-working-days-tuesday",
			NULL,
			NULL,
			TRUE,
			G_PARAM_READWRITE));

	e_mutual_binding_new_full (
		shell_settings, "cal-working-days-bitset",
		shell_settings, "cal-working-days-tuesday",
		transform_working_days_bitset_to_tuesday,
		transform_working_days_tuesday_to_bitset,
		(GDestroyNotify) NULL, shell_settings);

	e_shell_settings_install_property (
		g_param_spec_boolean (
			"cal-working-days-wednesday",
			NULL,
			NULL,
			TRUE,
			G_PARAM_READWRITE));

	e_mutual_binding_new_full (
		shell_settings, "cal-working-days-bitset",
		shell_settings, "cal-working-days-wednesday",
		transform_working_days_bitset_to_wednesday,
		transform_working_days_wednesday_to_bitset,
		(GDestroyNotify) NULL, shell_settings);

	e_shell_settings_install_property (
		g_param_spec_boolean (
			"cal-working-days-thursday",
			NULL,
			NULL,
			TRUE,
			G_PARAM_READWRITE));

	e_mutual_binding_new_full (
		shell_settings, "cal-working-days-bitset",
		shell_settings, "cal-working-days-thursday",
		transform_working_days_bitset_to_thursday,
		transform_working_days_thursday_to_bitset,
		(GDestroyNotify) NULL, shell_settings);

	e_shell_settings_install_property (
		g_param_spec_boolean (
			"cal-working-days-friday",
			NULL,
			NULL,
			TRUE,
			G_PARAM_READWRITE));

	e_mutual_binding_new_full (
		shell_settings, "cal-working-days-bitset",
		shell_settings, "cal-working-days-friday",
		transform_working_days_bitset_to_friday,
		transform_working_days_friday_to_bitset,
		(GDestroyNotify) NULL, shell_settings);

	e_shell_settings_install_property (
		g_param_spec_boolean (
			"cal-working-days-saturday",
			NULL,
			NULL,
			FALSE,
			G_PARAM_READWRITE));

	e_mutual_binding_new_full (
		shell_settings, "cal-working-days-bitset",
		shell_settings, "cal-working-days-saturday",
		transform_working_days_bitset_to_saturday,
		transform_working_days_saturday_to_bitset,
		(GDestroyNotify) g_object_unref,
		g_object_ref (shell_settings));
}
