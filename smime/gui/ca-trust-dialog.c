/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 *  Authors: Chris Toshok <toshok@ximian.com>
 *
 *  Copyright (C) 2004 Novell, Inc. (www.novell.com)
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

#include "ca-trust-dialog.h"
#include "certificate-viewer.h"

#include <gtk/gtk.h>

#include <libgnome/gnome-i18n.h>
#include <glade/glade.h>

#define GLADE_FILE_NAME "smime-ui.glade"

typedef struct {
	GladeXML *gui;
	GtkWidget *dialog;
	GtkWidget *ssl_checkbutton;
	GtkWidget *email_checkbutton;
	GtkWidget *objsign_checkbutton;
	GtkWidget *view_cert_button;

	ECert *cert;
} CATrustDialogData;

static void
free_data (gpointer data, GObject *where_the_object_was)
{
	CATrustDialogData *ctd = data;

	g_object_unref (ctd->cert);
	g_object_unref (ctd->gui);
	g_free (ctd);
}

static void
view_cert (GtkWidget *button, CATrustDialogData *data)
{
	GtkWidget *dialog = certificate_viewer_show (data->cert);

	gtk_window_set_transient_for (GTK_WINDOW (dialog), GTK_WINDOW (data->dialog));

	gtk_dialog_run (GTK_DIALOG (dialog));

	gtk_widget_destroy (dialog);
}

GtkWidget*
ca_trust_dialog_show (ECert *cert, gboolean importing)
{
	CATrustDialogData *ctd_data;

	ctd_data = g_new0 (CATrustDialogData, 1);
	ctd_data->gui = glade_xml_new (EVOLUTION_GLADEDIR "/" GLADE_FILE_NAME, NULL, NULL);

	ctd_data->dialog = glade_xml_get_widget (ctd_data->gui, "ca-trust-dialog");
	ctd_data->cert = g_object_ref (cert);

	ctd_data->ssl_checkbutton = glade_xml_get_widget (ctd_data->gui, "ssl_trust_checkbutton");
	ctd_data->email_checkbutton = glade_xml_get_widget (ctd_data->gui, "email_trust_checkbutton");
	ctd_data->objsign_checkbutton = glade_xml_get_widget (ctd_data->gui, "objsign_trust_checkbutton");
	ctd_data->view_cert_button = glade_xml_get_widget (ctd_data->gui, "view_certificate_button");

	g_signal_connect (ctd_data->view_cert_button,
			  "clicked", G_CALLBACK (view_cert),
			  ctd_data);

	gtk_widget_realize (ctd_data->dialog);
	gtk_container_set_border_width (GTK_CONTAINER (GTK_DIALOG (ctd_data->dialog)->action_area), 12);

	g_object_weak_ref (G_OBJECT (ctd_data->dialog), free_data, ctd_data);

	g_object_set_data (G_OBJECT (ctd_data->dialog), "CATrustDialogData", ctd_data);

	return ctd_data->dialog;
}

void
ca_trust_dialog_set_trust (GtkWidget *widget, gboolean ssl, gboolean email, gboolean objsign)
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
ca_trust_dialog_get_trust (GtkWidget *widget, gboolean *ssl, gboolean *email, gboolean *objsign)
{
	CATrustDialogData *ctd_data;

	ctd_data = g_object_get_data (G_OBJECT (widget), "CATrustDialogData");
	if (!ctd_data)
		return;

	*ssl = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (ctd_data->ssl_checkbutton));
	*email = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (ctd_data->email_checkbutton));
	*objsign = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (ctd_data->objsign_checkbutton));
}
