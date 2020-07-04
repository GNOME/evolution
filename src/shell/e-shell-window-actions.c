/*
 * e-shell-window-actions.c
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

#include "evolution-config.h"

#include "e-shell-window-private.h"

typedef struct {
	const gchar *name;
	const gchar *property_name;
} EPropertyActionEntry;

static void
e_action_map_add_property_action_entries (GActionMap                 *action_map,
                                          const EPropertyActionEntry *entries,
                                          gint                        n_entries,
                                          gpointer                    object)
{
	GPropertyAction *action;

	for (gint i = 0; i < n_entries; i++) {
		action = g_property_action_new (
			entries[i].name, object, entries[i].property_name);
		g_action_map_add_action (action_map, G_ACTION (action));
		g_object_unref (action);
	}
}

static void
win_clear_search (GSimpleAction *action,
                  GVariant      *parameter,
                  gpointer       user_data)
{
	EShellView *shell_view;
	const gchar *view_name;

	EShellWindow *shell_window = user_data;

	view_name = e_shell_window_get_active_view (shell_window);
	shell_view = e_shell_window_get_shell_view (shell_window, view_name);

	e_shell_view_clear_search (shell_view);
}

static void
win_close (GSimpleAction *action,
           GVariant      *parameter,
           gpointer       user_data)
{
	GtkWidget *widget;
	GdkWindow *window;
	GdkEvent *event;

	EShellWindow *shell_window = user_data;

	widget = GTK_WIDGET (shell_window);
	window = gtk_widget_get_window (widget);

	/* Synthesize a delete_event on this window. */
	event = gdk_event_new (GDK_DELETE);
	event->any.window = g_object_ref (window);
	event->any.send_event = TRUE;
	gtk_main_do_event (event);
	gdk_event_free (event);
}

static void
win_edit_search (GSimpleAction *action,
                 GVariant      *parameter,
                 gpointer       user_data)
{
	EShellView *shell_view;
	EShellContent *shell_content;
	const gchar *view_name;

	EShellWindow *shell_window = user_data;

	view_name = e_shell_window_get_active_view (shell_window);
	shell_view = e_shell_window_get_shell_view (shell_window, view_name);
	shell_content = e_shell_view_get_shell_content (shell_view);

	e_shell_content_run_edit_searches_dialog (shell_content);
	e_shell_window_update_search_menu (shell_window);
}

static void
win_page_setup (GSimpleAction *action,
                GVariant      *parameter,
                gpointer       user_data)
{
	EShellWindow *shell_window = user_data;

	e_print_run_page_setup_dialog (GTK_WINDOW (shell_window));
}

static void
win_save_search (GSimpleAction *action,
                 GVariant      *parameter,
                 gpointer       user_data)
{
	EShellView *shell_view;
	EShellContent *shell_content;
	const gchar *view_name;

	EShellWindow *shell_window = user_data;

	view_name = e_shell_window_get_active_view (shell_window);
	shell_view = e_shell_window_get_shell_view (shell_window, view_name);
	shell_content = e_shell_view_get_shell_content (shell_view);

	e_shell_content_run_save_search_dialog (shell_content);
	e_shell_window_update_search_menu (shell_window);
}

static void
win_search_advanced (GSimpleAction *action,
                     GVariant      *parameter,
                     gpointer       user_data)
{
	EShellView *shell_view;
	EShellContent *shell_content;
	const gchar *view_name;

	EShellWindow *shell_window = user_data;

	view_name = e_shell_window_get_active_view (shell_window);
	shell_view = e_shell_window_get_shell_view (shell_window, view_name);
	shell_content = e_shell_view_get_shell_content (shell_view);

	e_shell_content_run_advanced_search_dialog (shell_content);
	e_shell_window_update_search_menu (shell_window);
}

static void
win_search_quick (GSimpleAction *action,
                  GVariant      *parameter,
                  gpointer       user_data)
{
	EShellView *shell_view;
	const gchar *view_name;

	EShellWindow *shell_window = user_data;

	view_name = e_shell_window_get_active_view (shell_window);
	shell_view = e_shell_window_get_shell_view (shell_window, view_name);

	e_shell_view_execute_search (shell_view);
}

static void
win_style_switcher (GSimpleAction *action,
                    GVariant      *value,
                    gpointer       user_data)
{
	const gchar *stylev;
	GtkToolbarStyle style;

	EShellWindow *shell_window = user_data;

	g_simple_action_set_state (action, value);

	stylev = g_variant_get_string (value, NULL);
	if (g_strcmp0 (stylev, "both") == 0)
		style = GTK_TOOLBAR_BOTH;
	else if (g_strcmp0 (stylev, "icons") == 0)
		style = GTK_TOOLBAR_ICONS;
	else if (g_strcmp0 (stylev, "text") == 0)
		style = GTK_TOOLBAR_TEXT;
	else if (g_strcmp0 (stylev, "user") == 0)
		style = GTK_TOOLBAR_BOTH_HORIZ;
	else
		style = E_SHELL_SWITCHER_DEFAULT_TOOLBAR_STYLE;

	e_shell_switcher_set_style (
		E_SHELL_SWITCHER (shell_window->priv->switcher), style);
}

static GActionEntry win_entries[] = {
	{ "clear-search",    win_clear_search },
	{ "close",           win_close },
	{ "edit-search",     win_edit_search },
	{ "page-setup",      win_page_setup },
	{ "save-search",     win_save_search },
	{ "search-advanced", win_search_advanced },
	{ "search-quick",    win_search_quick },
	{ "style-switcher",  NULL, "s", "'user'", win_style_switcher },
};

