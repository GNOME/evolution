/*
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
 *
 * Authors: Milan Crha <mcrha@redhat.com>
 */

#include "evolution-config.h"

#include <string.h>
#include <glib/gi18n-lib.h>

#include <libedataserver/libedataserver.h>

#include <calendar/gui/comp-util.h>
#include <calendar/gui/e-comp-editor.h>

#include "shell/e-shell-backend.h"
#include "shell/e-shell-view.h"
#include "shell/e-shell-window.h"

#include "e-cal-base-shell-backend.h"

/*
 * FIXME: Remove this when there's a build time dependency on libical
 * 3.0.4 (where this is fixed). See
 * https://github.com/libical/libical/pull/335 and the implementation in
 * https://github.com/libical/libical/blob/master/src/libical/icalversion.h.cmake.
 */
#if defined(ICAL_CHECK_VERSION) && defined(ICAL_MAJOR_VERSION) && defined(ICAL_MINOR_VERSION) && defined(ICAL_MICRO_VERSION)
#undef ICAL_CHECK_VERSION
#define ICAL_CHECK_VERSION(major,minor,micro)                          \
    (ICAL_MAJOR_VERSION > (major) ||                                   \
    (ICAL_MAJOR_VERSION == (major) && ICAL_MINOR_VERSION > (minor)) || \
    (ICAL_MAJOR_VERSION == (major) && ICAL_MINOR_VERSION == (minor) && \
    ICAL_MICRO_VERSION >= (micro)))
#else
#if defined(ICAL_CHECK_VERSION)
#undef ICAL_CHECK_VERSION
#endif
#define ICAL_CHECK_VERSION(major,minor,micro) (0)
#endif

#define E_CAL_BASE_SHELL_BACKEND_GET_PRIVATE(obj) \
	(G_TYPE_INSTANCE_GET_PRIVATE \
	((obj), E_TYPE_CAL_BASE_SHELL_BACKEND, ECalBaseShellBackendPrivate))

struct _ECalBaseShellBackendPrivate {
	gint placeholder;
};

G_DEFINE_ABSTRACT_TYPE (ECalBaseShellBackend, e_cal_base_shell_backend, E_TYPE_SHELL_BACKEND)

static void
cal_base_shell_backend_handle_webcal_uri (EShellBackend *shell_backend,
					  const gchar *uri)
{
	EShell *shell;
	ESourceRegistry *registry;
	ESourceConfig *source_config;
	const gchar *extension_name;
	GtkWidget *config;
	GtkWidget *dialog;
	GtkWindow *window, *active_window;
	GSList *candidates, *link;

	g_return_if_fail (E_IS_SHELL_BACKEND (shell_backend));
	g_return_if_fail (uri != NULL);

	shell = e_shell_backend_get_shell (shell_backend);

	active_window = e_shell_get_active_window (shell);
	registry = e_shell_get_registry (shell);
	config = e_cal_source_config_new (registry, NULL, E_CAL_CLIENT_SOURCE_TYPE_EVENTS);
	source_config = E_SOURCE_CONFIG (config);

	extension_name = e_source_config_get_backend_extension_name (source_config);

	dialog = e_source_config_dialog_new (source_config);
	window = GTK_WINDOW (dialog);

	if (active_window)
		gtk_window_set_transient_for (window, active_window);
	gtk_window_set_icon_name (window, "x-office-calendar");
	gtk_window_set_title (window, _("New Calendar"));

	gtk_widget_show (dialog);

	/* Can do this only after the dialog is shown, thus the list
	   of candidates is populated. */
	candidates = e_source_config_list_candidates (source_config);

	for (link = candidates; link; link = g_slist_next (link)) {
		ESource *candidate = link->data;

		if (e_source_has_extension (candidate, extension_name)) {
			const gchar *backend_name;

			backend_name = e_source_backend_get_backend_name (
				e_source_get_extension (candidate, extension_name));
			if (g_strcmp0 (backend_name, "webcal") == 0) {
				ESourceWebdav *webdav_extension;
				SoupURI *soup_uri;

				soup_uri = soup_uri_new (uri);
				if (!soup_uri) {
					/* Just a fallback when the passed-in URI is invalid,
					   to have set something in the UI. */
					soup_uri = soup_uri_new (NULL);
					soup_uri_set_path (soup_uri, uri);
				}

				/* https everywhere */
				soup_uri_set_scheme (soup_uri, "https");

				if (soup_uri_get_path (soup_uri)) {
					gchar *basename;

					basename = g_path_get_basename (soup_uri_get_path (soup_uri));
					if (basename && g_utf8_strlen (basename, -1) > 3) {
						gchar *dot;

						dot = strrchr (basename, '.');
						if (dot && strlen (dot) <= 4)
							*dot = '\0';

						if (*basename)
							e_source_set_display_name (candidate, basename);
					}

					g_free (basename);
				}

				webdav_extension = e_source_get_extension (candidate, E_SOURCE_EXTENSION_WEBDAV_BACKEND);
				e_source_webdav_set_soup_uri (webdav_extension, soup_uri);

				e_source_config_select_page (source_config, candidate);

				soup_uri_free (soup_uri);
				break;
			}
		}
	}

	g_slist_free_full (candidates, g_object_unref);
}

