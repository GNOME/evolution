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
#include "e-addressbook-util.h"

#include <gnome.h>

#include "e-card-merging.h"
#include <shell/evolution-shell-client.h>
#include <addressbook/backend/ebook/e-book-util.h>

void
e_addressbook_error_dialog (const gchar *msg, EBookStatus status)
{
	static char *status_to_string[] = {
		N_("Success"),
		N_("Unknown error"),
		N_("Repository offline"),
		N_("Permission denied"),
		N_("Card not found"),
		N_("Card ID already exists"),
		N_("Protocol not supported"),
		N_("Cancelled"),
		N_("Other error")
	};
	char *error_msg;

	error_msg = g_strdup_printf ("%s: %s", msg, _(status_to_string [status]));

	gtk_widget_show (gnome_error_dialog (error_msg));

	g_free (error_msg);
}

gint
e_addressbook_prompt_save_dialog (GtkWindow *parent)
{
	GtkWidget *dialog;

	dialog = gnome_message_box_new (_("Do you want to save changes?"),
					GNOME_MESSAGE_BOX_QUESTION,
					GNOME_STOCK_BUTTON_YES,
					GNOME_STOCK_BUTTON_NO,
					GNOME_STOCK_BUTTON_CANCEL,
					NULL);

	gnome_dialog_set_default (GNOME_DIALOG (dialog), 0);
	gnome_dialog_grab_focus (GNOME_DIALOG (dialog), 0);
	gnome_dialog_set_parent (GNOME_DIALOG (dialog), parent);

	return gnome_dialog_run_and_close (GNOME_DIALOG (dialog));
}

static void
added_cb (EBook* book, EBookStatus status, const char *id,
	  gboolean is_list)
{
	if (status != E_BOOK_STATUS_SUCCESS) {
		e_addressbook_error_dialog (is_list ? _("Error adding list") : _("Error adding card"), status);
	}
}

static void
modified_cb (EBook* book, EBookStatus status,
	     gboolean is_list)
{
	if (status != E_BOOK_STATUS_SUCCESS) {
		e_addressbook_error_dialog (is_list ? _("Error modifying list") : _("Error modifying card"),
					    status);
	}
}

static void
deleted_cb (EBook* book, EBookStatus status,
	    gboolean is_list)
{
	if (status != E_BOOK_STATUS_SUCCESS) {
		e_addressbook_error_dialog (is_list ? _("Error removing list") : _("Error removing card"),
					    status);
	}
}

static void
editor_closed_cb (GtkObject *editor, gpointer data)
{
	gtk_object_unref (editor);
}

EContactEditor *
e_addressbook_show_contact_editor (EBook *book, ECard *card,
				   gboolean is_new_card,
				   gboolean editable)
{
	EContactEditor *ce;

	ce = e_contact_editor_new (book, card, is_new_card, editable);

	gtk_signal_connect (GTK_OBJECT (ce), "card_added",
			    GTK_SIGNAL_FUNC (added_cb), GINT_TO_POINTER (FALSE));
	gtk_signal_connect (GTK_OBJECT (ce), "card_modified",
			    GTK_SIGNAL_FUNC (modified_cb), GINT_TO_POINTER (FALSE));
	gtk_signal_connect (GTK_OBJECT (ce), "card_deleted",
			    GTK_SIGNAL_FUNC (deleted_cb), GINT_TO_POINTER (FALSE));
	gtk_signal_connect (GTK_OBJECT (ce), "editor_closed",
			    GTK_SIGNAL_FUNC (editor_closed_cb), NULL);

	return ce;
}

EContactListEditor *
e_addressbook_show_contact_list_editor (EBook *book, ECard *card,
					gboolean is_new_card,
					gboolean editable)
{
	EContactListEditor *ce;

	ce = e_contact_list_editor_new (book, card, is_new_card, editable);

	gtk_signal_connect (GTK_OBJECT (ce), "list_added",
			    GTK_SIGNAL_FUNC (added_cb), GINT_TO_POINTER (TRUE));
	gtk_signal_connect (GTK_OBJECT (ce), "list_modified",
			    GTK_SIGNAL_FUNC (modified_cb), GINT_TO_POINTER (TRUE));
	gtk_signal_connect (GTK_OBJECT (ce), "list_deleted",
			    GTK_SIGNAL_FUNC (deleted_cb), GINT_TO_POINTER (TRUE));
	gtk_signal_connect (GTK_OBJECT (ce), "editor_closed",
			    GTK_SIGNAL_FUNC (editor_closed_cb), GINT_TO_POINTER (TRUE));

	e_contact_list_editor_show (ce);

	return ce;
}

typedef struct {
	EBook *book;
	GList *list;
	gboolean editable;
} BookAndList;

static void
view_cards (EBook *book, GList *list, gboolean editable)
{
	for (; list; list = list->next) {
		ECard *card = list->data;
		if (e_card_evolution_list (card))
			e_addressbook_show_contact_list_editor (book, card, FALSE, editable);
		else
			e_addressbook_show_contact_editor (book, card, FALSE, editable);
	}
}

