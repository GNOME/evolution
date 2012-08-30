/*
 * e-cal-shell-backend.c
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

#include "e-cal-shell-backend.h"

#include <string.h>
#include <glib/gi18n.h>
#include <libecal/libecal.h>
#include <libedataserverui/libedataserverui.h>

#include "e-util/e-import.h"
#include "shell/e-shell.h"
#include "shell/e-shell-backend.h"
#include "shell/e-shell-window.h"
#include "widgets/misc/e-cal-source-config.h"
#include "widgets/misc/e-preferences-window.h"
#include "widgets/misc/e-source-config-dialog.h"

#include "calendar/gui/comp-util.h"
#include "calendar/gui/dialogs/event-editor.h"
#include "calendar/gui/e-calendar-view.h"
#include "calendar/gui/gnome-cal.h"
#include "calendar/importers/evolution-calendar-importer.h"

#include "e-cal-shell-content.h"
#include "e-cal-shell-migrate.h"
#include "e-cal-shell-settings.h"
#include "e-cal-shell-sidebar.h"
#include "e-cal-shell-view.h"

#include "e-calendar-preferences.h"

#define E_CAL_SHELL_BACKEND_GET_PRIVATE(obj) \
	(G_TYPE_INSTANCE_GET_PRIVATE \
	((obj), E_TYPE_CAL_SHELL_BACKEND, ECalShellBackendPrivate))

struct _ECalShellBackendPrivate {
	gint placeholder;
};

G_DEFINE_DYNAMIC_TYPE (
	ECalShellBackend,
	e_cal_shell_backend,
	E_TYPE_SHELL_BACKEND)

static void
cal_shell_backend_new_event (ESource *source,
                             GAsyncResult *result,
                             EShell *shell,
                             CompEditorFlags flags,
                             gboolean all_day)
{
	EClient *client = NULL;
	ECalClient *cal_client;
	ECalComponent *comp;
	EShellSettings *shell_settings;
	CompEditor *editor;
	GError *error = NULL;

	/* XXX Handle errors better. */
	e_client_utils_open_new_finish (source, result, &client, &error);

	if (error != NULL) {
		g_warn_if_fail (client == NULL);
		g_warning (
			"%s: Failed to open '%s': %s",
			G_STRFUNC, e_source_get_display_name (source),
			error->message);
		g_error_free (error);
		return;
	}

	g_return_if_fail (E_IS_CAL_CLIENT (client));

	cal_client = E_CAL_CLIENT (client);
	shell_settings = e_shell_get_shell_settings (shell);

	editor = event_editor_new (cal_client, shell, flags);
	comp = cal_comp_event_new_with_current_time (
		cal_client, all_day,
		e_shell_settings_get_pointer (
			shell_settings, "cal-timezone"),
		e_shell_settings_get_boolean (
			shell_settings, "cal-use-default-reminder"),
		e_shell_settings_get_int (
			shell_settings, "cal-default-reminder-interval"),
		e_shell_settings_get_int (  /* enum, actually */
			shell_settings, "cal-default-reminder-units"));
	e_cal_component_commit_sequence (comp);
	comp_editor_edit_comp (editor, comp);

	gtk_window_present (GTK_WINDOW (editor));

	g_object_unref (comp);
	g_object_unref (client);
}

static void
cal_shell_backend_event_new_cb (GObject *source_object,
                                GAsyncResult *result,
                                gpointer shell)
{
	CompEditorFlags flags = 0;
	gboolean all_day = FALSE;

	flags |= COMP_EDITOR_NEW_ITEM;
	flags |= COMP_EDITOR_USER_ORG;

	cal_shell_backend_new_event (
		E_SOURCE (source_object), result, shell, flags, all_day);

	g_object_unref (shell);
}

static void
cal_shell_backend_event_all_day_new_cb (GObject *source_object,
                                        GAsyncResult *result,
                                        gpointer shell)
{
	CompEditorFlags flags = 0;
	gboolean all_day = TRUE;

	flags |= COMP_EDITOR_NEW_ITEM;
	flags |= COMP_EDITOR_USER_ORG;

	cal_shell_backend_new_event (
		E_SOURCE (source_object), result, shell, flags, all_day);

	g_object_unref (shell);
}

