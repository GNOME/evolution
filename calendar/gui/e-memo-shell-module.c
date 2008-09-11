/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* e-memo-shell-module.c
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

#include <e-memo-shell-view.h>

#define MODULE_NAME		"memos"
#define MODULE_ALIASES		""
#define MODULE_SCHEMES		"memo"
#define MODULE_SEARCHES		"memotypes.xml"
#define MODULE_SORT_ORDER	500

/* Module Entry Point */
void e_shell_module_init (GTypeModule *type_module);

static void
action_memo_new_cb (GtkAction *action,
                    EShellWindow *shell_window)
{
}

static void
action_memo_shared_new_cb (GtkAction *action,
                           EShellWindow *shell_window)
{
}

static void
action_memo_list_new_cb (GtkAction *action,
                         EShellWindow *shell_window)
{
}

static GtkActionEntry item_entries[] = {

	{ "memo-new",
	  "stock_insert-note",
	  N_("Mem_o"),  /* XXX Need C_() here */
	  "<Control>o",
	  N_("Create a new memo"),
	  G_CALLBACK (action_memo_new_cb) },

	{ "memo-shared-new",
	  "stock_insert-note",
	  N_("_Shared Memo"),
	  "<Control>h",
	  N_("Create a new shared memo"),
	  G_CALLBACK (action_memo_shared_new_cb) }
};

static GtkActionEntry source_entries[] = {

	{ "memo-list-new",
	  "stock_notes",
	  N_("Memo Li_st"),
	  NULL,
	  N_("Create a new memo list"),
	  G_CALLBACK (action_memo_list_new_cb) }
};

static gboolean
memo_module_handle_uri (EShellModule *shell_module,
                        const gchar *uri)
{
	/* FIXME */
	return FALSE;
}

static void
memo_module_window_created (EShellModule *shell_module,
                            EShellWindow *shell_window)
{
	const gchar *module_name;

	module_name = G_TYPE_MODULE (shell_module)->name;

	e_shell_window_register_new_item_actions (
		shell_window, module_name,
		item_entries, G_N_ELEMENTS (item_entries));

	e_shell_window_register_new_source_actions (
		shell_window, module_name,
		source_entries, G_N_ELEMENTS (source_entries));
}

static EShellModuleInfo module_info = {

	MODULE_NAME,
	MODULE_ALIASES,
	MODULE_SCHEMES,
	MODULE_SEARCHES,
	MODULE_SORT_ORDER
};

void
e_shell_module_init (GTypeModule *type_module)
{
	EShell *shell;
	EShellModule *shell_module;

	shell_module = E_SHELL_MODULE (type_module);
	shell = e_shell_module_get_shell (shell_module);

	e_memo_shell_view_get_type (type_module);
	e_shell_module_set_info (shell_module, &module_info);

	g_signal_connect_swapped (
		shell, "handle-uri",
		G_CALLBACK (memo_module_handle_uri), shell_module);

	g_signal_connect_swapped (
		shell, "window-created",
		G_CALLBACK (memo_module_window_created), shell_module);
}
