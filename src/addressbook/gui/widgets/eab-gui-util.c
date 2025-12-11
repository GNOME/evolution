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
 *		Chris Toshok <toshok@ximian.com>
 *		Dan Vratil <dvratil@redhat.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#include "evolution-config.h"

#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <locale.h>

#include <gtk/gtk.h>
#include <glib/gi18n.h>

#include "shell/e-shell.h"

#include "eab-gui-util.h"
#include "util/eab-book-util.h"
#include "eab-contact-merging.h"

/* we link to camel for decoding quoted printable email addresses */
#include <camel/camel.h>

void
eab_error_dialog (EAlertSink *alert_sink,
                  GtkWindow *parent,
                  const gchar *msg,
                  const GError *error)
{
	if (error && error->message) {
		if (alert_sink)
			e_alert_submit (
				alert_sink,
				"addressbook:generic-error",
				msg, error->message, NULL);
		else {
			if (!parent)
				parent = e_shell_get_active_window (NULL);

			e_alert_run_dialog_for_args (
				parent,
				"addressbook:generic-error",
				msg, error->message, NULL);
		}
	}
}

void
eab_load_error_dialog (GtkWidget *parent,
                       EAlertSink *alert_sink,
                       ESource *source,
                       const GError *error)
{
	ESourceBackend *extension;
	gchar *label_string, *label = NULL;
	gboolean can_detail_error = TRUE;
	const gchar *backend_name;
	const gchar *extension_name;

	g_return_if_fail (source != NULL);

	extension_name = E_SOURCE_EXTENSION_ADDRESS_BOOK;
	extension = e_source_get_extension (source, extension_name);
	backend_name = e_source_backend_get_backend_name (extension);

	if (g_error_matches (error, E_CLIENT_ERROR, E_CLIENT_ERROR_OFFLINE_UNAVAILABLE)) {
		can_detail_error = FALSE;
		label_string =
			_("This address book cannot be opened. This either "
			  "means this book is not marked for offline usage "
			  "or not yet downloaded for offline usage. Please "
			  "load the address book once in online mode to "
			  "download its contents.");
	}

	else if (g_strcmp0 (backend_name, "local") == 0) {
		const gchar *user_data_dir;
		const gchar *uid;
		gchar *path;

		uid = e_source_get_uid (source);
		user_data_dir = e_get_user_data_dir ();

		path = g_build_filename (
			user_data_dir, "addressbook", uid, NULL);

		label = g_strdup_printf (
			_("This address book cannot be opened.  Please check that the "
			"path %s exists and that permissions are set to access it."), path);

		g_free (path);
		label_string = label;
	}

#ifndef HAVE_LDAP
	else if (g_strcmp0 (backend_name, "ldap") == 0) {
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

	if (g_error_matches (error, E_CLIENT_ERROR, E_CLIENT_ERROR_REPOSITORY_OFFLINE)) {
		/* Do not show a detailed message; too generic. */
	} else if (can_detail_error && error != NULL) {
		label = g_strconcat (
			label_string, "\n\n",
			_("Detailed error message:"),
			" ", error->message, NULL);
		label_string = label;
	}

	if (alert_sink) {
		e_alert_submit (
			alert_sink, "addressbook:load-error",
			e_source_get_display_name (source),
			label_string, NULL);
	} else {
		GtkWidget *dialog;

		dialog = e_alert_dialog_new_for_args (
			(GtkWindow *) parent,
			"addressbook:load-error",
			e_source_get_display_name (source),
			label_string, NULL);
		g_signal_connect (
			dialog, "response",
			G_CALLBACK (gtk_widget_destroy), NULL);
		gtk_widget_show (dialog);
	}

	g_free (label);
}

void
eab_search_result_dialog (EAlertSink *alert_sink,
                          const GError *error)
{
	gchar *str = NULL;

	if (!error)
		return;

	if (error->domain == E_CLIENT_ERROR) {
		switch (error->code) {
		case E_CLIENT_ERROR_SEARCH_SIZE_LIMIT_EXCEEDED:
			str = _("More cards matched this query than either the server is \n"
				"configured to return or Evolution is configured to display.\n"
				"Please make your search more specific or raise the result limit in\n"
				"the directory server preferences for this address book.");
			str = g_strdup (str);
			break;
		case E_CLIENT_ERROR_SEARCH_TIME_LIMIT_EXCEEDED:
			str = _("The time to execute this query exceeded the server limit or the limit\n"
				"configured for this address book.  Please make your search\n"
				"more specific or raise the time limit in the directory server\n"
				"preferences for this address book.");
			str = g_strdup (str);
			break;
		case E_CLIENT_ERROR_INVALID_QUERY:
			/* Translators: %s is replaced with a detailed error message, or an empty string, if not provided */
			str = _("The backend for this address book was unable to parse this query. %s");
			str = g_strdup_printf (str, error->message);
			break;
		case E_CLIENT_ERROR_QUERY_REFUSED:
			/* Translators: %s is replaced with a detailed error message, or an empty string, if not provided */
			str = _("The backend for this address book refused to perform this query. %s");
			str = g_strdup_printf (str, error->message);
			break;
		case E_CLIENT_ERROR_OTHER_ERROR:
		default:
			/* Translators: %s is replaced with a detailed error message, or an empty string, if not provided */
			str = _("This query did not complete successfully. %s");
			str = g_strdup_printf (str, error->message);
			break;
		}
	} else {
		/* Translators: %s is replaced with a detailed error message, or an empty string, if not provided */
		str = _("This query did not complete successfully. %s");
		str = g_strdup_printf (str, error->message);
	}

	e_alert_submit (alert_sink, "addressbook:search-error", str, NULL);

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

	e_util_make_safe_filename (safe);

	return safe;
}

static void
source_selection_changed_cb (ESourceSelector *selector,
                             GtkWidget *ok_button)
{
	ESource *except_source = NULL, *selected;
	gboolean sensitive;

	except_source = g_object_get_data (G_OBJECT (ok_button), "except-source");
	selected = e_source_selector_ref_primary_selection (selector);

	sensitive = (selected != NULL && selected != except_source);
	gtk_widget_set_sensitive (ok_button, sensitive);

	if (selected != NULL)
		g_object_unref (selected);
}

ESource *
eab_select_source (ESourceRegistry *registry,
                   ESource *except_source,
                   const gchar *title,
                   const gchar *message,
                   const gchar *select_uid,
                   GtkWindow *parent)
{
	ESource *source;
	GtkWidget *content_area;
	GtkWidget *dialog;
	GtkWidget *ok_button;
	/* GtkWidget *label; */
	GtkWidget *selector;
	GtkWidget *scrolled_window;
	const gchar *extension_name;
	gint response;

	g_return_val_if_fail (E_IS_SOURCE_REGISTRY (registry), NULL);

	dialog = gtk_dialog_new_with_buttons (
		_("Select Address Book"), parent,
		GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
		_("_Cancel"), GTK_RESPONSE_CANCEL,
		_("_OK"), GTK_RESPONSE_ACCEPT,
		NULL);
	gtk_window_set_default_size (GTK_WINDOW (dialog), 350, 300);

	/* label = gtk_label_new (message); */

	extension_name = E_SOURCE_EXTENSION_ADDRESS_BOOK;
	selector = e_source_selector_new (registry, extension_name);
	e_source_selector_set_show_toggles (
		E_SOURCE_SELECTOR (selector), FALSE);

	ok_button = gtk_dialog_get_widget_for_response (
		GTK_DIALOG (dialog), GTK_RESPONSE_ACCEPT);

	if (except_source)
		g_object_set_data (
			G_OBJECT (ok_button), "except-source", except_source);

	g_signal_connect (
		selector, "primary_selection_changed",
		G_CALLBACK (source_selection_changed_cb), ok_button);

	if (select_uid) {
		source = e_source_registry_ref_source (registry, select_uid);
		if (source != NULL) {
			e_source_selector_set_primary_selection (
				E_SOURCE_SELECTOR (selector), source);
			g_object_unref (source);
		}
	}

	source_selection_changed_cb (E_SOURCE_SELECTOR (selector), ok_button);

	scrolled_window = gtk_scrolled_window_new (NULL, NULL);
	gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (scrolled_window), GTK_SHADOW_IN);
	gtk_container_add (GTK_CONTAINER (scrolled_window), selector);

	content_area = gtk_dialog_get_content_area (GTK_DIALOG (dialog));
	gtk_box_pack_start (GTK_BOX (content_area), scrolled_window, TRUE, TRUE, 4);

	gtk_widget_show_all (dialog);
	response = gtk_dialog_run (GTK_DIALOG (dialog));

	if (response == GTK_RESPONSE_ACCEPT)
		source = e_source_selector_ref_primary_selection (
			E_SOURCE_SELECTOR (selector));
	else
		source = NULL;

	gtk_widget_destroy (dialog);

	/* XXX Return a borrowed reference for backward-compatibility. */
	if (source != NULL)
		g_object_unref (source);

	return source;
}

