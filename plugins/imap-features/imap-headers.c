/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 *  Author: Sankar P <psankar@novell.com>
 *   
 *  Copyright 2004 Novell, Inc. (www.novell.com)
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
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

#include <mail/em-config.h>
#include <mail/mail-config.h>

#include <gtk/gtkwidget.h>
#include <gtk/gtk.h>

#include <libedataserver/e-account.h>
#include <libedataserver/e-account-list.h>

#include <camel/camel-url.h>
#include <camel/camel-exception.h>

#include <glade/glade.h>

#include <glib/gi18n.h>

GtkWidget *all_headers, *basic_headers, *mailing_list_headers;
GtkWidget *custom_headers_box = NULL;
GtkTreeView *custom_headers_tree;
static GtkTreeStore *store;
GtkTreeIter iter;

GtkButton *add_header, *remove_header;

gchar **custom_headers_array = NULL;

void imap_headers_abort (GtkWidget *button, EConfigHookItemFactoryData *data);
void imap_headers_commit (GtkWidget *button, EConfigHookItemFactoryData *data);
GtkWidget * org_gnome_imap_headers (EPlugin *epl, EConfigHookItemFactoryData *data);

void 
imap_headers_abort (GtkWidget *button, EConfigHookItemFactoryData *data)
{
}

void 
imap_headers_commit (GtkWidget *button, EConfigHookItemFactoryData *data)
{
	EMConfigTargetAccount *target_account;
	EAccount *account;

	target_account = (EMConfigTargetAccount *)data->config->target;
	account = target_account->account;

	if (g_str_has_prefix (account->source->url, "imap://")) {
		EAccount *temp = NULL;
		EAccountList *accounts = mail_config_get_accounts ();
		CamelURL *url = NULL;
		CamelException ex;
		GtkTreeModel *model;
		GtkTreeIter iter;
		GString *str;
		gchar *header = NULL;

		str = g_string_new("");

		temp = mail_config_get_account_by_source_url (account->source->url);

		url = camel_url_new (e_account_get_string(account, E_ACCOUNT_SOURCE_URL), &ex);

		model = gtk_tree_view_get_model (custom_headers_tree);
		if (gtk_tree_model_get_iter_first(model, &iter)) {
			do
			{
				header = NULL;
				gtk_tree_model_get (model, &iter, 0, &header, -1);
				str = g_string_append (str, g_strstrip(header));
				str = g_string_append (str, " ");
				g_free (header);
			} while (gtk_tree_model_iter_next(model, &iter));
		}

		header = g_strstrip(g_strdup(str->str));
		camel_url_set_param (url, "imap_custom_headers", header);
		g_free (header);

		if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(all_headers))) {
			camel_url_set_param (url, "all_headers", "1");
			camel_url_set_param (url, "basic_headers", NULL);
		} else if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(basic_headers))) {
			camel_url_set_param (url, "basic_headers", "1");
			camel_url_set_param (url, "all_headers", NULL);
		} else {
			camel_url_set_param (url, "all_headers", NULL);
			camel_url_set_param (url, "basic_headers", NULL);
		}

		e_account_set_string (temp, E_ACCOUNT_SOURCE_URL, camel_url_to_string (url, 0));
		camel_url_free (url);
		g_string_free (str, TRUE);
		e_account_list_change (accounts, temp);
		e_account_list_save (accounts);
	}
}

static void
add_header_clicked (GtkButton *button)
{
	GtkDialog *dialog;
	GtkEntry *header;
	GtkTreeModel *model;
	GtkTreeIter iter;
	gint result;

	dialog = GTK_DIALOG (gtk_dialog_new_with_buttons (_("Custom Header"), 
			NULL, 
			GTK_DIALOG_MODAL, 
			GTK_STOCK_CANCEL,
			GTK_RESPONSE_REJECT,
			GTK_STOCK_OK,
			GTK_RESPONSE_ACCEPT,
			NULL));
	header = GTK_ENTRY(gtk_entry_new ());
	gtk_container_add (GTK_CONTAINER(dialog->vbox), GTK_WIDGET(header));
	gtk_dialog_set_default_response (dialog, GTK_RESPONSE_ACCEPT);
	gtk_widget_show_all (GTK_WIDGET(dialog));
	result = gtk_dialog_run (GTK_DIALOG (dialog));
	switch (result)
	{
		case GTK_RESPONSE_ACCEPT:
			model = gtk_tree_view_get_model (custom_headers_tree);
			gtk_tree_store_append (GTK_TREE_STORE(model), &iter, NULL); 
			gtk_tree_store_set (GTK_TREE_STORE(model), &iter, 0, gtk_entry_get_text (header), -1);
			break;
	}
	gtk_widget_destroy (GTK_WIDGET(dialog));
}

