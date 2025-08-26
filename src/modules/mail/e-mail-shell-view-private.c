/*
 * e-mail-shell-view-private.c
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

#include "e-mail-shell-view-private.h"

#include "e-util/e-util-private.h"

typedef struct _AsyncContext AsyncContext;

struct _AsyncContext {
	EActivity *activity;
	EMailReader *reader;
	EShellView *shell_view;
};

static void
async_context_free (AsyncContext *context)
{
	if (context->activity != NULL)
		g_object_unref (context->activity);

	if (context->reader != NULL)
		g_object_unref (context->reader);

	if (context->shell_view != NULL)
		g_object_unref (context->shell_view);

	g_slice_free (AsyncContext, context);
}

static void
mail_shell_view_got_folder_cb (CamelStore *store,
                               GAsyncResult *result,
                               AsyncContext *context)
{
	EAlertSink *alert_sink;
	CamelFolder *folder;
	GtkWidget *message_list;
	GError *error = NULL;

	alert_sink = e_activity_get_alert_sink (context->activity);

	folder = camel_store_get_folder_finish (store, result, &error);

	if (e_activity_handle_cancellation (context->activity, error)) {
		g_warn_if_fail (folder == NULL);
		async_context_free (context);
		g_error_free (error);
		return;

	} else if (error != NULL) {
		g_warn_if_fail (folder == NULL);
		e_alert_submit (
			alert_sink, "mail:folder-open",
			error->message, NULL);
		async_context_free (context);
		g_error_free (error);
		return;
	}

	message_list = e_mail_reader_get_message_list (context->reader);
	message_list_freeze (MESSAGE_LIST (message_list));

	e_mail_reader_set_folder (context->reader, folder);
	e_mail_shell_view_restore_state (E_MAIL_SHELL_VIEW (context->shell_view));
	e_shell_view_execute_search (context->shell_view);

	message_list_thaw (MESSAGE_LIST (message_list));

	e_shell_view_update_actions_in_idle (context->shell_view);

	g_object_unref (folder);

	async_context_free (context);
}

static void
mail_shell_view_folder_tree_selected_cb (EMailShellView *mail_shell_view,
                                         CamelStore *store,
                                         const gchar *folder_name,
                                         CamelFolderInfoFlags flags,
                                         EMFolderTree *folder_tree)
{
	EMailShellContent *mail_shell_content;
	EShellView *shell_view;
	EMailReader *reader;
	EMailView *mail_view;
	GCancellable *cancellable;
	AsyncContext *context;
	EActivity *activity;

	shell_view = E_SHELL_VIEW (mail_shell_view);

	mail_shell_content = mail_shell_view->priv->mail_shell_content;
	mail_view = e_mail_shell_content_get_mail_view (mail_shell_content);

	reader = E_MAIL_READER (mail_view);

	/* Cancel any unfinished open folder operations. */
	if (mail_shell_view->priv->opening_folder != NULL) {
		g_cancellable_cancel (mail_shell_view->priv->opening_folder);
		g_object_unref (mail_shell_view->priv->opening_folder);
		mail_shell_view->priv->opening_folder = NULL;
	}

	/* If we are to clear the message list, do so immediately. */
	if ((flags & CAMEL_FOLDER_NOSELECT) || folder_name == NULL) {
		e_mail_reader_set_folder (reader, NULL);
		e_shell_view_update_actions_in_idle (shell_view);
		return;
	}

	g_warn_if_fail (CAMEL_IS_STORE (store));

	/* Open the selected folder asynchronously. */

	activity = e_mail_reader_new_activity (reader);
	cancellable = e_activity_get_cancellable (activity);
	mail_shell_view->priv->opening_folder = g_object_ref (cancellable);

	context = g_slice_new0 (AsyncContext);
	context->activity = activity;
	context->reader = g_object_ref (reader);
	context->shell_view = g_object_ref (shell_view);

	camel_store_get_folder (
		store, folder_name, 0, G_PRIORITY_DEFAULT, cancellable,
		(GAsyncReadyCallback) mail_shell_view_got_folder_cb, context);
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
		case GDK_KEY_period:
		case GDK_KEY_comma:
		case GDK_KEY_bracketleft:
		case GDK_KEY_bracketright:
			goto emit;

		default:
			goto exit;
	}

ctrl:
	/* Ctrl + <keyval> */
	switch (event->keyval) {
		case GDK_KEY_period:
		case GDK_KEY_comma:
			goto emit;

		default:
			goto exit;
	}

emit:
	/* Forward the key press to the EMailReader interface. */
	g_signal_emit_by_name (mail_view, "key-press-event", event, &handled);

exit:
	return handled;
}

static void
mail_shell_view_match_folder_tree_and_message_list_folder (EMailShellView *mail_shell_view)
{
	EMailShellContent *mail_shell_content;
	EMailShellSidebar *mail_shell_sidebar;
	EMFolderTree *folder_tree;
	GtkWidget *message_list;
	EMailReader *reader;
	EMailView *mail_view;
	CamelFolder *folder;
	gchar *list_uri = NULL;
	gchar *tree_uri;

	mail_shell_content = mail_shell_view->priv->mail_shell_content;
	mail_view = e_mail_shell_content_get_mail_view (mail_shell_content);

	mail_shell_sidebar = mail_shell_view->priv->mail_shell_sidebar;
	folder_tree = e_mail_shell_sidebar_get_folder_tree (mail_shell_sidebar);

	reader = E_MAIL_READER (mail_view);
	message_list = e_mail_reader_get_message_list (reader);

	/* Don't use e_mail_reader_ref_folder() here.  The fact that the
	 * method gets the folder from the message list is supposed to be
	 * a hidden implementation detail, and we want to explicitly get
	 * the folder URI from the message list here. */
	folder = message_list_ref_folder (MESSAGE_LIST (message_list));
	if (folder != NULL) {
		list_uri = e_mail_folder_uri_from_folder (folder);
		g_object_unref (folder);
	}

	tree_uri = em_folder_tree_get_selected_uri (folder_tree);

	/* If the folder tree and message list disagree on the current
	 * folder, reset the folder tree to match the message list. */
	if (list_uri != NULL && g_strcmp0 (tree_uri, list_uri) != 0)
		em_folder_tree_set_selected (folder_tree, list_uri, FALSE);

	g_free (list_uri);
	g_free (tree_uri);
}

