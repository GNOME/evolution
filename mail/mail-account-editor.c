/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 *  Authors: Jeffrey Stedfast <fejj@helixcode.com>
 *
 *  Copyright 2001 Helix Code, Inc. (www.helixcode.com)
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

#include "mail-account-editor.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <camel/camel-url.h>


static void mail_account_editor_class_init (MailAccountEditorClass *class);
static void mail_account_editor_init       (MailAccountEditor *editor);
static void mail_account_editor_finalise   (GtkObject *obj);

static GnomeDialogClass *parent_class;


GtkType
mail_account_editor_get_type ()
{
	static GtkType type = 0;
	
	if (!type) {
		GtkTypeInfo type_info = {
			"MailAccountEditor",
			sizeof (MailAccountEditor),
			sizeof (MailAccountEditorClass),
			(GtkClassInitFunc) mail_account_editor_class_init,
			(GtkObjectInitFunc) mail_account_editor_init,
			(GtkArgSetFunc) NULL,
			(GtkArgGetFunc) NULL
		};
		
		type = gtk_type_unique (gnome_dialog_get_type (), &type_info);
	}
	
	return type;
}

static void
mail_account_editor_class_init (MailAccountEditorClass *class)
{
	GtkObjectClass *object_class;
	
	object_class = (GtkObjectClass *) class;
	parent_class = gtk_type_class (gnome_dialog_get_type ());
	
	object_class->finalize = mail_account_editor_finalise;
	/* override methods */
	
}

static void
mail_account_editor_init (MailAccountEditor *o)
{
	;
}

static void
mail_account_editor_finalise (GtkObject *obj)
{
	MailAccountEditor *editor = (MailAccountEditor *) obj;
	
	gtk_object_unref (GTK_OBJECT (editor->gui));
	
        ((GtkObjectClass *)(parent_class))->finalize (obj);
}

static void
source_auth_type_changed (GtkWidget *widget, gpointer user_data)
{
	MailAccountEditor *editor = user_data;
	CamelServiceAuthType *authtype;
	gboolean sensitive;
	
	authtype = gtk_object_get_data (GTK_OBJECT (widget), "authtype");
	
	gtk_object_set_data (GTK_OBJECT (editor), "source_authmech", authtype->authproto);
	
	if (authtype->need_password)
		sensitive = TRUE;
	else
		sensitive = FALSE;
	
	gtk_widget_set_sensitive (GTK_WIDGET (editor->source_passwd), sensitive);
	gtk_widget_set_sensitive (GTK_WIDGET (editor->save_passwd), sensitive);
}

static void
source_auth_init (MailAccountEditor *editor, CamelURL *url)
{
	GtkWidget *menu, *item, *authmech = NULL;
	CamelServiceAuthType *authtype;
	GList *authtypes = NULL;
	
	menu = gtk_menu_new ();
	gtk_option_menu_set_menu (editor->source_auth, menu);
	
	/* If we can't connect, don't let them continue. */
	if (!check_service (url, CAMEL_PROVIDER_STORE, &authtypes)) {
		return;
	}
	
	if (authtypes) {
		GList *l;
		
		menu = gtk_menu_new ();
		l = authtypes;
		while (l) {
			authtype = l->data;
			
			item = gtk_menu_item_new_with_label (authtype->name);
			gtk_object_set_data (GTK_OBJECT (item), "authtype", authtype);
			gtk_signal_connect (GTK_OBJECT (item), "activate",
					    GTK_SIGNAL_FUNC (source_auth_type_changed),
					    editor);
			
			gtk_menu_append (GTK_MENU (menu), item);
			
			if (!g_strcasecmp (authtype->authproto, url->authmech))
				authmech = item;
		}
		
		if (authmech)
			gtk_signal_emit_by_name (GTK_OBJECT (authmech), "activate", editor);
	}
}

static void
transport_type_changed (GtkWidget *widget, gpointer user_data)
{
	MailAccountEditor *editor = user_data;
	CamelProvider *provider;
	
	provider = gtk_object_get_data (GTK_OBJECT (widget), "provider");
	editor->transport = provider;
	
	/* hostname */
	if (provider->url_flags & CAMEL_URL_ALLOW_HOST)
		gtk_widget_set_sensitive (GTK_WIDGET (editor->transport_host), TRUE);
	else
		gtk_widget_set_sensitive (GTK_WIDGET (editor->transport_host), FALSE);
	
	/* auth */
	if (provider->url_flags & CAMEL_URL_ALLOW_AUTH)
		gtk_widget_set_sensitive (GTK_WIDGET (editor->transport_auth_type), TRUE);
	else
		gtk_widget_set_sensitive (GTK_WIDGET (editor->transport_auth_type), FALSE);
	
	/* FIXME: regen the auth list */
}

static void
transport_type_init (MailAccountEditor *editor, CamelURL *url)
{
	GtkWidget *menu, *xport = NULL;
	GList *providers, *l;
	
	menu = gtk_menu_new ();
	providers = camel_session_list_providers (session, FALSE);
	l = providers;
	while (l) {
		CamelProvider *provider = l->data;
		
		if (strcmp (provider->domain, "mail")) {
			l = l->next;
			continue;
		}
		
		if (provider->object_types[CAMEL_PROVIDER_TRANSPORT]) {
			GtkWidget *item;
			
			item = gtk_menu_item_new_with_label (provider->name);
			gtk_object_set_data (GTK_OBJECT (item), "provider", provider);
			gtk_signal_connect (GTK_OBJECT (item), "activate",
					    GTK_SIGNAL_FUNC (transport_type_changed),
					    editor);
			
			gtk_menu_append (GTK_MENU (menu), item);
			
			if (!g_strcasecmp (provider->protocol, url->protocol))
				xport = item;
		}
		
		l = l->next;
	}
	
	gtk_option_menu_set_menu (editor->transport_type, menu);
	
	if (xport)
		gtk_signal_emit_by_name (GTK_OBJECT (xport), "activate", editor);
}

