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

void
e_addressbook_error_dialog (const gchar *msg, EBookStatus status)
{
	static char *status_to_string[] = {
		N_("Success"),
		N_("Unknown error"),
		N_("Repository offline"),
		N_("Permission denied"),
		N_("Card not found"),
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
	if (status != E_BOOK_STATUS_SUCCESS)
		e_addressbook_error_dialog (_("Error adding card"), status);
}

static void
card_modified_cb (EBook* book, EBookStatus status,
		  gpointer user_data)
{
	g_print ("%s: %s(): a card was modified\n", __FILE__, __FUNCTION__);
	if (status != E_BOOK_STATUS_SUCCESS)
		e_addressbook_error_dialog (_("Error modifying card"), status);
}

static void
card_removed_cb (EBook* book, EBookStatus status,
		 gpointer user_data)
{
	g_print ("%s: %s(): a card was removed\n", __FILE__, __FUNCTION__);
	if (status != E_BOOK_STATUS_SUCCESS)
		e_addressbook_error_dialog (_("Error removing card"), status);
}

/* Callback for the add_card signal from the contact editor */
static void
add_card_cb (EContactEditor *ce, ECard *card, gpointer data)
{
	EBook *book;

	book = E_BOOK (data);
	e_book_add_card (book, card, card_added_cb, NULL);
}

/* Callback for the commit_card signal from the contact editor */
static void
commit_card_cb (EContactEditor *ce, ECard *card, gpointer data)
{
	EBook *book;

	book = E_BOOK (data);
	e_book_commit_card (book, card, card_modified_cb, NULL);
}

/* Callback for the delete_card signal from the contact editor */
static void
delete_card_cb (EContactEditor *ce, ECard *card, gpointer data)
{
	EBook *book;

	book = E_BOOK (data);
	e_book_remove_card (book, card, card_removed_cb, NULL);
}

/* Callback used when the contact editor is closed */
static void
editor_closed_cb (EContactEditor *ce, gpointer data)
{
	gtk_object_unref (GTK_OBJECT (ce));
}

typedef struct {
  ECard *card;
  gboolean editable;
} SupportedFieldsClosure;

static void
supported_fields_cb (EBook *book, EBookStatus status,
		     EList *fields, EContactEditor *ce)
{
	gtk_object_set (GTK_OBJECT (ce),
			"writable_fields", fields,
			NULL);

	gtk_signal_connect (GTK_OBJECT (ce), "add_card",
			    GTK_SIGNAL_FUNC (add_card_cb), book);
	gtk_signal_connect (GTK_OBJECT (ce), "commit_card",
			    GTK_SIGNAL_FUNC (commit_card_cb), book);
	gtk_signal_connect (GTK_OBJECT (ce), "delete_card",
			    GTK_SIGNAL_FUNC (delete_card_cb), book);
	gtk_signal_connect (GTK_OBJECT (ce), "editor_closed",
			    GTK_SIGNAL_FUNC (editor_closed_cb), NULL);

	e_contact_editor_show (ce);
}

EContactEditor *
e_addressbook_show_contact_editor (EBook *book, ECard *card,
				   gboolean editable)
{
	EContactEditor *ce;
	gboolean new_card = FALSE;

	if (card == NULL) {
		new_card = TRUE;
		card = e_card_new ("");
	}

	ce = e_contact_editor_new (card, new_card, NULL,
				   !editable);

	e_book_get_supported_fields (book, (EBookFieldsCallback)supported_fields_cb, ce);

	return ce;
}
