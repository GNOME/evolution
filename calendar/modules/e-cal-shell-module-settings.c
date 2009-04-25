/*
 * e-cal-shell-module-settings.c
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

#include "e-cal-shell-module-settings.h"

#include <gconf/gconf-client.h>

void
e_cal_shell_module_init_settings (EShell *shell)
{
	EShellSettings *shell_settings;

	shell_settings = e_shell_get_shell_settings (shell);

	/* XXX Default values should match the GConf schema.
	 *     Yes it's redundant, but we're stuck with GConf. */

	e_shell_settings_install_property (
		g_param_spec_string (
			"cal-primary-calendar",
			NULL,
			NULL,
			NULL,
			G_PARAM_READWRITE));

	e_shell_settings_bind_to_gconf (
		shell_settings, "cal-primary-calendar",
		"/apps/evolution/calendar/display/primary_calendar");

	e_shell_settings_install_property (
		g_param_spec_boolean (
			"cal-use-system-timezone",
			NULL,
			NULL,
			TRUE,
			G_PARAM_READWRITE));

	e_shell_settings_bind_to_gconf (
		shell_settings, "cal-use-system-timezone",
		"/apps/evolution/calendar/display/use_system_timezone");
}
