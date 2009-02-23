/*
 * e-test-shell-module.c
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

#include <glib/gi18n.h>

#include <e-shell.h>
#include <e-shell-module.h>
#include <e-shell-window.h>

#include "e-test-shell-view.h"

#define MODULE_NAME		"test"
#define MODULE_ALIASES		"monkey"
#define MODULE_SCHEMES		""
#define MODULE_SORT_ORDER	100

/* Module Entry Point */
void e_shell_module_init (GTypeModule *type_module);

static void
action_test_item_new_cb (GtkAction *action,
                         EShellWindow *shell_window)
{
	g_debug ("%s", G_STRFUNC);
}

static void
action_test_source_new_cb (GtkAction *action,
                           EShellWindow *shell_window)
{
	g_debug ("%s", G_STRFUNC);
}

static GtkActionEntry item_entries[] = {

	{ "test-item-new",
	  "document-new",
	  NC_("New", "_Test Item"),
	  NULL,
	  N_("Create a new test item"),
	  G_CALLBACK (action_test_item_new_cb) }
};

static GtkActionEntry source_entries[] = {

	{ "test-source-new",
	  "folder-new",
	  NC_("New", "Test _Source"),
	  NULL,
	  N_("Create a new test source"),
	  G_CALLBACK (action_test_source_new_cb) }
};

static void
test_module_start (EShellModule *shell_module)
{
	g_debug ("%s", G_STRFUNC);
}

static gboolean
test_module_is_busy (EShellModule *shell_module)
{
	g_debug ("%s", G_STRFUNC);

	return FALSE;
}

static gboolean
test_module_shutdown (EShellModule *shell_module)
{
	g_debug ("%s", G_STRFUNC);

	return TRUE;
}

static gboolean
test_module_migrate (EShellModule *shell_module,
                     gint major,
                     gint minor,
                     gint micro,
                     GError **error)
{
	g_debug ("%s (from %d.%d.%d)", G_STRFUNC, major, minor, micro);

	return TRUE;
}

static gboolean
test_module_handle_uri_cb (EShellModule *shell_module,
                           const gchar *uri)
{
	g_debug ("%s (uri=%s)", G_STRFUNC, uri);

	return FALSE;
}

static void
test_module_send_receive_cb (EShellModule *shell_module,
                             GtkWindow *parent_window)
{
	g_debug ("%s (window=%p)", G_STRFUNC, parent_window);
}

static void
test_module_window_created_cb (EShellModule *shell_module,
                               GtkWindow *window)
{
	const gchar *module_name;

	g_debug ("%s (%s)", G_STRFUNC, G_OBJECT_TYPE_NAME (window));

	if (!E_IS_SHELL_WINDOW (window))
		return;

	module_name = G_TYPE_MODULE (shell_module)->name;

	e_shell_window_register_new_item_actions (
		E_SHELL_WINDOW (window), module_name,
		item_entries, G_N_ELEMENTS (item_entries));

	e_shell_window_register_new_source_actions (
		E_SHELL_WINDOW (window), module_name,
		source_entries, G_N_ELEMENTS (source_entries));
}

static void
test_module_window_destroyed_cb (EShellModule *shell_module)
{
	g_debug ("%s", G_STRFUNC);
}

static EShellModuleInfo module_info = {

	MODULE_NAME,
	MODULE_ALIASES,
	MODULE_SCHEMES,
	MODULE_SORT_ORDER,

	/* Methods */
	test_module_start,
	test_module_is_busy,
	test_module_shutdown,
	test_module_migrate
};

void
e_shell_module_init (GTypeModule *type_module)
{
	EShell *shell;
	EShellModule *shell_module;

	shell_module = E_SHELL_MODULE (type_module);
	shell = e_shell_module_get_shell (shell_module);

	e_shell_module_set_info (
		shell_module, &module_info,
		e_test_shell_view_get_type (type_module));

	g_signal_connect_swapped (
		shell, "handle-uri",
		G_CALLBACK (test_module_handle_uri_cb), shell_module);

	g_signal_connect_swapped (
		shell, "send-receive",
		G_CALLBACK (test_module_send_receive_cb), shell_module);

	g_signal_connect_swapped (
		shell, "window-created",
		G_CALLBACK (test_module_window_created_cb), shell_module);

	g_signal_connect_swapped (
		shell, "window-destroyed",
		G_CALLBACK (test_module_window_destroyed_cb), shell_module);
}
