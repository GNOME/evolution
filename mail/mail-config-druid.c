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
#include "mail-config-druid.h"
#include "mail-ops.h"
#include <sys/types.h>
#include <string.h>
#include <unistd.h>

#define d(x) x


static void mail_config_druid_class_init (MailConfigDruidClass *class);
static void mail_config_druid_init       (MailConfigDruid *druid);
static void mail_config_druid_finalise   (GtkObject *obj);

static void construct_auth_menu (MailConfigDruid *druid, GList *authtypes);

static gboolean check_service (CamelURL *url, CamelProviderType type, GList **authtypes);

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
	parent_class = gtk_type_class (gnome_dialog_get_type ());
	
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
	GList *l;
	
	l = druid->providers;
	while (l) {
		CamelProvider *provider = l->data;
		
		camel_object_unref (CAMEL_OBJECT (provider));
		l = l->next;
	}
	g_list_free (druid->providers);
	
	gtk_object_unref (GTK_OBJECT (druid->gui));
	
        ((GtkObjectClass *)(parent_class))->finalize (obj);
}


static struct {
	char *name;
	char *text;
} info[] = {
	{ "htmlIdentity",
	  _("Please enter your name and email address below. The \"optional\" fields below do not need to be filled in, unless you wish to include this information in email you send.") },
	{ "htmlIncoming",
	  _("Please enter information about your incoming mail server below. If you don't know what kind of server you use, contact your system administrator or Internet Service Provider.") },
	{ "htmlAuthentication",
	  _("Your mail server supports the following types of authentication. Please select the type you want Evolution to use.") },
	{ "htmlTransport",
	  _("Please enter information about your outgoing mail protocol below. If you don't know which protocol you use, contact your system administrator or Internet Service Provider.") },
	{ "htmlAccountInfo",
	  _("You are almost done with the mail configuration process. The identity, incoming mail server and outgoing mail transport method which you provided will be grouped together to make an Evolution mail account. Please enter a name for this account in the space below. This name will be used for display purposes only.") }
};
static int num_info = (sizeof (info) / sizeof (info[0]));

static void
html_size_req (GtkWidget *widget, GtkRequisition *requisition)
{
	 requisition->height = GTK_LAYOUT (widget)->height;
}

static GtkWidget *
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
druid_cancel (GtkWidget *widget, gpointer user_data)
{
	/* Cancel the setup of the account */
	MailConfigDruid *druid = user_data;
	
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
	MailConfigDruid *config;
	
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
	
	if (prov->url_flags & CAMEL_URL_NEED_HOST)
		host = gtk_entry_get_text (druid->incoming_hostname) != NULL;
	
	if (prov->url_flags & CAMEL_URL_NEED_USER)
		user = gtk_entry_get_text (druid->incoming_username) != NULL;
	
	if (prov->url_flags & CAMEL_URL_NEED_PATH)
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
	MailConfigDruid *config;
	
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
	const CamelProvider *provider;
	GtkWidget *transport_page;
	GList *authtypes = NULL;
	CamelURL *url;
	
	provider = config->source_provider;
	url = g_new0 (CamelURL, 1);
	url->protocol = g_strdup (provider->protocol);
	url->host = mail_config_druid_get_incoming_hostname (config);
	url->user = mail_config_druid_get_incoming_username (config);
	url->path = mail_config_druid_get_incoming_path (config);
	
	/* If we can't connect, don't let them continue. */
	if (!check_service (url, CAMEL_PROVIDER_STORE, &authtypes)) {
		camel_url_free (url);
		return TRUE;
	}
	camel_url_free (url);
	
	/* If this service offers authentication, go to the next page. */
	if (authtypes) {
		construct_auth_menu (config, authtypes);
		return FALSE;
	}
	
	/* Otherwise, skip to transport page. */
	transport_page = glade_xml_get_widget (config->gui, "transport-page");
	gnome_druid_set_page (config->druid, GNOME_DRUID_PAGE (transport_page));
	
	return TRUE;
}

