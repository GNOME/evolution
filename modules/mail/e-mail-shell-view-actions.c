/*
 * e-mail-shell-view-actions.c
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

#include "mail/mail-folder-cache.h"
#include "e-mail-shell-view-private.h"

static void
action_gal_save_custom_view_cb (GtkAction *action,
                                EMailShellView *mail_shell_view)
{
	EMailShellContent *mail_shell_content;
	EShellView *shell_view;
	EMailView *mail_view;
	GalViewInstance *view_instance;

	/* All shell views repond to the activation of this action,
	 * which is defined by EShellWindow.  But only the currently
	 * active shell view proceeds with saving the custom view. */
	shell_view = E_SHELL_VIEW (mail_shell_view);
	if (!e_shell_view_is_active (shell_view))
		return;

	mail_shell_content = mail_shell_view->priv->mail_shell_content;
	mail_view = e_mail_shell_content_get_mail_view (mail_shell_content);
	view_instance = e_mail_view_get_view_instance (mail_view);

	gal_view_instance_save_as (view_instance);
}

static void
action_mail_account_disable_cb (GtkAction *action,
                                EMailShellView *mail_shell_view)
{
	EMailShellSidebar *mail_shell_sidebar;
	EMFolderTree *folder_tree;
	EAccountList *account_list;
	EAccount *account;
	gchar *folder_uri;

	mail_shell_sidebar = mail_shell_view->priv->mail_shell_sidebar;

	folder_tree = e_mail_shell_sidebar_get_folder_tree (mail_shell_sidebar);
	folder_uri = em_folder_tree_get_selected_uri (folder_tree);
	g_return_if_fail (folder_uri != NULL);

	account_list = e_get_account_list ();
	account = mail_config_get_account_by_source_url (folder_uri);
	g_return_if_fail (account != NULL);

	if (e_account_list_account_has_proxies (account_list, account))
		e_account_list_remove_account_proxies (account_list, account);

	account->enabled = !account->enabled;
	e_account_list_change (account_list, account);
	e_mail_store_remove_by_uri (folder_uri);

	if (account->parent_uid != NULL)
		e_account_list_remove (account_list, account);

	e_account_list_save (account_list);

	g_free (folder_uri);
}

static void
action_mail_create_search_folder_cb (GtkAction *action,
                                     EMailShellView *mail_shell_view)
{
	EMailShellContent *mail_shell_content;
	EMailReader *reader;
	EShellView *shell_view;
	EShellSearchbar *searchbar;
	EFilterRule *search_rule;
	EMVFolderRule *vfolder_rule;
	EMailView *mail_view;
	const gchar *folder_uri;
	const gchar *search_text;
	gchar *rule_name;

	vfolder_load_storage ();

	shell_view = E_SHELL_VIEW (mail_shell_view);
	search_rule = e_shell_view_get_search_rule (shell_view);
	g_return_if_fail (search_rule != NULL);

	mail_shell_content = mail_shell_view->priv->mail_shell_content;
	mail_view = e_mail_shell_content_get_mail_view (mail_shell_content);
	searchbar = e_mail_shell_content_get_searchbar (mail_shell_content);

	search_text = e_shell_searchbar_get_search_text (searchbar);
	if (search_text == NULL || *search_text == '\0')
		search_text = "''";

	reader = E_MAIL_READER (mail_view);
	folder_uri = e_mail_reader_get_folder_uri (reader);

	search_rule = vfolder_clone_rule (search_rule);
	rule_name = g_strdup_printf ("%s %s", search_rule->name, search_text);
	e_filter_rule_set_source (search_rule, E_FILTER_SOURCE_INCOMING);
	e_filter_rule_set_name (search_rule, rule_name);
	g_free (rule_name);

	vfolder_rule = EM_VFOLDER_RULE (search_rule);
	em_vfolder_rule_add_source (vfolder_rule, folder_uri);
	vfolder_gui_add_rule (vfolder_rule);
}

static void
action_mail_download_foreach_cb (CamelService *service)
{
	if (CAMEL_IS_DISCO_STORE (service) || CAMEL_IS_OFFLINE_STORE (service))
		mail_store_prepare_offline (CAMEL_STORE (service));
}

static void
action_mail_download_cb (GtkAction *action,
                         EMailShellView *mail_shell_view)
{
	e_mail_store_foreach ((GHFunc) action_mail_download_foreach_cb, NULL);
}

static void
action_mail_flush_outbox_cb (GtkAction *action,
                             EMailShellView *mail_shell_view)
{
	mail_send ();
}

static void
action_mail_folder_copy_cb (GtkAction *action,
                            EMailShellView *mail_shell_view)
{
	EShellView *shell_view;
	EShellWindow *shell_window;
	EMailShellSidebar *mail_shell_sidebar;
	CamelFolderInfo *folder_info;
	EMFolderTree *folder_tree;

	shell_view = E_SHELL_VIEW (mail_shell_view);
	shell_window = e_shell_view_get_shell_window (shell_view);

	mail_shell_sidebar = mail_shell_view->priv->mail_shell_sidebar;
	folder_tree = e_mail_shell_sidebar_get_folder_tree (mail_shell_sidebar);
	folder_info = em_folder_tree_get_selected_folder_info (folder_tree);
	g_return_if_fail (folder_info != NULL);

	/* XXX Leaking folder_info? */
	em_folder_utils_copy_folder (
		GTK_WINDOW (shell_window), folder_info, FALSE);
}

static void
action_mail_folder_delete_cb (GtkAction *action,
                              EMailShellView *mail_shell_view)
{
	EMailShellSidebar *mail_shell_sidebar;
	EMFolderTree *folder_tree;
	CamelFolder *folder;

	mail_shell_sidebar = mail_shell_view->priv->mail_shell_sidebar;
	folder_tree = e_mail_shell_sidebar_get_folder_tree (mail_shell_sidebar);
	folder = em_folder_tree_get_selected_folder (folder_tree);
	g_return_if_fail (folder != NULL);

	em_folder_utils_delete_folder (folder);
}

static void
action_mail_folder_expunge_cb (GtkAction *action,
                               EMailShellView *mail_shell_view)
{
	EMailShellSidebar *mail_shell_sidebar;
	EMFolderTree *folder_tree;
	EShellWindow *shell_window;
	EShellView *shell_view;
	CamelFolder *folder;

	/* This handles both the "folder-expunge" and "account-expunge"
	 * actions. */

	shell_view = E_SHELL_VIEW (mail_shell_view);
	shell_window = e_shell_view_get_shell_window (shell_view);

	/* Get the folder from the folder tree, not the message list.
	 * This correctly handles the use case of right-clicking on
	 * the "Trash" folder and selecting "Empty Trash" without
	 * actually selecting the folder.  In that case the message
	 * list would not contain the correct folder to expunge. */

	mail_shell_sidebar = mail_shell_view->priv->mail_shell_sidebar;
	folder_tree = e_mail_shell_sidebar_get_folder_tree (mail_shell_sidebar);
	folder = em_folder_tree_get_selected_folder (folder_tree);
	g_return_if_fail (folder != NULL);

	em_utils_expunge_folder (GTK_WIDGET (shell_window), folder);
}

