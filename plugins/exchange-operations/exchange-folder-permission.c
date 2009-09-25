/*
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
 * Authors:
 *		Shakti Sen <shprasad@novell.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <glib/gi18n.h>
#include <gtk/gtk.h>
#include <gconf/gconf-client.h>

#include <libedataserverui/e-source-selector.h>
#include <libebook/e-book.h>
#include <camel/camel-url.h>

#include <exchange-account.h>

#include <e-util/e-dialog-utils.h>
#include <calendar/gui/e-cal-model.h>

#include <shell/e-shell-view.h>
#include <shell/e-shell-window.h>

#include <mail/em-folder-tree.h>
#include <mail/em-folder-tree-model.h>

#include "exchange-config-listener.h"
#include "exchange-folder-subscription.h"
#include "exchange-operations.h"
#include "exchange-permissions-dialog.h"

#define d(x)

gboolean eex_ui_mail_init (GtkUIManager *ui_manager, EShellView *shell_view);
gboolean eex_ui_calendar_permissions (GtkUIManager *ui_manager, EShellView *shell_view);
gboolean eex_ui_tasks_permissions (GtkUIManager *ui_manager, EShellView *shell_view);
gboolean eex_ui_addressbook_permissions (GtkUIManager *ui_manager, EShellView *shell_view);

static gboolean
is_subscribed_folder (const gchar *uri)
{
	const gchar *path;
	ExchangeAccount *account;
	gint offset;

	g_return_val_if_fail (uri != NULL, FALSE);

	account = exchange_operations_get_exchange_account ();
	g_return_val_if_fail (account != NULL, FALSE);
	g_return_val_if_fail (account->account_filename != NULL, FALSE);

	offset = strlen ("exchange://") + strlen (account->account_filename) + strlen ("/;");
	g_return_val_if_fail (strlen (uri) >= offset, FALSE);

	path = uri + offset;

	return strchr (path, '@') != NULL;
}

static void
call_folder_permissions (const gchar *uri)
{
	ExchangeAccount *account = NULL;
	EFolder *folder = NULL;

	g_return_if_fail (uri != NULL);

	account = exchange_operations_get_exchange_account ();
	if (!account)
		return;

	folder = exchange_account_get_folder (account, uri);
	if (folder)
		exchange_permissions_dialog_new (account, folder, NULL);
}

static gboolean
is_eex_folder_selected (EShellView *shell_view, gchar **puri)
{
	ExchangeAccount *account = NULL;
	gint mode;
	EShellSidebar *shell_sidebar;
	EMFolderTree *folder_tree = NULL;
	GtkTreeSelection *selection;
	GtkTreeModel *model = NULL;
	GtkTreeIter iter;
	gboolean is_store = FALSE, res;
	gchar *uri = NULL;

	g_return_val_if_fail (shell_view != NULL, FALSE);

	account = exchange_operations_get_exchange_account ();
	if (!account)
		return FALSE;

	exchange_account_is_offline (account, &mode);
	if (mode == OFFLINE_MODE)
		return FALSE;

	shell_sidebar = e_shell_view_get_shell_sidebar (shell_view);
	g_object_get (shell_sidebar, "folder-tree", &folder_tree, NULL);
	g_return_val_if_fail (folder_tree != NULL, FALSE);

	selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (folder_tree));
	g_return_val_if_fail (selection != NULL, FALSE);

	if (!gtk_tree_selection_get_selected (selection, &model, &iter))
		return FALSE;

	gtk_tree_model_get (model, &iter,
		COL_STRING_URI, &uri,
		COL_BOOL_IS_STORE, &is_store,
		-1);

	res = !is_store && uri && g_ascii_strncasecmp (uri, "exchange://", 11) == 0;

	if (res) {
		const gchar *path;

		path = uri + strlen ("exchange://") + strlen (account->account_filename);
		res = path && *path;

		if (res) {
			if (puri)
				*puri = g_strdup (uri);
		}
	}

	g_free (uri);

	return res;
}

static void
eex_mail_folder_permissions_cb (GtkAction *action, EShellView *shell_view)
{
	gchar *uri = NULL;

	if (is_eex_folder_selected (shell_view, &uri))
		call_folder_permissions (uri);

	g_free (uri);
}

static void
eex_folder_subscribe_cb (GtkAction *action, EShellView *shell_view)
{
	const gchar *name;

	name = gtk_action_get_name (action);
	g_return_if_fail (name != NULL);

	name = strrchr (name, '-');
	g_return_if_fail (name != NULL && *name == '-');

	call_folder_subscribe (name + 1);
}

static void
eex_mail_folder_inbox_unsubscribe_cb (GtkAction *action, EShellView *shell_view)
{
	gchar *uri = NULL;

	if (is_eex_folder_selected (shell_view, &uri))
		call_folder_unsubscribe ("Inbox", uri, NULL);

	g_free (uri);
}

/* Beware, depends on the order */
static GtkActionEntry mail_entries[] = {
	{ "eex-mail-folder-permissions",
	  "folder-new",
	  N_("Permissions..."),
	  NULL,
	  N_("Check folder permissions"),
	  G_CALLBACK (eex_mail_folder_permissions_cb) },

	{ "eex-folder-subscribe-Inbox",
	  NULL,
	  N_("Subscribe to Other User's Folder..."),
	  NULL,
	  N_("Subscribe to Other User's Folder"),
	  G_CALLBACK (eex_folder_subscribe_cb) },

	{ "eex-mail-folder-inbox-unsubscribe",
	  "folder-new",
	  N_("Unsubscribe Folder..."),
	  NULL,
	  NULL,
	  G_CALLBACK (eex_mail_folder_inbox_unsubscribe_cb) }
};

