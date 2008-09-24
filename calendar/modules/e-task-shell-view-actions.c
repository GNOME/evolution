/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* e-task-shell-view-actions.c
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

#include "e-task-shell-view-private.h"

static void
action_task_assign_cb (GtkAction *action,
                       ETaskShellView *task_shell_view)
{
}

static void
action_task_clipboard_copy_cb (GtkAction *action,
                               ETaskShellView *task_shell_view)
{
	ETaskShellContent *task_shell_content;
	ETaskTable *task_table;

	task_shell_content = task_shell_view->priv->task_shell_content;
	task_table = e_task_shell_content_get_task_table (task_shell_content);
	e_task_table_copy_clipboard (task_table);
}

static void
action_task_clipboard_cut_cb (GtkAction *action,
                              ETaskShellView *task_shell_view)
{
	ETaskShellContent *task_shell_content;
	ETaskTable *task_table;

	task_shell_content = task_shell_view->priv->task_shell_content;
	task_table = e_task_shell_content_get_task_table (task_shell_content);
	e_task_table_cut_clipboard (task_table);
}

static void
action_task_clipboard_paste_cb (GtkAction *action,
                                ETaskShellView *task_shell_view)
{
	ETaskShellContent *task_shell_content;
	ETaskTable *task_table;

	task_shell_content = task_shell_view->priv->task_shell_content;
	task_table = e_task_shell_content_get_task_table (task_shell_content);
	e_task_table_paste_clipboard (task_table);
}

static void
action_task_delete_cb (GtkAction *action,
                       ETaskShellView *task_shell_view)
{
	ETaskShellContent *task_shell_content;
	ETaskPreview *task_preview;
	ETaskTable *task_table;
	const gchar *status_message;

	task_shell_content = task_shell_view->priv->task_shell_content;
	task_table = e_task_shell_content_get_task_table (task_shell_content);
	task_preview = e_task_shell_content_get_task_preview (task_shell_content);

	status_message = _("Deleting selected tasks...");
	e_task_shell_view_set_status_message (task_shell_view, status_message);
	e_task_table_delete_selected (task_table);
	e_task_shell_view_set_status_message (task_shell_view, NULL);

	e_task_preview_clear (task_preview);
}

static void
action_task_forward_cb (GtkAction *action,
                        ETaskShellView *task_shell_view)
{
}

static void
action_task_list_copy_cb (GtkAction *action,
                          ETaskShellView *task_shell_view)
{
	/* FIXME */
}

static void
action_task_list_delete_cb (GtkAction *action,
                            ETaskShellView *task_shell_view)
{
	/* FIXME */
}

static void
action_task_list_new_cb (GtkAction *action,
                         ETaskShellView *task_shell_view)
{
}

static void
action_task_list_properties_cb (GtkAction *action,
                                ETaskShellView *task_shell_view)
{
}

static void
action_task_mark_complete_cb (GtkAction *action,
                              ETaskShellView *task_shell_view)
{
}

static void
action_task_open_cb (GtkAction *action,
                     ETaskShellView *task_shell_view)
{
	ETaskShellContent *task_shell_content;
	ETaskTable *task_table;

	task_shell_content = task_shell_view->priv->task_shell_content;
	task_table = e_task_shell_content_get_task_table (task_shell_content);
	e_task_table_open_selected (task_table);
}

static void
action_task_preview_cb (GtkToggleAction *action,
                        ETaskShellView *task_shell_view)
{
	ETaskShellContent *task_shell_content;
	gboolean visible;

	task_shell_content = task_shell_view->priv->task_shell_content;
	visible = gtk_toggle_action_get_active (action);
	e_task_shell_content_set_preview_visible (task_shell_content, visible);
}

static void
action_task_print_cb (GtkAction *action,
                      ETaskShellView *task_shell_view)
{
	ETaskShellContent *task_shell_content;
	ETaskTable *task_table;
	ETable *table;

	task_shell_content = task_shell_view->priv->task_shell_content;
	task_table = e_task_shell_content_get_task_table (task_shell_content);
	table = e_task_table_get_table (task_table);

	print_table (
		table, _("Print Tasks"), _("Tasks"),
		GTK_PRINT_OPERATION_ACTION_PRINT_DIALOG);
}

static void
action_task_print_preview_cb (GtkAction *action,
                              ETaskShellView *task_shell_view)
{
	ETaskShellContent *task_shell_content;
	ETaskTable *task_table;
	ETable *table;

	task_shell_content = task_shell_view->priv->task_shell_content;
	task_table = e_task_shell_content_get_task_table (task_shell_content);
	table = e_task_table_get_table (task_table);

	print_table (
		table, _("Print Tasks"), _("Tasks"),
		GTK_PRINT_OPERATION_ACTION_PREVIEW);
}

