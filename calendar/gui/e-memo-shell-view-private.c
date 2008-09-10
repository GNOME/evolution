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
e_memo_shell_view_private_init (EMemoShellView *memo_shell_view)
{
	EMemoShellViewPrivate *priv = memo_shell_view->priv;

	priv->memo_actions = gtk_action_group_new ("memos");
}

void
e_memo_shell_view_private_constructed (EMemoShellView *memo_shell_view)
{
	EMemoShellViewPrivate *priv = memo_shell_view->priv;
}

void
e_memo_shell_view_private_dispose (EMemoShellView *memo_shell_view)
{
	EMemoShellViewPrivate *priv = memo_shell_view->priv;

	DISPOSE (priv->memo_actions);
}

void
e_memo_shell_view_private_finalize (EMemoShellView *memo_shell_view)
{
	EMemoShellViewPrivate *priv = memo_shell_view->priv;
}
