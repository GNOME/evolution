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
#include "mail-session.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <camel/camel-url.h>
#include <gal/widgets/e-unicode.h>

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

static gboolean
is_email (const char *address)
{
	const char *at, *hname;
	
	g_return_val_if_fail (address != NULL, FALSE);
	
	at = strchr (address, '@');
	/* make sure we have an '@' and that it's not the first or last char */
	if (!at || at == address || *(at + 1) == '\0')
		return FALSE;
	
	hname = at + 1;
	/* make sure the first and last chars aren't '.' */
	if (*hname == '.' || hname[strlen (hname) - 1] == '.')
		return FALSE;
	
	return strchr (hname, '.') != NULL;
}

/* callbacks */
static void
entry_changed (GtkEntry *entry, gpointer data)
{
	MailAccountEditor *editor = data;
	char *account_name, *name, *address;
	gboolean sensitive;
	
	account_name = gtk_entry_get_text (editor->account_name);
	name = gtk_entry_get_text (editor->name);
	address = gtk_entry_get_text (editor->email);
	
	sensitive = account_name && *account_name && name && *name && is_email (address);
	
	gnome_dialog_set_sensitive (GNOME_DIALOG (editor), 0, sensitive);
	gnome_dialog_set_sensitive (GNOME_DIALOG (editor), 1, sensitive);
}

static gboolean
apply_changes (MailAccountEditor *editor)
{
	MailConfigAccount *account;
	char *host, *pport, *str;
	CamelURL *source_url = NULL, *transport_url;
	gboolean retval = TRUE;
	int port;
	
	account = (MailConfigAccount *) editor->account;
	
	/* account name */
	if (editor->account_name) {
		g_free (account->name);
		account->name = e_utf8_gtk_entry_get_text (editor->account_name);
	}
	
	/* identity info */
	g_free (account->id->name);
	account->id->name = e_utf8_gtk_entry_get_text (editor->name);
	
	g_free (account->id->address);
	account->id->address = e_utf8_gtk_entry_get_text (editor->email);
	
	if (editor->reply_to) {
		g_free (account->id->reply_to);
		account->id->reply_to = e_utf8_gtk_entry_get_text (editor->reply_to);
	}
	
	if (editor->organization) {
		g_free (account->id->organization);
		account->id->organization = e_utf8_gtk_entry_get_text (editor->organization);
	}
	
	if (editor->signature) {
		g_free (account->id->signature);
		account->id->signature = gnome_file_entry_get_full_path (editor->signature, TRUE);
	}
	
	/* source */
	if (account->source->url) {
		source_url = camel_url_new (account->source->url, NULL);
		
		g_free (source_url->user);
		str = gtk_entry_get_text (editor->source_user);
		source_url->user = str && *str ? g_strdup (str) : NULL;
		
		g_free (source_url->passwd);
		str = gtk_entry_get_text (editor->source_passwd);
		source_url->passwd = str && *str ? g_strdup (str) : NULL;
		
		g_free (source_url->authmech);
		str = gtk_object_get_data (GTK_OBJECT (editor), "source_authmech");
		source_url->authmech = str && *str ? g_strdup (str) : NULL;
		
		g_free (source_url->host);
		host = g_strdup (gtk_entry_get_text (editor->source_host));
		if (host && (pport = strchr (host, ':'))) {
			*pport = '\0';
			port = atoi (pport + 1);
		} else {
			port = 0;
		}
		source_url->host = host;
		source_url->port = port;
		
		g_free (source_url->path);
		str = gtk_entry_get_text (editor->source_path);
		source_url->path = str && *str ? g_strdup (str) : NULL;
		
		account->source->save_passwd = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (editor->source_save_passwd));
		account->source->keep_on_server = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (editor->keep_on_server));
		
		account->source->enabled = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (editor->source_enabled));
		account->source->auto_check = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (editor->source_auto_check));
		account->source->auto_check_time = gtk_spin_button_get_value_as_int (editor->source_auto_timeout);
		
		/* set the new source url */
		g_free (account->source->url);
		account->source->url = camel_url_to_string (source_url, FALSE);
	}
	
	/* transport */
	transport_url = g_new0 (CamelURL, 1);
	
	if (editor->transport) {
		transport_url->protocol = g_strdup (editor->transport->protocol);
	} else {
		/* workaround for anna's dialog */
		CamelURL *url;
		
		url = camel_url_new (account->transport->url, NULL);
		transport_url->protocol = g_strdup (url->protocol);
		camel_url_free (url);
	}
	
	str = gtk_object_get_data (GTK_OBJECT (editor), "transport_authmech");
	transport_url->authmech = str && *str ? g_strdup (str) : NULL;
	
	if (transport_url->authmech) {
		str = gtk_entry_get_text (editor->transport_user);
		transport_url->user = str && *str ? g_strdup (str) : NULL;
		
		str = gtk_entry_get_text (editor->transport_passwd);
		transport_url->passwd = str && *str ? g_strdup (str) : NULL;
	}
	
	host = g_strdup (gtk_entry_get_text (editor->transport_host));
	if (host && (pport = strchr (host, ':'))) {
		*pport = '\0';
		port = atoi (pport + 1);
	} else {
		port = 0;
	}
	transport_url->host = host;
	transport_url->port = port;
	
	/* set the new transport url */
	g_free (account->transport->url);
	account->transport->url = camel_url_to_string (transport_url, FALSE);
	
	account->transport->save_passwd = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (editor->transport_save_passwd));
	
	/* check to make sure the source works */
	if (source_url) {
		if (mail_config_check_service (source_url, CAMEL_PROVIDER_STORE, FALSE, NULL)) {
			/* save the password if we were requested to do so */
			if (account->source->save_passwd && source_url->passwd) {
				mail_session_set_password (account->source->url, source_url->passwd);
				mail_session_remember_password (account->source->url);
			}
		} else {
			retval = FALSE;
		}
		camel_url_free (source_url);
	}
	
	/* check to make sure the transport works */
	if (mail_config_check_service (transport_url, CAMEL_PROVIDER_TRANSPORT, FALSE, NULL)) {
		/* save the password if we were requested to do so */
		if (account->transport->save_passwd && transport_url->passwd) {
			mail_session_set_password (account->transport->url, transport_url->passwd);
			mail_session_remember_password (account->transport->url);
		}
	} else {
		retval = FALSE;
	}
	
	camel_url_free (transport_url);
	
	/* save any changes we may have */
	mail_config_write ();
	
	return retval;
}

