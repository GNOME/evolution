/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * An auto-completer for addresses from the address book.
 *
 * Author:
 *   Jon Trowbridge <trow@ximian.com>
 *
 * Copyright (C) 2001 Ximian, Inc.
 */

/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307
 * USA
 */

#include <config.h>
#include "e-address-completion.h"

static void e_address_completion_class_init (EAddressCompletionClass *klass);
static void e_address_completion_init       (EAddressCompletion *addr_comp);
static void e_address_completion_destroy    (GtkObject *object);

static GtkObjectClass *parent_class;

typedef struct _BookQuery BookQuery;
struct _BookQuery {
	EAddressCompletion *completion;
	guint seq_no;

	gchar *text;

	EBookView *book_view;
	guint card_added_id;
	guint sequence_complete_id;
};

static BookQuery *book_query_new                  (EAddressCompletion *, const gchar *query_text);
static void       book_query_attach_book_view     (BookQuery *, EBookView *);
static void       book_query_free                 (BookQuery *);
static gboolean   book_query_has_expired          (BookQuery *);
static double     book_query_score_e_card         (BookQuery *, ECard *);
static void       book_query_book_view_cb         (EBook *, EBookStatus, EBookView *, gpointer book_query);
static void       book_query_card_added_cb        (EBookView *, const GList *cards, gpointer book_query);
static void       book_query_sequence_complete_cb (EBookView *, gpointer book_query);



GtkType
e_address_completion_get_type (void)
{
	static GtkType address_completion_type = 0;

	if (!address_completion_type) {
		GtkTypeInfo address_completion_info = {
			"EAddressCompletion",
			sizeof (EAddressCompletion),
			sizeof (EAddressCompletionClass),
			(GtkClassInitFunc)  e_address_completion_class_init,
			(GtkObjectInitFunc) e_address_completion_init,
			NULL, NULL,
			(GtkClassInitFunc) NULL
		};
		address_completion_type = gtk_type_unique (e_completion_get_type (),
							   &address_completion_info);
	}

	return address_completion_type;
}

static void
e_address_completion_class_init (EAddressCompletionClass *klass)
{
	GtkObjectClass *object_class = (GtkObjectClass *) klass;

	parent_class = GTK_OBJECT_CLASS (gtk_type_class (e_completion_get_type ()));

	object_class->destroy = e_address_completion_destroy;
}

static void
e_address_completion_init (EAddressCompletion *addr_comp)
{

}

static void
e_address_completion_destroy (GtkObject *object)
{
	EAddressCompletion *addr_comp = E_ADDRESS_COMPLETION (object);

	gtk_object_unref (GTK_OBJECT (addr_comp->book));
}

static BookQuery *
book_query_new (EAddressCompletion *comp, const gchar *text)
{
	BookQuery *q = g_new0 (BookQuery, 1);

	q->completion = comp;
	gtk_object_ref (GTK_OBJECT (comp));

	q->seq_no = comp->seq_no;

	q->text = g_strdup (text);

	return q;
}

static void
book_query_attach_book_view (BookQuery *q, EBookView *book_view)
{
	g_return_if_fail (q);

	g_assert (q->book_view == NULL);
	q->book_view = book_view;
	gtk_object_ref (GTK_OBJECT (book_view));

	q->card_added_id = gtk_signal_connect (GTK_OBJECT (book_view),
					       "card_added",
					       GTK_SIGNAL_FUNC (book_query_card_added_cb),
					       q);
	q->sequence_complete_id = gtk_signal_connect (GTK_OBJECT (book_view),
						      "sequence_complete",
						      GTK_SIGNAL_FUNC (book_query_sequence_complete_cb),
						      q);
}

static void
book_query_free (BookQuery *q)
{
	if (q) {
		if (q->book_view) {
			gtk_signal_disconnect (GTK_OBJECT (q->book_view), q->card_added_id);
			gtk_signal_disconnect (GTK_OBJECT (q->book_view), q->sequence_complete_id);
			gtk_object_unref (GTK_OBJECT (q->book_view));
		}

		gtk_object_unref (GTK_OBJECT (q->completion));

		g_free (q->text);

		g_free (q);
	}
}

static gboolean
book_query_has_expired (BookQuery *q)
{
	g_return_val_if_fail (q != NULL, FALSE);
	return q->seq_no != q->completion->seq_no;
}

