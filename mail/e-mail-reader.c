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
#include <gtkhtml/gtkhtml-stream.h>

#ifdef HAVE_XFREE
#include <X11/XF86keysym.h>
#endif

#include "e-util/e-util.h"
#include "e-util/e-binding.h"
#include "e-util/gconf-bridge.h"
#include "shell/e-shell.h"
#include "widgets/misc/e-charset-picker.h"
#include "widgets/misc/e-popup-action.h"

#include "mail/e-mail-browser.h"
#include "mail/e-mail-reader-utils.h"
#include "mail/e-mail-shell-backend.h"
#include "mail/em-composer-utils.h"
#include "mail/em-event.h"
#include "mail/em-folder-selector.h"
#include "mail/em-folder-tree.h"
#include "mail/em-utils.h"
#include "mail/mail-autofilter.h"
#include "mail/mail-config.h"
#include "mail/mail-ops.h"

enum {
	CHANGED,
	FOLDER_LOADED,
	SHOW_SEARCH_BAR,
	LAST_SIGNAL
};

/* Remembers the previously selected folder when transferring messages. */
static gchar *default_xfer_messages_uri;

static guint signals[LAST_SIGNAL];

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

	message_list = e_mail_reader_get_message_list (reader);
	window = e_mail_reader_get_window (reader);

	folder = message_list->folder;
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
action_mail_charset_cb (GtkRadioAction *action,
                        GtkRadioAction *current,
                        EMailReader *reader)
{
	EMFormatHTMLDisplay *html_display;
	const gchar *charset;

	if (action != current)
		return;

	html_display = e_mail_reader_get_html_display (reader);
	charset = g_object_get_data (G_OBJECT (action), "charset");

	/* Charset for "Default" action will be NULL. */
	em_format_set_charset (EM_FORMAT (html_display), charset);
}

static void
action_mail_check_for_junk_cb (GtkAction *action,
                               EMailReader *reader)
{
	MessageList *message_list;
	CamelFolder *folder;
	GPtrArray *uids;

	message_list = e_mail_reader_get_message_list (reader);

	folder = message_list->folder;
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
	html = EM_FORMAT_HTML (html_display)->html;

	gtk_html_copy (html);
}

static void
action_mail_copy_cb (GtkAction *action,
                     EMailReader *reader)
{
	EShellBackend *shell_backend;
	MessageList *message_list;
	EMFolderTreeModel *model;
	CamelFolder *folder;
	GtkWidget *folder_tree;
	GtkWidget *dialog;
	GPtrArray *selected;
	const gchar *uri;

	message_list = e_mail_reader_get_message_list (reader);
	shell_backend = e_mail_reader_get_shell_backend (reader);
	model = e_mail_shell_backend_get_folder_tree_model (
		E_MAIL_SHELL_BACKEND (shell_backend));

	folder_tree = em_folder_tree_new_with_model (model);
	selected = message_list_get_selected (message_list);

	folder = message_list->folder;

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

	message_list = e_mail_reader_get_message_list (reader);

	folder = message_list->folder;
	uids = message_list_get_selected (message_list);

	mail_filter_on_demand (folder, uids);
}

static void
action_mail_find_cb (GtkAction *action,
                     EMailReader *reader)
{
	e_mail_reader_show_search_bar (reader);
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

	html_display = e_mail_reader_get_html_display (reader);
	message_list = e_mail_reader_get_message_list (reader);
	window = e_mail_reader_get_window (reader);

	folder = message_list->folder;
	uids = message_list_get_selected (message_list);

	em_utils_flag_for_followup_clear (window, folder, uids);

	em_format_redraw (EM_FORMAT (html_display));
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

	html_display = e_mail_reader_get_html_display (reader);
	message_list = e_mail_reader_get_message_list (reader);
	window = e_mail_reader_get_window (reader);

	folder = message_list->folder;
	uids = message_list_get_selected (message_list);

	em_utils_flag_for_followup_completed (window, folder, uids);

	em_format_redraw (EM_FORMAT (html_display));
}

static void
action_mail_flag_for_followup_cb (GtkAction *action,
                                  EMailReader *reader)
{
	MessageList *message_list;
	CamelFolder *folder;
	GtkWindow *window;
	GPtrArray *uids;

	message_list = e_mail_reader_get_message_list (reader);
	window = e_mail_reader_get_window (reader);

	folder = message_list->folder;
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

	message_list = e_mail_reader_get_message_list (reader);
	window = e_mail_reader_get_window (reader);

	if (!em_utils_check_user_can_send_mail (window))
		return;

	folder = message_list->folder;
	folder_uri = message_list->folder_uri;
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

	message_list = e_mail_reader_get_message_list (reader);
	window = e_mail_reader_get_window (reader);

	if (!em_utils_check_user_can_send_mail (window))
		return;

	folder = message_list->folder;
	folder_uri = message_list->folder_uri;
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

	message_list = e_mail_reader_get_message_list (reader);
	window = e_mail_reader_get_window (reader);

	if (!em_utils_check_user_can_send_mail (window))
		return;

	folder = message_list->folder;
	folder_uri = message_list->folder_uri;
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

	message_list = e_mail_reader_get_message_list (reader);
	window = e_mail_reader_get_window (reader);

	if (!em_utils_check_user_can_send_mail (window))
		return;

	folder = message_list->folder;
	folder_uri = message_list->folder_uri;
	uids = message_list_get_selected (message_list);

	em_utils_forward_quoted (folder, uids, folder_uri);
}

static void
action_mail_load_images_cb (GtkAction *action,
                            EMailReader *reader)
{
	EMFormatHTMLDisplay *html_display;

	html_display = e_mail_reader_get_html_display (reader);

	em_format_html_load_images (EM_FORMAT_HTML (html_display));
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

	window = e_mail_reader_get_window (reader);
	message_list = e_mail_reader_get_message_list (reader);

	if (!em_utils_check_user_can_send_mail (window))
		return;

	folder = message_list->folder;
	uids = message_list_get_selected (message_list);

	em_utils_edit_messages (folder, uids, FALSE);
}

static void
action_mail_message_new_cb (GtkAction *action,
                            EMailReader *reader)
{
	MessageList *message_list;
	GtkWindow *window;

	message_list = e_mail_reader_get_message_list (reader);
	window = e_mail_reader_get_window (reader);

	if (!em_utils_check_user_can_send_mail (window))
		return;

	em_utils_compose_new_message (message_list->folder_uri);
}

