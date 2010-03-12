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

#ifdef HAVE_XFREE
#include <X11/XF86keysym.h>
#endif

#include "e-util/e-binding.h"
#include "e-util/e-charset.h"
#include "e-util/e-util.h"
#include "e-util/gconf-bridge.h"
#include "shell/e-shell-utils.h"
#include "widgets/misc/e-popup-action.h"
#include "widgets/misc/e-menu-tool-action.h"

#include "mail/e-mail-browser.h"
#include "mail/e-mail-display.h"
#include "mail/e-mail-reader-utils.h"
#include "mail/em-composer-utils.h"
#include "mail/em-event.h"
#include "mail/em-folder-selector.h"
#include "mail/em-folder-tree.h"
#include "mail/em-utils.h"
#include "mail/mail-autofilter.h"
#include "mail/mail-config.h"
#include "mail/mail-ops.h"
#include "mail/mail-mt.h"
#include "mail/mail-vfolder.h"
#include "mail/message-list.h"

#define E_MAIL_READER_GET_PRIVATE(obj) \
	(mail_reader_get_private (G_OBJECT (obj)))

typedef struct _EMailReaderPrivate EMailReaderPrivate;

struct _EMailReaderPrivate {

	/* This timer runs when the user selects a single message. */
	guint message_selected_timeout_id;

	/* This is the message UID to automatically mark as read
	 * after a short period (specified by a user preference). */
	gchar *mark_read_message_uid;

	/* This is the ID of an asynchronous operation
	 * to retrieve a message from a mail folder. */
	gint retrieving_message_operation_id;

	/* These flags work together to prevent message selection
	 * restoration after a folder switch from automatically
	 * marking the message as read.  We only want that to
	 * happen when the -user- selects a message. */
	guint folder_was_just_selected    : 1;
	guint restoring_message_selection : 1;
};

enum {
	CHANGED,
	FOLDER_LOADED,
	SHOW_SEARCH_BAR,
	UPDATE_ACTIONS,
	LAST_SIGNAL
};

/* Remembers the previously selected folder when transferring messages. */
static gchar *default_xfer_messages_uri;

static GQuark quark_private;
static guint signals[LAST_SIGNAL];

static void
mail_reader_finalize (EMailReaderPrivate *priv)
{
	if (priv->message_selected_timeout_id > 0)
		g_source_remove (priv->message_selected_timeout_id);

	g_free (priv->mark_read_message_uid);

	g_slice_free (EMailReaderPrivate, priv);
}

static EMailReaderPrivate *
mail_reader_get_private (GObject *object)
{
	EMailReaderPrivate *priv;

	priv = g_object_get_qdata (object, quark_private);

	if (G_UNLIKELY (priv == NULL)) {
		priv = g_slice_new0 (EMailReaderPrivate);
		g_object_set_qdata_full (
			object, quark_private, priv,
			(GDestroyNotify) mail_reader_finalize);
	}

	return priv;
}

static void
action_mail_add_sender_cb (GtkAction *action,
                           EMailReader *reader)
{
	EShell *shell;
	EShellBackend *shell_backend;
	CamelMessageInfo *info;
	CamelFolder *folder;
	GPtrArray *uids;
	const gchar *address;

	folder = e_mail_reader_get_folder (reader);
	shell_backend = e_mail_reader_get_shell_backend (reader);
	uids = e_mail_reader_get_selected_uids (reader);

	if (uids->len != 1)
		goto exit;

	info = camel_folder_get_message_info (folder, uids->pdata[0]);
	if (info == NULL)
		goto exit;

	address = camel_message_info_from (info);
	if (address == NULL || *address == '\0')
		goto exit;

	/* XXX EBookShellBackend should be listening for this
	 *     event.  Kind of kludgey, but works for now. */
	shell = e_shell_backend_get_shell (shell_backend);
	e_shell_event (shell, "contact-quick-add-email", (gpointer) address);
	emu_remove_from_mail_cache_1 (address);
exit:
	em_utils_uids_free (uids);
}

static void
action_add_to_address_book_cb (GtkAction *action,
                               EMailReader *reader)
{
	EShell *shell;
	EShellBackend *shell_backend;
	EMFormatHTMLDisplay *html_display;
	CamelInternetAddress *cia;
	EWebView *web_view;
	CamelURL *curl;
	const gchar *uri;
	gchar *email;

	/* This action is defined in EMailDisplay. */

	html_display = e_mail_reader_get_html_display (reader);
	shell_backend = e_mail_reader_get_shell_backend (reader);

	web_view = E_WEB_VIEW (EM_FORMAT_HTML (html_display)->html);

	uri = e_web_view_get_selected_uri (web_view);
	g_return_if_fail (uri != NULL);

	curl = camel_url_new (uri, NULL);
	g_return_if_fail (curl != NULL);

	if (curl->path == NULL || *curl->path == '\0')
		goto exit;

	cia = camel_internet_address_new ();
	if (camel_address_decode (CAMEL_ADDRESS (cia), curl->path) < 0) {
		camel_object_unref (cia);
		goto exit;
	}

	email = camel_address_format (CAMEL_ADDRESS (cia));

	/* XXX EBookShellBackend should be listening for this
	 *     event.  Kind of kludgey, but works for now. */
	shell = e_shell_backend_get_shell (shell_backend);
	e_shell_event (shell, "contact-quick-add-email", email);
	emu_remove_from_mail_cache_1 (curl->path);

	camel_object_unref (cia);
	g_free (email);

exit:
	camel_url_free (curl);
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
	CamelFolder *folder;
	GPtrArray *uids;

	folder = e_mail_reader_get_folder (reader);
	uids = e_mail_reader_get_selected_uids (reader);

	mail_filter_junk (folder, uids);
}

static void
action_mail_copy_cb (GtkAction *action,
                     EMailReader *reader)
{
	CamelFolder *folder;
	GtkWidget *folder_tree;
	GtkWidget *dialog;
	GtkWindow *window;
	GPtrArray *uids;
	const gchar *uri;

	folder = e_mail_reader_get_folder (reader);
	window = e_mail_reader_get_window (reader);
	uids = e_mail_reader_get_selected_uids (reader);

	folder_tree = em_folder_tree_new ();
	emu_restore_folder_tree_state (EM_FOLDER_TREE (folder_tree));

	em_folder_tree_set_excluded (
		EM_FOLDER_TREE (folder_tree),
		EMFT_EXCLUDE_NOSELECT | EMFT_EXCLUDE_VIRTUAL |
		EMFT_EXCLUDE_VTRASH);

	dialog = em_folder_selector_new (
		window, EM_FOLDER_TREE (folder_tree),
		EM_FOLDER_SELECTOR_CAN_CREATE,
		_("Copy to Folder"), NULL, _("C_opy"));

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
			folder, uids, FALSE, uri, 0, NULL, NULL);
		uids = NULL;
	}

