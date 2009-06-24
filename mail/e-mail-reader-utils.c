/*
 * e-mail-reader-utils.c
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

/* Miscellaneous utility functions used by EMailReader actions. */

#include "e-mail-reader-utils.h"

#include <glib/gi18n.h>
#include <gtkhtml/gtkhtml.h>
#include <camel/camel-mime-message.h>
#include <camel/camel-vee-folder.h>
#include <camel/camel-vee-store.h>

#include "e-util/e-error.h"
#include "filter/filter-rule.h"

#include "mail/e-mail-browser.h"
#include "mail/em-composer-utils.h"
#include "mail/em-format-html-print.h"
#include "mail/em-utils.h"
#include "mail/mail-autofilter.h"
#include "mail/mail-ops.h"
#include "mail/mail-tools.h"
#include "mail/mail-vfolder.h"

void
e_mail_reader_activate (EMailReader *reader,
                        const gchar *action_name)
{
	GtkActionGroup *action_group;
	GtkAction *action;

	g_return_if_fail (E_IS_MAIL_READER (reader));
	g_return_if_fail (action_name != NULL);

	action_group = e_mail_reader_get_action_group (reader);
	action = gtk_action_group_get_action (action_group, action_name);
	g_return_if_fail (action != NULL);

	gtk_action_activate (action);
}

gboolean
e_mail_reader_confirm_delete (EMailReader *reader)
{
	EShell *shell;
	EShellBackend *shell_backend;
	EShellSettings *shell_settings;
	MessageList *message_list;
	CamelFolder *folder;
	GtkWidget *check_button;
	GtkWidget *content_area;
	GtkWidget *dialog;
	GtkWindow *window;
	const gchar *label;
	gboolean prompt_delete_in_vfolder;
	gint response;

	/* Remind users what deleting from a search folder does. */

	g_return_val_if_fail (E_IS_MAIL_READER (reader), FALSE);

	message_list = e_mail_reader_get_message_list (reader);
	window = e_mail_reader_get_window (reader);

	shell_backend = e_mail_reader_get_shell_backend (reader);
	shell = e_shell_backend_get_shell (shell_backend);
	shell_settings = e_shell_get_shell_settings (shell);

	folder = message_list->folder;

	prompt_delete_in_vfolder = e_shell_settings_get_boolean (
		shell_settings, "mail-prompt-delete-in-vfolder");

	if (!CAMEL_IS_VEE_STORE (folder->parent_store))
		return TRUE;

	if (!prompt_delete_in_vfolder)
		return TRUE;

	dialog = e_error_new (
		window, "mail:ask-delete-vfolder-msg",
		folder->full_name, NULL);

	/* XXX e-error should provide a widget layout and API suitable
	 *     for packing additional widgets to the right of the alert
	 *     icon.  But for now, screw it. */

	label = _("Do not ask me again");
	content_area = gtk_dialog_get_content_area (GTK_DIALOG (dialog));
	check_button = gtk_check_button_new_with_label (label);
	gtk_box_pack_start (
		GTK_BOX (content_area), check_button, TRUE, TRUE, 6);
	gtk_widget_show (check_button);

	response = gtk_dialog_run (GTK_DIALOG (dialog));

	if (response == GTK_RESPONSE_OK)
		e_shell_settings_set_boolean (
			shell_settings,
			"mail-prompt-delete-in-vfolder",
			gtk_toggle_button_get_active (
			GTK_TOGGLE_BUTTON (check_button)));

	gtk_widget_destroy (dialog);

	return (response == GTK_RESPONSE_OK);
}

void
e_mail_reader_mark_as_read (EMailReader *reader,
                            const gchar *uid)
{
	EMFormatHTMLDisplay *html_display;
	MessageList *message_list;
	CamelFolder *folder;
	guint32 mask, set;
	guint32 flags;

	g_return_if_fail (E_IS_MAIL_READER (reader));
	g_return_if_fail (uid != NULL);

	html_display = e_mail_reader_get_html_display (reader);
	message_list = e_mail_reader_get_message_list (reader);

	folder = message_list->folder;
	flags = camel_folder_get_message_flags (folder, uid);

	if (!(flags & CAMEL_MESSAGE_SEEN)) {
		CamelMimeMessage *message;

		message = ((EMFormat *) html_display)->message;
		em_utils_handle_receipt (folder, uid, message);
	}

	mask = CAMEL_MESSAGE_SEEN;
	set  = CAMEL_MESSAGE_SEEN;
	camel_folder_set_message_flags (folder, uid, mask, set);
}