gchar *
eab_suggest_filename (EContact *contact)
{
	gchar *res = NULL;

	if (contact) {
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

struct ContactCopyProcess_ {
	gint count;
	gboolean book_status;
	GSList *contacts;
	EBookClient *source;
	EBookClient *destination;
	ESourceRegistry *registry;
	gboolean delete_from_source;
	EAlertSink *alert_sink;
};

static void process_unref (ContactCopyProcess *process);

static void
remove_contact_ready_cb (GObject *source_object,
                         GAsyncResult *result,
                         gpointer user_data)
{
	EBookClient *book_client = E_BOOK_CLIENT (source_object);
	ContactCopyProcess *process = user_data;
	GError *error = NULL;

	e_book_client_remove_contact_by_uid_finish (book_client, result, &error);

	if (error != NULL) {
		g_warning (
			"%s: Remove contact by uid failed: %s",
			G_STRFUNC, error->message);
		g_error_free (error);
	}

	process_unref (process);
}

static void
do_delete_from_source (gpointer data,
                       gpointer user_data)
{
	ContactCopyProcess *process = user_data;
	EContact *contact = data;
	const gchar *id;
	EBookClient *book_client = process->source;

	id = e_contact_get_const (contact, E_CONTACT_UID);
	g_return_if_fail (id != NULL);
	g_return_if_fail (book_client != NULL);

	process->count++;
	e_book_client_remove_contact_by_uid (book_client, id, E_BOOK_OPERATION_FLAG_NONE, NULL, remove_contact_ready_cb, process);
}

static void
delete_contacts (ContactCopyProcess *process)
{
	if (process->book_status == TRUE) {
		g_slist_foreach (process->contacts,
				do_delete_from_source,
				process);
	}
}

static void
process_unref (ContactCopyProcess *process)
{
	process->count--;
	if (process->count == 0) {
		if (process->delete_from_source) {
			delete_contacts (process);
			/* to not repeate this again */
			process->delete_from_source = FALSE;

			if (process->count > 0)
				return;
		}
		g_slist_free_full (
			process->contacts,
			(GDestroyNotify) g_object_unref);
		g_object_unref (process->source);
		g_object_unref (process->destination);
		g_object_unref (process->registry);
		g_slice_free (ContactCopyProcess, process);
	}
}

static void
contact_added_cb (EBookClient *book_client,
                  const GError *error,
                  const gchar *id,
                  gpointer user_data)
{
	ContactCopyProcess *process = user_data;

	if (g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED)) {
		process->book_status = FALSE;
	} else if (error != NULL) {
		process->book_status = FALSE;
		eab_error_dialog (
			process->alert_sink, NULL,
			_("Error adding contact"), error);
	} else {
		/* success */
		process->book_status = TRUE;
	}

	process_unref (process);
}

static void
do_copy (gpointer data,
         gpointer user_data)
{
	EBookClient *book_client;
	EContact *contact;
	ContactCopyProcess *process;

	process = user_data;
	contact = data;

	book_client = process->destination;
	e_contact_inline_local_photos (contact, NULL);

	process->count++;
	eab_merging_book_add_contact (
		process->registry, book_client,
		contact, NULL, contact_added_cb, process, TRUE);
}

static void
book_client_connect_cb (GObject *source_object,
                        GAsyncResult *result,
                        gpointer user_data)
{
	ContactCopyProcess *process = user_data;
	EClient *client;
	GError *error = NULL;

	client = e_book_client_connect_finish (result, &error);

	/* Sanity check. */
	g_return_if_fail (
		((client != NULL) && (error == NULL)) ||
		((client == NULL) && (error != NULL)));

	if (error != NULL) {
		g_warning ("%s: %s", G_STRFUNC, error->message);
		g_error_free (error);
		goto exit;
	}

	process->destination = E_BOOK_CLIENT (client);
	process->book_status = TRUE;
	g_slist_foreach (process->contacts, do_copy, process);

exit:
	process_unref (process);
}

void
eab_transfer_contacts (ESourceRegistry *registry,
                       EBookClient *source_client,
                       GSList *contacts /* adopted */,
                       gboolean delete_from_source,
                       EAlertSink *alert_sink)
{
	ESource *source;
	ESource *destination;
	static gchar *last_uid = NULL;
	ContactCopyProcess *process;
	gchar *desc;
	GtkWindow *window = GTK_WINDOW (gtk_widget_get_toplevel (GTK_WIDGET (alert_sink)));

	g_return_if_fail (E_IS_SOURCE_REGISTRY (registry));
	g_return_if_fail (E_IS_BOOK_CLIENT (source_client));

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

	source = e_client_get_source (E_CLIENT (source_client));

	destination = eab_select_source (
		registry, source, desc, NULL, last_uid, window);

	if (!destination) {
		g_slist_free_full (contacts, g_object_unref);
		return;
	}

	if (strcmp (last_uid, e_source_get_uid (destination)) != 0) {
		g_free (last_uid);
		last_uid = g_strdup (e_source_get_uid (destination));
	}

	process = g_slice_new0 (ContactCopyProcess);
	process->count = 1;
	process->book_status = FALSE;
	process->source = g_object_ref (source_client);
	process->contacts = contacts;
	process->destination = NULL;
	process->registry = g_object_ref (registry);
	process->alert_sink = alert_sink;
	process->delete_from_source = delete_from_source;

	e_book_client_connect (destination, (guint32) -1, NULL, book_client_connect_cb, process);
}

gboolean
eab_fullname_matches_nickname (EContact *contact)
{
	gchar *nickname, *fullname;
	gboolean same;

	g_return_val_if_fail (E_IS_CONTACT (contact), FALSE);

	nickname = e_contact_get (contact, E_CONTACT_NICKNAME);
	fullname = e_contact_get (contact, E_CONTACT_FULL_NAME);
	same = g_strcmp0 (nickname && *nickname ? nickname : NULL,
			  fullname && *fullname ? fullname : NULL) == 0;

	g_free (nickname);
	g_free (fullname);

	return same;
}
