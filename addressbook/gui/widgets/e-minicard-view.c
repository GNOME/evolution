/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* 
 * e-minicard-view.c
 * Copyright (C) 2000  Ximian, Inc.
 * Author: Chris Lahey <clahey@ximian.com>
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

#include "e-minicard-view.h"

#include "e-addressbook-util.h"

#include <gtk/gtkselection.h>
#include <gtk/gtkdnd.h>
#include <gal/widgets/e-canvas.h>
#include <gal/widgets/e-unicode.h>
#include <libgnome/gnome-i18n.h>

static void e_minicard_view_drag_data_get(GtkWidget *widget,
					  GdkDragContext *context,
					  GtkSelectionData *selection_data,
					  guint info,
					  guint time,
					  EMinicardView *view);

static EReflowClass *parent_class = NULL;
#define PARENT_TYPE (E_REFLOW_TYPE)

/* The arguments we take */
enum {
	ARG_0,
	ARG_ADAPTER,
	ARG_BOOK,
	ARG_QUERY,
	ARG_EDITABLE
};


enum {
	RIGHT_CLICK,
	LAST_SIGNAL
};

static guint signals [LAST_SIGNAL] = {0, };

enum DndTargetType {
	DND_TARGET_TYPE_VCARD_LIST,
};
#define VCARD_LIST_TYPE "text/x-vcard"
static GtkTargetEntry drag_types[] = {
	{ VCARD_LIST_TYPE, 0, DND_TARGET_TYPE_VCARD_LIST }
};
static gint num_drag_types = sizeof(drag_types) / sizeof(drag_types[0]);

static void
e_minicard_view_drag_data_get(GtkWidget *widget,
			      GdkDragContext *context,
			      GtkSelectionData *selection_data,
			      guint info,
			      guint time,
			      EMinicardView *view)
{
	if (!E_IS_MINICARD_VIEW(view))
		return;

	switch (info) {
	case DND_TARGET_TYPE_VCARD_LIST: {
		char *value;
		
		value = e_card_list_get_vcard(view->drag_list);

		gtk_selection_data_set (selection_data,
					selection_data->target,
					8,
					value, strlen (value));
		break;
	}
	}

	g_list_foreach (view->drag_list, (GFunc)gtk_object_unref, NULL);
	g_list_free (view->drag_list);
	view->drag_list = NULL;
}

static int
e_minicard_view_drag_begin (EAddressbookReflowAdapter *adapter, GdkEvent *event, EMinicardView *view)
{
	GdkDragContext *context;
	GtkTargetList *target_list;
	GdkDragAction actions = GDK_ACTION_MOVE | GDK_ACTION_COPY;
	
	view->drag_list = e_minicard_view_get_card_list (view);

	g_print ("dragging %d card(s)\n", g_list_length (view->drag_list));

	target_list = gtk_target_list_new (drag_types, num_drag_types);

	context = gtk_drag_begin (GTK_WIDGET (GNOME_CANVAS_ITEM (view)->canvas),
				  target_list, actions, 1/*XXX*/, event);

	if (!view->canvas_drag_data_get_id)
		view->canvas_drag_data_get_id = gtk_signal_connect (GTK_OBJECT (GNOME_CANVAS_ITEM (view)->canvas),
								    "drag_data_get",
								    GTK_SIGNAL_FUNC (e_minicard_view_drag_data_get),
								    view);

	gtk_drag_set_icon_default (context);

	return TRUE;
}

static void
set_empty_message (EMinicardView *view)
{
	char *empty_message;
	gboolean editable = FALSE;

	if (view->adapter) {
		gtk_object_get (GTK_OBJECT (view->adapter),
				"editable", &editable,
				NULL);
	}

	if (editable)
		empty_message = e_utf8_from_locale_string(_("\n\nThere are no items to show in this view.\n\n"
							    "Double-click here to create a new Contact."));
	else
		empty_message = e_utf8_from_locale_string(_("\n\nThere are no items to show in this view."));

	gtk_object_set (GTK_OBJECT(view),
			"empty_message", empty_message,
			NULL);

	g_free (empty_message);
}

static void
writable_status_change (EAddressbookModel *model, gboolean writable, EMinicardView *view)
{
	set_empty_message (view);
}

static void
adapter_changed (EMinicardView *view)
{
	set_empty_message (view);

	gtk_signal_connect (GTK_OBJECT (view->adapter), "drag_begin",
			    GTK_SIGNAL_FUNC (e_minicard_view_drag_begin), view);
}