guint
e_mail_reader_mark_selected (EMailReader *reader,
                             guint32 mask,
                             guint32 set)
{
	MessageList *message_list;
	CamelFolder *folder;
	GPtrArray *uids;
	guint ii;

	g_return_val_if_fail (E_IS_MAIL_READER (reader), 0);

	message_list = e_mail_reader_get_message_list (reader);

	folder = message_list->folder;
	if (folder == NULL)
		return 0;

	camel_folder_freeze (folder);
	uids = message_list_get_selected (message_list);

	for (ii = 0; ii < uids->len; ii++)
		camel_folder_set_message_flags (
			folder, uids->pdata[ii], mask, set);

	message_list_free_uids (message_list, uids);
	camel_folder_thaw (folder);

	return ii;
}

guint
e_mail_reader_open_selected (EMailReader *reader)
{
	EShellBackend *shell_backend;
	MessageList *message_list;
	CamelFolder *folder;
	GtkWindow *window;
	GPtrArray *views;
	GPtrArray *uids;
	const gchar *folder_uri;
	guint ii;

	g_return_val_if_fail (E_IS_MAIL_READER (reader), 0);

	message_list = e_mail_reader_get_message_list (reader);
	shell_backend = e_mail_reader_get_shell_backend (reader);
	window = e_mail_reader_get_window (reader);

	folder = message_list->folder;
	folder_uri = message_list->folder_uri;
	uids = message_list_get_selected (message_list);

	if (uids->len >= 10) {
		gchar *len_str;
		gboolean proceed;

		len_str = g_strdup_printf ("%d", uids->len);

		proceed = em_utils_prompt_user (
			window, "/apps/evolution/mail/prompts/open_many",
			"mail:ask-open-many", len_str, NULL);

		g_free (len_str);

		if (!proceed) {
			message_list_free_uids (message_list, uids);
			return 0;
		}
	}

	if (em_utils_folder_is_drafts (folder, folder_uri) ||
		em_utils_folder_is_outbox (folder, folder_uri) ||
		em_utils_folder_is_templates (folder, folder_uri)) {
		em_utils_edit_messages (folder, uids, TRUE);
		return uids->len;
	}

	views = g_ptr_array_new ();

	/* For vfolders we need to edit the original, not the vfolder copy. */
	for (ii = 0; ii < uids->len; ii++) {
		const gchar *uid = uids->pdata[ii];
		CamelFolder *real_folder;
		CamelMessageInfo *info;
		gchar *real_folder_uri;
		gchar *real_uid;

		if (!CAMEL_IS_VEE_FOLDER (folder)) {
			g_ptr_array_add (views, g_strdup (uid));
			continue;
		}

		info = camel_folder_get_message_info (folder, uid);
		if (info == NULL)
			continue;

		real_folder = camel_vee_folder_get_location (
			CAMEL_VEE_FOLDER (folder),
			(CamelVeeMessageInfo *) info, &real_uid);
		real_folder_uri = mail_tools_folder_to_url (real_folder);

		if (em_utils_folder_is_drafts (real_folder, real_folder_uri) ||
			em_utils_folder_is_outbox (real_folder, real_folder_uri)) {
			GPtrArray *edits;

			edits = g_ptr_array_new ();
			g_ptr_array_add (edits, real_uid);
			em_utils_edit_messages (real_folder, edits, TRUE);
		} else {
			g_free (real_uid);
			g_ptr_array_add (views, g_strdup (uid));
		}

		g_free (real_folder_uri);
	}

	for (ii = 0; ii < views->len; ii++) {
		const gchar *uid = views->pdata[ii];
		GtkWidget *browser;

		browser = e_mail_browser_new (shell_backend);
		e_mail_reader_set_folder (
			E_MAIL_READER (browser), folder, folder_uri);
		e_mail_reader_set_message (
			E_MAIL_READER (browser), uid, FALSE);
		gtk_widget_show (browser);
	}

	g_ptr_array_free (views, TRUE);

	message_list_free_uids (message_list, uids);

	return ii;
}

