/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* 
 * e-reflow.c
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
#include <math.h>
#include "e-reflow.h"
#include "e-canvas-utils.h"
#include "e-canvas.h"
#include "e-util.h"
static void e_reflow_init		(EReflow		 *card);
static void e_reflow_class_init	(EReflowClass	 *klass);
static void e_reflow_set_arg (GtkObject *o, GtkArg *arg, guint arg_id);
static void e_reflow_get_arg (GtkObject *object, GtkArg *arg, guint arg_id);
static gboolean e_reflow_event (GnomeCanvasItem *item, GdkEvent *event);
static void e_reflow_realize (GnomeCanvasItem *item);
static void e_reflow_unrealize (GnomeCanvasItem *item);
static void e_reflow_draw (GnomeCanvasItem *item, GdkDrawable *drawable,
				    int x, int y, int width, int height);
static void e_reflow_update (GnomeCanvasItem *item, double affine[6], ArtSVP *clip_path, gint flags);
static double e_reflow_point (GnomeCanvasItem *item, double x, double y, int cx, int cy, GnomeCanvasItem **actual_item);
static void e_reflow_reflow (GnomeCanvasItem *item, int flags);

static void e_reflow_resize_children (GnomeCanvasItem *item);

#define E_REFLOW_DIVIDER_WIDTH 2
#define E_REFLOW_BORDER_WIDTH 7
#define E_REFLOW_FULL_GUTTER (E_REFLOW_DIVIDER_WIDTH + E_REFLOW_BORDER_WIDTH * 2)

static GnomeCanvasGroupClass *parent_class = NULL;

/* The arguments we take */
enum {
	ARG_0,
	ARG_MINIMUM_WIDTH,
	ARG_WIDTH,
	ARG_HEIGHT
};

GtkType
e_reflow_get_type (void)
{
  static GtkType reflow_type = 0;

  if (!reflow_type)
    {
      static const GtkTypeInfo reflow_info =
      {
        "EReflow",
        sizeof (EReflow),
        sizeof (EReflowClass),
        (GtkClassInitFunc) e_reflow_class_init,
        (GtkObjectInitFunc) e_reflow_init,
        /* reserved_1 */ NULL,
        /* reserved_2 */ NULL,
        (GtkClassInitFunc) NULL,
      };

      reflow_type = gtk_type_unique (gnome_canvas_group_get_type (), &reflow_info);
    }

  return reflow_type;
}

static void
e_reflow_class_init (EReflowClass *klass)
{
  GtkObjectClass *object_class;
  GnomeCanvasItemClass *item_class;

  object_class = (GtkObjectClass*) klass;
  item_class = (GnomeCanvasItemClass *) klass;

  parent_class = gtk_type_class (gnome_canvas_group_get_type ());
  
  gtk_object_add_arg_type ("EReflow::minimum_width", GTK_TYPE_DOUBLE, 
			   GTK_ARG_READWRITE, ARG_MINIMUM_WIDTH); 
  gtk_object_add_arg_type ("EReflow::width", GTK_TYPE_DOUBLE, 
			   GTK_ARG_READABLE, ARG_WIDTH); 
  gtk_object_add_arg_type ("EReflow::height", GTK_TYPE_DOUBLE, 
			   GTK_ARG_READWRITE, ARG_HEIGHT);
 
  object_class->set_arg = e_reflow_set_arg;
  object_class->get_arg = e_reflow_get_arg;
  /*  object_class->destroy = e_reflow_destroy; */
  
  /* GnomeCanvasItem method overrides */
  item_class->event       = e_reflow_event;
  item_class->realize     = e_reflow_realize;
  item_class->unrealize   = e_reflow_unrealize;
  item_class->draw        = e_reflow_draw;
  item_class->update      = e_reflow_update;
  item_class->point       = e_reflow_point;
}

static void
e_reflow_init (EReflow *reflow)
{
  /*   reflow->card = NULL;*/
  reflow->items = NULL;
  reflow->columns = NULL;
  reflow->column_width = 150;

  reflow->minimum_width = 10;
  reflow->width = 10;
  reflow->height = 10;
  reflow->idle = 0;

  reflow->column_drag = FALSE;

  reflow->need_height_update = FALSE;
  reflow->need_column_resize = FALSE;

  reflow->default_cursor_shown = TRUE;
  reflow->arrow_cursor = NULL;
  reflow->default_cursor = NULL;

  e_canvas_item_set_reflow_callback(GNOME_CANVAS_ITEM(reflow), e_reflow_reflow);
}