static void
cal_shell_backend_event_meeting_new_cb (GObject *source_object,
                                        GAsyncResult *result,
                                        gpointer shell)
{
	CompEditorFlags flags = 0;
	gboolean all_day = FALSE;

	flags |= COMP_EDITOR_NEW_ITEM;
	flags |= COMP_EDITOR_USER_ORG;
	flags |= COMP_EDITOR_MEETING;

	cal_shell_backend_new_event (
		E_SOURCE (source_object), result, shell, flags, all_day);

	g_object_unref (shell);
}

static void
action_event_new_cb (GtkAction *action,
                     EShellWindow *shell_window)
{
	EShell *shell;
	EShellView *shell_view;
	EShellBackend *shell_backend;
	ESource *source;
	ESourceRegistry *registry;
	EClientSourceType source_type;
	const gchar *action_name;

	shell = e_shell_window_get_shell (shell_window);

	action_name = gtk_action_get_name (action);

	/* With a 'calendar' active shell view pass the new appointment
	 * request to it, thus the event will inherit selected time from
	 * the view. */
	shell_view = e_shell_window_peek_shell_view (shell_window, "calendar");
	if (shell_view != NULL) {
		EShellContent *shell_content;
		GnomeCalendar *gcal;
		GnomeCalendarViewType view_type;
		ECalendarView *view;

		shell_backend = e_shell_view_get_shell_backend (shell_view);
		shell_content = e_shell_view_get_shell_content (shell_view);

		gcal = e_cal_shell_content_get_calendar (
			E_CAL_SHELL_CONTENT (shell_content));

		view_type = gnome_calendar_get_view (gcal);
		view = gnome_calendar_get_calendar_view (gcal, view_type);

		if (view) {
			g_object_set (
				G_OBJECT (shell_backend),
				"prefer-new-item", action_name, NULL);

			e_calendar_view_new_appointment_full (
				view,
				g_str_equal (action_name, "event-all-day-new"),
				g_str_equal (action_name, "event-meeting-new"),
				TRUE);

			return;
		}
	}

	/* This callback is used for both appointments and meetings. */

	source_type = E_CLIENT_SOURCE_TYPE_EVENTS;

	registry = e_shell_get_registry (shell);
	source = e_source_registry_ref_default_calendar (registry);

	shell_backend = e_shell_get_backend_by_name (shell, "calendar");
	g_object_set (G_OBJECT (shell_backend), "prefer-new-item", action_name, NULL);

	/* Use a callback function appropriate for the action.
	 * FIXME Need to obtain a better default time zone. */
	if (strcmp (action_name, "event-all-day-new") == 0)
		e_client_utils_open_new (
			source, source_type, FALSE, NULL,
			cal_shell_backend_event_all_day_new_cb,
			g_object_ref (shell));
	else if (strcmp (action_name, "event-meeting-new") == 0)
		e_client_utils_open_new (
			source, source_type, FALSE, NULL,
			cal_shell_backend_event_meeting_new_cb,
			g_object_ref (shell));
	else
		e_client_utils_open_new (
			source, source_type, FALSE, NULL,
			cal_shell_backend_event_new_cb,
			g_object_ref (shell));

	g_object_unref (source);
}

static void
action_calendar_new_cb (GtkAction *action,
                        EShellWindow *shell_window)
{
	EShell *shell;
	ESourceRegistry *registry;
	ECalClientSourceType source_type;
	GtkWidget *config;
	GtkWidget *dialog;
	const gchar *icon_name;

	shell = e_shell_window_get_shell (shell_window);

	registry = e_shell_get_registry (shell);
	source_type = E_CAL_CLIENT_SOURCE_TYPE_EVENTS;
	config = e_cal_source_config_new (registry, NULL, source_type);

	dialog = e_source_config_dialog_new (E_SOURCE_CONFIG (config));

	gtk_window_set_transient_for (
		GTK_WINDOW (dialog), GTK_WINDOW (shell_window));

	icon_name = gtk_action_get_icon_name (action);
	gtk_window_set_icon_name (GTK_WINDOW (dialog), icon_name);

	gtk_window_set_title (GTK_WINDOW (dialog), _("New Calendar"));

	gtk_widget_show (dialog);
}

