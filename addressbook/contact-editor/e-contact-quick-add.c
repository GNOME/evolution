/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */

/*
 * e-contact-quick-add.c
 *
 * Copyright (C) 2001 Ximian, Inc.
 *
 * Developed by Jon Trowbridge <trow@ximian.com>
 */

/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307
 * USA.
 */

#include <config.h>
#include <gnome.h>
#include <addressbook/backend/ebook/e-book.h>
#include <addressbook/backend/ebook/e-card.h>
#include "e-contact-editor.h"
#include "e-contact-quick-add.h"

static FILE *out = NULL;

static void
e_card_quick_set_name (ECard *card, const gchar *str)
{
	g_return_if_fail (card && E_IS_CARD (card));

	if (str == NULL)
		return;

	if (out)
		fprintf (out, "quick-set name to \"%s\"\n", str);

	if (card->name)
		e_card_name_free (card->name);
	card->name = e_card_name_from_string (str);
}

static void
e_card_quick_set_email (ECard *card, const gchar *str)
{
	g_return_if_fail (card && E_IS_CARD (card));

	if (str == NULL)
		return;

	if (out)
		fprintf (out, "quick-set email to \"%s\"\n", str);

	if (card->email == NULL) {
		card->email = e_list_new ((EListCopyFunc) g_strdup,
					  (EListFreeFunc) g_free,
					  NULL);
		e_list_append (card->email, str);
	} else {
		EIterator *iter = e_list_get_iterator (card->email);
		e_iterator_reset (iter);
		if (e_iterator_is_valid (iter)) {
			e_iterator_set (iter, str);
		}
	}
}


static void
book_ready_cb (EBook *book, EBookStatus status, gpointer user_data)
{
	if (status == E_BOOK_STATUS_SUCCESS)
		e_book_add_card (book, E_CARD (user_data), NULL, NULL);
	gtk_object_unref (GTK_OBJECT (book));
}

static void
add_card (ECard *card)
{
	EBook *book = e_book_new ();
	gchar *filename, *uri;

	filename = gnome_util_prepend_user_home ("evolution/local/Contacts/addressbook.db");
	uri = g_strdup_printf ("file://%s", filename);

	e_book_load_uri (book, uri, book_ready_cb, card);
	
	g_free (filename);
	g_free (uri);
}

static void
add_card_cb (EContactEditor *ce, ECard *card, gpointer user_data)
{
	add_card (card);
}

static void
editor_closed_cb (GtkWidget *w, gpointer user_data)
{
	if (user_data)
		gtk_object_unref (user_data);
	gtk_object_unref (GTK_OBJECT (w));
}

static void
clicked_cb (GtkWidget *w, gint button, gpointer user_data)
{
	ECard *card = E_CARD (user_data);

	/* Get data out of entries. */
	if (button == 0 || button == 1) {
		gpointer name_entry;
		gpointer email_entry;
		gchar *name = NULL;
		gchar *email = NULL;

		name_entry = gtk_object_get_data (GTK_OBJECT (card), "e-contact-quick-add-name-entry");
		email_entry = gtk_object_get_data (GTK_OBJECT (card), "e-contact-quick-add-email-entry");
		
		if (name_entry)
			name = gtk_editable_get_chars (GTK_EDITABLE (name_entry), 0, -1);
		if (email_entry)
			email = gtk_editable_get_chars (GTK_EDITABLE (email_entry), 0, -1);

		e_card_quick_set_name (card, name);
		e_card_quick_set_email (card, email);
	
		g_free (name);
		g_free (email);
	}

	gtk_widget_destroy (w);

	if (button == 0) { /* OK */

		add_card (card);

	} else if (button == 1) {
		
		/* EDIT FULL */
		EContactEditor *contact_editor;
		contact_editor = e_contact_editor_new (card, TRUE, NULL);

		gtk_signal_connect (GTK_OBJECT (contact_editor),
				    "add_card",
				    GTK_SIGNAL_FUNC (add_card_cb),
				    NULL);
		gtk_signal_connect (GTK_OBJECT (contact_editor),
				    "editor_closed",
				    GTK_SIGNAL_FUNC (editor_closed_cb),
				    user_data);

		e_contact_editor_raise (contact_editor);

	} else {
		/* CANCEL */
		gtk_object_unref (user_data);
	}

}

