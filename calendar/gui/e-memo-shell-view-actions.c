/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* e-memo-shell-view-actions.c
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

#include "e-memo-shell-view-private.h"

#include <e-util/gconf-bridge.h>

#include "print.h"

static void
action_memo_clipboard_copy_cb (GtkAction *action,
                               EMemoShellView *memo_shell_view)
{
	EMemos *memos;
	EMemoTable *memo_table;

	memos = E_MEMOS (memo_shell_view->priv->memos);
	memo_table = e_memos_get_calendar_table (memos);
	e_memo_table_copy_clipboard (memo_table);
}

static void
action_memo_clipboard_cut_cb (GtkAction *action,
                              EMemoShellView *memo_shell_view)
{
	EMemos *memos;
	EMemoTable *memo_table;

	memos = E_MEMOS (memo_shell_view->priv->memos);
	memo_table = e_memos_get_calendar_table (memos);
	e_memo_table_cut_clipboard (memo_table);
}

static void
action_memo_clipboard_paste_cb (GtkAction *action,
                                EMemoShellView *memo_shell_view)
{
	EMemos *memos;
	EMemoTable *memo_table;

	memos = E_MEMOS (memo_shell_view->priv->memos);
	memo_table = e_memos_get_calendar_table (memos);
	e_memo_table_paste_clipboard (memo_table);
}

static void
action_memo_delete_cb (GtkAction *action,
                       EMemoShellView *memo_shell_view)
{
	EMemos *memos;

	memos = E_MEMOS (memo_shell_view->priv->memos);
	e_memos_delete_selected (memos);
}

static void
action_memo_list_copy_cb (GtkAction *action,
                          EMemoShellView *memo_shell_view)
{
	/* FIXME */
}

static void
action_memo_list_delete_cb (GtkAction *action,
                            EMemoShellView *memo_shell_view)
{
	/* FIXME */
}

static void
action_memo_list_new_cb (GtkAction *action,
                         EMemoShellView *memo_shell_view)
{
	EShellView *shell_view;
	EShellWindow *shell_window;

	shell_view = E_SHELL_VIEW (memo_shell_view);
	shell_window = e_shell_view_get_shell_window (shell_view);
	calendar_setup_new_memo_list (GTK_WINDOW (shell_window));
}

static void
action_memo_list_properties_cb (GtkAction *action,
                                EMemoShellView *memo_shell_view)
{
	EShellView *shell_view;
	EShellWindow *shell_window;
	ESource *source;
	ESourceSelector *selector;

	shell_view = E_SHELL_VIEW (memo_shell_view);
	shell_window = e_shell_view_get_shell_window (shell_view);

	selector = E_SOURCE_SELECTOR (memo_shell_view->priv->selector);
	source = e_source_selector_peek_primary_selection (selector);
	g_return_if_fail (source != NULL);

	calendar_setup_edit_memo_list (GTK_WINDOW (shell_window), source);
}

static void
action_memo_open_cb (GtkAction *action,
                     EMemoShellView *memo_shell_view)
{
	EMemos *memos;

	memos = E_MEMOS (memo_shell_view->priv->memos);
	e_memos_open_memo (memos);
}

static void
action_memo_preview_cb (GtkToggleAction *action,
                        EMemoShellView *memo_shell_view)
{
	/* FIXME */
}

static void
action_memo_print_cb (GtkAction *action,
                      EMemoShellView *memo_shell_view)
{
	EMemos *memos;
	ETable *table;
	EMemoTable *memo_table;

	memos = E_MEMOS (memo_shell_view->priv->memos);
	memo_table = e_memos_get_calendar_table (memos);
	table = e_memo_table_get_table (memo_table);

	print_table (
		table, _("Print Memos"), _("Memos"),
		GTK_PRINT_OPERATION_ACTION_PRINT_DIALOG);
}

static void
action_memo_print_preview_cb (GtkAction *action,
                              EMemoShellView *memo_shell_view)
{
	EMemos *memos;
	ETable *table;
	EMemoTable *memo_table;

	memos = E_MEMOS (memo_shell_view->priv->memos);
	memo_table = e_memos_get_calendar_table (memos);
	table = e_memo_table_get_table (memo_table);

	print_table (
		table, _("Print Memos"), _("Memos"),
		GTK_PRINT_OPERATION_ACTION_PREVIEW);
}

