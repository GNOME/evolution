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

#include "config.h"

#include <glade/glade.h>
#include <gtkhtml/gtkhtml.h>
#include "mail-config-druid.h"
#include "mail-config.h"
#include "mail-ops.h"
#include "mail.h"
#include "mail-session.h"
#include <sys/types.h>
#include <string.h>
#include <unistd.h>

#define d(x) x

GtkWidget *mail_config_create_html (const char *name, const char *str1, const char *str2,
				    int int1, int int2);

static void mail_config_druid_class_init (MailConfigDruidClass *class);
static void mail_config_druid_init       (MailConfigDruid *druid);
static void mail_config_druid_finalise   (GtkObject *obj);

static void construct_auth_menu (MailConfigDruid *druid, GList *authtypes);

static GtkWindowClass *parent_class;

GtkType
mail_config_druid_get_type (void)
{
	static GtkType type = 0;
	
	if (!type) {
		GtkTypeInfo type_info = {
			"MailConfigDruid",
			sizeof (MailConfigDruid),
			sizeof (MailConfigDruidClass),
			(GtkClassInitFunc) mail_config_druid_class_init,
			(GtkObjectInitFunc) mail_config_druid_init,
			(GtkArgSetFunc) NULL,
			(GtkArgGetFunc) NULL
		};
		
		type = gtk_type_unique (gtk_window_get_type (), &type_info);
	}
	
	return type;
}

static void
mail_config_druid_class_init (MailConfigDruidClass *class)
{
	GtkObjectClass *object_class;
	
	object_class = (GtkObjectClass *) class;
	parent_class = gtk_type_class (gtk_window_get_type ());
	
	object_class->finalize = mail_config_druid_finalise;
	/* override methods */
	
}

static void
mail_config_druid_init (MailConfigDruid *o)
{
	;
}

static void
mail_config_druid_finalise (GtkObject *obj)
{
	MailConfigDruid *druid = (MailConfigDruid *) obj;
	
	gtk_object_unref (GTK_OBJECT (druid->gui));
	
        ((GtkObjectClass *)(parent_class))->finalize (obj);
}


static struct {
	char *name;
	char *text;
} info[] = {
	{ "htmlIdentity",
	  "Please enter your name and email address below. The \"optional\" fields below do not need to be filled in, unless you wish to include this information in email you send." },
	{ "htmlIncoming",
	  "Please enter information about your incoming mail server below. If you don't know what kind of server you use, contact your system administrator or Internet Service Provider." },
	{ "htmlAuthentication",
	  "Your mail server supports the following types of authentication. Please select the type you want Evolution to use." },
	{ "htmlTransport",
	  "Please enter information about your outgoing mail protocol below. If you don't know which protocol you use, contact your system administrator or Internet Service Provider." },
	{ "htmlAccountInfo",
	  "You are almost done with the mail configuration process. The identity, incoming mail server and outgoing mail transport method which you provided will be grouped together to make an Evolution mail account. Please enter a name for this account in the space below. This name will be used for display purposes only." }
};
static int num_info = (sizeof (info) / sizeof (info[0]));

static void
html_size_req (GtkWidget *widget, GtkRequisition *requisition)
{
	 requisition->height = GTK_LAYOUT (widget)->height;
}

GtkWidget *
mail_config_create_html (const char *name, const char *str1, const char *str2,
			 int int1, int int2)
{
	GtkWidget *scrolled, *html;
	GtkHTMLStream *stream;
	GtkStyle *style;
	int i;
	
	html = gtk_html_new ();
	GTK_LAYOUT (html)->height = 0;
	gtk_signal_connect (GTK_OBJECT (html), "size_request",
			    GTK_SIGNAL_FUNC (html_size_req), NULL);
	gtk_html_set_editable (GTK_HTML (html), FALSE);
	style = gtk_rc_get_style (html);
	if (!style)
		style = gtk_widget_get_style (html);
	if (style) {
		gtk_html_set_default_background_color (GTK_HTML (html),
						       &style->bg[0]);
	}
	gtk_widget_show (html);
	
	scrolled = gtk_scrolled_window_new (NULL, NULL);
	gtk_widget_show (scrolled);
	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scrolled),
					GTK_POLICY_NEVER, GTK_POLICY_NEVER);
	gtk_container_add (GTK_CONTAINER (scrolled), html);
	
	for (i = 0; i < num_info; i++) {
		if (!strcmp (name, info[i].name))
			break;
	}
	g_return_val_if_fail (i != num_info, scrolled);
	
	stream = gtk_html_begin (GTK_HTML (html));
	gtk_html_write (GTK_HTML (html), stream, "<html><p>", 9);
	gtk_html_write (GTK_HTML (html), stream, info[i].text,
			strlen (info[i].text));
	gtk_html_write (GTK_HTML (html), stream, "</p></html>", 11);
	gtk_html_end (GTK_HTML (html), stream, GTK_HTML_STREAM_OK);
	
	return scrolled;
}

static void
druid_cancel (GnomeDruid *druid, gpointer user_data)
{
	/* Cancel the setup of the account */
	MailConfigDruid *config = user_data;
	
	gtk_widget_destroy (GTK_WIDGET (config));
}