static void
action_mail_message_open_cb (GtkAction *action,
                             EMailReader *reader)
{
	e_mail_reader_open_selected (reader);
}

static void
action_mail_message_post_cb (GtkAction *action,
                             EMailReader *reader)
{
	MessageList *message_list;

	message_list = e_mail_reader_get_message_list (reader);

	em_utils_post_to_folder (message_list->folder);
}

static void
action_mail_move_cb (GtkAction *action,
                     EMailReader *reader)
{
	EShellBackend *shell_backend;
	MessageList *message_list;
	EMFolderTreeModel *model;
	CamelFolder *folder;
	GtkWidget *folder_tree;
	GtkWidget *dialog;
	GPtrArray *selected;
	const gchar *uri;

	message_list = e_mail_reader_get_message_list (reader);
	shell_backend = e_mail_reader_get_shell_backend (reader);
	model = e_mail_shell_backend_get_folder_tree_model (
		E_MAIL_SHELL_BACKEND (shell_backend));

	folder_tree = em_folder_tree_new_with_model (model);
	selected = message_list_get_selected (message_list);

	folder = message_list->folder;

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

	message_list = e_mail_reader_get_message_list (reader);
	window = e_mail_reader_get_window (reader);

	folder = message_list->folder;
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

	message_list = e_mail_reader_get_message_list (reader);
	window = e_mail_reader_get_window (reader);

	folder = message_list->folder;
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

	message_list = e_mail_reader_get_message_list (reader);
	window = e_mail_reader_get_window (reader);

	folder = message_list->folder;
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
	EMFormatHTMLDisplay *html_display;
	GtkHTML *html;
	const gchar *action_name;
	gboolean selection_active;

	html_display = e_mail_reader_get_html_display (reader);
	html = EM_FORMAT_HTML (html_display)->html;

	gtk_html_select_all (html);

	action_name = "mail-clipboard-copy";
	action = e_mail_reader_get_action (reader, action_name);
	selection_active = gtk_html_command (html, "is-selection-active");
	gtk_action_set_sensitive (action, selection_active);
}

static void
action_mail_show_all_headers_cb (GtkToggleAction *action,
                                 EMailReader *reader)
{
	EMFormatHTMLDisplay *html_display;
	em_format_mode_t mode;

	html_display = e_mail_reader_get_html_display (reader);

	if (gtk_toggle_action_get_active (action))
		mode = EM_FORMAT_ALLHEADERS;
	else
		mode = EM_FORMAT_NORMAL;

	em_format_set_mode (EM_FORMAT (html_display), mode);
}

static void
action_mail_show_source_cb (GtkAction *action,
                            EMailReader *reader)
{
	EMFormatHTMLDisplay *html_display;
	EMailShellBackend *mail_shell_backend;
	EShellBackend *shell_backend;
	MessageList *message_list;
	CamelFolder *folder;
	GtkWidget *browser;
	GPtrArray *uids;
	const gchar *folder_uri;

	message_list = e_mail_reader_get_message_list (reader);
	shell_backend = e_mail_reader_get_shell_backend (reader);
	mail_shell_backend = E_MAIL_SHELL_BACKEND (shell_backend);

	folder = message_list->folder;
	folder_uri = message_list->folder_uri;
	uids = message_list_get_selected (message_list);
	g_return_if_fail (uids->len > 0);

	browser = e_mail_browser_new (mail_shell_backend);
	reader = E_MAIL_READER (browser);
	html_display = e_mail_reader_get_html_display (reader);
	em_format_set_mode (EM_FORMAT (html_display), EM_FORMAT_SOURCE);
	e_mail_reader_set_folder (reader, folder, folder_uri);
	e_mail_reader_set_message (reader, uids->pdata[0], FALSE);
	gtk_widget_show (browser);

	message_list_free_uids (message_list, uids);
}

