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
#include <glib.h>
#include <gtk/gtkentry.h>
#include <gtk/gtklabel.h>
#include <gtk/gtktable.h>
#include <libgnome/gnome-defs.h>
#include <libgnome/gnome-i18n.h>
#include <libgnomeui/gnome-app.h>
#include <libgnomeui/gnome-dialog.h>
#include <libgnomeui/gnome-stock.h>
#include <gal/widgets/e-unicode.h>
#include <addressbook/backend/ebook/e-book.h>
#include <addressbook/backend/ebook/e-book-util.h>
#include <addressbook/backend/ebook/e-card.h>
#include "e-contact-editor.h"
#include "e-contact-quick-add.h"
#include "e-card-merging.h"

typedef struct _QuickAdd QuickAdd;
struct _QuickAdd {
	gchar *name;
	gchar *email;
	ECard *card;

	EContactQuickAddCallback cb;
	gpointer closure;

	GtkWidget *name_entry;
	GtkWidget *email_entry;

	gint refs;

};

static QuickAdd *
quick_add_new (void)
{
	QuickAdd *qa = g_new0 (QuickAdd, 1);
	qa->card = e_card_new ("");
	qa->refs = 1;
	return qa;
}

static void
quick_add_ref (QuickAdd *qa)
{
	if (qa) {
		++qa->refs;
	}
}

static void
quick_add_unref (QuickAdd *qa)
{
	if (qa) {
		--qa->refs;
		if (qa->refs == 0) {
			g_message ("freeing %s / %s", qa->name, qa->email);
			g_free (qa->name);
			g_free (qa->email);
			gtk_object_unref (GTK_OBJECT (qa->card));
			g_free (qa);
		}
	}
}

static void
quick_add_set_name (QuickAdd *qa, const gchar *name)
{
	ECardSimple *simple;

	g_free (qa->name);
	qa->name = g_strdup (name);

	simple = e_card_simple_new (qa->card);
	e_card_simple_set (simple, E_CARD_SIMPLE_FIELD_FULL_NAME, name);
	e_card_simple_sync_card (simple);
	gtk_object_unref (GTK_OBJECT (simple));
}

static void
quick_add_set_email (QuickAdd *qa, const gchar *email)
{
	ECardSimple *simple;

	g_free (qa->email);
	qa->email = g_strdup (email);

	simple = e_card_simple_new (qa->card);
	e_card_simple_set (simple, E_CARD_SIMPLE_FIELD_EMAIL, email);
	e_card_simple_sync_card (simple);
	gtk_object_unref (GTK_OBJECT (simple));
}

static void
merge_cb (EBook *book, gpointer closure)
{
	QuickAdd *qa = (QuickAdd *) closure;

	if (book != NULL) {
		e_card_merging_book_add_card (book, qa->card, NULL, NULL);
		if (qa->cb)
			qa->cb (qa->card, qa->closure);
	} else {
		/* Something went wrong. */
		if (qa->cb)
			qa->cb (NULL, qa->closure);
	}
	
	quick_add_unref (qa);
}

static void
quick_add_merge_card (QuickAdd *qa)
{
	quick_add_ref (qa);
	e_book_use_local_address_book (merge_cb, qa);
}


/*
 * Raise a contact editor with all fields editable, and hook up all signals accordingly.
 */

static void
add_card_cb (EContactEditor *ce, ECard *card, gpointer closure)
{
	QuickAdd *qa = (QuickAdd *) closure;
	g_warning ("add_card_cb");
	quick_add_merge_card (qa);
}

static void
editor_closed_cb (GtkWidget *w, gpointer closure)
{
	QuickAdd *qa = (QuickAdd *) closure;
	g_warning ("editor_closed_cb");
	quick_add_unref (qa);
	gtk_object_unref (GTK_OBJECT (w));
}

static void
ce_book_found_fields (EBook *book, EBookStatus status, EList *fields, gpointer closure)
{
	QuickAdd *qa = (QuickAdd *) closure;
	EContactEditor *contact_editor;

	if (status != E_BOOK_STATUS_SUCCESS) {
		g_warning ("Couldn't find supported fields for local address book.");
		return;
	}

	contact_editor = e_contact_editor_new (qa->card, TRUE, fields, FALSE /* XXX */);

	gtk_signal_connect (GTK_OBJECT (contact_editor),
			    "add_card",
			    GTK_SIGNAL_FUNC (add_card_cb),
			    qa);
	gtk_signal_connect (GTK_OBJECT (contact_editor),
			    "editor_closed",
			    GTK_SIGNAL_FUNC (editor_closed_cb),
			    qa);

	e_contact_editor_show (contact_editor);
}