static void
druid_finish (GnomeDruidPage *page, gpointer arg1, gpointer user_data)
{
	/* Cancel the setup of the account */
	MailConfigDruid *druid = user_data;
	MailConfigAccount *account;
	MailConfigIdentity *id;
	MailConfigService *source;
	MailConfigService *transport;
	CamelURL *url;
	GSList *mini;
	char *str;
	
	account = g_new0 (MailConfigAccount, 1);
	account->name = mail_config_druid_get_account_name (druid);
	account->default_account = mail_config_druid_get_default_account (druid);
	
	/* construct the identity */
	id = g_new0 (MailConfigIdentity, 1);
	id->name = mail_config_druid_get_full_name (druid);
	id->address = mail_config_druid_get_email_address (druid);
	id->organization = mail_config_druid_get_organization (druid);
	id->signature = mail_config_druid_get_sigfile (druid);
	
	/* construct the source */
	source = g_new0 (MailConfigService, 1);
	source->keep_on_server = mail_config_druid_get_keep_mail_on_server (druid);
	source->save_passwd = mail_config_druid_get_save_password (druid);
	str = mail_config_druid_get_source_url (druid);
	if (str) {
		/* cache the password and rewrite the url without the password part */
		url = camel_url_new (str, NULL);
		g_free (str);
		source->url = camel_url_to_string (url, FALSE);
		if (source->save_passwd && url->passwd) {
			mail_session_set_password (source->url, url->passwd);
			mail_session_remember_password (source->url);
		}
		camel_url_free (url);
	} else {
		source->url = NULL;
	}
	
	/* construct the transport */
	transport = g_new0 (MailConfigService, 1);
	transport->url = mail_config_druid_get_transport_url (druid);
	
	account->id = id;
	account->source = source;
	account->transport = transport;
	
	mail_config_add_account (account);
	mail_config_write ();
	
	mini = g_slist_prepend (NULL, account);
	mail_load_storages (druid->shell, mini, TRUE);
	g_slist_free (mini);
	
	gtk_widget_destroy (GTK_WIDGET (druid));
}

/* Identity Page */
static void
identity_check (MailConfigDruid *druid)
{
	if (gtk_entry_get_text (druid->full_name) &&
	    gtk_entry_get_text (druid->email_address))
		gnome_druid_set_buttons_sensitive (druid->druid, TRUE, TRUE, TRUE);
	else
		gnome_druid_set_buttons_sensitive (druid->druid, TRUE, FALSE, TRUE);
}

static void
identity_changed (GtkWidget *widget, gpointer data)
{
	MailConfigDruid *druid = data;
	
	identity_check (druid);
}

static void
identity_prepare (GnomeDruidPage *page, GnomeDruid *druid, gpointer data)
{
	MailConfigDruid *config = data;
	
	gtk_widget_grab_focus (GTK_WIDGET (config->full_name));
	
	identity_check (config);
}

static gboolean
identity_next (GnomeDruidPage *page, GnomeDruid *druid, gpointer data)
{
	/* go to the next page */
	return FALSE;
}

/* Incoming mail Page */
static void
incoming_check (MailConfigDruid *druid)
{
	const CamelProvider *prov = druid->source_provider;
	gboolean host = TRUE, user = TRUE, path = TRUE;
	gboolean next_sensitive = TRUE;
	
	if (prov && prov->url_flags & CAMEL_URL_NEED_HOST)
		host = gtk_entry_get_text (druid->incoming_hostname) != NULL;
	
	if (prov && prov->url_flags & CAMEL_URL_NEED_USER)
		user = gtk_entry_get_text (druid->incoming_username) != NULL;
	
	if (prov && prov->url_flags & CAMEL_URL_NEED_PATH)
		path = gtk_entry_get_text (druid->incoming_path) != NULL;
	
	next_sensitive = host && user && path;
	
	gnome_druid_set_buttons_sensitive (druid->druid, TRUE, next_sensitive, TRUE);
}

static void
incoming_changed (GtkWidget *widget, gpointer data)
{
	MailConfigDruid *druid = data;
	
	incoming_check (druid);
}

static void
incoming_prepare (GnomeDruidPage *page, GnomeDruid *druid, gpointer data)
{
	MailConfigDruid *config = data;
	
	if (!gtk_object_get_data (GTK_OBJECT (page), "initialized")) {
		char *username;
		
		/* Copy the username part of the email address into
		 * the Username field.
		 */
		username = gtk_entry_get_text (config->email_address);
		username = g_strndup (username, strcspn (username, "@"));
		gtk_entry_set_text (config->incoming_username, username);
		g_free (username);
		
		gtk_object_set_data (GTK_OBJECT (page), "initialized",
				     GINT_TO_POINTER (1));
	}
	
	incoming_check (config);
}