exit:
	if (uids != NULL)
		em_utils_uids_free (uids);

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
	CamelFolder *folder;
	GPtrArray *uids;

	folder = e_mail_reader_get_folder (reader);
	uids = e_mail_reader_get_selected_uids (reader);

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
	CamelFolder *folder;
	GtkWindow *window;
	GPtrArray *uids;

	folder = e_mail_reader_get_folder (reader);
	html_display = e_mail_reader_get_html_display (reader);
	uids = e_mail_reader_get_selected_uids (reader);
	window = e_mail_reader_get_window (reader);

	em_utils_flag_for_followup_clear (window, folder, uids);

	em_format_redraw (EM_FORMAT (html_display));
}

static void
action_mail_flag_completed_cb (GtkAction *action,
                               EMailReader *reader)
{
	EMFormatHTMLDisplay *html_display;
	CamelFolder *folder;
	GtkWindow *window;
	GPtrArray *uids;

	folder = e_mail_reader_get_folder (reader);
	html_display = e_mail_reader_get_html_display (reader);
	uids = e_mail_reader_get_selected_uids (reader);
	window = e_mail_reader_get_window (reader);

	em_utils_flag_for_followup_completed (window, folder, uids);

	em_format_redraw (EM_FORMAT (html_display));
}

static void
action_mail_flag_for_followup_cb (GtkAction *action,
                                  EMailReader *reader)
{
	CamelFolder *folder;
	GPtrArray *uids;

	folder = e_mail_reader_get_folder (reader);
	uids = e_mail_reader_get_selected_uids (reader);

	em_utils_flag_for_followup (reader, folder, uids);
}

static void
action_mail_forward_cb (GtkAction *action,
                        EMailReader *reader)
{
	CamelFolder *folder;
	GtkWindow *window;
	GPtrArray *uids;
	const gchar *folder_uri;

	folder = e_mail_reader_get_folder (reader);
	folder_uri = e_mail_reader_get_folder_uri (reader);
	uids = e_mail_reader_get_selected_uids (reader);
	window = e_mail_reader_get_window (reader);

	g_return_if_fail (uids != NULL);

	if (em_utils_ask_open_many (window, uids->len))
		em_utils_forward_messages (folder, uids, folder_uri);
	else
		em_utils_uids_free (uids);
}

static void
action_mail_forward_attached_cb (GtkAction *action,
                                 EMailReader *reader)
{
	CamelFolder *folder;
	GtkWindow *window;
	GPtrArray *uids;
	const gchar *folder_uri;

	folder = e_mail_reader_get_folder (reader);
	folder_uri = e_mail_reader_get_folder_uri (reader);
	uids = e_mail_reader_get_selected_uids (reader);
	window = e_mail_reader_get_window (reader);

	g_return_if_fail (uids != NULL);

	if (em_utils_ask_open_many (window, uids->len))
		em_utils_forward_attached (folder, uids, folder_uri);
	else
		em_utils_uids_free (uids);
}

static void
action_mail_forward_inline_cb (GtkAction *action,
                               EMailReader *reader)
{
	CamelFolder *folder;
	GtkWindow *window;
	GPtrArray *uids;
	const gchar *folder_uri;

	folder = e_mail_reader_get_folder (reader);
	folder_uri = e_mail_reader_get_folder_uri (reader);
	uids = e_mail_reader_get_selected_uids (reader);
	window = e_mail_reader_get_window (reader);

	g_return_if_fail (uids != NULL);

	if (em_utils_ask_open_many (window, uids->len))
		em_utils_forward_inline (folder, uids, folder_uri);
	else
		em_utils_uids_free (uids);
}

static void
action_mail_forward_quoted_cb (GtkAction *action,
                               EMailReader *reader)
{
	CamelFolder *folder;
	GtkWindow *window;
	GPtrArray *uids;
	const gchar *folder_uri;

	folder = e_mail_reader_get_folder (reader);
	folder_uri = e_mail_reader_get_folder_uri (reader);
	uids = e_mail_reader_get_selected_uids (reader);
	window = e_mail_reader_get_window (reader);

	g_return_if_fail (uids != NULL);

	if (em_utils_ask_open_many (window, uids->len))
		em_utils_forward_quoted (folder, uids, folder_uri);
	else
		em_utils_uids_free (uids);
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
	GtkWidget *message_list;
	guint32 mask = CAMEL_MESSAGE_SEEN | CAMEL_MESSAGE_DELETED;
	guint32 set  = 0;

	message_list = e_mail_reader_get_message_list (reader);

	e_mail_reader_mark_selected (reader, mask, set);

	if (MESSAGE_LIST (message_list)->seen_id != 0) {
		g_source_remove (MESSAGE_LIST (message_list)->seen_id);
		MESSAGE_LIST (message_list)->seen_id = 0;
	}
}

static void
action_mail_message_edit_cb (GtkAction *action,
                             EMailReader *reader)
{
	CamelFolder *folder;
	GPtrArray *uids;

	folder = e_mail_reader_get_folder (reader);
	uids = e_mail_reader_get_selected_uids (reader);

	em_utils_edit_messages (folder, uids, FALSE);
}

static void
action_mail_message_new_cb (GtkAction *action,
                            EMailReader *reader)
{
	const gchar *folder_uri;

	folder_uri = e_mail_reader_get_folder_uri (reader);

	em_utils_compose_new_message (folder_uri);
}

static void
action_mail_message_open_cb (GtkAction *action,
                             EMailReader *reader)
{
	e_mail_reader_open_selected (reader);
}

static void
action_mail_move_cb (GtkAction *action,
                     EMailReader *reader)
{
	CamelFolder *folder;
	GtkWidget *folder_tree;
	GtkWidget *dialog;
	GtkWindow *window;
	GPtrArray *uids;
	const gchar *uri;

	folder = e_mail_reader_get_folder (reader);
	uids = e_mail_reader_get_selected_uids (reader);
	window = e_mail_reader_get_window (reader);

	folder_tree = em_folder_tree_new ();
	emu_restore_folder_tree_state (EM_FOLDER_TREE (folder_tree));

	em_folder_tree_set_excluded (
		EM_FOLDER_TREE (folder_tree),
		EMFT_EXCLUDE_NOSELECT | EMFT_EXCLUDE_VIRTUAL |
		EMFT_EXCLUDE_VTRASH);

	dialog = em_folder_selector_new (
		window, EM_FOLDER_TREE (folder_tree),
		EM_FOLDER_SELECTOR_CAN_CREATE,
		_("Move to Folder"), NULL, _("_Move"));

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
			folder, uids, TRUE, uri, 0, NULL, NULL);
		uids = NULL;
	}

