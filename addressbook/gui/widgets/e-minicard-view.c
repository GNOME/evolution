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

#include "e-minicard-view.h"

#include "e-contact-editor.h"

#include <gtk/gtkselection.h>
#include <gtk/gtkdnd.h>
#include <gal/widgets/e-canvas.h>
#include <gal/widgets/e-unicode.h>
#include <libgnome/gnome-i18n.h>

#if 0
static void canvas_destroy (GtkObject *object, EMinicardView *view);
static void e_minicard_view_drag_data_get(GtkWidget *widget,
					  GdkDragContext *context,
					  GtkSelectionData *selection_data,
					  guint info,
					  guint time,
					  EMinicardView *view);
#endif

static EReflowClass *parent_class = NULL;
#define PARENT_TYPE (E_REFLOW_TYPE)

/* The arguments we take */
enum {
	ARG_0,
	ARG_BOOK,
	ARG_QUERY,
	ARG_EDITABLE
};

enum {
	STATUS_MESSAGE,
	LAST_SIGNAL
};

#if 0
enum DndTargetType {
	DND_TARGET_TYPE_VCARD,
};
#define VCARD_TYPE "text/x-vcard"
static GtkTargetEntry drag_types[] = {
	{ VCARD_TYPE, 0, DND_TARGET_TYPE_VCARD }
};
static gint num_drag_types = sizeof(drag_types) / sizeof(drag_types[0]);
#endif

static guint e_minicard_view_signals [LAST_SIGNAL] = {0, };

#if 0
static void
e_minicard_view_drag_data_get(GtkWidget *widget,
			      GdkDragContext *context,
			      GtkSelectionData *selection_data,
			      guint info,
			      guint time,
			      EMinicardView *view)
{
	printf ("e_minicard_view_drag_data_get (e_minicard = %p)\n", view->drag_card);

	if (!E_IS_MINICARD_VIEW(view))
		return;

	switch (info) {
	case DND_TARGET_TYPE_VCARD: {
		char *value;

		value = e_card_simple_get_vcard(view->drag_card->simple);

		gtk_selection_data_set (selection_data,
					selection_data->target,
					8,
					value, strlen (value));
		break;
	}
	}
}

static int
e_minicard_view_drag_begin (EMinicard *card, GdkEvent *event, EMinicardView *view)
{
	GdkDragContext *context;
	GtkTargetList *target_list;
	GdkDragAction actions = GDK_ACTION_MOVE;

	view->drag_card = card;

	target_list = gtk_target_list_new (drag_types, num_drag_types);

	context = gtk_drag_begin (GTK_WIDGET (GNOME_CANVAS_ITEM (view)->canvas),
				  target_list, actions, 1/*XXX*/, event);

	gtk_drag_set_icon_default (context);

	return TRUE;
}
#endif

#if 0
static void
status_message (EBookView *book_view,
		char* status,
		EMinicardView *view)
{
	gtk_signal_emit (GTK_OBJECT (view),
			 e_minicard_view_signals [STATUS_MESSAGE],
			 status);
}
#endif

static void
card_added_cb (EBook* book, EBookStatus status, const char *id,
	    gpointer user_data)
{
	g_print ("%s: %s(): a card was added\n", __FILE__, __FUNCTION__);
}

static void
card_changed_cb (EBook* book, EBookStatus status, gpointer user_data)
{
	g_print ("%s: %s(): a card was changed with status %d\n", __FILE__, __FUNCTION__, status);
}

/* Callback for the add_card signal from the contact editor */
static void
add_card_cb (EContactEditor *ce, ECard *card, gpointer data)
{
	EBook *book;

	book = E_BOOK (data);
	e_book_add_card (book, card, card_added_cb, NULL);
}

/* Callback for the commit_card signal from the contact editor */
static void
commit_card_cb (EContactEditor *ce, ECard *card, gpointer data)
{
	EBook *book;

	book = E_BOOK (data);
	e_book_commit_card (book, card, card_changed_cb, NULL);
}

/* Callback used when the contact editor is closed */
static void
editor_closed_cb (EContactEditor *ce, gpointer data)
{
	gtk_object_unref (GTK_OBJECT (ce));
}