static gboolean
incoming_next (GnomeDruidPage *page, GnomeDruid *druid, gpointer data)
{
	MailConfigDruid *config = data;
	GtkWidget *transport_page;
	GList *authtypes = NULL;
	gchar *source_url;
	CamelURL *url;
	
	config->have_auth_page = TRUE;
	
	source_url = mail_config_druid_get_source_url (config);
	if (!source_url) {
		/* User opted to not setup a source for this account,
		 * so jump past the auth page */
		
		/* Skip to transport page. */
		config->have_auth_page = FALSE;
		transport_page = glade_xml_get_widget (config->gui, "druidTransportPage");
		gnome_druid_set_page (config->druid, GNOME_DRUID_PAGE (transport_page));
		
		return TRUE;
	}
	
	url = camel_url_new (source_url, NULL);
	g_free (source_url);
	
	/* If we can't connect, warn them and continue on to the Transport page. */
	if (!mail_config_check_service (url, CAMEL_PROVIDER_STORE, &authtypes)) {
		GtkWidget *dialog;
		char *source, *warning;
		
		source = camel_url_to_string (url, FALSE);
		camel_url_free (url);
		
		warning = g_strdup_printf (_("Failed to verify the incoming mail configuration.\n"
					     "You may experience problems retrieving your mail from %s"),
					   source);
		g_free (source);
		dialog = gnome_warning_dialog_parented (warning, GTK_WINDOW (config));
		g_free (warning);
		
		gnome_dialog_run_and_close (GNOME_DIALOG (dialog));
		
		/* Skip to transport page. */
		config->have_auth_page = FALSE;
		transport_page = glade_xml_get_widget (config->gui, "druidTransportPage");
		gnome_druid_set_page (config->druid, GNOME_DRUID_PAGE (transport_page));
		
		return TRUE;
	}
	camel_url_free (url);
	
	/* If this service offers authentication, go to the next page. */
	if (authtypes) {
		construct_auth_menu (config, authtypes);
		return FALSE;
	}
	
	/* Otherwise, skip to transport page. */
	config->have_auth_page = FALSE;
	transport_page = glade_xml_get_widget (config->gui, "druidTransportPage");
	gnome_druid_set_page (config->druid, GNOME_DRUID_PAGE (transport_page));
	
	return TRUE;
}

static void
incoming_type_changed (GtkWidget *widget, gpointer user_data)
{
	MailConfigDruid *druid = user_data;
	GtkWidget *label, *dwidget = NULL;
	CamelProvider *provider;
	
	provider = gtk_object_get_data (GTK_OBJECT (widget), "provider");
	
	druid->source_provider = provider;
	
	/* hostname */
	label = glade_xml_get_widget (druid->gui, "lblSourceHost");
	if (provider && provider->url_flags & CAMEL_URL_ALLOW_HOST) {
		dwidget = GTK_WIDGET (druid->incoming_hostname);
		gtk_widget_set_sensitive (GTK_WIDGET (druid->incoming_hostname), TRUE);
		gtk_widget_set_sensitive (label, TRUE);
	} else {
		gtk_entry_set_text (druid->incoming_hostname, "");
		gtk_widget_set_sensitive (GTK_WIDGET (druid->incoming_hostname), FALSE);
		gtk_widget_set_sensitive (label, FALSE);
	}
	
	/* username */
	label = glade_xml_get_widget (druid->gui, "lblSourceUser");
	if (provider && provider->url_flags & CAMEL_URL_ALLOW_USER) {
		if (!dwidget)
			dwidget = GTK_WIDGET (druid->incoming_username);
		gtk_widget_set_sensitive (GTK_WIDGET (druid->incoming_username), TRUE);
		gtk_widget_set_sensitive (label, TRUE);
	} else {
		gtk_entry_set_text (druid->incoming_username, "");
		gtk_widget_set_sensitive (GTK_WIDGET (druid->incoming_username), FALSE);
		gtk_widget_set_sensitive (label, FALSE);
	}
	
	/* password */
	label = glade_xml_get_widget (druid->gui, "lblSourcePasswd");
	if (provider && provider->url_flags & CAMEL_URL_ALLOW_PASSWORD) {
		if (!dwidget)
			dwidget = GTK_WIDGET (druid->password);
		gtk_widget_set_sensitive (GTK_WIDGET (druid->password), TRUE);
		gtk_widget_set_sensitive (label, TRUE);
	} else {
		gtk_entry_set_text (druid->password, "");
		gtk_widget_set_sensitive (GTK_WIDGET (druid->password), FALSE);
		gtk_widget_set_sensitive (label, FALSE);
	}
	
	/* auth */
	label = glade_xml_get_widget (druid->gui, "lblSourceAuth");
	if (provider && provider->url_flags & CAMEL_URL_ALLOW_AUTH) {
		gtk_widget_set_sensitive (GTK_WIDGET (druid->auth_type), TRUE);
		gtk_widget_set_sensitive (label, TRUE);
	} else {
		gtk_widget_set_sensitive (GTK_WIDGET (druid->auth_type), FALSE);
		gtk_widget_set_sensitive (label, FALSE);
	}
	
	/* path */
	label = glade_xml_get_widget (druid->gui, "lblSourcePath");
	/* FIXME */
	if (provider && !strcmp (provider->protocol, "imap"))
		gtk_label_set_text (GTK_LABEL (label), _("Namespace:"));
	else
		gtk_label_set_text (GTK_LABEL (label), _("Path:"));
	if (provider && provider->url_flags & CAMEL_URL_ALLOW_PATH) {
		if (!dwidget)
			dwidget = GTK_WIDGET (druid->incoming_path);
		
		if (!strcmp (provider->protocol, "mbox")) {
			char *path;
			
			if (getenv ("MAIL"))
				path = g_strdup (getenv ("MAIL"));
			else
				path = g_strdup_printf (SYSTEM_MAIL_DIR "/%s", g_get_user_name ());
			gtk_entry_set_text (druid->incoming_path, path);
			g_free (path);
		} else if (!strcmp (provider->protocol, "maildir") &&
			   getenv ("MAILDIR")) {
			gtk_entry_set_text (druid->incoming_path, getenv ("MAILDIR"));
		} else {
			gtk_entry_set_text (druid->incoming_path, "");
		}
		
		gtk_widget_set_sensitive (GTK_WIDGET (druid->incoming_path), TRUE);
		gtk_widget_set_sensitive (label, TRUE);
	} else {
		gtk_entry_set_text (druid->incoming_path, "");
		gtk_widget_set_sensitive (GTK_WIDGET (druid->incoming_path), FALSE);
		gtk_widget_set_sensitive (label, FALSE);
	}
	
	/* keep mail on server */
	if (provider && !(provider->flags & CAMEL_PROVIDER_IS_STORAGE))
		gtk_widget_set_sensitive (GTK_WIDGET (druid->incoming_keep_mail), TRUE);
	else
		gtk_widget_set_sensitive (GTK_WIDGET (druid->incoming_keep_mail), FALSE);
	
	incoming_check (druid);
	
	if (dwidget)
		gtk_widget_grab_focus (dwidget);
}

