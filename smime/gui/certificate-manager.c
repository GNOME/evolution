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


#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#define GLADE_FILE_NAME "smime-ui.glade"

#include <gtk/gtk.h>

#include <libgnome/gnome-i18n.h>

#include <glade/glade.h>
#include "evolution-config-control.h"
#include "ca-trust-dialog.h"
#include "cert-trust-dialog.h"
#include "certificate-manager.h"
#include "certificate-viewer.h"

#include "e-cert.h"
#include "e-cert-trust.h"
#include "e-cert-db.h"

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
	GtkTreeModel *yourcerts_streemodel;
	GHashTable *yourcerts_root_hash;
	GtkWidget *view_your_button;
	GtkWidget *backup_your_button;
	GtkWidget *backup_all_your_button;
	GtkWidget *import_your_button;
	GtkWidget *delete_your_button;

	GtkWidget *contactcerts_treeview;
	GtkTreeModel *contactcerts_streemodel;
	GHashTable *contactcerts_root_hash;
	GtkWidget *view_contact_button;
	GtkWidget *edit_contact_button;
	GtkWidget *import_contact_button;
	GtkWidget *delete_contact_button;

	GtkWidget *authoritycerts_treeview;
	GtkTreeModel *authoritycerts_streemodel;
	GHashTable *authoritycerts_root_hash;
	GtkWidget *view_ca_button;
	GtkWidget *edit_ca_button;
	GtkWidget *import_ca_button;
	GtkWidget *delete_ca_button;

} CertificateManagerData;

typedef void (*AddCertCb)(CertificateManagerData *cfm, ECert *cert);

static void unload_certs (CertificateManagerData *cfm, ECertType type);
static void load_certs (CertificateManagerData *cfm, ECertType type, AddCertCb add_cert);

static void add_user_cert (CertificateManagerData *cfm, ECert *cert);
static void add_contact_cert (CertificateManagerData *cfm, ECert *cert);
static void add_ca_cert (CertificateManagerData *cfm, ECert *cert);

static void
handle_selection_changed (GtkTreeSelection *selection,
			  int cert_column,
			  GtkWidget *view_button,
			  GtkWidget *edit_button,
			  GtkWidget *delete_button)
{
	GtkTreeIter iter;
	gboolean cert_selected = FALSE;
	GtkTreeModel *model;

	if (gtk_tree_selection_get_selected (selection,
					     &model,
					     &iter)) {
		ECert *cert;

		gtk_tree_model_get (model,
				    &iter,
				    cert_column, &cert,
				    -1);

		if (cert) {
			cert_selected = TRUE;
			g_object_unref (cert);
		}
	}

	if (delete_button)
		gtk_widget_set_sensitive (delete_button, cert_selected);
	if (edit_button)
		gtk_widget_set_sensitive (edit_button, cert_selected);
	if (view_button)
		gtk_widget_set_sensitive (view_button, cert_selected);
}

static void
import_your (GtkWidget *widget, CertificateManagerData *cfm)
{
	GtkWidget *filesel = gtk_file_selection_new (_("Select a cert to import..."));

	if (GTK_RESPONSE_OK == gtk_dialog_run (GTK_DIALOG (filesel))) {
		const char *filename = gtk_file_selection_get_filename (GTK_FILE_SELECTION (filesel));
		if (e_cert_db_import_pkcs12_file (e_cert_db_peek (),
						  filename, NULL /* XXX */)) {
			/* there's no telling how many certificates were added during the import,
			   so we blow away the contact cert display and regenerate it. */
			unload_certs (cfm, E_CERT_USER);
			load_certs (cfm, E_CERT_USER, add_user_cert);
			gtk_tree_view_expand_all (GTK_TREE_VIEW (cfm->yourcerts_treeview));
		}
	}

	gtk_widget_destroy (filesel);
}

