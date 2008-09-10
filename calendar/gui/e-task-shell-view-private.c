/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* e-task-shell-view-private.c
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

void
e_task_shell_view_private_init (ETaskShellView *task_shell_view)
{
	ETaskShellViewPrivate *priv = task_shell_view->priv;

	priv->task_actions = gtk_action_group_new ("tasks");
}

void
e_task_shell_view_private_constructed (ETaskShellView *task_shell_view)
{
	ETaskShellViewPrivate *priv = task_shell_view->priv;
}

void
e_task_shell_view_private_dispose (ETaskShellView *task_shell_view)
{
	ETaskShellViewPrivate *priv = task_shell_view->priv;

	DISPOSE (priv->task_actions);
}

void
e_task_shell_view_private_finalize (ETaskShellView *task_shell_view)
{
	ETaskShellViewPrivate *priv = task_shell_view->priv;
}
