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
action_mail_add_sender_cb (GtkAction *action,
                                   EMailShellView *mail_shell_view)
{
	/* FIXME */
	g_print ("Action: %s\n", gtk_action_get_name (GTK_ACTION (action)));
}

static void
action_mail_caret_mode_cb (GtkToggleAction *action,
                           EMailShellView *mail_shell_view)
{
	/* FIXME */
	g_print ("Action: %s\n", gtk_action_get_name (GTK_ACTION (action)));
}

static void
action_mail_check_for_junk_cb (GtkAction *action,
                               EMailShellView *mail_shell_view)
{
	/* FIXME */
	g_print ("Action: %s\n", gtk_action_get_name (GTK_ACTION (action)));
}

static void
action_mail_clipboard_copy_cb (GtkAction *action,
                               EMailShellView *mail_shell_view)
{
	/* FIXME */
	g_print ("Action: %s\n", gtk_action_get_name (GTK_ACTION (action)));
}

static void
action_mail_copy_cb (GtkAction *action,
                     EMailShellView *mail_shell_view)
{
	/* FIXME */
	g_print ("Action: %s\n", gtk_action_get_name (GTK_ACTION (action)));
}

static void
action_mail_create_search_folder_cb (GtkAction *action,
                                     EMailShellView *mail_shell_view)
{
	/* FIXME */
	g_print ("Action: %s\n", gtk_action_get_name (GTK_ACTION (action)));
}

static void
action_mail_delete_cb (GtkAction *action,
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
	/* FIXME */
	g_print ("Action: %s\n", gtk_action_get_name (GTK_ACTION (action)));
}

static void
action_mail_filter_on_mailing_list_cb (GtkAction *action,
                                       EMailShellView *mail_shell_view)
{
	/* FIXME */
	g_print ("Action: %s\n", gtk_action_get_name (GTK_ACTION (action)));
}

static void
action_mail_filter_on_recipients_cb (GtkAction *action,
                                     EMailShellView *mail_shell_view)
{
	/* FIXME */
	g_print ("Action: %s\n", gtk_action_get_name (GTK_ACTION (action)));
}

static void
action_mail_filter_on_sender_cb (GtkAction *action,
                                 EMailShellView *mail_shell_view)
{
	/* FIXME */
	g_print ("Action: %s\n", gtk_action_get_name (GTK_ACTION (action)));
}

static void
action_mail_filter_on_subject_cb (GtkAction *action,
                                  EMailShellView *mail_shell_view)
{
	/* FIXME */
	g_print ("Action: %s\n", gtk_action_get_name (GTK_ACTION (action)));
}

static void
action_mail_filters_apply_cb (GtkAction *action,
                              EMailShellView *mail_shell_view)
{
	/* FIXME */
	g_print ("Action: %s\n", gtk_action_get_name (GTK_ACTION (action)));
}

static void
action_mail_find_cb (GtkAction *action,
                     EMailShellView *mail_shell_view)
{
	/* FIXME */
	g_print ("Action: %s\n", gtk_action_get_name (GTK_ACTION (action)));
}

static void
action_mail_flag_clear_cb (GtkAction *action,
                           EMailShellView *mail_shell_view)
{
	/* FIXME */
	g_print ("Action: %s\n", gtk_action_get_name (GTK_ACTION (action)));
}

static void
action_mail_flag_completed_cb (GtkAction *action,
                               EMailShellView *mail_shell_view)
{
	/* FIXME */
	g_print ("Action: %s\n", gtk_action_get_name (GTK_ACTION (action)));
}

static void
action_mail_flag_for_followup_cb (GtkAction *action,
                                  EMailShellView *mail_shell_view)
{
	/* FIXME */
	g_print ("Action: %s\n", gtk_action_get_name (GTK_ACTION (action)));
}

static void
action_mail_flush_outbox_cb (GtkAction *action,
                             EMailShellView *mail_shell_view)
{
	/* FIXME */
	g_print ("Action: %s\n", gtk_action_get_name (GTK_ACTION (action)));
}

static void
action_mail_folder_copy_cb (GtkAction *action,
                            EMailShellView *mail_shell_view)
{
	/* FIXME */
	g_print ("Action: %s\n", gtk_action_get_name (GTK_ACTION (action)));
}

static void
action_mail_folder_delete_cb (GtkAction *action,
                              EMailShellView *mail_shell_view)
{
	/* FIXME */
	g_print ("Action: %s\n", gtk_action_get_name (GTK_ACTION (action)));
}

static void
action_mail_folder_expunge_cb (GtkAction *action,
                               EMailShellView *mail_shell_view)
{
	/* FIXME */
	g_print ("Action: %s\n", gtk_action_get_name (GTK_ACTION (action)));
}

static void
action_mail_folder_mark_all_as_read_cb (GtkAction *action,
                                        EMailShellView *mail_shell_view)
{
	/* FIXME */
	g_print ("Action: %s\n", gtk_action_get_name (GTK_ACTION (action)));
}

