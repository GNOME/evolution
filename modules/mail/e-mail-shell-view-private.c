/*
 * e-mail-shell-view-private.c
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) version 3.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with the program; if not, see <http://www.gnu.org/licenses/>
 *
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#include "e-mail-shell-view-private.h"

#include "widgets/menus/gal-view-factory-etable.h"

#include "e-util/e-util-private.h"

static void
mail_shell_view_folder_tree_selected_cb (EMailShellView *mail_shell_view,
                                         const gchar *full_name,
                                         const gchar *uri,
                                         guint32 flags,
                                         EMFolderTree *folder_tree)
{
	EMailShellContent *mail_shell_content;
	EShellView *shell_view;
	EMailReader *reader;
	EMailView *mail_view;
	gboolean folder_selected;

	shell_view = E_SHELL_VIEW (mail_shell_view);

	mail_shell_content = mail_shell_view->priv->mail_shell_content;
	mail_view = e_mail_shell_content_get_mail_view (mail_shell_content);

	reader = E_MAIL_READER (mail_view);

	folder_selected =
		!(flags & CAMEL_FOLDER_NOSELECT) &&
		full_name != NULL;

	if (folder_selected)
		e_mail_reader_set_folder_uri (reader, uri);
	else
		e_mail_reader_set_folder (reader, NULL, NULL);

	e_shell_view_update_actions (shell_view);
}

static gboolean
mail_shell_view_folder_tree_key_press_event_cb (EMailShellView *mail_shell_view,
                                                GdkEventKey *event)
{
	EMailShellContent *mail_shell_content;
	EMailView *mail_view;
	gboolean handled = FALSE;

	mail_shell_content = mail_shell_view->priv->mail_shell_content;
	mail_view = e_mail_shell_content_get_mail_view (mail_shell_content);

	if ((event->state & GDK_CONTROL_MASK) != 0)
		goto ctrl;

	/* <keyval> alone */
	switch (event->keyval) {
		case GDK_period:
		case GDK_comma:
		case GDK_bracketleft:
		case GDK_bracketright:
			goto emit;

		default:
			goto exit;
	}

ctrl:
	/* Ctrl + <keyval> */
	switch (event->keyval) {
		case GDK_period:
		case GDK_comma:
			goto emit;

		default:
			goto exit;
	}

	/* All branches jump past this. */
	g_return_val_if_reached (FALSE);

emit:
	/* Forward the key press to the EMailReader interface. */
	g_signal_emit_by_name (mail_view, "key-press-event", event, &handled);

exit:
	return handled;
}

static void
mail_shell_view_folder_tree_selection_done_cb (EMailShellView *mail_shell_view,
                                               GtkWidget *menu)
{
	EMailShellContent *mail_shell_content;
	EMailShellSidebar *mail_shell_sidebar;
	EMFolderTree *folder_tree;
	GtkWidget *message_list;
	EMailReader *reader;
	EMailView *mail_view;
	const gchar *list_uri;
	gchar *tree_uri;

	mail_shell_content = mail_shell_view->priv->mail_shell_content;
	mail_view = e_mail_shell_content_get_mail_view (mail_shell_content);

	mail_shell_sidebar = mail_shell_view->priv->mail_shell_sidebar;
	folder_tree = e_mail_shell_sidebar_get_folder_tree (mail_shell_sidebar);

	reader = E_MAIL_READER (mail_view);
	message_list = e_mail_reader_get_message_list (reader);

	/* Don't use e_mail_reader_get_folder_uri() here.  The fact that
	 * the method gets the folder URI from the message list is supposed
	 * to be a hidden implementation detail, and we want to explicitly
	 * get the folder URI from the message list here. */
	list_uri = MESSAGE_LIST (message_list)->folder_uri;
	tree_uri = em_folder_tree_get_selected_uri (folder_tree);

	/* If the folder tree and message list disagree on the current
	 * folder, reset the folder tree to match the message list. */
	if (g_strcmp0 (tree_uri, list_uri) != 0)
		em_folder_tree_set_selected (folder_tree, list_uri, FALSE);

	g_free (tree_uri);

	/* Disconnect from the "selection-done" signal. */
	g_signal_handlers_disconnect_by_func (
		menu, mail_shell_view_folder_tree_selection_done_cb,
		mail_shell_view);
}

