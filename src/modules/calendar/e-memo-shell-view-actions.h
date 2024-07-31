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

#include <shell/e-shell-view.h>

/* Memo Actions */
#define E_SHELL_VIEW_ACTION_MEMO_DELETE(view) \
	E_SHELL_VIEW_ACTION ((view), "memo-delete")
#define E_SHELL_VIEW_ACTION_MEMO_FIND(view) \
	E_SHELL_VIEW_ACTION ((view), "memo-find")
#define E_SHELL_VIEW_ACTION_MEMO_FORWARD(view) \
	E_SHELL_VIEW_ACTION ((view), "memo-forward")
#define E_SHELL_VIEW_ACTION_MEMO_NEW(view) \
	E_SHELL_VIEW_ACTION ((view), "memo-new")
#define E_SHELL_VIEW_ACTION_MEMO_OPEN(view) \
	E_SHELL_VIEW_ACTION ((view), "memo-open")
#define E_SHELL_VIEW_ACTION_MEMO_OPEN_URL(view) \
	E_SHELL_VIEW_ACTION ((view), "memo-open-url")
#define E_SHELL_VIEW_ACTION_MEMO_PREVIEW(view) \
	E_SHELL_VIEW_ACTION ((view), "memo-preview")
#define E_SHELL_VIEW_ACTION_MEMO_PRINT(view) \
	E_SHELL_VIEW_ACTION ((view), "memo-print")
#define E_SHELL_VIEW_ACTION_MEMO_SAVE_AS(view) \
	E_SHELL_VIEW_ACTION ((view), "memo-save-as")
#define E_SHELL_VIEW_ACTION_MEMO_VIEW_CLASSIC(view) \
	E_SHELL_VIEW_ACTION ((view), "memo-view-classic")
#define E_SHELL_VIEW_ACTION_MEMO_VIEW_VERTICAL(view) \
	E_SHELL_VIEW_ACTION ((view), "memo-view-vertical")

/* Memo List Actions */
#define E_SHELL_VIEW_ACTION_MEMO_LIST_COPY(view) \
	E_SHELL_VIEW_ACTION ((view), "memo-list-copy")
#define E_SHELL_VIEW_ACTION_MEMO_LIST_DELETE(view) \
	E_SHELL_VIEW_ACTION ((view), "memo-list-delete")
#define E_SHELL_VIEW_ACTION_MEMO_LIST_NEW(view) \
	E_SHELL_VIEW_ACTION ((view), "memo-list-new")
#define E_SHELL_VIEW_ACTION_MEMO_LIST_PRINT(view) \
	E_SHELL_VIEW_ACTION ((view), "memo-list-print")
#define E_SHELL_VIEW_ACTION_MEMO_LIST_PRINT_PREVIEW(view) \
	E_SHELL_VIEW_ACTION ((view), "memo-list-print-preview")
#define E_SHELL_VIEW_ACTION_MEMO_LIST_PROPERTIES(view) \
	E_SHELL_VIEW_ACTION ((view), "memo-list-properties")
#define E_SHELL_VIEW_ACTION_MEMO_LIST_REFRESH(view) \
	E_SHELL_VIEW_ACTION ((view), "memo-list-refresh")
#define E_SHELL_VIEW_ACTION_MEMO_LIST_REFRESH_BACKEND(view) \
	E_SHELL_VIEW_ACTION ((view), "memo-list-refresh-backend")
#define E_SHELL_VIEW_ACTION_MEMO_LIST_RENAME(view) \
	E_SHELL_VIEW_ACTION ((view), "memo-list-rename")
#define E_SHELL_VIEW_ACTION_MEMO_LIST_SELECT_ALL(view) \
	E_SHELL_VIEW_ACTION ((view), "memo-list-select-all")
#define E_SHELL_VIEW_ACTION_MEMO_LIST_SELECT_ONE(view) \
	E_SHELL_VIEW_ACTION ((view), "memo-list-select-one")

/* Memo Query Actions */
#define E_SHELL_VIEW_ACTION_MEMO_FILTER_ANY_CATEGORY(view) \
	E_SHELL_VIEW_ACTION ((view), "memo-filter-any-category")
#define E_SHELL_VIEW_ACTION_MEMO_FILTER_UNMATCHED(view) \
	E_SHELL_VIEW_ACTION ((view), "memo-filter-unmatched")
#define E_SHELL_VIEW_ACTION_MEMO_SEARCH_ADVANCED_HIDDEN(view) \
	E_SHELL_VIEW_ACTION ((view), "memo-search-advanced-hidden")
#define E_SHELL_VIEW_ACTION_MEMO_SEARCH_ANY_FIELD_CONTAINS(view) \
	E_SHELL_VIEW_ACTION ((view), "memo-search-any-field-contains")
#define E_SHELL_VIEW_ACTION_MEMO_SEARCH_DESCRIPTION_CONTAINS(view) \
	E_SHELL_VIEW_ACTION ((view), "memo-search-description-contains")
#define E_SHELL_VIEW_ACTION_MEMO_SEARCH_SUMMARY_CONTAINS(view) \
	E_SHELL_VIEW_ACTION ((view), "memo-search-summary-contains")

#endif /* E_MEMO_SHELL_VIEW_ACTIONS_H */