static void
delete_your (GtkWidget *widget, CertificateManagerData *cfm)
{
	GtkTreeIter iter;

	if (gtk_tree_selection_get_selected (gtk_tree_view_get_selection (GTK_TREE_VIEW(cfm->yourcerts_treeview)),
					     NULL,
					     &iter)) {
		ECert *cert;

		gtk_tree_model_get (GTK_TREE_MODEL (cfm->yourcerts_streemodel),
				    &iter,
				    4, &cert,
				    -1);

		if (cert) {
			GtkTreeIter child_iter;
			printf ("DELETE\n");
			e_cert_db_delete_cert (e_cert_db_peek (), cert);
			gtk_tree_model_sort_convert_iter_to_child_iter (GTK_TREE_MODEL_SORT (cfm->yourcerts_streemodel),
									&child_iter,
									&iter);
			gtk_tree_store_remove (GTK_TREE_STORE (gtk_tree_model_sort_get_model (GTK_TREE_MODEL_SORT (cfm->yourcerts_streemodel))),
					       &child_iter);

			/* we need two unrefs here, one to unref the
			   gtk_tree_model_get above, and one to unref
			   the initial ref when we created the cert
			   and added it to the tree */
			g_object_unref (cert);
			g_object_unref (cert);
		}
	}
					     
}

static void
view_your (GtkWidget *widget, CertificateManagerData *cfm)
{
	GtkTreeIter iter;

	if (gtk_tree_selection_get_selected (gtk_tree_view_get_selection (GTK_TREE_VIEW(cfm->yourcerts_treeview)),
					     NULL,
					     &iter)) {
		ECert *cert;

		gtk_tree_model_get (GTK_TREE_MODEL (cfm->yourcerts_streemodel),
				    &iter,
				    4, &cert,
				    -1);

		if (cert) {
			GtkWidget *dialog = certificate_viewer_show (cert);
			g_signal_connect (dialog, "response",
					  G_CALLBACK (gtk_widget_destroy), NULL);
			gtk_widget_show (dialog);
		}
	}
}

static void
backup_your (GtkWidget *widget, CertificateManagerData *cfm)
{
}

static void
backup_all_your (GtkWidget *widget, CertificateManagerData *cfm)
{
}

static void
yourcerts_selection_changed (GtkTreeSelection *selection, CertificateManagerData *cfm)
{
	handle_selection_changed (selection,
				  4,
				  cfm->view_your_button,
				  cfm->backup_your_button, /* yes yes, not really "edit", it's a hack :) */
				  cfm->delete_your_button);
}

static GtkTreeModel*
create_yourcerts_treemodel (void)
{
	return GTK_TREE_MODEL (gtk_tree_store_new (5,
						   G_TYPE_STRING,
						   G_TYPE_STRING,
						   G_TYPE_STRING,
						   G_TYPE_STRING,
						   G_TYPE_OBJECT));
}

static void
initialize_yourcerts_ui (CertificateManagerData *cfm)
{
	GtkCellRenderer *cell = gtk_cell_renderer_text_new ();
	GtkTreeSelection *selection;
	GtkTreeViewColumn *column;

	column = gtk_tree_view_column_new_with_attributes (_("Certificate Name"),
							   cell,
							   "text", 0,
							   NULL);
	gtk_tree_view_column_set_resizable(column, TRUE);
	gtk_tree_view_append_column (GTK_TREE_VIEW (cfm->yourcerts_treeview),
				     column);
	gtk_tree_view_column_set_sort_column_id (column, 0);

	column = gtk_tree_view_column_new_with_attributes (_("Purposes"),
							   cell,
							   "text", 1,
							   NULL);
	gtk_tree_view_column_set_resizable(column, TRUE);
	gtk_tree_view_append_column (GTK_TREE_VIEW (cfm->yourcerts_treeview),
				     column);
	gtk_tree_view_column_set_sort_column_id (column, 1);

	column = gtk_tree_view_column_new_with_attributes (_("Serial Number"),
							   cell,
							   "text", 2,
							   NULL);
	gtk_tree_view_append_column (GTK_TREE_VIEW (cfm->yourcerts_treeview),
				     column);
	gtk_tree_view_column_set_sort_column_id (column, 2);

	column = gtk_tree_view_column_new_with_attributes (_("Expires"),
							   cell,
							   "text", 3,
							   NULL);
	gtk_tree_view_append_column (GTK_TREE_VIEW (cfm->yourcerts_treeview),
				     column);
	gtk_tree_view_column_set_sort_column_id (column, 3);

	gtk_tree_sortable_set_sort_column_id (GTK_TREE_SORTABLE (cfm->yourcerts_streemodel),
					      0,
					      GTK_SORT_ASCENDING);

	selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (cfm->yourcerts_treeview));
	g_signal_connect (selection, "changed", G_CALLBACK (yourcerts_selection_changed), cfm);

	if (cfm->import_your_button)
		g_signal_connect (cfm->import_your_button, "clicked", G_CALLBACK (import_your), cfm);

	if (cfm->delete_your_button)
		g_signal_connect (cfm->delete_your_button, "clicked", G_CALLBACK (delete_your), cfm);

	if (cfm->view_your_button)
		g_signal_connect (cfm->view_your_button, "clicked", G_CALLBACK (view_your), cfm);

	if (cfm->backup_your_button)
		g_signal_connect (cfm->backup_your_button, "clicked", G_CALLBACK (backup_your), cfm);

	if (cfm->backup_all_your_button)
		g_signal_connect (cfm->backup_all_your_button, "clicked", G_CALLBACK (backup_all_your), cfm);
}

