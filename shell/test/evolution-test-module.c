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

#include <e-shell-module.h>

#define MODULE_SORT_ORDER	100
#define MODULE_ALIASES		"test"
#define MODULE_SCHEMES		""

/* Module Entry Point */
void e_shell_module_init (GTypeModule *module);

static gboolean
test_module_is_busy (void)
{
	g_debug ("%s", G_STRFUNC);

	return FALSE;
}

static gboolean
test_module_shutdown (void)
{
	g_debug ("%s", G_STRFUNC);

	return TRUE;
}

static gboolean
test_module_handle_uri (const gchar *uri)
{
	g_debug ("%s (uri=%s)", G_STRFUNC, uri);

	return TRUE;
}

static void
test_module_send_and_receive (void)
{
	g_debug ("%s", G_STRFUNC);
}

static void
test_module_window_created (EShellWindow *window)
{
	g_debug ("%s (window=%p)", G_STRFUNC, window);
}

static EShellModuleInfo module_info = {

	MODULE_SORT_ORDER,
	MODULE_ALIASES,
	MODULE_SCHEMES,
	G_TYPE_INVALID,

	/* Methods */
	test_module_is_busy,
	test_module_shutdown,
	test_module_handle_uri,
	test_module_send_and_receive,
	test_module_window_created
};

void
e_shell_module_init (GTypeModule *module)
{
	g_type_module_set_name (module, "name");
	e_shell_module_set_info (E_SHELL_MODULE (module), &module_info);
}
