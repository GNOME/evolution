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
 *		Chris Toshok <toshok@ximian.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#include <config.h>

#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>

#include <gtk/gtk.h>
#include <glib/gi18n.h>
#include <libedataserver/e-data-server-util.h>
#include <libedataserverui/e-book-auth-util.h>
#include <libedataserverui/e-source-selector.h>
#include <e-util/e-util.h>
#include "eab-gui-util.h"
#include "util/eab-book-util.h"
#include <libebook/e-destination.h>
#include "e-util/e-alert-dialog.h"
#include "e-util/e-html-utils.h"
#include "shell/e-shell.h"
#include "misc/e-image-chooser.h"
#include <e-util/e-icon-factory.h>
#include "eab-contact-merging.h"

/* we link to camel for decoding quoted printable email addresses */
#include <camel/camel.h>

void
eab_error_dialog (const gchar *msg, const GError *error)
{
	if (error && error->message)
		e_alert_run_dialog_for_args (e_shell_get_active_window (NULL),
					     "addressbook:generic-error",
					     msg, error->message, NULL);
}

void
eab_load_error_dialog (GtkWidget *parent, ESource *source, const GError *error)
{
	gchar *label_string, *label = NULL, *uri;
	GtkWidget *dialog;
	gboolean can_detail_error = TRUE;

	g_return_if_fail (source != NULL);

	uri = e_source_get_uri (source);

	if (g_error_matches (error, E_BOOK_ERROR, E_BOOK_ERROR_OFFLINE_UNAVAILABLE)) {
		can_detail_error = FALSE;
		label_string = _("This address book cannot be opened. This either means this "
                                 "book is not marked for offline usage or not yet downloaded "
                                 "for offline usage. Please load the address book once in online mode "
                                 "to download its contents.");
	}

	else if (uri && g_str_has_prefix (uri, "local:")) {
		const gchar *user_data_dir;
		const gchar *source_dir;
		gchar *mangled_source_dir;
		gchar *path;

		user_data_dir = e_get_user_data_dir ();
		source_dir = e_source_peek_relative_uri (source);

		if (!source_dir || !g_str_equal (source_dir, "system"))
			source_dir = e_source_peek_uid (source);

		/* Mangle the URI to not contain invalid characters. */
		mangled_source_dir = g_strdelimit (g_strdup (source_dir), ":/", '_');

		path = g_build_filename (
			user_data_dir, "addressbook", mangled_source_dir, NULL);

		g_free (mangled_source_dir);

		label = g_strdup_printf (
			_("This address book cannot be opened.  Please check that the "
			  "path %s exists and that permissions are set to access it."), path);

		g_free (path);
		label_string = label;
	}

#ifndef HAVE_LDAP
	else if (uri && !strncmp (uri, "ldap:", 5)) {
		/* special case for ldap: contact folders so we can tell the user about openldap */

		can_detail_error = FALSE;
		label_string =
			_("This version of Evolution does not have LDAP support "
			  "compiled in to it.  To use LDAP in Evolution "
			  "an LDAP-enabled Evolution package must be installed.");

	}
#endif
	 else {
		/* other network folders (or if ldap is enabled and server is unreachable) */
		label_string =
			_("This address book cannot be opened.  This either "
			  "means that an incorrect URI was entered, or the server "
			  "is unreachable.");
	}

	if (can_detail_error) {
		/* do not show repository offline message, it's kind of generic error */
		if (error && !g_error_matches (error, E_BOOK_ERROR, E_BOOK_ERROR_REPOSITORY_OFFLINE)) {
			label = g_strconcat (label_string, "\n\n", _("Detailed error message:"), " ", error->message, NULL);
			label_string = label;
		}
	}

	dialog  = e_alert_dialog_new_for_args ((GtkWindow *) parent, "addressbook:load-error", label_string, NULL);
	g_signal_connect (dialog, "response", G_CALLBACK (gtk_widget_destroy), NULL);

	gtk_widget_show (dialog);
	g_free (label);
	g_free (uri);
}

