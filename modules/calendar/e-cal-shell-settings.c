/*
 * e-cal-shell-backend-settings.c
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

void
e_cal_shell_backend_init_settings (EShell *shell)
{
	EShellSettings *shell_settings;

	shell_settings = e_shell_get_shell_settings (shell);

	e_shell_settings_install_property_for_key (
		"cal-compress-weekend",
		"/apps/evolution/calendar/display/compress_weekend");

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
		"cal-show-event-end-times",
		"/apps/evolution/calendar/display/show_event_end");

	e_shell_settings_install_property_for_key (
		"cal-time-divisions",
		"/apps/evolution/calendar/display/time_divisions");

	e_shell_settings_install_property_for_key (
		"cal-use-24-hour-format",
		"/apps/evolution/calendar/display/use_24hour_format");

	e_shell_settings_install_property_for_key (
		"cal-use-system-timezone",
		"/apps/evolution/calendar/display/use_system_timezone");

	e_shell_settings_install_property_for_key (
		"cal-week-start-day",
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
		"cal-working-days",
		"/apps/evolution/calendar/display/working_days");
}
