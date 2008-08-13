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

#include "e-shell-module.h"
#include "e-shell-registry.h"

#define SHUTDOWN_TIMEOUT	500  /* milliseconds */

static GList *active_windows;

static gboolean
shell_window_delete_event_cb (EShellWindow *shell_window)
{
	/* If other windows are open we can safely close this one. */
	if (g_list_length (active_windows) > 1)
		return FALSE;

	/* Otherwise we initiate application shutdown. */
	return !e_shell_quit ();
}

static void
shell_window_weak_notify_cb (gpointer unused,
                             GObject *where_the_object_was)
{
	active_windows = g_list_remove (active_windows, where_the_object_was);

	/* If that was the last window, we're done. */
	if (active_windows == NULL)
		gtk_main_quit ();
}

static gboolean
shell_shutdown_timeout (void)
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
		list = g_list_copy (active_windows);
		g_list_foreach (list, (GFunc) gtk_widget_destroy, NULL);
		g_list_free (list);

	/* If a module is still busy, try again after a short delay. */
	} else if (source_id == 0)
		source_id = g_timeout_add (
			SHUTDOWN_TIMEOUT, (GSourceFunc)
			shell_shutdown_timeout, NULL);

	/* Return TRUE to repeat the timeout, FALSE to stop it.  This
	 * may seem backwards if the function was called directly. */
	return !proceed;
}

EShellWindow *
e_shell_create_window (void)
{
	GtkWidget *shell_window;

	shell_window = e_shell_window_new ();

	active_windows = g_list_prepend (active_windows, shell_window);

	g_signal_connect (
		shell_window, "delete-event",
		G_CALLBACK (shell_window_delete_event_cb), NULL);

	g_object_weak_ref (
		G_OBJECT (shell_window), (GWeakNotify)
		shell_window_weak_notify_cb, NULL);

	g_list_foreach (
		e_shell_registry_list_modules (),
		(GFunc) e_shell_module_window_created, shell_window);

	gtk_widget_show (shell_window);

	return E_SHELL_WINDOW (shell_window);
}

gboolean
e_shell_handle_uri (const gchar *uri)
{
	EShellModule *shell_module;
	GFile *file;
	gchar *scheme;

	g_return_val_if_fail (uri != NULL, FALSE);

	file = g_file_new_for_uri (uri);
	scheme = g_file_get_uri_scheme (file);
	g_object_unref (file);

	if (scheme == NULL)
		return FALSE;

	shell_module = e_shell_registry_get_module_by_scheme (scheme);

	/* Scheme lookup failed so try looking up the shell module by
	 * name.  Note, we only open a shell window if the URI refers
	 * to a shell module by name, not by scheme. */
	if (shell_module == NULL) {
		EShellWindow *shell_window;

		shell_module = e_shell_registry_get_module_by_name (scheme);

		if (shell_module == NULL)
			return FALSE;

		shell_window = e_shell_create_window ();
		/* FIXME  Set window to appropriate view. */
	}

	return e_shell_module_handle_uri (shell_module, uri);
}

void
e_shell_send_receive (GtkWindow *parent)
{
	g_list_foreach (
		e_shell_registry_list_modules (),
		(GFunc) e_shell_module_send_and_receive, NULL);
}

void
e_shell_go_offline (void)
{
	/* FIXME */
}

void
e_shell_go_online (void)
{
	/* FIXME */
}

EShellLineStatus
e_shell_get_line_status (void)
{
	/* FIXME */
	return E_SHELL_LINE_STATUS_ONLINE;
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
e_shell_is_busy (void)
{
	/* FIXME */
	return FALSE;
}

gboolean
e_shell_do_quit (void)
{
	/* FIXME */
	return TRUE;
}

gboolean
e_shell_quit (void)
{
	/* FIXME */
	return TRUE;
}