static void
action_mail_folder_move_cb (GtkAction *action,
                            EMailShellView *mail_shell_view)
{
	/* FIXME */
	g_print ("Action: %s\n", gtk_action_get_name (GTK_ACTION (action)));
}

static void
action_mail_folder_new_cb (GtkAction *action,
                           EMailShellView *mail_shell_view)
{
	/* FIXME */
	g_print ("Action: %s\n", gtk_action_get_name (GTK_ACTION (action)));
}

static void
action_mail_folder_properties_cb (GtkAction *action,
                                  EMailShellView *mail_shell_view)
{
	/* FIXME */
	g_print ("Action: %s\n", gtk_action_get_name (GTK_ACTION (action)));
}

static void
action_mail_folder_refresh_cb (GtkAction *action,
                               EMailShellView *mail_shell_view)
{
	/* FIXME */
	g_print ("Action: %s\n", gtk_action_get_name (GTK_ACTION (action)));
}

static void
action_mail_folder_rename_cb (GtkAction *action,
                              EMailShellView *mail_shell_view)
{
	/* FIXME */
	g_print ("Action: %s\n", gtk_action_get_name (GTK_ACTION (action)));
}

static void
action_mail_folder_select_all_cb (GtkAction *action,
                                  EMailShellView *mail_shell_view)
{
	/* FIXME */
	g_print ("Action: %s\n", gtk_action_get_name (GTK_ACTION (action)));
}

static void
action_mail_folder_select_thread_cb (GtkAction *action,
                                     EMailShellView *mail_shell_view)
{
	/* FIXME */
	g_print ("Action: %s\n", gtk_action_get_name (GTK_ACTION (action)));
}

static void
action_mail_folder_select_subthread_cb (GtkAction *action,
                                        EMailShellView *mail_shell_view)
{
	/* FIXME */
	g_print ("Action: %s\n", gtk_action_get_name (GTK_ACTION (action)));
}

static void
action_mail_forward_cb (GtkAction *action,
                        EMailShellView *mail_shell_view)
{
	/* FIXME */
	g_print ("Action: %s\n", gtk_action_get_name (GTK_ACTION (action)));
}

static void
action_mail_forward_attached_cb (GtkAction *action,
                                 EMailShellView *mail_shell_view)
{
	/* FIXME */
	g_print ("Action: %s\n", gtk_action_get_name (GTK_ACTION (action)));
}

static void
action_mail_forward_inline_cb (GtkAction *action,
                               EMailShellView *mail_shell_view)
{
	/* FIXME */
	g_print ("Action: %s\n", gtk_action_get_name (GTK_ACTION (action)));
}

static void
action_mail_forward_quoted_cb (GtkAction *action,
                               EMailShellView *mail_shell_view)
{
	/* FIXME */
	g_print ("Action: %s\n", gtk_action_get_name (GTK_ACTION (action)));
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
	/* FIXME */
	g_print ("Action: %s\n", gtk_action_get_name (GTK_ACTION (action)));
}

static void
action_mail_hide_selected_cb (GtkAction *action,
                              EMailShellView *mail_shell_view)
{
	/* FIXME */
	g_print ("Action: %s\n", gtk_action_get_name (GTK_ACTION (action)));
}

static void
action_mail_load_images_cb (GtkAction *action,
                            EMailShellView *mail_shell_view)
{
	/* FIXME */
	g_print ("Action: %s\n", gtk_action_get_name (GTK_ACTION (action)));
}

static void
action_mail_mark_important_cb (GtkAction *action,
                               EMailShellView *mail_shell_view)
{
	/* FIXME */
	g_print ("Action: %s\n", gtk_action_get_name (GTK_ACTION (action)));
}

static void
action_mail_mark_junk_cb (GtkAction *action,
                          EMailShellView *mail_shell_view)
{
	/* FIXME */
	g_print ("Action: %s\n", gtk_action_get_name (GTK_ACTION (action)));
}

static void
action_mail_mark_notjunk_cb (GtkAction *action,
                             EMailShellView *mail_shell_view)
{
	/* FIXME */
	g_print ("Action: %s\n", gtk_action_get_name (GTK_ACTION (action)));
}

static void
action_mail_mark_read_cb (GtkAction *action,
                          EMailShellView *mail_shell_view)
{
	/* FIXME */
	g_print ("Action: %s\n", gtk_action_get_name (GTK_ACTION (action)));
}

static void
action_mail_mark_unimportant_cb (GtkAction *action,
                                 EMailShellView *mail_shell_view)
{
	/* FIXME */
	g_print ("Action: %s\n", gtk_action_get_name (GTK_ACTION (action)));
}

static void
action_mail_mark_unread_cb (GtkAction *action,
                            EMailShellView *mail_shell_view)
{
	/* FIXME */
	g_print ("Action: %s\n", gtk_action_get_name (GTK_ACTION (action)));
}

static void
action_mail_message_edit_cb (GtkAction *action,
                             EMailShellView *mail_shell_view)
{
	/* FIXME */
	g_print ("Action: %s\n", gtk_action_get_name (GTK_ACTION (action)));
}

