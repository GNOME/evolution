/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* 
 * e-minicard.c
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
#include <glib.h>
#include <gtk/gtkdnd.h>
#include <gdk/gdkkeysyms.h>
#include <libgnome/gnome-defs.h>
#include <libgnome/gnome-i18n.h>
#include <libgnomeui/gnome-canvas-rect-ellipse.h>
#include <gdk-pixbuf/gnome-canvas-pixbuf.h>
#include <gal/e-text/e-text.h>
#include <gal/util/e-util.h>
#include <gal/widgets/e-canvas-utils.h>
#include <gal/widgets/e-canvas.h>
#include <gal/unicode/gunicode.h>
#include "addressbook/backend/ebook/e-book.h"
#include "e-addressbook-util.h"
#include "e-minicard.h"
#include "e-minicard-label.h"
#include "e-minicard-view.h"
#include "e-contact-editor.h"
#include "e-card-merging.h"

static void e_minicard_init		(EMinicard		 *card);
static void e_minicard_class_init	(EMinicardClass	 *klass);
static void e_minicard_set_arg (GtkObject *o, GtkArg *arg, guint arg_id);
static void e_minicard_get_arg (GtkObject *object, GtkArg *arg, guint arg_id);
static void e_minicard_destroy (GtkObject *object);
static void e_minicard_finalize (GtkObject *object);
static gboolean e_minicard_event (GnomeCanvasItem *item, GdkEvent *event);
static void e_minicard_realize (GnomeCanvasItem *item);
static void e_minicard_unrealize (GnomeCanvasItem *item);
static void e_minicard_reflow ( GnomeCanvasItem *item, int flags );

static void e_minicard_resize_children( EMinicard *e_minicard );
static void remodel( EMinicard *e_minicard );

static gint e_minicard_drag_begin (EMinicard *minicard, GdkEvent *event);

static GnomeCanvasGroupClass *parent_class = NULL;

typedef struct _EMinicardField EMinicardField;

struct _EMinicardField {
	ECardSimpleField field;
	GnomeCanvasItem *label;
};

#define d(x)

#define LIST_ICON_FILENAME "contact-list-16.png"

#define E_MINICARD_FIELD(field) ((EMinicardField *)(field))

static void
e_minicard_field_destroy(EMinicardField *field)
{
	gtk_object_destroy(GTK_OBJECT(field->label));
	g_free(field);
}

/* The arguments we take */
enum {
	ARG_0,
	ARG_WIDTH,
	ARG_HEIGHT,
	ARG_HAS_FOCUS,
	ARG_SELECTED,
	ARG_HAS_CURSOR,
	ARG_EDITABLE,
	ARG_CARD
};

enum {
	SELECTED,
	DRAG_BEGIN,
	LAST_SIGNAL
};

static guint e_minicard_signals [LAST_SIGNAL] = {0, };

GtkType
e_minicard_get_type (void)
{
  static GtkType minicard_type = 0;

  if (!minicard_type)
    {
      static const GtkTypeInfo minicard_info =
      {
        "EMinicard",
        sizeof (EMinicard),
        sizeof (EMinicardClass),
        (GtkClassInitFunc) e_minicard_class_init,
        (GtkObjectInitFunc) e_minicard_init,
        /* reserved_1 */ NULL,
        /* reserved_2 */ NULL,
        (GtkClassInitFunc) NULL,
      };

      minicard_type = gtk_type_unique (gnome_canvas_group_get_type (), &minicard_info);
    }

  return minicard_type;
}

