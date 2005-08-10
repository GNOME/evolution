/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 *  Authors: 
 *  Shreyas Srinivasan (sshreyas@novell.com)
 *  Sankar P ( psankar@novell.com )
 *
 *  Copyright 2004 Novell, Inc. (www.novell.com)
 *
 *  This program is free software; you can redistribute it and/or
 *  modify it under the terms of version 2 of the GNU General Public
 *  License as published by the Free Software Foundation.
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

#include <stdlib.h>
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

#include <libedataserverui/e-contact-store.h>

#include <libgnomeui/gnome-ui-init.h>
#include <libgnome/gnome-init.h>
#include <e-util/e-error.h>
#include <e-gw-container.h>
#include <e-gw-connection.h>
#include <e-gw-message.h>

#include <mail/em-account-editor.h>
#include <mail/em-config.h>
#include <mail/mail-component.h>
#include <mail/mail-ops.h>
#include <e-util/e-account.h>
#include <e-util/e-account-list.h>
#include <e-util/e-icon-factory.h>

#include <camel/camel-url.h>
#include <libedataserverui/e-passwords.h>
#include <libedataserverui/e-name-selector.h>
#include <proxy.h>
#include <string.h>

#define GW(name) glade_xml_get_widget (priv->xml, name)

#define ACCOUNT_PICTURE 0
#define ACCOUNT_NAME 1
#define PROXY_ADD_DIALOG 2
#define PROXY_EDIT_DIALOG 3

static GObjectClass *parent_class = NULL;

struct _proxyDialogPrivate {
	/* Glade XML data for the Add/Edit Proxy dialog*/
	GladeXML *xml;
	/*Glade XML data for Proxy Tab*/
	GladeXML *xml_tab;

	/* Widgets */
	GtkWidget *main;

	/*name selector dialog*/
	ENameSelector *proxy_name_selector;
	
	GtkTreeView *tree;
	GtkTreeStore *store;

	/* Check Boxes for storing proxy priveleges*/
	GtkWidget *tab_dialog;
	GtkWidget *account_name;
	GtkWidget *mail_read;
	GtkWidget *mail_write;
	GtkWidget *app_read;
	GtkWidget *app_write;
	GtkWidget *note_read;
	GtkWidget *note_write;
	GtkWidget *task_read;
	GtkWidget *task_write;
	GtkWidget *alarms;
	GtkWidget *notifications;
	GtkWidget *options;
	GtkWidget *private;
        char *help_section;

	GList *proxy_list;
};

//static void free_proxy_handler (proxyHandler *handler);

static void
proxy_dialog_dispose (GObject *object)
{
	proxyDialog *prd = (proxyDialog *) object;

	g_return_if_fail (IS_PROXY_DIALOG (prd));

	if (parent_class->dispose)
		(*parent_class->dispose) (object);
}

static void
free_proxy_handler (proxyHandler *handler)
{
	if (handler->uniqueid)
		g_free (handler->uniqueid);

	if (handler->proxy_name)
		g_free (handler->proxy_name);

	if (handler->proxy_email)
		g_free (handler->proxy_email);			

	handler->uniqueid = NULL;
	handler->proxy_name = NULL;
	handler->proxy_email = NULL;
}

void
free_proxy_list (GList *proxy_list)
{
	if (proxy_list) {
		g_list_foreach (proxy_list, (GFunc) free_proxy_handler, NULL);
		g_list_free (proxy_list);
		proxy_list = NULL;
	}
}

static void
proxy_dialog_finalize (GObject *object)
{
	proxyDialog *prd = (proxyDialog *) object;
	proxyDialogPrivate *priv;

	g_return_if_fail (IS_PROXY_DIALOG (prd));
	priv = prd->priv;
	
	if(priv->proxy_name_selector)
		g_object_unref (priv->proxy_name_selector);

	if (priv) {
		free_proxy_list (priv->proxy_list);
		g_free (priv->help_section);
		g_object_unref (priv->xml_tab);
		g_object_unref (prd->priv);
		prd->priv = NULL;
	}
	
	prd = NULL;
	if (parent_class->finalize)
		(* parent_class->finalize) (object);
}

