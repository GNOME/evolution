/*
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 *
 *
 * Authors:
 *		Jon Trowbridge <trow@ximian.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#include "evolution-config.h"

#include <ctype.h>
#include <string.h>
#include <gtk/gtk.h>
#include <glib/gi18n.h>

#include <addressbook/util/eab-book-util.h>
#include "e-contact-editor.h"
#include "e-contact-quick-add.h"
#include "eab-contact-merging.h"

typedef struct _QuickAdd QuickAdd;
struct _QuickAdd {
	gchar *name;
	gchar *email;
	gchar *vcard;
	EContact *contact;
	GCancellable *cancellable;
	EClientCache *client_cache;
	ESource *source;

	EContactQuickAddCallback cb;
	gpointer closure;

	GtkWidget *dialog;
	GtkWidget *name_entry;
	GtkWidget *email_entry;
	GtkWidget *combo_box;

	gint refs;

};

static QuickAdd *
quick_add_new (EClientCache *client_cache)
{
	QuickAdd *qa = g_slice_new0 (QuickAdd);
	qa->contact = e_contact_new ();
	qa->client_cache = g_object_ref (client_cache);
	qa->refs = 1;
	return qa;
}

static void
quick_add_unref (QuickAdd *qa)
{
	if (qa) {
		--qa->refs;
		if (qa->refs == 0) {
			if (qa->cancellable != NULL) {
				g_cancellable_cancel (qa->cancellable);
				g_object_unref (qa->cancellable);
			}
			g_free (qa->name);
			g_free (qa->email);
			g_free (qa->vcard);
			g_object_unref (qa->contact);
			g_object_unref (qa->client_cache);
			g_slice_free (QuickAdd, qa);
		}
	}
}

static void
quick_add_set_name (QuickAdd *qa,
                    const gchar *name)
{
	if (name == qa->name)
		return;

	g_free (qa->name);
	qa->name = g_strdup (name);
}

static void
quick_add_set_email (QuickAdd *qa,
                     const gchar *email)
{
	if (email == qa->email)
		return;

	g_free (qa->email);
	qa->email = g_strdup (email);
}

static void
quick_add_set_vcard (QuickAdd *qa,
                     const gchar *vcard)
{
	if (vcard == qa->vcard)
		return;

	g_free (qa->vcard);
	qa->vcard = g_strdup (vcard);
}

static void
merge_cb (GObject *source_object,
          GAsyncResult *result,
          gpointer user_data)
{
	QuickAdd *qa = user_data;
	EClient *client;
	GError *error = NULL;

	client = e_client_cache_get_client_finish (
		E_CLIENT_CACHE (source_object), result, &error);

	/* Sanity check. */
	g_return_if_fail (
		((client != NULL) && (error == NULL)) ||
		((client == NULL) && (error != NULL)));

	/* Ignore cancellations. */
	if (g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED)) {
		g_warn_if_fail (client == NULL);
		g_error_free (error);
		return;
	}

	if (error != NULL) {
		e_alert_run_dialog_for_args (
			e_shell_get_active_window (NULL),
			"addressbook:load-error",
			e_source_get_display_name (qa->source),
			error->message,
			NULL);

		if (qa->cb)
			qa->cb (NULL, qa->closure);
		g_error_free (error);
		quick_add_unref (qa);
		return;
	}

	if (!e_client_is_readonly (client)) {
		ESourceRegistry *registry;

		registry = e_client_cache_ref_registry (qa->client_cache);

		eab_merging_book_add_contact (
			registry, E_BOOK_CLIENT (client),
			qa->contact, NULL, NULL, FALSE);

		g_object_unref (registry);
	} else {
		ESource *source = e_client_get_source (client);

		e_alert_run_dialog_for_args (
			e_shell_get_active_window (NULL),
			"addressbook:error-read-only",
			e_source_get_display_name (source),
			NULL);
	}

	if (qa->cb)
		qa->cb (qa->contact, qa->closure);

	g_object_unref (client);

	quick_add_unref (qa);
}

static void
quick_add_merge_contact (QuickAdd *qa)
{
	if (qa->cancellable != NULL) {
		g_cancellable_cancel (qa->cancellable);
		g_object_unref (qa->cancellable);
	}

	qa->cancellable = g_cancellable_new ();

	e_client_cache_get_client (
		qa->client_cache, qa->source,
		E_SOURCE_EXTENSION_ADDRESS_BOOK, (guint32) -1,
		qa->cancellable, merge_cb, qa);
}