void
e_mail_reader_print (EMailReader *reader,
                     GtkPrintOperationAction action)
{
	MessageList *message_list;
	EMFormatHTMLDisplay *html_display;
	EMFormatHTMLPrint *html_print;
	CamelFolder *folder;
	GPtrArray *uids;

	g_return_if_fail (E_IS_MAIL_READER (reader));

	html_display = e_mail_reader_get_html_display (reader);
	message_list = e_mail_reader_get_message_list (reader);

	folder = message_list->folder;
	g_return_if_fail (folder != NULL);

	/* XXX Learn to handle len > 1. */
	uids = message_list_get_selected (message_list);
	if (uids->len != 1)
		goto exit;

	html_print = em_format_html_print_new (
		(EMFormatHTML *) html_display, action);
	em_format_merge_handler (
		(EMFormat *) html_print,
		(EMFormat *) html_display);
	em_format_html_print_message (html_print, folder, uids->pdata[0]);
	g_object_unref (html_print);

exit:
	message_list_free_uids (message_list, uids);
}

/* Helper for e_mail_reader_reply_to_message()
 * XXX This function belongs in e-html-utils.c */
static gboolean
html_contains_nonwhitespace (const gchar *html,
                             gint len)
{
	const gchar *cp;
	gunichar uc = 0;

	if (html == NULL || len <= 0)
		return FALSE;

	cp = html;

	while (cp != NULL && cp - html < len) {
		uc = g_utf8_get_char (cp);
		if (uc == 0)
			break;

		if (uc == '<') {
			/* skip until next '>' */
			uc = g_utf8_get_char (cp);
			while (uc != 0 && uc != '>' && cp - html < len) {
				cp = g_utf8_next_char (cp);
				uc = g_utf8_get_char (cp);
			}
			if (uc == 0)
				break;
		} else if (uc == '&') {
			/* sequence '&nbsp;' is a space */
			if (g_ascii_strncasecmp (cp, "&nbsp;", 6) == 0)
				cp = cp + 5;
			else
				break;
		} else if (!g_unichar_isspace (uc))
			break;

		cp = g_utf8_next_char (cp);
	}

	return cp - html < len - 1 && uc != 0;
}

void
e_mail_reader_reply_to_message (EMailReader *reader,
                                gint reply_mode)
{
	EMFormatHTMLDisplay *html_display;
	MessageList *message_list;
	CamelMimeMessage *new_message;
	CamelMimeMessage *src_message;
	CamelFolder *folder;
	GtkWindow *window;
	GtkHTML *html;
	struct _camel_header_raw *header;
	const gchar *uid;
	gchar *selection = NULL;
	gint length;

	/* This handles quoting only selected text in the reply.  If
	 * nothing is selected or only whitespace is selected, fall
	 * back to the normal em_utils_reply_to_message(). */

	g_return_if_fail (E_IS_MAIL_READER (reader));

	html_display = e_mail_reader_get_html_display (reader);
	html = ((EMFormatHTML *) html_display)->html;

	message_list = e_mail_reader_get_message_list (reader);
	window = e_mail_reader_get_window (reader);

	folder = message_list->folder;
	uid = message_list->cursor_uid;
	g_return_if_fail (uid != NULL);

	if (!gtk_html_command (html, "is-selection-active"))
		goto whole_message;

	selection = gtk_html_get_selection_html (html, &length);
	if (selection == NULL || *selection == '\0')
		goto whole_message;

	if (!html_contains_nonwhitespace (selection, length))
		goto whole_message;

	src_message =
		CAMEL_MIME_MESSAGE (((EMFormat *) html_display)->message);
	new_message = camel_mime_message_new ();

	/* Filter out "content-*" headers. */
	header = CAMEL_MIME_PART (src_message)->headers;
	while (header != NULL) {
		if (g_ascii_strncasecmp (header->name, "content-", 8) != 0)
			camel_medium_add_header (
				CAMEL_MEDIUM (new_message),
				header->name, header->value);
	}

	camel_mime_part_set_encoding (
		CAMEL_MIME_PART (new_message),
		CAMEL_TRANSFER_ENCODING_8BIT);

	camel_mime_part_set_content (
		CAMEL_MIME_PART (new_message),
		selection, length, "text/html");

	em_utils_reply_to_message (
		folder, uid, new_message, reply_mode, NULL);

	g_free (selection);

	return;

whole_message:
	em_utils_reply_to_message (
		folder, uid, NULL, reply_mode, (EMFormat *) html_display);
}

