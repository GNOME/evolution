/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */

/* Copyright (C) 2001-2004 Novell, Inc.
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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "exchange-user-dialog.h"
#include "e2k-types.h"

#include <bonobo-activation/bonobo-activation.h>
#include <bonobo/bonobo-exception.h>
#include <bonobo/bonobo-widget.h>
#include <e-util/e-gtk-utils.h>
#include <e-util/e-dialog-utils.h>
#include <gtk/gtkbutton.h>
#include <gtk/gtkhbox.h>
#include <gtk/gtklabel.h>
#include <gtk/gtkstock.h>
#include <gtk/gtkvbox.h>
#include <libedataserverui/e-name-selector.h>
#include "e2k-xml-utils.h"

struct _E2kUserDialogPrivate {
	char *section_name;
	ENameSelector *name_selector;
	GtkWidget *entry, *parent_window;
};

#define PARENT_TYPE GTK_TYPE_DIALOG
static GtkDialogClass *parent_class;

static void parent_window_destroyed (gpointer dialog, GObject *where_parent_window_was);

static void
finalize (GObject *object)
{
	E2kUserDialog *dialog = E2K_USER_DIALOG (object);

	g_free (dialog->priv->section_name);
	g_free (dialog->priv);

	G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
dispose (GObject *object)
{
	E2kUserDialog *dialog = E2K_USER_DIALOG (object);

	if (dialog->priv->name_selector != NULL) {
		g_object_unref (dialog->priv->name_selector);
		dialog->priv->name_selector = NULL;
	}

	if (dialog->priv->parent_window) {
		g_object_weak_unref (G_OBJECT (dialog->priv->parent_window),
				     parent_window_destroyed, dialog);
		dialog->priv->parent_window = NULL;
	}

	G_OBJECT_CLASS (parent_class)->dispose (object);
}

static void
class_init (E2kUserDialogClass *class)
{
	GObjectClass *object_class = (GObjectClass *) class;

	parent_class = g_type_class_ref (GTK_TYPE_DIALOG);

	object_class->dispose = dispose;
	object_class->finalize = finalize;
}

static void
init (E2kUserDialog *dialog)
{
	dialog->priv = g_new0 (E2kUserDialogPrivate, 1);

	gtk_box_set_spacing (GTK_BOX (GTK_DIALOG (dialog)->vbox), 6);
}

E2K_MAKE_TYPE (e2k_user_dialog, E2kUserDialog, class_init, init, PARENT_TYPE)



static void
parent_window_destroyed (gpointer user_data, GObject *where_parent_window_was)
{
	E2kUserDialog *dialog = user_data;

	dialog->priv->parent_window = NULL;
	gtk_dialog_response (GTK_DIALOG (dialog), GTK_RESPONSE_CANCEL);
}

static void
addressbook_dialog_response (ENameSelectorDialog *name_selector_dialog, gint response, gpointer data)
{
	gtk_widget_hide (GTK_WIDGET (name_selector_dialog));
}

static void
addressbook_clicked_cb (GtkWidget *widget, gpointer data)
{
	E2kUserDialog *dialog = data;
	E2kUserDialogPrivate *priv;
	ENameSelectorDialog *name_selector_dialog;

	priv = dialog->priv;

	name_selector_dialog = e_name_selector_peek_dialog (priv->name_selector);
	gtk_window_set_modal (GTK_WINDOW (dialog), FALSE);
	gtk_widget_show (GTK_WIDGET (name_selector_dialog));
}

static gboolean
e2k_user_dialog_construct (E2kUserDialog *dialog,
			   GtkWidget *parent_window,
			   const char *label_text,
			   const char *section_name)
{
	E2kUserDialogPrivate *priv;
	GtkWidget *hbox, *vbox, *label, *button;
	ENameSelectorModel *name_selector_model;
	ENameSelectorDialog *name_selector_dialog;

	gtk_window_set_title (GTK_WINDOW (dialog), _("Select User"));
	gtk_dialog_add_buttons (GTK_DIALOG (dialog),
				GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
				GTK_STOCK_OK, GTK_RESPONSE_OK,
				NULL);

	e_dialog_set_transient_for (GTK_WINDOW (dialog), parent_window);

	priv = dialog->priv;
	priv->section_name = g_strdup (section_name);

	priv->parent_window = parent_window;
	g_object_weak_ref (G_OBJECT (parent_window),
			   parent_window_destroyed, dialog);

	/* Set up the actual select names bits */
	priv->name_selector = e_name_selector_new ();

	/* Listen for responses whenever the dialog is shown */
	name_selector_dialog = e_name_selector_peek_dialog (priv->name_selector);
	g_signal_connect (name_selector_dialog, "response",
			  G_CALLBACK (addressbook_dialog_response), dialog);

	name_selector_model = e_name_selector_peek_model (priv->name_selector);
	/* FIXME Limit to one user */
	e_name_selector_model_add_section (name_selector_model, section_name, section_name, NULL);

	hbox = gtk_hbox_new (FALSE, 6);

	label = gtk_label_new (label_text);
	gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, 6);

	/* The vbox is a workaround for bug 43315 */
	vbox = gtk_vbox_new (FALSE, 0);
	priv->entry = GTK_WIDGET (e_name_selector_peek_section_entry (priv->name_selector, section_name));
	gtk_box_pack_start (GTK_BOX (vbox), priv->entry, TRUE, FALSE, 0);
	gtk_box_pack_start (GTK_BOX (hbox), vbox, TRUE, TRUE, 6);

	button = gtk_button_new_with_label (_("Addressbook..."));
	g_signal_connect (button, "clicked",
			  G_CALLBACK (addressbook_clicked_cb),
			  dialog);
	gtk_box_pack_start (GTK_BOX (hbox), button, FALSE, FALSE, 6);

	gtk_box_pack_start (GTK_BOX (GTK_DIALOG (dialog)->vbox), hbox,
			    TRUE, TRUE, 6);
	gtk_widget_show_all (hbox);

	return TRUE;
}