static GtkActionEntry item_entries[] = {

	{ "event-new",
	  "appointment-new",
	  NC_("New", "_Appointment"),
	  "<Shift><Control>a",
	  N_("Create a new appointment"),
	  G_CALLBACK (action_event_new_cb) },

	{ "event-all-day-new",
	  "stock_new-24h-appointment",
	  NC_("New", "All Day A_ppointment"),
	  NULL,
	  N_("Create a new all-day appointment"),
	  G_CALLBACK (action_event_new_cb) },

	{ "event-meeting-new",
	  "stock_new-meeting",
	  NC_("New", "M_eeting"),
	  "<Shift><Control>e",
	  N_("Create a new meeting request"),
	  G_CALLBACK (action_event_new_cb) }
};

static GtkActionEntry source_entries[] = {

	{ "calendar-new",
	  "x-office-calendar",
	  NC_("New", "Cale_ndar"),
	  NULL,
	  N_("Create a new calendar"),
	  G_CALLBACK (action_calendar_new_cb) }
};

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

static time_t
utc_to_user_zone (time_t utc_time,
                  icaltimezone *zone)
{
	if (!zone || (gint) utc_time == -1)
		return utc_time;

	return icaltime_as_timet (
		icaltime_from_timet_with_zone (utc_time, FALSE, zone));
}

