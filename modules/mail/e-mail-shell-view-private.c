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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

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

	e_mail_reader_set_folder (context->reader, folder);
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
mail_shell_view_folder_tree_selection_done_cb (EMailShellView *mail_shell_view,
                                               GtkWidget *menu)
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

	/* Disconnect from the "selection-done" signal. */
	g_signal_handlers_disconnect_by_func (
		menu, mail_shell_view_folder_tree_selection_done_cb,
		mail_shell_view);
}

static void
mail_shell_view_folder_tree_popup_event_cb (EShellView *shell_view,
                                            GdkEvent *button_event)
{
	GtkWidget *menu;

	menu = e_shell_view_show_popup_menu (
		shell_view, "/mail-folder-popup", button_event);

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
	EShellContent *shell_content;
	EMailView *mail_view;
	EMailReader *reader;
	EMailDisplay *mail_display;
	GtkAction *action;

	shell_view = E_SHELL_VIEW (mail_shell_view);
	shell_window = e_shell_view_get_shell_window (shell_view);

	if ((event->state & (GDK_CONTROL_MASK | GDK_SHIFT_MASK | GDK_MOD1_MASK)) != 0)
		return FALSE;

	shell_content = e_shell_view_get_shell_content (shell_view);
	mail_view = e_mail_shell_content_get_mail_view (E_MAIL_SHELL_CONTENT (shell_content));
	reader = E_MAIL_READER (mail_view);
	mail_display = e_mail_reader_get_mail_display (reader);

	switch (event->keyval) {
		case GDK_KEY_space:
			action = ACTION (MAIL_SMART_FORWARD);
			break;

		case GDK_KEY_BackSpace:
			action = ACTION (MAIL_SMART_BACKWARD);
			break;

		case GDK_KEY_Home:
		case GDK_KEY_Left:
		case GDK_KEY_Up:
		case GDK_KEY_Right:
		case GDK_KEY_Down:
		case GDK_KEY_Next:
		case GDK_KEY_End:
		case GDK_KEY_Begin:
			/* If Caret mode is enabled don't try to process these keys */
			if (e_web_view_get_caret_mode (E_WEB_VIEW (mail_display)))
				return FALSE;
		case GDK_KEY_Prior:
			if (!e_mail_display_needs_key (mail_display, FALSE) &&
			    webkit_web_view_get_main_frame (WEBKIT_WEB_VIEW (mail_display)) !=
			    webkit_web_view_get_focused_frame (WEBKIT_WEB_VIEW (mail_display))) {
				WebKitDOMDocument *document;
				WebKitDOMDOMWindow *window;

				document = webkit_web_view_get_dom_document (WEBKIT_WEB_VIEW (mail_display));
				window = webkit_dom_document_get_default_view (document);

				/* Workaround WebKit bug for key navigation, when inner IFRAME is focused.
				 * EMailView's inner IFRAMEs have disabled scrolling, but WebKit doesn't post
				 * key navigation events to parent's frame, thus the view doesn't scroll.
				 * This is a poor workaround for this issue, the main frame is focused,
				 * which has scrolling enabled.
				*/
				webkit_dom_dom_window_focus (window);
			}

			return FALSE;
		default:
			return FALSE;
	}

	if (e_mail_display_needs_key (mail_display, TRUE))
		return FALSE;

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
                                             GdkEvent *button_event)
{
	const gchar *widget_path;

	widget_path = "/mail-message-popup";
	e_shell_view_show_popup_menu (shell_view, widget_path, button_event);

	return TRUE;
}

