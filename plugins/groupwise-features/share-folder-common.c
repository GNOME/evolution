/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 *  Authors: Vivek Jain <jvivek@novell.com>
 *
 *  Copyright 2004 Novell, Inc.
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
#include <string.h>
#include <glib.h>
#include <gtk/gtk.h>
#include <libgnome/gnome-i18n.h>
#include <e-util/e-config.h>
#include <mail/em-config.h>
#include <mail/em-popup.h>
#include <mail/em-folder-properties.h>
#include <mail/em-folder-tree.h>
#include <mail/em-folder-selector.h>
#include <mail/mail-mt.h>
#include <mail/mail-component.h>
#include <mail/mail-config.h>
#include <mail/em-vfolder-rule.h>
#include <filter/filter-rule.h>
#include <camel/camel-store.h>
#include <camel/camel-session.h>
#include <camel/camel-store.h>
#include <camel/camel-vee-store.h>
#include <camel/camel-folder.h>
#include <e-gw-container.h>
#include <e-gw-connection.h>
#include <glade/glade.h>
#include <libgnomeui/libgnomeui.h>
#include "share-folder.h"
#define d(x)

ShareFolder *common = NULL;
CamelSession *session;
struct ShareInfo {
	GtkWidget *d;
	ShareFolder *sf;
	EMFolderTreeModel *model;
	EMFolderSelector *emfs;
};
	
GtkWidget * org_gnome_shared_folder_factory (EPlugin *ep, EConfigHookItemFactoryData *hook_data);
void org_gnome_create_option(EPlugin *ep, EMPopupTargetFolder *target);
static void create_shared_folder(EPopup *ep, EPopupItem *p, void *data);
static void popup_free (EPopup *ep, GSList *items, void *data);
void shared_folder_commit (EPlugin *ep, EConfigTarget *tget);
void shared_folder_abort (EPlugin *ep, EConfigTarget *target);

static void refresh_folder_tree (EMFolderTreeModel *model, CamelStore *store);

static void 
refresh_folder_tree (EMFolderTreeModel *model, CamelStore *store)
{
	gchar *uri;
	EAccount *account;
	CamelException ex;
	CamelProvider *provider;

	uri = camel_url_to_string (((CamelService *) store)->url, CAMEL_URL_HIDE_ALL);
	account = mail_config_get_account_by_source_url (uri);
	uri = account->source->url;
	em_folder_tree_model_remove_store (model, store);

	camel_exception_init (&ex);
	if (!(provider = camel_provider_get(uri, &ex))) {
		camel_exception_clear (&ex);
		return;
	}
	if (!(provider->flags & CAMEL_PROVIDER_IS_STORAGE))
		return;
	em_folder_tree_model_add_store (model, store, account->name);
	//camel_object_unref (store);
}

void 
shared_folder_commit (EPlugin *ep, EConfigTarget *tget)
{
	EMConfigTargetFolder *target =  (EMConfigTargetFolder *)tget->config->target;
	CamelFolder *folder = target->folder;
	CamelStore *store = folder->parent_store;	
	EMFolderTreeModel *model = mail_component_peek_tree_model (mail_component_peek ());
	if (common) {
		share_folder (common);
		refresh_folder_tree (model, store);
		g_object_run_dispose ((GObject *)common);
		common = NULL;
	}
}

void 
shared_folder_abort (EPlugin *ep, EConfigTarget *target)
{
	if (common) {
		g_object_run_dispose ((GObject *)common);
		common = NULL;
	}
}

struct _EMCreateFolder {
	struct _mail_msg msg;
	
	/* input data */
	CamelStore *store;
	char *full_name;
	char *parent;
	char *name;
	
	/* output data */
	CamelFolderInfo *fi;
	
	/* callback data */
	void (* done) (struct _EMCreateFolder *m, void *user_data);
	void *user_data;
};
	
static char *
create_folder__desc (struct _mail_msg *mm, int done)
{
	struct _EMCreateFolder *m = (struct _EMCreateFolder *) mm;
	
	return g_strdup_printf (_("Creating folder `%s'"), m->full_name);
}

static void
create_folder__create (struct _mail_msg *mm)
{
	struct _EMCreateFolder *m = (struct _EMCreateFolder *) mm;
	
	d(printf ("creating folder parent='%s' name='%s' full_name='%s'\n", m->parent, m->name, m->full_name));
	
	if ((m->fi = camel_store_create_folder (m->store, m->parent, m->name, &mm->ex))) {
		if (camel_store_supports_subscriptions (m->store))
			camel_store_subscribe_folder (m->store, m->full_name, &mm->ex);
	}
}