static void
mail_shell_view_folder_tree_popup_event_cb (EShellView *shell_view,
                                            GdkEventButton *event)
{
	GtkWidget *menu;
	const gchar *widget_path;

	widget_path = "/mail-folder-popup";
	menu = e_shell_view_show_popup_menu (shell_view, widget_path, event);

	g_signal_connect_object (
		menu, "selection-done",
		G_CALLBACK (mail_shell_view_folder_tree_selection_done_cb),
		shell_view, G_CONNECT_SWAPPED);
}

static gboolean
mail_shell_view_key_press_event_cb (EMailShellView *mail_shell_view,
                                    GdkEventKey *event)
{
	EShellView *shell_view;
	EShellWindow *shell_window;
	GtkAction *action;

	shell_view = E_SHELL_VIEW (mail_shell_view);
	shell_window = e_shell_view_get_shell_window (shell_view);

	if ((event->state & GDK_CONTROL_MASK) != 0)
		return FALSE;

	switch (event->keyval) {
		case GDK_space:
			action = ACTION (MAIL_SMART_FORWARD);
			break;

		case GDK_BackSpace:
			action = ACTION (MAIL_SMART_BACKWARD);
			break;

		default:
			return FALSE;
	}

	gtk_action_activate (action);

	return TRUE;
}

static gboolean
mail_shell_view_message_list_key_press_cb (EMailShellView *mail_shell_view,
                                           gint row,
                                           ETreePath path,
                                           gint col,
                                           GdkEvent *event)
{
	return mail_shell_view_key_press_event_cb (
		mail_shell_view, &event->key);
}

static gboolean
mail_shell_view_message_list_popup_menu_cb (EShellView *shell_view)
{
	const gchar *widget_path;

	widget_path = "/mail-message-popup";
	e_shell_view_show_popup_menu (shell_view, widget_path, NULL);

	return TRUE;
}

static gboolean
mail_shell_view_message_list_right_click_cb (EShellView *shell_view,
                                             gint row,
                                             ETreePath path,
                                             gint col,
                                             GdkEventButton *event)
{
	const gchar *widget_path;

	widget_path = "/mail-message-popup";
	e_shell_view_show_popup_menu (shell_view, widget_path, event);

	return TRUE;
}

static gboolean
mail_shell_view_popup_event_cb (EMailShellView *mail_shell_view,
                                GdkEventButton *event,
                                const gchar *uri)
{
	EMailShellContent *mail_shell_content;
	EShellView *shell_view;
	EMailReader *reader;
	EMailView *mail_view;
	GtkMenu *menu;

	if (uri != NULL)
		return FALSE;

	mail_shell_content = mail_shell_view->priv->mail_shell_content;
	mail_view = e_mail_shell_content_get_mail_view (mail_shell_content);

	reader = E_MAIL_READER (mail_view);
	menu = e_mail_reader_get_popup_menu (reader);

	shell_view = E_SHELL_VIEW (mail_shell_view);
	e_shell_view_update_actions (shell_view);

	if (event == NULL)
		gtk_menu_popup (
			menu, NULL, NULL, NULL, NULL,
			0, gtk_get_current_event_time ());
	else
		gtk_menu_popup (
			menu, NULL, NULL, NULL, NULL,
			event->button, event->time);

	return TRUE;
}

static void
mail_shell_view_scroll_cb (EMailShellView *mail_shell_view,
                           GtkOrientation orientation,
                           GtkScrollType scroll_type,
                           gfloat position,
                           GtkHTML *html)
{
	EShell *shell;
	EShellView *shell_view;
	EShellWindow *shell_window;
	EShellSettings *shell_settings;
	EMailShellContent *mail_shell_content;
	EMailReader *reader;
	EMailView *mail_view;
	EWebView *web_view;
	GtkWidget *message_list;
	gboolean magic_spacebar;

	web_view = E_WEB_VIEW (html);

	if (html->binding_handled || e_web_view_get_caret_mode (web_view))
		return;

	if (orientation != GTK_ORIENTATION_VERTICAL)
		return;

	shell_view = E_SHELL_VIEW (mail_shell_view);
	shell_window = e_shell_view_get_shell_window (shell_view);
	shell = e_shell_window_get_shell (shell_window);
	shell_settings = e_shell_get_shell_settings (shell);

	magic_spacebar = e_shell_settings_get_boolean (
		shell_settings, "mail-magic-spacebar");

	if (!magic_spacebar)
		return;

	mail_shell_content = mail_shell_view->priv->mail_shell_content;
	mail_view = e_mail_shell_content_get_mail_view (mail_shell_content);

	reader = E_MAIL_READER (mail_view);
	message_list = e_mail_reader_get_message_list (reader);

	if (scroll_type == GTK_SCROLL_PAGE_FORWARD)
		message_list_select (
			MESSAGE_LIST (message_list),
			MESSAGE_LIST_SELECT_NEXT,
			0, CAMEL_MESSAGE_SEEN);
	else
		message_list_select (
			MESSAGE_LIST (message_list),
			MESSAGE_LIST_SELECT_PREVIOUS,
			0, CAMEL_MESSAGE_SEEN);
}