static GtkActionEntry memo_entries[] = {

	{ "memo-clipboard-copy",
	  GTK_STOCK_COPY,
	  NULL,
	  NULL,
	  N_("Copy selected memo"),
	  G_CALLBACK (action_memo_clipboard_copy_cb) },

	{ "memo-clipboard-cut",
	  GTK_STOCK_CUT,
	  NULL,
	  NULL,
	  N_("Cut selected memo"),
	  G_CALLBACK (action_memo_clipboard_cut_cb) },

	{ "memo-clipboard-paste",
	  GTK_STOCK_PASTE,
	  NULL,
	  NULL,
	  N_("Paste memo from the clipboard"),
	  G_CALLBACK (action_memo_clipboard_paste_cb) },

	{ "memo-delete",
	  GTK_STOCK_DELETE,
	  NULL,
	  NULL,
	  N_("Delete selected memos"),
	  G_CALLBACK (action_memo_delete_cb) },

	{ "memo-list-copy",
	  GTK_STOCK_COPY,
	  N_("_Copy..."),
	  NULL,
	  NULL,  /* XXX Add a tooltip! */
	  G_CALLBACK (action_memo_list_copy_cb) },

	{ "memo-list-delete",
	  GTK_STOCK_DELETE,
	  N_("_Delete"),
	  NULL,
	  NULL,  /* XXX Add a tooltip! */
	  G_CALLBACK (action_memo_list_delete_cb) },

	{ "memo-list-new",
	  "stock_notes",
	  N_("_New Memo List"),
	  NULL,
	  N_("Create a new memo list"),
	  G_CALLBACK (action_memo_list_new_cb) },

	{ "memo-list-properties",
	  GTK_STOCK_PROPERTIES,
	  NULL,
	  NULL,
	  NULL,  /* XXX Add a tooltip! */
	  G_CALLBACK (action_memo_list_properties_cb) },

	{ "memo-open",
	  NULL,
	  N_("Open Memo"),
	  "<Control>o",
	  N_("View the selected memo"),
	  G_CALLBACK (action_memo_open_cb) },

	{ "memo-print",
	  GTK_STOCK_PRINT,
	  NULL,
	  NULL,
	  N_("Print the list of memos"),
	  G_CALLBACK (action_memo_print_cb) },

	{ "memo-print-preview",
	  GTK_STOCK_PRINT_PREVIEW,
	  NULL,
	  NULL,
	  N_("Preview the list of memos to be printed"),
	  G_CALLBACK (action_memo_print_preview_cb) },
};

static GtkToggleActionEntry memo_toggle_entries[] = {

	{ "memo-preview",
	  NULL,
	  N_("Memo _Preview"),
	  "<Control>m",
	  N_("Show memo preview pane"),
	  G_CALLBACK (action_memo_preview_cb),
	  TRUE }
};

void
e_memo_shell_view_actions_init (EMemoShellView *memo_shell_view)
{
	EShellView *shell_view;
	EShellWindow *shell_window;
	GtkActionGroup *action_group;
	GtkUIManager *manager;
	GConfBridge *bridge;
	GObject *object;
	const gchar *domain;
	const gchar *key;

	shell_view = E_SHELL_VIEW (memo_shell_view);
	shell_window = e_shell_view_get_shell_window (shell_view);
	manager = e_shell_window_get_ui_manager (shell_window);
	domain = GETTEXT_PACKAGE;

	e_load_ui_definition (manager, "evolution-memos.ui");

	action_group = memo_shell_view->priv->memo_actions;
	gtk_action_group_set_translation_domain (action_group, domain);
	gtk_action_group_add_actions (
		action_group, memo_entries,
		G_N_ELEMENTS (memo_entries), memo_shell_view);
	gtk_action_group_add_toggle_actions (
		action_group, memo_toggle_entries,
		G_N_ELEMENTS (memo_toggle_entries), memo_shell_view);
	gtk_ui_manager_insert_action_group (manager, action_group, 0);

	/* Bind GObject properties to GConf keys. */

	bridge = gconf_bridge_get ();

	object = G_OBJECT (ACTION (MEMO_PREVIEW));
	key = "/apps/evolution/calendar/display/show_memo_preview";
	gconf_bridge_bind_property (bridge, key, object, "active");
}

void
e_memo_shell_view_actions_update (EMemoShellView *memo_shell_view)
{
	ECal *cal;
	EMemos *memos;
	ETable *table;
	ECalModel *model;
	EMemoTable *memo_table;
	EShellView *shell_view;
	EShellWindow *shell_window;
	GtkAction *action;
	gboolean read_only = TRUE;
	gboolean sensitive;
	gint n_selected;

	shell_view = E_SHELL_VIEW (memo_shell_view);
	shell_window = e_shell_view_get_shell_window (shell_view);

	memos = E_MEMOS (memo_shell_view->priv->memos);
	memo_table = e_memos_get_calendar_table (memos);

	model = e_memo_table_get_model (memo_table);
	cal = e_cal_model_get_default_client (model);

	table = e_memo_table_get_table (memo_table);
	n_selected = e_table_selected_count (table);

	if (cal != NULL)
		e_cal_is_read_only (cal, &read_only, NULL);

	action = ACTION (MEMO_OPEN);
	sensitive = (n_selected == 1);
	gtk_action_set_sensitive (action, sensitive);

	action = ACTION (MEMO_CLIPBOARD_COPY);
	sensitive = (n_selected > 0);
	gtk_action_set_sensitive (action, sensitive);

	action = ACTION (MEMO_CLIPBOARD_CUT);
	sensitive = (n_selected > 0);
	gtk_action_set_sensitive (action, sensitive);

	action = ACTION (MEMO_CLIPBOARD_PASTE);
	sensitive = !read_only;
	gtk_action_set_sensitive (action, sensitive);

	action = ACTION (MEMO_DELETE);
	sensitive = (n_selected > 0) && !read_only;
	gtk_action_set_sensitive (action, sensitive);
}