static void
e_minicard_class_init (EMinicardClass *klass)
{
	GtkObjectClass *object_class;
	GnomeCanvasItemClass *item_class;

	object_class = (GtkObjectClass*) klass;
	item_class = (GnomeCanvasItemClass *) klass;

	parent_class = gtk_type_class (gnome_canvas_group_get_type ());
  
	gtk_object_add_arg_type ("EMinicard::width", GTK_TYPE_DOUBLE, 
				 GTK_ARG_READWRITE, ARG_WIDTH); 
	gtk_object_add_arg_type ("EMinicard::height", GTK_TYPE_DOUBLE, 
				 GTK_ARG_READABLE, ARG_HEIGHT);
	gtk_object_add_arg_type ("EMinicard::has_focus", GTK_TYPE_ENUM,
				 GTK_ARG_READWRITE, ARG_HAS_FOCUS);
	gtk_object_add_arg_type ("EMinicard::selected", GTK_TYPE_BOOL,
				 GTK_ARG_READWRITE, ARG_SELECTED);
	gtk_object_add_arg_type ("EMinicard::has_cursor", GTK_TYPE_BOOL,
				 GTK_ARG_READWRITE, ARG_HAS_CURSOR);
	gtk_object_add_arg_type ("EMinicard::editable", GTK_TYPE_BOOL,
				 GTK_ARG_READWRITE, ARG_EDITABLE);
	gtk_object_add_arg_type ("EMinicard::card", GTK_TYPE_OBJECT, 
				 GTK_ARG_READWRITE, ARG_CARD);

	e_minicard_signals [SELECTED] =
		gtk_signal_new ("selected",
				GTK_RUN_LAST,
				object_class->type,
				GTK_SIGNAL_OFFSET (EMinicardClass, selected),
				gtk_marshal_INT__POINTER,
				GTK_TYPE_INT, 1, GTK_TYPE_POINTER);

	e_minicard_signals [DRAG_BEGIN] =
		gtk_signal_new ("drag_begin",
				GTK_RUN_LAST,
				object_class->type,
				GTK_SIGNAL_OFFSET (EMinicardClass, drag_begin),
				gtk_marshal_INT__POINTER,
				GTK_TYPE_INT, 1, GTK_TYPE_POINTER);

	gtk_object_class_add_signals (object_class, e_minicard_signals, LAST_SIGNAL);
 
	object_class->set_arg  = e_minicard_set_arg;
	object_class->get_arg  = e_minicard_get_arg;
	object_class->destroy  = e_minicard_destroy;
	object_class->finalize = e_minicard_finalize;
  
	/* GnomeCanvasItem method overrides */
	item_class->realize    = e_minicard_realize;
	item_class->unrealize  = e_minicard_unrealize;
	item_class->event      = e_minicard_event;

	klass->selected        = NULL;
}

static void
e_minicard_init (EMinicard *minicard)
{
	/*   minicard->card = NULL;*/
	minicard->rect             = NULL;
	minicard->fields           = NULL;
	minicard->width            = 10;
	minicard->height           = 10;
	minicard->has_focus        = FALSE;
	minicard->selected         = FALSE;
	minicard->editable         = FALSE;
	minicard->has_cursor       = FALSE;

	minicard->card             = NULL;
	minicard->simple           = e_card_simple_new(NULL);

	minicard->list_icon_pixbuf = gdk_pixbuf_new_from_file (EVOLUTION_IMAGESDIR "/" LIST_ICON_FILENAME);
	minicard->list_icon_size   = gdk_pixbuf_get_height (minicard->list_icon_pixbuf);

	minicard->editor           = NULL;

	minicard->changed          = FALSE;

	e_canvas_item_set_reflow_callback(GNOME_CANVAS_ITEM(minicard), e_minicard_reflow);
}

static void
set_selected (EMinicard *minicard, gboolean selected)
{
	GtkWidget *canvas = GTK_WIDGET(GNOME_CANVAS_ITEM(minicard)->canvas);
	if (selected) {
		gnome_canvas_item_set (minicard->rect, 
				       "outline_color_gdk", &canvas->style->bg[GTK_STATE_NORMAL],
				       NULL);
		gnome_canvas_item_set (minicard->header_rect, 
				       "fill_color_gdk", &canvas->style->bg[GTK_STATE_SELECTED],
				       NULL);
		gnome_canvas_item_set (minicard->header_text, 
				       "fill_color_gdk", &canvas->style->text[GTK_STATE_SELECTED],
				       NULL);
	} else {
		gnome_canvas_item_set (minicard->rect, 
				       "outline_color", NULL, 
				       NULL);
		gnome_canvas_item_set (minicard->header_rect, 
				       "fill_color_gdk", &canvas->style->bg[GTK_STATE_NORMAL],
				       NULL);
		gnome_canvas_item_set (minicard->header_text, 
				       "fill_color_gdk", &canvas->style->fg[GTK_STATE_NORMAL],
				       NULL);
	}
	minicard->selected = selected;
}

static void
set_has_cursor (EMinicard *minicard, gboolean has_cursor)
{
	if (!minicard->has_focus && has_cursor)
		e_canvas_item_grab_focus(GNOME_CANVAS_ITEM (minicard), FALSE);
	minicard->has_cursor = has_cursor;
}


