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

#include "certificate-viewer.h"

#include "e-asn1-object.h"

#include <gtk/gtk.h>

#include <libgnome/gnome-i18n.h>
#include <glade/glade.h>

#define GLADE_FILE_NAME "smime-ui.glade"

typedef struct {
	GladeXML *gui;
	GtkWidget *dialog;
	GtkTreeStore *hierarchy_store, *fields_store;
	GtkWidget *hierarchy_tree, *fields_tree;
	GtkWidget *field_text;
	GtkTextTag *text_tag;

	GList *cert_chain;
} CertificateViewerData;

static void
free_data (gpointer data, GObject *where_the_object_was)
{
	CertificateViewerData *cvm = data;

	g_list_foreach (cvm->cert_chain, (GFunc)g_object_unref, NULL);
	g_list_free (cvm->cert_chain);

	g_object_unref (cvm->gui);
	g_free (cvm);
}

#define NOT_PART_OF_CERT_MARKUP "<i>&lt;Not part of certificate&gt;</i>"

static void
fill_in_general (CertificateViewerData *cvm_data, ECert *cert)
{
	CERTCertificate *mcert = e_cert_get_internal_cert (cert);
	GtkWidget *label;
	const char *text;
	char *markup;

	/* issued to */
	label = glade_xml_get_widget (cvm_data->gui, "issued-to-cn");
	if (e_cert_get_cn (cert)) {
		gtk_label_set_text (GTK_LABEL (label), e_cert_get_cn (cert));
	}
	else {
		gtk_label_set_markup (GTK_LABEL (label), NOT_PART_OF_CERT_MARKUP);
	}

	label = glade_xml_get_widget (cvm_data->gui, "issued-to-o");
	if (e_cert_get_org (cert)) {
		gtk_label_set_text (GTK_LABEL (label), e_cert_get_org (cert));
	}
	else {
		gtk_label_set_markup (GTK_LABEL (label), NOT_PART_OF_CERT_MARKUP);
	}

	label = glade_xml_get_widget (cvm_data->gui, "issued-to-ou");
	if (e_cert_get_org_unit (cert)) {
		gtk_label_set_text (GTK_LABEL (label), e_cert_get_org_unit (cert));
	}
	else {
		gtk_label_set_markup (GTK_LABEL (label), NOT_PART_OF_CERT_MARKUP);
	}

	text = e_cert_get_serial_number (cert);
	label = glade_xml_get_widget (cvm_data->gui, "issued-to-serial");
	gtk_label_set_text (GTK_LABEL (label), text);

	/* issued by */
	label = glade_xml_get_widget (cvm_data->gui, "issued-by-cn");
	if (e_cert_get_issuer_cn (cert)) {
		gtk_label_set_text (GTK_LABEL (label), e_cert_get_issuer_cn (cert));
	}
	else {
		gtk_label_set_markup (GTK_LABEL (label), NOT_PART_OF_CERT_MARKUP);
	}

	label = glade_xml_get_widget (cvm_data->gui, "issued-by-o");
	if (e_cert_get_issuer_org (cert)) {
			gtk_label_set_text (GTK_LABEL (label), e_cert_get_issuer_org (cert));
	}
	else {
		gtk_label_set_markup (GTK_LABEL (label), NOT_PART_OF_CERT_MARKUP);
	}
	
	label = glade_xml_get_widget (cvm_data->gui, "issued-by-ou");
	if (e_cert_get_issuer_org_unit (cert)) {
		gtk_label_set_text (GTK_LABEL (label), e_cert_get_issuer_org_unit (cert));
	}
	else {
		gtk_label_set_markup (GTK_LABEL (label), NOT_PART_OF_CERT_MARKUP);
	}

	/* validity */
	label = glade_xml_get_widget (cvm_data->gui, "validity-issued-on");
	if (e_cert_get_issued_on (cert)) {
		gtk_label_set_text (GTK_LABEL (label), e_cert_get_issued_on (cert));
	}
	else {
		gtk_label_set_markup (GTK_LABEL (label), NOT_PART_OF_CERT_MARKUP);
	}

	label = glade_xml_get_widget (cvm_data->gui, "validity-expires-on");
	if (e_cert_get_expires_on (cert)) {
		gtk_label_set_text (GTK_LABEL (label), e_cert_get_expires_on (cert));
	}
	else {
		gtk_label_set_markup (GTK_LABEL (label), NOT_PART_OF_CERT_MARKUP);
	}

	/* fingerprints */
	markup = g_strdup_printf ("<tt>%s</tt>", e_cert_get_sha1_fingerprint (cert));
	label = glade_xml_get_widget (cvm_data->gui, "fingerprints-sha1");
	gtk_label_set_markup (GTK_LABEL (label), markup);
	g_free (markup);

	markup = g_strdup_printf ("<tt>%s</tt>", e_cert_get_md5_fingerprint (cert));
	label = glade_xml_get_widget (cvm_data->gui, "fingerprints-md5");
	gtk_label_set_markup (GTK_LABEL (label), markup);
	g_free (markup);
}