static gboolean
mail_shell_view_popup_event_cb (EMailShellView *mail_shell_view,
                                const gchar *uri)
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

	menu = e_mail_reader_get_popup_menu (reader);
	shell_view = E_SHELL_VIEW (mail_shell_view);
	e_shell_view_update_actions (shell_view);

	gtk_menu_popup (
		menu, NULL, NULL, NULL, NULL,
		0, gtk_get_current_event_time ());

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
		display, "key-press-event",
		G_CALLBACK (mail_shell_view_key_press_event_cb),
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
mail_shell_view_reader_update_actions_cb (EMailReader *reader,
                                          guint32 state,
                                          EMailShellView *mail_shell_view)
{
	EMailShellContent *mail_shell_content;

	g_return_if_fail (mail_shell_view != NULL);
	g_return_if_fail (mail_shell_view->priv != NULL);

	mail_shell_content = mail_shell_view->priv->mail_shell_content;
	e_mail_reader_update_actions (E_MAIL_READER (mail_shell_content), state);
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

void
e_mail_shell_view_private_init (EMailShellView *mail_shell_view)
{
	e_signal_connect_notify (
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
	EShellSidebar *shell_sidebar;
	EShellTaskbar *shell_taskbar;
	EShellWindow *shell_window;
	EShellSearchbar *searchbar;
	EMFolderTree *folder_tree;
	EActionComboBox *combo_box;
	ERuleContext *context;
	EFilterRule *rule = NULL;
	GtkTreeSelection *selection;
	GtkUIManager *ui_manager;
	GtkWidget *message_list;
	GSettings *settings;
	EMailLabelListStore *label_store;
	EMailBackend *backend;
	EMailSession *session;
	EMailReader *reader;
	EMailView *mail_view;
	EMailDisplay *display;
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

	backend = E_MAIL_BACKEND (shell_backend);
	session = e_mail_backend_get_session (backend);
	label_store = e_mail_ui_session_get_label_store (
		E_MAIL_UI_SESSION (session));

	e_shell_window_add_action_group (shell_window, "mail");
	e_shell_window_add_action_group (shell_window, "mail-filter");
	e_shell_window_add_action_group (shell_window, "mail-label");
	e_shell_window_add_action_group (shell_window, "search-folders");

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
	display = e_mail_reader_get_mail_display (reader);
	message_list = e_mail_reader_get_message_list (reader);

	em_folder_tree_set_selectable_widget (folder_tree, message_list);

	/* The folder tree and scope combo box are both insensitive
	 * when searching beyond the currently selected folder. */
	e_binding_bind_property (
		folder_tree, "sensitive",
		combo_box, "sensitive",
		G_BINDING_BIDIRECTIONAL |
		G_BINDING_SYNC_CREATE);

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
		mail_view, "update-actions",
		G_CALLBACK (mail_shell_view_reader_update_actions_cb),
		mail_shell_view, 0);

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
		label_store, "changed",
		G_CALLBACK (e_mail_shell_view_update_search_filter),
		mail_shell_view, G_CONNECT_SWAPPED);

	g_signal_connect_object (
		display, "key-press-event",
		G_CALLBACK (mail_shell_view_key_press_event_cb),
		mail_shell_view, G_CONNECT_SWAPPED);

	g_signal_connect_object (
		display, "popup-event",
		G_CALLBACK (mail_shell_view_popup_event_cb),
		mail_shell_view, G_CONNECT_SWAPPED);

	g_signal_connect_object (
		display, "status-message",
		G_CALLBACK (e_shell_taskbar_set_message),
		shell_taskbar, G_CONNECT_SWAPPED);

	g_signal_connect_object (
		mail_shell_view, "toggled",
		G_CALLBACK (e_mail_shell_view_update_send_receive_menus),
		mail_shell_view, G_CONNECT_AFTER | G_CONNECT_SWAPPED);

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
	e_binding_bind_property (
		shell_content, "group-by-threads",
		mail_view, "group-by-threads",
		G_BINDING_BIDIRECTIONAL |
		G_BINDING_SYNC_CREATE);

	settings = e_util_ref_settings ("org.gnome.evolution.mail");
	g_settings_bind (
		settings, "vfolder-allow-expunge",
		mail_shell_view, "vfolder-allow-expunge",
		G_SETTINGS_BIND_GET);
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

	for (ii = 0; ii < MAIL_NUM_SEARCH_RULES; ii++)
		g_clear_object (&priv->search_rules[ii]);

	if (priv->opening_folder != NULL) {
		g_cancellable_cancel (priv->opening_folder);
		g_clear_object (&priv->opening_folder);
	}

	if (priv->search_account_all != NULL) {
		g_object_unref (priv->search_account_all);
		priv->search_account_all = NULL;
	}

	if (priv->search_account_current != NULL) {
		g_object_unref (priv->search_account_current);
		priv->search_account_current = NULL;
	}

	if (priv->search_account_cancel != NULL) {
		g_object_unref (priv->search_account_cancel);
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
	gchar *folder_uri;
	gchar *new_state_group;

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

	folder_uri = e_mail_folder_uri_from_folder (folder);
	new_state_group = g_strdup_printf ("Folder %s", folder_uri);
	old_state_group = e_shell_searchbar_get_state_group (searchbar);
	g_free (folder_uri);

	/* Avoid loading search state unnecessarily. */
	if (g_strcmp0 (new_state_group, old_state_group) != 0) {
		e_shell_searchbar_set_state_group (searchbar, new_state_group);
		e_shell_searchbar_load_state (searchbar);
	}

	g_free (new_state_group);

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
		GtkAction *action;
		gchar *label;

		action = e_shell_view_get_action (shell_view);

		g_object_get (action, "label", &label, NULL);
		e_shell_sidebar_set_secondary_text (shell_sidebar, NULL);
		e_shell_view_set_title (shell_view, label);
		g_free (label);

		return;
	}

	folder_name = camel_folder_get_display_name (folder);
	parent_store = camel_folder_get_parent_store (folder);

	folder_cache = e_mail_session_get_folder_cache (
		e_mail_backend_get_session (E_MAIL_BACKEND (shell_backend)));
	mail_folder_cache_get_folder_info_flags (folder_cache, parent_store, folder_name, &flags);
	is_inbox = (flags & CAMEL_FOLDER_TYPE_MASK) == CAMEL_FOLDER_TYPE_INBOX;

	num_deleted = camel_folder_summary_get_deleted_count (folder->summary);
	num_junked = camel_folder_summary_get_junk_count (folder->summary);
	num_junked_not_deleted =
		camel_folder_summary_get_junk_not_deleted_count (folder->summary);
	num_unread = camel_folder_summary_get_unread_count (folder->summary);
	num_visible = camel_folder_summary_get_visible_count (folder->summary);

	buffer = g_string_sized_new (256);
	message_list = MESSAGE_LIST (e_mail_reader_get_message_list (reader));
	selected_count = message_list_selected_count (message_list);

	if (selected_count > 1)
		g_string_append_printf (
			buffer, ngettext ("%d selected, ", "%d selected, ",
			selected_count), selected_count);

	/* "Trash" folder (virtual or real) */
	if (folder->folder_flags & CAMEL_FOLDER_IS_TRASH) {
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
	} else if (folder->folder_flags & CAMEL_FOLDER_IS_JUNK) {
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
	GtkMenuShell *menu;
	CamelSession *session;
	EMailAccountStore *account_store;

	/* GtkMenuItem -> CamelService */
	GHashTable *menu_items;

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

static GtkMenuItem *
send_receive_find_menu_item (SendReceiveData *data,
                             gpointer service)
{
	GHashTableIter iter;
	gpointer menu_item;
	gpointer candidate;

	g_hash_table_iter_init (&iter, data->menu_items);

	while (g_hash_table_iter_next (&iter, &menu_item, &candidate))
		if (service == candidate)
			return GTK_MENU_ITEM (menu_item);

	return NULL;
}

static void
send_receive_account_item_activate_cb (GtkMenuItem *menu_item,
                                       SendReceiveData *data)
{
	CamelService *service;

	service = g_hash_table_lookup (data->menu_items, menu_item);
	g_return_if_fail (CAMEL_IS_SERVICE (service));

	mail_receive_service (service);
}

typedef struct _EMenuItemSensitivityData {
	GObject *service;
	GtkWidget *menu_item;
} EMenuItemSensitivityData;

static void
free_menu_item_sensitivity_data (gpointer ptr)
{
	EMenuItemSensitivityData *data = ptr;

	if (!data)
		return;

	g_object_unref (data->service);
	g_object_unref (data->menu_item);
	g_free (data);
}

static gboolean
update_menu_item_sensitivity_cb (gpointer user_data)
{
	EMenuItemSensitivityData *data = user_data;
	gboolean is_online = FALSE;

	g_return_val_if_fail (data != NULL, FALSE);

	g_object_get (data->service, "online", &is_online, NULL);

	gtk_widget_set_sensitive (data->menu_item, is_online);

	return FALSE;
}

static void
service_online_state_changed_cb (GObject *service,
				 GParamSpec *param,
				 GObject *menu_item)
{
	EMenuItemSensitivityData *data;

	g_return_if_fail (G_IS_OBJECT (service));
	g_return_if_fail (GTK_IS_WIDGET (menu_item));

	data = g_new0 (EMenuItemSensitivityData, 1);
	data->service = g_object_ref (service);
	data->menu_item = g_object_ref (menu_item);

	g_idle_add_full (G_PRIORITY_HIGH_IDLE, update_menu_item_sensitivity_cb, data, free_menu_item_sensitivity_data);
}

static void
send_receive_add_to_menu (SendReceiveData *data,
                          CamelService *service,
                          gint position)
{
	GtkWidget *menu_item;
	CamelProvider *provider;

	if (send_receive_find_menu_item (data, service) != NULL)
		return;

	provider = camel_service_get_provider (service);

	menu_item = gtk_menu_item_new ();
	gtk_widget_show (menu_item);

	e_binding_bind_property (
		service, "display-name",
		menu_item, "label",
		G_BINDING_SYNC_CREATE);

	if (provider && (provider->flags & CAMEL_PROVIDER_IS_REMOTE) != 0) {
		gpointer object;

		if (CAMEL_IS_OFFLINE_STORE (service))
			object = g_object_ref (service);
		else
			object = camel_service_ref_session (service);

		e_signal_connect_notify_object (
			object, "notify::online",
			G_CALLBACK (service_online_state_changed_cb), menu_item,
			0);

		g_object_unref (object);
	}

	g_hash_table_insert (
		data->menu_items, menu_item,
		g_object_ref (service));

	g_signal_connect (
		menu_item, "activate",
		G_CALLBACK (send_receive_account_item_activate_cb), data);

	/* Position is with respect to the sorted list of CamelService-s,
	 * not menu item position. */
	if (position < 0)
		gtk_menu_shell_append (data->menu, menu_item);
	else
		gtk_menu_shell_insert (data->menu, menu_item, position + 4);
}

static void
send_receive_gather_services (gpointer menu_item,
                              gpointer service,
                              gpointer queue)
{
	g_queue_push_head (queue, service);
}

static gint
sort_services_cb (gconstpointer service1,
                  gconstpointer service2,
                  gpointer account_store)
{
	return e_mail_account_store_compare_services (account_store, CAMEL_SERVICE (service1), CAMEL_SERVICE (service2));
}

static void
send_receive_menu_service_added_cb (EMailAccountStore *account_store,
                                    CamelService *service,
                                    SendReceiveData *data)
{
	GQueue *services;

	if (!send_receive_can_use_service (account_store, service, NULL))
		return;

	services = g_queue_new ();

	g_queue_push_head (services, service);
	g_hash_table_foreach (data->menu_items, send_receive_gather_services, services);
	g_queue_sort (services, sort_services_cb, account_store);

	send_receive_add_to_menu (data, service, g_queue_index (services, service));

	g_queue_free (services);
}

static void
send_receive_menu_service_removed_cb (EMailAccountStore *account_store,
                                      CamelService *service,
                                      SendReceiveData *data)
{
	GtkMenuItem *menu_item;

	menu_item = send_receive_find_menu_item (data, service);
	if (menu_item == NULL)
		return;

	g_hash_table_remove (data->menu_items, menu_item);

	gtk_container_remove (
		GTK_CONTAINER (data->menu),
		GTK_WIDGET (menu_item));
}

static void
send_receive_data_free (SendReceiveData *data)
{
	g_signal_handler_disconnect (data->account_store, data->service_added_id);
	g_signal_handler_disconnect (data->account_store, data->service_removed_id);
	g_signal_handler_disconnect (data->account_store, data->service_enabled_id);
	g_signal_handler_disconnect (data->account_store, data->service_disabled_id);

	g_object_unref (data->session);
	g_object_unref (data->account_store);

	g_hash_table_destroy (data->menu_items);

	g_slice_free (SendReceiveData, data);
}

static SendReceiveData *
send_receive_data_new (EMailShellView *mail_shell_view,
                       GtkWidget *menu)
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
	account_store = e_mail_ui_session_get_account_store (
		E_MAIL_UI_SESSION (session));

	data = g_slice_new0 (SendReceiveData);
	data->menu = GTK_MENU_SHELL (menu);  /* do not reference */
	data->session = g_object_ref (session);
	data->account_store = g_object_ref (account_store);

	data->menu_items = g_hash_table_new_full (
		(GHashFunc) g_direct_hash,
		(GEqualFunc) g_direct_equal,
		(GDestroyNotify) NULL,
		(GDestroyNotify) g_object_unref);

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

	g_object_weak_ref (
		G_OBJECT (menu), (GWeakNotify)
		send_receive_data_free, data);

	return data;
}

static GtkWidget *
create_send_receive_submenu (EMailShellView *mail_shell_view)
{
	EShellView *shell_view;
	EShellWindow *shell_window;
	EShellBackend *shell_backend;
	EMailAccountStore *account_store;
	EMailBackend *backend;
	EMailSession *session;
	GtkWidget *menu;
	GtkAccelGroup *accel_group;
	GtkUIManager *ui_manager;
	GtkAction *action;
	GtkTreeModel *model;
	GtkTreeIter iter;
	SendReceiveData *data;

	g_return_val_if_fail (mail_shell_view != NULL, NULL);

	shell_view = E_SHELL_VIEW (mail_shell_view);
	shell_window = e_shell_view_get_shell_window (shell_view);
	shell_backend = e_shell_view_get_shell_backend (shell_view);

	backend = E_MAIL_BACKEND (shell_backend);
	session = e_mail_backend_get_session (backend);
	account_store = e_mail_ui_session_get_account_store (E_MAIL_UI_SESSION (session));

	menu = gtk_menu_new ();
	ui_manager = e_shell_window_get_ui_manager (shell_window);
	accel_group = gtk_ui_manager_get_accel_group (ui_manager);

	action = e_shell_window_get_action (shell_window, "mail-send-receive");
	gtk_action_set_accel_group (action, accel_group);
	gtk_menu_shell_append (
		GTK_MENU_SHELL (menu),
		gtk_action_create_menu_item (action));

	action = e_shell_window_get_action (
		shell_window, "mail-send-receive-receive-all");
	gtk_action_set_accel_group (action, accel_group);
	gtk_menu_shell_append (
		GTK_MENU_SHELL (menu),
		gtk_action_create_menu_item (action));

	action = e_shell_window_get_action (
		shell_window, "mail-send-receive-send-all");
	gtk_action_set_accel_group (action, accel_group);
	gtk_menu_shell_append (
		GTK_MENU_SHELL (menu),
		gtk_action_create_menu_item (action));

	gtk_menu_shell_append (
		GTK_MENU_SHELL (menu),
		gtk_separator_menu_item_new ());

	data = send_receive_data_new (mail_shell_view, menu);

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

	gtk_widget_show_all (menu);

	return menu;
}

void
e_mail_shell_view_update_send_receive_menus (EMailShellView *mail_shell_view)
{
	EMailShellViewPrivate *priv;
	EShellWindow *shell_window;
	EShellView *shell_view;
	GtkWidget *widget;
	const gchar *widget_path;

	g_return_if_fail (E_IS_MAIL_SHELL_VIEW (mail_shell_view));

	priv = E_MAIL_SHELL_VIEW_GET_PRIVATE (mail_shell_view);

	shell_view = E_SHELL_VIEW (mail_shell_view);
	shell_window = e_shell_view_get_shell_window (shell_view);

	if (!e_shell_view_is_active (shell_view)) {
		if (priv->send_receive_tool_item) {
			GtkWidget *toolbar;

			toolbar = e_shell_window_get_managed_widget (
				shell_window, "/main-toolbar");
			g_return_if_fail (toolbar != NULL);

			gtk_container_remove (
				GTK_CONTAINER (toolbar),
				GTK_WIDGET (priv->send_receive_tool_item));
			gtk_container_remove (
				GTK_CONTAINER (toolbar),
				GTK_WIDGET (priv->send_receive_tool_separator));

			priv->send_receive_tool_item = NULL;
			priv->send_receive_tool_separator = NULL;
		}

		return;
	}

	widget_path =
		"/main-menu/file-menu"
		"/mail-send-receiver/mail-send-receive-submenu";
	widget = e_shell_window_get_managed_widget (shell_window, widget_path);
	if (widget != NULL)
		gtk_menu_item_set_submenu (
			GTK_MENU_ITEM (widget),
			create_send_receive_submenu (mail_shell_view));

	if (!priv->send_receive_tool_item) {
		GtkWidget *toolbar;
		GtkToolItem *tool_item;
		gint index;

		toolbar = e_shell_window_get_managed_widget (
			shell_window, "/main-toolbar");
		g_return_if_fail (toolbar != NULL);

		widget_path =
			"/main-toolbar/toolbar-actions/mail-send-receiver";
		widget = e_shell_window_get_managed_widget (
			shell_window, widget_path);
		g_return_if_fail (widget != NULL);

		index = gtk_toolbar_get_item_index (
			GTK_TOOLBAR (toolbar), GTK_TOOL_ITEM (widget));

		tool_item = gtk_separator_tool_item_new ();
		gtk_toolbar_insert (GTK_TOOLBAR (toolbar), tool_item, index);
		gtk_widget_show (GTK_WIDGET (tool_item));
		priv->send_receive_tool_separator = tool_item;

		tool_item = GTK_TOOL_ITEM (
			e_menu_tool_button_new (_("Send / Receive")));
		gtk_tool_item_set_is_important (tool_item, TRUE);
		gtk_toolbar_insert (GTK_TOOLBAR (toolbar), tool_item, index);
		gtk_widget_show (GTK_WIDGET (tool_item));
		priv->send_receive_tool_item = tool_item;

		e_binding_bind_property (
			ACTION (MAIL_SEND_RECEIVE), "sensitive",
			tool_item, "sensitive",
			G_BINDING_SYNC_CREATE);
	}

	if (priv->send_receive_tool_item)
		gtk_menu_tool_button_set_menu (
			GTK_MENU_TOOL_BUTTON (priv->send_receive_tool_item),
			create_send_receive_submenu (mail_shell_view));
}