static void
e_minicard_set_arg (GtkObject *o, GtkArg *arg, guint arg_id)
{
	GnomeCanvasItem *item;
	EMinicard *e_minicard;
	GList *l;

	item = GNOME_CANVAS_ITEM (o);
	e_minicard = E_MINICARD (o);
	
	switch (arg_id){
	case ARG_WIDTH:
		if (e_minicard->width != GTK_VALUE_DOUBLE (*arg)) {
			e_minicard->width = GTK_VALUE_DOUBLE (*arg);
			e_minicard_resize_children(e_minicard);
			if ( GTK_OBJECT_FLAGS( e_minicard ) & GNOME_CANVAS_ITEM_REALIZED )
				e_canvas_item_request_reflow(item);
		}
	  break;
	case ARG_HAS_FOCUS:
		if (e_minicard->fields) {
			if ( GTK_VALUE_ENUM(*arg) == E_FOCUS_START ||
			     GTK_VALUE_ENUM(*arg) == E_FOCUS_CURRENT) {
				gnome_canvas_item_set(E_MINICARD_FIELD(e_minicard->fields->data)->label,
						      "has_focus", GTK_VALUE_ENUM(*arg),
						      NULL);
			} else if ( GTK_VALUE_ENUM(*arg) == E_FOCUS_END ) {
				gnome_canvas_item_set(E_MINICARD_FIELD(g_list_last(e_minicard->fields)->data)->label,
						      "has_focus", GTK_VALUE_ENUM(*arg),
						      NULL);
			}
		}
		else {
			if (!e_minicard->has_focus)
				e_canvas_item_grab_focus(item, FALSE);
		}
		break;
	case ARG_SELECTED:
		if (e_minicard->selected != GTK_VALUE_BOOL(*arg))
			set_selected (e_minicard, GTK_VALUE_BOOL(*arg));
		break;
	case ARG_EDITABLE:
		e_minicard->editable = GTK_VALUE_BOOL(*arg);
		for (l = e_minicard->fields; l; l = l->next)
			gtk_object_set (GTK_OBJECT (E_MINICARD_FIELD (l->data)->label),
					"editable", e_minicard->editable,
					NULL);
		break;
	case ARG_HAS_CURSOR:
		d(g_print("%s: ARG_HAS_CURSOR\n", __FUNCTION__));
		if (e_minicard->has_cursor != GTK_VALUE_BOOL(*arg))
			set_has_cursor (e_minicard, GTK_VALUE_BOOL(*arg));
		break;
	case ARG_CARD:
		if (e_minicard->card)
			gtk_object_unref (GTK_OBJECT(e_minicard->card));
		e_minicard->card = E_CARD(GTK_VALUE_OBJECT (*arg));
		if (e_minicard->card)
			gtk_object_ref (GTK_OBJECT(e_minicard->card));
		gtk_object_set(GTK_OBJECT(e_minicard->simple),
			       "card", e_minicard->card,
			       NULL);
		remodel(e_minicard);
		e_canvas_item_request_reflow(item);
		e_minicard->changed = FALSE;
		break;
	}
}

static void
e_minicard_get_arg (GtkObject *object, GtkArg *arg, guint arg_id)
{
	EMinicard *e_minicard;

	e_minicard = E_MINICARD (object);

	switch (arg_id) {
	case ARG_WIDTH:
	  GTK_VALUE_DOUBLE (*arg) = e_minicard->width;
	  break;
	case ARG_HEIGHT:
	  GTK_VALUE_DOUBLE (*arg) = e_minicard->height;
	  break;
	case ARG_HAS_FOCUS:
		GTK_VALUE_ENUM (*arg) = e_minicard->has_focus ? E_FOCUS_CURRENT : E_FOCUS_NONE;
		break;
	case ARG_SELECTED:
		GTK_VALUE_BOOL (*arg) = e_minicard->selected;
		break;
	case ARG_HAS_CURSOR:
		GTK_VALUE_BOOL (*arg) = e_minicard->has_cursor;
		break;
	case ARG_EDITABLE:
		GTK_VALUE_BOOL (*arg) = e_minicard->editable;
		break;
	case ARG_CARD:
		e_card_simple_sync_card(e_minicard->simple);
		GTK_VALUE_OBJECT (*arg) = GTK_OBJECT(e_minicard->card);
		break;
	default:
	  arg->type = GTK_TYPE_INVALID;
	  break;
	}
}

static void
e_minicard_destroy (GtkObject *object)
{
	EMinicard *e_minicard;

	g_return_if_fail (object != NULL);
	g_return_if_fail (E_IS_MINICARD (object));

	e_minicard = E_MINICARD (object);
	
	g_list_foreach(e_minicard->fields, (GFunc) e_minicard_field_destroy, NULL);
	g_list_free(e_minicard->fields);

	gdk_pixbuf_unref (e_minicard->list_icon_pixbuf);

	if (GTK_OBJECT_CLASS (parent_class)->destroy)
		(* GTK_OBJECT_CLASS (parent_class)->destroy) (object);
}



