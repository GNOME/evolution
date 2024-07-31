/*
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 * Copyright (C) 2014 Red Hat, Inc. (www.redhat.com)
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE. See the GNU Lesser General Public License
 * for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 */

#include "evolution-config.h"

#include <string.h>
#include <glib/gi18n.h>

#include <calendar/gui/e-calendar-view.h>
#include <calendar/gui/e-cal-ops.h>
#include <calendar/importers/evolution-calendar-importer.h>

#include "e-util/e-util-private.h"

#include "e-calendar-preferences.h"

#include "e-cal-base-shell-sidebar.h"
#include "e-cal-shell-content.h"
#include "e-cal-shell-view.h"
#include "e-cal-shell-backend.h"

struct _ECalShellBackendPrivate {
	gint placeholder;
};

G_DEFINE_DYNAMIC_TYPE_EXTENDED (ECalShellBackend, e_cal_shell_backend, E_TYPE_CAL_BASE_SHELL_BACKEND, 0,
	G_ADD_PRIVATE_DYNAMIC (ECalShellBackend))

static void
action_event_new_cb (EUIAction *action,
		     GVariant *parameter,
		     gpointer user_data)
{
	EShellWindow *shell_window = user_data;
	EShell *shell;
	EShellView *shell_view;
	EShellBackend *shell_backend;
	GSettings *settings;
	const gchar *action_name;
	gboolean is_all_day;
	gboolean is_meeting;

	shell = e_shell_window_get_shell (shell_window);

	action_name = g_action_get_name (G_ACTION (action));
	is_all_day = g_strcmp0 (action_name, "event-all-day-new") == 0 ||
		     g_strcmp0 (action_name, "new-menu-event-all-day-new") == 0;
	is_meeting = g_strcmp0 (action_name, "event-meeting-new") == 0 ||
		     g_strcmp0 (action_name, "new-menu-event-meeting-new") == 0;

	/* With a 'calendar' active shell view pass the new appointment
	 * request to it, thus the event will inherit selected time from
	 * the view. */
	shell_view = e_shell_window_peek_shell_view (shell_window, "calendar");
	if (shell_view != NULL) {
		EShellContent *shell_content;
		ECalendarView *view;

		shell_backend = e_shell_view_get_shell_backend (shell_view);
		shell_content = e_shell_view_get_shell_content (shell_view);

		e_shell_backend_set_prefer_new_item (shell_backend, action_name);

		/* This forces the shell window to update the "New" toolbar
		 * button menu, and the toolbar button will then update its
		 * button image to reflect the "preferred new item" we just
		 * set on the shell backend. */
		g_object_notify (G_OBJECT (shell_window), "active-view");

		view = e_cal_shell_content_get_current_calendar_view (E_CAL_SHELL_CONTENT (shell_content));
		if (view != NULL) {
			e_calendar_view_new_appointment (view, E_NEW_APPOINTMENT_FLAG_NO_PAST_DATE |
				(is_all_day ? E_NEW_APPOINTMENT_FLAG_ALL_DAY : 0) |
				(is_meeting ? E_NEW_APPOINTMENT_FLAG_MEETING : 0) |
				(e_shell_view_is_active (shell_view) ? 0 : E_NEW_APPOINTMENT_FLAG_FORCE_CURRENT_TIME));
			return;
		}
	}

	shell_backend = e_shell_get_backend_by_name (shell, "calendar");
	e_shell_backend_set_prefer_new_item (shell_backend, action_name);

	settings = e_util_ref_settings ("org.gnome.evolution.calendar");

	e_cal_ops_new_event_editor (shell_window, NULL, is_meeting, is_all_day,
		g_settings_get_boolean (settings, "use-default-reminder"),
		g_settings_get_int (settings, "default-reminder-interval"),
		g_settings_get_enum (settings, "default-reminder-units"),
		0, 0);

	g_clear_object (&settings);
}

static void
action_calendar_new_cb (EUIAction *action,
			GVariant *parameter,
			gpointer user_data)
{
	EShellWindow *shell_window = user_data;

	e_cal_base_shell_backend_util_new_source (shell_window, E_CAL_CLIENT_SOURCE_TYPE_EVENTS);
}

static void
cal_shell_backend_handle_uri_start_end_dates (EShellBackend *shell_backend,
					      const GDate *start_date,
					      const GDate *end_date)
{
	g_return_if_fail (E_IS_CAL_SHELL_BACKEND (shell_backend));
	g_return_if_fail (g_date_valid (start_date));

	if (g_date_valid (end_date))
		e_cal_shell_backend_open_date_range (E_CAL_SHELL_BACKEND (shell_backend), start_date, end_date);
	else
		e_cal_shell_backend_open_date_range (E_CAL_SHELL_BACKEND (shell_backend), start_date, start_date);
}

