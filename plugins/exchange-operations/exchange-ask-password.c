/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 *  Sushma Rai <rsushma@novell.com>
 *  Copyright (C) 2004 Novell, Inc.
 *
 *  Permission is hereby granted, free of charge, to any person
 *  obtaining a copy of this software and associated documentation
 *  files (the "Software"), to deal in the Software without
 *  restriction, including without limitation the rights to use, copy,
 *  modify, merge, publish, distribute, sublicense, and/or sell copies
 *  of the Software, and to permit persons to whom the Software is
 *  furnished to do so, subject to the following conditions:
 *  
 *  The above copyright notice and this permission notice shall be
 *  included in all copies or substantial portions of the Software.
 *  
 *  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 *  EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 *  MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 *  NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
 *  HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 *  WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 *  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 *  DEALINGS IN THE SOFTWARE.
 */

#include <string.h>
#include <libgnome/gnome-i18n.h>
#include <glade/glade.h>
#include <gtk/gtk.h>
#include <gtk/gtkdialog.h>
#include <camel/camel-provider.h>
#include <camel/camel-url.h>
#include "mail/em-account-editor.h"
#include "mail/em-config.h"
#include "e-util/e-account.h"
#include "e-util/e-passwords.h"
#include "e-util/e-config.h"

int e_plugin_lib_enable (EPluginLib *ep, int enable);
void exchange_options_commit (EPlugin *epl, EConfigHookItemFactoryData *data);
GtkWidget *org_gnome_exchange_read_url (EPlugin *epl, EConfigHookItemFactoryData *data);
gboolean org_gnome_exchange_check_options (EPlugin *epl, EConfigHookPageCheckData *data);

const char *owa_entry_text = NULL; 

typedef gboolean (CamelProviderValidateUserFunc) (CamelURL *camel_url, const char *url, gboolean *remember_password, CamelException *ex);

typedef struct {
        CamelProviderValidateUserFunc *validate_user;
}CamelProviderValidate;

int
e_plugin_lib_enable (EPluginLib *ep, int enable)
{
	if (enable) {
	}
	return 0;
}

void
exchange_options_commit (EPlugin *epl, EConfigHookItemFactoryData *data)
{
	return;
}

static gboolean
validate_exchange_user (void *data)
{
	EMConfigTargetAccount *target_account = data;
	CamelProviderValidate *validate;
	CamelURL *url=NULL;
	CamelProvider *provider = NULL;
	gboolean valid = FALSE, *remember_password;
	char *account_url, *url_string; 
	const char *source_url, *id_name;
	static int count = 0;
	char *at, *user;

	if (count)
		return valid;

	source_url = e_account_get_string (target_account->account, 
					   E_ACCOUNT_SOURCE_URL); 
	account_url = g_strdup (source_url);
	provider = camel_provider_get (account_url, NULL);
	if (!provider) {
		return FALSE;	/* This should never happen */
	}
	url = camel_url_new_with_base (NULL, account_url);
	validate = provider->priv; 
	if (validate) {

		if (url->user == NULL) {
			id_name = e_account_get_string (target_account->account,
							E_ACCOUNT_ID_ADDRESS);
			if (id_name) {
				at = strchr(id_name, '@');
				user = g_alloca(at-id_name+1);
				memcpy(user, id_name, at-id_name);
				user[at-id_name] = 0; 

				camel_url_set_user (url, user);
			}
		}
		valid = validate->validate_user (url, owa_entry_text, 
						 remember_password, NULL); 
	}

	/* FIXME: need to check for return value */
	if (valid) {
		count ++;
		url_string = camel_url_to_string (url, 0);
		e_account_set_string (target_account->account, 
				      E_ACCOUNT_SOURCE_URL, url_string);
		e_account_set_string (target_account->account, 
				      E_ACCOUNT_TRANSPORT_URL, url_string);
		target_account->account->source->save_passwd = *remember_password;
	}
	
	camel_url_free (url);
	g_free (account_url);
	return valid;
}

static void
ok_button_clicked (GtkWidget *button, void *data)
{
	gboolean valid = FALSE;
	
	valid = validate_exchange_user (data); // FIXME: return value
}

