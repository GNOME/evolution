/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* 
 * e-table-field-chooser.c
 * Copyright (C) 2001  Ximian, Inc.
 * Author: Chris Toshok <toshok@ximian.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of version 2 of the GNU General Public
 * License as published by the Free Software Foundation.
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

#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>

#include <gal/util/e-util.h>
#include "eab-gui-util.h"
#include "util/eab-book-util.h"
#include "util/e-destination.h"
#include "widgets/misc/e-image-chooser.h"
#include "widgets/misc/e-source-selector.h"

#include <gnome.h>

#include "addressbook/gui/contact-editor/eab-editor.h"
#include "addressbook/gui/contact-editor/e-contact-editor.h"
#include "addressbook/gui/contact-list-editor/e-contact-list-editor.h"
#include "addressbook/gui/component/addressbook-component.h"

void
eab_error_dialog (const gchar *msg, EBookStatus status)
{
	static char *status_to_string[] = {
		N_("Success"),
		N_("Unknown error"),
		N_("Repository offline"),
		N_("Permission denied"),
		N_("Contact not found"),
		N_("Contact ID already exists"),
		N_("Protocol not supported"),
		N_("Cancelled"),
		N_("Authentication Failed"),
		N_("Authentication Required"),
		N_("TLS not Available"),
		N_("Address Book does not exist"),
		N_("Other error")
	};
	char *error_msg;
	GtkWidget *dialog;

	error_msg = g_strdup_printf ("%s: %s", msg, _(status_to_string [status]));

	dialog = gtk_message_dialog_new (NULL, 0, GTK_MESSAGE_ERROR, GTK_BUTTONS_OK,
					 error_msg);

	g_signal_connect (dialog, "response", G_CALLBACK (gtk_widget_destroy), NULL);

	gtk_widget_show (dialog);

	g_free (error_msg);
}

void
eab_load_error_dialog (GtkWidget *parent, ESource *source, EBookStatus status)
{
	char *label_string;
	GtkWidget *warning_dialog;
	GtkWidget *href = NULL;
	gchar *uri;

	g_return_if_fail (source != NULL);

	uri = e_source_get_uri (source);

	if (!strncmp (uri, "file:", 5)) {
		label_string = 
			_("We were unable to open this addressbook.  Please check that the "
			  "path exists and that you have permission to access it.");
	}
	else if (!strncmp (uri, "ldap:", 5)) {
		/* special case for ldap: contact folders so we can tell the user about openldap */
#if HAVE_LDAP
		label_string = 
			_("We were unable to open this addressbook.  This either "
			  "means you have entered an incorrect URI, or the LDAP server "
			  "is unreachable.");
#else
		label_string =
			_("This version of Evolution does not have LDAP support "
			  "compiled in to it.  If you want to use LDAP in Evolution "
			  "you must compile the program from the CVS sources after "
			  "retrieving OpenLDAP from the link below.");
		href = gnome_href_new ("http://www.openldap.org/", "OpenLDAP at http://www.openldap.org/");
#endif
	} else {
		/* other network folders */
		label_string =
			_("We were unable to open this addressbook.  This either "
			  "means you have entered an incorrect URI, or the server "
			  "is unreachable.");
	}

	warning_dialog = gtk_message_dialog_new (parent ? GTK_WINDOW (parent) : NULL,
						 0,
						 GTK_MESSAGE_WARNING,
						 GTK_BUTTONS_CLOSE, 
						 label_string,
						 NULL);

	g_signal_connect (warning_dialog, 
			  "response", 
			  G_CALLBACK (gtk_widget_destroy),
			  warning_dialog);

	gtk_window_set_title (GTK_WINDOW (warning_dialog), _("Unable to open addressbook"));

	if (href)
		gtk_box_pack_start (GTK_BOX (GTK_DIALOG (warning_dialog)->vbox), 
				    href, FALSE, FALSE, 0);

	gtk_widget_show_all (warning_dialog);

	g_free (uri);
}