static GtkWidget *
build_quick_add_dialog (ECard *new_card, EContactQuickAddCallback cb, gpointer user_data)
{
	GtkWidget *dialog;
	GtkTable *table;
	GtkWidget *name_entry;
	GtkWidget *email_entry;
	const gint xpad=1, ypad=1;

	g_return_val_if_fail (new_card && E_IS_CARD (new_card), NULL);

	dialog = gnome_dialog_new (_("Contact Quick-Add"),
				   GNOME_STOCK_BUTTON_OK,
				   _("Edit Full"),
				   GNOME_STOCK_BUTTON_CANCEL,
				   NULL);

	gtk_signal_connect (GTK_OBJECT (dialog),
			    "clicked",
			    clicked_cb,
			    new_card);

	name_entry = gtk_entry_new ();

	if (new_card->name) {
		gchar *str = e_card_name_to_string (new_card->name);
		gtk_entry_set_text (GTK_ENTRY (name_entry), str);
		g_free (str);
	}


	email_entry = gtk_entry_new ();

	if (new_card->email && e_list_length (new_card->email)) {
		EIterator *iterator = e_list_get_iterator (new_card->email);
		if (iterator) {
			e_iterator_reset (iterator);
			if (e_iterator_is_valid (iterator)) {
				const gchar *str = e_iterator_get (iterator);
				gtk_entry_set_text (GTK_ENTRY (email_entry), str);
			}
		}
	}

	gtk_object_set_data (GTK_OBJECT (new_card), "e-contact-quick-add-name-entry", name_entry);
	gtk_object_set_data (GTK_OBJECT (new_card), "e-contact-quick-add-email-entry", email_entry);


	table = GTK_TABLE (gtk_table_new (2, 2, FALSE));

	gtk_table_attach (table, gtk_label_new (_("Full Name")),
			  0, 1, 0, 1,
			  0, 0, xpad, ypad);
	gtk_table_attach (table, name_entry,
			  1, 2, 0, 1,
			  GTK_EXPAND | GTK_FILL, GTK_EXPAND, xpad, ypad);
	gtk_table_attach (table, gtk_label_new (_("E-mail")),
			  0, 1, 1, 2,
			  0, 0, xpad, ypad);
	gtk_table_attach (table, email_entry,
			  1, 2, 1, 2,
			  GTK_EXPAND | GTK_FILL, GTK_EXPAND, xpad, ypad);

	gtk_box_pack_start (GTK_BOX (GNOME_DIALOG (dialog)->vbox),
			    GTK_WIDGET (table),
			    TRUE, TRUE, 0);
	gtk_widget_show_all (GTK_WIDGET (table));
			  
	
	return dialog;
}

void
e_contact_quick_add (const gchar *name, const gchar *email,
		     EContactQuickAddCallback cb, gpointer user_data)
{
	ECard *new_card;
	GtkWidget *dialog;

	if (out == NULL) {
		out = fopen ("/tmp/barnass", "w");
		if (out)
			setvbuf (out, NULL, _IONBF, 0);
	}

	if (out)
		fprintf (out, "\n name: %s\nemail: %s\n", name, email);
		

	/* We need to have *something* to work with. */
	if (name == NULL && email == NULL) {
		if (cb)
			cb (NULL, user_data);
		return;
	}

	new_card = e_card_new ("");
	if (cb)
		gtk_object_set_data (GTK_OBJECT (new_card), "e-contact-quick-add-cb", cb);
	if (user_data)
		gtk_object_set_data (GTK_OBJECT (new_card), "e-contact-quick-add-user-data", user_data);

	
	e_card_quick_set_name (new_card, name);
	e_card_quick_set_email (new_card, email);

	dialog = build_quick_add_dialog (new_card, cb, user_data);
	
	gtk_widget_show_all (dialog);
}
