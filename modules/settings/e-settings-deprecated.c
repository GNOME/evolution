/*
 * e-settings-deprecated.c
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

/* This class is different from the others in this module.  Its purpose
 * is to transfer values from deprecated GSettings keys to the preferred
 * keys on startup, and keep them synchronized at all times for backward
 * compatibility. */

#include "e-settings-deprecated.h"

#include <shell/e-shell.h>

#define E_SETTINGS_DEPRECATED_GET_PRIVATE(obj) \
	(G_TYPE_INSTANCE_GET_PRIVATE \
	((obj), E_TYPE_SETTINGS_DEPRECATED, ESettingsDeprecatedPrivate))

struct _ESettingsDeprecatedPrivate {
	GSettings *calendar_settings;
	gulong week_start_day_name_handler_id;
	gulong work_day_monday_handler_id;
	gulong work_day_tuesday_handler_id;
	gulong work_day_wednesday_handler_id;
	gulong work_day_thursday_handler_id;
	gulong work_day_friday_handler_id;
	gulong work_day_saturday_handler_id;
	gulong work_day_sunday_handler_id;
};

/* Flag values used in the "working-days" key. */
enum {
	DEPRECATED_WORKING_DAYS_SUNDAY    = 1 << 0,
	DEPRECATED_WORKING_DAYS_MONDAY    = 1 << 1,
	DEPRECATED_WORKING_DAYS_TUESDAY   = 1 << 2,
	DEPRECATED_WORKING_DAYS_WEDNESDAY = 1 << 3,
	DEPRECATED_WORKING_DAYS_THURSDAY  = 1 << 4,
	DEPRECATED_WORKING_DAYS_FRIDAY    = 1 << 5,
	DEPRECATED_WORKING_DAYS_SATURDAY  = 1 << 6
};

G_DEFINE_DYNAMIC_TYPE (
	ESettingsDeprecated,
	e_settings_deprecated,
	E_TYPE_EXTENSION)

static void
settings_deprecated_week_start_day_name_cb (GSettings *settings,
                                            const gchar *key)
{
	GDateWeekday weekday;
	gint tm_wday;

	weekday = g_settings_get_enum (settings, "week-start-day-name");
	tm_wday = e_weekday_to_tm_wday (weekday);
	g_settings_set_int (settings, "week-start-day", tm_wday);
}

static void
settings_deprecated_work_day_monday_cb (GSettings *settings,
                                        const gchar *key)
{
	gint flags;

	flags = g_settings_get_int (settings, "working-days");
	if (g_settings_get_boolean (settings, "work-day-monday"))
		flags |= DEPRECATED_WORKING_DAYS_MONDAY;
	else
		flags &= ~DEPRECATED_WORKING_DAYS_MONDAY;
	g_settings_set_int (settings, "working-days", flags);
}

static void
settings_deprecated_work_day_tuesday_cb (GSettings *settings,
                                         const gchar *key)
{
	gint flags;

	flags = g_settings_get_int (settings, "working-days");
	if (g_settings_get_boolean (settings, "work-day-tuesday"))
		flags |= DEPRECATED_WORKING_DAYS_TUESDAY;
	else
		flags &= ~DEPRECATED_WORKING_DAYS_TUESDAY;
	g_settings_set_int (settings, "working-days", flags);
}

static void
settings_deprecated_work_day_wednesday_cb (GSettings *settings,
                                           const gchar *key)
{
	gint flags;

	flags = g_settings_get_int (settings, "working-days");
	if (g_settings_get_boolean (settings, "work-day-wednesday"))
		flags |= DEPRECATED_WORKING_DAYS_WEDNESDAY;
	else
		flags &= ~DEPRECATED_WORKING_DAYS_WEDNESDAY;
	g_settings_set_int (settings, "working-days", flags);
}

static void
settings_deprecated_work_day_thursday_cb (GSettings *settings,
                                          const gchar *key)
{
	gint flags;

	flags = g_settings_get_int (settings, "working-days");
	if (g_settings_get_boolean (settings, "work-day-thursday"))
		flags |= DEPRECATED_WORKING_DAYS_THURSDAY;
	else
		flags &= ~DEPRECATED_WORKING_DAYS_THURSDAY;
	g_settings_set_int (settings, "working-days", flags);
}