static EPropertyActionEntry win_property_entries[] = {
	{ "show-menubar",  "menubar-visible" },
	{ "show-sidebar",  "sidebar-visible" },
	{ "show-switcher", "switcher-visible" },
	{ "show-taskbar",  "taskbar-visible" },
	{ "show-toolbar",  "toolbar-visible" },
};

static void
action_custom_rule_cb (GtkAction *action,
                       EShellWindow *shell_window)
{
	EFilterRule *rule;
	EShellView *shell_view;
	const gchar *view_name;

	rule = g_object_get_data (G_OBJECT (action), "rule");
	g_return_if_fail (rule != NULL);

	view_name = e_shell_window_get_active_view (shell_window);
	shell_view = e_shell_window_get_shell_view (shell_window, view_name);

	rule = g_object_get_data (G_OBJECT (action), "rule");
	g_return_if_fail (E_IS_FILTER_RULE (rule));

	e_shell_view_custom_search (shell_view, rule);
}

/**
 * E_SHELL_WINDOW_ACTION_GAL_DELETE_VIEW:
 * @window: an #EShellWindow
 *
 * Activation of this action deletes the current user-created GAL view.
 *
 * Main menu item: View -> Current View -> Delete Current View
 **/
static void
action_gal_delete_view_cb (GtkAction *action,
                           EShellWindow *shell_window)
{
	EShellView *shell_view;
	GalViewInstance *view_instance;
	const gchar *view_name;
	gchar *gal_view_id;
	gint index = -1;

	view_name = e_shell_window_get_active_view (shell_window);
	shell_view = e_shell_window_get_shell_view (shell_window, view_name);
	view_instance = e_shell_view_get_view_instance (shell_view);
	g_return_if_fail (view_instance != NULL);

	/* XXX This is kinda cumbersome.  The view collection API
	 *     should be using only view ID's, not index numbers. */
	gal_view_id = gal_view_instance_get_current_view_id (view_instance);
	if (gal_view_id != NULL) {
		index = gal_view_collection_get_view_index_by_id (
			view_instance->collection, gal_view_id);
		g_free (gal_view_id);
	}

	gal_view_collection_delete_view (view_instance->collection, index);

	gal_view_collection_save (view_instance->collection);
}

/**
 * E_SHELL_WINDOW_ACTION_GAL_CUSTOM_VIEW:
 * @window: an #EShellWindow
 *
 * This radio action is selected when using a custom GAL view that has
 * not been saved.
 *
 * Main menu item: View -> Current View -> Custom View
 **/
static void
action_gal_view_cb (GtkRadioAction *action,
                    GtkRadioAction *current,
                    EShellWindow *shell_window)
{
	EShellView *shell_view;
	const gchar *view_name;
	const gchar *view_id;

	view_name = e_shell_window_get_active_view (shell_window);
	shell_view = e_shell_window_get_shell_view (shell_window, view_name);
	view_id = g_object_get_data (G_OBJECT (current), "view-id");
	e_shell_view_set_view_id (shell_view, view_id);
}

/**
 * E_SHELL_WINDOW_ACTION_GAL_SAVE_CUSTOM_VIEW:
 * @window: an #EShellWindow
 *
 * Activation of this action saves a custom GAL view.
 *
 * Main menu item: View -> Current View -> Save Custom View...
 **/
static void
action_gal_save_custom_view_cb (GtkAction *action,
                                EShellWindow *shell_window)
{
	EShellView *shell_view;
	GalViewInstance *view_instance;
	const gchar *view_name;

	view_name = e_shell_window_get_active_view (shell_window);
	shell_view = e_shell_window_get_shell_view (shell_window, view_name);
	view_instance = e_shell_view_get_view_instance (shell_view);

	gal_view_instance_save_as (view_instance);
}

static void
action_gal_customize_view_cb (GtkAction *action,
			      EShellWindow *shell_window)
{
	EShellView *shell_view;
	GalViewInstance *view_instance;
	GalView *gal_view;
	const gchar *view_name;

	view_name = e_shell_window_get_active_view (shell_window);
	shell_view = e_shell_window_get_shell_view (shell_window, view_name);
	view_instance = e_shell_view_get_view_instance (shell_view);

	gal_view = gal_view_instance_get_current_view (view_instance);

	if (GAL_IS_VIEW_ETABLE (gal_view)) {
		GalViewEtable *etable_view = GAL_VIEW_ETABLE (gal_view);
		ETable *etable;

		etable = gal_view_etable_get_table (etable_view);

		if (etable) {
			e_table_customize_view (etable);
		} else {
			ETree *etree;

			etree = gal_view_etable_get_tree (etable_view);

			if (etree)
				e_tree_customize_view (etree);
		}
	}
}

/**
 * E_SHELL_WINDOW_ACTION_CATEGORIES
 * @window: and #EShellWindow
 *
 * Activation of this action opens the Categories Editor dialog.
 *
 * Main menu item: Edit -> Available categories
 **/
