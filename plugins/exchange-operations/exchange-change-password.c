/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */

/* Copyright (C) 2004 Novell, Inc.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of version 2 of the GNU General Public
 * License as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

/* exchange-change-password: Change Password code */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "exchange-change-password.h"

#include <exchange/exchange-account.h>
#include <exchange/e2k-utils.h>

#include <glade/glade-xml.h>
#include <gtk/gtkdialog.h>
#include <gtk/gtkentry.h>
#include <gtk/gtklabel.h>

#define FILENAME EVOLUTION_GLADEDIR "/exchange-change-password.glade"
#define ROOTNODE "pass_dialog"
#define STARTNODE "pass_vbox"

static void
entry_changed (GtkEntry *entry, gpointer user_data)
{
	GladeXML *xml = user_data;
	GtkEntry *new_entry, *confirm_entry;
	GtkWidget *ok_button;
	const char *text;

        new_entry = GTK_ENTRY (glade_xml_get_widget (xml, "new_pass_entry"));
        confirm_entry = GTK_ENTRY (glade_xml_get_widget (xml, "confirm_pass_entry"));
	ok_button = glade_xml_get_widget (xml, "okbutton1");

	text = gtk_entry_get_text (new_entry);
	if (!text || !*text) {
		gtk_widget_set_sensitive (ok_button, FALSE);
		return;
	}

	text = gtk_entry_get_text (confirm_entry);
	if (!text || !*text) {
		gtk_widget_set_sensitive (ok_button, FALSE);
		return;
	}

	gtk_widget_set_sensitive (ok_button, TRUE);
}

/**
 * exchange_get_new_password:
 * @existing_password: The user's current password
 * @voluntary: %TRUE if the user has chosen "Change Password",
 * %FALSE if their old password has expired.
 *
 * Prompt the user for a new password.
 */
char *
exchange_get_new_password (const char *existing_password, gboolean voluntary)
{
	GladeXML *xml;
	GtkWidget *top_widget;
	GtkEntry *cur_entry, *new_entry, *confirm_entry;
	GtkResponseType response;
	GtkLabel *top_label;
	char *new_pass;

	xml = glade_xml_new (FILENAME, ROOTNODE, NULL);
	top_widget = glade_xml_get_widget (xml, ROOTNODE);

        cur_entry = GTK_ENTRY (glade_xml_get_widget (xml, "current_pass_entry"));
        new_entry = GTK_ENTRY (glade_xml_get_widget (xml, "new_pass_entry"));
	g_signal_connect (new_entry, "changed",
			  G_CALLBACK (entry_changed), xml);
        confirm_entry = GTK_ENTRY (glade_xml_get_widget (xml, "confirm_pass_entry"));
	g_signal_connect (confirm_entry, "changed",
			  G_CALLBACK (entry_changed), xml);
	entry_changed (NULL, xml);

	top_label = GTK_LABEL (glade_xml_get_widget (xml, "pass_label"));
	if (voluntary)
		gtk_widget_hide (GTK_WIDGET (top_label));

run_dialog_again:	
	response = gtk_dialog_run (GTK_DIALOG (top_widget));
	if (response == GTK_RESPONSE_OK) {
		const char *cur_pass, *new_pass1, *new_pass2;

		cur_pass = gtk_entry_get_text (cur_entry);
		new_pass1 = gtk_entry_get_text (new_entry);
		new_pass2 = gtk_entry_get_text (confirm_entry);

		if (existing_password) {
			if (strcmp (cur_pass, existing_password) != 0) {
				/* User entered a wrong existing
				 * password. Prompt him again.
				 */
				gtk_label_set_text (top_label, _("The current password does not match the existing password for your account. Please enter the correct password"));
				gtk_widget_show (GTK_WIDGET (top_label));
				goto run_dialog_again;
			}
		}

		if (strcmp (new_pass1, new_pass2) != 0) {
			gtk_label_set_text (top_label, _("The two passwords do not match. Please re-enter the passwords."));
			gtk_widget_show (GTK_WIDGET (top_label));
			goto run_dialog_again;
		}

		new_pass = g_strdup (new_pass1);
	} else
		new_pass = NULL;

	gtk_widget_destroy (top_widget);
	g_object_unref (xml);

	return new_pass;
}
