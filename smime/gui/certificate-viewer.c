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

#include "certificate-viewer.h"

#include <gtk/gtk.h>

#include <libgnome/gnome-i18n.h>
#include <glade/glade.h>

#define GLADE_FILE_NAME "smime-ui.glade"

typedef struct {
	GladeXML *gui;
	GtkWidget *dialog;
} CertificateViewerData;

static void
free_data (gpointer data, GObject *where_the_object_was)
{
	CertificateViewerData *cvm = data;

	g_object_unref (cvm->gui);
	g_free (cvm);
}


GtkWidget*
certificate_viewer_show (ECert *cert)
{
	CertificateViewerData *cvm_data;
	CERTCertificate *mcert;
	GtkWidget *label;
	const char *text;

	mcert = e_cert_get_internal_cert (cert);

	cvm_data = g_new0 (CertificateViewerData, 1);
	cvm_data->gui = glade_xml_new (EVOLUTION_GLADEDIR "/" GLADE_FILE_NAME, NULL, NULL);

	cvm_data->dialog = glade_xml_get_widget (cvm_data->gui, "certificate-viewer-dialog");

	/* issued to */
	if (e_cert_get_cn (cert)) {
		label = glade_xml_get_widget (cvm_data->gui, "issued-to-cn");
		gtk_label_set_text (GTK_LABEL (label), e_cert_get_cn (cert));
	}

	if (e_cert_get_org (cert)) {
		label = glade_xml_get_widget (cvm_data->gui, "issued-to-o");
		gtk_label_set_text (GTK_LABEL (label), e_cert_get_org (cert));
	}

	if (e_cert_get_org_unit (cert)) {
		label = glade_xml_get_widget (cvm_data->gui, "issued-to-ou");
		gtk_label_set_text (GTK_LABEL (label), e_cert_get_org_unit (cert));
	}

	text = e_cert_get_serial_number (cert);
	label = glade_xml_get_widget (cvm_data->gui, "issued-to-serial");
	gtk_label_set_text (GTK_LABEL (label), text);

	/* issued by */
	if (e_cert_get_issuer_cn (cert)) {
		label = glade_xml_get_widget (cvm_data->gui, "issued-by-cn");
		gtk_label_set_text (GTK_LABEL (label), e_cert_get_issuer_cn (cert));
	}

	if (e_cert_get_issuer_org (cert)) {
		label = glade_xml_get_widget (cvm_data->gui, "issued-by-o");
		gtk_label_set_text (GTK_LABEL (label), e_cert_get_issuer_org (cert));
	}

	if (e_cert_get_issuer_org_unit (cert)) {
		label = glade_xml_get_widget (cvm_data->gui, "issued-by-ou");
		gtk_label_set_text (GTK_LABEL (label), e_cert_get_issuer_org_unit (cert));
	}

	/* validity */
	if (e_cert_get_issued_on (cert)) {
		label = glade_xml_get_widget (cvm_data->gui, "validity-issued-on");
		gtk_label_set_text (GTK_LABEL (label), e_cert_get_issued_on (cert));
	}

	if (e_cert_get_expires_on (cert)) {
		label = glade_xml_get_widget (cvm_data->gui, "validity-expires-on");
		gtk_label_set_text (GTK_LABEL (label), e_cert_get_expires_on (cert));
	}

	/* fingerprints */
	text = e_cert_get_sha1_fingerprint (cert);
	label = glade_xml_get_widget (cvm_data->gui, "fingerprints-sha1");
	gtk_label_set_text (GTK_LABEL (label), text);

	text = e_cert_get_md5_fingerprint (cert);
	label = glade_xml_get_widget (cvm_data->gui, "fingerprints-md5");
	gtk_label_set_text (GTK_LABEL (label), text);

	g_object_weak_ref (G_OBJECT (cvm_data->dialog), free_data, cvm_data);

	g_signal_connect (cvm_data->dialog, "response",
			  G_CALLBACK (gtk_widget_destroy), NULL);

	gtk_widget_show (cvm_data->dialog);
	return cvm_data->dialog;
}
