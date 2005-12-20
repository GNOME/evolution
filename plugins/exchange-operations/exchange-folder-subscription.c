/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 *  Shakti Sen <shprasad@novell.com>
 *  Copyright (C) 2005 Novell, Inc.
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
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif


#include <glade/glade-xml.h>
#include <gtk/gtk.h>
#include <e-util/e-error.h>
#include <e-folder.h>
#include <exchange-account.h>
#include <exchange-hierarchy.h>
#include "exchange-hierarchy-foreign.h"
#include <e2k-types.h>
#include <exchange-types.h>
#include <e2k-propnames.h>
#include <libedataserver/e-xml-hash-utils.h>
#include <libedataserverui/e-name-selector.h>
#include "exchange-config-listener.h"
#include "exchange-folder-subscription.h"
#include "exchange-operations.h"

static void
user_response (ENameSelectorDialog *name_selector_dialog, gint response, gpointer data)
{
	gtk_widget_hide (GTK_WIDGET (name_selector_dialog));
}

static void
user_clicked (GtkWidget *button, ENameSelector *name_selector)
{
	ENameSelectorDialog *name_selector_dialog;

	name_selector_dialog = e_name_selector_peek_dialog (name_selector);
	gtk_window_set_modal (GTK_WINDOW (name_selector_dialog), TRUE);
	gtk_widget_show (GTK_WIDGET (name_selector_dialog));
}


static GtkWidget *
setup_name_selector (GladeXML *glade_xml, ENameSelector **name_selector_ret)
{
	ENameSelector *name_selector;
	ENameSelectorModel *name_selector_model;
	ENameSelectorDialog *name_selector_dialog;
	GtkWidget *placeholder;
	GtkWidget *widget;
	GtkWidget *button;

	placeholder = glade_xml_get_widget (glade_xml, "user-picker-placeholder");
	g_assert (GTK_IS_CONTAINER (placeholder));

	name_selector = e_name_selector_new ();

	name_selector_model = e_name_selector_peek_model (name_selector);
	/* FIXME Limit to one user */
	e_name_selector_model_add_section (name_selector_model, "User", "User", NULL);

	/* Listen for responses whenever the dialog is shown */
	name_selector_dialog = e_name_selector_peek_dialog (name_selector);
	g_signal_connect (name_selector_dialog, "response",
			  G_CALLBACK (user_response), name_selector);

	widget = GTK_WIDGET (e_name_selector_peek_section_entry (name_selector, "User"));
	gtk_widget_show (widget);

	button = glade_xml_get_widget (glade_xml, "button-user");
	g_signal_connect (button, "clicked", G_CALLBACK (user_clicked), name_selector);
	gtk_box_pack_start (GTK_BOX (placeholder), widget, TRUE, TRUE, 6);
	*name_selector_ret = name_selector;

	return widget;
}

static void
setup_folder_name_combo (GladeXML *glade_xml, gchar *fname)
{
	GtkWidget *combo;
	GList *string_list;
	char *strings[] = {
		"Calendar",
		"Inbox",
		"Contacts",
		"Tasks",
		NULL
		/* FIXME: Should these be translated?  */
	};
	int i;

	combo = glade_xml_get_widget (glade_xml, "folder-name-combo");
	g_assert (GTK_IS_COMBO (combo));

	string_list = NULL;
	for (i = 0; strings[i] != NULL; i ++)
		string_list = g_list_append (string_list, strings[i]);
	gtk_combo_set_popdown_strings (GTK_COMBO (combo), string_list);
	g_list_free (string_list);

	gtk_entry_set_text (GTK_ENTRY (GTK_COMBO (combo)->entry), fname);
}

static void
folder_name_entry_changed_callback (GtkEditable *editable,
                                    void *data)
{
	GtkDialog *dialog = GTK_DIALOG (data);
	const char *folder_name_text = gtk_entry_get_text (GTK_ENTRY (editable));

	if (*folder_name_text == '\0')
		gtk_dialog_set_response_sensitive (dialog, GTK_RESPONSE_OK, FALSE);
	else
		gtk_dialog_set_response_sensitive (dialog, GTK_RESPONSE_OK, TRUE);
}