static void
mail_shell_view_reader_changed_cb (EMailShellView *mail_shell_view,
                                   EMailReader *reader)
{
	GtkWidget *message_list;
	EMFormatHTML *formatter;
	EWebView *web_view;
	EShellView *shell_view;
	EShellTaskbar *shell_taskbar;

	shell_view = E_SHELL_VIEW (mail_shell_view);
	shell_taskbar = e_shell_view_get_shell_taskbar (shell_view);

	formatter = e_mail_reader_get_formatter (reader);
	message_list = e_mail_reader_get_message_list (reader);
	web_view = em_format_html_get_web_view (formatter);

	e_shell_view_update_actions (E_SHELL_VIEW (mail_shell_view));
	e_mail_shell_view_update_sidebar (mail_shell_view);

	/* Connect if its not connected already */
	if (g_signal_handler_find (
		message_list, G_SIGNAL_MATCH_FUNC, 0, 0, NULL,
		mail_shell_view_message_list_key_press_cb, NULL))
		return;

	g_signal_connect_object (
		message_list, "key-press",
		G_CALLBACK (mail_shell_view_message_list_key_press_cb),
		mail_shell_view, G_CONNECT_SWAPPED);

	g_signal_connect_object (
		message_list, "popup-menu",
		G_CALLBACK (mail_shell_view_message_list_popup_menu_cb),
		mail_shell_view, G_CONNECT_SWAPPED);

	g_signal_connect_object (
		message_list, "right-click",
		G_CALLBACK (mail_shell_view_message_list_right_click_cb),
		mail_shell_view, G_CONNECT_SWAPPED);

	g_signal_connect_object (
		web_view, "key-press-event",
		G_CALLBACK (mail_shell_view_key_press_event_cb),
		mail_shell_view, G_CONNECT_SWAPPED);

	g_signal_connect_object (
		web_view, "popup-event",
		G_CALLBACK (mail_shell_view_popup_event_cb),
		mail_shell_view, G_CONNECT_SWAPPED);

	g_signal_connect_object (
		web_view, "scroll",
		G_CALLBACK (mail_shell_view_scroll_cb),
		mail_shell_view,
		G_CONNECT_AFTER | G_CONNECT_SWAPPED);

	g_signal_connect_object (
		web_view, "status-message",
		G_CALLBACK (e_shell_taskbar_set_message),
		shell_taskbar, G_CONNECT_SWAPPED);
}

static void
mail_shell_view_prepare_for_quit_done_cb (CamelFolder *folder,
                                          gpointer user_data)
{
	g_object_unref (E_ACTIVITY (user_data));
}

static void
mail_shell_view_prepare_for_quit_cb (EMailShellView *mail_shell_view,
                                     EActivity *activity)
{
	EMailShellContent *mail_shell_content;
	CamelFolder *folder;
	EMailReader *reader;
	EMailView *mail_view;
	GtkWidget *message_list;

	/* If we got here, it means the application is shutting down
	 * and this is the last EMailShellView instance.  Synchronize
	 * the currently selected folder before we terminate. */

	mail_shell_content = mail_shell_view->priv->mail_shell_content;
	mail_view = e_mail_shell_content_get_mail_view (mail_shell_content);

	reader = E_MAIL_READER (mail_view);
	folder = e_mail_reader_get_folder (reader);
	message_list = e_mail_reader_get_message_list (reader);

	message_list_save_state (MESSAGE_LIST (message_list));

	if (folder == NULL)
		return;

	mail_sync_folder (
		folder,
		mail_shell_view_prepare_for_quit_done_cb,
		g_object_ref (activity));
}

static void
mail_shell_view_load_view_collection (EShellViewClass *shell_view_class)
{
	GalViewCollection *collection;
	GalViewFactory *factory;
	ETableSpecification *spec;
	const gchar *base_dir;
	gchar *filename;

	collection = shell_view_class->view_collection;

	base_dir = EVOLUTION_ETSPECDIR;
	spec = e_table_specification_new ();
	filename = g_build_filename (base_dir, ETSPEC_FILENAME, NULL);
	if (!e_table_specification_load_from_file (spec, filename))
		g_critical ("Unable to load ETable specification file "
			    "for mail");
	g_free (filename);

	factory = gal_view_factory_etable_new (spec);
	gal_view_collection_add_factory (collection, factory);
	g_object_unref (factory);
	g_object_unref (spec);

	gal_view_collection_load (collection);
}