static void
action_categories_cb (GtkAction *action,
                      EShellWindow *shell_window)
{
	GtkWidget *content_area;
	GtkWidget *dialog;
	GtkWidget *editor;

	editor = e_categories_editor_new ();
	e_categories_editor_set_entry_visible (
		E_CATEGORIES_EDITOR (editor), FALSE);

	dialog = gtk_dialog_new_with_buttons (
		_("Categories Editor"),
		GTK_WINDOW (shell_window),
		GTK_DIALOG_DESTROY_WITH_PARENT,
		_("_Close"), GTK_RESPONSE_CLOSE, NULL);
	gtk_container_set_border_width (GTK_CONTAINER (dialog), 12);
	content_area = gtk_dialog_get_content_area (GTK_DIALOG (dialog));
	gtk_box_pack_start (
		GTK_BOX (content_area), GTK_WIDGET (editor), TRUE, TRUE, 6);
	gtk_box_set_spacing (GTK_BOX (content_area), 12);

	gtk_dialog_run (GTK_DIALOG (dialog));

	gtk_widget_destroy (dialog);
}

static void
search_options_selection_cancel_cb (GtkMenuShell *menu,
				    EShellWindow *shell_window);

static void
search_options_selection_done_cb (GtkMenuShell *menu,
				  EShellWindow *shell_window)
{
	EShellView *shell_view;
	EShellSearchbar *search_bar;
	const gchar *view_name;

	/* disconnect first */
	g_signal_handlers_disconnect_by_func (menu, search_options_selection_done_cb, shell_window);
	g_signal_handlers_disconnect_by_func (menu, search_options_selection_cancel_cb, shell_window);

	g_return_if_fail (E_IS_SHELL_WINDOW (shell_window));

	view_name = e_shell_window_get_active_view (shell_window);
	shell_view = e_shell_window_get_shell_view (shell_window, view_name);
	g_return_if_fail (shell_view != NULL);

	search_bar = E_SHELL_SEARCHBAR (e_shell_view_get_searchbar (shell_view));
	e_shell_searchbar_search_entry_grab_focus (search_bar);
}

static void
search_options_selection_cancel_cb (GtkMenuShell *menu,
				    EShellWindow *shell_window)
{
	/* only disconnect both functions, thus the selection-done is not called */
	g_signal_handlers_disconnect_by_func (menu, search_options_selection_done_cb, shell_window);
	g_signal_handlers_disconnect_by_func (menu, search_options_selection_cancel_cb, shell_window);
}

/**
 * E_SHELL_WINDOW_ACTION_SEARCH_OPTIONS:
 * @window: an #EShellWindow
 *
 * Activation of this action displays a menu of search options.
 * This appears as a "find" icon in the window's search entry.
 **/
static void
action_search_options_cb (GtkAction *action,
                          EShellWindow *shell_window)
{
	EShellView *shell_view;
	EShellViewClass *shell_view_class;
	const gchar *view_name;
	const gchar *widget_path;
	GtkWidget *popup_menu;

	view_name = e_shell_window_get_active_view (shell_window);
	shell_view = e_shell_window_get_shell_view (shell_window, view_name);
	shell_view_class = E_SHELL_VIEW_GET_CLASS (shell_view);

	widget_path = shell_view_class->search_options;
	popup_menu = e_shell_view_show_popup_menu (shell_view, widget_path, NULL);

	if (popup_menu) {
		g_return_if_fail (GTK_IS_MENU_SHELL (popup_menu));

		g_signal_connect_object (popup_menu, "selection-done",
			G_CALLBACK (search_options_selection_done_cb), shell_window, 0);
		g_signal_connect_object (popup_menu, "cancel",
			G_CALLBACK (search_options_selection_cancel_cb), shell_window, 0);
	}
}

static void
action_switcher_cb (GtkRadioAction *action,
                    GtkRadioAction *current,
                    EShellWindow *shell_window)
{
	const gchar *view_name;

	view_name = g_object_get_data (G_OBJECT (current), "view-name");
	e_shell_window_switch_to_view (shell_window, view_name);
}

static void
action_new_view_window_cb (GtkAction *action,
                           EShellWindow *shell_window)
{
	EShell *shell;
	const gchar *view_name;
	gchar *modified_view_name;

	shell = e_shell_window_get_shell (shell_window);
	view_name = g_object_get_data (G_OBJECT (action), "view-name");

	/* Just a feature to not change default component, when
	   the view name begins with a star */
	modified_view_name = g_strconcat ("*", view_name, NULL);

	e_shell_create_shell_window (shell, modified_view_name);

	g_free (modified_view_name);
}

/**
 * E_SHELL_WINDOW_ACTION_WORK_OFFLINE:
 * @window: an #EShellWindow
 *
 * Activation of this action puts the application into offline mode.
 *
 * Main menu item: File -> Work Offline
 **/
static void
action_work_offline_cb (GtkAction *action,
                        EShellWindow *shell_window)
{
	EShell *shell;
	GSettings *settings;

	shell = e_shell_window_get_shell (shell_window);

	e_shell_set_online (shell, FALSE);

	/* XXX The boolean sense of the setting is backwards.  Would
	 *     be more intuitive and less error-prone as "start-online". */
	settings = e_util_ref_settings ("org.gnome.evolution.shell");
	g_settings_set_boolean (settings, "start-offline", TRUE);
	g_object_unref (settings);
}

/**
 * E_SHELL_WINDOW_ACTION_WORK_ONLINE:
 * @window: an #EShellWindow
 *
 * Activation of this action puts the application into online mode.
 *
 * Main menu item: File -> Work Online
 **/
static void
action_work_online_cb (GtkAction *action,
                       EShellWindow *shell_window)
{
	EShell *shell;
	GSettings *settings;

	shell = e_shell_window_get_shell (shell_window);

	e_shell_set_online (shell, TRUE);

	/* XXX The boolean sense of the setting is backwards.  Would
	 *     be more intuitive and less error-prone as "start-online". */
	settings = e_util_ref_settings ("org.gnome.evolution.shell");
	g_settings_set_boolean (settings, "start-offline", FALSE);
	g_object_unref (settings);
}