/* Authentication Page */
static void
authentication_check (MailConfigDruid *druid)
{
	if (mail_config_druid_get_save_password (druid)) {
		char *passwd = gtk_entry_get_text (druid->password);
		
		if (passwd && *passwd)
			gnome_druid_set_buttons_sensitive (druid->druid, TRUE, TRUE, TRUE);
		else
			gnome_druid_set_buttons_sensitive (druid->druid, TRUE, FALSE, TRUE);
	} else {
		gnome_druid_set_buttons_sensitive (druid->druid, TRUE, TRUE, TRUE);
	}
}

static void
authentication_changed (GtkWidget *widget, gpointer data)
{
	MailConfigDruid *druid = data;
	
	authentication_check (druid);
}

static void
authentication_prepare (GnomeDruidPage *page, GnomeDruid *druid, gpointer data)
{
	MailConfigDruid *config = data;
	
	authentication_check (config);
}

static gboolean
authentication_next (GnomeDruidPage *page, GnomeDruid *druid, gpointer data)
{
	/* go to the next page */
	return FALSE;
}

static void
auth_type_changed (GtkWidget *widget, gpointer user_data)
{
	MailConfigDruid *druid = user_data;
	CamelServiceAuthType *authtype;
	GtkWidget *label;
	gboolean sensitive;
	
	authtype = gtk_object_get_data (GTK_OBJECT (widget), "authtype");
	
	gtk_object_set_data (GTK_OBJECT (druid), "source_authmech", authtype->authproto);
	
	if (authtype->need_password)
		sensitive = TRUE;
	else
		sensitive = FALSE;
	
	label = glade_xml_get_widget (druid->gui, "lblSourcePasswd");
	gtk_widget_set_sensitive (GTK_WIDGET (druid->password), sensitive);
	gtk_widget_set_sensitive (GTK_WIDGET (druid->save_password), sensitive);
	gtk_widget_set_sensitive (label, sensitive);
	
	if (!sensitive)
		gtk_entry_set_text (druid->password, "");
	
	authentication_check (druid);
}

static void
construct_auth_menu (MailConfigDruid *druid, GList *authtypes)
{
	GtkWidget *menu, *item, *first = NULL;
	CamelServiceAuthType *authtype;
	GList *l;
	
	menu = gtk_menu_new ();
	
	l = authtypes;
	while (l) {
		authtype = l->data;
		
		item = gtk_menu_item_new_with_label (authtype->name);
		gtk_object_set_data (GTK_OBJECT (item), "authtype", authtype);
		gtk_signal_connect (GTK_OBJECT (item), "activate",
				    GTK_SIGNAL_FUNC (auth_type_changed),
				    druid);
		
		gtk_menu_append (GTK_MENU (menu), item);
		
		gtk_widget_show (item);
		
		if (!first)
			first = item;
		
		l = l->next;
	}
	
	if (first)
		gtk_signal_emit_by_name (GTK_OBJECT (first), "activate", druid);
	
	gtk_option_menu_remove_menu (druid->auth_type);
	gtk_option_menu_set_menu (druid->auth_type, menu);
}

/* Transport Page */
static void
transport_check (MailConfigDruid *druid)
{
	const CamelProvider *prov = druid->transport_provider;
	gboolean next_sensitive = TRUE;
	
	if (prov && prov->url_flags & CAMEL_URL_NEED_HOST)
		next_sensitive = gtk_entry_get_text (druid->outgoing_hostname) != NULL;
	
	gnome_druid_set_buttons_sensitive (druid->druid, TRUE, next_sensitive, TRUE);
}

static void
transport_prepare (GnomeDruidPage *page, GnomeDruid *druid, gpointer data)
{
	MailConfigDruid *config = data;
	
	transport_check (config);
}

static gboolean
transport_next (GnomeDruidPage *page, GnomeDruid *druid, gpointer data)
{
	MailConfigDruid *config = data;
	gchar *xport_url;
	CamelURL *url;
	
	xport_url = mail_config_druid_get_transport_url (config);
	url = camel_url_new (xport_url, NULL);
	g_free (xport_url);
	
	/* If we can't connect, don't let them continue. */
	if (!mail_config_check_service (url, CAMEL_PROVIDER_TRANSPORT, NULL)) {
		GtkWidget *dialog;
		char *transport, *warning;
		
		transport = camel_url_to_string (url, FALSE);
		
		warning = g_strdup_printf (_("Failed to verify the outgoing mail configuration.\n"
					     "You may experience problems sending your mail using %s"),
					   transport);
		g_free (transport);
		dialog = gnome_warning_dialog_parented (warning, GTK_WINDOW (config));
		g_free (warning);
		
		gnome_dialog_run_and_close (GNOME_DIALOG (dialog));
	}
	
	camel_url_free (url);
	
	return FALSE;
}