void
eab_search_result_dialog      (GtkWidget *parent,
			       EBookViewStatus status,
			       const gchar *error_msg)
{
	gchar *str = NULL;

	switch (status) {
	case E_BOOK_VIEW_STATUS_OK:
		return;
	case E_BOOK_VIEW_STATUS_SIZE_LIMIT_EXCEEDED:
		str = _("More cards matched this query than either the server is \n"
			"configured to return or Evolution is configured to display.\n"
			"Please make your search more specific or raise the result limit in\n"
			"the directory server preferences for this address book.");
		str = g_strdup (str);
		break;
	case E_BOOK_VIEW_STATUS_TIME_LIMIT_EXCEEDED:
		str = _("The time to execute this query exceeded the server limit or the limit\n"
			"configured for this address book.  Please make your search\n"
			"more specific or raise the time limit in the directory server\n"
			"preferences for this address book.");
		str = g_strdup (str);
		break;
	case E_BOOK_VIEW_ERROR_INVALID_QUERY:
		/* Translators: %s is replaced with a detailed error message, or an empty string, if not provided */
		str = _("The backend for this address book was unable to parse this query. %s");
		str = g_strdup_printf (str, error_msg ? error_msg : "");
		break;
	case E_BOOK_VIEW_ERROR_QUERY_REFUSED:
		/* Translators: %s is replaced with a detailed error message, or an empty string, if not provided */
		str = _("The backend for this address book refused to perform this query. %s");
		str = g_strdup_printf (str, error_msg ? error_msg : "");
		break;
	case E_BOOK_VIEW_ERROR_OTHER_ERROR:
		/* Translators: %s is replaced with a detailed error message, or an empty string, if not provided */
		str = _("This query did not complete successfully. %s");
		str = g_strdup_printf (str, error_msg ? error_msg : "");
		break;
	default:
		g_return_if_reached ();
	}

	e_alert_run_dialog_for_args ((GtkWindow *) parent, "addressbook:search-error", str, NULL);

	g_free (str);
}

gint
eab_prompt_save_dialog (GtkWindow *parent)
{
	return e_alert_run_dialog_for_args (parent, "addressbook:prompt-save", NULL);
}

static gchar *
make_safe_filename (gchar *name)
{
	gchar *safe;

	if (!name) {
		/* This is a filename. Translators take note. */
		name = _("card.vcf");
	}

	if (!g_strrstr (name, ".vcf"))
		safe = g_strdup_printf ("%s%s", name, ".vcf");
	else
		safe = g_strdup (name);

	e_filename_make_safe (safe);

	return safe;
}

static void
source_selection_changed_cb (ESourceSelector *selector, GtkWidget *ok_button)
{
	ESource *except_source = NULL, *selected;

	except_source = g_object_get_data (G_OBJECT (ok_button), "except-source");
	selected = e_source_selector_peek_primary_selection (selector);

	gtk_widget_set_sensitive (ok_button, selected && selected != except_source);
}

