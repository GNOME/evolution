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

#include <gtk/gtk.h>
#include <libedataserver/libedataserver.h>

#include "e-canvas.h"

#define d(x)

enum {
	REFLOW,
	LAST_SIGNAL
};

static guint signals[LAST_SIGNAL];

G_DEFINE_TYPE (
	ECanvas,
	e_canvas,
	GNOME_TYPE_CANVAS)

/* Emits an event for an item in the canvas, be it the current
 * item, grabbed item, or focused item, as appropriate. */
static gint
canvas_emit_event (GnomeCanvas *canvas,
                   GdkEvent *event)
{
	GdkEvent *ev;
	gint finished;
	GnomeCanvasItem *item;
	GnomeCanvasItem *parent;
	guint mask;

	/* Choose where we send the event */

	item = canvas->current_item;

	if (canvas->focused_item &&
		((event->type == GDK_KEY_PRESS) ||
		 (event->type == GDK_KEY_RELEASE) ||
		 (event->type == GDK_FOCUS_CHANGE)))
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

	/* Convert to world coordinates -- we have two cases because of
	 * different offsets of the fields in the event structures. */

	ev = gdk_event_copy (event);

	switch (ev->type) {
		case GDK_ENTER_NOTIFY:
		case GDK_LEAVE_NOTIFY:
			gnome_canvas_window_to_world (
				canvas,
				ev->crossing.x, ev->crossing.y,
				&ev->crossing.x, &ev->crossing.y);
			break;

		case GDK_MOTION_NOTIFY:
		case GDK_BUTTON_PRESS:
		case GDK_2BUTTON_PRESS:
		case GDK_3BUTTON_PRESS:
		case GDK_BUTTON_RELEASE:
			gnome_canvas_window_to_world (
				canvas,
				ev->motion.x, ev->motion.y,
				&ev->motion.x, &ev->motion.y);
			break;

		default:
			break;
	}

	/* The event is propagated up the hierarchy (for if someone connected
	 * to a group instead of a leaf event), and emission is stopped if a
	 * handler returns TRUE, just like for GtkWidget events. */

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

/* This routine invokes the point method of the item.  The argument x, y
 * should be in the parent's item-relative coordinate system.  This routine
 * applies the inverse of the item's transform, maintaining the affine
 * invariant. */
static GnomeCanvasItem *
gnome_canvas_item_invoke_point (GnomeCanvasItem *item,
                                gdouble x,
                                gdouble y,
                                gint cx,
                                gint cy)
{
	cairo_matrix_t inverse;

	/* Calculate x & y in item local coordinates */
	inverse = item->matrix;
	if (cairo_matrix_invert (&inverse) != CAIRO_STATUS_SUCCESS)
		return NULL;

	cairo_matrix_transform_point (&inverse, &x, &y);

	if (GNOME_CANVAS_ITEM_GET_CLASS (item)->point)
		return GNOME_CANVAS_ITEM_GET_CLASS (item)->point (item, x, y, cx, cy);

	return NULL;
}

/* Re-picks the current item in the canvas, based on the event's coordinates.
 * Also emits enter/leave events for items as appropriate.
 */
#define DISPLAY_X1(canvas) (GNOME_CANVAS (canvas)->layout.xoffset)
#define DISPLAY_Y1(canvas) (GNOME_CANVAS (canvas)->layout.yoffset)
static gint
pick_current_item (GnomeCanvas *canvas,
                   GdkEvent *event)
{
	gint button_down;
	gdouble x, y;
	gint cx, cy;
	gint retval;

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
	if (!button_down)
		canvas->left_grabbed_item = FALSE;

	/* Save the event in the canvas.  This is used to synthesize enter and
	 * leave events in case the current item changes.  It is also used to
	 * re-pick the current item if the current one gets deleted.  Also,
	 * synthesize an enter event.
	 */
	if (event != &canvas->pick_event) {
		if ((event->type == GDK_MOTION_NOTIFY) ||
		    (event->type == GDK_BUTTON_RELEASE)) {
			/* these fields have the same offsets in both types of events */

			canvas->pick_event.crossing.type = GDK_ENTER_NOTIFY;
			canvas->pick_event.crossing.window = event->motion.window;
			canvas->pick_event.crossing.send_event = event->motion.send_event;
			canvas->pick_event.crossing.subwindow = NULL;
			canvas->pick_event.crossing.x = event->motion.x;
			canvas->pick_event.crossing.y = event->motion.y;
			canvas->pick_event.crossing.mode = GDK_CROSSING_NORMAL;
			canvas->pick_event.crossing.detail = GDK_NOTIFY_NONLINEAR;
			canvas->pick_event.crossing.focus = FALSE;
			canvas->pick_event.crossing.state = event->motion.state;

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
			x = canvas->pick_event.crossing.x +
				canvas->scroll_x1 - canvas->zoom_xofs;
			y = canvas->pick_event.crossing.y +
				canvas->scroll_y1 - canvas->zoom_yofs;
		} else {
			x = canvas->pick_event.motion.x +
				canvas->scroll_x1 - canvas->zoom_xofs;
			y = canvas->pick_event.motion.y +
				canvas->scroll_y1 - canvas->zoom_yofs;
		}

		/* canvas pixel coords */

		cx = (gint) (x + 0.5);
		cy = (gint) (y + 0.5);

		/* world coords */

		x = canvas->scroll_x1 + x;
		y = canvas->scroll_y1 + y;

		/* find the closest item */

		if (canvas->root->flags & GNOME_CANVAS_ITEM_VISIBLE)
			canvas->new_current_item =
				gnome_canvas_item_invoke_point (
				canvas->root, x, y, cx, cy);
		else
			canvas->new_current_item = NULL;
	} else
		canvas->new_current_item = NULL;

	if ((canvas->new_current_item == canvas->current_item) &&
			!canvas->left_grabbed_item)
		return retval; /* current item did not change */

	/* Synthesize events for old and new current items */

	if ((canvas->new_current_item != canvas->current_item)
	    && (canvas->current_item != NULL)
	    && !canvas->left_grabbed_item) {
		GdkEvent new_event = { 0 };

		new_event = canvas->pick_event;
		new_event.type = GDK_LEAVE_NOTIFY;

		new_event.crossing.detail = GDK_NOTIFY_ANCESTOR;
		new_event.crossing.subwindow = NULL;
		canvas->in_repick = TRUE;
		retval = canvas_emit_event (canvas, &new_event);
		canvas->in_repick = FALSE;
	}

	/* new_current_item may have been set to NULL during
	 * the call to canvas_emit_event() above. */

	if ((canvas->new_current_item != canvas->current_item) && button_down) {
		canvas->left_grabbed_item = TRUE;
		return retval;
	}

	/* Handle the rest of cases */

	canvas->left_grabbed_item = FALSE;
	canvas->current_item = canvas->new_current_item;

	if (canvas->current_item != NULL) {
		GdkEvent new_event = { 0 };

		new_event = canvas->pick_event;
		new_event.type = GDK_ENTER_NOTIFY;
		new_event.crossing.detail = GDK_NOTIFY_ANCESTOR;
		new_event.crossing.subwindow = NULL;
		retval = canvas_emit_event (canvas, &new_event);
	}

	return retval;
}

static void
canvas_style_updated_recursive (GnomeCanvasItem *item)
{
	guint signal_id = g_signal_lookup ("style_updated", G_OBJECT_TYPE (item));
	if (signal_id >= 1) {
		GSignalQuery query;
		g_signal_query (signal_id, &query);
		if (query.return_type == G_TYPE_NONE &&
			query.n_params == 0) {
			g_signal_emit (item, signal_id, 0);
		}
	}

	if (GNOME_IS_CANVAS_GROUP (item)) {
		GList *items = GNOME_CANVAS_GROUP (item)->item_list;
		for (; items; items = items->next)
			canvas_style_updated_recursive (items->data);
	}
}

static void
canvas_dispose (GObject *object)
{
	ECanvas *canvas = E_CANVAS (object);

	if (canvas->idle_id)
		g_source_remove (canvas->idle_id);
	canvas->idle_id = 0;

	if (canvas->grab_cancelled_check_id)
		g_source_remove (canvas->grab_cancelled_check_id);
	canvas->grab_cancelled_check_id = 0;

	if (canvas->toplevel) {
		if (canvas->visibility_notify_id)
			g_signal_handler_disconnect (
				canvas->toplevel,
				canvas->visibility_notify_id);
		canvas->visibility_notify_id = 0;

		g_object_unref (canvas->toplevel);
		canvas->toplevel = NULL;
	}

	g_clear_object (&canvas->im_context);

	/* Chain up to parent's dispose() method. */
	G_OBJECT_CLASS (e_canvas_parent_class)->dispose (object);
}

static void
canvas_realize (GtkWidget *widget)
{
	ECanvas *ecanvas = E_CANVAS (widget);
	GdkWindow *window;

	/* Chain up to parent's realize() method. */
	GTK_WIDGET_CLASS (e_canvas_parent_class)->realize (widget);

	window = gtk_layout_get_bin_window (GTK_LAYOUT (widget));
	gdk_window_set_background_pattern (window, NULL);

	window = gtk_widget_get_window (widget);
	gtk_im_context_set_client_window (ecanvas->im_context, window);
}

static void
canvas_unrealize (GtkWidget *widget)
{
	ECanvas * ecanvas = E_CANVAS (widget);

	if (ecanvas->idle_id) {
		g_source_remove (ecanvas->idle_id);
		ecanvas->idle_id = 0;
	}

	gtk_im_context_set_client_window (ecanvas->im_context, NULL);

	/* Chain up to parent's unrealize() method. */
	GTK_WIDGET_CLASS (e_canvas_parent_class)->unrealize (widget);
}

static void
canvas_style_updated (GtkWidget *widget)
{
	GTK_WIDGET_CLASS (e_canvas_parent_class)->style_updated (widget);

	canvas_style_updated_recursive (
		GNOME_CANVAS_ITEM (gnome_canvas_root (
		GNOME_CANVAS (widget))));
}

static gint
canvas_button_event (GtkWidget *widget,
                     GdkEventButton *event)
{
	GnomeCanvas *canvas;
	GdkWindow *bin_window;
	gint mask;
	gint retval;

	g_return_val_if_fail (GNOME_IS_CANVAS (widget), FALSE);
	g_return_val_if_fail (event != NULL, FALSE);

	retval = FALSE;

	canvas = GNOME_CANVAS (widget);
	bin_window = gtk_layout_get_bin_window (GTK_LAYOUT (canvas));

	d (
		g_print ("button %d, event type %d, grabbed=%p, current=%p\n",
		event->button,
		event->type,
		canvas->grabbed_item,
		canvas->current_item));

        /* dispatch normally regardless of the event's window if an item has
	   has a pointer grab in effect */
	if (!canvas->grabbed_item && event->window != bin_window)
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
			/* Pick the current item as if the button were not
			 * pressed, and then process the event. */
			canvas->state = event->state;
			pick_current_item (canvas, (GdkEvent *) event);
			canvas->state ^= mask;
			retval = canvas_emit_event (canvas, (GdkEvent *) event);
			break;

		case GDK_BUTTON_RELEASE:
			/* Process the event as if the button were pressed,
			 * then repick after the button has been released. */
			canvas->state = event->state;
			retval = canvas_emit_event (canvas, (GdkEvent *) event);
			event->state ^= mask;
			canvas->state = event->state;
			pick_current_item (canvas, (GdkEvent *) event);
			event->state ^= mask;
			break;

		default:
			g_return_val_if_reached (0);
	}

	return retval;
}

