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
#include "util/eab-destination.h"

#include <gnome.h>

#include "addressbook/gui/contact-editor/e-contact-editor.h"
#include "addressbook/gui/contact-list-editor/e-contact-list-editor.h"

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
		N_("Addressbook does not exist"),
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

	g_signal_connect (ce, "list_added",
			  G_CALLBACK (added_cb), GINT_TO_POINTER (TRUE));
	g_signal_connect (ce, "list_modified",
			  G_CALLBACK (modified_cb), GINT_TO_POINTER (TRUE));
	g_signal_connect (ce, "list_deleted",
			  G_CALLBACK (deleted_cb), GINT_TO_POINTER (TRUE));
	g_signal_connect (ce, "editor_closed",
			  G_CALLBACK (editor_closed_cb), GINT_TO_POINTER (TRUE));

	e_contact_list_editor_show (ce);

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
				GTK_STOCK_CANCEL, GTK_RESPONSE_REJECT,
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
			case GTK_RESPONSE_REJECT : /* cancel */
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
#if 0
	EBook *dest;
	const char *allowed_types[] = { "contacts/*", NULL };
	GNOME_Evolution_Folder *folder;
	static char *last_uri = NULL;
	ContactCopyProcess *process;
	char *desc;

	if (contacts == NULL)
		return;

	if (last_uri == NULL)
		last_uri = g_strdup ("");

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

	evolution_shell_client_user_select_folder (global_shell_client,
						   parent_window,
						   desc, last_uri, allowed_types,
						   &folder);

	if (!folder)
		return;

	if (strcmp (last_uri, folder->evolutionUri) != 0) {
		g_free (last_uri);
		last_uri = g_strdup (folder->evolutionUri);
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
	e_book_async_load_uri (dest, folder->physicalUri, got_book_cb, process);

	CORBA_free (folder);
#endif
}

#include <Evolution-Composer.h>

#define COMPOSER_OAFID "OAFIID:GNOME_Evolution_Mail_Composer:" BASE_VERSION

void
eab_send_contact_list (GList *contacts, EABDisposition disposition)
{
#if notyet
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
			if (e_contact_get (contact, E_CONTACT_IS_LIST)) {
				gint len = card->email ? e_list_length (card->email) : 0;
				if (e_card_evolution_list_show_addresses (card))
					to_length += len;
				else
					bcc_length += len;
			} else {
				if (card->email != NULL)
					++to_length;
			}
		}

		/* Now I have to make a CORBA sequences that represents a recipient list with
		   the right number of entries, for the cards. */
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
		while (cards != NULL) {
			ECard *card = cards->data;
			EIterator *iterator;
			gchar *name, *addr;
			gboolean is_list, is_hidden, free_name_addr;
			GNOME_Evolution_Composer_Recipient *recipient;

			if (card->email != NULL) {

				is_list = e_card_evolution_list (card);
				is_hidden = is_list && !e_card_evolution_list_show_addresses (card);
			
				for (iterator = e_list_get_iterator (card->email); e_iterator_is_valid (iterator); e_iterator_next (iterator)) {
					
					if (is_hidden) {
						recipient = &(bcc_list->_buffer[bcc_i]);
						++bcc_i;
					} else {
						recipient = &(to_list->_buffer[to_i]);
						++to_i;
					}
					
					name = "";
					addr = "";
					free_name_addr = FALSE;
					if (e_iterator_is_valid (iterator)) {
						
						if (is_list) {
							/* We need to decode the list entries, which are XMLified EABDestinations. */
							EABDestination *dest = eab_destination_import (e_iterator_get (iterator));
							if (dest != NULL) {
								name = g_strdup (eab_destination_get_name (dest));
								addr = g_strdup (eab_destination_get_email (dest));
								free_name_addr = TRUE;
								g_object_unref (dest);
							}
							
						} else { /* is just a plain old card */
							if (card->name)
								name = e_card_name_to_string (card->name);
							addr = g_strdup ((char *) e_iterator_get (iterator));
							free_name_addr = TRUE;
						}
					}
					
					recipient->name    = CORBA_string_dup (name ? name : "");
					recipient->address = CORBA_string_dup (addr ? addr : "");
					
					if (free_name_addr) {
						g_free ((gchar *) name);
						g_free ((gchar *) addr);
					}
					
					/* If this isn't a list, we quit after the first (i.e. the default) address. */
					if (!is_list)
						break;
					
				}
				g_object_unref (iterator);
			}

			cards = g_list_next (cards);
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

		if (cards->next) {
			description = CORBA_string_dup (_("Multiple VCards"));
		} else {
			char *file_as;

			g_object_get(cards->data,
				     "file_as", &file_as,
				     NULL);

			tempstr = g_strdup_printf (_("VCard for %s"), file_as);
			description = CORBA_string_dup (tempstr);
			g_free (tempstr);
		}

		show_inline = FALSE;

		tempstr = eab_contact_list_to_string (cards);
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

		if (!cards || cards->next) {
			subject = CORBA_string_dup ("Contact information");
		} else {
			ECard *card = cards->data;
			const gchar *tempstr2;

			tempstr2 = NULL;
			g_object_get(card,
				     "file_as", &tempstr2,
				     NULL);
			if (!tempstr2 || !*tempstr2)
				g_object_get(card,
					     "full_name", &tempstr2,
					     NULL);
			if (!tempstr2 || !*tempstr2)
				g_object_get(card,
					     "org", &tempstr2,
					     NULL);
			if (!tempstr2 || !*tempstr2) {
				EList *list;
				EIterator *iterator;
				g_object_get(card,
					     "email", &list,
					     NULL);
				iterator = e_list_get_iterator (list);
				if (e_iterator_is_valid (iterator)) {
					tempstr2 = e_iterator_get (iterator);
				}
				g_object_unref (iterator);
			}

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
#endif
}

void
eab_send_contact (EContact *contact, EABDisposition disposition)
{
	GList *list;
	list = g_list_prepend (NULL, contact);
	eab_send_contact_list (list, disposition);
	g_list_free (list);
}
