/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* 
 * e-minicard-label.c
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
#include "e-minicard-label.h"
#include "e-text.h"
static void e_minicard_label_init		(EMinicardLabel		 *card);
static void e_minicard_label_class_init	(EMinicardLabelClass	 *klass);
static void e_minicard_label_set_arg (GtkObject *o, GtkArg *arg, guint arg_id);
static void e_minicard_label_get_arg (GtkObject *object, GtkArg *arg, guint arg_id);
static gboolean e_minicard_label_event (GnomeCanvasItem *item, GdkEvent *event);
static void e_minicard_label_realize (GnomeCanvasItem *item);
static void e_minicard_label_unrealize (GnomeCanvasItem *item);

static void _update_label( EMinicardLabel *minicard_label );
static void _resize( GtkObject *object, gpointer data );

static GnomeCanvasGroupClass *parent_class = NULL;

enum {
	E_MINICARD_LABEL_RESIZE,
	E_MINICARD_LABEL_LAST_SIGNAL
};

static guint e_minicard_label_signals[E_MINICARD_LABEL_LAST_SIGNAL] = { 0 };

/* The arguments we take */
enum {
	ARG_0,
	ARG_WIDTH,
	ARG_HEIGHT,
	ARG_FIELD,
	ARG_FIELDNAME
};

GtkType
e_minicard_label_get_type (void)
{
  static GtkType minicard_label_type = 0;

  if (!minicard_label_type)
    {
      static const GtkTypeInfo minicard_label_info =
      {
        "EMinicardLabel",
        sizeof (EMinicardLabel),
        sizeof (EMinicardLabelClass),
        (GtkClassInitFunc) e_minicard_label_class_init,
        (GtkObjectInitFunc) e_minicard_label_init,
        /* reserved_1 */ NULL,
        /* reserved_2 */ NULL,
        (GtkClassInitFunc) NULL,
      };

      minicard_label_type = gtk_type_unique (gnome_canvas_group_get_type (), &minicard_label_info);
    }

  return minicard_label_type;
}

static void
e_minicard_label_class_init (EMinicardLabelClass *klass)
{
  GtkObjectClass *object_class;
  GnomeCanvasItemClass *item_class;

  object_class = (GtkObjectClass*) klass;
  item_class = (GnomeCanvasItemClass *) klass;

  parent_class = gtk_type_class (gnome_canvas_group_get_type ());
  
  e_minicard_label_signals[E_MINICARD_LABEL_RESIZE] =
	  gtk_signal_new ("resize",
			  GTK_RUN_LAST,
			  object_class->type,
			  GTK_SIGNAL_OFFSET (EMinicardLabelClass, resize),
			  gtk_marshal_NONE__NONE,
			  GTK_TYPE_NONE, 0);
  
  
  gtk_object_class_add_signals (object_class, e_minicard_label_signals, E_MINICARD_LABEL_LAST_SIGNAL);
  
  gtk_object_add_arg_type ("EMinicardLabel::width", GTK_TYPE_DOUBLE, 
			   GTK_ARG_READWRITE, ARG_WIDTH); 
  gtk_object_add_arg_type ("EMinicardLabel::height", GTK_TYPE_DOUBLE, 
			   GTK_ARG_READABLE, ARG_HEIGHT);
  gtk_object_add_arg_type ("EMinicardLabel::field", GTK_TYPE_STRING, 
			   GTK_ARG_READWRITE, ARG_FIELD);
  gtk_object_add_arg_type ("EMinicardLabel::fieldname", GTK_TYPE_STRING, 
			   GTK_ARG_READWRITE, ARG_FIELDNAME);

  klass->resize = NULL;
 
  object_class->set_arg = e_minicard_label_set_arg;
  object_class->get_arg = e_minicard_label_get_arg;
  /*  object_class->destroy = e_minicard_label_destroy; */
  
  /* GnomeCanvasItem method overrides */
  item_class->realize     = e_minicard_label_realize;
  item_class->unrealize   = e_minicard_label_unrealize;
  item_class->event       = e_minicard_label_event;
}

static void
e_minicard_label_init (EMinicardLabel *minicard_label)
{
  GnomeCanvasGroup *group = GNOME_CANVAS_GROUP( minicard_label );
  minicard_label->width = 10;
  minicard_label->height = 10;
  minicard_label->rect = NULL;
  minicard_label->fieldname = NULL;
  minicard_label->field = NULL;
  minicard_label->fieldname_text = NULL;
  minicard_label->field_text = NULL;
}