static void
action_mail_message_new_cb (GtkAction *action,
                            EMailShellView *mail_shell_view)
{
	/* FIXME */
	g_print ("Action: %s\n", gtk_action_get_name (GTK_ACTION (action)));
}

static void
action_mail_message_open_cb (GtkAction *action,
                             EMailShellView *mail_shell_view)
{
	/* FIXME */
	g_print ("Action: %s\n", gtk_action_get_name (GTK_ACTION (action)));
}

static void
action_mail_message_post_cb (GtkAction *action,
                             EMailShellView *mail_shell_view)
{
	/* FIXME */
	g_print ("Action: %s\n", gtk_action_get_name (GTK_ACTION (action)));
}

static void
action_mail_move_cb (GtkAction *action,
                     EMailShellView *mail_shell_view)
{
	/* FIXME */
	g_print ("Action: %s\n", gtk_action_get_name (GTK_ACTION (action)));
}

static void
action_mail_next_cb (GtkAction *action,
                     EMailShellView *mail_shell_view)
{
	/* FIXME */
	g_print ("Action: %s\n", gtk_action_get_name (GTK_ACTION (action)));
}

static void
action_mail_next_important_cb (GtkAction *action,
                               EMailShellView *mail_shell_view)
{
	/* FIXME */
	g_print ("Action: %s\n", gtk_action_get_name (GTK_ACTION (action)));
}

static void
action_mail_next_thread_cb (GtkAction *action,
                            EMailShellView *mail_shell_view)
{
	/* FIXME */
	g_print ("Action: %s\n", gtk_action_get_name (GTK_ACTION (action)));
}

static void
action_mail_next_unread_cb (GtkAction *action,
                            EMailShellView *mail_shell_view)
{
	/* FIXME */
	g_print ("Action: %s\n", gtk_action_get_name (GTK_ACTION (action)));
}

static void
action_mail_preview_cb (GtkToggleAction *action,
                        EMailShellView *mail_shell_view)
{
	EMailShellContent *mail_shell_content;
	gboolean preview_visible;

	mail_shell_content = mail_shell_view->priv->mail_shell_content;
	preview_visible = gtk_toggle_action_get_active (action);

	e_mail_shell_content_set_preview_visible (
		mail_shell_content, preview_visible);
}

static void
action_mail_previous_cb (GtkAction *action,
                         EMailShellView *mail_shell_view)
{
	/* FIXME */
	g_print ("Action: %s\n", gtk_action_get_name (GTK_ACTION (action)));
}

static void
action_mail_previous_important_cb (GtkAction *action,
                                   EMailShellView *mail_shell_view)
{
	/* FIXME */
	g_print ("Action: %s\n", gtk_action_get_name (GTK_ACTION (action)));
}

static void
action_mail_previous_unread_cb (GtkAction *action,
                                EMailShellView *mail_shell_view)
{
	/* FIXME */
	g_print ("Action: %s\n", gtk_action_get_name (GTK_ACTION (action)));
}

static void
action_mail_print_cb (GtkAction *action,
                      EMailShellView *mail_shell_view)
{
	/* FIXME */
	g_print ("Action: %s\n", gtk_action_get_name (GTK_ACTION (action)));
}

static void
action_mail_print_preview_cb (GtkAction *action,
                              EMailShellView *mail_shell_view)
{
	/* FIXME */
	g_print ("Action: %s\n", gtk_action_get_name (GTK_ACTION (action)));
}

static void
action_mail_redirect_cb (GtkAction *action,
                         EMailShellView *mail_shell_view)
{
	/* FIXME */
	g_print ("Action: %s\n", gtk_action_get_name (GTK_ACTION (action)));
}

static void
action_mail_reply_all_cb (GtkAction *action,
                          EMailShellView *mail_shell_view)
{
	/* FIXME */
	g_print ("Action: %s\n", gtk_action_get_name (GTK_ACTION (action)));
}

static void
action_mail_reply_list_cb (GtkAction *action,
                           EMailShellView *mail_shell_view)
{
	/* FIXME */
	g_print ("Action: %s\n", gtk_action_get_name (GTK_ACTION (action)));
}

static void
action_mail_reply_post_cb (GtkAction *action,
                           EMailShellView *mail_shell_view)
{
	/* FIXME */
	g_print ("Action: %s\n", gtk_action_get_name (GTK_ACTION (action)));
}

static void
action_mail_reply_sender_cb (GtkAction *action,
                             EMailShellView *mail_shell_view)
{
	/* FIXME */
	g_print ("Action: %s\n", gtk_action_get_name (GTK_ACTION (action)));
}

static void
action_mail_save_as_cb (GtkAction *action,
                        EMailShellView *mail_shell_view)
{
	/* FIXME */
	g_print ("Action: %s\n", gtk_action_get_name (GTK_ACTION (action)));
}

static void
action_mail_search_folder_from_mailing_list_cb (GtkAction *action,
                                                EMailShellView *mail_shell_view)
{
	/* FIXME */
	g_print ("Action: %s\n", gtk_action_get_name (GTK_ACTION (action)));
}