static void
update_mail_entries_cb (EShellView *shell_view, gpointer user_data)
{
	GtkActionGroup *action_group;
	EShellWindow *shell_window;
	GtkAction *action;
	gboolean is_eex;
	gchar *uri = NULL;
	gint i;

	g_return_if_fail (E_IS_SHELL_VIEW (shell_view));

	is_eex = is_eex_folder_selected (shell_view, &uri);
	shell_window = e_shell_view_get_shell_window (shell_view);
	action_group = e_shell_window_get_action_group (shell_window, "mail");

	for (i = 0; i < G_N_ELEMENTS (mail_entries); i++) {
		gboolean visible = is_eex;

		action = gtk_action_group_get_action (action_group, mail_entries[i].name);
		g_return_if_fail (action != NULL);

		if (visible && i == 2) {
			/* it's an unsubscribe, check if this is public and show/hide based on that */
			visible = uri && is_subscribed_folder (uri);
		}

		gtk_action_set_visible (action, visible);
	}

	g_free (uri);
}

gboolean
eex_ui_mail_init (GtkUIManager *ui_manager, EShellView *shell_view)
{
	EShellWindow *shell_window;

	shell_window = e_shell_view_get_shell_window (shell_view);

	gtk_action_group_add_actions (
		e_shell_window_get_action_group (shell_window, "mail"),
		mail_entries, G_N_ELEMENTS (mail_entries), shell_view);

	g_signal_connect (shell_view, "update-actions", G_CALLBACK (update_mail_entries_cb), NULL);

	return TRUE;
}

static gboolean
is_eex_source_selected (EShellView *shell_view, gchar **puri)
{
	gint mode;
	ExchangeAccount *account = NULL;
	ESource *source = NULL;
	gchar *uri = NULL;
	EShellSidebar *shell_sidebar;
	ESourceSelector *selector = NULL;

	shell_sidebar = e_shell_view_get_shell_sidebar (shell_view);
	g_return_val_if_fail (shell_sidebar != NULL, FALSE);

	g_object_get (shell_sidebar, "selector", &selector, NULL);
	g_return_val_if_fail (selector != NULL, FALSE);

	source = e_source_selector_peek_primary_selection (selector);
	uri = e_source_get_uri (source);

	g_object_unref (selector);

	if (uri && !g_strrstr (uri, "exchange://")) {
		g_free (uri);
		return FALSE;
	}

	account = exchange_operations_get_exchange_account ();
	if (!account) {
		g_free (uri);
		return FALSE;
	}

	exchange_account_is_offline (account, &mode);
	if (mode == OFFLINE_MODE) {
		g_free (uri);
		return FALSE;
	}

	if (!exchange_account_get_folder (account, uri)) {
		g_free (uri);
		return FALSE;
	}

	if (puri)
		*puri = uri;
	else
		g_free (uri);

	return TRUE;
}

#define NUM_ENTRIES 3

static void
update_source_entries_cb (EShellView *shell_view, GtkActionEntry *entries)
{
	GtkActionGroup *action_group;
	EShellWindow *shell_window;
	GtkAction *action;
	const gchar *group;
	gchar *uri = NULL;
	gboolean is_eex;
	gint i;

	g_return_if_fail (E_IS_SHELL_VIEW (shell_view));
	g_return_if_fail (entries != NULL);

	if (strstr (entries->name, "calendar"))
		group = "calendar";
	else if (strstr (entries->name, "tasks"))
		group = "tasks";
	else
		group = "addressbook";

	is_eex = is_eex_source_selected (shell_view, &uri);
	shell_window = e_shell_view_get_shell_window (shell_view);
	action_group = e_shell_window_get_action_group (shell_window, group);

	for (i = 0; i < NUM_ENTRIES; i++) {
		gboolean visible = is_eex;

		action = gtk_action_group_get_action (action_group, entries->name);
		g_return_if_fail (action != NULL);

		if (visible && i == 2) {
			/* it's an unsubscribe, check if this is public and show/hide based on that */
			visible = uri && is_subscribed_folder (uri);
		}

		gtk_action_set_visible (action, visible);

	}

	g_free (uri);
}