static void
settings_deprecated_work_day_friday_cb (GSettings *settings,
                                        const gchar *key)
{
	gint flags;

	flags = g_settings_get_int (settings, "working-days");
	if (g_settings_get_boolean (settings, "work-day-friday"))
		flags |= DEPRECATED_WORKING_DAYS_FRIDAY;
	else
		flags &= ~DEPRECATED_WORKING_DAYS_FRIDAY;
	g_settings_set_int (settings, "working-days", flags);
}

static void
settings_deprecated_work_day_saturday_cb (GSettings *settings,
                                          const gchar *key)
{
	gint flags;

	flags = g_settings_get_int (settings, "working-days");
	if (g_settings_get_boolean (settings, "work-day-saturday"))
		flags |= DEPRECATED_WORKING_DAYS_SATURDAY;
	else
		flags &= ~DEPRECATED_WORKING_DAYS_SATURDAY;
	g_settings_set_int (settings, "working-days", flags);
}

static void
settings_deprecated_work_day_sunday_cb (GSettings *settings,
                                        const gchar *key)
{
	gint flags;

	flags = g_settings_get_int (settings, "working-days");
	if (g_settings_get_boolean (settings, "work-day-sunday"))
		flags |= DEPRECATED_WORKING_DAYS_SUNDAY;
	else
		flags &= ~DEPRECATED_WORKING_DAYS_SUNDAY;
	g_settings_set_int (settings, "working-days", flags);
}

static void
settings_deprecated_dispose (GObject *object)
{
	ESettingsDeprecatedPrivate *priv;

	priv = E_SETTINGS_DEPRECATED_GET_PRIVATE (object);

	if (priv->week_start_day_name_handler_id > 0) {
		g_signal_handler_disconnect (
			priv->calendar_settings,
			priv->week_start_day_name_handler_id);
		priv->week_start_day_name_handler_id = 0;
	}

	if (priv->work_day_monday_handler_id > 0) {
		g_signal_handler_disconnect (
			priv->calendar_settings,
			priv->work_day_monday_handler_id);
		priv->work_day_monday_handler_id = 0;
	}

	if (priv->work_day_tuesday_handler_id > 0) {
		g_signal_handler_disconnect (
			priv->calendar_settings,
			priv->work_day_tuesday_handler_id);
		priv->work_day_tuesday_handler_id = 0;
	}

	if (priv->work_day_wednesday_handler_id > 0) {
		g_signal_handler_disconnect (
			priv->calendar_settings,
			priv->work_day_wednesday_handler_id);
		priv->work_day_wednesday_handler_id = 0;
	}

	if (priv->work_day_thursday_handler_id > 0) {
		g_signal_handler_disconnect (
			priv->calendar_settings,
			priv->work_day_thursday_handler_id);
		priv->work_day_thursday_handler_id = 0;
	}

	if (priv->work_day_friday_handler_id > 0) {
		g_signal_handler_disconnect (
			priv->calendar_settings,
			priv->work_day_friday_handler_id);
		priv->work_day_friday_handler_id = 0;
	}

	if (priv->work_day_saturday_handler_id > 0) {
		g_signal_handler_disconnect (
			priv->calendar_settings,
			priv->work_day_saturday_handler_id);
		priv->work_day_saturday_handler_id = 0;
	}

	if (priv->work_day_sunday_handler_id > 0) {
		g_signal_handler_disconnect (
			priv->calendar_settings,
			priv->work_day_sunday_handler_id);
		priv->work_day_sunday_handler_id = 0;
	}

	g_clear_object (&priv->calendar_settings);

	/* Chain up to parent's dispose() method. */
	G_OBJECT_CLASS (e_settings_deprecated_parent_class)->dispose (object);
}