static void
action_task_purge_cb (GtkAction *action,
                      ETaskShellView *task_shell_view)
{
}

static GtkActionEntry task_entries[] = {

	{ "task-assign",
	  NULL,
	  N_("_Assign Task"),
	  NULL,
	  NULL,  /* XXX Add a tooltip! */
	  G_CALLBACK (action_task_assign_cb) },

	{ "task-clipboard-copy",
	  GTK_STOCK_COPY,
	  NULL,
	  NULL,
	  N_("Copy selected tasks"),
	  G_CALLBACK (action_task_clipboard_copy_cb) },

	{ "task-clipboard-cut",
	  GTK_STOCK_CUT,
	  NULL,
	  NULL,
	  N_("Cut selected tasks"),
	  G_CALLBACK (action_task_clipboard_cut_cb) },

	{ "task-clipboard-paste",
	  GTK_STOCK_PASTE,
	  NULL,
	  NULL,
	  N_("Paste tasks from the clipboard"),
	  G_CALLBACK (action_task_clipboard_paste_cb) },

	{ "task-delete",
	  GTK_STOCK_DELETE,
	  NULL,
	  NULL,
	  N_("Delete selected tasks"),
	  G_CALLBACK (action_task_delete_cb) },

	{ "task-forward",
	  "mail-forward",
	  N_("_Forward as iCalendar"),
	  "<Control>f",
	  NULL,  /* XXX Add a tooltip! */
	  G_CALLBACK (action_task_forward_cb) },

	{ "task-list-copy",
	  GTK_STOCK_COPY,
	  N_("Copy..."),
	  NULL,
	  NULL,  /* XXX Add a tooltip! */
	  G_CALLBACK (action_task_list_copy_cb) },

	{ "task-list-delete",
	  GTK_STOCK_DELETE,
	  N_("_Delete"),
	  NULL,
	  NULL,  /* XXX Add a tooltip! */
	  G_CALLBACK (action_task_list_delete_cb) },

	{ "task-list-new",
	  "stock_todo",
	  N_("_New Task List"),
	  NULL,
	  N_("Create a new task list"),
	  G_CALLBACK (action_task_list_new_cb) },

	{ "task-list-properties",
	  GTK_STOCK_PROPERTIES,
	  NULL,
	  NULL,
	  NULL,  /* XXX Add a tooltip! */
	  G_CALLBACK (action_task_list_properties_cb) },

	{ "task-mark-complete",
	  NULL,
	  N_("Mar_k as Complete"),
	  "<Control>k",
	  N_("Mark selected tasks as complete"),
	  G_CALLBACK (action_task_mark_complete_cb) },

	{ "task-open",
	  NULL,
	  N_("_Open Task"),
	  "<Control>o",
	  N_("View the selected task"),
	  G_CALLBACK (action_task_open_cb) },

	{ "task-print",
	  GTK_STOCK_PRINT,
	  NULL,
	  NULL,
	  N_("Print the list of tasks"),
	  G_CALLBACK (action_task_print_cb) },

	{ "task-print-preview",
	  GTK_STOCK_PRINT_PREVIEW,
	  NULL,
	  NULL,
	  N_("Preview the list of tasks to be printed"),
	  G_CALLBACK (action_task_print_preview_cb) },

	{ "task-purge",
	  NULL,
	  N_("Purg_e"),
	  "<Control>e",
	  N_("Delete completed tasks"),
	  G_CALLBACK (action_task_purge_cb) }
};

static GtkToggleActionEntry task_toggle_entries[] = {

	{ "task-preview",
	  NULL,
	  N_("Task _Preview"),
	  "<Control>m",
	  N_("Show task preview pane"),
	  G_CALLBACK (action_task_preview_cb),
	  TRUE }
};

void
e_task_shell_view_actions_init (ETaskShellView *task_shell_view)
{
	EShellView *shell_view;
	EShellWindow *shell_window;
	GtkActionGroup *action_group;
	GtkUIManager *manager;
	const gchar *domain;

	shell_view = E_SHELL_VIEW (task_shell_view);
	shell_window = e_shell_view_get_shell_window (shell_view);
	manager = e_shell_window_get_ui_manager (shell_window);
	domain = GETTEXT_PACKAGE;

	e_load_ui_definition (manager, "evolution-tasks.ui");

	action_group = task_shell_view->priv->task_actions;
	gtk_action_group_set_translation_domain (action_group, domain);
	gtk_action_group_add_actions (
		action_group, task_entries,
		G_N_ELEMENTS (task_entries), task_shell_view);
	gtk_action_group_add_toggle_actions (
		action_group, task_toggle_entries,
		G_N_ELEMENTS (task_toggle_entries), task_shell_view);
	gtk_ui_manager_insert_action_group (manager, action_group, 0);
}