static void
action_mail_search_folder_from_recipients_cb (GtkAction *action,
                                              EMailShellView *mail_shell_view)
{
	/* FIXME */
	g_print ("Action: %s\n", gtk_action_get_name (GTK_ACTION (action)));
}

static void
action_mail_search_folder_from_sender_cb (GtkAction *action,
                                          EMailShellView *mail_shell_view)
{
	/* FIXME */
	g_print ("Action: %s\n", gtk_action_get_name (GTK_ACTION (action)));
}

static void
action_mail_search_folder_from_subject_cb (GtkAction *action,
                                           EMailShellView *mail_shell_view)
{
	/* FIXME */
	g_print ("Action: %s\n", gtk_action_get_name (GTK_ACTION (action)));
}

static void
action_mail_select_all_cb (GtkAction *action,
                           EMailShellView *mail_shell_view)
{
	/* FIXME */
	g_print ("Action: %s\n", gtk_action_get_name (GTK_ACTION (action)));
}

static void
action_mail_show_all_headers_cb (GtkToggleAction *action,
                                 EMailShellView *mail_shell_view)
{
	/* FIXME */
	g_print ("Action: %s\n", gtk_action_get_name (GTK_ACTION (action)));
}

static void
action_mail_show_hidden_cb (GtkAction *action,
                            EMailShellView *mail_shell_view)
{
	/* FIXME */
	g_print ("Action: %s\n", gtk_action_get_name (GTK_ACTION (action)));
}

static void
action_mail_show_source_cb (GtkAction *action,
                            EMailShellView *mail_shell_view)
{
	/* FIXME */
	g_print ("Action: %s\n", gtk_action_get_name (GTK_ACTION (action)));
}

static void
action_mail_stop_cb (GtkAction *action,
                     EMailShellView *mail_shell_view)
{
	/* FIXME */
	g_print ("Action: %s\n", gtk_action_get_name (GTK_ACTION (action)));
}

static void
action_mail_threads_collapse_all_cb (GtkAction *action,
                                     EMailShellView *mail_shell_view)
{
	/* FIXME */
	g_print ("Action: %s\n", gtk_action_get_name (GTK_ACTION (action)));
}

static void
action_mail_threads_expand_all_cb (GtkAction *action,
                                   EMailShellView *mail_shell_view)
{
	/* FIXME */
	g_print ("Action: %s\n", gtk_action_get_name (GTK_ACTION (action)));
}

static void
action_mail_threads_group_by_cb (GtkToggleAction *action,
                                 EMailShellView *mail_shell_view)
{
	/* FIXME */
	g_print ("Action: %s\n", gtk_action_get_name (GTK_ACTION (action)));
}

static void
action_mail_tools_filters_cb (GtkAction *action,
                              EMailShellView *mail_shell_view)
{
	/* FIXME */
	g_print ("Action: %s\n", gtk_action_get_name (GTK_ACTION (action)));
}

static void
action_mail_tools_search_folders_cb (GtkAction *action,
                                     EMailShellView *mail_shell_view)
{
	/* FIXME */
	g_print ("Action: %s\n", gtk_action_get_name (GTK_ACTION (action)));
}

static void
action_mail_tools_subscriptions_cb (GtkAction *action,
                                    EMailShellView *mail_shell_view)
{
	/* FIXME */
	g_print ("Action: %s\n", gtk_action_get_name (GTK_ACTION (action)));
}

static void
action_mail_undelete_cb (GtkAction *action,
                         EMailShellView *mail_shell_view)
{
	/* FIXME */
	g_print ("Action: %s\n", gtk_action_get_name (GTK_ACTION (action)));
}

static void
action_mail_uri_call_to_cb (GtkAction *action,
                            EMailShellView *mail_shell_view)
{
	/* FIXME */
	g_print ("Action: %s\n", gtk_action_get_name (GTK_ACTION (action)));
}

static void
action_mail_uri_copy_cb (GtkAction *action,
                         EMailShellView *mail_shell_view)
{
	/* FIXME */
	g_print ("Action: %s\n", gtk_action_get_name (GTK_ACTION (action)));
}

static void
action_mail_uri_copy_address_cb (GtkAction *action,
                                 EMailShellView *mail_shell_view)
{
	/* FIXME */
	g_print ("Action: %s\n", gtk_action_get_name (GTK_ACTION (action)));
}

static void
action_mail_uri_to_search_folder_recipient_cb (GtkAction *action,
                                               EMailShellView *mail_shell_view)
{
	/* FIXME */
	g_print ("Action: %s\n", gtk_action_get_name (GTK_ACTION (action)));
}

static void
action_mail_uri_to_search_folder_sender_cb (GtkAction *action,
                                            EMailShellView *mail_shell_view)
{
	/* FIXME */
	g_print ("Action: %s\n", gtk_action_get_name (GTK_ACTION (action)));
}

static void
action_mail_zoom_100_cb (GtkAction *action,
                         EMailShellView *mail_shell_view)
{
	/* FIXME */
	g_print ("Action: %s\n", gtk_action_get_name (GTK_ACTION (action)));
}