static void
settings_deprecated_constructed (GObject *object)
{
	ESettingsDeprecatedPrivate *priv;
	gulong handler_id;
	gint int_value;

	priv = E_SETTINGS_DEPRECATED_GET_PRIVATE (object);

	/* Chain up to parent's constructed() method. */
	G_OBJECT_CLASS (e_settings_deprecated_parent_class)->
		constructed (object);

	/* Migrate values from deprecated to preferred keys. */

	int_value = g_settings_get_int (
		priv->calendar_settings, "week-start-day");
	g_settings_set_enum (
		priv->calendar_settings, "week-start-day-name",
		e_weekday_from_tm_wday (int_value));

	int_value = g_settings_get_int (
		priv->calendar_settings, "working-days");
	g_settings_set_boolean (
		priv->calendar_settings, "work-day-monday",
		(int_value & DEPRECATED_WORKING_DAYS_MONDAY) != 0);
	g_settings_set_boolean (
		priv->calendar_settings, "work-day-tuesday",
		(int_value & DEPRECATED_WORKING_DAYS_TUESDAY) != 0);
	g_settings_set_boolean (
		priv->calendar_settings, "work-day-wednesday",
		(int_value & DEPRECATED_WORKING_DAYS_WEDNESDAY) != 0);
	g_settings_set_boolean (
		priv->calendar_settings, "work-day-thursday",
		(int_value & DEPRECATED_WORKING_DAYS_THURSDAY) != 0);
	g_settings_set_boolean (
		priv->calendar_settings, "work-day-friday",
		(int_value & DEPRECATED_WORKING_DAYS_FRIDAY) != 0);
	g_settings_set_boolean (
		priv->calendar_settings, "work-day-saturday",
		(int_value & DEPRECATED_WORKING_DAYS_SATURDAY) != 0);
	g_settings_set_boolean (
		priv->calendar_settings, "work-day-sunday",
		(int_value & DEPRECATED_WORKING_DAYS_SUNDAY) != 0);

	/* Write changes back to the deprecated keys. */

	handler_id = g_signal_connect (
		priv->calendar_settings, "changed::week-start-day-name",
		G_CALLBACK (settings_deprecated_week_start_day_name_cb), NULL);
	priv->week_start_day_name_handler_id = handler_id;

	handler_id = g_signal_connect (
		priv->calendar_settings, "changed::work-day-monday",
		G_CALLBACK (settings_deprecated_work_day_monday_cb), NULL);
	priv->work_day_monday_handler_id = handler_id;

	handler_id = g_signal_connect (
		priv->calendar_settings, "changed::work-day-tuesday",
		G_CALLBACK (settings_deprecated_work_day_tuesday_cb), NULL);
	priv->work_day_tuesday_handler_id = handler_id;

	handler_id = g_signal_connect (
		priv->calendar_settings, "changed::work-day-wednesday",
		G_CALLBACK (settings_deprecated_work_day_wednesday_cb), NULL);
	priv->work_day_wednesday_handler_id = handler_id;

	handler_id = g_signal_connect (
		priv->calendar_settings, "changed::work-day-thursday",
		G_CALLBACK (settings_deprecated_work_day_thursday_cb), NULL);
	priv->work_day_thursday_handler_id = handler_id;

	handler_id = g_signal_connect (
		priv->calendar_settings, "changed::work-day-friday",
		G_CALLBACK (settings_deprecated_work_day_friday_cb), NULL);
	priv->work_day_friday_handler_id = handler_id;

	handler_id = g_signal_connect (
		priv->calendar_settings, "changed::work-day-saturday",
		G_CALLBACK (settings_deprecated_work_day_saturday_cb), NULL);
	priv->work_day_saturday_handler_id = handler_id;

	handler_id = g_signal_connect (
		priv->calendar_settings, "changed::work-day-sunday",
		G_CALLBACK (settings_deprecated_work_day_sunday_cb), NULL);
	priv->work_day_sunday_handler_id = handler_id;
}

static void
e_settings_deprecated_class_init (ESettingsDeprecatedClass *class)
{
	GObjectClass *object_class;
	EExtensionClass *extension_class;

	g_type_class_add_private (class, sizeof (ESettingsDeprecatedPrivate));

	object_class = G_OBJECT_CLASS (class);
	object_class->dispose = settings_deprecated_dispose;
	object_class->constructed = settings_deprecated_constructed;

	extension_class = E_EXTENSION_CLASS (class);
	extension_class->extensible_type = E_TYPE_SHELL;
}

static void
e_settings_deprecated_class_finalize (ESettingsDeprecatedClass *class)
{
}

static void
e_settings_deprecated_init (ESettingsDeprecated *extension)
{
	GSettings *settings;

	extension->priv = E_SETTINGS_DEPRECATED_GET_PRIVATE (extension);

	settings = g_settings_new ("org.gnome.evolution.calendar");
	extension->priv->calendar_settings = settings;
}

void
e_settings_deprecated_type_register (GTypeModule *type_module)
{
	/* XXX G_DEFINE_DYNAMIC_TYPE declares a static type registration
	 *     function, so we have to wrap it with a public function in
	 *     order to register types from a separate compilation unit. */
	e_settings_deprecated_register_type (type_module);
}

