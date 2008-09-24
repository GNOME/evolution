/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* e-cal-shell-view-private.c
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

void
e_cal_shell_view_private_init (ECalShellView *cal_shell_view,
                               EShellViewClass *shell_view_class)
{
	ECalShellViewPrivate *priv = cal_shell_view->priv;
	ESourceList *source_list;
	GObject *object;

	object = G_OBJECT (shell_view_class->type_module);
	source_list = g_object_get_data (object, "source-list");
	g_return_if_fail (E_IS_SOURCE_LIST (source_list));

	priv->source_list = g_object_ref (source_list);
	priv->calendar_actions = gtk_action_group_new ("calendars");
}

void
e_cal_shell_view_private_constructed (ECalShellView *cal_shell_view)
{
	ECalShellViewPrivate *priv = cal_shell_view->priv;
	EShellContent *shell_content;
	EShellSidebar *shell_sidebar;
	EShellView *shell_view;

	shell_view = E_SHELL_VIEW (cal_shell_view);
	shell_content = e_shell_view_get_shell_content (shell_view);
	shell_sidebar = e_shell_view_get_shell_sidebar (shell_view);

	/* Cache these to avoid lots of awkward casting. */
	priv->cal_shell_content = g_object_ref (shell_content);
	priv->cal_shell_sidebar = g_object_ref (shell_sidebar);
}

void
e_cal_shell_view_private_dispose (ECalShellView *cal_shell_view)
{
	ECalShellViewPrivate *priv = cal_shell_view->priv;

	DISPOSE (priv->source_list);

	DISPOSE (priv->calendar_actions);

	DISPOSE (priv->cal_shell_content);
	DISPOSE (priv->cal_shell_sidebar);
}

void
e_cal_shell_view_private_finalize (ECalShellView *cal_shell_view)
{
	ECalShellViewPrivate *priv = cal_shell_view->priv;
}