static void
action_mail_folder_mark_all_as_read_cb (GtkAction *action,
                                        EMailShellView *mail_shell_view)
{
	EShellView *shell_view;
	EShellWindow *shell_window;
	EMailShellContent *mail_shell_content;
	EMailReader *reader;
	EMailView *mail_view;
	CamelFolder *folder;
	GtkWindow *parent;
	MailFolderCache *cache;
	GtkWidget *message_list;
	GPtrArray *uids;
	const gchar *key;
	const gchar *prompt;
	guint ii;

	shell_view = E_SHELL_VIEW (mail_shell_view);
	shell_window = e_shell_view_get_shell_window (shell_view);
	parent = GTK_WINDOW (shell_window);

	mail_shell_content = mail_shell_view->priv->mail_shell_content;
	mail_view = e_mail_shell_content_get_mail_view (mail_shell_content);

	reader = E_MAIL_READER (mail_view);

	folder = e_mail_reader_get_folder (reader);
	g_return_if_fail (folder != NULL);

	cache = mail_folder_cache_get_default ();
	key = "/apps/evolution/mail/prompts/mark_all_read";
	if (mail_folder_cache_get_folder_has_children (cache, folder, NULL))
		prompt = "mail:ask-mark-all-read-sub";
	else
		prompt = "mail:ask-mark-all-read";

	if (!em_utils_prompt_user (parent, key, prompt, NULL))
		return;

	message_list = e_mail_reader_get_message_list (reader);
	g_return_if_fail (message_list != NULL);

	uids = message_list_get_uids (MESSAGE_LIST (message_list));

	camel_folder_freeze (folder);
	for (ii = 0; ii < uids->len; ii++)
		camel_folder_set_message_flags (
			folder, uids->pdata[ii],
			CAMEL_MESSAGE_SEEN, CAMEL_MESSAGE_SEEN);
	camel_folder_thaw (folder);

	em_utils_uids_free (uids);
}

static void
action_mail_folder_move_cb (GtkAction *action,
                            EMailShellView *mail_shell_view)
{
	EShellView *shell_view;
	EShellWindow *shell_window;
	EMailShellSidebar *mail_shell_sidebar;
	CamelFolderInfo *folder_info;
	EMFolderTree *folder_tree;

	shell_view = E_SHELL_VIEW (mail_shell_view);
	shell_window = e_shell_view_get_shell_window (shell_view);

	mail_shell_sidebar = mail_shell_view->priv->mail_shell_sidebar;
	folder_tree = e_mail_shell_sidebar_get_folder_tree (mail_shell_sidebar);
	folder_info = em_folder_tree_get_selected_folder_info (folder_tree);
	g_return_if_fail (folder_info != NULL);

	/* XXX Leaking folder_info? */
	em_folder_utils_copy_folder (
		GTK_WINDOW (shell_window), folder_info, TRUE);
}

static void
action_mail_folder_new_cb (GtkAction *action,
                           EMailShellView *mail_shell_view)
{
	EShellView *shell_view;
	EShellWindow *shell_window;
	EMailShellSidebar *mail_shell_sidebar;
	CamelFolderInfo *folder_info;
	EMFolderTree *folder_tree;

	shell_view = E_SHELL_VIEW (mail_shell_view);
	shell_window = e_shell_view_get_shell_window (shell_view);

	mail_shell_sidebar = mail_shell_view->priv->mail_shell_sidebar;
	folder_tree = e_mail_shell_sidebar_get_folder_tree (mail_shell_sidebar);
	folder_info = em_folder_tree_get_selected_folder_info (folder_tree);
	g_return_if_fail (folder_info != NULL);

	em_folder_utils_create_folder (
		folder_info, folder_tree, GTK_WINDOW (shell_window));
	camel_folder_info_free (folder_info);
}

static void
action_mail_folder_properties_cb (GtkAction *action,
                                  EMailShellView *mail_shell_view)
{
	EMailShellSidebar *mail_shell_sidebar;
	EMFolderTree *folder_tree;
	EShellView *shell_view;
	GtkTreeSelection *selection;
	GtkTreeView *tree_view;
	GtkTreeModel *model;
	GtkTreeIter iter;
	gchar *uri;

	shell_view = E_SHELL_VIEW (mail_shell_view);
	mail_shell_sidebar = mail_shell_view->priv->mail_shell_sidebar;
	folder_tree = e_mail_shell_sidebar_get_folder_tree (mail_shell_sidebar);

	tree_view = GTK_TREE_VIEW (folder_tree);
	selection = gtk_tree_view_get_selection (tree_view);
	if (!gtk_tree_selection_get_selected (selection, &model, &iter))
		return;

	gtk_tree_model_get (model, &iter, COL_STRING_URI, &uri, -1);
	em_folder_properties_show (shell_view, NULL, uri);
	g_free (uri);
}

static void
action_mail_folder_refresh_cb (GtkAction *action,
                               EMailShellView *mail_shell_view)
{
	EMailShellSidebar *mail_shell_sidebar;
	EMFolderTree *folder_tree;
	CamelFolder *folder;

	mail_shell_sidebar = mail_shell_view->priv->mail_shell_sidebar;
	folder_tree = e_mail_shell_sidebar_get_folder_tree (mail_shell_sidebar);
	folder = em_folder_tree_get_selected_folder (folder_tree);
	g_return_if_fail (folder != NULL);

	mail_refresh_folder (folder, NULL, NULL);
}

static void
action_mail_folder_rename_cb (GtkAction *action,
                              EMailShellView *mail_shell_view)
{
	EMailShellSidebar *mail_shell_sidebar;
	EMFolderTree *folder_tree;

	mail_shell_sidebar = mail_shell_view->priv->mail_shell_sidebar;
	folder_tree = e_mail_shell_sidebar_get_folder_tree (mail_shell_sidebar);

	em_folder_tree_edit_selected (folder_tree);
}

static void
action_mail_folder_select_thread_cb (GtkAction *action,
                                     EMailShellView *mail_shell_view)
{
	EMailShellContent *mail_shell_content;
	GtkWidget *message_list;
	EMailReader *reader;
	EMailView *mail_view;

	mail_shell_content = mail_shell_view->priv->mail_shell_content;
	mail_view = e_mail_shell_content_get_mail_view (mail_shell_content);

	reader = E_MAIL_READER (mail_view);
	message_list = e_mail_reader_get_message_list (reader);

	message_list_select_thread (MESSAGE_LIST (message_list));
}

static void
action_mail_folder_select_subthread_cb (GtkAction *action,
                                        EMailShellView *mail_shell_view)
{
	EMailShellContent *mail_shell_content;
	GtkWidget *message_list;
	EMailReader *reader;
	EMailView *mail_view;

	mail_shell_content = mail_shell_view->priv->mail_shell_content;
	mail_view = e_mail_shell_content_get_mail_view (mail_shell_content);

	reader = E_MAIL_READER (mail_view);
	message_list = e_mail_reader_get_message_list (reader);

	message_list_select_subthread (MESSAGE_LIST (message_list));
}

static void
action_mail_folder_unsubscribe_cb (GtkAction *action,
                                   EMailShellView *mail_shell_view)
{
	EMailShellSidebar *mail_shell_sidebar;
	EMFolderTree *folder_tree;
	gchar *folder_uri;

	mail_shell_sidebar = mail_shell_view->priv->mail_shell_sidebar;
	folder_tree = e_mail_shell_sidebar_get_folder_tree (mail_shell_sidebar);

	folder_uri = em_folder_tree_get_selected_uri (folder_tree);
	em_folder_utils_unsubscribe_folder (folder_uri);
	g_free (folder_uri);
}

