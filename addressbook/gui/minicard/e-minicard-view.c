/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* 
 * e-minicard-view.c
 * Copyright (C) 2000  Helix Code, Inc.
 * Author: Chris Lahey <clahey@helixcode.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
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
#include <gnome.h>
#include <e-util/e-canvas.h>
#include "e-minicard-view.h"
#include "e-minicard.h"
#include "e-contact-editor.h"
static void e_minicard_view_init		(EMinicardView		 *reflow);
static void e_minicard_view_class_init	(EMinicardViewClass	 *klass);
static void e_minicard_view_set_arg (GtkObject *o, GtkArg *arg, guint arg_id);
static void e_minicard_view_get_arg (GtkObject *object, GtkArg *arg, guint arg_id);
static void e_minicard_view_destroy (GtkObject *object);
static gboolean e_minicard_view_event (GnomeCanvasItem *item, GdkEvent *event);
static void canvas_destroy (GtkObject *object, EMinicardView *view);
static void disconnect_signals (EMinicardView *view);

static EReflowSortedClass *parent_class = NULL;

/* The arguments we take */
enum {
	ARG_0,
	ARG_BOOK,
	ARG_QUERY
};

GtkType
e_minicard_view_get_type (void)
{
  static GtkType reflow_type = 0;

  if (!reflow_type)
    {
      static const GtkTypeInfo reflow_info =
      {
        "EMinicardView",
        sizeof (EMinicardView),
        sizeof (EMinicardViewClass),
        (GtkClassInitFunc) e_minicard_view_class_init,
        (GtkObjectInitFunc) e_minicard_view_init,
        /* reserved_1 */ NULL,
        /* reserved_2 */ NULL,
        (GtkClassInitFunc) NULL,
      };

      reflow_type = gtk_type_unique (e_reflow_sorted_get_type (), &reflow_info);
    }

  return reflow_type;
}

static void
e_minicard_view_class_init (EMinicardViewClass *klass)
{
	GtkObjectClass *object_class;
	GnomeCanvasItemClass *item_class;
	
	object_class = (GtkObjectClass*) klass;
	item_class = (GnomeCanvasItemClass *) klass;
	
	parent_class = gtk_type_class (e_reflow_sorted_get_type ());
	
	gtk_object_add_arg_type ("EMinicardView::book", GTK_TYPE_OBJECT, 
				 GTK_ARG_READWRITE, ARG_BOOK);
	gtk_object_add_arg_type ("EMinicardView::query", GTK_TYPE_STRING,
				 GTK_ARG_READWRITE, ARG_QUERY);
	
	object_class->set_arg   = e_minicard_view_set_arg;
	object_class->get_arg   = e_minicard_view_get_arg;
	object_class->destroy   = e_minicard_view_destroy;

	item_class->event       = e_minicard_view_event;
	
	/* GnomeCanvasItem method overrides */
}

static void
e_minicard_view_init (EMinicardView *view)
{
	view->book = NULL;
	view->query = g_strdup("(contains \"full_name\" \"\")");
	view->book_view = NULL;
	view->get_view_idle = 0;
	view->create_card_id = 0;
	view->remove_card_id = 0;
	view->modify_card_id = 0;
	view->canvas_destroy_id = 0;
	
	gtk_object_set(GTK_OBJECT(view),
		       "empty_message", _("\n\nThere are no items to show in this view\n\n"
					  "Double-click here to create a new Contact."),
		       NULL);

	E_REFLOW_SORTED(view)->compare_func = (GCompareFunc) e_minicard_compare;
	E_REFLOW_SORTED(view)->string_func  = (EReflowStringFunc) e_minicard_get_card_id;
}

static void
create_card(EBookView *book_view, const GList *cards, EMinicardView *view)
{
	for (; cards; cards = g_list_next(cards)) {
		GnomeCanvasItem *item = gnome_canvas_item_new(GNOME_CANVAS_GROUP(view),
							      e_minicard_get_type(),
							      "card", cards->data,
							      NULL);
		e_reflow_add_item(E_REFLOW(view), item);
	}
}

static void
modify_card(EBookView *book_view, const GList *cards, EMinicardView *view)
{
	for (; cards; cards = g_list_next(cards)) {
		ECard *card = cards->data;
		gchar *id = e_card_get_id(card);
		GnomeCanvasItem *item = e_reflow_sorted_get_item(E_REFLOW_SORTED(view), id);
		if (item && !GTK_OBJECT_DESTROYED(item)) {
			gnome_canvas_item_set(item,
					      "card", card,
					      NULL);
			e_reflow_sorted_reorder_item(E_REFLOW_SORTED(view), id);
		}
	}
}

