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

static void
mail_shell_view_folder_tree_selected_cb (EMailShellView *mail_shell_view,
                                         const gchar *full_name,
                                         const gchar *uri,
                                         guint32 flags,
                                         EMFolderTree *folder_tree)
{
	EShellView *shell_view;
	EMailReader *reader;
	gboolean folder_selected;

	shell_view = E_SHELL_VIEW (mail_shell_view);
	reader = E_MAIL_READER (mail_shell_view->priv->mail_shell_content);

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
	EMailReader *reader;
	gboolean handled = FALSE;

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
	reader = E_MAIL_READER (mail_shell_view->priv->mail_shell_content);
	g_signal_emit_by_name (reader, "key-press-event", event, &handled);

exit:
	return handled;
}

static void
mail_shell_view_folder_tree_selection_done_cb (EMailShellView *mail_shell_view,
                                               GtkWidget *menu)
{
	EMailShellSidebar *mail_shell_sidebar;
	EMFolderTree *folder_tree;
	MessageList *message_list;
	EMailReader *reader;
	const gchar *list_uri;
	gchar *tree_uri;

	reader = E_MAIL_READER (mail_shell_view->priv->mail_shell_content);
	message_list = e_mail_reader_get_message_list (reader);

	mail_shell_sidebar = mail_shell_view->priv->mail_shell_sidebar;
	folder_tree = e_mail_shell_sidebar_get_folder_tree (mail_shell_sidebar);

	list_uri = message_list->folder_uri;
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

	g_signal_connect_swapped (
		menu, "selection-done",
		G_CALLBACK (mail_shell_view_folder_tree_selection_done_cb),
		shell_view);
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

static gint
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
	EShellView *shell_view;
	EMailReader *reader;
	GtkMenu *menu;

	if (uri != NULL)
		return FALSE;

	shell_view = E_SHELL_VIEW (mail_shell_view);
	reader = E_MAIL_READER (mail_shell_view->priv->mail_shell_content);
	menu = e_mail_reader_get_popup_menu (reader);

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
mail_shell_view_reader_changed_cb (EMailShellView *mail_shell_view,
                                   EMailReader *reader)
{
	e_shell_view_update_actions (E_SHELL_VIEW (mail_shell_view));
	e_mail_shell_view_update_sidebar (mail_shell_view);
}

static void
mail_shell_view_reader_status_message_cb (EMailShellView *mail_shell_view,
                                          const gchar *status_message)
{
	EShellView *shell_view;
	EShellTaskbar *shell_taskbar;

	shell_view = E_SHELL_VIEW (mail_shell_view);
	shell_taskbar = e_shell_view_get_shell_taskbar (shell_view);

	e_shell_taskbar_set_message (shell_taskbar, status_message);
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
	EMailReader *reader;
	MessageList *message_list;
	gboolean magic_spacebar;

	if (html->binding_handled)
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

	reader = E_MAIL_READER (mail_shell_view->priv->mail_shell_content);
	message_list = e_mail_reader_get_message_list (reader);

	if (scroll_type == GTK_SCROLL_PAGE_FORWARD) {
		gtk_widget_grab_focus (GTK_WIDGET (message_list));
		message_list_select (
			message_list, MESSAGE_LIST_SELECT_NEXT,
			0, CAMEL_MESSAGE_SEEN);
	} else {
		gtk_widget_grab_focus (GTK_WIDGET (message_list));
		message_list_select (
			message_list, MESSAGE_LIST_SELECT_PREVIOUS,
			0, CAMEL_MESSAGE_SEEN);
	}
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
	EMailReader *reader;
	MessageList *message_list;

	/* If we got here, it means the application is shutting down
	 * and this is the last EMailShellView instance.  Synchronize
	 * the currently selected folder before we terminate. */

	reader = E_MAIL_READER (mail_shell_view->priv->mail_shell_content);
	message_list = e_mail_reader_get_message_list (reader);

	if (message_list->folder == NULL)
		return;

	mail_sync_folder (
		message_list->folder,
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
	const gchar *view_id;

	mail_shell_content = mail_shell_view->priv->mail_shell_content;
	view_instance = e_mail_shell_content_get_view_instance (mail_shell_content);
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
	EMailShellSidebar *mail_shell_sidebar;
	EShell *shell;
	EShellView *shell_view;
	EShellBackend *shell_backend;
	EShellContent *shell_content;
	EShellSettings *shell_settings;
	EShellSidebar *shell_sidebar;
	EShellWindow *shell_window;
	EMFormatHTMLDisplay *html_display;
	EMFolderTree *folder_tree;
	ERuleContext *context;
	EFilterRule *rule = NULL;
	GtkTreeSelection *selection;
	GtkTreeModel *tree_model;
	GtkUIManager *ui_manager;
	MessageList *message_list;
	EMailReader *reader;
	EWebView *web_view;
	const gchar *source;
	guint merge_id;
	gint ii = 0;

	shell_view = E_SHELL_VIEW (mail_shell_view);
	shell_backend = e_shell_view_get_shell_backend (shell_view);
	shell_content = e_shell_view_get_shell_content (shell_view);
	shell_sidebar = e_shell_view_get_shell_sidebar (shell_view);
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

	reader = E_MAIL_READER (shell_content);
	html_display = e_mail_reader_get_html_display (reader);
	message_list = e_mail_reader_get_message_list (reader);

	mail_shell_sidebar = E_MAIL_SHELL_SIDEBAR (shell_sidebar);
	folder_tree = e_mail_shell_sidebar_get_folder_tree (mail_shell_sidebar);
	selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (folder_tree));

	web_view = E_WEB_VIEW (EM_FORMAT_HTML (html_display)->html);

	g_signal_connect_swapped (
		folder_tree, "folder-selected",
		G_CALLBACK (mail_shell_view_folder_tree_selected_cb),
		mail_shell_view);

	g_signal_connect_swapped (
		folder_tree, "key-press-event",
		G_CALLBACK (mail_shell_view_folder_tree_key_press_event_cb),
		mail_shell_view);

	g_signal_connect_swapped (
		folder_tree, "popup-event",
		G_CALLBACK (mail_shell_view_folder_tree_popup_event_cb),
		mail_shell_view);

	g_signal_connect_swapped (
		message_list->tree, "key-press",
		G_CALLBACK (mail_shell_view_message_list_key_press_cb),
		mail_shell_view);

	g_signal_connect_swapped (
		message_list->tree, "popup-menu",
		G_CALLBACK (mail_shell_view_message_list_popup_menu_cb),
		mail_shell_view);

	g_signal_connect_swapped (
		message_list->tree, "right-click",
		G_CALLBACK (mail_shell_view_message_list_right_click_cb),
		mail_shell_view);

	g_signal_connect_swapped (
		reader, "changed",
		G_CALLBACK (mail_shell_view_reader_changed_cb),
		mail_shell_view);

	g_signal_connect_swapped (
		reader, "folder-loaded",
		G_CALLBACK (e_mail_shell_content_update_view_instance),
		shell_content);

	/* Use the same callback as "changed". */
	g_signal_connect_swapped (
		reader, "folder-loaded",
		G_CALLBACK (mail_shell_view_reader_changed_cb),
		mail_shell_view);

	g_signal_connect_swapped (
		reader, "folder-loaded",
		G_CALLBACK (e_mail_shell_view_restore_state),
		mail_shell_view);

	g_signal_connect_swapped (
		tree_model, "row-changed",
		G_CALLBACK (e_mail_shell_view_update_search_filter),
		mail_shell_view);

	g_signal_connect_swapped (
		tree_model, "row-deleted",
		G_CALLBACK (e_mail_shell_view_update_search_filter),
		mail_shell_view);

	g_signal_connect_swapped (
		tree_model, "row-inserted",
		G_CALLBACK (e_mail_shell_view_update_search_filter),
		mail_shell_view);

	g_signal_connect_swapped (
		web_view, "key-press-event",
		G_CALLBACK (mail_shell_view_key_press_event_cb),
		mail_shell_view);

	g_signal_connect_swapped (
		web_view, "popup-event",
		G_CALLBACK (mail_shell_view_popup_event_cb),
		mail_shell_view);

	g_signal_connect_data (
		web_view, "scroll",
		G_CALLBACK (mail_shell_view_scroll_cb),
		mail_shell_view, (GClosureNotify) NULL,
		G_CONNECT_AFTER | G_CONNECT_SWAPPED);

	g_signal_connect_swapped (
		web_view, "status-message",
		G_CALLBACK (mail_shell_view_reader_status_message_cb),
		mail_shell_view);

	/* Need to keep the handler ID so we can disconnect it in
	 * dispose().  The shell outlives us and we don't want it
	 * invoking callbacks on finalized shell views. */
	priv->prepare_for_quit_handler_id =
		g_signal_connect_swapped (
			shell, "prepare-for-quit",
			G_CALLBACK (mail_shell_view_prepare_for_quit_cb),
			mail_shell_view);

	e_mail_shell_view_actions_init (mail_shell_view);
	e_mail_shell_view_update_search_filter (mail_shell_view);
	e_mail_reader_init (reader);

	/* Populate built-in rules for search entry popup menu.
	 * Keep the assertions, please.  If the conditions aren't
	 * met we're going to crash anyway, just more mysteriously. */
	context = e_shell_content_get_search_context (shell_content);
	source = E_FILTER_SOURCE_DEMAND;
	while ((rule = e_rule_context_next_rule (context, rule, source))) {
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
}

void
e_mail_shell_view_private_finalize (EMailShellView *mail_shell_view)
{
	/* XXX Nothing to do? */
}

void
e_mail_shell_view_restore_state (EMailShellView *mail_shell_view)
{
	EShellView *shell_view;
	EShellContent *shell_content;
	EMailReader *reader;
	MessageList *message_list;
	const gchar *folder_uri;
	gchar *group_name;

	/* XXX Move this to EMailShellContent. */

	g_return_if_fail (E_IS_MAIL_SHELL_VIEW (mail_shell_view));

	shell_view = E_SHELL_VIEW (mail_shell_view);
	shell_content = e_shell_view_get_shell_content (shell_view);

	reader = E_MAIL_READER (shell_content);
	message_list = e_mail_reader_get_message_list (reader);
	folder_uri = message_list->folder_uri;

	if (folder_uri == NULL)
		return;

	group_name = g_strdup_printf ("Folder %s", folder_uri);
	e_shell_content_restore_state (shell_content, group_name);
	g_free (group_name);
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
	EMailReader *reader;
	MessageList *message_list;
	CamelFolder *folder;
	const gchar *filter_source;
	const gchar *folder_uri;
	GPtrArray *uids;

	struct {
		const gchar *source;
		gint type;
	} *filter_data;

	g_return_if_fail (E_IS_MAIL_SHELL_VIEW (mail_shell_view));

	reader = E_MAIL_READER (mail_shell_view->priv->mail_shell_content);
	message_list = e_mail_reader_get_message_list (reader);
	folder_uri = message_list->folder_uri;
	folder = message_list->folder;

	if (em_utils_folder_is_sent (folder, folder_uri))
		filter_source = E_FILTER_SOURCE_OUTGOING;
	else if (em_utils_folder_is_outbox (folder, folder_uri))
		filter_source = E_FILTER_SOURCE_OUTGOING;
	else
		filter_source = E_FILTER_SOURCE_INCOMING;

	uids = message_list_get_selected (message_list);

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
	EMailReader *reader;
	MessageList *message_list;
	CamelFolder *folder;
	const gchar *folder_uri;
	GPtrArray *uids;

	struct {
		gchar *uri;
		gint type;
	} *vfolder_data;

	g_return_if_fail (E_IS_MAIL_SHELL_VIEW (mail_shell_view));

	reader = E_MAIL_READER (mail_shell_view->priv->mail_shell_content);
	message_list = e_mail_reader_get_message_list (reader);
	folder_uri = message_list->folder_uri;
	folder = message_list->folder;

	uids = message_list_get_selected (message_list);

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
	MessageList *message_list;
	CamelStore *local_store;
	CamelFolder *folder;
	GPtrArray *selected;
	GString *buffer;
	const gchar *display_name;
	const gchar *folder_uri;
	gchar *folder_name;
	gchar *title;
	guint32 num_deleted;
	guint32 num_junked;
	guint32 num_junked_not_deleted;
	guint32 num_unread;
	guint32 num_visible;

	g_return_if_fail (E_IS_MAIL_SHELL_VIEW (mail_shell_view));

	mail_shell_content = mail_shell_view->priv->mail_shell_content;

	shell_view = E_SHELL_VIEW (mail_shell_view);
	shell_sidebar = e_shell_view_get_shell_sidebar (shell_view);

	reader = E_MAIL_READER (mail_shell_content);
	message_list = e_mail_reader_get_message_list (reader);
	folder_uri = message_list->folder_uri;
	folder = message_list->folder;

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

	camel_object_get (
		folder, NULL,
		CAMEL_FOLDER_NAME, &folder_name,
		CAMEL_FOLDER_DELETED, &num_deleted,
		CAMEL_FOLDER_JUNKED, &num_junked,
		CAMEL_FOLDER_JUNKED_NOT_DELETED, &num_junked_not_deleted,
		CAMEL_FOLDER_UNREAD, &num_unread,
		CAMEL_FOLDER_VISIBLE, &num_visible,
		NULL);

	buffer = g_string_sized_new (256);
	selected = message_list_get_selected (message_list);

	if (selected->len > 1)
		g_string_append_printf (
			buffer, ngettext ("%d selected, ", "%d selected, ",
			selected->len), selected->len);

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

		if (num_unread > 0 && selected->len <= 1)
			g_string_append_printf (
				buffer, ngettext ("%d unread, ",
				"%d unread, ", num_unread), num_unread);
		g_string_append_printf (
			buffer, ngettext ("%d total", "%d total",
			num_visible), num_visible);
	}

	message_list_free_uids (message_list, selected);

	/* Choose a suitable folder name for displaying. */
	if (folder->parent_store == local_store && (
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

	camel_object_free (folder, CAMEL_FOLDER_NAME, folder_name);
	g_string_free (buffer, TRUE);
}