static gboolean
transport_back (GnomeDruidPage *page, GnomeDruid *druid, gpointer data)
{
	MailConfigDruid *config = data;
	
	if (config->have_auth_page) {
		return FALSE;
	} else {
		/* jump to the source page, skipping over the auth page */
		GtkWidget *widget;
		
		widget = glade_xml_get_widget (config->gui, "druidSourcePage");
		gnome_druid_set_page (config->druid, GNOME_DRUID_PAGE (widget));
		
		return TRUE;
	}
}

static void
transport_changed (GtkWidget *widget, gpointer data)
{
	MailConfigDruid *druid = data;
	
	transport_check (druid);
}

static void
transport_type_changed (GtkWidget *widget, gpointer user_data)
{
	MailConfigDruid *druid = user_data;
	CamelProvider *provider;
	GtkWidget *label;
	
	provider = gtk_object_get_data (GTK_OBJECT (widget), "provider");
	druid->transport_provider = provider;
	
	/* hostname */
	label = glade_xml_get_widget (druid->gui, "lblTransportHost");
	if (provider->url_flags & CAMEL_URL_ALLOW_HOST) {
		gtk_widget_grab_focus (GTK_WIDGET (druid->outgoing_hostname));
		gtk_widget_set_sensitive (GTK_WIDGET (druid->outgoing_hostname), TRUE);
		gtk_widget_set_sensitive (label, TRUE);
	} else {
		gtk_entry_set_text (druid->outgoing_hostname, "");
		gtk_widget_set_sensitive (GTK_WIDGET (druid->outgoing_hostname), FALSE);
		gtk_widget_set_sensitive (label, FALSE);
	}
	
	/* auth */
	if (provider->url_flags & CAMEL_URL_ALLOW_AUTH)
		gtk_widget_set_sensitive (GTK_WIDGET (druid->outgoing_requires_auth), TRUE);
	else
		gtk_widget_set_sensitive (GTK_WIDGET (druid->outgoing_requires_auth), FALSE);
	
	transport_check (druid);
}

/* Management page */
static void
management_check (MailConfigDruid *druid)
{
	gboolean next_sensitive;
	
	next_sensitive = gtk_entry_get_text (druid->account_name) != NULL;
	
	gnome_druid_set_buttons_sensitive (druid->druid, TRUE, next_sensitive, TRUE);
}

static void
management_prepare (GnomeDruidPage *page, GnomeDruid *druid, gpointer data)
{
	MailConfigDruid *config = data;
	const char *name;
	
	name = gtk_entry_get_text (config->email_address);
	if (name)
		gtk_entry_set_text (config->account_name, name);
	
	management_check (config);
}

static void
management_changed (GtkWidget *widget, gpointer data)
{
	MailConfigDruid *druid = data;
	
	management_check (druid);
}

static gboolean
management_next (GnomeDruidPage *page, GnomeDruid *druid, gpointer data)
{
	return FALSE;
}

static gint
provider_compare (const CamelProvider *p1, const CamelProvider *p2)
{
	/* sort providers based on "location" (ie. local or remote) */	
	if (p1->flags & CAMEL_PROVIDER_IS_REMOTE) {
		if (p2->flags & CAMEL_PROVIDER_IS_REMOTE)
			return 0;
		return -1;
	} else {
		if (p2->flags & CAMEL_PROVIDER_IS_REMOTE)
			return 1;
		return 0;
	}
}

