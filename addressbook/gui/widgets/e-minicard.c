/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* 
 * e-minicard.c
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

#include <gnome.h>
#include "e-minicard.h"
#include "e-minicard-label.h"
#include "e-text.h"
#include "e-book.h"
#include "e-canvas.h"
#include "e-util.h"
#include "e-canvas-utils.h"
static void e_minicard_init		(EMinicard		 *card);
static void e_minicard_class_init	(EMinicardClass	 *klass);
static void e_minicard_set_arg (GtkObject *o, GtkArg *arg, guint arg_id);
static void e_minicard_get_arg (GtkObject *object, GtkArg *arg, guint arg_id);
static void e_minicard_destroy (GtkObject *object);
static gboolean e_minicard_event (GnomeCanvasItem *item, GdkEvent *event);
static void e_minicard_realize (GnomeCanvasItem *item);
static void e_minicard_unrealize (GnomeCanvasItem *item);
static void e_minicard_reflow ( GnomeCanvasItem *item, int flags );

static void e_minicard_resize_children( EMinicard *e_minicard );
static void remodel( EMinicard *e_minicard );

static GnomeCanvasGroupClass *parent_class = NULL;

/* The arguments we take */
enum {
	ARG_0,
	ARG_WIDTH,
	ARG_HEIGHT,
	ARG_HAS_FOCUS,
	ARG_CARD
};

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
  gtk_object_add_arg_type ("EMinicard::card", GTK_TYPE_OBJECT, 
			   GTK_ARG_READWRITE, ARG_CARD);
 
  object_class->set_arg = e_minicard_set_arg;
  object_class->get_arg = e_minicard_get_arg;
  object_class->destroy = e_minicard_destroy;
  
  /* GnomeCanvasItem method overrides */
  item_class->realize     = e_minicard_realize;
  item_class->unrealize   = e_minicard_unrealize;
  item_class->event       = e_minicard_event;
}

static void
e_minicard_init (EMinicard *minicard)
{
  /*   minicard->card = NULL;*/
  minicard->rect = NULL;
  minicard->fields = NULL;
  minicard->width = 10;
  minicard->height = 10;
  minicard->has_focus = FALSE;
  
  minicard->card = NULL;

  e_canvas_item_set_reflow_callback(GNOME_CANVAS_ITEM(minicard), e_minicard_reflow);
}

static void
e_minicard_set_arg (GtkObject *o, GtkArg *arg, guint arg_id)
{
	GnomeCanvasItem *item;
	EMinicard *e_minicard;

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
				gnome_canvas_item_set(GNOME_CANVAS_ITEM(e_minicard->fields->data),
						      "has_focus", GTK_VALUE_ENUM(*arg),
						      NULL);
			} else if ( GTK_VALUE_ENUM(*arg) == E_FOCUS_END ) {
				gnome_canvas_item_set(GNOME_CANVAS_ITEM(g_list_last(e_minicard->fields)->data),
						      "has_focus", GTK_VALUE_ENUM(*arg),
						      NULL);
			}
		}
		else
			e_canvas_item_grab_focus(item);
		break;
	case ARG_CARD:
		if (e_minicard->card)
			gtk_object_unref (GTK_OBJECT(e_minicard->card));
		e_minicard->card = E_CARD(GTK_VALUE_OBJECT (*arg));
		if (e_minicard->card)
			gtk_object_ref (GTK_OBJECT(e_minicard->card));
		remodel(e_minicard);
		e_canvas_item_request_reflow(item);
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
	case ARG_CARD:
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

	if (e_minicard->card)
		gtk_object_unref (GTK_OBJECT(e_minicard->card));

	if (GTK_OBJECT_CLASS (parent_class)->destroy)
		(* GTK_OBJECT_CLASS (parent_class)->destroy) (object);
}

static void
e_minicard_realize (GnomeCanvasItem *item)
{
	EMinicard *e_minicard;
	GnomeCanvasGroup *group;

	e_minicard = E_MINICARD (item);
	group = GNOME_CANVAS_GROUP( item );

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
				 "fill_color", "grey70",
				 NULL );

	e_minicard->header_text =
	  gnome_canvas_item_new( group,
				 e_text_get_type(),
				 "anchor", GTK_ANCHOR_NW,
				 "width", (double) ( e_minicard->width - 12 ),
				 "clip", TRUE,
				 "use_ellipsis", TRUE,
				 "font", "lucidasans-bold-10",
				 "fill_color", "black",
				 "text", "",
				 NULL );
	e_canvas_item_move_absolute(e_minicard->header_text, 6, 6);

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

