/*
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
 * Authors:
 *		Vivek Jain <jvivek@novell.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <string.h>
#include <glib.h>
#include <gtk/gtk.h>
#include <glib/gi18n.h>
#include <gconf/gconf-client.h>
#include <e-util/e-config.h>
#include <shell/e-shell.h>
#include <mail/em-config.h>
#include <mail/em-event.h>
#include <mail/em-folder-tree.h>
#include <mail/mail-config.h>
#include <mail/em-folder-selector.h>
#include <e-gw-connection.h>
#include <share-folder.h>

struct AcceptData {
	CamelMimeMessage *msg;
	EMFolderTreeModel *model;
};

void org_gnome_popup_wizard (EPlugin *ep, EMEventTargetMessage *target);

static void
install_folder_response (EMFolderSelector *emfs, gint response, gpointer *data)
{
	struct AcceptData *accept_data = (struct AcceptData *)data;
	EMFolderTreeModel *model;
	const gchar *uri, *path;
	gint parts = 0;
	gchar **names;
	gchar *folder_name;
	gchar *parent_name;
	gchar *container_id;
	const gchar *item_id;
	CamelStore *store;
	CamelFolder *folder;
	EAccount *account;
	CamelProvider *provider;
	EGwConnection *cnc;

	if (response == GTK_RESPONSE_CANCEL) {
		gtk_widget_destroy (GTK_WIDGET (emfs));
	} else {
		CamelSession *session;
		EShell *shell;

		shell = e_shell_get_default ();
		session = e_shell_settings_get_pointer (e_shell_get_shell_settings (shell), "mail-session");

		model = accept_data->model;
		item_id = camel_mime_message_get_message_id (accept_data->msg);
		uri = em_folder_selector_get_selected_uri (emfs);
		path = em_folder_selector_get_selected_path (emfs);
		names = g_strsplit (path, "/", -1);
		if (names == NULL) {
			folder_name = (gchar *)path;
			parent_name = NULL;
		} else {
			while (names[parts])
				parts++;
			folder_name = names[parts -1];
			if (parts >= 2)
				parent_name = names[parts -2];
			else
				parent_name = NULL;
		}
		store = (CamelStore *) camel_session_get_service (
			session, uri, CAMEL_PROVIDER_STORE, NULL);
		if (store == NULL) {
			g_strfreev (names);
			return;
		}

		cnc = get_cnc (store);
		if (E_IS_GW_CONNECTION (cnc)) {
			container_id = get_container_id (cnc, parent_name);

			if (e_gw_connection_accept_shared_folder (cnc, folder_name, container_id, (gchar *)item_id, NULL) == E_GW_CONNECTION_STATUS_OK) {

				folder = camel_store_get_folder (store, "Mailbox", 0, NULL);
				/*changes = camel_folder_change_info_new ();
				camel_folder_change_info_remove_uid (changes, (gchar *) item_id);
				camel_folder_summary_remove_uid (folder->summary, item_id);*/
				/* camel_folder_delete_message (folder, item_id); */
				camel_folder_set_message_flags (folder, item_id, CAMEL_MESSAGE_DELETED, CAMEL_MESSAGE_DELETED);
				camel_folder_summary_touch (folder->summary);
				/* camel_object_trigger_event (CAMEL_OBJECT (folder), "folder_changed", changes); */
				uri = camel_url_to_string (((CamelService *) store)->url, CAMEL_URL_HIDE_ALL);
				account = mail_config_get_account_by_source_url (uri);
				uri = account->source->url;
				em_folder_tree_model_remove_store (model, store);
				provider = camel_provider_get (uri, NULL);
				if (provider == NULL) {
					g_strfreev (names);
					return;
				}

				/* make sure the new store belongs in the tree */
				if (!(provider->flags & CAMEL_PROVIDER_IS_STORAGE)) {
					g_strfreev (names);
					return;
				}

				em_folder_tree_model_add_store (model, store, account->name);
				g_object_unref (store);
			}
		}

		g_strfreev(names);
		gtk_widget_destroy ((GtkWidget *)emfs);
	}

}

static void
accept_free(gpointer data)
{
	struct AcceptData *accept_data = data;

	g_object_unref (accept_data->msg);
	g_free(accept_data);
}