/**
 * e2k_user_dialog_new:
 * @parent_window: The window invoking the dialog.
 * @label_text: Text to label the entry in the initial dialog with
 * @section_name: The section name for the select-names dialog
 *
 * Creates a new user selection dialog.
 *
 * Return value: A newly-created user selection dialog, or %NULL if
 * the dialog could not be created.
 **/
GtkWidget *
e2k_user_dialog_new (GtkWidget *parent_window,
		     const char *label_text, const char *section_name)
{
	E2kUserDialog *dialog;

	g_return_val_if_fail (GTK_IS_WINDOW (parent_window), NULL);
	g_return_val_if_fail (label_text != NULL, NULL);
	g_return_val_if_fail (section_name != NULL, NULL);

	dialog = g_object_new (E2K_TYPE_USER_DIALOG, NULL);
	if (!e2k_user_dialog_construct (dialog, parent_window,
					label_text, section_name)) {
		gtk_widget_destroy (GTK_WIDGET (dialog));
		return NULL;
	}

	return GTK_WIDGET (dialog);
}

/**
 * e2k_user_dialog_get_user_list:
 * @dialog: the dialog
 *
 * Gets the email addresses of the selected user from the dialog.
 *
 * Return value: the email addresses.
 **/
GList *
e2k_user_dialog_get_user_list (E2kUserDialog *dialog)
{
	E2kUserDialogPrivate *priv;
	EDestinationStore *destination_store;
	GList *destinations;
	GList *l;
	GList *email_list = NULL;
	EDestination *destination;

	g_return_val_if_fail (E2K_IS_USER_DIALOG (dialog), NULL);

	priv = dialog->priv;

	destination_store = e_name_selector_entry_peek_destination_store (E_NAME_SELECTOR_ENTRY (priv->entry));
	destinations = e_destination_store_list_destinations (destination_store);
	if (!destinations)
		return NULL;

	for (l = destinations; l; l = g_list_next (l)) {
		destination = l->data;
		email_list = g_list_prepend (email_list, g_strdup (e_destination_get_email (destination)));
	}
	g_list_free (destinations);

	return email_list;
}

/**
 * e2k_user_dialog_get_user:
 * @dialog: the dialog
 *
 * Gets the email address of the selected user from the dialog.
 *
 * Return value: the email address, which must be freed with g_free().
 **/
char *
e2k_user_dialog_get_user (E2kUserDialog *dialog)
{
	E2kUserDialogPrivate *priv;
	EDestinationStore *destination_store;
	GList *destinations;
	EDestination *destination;
	gchar *result = NULL;

	g_return_val_if_fail (E2K_IS_USER_DIALOG (dialog), NULL);

	priv = dialog->priv;

	destination_store = e_name_selector_entry_peek_destination_store (E_NAME_SELECTOR_ENTRY (priv->entry));
	destinations = e_destination_store_list_destinations (destination_store);
	if (!destinations)
		return NULL;

	destination = destinations->data;
	result = g_strdup (e_destination_get_email (destination));
	g_list_free (destinations);

	return result;
}
