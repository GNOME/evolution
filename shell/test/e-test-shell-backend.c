/*
 * e-test-shell-backend.c
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

#include "e-test-shell-backend.h"

#include <glib/gi18n.h>

#include "shell/e-shell.h"
#include "shell/e-shell-window.h"

#include "e-test-shell-view.h"

#define E_TEST_SHELL_BACKEND_GET_PRIVATE(obj) \
	(G_TYPE_INSTANCE_GET_PRIVATE \
	((obj), E_TYPE_TEST_SHELL_BACKEND, ETestShellBackendPrivate))

struct _ETestShellBackendPrivate {
	gint placeholder;
};

static gpointer parent_class;
static GType test_shell_backend_type;

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
test_shell_backend_start (EShellBackend *shell_backend)
{
	g_debug ("%s", G_STRFUNC);
}

static gboolean
test_shell_backend_migrate (EShellBackend *shell_backend,
                            gint major,
                            gint minor,
                            gint micro,
                            GError **error)
{
	g_debug ("%s (from %d.%d.%d)", G_STRFUNC, major, minor, micro);

	return TRUE;
}

static gboolean
test_shell_backend_handle_uri_cb (EShellBackend *shell_backend,
                                  const gchar *uri)
{
	g_debug ("%s (uri=%s)", G_STRFUNC, uri);

	return FALSE;
}

static void
test_shell_backend_send_receive_cb (EShellBackend *shell_backend,
                                    GtkWindow *parent_window)
{
	g_debug ("%s (window=%p)", G_STRFUNC, parent_window);
}

static void
test_shell_backend_window_created_cb (EShellBackend *shell_backend,
                                      GtkWindow *window)
{
	const gchar *backend_name;

	g_debug ("%s (%s)", G_STRFUNC, G_OBJECT_TYPE_NAME (window));

	if (!E_IS_SHELL_WINDOW (window))
		return;

	backend_name = E_SHELL_BACKEND_GET_CLASS (shell_backend)->name;

	e_shell_window_register_new_item_actions (
		E_SHELL_WINDOW (window), backend_name,
		item_entries, G_N_ELEMENTS (item_entries));

	e_shell_window_register_new_source_actions (
		E_SHELL_WINDOW (window), backend_name,
		source_entries, G_N_ELEMENTS (source_entries));
}

static void
test_shell_backend_window_destroyed_cb (EShellBackend *shell_backend)
{
	g_debug ("%s", G_STRFUNC);
}

static void
test_shell_backend_constructed (GObject *object)
{
	EShell *shell;
	EShellBackend *shell_backend;

	shell_backend = E_SHELL_BACKEND (object);
	shell = e_shell_backend_get_shell (shell_backend);

	g_signal_connect_swapped (
		shell, "handle-uri",
		G_CALLBACK (test_shell_backend_handle_uri_cb),
		shell_backend);

	g_signal_connect_swapped (
		shell, "send-receive",
		G_CALLBACK (test_shell_backend_send_receive_cb),
		shell_backend);

	g_signal_connect_swapped (
		shell, "window-created",
		G_CALLBACK (test_shell_backend_window_created_cb),
		shell_backend);

	g_signal_connect_swapped (
		shell, "window-destroyed",
		G_CALLBACK (test_shell_backend_window_destroyed_cb),
		shell_backend);
}

static void
test_shell_backend_class_init (ETestShellBackendClass *class)
{
	GObjectClass *object_class;
	EShellBackendClass *shell_backend_class;

	parent_class = g_type_class_peek_parent (class);
	g_type_class_add_private (class, sizeof (ETestShellBackendPrivate));

	object_class = G_OBJECT_CLASS (class);
	object_class->constructed = test_shell_backend_constructed;

	shell_backend_class = E_SHELL_BACKEND_CLASS (class);
	shell_backend_class->shell_view_type = E_TYPE_TEST_SHELL_VIEW;
	shell_backend_class->name = "test";
	shell_backend_class->aliases = "monkey";
	shell_backend_class->schemes = "";
	shell_backend_class->sort_order = 100;
	shell_backend_class->preferences_page = NULL;
	shell_backend_class->start = test_shell_backend_start;
	shell_backend_class->migrate = test_shell_backend_migrate;
}

static void
test_shell_backend_init (ETestShellBackend *test_shell_backend)
{
	test_shell_backend->priv =
		E_TEST_SHELL_BACKEND_GET_PRIVATE (test_shell_backend);
}

GType
e_test_shell_backend_get_type (void)
{
	return test_shell_backend_type;
}

void
e_test_shell_backend_register_type (GTypeModule *type_module)
{
	const GTypeInfo type_info = {
		sizeof (ETestShellBackendClass),
		(GBaseInitFunc) NULL,
		(GBaseFinalizeFunc) NULL,
		(GClassInitFunc) test_shell_backend_class_init,
		(GClassFinalizeFunc) NULL,
		NULL,  /* class_data */
		sizeof (ETestShellBackend),
		0,     /* n_preallocs */
		(GInstanceInitFunc) test_shell_backend_init,
		NULL   /* value_table */
	};

	test_shell_backend_type = g_type_module_register_type (
		type_module, E_TYPE_SHELL_BACKEND,
		"ETestShellBackend", &type_info, 0);
}
