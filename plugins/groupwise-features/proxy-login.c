
/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 *  Authors: 
 *  Shreyas Srinivasan <sshreyas@novell.com>
 *  Sankar P <psankar@novell.com>
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

#include <glib.h>
#include <string.h>
#include <glib/gi18n.h>
#include <glade/glade.h>
#include <glib/gmain.h>
#include <gtk/gtktreemodel.h>
#include <gtk/gtkliststore.h>
#include <gtk/gtktreeselection.h>
#include <gtk/gtktreeview.h>
#include <gtk/gtkdialog.h>
#include <gtk/gtkbutton.h>
#include <gtk/gtk.h>
#include <gtk/gtktogglebutton.h>
#include <gtk/gtkcellrenderertoggle.h>
#include <gtk/gtkcellrenderertext.h>

#include <libedataserverui/e-passwords.h>
#include <mail/mail-component.h>
#include <mail/em-folder-tree.h>
#include <mail/mail-config.h>
#include <mail/em-folder-selector.h>
#include <mail/em-popup.h>
#include <mail/em-account-editor.h>
#include <camel/camel-url.h>
#include <camel/camel-store.h>
#include <mail/mail-ops.h>
#include <e-util/e-account.h>
#include <e-util/e-error.h>
#include <e-util/e-icon-factory.h>
#include <e-util/e-util-private.h>

#include <e-gw-container.h>
#include <e-gw-connection.h>
#include <e-gw-message.h>
#include <libedataserverui/e-name-selector.h>
#include "proxy-login.h"

#define GW(name) glade_xml_get_widget (priv->xml, name)

#define ACCOUNT_PICTURE 0
#define ACCOUNT_NAME 1

proxyLogin *pld = NULL;
static GObjectClass *parent_class = NULL;

struct _proxyLoginPrivate {
	/* Glade XML data for the Add/Edit Proxy dialog*/
	GladeXML *xml;
	/* Widgets */
	GtkWidget *main;

	/*Tree Store*/
	GtkTreeStore *store;
	/*Tree View*/
	GtkTreeView *tree;

	char *help_section;
};

static void
proxy_login_finalize (GObject *object)
{
	proxyLogin *prd = (proxyLogin *) object;
	proxyLoginPrivate *priv;

	g_return_if_fail (IS_PROXY_LOGIN (prd));
	priv = prd->priv;
	g_list_foreach (prd->proxy_list, (GFunc)g_free, NULL);
	g_list_free (prd->proxy_list);
	prd->proxy_list = NULL;
	g_object_unref (priv->xml);
	g_free (priv->help_section);

	if (prd->priv) {
		g_free (prd->priv);
		prd->priv = NULL;
	}
	
	if (parent_class->finalize)
		(* parent_class->finalize) (object);
}

static void
proxy_login_dispose (GObject *object)
{
	proxyLogin *prd = (proxyLogin *) object;

	g_return_if_fail (IS_PROXY_LOGIN (prd));

	if (parent_class->dispose)
		(* parent_class->dispose) (object);
}

/* Class initialization function for the Send Options */
static void
proxy_login_class_init (GObjectClass *object)
{
	proxyLoginClass *klass;
	GObjectClass *object_class;

	klass = PROXY_LOGIN_CLASS (object);
	parent_class = g_type_class_peek_parent (klass);
	object_class = G_OBJECT_CLASS (klass);

	object_class->finalize = proxy_login_finalize;
	object_class->dispose = proxy_login_dispose;
}

static void
proxy_login_init (GObject *object)
{
	proxyLogin *prd;
	proxyLoginPrivate *priv;

	prd = PROXY_LOGIN (object);
	priv = g_new0 (proxyLoginPrivate, 1);
	prd->priv = priv;
	
	prd->proxy_list = NULL;
	priv->xml = NULL;
	priv->main = NULL;
	priv->store = NULL;
	priv->tree = NULL;
}

GType 
proxy_login_get_type (void)
{
  static GType type = 0;

  if (type == 0) {
    static const GTypeInfo info = {
      sizeof (proxyLoginClass),
      NULL,   /* base_init */
      NULL,   /* base_finalize */
      (GClassInitFunc) proxy_login_class_init,   /* class_init */
      NULL,   /* class_finalize */
      NULL,   /* class_data */
      sizeof (proxyLogin),
     0,      /* n_preallocs */
     (GInstanceInitFunc) proxy_login_init,
 	NULL    /* instance_init */
    };
    type = g_type_register_static (G_TYPE_OBJECT,
                                   "proxyLoginType",
                                   &info, 0);
  }

  return type;
}

