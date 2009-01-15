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

#include "e-mail-shell-view-private.h"

static void
action_mail_create_search_folder_cb (GtkAction *action,
                                     EMailShellView *mail_shell_view)
{
	/* FIXME */
	g_print ("Action: %s\n", gtk_action_get_name (GTK_ACTION (action)));
}

static void
action_mail_download_cb (GtkAction *action,
                         EMailShellView *mail_shell_view)
{
	/* FIXME */
	g_print ("Action: %s\n", gtk_action_get_name (GTK_ACTION (action)));
}

static void
action_mail_empty_trash_cb (GtkAction *action,
                            EMailShellView *mail_shell_view)
{
	EShellWindow *shell_window;
	EShellView *shell_view;

	shell_view = E_SHELL_VIEW (mail_shell_view);
	shell_window = e_shell_view_get_shell_window (shell_view);

	em_utils_empty_trash (GTK_WIDGET (shell_window));
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
	EMailShellSidebar *mail_shell_sidebar;
	CamelFolderInfo *folder_info;
	EMFolderTree *folder_tree;

	mail_shell_sidebar = mail_shell_view->priv->mail_shell_sidebar;
	folder_tree = e_mail_shell_sidebar_get_folder_tree (mail_shell_sidebar);
	folder_info = em_folder_tree_get_selected_folder_info (folder_tree);
	g_return_if_fail (folder_info != NULL);

	/* XXX Leaking folder_info? */
	em_folder_utils_copy_folder (folder_info, FALSE);
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
	EMailReader *reader;
	MessageList *message_list;
	EShellWindow *shell_window;
	EShellView *shell_view;

	shell_view = E_SHELL_VIEW (mail_shell_view);
	shell_window = e_shell_view_get_shell_window (shell_view);

	reader = E_MAIL_READER (mail_shell_view->priv->mail_shell_content);
	message_list = e_mail_reader_get_message_list (reader);
	g_return_if_fail (message_list->folder != NULL);

	em_utils_expunge_folder (
		GTK_WIDGET (shell_window), message_list->folder);
}

static void
action_mail_folder_mark_all_as_read_cb (GtkAction *action,
                                        EMailShellView *mail_shell_view)
{
	EMailReader *reader;
	MessageList *message_list;
	EShellWindow *shell_window;
	EShellView *shell_view;
	CamelFolder *folder;
	GtkWindow *parent;
	GPtrArray *uids;
	const gchar *key;
	const gchar *prompt;
	guint ii;

	shell_view = E_SHELL_VIEW (mail_shell_view);
	shell_window = e_shell_view_get_shell_window (shell_view);
	parent = GTK_WINDOW (shell_window);

	reader = E_MAIL_READER (mail_shell_view->priv->mail_shell_content);
	message_list = e_mail_reader_get_message_list (reader);
	folder = message_list->folder;
	g_return_if_fail (folder != NULL);

	key = "/apps/evolution/mail/prompts/mark_all_read";
	prompt = "mail:ask-mark-all-read";

	if (!em_utils_prompt_user (parent, key, prompt, NULL))
		return;

	uids = message_list_get_uids (message_list);

	camel_folder_freeze (folder);
	for (ii = 0; ii < uids->len; ii++)
		camel_folder_set_message_flags (
			folder, uids->pdata[ii],
			CAMEL_MESSAGE_SEEN, CAMEL_MESSAGE_SEEN);
	camel_folder_thaw (folder);

	message_list_free_uids (message_list, uids);
}

static void
action_mail_folder_move_cb (GtkAction *action,
                            EMailShellView *mail_shell_view)
{
	EMailShellSidebar *mail_shell_sidebar;
	CamelFolderInfo *folder_info;
	EMFolderTree *folder_tree;

	mail_shell_sidebar = mail_shell_view->priv->mail_shell_sidebar;
	folder_tree = e_mail_shell_sidebar_get_folder_tree (mail_shell_sidebar);
	folder_info = em_folder_tree_get_selected_folder_info (folder_tree);
	g_return_if_fail (folder_info != NULL);

	/* XXX Leaking folder_info? */
	em_folder_utils_copy_folder (folder_info, TRUE);
}