static void
mail_shell_view_notify_view_id_cb (EMailShellView *mail_shell_view)
{
	EMailShellContent *mail_shell_content;
	GalViewInstance *view_instance;
	EMailView *mail_view;
	const gchar *view_id;

	mail_shell_content = mail_shell_view->priv->mail_shell_content;
	mail_view = e_mail_shell_content_get_mail_view (mail_shell_content);

	view_instance = e_mail_view_get_view_instance (mail_view);
	view_id = e_shell_view_get_view_id (E_SHELL_VIEW (mail_shell_view));

	/* A NULL view ID implies we're in a custom view.  But you can
	 * only get to a custom view via the "Define Views" dialog, which
	 * would have already modified the view instance appropriately.
	 * Furthermore, there's no way to refer to a custom view by ID
	 * anyway, since custom views have no IDs. */
	if (view_id == NULL)
		return;

	gal_view_instance_set_current_view_id (view_instance, view_id);
}

void
e_mail_shell_view_private_init (EMailShellView *mail_shell_view,
                                EShellViewClass *shell_view_class)
{
	if (!gal_view_collection_loaded (shell_view_class->view_collection))
		mail_shell_view_load_view_collection (shell_view_class);

	g_signal_connect (
		mail_shell_view, "notify::view-id",
		G_CALLBACK (mail_shell_view_notify_view_id_cb), NULL);
}