static void
supported_fields_cb (EBook *book, EBookStatus status, EList *fields, EMinicardView *view)
{
	ECard *card;
	EContactEditor *ce;
	gboolean editable;

	card = e_card_new("");

	gtk_object_get (GTK_OBJECT (view->model),
			"editable", &editable,
			NULL);

	ce = e_contact_editor_new (card, TRUE, fields, !editable);

	gtk_signal_connect (GTK_OBJECT (ce), "add_card",
			    GTK_SIGNAL_FUNC (add_card_cb), book);
	gtk_signal_connect (GTK_OBJECT (ce), "commit_card",
			    GTK_SIGNAL_FUNC (commit_card_cb), book);
	gtk_signal_connect (GTK_OBJECT (ce), "editor_closed",
			    GTK_SIGNAL_FUNC (editor_closed_cb), NULL);

	gtk_object_sink(GTK_OBJECT(card));
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
		gtk_object_set (GTK_OBJECT (view->model),
					    "book", GTK_VALUE_OBJECT (*arg),
					    NULL);
		break;
	case ARG_QUERY:
		gtk_object_set (GTK_OBJECT (view->model),
					    "query", GTK_VALUE_STRING (*arg),
					    NULL);
		break;
	case ARG_EDITABLE:
		gtk_object_set (GTK_OBJECT (view->model),
					    "editable", GTK_VALUE_BOOL (*arg),
					    NULL);
		break;
	}
}

