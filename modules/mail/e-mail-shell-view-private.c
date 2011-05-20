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
#include "widgets/misc/e-menu-tool-button.h"

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
	e_shell_view_update_actions (context->shell_view);

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
		mail_shell_view->priv->opening_folder = NULL;
	}

	/* If we are to clear the message list, do so immediately. */
	if ((flags & CAMEL_FOLDER_NOSELECT) || folder_name == NULL) {
		e_mail_reader_set_folder (reader, NULL);
		e_shell_view_update_actions (shell_view);
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
	CamelFolder *folder;
	gchar *list_uri;
	gchar *tree_uri;

	mail_shell_content = mail_shell_view->priv->mail_shell_content;
	mail_view = e_mail_shell_content_get_mail_view (mail_shell_content);

	mail_shell_sidebar = mail_shell_view->priv->mail_shell_sidebar;
	folder_tree = e_mail_shell_sidebar_get_folder_tree (mail_shell_sidebar);

	reader = E_MAIL_READER (mail_view);
	message_list = e_mail_reader_get_message_list (reader);

	/* Don't use e_mail_reader_get_folder() here.  The fact that the
	 * method gets the folder from the message list is supposed to be
	 * a hidden implementation detail, and we want to explicitly get
	 * the folder URI from the message list here. */
	folder = MESSAGE_LIST (message_list)->folder;
	if (folder)
		list_uri = e_mail_folder_uri_from_folder (folder);
	else
		list_uri = NULL;
	tree_uri = em_folder_tree_get_selected_uri (folder_tree);

	/* If the folder tree and message list disagree on the current
	 * folder, reset the folder tree to match the message list. */
	if (list_uri && g_strcmp0 (tree_uri, list_uri) != 0)
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
		case GDK_KEY_space:
			action = ACTION (MAIL_SMART_FORWARD);
			break;

		case GDK_KEY_BackSpace:
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
	EMFormatHTML *html_formatter;
	EShellView *shell_view;
	EMailReader *reader;
	EMailView *mail_view;
	GtkMenu *menu;

	if (uri != NULL)
		return FALSE;

	mail_shell_content = mail_shell_view->priv->mail_shell_content;
	mail_view = e_mail_shell_content_get_mail_view (mail_shell_content);

	reader = E_MAIL_READER (mail_view);
	html_formatter = e_mail_reader_get_formatter (reader);

	if (html_formatter && e_web_view_get_cursor_image (em_format_html_get_web_view (html_formatter)) != NULL)
		return FALSE;

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
	formatter = e_mail_reader_get_formatter (reader);
	message_list = e_mail_reader_get_message_list (reader);

	em_folder_tree_set_selectable_widget (folder_tree, message_list);

	/* The folder tree and scope combo box are both insensitive
	 * when searching beyond the currently selected folder. */
	g_object_bind_property (
		folder_tree, "sensitive",
		combo_box, "sensitive",
		G_BINDING_BIDIRECTIONAL |
		G_BINDING_SYNC_CREATE);

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
		e_mail_shell_content_get_mail_view (mail_shell_content), "update-actions",
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
	g_object_bind_property (
		shell_content, "group-by-threads",
		mail_view, "group-by-threads",
		G_BINDING_BIDIRECTIONAL |
		G_BINDING_SYNC_CREATE);

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

	if (priv->opening_folder != NULL) {
		g_cancellable_cancel (priv->opening_folder);
		g_object_unref (priv->opening_folder);
		priv->opening_folder = NULL;
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
	folder = e_mail_reader_get_folder (reader);

	if (folder == NULL)
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

	folder_name = camel_folder_get_display_name (folder);
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
	} else if (em_utils_folder_is_drafts (folder)) {
		g_string_append_printf (
			buffer, ngettext ("%d draft", "%d drafts",
			num_visible), num_visible);

	/* "Outbox" folder */
	} else if (em_utils_folder_is_outbox (folder)) {
		g_string_append_printf (
			buffer, ngettext ("%d unsent", "%d unsent",
			num_visible), num_visible);

	/* "Sent" folder */
	} else if (em_utils_folder_is_sent (folder)) {
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
	display_name = folder_name;
	if (parent_store == local_store) {
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

	title = g_strdup_printf ("%s (%s)", display_name, buffer->str);
	e_shell_sidebar_set_secondary_text (shell_sidebar, buffer->str);
	e_shell_view_set_title (shell_view, title);
	g_free (title);

	g_string_free (buffer, TRUE);
}

void
e_mail_shell_view_send_receive (EMailShellView *mail_shell_view,
                                EMailSendReceiveMode mode,
                                const gchar *account_uid)
{
	EMailSession *session;
	EShellView *shell_view;
	EShellWindow *shell_window;
	EShellBackend *shell_backend;

	g_return_if_fail (mail_shell_view != NULL);

	shell_view = E_SHELL_VIEW (mail_shell_view);
	shell_window = e_shell_view_get_shell_window (shell_view);
	shell_backend = e_shell_view_get_shell_backend (shell_view);

	session = e_mail_backend_get_session (E_MAIL_BACKEND (shell_backend));

	em_utils_clear_get_password_canceled_accounts_flag ();

	if (!account_uid) {
		switch (mode) {
		case E_MAIL_SEND_RECEIVE_BOTH:
			mail_send_receive (GTK_WINDOW (shell_window), session);
			break;
		case E_MAIL_SEND_RECEIVE_RECEIVE:
			mail_receive (GTK_WINDOW (shell_window), session);
			break;
		case E_MAIL_SEND_RECEIVE_SEND:
			mail_send (session);
			break;
		}
	} else {
		/* allow only receive on individual accounts */
		EAccount *account;

		account = e_get_account_by_uid (account_uid);
		g_return_if_fail (account != NULL);

		if (account->enabled && account->source != NULL)
			mail_receive_account (session, account);
	}
}

static GtkMenuItem *
send_receive_find_account_menu_item (GtkMenuShell *menu,
                                     EAccount *account)
{
	GList *children, *child;

	g_return_val_if_fail (menu != NULL, NULL);
	g_return_val_if_fail (account != NULL, NULL);
	g_return_val_if_fail (account->uid != NULL, NULL);

	children = gtk_container_get_children (GTK_CONTAINER (menu));

	for (child = children; child != NULL; child = child->next) {
		GObject *obj = child->data;
		const gchar *uid;

		if (!obj)
			continue;

		uid = g_object_get_data (obj, "e-account-uid");
		if (!uid)
			continue;

		if (g_strcmp0 (uid, account->uid) == 0) {
			g_list_free (children);

			return GTK_MENU_ITEM (obj);
		}
	}

	g_list_free (children);

	return NULL;
}

static gint
send_receive_get_account_index (EAccount *account)
{
	gint res;
	EAccountList *accounts;
	EIterator *iterator;

	g_return_val_if_fail (account != NULL, -1);

	accounts = e_get_account_list ();
	g_return_val_if_fail (accounts != NULL, -1);

	res = 0;
	for (iterator = e_list_get_iterator (E_LIST (accounts));
	     e_iterator_is_valid (iterator);
	     e_iterator_next (iterator)) {
		EAccount *candidate;
		const gchar *name;

		candidate = (EAccount *) e_iterator_get (iterator);

		if (candidate == NULL)
			continue;

		if (!candidate->enabled)
			continue;

		if (candidate->source == NULL)
			continue;

		if (candidate->source->url == NULL)
			continue;

		if (*candidate->source->url == '\0')
			continue;

		name = e_account_get_string (candidate, E_ACCOUNT_NAME);
		if (name == NULL || *name == '\0')
			continue;

		if (candidate->uid == NULL)
			continue;

		if (*candidate->uid == '\0')
			continue;

		if (g_strcmp0 (candidate->uid, account->uid) == 0) {
			g_object_unref (iterator);
			return res;
		}

		res++;
	}

	g_object_unref (iterator);

	return -1;
}

static void
send_receive_account_item_activate_cb (GtkMenuItem *item,
                                       GtkMenuShell *menu)
{
	EMailShellView *mail_shell_view;
	const gchar *account_uid;

	g_return_if_fail (item != NULL);
	g_return_if_fail (menu != NULL);

	mail_shell_view = g_object_get_data (G_OBJECT (menu), "mail-shell-view");
	g_return_if_fail (mail_shell_view != NULL);

	account_uid = g_object_get_data (G_OBJECT (item), "e-account-uid");
	g_return_if_fail (account_uid != NULL);

	e_mail_shell_view_send_receive (
		mail_shell_view, E_MAIL_SEND_RECEIVE_RECEIVE, account_uid);
}

static void
send_receive_add_to_menu (GtkMenuShell *menu,
                          EAccount *account,
                          gint insert_index)
{
	const gchar *name;
	GtkWidget *item;

	g_return_if_fail (menu != NULL);
	g_return_if_fail (account != NULL);

	if (send_receive_find_account_menu_item (menu, account) != NULL)
		return;

	if (account->source == NULL)
		return;

	if (account->source->url == NULL)
		return;

	if (*account->source->url == '\0')
		return;

	name = e_account_get_string (account, E_ACCOUNT_NAME);
	if (name == NULL || *name == '\0')
		return;

	if (account->uid == NULL)
		return;

	if (*account->uid == '\0')
		return;

	item = gtk_menu_item_new_with_label (name);
	gtk_widget_show (item);
	g_object_set_data_full (
		G_OBJECT (item), "e-account-uid",
		g_strdup (account->uid), g_free);
	g_signal_connect (
		item, "activate",
		G_CALLBACK (send_receive_account_item_activate_cb), menu);

	/* it's index between accounts, not in the menu */
	if (insert_index < 0)
		gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
	else
		gtk_menu_shell_insert (
			GTK_MENU_SHELL (menu), item, insert_index + 4);
}

static void
send_receive_remove_from_menu (GtkMenuShell *menu,
                               EAccount *account)
{
	GtkMenuItem *item;

	g_return_if_fail (menu != NULL);
	g_return_if_fail (account != NULL);

	item = send_receive_find_account_menu_item (menu, account);
	if (item == NULL)
		return;

	gtk_container_remove (GTK_CONTAINER (menu), GTK_WIDGET (item));
}

static void
send_receive_menu_account_added_cb (EAccountList *list,
                                    EAccount *account,
                                    GtkMenuShell *menu)
{
	g_return_if_fail (account != NULL);
	g_return_if_fail (menu != NULL);

	if (account->enabled)
		send_receive_add_to_menu (
			menu, account,
			send_receive_get_account_index (account));
}

static void
send_receive_menu_account_changed_cb (EAccountList *list,
                                      EAccount *account,
                                      GtkMenuShell *menu)
{
	g_return_if_fail (account != NULL);
	g_return_if_fail (menu != NULL);

	if (account->enabled) {
		GtkMenuItem *item;

		item = send_receive_find_account_menu_item (menu, account);

		if (item) {
			if (account->source == NULL ||
				account->source->url == NULL ||
				*account->source->url == '\0') {
				send_receive_remove_from_menu (menu, account);
			} else {
				const gchar *name;

				name = e_account_get_string (
					account, E_ACCOUNT_NAME);
				if (name != NULL && *name != '\0')
					gtk_menu_item_set_label (item, name);
			}
		} else {
			send_receive_add_to_menu (
				menu, account,
				send_receive_get_account_index (account));
		}
	} else {
		send_receive_remove_from_menu (menu, account);
	}
}

static void
send_receive_menu_account_removed_cb (EAccountList *list,
                                      EAccount *account,
                                      GtkMenuShell *menu)
{
	g_return_if_fail (account != NULL);
	g_return_if_fail (menu != NULL);

	send_receive_remove_from_menu (menu, account);
}

static void
menu_weak_ref_cb (gpointer accounts,
                  GObject *where_the_object_was)
{
	g_return_if_fail (accounts != NULL);

	g_signal_handlers_disconnect_matched (
		accounts, G_SIGNAL_MATCH_DATA,
		0, 0, NULL, NULL, where_the_object_was);
}

static GtkWidget *
create_send_receive_submenu (EMailShellView *mail_shell_view)
{
	EShellView *shell_view;
	EShellWindow *shell_window;
	EAccountList *accounts;
	GtkWidget *menu;
	GtkAccelGroup *accel_group;
	GtkUIManager *ui_manager;
	GtkAction *action;

	g_return_val_if_fail (mail_shell_view != NULL, NULL);

	shell_view = E_SHELL_VIEW (mail_shell_view);
	shell_window = e_shell_view_get_shell_window (shell_view);

	accounts = e_get_account_list ();
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

	if (accounts) {
		EIterator *iterator;

		for (iterator = e_list_get_iterator (E_LIST (accounts));
		     e_iterator_is_valid (iterator);
		     e_iterator_next (iterator)) {
			EAccount *account;

			account = (EAccount *) e_iterator_get (iterator);

			if (account == NULL)
				continue;

			if (!account->enabled)
				continue;

			send_receive_add_to_menu (
				GTK_MENU_SHELL (menu), account, -1);
		}

		g_object_unref (iterator);

		g_signal_connect (
			accounts, "account-added",
			G_CALLBACK (send_receive_menu_account_added_cb), menu);
		g_signal_connect (
			accounts, "account-changed",
			G_CALLBACK (send_receive_menu_account_changed_cb), menu);
		g_signal_connect (
			accounts, "account-removed",
			G_CALLBACK (send_receive_menu_account_removed_cb), menu);

		g_object_weak_ref (
			G_OBJECT (menu), menu_weak_ref_cb, accounts);
	}

	gtk_widget_show_all (menu);

	g_object_set_data (G_OBJECT (menu), "mail-shell-view", mail_shell_view);

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

	priv = mail_shell_view->priv;

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

		g_object_bind_property (
			ACTION (MAIL_SEND_RECEIVE), "sensitive",
			tool_item, "sensitive",
			G_BINDING_SYNC_CREATE);
	}

	if (priv->send_receive_tool_item)
		gtk_menu_tool_button_set_menu (
			GTK_MENU_TOOL_BUTTON (priv->send_receive_tool_item),
			create_send_receive_submenu (mail_shell_view));
}