static gint
canvas_key_event (GtkWidget *widget,
                  GdkEventKey *event)
{
	GnomeCanvas *canvas;
	GdkEvent full_event = { 0 };

	g_return_val_if_fail (GNOME_IS_CANVAS (widget), FALSE);
	g_return_val_if_fail (event != NULL, FALSE);

	canvas = GNOME_CANVAS (widget);

	full_event.type = event->type;
	full_event.key = *event;

	return canvas_emit_event (canvas, &full_event);
}

static gint
canvas_focus_in_event (GtkWidget *widget,
                       GdkEventFocus *event)
{
	GnomeCanvas *canvas;
	ECanvas *ecanvas;
	GdkEvent full_event = { 0 };

	canvas = GNOME_CANVAS (widget);
	ecanvas = E_CANVAS (widget);

	/* XXX Can't access flags directly anymore, but is it really needed?
	 *     If so, could we call gtk_widget_send_focus_change() instead? */
#if 0
	GTK_WIDGET_SET_FLAGS (widget, GTK_HAS_FOCUS);
#endif

	gtk_im_context_focus_in (ecanvas->im_context);

	if (canvas->focused_item) {
		full_event.type = event->type;
		full_event.focus_change = *event;
		return canvas_emit_event (canvas, &full_event);
	} else {
		return FALSE;
	}
}

