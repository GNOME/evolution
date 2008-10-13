/*
 * e-mail-shell-module.c
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

#include "shell/e-shell.h"
#include "shell/e-shell-module.h"
#include "shell/e-shell-window.h"

#include "e-mail-shell-view.h"
#include "e-mail-shell-module.h"
#include "e-mail-shell-module-migrate.h"

#define MODULE_NAME		"mail"
#define MODULE_ALIASES		""
#define MODULE_SCHEMES		"mailto:email"
#define MODULE_SORT_ORDER	200

/* Module Entry Point */
void e_shell_module_init (GTypeModule *type_module);

static void
action_mail_folder_new_cb (GtkAction *action,
                           EShellWindow *shell_window)
{
	/* FIXME */
}

static void
action_mail_message_new_cb (GtkAction *action,
                            EShellWindow *shell_window)
{
	/* FIXME */
}

static GtkActionEntry item_entries[] = {

	{ "mail-message-new",
	  "mail-message-new",
	  N_("_Mail Message"),  /* XXX C_() here */
	  "<Shift><Control>m",
	  N_("Compose a new mail message"),
	  G_CALLBACK (action_mail_message_new_cb) }
};

static GtkActionEntry source_entries[] = {

	{ "mail-folder-new",
	  "folder-new",
	  N_("Mail _Folder"),
	  NULL,
	  N_("Create a new mail folder"),
	  G_CALLBACK (action_mail_folder_new_cb) }
};

static gboolean
mail_module_handle_uri (EShellModule *shell_module,
                        const gchar *uri)
{
	/* FIXME */
	return FALSE;
}

static void
mail_module_window_created (EShellModule *shell_module,
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
	MODULE_SORT_ORDER,

	/* is_busy */ NULL,
	/* shutdown */ NULL,
	e_mail_shell_module_migrate
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
		e_mail_shell_view_get_type (type_module));

	g_signal_connect_swapped (
		shell, "handle-uri",
		G_CALLBACK (mail_module_handle_uri), shell_module);

	g_signal_connect_swapped (
		shell, "window-created",
		G_CALLBACK (mail_module_window_created), shell_module);
}

/**
 * e_mail_shell_module_load_store_by_uri:
 * @uri: URI of the #CamelStore
 * @name: name of the #CamelStore (for display purposes)
 *
 * Returns the newly added #CamelStore, or %NULL if a store could not
 * be loaded for @uri.
 *
 * Returns: the newly added #CamelStore, or %NULL
 **/
CamelStore *
e_mail_shell_module_load_store_by_uri (const gchar *uri,
                                       const gchar *name)
{
	return NULL; /* FIXME */
}

EAccountList *
mail_config_get_accounts (void)
{
	/* XXX Temporary placeholder. */
	return NULL;
}

void
mail_config_save_accounts (void)
{
	/* XXX Temporary placeholder. */
}

ESignatureList *
mail_config_get_signatures (void)
{
	/* XXX Temporary placeholder. */
	return NULL;
}

gchar *
em_uri_from_camel (const gchar *curi)
{
	/* XXX Temporary placeholder. */
	return NULL;
}