static void
mail_shell_view_folder_tree_selection_done_cb (EMailShellView *mail_shell_view,
                                               GtkWidget *menu)
{
	if (!mail_shell_view->priv->ignore_folder_popup_selection_done)
		mail_shell_view_match_folder_tree_and_message_list_folder (mail_shell_view);

	mail_shell_view->priv->ignore_folder_popup_selection_done = FALSE;

	/* Disconnect from the "selection-done" signal. */
	g_signal_handlers_disconnect_by_func (
		menu, mail_shell_view_folder_tree_selection_done_cb,
		mail_shell_view);
}

static void
mail_shell_view_folder_tree_popup_event_cb (EShellView *shell_view,
                                            GdkEvent *button_event)
{
	EMailShellView *mail_shell_view;
	GtkWidget *menu;

	mail_shell_view = E_MAIL_SHELL_VIEW (shell_view);
	mail_shell_view->priv->ignore_folder_popup_selection_done = FALSE;

	menu = e_shell_view_show_popup_menu (shell_view, "mail-folder-popup", button_event);

	g_signal_connect_object (
		menu, "selection-done",
		G_CALLBACK (mail_shell_view_folder_tree_selection_done_cb),
		shell_view, G_CONNECT_SWAPPED);
}

static gboolean
mail_shell_view_message_list_popup_menu_cb (EShellView *shell_view)
{
	e_shell_view_show_popup_menu (shell_view, "mail-message-popup", NULL);

	return TRUE;
}

static gboolean
mail_shell_view_message_list_right_click_cb (EShellView *shell_view,
                                             gint row,
                                             ETreePath path,
                                             gint col,
                                             GdkEvent *button_event)
{
	e_shell_view_show_popup_menu (shell_view, "mail-message-popup", button_event);

	return TRUE;
}

static gboolean
mail_shell_view_popup_event_cb (EMailShellView *mail_shell_view,
                                const gchar *uri,
                                GdkEvent *event)
{
	EMailShellContent *mail_shell_content;
	EMailDisplay *display;
	EShellView *shell_view;
	EMailReader *reader;
	EMailView *mail_view;
	GtkMenu *menu;

	if (uri != NULL)
		return FALSE;

	mail_shell_content = mail_shell_view->priv->mail_shell_content;
	mail_view = e_mail_shell_content_get_mail_view (mail_shell_content);

	reader = E_MAIL_READER (mail_view);
	display = e_mail_reader_get_mail_display (reader);

	if (e_web_view_get_cursor_image_src (E_WEB_VIEW (display)) != NULL)
		return FALSE;

	shell_view = E_SHELL_VIEW (mail_shell_view);
	e_shell_view_update_actions (shell_view);

	menu = e_mail_reader_get_popup_menu (reader);
	gtk_menu_popup_at_pointer (menu, event);

	return TRUE;
}

static void
mail_shell_view_reader_changed_cb (EMailShellView *mail_shell_view,
                                   EMailReader *reader)
{
	GtkWidget *message_list;
	EMailDisplay *display;
	EShellView *shell_view;
	EShellTaskbar *shell_taskbar;

	shell_view = E_SHELL_VIEW (mail_shell_view);
	shell_taskbar = e_shell_view_get_shell_taskbar (shell_view);

	display = e_mail_reader_get_mail_display (reader);
	message_list = e_mail_reader_get_message_list (reader);

	e_shell_view_update_actions_in_idle (E_SHELL_VIEW (mail_shell_view));
	e_mail_shell_view_update_sidebar (mail_shell_view);

	/* Connect if its not connected already */
	if (g_signal_handler_find (message_list, G_SIGNAL_MATCH_FUNC, 0, 0, NULL, mail_shell_view_message_list_popup_menu_cb, NULL))
		return;

	g_signal_connect_object (
		message_list, "popup-menu",
		G_CALLBACK (mail_shell_view_message_list_popup_menu_cb),
		mail_shell_view, G_CONNECT_SWAPPED);

	g_signal_connect_object (
		message_list, "right-click",
		G_CALLBACK (mail_shell_view_message_list_right_click_cb),
		mail_shell_view, G_CONNECT_SWAPPED);

	g_signal_connect_object (
		display, "popup-event",
		G_CALLBACK (mail_shell_view_popup_event_cb),
		mail_shell_view, G_CONNECT_SWAPPED);

	g_signal_connect_object (
		display, "status-message",
		G_CALLBACK (e_shell_taskbar_set_message),
		shell_taskbar, G_CONNECT_SWAPPED);
}

