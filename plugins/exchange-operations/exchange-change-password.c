/*
 *
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
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

/* exchange-change-password: Change Password code */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "exchange-change-password.h"

#include <exchange-account.h>
#include <e2k-utils.h>

#include <gtk/gtk.h>

static void
entry_changed (GtkEntry *entry, gpointer user_data)
{
	GtkEntry *new_entry, *confirm_entry;
	GtkDialog *pass_dialog;
	const gchar *text;

        new_entry = GTK_ENTRY (entry);
        confirm_entry = GTK_ENTRY (user_data);
	pass_dialog = GTK_DIALOG (g_object_get_data (G_OBJECT (new_entry), "pass_dialog"));

	text = gtk_entry_get_text (new_entry);
	if (!text || !*text) {
		gtk_dialog_set_response_sensitive (pass_dialog, GTK_RESPONSE_OK, FALSE);
		return;
	}

	text = gtk_entry_get_text (confirm_entry);
	if (!text || !*text) {
		gtk_dialog_set_response_sensitive (pass_dialog, GTK_RESPONSE_OK, FALSE);
		return;
	}

	gtk_dialog_set_response_sensitive (pass_dialog, GTK_RESPONSE_OK, TRUE);
}

/**
 * exchange_get_new_password:
 * @existing_password: The user's current password
 * @voluntary: %TRUE if the user has chosen "Change Password",
 * %FALSE if their old password has expired.
 *
 * Prompt the user for a new password.
 */
