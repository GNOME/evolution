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

#include "e-util/e-account-utils.h"
#include "e-util/e-charset.h"
#include "e-util/e-util.h"
#include "e-util/e-alert-dialog.h"
#include "e-util/gconf-bridge.h"
#include "shell/e-shell-utils.h"
#include "widgets/misc/e-popup-action.h"
#include "widgets/misc/e-menu-tool-action.h"

#include "mail/e-mail-backend.h"
#include "mail/e-mail-browser.h"
#include "mail/e-mail-display.h"
#include "mail/e-mail-enumtypes.h"
#include "mail/e-mail-reader-utils.h"
#include "mail/em-composer-utils.h"
#include "mail/em-event.h"
#include "mail/em-folder-selector.h"
#include "mail/em-folder-tree.h"
#include "mail/em-utils.h"
#include "mail/mail-autofilter.h"
#include "mail/mail-ops.h"
#include "mail/mail-mt.h"
#include "mail/mail-vfolder.h"
#include "mail/message-list.h"

#if HAVE_CLUTTER
#include <clutter/clutter.h>
#include <mx/mx.h>
#include <clutter-gtk/clutter-gtk.h>
#endif

/* backward-compatibility cruft */
#include "e-util/gtk-compat.h"

#define E_MAIL_READER_GET_PRIVATE(obj) \
	((EMailReaderPrivate *) g_object_get_qdata \
	(G_OBJECT (obj), quark_private))

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

	guint group_by_threads : 1;
};

enum {
	CHANGED,
	FOLDER_LOADED,
	SHOW_SEARCH_BAR,
	UPDATE_ACTIONS,
	SHOW_FOLDER,
	SHOW_PREVTAB,
	SHOW_NEXTTAB,
	CLOSE_TAB,
	LAST_SIGNAL
};

/* Remembers the previously selected folder when transferring messages. */
static gchar *default_xfer_messages_uri;

static GQuark quark_private;
static GQuark quark_forward_style;
static GQuark quark_reply_style;
static guint signals[LAST_SIGNAL];

G_DEFINE_INTERFACE (EMailReader, e_mail_reader, G_TYPE_OBJECT)

static void
mail_reader_private_free (EMailReaderPrivate *priv)
{
	if (priv->message_selected_timeout_id > 0)
		g_source_remove (priv->message_selected_timeout_id);

	if (priv->retrieving_message_operation_id > 0)
		mail_msg_cancel (priv->retrieving_message_operation_id);

	g_free (priv->mark_read_message_uid);

	g_slice_free (EMailReaderPrivate, priv);
}

static void
action_mail_add_sender_cb (GtkAction *action,
                           EMailReader *reader)
{
	EShell *shell;
	EMailBackend *backend;
	EShellBackend *shell_backend;
	CamelMessageInfo *info = NULL;
	CamelFolder *folder;
	GPtrArray *uids;
	const gchar *address;

	folder = e_mail_reader_get_folder (reader);
	backend = e_mail_reader_get_backend (reader);
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
	shell_backend = E_SHELL_BACKEND (backend);
	shell = e_shell_backend_get_shell (shell_backend);
	e_shell_event (shell, "contact-quick-add-email", (gpointer) address);
	emu_remove_from_mail_cache_1 (address);

exit:
	if (info)
		camel_folder_free_message_info (folder, info);
	em_utils_uids_free (uids);
}

static void
action_add_to_address_book_cb (GtkAction *action,
                               EMailReader *reader)
{
	EShell *shell;
	EMailBackend *backend;
	EShellBackend *shell_backend;
	EMFormatHTML *formatter;
	CamelInternetAddress *cia;
	EWebView *web_view;
	CamelURL *curl;
	const gchar *uri;
	gchar *email;

	/* This action is defined in EMailDisplay. */

	backend = e_mail_reader_get_backend (reader);
	formatter = e_mail_reader_get_formatter (reader);

	web_view = em_format_html_get_web_view (formatter);

	uri = e_web_view_get_selected_uri (web_view);
	g_return_if_fail (uri != NULL);

	curl = camel_url_new (uri, NULL);
	g_return_if_fail (curl != NULL);

	if (curl->path == NULL || *curl->path == '\0')
		goto exit;

	cia = camel_internet_address_new ();
	if (camel_address_decode (CAMEL_ADDRESS (cia), curl->path) < 0) {
		g_object_unref (cia);
		goto exit;
	}

	email = camel_address_format (CAMEL_ADDRESS (cia));

	/* XXX EBookShellBackend should be listening for this
	 *     event.  Kind of kludgey, but works for now. */
	shell_backend = E_SHELL_BACKEND (backend);
	shell = e_shell_backend_get_shell (shell_backend);
	e_shell_event (shell, "contact-quick-add-email", email);
	emu_remove_from_mail_cache_1 (curl->path);

	g_object_unref (cia);
	g_free (email);

exit:
	camel_url_free (curl);
}

static void
action_mail_charset_cb (GtkRadioAction *action,
                        GtkRadioAction *current,
                        EMailReader *reader)
{
	EMFormatHTML *formatter;
	const gchar *charset;

	if (action != current)
		return;

	formatter = e_mail_reader_get_formatter (reader);
	charset = g_object_get_data (G_OBJECT (action), "charset");

	/* Charset for "Default" action will be NULL. */
	em_format_set_charset (EM_FORMAT (formatter), charset);
}

static void
action_mail_check_for_junk_cb (GtkAction *action,
                               EMailReader *reader)
{
	EMailBackend *backend;
	EMailSession *session;
	CamelFolder *folder;
	GPtrArray *uids;

	folder = e_mail_reader_get_folder (reader);
	backend = e_mail_reader_get_backend (reader);
	uids = e_mail_reader_get_selected_uids (reader);

	session = e_mail_backend_get_session (backend);

	mail_filter_folder (
		session, folder, uids,
		E_FILTER_SOURCE_JUNKTEST, FALSE);
}

static void
action_mail_copy_cb (GtkAction *action,
                     EMailReader *reader)
{
	CamelFolder *folder;
	EMailBackend *backend;
	EMailSession *session;
	GtkWidget *folder_tree;
	GtkWidget *dialog;
	GtkWindow *window;
	GPtrArray *uids;
	const gchar *uri;

	backend = e_mail_reader_get_backend (reader);
	session = e_mail_backend_get_session (backend);

	folder = e_mail_reader_get_folder (reader);
	window = e_mail_reader_get_window (reader);
	uids = e_mail_reader_get_selected_uids (reader);

	folder_tree = em_folder_tree_new (session);
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
			session, folder, uids,
			FALSE, uri, 0, NULL, NULL);
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
	EMailBackend *backend;
	EMailSession *session;
	CamelFolder *folder;
	GPtrArray *uids;

	folder = e_mail_reader_get_folder (reader);
	backend = e_mail_reader_get_backend (reader);
	uids = e_mail_reader_get_selected_uids (reader);

	session = e_mail_backend_get_session (backend);

	mail_filter_folder (
		session, folder, uids,
		E_FILTER_SOURCE_DEMAND, FALSE);
}

static void
action_mail_remove_attachments_cb (GtkAction *action, EMailReader *reader)
{
	CamelFolder *folder;
	GPtrArray *uids;
	gint i, j;

	folder = e_mail_reader_get_folder (reader);
	uids = e_mail_reader_get_selected_uids (reader);

	camel_folder_freeze (folder);
	for (i = 0; i < (uids ? uids->len : 0); i++) {
		CamelMimeMessage *message;
		CamelDataWrapper *containee;
		gchar *uid;

		uid = g_ptr_array_index (uids, i);

		/* retrieve the message from the CamelFolder */
		message = camel_folder_get_message_sync (folder, uid, NULL, NULL);
		if (!message) {
			continue;
		}

		containee = camel_medium_get_content (CAMEL_MEDIUM (message));
		if (containee == NULL) {
			continue;
		}

		if (CAMEL_IS_MULTIPART (containee)) {
			gboolean deleted = FALSE;
			gint parts;

			parts = camel_multipart_get_number (CAMEL_MULTIPART (containee));
			for (j = 0; j < parts; j++) {
				CamelMimePart *mpart = camel_multipart_get_part (CAMEL_MULTIPART (containee), j);
				const gchar *disposition = camel_mime_part_get_disposition (mpart);
				if (disposition && (!strcmp (disposition, "attachment") || !strcmp (disposition, "inline"))) {
					gchar *desc;
					const gchar *filename;

					filename = camel_mime_part_get_filename (mpart);
					desc = g_strdup_printf (_("File \"%s\" has been removed."), filename ? filename : "");
					camel_mime_part_set_disposition (mpart, "inline");
					camel_mime_part_set_content (mpart, desc, strlen (desc), "text/plain");
					camel_mime_part_set_content_type (mpart, "text/plain");
					deleted = TRUE;
				}
			}

			if (deleted) {
				/* copy the original message with the deleted attachment */
				CamelMessageInfo *info, *newinfo;
				guint32 flags;
				GError *error = NULL;

				info = camel_folder_get_message_info (folder, uid);
				newinfo = camel_message_info_new_from_header (NULL, CAMEL_MIME_PART (message)->headers);
				flags = camel_folder_get_message_flags (folder, uid);

				/* make a copy of the message */
				camel_message_info_set_flags (newinfo, flags, flags);
				camel_folder_append_message_sync (folder, message, newinfo, NULL, NULL, &error);
				
				if (!error) {
					/* marked the original message deleted */
					camel_message_info_set_flags (info, CAMEL_MESSAGE_DELETED, CAMEL_MESSAGE_DELETED);
				}

				camel_folder_free_message_info (folder, info);
				camel_message_info_free (newinfo);

				if (error)
					g_error_free (error);
			}
		}
	}

	camel_folder_synchronize_sync (folder, FALSE, NULL, NULL);
	camel_folder_thaw (folder);
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
	EMFormatHTML *formatter;
	CamelFolder *folder;
	GtkWindow *window;
	GPtrArray *uids;

	folder = e_mail_reader_get_folder (reader);
	formatter = e_mail_reader_get_formatter (reader);
	uids = e_mail_reader_get_selected_uids (reader);
	window = e_mail_reader_get_window (reader);

	em_utils_flag_for_followup_clear (window, folder, uids);

	em_format_queue_redraw (EM_FORMAT (formatter));
}

