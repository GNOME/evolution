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
static void e_minicard_init		(EMinicard		 *card);
static void e_minicard_class_init	(EMinicardClass	 *klass);
static void e_minicard_set_arg (GtkObject *o, GtkArg *arg, guint arg_id);
static void e_minicard_get_arg (GtkObject *object, GtkArg *arg, guint arg_id);
static gboolean e_minicard_event (GnomeCanvasItem *item, GdkEvent *event);
static void e_minicard_realize (GnomeCanvasItem *item);
static void e_minicard_unrealize (GnomeCanvasItem *item);

static void _update_card ( EMinicard *minicard );

static GnomeCanvasGroupClass *parent_class = NULL;

/* The arguments we take */
enum {
	ARG_0,
	ARG_WIDTH,
	ARG_HEIGHT,
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
  gtk_object_add_arg_type ("EMinicard::card", GTK_TYPE_OBJECT, 
			   GTK_ARG_READWRITE, ARG_CARD);
 
  object_class->set_arg = e_minicard_set_arg;
  object_class->get_arg = e_minicard_get_arg;
  /*  object_class->destroy = e_minicard_destroy; */
  
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
	  e_minicard->width = GTK_VALUE_DOUBLE (*arg);
	  _update_card(e_minicard);
	  gnome_canvas_item_request_update (item);
	  break;
	case ARG_CARD:
	  /*	  e_minicard->card = GTK_VALUE_POINTER (*arg);
	  _update_card(e_minicard);
	  gnome_canvas_item_request_update (item);*/
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
	case ARG_CARD:
	  /* GTK_VALUE_POINTER (*arg) = e_minicard->card; */
	  break;
	default:
	  arg->type = GTK_TYPE_INVALID;
	  break;
	}
}

static void
e_minicard_realize (GnomeCanvasItem *item)
{
        double text_height;
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
				 "x", (double) 6,
				 "y", (double) 6,
				 "anchor", GTK_ANCHOR_NW,
				 "clip_width", (double) ( e_minicard->width - 12 ),
				 "clip", TRUE,
				 "use_ellipsis", TRUE,
				 "font", "lucidasans-bold-10",
				 "fill_color", "black",
				 "text", "Chris Lahey",
				 NULL );

	gtk_object_get( GTK_OBJECT( e_minicard->header_text ),
			"text_height", &text_height,
			NULL );
	
	e_minicard->height = text_height + 12.0;

	gnome_canvas_item_set( e_minicard->header_rect,
			       "y2", text_height + 9.0,
			       NULL );

	gnome_canvas_item_set( e_minicard->header_text,
			       "clip_height", text_height,
			       NULL );

	e_minicard->fields =
	  g_list_append( e_minicard->fields,
			 gnome_canvas_item_new( group,
						e_minicard_label_get_type(),
						"x", (double) 2,
						"y", e_minicard->height,
						"width", e_minicard->width - 4,
						"height", text_height + 3,
						"fieldname", "Full Name:",
						"field", "Christopher James Lahey",
						NULL ) );
	e_minicard->height += text_height + 3;

	e_minicard->fields =
	  g_list_append( e_minicard->fields,
			 gnome_canvas_item_new( group,
						e_minicard_label_get_type(),
						"x", (double) 2,
						"y", e_minicard->height,
						"width", e_minicard->width - 4.0,
						"height", text_height + 3.0,
						"fieldname", "Email:",
						"field", "clahey@helixcode.com",
						NULL ) );
	e_minicard->height += text_height + 3;
	e_minicard->height += 2;

	gnome_canvas_item_set( e_minicard->rect,
			       "y2", e_minicard->height - 1,
			       NULL );

	if (!item->canvas->aa)
	  {
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
	  }
	else
	  {
	    gnome_canvas_item_set( e_minicard->rect, 
				   "outline_color", NULL, 
				   NULL );
	    gnome_canvas_item_set( e_minicard->header_rect, 
				   "fill_color", "grey50",
				   NULL );
	    gnome_canvas_item_set( e_minicard->header_text, 
				   "fill_color", "black",
				   NULL );
	  }
      }
      break;
    default:
      break;
    }
  
  if (GNOME_CANVAS_ITEM_CLASS( parent_class )->event)
	  return (* GNOME_CANVAS_ITEM_CLASS( parent_class )->event) (item, event);
  else
	  return 0;
}

static void
_update_card( EMinicard *minicard )
{
  if ( GTK_OBJECT_FLAGS( minicard ) & GNOME_CANVAS_ITEM_REALIZED )
    {
      GList *field;
      gnome_canvas_item_set( minicard->rect,
			     "x2", (double) minicard->width - 1.0,
			     "y2", (double) minicard->height - 1.0,
			     NULL );
      gnome_canvas_item_set( minicard->header_rect,
			     "x2", (double) minicard->width - 4.0,
			     NULL );
      gnome_canvas_item_set( minicard->header_text,
			     "clip_width", (double) minicard->width - 12,
			     NULL );
      for ( field = minicard->fields; field; field = g_list_next( field ) )
	{
	  gnome_canvas_item_set( GNOME_CANVAS_ITEM( field->data ),
				 "width", minicard->width - 4.0,
				 NULL );
	}
    }
}