/* Class initialization function for the Proxy*/
static void
proxy_dialog_class_init (GObjectClass *object)
{
	proxyDialogClass *klass;
	GObjectClass *object_class;

	klass = PROXY_DIALOG_CLASS (object);
	parent_class = g_type_class_peek_parent (klass);
	object_class = G_OBJECT_CLASS (klass);

	object_class->finalize = proxy_dialog_finalize;
	object_class->dispose = proxy_dialog_dispose;
}

static void
proxy_dialog_init (GObject *object)
{
	proxyDialog *prd;
	proxyDialogPrivate *priv;

	prd = PROXY_DIALOG (object);
	priv = g_new0 (proxyDialogPrivate, 1);

	prd->priv = priv;

	priv->tab_dialog = NULL;	
	priv->xml = NULL;
	priv->xml_tab = NULL;
	priv->main = NULL;
	priv->tree = NULL;
	priv->store = NULL;
	priv->proxy_name_selector = NULL;
	priv->account_name = NULL;
	priv->mail_read = NULL;
	priv->mail_write = NULL;
	priv->app_read = NULL;
	priv->app_write = NULL;
	priv->note_read = NULL;
	priv->note_write = NULL;
	priv->task_read = NULL;
	priv->task_write = NULL;
	priv->alarms = NULL;
	priv->notifications = NULL; 
	priv->options = NULL;
	priv->private = NULL;
	priv->help_section = NULL;

	priv->proxy_list = NULL;
}

GType
proxy_dialog_get_type (void) 
{
  static GType type = 0;

  if (type == 0) {
    static const GTypeInfo info = {
      sizeof (proxyDialogClass),
      NULL,   /* base_init */
      NULL,   /* base_finalize */
      (GClassInitFunc) proxy_dialog_class_init,   /* class_init */
      NULL,   /* class_finalize */
      NULL,   /* class_data */
      sizeof (proxyDialog),
     0,      /* n_preallocs */
     (GInstanceInitFunc) proxy_dialog_init,
 	NULL    /* instance_init */
    };

    type = g_type_register_static (G_TYPE_OBJECT,
                                   "proxyDialogType",
                                   &info, 0);
  }

  return type;
}

proxyDialog * 
proxy_dialog_new (void)
{
	proxyDialog *prd;

	prd = g_object_new (TYPE_PROXY_DIALOG, NULL);
	
	return prd;
}

static int 
proxy_get_permissions_from_dialog (EAccount *account)
{
	int permissions;
	proxyDialogPrivate *priv;
	proxyDialog *prd = NULL;

	prd = g_object_get_data ((GObject *)account, "prd");    
	priv = prd->priv;
	permissions = 0;
	
	if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (priv->mail_read)))
		permissions |= E_GW_PROXY_MAIL_READ;

	if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (priv->mail_write)))
		permissions |= E_GW_PROXY_MAIL_WRITE;

	if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (priv->app_read)))
		permissions |= E_GW_PROXY_APPOINTMENT_READ;

	if (gtk_toggle_button_get_active ( GTK_TOGGLE_BUTTON (priv->app_write)))
		permissions |= E_GW_PROXY_APPOINTMENT_WRITE;

	if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (priv->task_read)))
		permissions |= E_GW_PROXY_TASK_READ;

	if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (priv->task_write)))
		permissions |= E_GW_PROXY_TASK_WRITE;

	if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (priv->note_read)))
		permissions |= E_GW_PROXY_NOTES_READ;

	if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (priv->note_write)))
		permissions |= E_GW_PROXY_NOTES_WRITE;

	if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (priv->alarms)))
		permissions |= E_GW_PROXY_GET_ALARMS;

	if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (priv->notifications)))
		permissions |= E_GW_PROXY_GET_NOTIFICATIONS;

	if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (priv->options)))
		permissions |= E_GW_PROXY_MODIFY_FOLDERS;

	if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (priv->private)))
		permissions |= E_GW_PROXY_READ_PRIVATE;

	return permissions;
}

