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

#include "e-cal-shell-backend.h"

#include <string.h>
#include <glib/gi18n.h>
#include <libecal/e-cal.h>
#include <libecal/e-cal-time-util.h>
#include <libedataserver/e-url.h>
#include <libedataserver/e-source.h>
#include <libedataserver/e-source-group.h>

#include "e-util/e-import.h"
#include "shell/e-shell.h"
#include "shell/e-shell-backend.h"
#include "shell/e-shell-window.h"
#include "widgets/misc/e-preferences-window.h"

#include "calendar/common/authentication.h"
#include "calendar/gui/calendar-config.h"
#include "calendar/gui/comp-util.h"
#include "calendar/gui/dialogs/cal-prefs-dialog.h"
#include "calendar/gui/dialogs/calendar-setup.h"
#include "calendar/gui/dialogs/event-editor.h"
#include "calendar/gui/e-calendar-view.h"
#include "calendar/gui/gnome-cal.h"
#include "calendar/importers/evolution-calendar-importer.h"

#include "e-cal-shell-content.h"
#include "e-cal-shell-migrate.h"
#include "e-cal-shell-settings.h"
#include "e-cal-shell-sidebar.h"
#include "e-cal-shell-view.h"

#define E_CAL_SHELL_BACKEND_GET_PRIVATE(obj) \
	(G_TYPE_INSTANCE_GET_PRIVATE \
	((obj), E_TYPE_CAL_SHELL_BACKEND, ECalShellBackendPrivate))

struct _ECalShellBackendPrivate {
	ESourceList *source_list;
};

enum {
	PROP_0,
	PROP_SOURCE_LIST
};

static gpointer parent_class;
static GType cal_shell_backend_type;

