/*
 * e-mail-reader.c
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

#include "e-mail-reader.h"

#include <glib/gi18n.h>
#include <gdk/gdkkeysyms.h>
#include <gtkhtml/gtkhtml.h>

#ifdef HAVE_XFREE
#include <X11/XF86keysym.h>
#endif

#include "e-util/gconf-bridge.h"

#include "mail/e-mail-reader-utils.h"
#include "mail/em-composer-utils.h"
#include "mail/em-folder-selector.h"
#include "mail/em-folder-tree.h"
#include "mail/em-utils.h"
#include "mail/mail-autofilter.h"
#include "mail/mail-ops.h"

/* Remembers the previously selected folder when transferring messages. */
static gchar *default_xfer_messages_uri;

static void
action_mail_add_sender_cb (GtkAction *action,
                           EMailReader *reader)
{
	MessageList *message_list;
	CamelMessageInfo *info;
	CamelFolder *folder;
	GtkWindow *window;
	GPtrArray *uids;
	const gchar *address;

	folder = e_mail_reader_get_folder (reader);
	message_list = e_mail_reader_get_message_list (reader);
	window = e_mail_reader_get_window (reader);

	uids = message_list_get_selected (message_list);

	if (uids->len != 1)
		goto exit;

	info = camel_folder_get_message_info (folder, uids->pdata[0]);
	if (info == NULL)
		goto exit;

	address = camel_message_info_from (info);
	if (address == NULL || *address == '\0')
		goto exit;

	em_utils_add_address (window, address);

exit:
	em_utils_uids_free (uids);
}

static void
action_mail_caret_mode_cb (GtkToggleAction *action,
                           EMailReader *reader)
{
	EMFormatHTMLDisplay *html_display;
	gboolean active;

	active = gtk_toggle_action_get_active (action);
	html_display = e_mail_reader_get_html_display (reader);

	em_format_html_display_set_caret_mode (html_display, active);
}

static void
action_mail_check_for_junk_cb (GtkAction *action,
                               EMailReader *reader)
{
	MessageList *message_list;
	CamelFolder *folder;
	GPtrArray *uids;

	folder = e_mail_reader_get_folder (reader);
	message_list = e_mail_reader_get_message_list (reader);

	uids = message_list_get_selected (message_list);

	mail_filter_junk (folder, uids);
}

static void
action_mail_clipboard_copy_cb (GtkAction *action,
                               EMailReader *reader)
{
	EMFormatHTMLDisplay *html_display;
	GtkHTML *html;

	html_display = e_mail_reader_get_html_display (reader);
	html = ((EMFormatHTML *) html_display)->html;

	gtk_html_copy (html);
}

static void
action_mail_copy_cb (GtkAction *action,
                     EMailReader *reader)
{
	MessageList *message_list;
	EMFolderTreeModel *model;
	CamelFolder *folder;
	GtkWidget *folder_tree;
	GtkWidget *dialog;
	GPtrArray *selected;
	const gchar *uri;

	folder = e_mail_reader_get_folder (reader);
	message_list = e_mail_reader_get_message_list (reader);
	model = e_mail_reader_get_tree_model (reader);

	folder_tree = em_folder_tree_new_with_model (model);
	selected = message_list_get_selected (message_list);

	em_folder_tree_set_excluded (
		EM_FOLDER_TREE (folder_tree),
		EMFT_EXCLUDE_NOSELECT | EMFT_EXCLUDE_VIRTUAL |
		EMFT_EXCLUDE_VTRASH);

	dialog = em_folder_selector_new (
		EM_FOLDER_TREE (folder_tree),
		EM_FOLDER_SELECTOR_CAN_CREATE,
		_("Select Folder"), NULL, _("C_opy"));

	if (default_xfer_messages_uri != NULL)
		em_folder_selector_set_selected (
			EM_FOLDER_SELECTOR (dialog),
			default_xfer_messages_uri);

	if (gtk_dialog_run (GTK_DIALOG (dialog)) != GTK_RESPONSE_OK)
		goto exit;

	uri = em_folder_selector_get_selected_uri (
		EM_FOLDER_SELECTOR (dialog));

	g_free (default_xfer_messages_uri);
	default_xfer_messages_uri = g_strdup (uri);

	if (uri != NULL) {
		mail_transfer_messages (
			folder, selected, FALSE, uri, 0, NULL, NULL);
		selected = NULL;
	}

exit:
	if (selected != NULL)
		em_utils_uids_free (selected);

	gtk_widget_destroy (dialog);
}