void
e_mail_shell_view_private_constructed (EMailShellView *mail_shell_view)
{
	EMailShellViewPrivate *priv = mail_shell_view->priv;
	EMailShellContent *mail_shell_content;
	EMailShellSidebar *mail_shell_sidebar;
	EShell *shell;
	EShellView *shell_view;
	EShellBackend *shell_backend;
	EShellContent *shell_content;
	EShellSettings *shell_settings;
	EShellSidebar *shell_sidebar;
	EShellTaskbar *shell_taskbar;
	EShellWindow *shell_window;
	EShellSearchbar *searchbar;
	EMFormatHTML *formatter;
	EMFolderTree *folder_tree;
	EActionComboBox *combo_box;
	ERuleContext *context;
	EFilterRule *rule = NULL;
	GtkTreeSelection *selection;
	GtkTreeModel *tree_model;
	GtkUIManager *ui_manager;
	GtkWidget *message_list;
	EMailReader *reader;
	EMailView *mail_view;
	EWebView *web_view;
	const gchar *source;
	guint merge_id;
	gint ii = 0;

	shell_view = E_SHELL_VIEW (mail_shell_view);
	shell_backend = e_shell_view_get_shell_backend (shell_view);
	shell_content = e_shell_view_get_shell_content (shell_view);
	shell_sidebar = e_shell_view_get_shell_sidebar (shell_view);
	shell_taskbar = e_shell_view_get_shell_taskbar (shell_view);
	shell_window = e_shell_view_get_shell_window (shell_view);

	ui_manager = e_shell_window_get_ui_manager (shell_window);

	shell = e_shell_window_get_shell (shell_window);
	shell_settings = e_shell_get_shell_settings (shell);

	tree_model = e_shell_settings_get_object (
		shell_settings, "mail-label-list-store");

	e_shell_window_add_action_group (shell_window, "mail");
	e_shell_window_add_action_group (shell_window, "mail-filter");
	e_shell_window_add_action_group (shell_window, "mail-label");

	merge_id = gtk_ui_manager_new_merge_id (ui_manager);
	priv->label_merge_id = merge_id;

	/* Cache these to avoid lots of awkward casting. */
	priv->mail_shell_backend = g_object_ref (shell_backend);
	priv->mail_shell_content = g_object_ref (shell_content);
	priv->mail_shell_sidebar = g_object_ref (shell_sidebar);

	mail_shell_sidebar = E_MAIL_SHELL_SIDEBAR (shell_sidebar);
	folder_tree = e_mail_shell_sidebar_get_folder_tree (mail_shell_sidebar);
	selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (folder_tree));

	mail_shell_content = E_MAIL_SHELL_CONTENT (shell_content);
	mail_view = e_mail_shell_content_get_mail_view (mail_shell_content);
	searchbar = e_mail_shell_content_get_searchbar (mail_shell_content);
	combo_box = e_shell_searchbar_get_scope_combo_box (searchbar);

	reader = E_MAIL_READER (shell_content);
	formatter = e_mail_reader_get_formatter (reader);
	message_list = e_mail_reader_get_message_list (reader);

	em_folder_tree_set_selectable_widget (folder_tree, message_list);

	/* The folder tree and scope combo box are both insensitive
	 * when searching beyond the currently selected folder. */
	e_mutual_binding_new (
		folder_tree, "sensitive",
		combo_box, "sensitive");

	web_view = em_format_html_get_web_view (formatter);

	g_signal_connect_object (
		folder_tree, "folder-selected",
		G_CALLBACK (mail_shell_view_folder_tree_selected_cb),
		mail_shell_view, G_CONNECT_SWAPPED);

	g_signal_connect_object (
		folder_tree, "key-press-event",
		G_CALLBACK (mail_shell_view_folder_tree_key_press_event_cb),
		mail_shell_view, G_CONNECT_SWAPPED);

	g_signal_connect_object (
		folder_tree, "popup-event",
		G_CALLBACK (mail_shell_view_folder_tree_popup_event_cb),
		mail_shell_view, G_CONNECT_SWAPPED);

	g_signal_connect_object (
		message_list, "key-press",
		G_CALLBACK (mail_shell_view_message_list_key_press_cb),
		mail_shell_view, G_CONNECT_SWAPPED);

	g_signal_connect_object (
		message_list, "popup-menu",
		G_CALLBACK (mail_shell_view_message_list_popup_menu_cb),
		mail_shell_view, G_CONNECT_SWAPPED);

	g_signal_connect_object (
		message_list, "right-click",
		G_CALLBACK (mail_shell_view_message_list_right_click_cb),
		mail_shell_view, G_CONNECT_SWAPPED);

	g_signal_connect_object (
		reader, "changed",
		G_CALLBACK (mail_shell_view_reader_changed_cb),
		mail_shell_view, G_CONNECT_SWAPPED);

	g_signal_connect_object (
		reader, "folder-loaded",
		G_CALLBACK (e_mail_view_update_view_instance),
		mail_view, G_CONNECT_SWAPPED);

	/* Use the same callback as "changed". */
	g_signal_connect_object (
		reader, "folder-loaded",
		G_CALLBACK (mail_shell_view_reader_changed_cb),
		mail_shell_view, G_CONNECT_SWAPPED);

	g_signal_connect_object (
		reader, "folder-loaded",
		G_CALLBACK (e_mail_shell_view_restore_state),
		mail_shell_view, G_CONNECT_SWAPPED);

	g_signal_connect_object (
		tree_model, "row-changed",
		G_CALLBACK (e_mail_shell_view_update_search_filter),
		mail_shell_view, G_CONNECT_SWAPPED);

	g_signal_connect_object (
		tree_model, "row-deleted",
		G_CALLBACK (e_mail_shell_view_update_search_filter),
		mail_shell_view, G_CONNECT_SWAPPED);

	g_signal_connect_object (
		tree_model, "row-inserted",
		G_CALLBACK (e_mail_shell_view_update_search_filter),
		mail_shell_view, G_CONNECT_SWAPPED);

	g_signal_connect_object (
		web_view, "key-press-event",
		G_CALLBACK (mail_shell_view_key_press_event_cb),
		mail_shell_view, G_CONNECT_SWAPPED);

	g_signal_connect_object (
		web_view, "popup-event",
		G_CALLBACK (mail_shell_view_popup_event_cb),
		mail_shell_view, G_CONNECT_SWAPPED);

	g_signal_connect_object (
		web_view, "scroll",
		G_CALLBACK (mail_shell_view_scroll_cb),
		mail_shell_view,
		G_CONNECT_AFTER | G_CONNECT_SWAPPED);

	g_signal_connect_object (
		web_view, "status-message",
		G_CALLBACK (e_shell_taskbar_set_message),
		shell_taskbar, G_CONNECT_SWAPPED);

	/* Need to keep the handler ID so we can disconnect it in
	 * dispose().  The shell outlives us and we don't want it
	 * invoking callbacks on finalized shell views. */
	priv->prepare_for_quit_handler_id =
		g_signal_connect_object (
			shell, "prepare-for-quit",
			G_CALLBACK (mail_shell_view_prepare_for_quit_cb),
			mail_shell_view, G_CONNECT_SWAPPED);

	e_mail_reader_init (reader, TRUE, FALSE);
	e_mail_shell_view_actions_init (mail_shell_view);
	e_mail_shell_view_update_search_filter (mail_shell_view);

	/* This binding must come after e_mail_reader_init(). */
	e_mutual_binding_new (
		shell_content, "group-by-threads",
		mail_view, "group-by-threads");

	/* Populate built-in rules for search entry popup menu.
	 * Keep the assertions, please.  If the conditions aren't
	 * met we're going to crash anyway, just more mysteriously. */
	context = E_SHELL_VIEW_GET_CLASS (shell_view)->search_context;
	source = E_FILTER_SOURCE_DEMAND;
	while ((rule = e_rule_context_next_rule (context, rule, source))) {
		if (!rule->system)
			continue;
		g_assert (ii < MAIL_NUM_SEARCH_RULES);
		priv->search_rules[ii++] = g_object_ref (rule);
	}
	g_assert (ii == MAIL_NUM_SEARCH_RULES);

	/* Now that we're all set up, simulate selecting a folder. */
	g_signal_emit_by_name (selection, "changed");
}