static void
action_mail_global_expunge_cb (GtkAction *action,
                               EMailShellView *mail_shell_view)
{
	EShellWindow *shell_window;
	EShellView *shell_view;

	shell_view = E_SHELL_VIEW (mail_shell_view);
	shell_window = e_shell_view_get_shell_window (shell_view);

	em_utils_empty_trash (GTK_WIDGET (shell_window));
}

static void
action_mail_label_cb (GtkToggleAction *action,
                      EMailShellView *mail_shell_view)
{
	EMailShellContent *mail_shell_content;
	EMailReader *reader;
	EMailView *mail_view;
	CamelFolder *folder;
	GPtrArray *uids;
	const gchar *tag;
	gint ii;

	tag = g_object_get_data (G_OBJECT (action), "tag");
	g_return_if_fail (tag != NULL);

	mail_shell_content = mail_shell_view->priv->mail_shell_content;
	mail_view = e_mail_shell_content_get_mail_view (mail_shell_content);

	reader = E_MAIL_READER (mail_view);
	folder = e_mail_reader_get_folder (reader);
	uids = e_mail_reader_get_selected_uids (reader);

	for (ii = 0; ii < uids->len; ii++) {
		if (gtk_toggle_action_get_active (action))
			camel_folder_set_message_user_flag (
				folder, uids->pdata[ii], tag, TRUE);
		else {
			camel_folder_set_message_user_flag (
				folder, uids->pdata[ii], tag, FALSE);
			camel_folder_set_message_user_tag (
				folder, uids->pdata[ii], "label", NULL);
		}
	}

	em_utils_uids_free (uids);
}

static void
action_mail_label_new_cb (GtkAction *action,
                          EMailShellView *mail_shell_view)
{
	EShell *shell;
	EShellView *shell_view;
	EShellWindow *shell_window;
	EShellSettings *shell_settings;
	EMailShellContent *mail_shell_content;
	EMailLabelDialog *label_dialog;
	EMailLabelListStore *store;
	EMailReader *reader;
	EMailView *mail_view;
	CamelFolder *folder;
	GtkTreeModel *model;
	GtkTreeIter iter;
	GtkWidget *dialog;
	GPtrArray *uids;
	GdkColor label_color;
	const gchar *property_name;
	const gchar *label_name;
	gchar *label_tag;
	gint n_children;
	guint ii;

	shell_view = E_SHELL_VIEW (mail_shell_view);
	shell_window = e_shell_view_get_shell_window (shell_view);

	dialog = e_mail_label_dialog_new (GTK_WINDOW (shell_window));

	gtk_window_set_title (GTK_WINDOW (dialog), _("Add Label"));

	if (gtk_dialog_run (GTK_DIALOG (dialog)) != GTK_RESPONSE_OK)
		goto exit;

	shell = e_shell_window_get_shell (shell_window);
	shell_settings = e_shell_get_shell_settings (shell);

	label_dialog = E_MAIL_LABEL_DIALOG (dialog);
	label_name = e_mail_label_dialog_get_label_name (label_dialog);
	e_mail_label_dialog_get_label_color (label_dialog, &label_color);

	property_name = "mail-label-list-store";
	store = e_shell_settings_get_object (shell_settings, property_name);
	e_mail_label_list_store_set (store, NULL, label_name, &label_color);
	g_object_unref (store);

	/* XXX This is awkward.  We've added a new label to the list store
	 *     but we don't have the new label's tag nor an iterator to use
	 *     to fetch it.  We know the label was appended to the store,
	 *     so we have to dig it out manually.  EMailLabelListStore API
	 *     probably needs some rethinking. */
	model = GTK_TREE_MODEL (store);
	n_children = gtk_tree_model_iter_n_children (model, NULL);
	gtk_tree_model_iter_nth_child (model, &iter, NULL, n_children - 1);
	label_tag = e_mail_label_list_store_get_tag (store, &iter);

	mail_shell_content = mail_shell_view->priv->mail_shell_content;
	mail_view = e_mail_shell_content_get_mail_view (mail_shell_content);

	reader = E_MAIL_READER (mail_view);
	folder = e_mail_reader_get_folder (reader);
	uids = e_mail_reader_get_selected_uids (reader);

	for (ii = 0; ii < uids->len; ii++)
		camel_folder_set_message_user_flag (
			folder, uids->pdata[ii], label_tag, TRUE);

	em_utils_uids_free (uids);

	g_free (label_tag);

exit:
	gtk_widget_destroy (dialog);
}

static void
action_mail_label_none_cb (GtkAction *action,
                           EMailShellView *mail_shell_view)
{
	EShell *shell;
	EShellView *shell_view;
	EShellWindow *shell_window;
	EShellSettings *shell_settings;
	EMailShellContent *mail_shell_content;
	EMailReader *reader;
	EMailView *mail_view;
	GtkTreeModel *tree_model;
	CamelFolder *folder;
	GtkTreeIter iter;
	GPtrArray *uids;
	gboolean valid;
	guint ii;

	shell_view = E_SHELL_VIEW (mail_shell_view);
	shell_window = e_shell_view_get_shell_window (shell_view);
	shell = e_shell_window_get_shell (shell_window);
	shell_settings = e_shell_get_shell_settings (shell);

	tree_model = e_shell_settings_get_object (
		shell_settings, "mail-label-list-store");

	mail_shell_content = mail_shell_view->priv->mail_shell_content;
	mail_view = e_mail_shell_content_get_mail_view (mail_shell_content);

	reader = E_MAIL_READER (mail_view);
	folder = e_mail_reader_get_folder (reader);
	uids = e_mail_reader_get_selected_uids (reader);

	valid = gtk_tree_model_get_iter_first (tree_model, &iter);

	while (valid) {
		gchar *tag;

		tag = e_mail_label_list_store_get_tag (
			E_MAIL_LABEL_LIST_STORE (tree_model), &iter);

		for (ii = 0; ii < uids->len; ii++) {
			camel_folder_set_message_user_flag (
				folder, uids->pdata[ii], tag, FALSE);
			camel_folder_set_message_user_tag (
				folder, uids->pdata[ii], "label", NULL);
		}

		g_free (tag);

		valid = gtk_tree_model_iter_next (tree_model, &iter);
	}

	em_utils_uids_free (uids);
}

static void
action_mail_show_deleted_cb (GtkToggleAction *action,
                             EMailShellView *mail_shell_view)
{
	EMailShellContent *mail_shell_content;
	GtkWidget *message_list;
	EMailReader *reader;
	EMailView *mail_view;
	gboolean active;

	mail_shell_content = mail_shell_view->priv->mail_shell_content;
	mail_view = e_mail_shell_content_get_mail_view (mail_shell_content);

	reader = E_MAIL_READER (mail_view);
	message_list = e_mail_reader_get_message_list (reader);

	active = gtk_toggle_action_get_active (action);
	message_list_set_hidedeleted (MESSAGE_LIST (message_list), !active);
}