static void
setup_source_actions (EShellView *shell_view, GtkActionEntry *entries)
{
	EShellWindow *shell_window;
	const gchar *group;

	g_return_if_fail (shell_view != NULL);
	g_return_if_fail (entries != NULL);

	if (strstr (entries->name, "calendar"))
		group = "calendar";
	else if (strstr (entries->name, "tasks"))
		group = "tasks";
	else
		group = "addressbook";

	shell_window = e_shell_view_get_shell_window (shell_view);

	gtk_action_group_add_actions (
		e_shell_window_get_action_group (shell_window, group),
		entries, NUM_ENTRIES, shell_view);

	g_signal_connect (shell_view, "update-actions", G_CALLBACK (update_source_entries_cb), entries);
}

static void
source_permissions_cb (GtkAction *action, EShellView *shell_view)
{
	gchar *uri = NULL;

	g_return_if_fail (shell_view != NULL);

	if (is_eex_source_selected (shell_view, &uri))
		call_folder_permissions (uri);

	g_free (uri);
}

static void
eex_folder_unsubscribe_cb (GtkAction *action, EShellView *shell_view)
{
	gchar *uri = NULL;
	const gchar *name;

	g_return_if_fail (shell_view != NULL);

	name = gtk_action_get_name (action);
	g_return_if_fail (name != NULL);

	name = strrchr (name, '-');
	g_return_if_fail (name != NULL && *name == '-');

	if (is_eex_source_selected (shell_view, &uri)) {
		EShellSidebar *shell_sidebar;
		ESourceSelector *selector = NULL;

		shell_sidebar = e_shell_view_get_shell_sidebar (shell_view);
		g_return_if_fail (shell_sidebar != NULL);

		g_object_get (shell_sidebar, "selector", &selector, NULL);
		g_return_if_fail (selector != NULL);

		call_folder_unsubscribe (name + 1, uri, e_source_selector_peek_primary_selection (selector));
	}

	g_free (uri);
}

/* Beware, depends on count and order */
static GtkActionEntry calendar_entries[] = {
	{ "eex-calendar-permissions",
	  "folder-new",
	  N_("Permissions..."),
	  NULL,
	  N_("Check calendar permissions"),
	  G_CALLBACK (source_permissions_cb) },

	{ "eex-folder-subscribe-Calendar",
	  NULL,
	  N_("Subscribe to Other User's Folder..."),
	  NULL,
	  N_("Subscribe to Other User's Folder"),
	  G_CALLBACK (eex_folder_subscribe_cb) },

	{ "eex-folder-unsubscribe-Calendar",
	  "folder-new",
	  N_("Unsubscribe Folder..."),
	  NULL,
	  NULL,
	  G_CALLBACK (eex_folder_unsubscribe_cb) }
};

gboolean
eex_ui_calendar_permissions (GtkUIManager *ui_manager, EShellView *shell_view)
{
	g_return_val_if_fail (G_N_ELEMENTS (calendar_entries) != NUM_ENTRIES, FALSE);

	setup_source_actions (shell_view, calendar_entries);

	return TRUE;
}

/* Beware, depends on count and order */
static GtkActionEntry tasks_entries[] = {
	{ "eex-tasks-permissions",
	  "folder-new",
	  N_("Permissions..."),
	  NULL,
	  N_("Check tasks permissions"),
	  G_CALLBACK (source_permissions_cb) },

	{ "eex-folder-subscribe-Tasks",
	  NULL,
	  N_("Subscribe to Other User's Folder..."),
	  NULL,
	  N_("Subscribe to Other User's Folder"),
	  G_CALLBACK (eex_folder_subscribe_cb) },

	{ "eex-folder-unsubscribe-Tasks",
	  "folder-new",
	  N_("Unsubscribe Folder..."),
	  NULL,
	  NULL,
	  G_CALLBACK (eex_folder_unsubscribe_cb) }
};

gboolean
eex_ui_tasks_permissions (GtkUIManager *ui_manager, EShellView *shell_view)
{
	g_return_val_if_fail (G_N_ELEMENTS (tasks_entries) != NUM_ENTRIES, FALSE);

	setup_source_actions (shell_view, tasks_entries);

	return TRUE;
}

/* Beware, depends on count and order */
static GtkActionEntry addressbook_entries[] = {
	{ "eex-addressbook-permissions",
	  "folder-new",
	  N_("Permissions..."),
	  NULL,
	  N_("Check addressbook permissions"),
	  G_CALLBACK (source_permissions_cb) },

	{ "eex-folder-subscribe-Contacts",
	  NULL,
	  N_("Subscribe to Other User's Folder..."),
	  NULL,
	  N_("Subscribe to Other User's Folder"),
	  G_CALLBACK (eex_folder_subscribe_cb) },

	{ "eex-folder-unsubscribe-Contacts",
	  "folder-new",
	  N_("Unsubscribe Folder..."),
	  NULL,
	  NULL,
	  G_CALLBACK (eex_folder_unsubscribe_cb) }
};

gboolean
eex_ui_addressbook_permissions (GtkUIManager *ui_manager, EShellView *shell_view)
{
	g_return_val_if_fail (G_N_ELEMENTS (addressbook_entries) != NUM_ENTRIES, FALSE);

	setup_source_actions (shell_view, addressbook_entries);

	return TRUE;
}