static gint
canvas_focus_out_event (GtkWidget *widget,
                        GdkEventFocus *event)
{
	GnomeCanvas *canvas;
	ECanvas *ecanvas;
	GdkEvent full_event = { 0 };

	canvas = GNOME_CANVAS (widget);
	ecanvas = E_CANVAS (widget);

	/* XXX Can't access flags directly anymore, but is it really needed?
	 *     If so, could we call gtk_widget_send_focus_change() instead? */
#if 0
	GTK_WIDGET_UNSET_FLAGS (widget, GTK_HAS_FOCUS);
#endif

	gtk_im_context_focus_out (ecanvas->im_context);

	if (canvas->focused_item) {
		full_event.type = event->type;
		full_event.focus_change = *event;
		return canvas_emit_event (canvas, &full_event);
	} else {
		return FALSE;
	}
}

static void
canvas_reflow (ECanvas *canvas)
{
	/* Placeholder so subclasses can safely chain up. */
}

static void
e_canvas_class_init (ECanvasClass *class)
{
	GObjectClass *object_class;
	GtkWidgetClass *widget_class;

	object_class = G_OBJECT_CLASS (class);
	object_class->dispose = canvas_dispose;

	widget_class = GTK_WIDGET_CLASS (class);
	widget_class->realize = canvas_realize;
	widget_class->unrealize = canvas_unrealize;
	widget_class->style_updated = canvas_style_updated;
	widget_class->button_press_event = canvas_button_event;
	widget_class->button_release_event = canvas_button_event;
	widget_class->key_press_event = canvas_key_event;
	widget_class->key_release_event = canvas_key_event;
	widget_class->focus_in_event = canvas_focus_in_event;
	widget_class->focus_out_event = canvas_focus_out_event;

	class->reflow = canvas_reflow;

	signals[REFLOW] = g_signal_new (
		"reflow",
		G_OBJECT_CLASS_TYPE (object_class),
		G_SIGNAL_RUN_LAST,
		G_STRUCT_OFFSET (ECanvasClass, reflow),
		NULL, NULL,
		g_cclosure_marshal_VOID__VOID,
		G_TYPE_NONE, 0);
}