static void
e_minicard_finalize (GtkObject *object)
{
	EMinicard *e_minicard;

	g_return_if_fail (object != NULL);
	g_return_if_fail (E_IS_MINICARD (object));

	e_minicard = E_MINICARD (object);
	
	if (e_minicard->card)
		gtk_object_unref (GTK_OBJECT(e_minicard->card));
	if (e_minicard->simple)
		gtk_object_unref (GTK_OBJECT(e_minicard->simple));

	if (GTK_OBJECT_CLASS (parent_class)->finalize)
		(* GTK_OBJECT_CLASS (parent_class)->finalize) (object);
}

static void
e_minicard_realize (GnomeCanvasItem *item)
{
	EMinicard *e_minicard;
	GnomeCanvasGroup *group;
	GtkWidget *canvas;

	e_minicard = E_MINICARD (item);
	group = GNOME_CANVAS_GROUP( item );
	canvas = GTK_WIDGET (GNOME_CANVAS_ITEM (item)->canvas);

	if (GNOME_CANVAS_ITEM_CLASS(parent_class)->realize)
		(* GNOME_CANVAS_ITEM_CLASS(parent_class)->realize) (item);
	
	e_minicard->rect =
	  gnome_canvas_item_new( group,
				 gnome_canvas_rect_get_type(),
				 "x1", (double) 0,
				 "y1", (double) 0,
				 "x2", (double) e_minicard->width - 1,
				 "y2", (double) e_minicard->height - 1,
				 "outline_color", NULL,
				 NULL );

	e_minicard->header_rect =
	  gnome_canvas_item_new( group,
				 gnome_canvas_rect_get_type(),
				 "x1", (double) 2,
				 "y1", (double) 2,
				 "x2", (double) e_minicard->width - 3,
				 "y2", (double) e_minicard->height - 3,
				 "fill_color_gdk", &canvas->style->bg[GTK_STATE_NORMAL],
				 NULL );

	e_minicard->header_text =
	  gnome_canvas_item_new( group,
				 e_text_get_type(),
				 "anchor", GTK_ANCHOR_NW,
				 "width", (double) ( e_minicard->width - 12 ),
				 "clip", TRUE,
				 "use_ellipsis", TRUE,
#if 0
				 "font", "fixed-bold-10",
#endif
				 "fill_color_gdk", &canvas->style->fg[GTK_STATE_NORMAL],
				 "text", "",
				 "draw_background", FALSE,
				 NULL );

	e_canvas_item_move_absolute(e_minicard->header_text, 6, 6);

	e_minicard->list_icon = 
		gnome_canvas_item_new ( group,
					gnome_canvas_pixbuf_get_type(),
					"pixbuf", e_minicard->list_icon_pixbuf,
					NULL);

	remodel(e_minicard);
	e_canvas_item_request_reflow(item);

	if (!item->canvas->aa) {
	}
}

static void
e_minicard_unrealize (GnomeCanvasItem *item)
{
  EMinicard *e_minicard;

  e_minicard = E_MINICARD (item);

  if (!item->canvas->aa)
    {
    }

  if (GNOME_CANVAS_ITEM_CLASS(parent_class)->unrealize)
    (* GNOME_CANVAS_ITEM_CLASS(parent_class)->unrealize) (item);
}

static void
card_modified_cb (EBook* book, EBookStatus status, gpointer user_data)
{
	d(g_print ("%s: %s(): a card was modified\n", __FILE__, __FUNCTION__));
	if (status != E_BOOK_STATUS_SUCCESS)
		e_addressbook_error_dialog (_("Error modifying card"), status);
}

/* Callback used when the contact editor is closed */
static void
editor_closed_cb (GtkObject *editor, gpointer data)
{
	EMinicard *minicard = data;
	gtk_object_unref (GTK_OBJECT (editor));
	minicard->editor = NULL;
}