static gboolean
cal_shell_backend_handle_uri_cb (EShellBackend *shell_backend,
                                 const gchar *uri)
{
	EShell *shell;
	EShellSettings *shell_settings;
	CompEditor *editor;
	CompEditorFlags flags = 0;
	ECalClient *client;
	ECalComponent *comp;
	ESource *source;
	ESourceRegistry *registry;
	ECalClientSourceType source_type;
	SoupURI *soup_uri;
	icalcomponent *icalcomp;
	icalproperty *icalprop;
	const gchar *cp;
	gchar *source_uid = NULL;
	gchar *comp_uid = NULL;
	gchar *comp_rid = NULL;
	GDate start_date;
	GDate end_date;
	icaltimezone *zone;
	gboolean handled = FALSE;
	GError *error = NULL;

	source_type = E_CAL_CLIENT_SOURCE_TYPE_EVENTS;
	shell = e_shell_backend_get_shell (shell_backend);
	shell_settings = e_shell_get_shell_settings (shell);

	zone = e_shell_settings_get_pointer (shell_settings, "cal-timezone");

	if (strncmp (uri, "calendar:", 9) != 0)
		return FALSE;

	soup_uri = soup_uri_new (uri);

	if (soup_uri == NULL)
		return FALSE;

	cp = soup_uri_get_query (soup_uri);
	if (cp == NULL)
		goto exit;

	g_date_clear (&start_date, 1);
	g_date_clear (&end_date, 1);

	while (*cp != '\0') {
		gchar *header;
		gchar *content;
		gsize header_len;
		gsize content_len;

		header_len = strcspn (cp, "=&");

		/* It it's malformed, give up. */
		if (cp[header_len] != '=')
			break;

		header = (gchar *) cp;
		header[header_len] = '\0';
		cp += header_len + 1;

		content_len = strcspn (cp, "&");

		content = g_strndup (cp, content_len);
		if (g_ascii_strcasecmp (header, "startdate") == 0)
			g_date_set_time_t (
				&start_date, utc_to_user_zone (
				time_from_isodate (content), zone));
		else if (g_ascii_strcasecmp (header, "enddate") == 0)
			g_date_set_time_t (
				&end_date, utc_to_user_zone (
				time_from_isodate (content), zone));
		else if (g_ascii_strcasecmp (header, "source-uid") == 0)
			source_uid = g_strdup (content);
		else if (g_ascii_strcasecmp (header, "comp-uid") == 0)
			comp_uid = g_strdup (content);
		else if (g_ascii_strcasecmp (header, "comp-rid") == 0)
			comp_rid = g_strdup (content);
		g_free (content);

		cp += content_len;
		if (*cp == '&') {
			cp++;
			if (strncmp (cp, "amp;", 4) == 0)
				cp += 4;
		}
	}

	/* This is primarily for launching Evolution
	 * from the calendar in the clock applet. */
	if (g_date_valid (&start_date)) {
		if (g_date_valid (&end_date))
			e_cal_shell_backend_open_date_range (
				E_CAL_SHELL_BACKEND (shell_backend),
				&start_date, &end_date);
		else
			e_cal_shell_backend_open_date_range (
				E_CAL_SHELL_BACKEND (shell_backend),
				&start_date, NULL);
		handled = TRUE;
		goto exit;
	}

	if (source_uid == NULL || comp_uid == NULL)
		goto exit;

	/* URI is valid, so consider it handled.  Whether
	 * we successfully open it is another matter... */
	handled = TRUE;

	registry = e_shell_get_registry (shell);
	source = e_source_registry_ref_source (registry, source_uid);
	if (source == NULL) {
		g_printerr ("No source for UID '%s'\n", source_uid);
		goto exit;
	}

	client = e_cal_client_new (source, source_type, &error);

	if (client != NULL)
		e_client_open_sync (E_CLIENT (client), TRUE, NULL, &error);

	if (error != NULL) {
		g_warning (
			"%s: Failed to create/open client '%s': %s",
			G_STRFUNC, e_source_get_display_name (source),
			error->message);
		if (client != NULL)
			g_object_unref (client);
		g_object_unref (source);
		g_error_free (error);
		goto exit;
	}

	g_object_unref (source);
	source = NULL;

	/* XXX Copied from e_cal_shell_view_open_event().
	 *     Clearly a new utility function is needed. */

	editor = comp_editor_find_instance (comp_uid);

	if (editor != NULL)
		goto present;

	e_cal_client_get_object_sync (
		client, comp_uid, comp_rid, &icalcomp, NULL, &error);

	if (error != NULL) {
		g_warning (
			"%s: Failed to get object from client: %s",
			G_STRFUNC, error->message);
		g_object_unref (client);
		g_error_free (error);
		goto exit;
	}

	comp = e_cal_component_new ();
	if (!e_cal_component_set_icalcomponent (comp, icalcomp)) {
		g_warning ("%s: Failed to set icalcomp to comp\n", G_STRFUNC);
		icalcomponent_free (icalcomp);
		icalcomp = NULL;
	}

	icalprop = icalcomp ? icalcomponent_get_first_property (
		icalcomp, ICAL_ATTENDEE_PROPERTY) : NULL;
	if (icalprop != NULL)
		flags |= COMP_EDITOR_MEETING;

	if (itip_organizer_is_user (registry, comp, client))
		flags |= COMP_EDITOR_USER_ORG;

	if (itip_sentby_is_user (registry, comp, client))
		flags |= COMP_EDITOR_USER_ORG;

	if (!e_cal_component_has_attendees (comp))
		flags |= COMP_EDITOR_USER_ORG;

	editor = event_editor_new (client, shell, flags);
	comp_editor_edit_comp (editor, comp);

	g_object_unref (comp);

present:
	gtk_window_present (GTK_WINDOW (editor));

	g_object_unref (client);

exit:
	g_free (source_uid);
	g_free (comp_uid);
	g_free (comp_rid);

	soup_uri_free (soup_uri);

	return handled;
}

static void
cal_shell_backend_window_added_cb (EShellBackend *shell_backend,
                                   GtkWindow *window)
{
	const gchar *backend_name;

	if (!E_IS_SHELL_WINDOW (window))
		return;

	backend_name = E_SHELL_BACKEND_GET_CLASS (shell_backend)->name;

	e_shell_window_register_new_item_actions (
		E_SHELL_WINDOW (window), backend_name,
		item_entries, G_N_ELEMENTS (item_entries));

	e_shell_window_register_new_source_actions (
		E_SHELL_WINDOW (window), backend_name,
		source_entries, G_N_ELEMENTS (source_entries));
}

static void
ensure_alarm_notify_is_running (void)
{
	const gchar *base_dir;
	gchar *filename;

	#ifdef G_OS_WIN32
	base_dir = EVOLUTION_BINDIR;
	#else
	base_dir = EVOLUTION_PRIVLIBEXECDIR;
	#endif

	filename = g_build_filename (base_dir, "evolution-alarm-notify", NULL);

	if (g_file_test (filename, G_FILE_TEST_EXISTS | G_FILE_TEST_IS_EXECUTABLE)) {
		gchar *argv[2];
		GError *error = NULL;

		argv[0] = filename;
		argv[1] = NULL;

		if (!g_spawn_async (base_dir, argv, NULL, 0, NULL, NULL, NULL, &error))
			g_message ("Failed to start '%s': %s", filename, error ? error->message : "Unknown error");

		g_clear_error (&error);
	}

	g_free (filename);
}

