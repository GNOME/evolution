/*
 * e-shell.c
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

/**
 * SECTION: e-shell
 * @short_description: the backbone of Evolution
 * @include: shell/e-shell.h
 **/

#include "e-shell.h"

#include <glib/gi18n.h>
#include <libedataserverui/e-passwords.h>

#include "e-util/e-module.h"
#include "e-util/e-extensible.h"
#include "e-util/e-util-private.h"
#include "e-util/e-util.h"
#include "smclient/eggsmclient.h"
#include "widgets/misc/e-preferences-window.h"

#include "e-shell-backend.h"
#include "e-shell-enumtypes.h"
#include "e-shell-window.h"
#include "e-shell-utils.h"

#define E_SHELL_GET_PRIVATE(obj) \
	(G_TYPE_INSTANCE_GET_PRIVATE \
	((obj), E_TYPE_SHELL, EShellPrivate))

struct _EShellPrivate {
	GList *watched_windows;
	EShellSettings *settings;
	GConfClient *gconf_client;
	GtkWidget *preferences_window;

	/* Shell Backends */
	GList *loaded_backends;              /* not referenced */
	GHashTable *backends_by_name;
	GHashTable *backends_by_scheme;

	gpointer preparing_for_line_change;  /* weak pointer */
	gpointer preparing_for_quit;         /* weak pointer */

	gchar *geometry;
	gchar *module_directory;

	gchar *startup_view;

	guint auto_reconnect	: 1;
	guint modules_loaded	: 1;
	guint network_available	: 1;
	guint online		: 1;
	guint quit_cancelled	: 1;
	guint safe_mode		: 1;
	guint express_mode	: 1;
	guint meego_mode	: 1;
	guint small_screen_mode	: 1;
};

enum {
	PROP_0,
	PROP_EXPRESS_MODE,
	PROP_MEEGO_MODE,
	PROP_SMALL_SCREEN_MODE,
	PROP_GEOMETRY,
	PROP_MODULE_DIRECTORY,
	PROP_NETWORK_AVAILABLE,
	PROP_ONLINE,
	PROP_SHELL_SETTINGS
};

enum {
	EVENT,
	HANDLE_URI,
	PREPARE_FOR_OFFLINE,
	PREPARE_FOR_ONLINE,
	PREPARE_FOR_QUIT,
	QUIT_REQUESTED,
	SEND_RECEIVE,
	WINDOW_CREATED,
	WINDOW_DESTROYED,
	LAST_SIGNAL
};

enum {
	DEBUG_KEY_SETTINGS = 1 << 0
};

static GDebugKey debug_keys[] = {
	{ "settings",	DEBUG_KEY_SETTINGS }
};

static gpointer default_shell;
static guint signals[LAST_SIGNAL];

G_DEFINE_TYPE_WITH_CODE (
	EShell,
	e_shell,
	UNIQUE_TYPE_APP,
	G_IMPLEMENT_INTERFACE (
		E_TYPE_EXTENSIBLE, NULL))

static void
shell_parse_debug_string (EShell *shell)
{
	guint flags;

	flags = g_parse_debug_string (
		g_getenv ("EVOLUTION_DEBUG"),
		debug_keys, G_N_ELEMENTS (debug_keys));

	if (flags & DEBUG_KEY_SETTINGS)
		e_shell_settings_enable_debug (shell->priv->settings);
}

static void
shell_notify_online_cb (EShell *shell)
{
	gboolean online;

	online = e_shell_get_online (shell);
	e_passwords_set_online (online);
}

static gboolean
shell_window_delete_event_cb (EShell *shell,
                              GtkWindow *window)
{
	/* If other windows are open we can safely close this one. */
	if (g_list_length (shell->priv->watched_windows) > 1)
		return FALSE;

	/* Otherwise we initiate application quit. */
	e_shell_quit (shell, E_SHELL_QUIT_LAST_WINDOW);

	return TRUE;
}

static gboolean
shell_window_focus_in_event_cb (EShell *shell,
                                GdkEventFocus *event,
                                GtkWindow *window)
{
	GList *list, *link;

	/* Keep the watched windows list sorted by most recently focused,
	 * so the first item in the list should always be the currently
	 * focused window. */

	list = shell->priv->watched_windows;
	link = g_list_find (list, window);
	g_return_val_if_fail (link != NULL, FALSE);

	if (link != list) {
		list = g_list_remove_link (list, link);
		list = g_list_concat (link, list);
	}

	shell->priv->watched_windows = list;

	return FALSE;
}

static gboolean
shell_emit_window_destroyed_cb (EShell *shell)
{
	g_signal_emit (shell, signals[WINDOW_DESTROYED], 0);

	g_object_unref (shell);

	return FALSE;
}

static void
shell_window_weak_notify_cb (EShell *shell,
                             GObject *where_the_object_was)
{
	GList *list;

	list = shell->priv->watched_windows;
	list = g_list_remove (list, where_the_object_was);
	shell->priv->watched_windows = list;

	/* Let the watched window finish finalizing itself before we
	 * emit the "window-destroyed" signal, which may trigger the
	 * application to initiate shutdown. */
	g_idle_add (
		(GSourceFunc) shell_emit_window_destroyed_cb,
		g_object_ref (shell));
}

static void
shell_ready_for_offline (EShell *shell,
                         EActivity *activity,
                         gboolean is_last_ref)
{
	if (!is_last_ref)
		return;

	/* Increment the reference count so we can safely emit
	 * a signal without triggering the toggle reference. */
	g_object_ref (activity);

	e_activity_complete (activity);

	g_object_remove_toggle_ref (
		G_OBJECT (activity), (GToggleNotify)
		shell_ready_for_offline, shell);

	/* Finalize the activity. */
	g_object_unref (activity);

	shell->priv->online = FALSE;
	g_object_notify (G_OBJECT (shell), "online");
}

static void
shell_prepare_for_offline (EShell *shell)
{
	/* Are preparations already in progress? */
	if (shell->priv->preparing_for_line_change != NULL)
		return;

	shell->priv->preparing_for_line_change =
		e_activity_new (_("Preparing to go offline..."));

	g_object_add_toggle_ref (
		G_OBJECT (shell->priv->preparing_for_line_change),
		(GToggleNotify) shell_ready_for_offline, shell);

	g_object_add_weak_pointer (
		G_OBJECT (shell->priv->preparing_for_line_change),
		&shell->priv->preparing_for_line_change);

	g_signal_emit (
		shell, signals[PREPARE_FOR_OFFLINE], 0,
		shell->priv->preparing_for_line_change);

	g_object_unref (shell->priv->preparing_for_line_change);
}

static void
shell_ready_for_online (EShell *shell,
                        EActivity *activity,
                        gboolean is_last_ref)
{
	if (!is_last_ref)
		return;

	/* Increment the reference count so we can safely emit
	 * a signal without triggering the toggle reference. */
	g_object_ref (activity);

	e_activity_complete (activity);

	g_object_remove_toggle_ref (
		G_OBJECT (activity), (GToggleNotify)
		shell_ready_for_online, shell);

	/* Finalize the activity. */
	g_object_unref (activity);

	shell->priv->online = TRUE;
	g_object_notify (G_OBJECT (shell), "online");
}

