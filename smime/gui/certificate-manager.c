/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 *  Authors: Chris Toshok <toshok@ximian.com>
 *
 *  Copyright (C) 2003 Ximian, Inc. (www.ximian.com)
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

#define GLADE_FILE_NAME "smime-ui.glade"

#include <gtk/gtkcontainer.h>
#include <gtk/gtktreeview.h>
#include <gtk/gtktreestore.h>
#include <gtk/gtktreemodelsort.h>
#include <gtk/gtkcellrenderertext.h>

#include <libgnome/gnome-i18n.h>

#include <glade/glade.h>
#include "evolution-config-control.h"
#include "certificate-manager.h"

#include "e-cert.h"

#include "nss.h"
#include <cms.h>
#include <cert.h>
#include <certdb.h>
#include <pkcs11.h>
#include <pk11func.h>

typedef struct {
	GladeXML *gui;

	GtkWidget *yourcerts_treeview;
	GtkTreeStore *yourcerts_treemodel;
	GHashTable *yourcerts_root_hash;

	GtkWidget *contactcerts_treeview;
	GtkTreeStore *contactcerts_treemodel;
	GHashTable *contactcerts_root_hash;

	GtkWidget *authoritycerts_treeview;
	GtkTreeStore *authoritycerts_treemodel;
	GHashTable *authoritycerts_root_hash;
} CertificateManagerData;

typedef enum {
	USER_CERT,
	CONTACT_CERT,
	CA_CERT
} CertType;

static void
initialize_yourcerts_ui (CertificateManagerData *cfm)
{
	GtkCellRenderer *cell = gtk_cell_renderer_text_new ();

	gtk_tree_view_append_column (GTK_TREE_VIEW (cfm->yourcerts_treeview),
				     gtk_tree_view_column_new_with_attributes (_("Certificate Name"),
									       cell,
									       "text", 0,
									       NULL));

	gtk_tree_view_append_column (GTK_TREE_VIEW (cfm->yourcerts_treeview),
				     gtk_tree_view_column_new_with_attributes (_("Purposes"),
									       cell,
									       "text", 1,
									       NULL));

	gtk_tree_view_append_column (GTK_TREE_VIEW (cfm->yourcerts_treeview),
				     gtk_tree_view_column_new_with_attributes (_("Serial Number"),
									       cell,
									       "text", 2,
									       NULL));

	gtk_tree_view_append_column (GTK_TREE_VIEW (cfm->yourcerts_treeview),
				     gtk_tree_view_column_new_with_attributes (_("Expires"),
									       cell,
									       "text", 3,
									       NULL));

	cfm->yourcerts_treemodel = gtk_tree_store_new (4,
						       G_TYPE_STRING,
						       G_TYPE_STRING,
						       G_TYPE_STRING,
						       G_TYPE_STRING);

	gtk_tree_view_set_model (GTK_TREE_VIEW (cfm->yourcerts_treeview),
				 GTK_TREE_MODEL (cfm->yourcerts_treemodel));

	cfm->yourcerts_root_hash = g_hash_table_new (g_str_hash, g_str_equal);
}

static void
initialize_contactcerts_ui (CertificateManagerData *cfm)
{
	GtkCellRenderer *cell = gtk_cell_renderer_text_new ();

	gtk_tree_view_append_column (GTK_TREE_VIEW (cfm->contactcerts_treeview),
				     gtk_tree_view_column_new_with_attributes (_("Certificate Name"),
									       cell,
									       "text", 0,
									       NULL));

	gtk_tree_view_append_column (GTK_TREE_VIEW (cfm->contactcerts_treeview),
				     gtk_tree_view_column_new_with_attributes (_("E-Mail Address"),
									       cell,
									       "text", 1,
									       NULL));

	gtk_tree_view_append_column (GTK_TREE_VIEW (cfm->contactcerts_treeview),
				     gtk_tree_view_column_new_with_attributes (_("Purposes"),
									       cell,
									       "text", 2,
									       NULL));

	cfm->contactcerts_treemodel = gtk_tree_store_new (3,
							  G_TYPE_STRING,
							  G_TYPE_STRING,
							  G_TYPE_STRING);
	
	gtk_tree_view_set_model (GTK_TREE_VIEW (cfm->contactcerts_treeview),
				 GTK_TREE_MODEL (cfm->contactcerts_treemodel));

	cfm->contactcerts_root_hash = g_hash_table_new (g_str_hash, g_str_equal);
}