static void
populate_fields_tree (CertificateViewerData *cvm_data, EASN1Object *asn1, GtkTreeIter *root)
{
	GtkTreeIter new_iter;

	/* first insert a node for the current asn1 */
	gtk_tree_store_insert (cvm_data->fields_store, &new_iter, root, -1);
	gtk_tree_store_set (cvm_data->fields_store, &new_iter,
			    0, e_asn1_object_get_display_name (asn1),
			    1, asn1,
			    -1);

	if (e_asn1_object_is_valid_container (asn1)) {
		GList *children = e_asn1_object_get_children (asn1);
		if (children) {
			GList *l;
			for (l = children; l; l = l->next) {
				EASN1Object *subasn1 = l->data;
				populate_fields_tree (cvm_data, subasn1, &new_iter);
			}
		}
		g_list_foreach (children, (GFunc)g_object_unref, NULL);
		g_list_free (children);
	}
}

static void
hierarchy_selection_changed (GtkTreeSelection *selection, CertificateViewerData *cvm_data)
{
	GtkTreeIter iter;
	GtkTreeModel *model;

	if (gtk_tree_selection_get_selected (selection,
					     &model,
					     &iter)) {
		EASN1Object *asn1_object;
		ECert *cert;

		gtk_tree_model_get (model,
				    &iter,
				    1, &cert,
				    -1);

		if (!cert)
			return;

		/* display the cert's ASN1 structure */
		asn1_object = e_cert_get_asn1_struct (cert);

		/* wipe out the old model */
		cvm_data->fields_store = gtk_tree_store_new (2, G_TYPE_STRING, G_TYPE_POINTER);
		gtk_tree_view_set_model (GTK_TREE_VIEW (cvm_data->fields_tree),
					 GTK_TREE_MODEL (cvm_data->fields_store));

		/* populate the fields from the newly selected cert */
		populate_fields_tree (cvm_data, asn1_object, NULL);
		gtk_tree_view_expand_all (GTK_TREE_VIEW (cvm_data->fields_tree));
		g_object_unref (asn1_object);

		/* and blow away the field value */
		gtk_text_buffer_set_text (gtk_text_view_get_buffer (GTK_TEXT_VIEW (cvm_data->field_text)),
					  "", 0);
	}	
}

static void
fields_selection_changed (GtkTreeSelection *selection, CertificateViewerData *cvm_data)
{
	GtkTreeIter iter;
	GtkTreeModel *model;

	if (gtk_tree_selection_get_selected (selection,
					     &model,
					     &iter)) {
		EASN1Object *asn1_object;
		const char *value;

		gtk_tree_model_get (model,
				    &iter,
				    1, &asn1_object,
				    -1);

		value = e_asn1_object_get_display_value (asn1_object);

		gtk_text_buffer_set_text (gtk_text_view_get_buffer (GTK_TEXT_VIEW (cvm_data->field_text)),
					  "", 0);

		if (value) {
			GtkTextIter text_iter;
			
			gtk_text_buffer_get_start_iter (gtk_text_view_get_buffer (GTK_TEXT_VIEW (cvm_data->field_text)),
							&text_iter);

			gtk_text_buffer_insert_with_tags (gtk_text_view_get_buffer (GTK_TEXT_VIEW (cvm_data->field_text)),
							  &text_iter,
							  value, strlen (value),
							  cvm_data->text_tag, NULL);
		}
	}
}

