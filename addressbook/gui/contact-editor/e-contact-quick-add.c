/*
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
 * Authors:
 *		Jon Trowbridge <trow@ximian.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#include <config.h>
#include <ctype.h>
#include <string.h>
#include <gtk/gtk.h>
#include <glib/gi18n.h>
#include <libebook/e-book.h>
#include <libebook/e-contact.h>
#include <libedataserverui/e-source-combo-box.h>
#include <addressbook/util/addressbook.h>
#include <addressbook/util/eab-book-util.h>
#include "e-contact-editor.h"
#include "e-contact-quick-add.h"
#include "eab-contact-merging.h"
#include "e-util/e-alert-dialog.h"

typedef struct _QuickAdd QuickAdd;
struct _QuickAdd {
	gchar *name;
	gchar *email;
	gchar *vcard;
	EContact *contact;
	EBook *book;

	EContactQuickAddCallback cb;
	gpointer closure;

	GtkWidget *dialog;
	GtkWidget *name_entry;
	GtkWidget *email_entry;
	GtkWidget *combo_box;

	gint refs;

};

static QuickAdd *
quick_add_new (void)
{
	QuickAdd *qa = g_new0 (QuickAdd, 1);
	qa->contact = e_contact_new ();
	qa->book = NULL;
	qa->refs = 1;
	return qa;
}

#if 0
static void
quick_add_ref (QuickAdd *qa)
{
	if (qa) {
		++qa->refs;
	}
}
#endif

static void
quick_add_unref (QuickAdd *qa)
{
	if (qa) {
		--qa->refs;
		if (qa->refs == 0) {
			g_free (qa->name);
			g_free (qa->email);
			g_free (qa->vcard);
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
quick_add_set_vcard (QuickAdd *qa, const gchar *vcard)
{
	if (vcard == qa->vcard)
		return;

	g_free (qa->vcard);
	qa->vcard = g_strdup (vcard);
}

static void
merge_cb (EBook *book, const GError *error, gpointer closure)
{
	QuickAdd *qa = (QuickAdd *) closure;

	if (!error) {
		if (e_book_is_writable (book))
			eab_merging_book_add_contact (book, qa->contact, NULL, NULL);
		else
			e_alert_run_dialog_for_args (e_shell_get_active_window (NULL),
						     "addressbook:error-read-only",
						     e_source_peek_name (e_book_get_source (book)),
						     NULL);

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
	addressbook_load (qa->book, merge_cb, qa);
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
ce_have_contact (EBook *book, const GError *error, EContact *contact, gpointer closure)
{
	QuickAdd *qa = (QuickAdd *) closure;

	if (error) {
		if (book)
			g_object_unref (book);
		g_warning ("Failed to find contact, status %d (%s).", error->code, error->message);
		quick_add_unref (qa);
	} else {
		EShell *shell;
		EABEditor *contact_editor;

		if (contact) {
			/* use found contact */
			if (qa->contact)
				g_object_unref (qa->contact);
			qa->contact = g_object_ref (contact);
		}

		shell = e_shell_get_default ();
		contact_editor = e_contact_editor_new (
			shell, book, qa->contact, TRUE, TRUE /* XXX */);

		/* mark it as changed so the Save buttons are enabled when we bring up the dialog. */
		g_object_set (contact_editor,
				"changed", contact != NULL,
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
ce_have_book (EBook *book, const GError *error, gpointer closure)
{
	QuickAdd *qa = (QuickAdd *) closure;

	if (error) {
		if (book)
			g_object_unref (book);
		g_warning ("Couldn't open local address book (%s).", error->message);
		quick_add_unref (qa);
	} else {
		eab_merging_book_find_contact (book, qa->contact, ce_have_contact, qa);
	}
}

static void
edit_contact (QuickAdd *qa)
{
	addressbook_load (qa->book, ce_have_book, qa);
}

#define QUICK_ADD_RESPONSE_EDIT_FULL 2

static void
clicked_cb (GtkWidget *w, gint button, gpointer closure)
{
	QuickAdd *qa = (QuickAdd *) closure;

	/* Get data out of entries. */
	if (!qa->vcard && (button == GTK_RESPONSE_OK || button == QUICK_ADD_RESPONSE_EDIT_FULL)) {
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

		e_contact_set (qa->contact, E_CONTACT_FULL_NAME, (gpointer) (name ? name : ""));
		e_contact_set (qa->contact, E_CONTACT_EMAIL_1,   (gpointer) (email ? email : ""));

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

static void
sanitize_widgets (QuickAdd *qa)
{
	gboolean enabled = TRUE;

	g_return_if_fail (qa != NULL);
	g_return_if_fail (qa->dialog != NULL);

	/* do not call here e_book_is_writable (qa->book), because it requires opened book, which takes time for remote books */
	enabled = qa->book != NULL && e_source_combo_box_get_active_uid (E_SOURCE_COMBO_BOX (qa->combo_box));

	gtk_dialog_set_response_sensitive (GTK_DIALOG (qa->dialog), QUICK_ADD_RESPONSE_EDIT_FULL, enabled);
	gtk_dialog_set_response_sensitive (GTK_DIALOG (qa->dialog), GTK_RESPONSE_OK, enabled);
}

static void
source_changed (ESourceComboBox *source_combo_box, QuickAdd *qa)
{
	ESource *source;

	source = e_source_combo_box_get_active (source_combo_box);
	if (source != NULL) {
		if (qa->book) {
			g_object_unref (qa->book);
			qa->book = NULL;
		}
		qa->book = e_book_new (source, NULL);
	}

	sanitize_widgets (qa);
}

static GtkWidget *
build_quick_add_dialog (QuickAdd *qa)
{
	ESourceList *source_list;
	GConfClient *gconf_client;
	GtkWidget *container;
	GtkWidget *dialog;
	GtkWidget *label;
	GtkTable *table;
	EBook *book;
	const gint xpad=0, ypad=0;

	g_return_val_if_fail (qa != NULL, NULL);

	dialog = gtk_dialog_new_with_buttons (
		_("Contact Quick-Add"),
		e_shell_get_active_window (NULL),
		GTK_DIALOG_NO_SEPARATOR,
		_("_Edit Full"), QUICK_ADD_RESPONSE_EDIT_FULL,
		GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
		GTK_STOCK_OK, GTK_RESPONSE_OK,
		NULL);

	gtk_widget_ensure_style (dialog);

	container = gtk_dialog_get_action_area (GTK_DIALOG (dialog));
	gtk_container_set_border_width (GTK_CONTAINER (container), 12);
	container = gtk_dialog_get_content_area (GTK_DIALOG (dialog));
	gtk_container_set_border_width (GTK_CONTAINER (container), 0);

	g_signal_connect (dialog, "response", G_CALLBACK (clicked_cb), qa);

	qa->dialog = dialog;

	qa->name_entry = gtk_entry_new ();
	if (qa->name)
		gtk_entry_set_text (GTK_ENTRY (qa->name_entry), qa->name);

	qa->email_entry = gtk_entry_new ();
	if (qa->email)
		gtk_entry_set_text (GTK_ENTRY (qa->email_entry), qa->email);

	if (qa->vcard) {
		/* when adding vCard, then do not allow change name or email */
		gtk_widget_set_sensitive (qa->name_entry, FALSE);
		gtk_widget_set_sensitive (qa->email_entry, FALSE);
	}

	gconf_client = gconf_client_get_default ();
	source_list = e_source_list_new_for_gconf (gconf_client, "/apps/evolution/addressbook/sources");
	g_object_unref (gconf_client);
	qa->combo_box = e_source_combo_box_new (source_list);
	book = e_book_new_default_addressbook (NULL);
	e_source_combo_box_set_active (
		E_SOURCE_COMBO_BOX (qa->combo_box),
		e_book_get_source (book));

	if (!e_source_combo_box_get_active_uid (E_SOURCE_COMBO_BOX (qa->combo_box))) {
		/* this means the e_book_new_default_addressbook didn't find any "default" nor "system" source,
		    and created new one for us. That is wrong, choose one from combo instead. */

		if (book) {
			g_object_unref (book);
			book = NULL;
		}

		book = e_book_new (e_source_list_peek_source_any (source_list), NULL);
		e_source_combo_box_set_active (E_SOURCE_COMBO_BOX (qa->combo_box), e_book_get_source (book));

		if (!e_source_combo_box_get_active_uid (E_SOURCE_COMBO_BOX (qa->combo_box))) {
			/* Does it failed again? What is going on? */
			if (book)
				g_object_unref (book);
			book = NULL;
		}
	}

	if (qa->book) {
		g_object_unref (qa->book);
		qa->book = NULL;
	}
	qa->book = book;
	source_changed (E_SOURCE_COMBO_BOX (qa->combo_box), qa);
	g_signal_connect (
		qa->combo_box, "changed",
		G_CALLBACK (source_changed), qa);

	g_object_unref (source_list);

	table = GTK_TABLE (gtk_table_new (3, 2, FALSE));
	gtk_table_set_row_spacings (table, 6);
	gtk_table_set_col_spacings (table, 12);

	label = gtk_label_new_with_mnemonic (_("_Full name"));
	gtk_label_set_mnemonic_widget ((GtkLabel*)label, qa->name_entry);
	gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);

	gtk_table_attach (table, label,
			  0, 1, 0, 1,
			  GTK_FILL, 0, xpad, ypad);
	gtk_table_attach (table, qa->name_entry,
			  1, 2, 0, 1,
			  GTK_EXPAND | GTK_FILL, 0, xpad, ypad);

	label = gtk_label_new_with_mnemonic (_("E_mail"));
	gtk_label_set_mnemonic_widget ((GtkLabel *)label, qa->email_entry);
	gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);

	gtk_table_attach (table, label,
			  0, 1, 1, 2,
			  GTK_FILL, 0, xpad, ypad);
	gtk_table_attach (table, qa->email_entry,
			  1, 2, 1, 2,
			  GTK_EXPAND | GTK_FILL, 0, xpad, ypad);

	label = gtk_label_new_with_mnemonic (_("_Select Address Book"));
	gtk_label_set_mnemonic_widget ((GtkLabel *)label, qa->combo_box);
	gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);

	gtk_table_attach (table, label,
			  0, 1, 2, 3,
			  GTK_FILL, 0, xpad, ypad);
	gtk_table_attach (table, qa->combo_box,
			  1, 2, 2, 3,
			  GTK_EXPAND | GTK_FILL, 0, xpad, ypad);

	gtk_container_set_border_width (GTK_CONTAINER (table), 12);
	container = gtk_dialog_get_content_area (GTK_DIALOG (dialog));
	gtk_box_pack_start (
		GTK_BOX (container), GTK_WIDGET (table), FALSE, FALSE, 0);
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

void
e_contact_quick_add_email (const gchar *email, EContactQuickAddCallback cb, gpointer closure)
{
	gchar *name = NULL;
	gchar *addr = NULL;
	gchar *lt, *gt;

	/* Handle something of the form "Foo <foo@bar.com>".  This is more
	 * more forgiving than the free-form parser, allowing for unquoted
	 * whitespace since we know the whole string is an email address. */

	lt = (email != NULL) ? strchr (email, '<') : NULL;
	gt = (lt != NULL) ? strchr (email, '>') : NULL;

	if (lt != NULL && gt != NULL && (gt - lt) > 0) {
		name = g_strndup (email, lt - email);
		addr = g_strndup (lt + 1, gt - lt - 1);
	} else {
		addr = g_strdup (email);
	}

	e_contact_quick_add (name, addr, cb, closure);

	g_free (name);
	g_free (addr);
}

void
e_contact_quick_add_vcard (const gchar *vcard, EContactQuickAddCallback cb, gpointer closure)
{
	QuickAdd *qa;
	GtkWidget *dialog;
	EContact *contact;

	/* We need to have *something* to work with. */
	if (vcard == NULL) {
		if (cb)
			cb (NULL, closure);
		return;
	}

	qa = quick_add_new ();
	qa->cb = cb;
	qa->closure = closure;
	quick_add_set_vcard (qa, vcard);

	contact = e_contact_new_from_vcard (qa->vcard);

	if (contact) {
		GList *emails;
		gchar *name;
		EContactName *contact_name;

		g_object_unref (qa->contact);
		qa->contact = contact;

		contact_name = e_contact_get (qa->contact, E_CONTACT_NAME);
		name = e_contact_name_to_string (contact_name);
		quick_add_set_name (qa, name);
		g_free (name);
		e_contact_name_free (contact_name);

		emails = e_contact_get (qa->contact, E_CONTACT_EMAIL);
		if (emails) {
			quick_add_set_email (qa, emails->data);

			g_list_foreach (emails, (GFunc)g_free, NULL);
			g_list_free (emails);
		}
	} else {
		if (cb)
			cb (NULL, closure);

		quick_add_unref (qa);
		g_warning ("Contact's vCard parsing failed!");
		return;
	}

	dialog = build_quick_add_dialog (qa);
	gtk_widget_show_all (dialog);
}
