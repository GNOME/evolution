/*
 * e-cal-config-view.c
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
 */

#include "e-cal-config-view.h"

#include <shell/e-shell.h>
#include <e-util/e-binding.h>
#include <e-util/e-extension.h>
#include <calendar/gui/e-day-view.h>
#include <calendar/gui/e-week-view.h>

static void
cal_config_view_constructed (GObject *object)
{
	EExtension *extension;
	EExtensible *extensible;
	EShellSettings *shell_settings;
	EShell *shell;

	extension = E_EXTENSION (object);
	extensible = e_extension_get_extensible (extension);

	shell = e_shell_get_default ();
	shell_settings = e_shell_get_shell_settings (shell);

	/*** EDayView ***/

	if (E_IS_DAY_VIEW (extensible)) {

		e_binding_new (
			shell_settings, "cal-show-week-numbers",
			E_DAY_VIEW (extensible)->week_number_label, "visible");

		e_binding_new (
			shell_settings, "cal-marcus-bains-show-line",
			extensible, "marcus-bains-show-line");

		e_binding_new (
			shell_settings, "cal-marcus-bains-day-view-color",
			extensible, "marcus-bains-day-view-color");

		e_binding_new (
			shell_settings, "cal-marcus-bains-time-bar-color",
			extensible, "marcus-bains-time-bar-color");

		e_binding_new (
			shell_settings, "cal-time-divisions",
			extensible, "mins-per-row");

		e_binding_new (
			shell_settings, "cal-work-day-end-hour",
			extensible, "work-day-end-hour");

		e_binding_new (
			shell_settings, "cal-work-day-end-minute",
			extensible, "work-day-end-minute");

		e_binding_new (
			shell_settings, "cal-work-day-start-hour",
			extensible, "work-day-start-hour");

		e_binding_new (
			shell_settings, "cal-work-day-start-minute",
			extensible, "work-day-start-minute");

		e_binding_new (
			shell_settings, "cal-working-days-bitset",
			extensible, "working-days");
	}

	/*** EWeekView ***/

	if (E_IS_WEEK_VIEW (extensible)) {

		e_binding_new (
			shell_settings, "cal-compress-weekend",
			extensible, "compress-weekend");

		e_binding_new (
			shell_settings, "cal-show-event-end-times",
			extensible, "show-event-end-times");
	}
}

static void
cal_config_view_class_init (EExtensionClass *class)
{
	GObjectClass *object_class;

	object_class = G_OBJECT_CLASS (class);
	object_class->constructed = cal_config_view_constructed;

	class->extensible_type = E_TYPE_CALENDAR_VIEW;
}

void
e_cal_config_view_register_type (GTypeModule *type_module)
{
	static const GTypeInfo type_info = {
		sizeof (EExtensionClass),
		(GBaseInitFunc) NULL,
		(GBaseFinalizeFunc) NULL,
		(GClassInitFunc) cal_config_view_class_init,
		(GClassFinalizeFunc) NULL,
		NULL,  /* class_data */
		sizeof (EExtension),
		0,     /* n_preallocs */
		(GInstanceInitFunc) NULL,
		NULL   /* value_table */
	};

	g_type_module_register_type (
		type_module, E_TYPE_EXTENSION,
		"ECalConfigView", &type_info, 0);
}