static void
incoming_type_changed (GtkWidget *widget, gpointer user_data)
{
	MailConfigDruid *druid = user_data;
	CamelProvider *provider;
	
	provider = gtk_object_get_data (GTK_OBJECT (widget), "provider");
	
	druid->source_provider = provider;
	
	/* hostname */
	if (provider->url_flags & CAMEL_URL_ALLOW_HOST)
		gtk_widget_set_sensitive (GTK_WIDGET (druid->incoming_hostname), TRUE);
	else
		gtk_widget_set_sensitive (GTK_WIDGET (druid->incoming_hostname), FALSE);
	
	/* username */
	if (provider->url_flags & CAMEL_URL_ALLOW_USER)
		gtk_widget_set_sensitive (GTK_WIDGET (druid->incoming_username), TRUE);
	else
		gtk_widget_set_sensitive (GTK_WIDGET (druid->incoming_username), FALSE);
	
	/* password */
	if (provider->url_flags & CAMEL_URL_ALLOW_PASSWORD)
		gtk_widget_set_sensitive (GTK_WIDGET (druid->password), TRUE);
	else
		gtk_widget_set_sensitive (GTK_WIDGET (druid->password), FALSE);
	
	/* auth */
	if (provider->url_flags & CAMEL_URL_ALLOW_AUTH)
		gtk_widget_set_sensitive (GTK_WIDGET (druid->auth_type), TRUE);
	else
		gtk_widget_set_sensitive (GTK_WIDGET (druid->auth_type), FALSE);
	
	/* path */
	if (provider->url_flags & CAMEL_URL_ALLOW_PATH)
		gtk_widget_set_sensitive (GTK_WIDGET (druid->incoming_path), TRUE);
	else
		gtk_widget_set_sensitive (GTK_WIDGET (druid->incoming_path), FALSE);
	
	incoming_check (druid);
}

/* Authentication Page */
static void
authentication_check (MailConfigDruid *druid)
{
	if (mail_config_druid_get_save_passwd (druid) &&
	    gtk_entry_get_text (druid->password) != NULL)
		gnome_druid_set_buttons_sensitive (druid->druid, TRUE, TRUE, TRUE);
	else
		gnome_druid_set_buttons_sensitive (druid->druid, TRUE, FALSE, TRUE);
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
	MailConfigDruid *config;
	
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
	gboolean sensitive;
	
	authtype = gtk_object_get_data (GTK_OBJECT (widget), "authtype");
	
	if (authtype->need_password)
		sensitive = TRUE;
	else
		sensitive = FALSE;
	
	gtk_widget_set_sensitive (GTK_WIDGET (druid->password), sensitive);
	gtk_widget_set_sensitive (GTK_WIDGET (druid->save_password), sensitive);
	
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
		
		if (!first)
			first = item;
		
		l = l->next;
	}
	
	if (first)
		gtk_signal_emit_by_name (GTK_OBJECT (first), "activate", druid);
	
	gtk_option_menu_set_menu (druid->auth_type, menu);
}

/* Transport Page */
static void
transport_check (MailConfigDruid *druid)
{
	const CamelProvider *prov = druid->transport_provider;
	gboolean next_sensitive = TRUE;
	
	if (prov->url_flags & CAMEL_URL_NEED_HOST)
		next_sensitive = gtk_entry_get_text (druid->outgoing_hostname) != NULL;
	
	gnome_druid_set_buttons_sensitive (druid->druid, TRUE, next_sensitive, TRUE);
}

static void
transport_prepare (GnomeDruidPage *page, GnomeDruid *druid, gpointer data)
{
	MailConfigDruid *config = data;
	
	transport_check (config);
}