/* Raise a contact editor with all fields editable,
 * and hook up all signals accordingly. */

static void
contact_added_cb (EContactEditor *ce,
                  const GError *error,
                  EContact *contact,
                  gpointer closure)
{
	QuickAdd *qa;

	qa = g_object_get_data (G_OBJECT (ce), "quick_add");

	if (qa) {
		if (qa->cb)
			qa->cb (qa->contact, qa->closure);

		/* We don't need to unref qa because we set_data_full below */
		g_object_set_data (G_OBJECT (ce), "quick_add", NULL);
	}
}

static void
editor_closed_cb (GtkWidget *w,
                  gpointer closure)
{
	QuickAdd *qa;

	qa = g_object_get_data (G_OBJECT (w), "quick_add");

	if (qa)
		/* We don't need to unref qa because we set_data_full below */
		g_object_set_data (G_OBJECT (w), "quick_add", NULL);
}

static void
ce_have_contact (EBookClient *book_client,
                 const GError *error,
                 EContact *contact,
                 gpointer closure)
{
	QuickAdd *qa = (QuickAdd *) closure;

	if (error != NULL) {
		if (book_client)
			g_object_unref (book_client);
		e_alert_run_dialog_for_args (
			e_shell_get_active_window (NULL),
			"addressbook:generic-error",
			_("Failed to find contact"),
			error->message,
			NULL);
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
			shell, book_client, qa->contact, TRUE, TRUE /* XXX */);

		/* Mark it as changed so the Save buttons are
		 * enabled when we bring up the dialog. */
		g_object_set (
			contact_editor, "changed", contact != NULL, NULL);

		/* We pass this via object data, so that we don't get a
		 * dangling pointer referenced if both the "contact_added"
		 * and "editor_closed" get emitted.  (Which, based on a
		 * backtrace in bugzilla, I think can happen and cause a
		 * crash. */
		g_object_set_data_full (
			G_OBJECT (contact_editor), "quick_add", qa,
			(GDestroyNotify) quick_add_unref);

		g_signal_connect (
			contact_editor, "contact_added",
			G_CALLBACK (contact_added_cb), NULL);
		g_signal_connect (
			contact_editor, "editor_closed",
			G_CALLBACK (editor_closed_cb), NULL);

		g_object_unref (book_client);
	}
}

static void
ce_have_book (GObject *source_object,
              GAsyncResult *result,
              gpointer user_data)
{
	QuickAdd *qa = user_data;
	EClient *client;
	ESourceRegistry *registry;
	GError *error = NULL;

	client = e_client_cache_get_client_finish (
		E_CLIENT_CACHE (source_object), result, &error);

	/* Sanity check. */
	g_return_if_fail (
		((client != NULL) && (error == NULL)) ||
		((client == NULL) && (error != NULL)));

	/* Ignore cancellations. */
	if (g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED)) {
		g_warn_if_fail (client == NULL);
		g_error_free (error);
		return;
	}

	if (error != NULL) {
		e_alert_run_dialog_for_args (
			e_shell_get_active_window (NULL),
			"addressbook:load-error",
			e_source_get_display_name (qa->source),
			error->message,
			NULL);
		quick_add_unref (qa);
		g_error_free (error);
		return;
	}

	registry = e_client_cache_ref_registry (qa->client_cache);

	eab_merging_book_find_contact (
		registry, E_BOOK_CLIENT (client),
		qa->contact, ce_have_contact, qa);

	g_object_unref (registry);
}

static void
edit_contact (QuickAdd *qa)
{
	if (qa->cancellable != NULL) {
		g_cancellable_cancel (qa->cancellable);
		g_object_unref (qa->cancellable);
	}

	qa->cancellable = g_cancellable_new ();

	e_client_cache_get_client (
		qa->client_cache, qa->source,
		E_SOURCE_EXTENSION_ADDRESS_BOOK, (guint32) -1,
		qa->cancellable, ce_have_book, qa);
}

#define QUICK_ADD_RESPONSE_EDIT_FULL 2