static int 
proxy_dialog_store_widgets_data (EAccount *account, gint32 dialog)
{
	GtkTreeIter iter;
	GtkTreeSelection* account_select;
	GtkTreeModel *model;
	proxyHandler *new_proxy = NULL;
	proxyDialogPrivate *priv;
	char *account_mailid;
	proxyDialog *prd = NULL;

	prd = g_object_get_data ((GObject *)account, "prd");    
	priv = prd->priv;

	switch (dialog)
	{
		case PROXY_ADD_DIALOG:
			{
				ENameSelectorEntry *name_selector_entry;
				EDestinationStore *destination_store;
				GList *destinations, *tmp;
				char *name, *email;
				GList *existing_list;
				name_selector_entry = e_name_selector_peek_section_entry (priv->proxy_name_selector, "Add User");
				destination_store = e_name_selector_entry_peek_destination_store (E_NAME_SELECTOR_ENTRY (
							name_selector_entry));
				destinations = e_destination_store_list_destinations (destination_store);
				tmp = destinations;

				if (!tmp) {
					e_error_run (NULL, "org.gnome.evolution.proxy:no-user",NULL ,NULL);
					return -1; 
				}
				
				for (; tmp != NULL; tmp = g_list_next (tmp)) {
					email = NULL;
					email = (char *)e_destination_get_email (tmp->data);

					if (g_strrstr (email, "@") == NULL ) {
						e_error_run (NULL, "org.gnome.evolution.proxy:invalid-user", email, NULL);
						return -1;
					} 	
					if (! g_ascii_strcasecmp(e_gw_connection_get_user_email (prd->cnc), email)) {
						e_error_run (NULL, "org.gnome.evolution.proxy:invalid-user", email, NULL);
						return -1;
					}

					/*check whether already exists*/
					existing_list = priv->proxy_list;

					for (;existing_list; existing_list = g_list_next(existing_list)) {
						new_proxy = (proxyHandler *) existing_list->data;
						if ( !g_ascii_strcasecmp (new_proxy->proxy_email, email) ) {
							
							e_error_run (NULL, "org.gnome.evolution.proxy:user-is-proxy",email ,NULL);
							return -1;
						}
					}
				}
				tmp = destinations;

				for (; tmp != NULL; tmp = g_list_next (tmp)) {
					name = NULL; email = NULL;
					email = (char *) e_destination_get_email (tmp->data);
					name = (char *) e_destination_get_name (tmp->data);
					new_proxy = (proxyHandler *) g_malloc (sizeof (proxyHandler));

					if (name)
						new_proxy->proxy_name = g_strdup (name);
					else
						new_proxy->proxy_name = g_strdup (email);

					new_proxy->proxy_email = g_strdup (email);
					new_proxy->uniqueid = NULL;
					new_proxy->flags =  E_GW_PROXY_NEW;
					new_proxy->permissions = proxy_get_permissions_from_dialog (account);
				
					priv->proxy_list = g_list_append (priv->proxy_list, new_proxy);
				}
			}
			break;
		case PROXY_EDIT_DIALOG:
			account_select = gtk_tree_view_get_selection (GTK_TREE_VIEW (priv->tree));
			gtk_tree_selection_get_selected (account_select, &model, &iter);
			gtk_tree_model_get (model, &iter, ACCOUNT_NAME, &account_mailid, -1);
			account_mailid = g_strrstr (account_mailid, "\n") + 1;
			new_proxy = proxy_get_item_from_list (account, account_mailid);

			if (!new_proxy->flags & E_GW_PROXY_NEW)
				new_proxy->flags = E_GW_PROXY_EDITED;

			new_proxy->permissions = proxy_get_permissions_from_dialog (account);
			break;
		default:
			return -1;
	}    

	return 0;
}


