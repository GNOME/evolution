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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "e-cal-shell-settings.h"

#include <libecal/libecal.h>

#include <e-util/e-util.h>
#include <e-util/e-util-enumtypes.h>

#define CALENDAR_SCHEMA "org.gnome.evolution.calendar"
#define EDS_CALENDAR_CONTACTS_SCHEMA "org.gnome.evolution-data-server.calendar"

static gboolean
transform_string_to_icaltimezone (GBinding *binding,
                                  const GValue *source_value,
                                  GValue *target_value,
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
		location = g_value_get_string (source_value);

	if (location != NULL && *location != '\0')
		timezone = icaltimezone_get_builtin_timezone (location);

	if (timezone == NULL)
		timezone = icaltimezone_get_utc_timezone ();

	g_value_set_pointer (target_value, timezone);

	return TRUE;
}

static gboolean
transform_icaltimezone_to_string (GBinding *binding,
                                  const GValue *source_value,
                                  GValue *target_value,
                                  gpointer user_data)
{
	EShellSettings *shell_settings;
	gboolean use_system_timezone;
	const gchar *location = NULL;
	gchar *location_str = NULL;
	icaltimezone *timezone;

	shell_settings = E_SHELL_SETTINGS (user_data);

	use_system_timezone = e_shell_settings_get_boolean (
		shell_settings, "cal-use-system-timezone");

	if (use_system_timezone) {
		location_str = e_shell_settings_get_string (
			shell_settings, "cal-timezone-string");
		location = location_str;
	} else {
		timezone = g_value_get_pointer (source_value);

		if (timezone != NULL)
			location = icaltimezone_get_location (timezone);
	}

	if (location == NULL)
		location = "UTC";

	g_value_set_string (target_value, location);

	g_free (location_str);

	return TRUE;
}