static void
set_defaults (MailConfigDruid *druid)
{
	const MailConfigService *default_xport;
	GtkWidget *stores, *transports, *item;
	GtkWidget *fstore = NULL, *ftransport = NULL;
	int si = 0, hstore = 0, ti = 0, htransport = 0;
	char *user, *realname;
	char hostname[1024];
	char domain[1024];
	CamelURL *url;
	GList *l;
	
	/* set the default Name field */
	realname = g_get_real_name ();
	if (realname)
		gtk_entry_set_text (druid->full_name, realname);
	
	/* set the default E-Mail Address field */
	user = getenv ("USER");
	if (user && !gethostname (hostname, 1023)) {
		char *address;
		
		memset (domain, 0, sizeof (domain));
		getdomainname (domain, 1023);
		
		address = g_strdup_printf ("%s@%s%s%s", user, hostname, domain && *domain ? "." : "",
					   domain && *domain ? domain : "");
		
		gtk_entry_set_text (druid->email_address, address);
		g_free (address);
	}
	
	/* construct incoming/outgoing option menus */
	stores = gtk_menu_new ();
	transports = gtk_menu_new ();
	druid->providers = camel_session_list_providers (session, TRUE);
	
	/* get the default transport */
	default_xport = mail_config_get_default_transport ();
	if (default_xport && default_xport->url)
		url = camel_url_new (default_xport->url, NULL);
	else
		url = NULL;
	
	/* sort the providers, remote first */
	druid->providers = g_list_sort (druid->providers, (GCompareFunc) provider_compare);
	
	l = druid->providers;
	while (l) {
		CamelProvider *provider = l->data;
		
		if (strcmp (provider->domain, "mail")) {
			l = l->next;
			continue;
		}
		
		if (provider->object_types[CAMEL_PROVIDER_STORE] && provider->flags & CAMEL_PROVIDER_IS_SOURCE) {
			item = gtk_menu_item_new_with_label (provider->name);
			gtk_object_set_data (GTK_OBJECT (item), "provider", provider);
			gtk_signal_connect (GTK_OBJECT (item), "activate",
					    GTK_SIGNAL_FUNC (incoming_type_changed),
					    druid);
			
			gtk_menu_append (GTK_MENU (stores), item);
			
			gtk_widget_show (item);
			
			if (!fstore) {
				fstore = item;
				hstore = si;
			}
			
			si++;
		}
		
		if (provider->object_types[CAMEL_PROVIDER_TRANSPORT]) {
			item = gtk_menu_item_new_with_label (provider->name);
			gtk_object_set_data (GTK_OBJECT (item), "provider", provider);
			gtk_signal_connect (GTK_OBJECT (item), "activate",
					    GTK_SIGNAL_FUNC (transport_type_changed),
					    druid);
			
			gtk_menu_append (GTK_MENU (transports), item);
			
			gtk_widget_show (item);
			
			if (!ftransport) {
				ftransport = item;
				htransport = ti;
			}
			
			if (url && !g_strcasecmp (provider->protocol, url->protocol)) {
				ftransport = item;
				htransport = ti;
			}
			
			ti++;
		}
		
		l = l->next;
	}
	
	/* add a "None" option to the stores menu */
	item = gtk_menu_item_new_with_label (_("None"));
	gtk_object_set_data (GTK_OBJECT (item), "provider", NULL);
	gtk_signal_connect (GTK_OBJECT (item), "activate",
			    GTK_SIGNAL_FUNC (incoming_type_changed),
			    druid);
	
	gtk_menu_append (GTK_MENU (stores), item);
	
	gtk_widget_show (item);
	
	if (!fstore) {
		fstore = item;
		hstore = si;
	}
	
	/* set the menus on the optionmenus */
	gtk_option_menu_remove_menu (druid->incoming_type);
	gtk_option_menu_set_menu (druid->incoming_type, stores);
	gtk_option_menu_set_history (druid->incoming_type, hstore);
	
	gtk_option_menu_remove_menu (druid->outgoing_type);
	gtk_option_menu_set_menu (druid->outgoing_type, transports);
	gtk_option_menu_set_history (druid->outgoing_type, htransport);
	
	if (fstore)
		gtk_signal_emit_by_name (GTK_OBJECT (fstore), "activate", druid);
	
	if (ftransport)
		gtk_signal_emit_by_name (GTK_OBJECT (ftransport), "activate", druid);
	
	if (url) {
		if (url->host) {
			char *hostname;
			
			if (url->port)
				hostname = g_strdup_printf ("%s:%d", url->host, url->port);
			else
				hostname = g_strdup (url->host);
			
			gtk_entry_set_text (druid->outgoing_hostname, hostname);
			g_free (hostname);
		}
		camel_url_free (url);
	}
}

static gboolean
start_next (GnomeDruidPage *page, GnomeDruid *druid, gpointer data)
{
	return FALSE;
}

static struct {
	char *name;
	GtkSignalFunc next_func;
	GtkSignalFunc prepare_func;
	GtkSignalFunc back_func;
	GtkSignalFunc finish_func;
} pages[] = {
	{ "druidStartPage",
	  GTK_SIGNAL_FUNC (start_next),
	  GTK_SIGNAL_FUNC (NULL),
	  GTK_SIGNAL_FUNC (NULL),
	  GTK_SIGNAL_FUNC (NULL) },
	{ "druidIdentityPage",
	  GTK_SIGNAL_FUNC (identity_next),
	  GTK_SIGNAL_FUNC (identity_prepare),
	  GTK_SIGNAL_FUNC (NULL),
	  GTK_SIGNAL_FUNC (NULL) },
	{ "druidSourcePage",
	  GTK_SIGNAL_FUNC (incoming_next),
	  GTK_SIGNAL_FUNC (incoming_prepare),
	  GTK_SIGNAL_FUNC (NULL),
	  GTK_SIGNAL_FUNC (NULL) },
	{ "druidAuthPage",
	  GTK_SIGNAL_FUNC (authentication_next),
	  GTK_SIGNAL_FUNC (authentication_prepare),
	  GTK_SIGNAL_FUNC (NULL),
	  GTK_SIGNAL_FUNC (NULL) },
	{ "druidTransportPage",
	  GTK_SIGNAL_FUNC (transport_next),
	  GTK_SIGNAL_FUNC (transport_prepare),
	  GTK_SIGNAL_FUNC (transport_back),
	  GTK_SIGNAL_FUNC (NULL) },
	{ "druidManagementPage",
	  GTK_SIGNAL_FUNC (management_next),
	  GTK_SIGNAL_FUNC (management_prepare),
	  GTK_SIGNAL_FUNC (NULL),
	  GTK_SIGNAL_FUNC (NULL) },
	{ "druidFinishPage",
	  GTK_SIGNAL_FUNC (NULL),
	  GTK_SIGNAL_FUNC (NULL),
	  GTK_SIGNAL_FUNC (NULL),
	  GTK_SIGNAL_FUNC (druid_finish) },
	{ NULL,
	  GTK_SIGNAL_FUNC (NULL),
	  GTK_SIGNAL_FUNC (NULL),
	  GTK_SIGNAL_FUNC (NULL),
	  GTK_SIGNAL_FUNC (NULL) }
};