proxyLogin * 
proxy_login_new (void)
{
	proxyLogin *prd;

	prd = g_object_new (TYPE_PROXY_LOGIN, NULL);
	
	return prd;
}

static int
proxy_get_password (EAccount *account, char **user_name, char **password)
{
	char *uri, *failed_auth, *key, *prompt;
	CamelURL *url;
	const char *poa_address, *use_ssl, *soap_port;

	url = camel_url_new (account->source->url, NULL);
	if (url == NULL) 
		return 0;
	*user_name = g_strdup (url->user);
	poa_address = url->host; 
	if (!poa_address || strlen (poa_address) ==0)
		return 0;
	
        soap_port = camel_url_get_param (url, "soap_port");
        if (!soap_port || strlen (soap_port) == 0)
                soap_port = "7191";
	use_ssl = camel_url_get_param (url, "use_ssl");

	key =  g_strdup_printf ("groupwise://%s@%s/", url->user, poa_address); 
	
	if (!g_str_equal (use_ssl, "never"))
		uri = g_strdup_printf ("https://%s:%s/soap", poa_address, soap_port);
	else 
		uri = g_strdup_printf ("http://%s:%s/soap", poa_address, soap_port);
	
	failed_auth = "";
	
	prompt = g_strdup_printf (_("%sEnter password for %s (user %s)"),
			failed_auth, poa_address, url->user);

	*password = e_passwords_get_password ("Groupwise", key);

	g_free (key);
	g_free (prompt);
	g_free (uri); 
	camel_url_free (url);

	return 1;
}

static EGwConnection * 
proxy_login_get_cnc (EAccount *account)
{
	EGwConnection *cnc;
	CamelURL *url;
	url = camel_url_new (account->source->url, NULL);
	char *uri = NULL, *failed_auth = NULL, *key = NULL, *prompt = NULL, *password = NULL;
	const char *use_ssl, *soap_port;
	gboolean remember;
	
	url = camel_url_new (account->source->url, NULL);
	if (url == NULL) 
		return NULL;
	if (!url->host || strlen (url->host) ==0)
		return NULL;
	
        soap_port = camel_url_get_param (url, "soap_port");
        if (!soap_port || strlen (soap_port) == 0)
                soap_port = "7191";
	use_ssl = camel_url_get_param (url, "use_ssl");

	key =  g_strdup_printf ("groupwise://%s@%s/", url->user, url->host); 
	if (!g_str_equal (use_ssl, "never"))
		uri = g_strdup_printf ("https://%s:%s/soap", url->host, soap_port);
	else 
		uri = g_strdup_printf ("http://%s:%s/soap", url->host, soap_port);
	
	failed_auth = "";
	cnc = NULL;
	
	prompt = g_strdup_printf (_("%sEnter password for %s (user %s)"),
			failed_auth, url->host, url->user);

	password = e_passwords_get_password ("Groupwise", key);

	if (!password)
		password = e_passwords_ask_password (prompt, "Groupwise", key, prompt,
				E_PASSWORDS_REMEMBER_FOREVER|E_PASSWORDS_SECRET, &remember, NULL);
	
	g_free (prompt);
	cnc = e_gw_connection_new (uri, url->user, password);
	if (!E_IS_GW_CONNECTION(cnc) && use_ssl && g_str_equal (use_ssl, "when-possible")) {
		char *http_uri = g_strconcat ("http://", uri + 8, NULL);
		cnc = e_gw_connection_new (http_uri, url->user, password);
		g_free (http_uri);
	}
	
	g_free (key);
	g_free (password);
	g_free (uri); 
	camel_url_free (url);

	return cnc;
}
	

static void 
proxy_login_cb (GtkDialog *dialog, gint state)
{
	GtkWidget *account_name_tbox;
	proxyLoginPrivate *priv;
	char *proxy_name;

	priv = pld->priv;
	account_name_tbox = glade_xml_get_widget (priv->xml, "account_name");
	proxy_name = g_strdup ((char *) gtk_entry_get_text ((GtkEntry *) account_name_tbox));

	switch (state) {
	    case GTK_RESPONSE_OK:
		    gtk_widget_destroy (priv->main);
		    proxy_soap_login (proxy_name);
		    g_object_unref (pld);    
		    break;
	    case GTK_RESPONSE_CANCEL:
    		    gtk_widget_destroy (priv->main);
		    g_object_unref (pld);
		    break;
	    case GTK_RESPONSE_HELP:
		    break;
	}

	g_free (proxy_name);
}

