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
#include <libxml/tree.h>
#include <gtkhtml/gtkhtml.h>
#include <camel/camel.h>

#include "e-util/e-alert-dialog.h"
#include "filter/e-filter-rule.h"
#include "misc/e-web-view.h"

#include "mail/e-mail-browser.h"
#include "mail/em-composer-utils.h"
#include "mail/em-format-html-print.h"
#include "mail/em-utils.h"
#include "mail/mail-autofilter.h"
#include "mail/mail-config.h"
#include "mail/mail-ops.h"
#include "mail/mail-tools.h"
#include "mail/mail-vfolder.h"
#include "mail/message-list.h"

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
	CamelFolder *folder;
	CamelStore *parent_store;
	GtkWidget *check_button;
	GtkWidget *content_area;
	GtkWidget *dialog;
	GtkWindow *window;
	const gchar *label;
	gboolean prompt_delete_in_vfolder;
	gint response;

	/* Remind users what deleting from a search folder does. */

	g_return_val_if_fail (E_IS_MAIL_READER (reader), FALSE);

	folder = e_mail_reader_get_folder (reader);
	window = e_mail_reader_get_window (reader);

	shell_backend = e_mail_reader_get_shell_backend (reader);
	shell = e_shell_backend_get_shell (shell_backend);
	shell_settings = e_shell_get_shell_settings (shell);

	prompt_delete_in_vfolder = e_shell_settings_get_boolean (
		shell_settings, "mail-prompt-delete-in-vfolder");

	parent_store = camel_folder_get_parent_store (folder);

	if (!CAMEL_IS_VEE_STORE (parent_store))
		return TRUE;

	if (!prompt_delete_in_vfolder)
		return TRUE;

	dialog = e_alert_dialog_new_for_args (
		window, "mail:ask-delete-vfolder-msg",
		camel_folder_get_full_name (folder), NULL);

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

	if (response != GTK_RESPONSE_DELETE_EVENT)
		e_shell_settings_set_boolean (
			shell_settings,
			"mail-prompt-delete-in-vfolder",
			!gtk_toggle_button_get_active (
			GTK_TOGGLE_BUTTON (check_button)));

	gtk_widget_destroy (dialog);

	return (response == GTK_RESPONSE_OK);
}

void
e_mail_reader_mark_as_read (EMailReader *reader,
                            const gchar *uid)
{
	EMFormatHTML *formatter;
	CamelFolder *folder;
	guint32 mask, set;
	guint32 flags;

	g_return_if_fail (E_IS_MAIL_READER (reader));
	g_return_if_fail (uid != NULL);

	folder = e_mail_reader_get_folder (reader);
	formatter = e_mail_reader_get_formatter (reader);

	flags = camel_folder_get_message_flags (folder, uid);

	if (!(flags & CAMEL_MESSAGE_SEEN)) {
		CamelMimeMessage *message;

		message = EM_FORMAT (formatter)->message;
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
	CamelFolder *folder;
	GPtrArray *uids;
	guint ii;

	g_return_val_if_fail (E_IS_MAIL_READER (reader), 0);

	folder = e_mail_reader_get_folder (reader);

	if (folder == NULL)
		return 0;

	camel_folder_freeze (folder);
	uids = e_mail_reader_get_selected_uids (reader);

	for (ii = 0; ii < uids->len; ii++)
		camel_folder_set_message_flags (
			folder, uids->pdata[ii], mask, set);

	em_utils_uids_free (uids);
	camel_folder_thaw (folder);

	return ii;
}
static void
copy_tree_state (EMailReader *src_reader, EMailReader *des_reader)
{
	GtkWidget *src_mlist, *des_mlist;
	gchar *state;

	g_return_if_fail (src_reader != NULL);
	g_return_if_fail (des_reader != NULL);

	src_mlist = e_mail_reader_get_message_list (src_reader);
	if (!src_mlist)
		return;

	des_mlist = e_mail_reader_get_message_list (des_reader);
	if (!des_mlist)
		return;

	state = e_tree_get_state (E_TREE (src_mlist));
	if (state)
		e_tree_set_state (E_TREE (des_mlist), state);
	g_free (state);
}

guint
e_mail_reader_open_selected (EMailReader *reader)
{
	EShell *shell;
	EShellBackend *shell_backend;
	CamelFolder *folder;
	GtkWindow *window;
	GPtrArray *views;
	GPtrArray *uids;
	const gchar *folder_uri;
	guint ii;

	g_return_val_if_fail (E_IS_MAIL_READER (reader), 0);

	shell_backend = e_mail_reader_get_shell_backend (reader);
	shell = e_shell_backend_get_shell (shell_backend);

	folder = e_mail_reader_get_folder (reader);
	folder_uri = e_mail_reader_get_folder_uri (reader);
	uids = e_mail_reader_get_selected_uids (reader);
	window = e_mail_reader_get_window (reader);

	if (!em_utils_ask_open_many (window, uids->len)) {
		em_utils_uids_free (uids);
		return 0;
	}

	if (em_utils_folder_is_drafts (folder, folder_uri) ||
		em_utils_folder_is_outbox (folder, folder_uri) ||
		em_utils_folder_is_templates (folder, folder_uri)) {
		em_utils_edit_messages (shell, folder, uids, TRUE);
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
			em_utils_edit_messages (
				shell, real_folder, edits, TRUE);
		} else {
			g_free (real_uid);
			g_ptr_array_add (views, g_strdup (uid));
		}

		g_free (real_folder_uri);
		camel_folder_free_message_info (folder, info);
	}

	for (ii = 0; ii < views->len; ii++) {
		const gchar *uid = views->pdata[ii];
		GtkWidget *browser;

		browser = e_mail_browser_new (shell_backend);
		e_mail_reader_set_folder (
			E_MAIL_READER (browser), folder, folder_uri);
		e_mail_reader_set_message (E_MAIL_READER (browser), uid);
		copy_tree_state (reader, E_MAIL_READER (browser));
		e_mail_reader_set_group_by_threads (
			E_MAIL_READER (browser),
			e_mail_reader_get_group_by_threads (reader));
		gtk_widget_show (browser);
	}

	g_ptr_array_foreach (views, (GFunc) g_free, NULL);
	g_ptr_array_free (views, TRUE);

	em_utils_uids_free (uids);

	return ii;
}