static void
action_mail_folder_new_cb (GtkAction *action,
                           EMailShellView *mail_shell_view)
{
	EMailShellSidebar *mail_shell_sidebar;
	CamelFolderInfo *folder_info;
	EMFolderTree *folder_tree;

	mail_shell_sidebar = mail_shell_view->priv->mail_shell_sidebar;
	folder_tree = e_mail_shell_sidebar_get_folder_tree (mail_shell_sidebar);
	folder_info = em_folder_tree_get_selected_folder_info (folder_tree);
	g_return_if_fail (folder_info != NULL);

	em_folder_utils_create_folder (folder_info, folder_tree);
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
	CamelFolder *folder;

	mail_shell_sidebar = mail_shell_view->priv->mail_shell_sidebar;
	folder_tree = e_mail_shell_sidebar_get_folder_tree (mail_shell_sidebar);
	folder = em_folder_tree_get_selected_folder (folder_tree);
	g_return_if_fail (folder != NULL);

	em_folder_utils_rename_folder (folder);
}

/* Helper for action_mail_folder_select_all_cb() */
static gboolean
action_mail_folder_select_all_timeout_cb (MessageList *message_list)
{
	message_list_select_all (message_list);
	gtk_widget_grab_focus (GTK_WIDGET (message_list));

	return FALSE;
}

static void
action_mail_folder_select_all_cb (GtkAction *action,
                                  EMailShellView *mail_shell_view)
{
	EMailReader *reader;
	MessageList *message_list;
	EShellWindow *shell_window;
	EShellView *shell_view;

	shell_view = E_SHELL_VIEW (mail_shell_view);
	shell_window = e_shell_view_get_shell_window (shell_view);

	reader = E_MAIL_READER (mail_shell_view->priv->mail_shell_content);
	message_list = e_mail_reader_get_message_list (reader);

	if (message_list->threaded) {
		gtk_action_activate (ACTION (MAIL_THREADS_EXPAND_ALL));

		/* XXX The timeout below is added so that the execution
		 *     thread to expand all conversation threads would
		 *     have completed.  The timeout 505 is just to ensure
		 *     that the value is a small delta more than the
		 *     timeout value in mail_regen_list(). */
		g_timeout_add (
			505, (GSourceFunc)
			action_mail_folder_select_all_timeout_cb,
			message_list);
	} else
		/* If there is no threading, just select all immediately. */
		action_mail_folder_select_all_timeout_cb (message_list);
}

static void
action_mail_folder_select_thread_cb (GtkAction *action,
                                     EMailShellView *mail_shell_view)
{
	MessageList *message_list;
	EMailReader *reader;

	reader = E_MAIL_READER (mail_shell_view->priv->mail_shell_content);
	message_list = e_mail_reader_get_message_list (reader);

	message_list_select_thread (message_list);
}

static void
action_mail_folder_select_subthread_cb (GtkAction *action,
                                        EMailShellView *mail_shell_view)
{
	MessageList *message_list;
	EMailReader *reader;

	reader = E_MAIL_READER (mail_shell_view->priv->mail_shell_content);
	message_list = e_mail_reader_get_message_list (reader);

	message_list_select_subthread (message_list);
}

static void
action_mail_hide_deleted_cb (GtkToggleAction *action,
                             EMailShellView *mail_shell_view)
{
	/* FIXME */
	g_print ("Action: %s\n", gtk_action_get_name (GTK_ACTION (action)));
}

static void
action_mail_hide_read_cb (GtkAction *action,
                          EMailShellView *mail_shell_view)
{
	MessageList *message_list;
	EMailReader *reader;

	reader = E_MAIL_READER (mail_shell_view->priv->mail_shell_content);
	message_list = e_mail_reader_get_message_list (reader);

	message_list_hide_add (
		message_list,
		"(match-all (system-flag \"seen\"))",
		ML_HIDE_SAME, ML_HIDE_SAME);
}

static void
action_mail_hide_selected_cb (GtkAction *action,
                              EMailShellView *mail_shell_view)
{
	MessageList *message_list;
	EMailReader *reader;
	GPtrArray *uids;

	reader = E_MAIL_READER (mail_shell_view->priv->mail_shell_content);
	message_list = e_mail_reader_get_message_list (reader);

	uids = message_list_get_selected (message_list);
	message_list_hide_uids (message_list, uids);
	message_list_free_uids (message_list, uids);
}

static void
action_mail_preview_cb (GtkToggleAction *action,
                        EMailShellView *mail_shell_view)
{
	EMailShellContent *mail_shell_content;
	MessageList *message_list;
	CamelFolder *folder;
	EMailReader *reader;
	const gchar *state;
	gboolean active;

	mail_shell_content = mail_shell_view->priv->mail_shell_content;
	active = gtk_toggle_action_get_active (action);
	state = active ? "1" : "0";

	reader = E_MAIL_READER (mail_shell_content);
	message_list = e_mail_reader_get_message_list (reader);
	folder = message_list->folder;

	if (camel_object_meta_set (folder, "evolution:show_preview", state))
		camel_object_state_write (folder);

	e_mail_shell_content_set_preview_visible (mail_shell_content, active);
}