static void
view_contact (GtkWidget *widget, CertificateManagerData *cfm)
{
	GtkTreeIter iter;

	if (gtk_tree_selection_get_selected (gtk_tree_view_get_selection (GTK_TREE_VIEW(cfm->contactcerts_treeview)),
					     NULL,
					     &iter)) {
		ECert *cert;

		gtk_tree_model_get (GTK_TREE_MODEL (cfm->contactcerts_streemodel),
				    &iter,
				    3, &cert,
				    -1);

		if (cert) {
			GtkWidget *dialog = certificate_viewer_show (cert);
			g_signal_connect (dialog, "response",
					  G_CALLBACK (gtk_widget_destroy), NULL);
			gtk_widget_show (dialog);
		}
	}
}

static void
edit_contact (GtkWidget *widget, CertificateManagerData *cfm)
{
	GtkTreeIter iter;

	if (gtk_tree_selection_get_selected (gtk_tree_view_get_selection (GTK_TREE_VIEW(cfm->contactcerts_treeview)),
					     NULL,
					     &iter)) {
		ECert *cert;

		gtk_tree_model_get (GTK_TREE_MODEL (cfm->contactcerts_streemodel),
				    &iter,
				    3, &cert,
				    -1);

		if (cert) {
			GtkWidget *dialog = cert_trust_dialog_show (cert);
			g_signal_connect (dialog, "response",
					  G_CALLBACK (gtk_widget_destroy), NULL);
			gtk_widget_show (dialog);
		}
	}
}

static void
import_contact (GtkWidget *widget, CertificateManagerData *cfm)
{
	GtkWidget *filesel = gtk_file_selection_new (_("Select a cert to import..."));

	if (GTK_RESPONSE_OK == gtk_dialog_run (GTK_DIALOG (filesel))) {
		const char *filename = gtk_file_selection_get_filename (GTK_FILE_SELECTION (filesel));

		if (e_cert_db_import_certs_from_file (e_cert_db_peek (),
						      filename,
						      E_CERT_CONTACT,
						      NULL)) {

			/* there's no telling how many certificates were added during the import,
			   so we blow away the contact cert display and regenerate it. */
			unload_certs (cfm, E_CERT_CONTACT);
			load_certs (cfm, E_CERT_CONTACT, add_contact_cert);
			gtk_tree_view_expand_all (GTK_TREE_VIEW (cfm->contactcerts_treeview));
		}
	}

	gtk_widget_destroy (filesel);
}

static void
delete_contact (GtkWidget *widget, CertificateManagerData *cfm)
{
	GtkTreeIter iter;

	if (gtk_tree_selection_get_selected (gtk_tree_view_get_selection (GTK_TREE_VIEW(cfm->contactcerts_treeview)),
					     NULL,
					     &iter)) {
		ECert *cert;

		gtk_tree_model_get (GTK_TREE_MODEL (cfm->contactcerts_streemodel),
				    &iter,
				    3, &cert,
				    -1);

		if (cert) {
			GtkTreeIter child_iter;
			printf ("DELETE\n");
			e_cert_db_delete_cert (e_cert_db_peek (), cert);
			gtk_tree_model_sort_convert_iter_to_child_iter (GTK_TREE_MODEL_SORT (cfm->contactcerts_streemodel),
									&child_iter,
									&iter);
			gtk_tree_store_remove (GTK_TREE_STORE (gtk_tree_model_sort_get_model (GTK_TREE_MODEL_SORT (cfm->contactcerts_streemodel))),
					       &child_iter);

			/* we need two unrefs here, one to unref the
			   gtk_tree_model_get above, and one to unref
			   the initial ref when we created the cert
			   and added it to the tree */
			g_object_unref (cert);
			g_object_unref (cert);
		}
	}
					     
}