static gboolean
e_minicard_event (GnomeCanvasItem *item, GdkEvent *event)
{
  EMinicard *e_minicard;
 
  e_minicard = E_MINICARD (item);

  switch( event->type )
    {
    case GDK_FOCUS_CHANGE:
      {
	GdkEventFocus *focus_event = (GdkEventFocus *) event;
	if ( focus_event->in )
	  {
	    gnome_canvas_item_set( e_minicard->rect, 
				   "outline_color", "grey50", 
				   NULL );
	    gnome_canvas_item_set( e_minicard->header_rect, 
				   "fill_color", "darkblue",
				   NULL );
	    gnome_canvas_item_set( e_minicard->header_text, 
				   "fill_color", "white",
				   NULL );
	    e_minicard->has_focus = TRUE;
	  }
	else
	  {
	    gnome_canvas_item_set( e_minicard->rect, 
				   "outline_color", NULL, 
				   NULL );
	    gnome_canvas_item_set( e_minicard->header_rect, 
				   "fill_color", "grey70",
				   NULL );
	    gnome_canvas_item_set( e_minicard->header_text, 
				   "fill_color", "black",
				   NULL );
	    e_minicard->has_focus = FALSE;
	  }
      }
      break;
    case GDK_BUTTON_PRESS:
	    if (event->button.button == 1) {
		    e_canvas_item_grab_focus(item);
	    }
	    break;
    case GDK_KEY_PRESS:
	    if (event->key.keyval == GDK_Tab || 
		event->key.keyval == GDK_KP_Tab || 
		event->key.keyval == GDK_ISO_Left_Tab) {
		    GList *list;
		    for (list = e_minicard->fields; list; list = list->next) {
			    GnomeCanvasItem *item = GNOME_CANVAS_ITEM (list->data);
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
					    item = GNOME_CANVAS_ITEM (list->data);
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
	if ( GTK_OBJECT_FLAGS( e_minicard ) & GNOME_CANVAS_ITEM_REALIZED ) {
		GList *list;
		
		gnome_canvas_item_set( e_minicard->header_text,
				       "width", (double) e_minicard->width - 12,
				       NULL );
		for ( list = e_minicard->fields; list; list = g_list_next( list ) ) {
			gnome_canvas_item_set( GNOME_CANVAS_ITEM( list->data ),
					       "width", (double) e_minicard->width - 4.0,
					       NULL );
		}
	}
}

static void
add_field (EMinicard *e_minicard, char *fieldname, char* field)
{
	GnomeCanvasItem *new_item;
	GnomeCanvasGroup *group;

	group = GNOME_CANVAS_GROUP( e_minicard );

	new_item = e_minicard_label_new(group);
	gnome_canvas_item_set( new_item,
			       "width", e_minicard->width - 4.0,
			       "fieldname", fieldname,
			       "field", field,
			       NULL );
	e_minicard->fields = g_list_append( e_minicard->fields, new_item);
	e_canvas_item_move_absolute(new_item, 2, e_minicard->height);
}
		

static void
remodel( EMinicard *e_minicard )
{
	if (e_minicard->card) {
		char *fname;
		ECardList *address_list;
		ECardList *phone_list;
		ECardList *email_list;

		ECardIterator *iterator;

		GList *list;

		for ( list = e_minicard->fields; list; list = g_list_next( list ) ) {
			gtk_object_destroy( GTK_OBJECT( list->data ) );
		}
		g_list_free(e_minicard->fields);
		e_minicard->fields = NULL;

		gtk_object_get(GTK_OBJECT(e_minicard->card),
		       "full_name",  &fname,
		       "address",    &address_list,
		       "phone",      &phone_list,
		       "email",      &email_list,
		       NULL);

		if (fname) {
			add_field(e_minicard, "Name:", fname);
			if (e_minicard->header_text)
				gnome_canvas_item_set(e_minicard->header_text, 
						      "text", fname,
						      NULL);
		} else
			if (e_minicard->header_text)
				gnome_canvas_item_set(e_minicard->header_text,
						      "text", "",
						      NULL);
		if (address_list) {
			for (iterator = e_card_list_get_iterator(address_list); e_card_iterator_is_valid(iterator); e_card_iterator_next(iterator)) {
				const ECardDeliveryAddress *address = e_card_iterator_get(iterator);
				if (address->flags & ADDR_WORK) {
					add_field(e_minicard, "Work Address:", address->city);
				} else if (address->flags & ADDR_HOME) {
					add_field(e_minicard, "Home Address:", address->city);
				} else {
					add_field(e_minicard, "Address:", address->city);
				}
			}
		}
		if (phone_list) {
			for (iterator = e_card_list_get_iterator(phone_list); e_card_iterator_is_valid(iterator); e_card_iterator_next(iterator)) {
				const ECardPhone *phone = e_card_iterator_get(iterator);
				if (phone->flags & E_CARD_PHONE_WORK) {
					add_field(e_minicard, "Work Phone:", phone->number);
				} else if (phone->flags & E_CARD_PHONE_HOME) {
					add_field(e_minicard, "Home Phone:", phone->number);
				} else if (phone->flags & E_CARD_PHONE_CELL) {
					add_field(e_minicard, "Mobile Phone:", phone->number);
				} else {
					add_field(e_minicard, "Phone:", phone->number);
				}
			}
		}
		if (email_list) {
			for (iterator = e_card_list_get_iterator(email_list); e_card_iterator_is_valid(iterator); e_card_iterator_next(iterator)) {
				add_field(e_minicard, "Email:", (char *) e_card_iterator_get(iterator));
			}
		}
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
			gtk_object_get (GTK_OBJECT(list->data),
					"height", &text_height,
					NULL);
			e_canvas_item_move_absolute(GNOME_CANVAS_ITEM(list->data), 2, e_minicard->height);
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

char *
e_minicard_get_card_id (EMinicard *minicard)
{
	g_return_val_if_fail(minicard != NULL, NULL);
	g_return_val_if_fail(E_IS_MINICARD(minicard), NULL);

	if (minicard->card) {
		return e_card_get_id(minicard->card);
	} else {
		return NULL;
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
		char *fname1, *fname2;
		gtk_object_get(GTK_OBJECT(minicard1->card),
			       "full_name", &fname1,
			       NULL);
		gtk_object_get(GTK_OBJECT(minicard2->card),
			       "full_name", &fname2,
			       NULL);
		return strcmp(fname1, fname2);
	} else {
		return 0;
	}
}
