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
#include <ebook/e-card-compare.h>
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
free_lookup (ECardMergingLookup *lookup)
{
	g_object_unref (lookup->book);
	g_object_unref (lookup->card);

	g_free (lookup);
}

static void
final_id_cb (EBook *book, EBookStatus status, const char *id, gpointer closure)
{
	ECardMergingLookup *lookup = closure;

	if (lookup->id_cb)
		lookup->id_cb (lookup->book, status, id, lookup->closure);

	free_lookup (lookup);
}

static void
final_cb (EBook *book, EBookStatus status, gpointer closure)
{
	ECardMergingLookup *lookup = closure;

	if (lookup->cb)
		lookup->cb (lookup->book, status, lookup->closure);

	free_lookup (lookup);
}

static void
doit (ECardMergingLookup *lookup)
{
	if (lookup->op == E_CARD_MERGING_ADD)
		e_book_add_card (lookup->book, lookup->card, final_id_cb, lookup);
	else if (lookup->op == E_CARD_MERGING_COMMIT)
		e_book_commit_card (lookup->book, lookup->card, final_cb, lookup);
}

static void
cancelit (ECardMergingLookup *lookup)
{
	if (lookup->op == E_CARD_MERGING_ADD) {
		if (lookup->id_cb)
			final_id_cb (lookup->book, E_BOOK_STATUS_CANCELLED, NULL, lookup);
	} else if (lookup->op == E_CARD_MERGING_COMMIT) {
		if (lookup->cb)
			final_cb (lookup->book, E_BOOK_STATUS_CANCELLED, lookup);
	}
}

static void
response (GtkWidget *dialog, int response, ECardMergingLookup *lookup)
{
	gtk_widget_destroy (dialog);

	switch (response) {
	case 0:
		doit (lookup);
		break;
	case 1:
		cancelit (lookup);
		break;
	}
}

static void
match_query_callback (ECard *card, ECard *match, ECardMatchType type, gpointer closure)
{
	ECardMergingLookup *lookup = closure;

	if ((gint) type <= (gint) E_CARD_MATCH_VAGUE) {
		doit (lookup);
	} else {
		GladeXML *ui;
		
		GtkWidget *widget;

		if (lookup->op == E_CARD_MERGING_ADD)
			ui = glade_xml_new (EVOLUTION_GLADEDIR "/e-card-duplicate-detected.glade", NULL, NULL);
		else if (lookup->op == E_CARD_MERGING_COMMIT)
			ui = glade_xml_new (EVOLUTION_GLADEDIR "/e-card-merging-book-commit-duplicate-detected.glade", NULL, NULL);
		else {
			doit (lookup);
			return;
		}

		widget = glade_xml_get_widget (ui, "custom-old-card");
		g_object_set (widget,
			      "card", match,
			      NULL);

		widget = glade_xml_get_widget (ui, "custom-new-card");
		g_object_set (widget,
			      "card", card,
			      NULL);

		widget = glade_xml_get_widget (ui, "dialog-duplicate-contact");

		g_signal_connect (widget, "response",
				  G_CALLBACK (response), lookup);

		gtk_widget_show_all (widget);
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
	lookup->book = g_object_ref (book);
	lookup->card = g_object_ref (card);
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
	lookup->book = g_object_ref (book);
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
