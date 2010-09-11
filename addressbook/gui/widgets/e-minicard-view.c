/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) version 3.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with the program; if not, see <http://www.gnu.org/licenses/>
 *
 *
 * Authors:
 *		Chris Lahey <clahey@ximian.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#include <config.h>

#include "e-minicard-view.h"

#include "eab-gui-util.h"
#include "util/eab-book-util.h"
#include "e-util/e-util.h"

#include <gtk/gtk.h>
#include <gdk/gdkkeysyms.h>
#include <misc/e-canvas.h>
#include <glib/gi18n.h>
#include <string.h>
#include "e-util/e-util.h"
#include "ea-addressbook.h"

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
	PROP_0,
	PROP_ADAPTER,
	PROP_BOOK,
	PROP_QUERY,
	PROP_EDITABLE
};

enum {
	CREATE_CONTACT,
	CREATE_CONTACT_LIST,
	RIGHT_CLICK,
	LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = {0, };

enum DndTargetType {
	DND_TARGET_TYPE_VCARD_LIST,
	DND_TARGET_TYPE_SOURCE_VCARD_LIST
};
#define VCARD_LIST_TYPE "text/x-vcard"
#define SOURCE_VCARD_LIST_TYPE "text/x-source-vcard"
static GtkTargetEntry drag_types[] = {
	{ (gchar *) SOURCE_VCARD_LIST_TYPE, 0, DND_TARGET_TYPE_SOURCE_VCARD_LIST },
	{ (gchar *) VCARD_LIST_TYPE, 0, DND_TARGET_TYPE_VCARD_LIST }
};

static void
e_minicard_view_drag_data_get(GtkWidget *widget,
			      GdkDragContext *context,
			      GtkSelectionData *selection_data,
			      guint info,
			      guint time,
			      EMinicardView *view)
{
	GdkAtom target;

	if (!E_IS_MINICARD_VIEW(view))
		return;

	target = gtk_selection_data_get_target (selection_data);

	switch (info) {
	case DND_TARGET_TYPE_VCARD_LIST: {
		gchar *value;

		value = eab_contact_list_to_string (view->drag_list);

		gtk_selection_data_set (
			selection_data, target, 8,
			(guchar *) value, strlen (value));
		g_free (value);
		break;
	}
	case DND_TARGET_TYPE_SOURCE_VCARD_LIST: {
		EBook *book;
		gchar *value;

		g_object_get (view->adapter, "book", &book, NULL);
		value = eab_book_and_contact_list_to_string (book, view->drag_list);

		gtk_selection_data_set (
			selection_data, target, 8,
			(guchar *) value, strlen (value));
		g_free (value);
		break;
	}
	}
}

static void
clear_drag_data (EMinicardView *view)
{
	g_list_foreach (view->drag_list, (GFunc)g_object_unref, NULL);
	g_list_free (view->drag_list);
	view->drag_list = NULL;
}

static gint
e_minicard_view_drag_begin (EAddressbookReflowAdapter *adapter,
                            GdkEvent *event,
                            EMinicardView *view)
{
	GdkDragContext *context;
	GtkTargetList *target_list;
	GdkDragAction actions = GDK_ACTION_MOVE | GDK_ACTION_COPY;

	clear_drag_data (view);

	view->drag_list = e_minicard_view_get_card_list (view);

	g_print ("dragging %d card(s)\n", g_list_length (view->drag_list));

	target_list = gtk_target_list_new (drag_types, G_N_ELEMENTS (drag_types));

	context = gtk_drag_begin (GTK_WIDGET (GNOME_CANVAS_ITEM (view)->canvas),
				  target_list, actions, 1/*XXX*/, event);

	if (!view->canvas_drag_data_get_id)
		view->canvas_drag_data_get_id = g_signal_connect (GNOME_CANVAS_ITEM (view)->canvas,
								  "drag_data_get",
								  G_CALLBACK (e_minicard_view_drag_data_get),
								  view);

	gtk_drag_set_icon_default (context);

	return TRUE;
}

static void
set_empty_message (EMinicardView *view)
{
	gchar *empty_message;
	gboolean editable = FALSE, perform_initial_query = FALSE, searching = FALSE;
	EBook *book;

	if (view->adapter) {
		EAddressbookModel *model = NULL;

		g_object_get (view->adapter,
			      "editable", &editable,
			      "model", &model,
			      "book", &book,
			      NULL);

		if (book && !e_book_check_static_capability (book, "do-initial-query"))
			perform_initial_query = TRUE;

		searching = model && e_addressbook_model_can_stop (model);
	}

	if (searching) {
		empty_message = _("\n\nSearching for the Contacts...");
	} else if (editable) {
		if (perform_initial_query)
			empty_message = _("\n\nSearch for the Contact\n\n"
					  "or double-click here to create a new Contact.");
		else
			empty_message = _("\n\nThere are no items to show in this view.\n\n"
					  "Double-click here to create a new Contact.");
	} else {
		if (perform_initial_query)
			empty_message = _("\n\nSearch for the Contact.");
		else
			empty_message = _("\n\nThere are no items to show in this view.");
	}

	g_object_set (view,
		      "empty_message", empty_message,
		      NULL);
}

static void
writable_status_change (EAddressbookModel *model,
                        gboolean writable,
                        EMinicardView *view)
{
	set_empty_message (view);
}

static void
stop_state_changed (EAddressbookModel *model,
                    EMinicardView *view)
{
	set_empty_message (view);
}

static void
adapter_changed (EMinicardView *view)
{
	set_empty_message (view);

	g_signal_connect (view->adapter, "drag_begin",
			  G_CALLBACK (e_minicard_view_drag_begin), view);
}

static void
e_minicard_view_set_property (GObject *object,
			      guint prop_id,
			      const GValue *value,
			      GParamSpec *pspec)
{
	EMinicardView *view;

	view = E_MINICARD_VIEW (object);

	switch (prop_id) {
	case PROP_ADAPTER:
		if (view->adapter) {
			if (view->writable_status_id || view->stop_state_id) {
				EAddressbookModel *model;
				g_object_get (view->adapter,
					      "model", &model,
					      NULL);
				if (model) {
					if (view->writable_status_id)
						g_signal_handler_disconnect (model, view->writable_status_id);
					if (view->stop_state_id)
						g_signal_handler_disconnect (model, view->stop_state_id);
				}
			}

			g_object_unref (view->adapter);
		}
		view->writable_status_id = 0;
		view->stop_state_id = 0;
		view->adapter = g_value_get_object (value);
		g_object_ref (view->adapter);
		adapter_changed (view);
		g_object_set (view,
			      "model", view->adapter,
			      NULL);
		if (view->adapter) {
			EAddressbookModel *model;
			g_object_get (view->adapter,
				      "model", &model,
				      NULL);
			if (model) {
				view->writable_status_id =
					g_signal_connect (model, "writable_status",
							  G_CALLBACK (writable_status_change), view);
				view->stop_state_id =
					g_signal_connect (model, "stop_state_changed",
							  G_CALLBACK (stop_state_changed), view);
			}

		}
		break;
	case PROP_BOOK:
		g_object_set (view->adapter,
			      "book", g_value_get_object (value),
			      NULL);
		set_empty_message (view);
		break;
	case PROP_QUERY:
		g_object_set (view->adapter,
			      "query", g_value_get_string (value),
			      NULL);
		break;
	case PROP_EDITABLE:
		g_object_set (view->adapter,
			      "editable", g_value_get_boolean (value),
			      NULL);
		set_empty_message (view);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

static void
e_minicard_view_get_property (GObject *object,
			      guint prop_id,
			      GValue *value,
			      GParamSpec *pspec)
{
	EMinicardView *view;

	view = E_MINICARD_VIEW (object);

	switch (prop_id) {
	case PROP_ADAPTER:
		g_value_set_object (value, view->adapter);
		break;
	case PROP_BOOK:
		g_object_get_property (G_OBJECT (view->adapter),
				       "book", value);
		break;
	case PROP_QUERY:
		g_object_get_property (G_OBJECT (view->adapter),
				       "query", value);
		break;
	case PROP_EDITABLE:
		g_object_get_property (G_OBJECT (view->adapter),
				       "editable", value);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

static void
e_minicard_view_dispose (GObject *object)
{
	EMinicardView *view = E_MINICARD_VIEW(object);

	clear_drag_data (view);

	if (view->canvas_drag_data_get_id) {
		g_signal_handler_disconnect (GNOME_CANVAS_ITEM (view)->canvas,
					     view->canvas_drag_data_get_id);
		view->canvas_drag_data_get_id = 0;
	}

	if (view->adapter) {
		if (view->writable_status_id || view->stop_state_id) {
			EAddressbookModel *model;
			g_object_get (view->adapter,
				      "model", &model,
				      NULL);
			if (model) {
				if (view->writable_status_id)
					g_signal_handler_disconnect (model, view->writable_status_id);
				if (view->stop_state_id)
					g_signal_handler_disconnect (model, view->stop_state_id);
			}
		}

		g_object_unref (view->adapter);
	}
	view->writable_status_id = 0;
	view->stop_state_id = 0;
	view->adapter = NULL;

	if (G_OBJECT_CLASS(parent_class)->dispose)
		G_OBJECT_CLASS(parent_class)->dispose (object);
}

static guint
e_minicard_view_right_click (EMinicardView *view, GdkEvent *event)
{
	guint ret_val = 0;
	g_signal_emit (view, signals[RIGHT_CLICK], 0,
		       event, &ret_val);
	return ret_val;
}

static gboolean
e_minicard_view_event (GnomeCanvasItem *item, GdkEvent *event)
{
	EMinicardView *view;

	view = E_MINICARD_VIEW (item);

	switch (event->type) {
	case GDK_2BUTTON_PRESS:
		if (((GdkEventButton *)event)->button == 1) {
			gboolean editable;

			g_object_get(view->adapter, "editable", &editable, NULL);

			if (editable)
				e_minicard_view_create_contact (view);
			return TRUE;
		}
	case GDK_BUTTON_PRESS:
		if (event->button.button == 3) {
			e_minicard_view_right_click (view, event);
		}
		break;
	case GDK_KEY_PRESS:
		if (event->key.keyval & GDK_SHIFT_MASK &&
			event->key.keyval == GDK_F10) {
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
e_minicard_view_selection_event (EReflow *reflow,
                                 GnomeCanvasItem *item,
                                 GdkEvent *event)
{
	EMinicardView *view;
	gint return_val = FALSE;

	view = E_MINICARD_VIEW (reflow);
	if (parent_class->selection_event) {
		return_val = parent_class->selection_event (reflow, item, event);
	}

	switch (event->type) {
	case GDK_FOCUS_CHANGE:
		if (event->focus_change.in) {
			gint i;
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
	EBookAsyncCallback cb;
	gpointer closure;
} ViewCbClosure;

static void
do_remove (gint i, gpointer user_data)
{
	EBook *book;
	EContact *contact;
	ViewCbClosure *viewcbclosure = user_data;
	EMinicardView *view = viewcbclosure->view;
	EBookAsyncCallback cb = viewcbclosure->cb;
	gpointer closure = viewcbclosure->closure;

	g_object_get (view->adapter,
		      "book", &book,
		      NULL);

	contact = e_addressbook_reflow_adapter_get_contact (view->adapter, i);

	e_book_remove_contact_async (book, contact, cb, closure);

	g_object_unref (contact);
}

#if 0
static gint
compare_to_utf_str (EMinicard *card, const gchar *utf_str)
{
	g_return_val_if_fail(card != NULL, 0);
	g_return_val_if_fail(E_IS_MINICARD(card), 0);

	if (g_unichar_isdigit (g_utf8_get_char (utf_str))) {
		return 1;
	}

	if (card->card) {
		gchar *file_as;
		g_object_get(card->card,
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
	GObjectClass *object_class;
	GnomeCanvasItemClass *item_class;
	EReflowClass *reflow_class;

	object_class = G_OBJECT_CLASS (klass);
	item_class = (GnomeCanvasItemClass *) klass;
	reflow_class = (EReflowClass *) klass;

	parent_class = g_type_class_peek_parent (klass);

	object_class->set_property    = e_minicard_view_set_property;
	object_class->get_property    = e_minicard_view_get_property;
	object_class->dispose         = e_minicard_view_dispose;

	g_object_class_install_property (object_class, PROP_ADAPTER,
					 g_param_spec_object ("adapter",
							      "Adapter",
							      NULL,
							      E_TYPE_ADDRESSBOOK_REFLOW_ADAPTER,
							      G_PARAM_READWRITE));

	g_object_class_install_property (object_class, PROP_BOOK,
					 g_param_spec_object ("book",
							      "Book",
							      NULL,
							      E_TYPE_BOOK,
							      G_PARAM_READWRITE));

	g_object_class_install_property (object_class, PROP_QUERY,
					 g_param_spec_string ("query",
							      "Query",
							      NULL,
							      NULL,
							      G_PARAM_READWRITE));

	g_object_class_install_property (object_class, PROP_EDITABLE,
					 g_param_spec_boolean ("editable",
							       "Editable",
							       NULL,
							       FALSE,
							       G_PARAM_READWRITE));

	signals[CREATE_CONTACT] =
		g_signal_new ("create-contact",
			      G_OBJECT_CLASS_TYPE (object_class),
			      G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
			      0, NULL, NULL,
			      g_cclosure_marshal_VOID__VOID,
			      G_TYPE_NONE, 0);

	signals[CREATE_CONTACT_LIST] =
		g_signal_new ("create-contact-list",
			      G_OBJECT_CLASS_TYPE (object_class),
			      G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
			      0, NULL, NULL,
			      g_cclosure_marshal_VOID__VOID,
			      G_TYPE_NONE, 0);

	signals[RIGHT_CLICK] =
		g_signal_new ("right_click",
			      G_OBJECT_CLASS_TYPE (object_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (EMinicardViewClass, right_click),
			      NULL, NULL,
			      e_marshal_INT__POINTER,
			      G_TYPE_INT, 1, G_TYPE_POINTER);

	item_class->event             = e_minicard_view_event;

	reflow_class->selection_event = e_minicard_view_selection_event;
	/* GnomeCanvasItem method overrides */

	/* init the accessibility support for e_minicard_view */
	e_minicard_view_a11y_init();
}

static void
e_minicard_view_init (EMinicardView *view)
{
	view->drag_list = NULL;
	view->adapter = NULL;
	view->canvas_drag_data_get_id = 0;
	view->writable_status_id = 0;
	view->stop_state_id = 0;

	set_empty_message (view);
}

GType
e_minicard_view_get_type (void)
{
	static GType reflow_type = 0;

	if (!reflow_type) {
		static const GTypeInfo reflow_info =  {
			sizeof (EMinicardViewClass),
			NULL,           /* base_init */
			NULL,           /* base_finalize */
			(GClassInitFunc) e_minicard_view_class_init,
			NULL,           /* class_finalize */
			NULL,           /* class_data */
			sizeof (EMinicardView),
			0,             /* n_preallocs */
			(GInstanceInitFunc) e_minicard_view_init,
		};

		reflow_type = g_type_register_static (PARENT_TYPE, "EMinicardView", &reflow_info, 0);
	}

	return reflow_type;
}

void
e_minicard_view_remove_selection(EMinicardView      *view,
				 EBookAsyncCallback  cb,
				 gpointer            closure)
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
	gchar uft_str[6 + 1];

	utf_str[g_unichar_to_utf8 (letter, utf_str)] = '\0';
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
add_to_list (gint index, gpointer closure)
{
	ModelAndList *mal = closure;
	mal->list = g_list_prepend (
		mal->list, e_addressbook_reflow_adapter_get_contact (
		mal->adapter, index));
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

void
e_minicard_view_create_contact (EMinicardView *view)
{
	g_return_if_fail (E_IS_MINICARD_VIEW (view));

	g_signal_emit (view, signals[CREATE_CONTACT], 0);
}

void
e_minicard_view_create_contact_list (EMinicardView *view)
{
	g_return_if_fail (E_IS_MINICARD_VIEW (view));

	g_signal_emit (view, signals[CREATE_CONTACT_LIST], 0);
}