static void
setup_server_option_menu (GladeXML *glade_xml, gchar *mail_account)
{
	GtkWidget *widget;
	GtkWidget *menu;
	GtkWidget *menu_item;

	widget = glade_xml_get_widget (glade_xml, "server-option-menu");
	g_return_if_fail (GTK_IS_OPTION_MENU (widget));

	menu = gtk_menu_new ();
	gtk_widget_show (menu);

	menu_item = gtk_menu_item_new_with_label (mail_account);

	gtk_widget_show (menu_item);
	gtk_menu_shell_append(GTK_MENU_SHELL(menu), menu_item);


	gtk_option_menu_set_menu (GTK_OPTION_MENU (widget), menu);

	/* FIXME: Default to the current storage in the shell view.  */
}


gboolean
create_folder_subscription_dialog (gchar *mail_account, gchar *fname, gchar **user_email_address_ret, gchar **folder_name_ret)
{
	ENameSelector *name_selector;
	GladeXML *glade_xml;
	GtkWidget *dialog;
	GtkWidget *name_selector_widget;
	GtkWidget *folder_name_entry;
	char *user_email_address = NULL;
	int response;
	EDestinationStore *destination_store;
	GList *destinations;
	EDestination *destination;


	glade_xml = glade_xml_new (CONNECTOR_GLADEDIR "/e-foreign-folder-dialog.glade",
				   NULL, NULL);
	g_return_val_if_fail (glade_xml != NULL, FALSE);

	dialog = glade_xml_get_widget (glade_xml, "dialog");
	g_return_val_if_fail (dialog != NULL, FALSE);
	gtk_window_set_modal (GTK_WINDOW (dialog), FALSE);
	gtk_window_set_title (GTK_WINDOW (dialog), _("Subscribe to Other User's Folder"));

	name_selector_widget = setup_name_selector (glade_xml, &name_selector);
	gtk_widget_grab_focus (name_selector_widget);
	setup_server_option_menu (glade_xml, mail_account);
	setup_folder_name_combo (glade_xml, fname);
	folder_name_entry = glade_xml_get_widget (glade_xml, "folder-name-entry");

	/* Connect the callback to set the OK button insensitive when there is
	   no text in the folder_name_entry.  Notice that we put a value there
	   by default so the OK button is sensitive by default.  */
	g_signal_connect (folder_name_entry, "changed",
			  G_CALLBACK (folder_name_entry_changed_callback), dialog);

	while (TRUE) {
		response = gtk_dialog_run (GTK_DIALOG (dialog));
		if (response == GTK_RESPONSE_CANCEL) {
			gtk_widget_destroy (dialog);
			g_object_unref (name_selector);
			return FALSE;
		}
		destination_store = e_name_selector_entry_peek_destination_store (E_NAME_SELECTOR_ENTRY (GTK_ENTRY (name_selector_widget)));
		destinations = e_destination_store_list_destinations (destination_store);
		if (!destinations) {
			gtk_widget_destroy (dialog);
			g_object_unref (name_selector);
			return FALSE;
		}
		destination = destinations->data;
		user_email_address = g_strdup (e_destination_get_email (destination));
		g_list_free (destinations);

		if (user_email_address != NULL && *user_email_address != '\0')
			break;

		/* It would be nice to insensitivize the OK button appropriately                   instead of doing this, but unfortunately we can't do this for the
		   Bonobo control.  */
		e_error_run (GTK_WINDOW (dialog), ERROR_DOMAIN ":select-user", NULL);


	}
	gtk_widget_show_all (dialog);

	if (user_email_address)
		*user_email_address_ret = user_email_address;
	*folder_name_ret = g_strdup (gtk_entry_get_text (GTK_ENTRY (folder_name_entry)));

	gtk_widget_destroy (dialog);
	g_object_unref (name_selector);
	return TRUE;

}