static void
construct (MailConfigDruid *druid)
{
	GladeXML *gui;
	GtkWidget *widget;
	int i;
	
	gui = glade_xml_new (EVOLUTION_GLADEDIR "/mail-config.glade", "mail-config-druid");
	druid->gui = gui;
	
	/* get our toplevel widget */
	widget = glade_xml_get_widget (gui, "druid");
	
	/* reparent */
	gtk_widget_reparent (widget, GTK_WIDGET (druid));
	
	druid->druid = GNOME_DRUID (widget);
	
	/* set window title */
	gtk_window_set_title (GTK_WINDOW (druid), _("Evolution Account Wizard"));
	gtk_window_set_policy (GTK_WINDOW (druid), FALSE, TRUE, TRUE);
	gtk_window_set_modal (GTK_WINDOW (druid), TRUE);
	gtk_object_set (GTK_OBJECT (druid), "type", GTK_WINDOW_DIALOG, NULL);
	
	/* attach to druid page signals */
	i = 0;
	while (pages[i].name) {
		GnomeDruidPage *page;
		
		page = GNOME_DRUID_PAGE (glade_xml_get_widget (gui, pages[i].name));
		
		if (pages[i].next_func)
			gtk_signal_connect (GTK_OBJECT (page), "next",
					    pages[i].next_func, druid);
		if (pages[i].prepare_func)
			gtk_signal_connect (GTK_OBJECT (page), "prepare",
					    pages[i].prepare_func, druid);
		if (pages[i].back_func)
			gtk_signal_connect (GTK_OBJECT (page), "back",
					    pages[i].back_func, druid);
		if (pages[i].finish_func)
			gtk_signal_connect (GTK_OBJECT (page), "finish",
					    pages[i].finish_func, druid);
		
		i++;
	}
	gtk_signal_connect (GTK_OBJECT (druid->druid), "cancel",
			    druid_cancel, druid);
	
	/* get our cared-about widgets */
	druid->account_text = glade_xml_get_widget (gui, "htmlAccountInfo");
	druid->account_name = GTK_ENTRY (glade_xml_get_widget (gui, "txtAccountName"));
	gtk_signal_connect (GTK_OBJECT (druid->account_name), "changed", management_changed, druid);
	druid->default_account = GTK_CHECK_BUTTON (glade_xml_get_widget (gui, "chkAccountDefault"));
	
	druid->identity_text = glade_xml_get_widget (gui, "htmlIdentity");
	druid->full_name = GTK_ENTRY (glade_xml_get_widget (gui, "txtFullName"));
	gtk_signal_connect (GTK_OBJECT (druid->full_name), "changed", identity_changed, druid);
	druid->email_address = GTK_ENTRY (glade_xml_get_widget (gui, "txtAddress"));
	gtk_signal_connect (GTK_OBJECT (druid->email_address), "changed", identity_changed, druid);
	druid->organization = GTK_ENTRY (glade_xml_get_widget (gui, "txtOrganization"));
	druid->signature = GNOME_FILE_ENTRY (glade_xml_get_widget (gui, "fileSignature"));
	
	druid->incoming_text = glade_xml_get_widget (gui, "htmlIncoming");
	druid->incoming_type = GTK_OPTION_MENU (glade_xml_get_widget (gui, "omenuIncomingType"));
	druid->incoming_hostname = GTK_ENTRY (glade_xml_get_widget (gui, "txtIncomingHostname"));
	gtk_signal_connect (GTK_OBJECT (druid->incoming_hostname), "changed", incoming_changed, druid);
	druid->incoming_username = GTK_ENTRY (glade_xml_get_widget (gui, "txtIncomingUsername"));
	gtk_signal_connect (GTK_OBJECT (druid->incoming_username), "changed", incoming_changed, druid);
	druid->incoming_path = GTK_ENTRY (glade_xml_get_widget (gui, "txtIncomingPath"));
	gtk_signal_connect (GTK_OBJECT (druid->incoming_path), "changed", incoming_changed, druid);
	druid->incoming_keep_mail = GTK_CHECK_BUTTON (glade_xml_get_widget (gui, "chkIncomingKeepMail"));
	
	druid->have_auth_page = TRUE;
	druid->auth_text = glade_xml_get_widget (gui, "htmlAuthentication");
	druid->auth_type = GTK_OPTION_MENU (glade_xml_get_widget (gui, "omenuAuthType"));
	druid->password = GTK_ENTRY (glade_xml_get_widget (gui, "txtAuthPasswd"));
	gtk_signal_connect (GTK_OBJECT (druid->password), "changed", authentication_changed, druid);
	druid->save_password = GTK_CHECK_BUTTON (glade_xml_get_widget (gui, "chkAuthSavePasswd"));
	gtk_signal_connect (GTK_OBJECT (druid->save_password), "toggled", authentication_changed, druid);
	
	druid->outgoing_text = glade_xml_get_widget (gui, "htmlTransport");
	druid->outgoing_type = GTK_OPTION_MENU (glade_xml_get_widget (gui, "omenuTransportType"));
	druid->outgoing_hostname = GTK_ENTRY (glade_xml_get_widget (gui, "txtTransportHostname"));
	gtk_signal_connect (GTK_OBJECT (druid->outgoing_hostname), "changed", transport_changed, druid);
	druid->outgoing_requires_auth = GTK_CHECK_BUTTON (glade_xml_get_widget (gui, "chkTransportNeedsAuth"));
	
	set_defaults (druid);
	
	gnome_druid_set_buttons_sensitive (druid->druid, FALSE, TRUE, TRUE);
}