static gboolean
proxy_dialog_initialize_widgets (EAccount *account)
{
	proxyDialogPrivate *priv;
	proxyDialog *prd = NULL;

	prd = g_object_get_data ((GObject *)account, "prd");    
	priv = prd->priv;
	priv->account_name = GW ("proxy_account_name");
	priv->mail_read = GW ("mailRead");
	priv->mail_write = GW ("mailWrite");
	priv->app_read = GW ("appRead");
	priv->app_write = GW ("appWrite");
	priv->note_read = GW ("noteRead");
	priv->note_write = GW ("noteWrite");
	priv->task_read = GW ("taskRead");
	priv->task_write = GW ("taskWrite");
	priv->alarms = GW ("alarms");
	priv->notifications = GW ("notifications");
	priv->options = GW ("modify_rules");
	priv->private = GW ("read_private");
	
	return (priv->account_name
		&& priv->mail_read
		&& priv->mail_write
		&& priv->app_read
		&& priv->app_write
		&& priv->note_read
		&& priv->note_write
		&& priv->task_read
		&& priv->task_write
		&& priv->alarms
		&& priv->notifications
		&& priv->options
		&& priv->private);
}

static EGwConnection * 
proxy_get_cnc (EAccount *account)
{
	EGwConnection *cnc;
	char *uri, *failed_auth, *key, *prompt, *password = NULL;
	CamelURL *url;
	const char *poa_address, *use_ssl, *soap_port;
	gboolean remember;

	url = camel_url_new (account->source->url, NULL);
	if (url == NULL) 
		return NULL;
	poa_address = url->host; 
	if (!poa_address || strlen (poa_address) ==0)
		return NULL;
	
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
	cnc = NULL;
	
	prompt = g_strdup_printf (_("%sEnter password for %s (user %s)"),
			failed_auth, poa_address, url->user);

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

	camel_url_free (url);
	return cnc;
}

void 
proxy_abort (GtkWidget *button, EConfigHookItemFactoryData *data)
{
	EMConfigTargetAccount *target_account;
	EAccount *account;
	proxyDialog *prd = NULL;

	target_account = (EMConfigTargetAccount *)data->config->target;
	account = target_account->account;
	prd = g_object_get_data ((GObject *)account, "prd");    
	
	if (prd == NULL)
		return;

	g_object_unref (prd);
	prd = NULL;
}
void 
proxy_commit (GtkWidget *button, EConfigHookItemFactoryData *data)
{
	EAccount *account;
	EMConfigTargetAccount *target_account;
	proxyDialogPrivate *priv;
	GList *list_iter;
	proxyHandler *aclInstance;
	proxyDialog *prd = NULL;

	target_account = (EMConfigTargetAccount *)data->config->target;
	account = target_account->account;
	prd = g_object_get_data ((GObject *)account, "prd");    
	priv = prd->priv;
	list_iter = priv->proxy_list;
	if (prd == NULL || list_iter == NULL)
		return;
	for (;list_iter; list_iter = g_list_next (list_iter)) {
		aclInstance = (proxyHandler *) list_iter->data;

		/* Handle case where the structure is new and deleted*/
		if ( !((aclInstance->flags & E_GW_PROXY_NEW) && (aclInstance->flags & E_GW_PROXY_DELETED))) {

			if ( !E_IS_GW_CONNECTION(prd->cnc)) 	/* Add check in case the connection request fails*/
				prd->cnc = proxy_get_cnc (account);

			if (aclInstance->flags & E_GW_PROXY_NEW )         	
				e_gw_connection_add_proxy (prd->cnc, aclInstance);		

			if (aclInstance->flags & E_GW_PROXY_DELETED)
				e_gw_connection_remove_proxy (prd->cnc, aclInstance);

			if (aclInstance->flags & E_GW_PROXY_EDITED)
				e_gw_connection_modify_proxy (prd->cnc, aclInstance);
		}
	}

	g_object_unref (prd);
	prd = NULL;
}