static void
action_mail_delete_cb (GtkAction *action,
                       EMailReader *reader)
{
	guint32 mask = CAMEL_MESSAGE_SEEN | CAMEL_MESSAGE_DELETED;
	guint32 set  = CAMEL_MESSAGE_SEEN | CAMEL_MESSAGE_DELETED;

	if (!e_mail_reader_confirm_delete (reader))
		return;

	/* FIXME Verify all selected messages are deletable.
	 *       But handle it by disabling this action. */

	if (e_mail_reader_mark_selected (reader, mask, set) == 1)
		e_mail_reader_select_next_message (reader, FALSE);
}

static void
action_mail_filter_on_mailing_list_cb (GtkAction *action,
                                       EMailReader *reader)
{
	e_mail_reader_create_filter_from_selected (reader, AUTO_MLIST);
}

static void
action_mail_filter_on_recipients_cb (GtkAction *action,
                                     EMailReader *reader)
{
	e_mail_reader_create_filter_from_selected (reader, AUTO_TO);
}

static void
action_mail_filter_on_sender_cb (GtkAction *action,
                                 EMailReader *reader)
{
	e_mail_reader_create_filter_from_selected (reader, AUTO_FROM);
}

static void
action_mail_filter_on_subject_cb (GtkAction *action,
                                  EMailReader *reader)
{
	e_mail_reader_create_filter_from_selected (reader, AUTO_SUBJECT);
}

static void
action_mail_filters_apply_cb (GtkAction *action,
                              EMailReader *reader)
{
	MessageList *message_list;
	CamelFolder *folder;
	GPtrArray *uids;

	folder = e_mail_reader_get_folder (reader);
	message_list = e_mail_reader_get_message_list (reader);

	uids = message_list_get_selected (message_list);

	mail_filter_on_demand (folder, uids);
}

static void
action_mail_find_cb (GtkAction *action,
                     EMailReader *reader)
{
	/* FIXME */
}

static void
action_mail_flag_clear_cb (GtkAction *action,
                           EMailReader *reader)
{
	EMFormatHTMLDisplay *html_display;
	MessageList *message_list;
	CamelFolder *folder;
	GtkWindow *window;
	GPtrArray *uids;

	folder = e_mail_reader_get_folder (reader);
	html_display = e_mail_reader_get_html_display (reader);
	message_list = e_mail_reader_get_message_list (reader);
	window = e_mail_reader_get_window (reader);

	uids = message_list_get_selected (message_list);

	em_utils_flag_for_followup_clear (window, folder, uids);

	em_format_redraw ((EMFormat *) html_display);
}

static void
action_mail_flag_completed_cb (GtkAction *action,
                               EMailReader *reader)
{
	EMFormatHTMLDisplay *html_display;
	MessageList *message_list;
	CamelFolder *folder;
	GtkWindow *window;
	GPtrArray *uids;

	folder = e_mail_reader_get_folder (reader);
	html_display = e_mail_reader_get_html_display (reader);
	message_list = e_mail_reader_get_message_list (reader);
	window = e_mail_reader_get_window (reader);

	uids = message_list_get_selected (message_list);

	em_utils_flag_for_followup_completed (window, folder, uids);

	em_format_redraw ((EMFormat *) html_display);
}

static void
action_mail_flag_for_followup_cb (GtkAction *action,
                                  EMailReader *reader)
{
	MessageList *message_list;
	CamelFolder *folder;
	GtkWindow *window;
	GPtrArray *uids;

	folder = e_mail_reader_get_folder (reader);
	message_list = e_mail_reader_get_message_list (reader);
	window = e_mail_reader_get_window (reader);

	uids = message_list_get_selected (message_list);

	em_utils_flag_for_followup (window, folder, uids);
}

static void
action_mail_forward_cb (GtkAction *action,
                        EMailReader *reader)
{
	MessageList *message_list;
	CamelFolder *folder;
	GtkWindow *window;
	GPtrArray *uids;
	const gchar *folder_uri;

	folder = e_mail_reader_get_folder (reader);
	folder_uri = e_mail_reader_get_folder_uri (reader);
	message_list = e_mail_reader_get_message_list (reader);
	window = e_mail_reader_get_window (reader);

	if (!em_utils_check_user_can_send_mail (window))
		return;

	uids = message_list_get_selected (message_list);

	em_utils_forward_messages (folder, uids, folder_uri);
}