static void
action_mail_flag_completed_cb (GtkAction *action,
                               EMailReader *reader)
{
	EMFormatHTML *formatter;
	CamelFolder *folder;
	GtkWindow *window;
	GPtrArray *uids;

	folder = e_mail_reader_get_folder (reader);
	formatter = e_mail_reader_get_formatter (reader);
	uids = e_mail_reader_get_selected_uids (reader);
	window = e_mail_reader_get_window (reader);

	em_utils_flag_for_followup_completed (window, folder, uids);

	em_format_queue_redraw (EM_FORMAT (formatter));
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
check_close_browser_reader (EMailReader *reader)
{
	GConfClient *client;
	gchar *value;
	gboolean close_it = FALSE;

	g_return_if_fail (read != NULL);

	/* only allow closing of a mail browser and nothing else */
	if (!E_IS_MAIL_BROWSER (reader))
		return;

	client = gconf_client_get_default ();

	value = gconf_client_get_string (
		client, "/apps/evolution/mail/prompts/reply_close_browser", NULL);

	if (value && g_str_equal (value, "always")) {
		close_it = TRUE;
	} else if (!value || !g_str_equal (value, "never")) {
		GtkWidget *dialog;
		GtkWindow *parent;
		gint response;
		EShell *shell;
		EMailBackend *backend;
		EShellBackend *shell_backend;

		backend = e_mail_reader_get_backend (reader);

		shell_backend = E_SHELL_BACKEND (backend);
		shell = e_shell_backend_get_shell (shell_backend);

		parent = e_shell_get_active_window (shell);
		if (!parent)
			parent = e_mail_reader_get_window (reader);

		dialog = e_alert_dialog_new_for_args (parent, "mail:ask-reply-close-browser", NULL);
		response = gtk_dialog_run (GTK_DIALOG (dialog));
		gtk_widget_destroy (dialog);

		close_it = response == GTK_RESPONSE_YES || response == GTK_RESPONSE_OK;

		if (response == GTK_RESPONSE_OK || response == GTK_RESPONSE_CANCEL)
			gconf_client_set_string (
				client, "/apps/evolution/mail/prompts/reply_close_browser",
				response == GTK_RESPONSE_OK ? "always" : "never", NULL);
	}

	g_free (value);

	if (close_it)
		gtk_widget_destroy (GTK_WIDGET (reader));

	g_object_unref (client);
}

static void
action_mail_forward_cb (GtkAction *action,
                        EMailReader *reader)
{
	EShell *shell;
	EMailBackend *backend;
	EShellBackend *shell_backend;
	CamelFolder *folder;
	GtkWindow *window;
	GPtrArray *uids;
	const gchar *folder_uri;

	backend = e_mail_reader_get_backend (reader);
	folder = e_mail_reader_get_folder (reader);
	folder_uri = e_mail_reader_get_folder_uri (reader);
	uids = e_mail_reader_get_selected_uids (reader);
	window = e_mail_reader_get_window (reader);

	g_return_if_fail (uids != NULL);

	shell_backend = E_SHELL_BACKEND (backend);
	shell = e_shell_backend_get_shell (shell_backend);

	if (em_utils_ask_open_many (window, uids->len))
		em_utils_forward_messages (
			shell, folder, uids, folder_uri,
			e_mail_reader_get_forward_style (reader));
	else
		em_utils_uids_free (uids);

	check_close_browser_reader (reader);
}

static void
action_mail_forward_attached_cb (GtkAction *action,
                                 EMailReader *reader)
{
	EShell *shell;
	EMailBackend *backend;
	EShellBackend *shell_backend;
	CamelFolder *folder;
	GtkWindow *window;
	GPtrArray *uids;
	const gchar *folder_uri;

	backend = e_mail_reader_get_backend (reader);
	folder = e_mail_reader_get_folder (reader);
	folder_uri = e_mail_reader_get_folder_uri (reader);
	uids = e_mail_reader_get_selected_uids (reader);
	window = e_mail_reader_get_window (reader);

	g_return_if_fail (uids != NULL);

	shell_backend = E_SHELL_BACKEND (backend);
	shell = e_shell_backend_get_shell (shell_backend);

	if (em_utils_ask_open_many (window, uids->len))
		em_utils_forward_messages (
			shell, folder, uids, folder_uri,
			E_MAIL_FORWARD_STYLE_ATTACHED);
	else
		em_utils_uids_free (uids);

	check_close_browser_reader (reader);
}

static void
action_mail_forward_inline_cb (GtkAction *action,
                               EMailReader *reader)
{
	EShell *shell;
	EMailBackend *backend;
	EShellBackend *shell_backend;
	CamelFolder *folder;
	GtkWindow *window;
	GPtrArray *uids;
	const gchar *folder_uri;

	backend = e_mail_reader_get_backend (reader);
	folder = e_mail_reader_get_folder (reader);
	folder_uri = e_mail_reader_get_folder_uri (reader);
	uids = e_mail_reader_get_selected_uids (reader);
	window = e_mail_reader_get_window (reader);

	g_return_if_fail (uids != NULL);

	shell_backend = E_SHELL_BACKEND (backend);
	shell = e_shell_backend_get_shell (shell_backend);

	if (em_utils_ask_open_many (window, uids->len))
		em_utils_forward_messages (
			shell, folder, uids, folder_uri,
			E_MAIL_FORWARD_STYLE_INLINE);
	else
		em_utils_uids_free (uids);

	check_close_browser_reader (reader);
}

static void
action_mail_forward_quoted_cb (GtkAction *action,
                               EMailReader *reader)
{
	EShell *shell;
	EMailBackend *backend;
	EShellBackend *shell_backend;
	CamelFolder *folder;
	GtkWindow *window;
	GPtrArray *uids;
	const gchar *folder_uri;

	backend = e_mail_reader_get_backend (reader);
	folder = e_mail_reader_get_folder (reader);
	folder_uri = e_mail_reader_get_folder_uri (reader);
	uids = e_mail_reader_get_selected_uids (reader);
	window = e_mail_reader_get_window (reader);

	g_return_if_fail (uids != NULL);

	shell_backend = E_SHELL_BACKEND (backend);
	shell = e_shell_backend_get_shell (shell_backend);

	if (em_utils_ask_open_many (window, uids->len))
		em_utils_forward_messages (
			shell, folder, uids, folder_uri,
			E_MAIL_FORWARD_STYLE_QUOTED);
	else
		em_utils_uids_free (uids);

	check_close_browser_reader (reader);
}

static void
action_mail_load_images_cb (GtkAction *action,
                            EMailReader *reader)
{
	EMFormatHTML *formatter;

	formatter = e_mail_reader_get_formatter (reader);

	em_format_html_load_images (formatter);
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
	EMFolderTreeModel *model;
	const gchar *folder_uri;
	guint32 mask = CAMEL_MESSAGE_SEEN | CAMEL_MESSAGE_DELETED;
	guint32 set  = 0;
	guint n_marked;

	message_list = e_mail_reader_get_message_list (reader);

	n_marked = e_mail_reader_mark_selected (reader, mask, set);

	if (MESSAGE_LIST (message_list)->seen_id != 0) {
		g_source_remove (MESSAGE_LIST (message_list)->seen_id);
		MESSAGE_LIST (message_list)->seen_id = 0;
	}

	/* Notify the tree model that the user has marked messages as
	 * unread so it doesn't mistake the event as new mail arriving. */
	model = em_folder_tree_model_get_default ();
	folder_uri = e_mail_reader_get_folder_uri (reader);
	em_folder_tree_model_user_marked_unread (model, folder_uri, n_marked);
}

static void
action_mail_message_edit_cb (GtkAction *action,
                             EMailReader *reader)
{
	EShell *shell;
	EMailBackend *backend;
	EShellBackend *shell_backend;
	CamelFolder *folder;
	const gchar *folder_uri;
	GPtrArray *uids;
	gboolean replace;

	backend = e_mail_reader_get_backend (reader);
	folder = e_mail_reader_get_folder (reader);
	folder_uri = e_mail_reader_get_folder_uri (reader);
	uids = e_mail_reader_get_selected_uids (reader);

	shell_backend = E_SHELL_BACKEND (backend);
	shell = e_shell_backend_get_shell (shell_backend);

	replace = em_utils_folder_is_drafts (folder, folder_uri);
	em_utils_edit_messages (shell, folder, uids, replace);
}

static void
action_mail_message_new_cb (GtkAction *action,
                            EMailReader *reader)
{
	EShell *shell;
	EMailBackend *backend;
	EShellBackend *shell_backend;
	const gchar *folder_uri;

	backend = e_mail_reader_get_backend (reader);
	folder_uri = e_mail_reader_get_folder_uri (reader);

	shell_backend = E_SHELL_BACKEND (backend);
	shell = e_shell_backend_get_shell (shell_backend);

	em_utils_compose_new_message (shell, folder_uri);
}

static void
action_mail_message_open_cb (GtkAction *action,
                             EMailReader *reader)
{
	e_mail_reader_open_selected_mail (reader);
}

static void
action_mail_move_cb (GtkAction *action,
                     EMailReader *reader)
{
	CamelFolder *folder;
	EMailBackend *backend;
	EMailSession *session;
	GtkWidget *folder_tree;
	GtkWidget *dialog;
	GtkWindow *window;
	GPtrArray *uids;
	const gchar *uri;

	backend = e_mail_reader_get_backend (reader);
	folder = e_mail_reader_get_folder (reader);
	uids = e_mail_reader_get_selected_uids (reader);
	window = e_mail_reader_get_window (reader);

	session = e_mail_backend_get_session (backend);

	folder_tree = em_folder_tree_new (session);
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
			session, folder, uids,
			TRUE, uri, 0, NULL, NULL);
		uids = NULL;
	}