static void
action_new_collection_account_cb (GtkAction *action,
				  EShellWindow *shell_window)
{
	EShell *shell;
	GtkWindow *window;

	shell = e_shell_window_get_shell (shell_window);
	window = e_collection_account_wizard_new_window (GTK_WINDOW (shell_window), e_shell_get_registry (shell));

	gtk_window_present (window);
}

/**
 * E_SHELL_WINDOW_ACTION_GROUP_CUSTOM_RULES:
 * @window: an #EShellWindow
 **/

/**
 * E_SHELL_WINDOW_ACTION_GROUP_GAL_VIEW:
 * @window: an #EShellWindow
 **/

/**
 * E_SHELL_WINDOW_ACTION_GROUP_NEW_ITEM:
 * @window: an #EShellWindow
 **/

/**
 * E_SHELL_WINDOW_ACTION_GROUP_NEW_SOURCE:
 * @window: an #EShellWindow
 **/

/**
 * E_SHELL_WINDOW_ACTION_GROUP_SHELL:
 * @window: an #EShellWindow
 **/

/**
 * E_SHELL_WINDOW_ACTION_GROUP_SWITCHER:
 * @window: an #EShellWindow
 **/

static GtkActionEntry new_source_entries[] = {

	{ "new-collection-account",
	  "evolution",
	  N_("Collect_ion Account"),
	  NULL,
	  N_("Create a new collection account"),
	  G_CALLBACK (action_new_collection_account_cb) }
};

static GtkActionEntry shell_entries[] = {

	{ "copy-clipboard",
	  "edit-copy",
	  N_("_Copy"),
	  "<Control>c",
	  N_("Copy the selection"),
	  NULL },  /* Handled by EFocusTracker */

	{ "cut-clipboard",
	  "edit-cut",
	  N_("Cu_t"),
	  "<Control>x",
	  N_("Cut the selection"),
	  NULL },  /* Handled by EFocusTracker */

	{ "delete-selection",
	  "edit-delete",
	  N_("_Delete"),
	  NULL,
	  N_("Delete the selection"),
	  NULL },  /* Handled by EFocusTracker */

	{ "paste-clipboard",
	  "edit-paste",
	  N_("_Paste"),
	  "<Control>v",
	  N_("Paste the clipboard"),
	  NULL },  /* Handled by EFocusTracker */

	{ "categories",
	  NULL,
	  N_("Available Cate_gories"),
	  NULL,
	  N_("Manage available categories"),
	  G_CALLBACK (action_categories_cb) },

	{ "saved-searches",
	  NULL,
	  N_("_Saved Searches"),
	  NULL,
	  NULL,
	  NULL },

	{ "search-options",
	  "edit-find",
	  N_("_Find"),
	  "<Control>f",
	  N_("Click here to change the search type"),
	  G_CALLBACK (action_search_options_cb) },

	{ "select-all",
	  "edit-select-all",
	  N_("Select _All"),
	  "<Control>a",
	  N_("Select all text"),
	  NULL },  /* Handled by EFocusTracker */

	{ "work-offline",
	  "stock_disconnect",
	  N_("_Work Offline"),
	  NULL,
	  N_("Put Evolution into offline mode"),
	  G_CALLBACK (action_work_offline_cb) },

	{ "work-online",
	  "stock_connect",
	  N_("_Work Online"),
	  NULL,
	  N_("Put Evolution into online mode"),
	  G_CALLBACK (action_work_online_cb) },

	/*** Menus ***/

	{ "edit-menu",
	  NULL,
	  N_("_Edit"),
	  NULL,
	  NULL,
	  NULL },

	{ "file-menu",
	  NULL,
	  N_("_File"),
	  NULL,
	  NULL,
	  NULL },

	{ "new-menu",
	  "document-new",
	  /* Translators: This is a New menu item caption, under File->New */
	  N_("_New"),
	  NULL,
	  NULL,
	  NULL },

	{ "search-menu",
	  NULL,
	  N_("_Search"),
	  NULL,
	  NULL,
	  NULL },

	{ "view-menu",
	  NULL,
	  N_("_View"),
	  NULL,
	  NULL,
	  NULL },

	{ "window-menu",
	  NULL,
	  N_("_Window"),
	  NULL,
	  NULL,
	  NULL }
};

static EPopupActionEntry shell_popup_entries[] = {

	{ "popup-copy-clipboard",
	  NULL,
	  "copy-clipboard" },

	{ "popup-cut-clipboard",
	  NULL,
	  "cut-clipboard" },

	{ "popup-delete-selection",
	  NULL,
	  "delete-selection" },

	{ "popup-paste-clipboard",
	  NULL,
	  "paste-clipboard" }
};

static GtkRadioActionEntry shell_switcher_entries[] = {

	/* This action represents the initial active shell view.
	 * It should not be visible in the UI, nor should it be
	 * possible to switch to it from another shell view. */
	{ "switcher-initial",
	  NULL,
	  NULL,
	  NULL,
	  NULL,
	  -1 }
};