static void
create_folder__created (struct _mail_msg *mm)
{
	struct _EMCreateFolder *m = (struct _EMCreateFolder *) mm;
	struct ShareInfo *ssi = (struct ShareInfo *) m->user_data;
	CamelStore *store = CAMEL_STORE (m->store) ;
	EGwConnection *ccnc;
	
	if (m->done) {
		ccnc = get_cnc (store);
		if(E_IS_GW_CONNECTION (ccnc)) {
			(ssi->sf)->cnc = ccnc;

			(ssi->sf)->container_id = g_strdup (get_container_id ((ssi->sf)->cnc, m->full_name));
			share_folder(ssi->sf);
		}

		m->done (m, m->user_data);
	}
}
	
static void
create_folder__free (struct _mail_msg *mm)
{
	struct _EMCreateFolder *m = (struct _EMCreateFolder *) mm;
	
	camel_store_free_folder_info (m->store, m->fi);
	camel_object_unref (m->store);
	g_free (m->full_name);
	g_free (m->parent);
	g_free (m->name);
}

static struct _mail_msg_op create_folder_op = {
	create_folder__desc,
	create_folder__create,
	create_folder__created,
	create_folder__free,
};

static void
new_folder_created_cb (struct _EMCreateFolder *m, void *user_data)
{
	struct ShareInfo *ssi = (struct ShareInfo *) user_data;
	EMFolderSelector *emfs = ssi->emfs;
	if (m->fi){
		refresh_folder_tree (ssi->model, m->store);
		gtk_widget_destroy ((GtkWidget *) emfs);
		gtk_widget_destroy ((GtkWidget *) ssi->d);
	}

	g_object_unref (emfs);
}

static int
create_folder (CamelStore *store, const char *full_name, void (* done) (struct _EMCreateFolder *m, void *user_data), void *user_data)
{
	char *name, *namebuf = NULL;
	struct _EMCreateFolder *m;
	const char *parent;
	int id;

	namebuf = g_strdup (full_name);
	if (!(name = strrchr (namebuf, '/'))) {
		name = namebuf;
		parent = "";
	} else {
		*name++ = '\0';
		parent = namebuf;
	}
	
	m = mail_msg_new (&create_folder_op, NULL, sizeof (struct _EMCreateFolder));
	camel_object_ref (store);
	m->store = store;
	m->full_name = g_strdup (full_name);
	m->parent = g_strdup (parent);
	m->name = g_strdup (name);
	m->user_data = (struct ShareInfo *) user_data;
	m->done = done;
	g_free (namebuf);
	id = m->msg.seq;
	e_thread_put (mail_thread_new, (EMsg *) m);
		
	return id;
}

static void 
users_dialog_response(GtkWidget *dialog, int response, struct ShareInfo *ssi)
{
	struct _EMFolderTreeModelStoreInfo *si;
	EMFolderSelector *emfs = ssi->emfs;
	const char *uri, *path;
	CamelException ex;
	CamelStore *store;

	if (response != GTK_RESPONSE_OK) {
		gtk_widget_destroy ((GtkWidget *) emfs);
		gtk_widget_destroy(dialog);
		return;
	}

	uri = em_folder_selector_get_selected_uri (emfs);
	path = em_folder_selector_get_selected_path (emfs);

	d(printf ("Creating new folder: %s (%s)\n", path, uri));
	
	camel_exception_init (&ex);
	if (!(store = (CamelStore *) camel_session_get_service (session, uri, CAMEL_PROVIDER_STORE, &ex))) {
		camel_exception_clear (&ex);
		return;
	}
	
	if (!(si = g_hash_table_lookup ((ssi->model)->store_hash, store))) {
		g_assert_not_reached ();
		camel_object_unref (store);
		return;
	}

	if (CAMEL_IS_VEE_STORE(store)) {
		EMVFolderRule *rule;

		rule = em_vfolder_rule_new();
		filter_rule_set_name((FilterRule *)rule, path);
		vfolder_gui_add_rule(rule);
		gtk_widget_destroy((GtkWidget *)emfs);
	} else {
		g_object_ref (emfs);
		ssi->d = dialog;
		create_folder (si->store, path, new_folder_created_cb, ssi);

	}
	camel_object_unref (store);
}