ESource *
eab_select_source (ESource *except_source, const gchar *title, const gchar *message, const gchar *select_uid, GtkWindow *parent)
{
	ESource *source;
	ESourceList *source_list;
	GtkWidget *content_area;
	GtkWidget *dialog;
	GtkWidget *ok_button;
	/* GtkWidget *label; */
	GtkWidget *selector;
	GtkWidget *scrolled_window;
	gint response;

	if (!e_book_get_addressbooks (&source_list, NULL))
		return NULL;

	dialog = gtk_dialog_new_with_buttons (_("Select Address Book"), parent,
					      GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
					      NULL);
	gtk_window_set_default_size (GTK_WINDOW (dialog), 350, 300);

	gtk_dialog_add_button (GTK_DIALOG (dialog), GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL);
	ok_button = gtk_dialog_add_button (GTK_DIALOG (dialog), GTK_STOCK_OK, GTK_RESPONSE_ACCEPT);
	gtk_widget_set_sensitive (ok_button, FALSE);

	/* label = gtk_label_new (message); */

	selector = e_source_selector_new (source_list);
	e_source_selector_show_selection (E_SOURCE_SELECTOR (selector), FALSE);
	if (except_source)
		g_object_set_data (G_OBJECT (ok_button), "except-source", e_source_list_peek_source_by_uid (source_list, e_source_peek_uid (except_source)));
	g_signal_connect (selector, "primary_selection_changed",
			  G_CALLBACK (source_selection_changed_cb), ok_button);

	if (select_uid) {
		source = e_source_list_peek_source_by_uid (source_list, select_uid);
		if (source)
			e_source_selector_set_primary_selection (E_SOURCE_SELECTOR (selector), source);
	}

	scrolled_window = gtk_scrolled_window_new (NULL, NULL);
	gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (scrolled_window), GTK_SHADOW_IN);
	gtk_container_add (GTK_CONTAINER (scrolled_window), selector);

	content_area = gtk_dialog_get_content_area (GTK_DIALOG (dialog));
	gtk_box_pack_start (GTK_BOX (content_area), scrolled_window, TRUE, TRUE, 4);

	gtk_widget_show_all (dialog);
	response = gtk_dialog_run (GTK_DIALOG (dialog));

	if (response == GTK_RESPONSE_ACCEPT)
		source = e_source_selector_peek_primary_selection (E_SOURCE_SELECTOR (selector));
	else
		source = NULL;

	gtk_widget_destroy (dialog);
	return source;
}

gchar *
eab_suggest_filename (GList *contact_list)
{
	gchar *res = NULL;

	g_return_val_if_fail (contact_list != NULL, NULL);

	if (g_list_length (contact_list) == 1) {
		EContact *contact = E_CONTACT (contact_list->data);
		gchar *string;

		string = e_contact_get (contact, E_CONTACT_FILE_AS);
		if (string == NULL)
			string = e_contact_get (contact, E_CONTACT_FULL_NAME);
		if (string != NULL)
			res = make_safe_filename (string);
		g_free (string);
	}

	if (res == NULL)
		res = make_safe_filename (_("list"));

	return res;
}

typedef struct ContactCopyProcess_ ContactCopyProcess;

typedef void (*ContactCopyDone) (ContactCopyProcess *process);

struct ContactCopyProcess_ {
	gint count;
	gboolean book_status;
	GList *contacts;
	EBook *source;
	EBook *destination;
	ContactCopyDone done_cb;
};

static void
do_delete (gpointer data, gpointer user_data)
{
	EBook *book = user_data;
	EContact *contact = data;
	const gchar *id;

	id = e_contact_get_const (contact, E_CONTACT_UID);
	e_book_remove_contact(book, id, NULL);
}

static void
delete_contacts (ContactCopyProcess *process)
{
	if (process->book_status == TRUE) {
		g_list_foreach (process->contacts,
				do_delete,
				process->source);
	}
}

static void
process_unref (ContactCopyProcess *process)
{
	process->count--;
	if (process->count == 0) {
		if (process->done_cb)
			process->done_cb (process);
		g_list_foreach (
			process->contacts,
			(GFunc) g_object_unref, NULL);
		g_list_free (process->contacts);
		g_object_unref (process->source);
		g_object_unref (process->destination);
		g_free (process);
	}
}

static void
contact_added_cb (EBook* book, const GError *error, const gchar *id, gpointer user_data)
{
	ContactCopyProcess *process = user_data;

	if (error && !g_error_matches (error, E_BOOK_ERROR, E_BOOK_ERROR_CANCELLED)) {
		process->book_status = FALSE;
		eab_error_dialog (_("Error adding contact"), error);
	}
	else if (g_error_matches (error, E_BOOK_ERROR, E_BOOK_ERROR_CANCELLED)) {
		process->book_status = FALSE;
	}
	else {
		/* success */
		process->book_status = TRUE;
	}
	process_unref (process);
}

static void
do_copy (gpointer data, gpointer user_data)
{
	EBook *book;
	EContact *contact;
	ContactCopyProcess *process;

	process = user_data;
	contact = data;

	book = process->destination;

	process->count++;
	eab_merging_book_add_contact(book, contact, contact_added_cb, process);
}