exit:
	if (uids != NULL)
		em_utils_uids_free (uids);

	gtk_widget_destroy (dialog);
}

static void
action_mail_next_cb (GtkAction *action,
                     EMailReader *reader)
{
	GtkWidget *message_list;
	MessageListSelectDirection direction;
	guint32 flags, mask;

	direction = MESSAGE_LIST_SELECT_NEXT;
	flags = 0;
	mask  = 0;

	message_list = e_mail_reader_get_message_list (reader);

	message_list_select (
		MESSAGE_LIST (message_list), direction, flags, mask);
}

static void
action_mail_next_important_cb (GtkAction *action,
                               EMailReader *reader)
{
	GtkWidget *message_list;
	MessageListSelectDirection direction;
	guint32 flags, mask;

	direction = MESSAGE_LIST_SELECT_NEXT | MESSAGE_LIST_SELECT_WRAP;
	flags = CAMEL_MESSAGE_FLAGGED;
	mask  = CAMEL_MESSAGE_FLAGGED;

	message_list = e_mail_reader_get_message_list (reader);

	message_list_select (
		MESSAGE_LIST (message_list), direction, flags, mask);
}

static void
action_mail_next_thread_cb (GtkAction *action,
                            EMailReader *reader)
{
	GtkWidget *message_list;

	message_list = e_mail_reader_get_message_list (reader);

	message_list_select_next_thread (MESSAGE_LIST (message_list));
}

static void
action_mail_next_unread_cb (GtkAction *action,
                            EMailReader *reader)
{
	GtkWidget *message_list;
	MessageListSelectDirection direction;
	guint32 flags, mask;

	direction = MESSAGE_LIST_SELECT_NEXT | MESSAGE_LIST_SELECT_WRAP;
	flags = 0;
	mask  = CAMEL_MESSAGE_SEEN;

	message_list = e_mail_reader_get_message_list (reader);

	message_list_select (
		MESSAGE_LIST (message_list), direction, flags, mask);
}

static void
action_mail_previous_cb (GtkAction *action,
                         EMailReader *reader)
{
	GtkWidget *message_list;
	MessageListSelectDirection direction;
	guint32 flags, mask;

	direction = MESSAGE_LIST_SELECT_PREVIOUS;
	flags = 0;
	mask  = 0;

	message_list = e_mail_reader_get_message_list (reader);

	message_list_select (
		MESSAGE_LIST (message_list), direction, flags, mask);
}

static void
action_mail_previous_important_cb (GtkAction *action,
                                   EMailReader *reader)
{
	GtkWidget *message_list;
	MessageListSelectDirection direction;
	guint32 flags, mask;

	direction = MESSAGE_LIST_SELECT_PREVIOUS | MESSAGE_LIST_SELECT_WRAP;
	flags = CAMEL_MESSAGE_FLAGGED;
	mask  = CAMEL_MESSAGE_FLAGGED;

	message_list = e_mail_reader_get_message_list (reader);

	message_list_select (
		MESSAGE_LIST (message_list), direction, flags, mask);
}

