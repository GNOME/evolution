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
 * modify it under the terms of version 2 of the GNU General Public
 * License as published by the Free Software Foundation.
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
#include <string.h>
#include <glib.h>
#include <gtk/gtkentry.h>
#include <gtk/gtklabel.h>
#include <gtk/gtktable.h>
#include <gtk/gtkdialog.h>
#include <gtk/gtkstock.h>
#include <libgnome/gnome-i18n.h>
#include <libgnomeui/gnome-app.h>
#include <libebook/e-book.h>
#include <libebook/e-contact.h>
#include <addressbook/gui/component/addressbook.h>
#include <addressbook/util/eab-book-util.h>
#include "e-contact-editor.h"
#include "e-contact-quick-add.h"
#include "eab-contact-merging.h"

typedef struct _QuickAdd QuickAdd;
struct _QuickAdd {
	gchar *name;
	gchar *email;
	EContact *contact;

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
	qa->contact = e_contact_new ();
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
			g_free (qa->name);
			g_free (qa->email);
			g_object_unref (qa->contact);
			g_free (qa);
		}
	}
}

static void
quick_add_set_name (QuickAdd *qa, const gchar *name)
{
	if (name == qa->name)
		return;

	g_free (qa->name);
	qa->name = g_strdup (name);
}

static void
quick_add_set_email (QuickAdd *qa, const gchar *email)
{
	if (email == qa->email)
		return;

	g_free (qa->email);
	qa->email = g_strdup (email);
}

static void
merge_cb (EBook *book, EBookStatus status, gpointer closure)
{
	QuickAdd *qa = (QuickAdd *) closure;

	if (status == E_BOOK_ERROR_OK) {
		eab_merging_book_add_contact (book, qa->contact, NULL, NULL);
		if (qa->cb)
			qa->cb (qa->contact, qa->closure);
		g_object_unref (book);
	} else {
		/* Something went wrong. */
		if (book)
			g_object_unref (book);
		if (qa->cb)
			qa->cb (NULL, qa->closure);
	}
	
	quick_add_unref (qa);
}

static void
quick_add_merge_contact (QuickAdd *qa)
{
	quick_add_ref (qa);

	addressbook_load_default_book (merge_cb, qa);
}


/*
 * Raise a contact editor with all fields editable, and hook up all signals accordingly.
 */

static void
contact_added_cb (EContactEditor *ce, EBookStatus status, EContact *contact, gpointer closure)
{
	QuickAdd *qa = (QuickAdd *) g_object_get_data (G_OBJECT (ce), "quick_add");

	if (qa) {

		if (qa->cb)
			qa->cb (qa->contact, qa->closure);
	
		/* We don't need to unref qa because we set_data_full below */
		g_object_set_data (G_OBJECT (ce), "quick_add", NULL);
	}
}

static void
editor_closed_cb (GtkWidget *w, gpointer closure)
{
	QuickAdd *qa = (QuickAdd *) g_object_get_data (G_OBJECT (w), "quick_add");

	if (qa)
		/* We don't need to unref qa because we set_data_full below */
		g_object_set_data (G_OBJECT (w), "quick_add", NULL);

	g_object_unref (w);
}

static void
ce_have_book (EBook *book, EBookStatus status, gpointer closure)
{
	QuickAdd *qa = (QuickAdd *) closure;

	if (status != E_BOOK_ERROR_OK) {
		if (book)
			g_object_unref (book);
		g_warning ("Couldn't open local address book.");
		quick_add_unref (qa);
	} else {
		EContactEditor *contact_editor = e_contact_editor_new (book, qa->contact, TRUE, TRUE /* XXX */);

		/* mark it as changed so the Save buttons are enabled when we bring up the dialog. */
		g_object_set (contact_editor,
				"changed", TRUE,
				NULL);

		/* We pass this via object data, so that we don't get a dangling pointer referenced if both
		   the "contact_added" and "editor_closed" get emitted.  (Which, based on a backtrace in bugzilla,
		   I think can happen and cause a crash. */
		g_object_set_data_full (G_OBJECT (contact_editor), "quick_add", qa,
					(GDestroyNotify) quick_add_unref);

		g_signal_connect (contact_editor,
				  "contact_added",
				  G_CALLBACK (contact_added_cb),
				  NULL);
		g_signal_connect (contact_editor,
				  "editor_closed",
				  G_CALLBACK (editor_closed_cb),
				  NULL);

		g_object_unref (book);
	}
}

