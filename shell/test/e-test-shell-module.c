/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* e-test-module.c
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

#include <e-shell.h>
#include <e-shell-module.h>
#include <e-shell-window.h>

#include "e-test-shell-view.h"

#define MODULE_NAME		"test"
#define MODULE_ALIASES		"monkey"
#define MODULE_SCHEMES		""
#define MODULE_SORT_ORDER	100

/* Module Entry Point */
void e_shell_module_init (GTypeModule *module);

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
	g_debug ("%s (window=%p)", G_STRFUNC, shell_window);
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
e_shell_module_init (GTypeModule *module)
{
	EShell *shell;

	e_test_shell_view_get_type (module);
	shell = e_shell_module_get_shell (E_SHELL_MODULE (module));
	e_shell_module_set_info (E_SHELL_MODULE (module), &module_info);

	g_signal_connect_swapped (
		shell, "handle-uri",
		G_CALLBACK (test_module_handle_uri), module);

	g_signal_connect_swapped (
		shell, "send-receive",
		G_CALLBACK (test_module_send_receive), module);

	g_signal_connect_swapped (
		shell, "window-created",
		G_CALLBACK (test_module_window_created), module);

	g_signal_connect_swapped (
		shell, "window-destroyed",
		G_CALLBACK (test_module_window_destroyed), module);
}