static void
transport_next (GnomeDruidPage *page, GnomeDruid *druid, gpointer data)
{
	MailConfigDruid *config = data;
	const CamelProvider *provider;
	CamelURL *url;
	
	provider = config->transport_provider;
	url = g_new0 (CamelURL, 1);
	url->protocol = g_strdup (provider->protocol);
	url->host = mail_config_druid_get_outgoing_hostname (config);
	
	/* If we can't connect, don't let them continue. */
	if (!check_service (url, CAMEL_PROVIDER_TRANSPORT, NULL)) {
		camel_url_free (url);
		return TRUE;
	}
	camel_url_free (url);
	
	/* FIXME: check if we need auth; */
	
	return FALSE;
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
	
	provider = gtk_object_get_data (GTK_OBJECT (widget), "provider");
	druid->transport_provider = provider;
	
	/* hostname */
	if (provider->url_flags & CAMEL_URL_ALLOW_HOST)
		gtk_widget_set_sensitive (GTK_WIDGET (druid->outgoing_hostname), TRUE);
	else
		gtk_widget_set_sensitive (GTK_WIDGET (druid->outgoing_hostname), FALSE);
	
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

static void
set_defaults (MailConfigDruid *druid)
{
	GtkWidget *stores, *transports;
	GtkWidget *fstore = NULL, *ftransport = NULL;
	char *user, *realname;
	char hostname[1024];
	char domain[1024];
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
		
		address = g_strdup_printf ("%s@%s%s%s", user, hostname, domain ? "." : "",
					   domain ? domain : "");
		
		gtk_entry_set_text (druid->email_address, address);
		g_free (address);
	}
	
	/* construct incoming/outgoing option menus */
	stores = gtk_menu_new ();
	transports = gtk_menu_new ();
	druid->providers = camel_session_list_providers (session, FALSE);
	l = druid->providers;
	while (l) {
		CamelProvider *provider = l->data;
		
		if (strcmp (provider->domain, "mail")) {
			l = l->next;
			continue;
		}
		
		if (provider->object_types[CAMEL_PROVIDER_STORE] && provider->flags & CAMEL_PROVIDER_IS_SOURCE) {
			GtkWidget *item;
			
			item = gtk_menu_item_new_with_label (provider->name);
			gtk_object_set_data (GTK_OBJECT (item), "provider", provider);
			gtk_signal_connect (GTK_OBJECT (item), "activate",
					    GTK_SIGNAL_FUNC (incoming_type_changed),
					    druid);
			
			gtk_menu_append (GTK_MENU (stores), item);
			
			if (!fstore)
				fstore = item;
		}
		
		if (provider->object_types[CAMEL_PROVIDER_TRANSPORT]) {
			GtkWidget *item;
			
			item = gtk_menu_item_new_with_label (provider->name);
			gtk_object_set_data (GTK_OBJECT (item), "provider", provider);
			gtk_signal_connect (GTK_OBJECT (item), "activate",
					    GTK_SIGNAL_FUNC (transport_type_changed),
					    druid);
			
			gtk_menu_append (GTK_MENU (transports), item);
			
			if (!ftransport)
				ftransport = item;
		}
		
		l = l->next;
	}
	
	if (fstore)
		gtk_signal_emit_by_name (GTK_OBJECT (fstore), "activate", druid);
	
	if (ftransport)
		gtk_signal_emit_by_name (GTK_OBJECT (ftransport), "activate", druid);
	
	gtk_option_menu_set_menu (druid->incoming_type, stores);
	gtk_option_menu_set_menu (druid->outgoing_type, transports);
}

static void
construct (MailConfigDruid *druid)
{
	GladeXML *gui;
	GnomeDruid *druid;
	
	gui = glade_xml_new (EVOLUTION_DATA_DIR "/mail-config-druid.glade", "mail-config-window");
	druid->gui = gui;
	
	/* get our toplevel widget */
	druid = GNOME_DRUID (glade_xml_get_widget (gui, "gnome-config-druid"));
	
	/* reparent */
	gtk_widget_reparent (notebook, GTK_WIDGET (druid));
	
	druid->druid = druid;
	
	/* get our cared-about widgets */
	druid->account_text = glade_xml_get_widget (gui, "htmlAccountInfo");
	druid->account_name = GTK_ENTRY (glade_xml_get_widget (gui, "txtAccountName"));
	druid->default_account = GTK_CHECK_BOX (glade_xml_get_widget (gui, "chkAccountDefault"));
	
	druid->identity_text = glade_xml_get_widget (gui, "htmlIdentity");
	druid->full_name = GTK_ENTRY (glade_xml_get_widget (gui, "txtFullName"));
	gtk_signal_connect (GTK_OBJECT (druid->full_name), "changed", identity_changed, druid);
	druid->email_address = GTK_ENTRY (glade_xml_get_widget (gui, "txtEMail"));
	gtk_signal_connect (GTK_OBJECT (druid->email_address), "changed", identity_changed, druid);
	druid->reply_to = GTK_ENTRY (glade_xml_get_widget (gui, "txtReplyTo"));
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
	druid->incoming_delete_mail = GTK_CHECK_BOX (glade_xml_get_widget (gui, "chkIncomingDeleteMail"));
	
	druid->auth_text = glade_xml_get_widget (gui, "htmlAuthentication");
	druid->auth_type = GTK_OPTION_MENU (glade_xml_get_widget (gui, "omenuAuthType"));
	druid->password = GTK_ENTRY (glade_xml_get_widget (gui, "txtAuthPasswd"));
	gtk_signal_connect (GTK_OBJECT (druid->password), "changed", authentication_changed, druid);
	druid->save_password = GTK_CHECK_BOX (glade_xml_get_widget (gui, "chkAuthSavePasswd"));
	
	druid->outgoing_text = glade_xml_get_widget (gui, "htmlTransport");
	druid->outgoing_type = GTK_OPTION_MENU (glade_xml_get_widget (gui, "omenuTransportType"));
	druid->outgoing_hostname = GTK_ENTRY (glade_xml_get_widget (gui, "txtTransportHostname"));
	gtk_signal_connect (GTK_OBJECT (druid->outgoing_hostname), "changed", transport_changed, druid);
	druid->outgoing_requires_auth = GTK_CHECK_BOX (glade_xml_get_widget (gui, "chkTransportNeedsAuth"));
	
	glade_xml_signal_autoconnect (gui);
	
	set_defaults (druid);
	
	gnome_druid_set_buttons_sensitive (druid->druid, FALSE, TRUE, TRUE);
}