static gboolean
e_minicard_event (GnomeCanvasItem *item, GdkEvent *event)
{
	EMinicard *e_minicard;
	GtkWidget *canvas;
	
	e_minicard = E_MINICARD (item);
	canvas = GTK_WIDGET (GNOME_CANVAS_ITEM (item)->canvas);
	
	switch( event->type ) {
	case GDK_FOCUS_CHANGE:
		{
			GdkEventFocus *focus_event = (GdkEventFocus *) event;
			d(g_print("%s: GDK_FOCUS_CHANGE: %s\n", __FUNCTION__, focus_event->in?"in":"out"));
			if (focus_event->in) {
				/* Chris: When EMinicard gets the cursor, if it doesn't have the focus, it should take it.  */
				e_minicard->has_focus = TRUE;
				if (!e_minicard->selected) {
					e_minicard_selected(e_minicard, event);
				}
			} else {
				EBook *book = NULL;

				if (e_minicard->changed) {
				
					e_card_simple_sync_card(e_minicard->simple);

					if (E_IS_MINICARD_VIEW(GNOME_CANVAS_ITEM(e_minicard)->parent)) {
					
						gtk_object_get(GTK_OBJECT(GNOME_CANVAS_ITEM(e_minicard)->parent),
							       "book", &book,
							       NULL);
					
					}
				
					if (book) {
					
						/* Add the card in the contact editor to our ebook */
						e_card_merging_book_commit_card (book,
										 e_minicard->card,
										 card_modified_cb,
										 NULL);
					} else {
						remodel(e_minicard);
						e_canvas_item_request_reflow(GNOME_CANVAS_ITEM(e_minicard));
					}
					e_minicard->changed = FALSE;
				}
				e_minicard->has_focus = FALSE;
			}
		}
		break;
	case GDK_BUTTON_PRESS: {
		if (1 <= event->button.button && event->button.button <= 2) {
			int ret_val = e_minicard_selected(e_minicard, event);
			GdkEventMask mask = ((1 << (4 + event->button.button)) |
					     GDK_POINTER_MOTION_MASK |
					     GDK_BUTTON_PRESS_MASK |
					     GDK_BUTTON_RELEASE_MASK);
			
			e_canvas_item_grab_focus(item, TRUE);

			if (gnome_canvas_item_grab (GNOME_CANVAS_ITEM (e_minicard),
						    mask, NULL, event->button.time)) {
				return FALSE;
			}
			gtk_grab_add (GTK_WIDGET (GNOME_CANVAS_ITEM (e_minicard)->canvas));
			e_minicard->button_x = event->button.x;
			e_minicard->button_y = event->button.y;
			e_minicard->drag_button = event->button.button;
			e_minicard->drag_button_down = TRUE;
			return ret_val;
		} else if (event->button.button == 3) {
			int ret_val = e_minicard_selected(e_minicard, event);
			if (ret_val != 0)
				return ret_val;
		}
		break;
	}
	case GDK_BUTTON_RELEASE:
		e_minicard_selected(e_minicard, event);
		if (e_minicard->drag_button == event->button.button) {
			e_minicard->drag_button = 0;
			e_minicard->drag_button_down = FALSE;
			e_minicard->button_x = -1;
			e_minicard->button_y = -1;

			if (GTK_WIDGET_HAS_GRAB (GNOME_CANVAS_ITEM (e_minicard)->canvas)) {
				gtk_grab_remove (GTK_WIDGET (GNOME_CANVAS_ITEM (e_minicard)->canvas));
				gnome_canvas_item_ungrab (GNOME_CANVAS_ITEM (e_minicard), event->button.time);
			}
		}
		break;
	case GDK_MOTION_NOTIFY:
		if (e_minicard->drag_button_down && event->motion.state & GDK_BUTTON1_MASK) {
			if (MAX (abs (e_minicard->button_x - event->motion.x),
				 abs (e_minicard->button_y - event->motion.y)) > 3) {
				gint ret_val;

				ret_val = e_minicard_drag_begin(e_minicard, event);

				e_minicard->drag_button_down = FALSE;

				return ret_val;
			}
		}
		break;
	case GDK_2BUTTON_PRESS:
		if (event->button.button == 1 && E_IS_MINICARD_VIEW(item->parent)) {
			if (e_minicard->editor) {
				if (e_card_evolution_list (e_minicard->card))
					e_contact_list_editor_raise (E_CONTACT_LIST_EDITOR(e_minicard->editor));
				else
					e_contact_editor_raise(E_CONTACT_EDITOR(e_minicard->editor));
			} else {
				EBook *book = NULL;
				if (E_IS_MINICARD_VIEW(item->parent)) {
					gtk_object_get(GTK_OBJECT(item->parent),
						       "book", &book,
						       NULL);
				}

				if (book != NULL) {
					if (e_card_evolution_list (e_minicard->card)) {
						EContactListEditor *editor = e_addressbook_show_contact_list_editor (book, e_minicard->card,
														     FALSE, e_minicard->editable);
						e_minicard->editor = GTK_OBJECT (editor);
					}
					else {
						EContactEditor *editor = e_addressbook_show_contact_editor (book, e_minicard->card,
													    FALSE, e_minicard->editable);
						e_minicard->editor = GTK_OBJECT (editor);
					}
					gtk_object_ref (e_minicard->editor);

					gtk_signal_connect (e_minicard->editor, "editor_closed",
							    GTK_SIGNAL_FUNC (editor_closed_cb), e_minicard);

				}
			}
			return TRUE;
		}
		break;
	case GDK_KEY_PRESS:
		if (event->key.keyval == GDK_Tab || 
		    event->key.keyval == GDK_KP_Tab || 
		    event->key.keyval == GDK_ISO_Left_Tab) {
			GList *list;
			for (list = e_minicard->fields; list; list = list->next) {
				EMinicardField *field = E_MINICARD_FIELD(list->data);
				GnomeCanvasItem *item = field->label;
				EFocus has_focus;
				gtk_object_get(GTK_OBJECT(item),
					       "has_focus", &has_focus,
					       NULL);
				if (has_focus != E_FOCUS_NONE) {
					if (event->key.state & GDK_SHIFT_MASK)
						list = list->prev;
					else
						list = list->next;
					if (list) {
						EMinicardField *field = E_MINICARD_FIELD(list->data);
						GnomeCanvasItem *item = field->label;
						gnome_canvas_item_set(item,
								      "has_focus", (event->key.state & GDK_SHIFT_MASK) ? E_FOCUS_END : E_FOCUS_START,
								      NULL);
						return 1;
					} else {
						return 0;
					}
				}
			}
		}
	default:
		break;
	}
	
	if (GNOME_CANVAS_ITEM_CLASS( parent_class )->event)
		return (* GNOME_CANVAS_ITEM_CLASS( parent_class )->event) (item, event);
	else
		return 0;
}