static gboolean
cal_base_shell_backend_handle_uri_cb (EShellBackend *shell_backend,
				      const gchar *uri)
{
	ECalBaseShellBackendClass *klass;

	g_return_val_if_fail (E_IS_CAL_BASE_SHELL_BACKEND (shell_backend), FALSE);
	g_return_val_if_fail (uri != NULL, FALSE);

	if (g_str_has_prefix (uri, "webcal:")) {
		cal_base_shell_backend_handle_webcal_uri (shell_backend, uri);
		return TRUE;
	}

	klass = E_CAL_BASE_SHELL_BACKEND_GET_CLASS (shell_backend);
	g_return_val_if_fail (klass != NULL, FALSE);

	return klass->handle_uri && klass->handle_uri (shell_backend, uri);
}

static void
cal_base_shell_backend_window_added_cb (ECalBaseShellBackend *cal_base_shell_backend,
					GtkWindow *window)
{
	ECalBaseShellBackendClass *cal_base_shell_backend_class;
	const gchar *backend_name;

	if (!E_IS_SHELL_WINDOW (window))
		return;

	cal_base_shell_backend_class = E_CAL_BASE_SHELL_BACKEND_GET_CLASS (cal_base_shell_backend);
	g_return_if_fail (cal_base_shell_backend_class != NULL);

	backend_name = E_SHELL_BACKEND_GET_CLASS (cal_base_shell_backend)->name;

	if (cal_base_shell_backend_class->new_item_entries &&
	    cal_base_shell_backend_class->new_item_n_entries > 0)
		e_shell_window_register_new_item_actions (
			E_SHELL_WINDOW (window), backend_name,
			cal_base_shell_backend_class->new_item_entries,
			cal_base_shell_backend_class->new_item_n_entries);

	if (cal_base_shell_backend_class->source_entries &&
	    cal_base_shell_backend_class->source_n_entries > 0)
		e_shell_window_register_new_source_actions (
			E_SHELL_WINDOW (window), backend_name,
			cal_base_shell_backend_class->source_entries,
			cal_base_shell_backend_class->source_n_entries);
}

static void
cal_base_shell_backend_constructed (GObject *object)
{
	EShell *shell;
	EShellBackend *shell_backend;

	/* Chain up to parent's constructed() method. */
	G_OBJECT_CLASS (e_cal_base_shell_backend_parent_class)->constructed (object);

	shell_backend = E_SHELL_BACKEND (object);
	shell = e_shell_backend_get_shell (shell_backend);

	g_signal_connect_swapped (
		shell, "handle-uri",
		G_CALLBACK (cal_base_shell_backend_handle_uri_cb),
		shell_backend);

	g_signal_connect_swapped (
		shell, "window-added",
		G_CALLBACK (cal_base_shell_backend_window_added_cb),
		shell_backend);
}