static void
apply_clicked (GtkWidget *widget, gpointer data)
{
	MailAccountEditor *editor = data;
	
	apply_changes (editor);
}

static void
ok_clicked (GtkWidget *widget, gpointer data)
{
	MailAccountEditor *editor = data;
	
	if (apply_changes (editor)) {
		gtk_widget_destroy (GTK_WIDGET (editor));
	} else {
		GtkWidget *mbox;
		
		mbox = gnome_message_box_new (_("One or more of your servers are not configured correctly.\n"
						"Do you wish to save anyway?"),
					      GNOME_MESSAGE_BOX_WARNING,
					      GNOME_STOCK_BUTTON_YES,
					      GNOME_STOCK_BUTTON_NO, NULL);
		
		gnome_dialog_set_default (GNOME_DIALOG (mbox), 1);
		gtk_widget_grab_focus (GTK_WIDGET (GNOME_DIALOG (mbox)->buttons->data));
		
		gnome_dialog_set_parent (GNOME_DIALOG (mbox), GTK_WINDOW (editor));
		
		if (gnome_dialog_run_and_close (GNOME_DIALOG (mbox)) == 0)
			gtk_widget_destroy (GTK_WIDGET (editor));
	}
}

static void
cancel_clicked (GtkWidget *widget, gpointer data)
{
	MailAccountEditor *editor = data;
	
	gtk_widget_destroy (GTK_WIDGET (editor));
}