void
e_mail_shell_view_private_dispose (EMailShellView *mail_shell_view)
{
	EMailShellViewPrivate *priv = mail_shell_view->priv;
	gint ii;

	/* XXX It's a little awkward to have to dig up the
	 *     shell this late in the game.  Should we just
	 *     keep a direct reference to it?  Not sure. */
	if (priv->prepare_for_quit_handler_id > 0) {
		EShellBackend *shell_backend;
		EShell *shell;

		shell_backend = E_SHELL_BACKEND (priv->mail_shell_backend);
		shell = e_shell_backend_get_shell (shell_backend);

		g_signal_handler_disconnect (
			shell, priv->prepare_for_quit_handler_id);
		priv->prepare_for_quit_handler_id = 0;
	}

	DISPOSE (priv->mail_shell_backend);
	DISPOSE (priv->mail_shell_content);
	DISPOSE (priv->mail_shell_sidebar);

	for (ii = 0; ii < MAIL_NUM_SEARCH_RULES; ii++)
		DISPOSE (priv->search_rules[ii]);

	if (priv->search_account_all != NULL) {
		g_object_unref (priv->search_account_all);
		priv->search_account_all = NULL;
	}

	if (priv->search_account_current != NULL) {
		g_object_unref (priv->search_account_current);
		priv->search_account_current = NULL;
	}

	if (priv->search_account_cancel != NULL) {
		camel_operation_unref (priv->search_account_cancel);
		priv->search_account_cancel = NULL;
	}
}

void
e_mail_shell_view_private_finalize (EMailShellView *mail_shell_view)
{
	/* XXX Nothing to do? */
}

void
e_mail_shell_view_restore_state (EMailShellView *mail_shell_view)
{
	EMailShellContent *mail_shell_content;
	EShellSearchbar *searchbar;
	EMailReader *reader;
	EMailView *mail_view;
	CamelFolder *folder;
	CamelVeeFolder *vee_folder;
	const gchar *old_state_group;
	const gchar *folder_uri;
	gchar *new_state_group;

	/* XXX Move this to EMailShellContent. */

	g_return_if_fail (E_IS_MAIL_SHELL_VIEW (mail_shell_view));

	mail_shell_content = mail_shell_view->priv->mail_shell_content;
	mail_view = e_mail_shell_content_get_mail_view (mail_shell_content);
	searchbar = e_mail_shell_content_get_searchbar (mail_shell_content);

	reader = E_MAIL_READER (mail_view);
	folder = e_mail_reader_get_folder (reader);
	folder_uri = e_mail_reader_get_folder_uri (reader);

	if (folder_uri == NULL)
		return;

	/* Do not restore state if we're running a "Current Account"
	 * or "All Accounts" search, since we don't want the search
	 * criteria to be destroyed in those cases. */

	vee_folder = mail_shell_view->priv->search_account_all;
	if (vee_folder != NULL && folder == CAMEL_FOLDER (vee_folder))
		return;

	vee_folder = mail_shell_view->priv->search_account_current;
	if (vee_folder != NULL && folder == CAMEL_FOLDER (vee_folder))
		return;

	new_state_group = g_strdup_printf ("Folder %s", folder_uri);
	old_state_group = e_shell_searchbar_get_state_group (searchbar);

	/* Avoid loading search state unnecessarily. */
	if (g_strcmp0 (new_state_group, old_state_group) != 0) {
		e_shell_searchbar_set_state_group (searchbar, new_state_group);
		e_shell_searchbar_load_state (searchbar);
	}

	g_free (new_state_group);
}