MailConfigDruid *
mail_config_druid_new (GNOME_Evolution_Shell shell)
{
	MailConfigDruid *new;
	
	new = (MailConfigDruid *) gtk_type_new (mail_config_druid_get_type ());
	construct (new);
	new->shell = shell;
	
	return new;
}

char *
mail_config_druid_get_account_name (MailConfigDruid *druid)
{
	g_return_val_if_fail (IS_MAIL_CONFIG_DRUID (druid), NULL);
	
	return g_strdup (gtk_entry_get_text (druid->account_name));
}


gboolean
mail_config_druid_get_default_account (MailConfigDruid *druid)
{
	g_return_val_if_fail (IS_MAIL_CONFIG_DRUID (druid), FALSE);
	
	return GTK_TOGGLE_BUTTON (druid->default_account)->active;
}


char *
mail_config_druid_get_full_name (MailConfigDruid *druid)
{
	g_return_val_if_fail (IS_MAIL_CONFIG_DRUID (druid), NULL);
	
	return g_strdup (gtk_entry_get_text (druid->full_name));
}


char *
mail_config_druid_get_email_address (MailConfigDruid *druid)
{
	g_return_val_if_fail (IS_MAIL_CONFIG_DRUID (druid), NULL);
	
	return g_strdup (gtk_entry_get_text (druid->email_address));
}


char *
mail_config_druid_get_organization (MailConfigDruid *druid)
{
	g_return_val_if_fail (IS_MAIL_CONFIG_DRUID (druid), NULL);
	
	return g_strdup (gtk_entry_get_text (druid->organization));
}


char *
mail_config_druid_get_sigfile (MailConfigDruid *druid)
{
	g_return_val_if_fail (IS_MAIL_CONFIG_DRUID (druid), NULL);
	
	return gnome_file_entry_get_full_path (druid->signature, TRUE);
}


char *
mail_config_druid_get_source_url (MailConfigDruid *druid)
{
	char *source_url, *host, *pport, *str;
	const CamelProvider *provider;
	CamelURL *url;
	int port = 0;
	
	g_return_val_if_fail (IS_MAIL_CONFIG_DRUID (druid), NULL);
	
	provider = druid->source_provider;
	if (!provider)
		return NULL;
	
	url = g_new0 (CamelURL, 1);
	url->protocol = g_strdup (provider->protocol);
	
	str = gtk_entry_get_text (druid->incoming_username);
	url->user = str && *str ? g_strdup (str) : NULL;
	
	str = gtk_object_get_data (GTK_OBJECT (druid), "source_authmech");
	url->authmech = str && *str ? g_strdup (str) : NULL;
	
	str = gtk_entry_get_text (druid->password);
	url->passwd = str && *str ? g_strdup (str) : NULL;
	
	host = g_strdup (gtk_entry_get_text (druid->incoming_hostname));
	if (host && (pport = strchr (host, ':'))) {
		port = atoi (pport + 1);
		*pport = '\0';
	}
	url->host = host;
	url->port = port;
	url->path = g_strdup (gtk_entry_get_text (druid->incoming_path));
	
	/* only 'show password' if we intend to save it */
	source_url = camel_url_to_string (url, GTK_TOGGLE_BUTTON (druid->save_password)->active);
	camel_url_free (url);
	
	return source_url;
}


gboolean
mail_config_druid_get_keep_mail_on_server (MailConfigDruid *druid)
{
	g_return_val_if_fail (IS_MAIL_CONFIG_DRUID (druid), FALSE);
	
	return GTK_TOGGLE_BUTTON (druid->incoming_keep_mail)->active;
}


gboolean
mail_config_druid_get_save_password (MailConfigDruid *druid)
{
	g_return_val_if_fail (IS_MAIL_CONFIG_DRUID (druid), FALSE);
	
	return GTK_TOGGLE_BUTTON (druid->save_password)->active;
}


char *
mail_config_druid_get_transport_url (MailConfigDruid *druid)
{
	char *transport_url, *host, *pport;
	const CamelProvider *provider;
	CamelURL *url;
	int port = 0;
	
	g_return_val_if_fail (IS_MAIL_CONFIG_DRUID (druid), NULL);
	
	provider = druid->transport_provider;
	if (!provider)
		return NULL;
	
	url = g_new0 (CamelURL, 1);
	url->protocol = g_strdup (provider->protocol);
	host = g_strdup (gtk_entry_get_text (druid->outgoing_hostname));
	if (host && (pport = strchr (host, ':'))) {
		port = atoi (pport + 1);
		*pport = '\0';
	}
	url->host = host;
	url->port = port;
	
	transport_url = camel_url_to_string (url, FALSE);
	camel_url_free (url);
	
	return transport_url;
}


gboolean
mail_config_druid_get_transport_requires_auth (MailConfigDruid *druid)
{
	g_return_val_if_fail (IS_MAIL_CONFIG_DRUID (druid), FALSE);
	
	return GTK_TOGGLE_BUTTON (druid->outgoing_requires_auth)->active;
}
