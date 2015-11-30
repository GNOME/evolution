/*
 * e-memo-shell-view-actions.h
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 *
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#ifndef E_MEMO_SHELL_VIEW_ACTIONS_H
#define E_MEMO_SHELL_VIEW_ACTIONS_H

#include <shell/e-shell-window-actions.h>

/* Memo Actions */
#define E_SHELL_WINDOW_ACTION_MEMO_DELETE(window) \
	E_SHELL_WINDOW_ACTION ((window), "memo-delete")
#define E_SHELL_WINDOW_ACTION_MEMO_FIND(window) \
	E_SHELL_WINDOW_ACTION ((window), "memo-find")
#define E_SHELL_WINDOW_ACTION_MEMO_FORWARD(window) \
	E_SHELL_WINDOW_ACTION ((window), "memo-forward")
#define E_SHELL_WINDOW_ACTION_MEMO_NEW(window) \
	E_SHELL_WINDOW_ACTION ((window), "memo-new")
#define E_SHELL_WINDOW_ACTION_MEMO_OPEN(window) \
	E_SHELL_WINDOW_ACTION ((window), "memo-open")
#define E_SHELL_WINDOW_ACTION_MEMO_OPEN_URL(window) \
	E_SHELL_WINDOW_ACTION ((window), "memo-open-url")
#define E_SHELL_WINDOW_ACTION_MEMO_PREVIEW(window) \
	E_SHELL_WINDOW_ACTION ((window), "memo-preview")
#define E_SHELL_WINDOW_ACTION_MEMO_PRINT(window) \
	E_SHELL_WINDOW_ACTION ((window), "memo-print")
#define E_SHELL_WINDOW_ACTION_MEMO_SAVE_AS(window) \
	E_SHELL_WINDOW_ACTION ((window), "memo-save-as")
#define E_SHELL_WINDOW_ACTION_MEMO_VIEW_CLASSIC(window) \
	E_SHELL_WINDOW_ACTION ((window), "memo-view-classic")
#define E_SHELL_WINDOW_ACTION_MEMO_VIEW_VERTICAL(window) \
	E_SHELL_WINDOW_ACTION ((window), "memo-view-vertical")

/* Memo List Actions */
#define E_SHELL_WINDOW_ACTION_MEMO_LIST_COPY(window) \
	E_SHELL_WINDOW_ACTION ((window), "memo-list-copy")
#define E_SHELL_WINDOW_ACTION_MEMO_LIST_DELETE(window) \
	E_SHELL_WINDOW_ACTION ((window), "memo-list-delete")
#define E_SHELL_WINDOW_ACTION_MEMO_LIST_NEW(window) \
	E_SHELL_WINDOW_ACTION ((window), "memo-list-new")
#define E_SHELL_WINDOW_ACTION_MEMO_LIST_PRINT(window) \
	E_SHELL_WINDOW_ACTION ((window), "memo-list-print")
#define E_SHELL_WINDOW_ACTION_MEMO_LIST_PRINT_PREVIEW(window) \
	E_SHELL_WINDOW_ACTION ((window), "memo-list-print-preview")
#define E_SHELL_WINDOW_ACTION_MEMO_LIST_PROPERTIES(window) \
	E_SHELL_WINDOW_ACTION ((window), "memo-list-properties")
#define E_SHELL_WINDOW_ACTION_MEMO_LIST_REFRESH(window) \
	E_SHELL_WINDOW_ACTION ((window), "memo-list-refresh")
#define E_SHELL_WINDOW_ACTION_MEMO_LIST_RENAME(window) \
	E_SHELL_WINDOW_ACTION ((window), "memo-list-rename")
#define E_SHELL_WINDOW_ACTION_MEMO_LIST_SELECT_ALL(window) \
	E_SHELL_WINDOW_ACTION ((window), "memo-list-select-all")
#define E_SHELL_WINDOW_ACTION_MEMO_LIST_SELECT_ONE(window) \
	E_SHELL_WINDOW_ACTION ((window), "memo-list-select-one")

/* Memo Query Actions */
#define E_SHELL_WINDOW_ACTION_MEMO_FILTER_ANY_CATEGORY(window) \
	E_SHELL_WINDOW_ACTION ((window), "memo-filter-any-category")
#define E_SHELL_WINDOW_ACTION_MEMO_FILTER_UNMATCHED(window) \
	E_SHELL_WINDOW_ACTION ((window), "memo-filter-unmatched")
#define E_SHELL_WINDOW_ACTION_MEMO_SEARCH_ADVANCED_HIDDEN(window) \
	E_SHELL_WINDOW_ACTION ((window), "memo-search-advanced-hidden")
#define E_SHELL_WINDOW_ACTION_MEMO_SEARCH_ANY_FIELD_CONTAINS(window) \
	E_SHELL_WINDOW_ACTION ((window), "memo-search-any-field-contains")
#define E_SHELL_WINDOW_ACTION_MEMO_SEARCH_DESCRIPTION_CONTAINS(window) \
	E_SHELL_WINDOW_ACTION ((window), "memo-search-description-contains")
#define E_SHELL_WINDOW_ACTION_MEMO_SEARCH_SUMMARY_CONTAINS(window) \
	E_SHELL_WINDOW_ACTION ((window), "memo-search-summary-contains")

/* Action Groups */
#define E_SHELL_WINDOW_ACTION_GROUP_MEMOS(window) \
	E_SHELL_WINDOW_ACTION_GROUP ((window), "memos")
#define E_SHELL_WINDOW_ACTION_GROUP_MEMOS_FILTER(window) \
	E_SHELL_WINDOW_ACTION_GROUP ((window), "memos-filter")

#endif /* E_MEMO_SHELL_VIEW_ACTIONS_H */