static void
mail_shell_view_prepare_for_quit_cb (EMailShellView *mail_shell_view,
                                     EActivity *activity)
{
	EMailShellContent *mail_shell_content;
	EMailReader *reader;
	EMailView *mail_view;
	GtkWidget *message_list;

	/* If we got here, it means the application is shutting down
	 * and this is the last EMailShellView instance.  Synchronize
	 * the currently selected folder before we terminate. */

	mail_shell_content = mail_shell_view->priv->mail_shell_content;
	mail_view = e_mail_shell_content_get_mail_view (mail_shell_content);

	reader = E_MAIL_READER (mail_view);
	message_list = e_mail_reader_get_message_list (reader);
	message_list_save_state (MESSAGE_LIST (message_list));

	/* Do not sync folder content here, it's duty of EMailBackend,
	 * which does it for all accounts */
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

static void
mail_shell_view_search_filter_changed_cb (EMailShellView *mail_shell_view)
{
	EMailShellContent *mail_shell_content;
	EMailView *mail_view;

	g_return_if_fail (mail_shell_view != NULL);
	g_return_if_fail (mail_shell_view->priv != NULL);

	if (e_shell_view_is_execute_search_blocked (E_SHELL_VIEW (mail_shell_view)))
		return;

	mail_shell_content = mail_shell_view->priv->mail_shell_content;
	mail_view = e_mail_shell_content_get_mail_view (mail_shell_content);

	e_mail_reader_avoid_next_mark_as_seen (E_MAIL_READER (mail_view));
}

static void
e_mail_shell_view_mail_view_notify_cb (GObject *object,
				       GParamSpec *param,
				       gpointer user_data)
{
	GAction *action = G_ACTION (object);
	EMailShellView *mail_shell_view = user_data;
	EMailShellContent *mail_shell_content;
	GtkOrientation orientation;
	EMailView *mail_view;
	GVariant *state;

	mail_shell_content = mail_shell_view->priv->mail_shell_content;
	mail_view = e_mail_shell_content_get_mail_view (mail_shell_content);
	state = g_action_get_state (G_ACTION (action));

	switch (g_variant_get_int32 (state)) {
		case 0:
			orientation = GTK_ORIENTATION_VERTICAL;
			break;
		case 1:
			orientation = GTK_ORIENTATION_HORIZONTAL;
			break;
		default:
			g_return_if_reached ();
	}

	e_mail_view_set_orientation (mail_view, orientation);
	g_clear_pointer (&state, g_variant_unref);
}

void
e_mail_shell_view_private_init (EMailShellView *mail_shell_view)
{
	e_signal_connect_notify (
		mail_shell_view, "notify::view-id",
		G_CALLBACK (mail_shell_view_notify_view_id_cb), NULL);

	mail_shell_view->priv->send_receive_menu = g_menu_new ();
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
	EShellSidebar *shell_sidebar;
	EShellTaskbar *shell_taskbar;
	EShellWindow *shell_window;
	EShellSearchbar *searchbar;
	EMFolderTree *folder_tree;
	EActionComboBox *combo_box;
	ERuleContext *context;
	EFilterRule *rule = NULL;
	EUIAction *action;
	EUIManager *ui_manager;
	GtkTreeSelection *selection;
	GtkWidget *message_list;
	GSettings *settings;
	EMailLabelListStore *label_store;
	EMailBackend *backend;
	EMailSession *session;
	EMailReader *reader;
	EMailView *mail_view;
	EMailDisplay *display;
	const gchar *source;
	gint ii = 0;

	shell_view = E_SHELL_VIEW (mail_shell_view);
	shell_backend = e_shell_view_get_shell_backend (shell_view);
	shell_content = e_shell_view_get_shell_content (shell_view);
	shell_sidebar = e_shell_view_get_shell_sidebar (shell_view);
	shell_taskbar = e_shell_view_get_shell_taskbar (shell_view);
	shell_window = e_shell_view_get_shell_window (shell_view);

	shell = e_shell_window_get_shell (shell_window);
	ui_manager = e_shell_view_get_ui_manager (shell_view);

	e_ui_manager_freeze (ui_manager);

	backend = E_MAIL_BACKEND (shell_backend);
	session = e_mail_backend_get_session (backend);
	label_store = e_mail_ui_session_get_label_store (
		E_MAIL_UI_SESSION (session));

	/* Cache these to avoid lots of awkward casting. */
	priv->mail_shell_backend = E_MAIL_SHELL_BACKEND (g_object_ref (shell_backend));
	priv->mail_shell_sidebar = E_MAIL_SHELL_SIDEBAR (g_object_ref (shell_sidebar));

	/* this is set from the EMaiLShellView::contructed() method, because it's needed in the init_ui_data(),
	   to have available actions from the EMailReader, which is called from the EShellView::contructed(),
	   thus before this private_constructed() function  */
	g_warn_if_fail (priv->mail_shell_content != NULL);

	mail_shell_sidebar = E_MAIL_SHELL_SIDEBAR (shell_sidebar);
	folder_tree = e_mail_shell_sidebar_get_folder_tree (mail_shell_sidebar);
	selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (folder_tree));

	mail_shell_content = E_MAIL_SHELL_CONTENT (shell_content);
	mail_view = e_mail_shell_content_get_mail_view (mail_shell_content);
	searchbar = e_mail_shell_content_get_searchbar (mail_shell_content);

	reader = E_MAIL_READER (mail_view);
	display = e_mail_reader_get_mail_display (reader);
	message_list = e_mail_reader_get_message_list (reader);

	em_folder_tree_set_selectable_widget (folder_tree, message_list);

	combo_box = e_shell_searchbar_get_filter_combo_box (searchbar);
	g_signal_connect_object (
		combo_box, "changed",
		G_CALLBACK (mail_shell_view_search_filter_changed_cb),
		mail_shell_view, G_CONNECT_SWAPPED);

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
		label_store, "changed",
		G_CALLBACK (e_mail_shell_view_update_search_filter),
		mail_shell_view, G_CONNECT_SWAPPED);

	g_signal_connect_object (
		display, "popup-event",
		G_CALLBACK (mail_shell_view_popup_event_cb),
		mail_shell_view, G_CONNECT_SWAPPED);

	g_signal_connect_object (
		display, "status-message",
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

	/* Advanced Search Action */
	action = ACTION (MAIL_SEARCH_ADVANCED_HIDDEN);
	e_ui_action_set_visible (action, FALSE);
	searchbar = e_mail_shell_content_get_searchbar (mail_shell_view->priv->mail_shell_content);
	e_shell_searchbar_set_search_option (searchbar, action);

	e_mail_shell_view_update_search_filter (mail_shell_view);

	/* Bind GObject properties for GSettings keys. */

	settings = e_util_ref_settings ("org.gnome.evolution.mail");

	g_settings_bind (
		settings, "show-deleted",
		ACTION (MAIL_SHOW_DELETED), "active",
		G_SETTINGS_BIND_DEFAULT | G_SETTINGS_BIND_NO_SENSITIVITY);

	g_settings_bind (
		settings, "show-junk",
		ACTION (MAIL_SHOW_JUNK), "active",
		G_SETTINGS_BIND_DEFAULT | G_SETTINGS_BIND_NO_SENSITIVITY);

	g_settings_bind (
		settings, "show-preview-toolbar",
		ACTION (MAIL_SHOW_PREVIEW_TOOLBAR), "active",
		G_SETTINGS_BIND_DEFAULT | G_SETTINGS_BIND_NO_SENSITIVITY);

	/* use the "classic" action, because it's the first in the group and
	   the group is not set yet, due to the UI manager being frozen */
	action = ACTION (MAIL_VIEW_CLASSIC);

	g_settings_bind_with_mapping (
		settings, "layout",
		action, "state",
		G_SETTINGS_BIND_DEFAULT | G_SETTINGS_BIND_NO_SENSITIVITY,
		e_shell_view_util_layout_to_state_cb,
		e_shell_view_util_state_to_layout_cb, NULL, NULL);

	g_signal_connect_object (action, "notify::state",
		G_CALLBACK (e_mail_shell_view_mail_view_notify_cb), mail_shell_view, 0);

	/* to propagate the loaded state */
	e_mail_shell_view_mail_view_notify_cb (G_OBJECT (action), NULL, mail_shell_view);

	g_settings_bind (
		settings, "show-attachment-bar",
		ACTION (MAIL_ATTACHMENT_BAR), "active",
		G_SETTINGS_BIND_DEFAULT | G_SETTINGS_BIND_NO_SENSITIVITY);

	if (e_shell_window_is_main_instance (shell_window)) {
		g_settings_bind (
			settings, "show-to-do-bar",
			ACTION (MAIL_TO_DO_BAR), "active",
			G_SETTINGS_BIND_DEFAULT | G_SETTINGS_BIND_NO_SENSITIVITY);
	} else {
		g_settings_bind (
			settings, "show-to-do-bar-sub",
			ACTION (MAIL_TO_DO_BAR), "active",
			G_SETTINGS_BIND_DEFAULT | G_SETTINGS_BIND_NO_SENSITIVITY);
	}

	g_settings_bind (
		settings, "vfolder-allow-expunge",
		mail_shell_view, "vfolder-allow-expunge",
		G_SETTINGS_BIND_GET | G_SETTINGS_BIND_NO_SENSITIVITY);

	g_clear_object (&settings);

	/* Populate built-in rules for search entry popup menu.
	 * Keep the assertions, please.  If the conditions aren't
	 * met we're going to crash anyway, just more mysteriously. */
	context = E_SHELL_VIEW_GET_CLASS (shell_view)->search_context;
	source = E_FILTER_SOURCE_DEMAND;
	while ((rule = e_rule_context_next_rule (context, rule, source))) {
		if (!rule->system)
			continue;
		g_return_if_fail (ii < MAIL_NUM_SEARCH_RULES);
		priv->search_rules[ii++] = g_object_ref (rule);
	}
	g_return_if_fail (ii == MAIL_NUM_SEARCH_RULES);

	/* Now that we're all set up, simulate selecting a folder. */
	g_signal_emit_by_name (selection, "changed");

	e_ui_manager_thaw (ui_manager);
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

	g_clear_object (&priv->mail_shell_backend);
	g_clear_object (&priv->mail_shell_content);
	g_clear_object (&priv->mail_shell_sidebar);
	g_clear_object (&priv->send_receive_menu);

	for (ii = 0; ii < MAIL_NUM_SEARCH_RULES; ii++)
		g_clear_object (&priv->search_rules[ii]);

	if (priv->opening_folder != NULL) {
		g_cancellable_cancel (priv->opening_folder);
		g_clear_object (&priv->opening_folder);
	}

	g_clear_object (&priv->search_folder_and_subfolders);
	g_clear_object (&priv->search_account_all);
	g_clear_object (&priv->search_account_current);
	g_clear_object (&priv->search_account_cancel);

	g_slist_free_full (priv->selected_uids, (GDestroyNotify) camel_pstring_free);
	priv->selected_uids = NULL;
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
	const gchar *old_state_group, *new_state_group;
	gchar *folder_uri;
	gchar *tmp = NULL;
	GSettings *settings;
	GtkWidget *message_list;

	/* XXX Move this to EMailShellContent. */

	g_return_if_fail (E_IS_MAIL_SHELL_VIEW (mail_shell_view));

	mail_shell_content = mail_shell_view->priv->mail_shell_content;
	mail_view = e_mail_shell_content_get_mail_view (mail_shell_content);
	searchbar = e_mail_shell_content_get_searchbar (mail_shell_content);

	reader = E_MAIL_READER (mail_view);
	folder = e_mail_reader_ref_folder (reader);

	if (folder == NULL) {
		if (e_shell_searchbar_get_state_group (searchbar)) {
			e_shell_searchbar_set_state_group (searchbar, NULL);
			e_shell_searchbar_load_state (searchbar);
		}
		return;
	}

	/* Do not restore state if we're running a "Current Account"
	 * or "All Accounts" search, since we don't want the search
	 * criteria to be destroyed in those cases. */

	vee_folder = mail_shell_view->priv->search_account_all;
	if (vee_folder != NULL && folder == CAMEL_FOLDER (vee_folder))
		goto exit;

	vee_folder = mail_shell_view->priv->search_account_current;
	if (vee_folder != NULL && folder == CAMEL_FOLDER (vee_folder))
		goto exit;

	vee_folder = mail_shell_view->priv->search_folder_and_subfolders;
	if (vee_folder != NULL && folder == CAMEL_FOLDER (vee_folder))
		goto exit;

	settings = e_util_ref_settings ("org.gnome.evolution.mail");
	if (g_settings_get_boolean (settings, "global-view-setting") &&
	    g_settings_get_boolean (settings, "global-view-search")) {
		    new_state_group = "GlobalSearch";
	} else {
		folder_uri = e_mail_folder_uri_from_folder (folder);
		tmp = g_strdup_printf ("Folder %s", folder_uri);
		new_state_group = tmp;
		g_free (folder_uri);
	}

	old_state_group = e_shell_searchbar_get_state_group (searchbar);
	message_list = e_mail_reader_get_message_list (reader);

	/* Avoid loading search state unnecessarily. */
	if ((!tmp && IS_MESSAGE_LIST (message_list) && MESSAGE_LIST (message_list)->just_set_folder) ||
	    g_strcmp0 (new_state_group, old_state_group) != 0) {
		e_shell_view_block_execute_search (E_SHELL_VIEW (mail_shell_view));

		e_shell_searchbar_set_state_group (searchbar, new_state_group);
		e_shell_searchbar_load_state (searchbar);

		e_shell_view_unblock_execute_search (E_SHELL_VIEW (mail_shell_view));
	}

	g_free (tmp);

exit:
	g_clear_object (&folder);
}