static void
e_cal_base_shell_backend_class_init (ECalBaseShellBackendClass *class)
{
	GObjectClass *object_class;

	g_type_class_add_private (class, sizeof (ECalBaseShellBackendPrivate));

	object_class = G_OBJECT_CLASS (class);
	object_class->constructed = cal_base_shell_backend_constructed;

	class->new_item_entries = NULL;
	class->new_item_n_entries = 0;
	class->source_entries = NULL;
	class->source_n_entries = 0;
	class->handle_uri = NULL;

	/* Register relevant ESource extensions. */
	g_type_ensure (E_TYPE_SOURCE_CALENDAR);
}

static void
e_cal_base_shell_backend_init (ECalBaseShellBackend *cal_base_shell_backend)
{
#if !ICAL_CHECK_VERSION(3, 0, 2)
	icalarray *builtin_timezones;
	gint ii;
#endif

	cal_base_shell_backend->priv = E_CAL_BASE_SHELL_BACKEND_GET_PRIVATE (cal_base_shell_backend);

#if !ICAL_CHECK_VERSION(3, 0, 2)
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
	 *
	 *     This is apparently fixed with additional locking in
	 *     libical 3.0.1 and 3.0.2:
	 *     https://github.com/libical/libical/releases/tag/v3.0.1
	 *     https://github.com/libical/libical/releases/tag/v3.0.2
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
#endif
}

void
e_cal_base_shell_backend_util_new_source (EShellWindow *shell_window,
					  ECalClientSourceType source_type)
{
	EShell *shell;
	ESourceRegistry *registry;
	GtkWidget *config;
	GtkWidget *dialog;
	GtkWindow *window;
	const gchar *icon_name;
	const gchar *title;

	g_return_if_fail (E_IS_SHELL_WINDOW (shell_window));

	switch (source_type) {
		case E_CAL_CLIENT_SOURCE_TYPE_EVENTS:
			title = _("New Calendar");
			icon_name = "x-office-calendar";
			break;
		case E_CAL_CLIENT_SOURCE_TYPE_MEMOS:
			title = _("New Memo List");
			icon_name = "stock_notes";
			break;
		case E_CAL_CLIENT_SOURCE_TYPE_TASKS:
			title = _("New Task List");
			icon_name = "stock_todo";
			break;
		default:
			g_warn_if_reached ();
			return;
	}

	shell = e_shell_window_get_shell (shell_window);

	registry = e_shell_get_registry (shell);
	config = e_cal_source_config_new (registry, NULL, source_type);

	dialog = e_source_config_dialog_new (E_SOURCE_CONFIG (config));
	window = GTK_WINDOW (dialog);

	gtk_window_set_transient_for (window, GTK_WINDOW (shell_window));
	gtk_window_set_icon_name (window, icon_name);
	gtk_window_set_title (window, title);

	gtk_widget_show (dialog);
}

typedef struct {
	EShellBackend *shell_backend;
	ECalClientSourceType source_type;
	gchar *source_uid;
	gchar *comp_uid;
	gchar *comp_rid;

	ECalClient *cal_client;
	icalcomponent *existing_icalcomp;
} HandleUriData;

static void
handle_uri_data_free (gpointer ptr)
{
	HandleUriData *hud = ptr;

	if (!hud)
		return;

	if (hud->cal_client) {
		ECompEditor *comp_editor = NULL;

		comp_editor = e_comp_editor_open_for_component (NULL,
			e_shell_backend_get_shell (hud->shell_backend),
			e_client_get_source (E_CLIENT (hud->cal_client)),
			hud->existing_icalcomp, 0);

		if (comp_editor)
			gtk_window_present (GTK_WINDOW (comp_editor));
	}

	if (hud->existing_icalcomp)
		icalcomponent_free (hud->existing_icalcomp);

	g_clear_object (&hud->cal_client);
	g_clear_object (&hud->shell_backend);
	g_free (hud->source_uid);
	g_free (hud->comp_uid);
	g_free (hud->comp_rid);
	g_free (hud);
}