static void
e_minicard_label_set_arg (GtkObject *o, GtkArg *arg, guint arg_id)
{
	GnomeCanvasItem *item;
	EMinicardLabel *e_minicard_label;

	item = GNOME_CANVAS_ITEM (o);
	e_minicard_label = E_MINICARD_LABEL (o);
	
	switch (arg_id){
	case ARG_WIDTH:
	  e_minicard_label->width = GTK_VALUE_DOUBLE (*arg);
	  _update_label( e_minicard_label );
	  gnome_canvas_item_request_update (item);
	  break;
	case ARG_FIELD:
	  if ( e_minicard_label->field )
	    gnome_canvas_item_set( e_minicard_label->field, "text", GTK_VALUE_STRING (*arg), NULL );
	  else
	    e_minicard_label->field_text = g_strdup( GTK_VALUE_STRING (*arg) );
	  break;
	case ARG_FIELDNAME:
	  if ( e_minicard_label->fieldname )
	    gnome_canvas_item_set( e_minicard_label->fieldname, "text", GTK_VALUE_STRING (*arg), NULL );
	  else
	    e_minicard_label->fieldname_text = g_strdup( GTK_VALUE_STRING (*arg) );
	  break;
	}
}

static void
e_minicard_label_get_arg (GtkObject *object, GtkArg *arg, guint arg_id)
{
	EMinicardLabel *e_minicard_label;
	char *temp;

	e_minicard_label = E_MINICARD_LABEL (object);

	switch (arg_id) {
	case ARG_WIDTH:
	  GTK_VALUE_DOUBLE (*arg) = e_minicard_label->width;
	  break;
	case ARG_HEIGHT:
	  GTK_VALUE_DOUBLE (*arg) = e_minicard_label->height;
	  break;
	case ARG_FIELD:
	  if ( e_minicard_label->field )
	    {
	      gtk_object_get( GTK_OBJECT( e_minicard_label->field ), "text", &temp, NULL );
	      GTK_VALUE_STRING (*arg) = temp;
	    }
	  else
	    GTK_VALUE_STRING (*arg) = g_strdup( e_minicard_label->field_text );
	  break;
	case ARG_FIELDNAME:
	  if ( e_minicard_label->fieldname )
	    {
	      gtk_object_get( GTK_OBJECT( e_minicard_label->fieldname ), "text", &temp, NULL );
	      GTK_VALUE_STRING (*arg) = temp;
	    }
	  else
	    GTK_VALUE_STRING (*arg) = g_strdup( e_minicard_label->fieldname_text );
	  break;
	default:
	  arg->type = GTK_TYPE_INVALID;
	  break;
	}
}

static void
e_minicard_label_realize (GnomeCanvasItem *item)
{
        double ascent, descent;
	EMinicardLabel *e_minicard_label;
	GnomeCanvasGroup *group;

	e_minicard_label = E_MINICARD_LABEL (item);
	group = GNOME_CANVAS_GROUP( item );

	if (GNOME_CANVAS_ITEM_CLASS( parent_class )->realize)
	  (* GNOME_CANVAS_ITEM_CLASS( parent_class )->realize) (item);

	e_minicard_label->rect =
	  gnome_canvas_item_new( group,
				 gnome_canvas_rect_get_type(),
				 "x1", (double) 0,
				 "y1", (double) 0,
				 "x2", (double) e_minicard_label->width - 1,
				 "y2", (double) e_minicard_label->height - 1,
				 "outline_color", NULL,
				 NULL );
	e_minicard_label->fieldname =
	  gnome_canvas_item_new( group,
				 e_text_get_type(),
				 "x", (double) 2,
				 "y", (double) 1,
				 "anchor", GTK_ANCHOR_NW,
				 "clip_width", (double) ( e_minicard_label->width / 2 - 4 ),
				 "clip_height", (double) 1,
				 "clip", TRUE,
				 "use_ellipsis", TRUE,
				 "font", "lucidasans-10",
				 "fill_color", "black",
				 NULL );
	if ( e_minicard_label->fieldname_text )
	  {
	    gnome_canvas_item_set( e_minicard_label->fieldname,
				   "text", e_minicard_label->fieldname_text,
				   NULL );
	    g_free( e_minicard_label->fieldname_text );
	  }
	gtk_signal_connect(GTK_OBJECT(e_minicard_label->fieldname),
			   "resize",
			   GTK_SIGNAL_FUNC(_resize),
			   (gpointer) e_minicard_label);

	e_minicard_label->field =
	  gnome_canvas_item_new( group,
				 e_text_get_type(),
				 "x", (double) ( e_minicard_label->width / 2 + 2 ),
				 "y", (double) 1,
				 "anchor", GTK_ANCHOR_NW,
				 "clip_width", (double) ( ( e_minicard_label->width + 1 ) / 2 - 4 ),
				 "clip_height", (double) 1,
				 "clip", TRUE,
				 "use_ellipsis", TRUE,
				 "font", "lucidasans-10",
				 "fill_color", "black",
				 "editable", TRUE,
				 NULL );
	if ( e_minicard_label->field_text )
	  {
	    gnome_canvas_item_set( e_minicard_label->field,
				   "text", e_minicard_label->field_text,
				   NULL );
	    g_free( e_minicard_label->field_text );
	  }

	gtk_signal_connect(GTK_OBJECT(e_minicard_label->field),
			   "resize",
			   GTK_SIGNAL_FUNC(_resize),
			   (gpointer) e_minicard_label);

	_update_label (e_minicard_label);
	
	if (!item->canvas->aa)
	  {
	  }

}

