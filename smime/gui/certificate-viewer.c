/*
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
 *		Chris Toshok <toshok@ximian.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <glib/gi18n.h>

#include "pk11pub.h"
#include "hasht.h"

#include <libedataserver/libedataserver.h>

#include "e-asn1-object.h"
#include "certificate-viewer.h"

#define CERTIFICATE_VIEWER_PRIV_KEY "CertificateViewerPriv-key"

typedef struct _CertificateViewerPriv
{
	GtkWidget *issued_to_cn;
	GtkWidget *issued_to_o;
	GtkWidget *issued_to_ou;
	GtkWidget *issued_to_serial;
	GtkWidget *issued_by_cn;
	GtkWidget *issued_by_o;
	GtkWidget *issued_by_ou;
	GtkWidget *validity_issued_on;
	GtkWidget *validity_expires_on;
	GtkWidget *fingerprints_sha1;
	GtkWidget *fingerprints_md5;
	GtkWidget *cert_hierarchy_treeview;
	GtkWidget *cert_fields_treeview;
	GtkWidget *cert_field_value_textview;

	CERTCertificate *cert;
	GSList *issuers;
	GtkTextTag *monospace_tag;
} CertificateViewerPriv;

static void
free_priv_struct (gpointer ptr)
{
	CertificateViewerPriv *priv = ptr;
	GSList *iter;

	if (!priv)
		return;

	if (priv->cert)
		CERT_DestroyCertificate (priv->cert);

	for (iter = priv->issuers; iter; iter = iter->next) {
		CERTCertificate *cert = iter->data;

		if (cert)
			CERT_DestroyCertificate (cert);
	}

	g_slist_free (priv->issuers);

	g_free (priv);
}

static void
begin_section (GtkGrid *add_to,
               const gchar *caption,
               gint *from_row,
               gint for_rows)
{
	GtkWidget *widget;
	PangoAttribute *attr;
	PangoAttrList *bold;

	g_return_if_fail (add_to != NULL);
	g_return_if_fail (caption != NULL);
	g_return_if_fail (from_row != NULL);

	bold = pango_attr_list_new ();
	attr = pango_attr_weight_new (PANGO_WEIGHT_BOLD);
	pango_attr_list_insert (bold, attr);

	widget = gtk_label_new (caption);
	g_object_set (
		G_OBJECT (widget),
		"hexpand", TRUE,
		"halign", GTK_ALIGN_START,
		"justify", GTK_JUSTIFY_LEFT,
		"attributes", bold,
		"ellipsize", PANGO_ELLIPSIZE_NONE,
		NULL);

	pango_attr_list_unref (bold);

	gtk_grid_attach (add_to, widget, 0, *from_row, 3, 1);
	(*from_row)++;

	widget = gtk_alignment_new (0.0, 0.0, 0.0, 0.0);
	gtk_alignment_set_padding (GTK_ALIGNMENT (widget), 0, 0, 12, 0);

	gtk_grid_attach (add_to, widget, 0, *from_row, 1, for_rows);
}

static GtkWidget *
add_info_label (GtkGrid *add_to,
                const gchar *caption,
                gint *at_row)
{
	GtkWidget *widget;

	g_return_val_if_fail (add_to != NULL, NULL);
	g_return_val_if_fail (at_row != NULL, NULL);

	if (caption) {
		widget = gtk_label_new (caption);
		g_object_set (
			G_OBJECT (widget),
			"hexpand", FALSE,
			"halign", GTK_ALIGN_START,
			"justify", GTK_JUSTIFY_LEFT,
			"ellipsize", PANGO_ELLIPSIZE_NONE,
			NULL);

		gtk_grid_attach (add_to, widget, 1, *at_row, 1, 1);
	}

	widget = gtk_label_new ("");
	g_object_set (
		G_OBJECT (widget),
		"hexpand", TRUE,
		"halign", GTK_ALIGN_START,
		"justify", GTK_JUSTIFY_LEFT,
		"ellipsize", PANGO_ELLIPSIZE_NONE,
		"selectable", caption != NULL,
		NULL);

	gtk_grid_attach (add_to, widget, caption ? 2 : 1, *at_row, caption ? 1 : 2, 1);

	(*at_row)++;

	return widget;
}

static GtkWidget *
add_scrolled_window (GtkGrid *add_to,
                     const gchar *caption,
                     gint *at_row,
                     GtkWidget *add_widget)
{
	GtkWidget *widget;
	PangoAttribute *attr;
	PangoAttrList *bold;

	g_return_val_if_fail (add_to != NULL, NULL);
	g_return_val_if_fail (caption != NULL, NULL);
	g_return_val_if_fail (at_row != NULL, NULL);

	bold = pango_attr_list_new ();
	attr = pango_attr_weight_new (PANGO_WEIGHT_BOLD);
	pango_attr_list_insert (bold, attr);

	widget = gtk_label_new (caption);
	g_object_set (
		G_OBJECT (widget),
		"hexpand", TRUE,
		"halign", GTK_ALIGN_START,
		"justify", GTK_JUSTIFY_LEFT,
		"attributes", bold,
		"ellipsize", PANGO_ELLIPSIZE_NONE,
		NULL);

	pango_attr_list_unref (bold);

	gtk_grid_attach (add_to, widget, 0, *at_row, 1, 1);
	(*at_row)++;

	widget = gtk_scrolled_window_new (NULL, NULL);
	g_object_set (
		G_OBJECT (widget),
		"hexpand", TRUE,
		"halign", GTK_ALIGN_FILL,
		"vexpand", TRUE,
		"valign", GTK_ALIGN_FILL,
		"hscrollbar-policy", GTK_POLICY_AUTOMATIC,
		"vscrollbar-policy", GTK_POLICY_AUTOMATIC,
		"shadow-type", GTK_SHADOW_ETCHED_IN,
		NULL);

	gtk_scrolled_window_add_with_viewport (GTK_SCROLLED_WINDOW (widget), add_widget);

	gtk_grid_attach (add_to, widget, 0, *at_row, 1, 1);
	(*at_row)++;

	return add_widget;
}

#define FLAG_NONE	 (0)
#define FLAG_PORT_MEMORY (1 << 0)
#define FLAG_MARKUP	 (1 << 1)

static void
set_label_text (GtkWidget *label,
                const gchar *not_part_markup,
                gchar *text,
                guint32 flags)
{
	if (text) {
		if ((flags & FLAG_MARKUP) != 0)
			gtk_label_set_markup (GTK_LABEL (label), text);
		else
			gtk_label_set_text (GTK_LABEL (label), text);

		if ((flags & FLAG_PORT_MEMORY) != 0)
			PORT_Free (text);
		else
			g_free (text);
	} else {
		gtk_label_set_markup (GTK_LABEL (label), not_part_markup);
	}
}

static void
get_cert_times (CERTCertificate *cert,
                gchar **issued_on,
                gchar **expires_on)
{
	PRTime time_issued_on;
	PRTime time_expires_on;
	PRExplodedTime explodedTime;
	struct tm exploded_tm;
	gchar buf[128];

	g_return_if_fail (cert != NULL);
	g_return_if_fail (issued_on != NULL);
	g_return_if_fail (expires_on != NULL);

	if (SECSuccess != CERT_GetCertTimes (cert, &time_issued_on, &time_expires_on))
		return;

	PR_ExplodeTime (time_issued_on, PR_LocalTimeParameters, &explodedTime);
	exploded_tm.tm_sec = explodedTime.tm_sec;
	exploded_tm.tm_min = explodedTime.tm_min;
	exploded_tm.tm_hour = explodedTime.tm_hour;
	exploded_tm.tm_mday = explodedTime.tm_mday;
	exploded_tm.tm_mon = explodedTime.tm_month;
	exploded_tm.tm_year = explodedTime.tm_year - 1900;
	e_utf8_strftime (buf, sizeof (buf), "%x", &exploded_tm);
	*issued_on = g_strdup (buf);

	PR_ExplodeTime (time_expires_on, PR_LocalTimeParameters, &explodedTime);
	exploded_tm.tm_sec = explodedTime.tm_sec;
	exploded_tm.tm_min = explodedTime.tm_min;
	exploded_tm.tm_hour = explodedTime.tm_hour;
	exploded_tm.tm_mday = explodedTime.tm_mday;
	exploded_tm.tm_mon = explodedTime.tm_month;
	exploded_tm.tm_year = explodedTime.tm_year - 1900;
	e_utf8_strftime (buf, sizeof (buf), "%x", &exploded_tm);
	*expires_on = g_strdup (buf);
}

static void
fill_general_page (CertificateViewerPriv *priv)
{
	gchar *not_part_markup;
	gchar *issued_on = NULL;
	gchar *expires_on = NULL;
	gchar *port_str;
	guchar fingerprint[128];
	SECItem fpItem;

	g_return_if_fail (priv != NULL);

	not_part_markup = g_strconcat ("<i>&lt;", _("Not part of certificate"), "&gt;</i>", NULL);

	set_label_text (priv->issued_to_cn, not_part_markup, CERT_GetCommonName (&priv->cert->subject), FLAG_PORT_MEMORY);
	set_label_text (priv->issued_to_o, not_part_markup, CERT_GetOrgName (&priv->cert->subject), FLAG_PORT_MEMORY);
	set_label_text (priv->issued_to_ou, not_part_markup, CERT_GetOrgUnitName (&priv->cert->subject), FLAG_PORT_MEMORY);
	set_label_text (priv->issued_to_serial, not_part_markup, CERT_Hexify (&priv->cert->serialNumber, TRUE), FLAG_PORT_MEMORY);

	set_label_text (priv->issued_by_cn, not_part_markup, CERT_GetCommonName (&priv->cert->issuer), FLAG_PORT_MEMORY);
	set_label_text (priv->issued_by_o, not_part_markup, CERT_GetOrgName (&priv->cert->issuer), FLAG_PORT_MEMORY);
	set_label_text (priv->issued_by_ou, not_part_markup, CERT_GetOrgUnitName (&priv->cert->issuer), FLAG_PORT_MEMORY);

	get_cert_times (priv->cert, &issued_on, &expires_on);
	set_label_text (priv->validity_issued_on, not_part_markup, issued_on, FLAG_NONE);
	set_label_text (priv->validity_expires_on, not_part_markup, expires_on, FLAG_NONE);

	memset (fingerprint, 0, sizeof fingerprint);
	PK11_HashBuf (
		SEC_OID_SHA1, fingerprint,
		priv->cert->derCert.data,
		priv->cert->derCert.len);
	fpItem.data = fingerprint;
	fpItem.len = SHA1_LENGTH;
	port_str = CERT_Hexify (&fpItem, TRUE);
	set_label_text (priv->fingerprints_sha1, not_part_markup, g_strconcat ("<tt>", port_str, "</tt>", NULL), FLAG_MARKUP);
	PORT_Free (port_str);

	memset (fingerprint, 0, sizeof fingerprint);
	PK11_HashBuf (
		SEC_OID_MD5, fingerprint,
		priv->cert->derCert.data,
		priv->cert->derCert.len);
	fpItem.data = fingerprint;
	fpItem.len = MD5_LENGTH;
	port_str = CERT_Hexify (&fpItem, TRUE);
	set_label_text (priv->fingerprints_md5, not_part_markup,  g_strconcat ("<tt>", port_str, "</tt>", NULL), FLAG_MARKUP);
	PORT_Free (port_str);

	g_free (not_part_markup);
}

static void
populate_fields_tree (CertificateViewerPriv *priv,
                      EASN1Object *asn1,
                      GtkTreeIter *root)
{
	GtkTreeStore *fields_store;
	GtkTreeIter new_iter;

	if (!asn1)
		return;

	fields_store = GTK_TREE_STORE (gtk_tree_view_get_model (GTK_TREE_VIEW (priv->cert_fields_treeview)));

	/* first insert a node for the current asn1 */
	gtk_tree_store_insert (fields_store, &new_iter, root, -1);
	gtk_tree_store_set (
		fields_store, &new_iter,
		0, e_asn1_object_get_display_name (asn1),
		1, asn1,
		-1);

	if (e_asn1_object_is_valid_container (asn1)) {
		GList *children = e_asn1_object_get_children (asn1);

		if (children) {
			GList *iter;
			for (iter = children; iter; iter = iter->next) {
				EASN1Object *subasn1 = iter->data;

				populate_fields_tree (priv, subasn1, &new_iter);
			}
		}

		g_list_free_full (children, g_object_unref);
	}
}