/* Async service-checking/authtype-lookup code. */

typedef struct {
	char *url;
	CamelProviderType type;
	GList **authtypes;
	gboolean success;
} check_service_input_t;

static char *
describe_check_service (gpointer in_data, gboolean gerund)
{
	if (gerund)
		return g_strdup (_("Connecting to server"));
	else
		return g_strdup (_("Connect to server"));
}

static void
do_check_service (gpointer in_data, gpointer op_data, CamelException *ex)
{
	check_service_input_t *input = in_data;
	CamelService *service;

	if (input->authtypes) {
		service = camel_session_get_service (
			session, input->url, input->type, ex);
		if (!service)
			return;
		*input->authtypes = camel_service_query_auth_types (service, ex);
	} else {
		service = camel_session_get_service_connected (
			session, input->url, input->type, ex);
	}
	if (service)
		camel_object_unref (CAMEL_OBJECT (service));
	if (!camel_exception_is_set (ex))
		input->success = TRUE;
}

static const mail_operation_spec op_check_service = {
	describe_check_service,
	0,
	NULL,
	do_check_service,
	NULL
};

static gboolean
check_service (CamelURL *url, CamelProviderType type, GList **authtypes)
{
	check_service_input_t input;
	
	input.url = camel_url_to_string (url, TRUE);
	input.type = type;
	input.authtypes = authtypes;
	input.success = FALSE;
	
	mail_operation_queue (&op_check_service, &input, FALSE);
	mail_operation_wait_for_finish ();
	g_free (input.url);
	
	return input.success;
}

MailConfigDruid *
mail_config_druid_new ()
{
	MailConfigDruid *new;
	
	new = (MailConfigDruid *) gtk_type_new (mail_config_druid_get_type ());
	construct (new);
	
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
mail_config_druid_get_reply_to (MailConfigDruid *druid)
{
	g_return_val_if_fail (IS_MAIL_CONFIG_DRUID (druid), NULL);
	
	return g_strdup (gtk_entry_get_text (druid->reply_to));
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


int
mail_config_druid_get_incoming_type (MailConfigDruid *druid)
{

}

char *
mail_config_druid_get_incoming_hostname (MailConfigDruid *druid)
{
	g_return_val_if_fail (IS_MAIL_CONFIG_DRUID (druid), NULL);
	
	return g_strdup (gtk_entry_get_text (druid->incoming_hostname));
}


char *
mail_config_druid_get_incoming_username (MailConfigDruid *druid)
{
	g_return_val_if_fail (IS_MAIL_CONFIG_DRUID (druid), NULL);
	
	return g_strdup (gtk_entry_get_text (druid->incoming_username));
}


char *
mail_config_druid_get_incoming_path (MailConfigDruid *druid)
{
	g_return_val_if_fail (IS_MAIL_CONFIG_DRUID (druid), NULL);
	
	return g_strdup (gtk_entry_get_text (druid->incoming_path));
}


gboolean
mail_config_druid_get_incoming_delete_mail (MailConfigDruid *druid)
{
	g_return_val_if_fail (IS_MAIL_CONFIG_DRUID (druid), FALSE);
	
	return GTK_TOGGLE_BUTTON (druid->incoming_delete_mail)->active;
}


int
mail_config_druid_get_auth_type (MailConfigDruid *druid)
{
	
}


char *
mail_config_druid_get_passwd (MailConfigDruid *druid)
{
	g_return_val_if_fail (IS_MAIL_CONFIG_DRUID (druid), NULL);
	
	return g_strdup (gtk_entry_get_text (druid->password));
}


gboolean
mail_config_druid_get_save_passwd (MailConfigDruid *druid)
{
	g_return_val_if_fail (IS_MAIL_CONFIG_DRUID (druid), FALSE);
	
	return GTK_TOGGLE_BUTTON (druid->save_password)->active;
}


int
mail_config_druid_get_outgoing_type (MailConfigDruid *druid)
{
	
}


char *
mail_config_druid_get_outgoing_hostname (MailConfigDruid *druid)
{
	g_return_val_if_fail (IS_MAIL_CONFIG_DRUID (druid), NULL);
	
	return g_strdup (gtk_entry_get_text (druid->outgoing_hostname));
}


gboolean
mail_config_druid_get_outgoing_requires_auth (MailConfigDruid *druid)
{
	g_return_val_if_fail (IS_MAIL_CONFIG_DRUID (druid), FALSE);
	
	return GTK_TOGGLE_BUTTON (druid->outgoing_requires_auth)->active;
}