static void
remove_card(EBookView *book_view, const char *id, EMinicardView *view)
{
	e_reflow_sorted_remove_item(E_REFLOW_SORTED(view), id);
}

static void
book_view_loaded (EBook *book, EBookStatus status, EBookView *book_view, gpointer closure)
{
	EMinicardView *view = closure;
	disconnect_signals(view);
	if (view->book_view)
		gtk_object_unref(GTK_OBJECT(view->book_view));

	if (!view->canvas_destroy_id)
		view->canvas_destroy_id = 
			gtk_signal_connect(GTK_OBJECT(GNOME_CANVAS_ITEM(view)->canvas),
					   "destroy", GTK_SIGNAL_FUNC(canvas_destroy),
					   view);

	view->book_view = book_view;
	if (view->book_view)
		gtk_object_ref(GTK_OBJECT(view->book_view));
	

	view->create_card_id = gtk_signal_connect(GTK_OBJECT(view->book_view),
						  "card_added",
						  GTK_SIGNAL_FUNC(create_card),
						  view);
	view->remove_card_id = gtk_signal_connect(GTK_OBJECT(view->book_view),
						  "card_removed",
						  GTK_SIGNAL_FUNC(remove_card),
						  view);
	view->modify_card_id = gtk_signal_connect(GTK_OBJECT(view->book_view),
						  "card_changed",
						  GTK_SIGNAL_FUNC(modify_card),
						  view);

	g_list_foreach(E_REFLOW(view)->items, (GFunc) gtk_object_unref, NULL);
	g_list_foreach(E_REFLOW(view)->items, (GFunc) gtk_object_destroy, NULL);
	g_list_free(E_REFLOW(view)->items);
	E_REFLOW(view)->items = NULL;
	e_canvas_item_request_reflow(GNOME_CANVAS_ITEM(view));
}

static gboolean
get_view(EMinicardView *view)
{
	if (view->book && view->query)
		e_book_get_book_view(view->book, view->query, book_view_loaded, view);

	view->get_view_idle = 0;
	return FALSE;
}

static void
e_minicard_view_set_arg (GtkObject *o, GtkArg *arg, guint arg_id)
{
	GnomeCanvasItem *item;
	EMinicardView *view;

	item = GNOME_CANVAS_ITEM (o);
	view = E_MINICARD_VIEW (o);
	
	switch (arg_id){
	case ARG_BOOK:
		if (view->book)
			gtk_object_unref(GTK_OBJECT(view->book));
		view->book = E_BOOK(GTK_VALUE_OBJECT (*arg));
		if (view->book) {
			gtk_object_ref(GTK_OBJECT(view->book));
			if (view->get_view_idle == 0)
				view->get_view_idle = g_idle_add((GSourceFunc)get_view, view);
		}
		break;
	case ARG_QUERY:
		g_free(view->query);
		view->query = g_strdup(GTK_VALUE_STRING (*arg));
		if (view->get_view_idle == 0)
			view->get_view_idle = g_idle_add((GSourceFunc)get_view, view);
		break;
	}
}

static void
e_minicard_view_get_arg (GtkObject *object, GtkArg *arg, guint arg_id)
{
	EMinicardView *e_minicard_view;

	e_minicard_view = E_MINICARD_VIEW (object);

	switch (arg_id) {
	case ARG_BOOK:
		GTK_VALUE_OBJECT (*arg) = GTK_OBJECT(e_minicard_view->book);
		break;
	case ARG_QUERY:
		GTK_VALUE_STRING (*arg) = g_strdup(e_minicard_view->query);
		break;
	default:
		arg->type = GTK_TYPE_INVALID;
		break;
	}
}

static void
e_minicard_view_destroy (GtkObject *object)
{
	EMinicardView *view = E_MINICARD_VIEW(object);

	if (view->get_view_idle)
		g_source_remove(view->get_view_idle);
	if (view->canvas_destroy_id)
		gtk_signal_disconnect(GTK_OBJECT (GNOME_CANVAS_ITEM(view)->canvas),
				      view->canvas_destroy_id);
	disconnect_signals(view);
	g_free(view->query);
	if (view->book)
		gtk_object_unref(GTK_OBJECT(view->book));
	if (view->book_view)
		gtk_object_unref(GTK_OBJECT(view->book_view));
  
	GTK_OBJECT_CLASS(parent_class)->destroy (object);
}

