/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* e-shell.c
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of version 2 of the GNU General Public
 * License as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include "e-shell.h"

#include <glib/gi18n.h>
#include <e-preferences-window.h>
#include <e-util/e-util.h>

#include "e-shell-marshal.h"
#include "e-shell-module.h"
#include "e-shell-registry.h"
#include "e-shell-window.h"

#define SHUTDOWN_TIMEOUT	500  /* milliseconds */

#define E_SHELL_GET_PRIVATE(obj) \
	(G_TYPE_INSTANCE_GET_PRIVATE \
	((obj), E_TYPE_SHELL, EShellPrivate))

struct _EShellPrivate {
	GList *active_windows;
	EShellLineStatus line_status;

	guint online    : 1;
	guint safe_mode : 1;
};

enum {
	PROP_0,
	PROP_ONLINE
};

enum {
	HANDLE_URI,
	SEND_RECEIVE,
	WINDOW_CREATED,
	WINDOW_DESTROYED,
	LAST_SIGNAL
};

static gpointer parent_class;
static guint signals[LAST_SIGNAL];

#if NM_SUPPORT
void e_shell_dbus_initialize (EShell *shell);
#endif

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

static void
shell_window_weak_notify_cb (EShell *shell,
                             GObject *where_the_object_was)
{
	GList *active_windows;
	gboolean last_window;

	active_windows = shell->priv->active_windows;
	active_windows = g_list_remove (active_windows, where_the_object_was);
	shell->priv->active_windows = active_windows;

	last_window = (shell->priv->active_windows == NULL);
	g_signal_emit (shell, signals[WINDOW_DESTROYED], 0, last_window);
}

static gboolean
shell_shutdown_timeout (EShell *shell)
{
	GList *list, *iter;
	gboolean proceed = TRUE;
	static guint source_id = 0;
	static guint message_timer = 1;

	/* Module list is read-only; do not free. */
	list = e_shell_registry_list_modules ();

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
		case PROP_ONLINE:
			g_value_set_boolean (
				value, e_shell_get_online (
				E_SHELL (object)));
			return;
	}

	G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
}

static void
shell_dispose (GObject *object)
{
	/* Chain up to parent's dispose() method. */
	G_OBJECT_CLASS (parent_class)->dispose (object);
}

static void
shell_finalize (GObject *object)
{
	/* Chain up to parent's finalize() method. */
	G_OBJECT_CLASS (parent_class)->finalize (object);
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

	g_object_class_install_property (
		object_class,
		PROP_ONLINE,
		g_param_spec_boolean (
			"online",
			_("Online"),
			_("Whether the shell is online"),
			TRUE,
			G_PARAM_READWRITE |
			G_PARAM_CONSTRUCT));

	signals[HANDLE_URI] = g_signal_new (
		"handle-uri",
		G_OBJECT_CLASS_TYPE (object_class),
		G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
		0, g_signal_accumulator_true_handled, NULL,
		e_shell_marshal_BOOLEAN__STRING,
		G_TYPE_BOOLEAN, 1,
		G_TYPE_STRING);

	signals[SEND_RECEIVE] = g_signal_new (
		"send-receive",
		G_OBJECT_CLASS_TYPE (object_class),
		G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
		0, NULL, NULL,
		g_cclosure_marshal_VOID__OBJECT,
		G_TYPE_NONE, 1,
		GTK_TYPE_WINDOW);

	signals[WINDOW_CREATED] = g_signal_new (
		"window-created",
		G_OBJECT_CLASS_TYPE (object_class),
		G_SIGNAL_RUN_LAST,
		0, NULL, NULL,
		g_cclosure_marshal_VOID__OBJECT,
		G_TYPE_NONE, 1,
		E_TYPE_SHELL_WINDOW);

	signals[WINDOW_DESTROYED] = g_signal_new (
		"window-destroyed",
		G_OBJECT_CLASS_TYPE (object_class),
		G_SIGNAL_RUN_LAST,
		0, NULL, NULL,
		g_cclosure_marshal_VOID__BOOLEAN,
		G_TYPE_NONE, 1,
		G_TYPE_BOOLEAN);
}

static void
shell_init (EShell *shell)
{
	shell->priv = E_SHELL_GET_PRIVATE (shell);

	shell->priv->safe_mode = e_file_lock_exists ();

#if NM_SUPPORT
	e_shell_dbus_initialize (shell);
#endif

	e_file_lock_create ();
	e_shell_registry_init (shell);
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

EShell *
e_shell_new (gboolean online)
{
	return g_object_new (E_TYPE_SHELL, "online", online, NULL);
}

GtkWidget *
e_shell_create_window (EShell *shell)
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

	g_object_weak_ref (
		G_OBJECT (shell_window), (GWeakNotify)
		shell_window_weak_notify_cb, shell);

	g_signal_emit (shell, signals[WINDOW_CREATED], 0, shell_window);

	gtk_widget_show (shell_window);

	return shell_window;
}

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

void
e_shell_send_receive (EShell *shell,
                      GtkWindow *parent)
{
	g_return_if_fail (E_IS_SHELL (shell));
	g_return_if_fail (GTK_IS_WINDOW (parent));

	g_signal_emit (shell, signals[SEND_RECEIVE], 0, parent);
}

gboolean
e_shell_get_online (EShell *shell)
{
	g_return_val_if_fail (E_IS_SHELL (shell), FALSE);

	return shell->priv->online;
}

void
e_shell_set_online (EShell *shell,
                    gboolean online)
{
	g_return_if_fail (E_IS_SHELL (shell));

	shell->priv->online = online;

	g_object_notify (G_OBJECT (shell), "online");
}

EShellLineStatus
e_shell_get_line_status (EShell *shell)
{
	g_return_val_if_fail (E_IS_SHELL (shell), E_SHELL_LINE_STATUS_OFFLINE);

	return shell->priv->line_status;
}

void
e_shell_set_line_status (EShell *shell,
                         EShellLineStatus status)
{
}

GtkWidget *
e_shell_get_preferences_window (void)
{
	static GtkWidget *preferences_window = NULL;

	if (G_UNLIKELY (preferences_window == NULL))
		preferences_window = e_preferences_window_new ();

	return preferences_window;
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