static GtkActionEntry shell_gal_view_entries[] = {

	{ "gal-delete-view",
	  NULL,
	  N_("Delete Current View"),
	  NULL,
	  NULL,  /* Set in update_view_menu */
	  G_CALLBACK (action_gal_delete_view_cb) },

	{ "gal-save-custom-view",
	  NULL,
	  N_("Save Custom View…"),
	  NULL,
	  N_("Save current custom view"),
	  G_CALLBACK (action_gal_save_custom_view_cb) },

	{ "gal-customize-view",
	  NULL,
	  N_("Custo_mize Current View…"),
	  NULL,
	  NULL,
	  G_CALLBACK (action_gal_customize_view_cb) },

	/*** Menus ***/

	{ "gal-view-menu",
	  NULL,
	  N_("C_urrent View"),
	  NULL,
	  NULL,
	  NULL }
};

static GtkRadioActionEntry shell_gal_view_radio_entries[] = {

	{ "gal-custom-view",
	  NULL,
	  N_("Custom View"),
	  NULL,
	  N_("Current view is a customized view"),
	  -1 }
};

static void
shell_window_extract_actions (EShellWindow *shell_window,
                              GList **source_list,
                              GList **destination_list)
{
	const gchar *current_view;
	GList *match_list = NULL;
	GList *iter;

	/* Pick out the actions from the source list that are tagged
	 * as belonging to the current EShellView and move them to the
	 * destination list. */

	current_view = e_shell_window_get_active_view (shell_window);

	/* Example: Suppose [A] and [C] are tagged for this EShellView.
	 *
	 *        source_list = [A] -> [B] -> [C]
	 *                       ^             ^
	 *                       |             |
	 *         match_list = [ ] --------> [ ]
	 *
	 *
	 *   destination_list = [1] -> [2]  (other actions)
	 */
	for (iter = *source_list; iter != NULL; iter = iter->next) {
		GtkAction *action = iter->data;
		const gchar *backend_name;

		backend_name = g_object_get_data (
			G_OBJECT (action), "backend-name");

		if (g_strcmp0 (backend_name, current_view) != 0)
			continue;

		if (g_object_get_data (G_OBJECT (action), "primary"))
			match_list = g_list_prepend (match_list, iter);
		else
			match_list = g_list_append (match_list, iter);
	}

	/* source_list = [B]   match_list = [A] -> [C] */
	for (iter = match_list; iter != NULL; iter = iter->next) {
		GList *link = iter->data;

		iter->data = link->data;
		*source_list = g_list_delete_link (*source_list, link);
	}

	/* destination_list = [1] -> [2] -> [A] -> [C] */
	*destination_list = g_list_concat (*destination_list, match_list);
}

void
e_shell_window_actions_init (EShellWindow *shell_window)
{
	EShell *shell;
	GActionMap *action_map;
	GAction *action;
	GtkActionGroup *action_group;
	EFocusTracker *focus_tracker;
	GtkUIManager *ui_manager;
	gchar *path;

	g_return_if_fail (E_IS_SHELL_WINDOW (shell_window));

	shell = e_shell_window_get_shell (shell_window);
	action_map = G_ACTION_MAP (shell_window);

	g_action_map_add_action_entries (
		action_map,
		win_entries, G_N_ELEMENTS (win_entries),
		shell_window);

	e_shell_set_accelerator (shell, "win.clear-search", "<Shift><Ctrl>q");
	e_shell_set_accelerator (shell, "win.close", "<Ctrl>w");

	e_action_map_add_property_action_entries (
		action_map,
		win_property_entries, G_N_ELEMENTS (win_property_entries),
		shell_window);

	e_shell_set_accelerator (shell, "win.show-menubar", "F8");
	e_shell_set_accelerator (shell, "win.show-sidebar", "F9");

	ui_manager = e_shell_window_get_ui_manager (shell_window);

	e_load_ui_manager_definition (ui_manager, "evolution-shell.ui");

	e_shell_window_register_new_source_actions (shell_window, "shell",
		new_source_entries, G_N_ELEMENTS (new_source_entries));

	/* Shell Actions */
	action_group = ACTION_GROUP (SHELL);
	gtk_action_group_add_actions (
		action_group, shell_entries,
		G_N_ELEMENTS (shell_entries), shell_window);
	e_action_group_add_popup_actions (
		action_group, shell_popup_entries,
		G_N_ELEMENTS (shell_popup_entries));
	gtk_action_group_add_actions (
		action_group, shell_gal_view_entries,
		G_N_ELEMENTS (shell_gal_view_entries), shell_window);
	gtk_action_group_add_radio_actions (
		action_group, shell_gal_view_radio_entries,
		G_N_ELEMENTS (shell_gal_view_radio_entries),
		0, G_CALLBACK (action_gal_view_cb), shell_window);

	/* Switcher Actions */
	action_group = ACTION_GROUP (SWITCHER);
	gtk_action_group_add_radio_actions (
		action_group, shell_switcher_entries,
		G_N_ELEMENTS (shell_switcher_entries),
		-1, G_CALLBACK (action_switcher_cb), shell_window);

	/* Configure an EFocusTracker to manage selection actions. */

	focus_tracker = e_focus_tracker_new (GTK_WINDOW (shell_window));
	e_focus_tracker_set_cut_clipboard_action (
		focus_tracker, ACTION (CUT_CLIPBOARD));
	e_focus_tracker_set_copy_clipboard_action (
		focus_tracker, ACTION (COPY_CLIPBOARD));
	e_focus_tracker_set_paste_clipboard_action (
		focus_tracker, ACTION (PASTE_CLIPBOARD));
	e_focus_tracker_set_delete_selection_action (
		focus_tracker, ACTION (DELETE_SELECTION));
	e_focus_tracker_set_select_all_action (
		focus_tracker, ACTION (SELECT_ALL));
	shell_window->priv->focus_tracker = focus_tracker;

	/* Fine tuning. */

	action = E_SHELL_WINDOW_ACTION (shell_window, "search-quick");
	g_simple_action_set_enabled (G_SIMPLE_ACTION (action), FALSE);
}