static gboolean
transform_weekdays_settings_to_evolution (GBinding *binding,
                                          const GValue *source_value,
                                          GValue *target_value,
                                          gpointer user_data)
{
	GDateWeekday weekday;

	/* XXX At some point, Evolution changed its weekday numbering
	 *     from 0 = Sunday to 0 = Monday, but did not migrate the
	 *     "week_start_day" key.  Both enumerations are of course
	 *     different from GDateWeekday.  We should have saved the
	 *     weekday as a string instead. */

	/* This is purposefully verbose for better readability. */

	/* setting numbering */
	switch (g_value_get_int (source_value)) {
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
	g_value_set_enum (target_value, weekday);

	return TRUE;
}

static gboolean
transform_weekdays_evolution_to_settings (GBinding *binding,
                                          const GValue *source_value,
                                          GValue *target_value,
                                          gpointer user_data)
{
	GDateWeekday weekday;

	/* XXX At some point, Evolution changed its weekday numbering
	 *     from 0 = Sunday to 0 = Monday, but did not migrate the
	 *     "week_start_day" key.  Both enumerations are of course
	 *     different from GDateWeekday.  We should have saved the
	 *     weekday as a string instead. */

	/* This is purposefully verbose for better readability. */

	/* Evolution numbering */
	weekday = g_value_get_enum (source_value);

	/* setting numbering */
	switch (weekday) {
		case G_DATE_MONDAY:
			g_value_set_int (target_value, 1);
			break;
		case G_DATE_TUESDAY:
			g_value_set_int (target_value, 2);
			break;
		case G_DATE_WEDNESDAY:
			g_value_set_int (target_value, 3);
			break;
		case G_DATE_THURSDAY:
			g_value_set_int (target_value, 4);
			break;
		case G_DATE_FRIDAY:
			g_value_set_int (target_value, 5);
			break;
		case G_DATE_SATURDAY:
			g_value_set_int (target_value, 6);
			break;
		case G_DATE_SUNDAY:
			g_value_set_int (target_value, 0);
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
transform_working_days_bitset_to_sunday (GBinding *binding,
                                         const GValue *source_value,
                                         GValue *target_value,
                                         gpointer user_data)
{
	gint bitset;
	gboolean working_day;

	bitset = g_value_get_int (source_value);
	working_day = ((bitset & WORKING_DAY_SUNDAY) != 0);
	g_value_set_boolean (target_value, working_day);

	return TRUE;
}

static gboolean
transform_working_days_sunday_to_bitset (GBinding *binding,
                                         const GValue *source_value,
                                         GValue *target_value,
                                         gpointer user_data)
{
	EShellSettings *shell_settings;
	gint bitset, bit;

	shell_settings = E_SHELL_SETTINGS (user_data);

	bitset = e_shell_settings_get_int (
		shell_settings, "cal-working-days-bitset");

	bit = g_value_get_boolean (source_value) ? WORKING_DAY_SUNDAY : 0;
	g_value_set_int (target_value, (bitset & ~WORKING_DAY_SUNDAY) | bit);

	return TRUE;
}

static gboolean
transform_working_days_bitset_to_monday (GBinding *binding,
                                         const GValue *source_value,
                                         GValue *target_value,
                                         gpointer user_data)
{
	gint bitset;
	gboolean working_day;

	bitset = g_value_get_int (source_value);
	working_day = ((bitset & WORKING_DAY_MONDAY) != 0);
	g_value_set_boolean (target_value, working_day);

	return TRUE;
}

static gboolean
transform_working_days_monday_to_bitset (GBinding *binding,
                                         const GValue *source_value,
                                         GValue *target_value,
                                         gpointer user_data)
{
	EShellSettings *shell_settings;
	gint bitset, bit;

	shell_settings = E_SHELL_SETTINGS (user_data);

	bitset = e_shell_settings_get_int (
		shell_settings, "cal-working-days-bitset");

	bit = g_value_get_boolean (source_value) ? WORKING_DAY_MONDAY : 0;
	g_value_set_int (target_value, (bitset & ~WORKING_DAY_MONDAY) | bit);

	return TRUE;
}

static gboolean
transform_working_days_bitset_to_tuesday (GBinding *binding,
                                          const GValue *source_value,
                                          GValue *target_value,
                                          gpointer user_data)
{
	gint bitset;
	gboolean working_day;

	bitset = g_value_get_int (source_value);
	working_day = ((bitset & WORKING_DAY_TUESDAY) != 0);
	g_value_set_boolean (target_value, working_day);

	return TRUE;
}

static gboolean
transform_working_days_tuesday_to_bitset (GBinding *binding,
                                          const GValue *source_value,
                                          GValue *target_value,
                                          gpointer user_data)
{
	EShellSettings *shell_settings;
	gint bitset, bit;

	shell_settings = E_SHELL_SETTINGS (user_data);

	bitset = e_shell_settings_get_int (
		shell_settings, "cal-working-days-bitset");

	bit = g_value_get_boolean (source_value) ? WORKING_DAY_TUESDAY : 0;
	g_value_set_int (target_value, (bitset & ~WORKING_DAY_TUESDAY) | bit);

	return TRUE;
}

static gboolean
transform_working_days_bitset_to_wednesday (GBinding *binding,
                                            const GValue *source_value,
                                            GValue *target_value,
                                            gpointer user_data)
{
	gint bitset;
	gboolean working_day;

	bitset = g_value_get_int (source_value);
	working_day = ((bitset & WORKING_DAY_WEDNESDAY) != 0);
	g_value_set_boolean (target_value, working_day);

	return TRUE;
}

static gboolean
transform_working_days_wednesday_to_bitset (GBinding *binding,
                                            const GValue *source_value,
                                            GValue *target_value,
                                            gpointer user_data)
{
	EShellSettings *shell_settings;
	gint bitset, bit;

	shell_settings = E_SHELL_SETTINGS (user_data);

	bitset = e_shell_settings_get_int (
		shell_settings, "cal-working-days-bitset");

	bit = g_value_get_boolean (source_value) ? WORKING_DAY_WEDNESDAY : 0;
	g_value_set_int (target_value, (bitset & ~WORKING_DAY_WEDNESDAY) | bit);

	return TRUE;
}

static gboolean
transform_working_days_bitset_to_thursday (GBinding *binding,
                                           const GValue *source_value,
                                           GValue *target_value,
                                           gpointer user_data)
{
	gint bitset;
	gboolean working_day;

	bitset = g_value_get_int (source_value);
	working_day = ((bitset & WORKING_DAY_THURSDAY) != 0);
	g_value_set_boolean (target_value, working_day);

	return TRUE;
}

static gboolean
transform_working_days_thursday_to_bitset (GBinding *binding,
                                           const GValue *source_value,
                                           GValue *target_value,
                                           gpointer user_data)
{
	EShellSettings *shell_settings;
	gint bitset, bit;

	shell_settings = E_SHELL_SETTINGS (user_data);

	bitset = e_shell_settings_get_int (
		shell_settings, "cal-working-days-bitset");

	bit = g_value_get_boolean (source_value) ? WORKING_DAY_THURSDAY : 0;
	g_value_set_int (target_value, (bitset & ~WORKING_DAY_THURSDAY) | bit);

	return TRUE;
}

static gboolean
transform_working_days_bitset_to_friday (GBinding *binding,
                                         const GValue *source_value,
                                         GValue *target_value,
                                         gpointer user_data)
{
	gint bitset;
	gboolean working_day;

	bitset = g_value_get_int (source_value);
	working_day = ((bitset & WORKING_DAY_FRIDAY) != 0);
	g_value_set_boolean (target_value, working_day);

	return TRUE;
}

static gboolean
transform_working_days_friday_to_bitset (GBinding *binding,
                                         const GValue *source_value,
                                         GValue *target_value,
                                         gpointer user_data)
{
	EShellSettings *shell_settings;
	gint bitset, bit;

	shell_settings = E_SHELL_SETTINGS (user_data);

	bitset = e_shell_settings_get_int (
		shell_settings, "cal-working-days-bitset");

	bit = g_value_get_boolean (source_value) ? WORKING_DAY_FRIDAY : 0;
	g_value_set_int (target_value, (bitset & ~WORKING_DAY_FRIDAY) | bit);

	return TRUE;
}

static gboolean
transform_working_days_bitset_to_saturday (GBinding *binding,
                                           const GValue *source_value,
                                           GValue *target_value,
                                           gpointer user_data)
{
	gint bitset;
	gboolean working_day;

	bitset = g_value_get_int (source_value);
	working_day = ((bitset & WORKING_DAY_SATURDAY) != 0);
	g_value_set_boolean (target_value, working_day);

	return TRUE;
}

static gboolean
transform_working_days_saturday_to_bitset (GBinding *binding,
                                           const GValue *source_value,
                                           GValue *target_value,
                                           gpointer user_data)
{
	EShellSettings *shell_settings;
	gint bitset, bit;

	shell_settings = E_SHELL_SETTINGS (user_data);

	bitset = e_shell_settings_get_int (
		shell_settings, "cal-working-days-bitset");

	bit = g_value_get_boolean (source_value) ? WORKING_DAY_SATURDAY : 0;
	g_value_set_int (target_value, (bitset & ~WORKING_DAY_SATURDAY) | bit);

	return TRUE;
}

static void
cal_use_system_timezone_changed_cb (GObject *shell_settings)
{
	g_object_notify (shell_settings, "cal-timezone-string");
}

void
e_cal_shell_backend_init_settings (EShell *shell)
{
	EShellSettings *shell_settings;

	shell_settings = e_shell_get_shell_settings (shell);

	e_shell_settings_install_property_for_key (
		"cal-ba-reminder-interval",
		EDS_CALENDAR_CONTACTS_SCHEMA,
		"contacts-reminder-interval");

	e_shell_settings_install_property_for_key (
		"cal-ba-reminder-units-string",
		EDS_CALENDAR_CONTACTS_SCHEMA,
		"contacts-reminder-units");

	e_shell_settings_install_property_for_key (
		"cal-compress-weekend",
		CALENDAR_SCHEMA,
		"compress-weekend");

	e_shell_settings_install_property_for_key (
		"cal-confirm-delete",
		CALENDAR_SCHEMA,
		"confirm-delete");

	e_shell_settings_install_property_for_key (
		"cal-confirm-purge",
		CALENDAR_SCHEMA,
		"confirm-purge");

	e_shell_settings_install_property_for_key (
		"cal-default-reminder-interval",
		CALENDAR_SCHEMA,
		"default-reminder-interval");

	/* Do not bind to this.
	 * Use "cal-default-reminder-units" instead. */
	e_shell_settings_install_property_for_key (
		"cal-default-reminder-units-string",
		CALENDAR_SCHEMA,
		"default-reminder-units");

	e_shell_settings_install_property_for_key (
		"cal-free-busy-template",
		CALENDAR_SCHEMA,
		"publish-template");

	e_shell_settings_install_property_for_key (
		"cal-hide-completed-tasks",
		CALENDAR_SCHEMA,
		"hide-completed-tasks");

	/* Do not bind to this.
	 * Use "cal-hide-completed-tasks-units" instead. */
	e_shell_settings_install_property_for_key (
		"cal-hide-completed-tasks-units-string",
		CALENDAR_SCHEMA,
		"hide-completed-tasks-units");

	e_shell_settings_install_property_for_key (
		"cal-hide-completed-tasks-value",
		CALENDAR_SCHEMA,
		"hide-completed-tasks-value");

	e_shell_settings_install_property_for_key (
		"cal-marcus-bains-day-view-color",
		CALENDAR_SCHEMA,
		"marcus-bains-color-dayview");

	e_shell_settings_install_property_for_key (
		"cal-marcus-bains-time-bar-color",
		CALENDAR_SCHEMA,
		"marcus-bains-color-timebar");

	e_shell_settings_install_property_for_key (
		"cal-marcus-bains-show-line",
		CALENDAR_SCHEMA,
		"marcus-bains-line");

	e_shell_settings_install_property_for_key (
		"cal-month-scroll-by-week",
		CALENDAR_SCHEMA,
		"month-scroll-by-week");

	e_shell_settings_install_property_for_key (
		"cal-primary-calendar",
		CALENDAR_SCHEMA,
		"primary-calendar");

	e_shell_settings_install_property_for_key (
		"cal-primary-memo-list",
		CALENDAR_SCHEMA,
		"primary-memos");

	e_shell_settings_install_property_for_key (
		"cal-primary-task-list",
		CALENDAR_SCHEMA,
		"primary-tasks");

	e_shell_settings_install_property_for_key (
		"cal-recur-events-italic",
		CALENDAR_SCHEMA,
		"recur-events-italic");

	e_shell_settings_install_property_for_key (
		"cal-search-range-years",
		CALENDAR_SCHEMA,
		"search-range-years");

	e_shell_settings_install_property_for_key (
		"cal-show-event-end-times",
		CALENDAR_SCHEMA,
		"show-event-end");

	e_shell_settings_install_property_for_key (
		"cal-show-week-numbers",
		CALENDAR_SCHEMA,
		"show-week-numbers");

	e_shell_settings_install_property_for_key (
		"cal-tasks-highlight-due-today",
		CALENDAR_SCHEMA,
		"task-due-today-highlight");

	e_shell_settings_install_property_for_key (
		"cal-tasks-color-due-today",
		CALENDAR_SCHEMA,
		"task-due-today-color");

	e_shell_settings_install_property_for_key (
		"cal-tasks-highlight-overdue",
		CALENDAR_SCHEMA,
		"task-overdue-highlight");

	e_shell_settings_install_property_for_key (
		"cal-tasks-color-overdue",
		CALENDAR_SCHEMA,
		"task-overdue-color");

	e_shell_settings_install_property_for_key (
		"cal-time-divisions",
		CALENDAR_SCHEMA,
		"time-divisions");

	/* Do not bind to this.  Use "cal-timezone" instead. */
	e_shell_settings_install_property_for_key (
		"cal-timezone-string",
		CALENDAR_SCHEMA,
		"timezone");

	e_shell_settings_install_property_for_key (
		"cal-use-24-hour-format",
		CALENDAR_SCHEMA,
		"use-24hour-format");

	e_shell_settings_install_property_for_key (
		"cal-use-ba-reminder",
		EDS_CALENDAR_CONTACTS_SCHEMA,
		"contacts-reminder-enabled");

	e_shell_settings_install_property_for_key (
		"cal-use-default-reminder",
		CALENDAR_SCHEMA,
		"use-default-reminder");

	e_shell_settings_install_property_for_key (
		"cal-use-system-timezone",
		CALENDAR_SCHEMA,
		"use-system-timezone");

	/* Do not bind to this.  Use "cal-week-start-day" instead. */
	e_shell_settings_install_property_for_key (
		"cal-week-start-day-setting",
		CALENDAR_SCHEMA,
		"week-start-day");

	e_shell_settings_install_property_for_key (
		"cal-work-day-end-hour",
		CALENDAR_SCHEMA,
		"day-end-hour");

	e_shell_settings_install_property_for_key (
		"cal-work-day-end-minute",
		CALENDAR_SCHEMA,
		"day-end-minute");

	e_shell_settings_install_property_for_key (
		"cal-work-day-start-hour",
		CALENDAR_SCHEMA,
		"day-start-hour");

	e_shell_settings_install_property_for_key (
		"cal-work-day-start-minute",
		CALENDAR_SCHEMA,
		"day-start-minute");

	e_shell_settings_install_property_for_key (
		"cal-working-days-bitset",
		CALENDAR_SCHEMA,
		"working-days");

	e_shell_settings_install_property_for_key (
		"cal-prefer-new-item",
		CALENDAR_SCHEMA,
		"prefer-new-item");

	/* These properties use transform functions to convert
	 * GSettings values to forms more useful to Evolution.
	 */

	e_shell_settings_install_property (
		g_param_spec_enum (
			"cal-ba-reminder-units",
			NULL,
			NULL,
			E_TYPE_DURATION_TYPE,
			E_DURATION_MINUTES,
			G_PARAM_READWRITE));

	g_object_bind_property_full (
		shell_settings, "cal-ba-reminder-units-string",
		shell_settings, "cal-ba-reminder-units",
		G_BINDING_BIDIRECTIONAL |
		G_BINDING_SYNC_CREATE,
		e_binding_transform_enum_nick_to_value,
		e_binding_transform_enum_value_to_nick,
		NULL, (GDestroyNotify) NULL);

	e_shell_settings_install_property (
		g_param_spec_enum (
			"cal-default-reminder-units",
			NULL,
			NULL,
			E_TYPE_DURATION_TYPE,
			E_DURATION_MINUTES,
			G_PARAM_READWRITE));

	g_object_bind_property_full (
		shell_settings, "cal-default-reminder-units-string",
		shell_settings, "cal-default-reminder-units",
		G_BINDING_BIDIRECTIONAL |
		G_BINDING_SYNC_CREATE,
		e_binding_transform_enum_nick_to_value,
		e_binding_transform_enum_value_to_nick,
		NULL, (GDestroyNotify) NULL);

	e_shell_settings_install_property (
		g_param_spec_enum (
			"cal-hide-completed-tasks-units",
			NULL,
			NULL,
			E_TYPE_DURATION_TYPE,
			E_DURATION_MINUTES,
			G_PARAM_READWRITE));

	g_object_bind_property_full (
		shell_settings, "cal-hide-completed-tasks-units-string",
		shell_settings, "cal-hide-completed-tasks-units",
		G_BINDING_BIDIRECTIONAL |
		G_BINDING_SYNC_CREATE,
		e_binding_transform_enum_nick_to_value,
		e_binding_transform_enum_value_to_nick,
		NULL, (GDestroyNotify) NULL);

	e_shell_settings_install_property (
		g_param_spec_pointer (
			"cal-timezone",
			NULL,
			NULL,
			G_PARAM_READWRITE));

	g_object_bind_property_full (
		shell_settings, "cal-timezone-string",
		shell_settings, "cal-timezone",
		G_BINDING_BIDIRECTIONAL |
		G_BINDING_SYNC_CREATE,
		transform_string_to_icaltimezone,
		transform_icaltimezone_to_string,
		g_object_ref (shell_settings),
		(GDestroyNotify) g_object_unref);

	e_shell_settings_install_property (
		g_param_spec_enum (
			"cal-week-start-day",
			NULL,
			NULL,
			E_TYPE_DATE_WEEKDAY,
			G_DATE_MONDAY,
			G_PARAM_READWRITE));

	g_object_bind_property_full (
		shell_settings, "cal-week-start-day-setting",
		shell_settings, "cal-week-start-day",
		G_BINDING_BIDIRECTIONAL |
		G_BINDING_SYNC_CREATE,
		transform_weekdays_settings_to_evolution,
		transform_weekdays_evolution_to_settings,
		NULL, (GDestroyNotify) NULL);

	/* XXX These are my favorite.  Storing a bit array in GSettings
	 *     instead of separate boolean keys.  Brilliant move. */

	e_shell_settings_install_property (
		g_param_spec_boolean (
			"cal-working-days-sunday",
			NULL,
			NULL,
			FALSE,
			G_PARAM_READWRITE));

	g_object_bind_property_full (
		shell_settings, "cal-working-days-bitset",
		shell_settings, "cal-working-days-sunday",
		G_BINDING_BIDIRECTIONAL |
		G_BINDING_SYNC_CREATE,
		transform_working_days_bitset_to_sunday,
		transform_working_days_sunday_to_bitset,
		g_object_ref (shell_settings),
		(GDestroyNotify) g_object_unref);

	e_shell_settings_install_property (
		g_param_spec_boolean (
			"cal-working-days-monday",
			NULL,
			NULL,
			TRUE,
			G_PARAM_READWRITE));

	g_object_bind_property_full (
		shell_settings, "cal-working-days-bitset",
		shell_settings, "cal-working-days-monday",
		G_BINDING_BIDIRECTIONAL |
		G_BINDING_SYNC_CREATE,
		transform_working_days_bitset_to_monday,
		transform_working_days_monday_to_bitset,
		g_object_ref (shell_settings),
		(GDestroyNotify) g_object_unref);

	e_shell_settings_install_property (
		g_param_spec_boolean (
			"cal-working-days-tuesday",
			NULL,
			NULL,
			TRUE,
			G_PARAM_READWRITE));

	g_object_bind_property_full (
		shell_settings, "cal-working-days-bitset",
		shell_settings, "cal-working-days-tuesday",
		G_BINDING_BIDIRECTIONAL |
		G_BINDING_SYNC_CREATE,
		transform_working_days_bitset_to_tuesday,
		transform_working_days_tuesday_to_bitset,
		g_object_ref (shell_settings),
		(GDestroyNotify) g_object_unref);

	e_shell_settings_install_property (
		g_param_spec_boolean (
			"cal-working-days-wednesday",
			NULL,
			NULL,
			TRUE,
			G_PARAM_READWRITE));

	g_object_bind_property_full (
		shell_settings, "cal-working-days-bitset",
		shell_settings, "cal-working-days-wednesday",
		G_BINDING_BIDIRECTIONAL |
		G_BINDING_SYNC_CREATE,
		transform_working_days_bitset_to_wednesday,
		transform_working_days_wednesday_to_bitset,
		g_object_ref (shell_settings),
		(GDestroyNotify) g_object_unref);

	e_shell_settings_install_property (
		g_param_spec_boolean (
			"cal-working-days-thursday",
			NULL,
			NULL,
			TRUE,
			G_PARAM_READWRITE));

	g_object_bind_property_full (
		shell_settings, "cal-working-days-bitset",
		shell_settings, "cal-working-days-thursday",
		G_BINDING_BIDIRECTIONAL |
		G_BINDING_SYNC_CREATE,
		transform_working_days_bitset_to_thursday,
		transform_working_days_thursday_to_bitset,
		g_object_ref (shell_settings),
		(GDestroyNotify) g_object_unref);

	e_shell_settings_install_property (
		g_param_spec_boolean (
			"cal-working-days-friday",
			NULL,
			NULL,
			TRUE,
			G_PARAM_READWRITE));

	g_object_bind_property_full (
		shell_settings, "cal-working-days-bitset",
		shell_settings, "cal-working-days-friday",
		G_BINDING_BIDIRECTIONAL |
		G_BINDING_SYNC_CREATE,
		transform_working_days_bitset_to_friday,
		transform_working_days_friday_to_bitset,
		g_object_ref (shell_settings),
		(GDestroyNotify) g_object_unref);

	e_shell_settings_install_property (
		g_param_spec_boolean (
			"cal-working-days-saturday",
			NULL,
			NULL,
			FALSE,
			G_PARAM_READWRITE));

	g_object_bind_property_full (
		shell_settings, "cal-working-days-bitset",
		shell_settings, "cal-working-days-saturday",
		G_BINDING_BIDIRECTIONAL |
		G_BINDING_SYNC_CREATE,
		transform_working_days_bitset_to_saturday,
		transform_working_days_saturday_to_bitset,
		g_object_ref (shell_settings),
		(GDestroyNotify) g_object_unref);

	g_signal_connect (
		shell_settings, "notify::cal-use-system-timezone",
		G_CALLBACK (cal_use_system_timezone_changed_cb), NULL);
}