void
eab_search_result_dialog      (GtkWidget *parent,
			       EBookViewStatus status)
{
	char *str = NULL;

	switch (status) {
	case E_BOOK_VIEW_STATUS_OK:
		return;
	case E_BOOK_VIEW_STATUS_SIZE_LIMIT_EXCEEDED:
		str = _("More cards matched this query than either the server is \n"
			"configured to return or Evolution is configured to display.\n"
			"Please make your search more specific or raise the result limit in\n"
			"the directory server preferences for this addressbook.");
		break;
	case E_BOOK_VIEW_STATUS_TIME_LIMIT_EXCEEDED:
		str = _("The time to execute this query exceeded the server limit or the limit\n"
			"you have configured for this addressbook.  Please make your search\n"
			"more specific or raise the time limit in the directory server\n"
			"preferences for this addressbook.");
		break;
	case E_BOOK_VIEW_ERROR_INVALID_QUERY:
		str = _("The backend for this addressbook was unable to parse this query.");
		break;
	case E_BOOK_VIEW_ERROR_QUERY_REFUSED:
		str = _("The backend for this addressbook refused to perform this query.");
		break;
	case E_BOOK_VIEW_ERROR_OTHER_ERROR:
		str = _("This query did not complete successfully.");
		break;
	}

	if (str) {
		GtkWidget *dialog;
		dialog = gtk_message_dialog_new (parent ? GTK_WINDOW (parent) : NULL,
						 0,
						 GTK_MESSAGE_WARNING,
						 GTK_BUTTONS_OK,
						 str);
		g_signal_connect (dialog, "response", G_CALLBACK(gtk_widget_destroy), NULL);
		gtk_widget_show (dialog);
	}
}

gint
eab_prompt_save_dialog (GtkWindow *parent)
{
	GtkWidget *dialog;
	gint response;

	dialog = gtk_message_dialog_new (parent,
					  (GtkDialogFlags)0,
					  GTK_MESSAGE_QUESTION,
					  GTK_BUTTONS_NONE,
					 _("Do you want to save changes?"));
	gtk_dialog_add_buttons (GTK_DIALOG (dialog),
				_("_Discard"), GTK_RESPONSE_NO,
				GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
				GTK_STOCK_SAVE, GTK_RESPONSE_YES,
				NULL);

	gtk_dialog_set_default_response (GTK_DIALOG (dialog), GTK_RESPONSE_YES);

	response = gtk_dialog_run (GTK_DIALOG (dialog));

	gtk_widget_destroy (dialog);

	return response;
}

static void
added_cb (EBook* book, EBookStatus status, const char *id,
	  gboolean is_list)
{
	if (status != E_BOOK_ERROR_OK) {
		eab_error_dialog (is_list ? _("Error adding list") : _("Error adding contact"), status);
	}
}

static void
modified_cb (EBook* book, EBookStatus status,
	     gboolean is_list)
{
	if (status != E_BOOK_ERROR_OK) {
		eab_error_dialog (is_list ? _("Error modifying list") : _("Error modifying contact"),
				  status);
	}
}

static void
deleted_cb (EBook* book, EBookStatus status,
	    gboolean is_list)
{
	if (status != E_BOOK_ERROR_OK) {
		eab_error_dialog (is_list ? _("Error removing list") : _("Error removing contact"),
				  status);
	}
}

static void
editor_closed_cb (GtkObject *editor, gpointer data)
{
	g_object_unref (editor);
}

EContactEditor *
eab_show_contact_editor (EBook *book, EContact *contact,
			 gboolean is_new_contact,
			 gboolean editable)
{
	EContactEditor *ce;

	ce = e_contact_editor_new (book, contact, is_new_contact, editable);

	g_signal_connect (ce, "contact_added",
			  G_CALLBACK (added_cb), GINT_TO_POINTER (FALSE));
	g_signal_connect (ce, "contact_modified",
			  G_CALLBACK (modified_cb), GINT_TO_POINTER (FALSE));
	g_signal_connect (ce, "contact_deleted",
			  G_CALLBACK (deleted_cb), GINT_TO_POINTER (FALSE));
	g_signal_connect (ce, "editor_closed",
			  G_CALLBACK (editor_closed_cb), NULL);

	return ce;
}

EContactListEditor *
eab_show_contact_list_editor (EBook *book, EContact *contact,
			      gboolean is_new_contact,
			      gboolean editable)
{
	EContactListEditor *ce;

	ce = e_contact_list_editor_new (book, contact, is_new_contact, editable);

	g_signal_connect (ce, "contact_added",
			  G_CALLBACK (added_cb), GINT_TO_POINTER (TRUE));
	g_signal_connect (ce, "contact_modified",
			  G_CALLBACK (modified_cb), GINT_TO_POINTER (TRUE));
	g_signal_connect (ce, "contact_deleted",
			  G_CALLBACK (deleted_cb), GINT_TO_POINTER (TRUE));
	g_signal_connect (ce, "editor_closed",
			  G_CALLBACK (editor_closed_cb), GINT_TO_POINTER (TRUE));

	eab_editor_show (EAB_EDITOR (ce));

	return ce;
}