GtkWidget *
e_shell_window_create_new_menu (EShellWindow *shell_window)
{
	GtkActionGroup *action_group;
	GList *new_item_actions;
	GList *new_source_actions;
	GList *iter, *list = NULL;
	GtkWidget *menu;
	GtkWidget *separator;

	/* Get sorted lists of "new item" and "new source" actions. */

	action_group = ACTION_GROUP (NEW_ITEM);

	new_item_actions = g_list_sort (
		gtk_action_group_list_actions (action_group),
		(GCompareFunc) e_action_compare_by_label);

	action_group = ACTION_GROUP (NEW_SOURCE);

	new_source_actions = g_list_sort (
		gtk_action_group_list_actions (action_group),
		(GCompareFunc) e_action_compare_by_label);

	/* Give priority to actions that belong to this shell view. */

	shell_window_extract_actions (
		shell_window, &new_item_actions, &list);

	shell_window_extract_actions (
		shell_window, &new_source_actions, &list);

	/* Convert the actions to menu item proxy widgets. */

	for (iter = list; iter != NULL; iter = iter->next)
		iter->data = gtk_action_create_menu_item (iter->data);

	for (iter = new_item_actions; iter != NULL; iter = iter->next)
		iter->data = gtk_action_create_menu_item (iter->data);

	for (iter = new_source_actions; iter != NULL; iter = iter->next)
		iter->data = gtk_action_create_menu_item (iter->data);

	/* Add menu separators. */

	if (new_item_actions != NULL) {
		separator = gtk_separator_menu_item_new ();
		new_item_actions = g_list_prepend (new_item_actions, separator);
		gtk_widget_show (GTK_WIDGET (separator));
	}

	if (new_source_actions != NULL) {
		separator = gtk_separator_menu_item_new ();
		new_source_actions = g_list_prepend (new_source_actions, separator);
		gtk_widget_show (GTK_WIDGET (separator));
	}

	/* Merge everything into one list, reflecting the menu layout. */

	list = g_list_concat (list, new_item_actions);
	new_item_actions = NULL;    /* just for clarity */

	list = g_list_concat (list, new_source_actions);
	new_source_actions = NULL;  /* just for clarity */

	/* And finally, build the menu. */

	menu = gtk_menu_new ();

	for (iter = list; iter != NULL; iter = iter->next)
		gtk_menu_shell_append (GTK_MENU_SHELL (menu), iter->data);

	g_list_free (list);

	return menu;
}

static GtkAction *
e_shell_window_create_switcher_action (GType type,
                                       EShellViewClass *class,
                                       const gchar *name,
                                       const gchar *tooltip,
                                       const gchar *view_name)
{
	GtkAction *action;

	action = g_object_new (
		type,
		"name", name,
		"label", class->label,
		"tooltip", tooltip,
		"icon-name", class->icon_name,
		NULL);

	g_object_set_data (
		G_OBJECT (action),
		"view-name", (gpointer) view_name);

	return action;
}

/*
 * Create both the actions to switch the current window, and also
 * to create each view in a new window.
 */
