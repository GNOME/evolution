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

static void e_shell_window_create_views_actions (EShellWindow *shell_window);

/**
 * E_SHELL_WINDOW_ACTION_ABOUT:
 * @window: an #EShellWindow
 *
 * Activation of this action displays the application's About dialog.
 *
 * Main menu item: Help -> About
 **/
static void
action_about_cb (EUIAction *action,
		 GVariant *parameter,
		 gpointer user_data)
{
	EShellWindow *shell_window = user_data;
	e_shell_utils_run_help_about (e_shell_window_get_shell (shell_window));
}

static void
action_accounts_cb (EUIAction *action,
		    GVariant *parameter,
		    gpointer user_data)
{
	EShellWindow *shell_window = user_data;
	static GtkWidget *accounts_window = NULL;

	g_return_if_fail (E_IS_SHELL_WINDOW (shell_window));

	if (!accounts_window) {
		ESourceRegistry *registry;
		EShell *shell;

		shell = e_shell_window_get_shell (shell_window);
		registry = e_shell_get_registry (shell);

		accounts_window = e_accounts_window_new (registry);

		g_object_weak_ref (G_OBJECT (accounts_window), (GWeakNotify) g_nullify_pointer, &accounts_window);
	}

	e_accounts_window_show_with_parent (E_ACCOUNTS_WINDOW (accounts_window), GTK_WINDOW (shell_window));
}

/**
 * E_SHELL_WINDOW_ACTION_CLOSE:
 * @window: an #EShellWindow
 *
 * Activation of this action closes @window.  If this is the last window,
 * the application initiates shutdown.
 *
 * Main menu item: File -> Close
 **/
static void
action_close_cb (EUIAction *action,
		 GVariant *parameter,
		 gpointer user_data)
{
	EShellWindow *shell_window = user_data;
	GtkWidget *widget;
	GdkWindow *window;
	GdkEvent *event;

	widget = GTK_WIDGET (shell_window);
	window = gtk_widget_get_window (widget);

	/* Synthesize a delete_event on this window. */
	event = gdk_event_new (GDK_DELETE);
	event->any.window = g_object_ref (window);
	event->any.send_event = TRUE;
	gtk_main_do_event (event);
	gdk_event_free (event);
}

/**
 * E_SHELL_WINDOW_ACTION_CONTENTS:
 * @window: an #EShellWindow
 *
 * Activation of this action opens the application's user manual.
 *
 * Main menu item: Help -> Contents
 **/
static void
action_contents_cb (EUIAction *action,
		    GVariant *parameter,
		    gpointer user_data)
{
	EShellWindow *shell_window = user_data;
	e_shell_utils_run_help_contents (e_shell_window_get_shell (shell_window));
}

static void
action_shortcuts_cb (EUIAction *action,
		     GVariant *parameter,
		     gpointer user_data)
{
	EShellWindow *shell_window = user_data;
	GtkBuilder *builder;
	GtkWidget *widget;

	builder = gtk_builder_new ();
	e_load_ui_builder_definition (builder, "evolution-shortcuts.ui");

	widget = e_builder_get_widget (builder, "evolution-shortcuts");
	gtk_window_set_transient_for (GTK_WINDOW (widget), GTK_WINDOW (shell_window));

	gtk_widget_show (widget);

	g_object_unref (builder);
}

/**
 * E_SHELL_WINDOW_ACTION_IMPORT:
 * @window: an #EShellWindow
 *
 * Activation of this action opens the Evolution Import Assistant.
 *
 * Main menu item: File -> Import...
 **/
static void
action_import_cb (EUIAction *action,
		  GVariant *parameter,
		  gpointer user_data)
{
	EShellWindow *shell_window = user_data;
	GtkWidget *assistant;

	assistant = e_import_assistant_new (GTK_WINDOW (shell_window));

	/* These are "Run Last" signals, so use g_signal_connect_after()
	 * to give the default handlers a chance to run before we destroy
	 * the window. */

	g_signal_connect_after (
		assistant, "cancel",
		G_CALLBACK (gtk_widget_destroy), NULL);

	g_signal_connect_after (
		assistant, "finished",
		G_CALLBACK (gtk_widget_destroy), NULL);

	gtk_widget_show (assistant);
}