static gboolean
e_cal_shell_backend_handle_uri (EShellBackend *shell_backend,
				const gchar *uri)
{
	if (strncmp (uri, "calendar:", 9) != 0)
		return FALSE;

	return e_cal_base_shell_backend_util_handle_uri (shell_backend,
		E_CAL_CLIENT_SOURCE_TYPE_EVENTS, uri, cal_shell_backend_handle_uri_start_end_dates);
}

static void
cal_shell_backend_init_importers (void)
{
	EImportClass *import_class;
	EImportImporter *importer;

	import_class = g_type_class_ref (e_import_get_type ());

	importer = gnome_calendar_importer_peek ();
	e_import_class_add_importer (import_class, importer, NULL, NULL);

	importer = ical_importer_peek ();
	e_import_class_add_importer (import_class, importer, NULL, NULL);

	importer = vcal_importer_peek ();
	e_import_class_add_importer (import_class, importer, NULL, NULL);
}

static void
ensure_alarm_notify_is_running (void)
{
	const gchar *base_dir;
	gchar *filename;

	#ifdef G_OS_WIN32
	base_dir = EVOLUTION_BINDIR;
	#else
	base_dir = EVOLUTION_DATA_SERVER_PRIVLIBEXECDIR;
	#endif

	filename = g_build_filename (base_dir, "evolution-alarm-notify", NULL);

	if (g_file_test (filename, G_FILE_TEST_IS_EXECUTABLE)) {
		gchar *argv[2];
		GError *error = NULL;

		argv[0] = filename;
		argv[1] = NULL;

		g_spawn_async (
			base_dir, argv, NULL, 0, NULL, NULL, NULL, &error);

		if (error != NULL) {
			g_message (
				"Failed to start '%s': %s",
				filename, error->message);
			g_error_free (error);
		}
	}

	g_free (filename);
}

static void
cal_shell_backend_use_system_timezone_changed_cb (GSettings *settings,
                                                  const gchar *key)
{
	/* the '-1' is a trick to emit the change the first time */
	static gint old_value = -1;
	gboolean value;

	value = g_settings_get_boolean (settings, key);

	if ((value ? 1 : 0) != old_value) {
		old_value = value ? 1 : 0;

		/* GSettings Bindings rely on quarks */
		g_signal_emit_by_name (settings, "changed::timezone",
			g_quark_to_string (g_quark_from_string ("timezone")));
	}
}

static void
cal_shell_backend_constructed (GObject *object)
{
	EShellBackend *shell_backend;
	GtkWidget *preferences_window;
	GSettings *settings;

	shell_backend = E_SHELL_BACKEND (object);

	/* Setup preference widget factories */
	preferences_window = e_shell_get_preferences_window (e_shell_backend_get_shell (shell_backend));

	e_preferences_window_add_page (
		E_PREFERENCES_WINDOW (preferences_window),
		"calendar-and-tasks",
		"preferences-calendar-and-tasks",
		_("Calendar and Tasks"),
		"index#calendar",
		e_calendar_preferences_new,
		600);

	settings = e_util_ref_settings ("org.gnome.evolution.calendar");

	g_settings_bind (
		settings, "prefer-new-item",
		shell_backend, "prefer-new-item",
		G_SETTINGS_BIND_DEFAULT);

	/* Changing whether or not to use the system timezone may change
	 * Evolution's current timezone.  Need to emit "changed" signals
	 * for both keys. */
	g_signal_connect (
		settings, "changed::use-system-timezone",
		G_CALLBACK (cal_shell_backend_use_system_timezone_changed_cb),
		NULL);

	g_object_unref (settings);

	/* Chain up to parent's constructed() method. */
	G_OBJECT_CLASS (e_cal_shell_backend_parent_class)->constructed (object);

	cal_shell_backend_init_importers ();

	ensure_alarm_notify_is_running ();
}

