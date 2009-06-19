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

#include "e-shell.h"

#include <glib/gi18n.h>
#include <libedataserverui/e-passwords.h>

#include "e-util/e-util.h"
#include "e-util/e-module.h"
#include "widgets/misc/e-preferences-window.h"

#include "e-shell-backend.h"
#include "e-shell-migrate.h"
#include "e-shell-window.h"

#define SHUTDOWN_TIMEOUT	500  /* milliseconds */

#define E_SHELL_GET_PRIVATE(obj) \
	(G_TYPE_INSTANCE_GET_PRIVATE \
	((obj), E_TYPE_SHELL, EShellPrivate))

struct _EShellPrivate {
	GList *watched_windows;
	EShellSettings *settings;
	GConfClient *gconf_client;
	GtkWidget *preferences_window;

	/* Shell Backends */
	GList *loaded_backends;
	GHashTable *backends_by_name;
	GHashTable *backends_by_scheme;

	gpointer preparing_for_line_change;  /* weak pointer */

	guint auto_reconnect	: 1;
	guint network_available	: 1;
	guint online		: 1;
	guint safe_mode		: 1;
};

enum {
	PROP_0,
	PROP_NETWORK_AVAILABLE,
	PROP_ONLINE,
	PROP_SHELL_SETTINGS
};

enum {
	EVENT,
	HANDLE_URI,
	PREPARE_FOR_OFFLINE,
	PREPARE_FOR_ONLINE,
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

EShell *default_shell = NULL;
static gpointer parent_class;
static guint signals[LAST_SIGNAL];

#if NM_SUPPORT
void e_shell_dbus_initialize (EShell *shell);
#endif

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