static void
e_reflow_set_arg (GtkObject *o, GtkArg *arg, guint arg_id)
{
	GnomeCanvasItem *item;
	EReflow *e_reflow;

	item = GNOME_CANVAS_ITEM (o);
	e_reflow = E_REFLOW (o);
	
	switch (arg_id){
	case ARG_HEIGHT:
	  e_reflow->height = GTK_VALUE_DOUBLE (*arg);
	  e_canvas_item_request_reflow(item);
	  break;
	case ARG_MINIMUM_WIDTH:
		e_reflow->minimum_width = GTK_VALUE_DOUBLE (*arg);
		e_canvas_item_request_reflow(item);
		break;
	}
}

static void
e_reflow_get_arg (GtkObject *object, GtkArg *arg, guint arg_id)
{
	EReflow *e_reflow;

	e_reflow = E_REFLOW (object);

	switch (arg_id) {
	case ARG_MINIMUM_WIDTH:
	  GTK_VALUE_DOUBLE (*arg) = e_reflow->minimum_width;
	  break;
	case ARG_WIDTH:
	  GTK_VALUE_DOUBLE (*arg) = e_reflow->width;
	  break;
	case ARG_HEIGHT:
	  GTK_VALUE_DOUBLE (*arg) = e_reflow->height;
	  break;
	default:
	  arg->type = GTK_TYPE_INVALID;
	  break;
	}
}

static void
e_reflow_realize (GnomeCanvasItem *item)
{
	EReflow *e_reflow;
	GnomeCanvasGroup *group;
	GList *list;
	GtkAdjustment *adjustment;

	e_reflow = E_REFLOW (item);
	group = GNOME_CANVAS_GROUP( item );

	if (GNOME_CANVAS_ITEM_CLASS(parent_class)->realize)
		(* GNOME_CANVAS_ITEM_CLASS(parent_class)->realize) (item);
	
	e_reflow->arrow_cursor = gdk_cursor_new (GDK_SB_H_DOUBLE_ARROW);
	e_reflow->default_cursor = gdk_cursor_new (GDK_LEFT_PTR);

	for(list = e_reflow->items; list; list = g_list_next(list)) {
		GnomeCanvasItem *item = GNOME_CANVAS_ITEM(list->data);
		gnome_canvas_item_set(item,
				      "width", (double) e_reflow->column_width,
				      NULL);
	}

	e_canvas_item_request_reflow(item);
	
	adjustment = gtk_layout_get_hadjustment(GTK_LAYOUT(item->canvas));
	adjustment->step_increment = (e_reflow->column_width + E_REFLOW_FULL_GUTTER) / 2;
	adjustment->page_increment = adjustment->page_size - adjustment->step_increment;
	gtk_adjustment_changed(adjustment);
	
	if (!item->canvas->aa) {
	}
}

static void
e_reflow_unrealize (GnomeCanvasItem *item)
{
  EReflow *e_reflow;

  e_reflow = E_REFLOW (item);

  if (!item->canvas->aa)
    {
    }
  
  gdk_cursor_destroy (e_reflow->arrow_cursor);
  gdk_cursor_destroy (e_reflow->default_cursor);

  g_list_free (e_reflow->items);
  g_list_free (e_reflow->columns);

  if (GNOME_CANVAS_ITEM_CLASS(parent_class)->unrealize)
    (* GNOME_CANVAS_ITEM_CLASS(parent_class)->unrealize) (item);
}

static gint
e_reflow_pick_line (EReflow *e_reflow, double x)
{
	x += E_REFLOW_BORDER_WIDTH + E_REFLOW_DIVIDER_WIDTH;
	x /= e_reflow->column_width + E_REFLOW_FULL_GUTTER;
	return x;
}

