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
GtkWidget *org_gnome_exchange_read_url(EPlugin *epl, EConfigHookItemFactoryData *data);

char *owa_entry_text = NULL; 

typedef gboolean (CamelProviderValidateUserFunc) (CamelURL *camel_url, char *url, CamelException *ex);

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
	gboolean valid = TRUE;
	char *account_url, *url_string; 
	const char *source_url;
	static int count = 0;
	char *id_name, *at, *user;

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

		validate->validate_user (url, owa_entry_text, NULL); 
	}
	else
		valid = FALSE; 
	/* FIXME: need to check for return value */
	if (valid) {
		count ++;
		url_string = camel_url_to_string (url, 0);
		e_account_set_string (target_account->account, 
				      E_ACCOUNT_SOURCE_URL, url_string);
		e_account_set_string (target_account->account, 
				      E_ACCOUNT_TRANSPORT_URL, url_string);
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
	GtkWidget *hbox, *button, *label;

	section =  gtk_vbox_new (FALSE, 0);
	gtk_widget_show (section);
	gtk_box_pack_start (GTK_BOX (parent), section, FALSE, FALSE, 0);

	hbox = gtk_hbox_new (FALSE, 6);
	gtk_widget_show (hbox);
	gtk_box_pack_start (GTK_BOX (section), hbox, FALSE, FALSE, 0);
	label = gtk_label_new_with_mnemonic(_("_Url:"));
	gtk_widget_show (label);
	owa_entry = gtk_entry_new ();
	gtk_widget_show (owa_entry);
	button = gtk_button_new_from_stock (GTK_STOCK_OK);
	gtk_widget_set_sensitive (button, FALSE);
	gtk_widget_show (button);

	gtk_box_pack_start (GTK_BOX (hbox), label, TRUE, TRUE, 0);
	gtk_box_pack_start (GTK_BOX (hbox), owa_entry, TRUE, TRUE, 0);
	gtk_box_pack_start (GTK_BOX (hbox), button, TRUE, TRUE, 0);

	g_signal_connect (owa_entry, "changed", 
			  G_CALLBACK (owa_entry_changed), button);
	g_signal_connect (button, "clicked", 
			  G_CALLBACK (ok_button_clicked), target_account);
	
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