void
e_shell_window_create_switcher_actions (EShellWindow *shell_window)
{
	GSList *group = NULL;
	GtkRadioAction *s_action;
	GtkActionGroup *s_action_group;
	GtkActionGroup *n_action_group;
	GtkUIManager *ui_manager;
	EShellSwitcher *switcher;
	EShell *shell;
	GList *list, *iter;
	guint merge_id;
	guint ii = 0;

	g_return_if_fail (E_IS_SHELL_WINDOW (shell_window));

	s_action_group = ACTION_GROUP (SWITCHER);
	n_action_group = ACTION_GROUP (NEW_WINDOW);
	switcher = E_SHELL_SWITCHER (shell_window->priv->switcher);
	ui_manager = e_shell_window_get_ui_manager (shell_window);
	merge_id = gtk_ui_manager_new_merge_id (ui_manager);
	shell = e_shell_window_get_shell (shell_window);
	list = e_shell_get_shell_backends (shell);

	/* Construct a group of radio actions from the various EShellView
	 * subclasses and register them with the EShellSwitcher.  These
	 * actions are manifested as switcher buttons and View->Window
	 * menu items. */

	s_action = GTK_RADIO_ACTION (ACTION (SWITCHER_INITIAL));
	gtk_radio_action_set_group (s_action, group);
	group = gtk_radio_action_get_group (s_action);

	for (iter = list; iter != NULL; iter = iter->next) {
		EShellBackend *shell_backend = iter->data;
		EShellBackendClass *backend_class;
		EShellViewClass *class;
		GtkAction *n_action;
		GType view_type;
		const gchar *view_name;
		gchar *accelerator;
		gchar *s_action_name;
		gchar *n_action_name;
		gchar *tooltip;

		/* The backend name is also the view name. */
		backend_class = E_SHELL_BACKEND_GET_CLASS (shell_backend);
		view_type = backend_class->shell_view_type;
		view_name = backend_class->name;

		if (!g_type_is_a (view_type, E_TYPE_SHELL_VIEW)) {
			g_critical (
				"%s is not a subclass of %s",
				g_type_name (view_type),
				g_type_name (E_TYPE_SHELL_VIEW));
			continue;
		}

		class = g_type_class_ref (view_type);

		if (class->label == NULL) {
			g_critical (
				"Label member not set on %s",
				G_OBJECT_CLASS_NAME (class));
			continue;
		}

		tooltip = g_strdup_printf (_("Switch to %s"), class->label);

		s_action_name = g_strdup_printf (
			E_SHELL_SWITCHER_FORMAT, view_name);

		/* Note, we have to set "icon-name" separately because
		 * gtk_radio_action_new() expects a "stock-id".  Sadly,
		 * GTK+ still distinguishes between the two. */

		s_action = GTK_RADIO_ACTION (
			e_shell_window_create_switcher_action (
				GTK_TYPE_RADIO_ACTION,
				class, s_action_name,
				tooltip, view_name));
		g_object_set (s_action, "value", ii++, NULL);
		gtk_radio_action_set_group (s_action, group);
		group = gtk_radio_action_get_group (s_action);

		/* The first nine views have accelerators Ctrl+(1-9). */
		if (ii < 10)
			accelerator = g_strdup_printf ("<Control>%d", ii);
		else
			accelerator = g_strdup ("");

		gtk_action_group_add_action_with_accel (
			s_action_group, GTK_ACTION (s_action), accelerator);

		g_free (accelerator);

		gtk_ui_manager_add_ui (
			ui_manager, merge_id,
			"/main-menu/view-menu/window-menu",
			s_action_name, s_action_name,
			GTK_UI_MANAGER_AUTO, FALSE);
		g_free (s_action_name);

		/* Create in new window actions */
		n_action_name = g_strdup_printf (
			E_SHELL_NEW_WINDOW_FORMAT, view_name);
		n_action = e_shell_window_create_switcher_action (
			GTK_TYPE_ACTION,
			class, n_action_name,
			tooltip, view_name);
		g_signal_connect (
			n_action, "activate",
			G_CALLBACK (action_new_view_window_cb), shell_window);
		gtk_action_group_add_action (n_action_group, n_action);

		e_shell_switcher_add_action (
			switcher, GTK_ACTION (s_action), n_action);

		g_free (n_action_name);
		g_free (tooltip);

		g_type_class_unref (class);
	}
}

void
e_shell_window_update_view_menu (EShellWindow *shell_window)
{
	EShellView *shell_view;
	EShellViewClass *shell_view_class;
	GtkUIManager *ui_manager;
	GtkActionGroup *action_group;
	GalViewCollection *view_collection;
	GalViewInstance *view_instance;
	GtkRadioAction *radio_action;
	GtkAction *action;
	GSList *radio_group;
	gboolean visible;
	const gchar *path;
	const gchar *view_id;
	const gchar *view_name;
	gchar *delete_tooltip = NULL;
	gboolean delete_visible = FALSE;
	guint merge_id;
	gint count, ii;

	ui_manager = e_shell_window_get_ui_manager (shell_window);
	view_name = e_shell_window_get_active_view (shell_window);
	shell_view = e_shell_window_get_shell_view (shell_window, view_name);
	g_return_if_fail (shell_view != NULL);

	shell_view_class = E_SHELL_VIEW_GET_CLASS (shell_view);
	view_collection = shell_view_class->view_collection;
	view_id = e_shell_view_get_view_id (shell_view);
	g_return_if_fail (view_collection != NULL);

	action_group = ACTION_GROUP (GAL_VIEW);
	merge_id = shell_window->priv->gal_view_merge_id;

	/* Unmerge the previous menu. */
	gtk_ui_manager_remove_ui (ui_manager, merge_id);
	e_action_group_remove_all_actions (action_group);
	gtk_ui_manager_ensure_update (ui_manager);

	/* We have a view ID, so forge ahead. */
	count = gal_view_collection_get_count (view_collection);
	path = "/main-menu/view-menu/gal-view-menu/gal-view-list";

	/* Prevent spurious activations. */
	action = ACTION (GAL_CUSTOM_VIEW);
	g_signal_handlers_block_matched (
		action, G_SIGNAL_MATCH_FUNC, 0, 0,
		NULL, action_gal_view_cb, NULL);

	/* Default to "Custom View", unless we find our view ID. */
	radio_action = GTK_RADIO_ACTION (ACTION (GAL_CUSTOM_VIEW));
	gtk_radio_action_set_group (radio_action, NULL);
	radio_group = gtk_radio_action_get_group (radio_action);
	gtk_radio_action_set_current_value (radio_action, -1);

	/* Add a menu item for each view collection item. */
	for (ii = 0; ii < count; ii++) {
		GalViewCollectionItem *item;
		gchar *action_name;
		gchar *tooltip, *title;

		item = gal_view_collection_get_view_item (view_collection, ii);

		action_name = g_strdup_printf (
			"gal-view-%s-%d", view_name, ii);
		title = e_str_without_underscores (item->title);
		tooltip = g_strdup_printf (_("Select view: %s"), title);

		radio_action = gtk_radio_action_new (
			action_name, item->title, tooltip, NULL, ii);

		action = GTK_ACTION (radio_action);
		gtk_radio_action_set_group (radio_action, radio_group);
		radio_group = gtk_radio_action_get_group (radio_action);

		g_object_set_data_full (
			G_OBJECT (radio_action), "view-id",
			g_strdup (item->id), (GDestroyNotify) g_free);

		if (view_id != NULL && strcmp (item->id, view_id) == 0) {
			gtk_radio_action_set_current_value (radio_action, ii);
			delete_visible = (!item->built_in);
			delete_tooltip = g_strdup_printf (
				_("Delete view: %s"), title);
		}

		if (item->built_in && item->accelerator)
			gtk_action_group_add_action_with_accel (action_group, action, item->accelerator);
		else
			gtk_action_group_add_action (action_group, action);

		gtk_ui_manager_add_ui (
			ui_manager, merge_id,
			path, action_name, action_name,
			GTK_UI_MANAGER_AUTO, FALSE);

		g_free (action_name);
		g_free (tooltip);
		g_free (title);
	}

	view_instance = e_shell_view_get_view_instance (shell_view);
	visible = view_instance && gal_view_instance_get_current_view (view_instance) &&
		GAL_IS_VIEW_ETABLE (gal_view_instance_get_current_view (view_instance));
	action = ACTION (GAL_CUSTOMIZE_VIEW);
	gtk_action_set_visible (action, visible);

	/* Doesn't matter which radio action we check. */
	visible = (gtk_radio_action_get_current_value (radio_action) < 0);

	action = ACTION (GAL_CUSTOM_VIEW);
	gtk_action_set_visible (action, visible);
	g_signal_handlers_unblock_matched (
		action, G_SIGNAL_MATCH_FUNC, 0, 0,
		NULL, action_gal_view_cb, NULL);

	action = ACTION (GAL_SAVE_CUSTOM_VIEW);
	gtk_action_set_visible (action, visible);

	action = ACTION (GAL_DELETE_VIEW);
	gtk_action_set_tooltip (action, delete_tooltip);
	gtk_action_set_visible (action, delete_visible);

	g_free (delete_tooltip);
}

