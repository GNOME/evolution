/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 *  Sushma Rai <rsushma@novell.com>
 *  Copyright (C) 2004 Novell, Inc.
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
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <string.h>
#include <glib/gi18n.h>
#include <glade/glade.h>
#include <gtk/gtk.h>
#include <gtk/gtkdialog.h>
#include <gconf/gconf-client.h>
#include <camel/camel-provider.h>
#include <camel/camel-url.h>
#include "mail/em-account-editor.h"
#include "mail/em-config.h"
#include "e-util/e-account.h"

GtkWidget* org_gnome_exchange_settings(EPlugin *epl, EConfigHookItemFactoryData *data);
GtkWidget *org_gnome_exchange_owa_url(EPlugin *epl, EConfigHookItemFactoryData *data);
gboolean org_gnome_exchange_check_options(EPlugin *epl, EConfigHookPageCheckData *data);

/* NB: This should be given a better name, it is NOT a camel service, it is only a camel-exchange one */
typedef gboolean (CamelProviderValidateUserFunc) (CamelURL *camel_url, const char *url, gboolean *remember_password, CamelException *ex);
typedef struct {
        CamelProviderValidateUserFunc *validate_user;
}CamelProviderValidate;

/* only used in editor */
GtkWidget *
org_gnome_exchange_settings(EPlugin *epl, EConfigHookItemFactoryData *data)
{
	EMConfigTargetAccount *target_account;
	const char *source_url;
	CamelURL *url;
	GtkWidget *oof_page;
	GtkWidget *oof_table;
	GtkWidget *oof_description, *label_status, *label_empty;
	GtkWidget *radiobutton_inoff, *radiobutton_oof;
	GtkWidget *vbox_oof, *vbox_oof_message;
	GtkWidget *oof_frame;
	GtkWidget *scrolledwindow_oof;
	GtkWidget *textview_oof;
	char *txt;

	target_account = (EMConfigTargetAccount *)data->config->target;
	source_url = e_account_get_string (target_account->account,  E_ACCOUNT_SOURCE_URL);
	url = camel_url_new(source_url, NULL);
	if (url == NULL
	    || strcmp(url->protocol, "exchange") != 0) {
		if (url)
			camel_url_free(url);
		return NULL;
	}

	if (data->old) {
		camel_url_free(url);
		return data->old;
	}

	/* FIXME: This out of office data never goes anywhere */

	oof_page = gtk_vbox_new (FALSE, 6);
	gtk_container_set_border_width (GTK_CONTAINER (oof_page), 12);

	/* Description section */

	oof_description = gtk_label_new (_("The message specified below will be automatically sent to \neach person who sends mail to you while you are out of the office."));
	gtk_label_set_justify (GTK_LABEL (oof_description), GTK_JUSTIFY_LEFT);
	gtk_label_set_line_wrap (GTK_LABEL (oof_description), TRUE);
	gtk_misc_set_alignment (GTK_MISC (oof_description), 0.5, 0.5);
	gtk_misc_set_padding (GTK_MISC (oof_description), 0, 18);
	
	gtk_box_pack_start (GTK_BOX (oof_page), oof_description, FALSE, TRUE, 0);

	/* Table with out of office radio buttons */

	oof_table = gtk_table_new (2, 2, FALSE);
	gtk_table_set_col_spacings (GTK_TABLE (oof_table), 6);
	gtk_table_set_row_spacings (GTK_TABLE (oof_table), 6);
	gtk_box_pack_start (GTK_BOX (oof_page), oof_table, FALSE, FALSE, 0);

	/* translators: exchange out of office status header */
	txt = g_strdup_printf("<b>%s</b>", _("Status:"));
	label_status = gtk_label_new (txt);
	g_free(txt);
	gtk_label_set_justify (GTK_LABEL (label_status), GTK_JUSTIFY_CENTER);
	gtk_misc_set_alignment (GTK_MISC (label_status), 0, 0.5);
	gtk_misc_set_padding (GTK_MISC (label_status), 0, 0); 
	gtk_label_set_use_markup (GTK_LABEL (label_status), TRUE);
	gtk_table_attach (GTK_TABLE (oof_table), label_status, 0, 1, 0, 1, 
			  GTK_FILL, GTK_FILL, 0, 0); 

	radiobutton_inoff = gtk_radio_button_new_with_label (NULL, 
						_("I am in the office"));
	gtk_table_attach (GTK_TABLE (oof_table), radiobutton_inoff, 1, 2, 0, 1, 
			  GTK_FILL, GTK_FILL, 0, 0);

	label_empty = gtk_label_new ("");
	gtk_label_set_justify (GTK_LABEL (label_empty), GTK_JUSTIFY_LEFT);
	gtk_misc_set_alignment (GTK_MISC (label_empty), 0, 0.5);
	gtk_misc_set_padding (GTK_MISC (label_empty), 0, 0);
	gtk_label_set_use_markup (GTK_LABEL (label_empty), FALSE);
	gtk_table_attach (GTK_TABLE (oof_table), label_empty, 0, 1, 1, 2, 
			  GTK_FILL, GTK_FILL, 0, 0);

	radiobutton_oof = gtk_radio_button_new_with_label_from_widget (
					GTK_RADIO_BUTTON (radiobutton_inoff), 
					_("I am out of the office"));


	gtk_table_attach (GTK_TABLE (oof_table), radiobutton_oof, 1, 2, 1, 2, 
			  GTK_FILL, GTK_FILL, 0, 0);

	/* frame containg oof message text box */

	vbox_oof = gtk_vbox_new (FALSE, 6);
	gtk_box_pack_start (GTK_BOX (oof_page), vbox_oof, FALSE, FALSE, 0);

	oof_frame = gtk_frame_new ("");
	gtk_container_set_border_width (GTK_CONTAINER (oof_frame), 1); 
	gtk_frame_set_shadow_type (GTK_FRAME (oof_frame), GTK_SHADOW_ETCHED_IN);
	gtk_frame_set_label (GTK_FRAME (oof_frame), _("Out of office Message:"));
	gtk_box_pack_start (GTK_BOX (vbox_oof), oof_frame, FALSE, FALSE, 0);

	vbox_oof_message = gtk_vbox_new (FALSE, 6);
	gtk_container_add (GTK_CONTAINER (oof_frame), vbox_oof_message);
	
	scrolledwindow_oof = gtk_scrolled_window_new(NULL, NULL);
	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scrolledwindow_oof), 
			GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
	gtk_scrolled_window_set_shadow_type (
			GTK_SCROLLED_WINDOW (scrolledwindow_oof), 
			GTK_SHADOW_IN); 
	gtk_box_pack_start (GTK_BOX (vbox_oof_message), 
			    scrolledwindow_oof, TRUE, TRUE, 0);

	textview_oof = gtk_text_view_new(); 
	gtk_text_view_set_justification (GTK_TEXT_VIEW (textview_oof), 
					 GTK_JUSTIFY_LEFT);
	gtk_text_view_set_wrap_mode (GTK_TEXT_VIEW (textview_oof), 
				     GTK_WRAP_WORD);
	gtk_text_view_set_editable (GTK_TEXT_VIEW (textview_oof), TRUE);
	gtk_container_add (GTK_CONTAINER (scrolledwindow_oof), textview_oof);	
	gtk_widget_show_all (scrolledwindow_oof);

	gtk_widget_show_all (oof_page);

	gtk_notebook_insert_page (GTK_NOTEBOOK (data->parent), oof_page, gtk_label_new(_("Exchange Settings")), 4);

	return oof_page;
}