static void
clicked_cb (GtkWidget *w,
            gint button,
            gpointer closure)
{
	QuickAdd *qa = (QuickAdd *) closure;

	/* Get data out of entries. */
	if (!qa->vcard && (button == GTK_RESPONSE_OK ||
			button == QUICK_ADD_RESPONSE_EDIT_FULL)) {
		gchar *name = NULL;
		gchar *email = NULL;

		if (qa->name_entry)
			name = gtk_editable_get_chars (
				GTK_EDITABLE (qa->name_entry), 0, -1);

		if (qa->email_entry)
			email = gtk_editable_get_chars (
				GTK_EDITABLE (qa->email_entry), 0, -1);

		e_contact_set (
			qa->contact, E_CONTACT_FULL_NAME,
			(name != NULL) ? name : "");

		e_contact_set (
			qa->contact, E_CONTACT_EMAIL_1,
			(email != NULL) ? email : "");

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
	GtkComboBox *combo_box;
	const gchar *active_id;
	gboolean enabled = TRUE;

	g_return_if_fail (qa != NULL);
	g_return_if_fail (qa->dialog != NULL);

	combo_box = GTK_COMBO_BOX (qa->combo_box);
	active_id = gtk_combo_box_get_active_id (combo_box);
	enabled = (active_id != NULL);

	gtk_dialog_set_response_sensitive (
		GTK_DIALOG (qa->dialog),
		QUICK_ADD_RESPONSE_EDIT_FULL, enabled);
	gtk_dialog_set_response_sensitive (
		GTK_DIALOG (qa->dialog), GTK_RESPONSE_OK, enabled);
}

static void
source_changed (ESourceComboBox *source_combo_box,
                QuickAdd *qa)
{
	ESource *source;

	source = e_source_combo_box_ref_active (source_combo_box);

	if (source != NULL) {
		if (qa->source != NULL)
			g_object_unref (qa->source);
		qa->source = source;  /* takes reference */
	}

	sanitize_widgets (qa);
}

static GtkWidget *
build_quick_add_dialog (QuickAdd *qa)
{
	GtkWidget *container;
	GtkWidget *dialog;
	GtkWidget *label;
	GtkWidget *grid;
	ESource *source;
	ESourceRegistry *registry;
	const gchar *extension_name;

	g_return_val_if_fail (qa != NULL, NULL);

	dialog = gtk_dialog_new_with_buttons (
		_("Contact Quick-Add"),
		e_shell_get_active_window (NULL),
		0,
		_("_Edit Full"), QUICK_ADD_RESPONSE_EDIT_FULL,
		_("_Cancel"), GTK_RESPONSE_CANCEL,
		_("_OK"), GTK_RESPONSE_OK,
		NULL);

	container = gtk_dialog_get_action_area (GTK_DIALOG (dialog));
	gtk_container_set_border_width (GTK_CONTAINER (container), 12);
	container = gtk_dialog_get_content_area (GTK_DIALOG (dialog));
	gtk_container_set_border_width (GTK_CONTAINER (container), 0);

	g_signal_connect (
		dialog, "response",
		G_CALLBACK (clicked_cb), qa);

	qa->dialog = dialog;

	qa->name_entry = gtk_entry_new ();
	gtk_widget_set_hexpand (qa->name_entry, TRUE);
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

	extension_name = E_SOURCE_EXTENSION_ADDRESS_BOOK;
	registry = e_client_cache_ref_registry (qa->client_cache);
	source = e_source_registry_ref_default_address_book (registry);
	g_object_unref (registry);

	qa->combo_box = e_client_combo_box_new (
		qa->client_cache, extension_name);
	e_source_combo_box_set_active (
		E_SOURCE_COMBO_BOX (qa->combo_box), source);

	g_object_unref (source);

	source_changed (E_SOURCE_COMBO_BOX (qa->combo_box), qa);
	g_signal_connect (
		qa->combo_box, "changed",
		G_CALLBACK (source_changed), qa);

	grid = g_object_new (
		GTK_TYPE_GRID,
		"row-spacing", 6,
		"column-spacing", 12,
		"margin-start", 12,
		"margin-end", 12,
		"margin-top", 12,
		"margin-bottom", 12,
		NULL);

	label = gtk_label_new_with_mnemonic (_("_Full name"));
	gtk_label_set_mnemonic_widget ((GtkLabel *) label, qa->name_entry);
	gtk_label_set_xalign (GTK_LABEL (label), 0);

	gtk_grid_attach (
		GTK_GRID (grid), label,
		0, 0, 1, 1);
	gtk_grid_attach (
		GTK_GRID (grid), qa->name_entry,
		1, 0, 1, 1);

	label = gtk_label_new_with_mnemonic (_("E_mail"));
	gtk_label_set_mnemonic_widget ((GtkLabel *) label, qa->email_entry);
	gtk_label_set_xalign (GTK_LABEL (label), 0);

	gtk_grid_attach (
		GTK_GRID (grid), label,
		0, 1, 1, 1);
	gtk_grid_attach (
		GTK_GRID (grid), qa->email_entry,
		1, 1, 1, 1);

	label = gtk_label_new_with_mnemonic (_("_Select Address Book"));
	gtk_label_set_mnemonic_widget ((GtkLabel *) label, qa->combo_box);
	gtk_label_set_xalign (GTK_LABEL (label), 0);

	gtk_grid_attach (
		GTK_GRID (grid), label,
		0, 2, 1, 1);
	gtk_grid_attach (
		GTK_GRID (grid), qa->combo_box,
		1, 2, 1, 1);

	container = gtk_dialog_get_content_area (GTK_DIALOG (dialog));
	gtk_box_pack_start (
		GTK_BOX (container), grid, FALSE, FALSE, 0);
	gtk_widget_show_all (grid);

	return dialog;
}

void
e_contact_quick_add (EClientCache *client_cache,
                     const gchar *in_name,
                     const gchar *email,
                     EContactQuickAddCallback cb,
                     gpointer closure)
{
	QuickAdd *qa;
	GtkWidget *dialog;
	gchar *name = NULL;
	gint len;

	g_return_if_fail (E_IS_CLIENT_CACHE (client_cache));

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
		if ((name[0] == '\'' && name[len - 1] == '\'') ||
			(name[0] == '"' && name[len - 1] == '"')) {
			name[0] = ' ';
			name[len - 1] = ' ';
		}
		g_strstrip (name);
	}

	qa = quick_add_new (client_cache);
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
e_contact_quick_add_free_form (EClientCache *client_cache,
                               const gchar *text,
                               EContactQuickAddCallback cb,
                               gpointer closure)
{
	gchar *name = NULL, *email = NULL;
	const gchar *last_at, *s;
	gboolean in_quote;

	g_return_if_fail (E_IS_CLIENT_CACHE (client_cache));

	if (text == NULL) {
		e_contact_quick_add (client_cache, NULL, NULL, cb, closure);
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
		       && !(isspace ((gint) *last_at) ||
			  *last_at == '<' ||
			  *last_at == '"')) {
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
				name = g_strndup (text, last_at - text);
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
		if (email[strlen (email) - 1] == '>') {
			email[strlen (email) - 1] = ' ';
			changed = TRUE;
		}
		if (changed)
			g_strstrip (email);
	}

	e_contact_quick_add (client_cache, name, email, cb, closure);

	g_free (name);
	g_free (email);
}

void
e_contact_quick_add_email (EClientCache *client_cache,
                           const gchar *email,
                           EContactQuickAddCallback cb,
                           gpointer closure)
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

	e_contact_quick_add (client_cache, name, addr, cb, closure);

	g_free (name);
	g_free (addr);
}

void
e_contact_quick_add_vcard (EClientCache *client_cache,
                           const gchar *vcard,
                           EContactQuickAddCallback cb,
                           gpointer closure)
{
	QuickAdd *qa;
	GtkWidget *dialog;
	EContact *contact;

	g_return_if_fail (E_IS_CLIENT_CACHE (client_cache));

	/* We need to have *something* to work with. */
	if (vcard == NULL) {
		if (cb)
			cb (NULL, closure);
		return;
	}

	qa = quick_add_new (client_cache);
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

			g_list_foreach (emails, (GFunc) g_free, NULL);
			g_list_free (emails);
		}
	} else {
		e_alert_run_dialog_for_args (
			e_shell_get_active_window (NULL),
			"addressbook:generic-error",
			_("Failed to parse vCard data"),
			qa->vcard,
			NULL);
		if (cb)
			cb (NULL, closure);

		quick_add_unref (qa);
		return;
	}

	dialog = build_quick_add_dialog (qa);
	gtk_widget_show_all (dialog);
}