/* Helper for e_mail_shell_view_create_filter_from_selected() */
static void
mail_shell_view_create_filter_cb (CamelFolder *folder,
                                  const gchar *uid,
                                  CamelMimeMessage *message,
                                  gpointer user_data)
{
	struct {
		const gchar *source;
		gint type;
	} *filter_data = user_data;

	if (message != NULL)
		filter_gui_add_from_message (
			message, filter_data->source, filter_data->type);

	g_free (filter_data);
}

void
e_mail_shell_view_create_filter_from_selected (EMailShellView *mail_shell_view,
                                               gint filter_type)
{
	EMailShellContent *mail_shell_content;
	EMailReader *reader;
	EMailView *mail_view;
	CamelFolder *folder;
	const gchar *filter_source;
	const gchar *folder_uri;
	GPtrArray *uids;

	struct {
		const gchar *source;
		gint type;
	} *filter_data;

	g_return_if_fail (E_IS_MAIL_SHELL_VIEW (mail_shell_view));

	mail_shell_content = mail_shell_view->priv->mail_shell_content;
	mail_view = e_mail_shell_content_get_mail_view (mail_shell_content);

	reader = E_MAIL_READER (mail_view);
	folder = e_mail_reader_get_folder (reader);
	folder_uri = e_mail_reader_get_folder_uri (reader);
	uids = e_mail_reader_get_selected_uids (reader);

	if (em_utils_folder_is_sent (folder, folder_uri))
		filter_source = E_FILTER_SOURCE_OUTGOING;
	else if (em_utils_folder_is_outbox (folder, folder_uri))
		filter_source = E_FILTER_SOURCE_OUTGOING;
	else
		filter_source = E_FILTER_SOURCE_INCOMING;

	if (uids->len == 1) {
		filter_data = g_malloc (sizeof (*filter_data));
		filter_data->source = filter_source;
		filter_data->type = filter_type;

		mail_get_message (
			folder, uids->pdata[0],
			mail_shell_view_create_filter_cb,
			filter_data, mail_msg_unordered_push);
	}

	em_utils_uids_free (uids);
}

/* Helper for e_mail_shell_view_create_vfolder_from_selected() */
static void
mail_shell_view_create_vfolder_cb (CamelFolder *folder,
                                   const gchar *uid,
                                   CamelMimeMessage *message,
                                   gpointer user_data)
{
	struct {
		gchar *uri;
		gint type;
	} *vfolder_data = user_data;

	if (message != NULL)
		vfolder_gui_add_from_message (
			message, vfolder_data->type, vfolder_data->uri);

	g_free (vfolder_data->uri);
	g_free (vfolder_data);
}

void
e_mail_shell_view_create_vfolder_from_selected (EMailShellView *mail_shell_view,
                                                gint vfolder_type)
{
	EMailShellContent *mail_shell_content;
	EMailReader *reader;
	EMailView *mail_view;
	CamelFolder *folder;
	const gchar *folder_uri;
	GPtrArray *uids;

	struct {
		gchar *uri;
		gint type;
	} *vfolder_data;

	g_return_if_fail (E_IS_MAIL_SHELL_VIEW (mail_shell_view));

	mail_shell_content = mail_shell_view->priv->mail_shell_content;
	mail_view = e_mail_shell_content_get_mail_view (mail_shell_content);

	reader = E_MAIL_READER (mail_view);
	folder = e_mail_reader_get_folder (reader);
	folder_uri = e_mail_reader_get_folder_uri (reader);
	uids = e_mail_reader_get_selected_uids (reader);

	if (uids->len == 1) {
		vfolder_data = g_malloc (sizeof (*vfolder_data));
		vfolder_data->uri = g_strdup (folder_uri);
		vfolder_data->type = vfolder_type;

		mail_get_message (
			folder, uids->pdata[0],
			mail_shell_view_create_vfolder_cb,
			vfolder_data, mail_msg_unordered_push);
	}

	em_utils_uids_free (uids);
}

