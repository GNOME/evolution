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
#include <ctype.h>
#include <gnome.h>
#include <addressbook/backend/ebook/e-book.h>
#include <addressbook/backend/ebook/e-card.h>
#include "e-contact-editor.h"
#include "e-contact-quick-add.h"

static void
e_card_quick_set_name (ECard *card, const gchar *str)
{
	ECardSimple *simple;

	g_return_if_fail (card && E_IS_CARD (card));

	if (str == NULL)
		return;

	simple = e_card_simple_new (card);
	e_card_simple_set (simple, E_CARD_SIMPLE_FIELD_FULL_NAME, str);
	e_card_simple_sync_card (simple);
	gtk_object_unref (GTK_OBJECT (simple));
}

static void
e_card_quick_set_email (ECard *card, const gchar *str)
{
	ECardSimple *simple;

	g_return_if_fail (card && E_IS_CARD (card));

	if (str == NULL)
		return;

	simple = e_card_simple_new (card);
	e_card_simple_set (simple, E_CARD_SIMPLE_FIELD_EMAIL, str);
	e_card_simple_sync_card (simple);
	gtk_object_unref (GTK_OBJECT (simple));
}


static void
book_ready_cb (EBook *book, EBookStatus status, gpointer user_data)
{
	ECard *card = E_CARD (user_data);

	EContactQuickAddCallback cb = gtk_object_get_data (GTK_OBJECT (card), "e-contact-quick-add-cb");
	gpointer cb_user_data = gtk_object_get_data (GTK_OBJECT (card), "e-contact-quick-add-user-data");

	if (status == E_BOOK_STATUS_SUCCESS) {
		e_book_add_card (book, card, NULL, NULL);
		if (cb)
			cb (card, cb_user_data);
	} else {
		/* Something went wrong... */
		if (cb)
			cb (NULL, cb_user_data);

		gtk_object_unref (GTK_OBJECT (book));
	}
}

static void
add_card (ECard *card)
{
	EBook *book = e_book_new ();
	e_book_load_local_address_book (book, book_ready_cb, card);
}

/*
 * Raise a contact editor with all fields editable, and hook up all signals accordingly.
 */

static void
add_card_cb (EContactEditor *ce, ECard *card, gpointer user_data)
{
	add_card (card);
}

static void
editor_closed_cb (GtkWidget *w, gpointer user_data)
{
	/* w is the contact editor, user_data is an ECard. */
	if (user_data)
		gtk_object_unref (user_data);
	gtk_object_unref (GTK_OBJECT (w));
}

static void
ce_book_found_fields (EBook *book, EBookStatus status, EList *fields, gpointer user_data)
{
	ECard *card = E_CARD (user_data);
	EContactEditor *contact_editor;

	if (status != E_BOOK_STATUS_SUCCESS) {
		g_warning ("Couldn't find supported fields for local address book.");
		return;
	}

	contact_editor = e_contact_editor_new (card, TRUE, fields, FALSE /* XXX */);

	gtk_signal_connect (GTK_OBJECT (contact_editor),
			    "add_card",
			    GTK_SIGNAL_FUNC (add_card_cb),
			    NULL);
	gtk_signal_connect (GTK_OBJECT (contact_editor),
			    "editor_closed",
			    GTK_SIGNAL_FUNC (editor_closed_cb),
			    user_data);

	e_contact_editor_raise (contact_editor);
}

static void
ce_book_ready (EBook *book, EBookStatus status, gpointer user_data)
{
	if (status != E_BOOK_STATUS_SUCCESS) {
		g_warning ("Couldn't open local address book.");
		return;
	}

	e_book_get_supported_fields (book, ce_book_found_fields, user_data);
}

static void
edit_card (ECard *card)
{
	e_book_load_local_address_book (e_book_new (), ce_book_ready, card);
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
		edit_card (card);

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

void
e_contact_quick_add_free_form (const gchar *text, EContactQuickAddCallback cb, gpointer user_data)
{
	gchar *name=NULL, *email=NULL;
	const gchar *last_at, *s;
	gboolean in_quote;

	if (text == NULL) {
		e_contact_quick_add (NULL, NULL, cb, user_data);
		return;
	}

	/* Look for things that look like e-mail addresses embedded in text */
	in_quote = FALSE;
	last_at = NULL;
	for (s = text; *s; ++s) {
		if (*s == '@' && !in_quote)
			last_at = s;
		else if (*s == '"')
			in_quote = !in_quote;
	}

	
	if (last_at == NULL) {
		/* No at sign, so we treat it all as the name */
		name = g_strdup (text);
	} else {
		gboolean bad_char = FALSE;
		
		/* walk backwards to whitespace or a < or a quote... */
		while (last_at >= text && !bad_char
		       && !(isspace ((gint) *last_at) || *last_at == '<' || *last_at == '"')) {
			/* Check for some stuff that can't appear in a legal e-mail address. */
			if (*last_at == '['
			    || *last_at == ']'
			    || *last_at == '('
			    || *last_at == ')')
				bad_char = TRUE;
			--last_at;
		}
		if (last_at < text)
			last_at = text;

		/* ...and then split the text there */
		if (!bad_char) {
			if (text < last_at)
				name = g_strndup (text, last_at-text);
			email = g_strdup (last_at);
		}
	}

	/* If all else has failed, make it the name. */
	if (name == NULL && email == NULL) 
		name = g_strdup (text);
		

	/* Clean up name */
	if (name && *name)
		g_strstrip (name);

	/* Clean up email, remove bracketing <>s */
	if (email && *email) {
		gboolean changed = FALSE;
		g_strstrip (email);
		if (*email == '<') {
			*email = ' ';
			changed = TRUE;
		}
		if (email[strlen (email)-1] == '>') {
			email[strlen (email)-1] = ' ';
			changed = TRUE;
		}
		if (changed)
			g_strstrip (email);
	}
	

	e_contact_quick_add (name, email, cb, user_data);
	g_free (name);
	g_free (email);
}