static void
contactcerts_selection_changed (GtkTreeSelection *selection, CertificateManagerData *cfm)
{
	handle_selection_changed (selection,
				  3,
				  cfm->view_contact_button,
				  cfm->edit_contact_button,
				  cfm->delete_contact_button);
}

static GtkTreeModel*
create_contactcerts_treemodel (void)
{
	return GTK_TREE_MODEL (gtk_tree_store_new (4,
						   G_TYPE_STRING,
						   G_TYPE_STRING,
						   G_TYPE_STRING,
						   G_TYPE_OBJECT));
}

static void
initialize_contactcerts_ui (CertificateManagerData *cfm)
{
	GtkCellRenderer *cell = gtk_cell_renderer_text_new ();
	GtkTreeSelection *selection;
	GtkTreeViewColumn *column;

	column = gtk_tree_view_column_new_with_attributes (_("Certificate Name"),
							   cell,
							   "text", 0,
							   NULL);
	gtk_tree_view_column_set_resizable(column, TRUE);
	gtk_tree_view_append_column (GTK_TREE_VIEW (cfm->contactcerts_treeview),
				     column);
	gtk_tree_view_column_set_sort_column_id (column, 0);

	column = gtk_tree_view_column_new_with_attributes (_("E-Mail Address"),
							   cell,
							   "text", 1,
							   NULL);
	gtk_tree_view_column_set_resizable(column, TRUE);
	gtk_tree_view_append_column (GTK_TREE_VIEW (cfm->contactcerts_treeview),
				     column);
	gtk_tree_view_column_set_sort_column_id (column, 1);

	column = gtk_tree_view_column_new_with_attributes (_("Purposes"),
							   cell,
							   "text", 2,
							   NULL);
	gtk_tree_view_column_set_resizable(column, TRUE);
	gtk_tree_view_append_column (GTK_TREE_VIEW (cfm->contactcerts_treeview),
				     column);
	gtk_tree_view_column_set_sort_column_id (column, 2);

	cfm->contactcerts_root_hash = g_hash_table_new (g_str_hash, g_str_equal);

	selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (cfm->contactcerts_treeview));
	g_signal_connect (selection, "changed", G_CALLBACK (contactcerts_selection_changed), cfm);

	if (cfm->view_contact_button)
		g_signal_connect (cfm->view_contact_button, "clicked", G_CALLBACK (view_contact), cfm);

	if (cfm->edit_contact_button)
		g_signal_connect (cfm->edit_contact_button, "clicked", G_CALLBACK (edit_contact), cfm);

	if (cfm->import_contact_button)
		g_signal_connect (cfm->import_contact_button, "clicked", G_CALLBACK (import_contact), cfm);

	if (cfm->delete_contact_button)
		g_signal_connect (cfm->delete_contact_button, "clicked", G_CALLBACK (delete_contact), cfm);

}

static void
view_ca (GtkWidget *widget, CertificateManagerData *cfm)
{
	GtkTreeIter iter;
	
	if (gtk_tree_selection_get_selected (gtk_tree_view_get_selection (GTK_TREE_VIEW(cfm->authoritycerts_treeview)),
					     NULL,
					     &iter)) {
		ECert *cert;

		gtk_tree_model_get (GTK_TREE_MODEL (cfm->authoritycerts_streemodel),
				    &iter,
				    1, &cert,
				    -1);

		if (cert) {
			GtkWidget *dialog = certificate_viewer_show (cert);
			g_signal_connect (dialog, "response",
					  G_CALLBACK (gtk_widget_destroy), NULL);
			gtk_widget_show (dialog);
		}
	}
}