void
e_mail_reader_print (EMailReader *reader,
                     GtkPrintOperationAction action)
{
	EMFormatHTML *formatter;
	EMFormatHTMLPrint *html_print;
	CamelFolder *folder;
	GPtrArray *uids;

	g_return_if_fail (E_IS_MAIL_READER (reader));

	folder = e_mail_reader_get_folder (reader);
	g_return_if_fail (folder != NULL);

	/* XXX Learn to handle len > 1. */
	uids = e_mail_reader_get_selected_uids (reader);
	if (uids->len != 1)
		goto exit;

	formatter = e_mail_reader_get_formatter (reader);

	html_print = em_format_html_print_new (formatter, action);
	em_format_merge_handler (
		EM_FORMAT (html_print), EM_FORMAT (formatter));
	em_format_html_print_message (html_print, folder, uids->pdata[0]);
	g_object_unref (html_print);

exit:
	em_utils_uids_free (uids);
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
                                CamelMimeMessage *src_message,
                                gint reply_mode)
{
	EShell *shell;
	EShellBackend *shell_backend;
	EMFormatHTML *formatter;
	GtkWidget *message_list;
	CamelMimeMessage *new_message;
	CamelFolder *folder;
	EWebView *web_view;
	struct _camel_header_raw *header;
	const gchar *uid;
	gchar *selection = NULL;
	gint length;

	/* This handles quoting only selected text in the reply.  If
	 * nothing is selected or only whitespace is selected, fall
	 * back to the normal em_utils_reply_to_message(). */

	g_return_if_fail (E_IS_MAIL_READER (reader));

	shell_backend = e_mail_reader_get_shell_backend (reader);
	shell = e_shell_backend_get_shell (shell_backend);

	formatter = e_mail_reader_get_formatter (reader);
	web_view = em_format_html_get_web_view (formatter);

	folder = e_mail_reader_get_folder (reader);
	message_list = e_mail_reader_get_message_list (reader);

	uid = MESSAGE_LIST (message_list)->cursor_uid;
	g_return_if_fail (uid != NULL);

	if (!gtk_widget_get_mapped (GTK_WIDGET(web_view)))
		goto whole_message;

	if (src_message == NULL) {
		src_message = EM_FORMAT (formatter)->message;
		if (src_message != NULL)
			g_object_ref (src_message);
	}

	if (!e_web_view_is_selection_active (web_view))
		goto whole_message;

	selection = gtk_html_get_selection_html (GTK_HTML (web_view), &length);
	if (selection == NULL || *selection == '\0')
		goto whole_message;

	if (!html_contains_nonwhitespace (selection, length))
		goto whole_message;

	new_message = camel_mime_message_new ();

	/* Filter out "content-*" headers. */
	header = CAMEL_MIME_PART (src_message)->headers;
	while (header != NULL) {
		if (g_ascii_strncasecmp (header->name, "content-", 8) != 0)
			camel_medium_add_header (
				CAMEL_MEDIUM (new_message),
				header->name, header->value);

		header = header->next;
	}

	camel_mime_part_set_encoding (
		CAMEL_MIME_PART (new_message),
		CAMEL_TRANSFER_ENCODING_8BIT);

	camel_mime_part_set_content (
		CAMEL_MIME_PART (new_message),
		selection, length, "text/html");

	g_object_unref (src_message);

	em_utils_reply_to_message (
		shell, folder, uid, new_message, reply_mode, NULL);

	g_free (selection);

	return;

whole_message:
	em_utils_reply_to_message (
		shell, folder, uid, src_message,
		reply_mode, EM_FORMAT (formatter));
}

