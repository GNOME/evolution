/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* 
 * e-canvas.c
 * Copyright 2000, 2001, Ximian, Inc.
 *
 * Authors:
 *   Chris Lahey <clahey@ximian.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License, version 2, as published by the Free Software Foundation.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
 * 02111-1307, USA.
 */

#include <config.h>

#include <gtk/gtk.h>

#include "gal/util/e-util.h"

#include "e-canvas.h"

static void e_canvas_init           (ECanvas         *card);
static void e_canvas_dispose        (GObject        *object);
static void e_canvas_class_init	    (ECanvasClass    *klass);
static void e_canvas_realize        (GtkWidget        *widget);
static void e_canvas_unrealize      (GtkWidget        *widget);
static gint e_canvas_key            (GtkWidget        *widget,
				     GdkEventKey      *event);
static gint e_canvas_button         (GtkWidget        *widget,
				     GdkEventButton   *event);

static gint e_canvas_visibility     (GtkWidget        *widget,
				     GdkEventVisibility *event,
				     ECanvas          *canvas);

static gint e_canvas_focus_in       (GtkWidget        *widget,
				     GdkEventFocus    *event);
static gint e_canvas_focus_out      (GtkWidget        *widget,
				     GdkEventFocus    *event);

static void e_canvas_style_set      (GtkWidget        *widget,
				     GtkStyle         *previous_style);

static int emit_event (GnomeCanvas *canvas, GdkEvent *event);

#define PARENT_TYPE GNOME_TYPE_CANVAS
static GnomeCanvasClass *parent_class = NULL;

#define d(x)

enum {
	REFLOW,
	LAST_SIGNAL
};

static guint e_canvas_signals [LAST_SIGNAL] = { 0, };

E_MAKE_TYPE (e_canvas,
	     "ECanvas",
	     ECanvas,
	     e_canvas_class_init,
	     e_canvas_init,
	     PARENT_TYPE)