static void
view_question_clicked (GtkObject *object, int button, BookAndList *bnl)
{
	GnomeDialog *dialog = GNOME_DIALOG (object);
	switch (button) {
	case 0:
		view_cards (bnl->book, bnl->list, bnl->editable);
		break;
	}
	gnome_dialog_close(dialog);
}

static void
view_question_destroyed (GtkObject *object, GList *list)
{
	gtk_main_quit();
}

void
e_addressbook_show_multiple_cards (EBook *book,
				   GList *list,
				   gboolean editable)
{
	if (list) {
		int length = g_list_length (list);
		if (length > 5) {
			char *string;
			GtkWidget *dialog;
			BookAndList bnl;

			bnl.book = book;
			bnl.list = list;
			bnl.editable = editable;

			dialog = gnome_dialog_new (_("Display Cards?"),
						   _("Display Cards"),
						   GNOME_STOCK_BUTTON_CANCEL,
						   NULL);

			string = g_strdup_printf (_("You have requested that %d cards be cards. This will cause %d new windows to be\n"
						    "displayed on your screen. Do you really want to display all of these cards?"), length, length);
			gtk_box_pack_start (GTK_BOX (GNOME_DIALOG (dialog)->vbox), gtk_label_new (string), FALSE, FALSE, 0);
			g_free (string);

			gtk_signal_connect (GTK_OBJECT (dialog), "destroy",
					    GTK_SIGNAL_FUNC (view_question_destroyed), &bnl);
			gtk_signal_connect (GTK_OBJECT (dialog), "clicked",
					    GTK_SIGNAL_FUNC (view_question_clicked), &bnl);

			gtk_widget_show_all (dialog);

			gtk_main();
		} else {
			view_cards (book, list, editable);
		}
	}
}



typedef struct CardCopyProcess_ CardCopyProcess;

typedef void (*CardCopyDone) (CardCopyProcess *process);

struct CardCopyProcess_ {
	int count;
	GList *cards;
	EBook *source;
	EBook *destination;
	CardCopyDone done_cb;
};

static void
card_deleted_cb (EBook* book, EBookStatus status, gpointer user_data)
{
	if (status != E_BOOK_STATUS_SUCCESS) {
		e_addressbook_error_dialog (_("Error removing card"), status);
	}
}

static void
do_delete (gpointer data, gpointer user_data)
{
	EBook *book = user_data;
	ECard *card = data;

	e_book_remove_card(book, card, card_deleted_cb, NULL);
}

static void
delete_cards (CardCopyProcess *process)
{
	g_list_foreach (process->cards,
			do_delete,
			process->source);
}

static void
process_unref (CardCopyProcess *process)
{
	process->count --;
	if (process->count == 0) {
		if (process->done_cb) {
			process->done_cb (process);
		}
		e_free_object_list(process->cards);
		gtk_object_unref (GTK_OBJECT (process->source));
		gtk_object_unref (GTK_OBJECT (process->destination));
		g_free (process);
	}
}

static void
card_added_cb (EBook* book, EBookStatus status, const char *id, gpointer user_data)
{
	CardCopyProcess *process = user_data;

	if (status != E_BOOK_STATUS_SUCCESS) {
		e_addressbook_error_dialog (_("Error adding card"), status);
	} else {
		process_unref (process);
	}
}

static void
do_copy (gpointer data, gpointer user_data)
{
	EBook *book;
	ECard *card;
	CardCopyProcess *process;

	process = user_data;
	card = data;

	book = process->destination;

	process->count ++;
	e_book_add_card(book, card, card_added_cb, process);
}

static void
got_book_cb (EBook *book, gpointer closure)
{
	CardCopyProcess *process;
	process = closure;
	if (book) {
		process->destination = book;
		gtk_object_ref (GTK_OBJECT (book));
		g_list_foreach (process->cards,
				do_copy,
				process);
	}
	process_unref (process);
}

void
e_addressbook_transfer_cards (EBook *source, GList *cards /* adopted */, gboolean delete_from_source, GtkWindow *parent_window)
{
	const char *allowed_types[] = { "contacts", NULL };
	extern EvolutionShellClient *global_shell_client;
	GNOME_Evolution_Folder *folder;
	static char *last_uri = NULL;
	CardCopyProcess *process;
	char *desc;

	if (cards == NULL)
		return;

	if (last_uri == NULL)
		last_uri = g_strdup ("");

	if (cards->next == NULL) {
		if (delete_from_source)
			desc = _("Move card to");
		else
			desc = _("Copy card to");
	} else {
		if (delete_from_source)
			desc = _("Move cards to");
		else
			desc = _("Copy cards to");
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

	process = g_new (CardCopyProcess, 1);
	process->count = 1;
	process->source = source;
	gtk_object_ref (GTK_OBJECT (source));
	process->cards = cards;
	process->destination = NULL;

	if (delete_from_source)
		process->done_cb = delete_cards;
	else
		process->done_cb = NULL;

	e_book_use_address_book_by_uri (folder->physicalUri, got_book_cb, process);

	CORBA_free (folder);
}