void
e_mail_shell_view_update_sidebar (EMailShellView *mail_shell_view)
{
	EMailShellContent *mail_shell_content;
	EShellBackend *shell_backend;
	EShellSidebar *shell_sidebar;
	EShellView *shell_view;
	EShell *shell;
	EMailReader *reader;
	EMailView *mail_view;
	ESourceRegistry *registry;
	CamelStore *parent_store;
	CamelFolder *folder;
	CamelFolderInfoFlags flags = 0;
	CamelFolderSummary *folder_summary;
	MailFolderCache *folder_cache;
	MessageList *message_list;
	guint selected_count;
	GString *buffer, *title_short = NULL;
	gboolean store_is_local, is_inbox;
	const gchar *display_name;
	const gchar *folder_name;
	const gchar *uid;
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
	shell_backend = e_shell_view_get_shell_backend (shell_view);
	shell_sidebar = e_shell_view_get_shell_sidebar (shell_view);

	shell = e_shell_backend_get_shell (shell_backend);
	registry = e_shell_get_registry (shell);

	reader = E_MAIL_READER (mail_view);
	folder = e_mail_reader_ref_folder (reader);

	/* If no folder is selected, reset the sidebar banners
	 * to their default values and stop. */
	if (folder == NULL) {
		EUIAction *action;

		action = e_shell_view_get_switcher_action (shell_view);

		e_shell_sidebar_set_secondary_text (shell_sidebar, NULL);
		e_shell_view_set_title (shell_view, e_ui_action_get_label (action));

		return;
	}

	folder_name = camel_folder_get_display_name (folder);
	parent_store = camel_folder_get_parent_store (folder);
	folder_summary = camel_folder_get_folder_summary (folder);

	folder_cache = e_mail_session_get_folder_cache (
		e_mail_backend_get_session (E_MAIL_BACKEND (shell_backend)));
	mail_folder_cache_get_folder_info_flags (folder_cache, parent_store, folder_name, &flags);
	is_inbox = (flags & CAMEL_FOLDER_TYPE_MASK) == CAMEL_FOLDER_TYPE_INBOX;

	num_deleted = camel_folder_summary_get_deleted_count (folder_summary);
	num_junked = camel_folder_summary_get_junk_count (folder_summary);
	num_junked_not_deleted = camel_folder_summary_get_junk_not_deleted_count (folder_summary);
	num_unread = camel_folder_summary_get_unread_count (folder_summary);
	num_visible = camel_folder_summary_get_visible_count (folder_summary);

	buffer = g_string_sized_new (256);
	message_list = MESSAGE_LIST (e_mail_reader_get_message_list (reader));
	selected_count = message_list_selected_count (message_list);

	if (selected_count > 1)
		g_string_append_printf (
			buffer, ngettext ("%d selected, ", "%d selected, ",
			selected_count), selected_count);

	/* "Trash" folder (virtual or real) */
	if (camel_folder_get_flags (folder) & CAMEL_FOLDER_IS_TRASH) {
		if (num_unread > 0 && selected_count <= 1) {
			g_string_append_printf (
				buffer, ngettext ("%d unread, ",
				"%d unread, ", num_unread), num_unread);
		}

		if (CAMEL_IS_VTRASH_FOLDER (folder)) {
			/* For a virtual Trash folder, count
			 * the messages marked for deletion. */
			g_string_append_printf (
				buffer, ngettext ("%d deleted",
				"%d deleted", num_deleted), num_deleted);
		} else {
			/* For a regular Trash folder, just
			 * count the visible messages.
			 *
			 * XXX Open question: what to do about messages
			 *     marked for deletion in Trash?  Probably
			 *     this is the wrong question to be asking
			 *     anyway.  Deleting a message in a real
			 *     Trash should permanently expunge the
			 *     message (if the server supports that),
			 *     which would eliminate this corner case. */
			if (!e_mail_reader_get_hide_deleted (reader))
				num_visible += num_deleted;

			g_string_append_printf (
				buffer, ngettext ("%d deleted",
				"%d deleted", num_visible), num_visible);
		}
	/* "Junk" folder (virtual or real) */
	} else if (camel_folder_get_flags (folder) & CAMEL_FOLDER_IS_JUNK) {
		if (num_unread > 0 && selected_count <= 1) {
			g_string_append_printf (
				buffer, ngettext ("%d unread, ",
				"%d unread, ", num_unread), num_unread);
		}

		if (e_mail_reader_get_hide_deleted (reader)) {
			/* Junk folder with deleted messages hidden. */
			g_string_append_printf (
				buffer, ngettext ("%d junk",
				"%d junk", num_junked_not_deleted),
				num_junked_not_deleted);
		} else {
			/* Junk folder with deleted messages visible. */
			g_string_append_printf (
				buffer, ngettext ("%d junk", "%d junk",
				num_junked), num_junked);
		}

	/* "Drafts" folder */
	} else if (!is_inbox && em_utils_folder_is_drafts (registry, folder)) {
		g_string_append_printf (
			buffer, ngettext ("%d draft", "%d drafts",
			num_visible), num_visible);

	/* "Outbox" folder */
	} else if (!is_inbox && em_utils_folder_is_outbox (registry, folder)) {
		g_string_append_printf (
			buffer, ngettext ("%d unsent", "%d unsent",
			num_visible), num_visible);

	/* "Sent" folder */
	} else if (!is_inbox && em_utils_folder_is_sent (registry, folder)) {
		g_string_append_printf (
			buffer, ngettext ("%d sent", "%d sent",
			num_visible), num_visible);

	/* Normal folder */
	} else {
		if (!e_mail_reader_get_hide_deleted (reader))
			num_visible +=
				num_deleted - num_junked +
				num_junked_not_deleted;

		if (num_unread > 0 && selected_count <= 1) {
			g_string_append_printf (
				buffer, ngettext ("%d unread, ",
				"%d unread, ", num_unread), num_unread);

			title_short = g_string_sized_new (64);
			g_string_append_printf (
				title_short, ngettext ("%d unread",
				"%d unread", num_unread), num_unread);
		}

		g_string_append_printf (
			buffer, ngettext ("%d total", "%d total",
			num_visible), num_visible);
	}

	uid = camel_service_get_uid (CAMEL_SERVICE (parent_store));
	store_is_local = (g_strcmp0 (uid, E_MAIL_SESSION_LOCAL_UID) == 0);

	/* Choose a suitable folder name for displaying. */
	display_name = folder_name;
	if (store_is_local) {
		if (strcmp (folder_name, "Drafts") == 0)
			display_name = _("Drafts");
		else if (strcmp (folder_name, "Inbox") == 0)
			display_name = _("Inbox");
		else if (strcmp (folder_name, "Outbox") == 0)
			display_name = _("Outbox");
		else if (strcmp (folder_name, "Sent") == 0)
			display_name = _("Sent");
		else if (strcmp (folder_name, "Templates") == 0)
			display_name = _("Templates");
		else if (strcmp (folder_name, "Trash") == 0)
			display_name = _("Trash");
	}
	if (strcmp (folder_name, "INBOX") == 0)
		display_name = _("Inbox");

	if (title_short && title_short->len > 0)
		title = g_strdup_printf ("%s (%s)", display_name, title_short->str);
	else
		title = g_strdup (display_name);
	e_shell_sidebar_set_secondary_text (shell_sidebar, buffer->str);
	e_shell_view_set_title (shell_view, title);
	g_free (title);

	g_string_free (buffer, TRUE);
	if (title_short)
		g_string_free (title_short, TRUE);

	g_clear_object (&folder);
}

