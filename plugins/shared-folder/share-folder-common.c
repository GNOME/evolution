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
#include <libebook/e-destination.h>

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
#include <camel/providers/groupwise/camel-groupwise-store.h>
#include <camel/providers/groupwise/camel-groupwise-folder.h>
#include <e-gw-container.h>
#include <e-gw-connection.h>
#include <glade/glade.h>
#include "share-folder.h"
#define SELECT_NAMES_OAFID "OAFIID:GNOME_Evolution_Addressbook_SelectNames:" BASE_VERSION

typedef CORBA_Object GNOME_Evolution_Addressbook_SelectNames;
ShareFolder *common = NULL;



void 
shared_folder_check (EPlugin *ep, EConfigTarget *target)
{
	printf ("check **********\n");
}


	
void 
shared_folder_commit (EPlugin *ep, EConfigTarget *target)
{

	gchar *mesg = "Folder shared to you";
	gchar *sub = "Shared folder notification";
	ShareFolder *sf;
	gchar *check = NULL;
	if(common)
		share_folder (common);
	g_object_run_dispose(common);

	printf ("commit **********\n");
}


void 
shared_folder_abort (EPlugin *ep, EConfigTarget *target)
{
	printf ("aborttttttt**********\n");
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
	EMConfigTargetPrefs *target1 = (EMConfigTargetPrefs *) hook_data->config->target;
	EMConfigTargetFolder *target2=  (EMConfigTargetFolder *)hook_data->config->target;	
	folderuri=g_strdup(target2->uri);
	account = g_strrstr(folderuri, "groupwise");
	if(account)
	{
		sub = g_strrstr(folderuri, "#");
		if(sub == NULL)
			sub = g_strrstr(folderuri, "/");
		sub++;
		g_print("\n\nTHE URI OF THE FOLDER%s\n\n %s\n\n",target2->uri,sub);
		CamelFolder *folder = target2->folder ;
		CamelGroupwiseFolder *gw_folder = CAMEL_GROUPWISE_FOLDER(folder) ;
		CamelGroupwiseStore *gw_store = CAMEL_GROUPWISE_STORE (folder->parent_store) ;
		CamelGroupwiseStorePrivate *priv = gw_store->priv ;

		id = g_strdup (container_id_lookup(priv,sub));
		cnc = cnc_lookup (priv);
		g_free (sub);
		g_free (folderuri);
		
		sharing_tab = share_folder_new (cnc, id);
		gtk_notebook_append_page((GtkNotebook *) hook_data->parent, sharing_tab->vbox, gtk_label_new_with_mnemonic N_("Sharing"));
		common = sharing_tab;
		

		return sharing_tab;
	} else
		return NULL;
		

}