static void
edit_ca (GtkWidget *widget, CertificateManagerData *cfm)
{
	GtkTreeIter iter;
	
	if (gtk_tree_selection_get_selected (gtk_tree_view_get_selection (GTK_TREE_VIEW(cfm->authoritycerts_treeview)),
					     NULL,
					     &iter)) {
		ECert *cert;

		gtk_tree_model_get (GTK_TREE_MODEL (cfm->authoritycerts_streemodel),
				    &iter,
				    1, &cert,
				    -1);

		if (cert) {
			GtkWidget *dialog = ca_trust_dialog_show (cert, FALSE);
			CERTCertificate *icert = e_cert_get_internal_cert (cert);

			ca_trust_dialog_set_trust (dialog,
						   e_cert_trust_has_trusted_ca (icert->trust, TRUE,  FALSE, FALSE),
						   e_cert_trust_has_trusted_ca (icert->trust, FALSE, TRUE,  FALSE),
						   e_cert_trust_has_trusted_ca (icert->trust, FALSE, FALSE, TRUE));
						   
			if (gtk_dialog_run (GTK_DIALOG (dialog)) == GTK_RESPONSE_OK) {
				gboolean trust_ssl, trust_email, trust_objsign;
				CERTCertTrust trust;

				ca_trust_dialog_get_trust (dialog,
							   &trust_ssl, &trust_email, &trust_objsign);

				e_cert_trust_init (&trust);
				e_cert_trust_set_valid_ca (&trust);
				e_cert_trust_add_ca_trust (&trust,
							   trust_ssl,
							   trust_email,
							   trust_objsign);
				
				CERT_ChangeCertTrust (CERT_GetDefaultCertDB(), icert, &trust);
			}

			gtk_widget_destroy (dialog);
		}
	}
}

static void
import_ca (GtkWidget *widget, CertificateManagerData *cfm)
{
	GtkWidget *filesel = gtk_file_selection_new (_("Select a cert to import..."));

	if (GTK_RESPONSE_OK == gtk_dialog_run (GTK_DIALOG (filesel))) {
		const char *filename = gtk_file_selection_get_filename (GTK_FILE_SELECTION (filesel));

		if (e_cert_db_import_certs_from_file (e_cert_db_peek (),
						      filename,
						      E_CERT_CA,
						      NULL)) {

			/* there's no telling how many certificates were added during the import,
			   so we blow away the CA cert display and regenerate it. */
			unload_certs (cfm, E_CERT_CA);
			load_certs (cfm, E_CERT_CA, add_ca_cert);
		}
	}

	gtk_widget_destroy (filesel);
}

static void
delete_ca (GtkWidget *widget, CertificateManagerData *cfm)
{
	GtkTreeIter iter;

	if (gtk_tree_selection_get_selected (gtk_tree_view_get_selection (GTK_TREE_VIEW(cfm->authoritycerts_treeview)),
					     NULL,
					     &iter)) {
		ECert *cert;

		gtk_tree_model_get (GTK_TREE_MODEL (cfm->authoritycerts_streemodel),
				    &iter,
				    1, &cert,
				    -1);

		if (cert) {
			GtkTreeIter child_iter;
			printf ("DELETE\n");
			e_cert_db_delete_cert (e_cert_db_peek (), cert);
			gtk_tree_model_sort_convert_iter_to_child_iter (GTK_TREE_MODEL_SORT (cfm->authoritycerts_streemodel),
									&child_iter,
									&iter);
			gtk_tree_store_remove (GTK_TREE_STORE (gtk_tree_model_sort_get_model (GTK_TREE_MODEL_SORT (cfm->authoritycerts_streemodel))),
					       &child_iter);

			/* we need two unrefs here, one to unref the
			   gtk_tree_model_get above, and one to unref
			   the initial ref when we created the cert
			   and added it to the tree */
			g_object_unref (cert);
			g_object_unref (cert);
		}
	}
					     
}

static void
authoritycerts_selection_changed (GtkTreeSelection *selection, CertificateManagerData *cfm)
{
	handle_selection_changed (selection,
				  1,
				  cfm->view_ca_button,
				  cfm->edit_ca_button,
				  cfm->delete_ca_button);
}

static GtkTreeModel*
create_authoritycerts_treemodel (void)
{
	return GTK_TREE_MODEL (gtk_tree_store_new (2,
						   G_TYPE_STRING,
						   G_TYPE_OBJECT));

}