static void
owa_entry_changed (GtkWidget *entry, void *data) 
{
	GtkWidget *button = data;

	/* FIXME: return owa_entry_text instead of making it global */
	owa_entry_text = gtk_entry_get_text (GTK_ENTRY (entry));
	if (owa_entry_text)
		gtk_widget_set_sensitive (button, TRUE);
}

static GtkWidget *
add_owa_entry (GtkWidget *parent, 
	       EConfig *config, 
	       EMConfigTargetAccount *target_account)
{
	GtkWidget *section, *owa_entry;
        GtkWidget *hbox, *hbox_inner, *label, *button;
        GList *container_list, *l;
        GValue rows = { 0, };
        GValue cols = { 0, };
        gint n_rows, n_cols;

	/* Since configure section in the receive page is not plugin enabled
	 * traversing through the container hierarchy to get the reference
	 * to the table, to which owa_url entry has to be added.
	 * This needs to be changed once we can access configure section from
	 * the plugin.
	 */

        container_list = gtk_container_get_children (GTK_CONTAINER (parent));
        l = g_list_nth (container_list, 1); /* vboxsourceborder */
        container_list = gtk_container_get_children (GTK_CONTAINER (l->data));
        l = g_list_nth (container_list, 0); /* sourcevbox */
        container_list = gtk_container_get_children (GTK_CONTAINER (l->data));
        l = g_list_nth (container_list, 2); /* source frame */
        container_list = gtk_container_get_children (GTK_CONTAINER (l->data));
        l = g_list_nth (container_list, 1); /* hbox173 */
        container_list = gtk_container_get_children (GTK_CONTAINER (l->data));
        l = g_list_nth (container_list, 1); /* table 13 */
        container_list = gtk_container_get_children (GTK_CONTAINER (l->data));
        l = g_list_nth (container_list, 0); /* table 4*/

        g_value_init (&rows, G_TYPE_INT);
        g_value_init (&cols, G_TYPE_INT);
        g_object_get_property (G_OBJECT (l->data), "n-rows", &rows);
        g_object_get_property (G_OBJECT (l->data), "n-columns", &cols);
        n_rows = g_value_get_int (&rows);
        n_cols = g_value_get_int (&cols);

	hbox = gtk_hbox_new (FALSE, 6);
	gtk_widget_show (hbox);

	hbox_inner = gtk_hbox_new (FALSE, 6);
	gtk_widget_show (hbox_inner);

	owa_entry = gtk_entry_new ();
	gtk_widget_show (owa_entry);

	button = gtk_button_new_with_mnemonic (_("A_uthenticate"));
	gtk_widget_set_sensitive (button, FALSE);
	gtk_widget_show (button);

	gtk_box_pack_start (GTK_BOX (hbox_inner), owa_entry, TRUE, TRUE, 0);
	gtk_box_pack_start (GTK_BOX (hbox_inner), button, TRUE, TRUE, 0);

	label = gtk_label_new_with_mnemonic(_("_OWA Url:"));
	gtk_label_set_use_markup (GTK_LABEL (label), TRUE);
	gtk_label_set_justify (GTK_LABEL (label), GTK_JUSTIFY_LEFT);
	gtk_widget_show (label);

	gtk_box_pack_start (GTK_BOX (hbox), hbox_inner, TRUE, TRUE, 0);

        gtk_table_attach (GTK_TABLE (l->data), label, 0, n_cols-1, n_rows, n_rows+1, GTK_FILL, GTK_FILL, 0, 0);
        gtk_table_attach (GTK_TABLE (l->data), hbox, n_cols-1, n_cols, n_rows, n_rows+1, GTK_FILL, GTK_FILL, 0, 0);

	gtk_widget_show (GTK_WIDGET (l->data));

	g_signal_connect (owa_entry, "changed", 
			  G_CALLBACK (owa_entry_changed), button);
	g_signal_connect (button, "clicked", 
			  G_CALLBACK (ok_button_clicked), target_account);

	section =  gtk_vbox_new (FALSE, 0);
        gtk_widget_hide (section);
	return section;	/* FIXME: return entry */
}