static void
remove_header_clicked (GtkButton *button)
{
	GtkTreeSelection *select;
	GtkTreeModel *model;
	GtkTreeIter iter;

	select = gtk_tree_view_get_selection (custom_headers_tree);

	if (gtk_tree_selection_get_selected (select, &model, &iter))
	{
		gtk_tree_store_remove(GTK_TREE_STORE(model), &iter);
	}
}

static void
fetch_all_headers_toggled (GtkWidget *all_option)
{
	if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(all_option)))
		gtk_widget_set_sensitive (custom_headers_box, FALSE);
	else
		gtk_widget_set_sensitive (custom_headers_box, TRUE);
}


GtkWidget *
org_gnome_imap_headers (EPlugin *epl, EConfigHookItemFactoryData *data)
{
	EMConfigTargetAccount *target_account;
	EAccount *account;
	GtkWidget *vbox;
	CamelURL *url = NULL;
	CamelException ex;
	char *gladefile;
	GladeXML *gladexml;
	GtkCellRenderer *renderer;
	GtkTreeViewColumn *column;

	target_account = (EMConfigTargetAccount *)data->config->target;
	account = target_account->account;

	if(!g_str_has_prefix (account->source->url, "imap://"))
		return NULL;

	gladefile = g_build_filename (EVOLUTION_GLADEDIR, "imap-headers.glade", NULL);
	gladexml = glade_xml_new (gladefile, "vbox2", NULL);
	g_free (gladefile);

	vbox = glade_xml_get_widget (gladexml, "vbox2");
	all_headers = glade_xml_get_widget (gladexml, "allHeaders");
	basic_headers = glade_xml_get_widget (gladexml, "basicHeaders");
	mailing_list_headers = glade_xml_get_widget (gladexml, "mailingListHeaders");
	custom_headers_box = glade_xml_get_widget (gladexml, "custHeaderHbox");
	custom_headers_tree = GTK_TREE_VIEW(glade_xml_get_widget (gladexml, "custHeaderTree"));
	add_header = GTK_BUTTON(glade_xml_get_widget (gladexml, "addHeader"));
	remove_header = GTK_BUTTON(glade_xml_get_widget (gladexml, "removeHeader"));

	url = camel_url_new (e_account_get_string(account, E_ACCOUNT_SOURCE_URL), &ex);
	if (url) {
		char *custom_headers;
		store = gtk_tree_store_new (1, G_TYPE_STRING);
		custom_headers = g_strdup(camel_url_get_param (url, "imap_custom_headers"));

		if (custom_headers) {
			int i=0;

			custom_headers_array = g_strsplit (custom_headers, " ", -1);
			while (custom_headers_array[i] ) {
				if (strlen(g_strstrip(custom_headers_array[i]))) {
					gtk_tree_store_append (store, &iter, NULL); 
					gtk_tree_store_set (store, &iter, 0, custom_headers_array[i], -1);
				}
				i++;
			}
			g_strfreev (custom_headers_array);
			gtk_tree_view_set_model (custom_headers_tree, GTK_TREE_MODEL(store));
		}
		g_free (custom_headers);

		gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON(mailing_list_headers), TRUE);
		if (camel_url_get_param (url, "all_headers")) {
				gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON(all_headers), TRUE);
				gtk_widget_set_sensitive (custom_headers_box, FALSE);
		} else if (camel_url_get_param (url, "basic_headers")) 
				gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON(basic_headers), TRUE);
		camel_url_free (url);
	}
	renderer = gtk_cell_renderer_text_new ();
	column = gtk_tree_view_column_new_with_attributes (_("Custom Headers"), renderer, "text", 0, NULL);
	gtk_tree_view_append_column (custom_headers_tree , column);

	g_signal_connect (all_headers, "toggled", G_CALLBACK(fetch_all_headers_toggled), NULL);
	g_signal_connect (add_header, "clicked", G_CALLBACK(add_header_clicked), NULL);
	g_signal_connect (remove_header, "clicked", G_CALLBACK(remove_header_clicked), NULL);

	gtk_notebook_append_page ((GtkNotebook *)(data->parent), vbox, gtk_label_new(_("IMAP Headers")));
	gtk_widget_show_all (vbox);
	return NULL;
}