static void 
proxy_setup_meta_tree_view (EAccount *account)
{
	proxyDialog *prd = NULL;
	proxyDialogPrivate *priv;
	GtkTreeSelection *selection;
	GtkCellRenderer *renderer;
	GtkTreeViewColumn *column;

	prd = g_object_get_data ((GObject *)account, "prd");    
	priv = prd->priv;
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
}

static void
proxy_update_tree_view (EAccount *account)
{
	proxyDialog *prd = NULL;
    	GtkTreeIter iter;
	GdkPixbuf *broken_image = NULL;
	GList *list_iter;
	proxyHandler *aclInstance;
	gchar *file_name = e_icon_factory_get_icon_filename ("stock_person", E_ICON_SIZE_DIALOG);
	proxyDialogPrivate *priv;

	prd = g_object_get_data ((GObject *)account, "prd");    
	priv = prd->priv;
	broken_image = gdk_pixbuf_new_from_file (file_name, NULL);
	
	gtk_tree_store_clear (priv->store);
	list_iter = priv->proxy_list;

	for (;list_iter; list_iter = g_list_next(list_iter)) {        
	        aclInstance = (proxyHandler *) list_iter->data;

		if(! (aclInstance->flags & E_GW_PROXY_DELETED )) {
			gtk_tree_store_append (priv->store, &iter, NULL);
			gtk_tree_store_set (priv->store, &iter, 0, broken_image, 1, g_strconcat(aclInstance->proxy_name,"\n",aclInstance->proxy_email, NULL), -1);
		}
	}
   
	/*Fixme : Desensitize buttons if the list is Null*/
	gtk_tree_view_set_model (GTK_TREE_VIEW(priv->tree), GTK_TREE_MODEL (priv->store));
}

GtkWidget* 
org_gnome_proxy (EPlugin *epl, EConfigHookItemFactoryData *data)
{
	EMConfigTargetAccount *target_account;
	EAccount *account;
	GtkButton *addProxy, *removeProxy, *editProxy;
	proxyDialog *prd;
	proxyDialogPrivate *priv;

	target_account = (EMConfigTargetAccount *)data->config->target;
	account = target_account->account;
	if (g_strrstr (e_account_get_string(account, E_ACCOUNT_SOURCE_URL), "groupwise://"))
	{
		prd = proxy_dialog_new ();
		g_object_set_data_full ((GObject *) account, "prd", prd, (GDestroyNotify) g_object_unref);
		priv = prd->priv;
		priv->xml_tab = glade_xml_new (EVOLUTION_GLADEDIR "/proxy-listing.glade", "proxy_vbox", NULL);

		if (account->enabled) {	
			priv->tab_dialog = GTK_WIDGET (glade_xml_get_widget (priv->xml_tab, "proxy_vbox"));
			priv->tree = GTK_TREE_VIEW (glade_xml_get_widget (priv->xml_tab, "proxy_access_list"));
			priv->store =  gtk_tree_store_new (2,
							   GDK_TYPE_PIXBUF,
							   G_TYPE_STRING
				);
			proxy_setup_meta_tree_view (account);
			
			addProxy = (GtkButton *) glade_xml_get_widget (priv->xml_tab, "add_proxy");
			removeProxy = (GtkButton *) glade_xml_get_widget (priv->xml_tab, "remove_proxy");
			editProxy = (GtkButton *) glade_xml_get_widget (priv->xml_tab, "edit_proxy");
			
			g_signal_connect (addProxy, "clicked", G_CALLBACK(proxy_add_account), account);	
			g_signal_connect (removeProxy, "clicked", G_CALLBACK(proxy_remove_account), account);
			g_signal_connect (editProxy, "clicked", G_CALLBACK(proxy_edit_account), account);
			
			prd->cnc = proxy_get_cnc(account);
			
			priv->proxy_list = NULL;
			if (e_gw_connection_get_proxy_access_list(prd->cnc, &priv->proxy_list)!= E_GW_CONNECTION_STATUS_OK) 
				return NULL;
			proxy_update_tree_view (account);
		} else {
			GtkWidget *label;
			priv->tab_dialog = gtk_vbox_new (TRUE, 10);
			label = gtk_label_new (_("The Proxy tab will be available only when the account is enabled."));
			gtk_box_pack_start ((GtkBox *)priv->tab_dialog, label, TRUE, TRUE, 10);	
		}	
			
		gtk_notebook_append_page ((GtkNotebook *)(data->parent), (GtkWidget *)priv->tab_dialog, gtk_label_new("Proxy"));
		gtk_widget_show_all (priv->tab_dialog);
	}  else if (!g_strrstr (e_account_get_string(account, E_ACCOUNT_SOURCE_URL), "groupwise://")) {
		prd = g_object_get_data ((GObject *) account, "prd");

		if (prd) {
			priv = prd->priv;
			int pag_num;
			if (priv) {
			pag_num = gtk_notebook_page_num ( (GtkNotebook *)(data->parent), (GtkWidget *) priv->tab_dialog);
			gtk_notebook_remove_page ( (GtkNotebook *)(data->parent), pag_num); 
			}
		}	
	}
	return NULL;
}

