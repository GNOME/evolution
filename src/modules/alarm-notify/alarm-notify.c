/*
 * Copyright (C) 2018 Red Hat, Inc. (www.redhat.com)
 *
 * This library is free software: you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE. See the GNU Lesser General Public License
 * for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

#include "evolution-config.h"

#include <gmodule.h>
#include <glib-object.h>
#include <glib/gi18n-lib.h>

#include <libedataserverui/libedataserverui.h>

#include "e-util/e-util.h"

/* Standard GObject macros */
#define E_TYPE_ALARM_NOTIFY_MODULE \
	(e_alarm_notify_module_get_type ())
#define E_ALARM_NOTIFY_MODULE(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), E_TYPE_ALARM_NOTIFY_MODULE, EAlarmNotifyModule))
#define E_ALARM_NOTIFY_MODULE_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((cls), E_TYPE_ALARM_NOTIFY_MODULE, EAlarmNotifyModuleClass))
#define E_IS_ALARM_NOTIFY_MODULE(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), E_TYPE_ALARM_NOTIFY_MODULE))
#define E_IS_ALARM_NOTIFY_MODULE_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_TYPE \
	((cls), E_TYPE_ALARM_NOTIFY_MODULE))
#define E_ALARM_NOTIFY_MODULE_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS \
	((obj), E_TYPE_ALARM_NOTIFY_MODULE, EAlarmNotifyModuleClass))

typedef struct _EAlarmNotifyModule EAlarmNotifyModule;
typedef struct _EAlarmNotifyModuleClass EAlarmNotifyModuleClass;

struct _EAlarmNotifyModule {
	EExtension parent;
	GFileMonitor *monitor;
};

struct _EAlarmNotifyModuleClass {
	EExtensionClass parent_class;
};

GType e_alarm_notify_module_get_type (void) G_GNUC_CONST;

G_DEFINE_DYNAMIC_TYPE (EAlarmNotifyModule, e_alarm_notify_module, E_TYPE_EXTENSION)

static gboolean
alarm_notify_module_map_string_to_icaltimezone (GValue *value,
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

	if (location && *location)
		timezone = i_cal_timezone_get_builtin_timezone (location);

	if (!timezone)
		timezone = i_cal_timezone_get_utc_timezone ();

	g_value_set_object (value, timezone);

	g_object_unref (settings);

	return TRUE;
}

static void
alarm_notify_module_format_time_cb (EReminderWatcher *watcher,
				    const EReminderData *rd,
				    ICalTime *itt,
				    gchar **inout_buffer,
				    gint buffer_size)
{
	gchar *buffer;
	struct tm tm;

	g_return_if_fail (rd != NULL);
	g_return_if_fail (itt != NULL);
	g_return_if_fail (inout_buffer != NULL);
	g_return_if_fail (*inout_buffer != NULL);
	g_return_if_fail (buffer_size > 0);

	/* This is inlined cal_comp_util_format_itt(), to not bring
	   into alarm-notify almost all evolution libraries through
	   the calendar library dependency */
	buffer = *inout_buffer;
	buffer[0] = '\0';

	tm = e_cal_util_icaltime_to_tm (itt);
	e_datetime_format_format_tm_inline ("calendar", "table", i_cal_time_is_date (itt) ? DTFormatKindDate : DTFormatKindDateTime, &tm, buffer, buffer_size);
}

static gboolean
alarm_notify_module_row_activated_cb (ERemindersWidget *reminders,
				      const EReminderData *rd,
				      gpointer user_data)
{
	ECalComponent *comp;
	const gchar *scheme = NULL;
	const gchar *comp_uid = NULL;

	g_return_val_if_fail (E_IS_REMINDERS_WIDGET (reminders), FALSE);
	g_return_val_if_fail (rd != NULL, FALSE);

	comp = e_reminder_data_get_component (rd);
	comp_uid = e_cal_component_get_uid (comp);

	switch (e_cal_component_get_vtype (comp)) {
		case E_CAL_COMPONENT_EVENT:
			scheme = "calendar:";
			break;
		case E_CAL_COMPONENT_TODO:
			scheme = "task:";
			break;
		case E_CAL_COMPONENT_JOURNAL:
			scheme = "memo:";
			break;
		default:
			break;
	}

	if (scheme && comp_uid && e_reminder_data_get_source_uid (rd)) {
		GString *cmd;
		gchar *tmp;
		GError *error = NULL;

		cmd = g_string_sized_new (128);

		g_string_append (cmd, PACKAGE);
		g_string_append_c (cmd, ' ');
		g_string_append (cmd, scheme);
		g_string_append (cmd, "///?");

		tmp = g_uri_escape_string (e_reminder_data_get_source_uid (rd), NULL, TRUE);
		g_string_append (cmd, "source-uid=");
		g_string_append (cmd, tmp);
		g_free (tmp);

		g_string_append_c (cmd, '&');

		tmp = g_uri_escape_string (comp_uid, NULL, TRUE);
		g_string_append (cmd, "comp-uid=");
		g_string_append (cmd, tmp);
		g_free (tmp);

		if (!g_spawn_command_line_async (cmd->str, &error) &&
		    !g_error_matches (error, G_IO_ERROR, G_IO_ERROR_NOT_SUPPORTED)) {
			gchar *prefix = g_strdup_printf (_("Failed to launch command “%s”:"), cmd->str);
			e_reminders_widget_report_error (reminders, prefix, error);
			g_free (prefix);
		}

		g_string_free (cmd, TRUE);
		g_clear_error (&error);

		return TRUE;
	}

	return FALSE;
}