static void
action_mail_zoom_in_cb (GtkAction *action,
                        EMailShellView *mail_shell_view)
{
	/* FIXME */
	g_print ("Action: %s\n", gtk_action_get_name (GTK_ACTION (action)));
}

static void
action_mail_zoom_out_cb (GtkAction *action,
                         EMailShellView *mail_shell_view)
{
	/* FIXME */
	g_print ("Action: %s\n", gtk_action_get_name (GTK_ACTION (action)));
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

	{ "mail-add-sender",
	  NULL,
	  N_("A_dd Sender to Address Book"),
	  NULL,
	  N_("Add sender to address book"),
	  G_CALLBACK (action_mail_add_sender_cb) },

	{ "mail-check-for-junk",
	  "mail-mark-junk",
	  N_("Check for _Junk"),
	  NULL,
	  N_("Filter the selected messages for junk status"),
	  G_CALLBACK (action_mail_check_for_junk_cb) },

	{ "mail-clipboard-copy",
	  GTK_STOCK_COPY,
	  NULL,
	  NULL,
	  N_("Copy selected messages to the clipboard"),
	  G_CALLBACK (action_mail_clipboard_copy_cb) },

	{ "mail-copy",
	  "mail-copy",
	  N_("_Copy to Folder"),
	  "<Shift><Control>y",
	  N_("Copy selected messages to another folder"),
	  G_CALLBACK (action_mail_copy_cb) },

	{ "mail-create-search-folder",
	  NULL,
	  N_("C_reate Search Folder From Search..."),
	  NULL,
	  NULL,  /* XXX Add a tooltip! */
	  G_CALLBACK (action_mail_create_search_folder_cb) },

	{ "mail-delete",
	  "user-trash",
	  N_("_Delete Message"),
	  "<Control>d",
	  N_("Mark the selected messages for deletion"),
	  G_CALLBACK (action_mail_delete_cb) },

	/* XXX Work around one-accelerator-per-action limit. */
	{ "mail-delete-1",
	  NULL,
	  NULL,
	  "Delete",
	  NULL,
	  G_CALLBACK (action_mail_delete_cb) },

	/* XXX Work around one-accelerator-per-action limit. */
	{ "mail-delete-2",
	  NULL,
	  NULL,
	  "KP_Delete",
	  NULL,
	  G_CALLBACK (action_mail_delete_cb) },

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

	{ "mail-filter-on-mailing-list",
	  NULL,
	  N_("Filter on Mailing _List..."),
	  NULL,
	  N_("Create a rule to filter messages to this mailing list"),
	  G_CALLBACK (action_mail_filter_on_mailing_list_cb) },

	{ "mail-filter-on-recipients",
	  NULL,
	  N_("Filter on _Recipients..."),
	  NULL,
	  N_("Create a rule to filter messages to these recipients"),
	  G_CALLBACK (action_mail_filter_on_recipients_cb) },

	{ "mail-filter-on-sender",
	  NULL,
	  N_("Filter on Se_nder..."),
	  NULL,
	  N_("Create a rule to filter messages from this sender"),
	  G_CALLBACK (action_mail_filter_on_sender_cb) },

	{ "mail-filter-on-subject",
	  NULL,
	  N_("Filter on _Subject..."),
	  NULL,
	  N_("Create a rule to filter messages with this subject"),
	  G_CALLBACK (action_mail_filter_on_subject_cb) },

	{ "mail-filters-apply",
	  "stock_mail-filters-apply",
	  N_("A_pply filters"),
	  "<Control>y",
	  N_("Apply filter rules to the selected messages"),
	  G_CALLBACK (action_mail_filters_apply_cb) },

	{ "mail-find",
	  GTK_STOCK_FIND,
	  N_("_Find in Message..."),
	  "<Shift><Control>f",
	  N_("Search for text in the body of the displayed message"),
	  G_CALLBACK (action_mail_find_cb) },

	{ "mail-flag-clear",
	  NULL,
	  N_("_Clear Flag"),
	  NULL,
	  N_("Remove the follow-up flag from the selected messages"),
	  G_CALLBACK (action_mail_flag_clear_cb) },

	{ "mail-flag-completed",
	  NULL,
	  N_("_Flag Completed"),
	  NULL,
	  N_("Set the follow-up flag to completed on the selected messages"),
	  G_CALLBACK (action_mail_flag_completed_cb) },

	{ "mail-flag-for-followup",
	  "stock_mail-flag-for-followup",
	  N_("Follow _Up..."),
	  "<Shift><Control>g",
	  N_("Flag the selected messages for follow-up"),
	  G_CALLBACK (action_mail_flag_for_followup_cb) },

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

	{ "mail-forward",
	  "mail-forward",
	  N_("_Forward"),
	  "<Control>f",
	  N_("Forward the selected message to someone"),
	  G_CALLBACK (action_mail_forward_cb) },

	{ "mail-forward-attached",
	  NULL,
	  N_("_Attached"),
	  NULL,
	  N_("Forward the selected message to someone as an attachment"),
	  G_CALLBACK (action_mail_forward_attached_cb) },

	{ "mail-forward-inline",
	  NULL,
	  N_("_Inline"),
	  NULL,
	  N_("Forward the selected message in the body of a new message"),
	  G_CALLBACK (action_mail_forward_inline_cb) },

	{ "mail-forward-quoted",
	  NULL,
	  N_("_Quoted"),
	  NULL,
	  N_("Forward the selected message quoted like a reply"),
	  G_CALLBACK (action_mail_forward_quoted_cb) },

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

	{ "mail-load-images",
	  "image-x-generic",
	  N_("_Load Images"),
	  "<Control>i",
	  N_("Force images in HTML mail to be loaded"),
	  G_CALLBACK (action_mail_load_images_cb) },

	{ "mail-mark-important",
	  "mail-mark-important",
	  N_("_Important"),
	  NULL,
	  N_("Mark the selected messages as important"),
	  G_CALLBACK (action_mail_mark_important_cb) },

	{ "mail-mark-junk",
	  "mail-mark-junk",
	  N_("_Junk"),
	  "<Control>j",
	  N_("Mark the selected messages as junk"),
	  G_CALLBACK (action_mail_mark_junk_cb) },

	{ "mail-mark-notjunk",
	  "mail-mark-notjunk",
	  N_("_Not Junk"),
	  "<Shift><Control>j",
	  N_("Mark the selected messages as not being junk"),
	  G_CALLBACK (action_mail_mark_notjunk_cb) },

	{ "mail-mark-read",
	  "mail-mark-read",
	  N_("_Read"),
	  "<Control>k",
	  N_("Mark the selected messages as having been read"),
	  G_CALLBACK (action_mail_mark_read_cb) },

	{ "mail-mark-unimportant",
	  NULL,
	  N_("Uni_mportant"),
	  NULL,
	  N_("Mark the selected messages as unimportant"),
	  G_CALLBACK (action_mail_mark_unimportant_cb) },

	{ "mail-mark-unread",
	  "mail-mark-unread",
	  N_("_Unread"),
	  "<Shift><Control>k",
	  N_("Mark the selected messages as not having been read"),
	  G_CALLBACK (action_mail_mark_unread_cb) },

	{ "mail-message-edit",
	  NULL,
	  N_("_Edit as New Message..."),
	  NULL,
	  N_("Open the selected messages in the composer for editing"),
	  G_CALLBACK (action_mail_message_edit_cb) },

	{ "mail-message-new",
	  "mail-message-new",
	  N_("Compose _New Message"),
	  "<Shift><Control>m",
	  N_("Open a window for composing a mail message"),
	  G_CALLBACK (action_mail_message_new_cb) },

	{ "mail-message-open",
	  NULL,
	  N_("_Open in New Window"),
	  "<Control>o",
	  N_("Open the selected messages in a new window"),
	  G_CALLBACK (action_mail_message_open_cb) },

	{ "mail-message-post",
	  NULL,
	  N_("Pos_t New Message to Folder"),
	  NULL,
	  N_("Post a message to a public folder"),
	  G_CALLBACK (action_mail_message_post_cb) },

	{ "mail-move",
	  "mail-move",
	  N_("_Move to Folder"),
	  "<Shift><Control>v",
	  N_("Move selected messages to another folder"),
	  G_CALLBACK (action_mail_move_cb) },

	{ "mail-next",
	  GTK_STOCK_GO_FORWARD,
	  N_("_Next Message"),
	  "<Control>Page_Down",
	  N_("Display the next message"),
	  G_CALLBACK (action_mail_next_cb) },

	{ "mail-next-important",
	  NULL,
	  N_("Next _Important Message"),
	  NULL,
	  N_("Display the next important message"),
	  G_CALLBACK (action_mail_next_important_cb) },

	{ "mail-next-thread",
	  NULL,
	  N_("Next _Thread"),
	  NULL,
	  N_("Display the next thread"),
	  G_CALLBACK (action_mail_next_thread_cb) },

	{ "mail-next-unread",
	  NULL,
	  N_("Next _Unread Message"),
	  "<Control>bracketright",
	  N_("Display the next unread message"),
	  G_CALLBACK (action_mail_next_unread_cb) },

	/* XXX Work around one-accelerator-per-action limit. */
	{ "mail-next-unread-1",
	  NULL,
	  NULL,
	  "period",
	  NULL,
	  G_CALLBACK (action_mail_next_unread_cb) },

	/* XXX Work around one-accelerator-per-action limit. */
	{ "mail-next-unread-2",
	  NULL,
	  NULL,
	  "<Control>period",
	  NULL,
	  G_CALLBACK (action_mail_next_unread_cb) },

	{ "mail-previous",
	  GTK_STOCK_GO_BACK,
	  N_("_Previous Message"),
	  "<Control>Page_Up",
	  N_("Display the previous message"),
	  G_CALLBACK (action_mail_previous_cb) },

	{ "mail-previous-important",
	  NULL,
	  N_("Pr_evious Important Message"),
	  NULL,
	  N_("Display the previous important message"),
	  G_CALLBACK (action_mail_previous_important_cb) },

	{ "mail-previous-unread",
	  NULL,
	  N_("P_revious Unread Message"),
	  "<Control>bracketleft",
	  N_("Display the previous unread message"),
	  G_CALLBACK (action_mail_previous_unread_cb) },

	/* XXX Work around one-accelerator-per-action limit. */
	{ "mail-previous-unread-1",
	  NULL,
	  NULL,
	  "comma",
	  NULL,
	  G_CALLBACK (action_mail_previous_unread_cb) },

	/* XXX Work around one-accelerator-per-action limit. */
	{ "mail-previous-unread-2",
	  NULL,
	  NULL,
	  "<Control>comma",
	  NULL,
	  G_CALLBACK (action_mail_previous_unread_cb) },

	{ "mail-print",
	  GTK_STOCK_PRINT,
	  NULL,
	  NULL,
	  N_("Print this message"),
	  G_CALLBACK (action_mail_print_cb) },

	{ "mail-print-preview",
	  GTK_STOCK_PRINT_PREVIEW,
	  NULL,
	  NULL,
	  N_("Preview the message to be printed"),
	  G_CALLBACK (action_mail_print_preview_cb) },

	{ "mail-redirect",
	  NULL,
	  N_("Re_direct"),
	  NULL,
	  N_("Redirect (bounce) the selected message to someone"),
	  G_CALLBACK (action_mail_redirect_cb) },

	{ "mail-reply-all",
	  "mail-reply-all",
	  N_("Reply to _All"),
	  "<Shift><Control>r",
	  N_("Compose a reply to all the recipients of the selected message"),
	  G_CALLBACK (action_mail_reply_all_cb) },

	{ "mail-reply-list",
	  NULL,
	  N_("Reply to _List"),
	  "<Control>l",
	  N_("Compose a reply to the mailing list of the selected message"),
	  G_CALLBACK (action_mail_reply_list_cb) },

	{ "mail-reply-post",
	  NULL,
	  N_("Post a Repl_y"),
	  NULL,
	  N_("Post a reply to a message in a public folder"),
	  G_CALLBACK (action_mail_reply_post_cb) },

	{ "mail-reply-sender",
	  "mail-reply-sender",
	  N_("_Reply to Sender"),
	  "<Control>r",
	  N_("Compose a reply to the sender of the selected message"),
	  G_CALLBACK (action_mail_reply_sender_cb) },

	{ "mail-save-as",
	  GTK_STOCK_SAVE_AS,
	  N_("_Save As mbox..."),
	  NULL,
	  N_("Save selected message as an mbox file"),
	  G_CALLBACK (action_mail_save_as_cb) },

	{ "mail-search-folder-from-mailing-list",
	  NULL,
	  N_("Search Folder from Mailing _List..."),
	  NULL,
	  N_("Create a search folder for this mailing list"),
	  G_CALLBACK (action_mail_search_folder_from_mailing_list_cb) },

	{ "mail-search-folder-from-recipients",
	  NULL,
	  N_("Search Folder from Recipien_ts..."),
	  NULL,
	  N_("Create a search folder for these recipients"),
	  G_CALLBACK (action_mail_search_folder_from_recipients_cb) },

	{ "mail-search-folder-from-sender",
	  NULL,
	  N_("Search Folder from Sen_der..."),
	  NULL,
	  N_("Create a search folder for this sender"),
	  G_CALLBACK (action_mail_search_folder_from_sender_cb) },

	{ "mail-search-folder-from-subject",
	  NULL,
	  N_("Search Folder from S_ubject..."),
	  NULL,
	  N_("Create a search folder for this subject"),
	  G_CALLBACK (action_mail_search_folder_from_subject_cb) },

	{ "mail-select-all",
	  NULL,
	  N_("Select _All Text"),
	  "<Shift><Control>x",
	  N_("Select all the text in a message"),
	  G_CALLBACK (action_mail_select_all_cb) },

	{ "mail-show-hidden",
	  NULL,
	  N_("Show Hidde_n Messages"),
	  NULL,
	  N_("Show messages that have been temporarily hidden"),
	  G_CALLBACK (action_mail_show_hidden_cb) },

	{ "mail-show-source",
	  NULL,
	  N_("_Message Source"),
	  "<Control>u",
	  N_("Show the raw email source of the message"),
	  G_CALLBACK (action_mail_show_source_cb) },

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

	{ "mail-undelete",
	  NULL,
	  N_("_Undelete Message"),
	  "<Shift><Control>d",
	  N_("Undelete the selected messages"),
	  G_CALLBACK (action_mail_undelete_cb) },

	{ "mail-uri-call-to",
	  NULL,
	  N_("C_all To..."),
	  NULL,
	  NULL,  /* XXX Add a tooltip! */
	  G_CALLBACK (action_mail_uri_call_to_cb) },

	{ "mail-uri-copy",
	  GTK_STOCK_COPY,
	  N_("_Copy Link Location"),
	  NULL,
	  NULL,  /* XXX Add a tooltip! */
	  G_CALLBACK (action_mail_uri_copy_cb) },

	{ "mail-uri-copy-address",
	  GTK_STOCK_COPY,
	  N_("Copy _Email Address"),
	  NULL,
	  NULL,  /* XXX Add a tooltip! */
	  G_CALLBACK (action_mail_uri_copy_address_cb) },

	{ "mail-uri-to-search-folder-recipient",
	  NULL,
	  N_("_To This Address"),
	  NULL,
	  NULL,  /* XXX Add a tooltip! */
	  G_CALLBACK (action_mail_uri_to_search_folder_recipient_cb) },

	{ "mail-uri-to-search-folder-sender",
	  NULL,
	  N_("_From This Address"),
	  NULL,
	  NULL,  /* XXX Add a tooltip! */
	  G_CALLBACK (action_mail_uri_to_search_folder_sender_cb) },

	{ "mail-zoom-100",
	  GTK_STOCK_ZOOM_100,
	  N_("_Normal Size"),
	  "<Control>0",
	  N_("Reset the text to its original size"),
	  G_CALLBACK (action_mail_zoom_100_cb) },

	{ "mail-zoom-in",
	  GTK_STOCK_ZOOM_IN,
	  N_("_Zoom In"),
	  "<Control>plus",
	  N_("Increase the text size"),
	  G_CALLBACK (action_mail_zoom_in_cb) },

	{ "mail-zoom-out",
	  GTK_STOCK_ZOOM_OUT,
	  N_("Zoom _Out"),
	  "<Control>minus",
	  N_("Decreate the text size"),
	  G_CALLBACK (action_mail_zoom_out_cb) },

	/*** Menus ***/

	{ "mail-create-rule-menu",
	  NULL,
	  N_("Create R_ule"),
	  NULL,
	  NULL,
	  NULL },

	{ "mail-encoding-menu",
	  NULL,
	  N_("Ch_aracter Encoding"),
	  NULL,
	  NULL,
	  NULL },

	{ "mail-folder-menu",
	  NULL,
	  N_("F_older"),
	  NULL,
	  NULL,
	  NULL },

	{ "mail-forward-as-menu",
	  NULL,
	  N_("F_orward As..."),
	  NULL,
	  NULL,
	  NULL },

	{ "mail-goto-menu",
	  GTK_STOCK_JUMP_TO,
	  N_("_Go To"),
	  NULL,
	  NULL,
	  NULL },

	{ "mail-label-menu",
	  NULL,
	  N_("_Label"),
	  NULL,
	  NULL,
	  NULL },

	{ "mail-mark-as-menu",
	  NULL,
	  N_("Mar_k As"),
	  NULL,
	  NULL,
	  NULL },

	{ "mail-message-menu",
	  NULL,
	  N_("_Message"),
	  NULL,
	  NULL,
	  NULL },

	{ "mail-preview-menu",
	  NULL,
	  N_("_Preview"),
	  NULL,
	  NULL,
	  NULL },

	{ "mail-uri-to-search-folder-menu",
	  NULL,
	  N_("Create _Search Folder"),
	  NULL,
	  NULL,
	  NULL },

	{ "mail-zoom-menu",
	  NULL,
	  N_("_Zoom"),
	  NULL,
	  NULL,
	  NULL }
};