static void
hierarchy_selection_changed_cb (GtkTreeSelection *selection,
                                CertificateViewerPriv *priv)
{
	GtkTreeIter iter;
	GtkTreeModel *model;

	if (gtk_tree_selection_get_selected (selection, &model, &iter)) {
		CERTCertificate *cert;
		EASN1Object *asn1;
		GtkTreeStore *fields_store;

		gtk_tree_model_get (model, &iter, 1, &cert, -1);

		if (!cert)
			return;

		/* display the cert's ASN1 structure */
		asn1 = e_asn1_object_new_from_cert (cert);

		/* wipe out the old model */
		fields_store = gtk_tree_store_new (2, G_TYPE_STRING, G_TYPE_OBJECT);
		gtk_tree_view_set_model (
			GTK_TREE_VIEW (priv->cert_fields_treeview),
			GTK_TREE_MODEL (fields_store));

		/* populate the fields from the newly selected cert */
		populate_fields_tree (priv, asn1, NULL);
		gtk_tree_view_expand_all (GTK_TREE_VIEW (priv->cert_fields_treeview));
		if (asn1)
			g_object_unref (asn1);

		/* and blow away the field value */
		gtk_text_buffer_set_text (
			gtk_text_view_get_buffer (GTK_TEXT_VIEW (priv->cert_field_value_textview)),
			"", 0);
	}
}