static void
action_mail_forward_attached_cb (GtkAction *action,
                                 EMailReader *reader)
{
	MessageList *message_list;
	CamelFolder *folder;
	GtkWindow *window;
	GPtrArray *uids;
	const gchar *folder_uri;

	folder = e_mail_reader_get_folder (reader);
	folder_uri = e_mail_reader_get_folder_uri (reader);
	message_list = e_mail_reader_get_message_list (reader);
	window = e_mail_reader_get_window (reader);

	if (!em_utils_check_user_can_send_mail (window))
		return;

	uids = message_list_get_selected (message_list);

	em_utils_forward_attached (folder, uids, folder_uri);
}

static void
action_mail_forward_inline_cb (GtkAction *action,
                               EMailReader *reader)
{
	MessageList *message_list;
	CamelFolder *folder;
	GtkWindow *window;
	GPtrArray *uids;
	const gchar *folder_uri;

	folder = e_mail_reader_get_folder (reader);
	folder_uri = e_mail_reader_get_folder_uri (reader);
	message_list = e_mail_reader_get_message_list (reader);
	window = e_mail_reader_get_window (reader);

	if (!em_utils_check_user_can_send_mail (window))
		return;

	uids = message_list_get_selected (message_list);

	em_utils_forward_inline (folder, uids, folder_uri);
}

static void
action_mail_forward_quoted_cb (GtkAction *action,
                               EMailReader *reader)
{
	MessageList *message_list;
	CamelFolder *folder;
	GtkWindow *window;
	GPtrArray *uids;
	const gchar *folder_uri;

	folder = e_mail_reader_get_folder (reader);
	folder_uri = e_mail_reader_get_folder_uri (reader);
	message_list = e_mail_reader_get_message_list (reader);
	window = e_mail_reader_get_window (reader);

	if (!em_utils_check_user_can_send_mail (window))
		return;

	uids = message_list_get_selected (message_list);

	em_utils_forward_quoted (folder, uids, folder_uri);
}

static void
action_mail_load_images_cb (GtkAction *action,
                            EMailReader *reader)
{
	EMFormatHTMLDisplay *html_display;

	html_display = e_mail_reader_get_html_display (reader);

	em_format_html_load_http ((EMFormatHTML *) html_display);
}

static void
action_mail_mark_important_cb (GtkAction *action,
                               EMailReader *reader)
{
	guint32 mask = CAMEL_MESSAGE_FLAGGED | CAMEL_MESSAGE_DELETED;
	guint32 set  = CAMEL_MESSAGE_FLAGGED;

	e_mail_reader_mark_selected (reader, mask, set);
}

static void
action_mail_mark_junk_cb (GtkAction *action,
                          EMailReader *reader)
{
	guint32 mask = CAMEL_MESSAGE_SEEN | CAMEL_MESSAGE_JUNK |
		CAMEL_MESSAGE_NOTJUNK | CAMEL_MESSAGE_JUNK_LEARN;
	guint32 set  = CAMEL_MESSAGE_SEEN | CAMEL_MESSAGE_JUNK |
		CAMEL_MESSAGE_JUNK_LEARN;

	if (e_mail_reader_mark_selected (reader, mask, set) == 1)
		e_mail_reader_select_next_message (reader, TRUE);
}

static void
action_mail_mark_notjunk_cb (GtkAction *action,
                             EMailReader *reader)
{
	guint32 mask = CAMEL_MESSAGE_JUNK | CAMEL_MESSAGE_NOTJUNK |
		CAMEL_MESSAGE_JUNK_LEARN;
	guint32 set  = CAMEL_MESSAGE_NOTJUNK | CAMEL_MESSAGE_JUNK_LEARN;

	if (e_mail_reader_mark_selected (reader, mask, set) == 1)
		e_mail_reader_select_next_message (reader, TRUE);
}

static void
action_mail_mark_read_cb (GtkAction *action,
                          EMailReader *reader)
{
	guint32 mask = CAMEL_MESSAGE_SEEN;
	guint32 set  = CAMEL_MESSAGE_SEEN;

	e_mail_reader_mark_selected (reader, mask, set);
}

static void
action_mail_mark_unimportant_cb (GtkAction *action,
                                 EMailReader *reader)
{
	guint32 mask = CAMEL_MESSAGE_FLAGGED;
	guint32 set  = 0;

	e_mail_reader_mark_selected (reader, mask, set);
}