static void
proxy_cancel(GtkWidget *button, EAccount *account)
{
	proxyDialog *prd = NULL;
	proxyDialogPrivate *priv;

	prd = g_object_get_data ((GObject *)account, "prd");
	priv = prd->priv;
	gtk_widget_destroy (priv->main);
	g_object_unref (priv->xml);
}


static void 
proxy_add_ok (GtkWidget *button, EAccount *account)
{
	proxyDialog *prd = NULL;
	proxyDialogPrivate *priv;
	
	prd = g_object_get_data ((GObject *)account, "prd");
	priv = prd->priv;

	if (proxy_dialog_store_widgets_data (account, PROXY_ADD_DIALOG) < 0)
		return;

	proxy_update_tree_view (account);
	gtk_widget_destroy (priv->main);
	g_object_unref (priv->xml);
}

static void 
proxy_edit_ok (GtkWidget *button, EAccount *account)
{
	proxyDialog *prd = NULL;
	proxyDialogPrivate *priv;
	
	prd = g_object_get_data ((GObject *)account, "prd");
	priv = prd->priv;

	if ( proxy_dialog_store_widgets_data (account, PROXY_EDIT_DIALOG) < 0)
		return;

	proxy_update_tree_view (account);
	gtk_widget_destroy (priv->main);
	g_object_unref (priv->xml);
}

static proxyHandler * 
proxy_get_item_from_list (EAccount *account, char *account_name)
{
	proxyDialogPrivate *priv;
	proxyDialog *prd = NULL;
	GList *list_iter;
	proxyHandler *iter;
	
	prd = g_object_get_data ((GObject *)account, "prd");    
	priv = prd->priv;
	list_iter = priv->proxy_list;

	for (;list_iter; list_iter = g_list_next(list_iter)) {        
	        iter = (proxyHandler *) list_iter->data;

		if ( g_str_equal (iter->proxy_email,account_name))
			return iter;
	}

	return NULL;
}

static void 
proxy_remove_account (GtkWidget *button, EAccount *account)
{
	GtkTreeIter iter;
        GtkTreeModel *model;
	proxyDialogPrivate *priv;
	proxyHandler *deleted;
	GtkTreeSelection* account_select;
	char *account_mailid;
	proxyDialog *prd = NULL;

	prd = g_object_get_data ((GObject *)account, "prd");    
	priv = prd->priv;
	account_select = gtk_tree_view_get_selection (GTK_TREE_VIEW (priv->tree));

        if (gtk_tree_selection_get_selected (account_select, &model, &iter)) {
                gtk_tree_model_get (model, &iter, ACCOUNT_NAME, &account_mailid, -1);
		account_mailid = g_strrstr (account_mailid, "\n") + 1;
		deleted = proxy_get_item_from_list (account, account_mailid);

		if (deleted != NULL)
			deleted->flags |= E_GW_PROXY_DELETED;

		proxy_update_tree_view (account);
        }
}