static void
initialize_authoritycerts_ui (CertificateManagerData *cfm)
{
	GtkCellRenderer *cell = gtk_cell_renderer_text_new ();
	GtkTreeSelection *selection;
	GtkTreeViewColumn *column;

	column = gtk_tree_view_column_new_with_attributes (_("Certificate Name"),
							   cell,
							   "text", 0,
							   NULL);
	gtk_tree_view_append_column (GTK_TREE_VIEW (cfm->authoritycerts_treeview),
				     column);
	gtk_tree_view_column_set_sort_column_id (column, 0);

	selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (cfm->authoritycerts_treeview));
	g_signal_connect (selection, "changed", G_CALLBACK (authoritycerts_selection_changed), cfm);

	if (cfm->view_ca_button)
		g_signal_connect (cfm->view_ca_button, "clicked", G_CALLBACK (view_ca), cfm);

	if (cfm->edit_ca_button)
		g_signal_connect (cfm->edit_ca_button, "clicked", G_CALLBACK (edit_ca), cfm);

	if (cfm->import_ca_button)
		g_signal_connect (cfm->import_ca_button, "clicked", G_CALLBACK (import_ca), cfm);

	if (cfm->delete_ca_button)
		g_signal_connect (cfm->delete_ca_button, "clicked", G_CALLBACK (delete_ca), cfm);
}

static void
add_user_cert (CertificateManagerData *cfm, ECert *cert)
{
	GtkTreeIter iter;
	GtkTreeIter *parent_iter = NULL;
	const char *organization = e_cert_get_org (cert);
	GtkTreeModel *model = gtk_tree_model_sort_get_model (GTK_TREE_MODEL_SORT (cfm->yourcerts_streemodel));

	if (organization) {
		parent_iter = g_hash_table_lookup (cfm->yourcerts_root_hash, organization);
		if (!parent_iter) {
			/* create a new toplevel node */
			gtk_tree_store_append (GTK_TREE_STORE (model), &iter, NULL);
		
			gtk_tree_store_set (GTK_TREE_STORE (model), &iter,
					    0, organization, -1);

			/* now copy it off into parent_iter and insert it into
			   the hashtable */
			parent_iter = gtk_tree_iter_copy (&iter);
			g_hash_table_insert (cfm->yourcerts_root_hash, g_strdup (organization), parent_iter);
		}
	}

	gtk_tree_store_append (GTK_TREE_STORE (model), &iter, parent_iter);

	if (e_cert_get_cn (cert))
		gtk_tree_store_set (GTK_TREE_STORE (model), &iter,
				    0, e_cert_get_cn (cert),
				    1, e_cert_get_usage(cert),
				    2, e_cert_get_serial_number(cert),
				    3, e_cert_get_expires_on(cert),
				    4, cert,
				    -1);
	else
		gtk_tree_store_set (GTK_TREE_STORE (model), &iter,
				    0, e_cert_get_nickname (cert),
				    1, e_cert_get_usage(cert),
				    2, e_cert_get_serial_number(cert),
				    3, e_cert_get_expires_on(cert),
				    4, cert,
				    -1);
}

static void
add_contact_cert (CertificateManagerData *cfm, ECert *cert)
{
	GtkTreeIter iter;
	GtkTreeIter *parent_iter = NULL;
	const char *organization = e_cert_get_org (cert);
	GtkTreeModel *model = gtk_tree_model_sort_get_model (GTK_TREE_MODEL_SORT (cfm->contactcerts_streemodel));

	if (organization) {
		parent_iter = g_hash_table_lookup (cfm->contactcerts_root_hash, organization);
		if (!parent_iter) {
			/* create a new toplevel node */
			gtk_tree_store_append (GTK_TREE_STORE (model), &iter, NULL);
		
			gtk_tree_store_set (GTK_TREE_STORE (model), &iter,
					    0, organization, -1);

			/* now copy it off into parent_iter and insert it into
			   the hashtable */
			parent_iter = gtk_tree_iter_copy (&iter);
			g_hash_table_insert (cfm->contactcerts_root_hash, g_strdup (organization), parent_iter);
		}
	}

	gtk_tree_store_append (GTK_TREE_STORE (model), &iter, parent_iter);

	if (e_cert_get_cn (cert))
		gtk_tree_store_set (GTK_TREE_STORE (model), &iter,
				    0, e_cert_get_cn (cert),
				    1, e_cert_get_email (cert),
				    2, e_cert_get_usage(cert),
				    3, cert,
				    -1);
	else
		gtk_tree_store_set (GTK_TREE_STORE (model), &iter,
				    0, e_cert_get_nickname (cert),
				    1, e_cert_get_email (cert),
				    2, e_cert_get_usage(cert),
				    3, cert,
				    -1);
}