static void
e_canvas_class_init (ECanvasClass *klass)
{
	GObjectClass *object_class;
	GnomeCanvasClass *canvas_class;
	GtkWidgetClass *widget_class;

	object_class                       = (GObjectClass*) klass;
	canvas_class                       = (GnomeCanvasClass *) klass;
	widget_class                       = (GtkWidgetClass *) klass;

	parent_class                       = g_type_class_ref (PARENT_TYPE);

	object_class->dispose              = e_canvas_dispose;

	widget_class->key_press_event      = e_canvas_key;
	widget_class->key_release_event    = e_canvas_key;
	widget_class->button_press_event   = e_canvas_button;
	widget_class->button_release_event = e_canvas_button;
	widget_class->focus_in_event       = e_canvas_focus_in;
	widget_class->focus_out_event      = e_canvas_focus_out;
	widget_class->style_set            = e_canvas_style_set;
	widget_class->realize              = e_canvas_realize;
	widget_class->unrealize            = e_canvas_unrealize;

	klass->reflow                      = NULL;

	e_canvas_signals [REFLOW] =
		g_signal_new ("reflow",
			      G_OBJECT_CLASS_TYPE (object_class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (ECanvasClass, reflow),
			      NULL, NULL,
			      e_marshal_NONE__NONE,
			      G_TYPE_NONE, 0);
}

static void
e_canvas_init (ECanvas *canvas)
{
	canvas->selection = NULL;
	canvas->cursor = NULL;
	canvas->im_context = gtk_im_multicontext_new ();
	canvas->tooltip_window = NULL;
}

static void
e_canvas_dispose (GObject *object)
{
	ECanvas *canvas = E_CANVAS(object);

	if (canvas->idle_id)
		g_source_remove(canvas->idle_id);
	canvas->idle_id = 0;

	if (canvas->grab_cancelled_check_id)
		g_source_remove (canvas->grab_cancelled_check_id);
	canvas->grab_cancelled_check_id = 0;

	if (canvas->toplevel) {
		if (canvas->visibility_notify_id)
			g_signal_handler_disconnect (canvas->toplevel,
						     canvas->visibility_notify_id);
		canvas->visibility_notify_id = 0;

		g_object_unref (canvas->toplevel);
		canvas->toplevel = NULL;
	}

	if (canvas->im_context) {
		g_object_unref (canvas->im_context);
		canvas->im_context = NULL;
	}

	e_canvas_hide_tooltip(canvas);

	if ((G_OBJECT_CLASS (parent_class))->dispose)
		(*(G_OBJECT_CLASS (parent_class))->dispose) (object);
}

GtkWidget *
e_canvas_new ()
{
	return GTK_WIDGET (g_object_new (E_CANVAS_TYPE, NULL));
}


/* Emits an event for an item in the canvas, be it the current item, grabbed
 * item, or focused item, as appropriate.
 */
static int
emit_event (GnomeCanvas *canvas, GdkEvent *event)
{
	GdkEvent *ev;
	gint finished;
	GnomeCanvasItem *item;
	GnomeCanvasItem *parent;
	guint mask;

	/* Choose where we send the event */

	item = canvas->current_item;

	if (canvas->focused_item
	    && ((event->type == GDK_KEY_PRESS) || (event->type == GDK_KEY_RELEASE) || (event->type == GDK_FOCUS_CHANGE)))
		item = canvas->focused_item;

	if (canvas->grabbed_item)
		item = canvas->grabbed_item;

	/* Perform checks for grabbed items */

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

	ev = gdk_event_copy (event);

	switch (ev->type) {
	case GDK_ENTER_NOTIFY:
	case GDK_LEAVE_NOTIFY:
		gnome_canvas_window_to_world (canvas,
					      ev->crossing.x, ev->crossing.y,
					      &ev->crossing.x, &ev->crossing.y);
		break;

	case GDK_MOTION_NOTIFY:
	case GDK_BUTTON_PRESS:
	case GDK_2BUTTON_PRESS:
	case GDK_3BUTTON_PRESS:
	case GDK_BUTTON_RELEASE:
		gnome_canvas_window_to_world (canvas,
					      ev->motion.x, ev->motion.y,
					      &ev->motion.x, &ev->motion.y);
		break;

	default:
		break;
	}

	/* The event is propagated up the hierarchy (for if someone connected to
	 * a group instead of a leaf event), and emission is stopped if a
	 * handler returns TRUE, just like for GtkWidget events.
	 */

	finished = FALSE;

	while (item && !finished) {
		g_object_ref (item);

		g_signal_emit_by_name (item, "event", ev, &finished);

		parent = item->parent;
		g_object_unref (item);

		item = parent;
	}

	gdk_event_free (ev);

	return finished;
}

/* Key event handler for the canvas */
static gint
e_canvas_key (GtkWidget *widget, GdkEventKey *event)
{
	GnomeCanvas *canvas;
	GdkEvent full_event;

	g_return_val_if_fail (widget != NULL, FALSE);
	g_return_val_if_fail (GNOME_IS_CANVAS (widget), FALSE);
	g_return_val_if_fail (event != NULL, FALSE);

	canvas = GNOME_CANVAS (widget);
	
	full_event.key = *event;

	return emit_event (canvas, &full_event);
}


/* This routine invokes the point method of the item.  The argument x, y should
 * be in the parent's item-relative coordinate system.  This routine applies the
 * inverse of the item's transform, maintaining the affine invariant.
 */
#define HACKISH_AFFINE

static double
gnome_canvas_item_invoke_point (GnomeCanvasItem *item, double x, double y, int cx, int cy,
				GnomeCanvasItem **actual_item)
{
#ifdef HACKISH_AFFINE
	double i2w[6], w2c[6], i2c[6], c2i[6];
	ArtPoint c, i;
#endif

#ifdef HACKISH_AFFINE
	gnome_canvas_item_i2w_affine (item, i2w);
	gnome_canvas_w2c_affine (item->canvas, w2c);
	art_affine_multiply (i2c, i2w, w2c);
	art_affine_invert (c2i, i2c);
	c.x = cx;
	c.y = cy;
	art_affine_point (&i, &c, c2i);
	x = i.x;
	y = i.y;
#endif

	return (* GNOME_CANVAS_ITEM_CLASS (GTK_OBJECT_GET_CLASS (item))->point) (
		item, x, y, cx, cy, actual_item);
}

/* Re-picks the current item in the canvas, based on the event's coordinates.
 * Also emits enter/leave events for items as appropriate.
 */
#define DISPLAY_X1(canvas) (GNOME_CANVAS (canvas)->layout.xoffset)
#define DISPLAY_Y1(canvas) (GNOME_CANVAS (canvas)->layout.yoffset)
static int
pick_current_item (GnomeCanvas *canvas, GdkEvent *event)
{
	int button_down;
	double x, y;
	int cx, cy;
	int retval;

	retval = FALSE;

	/* If a button is down, we'll perform enter and leave events on the
	 * current item, but not enter on any other item.  This is more or less
	 * like X pointer grabbing for canvas items.
	 */
	button_down = canvas->state & (GDK_BUTTON1_MASK
				       | GDK_BUTTON2_MASK
				       | GDK_BUTTON3_MASK
				       | GDK_BUTTON4_MASK
				       | GDK_BUTTON5_MASK);
	d(g_print ("%s:%d: button_down = %s\n", __FUNCTION__, __LINE__, button_down ? "TRUE" : "FALSE"));
	if (!button_down)
		canvas->left_grabbed_item = FALSE;

	/* Save the event in the canvas.  This is used to synthesize enter and
	 * leave events in case the current item changes.  It is also used to
	 * re-pick the current item if the current one gets deleted.  Also,
	 * synthesize an enter event.
	 */
	if (event != &canvas->pick_event) {
		if ((event->type == GDK_MOTION_NOTIFY) || (event->type == GDK_BUTTON_RELEASE)) {
			/* these fields have the same offsets in both types of events */

			canvas->pick_event.crossing.type       = GDK_ENTER_NOTIFY;
			canvas->pick_event.crossing.window     = event->motion.window;
			canvas->pick_event.crossing.send_event = event->motion.send_event;
			canvas->pick_event.crossing.subwindow  = NULL;
			canvas->pick_event.crossing.x          = event->motion.x;
			canvas->pick_event.crossing.y          = event->motion.y;
			canvas->pick_event.crossing.mode       = GDK_CROSSING_NORMAL;
			canvas->pick_event.crossing.detail     = GDK_NOTIFY_NONLINEAR;
			canvas->pick_event.crossing.focus      = FALSE;
			canvas->pick_event.crossing.state      = event->motion.state;

			/* these fields don't have the same offsets in both types of events */

			if (event->type == GDK_MOTION_NOTIFY) {
				canvas->pick_event.crossing.x_root = event->motion.x_root;
				canvas->pick_event.crossing.y_root = event->motion.y_root;
			} else {
				canvas->pick_event.crossing.x_root = event->button.x_root;
				canvas->pick_event.crossing.y_root = event->button.y_root;
			}
		} else
			canvas->pick_event = *event;
	}

	/* Don't do anything else if this is a recursive call */

	if (canvas->in_repick)
		return retval;

	/* LeaveNotify means that there is no current item, so we don't look for one */

	if (canvas->pick_event.type != GDK_LEAVE_NOTIFY) {
		/* these fields don't have the same offsets in both types of events */

		if (canvas->pick_event.type == GDK_ENTER_NOTIFY) {
			x = canvas->pick_event.crossing.x + canvas->scroll_x1 - canvas->zoom_xofs;
			y = canvas->pick_event.crossing.y + canvas->scroll_y1 - canvas->zoom_yofs;
		} else {
			x = canvas->pick_event.motion.x + canvas->scroll_x1 - canvas->zoom_xofs;
			y = canvas->pick_event.motion.y + canvas->scroll_y1 - canvas->zoom_yofs;
		}

		/* canvas pixel coords */

		cx = (int) (x + 0.5);
		cy = (int) (y + 0.5);

		/* world coords */

		x = canvas->scroll_x1 + x / canvas->pixels_per_unit;
		y = canvas->scroll_y1 + y / canvas->pixels_per_unit;

		/* find the closest item */

		if (canvas->root->object.flags & GNOME_CANVAS_ITEM_VISIBLE)
			gnome_canvas_item_invoke_point (canvas->root, x, y, cx, cy,
							&canvas->new_current_item);
		else
			canvas->new_current_item = NULL;
	} else
		canvas->new_current_item = NULL;

	if ((canvas->new_current_item == canvas->current_item) && !canvas->left_grabbed_item)
		return retval; /* current item did not change */

	/* Synthesize events for old and new current items */

	if ((canvas->new_current_item != canvas->current_item)
	    && (canvas->current_item != NULL)
	    && !canvas->left_grabbed_item) {
		GdkEvent new_event;
		GnomeCanvasItem *item;

		item = canvas->current_item;

		new_event = canvas->pick_event;
		new_event.type = GDK_LEAVE_NOTIFY;

		new_event.crossing.detail = GDK_NOTIFY_ANCESTOR;
		new_event.crossing.subwindow = NULL;
		canvas->in_repick = TRUE;
		retval = emit_event (canvas, &new_event);
		canvas->in_repick = FALSE;
	}

	/* new_current_item may have been set to NULL during the call to emit_event() above */

	if ((canvas->new_current_item != canvas->current_item) && button_down) {
		canvas->left_grabbed_item = TRUE;
		return retval;
	}

	/* Handle the rest of cases */

	canvas->left_grabbed_item = FALSE;
	canvas->current_item = canvas->new_current_item;

	if (canvas->current_item != NULL) {
		GdkEvent new_event;

		new_event = canvas->pick_event;
		new_event.type = GDK_ENTER_NOTIFY;
		new_event.crossing.detail = GDK_NOTIFY_ANCESTOR;
		new_event.crossing.subwindow = NULL;
		retval = emit_event (canvas, &new_event);
	}

	return retval;
}

/* Button event handler for the canvas */
static gint
e_canvas_button (GtkWidget *widget, GdkEventButton *event)
{
	GnomeCanvas *canvas;
	int mask;
	int retval;

	g_return_val_if_fail (widget != NULL, FALSE);
	g_return_val_if_fail (GNOME_IS_CANVAS (widget), FALSE);
	g_return_val_if_fail (event != NULL, FALSE);

	retval = FALSE;

	canvas = GNOME_CANVAS (widget);

	d(g_print ("button %d, event type %d, grabbed=%p, current=%p\n",
		   event->button,
		   event->type,
		   canvas->grabbed_item,
		   canvas->current_item));

        /* dispatch normally regardless of the event's window if an item has
           has a pointer grab in effect */
	if (!canvas->grabbed_item && event->window != canvas->layout.bin_window)
		return retval;

	switch (event->button) {
	case 1:
		mask = GDK_BUTTON1_MASK;
		break;
	case 2:
		mask = GDK_BUTTON2_MASK;
		break;
	case 3:
		mask = GDK_BUTTON3_MASK;
		break;
	case 4:
		mask = GDK_BUTTON4_MASK;
		break;
	case 5:
		mask = GDK_BUTTON5_MASK;
		break;
	default:
		mask = 0;
	}

	switch (event->type) {
	case GDK_BUTTON_PRESS:
	case GDK_2BUTTON_PRESS:
	case GDK_3BUTTON_PRESS:
		/* Pick the current item as if the button were not pressed, and
		 * then process the event.
		 */
		canvas->state = event->state;
		pick_current_item (canvas, (GdkEvent *) event);
		canvas->state ^= mask;
		retval = emit_event (canvas, (GdkEvent *) event);
		break;

	case GDK_BUTTON_RELEASE:
		/* Process the event as if the button were pressed, then repick
		 * after the button has been released
		 */
		canvas->state = event->state;
		retval = emit_event (canvas, (GdkEvent *) event);
		event->state ^= mask;
		canvas->state = event->state;
		pick_current_item (canvas, (GdkEvent *) event);
		event->state ^= mask;
		break;

	default:
		g_assert_not_reached ();
	}

	return retval;
}

/* Key event handler for the canvas */
static gint
e_canvas_visibility (GtkWidget *widget, GdkEventVisibility *event, ECanvas *canvas)
{
	if (! canvas->visibility_first) {
		e_canvas_hide_tooltip(canvas);
	}
	canvas->visibility_first = FALSE;

	return FALSE;
}


/**
 * e_canvas_item_grab_focus:
 * @item: A canvas item.
 * @widget_too: Whether or not to grab the widget-level focus too
 *
 * Makes the specified item take the keyboard focus, so all keyboard
 * events will be sent to it. If the canvas widget itself did not have
 * the focus and @widget_too is %TRUE, it grabs that focus as well.
 **/
void
e_canvas_item_grab_focus (GnomeCanvasItem *item, gboolean widget_too)
{
	GnomeCanvasItem *focused_item;
	GdkEvent ev;

	g_return_if_fail (item != NULL);
	g_return_if_fail (GNOME_IS_CANVAS_ITEM (item));
	g_return_if_fail (GTK_WIDGET_CAN_FOCUS (GTK_WIDGET (item->canvas)));

	focused_item = item->canvas->focused_item;

	if (focused_item) {
		ev.focus_change.type = GDK_FOCUS_CHANGE;
		ev.focus_change.window = GTK_LAYOUT (item->canvas)->bin_window;
		ev.focus_change.send_event = FALSE;
		ev.focus_change.in = FALSE;

		emit_event (item->canvas, &ev);
	}

	item->canvas->focused_item = item;

	if (widget_too && !GTK_WIDGET_HAS_FOCUS (GTK_WIDGET(item->canvas))) {
		gtk_widget_grab_focus (GTK_WIDGET (item->canvas));
	}

	if (item) {
		ev.focus_change.type = GDK_FOCUS_CHANGE;
		ev.focus_change.window = GTK_LAYOUT (item->canvas)->bin_window;
		ev.focus_change.send_event = FALSE;
		ev.focus_change.in = TRUE;

		emit_event (item->canvas, &ev);
	}
}

/* Focus in handler for the canvas */
static gint
e_canvas_focus_in (GtkWidget *widget, GdkEventFocus *event)
{
	GnomeCanvas *canvas;
	ECanvas *ecanvas;
	GdkEvent full_event;

	canvas = GNOME_CANVAS (widget);
	ecanvas = E_CANVAS (widget);

	GTK_WIDGET_SET_FLAGS (widget, GTK_HAS_FOCUS);

	gtk_im_context_focus_in (ecanvas->im_context);

	if (canvas->focused_item) {
		full_event.focus_change = *event;
		return emit_event (canvas, &full_event);
	} else {
		return FALSE;
	}
}

/* Focus out handler for the canvas */
static gint
e_canvas_focus_out (GtkWidget *widget, GdkEventFocus *event)
{
	GnomeCanvas *canvas;
	ECanvas *ecanvas;
	GdkEvent full_event;

	canvas = GNOME_CANVAS (widget);
	ecanvas = E_CANVAS (widget);

	GTK_WIDGET_UNSET_FLAGS (widget, GTK_HAS_FOCUS);

	gtk_im_context_focus_out (ecanvas->im_context);

	if (canvas->focused_item) {
		full_event.focus_change = *event;
		return emit_event (canvas, &full_event);
	} else {
		return FALSE;
	}
}

static void
ec_style_set_recursive (GnomeCanvasItem *item, GtkStyle *previous_style)
{
	guint signal_id = g_signal_lookup ("style_set", G_OBJECT_TYPE (item));
	if (signal_id >= 1) {
		GSignalQuery query;
		g_signal_query (signal_id, &query);
		if (query.return_type == GTK_TYPE_NONE && query.n_params == 1 && query.param_types[0] == GTK_TYPE_STYLE) {
			g_signal_emit (item, signal_id, 0, previous_style);
		}
	}

	if (GNOME_IS_CANVAS_GROUP (item) ) {
		GList *items = GNOME_CANVAS_GROUP (item)->item_list;
		for (; items; items = items->next)
			ec_style_set_recursive (items->data, previous_style);
	}
}

static void
e_canvas_style_set (GtkWidget *widget, GtkStyle *previous_style)
{
	ec_style_set_recursive (GNOME_CANVAS_ITEM (gnome_canvas_root (GNOME_CANVAS (widget))), previous_style);
}


static void
e_canvas_realize (GtkWidget *widget)
{
	ECanvas *ecanvas = E_CANVAS (widget);

	if (GTK_WIDGET_CLASS (parent_class)->realize)
		(* GTK_WIDGET_CLASS (parent_class)->realize) (widget);

	gdk_window_set_back_pixmap (GTK_LAYOUT (widget)->bin_window, NULL, FALSE);

	gtk_im_context_set_client_window (ecanvas->im_context, widget->window);
}

static void
e_canvas_unrealize (GtkWidget *widget)
{
	ECanvas * ecanvas = E_CANVAS (widget);

	if (ecanvas->idle_id) {
		g_source_remove(ecanvas->idle_id);
		ecanvas->idle_id = 0;
	}

	gtk_im_context_set_client_window (ecanvas->im_context, widget->window);

	if (GTK_WIDGET_CLASS (parent_class)->unrealize)
		(* GTK_WIDGET_CLASS (parent_class)->unrealize) (widget);
}

static void
e_canvas_item_invoke_reflow (GnomeCanvasItem *item, int flags)
{
	GnomeCanvasGroup *group;
	GList *list;
	GnomeCanvasItem *child;

	if (GNOME_IS_CANVAS_GROUP (item)) {
		group = GNOME_CANVAS_GROUP (item);
		for (list = group->item_list; list; list = list->next) {
			child = GNOME_CANVAS_ITEM (list->data);
			if (child->object.flags & E_CANVAS_ITEM_DESCENDENT_NEEDS_REFLOW)
				e_canvas_item_invoke_reflow (child, flags);
		}
	}

	if (item->object.flags & E_CANVAS_ITEM_NEEDS_REFLOW) {
		ECanvasItemReflowFunc func;
		func = (ECanvasItemReflowFunc)
			g_object_get_data (G_OBJECT (item),
					   "ECanvasItem::reflow_callback");
		if (func)
			func (item, flags);
	}

	item->object.flags &= ~E_CANVAS_ITEM_NEEDS_REFLOW;
	item->object.flags &= ~E_CANVAS_ITEM_DESCENDENT_NEEDS_REFLOW;
}

static void
do_reflow (ECanvas *canvas)
{
	if (GNOME_CANVAS(canvas)->root->object.flags & E_CANVAS_ITEM_DESCENDENT_NEEDS_REFLOW)
		e_canvas_item_invoke_reflow (GNOME_CANVAS(canvas)->root, 0);
}

/* Idle handler for the e-canvas.  It deals with pending reflows. */
static gint
idle_handler (gpointer data)
{
	ECanvas *canvas;

	GDK_THREADS_ENTER();

	canvas = E_CANVAS (data);
	do_reflow (canvas);

	/* Reset idle id */
	canvas->idle_id = 0;

	g_signal_emit (canvas,
		       e_canvas_signals [REFLOW], 0);

	GDK_THREADS_LEAVE();

	return FALSE;
}

/* Convenience function to add an idle handler to a canvas */
static void
add_idle (ECanvas *canvas)
{
	if (canvas->idle_id != 0)
		return;

	canvas->idle_id = g_idle_add_full (G_PRIORITY_HIGH_IDLE, idle_handler, (gpointer) canvas, NULL);
}

static void
e_canvas_item_descendent_needs_reflow (GnomeCanvasItem *item)
{
	if (item->object.flags & E_CANVAS_ITEM_DESCENDENT_NEEDS_REFLOW)
		return;

	item->object.flags |= E_CANVAS_ITEM_DESCENDENT_NEEDS_REFLOW;
	if (item->parent)
		e_canvas_item_descendent_needs_reflow(item->parent);
}

void
e_canvas_item_request_reflow (GnomeCanvasItem *item)
{
	if (item->object.flags & GNOME_CANVAS_ITEM_REALIZED) {
		item->object.flags |= E_CANVAS_ITEM_NEEDS_REFLOW;
		e_canvas_item_descendent_needs_reflow(item);
		add_idle(E_CANVAS(item->canvas));
	}
}

void
e_canvas_item_request_parent_reflow (GnomeCanvasItem *item)
{
	g_return_if_fail(item != NULL);
	g_return_if_fail(GNOME_IS_CANVAS_ITEM(item));
	e_canvas_item_request_reflow(item->parent);
}

void
e_canvas_item_set_reflow_callback (GnomeCanvasItem *item, ECanvasItemReflowFunc func)
{
	g_object_set_data(G_OBJECT(item), "ECanvasItem::reflow_callback", (gpointer) func);
}


void
e_canvas_item_set_selection_callback (GnomeCanvasItem *item, ECanvasItemSelectionFunc func)
{
	g_object_set_data(G_OBJECT(item), "ECanvasItem::selection_callback", (gpointer) func);
}

void
e_canvas_item_set_selection_compare_callback (GnomeCanvasItem *item, ECanvasItemSelectionCompareFunc func)
{
	g_object_set_data(G_OBJECT(item), "ECanvasItem::selection_compare_callback", (gpointer) func);
}

void
e_canvas_item_set_cursor (GnomeCanvasItem *item, gpointer id)
{
	GList *list;
	int flags;
	ECanvas *canvas;
	ECanvasSelectionInfo *info;
	ECanvasItemSelectionFunc func;

	g_return_if_fail(item != NULL);
	g_return_if_fail(GNOME_IS_CANVAS_ITEM(item));
	g_return_if_fail(item->canvas != NULL);
	g_return_if_fail(E_IS_CANVAS(item->canvas));

	canvas = E_CANVAS(item->canvas);
	flags = E_CANVAS_ITEM_SELECTION_DELETE_DATA;

	for (list = canvas->selection; list; list = g_list_next(list)) {
		info = list->data;

		func = (ECanvasItemSelectionFunc)g_object_get_data(G_OBJECT(info->item),
								   "ECanvasItem::selection_callback");
		if (func)
			func(info->item, flags, info->id);
		g_message ("ECANVAS: free info (2): item %p, id %p",
			   info->item, info->id);
		g_object_unref (info->item);
		g_free(info);
	}
	g_list_free(canvas->selection);

	canvas->selection = NULL;

	gnome_canvas_item_grab_focus(item);

	info = g_new(ECanvasSelectionInfo, 1);
	info->item = item;
	g_object_ref (info->item);
	info->id = id;
	g_message ("ECANVAS: new info item %p, id %p", item, id);

	flags = E_CANVAS_ITEM_SELECTION_SELECT | E_CANVAS_ITEM_SELECTION_CURSOR;
	func = (ECanvasItemSelectionFunc)g_object_get_data(G_OBJECT(item),
							   "ECanvasItem::selection_callback");
	if (func)
		func(item, flags, id);

	canvas->selection = g_list_prepend(canvas->selection, info);
	canvas->cursor = info;
}

void
e_canvas_item_set_cursor_end (GnomeCanvasItem *item, gpointer id)
{
}

void
e_canvas_item_add_selection (GnomeCanvasItem *item, gpointer id)
{
	int flags;
	ECanvas *canvas;
	ECanvasSelectionInfo *info;
	ECanvasItemSelectionFunc func;
	GList *list;

	g_return_if_fail(item != NULL);
	g_return_if_fail(GNOME_IS_CANVAS_ITEM(item));
	g_return_if_fail(item->canvas != NULL);
	g_return_if_fail(E_IS_CANVAS(item->canvas));

	flags = E_CANVAS_ITEM_SELECTION_SELECT;
	canvas = E_CANVAS(item->canvas);

	if (canvas->cursor) {
		func = (ECanvasItemSelectionFunc)g_object_get_data(G_OBJECT(canvas->cursor->item),
								   "ECanvasItem::selection_callback");
		if (func)
			func(canvas->cursor->item, flags, canvas->cursor->id);
	}

	gnome_canvas_item_grab_focus(item);

	flags = E_CANVAS_ITEM_SELECTION_SELECT | E_CANVAS_ITEM_SELECTION_CURSOR;

	for (list = canvas->selection; list; list = g_list_next(list)) {
		ECanvasSelectionInfo *search;
		search = list->data;

		if (search->item == item) {
			ECanvasItemSelectionCompareFunc compare_func;
			compare_func = (ECanvasItemSelectionCompareFunc)g_object_get_data(G_OBJECT(search->item),
											  "ECanvasItem::selection_compare_callback");

			if (compare_func(search->item, search->id, id, 0) == 0) {
				canvas->cursor = search;
				func = (ECanvasItemSelectionFunc)g_object_get_data(G_OBJECT(item),
										   "ECanvasItem::selection_callback");
				if (func)
					func(item, flags, search->id);
				return;
			}
		}
	}

	info = g_new(ECanvasSelectionInfo, 1);
	info->item = item;
	g_object_ref (info->item);
	info->id = id;
	g_message ("ECANVAS: new info (2): item %p, id %p", item, id);

	func = (ECanvasItemSelectionFunc)g_object_get_data(G_OBJECT(item),
							   "ECanvasItem::selection_callback");
	if (func)
		func(item, flags, id);

	canvas->selection = g_list_prepend(canvas->selection, info);
	canvas->cursor = info;
}

void
e_canvas_item_remove_selection (GnomeCanvasItem *item, gpointer id)
{
	int flags;
	ECanvas *canvas;
	ECanvasSelectionInfo *info;
	GList *list;

	g_return_if_fail(item != NULL);
	g_return_if_fail(GNOME_IS_CANVAS_ITEM(item));
	g_return_if_fail(item->canvas != NULL);
	g_return_if_fail(E_IS_CANVAS(item->canvas));

	flags = E_CANVAS_ITEM_SELECTION_DELETE_DATA;
	canvas = E_CANVAS(item->canvas);

	for (list = canvas->selection; list; list = g_list_next(list)) {
		info = list->data;

		if (info->item == item) {
			ECanvasItemSelectionCompareFunc compare_func;
			compare_func = (ECanvasItemSelectionCompareFunc)g_object_get_data(G_OBJECT(info->item),
											  "ECanvasItem::selection_compare_callback");

			if (compare_func(info->item, info->id, id, 0) == 0) {
				ECanvasItemSelectionFunc func;
				func = (ECanvasItemSelectionFunc) g_object_get_data(G_OBJECT(info->item),
										    "ECanvasItem::selection_callback");
				if (func)
					func(info->item, flags, info->id);
				canvas->selection = g_list_remove_link(canvas->selection, list);

				if (canvas->cursor == info)
					canvas->cursor = NULL;

				g_message ("ECANVAS: removing info: item %p, info %p",
					   info->item, info->id);
				g_object_unref (info->item);
				g_free(info);
				g_list_free_1(list);
				break;
			}
		}
	}
}

void e_canvas_popup_tooltip (ECanvas *canvas, GtkWidget *widget, int x, int y)
{
	if (canvas->tooltip_window && canvas->tooltip_window != widget) {
		e_canvas_hide_tooltip(canvas);
	}
	canvas->tooltip_window = widget;
	canvas->visibility_first = TRUE;
	if (canvas->toplevel == NULL) {
		canvas->toplevel = gtk_widget_get_toplevel (GTK_WIDGET(canvas));
		if (canvas->toplevel) {
			gtk_widget_add_events(canvas->toplevel, GDK_VISIBILITY_NOTIFY_MASK);
			g_object_ref (canvas->toplevel);
			canvas->visibility_notify_id =
				g_signal_connect (canvas->toplevel, "visibility_notify_event",
						  G_CALLBACK (e_canvas_visibility), canvas);
		}
	}
	gtk_widget_set_uposition (widget, x, y);
	gtk_widget_show (widget);
}

void e_canvas_hide_tooltip  (ECanvas *canvas)
{
	if (canvas->tooltip_window) {
		gtk_widget_destroy (canvas->tooltip_window);
		canvas->tooltip_window = NULL;
	}
}


static gboolean
grab_cancelled_check (gpointer data)
{
	ECanvas *canvas = data;

	if (GNOME_CANVAS (canvas)->grabbed_item == NULL) {
		canvas->grab_cancelled_cb = NULL;
		canvas->grab_cancelled_check_id = 0;
		canvas->grab_cancelled_time = 0;
		canvas->grab_cancelled_data = NULL;
		return FALSE;
	}

	if (gtk_grab_get_current ()) {
		gnome_canvas_item_ungrab(GNOME_CANVAS (canvas)->grabbed_item, canvas->grab_cancelled_time);
		if (canvas->grab_cancelled_cb) {
			canvas->grab_cancelled_cb (canvas,
						   GNOME_CANVAS (canvas)->grabbed_item,
						   canvas->grab_cancelled_data);
		}
		canvas->grab_cancelled_cb = NULL;
		canvas->grab_cancelled_check_id = 0;
		canvas->grab_cancelled_time = 0;
		canvas->grab_cancelled_data = NULL;
		return FALSE;
	}
	return TRUE;
}

int
e_canvas_item_grab (ECanvas *canvas,
		    GnomeCanvasItem *item,
		    guint event_mask,
		    GdkCursor *cursor,
		    guint32 etime,
		    ECanvasItemGrabCancelled cancelled_cb,
		    gpointer cancelled_data)
{
	if (gtk_grab_get_current ()) {
		return GDK_GRAB_ALREADY_GRABBED;
	} else {
		int ret_val = gnome_canvas_item_grab (item, event_mask, cursor, etime);
		if (ret_val == GDK_GRAB_SUCCESS) {
			canvas->grab_cancelled_cb = cancelled_cb;
			canvas->grab_cancelled_check_id =
				g_timeout_add_full (G_PRIORITY_LOW,
						    100,
						    grab_cancelled_check,
						    canvas,
						    NULL);
			canvas->grab_cancelled_time = etime;
			canvas->grab_cancelled_data = cancelled_data;
		}

		return ret_val;
	}
}

void
e_canvas_item_ungrab (ECanvas *canvas,
		      GnomeCanvasItem *item,
		      guint32 etime)
{
	if (canvas->grab_cancelled_check_id) {
		g_source_remove (canvas->grab_cancelled_check_id);
		canvas->grab_cancelled_cb = NULL;
		canvas->grab_cancelled_check_id = 0;
		canvas->grab_cancelled_time = 0;
		canvas->grab_cancelled_data = NULL;
		gnome_canvas_item_ungrab (item, etime);
	}
}