exit:
	if (uids != NULL)
		em_utils_uids_free (uids);

	gtk_widget_destroy (dialog);
}

static void
action_mail_folder_cb (GtkAction *action,
                       EMailReader *reader)
{
	g_signal_emit (reader, signals[SHOW_FOLDER], 0);
}

static void
action_mail_nexttab_cb (GtkAction *action,
                     EMailReader *reader)
{
	g_signal_emit (reader, signals[SHOW_NEXTTAB], 0);
}

static void
action_mail_prevtab_cb (GtkAction *action,
                     EMailReader *reader)
{
	g_signal_emit (reader, signals[SHOW_PREVTAB], 0);
}

static void
action_mail_closetab_cb (GtkAction *action,
                     EMailReader *reader)
{
	g_signal_emit (reader, signals[CLOSE_TAB], 0);
}

static void
action_mail_next_cb (GtkAction *action,
                     EMailReader *reader)
{
	GtkWidget *message_list;
	MessageListSelectDirection direction;
	guint32 flags, mask;
#if HAVE_CLUTTER
	ClutterActor *actor;
#endif

	direction = MESSAGE_LIST_SELECT_NEXT;
	flags = 0;
	mask  = 0;

	message_list = e_mail_reader_get_message_list (reader);

#if HAVE_CLUTTER
	actor = g_object_get_data (G_OBJECT (message_list), "preview-actor");
	if (actor != NULL) {
		clutter_actor_set_opacity (actor, 0);
		clutter_actor_animate (
			actor, CLUTTER_EASE_OUT_SINE,
			500, "opacity", 255, NULL);
	}
#endif

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
	EShell *shell;
	EMailBackend *backend;
	EShellBackend *shell_backend;
	GtkWidget *message_list;
	CamelFolder *folder;
	const gchar *uid;

	backend = e_mail_reader_get_backend (reader);
	folder = e_mail_reader_get_folder (reader);
	message_list = e_mail_reader_get_message_list (reader);

	uid = MESSAGE_LIST (message_list)->cursor_uid;
	g_return_if_fail (uid != NULL);

	shell_backend = E_SHELL_BACKEND (backend);
	shell = e_shell_backend_get_shell (shell_backend);

	em_utils_redirect_message_by_uid (shell, folder, uid);
	check_close_browser_reader (reader);
}

static void
action_mail_reply_all_check (CamelFolder *folder,
                             const gchar *uid,
                             CamelMimeMessage *message,
                             gpointer user_data)
{
	EMailReader *reader = user_data;
	CamelInternetAddress *to, *cc;
	gint recip_count = 0;
	EMailReplyType type = E_MAIL_REPLY_TO_ALL;

	if (!message)
		return;

	/* get_message_free () will unref the message, so we need to take an
	   extra ref for e_mail_reader_reply_to_message () to own. */
	g_object_ref (message);
	to = camel_mime_message_get_recipients (message, CAMEL_RECIPIENT_TYPE_TO);
	cc = camel_mime_message_get_recipients (message, CAMEL_RECIPIENT_TYPE_CC);

	recip_count = camel_address_length (CAMEL_ADDRESS (to));
	recip_count += camel_address_length (CAMEL_ADDRESS (cc));

	if (recip_count >= 15) {
		GtkWidget *dialog;
		GtkWidget *content_area, *check;
		gint response;

		dialog = e_alert_dialog_new_for_args (
			e_mail_reader_get_window (reader),
			"mail:ask-reply-many-recips", NULL);

		/* Check buttons */
		check = gtk_check_button_new_with_mnemonic (_("_Do not ask me again."));
		gtk_container_set_border_width (GTK_CONTAINER (check), 12);
		content_area = gtk_dialog_get_content_area (GTK_DIALOG (dialog));
		gtk_box_pack_start (GTK_BOX (content_area), check, TRUE, TRUE, 0);
		gtk_widget_show (check);

		response = gtk_dialog_run (GTK_DIALOG (dialog));

		if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (check))) {
			GConfClient *client;
			const gchar *key;

			client = gconf_client_get_default ();
			key = "/apps/evolution/mail/prompts/reply_many_recips";
			gconf_client_set_bool (client, key, FALSE, NULL);
			g_object_unref (client);
		}

		gtk_widget_destroy (dialog);

		if (response == GTK_RESPONSE_NO)
			type = E_MAIL_REPLY_TO_SENDER;
		else if (response == GTK_RESPONSE_CANCEL)
			return;
	}

	e_mail_reader_reply_to_message (reader, message, type);
	check_close_browser_reader (reader);
}

static void
action_mail_reply_all_cb (GtkAction *action,
                          EMailReader *reader)
{
	GConfClient *client;
	const gchar *key;
	guint32 state;
	gboolean ask;

	state = e_mail_reader_check_state (reader);

	client = gconf_client_get_default ();
	key = "/apps/evolution/mail/prompts/reply_many_recips";
	ask = gconf_client_get_bool (client, key, NULL);
	g_object_unref (client);

	if (ask && !(state & E_MAIL_READER_SELECTION_IS_MAILING_LIST)) {
		CamelMimeMessage *message = NULL;
		EWebView *web_view;
		EMFormatHTML *formatter;

		formatter = e_mail_reader_get_formatter (reader);
		web_view = em_format_html_get_web_view (formatter);
		if (gtk_widget_get_mapped (GTK_WIDGET (web_view)))
			message = CAMEL_MIME_MESSAGE (EM_FORMAT (formatter)->message);

		if (!message) {
			CamelFolder *folder;
			GtkWidget *message_list;
			gchar *uid;

			folder = e_mail_reader_get_folder (reader);
			message_list = e_mail_reader_get_message_list (reader);

			uid = MESSAGE_LIST (message_list)->cursor_uid;
			g_return_if_fail (uid != NULL);

			mail_get_message (
				folder, uid, action_mail_reply_all_check,
				reader, mail_msg_unordered_push);
			return;
		}
		action_mail_reply_all_check (NULL, NULL, message, reader);
		return;
	}

	e_mail_reader_reply_to_message (reader, NULL, E_MAIL_REPLY_TO_ALL);
	check_close_browser_reader (reader);
}

static void
action_mail_reply_group_cb (GtkAction *action,
                            EMailReader *reader)
{
	GConfClient *client;
	gboolean reply_list;
	const gchar *key;
	guint32 state;

	state = e_mail_reader_check_state (reader);

	client = gconf_client_get_default ();
	key = "/apps/evolution/mail/composer/group_reply_to_list";
	reply_list = gconf_client_get_bool (client, key, NULL);
	g_object_unref (client);

	if (reply_list && (state & E_MAIL_READER_SELECTION_IS_MAILING_LIST)) {
		e_mail_reader_reply_to_message (
			reader, NULL, E_MAIL_REPLY_TO_LIST);
		check_close_browser_reader (reader);
	} else
		action_mail_reply_all_cb (action, reader);
}

static void
action_mail_reply_list_cb (GtkAction *action,
                           EMailReader *reader)
{
	e_mail_reader_reply_to_message (reader, NULL, E_MAIL_REPLY_TO_LIST);
	check_close_browser_reader (reader);
}