static void
cal_shell_backend_constructed (GObject *object)
{
	EShell *shell;
	EShellBackend *shell_backend;
	GtkWidget *preferences_window;

	shell_backend = E_SHELL_BACKEND (object);
	shell = e_shell_backend_get_shell (shell_backend);

	g_signal_connect_swapped (
		shell, "handle-uri",
		G_CALLBACK (cal_shell_backend_handle_uri_cb),
		shell_backend);

	g_signal_connect_swapped (
		shell, "window-added",
		G_CALLBACK (cal_shell_backend_window_added_cb),
		shell_backend);

	cal_shell_backend_init_importers ();

	e_cal_shell_backend_init_settings (shell);

	/* Setup preference widget factories */
	preferences_window = e_shell_get_preferences_window (shell);

	e_preferences_window_add_page (
		E_PREFERENCES_WINDOW (preferences_window),
		"calendar-and-tasks",
		"preferences-calendar-and-tasks",
		_("Calendar and Tasks"),
		"index#calendar",
		e_calendar_preferences_new,
		600);

	g_object_bind_property (
		e_shell_get_shell_settings (shell), "cal-prefer-new-item",
		shell_backend, "prefer-new-item",
		G_BINDING_BIDIRECTIONAL |
		G_BINDING_SYNC_CREATE);

	/* Chain up to parent's constructed() method. */
	G_OBJECT_CLASS (e_cal_shell_backend_parent_class)->constructed (object);

	ensure_alarm_notify_is_running ();
}

static void
e_cal_shell_backend_class_init (ECalShellBackendClass *class)
{
	GObjectClass *object_class;
	EShellBackendClass *shell_backend_class;

	g_type_class_add_private (class, sizeof (ECalShellBackendPrivate));

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
	shell_backend_class->migrate = e_cal_shell_backend_migrate;

	/* Register relevant ESource extensions. */
	E_TYPE_SOURCE_CALENDAR;
}

static void
e_cal_shell_backend_class_finalize (ECalShellBackendClass *class)
{
}

static void
e_cal_shell_backend_init (ECalShellBackend *cal_shell_backend)
{
	icalarray *builtin_timezones;
	gint ii;

	cal_shell_backend->priv =
		E_CAL_SHELL_BACKEND_GET_PRIVATE (cal_shell_backend);

	/* XXX Pre-load all built-in timezones in libical.
	 *
	 *     Built-in time zones in libical 0.43 are loaded on demand,
	 *     but not in a thread-safe manner, resulting in a race when
	 *     multiple threads call icaltimezone_load_builtin_timezone()
	 *     on the same time zone.  Until built-in time zone loading
	 *     in libical is made thread-safe, work around the issue by
	 *     loading all built-in time zones now, so libical's internal
	 *     time zone array will be fully populated before any threads
	 *     are spawned.
	 */
	builtin_timezones = icaltimezone_get_builtin_timezones ();
	for (ii = 0; ii < builtin_timezones->num_elements; ii++) {
		icaltimezone *zone;

		zone = icalarray_element_at (builtin_timezones, ii);

		/* We don't care about the component right now,
		 * we just need some function that will trigger
		 * icaltimezone_load_builtin_timezone(). */
		icaltimezone_get_component (zone);
	}
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
	ECalendar *navigator;
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

			active_view = e_shell_window_get_active_view (
				E_SHELL_WINDOW (window));
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

	shell_view = e_shell_window_get_shell_view (
		E_SHELL_WINDOW (shell_window), "calendar");
	shell_sidebar = e_shell_view_get_shell_sidebar (shell_view);
	navigator = e_cal_shell_sidebar_get_date_navigator (
		E_CAL_SHELL_SIDEBAR (shell_sidebar));

	e_calendar_item_set_selection (
		navigator->calitem, start_date, end_date);
}