static void
new_folder_response (EMFolderSelector *emfs, int response, EMFolderTreeModel *model)
{
	GtkWidget *users_dialog;
	GtkWidget *w;
	struct ShareInfo *ssi;
	const char *uri;
	EGwConnection *cnc;
	CamelException ex;
	CamelStore *store;

	ssi = g_new0(struct ShareInfo, 1);
	if (response != GTK_RESPONSE_OK) {
		gtk_widget_destroy ((GtkWidget *) emfs);
		return;
	}
	
	/* i want store at this point to get cnc not sure proper or not*/
	uri = em_folder_selector_get_selected_uri (emfs);
	camel_exception_init (&ex);
	if (!(store = (CamelStore *) camel_session_get_service (session, uri, CAMEL_PROVIDER_STORE, &ex))) {
		camel_exception_clear (&ex);
		return;
	}

	cnc = get_cnc (store);
	users_dialog = gtk_dialog_new_with_buttons (
			_("Users"), NULL, GTK_DIALOG_DESTROY_WITH_PARENT | GTK_DIALOG_NO_SEPARATOR, GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,GTK_STOCK_OK, GTK_RESPONSE_OK, NULL);
	w = gtk_label_new_with_mnemonic (_("Enter the users and set permissions"));
	gtk_widget_show(w);
	gtk_box_pack_start(GTK_BOX (GTK_DIALOG (users_dialog)->vbox), (GtkWidget *) w, TRUE, TRUE, 6);
	ssi->sf = share_folder_new (cnc, NULL);
	gtk_widget_set_sensitive (GTK_WIDGET ((ssi->sf)->table), TRUE);	
	ssi->model = model;
	ssi->emfs = emfs;
	gtk_widget_reparent (GTK_WIDGET ((ssi->sf)->table), GTK_DIALOG (users_dialog)->vbox);
	gtk_widget_hide((GtkWidget*) emfs);
	gtk_window_resize (GTK_WINDOW (users_dialog), 350, 300);
	gtk_widget_show(users_dialog);
	g_signal_connect (users_dialog, "response", G_CALLBACK (users_dialog_response), ssi);
	
	camel_object_unref (store);
	return ;

}

static EPopupItem popup_items[] = {
{ E_POPUP_ITEM, "20.emc.001", N_("New _Shared Folder..."), create_shared_folder, NULL, "stock_new-dir", 0, EM_POPUP_FOLDER_INFERIORS }
};

static void 
popup_free (EPopup *ep, GSList *items, void *data)
{
g_slist_free (items);
}

void 
org_gnome_create_option(EPlugin *ep, EMPopupTargetFolder *t)
{
	GSList *menus = NULL;
	int i = 0;
	static int first = 0;
	
	if (! g_strrstr (t->uri, "groupwise://"))
		return ;
	
	/* for translation*/
	if (!first) {
		popup_items[0].label =  _(popup_items[0].label);
	
	}
	
	first++;
	
	for (i = 0; i < sizeof (popup_items) / sizeof (popup_items[0]); i++)
		menus = g_slist_prepend (menus, &popup_items[i]);
	
	e_popup_add_items (t->target.popup, menus, NULL, popup_free, NULL);
       	
}
	
static void 
create_shared_folder(EPopup *ep, EPopupItem *p, void *data)
{

	EMFolderTreeModel *model;
	EMFolderTree *folder_tree;
	GtkWidget *dialog ;
	char *uri;
	
	model = mail_component_peek_tree_model (mail_component_peek ());
	folder_tree = (EMFolderTree *) em_folder_tree_new_with_model (model);
	dialog = em_folder_selector_create_new (folder_tree, 0, _("Create folder"), _("Specify where to create the folder:"));
	uri = em_folder_tree_get_selected_uri(folder_tree);
	em_folder_selector_set_selected ((EMFolderSelector *) dialog, uri);
	g_free(uri);
	g_signal_connect (dialog, "response", G_CALLBACK (new_folder_response), model);
	gtk_window_set_title (GTK_WINDOW (dialog), "New Shared Folder" );
	gtk_widget_show(dialog);
	
}