	/* Otherwise we initiate application shutdown. */
	return !e_shell_quit (shell);
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

static void
shell_window_weak_notify_cb (EShell *shell,
                             GObject *where_the_object_was)
{
	GList *list;

	list = shell->priv->watched_windows;
	list = g_list_remove (list, where_the_object_was);
	shell->priv->watched_windows = list;

	g_signal_emit (shell, signals[WINDOW_DESTROYED], 0);
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

	g_message ("Offline preparations complete.");
}

static void
shell_prepare_for_offline (EShell *shell)
{
	/* Are preparations already in progress? */
	if (shell->priv->preparing_for_line_change != NULL)
		return;

	g_message ("Preparing for offline mode...");

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

	g_message ("Online preparations complete.");
}

static void
shell_prepare_for_online (EShell *shell)
{
	/* Are preparations already in progress? */
	if (shell->priv->preparing_for_line_change != NULL)
		return;

	g_message ("Preparing for online mode...");

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
shell_load_modules (EShell *shell)
{
	GList *modules;

	/* Load all shared library modules. */
	modules = e_module_load_all_in_directory (EVOLUTION_MODULEDIR);

	while (modules != NULL) {
		g_type_module_unuse (G_TYPE_MODULE (modules->data));
		modules = g_list_delete_link (modules, modules);
	}
}

/* Helper for shell_process_backend() */
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
shell_process_backend (EShell *shell,
                       EShellBackend *shell_backend)
{
	EShellBackendClass *class;
	GHashTable *backends_by_name;
	GHashTable *backends_by_scheme;
	const gchar *string;

	shell->priv->loaded_backends = g_list_insert_sorted (
		shell->priv->loaded_backends, shell_backend,
		(GCompareFunc) e_shell_backend_compare);

	/* Bookkeeping */

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
shell_create_backends (EShell *shell)
{
	GType *children;
	guint ii, n_children;

	/* Create an instance of each EShellBackend subclass. */
	children = g_type_children (E_TYPE_SHELL_BACKEND, &n_children);

	for (ii = 0; ii < n_children; ii++) {
		EShellBackend *shell_backend;
		GType type = children[ii];

		shell_backend = g_object_new (type, "shell", shell, NULL);
		shell_process_backend (shell, shell_backend);
	}

	g_free (children);
}

static gboolean
shell_shutdown_timeout (EShell *shell)
{
	GList *list, *iter;
	gboolean proceed = TRUE;
	static guint source_id = 0;
	static guint message_timer = 1;

	/* Module list is read-only; do not free. */
	list = e_shell_get_shell_backends (shell);

	/* Any backend can defer shutdown if it's still busy. */
	for (iter = list; proceed && iter != NULL; iter = iter->next) {
		EShellBackend *shell_backend = iter->data;
		proceed = e_shell_backend_shutdown (shell_backend);

		/* Emit a message every few seconds to indicate
		 * which backend(s) we're still waiting on. */
		if (proceed || message_timer == 0)
			continue;

		g_message (
			_("Waiting for the \"%s\" backend to finish..."),
			E_SHELL_BACKEND_GET_CLASS (shell_backend)->name);
	}

	message_timer = (message_timer + 1) % 10;

	/* If we're go for shutdown, destroy all shell windows.  Note,
	 * we iterate over a /copy/ of the active windows list because
	 * the act of destroying a shell window will modify the active
	 * windows list, which would otherwise derail the iteration. */
	if (proceed) {
		list = g_list_copy (shell->priv->watched_windows);
		g_list_foreach (list, (GFunc) gtk_widget_destroy, NULL);
		g_list_free (list);

	/* If a backend is still busy, try again after a short delay. */
	} else if (source_id == 0)
		source_id = g_timeout_add (
			SHUTDOWN_TIMEOUT, (GSourceFunc)
			shell_shutdown_timeout, shell);

	/* Return TRUE to repeat the timeout, FALSE to stop it.  This
	 * may seem backwards if the function was called directly. */
	return !proceed;
}

static void
shell_set_property (GObject *object,
                    guint property_id,
                    const GValue *value,
                    GParamSpec *pspec)
{
	switch (property_id) {
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

	g_list_foreach (priv->loaded_backends, (GFunc) g_object_unref, NULL);
	g_list_free (priv->loaded_backends);
	priv->loaded_backends = NULL;

	if (priv->preparing_for_line_change != NULL) {
		g_object_remove_weak_pointer (
			G_OBJECT (priv->preparing_for_line_change),
			&priv->preparing_for_line_change);
	}

	/* Chain up to parent's dispose() method. */
	G_OBJECT_CLASS (parent_class)->dispose (object);
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

	/* Chain up to parent's finalize() method. */
	G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
shell_constructed (GObject *object)
{
	/* UniqueApp will have by this point determined whether we're
	 * the only Evolution process running.  If so, proceed normally.
	 * Otherwise we just issue commands to the other process. */
	if (unique_app_is_running (UNIQUE_APP (object)))
		return;

	e_file_lock_create ();

	shell_load_modules (E_SHELL (object));
	shell_create_backends (E_SHELL (object));
	e_shell_migrate_attempt (E_SHELL (object));
}

static gboolean
shell_message_handle_new (EShell *shell,
                          UniqueMessageData *data)
{
	gchar *view_name;

	view_name = unique_message_data_get_text (data);
	e_shell_create_shell_window (shell, view_name);
	g_free (view_name);

	return TRUE;
}

static gboolean
shell_message_handle_open (EShell *shell,
                           UniqueMessageData *data)
{
	gchar **uris;

	uris = unique_message_data_get_uris (data);
	e_shell_handle_uris (shell, uris);
	g_strfreev (uris);

	return TRUE;
}

static gboolean
shell_message_handle_close (EShell *shell,
                            UniqueMessageData *data)
{
	e_shell_quit (shell);

	return TRUE;
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
			break;  /* use the default behavior */

		case UNIQUE_NEW:
			if (shell_message_handle_new (shell, data))
				return UNIQUE_RESPONSE_OK;
			break;

		case UNIQUE_OPEN:
			if (shell_message_handle_open (shell, data))
				return UNIQUE_RESPONSE_OK;
			break;

		case UNIQUE_CLOSE:
			if (shell_message_handle_close (shell, data))
				return UNIQUE_RESPONSE_OK;
			break;

		default:
			break;
	}

	/* Chain up to parent's message_received() method. */
	return UNIQUE_APP_CLASS (parent_class)->
		message_received (app, command, data, time_);
}

static void
shell_class_init (EShellClass *class)
{
	GObjectClass *object_class;
	UniqueAppClass *unique_app_class;

	parent_class = g_type_class_peek_parent (class);
	g_type_class_add_private (class, sizeof (EShellPrivate));

	object_class = G_OBJECT_CLASS (class);
	object_class->set_property = shell_set_property;
	object_class->get_property = shell_get_property;
	object_class->dispose = shell_dispose;
	object_class->finalize = shell_finalize;
	object_class->constructed = shell_constructed;

	unique_app_class = UNIQUE_APP_CLASS (class);
	unique_app_class->message_received = shell_message_received;

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
			_("Network Available"),
			_("Whether the network is available"),
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
			_("Online"),
			_("Whether the shell is online"),
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
			_("Shell Settings"),
			_("Application-wide settings"),
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
		0, g_signal_accumulator_true_handled, NULL,
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
		0, NULL, NULL,
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
		0, NULL, NULL,
		g_cclosure_marshal_VOID__OBJECT,
		G_TYPE_NONE, 1,
		E_TYPE_ACTIVITY);

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
		0, NULL, NULL,
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
		0, NULL, NULL,
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
		0, NULL, NULL,
		g_cclosure_marshal_VOID__VOID,
		G_TYPE_NONE, 0);

	/* Install some application-wide settings. */

	e_shell_settings_install_property (
		g_param_spec_boolean (
			"disable-application-handlers",
			NULL,
			NULL,
			FALSE,
			G_PARAM_READWRITE));

	e_shell_settings_install_property (
		g_param_spec_boolean (
			"disable-command-line",
			NULL,
			NULL,
			FALSE,
			G_PARAM_READWRITE));

	e_shell_settings_install_property (
		g_param_spec_boolean (
			"disable-printing",
			NULL,
			NULL,
			FALSE,
			G_PARAM_READWRITE));

	e_shell_settings_install_property (
		g_param_spec_boolean (
			"disable-print-setup",
			NULL,
			NULL,
			FALSE,
			G_PARAM_READWRITE));

	e_shell_settings_install_property (
		g_param_spec_boolean (
			"disable-save-to-disk",
			NULL,
			NULL,
			FALSE,
			G_PARAM_READWRITE));

	e_shell_settings_install_property (
		g_param_spec_string (
			"file-chooser-folder",
			NULL,
			NULL,
			NULL,
			G_PARAM_READWRITE));
}

static void
shell_init (EShell *shell)
{
	GHashTable *backends_by_name;
	GHashTable *backends_by_scheme;

	shell->priv = E_SHELL_GET_PRIVATE (shell);

	backends_by_name = g_hash_table_new (g_str_hash, g_str_equal);
	backends_by_scheme = g_hash_table_new (g_str_hash, g_str_equal);

	shell->priv->settings = g_object_new (E_TYPE_SHELL_SETTINGS, NULL);
	shell->priv->gconf_client = gconf_client_get_default ();
	shell->priv->preferences_window = e_preferences_window_new ();
	shell->priv->backends_by_name = backends_by_name;
	shell->priv->backends_by_scheme = backends_by_scheme;
	shell->priv->safe_mode = e_file_lock_exists ();

	g_object_ref_sink (shell->priv->preferences_window);

#if NM_SUPPORT
	e_shell_dbus_initialize (shell);
#endif

	shell_parse_debug_string (shell);

	g_signal_connect (
		shell, "notify::online",
		G_CALLBACK (shell_notify_online_cb), NULL);

	e_shell_settings_bind_to_gconf (
		shell->priv->settings, "disable-application-handlers",
		"/desktop/gnome/lockdown/disable_application_handlers");

	e_shell_settings_bind_to_gconf (
		shell->priv->settings, "disable-command-line",
		"/desktop/gnome/lockdown/disable_command_line");

	e_shell_settings_bind_to_gconf (
		shell->priv->settings, "disable-printing",
		"/desktop/gnome/lockdown/disable_printing");

	e_shell_settings_bind_to_gconf (
		shell->priv->settings, "disable-print-setup",
		"/desktop/gnome/lockdown/disable_print_setup");

	e_shell_settings_bind_to_gconf (
		shell->priv->settings, "disable-save-to-disk",
		"/desktop/gnome/lockdown/disable_save_to_disk");
}

GType
e_shell_get_type (void)
{
	static GType type = 0;

	if (G_UNLIKELY (type == 0)) {
		const GTypeInfo type_info = {
			sizeof (EShellClass),
			(GBaseInitFunc) NULL,
			(GBaseFinalizeFunc) NULL,
			(GClassInitFunc) shell_class_init,
			(GClassFinalizeFunc) NULL,
			NULL,  /* class_data */
			sizeof (EShell),
			0,     /* n_preallocs */
			(GInstanceInitFunc) shell_init,
			NULL   /* value_table */
		};

		type = g_type_register_static (
			UNIQUE_TYPE_APP, "EShell", &type_info, 0);
	}

	return type;
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
	/* Emit a warning if we call this too early. */
	g_return_val_if_fail (default_shell != NULL, NULL);

	return default_shell;
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

	shell_window = e_shell_window_new (shell, shell->priv->safe_mode);

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
 *
 * Emits the #EShell::handle-uri signal for each URI.
 *
 * Returns: the number of URIs successfully handled
 **/
guint
e_shell_handle_uris (EShell *shell,
                     gchar **uris)
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

	for (ii = 0; uris[ii] != NULL; ii++) {
		gboolean handled;

		g_signal_emit (
			shell, signals[HANDLE_URI],
			0, uris[ii], &handled);
		n_handled += handled ? 1 : 0;
	}

	return n_handled;

unique:  /* Send a message to the other Evolution process. */

	/* XXX Do something with UniqueResponse? */

	data = unique_message_data_new ();
	unique_message_data_set_uris (data, uris);
	unique_app_send_message (app, UNIQUE_OPEN, data);
	unique_message_data_free (data);

	return 0;
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

	g_return_if_fail (E_IS_SHELL (shell));
	g_return_if_fail (GTK_IS_WINDOW (window));

	list = shell->priv->watched_windows;

	/* Ignore duplicates. */
	if (g_list_find (list, window) != NULL)
		return;

	list = g_list_prepend (list, window);
	shell->priv->watched_windows = list;

	unique_app_watch_window (UNIQUE_APP (shell), window);

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

gboolean
e_shell_is_busy (EShell *shell)
{
	/* FIXME */
	return FALSE;
}

gboolean
e_shell_do_quit (EShell *shell)
{
	/* FIXME */
	return TRUE;
}

gboolean
e_shell_quit (EShell *shell)
{
	/* FIXME */
	return TRUE;
}