static void
addressbook_dialog_response (ENameSelectorDialog *name_selector_dialog, gint response, gpointer user_data)
{
	gtk_widget_hide (GTK_WIDGET (name_selector_dialog));
}

static void
addressbook_entry_changed (GtkWidget *entry, gpointer user_data)
{
}

static void
address_button_clicked (GtkButton *button, EAccount *account)
{
	proxyDialog *prd;
	proxyDialogPrivate *priv;
	ENameSelectorDialog *name_selector_dialog;

	prd = g_object_get_data ((GObject *)account, "prd");    
	priv = prd->priv;
	name_selector_dialog = e_name_selector_peek_dialog (priv->proxy_name_selector);
	gtk_widget_show (GTK_WIDGET (name_selector_dialog));
}

static void 
proxy_add_account (GtkWidget *button, EAccount *account)
{
	GtkButton *contacts, *cancel;
	proxyDialogPrivate *priv;
	GtkButton *okButton;
	ENameSelectorDialog *name_selector_dialog;
	ENameSelectorModel *name_selector_model;
	ENameSelectorEntry *name_selector_entry;
	GtkWidget *proxy_name, *name_box;
	proxyDialog *prd = NULL;

	prd = g_object_get_data ((GObject *)account, "prd");    
	priv = prd->priv;
	priv->xml = glade_xml_new (EVOLUTION_GLADEDIR "/proxy-add-dialog.glade", NULL, NULL);
	proxy_dialog_initialize_widgets (account);
	priv->main = glade_xml_get_widget (priv->xml, "ProxyAccessRights");
	okButton = (GtkButton *) glade_xml_get_widget (priv->xml,"proxy_button_ok");
	contacts = (GtkButton *) glade_xml_get_widget (priv->xml,"contacts");
	cancel = (GtkButton *) glade_xml_get_widget (priv->xml,"proxy_cancel");

	priv->proxy_name_selector = e_name_selector_new ();
	name_selector_dialog = e_name_selector_peek_dialog (priv->proxy_name_selector);

	g_signal_connect ((GtkWidget *)okButton, "clicked", G_CALLBACK (proxy_add_ok), account);
	g_signal_connect ((GtkWidget *)cancel, "clicked", G_CALLBACK (proxy_cancel), account);
	g_signal_connect ((GtkWidget *)contacts, "clicked", G_CALLBACK (address_button_clicked), account);
	g_signal_connect (name_selector_dialog, "response", G_CALLBACK (addressbook_dialog_response), account);
	gtk_widget_show (GTK_WIDGET (priv->main));		

	name_selector_model = e_name_selector_peek_model (priv->proxy_name_selector);
	e_name_selector_model_add_section (name_selector_model, "Add User", "Add User", NULL);

	name_selector_entry = e_name_selector_peek_section_entry (priv->proxy_name_selector, "Add User");
	g_signal_connect (name_selector_entry, "changed",
			  G_CALLBACK (addressbook_entry_changed), prd);

	proxy_name = glade_xml_get_widget (priv->xml, "proxy_account_name");
	name_box = glade_xml_get_widget (priv->xml, "proxy_name_box");
	gtk_widget_hide (proxy_name);
	gtk_container_add ((GtkContainer *)name_box, (GtkWidget *)name_selector_entry);
	gtk_widget_show ((GtkWidget *) name_selector_entry);
	gtk_widget_grab_focus ((GtkWidget *) name_selector_entry);
}