static void
transport_auth_init (MailAccountEditor *editor, CamelURL *url)
{
	/* FIXME: look through the options and select the prefered authmech */
	;
}

static void
construct (MailAccountEditor *editor, const MailConfigAccount *account)
{
	GladeXML *gui;
	GtkWidget *notebook, *entry;
	CamelURL *url;
	
	gui = glade_xml_new (EVOLUTION_DATA_DIR "/mail-config-druid.glade", "mail-account-editor");
	editor->gui = gui;
	
	/* get our toplevel widget */
	notebook = glade_xml_get_widget (gui, "notebook");
	
	/* reparent */
	gtk_widget_reparent (widget, GTK_WIDGET (editor));
	
	/* give our dialog an OK button and title */
	gnome_dialog_construct (GNOME_DIALOG (editor), _("Evolution Account Editor"),
				GNOME_STOCK_BUTTON_OK, GNOME_STOCK_BUTTON_APPLY,
				GNOME_STOCK_BUTTON_CANCEL);
	
	/* General */
	editor->account_name = GTK_ENTRY (glade_xml_get_widget (gui, "txtAccountName"));
	gtk_entry_set_text (editor->account_name, account->name);
	editor->name = GTK_ENTRY (glade_xml_get_widget (gui, "txtName"));
	gtk_entry_set_text (editor->name, account->id->name);
	editor->email = GTK_ENTRY (glade_xml_get_widget (gui, "txtEMail"));
	gtk_entry_set_text (editor->email, account->id->address);
	editor->reply_to = GTK_ENTRY (glade_xml_get_widget (gui, "txtReplyTo"));
	gtk_entry_set_text (editor->reply_to, account->id->reply_to);
	editor->organization = GTK_ENTRY (glade_xml_get_widget (gui, "txtOrganization"));
	gtk_entry_set_text (editor->organization, account->id->organization);
	editor->signature = GNOME_FILE_ENTRY (glade_xml_get_widget (gui, "fileSignature"));
	entry = gnome_file_entry_gtk_entry (editor->signature);
	gtk_entry_set_text (GTK_ENTRY (entry), account->id->signature);
	
	/* Servers */
	url = camel_url_new (account->source->url, NULL);
	editor->source_type = GTK_ENTRY (glade_xml_get_widget (gui, "txtSourceType"));
	gtk_entry_set_text (editor->source_type, url->protocol);
	editor->source_host = GTK_ENTRY (glade_xml_get_widget (gui, "txtSourceHost"));
	gtk_entry_set_text (editor->source_host, url->host);
	if (url->port) {
		char port[10];
		
		g_snprintf (port, 9, ":%d", port);
		gtk_entry_append_text (editor->source_host, port);
	}
	editor->source_user = GTK_ENTRY (glade_xml_get_widget (gui, "txtSourceUser"));
	gtk_entry_set_text (editor->source_user, url->user);
	editor->source_passwd = GTK_ENTRY (glade_xml_get_widget (gui, "txtSourcePasswd"));
	gtk_entry_set_text (editor->source_passwd, url->passwd);
	editor->save_passwd = GTK_CHECK_BUTTON (glade_xml_get_widget (gui, "chkSavePasswd"));
	gtk_check_button_set_active (GTK_TOGGLE_BUTTON (editor->save_passwd), account->source->save_passwd);
	editor->source_auth = GTK_OPTION_MENU (glade_xml_get_widget (gui, "omenuSourceAuth"));
	editor->source_ssl = GTK_CHECK_BUTTON (glade_xml_get_widget (gui, "chkSourceSSL"));
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (editor->source_ssl), account->source->use_ssl);
	editor->keep_on_server = GTK_CHECK_BUTTON (glade_xml_get_widget (gui, "chkKeepMailOnServer"));
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (editor->keep_on_server), account->source->keep_on_server);
	source_auth_init (editor, url);
	camel_url_free (url);
	
	/* Transport */
	url = camel_url_new (account->transport->url, NULL);
	editor->transport_type = GTK_OPTION_MENU (glade_xml_get_widget (gui, "omenuTransportType"));
	gtk_entry_set_text (editor->transport_type, url->protocol);
	editor->transport_host = GTK_ENTRY (glade_xml_get_widget (gui, "txtTransportHost"));
	gtk_entry_set_text (editor->transport_host, url->host);
	if (url->port) {
		char port[10];
		
		g_snprintf (port, 9, ":%d", port);
		gtk_entry_append_text (editor->transport_host, port);
	}
	editor->transport_auth = GTK_OPTION_MENU (glade_xml_get_widget (gui, "omenuTransportAuth"));
	transport_auth_init (editor);
	editor->transport_ssl = GTK_CHECK_BUTTON (glade_xml_get_widget (gui, "chkTransportSSL"));
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (editor->transport_ssl), account->transport->use_ssl);
	transport_type_init (editor, url);
	transport_auth_init (editor, url);
	camel_url_free (url);
	
	editor->account = account;
}

MailAccountEditor *
mail_account_editor_new (const MailConfigAccount *account)
{
	MailAccountsDialog *new;
	
	new = (MailAccountEditor *) gtk_type_new (mail_account_editor_get_type ());
	construct (new, account);
	
	return new;
}