static void
owa_authenticate_user(GtkWidget *button, EConfig *config)
{
	EMConfigTargetAccount *target_account = (EMConfigTargetAccount *)config->target;
	CamelProviderValidate *validate;
	CamelURL *url=NULL;
	CamelProvider *provider = NULL;
	gboolean remember_password;
	char *url_string; 
	const char *source_url, *id_name;
	char *at, *user;

	source_url = e_account_get_string (target_account->account, E_ACCOUNT_SOURCE_URL);
	provider = camel_provider_get (source_url, NULL);
	if (!provider || provider->priv == NULL) {
		/* can't happen? */
		return;
	}

	url = camel_url_new(source_url, NULL);
	validate = provider->priv; 
	if (url->user == NULL) {
		id_name = e_account_get_string (target_account->account, E_ACCOUNT_ID_ADDRESS);
		if (id_name) {
			at = strchr(id_name, '@');
			user = g_alloca(at-id_name+1);
			memcpy(user, id_name, at-id_name);
			user[at-id_name] = 0; 
			camel_url_set_user (url, user);
		}
	}

	/* validate_user() CALLS GTK!!!

	   THIS IS TOTALLY UNNACCEPTABLE!!!!!!!!

	   It must use camel_session_ask_password, and it should return an exception for any problem,
	   which should then be shown using e-error */

	if (validate->validate_user(url, camel_url_get_param(url, "owa_url"), &remember_password, NULL)) {
		url_string = camel_url_to_string (url, 0);
		e_account_set_string(target_account->account, E_ACCOUNT_SOURCE_URL, url_string);
		e_account_set_string(target_account->account, E_ACCOUNT_TRANSPORT_URL, url_string);
		e_account_set_bool(target_account->account, E_ACCOUNT_SOURCE_SAVE_PASSWD, remember_password);
		g_free(url_string);
	}
	
	camel_url_free (url);
}