static void
e_minicard_resize_children( EMinicard *e_minicard )
{
	GList *list;
	
	if (e_minicard->header_text) {
		gnome_canvas_item_set( e_minicard->header_text,
				       "width", ((double) e_minicard->width - 12 
						 - (e_card_evolution_list (e_minicard->card) ? e_minicard->list_icon_size : 0.0)),
				       NULL );
	}
	if (e_minicard->list_icon) {
		e_canvas_item_move_absolute(e_minicard->list_icon,
					    e_minicard->width - e_minicard->list_icon_size - 3,
					    3);
	}
	for ( list = e_minicard->fields; list; list = g_list_next( list ) ) {
		gnome_canvas_item_set( E_MINICARD_FIELD( list->data )->label,
				       "width", (double) e_minicard->width - 4.0,
				       NULL );
	}
}

static void
field_changed (EText *text, EMinicard *e_minicard)
{
	ECardSimpleType type;
	char *string;
	char *new_string;
	gboolean is_list = FALSE;

	type = GPOINTER_TO_INT
		(gtk_object_get_data(GTK_OBJECT(text),
				     "EMinicard:field"));
	gtk_object_get(GTK_OBJECT(text),
		       "text", &string,
		       NULL);

	/* 
	 * If the card is coresponding with a contact list and the field be 
	 * changed is e-mail address, should wrap it before write it back.
	 */
	new_string = (char*)e_card_simple_get_const (e_minicard->simple, 
						     E_CARD_SIMPLE_FIELD_IS_LIST); 

	is_list = (NULL != new_string);	

	if  (is_list && (E_CARD_SIMPLE_FIELD_EMAIL == type || 
	    	  E_CARD_SIMPLE_FIELD_EMAIL_2 == type ||
		      E_CARD_SIMPLE_FIELD_EMAIL_3 == type)) {
		if (string && *string) {
			EDestination *dest = e_destination_new (); 
			if (dest != NULL){
				e_destination_set_email (dest, string);
				new_string = e_destination_export(dest);
				g_free(string);
				string=new_string;
				gtk_object_unref (GTK_OBJECT (dest));
			}
		}
	}

	e_card_simple_set(e_minicard->simple,
			  type,
			  string);
	g_free(string);
	e_minicard->changed = TRUE;
}

static void
field_activated (EText *text, EMinicard *e_minicard)
{
	e_text_stop_editing (text);
	e_canvas_item_grab_focus (GNOME_CANVAS_ITEM (e_minicard), FALSE);
}