static void
cal_base_shell_backend_handle_uri_thread (EAlertSinkThreadJobData *job_data,
					  gpointer user_data,
					  GCancellable *cancellable,
					  GError **error)
{
	HandleUriData *hud = user_data;
	EShell *shell;
	ESourceRegistry *registry;
	ESource *source;
	const gchar *extension_name;
	GError *local_error = NULL;

	g_return_if_fail (hud != NULL);

	switch (hud->source_type) {
		case E_CAL_CLIENT_SOURCE_TYPE_EVENTS:
			extension_name = E_SOURCE_EXTENSION_CALENDAR;
			break;
		case E_CAL_CLIENT_SOURCE_TYPE_MEMOS:
			extension_name = E_SOURCE_EXTENSION_MEMO_LIST;
			break;
		case E_CAL_CLIENT_SOURCE_TYPE_TASKS:
			extension_name = E_SOURCE_EXTENSION_TASK_LIST;
			break;
		default:
			g_warn_if_reached ();
			return;
	}

	shell = e_shell_backend_get_shell (hud->shell_backend);
	registry = e_shell_get_registry (shell);
	source = e_source_registry_ref_source (registry, hud->source_uid);
	if (!source) {
		g_set_error (&local_error, G_IO_ERROR, G_IO_ERROR_NOT_FOUND,
			_("Source with UID “%s” not found"), hud->source_uid);
	} else {
		EClientCache *client_cache;
		EClient *client;

		client_cache = e_shell_get_client_cache (shell);

		client = e_client_cache_get_client_sync (client_cache, source, extension_name, 30, cancellable, &local_error);
		if (client) {
			hud->cal_client = E_CAL_CLIENT (client);

			if (!e_cal_client_get_object_sync (hud->cal_client, hud->comp_uid,
				hud->comp_rid, &hud->existing_icalcomp, cancellable, &local_error))
				g_clear_object (&hud->cal_client);
		}
	}

	e_util_propagate_open_source_job_error (job_data, extension_name, local_error, error);

	g_clear_object (&source);
}

static void
populate_g_date (GDate *date,
                 time_t utc_time,
                 icaltimezone *zone)
{
	struct icaltimetype icaltm;

	g_return_if_fail (date != NULL);

	if ((gint) utc_time == -1)
		return;

	icaltm = icaltime_from_timet_with_zone (utc_time, FALSE, zone);

	if (icaltime_is_null_time (icaltm) ||
	    !icaltime_is_valid_time (icaltm))
		return;

	g_date_set_dmy (date, icaltm.day, icaltm.month, icaltm.year);
}

static time_t
convert_time_from_isodate (const gchar *text,
			   icaltimezone *use_date_zone)
{
	time_t res;

	g_return_val_if_fail (text != NULL, (time_t) 0);

	res = time_from_isodate (text);

	/* Is it date only? Then use the date zone to match the right day */
	if (use_date_zone && strlen (text) == 8) {
		struct icaltimetype itt;

		itt = icaltime_from_timet_with_zone (res, TRUE, NULL);
		res = icaltime_as_timet_with_zone (itt, use_date_zone);
	}

	return res;
}