static void
shell_prepare_for_online (EShell *shell)
{
	/* Are preparations already in progress? */
	if (shell->priv->preparing_for_line_change != NULL)
		return;

	shell->priv->preparing_for_line_change =
		e_activity_new (_("Preparing to go online..."));

	g_object_add_toggle_ref (
		G_OBJECT (shell->priv->preparing_for_line_change),
		(GToggleNotify) shell_ready_for_online, shell);

	g_object_add_weak_pointer (
		G_OBJECT (shell->priv->preparing_for_line_change),
		&shell->priv->preparing_for_line_change);

	g_signal_emit (
		shell, signals[PREPARE_FOR_ONLINE], 0,
		shell->priv->preparing_for_line_change);

	g_object_unref (shell->priv->preparing_for_line_change);
}

static void
shell_ready_for_quit (EShell *shell,
                      EActivity *activity,
                      gboolean is_last_ref)
{
	GList *list;

	if (!is_last_ref)
		return;

	/* Increment the reference count so we can safely emit
	 * a signal without triggering the toggle reference. */
	g_object_ref (activity);

	e_activity_complete (activity);

	g_object_remove_toggle_ref (
		G_OBJECT (activity), (GToggleNotify)
		shell_ready_for_quit, shell);

	/* Finalize the activity. */
	g_object_unref (activity);

	/* Destroy all watched windows.  Note, we iterate over a -copy-
	 * of the watched windows list because the act of destroying a
	 * watched window will modify the watched windows list, which
	 * would derail the iteration. */
	list = g_list_copy (e_shell_get_watched_windows (shell));
	g_list_foreach (list, (GFunc) gtk_widget_destroy, NULL);
	g_list_free (list);
}

static void
shell_prepare_for_quit (EShell *shell)
{
	GList *list, *iter;

	/* Are preparations already in progress? */
	if (shell->priv->preparing_for_quit != NULL)
		return;

	shell->priv->preparing_for_quit =
		e_activity_new (_("Preparing to quit..."));

	g_object_add_toggle_ref (
		G_OBJECT (shell->priv->preparing_for_quit),
		(GToggleNotify) shell_ready_for_quit, shell);

	g_object_add_weak_pointer (
		G_OBJECT (shell->priv->preparing_for_quit),
		&shell->priv->preparing_for_quit);

	g_signal_emit (
		shell, signals[PREPARE_FOR_QUIT], 0,
		shell->priv->preparing_for_quit);

	g_object_unref (shell->priv->preparing_for_quit);

	/* Desensitize all watched windows to prevent user action. */
	list = e_shell_get_watched_windows (shell);
	for (iter = list; iter != NULL; iter = iter->next)
		gtk_widget_set_sensitive (GTK_WIDGET (iter->data), FALSE);
}

static gboolean
shell_request_quit (EShell *shell,
                    EShellQuitReason reason)
{
	/* Are preparations already in progress? */
	if (shell->priv->preparing_for_quit != NULL)
		return TRUE;

	/* Give the application a chance to cancel quit. */
	shell->priv->quit_cancelled = FALSE;
	g_signal_emit (shell, signals[QUIT_REQUESTED], 0, reason);

	return !shell->priv->quit_cancelled;
}

/* Helper for shell_add_backend() */
static void
shell_split_and_insert_items (GHashTable *hash_table,
                              const gchar *items,
                              EShellBackend *shell_backend)
{
	gpointer key;
	gchar **strv;
	gint ii;

	strv = g_strsplit_set (items, ":", -1);

	for (ii = 0; strv[ii] != NULL; ii++) {
		key = (gpointer) g_intern_string (strv[ii]);
		g_hash_table_insert (hash_table, key, shell_backend);
	}

	g_strfreev (strv);
}

static void
shell_process_backend (EShellBackend *shell_backend,
                       EShell *shell)
{
	EShellBackendClass *class;
	GHashTable *backends_by_name;
	GHashTable *backends_by_scheme;
	const gchar *string;

	class = E_SHELL_BACKEND_GET_CLASS (shell_backend);
	backends_by_name = shell->priv->backends_by_name;
	backends_by_scheme = shell->priv->backends_by_scheme;

	if ((string = class->name) != NULL)
		g_hash_table_insert (
			backends_by_name, (gpointer)
			g_intern_string (string), shell_backend);

	if ((string = class->aliases) != NULL)
		shell_split_and_insert_items (
			backends_by_name, string, shell_backend);

	if ((string = class->schemes) != NULL)
		shell_split_and_insert_items (
			backends_by_scheme, string, shell_backend);
}

static void
shell_sm_quit_requested_cb (EShell *shell,
                            EggSMClient *sm_client)
{
	EShellQuitReason reason = E_SHELL_QUIT_SESSION_REQUEST;
	gboolean will_quit;

	/* If preparations are already in progress then we have already
	 * committed ourselves to quitting, and can answer 'yes'. */
	if (shell->priv->preparing_for_quit == NULL)
		will_quit = shell_request_quit (shell, reason);
	else
		will_quit = TRUE;

	egg_sm_client_will_quit (sm_client, will_quit);
}

static void
shell_sm_quit_cancelled_cb (EShell *shell,
                            EggSMClient *sm_client)
{
	/* Nothing to do.  This is just to aid debugging. */
}

static void
shell_sm_quit_cb (EShell *shell,
                  EggSMClient *sm_client)
{
	shell_prepare_for_quit (shell);
}

static void
shell_set_express_mode (EShell *shell,
                        gboolean express_mode)
{
	shell->priv->express_mode = express_mode;
}

static void
shell_set_meego_mode (EShell *shell,
                      gboolean is_meego)
{
	shell->priv->meego_mode = is_meego;
}

static void
shell_set_small_screen_mode (EShell *shell,
                             gboolean small_screen)
{
	shell->priv->small_screen_mode = small_screen;
}

static void
shell_set_geometry (EShell *shell,
                    const gchar *geometry)
{
	g_return_if_fail (shell->priv->geometry == NULL);

	shell->priv->geometry = g_strdup (geometry);
}

static void
shell_set_module_directory (EShell *shell,
                            const gchar *module_directory)
{
	g_return_if_fail (shell->priv->module_directory == NULL);

	shell->priv->module_directory = g_strdup (module_directory);
}

