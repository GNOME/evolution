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
#include <bonobo/bonobo-control.h>
#include <bonobo/bonobo-widget.h>
#include <bonobo/bonobo-exception.h>
#include <bonobo/bonobo-ui-component.h>
#include <camel/camel-store.h>
#include <camel/camel-vee-store.h>
#include <string.h>
#include <glib.h>
#include <gtk/gtk.h>
#include <libgnome/gnome-i18n.h>
#include <gconf/gconf-client.h>
#include <e-util/e-config.h>
#include <mail/em-config.h>
#include <mail/em-popup.h>
#include <mail/em-folder-properties.h>
#include <mail/em-folder-tree.h>
#include <mail/em-folder-selector.h>
#include <mail/mail-mt.h>
#include <mail/em-vfolder-rule.h>
#include <filter/filter-rule.h>
#include <camel/providers/groupwise/camel-groupwise-store.h>
#include <camel/providers/groupwise/camel-groupwise-folder.h>
#include <e-gw-container.h>
#include <e-gw-connection.h>
#include <glade/glade.h>
#include <libgnomeui/libgnomeui.h>
#include "share-folder.h"
#define d(x)

ShareFolder *common = NULL;
extern CamelSession *session;

struct ShareInfo {
	GtkWidget *d;
	ShareFolder *sf;
	EMFolderTreeModel *model;
	EMFolderSelector *emfs;
};

void 
shared_folder_check (EPlugin *ep, EConfigTarget *target)
{
	printf ("check **********\n");
}


	
void 
shared_folder_commit (EPlugin *ep, EConfigTarget *target)
{
	if (common) {
		share_folder (common);
		g_object_run_dispose (common);
	}
	printf ("commit **********\n");
}


void 
shared_folder_abort (EPlugin *ep, EConfigTarget *target)
{
	if (common) {
		share_folder (common);
		g_object_run_dispose (common);
	}
	printf ("aborttttttt**********\n");
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
	void (* done) (CamelFolderInfo *fi, void *user_data);
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

	CamelGroupwiseStore *gw_store = CAMEL_GROUPWISE_STORE (m->store) ;
	CamelGroupwiseStorePrivate *priv = gw_store->priv ;

	if (m->done) {
	(ssi->sf)->container_id = g_strdup (container_id_lookup (priv, m->name));
	(ssi->sf)->cnc = cnc_lookup (priv);
	g_print("\n\n\name :%s\n\nid: %s", m->name, (ssi->sf)->container_id);
		 share_folder(ssi->sf);
		m->done (m->fi, m->user_data);
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
new_folder_created_cb (CamelFolderInfo *fi, void *user_data)
{
	struct ShareInfo *ssi = (struct ShareInfo *) user_data;
	EMFolderSelector *emfs = ssi->emfs;

	if (fi)	{
	gtk_widget_destroy ((GtkWidget *) emfs);
	gtk_widget_destroy ((GtkWidget *) ssi->d);
	}
	
	g_object_unref (emfs);
}

static int
create_folder (CamelStore *store, const char *full_name, void (* done) (CamelFolderInfo *fi, void *user_data), void *user_data)
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

	/* HACK: we need to create vfolders using the vfolder editor */
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

	ssi = g_new0(struct ShareInfo, 1);
	if (response != GTK_RESPONSE_OK) {
		gtk_widget_destroy ((GtkWidget *) emfs);
		return;
	}
	
	users_dialog = gtk_dialog_new_with_buttons (
			_("Users"), NULL, GTK_DIALOG_DESTROY_WITH_PARENT | GTK_DIALOG_NO_SEPARATOR, GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,GTK_STOCK_OK, GTK_RESPONSE_OK, NULL);
	w = gtk_label_new_with_mnemonic (_("Enter the users and set permissions"));
	gtk_widget_show(w);
	gtk_box_pack_start(GTK_BOX (GTK_DIALOG (users_dialog)->vbox), (GtkWidget *) w, TRUE, TRUE, 6);
	ssi->sf = share_folder_new (NULL, NULL);
	((ssi->sf)->table)->parent = NULL;
	gtk_widget_set_sensitive (GTK_WIDGET ((ssi->sf)->table), TRUE);	
	ssi->model = model;
	ssi->emfs = emfs;
	gtk_box_pack_end(GTK_BOX (GTK_DIALOG (users_dialog)->vbox), (GtkWidget *) (ssi->sf)->table, TRUE, TRUE, 6);
	gtk_widget_hide((GtkWidget*) emfs);
	gtk_window_resize (GTK_WINDOW (users_dialog), 350, 300);
	gtk_widget_show(users_dialog);
	g_signal_connect (users_dialog, "response", G_CALLBACK (users_dialog_response), ssi);
	
	return ;
}

GtkWidget *
org_gnome_create_option(EPlugin *ep, EMPopupTargetFolder *target)
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
	gchar *sub = NULL;
	EGwConnection *cnc;
	ShareFolder *sharing_tab;
	EMConfigTargetFolder *target=  (EMConfigTargetFolder *)hook_data->config->target;

	folderuri = g_strdup(target->uri);
	account = g_strrstr(folderuri, "groupwise");
	
	if(account){
		sub = g_strrstr(folderuri, "#");
		if(sub == NULL)
			sub = g_strrstr(folderuri, "/");
		sub++;
		CamelFolder *folder = target->folder ;
		CamelGroupwiseFolder *gw_folder = CAMEL_GROUPWISE_FOLDER(folder) ;
		CamelGroupwiseStore *gw_store = CAMEL_GROUPWISE_STORE (folder->parent_store) ;
		CamelGroupwiseStorePrivate *priv = gw_store->priv ;
		
		if (priv && sub) {

			id = g_strdup (container_id_lookup(priv,sub));
			cnc = cnc_lookup (priv);
		} else {
			cnc = NULL;
			id = NULL;
		}
		
		if (cnc && id)
			sharing_tab = share_folder_new (cnc, id);
		else 
			return NULL;

		g_free (folderuri);
		gtk_notebook_append_page((GtkNotebook *) hook_data->parent, sharing_tab->vbox, gtk_label_new_with_mnemonic N_("Sharing"));
		common = sharing_tab;

		return sharing_tab;
	} else
		return NULL;
}