static void
add_ca_cert (CertificateManagerData *cfm, ECert *cert)
{
	GtkTreeIter iter;
	GtkTreeIter *parent_iter = NULL;
	const char *organization = e_cert_get_org (cert);
	GtkTreeModel *model = gtk_tree_model_sort_get_model (GTK_TREE_MODEL_SORT (cfm->authoritycerts_streemodel));

	if (organization) {
		parent_iter = g_hash_table_lookup (cfm->authoritycerts_root_hash, organization);
		if (!parent_iter) {
			/* create a new toplevel node */
			gtk_tree_store_append (GTK_TREE_STORE (model),
					       &iter, NULL);
		
			gtk_tree_store_set (GTK_TREE_STORE (model), &iter,
					    0, organization, -1);

			/* now copy it off into parent_iter and insert it into
			   the hashtable */
			parent_iter = gtk_tree_iter_copy (&iter);
			g_hash_table_insert (cfm->authoritycerts_root_hash, g_strdup (organization), parent_iter);
		}
	}


	gtk_tree_store_append (GTK_TREE_STORE (model), &iter, parent_iter);

	if (e_cert_get_cn (cert))
		gtk_tree_store_set (GTK_TREE_STORE (model), &iter,
				    0, e_cert_get_cn (cert),
				    1, cert,
				    -1);
	else
		gtk_tree_store_set (GTK_TREE_STORE (model), &iter,
				    0, e_cert_get_nickname (cert),
				    1, cert,
				    -1);
}

static void
destroy_key (gpointer data)
{
	g_free (data);
}

static void
destroy_value (gpointer data)
{
	gtk_tree_iter_free (data);
}

static void
unload_certs (CertificateManagerData *cfm,
	      ECertType type)
{
	GtkTreeModel *treemodel;

	switch (type) {
	case E_CERT_USER:
		treemodel = create_yourcerts_treemodel ();

		cfm->yourcerts_streemodel = gtk_tree_model_sort_new_with_model (GTK_TREE_MODEL (treemodel));

		g_object_unref (treemodel);

		gtk_tree_view_set_model (GTK_TREE_VIEW (cfm->yourcerts_treeview),
					 cfm->yourcerts_streemodel);

		gtk_tree_sortable_set_sort_column_id (GTK_TREE_SORTABLE (cfm->yourcerts_streemodel),
						      0,
						      GTK_SORT_ASCENDING);

		if (cfm->yourcerts_root_hash)
			g_hash_table_destroy (cfm->yourcerts_root_hash);

		cfm->yourcerts_root_hash = g_hash_table_new_full (g_str_hash, g_str_equal,
								  destroy_key, destroy_value);
		break;
	case E_CERT_CONTACT:
		treemodel = create_contactcerts_treemodel ();

		cfm->contactcerts_streemodel = gtk_tree_model_sort_new_with_model (GTK_TREE_MODEL (treemodel));

		g_object_unref (treemodel);

		gtk_tree_view_set_model (GTK_TREE_VIEW (cfm->contactcerts_treeview),
					 cfm->contactcerts_streemodel);

		gtk_tree_sortable_set_sort_column_id (GTK_TREE_SORTABLE (cfm->contactcerts_streemodel),
						      0,
						      GTK_SORT_ASCENDING);

		if (cfm->contactcerts_root_hash)
			g_hash_table_destroy (cfm->contactcerts_root_hash);

		cfm->contactcerts_root_hash = g_hash_table_new_full (g_str_hash, g_str_equal,
								     destroy_key, destroy_value);
		break;
	case E_CERT_SITE:
		break;
	case E_CERT_CA:
		treemodel = create_authoritycerts_treemodel ();

		cfm->authoritycerts_streemodel = gtk_tree_model_sort_new_with_model (GTK_TREE_MODEL (treemodel));

		g_object_unref (treemodel);

		gtk_tree_view_set_model (GTK_TREE_VIEW (cfm->authoritycerts_treeview),
					 cfm->authoritycerts_streemodel);

		gtk_tree_sortable_set_sort_column_id (GTK_TREE_SORTABLE (cfm->authoritycerts_streemodel),
						      0,
						      GTK_SORT_ASCENDING);

		if (cfm->authoritycerts_root_hash)
			g_hash_table_destroy (cfm->authoritycerts_root_hash);

		cfm->authoritycerts_root_hash = g_hash_table_new_full (g_str_hash, g_str_equal,
								       destroy_key, destroy_value);


		break;
	case E_CERT_UNKNOWN:
		/* nothing to do here */
		break;
	}
}

