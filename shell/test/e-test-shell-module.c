/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* e-test-shell-module.c
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
	  N_("_Test Item"),
	  NULL,
	  N_("Create a new test item"),
	  G_CALLBACK (action_test_item_new_cb) }
};

static GtkActionEntry source_entries[] = {

	{ "test-source-new",
	  "folder-new",
	  N_("Test _Source"),
	  NULL,
	  N_("Create a new test source"),
	  G_CALLBACK (action_test_source_new_cb) }
};

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
test_module_handle_uri (EShellModule *shell_module,
                        const gchar *uri)
{
	g_debug ("%s (uri=%s)", G_STRFUNC, uri);

	return FALSE;
}

static void
test_module_send_receive (EShellModule *shell_module,
                          GtkWindow *parent_window)
{
	g_debug ("%s (window=%p)", G_STRFUNC, parent_window);
}

static void
test_module_window_created (EShellModule *shell_module,
                            EShellWindow *shell_window)
{
	const gchar *module_name;

	g_debug ("%s (window=%p)", G_STRFUNC, shell_window);

	module_name = G_TYPE_MODULE (shell_module)->name;

	e_shell_window_register_new_item_actions (
		shell_window, module_name,
		item_entries, G_N_ELEMENTS (item_entries));

	e_shell_window_register_new_source_actions (
		shell_window, module_name,
		source_entries, G_N_ELEMENTS (source_entries));
}

static void
test_module_window_destroyed (EShellModule *shell_module,
                              gboolean last_window)
{
	g_debug ("%s (last=%d)", G_STRFUNC, last_window);
}

static EShellModuleInfo module_info = {

	MODULE_NAME,
	MODULE_ALIASES,
	MODULE_SCHEMES,
	MODULE_SORT_ORDER,

	/* Methods */
	test_module_is_busy,
	test_module_shutdown
};

void
e_shell_module_init (GTypeModule *type_module)
{
	EShell *shell;
	EShellModule *shell_module;

	shell_module = E_SHELL_MODULE (type_module);
	shell = e_shell_module_get_shell (shell_module);

	e_test_shell_view_get_type (type_module);
	e_shell_module_set_info (shell_module, &module_info);

	g_signal_connect_swapped (
		shell, "handle-uri",
		G_CALLBACK (test_module_handle_uri), shell_module);

	g_signal_connect_swapped (
		shell, "send-receive",
		G_CALLBACK (test_module_send_receive), shell_module);

	g_signal_connect_swapped (
		shell, "window-created",
		G_CALLBACK (test_module_window_created), shell_module);

	g_signal_connect_swapped (
		shell, "window-destroyed",
		G_CALLBACK (test_module_window_destroyed), shell_module);
}