static void
fields_selection_changed_cb (GtkTreeSelection *selection,
                             CertificateViewerPriv *priv)
{
	GtkTreeIter iter;
	GtkTreeModel *model;

	if (gtk_tree_selection_get_selected (selection, &model, &iter)) {
		EASN1Object *asn1 = NULL;
		const gchar *value = NULL;
		GtkTextView *textview;
		GtkTextBuffer *textbuffer;

		gtk_tree_model_get (model, &iter, 1, &asn1, -1);

		if (asn1)
			value = e_asn1_object_get_display_value (asn1);

		textview = GTK_TEXT_VIEW (priv->cert_field_value_textview);
		textbuffer = gtk_text_view_get_buffer (textview);

		gtk_text_buffer_set_text (textbuffer, "", 0);

		if (value) {
			GtkTextIter text_iter;

			gtk_text_buffer_get_start_iter (textbuffer, &text_iter);

			gtk_text_buffer_insert_with_tags (
				textbuffer, &text_iter,
				value, strlen (value),
				priv->monospace_tag, NULL);
		}

		if (asn1)
			g_object_unref (asn1);
	}
}

static void
fill_details_page (CertificateViewerPriv *priv)
{
	GSList *iter;
	GtkTreeIter root;
	GtkTreeSelection *selection;
	gboolean root_set = FALSE;
	GtkTreeStore *hierarchy_store;

	g_return_if_fail (priv != NULL);

	gtk_tree_view_set_headers_visible (GTK_TREE_VIEW (priv->cert_hierarchy_treeview), FALSE);

	hierarchy_store = gtk_tree_store_new (2, G_TYPE_STRING, G_TYPE_POINTER);
	gtk_tree_view_set_model (
		GTK_TREE_VIEW (priv->cert_hierarchy_treeview),
		GTK_TREE_MODEL (hierarchy_store));

	gtk_tree_view_insert_column_with_attributes (
		GTK_TREE_VIEW (priv->cert_hierarchy_treeview),
		-1, "Cert", gtk_cell_renderer_text_new (),
		"text", 0, NULL);

	selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (priv->cert_hierarchy_treeview));
	g_signal_connect (
		selection, "changed",
		G_CALLBACK (hierarchy_selection_changed_cb), priv);

	gtk_tree_view_set_headers_visible (GTK_TREE_VIEW (priv->cert_fields_treeview), FALSE);

	gtk_tree_view_insert_column_with_attributes (
		GTK_TREE_VIEW (priv->cert_fields_treeview),
		-1, "Field", gtk_cell_renderer_text_new (),
		"text", 0, NULL);

	selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (priv->cert_fields_treeview));
	g_signal_connect (
		selection, "changed",
		G_CALLBACK (fields_selection_changed_cb), priv);

	/* set the font of the field value viewer to be some fixed
	 * width font to the hex display looks nice. */
	priv->monospace_tag = gtk_text_buffer_create_tag (
		gtk_text_view_get_buffer (GTK_TEXT_VIEW (priv->cert_field_value_textview)),
		"mono", "font", "Mono", NULL);

	/* initially populate the hierarchy from the issuers' chain */
	for (iter = priv->issuers; iter; iter = g_slist_next (iter)) {
		CERTCertificate *cert = iter->data;
		gchar *str;
		GtkTreeIter new_iter;

		if (!cert)
			continue;

		str = CERT_GetCommonName (&cert->subject);

		gtk_tree_store_insert (hierarchy_store, &new_iter, root_set ? &root : NULL, -1);
		gtk_tree_store_set (
			hierarchy_store, &new_iter,
			0, str ? str : cert->subjectName,
			1, cert,
			-1);

		root = new_iter;
		root_set = TRUE;

		if (str)
			PORT_Free (str);
	}

	gtk_tree_view_expand_all (GTK_TREE_VIEW (priv->cert_hierarchy_treeview));
}

