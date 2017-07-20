/*
 * e-shell-window-actions.h
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

#ifndef E_SHELL_WINDOW_ACTIONS_H
#define E_SHELL_WINDOW_ACTIONS_H

#define E_SHELL_WINDOW_ACTION(window, name) \
	(e_shell_window_get_action (E_SHELL_WINDOW (window), (name)))

#define E_SHELL_WINDOW_ACTION_GROUP(window, name) \
	(e_shell_window_get_action_group (E_SHELL_WINDOW (window), (name)))

/* Actions */
#define E_SHELL_WINDOW_ACTION_ABOUT(window) \
	E_SHELL_WINDOW_ACTION ((window), "about")
#define E_SHELL_WINDOW_ACTION_ACCOUNTS(window) \
	E_SHELL_WINDOW_ACTION ((window), "accounts")
#define E_SHELL_WINDOW_ACTION_CLOSE(window) \
	E_SHELL_WINDOW_ACTION ((window), "close")
#define E_SHELL_WINDOW_ACTION_CONTENTS(window) \
	E_SHELL_WINDOW_ACTION ((window), "contents")
#define E_SHELL_WINDOW_ACTION_COPY_CLIPBOARD(window) \
	E_SHELL_WINDOW_ACTION ((window), "copy-clipboard")
#define E_SHELL_WINDOW_ACTION_CUT_CLIPBOARD(window) \
	E_SHELL_WINDOW_ACTION ((window), "cut-clipboard")
#define E_SHELL_WINDOW_ACTION_DELETE_SELECTION(window) \
	E_SHELL_WINDOW_ACTION ((window), "delete-selection")
#define E_SHELL_WINDOW_ACTION_GAL_CUSTOM_VIEW(window) \
	E_SHELL_WINDOW_ACTION ((window), "gal-custom-view")
#define E_SHELL_WINDOW_ACTION_GAL_DELETE_VIEW(window) \
	E_SHELL_WINDOW_ACTION ((window), "gal-delete-view")
#define E_SHELL_WINDOW_ACTION_GAL_SAVE_CUSTOM_VIEW(window) \
	E_SHELL_WINDOW_ACTION ((window), "gal-save-custom-view")
#define E_SHELL_WINDOW_ACTION_IMPORT(window) \
	E_SHELL_WINDOW_ACTION ((window), "import")
#define E_SHELL_WINDOW_ACTION_NEW_WINDOW(window) \
	E_SHELL_WINDOW_ACTION ((window), "new-window")
#define E_SHELL_WINDOW_ACTION_PAGE_SETUP(window) \
	E_SHELL_WINDOW_ACTION ((window), "page-setup")
#define E_SHELL_WINDOW_ACTION_PASTE_CLIPBOARD(window) \
	E_SHELL_WINDOW_ACTION ((window), "paste-clipboard")
#define E_SHELL_WINDOW_ACTION_PREFERENCES(window) \
	E_SHELL_WINDOW_ACTION ((window), "preferences")
#define E_SHELL_WINDOW_ACTION_QUICK_REFERENCE(window) \
	E_SHELL_WINDOW_ACTION ((window), "quick-reference")
#define E_SHELL_WINDOW_ACTION_QUIT(window) \
	E_SHELL_WINDOW_ACTION ((window), "quit")
#define E_SHELL_WINDOW_ACTION_SEARCH_ADVANCED(window) \
	E_SHELL_WINDOW_ACTION ((window), "search-advanced")
#define E_SHELL_WINDOW_ACTION_SEARCH_CLEAR(window) \
	E_SHELL_WINDOW_ACTION ((window), "search-clear")
#define E_SHELL_WINDOW_ACTION_SEARCH_EDIT(window) \
	E_SHELL_WINDOW_ACTION ((window), "search-edit")
#define E_SHELL_WINDOW_ACTION_SEARCH_OPTIONS(window) \
	E_SHELL_WINDOW_ACTION ((window), "search-options")
#define E_SHELL_WINDOW_ACTION_SEARCH_QUICK(window) \
	E_SHELL_WINDOW_ACTION ((window), "search-quick")
#define E_SHELL_WINDOW_ACTION_SEARCH_SAVE(window) \
	E_SHELL_WINDOW_ACTION ((window), "search-save")
#define E_SHELL_WINDOW_ACTION_SELECT_ALL(window) \
	E_SHELL_WINDOW_ACTION ((window), "select-all")
#define E_SHELL_WINDOW_ACTION_SHOW_MENUBAR(window) \
	E_SHELL_WINDOW_ACTION ((window), "show-menubar")