static void
action_mail_mark_unread_cb (GtkAction *action,
                            EMailReader *reader)
{
	MessageList *message_list;
	guint32 mask = CAMEL_MESSAGE_SEEN | CAMEL_MESSAGE_DELETED;
	guint32 set  = 0;

	message_list = e_mail_reader_get_message_list (reader);

	e_mail_reader_mark_selected (reader, mask, set);

	if (message_list->seen_id != 0) {
		g_source_remove (message_list->seen_id);
		message_list->seen_id = 0;
	}
}

static void
action_mail_message_edit_cb (GtkAction *action,
                             EMailReader *reader)
{
	MessageList *message_list;
	CamelFolder *folder;
	GtkWindow *window;
	GPtrArray *uids;

	folder = e_mail_reader_get_folder (reader);
	window = e_mail_reader_get_window (reader);
	message_list = e_mail_reader_get_message_list (reader);

	if (!em_utils_check_user_can_send_mail (window))
		return;

	uids = message_list_get_selected (message_list);

	em_utils_edit_messages (folder, uids, FALSE);
}

static void
action_mail_message_new_cb (GtkAction *action,
                            EMailReader *reader)
{
	GtkWindow *window;
	const gchar *folder_uri;

	folder_uri = e_mail_reader_get_folder_uri (reader);
	window = e_mail_reader_get_window (reader);

	if (!em_utils_check_user_can_send_mail (window))
		return;

	em_utils_compose_new_message (folder_uri);
}

static void
action_mail_message_open_cb (GtkAction *action,
                             EMailReader *reader)
{
	/* FIXME This belongs in EMailShellView */
	e_mail_reader_open_selected (reader);
}

static void
action_mail_message_post_cb (GtkAction *action,
                             EMailReader *reader)
{
	CamelFolder *folder;

	folder = e_mail_reader_get_folder (reader);

	em_utils_post_to_folder (folder);
}

static void
action_mail_move_cb (GtkAction *action,
                     EMailReader *reader)
{
	MessageList *message_list;
	EMFolderTreeModel *model;
	CamelFolder *folder;
	GtkWidget *folder_tree;
	GtkWidget *dialog;
	GPtrArray *selected;
	const gchar *uri;

	folder = e_mail_reader_get_folder (reader);
	message_list = e_mail_reader_get_message_list (reader);
	model = e_mail_reader_get_tree_model (reader);

	folder_tree = em_folder_tree_new_with_model (model);
	selected = message_list_get_selected (message_list);

	em_folder_tree_set_excluded (
		EM_FOLDER_TREE (folder_tree),
		EMFT_EXCLUDE_NOSELECT | EMFT_EXCLUDE_VIRTUAL |
		EMFT_EXCLUDE_VTRASH);

	dialog = em_folder_selector_new (
		EM_FOLDER_TREE (folder_tree),
		EM_FOLDER_SELECTOR_CAN_CREATE,
		_("Select Folder"), NULL, _("C_opy"));

	if (default_xfer_messages_uri != NULL)
		em_folder_selector_set_selected (
			EM_FOLDER_SELECTOR (dialog),
			default_xfer_messages_uri);

	if (gtk_dialog_run (GTK_DIALOG (dialog)) != GTK_RESPONSE_OK)
		goto exit;

	uri = em_folder_selector_get_selected_uri (
		EM_FOLDER_SELECTOR (dialog));

	g_free (default_xfer_messages_uri);
	default_xfer_messages_uri = g_strdup (uri);

	if (uri != NULL) {
		mail_transfer_messages (
			folder, selected, TRUE, uri, 0, NULL, NULL);
		selected = NULL;
	}

exit:
	if (selected != NULL)
		em_utils_uids_free (selected);

	gtk_widget_destroy (dialog);
}

static void
action_mail_next_cb (GtkAction *action,
                     EMailReader *reader)
{
	MessageList *message_list;
	MessageListSelectDirection direction;
	guint32 flags, mask;

	direction = MESSAGE_LIST_SELECT_NEXT;
	flags = 0;
	mask  = 0;

	message_list = e_mail_reader_get_message_list (reader);
	message_list_select (message_list, direction, flags, mask);
}

static void
action_mail_next_important_cb (GtkAction *action,
                               EMailReader *reader)
{
	MessageList *message_list;
	MessageListSelectDirection direction;
	guint32 flags, mask;

	direction = MESSAGE_LIST_SELECT_NEXT | MESSAGE_LIST_SELECT_WRAP;
	flags = CAMEL_MESSAGE_FLAGGED;
	mask  = CAMEL_MESSAGE_FLAGGED;

	message_list = e_mail_reader_get_message_list (reader);
	message_list_select (message_list, direction, flags, mask);
}