static void
view_contacts (EBook *book, GList *list, gboolean editable)
{
	for (; list; list = list->next) {
		EContact *contact = list->data;
		if (e_contact_get (contact, E_CONTACT_IS_LIST))
			eab_show_contact_list_editor (book, contact, FALSE, editable);
		else
			eab_show_contact_editor (book, contact, FALSE, editable);
	}
}

void
eab_show_multiple_contacts (EBook *book,
			    GList *list,
			    gboolean editable)
{
	if (list) {
		int length = g_list_length (list);
		if (length > 5) {
			GtkWidget *dialog;
			gint response;

			dialog = gtk_message_dialog_new (NULL,
							 0,
							 GTK_MESSAGE_QUESTION,
							 GTK_BUTTONS_YES_NO,
							 _("Opening %d contacts will open %d new windows as well.\n"
							   "Do you really want to display all of these contacts?"),
							 length,
							 length);

			response = gtk_dialog_run (GTK_DIALOG (dialog));
			gtk_widget_destroy (dialog);
			if (response == GTK_RESPONSE_YES)
				view_contacts (book, list, editable);
		} else {
			view_contacts (book, list, editable);
		}
	}
}


static gint
file_exists(GtkFileSelection *filesel, const char *filename)
{
	GtkWidget *dialog;
	gint response;

	dialog = gtk_message_dialog_new (GTK_WINDOW (filesel),
					 0,
					 GTK_MESSAGE_QUESTION,
					 GTK_BUTTONS_NONE,
					 _("%s already exists\nDo you want to overwrite it?"), filename);

	gtk_dialog_add_buttons (GTK_DIALOG (dialog),
				GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
				_("Overwrite"), GTK_RESPONSE_ACCEPT,
				NULL);

	response = gtk_dialog_run (GTK_DIALOG (dialog));
	gtk_widget_destroy (dialog);
	return response;
}

typedef struct {
	GtkFileSelection *filesel;
	char *vcard;
} SaveAsInfo;

static void
save_it(GtkWidget *widget, SaveAsInfo *info)
{
	gint error = 0;
	gint response = 0;
	
	const char *filename = gtk_file_selection_get_filename (info->filesel);

	error = e_write_file (filename, info->vcard, O_WRONLY | O_CREAT | O_EXCL | O_TRUNC);

	if (error == EEXIST) {
		response = file_exists(info->filesel, filename);
		switch (response) {
			case GTK_RESPONSE_ACCEPT : /* Overwrite */
				e_write_file(filename, info->vcard, O_WRONLY | O_CREAT | O_TRUNC);
				break;
			case GTK_RESPONSE_CANCEL : /* cancel */
				return;
		}
	} else if (error != 0) {
		GtkWidget *dialog;
		char *str;

		str = g_strdup_printf (_("Error saving %s: %s"), filename, strerror(errno));
		dialog = gtk_message_dialog_new (GTK_WINDOW (info->filesel),
						 GTK_DIALOG_DESTROY_WITH_PARENT,
						 GTK_MESSAGE_ERROR,
						 GTK_BUTTONS_OK,
						 str);
		g_free (str);

		gtk_widget_show (dialog);
		
		return;
	}
	
	gtk_widget_destroy(GTK_WIDGET(info->filesel));
}

static void
close_it(GtkWidget *widget, SaveAsInfo *info)
{
	gtk_widget_destroy (GTK_WIDGET (info->filesel));
}

static void
destroy_it(void *data, GObject *where_the_object_was)
{
	SaveAsInfo *info = data;
	g_free (info->vcard);
	g_free (info);
}

static char *
make_safe_filename (const char *prefix, char *name)
{
	char *safe, *p;

	if (!name) {
		/* This is a filename. Translators take note. */
		name = _("card.vcf");
	}

	p = strrchr (name, '/');
	if (p)
		safe = g_strdup_printf ("%s%s%s", prefix, p, ".vcf");
	else
		safe = g_strdup_printf ("%s/%s%s", prefix, name, ".vcf");
	
	p = strrchr (safe, '/') + 1;
	if (p)
		e_filename_make_safe (p);
	
	return safe;
}