gboolean
e_cal_base_shell_backend_util_handle_uri (EShellBackend *shell_backend,
					  ECalClientSourceType source_type,
					  const gchar *uri,
					  ECalBaseShellBackendHandleStartEndDatesFunc handle_start_end_dates)
{
	EShell *shell;
	EShellWindow *shell_window;
	SoupURI *soup_uri;
	const gchar *cp;
	gchar *source_uid = NULL;
	gchar *comp_uid = NULL;
	gchar *comp_rid = NULL;
	gchar *new_ics = NULL;
	gboolean attendees = FALSE;
	gboolean handled = FALSE;
	GSettings *settings;
	GList *windows, *link;
	GDate start_date;
	GDate end_date;
	icaltimezone *zone = NULL;
	const gchar *extension_name;

	g_return_val_if_fail (E_IS_CAL_BASE_SHELL_BACKEND (shell_backend), FALSE);
	g_return_val_if_fail (uri != NULL, FALSE);

	switch (source_type) {
		case E_CAL_CLIENT_SOURCE_TYPE_EVENTS:
			extension_name = E_SOURCE_EXTENSION_CALENDAR;
			break;
		case E_CAL_CLIENT_SOURCE_TYPE_MEMOS:
			extension_name = E_SOURCE_EXTENSION_MEMO_LIST;
			break;
		case E_CAL_CLIENT_SOURCE_TYPE_TASKS:
			extension_name = E_SOURCE_EXTENSION_TASK_LIST;
			break;
		default:
			g_warn_if_reached ();
			return FALSE;
	}

	shell = e_shell_backend_get_shell (shell_backend);

	soup_uri = soup_uri_new (uri);

	if (soup_uri == NULL)
		return FALSE;

	g_date_clear (&start_date, 1);
	g_date_clear (&end_date, 1);

	settings = e_util_ref_settings ("org.gnome.evolution.calendar");

	if (g_settings_get_boolean (settings, "use-system-timezone"))
		zone = e_cal_util_get_system_timezone ();
	else {
		gchar *location;

		location = g_settings_get_string (settings, "timezone");

		if (location != NULL) {
			zone = icaltimezone_get_builtin_timezone (location);
			g_free (location);
		}
	}

	if (zone == NULL)
		zone = icaltimezone_get_utc_timezone ();

	g_object_unref (settings);

	cp = soup_uri_get_query (soup_uri);
	if (cp == NULL)
		goto exit;

	while (*cp != '\0') {
		gchar *header;
		gchar *content;
		gsize header_len;
		gsize content_len;

		header_len = strcspn (cp, "=&");

		/* If it's malformed, give up. */
		if (cp[header_len] != '=')
			break;

		header = (gchar *) cp;
		header[header_len] = '\0';
		cp += header_len + 1;

		content_len = strcspn (cp, "&");

		content = g_strndup (cp, content_len);
		if (g_ascii_strcasecmp (header, "startdate") == 0)
			populate_g_date (&start_date, convert_time_from_isodate (content, zone), zone);
		else if (g_ascii_strcasecmp (header, "enddate") == 0)
			populate_g_date (&end_date, convert_time_from_isodate (content, zone) - 1, zone);
		else if (g_ascii_strcasecmp (header, "source-uid") == 0)
			source_uid = g_strdup (content);
		else if (g_ascii_strcasecmp (header, "comp-uid") == 0)
			comp_uid = g_strdup (content);
		else if (g_ascii_strcasecmp (header, "comp-rid") == 0)
			comp_rid = g_strdup (content);
		else if (g_ascii_strcasecmp (header, "new-ics") == 0)
			new_ics = g_strdup (content);
		else if (g_ascii_strcasecmp (header, "attendees") == 0)
			attendees = g_strcmp0 (content, "true") == 0 || g_strcmp0 (content, "1") == 0;
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
	if (g_date_valid (&start_date) && handle_start_end_dates) {
		if (g_date_valid (&end_date) && g_date_compare (&start_date, &end_date) > 0)
			end_date = start_date;

		handle_start_end_dates (shell_backend, &start_date, &end_date);
		handled = TRUE;
		goto exit;
	}

	if (!new_ics && (!source_uid || !comp_uid))
		goto exit;

	/* URI is valid, so consider it handled.  Whether
	 * we successfully open it is another matter... */
	handled = TRUE;

	shell_window = NULL;
	windows = gtk_application_get_windows (GTK_APPLICATION (shell));
	for (link = windows; link; link = g_list_next (link)) {
		GtkWindow *window = link->data;

		if (E_IS_SHELL_WINDOW (window)) {
			shell_window = E_SHELL_WINDOW (window);
			break;
		}
	}

	if (new_ics) {
		gchar *content = NULL;
		icalcomponent *icalcomp;
		GError *error = NULL;

		if (!g_file_get_contents (new_ics, &content, NULL, &error)) {
			if (error)
				g_warning ("Cannot create new ics: %s", error->message);
			else
				g_warning ("Cannot create new ics: Failed to open file '%s': Unknown error", new_ics);
			g_clear_error (&error);
			goto exit;
		}

		icalcomp = content ? icalcomponent_new_from_string (content) : NULL;
		if (!icalcomp) {
			g_warning ("Cannot create new ics: File '%s' doesn't contain valid iCalendar component", new_ics);
			g_free (content);
			goto exit;
		}

		if (icalcomponent_isa (icalcomp) == ICAL_VEVENT_COMPONENT &&
		    source_type != E_CAL_CLIENT_SOURCE_TYPE_EVENTS) {
			g_warning ("Cannot create new ics: Expected %s, but got VEVENT", source_type == E_CAL_CLIENT_SOURCE_TYPE_TASKS ? "VTODO" : "VJOURNAL");
		} else if (icalcomponent_isa (icalcomp) == ICAL_VJOURNAL_COMPONENT &&
			   source_type != E_CAL_CLIENT_SOURCE_TYPE_MEMOS) {
			g_warning ("Cannot create new ics: Expected %s, but got VJOURNAL", source_type == E_CAL_CLIENT_SOURCE_TYPE_TASKS ? "VTODO" : "VEVENT");
		} else if (icalcomponent_isa (icalcomp) == ICAL_VTODO_COMPONENT &&
			   source_type != E_CAL_CLIENT_SOURCE_TYPE_TASKS) {
			g_warning ("Cannot create new ics: Expected %s, but got VTODO", source_type == E_CAL_CLIENT_SOURCE_TYPE_MEMOS ? "VJOURNAL" : "VEVENT");
		} else if (icalcomponent_isa (icalcomp) != ICAL_VEVENT_COMPONENT &&
			   icalcomponent_isa (icalcomp) != ICAL_VJOURNAL_COMPONENT &&
			   icalcomponent_isa (icalcomp) != ICAL_VTODO_COMPONENT) {
			g_warning ("Cannot create new ics: Received unexpected component type '%s'", icalcomponent_kind_to_string (icalcomponent_isa (icalcomp)));
		} else {
			ECompEditor *comp_editor;
			ESource *source = NULL;
			ECompEditorFlags flags;

			if (source_uid) {
				ESourceRegistry *registry;

				registry = e_shell_get_registry (shell);
				source = e_source_registry_ref_source (registry, source_uid);
			}

			flags = E_COMP_EDITOR_FLAG_IS_NEW | E_COMP_EDITOR_FLAG_ORGANIZER_IS_USER |
				(attendees ? E_COMP_EDITOR_FLAG_WITH_ATTENDEES : 0);

			comp_editor = e_comp_editor_open_for_component (NULL, shell, source, icalcomp, flags);

			if (comp_editor)
				gtk_window_present (GTK_WINDOW (comp_editor));

			g_clear_object (&source);
		}

		icalcomponent_free (icalcomp);
		g_free (content);
	} else if (shell_window) {
		HandleUriData *hud;
		ESourceRegistry *registry;
		ESource *source;
		EShellView *shell_view;
		EActivity *activity;
		gchar *description = NULL, *alert_ident = NULL, *alert_arg_0 = NULL;
		gchar *source_display_name = NULL;

		hud = g_new0 (HandleUriData, 1);
		hud->shell_backend = g_object_ref (shell_backend);
		hud->source_type = source_type;
		hud->source_uid = g_strdup (source_uid);
		hud->comp_uid = g_strdup (comp_uid);
		hud->comp_rid = g_strdup (comp_rid);
		hud->cal_client = NULL;
		hud->existing_icalcomp = NULL;

		registry = e_shell_get_registry (shell);
		source = e_source_registry_ref_source (registry, source_uid);
		if (source)
			source_display_name = e_util_get_source_full_name (registry, source);

		shell_view = e_shell_window_get_shell_view (shell_window,
			e_shell_window_get_active_view (shell_window));

		g_warn_if_fail (e_util_get_open_source_job_info (extension_name,
			source_display_name ? source_display_name : "", &description, &alert_ident, &alert_arg_0));

		activity = e_shell_view_submit_thread_job (
			shell_view, description, alert_ident, alert_arg_0,
			cal_base_shell_backend_handle_uri_thread, hud, handle_uri_data_free);

		g_clear_object (&activity);
		g_clear_object (&source);
		g_free (source_display_name);
		g_free (description);
		g_free (alert_ident);
		g_free (alert_arg_0);
	} else {
		g_warn_if_reached ();
	}

 exit:
	g_free (source_uid);
	g_free (comp_uid);
	g_free (comp_rid);
	g_free (new_ics);

	soup_uri_free (soup_uri);

	return handled;
}