static void
action_mail_next_thread_cb (GtkAction *action,
                            EMailReader *reader)
{
	MessageList *message_list;

	message_list = e_mail_reader_get_message_list (reader);
	message_list_select_next_thread (message_list);
}

static void
action_mail_next_unread_cb (GtkAction *action,
                            EMailReader *reader)
{
	MessageList *message_list;
	MessageListSelectDirection direction;
	guint32 flags, mask;

	direction = MESSAGE_LIST_SELECT_NEXT | MESSAGE_LIST_SELECT_WRAP;
	flags = 0;
	mask  = CAMEL_MESSAGE_SEEN;

	message_list = e_mail_reader_get_message_list (reader);
	message_list_select (message_list, direction, flags, mask);
}

static void
action_mail_previous_cb (GtkAction *action,
                         EMailReader *reader)
{
	MessageList *message_list;
	MessageListSelectDirection direction;
	guint32 flags, mask;

	direction = MESSAGE_LIST_SELECT_PREVIOUS;
	flags = 0;
	mask  = 0;

	message_list = e_mail_reader_get_message_list (reader);
	message_list_select (message_list, direction, flags, mask);
}

static void
action_mail_previous_important_cb (GtkAction *action,
                                   EMailReader *reader)
{
	MessageList *message_list;
	MessageListSelectDirection direction;
	guint32 flags, mask;

	direction = MESSAGE_LIST_SELECT_PREVIOUS | MESSAGE_LIST_SELECT_WRAP;
	flags = CAMEL_MESSAGE_FLAGGED;
	mask  = CAMEL_MESSAGE_FLAGGED;

	message_list = e_mail_reader_get_message_list (reader);
	message_list_select (message_list, direction, flags, mask);
}

static void
action_mail_previous_unread_cb (GtkAction *action,
                                EMailReader *reader)
{
	MessageList *message_list;
	MessageListSelectDirection direction;
	guint32 flags, mask;

	direction = MESSAGE_LIST_SELECT_PREVIOUS | MESSAGE_LIST_SELECT_WRAP;
	flags = 0;
	mask  = CAMEL_MESSAGE_SEEN;

	message_list = e_mail_reader_get_message_list (reader);
	message_list_select (message_list, direction, flags, mask);
}

static void
action_mail_print_cb (GtkAction *action,
                      EMailReader *reader)
{
	GtkPrintOperationAction print_action;

	print_action = GTK_PRINT_OPERATION_ACTION_PRINT_DIALOG;
	e_mail_reader_print (reader, print_action);
}

static void
action_mail_print_preview_cb (GtkAction *action,
                              EMailReader *reader)
{
	GtkPrintOperationAction print_action;

	print_action = GTK_PRINT_OPERATION_ACTION_PREVIEW;
	e_mail_reader_print (reader, print_action);
}

static void
action_mail_redirect_cb (GtkAction *action,
                         EMailReader *reader)
{
	MessageList *message_list;
	CamelFolder *folder;
	GtkWindow *window;
	const gchar *uid;

	folder = e_mail_reader_get_folder (reader);
	message_list = e_mail_reader_get_message_list (reader);
	window = e_mail_reader_get_window (reader);

	uid = message_list->cursor_uid;
	g_return_if_fail (uid != NULL);

	if (!em_utils_check_user_can_send_mail (window))
		return;

	em_utils_redirect_message_by_uid (folder, uid);
}

static void
action_mail_reply_all_cb (GtkAction *action,
                          EMailReader *reader)
{
	e_mail_reader_reply_to_message (reader, REPLY_MODE_ALL);
}

static void
action_mail_reply_list_cb (GtkAction *action,
                           EMailReader *reader)
{
	e_mail_reader_reply_to_message (reader, REPLY_MODE_LIST);
}

static void
action_mail_reply_post_cb (GtkAction *action,
                           EMailReader *reader)
{
	MessageList *message_list;
	CamelFolder *folder;
	GtkWindow *window;
	const gchar *uid;

	folder = e_mail_reader_get_folder (reader);
	message_list = e_mail_reader_get_message_list (reader);
	window = e_mail_reader_get_window (reader);

	uid = message_list->cursor_uid;
	g_return_if_fail (uid != NULL);

	if (!em_utils_check_user_can_send_mail (window))
		return;

	em_utils_post_reply_to_message_by_uid (folder, uid);
}