static void 
proxy_load_edit_dialog (EAccount *account, proxyHandler *edited)
{
	proxyDialogPrivate *priv;
	proxyDialog *prd = NULL;

	prd = g_object_get_data ((GObject *)account, "prd");    
	priv = prd->priv;
	gtk_entry_set_text ((GtkEntry *) priv->account_name, edited->proxy_email);
	gtk_widget_set_sensitive (priv->account_name, FALSE);
	
	if (edited->permissions & E_GW_PROXY_MAIL_READ)
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON (priv->mail_read), TRUE);

	if (edited->permissions & E_GW_PROXY_MAIL_WRITE)
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON (priv->mail_write), TRUE);

	if (edited->permissions & E_GW_PROXY_APPOINTMENT_READ)
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON (priv->app_read), TRUE);

	if (edited->permissions & E_GW_PROXY_APPOINTMENT_WRITE)
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON (priv->app_write), TRUE);

	if (edited->permissions & E_GW_PROXY_NOTES_READ)
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON (priv->note_read), TRUE);

	if (edited->permissions & E_GW_PROXY_NOTES_WRITE)
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON (priv->note_write), TRUE);

	if (edited->permissions & E_GW_PROXY_TASK_READ)
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON (priv->task_read), TRUE); 

	if (edited->permissions & E_GW_PROXY_TASK_WRITE)
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON (priv->task_write), TRUE); 

	if (edited->permissions & E_GW_PROXY_GET_ALARMS)
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON (priv->alarms), TRUE); 

	if (edited->permissions & E_GW_PROXY_GET_NOTIFICATIONS)
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON (priv->notifications), TRUE); 

	if (edited->permissions & E_GW_PROXY_MODIFY_FOLDERS)
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON (priv->options), TRUE); 

	if (edited->permissions & E_GW_PROXY_READ_PRIVATE)
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON (priv->private), TRUE); 
}	

static void 
proxy_edit_account (GtkWidget *button, EAccount *account)
{
	GtkTreeIter iter;
        GtkTreeModel *model;
	proxyDialogPrivate *priv;
	GtkTreeSelection* account_select;
	proxyHandler *edited;
	GtkButton *okButton, *proxyCancel;
	char *account_mailid;
	GtkWidget *contacts;
	proxyDialog *prd = NULL;

	prd = g_object_get_data ((GObject *)account, "prd");    
	priv = prd->priv;
	
	/*FIXME: If multiple properties dialogs are launched then the widgets lose data*/
		
	account_select = gtk_tree_view_get_selection (GTK_TREE_VIEW (priv->tree));

        if (gtk_tree_selection_get_selected (account_select, &model, &iter)) {
                gtk_tree_model_get (model, &iter, ACCOUNT_NAME, &account_mailid, -1);
		account_mailid = g_strrstr (account_mailid, "\n") + 1;
		edited = proxy_get_item_from_list (account, account_mailid);
		if (edited) {
			priv->xml = glade_xml_new (EVOLUTION_GLADEDIR "/proxy-add-dialog.glade", NULL, NULL);
			priv->main = glade_xml_get_widget (priv->xml, "ProxyAccessRights");
			proxy_dialog_initialize_widgets (account);
			okButton = (GtkButton *) glade_xml_get_widget (priv->xml,"proxy_button_ok");
			proxyCancel = (GtkButton *) glade_xml_get_widget (priv->xml,"proxy_cancel");
			contacts = glade_xml_get_widget (priv->xml, "contacts");

			g_signal_connect ((GtkWidget *)okButton, "clicked", G_CALLBACK (proxy_edit_ok), account);
			g_signal_connect ((GtkWidget *)proxyCancel, "clicked", G_CALLBACK (proxy_cancel), account);
			proxy_load_edit_dialog (account, edited);
			gtk_widget_hide (contacts);
			gtk_widget_show (GTK_WIDGET (priv->main));		
		}	
        }
}