static void
e_minicard_view_set_arg (GtkObject *o, GtkArg *arg, guint arg_id)
{
	GnomeCanvasItem *item;
	EMinicardView *view;

	item = GNOME_CANVAS_ITEM (o);
	view = E_MINICARD_VIEW (o);
	
	switch (arg_id){
	case ARG_ADAPTER:
		if (view->adapter) {
			if (view->writable_status_id) {
				EAddressbookModel *model;
				gtk_object_get (GTK_OBJECT (view->adapter),
						"model", &model,
						NULL);
				if (model) {
					gtk_signal_disconnect (GTK_OBJECT (model), view->writable_status_id);
				}
			}

			gtk_object_unref (GTK_OBJECT(view->adapter));
		}
		view->writable_status_id = 0;
		view->adapter = GTK_VALUE_POINTER (*arg);
		gtk_object_ref (GTK_OBJECT (view->adapter));
		adapter_changed (view);
		gtk_object_set (GTK_OBJECT (view),
				"model", view->adapter,
				NULL);
		if (view->adapter) {
			EAddressbookModel *model;
			gtk_object_get (GTK_OBJECT (view->adapter),
					"model", &model,
					NULL);
			if (model) {
				view->writable_status_id =
					gtk_signal_connect (GTK_OBJECT (model), "writable_status",
							    GTK_SIGNAL_FUNC (writable_status_change), view);
			}
							    
		}
		break;
	case ARG_BOOK:
		gtk_object_set (GTK_OBJECT (view->adapter),
					    "book", GTK_VALUE_OBJECT (*arg),
					    NULL);
		set_empty_message (view);
		break;
	case ARG_QUERY:
		gtk_object_set (GTK_OBJECT (view->adapter),
					    "query", GTK_VALUE_STRING (*arg),
					    NULL);
		break;
	case ARG_EDITABLE:
		gtk_object_set (GTK_OBJECT (view->adapter),
					    "editable", GTK_VALUE_BOOL (*arg),
					    NULL);
		set_empty_message (view);
		break;
	}
}