static void
action_mail_reply_sender_cb (GtkAction *action,
                             EMailReader *reader)
{
	e_mail_reader_reply_to_message (reader, REPLY_MODE_SENDER);
}

static void
action_mail_save_as_cb (GtkAction *action,
                        EMailReader *reader)
{
	MessageList *message_list;
	CamelFolder *folder;
	GtkWindow *window;
	GPtrArray *uids;

	folder = e_mail_reader_get_folder (reader);
	message_list = e_mail_reader_get_message_list (reader);
	window = e_mail_reader_get_window (reader);

	uids = message_list_get_selected (message_list);

	em_utils_save_messages (window, folder, uids);
}

static void
action_mail_search_folder_from_mailing_list_cb (GtkAction *action,
                                                EMailReader *reader)
{
	e_mail_reader_create_vfolder_from_selected (reader, AUTO_MLIST);
}

static void
action_mail_search_folder_from_recipients_cb (GtkAction *action,
                                              EMailReader *reader)
{
	e_mail_reader_create_vfolder_from_selected (reader, AUTO_TO);
}

static void
action_mail_search_folder_from_sender_cb (GtkAction *action,
                                          EMailReader *reader)
{
	e_mail_reader_create_vfolder_from_selected (reader, AUTO_FROM);
}

static void
action_mail_search_folder_from_subject_cb (GtkAction *action,
                                           EMailReader *reader)
{
	e_mail_reader_create_vfolder_from_selected (reader, AUTO_SUBJECT);
}

static void
action_mail_select_all_cb (GtkAction *action,
                           EMailReader *reader)
{
	/* FIXME */
}

static void
action_mail_show_all_headers_cb (GtkToggleAction *action,
                                 EMailReader *reader)
{
	/* FIXME */
}

static void
action_mail_show_source_cb (GtkAction *action,
                            EMailReader *reader)
{
	/* FIXME */
}

static void
action_mail_toggle_important_cb (GtkAction *action,
                                 EMailReader *reader)
{
	MessageList *message_list;
	CamelFolder *folder;
	GPtrArray *uids;
	guint ii;

	folder = e_mail_reader_get_folder (reader);
	message_list = e_mail_reader_get_message_list (reader);

	uids = message_list_get_selected (message_list);

	camel_folder_freeze (folder);

	for (ii = 0; ii < uids->len; ii++) {
		guint32 flags;

		flags = camel_folder_get_message_flags (
			folder, uids->pdata[ii]);
		flags ^= CAMEL_MESSAGE_FLAGGED;
		if (flags & CAMEL_MESSAGE_FLAGGED)
			flags &= ~CAMEL_MESSAGE_DELETED;
		camel_folder_set_message_flags (
			folder, uids->pdata[ii], CAMEL_MESSAGE_FLAGGED |
			CAMEL_MESSAGE_DELETED, flags);
	}

	camel_folder_thaw (folder);

	message_list_free_uids (message_list, uids);
}

static void
action_mail_undelete_cb (GtkAction *action,
                         EMailReader *reader)
{
	guint32 mask = CAMEL_MESSAGE_DELETED;
	guint32 set  = 0;

	e_mail_reader_mark_selected (reader, mask, set);
}

static void
action_mail_zoom_100_cb (GtkAction *action,
                         EMailReader *reader)
{
	EMFormatHTMLDisplay *html_display;

	html_display = e_mail_reader_get_html_display (reader);

	em_format_html_display_zoom_reset (html_display);
}

static void
action_mail_zoom_in_cb (GtkAction *action,
                        EMailReader *reader)
{
	EMFormatHTMLDisplay *html_display;

	html_display = e_mail_reader_get_html_display (reader);

	em_format_html_display_zoom_in (html_display);
}

static void
action_mail_zoom_out_cb (GtkAction *action,
                         EMailReader *reader)
{
	EMFormatHTMLDisplay *html_display;

	html_display = e_mail_reader_get_html_display (reader);

	em_format_html_display_zoom_out (html_display);
}