static void
cal_shell_backend_ensure_sources (EShellBackend *shell_backend)
{
	/* XXX This is basically the same algorithm across all backends.
	 *     Maybe we could somehow integrate this into EShellBackend? */

	ECalShellBackendPrivate *priv;
	ESourceGroup *on_this_computer;
	ESourceGroup *contacts;
	ESource *birthdays;
	ESource *personal;
	EShell *shell;
	EShellSettings *shell_settings;
	GSList *sources, *iter;
	const gchar *name;
	gchar *property;
	gboolean save_list = FALSE;

	birthdays = NULL;
	personal = NULL;

	priv = E_CAL_SHELL_BACKEND_GET_PRIVATE (shell_backend);

	shell = e_shell_backend_get_shell (shell_backend);
	shell_settings = e_shell_get_shell_settings (shell);

	if (!e_cal_get_sources (&priv->source_list, E_CAL_SOURCE_TYPE_EVENT, NULL)) {
		g_warning ("Could not get calendar sources from GConf!");
		return;
	}

	on_this_computer = e_source_list_ensure_group (
		priv->source_list, _("On This Computer"), "local:", TRUE);
	contacts = e_source_list_ensure_group (
		priv->source_list, _("Contacts"), "contacts://", TRUE);
	e_source_list_ensure_group (
		priv->source_list, _("On The Web"), "webcal://", FALSE);
	e_source_list_ensure_group (
		priv->source_list, _("Weather"), "weather://", FALSE);

	g_return_if_fail (on_this_computer != NULL);
	g_return_if_fail (contacts != NULL);

	sources = e_source_group_peek_sources (on_this_computer);

	/* Make sure this group includes a "Personal" source. */
	for (iter = sources; iter != NULL; iter = iter->next) {
		ESource *source = iter->data;
		const gchar *relative_uri;

		relative_uri = e_source_peek_relative_uri (source);
		if (g_strcmp0 (relative_uri, "system") == 0) {
			personal = source;
			break;
		}
	}

	name = _("Personal");

	if (personal == NULL) {
		ESource *source;
		GSList *selected;
		gchar *primary;

		source = e_source_new (name, "system");
		e_source_set_color_spec (source, "#BECEDD");
		e_source_group_add_source (on_this_computer, source, -1);
		g_object_unref (source);
		save_list = TRUE;

		primary = e_shell_settings_get_string (
			shell_settings, "cal-primary-calendar");

		selected = calendar_config_get_calendars_selected ();

		if (primary == NULL && selected == NULL) {
			const gchar *uid;

			uid = e_source_peek_uid (source);
			selected = g_slist_prepend (NULL, g_strdup (uid));

			e_shell_settings_set_string (
				shell_settings, "cal-primary-calendar", uid);
			calendar_config_set_calendars_selected (selected);
		}

		g_slist_foreach (selected, (GFunc) g_free, NULL);
		g_slist_free (selected);
		g_free (primary);
	} else {
		/* Force the source name to the current locale. */
		e_source_set_name (personal, name);
	}

	sources = e_source_group_peek_sources (contacts);

	if (sources != NULL) {
		GSList *trash;

		/* There is only one source under Contacts. */
		birthdays = E_SOURCE (sources->data);
		sources = g_slist_next (sources);

		/* Delete any other sources in this group.
		 * Earlier versions allowed you to create
		 * additional sources under Contacts. */
		trash = g_slist_copy (sources);
		while (trash != NULL) {
			ESource *source = trash->data;
			e_source_group_remove_source (contacts, source);
			trash = g_slist_delete_link (trash, trash);
			save_list = TRUE;
		}
	}

	/* XXX e_source_group_get_property() returns a newly-allocated
	 *     string when it could just as easily return a const string.
	 *     Unfortunately, fixing that would break the API. */
	property = e_source_group_get_property (contacts, "create_source");
	if (property == NULL)
		e_source_group_set_property (contacts, "create_source", "no");
	g_free (property);

	name = _("Birthdays & Anniversaries");

	if (birthdays == NULL) {
		ESource *source;

		source = e_source_new (name, "/");
		e_source_group_add_source (contacts, source, -1);
		g_object_unref (source);
		save_list = TRUE;

		/* This is now a borrowed reference. */
		birthdays = source;
	} else {
		/* Force the source name to the current locale. */
		e_source_set_name (birthdays, name);
	}

	if (e_source_get_property (birthdays, "delete") == NULL)
		e_source_set_property (birthdays, "delete", "no");

	if (e_source_peek_color_spec (birthdays) == NULL)
		e_source_set_color_spec (birthdays, "#DDBECE");

	g_object_unref (on_this_computer);
	g_object_unref (contacts);

	if (save_list)
		e_source_list_sync (priv->source_list, NULL);
}

static void
cal_shell_backend_new_event (ESource *source,
                             GAsyncResult *result,
                             EShell *shell,
                             CompEditorFlags flags,
                             gboolean all_day)
{
	ECal *cal;
	ECalComponent *comp;
	CompEditor *editor;

	/* XXX Handle errors better. */
	cal = e_load_cal_source_finish (source, result, NULL);
	g_return_if_fail (E_IS_CAL (cal));

	editor = event_editor_new (cal, shell, flags);
	comp = cal_comp_event_new_with_current_time (cal, all_day);
	e_cal_component_commit_sequence (comp);
	comp_editor_edit_comp (editor, comp);

	gtk_window_present (GTK_WINDOW (editor));

	g_object_unref (comp);
	g_object_unref (cal);
}

static void
cal_shell_backend_event_new_cb (ESource *source,
                                GAsyncResult *result,
                                EShell *shell)
{
	CompEditorFlags flags = 0;
	gboolean all_day = FALSE;

	flags |= COMP_EDITOR_NEW_ITEM;
	flags |= COMP_EDITOR_USER_ORG;

	cal_shell_backend_new_event (source, result, shell, flags, all_day);

	g_object_unref (shell);
}

static void
cal_shell_backend_event_all_day_new_cb (ESource *source,
                                        GAsyncResult *result,
                                        EShell *shell)
{
	CompEditorFlags flags = 0;
	gboolean all_day = TRUE;

	flags |= COMP_EDITOR_NEW_ITEM;
	flags |= COMP_EDITOR_USER_ORG;

	cal_shell_backend_new_event (source, result, shell, flags, all_day);

	g_object_unref (shell);
}