static void
e_minicard_view_get_arg (GtkObject *object, GtkArg *arg, guint arg_id)
{
	EMinicardView *view;

	view = E_MINICARD_VIEW (object);

	switch (arg_id) {
	case ARG_ADAPTER:
		GTK_VALUE_POINTER (*arg) = view->adapter;
		break;
	case ARG_BOOK:
		gtk_object_get (GTK_OBJECT (view->adapter),
					    "book", &GTK_VALUE_OBJECT (*arg),
					    NULL);
		break;
	case ARG_QUERY:
		gtk_object_get (GTK_OBJECT (view->adapter),
					    "query", &GTK_VALUE_STRING (*arg),
					    NULL);
		break;
	case ARG_EDITABLE:
		gtk_object_get (GTK_OBJECT (view->adapter),
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

	if (view->canvas_drag_data_get_id) {
		gtk_signal_disconnect (GTK_OBJECT (GNOME_CANVAS_ITEM (view)->canvas),
				       view->canvas_drag_data_get_id);
	}

	if (view->adapter) {
		if (view->writable_status_id) {
			EAddressbookModel *model;
			gtk_object_get (GTK_OBJECT (view->adapter),
					"model", &model,
					NULL);
			if (model) {
				gtk_signal_disconnect (GTK_OBJECT (model), view->writable_status_id);
			}
		}

		gtk_object_unref (GTK_OBJECT(view->adapter));
	}
	view->writable_status_id = 0;
	view->adapter = NULL;

	GTK_OBJECT_CLASS(parent_class)->destroy (object);
}

static guint
e_minicard_view_right_click (EMinicardView *view, GdkEvent *event)
{
	guint ret_val = 0;
	gtk_signal_emit (GTK_OBJECT (view), signals[RIGHT_CLICK],
			 event, &ret_val);
	return ret_val;
}

static gboolean
e_minicard_view_event (GnomeCanvasItem *item, GdkEvent *event)
{
	EMinicardView *view;
 
	view = E_MINICARD_VIEW (item);

	switch( event->type ) {
	case GDK_2BUTTON_PRESS:
		if (((GdkEventButton *)event)->button == 1) {
			gboolean editable;

			gtk_object_get(GTK_OBJECT(view->adapter), "editable", &editable, NULL);
  
			if (editable) {
				EBook *book;
				gtk_object_get(GTK_OBJECT(view), "book", &book, NULL);
 
				if (book && E_IS_BOOK (book))
					e_addressbook_show_contact_editor (book, e_card_new(""), TRUE, editable);
			}
			return TRUE;
		}
	case GDK_BUTTON_PRESS:
		if (event->button.button == 3) {
			e_minicard_view_right_click (view, event);
		}
		break;
	default:
		break;
	}

	if (GNOME_CANVAS_ITEM_CLASS(parent_class)->event)
		return GNOME_CANVAS_ITEM_CLASS(parent_class)->event(item, event);
	else
		return FALSE;
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
	case GDK_FOCUS_CHANGE:
		if (event->focus_change.in) {
			int i;
			for (i = 0; i < reflow->count; i++) {
				if (reflow->items[i] == item) {
					e_selection_model_maybe_do_something(reflow->selection, i, 0, 0);
					break;
				}
			}
		}
		break;
	case GDK_BUTTON_PRESS:
		if (event->button.button == 3) {
			return_val = e_minicard_view_right_click (view, event);
			if (!return_val)
				e_selection_model_right_click_up(reflow->selection);
		}
		break;
	default:
		break;
	}
	return return_val;
}

typedef struct {
	EMinicardView *view;
	EBookCallback cb;
	gpointer closure;
} ViewCbClosure;

static void
do_remove (int i, gpointer user_data)
{
	EBook *book;
	ECard *card;
	ViewCbClosure *viewcbclosure = user_data;
	EMinicardView *view = viewcbclosure->view;
	EBookCallback cb = viewcbclosure->cb;
	gpointer closure = viewcbclosure->closure;

	gtk_object_get (GTK_OBJECT(view->adapter),
			"book", &book,
			NULL);

	card = e_addressbook_reflow_adapter_get_card (view->adapter, i);

	e_book_remove_card(book, card, cb, closure);

	gtk_object_unref (GTK_OBJECT (card));
}

#if 0
static int
compare_to_utf_str (EMinicard *card, const char *utf_str)
{
	g_return_val_if_fail(card != NULL, 0);
	g_return_val_if_fail(E_IS_MINICARD(card), 0);

	if (g_unichar_isdigit (g_utf8_get_char (utf_str))) {
		return 1;
	}

	if (card->card) {
		char *file_as;
		gtk_object_get(GTK_OBJECT(card->card),
			       "file_as", &file_as,
			       NULL);
		if (file_as)
			return g_utf8_strcasecmp (file_as, utf_str);
		else
			return 0;
	} else {
		return 0;
	}
}
#endif

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
	
	gtk_object_add_arg_type ("EMinicardView::adapter", GTK_TYPE_OBJECT, 
				 GTK_ARG_READWRITE, ARG_ADAPTER);
	gtk_object_add_arg_type ("EMinicardView::book", GTK_TYPE_OBJECT, 
				 GTK_ARG_READWRITE, ARG_BOOK);
	gtk_object_add_arg_type ("EMinicardView::query", GTK_TYPE_STRING,
				 GTK_ARG_READWRITE, ARG_QUERY);
	gtk_object_add_arg_type ("EMinicardView::editable", GTK_TYPE_BOOL,
				 GTK_ARG_READWRITE, ARG_EDITABLE);

	signals [RIGHT_CLICK] =
		gtk_signal_new ("right_click",
				GTK_RUN_LAST,
				object_class->type,
				GTK_SIGNAL_OFFSET (EMinicardViewClass, right_click),
				gtk_marshal_INT__POINTER,
				GTK_TYPE_INT, 1, GTK_TYPE_GDK_EVENT);

	gtk_object_class_add_signals (object_class, signals, LAST_SIGNAL);

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
	view->adapter = NULL;
	view->canvas_drag_data_get_id = 0;
	view->writable_status_id = 0;

	set_empty_message (view);
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

void
e_minicard_view_jump_to_letter (EMinicardView *view,
                                gunichar letter)
{
#if 0
	char uft_str[6 + 1];

	utf_str [g_unichar_to_utf8 (letter, utf_str)] = '\0';
	e_reflow_sorted_jump (E_REFLOW_SORTED (view),
	                      (GCompareFunc) compare_to_utf_str,
	                      utf_str);
#endif
}

typedef struct {
	GList *list;
	EAddressbookReflowAdapter *adapter;
} ModelAndList;

static void
add_to_list (int index, gpointer closure)
{
	ModelAndList *mal = closure;
	mal->list = g_list_prepend (mal->list, e_addressbook_reflow_adapter_get_card (mal->adapter, index));
}

GList *
e_minicard_view_get_card_list (EMinicardView *view)
{
	ModelAndList mal;

	mal.adapter = view->adapter;
	mal.list = NULL;

	e_selection_model_foreach (E_REFLOW (view)->selection, add_to_list, &mal);

	mal.list = g_list_reverse (mal.list);
	return mal.list;
}
