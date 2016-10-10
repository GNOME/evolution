/*
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 *
 *
 * Authors:
 *		Chris Lahey <clahey@ximian.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#include "evolution-config.h"

#include "e-minicard-view.h"

#include <string.h>

#include <gtk/gtk.h>
#include <glib/gi18n.h>
#include <gdk/gdkkeysyms.h>

#include "e-util/e-util.h"

#include "eab-gui-util.h"
#include "util/eab-book-util.h"

#include "ea-addressbook.h"

static void e_minicard_view_drag_data_get (GtkWidget *widget,
					  GdkDragContext *context,
					  GtkSelectionData *selection_data,
					  guint info,
					  guint time,
					  EMinicardView *view);

enum {
	PROP_0,
	PROP_ADAPTER,
	PROP_CLIENT,
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

G_DEFINE_TYPE (EMinicardView, e_minicard_view, E_TYPE_REFLOW)

static void
e_minicard_view_drag_data_get (GtkWidget *widget,
                              GdkDragContext *context,
                              GtkSelectionData *selection_data,
                              guint info,
                              guint time,
                              EMinicardView *view)
{
	GdkAtom target;

	if (!E_IS_MINICARD_VIEW (view))
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
		EBookClient *book_client = NULL;
		gchar *value;

		g_object_get (view->adapter, "client", &book_client, NULL);
		value = eab_book_and_contact_list_to_string (book_client, view->drag_list);

		gtk_selection_data_set (
			selection_data, target, 8,
			(guchar *) value, strlen (value));

		g_object_unref (book_client);
		g_free (value);
		break;
	}
	}
}

static void
clear_drag_data (EMinicardView *view)
{
	g_slist_free_full (view->drag_list, (GDestroyNotify) g_object_unref);
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

	target_list = gtk_target_list_new (drag_types, G_N_ELEMENTS (drag_types));

	context = gtk_drag_begin (
		GTK_WIDGET (GNOME_CANVAS_ITEM (view)->canvas),
		target_list, actions, 1/*XXX */, event);

	if (!view->canvas_drag_data_get_id)
		view->canvas_drag_data_get_id = g_signal_connect (
			GNOME_CANVAS_ITEM (view)->canvas, "drag_data_get",
			G_CALLBACK (e_minicard_view_drag_data_get), view);

	gtk_drag_set_icon_default (context);

	return TRUE;
}