static void
card_added_cb (EBook* book, EBookStatus status, const char *id,
	    gpointer user_data)
{
	g_print ("%s: %s(): a card was added\n", __FILE__, __FUNCTION__);
}

static gboolean
e_minicard_view_event (GnomeCanvasItem *item, GdkEvent *event)
{
	EMinicardView *view;
 
	view = E_MINICARD_VIEW (item);

	switch( event->type ) {
	case GDK_2BUTTON_PRESS:
		{
			gint result;
			ECard *card;
			GtkWidget* contact_editor;
			EBook *book;
			GtkWidget* dlg = gnome_dialog_new ("Contact Editor", "Save", "Cancel", NULL);
		
			card = e_card_new("");
			contact_editor = e_contact_editor_new(card);
			gtk_object_sink(GTK_OBJECT(card));
		
			gtk_window_set_policy(GTK_WINDOW(dlg), FALSE, TRUE, FALSE);
		
			gtk_object_get(GTK_OBJECT(view), "book", &book, NULL);
		
		
			g_assert (E_IS_BOOK (book));
		
			gtk_box_pack_start (GTK_BOX (GNOME_DIALOG (dlg)->vbox),
					    contact_editor, TRUE, TRUE, 0);
		
			gtk_widget_show (contact_editor);
			gtk_widget_show (dlg);
		
			gnome_dialog_close_hides (GNOME_DIALOG (dlg), TRUE);
			result = gnome_dialog_run_and_close (GNOME_DIALOG (dlg));
		
		
			/* If the user clicks "okay"...*/
			if (result == 0) {
				ECard *card;
				g_assert (contact_editor);
				g_assert (GTK_IS_OBJECT (contact_editor));
				gtk_object_get(GTK_OBJECT(contact_editor),
					       "card", &card,
					       NULL);
			
				/* Add the card in the contact editor to our ebook */
				e_book_add_card (
						 book,
						 card,
						 card_added_cb,
						 NULL);
			}
		}
		return FALSE;
	default:
		return TRUE;
		break;
	}
}

static void
disconnect_signals(EMinicardView *view)
{
	if (view->book_view && view->create_card_id)
		gtk_signal_disconnect(GTK_OBJECT (view->book_view),
				      view->create_card_id);
	if (view->book_view && view->remove_card_id)
		gtk_signal_disconnect(GTK_OBJECT (view->book_view),
				      view->remove_card_id);
	if (view->book_view && view->modify_card_id)
		gtk_signal_disconnect(GTK_OBJECT (view->book_view),
				      view->modify_card_id);
}

static void
canvas_destroy(GtkObject *object, EMinicardView *view)
{
	disconnect_signals(view);
}

void
e_minicard_view_remove_selection(EMinicardView *view,
				 EBookCallback  cb,
				 gpointer       closure)
{
	if (view->book) {
		EReflow *reflow = E_REFLOW(view);
		GList *list;
		for (list = reflow->items; list; list = g_list_next(list)) {
			GnomeCanvasItem *item = list->data;
			gboolean has_focus;
			gtk_object_get(GTK_OBJECT(item),
				       "has_focus", &has_focus,
				       NULL);
			if (has_focus) {
				ECard *card;
				gtk_object_get(GTK_OBJECT(item),
					       "card", &card,
					       NULL);
				e_book_remove_card(view->book, card, cb, closure);
				return;
			}
		}
	}
}

static int
compare_to_letter(EMinicard *card, char *letter)
{
	g_return_val_if_fail(card != NULL, 0);
	g_return_val_if_fail(E_IS_MINICARD(card), 0);

	if (*letter == '1')
		return 1;

	if (card->card) {
		char *file_as;
		gtk_object_get(GTK_OBJECT(card->card),
			       "file_as", &file_as,
			       NULL);
		if (file_as)
			return strncasecmp(file_as, letter, 1);
		else
			return 0;
	} else {
		return 0;
	}
}

void       e_minicard_view_jump_to_letter   (EMinicardView *view,
					     char           letter)
{
	e_reflow_sorted_jump(E_REFLOW_SORTED(view),
			     (GCompareFunc) compare_to_letter,
			     &letter);
}