/**
 * E_SHELL_WINDOW_ACTION_NEW_WINDOW:
 * @window: an #EShellWindow
 *
 * Activation of this action opens a new shell window.
 *
 * Main menu item: File -> New Window
 **/
static void
action_new_window_cb (EUIAction *action,
		      GVariant *parameter,
		      gpointer user_data)
{
	EShellWindow *shell_window = user_data;
	EShell *shell;
	EShellView *shell_view;
	const gchar *view_name;

	shell = e_shell_window_get_shell (shell_window);
	view_name = e_shell_window_get_active_view (shell_window);

	shell_view = e_shell_window_get_shell_view (shell_window, view_name);

	if (shell_view)
		e_shell_view_save_state_immediately (shell_view);

	e_shell_create_shell_window (shell, view_name);
}

/**
 * E_SHELL_WINDOW_ACTION_PAGE_SETUP:
 * @window: an #EShellWindow
 *
 * Activation of this action opens the application's Page Setup dialog.
 *
 * Main menu item: File -> Page Setup...
 **/
static void
action_page_setup_cb (EUIAction *action,
		      GVariant *parameter,
		      gpointer user_data)
{
	EShellWindow *shell_window = user_data;
	e_print_run_page_setup_dialog (GTK_WINDOW (shell_window));
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
action_categories_cb (EUIAction *action,
		      GVariant *parameter,
		      gpointer user_data)
{
	EShellWindow *shell_window = user_data;
	GtkWidget *content_area;
	GtkWidget *dialog;
	GtkWidget *editor;

	editor = e_categories_editor_new ();
	e_categories_editor_set_entry_visible (
		E_CATEGORIES_EDITOR (editor), FALSE);

	dialog = g_object_new (
		GTK_TYPE_DIALOG,
		"transient-for", GTK_WINDOW (shell_window),
		"use-header-bar", e_util_get_use_header_bar (),
		"title", _("Categories Editor"),
		NULL);

	gtk_window_set_destroy_with_parent (GTK_WINDOW (dialog), TRUE);

	gtk_container_set_border_width (GTK_CONTAINER (dialog), 12);
	content_area = gtk_dialog_get_content_area (GTK_DIALOG (dialog));
	gtk_box_pack_start (
		GTK_BOX (content_area), GTK_WIDGET (editor), TRUE, TRUE, 6);
	gtk_box_set_spacing (GTK_BOX (content_area), 12);

	gtk_dialog_run (GTK_DIALOG (dialog));

	gtk_widget_destroy (dialog);
}

/**
 * E_SHELL_WINDOW_ACTION_PREFERENCES:
 * @window: an #EShellWindow
 *
 * Activation of this action opens the application's Preferences window.
 *
 * Main menu item: Edit -> Preferences
 **/
static void
action_preferences_cb (EUIAction *action,
		       GVariant *parameter,
		       gpointer user_data)
{
	EShellWindow *shell_window = user_data;
	e_shell_utils_run_preferences (e_shell_window_get_shell (shell_window));
}

/**
 * E_SHELL_WINDOW_ACTION_QUIT:
 * @window: an #EShellWindow
 *
 * Activation of this action initiates application shutdown.
 *
 * Main menu item: File -> Quit
 **/
static void
action_quit_cb (EUIAction *action,
		GVariant *parameter,
		gpointer user_data)
{
	EShellWindow *shell_window = user_data;
	EShell *shell;

	shell = e_shell_window_get_shell (shell_window);
	e_shell_quit (shell, E_SHELL_QUIT_ACTION);
}

/**
 * E_SHELL_WINDOW_ACTION_SHOW_MENUBAR:
 * @window: an #EShellWindow
 *
 * This toggle action controls whether the menu bar is visible.
 *
 * Main menu item: View -> Layout -> Show Menu Bar
 *
 * Since: 3.24
 **/

/**
 * E_SHELL_WINDOW_ACTION_SHOW_SIDEBAR:
 * @window: an #EShellWindow
 *
 * This toggle action controls whether the sidebar is visible.
 *
 * Main menu item: View -> Layout -> Show Sidebar
 **/

/**
 * E_SHELL_WINDOW_ACTION_SHOW_SWITCHER:
 * @window: an #EShellWindow
 *
 * This toggle action controls whether the switcher buttons are visible.
 *
 * Main menu item: View -> Switcher Appearance -> Show Buttons
 **/

/**
 * E_SHELL_WINDOW_ACTION_SHOW_TASKBAR:
 * @window: an #EShellWindow
 *
 * This toggle action controls whether the task bar is visible.
 *
 * Main menu item: View -> Layout -> Show Status Bar
 **/

/**
 * E_SHELL_WINDOW_ACTION_SHOW_TOOLBAR:
 * @window: an #EShellWindow
 *
 * This toggle action controls whether the tool bar is visible.
 *
 * Main menu item: View -> Layout -> Show Tool Bar
 **/

/**
 * E_SHELL_WINDOW_ACTION_SUBMIT_BUG:
 * @window: an #EShellWindow
 *
 * Activation of this action allows users to report a bug using
 * Bug Buddy.
 *
 * Main menu item: Help -> Submit Bug Report
 **/
static void
action_submit_bug_cb (EUIAction *action,
		      GVariant *parameter,
		      gpointer user_data)
{
	EShellWindow *shell_window = user_data;
	const gchar *command_line;
	GError *error = NULL;

	command_line = "bug-buddy --package=Evolution";

	g_debug ("Spawning: %s", command_line);
	g_spawn_command_line_async (command_line, &error);

	if (error != NULL) {
		e_notice (
			shell_window, GTK_MESSAGE_ERROR,
			error->code == G_SPAWN_ERROR_NOENT ?
			_("Bug Buddy is not installed.") :
			_("Bug Buddy could not be run."));
		g_error_free (error);
	}
}

static WebKitWebView *
shell_window_actions_find_webview (GtkContainer *container)
{
	GList *children, *link;
	WebKitWebView *webview = NULL;

	if (!container)
		return NULL;

	children = gtk_container_get_children (container);

	for (link = children; link && !webview; link = g_list_next (link)) {
		GtkWidget *child = link->data;

		if (WEBKIT_IS_WEB_VIEW (child))
			webview = WEBKIT_WEB_VIEW (child);
		else if (GTK_IS_CONTAINER (child))
			webview = shell_window_actions_find_webview (GTK_CONTAINER (child));
	}

	g_list_free (children);

	return webview;
}

static void
action_show_webkit_gpu_cb (EUIAction *action,
			   GVariant *parameter,
			   gpointer user_data)
{
	EShellWindow *shell_window = user_data;
	WebKitWebView *webview;
	EShellView *shell_view;
	EShellContent *shell_content;

	shell_view = e_shell_window_get_shell_view (shell_window, e_shell_window_get_active_view (shell_window));
	shell_content = e_shell_view_get_shell_content (shell_view);

	webview = shell_window_actions_find_webview (GTK_CONTAINER (shell_content));

	if (webview)
		webkit_web_view_load_uri (webview, "webkit://gpu");
	else
		g_message ("%s: No WebKitWebView found", G_STRFUNC);
}

static void
action_switcher_cb (EUIAction *action,
                    GParamSpec *param,
                    EShellWindow *shell_window)
{
	GVariant *target;
	const gchar *view_name;

	if (!e_ui_action_get_active (action))
		return;

	target = e_ui_action_ref_target (action);
	view_name = g_variant_get_string (target, NULL);

	e_shell_window_switch_to_view (shell_window, view_name);

	g_clear_pointer (&target, g_variant_unref);
}

static void
action_new_view_window_cb (EUIAction *action,
			   GVariant *parameter,
			   gpointer user_data)
{
	EShellWindow *shell_window = user_data;
	EShell *shell;
	GVariant *target;
	const gchar *view_name;
	gchar *modified_view_name;

	shell = e_shell_window_get_shell (shell_window);
	target = e_ui_action_ref_target (action);
	view_name = g_variant_get_string (target, NULL);

	/* Just a feature to not change default component, when
	   the view name begins with a star */
	modified_view_name = g_strconcat ("*", view_name, NULL);

	e_shell_create_shell_window (shell, modified_view_name);

	g_clear_pointer (&target, g_variant_unref);
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
action_work_offline_cb (EUIAction *action,
			GVariant *parameter,
			gpointer user_data)
{
	EShellWindow *shell_window = user_data;
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
action_work_online_cb (EUIAction *action,
		       GVariant *parameter,
		       gpointer user_data)
{
	EShellWindow *shell_window = user_data;
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
action_shell_window_new_shortcut_cb (EUIAction *action,
				     GVariant *parameter,
				     gpointer user_data)
{
	EShellWindow *shell_window = user_data;
	EShellView *shell_view;

	shell_view = e_shell_window_get_shell_view (shell_window, e_shell_window_get_active_view (shell_window));
	if (shell_view) {
		EUIAction *new_action;

		new_action = e_shell_view_get_action (shell_view, "EShellView::new-menu");
		if (new_action)
			g_action_activate (G_ACTION (new_action), NULL);
		else
			g_warning ("%s: Cannot find action '%s' in %s", G_STRFUNC, "EShellView::new-menu", G_OBJECT_TYPE_NAME (shell_view));
	}
}

static void
action_new_collection_account_cb (EUIAction *action,
				  GVariant *parameter,
				  gpointer user_data)
{
	EShellWindow *shell_window = user_data;
	EShell *shell;
	GtkWindow *window;

	shell = e_shell_window_get_shell (shell_window);
	window = e_collection_account_wizard_new_window (GTK_WINDOW (shell_window), e_shell_get_registry (shell));

	gtk_window_present (window);
}

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

void
e_shell_window_actions_constructed (EShellWindow *shell_window)
{
	static const EUIActionEntry new_source_entries[] = {

		{ "new-collection-account",
		  "evolution",
		  N_("Collect_ion Account"),
		  NULL,
		  N_("Create a new collection account"),
		  action_new_collection_account_cb, NULL, NULL, NULL }
	};

	static const EUIActionEntry shell_entries[] = {

		{ "about",
		  "help-about",
		  N_("_About"),
		  NULL,
		  N_("Show information about Evolution"),
		  action_about_cb, NULL, NULL, NULL },

		{ "accounts",
		  NULL,
		  N_("_Accounts"),
		  NULL,
		  N_("Configure Evolution Accounts"),
		  action_accounts_cb, NULL, NULL, NULL },

		{ "close",
		  "window-close",
		  N_("_Close Window"),
		  "<Control>w",
		  N_("Close this window"),
		  action_close_cb, NULL, NULL, NULL },

		{ "close-window-menu",
		  "window-close",
		  N_("_Close"),
		  "<Control>w",
		  N_("Close this window"),
		  action_close_cb, NULL, NULL, NULL },

		{ "close-window",
		  "window-close",
		  N_("_Close Window"),
		  "<Control>w",
		  N_("Close this window"),
		  action_close_cb, NULL, NULL, NULL },

		{ "contents",
		  "help-browser",
		  N_("_Contents"),
		  "F1",
		  N_("Open the Evolution User Guide"),
		  action_contents_cb, NULL, NULL, NULL },

		{ "copy-clipboard",
		  "edit-copy",
		  N_("_Copy"),
		  "<Control>c",
		  N_("Copy the selection"),
		  NULL, NULL, NULL, NULL },  /* Handled by EFocusTracker */

		{ "cut-clipboard",
		  "edit-cut",
		  N_("Cu_t"),
		  "<Control>x",
		  N_("Cut the selection"),
		  NULL, NULL, NULL, NULL },  /* Handled by EFocusTracker */

		{ "delete-selection",
		  "edit-delete",
		  N_("_Delete"),
		  NULL,
		  N_("Delete the selection"),
		  NULL, NULL, NULL, NULL },  /* Handled by EFocusTracker */

		{ "import",
		  NULL,
		  N_("I_mport…"),
		  NULL,
		  N_("Import data from other programs"),
		  action_import_cb, NULL, NULL, NULL },

		{ "new-window",
		  "window-new",
		  N_("New _Window"),
		  "<Control><Shift>w",
		  N_("Create a new window displaying this view"),
		  action_new_window_cb, NULL, NULL, NULL },

		{ "paste-clipboard",
		  "edit-paste",
		  N_("_Paste"),
		  "<Control>v",
		  N_("Paste the clipboard"),
		  NULL, NULL, NULL, NULL },  /* Handled by EFocusTracker */

		{ "categories",
		  NULL,
		  N_("Available Cate_gories"),
		  NULL,
		  N_("Manage available categories"),
		  action_categories_cb, NULL, NULL, NULL },

		{ "preferences",
		  "preferences-system",
		  N_("_Preferences"),
		  "<Control><Shift>s",
		  N_("Configure Evolution"),
		  action_preferences_cb, NULL, NULL, NULL },

		{ "quit",
		  "application-exit",
		  N_("_Quit"),
		  "<Control>q",
		  N_("Exit the program"),
		  action_quit_cb, NULL, NULL, NULL },

		{ "select-all",
		  "edit-select-all",
		  N_("Select _All"),
		  "<Control>a",
		  N_("Select all text"),
		  NULL, NULL, NULL, NULL },  /* Handled by EFocusTracker */

		{ "shortcuts",
		  NULL,
		  N_("_Keyboard Shortcuts"),
		  "<Control><Shift>question",
		  N_("Show keyboard shortcuts"),
		  action_shortcuts_cb, NULL, NULL, NULL },

		{ "show-webkit-gpu",
		  NULL,
		  N_("Show _WebKit GPU information"),
		  NULL,
		  N_("Show WebKit GPU information page in the preview panel"),
		  action_show_webkit_gpu_cb, NULL, NULL, NULL },

		{ "submit-bug",
		  NULL,
		  N_("Submit _Bug Report…"),
		  NULL,
		  N_("Submit a bug report using Bug Buddy"),
		  action_submit_bug_cb, NULL, NULL, NULL },

		{ "work-offline",
		  NULL,
		  N_("_Work Offline"),
		  NULL,
		  N_("Put Evolution into offline mode"),
		  action_work_offline_cb, NULL, NULL, NULL },

		{ "work-online",
		  NULL,
		  N_("_Work Online"),
		  NULL,
		  N_("Put Evolution into online mode"),
		  action_work_online_cb, NULL, NULL, NULL },

		{ "EShellWindow::new-shortcut",
		  NULL,
		  N_("_New"),
		  "<Control>n",
		  NULL,
		  action_shell_window_new_shortcut_cb, NULL, NULL, NULL },

		/*** Menus ***/

		{ "edit-menu", NULL, N_("_Edit"), NULL, NULL, NULL, NULL, NULL, NULL },
		{ "file-menu", NULL, N_("_File"), NULL, NULL, NULL, NULL, NULL, NULL },
		{ "help-menu", NULL, N_("_Help"), NULL, NULL, NULL, NULL, NULL, NULL },
		{ "layout-menu", NULL, N_("Lay_out"), NULL, NULL, NULL, NULL, NULL, NULL },
		{ "search-menu", NULL, N_("_Search"), NULL, NULL, NULL, NULL, NULL, NULL },
		{ "switcher-menu", NULL, N_("_Switcher Appearance"), NULL, NULL, NULL, NULL, NULL, NULL },
		{ "view-menu", NULL, N_("_View"), NULL, NULL, NULL, NULL, NULL, NULL },
		{ "window-menu", NULL, N_("_Window"), NULL, NULL, NULL, NULL, NULL, NULL },
		{ "EShellWindow::new-button", NULL, "New", NULL, NULL, NULL, NULL, NULL, NULL }
	};

	static const EUIActionEntry shell_lockdown_print_setup_entries[] = {

		{ "page-setup",
		  "document-page-setup",
		  N_("Page Set_up…"),
		  NULL,
		  N_("Change the page settings for your current printer"),
		  action_page_setup_cb, NULL, NULL, NULL }
	};

	EFocusTracker *focus_tracker;
	EUIActionGroup *action_group;
	EUIManager *ui_manager; /* only temporary, for easier creation of the actions */
	GSettings *settings;
	const gchar *action_group_name;
	gchar *path;

	g_return_if_fail (E_IS_SHELL_WINDOW (shell_window));

	ui_manager = e_ui_manager_new (NULL);

	action_group_name = "shell";
	e_ui_manager_add_actions (ui_manager, action_group_name, NULL,
		shell_entries, G_N_ELEMENTS (shell_entries), shell_window);
	action_group = e_ui_manager_get_action_group (ui_manager, action_group_name);
	g_hash_table_insert (shell_window->priv->action_groups, g_strdup (action_group_name), g_object_ref (action_group));

	action_group_name = "lockdown-print-setup";
	e_ui_manager_add_actions (ui_manager, action_group_name, NULL,
		shell_lockdown_print_setup_entries, G_N_ELEMENTS (shell_lockdown_print_setup_entries), shell_window);
	action_group = e_ui_manager_get_action_group (ui_manager, action_group_name);
	g_hash_table_insert (shell_window->priv->action_groups, g_strdup (action_group_name), g_object_ref (action_group));

	e_ui_manager_set_actions_usable_for_kinds (ui_manager, E_UI_ELEMENT_KIND_MENU,
		"edit-menu",
		"file-menu",
		"help-menu",
		"layout-menu",
		"search-menu",
		"switcher-menu",
		"view-menu",
		"window-menu",
		NULL);

	g_clear_object (&ui_manager);

	action_group_name = "new-item";
	g_hash_table_insert (shell_window->priv->action_groups, g_strdup (action_group_name), e_ui_action_group_new (action_group_name));
	gtk_widget_insert_action_group (GTK_WIDGET (shell_window), action_group_name,
		G_ACTION_GROUP (g_hash_table_lookup (shell_window->priv->action_groups, action_group_name)));

	action_group_name = "new-source";
	g_hash_table_insert (shell_window->priv->action_groups, g_strdup (action_group_name), e_ui_action_group_new (action_group_name));
	gtk_widget_insert_action_group (GTK_WIDGET (shell_window), action_group_name,
		G_ACTION_GROUP (g_hash_table_lookup (shell_window->priv->action_groups, action_group_name)));

	action_group_name = "lockdown-application-handlers";
	g_hash_table_insert (shell_window->priv->action_groups, g_strdup (action_group_name), e_ui_action_group_new (action_group_name));

	action_group_name = "lockdown-printing";
	g_hash_table_insert (shell_window->priv->action_groups, g_strdup (action_group_name), e_ui_action_group_new (action_group_name));

	action_group_name = "lockdown-save-to-disk";
	g_hash_table_insert (shell_window->priv->action_groups, g_strdup (action_group_name), e_ui_action_group_new (action_group_name));

	/* only after the groups are created */
	e_shell_window_register_new_source_actions (shell_window, "shell",
		new_source_entries, G_N_ELEMENTS (new_source_entries));

	/* Configure an EFocusTracker to manage selection actions. */

	action_group = g_hash_table_lookup (shell_window->priv->action_groups, "shell");

	focus_tracker = e_focus_tracker_new (GTK_WINDOW (shell_window));

	e_focus_tracker_set_cut_clipboard_action (focus_tracker, e_ui_action_group_get_action (action_group, "cut-clipboard"));
	e_focus_tracker_set_copy_clipboard_action (focus_tracker, e_ui_action_group_get_action (action_group, "copy-clipboard"));
	e_focus_tracker_set_paste_clipboard_action (focus_tracker, e_ui_action_group_get_action (action_group, "paste-clipboard"));
	e_focus_tracker_set_delete_selection_action (focus_tracker, e_ui_action_group_get_action (action_group, "delete-selection"));
	e_focus_tracker_set_select_all_action (focus_tracker, e_ui_action_group_get_action (action_group, "select-all"));

	shell_window->priv->focus_tracker = focus_tracker;

	/* Submitting bug reports requires bug-buddy. */
	path = g_find_program_in_path ("bug-buddy");
	if (path == NULL)
		e_ui_action_set_visible (e_ui_action_group_get_action (action_group, "submit-bug"), FALSE);
	g_free (path);

	settings = e_util_ref_settings ("org.gnome.evolution.shell");
	e_ui_action_set_visible (e_ui_action_group_get_action (action_group, "show-webkit-gpu"), g_settings_get_boolean (settings, "webkit-developer-mode"));
	g_object_unref (settings);

	e_shell_window_create_views_actions (shell_window);
}

void
e_shell_window_init_ui_data (EShellWindow *shell_window,
			     EShellView *shell_view)
{
	EUIManager *ui_manager;
	EUIActionGroup *action_group;
	GHashTableIter iter;
	gpointer value;
	GError *local_error = NULL;

	ui_manager = e_shell_view_get_ui_manager (shell_view);

	/* add all action groups to the view's UI manager */
	g_hash_table_iter_init (&iter, shell_window->priv->action_groups);
	while (g_hash_table_iter_next (&iter, NULL, &value)) {
		action_group = value;
		e_ui_manager_add_action_group (ui_manager, action_group);
	}

	if (!e_ui_parser_merge_file (e_ui_manager_get_parser (ui_manager), "evolution-shell.eui", &local_error))
		g_warning ("%s: Failed to read evolution-shell.eui file: %s", G_STRFUNC, local_error ? local_error->message : "Unknown error");

	g_clear_error (&local_error);
}

static EUIAction *
e_shell_window_create_switcher_action (EShellViewClass *klass,
                                       const gchar *name,
                                       const gchar *tooltip,
                                       const gchar *view_name)
{
	EUIAction *action;

	action = e_ui_action_new_stateful ("shell", name, G_VARIANT_TYPE_STRING,
		g_variant_new_string (view_name));
	e_ui_action_set_label (action, klass->label);
	e_ui_action_set_tooltip (action, tooltip);
	e_ui_action_set_icon_name (action, klass->icon_name);

	return action;
}

void
e_shell_window_fill_switcher_actions (EShellWindow *shell_window,
				      EUIManager *ui_manager,
				      EShellSwitcher *switcher)
{
	EUIActionGroup *action_group;
	EShell *shell;
	GPtrArray *group;
	GList *list, *iter;
	guint ii = 1;

	g_return_if_fail (E_IS_SHELL_WINDOW (shell_window));

	group = g_ptr_array_new ();
	shell = e_shell_window_get_shell (shell_window);
	list = e_shell_get_shell_backends (shell);

	/* Construct a group of radio actions from the various EShellView
	 * subclasses and register them with the EShellSwitcher.  These
	 * actions are manifested as switcher buttons and View->Window
	 * menu items. */

	action_group = e_shell_window_get_ui_action_group (shell_window, "shell");

	for (iter = list; iter != NULL; iter = iter->next, ii++) {
		EShellBackend *shell_backend = iter->data;
		EShellBackendClass *backend_class;
		EShellViewClass *class;
		EUIAction *s_action, *n_action;
		GType view_type;
		const gchar *view_name;
		gchar tmp_str[128];
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

		g_warn_if_fail (g_snprintf (tmp_str, sizeof (tmp_str), E_SHELL_SWITCHER_FORMAT, view_name) < sizeof (tmp_str));

		s_action = e_ui_action_group_get_action (action_group, tmp_str);
		if (s_action) {
			g_object_ref (s_action);
		} else {
			/* these are prepared beforehand, within e_shell_window_create_views_actions() */
			g_warn_if_reached ();
		}

		/* Create new window actions */
		g_warn_if_fail (g_snprintf (tmp_str, sizeof (tmp_str), "new-%s-window", view_name) < sizeof (tmp_str));

		n_action = e_ui_action_group_get_action (action_group, tmp_str);
		if (n_action) {
			g_object_ref (n_action);
		} else {
			n_action = e_shell_window_create_switcher_action (class, tmp_str, tooltip, view_name);
			g_signal_connect (
				n_action, "activate",
				G_CALLBACK (action_new_view_window_cb), shell_window);
			e_ui_action_group_add (action_group, n_action);
		}

		e_shell_switcher_add_action (switcher, s_action, n_action);

		g_clear_object (&s_action);
		g_clear_object (&n_action);
		g_free (tooltip);

		g_type_class_unref (class);
	}

	g_ptr_array_unref (group);
}

static void
e_shell_window_create_views_actions (EShellWindow *shell_window)
{
	EUIActionGroup *action_group;
	EShell *shell;
	EUIManager *tmp_ui_manager;
	GPtrArray *group;
	GList *list, *iter;
	guint ii = 1;

	g_return_if_fail (E_IS_SHELL_WINDOW (shell_window));

	group = NULL;
	shell = e_shell_window_get_shell (shell_window);
	list = e_shell_get_shell_backends (shell);
	tmp_ui_manager = e_ui_manager_new (NULL);

	/* Construct a group of radio actions from the various EShellView
	 * subclasses. */

	action_group = e_shell_window_get_ui_action_group (shell_window, "shell");

	for (iter = list; iter != NULL; iter = iter->next, ii++) {
		EShellBackend *shell_backend = iter->data;
		EShellBackendClass *backend_class;
		EShellViewClass *klass;
		EUIAction *s_action;
		GType view_type;
		const gchar *view_name;
		gchar tmp_str[128];
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

		klass = g_type_class_ref (view_type);

		if (klass->label == NULL) {
			g_critical (
				"Label member not set on %s",
				G_OBJECT_CLASS_NAME (klass));
			continue;
		}

		tooltip = g_strdup_printf (_("Switch to %s"), klass->label);

		g_warn_if_fail (g_snprintf (tmp_str, sizeof (tmp_str), E_SHELL_SWITCHER_FORMAT, view_name) < sizeof (tmp_str));

		s_action = e_ui_action_group_get_action (action_group, tmp_str);
		if (s_action) {
			g_object_ref (s_action);
			if (!group) {
				group = e_ui_action_get_radio_group (s_action);
				if (group) {
					g_ptr_array_ref (group);
				} else {
					group = g_ptr_array_new ();
					e_ui_action_set_radio_group (s_action, group);
				}
			}
		} else {
			GMenuItem *menu_item;

			if (!group)
				group = g_ptr_array_new ();

			s_action = e_shell_window_create_switcher_action (klass, tmp_str, tooltip, view_name);
			e_ui_action_set_radio_group (s_action, group);
			e_ui_action_group_add (action_group, s_action);

			g_signal_connect_object (s_action, "notify::active",
				G_CALLBACK (action_switcher_cb), shell_window, 0);

			/* The first nine views have accelerators Ctrl+(1-9). */
			if (ii < 10) {
				g_warn_if_fail (g_snprintf (tmp_str, sizeof (tmp_str), "<Control>%d", ii) < sizeof (tmp_str));
				e_ui_action_set_accel (s_action, tmp_str);
			}

			menu_item = g_menu_item_new (NULL, NULL);
			/* cannot use custom icons here, because the tmp_ui_manager is
			   only temporary and without set callbacks for the "create-gicon"
			   signal, but it's not a problem here; the actions are needed
			   before the EShellView is created, because it references it */
			e_ui_manager_update_item_from_action (tmp_ui_manager, menu_item, s_action);
			g_menu_append_item (shell_window->priv->switch_to_menu, menu_item);
			g_clear_object (&menu_item);
		}

		g_clear_object (&s_action);
		g_free (tooltip);

		g_type_class_unref (klass);
	}

	g_clear_object (&tmp_ui_manager);
	g_clear_pointer (&group, g_ptr_array_unref);
}