static void
source_selection_changed_cb (GtkWidget *selector, GtkWidget *ok_button)
{
	gtk_widget_set_sensitive (ok_button,
				  e_source_selector_peek_primary_selection (E_SOURCE_SELECTOR (selector)) ?
				  TRUE : FALSE);
}

ESource *
eab_select_source (const gchar *title, const gchar *message, const gchar *select_uid, GtkWindow *parent)
{
	ESource *source;
	ESourceList *source_list;
	GtkWidget *dialog;
	GtkWidget *ok_button;
	GtkWidget *cancel_button;
	GtkWidget *label;
	GtkWidget *selector;
	GtkWidget *scrolled_window;
	gint response;

	source_list = addressbook_component_peek_source_list (addressbook_component_peek ());

	dialog = gtk_dialog_new_with_buttons (title, parent,
					      GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
					      NULL);
	gtk_window_set_default_size (GTK_WINDOW (dialog), 200, 350);

	cancel_button = gtk_dialog_add_button (GTK_DIALOG (dialog), GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL);
	ok_button = gtk_dialog_add_button (GTK_DIALOG (dialog), GTK_STOCK_OK, GTK_RESPONSE_ACCEPT);
	gtk_widget_set_sensitive (ok_button, FALSE);

	label = gtk_label_new (message);

	selector = e_source_selector_new (source_list);
	e_source_selector_show_selection (E_SOURCE_SELECTOR (selector), FALSE);
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

	gtk_box_pack_start (GTK_BOX (GTK_DIALOG (dialog)->vbox), label, FALSE, FALSE, 4);
	gtk_box_pack_start (GTK_BOX (GTK_DIALOG (dialog)->vbox), scrolled_window, TRUE, TRUE, 4);

	gtk_widget_show_all (dialog);
	response = gtk_dialog_run (GTK_DIALOG (dialog));

	if (response == GTK_RESPONSE_ACCEPT)
		source = e_source_selector_peek_primary_selection (E_SOURCE_SELECTOR (selector));
	else
		source = NULL;

	gtk_widget_destroy (dialog);
	return source;
}

void
eab_contact_save (char *title, EContact *contact, GtkWindow *parent_window)
{
	GtkFileSelection *filesel;
	char *file;
	char *name;
	SaveAsInfo *info = g_new(SaveAsInfo, 1);

	filesel = GTK_FILE_SELECTION(gtk_file_selection_new(title));

	name = e_contact_get (contact, E_CONTACT_FILE_AS);
	file = make_safe_filename (g_get_home_dir(), name);
	gtk_file_selection_set_filename (filesel, file);
	g_free (file);

	info->filesel = filesel;
	info->vcard = e_vcard_to_string (E_VCARD (contact), EVC_FORMAT_VCARD_30);

	g_signal_connect(filesel->ok_button, "clicked",
			 G_CALLBACK (save_it), info);
	g_signal_connect(filesel->cancel_button, "clicked",
			 G_CALLBACK (close_it), info);
	g_object_weak_ref (G_OBJECT (filesel), destroy_it, info);

	if (parent_window) {
		gtk_window_set_transient_for (GTK_WINDOW (filesel),
					      parent_window);
		gtk_window_set_modal (GTK_WINDOW (filesel), TRUE);
	}

	gtk_widget_show(GTK_WIDGET(filesel));
}

void
eab_contact_list_save (char *title, GList *list, GtkWindow *parent_window)
{
	GtkFileSelection *filesel;
	SaveAsInfo *info = g_new(SaveAsInfo, 1);

	filesel = GTK_FILE_SELECTION(gtk_file_selection_new(title));

	/* This is a filename. Translators take note. */
	if (list && list->data && list->next == NULL) {
		char *name, *file;
		name = e_contact_get (E_CONTACT (list->data), E_CONTACT_FILE_AS);
		if (!name)
			name = e_contact_get (E_CONTACT (list->data), E_CONTACT_FULL_NAME);

		file = make_safe_filename (g_get_home_dir(), name);
		gtk_file_selection_set_filename (filesel, file);
		g_free (file);
	} else {
		char *file;
		file = make_safe_filename (g_get_home_dir(), _("list"));
		gtk_file_selection_set_filename (filesel, file);
		g_free (file);
	}

	info->filesel = filesel;
	info->vcard = eab_contact_list_to_string (list);
	
	g_signal_connect(filesel->ok_button, "clicked",
			 G_CALLBACK (save_it), info);
	g_signal_connect(filesel->cancel_button, "clicked",
			 G_CALLBACK (close_it), info);
	g_object_weak_ref (G_OBJECT (filesel), destroy_it, info);

	if (parent_window) {
		gtk_window_set_transient_for (GTK_WINDOW (filesel),
					      parent_window);
		gtk_window_set_modal (GTK_WINDOW (filesel), TRUE);
	}

	gtk_widget_show(GTK_WIDGET(filesel));
}