static void
action_mail_toggle_important_cb (GtkAction *action,
                                 EMailReader *reader)
{
	MessageList *message_list;
	CamelFolder *folder;
	GPtrArray *uids;
	guint ii;

	message_list = e_mail_reader_get_message_list (reader);

	folder = message_list->folder;
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
action_mail_uri_call_to_cb (GtkAction *action,
                            EMailReader *reader)
{
	/* FIXME */
	g_print ("Action: %s\n", gtk_action_get_name (GTK_ACTION (action)));
}

static void
action_mail_uri_copy_cb (GtkAction *action,
                         EMailReader *reader)
{
	/* FIXME */
	g_print ("Action: %s\n", gtk_action_get_name (GTK_ACTION (action)));
}

static void
action_mail_uri_copy_address_cb (GtkAction *action,
                                 EMailReader *reader)
{
	/* FIXME */
	g_print ("Action: %s\n", gtk_action_get_name (GTK_ACTION (action)));
}

static void
action_mail_uri_to_search_folder_recipient_cb (GtkAction *action,
                                               EMailReader *reader)
{
	/* FIXME */
	g_print ("Action: %s\n", gtk_action_get_name (GTK_ACTION (action)));
}

static void
action_mail_uri_to_search_folder_sender_cb (GtkAction *action,
                                            EMailReader *reader)
{
	/* FIXME */
	g_print ("Action: %s\n", gtk_action_get_name (GTK_ACTION (action)));
}

static void
action_mail_zoom_100_cb (GtkAction *action,
                         EMailReader *reader)
{
	EMFormatHTMLDisplay *html_display;
	GtkHTML *html;

	html_display = e_mail_reader_get_html_display (reader);
	html = EM_FORMAT_HTML (html_display)->html;

	gtk_html_zoom_reset (html);
}

static void
action_mail_zoom_in_cb (GtkAction *action,
                        EMailReader *reader)
{
	EMFormatHTMLDisplay *html_display;
	GtkHTML *html;

	html_display = e_mail_reader_get_html_display (reader);
	html = EM_FORMAT_HTML (html_display)->html;

	gtk_html_zoom_out (html);
}

static void
action_mail_zoom_out_cb (GtkAction *action,
                         EMailReader *reader)
{
	EMFormatHTMLDisplay *html_display;
	GtkHTML *html;

	html_display = e_mail_reader_get_html_display (reader);
	html = EM_FORMAT_HTML (html_display)->html;

	gtk_html_zoom_out (html);
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
	  N_("_Copy to Folder..."),
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
	  N_("_Move to Folder..."),
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
	  "<Control>p",
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

	{ "mail-uri-call-to",
	  NULL,
	  N_("C_all To..."),
	  NULL,
	  NULL,  /* XXX Add a tooltip! */
	  G_CALLBACK (action_mail_uri_call_to_cb) },

	{ "mail-uri-copy",
	  NULL,
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

static EPopupActionEntry mail_reader_popup_entries[] = {

	{ "mail-popup-copy",
	  NULL,
	  "mail-copy" },

	{ "mail-popup-delete",
	  NULL,
	  "mail-delete" },

	{ "mail-popup-flag-for-followup",
	  N_("Mark for Follo_w Up..."),
	  "mail-flag-for-followup" },

	{ "mail-popup-forward",
	  NULL,
	  "mail-forward" },

	{ "mail-popup-mark-important",
	  N_("Mark as _Important"),
	  "mail-mark-important" },

	{ "mail-popup-mark-junk",
	  N_("Mark as _Junk"),
	  "mail-mark-junk" },

	{ "mail-popup-mark-notjunk",
	  N_("Mark as _Not Junk"),
	  "mail-mark-notjunk" },

	{ "mail-popup-mark-read",
	  N_("Mark as _Read"),
	  "mail-mark-read" },

	{ "mail-popup-mark-unimportant",
	  N_("Mark as Uni_mportant"),
	  "mail-mark-unimportant" },

	{ "mail-popup-mark-unread",
	  N_("Mark as _Unread"),
	  "mail-mark-unread" },

	{ "mail-popup-message-edit",
	  NULL,
	  "mail-message-edit" },

	{ "mail-popup-move",
	  NULL,
	  "mail-move" },

	{ "mail-popup-print",
	  NULL,
	  "mail-print" },

	{ "mail-popup-reply-all",
	  NULL,
	  "mail-reply-all" },

	{ "mail-popup-reply-sender",
	  NULL,
	  "mail-reply-sender" },

	{ "mail-popup-save-as",
	  NULL,
	  "mail-save-as" },

	{ "mail-popup-undelete",
	  NULL,
	  "mail-undelete" }
};

static GtkToggleActionEntry mail_reader_toggle_entries[] = {

	{ "mail-caret-mode",
	  NULL,
	  N_("_Caret Mode"),
	  "F7",
	  N_("Show a blinking cursor in the body of displayed messages"),
	  NULL,  /* No callback required */
	  FALSE },

	{ "mail-show-all-headers",
	  NULL,
	  N_("All Message _Headers"),
	  NULL,
	  N_("Show messages with all email headers"),
	  G_CALLBACK (action_mail_show_all_headers_cb),
	  FALSE }
};

static gboolean
mail_reader_html_button_release_event_cb (EMailReader *reader,
                                          GdkEventButton *button,
                                          GtkHTML *html)
{
	GtkAction *action;
	const gchar *action_name;
	gboolean selection_active;

	action_name = "mail-clipboard-copy";
	action = e_mail_reader_get_action (reader, action_name);
	selection_active = gtk_html_command (html, "is-selection-active");
	gtk_action_set_sensitive (action, selection_active);

	return FALSE;
}

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

static gboolean
mail_reader_key_press_event_cb (EMailReader *reader,
                                GdkEventKey *event)
{
	const gchar *action_name;

	if ((event->state & GDK_CONTROL_MASK) != 0)
		goto ctrl;

	/* <keyval> alone */
	switch (event->keyval) {
		case GDK_Delete:
		case GDK_KP_Delete:
			action_name = "mail-delete";
			break;

		case GDK_Return:
		case GDK_KP_Enter:
		case GDK_ISO_Enter:
			action_name = "mail-message-open";
			break;

		case GDK_period:
			action_name = "mail-next-unread";
			break;

		case GDK_comma:
			action_name = "mail-previous-unread";
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

	goto exit;

ctrl:

	/* Ctrl + <keyval> */
	switch (event->keyval) {
		case GDK_period:
			action_name = "mail-next-unread";
			break;

		case GDK_comma:
			action_name = "mail-previous-unread";
			break;

		default:
			return FALSE;
	}

exit:
	e_mail_reader_activate (reader, action_name);

	return TRUE;
}

static gint
mail_reader_key_press_cb (EMailReader *reader,
                          gint row,
                          ETreePath path,
                          gint col,
                          GdkEvent *event)
{
	return mail_reader_key_press_event_cb (reader, &event->key);
}

static gboolean
mail_reader_message_read_cb (EMailReader *reader)
{
	MessageList *message_list;
	const gchar *uid;

	message_list = e_mail_reader_get_message_list (reader);

	uid = g_object_get_data (G_OBJECT (reader), "mark-read-uid");
	g_return_val_if_fail (uid != NULL, FALSE);

	if (g_strcmp0 (message_list->cursor_uid, uid) == 0)
		e_mail_reader_mark_as_read (reader, uid);

	return FALSE;
}

static void
mail_reader_message_loaded_cb (CamelFolder *folder,
                               const gchar *message_uid,
                               CamelMimeMessage *message,
                               gpointer user_data,
                               CamelException *ex)
{
	EMailReader *reader = user_data;
	EMFormatHTMLDisplay *html_display;
	MessageList *message_list;
	EShellBackend *shell_backend;
	EShellSettings *shell_settings;
	EShell *shell;
	EMEvent *event;
	EMEventTargetMessage *target;
	gboolean mark_read;
	gint timeout_interval;

	html_display = e_mail_reader_get_html_display (reader);
	message_list = e_mail_reader_get_message_list (reader);

	shell_backend = e_mail_reader_get_shell_backend (reader);
	shell = e_shell_backend_get_shell (shell_backend);
	shell_settings = e_shell_get_shell_settings (shell);

	/* If the user picked a different message in the time it took
	 * to fetch this message, then don't bother rendering it. */
	if (g_strcmp0 (message_list->cursor_uid, message_uid) != 0)
		return;

	/** @Event: message.reading
	 * @Title: Viewing a message
	 * @Target: EMEventTargetMessage
	 *
	 * message.reading is emitted whenever a user views a message.
	 */
	event = em_event_peek ();
	target = em_event_target_new_message (
		event, folder, message, message_uid, 0);
	e_event_emit (
		(EEvent *) event, "message.reading",
		(EEventTarget *) target);

	em_format_format (
		EM_FORMAT (html_display), folder, message_uid, message);

	/* Reset the shell view icon. */
	e_shell_event (shell, "mail-icon", "evolution-mail");

	/* Determine whether to mark the message as read. */
	mark_read = e_shell_settings_get_boolean (
		shell_settings, "mail-mark-seen");
	timeout_interval = e_shell_settings_get_int (
		shell_settings, "mail-mark-seen-timeout");

	g_object_set_data_full (
		G_OBJECT (reader), "mark-read-uid",
		g_strdup (message_uid), (GDestroyNotify) g_free);

	if (message_list->seen_id > 0)
		g_source_remove (message_list->seen_id);

	if (mark_read) {
		message_list->seen_id = g_timeout_add (
			timeout_interval, (GSourceFunc)
			mail_reader_message_read_cb, reader);

	} else if (camel_exception_is_set (ex)) {
		GtkHTMLStream *stream;

		/* Display the error inline and clear the exception. */
		stream = gtk_html_begin (
			EM_FORMAT_HTML (html_display)->html);
		gtk_html_stream_printf (
			stream, "<h2>%s</h2><p>%s</p>",
			_("Unable to retrieve message"),
			ex->desc);
		gtk_html_stream_close (stream, GTK_HTML_STREAM_OK);
		camel_exception_clear (ex);
	}

	e_mail_reader_update_actions (reader);

	/* We referenced this in the call to mail_get_messagex(). */
	g_object_unref (reader);
}

static gboolean
mail_reader_message_selected_timeout_cb (EMailReader *reader)
{
	EMFormatHTMLDisplay *html_display;
	MessageList *message_list;
	const gchar *cursor_uid;
	const gchar *format_uid;
	const gchar *key;

	html_display = e_mail_reader_get_html_display (reader);
	message_list = e_mail_reader_get_message_list (reader);

	cursor_uid = message_list->cursor_uid;
	format_uid = EM_FORMAT (html_display)->uid;

	if (cursor_uid != NULL) {
		if (g_strcmp0 (cursor_uid, format_uid) != 0)
			mail_get_messagex (
				message_list->folder, cursor_uid,
				mail_reader_message_loaded_cb,
				g_object_ref (reader),
				mail_msg_fast_ordered_push);
	} else
		em_format_format (EM_FORMAT (html_display), NULL, NULL, NULL);

	key = "message-selected-timeout";
	g_object_set_data (G_OBJECT (reader), key, NULL);

	return FALSE;
}

static void
mail_reader_message_selected_cb (EMailReader *reader,
                                 const gchar *uid)
{
	const gchar *key;
	guint source_id;
	gpointer data;

	/* XXX This is kludgy, but we have no other place to store
	 *     timeout state information. */

	key = "message-selected-timeout";
	data = g_object_get_data (G_OBJECT (reader), key);
	source_id = GPOINTER_TO_UINT (data);

	if (source_id > 0)
		g_source_remove (source_id);

	source_id = g_timeout_add (
		100, (GSourceFunc)
		mail_reader_message_selected_timeout_cb, reader);

	data = GUINT_TO_POINTER (source_id);
	g_object_set_data (G_OBJECT (reader), key, data);

	e_mail_reader_changed (reader);
}

static void
mail_reader_emit_folder_loaded (EMailReader *reader)
{
	g_signal_emit (reader, signals[FOLDER_LOADED], 0);
}

static void
mail_reader_set_folder (EMailReader *reader,
                        CamelFolder *folder,
                        const gchar *folder_uri)
{
	EMFormatHTMLDisplay *html_display;
	MessageList *message_list;
	gboolean outgoing;

	html_display = e_mail_reader_get_html_display (reader);
	message_list = e_mail_reader_get_message_list (reader);

	outgoing = folder != NULL && folder_uri != NULL && (
		em_utils_folder_is_drafts (folder, folder_uri) ||
		em_utils_folder_is_outbox (folder, folder_uri) ||
		em_utils_folder_is_sent (folder, folder_uri));

	if (message_list->folder != NULL)
		mail_sync_folder (message_list->folder, NULL, NULL);

	em_format_format (EM_FORMAT (html_display), NULL, NULL, NULL);
	message_list_set_folder (message_list, folder, folder_uri, outgoing);

	mail_reader_emit_folder_loaded (reader);
}

static void
mail_reader_set_message (EMailReader *reader,
                         const gchar *uid,
                         gboolean mark_read)
{
	MessageList *message_list;

	message_list = e_mail_reader_get_message_list (reader);
	message_list_select_uid (message_list, uid);
}

static void
mail_reader_init_charset_actions (EMailReader *reader)
{
	GtkActionGroup *action_group;
	GtkRadioAction *default_action;
	GSList *radio_group;

	action_group = e_mail_reader_get_action_group (reader);

	radio_group = e_charset_add_radio_actions (
		action_group, "mail-charset-", NULL,
		G_CALLBACK (action_mail_charset_cb), reader);

	/* XXX Add a tooltip! */
	default_action = gtk_radio_action_new (
		"mail-charset-default", _("Default"), NULL, NULL, -1);

	gtk_radio_action_set_group (default_action, radio_group);

	g_signal_connect (
		default_action, "changed",
		G_CALLBACK (action_mail_charset_cb), reader);

	gtk_action_group_add_action (
		action_group, GTK_ACTION (default_action));

	gtk_radio_action_set_current_value (default_action, -1);
}

static void
mail_reader_class_init (EMailReaderIface *iface)
{
	iface->set_folder = mail_reader_set_folder;
	iface->set_message = mail_reader_set_message;

	signals[CHANGED] = g_signal_new (
		"changed",
		G_OBJECT_CLASS_TYPE (iface),
		G_SIGNAL_RUN_FIRST,
		0, NULL, NULL,
		g_cclosure_marshal_VOID__VOID,
		G_TYPE_NONE, 0);

	signals[FOLDER_LOADED] = g_signal_new (
		"folder-loaded",
		G_OBJECT_CLASS_TYPE (iface),
		G_SIGNAL_RUN_FIRST,
		0, NULL, NULL,
		g_cclosure_marshal_VOID__VOID,
		G_TYPE_NONE, 0);

	signals[SHOW_SEARCH_BAR] = g_signal_new (
		"show-search-bar",
		G_OBJECT_CLASS_TYPE (iface),
		G_SIGNAL_RUN_FIRST | G_SIGNAL_ACTION,
		G_STRUCT_OFFSET (EMailReaderIface, show_search_bar),
		NULL, NULL,
		g_cclosure_marshal_VOID__VOID,
		G_TYPE_NONE, 0);
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
			(GInstanceInitFunc) NULL,
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
	EShell *shell;
	EShellBackend *shell_backend;
	EShellSettings *shell_settings;
	EMFormatHTMLDisplay *html_display;
	GtkActionGroup *action_group;
	MessageList *message_list;
	GConfBridge *bridge;
	GtkAction *action;
	GtkHTML *html;
	const gchar *action_name;
	const gchar *key;

	g_return_if_fail (E_IS_MAIL_READER (reader));

	action_group = e_mail_reader_get_action_group (reader);
	html_display = e_mail_reader_get_html_display (reader);
	message_list = e_mail_reader_get_message_list (reader);
	shell_backend = e_mail_reader_get_shell_backend (reader);

	shell = e_shell_backend_get_shell (shell_backend);
	shell_settings = e_shell_get_shell_settings (shell);

	html = EM_FORMAT_HTML (html_display)->html;

	gtk_action_group_add_actions (
		action_group, mail_reader_entries,
		G_N_ELEMENTS (mail_reader_entries), reader);
	e_action_group_add_popup_actions (
		action_group, mail_reader_popup_entries,
		G_N_ELEMENTS (mail_reader_popup_entries));
	gtk_action_group_add_toggle_actions (
		action_group, mail_reader_toggle_entries,
		G_N_ELEMENTS (mail_reader_toggle_entries), reader);

	mail_reader_init_charset_actions (reader);

	/* Bind GObject properties to GConf keys. */

	bridge = gconf_bridge_get ();

	action_name = "mail-caret-mode";
	key = "/apps/evolution/mail/display/caret_mode";
	action = e_mail_reader_get_action (reader, action_name);
	gconf_bridge_bind_property (bridge, key, G_OBJECT (action), "active");

	action_name = "mail-show-all-headers";
	key = "/apps/evolution/mail/display/show_all_headers";
	action = e_mail_reader_get_action (reader, action_name);
	gconf_bridge_bind_property (bridge, key, G_OBJECT (action), "active");

	/* Fine tuning. */

	action_name = "mail-clipboard-copy";
	action = e_mail_reader_get_action (reader, action_name);
	gtk_action_set_sensitive (action, FALSE);

	action_name = "mail-delete";
	action = e_mail_reader_get_action (reader, action_name);
	g_object_set (action, "short-label", _("Delete"), NULL);

	action_name = "mail-next";
	action = e_mail_reader_get_action (reader, action_name);
	g_object_set (action, "short-label", _("Next"), NULL);

	action_name = "mail-previous";
	action = e_mail_reader_get_action (reader, action_name);
	g_object_set (action, "short-label", _("Previous"), NULL);

	action_name = "mail-reply-sender";
	action = e_mail_reader_get_action (reader, action_name);
	g_object_set (action, "short-label", _("Reply"), NULL);

	/* Bind properties. */

	e_binding_new_full (
		G_OBJECT (shell_settings), "mail-citation-color",
		G_OBJECT (html_display), "citation-color",
		e_binding_transform_string_to_color,
		NULL, NULL);

	e_binding_new (
		G_OBJECT (shell_settings), "mail-image-loading-policy",
		G_OBJECT (html_display), "image-loading-policy");

	e_binding_new (
		G_OBJECT (shell_settings), "mail-only-local-photos",
		G_OBJECT (html_display), "only-local-photos");

	e_binding_new (
		G_OBJECT (shell_settings), "mail-show-animated-images",
		G_OBJECT (html), "animate");

	e_binding_new (
		G_OBJECT (shell_settings), "mail-show-sender-photo",
		G_OBJECT (html_display), "show-sender-photo");

	action_name = "mail-caret-mode";
	action = e_mail_reader_get_action (reader, action_name);

	e_mutual_binding_new (
		G_OBJECT (action), "active",
		G_OBJECT (html), "caret-mode");

	/* Connect signals. */

	g_signal_connect_swapped (
		html, "button-release-event",
		G_CALLBACK (mail_reader_html_button_release_event_cb), reader);

	g_signal_connect_swapped (
		html, "key-press-event",
		G_CALLBACK (mail_reader_key_press_event_cb), reader);

	g_signal_connect_swapped (
		message_list, "message-selected",
		G_CALLBACK (mail_reader_message_selected_cb), reader);

	g_signal_connect_swapped (
		message_list, "message-list-built",
		G_CALLBACK (mail_reader_emit_folder_loaded), reader);

	g_signal_connect_swapped (
		message_list->tree, "double-click",
		G_CALLBACK (mail_reader_double_click_cb), reader);

	g_signal_connect_swapped (
		message_list->tree, "key-press",
		G_CALLBACK (mail_reader_key_press_cb), reader);

	g_signal_connect_swapped (
		message_list->tree, "selection-change",
		G_CALLBACK (e_mail_reader_changed), reader);
}

void
e_mail_reader_changed (EMailReader *reader)
{
	g_return_if_fail (E_IS_MAIL_READER (reader));

	g_signal_emit (reader, signals[CHANGED], 0);
}

guint32
e_mail_reader_check_state (EMailReader *reader)
{
	MessageList *message_list;
	GPtrArray *uids;
	CamelFolder *folder;
	CamelStore *store = NULL;
	const gchar *folder_uri;
	const gchar *tag;
	gboolean can_clear_flags = FALSE;
	gboolean can_flag_completed = FALSE;
	gboolean can_flag_for_followup = FALSE;
	gboolean has_deleted = FALSE;
	gboolean has_important = FALSE;
	gboolean has_junk = FALSE;
	gboolean has_not_junk = FALSE;
	gboolean has_read = FALSE;
	gboolean has_undeleted = FALSE;
	gboolean has_unimportant = FALSE;
	gboolean has_unread = FALSE;
	gboolean drafts_or_outbox;
	gboolean store_supports_vjunk = FALSE;
	guint32 state = 0;
	guint ii;

	g_return_val_if_fail (E_IS_MAIL_READER (reader), 0);

	message_list = e_mail_reader_get_message_list (reader);
	uids = message_list_get_selected (message_list);
	folder_uri = message_list->folder_uri;
	folder = message_list->folder;

	if (folder != NULL) {
		store = CAMEL_STORE (folder->parent_store);
		store_supports_vjunk = (store->flags & CAMEL_STORE_VJUNK);
	}

	drafts_or_outbox =
		em_utils_folder_is_drafts (folder, folder_uri) ||
		em_utils_folder_is_outbox (folder, folder_uri);

	for (ii = 0; ii < uids->len; ii++) {
		CamelMessageInfo *info;
		guint32 flags;

		info = camel_folder_get_message_info (
			folder, uids->pdata[ii]);
		if (info == NULL)
			continue;

		flags = camel_message_info_flags (info);

		if (flags & CAMEL_MESSAGE_SEEN)
			has_read = TRUE;
		else
			has_unread = TRUE;

		if (drafts_or_outbox) {
			has_junk = FALSE;
			has_not_junk = FALSE;
		} else if (store_supports_vjunk) {
			guint32 bitmask;

			/* XXX Strictly speaking, this logic is correct.
			 *     Problem is there's nothing in the message
			 *     list that indicates whether a message is
			 *     already marked "Not Junk".  So the user may
			 *     think the "Not Junk" button is enabling and
			 *     disabling itself randomly as he reads mail. */

			if (flags & CAMEL_MESSAGE_JUNK)
				has_junk = TRUE;
			if (flags & CAMEL_MESSAGE_NOTJUNK)
				has_not_junk = TRUE;

			bitmask = CAMEL_MESSAGE_JUNK | CAMEL_MESSAGE_NOTJUNK;

			/* If neither junk flag is set, the
			 * message can be marked either way. */
			if ((flags & bitmask) == 0) {
				has_junk = TRUE;
				has_not_junk = TRUE;
			}

		} else {
			has_junk = TRUE;
			has_not_junk = TRUE;
		}

		if (flags & CAMEL_MESSAGE_DELETED)
			has_deleted = TRUE;
		else
			has_undeleted = TRUE;

		if (flags & CAMEL_MESSAGE_FLAGGED)
			has_important = TRUE;
		else
			has_unimportant = TRUE;

		tag = camel_message_info_user_tag (info, "follow-up");
		if (tag != NULL && *tag != '\0') {
			can_clear_flags = TRUE;
			tag = camel_message_info_user_tag (
				info, "completed-on");
			if (tag != NULL && *tag != '\0')
				can_flag_completed = TRUE;
		} else
			can_flag_for_followup = TRUE;
	}

	if (uids->len == 1)
		state |= E_MAIL_READER_SELECTION_SINGLE;
	if (uids->len > 1)
		state |= E_MAIL_READER_SELECTION_MULTIPLE;
	if (!drafts_or_outbox && uids->len == 1)
		state |= E_MAIL_READER_SELECTION_CAN_ADD_SENDER;
#if 0  /* FIXME */
	if (can_edit)
		state |= E_MAIL_READER_SELECTION_CAN_EDIT;
#endif
	if (can_clear_flags)
		state |= E_MAIL_READER_SELECTION_FLAG_CLEAR;
	if (can_flag_completed)
		state |= E_MAIL_READER_SELECTION_FLAG_COMPLETED;
	if (can_flag_for_followup)
		state |= E_MAIL_READER_SELECTION_FLAG_FOLLOWUP;
	if (has_deleted)
		state |= E_MAIL_READER_SELECTION_HAS_DELETED;
	if (has_important)
		state |= E_MAIL_READER_SELECTION_HAS_IMPORTANT;
	if (has_junk)
		state |= E_MAIL_READER_SELECTION_HAS_JUNK;
	if (has_not_junk)
		state |= E_MAIL_READER_SELECTION_HAS_NOT_JUNK;
	if (has_read)
		state |= E_MAIL_READER_SELECTION_HAS_READ;
	if (has_undeleted)
		state |= E_MAIL_READER_SELECTION_HAS_UNDELETED;
	if (has_unimportant)
		state |= E_MAIL_READER_SELECTION_HAS_UNIMPORTANT;
	if (has_unread)
		state |= E_MAIL_READER_SELECTION_HAS_UNREAD;
#if 0  /* FIXME */
	if (has_callto_uri)
		state |= E_MAIL_READER_SELECTION_HAS_URI_CALLTO;
	if (has_http_uri)
		state |= E_MAIL_READER_SELECTION_HAS_URI_HTTP;
	if (has_mailto_uri)
		state |= E_MAIL_READER_SELECTION_HAS_URI_MAILTO;
	if (is_mailing_list)
		state |= E_MAIL_READER_SELECTION_IS_MAILING_LIST;
#endif

	em_utils_uids_free (uids);

	return state;

}

void
e_mail_reader_update_actions (EMailReader *reader)
{
	EShell *shell;
	EShellBackend *shell_backend;
	EShellSettings *shell_settings;
	GtkAction *action;
	GtkActionGroup *action_group;
	const gchar *action_name;
	gboolean sensitive;
	guint32 state;

	/* Be descriptive. */
	gboolean any_messages_selected;
	gboolean disable_printing;
	gboolean enable_flag_clear;
	gboolean enable_flag_completed;
	gboolean enable_flag_for_followup;
	gboolean single_message_selected;
	gboolean multiple_messages_selected;
	gboolean selection_has_deleted_messages;
	gboolean selection_has_important_messages;
	gboolean selection_has_junk_messages;
	gboolean selection_has_not_junk_messages;
	gboolean selection_has_read_messages;
	gboolean selection_has_undeleted_messages;
	gboolean selection_has_unimportant_messages;
	gboolean selection_has_unread_messages;
	gboolean selection_is_mailing_list;

	g_return_if_fail (E_IS_MAIL_READER (reader));

	action_group = e_mail_reader_get_action_group (reader);
	state = e_mail_reader_check_state (reader);

	shell_backend = e_mail_reader_get_shell_backend (reader);
	shell = e_shell_backend_get_shell (shell_backend);
	shell_settings = e_shell_get_shell_settings (shell);

	disable_printing = e_shell_settings_get_boolean (
		shell_settings, "disable-printing");

	single_message_selected =
		(state & E_MAIL_READER_SELECTION_SINGLE);
	multiple_messages_selected =
		(state & E_MAIL_READER_SELECTION_MULTIPLE);
	/* FIXME Missing booleans */
	enable_flag_clear =
		(state & E_MAIL_READER_SELECTION_FLAG_CLEAR);
	enable_flag_completed =
		(state & E_MAIL_READER_SELECTION_FLAG_COMPLETED);
	enable_flag_for_followup =
		(state & E_MAIL_READER_SELECTION_FLAG_FOLLOWUP);
	selection_has_deleted_messages =
		(state & E_MAIL_READER_SELECTION_HAS_DELETED);
	selection_has_important_messages =
		(state & E_MAIL_READER_SELECTION_HAS_IMPORTANT);
	selection_has_junk_messages =
		(state & E_MAIL_READER_SELECTION_HAS_JUNK);
	selection_has_not_junk_messages =
		(state & E_MAIL_READER_SELECTION_HAS_NOT_JUNK);
	selection_has_read_messages =
		(state & E_MAIL_READER_SELECTION_HAS_READ);
	selection_has_undeleted_messages =
		(state & E_MAIL_READER_SELECTION_HAS_UNDELETED);
	selection_has_unimportant_messages =
		(state & E_MAIL_READER_SELECTION_HAS_UNIMPORTANT);
	selection_has_unread_messages =
		(state & E_MAIL_READER_SELECTION_HAS_UNREAD);
	/* FIXME Missing booleans */
	selection_is_mailing_list =
		(state & E_MAIL_READER_SELECTION_IS_MAILING_LIST);

	any_messages_selected =
		(single_message_selected || multiple_messages_selected);

	action_name = "mail-check-for-junk";
	sensitive = any_messages_selected;
	action = e_mail_reader_get_action (reader, action_name);
	gtk_action_set_sensitive (action, sensitive);

	action_name = "mail-copy";
	sensitive = any_messages_selected;
	action = e_mail_reader_get_action (reader, action_name);
	gtk_action_set_sensitive (action, sensitive);

	action_name = "mail-delete";
	sensitive = selection_has_undeleted_messages;
	action = e_mail_reader_get_action (reader, action_name);
	gtk_action_set_sensitive (action, sensitive);

	action_name = "mail-filters-apply";
	sensitive = any_messages_selected;
	action = e_mail_reader_get_action (reader, action_name);
	gtk_action_set_sensitive (action, sensitive);

	action_name = "mail-forward";
	sensitive = any_messages_selected;
	action = e_mail_reader_get_action (reader, action_name);
	gtk_action_set_sensitive (action, sensitive);

	action_name = "mail-forward-attached";
	sensitive = any_messages_selected;
	action = e_mail_reader_get_action (reader, action_name);
	gtk_action_set_sensitive (action, sensitive);

	action_name = "mail-forward-inline";
	sensitive = single_message_selected;
	action = e_mail_reader_get_action (reader, action_name);
	gtk_action_set_sensitive (action, sensitive);

	action_name = "mail-forward-quoted";
	sensitive = single_message_selected;
	action = e_mail_reader_get_action (reader, action_name);
	gtk_action_set_sensitive (action, sensitive);

	action_name = "mail-load-images";
	sensitive = single_message_selected;
	action = e_mail_reader_get_action (reader, action_name);
	gtk_action_set_sensitive (action, sensitive);

	action_name = "mail-mark-important";
	sensitive = selection_has_unimportant_messages;
	action = e_mail_reader_get_action (reader, action_name);
	gtk_action_set_sensitive (action, sensitive);

	action_name = "mail-mark-junk";
	sensitive = selection_has_not_junk_messages;
	action = e_mail_reader_get_action (reader, action_name);
	gtk_action_set_sensitive (action, sensitive);

	action_name = "mail-mark-notjunk";
	sensitive = selection_has_junk_messages;
	action = e_mail_reader_get_action (reader, action_name);
	gtk_action_set_sensitive (action, sensitive);

	action_name = "mail-mark-read";
	sensitive = selection_has_unread_messages;
	action = e_mail_reader_get_action (reader, action_name);
	gtk_action_set_sensitive (action, sensitive);

	action_name = "mail-mark-unimportant";
	sensitive = selection_has_important_messages;
	action = e_mail_reader_get_action (reader, action_name);
	gtk_action_set_sensitive (action, sensitive);

	action_name = "mail-mark-unread";
	sensitive = selection_has_read_messages;
	action = e_mail_reader_get_action (reader, action_name);
	gtk_action_set_sensitive (action, sensitive);

	action_name = "mail-message-edit";
	sensitive = single_message_selected;
	action = e_mail_reader_get_action (reader, action_name);
	gtk_action_set_sensitive (action, sensitive);

	action_name = "mail-message-open";
	sensitive = any_messages_selected;
	action = e_mail_reader_get_action (reader, action_name);
	gtk_action_set_sensitive (action, sensitive);

	action_name = "mail-move";
	sensitive = any_messages_selected;
	action = e_mail_reader_get_action (reader, action_name);
	gtk_action_set_sensitive (action, sensitive);

	action_name = "mail-next-important";
	sensitive = single_message_selected;
	action = e_mail_reader_get_action (reader, action_name);
	gtk_action_set_sensitive (action, sensitive);

	action_name = "mail-next-thread";
	sensitive = single_message_selected;
	action = e_mail_reader_get_action (reader, action_name);
	gtk_action_set_sensitive (action, sensitive);

	action_name = "mail-next-unread";
	sensitive = single_message_selected;
	action = e_mail_reader_get_action (reader, action_name);
	gtk_action_set_sensitive (action, sensitive);

	action_name = "mail-previous-important";
	sensitive = single_message_selected;
	action = e_mail_reader_get_action (reader, action_name);
	gtk_action_set_sensitive (action, sensitive);

	action_name = "mail-previous-unread";
	sensitive = single_message_selected;
	action = e_mail_reader_get_action (reader, action_name);
	gtk_action_set_sensitive (action, sensitive);

	action_name = "mail-print";
	sensitive = single_message_selected && !disable_printing;
	action = e_mail_reader_get_action (reader, action_name);
	gtk_action_set_sensitive (action, sensitive);

	action_name = "mail-print-preview";
	sensitive = single_message_selected && !disable_printing;
	action = e_mail_reader_get_action (reader, action_name);
	gtk_action_set_sensitive (action, sensitive);

	action_name = "mail-redirect";
	sensitive = single_message_selected;
	action = e_mail_reader_get_action (reader, action_name);
	gtk_action_set_sensitive (action, sensitive);

	action_name = "mail-reply-all";
	sensitive = single_message_selected;
	action = e_mail_reader_get_action (reader, action_name);
	gtk_action_set_sensitive (action, sensitive);

	action_name = "mail-reply-list";
	sensitive = single_message_selected && selection_is_mailing_list;
	action = e_mail_reader_get_action (reader, action_name);
	gtk_action_set_sensitive (action, sensitive);

	action_name = "mail-reply-post";
	sensitive = single_message_selected;
	action = e_mail_reader_get_action (reader, action_name);
	gtk_action_set_sensitive (action, sensitive);

	action_name = "mail-reply-sender";
	sensitive = single_message_selected;
	action = e_mail_reader_get_action (reader, action_name);
	gtk_action_set_sensitive (action, sensitive);

	action_name = "mail-save-as";
	sensitive = any_messages_selected;
	action = e_mail_reader_get_action (reader, action_name);
	gtk_action_set_sensitive (action, sensitive);

	action_name = "mail-select-all";
	sensitive = single_message_selected;
	action = e_mail_reader_get_action (reader, action_name);
	gtk_action_set_sensitive (action, sensitive);

	action_name = "mail-show-source";
	sensitive = single_message_selected;
	action = e_mail_reader_get_action (reader, action_name);
	gtk_action_set_sensitive (action, sensitive);

	action_name = "mail-undelete";
	sensitive = selection_has_deleted_messages;
	action = e_mail_reader_get_action (reader, action_name);
	gtk_action_set_sensitive (action, sensitive);

	action_name = "mail-zoom-100";
	sensitive = single_message_selected;
	action = e_mail_reader_get_action (reader, action_name);
	gtk_action_set_sensitive (action, sensitive);

	action_name = "mail-zoom-in";
	sensitive = single_message_selected;
	action = e_mail_reader_get_action (reader, action_name);
	gtk_action_set_sensitive (action, sensitive);

	action_name = "mail-zoom-out";
	sensitive = single_message_selected;
	action = e_mail_reader_get_action (reader, action_name);
	gtk_action_set_sensitive (action, sensitive);
}

GtkAction *
e_mail_reader_get_action (EMailReader *reader,
                          const gchar *action_name)
{
	GtkActionGroup *action_group;
	GtkAction *action;

	g_return_val_if_fail (E_IS_MAIL_READER (reader), NULL);
	g_return_val_if_fail (action_name != NULL, NULL);

	action_group = e_mail_reader_get_action_group (reader);
	action = gtk_action_group_get_action (action_group, action_name);

	if (action == NULL)
		g_critical (
			"%s: action `%s' not found", G_STRFUNC, action_name);

	return action;
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

EShellBackend *
e_mail_reader_get_shell_backend (EMailReader *reader)
{
	EMailReaderIface *iface;

	g_return_val_if_fail (E_IS_MAIL_READER (reader), NULL);

	iface = E_MAIL_READER_GET_IFACE (reader);
	g_return_val_if_fail (iface->get_shell_backend != NULL, NULL);

	return iface->get_shell_backend (reader);
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

void
e_mail_reader_set_folder (EMailReader *reader,
                          CamelFolder *folder,
                          const gchar *folder_uri)
{
	EMailReaderIface *iface;

	g_return_if_fail (E_IS_MAIL_READER (reader));

	iface = E_MAIL_READER_GET_IFACE (reader);
	g_return_if_fail (iface->set_folder != NULL);

	iface->set_folder (reader, folder, folder_uri);
}

/* Helper for e_mail_reader_set_folder_uri() */
static void
mail_reader_got_folder_cb (gchar *folder_uri,
                           CamelFolder *folder,
                           gpointer user_data)
{
	EMailReader *reader = user_data;

	e_mail_reader_set_folder (reader, folder, folder_uri);
}

void
e_mail_reader_set_folder_uri (EMailReader *reader,
                              const gchar *folder_uri)
{
	g_return_if_fail (E_IS_MAIL_READER (reader));
	g_return_if_fail (folder_uri != NULL);

	/* Fetch the CamelFolder asynchronously. */
	mail_get_folder (
		folder_uri, 0, mail_reader_got_folder_cb,
		reader, mail_msg_fast_ordered_push);
}

void
e_mail_reader_set_message (EMailReader *reader,
                           const gchar *uid,
                           gboolean mark_read)
{
	EMailReaderIface *iface;

	g_return_if_fail (E_IS_MAIL_READER (reader));

	iface = E_MAIL_READER_GET_IFACE (reader);
	g_return_if_fail (iface->set_message != NULL);

	iface->set_message (reader, uid, mark_read);
}

void
e_mail_reader_create_charset_menu (EMailReader *reader,
                                   GtkUIManager *ui_manager,
                                   guint merge_id)
{
	GtkAction *action;
	const gchar *action_name;
	const gchar *path;
	GSList *list;

	g_return_if_fail (E_IS_MAIL_READER (reader));
	g_return_if_fail (GTK_IS_UI_MANAGER (ui_manager));

	action_name = "mail-charset-default";
	action = e_mail_reader_get_action (reader, action_name);
	g_return_if_fail (action != NULL);

	list = gtk_radio_action_get_group (GTK_RADIO_ACTION (action));
	list = g_slist_copy (list);
	list = g_slist_remove (list, action);
	list = g_slist_sort (list, (GCompareFunc) e_action_compare_by_label);

	path = "/main-menu/view-menu/mail-message-view-actions/mail-encoding-menu";

	while (list != NULL) {
		action = list->data;

		gtk_ui_manager_add_ui (
			ui_manager, merge_id, path,
			gtk_action_get_name (action),
			gtk_action_get_name (action),
			GTK_UI_MANAGER_AUTO, FALSE);

		list = g_slist_delete_link (list, list);
	}

	gtk_ui_manager_ensure_update (ui_manager);
}

void
e_mail_reader_show_search_bar (EMailReader *reader)
{
	g_return_if_fail (E_IS_MAIL_READER (reader));

	g_signal_emit (reader, signals[SHOW_SEARCH_BAR], 0);
}
