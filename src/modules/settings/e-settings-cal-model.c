/*
 * e-settings-cal-model.c
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
 */

#include "evolution-config.h"

#include "e-settings-cal-model.h"

#include <calendar/gui/e-cal-model.h>
#include <calendar/gui/e-cal-model-tasks.h>

struct _ESettingsCalModelPrivate {
	gint placeholder;
};

G_DEFINE_DYNAMIC_TYPE_EXTENDED (ESettingsCalModel, e_settings_cal_model, E_TYPE_EXTENSION, 0,
	G_ADD_PRIVATE_DYNAMIC (ESettingsCalModel))

static gboolean
settings_map_string_to_icaltimezone (GValue *value,
                                     GVariant *variant,
                                     gpointer user_data)
{
	GSettings *settings;
	const gchar *location = NULL;
	ICalTimezone *timezone = NULL;

	settings = e_util_ref_settings ("org.gnome.evolution.calendar");

	if (g_settings_get_boolean (settings, "use-system-timezone"))
		timezone = e_cal_util_get_system_timezone ();
	else
		location = g_variant_get_string (variant, NULL);

	if (location != NULL && *location != '\0')
		timezone = i_cal_timezone_get_builtin_timezone (location);

	if (timezone == NULL)
		timezone = i_cal_timezone_get_utc_timezone ();

	g_value_set_object (value, timezone);

	g_object_unref (settings);

	return TRUE;
}

