/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* 
 * e-canvas.c
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
#include "e-canvas.h"
static void e_canvas_init		(ECanvas		 *card);
static void e_canvas_class_init	(ECanvasClass	 *klass);
static gint e_canvas_key            (GtkWidget        *widget,
				     GdkEventKey      *event);

static int emit_event (GnomeCanvas *canvas, GdkEvent *event);

static GnomeCanvasClass *parent_class = NULL;

GtkType
e_canvas_get_type (void)
{
  static GtkType canvas_type = 0;

  if (!canvas_type)
    {
      static const GtkTypeInfo canvas_info =
      {
        "ECanvas",
        sizeof (ECanvas),
        sizeof (ECanvasClass),
        (GtkClassInitFunc) e_canvas_class_init,
        (GtkObjectInitFunc) e_canvas_init,
        /* reserved_1 */ NULL,
        /* reserved_2 */ NULL,
        (GtkClassInitFunc) NULL,
      };

      canvas_type = gtk_type_unique (gnome_canvas_get_type (), &canvas_info);
    }

  return canvas_type;
}

static void
e_canvas_class_init (ECanvasClass *klass)
{
	GtkObjectClass *object_class;
	GnomeCanvasClass *canvas_class;
	GtkWidgetClass *widget_class;
	
	object_class = (GtkObjectClass*) klass;
	canvas_class = (GnomeCanvasClass *) klass;
	widget_class = (GtkWidgetClass *) klass;
	
	parent_class = gtk_type_class (gnome_canvas_get_type ());

	widget_class->key_press_event = e_canvas_key;
	widget_class->key_release_event = e_canvas_key;
}

static void
e_canvas_init (ECanvas *canvas)
{
}

GtkWidget *
e_canvas_new()
{
	return GTK_WIDGET (gtk_type_new (e_canvas_get_type ()));
}


/* Returns whether the item is an inferior of or is equal to the parent. */
static int
is_descendant (GnomeCanvasItem *item, GnomeCanvasItem *parent)
{
	for (; item; item = item->parent)
		if (item == parent)
			return TRUE;

	return FALSE;
}

/* Emits an event for an item in the canvas, be it the current item, grabbed
 * item, or focused item, as appropriate.
 */
static int
emit_event (GnomeCanvas *canvas, GdkEvent *event)
{
	GdkEvent ev;
	gint finished;
	GnomeCanvasItem *item;
	GnomeCanvasItem *parent;
	guint mask;

	/* Perform checks for grabbed items */

	if (canvas->grabbed_item && !is_descendant (canvas->current_item, canvas->grabbed_item))
		return FALSE;

	if (canvas->grabbed_item) {
		switch (event->type) {
		case GDK_ENTER_NOTIFY:
			mask = GDK_ENTER_NOTIFY_MASK;
			break;

		case GDK_LEAVE_NOTIFY:
			mask = GDK_LEAVE_NOTIFY_MASK;
			break;

		case GDK_MOTION_NOTIFY:
			mask = GDK_POINTER_MOTION_MASK;
			break;

		case GDK_BUTTON_PRESS:
		case GDK_2BUTTON_PRESS:
		case GDK_3BUTTON_PRESS:
			mask = GDK_BUTTON_PRESS_MASK;
			break;

		case GDK_BUTTON_RELEASE:
			mask = GDK_BUTTON_RELEASE_MASK;
			break;

		case GDK_KEY_PRESS:
			mask = GDK_KEY_PRESS_MASK;
			break;

		case GDK_KEY_RELEASE:
			mask = GDK_KEY_RELEASE_MASK;
			break;

		default:
			mask = 0;
			break;
		}

		if (!(mask & canvas->grabbed_event_mask))
			return FALSE;
	}

	/* Convert to world coordinates -- we have two cases because of diferent
	 * offsets of the fields in the event structures.
	 */

	ev = *event;

	switch (ev.type) {
	case GDK_ENTER_NOTIFY:
	case GDK_LEAVE_NOTIFY:
		gnome_canvas_window_to_world (canvas,
					      ev.crossing.x, ev.crossing.y,
					      &ev.crossing.x, &ev.crossing.y);
		break;

	case GDK_MOTION_NOTIFY:
	case GDK_BUTTON_PRESS:
	case GDK_2BUTTON_PRESS:
	case GDK_3BUTTON_PRESS:
	case GDK_BUTTON_RELEASE:
		gnome_canvas_window_to_world (canvas,
					      ev.motion.x, ev.motion.y,
					      &ev.motion.x, &ev.motion.y);
		break;

	default:
		break;
	}

	/* Choose where we send the event */

	item = canvas->current_item;

	if (canvas->focused_item
	    && ((event->type == GDK_KEY_PRESS) || (event->type == GDK_KEY_RELEASE) || (event->type == GDK_FOCUS_CHANGE)))
		item = canvas->focused_item;

	/* The event is propagated up the hierarchy (for if someone connected to
	 * a group instead of a leaf event), and emission is stopped if a
	 * handler returns TRUE, just like for GtkWidget events.
	 */

	finished = FALSE;

	while (item && !finished) {
		gtk_object_ref (GTK_OBJECT (item));

		gtk_signal_emit_by_name (GTK_OBJECT (item), "event",
					 &ev,
					 &finished);

		if (GTK_OBJECT_DESTROYED (item))
			finished = TRUE;

		parent = item->parent;
		gtk_object_unref (GTK_OBJECT (item));

		item = parent;
	}

	return finished;
}

/* Key event handler for the canvas */
static gint
e_canvas_key (GtkWidget *widget, GdkEventKey *event)
{
	GnomeCanvas *canvas;

	g_return_val_if_fail (widget != NULL, FALSE);
	g_return_val_if_fail (GNOME_IS_CANVAS (widget), FALSE);
	g_return_val_if_fail (event != NULL, FALSE);

	canvas = GNOME_CANVAS (widget);

	return emit_event (canvas, (GdkEvent *) event);
}