static void
action_mail_previous_unread_cb (GtkAction *action,
                                EMailReader *reader)
{
	GtkWidget *message_list;
	MessageListSelectDirection direction;
	guint32 flags, mask;

	direction = MESSAGE_LIST_SELECT_PREVIOUS | MESSAGE_LIST_SELECT_WRAP;
	flags = 0;
	mask  = CAMEL_MESSAGE_SEEN;

	message_list = e_mail_reader_get_message_list (reader);

	message_list_select (
		MESSAGE_LIST (message_list), direction, flags, mask);
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
	GtkWidget *message_list;
	CamelFolder *folder;
	const gchar *uid;

	folder = e_mail_reader_get_folder (reader);
	message_list = e_mail_reader_get_message_list (reader);

	uid = MESSAGE_LIST (message_list)->cursor_uid;
	g_return_if_fail (uid != NULL);

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
action_mail_reply_sender_cb (GtkAction *action,
                             EMailReader *reader)
{
	e_mail_reader_reply_to_message (reader, REPLY_MODE_SENDER);
}

static void
action_mail_save_as_cb (GtkAction *action,
                        EMailReader *reader)
{
	EShell *shell;
	EShellBackend *shell_backend;
	CamelMessageInfo *info;
	CamelFolder *folder;
	GPtrArray *uids;
	GFile *file;
	const gchar *title;
	gchar *suggestion = NULL;
	gchar *uri;

	folder = e_mail_reader_get_folder (reader);
	shell_backend = e_mail_reader_get_shell_backend (reader);
	uids = e_mail_reader_get_selected_uids (reader);

	g_return_if_fail (uids->len > 0);

	title = ngettext ("Save Message", "Save Messages", uids->len);

	/* Suggest as a filename the subject of the first message. */
	info = camel_folder_get_message_info (folder, uids->pdata[0]);
	if (info != NULL) {
		const gchar *subject = camel_message_info_subject (info);

		if (subject)
			suggestion = g_strconcat (subject, ".mbox", NULL);
		camel_message_info_free (info);
	}

	if (!suggestion) {
		const gchar *basename;

		/* Translators: This is a part of a suggested file name
		 * used when saving a message or multiple messages to an
		 * mbox format, when the first message doesn't have a
		 * Subject. The extension ".mbox" is appended to this
		 * string, thus it will be something like "Message.mbox"
		 * at the end. */
		basename = ngettext ("Message", "Messages", uids->len);
		suggestion = g_strconcat (basename, ".mbox", NULL);
	}

	shell = e_shell_backend_get_shell (shell_backend);
	file = e_shell_run_save_dialog (
		shell, title, suggestion,
		"*.mbox:application/mbox,message/rfc822", NULL, NULL);

	if (file == NULL) {
		em_utils_uids_free (uids);
		return;
	}

	uri = g_file_get_uri (file);

	/* This eats the UID array, so do not free it. */
	mail_save_messages (folder, uids, uri, NULL, NULL);

	g_free (uri);

	g_object_unref (file);
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
	EShellBackend *shell_backend;
	CamelFolder *folder;
	GtkWidget *browser;
	GPtrArray *uids;
	const gchar *folder_uri;

	folder = e_mail_reader_get_folder (reader);
	folder_uri = e_mail_reader_get_folder_uri (reader);
	shell_backend = e_mail_reader_get_shell_backend (reader);
	uids = e_mail_reader_get_selected_uids (reader);

	g_return_if_fail (uids->len > 0);

	browser = e_mail_browser_new (shell_backend);
	reader = E_MAIL_READER (browser);
	html_display = e_mail_reader_get_html_display (reader);
	em_format_set_mode (EM_FORMAT (html_display), EM_FORMAT_SOURCE);
	e_mail_reader_set_folder (reader, folder, folder_uri);
	e_mail_reader_set_message (reader, uids->pdata[0]);
	gtk_widget_show (browser);

	em_utils_uids_free (uids);
}

static void
action_mail_toggle_important_cb (GtkAction *action,
                                 EMailReader *reader)
{
	CamelFolder *folder;
	GPtrArray *uids;
	guint ii;

	folder = e_mail_reader_get_folder (reader);
	uids = e_mail_reader_get_selected_uids (reader);

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

	em_utils_uids_free (uids);
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
	EWebView *web_view;

	html_display = e_mail_reader_get_html_display (reader);
	web_view = E_WEB_VIEW (EM_FORMAT_HTML (html_display)->html);

	e_web_view_zoom_100 (web_view);
}

static void
action_mail_zoom_in_cb (GtkAction *action,
                        EMailReader *reader)
{
	EMFormatHTMLDisplay *html_display;
	EWebView *web_view;

	html_display = e_mail_reader_get_html_display (reader);
	web_view = E_WEB_VIEW (EM_FORMAT_HTML (html_display)->html);

	e_web_view_zoom_in (web_view);
}

static void
action_mail_zoom_out_cb (GtkAction *action,
                         EMailReader *reader)
{
	EMFormatHTMLDisplay *html_display;
	EWebView *web_view;

	html_display = e_mail_reader_get_html_display (reader);
	web_view = E_WEB_VIEW (EM_FORMAT_HTML (html_display)->html);

	e_web_view_zoom_out (web_view);
}

static void
action_search_folder_recipient_cb (GtkAction *action,
                                   EMailReader *reader)
{
	EMFormatHTMLDisplay *html_display;
	EWebView *web_view;
	CamelURL *curl;
	const gchar *folder_uri;
	const gchar *uri;

	/* This action is defined in EMailDisplay. */

	folder_uri = e_mail_reader_get_folder_uri (reader);
	html_display = e_mail_reader_get_html_display (reader);

	web_view = E_WEB_VIEW (EM_FORMAT_HTML (html_display)->html);

	uri = e_web_view_get_selected_uri (web_view);
	g_return_if_fail (uri != NULL);

	curl = camel_url_new (uri, NULL);
	g_return_if_fail (curl != NULL);

	if (curl->path != NULL && *curl->path != '\0') {
		CamelInternetAddress *inet_addr;

		/* Ensure vfolder is running. */
		vfolder_load_storage ();

		inet_addr = camel_internet_address_new ();
		camel_address_decode (CAMEL_ADDRESS (inet_addr), curl->path);
		vfolder_gui_add_from_address (inet_addr, AUTO_TO, folder_uri);
		camel_object_unref (inet_addr);
	}

	camel_url_free (curl);
}

static void
action_search_folder_sender_cb (GtkAction *action,
                                EMailReader *reader)
{
	EMFormatHTMLDisplay *html_display;
	EWebView *web_view;
	CamelURL *curl;
	const gchar *folder_uri;
	const gchar *uri;

	/* This action is defined in EMailDisplay. */

	folder_uri = e_mail_reader_get_folder_uri (reader);
	html_display = e_mail_reader_get_html_display (reader);

	web_view = E_WEB_VIEW (EM_FORMAT_HTML (html_display)->html);

	uri = e_web_view_get_selected_uri (web_view);
	g_return_if_fail (uri != NULL);

	curl = camel_url_new (uri, NULL);
	g_return_if_fail (curl != NULL);

	if (curl->path != NULL && *curl->path != '\0') {
		CamelInternetAddress *inet_addr;

		/* Ensure vfolder is running. */
		vfolder_load_storage ();

		inet_addr = camel_internet_address_new ();
		camel_address_decode (CAMEL_ADDRESS (inet_addr), curl->path);
		vfolder_gui_add_from_address (inet_addr, AUTO_FROM, folder_uri);
		camel_object_unref (inet_addr);
	}

	camel_url_free (curl);
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

	{ "mail-forward-attached",
	  NULL,
	  N_("_Attached"),
	  NULL,
	  N_("Forward the selected message to someone as an attachment"),
	  G_CALLBACK (action_mail_forward_attached_cb) },

	{ "mail-forward-attached-full",
	  NULL,
	  N_("Forward As _Attached"),
	  NULL,
	  N_("Forward the selected message to someone as an attachment"),
	  G_CALLBACK (action_mail_forward_attached_cb) },

	{ "mail-forward-inline",
	  NULL,
	  N_("_Inline"),
	  NULL,
	  N_("Forward the selected message in the body of a new message"),
	  G_CALLBACK (action_mail_forward_inline_cb) },

	{ "mail-forward-inline-full",
	  NULL,
	  N_("Forward As _Inline"),
	  NULL,
	  N_("Forward the selected message in the body of a new message"),
	  G_CALLBACK (action_mail_forward_inline_cb) },

	{ "mail-forward-quoted",
	  NULL,
	  N_("_Quoted"),
	  NULL,
	  N_("Forward the selected message quoted like a reply"),
	  G_CALLBACK (action_mail_forward_quoted_cb) },

	{ "mail-forward-quoted-full",
	  NULL,
	  N_("Forward As _Quoted"),
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
	  N_("F_orward As"),
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

static EPopupActionEntry mail_reader_popup_entries[] = {

	{ "mail-popup-copy",
	  NULL,
	  "mail-copy" },

	{ "mail-popup-delete",
	  NULL,
	  "mail-delete" },

	{ "mail-popup-flag-clear",
	  NULL,
	  "mail-flag-clear" },

	{ "mail-popup-flag-completed",
	  NULL,
	  "mail-flag-completed" },

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
	  N_("Mar_k as Read"),
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
		case GDK_bracketright:
			action_name = "mail-next-unread";
			break;

		case GDK_comma:
		case GDK_bracketleft:
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

		case GDK_exclam:
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
	EMailReaderPrivate *priv;
	GtkWidget *message_list;
	const gchar *cursor_uid;
	const gchar *message_uid;

	priv = E_MAIL_READER_GET_PRIVATE (reader);

	message_uid = priv->mark_read_message_uid;
	g_return_val_if_fail (message_uid != NULL, FALSE);

	message_list = e_mail_reader_get_message_list (reader);
	cursor_uid = MESSAGE_LIST (message_list)->cursor_uid;

	if (g_strcmp0 (cursor_uid, message_uid) == 0)
		e_mail_reader_mark_as_read (reader, message_uid);

	return FALSE;
}

static void
update_webview_content (EMailReader *reader, const gchar *content)
{
	EMFormatHTMLDisplay *html_display;
	EWebView *web_view;

	g_return_if_fail (reader != NULL);
	g_return_if_fail (content != NULL);

	html_display = e_mail_reader_get_html_display (reader);
	g_return_if_fail (html_display != NULL);

	/* skip the progress message when it's formatting something */
	if (em_format_busy (EM_FORMAT (html_display)))
		return;

	web_view = E_WEB_VIEW (EM_FORMAT_HTML (html_display)->html);
	g_return_if_fail (web_view != NULL);

	e_web_view_load_string (web_view, content);
}

static void
mail_reader_message_loaded_cb (CamelFolder *folder,
                               const gchar *message_uid,
                               CamelMimeMessage *message,
                               gpointer user_data,
                               CamelException *ex)
{
	EMailReader *reader = user_data;
	EMailReaderPrivate *priv;
	EMFormatHTMLDisplay *html_display;
	GtkWidget *message_list;
	EShellBackend *shell_backend;
	EShellSettings *shell_settings;
	EShell *shell;
	EMEvent *event;
	EMEventTargetMessage *target;
	const gchar *cursor_uid;
	gboolean schedule_timeout;
	gint timeout_interval;

	priv = E_MAIL_READER_GET_PRIVATE (reader);

	html_display = e_mail_reader_get_html_display (reader);
	message_list = e_mail_reader_get_message_list (reader);

	shell_backend = e_mail_reader_get_shell_backend (reader);
	shell = e_shell_backend_get_shell (shell_backend);
	shell_settings = e_shell_get_shell_settings (shell);

	cursor_uid = MESSAGE_LIST (message_list)->cursor_uid;

	/* If the user picked a different message in the time it took
	 * to fetch this message, then don't bother rendering it. */
	if (g_strcmp0 (cursor_uid, message_uid) != 0)
		goto exit;

	/** @Event: message.reading
	 * @Title: Viewing a message
	 * @Target: EMEventTargetMessage
	 *
	 * message.reading is emitted whenever a user views a message.
	 */
	event = em_event_peek ();
	target = em_event_target_new_message (
		event, folder, message, message_uid, 0, NULL);
	e_event_emit (
		(EEvent *) event, "message.reading",
		(EEventTarget *) target);

	em_format_format (
		EM_FORMAT (html_display), folder, message_uid, message);

	/* Reset the shell view icon. */
	e_shell_event (shell, "mail-icon", (gpointer) "evolution-mail");

	/* Determine whether to mark the message as read. */
	schedule_timeout =
		(message != NULL) &&
		e_shell_settings_get_boolean (
			shell_settings, "mail-mark-seen") &&
		!priv->restoring_message_selection;
	timeout_interval =
		e_shell_settings_get_int (
		shell_settings, "mail-mark-seen-timeout");

	g_free (priv->mark_read_message_uid);
	priv->mark_read_message_uid = NULL;

	if (MESSAGE_LIST (message_list)->seen_id > 0) {
		g_source_remove (MESSAGE_LIST (message_list)->seen_id);
		MESSAGE_LIST (message_list)->seen_id = 0;
	}

	if (schedule_timeout) {
		priv->mark_read_message_uid = g_strdup (message_uid);
		MESSAGE_LIST (message_list)->seen_id = g_timeout_add (
			timeout_interval, (GSourceFunc)
			mail_reader_message_read_cb, reader);

	} else if (camel_exception_is_set (ex)) {
		gchar *string;

		if (ex->id != CAMEL_EXCEPTION_OPERATION_IN_PROGRESS) {
			/* Display the error inline and clear the exception. */
			string = g_strdup_printf (
					"<h2>%s</h2><p>%s</p>",
					_("Unable to retrieve message"),
					ex->desc);
		} else {
			string = g_strdup_printf (_("Retrieving message '%s'"), cursor_uid);
		}

		update_webview_content (reader, string);
		g_free (string);

		camel_exception_clear (ex);
	}

	/* We referenced this in the call to mail_get_messagex(). */
	g_object_unref (reader);

exit:
	priv->restoring_message_selection = FALSE;
}

static gboolean
mail_reader_message_selected_timeout_cb (EMailReader *reader)
{
	EMailReaderPrivate *priv;
	EMFormatHTMLDisplay *html_display;
	GtkWidget *message_list;
	CamelFolder *folder;
	const gchar *cursor_uid;
	const gchar *format_uid;

	priv = E_MAIL_READER_GET_PRIVATE (reader);

	folder = e_mail_reader_get_folder (reader);
	html_display = e_mail_reader_get_html_display (reader);
	message_list = e_mail_reader_get_message_list (reader);

	cursor_uid = MESSAGE_LIST (message_list)->cursor_uid;
	format_uid = EM_FORMAT (html_display)->uid;

	if (MESSAGE_LIST (message_list)->last_sel_single) {
		GtkWidget *widget;
		gboolean html_display_visible;
		gboolean selected_uid_changed;

		/* Decide whether to download the full message now. */

		widget = GTK_WIDGET (EM_FORMAT_HTML (html_display)->html);

#if GTK_CHECK_VERSION(2,19,7)
		html_display_visible = gtk_widget_get_mapped (widget);
#else
		html_display_visible = GTK_WIDGET_MAPPED (widget);
#endif
		selected_uid_changed = g_strcmp0 (cursor_uid, format_uid);

		if (html_display_visible && selected_uid_changed) {
			gint op_id;
			gchar *string;
			gboolean store_async;
			MailMsgDispatchFunc disp_func;

			string = g_strdup_printf (_("Retrieving message '%s'"), cursor_uid);
			update_webview_content (reader, string);
			g_free (string);

			store_async = folder->parent_store->flags & CAMEL_STORE_ASYNC;

			if (store_async)
				disp_func = mail_msg_unordered_push;
			else
				disp_func = mail_msg_fast_ordered_push;

			op_id = mail_get_messagex (
				folder, cursor_uid,
				mail_reader_message_loaded_cb,
				g_object_ref (reader),
				disp_func);

			if (!store_async)
				priv->retrieving_message_operation_id = op_id;
		}
	} else {
		em_format_format (EM_FORMAT (html_display), NULL, NULL, NULL);
		priv->restoring_message_selection = FALSE;
	}

	priv->message_selected_timeout_id = 0;

	return FALSE;
}

static void
mail_reader_message_selected_cb (EMailReader *reader,
                                 const gchar *uid)
{
	EMailReaderPrivate *priv;
	MessageList *message_list;
	gboolean store_async;
	CamelFolder *folder;

	priv = E_MAIL_READER_GET_PRIVATE (reader);

	folder = e_mail_reader_get_folder (reader);
	store_async = folder->parent_store->flags & CAMEL_STORE_ASYNC;

	/* Cancel previous message retrieval if the store is not async. */
	if (!store_async && priv->retrieving_message_operation_id > 0)
		mail_msg_cancel (priv->retrieving_message_operation_id);

	/* Cancel the seen timer. */
	message_list = MESSAGE_LIST (e_mail_reader_get_message_list (reader));
	if (message_list && message_list->seen_id) {
		g_source_remove (message_list->seen_id);
		message_list->seen_id = 0;
	}

	/* Cancel the message selected timer. */
	if (priv->message_selected_timeout_id > 0) {
		g_source_remove (priv->message_selected_timeout_id);
		priv->message_selected_timeout_id = 0;
	}

	/* If a folder was just selected then we are now automatically
	 * restoring the previous message selection.  We behave slightly
	 * differently than if the user had selected the message. */
	priv->restoring_message_selection = priv->folder_was_just_selected;
	priv->folder_was_just_selected = FALSE;

	/* Skip the timeout if we're restoring the previous message
	 * selection.  The timeout is there for when we're scrolling
	 * rapidly through the message list. */
	if (priv->restoring_message_selection)
		mail_reader_message_selected_timeout_cb (reader);
	else
		priv->message_selected_timeout_id = g_timeout_add (
			100, (GSourceFunc)
			mail_reader_message_selected_timeout_cb, reader);

	e_mail_reader_changed (reader);
}

static void
mail_reader_emit_folder_loaded (EMailReader *reader)
{
	g_signal_emit (reader, signals[FOLDER_LOADED], 0);
}

static GPtrArray *
mail_reader_get_selected_uids (EMailReader *reader)
{
	GtkWidget *message_list;

	message_list = e_mail_reader_get_message_list (reader);

	return message_list_get_selected (MESSAGE_LIST (message_list));
}

static CamelFolder *
mail_reader_get_folder (EMailReader *reader)
{
	GtkWidget *message_list;

	message_list = e_mail_reader_get_message_list (reader);

	return MESSAGE_LIST (message_list)->folder;
}

static const gchar *
mail_reader_get_folder_uri (EMailReader *reader)
{
	GtkWidget *message_list;

	message_list = e_mail_reader_get_message_list (reader);

	return MESSAGE_LIST (message_list)->folder_uri;
}

static void
mail_reader_set_folder (EMailReader *reader,
                        CamelFolder *folder,
                        const gchar *folder_uri)
{
	EMailReaderPrivate *priv;
	EMFormatHTMLDisplay *html_display;
	CamelFolder *previous_folder;
	GtkWidget *message_list;
	const gchar *previous_folder_uri;
	gboolean outgoing;

	priv = E_MAIL_READER_GET_PRIVATE (reader);

	html_display = e_mail_reader_get_html_display (reader);
	message_list = e_mail_reader_get_message_list (reader);

	previous_folder = e_mail_reader_get_folder (reader);
	previous_folder_uri = e_mail_reader_get_folder_uri (reader);

	if (previous_folder != NULL)
		mail_sync_folder (previous_folder, NULL, NULL);

	/* Skip the rest if we're already viewing the folder. */
	if (g_strcmp0 (folder_uri, previous_folder_uri) == 0)
		return;

	outgoing = folder != NULL && folder_uri != NULL && (
		em_utils_folder_is_drafts (folder, folder_uri) ||
		em_utils_folder_is_outbox (folder, folder_uri) ||
		em_utils_folder_is_sent (folder, folder_uri));

	em_format_format (EM_FORMAT (html_display), NULL, NULL, NULL);

	priv->folder_was_just_selected = (folder != NULL);

	message_list_set_folder (
		MESSAGE_LIST (message_list), folder, folder_uri, outgoing);

	mail_reader_emit_folder_loaded (reader);
}

static void
mail_reader_set_message (EMailReader *reader,
                         const gchar *uid)
{
	GtkWidget *message_list;

	message_list = e_mail_reader_get_message_list (reader);

	message_list_select_uid (MESSAGE_LIST (message_list), uid);
}

static void
mail_reader_update_actions (EMailReader *reader)
{
	EShell *shell;
	EShellBackend *shell_backend;
	EShellSettings *shell_settings;
	GtkAction *action;
	const gchar *action_name;
	gboolean sensitive;
	guint32 state;

	/* Be descriptive. */
	gboolean any_messages_selected;
	gboolean disable_printing;
	gboolean enable_flag_clear;
	gboolean enable_flag_completed;
	gboolean enable_flag_for_followup;
	gboolean have_an_account;
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
	gboolean single_message_selected;

	state = e_mail_reader_check_state (reader);

	shell_backend = e_mail_reader_get_shell_backend (reader);
	shell = e_shell_backend_get_shell (shell_backend);
	shell_settings = e_shell_get_shell_settings (shell);

	disable_printing = e_shell_settings_get_boolean (
		shell_settings, "disable-printing");

	have_an_account =
		(state & E_MAIL_READER_HAVE_ACCOUNT);
	single_message_selected =
		(state & E_MAIL_READER_SELECTION_SINGLE);
	multiple_messages_selected =
		(state & E_MAIL_READER_SELECTION_MULTIPLE);
	/* FIXME Missing CAN_ADD_SENDER */
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
	selection_is_mailing_list =
		(state & E_MAIL_READER_SELECTION_IS_MAILING_LIST);

	any_messages_selected =
		(single_message_selected || multiple_messages_selected);

	action_name = "mail-add-sender";
	sensitive = single_message_selected;
	action = e_mail_reader_get_action (reader, action_name);
	gtk_action_set_sensitive (action, sensitive);

	action_name = "mail-check-for-junk";
	sensitive = any_messages_selected;
	action = e_mail_reader_get_action (reader, action_name);
	gtk_action_set_sensitive (action, sensitive);

	action_name = "mail-copy";
	sensitive = any_messages_selected;
	action = e_mail_reader_get_action (reader, action_name);
	gtk_action_set_sensitive (action, sensitive);

	action_name = "mail-create-rule-menu";
	sensitive = single_message_selected;
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

	action_name = "mail-find";
	sensitive = single_message_selected;
	action = e_mail_reader_get_action (reader, action_name);
	gtk_action_set_sensitive (action, sensitive);

	action_name = "mail-flag-clear";
	sensitive = enable_flag_clear;
	action = e_mail_reader_get_action (reader, action_name);
	gtk_action_set_sensitive (action, sensitive);

	action_name = "mail-flag-completed";
	sensitive = enable_flag_completed;
	action = e_mail_reader_get_action (reader, action_name);
	gtk_action_set_sensitive (action, sensitive);

	action_name = "mail-flag-for-followup";
	sensitive = enable_flag_for_followup;
	action = e_mail_reader_get_action (reader, action_name);
	gtk_action_set_sensitive (action, sensitive);

	action_name = "mail-forward";
	sensitive = have_an_account && any_messages_selected;
	action = e_mail_reader_get_action (reader, action_name);
	gtk_action_set_sensitive (action, sensitive);

	action_name = "mail-forward-attached";
	sensitive = have_an_account && any_messages_selected;
	action = e_mail_reader_get_action (reader, action_name);
	gtk_action_set_sensitive (action, sensitive);

	action_name = "mail-forward-attached-full";
	sensitive = have_an_account && any_messages_selected;
	action = e_mail_reader_get_action (reader, action_name);
	gtk_action_set_sensitive (action, sensitive);

	action_name = "mail-forward-as-menu";
	sensitive = have_an_account && any_messages_selected;
	action = e_mail_reader_get_action (reader, action_name);
	gtk_action_set_sensitive (action, sensitive);

	action_name = "mail-forward-inline";
	sensitive = have_an_account && single_message_selected;
	action = e_mail_reader_get_action (reader, action_name);
	gtk_action_set_sensitive (action, sensitive);

	action_name = "mail-forward-inline-full";
	sensitive = have_an_account && single_message_selected;
	action = e_mail_reader_get_action (reader, action_name);
	gtk_action_set_sensitive (action, sensitive);

	action_name = "mail-forward-quoted";
	sensitive = have_an_account && single_message_selected;
	action = e_mail_reader_get_action (reader, action_name);
	gtk_action_set_sensitive (action, sensitive);

	action_name = "mail-forward-quoted-full";
	sensitive = have_an_account && single_message_selected;
	action = e_mail_reader_get_action (reader, action_name);
	gtk_action_set_sensitive (action, sensitive);

	action_name = "mail-goto-menu";
	sensitive = any_messages_selected;
	action = e_mail_reader_get_action (reader, action_name);
	gtk_action_set_sensitive (action, sensitive);

	action_name = "mail-load-images";
	sensitive = single_message_selected;
	action = e_mail_reader_get_action (reader, action_name);
	gtk_action_set_sensitive (action, sensitive);

	action_name = "mail-mark-as-menu";
	sensitive = any_messages_selected;
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
	sensitive = have_an_account && single_message_selected;
	action = e_mail_reader_get_action (reader, action_name);
	gtk_action_set_sensitive (action, sensitive);

	action_name = "mail-message-new";
	sensitive = have_an_account;
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

	action_name = "mail-next";
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
	sensitive = any_messages_selected;
	action = e_mail_reader_get_action (reader, action_name);
	gtk_action_set_sensitive (action, sensitive);

	action_name = "mail-previous";
	sensitive = any_messages_selected;
	action = e_mail_reader_get_action (reader, action_name);
	gtk_action_set_sensitive (action, sensitive);

	action_name = "mail-previous-important";
	sensitive = single_message_selected;
	action = e_mail_reader_get_action (reader, action_name);
	gtk_action_set_sensitive (action, sensitive);

	action_name = "mail-previous-unread";
	sensitive = any_messages_selected;
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
	sensitive = have_an_account && single_message_selected;
	action = e_mail_reader_get_action (reader, action_name);
	gtk_action_set_sensitive (action, sensitive);

	action_name = "mail-reply-all";
	sensitive = have_an_account && single_message_selected;
	action = e_mail_reader_get_action (reader, action_name);
	gtk_action_set_sensitive (action, sensitive);

	action_name = "mail-reply-list";
	sensitive = have_an_account && single_message_selected &&
		selection_is_mailing_list;
	action = e_mail_reader_get_action (reader, action_name);
	gtk_action_set_sensitive (action, sensitive);

	action_name = "mail-reply-sender";
	sensitive = have_an_account && single_message_selected;
	action = e_mail_reader_get_action (reader, action_name);
	gtk_action_set_sensitive (action, sensitive);

	action_name = "mail-save-as";
	sensitive = any_messages_selected;
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
	quark_private = g_quark_from_static_string ("EMailReader-private");

	iface->get_selected_uids = mail_reader_get_selected_uids;
	iface->get_folder = mail_reader_get_folder;
	iface->get_folder_uri = mail_reader_get_folder_uri;
	iface->set_folder = mail_reader_set_folder;
	iface->set_message = mail_reader_set_message;
	iface->update_actions = mail_reader_update_actions;

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

	signals[UPDATE_ACTIONS] = g_signal_new (
		"update-actions",
		G_OBJECT_CLASS_TYPE (iface),
		G_SIGNAL_RUN_FIRST | G_SIGNAL_ACTION,
		G_STRUCT_OFFSET (EMailReaderIface, update_actions),
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
	EMenuToolAction *menu_tool_action;
	EWebView *web_view;
	GtkActionGroup *action_group;
	GtkWidget *message_list;
	GConfBridge *bridge;
	GtkAction *action;
	const gchar *action_name;
	const gchar *key;

	g_return_if_fail (E_IS_MAIL_READER (reader));

	action_group = e_mail_reader_get_action_group (reader);
	html_display = e_mail_reader_get_html_display (reader);
	message_list = e_mail_reader_get_message_list (reader);
	shell_backend = e_mail_reader_get_shell_backend (reader);

	shell = e_shell_backend_get_shell (shell_backend);
	shell_settings = e_shell_get_shell_settings (shell);

	web_view = E_WEB_VIEW (EM_FORMAT_HTML (html_display)->html);

	/* The "mail-forward" action is special: it uses a GtkMenuToolButton
	 * for its toolbar item type.  So we have to create it separately. */

	menu_tool_action = e_menu_tool_action_new (
		"mail-forward", _("_Forward"),
		_("Forward the selected message to someone"), NULL);

	gtk_action_set_icon_name (
		GTK_ACTION (menu_tool_action), "mail-forward");

	g_signal_connect (
		menu_tool_action, "activate",
		G_CALLBACK (action_mail_forward_cb), reader);

	gtk_action_group_add_action_with_accel (
		action_group, GTK_ACTION (menu_tool_action), "<Control>f");

	/* Add the other actions the normal way. */

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

	action_name = "add-to-address-book";
	action = e_web_view_get_action (web_view, action_name);
	g_signal_connect (
		action, "activate",
		G_CALLBACK (action_add_to_address_book_cb), reader);

	action_name = "search-folder-recipient";
	action = e_web_view_get_action (web_view, action_name);
	g_signal_connect (
		action, "activate",
		G_CALLBACK (action_search_folder_recipient_cb), reader);

	action_name = "search-folder-sender";
	action = e_web_view_get_action (web_view, action_name);
	g_signal_connect (
		action, "activate",
		G_CALLBACK (action_search_folder_sender_cb), reader);

	/* Bind properties. */

	e_binding_new_full (
		shell_settings, "mail-citation-color",
		html_display, "citation-color",
		e_binding_transform_string_to_color,
		NULL, NULL);

	e_binding_new (
		shell_settings, "mail-image-loading-policy",
		html_display, "image-loading-policy");

	e_binding_new (
		shell_settings, "mail-only-local-photos",
		html_display, "only-local-photos");

	e_binding_new (
		shell_settings, "mail-show-animated-images",
		web_view, "animate");

	e_binding_new (
		shell_settings, "mail-show-sender-photo",
		html_display, "show-sender-photo");

	e_binding_new (
		shell_settings, "mail-show-real-date",
		html_display, "show-real-date");

	action_name = "mail-caret-mode";
	action = e_mail_reader_get_action (reader, action_name);

	e_mutual_binding_new (
		action, "active",
		web_view, "caret-mode");

	/* Connect signals. */

	g_signal_connect_swapped (
		web_view, "key-press-event",
		G_CALLBACK (mail_reader_key_press_event_cb), reader);

	g_signal_connect_swapped (
		message_list, "message-selected",
		G_CALLBACK (mail_reader_message_selected_cb), reader);

	g_signal_connect_swapped (
		message_list, "message-list-built",
		G_CALLBACK (mail_reader_emit_folder_loaded), reader);

	g_signal_connect_swapped (
		message_list, "double-click",
		G_CALLBACK (mail_reader_double_click_cb), reader);

	g_signal_connect_swapped (
		message_list, "key-press",
		G_CALLBACK (mail_reader_key_press_cb), reader);

	g_signal_connect_swapped (
		message_list, "selection-change",
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
	gboolean is_mailing_list;
	guint32 state = 0;
	guint ii;

	g_return_val_if_fail (E_IS_MAIL_READER (reader), 0);

	folder = e_mail_reader_get_folder (reader);
	folder_uri = e_mail_reader_get_folder_uri (reader);
	uids = e_mail_reader_get_selected_uids (reader);

	if (folder != NULL) {
		store = CAMEL_STORE (folder->parent_store);
		store_supports_vjunk = (store->flags & CAMEL_STORE_VJUNK);
	}

	drafts_or_outbox =
		em_utils_folder_is_drafts (folder, folder_uri) ||
		em_utils_folder_is_outbox (folder, folder_uri);

	/* Initialize this flag based on whether there are any
	 * messages selected.  We will update it in the loop. */
	is_mailing_list = (uids->len > 0);

	for (ii = 0; ii < uids->len; ii++) {
		CamelMessageInfo *info;
		const gchar *string;
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
			if (tag == NULL || *tag == '\0')
				can_flag_completed = TRUE;
		} else
			can_flag_for_followup = TRUE;

		string = camel_message_info_mlist (info);
		is_mailing_list &= (string != NULL && *string != '\0');
	}

	if (em_utils_check_user_can_send_mail ())
		state |= E_MAIL_READER_HAVE_ACCOUNT;
	if (uids->len == 1)
		state |= E_MAIL_READER_SELECTION_SINGLE;
	if (uids->len > 1)
		state |= E_MAIL_READER_SELECTION_MULTIPLE;
	if (!drafts_or_outbox && uids->len == 1)
		state |= E_MAIL_READER_SELECTION_CAN_ADD_SENDER;
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
	if (is_mailing_list)
		state |= E_MAIL_READER_SELECTION_IS_MAILING_LIST;

	em_utils_uids_free (uids);

	return state;

}

void
e_mail_reader_update_actions (EMailReader *reader)
{
	g_return_if_fail (E_IS_MAIL_READER (reader));

	g_signal_emit (reader, signals[UPDATE_ACTIONS], 0);
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

GtkWidget *
e_mail_reader_get_message_list (EMailReader *reader)
{
	EMailReaderIface *iface;

	g_return_val_if_fail (E_IS_MAIL_READER (reader), NULL);

	iface = E_MAIL_READER_GET_IFACE (reader);
	g_return_val_if_fail (iface->get_message_list != NULL, NULL);

	return iface->get_message_list (reader);
}

GtkMenu *
e_mail_reader_get_popup_menu (EMailReader *reader)
{
	EMailReaderIface *iface;

	g_return_val_if_fail (E_IS_MAIL_READER (reader), NULL);

	iface = E_MAIL_READER_GET_IFACE (reader);
	g_return_val_if_fail (iface->get_popup_menu != NULL, NULL);

	return iface->get_popup_menu (reader);
}

GPtrArray *
e_mail_reader_get_selected_uids (EMailReader *reader)
{
	EMailReaderIface *iface;

	g_return_val_if_fail (E_IS_MAIL_READER (reader), NULL);

	iface = E_MAIL_READER_GET_IFACE (reader);
	g_return_val_if_fail (iface->get_selected_uids != NULL, NULL);

	return iface->get_selected_uids (reader);
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
                           const gchar *uid)
{
	EMailReaderIface *iface;

	g_return_if_fail (E_IS_MAIL_READER (reader));

	iface = E_MAIL_READER_GET_IFACE (reader);
	g_return_if_fail (iface->set_message != NULL);

	iface->set_message (reader, uid);
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