void
e_mail_reader_select_next_message (EMailReader *reader,
                                   gboolean or_else_previous)
{
	MessageList *message_list;
	gboolean hide_deleted;
	gboolean success;

	g_return_if_fail (E_IS_MAIL_READER (reader));

	hide_deleted = e_mail_reader_get_hide_deleted (reader);
	message_list = e_mail_reader_get_message_list (reader);

	success = message_list_select (
		message_list, MESSAGE_LIST_SELECT_NEXT, 0, 0);

	if (!success && (hide_deleted || or_else_previous))
		message_list_select (
			message_list, MESSAGE_LIST_SELECT_PREVIOUS, 0, 0);
}

/* Helper for e_mail_reader_create_filter_from_selected() */
static void
mail_reader_create_filter_cb (CamelFolder *folder,
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
e_mail_reader_create_filter_from_selected (EMailReader *reader,
                                           gint filter_type)
{
	MessageList *message_list;
	CamelFolder *folder;
	const gchar *filter_source;
	const gchar *folder_uri;
	GPtrArray *uids;

	struct {
		const gchar *source;
		gint type;
	} *filter_data;

	g_return_if_fail (E_IS_MAIL_READER (reader));

	message_list = e_mail_reader_get_message_list (reader);

	folder = message_list->folder;
	folder_uri = message_list->folder_uri;

	if (em_utils_folder_is_sent (folder, folder_uri))
		filter_source = FILTER_SOURCE_OUTGOING;
	else if (em_utils_folder_is_outbox (folder, folder_uri))
		filter_source = FILTER_SOURCE_OUTGOING;
	else
		filter_source = FILTER_SOURCE_INCOMING;

	uids = message_list_get_selected (message_list);

	if (uids->len == 1) {
		filter_data = g_malloc (sizeof (*filter_data));
		filter_data->source = filter_source;
		filter_data->type = filter_type;

		mail_get_message (
			folder, uids->pdata[0],
			mail_reader_create_filter_cb,
			filter_data, mail_msg_unordered_push);
	}

	em_utils_uids_free (uids);
}

/* Helper for e_mail_reader_create_vfolder_from_selected() */
static void
mail_reader_create_vfolder_cb (CamelFolder *folder,
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
e_mail_reader_create_vfolder_from_selected (EMailReader *reader,
                                            gint vfolder_type)
{
	MessageList *message_list;
	CamelFolder *folder;
	const gchar *folder_uri;
	GPtrArray *uids;

	struct {
		gchar *uri;
		gint type;
	} *vfolder_data;

	g_return_if_fail (E_IS_MAIL_READER (reader));

	message_list = e_mail_reader_get_message_list (reader);

	folder = message_list->folder;
	folder_uri = message_list->folder_uri;

	uids = message_list_get_selected (message_list);

	if (uids->len == 1) {
		vfolder_data = g_malloc (sizeof (*vfolder_data));
		vfolder_data->uri = g_strdup (folder_uri);
		vfolder_data->type = vfolder_type;

		mail_get_message (
			folder, uids->pdata[0],
			mail_reader_create_vfolder_cb,
			vfolder_data, mail_msg_unordered_push);
	}

	em_utils_uids_free (uids);
}