static void
alarm_notify_datetime_format_changed_cb (GFileMonitor *monitor,
					 GFile *file,
					 GFile *other_file,
					 GFileMonitorEvent event_type,
					 gpointer user_data)
{
	EAlarmNotifyModule *self = user_data;
	ERemindersWidget *reminders;
	EReminderWatcher *watcher;

	g_return_if_fail (E_IS_ALARM_NOTIFY_MODULE (self));

	e_datetime_format_free_memory ();

	reminders = E_REMINDERS_WIDGET (e_extension_get_extensible (E_EXTENSION (self)));
	watcher = e_reminders_widget_get_watcher (reminders);

	/* This causes the reminders widget to refresh its content, thus to use the new format */
	g_signal_emit_by_name (watcher, "changed", NULL);
}

static void
alarm_notify_module_constructed (GObject *object)
{
	EAlarmNotifyModule *self = E_ALARM_NOTIFY_MODULE (object);
	ERemindersWidget *reminders;
	EReminderWatcher *watcher;
	GSettings *settings;
	GFile *file;
	gchar *filename;

	/* Chain up to parent's method. */
	G_OBJECT_CLASS (e_alarm_notify_module_parent_class)->constructed (object);

	reminders = E_REMINDERS_WIDGET (e_extension_get_extensible (E_EXTENSION (object)));
	watcher = e_reminders_widget_get_watcher (reminders);

	settings = e_util_ref_settings ("org.gnome.evolution.calendar");

	g_settings_bind_with_mapping (
		settings, "timezone",
		watcher, "default-zone",
		G_SETTINGS_BIND_GET,
		alarm_notify_module_map_string_to_icaltimezone,
		NULL, /* one-way binding */
		NULL, NULL);

	g_object_unref (settings);

	g_signal_connect (watcher, "format-time",
		G_CALLBACK (alarm_notify_module_format_time_cb), object);

	g_signal_connect (reminders, "activated",
		G_CALLBACK (alarm_notify_module_row_activated_cb), object);

	filename = e_datetime_format_dup_config_filename ();
	file = g_file_new_for_path (filename);

	self->monitor = g_file_monitor_file (file, G_FILE_MONITOR_NONE, NULL, NULL);

	g_signal_connect (self->monitor, "changed",
		G_CALLBACK (alarm_notify_datetime_format_changed_cb), self);

	g_clear_object (&file);
	g_free (filename);
}

static void
alarm_notify_module_finalize (GObject *object)
{
	EAlarmNotifyModule *self = E_ALARM_NOTIFY_MODULE (object);

	g_clear_object (&self->monitor);

	/* Chain up to parent's method. */
	G_OBJECT_CLASS (e_alarm_notify_module_parent_class)->finalize (object);
}

static void
e_alarm_notify_module_class_init (EAlarmNotifyModuleClass *class)
{
	GObjectClass *object_class;
	EExtensionClass *extension_class;

	object_class = G_OBJECT_CLASS (class);
	object_class->constructed = alarm_notify_module_constructed;
	object_class->finalize = alarm_notify_module_finalize;

	extension_class = E_EXTENSION_CLASS (class);
	extension_class->extensible_type = E_TYPE_REMINDERS_WIDGET;
}

static void
e_alarm_notify_module_class_finalize (EAlarmNotifyModuleClass *class)
{
}

static void
e_alarm_notify_module_init (EAlarmNotifyModule *extension)
{
}

/* Module Entry Points */
void e_module_load (GTypeModule *type_module);
void e_module_unload (GTypeModule *type_module);

G_MODULE_EXPORT void
e_module_load (GTypeModule *type_module)
{
	e_alarm_notify_module_register_type (type_module);
}

G_MODULE_EXPORT void
e_module_unload (GTypeModule *type_module)
{
}
