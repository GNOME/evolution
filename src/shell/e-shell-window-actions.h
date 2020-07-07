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

#define E_SHELL_WINDOW_ACTION(window, action_name) \
	(g_action_map_lookup_action (              \
		G_ACTION_MAP ((window)),           \
		(action_name)))

#define E_SHELL_WINDOW_ACTIVATE_ACTION(window, action_name, parameter) \
	(g_action_group_activate_action (                              \
		G_ACTION_GROUP ((window)),                             \
		(action_name),                                         \
		(parameter)))

#define E_SHELL_WINDOW_CHANGE_ACTION_STATE(action, vtype, vval) do {              \
	if (g_variant_type_equal ((vtype), g_action_get_state_type ((action)))) { \
		GVariant *value = g_variant_new ((vtype), (vval));                \
		g_action_change_state (action, value);                            \
	}} while (0)

#define E_SHELL_WINDOW_ACTIONX(window, name) \
	(e_shell_window_get_action (E_SHELL_WINDOW (window), (name)))

#define E_SHELL_WINDOW_ACTION_GROUP(window, name) \
	(e_shell_window_get_action_group (E_SHELL_WINDOW (window), (name)))

/* Actions */
#define E_SHELL_WINDOW_ACTION_COPY_CLIPBOARD(window) \
	E_SHELL_WINDOW_ACTIONX ((window), "copy-clipboard")
#define E_SHELL_WINDOW_ACTION_CUT_CLIPBOARD(window) \
	E_SHELL_WINDOW_ACTIONX ((window), "cut-clipboard")
#define E_SHELL_WINDOW_ACTION_DELETE_SELECTION(window) \
	E_SHELL_WINDOW_ACTIONX ((window), "delete-selection")
#define E_SHELL_WINDOW_ACTION_GAL_CUSTOM_VIEW(window) \
	E_SHELL_WINDOW_ACTIONX ((window), "gal-custom-view")
#define E_SHELL_WINDOW_ACTION_GAL_CUSTOMIZE_VIEW(window) \
	E_SHELL_WINDOW_ACTIONX ((window), "gal-customize-view")
#define E_SHELL_WINDOW_ACTION_GAL_DELETE_VIEW(window) \
	E_SHELL_WINDOW_ACTIONX ((window), "gal-delete-view")
#define E_SHELL_WINDOW_ACTION_GAL_SAVE_CUSTOM_VIEW(window) \
	E_SHELL_WINDOW_ACTIONX ((window), "gal-save-custom-view")
#define E_SHELL_WINDOW_ACTION_PASTE_CLIPBOARD(window) \
	E_SHELL_WINDOW_ACTIONX ((window), "paste-clipboard")
#define E_SHELL_WINDOW_ACTION_SAVED_SEARCHES(window) \
	E_SHELL_WINDOW_ACTIONX ((window), "saved-searches")
#define E_SHELL_WINDOW_ACTION_SEARCH_OPTIONS(window) \
	E_SHELL_WINDOW_ACTIONX ((window), "search-options")
#define E_SHELL_WINDOW_ACTION_SELECT_ALL(window) \
	E_SHELL_WINDOW_ACTIONX ((window), "select-all")
#define E_SHELL_WINDOW_ACTION_SWITCHER_INITIAL(window) \
	E_SHELL_WINDOW_ACTIONX ((window), "switcher-initial")
#define E_SHELL_WINDOW_ACTION_WORK_OFFLINE(window) \
	E_SHELL_WINDOW_ACTIONX ((window), "work-offline")
#define E_SHELL_WINDOW_ACTION_WORK_ONLINE(window) \
	E_SHELL_WINDOW_ACTIONX ((window), "work-online")

/* Action Groups */
#define E_SHELL_WINDOW_ACTION_GROUP_CUSTOM_RULES(window) \
	E_SHELL_WINDOW_ACTION_GROUP ((window), "custom-rules")
#define E_SHELL_WINDOW_ACTION_GROUP_GAL_VIEW(window) \
	E_SHELL_WINDOW_ACTION_GROUP ((window), "gal-view")
#define E_SHELL_WINDOW_ACTION_GROUP_LOCKDOWN_APPLICATION_HANDLERS(window) \
	E_SHELL_WINDOW_ACTION_GROUP ((window), "lockdown-application-handlers")
#define E_SHELL_WINDOW_ACTION_GROUP_LOCKDOWN_PRINTING(window) \
	E_SHELL_WINDOW_ACTION_GROUP ((window), "lockdown-printing")
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
