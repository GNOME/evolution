/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* e-memo-shell-view-actions.h
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

#ifndef E_MEMO_SHELL_VIEW_ACTIONS_H
#define E_MEMO_SHELL_VIEW_ACTIONS_H

#include <shell/e-shell-window-actions.h>

/* Memo Actions */
#define E_SHELL_WINDOW_ACTION_MEMO_CLIPBOARD_COPY(window) \
	E_SHELL_WINDOW_ACTION ((window), "memo-clipboard-copy")
#define E_SHELL_WINDOW_ACTION_MEMO_CLIPBOARD_CUT(window) \
	E_SHELL_WINDOW_ACTION ((window), "memo-clipboard-cut")
#define E_SHELL_WINDOW_ACTION_MEMO_CLIPBOARD_PASTE(window) \
	E_SHELL_WINDOW_ACTION ((window), "memo-clipboard-paste")
#define E_SHELL_WINDOW_ACTION_MEMO_DELETE(window) \
	E_SHELL_WINDOW_ACTION ((window), "memo-delete")
#define E_SHELL_WINDOW_ACTION_MEMO_OPEN(window) \
	E_SHELL_WINDOW_ACTION ((window), "memo-open")
#define E_SHELL_WINDOW_ACTION_MEMO_PREVIEW(window) \
	E_SHELL_WINDOW_ACTION ((window), "memo-preview")
#define E_SHELL_WINDOW_ACTION_MEMO_PRINT(window) \
	E_SHELL_WINDOW_ACTION ((window), "memo-print")
#define E_SHELL_WINDOW_ACTION_MEMO_PRINT_PREVIEW(window) \
	E_SHELL_WINDOW_ACTION ((window), "memo-print-preview")

/* Action Groups */
#define E_SHELL_WINDOW_ACTION_GROUP_MEMOS(window) \
	E_SHELL_WINDOW_ACTION_GROUP ((window), "memos")

#endif /* E_MEMO_SHELL_VIEW_ACTIONS_H */