static void
cal_shell_backend_event_meeting_new_cb (ESource *source,
                                        GAsyncResult *result,
                                        EShell *shell)
{
	CompEditorFlags flags = 0;
	gboolean all_day = FALSE;

	flags |= COMP_EDITOR_NEW_ITEM;
	flags |= COMP_EDITOR_USER_ORG;
	flags |= COMP_EDITOR_MEETING;

	cal_shell_backend_new_event (source, result, shell, flags, all_day);

	g_object_unref (shell);
}

static void
action_event_new_cb (GtkAction *action,
                     EShellWindow *shell_window)
{
	EShell *shell;
	EShellView *shell_view;
	EShellBackend *shell_backend;
	EShellSettings *shell_settings;
	ESource *source = NULL;
	ESourceList *source_list;
	ECalSourceType source_type;
	const gchar *action_name;
	gchar *uid;

	/* With a 'calendar' active shell view pass the new appointment
	 * request to it, thus the event will inherit selected time from
	 * the view. */
	shell_view = e_shell_window_peek_shell_view (shell_window, "calendar");
	if (shell_view != NULL) {
		EShellContent *shell_content;
		GnomeCalendar *gcal;
		GnomeCalendarViewType view_type;
		ECalendarView *view;

		shell_content = e_shell_view_get_shell_content (shell_view);

		gcal = e_cal_shell_content_get_calendar (
			E_CAL_SHELL_CONTENT (shell_content));

		view_type = gnome_calendar_get_view (gcal);
		view = gnome_calendar_get_calendar_view (gcal, view_type);

		if (view) {
			action_name = gtk_action_get_name (action);

			e_calendar_view_new_appointment_full (
				view,
				g_str_equal (action_name, "event-all-day-new"),
				g_str_equal (action_name, "event-meeting-new"),
				TRUE);

			return;
		}
	}

	/* This callback is used for both appointments and meetings. */

	source_type = E_CAL_SOURCE_TYPE_EVENT;

	shell = e_shell_window_get_shell (shell_window);
	shell_settings = e_shell_get_shell_settings (shell);
	shell_backend = e_shell_get_backend_by_name (shell, "calendar");

	g_object_get (shell_backend, "source-list", &source_list, NULL);
	g_return_if_fail (E_IS_SOURCE_LIST (source_list));

	uid = e_shell_settings_get_string (
		shell_settings, "cal-primary-calendar");

	if (uid != NULL) {
		source = e_source_list_peek_source_by_uid (source_list, uid);
		g_free (uid);
	}

	if (source == NULL)
		source = e_source_list_peek_default_source (source_list);

	g_return_if_fail (E_IS_SOURCE (source));

	/* Use a callback function appropriate for the action.
	 * FIXME Need to obtain a better default time zone. */
	action_name = gtk_action_get_name (action);
	if (strcmp (action_name, "event-all-day-new") == 0)
		e_load_cal_source_async (
			source, source_type, NULL,
			GTK_WINDOW (shell_window),
			NULL, (GAsyncReadyCallback)
			cal_shell_backend_event_all_day_new_cb,
			g_object_ref (shell));
	else if (strcmp (action_name, "event-meeting-new") == 0)
		e_load_cal_source_async (
			source, source_type, NULL,
			GTK_WINDOW (shell_window),
			NULL, (GAsyncReadyCallback)
			cal_shell_backend_event_meeting_new_cb,
			g_object_ref (shell));
	else
		e_load_cal_source_async (
			source, source_type, NULL,
			GTK_WINDOW (shell_window),
			NULL, (GAsyncReadyCallback)
			cal_shell_backend_event_new_cb,
			g_object_ref (shell));

	g_object_unref (source_list);
}