static void
action_mail_smart_backward_cb (GtkAction *action,
                               EMailShellView *mail_shell_view)
{
	EShell *shell;
	EShellView *shell_view;
	EShellWindow *shell_window;
	EShellSettings *shell_settings;
	EMailShellContent *mail_shell_content;
	EMailShellSidebar *mail_shell_sidebar;
	EMFolderTree *folder_tree;
	EMFormatHTML *formatter;
	EMailReader *reader;
	EMailView *mail_view;
	GtkWidget *message_list;
	GtkToggleAction *toggle_action;
	EWebView *web_view;
	gboolean caret_mode;
	gboolean magic_spacebar;

	/* This implements the so-called "Magic Backspace". */

	shell_view = E_SHELL_VIEW (mail_shell_view);
	shell_window = e_shell_view_get_shell_window (shell_view);
	shell = e_shell_window_get_shell (shell_window);
	shell_settings = e_shell_get_shell_settings (shell);

	mail_shell_content = mail_shell_view->priv->mail_shell_content;
	mail_view = e_mail_shell_content_get_mail_view (mail_shell_content);

	mail_shell_sidebar = mail_shell_view->priv->mail_shell_sidebar;
	folder_tree = e_mail_shell_sidebar_get_folder_tree (mail_shell_sidebar);

	reader = E_MAIL_READER (mail_view);
	formatter = e_mail_reader_get_formatter (reader);
	message_list = e_mail_reader_get_message_list (reader);

	magic_spacebar = e_shell_settings_get_boolean (
		shell_settings, "mail-magic-spacebar");

	toggle_action = GTK_TOGGLE_ACTION (ACTION (MAIL_CARET_MODE));
	caret_mode = gtk_toggle_action_get_active (toggle_action);

	web_view = em_format_html_get_web_view (formatter);

	if (e_web_view_scroll_backward (web_view))
		return;

	if (caret_mode || !magic_spacebar)
		return;

	/* XXX Are two separate calls really necessary? */

	if (message_list_select (
		MESSAGE_LIST (message_list),
		MESSAGE_LIST_SELECT_PREVIOUS,
		0, CAMEL_MESSAGE_SEEN))
		return;

	if (message_list_select (
		MESSAGE_LIST (message_list),
		MESSAGE_LIST_SELECT_PREVIOUS |
		MESSAGE_LIST_SELECT_WRAP, 0,
		CAMEL_MESSAGE_SEEN))
		return;

	em_folder_tree_select_prev_path (folder_tree, TRUE);

	gtk_widget_grab_focus (message_list);
}

static void
action_mail_smart_forward_cb (GtkAction *action,
                              EMailShellView *mail_shell_view)
{
	EShell *shell;
	EShellView *shell_view;
	EShellWindow *shell_window;
	EShellSettings *shell_settings;
	EMailShellContent *mail_shell_content;
	EMailShellSidebar *mail_shell_sidebar;
	EMFolderTree *folder_tree;
	EMFormatHTML *formatter;
	EMailReader *reader;
	EMailView *mail_view;
	GtkWidget *message_list;
	GtkToggleAction *toggle_action;
	EWebView *web_view;
	gboolean caret_mode;
	gboolean magic_spacebar;

	/* This implements the so-called "Magic Spacebar". */

	shell_view = E_SHELL_VIEW (mail_shell_view);
	shell_window = e_shell_view_get_shell_window (shell_view);
	shell = e_shell_window_get_shell (shell_window);
	shell_settings = e_shell_get_shell_settings (shell);

	mail_shell_content = mail_shell_view->priv->mail_shell_content;
	mail_view = e_mail_shell_content_get_mail_view (mail_shell_content);

	mail_shell_sidebar = mail_shell_view->priv->mail_shell_sidebar;
	folder_tree = e_mail_shell_sidebar_get_folder_tree (mail_shell_sidebar);

	reader = E_MAIL_READER (mail_view);
	formatter = e_mail_reader_get_formatter (reader);
	message_list = e_mail_reader_get_message_list (reader);

	magic_spacebar = e_shell_settings_get_boolean (
		shell_settings, "mail-magic-spacebar");

	toggle_action = GTK_TOGGLE_ACTION (ACTION (MAIL_CARET_MODE));
	caret_mode = gtk_toggle_action_get_active (toggle_action);

	web_view = em_format_html_get_web_view (formatter);

	if (e_web_view_scroll_forward (web_view))
		return;

	if (caret_mode || !magic_spacebar)
		return;

	/* XXX Are two separate calls really necessary? */

	if (message_list_select (
		MESSAGE_LIST (message_list),
		MESSAGE_LIST_SELECT_NEXT,
		0, CAMEL_MESSAGE_SEEN))
		return;

	if (message_list_select (
		MESSAGE_LIST (message_list),
		MESSAGE_LIST_SELECT_NEXT |
		MESSAGE_LIST_SELECT_WRAP,
		0, CAMEL_MESSAGE_SEEN))
		return;

	em_folder_tree_select_next_path (folder_tree, TRUE);

	gtk_widget_grab_focus (message_list);
}

static void
action_mail_stop_cb (GtkAction *action,
                     EMailShellView *mail_shell_view)
{
	mail_cancel_all ();
}

static void
action_mail_threads_collapse_all_cb (GtkAction *action,
                                     EMailShellView *mail_shell_view)
{
	EMailShellContent *mail_shell_content;
	GtkWidget *message_list;
	EMailReader *reader;
	EMailView *mail_view;

	mail_shell_content = mail_shell_view->priv->mail_shell_content;
	mail_view = e_mail_shell_content_get_mail_view (mail_shell_content);

	reader = E_MAIL_READER (mail_view);
	message_list = e_mail_reader_get_message_list (reader);

	message_list_set_threaded_collapse_all (MESSAGE_LIST (message_list));
}

static void
action_mail_threads_expand_all_cb (GtkAction *action,
                                   EMailShellView *mail_shell_view)
{
	EMailShellContent *mail_shell_content;
	GtkWidget *message_list;
	EMailReader *reader;
	EMailView *mail_view;

	mail_shell_content = mail_shell_view->priv->mail_shell_content;
	mail_view = e_mail_shell_content_get_mail_view (mail_shell_content);

	reader = E_MAIL_READER (mail_view);
	message_list = e_mail_reader_get_message_list (reader);

	message_list_set_threaded_expand_all (MESSAGE_LIST (message_list));
}

static void
action_mail_tools_filters_cb (GtkAction *action,
                              EMailShellView *mail_shell_view)
{
	EShellWindow *shell_window;
	EShellView *shell_view;

	shell_view = E_SHELL_VIEW (mail_shell_view);
	shell_window = e_shell_view_get_shell_window (shell_view);

	em_utils_edit_filters (GTK_WIDGET (shell_window));
}

static void
action_mail_tools_search_folders_cb (GtkAction *action,
                                     EMailShellView *mail_shell_view)
{
	vfolder_edit (E_SHELL_VIEW (mail_shell_view));
}

static void
action_mail_tools_subscriptions_cb (GtkAction *action,
                                    EMailShellView *mail_shell_view)
{
	EShellWindow *shell_window;
	EShellView *shell_view;
	GtkWidget *dialog;

	shell_view = E_SHELL_VIEW (mail_shell_view);
	shell_window = e_shell_view_get_shell_window (shell_view);

	dialog = em_subscribe_editor_new ();
	gtk_window_set_transient_for (
		GTK_WINDOW (dialog), GTK_WINDOW (shell_window));
	gtk_dialog_run (GTK_DIALOG (dialog));
	/* XXX Dialog destroys itself. */
}