GtkWidget *
org_gnome_shared_folder_factory (EPlugin *ep, EConfigHookItemFactoryData *hook_data)
{

	gchar *folderuri = NULL;
	gchar *account = NULL;
	gchar *id = NULL;
	gchar *folder_name = NULL;
	EGwConnection *cnc;
	ShareFolder *sharing_tab;
	EMConfigTargetFolder *target=  (EMConfigTargetFolder *)hook_data->config->target;
	CamelFolder *folder = target->folder;
	
	folder_name = g_strdup (folder->full_name);
	folderuri = g_strdup(target->uri);
	if (folderuri && folder_name) 
		account = g_strrstr(folderuri, "groupwise");
	else
		return NULL;

	 /* This is kind of bad..but we don't have types for all these folders.*/

	if ( !( strcmp (folder_name, "Mailbox") 
	     && strcmp (folder_name, "Calendar") 
	     && strcmp (folder_name, "Contacts") 
	     && strcmp (folder_name, "Documents") 
	     && strcmp (folder_name, "Authored") 
	     && strcmp (folder_name, "Default Library") 
	     && strcmp (folder_name, "Work In Progress") 
	     && strcmp (folder_name, "Cabinet") 
	     && strcmp (folder_name, "Sent Items") 
	     && strcmp (folder_name, "Trash") 
	     && strcmp (folder_name, "Checklist"))) {

		g_free (folderuri);
		return NULL;
	}

	if (account) {
		CamelStore *store = folder->parent_store;	
		cnc = get_cnc (store);	
	
		if (E_IS_GW_CONNECTION (cnc)) 
			id = get_container_id (cnc, folder_name);
		else
			g_warning("Could not Connnect\n");
		
		if (cnc && id)
			sharing_tab = share_folder_new (cnc, id);
		else 
			return NULL;
		
		gtk_notebook_append_page((GtkNotebook *) hook_data->parent, (GtkWidget *) sharing_tab->vbox, gtk_label_new_with_mnemonic N_("Sharing"));
		common = sharing_tab;
		g_free (folderuri);
		return GTK_WIDGET (sharing_tab);
	} else
		return NULL;
}

EGwConnection * 
get_cnc (CamelStore *store)
{
		EGwConnection *cnc;
		const char *uri, *property_value, *server_name, *user, *port;
		char *use_ssl;
		CamelService *service;
		CamelURL *url;
		
		if (!store)
			return  NULL;

		service = CAMEL_SERVICE(store);
		url = service->url;
		server_name = g_strdup (url->host);
		user = g_strdup (url->user);
		property_value =  camel_url_get_param (url, "soap_port");
		use_ssl = g_strdup (camel_url_get_param (url, "use_ssl"));
		if(property_value == NULL)
			port = g_strdup ("7191");
		else if (strlen(property_value) == 0)
			port = g_strdup ("7191");
		else
			port = g_strdup (property_value);

		if (use_ssl && !g_str_equal (use_ssl, "never"))
			uri = g_strconcat ("https://", server_name, ":", port, "/soap", NULL);	
		else
			uri = g_strconcat ("http://", server_name, ":", port, "/soap", NULL);

		cnc = e_gw_connection_new (uri, user, service->url->passwd);
		if (!E_IS_GW_CONNECTION(cnc) && use_ssl && g_str_equal (use_ssl, "when-possible")) {
			char *http_uri = g_strconcat ("http://", uri + 8, NULL);
			cnc = e_gw_connection_new (http_uri, user, service->url->passwd);
			g_free (http_uri);
		}
		g_free (use_ssl);
		use_ssl = NULL;

		return cnc;

}

gchar *
get_container_id(EGwConnection *cnc, gchar *fname)
{
	GList *container_list = NULL;	
	gchar *id = NULL;
	gchar *name;
	gchar **names;
	int i = 0, parts = 0;

	names = g_strsplit (fname, "/", -1);
	if(names){
		while (names [parts])
			parts++;
		fname = names[i]; 
	}

	/* get list of containers */
	if (e_gw_connection_get_container_list (cnc, "folders", &(container_list)) == E_GW_CONNECTION_STATUS_OK) {
		GList *container = NULL;

		for (container = container_list; container != NULL; container = container->next) {
			name = g_strdup (e_gw_container_get_name (container->data));
			/* if Null is passed as name then we return top lavel id*/
			if (fname == NULL) {
				id = g_strdup (e_gw_container_get_id (container->data));
				break;
			} else if (!strcmp (name, fname)) {
				if (i == parts - 1) {
					id = g_strdup (e_gw_container_get_id (container->data));
					break;
				} else
					fname = names[++i];
			}
			g_free (name);
		}
		e_gw_connection_free_container_list (container_list);
		if (names)
		g_strfreev(names);
	}
	return id;
}