static void
source_auth_type_changed (GtkWidget *widget, gpointer user_data)
{
	MailAccountEditor *editor = user_data;
	CamelServiceAuthType *authtype;
	gboolean sensitive;
	GtkWidget *label;
	
	authtype = gtk_object_get_data (GTK_OBJECT (widget), "authtype");
	
	gtk_object_set_data (GTK_OBJECT (editor), "source_authmech", authtype->authproto);
	
	if (authtype->need_password)
		sensitive = TRUE;
	else
		sensitive = FALSE;
	
	label = glade_xml_get_widget (editor->gui, "lblSourcePasswd");
	gtk_widget_set_sensitive (label, sensitive);
	gtk_widget_set_sensitive (GTK_WIDGET (editor->source_passwd), sensitive);
	gtk_widget_set_sensitive (GTK_WIDGET (editor->source_save_passwd), sensitive);
}

static void
source_auth_init (MailAccountEditor *editor, CamelURL *url)
{
	GtkWidget *menu, *item, *authmech = NULL;
	CamelServiceAuthType *authtype;
	GList *authtypes = NULL;
	guint i = 0, history = 0;
	
	menu = gtk_menu_new ();
	gtk_option_menu_remove_menu (editor->source_auth);
	
	if (!url || !mail_config_check_service (url, CAMEL_PROVIDER_STORE, FALSE, &authtypes)) {
		gtk_option_menu_set_menu (editor->source_auth, menu);
		
		return;
	}
	
	if (authtypes) {
		GList *l;
		
		l = authtypes;
		while (l) {
			authtype = l->data;
			
			item = gtk_menu_item_new_with_label (authtype->name);
			gtk_object_set_data (GTK_OBJECT (item), "authtype", authtype);
			gtk_signal_connect (GTK_OBJECT (item), "activate",
					    GTK_SIGNAL_FUNC (source_auth_type_changed),
					    editor);
			
			gtk_menu_append (GTK_MENU (menu), item);
			
			gtk_widget_show (item);
			
			if (!authmech || (url->authmech && !g_strcasecmp (authtype->authproto, url->authmech))) {
				authmech = item;
				history = i;
			}
			
			l = l->next;
			i++;
		}
	}
	
	gtk_option_menu_set_menu (editor->source_auth, menu);
	
	if (authmech) {
		gtk_signal_emit_by_name (GTK_OBJECT (authmech), "activate", editor);
		gtk_option_menu_set_history (editor->source_auth, history);
	}
}

static void
transport_auth_type_changed (GtkWidget *widget, gpointer user_data)
{
	MailAccountEditor *editor = user_data;
	CamelServiceAuthType *authtype;
	GtkWidget *user, *passwd;
	gboolean sensitive;
	
	authtype = gtk_object_get_data (GTK_OBJECT (widget), "authtype");
	
	gtk_object_set_data (GTK_OBJECT (editor), "transport_authmech",
			     authtype ? authtype->authproto : NULL);
	
	if (authtype && authtype->need_password)
		sensitive = TRUE;
	else
		sensitive = FALSE;
	
	user = glade_xml_get_widget (editor->gui, "lblTransportUser");
	passwd = glade_xml_get_widget (editor->gui, "lblTransportPasswd");
	gtk_widget_set_sensitive (user, sensitive);
	gtk_widget_set_sensitive (passwd, sensitive);
	gtk_widget_set_sensitive (GTK_WIDGET (editor->transport_user), sensitive);
	gtk_widget_set_sensitive (GTK_WIDGET (editor->transport_passwd), sensitive);
	gtk_widget_set_sensitive (GTK_WIDGET (editor->transport_save_passwd), sensitive);
}