static void 
proxy_soap_login (char *email)
{
	EAccountList *accounts = mail_config_get_accounts();
	EAccount *srcAccount;
	EAccount *dstAccount;
	EGwConnection *proxy_cnc, *cnc;
	CamelURL *uri = NULL, *parent = NULL ;
	char *password = NULL, *user_name = NULL;
	char *proxy_source_url = NULL, *parent_source_url = NULL ;
	char *name;
	int i;
	int permissions = 0;
	
	for (i=0; email[i]!='\0' && email[i]!='@' ; i++);
	if (email[i]=='@')
		name = g_strndup(email, i);
	else {
		e_error_run (NULL, "org.gnome.evolution.proxy-login:invalid-user",email ,NULL);
		return;
	}	
	
	srcAccount = pld->account;
	cnc = proxy_login_get_cnc(srcAccount);
	proxy_get_password (srcAccount, &user_name, &password);
	
	proxy_cnc = e_gw_connection_get_proxy_connection (cnc, user_name, password, email, &permissions);
	
	if (proxy_cnc) {
		parent = camel_url_new (e_account_get_string(srcAccount, E_ACCOUNT_SOURCE_URL), NULL);
		parent_source_url = camel_url_to_string (parent, CAMEL_URL_HIDE_PASSWORD);
		uri = camel_url_copy (parent);
		camel_url_set_user (uri, name);
		proxy_source_url = camel_url_to_string (uri, CAMEL_URL_HIDE_PASSWORD);
		dstAccount = e_account_new();
		e_account_set_string(dstAccount, E_ACCOUNT_ID_ADDRESS, email);
		dstAccount->enabled = TRUE;
		e_account_set_string(dstAccount, E_ACCOUNT_SOURCE_URL, proxy_source_url);
		e_account_set_string (dstAccount, E_ACCOUNT_TRANSPORT_URL, proxy_source_url);
		e_account_set_string (dstAccount, E_ACCOUNT_NAME, email);
		e_account_set_string (dstAccount, E_ACCOUNT_ID_NAME, name);
		e_account_set_string (dstAccount, E_ACCOUNT_PROXY_PARENT_UID, srcAccount->uid);
		e_account_list_add(accounts, dstAccount);
		e_account_list_change (accounts, srcAccount);
		e_account_list_save(accounts);
		g_object_set_data ((GObject *)dstAccount, "permissions", GINT_TO_POINTER(permissions));
		mail_get_store(e_account_get_string(dstAccount, E_ACCOUNT_SOURCE_URL), NULL, proxy_login_add_new_store, dstAccount);

		g_free (proxy_source_url);
		g_free (parent_source_url);
		camel_url_free (parent);
	} else {
		e_error_run (NULL, "org.gnome.evolution.proxy-login:invalid-user",email ,NULL);
		return;  
	}	

	g_object_unref (cnc);
	g_free (name);
	g_free (user_name);
	g_free (password);
}

 
static void 
proxy_login_add_new_store (char *uri, CamelStore *store, void *user_data)
{
	MailComponent *component = mail_component_peek ();
	EAccount *account = user_data;
	int permissions = GPOINTER_TO_INT(g_object_get_data ((GObject *)account, "permissions"));

	if (store == NULL)
		return;

	if (!(permissions & E_GW_PROXY_MAIL_WRITE))
	    store->mode &= !CAMEL_STORE_WRITE;

	store->flags |= CAMEL_STORE_PROXY;
	mail_component_add_store (component, store, account->name);
}

static void 
proxy_login_tree_view_changed_cb(GtkDialog *dialog)
{
	proxyLoginPrivate *priv = pld->priv;
	GtkTreeSelection* account_select;
	GtkTreeIter iter;
	GtkWidget *account_name_tbox;
	GtkTreeModel *model;
	char *account_mailid;

	account_select = gtk_tree_view_get_selection (GTK_TREE_VIEW (priv->tree));
	gtk_tree_selection_get_selected (account_select, &model, &iter);
        gtk_tree_model_get (model, &iter, ACCOUNT_NAME, &account_mailid, -1);
	account_mailid = g_strrstr (account_mailid, "\n") + 1;
	account_name_tbox = glade_xml_get_widget (priv->xml, "account_name");
	gtk_entry_set_text((GtkEntry*) account_name_tbox,account_mailid);
}