static void
add_field (EMinicard *e_minicard, ECardSimpleField field, gdouble left_width)
{
	GnomeCanvasItem *new_item;
	GnomeCanvasGroup *group;
	ECardSimpleType type;
	EMinicardField *minicard_field;
	char *name;
	char *string;
	
	group = GNOME_CANVAS_GROUP( e_minicard );
	
	type = e_card_simple_type(e_minicard->simple, field);
	name = g_strdup_printf("%s:", e_card_simple_get_name(e_minicard->simple, field));
	string = e_card_simple_get(e_minicard->simple, field);

	/* Magically convert embedded XML into an address. */
	if (!strncmp (string, "<?xml", 5)) {
		EDestination *dest = e_destination_import (string);
		if (dest != NULL) {
			gchar *new_string = g_strdup (e_destination_get_address (dest));
			g_free (string);
			string = new_string;
			gtk_object_unref (GTK_OBJECT (dest));
		}
	}

	new_item = e_minicard_label_new(group);
	gnome_canvas_item_set( new_item,
			       "width", e_minicard->width - 4.0,
			       "fieldname", name,
			       "field", string,
			       "max_field_name_length", left_width,
			       "editable", e_minicard->editable,
			       NULL );
	gtk_signal_connect(GTK_OBJECT(E_MINICARD_LABEL(new_item)->field),
			   "changed", GTK_SIGNAL_FUNC(field_changed), e_minicard);
	gtk_signal_connect(GTK_OBJECT(E_MINICARD_LABEL(new_item)->field),
			   "activate", GTK_SIGNAL_FUNC(field_activated), e_minicard);
	gtk_object_set(GTK_OBJECT(E_MINICARD_LABEL(new_item)->field),
		       "allow_newlines", e_card_simple_get_allow_newlines (e_minicard->simple, field),
		       NULL);
	gtk_object_set_data(GTK_OBJECT(E_MINICARD_LABEL(new_item)->field),
			    "EMinicard:field",
			    GINT_TO_POINTER(field));

	minicard_field = g_new(EMinicardField, 1);
	minicard_field->field = field;
	minicard_field->label = new_item;

	e_minicard->fields = g_list_append( e_minicard->fields, minicard_field);
	e_canvas_item_move_absolute(new_item, 2, e_minicard->height);
	g_free(name);
	g_free(string);
}

static gdouble
get_left_width(EMinicard *e_minicard)
{
	gchar *name;
	ECardSimpleField field;
	gdouble width = -1;
	GdkFont *font;

	font = ((GtkWidget *) ((GnomeCanvasItem *) e_minicard)->canvas)->style->font;

	for(field = E_CARD_SIMPLE_FIELD_FULL_NAME; field != E_CARD_SIMPLE_FIELD_LAST; field++) {
		gdouble this_width;
		name = g_strdup_printf("%s:", e_card_simple_get_name(e_minicard->simple, field));
		this_width = gdk_text_width(font, name, strlen(name));
		if (width < this_width)
			width = this_width;
		g_free(name);
	}
	return width;
}

static void
remodel( EMinicard *e_minicard )
{
	int count = 0;
	if ( !(GTK_OBJECT_FLAGS( e_minicard ) & GNOME_CANVAS_ITEM_REALIZED) )
		return;
	if (e_minicard->simple) {
		ECardSimpleField field;
		GList *list;
		char *file_as;
		gdouble left_width = -1;

		if (e_minicard->header_text) {
			file_as = e_card_simple_get(e_minicard->simple, E_CARD_SIMPLE_FIELD_FILE_AS);
			gnome_canvas_item_set( e_minicard->header_text,
					       "text", file_as ? file_as : "",
					       NULL );
			g_free(file_as);
		}

		if (e_minicard->card && e_card_evolution_list (e_minicard->card) ) {
			gnome_canvas_item_show (e_minicard->list_icon);
		}
		else {
			gnome_canvas_item_hide (e_minicard->list_icon);
		}

		list = e_minicard->fields;
		e_minicard->fields = NULL;

		for(field = E_CARD_SIMPLE_FIELD_FULL_NAME; field != E_CARD_SIMPLE_FIELD_LAST_SIMPLE_STRING && count < 5; field++) {
			EMinicardField *minicard_field = NULL;

			if (list)
				minicard_field = list->data;
			if (minicard_field && minicard_field->field == field) {
				GList *this_list = list;
				char *string;

				string = e_card_simple_get(e_minicard->simple, field);
				if (string && *string) {
					/* Magically convert embedded XML into an address. */
					if (!strncmp (string, "<?xml", 4)) {
						EDestination *dest = e_destination_import (string);
						if (dest != NULL) {
							gchar *new_string = g_strdup (e_destination_get_address (dest));
							g_free (string);
							string = new_string;
							gtk_object_unref (GTK_OBJECT (dest));
						}
					}

					e_minicard->fields = g_list_append(e_minicard->fields, minicard_field);
					gtk_object_set(GTK_OBJECT(minicard_field->label),
						       "field", string,
						       NULL);
					count ++;
				} else {
					e_minicard_field_destroy(minicard_field);
				}
				list = g_list_remove_link(list, this_list);
				g_list_free_1(this_list);
				g_free(string);
			} else {
				char *string;
				if (left_width == -1) {
					left_width = get_left_width(e_minicard);
				}

				string = e_card_simple_get(e_minicard->simple, field);
				if (string && *string) {
					add_field(e_minicard, field, left_width);
					count++;
				}
				g_free(string);
			}
		}

		g_list_foreach(list, (GFunc) e_minicard_field_destroy, NULL);
		g_list_free(list);
	}
}

