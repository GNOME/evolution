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

gboolean org_gnome_exchange_ask_password (EPlugin *epl, EConfigHookPageCheckData *data);
GtkWidget *org_gnome_exchange_read_url(EPlugin *epl, EConfigHookItemFactoryData *data);

char *owa_entry_text = NULL; 

static gboolean
validate_exchange_user (EConfig *ec, const char *pageid, void *data)
{
	EMConfigTargetAccount *target_account = data;
	CamelURL *url=NULL;
	CamelProvider *provider = NULL;
	gboolean valid = TRUE;
	char *account_url, *url_string;
	static int count = 0;

	if (count) 
		return valid;

	account_url = g_strdup (target_account->account->source->url);
	url = camel_url_new_with_base (NULL, account_url);
	provider = camel_provider_get (account_url, NULL);
	g_free (account_url);
	if (!provider) {
		return FALSE;	/* This should never happen */
	}

	valid = camel_provider_validate_user (provider, url, owa_entry_text, NULL);
	if (valid) {
		count ++;
		url_string = camel_url_to_string (url, 0);
		target_account->account->source->url = url_string;
	}
	return valid;
}

static void
owa_entry_changed (GtkWidget *entry, void *data) 
{
	/* FIXME: return owa_entry_text instead of making it global */
	owa_entry_text = gtk_entry_get_text (GTK_ENTRY (entry));
}

static GtkWidget *
add_owa_entry (GtkWidget *parent)
{
	GtkWidget *section, *owa_entry;

	section =  gtk_vbox_new (FALSE, 0);
	gtk_widget_show (section);
	gtk_box_pack_start (GTK_BOX (parent), section, FALSE, FALSE, 0);

	owa_entry = gtk_entry_new ();
	gtk_widget_show (owa_entry);
	gtk_box_pack_start (GTK_BOX (section), owa_entry, FALSE, FALSE, 0);

	g_signal_connect (owa_entry, "changed", G_CALLBACK(owa_entry_changed), NULL);
	
	return section;	/* FIXME: return entry */
}

int
e_plugin_lib_enable (EPluginLib *ep, int enable)
{
	if (enable) {
	}
	return 0;
}

GtkWidget *
org_gnome_exchange_read_url(EPlugin *epl, EConfigHookItemFactoryData *data)
{
	EMConfigTargetAccount *target_account;
	char *account_url = NULL, *exchange_url = NULL, *temp_url;
	GtkWidget *owa_entry, *parent;
	CamelURL *url=NULL;

	target_account = (EMConfigTargetAccount *)data->config->target;
	account_url = g_strdup (target_account->account->source->url);
	exchange_url = g_strrstr (account_url, "exchange");

	if (exchange_url) {
		printf ("org_gnome_exchange_read_url \n");
		if (data->old) {
			return data->old;
		}
	
		/* hack for making page check work when host is not there */
		url = camel_url_new_with_base (NULL, account_url);
		if (url->host == NULL) {
			url->host = g_strdup("localhost"); 
			/* FIXME: extract host name from url and use it*/
			temp_url = camel_url_to_string (url, 0);
			target_account->account->source->url = g_strdup(temp_url);
			g_free (temp_url);
		}

		parent = data->parent;
		owa_entry = add_owa_entry(parent);
		g_free (account_url);
		return owa_entry;
	}
	else {
		g_free (account_url);
		return NULL;
	}
}

gboolean
org_gnome_exchange_ask_password(EPlugin *epl, EConfigHookPageCheckData *data)
{
	EMConfigTargetAccount *target_account;
	EConfig *config;
	char *account_url = NULL, *exchange_url = NULL;
	GtkWidget *page;

	if (strcmp (data->pageid, "10.receive"))
		return TRUE;

	config = data->config;	
	target_account = (EMConfigTargetAccount *)data->config->target;
	account_url = g_strdup (target_account->account->source->url);
	exchange_url = g_strrstr (account_url, "exchange");

	/* On page next signal, authenticate user and allow 
	 * to go to receive options page only if user is a valid user */

	if (exchange_url) { 
		page = e_config_get_druid_page (config);
		g_signal_connect (page, "next", G_CALLBACK(validate_exchange_user), target_account);
		g_free (account_url);
		return TRUE;
	}
	else {
		g_free (account_url);
		return TRUE;
	}
}