static void 
proxy_login_setup_tree_view (void)
{
	proxyLoginPrivate *priv;
	GtkTreeSelection *selection;
	GtkCellRenderer *renderer;
	GtkTreeViewColumn *column;
	
	priv = pld->priv;
	renderer = g_object_new (GTK_TYPE_CELL_RENDERER_PIXBUF,
				 "xpad", 4,
				 "ypad", 4,
				 NULL);
	column = gtk_tree_view_column_new_with_attributes ("Picture", renderer, "pixbuf", ACCOUNT_PICTURE, NULL);
	gtk_tree_view_append_column (GTK_TREE_VIEW (priv->tree), column);
	renderer = gtk_cell_renderer_text_new ();
	column = gtk_tree_view_column_new_with_attributes ("Name", renderer, "text", ACCOUNT_NAME, NULL);
	gtk_tree_view_append_column (GTK_TREE_VIEW (priv->tree), column);
	gtk_tree_view_set_model (priv->tree, GTK_TREE_MODEL (priv->store));
	selection = gtk_tree_view_get_selection (priv->tree);
	gtk_tree_selection_set_mode (selection, GTK_SELECTION_BROWSE);
	g_signal_connect (G_OBJECT (selection), "changed", G_CALLBACK(proxy_login_tree_view_changed_cb), NULL);
}

static void 
proxy_login_update_tree (void)
{
    	GtkTreeIter iter;
	int i,n;
	GdkPixbuf *broken_image = NULL;
	GList *proxy_list = NULL;
	char *proxy_name;
	char *proxy_email;
	EGwConnection *cnc;
	proxyLoginPrivate *priv = pld->priv;
	gchar *file_name = e_icon_factory_get_icon_filename ("stock_person", 48);
	broken_image = gdk_pixbuf_new_from_file (file_name, NULL);
	
	cnc = proxy_login_get_cnc(pld->account);
	e_gw_connection_get_proxy_list (cnc, &proxy_list);

	gtk_tree_store_clear (priv->store);
	if (proxy_list != NULL) {
		n = g_list_length(proxy_list);
		for (i=0;i<n;i=i+2) {
			proxy_name = g_list_nth_data(proxy_list,i);
			proxy_email = g_list_nth_data(proxy_list,i+1);
			gtk_tree_store_append (priv->store, &iter, NULL);
			gtk_tree_store_set (priv->store, &iter, 0, broken_image, 1, g_strconcat(proxy_name, "\n", proxy_email, NULL), -1);
		}
		gtk_tree_view_set_model (GTK_TREE_VIEW(priv->tree),GTK_TREE_MODEL (priv->store));
	}
	g_object_unref (cnc);
}

void
org_gnome_proxy_account_login (EPopup *ep, EPopupItem *p, void *data)
{
	char *uri = data;
	proxyLoginPrivate *priv;
	EGwConnection *cnc;
	char *gladefile;

	/* This pops-up the password dialog in case the User has forgot-passwords explicitly */
	cnc = proxy_login_get_cnc (mail_config_get_account_by_source_url (uri));
	g_object_unref (cnc);

	pld = proxy_login_new();
	priv = pld->priv;

	gladefile = g_build_filename (EVOLUTION_GLADEDIR,
				      "proxy-login-dialog.glade",
				      NULL);
	priv->xml = glade_xml_new (gladefile, NULL, NULL);
	g_free (gladefile);

	priv->main = glade_xml_get_widget (priv->xml, "proxy_login_dialog");
	pld->account = mail_config_get_account_by_source_url (uri);
	priv->tree = GTK_TREE_VIEW (glade_xml_get_widget (priv->xml, "proxy_login_treeview"));
	priv->store =  gtk_tree_store_new (2,
					   GDK_TYPE_PIXBUF,
					   G_TYPE_STRING
					   );
	proxy_login_setup_tree_view ();
	proxy_login_update_tree ();
	g_signal_connect (GTK_DIALOG (priv->main), "response", G_CALLBACK(proxy_login_cb), NULL);
	gtk_widget_show (GTK_WIDGET (priv->main));		
 }

static EPopupItem popup_items[] = {
{ E_POPUP_ITEM, "20.emc.04", N_("_Proxy Login..."), org_gnome_proxy_account_login, NULL, NULL, 0, EM_POPUP_FOLDER_STORE }
};

static void 
popup_free (EPopup *ep, GSList *items, void *data)
{
	g_slist_free (items);
}

void
org_gnome_create_proxy_login_option (EPlugin *ep, EMPopupTargetFolder *t)
{
	EAccount *account;
	GSList *menus = NULL;
	int i;

	account = mail_config_get_account_by_source_url (t->uri);
	if (g_strrstr (t->uri,"groupwise://") && !account->parent_uid) {
		popup_items[0].label =  _(popup_items[0].label);
		for (i = 0; i < sizeof (popup_items) / sizeof (popup_items[0]); i++)
			menus = g_slist_prepend (menus, &popup_items[i]);
		e_popup_add_items (t->target.popup, menus, NULL, popup_free, t->uri);
	}
	return;

}