typedef struct {
	GMenu *section;
	CamelSession *session;
	EMailAccountStore *account_store;
	EUIManager *ui_manager;

	/* Signal handlers */
	gulong service_added_id;
	gulong service_removed_id;
	gulong service_enabled_id;
	gulong service_disabled_id;
} SendReceiveData;

static gboolean
send_receive_can_use_service (EMailAccountStore *account_store,
                              CamelService *service,
                              GtkTreeIter *piter)
{
	GtkTreeModel *model;
	GtkTreeIter iter;
	gboolean found = FALSE, enabled = FALSE, builtin = TRUE;

	if (!CAMEL_IS_STORE (service))
		return FALSE;

	model = GTK_TREE_MODEL (account_store);

	if (piter) {
		found = TRUE;
		iter = *piter;
	} else if (gtk_tree_model_get_iter_first (model, &iter)) {
		CamelService *adept;

		do {
			adept = NULL;

			gtk_tree_model_get (
				model, &iter,
				E_MAIL_ACCOUNT_STORE_COLUMN_SERVICE, &adept,
				-1);

			if (service == adept) {
				found = TRUE;
				g_object_unref (adept);
				break;
			}

			if (adept)
				g_object_unref (adept);
		} while (gtk_tree_model_iter_next (model, &iter));
	}

	if (!found)
		return FALSE;

	gtk_tree_model_get (
		model, &iter,
		E_MAIL_ACCOUNT_STORE_COLUMN_ENABLED, &enabled,
		E_MAIL_ACCOUNT_STORE_COLUMN_BUILTIN, &builtin,
		-1);

	return enabled && !builtin;
}