static void
ce_get_fields (EBook *book, gpointer closure)
{
	QuickAdd *qa = (QuickAdd *) closure;

	if (book == NULL) {
		g_warning ("Couldn't open local address book.");
		quick_add_unref (qa);
	} else {
		e_book_get_supported_fields (book, ce_book_found_fields, qa);
	}
}

static void
edit_card (QuickAdd *qa)
{
	e_book_use_local_address_book (ce_get_fields, qa);
}

static void
clicked_cb (GtkWidget *w, gint button, gpointer closure)
{
	QuickAdd *qa = (QuickAdd *) closure;

	/* Get data out of entries. */
	if (button == 0 || button == 1) {
		gchar *name = NULL;
		gchar *email = NULL;

		if (qa->name_entry) {
			gchar *tmp;
			tmp = gtk_editable_get_chars (GTK_EDITABLE (qa->name_entry), 0, -1);
			name = e_utf8_from_gtk_string (qa->name_entry, tmp);
			g_free (tmp);
		}

		if (qa->email_entry) {
			gchar *tmp;
			tmp = gtk_editable_get_chars (GTK_EDITABLE (qa->email_entry), 0, -1);
			email = e_utf8_from_gtk_string (qa->email_entry, tmp);
			g_free (tmp);
		}

		quick_add_set_name (qa, name);
		quick_add_set_email (qa, email);
	
		g_free (name);
		g_free (email);
	}

	gtk_widget_destroy (w);

	if (button == 0) {

		/* OK */
		quick_add_merge_card (qa);

	} else if (button == 1) {
		
		/* EDIT FULL */
		edit_card (qa);

	} else {
		/* CANCEL */
		quick_add_unref (qa);
	}

}

static GtkWidget *
build_quick_add_dialog (QuickAdd *qa)
{
	GtkWidget *dialog;
	GtkTable *table;
	const gint xpad=1, ypad=1;

	g_return_val_if_fail (qa != NULL, NULL);

	dialog = gnome_dialog_new (_("Contact Quick-Add"),
				   GNOME_STOCK_BUTTON_OK,
				   _("Edit Full"),
				   GNOME_STOCK_BUTTON_CANCEL,
				   NULL);

	gtk_signal_connect (GTK_OBJECT (dialog),
			    "clicked",
			    clicked_cb,
			    qa);

	qa->name_entry = gtk_entry_new ();
	if (qa->name) {
		gchar *str = e_utf8_to_gtk_string (qa->name_entry, qa->name);
		gtk_entry_set_text (GTK_ENTRY (qa->name_entry), str);
		g_free (str);
	}


	qa->email_entry = gtk_entry_new ();
	if (qa->email) {
		gchar *str = e_utf8_to_gtk_string (qa->email_entry, qa->email);
		gtk_entry_set_text (GTK_ENTRY (qa->email_entry), str);
		g_free (str);
	}

	table = GTK_TABLE (gtk_table_new (2, 2, FALSE));

	gtk_table_attach (table, gtk_label_new (_("Full Name")),
			  0, 1, 0, 1,
			  0, 0, xpad, ypad);
	gtk_table_attach (table, qa->name_entry,
			  1, 2, 0, 1,
			  GTK_EXPAND | GTK_FILL, GTK_EXPAND, xpad, ypad);
	gtk_table_attach (table, gtk_label_new (_("E-mail")),
			  0, 1, 1, 2,
			  0, 0, xpad, ypad);
	gtk_table_attach (table, qa->email_entry,
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
		     EContactQuickAddCallback cb, gpointer closure)
{
	QuickAdd *qa;
	GtkWidget *dialog;

	/* We need to have *something* to work with. */
	if (name == NULL && email == NULL) {
		if (cb)
			cb (NULL, closure);
		return;
	}

	qa = quick_add_new ();
	qa->cb = cb;
	qa->closure = closure;
	if (name)
		quick_add_set_name (qa, name);
	if (email)
		quick_add_set_email (qa, email);

	dialog = build_quick_add_dialog (qa);
	gtk_widget_show_all (dialog);
}

void
e_contact_quick_add_free_form (const gchar *text, EContactQuickAddCallback cb, gpointer closure)
{
	gchar *name=NULL, *email=NULL;
	const gchar *last_at, *s;
	gboolean in_quote;

	if (text == NULL) {
		e_contact_quick_add (NULL, NULL, cb, closure);
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
	

	e_contact_quick_add (name, email, cb, closure);
	g_free (name);
	g_free (email);
}