static void
e_minicard_view_get_arg (GtkObject *object, GtkArg *arg, guint arg_id)
{
	EMinicardView *view;

	view = E_MINICARD_VIEW (object);

	switch (arg_id) {
	case ARG_BOOK:
		gtk_object_get (GTK_OBJECT (view->model),
					    "book", &GTK_VALUE_OBJECT (*arg),
					    NULL);
		break;
		break;
	case ARG_QUERY:
		gtk_object_get (GTK_OBJECT (view->model),
					    "query", &GTK_VALUE_STRING (*arg),
					    NULL);
		break;
	case ARG_EDITABLE:
		gtk_object_get (GTK_OBJECT (view->model),
					    "editable", &GTK_VALUE_BOOL (*arg),
					    NULL);
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

	
	gtk_object_unref (GTK_OBJECT (view->model));

	GTK_OBJECT_CLASS(parent_class)->destroy (object);
}

static gboolean
e_minicard_view_event (GnomeCanvasItem *item, GdkEvent *event)
{
	EMinicardView *view;
 
	view = E_MINICARD_VIEW (item);

	switch( event->type ) {
	case GDK_2BUTTON_PRESS:
		if (((GdkEventButton *)event)->button == 1)
		{
			EBook *book;

			gtk_object_get(GTK_OBJECT(view), "book", &book, NULL);

			g_assert (E_IS_BOOK (book));

			e_book_get_supported_fields (book,
						     (EBookFieldsCallback)supported_fields_cb,
						     view);
		}
		return TRUE;
	default:
		if (GNOME_CANVAS_ITEM_CLASS(parent_class)->event)
			return GNOME_CANVAS_ITEM_CLASS(parent_class)->event(item, event);
		else
			return FALSE;
		break;
	}
}

static gint
e_minicard_view_selection_event (EReflow *reflow, GnomeCanvasItem *item, GdkEvent *event)
{
	EMinicardView *view;
	int return_val = FALSE;

	view = E_MINICARD_VIEW (reflow);
	if (parent_class->selection_event) {
		return_val = parent_class->selection_event (reflow, item, event);
	}

	switch (event->type) {
	case GDK_BUTTON_PRESS:
		if (event->button.button == 3) {
			return_val = e_minicard_view_model_right_click (view->model, event, reflow->selection);
		}
		break;
	default:
		break;
	}
	return return_val;
}

static void
disconnect_signals(EMinicardView *view)
{
	if (view->model && view->status_message_id)
		gtk_signal_disconnect(GTK_OBJECT (view->model),
				      view->status_message_id);

	view->status_message_id = 0;
}

#if 0
static void
canvas_destroy(GtkObject *object, EMinicardView *view)
{
	disconnect_signals(view);
}
#endif

typedef struct {
	EMinicardView *view;
	EBookCallback cb;
	gpointer closure;
} ViewCbClosure;

static void
do_remove (int i, gpointer user_data)
{
	ECard *card;
	ViewCbClosure *viewcbclosure = user_data;
	EMinicardView *view = viewcbclosure->view;
	EBookCallback cb = viewcbclosure->cb;
	gpointer closure = viewcbclosure->closure;
	gtk_object_get(GTK_OBJECT(view->model->data[i]),
		       "card", &card,
		       NULL);
	e_book_remove_card(view->model->book, card, cb, closure);
}

void
e_minicard_view_remove_selection(EMinicardView *view,
				 EBookCallback  cb,
				 gpointer       closure)
{
	ViewCbClosure viewcbclosure;
	viewcbclosure.view = view;
	viewcbclosure.cb = cb;
	viewcbclosure.closure = closure;

	e_selection_model_foreach (E_REFLOW (view)->selection,
				   do_remove,
				   &viewcbclosure);
}

#if 0
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
#endif



void
e_minicard_view_jump_to_letter   (EMinicardView *view,
					     char           letter)
{
#if 0
	e_reflow_sorted_jump(E_REFLOW_SORTED(view),
			     (GCompareFunc) compare_to_letter,
			     &letter);
#endif
}

void
e_minicard_view_stop             (EMinicardView *view)
{
	e_minicard_view_model_stop (view->model);
	disconnect_signals(view);
}


static void
e_minicard_view_class_init (EMinicardViewClass *klass)
{
	GtkObjectClass *object_class;
	GnomeCanvasItemClass *item_class;
	EReflowClass *reflow_class;
	
	object_class = (GtkObjectClass*) klass;
	item_class = (GnomeCanvasItemClass *) klass;
	reflow_class = (EReflowClass *) klass;
	
	parent_class = gtk_type_class (PARENT_TYPE);
	
	gtk_object_add_arg_type ("EMinicardView::book", GTK_TYPE_OBJECT, 
				 GTK_ARG_READWRITE, ARG_BOOK);
	gtk_object_add_arg_type ("EMinicardView::query", GTK_TYPE_STRING,
				 GTK_ARG_READWRITE, ARG_QUERY);
	gtk_object_add_arg_type ("EMinicardView::editable", GTK_TYPE_BOOL,
				 GTK_ARG_READWRITE, ARG_EDITABLE);

	e_minicard_view_signals [STATUS_MESSAGE] =
		gtk_signal_new ("status_message",
				GTK_RUN_LAST,
				object_class->type,
				GTK_SIGNAL_OFFSET (EMinicardViewClass, status_message),
				gtk_marshal_NONE__POINTER,
				GTK_TYPE_NONE, 1, GTK_TYPE_POINTER);

	gtk_object_class_add_signals (object_class, e_minicard_view_signals, LAST_SIGNAL);
	
	object_class->set_arg         = e_minicard_view_set_arg;
	object_class->get_arg         = e_minicard_view_get_arg;
	object_class->destroy         = e_minicard_view_destroy;

	item_class->event             = e_minicard_view_event;

	reflow_class->selection_event = e_minicard_view_selection_event;
	/* GnomeCanvasItem method overrides */
}

static void
e_minicard_view_init (EMinicardView *view)
{
	char *empty_message;

	view->model = E_MINICARD_VIEW_MODEL(e_minicard_view_model_new());

	empty_message = e_utf8_from_locale_string(_("\n\nThere are no items to show in this view\n\n"
						    "Double-click here to create a new Contact."));
	gtk_object_set (GTK_OBJECT(view),
			"empty_message", empty_message,
			"model", view->model,
			NULL);
	g_free (empty_message);
}

GtkType
e_minicard_view_get_type (void)
{
	static GtkType reflow_type = 0;

	if (!reflow_type) {
		static const GtkTypeInfo reflow_info = {
			"EMinicardView",
			sizeof (EMinicardView),
			sizeof (EMinicardViewClass),
			(GtkClassInitFunc) e_minicard_view_class_init,
			(GtkObjectInitFunc) e_minicard_view_init,
				/* reserved_1 */ NULL,
				/* reserved_2 */ NULL,
			(GtkClassInitFunc) NULL,
		};

		reflow_type = gtk_type_unique (PARENT_TYPE, &reflow_info);
	}

	return reflow_type;
}