static gint
send_receive_find_menu_index (SendReceiveData *data,
			      CamelService *service)
{
	GMenuModel *menu_model;
	const gchar *uid;
	const gchar *prefix = "mail-send-receive.mail-send-receive-service-";
	guint prefix_len = strlen (prefix);
	gint ii, n_items;

	menu_model = G_MENU_MODEL (data->section);
	n_items = g_menu_model_get_n_items (menu_model);
	uid = camel_service_get_uid (service);

	for (ii = 0; ii < n_items; ii++) {
		GVariant *attr;
		const gchar *action_name_with_uid;

		attr = g_menu_model_get_item_attribute_value (menu_model, ii, G_MENU_ATTRIBUTE_ACTION, G_VARIANT_TYPE_STRING);
		action_name_with_uid = attr ? g_variant_get_string (attr, NULL) : NULL;

		if (action_name_with_uid &&
		    g_str_has_prefix (action_name_with_uid, prefix) &&
		    g_strcmp0 (uid, action_name_with_uid + prefix_len) == 0) {
			g_clear_pointer (&attr, g_variant_unref);
			return ii;
		}

		g_clear_pointer (&attr, g_variant_unref);
	}

	return -1;
}

static void
send_receive_service_activated_cb (EUIAction *action,
				   GVariant *parameter,
				   gpointer user_data)
{
	CamelSession *session = user_data;
	CamelService *service;
	GVariant *state;

	state = g_action_get_state (G_ACTION (action));
	service = camel_session_ref_service (session, g_variant_get_string (state, NULL));
	g_clear_pointer (&state, g_variant_unref);

	g_return_if_fail (CAMEL_IS_SERVICE (service));

	mail_receive_service (service);

	g_clear_object (&service);
}

typedef struct _EMenuItemSensitivityData {
	EUIAction *action;
	gboolean is_online;
} EMenuItemSensitivityData;

static void
free_menu_item_sensitivity_data (gpointer ptr)
{
	EMenuItemSensitivityData *data = ptr;

	if (!data)
		return;

	g_object_unref (data->action);
	g_slice_free (EMenuItemSensitivityData, data);
}