static void
transport_auth_init (MailAccountEditor *editor, CamelURL *url)
{
	GtkWidget *authmech = NULL;
	GtkWidget *menu, *item;
	CamelServiceAuthType *authtype;
	GList *authtypes = NULL;
	guint i = 0, history = 0;
	
	menu = gtk_menu_new ();
	gtk_option_menu_remove_menu (editor->transport_auth);
	
	if (!url || !mail_config_check_service (url, CAMEL_PROVIDER_TRANSPORT, FALSE, &authtypes)) {
		gtk_option_menu_set_menu (editor->transport_auth, menu);
		
		return;
	}
	
	menu = gtk_menu_new ();
	
	if (CAMEL_PROVIDER_ALLOWS (editor->transport, CAMEL_URL_ALLOW_AUTH) &&
	    !CAMEL_PROVIDER_NEEDS (editor->transport, CAMEL_URL_NEED_AUTH)) {
		/* It allows auth, but doesn't require it so give the user a
		   way to say he doesn't need it */
		item = gtk_menu_item_new_with_label (_("None"));
		gtk_object_set_data (GTK_OBJECT (item), "authtype", NULL);
		gtk_signal_connect (GTK_OBJECT (item), "activate",
				    GTK_SIGNAL_FUNC (transport_auth_type_changed),
				    editor);
		
		gtk_menu_append (GTK_MENU (menu), item);
		
		gtk_widget_show (item);
		
		authmech = item;
		history = i;
		i++;
	}
	
	if (authtypes) {
		GList *l;
		
		l = authtypes;
		while (l) {
			authtype = l->data;
			
			item = gtk_menu_item_new_with_label (authtype->name);
			gtk_object_set_data (GTK_OBJECT (item), "authtype", authtype);
			gtk_signal_connect (GTK_OBJECT (item), "activate",
					    GTK_SIGNAL_FUNC (transport_auth_type_changed),
					    editor);
			
			gtk_menu_append (GTK_MENU (menu), item);
			
			gtk_widget_show (item);
			
			if (!authmech || (url->authmech && !g_strcasecmp (authtype->authproto, url->authmech))) {
				authmech = item;
				history = i;
			}
			
			l = l->next;
			i++;
		}
	}
	
	gtk_option_menu_set_menu (editor->transport_auth, menu);
	
	if (authmech) {
		gtk_signal_emit_by_name (GTK_OBJECT (authmech), "activate", editor);
		gtk_option_menu_set_history (editor->transport_auth, history);
		if (url->authmech) {
			gtk_entry_set_text (editor->transport_user, url->user ? url->user : "");
			gtk_entry_set_text (editor->transport_passwd, url->passwd ? url->passwd : "");
		} else {
			gtk_entry_set_text (editor->transport_user, "");
			gtk_entry_set_text (editor->transport_passwd, "");
		}
	}
}

static void
transport_type_changed (GtkWidget *widget, gpointer user_data)
{
	MailAccountEditor *editor = user_data;
	CamelProvider *provider;
	GtkWidget *label;
	
	provider = gtk_object_get_data (GTK_OBJECT (widget), "provider");
	editor->transport = provider;
	
	/* hostname */
	label = glade_xml_get_widget (editor->gui, "lblTransportHost");
	if (CAMEL_PROVIDER_ALLOWS (provider, CAMEL_URL_PART_HOST)) {
		gtk_widget_set_sensitive (GTK_WIDGET (editor->transport_host), TRUE);
		gtk_widget_set_sensitive (label, TRUE);
	} else {
		gtk_entry_set_text (editor->transport_host, "");
		gtk_widget_set_sensitive (GTK_WIDGET (editor->transport_host), FALSE);
		gtk_widget_set_sensitive (label, FALSE);
	}
	
	/* username */
	label = glade_xml_get_widget (editor->gui, "lblTransportUser");
	if (CAMEL_PROVIDER_ALLOWS (provider, CAMEL_URL_PART_AUTH)) {
		gtk_widget_set_sensitive (GTK_WIDGET (editor->transport_user), TRUE);
		gtk_widget_set_sensitive (label, TRUE);
	} else {
		gtk_entry_set_text (editor->transport_user, "");
		gtk_widget_set_sensitive (GTK_WIDGET (editor->transport_user), FALSE);
		gtk_widget_set_sensitive (label, FALSE);
	}
	
	/* password */
	label = glade_xml_get_widget (editor->gui, "lblTransportPasswd");
	if (CAMEL_PROVIDER_ALLOWS (provider, CAMEL_URL_PART_AUTH)) {
		gtk_widget_set_sensitive (GTK_WIDGET (editor->transport_passwd), TRUE);
		gtk_widget_set_sensitive (GTK_WIDGET (editor->transport_save_passwd), TRUE);
		gtk_widget_set_sensitive (label, TRUE);
	} else {
		gtk_entry_set_text (editor->transport_passwd, "");
		gtk_widget_set_sensitive (GTK_WIDGET (editor->transport_passwd), FALSE);
		gtk_widget_set_sensitive (GTK_WIDGET (editor->transport_save_passwd), FALSE);
		gtk_widget_set_sensitive (label, FALSE);
	}
	
	/* auth */
	label = glade_xml_get_widget (editor->gui, "lblTransportAuth");
	if (CAMEL_PROVIDER_ALLOWS (provider, CAMEL_URL_PART_AUTH)) {
		CamelURL *url;
		char *host;
		
		gtk_widget_set_sensitive (GTK_WIDGET (editor->transport_auth), TRUE);
		gtk_widget_set_sensitive (label, TRUE);
		
		/* regen the auth list */
		url = g_new0 (CamelURL, 1);
		url->protocol = g_strdup (provider->protocol);
		host = gtk_entry_get_text (editor->transport_host);
		if (host && *host)
			url->host = g_strdup (host);
		else
			url->host = g_strdup ("localhost");
		transport_auth_init (editor, url);
		camel_url_free (url);
	} else {
		gtk_widget_set_sensitive (GTK_WIDGET (editor->transport_auth), FALSE);
		gtk_widget_set_sensitive (label, FALSE);
	}
}