static void
e_minicard_reflow( GnomeCanvasItem *item, int flags )
{
	EMinicard *e_minicard = E_MINICARD(item);
	if ( GTK_OBJECT_FLAGS( e_minicard ) & GNOME_CANVAS_ITEM_REALIZED ) {
		GList *list;
		gdouble text_height;
		gint old_height;
		
		old_height = e_minicard->height;

		gtk_object_get( GTK_OBJECT( e_minicard->header_text ),
				"text_height", &text_height,
				NULL );
		
		e_minicard->height = text_height + 10.0;
		
		gnome_canvas_item_set( e_minicard->header_rect,
				       "y2", text_height + 9.0,
				       NULL );
		
		for(list = e_minicard->fields; list; list = g_list_next(list)) {
			EMinicardField *field = E_MINICARD_FIELD(list->data);
			GnomeCanvasItem *item = field->label;
			gtk_object_get (GTK_OBJECT(item),
					"height", &text_height,
					NULL);
			e_canvas_item_move_absolute(item, 2, e_minicard->height);
			e_minicard->height += text_height;
		}
		e_minicard->height += 2;
		
		gnome_canvas_item_set( e_minicard->rect,
				       "y2", (double) e_minicard->height - 1,
				       NULL );
		
		gnome_canvas_item_set( e_minicard->rect,
				       "x2", (double) e_minicard->width - 1.0,
				       "y2", (double) e_minicard->height - 1.0,
				       NULL );
		gnome_canvas_item_set( e_minicard->header_rect,
				       "x2", (double) e_minicard->width - 3.0,
				       NULL );

		if (old_height != e_minicard->height)
			e_canvas_item_request_parent_reflow(item);
	}
}

const char *
e_minicard_get_card_id (EMinicard *minicard)
{
	g_return_val_if_fail(minicard != NULL, NULL);
	g_return_val_if_fail(E_IS_MINICARD(minicard), NULL);

	if (minicard->card) {
		return e_card_get_id(minicard->card);
	} else {
		return "";
	}
}

int
e_minicard_compare (EMinicard *minicard1, EMinicard *minicard2)
{
	g_return_val_if_fail(minicard1 != NULL, 0);
	g_return_val_if_fail(E_IS_MINICARD(minicard1), 0);
	g_return_val_if_fail(minicard2 != NULL, 0);
	g_return_val_if_fail(E_IS_MINICARD(minicard2), 0);

	if (minicard1->card && minicard2->card) {
		char *file_as1, *file_as2;
		gtk_object_get(GTK_OBJECT(minicard1->card),
			       "file_as", &file_as1,
			       NULL);
		gtk_object_get(GTK_OBJECT(minicard2->card),
			       "file_as", &file_as2,
			       NULL);
		if (file_as1 && file_as2)
			return g_utf8_collate(file_as1, file_as2);
		if (file_as1)
			return -1;
		if (file_as2)
			return 1;
		return strcmp(e_minicard_get_card_id(minicard1), e_minicard_get_card_id(minicard2));
	} else {
		return 0;
	}
}

int
e_minicard_selected (EMinicard *minicard, GdkEvent *event)
{
	gint ret_val = 0;
	GnomeCanvasItem *item = GNOME_CANVAS_ITEM (minicard);
	if (item->parent) {
		guint signal_id = gtk_signal_lookup ("selection_event", GTK_OBJECT_TYPE (item->parent));
		/* We should probably check the signature here, but I
		 * don't think it's worth the time required to code
		 * it.
		 */
		if (signal_id != 0) {
			gtk_signal_emit(GTK_OBJECT(item->parent),
					signal_id,
					item, event, &ret_val);
		}
	}
	return ret_val;
}

static gint
e_minicard_drag_begin (EMinicard *minicard, GdkEvent *event)
{
	gint ret_val = 0;
	GnomeCanvasItem *parent;
	gtk_signal_emit (GTK_OBJECT(minicard),
			 e_minicard_signals[DRAG_BEGIN],
			 event, &ret_val);

	parent = GNOME_CANVAS_ITEM (minicard)->parent;
	if (parent && E_IS_REFLOW (parent)) {
		E_REFLOW (parent)->maybe_in_drag = FALSE;
	}
	return ret_val;
}