static void
edit_contact (QuickAdd *qa)
{
	addressbook_load_default_book (ce_have_book, qa);
}

#define QUICK_ADD_RESPONSE_EDIT_FULL 2

static void
clicked_cb (GtkWidget *w, gint button, gpointer closure)
{
	QuickAdd *qa = (QuickAdd *) closure;

	/* Get data out of entries. */
	if (button == GTK_RESPONSE_OK || button == QUICK_ADD_RESPONSE_EDIT_FULL) {
		gchar *name = NULL;
		gchar *email = NULL;

		if (qa->name_entry) {
			gchar *tmp;
			tmp = gtk_editable_get_chars (GTK_EDITABLE (qa->name_entry), 0, -1);
			name = tmp;
		}

		if (qa->email_entry) {
			gchar *tmp;
			tmp = gtk_editable_get_chars (GTK_EDITABLE (qa->email_entry), 0, -1);
			email = tmp;
		}

		e_contact_set (qa->contact, E_CONTACT_FULL_NAME, (char *) name ? name : "");
		e_contact_set (qa->contact, E_CONTACT_EMAIL_1,   (char *) email ? email : "");

		g_free (name);
		g_free (email);
	}

	gtk_widget_destroy (w);

	if (button == GTK_RESPONSE_OK) {

		/* OK */
		quick_add_merge_contact (qa);

	} else if (button == QUICK_ADD_RESPONSE_EDIT_FULL) {
		
		/* EDIT FULL */
		edit_contact (qa);

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
	const gint xpad=6, ypad=6;

	g_return_val_if_fail (qa != NULL, NULL);

	dialog = gtk_dialog_new_with_buttons (_("Contact Quick-Add"),
					      NULL, /* XXX */
					      (GtkDialogFlags) 0,
					      _("_Edit Full"), QUICK_ADD_RESPONSE_EDIT_FULL,
					        GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
					      GTK_STOCK_OK, GTK_RESPONSE_OK,
					      NULL);

	g_signal_connect (dialog, "response", G_CALLBACK (clicked_cb), qa);

	qa->name_entry = gtk_entry_new ();
	if (qa->name)
		gtk_entry_set_text (GTK_ENTRY (qa->name_entry), qa->name);


	qa->email_entry = gtk_entry_new ();
	if (qa->email)
		gtk_entry_set_text (GTK_ENTRY (qa->email_entry), qa->email);

	table = GTK_TABLE (gtk_table_new (2, 2, FALSE));

	gtk_table_attach (table, gtk_label_new_with_mnemonic (_("_Full Name:")),
			  0, 1, 0, 1,
			  0, 0, xpad, ypad);
	gtk_table_attach (table, qa->name_entry,
			  1, 2, 0, 1,
			  GTK_EXPAND | GTK_FILL, GTK_EXPAND, xpad, ypad);
	gtk_table_attach (table, gtk_label_new_with_mnemonic (_("E-_mail:")),
			  0, 1, 1, 2,
			  0, 0, xpad, ypad);
	gtk_table_attach (table, qa->email_entry,
			  1, 2, 1, 2,
			  GTK_EXPAND | GTK_FILL, GTK_EXPAND, xpad, ypad);
	gtk_container_set_border_width (GTK_CONTAINER (GTK_DIALOG (dialog)->vbox), 
					6);

	gtk_box_set_spacing (GTK_BOX (GTK_DIALOG (dialog)->vbox),6);

	gtk_box_pack_start (GTK_BOX (GTK_DIALOG (dialog)->vbox),
			    GTK_WIDGET (table),
			    TRUE, TRUE, 6);
	gtk_widget_show_all (GTK_WIDGET (table));
			  
	
	return dialog;
}

void
e_contact_quick_add (const gchar *in_name, const gchar *email,
		     EContactQuickAddCallback cb, gpointer closure)
{
	QuickAdd *qa;
	GtkWidget *dialog;
	gchar *name = NULL;
	gint len;

	/* We need to have *something* to work with. */
	if (in_name == NULL && email == NULL) {
		if (cb)
			cb (NULL, closure);
		return;
	}

	if (in_name) {
		name = g_strdup (in_name);

		/* Remove extra whitespace and the quotes some mailers put around names. */
		g_strstrip (name);
		len = strlen (name);
		if ((name[0] == '\'' && name[len-1] == '\'') || (name[0] == '"' && name[len-1] == '"')) {
			name[0] = ' ';
			name[len-1] = ' ';
		}
		g_strstrip (name);
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

	g_free (name);
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