static GtkActionEntry mail_reader_entries[] = {

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

	{ "mail-delete",
	  "user-trash",
	  N_("_Delete Message"),
	  "<Control>d",
	  N_("Mark the selected messages for deletion"),
	  G_CALLBACK (action_mail_delete_cb) },

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
	  N_("A_pply Filters"),
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
	  N_("Mark the selected messasges as not being junk"),
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
	  N_("_Save as mbox..."),
	  NULL,
	  N_("Save selected messages as an mbox file"),
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

	{ "mail-show-source",
	  NULL,
	  N_("_Message Source"),
	  "<Control>u",
	  N_("Show the raw email source of the message"),
	  G_CALLBACK (action_mail_show_source_cb) },

	{ "mail-toggle-important",
	  NULL,
	  NULL,  /* No menu item; key press only */
	  NULL,
	  NULL,
	  G_CALLBACK (action_mail_toggle_important_cb) },

	{ "mail-undelete",
	  NULL,
	  N_("_Undelete Message"),
	  "<Shift><Control>d",
	  N_("Undelete the selected messages"),
	  G_CALLBACK (action_mail_undelete_cb) },

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
	  N_("Decrease the text size"),
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

	{ "mail-zoom-menu",
	  NULL,
	  N_("_Zoom"),
	  NULL,
	  NULL,
	  NULL }
};

static GtkToggleActionEntry mail_reader_toggle_entries[] = {

	{ "mail-caret-mode",
	  NULL,
	  N_("_Caret Mode"),
	  "F7",
	  N_("Show a blinking cursor in the body of displayed messages"),
	  G_CALLBACK (action_mail_caret_mode_cb),
	  FALSE },

	{ "mail-show-all-headers",
	  NULL,
	  N_("All Message _Headers"),
	  NULL,
	  N_("Show messages with all email headers"),
	  G_CALLBACK (action_mail_show_all_headers_cb),
	  FALSE }
};

static void
mail_reader_double_click_cb (EMailReader *reader,
                             gint row,
                             ETreePath path,
                             gint col,
                             GdkEvent *event)
{
	/* Ignore double clicks on columns that handle their own state. */
	if (MESSAGE_LIST_COLUMN_IS_ACTIVE (col))
		return;

	e_mail_reader_activate (reader, "mail-message-open");
}

static gint
mail_reader_key_press_cb (EMailReader *reader,
                          gint row,
                          ETreePath path,
                          gint col,
                          GdkEvent *event)
{
	const gchar *action_name;

	if ((event->key.state & GDK_CONTROL_MASK) != 0)
		return FALSE;

	switch (event->key.keyval) {
		case GDK_Return:
		case GDK_KP_Enter:
		case GDK_ISO_Enter:
			action_name = "mail-message-open";
			break;

#ifdef HAVE_XFREE
		case XF86XK_Reply:
			action_name = "mail-reply-all";
			break;

		case XF86XK_MailForward:
			action_name = "mail-forward";
			break;
#endif

		case '!':
			action_name = "mail-toggle-important";
			break;

		default:
			return FALSE;
	}

	e_mail_reader_activate (reader, action_name);

	return TRUE;
}

static void
mail_reader_class_init (EMailReaderIface *iface)
{
}

GType
e_mail_reader_get_type (void)
{
	static GType type = 0;

	if (G_UNLIKELY (type == 0)) {
		static const GTypeInfo type_info = {
			sizeof (EMailReaderIface),
			(GBaseInitFunc) NULL,
			(GBaseFinalizeFunc) NULL,
			(GClassInitFunc) mail_reader_class_init,
			(GClassFinalizeFunc) NULL,
			NULL,  /* class_data */
			0,     /* instance_size */
			0,     /* n_preallocs */
			NULL,  /* instance_init */
			NULL   /* value_table */
		};

		type = g_type_register_static (
			G_TYPE_INTERFACE, "EMailReader", &type_info, 0);

		g_type_interface_add_prerequisite (type, G_TYPE_OBJECT);
	}

	return type;
}

void
e_mail_reader_init (EMailReader *reader)
{
	MessageList *message_list;
	GtkActionGroup *action_group;
	GConfBridge *bridge;
	GtkAction *action;
	const gchar *key;

	g_return_if_fail (E_IS_MAIL_READER (reader));

	action_group = e_mail_reader_get_action_group (reader);
	message_list = e_mail_reader_get_message_list (reader);

	gtk_action_group_add_actions (
		action_group, mail_reader_entries,
		G_N_ELEMENTS (mail_reader_entries), reader);
	gtk_action_group_add_toggle_actions (
		action_group, mail_reader_toggle_entries,
		G_N_ELEMENTS (mail_reader_toggle_entries), reader);

	/* Bind GObject properties to GConf keys. */

	bridge = gconf_bridge_get ();

	key = "/apps/evolution/mail/display/caret_mode";
	action = gtk_action_group_get_action (action_group, "mail-caret-mode");
	gconf_bridge_bind_property (bridge, key, G_OBJECT (action), "active");

	/* Fine tuning. */

	action = gtk_action_group_get_action (action_group, "mail-delete");
	g_object_set (action, "short-label", _("Delete"), NULL);

	action = gtk_action_group_get_action (action_group, "mail-next");
	g_object_set (action, "short-label", _("Next"), NULL);

	action = gtk_action_group_get_action (action_group, "mail-previous");
	g_object_set (action, "short-label", _("Previous"), NULL);

	action = gtk_action_group_get_action (action_group, "mail-reply-sender");
	g_object_set (action, "short-label", _("Reply"), NULL);

	/* Connect signals. */

	g_signal_connect_swapped (
		message_list->tree, "double-click",
		G_CALLBACK (mail_reader_double_click_cb), reader);

	g_signal_connect_swapped (
		message_list->tree, "key-press",
		G_CALLBACK (mail_reader_key_press_cb), reader);
}