static void
book_loaded_cb (ESource *destination,
                GAsyncResult *result,
                ContactCopyProcess *process)
{
	EBook *book;
	GError *error = NULL;

	book = e_load_book_source_finish (destination, result, &error);

	if (book != NULL) {
		g_warn_if_fail (error == NULL);
		process->destination = book;
		process->book_status = TRUE;
		g_list_foreach (process->contacts, do_copy, process);

	} else if (error != NULL) {
		g_warning ("%s", error->message);
		g_error_free (error);
	}

	process_unref (process);
}

void
eab_transfer_contacts (EBook *source_book,
                       GList *contacts /* adopted */,
                       gboolean delete_from_source,
                       GtkWindow *parent_window)
{
	ESource *destination;
	static gchar *last_uid = NULL;
	ContactCopyProcess *process;
	gchar *desc;

	g_return_if_fail (E_IS_BOOK (source_book));

	if (contacts == NULL)
		return;

	if (last_uid == NULL)
		last_uid = g_strdup ("");

	if (contacts->next == NULL) {
		if (delete_from_source)
			desc = _("Move contact to");
		else
			desc = _("Copy contact to");
	} else {
		if (delete_from_source)
			desc = _("Move contacts to");
		else
			desc = _("Copy contacts to");
	}

	destination = eab_select_source (
		e_book_get_source (source_book),
		desc, NULL, last_uid, parent_window);

	if (!destination)
		return;

	if (strcmp (last_uid, e_source_peek_uid (destination)) != 0) {
		g_free (last_uid);
		last_uid = g_strdup (e_source_peek_uid (destination));
	}

	process = g_new (ContactCopyProcess, 1);
	process->count = 1;
	process->book_status = FALSE;
	process->source = g_object_ref (source_book);
	process->contacts = contacts;
	process->destination = NULL;

	if (delete_from_source)
		process->done_cb = delete_contacts;
	else
		process->done_cb = NULL;

	e_load_book_source_async (
		destination, parent_window, NULL,
		(GAsyncReadyCallback) book_loaded_cb, process);
}

/* To parse something like...
=?UTF-8?Q?=E0=A4=95=E0=A4=95=E0=A4=AC=E0=A5=82=E0=A5=8B=E0=A5=87?=\t\n=?UTF-8?Q?=E0=A4=B0?=\t\n<aa@aa.ccom>
and return the decoded representation of name & email parts.
*/
gboolean
eab_parse_qp_email (const gchar *string, gchar **name, gchar **email)
{
	struct _camel_header_address *address;
	gboolean res = FALSE;

	address = camel_header_address_decode (string, "UTF-8");

	if (!address)
		return FALSE;

	/* report success only when we have filled both name and email address */
	if (address->type == CAMEL_HEADER_ADDRESS_NAME  && address->name && *address->name && address->v.addr && *address->v.addr) {
		*name = g_strdup (address->name);
		*email = g_strdup (address->v.addr);
		res = TRUE;
	}

	camel_header_address_unref (address);

	return res;
}

/* This is only wrapper to parse_qp_mail, it decodes string and if returned TRUE,
   then makes one string and returns it, otherwise returns NULL.
   Returned string is usable to place directly into GtkHtml stream.
   Returned value should be freed with g_free. */
gchar *
eab_parse_qp_email_to_html (const gchar *string)
{
	gchar *name = NULL, *mail = NULL;
	gchar *html_name, *html_mail;
	gchar *value;

	if (!eab_parse_qp_email (string, &name, &mail))
		return NULL;

	html_name = e_text_to_html (name, 0);
	html_mail = e_text_to_html (mail, E_TEXT_TO_HTML_CONVERT_ADDRESSES);

	value = g_strdup_printf ("%s &lt;%s&gt;", html_name, html_mail);

	g_free (html_name);
	g_free (html_mail);
	g_free (name);
	g_free (mail);

	return value;
}