typedef struct ContactCopyProcess_ ContactCopyProcess;

typedef void (*ContactCopyDone) (ContactCopyProcess *process);

struct ContactCopyProcess_ {
	int count;
	GList *contacts;
	EBook *source;
	EBook *destination;
	ContactCopyDone done_cb;
};

static void
contact_deleted_cb (EBook* book, EBookStatus status, gpointer user_data)
{
	if (status != E_BOOK_ERROR_OK) {
		eab_error_dialog (_("Error removing contact"), status);
	}
}

static void
do_delete (gpointer data, gpointer user_data)
{
	EBook *book = user_data;
	EContact *contact = data;

	e_book_async_remove_contact(book, contact, contact_deleted_cb, NULL);
}

static void
delete_contacts (ContactCopyProcess *process)
{
	g_list_foreach (process->contacts,
			do_delete,
			process->source);
}

static void
process_unref (ContactCopyProcess *process)
{
	process->count --;
	if (process->count == 0) {
		if (process->done_cb) {
			process->done_cb (process);
		}
		e_free_object_list(process->contacts);
		g_object_unref (process->source);
		g_object_unref (process->destination);
		g_free (process);
	}
}

static void
contact_added_cb (EBook* book, EBookStatus status, const char *id, gpointer user_data)
{
	ContactCopyProcess *process = user_data;

	if (status != E_BOOK_ERROR_OK) {
		eab_error_dialog (_("Error adding contact"), status);
	} else {
		process_unref (process);
	}
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

	process->count ++;
	e_book_async_add_contact(book, contact, contact_added_cb, process);
}

static void
got_book_cb (EBook *book, EBookStatus status, gpointer closure)
{
	ContactCopyProcess *process;
	process = closure;
	if (status == E_BOOK_ERROR_OK) {
		process->destination = book;
		g_object_ref (book);
		g_list_foreach (process->contacts,
				do_copy,
				process);
	}
	process_unref (process);
}

void
eab_transfer_contacts (EBook *source, GList *contacts /* adopted */, gboolean delete_from_source, GtkWindow *parent_window)
{
	EBook *dest;
	ESource *destination_source;
	static char *last_uid = NULL;
	ContactCopyProcess *process;
	char *desc;

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

	destination_source = eab_select_source (desc, _("Select target addressbook."),
						last_uid, parent_window);

	if (!destination_source)
		return;

	if (strcmp (last_uid, e_source_peek_uid (destination_source)) != 0) {
		g_free (last_uid);
		last_uid = g_strdup (e_source_peek_uid (destination_source));
	}

	process = g_new (ContactCopyProcess, 1);
	process->count = 1;
	process->source = source;
	g_object_ref (source);
	process->contacts = contacts;
	process->destination = NULL;

	if (delete_from_source)
		process->done_cb = delete_contacts;
	else
		process->done_cb = NULL;

	dest = e_book_new ();
	e_book_async_load_source (dest, destination_source, got_book_cb, process);
}

#include <Evolution-Composer.h>

#define COMPOSER_OAFID "OAFIID:GNOME_Evolution_Mail_Composer:" BASE_VERSION