static void
e_cal_shell_backend_class_init (ECalShellBackendClass *class)
{
	static const EUIActionEntry item_entries[] = {
		{ "new-menu-event-new",
		  "appointment-new",
		  NC_("New", "_Appointment"),
		  "<Shift><Control>a",
		  N_("Create a new appointment"),
		  action_event_new_cb, NULL, NULL, NULL },

		{ "new-menu-event-all-day-new",
		  "stock_new-24h-appointment",
		  NC_("New", "All Day A_ppointment"),
		  NULL,
		  N_("Create a new all-day appointment"),
		  action_event_new_cb, NULL, NULL, NULL },

		{ "new-menu-event-meeting-new",
		  "stock_people",
		  NC_("New", "M_eeting"),
		  "<Shift><Control>e",
		  N_("Create a new meeting request"),
		  action_event_new_cb, NULL, NULL, NULL }
	};

	static const EUIActionEntry source_entries[] = {
		{ "new-menu-calendar-new",
		  "x-office-calendar",
		  NC_("New", "Cale_ndar"),
		  NULL,
		  N_("Create a new calendar"),
		  action_calendar_new_cb, NULL, NULL, NULL }
	};

	GObjectClass *object_class;
	EShellBackendClass *shell_backend_class;
	ECalBaseShellBackendClass *cal_base_shell_backend_class;

	object_class = G_OBJECT_CLASS (class);
	object_class->constructed = cal_shell_backend_constructed;

	shell_backend_class = E_SHELL_BACKEND_CLASS (class);
	shell_backend_class->shell_view_type = E_TYPE_CAL_SHELL_VIEW;
	shell_backend_class->name = "calendar";
	shell_backend_class->aliases = "";
	shell_backend_class->schemes = "calendar";
	shell_backend_class->sort_order = 400;
	shell_backend_class->preferences_page = "calendar-and-tasks";
	shell_backend_class->start = NULL;

	cal_base_shell_backend_class = E_CAL_BASE_SHELL_BACKEND_CLASS (class);
	cal_base_shell_backend_class->new_item_entries = item_entries;
	cal_base_shell_backend_class->new_item_n_entries = G_N_ELEMENTS (item_entries);
	cal_base_shell_backend_class->source_entries = source_entries;
	cal_base_shell_backend_class->source_n_entries = G_N_ELEMENTS (source_entries);
	cal_base_shell_backend_class->handle_uri = e_cal_shell_backend_handle_uri;
}

static void
e_cal_shell_backend_init (ECalShellBackend *cal_shell_backend)
{
	cal_shell_backend->priv = e_cal_shell_backend_get_instance_private (cal_shell_backend);
}

static void
e_cal_shell_backend_class_finalize (ECalShellBackendClass *class)
{
}

void
e_cal_shell_backend_type_register (GTypeModule *type_module)
{
	/* XXX G_DEFINE_DYNAMIC_TYPE declares a static type registration
	 *     function, so we have to wrap it with a public function in
	 *     order to register types from a separate compilation unit. */
	e_cal_shell_backend_register_type (type_module);
}

void
e_cal_shell_backend_open_date_range (ECalShellBackend *cal_shell_backend,
                                     const GDate *start_date,
                                     const GDate *end_date)
{
	EShell *shell;
	EShellView *shell_view;
	EShellBackend *shell_backend;
	EShellSidebar *shell_sidebar;
	GtkWidget *shell_window = NULL;
	GtkApplication *application;
	ECalendar *calendar;
	GList *list;

	g_return_if_fail (E_IS_CAL_SHELL_BACKEND (cal_shell_backend));

	shell_backend = E_SHELL_BACKEND (cal_shell_backend);
	shell = e_shell_backend_get_shell (shell_backend);

	application = GTK_APPLICATION (shell);
	list = gtk_application_get_windows (application);

	/* Try to find an EShellWindow already in calendar view. */
	while (list != NULL) {
		GtkWidget *window = GTK_WIDGET (list->data);

		if (E_IS_SHELL_WINDOW (window)) {
			const gchar *active_view;

			active_view = e_shell_window_get_active_view (E_SHELL_WINDOW (window));
			if (g_strcmp0 (active_view, "calendar") == 0) {
				gtk_window_present (GTK_WINDOW (window));
				shell_window = window;
				break;
			}
		}

		list = g_list_next (list);
	}

	/* Otherwise create a new EShellWindow in calendar view. */
	if (shell_window == NULL)
		shell_window = e_shell_create_shell_window (shell, "calendar");

	/* Now dig up the date navigator and select the date range. */

	shell_view = e_shell_window_get_shell_view (E_SHELL_WINDOW (shell_window), "calendar");
	shell_sidebar = e_shell_view_get_shell_sidebar (shell_view);
	calendar = e_cal_base_shell_sidebar_get_date_navigator (E_CAL_BASE_SHELL_SIDEBAR (shell_sidebar));

	e_calendar_item_set_selection (e_calendar_get_item (calendar), start_date, end_date);
}