static void
owa_editor_entry_changed(GtkWidget *entry, EConfig *config)
{
	const char *uri, *ssl = NULL;
	CamelURL *url, *owaurl = NULL;
	char *url_string;
	EMConfigTargetAccount *target = (EMConfigTargetAccount *)config->target;
	GtkWidget *button = g_object_get_data((GObject *)entry, "authenticate-button");
	int active = FALSE;

	/* NB: we set the button active only if we have a parsable uri entered */

	url = camel_url_new(e_account_get_string(target->account, E_ACCOUNT_SOURCE_URL), NULL);
	uri = gtk_entry_get_text((GtkEntry *)entry);
	if (uri && uri[0]) {
		camel_url_set_param(url, "owa_url", uri);
		owaurl = camel_url_new(uri, NULL);
		if (owaurl) {
			active = TRUE;

			/* i'm not sure why we need this, "ssl connection mode" is redundant
			   since we have it in the owa-url protocol */
			if (!strcmp(owaurl->protocol, "https"))
				ssl = "always";
			camel_url_free(owaurl);
		}
	} else {
		camel_url_set_param(url, "owa_url", NULL);
	}

	camel_url_set_param(url, "use_ssl", ssl);
	gtk_widget_set_sensitive(button, active);

	url_string = camel_url_to_string(url, 0);
	e_account_set_string(target->account, E_ACCOUNT_SOURCE_URL, url_string);
	g_free(url_string);
}

static void
destroy_label(GtkWidget *old, GtkWidget *label)
{
	gtk_widget_destroy(label);
}

/* used by editor and druid - same code */
GtkWidget *
org_gnome_exchange_owa_url(EPlugin *epl, EConfigHookItemFactoryData *data)
{
	EMConfigTargetAccount *target_account;
	const char *source_url, *owa_url;
	GtkWidget *owa_entry;
	CamelURL *url;
	int row;
	GtkWidget *hbox, *label, *button;

	target_account = (EMConfigTargetAccount *)data->config->target;
	source_url = e_account_get_string (target_account->account,  E_ACCOUNT_SOURCE_URL);
	url = camel_url_new(source_url, NULL);
	if (url == NULL
	    || strcmp(url->protocol, "exchange") != 0) {
		if (url)
			camel_url_free(url);

		if (data->old
		    && (label = g_object_get_data((GObject *)data->old, "authenticate-label")))
			gtk_widget_destroy(label);

		/* TODO: we could remove 'owa-url' from the url,
		   but that will lose it if we come back.  Maybe a commit callback could do it */

		return NULL;
	}

	if (data->old) {
		camel_url_free(url);
		return data->old;
	}

	owa_url = camel_url_get_param(url, "owa_url");

	row = ((GtkTable *)data->parent)->nrows;

	hbox = gtk_hbox_new (FALSE, 6);
	label = gtk_label_new_with_mnemonic(_("_OWA Url:"));
	gtk_widget_show(label);

	owa_entry = gtk_entry_new();
	if (owa_url)
		gtk_entry_set_text(GTK_ENTRY (owa_entry), owa_url); 
	gtk_label_set_mnemonic_widget((GtkLabel *)label, owa_entry);

	button = gtk_button_new_with_mnemonic (_("A_uthenticate"));
	gtk_widget_set_sensitive (button, owa_url && owa_url[0]);

	gtk_box_pack_start (GTK_BOX (hbox), owa_entry, TRUE, TRUE, 0);
	gtk_box_pack_start (GTK_BOX (hbox), button, FALSE, FALSE, 0);
	gtk_widget_show_all(hbox);

	gtk_table_attach (GTK_TABLE (data->parent), label, 0, 1, row, row+1, 0, 0, 0, 0); 
	gtk_table_attach (GTK_TABLE (data->parent), hbox, 1, 2, row, row+1, GTK_FILL|GTK_EXPAND, GTK_FILL, 0, 0); 

	g_signal_connect (owa_entry, "changed", G_CALLBACK(owa_editor_entry_changed), data->config);
	g_object_set_data((GObject *)owa_entry, "authenticate-button", button);
	g_signal_connect (button, "clicked", G_CALLBACK(owa_authenticate_user), data->config);

	/* Track the authenticate label, so we can destroy it if e-config is to destroy the hbox */
	g_object_set_data((GObject *)hbox, "authenticate-label", label);

	return hbox;
}

gboolean
org_gnome_exchange_check_options(EPlugin *epl, EConfigHookPageCheckData *data)
{
	EMConfigTargetAccount *target = (EMConfigTargetAccount *)data->config->target;
	int status = TRUE;

	/* We assume that if the host is set, then the setting is valid.
	   The host gets set when the provider validate() call is made */
	if (data->pageid == NULL || strcmp(data->pageid, "20.receive_options") == 0) {
		CamelURL *url;

		url = camel_url_new(e_account_get_string(target->account,  E_ACCOUNT_SOURCE_URL), NULL);
		/* Note: we only care about exchange url's, we WILL get called on all other url's too. */
		if (url != NULL
		    && strcmp(url->protocol, "exchange") == 0
		    && (url->host == NULL || url->host[0] == 0))
			status = FALSE;

		if (url)
			camel_url_free(url);
	}

	return status;
}