void
e_shell_window_update_search_menu (EShellWindow *shell_window)
{
	EShellView *shell_view;
	EShellViewClass *shell_view_class;
	ERuleContext *context;
	EFilterRule *rule;
	GtkUIManager *ui_manager;
	GtkActionGroup *action_group;
	const gchar *source;
	const gchar *view_name;
	gchar *search_options_path;
	gboolean sensitive;
	guint merge_id;
	gint ii = 0;

	g_return_if_fail (E_IS_SHELL_WINDOW (shell_window));

	ui_manager = e_shell_window_get_ui_manager (shell_window);
	view_name = e_shell_window_get_active_view (shell_window);
	shell_view = e_shell_window_get_shell_view (shell_window, view_name);

	/* Check for a NULL shell view before proceeding.  This can
	 * happen if the initial view name from GSettings is unrecognized.
	 * Without this we would crash at E_SHELL_VIEW_GET_CLASS(). */
	g_return_if_fail (shell_view != NULL);

	shell_view_class = E_SHELL_VIEW_GET_CLASS (shell_view);
	context = shell_view_class->search_context;
	search_options_path = g_strconcat (shell_view_class->search_options, "/saved-searches/custom-rules", NULL);

	source = E_FILTER_SOURCE_INCOMING;

	/* Update sensitivity of search_options action. */
	sensitive = (shell_view_class->search_options != NULL);
	gtk_action_set_sensitive (ACTION (SEARCH_OPTIONS), sensitive);

	/* Add custom rules to the Search menu. */

	action_group = ACTION_GROUP (CUSTOM_RULES);
	merge_id = shell_window->priv->custom_rule_merge_id;

	/* Unmerge the previous menu. */
	gtk_ui_manager_remove_ui (ui_manager, merge_id);
	e_action_group_remove_all_actions (action_group);
	gtk_ui_manager_ensure_update (ui_manager);

	if (!gtk_ui_manager_get_widget (ui_manager, search_options_path))
		g_clear_pointer (&search_options_path, g_free);

	rule = e_rule_context_next_rule (context, NULL, source);
	while (rule != NULL) {
		GtkAction *action;
		GString *escaped_name = NULL;
		gchar *action_name;
		gchar *action_label;

		if (rule->name && strchr (rule->name, '_') != NULL)
			escaped_name = e_str_replace_string (rule->name, "_", "__");

		action_name = g_strdup_printf ("custom-rule-%d", ii++);
		if (ii < 10)
			action_label = g_strdup_printf (
				"_%d. %s", ii, escaped_name ? escaped_name->str : rule->name);
		else
			action_label = g_strdup (escaped_name ? escaped_name->str : rule->name);

		if (escaped_name)
			g_string_free (escaped_name, TRUE);

		action = gtk_action_new (
			action_name, action_label,
			_("Execute these search parameters"), NULL);

		g_object_set_data_full (
			G_OBJECT (action),
			"rule", g_object_ref (rule),
			(GDestroyNotify) g_object_unref);

		g_signal_connect (
			action, "activate",
			G_CALLBACK (action_custom_rule_cb), shell_window);

		gtk_action_group_add_action (action_group, action);

		gtk_ui_manager_add_ui (
			ui_manager, merge_id,
			"/main-menu/search-menu/custom-rules",
			action_name, action_name,
			GTK_UI_MANAGER_AUTO, FALSE);

		if (search_options_path) {
			gtk_ui_manager_add_ui (
				ui_manager, merge_id,
				search_options_path,
				action_name, action_name,
				GTK_UI_MANAGER_AUTO, FALSE);
		}

		g_free (action_name);
		g_free (action_label);

		rule = e_rule_context_next_rule (context, rule, source);
	}

	g_clear_pointer (&search_options_path, g_free);
}