static void
action_mail_view_cb (GtkRadioAction *action,
                     GtkRadioAction *current,
                     EMailShellView *mail_shell_view)
{
	EMailShellContent *mail_shell_content;
	GtkOrientation orientation;
	EMailView *mail_view;

	mail_shell_content = mail_shell_view->priv->mail_shell_content;
	mail_view = e_mail_shell_content_get_mail_view (mail_shell_content);

	switch (gtk_radio_action_get_current_value (action)) {
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
}

static GtkActionEntry mail_entries[] = {

	{ "mail-account-disable",
	  NULL,
	  N_("_Disable Account"),
	  NULL,
	  N_("Disable this account"),
	  G_CALLBACK (action_mail_account_disable_cb) },

	{ "mail-account-expunge",
	  NULL,
	  N_("_Empty Trash"),
	  NULL,
	  N_("Permanently remove all the deleted messages from all folders"),
	  G_CALLBACK (action_mail_folder_expunge_cb) },

	{ "mail-create-search-folder",
	  NULL,
	  N_("C_reate Search Folder From Search..."),
	  NULL,
	  NULL,  /* XXX Add a tooltip! */
	  G_CALLBACK (action_mail_create_search_folder_cb) },

	{ "mail-download",
	  NULL,
	  N_("_Download Messages for Offline Usage"),
	  NULL,
	  N_("Download messages of accounts and folders marked for offline usage"),
	  G_CALLBACK (action_mail_download_cb) },

	{ "mail-flush-outbox",
	  "mail-send",
	  N_("Fl_ush Outbox"),
	  NULL,
	  NULL,  /* XXX Add a tooltip! */
	  G_CALLBACK (action_mail_flush_outbox_cb) },

	{ "mail-folder-copy",
	  "folder-copy",
	  N_("_Copy Folder To..."),
	  NULL,
	  N_("Copy the selected folder into another folder"),
	  G_CALLBACK (action_mail_folder_copy_cb) },

	{ "mail-folder-delete",
	  GTK_STOCK_DELETE,
	  NULL,
	  NULL,
	  N_("Permanently remove this folder"),
	  G_CALLBACK (action_mail_folder_delete_cb) },

	{ "mail-folder-expunge",
	  NULL,
	  N_("E_xpunge"),
	  "<Control>e",
	  N_("Permanently remove all deleted messages from this folder"),
	  G_CALLBACK (action_mail_folder_expunge_cb) },

	{ "mail-folder-mark-all-as-read",
	  "mail-read",
	  N_("Mar_k All Messages as Read"),
	  "<Control>slash",
	  N_("Mark all messages in the folder as read"),
	  G_CALLBACK (action_mail_folder_mark_all_as_read_cb) },

	{ "mail-folder-move",
	  "folder-move",
	  N_("_Move Folder To..."),
	  NULL,
	  N_("Move the selected folder into another folder"),
	  G_CALLBACK (action_mail_folder_move_cb) },

	{ "mail-folder-new",
	  "folder-new",
	  N_("_New..."),
	  NULL,
	  N_("Create a new folder for storing mail"),
	  G_CALLBACK (action_mail_folder_new_cb) },

	{ "mail-folder-properties",
	  GTK_STOCK_PROPERTIES,
	  NULL,
	  NULL,
	  N_("Change the properties of this folder"),
	  G_CALLBACK (action_mail_folder_properties_cb) },

	{ "mail-folder-refresh",
	  GTK_STOCK_REFRESH,
	  NULL,
	  "F5",
	  N_("Refresh the folder"),
	  G_CALLBACK (action_mail_folder_refresh_cb) },

	{ "mail-folder-rename",
	  NULL,
	  N_("_Rename..."),
	  "F2",
	  N_("Change the name of this folder"),
	  G_CALLBACK (action_mail_folder_rename_cb) },

	{ "mail-folder-select-thread",
	  NULL,
	  N_("Select Message _Thread"),
	  "<Control>h",
	  N_("Select all messages in the same thread as the selected message"),
	  G_CALLBACK (action_mail_folder_select_thread_cb) },

	{ "mail-folder-select-subthread",
	  NULL,
	  N_("Select Message S_ubthread"),
	  "<Shift><Control>h",
	  N_("Select all replies to the currently selected message"),
	  G_CALLBACK (action_mail_folder_select_subthread_cb) },

	{ "mail-folder-unsubscribe",
	  NULL,
	  N_("_Unsubscribe"),
	  NULL,
	  N_("Unsubscribe from the selected folder"),
	  G_CALLBACK (action_mail_folder_unsubscribe_cb) },

	{ "mail-global-expunge",
	  NULL,
	  N_("Empty _Trash"),
	  NULL,
	  N_("Permanently remove all the deleted messages from all accounts"),
	  G_CALLBACK (action_mail_global_expunge_cb) },

	{ "mail-label-new",
	  NULL,
	  N_("_New Label"),
	  NULL,
	  NULL,  /* XXX Add a tooltip! */
	  G_CALLBACK (action_mail_label_new_cb) },

	/* Translators: "None" is used in the message label context menu.
	 *              It removes all labels from the selected messages. */
	{ "mail-label-none",
	  NULL,
	  N_("N_one"),
	  NULL,
	  NULL,  /* XXX Add a tooltip! */
	  G_CALLBACK (action_mail_label_none_cb) },

	{ "mail-smart-backward",
	  NULL,
	  NULL,  /* No menu item; key press only */
	  NULL,
	  NULL,
	  G_CALLBACK (action_mail_smart_backward_cb) },

	{ "mail-smart-forward",
	  NULL,
	  NULL,  /* No menu item; key press only */
	  NULL,
	  NULL,
	  G_CALLBACK (action_mail_smart_forward_cb) },

	{ "mail-stop",
	  GTK_STOCK_STOP,
	  N_("Cancel"),
	  NULL,
	  N_("Cancel the current mail operation"),
	  G_CALLBACK (action_mail_stop_cb) },

	{ "mail-threads-collapse-all",
	  NULL,
	  N_("Collapse All _Threads"),
	  "<Shift><Control>b",
	  N_("Collapse all message threads"),
	  G_CALLBACK (action_mail_threads_collapse_all_cb) },

	{ "mail-threads-expand-all",
	  NULL,
	  N_("E_xpand All Threads"),
	  NULL,
	  N_("Expand all message threads"),
	  G_CALLBACK (action_mail_threads_expand_all_cb) },

	{ "mail-tools-filters",
	  NULL,
	  N_("_Message Filters"),
	  NULL,
	  N_("Create or edit rules for filtering new mail"),
	  G_CALLBACK (action_mail_tools_filters_cb) },

	{ "mail-tools-search-folders",
	  NULL,
	  N_("Search F_olders"),
	  NULL,
	  N_("Create or edit search folder definitions"),
	  G_CALLBACK (action_mail_tools_search_folders_cb) },

	{ "mail-tools-subscriptions",
	  NULL,
	  N_("_Subscriptions..."),
	  NULL,
	  N_("Subscribe or unsubscribe to folders on remote servers"),
	  G_CALLBACK (action_mail_tools_subscriptions_cb) },

	/*** Menus ***/

	{ "mail-folder-menu",
	  NULL,
	  N_("F_older"),
	  NULL,
	  NULL,
	  NULL },

	{ "mail-label-menu",
	  NULL,
	  N_("_Label"),
	  NULL,
	  NULL,
	  NULL },

	{ "mail-preview-menu",
	  NULL,
	  N_("_Preview"),
	  NULL,
	  NULL,
	  NULL }
};

static EPopupActionEntry mail_popup_entries[] = {

	{ "mail-popup-account-disable",
	  NULL,
	  "mail-account-disable" },

	{ "mail-popup-account-expunge",
	  NULL,
	  "mail-account-expunge" },

	{ "mail-popup-flush-outbox",
	  NULL,
	  "mail-flush-outbox" },

	{ "mail-popup-folder-copy",
	  NULL,
	  "mail-folder-copy" },

	{ "mail-popup-folder-delete",
	  NULL,
	  "mail-folder-delete" },

	{ "mail-popup-folder-move",
	  NULL,
	  "mail-folder-move" },

	{ "mail-popup-folder-new",
	  N_("_New Folder..."),
	  "mail-folder-new" },

	{ "mail-popup-folder-properties",
	  NULL,
	  "mail-folder-properties" },

	{ "mail-popup-folder-refresh",
	  NULL,
	  "mail-folder-refresh" },

	{ "mail-popup-folder-rename",
	  NULL,
	  "mail-folder-rename" },

	{ "mail-popup-folder-unsubscribe",
	  NULL,
	  "mail-folder-unsubscribe" }
};

static GtkToggleActionEntry mail_toggle_entries[] = {

	{ "mail-preview",
	  NULL,
	  N_("Show Message _Preview"),
	  "<Control>m",
	  N_("Show message preview pane"),
	  NULL,  /* Handled by property bindings */
	  TRUE },

	{ "mail-show-deleted",
	  NULL,
	  N_("Show _Deleted Messages"),
	  NULL,
	  N_("Show deleted messages with a line through them"),
	  G_CALLBACK (action_mail_show_deleted_cb),
	  FALSE },

	{ "mail-threads-group-by",
	  NULL,
	  N_("_Group By Threads"),
	  "<Control>t",
	  N_("Threaded message list"),
	  NULL,  /* Handled by property bindings */
	  FALSE }
};

static GtkRadioActionEntry mail_view_entries[] = {

	/* This action represents the initial active mail view.
	 * It should not be visible in the UI, nor should it be
	 * possible to switch to it from another shell view. */
	{ "mail-view-initial",
	  NULL,
	  NULL,
	  NULL,
	  NULL,
	  -1 },

	{ "mail-view-classic",
	  NULL,
	  N_("_Classic View"),
	  NULL,
	  N_("Show message preview below the message list"),
	  0 },

	{ "mail-view-vertical",
	  NULL,
	  N_("_Vertical View"),
	  NULL,
	  N_("Show message preview alongside the message list"),
	  1 }
};

static GtkRadioActionEntry mail_filter_entries[] = {

	{ "mail-filter-all-messages",
	  NULL,
	  N_("All Messages"),
	  NULL,
	  NULL,  /* XXX Add a tooltip! */
	  MAIL_FILTER_ALL_MESSAGES },

	{ "mail-filter-important-messages",
	  "emblem-important",
	  N_("Important Messages"),
	  NULL,
	  NULL,  /* XXX Add a tooltip! */
	  MAIL_FILTER_IMPORTANT_MESSAGES },

	{ "mail-filter-last-5-days-messages",
	  NULL,
	  N_("Last 5 Days' Messages"),
	  NULL,
	  NULL,  /* XXX Add a tooltip! */
	  MAIL_FILTER_LAST_5_DAYS_MESSAGES },

	{ "mail-filter-messages-not-junk",
	  "mail-mark-notjunk",
	  N_("Messages Not Junk"),
	  NULL,
	  NULL,  /* XXX Add a tooltip! */
	  MAIL_FILTER_MESSAGES_NOT_JUNK },

	{ "mail-filter-messages-with-attachments",
	  "mail-attachment",
	  N_("Messages with Attachments"),
	  NULL,
	  NULL,  /* XXX Add a tooltip! */
	  MAIL_FILTER_MESSAGES_WITH_ATTACHMENTS },

	{ "mail-filter-no-label",
	  NULL,
	  N_("No Label"),
	  NULL,
	  NULL,  /* XXX Add a tooltip! */
	  MAIL_FILTER_NO_LABEL },

	{ "mail-filter-read-messages",
	  "mail-read",
	  N_("Read Messages"),
	  NULL,
	  NULL,  /* XXX Add a tooltip! */
	  MAIL_FILTER_READ_MESSAGES },

	{ "mail-filter-recent-messages",
	  NULL,
	  N_("Recent Messages"),
	  NULL,
	  NULL,  /* XXX Add a tooltip! */
	  MAIL_FILTER_RECENT_MESSAGES },

	{ "mail-filter-unread-messages",
	  "mail-unread",
	  N_("Unread Messages"),
	  NULL,
	  NULL,  /* XXX Add a tooltip! */
	  MAIL_FILTER_UNREAD_MESSAGES }
};

static GtkRadioActionEntry mail_search_entries[] = {

	{ "mail-search-advanced-hidden",
	  NULL,
	  N_("Advanced Search"),
	  NULL,
	  NULL,
	  MAIL_SEARCH_ADVANCED },

	{ "mail-search-body-contains",
	  NULL,
	  N_("Body contains"),
	  NULL,
	  NULL,  /* XXX Add a tooltip! */
	  MAIL_SEARCH_BODY_CONTAINS },

	{ "mail-search-message-contains",
	  NULL,
	  N_("Message contains"),
	  NULL,
	  NULL,  /* XXX Add a tooltip! */
	  MAIL_SEARCH_MESSAGE_CONTAINS },

	{ "mail-search-recipients-contain",
	  NULL,
	  N_("Recipients contain"),
	  NULL,
	  NULL,  /* XXX Add a tooltip! */
	  MAIL_SEARCH_RECIPIENTS_CONTAIN },

	{ "mail-search-sender-contains",
	  NULL,
	  N_("Sender contains"),
	  NULL,
	  NULL,  /* XXX Add a tooltip! */
	  MAIL_SEARCH_SENDER_CONTAINS },

	{ "mail-search-subject-contains",
	  NULL,
	  N_("Subject contains"),
	  NULL,
	  NULL,  /* XXX Add a tooltip! */
	  MAIL_SEARCH_SUBJECT_CONTAINS },

	{ "mail-search-subject-or-addresses-contain",
	  NULL,
	  N_("Subject or Addresses contain"),
	  NULL,
	  NULL,  /* XXX Add a tooltip! */
	  MAIL_SEARCH_SUBJECT_OR_ADDRESSES_CONTAIN }
};

static GtkRadioActionEntry mail_scope_entries[] = {

	{ "mail-scope-all-accounts",
	  NULL,
	  N_("All Accounts"),
	  NULL,
	  NULL,  /* XXX Add a tooltip! */
	  MAIL_SCOPE_ALL_ACCOUNTS },

	{ "mail-scope-current-account",
	  NULL,
	  N_("Current Account"),
	  NULL,
	  NULL,  /* XXX Add a tooltip! */
	  MAIL_SCOPE_CURRENT_ACCOUNT },

	{ "mail-scope-current-folder",
	  NULL,
	  N_("Current Folder"),
	  NULL,
	  NULL,  /* XXX Add a tooltip! */
	  MAIL_SCOPE_CURRENT_FOLDER }
};

void
e_mail_shell_view_actions_init (EMailShellView *mail_shell_view)
{
	EMailShellContent *mail_shell_content;
	EShellView *shell_view;
	EShellWindow *shell_window;
	EShell *shell;
	EShellSearchbar *searchbar;
	EActionComboBox *combo_box;
	EMailView *mail_view;
	GtkActionGroup *action_group;
	GtkAction *action;
	GConfBridge *bridge;
	GObject *object;
	const gchar *key;

	g_return_if_fail (E_IS_MAIL_SHELL_VIEW (mail_shell_view));

	shell_view = E_SHELL_VIEW (mail_shell_view);
	shell_window = e_shell_view_get_shell_window (shell_view);
	shell = e_shell_window_get_shell (shell_window);

	mail_shell_content = mail_shell_view->priv->mail_shell_content;
	mail_view = e_mail_shell_content_get_mail_view (mail_shell_content);
	searchbar = e_mail_shell_content_get_searchbar (mail_shell_content);

	/* Mail Actions */
	action_group = ACTION_GROUP (MAIL);
	gtk_action_group_add_actions (
		action_group, mail_entries,
		G_N_ELEMENTS (mail_entries), mail_shell_view);
	e_action_group_add_popup_actions (
		action_group, mail_popup_entries,
		G_N_ELEMENTS (mail_popup_entries));
	gtk_action_group_add_toggle_actions (
		action_group, mail_toggle_entries,
		G_N_ELEMENTS (mail_toggle_entries), mail_shell_view);
	gtk_action_group_add_radio_actions (
		action_group, mail_view_entries,
		G_N_ELEMENTS (mail_view_entries), -1,
		G_CALLBACK (action_mail_view_cb), mail_shell_view);
	gtk_action_group_add_radio_actions (
		action_group, mail_search_entries,
		G_N_ELEMENTS (mail_search_entries),
		-1, NULL, NULL);
	gtk_action_group_add_radio_actions (
		action_group, mail_scope_entries,
		G_N_ELEMENTS (mail_scope_entries),
		MAIL_SCOPE_CURRENT_FOLDER, NULL, NULL);

	action = ACTION (MAIL_SCOPE_ALL_ACCOUNTS);
	combo_box = e_shell_searchbar_get_scope_combo_box (searchbar);
	e_action_combo_box_set_action (combo_box, GTK_RADIO_ACTION (action));
	e_shell_searchbar_set_scope_visible (searchbar, TRUE);

	/* Advanced Search Action */
	action = ACTION (MAIL_SEARCH_ADVANCED_HIDDEN);
	gtk_action_set_visible (action, FALSE);
	e_shell_searchbar_set_search_option (
		searchbar, GTK_RADIO_ACTION (action));

	/* Bind GObject properties for GConf keys. */

	bridge = gconf_bridge_get ();

	object = G_OBJECT (ACTION (MAIL_SHOW_DELETED));
	key = "/apps/evolution/mail/display/show_deleted";
	gconf_bridge_bind_property (bridge, key, object, "active");

	object = G_OBJECT (ACTION (MAIL_VIEW_VERTICAL));
	key = "/apps/evolution/mail/display/layout";
	gconf_bridge_bind_property (bridge, key, object, "current-value");

	/* Fine tuning. */

	e_binding_new (
		ACTION (MAIL_THREADS_GROUP_BY), "active",
		ACTION (MAIL_FOLDER_SELECT_THREAD), "sensitive");

	e_binding_new (
		ACTION (MAIL_THREADS_GROUP_BY), "active",
		ACTION (MAIL_FOLDER_SELECT_SUBTHREAD), "sensitive");

	e_binding_new (
		ACTION (MAIL_THREADS_GROUP_BY), "active",
		ACTION (MAIL_THREADS_COLLAPSE_ALL), "sensitive");

	e_binding_new (
		ACTION (MAIL_THREADS_GROUP_BY), "active",
		ACTION (MAIL_THREADS_EXPAND_ALL), "sensitive");

	e_mutual_binding_new (
		ACTION (MAIL_PREVIEW), "active",
		mail_view, "preview-visible");

	e_mutual_binding_new (
		ACTION (MAIL_THREADS_GROUP_BY), "active",
		mail_shell_content, "group-by-threads");

	e_binding_new (
		ACTION (MAIL_PREVIEW), "active",
		ACTION (MAIL_VIEW_CLASSIC), "sensitive");

	e_binding_new (
		ACTION (MAIL_PREVIEW), "active",
		ACTION (MAIL_VIEW_VERTICAL), "sensitive");

	e_mutual_binding_new (
		ACTION (MAIL_SHOW_DELETED), "active",
		mail_view, "show-deleted");

	/* Keep the sensitivity of "Create Search Folder from Search"
	 * in sync with "Save Search" so that its only selectable when
	 * showing search results. */
	e_binding_new (
		ACTION (SEARCH_SAVE), "sensitive",
		ACTION (MAIL_CREATE_SEARCH_FOLDER), "sensitive");

	e_binding_new (
		shell, "online",
		ACTION (MAIL_DOWNLOAD), "sensitive");

	g_signal_connect (
		ACTION (GAL_SAVE_CUSTOM_VIEW), "activate",
		G_CALLBACK (action_gal_save_custom_view_cb), mail_shell_view);
}

/* Helper for e_mail_shell_view_update_popup_labels() */
static void
mail_shell_view_update_label_action (GtkToggleAction *action,
                                     EMailReader *reader,
                                     GPtrArray *uids,
                                     const gchar *label_tag)
{
	CamelFolder *folder;
	gboolean exists = FALSE;
	gboolean not_exists = FALSE;
	gboolean sensitive;
	guint ii;

	folder = e_mail_reader_get_folder (reader);

	/* Figure out the proper label action state for the selected
	 * messages.  If all the selected messages have the given label,
	 * make the toggle action active.  If all the selected message
	 * DO NOT have the given label, make the toggle action inactive.
	 * If some do and some don't, make the action insensitive. */

	for (ii = 0; ii < uids->len && (!exists || !not_exists); ii++) {
		const gchar *old_label;
		gchar *new_label;

		/* Check for new-style labels. */
		if (camel_folder_get_message_user_flag (
			folder, uids->pdata[ii], label_tag)) {
			exists = TRUE;
			continue;
		}

		/* Check for old-style labels. */
		old_label = camel_folder_get_message_user_tag (
			folder, uids->pdata[ii], "label");
		if (old_label == NULL) {
			not_exists = TRUE;
			continue;
		}

		/* Convert old-style labels ("<name>") to "$Label<name>". */
		new_label = g_alloca (strlen (old_label) + 10);
		g_stpcpy (g_stpcpy (new_label, "$Label"), old_label);

		if (strcmp (new_label, label_tag) == 0)
			exists = TRUE;
		else
			not_exists = TRUE;
	}

	sensitive = !(exists && not_exists);
	gtk_toggle_action_set_active (action, exists);
	gtk_action_set_sensitive (GTK_ACTION (action), sensitive);
}

void
e_mail_shell_view_update_popup_labels (EMailShellView *mail_shell_view)
{
	EShell *shell;
	EShellView *shell_view;
	EShellWindow *shell_window;
	EShellSettings *shell_settings;
	EMailShellContent *mail_shell_content;
	EMailReader *reader;
	EMailView *mail_view;
	GtkUIManager *ui_manager;
	GtkActionGroup *action_group;
	GtkTreeModel *tree_model;
	GtkTreeIter iter;
	GPtrArray *uids;
	const gchar *path;
	gboolean valid;
	guint merge_id;
	gint ii = 0;

	g_return_if_fail (E_IS_MAIL_SHELL_VIEW (mail_shell_view));

	shell_view = E_SHELL_VIEW (mail_shell_view);
	shell_window = e_shell_view_get_shell_window (shell_view);
	ui_manager = e_shell_window_get_ui_manager (shell_window);

	shell = e_shell_window_get_shell (shell_window);
	shell_settings = e_shell_get_shell_settings (shell);

	tree_model = e_shell_settings_get_object (
		shell_settings, "mail-label-list-store");

	action_group = ACTION_GROUP (MAIL_LABEL);
	merge_id = mail_shell_view->priv->label_merge_id;
	path = "/mail-message-popup/mail-label-menu/mail-label-actions";

	/* Unmerge the previous menu items. */
	gtk_ui_manager_remove_ui (ui_manager, merge_id);
	e_action_group_remove_all_actions (action_group);

	mail_shell_content = mail_shell_view->priv->mail_shell_content;
	mail_view = e_mail_shell_content_get_mail_view (mail_shell_content);

	reader = E_MAIL_READER (mail_view);
	uids = e_mail_reader_get_selected_uids (reader);

	valid = gtk_tree_model_get_iter_first (tree_model, &iter);

	while (valid) {
		EMailLabelAction *label_action;
		GtkAction *action;
		gchar *action_name;
		gchar *stock_id;
		gchar *label;
		gchar *tag;

		label = e_mail_label_list_store_get_name (
			E_MAIL_LABEL_LIST_STORE (tree_model), &iter);
		stock_id = e_mail_label_list_store_get_stock_id (
			E_MAIL_LABEL_LIST_STORE (tree_model), &iter);
		tag = e_mail_label_list_store_get_tag (
			E_MAIL_LABEL_LIST_STORE (tree_model), &iter);
		action_name = g_strdup_printf ("mail-label-%d", ii);

		/* XXX Add a tooltip! */
		label_action = e_mail_label_action_new (
			action_name, label, NULL, stock_id);

		g_object_set_data_full (
			G_OBJECT (label_action), "tag",
			tag, (GDestroyNotify) g_free);

		/* Configure the action before we connect to signals. */
		mail_shell_view_update_label_action (
			GTK_TOGGLE_ACTION (label_action),
			reader, uids, tag);

		g_signal_connect (
			label_action, "toggled",
			G_CALLBACK (action_mail_label_cb), mail_shell_view);

		/* The action group takes ownership of the action. */
		action = GTK_ACTION (label_action);
		gtk_action_group_add_action (action_group, action);
		g_object_unref (label_action);

		gtk_ui_manager_add_ui (
			ui_manager, merge_id, path, action_name,
			action_name, GTK_UI_MANAGER_AUTO, FALSE);

		g_free (label);
		g_free (stock_id);
		g_free (action_name);

		valid = gtk_tree_model_iter_next (tree_model, &iter);
		ii++;
	}

	em_utils_uids_free (uids);

	g_object_unref (tree_model);
}

void
e_mail_shell_view_update_search_filter (EMailShellView *mail_shell_view)
{
	EMailShellContent *mail_shell_content;
	EShell *shell;
	EShellView *shell_view;
	EShellWindow *shell_window;
	EShellSettings *shell_settings;
	EShellSearchbar *searchbar;
	EActionComboBox *combo_box;
	GtkActionGroup *action_group;
	GtkRadioAction *radio_action;
	GtkTreeModel *tree_model;
	GtkTreeIter iter;
	GList *list;
	GSList *group;
	gboolean valid;
	gint ii = 0;

	g_return_if_fail (E_IS_MAIL_SHELL_VIEW (mail_shell_view));

	shell_view = E_SHELL_VIEW (mail_shell_view);
	shell_window = e_shell_view_get_shell_window (shell_view);

	shell = e_shell_window_get_shell (shell_window);
	shell_settings = e_shell_get_shell_settings (shell);

	tree_model = e_shell_settings_get_object (
		shell_settings, "mail-label-list-store");

	action_group = ACTION_GROUP (MAIL_FILTER);
	e_action_group_remove_all_actions (action_group);

	/* Add the standard filter actions.  No callback is needed
	 * because changes in the EActionComboBox are detected and
	 * handled by EShellSearchbar. */
	gtk_action_group_add_radio_actions (
		action_group, mail_filter_entries,
		G_N_ELEMENTS (mail_filter_entries),
		MAIL_FILTER_ALL_MESSAGES, NULL, NULL);

	/* Retrieve the radio group from an action we just added. */
	list = gtk_action_group_list_actions (action_group);
	radio_action = GTK_RADIO_ACTION (list->data);
	group = gtk_radio_action_get_group (radio_action);
	g_list_free (list);

	valid = gtk_tree_model_get_iter_first (tree_model, &iter);

	while (valid) {
		GtkAction *action;
		gchar *action_name;
		gchar *stock_id;
		gchar *label;

		label = e_mail_label_list_store_get_name (
			E_MAIL_LABEL_LIST_STORE (tree_model), &iter);
		stock_id = e_mail_label_list_store_get_stock_id (
			E_MAIL_LABEL_LIST_STORE (tree_model), &iter);

		action_name = g_strdup_printf ("mail-filter-label-%d", ii);
		radio_action = gtk_radio_action_new (
			action_name, label, NULL, stock_id, ii);
		g_free (action_name);

		gtk_radio_action_set_group (radio_action, group);
		group = gtk_radio_action_get_group (radio_action);

		/* The action group takes ownership of the action. */
		action = GTK_ACTION (radio_action);
		gtk_action_group_add_action (action_group, action);
		g_object_unref (radio_action);

		g_free (label);
		g_free (stock_id);

		valid = gtk_tree_model_iter_next (tree_model, &iter);
		ii++;
	}

	mail_shell_content = mail_shell_view->priv->mail_shell_content;
	searchbar = e_mail_shell_content_get_searchbar (mail_shell_content);
	combo_box = e_shell_searchbar_get_filter_combo_box (searchbar);

	e_shell_view_block_execute_search (shell_view);

	/* Use any action in the group; doesn't matter which. */
	e_action_combo_box_set_action (combo_box, radio_action);

	ii = MAIL_FILTER_UNREAD_MESSAGES;
	e_action_combo_box_add_separator_after (combo_box, ii);

	ii = MAIL_FILTER_READ_MESSAGES;
	e_action_combo_box_add_separator_after (combo_box, ii);

	e_shell_view_unblock_execute_search (shell_view);

	g_object_unref (tree_model);
}