static gchar *
get_window_title (CERTCertificate *cert)
{
	gchar *str;

	g_return_val_if_fail (cert != NULL, NULL);

	if (cert->nickname)
		return g_strdup (cert->nickname);

	str = CERT_GetCommonName (&cert->subject);
	if (str) {
		gchar *title;

		title = g_strdup (str);
		PORT_Free (str);

		return title;
	}

	return g_strdup (cert->subjectName);
}

GtkWidget *
certificate_viewer_new (GtkWindow *parent,
                        const CERTCertificate *cert,
                        const GSList *issuers_chain_certs)
{
	CertificateViewerPriv *priv;
	GtkWidget *dialog, *notebook, *widget;
	GtkGrid *grid;
	gint row;
	GSList *iter;
	gchar *title;

	g_return_val_if_fail (cert != NULL, NULL);

	priv = g_new0 (CertificateViewerPriv, 1);
	priv->cert = CERT_DupCertificate ((CERTCertificate *) cert);
	priv->issuers = g_slist_copy ((GSList *) issuers_chain_certs);

	/* root issuer first, then bottom down to certificate itself */
	priv->issuers = g_slist_reverse (priv->issuers);
	priv->issuers = g_slist_append (priv->issuers, priv->cert);

	for (iter = priv->issuers; iter; iter = g_slist_next (iter)) {
		iter->data = CERT_DupCertificate (iter->data);
	}

	title = get_window_title (priv->cert);

	dialog = gtk_dialog_new_with_buttons (
		title, parent,
		GTK_DIALOG_DESTROY_WITH_PARENT | GTK_DIALOG_MODAL,
		GTK_STOCK_CLOSE, GTK_RESPONSE_CLOSE,
		NULL);

	g_free (title);

	g_object_set_data_full (G_OBJECT (dialog), CERTIFICATE_VIEWER_PRIV_KEY, priv, free_priv_struct);

	notebook = gtk_notebook_new ();
	gtk_container_add (GTK_CONTAINER (gtk_dialog_get_content_area (GTK_DIALOG (dialog))), notebook);
	gtk_container_set_border_width (GTK_CONTAINER (notebook), 12);

	/* General page */
	row = 0;
	grid = GTK_GRID (gtk_grid_new ());
	g_object_set (
		G_OBJECT (grid),
		"hexpand", TRUE,
		"halign", GTK_ALIGN_FILL,
		"vexpand", FALSE,
		"valign", GTK_ALIGN_START,
		"border-width", 12,
		"row-spacing", 6,
		"column-spacing", 6,
		NULL);

	begin_section (grid, _("This certificate has been verified for the following uses:"), &row, 4);

	if (!priv->cert->keyUsagePresent || (priv->cert->keyUsage & certificateUsageSSLClient) != 0) {
		widget = add_info_label (grid, NULL, &row);
		gtk_label_set_text (GTK_LABEL (widget), _("SSL Client Certificate"));
	}

	if (!priv->cert->keyUsagePresent || (priv->cert->keyUsage & (certificateUsageSSLServer | certificateUsageSSLCA)) != 0) {
		widget = add_info_label (grid, NULL, &row);
		gtk_label_set_text (GTK_LABEL (widget), _("SSL Server Certificate"));
	}

	if (!priv->cert->keyUsagePresent || (priv->cert->keyUsage & certificateUsageEmailSigner) != 0) {
		widget = add_info_label (grid, NULL, &row);
		gtk_label_set_text (GTK_LABEL (widget), _("Email Signer Certificate"));
	}

	if (!priv->cert->keyUsagePresent || (priv->cert->keyUsage & certificateUsageEmailRecipient) != 0) {
		widget = add_info_label (grid, NULL, &row);
		gtk_label_set_text (GTK_LABEL (widget), _("Email Recipient Certificate"));
	}

	widget = gtk_separator_new (GTK_ORIENTATION_HORIZONTAL);
	g_object_set (
		G_OBJECT (widget),
		"hexpand", TRUE,
		"halign", GTK_ALIGN_FILL,
		"vexpand", FALSE,
		"valign", GTK_ALIGN_START,
		NULL);

	gtk_grid_attach (grid, widget, 0, row, 3, 1);
	row++;

	begin_section (grid, _("Issued To"), &row, 4);
	priv->issued_to_cn = add_info_label (grid, _("Common Name (CN)"), &row);
	priv->issued_to_o = add_info_label (grid, _("Organization (O)"), &row);
	priv->issued_to_ou = add_info_label (grid, _("Organizational Unit (OU)"), &row);
	priv->issued_to_serial = add_info_label (grid, _("Serial Number"), &row);

	begin_section (grid, _("Issued By"), &row, 3);
	priv->issued_by_cn = add_info_label (grid, _("Common Name (CN)"), &row);
	priv->issued_by_o = add_info_label (grid, _("Organization (O)"), &row);
	priv->issued_by_ou = add_info_label (grid, _("Organizational Unit (OU)"), &row);

	begin_section (grid, _("Validity"), &row, 2);
	priv->validity_issued_on = add_info_label (grid, _("Issued On"), &row);
	priv->validity_expires_on = add_info_label (grid, _("Expires On"), &row);

	begin_section (grid, _("Fingerprints"), &row, 2);
	priv->fingerprints_sha1 = add_info_label (grid, _("SHA1 Fingerprint"), &row);
	priv->fingerprints_md5 = add_info_label (grid, _("MD5 Fingerprint"), &row);

	widget = gtk_label_new (_("General"));
	gtk_notebook_append_page (GTK_NOTEBOOK (notebook), GTK_WIDGET (grid), widget);

	/* Details page */
	row = 0;
	grid = GTK_GRID (gtk_grid_new ());
	g_object_set (
		G_OBJECT (grid),
		"hexpand", TRUE,
		"halign", GTK_ALIGN_FILL,
		"vexpand", TRUE,
		"valign", GTK_ALIGN_FILL,
		"border-width", 12,
		"row-spacing", 6,
		"column-spacing", 6,
		NULL);

	priv->cert_hierarchy_treeview = add_scrolled_window (
		grid, _("Certificate Hierarchy"), &row, gtk_tree_view_new ());

	priv->cert_fields_treeview = add_scrolled_window (
		grid, _("Certificate Fields"), &row, gtk_tree_view_new ());

	priv->cert_field_value_textview = add_scrolled_window (
		grid, _("Field Value"), &row, gtk_text_view_new ());

	widget = gtk_label_new (_("Details"));
	gtk_notebook_append_page (GTK_NOTEBOOK (notebook), GTK_WIDGET (grid), widget);

	gtk_widget_show_all (notebook);

	fill_general_page (priv);
	fill_details_page (priv);

	return dialog;
}