void
e_mail_reader_select_next_message (EMailReader *reader,
                                   gboolean or_else_previous)
{
	GtkWidget *message_list;
	gboolean hide_deleted;
	gboolean success;

	g_return_if_fail (E_IS_MAIL_READER (reader));

	hide_deleted = e_mail_reader_get_hide_deleted (reader);
	message_list = e_mail_reader_get_message_list (reader);

	success = message_list_select (
		MESSAGE_LIST (message_list),
		MESSAGE_LIST_SELECT_NEXT, 0, 0);

	if (!success && (hide_deleted || or_else_previous))
		message_list_select (
			MESSAGE_LIST (message_list),
			MESSAGE_LIST_SELECT_PREVIOUS, 0, 0);
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
	CamelFolder *folder;
	const gchar *filter_source;
	const gchar *folder_uri;
	GPtrArray *uids;

	struct {
		const gchar *source;
		gint type;
	} *filter_data;

	g_return_if_fail (E_IS_MAIL_READER (reader));

	folder = e_mail_reader_get_folder (reader);
	folder_uri = e_mail_reader_get_folder_uri (reader);

	if (em_utils_folder_is_sent (folder, folder_uri))
		filter_source = E_FILTER_SOURCE_OUTGOING;
	else if (em_utils_folder_is_outbox (folder, folder_uri))
		filter_source = E_FILTER_SOURCE_OUTGOING;
	else
		filter_source = E_FILTER_SOURCE_INCOMING;

	uids = e_mail_reader_get_selected_uids (reader);

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
	CamelFolder *folder;
	const gchar *folder_uri;
	GPtrArray *uids;

	struct {
		gchar *uri;
		gint type;
	} *vfolder_data;

	g_return_if_fail (E_IS_MAIL_READER (reader));

	folder = e_mail_reader_get_folder (reader);
	folder_uri = e_mail_reader_get_folder_uri (reader);
	uids = e_mail_reader_get_selected_uids (reader);

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

static EMailReaderHeader *
emr_header_from_xmldoc (xmlDocPtr doc)
{
	EMailReaderHeader *h;
	xmlNodePtr root;
	xmlChar *name;

	if (doc == NULL)
		return NULL;

	root = doc->children;
	if (strcmp ((gchar *)root->name, "header") != 0)
		return NULL;

	name = xmlGetProp (root, (const guchar *)"name");
	if (name == NULL)
		return NULL;

	h = g_malloc0 (sizeof (EMailReaderHeader));
	h->name = g_strdup ((gchar *)name);
	xmlFree (name);

	if (xmlHasProp (root, (const guchar *)"enabled"))
		h->enabled = 1;
	else
		h->enabled = 0;

	return h;
}

/**
 * e_mail_reader_header_from_xml
 * @xml: XML configuration data
 *
 * Parses passed XML data, which should be of
 * the format <header name="foo" enabled />, and
 * returns a EMailReaderHeader structure, or NULL if there
 * is an error.
 **/
EMailReaderHeader *
e_mail_reader_header_from_xml (const gchar *xml)
{
	EMailReaderHeader *header;
	xmlDocPtr doc;

	if (!(doc = xmlParseDoc ((guchar *) xml)))
		return NULL;

	header = emr_header_from_xmldoc (doc);
	xmlFreeDoc (doc);

	return header;
}

/**
 * e_mail_reader_header_to_xml
 * @header: header from which to generate XML
 *
 * Returns the passed header as a XML structure,
 * or NULL on error
 */
gchar *
e_mail_reader_header_to_xml (EMailReaderHeader *header)
{
	xmlDocPtr doc;
	xmlNodePtr root;
	xmlChar *xml;
	gchar *out;
	gint size;

	g_return_val_if_fail (header != NULL, NULL);
	g_return_val_if_fail (header->name != NULL, NULL);

	doc = xmlNewDoc ((const guchar *)"1.0");

	root = xmlNewDocNode (doc, NULL, (const guchar *)"header", NULL);
	xmlSetProp (root, (const guchar *)"name", (guchar *)header->name);
	if (header->enabled)
		xmlSetProp (root, (const guchar *)"enabled", NULL);

	xmlDocSetRootElement (doc, root);
	xmlDocDumpMemory (doc, &xml, &size);
	xmlFreeDoc (doc);

	out = g_malloc (size + 1);
	memcpy (out, xml, size);
	out[size] = '\0';
	xmlFree (xml);

	return out;
}

/**
 * e_mail_reader_header_free
 * @header: header to free
 *
 * Frees the memory associated with the passed header
 * structure.
 */
void
e_mail_reader_header_free (EMailReaderHeader *header)
{
	if (header == NULL)
		return;

	g_free (header->name);
	g_free (header);
}

static void
headers_changed_cb (GConfClient *client,
                    guint cnxn_id,
                    GConfEntry *entry,
                    EMailReader *reader)
{
	EMFormatHTML *formatter;
	GSList *header_config_list, *p;

	g_return_if_fail (client != NULL);
	g_return_if_fail (reader != NULL);

	formatter = e_mail_reader_get_formatter (reader);

	header_config_list = gconf_client_get_list (
		client, "/apps/evolution/mail/display/headers",
		GCONF_VALUE_STRING, NULL);
	em_format_clear_headers (EM_FORMAT (formatter));
	for (p = header_config_list; p; p = g_slist_next(p)) {
		EMailReaderHeader *h;
		gchar *xml = (gchar *)p->data;

		h = e_mail_reader_header_from_xml (xml);
		if (h && h->enabled)
			em_format_add_header (
				EM_FORMAT (formatter),
				h->name, EM_FORMAT_HEADER_BOLD);

		e_mail_reader_header_free (h);
	}

	if (!header_config_list)
		em_format_default_headers (EM_FORMAT (formatter));

	g_slist_foreach (header_config_list, (GFunc) g_free, NULL);
	g_slist_free (header_config_list);

	/* force a redraw */
	if (EM_FORMAT (formatter)->message)
		em_format_queue_redraw (EM_FORMAT (formatter));
}

static void
remove_header_notify_cb (gpointer data)
{
	GConfClient *client = mail_config_get_gconf_client ();
	guint notify_id;

	g_return_if_fail (client != NULL);

	notify_id = GPOINTER_TO_INT (data);
	g_return_if_fail (notify_id != 0);

	gconf_client_notify_remove (client, notify_id);
	gconf_client_remove_dir (client, "/apps/evolution/mail/display", NULL);
}

/**
 * e_mail_reader_connect_headers
 * @reader: an #EMailReader
 *
 * Connects @reader to listening for changes in headers and
 * updates the EMFormat whenever it changes and on this call too.
 **/
void
e_mail_reader_connect_headers (EMailReader *reader)
{
	GConfClient *client = mail_config_get_gconf_client ();
	guint notify_id;

	gconf_client_add_dir (
		client, "/apps/evolution/mail/display",
		GCONF_CLIENT_PRELOAD_NONE, NULL);
	notify_id = gconf_client_notify_add (
		client, "/apps/evolution/mail/display/headers",
		(GConfClientNotifyFunc) headers_changed_cb,
		reader, NULL, NULL);

	g_object_set_data_full (
		G_OBJECT (reader), "reader-header-notify-id",
		GINT_TO_POINTER (notify_id), remove_header_notify_cb);

	headers_changed_cb (client, 0, NULL, reader);
}