static double
book_query_score_e_card (BookQuery *q, ECard *card)
{
	gint len;

	g_return_val_if_fail (q != NULL, -1);
	g_return_val_if_fail (card != NULL && E_IS_CARD (card), -1);

	len = strlen (q->text);

	if (card->name->given && !g_strncasecmp (card->name->given, q->text, len))
		return len;

	if (card->name->additional && !g_strncasecmp (card->name->additional, q->text, len))
		return len;

	if (card->name->family && !g_strncasecmp (card->name->family, q->text, len))
		return len;
 
	return 0.5; /* Not good, but we'll leave them in anyway for now... */
}

static gchar*
book_query_card_text (ECard *card)
{
	if (card->name) {
		/* Sort of a lame hack. */
		return g_strdup_printf ("%s %s%s%s",
					card->name->given ? card->name->given : "",
					card->name->additional ? card->name->additional : "",
					card->name->additional ? " " : "",
					card->name->family ? card->name->family : "");
	} else if (card->fname) {
		return g_strdup (card->fname);
	} else if (card->file_as) {
		return g_strdup (card->file_as);
	} 
	
	return NULL;
}

static void
book_query_book_view_cb (EBook *book, EBookStatus status, EBookView *book_view, gpointer user_data)
{
	BookQuery *q = (BookQuery *) user_data;

	if (book_query_has_expired (q)) {
		book_query_free (q);
		return;
	}

	book_query_attach_book_view (q, book_view);
}

static void
book_query_card_added_cb (EBookView *book_view, const GList *cards, gpointer user_data)
{
	BookQuery *q = (BookQuery *) user_data;

	if (book_query_has_expired (q)) {
		book_query_free (q);
		return;
	}

	while (cards) {
		ECard *card = (ECard *) cards->data;
		double score;

		if (card && (score = book_query_score_e_card (q, card)) >= 0) {
			gchar *str = book_query_card_text (card);
			
			if (str) {
				gtk_object_ref (GTK_OBJECT (card));
				e_completion_found_match_full (E_COMPLETION (q->completion), str, score,
							       card, (GtkDestroyNotify)gtk_object_unref);
			}
		}
		
		cards = g_list_next (cards);
	}
}

static void
book_query_sequence_complete_cb (EBookView *book_view, gpointer user_data)
{
	BookQuery *q = (BookQuery *) user_data;

	if (! book_query_has_expired (q)) {
		e_completion_end_search (E_COMPLETION (q->completion));
	}

	book_query_free (q);
}

static void
e_address_completion_begin (ECompletion *comp, const gchar *text, gint pos, gint limit, gpointer user_data)
{
	EAddressCompletion *addr_comp = E_ADDRESS_COMPLETION (comp);
	BookQuery *q;
	gchar *query;

	++addr_comp->seq_no; /* Paranoia, in case completion_begin were to be called twice in a row
				without an intervening completion_end.  (Of course, this shouldn't
				happen...) */
	q = book_query_new (addr_comp, text);

	query = g_strdup_printf ("(contains \"x-evolution-any-field\" \"%s\")", text);
	e_book_get_book_view (addr_comp->book, query, book_query_book_view_cb, q);
	g_free (query);
}

static void
e_address_completion_end (ECompletion *comp, gboolean finished, gpointer user_data)
{
	EAddressCompletion *addr_comp = E_ADDRESS_COMPLETION (comp);

	++addr_comp->seq_no;
}

void
e_address_completion_construct (EAddressCompletion *addr_comp, EBook *book)
{
	g_return_if_fail (addr_comp != NULL);
	g_return_if_fail (E_IS_ADDRESS_COMPLETION (addr_comp));
	g_return_if_fail (book != NULL);
	g_return_if_fail (E_IS_BOOK (book));

	/* No switching books mid-stream. */
	g_return_if_fail (addr_comp->book == NULL);

	e_completion_construct (E_COMPLETION (addr_comp),
				e_address_completion_begin,
				e_address_completion_end,
				NULL);

	addr_comp->book = book;
	gtk_object_ref (GTK_OBJECT (book));
}

ECompletion *
e_address_completion_new (EBook *book)
{
	gpointer ptr;

	g_return_val_if_fail (book != NULL, NULL);
	g_return_val_if_fail (E_IS_BOOK (book), NULL);

	ptr = gtk_type_new (e_address_completion_get_type ());
	e_address_completion_construct (E_ADDRESS_COMPLETION (ptr), book);
	return E_COMPLETION (ptr);
}
