/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 *  Authors: Vivek Jain <jvivek@novell.com>
 *
 *  Copyright 2004 Novell, Inc.
 *
 *  This program is free software; you can redistribute it and/or
 *  modify it under the terms of version 2 of the GNU General Public
 *  License as published by the Free Software Foundation.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Street #330, Boston, MA 02111-1307, USA.
 *
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <string.h>
#include <glib.h>
#include <gnome.h>
#include <gtk/gtk.h>
#include <libgnomeui/libgnomeui.h>
#include <libgnome/gnome-i18n.h>
#include <gconf/gconf-client.h>
#include <e-util/e-config.h>
#include <mail/em-config.h>
#include <mail/em-event.h>
#include <mail/mail-component.h>
#include <camel/camel-mime-message.h>
#include <camel/camel-stream.h>
#include <camel/camel-session.h>
#include <camel/camel-stream-mem.h>
#include <camel/camel-data-wrapper.h>
#include <mail/em-folder-tree.h>
#include <mail/mail-config.h>
#include <mail/em-folder-selector.h>
#include <camel/camel-medium.h>
#include <e-gw-connection.h>
#include <share-folder.h>

extern CamelSession *session;
struct AcceptData {
	const char *item_id;
	EMFolderTreeModel *model;
};


void org_gnome_popup_wizard (EPlugin *ep, EMEventTargetMessage *target);

static void
install_folder_response (EMFolderSelector *emfs, int response, gpointer *data)
{
	struct AcceptData *accept_data = (struct AcceptData *)data;
	EMFolderTreeModel *model;
	const char *uri, *path;
	int parts = 0;
	gchar **names;
	gchar *folder_name;
	gchar *parent_name;
	gchar *container_id,*item_id;
	CamelException ex;
	CamelStore *store;
	EAccount *account;
	CamelProvider *provider;
	EGwConnection *cnc;

	if (response == GTK_RESPONSE_CANCEL){
		gtk_widget_destroy (GTK_WIDGET (emfs));
	} else {
		model = accept_data->model;
		item_id = accept_data->item_id;
		g_print("\n\nitem_id :%s\n\n", item_id);

		uri = em_folder_selector_get_selected_uri (emfs);
		path = em_folder_selector_get_selected_path (emfs);
		printf ("Creating new folder: %s (%s)\n", path, uri);
		names = g_strsplit (path, "/", -1);
		if(names == NULL){
			folder_name = (gchar *)path;
			parent_name = NULL;
		} else {
			while (names [parts])
				parts++;
			folder_name = names[parts -1];
			parent_name = names[parts -2];
		}	
		camel_exception_init (&ex);
		if (!(store = (CamelStore *) camel_session_get_service (session, uri, CAMEL_PROVIDER_STORE, &ex))) {
			camel_exception_clear (&ex);
			return;
		}

		cnc = get_cnc (store);	
		if(!cnc)
			g_print ("cnc was null");
		container_id = get_container_id (cnc, parent_name);	

		if(e_gw_connection_accept_shared_folder (cnc, folder_name, container_id, item_id, NULL) == E_GW_CONNECTION_STATUS_OK) {

			uri = camel_url_to_string (((CamelService *) store)->url, CAMEL_URL_HIDE_ALL);
			account = mail_config_get_account_by_source_url (uri);
			uri = account->source->url;
			em_folder_tree_model_remove_store (model, store);
			camel_exception_init (&ex);
			if (!(provider = camel_provider_get(uri, &ex))) {
				camel_exception_clear (&ex);
				return;
			}

			/* make sure the new store belongs in the tree */
			if (!(provider->flags & CAMEL_PROVIDER_IS_STORAGE))
				return;

			em_folder_tree_model_add_store (model, store, account->name);
			camel_object_unref (store);
			//camel_folder_set_message_flags (folder, item_id, CAMEL_MESSAGE_DELETED|CAMEL_MESSAGE_SEEN, CAMEL_MESSAGE_DELETED|CAMEL_MESSAGE_SEEN )
		}  

		gtk_widget_destroy ((GtkWidget *)emfs);
	}

}

static void 
accept_clicked(GnomeDruidPage *page, GtkWidget *druid, const char *id)
{
	g_print("\n\naccepting\n\n");
	EMFolderTreeModel *model;
	EMFolderTree *folder_tree;
	GtkWidget *dialog ;
	struct AcceptData *accept_data; 
	char *uri;
	accept_data = g_new0(struct AcceptData, 1);
	model = mail_component_peek_tree_model (mail_component_peek ());
	folder_tree = (EMFolderTree *) em_folder_tree_new_with_model (model);
	dialog = em_folder_selector_create_new (folder_tree, 0, _("Create folder"), _("Specify where to create the folder:"));
	uri = em_folder_tree_get_selected_uri(folder_tree);
	em_folder_selector_set_selected ((EMFolderSelector *) dialog, uri);
	g_free(uri);
	accept_data->item_id = id;
	accept_data->model = model;
	g_signal_connect (dialog, "response", G_CALLBACK (install_folder_response), accept_data);
	gtk_window_set_title (GTK_WINDOW (dialog), "Install Shared Folder");
	gtk_widget_destroy (druid);
	gtk_widget_show (dialog);

}

