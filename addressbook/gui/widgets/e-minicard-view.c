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
static void e_minicard_view_init		(EMinicardView		 *reflow);
static void e_minicard_view_class_init	(EMinicardViewClass	 *klass);
static void e_minicard_view_set_arg (GtkObject *o, GtkArg *arg, guint arg_id);
static void e_minicard_view_get_arg (GtkObject *object, GtkArg *arg, guint arg_id);
static void e_minicard_view_destroy (GtkObject *object);

#define E_MINICARD_VIEW_DIVIDER_WIDTH 2
#define E_MINICARD_VIEW_BORDER_WIDTH 7
#define E_MINICARD_VIEW_FULL_GUTTER (E_MINICARD_VIEW_DIVIDER_WIDTH + E_MINICARD_VIEW_BORDER_WIDTH * 2)

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
		GnomeCanvasItem *item = gnome_canvas_item_new(GNOME_CANVAS_GROUP(view),
							      e_minicard_get_type(),
							      "card", cards->data,
							      NULL);
		e_reflow_sorted_replace_item(E_REFLOW_SORTED(view), item);
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
	if (view->book_view && view->create_card_id)
		gtk_signal_disconnect(GTK_OBJECT (view->book_view),
				      view->create_card_id);
	if (view->book_view && view->remove_card_id)
		gtk_signal_disconnect(GTK_OBJECT (view->book_view),
				      view->remove_card_id);
	if (view->book_view && view->modify_card_id)
		gtk_signal_disconnect(GTK_OBJECT (view->book_view),
				      view->modify_card_id);
	if (view->book_view)
		gtk_object_unref(GTK_OBJECT(view->book_view));
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
	g_list_foreach(E_REFLOW(view)->items, (GFunc) gtk_object_destroy, NULL);
	g_list_free(E_REFLOW(view)->items);
	E_REFLOW(view)->items = NULL;
	e_canvas_item_request_reflow(GNOME_CANVAS_ITEM(view));
}

static gboolean
get_view(EMinicardView *view)
{
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
		if (view->query)
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
	if (view->book)
		gtk_object_unref(GTK_OBJECT(view->book));
	if (view->book_view && view->create_card_id)
		gtk_signal_disconnect(GTK_OBJECT (view->book_view),
				      view->create_card_id);
	if (view->book_view && view->remove_card_id)
		gtk_signal_disconnect(GTK_OBJECT (view->book_view),
				      view->remove_card_id);
	if (view->book_view && view->modify_card_id)
		gtk_signal_disconnect(GTK_OBJECT (view->book_view),
				      view->modify_card_id);
	if (view->book_view)
		gtk_object_unref(GTK_OBJECT(view->book_view));
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