static gint
iter_string_compare (GtkTreeModel *model,
		     GtkTreeIter  *a,
		     GtkTreeIter  *b,
		     gpointer      user_data)
{
	char *string1, *string2;

	gtk_tree_model_get (model, a,
			    0, &string1,
			    -1);

	gtk_tree_model_get (model, b,
			    0, &string2,
			    -1);

	return g_utf8_collate (string1, string2);
}

static void
initialize_authoritycerts_ui (CertificateManagerData *cfm)
{
	GtkCellRenderer *cell = gtk_cell_renderer_text_new ();

	gtk_tree_view_append_column (GTK_TREE_VIEW (cfm->authoritycerts_treeview),
				     gtk_tree_view_column_new_with_attributes (_("Certificate Name"),
									       cell,
									       "text", 0,
									       NULL));

	cfm->authoritycerts_treemodel = gtk_tree_store_new (1,
							    G_TYPE_STRING);

	gtk_tree_sortable_set_sort_func (GTK_TREE_SORTABLE (cfm->authoritycerts_treemodel),
					 0,
					 iter_string_compare, NULL, NULL);

	gtk_tree_sortable_set_sort_column_id (GTK_TREE_SORTABLE (cfm->authoritycerts_treemodel),
					      0,
					      GTK_SORT_ASCENDING);

	gtk_tree_view_set_model (GTK_TREE_VIEW (cfm->authoritycerts_treeview),
				 GTK_TREE_MODEL (cfm->authoritycerts_treemodel));

	cfm->authoritycerts_root_hash = g_hash_table_new (g_str_hash, g_str_equal);
}

static CertType
get_cert_type (ECert *cert)
{
	const char *nick = e_cert_get_nickname (cert);
	const char *email = e_cert_get_email (cert);

	if (e_cert_is_ca_cert (cert))
		return CA_CERT;

	/* XXX more stuff in here */
	else
		return USER_CERT;
}

typedef void (*AddCertCb)(CertificateManagerData *cfm, ECert *cert);

static void
add_user_cert (CertificateManagerData *cfm, ECert *cert)
{
	GtkTreeIter iter;
	GtkTreeIter *parent_iter = NULL;
	const char *organization = e_cert_get_org (cert);
	const char *common_name;

	if (organization) {
		parent_iter = g_hash_table_lookup (cfm->yourcerts_root_hash, organization);
		if (!parent_iter) {
			/* create a new toplevel node */
			gtk_tree_store_append (GTK_TREE_STORE (cfm->yourcerts_treemodel), &iter, NULL);
		
			gtk_tree_store_set (GTK_TREE_STORE (cfm->yourcerts_treemodel), &iter,
					    0, organization, -1);

			/* now copy it off into parent_iter and insert it into
			   the hashtable */
			parent_iter = gtk_tree_iter_copy (&iter);
			g_hash_table_insert (cfm->yourcerts_root_hash, g_strdup (organization), parent_iter);
		}
	}

	gtk_tree_store_append (GTK_TREE_STORE (cfm->yourcerts_treemodel), &iter, parent_iter);

	common_name = e_cert_get_cn (cert);
	if (common_name) {
		gtk_tree_store_set (GTK_TREE_STORE (cfm->yourcerts_treemodel), &iter,
				    0, common_name, -1);
	}
	else
		gtk_tree_store_set (GTK_TREE_STORE (cfm->yourcerts_treemodel), &iter,
				    0, e_cert_get_nickname (cert), -1);
}

