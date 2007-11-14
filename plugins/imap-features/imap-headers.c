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
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <string.h>

#include <mail/em-config.h>
#include <mail/mail-config.h>

#include <gtk/gtkwidget.h>
#include <gtk/gtktreemodel.h>
#include <gtk/gtk.h>

#include <libedataserver/e-account.h>
#include <libedataserver/e-account-list.h>

#include <camel/camel-url.h>
#include <camel/camel-exception.h>

#include <glade/glade.h>

#include <glib/gi18n.h>

typedef struct _epif_data EPImapFeaturesData;
struct _epif_data {
	GtkWidget *all_headers;
	GtkWidget *basic_headers;
 	GtkWidget *mailing_list_headers;
	GtkWidget *custom_headers_box;

	GtkEntry *entry_header;

	GtkButton *add_header;
 	GtkButton *remove_header;

	GtkTreeView *custom_headers_tree;
	GtkTreeStore *store;

	gchar **custom_headers_array;
};

static EPImapFeaturesData *ui = NULL;

void imap_headers_abort (EPlugin *efp, EConfigHookItemFactoryData *data);
void imap_headers_commit (EPlugin *efp, EConfigHookItemFactoryData *data);
GtkWidget * org_gnome_imap_headers (EPlugin *epl, EConfigHookItemFactoryData *data);

void
imap_headers_abort (EPlugin *efp, EConfigHookItemFactoryData *data)
{
	/* Nothing to do here */
}

void
imap_headers_commit (EPlugin *efp, EConfigHookItemFactoryData *data)
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

		model = gtk_tree_view_get_model (ui->custom_headers_tree);
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

		if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(ui->all_headers))) {
			camel_url_set_param (url, "all_headers", "1");
			camel_url_set_param (url, "basic_headers", NULL);
		} else if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(ui->basic_headers))) {
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

/* return true is the header is considered valid */
static gboolean
epif_header_is_valid (const char *header)
{
	gint len = g_utf8_strlen (header, -1);

	if (header[0] == 0
	    || g_utf8_strchr (header, len, ':') != NULL
	    || g_utf8_strchr (header, len, ' ') != NULL)
		return FALSE;

	return TRUE;
}

static void
epif_add_sensitivity (EPImapFeaturesData *ui)
{
	const char *entry_contents;
	GtkTreeIter iter;
	gboolean valid;

	/* the add header button should be sensitive if the text box contains
	 * a valid header string, that is not a duplicate with something already
	 * in the list view */
	entry_contents = gtk_entry_get_text (GTK_ENTRY (ui->entry_header));
	if (!epif_header_is_valid (entry_contents)) {
		gtk_widget_set_sensitive (GTK_WIDGET (ui->add_header), FALSE);
		return;
	}

	/* check if this is a duplicate */
	valid = gtk_tree_model_get_iter_first (GTK_TREE_MODEL (ui->store), &iter);
	while (valid) {
		char *header_name;

		gtk_tree_model_get (GTK_TREE_MODEL (ui->store), &iter,
						    0, &header_name,
						    -1);
		if (g_ascii_strcasecmp (header_name, entry_contents) == 0) {
			gtk_widget_set_sensitive (GTK_WIDGET (ui->add_header), FALSE);
			return;
 		}

		valid = gtk_tree_model_iter_next (GTK_TREE_MODEL (ui->store), &iter);
	}

	gtk_widget_set_sensitive (GTK_WIDGET (ui->add_header), TRUE);
}

static void
epif_add_header (GtkButton *button, EPImapFeaturesData *ui)
{
	GtkTreeModel *model;
	GtkTreeIter iter, first;

	model = gtk_tree_view_get_model (ui->custom_headers_tree);
	gtk_tree_store_append (GTK_TREE_STORE(model), &iter, NULL);
	gtk_tree_store_set (GTK_TREE_STORE(model), &iter, 0, gtk_entry_get_text (ui->entry_header), -1);

	if (gtk_tree_model_get_iter_first (model, &first)!=FALSE)
		gtk_widget_set_sensitive (GTK_WIDGET (ui->remove_header), TRUE);

	gtk_entry_set_text (ui->entry_header, "");
	epif_add_sensitivity (ui);
}

static void
epif_remove_header_clicked (GtkButton *button, EPImapFeaturesData *ui)
{
	GtkTreeSelection *select;
	GtkTreeModel *model;
	GtkTreeIter iter, first;
	GtkTreePath *path;
	gboolean valid = TRUE;

	select = gtk_tree_view_get_selection (ui->custom_headers_tree);

	if (gtk_tree_selection_get_selected (select, &model, &iter))
	{
		path = gtk_tree_model_get_path (model, &iter);
		gtk_tree_store_remove(GTK_TREE_STORE(model), &iter);

		if (gtk_tree_path_prev (path)) {
			gtk_tree_model_get_iter (model, &iter, path);
		} else {
			valid = gtk_tree_model_get_iter_first (model, &iter);
		}

		if (valid)
			gtk_tree_selection_select_iter (select, &iter);
	}

	if (gtk_tree_model_get_iter_first (model, &first)==FALSE)
		gtk_widget_set_sensitive (GTK_WIDGET (button), FALSE);

	epif_add_sensitivity (ui);
}

static void
epif_fetch_all_headers_toggled (GtkWidget *all_option, EPImapFeaturesData *ui)
{
	if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(all_option)))
		gtk_widget_set_sensitive (ui->custom_headers_box, FALSE);
	else
		gtk_widget_set_sensitive (ui->custom_headers_box, TRUE);
}