static void
action_calendar_new_cb (GtkAction *action,
                        EShellWindow *shell_window)
{
	calendar_setup_new_calendar (GTK_WINDOW (shell_window));
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
utc_to_user_zone (time_t utc_time)
{
	icaltimezone *zone = calendar_config_get_icaltimezone ();

	if (!zone || (int) utc_time == -1)
		return utc_time;

	return icaltime_as_timet (
		icaltime_from_timet_with_zone (utc_time, FALSE, zone));
}

static gboolean
cal_shell_backend_handle_uri_cb (EShellBackend *shell_backend,
                                 const gchar *uri)
{
	EShell *shell;
	CompEditor *editor;
	CompEditorFlags flags = 0;
	ECal *client;
	ECalComponent *comp;
	ESource *source;
	ESourceList *source_list;
	ECalSourceType source_type;
	EUri *euri;
	icalcomponent *icalcomp;
	icalproperty *icalprop;
	const gchar *cp;
	gchar *source_uid = NULL;
	gchar *comp_uid = NULL;
	gchar *comp_rid = NULL;
	GDate start_date;
	GDate end_date;
	gboolean handled = FALSE;
	GError *error = NULL;

	source_type = E_CAL_SOURCE_TYPE_EVENT;
	shell = e_shell_backend_get_shell (shell_backend);

	if (strncmp (uri, "calendar:", 9) != 0)
		return FALSE;

	euri = e_uri_new (uri);
	cp = euri->query;
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
				&start_date, utc_to_user_zone (time_from_isodate (content)));
		else if (g_ascii_strcasecmp (header, "enddate") == 0)
			g_date_set_time_t (
				&end_date, utc_to_user_zone (time_from_isodate (content)));
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
			if (strcmp (cp, "amp;") == 0)
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

	if (!e_cal_get_sources (&source_list, source_type, NULL)) {
		g_printerr ("Could not get calendar sources from GConf!\n");
		goto exit;
	}

	source = e_source_list_peek_source_by_uid (source_list, source_uid);
	if (source == NULL) {
		g_printerr ("No source for UID '%s'\n", source_uid);
		g_object_unref (source_list);
		goto exit;
	}

	client = e_auth_new_cal_from_source (source, source_type);
	if (client == NULL || !e_cal_open (client, TRUE, &error)) {
		if (error != NULL) {
			g_printerr ("%s\n", error->message);
			g_error_free (error);
		}
		g_object_unref (source_list);
		goto exit;
	}

	/* XXX Copied from e_cal_shell_view_open_event().
	 *     Clearly a new utility function is needed. */

	editor = comp_editor_find_instance (comp_uid);

	if (editor != NULL)
		goto present;

	if (!e_cal_get_object (client, comp_uid, comp_rid, &icalcomp, &error)) {
		g_printerr ("%s\n", error->message);
		g_object_unref (source_list);
		g_error_free (error);
		goto exit;
	}

	comp = e_cal_component_new ();
	e_cal_component_set_icalcomponent (comp, icalcomp);

	icalprop = icalcomponent_get_first_property (
		icalcomp, ICAL_ATTENDEE_PROPERTY);
	if (icalprop != NULL)
		flags |= COMP_EDITOR_MEETING;

	if (itip_organizer_is_user (comp, client))
		flags |= COMP_EDITOR_USER_ORG;

	if (itip_sentby_is_user (comp, client))
		flags |= COMP_EDITOR_USER_ORG;

	if (!e_cal_component_has_attendees (comp))
		flags |= COMP_EDITOR_USER_ORG;

	editor = event_editor_new (client, shell, flags);
	comp_editor_edit_comp (editor, comp);

	g_object_unref (comp);

present:
	gtk_window_present (GTK_WINDOW (editor));

	g_object_unref (source_list);
	g_object_unref (client);

exit:
	g_free (source_uid);
	g_free (comp_uid);
	g_free (comp_rid);

	e_uri_free (euri);

	return handled;
}