static void
action_mail_reply_sender_check (CamelFolder *folder,
                                const gchar *uid,
                                CamelMimeMessage *message,
                                gpointer user_data)
{
	EMailReader *reader = user_data;
	GConfClient *client;
	EMailReplyType type = E_MAIL_REPLY_TO_SENDER;
	const gchar *key;
	gboolean ask_ignore_list_reply_to;
	gboolean ask_list_reply_to;
	gboolean munged_list_message;

	if (!message)
		return;

	client = gconf_client_get_default ();

	key = "/apps/evolution/mail/composer/ignore_list_reply_to";
	ask_ignore_list_reply_to = gconf_client_get_bool (client, key, NULL);

	key = "/apps/evolution/mail/prompts/list_reply_to";
	ask_list_reply_to = gconf_client_get_bool (client, key, NULL);

	munged_list_message = em_utils_is_munged_list_message (message);

	/* get_message_free () will unref the message, so we need to take an
	   extra ref for e_mail_reader_reply_to_message () to own. */
	g_object_ref (message);

	/* Don't do the "Are you sure you want to reply in private?" pop-up if
	   it's a Reply-To: munged list message... unless we're ignoring munging */
	if (ask_ignore_list_reply_to || !munged_list_message) {
		GtkWidget *dialog;
		GtkWidget *content_area, *check;
		gint response;

		dialog = e_alert_dialog_new_for_args (
			e_mail_reader_get_window (reader),
			"mail:ask-list-private-reply", NULL);

		/* Check buttons */
		check = gtk_check_button_new_with_mnemonic (
			_("_Do not ask me again."));
		gtk_container_set_border_width ((GtkContainer *)check, 12);
		content_area = gtk_dialog_get_content_area (GTK_DIALOG (dialog));
		gtk_box_pack_start (
			GTK_BOX (content_area), check, TRUE, TRUE, 0);
		gtk_widget_show (check);

		response = gtk_dialog_run (GTK_DIALOG (dialog));

		if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (check)))
			gconf_client_set_bool (
				client, "/apps/evolution/mail/prompts/private_list_reply", FALSE, NULL);

		gtk_widget_destroy (dialog);

		if (response == GTK_RESPONSE_YES)
			type = E_MAIL_REPLY_TO_ALL;
		else if (response == GTK_RESPONSE_OK)
			type = E_MAIL_REPLY_TO_LIST;
		else if (response == GTK_RESPONSE_CANCEL)
			goto exit;

	} else if (ask_list_reply_to) {
		GtkWidget *dialog;
		GtkWidget *content_area;
		GtkWidget *vbox;
		GtkWidget *check_again;
		GtkWidget *check_always_ignore;
		gint response;

		dialog = e_alert_dialog_new_for_args (
			e_mail_reader_get_window (reader),
			"mail:ask-list-honour-reply-to", NULL);

		/*Check buttons*/
		vbox = gtk_vbox_new (FALSE, 0);
		content_area = gtk_dialog_get_content_area (GTK_DIALOG (dialog));
		gtk_container_set_border_width (GTK_CONTAINER (vbox), 12);
		gtk_box_pack_start (GTK_BOX (content_area), vbox, TRUE, TRUE, 0);
		gtk_widget_show (vbox);

		check_again = gtk_check_button_new_with_mnemonic (
			_("_Do not ask me again."));
		gtk_box_pack_start (GTK_BOX (vbox), check_again, TRUE, TRUE, 0);
		gtk_widget_show (check_again);

		check_always_ignore = gtk_check_button_new_with_mnemonic (
			_("_Always ignore Reply-To: for mailing lists."));
		gtk_box_pack_start (
			GTK_BOX (vbox), check_always_ignore, TRUE, TRUE, 0);
		gtk_widget_show (check_always_ignore);

		response = gtk_dialog_run (GTK_DIALOG (dialog));

		if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (check_again)))
			gconf_client_set_bool (
				client, "/apps/evolution/mail/prompts/list_reply_to", FALSE, NULL);

		gconf_client_set_bool (
			client, "/apps/evolution/mail/composer/ignore_list_reply_to",
			gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (check_always_ignore)),
			NULL);

		gtk_widget_destroy (dialog);

		if (response == GTK_RESPONSE_NO)
			type = E_MAIL_REPLY_TO_FROM;
		else if (response == GTK_RESPONSE_OK)
			type = E_MAIL_REPLY_TO_LIST;
		else if (response == GTK_RESPONSE_CANCEL)
			goto exit;
	}

	e_mail_reader_reply_to_message (reader, message, type);
	check_close_browser_reader (reader);

exit:
	g_object_unref (client);
}

static void
action_mail_reply_sender_cb (GtkAction *action,
                             EMailReader *reader)
{
	GConfClient *client;
	gboolean ask_list_reply_to;
	gboolean ask_private_list_reply;
	const gchar *key;
	guint32 state;

	state = e_mail_reader_check_state (reader);

	client = gconf_client_get_default ();
	key = "/apps/evolution/mail/prompts/list_reply_to";
	ask_list_reply_to = gconf_client_get_bool (client, key, NULL);
	key = "/apps/evolution/mail/prompts/private_list_reply";
	ask_private_list_reply = gconf_client_get_bool (client, key, NULL);
	g_object_unref (client);

	if ((ask_private_list_reply || ask_list_reply_to) &&
	    state & E_MAIL_READER_SELECTION_IS_MAILING_LIST) {
		CamelMimeMessage *message = NULL;
		EWebView *web_view;
		EMFormatHTML *formatter;

		formatter = e_mail_reader_get_formatter (reader);
		web_view = em_format_html_get_web_view (formatter);
		if (gtk_widget_get_mapped (GTK_WIDGET (web_view)))
			message = CAMEL_MIME_MESSAGE (EM_FORMAT (formatter)->message);

		if (!message) {
			CamelFolder *folder;
			GtkWidget *message_list;
			gchar *uid;

			folder = e_mail_reader_get_folder (reader);
			message_list = e_mail_reader_get_message_list (reader);

			uid = MESSAGE_LIST (message_list)->cursor_uid;
			g_return_if_fail (uid != NULL);

			mail_get_message (
				folder, uid, action_mail_reply_sender_check,
				reader, mail_msg_unordered_push);
			return;
		}
		action_mail_reply_sender_check (NULL, NULL, message, reader);
		return;
	}
	e_mail_reader_reply_to_message (reader, NULL, E_MAIL_REPLY_TO_SENDER);
	check_close_browser_reader (reader);
}

