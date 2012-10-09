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

#include <shell/e-shell.h>
#include <calendar/gui/e-cal-model.h>
#include <calendar/gui/e-cal-model-tasks.h>

#define E_CAL_CONFIG_MODEL_GET_PRIVATE(obj) \
	(G_TYPE_INSTANCE_GET_PRIVATE \
	((obj), E_TYPE_CAL_CONFIG_MODEL, ECalConfigModelPrivate))

struct _ECalConfigModelPrivate {
	gint placeholder;
};

G_DEFINE_DYNAMIC_TYPE (
	ECalConfigModel,
	e_cal_config_model,
	E_TYPE_EXTENSION)

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
	G_OBJECT_CLASS (e_cal_config_model_parent_class)->constructed (object);
}

static void
e_cal_config_model_class_init (ECalConfigModelClass *class)
{
	GObjectClass *object_class;
	EExtensionClass *extension_class;

	g_type_class_add_private (class, sizeof (ECalConfigModelPrivate));

	object_class = G_OBJECT_CLASS (class);
	object_class->constructed = cal_config_model_constructed;

	extension_class = E_EXTENSION_CLASS (class);
	extension_class->extensible_type = E_TYPE_CAL_MODEL;
}

static void
e_cal_config_model_class_finalize (ECalConfigModelClass *class)
{
}

static void
e_cal_config_model_init (ECalConfigModel *extension)
{
	extension->priv = E_CAL_CONFIG_MODEL_GET_PRIVATE (extension);
}

void
e_cal_config_model_type_register (GTypeModule *type_module)
{
	/* XXX G_DEFINE_DYNAMIC_TYPE declares a static type registration
	 *     function, so we have to wrap it with a public function in
	 *     order to register types from a separate compilation unit. */
	e_cal_config_model_register_type (type_module);
}

