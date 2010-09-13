/*
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) version 3.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with the program; if not, see <http://www.gnu.org/licenses/>
 *
 *
 * Authors:
 *		Vivek Jain <jvivek@novell.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif
#include <string.h>
#include <glib.h>
#include <gtk/gtk.h>
#include <glib/gi18n.h>
#include <e-util/e-config.h>
#include <mail/em-config.h>
#include <mail/em-folder-properties.h>
#include <mail/em-folder-tree.h>
#include <mail/em-folder-selector.h>
#include <mail/mail-mt.h>
#include <mail/mail-config.h>
#include <mail/mail-vfolder.h>
#include <mail/em-utils.h>
#include <mail/em-vfolder-rule.h>
#include <filter/e-filter-rule.h>
#include <e-gw-container.h>
#include <e-gw-connection.h>
#include <shell/e-shell-sidebar.h>
#include "share-folder.h"
#include "gw-ui.h"

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
void shared_folder_commit (EPlugin *ep, EConfigTarget *tget);
void shared_folder_abort (EPlugin *ep, EConfigTarget *target);

static void refresh_folder_tree (EMFolderTreeModel *model, CamelStore *store);

static void
refresh_folder_tree (EMFolderTreeModel *model, CamelStore *store)
{
	gchar *uri;
	EAccount *account;
	CamelProvider *provider;

	uri = camel_url_to_string (((CamelService *) store)->url, CAMEL_URL_HIDE_ALL);
	account = mail_config_get_account_by_source_url (uri);
	if (!account) {
		return;
	}

	uri = account->source->url;
	em_folder_tree_model_remove_store (model, store);

	provider = camel_provider_get (uri, NULL);
	if (provider == NULL)
		return;

	if (!(provider->flags & CAMEL_PROVIDER_IS_STORAGE))
		return;
	em_folder_tree_model_add_store (model, store, account->name);
	/* g_object_unref (store); */
}