static GtkToggleActionEntry mail_toggle_entries[] = {

	{ "mail-caret-mode",
	  NULL,
	  N_("_Caret Mode"),
	  "F7",
	  N_("Show a blinking cursor in the body of displayed messages"),
	  G_CALLBACK (action_mail_caret_mode_cb),
	  FALSE },

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

	{ "mail-show-all-headers",
	  NULL,
	  N_("All Message _Headers"),
	  NULL,
	  N_("Show messages with all email headers"),
	  G_CALLBACK (action_mail_show_all_headers_cb),
	  FALSE },

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
	GtkAction *action;
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

	object = G_OBJECT (ACTION (MAIL_CARET_MODE));
	key = "/apps/evolution/mail/display/caret_mode";
	gconf_bridge_bind_property (bridge, key, object, "active");

	object = G_OBJECT (ACTION (MAIL_PREVIEW));
	key = "/apps/evolution/mail/display/show_preview";
	gconf_bridge_bind_property (bridge, key, object, "active");

	object = G_OBJECT (ACTION (MAIL_VIEW_VERTICAL));
	key = "/apps/evolution/mail/display/show_wide";
	gconf_bridge_bind_property (bridge, key, object, "active");

	object = G_OBJECT (ACTION (MAIL_THREADS_GROUP_BY));
	key = "/apps/evolution/mail/display/thread_list";
	gconf_bridge_bind_property (bridge, key, object, "active");

	/* Fine tuning. */

	action = ACTION (MAIL_DELETE);
	g_object_set (action, "short-label", _("Delete"), NULL);

	action = ACTION (MAIL_NEXT);
	g_object_set (action, "short-label", _("Next"), NULL);

	action = ACTION (MAIL_PREVIOUS);
	g_object_set (action, "short-label", _("Previous"), NULL);

	action = ACTION (MAIL_REPLY_SENDER);
	g_object_set (action, "short-label", _("Reply"), NULL);
}