gchar *
exchange_get_new_password (const gchar *existing_password, gboolean voluntary)
{
	GtkResponseType response;
	gchar *new_pass;
	GtkWidget *pass_dialog;
	GtkWidget *dialog_vbox1;
	GtkWidget *pass_label;
	GtkWidget *table1;
	GtkWidget *current_pass_label;
	GtkWidget *new_pass_label;
	GtkWidget *confirm_pass_label;
	GtkWidget *current_pass_entry;
	GtkWidget *new_pass_entry;
	GtkWidget *confirm_pass_entry;

	pass_dialog = gtk_dialog_new_with_buttons (
		_("Change Password"),
		NULL,
		GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
		GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
		GTK_STOCK_OK, GTK_RESPONSE_OK,
		NULL);

	dialog_vbox1 = gtk_dialog_get_content_area (GTK_DIALOG (pass_dialog));
	gtk_widget_show (dialog_vbox1);

	pass_label = gtk_label_new (_("Your current password has expired. Please change your password now."));
	gtk_widget_show (pass_label);
	gtk_box_pack_start (GTK_BOX (dialog_vbox1), pass_label, FALSE, FALSE, 0);
	gtk_label_set_justify (GTK_LABEL (pass_label), GTK_JUSTIFY_CENTER);
	gtk_label_set_line_wrap (GTK_LABEL (pass_label), TRUE);
	gtk_misc_set_alignment (GTK_MISC (pass_label), 0.52, 0.5);
	gtk_misc_set_padding (GTK_MISC (pass_label), 0, 6);

	table1 = gtk_table_new (3, 2, FALSE);
	gtk_widget_show (table1);
	gtk_box_pack_start (GTK_BOX (dialog_vbox1), table1, TRUE, TRUE, 0);
	gtk_container_set_border_width (GTK_CONTAINER (table1), 6);
	gtk_table_set_row_spacings (GTK_TABLE (table1), 6);
	gtk_table_set_col_spacings (GTK_TABLE (table1), 6);

	current_pass_label = gtk_label_new_with_mnemonic (_("Current _Password:"));
	gtk_widget_show (current_pass_label);
	gtk_table_attach (GTK_TABLE (table1), current_pass_label, 0, 1, 0, 1,
			  (GtkAttachOptions) (GTK_FILL),
			  (GtkAttachOptions) (0), 0, 0);
	gtk_misc_set_alignment (GTK_MISC (current_pass_label), 0, 0.5);

	new_pass_label = gtk_label_new_with_mnemonic (_("_New Password:"));
	gtk_widget_show (new_pass_label);
	gtk_table_attach (GTK_TABLE (table1), new_pass_label, 0, 1, 1, 2,
			  (GtkAttachOptions) (GTK_FILL),
			  (GtkAttachOptions) (0), 0, 0);
	gtk_misc_set_alignment (GTK_MISC (new_pass_label), 0, 0.5);

	confirm_pass_label = gtk_label_new_with_mnemonic (_("_Confirm Password:"));
	gtk_widget_show (confirm_pass_label);
	gtk_table_attach (GTK_TABLE (table1), confirm_pass_label, 0, 1, 2, 3,
			  (GtkAttachOptions) (GTK_FILL),
			  (GtkAttachOptions) (0), 0, 0);
	gtk_misc_set_alignment (GTK_MISC (confirm_pass_label), 0, 0.5);

	new_pass_entry = gtk_entry_new ();
	gtk_widget_show (new_pass_entry);
	gtk_table_attach (GTK_TABLE (table1), new_pass_entry, 1, 2, 1, 2,
			  (GtkAttachOptions) (GTK_EXPAND | GTK_FILL),
			  (GtkAttachOptions) (0), 0, 0);
	gtk_entry_set_visibility (GTK_ENTRY (new_pass_entry), FALSE);

	confirm_pass_entry = gtk_entry_new ();
	gtk_widget_show (confirm_pass_entry);
	gtk_table_attach (GTK_TABLE (table1), confirm_pass_entry, 1, 2, 2, 3,
			  (GtkAttachOptions) (GTK_EXPAND | GTK_FILL),
			  (GtkAttachOptions) (0), 0, 0);
	gtk_entry_set_visibility (GTK_ENTRY (confirm_pass_entry), FALSE);

	current_pass_entry = gtk_entry_new ();
	gtk_widget_show (current_pass_entry);
	gtk_table_attach (GTK_TABLE (table1), current_pass_entry, 1, 2, 0, 1,
			  (GtkAttachOptions) (GTK_EXPAND | GTK_FILL),
			  (GtkAttachOptions) (0), 0, 12);
	gtk_entry_set_visibility (GTK_ENTRY (current_pass_entry), FALSE);

	g_object_set_data (G_OBJECT (new_pass_entry), "pass_dialog", pass_dialog);
	g_object_set_data (G_OBJECT (confirm_pass_entry), "pass_dialog", pass_dialog);
	g_signal_connect (new_pass_entry, "changed", G_CALLBACK (entry_changed), confirm_pass_entry);
	g_signal_connect (confirm_pass_entry, "changed", G_CALLBACK (entry_changed), new_pass_entry);
	entry_changed (GTK_ENTRY (new_pass_entry), confirm_pass_entry);

	if (voluntary)
		gtk_widget_hide (GTK_WIDGET (pass_label));

run_dialog_again:
	response = gtk_dialog_run (GTK_DIALOG (pass_dialog));
	if (response == GTK_RESPONSE_OK) {
		const gchar *cur_pass, *new_pass1, *new_pass2;

		cur_pass = gtk_entry_get_text (GTK_ENTRY (current_pass_entry));
		new_pass1 = gtk_entry_get_text (GTK_ENTRY (new_pass_entry));
		new_pass2 = gtk_entry_get_text (GTK_ENTRY (confirm_pass_entry));

		if (existing_password) {
			if (strcmp (cur_pass, existing_password) != 0) {
				/* User entered a wrong existing
				 * password. Prompt him again.
				 */
				gtk_label_set_text (GTK_LABEL (pass_label), _("The current password does not match the existing password for your account. Please enter the correct password"));
				gtk_widget_show (pass_label);
				goto run_dialog_again;
			}
		}

		if (strcmp (new_pass1, new_pass2) != 0) {
			gtk_label_set_text (GTK_LABEL (pass_label), _("The two passwords do not match. Please re-enter the passwords."));
			gtk_widget_show (pass_label);
			goto run_dialog_again;
		}

		new_pass = g_strdup (new_pass1);
	} else
		new_pass = NULL;

	gtk_widget_destroy (pass_dialog);

	return new_pass;
}