GtkActionGroup *
e_mail_reader_get_action_group (EMailReader *reader)
{
	EMailReaderIface *iface;

	g_return_val_if_fail (E_IS_MAIL_READER (reader), NULL);

	iface = E_MAIL_READER_GET_IFACE (reader);
	g_return_val_if_fail (iface->get_action_group != NULL, NULL);

	return iface->get_action_group (reader);
}

CamelFolder *
e_mail_reader_get_folder (EMailReader *reader)
{
	EMailReaderIface *iface;

	g_return_val_if_fail (E_IS_MAIL_READER (reader), NULL);

	iface = E_MAIL_READER_GET_IFACE (reader);
	g_return_val_if_fail (iface->get_folder != NULL, NULL);

	return iface->get_folder (reader);
}

const gchar *
e_mail_reader_get_folder_uri (EMailReader *reader)
{
	EMailReaderIface *iface;

	g_return_val_if_fail (E_IS_MAIL_READER (reader), NULL);

	iface = E_MAIL_READER_GET_IFACE (reader);
	g_return_val_if_fail (iface->get_folder_uri != NULL, NULL);

	return iface->get_folder_uri (reader);
}

gboolean
e_mail_reader_get_hide_deleted (EMailReader *reader)
{
	EMailReaderIface *iface;

	g_return_val_if_fail (E_IS_MAIL_READER (reader), FALSE);

	iface = E_MAIL_READER_GET_IFACE (reader);
	g_return_val_if_fail (iface->get_hide_deleted != NULL, FALSE);

	return iface->get_hide_deleted (reader);
}

EMFormatHTMLDisplay *
e_mail_reader_get_html_display (EMailReader *reader)
{
	EMailReaderIface *iface;

	g_return_val_if_fail (E_IS_MAIL_READER (reader), NULL);

	iface = E_MAIL_READER_GET_IFACE (reader);
	g_return_val_if_fail (iface->get_html_display != NULL, NULL);

	return iface->get_html_display (reader);
}

MessageList *
e_mail_reader_get_message_list (EMailReader *reader)
{
	EMailReaderIface *iface;

	g_return_val_if_fail (E_IS_MAIL_READER (reader), NULL);

	iface = E_MAIL_READER_GET_IFACE (reader);
	g_return_val_if_fail (iface->get_message_list != NULL, NULL);

	return iface->get_message_list (reader);
}

EShellSettings *
e_mail_reader_get_shell_settings (EMailReader *reader)
{
	EMailReaderIface *iface;

	g_return_val_if_fail (E_IS_MAIL_READER (reader), NULL);

	iface = E_MAIL_READER_GET_IFACE (reader);
	g_return_val_if_fail (iface->get_shell_settings != NULL, NULL);

	return iface->get_shell_settings (reader);
}

EMFolderTreeModel *
e_mail_reader_get_tree_model (EMailReader *reader)
{
	EMailReaderIface *iface;

	g_return_val_if_fail (E_IS_MAIL_READER (reader), NULL);

	iface = E_MAIL_READER_GET_IFACE (reader);
	g_return_val_if_fail (iface->get_tree_model != NULL, NULL);

	return iface->get_tree_model (reader);
}

GtkWindow *
e_mail_reader_get_window (EMailReader *reader)
{
	EMailReaderIface *iface;

	g_return_val_if_fail (E_IS_MAIL_READER (reader), NULL);

	iface = E_MAIL_READER_GET_IFACE (reader);
	g_return_val_if_fail (iface->get_window != NULL, NULL);

	return iface->get_window (reader);
}