static void
e_canvas_init (ECanvas *canvas)
{
	canvas->im_context = gtk_im_multicontext_new ();
}

GtkWidget *
e_canvas_new (void)
{
	return g_object_new (E_TYPE_CANVAS, NULL);
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
e_canvas_item_grab_focus (GnomeCanvasItem *item,
                          gboolean widget_too)
{
	GnomeCanvasItem *focused_item;
	GdkWindow *bin_window;
	GdkEvent ev = { 0 };

	g_return_if_fail (GNOME_IS_CANVAS_ITEM (item));
	g_return_if_fail (gtk_widget_get_can_focus (GTK_WIDGET (item->canvas)));

	bin_window = gtk_layout_get_bin_window (GTK_LAYOUT (item->canvas));

	focused_item = item->canvas->focused_item;

	if (focused_item) {
		ev.type = GDK_FOCUS_CHANGE;
		ev.focus_change.type = GDK_FOCUS_CHANGE;
		ev.focus_change.window = bin_window;
		ev.focus_change.send_event = FALSE;
		ev.focus_change.in = FALSE;

		canvas_emit_event (item->canvas, &ev);
	}

	item->canvas->focused_item = item;

	if (widget_too && !gtk_widget_has_focus (GTK_WIDGET (item->canvas))) {
		gtk_widget_grab_focus (GTK_WIDGET (item->canvas));
	}

	ev.focus_change.type = GDK_FOCUS_CHANGE;
	ev.focus_change.window = bin_window;
	ev.focus_change.send_event = FALSE;
	ev.focus_change.in = TRUE;

	canvas_emit_event (item->canvas, &ev);
}

static void
e_canvas_item_invoke_reflow (GnomeCanvasItem *item,
                             gint flags)
{
	GnomeCanvasGroup *group;
	GList *list;
	GnomeCanvasItem *child;

	if (GNOME_IS_CANVAS_GROUP (item)) {
		group = GNOME_CANVAS_GROUP (item);
		for (list = group->item_list; list; list = list->next) {
			child = GNOME_CANVAS_ITEM (list->data);
			if (child->flags & E_CANVAS_ITEM_DESCENDENT_NEEDS_REFLOW)
				e_canvas_item_invoke_reflow (child, flags);
		}
	}

	if (item->flags & E_CANVAS_ITEM_NEEDS_REFLOW) {
		ECanvasItemReflowFunc func;
		func = (ECanvasItemReflowFunc)
			g_object_get_data (
				G_OBJECT (item),
				"ECanvasItem::reflow_callback");
		if (func)
			func (item, flags);
	}

	item->flags &= ~E_CANVAS_ITEM_NEEDS_REFLOW;
	item->flags &= ~E_CANVAS_ITEM_DESCENDENT_NEEDS_REFLOW;
}

static void
do_reflow (ECanvas *canvas)
{
	if (GNOME_CANVAS (canvas)->root->flags & E_CANVAS_ITEM_DESCENDENT_NEEDS_REFLOW)
		e_canvas_item_invoke_reflow (GNOME_CANVAS (canvas)->root, 0);
}

/* Idle handler for the e-canvas.  It deals with pending reflows. */
static gint
idle_handler (gpointer data)
{
	ECanvas *canvas;

	canvas = E_CANVAS (data);
	do_reflow (canvas);

	/* Reset idle id */
	canvas->idle_id = 0;

	g_signal_emit (canvas, signals[REFLOW], 0);

	return FALSE;
}

/* Convenience function to add an idle handler to a canvas */
static void
add_idle (ECanvas *canvas)
{
	if (canvas->idle_id != 0)
		return;

	canvas->idle_id = g_idle_add_full (
		G_PRIORITY_HIGH_IDLE, idle_handler, (gpointer) canvas, NULL);
}

static void
e_canvas_item_descendent_needs_reflow (GnomeCanvasItem *item)
{
	if (item->flags & E_CANVAS_ITEM_DESCENDENT_NEEDS_REFLOW)
		return;

	item->flags |= E_CANVAS_ITEM_DESCENDENT_NEEDS_REFLOW;
	if (item->parent)
		e_canvas_item_descendent_needs_reflow (item->parent);
}

void
e_canvas_item_request_reflow (GnomeCanvasItem *item)
{
	g_return_if_fail (GNOME_IS_CANVAS_ITEM (item));

	if (item->flags & GNOME_CANVAS_ITEM_REALIZED) {
		item->flags |= E_CANVAS_ITEM_NEEDS_REFLOW;
		e_canvas_item_descendent_needs_reflow (item);
		add_idle (E_CANVAS (item->canvas));
	}
}

void
e_canvas_item_request_parent_reflow (GnomeCanvasItem *item)
{
	g_return_if_fail (GNOME_IS_CANVAS_ITEM (item));

	e_canvas_item_request_reflow (item->parent);
}

void
e_canvas_item_set_reflow_callback (GnomeCanvasItem *item,
                                   ECanvasItemReflowFunc func)
{
	g_return_if_fail (GNOME_IS_CANVAS_ITEM (item));
	g_return_if_fail (func != NULL);

	g_object_set_data (
		G_OBJECT (item), "ECanvasItem::reflow_callback",
		(gpointer) func);
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
		gnome_canvas_item_ungrab (
			GNOME_CANVAS (canvas)->grabbed_item,
			canvas->grab_cancelled_time);
		if (canvas->grab_cancelled_cb)
			canvas->grab_cancelled_cb (
				canvas, GNOME_CANVAS (canvas)->grabbed_item,
				canvas->grab_cancelled_data);
		canvas->grab_cancelled_cb = NULL;
		canvas->grab_cancelled_check_id = 0;
		canvas->grab_cancelled_time = 0;
		canvas->grab_cancelled_data = NULL;
		return FALSE;
	}
	return TRUE;
}