#define E_SHELL_WINDOW_ACTION_SHOW_SIDEBAR(window) \
	E_SHELL_WINDOW_ACTION ((window), "show-sidebar")
#define E_SHELL_WINDOW_ACTION_SHOW_SWITCHER(window) \
	E_SHELL_WINDOW_ACTION ((window), "show-switcher")
#define E_SHELL_WINDOW_ACTION_SHOW_TASKBAR(window) \
	E_SHELL_WINDOW_ACTION ((window), "show-taskbar")
#define E_SHELL_WINDOW_ACTION_SHOW_TOOLBAR(window) \
	E_SHELL_WINDOW_ACTION ((window), "show-toolbar")
#define E_SHELL_WINDOW_ACTION_SUBMIT_BUG(window) \
	E_SHELL_WINDOW_ACTION ((window), "submit-bug")
#define E_SHELL_WINDOW_ACTION_SWITCHER_INITIAL(window) \
	E_SHELL_WINDOW_ACTION ((window), "switcher-initial")
#define E_SHELL_WINDOW_ACTION_SWITCHER_MENU(window) \
	E_SHELL_WINDOW_ACTION ((window), "switcher-menu")
#define E_SHELL_WINDOW_ACTION_SWITCHER_STYLE_BOTH(window) \
	E_SHELL_WINDOW_ACTION ((window), "switcher-style-both")
#define E_SHELL_WINDOW_ACTION_SWITCHER_STYLE_ICONS(window) \
	E_SHELL_WINDOW_ACTION ((window), "switcher-style-icons")
#define E_SHELL_WINDOW_ACTION_SWITCHER_STYLE_TEXT(window) \
	E_SHELL_WINDOW_ACTION ((window), "switcher-style-text")
#define E_SHELL_WINDOW_ACTION_SWITCHER_STYLE_USER(window) \
	E_SHELL_WINDOW_ACTION ((window), "switcher-style-user")
#define E_SHELL_WINDOW_ACTION_WORK_OFFLINE(window) \
	E_SHELL_WINDOW_ACTION ((window), "work-offline")
#define E_SHELL_WINDOW_ACTION_WORK_ONLINE(window) \
	E_SHELL_WINDOW_ACTION ((window), "work-online")

/* Action Groups */
#define E_SHELL_WINDOW_ACTION_GROUP_CUSTOM_RULES(window) \
	E_SHELL_WINDOW_ACTION_GROUP ((window), "custom-rules")
#define E_SHELL_WINDOW_ACTION_GROUP_GAL_VIEW(window) \
	E_SHELL_WINDOW_ACTION_GROUP ((window), "gal-view")
#define E_SHELL_WINDOW_ACTION_GROUP_LOCKDOWN_APPLICATION_HANDLERS(window) \
	E_SHELL_WINDOW_ACTION_GROUP ((window), "lockdown-application-handlers")
#define E_SHELL_WINDOW_ACTION_GROUP_LOCKDOWN_PRINTING(window) \
	E_SHELL_WINDOW_ACTION_GROUP ((window), "lockdown-printing")
#define E_SHELL_WINDOW_ACTION_GROUP_LOCKDOWN_PRINT_SETUP(window) \
	E_SHELL_WINDOW_ACTION_GROUP ((window), "lockdown-print-setup")
#define E_SHELL_WINDOW_ACTION_GROUP_LOCKDOWN_SAVE_TO_DISK(window) \
	E_SHELL_WINDOW_ACTION_GROUP ((window), "lockdown-save-to-disk")
#define E_SHELL_WINDOW_ACTION_GROUP_NEW_ITEM(window) \
	E_SHELL_WINDOW_ACTION_GROUP ((window), "new-item")
#define E_SHELL_WINDOW_ACTION_GROUP_NEW_SOURCE(window) \
	E_SHELL_WINDOW_ACTION_GROUP ((window), "new-source")
#define E_SHELL_WINDOW_ACTION_GROUP_SHELL(window) \
	E_SHELL_WINDOW_ACTION_GROUP ((window), "shell")
#define E_SHELL_WINDOW_ACTION_GROUP_SWITCHER(window) \
	E_SHELL_WINDOW_ACTION_GROUP ((window), "switcher")
#define E_SHELL_WINDOW_ACTION_GROUP_NEW_WINDOW(window) \
	E_SHELL_WINDOW_ACTION_GROUP ((window), "new-window")

#endif /* E_SHELL_WINDOW_ACTIONS_H */