static void
shell_set_property (GObject *object,
                    guint property_id,
                    const GValue *value,
                    GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_EXPRESS_MODE:
			shell_set_express_mode (
				E_SHELL (object),
				g_value_get_boolean (value));
			return;

		case PROP_MEEGO_MODE:
			shell_set_meego_mode (
				E_SHELL (object),
				g_value_get_boolean (value));
			return;

		case PROP_SMALL_SCREEN_MODE:
			shell_set_small_screen_mode (
				E_SHELL (object),
				g_value_get_boolean (value));
			return;

		case PROP_GEOMETRY:
			shell_set_geometry (
				E_SHELL (object),
				g_value_get_string (value));
			return;

		case PROP_MODULE_DIRECTORY:
			shell_set_module_directory (
				E_SHELL (object),
				g_value_get_string (value));
			return;

		case PROP_NETWORK_AVAILABLE:
			e_shell_set_network_available (
				E_SHELL (object),
				g_value_get_boolean (value));
			return;

		case PROP_ONLINE:
			e_shell_set_online (
				E_SHELL (object),
				g_value_get_boolean (value));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
shell_get_property (GObject *object,
                    guint property_id,
                    GValue *value,
                    GParamSpec *pspec)
{
	switch (property_id) {
		case PROP_EXPRESS_MODE:
			g_value_set_boolean (
				value, e_shell_get_express_mode (
				E_SHELL (object)));
			return;

		case PROP_MEEGO_MODE:
			g_value_set_boolean (
				value, e_shell_get_meego_mode (
				E_SHELL (object)));
			return;

		case PROP_SMALL_SCREEN_MODE:
			g_value_set_boolean (
				value, e_shell_get_small_screen_mode (
				E_SHELL (object)));
			return;

		case PROP_MODULE_DIRECTORY:
			g_value_set_string (
				value, e_shell_get_module_directory (
				E_SHELL (object)));
			return;

		case PROP_NETWORK_AVAILABLE:
			g_value_set_boolean (
				value, e_shell_get_network_available (
				E_SHELL (object)));
			return;

		case PROP_ONLINE:
			g_value_set_boolean (
				value, e_shell_get_online (
				E_SHELL (object)));
			return;

		case PROP_SHELL_SETTINGS:
			g_value_set_object (
				value, e_shell_get_shell_settings (
				E_SHELL (object)));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
shell_dispose (GObject *object)
{
	EShellPrivate *priv;

	priv = E_SHELL_GET_PRIVATE (object);

	if (priv->startup_view != NULL) {
		g_free (priv->startup_view);
		priv->startup_view = NULL;
	}

	if (priv->settings != NULL) {
		g_object_unref (priv->settings);
		priv->settings = NULL;
	}

	if (priv->gconf_client != NULL) {
		g_object_unref (priv->gconf_client);
		priv->gconf_client = NULL;
	}

	if (priv->preferences_window != NULL) {
		g_object_unref (priv->preferences_window);
		priv->preferences_window = NULL;
	}

	if (priv->preparing_for_line_change != NULL) {
		g_object_remove_weak_pointer (
			G_OBJECT (priv->preparing_for_line_change),
			&priv->preparing_for_line_change);
	}

	/* Chain up to parent's dispose() method. */
	G_OBJECT_CLASS (e_shell_parent_class)->dispose (object);
}

static void
shell_finalize (GObject *object)
{
	EShellPrivate *priv;

	priv = E_SHELL_GET_PRIVATE (object);

	g_hash_table_destroy (priv->backends_by_name);
	g_hash_table_destroy (priv->backends_by_scheme);

	/* Indicates a clean shut down to the next session. */
	if (!unique_app_is_running (UNIQUE_APP (object)))
		e_file_lock_destroy ();

	g_list_foreach (priv->loaded_backends, (GFunc) g_object_unref, NULL);
	g_list_free (priv->loaded_backends);

	g_free (priv->geometry);
	g_free (priv->module_directory);

	/* Chain up to parent's finalize() method. */
	G_OBJECT_CLASS (e_shell_parent_class)->finalize (object);
}

static void
shell_constructed (GObject *object)
{
	/* The first EShell instance is the default. */
	if (default_shell == NULL) {
		default_shell = object;
		g_object_add_weak_pointer (object, &default_shell);
	}

	if (!unique_app_is_running (UNIQUE_APP (object)))
		e_file_lock_create ();
}

static UniqueResponse
shell_message_handle_activate (EShell *shell,
                               UniqueMessageData *data)
{
	GList *watched_windows;
	GdkScreen *screen;

	screen = unique_message_data_get_screen (data);
	watched_windows = e_shell_get_watched_windows (shell);

	/* Present the first EShellWindow, if found. */
	while (watched_windows != NULL) {
		GtkWindow *window = GTK_WINDOW (watched_windows->data);

		if (E_IS_SHELL_WINDOW (window)) {
			gtk_window_set_screen (window, screen);
			gtk_window_present (window);
			return UNIQUE_RESPONSE_OK;
		}

		watched_windows = g_list_next (watched_windows);
	}

	/* No EShellWindow found, so create one. */
	e_shell_create_shell_window (shell, NULL);

	return UNIQUE_RESPONSE_OK;
}

static UniqueResponse
shell_message_handle_new (EShell *shell,
                          UniqueMessageData *data)
{
	gchar *view_name;

	view_name = unique_message_data_get_text (data);
	e_shell_create_shell_window (shell, view_name);
	g_free (view_name);

	return UNIQUE_RESPONSE_OK;
}

static UniqueResponse
shell_message_handle_open (EShell *shell,
                           UniqueMessageData *data)
{
	gchar **uris;

	uris = unique_message_data_get_uris (data);
	if (uris && uris[0] && g_str_equal (uris[0], "--import")) {
		gint ii;
		GPtrArray *arr = g_ptr_array_new ();

		/* skip the first argument */
		for (ii = 1; uris[ii] != NULL; ii++) {
			g_ptr_array_add (arr, uris[ii]);
		}

		g_ptr_array_add (arr, NULL);

		e_shell_handle_uris (shell, (gchar **)arr->pdata, TRUE);

		g_ptr_array_free (arr, TRUE);
	} else {
		e_shell_handle_uris (shell, uris, FALSE);
	}
	g_strfreev (uris);

	return UNIQUE_RESPONSE_OK;
}

static UniqueResponse
shell_message_handle_close (EShell *shell,
                            UniqueMessageData *data)
{
	UniqueResponse response;

	if (e_shell_quit (shell, E_SHELL_QUIT_REMOTE_REQUEST))
		response = UNIQUE_RESPONSE_OK;
	else
		response = UNIQUE_RESPONSE_CANCEL;

	return response;
}

static UniqueResponse
shell_message_received (UniqueApp *app,
                        gint command,
                        UniqueMessageData *data,
                        guint time_)
{
	EShell *shell = E_SHELL (app);

	switch (command) {
		case UNIQUE_ACTIVATE:
			return shell_message_handle_activate (shell, data);

		case UNIQUE_NEW:
			return shell_message_handle_new (shell, data);

		case UNIQUE_OPEN:
			return shell_message_handle_open (shell, data);

		case UNIQUE_CLOSE:
			return shell_message_handle_close (shell, data);

		default:
			break;
	}

	/* Chain up to parent's message_received() method. */
	return UNIQUE_APP_CLASS (e_shell_parent_class)->
		message_received (app, command, data, time_);
}

static void
shell_window_destroyed (EShell *shell)
{
	if (e_shell_get_watched_windows (shell) == NULL)
		gtk_main_quit ();
}

static void
e_shell_class_init (EShellClass *class)
{
	GObjectClass *object_class;
	UniqueAppClass *unique_app_class;

	g_type_class_add_private (class, sizeof (EShellPrivate));

	object_class = G_OBJECT_CLASS (class);
	object_class->set_property = shell_set_property;
	object_class->get_property = shell_get_property;
	object_class->dispose = shell_dispose;
	object_class->finalize = shell_finalize;
	object_class->constructed = shell_constructed;

	unique_app_class = UNIQUE_APP_CLASS (class);
	unique_app_class->message_received = shell_message_received;

	class->window_destroyed = shell_window_destroyed;

	/**
	 * EShell:express-mode
	 *
	 * Express mode alters Evolution's user interface to be more
	 * usable on devices with small screens.
	 **/
	g_object_class_install_property (
		object_class,
		PROP_EXPRESS_MODE,
		g_param_spec_boolean (
			"express-mode",
			"Express Mode",
			"Whether express mode is enabled",
			FALSE,
			G_PARAM_READWRITE |
			G_PARAM_CONSTRUCT_ONLY));

	/**
	 * EShell:meego
	 *
	 * Are we running under meego - if so, adapt ourselves
	 * to fit in well with their theming.
	 **/
	g_object_class_install_property (
		object_class,
		PROP_MEEGO_MODE,
		g_param_spec_boolean (
			"meego-mode",
			"Meego Mode",
			"Whether meego mode is enabled",
			FALSE,
			G_PARAM_READWRITE |
			G_PARAM_CONSTRUCT_ONLY));

	/**
	 * EShell:small-screen
	 *
	 * Are we running with a small (1024x600) screen - if so, start
	 * throwing the babies overboard to fit onto that screen size.
	 **/
	g_object_class_install_property (
		object_class,
		PROP_SMALL_SCREEN_MODE,
		g_param_spec_boolean (
			"small-screen-mode",
			"Small Screen Mode",
			"Whether we run on a rather small screen",
			FALSE,
			G_PARAM_READWRITE |
			G_PARAM_CONSTRUCT_ONLY));

	/**
	 * EShell:geometry
	 *
	 * User-specified initial window geometry string to apply
	 * to the first #EShellWindow created.
	 **/
	g_object_class_install_property (
		object_class,
		PROP_GEOMETRY,
		g_param_spec_string (
			"geometry",
			"Geometry",
			"Initial window geometry string",
			NULL,
			G_PARAM_WRITABLE |
			G_PARAM_CONSTRUCT_ONLY));

	/**
	 * EShell:module-directory
	 *
	 * The directory from which to load #EModule<!-- -->s.
	 **/
	g_object_class_install_property (
		object_class,
		PROP_MODULE_DIRECTORY,
		g_param_spec_string (
			"module-directory",
			"Module Directory",
			"The directory from which to load EModules",
			NULL,
			G_PARAM_READWRITE |
			G_PARAM_CONSTRUCT_ONLY));

	/**
	 * EShell:network-available
	 *
	 * Whether the network is available.
	 **/
	g_object_class_install_property (
		object_class,
		PROP_NETWORK_AVAILABLE,
		g_param_spec_boolean (
			"network-available",
			"Network Available",
			"Whether the network is available",
			TRUE,
			G_PARAM_READWRITE |
			G_PARAM_CONSTRUCT));

	/**
	 * EShell:online
	 *
	 * Whether the shell is online.
	 **/
	g_object_class_install_property (
		object_class,
		PROP_ONLINE,
		g_param_spec_boolean (
			"online",
			"Online",
			"Whether the shell is online",
			FALSE,
			G_PARAM_READWRITE |
			G_PARAM_CONSTRUCT));

	/**
	 * EShell:settings
	 *
	 * The #EShellSettings object stores application settings.
	 **/
	g_object_class_install_property (
		object_class,
		PROP_SHELL_SETTINGS,
		g_param_spec_object (
			"shell-settings",
			"Shell Settings",
			"Application-wide settings",
			E_TYPE_SHELL_SETTINGS,
			G_PARAM_READABLE));

	/**
	 * EShell::event
	 * @shell: the #EShell which emitted the signal
	 * @event_data: data associated with the event
	 *
	 * This signal is used to broadcast custom events to the entire
	 * application.  The nature of @event_data depends on the event
	 * being broadcast.  The signal's detail denotes the event.
	 **/
	signals[EVENT] = g_signal_new (
		"event",
		G_OBJECT_CLASS_TYPE (object_class),
		G_SIGNAL_RUN_FIRST | G_SIGNAL_DETAILED | G_SIGNAL_ACTION,
		0, NULL, NULL,
		g_cclosure_marshal_VOID__POINTER,
		G_TYPE_NONE, 1,
		G_TYPE_POINTER);

	/**
	 * EShell::handle-uri
	 * @shell: the #EShell which emitted the signal
	 * @uri: the URI to be handled
	 *
	 * Emitted when @shell receives a URI to be handled, usually by
	 * way of a command-line argument.  An #EShellBackend should listen
	 * for this signal and try to handle the URI, usually by opening an
	 * editor window for the identified resource.
	 *
	 * Returns: %TRUE if the URI could be handled, %FALSE otherwise
	 **/
	signals[HANDLE_URI] = g_signal_new (
		"handle-uri",
		G_OBJECT_CLASS_TYPE (object_class),
		G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
		G_STRUCT_OFFSET (EShellClass, handle_uri),
		g_signal_accumulator_true_handled, NULL,
		e_marshal_BOOLEAN__STRING,
		G_TYPE_BOOLEAN, 1,
		G_TYPE_STRING);

	/**
	 * EShell::prepare-for-offline
	 * @shell: the #EShell which emitted the signal
	 * @activity: the #EActivity for offline preparations
	 *
	 * Emitted when the user elects to work offline.  An #EShellBackend
	 * should listen for this signal and make preparations for working
	 * in offline mode.
	 *
	 * If preparations for working offline cannot immediately be
	 * completed (such as when synchronizing with a remote server),
	 * the #EShellBackend should reference the @activity until
	 * preparations are complete, and then unreference the @activity.
	 * This will delay Evolution from actually going to offline mode
	 * until all backends have unreferenced @activity.
	 **/
	signals[PREPARE_FOR_OFFLINE] = g_signal_new (
		"prepare-for-offline",
		G_OBJECT_CLASS_TYPE (object_class),
		G_SIGNAL_RUN_FIRST,
		G_STRUCT_OFFSET (EShellClass, prepare_for_offline),
		NULL, NULL,
		g_cclosure_marshal_VOID__OBJECT,
		G_TYPE_NONE, 1,
		E_TYPE_ACTIVITY);

	/**
	 * EShell::prepare-for-online
	 * @shell: the #EShell which emitted the signal
	 * @activity: the #EActivity for offline preparations
	 *
	 * Emitted when the user elects to work online.  An #EShellBackend
	 * should listen for this signal and make preparations for working
	 * in online mode.
	 *
	 * If preparations for working online cannot immediately be
	 * completed (such as when re-connecting to a remote server), the
	 * #EShellBackend should reference the @activity until preparations
	 * are complete, and then unreference the @activity.  This will
	 * delay Evolution from actually going to online mode until all
	 * backends have unreferenced @activity.
	 **/
	signals[PREPARE_FOR_ONLINE] = g_signal_new (
		"prepare-for-online",
		G_OBJECT_CLASS_TYPE (object_class),
		G_SIGNAL_RUN_FIRST,
		G_STRUCT_OFFSET (EShellClass, prepare_for_online),
		NULL, NULL,
		g_cclosure_marshal_VOID__OBJECT,
		G_TYPE_NONE, 1,
		E_TYPE_ACTIVITY);

	/**
	 * EShell::prepare-for-quit
	 * @shell: the #EShell which emitted the signal
	 * @activity: the #EActivity for quit preparations
	 *
	 * Emitted when the user elects to quit the application, after
	 * #EShell::quit-requested.  An #EShellBackend should listen for
	 * this signal and make preparations for shutting down.
	 *
	 * If preparations for shutting down cannot immediately be completed
	 * (such as when there are uncompleted network operations), the
	 * #EShellBackend should reference the @activity until preparations
	 * are complete, and then unreference the @activity.  This will
	 * delay Evolution from actually shutting down until all backends
	 * have unreferenced @activity.
	 **/
	signals[PREPARE_FOR_QUIT] = g_signal_new (
		"prepare-for-quit",
		G_OBJECT_CLASS_TYPE (object_class),
		G_SIGNAL_RUN_FIRST,
		G_STRUCT_OFFSET (EShellClass, prepare_for_quit),
		NULL, NULL,
		g_cclosure_marshal_VOID__OBJECT,
		G_TYPE_NONE, 1,
		E_TYPE_ACTIVITY);

	/**
	 * EShell::quit-requested
	 * @shell: the #EShell which emitted the signal
	 * @reason: the reason for quitting
	 *
	 * Emitted when the user elects to quit the application, before
	 * #EShell::prepare-for-quit.
	 *
	 * #EShellBackend<!-- -->s and editor windows can listen for
	 * this signal to prompt the user to save changes or finish
	 * scheduled operations immediately (such as sending mail in
	 * Outbox).  If the user elects to cancel, the signal handler
	 * should call e_shell_cancel_quit() to abort the quit.
	 **/
	signals[QUIT_REQUESTED] = g_signal_new (
		"quit-requested",
		G_OBJECT_CLASS_TYPE (object_class),
		G_SIGNAL_RUN_FIRST,
		G_STRUCT_OFFSET (EShellClass, quit_requested),
		NULL, NULL,
		g_cclosure_marshal_VOID__ENUM,
		G_TYPE_NONE, 1,
		E_TYPE_SHELL_QUIT_REASON);

	/**
	 * EShell::send-receive
	 * @shell: the #EShell which emitted the signal
	 * @parent: a parent #GtkWindow
	 *
	 * Emitted when the user chooses the "Send / Receive" action.
	 * The parent window can be used for showing transient windows.
	 **/
	signals[SEND_RECEIVE] = g_signal_new (
		"send-receive",
		G_OBJECT_CLASS_TYPE (object_class),
		G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
		G_STRUCT_OFFSET (EShellClass, send_receive),
		NULL, NULL,
		g_cclosure_marshal_VOID__OBJECT,
		G_TYPE_NONE, 1,
		GTK_TYPE_WINDOW);

	/**
	 * EShell::window-created
	 * @shell: the #EShell which emitted the signal
	 * @window: the newly created #GtkWindow
	 *
	 * Emitted when @shell begins watching a newly created window.
	 **/
	signals[WINDOW_CREATED] = g_signal_new (
		"window-created",
		G_OBJECT_CLASS_TYPE (object_class),
		G_SIGNAL_RUN_LAST,
		G_STRUCT_OFFSET (EShellClass, window_created),
		NULL, NULL,
		g_cclosure_marshal_VOID__OBJECT,
		G_TYPE_NONE, 1,
		GTK_TYPE_WINDOW);

	/**
	 * EShell::window-destroyed
	 * @shell: the #EShell which emitted the signal
	 *
	 * Emitted when a watched is destroyed.
	 **/
	signals[WINDOW_DESTROYED] = g_signal_new (
		"window-destroyed",
		G_OBJECT_CLASS_TYPE (object_class),
		G_SIGNAL_RUN_LAST,
		G_STRUCT_OFFSET (EShellClass, window_destroyed),
		NULL, NULL,
		g_cclosure_marshal_VOID__VOID,
		G_TYPE_NONE, 0);
}

static void
e_shell_init (EShell *shell)
{
	GHashTable *backends_by_name;
	GHashTable *backends_by_scheme;
	GtkIconTheme *icon_theme;
	EggSMClient *sm_client;

	shell->priv = E_SHELL_GET_PRIVATE (shell);

	backends_by_name = g_hash_table_new (g_str_hash, g_str_equal);
	backends_by_scheme = g_hash_table_new (g_str_hash, g_str_equal);

	shell->priv->settings = g_object_new (E_TYPE_SHELL_SETTINGS, NULL);
	shell->priv->gconf_client = gconf_client_get_default ();
	shell->priv->preferences_window = e_preferences_window_new (shell);
	shell->priv->backends_by_name = backends_by_name;
	shell->priv->backends_by_scheme = backends_by_scheme;
	shell->priv->safe_mode = e_file_lock_exists ();

	shell->priv->startup_view = NULL;

	g_object_ref_sink (shell->priv->preferences_window);

	/* Add our icon directory to the theme's search path
	 * here instead of in main() so Anjal picks it up. */
	icon_theme = gtk_icon_theme_get_default ();
	gtk_icon_theme_append_search_path (icon_theme, EVOLUTION_ICONDIR);

	shell_parse_debug_string (shell);

	g_signal_connect (
		shell, "notify::online",
		G_CALLBACK (shell_notify_online_cb), NULL);

	/* XXX Do this after creating the EShellSettings instance,
	 *     otherwise the GConf bindings will not get set up. */

	e_shell_settings_install_property_for_key (
		"file-chooser-folder",
		"/apps/evolution/shell/file_chooser_folder");

	e_shell_settings_install_property_for_key (
		"start-offline",
		"/apps/evolution/shell/start_offline");

#ifndef G_OS_WIN32
	e_shell_settings_install_property_for_key (
		"disable-application-handlers",
		"/desktop/gnome/lockdown/disable_application_handlers");

	e_shell_settings_install_property_for_key (
		"disable-command-line",
		"/desktop/gnome/lockdown/disable_command_line");

	e_shell_settings_install_property_for_key (
		"disable-printing",
		"/desktop/gnome/lockdown/disable_printing");

	e_shell_settings_install_property_for_key (
		"disable-print-setup",
		"/desktop/gnome/lockdown/disable_print_setup");

	e_shell_settings_install_property_for_key (
		"disable-save-to-disk",
		"/desktop/gnome/lockdown/disable_save_to_disk");
#endif /* G_OS_WIN32 */

	/*** Session Management ***/

	sm_client = egg_sm_client_get ();

	/* Not participating in session saving yet. */
	egg_sm_client_set_mode (EGG_SM_CLIENT_MODE_NO_RESTART);

	g_signal_connect_swapped (
		sm_client, "quit-requested",
		G_CALLBACK (shell_sm_quit_requested_cb), shell);

	g_signal_connect_swapped (
		sm_client, "quit-cancelled",
		G_CALLBACK (shell_sm_quit_cancelled_cb), shell);

	g_signal_connect_swapped (
		sm_client, "quit",
		G_CALLBACK (shell_sm_quit_cb), shell);
}

/**
 * e_shell_get_default:
 *
 * Returns the #EShell created by <function>main()</function>.
 *
 * Try to obtain the #EShell from elsewhere if you can.  This function
 * is intended as a temporary workaround for when that proves difficult.
 *
 * Returns: the #EShell singleton
 **/
EShell *
e_shell_get_default (void)
{
	return default_shell;
}

/**
 * e_shell_load_modules:
 * @shell: an #EShell
 *
 * Loads all installed modules and performs some internal bookkeeping.
 * This function should be called after creating the #EShell instance
 * but before initiating migration or starting the main loop.
 **/
void
e_shell_load_modules (EShell *shell)
{
	const gchar *module_directory;
	GList *list;

	g_return_if_fail (E_IS_SHELL (shell));

	if (shell->priv->modules_loaded)
		return;

	/* Load all shared library modules. */

	module_directory = e_shell_get_module_directory (shell);
	g_return_if_fail (module_directory != NULL);

	list = e_module_load_all_in_directory (module_directory);
	g_list_foreach (list, (GFunc) g_type_module_unuse, NULL);
	g_list_free (list);

	/* Process shell backends. */

	list = g_list_sort (
		e_extensible_list_extensions (
		E_EXTENSIBLE (shell), E_TYPE_SHELL_BACKEND),
		(GCompareFunc) e_shell_backend_compare);
	g_list_foreach (list, (GFunc) shell_process_backend, shell);
	shell->priv->loaded_backends = list;

	shell->priv->modules_loaded = TRUE;
}

/**
 * e_shell_get_shell_backends:
 * @shell: an #EShell
 *
 * Returns a list of loaded #EShellBackend instances.  The list is
 * owned by @shell and should not be modified or freed.
 *
 * Returns: a list of loaded #EShellBackend instances
 **/
GList *
e_shell_get_shell_backends (EShell *shell)
{
	g_return_val_if_fail (E_IS_SHELL (shell), NULL);

	return shell->priv->loaded_backends;
}

/**
 * e_shell_get_canonical_name:
 * @shell: an #EShell
 * @name: the name or alias of an #EShellBackend
 *
 * Returns the canonical name for the #EShellBackend whose name or alias
 * is @name.
 *
 * Returns: the canonical #EShellBackend name
 **/
const gchar *
e_shell_get_canonical_name (EShell *shell,
                            const gchar *name)
{
	EShellBackend *shell_backend;

	g_return_val_if_fail (E_IS_SHELL (shell), NULL);

	/* Handle NULL name arguments silently. */
	if (name == NULL)
		return NULL;

	shell_backend = e_shell_get_backend_by_name (shell, name);

	if (shell_backend == NULL)
		return NULL;

	return E_SHELL_BACKEND_GET_CLASS (shell_backend)->name;
}

/**
 * e_shell_get_backend_by_name:
 * @shell: an #EShell
 * @name: the name or alias of an #EShellBackend
 *
 * Returns the corresponding #EShellBackend for the given name or alias,
 * or %NULL if @name is not recognized.
 *
 * Returns: the #EShellBackend named @name, or %NULL
 **/
EShellBackend *
e_shell_get_backend_by_name (EShell *shell,
                             const gchar *name)
{
	GHashTable *hash_table;

	g_return_val_if_fail (E_IS_SHELL (shell), NULL);
	g_return_val_if_fail (name != NULL, NULL);

	hash_table = shell->priv->backends_by_name;

	return g_hash_table_lookup (hash_table, name);
}

/**
 * e_shell_get_backend_by_scheme:
 * @shell: an #EShell
 * @scheme: a URI scheme
 *
 * Returns the #EShellBackend that implements the given URI scheme,
 * or %NULL if @scheme is not recognized.
 *
 * Returns: the #EShellBackend that implements @scheme, or %NULL
 **/
EShellBackend *
e_shell_get_backend_by_scheme (EShell *shell,
                               const gchar *scheme)
{
	GHashTable *hash_table;

	g_return_val_if_fail (E_IS_SHELL (shell), NULL);
	g_return_val_if_fail (scheme != NULL, NULL);

	hash_table = shell->priv->backends_by_scheme;

	return g_hash_table_lookup (hash_table, scheme);
}
/**
 * e_shell_get_shell_settings:
 * @shell: an #EShell
 *
 * Returns the #EShellSettings instance for @shell.
 *
 * Returns: the #EShellSettings instance for @shell
 **/
EShellSettings *
e_shell_get_shell_settings (EShell *shell)
{
	g_return_val_if_fail (E_IS_SHELL (shell), NULL);

	return shell->priv->settings;
}

/**
 * e_shell_get_gconf_client:
 * @shell: an #EShell
 *
 * Returns the default #GConfClient.  This function is purely for
 * convenience.  The @shell owns the reference so you don't have to.
 *
 * Returns: the default #GConfClient
 **/
GConfClient *
e_shell_get_gconf_client (EShell *shell)
{
	g_return_val_if_fail (E_IS_SHELL (shell), NULL);

	return shell->priv->gconf_client;
}

/**
 * e_shell_create_shell_window:
 * @shell: an #EShell
 * @view_name: name of the initial shell view, or %NULL
 *
 * Creates a new #EShellWindow and emits the #EShell::window-created
 * signal.  Use this function instead of e_shell_window_new() so that
 * @shell can track the window.
 *
 * Returns: a new #EShellWindow
 **/
GtkWidget *
e_shell_create_shell_window (EShell *shell,
                             const gchar *view_name)
{
	GtkWidget *shell_window;
	UniqueMessageData *data;
	UniqueApp *app;

	g_return_val_if_fail (E_IS_SHELL (shell), NULL);

	app = UNIQUE_APP (shell);

	if (unique_app_is_running (app))
		goto unique;

	view_name = e_shell_get_canonical_name (shell, view_name);

	/* EShellWindow initializes its active view from a GConf key,
	 * so set the key ahead of time to control the intial view. */
	if (view_name != NULL) {
		GConfClient *client;
		const gchar *key;
		GError *error = NULL;

		client = e_shell_get_gconf_client (shell);
		key = "/apps/evolution/shell/view_defaults/component_id";
		gconf_client_set_string (client, key, view_name, &error);

		if (error != NULL) {
			g_warning ("%s", error->message);
			g_error_free (error);
		}
	}

	shell_window = e_shell_window_new (
		shell,
		shell->priv->safe_mode,
		shell->priv->geometry);

	/* Clear the first-time-only options. */
	shell->priv->safe_mode = FALSE;
	g_free (shell->priv->geometry);
	shell->priv->geometry = NULL;

	gtk_widget_show (shell_window);

	return shell_window;

unique:  /* Send a message to the other Evolution process. */

	/* XXX Do something with UniqueResponse? */

	if (view_name != NULL) {
		data = unique_message_data_new ();
		unique_message_data_set_text (data, view_name, -1);
		unique_app_send_message (app, UNIQUE_NEW, data);
		unique_message_data_free (data);
	} else
		unique_app_send_message (app, UNIQUE_ACTIVATE, NULL);

	return NULL;
}

/**
 * e_shell_handle_uris:
 * @shell: an #EShell
 * @uris: %NULL-terminated list of URIs
 * @do_import: request an import of the URIs
 *
 * Emits the #EShell::handle-uri signal for each URI.
 *
 * Returns: the number of URIs successfully handled
 **/
guint
e_shell_handle_uris (EShell *shell,
                     gchar **uris,
                     gboolean do_import)
{
	UniqueApp *app;
	UniqueMessageData *data;
	guint n_handled = 0;
	gint ii;

	g_return_val_if_fail (E_IS_SHELL (shell), FALSE);
	g_return_val_if_fail (uris != NULL, FALSE);

	app = UNIQUE_APP (shell);

	if (unique_app_is_running (app))
		goto unique;

	if (do_import) {
		n_handled = e_shell_utils_import_uris (shell, uris);
	} else {
		for (ii = 0; uris[ii] != NULL; ii++) {
			gboolean handled;

			g_signal_emit (
				shell, signals[HANDLE_URI],
				0, uris[ii], &handled);
			n_handled += handled ? 1 : 0;
		}

		if (n_handled == 0)
			n_handled = e_shell_utils_import_uris (shell, uris);
	}

	return n_handled;

unique:  /* Send a message to the other Evolution process. */

	/* XXX Do something with UniqueResponse? */

	data = unique_message_data_new ();
	if (do_import) {
		GPtrArray *arr = g_ptr_array_new ();

		g_ptr_array_add (arr, (gpointer)"--import");

		for (ii = 0; uris[ii] != NULL; ii++) {
			g_ptr_array_add (arr, uris[ii]);
		}

		g_ptr_array_add (arr, NULL);

		unique_message_data_set_uris (data, (gchar **)arr->pdata);

		g_ptr_array_free (arr, TRUE);
	} else {
		unique_message_data_set_uris (data, uris);
	}
	unique_app_send_message (app, UNIQUE_OPEN, data);
	unique_message_data_free (data);

	/* As far as we're concerned, all URIs have been handled. */

	return g_strv_length (uris);
}

/**
 * e_shell_watch_window:
 * @shell: an #EShell
 * @window: a #GtkWindow
 *
 * Makes @shell "watch" a newly created toplevel window, and emits the
 * #EShell::window-created signal.  All #EShellWindow<!-- -->s should be
 * watched, along with any editor or viewer windows that may be shown in
 * response to e_shell_handle_uris().  When the last watched window is
 * closed, Evolution terminates.
 **/
void
e_shell_watch_window (EShell *shell,
                      GtkWindow *window)
{
	GList *list;
	gchar *role;

	g_return_if_fail (E_IS_SHELL (shell));
	g_return_if_fail (GTK_IS_WINDOW (window));

	list = shell->priv->watched_windows;

	/* Ignore duplicates. */
	if (g_list_find (list, window) != NULL)
		return;

	list = g_list_prepend (list, window);
	shell->priv->watched_windows = list;

	unique_app_watch_window (UNIQUE_APP (shell), window);

	/* We use the window's own type name and memory
	 * address to form a unique window role for X11. */
	role = g_strdup_printf (
		"%s-%" G_GINTPTR_FORMAT,
		G_OBJECT_TYPE_NAME (window),
		(gintptr) window);
	gtk_window_set_role (window, role);
	g_free (role);

	g_signal_connect_swapped (
		window, "delete-event",
		G_CALLBACK (shell_window_delete_event_cb), shell);

	g_signal_connect_swapped (
		window, "focus-in-event",
		G_CALLBACK (shell_window_focus_in_event_cb), shell);

	g_object_weak_ref (
		G_OBJECT (window), (GWeakNotify)
		shell_window_weak_notify_cb, shell);

	g_signal_emit (shell, signals[WINDOW_CREATED], 0, window);
}

/**
 * e_shell_get_watched_windows:
 * @shell: an #EShell
 *
 * Returns a list of windows being watched by @shell.  The list is sorted
 * by the most recently focused window, such that the first instance is the
 * currently focused window.  (Useful for choosing a parent for a transient
 * window.)  The list is owned by @shell and should not be modified or freed.
 *
 * Returns: a list of watched windows
 **/
GList *
e_shell_get_watched_windows (EShell *shell)
{
	g_return_val_if_fail (E_IS_SHELL (shell), NULL);

	return shell->priv->watched_windows;
}

/**
 * e_shell_get_active_window:
 * @shell: an #EShell or %NULL to use the default shell
 *
 * Returns the most recently focused watched window, according to
 * e_shell_get_watched_windows().  Convenient for finding a parent
 * for a transient window.
 *
 * Note the returned window is not necessarily an #EShellWindow.
 *
 * Returns: the most recently focused watched window
 **/
GtkWindow *
e_shell_get_active_window (EShell *shell)
{
	GList *watched_windows;

	if (shell == NULL)
		shell = e_shell_get_default ();

	g_return_val_if_fail (E_IS_SHELL (shell), NULL);

	watched_windows = e_shell_get_watched_windows (shell);

	if (!watched_windows)
		return NULL;

	/* Sanity check */
	g_return_val_if_fail (GTK_IS_WINDOW (watched_windows->data), NULL);

	return GTK_WINDOW (watched_windows->data);
}

/**
 * e_shell_send_receive:
 * @shell: an #EShell
 * @parent: the parent #GtkWindow
 *
 * Emits the #EShell::send-receive signal.
 **/
void
e_shell_send_receive (EShell *shell,
                      GtkWindow *parent)
{
	g_return_if_fail (E_IS_SHELL (shell));
	g_return_if_fail (GTK_IS_WINDOW (parent));

	g_signal_emit (shell, signals[SEND_RECEIVE], 0, parent);
}

/**
 * e_shell_get_express_mode:
 * @shell: an #EShell
 *
 * Returns %TRUE if Evolution is in express mode.
 *
 * Returns: %TRUE if Evolution is in express mode
 **/
gboolean
e_shell_get_express_mode (EShell *shell)
{
	g_return_val_if_fail (E_IS_SHELL (shell), FALSE);

	return shell->priv->express_mode;
}

/**
 * e_shell_get_meego_mode:
 * @shell: an #EShell
 *
 * Returns %TRUE if Evolution is in MeeGo mode.
 *
 * Returns: %TRUE if Evolution is in MeeGo mode
 **/
gboolean
e_shell_get_meego_mode (EShell *shell)
{
	g_return_val_if_fail (E_IS_SHELL (shell), FALSE);

	return shell->priv->meego_mode;
}

/**
 * e_shell_get_small_screen_mode:
 * @shell: an #EShell
 *
 * Returns %TRUE if Evolution is in small (netbook) screen mode.
 *
 * Returns: %TRUE if Evolution is in small screen mode
 **/
gboolean
e_shell_get_small_screen_mode (EShell *shell)
{
	g_return_val_if_fail (E_IS_SHELL (shell), FALSE);

	return shell->priv->small_screen_mode;
}

/**
 * e_shell_get_module_directory:
 * @shell: an #EShell
 *
 * Returns the directory from which #EModule<!-- -->s were loaded.
 *
 * Returns: the #EModule directory
 **/
const gchar *
e_shell_get_module_directory (EShell *shell)
{
	g_return_val_if_fail (E_IS_SHELL (shell), NULL);

	return shell->priv->module_directory;
}

/**
 * e_shell_get_network_available:
 * @shell: an #EShell
 *
 * Returns %TRUE if a network is available.
 *
 * Returns: %TRUE if a network is available
 **/
gboolean
e_shell_get_network_available (EShell *shell)
{
	g_return_val_if_fail (E_IS_SHELL (shell), FALSE);

	return shell->priv->network_available;
}

/**
 * e_shell_set_network_available:
 * @shell: an #EShell
 * @network_available: whether a network is available
 *
 * Sets whether a network is available.  This is usually called in
 * response to a status change signal from NetworkManager.  If the
 * network becomes unavailable while #EShell:online is %TRUE, the
 * @shell will force #EShell:online to %FALSE until the network
 * becomes available again.
 **/
void
e_shell_set_network_available (EShell *shell,
                               gboolean network_available)
{
	g_return_if_fail (E_IS_SHELL (shell));

	if (network_available == shell->priv->network_available)
		return;

	shell->priv->network_available = network_available;
	g_object_notify (G_OBJECT (shell), "network-available");

	/* If we're being forced offline, perhaps due to a network outage,
	 * reconnect automatically when the network becomes available. */
	if (!network_available && shell->priv->online) {
		g_message ("Network disconnected.  Forced offline.");
		e_shell_set_online (shell, FALSE);
		shell->priv->auto_reconnect = TRUE;
	} else if (network_available && shell->priv->auto_reconnect) {
		g_message ("Connection established.  Going online.");
		e_shell_set_online (shell, TRUE);
		shell->priv->auto_reconnect = FALSE;
	}
}

/**
 * e_shell_get_online:
 * @shell: an #EShell
 *
 * Returns %TRUE if Evolution is online, %FALSE if Evolution is offline.
 * Evolution may be offline because the user elected to work offline, or
 * because the network has become unavailable.
 *
 * Returns: %TRUE if Evolution is online
 **/
gboolean
e_shell_get_online (EShell *shell)
{
	g_return_val_if_fail (E_IS_SHELL (shell), FALSE);

	return shell->priv->online;
}

/**
 * e_shell_set_online:
 * @shell: an #EShell
 * @online: %TRUE to go online, %FALSE to go offline
 *
 * Asynchronously places Evolution in online or offline mode.
 **/
void
e_shell_set_online (EShell *shell,
                    gboolean online)
{
	g_return_if_fail (E_IS_SHELL (shell));

	if (online == shell->priv->online)
		return;

	if (online)
		shell_prepare_for_online (shell);
	else
		shell_prepare_for_offline (shell);
}

/**
 * e_shell_get_preferences_window:
 * @shell: an #EShell
 *
 * Returns the Evolution Preferences window.
 *
 * Returns: the preferences window
 **/
GtkWidget *
e_shell_get_preferences_window (EShell *shell)
{
	g_return_val_if_fail (E_IS_SHELL (shell), NULL);

	return shell->priv->preferences_window;
}

/**
 * e_shell_event:
 * @shell: an #EShell
 * @event_name: the name of the event
 * @event_data: data associated with the event
 *
 * The #EShell::event signal acts as a cheap mechanism for broadcasting
 * events to the rest of the application, such as new mail arriving.  The
 * @event_name is used as the signal detail, and @event_data may point to
 * an object or data structure associated with the event.
 **/
void
e_shell_event (EShell *shell,
               const gchar *event_name,
               gpointer event_data)
{
	GQuark detail;

	g_return_if_fail (E_IS_SHELL (shell));
	g_return_if_fail (event_name != NULL);

	detail = g_quark_from_string (event_name);
	g_signal_emit (shell, signals[EVENT], detail, event_data);
}

/**
 * e_shell_quit:
 * @shell: an #EShell
 * @reason: the reason for quitting
 *
 * Requests an application shutdown.  This happens in two phases: the
 * first is synchronous, the second is asynchronous.
 *
 * In the first phase, the @shell emits a #EShell::quit-requested signal
 * to potentially give the user a chance to cancel shutdown.  If the user
 * cancels shutdown, the function returns %FALSE.  Otherwise it proceeds
 * into the second phase.
 *
 * In the second phase, the @shell emits a #EShell::prepare-for-quit
 * signal and immediately returns %TRUE.  Signal handlers may delay the
 * actual application shutdown while they clean up resources, but there
 * is no way to cancel shutdown at this point.
 *
 * Consult the documentation for these two signals for details on how
 * to handle them.
 *
 * Returns: %TRUE if shutdown is underway, %FALSE if it was cancelled
 **/
gboolean
e_shell_quit (EShell *shell,
              EShellQuitReason reason)
{
	UniqueApp *app;
	UniqueResponse response;

	g_return_val_if_fail (E_IS_SHELL (shell), FALSE);

	app = UNIQUE_APP (shell);

	if (unique_app_is_running (app))
		goto unique;

	if (!shell_request_quit (shell, reason))
		return FALSE;

	shell_prepare_for_quit (shell);

	return TRUE;

unique:  /* Send a message to the other Evolution process. */

	response = unique_app_send_message (app, UNIQUE_CLOSE, NULL);

	return (response == UNIQUE_RESPONSE_OK);
}

/**
 * e_shell_cancel_quit:
 * @shell: an #EShell
 *
 * This function may only be called from #EShell::quit-requested signal
 * handlers to prevent Evolution from quitting.  Calling this will stop
 * further emission of the #EShell::quit-requested signal.
 *
 * Note: This function has no effect during a #EShell::prepare-for-quit
 * signal emission.
 **/
void
e_shell_cancel_quit (EShell *shell)
{
	g_return_if_fail (E_IS_SHELL (shell));

	shell->priv->quit_cancelled = TRUE;

	g_signal_stop_emission (shell, signals[QUIT_REQUESTED], 0);
}

/**
 * e_shell_adapt_window_size:
 * @shell: an #EShell
 * @window: a #GtkWindow to adapt to full-screen
 *
 * This is used to adapt to window's size to be optimal for
 * the platform. The shell settings are used to determine if
 * a window should be set to full screen etc.
 *
 * This method is best called when the widget is realized on
 * a given screen.
 **/
void
e_shell_adapt_window_size (EShell *shell,
                           GtkWindow *window)
{
	GdkScreen *screen;
	GdkRectangle rect;
	GdkWindow *gdk_window;
	gint monitor;

	if (!e_shell_get_meego_mode (shell) ||
	    !e_shell_get_small_screen_mode (shell))
		return;

	screen = gdk_screen_get_default ();
	gdk_window = gtk_widget_get_window (GTK_WIDGET (window));
	monitor = gdk_screen_get_monitor_at_window (screen, gdk_window);
	gdk_screen_get_monitor_geometry (screen, monitor, &rect);

	gtk_window_set_default_size (window, rect.width, rect.height);
	gtk_window_set_decorated (window, FALSE);
	gtk_window_maximize (window);
}

void
e_shell_set_startup_view (EShell *shell,
                          const gchar *view)
{
	g_return_if_fail (E_IS_SHELL (shell));

	shell->priv->startup_view = g_strdup (view);
}

const gchar *
e_shell_get_startup_view (EShell *shell)
{
	g_return_val_if_fail (E_IS_SHELL (shell), NULL);

	return shell->priv->startup_view;
}