static void
action_mail_show_hidden_cb (GtkAction *action,
                            EMailShellView *mail_shell_view)
{
	MessageList *message_list;
	EMailReader *reader;

	reader = E_MAIL_READER (mail_shell_view->priv->mail_shell_content);
	message_list = e_mail_reader_get_message_list (reader);

	message_list_hide_clear (message_list);
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
	MessageList *message_list;
	EMailReader *reader;

	reader = E_MAIL_READER (mail_shell_view->priv->mail_shell_content);
	message_list = e_mail_reader_get_message_list (reader);

	message_list_set_threaded_collapse_all (message_list);
}

static void
action_mail_threads_expand_all_cb (GtkAction *action,
                                   EMailShellView *mail_shell_view)
{
	MessageList *message_list;
	EMailReader *reader;

	reader = E_MAIL_READER (mail_shell_view->priv->mail_shell_content);
	message_list = e_mail_reader_get_message_list (reader);

	message_list_set_threaded_expand_all (message_list);
}

static void
action_mail_threads_group_by_cb (GtkToggleAction *action,
                                 EMailShellView *mail_shell_view)
{
	EMailShellContent *mail_shell_content;
	MessageList *message_list;
	EMailReader *reader;
	CamelFolder *folder;
	const gchar *state;
	gboolean active;

	mail_shell_content = mail_shell_view->priv->mail_shell_content;
	active = gtk_toggle_action_get_active (action);
	state = active ? "1" : "0";

	reader = E_MAIL_READER (mail_shell_content);
	message_list = e_mail_reader_get_message_list (reader);
	folder = message_list->folder;

	if (camel_object_meta_set (folder, "evolution:thread_list", state))
		camel_object_state_write (folder);

	message_list_set_threaded (message_list, active);
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
	gboolean vertical_view;

	mail_shell_content = mail_shell_view->priv->mail_shell_content;
	vertical_view = (gtk_radio_action_get_current_value (action) == 1);

	e_mail_shell_content_set_vertical_view (
		mail_shell_content, vertical_view);
}

static GtkActionEntry mail_entries[] = {

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
	  N_("Download messages of accounts and folders marked for offline"),
	  G_CALLBACK (action_mail_download_cb) },

	{ "mail-empty-trash",
	  NULL,
	  N_("Empty _Trash"),
	  NULL,
	  N_("Permanently remove all the deleted messages from all folders"),
	  G_CALLBACK (action_mail_empty_trash_cb) },

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
	  NULL,
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

	{ "mail-folder-select-all",
	  NULL,
	  N_("Select _All Messages"),
	  "<Control>a",
	  N_("Select all visible messages"),
	  G_CALLBACK (action_mail_folder_select_all_cb) },

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

	{ "mail-hide-read",
	  NULL,
	  N_("Hide _Read Messages"),
	  NULL,
	  N_("Temporarily hide all messages that have already been read"),
	  G_CALLBACK (action_mail_hide_read_cb) },

	{ "mail-hide-selected",
	  NULL,
	  N_("Hide S_elected Messages"),
	  NULL,
	  N_("Temporarily hide the selected messages"),
	  G_CALLBACK (action_mail_hide_selected_cb) },

	{ "mail-show-hidden",
	  NULL,
	  N_("Show Hidde_n Messages"),
	  NULL,
	  N_("Show messages that have been temporarily hidden"),
	  G_CALLBACK (action_mail_show_hidden_cb) },

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

	{ "mail-preview-menu",
	  NULL,
	  N_("_Preview"),
	  NULL,
	  NULL,
	  NULL }
};

static GtkToggleActionEntry mail_toggle_entries[] = {

	{ "mail-hide-deleted",
	  NULL,
	  N_("Hide _Deleted Messages"),
	  NULL,
	  N_("Hide deleted messages rather than displaying "
	     "them with a line through them"),
	  G_CALLBACK (action_mail_hide_deleted_cb),
	  TRUE },

	{ "mail-preview",
	  NULL,
	  N_("Show Message _Preview"),
	  "<Control>m",
	  N_("Show message preview pane"),
	  G_CALLBACK (action_mail_preview_cb),
	  TRUE },

	{ "mail-threads-group-by",
	  NULL,
	  N_("_Group By Threads"),
	  "<Control>t",
	  N_("Threaded message list"),
	  G_CALLBACK (action_mail_threads_group_by_cb),
	  FALSE }
};

