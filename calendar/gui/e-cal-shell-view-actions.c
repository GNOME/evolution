/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* e-cal-shell-view-actions.c
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

#include "e-cal-shell-view-private.h"

static GtkActionEntry cal_entries[] = {
};

void
e_cal_shell_view_actions_init (ECalShellView *cal_shell_view)
{
	EShellView *shell_view;
	EShellWindow *shell_window;
	GtkActionGroup *action_group;
	GtkUIManager *manager;
	const gchar *domain;

	shell_view = E_SHELL_VIEW (cal_shell_view);
	shell_window = e_shell_view_get_shell_window (shell_view);
	manager = e_shell_window_get_ui_manager (shell_window);
	domain = GETTEXT_PACKAGE;

	e_load_ui_definition (manager, "evolution-cals.ui");

	action_group = cal_shell_view->priv->cal_actions;
	gtk_action_group_set_translation_domain (action_group, domain);
	gtk_action_group_add_actions (
		action_group, cal_entries,
		G_N_ELEMENTS (cal_entries), cal_shell_view);
	gtk_ui_manager_insert_action_group (manager, action_group, 0);
}
