/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* 
 * e-table-field-chooser.c
 * Copyright (C) 2001  Ximian, Inc.
 * Author: Chris Toshok <toshok@ximian.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
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
#include "e-contact-editor.h"
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
		N_("Canceled"),
		N_("Other error")
	};
	char *error_msg;

	error_msg = g_strdup_printf ("%s: %s", msg, status_to_string [status]);

	gtk_widget_show (gnome_error_dialog (error_msg));

	g_free (error_msg);
}


static void
card_added_cb (EBook* book, EBookStatus status, const char *id,
	       gpointer user_data)
{
	g_print ("%s: %s(): a card was added\n", __FILE__, __FUNCTION__);
	if (status != E_BOOK_STATUS_SUCCESS) {
		e_addressbook_error_dialog (_("Error adding card"), status);
	}
}

static void
card_modified_cb (EBook* book, EBookStatus status,
		  gpointer user_data)
{
	g_print ("%s: %s(): a card was modified\n", __FILE__, __FUNCTION__);
	if (status != E_BOOK_STATUS_SUCCESS) {
		e_addressbook_error_dialog (_("Error modifying card"), status);
	}
}

static void
card_deleted_cb (EBook* book, EBookStatus status,
		 gpointer user_data)
{
	g_print ("%s: %s(): a card was removed\n", __FILE__, __FUNCTION__);
	if (status != E_BOOK_STATUS_SUCCESS) {
		e_addressbook_error_dialog (_("Error removing card"), status);
	}
}

static void
editor_closed_cb (EContactEditor *ce, gpointer data)
{
	gtk_object_unref (GTK_OBJECT (ce));
}

EContactEditor *
e_addressbook_show_contact_editor (EBook *book, ECard *card,
				   gboolean is_new_card,
				   gboolean editable)
{
	EContactEditor *ce;

	ce = e_contact_editor_new (book, card, is_new_card, editable);

	gtk_signal_connect (GTK_OBJECT (ce), "card_added",
			    GTK_SIGNAL_FUNC (card_added_cb), NULL);
	gtk_signal_connect (GTK_OBJECT (ce), "card_modified",
			    GTK_SIGNAL_FUNC (card_modified_cb), NULL);
	gtk_signal_connect (GTK_OBJECT (ce), "card_deleted",
			    GTK_SIGNAL_FUNC (card_deleted_cb), NULL);
	gtk_signal_connect (GTK_OBJECT (ce), "editor_closed",
			    GTK_SIGNAL_FUNC (editor_closed_cb), NULL);

	return ce;
}