GtkWidget *
org_gnome_exchange_read_url (EPlugin *epl, EConfigHookItemFactoryData *data)
{
	EMConfigTargetAccount *target_account;
	EConfig *config;
	char *account_url = NULL, *exchange_url = NULL;
	const char *source_url;
	GtkWidget *owa_entry = NULL, *parent;

	config = data->config;
	target_account = (EMConfigTargetAccount *)data->config->target;

	source_url = e_account_get_string (target_account->account, 
					   E_ACCOUNT_SOURCE_URL); 
	account_url = g_strdup (source_url);
	exchange_url = g_strrstr (account_url, "exchange");

	if (exchange_url) {
		if (data->old) 
			return data->old;

		parent = data->parent;
		owa_entry = add_owa_entry (parent, config, target_account);
	}
	g_free (account_url);
	return owa_entry;
}


GtkWidget *
org_gnome_exchange_handle_auth (EPlugin *epl, EConfigHookItemFactoryData *data)
{
	EMConfigTargetAccount *target_account;
	EConfig *config;
	char *account_url = NULL, *exchange_url = NULL, *url_string;
	const char *source_url;
	char *auth_type;
	GtkWidget *auth_section=NULL, *parent, *section;
	
	config = data->config;
	target_account = (EMConfigTargetAccount *)data->config->target;

	source_url = e_account_get_string (target_account->account, 
					   E_ACCOUNT_SOURCE_URL); 
	account_url = g_strdup (source_url);
	exchange_url = g_strrstr (account_url, "exchange");

	if (exchange_url) {
		parent = data->parent;

		/* We don't need auth section while creating the account. But
		 * we need that in the Editor. And since we get the child vbox
		 * from the plugin, we are finding the parent section and
		 * hiding it. This is a temporary fix and this needs to be handled 
		 * in the proper way. */
		section = gtk_widget_get_parent (gtk_widget_get_parent (parent));
		gtk_widget_hide (section);
	}
	auth_section = gtk_entry_new ();
	gtk_widget_hide (auth_section);
	return auth_section;		
}

GtkWidget *
org_gnome_exchange_handle_send_auth_option (EPlugin *epl, EConfigHookItemFactoryData *data)
{
	EMConfigTargetAccount *target_account;
	EConfig *config;
	char *account_url = NULL, *exchange_url = NULL, *url_string;
	const char *source_url;
	char *auth_type;
	GtkWidget *auth_section=NULL, *parent, *section;
	
	config = data->config;
	target_account = (EMConfigTargetAccount *)data->config->target;

	source_url = e_account_get_string (target_account->account, 
					   E_ACCOUNT_SOURCE_URL); 
	account_url = g_strdup (source_url);
	exchange_url = g_strrstr (account_url, "exchange");

	if (exchange_url) {
		parent = data->parent;
		/* We don't need auth section while creating the account. But
		 * we need that in the Editor. And since we get the child vbox
		 * from the plugin, we are finding the parent section and
		 * hiding it. This is a temporary fix and this needs to be handled 
		 * in the proper way. */
		section = gtk_widget_get_parent (
				gtk_widget_get_parent (gtk_widget_get_parent(parent)));
		gtk_widget_hide (section);
	}
	auth_section = gtk_entry_new ();
	gtk_widget_hide (auth_section);
	return auth_section;		
}

gboolean
org_gnome_exchange_check_options (EPlugin *epl, EConfigHookPageCheckData *data)
{
	EMConfigTargetAccount *target_account;
	EConfig *config;
	char *account_url = NULL, *exchange_url = NULL, *url_string;
	char *use_ssl = NULL;
	static int page_check_count = 0;
	CamelURL *url;
	
	if ((strcmp (data->pageid, "20.receive_options")) || page_check_count)
		return TRUE;

	config = data->config;
	target_account = (EMConfigTargetAccount *)data->config->target;
	account_url = g_strdup (target_account->account->source->url);
	exchange_url = g_strrstr (account_url, "exchange");
	
	if (exchange_url) {
		page_check_count ++;

		if (owa_entry_text){
			if (!strncmp (owa_entry_text, "https:", 6))
				use_ssl = "always";

			url = camel_url_new_with_base (NULL, account_url);

			if (use_ssl)
				camel_url_set_param (url, "use_ssl", use_ssl);
			camel_url_set_param (url, "owa_url", owa_entry_text);

			url_string = camel_url_to_string (url, 0);
			e_account_set_string (target_account->account, 
				      	E_ACCOUNT_SOURCE_URL, url_string);
			camel_url_free (url);
		}
	}
	return TRUE;
}
