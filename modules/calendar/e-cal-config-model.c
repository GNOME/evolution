/*
 * e-cal-config-model.c
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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "e-cal-config-model.h"

#include <libebackend/e-extension.h>

#include <shell/e-shell.h>
#include <calendar/gui/e-cal-model.h>
#include <calendar/gui/e-cal-model-tasks.h>

static gpointer parent_class;

static void
cal_config_model_constructed (GObject *object)
{
	EExtension *extension;
	EExtensible *extensible;
	EShellSettings *shell_settings;
	EShell *shell;

	extension = E_EXTENSION (object);
	extensible = e_extension_get_extensible (extension);

	shell = e_shell_get_default ();
	shell_settings = e_shell_get_shell_settings (shell);

	/*** ECalModel ***/

	g_object_bind_property (
		shell_settings, "cal-compress-weekend",
		extensible, "compress-weekend",
		G_BINDING_SYNC_CREATE);

	g_object_bind_property (
		shell_settings, "cal-confirm-delete",
		extensible, "confirm-delete",
		G_BINDING_SYNC_CREATE);

	g_object_bind_property (
		shell_settings, "cal-default-reminder-interval",
		extensible, "default-reminder-interval",
		G_BINDING_SYNC_CREATE);

	g_object_bind_property (
		shell_settings, "cal-default-reminder-units",
		extensible, "default-reminder-units",
		G_BINDING_SYNC_CREATE);

	g_object_bind_property (
		shell_settings, "cal-timezone",
		extensible, "timezone",
		G_BINDING_SYNC_CREATE);

	g_object_bind_property (
		shell_settings, "cal-use-24-hour-format",
		extensible, "use-24-hour-format",
		G_BINDING_SYNC_CREATE);

	g_object_bind_property (
		shell_settings, "cal-use-default-reminder",
		extensible, "use-default-reminder",
		G_BINDING_SYNC_CREATE);

	g_object_bind_property (
		shell_settings, "cal-week-start-day",
		extensible, "week-start-day",
		G_BINDING_SYNC_CREATE);

	g_object_bind_property (
		shell_settings, "cal-work-day-end-hour",
		extensible, "work-day-end-hour",
		G_BINDING_SYNC_CREATE);

	g_object_bind_property (
		shell_settings, "cal-work-day-end-minute",
		extensible, "work-day-end-minute",
		G_BINDING_SYNC_CREATE);

	g_object_bind_property (
		shell_settings, "cal-work-day-start-hour",
		extensible, "work-day-start-hour",
		G_BINDING_SYNC_CREATE);

	g_object_bind_property (
		shell_settings, "cal-work-day-start-minute",
		extensible, "work-day-start-minute",
		G_BINDING_SYNC_CREATE);

	/*** ECalModelTasks ***/

	if (E_IS_CAL_MODEL_TASKS (extensible)) {

		g_object_bind_property (
			shell_settings, "cal-tasks-highlight-due-today",
			extensible, "highlight-due-today",
			G_BINDING_SYNC_CREATE);

		g_object_bind_property (
			shell_settings, "cal-tasks-color-due-today",
			extensible, "color-due-today",
			G_BINDING_SYNC_CREATE);

		g_object_bind_property (
			shell_settings, "cal-tasks-highlight-overdue",
			extensible, "highlight-overdue",
			G_BINDING_SYNC_CREATE);

		g_object_bind_property (
			shell_settings, "cal-tasks-color-overdue",
			extensible, "color-overdue",
			G_BINDING_SYNC_CREATE);
	}

	/* Chain up to parent's constructed() method. */
	G_OBJECT_CLASS (parent_class)->constructed (object);
}

static void
cal_config_model_class_init (EExtensionClass *class)
{
	GObjectClass *object_class;

	parent_class = g_type_class_peek_parent (class);

	object_class = G_OBJECT_CLASS (class);
	object_class->constructed = cal_config_model_constructed;

	class->extensible_type = E_TYPE_CAL_MODEL;
}

void
e_cal_config_model_register_type (GTypeModule *type_module)
{
	static const GTypeInfo type_info = {
		sizeof (EExtensionClass),
		(GBaseInitFunc) NULL,
		(GBaseFinalizeFunc) NULL,
		(GClassInitFunc) cal_config_model_class_init,
		(GClassFinalizeFunc) NULL,
		NULL,  /* class_data */
		sizeof (EExtension),
		0,     /* n_preallocs */
		(GInstanceInitFunc) NULL,
		NULL   /* value_table */
	};

	g_type_module_register_type (
		type_module, E_TYPE_EXTENSION,
		"ECalConfigModel", &type_info, 0);
}