static gboolean
update_menu_item_sensitivity_cb (gpointer user_data)
{
	EMenuItemSensitivityData *data = user_data;

	g_return_val_if_fail (data != NULL, FALSE);

	e_ui_action_set_sensitive (data->action, data->is_online);

	return FALSE;
}

static void
service_online_state_changed_cb (GObject *object,
				 GParamSpec *param,
				 gpointer user_data)
{
	EUIAction *action = user_data;
	EMenuItemSensitivityData *data;
	gboolean is_online = FALSE;

	g_return_if_fail (CAMEL_IS_SESSION (object) || CAMEL_IS_SERVICE (object));
	g_return_if_fail (E_IS_UI_ACTION (action));

	g_object_get (object, "online", &is_online, NULL);

	data = g_slice_new0 (EMenuItemSensitivityData);
	data->action = g_object_ref (action);
	data->is_online = is_online;

	g_idle_add_full (G_PRIORITY_HIGH_IDLE, update_menu_item_sensitivity_cb, data, free_menu_item_sensitivity_data);
}

static void
send_receive_add_to_menu (SendReceiveData *data,
                          CamelService *service,
                          gint position)
{
	CamelProvider *provider;
	EUIAction *action;
	GMenuItem *item;
	gchar *action_name;

	if (send_receive_find_menu_index (data, service) >= 0)
		return;

	provider = camel_service_get_provider (service);
	action_name = g_strconcat ("mail-send-receive-service-", camel_service_get_uid (service), NULL);

	action = e_ui_action_new ("mail-send-receive", action_name, NULL);
	e_ui_action_set_state (action, g_variant_new_string (camel_service_get_uid (service)));

	g_free (action_name);

	e_binding_bind_property (
		service, "display-name",
		action, "label",
		G_BINDING_SYNC_CREATE);

	g_signal_connect_object (action, "activate",
		G_CALLBACK (send_receive_service_activated_cb), data->session, 0);

	e_ui_manager_add_action (data->ui_manager, "mail-send-receive", action, NULL, NULL, NULL);

	item = g_menu_item_new (NULL, NULL);
	e_ui_manager_update_item_from_action (data->ui_manager, item, action);

	if (position < 0)
		g_menu_append_item (data->section, item);
	else
		g_menu_insert_item (data->section, position, item);

	g_clear_object (&item);

	if (provider && (provider->flags & CAMEL_PROVIDER_IS_REMOTE) != 0) {
		gpointer object;
		gboolean is_online = FALSE;

		if (CAMEL_IS_OFFLINE_STORE (service))
			object = g_object_ref (service);
		else
			object = camel_service_ref_session (service);

		g_object_get (object, "online", &is_online, NULL);

		e_signal_connect_notify_object (
			object, "notify::online",
			G_CALLBACK (service_online_state_changed_cb), action,
			0);

		e_ui_action_set_sensitive (action, is_online);

		g_object_unref (object);
	}

	g_clear_object (&action);
}

static gint
sort_services_cb (gconstpointer pservice1,
                  gconstpointer pservice2,
                  gpointer account_store)
{
	CamelService *service1 = *((CamelService **) pservice1);
	CamelService *service2 = *((CamelService **) pservice2);

	return e_mail_account_store_compare_services (account_store, service1, service2);
}

static void
send_receive_menu_service_added_cb (EMailAccountStore *account_store,
                                    CamelService *service,
                                    SendReceiveData *data)
{
	EUIActionGroup *action_group;
	GPtrArray *actions;
	GPtrArray *services;
	guint position = G_MAXUINT;

	if (!send_receive_can_use_service (account_store, service, NULL))
		return;

	action_group = e_ui_manager_get_action_group (data->ui_manager, "mail-send-receive");
	actions = e_ui_action_group_list_actions (action_group);

	services = g_ptr_array_new_full (1 + (actions ? actions->len : 0), g_object_unref);
	g_ptr_array_add (services, g_object_ref (service));

	if (actions) {
		guint ii;

		for (ii = 0; ii < actions->len; ii++) {
			EUIAction *action = g_ptr_array_index (actions, ii);
			GVariant *state;

			state = g_action_get_state (G_ACTION (action));
			if (state) {
				CamelService *action_service;

				action_service = camel_session_ref_service (data->session, g_variant_get_string (state, NULL));
				if (action_service)
					g_ptr_array_add (services, action_service);

				g_clear_pointer (&state, g_variant_unref);
			}
		}
	}

	g_ptr_array_sort_with_data (services, sort_services_cb, account_store);

	if (!g_ptr_array_find (services, service, &position))
		position = -1;

	send_receive_add_to_menu (data, service, position);

	g_clear_pointer (&actions, g_ptr_array_unref);
	g_clear_pointer (&services, g_ptr_array_unref);
}

static void
send_receive_menu_service_removed_cb (EMailAccountStore *account_store,
                                      CamelService *service,
                                      SendReceiveData *data)
{
	EUIActionGroup *action_group;
	gint index;

	action_group = e_ui_manager_get_action_group (data->ui_manager, "mail-send-receive");
	if (action_group) {
		gchar *action_name;

		action_name = g_strconcat ("mail-send-receive-service-", camel_service_get_uid (service), NULL);
		e_ui_action_group_remove_by_name (action_group, action_name);
		g_free (action_name);
	}

	index = send_receive_find_menu_index (data, service);
	if (index >= 0)
		g_menu_remove (data->section, index);
}

static void
send_receive_data_free (SendReceiveData *data)
{
	g_signal_handler_disconnect (data->account_store, data->service_added_id);
	g_signal_handler_disconnect (data->account_store, data->service_removed_id);
	g_signal_handler_disconnect (data->account_store, data->service_enabled_id);
	g_signal_handler_disconnect (data->account_store, data->service_disabled_id);

	g_clear_object (&data->session);
	g_clear_object (&data->account_store);
	g_clear_object (&data->ui_manager);

	g_slice_free (SendReceiveData, data);
}