static void
cal_shell_backend_window_created_cb (EShellBackend *shell_backend,
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
cal_shell_backend_get_property (GObject *object,
                                guint property_id,
                                GValue *value,
                                GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_SOURCE_LIST:
			g_value_set_object (
				value,
				e_cal_shell_backend_get_source_list (
				E_CAL_SHELL_BACKEND (object)));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
cal_shell_backend_dispose (GObject *object)
{
	ECalShellBackendPrivate *priv;

	priv = E_CAL_SHELL_BACKEND_GET_PRIVATE (object);

	if (priv->source_list != NULL) {
		g_object_unref (priv->source_list);
		priv->source_list = NULL;
	}

	/* Chain up to parent's dispose() method. */
	G_OBJECT_CLASS (parent_class)->dispose (object);
}

static void
cal_shell_backend_constructed (GObject *object)
{
	EShell *shell;
	EShellBackend *shell_backend;
	GtkWidget *preferences_window;

	shell_backend = E_SHELL_BACKEND (object);
	shell = e_shell_backend_get_shell (shell_backend);

	cal_shell_backend_ensure_sources (shell_backend);

	g_signal_connect_swapped (
		shell, "handle-uri",
		G_CALLBACK (cal_shell_backend_handle_uri_cb),
		shell_backend);

	g_signal_connect_swapped (
		shell, "window-created",
		G_CALLBACK (cal_shell_backend_window_created_cb),
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
		calendar_prefs_dialog_new,
		600);
}

static void
cal_shell_backend_class_init (ECalShellBackendClass *class)
{
	GObjectClass *object_class;
	EShellBackendClass *shell_backend_class;

	parent_class = g_type_class_peek_parent (class);
	g_type_class_add_private (class, sizeof (ECalShellBackendPrivate));

	object_class = G_OBJECT_CLASS (class);
	object_class->get_property = cal_shell_backend_get_property;
	object_class->dispose = cal_shell_backend_dispose;
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

	g_object_class_install_property (
		object_class,
		PROP_SOURCE_LIST,
		g_param_spec_object (
			"source-list",
			"Source List",
			"The registry of calendars",
			E_TYPE_SOURCE_LIST,
			G_PARAM_READABLE));
}

static void
cal_shell_backend_init (ECalShellBackend *cal_shell_backend)
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

GType
e_cal_shell_backend_get_type (void)
{
	return cal_shell_backend_type;
}

void
e_cal_shell_backend_register_type (GTypeModule *type_module)
{
	const GTypeInfo type_info = {
		sizeof (ECalShellBackendClass),
		(GBaseInitFunc) NULL,
		(GBaseFinalizeFunc) NULL,
		(GClassInitFunc) cal_shell_backend_class_init,
		(GClassFinalizeFunc) NULL,
		NULL,  /* class_data */
		sizeof (ECalShellBackend),
		0,     /* n_preallocs */
		(GInstanceInitFunc) cal_shell_backend_init,
		NULL   /* value_table */
	};

	cal_shell_backend_type = g_type_module_register_type (
		type_module, E_TYPE_SHELL_BACKEND,
		"ECalShellBackend", &type_info, 0);
}

ESourceList *
e_cal_shell_backend_get_source_list (ECalShellBackend *cal_shell_backend)
{
	g_return_val_if_fail (
		E_IS_CAL_SHELL_BACKEND (cal_shell_backend), NULL);

	return cal_shell_backend->priv->source_list;
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
	ECalendar *navigator;
	GList *watched_windows;

	g_return_if_fail (E_IS_CAL_SHELL_BACKEND (cal_shell_backend));

	shell_backend = E_SHELL_BACKEND (cal_shell_backend);
	shell = e_shell_backend_get_shell (shell_backend);
	watched_windows = e_shell_get_watched_windows (shell);

	/* Try to find an EShellWindow already in calendar view. */
	while (watched_windows != NULL) {
		GtkWidget *window = GTK_WIDGET (watched_windows->data);

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

		watched_windows = g_list_next (watched_windows);
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