gint
e_canvas_item_grab (ECanvas *canvas,
                    GnomeCanvasItem *item,
                    guint event_mask,
                    GdkCursor *cursor,
                    GdkDevice *device,
                    guint32 etime,
                    ECanvasItemGrabCancelled cancelled_cb,
                    gpointer cancelled_data)
{
	GdkGrabStatus grab_status;

	g_return_val_if_fail (E_IS_CANVAS (canvas), -1);
	g_return_val_if_fail (GNOME_IS_CANVAS_ITEM (item), -1);
	g_return_val_if_fail (GDK_IS_DEVICE (device), -1);

	if (gtk_grab_get_current ())
		return GDK_GRAB_ALREADY_GRABBED;

	grab_status = gnome_canvas_item_grab (
		item, event_mask, cursor, device, etime);
	if (grab_status == GDK_GRAB_SUCCESS) {
		canvas->grab_cancelled_cb = cancelled_cb;
		canvas->grab_cancelled_check_id = e_named_timeout_add_full (
			G_PRIORITY_LOW, 100,
			grab_cancelled_check, canvas, NULL);
		canvas->grab_cancelled_time = etime;
		canvas->grab_cancelled_data = cancelled_data;
	}

	return grab_status;
}

void
e_canvas_item_ungrab (ECanvas *canvas,
                      GnomeCanvasItem *item,
                      guint32 etime)
{
	g_return_if_fail (E_IS_CANVAS (canvas));
	g_return_if_fail (GNOME_IS_CANVAS_ITEM (item));

	if (canvas->grab_cancelled_check_id) {
		g_source_remove (canvas->grab_cancelled_check_id);
		canvas->grab_cancelled_cb = NULL;
		canvas->grab_cancelled_check_id = 0;
		canvas->grab_cancelled_time = 0;
		canvas->grab_cancelled_data = NULL;
		gnome_canvas_item_ungrab (item, etime);
	}
}