static void
add_contact_cert (CertificateManagerData *cfm, ECert *cert)
{
	/* nothing yet */
}

static void
add_ca_cert (CertificateManagerData *cfm, ECert *cert)
{
	GtkTreeIter iter;
	GtkTreeIter *parent_iter = NULL;
	const char *organization = e_cert_get_org (cert);
	const char *common_name;

	if (organization) {
		parent_iter = g_hash_table_lookup (cfm->authoritycerts_root_hash, organization);
		if (!parent_iter) {
			/* create a new toplevel node */
			gtk_tree_store_append (GTK_TREE_STORE (cfm->authoritycerts_treemodel), &iter, NULL);
		
			gtk_tree_store_set (GTK_TREE_STORE (cfm->authoritycerts_treemodel), &iter,
					    0, organization, -1);

			/* now copy it off into parent_iter and insert it into
			   the hashtable */
			parent_iter = gtk_tree_iter_copy (&iter);
			g_hash_table_insert (cfm->authoritycerts_root_hash, g_strdup (organization), parent_iter);
		}
	}


	gtk_tree_store_append (GTK_TREE_STORE (cfm->authoritycerts_treemodel), &iter, parent_iter);

	common_name = e_cert_get_cn (cert);
	if (common_name) {
		gtk_tree_store_set (GTK_TREE_STORE (cfm->authoritycerts_treemodel), &iter,
				    0, common_name, -1);
	}
	else
		gtk_tree_store_set (GTK_TREE_STORE (cfm->authoritycerts_treemodel), &iter,
				    0, e_cert_get_nickname (cert), -1);
}

static void
load_certs (CertificateManagerData *cfm,
	    CertType type,
	    AddCertCb add_cert)
{
	CERTCertList *certList;
	CERTCertListNode *node;

	certList = PK11_ListCerts (PK11CertListUnique, NULL);

	printf ("certList = %p\n", certList);

	for (node = CERT_LIST_HEAD(certList);
	     !CERT_LIST_END(node, certList);
	     node = CERT_LIST_NEXT(node)) {
		ECert *cert = e_cert_new ((CERTCertificate*)node->cert);
		if (get_cert_type(cert) == type) {
			printf ("cert (nickname = '%s') matches\n", e_cert_get_nickname (cert));
			add_cert (cfm, cert);
		}
		/* XXX we leak cert */
	}

}

static void
populate_ui (CertificateManagerData *cfm)
{
	load_certs (cfm, USER_CERT, add_user_cert);
	load_certs (cfm, CONTACT_CERT, add_contact_cert);
	load_certs (cfm, CA_CERT, add_ca_cert);
}

EvolutionConfigControl*
certificate_manager_config_control_new (void)
{
	CertificateManagerData *cfm_data = g_new0 (CertificateManagerData, 1);
	GtkWidget *control_widget;

	/* XXX this should happen someplace else, and shouldn't
	   reference my default mozilla profile :) */
	NSS_InitReadWrite ("/home/toshok/.mozilla/default/xuvq7jx3.slt");

	cfm_data->gui = glade_xml_new (EVOLUTION_GLADEDIR "/" GLADE_FILE_NAME, NULL, NULL);

	cfm_data->yourcerts_treeview = glade_xml_get_widget (cfm_data->gui, "yourcerts-treeview");
	cfm_data->contactcerts_treeview = glade_xml_get_widget (cfm_data->gui, "contactcerts-treeview");
	cfm_data->authoritycerts_treeview = glade_xml_get_widget (cfm_data->gui, "authoritycerts-treeview");

	initialize_yourcerts_ui(cfm_data);
	initialize_contactcerts_ui(cfm_data);
	initialize_authoritycerts_ui(cfm_data);

	populate_ui (cfm_data);

	control_widget = glade_xml_get_widget (cfm_data->gui, "cert-manager-notebook");
	gtk_widget_ref (control_widget);

	gtk_container_remove (GTK_CONTAINER (control_widget->parent), control_widget);

	return evolution_config_control_new (control_widget);
}
