/*
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 *
 *
 * Authors:
 *		Chris Toshok <toshok@ximian.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#include "evolution-config.h"

#include "ca-trust-dialog.h"
#include "certificate-manager.h"

#include <glib/gi18n.h>

#include "e-util/e-util.h"
#include "e-util/e-util-private.h"

typedef struct {
	GtkBuilder *builder;
	GtkWidget *dialog;
	GtkWidget *ssl_checkbutton;
	GtkWidget *email_checkbutton;
	GtkWidget *objsign_checkbutton;

	ECert *cert;
} CATrustDialogData;

static void
free_data (gpointer data)
{
	CATrustDialogData *ctd = data;

	g_object_unref (ctd->cert);
	g_object_unref (ctd->builder);
	g_free (ctd);
}

static void
catd_response (GtkWidget *w,
               guint id,
               CATrustDialogData *data)
{
	switch (id) {
	case GTK_RESPONSE_ACCEPT: {
		GtkWidget *dialog;

		dialog = e_cert_manager_new_certificate_viewer (GTK_WINDOW (data->dialog), data->cert);

		g_signal_stop_emission_by_name (w, "response");
		gtk_dialog_run (GTK_DIALOG (dialog));
		gtk_widget_destroy (dialog);

		break; }
	}
}

GtkWidget *
ca_trust_dialog_show (ECert *cert,
                      gboolean importing)
{
	CATrustDialogData *ctd_data;
	GtkDialog *dialog;
	GtkWidget *action_area;
	GtkWidget *content_area;
	GtkWidget *w;
	gchar *txt;

	ctd_data = g_new0 (CATrustDialogData, 1);

	ctd_data->builder = gtk_builder_new ();
	e_load_ui_builder_definition (ctd_data->builder, "smime-ui.ui");

	ctd_data->dialog = e_builder_get_widget (ctd_data->builder, "ca-trust-dialog");

	dialog = GTK_DIALOG (ctd_data->dialog);
	action_area = gtk_dialog_get_action_area (dialog);
	content_area = gtk_dialog_get_content_area (dialog);

	gtk_container_set_border_width (GTK_CONTAINER (action_area), 12);
	gtk_container_set_border_width (GTK_CONTAINER (content_area), 0);

	ctd_data->cert = g_object_ref (cert);

	ctd_data->ssl_checkbutton = e_builder_get_widget (ctd_data->builder, "ssl_trust_checkbutton");
	ctd_data->email_checkbutton = e_builder_get_widget (ctd_data->builder, "email_trust_checkbutton");
	ctd_data->objsign_checkbutton = e_builder_get_widget (ctd_data->builder, "objsign_trust_checkbutton");

	w = e_builder_get_widget (ctd_data->builder, "ca-trust-label");
	txt = g_strdup_printf (_("Certificate “%s” is a CA certificate.\n\nEdit trust settings:"), e_cert_get_cn (cert));
	gtk_label_set_text ((GtkLabel *) w, txt);
	g_free (txt);

	g_signal_connect (
		ctd_data->dialog, "response",
		G_CALLBACK (catd_response), ctd_data);

	g_object_set_data_full (G_OBJECT (ctd_data->dialog), "CATrustDialogData", ctd_data, free_data);

	return ctd_data->dialog;
}

void
ca_trust_dialog_set_trust (GtkWidget *widget,
                           gboolean ssl,
                           gboolean email,
                           gboolean objsign)
{
	CATrustDialogData *ctd_data;

	ctd_data = g_object_get_data (G_OBJECT (widget), "CATrustDialogData");
	if (!ctd_data)
		return;

	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (ctd_data->ssl_checkbutton), ssl);
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (ctd_data->email_checkbutton), email);
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (ctd_data->objsign_checkbutton), objsign);
}

void
ca_trust_dialog_get_trust (GtkWidget *widget,
                           gboolean *ssl,
                           gboolean *email,
                           gboolean *objsign)
{
	CATrustDialogData *ctd_data;

	ctd_data = g_object_get_data (G_OBJECT (widget), "CATrustDialogData");
	if (!ctd_data)
		return;

	*ssl = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (ctd_data->ssl_checkbutton));
	*email = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (ctd_data->email_checkbutton));
	*objsign = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (ctd_data->objsign_checkbutton));
}