static void
action_mail_save_as_cb (GtkAction *action,
                        EMailReader *reader)
{
	EShell *shell;
	EMailBackend *backend;
	EShellBackend *shell_backend;
	CamelMessageInfo *info;
	CamelFolder *folder;
	GPtrArray *uids;
	GFile *file;
	const gchar *title;
	gchar *suggestion = NULL;
	gchar *uri;

	folder = e_mail_reader_get_folder (reader);
	backend = e_mail_reader_get_backend (reader);
	uids = e_mail_reader_get_selected_uids (reader);

	g_return_if_fail (uids->len > 0);

	title = ngettext ("Save Message", "Save Messages", uids->len);

	/* Suggest as a filename the subject of the first message. */
	info = camel_folder_get_message_info (folder, uids->pdata[0]);
	if (info != NULL) {
		const gchar *subject = camel_message_info_subject (info);

		if (subject)
			suggestion = g_strconcat (subject, ".mbox", NULL);
		camel_folder_free_message_info (folder, info);
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

	shell_backend = E_SHELL_BACKEND (backend);
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
	EMFormatHTML *formatter;
	EMFormatMode mode;

	formatter = e_mail_reader_get_formatter (reader);

	if (!formatter)
		return;

	if (gtk_toggle_action_get_active (action))
		mode = EM_FORMAT_MODE_ALLHEADERS;
	else
		mode = EM_FORMAT_MODE_NORMAL;

	em_format_set_mode (EM_FORMAT (formatter), mode);
}

static void
action_mail_show_source_cb (GtkAction *action,
                            EMailReader *reader)
{
	EMailBackend *backend;
	EMFormatHTML *formatter;
	CamelFolder *folder;
	GtkWidget *browser;
	GPtrArray *uids;
	const gchar *folder_uri;

	backend = e_mail_reader_get_backend (reader);
	folder = e_mail_reader_get_folder (reader);
	folder_uri = e_mail_reader_get_folder_uri (reader);
	uids = e_mail_reader_get_selected_uids (reader);

	g_return_if_fail (uids->len > 0);

	browser = e_mail_browser_new (backend);
	reader = E_MAIL_READER (browser);
	formatter = e_mail_reader_get_formatter (reader);

	if (formatter != NULL)
		em_format_set_mode (
			EM_FORMAT (formatter), EM_FORMAT_MODE_SOURCE);

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
	EMFormatHTML *formatter;
	EWebView *web_view;

	formatter = e_mail_reader_get_formatter (reader);
	web_view = em_format_html_get_web_view (formatter);

	e_web_view_zoom_100 (web_view);
}

static void
action_mail_zoom_in_cb (GtkAction *action,
                        EMailReader *reader)
{
	EMFormatHTML *formatter;
	EWebView *web_view;

	formatter = e_mail_reader_get_formatter (reader);
	web_view = em_format_html_get_web_view (formatter);

	e_web_view_zoom_in (web_view);
}

static void
action_mail_zoom_out_cb (GtkAction *action,
                         EMailReader *reader)
{
	EMFormatHTML *formatter;
	EWebView *web_view;

	formatter = e_mail_reader_get_formatter (reader);
	web_view = em_format_html_get_web_view (formatter);

	e_web_view_zoom_out (web_view);
}

static void
action_search_folder_recipient_cb (GtkAction *action,
                                   EMailReader *reader)
{
	EMailBackend *backend;
	EMailSession *session;
	EMFormatHTML *formatter;
	EWebView *web_view;
	CamelURL *curl;
	const gchar *folder_uri;
	const gchar *uri;

	/* This action is defined in EMailDisplay. */

	folder_uri = e_mail_reader_get_folder_uri (reader);
	formatter = e_mail_reader_get_formatter (reader);

	web_view = em_format_html_get_web_view (formatter);

	uri = e_web_view_get_selected_uri (web_view);
	g_return_if_fail (uri != NULL);

	curl = camel_url_new (uri, NULL);
	g_return_if_fail (curl != NULL);

	backend = e_mail_reader_get_backend (reader);
	session = e_mail_backend_get_session (backend);

	if (curl->path != NULL && *curl->path != '\0') {
		CamelInternetAddress *inet_addr;

		inet_addr = camel_internet_address_new ();
		camel_address_decode (CAMEL_ADDRESS (inet_addr), curl->path);
		vfolder_gui_add_from_address (
			session, inet_addr, AUTO_TO, folder_uri);
		g_object_unref (inet_addr);
	}

	camel_url_free (curl);
}

static void
action_search_folder_sender_cb (GtkAction *action,
                                EMailReader *reader)
{
	EMailBackend *backend;
	EMailSession *session;
	EMFormatHTML *formatter;
	EWebView *web_view;
	CamelURL *curl;
	const gchar *folder_uri;
	const gchar *uri;

	/* This action is defined in EMailDisplay. */

	folder_uri = e_mail_reader_get_folder_uri (reader);
	formatter = e_mail_reader_get_formatter (reader);

	web_view = em_format_html_get_web_view (formatter);

	uri = e_web_view_get_selected_uri (web_view);
	g_return_if_fail (uri != NULL);

	curl = camel_url_new (uri, NULL);
	g_return_if_fail (curl != NULL);

	backend = e_mail_reader_get_backend (reader);
	session = e_mail_backend_get_session (backend);

	if (curl->path != NULL && *curl->path != '\0') {
		CamelInternetAddress *inet_addr;

		inet_addr = camel_internet_address_new ();
		camel_address_decode (CAMEL_ADDRESS (inet_addr), curl->path);
		vfolder_gui_add_from_address (
			session, inet_addr, AUTO_FROM, folder_uri);
		g_object_unref (inet_addr);
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

	{ "mail-goto-folder",
	  NULL,
	  N_("_Switch to Folder"),
	  "<Control>Up",
	  N_("Display the parent folder"),
	  G_CALLBACK (action_mail_folder_cb) },

	{ "mail-goto-nexttab",
	  NULL,
	  N_("Switch to _next tab"),
	  "<Shift><Control>Down",
	  N_("Switch to the next tab"),
	  G_CALLBACK (action_mail_nexttab_cb) },

	{ "mail-goto-prevtab",
	  NULL,
	  N_("Switch to _previous tab"),
	  "<Shift><Control>Up",
	  N_("Switch to the previous tab"),
	  G_CALLBACK (action_mail_prevtab_cb) },

	{ "mail-close-tab",
	  NULL,
	  N_("Cl_ose current tab"),
	  "<Shift><Control>w",
	  N_("Close current tab"),
	  G_CALLBACK (action_mail_closetab_cb) },

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

	{ "mail-remove-attachments",
	  GTK_STOCK_DELETE,
	  N_("Remo_ve attachments"),
	  NULL,
	  N_("Remove attachments"),
	  G_CALLBACK (action_mail_remove_attachments_cb) },

	{ "mail-reply-all",
	  NULL,
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
	  "<Control>s",
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

	{ "mail-reply-group-menu",
	  NULL,
	  N_("_Group Reply"),
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
		case GDK_KEY_Delete:
		case GDK_KEY_KP_Delete:
			action_name = "mail-delete";
			break;

		case GDK_KEY_Return:
		case GDK_KEY_KP_Enter:
		case GDK_KEY_ISO_Enter:
			action_name = "mail-message-open";
			break;

		case GDK_KEY_period:
		case GDK_KEY_bracketright:
			action_name = "mail-next-unread";
			break;

		case GDK_KEY_comma:
		case GDK_KEY_bracketleft:
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

		case GDK_KEY_exclam:
			action_name = "mail-toggle-important";
			break;

		default:
			return FALSE;
	}

	goto exit;

ctrl:

	/* Ctrl + <keyval> */
	switch (event->keyval) {
		case GDK_KEY_period:
			action_name = "mail-next-unread";
			break;

		case GDK_KEY_comma:
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
mail_reader_message_loaded_cb (CamelFolder *folder,
                               const gchar *message_uid,
                               CamelMimeMessage *message,
                               gpointer user_data,
                               GError **error)
{
	EMailReader *reader = user_data;
	EMailReaderPrivate *priv;
	EMFormatHTML *formatter;
	GtkWidget *message_list;
	EMailBackend *backend;
	EShellBackend *shell_backend;
	EShellSettings *shell_settings;
	EShell *shell;
	EWebView *web_view;
	EMEvent *event;
	EMEventTargetMessage *target;
	const gchar *cursor_uid;
	gboolean schedule_timeout;
	gint timeout_interval;

	priv = E_MAIL_READER_GET_PRIVATE (reader);

	/* If the private struct is NULL, the EMailReader was destroyed
	 * while we were loading the message and we're likely holding the
	 * last reference.  Nothing to do but drop the reference. */
	if (priv == NULL) {
		g_object_unref (reader);
		return;
	}

	backend = e_mail_reader_get_backend (reader);
	formatter = e_mail_reader_get_formatter (reader);
	message_list = e_mail_reader_get_message_list (reader);

	shell_backend = E_SHELL_BACKEND (backend);
	shell = e_shell_backend_get_shell (shell_backend);
	shell_settings = e_shell_get_shell_settings (shell);

	cursor_uid = MESSAGE_LIST (message_list)->cursor_uid;

	web_view = em_format_html_get_web_view (formatter);

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

	/* FIXME Need to pass a GCancellable. */
	em_format_format (
		EM_FORMAT (formatter), folder,
		message_uid, message, NULL);

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

	} else if (error != NULL && *error != NULL) {
		gchar *string;

		/* Display the error inline and clear the exception. */
		string = g_strdup_printf (
			"<h2>%s</h2><p>%s</p>",
			_("Unable to retrieve message"),
			(*error)->message);

		e_web_view_load_string (web_view, string);
		g_free (string);

		g_clear_error (error);
	}

	/* We referenced this in the call to mail_get_messagex (). */
	g_object_unref (reader);

exit:
	priv->restoring_message_selection = FALSE;
}

static gboolean
mail_reader_message_selected_timeout_cb (EMailReader *reader)
{
	EMailReaderPrivate *priv;
	EMFormatHTML *formatter;
	GtkWidget *message_list;
	EWebView *web_view;
	CamelFolder *folder;
	CamelStore *parent_store;
	const gchar *cursor_uid;
	const gchar *format_uid;

	priv = E_MAIL_READER_GET_PRIVATE (reader);

	folder = e_mail_reader_get_folder (reader);
	parent_store = camel_folder_get_parent_store (folder);

	formatter = e_mail_reader_get_formatter (reader);
	message_list = e_mail_reader_get_message_list (reader);

	cursor_uid = MESSAGE_LIST (message_list)->cursor_uid;
	format_uid = EM_FORMAT (formatter)->uid;

	web_view = em_format_html_get_web_view (formatter);

	if (MESSAGE_LIST (message_list)->last_sel_single) {
		GtkWidget *widget;
		gboolean web_view_visible;
		gboolean selected_uid_changed;

		/* Decide whether to download the full message now. */

		widget = GTK_WIDGET (web_view);

		web_view_visible = gtk_widget_get_mapped (widget);
		selected_uid_changed = g_strcmp0 (cursor_uid, format_uid);

		if (web_view_visible && selected_uid_changed) {
			gint op_id;
			gchar *string;
			gboolean store_async;
			MailMsgDispatchFunc disp_func;

			string = g_strdup_printf (
				_("Retrieving message '%s'"), cursor_uid);
#if HAVE_CLUTTER
			if (!e_shell_get_express_mode (e_shell_get_default ()))
				e_web_view_load_string (web_view, string);
#else
			e_web_view_load_string (web_view, string);
#endif
			g_free (string);

			store_async = parent_store->flags & CAMEL_STORE_ASYNC;

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
		/* FIXME Need to pass a GCancellable. */
		em_format_format (
			EM_FORMAT (formatter), NULL, NULL, NULL, NULL);
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
	CamelStore *parent_store;

	priv = E_MAIL_READER_GET_PRIVATE (reader);

	folder = e_mail_reader_get_folder (reader);
	parent_store = camel_folder_get_parent_store (folder);
	store_async = parent_store->flags & CAMEL_STORE_ASYNC;

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

static gboolean
mail_reader_get_enable_show_folder (EMailReader *reader)
{
	return FALSE;
}

static void
mail_reader_set_folder (EMailReader *reader,
                        CamelFolder *folder,
                        const gchar *folder_uri)
{
	EMailReaderPrivate *priv;
	EMFormatHTML *formatter;
	CamelFolder *previous_folder;
	GtkWidget *message_list;
	const gchar *previous_folder_uri;
	gboolean outgoing;

	priv = E_MAIL_READER_GET_PRIVATE (reader);

	formatter = e_mail_reader_get_formatter (reader);
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

	/* FIXME Need to pass a GCancellable. */
	em_format_format (EM_FORMAT (formatter), NULL, NULL, NULL, NULL);

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

	message_list_select_uid (MESSAGE_LIST (message_list), uid, FALSE);
}

static void
mail_reader_update_actions (EMailReader *reader,
                            guint32 state)
{
	EShell *shell;
	EMailBackend *backend;
	EShellBackend *shell_backend;
	EShellSettings *shell_settings;
	GtkAction *action;
	const gchar *action_name;
	gboolean sensitive;
	EMailReaderPrivate *priv;

	/* Be descriptive. */
	gboolean any_messages_selected;
	gboolean disable_printing;
	gboolean enable_flag_clear;
	gboolean enable_flag_completed;
	gboolean enable_flag_for_followup;
	gboolean have_enabled_account;
	gboolean multiple_messages_selected;
	gboolean selection_has_attachment_messages;
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

	priv = E_MAIL_READER_GET_PRIVATE (reader);

	backend = e_mail_reader_get_backend (reader);

	shell_backend = E_SHELL_BACKEND (backend);
	shell = e_shell_backend_get_shell (shell_backend);
	shell_settings = e_shell_get_shell_settings (shell);

#ifndef G_OS_WIN32
	disable_printing = e_shell_settings_get_boolean (
		shell_settings, "disable-printing");
#else
	disable_printing = FALSE;
#endif

	have_enabled_account =
		(state & E_MAIL_READER_HAVE_ENABLED_ACCOUNT);
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
	selection_has_attachment_messages =
		(state & E_MAIL_READER_SELECTION_HAS_ATTACHMENTS);
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

	/* If a single message is selected, let the user hit delete to
	 * advance the cursor even if the message is already deleted. */
	action_name = "mail-delete";
	sensitive =
		single_message_selected ||
		selection_has_undeleted_messages;
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
	sensitive = have_enabled_account && any_messages_selected;
	action = e_mail_reader_get_action (reader, action_name);
	gtk_action_set_sensitive (action, sensitive);

	action_name = "mail-forward-attached";
	sensitive = have_enabled_account && any_messages_selected;
	action = e_mail_reader_get_action (reader, action_name);
	gtk_action_set_sensitive (action, sensitive);

	action_name = "mail-forward-attached-full";
	sensitive = have_enabled_account && any_messages_selected;
	action = e_mail_reader_get_action (reader, action_name);
	gtk_action_set_sensitive (action, sensitive);

	action_name = "mail-forward-as-menu";
	sensitive = have_enabled_account && any_messages_selected;
	action = e_mail_reader_get_action (reader, action_name);
	gtk_action_set_sensitive (action, sensitive);

	action_name = "mail-forward-inline";
	sensitive = have_enabled_account && single_message_selected;
	action = e_mail_reader_get_action (reader, action_name);
	gtk_action_set_sensitive (action, sensitive);

	action_name = "mail-forward-inline-full";
	sensitive = have_enabled_account && single_message_selected;
	action = e_mail_reader_get_action (reader, action_name);
	gtk_action_set_sensitive (action, sensitive);

	action_name = "mail-forward-quoted";
	sensitive = have_enabled_account && single_message_selected;
	action = e_mail_reader_get_action (reader, action_name);
	gtk_action_set_sensitive (action, sensitive);

	action_name = "mail-forward-quoted-full";
	sensitive = have_enabled_account && single_message_selected;
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
	sensitive =
		selection_has_not_junk_messages &&
		!(state & E_MAIL_READER_FOLDER_IS_JUNK);
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
	sensitive = have_enabled_account && single_message_selected;
	action = e_mail_reader_get_action (reader, action_name);
	gtk_action_set_sensitive (action, sensitive);

	action_name = "mail-message-new";
	sensitive = have_enabled_account;
	action = e_mail_reader_get_action (reader, action_name);
	gtk_action_set_sensitive (action, sensitive);

	action_name = "mail-message-open";
	sensitive = any_messages_selected;
	action = e_mail_reader_get_action (reader, action_name);
	gtk_action_set_sensitive (action, sensitive);

	action_name = "mail-goto-folder";
	action = e_mail_reader_get_action (reader, action_name);
	g_object_set (action, "visible", e_mail_reader_get_enable_show_folder (reader), NULL);
	gtk_action_set_sensitive (action, e_mail_reader_get_enable_show_folder (reader));

	action_name = "mail-goto-nexttab";
	action = e_mail_reader_get_action (reader, action_name);
	g_object_set (action, "visible", FALSE, NULL);
	gtk_action_set_sensitive (action, TRUE);

	action_name = "mail-goto-prevtab";
	action = e_mail_reader_get_action (reader, action_name);
	g_object_set (action, "visible", FALSE, NULL);
	gtk_action_set_sensitive (action, TRUE);

	action_name = "mail-close-tab";
	action = e_mail_reader_get_action (reader, action_name);
	g_object_set (action, "visible", FALSE, NULL);
	gtk_action_set_sensitive (action, TRUE);

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
	sensitive = have_enabled_account && single_message_selected;
	action = e_mail_reader_get_action (reader, action_name);
	gtk_action_set_sensitive (action, sensitive);

	action_name = "mail-remove-attachments";
	sensitive = any_messages_selected && selection_has_attachment_messages;
	action = e_mail_reader_get_action (reader, action_name);
	gtk_action_set_sensitive (action, sensitive);

	action_name = "mail-reply-all";
	sensitive = have_enabled_account && single_message_selected;
	action = e_mail_reader_get_action (reader, action_name);
	gtk_action_set_sensitive (action, sensitive);

	action_name = "mail-reply-group";
	sensitive = have_enabled_account && single_message_selected;
	action = e_mail_reader_get_action (reader, action_name);
	gtk_action_set_sensitive (action, sensitive);

	action_name = "mail-reply-group-menu";
	sensitive = have_enabled_account && any_messages_selected;
	action = e_mail_reader_get_action (reader, action_name);
	gtk_action_set_sensitive (action, sensitive);

	action_name = "mail-reply-list";
	sensitive = have_enabled_account && single_message_selected &&
		selection_is_mailing_list;
	action = e_mail_reader_get_action (reader, action_name);
	gtk_action_set_sensitive (action, sensitive);

	action_name = "mail-reply-sender";
	sensitive = have_enabled_account && single_message_selected;
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
e_mail_reader_default_init (EMailReaderInterface *interface)
{
	quark_private = g_quark_from_static_string ("e-mail-reader-private");

	/* Forward and reply styles are stored outside of the private
	 * structure as a workaround for EMailShellContent, which loads
	 * extensions long before the private structure is initialized,
	 * and one of those extensions binds our "forward-style" and
	 * "reply-style" properties to EShellSettings properties. */
	quark_forward_style =
		g_quark_from_static_string ("e-mail-reader-forward-style");
	quark_reply_style =
		g_quark_from_static_string ("e-mail-reader-reply-style");

	interface->get_selected_uids = mail_reader_get_selected_uids;
	interface->get_folder = mail_reader_get_folder;
	interface->get_folder_uri = mail_reader_get_folder_uri;
	interface->enable_show_folder = mail_reader_get_enable_show_folder;
	interface->set_folder = mail_reader_set_folder;
	interface->set_message = mail_reader_set_message;
	interface->open_selected_mail = e_mail_reader_open_selected;
	interface->update_actions = mail_reader_update_actions;

	g_object_interface_install_property (
		interface,
		g_param_spec_enum (
			"forward-style",
			"Forward Style",
			"How to forward messages",
			E_TYPE_MAIL_FORWARD_STYLE,
			E_MAIL_FORWARD_STYLE_ATTACHED,
			G_PARAM_READWRITE));

	g_object_interface_install_property (
		interface,
		g_param_spec_boolean (
			"group-by-threads",
			"Group by Threads",
			"Whether to group messages by threads",
			FALSE,
			G_PARAM_READWRITE));

	g_object_interface_install_property (
		interface,
		g_param_spec_enum (
			"reply-style",
			"Reply Style",
			"How to reply to messages",
			E_TYPE_MAIL_REPLY_STYLE,
			E_MAIL_REPLY_STYLE_QUOTED,
			G_PARAM_READWRITE));

	signals[CHANGED] = g_signal_new (
		"changed",
		G_OBJECT_CLASS_TYPE (interface),
		G_SIGNAL_RUN_FIRST,
		0, NULL, NULL,
		g_cclosure_marshal_VOID__VOID,
		G_TYPE_NONE, 0);

	signals[FOLDER_LOADED] = g_signal_new (
		"folder-loaded",
		G_OBJECT_CLASS_TYPE (interface),
		G_SIGNAL_RUN_FIRST,
		0, NULL, NULL,
		g_cclosure_marshal_VOID__VOID,
		G_TYPE_NONE, 0);

	signals[SHOW_SEARCH_BAR] = g_signal_new (
		"show-search-bar",
		G_OBJECT_CLASS_TYPE (interface),
		G_SIGNAL_RUN_FIRST | G_SIGNAL_ACTION,
		G_STRUCT_OFFSET (EMailReaderInterface, show_search_bar),
		NULL, NULL,
		g_cclosure_marshal_VOID__VOID,
		G_TYPE_NONE, 0);

	signals[SHOW_FOLDER] = g_signal_new (
		"show-folder",
		G_OBJECT_CLASS_TYPE (interface),
		G_SIGNAL_RUN_FIRST | G_SIGNAL_ACTION,
		0,
		NULL, NULL,
		g_cclosure_marshal_VOID__VOID,
		G_TYPE_NONE, 0);

	signals[SHOW_NEXTTAB] = g_signal_new (
		"show-next-tab",
		G_OBJECT_CLASS_TYPE (interface),
		G_SIGNAL_RUN_FIRST | G_SIGNAL_ACTION,
		0,
		NULL, NULL,
		g_cclosure_marshal_VOID__VOID,
		G_TYPE_NONE, 0);

	signals[SHOW_PREVTAB] = g_signal_new (
		"show-previous-tab",
		G_OBJECT_CLASS_TYPE (interface),
		G_SIGNAL_RUN_FIRST | G_SIGNAL_ACTION,
		0,
		NULL, NULL,
		g_cclosure_marshal_VOID__VOID,
		G_TYPE_NONE, 0);

	signals[CLOSE_TAB] = g_signal_new (
		"close-tab",
		G_OBJECT_CLASS_TYPE (interface),
		G_SIGNAL_RUN_FIRST | G_SIGNAL_ACTION,
		0,
		NULL, NULL,
		g_cclosure_marshal_VOID__VOID,
		G_TYPE_NONE, 0);

	signals[UPDATE_ACTIONS] = g_signal_new (
		"update-actions",
		G_OBJECT_CLASS_TYPE (interface),
		G_SIGNAL_RUN_FIRST | G_SIGNAL_ACTION,
		G_STRUCT_OFFSET (EMailReaderInterface, update_actions),
		NULL, NULL,
		g_cclosure_marshal_VOID__UINT,
		G_TYPE_NONE, 1,
		G_TYPE_UINT);
}

void
e_mail_reader_init (EMailReader *reader,
                    gboolean init_actions,
                    gboolean connect_signals)
{
	EShell *shell;
	EMailBackend *backend;
	EShellBackend *shell_backend;
	EShellSettings *shell_settings;
	EMFormatHTML *formatter;
	EMenuToolAction *menu_tool_action;
	EWebView *web_view;
	GtkActionGroup *action_group;
	GtkWidget *message_list;
	GConfBridge *bridge;
	GtkAction *action;
	gboolean sensitive;
	const gchar *action_name;
	const gchar *key;

	g_return_if_fail (E_IS_MAIL_READER (reader));

	backend = e_mail_reader_get_backend (reader);
	formatter = e_mail_reader_get_formatter (reader);
	message_list = e_mail_reader_get_message_list (reader);

	shell_backend = E_SHELL_BACKEND (backend);
	shell = e_shell_backend_get_shell (shell_backend);
	shell_settings = e_shell_get_shell_settings (shell);

	web_view = em_format_html_get_web_view (formatter);

	if (!init_actions)
		goto connect_signals;

	action_group = e_mail_reader_get_action_group (reader);

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

	/* Likewise the "mail-reply-group" action */

        /* For Translators: "Group Reply" will reply either to a mailing list
	   (if possible and if that configuration option is enabled), or else
	   it will reply to all. The word "Group" was chosen because it covers
	   either of those, without too strongly implying one or the other. */
	menu_tool_action = e_menu_tool_action_new (
		"mail-reply-group", _("Group Reply"),
		_("Reply to the mailing list, or to all recipients"), NULL);

	gtk_action_set_icon_name (
		GTK_ACTION (menu_tool_action), "mail-reply-all");

	g_signal_connect (
		menu_tool_action, "activate",
		G_CALLBACK (action_mail_reply_group_cb), reader);

	gtk_action_group_add_action_with_accel (
		action_group, GTK_ACTION (menu_tool_action), "<Control>g");

	/* Add the other actions the normal way. */

	gtk_action_group_set_translation_domain (action_group, GETTEXT_PACKAGE);
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

	action_name = "mail-forward";
	action = e_mail_reader_get_action (reader, action_name);
	g_object_set (action, "is-important", TRUE, NULL);

	action_name = "mail-reply-group";
	action = e_mail_reader_get_action (reader, action_name);
	g_object_set (action, "is-important", TRUE, NULL);

	action_name = "mail-goto-folder";
	action = e_mail_reader_get_action (reader, action_name);
	sensitive = e_mail_reader_get_enable_show_folder (reader);
	g_object_set (action, "visible", FALSE, NULL);
	gtk_action_set_sensitive (action, sensitive);

	action_name = "mail-goto-nexttab";
	action = e_mail_reader_get_action (reader, action_name);
	g_object_set (action, "visible", FALSE, NULL);
	gtk_action_set_sensitive (action, TRUE);

	action_name = "mail-goto-prevtab";
	action = e_mail_reader_get_action (reader, action_name);
	g_object_set (action, "visible", FALSE, NULL);
	gtk_action_set_sensitive (action, TRUE);

	action_name = "mail-close-tab";
	action = e_mail_reader_get_action (reader, action_name);
	g_object_set (action, "visible", FALSE, NULL);
	gtk_action_set_sensitive (action, TRUE);

	action_name = "mail-next";
	action = e_mail_reader_get_action (reader, action_name);
	g_object_set (action, "short-label", _("Next"), NULL);

	action_name = "mail-previous";
	action = e_mail_reader_get_action (reader, action_name);
	g_object_set (action, "short-label", _("Previous"), NULL);

	action_name = "mail-reply-all";
	action = e_mail_reader_get_action (reader, action_name);
	g_object_set (action, "is-important", TRUE, NULL);

	action_name = "mail-reply-sender";
	action = e_mail_reader_get_action (reader, action_name);
	g_object_set (
		action, "is-important", TRUE,
		"short-label", _("Reply"), NULL);

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

	action_name = "mail-caret-mode";
	action = e_mail_reader_get_action (reader, action_name);

	g_object_bind_property (
		action, "active",
		web_view, "caret-mode",
		G_BINDING_BIDIRECTIONAL |
		G_BINDING_SYNC_CREATE);

connect_signals:

	if (!connect_signals)
		goto init_private;

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

init_private:

	/* Initialize a private struct. */

	g_object_set_qdata_full (
		G_OBJECT (reader), quark_private,
		g_slice_new0 (EMailReaderPrivate),
		(GDestroyNotify) mail_reader_private_free);
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
	gboolean has_attachments = FALSE;
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
	gboolean is_junk_folder = FALSE;
	guint32 state = 0;
	guint ii;

	g_return_val_if_fail (E_IS_MAIL_READER (reader), 0);

	folder = e_mail_reader_get_folder (reader);
	folder_uri = e_mail_reader_get_folder_uri (reader);
	uids = e_mail_reader_get_selected_uids (reader);

	if (folder != NULL) {
		store = camel_folder_get_parent_store (folder);
		store_supports_vjunk = (store->flags & CAMEL_STORE_VJUNK);
		is_junk_folder = (folder->folder_flags & CAMEL_FOLDER_IS_JUNK) != 0;
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

		if (flags & CAMEL_MESSAGE_ATTACHMENTS)
			has_attachments = TRUE;

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

		camel_folder_free_message_info (folder, info);
	}

	if (e_get_any_enabled_account () != NULL)
		state |= E_MAIL_READER_HAVE_ENABLED_ACCOUNT;
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
	if (has_attachments)
		state |= E_MAIL_READER_SELECTION_HAS_ATTACHMENTS;
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
	if (is_junk_folder)
		state |= E_MAIL_READER_FOLDER_IS_JUNK;

	em_utils_uids_free (uids);

	return state;

}

void
e_mail_reader_update_actions (EMailReader *reader,
                              guint32 state)
{
	g_return_if_fail (E_IS_MAIL_READER (reader));

	g_signal_emit (reader, signals[UPDATE_ACTIONS], 0, state);
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
			"%s: action '%s' not found", G_STRFUNC, action_name);

	return action;
}

GtkActionGroup *
e_mail_reader_get_action_group (EMailReader *reader)
{
	EMailReaderInterface *interface;

	g_return_val_if_fail (E_IS_MAIL_READER (reader), NULL);

	interface = E_MAIL_READER_GET_INTERFACE (reader);
	g_return_val_if_fail (interface->get_action_group != NULL, NULL);

	return interface->get_action_group (reader);
}

EMailBackend *
e_mail_reader_get_backend (EMailReader *reader)
{
	EMailReaderInterface *interface;

	g_return_val_if_fail (E_IS_MAIL_READER (reader), NULL);

	interface = E_MAIL_READER_GET_INTERFACE (reader);
	g_return_val_if_fail (interface->get_backend != NULL, NULL);

	return interface->get_backend (reader);
}

EMFormatHTML *
e_mail_reader_get_formatter (EMailReader *reader)
{
	EMailReaderInterface *interface;

	g_return_val_if_fail (E_IS_MAIL_READER (reader), NULL);

	interface = E_MAIL_READER_GET_INTERFACE (reader);
	g_return_val_if_fail (interface->get_formatter != NULL, NULL);

	return interface->get_formatter (reader);
}

gboolean
e_mail_reader_get_hide_deleted (EMailReader *reader)
{
	EMailReaderInterface *interface;

	g_return_val_if_fail (E_IS_MAIL_READER (reader), FALSE);

	interface = E_MAIL_READER_GET_INTERFACE (reader);
	g_return_val_if_fail (interface->get_hide_deleted != NULL, FALSE);

	return interface->get_hide_deleted (reader);
}

GtkWidget *
e_mail_reader_get_message_list (EMailReader *reader)
{
	EMailReaderInterface *interface;

	g_return_val_if_fail (E_IS_MAIL_READER (reader), NULL);

	interface = E_MAIL_READER_GET_INTERFACE (reader);
	g_return_val_if_fail (interface->get_message_list != NULL, NULL);

	return interface->get_message_list (reader);
}

GtkMenu *
e_mail_reader_get_popup_menu (EMailReader *reader)
{
	EMailReaderInterface *interface;

	g_return_val_if_fail (E_IS_MAIL_READER (reader), NULL);

	interface = E_MAIL_READER_GET_INTERFACE (reader);
	g_return_val_if_fail (interface->get_popup_menu != NULL, NULL);

	return interface->get_popup_menu (reader);
}

GPtrArray *
e_mail_reader_get_selected_uids (EMailReader *reader)
{
	EMailReaderInterface *interface;

	g_return_val_if_fail (E_IS_MAIL_READER (reader), NULL);

	interface = E_MAIL_READER_GET_INTERFACE (reader);
	g_return_val_if_fail (interface->get_selected_uids != NULL, NULL);

	return interface->get_selected_uids (reader);
}

GtkWindow *
e_mail_reader_get_window (EMailReader *reader)
{
	EMailReaderInterface *interface;

	g_return_val_if_fail (E_IS_MAIL_READER (reader), NULL);

	interface = E_MAIL_READER_GET_INTERFACE (reader);
	g_return_val_if_fail (interface->get_window != NULL, NULL);

	return interface->get_window (reader);
}

CamelFolder *
e_mail_reader_get_folder (EMailReader *reader)
{
	EMailReaderInterface *interface;

	g_return_val_if_fail (E_IS_MAIL_READER (reader), NULL);

	interface = E_MAIL_READER_GET_INTERFACE (reader);
	g_return_val_if_fail (interface->get_folder != NULL, NULL);

	return interface->get_folder (reader);
}

const gchar *
e_mail_reader_get_folder_uri (EMailReader *reader)
{
	EMailReaderInterface *interface;

	g_return_val_if_fail (E_IS_MAIL_READER (reader), NULL);

	interface = E_MAIL_READER_GET_INTERFACE (reader);
	g_return_val_if_fail (interface->get_folder_uri != NULL, NULL);

	return interface->get_folder_uri (reader);
}

void
e_mail_reader_set_folder (EMailReader *reader,
                          CamelFolder *folder,
                          const gchar *folder_uri)
{
	EMailReaderInterface *interface;

	g_return_if_fail (E_IS_MAIL_READER (reader));

	interface = E_MAIL_READER_GET_INTERFACE (reader);
	g_return_if_fail (interface->set_folder != NULL);

	interface->set_folder (reader, folder, folder_uri);
}

/* Helper for e_mail_reader_set_folder_uri () */
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
	EMailBackend *backend;
	EMailSession *session;

	g_return_if_fail (E_IS_MAIL_READER (reader));
	g_return_if_fail (folder_uri != NULL);

	backend = e_mail_reader_get_backend (reader);
	session = e_mail_backend_get_session (backend);

	/* Fetch the CamelFolder asynchronously. */
	mail_get_folder (
		session, folder_uri, 0, mail_reader_got_folder_cb,
		reader, mail_msg_fast_ordered_push);
}

void
e_mail_reader_set_message (EMailReader *reader,
                           const gchar *uid)
{
	EMailReaderInterface *interface;

	g_return_if_fail (E_IS_MAIL_READER (reader));

	interface = E_MAIL_READER_GET_INTERFACE (reader);
	g_return_if_fail (interface->set_message != NULL);

	interface->set_message (reader, uid);
}

guint
e_mail_reader_open_selected_mail (EMailReader *reader)
{
	EMailReaderInterface *interface;

	g_return_val_if_fail (E_IS_MAIL_READER (reader), 0);

	interface = E_MAIL_READER_GET_INTERFACE (reader);
	g_return_val_if_fail (interface->open_selected_mail != NULL, 0);

	return interface->open_selected_mail (reader);
}

EMailForwardStyle
e_mail_reader_get_forward_style (EMailReader *reader)
{
	g_return_val_if_fail (E_IS_MAIL_READER (reader), 0);

	return (EMailForwardStyle)
		GPOINTER_TO_INT (g_object_get_qdata (
		G_OBJECT (reader), quark_forward_style));
}

void
e_mail_reader_set_forward_style (EMailReader *reader,
                                 EMailForwardStyle style)
{
	g_return_if_fail (E_IS_MAIL_READER (reader));

	g_object_set_qdata (
		G_OBJECT (reader), quark_forward_style,
		GINT_TO_POINTER (style));

	g_object_notify (G_OBJECT (reader), "forward-style");
}

gboolean
e_mail_reader_get_group_by_threads (EMailReader *reader)
{
	EMailReaderPrivate *priv;

	g_return_val_if_fail (E_IS_MAIL_READER (reader), FALSE);

	priv = E_MAIL_READER_GET_PRIVATE (reader);

	return priv->group_by_threads;
}

void
e_mail_reader_set_group_by_threads (EMailReader *reader,
                                    gboolean group_by_threads)
{
	EMailReaderPrivate *priv;
	GtkWidget *message_list;

	g_return_if_fail (E_IS_MAIL_READER (reader));

	priv = E_MAIL_READER_GET_PRIVATE (reader);

	if (group_by_threads == priv->group_by_threads)
		return;

	priv->group_by_threads = group_by_threads;

	/* XXX MessageList should define a property for this. */
	message_list = e_mail_reader_get_message_list (reader);
	message_list_set_threaded (
		MESSAGE_LIST (message_list), group_by_threads);

	g_object_notify (G_OBJECT (reader), "group-by-threads");
}

EMailReplyStyle
e_mail_reader_get_reply_style (EMailReader *reader)
{
	g_return_val_if_fail (E_IS_MAIL_READER (reader), 0);

	return (EMailReplyStyle)
		GPOINTER_TO_INT (g_object_get_qdata (
		G_OBJECT (reader), quark_reply_style));
}

void
e_mail_reader_set_reply_style (EMailReader *reader,
                               EMailReplyStyle style)
{
	g_return_if_fail (E_IS_MAIL_READER (reader));

	g_object_set_qdata (
		G_OBJECT (reader), quark_reply_style,
		GINT_TO_POINTER (style));

	g_object_notify (G_OBJECT (reader), "reply-style");
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

void
e_mail_reader_enable_show_folder (EMailReader *reader)
{
	EMailReaderPrivate *priv;
	CamelFolder *folder;
	GtkAction *action;
	const gchar *action_name;
	const gchar *full_name;
	gboolean sensitive;
	gchar *label;

	g_return_if_fail (E_IS_MAIL_READER (reader));

	priv = E_MAIL_READER_GET_PRIVATE (reader);
	folder = e_mail_reader_get_folder (reader);

	full_name = camel_folder_get_full_name (folder);
	label = g_strdup_printf (_("Folder '%s'"), full_name);

	action_name = "mail-goto-folder";
	action = e_mail_reader_get_action (reader, action_name);
	sensitive = e_mail_reader_get_enable_show_folder (reader);
	gtk_action_set_label (action, label);
	gtk_action_set_visible (action, TRUE);
	gtk_action_set_sensitive (action, sensitive);

	g_free (label);
}

gboolean
e_mail_reader_get_enable_show_folder (EMailReader *reader)
{
	EMailReaderInterface *interface;

	g_return_val_if_fail (E_IS_MAIL_READER (reader), FALSE);

	interface = E_MAIL_READER_GET_INTERFACE (reader);
	g_return_val_if_fail (interface->enable_show_folder != NULL, FALSE);

	return interface->enable_show_folder (reader);
}