static void
transport_type_init (MailAccountEditor *editor, CamelURL *url)
{
	GtkWidget *menu, *xport = NULL;
	GList *providers, *l;
	guint i = 0, history = 0;
	
	menu = gtk_menu_new ();
	gtk_option_menu_remove_menu (GTK_OPTION_MENU (editor->transport_auth));
	
	providers = camel_session_list_providers (session, TRUE);
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
			
			gtk_widget_show (item);
			
			if (!xport && !g_strcasecmp (provider->protocol, url->protocol)) {
				xport = item;
				history = i;
			}
			
			i++;
		}
		
		l = l->next;
	}
	
	gtk_option_menu_set_menu (GTK_OPTION_MENU (editor->transport_type), menu);
	
	if (xport) {
		gtk_signal_emit_by_name (GTK_OBJECT (xport), "activate", editor);
		gtk_option_menu_set_history (GTK_OPTION_MENU (editor->transport_type), history);
	}
}

static void
auto_check_toggled (GtkToggleButton *button, gpointer data)
{
	MailAccountEditor *editor = data;
	
	gtk_widget_set_sensitive (GTK_WIDGET (editor->source_auto_timeout), gtk_toggle_button_get_active (button));
}

static void
source_check (MailAccountEditor *editor, CamelURL *url)
{
	GList *providers, *l;
	
	providers = camel_session_list_providers (session, TRUE);
	l = providers;
	while (l) {
		CamelProvider *provider = l->data;
		
		if (strcmp (provider->domain, "mail")) {
			l = l->next;
			continue;
		}
		
		if (provider->object_types[CAMEL_PROVIDER_STORE] && provider->flags & CAMEL_PROVIDER_IS_SOURCE) {
			if (!url || !g_strcasecmp (provider->protocol, url->protocol)) {
				GtkWidget *label;
				
				/* keep-on-server */
				if (url && !(provider->flags & CAMEL_PROVIDER_IS_STORAGE))
					gtk_widget_set_sensitive (GTK_WIDGET (editor->keep_on_server), TRUE);
				else
					gtk_widget_set_sensitive (GTK_WIDGET (editor->keep_on_server), FALSE);
				
				/* host */
				label = glade_xml_get_widget (editor->gui, "lblSourceHost");
				if (url && CAMEL_PROVIDER_ALLOWS (provider, CAMEL_URL_PART_HOST)) {
					gtk_widget_set_sensitive (GTK_WIDGET (editor->source_host), TRUE);
					gtk_widget_set_sensitive (label, TRUE);
				} else {
					gtk_widget_set_sensitive (GTK_WIDGET (editor->source_host), FALSE);
					gtk_widget_set_sensitive (label, FALSE);
				}
				
				/* user */
				label = glade_xml_get_widget (editor->gui, "lblSourceUser");
				if (url && CAMEL_PROVIDER_ALLOWS (provider, CAMEL_URL_PART_USER)) {
					gtk_widget_set_sensitive (GTK_WIDGET (editor->source_user), TRUE);
					gtk_widget_set_sensitive (label, TRUE);
				} else {
					gtk_widget_set_sensitive (GTK_WIDGET (editor->source_user), FALSE);
					gtk_widget_set_sensitive (label, FALSE);
				}
				
				/* path */
				label = glade_xml_get_widget (editor->gui, "lblSourcePath");
				if (url && CAMEL_PROVIDER_ALLOWS (provider, CAMEL_URL_PART_PATH)) {
					gtk_widget_set_sensitive (GTK_WIDGET (editor->source_path), TRUE);
					gtk_widget_set_sensitive (label, TRUE);
				} else {
					gtk_widget_set_sensitive (GTK_WIDGET (editor->source_path), FALSE);
					gtk_widget_set_sensitive (label, FALSE);
				}
				
				/* auth */
				label = glade_xml_get_widget (editor->gui, "lblSourceAuth");
				if (url && CAMEL_PROVIDER_ALLOWS (provider, CAMEL_URL_PART_AUTH)) {
					gtk_widget_set_sensitive (GTK_WIDGET (editor->source_auth), TRUE);
					gtk_widget_set_sensitive (label, TRUE);
				} else {
					gtk_widget_set_sensitive (GTK_WIDGET (editor->source_auth), FALSE);
					gtk_widget_set_sensitive (label, FALSE);
				}
				
				/* passwd */
				label = glade_xml_get_widget (editor->gui, "lblSourcePasswd");
				if (url && CAMEL_PROVIDER_ALLOWS (provider, CAMEL_URL_PART_PASSWORD)) {
					gtk_widget_set_sensitive (GTK_WIDGET (editor->source_passwd), TRUE);
					gtk_widget_set_sensitive (GTK_WIDGET (editor->source_save_passwd), TRUE);
					gtk_widget_set_sensitive (label, TRUE);
				} else {
					gtk_widget_set_sensitive (GTK_WIDGET (editor->source_passwd), FALSE);
					gtk_widget_set_sensitive (GTK_WIDGET (editor->source_save_passwd), FALSE);
					gtk_widget_set_sensitive (label, FALSE);
				}
				
				break;
			}
		}
		
		l = l->next;
	}
}