void
shared_folder_commit (EPlugin *ep, EConfigTarget *tget)
{
	EMConfigTargetFolder *target =  (EMConfigTargetFolder *)tget->config->target;
	CamelStore *parent_store;
	EMFolderTreeModel *model = NULL; /*mail_component_peek_tree_model (mail_component_peek ())*/;

	parent_store = camel_folder_get_parent_store (target->folder);

	if (common) {
		share_folder (common);
		refresh_folder_tree (model, parent_store);
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
	MailMsg base;

	/* input data */
	CamelStore *store;
	gchar *full_name;
	gchar *parent;
	gchar *name;

	/* output data */
	CamelFolderInfo *fi;

	/* callback data */
	void (* done) (struct _EMCreateFolder *m, gpointer user_data);
	gpointer user_data;
};

static gchar *
create_folder_desc (struct _EMCreateFolder *m)
{
	return g_strdup_printf (_("Creating folder '%s'"), m->full_name);
}

static void
create_folder_exec (struct _EMCreateFolder *m)
{
	d(printf ("creating folder parent='%s' name='%s' full_name='%s'\n", m->parent, m->name, m->full_name));

	if ((m->fi = camel_store_create_folder (m->store, m->parent, m->name, &m->base.error))) {
		if (camel_store_supports_subscriptions (m->store))
			camel_store_subscribe_folder (m->store, m->full_name, &m->base.error);
	}
}

static void
create_folder_done (struct _EMCreateFolder *m)
{
	struct ShareInfo *ssi = (struct ShareInfo *) m->user_data;
	CamelStore *store = CAMEL_STORE (m->store);
	EGwConnection *ccnc;

	if (m->done) {
		ccnc = get_cnc (store);
		if (E_IS_GW_CONNECTION (ccnc)) {
			(ssi->sf)->cnc = ccnc;

			(ssi->sf)->container_id = g_strdup (get_container_id ((ssi->sf)->cnc, m->full_name));
			share_folder(ssi->sf);
		}

		m->done (m, m->user_data);
	}
}

static void
create_folder_free (struct _EMCreateFolder *m)
{
	camel_store_free_folder_info (m->store, m->fi);
	g_object_unref (m->store);
	g_free (m->full_name);
	g_free (m->parent);
	g_free (m->name);
}

static MailMsgInfo create_folder_info = {
	sizeof (struct _EMCreateFolder),
	(MailMsgDescFunc) create_folder_desc,
	(MailMsgExecFunc) create_folder_exec,
	(MailMsgDoneFunc) create_folder_done,
	(MailMsgFreeFunc) create_folder_free
};

static void
new_folder_created_cb (struct _EMCreateFolder *m, gpointer user_data)
{
	struct ShareInfo *ssi = (struct ShareInfo *) user_data;
	EMFolderSelector *emfs = ssi->emfs;
	if (m->fi) {
		refresh_folder_tree (ssi->model, m->store);
		gtk_widget_destroy ((GtkWidget *) emfs);
		gtk_widget_destroy ((GtkWidget *) ssi->d);
	}

	g_object_unref (emfs);
}

static gint
create_folder (CamelStore *store, const gchar *full_name, void (* done) (struct _EMCreateFolder *m, gpointer user_data), gpointer user_data)
{
	gchar *name, *namebuf = NULL;
	struct _EMCreateFolder *m;
	const gchar *parent;
	gint id;

	namebuf = g_strdup (full_name);
	if (!(name = strrchr (namebuf, '/'))) {
		name = namebuf;
		parent = "";
	} else {
		*name++ = '\0';
		parent = namebuf;
	}

	m = mail_msg_new (&create_folder_info);
	g_object_ref (store);
	m->store = store;
	m->full_name = g_strdup (full_name);
	m->parent = g_strdup (parent);
	m->name = g_strdup (name);
	m->user_data = (struct ShareInfo *) user_data;
	m->done = done;
	g_free (namebuf);
	id = m->base.seq;
	mail_msg_unordered_push (m);

	return id;
}

static void
users_dialog_response(GtkWidget *dialog, gint response, struct ShareInfo *ssi)
{
	struct _EMFolderTreeModelStoreInfo *si;
	EMFolderSelector *emfs = ssi->emfs;
	const gchar *uri, *path;
	CamelStore *store;

	if (response != GTK_RESPONSE_OK) {
		gtk_widget_destroy ((GtkWidget *) emfs);
		gtk_widget_destroy(dialog);
		return;
	}

	uri = em_folder_selector_get_selected_uri (emfs);
	path = em_folder_selector_get_selected_path (emfs);

	d(printf ("Creating new folder: %s (%s)\n", path, uri));

	store = (CamelStore *) camel_session_get_service (
		session, uri, CAMEL_PROVIDER_STORE, NULL);
	if (store == NULL)
		return;

	if (!(si = em_folder_tree_model_lookup_store_info (ssi->model, store))) {
		g_assert_not_reached ();
		g_object_unref (store);
		return;
	}

	if (CAMEL_IS_VEE_STORE(store)) {
		EMVFolderRule *rule;

		/* ensures vfolder is running */
		vfolder_load_storage ();

		rule = em_vfolder_rule_new();
		e_filter_rule_set_name((EFilterRule *)rule, path);
		vfolder_gui_add_rule(rule);
		gtk_widget_destroy((GtkWidget *)emfs);
	} else {
		g_object_ref (emfs);
		ssi->d = dialog;
		create_folder (si->store, path, new_folder_created_cb, ssi);

	}
	g_object_unref (store);
}

static void
new_folder_response (EMFolderSelector *emfs, gint response, EMFolderTreeModel *model)
{
	GtkWidget *users_dialog;
	GtkWidget *content_area;
	GtkWidget *w;
	struct ShareInfo *ssi;
	const gchar *uri;
	EGwConnection *cnc;
	CamelStore *store;

	ssi = g_new0(struct ShareInfo, 1);
	if (response != GTK_RESPONSE_OK) {
		gtk_widget_destroy ((GtkWidget *) emfs);
		return;
	}

	/* i want store at this point to get cnc not sure proper or not*/
	uri = em_folder_selector_get_selected_uri (emfs);
	store = (CamelStore *) camel_session_get_service (
		session, uri, CAMEL_PROVIDER_STORE, NULL);
	if (store == NULL)
		return;

	cnc = get_cnc (store);
	users_dialog = gtk_dialog_new_with_buttons (
			_("Users"), NULL, GTK_DIALOG_DESTROY_WITH_PARENT | GTK_DIALOG_NO_SEPARATOR, GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,GTK_STOCK_OK, GTK_RESPONSE_OK, NULL);
	w = gtk_label_new_with_mnemonic (_("Enter the users and set permissions"));
	gtk_widget_show(w);
	content_area = gtk_dialog_get_content_area (GTK_DIALOG (users_dialog));
	gtk_box_pack_start(GTK_BOX (content_area), (GtkWidget *) w, TRUE, TRUE, 6);
	ssi->sf = share_folder_new (cnc, NULL);
	gtk_widget_set_sensitive (GTK_WIDGET ((ssi->sf)->table), TRUE);
	ssi->model = model;
	ssi->emfs = emfs;
	gtk_widget_reparent (GTK_WIDGET ((ssi->sf)->table), content_area);
	gtk_widget_hide((GtkWidget*) emfs);
	gtk_window_resize (GTK_WINDOW (users_dialog), 350, 300);
	gtk_widget_show(users_dialog);
	g_signal_connect (users_dialog, "response", G_CALLBACK (users_dialog_response), ssi);

	g_object_unref (store);
	return;

}

void
gw_new_shared_folder_cb (GtkAction *action, EShellView *shell_view)
{
	EMFolderTree *folder_tree;
	GtkWidget *dialog;
	gchar *uri;
	gpointer parent;

	parent = e_shell_view_get_shell_window (shell_view);
	folder_tree = (EMFolderTree *) em_folder_tree_new ();
	emu_restore_folder_tree_state (folder_tree);

	dialog = em_folder_selector_create_new (parent, folder_tree, 0, _("Create folder"), _("Specify where to create the folder:"));
	uri = em_folder_tree_get_selected_uri (folder_tree);
	if (uri != NULL)
		em_folder_selector_set_selected ((EMFolderSelector *) dialog, uri);
	g_free(uri);

	g_signal_connect (dialog, "response", G_CALLBACK (new_folder_response), gtk_tree_view_get_model (GTK_TREE_VIEW (folder_tree)));
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

	folder_name = g_strdup (camel_folder_get_full_name (folder));
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
		CamelStore *parent_store;

		parent_store = camel_folder_get_parent_store (folder);
		cnc = get_cnc (parent_store);

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
		const gchar *uri, *property_value, *server_name, *user, *port;
		gchar *use_ssl;
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
		if (property_value == NULL)
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
			gchar *http_uri = g_strconcat ("http://", uri + 8, NULL);
			cnc = e_gw_connection_new (http_uri, user, service->url->passwd);
			g_free (http_uri);
		}
		g_free (use_ssl);
		use_ssl = NULL;

		return cnc;

}

gchar *
get_container_id(EGwConnection *cnc, const gchar *fname)
{
	GList *container_list = NULL;
	gchar *id = NULL;
	gchar *name;
	gchar **names;
	gint i = 0, parts = 0;

	names = g_strsplit (fname, "/", -1);
	if (names) {
		while (names[parts])
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
	}

	if (names)
		g_strfreev (names);
	return id;
}