void
org_gnome_popup_wizard (EPlugin *ep, EMEventTargetMessage *target)
{
	const CamelInternetAddress *from_addr = NULL;
	const char *name, *item_id;
	const char *email;
	const char *subject;
	GtkWidget *window;
	GnomeDruid *wizard;
	GnomeDruidPageEdge *title_page, *finish_page;
	GnomeDruidPageStandard *middle_page;
	CamelMimeMessage *msg = (CamelMimeMessage *) target->message ;
	CamelStreamMem *content ;
	CamelDataWrapper *dw ;
	CamelMimePart *mime_part = CAMEL_MIME_PART(msg) ;
	char *notification;
	char *start_message;
	char *buffer = NULL;
	char *uri;
	EMFolderTreeModel *model;
	EMFolderTree *folder_tree;
	GtkWidget *selector_dialog ;
	struct AcceptData *accept_data; 

	notification = (char *)camel_medium_get_header (CAMEL_MEDIUM(msg),"X-notification") ;
	if (!notification) {
		return ;
	}

	else {
		dw = camel_data_wrapper_new () ;
		camel_medium_remove_header (CAMEL_MEDIUM (mime_part), "Content-Transfer-Encoding") ;
		camel_medium_remove_header (CAMEL_MEDIUM (mime_part), "Content-Type") ;
		dw = camel_medium_get_content_object (CAMEL_MEDIUM (mime_part));
		content = (CamelStreamMem *)camel_stream_mem_new();
		camel_data_wrapper_write_to_stream(dw, (CamelStream *)content);
		buffer = g_malloc0 (content->buffer->len+1) ;
		buffer = memcpy (buffer, content->buffer->data, content->buffer->len) ;
/*		buffer = camel_mime_message_build_mbox_from ( (CamelMimeMessage *)target->message) ;*/
		from_addr = camel_mime_message_get_from ((CamelMimeMessage *)target->message);
		if (camel_internet_address_get (from_addr,0, &name, &email))
		subject = camel_mime_message_get_subject (target->message) ;

		start_message = g_strconcat (" The User ", "'", name, "'" ," has shared a folder with you\n\n", " Message from ", "'" , name, "'\n", buffer, "Click 'Forward' to install the shared folder\n\n",NULL);

		title_page = GNOME_DRUID_PAGE_EDGE (gnome_druid_page_edge_new_with_vals(GNOME_EDGE_START, TRUE, "Install the shared folder", start_message, NULL, NULL, NULL));
		middle_page = g_object_new (GNOME_TYPE_DRUID_PAGE_STANDARD, "title", "vivek", NULL);
		finish_page = GNOME_DRUID_PAGE_EDGE (gnome_druid_page_edge_new_with_vals(GNOME_EDGE_FINISH, TRUE, "finished Install the shared folder", "said", NULL,NULL, NULL));
		wizard = GNOME_DRUID (gnome_druid_new_with_window ("Shared Folder Installation", NULL, TRUE, (GtkWidget**)(&window)));
		gnome_druid_append_page(wizard, GNOME_DRUID_PAGE(title_page));
		gtk_window_set_position (GTK_WINDOW (window) , GTK_WIN_POS_CENTER_ALWAYS);
		gtk_widget_show_all (GTK_WIDGET (title_page));
		gnome_druid_append_page(wizard, GNOME_DRUID_PAGE(middle_page));	
		gtk_widget_show_all (GTK_WIDGET (middle_page));
		gnome_druid_append_page(wizard, GNOME_DRUID_PAGE(finish_page));
		gtk_widget_show_all (GTK_WIDGET (finish_page));
		model = mail_component_peek_tree_model (mail_component_peek ());
		folder_tree = (EMFolderTree *) em_folder_tree_new_with_model (model);
		selector_dialog = em_folder_selector_create_new (folder_tree, 0, _("Create folder"), _("Specify where to postion the folder:"));
		uri = em_folder_tree_get_selected_uri(folder_tree);
		g_print("\nselected uri:%s\n",uri);
		gtk_widget_destroy (GTK_DIALOG (selector_dialog)->action_area);	
		gtk_widget_show_all (GTK_WIDGET (middle_page));
		item_id = camel_mime_message_get_message_id (msg);
		g_signal_connect (title_page, "next", G_CALLBACK(accept_clicked), item_id);

	}
}