static GtkRadioActionEntry mail_view_entries[] = {

	/* This action represents the initial active mail view.
	 * It should not be visible in the UI, nor should it be
	 * possible to switch to it from another shell view. */
	{ "mail-view-internal",
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

	{ "mail-filter-label-important",
	  NULL,
	  N_("Important"),
	  NULL,
	  NULL,  /* XXX Add a tooltip! */
	  MAIL_FILTER_LABEL_IMPORTANT },

	{ "mail-filter-label-later",
	  NULL,
	  N_("Later"),
	  NULL,
	  NULL,  /* XXX Add a tooltip! */
	  MAIL_FILTER_LABEL_LATER },

	{ "mail-filter-label-personal",
	  NULL,
	  N_("Personal"),
	  NULL,
	  NULL,  /* XXX Add a tooltip! */
	  MAIL_FILTER_LABEL_PERSONAL },

	{ "mail-filter-label-to-do",
	  NULL,
	  N_("To Do"),
	  NULL,
	  NULL,  /* XXX Add a tooltip! */
	  MAIL_FILTER_LABEL_TO_DO },

	{ "mail-filter-label-work",
	  NULL,
	  N_("Work"),
	  NULL,
	  NULL,  /* XXX Add a tooltip! */
	  MAIL_FILTER_LABEL_WORK },

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

	{ "mail-search-subject-or-recipients-contains",
	  NULL,
	  N_("Subject or Recipients contains"),
	  NULL,
	  NULL,  /* XXX Add a tooltip! */
	  MAIL_SEARCH_SUBJECT_OR_RECIPIENTS_CONTAINS },

	{ "mail-search-subject-or-sender-contains",
	  NULL,
	  N_("Subject or Sender contains"),
	  NULL,
	  NULL,  /* XXX Add a tooltip! */
	  MAIL_SEARCH_SUBJECT_OR_SENDER_CONTAINS }
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
	  MAIL_SCOPE_CURRENT_FOLDER },

	{ "mail-scope-current-message",
	  NULL,
	  N_("Current Message"),
	  NULL,
	  NULL,  /* XXX Add a tooltip! */
	  MAIL_SCOPE_CURRENT_MESSAGE }
};

void
e_mail_shell_view_actions_init (EMailShellView *mail_shell_view)
{
	EShellView *shell_view;
	EShellWindow *shell_window;
	GtkActionGroup *action_group;
	GtkUIManager *ui_manager;
	GConfBridge *bridge;
	GObject *object;
	const gchar *domain;
	const gchar *key;

	shell_view = E_SHELL_VIEW (mail_shell_view);
	shell_window = e_shell_view_get_shell_window (shell_view);
	ui_manager = e_shell_window_get_ui_manager (shell_window);
	domain = GETTEXT_PACKAGE;

	action_group = mail_shell_view->priv->mail_actions;
	gtk_action_group_set_translation_domain (action_group, domain);
	gtk_action_group_add_actions (
		action_group, mail_entries,
		G_N_ELEMENTS (mail_entries), mail_shell_view);
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
		MAIL_SEARCH_SUBJECT_OR_SENDER_CONTAINS,
		NULL, NULL);
	gtk_action_group_add_radio_actions (
		action_group, mail_scope_entries,
		G_N_ELEMENTS (mail_scope_entries),
		MAIL_SCOPE_CURRENT_FOLDER,
		NULL, NULL);
	gtk_ui_manager_insert_action_group (ui_manager, action_group, 0);

	/* Bind GObject properties for GConf keys. */

	bridge = gconf_bridge_get ();

	object = G_OBJECT (ACTION (MAIL_PREVIEW));
	key = "/apps/evolution/mail/display/show_preview";
	gconf_bridge_bind_property (bridge, key, object, "active");

	object = G_OBJECT (ACTION (MAIL_THREADS_GROUP_BY));
	key = "/apps/evolution/mail/display/thread_list";
	gconf_bridge_bind_property (bridge, key, object, "active");

	object = G_OBJECT (ACTION (MAIL_VIEW_VERTICAL));
	key = "/apps/evolution/mail/display/layout";
	gconf_bridge_bind_property (bridge, key, object, "current-value");
}