static void
apply_clicked (GtkAssistant *assistant, CamelMimeMessage *msg)
{
	EMFolderTree *folder_tree;
	GtkWidget *dialog;
	struct AcceptData *accept_data;
	gchar *uri;
	gpointer parent;

	parent = gtk_widget_get_toplevel (GTK_WIDGET (assistant));
	parent = gtk_widget_is_toplevel (parent) ? parent : NULL;

	accept_data = g_new0(struct AcceptData, 1);
	folder_tree = (EMFolderTree *) em_folder_tree_new ();

	dialog = em_folder_selector_create_new (parent, folder_tree, 0, _("Create folder"), _("Specify where to create the folder:"));
	uri = em_folder_tree_get_selected_uri(folder_tree);
	em_folder_selector_set_selected ((EMFolderSelector *) dialog, uri);
	g_free(uri);
	accept_data->msg = msg;
	g_object_ref (msg);
	accept_data->model = EM_FOLDER_TREE_MODEL (gtk_tree_view_get_model (GTK_TREE_VIEW (folder_tree)));
	g_object_set_data_full((GObject *)dialog, "accept-data", accept_data, accept_free);
	g_signal_connect (dialog, "response", G_CALLBACK (install_folder_response), accept_data);
	g_object_set_data_full((GObject *)dialog, "assistant", assistant, (GDestroyNotify)gtk_widget_destroy);
	gtk_window_set_title (GTK_WINDOW (dialog), "Install Shared Folder");
	gtk_window_set_modal (GTK_WINDOW (dialog), TRUE);
	gtk_widget_show (dialog);
}

void
org_gnome_popup_wizard (EPlugin *ep, EMEventTargetMessage *target)
{
	CamelInternetAddress *from_addr = NULL;
	const gchar *name;
	const gchar *email;
	CamelMimeMessage *msg = (CamelMimeMessage *) target->message;
	CamelStream *stream;
	CamelDataWrapper *dw;
	GByteArray *byte_array;
	gchar *start_message;

	if (!msg)
		return;

	if (((gchar *)camel_medium_get_header (CAMEL_MEDIUM(msg),"X-notification")) == NULL
	    || (from_addr = camel_mime_message_get_from ((CamelMimeMessage *)target->message)) == NULL
	    || !camel_internet_address_get(from_addr, 0, &name, &email)
	    || (dw = camel_medium_get_content (CAMEL_MEDIUM (msg))) == NULL) {
		return;
	} else {
		if (CAMEL_IS_MULTIPART (dw)) {
			dw = camel_medium_get_content ((CamelMedium *)camel_multipart_get_part((CamelMultipart *)dw, 0));
			if (dw == NULL)
				return;
		}

		byte_array = g_byte_array_new ();
		stream = camel_stream_mem_new_with_byte_array (byte_array);
		camel_data_wrapper_write_to_stream (dw, stream, NULL);
		camel_stream_write (stream, "", 1, NULL);

		from_addr = camel_mime_message_get_from ((CamelMimeMessage *)target->message);
		if (from_addr && camel_internet_address_get(from_addr, 0, &name, &email)) {
			GtkWidget *page;
			GtkAssistant *assistant = GTK_ASSISTANT (gtk_assistant_new ());

			start_message = g_strdup_printf (_("The user '%s' has shared a folder with you\n\n"
							   "Message from '%s'\n\n\n"
							   "%s\n\n\n"
							   "Click 'Apply' to install the shared folder\n\n"),
							   name, name, byte_array->data);

			page = gtk_label_new (start_message);
			gtk_label_set_line_wrap (GTK_LABEL (page), TRUE);
			gtk_misc_set_alignment (GTK_MISC (page), 0.0, 0.0);
			gtk_misc_set_padding (GTK_MISC (page), 10, 10);

			gtk_assistant_append_page (assistant, page);
			gtk_assistant_set_page_title (assistant, page, _("Install the shared folder"));
			gtk_assistant_set_page_type (assistant, page, GTK_ASSISTANT_PAGE_CONFIRM);
			gtk_assistant_set_page_complete (assistant, page, TRUE);

			gtk_window_set_title (GTK_WINDOW (assistant), _("Shared Folder Installation"));
			gtk_window_set_position (GTK_WINDOW (assistant) , GTK_WIN_POS_CENTER_ALWAYS);

			g_object_ref (msg);
			g_object_set_data_full((GObject *)page, "msg", msg, g_object_unref);

			g_signal_connect (assistant, "apply", G_CALLBACK (apply_clicked), msg);

			gtk_widget_show_all (GTK_WIDGET (assistant));

			g_free (start_message);
		} else
			g_warning ("Could not get the sender name");

		g_object_unref (stream);
	}
}