static gboolean
e_reflow_event (GnomeCanvasItem *item, GdkEvent *event)
{
	EReflow *e_reflow;
 
	e_reflow = E_REFLOW (item);

	switch( event->type )
		{
		case GDK_KEY_PRESS:
			if (event->key.keyval == GDK_Tab || 
			    event->key.keyval == GDK_KP_Tab || 
			    event->key.keyval == GDK_ISO_Left_Tab) {
				GList *list;
				for (list = e_reflow->items; list; list = list->next) {
					GnomeCanvasItem *item = GNOME_CANVAS_ITEM (list->data);
					EFocus has_focus;
					gtk_object_get(GTK_OBJECT(item),
						       "has_focus", &has_focus,
						       NULL);
					if (has_focus) {
						if (event->key.state & GDK_SHIFT_MASK)
							list = list->prev;
						else
							list = list->next;
						if (list) {
							item = GNOME_CANVAS_ITEM(list->data);
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
			break;
		case GDK_BUTTON_PRESS:
			switch(event->button.button) 
				{
				case 1:
					{
						GdkEventButton *button = (GdkEventButton *) event;
						double n_x;
						n_x = button->x;
						n_x += E_REFLOW_BORDER_WIDTH + E_REFLOW_DIVIDER_WIDTH;
						n_x = fmod(n_x,(e_reflow->column_width + E_REFLOW_FULL_GUTTER));
						if ( button->y >= E_REFLOW_BORDER_WIDTH && button->y <= e_reflow->height - E_REFLOW_BORDER_WIDTH && n_x < E_REFLOW_FULL_GUTTER ) {
							e_reflow->which_column_dragged = e_reflow_pick_line(e_reflow, button->x);
							e_reflow->start_x = e_reflow->which_column_dragged * (e_reflow->column_width + E_REFLOW_FULL_GUTTER) - E_REFLOW_DIVIDER_WIDTH / 2;
							e_reflow->temp_column_width = e_reflow->column_width;
							e_reflow->column_drag = TRUE;
						  
							gnome_canvas_item_grab (item, 
										GDK_BUTTON_RELEASE_MASK | GDK_POINTER_MOTION_MASK,
										e_reflow->arrow_cursor,
										button->time);
						  
							e_reflow->previous_temp_column_width = -1;
							e_reflow->need_column_resize = TRUE;
							gnome_canvas_item_request_update(item);
							return TRUE;
						}
					}
					break;
				case 4:
					{
						GtkAdjustment *adjustment = gtk_layout_get_hadjustment(GTK_LAYOUT(item->canvas));
						gdouble new_value = adjustment->value;
						new_value -= adjustment->step_increment;
						gtk_adjustment_set_value(adjustment, new_value);
					}
					break;
				case 5: 
					{
						GtkAdjustment *adjustment = gtk_layout_get_hadjustment(GTK_LAYOUT(item->canvas));
						gdouble new_value = adjustment->value;
						new_value += adjustment->step_increment;
						if ( new_value > adjustment->upper - adjustment->page_size )
							new_value = adjustment->upper - adjustment->page_size;
						gtk_adjustment_set_value(adjustment, new_value);
					}
					break;
				}
			break;
		case GDK_BUTTON_RELEASE:
			if (e_reflow->column_drag) {
				gdouble old_width = e_reflow->column_width;
				GdkEventButton *button = (GdkEventButton *) event;
				GtkAdjustment *adjustment = gtk_layout_get_hadjustment(GTK_LAYOUT(item->canvas));
				e_reflow->temp_column_width = e_reflow->column_width +
					(button->x - e_reflow->start_x)/(e_reflow->which_column_dragged - e_reflow_pick_line(e_reflow, adjustment->value));
				if ( e_reflow->temp_column_width < 50 )
					e_reflow->temp_column_width = 50;
				e_reflow->column_drag = FALSE;
				if ( old_width != e_reflow->temp_column_width ) {
					gtk_adjustment_set_value(adjustment, adjustment->value + e_reflow_pick_line(e_reflow, adjustment->value) * (e_reflow->temp_column_width - e_reflow->column_width));
					e_reflow->column_width = e_reflow->temp_column_width;
					adjustment->step_increment = (e_reflow->column_width + E_REFLOW_FULL_GUTTER) / 2;
					adjustment->page_increment = adjustment->page_size - adjustment->step_increment;
					gtk_adjustment_changed(adjustment);
					e_reflow_resize_children(item);
					e_canvas_item_request_reflow(item);
				}
				e_reflow->need_column_resize = TRUE;
				gnome_canvas_item_request_update(item);
				gnome_canvas_item_ungrab (item, button->time);
				return TRUE;
			}
			break;
		case GDK_MOTION_NOTIFY:
			if (e_reflow->column_drag) {
				double old_width = e_reflow->temp_column_width;
				GdkEventMotion *motion = (GdkEventMotion *) event;
				GtkAdjustment *adjustment = gtk_layout_get_hadjustment(GTK_LAYOUT(item->canvas));
				e_reflow->temp_column_width = e_reflow->column_width +
					(motion->x - e_reflow->start_x)/(e_reflow->which_column_dragged - e_reflow_pick_line(e_reflow, adjustment->value));
				if (e_reflow->temp_column_width < 50)
					e_reflow->temp_column_width = 50;
				if (old_width != e_reflow->temp_column_width) {
					e_reflow->need_column_resize = TRUE;
					gnome_canvas_item_request_update(item);
				}
				return TRUE;
			} else {
				GdkEventMotion *motion = (GdkEventMotion *) event;
				double n_x;
				n_x = motion->x;
				n_x += E_REFLOW_BORDER_WIDTH + E_REFLOW_DIVIDER_WIDTH;
				n_x = fmod(n_x,(e_reflow->column_width + E_REFLOW_FULL_GUTTER));
				if ( motion->y >= E_REFLOW_BORDER_WIDTH && motion->y <= e_reflow->height - E_REFLOW_BORDER_WIDTH && n_x < E_REFLOW_FULL_GUTTER ) {
					if ( e_reflow->default_cursor_shown ) {
						gdk_window_set_cursor(GTK_WIDGET(item->canvas)->window, e_reflow->arrow_cursor);
						e_reflow->default_cursor_shown = FALSE;
					}
				} else 
					if ( ! e_reflow->default_cursor_shown ) {
						gdk_window_set_cursor(GTK_WIDGET(item->canvas)->window, e_reflow->default_cursor);
						e_reflow->default_cursor_shown = TRUE;
					}
			    
			}
			break;
		case GDK_ENTER_NOTIFY:
			if (!e_reflow->column_drag) {
				GdkEventCrossing *crossing = (GdkEventCrossing *) event;
				double n_x;
				n_x = crossing->x;
				n_x += E_REFLOW_BORDER_WIDTH + E_REFLOW_DIVIDER_WIDTH;
				n_x = fmod(n_x,(e_reflow->column_width + E_REFLOW_FULL_GUTTER));
				if ( crossing->y >= E_REFLOW_BORDER_WIDTH && crossing->y <= e_reflow->height - E_REFLOW_BORDER_WIDTH && n_x < E_REFLOW_FULL_GUTTER ) {
					if ( e_reflow->default_cursor_shown ) {
						gdk_window_set_cursor(GTK_WIDGET(item->canvas)->window, e_reflow->arrow_cursor);
						e_reflow->default_cursor_shown = FALSE;
					}
				}
			}
			break;
		case GDK_LEAVE_NOTIFY:
			if (!e_reflow->column_drag) {
				GdkEventCrossing *crossing = (GdkEventCrossing *) event;
				double n_x;
				n_x = crossing->x;
				n_x += E_REFLOW_BORDER_WIDTH + E_REFLOW_DIVIDER_WIDTH;
				n_x = fmod(n_x,(e_reflow->column_width + E_REFLOW_FULL_GUTTER));
				if ( !( crossing->y >= E_REFLOW_BORDER_WIDTH && crossing->y <= e_reflow->height - E_REFLOW_BORDER_WIDTH && n_x < E_REFLOW_FULL_GUTTER ) ) {
					if ( ! e_reflow->default_cursor_shown ) {
						gdk_window_set_cursor(GTK_WIDGET(item->canvas)->window, e_reflow->default_cursor);
						e_reflow->default_cursor_shown = TRUE;
					}
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

void
e_reflow_add_item(EReflow *e_reflow, GnomeCanvasItem *item)
{
	e_reflow->items = g_list_append(e_reflow->items, item);
	if ( GTK_OBJECT_FLAGS( e_reflow ) & GNOME_CANVAS_ITEM_REALIZED ) {
		gnome_canvas_item_set(item,
				      "width", (double) e_reflow->column_width,
				      NULL);
		e_canvas_item_request_reflow(item);
	}

}

static void e_reflow_draw (GnomeCanvasItem *item, GdkDrawable *drawable,
				    int x, int y, int width, int height)
{
	int x_rect, y_rect, width_rect, height_rect;
	gdouble running_width;
	EReflow *e_reflow = E_REFLOW(item);
	int i;
	double column_width;

	if (GNOME_CANVAS_ITEM_CLASS(parent_class)->draw)
		GNOME_CANVAS_ITEM_CLASS(parent_class)->draw (item, drawable, x, y, width, height);
	column_width = e_reflow->column_width;
	running_width = E_REFLOW_BORDER_WIDTH + column_width + E_REFLOW_BORDER_WIDTH;
	x_rect = running_width;
	y_rect = E_REFLOW_BORDER_WIDTH;
	width_rect = E_REFLOW_DIVIDER_WIDTH;
	height_rect = e_reflow->height - (E_REFLOW_BORDER_WIDTH * 2);

	/* Compute first column to draw. */
	i = x;
	i /= column_width + E_REFLOW_FULL_GUTTER;
	running_width += i * (column_width + E_REFLOW_FULL_GUTTER);

	for ( ; i < e_reflow->column_count; i++) {
		if ( running_width > x + width )
			break;
		x_rect = running_width;
		gtk_paint_flat_box(GTK_WIDGET(item->canvas)->style,
				   drawable,
				   GTK_STATE_ACTIVE,
				   GTK_SHADOW_NONE,
				   NULL,
				   GTK_WIDGET(item->canvas),
				   "reflow",
				   x_rect - x,
				   y_rect - y,
				   width_rect,
				   height_rect);
		running_width += E_REFLOW_DIVIDER_WIDTH + E_REFLOW_BORDER_WIDTH + column_width + E_REFLOW_BORDER_WIDTH;
	}
	if (e_reflow->column_drag) {
		int start_line = e_reflow_pick_line(e_reflow,
						    gtk_layout_get_hadjustment(GTK_LAYOUT(item->canvas))->value); 
		i = x - start_line * (column_width + E_REFLOW_FULL_GUTTER);
		running_width = start_line * (column_width + E_REFLOW_FULL_GUTTER);
		column_width = e_reflow->temp_column_width;
		running_width -= start_line * (column_width + E_REFLOW_FULL_GUTTER);
		i += start_line * (column_width + E_REFLOW_FULL_GUTTER);
		running_width += E_REFLOW_BORDER_WIDTH + column_width + E_REFLOW_BORDER_WIDTH;
		x_rect = running_width;
		y_rect = E_REFLOW_BORDER_WIDTH;
		width_rect = E_REFLOW_DIVIDER_WIDTH;
		height_rect = e_reflow->height - (E_REFLOW_BORDER_WIDTH * 2);

		/* Compute first column to draw. */
		i /= column_width + E_REFLOW_FULL_GUTTER;
		running_width += i * (column_width + E_REFLOW_FULL_GUTTER);
		
		for ( ; i < e_reflow->column_count; i++) {
			if ( running_width > x + width )
				break;
			x_rect = running_width;
			gdk_draw_rectangle(drawable,
					   GTK_WIDGET(item->canvas)->style->fg_gc[GTK_STATE_NORMAL],
					   TRUE,
					   x_rect - x,
					   y_rect - y,
					   width_rect - 1,
					   height_rect - 1);					   
			running_width += E_REFLOW_DIVIDER_WIDTH + E_REFLOW_BORDER_WIDTH + column_width + E_REFLOW_BORDER_WIDTH;
		}
	}
}

static void
e_reflow_update (GnomeCanvasItem *item, double affine[6], ArtSVP *clip_path, gint flags)
{
	EReflow *e_reflow;
	double x0, x1, y0, y1;

	e_reflow = E_REFLOW (item);

	if (GNOME_CANVAS_ITEM_CLASS(parent_class)->update)
		GNOME_CANVAS_ITEM_CLASS(parent_class)->update (item, affine, clip_path, flags);
	
	x0 = item->x1;
	y0 = item->y1;
	x1 = item->x2;
	y1 = item->y2;
	if ( x1 < x0 + e_reflow->width )
		x1 = x0 + e_reflow->width;
	if ( y1 < y0 + e_reflow->height )
		y1 = y0 + e_reflow->height;
	item->x2 = x1;
	item->y2 = y1;

	if (e_reflow->need_height_update) {
		x0 = item->x1;
		y0 = item->y1;
		x1 = item->x2;
		y1 = item->y2;
		if ( x0 > 0 )
			x0 = 0;
		if ( y0 > 0 )
			y0 = 0;
		if ( x1 < E_REFLOW(item)->width )
			x1 = E_REFLOW(item)->width;
		if ( x1 < E_REFLOW(item)->height )
			x1 = E_REFLOW(item)->height;

		gnome_canvas_request_redraw(item->canvas, x0, y0, x1, y1);
		e_reflow->need_height_update = FALSE;
	} else if (e_reflow->need_column_resize) {
		int x_rect, y_rect, width_rect, height_rect;
		int start_line = e_reflow_pick_line(e_reflow,
						    gtk_layout_get_hadjustment(GTK_LAYOUT(item->canvas))->value); 
		gdouble running_width;
		int i;
		double column_width;
		
		if ( e_reflow->previous_temp_column_width != -1 ) {
			running_width = start_line * (e_reflow->column_width + E_REFLOW_FULL_GUTTER);
			column_width = e_reflow->previous_temp_column_width;
			running_width -= start_line * (column_width + E_REFLOW_FULL_GUTTER);
			running_width += E_REFLOW_BORDER_WIDTH + column_width + E_REFLOW_BORDER_WIDTH;
			y_rect = E_REFLOW_BORDER_WIDTH;
			width_rect = E_REFLOW_DIVIDER_WIDTH;
			height_rect = e_reflow->height - (E_REFLOW_BORDER_WIDTH * 2);
			
			for ( i = 0; i < e_reflow->column_count; i++) {
				x_rect = running_width;
				gnome_canvas_request_redraw(item->canvas, x_rect, y_rect, x_rect + width_rect, y_rect + height_rect);
				running_width += E_REFLOW_DIVIDER_WIDTH + E_REFLOW_BORDER_WIDTH + column_width + E_REFLOW_BORDER_WIDTH;
			}
		}
		
		if ( e_reflow->temp_column_width != -1 ) {
			running_width = start_line * (e_reflow->column_width + E_REFLOW_FULL_GUTTER);
			column_width = e_reflow->temp_column_width;
			running_width -= start_line * (column_width + E_REFLOW_FULL_GUTTER);
			running_width += E_REFLOW_BORDER_WIDTH + column_width + E_REFLOW_BORDER_WIDTH;
			y_rect = E_REFLOW_BORDER_WIDTH;
			width_rect = E_REFLOW_DIVIDER_WIDTH;
			height_rect = e_reflow->height - (E_REFLOW_BORDER_WIDTH * 2);
			
			for ( i = 0; i < e_reflow->column_count; i++) {
				x_rect = running_width;
				gnome_canvas_request_redraw(item->canvas, x_rect, y_rect, x_rect + width_rect, y_rect + height_rect);
				running_width += E_REFLOW_DIVIDER_WIDTH + E_REFLOW_BORDER_WIDTH + column_width + E_REFLOW_BORDER_WIDTH;
			}
		}
		
		e_reflow->previous_temp_column_width = e_reflow->temp_column_width;
		e_reflow->need_column_resize = FALSE;
	}
}

static void
e_reflow_resize_children (GnomeCanvasItem *item)
{
	GList *list;
	EReflow *e_reflow;

	e_reflow = E_REFLOW (item);
	for ( list = e_reflow->items; list; list = list->next ) {
		GnomeCanvasItem *child = GNOME_CANVAS_ITEM(list->data);
		gnome_canvas_item_set(child,
				      "width", (double) e_reflow->column_width,
				      NULL);
	}
}

static double
e_reflow_point (GnomeCanvasItem *item,
		double x, double y, int cx, int cy,
		GnomeCanvasItem **actual_item)
{
	double distance = 1;

	if (GNOME_CANVAS_ITEM_CLASS(parent_class)->point)
		distance = GNOME_CANVAS_ITEM_CLASS(parent_class)->point (item, x, y, cx, cy, actual_item);
	if (*actual_item)
		return 0;
	
	*actual_item = item;
	return 0;
#if 0
	if (y >= E_REFLOW_BORDER_WIDTH && y <= e_reflow->height - E_REFLOW_BORDER_WIDTH) {
	        float n_x;
		n_x = x;
		n_x += E_REFLOW_BORDER_WIDTH + E_REFLOW_DIVIDER_WIDTH;
		n_x = fmod(n_x, (e_reflow->column_width + E_REFLOW_FULL_GUTTER));
		if (n_x < E_REFLOW_FULL_GUTTER) {
			*actual_item = item;
			return 0;
		}
	}
	return distance;
#endif
}

static void
_reflow( EReflow *e_reflow )
{
	gdouble running_height;
	GList *list;
	double item_height;

	if (e_reflow->columns) {
		g_list_free (e_reflow->columns);
		e_reflow->columns = NULL;
	}

	e_reflow->column_count = 0;

	if (e_reflow->items == NULL) {
		e_reflow->columns = NULL;
		e_reflow->column_count = 1;
		return;
	}

	list = e_reflow->items;
	
	gtk_object_get (GTK_OBJECT(list->data),
			"height", &item_height,
			NULL);
	running_height = E_REFLOW_BORDER_WIDTH + item_height + E_REFLOW_BORDER_WIDTH;
	e_reflow->columns = g_list_append (e_reflow->columns, list);
	e_reflow->column_count = 1;

	list = g_list_next(list);

	for ( ; list; list = g_list_next(list)) {
		gtk_object_get (GTK_OBJECT(list->data),
				"height", &item_height,
				NULL);
		if (running_height + item_height + E_REFLOW_BORDER_WIDTH > e_reflow->height) {
			running_height = E_REFLOW_BORDER_WIDTH + item_height + E_REFLOW_BORDER_WIDTH;
			e_reflow->columns = g_list_append (e_reflow->columns, list);
			e_reflow->column_count ++;
		} else {
			running_height += item_height + E_REFLOW_BORDER_WIDTH;
		}
	}
}

static void
e_reflow_reflow( GnomeCanvasItem *item, int flags )
{
	EReflow *e_reflow = E_REFLOW(item);
	if ( GTK_OBJECT_FLAGS( e_reflow ) & GNOME_CANVAS_ITEM_REALIZED ) {

		gdouble old_width;
		gdouble running_width;

		_reflow (e_reflow);
		
		old_width = e_reflow->width;
		
		running_width = E_REFLOW_BORDER_WIDTH;

		if (e_reflow->items == NULL) {
		} else {
			GList *list;
			GList *next_column;
			gdouble item_height;
			gdouble running_height;

			running_height = E_REFLOW_BORDER_WIDTH;
			
			list = e_reflow->items;
			gtk_object_get (GTK_OBJECT(list->data),
					"height", &item_height,
					NULL);
			e_canvas_item_move_absolute(GNOME_CANVAS_ITEM(list->data),
						    (double) running_width,
						    (double) running_height);
			running_height += item_height + E_REFLOW_BORDER_WIDTH;
			next_column = g_list_next(e_reflow->columns);
			list = g_list_next(list);
			
			for( ; list; list = g_list_next(list)) {
				gtk_object_get (GTK_OBJECT(list->data),
						"height", &item_height,
						NULL);

				if (next_column && (next_column->data == list)) {
					next_column = g_list_next (next_column);
					running_height = E_REFLOW_BORDER_WIDTH;
					running_width += e_reflow->column_width + E_REFLOW_BORDER_WIDTH + E_REFLOW_DIVIDER_WIDTH + E_REFLOW_BORDER_WIDTH;
				}
				e_canvas_item_move_absolute(GNOME_CANVAS_ITEM(list->data),
							    (double) running_width,
							    (double) running_height);

				running_height += item_height + E_REFLOW_BORDER_WIDTH;
			}
				 
		}
		e_reflow->width = running_width + e_reflow->column_width + E_REFLOW_BORDER_WIDTH;
		if ( e_reflow->width < e_reflow->minimum_width )
			e_reflow->width = e_reflow->minimum_width;
		if (old_width != e_reflow->width)
			e_canvas_item_request_parent_reflow(item);
	}
}