static void
epif_entry_changed (GtkWidget *entry, EPImapFeaturesData *ui)
{
	epif_add_sensitivity (ui);
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
	GtkTreeIter first, iter;

	ui = g_new0 (EPImapFeaturesData, 1);

	target_account = (EMConfigTargetAccount *)data->config->target;
	account = target_account->account;

	if(!g_str_has_prefix (account->source->url, "imap://"))
		return NULL;

	gladefile = g_build_filename (EVOLUTION_GLADEDIR, "imap-headers.glade", NULL);
	gladexml = glade_xml_new (gladefile, "vbox2", NULL);
	g_free (gladefile);

	vbox = glade_xml_get_widget (gladexml, "vbox2");
	ui->all_headers = glade_xml_get_widget (gladexml, "allHeaders");
	ui->basic_headers = glade_xml_get_widget (gladexml, "basicHeaders");
	ui->mailing_list_headers = glade_xml_get_widget (gladexml, "mailingListHeaders");
	ui->custom_headers_box = glade_xml_get_widget (gladexml, "custHeaderHbox");
	ui->custom_headers_tree = GTK_TREE_VIEW(glade_xml_get_widget (gladexml, "custHeaderTree"));
	ui->add_header = GTK_BUTTON(glade_xml_get_widget (gladexml, "addHeader"));
	ui->remove_header = GTK_BUTTON(glade_xml_get_widget (gladexml, "removeHeader"));
	ui->entry_header = GTK_ENTRY (glade_xml_get_widget (gladexml, "customHeaderEntry"));

	url = camel_url_new (e_account_get_string(account, E_ACCOUNT_SOURCE_URL), &ex);

	ui->store = gtk_tree_store_new (1, G_TYPE_STRING);
	gtk_tree_view_set_model (ui->custom_headers_tree, GTK_TREE_MODEL(ui->store));

	if (url) {
		char *custom_headers;

		custom_headers = g_strdup(camel_url_get_param (url, "imap_custom_headers"));
		if (custom_headers) {
			int i=0;

			ui->custom_headers_array = g_strsplit (custom_headers, " ", -1);
			while (ui->custom_headers_array[i] ) {
				if (strlen(g_strstrip(ui->custom_headers_array[i]))) {
					gtk_tree_store_append (ui->store, &iter, NULL);
					gtk_tree_store_set (ui->store, &iter, 0, ui->custom_headers_array[i], -1);
				}
				i++;
			}
			g_strfreev (ui->custom_headers_array);

		}
		g_free (custom_headers);

		gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (ui->mailing_list_headers), TRUE);
		if (camel_url_get_param (url, "all_headers")) {
				gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (ui->all_headers), TRUE);
				gtk_widget_set_sensitive (ui->custom_headers_box, FALSE);
		} else if (camel_url_get_param (url, "basic_headers"))
				gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (ui->basic_headers), TRUE);
		camel_url_free (url);
	}
	renderer = gtk_cell_renderer_text_new ();
	column = gtk_tree_view_column_new_with_attributes (_("Custom Headers"), renderer, "text", 0, NULL);
	gtk_tree_view_append_column (ui->custom_headers_tree , column);

	gtk_widget_set_sensitive (GTK_WIDGET (ui->add_header), FALSE);
	if (gtk_tree_model_get_iter_first (gtk_tree_view_get_model (ui->custom_headers_tree), &first)==FALSE)
		gtk_widget_set_sensitive (GTK_WIDGET (ui->remove_header), FALSE);

	g_signal_connect (ui->all_headers, "toggled", G_CALLBACK (epif_fetch_all_headers_toggled), ui);
	g_signal_connect (ui->add_header, "clicked", G_CALLBACK (epif_add_header), ui);
	g_signal_connect (ui->remove_header, "clicked", G_CALLBACK (epif_remove_header_clicked), ui);
	g_signal_connect (ui->entry_header, "changed", G_CALLBACK (epif_entry_changed), ui);
	g_signal_connect (ui->entry_header, "activate", G_CALLBACK (epif_add_header), ui);

	gtk_notebook_append_page ((GtkNotebook *)(data->parent), vbox, gtk_label_new(_("IMAP Headers")));
	gtk_widget_show_all (vbox);

	return GTK_WIDGET (vbox);
}