static void
set_empty_message (EMinicardView *view)
{
	gchar *empty_message;
	gboolean editable = FALSE, perform_initial_query = FALSE, searching = FALSE;

	if (view->adapter) {
		EAddressbookModel *model = NULL;
		EBookClient *book_client = NULL;

		g_object_get (
			view->adapter,
			"editable", &editable,
			"model", &model,
			"client", &book_client,
			NULL);

		if (book_client && !e_client_check_capability (E_CLIENT (book_client), "do-initial-query"))
			perform_initial_query = TRUE;

		searching = model && e_addressbook_model_can_stop (model);

		if (book_client)
			g_object_unref (book_client);
		if (model)
			g_object_unref (model);
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

	g_object_set (
		view,
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

	g_signal_connect (
		view->adapter, "drag_begin",
		G_CALLBACK (e_minicard_view_drag_begin), view);
}

static void
e_minicard_view_set_property (GObject *object,
                              guint property_id,
                              const GValue *value,
                              GParamSpec *pspec)
{
	EMinicardView *view;

	view = E_MINICARD_VIEW (object);

	switch (property_id) {
	case PROP_ADAPTER:
		if (view->adapter) {
			if (view->writable_status_id || view->stop_state_id) {
				EAddressbookModel *model;
				g_object_get (
					view->adapter,
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
		g_object_set (
			view,
			"model", view->adapter,
			NULL);
		if (view->adapter) {
			EAddressbookModel *model;
			g_object_get (
				view->adapter,
				"model", &model,
				NULL);
			if (model) {
				view->writable_status_id = g_signal_connect (
					model, "writable_status",
					G_CALLBACK (writable_status_change), view);
				view->stop_state_id = g_signal_connect (
					model, "stop_state_changed",
					G_CALLBACK (stop_state_changed), view);
			}

		}
		break;
	case PROP_CLIENT:
		g_object_set (
			view->adapter,
			"client", g_value_get_object (value),
			NULL);
		set_empty_message (view);
		break;
	case PROP_QUERY:
		g_object_set (
			view->adapter,
			"query", g_value_get_string (value),
			NULL);
		break;
	case PROP_EDITABLE:
		g_object_set (
			view->adapter,
			"editable", g_value_get_boolean (value),
			NULL);
		set_empty_message (view);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
		break;
	}
}

static void
e_minicard_view_get_property (GObject *object,
                              guint property_id,
                              GValue *value,
                              GParamSpec *pspec)
{
	EMinicardView *view;

	view = E_MINICARD_VIEW (object);

	switch (property_id) {
	case PROP_ADAPTER:
		g_value_set_object (value, view->adapter);
		break;
	case PROP_CLIENT:
		g_object_get_property (
			G_OBJECT (view->adapter),
			"client", value);
		break;
	case PROP_QUERY:
		g_object_get_property (
			G_OBJECT (view->adapter),
			"query", value);
		break;
	case PROP_EDITABLE:
		g_object_get_property (
			G_OBJECT (view->adapter),
			"editable", value);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
		break;
	}
}

static void
e_minicard_view_dispose (GObject *object)
{
	EMinicardView *view = E_MINICARD_VIEW (object);

	clear_drag_data (view);

	if (view->canvas_drag_data_get_id) {
		g_signal_handler_disconnect (
			GNOME_CANVAS_ITEM (view)->canvas,
			view->canvas_drag_data_get_id);
		view->canvas_drag_data_get_id = 0;
	}

	if (view->adapter) {
		if (view->writable_status_id || view->stop_state_id) {
			EAddressbookModel *model;
			g_object_get (
				view->adapter,
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

	/* Chain up to parent's dispose() method. */
	G_OBJECT_CLASS (e_minicard_view_parent_class)->dispose (object);
}

static guint
e_minicard_view_right_click (EMinicardView *view,
                             GdkEvent *event)
{
	guint ret_val = 0;
	g_signal_emit (
		view, signals[RIGHT_CLICK], 0,
		event, &ret_val);
	return ret_val;
}

static gboolean
e_minicard_view_event (GnomeCanvasItem *item,
                       GdkEvent *event)
{
	EMinicardView *view;
	guint event_button = 0;

	view = E_MINICARD_VIEW (item);

	switch (event->type) {
	case GDK_2BUTTON_PRESS:
		gdk_event_get_button (event, &event_button);
		if (event_button == 1) {
			gboolean editable;

			g_object_get (view->adapter, "editable", &editable, NULL);

			if (editable)
				e_minicard_view_create_contact (view);
			return TRUE;
		}
	case GDK_BUTTON_PRESS:
		gdk_event_get_button (event, &event_button);
		if (event_button == 3)
			e_minicard_view_right_click (view, event);
		break;
	case GDK_KEY_PRESS:
		if (((event->key.state & GDK_SHIFT_MASK) != 0 && event->key.keyval == GDK_KEY_F10) ||
		    ((event->key.state & (GDK_SHIFT_MASK | GDK_CONTROL_MASK | GDK_MOD1_MASK)) == 0 && event->key.keyval == GDK_KEY_Menu)) {
			e_minicard_view_right_click (view, event);
		}
		break;
	default:
		break;
	}

	return GNOME_CANVAS_ITEM_CLASS (e_minicard_view_parent_class)->
		event (item, event);
}

static gint
e_minicard_view_selection_event (EReflow *reflow,
                                 GnomeCanvasItem *item,
                                 GdkEvent *event)
{
	EMinicardView *view;
	gint return_val = FALSE;

	view = E_MINICARD_VIEW (reflow);
	return_val = E_REFLOW_CLASS (e_minicard_view_parent_class)->
		selection_event (reflow, item, event);

	switch (event->type) {
	case GDK_FOCUS_CHANGE:
		if (event->focus_change.in) {
			gint i;
			for (i = 0; i < reflow->count; i++) {
				if (reflow->items[i] == item) {
					e_selection_model_maybe_do_something (reflow->selection, i, 0, 0);
					break;
				}
			}
		}
		break;
	case GDK_BUTTON_PRESS:
		if (event->button.button == 3) {
			return_val = e_minicard_view_right_click (view, event);
			if (!return_val)
				e_selection_model_right_click_up (reflow->selection);
		}
		break;
	default:
		break;
	}
	return return_val;
}

static void
e_minicard_view_class_init (EMinicardViewClass *class)
{
	GObjectClass *object_class;
	GnomeCanvasItemClass *item_class;
	EReflowClass *reflow_class;

	object_class = G_OBJECT_CLASS (class);
	item_class = (GnomeCanvasItemClass *) class;
	reflow_class = (EReflowClass *) class;

	object_class->set_property = e_minicard_view_set_property;
	object_class->get_property = e_minicard_view_get_property;
	object_class->dispose = e_minicard_view_dispose;

	g_object_class_install_property (
		object_class,
		PROP_ADAPTER,
		g_param_spec_object (
			"adapter",
			"Adapter",
			NULL,
			E_TYPE_ADDRESSBOOK_REFLOW_ADAPTER,
			G_PARAM_READWRITE));

	g_object_class_install_property (
		object_class,
		PROP_CLIENT,
		g_param_spec_object (
			"client",
			"EBookClient",
			NULL,
			E_TYPE_BOOK_CLIENT,
			G_PARAM_READWRITE));

	g_object_class_install_property (
		object_class,
		PROP_QUERY,
		g_param_spec_string (
			"query",
			"Query",
			NULL,
			NULL,
			G_PARAM_READWRITE));

	g_object_class_install_property (
		object_class,
		PROP_EDITABLE,
		g_param_spec_boolean (
			"editable",
			"Editable",
			NULL,
			FALSE,
			G_PARAM_READWRITE));

	signals[CREATE_CONTACT] = g_signal_new (
		"create-contact",
		G_OBJECT_CLASS_TYPE (object_class),
		G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
		0, NULL, NULL,
		g_cclosure_marshal_VOID__VOID,
		G_TYPE_NONE, 0);

	signals[CREATE_CONTACT_LIST] = g_signal_new (
		"create-contact-list",
		G_OBJECT_CLASS_TYPE (object_class),
		G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
		0, NULL, NULL,
		g_cclosure_marshal_VOID__VOID,
		G_TYPE_NONE, 0);

	signals[RIGHT_CLICK] = g_signal_new (
		"right_click",
		G_OBJECT_CLASS_TYPE (object_class),
		G_SIGNAL_RUN_LAST,
		G_STRUCT_OFFSET (EMinicardViewClass, right_click),
		NULL, NULL,
		e_marshal_INT__POINTER,
		G_TYPE_INT, 1,
		G_TYPE_POINTER);

	item_class->event = e_minicard_view_event;

	reflow_class->selection_event = e_minicard_view_selection_event;
	/* GnomeCanvasItem method overrides */

	/* init the accessibility support for e_minicard_view */
	e_minicard_view_a11y_init ();
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

void
e_minicard_view_jump_to_letter (EMinicardView *view,
                                gunichar letter)
{
#if 0
	gchar uft_str[6 + 1];

	utf_str[g_unichar_to_utf8 (letter, utf_str)] = '\0';
	e_reflow_sorted_jump (
		E_REFLOW_SORTED (view),
		(GCompareFunc) compare_to_utf_str,
		utf_str);
#endif
}

typedef struct {
	GSList *list;
	EAddressbookReflowAdapter *adapter;
} ModelAndList;

static void
add_to_list (gint index,
             gpointer closure)
{
	ModelAndList *mal = closure;
	mal->list = g_slist_prepend (
		mal->list, e_addressbook_reflow_adapter_get_contact (
		mal->adapter, index));
}

GSList *
e_minicard_view_get_card_list (EMinicardView *view)
{
	ModelAndList mal;

	mal.adapter = view->adapter;
	mal.list = NULL;

	e_selection_model_foreach (E_REFLOW (view)->selection, add_to_list, &mal);

	return g_slist_reverse (mal.list);
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