static SendReceiveData *
send_receive_data_new (EMailShellView *mail_shell_view,
		       GMenu *section)
{
	SendReceiveData *data;
	EShellView *shell_view;
	EShellBackend *shell_backend;
	EMailAccountStore *account_store;
	EMailBackend *backend;
	EMailSession *session;

	shell_view = E_SHELL_VIEW (mail_shell_view);
	shell_backend = e_shell_view_get_shell_backend (shell_view);

	backend = E_MAIL_BACKEND (shell_backend);
	session = e_mail_backend_get_session (backend);
	account_store = e_mail_ui_session_get_account_store (E_MAIL_UI_SESSION (session));

	data = g_slice_new0 (SendReceiveData);
	data->section = section;  /* do not reference */
	data->session = CAMEL_SESSION (g_object_ref (session));
	data->account_store = g_object_ref (account_store);
	data->ui_manager = g_object_ref (e_shell_view_get_ui_manager (shell_view));

	data->service_added_id = g_signal_connect (
		account_store, "service-added",
		G_CALLBACK (send_receive_menu_service_added_cb), data);
	data->service_removed_id = g_signal_connect (
		account_store, "service-removed",
		G_CALLBACK (send_receive_menu_service_removed_cb), data);
	data->service_enabled_id = g_signal_connect (
		account_store, "service-enabled",
		G_CALLBACK (send_receive_menu_service_added_cb), data);
	data->service_disabled_id = g_signal_connect (
		account_store, "service-disabled",
		G_CALLBACK (send_receive_menu_service_removed_cb), data);

	g_object_weak_ref (G_OBJECT (shell_view), (GWeakNotify) send_receive_data_free, data);

	return data;
}

void
e_mail_shell_view_fill_send_receive_menu (EMailShellView *self)
{
	EShellView *shell_view;
	EShellBackend *shell_backend;
	EMailAccountStore *account_store;
	EMailBackend *backend;
	EMailSession *session;
	EUIManager *ui_manager;
	EUIAction *action;
	EUIActionGroup *action_group;
	GMenu *section;
	GMenuItem *item;
	GtkTreeModel *model;
	GtkTreeIter iter;
	SendReceiveData *data;

	g_return_if_fail (self != NULL);

	shell_view = E_SHELL_VIEW (self);
	shell_backend = e_shell_view_get_shell_backend (shell_view);
	ui_manager = e_shell_view_get_ui_manager (shell_view);

	backend = E_MAIL_BACKEND (shell_backend);
	session = e_mail_backend_get_session (backend);
	account_store = e_mail_ui_session_get_account_store (E_MAIL_UI_SESSION (session));
	action_group = e_ui_manager_get_action_group (ui_manager, "mail-send-receive");

	e_ui_manager_freeze (ui_manager);

	g_menu_remove_all (self->priv->send_receive_menu);
	e_ui_action_group_remove_all (action_group);

	section = g_menu_new ();

	action = ACTION (MAIL_SEND_RECEIVE);
	item = g_menu_item_new (NULL, NULL);
	e_ui_manager_update_item_from_action (ui_manager, item, action);
	g_menu_append_item (section, item);
	g_clear_object (&item);

	action = ACTION (MAIL_SEND_RECEIVE_RECEIVE_ALL);
	item = g_menu_item_new (NULL, NULL);
	e_ui_manager_update_item_from_action (ui_manager, item, action);
	g_menu_append_item (section, item);
	g_clear_object (&item);

	action = ACTION (MAIL_SEND_RECEIVE_SEND_ALL);
	item = g_menu_item_new (NULL, NULL);
	e_ui_manager_update_item_from_action (ui_manager, item, action);
	g_menu_append_item (section, item);
	g_clear_object (&item);

	g_menu_append_section (self->priv->send_receive_menu, NULL, G_MENU_MODEL (section));
	g_clear_object (&section);

	section = g_menu_new ();
	data = send_receive_data_new (self, section);

	model = GTK_TREE_MODEL (account_store);
	if (gtk_tree_model_get_iter_first (model, &iter)) {
		CamelService *service;

		do {
			service = NULL;

			gtk_tree_model_get (
				model, &iter,
				E_MAIL_ACCOUNT_STORE_COLUMN_SERVICE, &service,
				-1);

			if (send_receive_can_use_service (account_store, service, &iter))
				send_receive_add_to_menu (data, service, -1);

			if (service)
				g_object_unref (service);
		} while (gtk_tree_model_iter_next (model, &iter));
	}

	g_menu_append_section (self->priv->send_receive_menu, NULL, G_MENU_MODEL (section));
	g_clear_object (&section);

	e_ui_manager_thaw (ui_manager);
}

static void
mail_shell_view_folder_renamed_cb (EMFolderTree *folder_tree,
				   gboolean is_cancelled,
				   EMailShellView *mail_shell_view)
{
	g_return_if_fail (EM_IS_FOLDER_TREE (folder_tree));
	g_return_if_fail (E_IS_MAIL_SHELL_VIEW (mail_shell_view));

	mail_shell_view_match_folder_tree_and_message_list_folder (mail_shell_view);

	g_signal_handlers_disconnect_by_func (folder_tree,
		mail_shell_view_folder_renamed_cb, mail_shell_view);
}

void
e_mail_shell_view_rename_folder (EMailShellView *mail_shell_view)
{
	EMailShellSidebar *mail_shell_sidebar;
	EMFolderTree *folder_tree;

	g_return_if_fail (E_IS_MAIL_SHELL_VIEW (mail_shell_view));

	mail_shell_sidebar = mail_shell_view->priv->mail_shell_sidebar;
	folder_tree = e_mail_shell_sidebar_get_folder_tree (mail_shell_sidebar);

	em_folder_tree_edit_selected (folder_tree);

	mail_shell_view->priv->ignore_folder_popup_selection_done = TRUE;

	g_signal_connect_object (folder_tree, "folder-renamed",
		G_CALLBACK (mail_shell_view_folder_renamed_cb), mail_shell_view, 0);
}
