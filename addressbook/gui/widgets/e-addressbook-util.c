/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* 
 * e-table-field-chooser.c
 * Copyright (C) 2001  Ximian, Inc.
 * Author: Chris Toshok <toshok@ximian.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of version 2 of the GNU General Public
 * License as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#include <config.h>

#include <gnome.h>

#include "e-addressbook-util.h"
#include "e-card-merging.h"

void
e_addressbook_error_dialog (const gchar *msg, EBookStatus status)
{
	static char *status_to_string[] = {
		N_("Success"),
		N_("Unknown error"),
		N_("Repository offline"),
		N_("Permission denied"),
		N_("Card not found"),
		N_("Card ID already exists"),
		N_("Protocol not supported"),
		N_("Cancelled"),
		N_("Authentication Failed"),
		N_("Authentication Required"),
		N_("Other error")
	};
	char *error_msg;

	error_msg = g_strdup_printf ("%s: %s", msg, _(status_to_string [status]));

	gtk_widget_show (gnome_error_dialog (error_msg));

	g_free (error_msg);
}

gint
e_addressbook_prompt_save_dialog (GtkWindow *parent)
{
	GtkWidget *dialog;

	dialog = gnome_message_box_new (_("Do you want to save changes?"),
					GNOME_MESSAGE_BOX_QUESTION,
					GNOME_STOCK_BUTTON_YES,
					GNOME_STOCK_BUTTON_NO,
					GNOME_STOCK_BUTTON_CANCEL,
					NULL);

	gnome_dialog_set_default (GNOME_DIALOG (dialog), 0);
	gnome_dialog_grab_focus (GNOME_DIALOG (dialog), 0);
	gnome_dialog_set_parent (GNOME_DIALOG (dialog), parent);

	return gnome_dialog_run_and_close (GNOME_DIALOG (dialog));
}

static void
added_cb (EBook* book, EBookStatus status, const char *id,
	  gboolean is_list)
{
	if (status != E_BOOK_STATUS_SUCCESS) {
		e_addressbook_error_dialog (is_list ? _("Error adding list") : _("Error adding card"), status);
	}
}

static void
modified_cb (EBook* book, EBookStatus status,
	     gboolean is_list)
{
	if (status != E_BOOK_STATUS_SUCCESS) {
		e_addressbook_error_dialog (is_list ? _("Error modifying list") : _("Error modifying card"),
					    status);
	}
}

static void
deleted_cb (EBook* book, EBookStatus status,
	    gboolean is_list)
{
	if (status != E_BOOK_STATUS_SUCCESS) {
		e_addressbook_error_dialog (is_list ? _("Error removing list") : _("Error removing card"),
					    status);
	}
}

static void
editor_closed_cb (GtkObject *editor, gpointer data)
{
	gtk_object_unref (editor);
}

EContactEditor *
e_addressbook_show_contact_editor (EBook *book, ECard *card,
				   gboolean is_new_card,
				   gboolean editable)
{
	EContactEditor *ce;

	ce = e_contact_editor_new (book, card, is_new_card, editable);

	gtk_signal_connect (GTK_OBJECT (ce), "card_added",
			    GTK_SIGNAL_FUNC (added_cb), GINT_TO_POINTER (FALSE));
	gtk_signal_connect (GTK_OBJECT (ce), "card_modified",
			    GTK_SIGNAL_FUNC (modified_cb), GINT_TO_POINTER (FALSE));
	gtk_signal_connect (GTK_OBJECT (ce), "card_deleted",
			    GTK_SIGNAL_FUNC (deleted_cb), GINT_TO_POINTER (FALSE));
	gtk_signal_connect (GTK_OBJECT (ce), "editor_closed",
			    GTK_SIGNAL_FUNC (editor_closed_cb), NULL);

	return ce;
}

EContactListEditor *
e_addressbook_show_contact_list_editor (EBook *book, ECard *card,
					gboolean is_new_card,
					gboolean editable)
{
	EContactListEditor *ce;

	ce = e_contact_list_editor_new (book, card, is_new_card, editable);

	gtk_signal_connect (GTK_OBJECT (ce), "list_added",
			    GTK_SIGNAL_FUNC (added_cb), GINT_TO_POINTER (TRUE));
	gtk_signal_connect (GTK_OBJECT (ce), "list_modified",
			    GTK_SIGNAL_FUNC (modified_cb), GINT_TO_POINTER (TRUE));
	gtk_signal_connect (GTK_OBJECT (ce), "list_deleted",
			    GTK_SIGNAL_FUNC (deleted_cb), GINT_TO_POINTER (TRUE));
	gtk_signal_connect (GTK_OBJECT (ce), "editor_closed",
			    GTK_SIGNAL_FUNC (editor_closed_cb), GINT_TO_POINTER (TRUE));

	e_contact_list_editor_show (ce);

	return ce;
}