static void
settings_cal_model_constructed (GObject *object)
{
	EExtension *extension;
	EExtensible *extensible;
	GSettings *settings;

	extension = E_EXTENSION (object);
	extensible = e_extension_get_extensible (extension);

	settings = e_util_ref_settings ("org.gnome.evolution.calendar");

	/*** ECalModel ***/

	g_settings_bind (
		settings, "compress-weekend",
		extensible, "compress-weekend",
		G_SETTINGS_BIND_GET);

	g_settings_bind (
		settings, "confirm-delete",
		extensible, "confirm-delete",
		G_SETTINGS_BIND_GET);

	g_settings_bind (
		settings, "default-reminder-interval",
		extensible, "default-reminder-interval",
		G_SETTINGS_BIND_GET);

	g_settings_bind (
		settings, "default-reminder-units",
		extensible, "default-reminder-units",
		G_SETTINGS_BIND_GET);

	g_settings_bind_with_mapping (
		settings, "timezone",
		extensible, "timezone",
		G_SETTINGS_BIND_GET,
		settings_map_string_to_icaltimezone,
		NULL, /* one-way binding */
		NULL, (GDestroyNotify) NULL);

	g_settings_bind (
		settings, "use-24hour-format",
		extensible, "use-24-hour-format",
		G_SETTINGS_BIND_GET);

	g_settings_bind (
		settings, "use-default-reminder",
		extensible, "use-default-reminder",
		G_SETTINGS_BIND_GET);

	g_settings_bind (
		settings, "week-start-day-name",
		extensible, "week-start-day",
		G_SETTINGS_BIND_GET);

	g_settings_bind (
		settings, "work-day-monday",
		extensible, "work-day-monday",
		G_SETTINGS_BIND_GET);

	g_settings_bind  (
		settings, "work-day-tuesday",
		extensible, "work-day-tuesday",
		G_SETTINGS_BIND_GET);

	g_settings_bind (
		settings, "work-day-wednesday",
		extensible, "work-day-wednesday",
		G_SETTINGS_BIND_GET);

	g_settings_bind (
		settings, "work-day-thursday",
		extensible, "work-day-thursday",
		G_SETTINGS_BIND_GET);

	g_settings_bind (
		settings, "work-day-friday",
		extensible, "work-day-friday",
		G_SETTINGS_BIND_GET);

	g_settings_bind (
		settings, "work-day-saturday",
		extensible, "work-day-saturday",
		G_SETTINGS_BIND_GET);

	g_settings_bind (
		settings, "work-day-sunday",
		extensible, "work-day-sunday",
		G_SETTINGS_BIND_GET);

	g_settings_bind (
		settings, "day-end-hour",
		extensible, "work-day-end-hour",
		G_SETTINGS_BIND_GET);

	g_settings_bind (
		settings, "day-end-minute",
		extensible, "work-day-end-minute",
		G_SETTINGS_BIND_GET);

	g_settings_bind (
		settings, "day-start-hour",
		extensible, "work-day-start-hour",
		G_SETTINGS_BIND_GET);

	g_settings_bind (
		settings, "day-start-minute",
		extensible, "work-day-start-minute",
		G_SETTINGS_BIND_GET);

	g_settings_bind (
		settings, "day-start-mon",
		extensible, "work-day-start-mon",
		G_SETTINGS_BIND_GET);

	g_settings_bind (
		settings, "day-end-mon",
		extensible, "work-day-end-mon",
		G_SETTINGS_BIND_GET);

	g_settings_bind (
		settings, "day-start-tue",
		extensible, "work-day-start-tue",
		G_SETTINGS_BIND_GET);

	g_settings_bind (
		settings, "day-end-tue",
		extensible, "work-day-end-tue",
		G_SETTINGS_BIND_GET);

	g_settings_bind (
		settings, "day-start-wed",
		extensible, "work-day-start-wed",
		G_SETTINGS_BIND_GET);

	g_settings_bind (
		settings, "day-end-wed",
		extensible, "work-day-end-wed",
		G_SETTINGS_BIND_GET);

	g_settings_bind (
		settings, "day-start-thu",
		extensible, "work-day-start-thu",
		G_SETTINGS_BIND_GET);

	g_settings_bind (
		settings, "day-end-thu",
		extensible, "work-day-end-thu",
		G_SETTINGS_BIND_GET);

	g_settings_bind (
		settings, "day-start-fri",
		extensible, "work-day-start-fri",
		G_SETTINGS_BIND_GET);

	g_settings_bind (
		settings, "day-end-fri",
		extensible, "work-day-end-fri",
		G_SETTINGS_BIND_GET);

	g_settings_bind (
		settings, "day-start-sat",
		extensible, "work-day-start-sat",
		G_SETTINGS_BIND_GET);

	g_settings_bind (
		settings, "day-end-sat",
		extensible, "work-day-end-sat",
		G_SETTINGS_BIND_GET);

	g_settings_bind (
		settings, "day-start-sun",
		extensible, "work-day-start-sun",
		G_SETTINGS_BIND_GET);

	g_settings_bind (
		settings, "day-end-sun",
		extensible, "work-day-end-sun",
		G_SETTINGS_BIND_GET);

	/*** ECalModelTasks ***/

	if (E_IS_CAL_MODEL_TASKS (extensible)) {

		g_settings_bind (
			settings, "task-due-today-highlight",
			extensible, "highlight-due-today",
			G_SETTINGS_BIND_GET);

		g_settings_bind (
			settings, "task-due-today-color",
			extensible, "color-due-today",
			G_SETTINGS_BIND_GET);

		g_settings_bind (
			settings, "task-overdue-highlight",
			extensible, "highlight-overdue",
			G_SETTINGS_BIND_GET);

		g_settings_bind (
			settings, "task-overdue-color",
			extensible, "color-overdue",
			G_SETTINGS_BIND_GET);
	}

	g_object_unref (settings);

	/* Chain up to parent's constructed() method. */
	G_OBJECT_CLASS (e_settings_cal_model_parent_class)->constructed (object);
}

static void
e_settings_cal_model_class_init (ESettingsCalModelClass *class)
{
	GObjectClass *object_class;
	EExtensionClass *extension_class;

	object_class = G_OBJECT_CLASS (class);
	object_class->constructed = settings_cal_model_constructed;

	extension_class = E_EXTENSION_CLASS (class);
	extension_class->extensible_type = E_TYPE_CAL_MODEL;
}

static void
e_settings_cal_model_class_finalize (ESettingsCalModelClass *class)
{
}

static void
e_settings_cal_model_init (ESettingsCalModel *extension)
{
	extension->priv = e_settings_cal_model_get_instance_private (extension);
}

void
e_settings_cal_model_type_register (GTypeModule *type_module)
{
	/* XXX G_DEFINE_DYNAMIC_TYPE declares a static type registration
	 *     function, so we have to wrap it with a public function in
	 *     order to register types from a separate compilation unit. */
	e_settings_cal_model_register_type (type_module);
}