void
e_mail_shell_view_update_sidebar (EMailShellView *mail_shell_view)
{
	EMailShellContent *mail_shell_content;
	EShellSidebar *shell_sidebar;
	EShellView *shell_view;
	EMailReader *reader;
	EMailView *mail_view;
	CamelStore *local_store;
	CamelStore *parent_store;
	CamelFolder *folder;
	GPtrArray *uids;
	GString *buffer;
	const gchar *display_name;
	const gchar *folder_name;
	const gchar *folder_uri;
	gchar *title;
	guint32 num_deleted;
	guint32 num_junked;
	guint32 num_junked_not_deleted;
	guint32 num_unread;
	guint32 num_visible;

	g_return_if_fail (E_IS_MAIL_SHELL_VIEW (mail_shell_view));

	mail_shell_content = mail_shell_view->priv->mail_shell_content;
	mail_view = e_mail_shell_content_get_mail_view (mail_shell_content);

	shell_view = E_SHELL_VIEW (mail_shell_view);
	shell_sidebar = e_shell_view_get_shell_sidebar (shell_view);

	reader = E_MAIL_READER (mail_view);
	folder = e_mail_reader_get_folder (reader);
	folder_uri = e_mail_reader_get_folder_uri (reader);

	local_store = e_mail_local_get_store ();

	/* If no folder is selected, reset the sidebar banners
	 * to their default values and stop. */
	if (folder == NULL) {
		GtkAction *action;
		gchar *label;

		action = e_shell_view_get_action (shell_view);

		g_object_get (action, "label", &label, NULL);
		e_shell_sidebar_set_secondary_text (shell_sidebar, NULL);
		e_shell_view_set_title (shell_view, label);
		g_free (label);

		return;
	}

	folder_name = camel_folder_get_name (folder);
	parent_store = camel_folder_get_parent_store (folder);

	num_deleted = folder->summary->deleted_count;
	num_junked = folder->summary->junk_count;
	num_junked_not_deleted = folder->summary->junk_not_deleted_count;
	num_unread = folder->summary->unread_count;
	num_visible = folder->summary->visible_count;

	buffer = g_string_sized_new (256);
	uids = e_mail_reader_get_selected_uids (reader);

	if (uids->len > 1)
		g_string_append_printf (
			buffer, ngettext ("%d selected, ", "%d selected, ",
			uids->len), uids->len);

	if (CAMEL_IS_VTRASH_FOLDER (folder)) {
		CamelVTrashFolder *trash_folder;

		trash_folder = (CamelVTrashFolder *) folder;

		/* "Trash" folder */
		if (trash_folder->type == CAMEL_VTRASH_FOLDER_TRASH)
			g_string_append_printf (
				buffer, ngettext ("%d deleted",
				"%d deleted", num_deleted), num_deleted);

		/* "Junk" folder (hide deleted messages) */
		else if (e_mail_reader_get_hide_deleted (reader))
			g_string_append_printf (
				buffer, ngettext ("%d junk",
				"%d junk", num_junked_not_deleted),
				num_junked_not_deleted);

		/* "Junk" folder (show deleted messages) */
		else
			g_string_append_printf (
				buffer, ngettext ("%d junk", "%d junk",
				num_junked), num_junked);

	/* "Drafts" folder */
	} else if (em_utils_folder_is_drafts (folder, folder_uri)) {
		g_string_append_printf (
			buffer, ngettext ("%d draft", "%d drafts",
			num_visible), num_visible);

	/* "Outbox" folder */
	} else if (em_utils_folder_is_outbox (folder, folder_uri)) {
		g_string_append_printf (
			buffer, ngettext ("%d unsent", "%d unsent",
			num_visible), num_visible);

	/* "Sent" folder */
	} else if (em_utils_folder_is_sent (folder, folder_uri)) {
		g_string_append_printf (
			buffer, ngettext ("%d sent", "%d sent",
			num_visible), num_visible);

	/* Normal folder */
	} else {
		if (!e_mail_reader_get_hide_deleted (reader))
			num_visible +=
				num_deleted - num_junked +
				num_junked_not_deleted;

		if (num_unread > 0 && uids->len <= 1)
			g_string_append_printf (
				buffer, ngettext ("%d unread, ",
				"%d unread, ", num_unread), num_unread);
		g_string_append_printf (
			buffer, ngettext ("%d total", "%d total",
			num_visible), num_visible);
	}

	em_utils_uids_free (uids);

	/* Choose a suitable folder name for displaying. */
	if (parent_store == local_store && (
		strcmp (folder_name, "Drafts") == 0 ||
		strcmp (folder_name, "Inbox") == 0 ||
		strcmp (folder_name, "Outbox") == 0 ||
		strcmp (folder_name, "Sent") == 0 ||
		strcmp (folder_name, "Templates") == 0))
		display_name = _(folder_name);
	else if (strcmp (folder_name, "INBOX") == 0)
		display_name = _("Inbox");
	else
		display_name = folder_name;

	title = g_strdup_printf ("%s (%s)", display_name, buffer->str);
	e_shell_sidebar_set_secondary_text (shell_sidebar, buffer->str);
	e_shell_view_set_title (shell_view, title);
	g_free (title);

	g_string_free (buffer, TRUE);
}