static void
fill_in_details (CertificateViewerData *cvm_data, ECert *cert)
{
	GList *l;
	GtkTreeIter *root = NULL;
	GtkTreeSelection *selection;

	/* hook up all the hierarchy tree foo */
	cvm_data->hierarchy_store = gtk_tree_store_new (2, G_TYPE_STRING, G_TYPE_OBJECT);
	cvm_data->hierarchy_tree = glade_xml_get_widget (cvm_data->gui, "cert-hierarchy-treeview");
	gtk_tree_view_set_model (GTK_TREE_VIEW (cvm_data->hierarchy_tree),
				 GTK_TREE_MODEL (cvm_data->hierarchy_store));

	gtk_tree_view_insert_column_with_attributes (GTK_TREE_VIEW (cvm_data->hierarchy_tree),
						     -1, "Cert", gtk_cell_renderer_text_new(),
						     "text", 0, NULL);

	selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (cvm_data->hierarchy_tree));
	g_signal_connect (selection, "changed", G_CALLBACK (hierarchy_selection_changed), cvm_data);

	/* hook up all the fields tree foo */
	cvm_data->fields_tree = glade_xml_get_widget (cvm_data->gui, "cert-fields-treeview");

	gtk_tree_view_insert_column_with_attributes (GTK_TREE_VIEW (cvm_data->fields_tree),
						     -1, "Field", gtk_cell_renderer_text_new(),
						     "text", 0, NULL);

	selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (cvm_data->fields_tree));
	g_signal_connect (selection, "changed", G_CALLBACK (fields_selection_changed), cvm_data);

	/* hook up all the field display foo */
	cvm_data->field_text = glade_xml_get_widget (cvm_data->gui, "cert-field-value-textview");

	/* set the font of the field value viewer to be some fixed
	   width font to the hex display doesn't look like ass. */
	cvm_data->text_tag = gtk_text_buffer_create_tag (gtk_text_view_get_buffer (GTK_TEXT_VIEW (cvm_data->field_text)),
							 "mono",
							 "font", "Mono",
							 NULL);

	/* initially populate the hierarchy from the cert's chain */
	cvm_data->cert_chain = e_cert_get_chain (cert);
	cvm_data->cert_chain = g_list_reverse (cvm_data->cert_chain);
	for (l = cvm_data->cert_chain; l; l = l->next) {
		ECert *c = l->data;
		const char *str;
		GtkTreeIter new_iter;

		str = e_cert_get_cn (c);
		if (!str)
			str = e_cert_get_subject_name (c);

		gtk_tree_store_insert (cvm_data->hierarchy_store, &new_iter, root, -1);
		gtk_tree_store_set (cvm_data->hierarchy_store, &new_iter,
				    0, str,
				    1, c,
				    -1);

		root = &new_iter;
	}

	gtk_tree_view_expand_all (GTK_TREE_VIEW (cvm_data->hierarchy_tree));
}

GtkWidget*
certificate_viewer_show (ECert *cert)
{
	CertificateViewerData *cvm_data;
	char *title;

	cvm_data = g_new0 (CertificateViewerData, 1);
	cvm_data->gui = glade_xml_new (EVOLUTION_GLADEDIR "/" GLADE_FILE_NAME, NULL, NULL);

	cvm_data->dialog = glade_xml_get_widget (cvm_data->gui, "certificate-viewer-dialog");

	gtk_widget_realize (cvm_data->dialog);
	gtk_container_set_border_width (GTK_CONTAINER (GTK_DIALOG (cvm_data->dialog)->action_area), 12);

	title = g_strdup_printf (_("Certificate Viewer: %s"), e_cert_get_window_title (cert));

	gtk_window_set_title (GTK_WINDOW (cvm_data->dialog),
			      title);
	
	g_free (title);

	fill_in_general (cvm_data, cert);
	fill_in_details (cvm_data, cert);

	g_object_weak_ref (G_OBJECT (cvm_data->dialog), free_data, cvm_data);

	/*	gtk_widget_show (cvm_data->dialog);*/
	return cvm_data->dialog;
}
