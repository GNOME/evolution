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
#include "widgets/misc/e-preferences-window.h"

#include "e-shell-migrate.h"
#include "e-shell-module.h"
#include "e-shell-window.h"

#define SHUTDOWN_TIMEOUT	500  /* milliseconds */

#define E_SHELL_GET_PRIVATE(obj) \
	(G_TYPE_INSTANCE_GET_PRIVATE \
	((obj), E_TYPE_SHELL, EShellPrivate))

struct _EShellPrivate {
	GList *active_windows;
	EShellSettings *settings;

	/* Shell Modules */
	GList *loaded_modules;
	GHashTable *modules_by_name;
	GHashTable *modules_by_scheme;

	gpointer preparing_for_line_change;  /* weak pointer */

	guint auto_reconnect	: 1;
	guint network_available	: 1;
	guint online_mode	: 1;
	guint safe_mode		: 1;
};

enum {
	PROP_0,
	PROP_NETWORK_AVAILABLE,
	PROP_ONLINE_MODE,
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

static gboolean
shell_window_delete_event_cb (EShell *shell,
                              EShellWindow *shell_window)
{
	/* If other windows are open we can safely close this one. */
	if (g_list_length (shell->priv->active_windows) > 1)
		return FALSE;

	/* Otherwise we initiate application shutdown. */
	return !e_shell_quit (shell);
}

static gboolean
shell_window_focus_in_event_cb (EShell *shell,
                                GdkEventFocus *event,
                                EShellWindow *shell_window)
{
	GList *list, *link;

	/* Keep the active windows list sorted by most recently focused,
	 * so the first item in the list should always be the currently
	 * focused shell window. */

	list = shell->priv->active_windows;
	link = g_list_find (list, shell_window);
	g_return_val_if_fail (link != NULL, FALSE);

	if (link != list) {
		list = g_list_remove_link (list, link);
		list = g_list_concat (link, list);
	}

	shell->priv->active_windows = list;

	return FALSE;
}

static void
shell_notify_online_mode_cb (EShell *shell)
{
	gboolean online;

	online = e_shell_get_online_mode (shell);
	e_passwords_set_online (online);
}

static void
shell_window_weak_notify_cb (EShell *shell,
                             GObject *where_the_object_was)
{
	GList *active_windows;

	active_windows = shell->priv->active_windows;
	active_windows = g_list_remove (active_windows, where_the_object_was);
	shell->priv->active_windows = active_windows;

	g_signal_emit (shell, signals[WINDOW_DESTROYED], 0);
}

static void
shell_ready_for_offline (EShell *shell,
                         EActivity *activity,
                         gboolean is_last_ref)
{
	if (!is_last_ref)
		return;

	e_activity_complete (activity);

	g_object_remove_toggle_ref (
		G_OBJECT (activity), (GToggleNotify)
		shell_ready_for_offline, shell);

	shell->priv->online_mode = FALSE;
	g_object_notify (G_OBJECT (shell), "online-mode");

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

	e_activity_complete (activity);

	g_object_remove_toggle_ref (
		G_OBJECT (activity), (GToggleNotify)
		shell_ready_for_online, shell);

	shell->priv->online_mode = TRUE;
	g_object_notify (G_OBJECT (shell), "online-mode");

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

/* Helper for shell_query_module() */
static void
shell_split_and_insert_items (GHashTable *hash_table,
                              const gchar *items,
                              EShellModule *shell_module)
{
	gpointer key;
	gchar **strv;
	gint ii;

	strv = g_strsplit_set (items, ":", -1);

	for (ii = 0; strv[ii] != NULL; ii++) {
		key = (gpointer) g_intern_string (strv[ii]);
		g_hash_table_insert (hash_table, key, shell_module);
	}

	g_strfreev (strv);
}

static void
shell_query_module (EShell *shell,
                    const gchar *filename)
{
	EShellModule *shell_module;
	EShellModuleInfo *info;
	GHashTable *modules_by_name;
	GHashTable *modules_by_scheme;
	const gchar *string;

	shell_module = e_shell_module_new (shell, filename);

	if (!g_type_module_use (G_TYPE_MODULE (shell_module))) {
		g_critical ("Failed to load module: %s", filename);
		g_object_unref (shell_module);
		return;
	}

	shell->priv->loaded_modules = g_list_insert_sorted (
		shell->priv->loaded_modules, shell_module,
		(GCompareFunc) e_shell_module_compare);

	/* Bookkeeping */

	info = (EShellModuleInfo *) shell_module->priv;
	modules_by_name = shell->priv->modules_by_name;
	modules_by_scheme = shell->priv->modules_by_scheme;

	if ((string = info->name) != NULL)
		g_hash_table_insert (
			modules_by_name, (gpointer)
			g_intern_string (string), shell_module);

	if ((string = info->aliases) != NULL)
		shell_split_and_insert_items (
			modules_by_name, string, shell_module);

	if ((string = info->schemes) != NULL)
		shell_split_and_insert_items (
			modules_by_scheme, string, shell_module);
}

static gboolean
shell_shutdown_timeout (EShell *shell)
{
	GList *list, *iter;
	gboolean proceed = TRUE;
	static guint source_id = 0;
	static guint message_timer = 1;

	/* Module list is read-only; do not free. */
	list = e_shell_get_shell_modules (shell);

	/* Any module can defer shutdown if it's still busy. */
	for (iter = list; proceed && iter != NULL; iter = iter->next) {
		EShellModule *shell_module = iter->data;
		proceed = e_shell_module_shutdown (shell_module);

		/* Emit a message every few seconds to indicate
		 * which module(s) we're still waiting on. */
		if (proceed || message_timer == 0)
			continue;

		g_message (
			_("Waiting for the \"%s\" module to finish..."),
			G_TYPE_MODULE (shell_module)->name);
	}

	message_timer = (message_timer + 1) % 10;

	/* If we're go for shutdown, destroy all shell windows.  Note,
	 * we iterate over a /copy/ of the active windows list because
	 * the act of destroying a shell window will modify the active
	 * windows list, which would otherwise derail the iteration. */
	if (proceed) {
		list = g_list_copy (shell->priv->active_windows);
		g_list_foreach (list, (GFunc) gtk_widget_destroy, NULL);
		g_list_free (list);

	/* If a module is still busy, try again after a short delay. */
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

		case PROP_ONLINE_MODE:
			e_shell_set_online_mode (
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

		case PROP_ONLINE_MODE:
			g_value_set_boolean (
				value, e_shell_get_online_mode (
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

	g_list_foreach (
		priv->loaded_modules,
		(GFunc) g_type_module_unuse, NULL);
	g_list_free (priv->loaded_modules);
	priv->loaded_modules = NULL;

	/* Chain up to parent's dispose() method. */
	G_OBJECT_CLASS (parent_class)->dispose (object);
}

static void
shell_finalize (GObject *object)
{
	EShellPrivate *priv;

	priv = E_SHELL_GET_PRIVATE (object);

	g_hash_table_destroy (priv->modules_by_name);
	g_hash_table_destroy (priv->modules_by_scheme);

	/* Chain up to parent's finalize() method. */
	G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
shell_constructed (GObject *object)
{
	GDir *dir;
	EShell *shell;
	const gchar *dirname;
	const gchar *basename;
	GError *error = NULL;

	shell = E_SHELL (object);
	dirname = EVOLUTION_MODULEDIR;

	dir = g_dir_open (dirname, 0, &error);
	if (dir == NULL) {
		g_critical ("%s", error->message);
		g_error_free (error);
		return;
	}

	while ((basename = g_dir_read_name (dir)) != NULL) {
		gchar *filename;

		if (!g_str_has_suffix (basename, "." G_MODULE_SUFFIX))
			continue;

		filename = g_build_filename (dirname, basename, NULL);
		shell_query_module (shell, filename);
		g_free (filename);
	}

	g_dir_close (dir);

	e_shell_migrate_attempt (shell);
}

static void
shell_class_init (EShellClass *class)
{
	GObjectClass *object_class;

	parent_class = g_type_class_peek_parent (class);
	g_type_class_add_private (class, sizeof (EShellPrivate));

	object_class = G_OBJECT_CLASS (class);
	object_class->set_property = shell_set_property;
	object_class->get_property = shell_get_property;
	object_class->dispose = shell_dispose;
	object_class->finalize = shell_finalize;
	object_class->constructed = shell_constructed;

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
	 * EShell:online-mode
	 *
	 * Whether the shell is online.
	 **/
	g_object_class_install_property (
		object_class,
		PROP_ONLINE_MODE,
		g_param_spec_boolean (
			"online-mode",
			_("Online Mode"),
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
	 * way of a command-line argument.  An #EShellModule should listen
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
	 * Emitted when the user elects to work offline.  An #EShellModule
	 * should listen for this signal and make preparations for working
	 * in offline mode.
	 *
	 * If preparations for working offline cannot immediately be
	 * completed (such as when synchronizing with a remote server),
	 * the #EShellModule should reference the @activity until
	 * preparations are complete, and then unreference the @activity.
	 * This will delay Evolution from actually going to offline mode
	 * until all modules have unreferenced @activity.
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
	 * Emitted when the user elects to work online.  An #EShellModule
	 * should listen for this signal and make preparations for working
	 * in online mode.
	 *
	 * If preparations for working online cannot immediately be
	 * completed (such as when re-connecting to a remote server), the
	 * #EShellModule should reference the @activity until preparations
	 * are complete, and then unreference the @activity.  This will
	 * delay Evolution from actually going to online mode until all
	 * modules have unreferenced @activity.
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
	 * @shell_window: the newly created #EShellWindow
	 *
	 * Emitted when a new #EShellWindow is created.
	 **/
	signals[WINDOW_CREATED] = g_signal_new (
		"window-created",
		G_OBJECT_CLASS_TYPE (object_class),
		G_SIGNAL_RUN_LAST,
		0, NULL, NULL,
		g_cclosure_marshal_VOID__OBJECT,
		G_TYPE_NONE, 1,
		E_TYPE_SHELL_WINDOW);

	/**
	 * EShell::window-destroyed
	 * @shell: the #EShell which emitted the signal
	 *
	 * Emitted when an #EShellWindow is destroyed.
	 **/
	signals[WINDOW_DESTROYED] = g_signal_new (
		"window-destroyed",
		G_OBJECT_CLASS_TYPE (object_class),
		G_SIGNAL_RUN_LAST,
		0, NULL, NULL,
		g_cclosure_marshal_VOID__VOID,
		G_TYPE_NONE, 0);

	/* Install some desktop-wide settings. */

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
}

static void
shell_init (EShell *shell)
{
	GHashTable *modules_by_name;
	GHashTable *modules_by_scheme;

	shell->priv = E_SHELL_GET_PRIVATE (shell);

	modules_by_name = g_hash_table_new (g_str_hash, g_str_equal);
	modules_by_scheme = g_hash_table_new (g_str_hash, g_str_equal);

	shell->priv->settings = g_object_new (E_TYPE_SHELL_SETTINGS, NULL);
	shell->priv->modules_by_name = modules_by_name;
	shell->priv->modules_by_scheme = modules_by_scheme;
	shell->priv->safe_mode = e_file_lock_exists ();

#if NM_SUPPORT
	e_shell_dbus_initialize (shell);
#endif

	e_file_lock_create ();

	shell_parse_debug_string (shell);

	g_signal_connect (
		shell, "notify::online-mode",
		G_CALLBACK (shell_notify_online_mode_cb), NULL);

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
			G_TYPE_OBJECT, "EShell", &type_info, 0);
	}

	return type;
}

/**
 * e_shell_get_default:
 *
 * Returns the #EShell created by <function>main()</function>.
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
 * e_shell_get_shell_modules:
 * @shell: an #EShell
 *
 * Returns a list of loaded #EShellModule instances.  The list is
 * owned by @shell and should not be modified or freed.
 *
 * Returns: a list of loaded #EShellModule instances
 **/
GList *
e_shell_get_shell_modules (EShell *shell)
{
	g_return_val_if_fail (E_IS_SHELL (shell), NULL);

	return shell->priv->loaded_modules;
}

/**
 * e_shell_get_shell_windows:
 * @shell: an #EShell
 *
 * Returns a list of active #EShellWindow instances that were created by
 * e_shell_create_shell_window().  The list is sorted by the most recently
 * focused window, such that the first instance is the currently focused
 * window.  (Useful for choosing a parent for a transient window.)  The
 * list is owned by @shell and should not be modified or freed.
 *
 * Returns: a list of active #EShellWindow instances
 **/
GList *
e_shell_get_shell_windows (EShell *shell)
{
	g_return_val_if_fail (E_IS_SHELL (shell), NULL);

	return shell->priv->active_windows;
}

/**
 * e_shell_get_canonical_name:
 * @shell: an #EShell
 * @name: the name or alias of an #EShellModule
 *
 * Returns the canonical name for the #EShellModule whose name or alias
 * is @name.
 *
 * XXX Not sure this function is worth keeping around.
 *
 * Returns: the canonical #EShellModule name
 **/
const gchar *
e_shell_get_canonical_name (EShell *shell,
                            const gchar *name)
{
	EShellModule *shell_module;

	g_return_val_if_fail (E_IS_SHELL (shell), NULL);

	/* Handle NULL name arguments silently. */
	if (name == NULL)
		return NULL;

	shell_module = e_shell_get_module_by_name (shell, name);

	if (shell_module == NULL)
		return NULL;

	return G_TYPE_MODULE (shell_module)->name;
}

/**
 * e_shell_get_module_by_name:
 * @shell: an #EShell
 * @name: the name or alias of an #EShellModule
 *
 * Returns the corresponding #EShellModule for the given name or alias,
 * or %NULL if @name is not recognized.
 *
 * Returns: the #EShellModule named @name, or %NULL
 **/
EShellModule *
e_shell_get_module_by_name (EShell *shell,
                            const gchar *name)
{
	GHashTable *hash_table;

	g_return_val_if_fail (E_IS_SHELL (shell), NULL);
	g_return_val_if_fail (name != NULL, NULL);

	hash_table = shell->priv->modules_by_name;

	return g_hash_table_lookup (hash_table, name);
}

/**
 * e_shell_get_module_by_scheme:
 * @shell: an #EShell
 * @scheme: a URI scheme
 *
 * Returns the #EShellModule that implements the given URI scheme,
 * or %NULL if @scheme is not recognized.
 *
 * Returns: the #EShellModule that implements @scheme, or %NULL
 **/
EShellModule *
e_shell_get_module_by_scheme (EShell *shell,
                              const gchar *scheme)
{
	GHashTable *hash_table;

	g_return_val_if_fail (E_IS_SHELL (shell), NULL);
	g_return_val_if_fail (scheme != NULL, NULL);

	hash_table = shell->priv->modules_by_scheme;

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
 * e_shell_create_shell_window:
 * @shell: an #EShell
 *
 * Creates a new #EShellWindow and emits the #EShell::window-created
 * signal.  Use this function instead of e_shell_window_new() so that
 * @shell can track the window.
 *
 * Returns: a new #EShellWindow
 **/
GtkWidget *
e_shell_create_shell_window (EShell *shell)
{
	GList *active_windows;
	GtkWidget *shell_window;

	g_return_val_if_fail (E_IS_SHELL (shell), NULL);

	shell_window = e_shell_window_new (shell, shell->priv->safe_mode);

	active_windows = shell->priv->active_windows;
	active_windows = g_list_prepend (active_windows, shell_window);
	shell->priv->active_windows = active_windows;

	g_signal_connect_swapped (
		shell_window, "delete-event",
		G_CALLBACK (shell_window_delete_event_cb), shell);

	g_signal_connect_swapped (
		shell_window, "focus-in-event",
		G_CALLBACK (shell_window_focus_in_event_cb), shell);

	g_object_weak_ref (
		G_OBJECT (shell_window), (GWeakNotify)
		shell_window_weak_notify_cb, shell);

	g_signal_emit (shell, signals[WINDOW_CREATED], 0, shell_window);

	gtk_widget_show (shell_window);

	return shell_window;
}

/**
 * e_shell_handle_uri:
 * @shell: an #EShell
 * @uri: the URI to be handled
 *
 * Emits the #EShell::handle-uri signal.
 *
 * Returns: %TRUE if the URI was handled, %FALSE otherwise
 **/
gboolean
e_shell_handle_uri (EShell *shell,
                    const gchar *uri)
{
	gboolean handled;

	g_return_val_if_fail (E_IS_SHELL (shell), FALSE);
	g_return_val_if_fail (uri != NULL, FALSE);

	g_signal_emit (shell, signals[HANDLE_URI], 0, uri, &handled);

	return handled;
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
 * network becomes unavailable while #EShell:online-mode is %TRUE,
 * the @shell will force #EShell:online-mode to %FALSE until the
 * network becomes available again.
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
	if (!network_available && shell->priv->online_mode) {
		g_message ("Network disconnected.  Forced offline.");
		e_shell_set_online_mode (shell, FALSE);
		shell->priv->auto_reconnect = TRUE;
	} else if (network_available && shell->priv->auto_reconnect) {
		g_message ("Connection established.  Going online.");
		e_shell_set_online_mode (shell, TRUE);
		shell->priv->auto_reconnect = FALSE;
	}
}

/**
 * e_shell_get_online_mode:
 * @shell: an #EShell
 *
 * Returns %TRUE if Evolution is in online mode, %FALSE if Evolution is
 * offline.  Evolution may be offline because the user elected to work
 * offline, or because the network has become unavailable.
 *
 * Returns: %TRUE if Evolution is in online mode
 **/
gboolean
e_shell_get_online_mode (EShell *shell)
{
	g_return_val_if_fail (E_IS_SHELL (shell), FALSE);

	return shell->priv->online_mode;
}

/**
 * e_shell_set_online_mode:
 * @shell: an #EShell
 * @online_mode: whether to put Evolution in online mode
 *
 * Asynchronously places Evolution in online or offline mode.
 **/
void
e_shell_set_online_mode (EShell *shell,
                         gboolean online_mode)
{
	g_return_if_fail (E_IS_SHELL (shell));

	if (online_mode == shell->priv->online_mode)
		return;

	if (online_mode)
		shell_prepare_for_online (shell);
	else
		shell_prepare_for_offline (shell);
}

GtkWidget *
e_shell_get_preferences_window (void)
{
	static GtkWidget *preferences_window = NULL;

	if (G_UNLIKELY (preferences_window == NULL))
		preferences_window = e_preferences_window_new ();

	return preferences_window;
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