static void
construct (MailAccountEditor *editor, const MailConfigAccount *account)
{
	GtkWidget *toplevel, *entry;
	GladeXML *gui;
	CamelURL *url;
	
	gui = glade_xml_new (EVOLUTION_GLADEDIR "/mail-config.glade", "mail-account-editor");
	editor->gui = gui;
	
	/* get our toplevel widget */
	toplevel = glade_xml_get_widget (gui, "toplevel");
	
	/* reparent */
	gtk_widget_reparent (toplevel, GNOME_DIALOG (editor)->vbox);
	
	/* give our dialog an OK button and title */
	gtk_window_set_title (GTK_WINDOW (editor), _("Evolution Account Editor"));
	gtk_window_set_policy (GTK_WINDOW (editor), FALSE, TRUE, TRUE);
	gtk_window_set_modal (GTK_WINDOW (editor), TRUE);
	gnome_dialog_append_buttons (GNOME_DIALOG (editor),
				     GNOME_STOCK_BUTTON_OK,
				     GNOME_STOCK_BUTTON_APPLY,
				     GNOME_STOCK_BUTTON_CANCEL,
				     NULL);
	
	gnome_dialog_button_connect (GNOME_DIALOG (editor), 0 /* OK */,
				     GTK_SIGNAL_FUNC (ok_clicked),
				     editor);
	gnome_dialog_button_connect (GNOME_DIALOG (editor), 1 /* APPLY */,
				     GTK_SIGNAL_FUNC (apply_clicked),
				     editor);
	gnome_dialog_button_connect (GNOME_DIALOG (editor), 2 /* CANCEL */,
				     GTK_SIGNAL_FUNC (cancel_clicked),
				     editor);
	
	/* General */
	editor->account_name = GTK_ENTRY (glade_xml_get_widget (gui, "txtAccountName"));
	e_utf8_gtk_entry_set_text (editor->account_name, account->name ? account->name : _("Unspecified"));
	gtk_signal_connect (GTK_OBJECT (editor->account_name), "changed", entry_changed, editor);
	editor->name = GTK_ENTRY (glade_xml_get_widget (gui, "txtName"));
	e_utf8_gtk_entry_set_text (editor->name, account->id->name ? account->id->name : "");
	gtk_signal_connect (GTK_OBJECT (editor->name), "changed", entry_changed, editor);
	editor->email = GTK_ENTRY (glade_xml_get_widget (gui, "txtAddress"));
	e_utf8_gtk_entry_set_text (editor->email, account->id->address ? account->id->address : "");
	gtk_signal_connect (GTK_OBJECT (editor->email), "changed", entry_changed, editor);
	editor->reply_to = GTK_ENTRY (glade_xml_get_widget (gui, "txtReplyTo"));
	if (editor->reply_to)
		e_utf8_gtk_entry_set_text (editor->reply_to, account->id->reply_to ? account->id->reply_to : "");
	editor->organization = GTK_ENTRY (glade_xml_get_widget (gui, "txtOrganization"));
	if (editor->organization)
		e_utf8_gtk_entry_set_text (editor->organization, account->id->organization ?
					   account->id->organization : "");
	editor->signature = GNOME_FILE_ENTRY (glade_xml_get_widget (gui, "fileSignature"));
	if (editor->signature) {
		entry = gnome_file_entry_gtk_entry (editor->signature);
		gtk_entry_set_text (GTK_ENTRY (entry), account->id->signature ? account->id->signature : "");
	}
	
	/* Servers */
	if (account->source->url)
		url = camel_url_new (account->source->url, NULL);
	else
		url = NULL;
	
	editor->source_type = glade_xml_get_widget (gui, "txtSourceType");
	if (GTK_IS_LABEL (editor->source_type))
		gtk_label_set_text (GTK_LABEL (editor->source_type), url ? url->protocol : _("None"));
	else
		gtk_entry_set_text (GTK_ENTRY (editor->source_type), url ? url->protocol : _("None"));
	editor->source_host = GTK_ENTRY (glade_xml_get_widget (gui, "txtSourceHost"));
	gtk_entry_set_text (editor->source_host, url && url->host ? url->host : "");
	if (url && url->port) {
		char port[10];
		
		g_snprintf (port, 9, ":%d", url->port);
		gtk_entry_append_text (editor->source_host, port);
	}
	editor->source_user = GTK_ENTRY (glade_xml_get_widget (gui, "txtSourceUser"));
	gtk_entry_set_text (editor->source_user, url && url->user ? url->user : "");
	editor->source_passwd = GTK_ENTRY (glade_xml_get_widget (gui, "txtSourcePasswd"));
	gtk_entry_set_text (editor->source_passwd, url && url->passwd ? url->passwd : "");
	editor->source_path = GTK_ENTRY (glade_xml_get_widget (gui, "txtSourcePath"));
	if (url && url->path && *(url->path)) {
		GList *providers;
		CamelProvider *provider = NULL;
		
		providers = camel_session_list_providers (session, TRUE);
		while (providers) {
			provider = providers->data;
			
			if (strcmp (provider->domain, "mail")) {
				provider = NULL;
				providers = providers->next;
				continue;
			}
			
			if (provider->object_types[CAMEL_PROVIDER_STORE] && provider->flags & CAMEL_PROVIDER_IS_SOURCE)
				if (!url || !g_strcasecmp (provider->protocol, url->protocol))
					break;
			
			provider = NULL;
			providers = providers->next;
		}
		
		if (provider) {
			if (provider->url_flags & CAMEL_URL_PATH_IS_ABSOLUTE)
				gtk_entry_set_text (editor->source_path, url->path ? url->path : "");
			else
				gtk_entry_set_text (editor->source_path, url->path + 1 ? url->path + 1 : "");
		} else {
			/* we've got a serious problem if we ever get to here */
			g_assert_not_reached ();
		}
	}
	editor->source_save_passwd = GTK_CHECK_BUTTON (glade_xml_get_widget (gui, "chkSourceSavePasswd"));
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (editor->source_save_passwd), account->source->save_passwd);
	editor->source_auth = GTK_OPTION_MENU (glade_xml_get_widget (gui, "omenuSourceAuth"));
	editor->keep_on_server = GTK_CHECK_BUTTON (glade_xml_get_widget (gui, "chkKeepMailOnServer"));
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (editor->keep_on_server), account->source->keep_on_server);
	editor->source_auto_timeout = GTK_SPIN_BUTTON (glade_xml_get_widget (gui, "spinAutoCheckTimeout"));
	gtk_spin_button_set_value (editor->source_auto_timeout,
				   (gfloat) (account->source->auto_check_time * 1.0));
	editor->source_auto_check = GTK_CHECK_BUTTON (glade_xml_get_widget (gui, "chkAutoCheckMail"));
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (editor->source_auto_check), account->source->auto_check);
	gtk_signal_connect (GTK_OBJECT (editor->source_auto_check), "toggled", auto_check_toggled, editor);
	gtk_widget_set_sensitive (GTK_WIDGET (editor->source_auto_timeout), account->source->auto_check);
	editor->source_enabled = GTK_CHECK_BUTTON (glade_xml_get_widget (gui, "chkEnabled"));
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (editor->source_enabled), account->source->enabled);
	source_check (editor, url);
	source_auth_init (editor, url);
	if (url)
		camel_url_free (url);
	
	/* Transport */
	if (account->transport->url)
		url = camel_url_new (account->transport->url, NULL);
	else
		url = NULL;
	
	editor->transport_type = glade_xml_get_widget (gui, "omenuTransportType");
	editor->transport_host = GTK_ENTRY (glade_xml_get_widget (gui, "txtTransportHost"));
	gtk_entry_set_text (editor->transport_host, url && url->host ? url->host : "");
	if (url && url->port) {
		char port[10];
		
		g_snprintf (port, 9, ":%d", url->port);
		gtk_entry_append_text (editor->transport_host, port);
	}
	editor->transport_auth = GTK_OPTION_MENU (glade_xml_get_widget (gui, "omenuTransportAuth"));
	editor->transport_user = GTK_ENTRY (glade_xml_get_widget (gui, "txtTransportUser"));
	gtk_entry_set_text (editor->transport_user, url && url->user ? url->user : "");
	editor->transport_passwd = GTK_ENTRY (glade_xml_get_widget (gui, "txtTransportPasswd"));
	gtk_entry_set_text (editor->transport_passwd, url && url->passwd ? url->passwd : "");
	editor->transport_save_passwd = GTK_CHECK_BUTTON (glade_xml_get_widget (gui, "chkTransportSavePasswd"));
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (editor->transport_save_passwd), account->transport->save_passwd);
	if (GTK_IS_OPTION_MENU (editor->transport_type))
		transport_type_init (editor, url);
	else
		gtk_label_set_text (GTK_LABEL (editor->transport_type),
				    url && url->protocol ? url->protocol : _("None"));
	
	transport_auth_init (editor, url);
	
	if (url)
		camel_url_free (url);
	
	editor->account = account;
}

MailAccountEditor *
mail_account_editor_new (const MailConfigAccount *account)
{
	MailAccountEditor *new;
	
	new = (MailAccountEditor *) gtk_type_new (mail_account_editor_get_type ());
	construct (new, account);
	
	return new;
}