static void
load_certs (CertificateManagerData *cfm,
	    ECertType type,
	    AddCertCb add_cert)
{
	CERTCertList *certList;
	CERTCertListNode *node;

	certList = PK11_ListCerts (PK11CertListUnique, NULL);

	for (node = CERT_LIST_HEAD(certList);
	     !CERT_LIST_END(node, certList);
	     node = CERT_LIST_NEXT(node)) {
		ECert *cert = e_cert_new ((CERTCertificate*)node->cert);
		if (e_cert_get_cert_type(cert) == type) {
			add_cert (cfm, cert);
		}
	}
}

static void
populate_ui (CertificateManagerData *cfm)
{
	unload_certs (cfm, E_CERT_USER);
	load_certs (cfm, E_CERT_USER, add_user_cert);

	unload_certs (cfm, E_CERT_CONTACT);
	load_certs (cfm, E_CERT_CONTACT, add_contact_cert);

	unload_certs (cfm, E_CERT_CA);
	load_certs (cfm, E_CERT_CA, add_ca_cert);

	/* expand all three trees */
	gtk_tree_view_expand_all (GTK_TREE_VIEW (cfm->yourcerts_treeview));
	gtk_tree_view_expand_all (GTK_TREE_VIEW (cfm->contactcerts_treeview));
}

EvolutionConfigControl*
certificate_manager_config_control_new (void)
{
	CertificateManagerData *cfm_data;
	GtkWidget *control_widget;

	/* We need to peek the db here to make sure it (and NSS) are fully initialized. */
	e_cert_db_peek ();

	cfm_data = g_new0 (CertificateManagerData, 1);
	cfm_data->gui = glade_xml_new (EVOLUTION_GLADEDIR "/" GLADE_FILE_NAME, NULL, NULL);

	cfm_data->yourcerts_treeview = glade_xml_get_widget (cfm_data->gui, "yourcerts-treeview");
	cfm_data->contactcerts_treeview = glade_xml_get_widget (cfm_data->gui, "contactcerts-treeview");
	cfm_data->authoritycerts_treeview = glade_xml_get_widget (cfm_data->gui, "authoritycerts-treeview");

	cfm_data->view_your_button = glade_xml_get_widget (cfm_data->gui, "your-view-button");
	cfm_data->backup_your_button = glade_xml_get_widget (cfm_data->gui, "your-backup-button");
	cfm_data->backup_all_your_button = glade_xml_get_widget (cfm_data->gui, "your-backup-all-button");
	cfm_data->import_your_button = glade_xml_get_widget (cfm_data->gui, "your-import-button");
	cfm_data->delete_your_button = glade_xml_get_widget (cfm_data->gui, "your-delete-button");

	cfm_data->view_contact_button = glade_xml_get_widget (cfm_data->gui, "contact-view-button");
	cfm_data->edit_contact_button = glade_xml_get_widget (cfm_data->gui, "contact-edit-button");
	cfm_data->import_contact_button = glade_xml_get_widget (cfm_data->gui, "contact-import-button");
	cfm_data->delete_contact_button = glade_xml_get_widget (cfm_data->gui, "contact-delete-button");

	cfm_data->view_ca_button = glade_xml_get_widget (cfm_data->gui, "authority-view-button");
	cfm_data->edit_ca_button = glade_xml_get_widget (cfm_data->gui, "authority-edit-button");
	cfm_data->import_ca_button = glade_xml_get_widget (cfm_data->gui, "authority-import-button");
	cfm_data->delete_ca_button = glade_xml_get_widget (cfm_data->gui, "authority-delete-button");

	initialize_yourcerts_ui(cfm_data);
	initialize_contactcerts_ui(cfm_data);
	initialize_authoritycerts_ui(cfm_data);

	populate_ui (cfm_data);

	control_widget = glade_xml_get_widget (cfm_data->gui, "cert-manager-notebook");
	gtk_widget_ref (control_widget);

	gtk_container_remove (GTK_CONTAINER (control_widget->parent), control_widget);

	return evolution_config_control_new (control_widget);
}
