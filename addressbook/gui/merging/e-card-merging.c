/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Code for checking for duplicates when doing ECard work.
 *
 * Author:
 *   Christopher James Lahey <clahey@ximian.com>
 *
 * Copyright 2001, Ximian, Inc.
 */

#include <config.h>

#include "e-card-merging.h"
#include <libgnomeui/gnome-dialog.h>
#include "e-card-compare.h"
#include <glade/glade.h>
#include <gtk/gtksignal.h>
#include "addressbook/gui/widgets/e-minicard-widget.h"

typedef enum {
	E_CARD_MERGING_ADD,
	E_CARD_MERGING_COMMIT
} ECardMergingOpType;

typedef struct {
	ECardMergingOpType op;
	EBook *book;
	ECard *card;
	EBookIdCallback id_cb;
	EBookCallback   cb;
	gpointer closure;
} ECardMergingLookup;

static void
doit (ECardMergingLookup *lookup)
{
	if (lookup->op == E_CARD_MERGING_ADD)
		e_book_add_card (lookup->book, lookup->card, lookup->id_cb, lookup->closure);
	else if (lookup->op == E_CARD_MERGING_COMMIT)
		e_book_commit_card (lookup->book, lookup->card, lookup->cb, lookup->closure);
}

static void
cancelit (ECardMergingLookup *lookup)
{
	if (lookup->op == E_CARD_MERGING_ADD) {
		if (lookup->id_cb)
			lookup->id_cb (lookup->book, E_BOOK_STATUS_CANCELLED, NULL, lookup->closure);
	} else if (lookup->op == E_CARD_MERGING_COMMIT) {
		if (lookup->cb)
			lookup->cb (lookup->book, E_BOOK_STATUS_CANCELLED, lookup->closure);
	}
}

static void
clicked (GnomeDialog *dialog, int button, ECardMergingLookup *lookup)
{
	switch (button) {
	case 0:
		doit (lookup);
		break;
	case 1:
		cancelit (lookup);
		break;
	}
	g_free (lookup);
	gnome_dialog_close (dialog);
}

static void
match_query_callback (ECard *card, ECard *match, ECardMatchType type, gpointer closure)
{
	ECardMergingLookup *lookup = closure;
	if (type == E_CARD_MATCH_NONE) {
		doit (lookup);
		g_free (lookup);
	} else {
		GladeXML *ui;
		
		GtkWidget *widget;

		if (lookup->op == E_CARD_MERGING_ADD)
			ui = glade_xml_new (EVOLUTION_GLADEDIR "/e-card-duplicate-detected.glade", NULL);
		else if (lookup->op == E_CARD_MERGING_COMMIT)
			ui = glade_xml_new (EVOLUTION_GLADEDIR "/e-card-merging-book-commit-duplicate-detected.glade", NULL);
		else {
			doit (lookup);
			g_free (lookup);
			return;
		}

		widget = glade_xml_get_widget (ui, "custom-old-card");
		gtk_object_set (GTK_OBJECT (widget),
				"card", match,
				NULL);

		widget = glade_xml_get_widget (ui, "custom-new-card");
		gtk_object_set (GTK_OBJECT (widget),
				"card", card,
				NULL);

		widget = glade_xml_get_widget (ui, "dialog-duplicate-contact");

		gtk_signal_connect (GTK_OBJECT (widget), "clicked",
				    GTK_SIGNAL_FUNC (clicked), lookup);
	}
}

gboolean
e_card_merging_book_add_card (EBook           *book,
			      ECard           *card,
			      EBookIdCallback  cb,
			      gpointer         closure)
{
	ECardMergingLookup *lookup;
	lookup = g_new (ECardMergingLookup, 1);

	lookup->op = E_CARD_MERGING_ADD;
	lookup->book = book;
	lookup->card = card;
	lookup->id_cb = cb;
	lookup->closure = closure;

	e_card_locate_match_full (book, card, NULL, match_query_callback, lookup);
	return TRUE;
}

gboolean
e_card_merging_book_commit_card (EBook                 *book,
				 ECard                 *card,
				 EBookCallback          cb,
				 gpointer               closure)
{
	ECardMergingLookup *lookup;
	GList *avoid;
	lookup = g_new (ECardMergingLookup, 1);

	lookup->op = E_CARD_MERGING_COMMIT;
	lookup->book = book;
	lookup->card = card;
	lookup->cb = cb;
	lookup->closure = closure;

	avoid = g_list_append (NULL, card);

	e_card_locate_match_full (book, card, avoid, match_query_callback, lookup);

	g_list_free (avoid);

	return TRUE;
}

GtkWidget *
e_card_merging_create_old_card(gchar *name,
			       gchar *string1, gchar *string2,
			       gint int1, gint int2);

GtkWidget *
e_card_merging_create_old_card(gchar *name,
			     gchar *string1, gchar *string2,
			     gint int1, gint int2)
{
	return e_minicard_widget_new ();
}
