/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* e-memo-shell-view-private.c
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

void
e_memo_shell_view_private_init (EMemoShellView *memo_shell_view,
                                EShellViewClass *shell_view_class)
{
	EMemoShellViewPrivate *priv = memo_shell_view->priv;
	ESourceList *source_list;
	GObject *object;

	object = G_OBJECT (shell_view_class->type_module);
	source_list = g_object_get_data (object, "source-list");
	g_return_if_fail (E_IS_SOURCE_LIST (source_list));

	priv->source_list = g_object_ref (source_list);
	priv->memo_actions = gtk_action_group_new ("memos");
	priv->activity_handler = e_activity_handler_new ();
}

void
e_memo_shell_view_private_constructed (EMemoShellView *memo_shell_view)
{
	EMemoShellViewPrivate *priv = memo_shell_view->priv;
	EShellContent *shell_content;
	EShellTaskbar *shell_taskbar;
	EShellView *shell_view;
	GtkWidget *container;
	GtkWidget *widget;

	shell_view = E_SHELL_VIEW (memo_shell_view);

	/* Construct view widgets. */

	widget = e_memos_new ();
	shell_content = e_shell_view_get_content (shell_view);
	gtk_container_add (GTK_CONTAINER (shell_content), widget);
	priv->memos = g_object_ref (widget);
	gtk_widget_show (widget);

	shell_taskbar = e_shell_view_get_taskbar (shell_view);
	e_activity_handler_attach_task_bar (
		priv->activity_handler, shell_taskbar);

	e_memo_shell_view_actions_update (memo_shell_view);
}

void
e_memo_shell_view_private_dispose (EMemoShellView *memo_shell_view)
{
	EMemoShellViewPrivate *priv = memo_shell_view->priv;

	DISPOSE (priv->source_list);

	DISPOSE (priv->memo_actions);

	DISPOSE (priv->memos);

	DISPOSE (priv->activity_handler);
}

void
e_memo_shell_view_private_finalize (EMemoShellView *memo_shell_view)
{
	EMemoShellViewPrivate *priv = memo_shell_view->priv;
}