static void
e_minicard_label_unrealize (GnomeCanvasItem *item)
{
  EMinicardLabel *e_minicard_label;

  e_minicard_label = E_MINICARD_LABEL (item);

  if (!item->canvas->aa)
    {
    }

  if (GNOME_CANVAS_ITEM_CLASS( parent_class )->unrealize)
    (* GNOME_CANVAS_ITEM_CLASS( parent_class )->unrealize) (item);
}

static gboolean
e_minicard_label_event (GnomeCanvasItem *item, GdkEvent *event)
{
  EMinicardLabel *e_minicard_label;
 
  e_minicard_label = E_MINICARD_LABEL (item);

  switch( event->type )
    {
    case GDK_FOCUS_CHANGE:
      {
	GdkEventFocus *focus_event = (GdkEventFocus *) event;
	if ( focus_event->in )
	  {
	    gnome_canvas_item_set( e_minicard_label->rect, 
				   "outline_color", "grey50", 
				   "fill_color", "grey90",
				   NULL );
	  }
	else
	  {
	    gnome_canvas_item_set( e_minicard_label->rect, 
				   "outline_color", NULL, 
				   "fill_color", NULL,
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
_update_label( EMinicardLabel *e_minicard_label )
{
  if ( GTK_OBJECT_FLAGS( e_minicard_label ) & GNOME_CANVAS_ITEM_REALIZED )
    {
	    gint old_height;
	    gdouble text_height;
	    old_height = e_minicard_label->height;

	    gtk_object_get(GTK_OBJECT(e_minicard_label->fieldname), 
			   "text_height", &text_height,
			   NULL);
	    gnome_canvas_item_set(e_minicard_label->fieldname,
				  "clip_height", (double) text_height,
				  NULL);

	    e_minicard_label->height = text_height;


	    gtk_object_get(GTK_OBJECT(e_minicard_label->field), 
			   "text_height", &text_height,
			   NULL);
	    gnome_canvas_item_set(e_minicard_label->field,
				  "clip_height", (double) text_height,
				  NULL);

	    if (e_minicard_label->height < text_height)
		    e_minicard_label->height = text_height;
	    e_minicard_label->height += 3;

	    gnome_canvas_item_set( e_minicard_label->rect,
				   "x2", (double) e_minicard_label->width - 1,
				   "y2", (double) e_minicard_label->height - 1,
				   NULL );
	    gnome_canvas_item_set( e_minicard_label->fieldname,
				   "clip_width", (double) ( e_minicard_label->width / 2 - 4 ),
				   NULL );
	    gnome_canvas_item_set( e_minicard_label->field,
				   "x", (double) ( e_minicard_label->width / 2 + 2 ),
				   "clip_width", (double) ( ( e_minicard_label->width + 1 ) / 2 - 4 ),
				   NULL );

	    if (old_height != e_minicard_label->height)
		    gtk_signal_emit_by_name (GTK_OBJECT (e_minicard_label), "resize");
	    
    }
}



static void 
_resize( GtkObject *object, gpointer data )
{
	_update_label(E_MINICARD_LABEL(data));
}