void
eab_send_contact_list (GList *contacts, EABDisposition disposition)
{
	GNOME_Evolution_Composer composer_server;
	CORBA_Environment ev;

	if (contacts == NULL)
		return;

	CORBA_exception_init (&ev);
	
	composer_server = bonobo_activation_activate_from_id (COMPOSER_OAFID, 0, NULL, &ev);

	if (disposition == EAB_DISPOSITION_AS_TO) {
		GNOME_Evolution_Composer_RecipientList *to_list, *cc_list, *bcc_list;
		CORBA_char *subject;
		int to_i, bcc_i;
		GList *iter;
		gint to_length = 0, bcc_length = 0;

		/* Figure out how many addresses of each kind we have. */
		for (iter = contacts; iter != NULL; iter = g_list_next (iter)) {
			EContact *contact = E_CONTACT (iter->data);
			GList *emails = e_contact_get (contact, E_CONTACT_EMAIL);
			if (e_contact_get (contact, E_CONTACT_IS_LIST)) {
				gint len = g_list_length (emails);
				if (e_contact_get (contact, E_CONTACT_LIST_SHOW_ADDRESSES))
					to_length += len;
				else
					bcc_length += len;
			} else {
				if (emails != NULL)
					++to_length;
			}
			g_list_foreach (emails, (GFunc)g_free, NULL);
			g_list_free (emails);
		}

		/* Now I have to make a CORBA sequences that represents a recipient list with
		   the right number of entries, for the contacts. */
		to_list = GNOME_Evolution_Composer_RecipientList__alloc ();
		to_list->_maximum = to_length;
		to_list->_length = to_length;
		if (to_length > 0) {
			to_list->_buffer = CORBA_sequence_GNOME_Evolution_Composer_Recipient_allocbuf (to_length);
		}

		cc_list = GNOME_Evolution_Composer_RecipientList__alloc ();
		cc_list->_maximum = cc_list->_length = 0;
		
		bcc_list = GNOME_Evolution_Composer_RecipientList__alloc ();
		bcc_list->_maximum = bcc_length;
		bcc_list->_length = bcc_length;
		if (bcc_length > 0) {
			bcc_list->_buffer = CORBA_sequence_GNOME_Evolution_Composer_Recipient_allocbuf (bcc_length);
		}

		to_i = 0;
		bcc_i = 0;
		while (contacts != NULL) {
			EContact *contact = contacts->data;
			gchar *name, *addr;
			gboolean is_list, is_hidden;
			GNOME_Evolution_Composer_Recipient *recipient;
			GList *emails = e_contact_get (contact, E_CONTACT_EMAIL);
			GList *iterator;

			if (emails != NULL) {

				is_list = (gboolean)e_contact_get (contact, E_CONTACT_IS_LIST);
				is_hidden = is_list && !e_contact_get (contact, E_CONTACT_LIST_SHOW_ADDRESSES);
			
				for (iterator = emails; iterator; iterator = iterator->next) {
					
					if (is_hidden) {
						recipient = &(bcc_list->_buffer[bcc_i]);
						++bcc_i;
					} else {
						recipient = &(to_list->_buffer[to_i]);
						++to_i;
					}
					
					name = NULL;
					addr = NULL;
					if (iterator && iterator->data) {
						if (is_list) {
							/* XXX we should probably try to get the name from the attribute parameter here.. */
							addr = g_strdup ((char*)iterator->data);
						} else { /* is just a plain old card */
							EContactName *contact_name = e_contact_get (contact, E_CONTACT_NAME);
							if (contact_name) {
								name = e_contact_name_to_string (contact_name);
								e_contact_name_free (contact_name);
							}
							addr = g_strdup ((char *) iterator->data);
						}
					}
					
					recipient->name    = CORBA_string_dup (name ? name : "");
					recipient->address = CORBA_string_dup (addr ? addr : "");
					
					g_free ((gchar *) name);
					g_free ((gchar *) addr);
					
					/* If this isn't a list, we quit after the first (i.e. the default) address. */
					if (!is_list)
						break;
					
				}
				g_list_foreach (emails, (GFunc)g_free, NULL);
				g_list_free (emails);
			}

			contacts = g_list_next (contacts);
		}

		subject = CORBA_string_dup ("");

		GNOME_Evolution_Composer_setHeaders (composer_server, "", to_list, cc_list, bcc_list, subject, &ev);
		if (ev._major != CORBA_NO_EXCEPTION) {
			g_printerr ("gui/e-meeting-edit.c: I couldn't set the composer headers via CORBA! Aagh.\n");
			CORBA_exception_free (&ev);
			return;
		}

		CORBA_free (to_list);
		CORBA_free (cc_list);
		CORBA_free (bcc_list);
		CORBA_free (subject);
	} else if (disposition == EAB_DISPOSITION_AS_ATTACHMENT) {
		CORBA_char *content_type, *filename, *description;
		GNOME_Evolution_Composer_AttachmentData *attach_data;
		CORBA_boolean show_inline;
		char *tempstr;

		GNOME_Evolution_Composer_RecipientList *to_list, *cc_list, *bcc_list;
		CORBA_char *subject;
		
		content_type = CORBA_string_dup ("text/x-vcard");
		filename = CORBA_string_dup ("");

		if (contacts->next) {
			description = CORBA_string_dup (_("Multiple VCards"));
		} else {
			char *file_as = e_contact_get (E_CONTACT (contacts->data), E_CONTACT_FILE_AS);
			tempstr = g_strdup_printf (_("VCard for %s"), file_as);
			description = CORBA_string_dup (tempstr);
			g_free (tempstr);
			g_free (file_as);
		}

		show_inline = FALSE;

		tempstr = eab_contact_list_to_string (contacts);
		attach_data = GNOME_Evolution_Composer_AttachmentData__alloc();
		attach_data->_maximum = attach_data->_length = strlen (tempstr);
		attach_data->_buffer = CORBA_sequence_CORBA_char_allocbuf (attach_data->_length);
		strcpy (attach_data->_buffer, tempstr);
		g_free (tempstr);

		GNOME_Evolution_Composer_attachData (composer_server, 
						     content_type, filename, description,
						     show_inline, attach_data,
						     &ev);
	
		if (ev._major != CORBA_NO_EXCEPTION) {
			g_printerr ("gui/e-meeting-edit.c: I couldn't attach data to the composer via CORBA! Aagh.\n");
			CORBA_exception_free (&ev);
			return;
		}
	
		CORBA_free (content_type);
		CORBA_free (filename);
		CORBA_free (description);
		CORBA_free (attach_data);

		to_list = GNOME_Evolution_Composer_RecipientList__alloc ();
		to_list->_maximum = to_list->_length = 0;
		
		cc_list = GNOME_Evolution_Composer_RecipientList__alloc ();
		cc_list->_maximum = cc_list->_length = 0;

		bcc_list = GNOME_Evolution_Composer_RecipientList__alloc ();
		bcc_list->_maximum = bcc_list->_length = 0;

		if (!contacts || contacts->next) {
			subject = CORBA_string_dup ("Contact information");
		} else {
			EContact *contact = contacts->data;
			const gchar *tempstr2;

			tempstr2 = e_contact_get_const (contact, E_CONTACT_FILE_AS);
			if (!tempstr2 || !*tempstr2)
				tempstr2 = e_contact_get_const (contact, E_CONTACT_FULL_NAME);
			if (!tempstr2 || !*tempstr2)
				tempstr2 = e_contact_get_const (contact, E_CONTACT_ORG);
			if (!tempstr2 || !*tempstr2)
				tempstr2 = e_contact_get_const (contact, E_CONTACT_EMAIL_1);
			if (!tempstr2 || !*tempstr2)
				tempstr2 = e_contact_get_const (contact, E_CONTACT_EMAIL_2);
			if (!tempstr2 || !*tempstr2)
				tempstr2 = e_contact_get_const (contact, E_CONTACT_EMAIL_3);

			if (!tempstr2 || !*tempstr2)
				tempstr = g_strdup_printf ("Contact information");
			else
				tempstr = g_strdup_printf ("Contact information for %s", tempstr2);
			subject = CORBA_string_dup (tempstr);
			g_free (tempstr);
		}
		
		GNOME_Evolution_Composer_setHeaders (composer_server, "", to_list, cc_list, bcc_list, subject, &ev);

		CORBA_free (to_list);
		CORBA_free (cc_list);
		CORBA_free (bcc_list);
		CORBA_free (subject);
	}

	GNOME_Evolution_Composer_show (composer_server, &ev);

	if (ev._major != CORBA_NO_EXCEPTION) {
		g_printerr ("gui/e-meeting-edit.c: I couldn't show the composer via CORBA! Aagh.\n");
		CORBA_exception_free (&ev);
		return;
	}

	CORBA_exception_free (&ev);
}

void
eab_send_contact (EContact *contact, EABDisposition disposition)
{
	GList *list;
	list = g_list_prepend (NULL, contact);
	eab_send_contact_list (list, disposition);
	g_list_free (list);
}

GtkWidget *
eab_create_image_chooser_widget(gchar *name,
				gchar *string1, gchar *string2,
				gint int1, gint int2)
{
	char *filename;
	GtkWidget *w = NULL;

	w = e_image_chooser_new ();
	gtk_widget_show_all (w);

	if (string1) {
		filename = e_icon_factory_get_icon_filename (string1, 48);

		e_image_chooser_set_from_file (E_IMAGE_CHOOSER (w), filename);

		g_free (filename);
	}

	return w;
}
