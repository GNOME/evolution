/*
 * Map widget.
 *
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
 *		Hans Petter Jansson <hpj@ximian.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#include <config.h>
#include <math.h>
#include <stdlib.h>
#include <gdk/gdkkeysyms.h>
#include <glib/gi18n.h>

#include "e-util/e-util-private.h"
#include "e-util/e-util.h"

#include "e-map.h"

/* Scroll step increment */

#define SCROLL_STEP_SIZE 32

/* */

#define E_MAP_GET_WIDTH(map) gdk_pixbuf_get_width(((EMapPrivate *) E_MAP(map)->priv)->map_render_pixbuf)
#define E_MAP_GET_HEIGHT(map) gdk_pixbuf_get_height(((EMapPrivate *) E_MAP(map)->priv)->map_render_pixbuf)

/* Zoom state - keeps track of animation hacks */

typedef enum
{
	E_MAP_ZOOMED_IN,
	E_MAP_ZOOMED_OUT,
	E_MAP_ZOOMING_IN,
	E_MAP_ZOOMING_OUT
}
EMapZoomState;

/* Private part of the EMap structure */

struct _EMapPrivate {
	/* Pointer to map image */
	GdkPixbuf *map_pixbuf, *map_render_pixbuf;

	/* Settings */
	gboolean frozen, smooth_zoom;

	/* Adjustments for scrolling */
	GtkAdjustment *hadj;
	GtkAdjustment *vadj;

	/* Current scrolling offsets */
	gint xofs, yofs;

	/* Realtime zoom data */
	EMapZoomState zoom_state;
	gdouble zoom_target_long, zoom_target_lat;

	/* Dots */
	GPtrArray *points;
};

/* Internal prototypes */

static void e_map_finalize (GObject *object);
static void e_map_destroy (GtkObject *object);
static void e_map_realize (GtkWidget *widget);
static void e_map_size_request (GtkWidget *widget, GtkRequisition *requisition);
static void e_map_size_allocate (GtkWidget *widget, GtkAllocation *allocation);
static gint e_map_button_press (GtkWidget *widget, GdkEventButton *event);
static gint e_map_button_release (GtkWidget *widget, GdkEventButton *event);
static gint e_map_motion (GtkWidget *widget, GdkEventMotion *event);
static gint e_map_expose (GtkWidget *widget, GdkEventExpose *event);
static gint e_map_key_press (GtkWidget *widget, GdkEventKey *event);
static void e_map_set_scroll_adjustments (GtkWidget *widget, GtkAdjustment *hadj, GtkAdjustment *vadj);

static void update_render_pixbuf (EMap *map, GdkInterpType interp, gboolean render_overlays);
static void set_scroll_area (EMap *view);
static void request_paint_area (EMap *view, GdkRectangle *area);
static void center_at (EMap *map, gint x, gint y, gboolean scroll);
static void smooth_center_at (EMap *map, gint x, gint y);
static void scroll_to (EMap *view, gint x, gint y);
static void zoom_do (EMap *map);
static gint load_map_background (EMap *view, gchar *name);
static void adjustment_changed_cb (GtkAdjustment *adj, gpointer data);
static void update_and_paint (EMap *map);
static void update_render_point (EMap *map, EMapPoint *point);
static void repaint_point (EMap *map, EMapPoint *point);

G_DEFINE_TYPE (
	EMap,
	e_map,
	GTK_TYPE_WIDGET)

/* ----------------- *
 * Widget management *
 * ----------------- */

/* Class initialization function for the map view */

static void
e_map_class_init (EMapClass *class)
{
	GObjectClass *gobject_class;
	GtkObjectClass *object_class;
	GtkWidgetClass *widget_class;

	gobject_class = (GObjectClass *) class;
	object_class = (GtkObjectClass *) class;
	widget_class = (GtkWidgetClass *) class;

	gobject_class->finalize = e_map_finalize;

	object_class->destroy = e_map_destroy;

	class->set_scroll_adjustments = e_map_set_scroll_adjustments;
	widget_class->set_scroll_adjustments_signal = g_signal_new ("set_scroll_adjustments",
								    G_OBJECT_CLASS_TYPE (gobject_class),
								    G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
								    G_STRUCT_OFFSET (EMapClass, set_scroll_adjustments),
								    NULL, NULL,
								    e_marshal_NONE__OBJECT_OBJECT,
								    G_TYPE_NONE, 2,
								    GTK_TYPE_ADJUSTMENT,
								    GTK_TYPE_ADJUSTMENT);

	widget_class->realize = e_map_realize;
	widget_class->size_request = e_map_size_request;
	widget_class->size_allocate = e_map_size_allocate;
	widget_class->button_press_event = e_map_button_press;
	widget_class->button_release_event = e_map_button_release;
	widget_class->motion_notify_event = e_map_motion;
	widget_class->expose_event = e_map_expose;
	widget_class->key_press_event = e_map_key_press;
}

/* Object initialization function for the map view */

static void
e_map_init (EMap *view)
{
	EMapPrivate *priv;
	GtkWidget *widget;
	gchar *map_file_name = g_build_filename (EVOLUTION_IMAGESDIR, "world_map-960.png", NULL);

	widget = GTK_WIDGET (view);

	priv = g_new0 (EMapPrivate, 1);
	view->priv = priv;

	load_map_background (view, map_file_name);
	g_free (map_file_name);
	priv->frozen = FALSE;
	priv->smooth_zoom = TRUE;
	priv->zoom_state = E_MAP_ZOOMED_OUT;
        priv->points = g_ptr_array_new ();

	gtk_widget_set_can_focus (widget, TRUE);
	gtk_widget_set_has_window (widget, TRUE);
}

/* Destroy handler for the map view */

static void
e_map_destroy (GtkObject *object)
{
	EMap *view;
	EMapPrivate *priv;

	g_return_if_fail (object != NULL);
	g_return_if_fail (E_IS_MAP (object));

	view = E_MAP (object);
	priv = view->priv;

	g_signal_handlers_disconnect_by_func (priv->hadj, adjustment_changed_cb, view);
	g_signal_handlers_disconnect_by_func (priv->vadj, adjustment_changed_cb, view);

	GTK_OBJECT_CLASS (e_map_parent_class)->destroy (object);
}

/* Finalize handler for the map view */

static void
e_map_finalize (GObject *object)
{
	EMap *view;
	EMapPrivate *priv;

	g_return_if_fail (object != NULL);
	g_return_if_fail (E_IS_MAP (object));

	view = E_MAP (object);
	priv = view->priv;

	g_object_unref ((priv->hadj));
	priv->hadj = NULL;

	g_object_unref ((priv->vadj));
	priv->vadj = NULL;

	if (priv->map_pixbuf)
	{
		g_object_unref (priv->map_pixbuf);
		priv->map_pixbuf = NULL;
	}

	if (priv->map_render_pixbuf)
	{
		g_object_unref (priv->map_render_pixbuf);
		priv->map_render_pixbuf = NULL;
	}

	g_free (priv);
	view->priv = NULL;

	G_OBJECT_CLASS (e_map_parent_class)->finalize (object);
}

/* Realize handler for the map view */

static void
e_map_realize (GtkWidget *widget)
{
	GtkAllocation allocation;
	GdkWindowAttr attr;
	GdkWindow *window;
	GtkStyle *style;
	gint attr_mask;

	g_return_if_fail (widget != NULL);
	g_return_if_fail (E_IS_MAP (widget));

	gtk_widget_set_realized (widget, TRUE);

	gtk_widget_get_allocation (widget, &allocation);

	attr.window_type = GDK_WINDOW_CHILD;
	attr.x = allocation.x;
	attr.y = allocation.y;
	attr.width = allocation.width;
	attr.height = allocation.height;
	attr.wclass = GDK_INPUT_OUTPUT;
	attr.visual = gdk_rgb_get_visual ();
	attr.colormap = gdk_rgb_get_colormap ();
	attr.event_mask = gtk_widget_get_events (widget) |
	  GDK_EXPOSURE_MASK | GDK_BUTTON_PRESS_MASK | GDK_KEY_PRESS_MASK |
	  GDK_POINTER_MOTION_MASK;

	attr_mask = GDK_WA_X | GDK_WA_Y | GDK_WA_VISUAL | GDK_WA_COLORMAP;

	window = gdk_window_new (
		gtk_widget_get_parent_window (widget), &attr, attr_mask);
	gtk_widget_set_window (widget, window);
	gdk_window_set_user_data (window, widget);

	style = gtk_widget_get_style (widget);
	style = gtk_style_attach (style, window);
	gtk_widget_set_style (widget, style);

	gdk_window_set_back_pixmap (window, NULL, FALSE);
	update_render_pixbuf (E_MAP (widget), GDK_INTERP_BILINEAR, TRUE);
}

/* Size_request handler for the map view */

static void
e_map_size_request (GtkWidget *widget, GtkRequisition *requisition)
{
	EMap *view;
	EMapPrivate *priv;

	g_return_if_fail (widget != NULL);
	g_return_if_fail (E_IS_MAP (widget));
	g_return_if_fail (requisition != NULL);

	view = E_MAP (widget);
	priv = view->priv;

	/* TODO: Put real sizes here. */

	requisition->width = gdk_pixbuf_get_width (priv->map_pixbuf);
	requisition->height = gdk_pixbuf_get_height (priv->map_pixbuf);
}

/* Size_allocate handler for the map view */

static void
e_map_size_allocate (GtkWidget *widget, GtkAllocation *allocation)
{
	EMap *view;
	GdkRectangle area;

	g_return_if_fail (widget != NULL);
	g_return_if_fail (E_IS_MAP (widget));
	g_return_if_fail (allocation != NULL);

	view = E_MAP (widget);

	/* Resize the window */

	gtk_widget_set_allocation (widget, allocation);

	if (gtk_widget_get_realized (widget)) {
		GdkWindow *window;

		window = gtk_widget_get_window (widget);

		gdk_window_move_resize (
			window, allocation->x, allocation->y,
			allocation->width, allocation->height);

		area.x = 0;
		area.y = 0;
		area.width = allocation->width;
		area.height = allocation->height;
		request_paint_area (E_MAP (widget), &area);
	}

	update_render_pixbuf (view, GDK_INTERP_BILINEAR, TRUE);
}

/* Button press handler for the map view */

static gint
e_map_button_press (GtkWidget *widget, GdkEventButton *event)
{
	if (!gtk_widget_has_focus (widget)) gtk_widget_grab_focus (widget);
		return TRUE;
}

/* Button release handler for the map view */

static gint
e_map_button_release (GtkWidget *widget, GdkEventButton *event)
{
	if (event->button != 1) return FALSE;

	gdk_pointer_ungrab (event->time);
	return TRUE;
}

/* Motion handler for the map view */

static gint
e_map_motion (GtkWidget *widget, GdkEventMotion *event)
{
	return FALSE;

/*
 * if (event->is_hint)
 *   gdk_window_get_pointer(widget->window, &x, &y, &mods);
 * else
 * {
 *   x = event->x;
 *   y = event->y;
 * }
 *
 * return TRUE;
 */
}

/* Expose handler for the map view */

static gint
e_map_expose (GtkWidget *widget, GdkEventExpose *event)
{
	EMap *view;

	g_return_val_if_fail (widget != NULL, FALSE);
	g_return_val_if_fail (E_IS_MAP (widget), FALSE);
	g_return_val_if_fail (event != NULL, FALSE);

	view = E_MAP (widget);

	request_paint_area (view, &event->area);
	return TRUE;
}

/* Set_scroll_adjustments handler for the map view */

static void
e_map_set_scroll_adjustments (GtkWidget *widget, GtkAdjustment *hadj, GtkAdjustment *vadj)
{
	EMap *view;
	EMapPrivate *priv;
	gboolean need_adjust;

	g_return_if_fail (widget != NULL);
	g_return_if_fail (E_IS_MAP (widget));

	view = E_MAP (widget);
	priv = view->priv;

	if (hadj) g_return_if_fail (GTK_IS_ADJUSTMENT (hadj));
	else hadj = GTK_ADJUSTMENT (gtk_adjustment_new (0.0, 0.0, 0.0, 0.0, 0.0, 0.0));

	if (vadj) g_return_if_fail (GTK_IS_ADJUSTMENT (vadj));
	else vadj = GTK_ADJUSTMENT (gtk_adjustment_new (0.0, 0.0, 0.0, 0.0, 0.0, 0.0));

	if (priv->hadj && priv->hadj != hadj)
	{
		g_signal_handlers_disconnect_by_func (priv->hadj,
						      adjustment_changed_cb,
						      view);
		g_object_unref (priv->hadj);
	}

	if (priv->vadj && priv->vadj != vadj)
	{
		g_signal_handlers_disconnect_by_func (priv->vadj,
						      adjustment_changed_cb,
						      view);
		g_object_unref (priv->vadj);
	}

	need_adjust = FALSE;

	if (priv->hadj != hadj)
	{
		priv->hadj = hadj;
		g_object_ref_sink (priv->hadj);

		g_signal_connect (priv->hadj, "value_changed",
				  G_CALLBACK (adjustment_changed_cb), view);

		need_adjust = TRUE;
	}

	if (priv->vadj != vadj)
	{
		priv->vadj = vadj;
		g_object_ref_sink (priv->vadj);

		g_signal_connect (priv->vadj, "value_changed",
				  G_CALLBACK (adjustment_changed_cb), view);

		need_adjust = TRUE;
	}

	if (need_adjust) adjustment_changed_cb (NULL, view);
}

/* Key press handler for the map view */

static gint
e_map_key_press (GtkWidget *widget, GdkEventKey *event)
{
	EMap *view;
	EMapPrivate *priv;
	gboolean do_scroll;
	gint xofs, yofs;

	view = E_MAP (widget);
	priv = view->priv;

	switch (event->keyval)
	{
		case GDK_Up:
			do_scroll = TRUE;
			xofs = 0;
			yofs = -SCROLL_STEP_SIZE;
			break;

		case GDK_Down:
			do_scroll = TRUE;
			xofs = 0;
			yofs = SCROLL_STEP_SIZE;
			break;

		case GDK_Left:
			do_scroll = TRUE;
			xofs = -SCROLL_STEP_SIZE;
			yofs = 0;
			break;

		case GDK_Right:
			do_scroll = TRUE;
			xofs = SCROLL_STEP_SIZE;
			yofs = 0;
			break;

		default:
			return FALSE;
	}

	if (do_scroll)
	{
		gint page_size;
		gint upper;
		gint x, y;

		page_size = gtk_adjustment_get_page_size (priv->hadj);
		upper = gtk_adjustment_get_upper (priv->hadj);
		x = CLAMP (priv->xofs + xofs, 0, upper - page_size);

		page_size = gtk_adjustment_get_page_size (priv->vadj);
		upper = gtk_adjustment_get_upper (priv->vadj);
		y = CLAMP (priv->yofs + yofs, 0, upper - page_size);

		scroll_to (view, x, y);

		g_signal_handlers_block_matched (priv->hadj, G_SIGNAL_MATCH_DATA, 0, 0, NULL, NULL, view);
		g_signal_handlers_block_matched (priv->vadj, G_SIGNAL_MATCH_DATA, 0, 0, NULL, NULL, view);

		gtk_adjustment_set_value (priv->hadj, x);
		gtk_adjustment_set_value (priv->vadj, y);

		g_signal_emit_by_name (priv->hadj, "value_changed");
		g_signal_emit_by_name (priv->vadj, "value_changed");

		g_signal_handlers_unblock_matched (priv->hadj, G_SIGNAL_MATCH_DATA, 0, 0, NULL, NULL, view);
		g_signal_handlers_unblock_matched (priv->vadj, G_SIGNAL_MATCH_DATA, 0, 0, NULL, NULL, view);
	}

	return TRUE;
}

/* ---------------- *
 * Widget interface *
 * ---------------- */

/**
 * e_map_new:
 * @void:
 *
 * Creates a new empty map widget.
 *
 * Return value: A newly-created map widget.
 **/

EMap *
e_map_new (void)
{
	GtkWidget *widget;
	AtkObject *a11y;

	widget = g_object_new (E_TYPE_MAP, NULL);
	a11y = gtk_widget_get_accessible (widget);
	atk_object_set_name (a11y, _("World Map"));
	atk_object_set_role (a11y, ATK_ROLE_IMAGE);
	atk_object_set_description (a11y, _("Mouse-based interactive map widget for selecting timezone. Keyboard users should instead select the timezone from the drop-down combination box below."));
	return (E_MAP (widget));
}

/* --- Coordinate translation --- */

/* These functions translate coordinates between longitude/latitude and
 * the image x/y offsets, using the equidistant cylindrical projection.
 *
 * Longitude E <-180, 180]
 * Latitude  E <-90, 90]   */

void
e_map_window_to_world (EMap *map, gdouble win_x, gdouble win_y, gdouble *world_longitude, gdouble *world_latitude)
{
	EMapPrivate *priv;
	gint width, height;

	g_return_if_fail (map);

	priv = map->priv;
	g_return_if_fail (gtk_widget_get_realized (GTK_WIDGET (map)));

	width = gdk_pixbuf_get_width (priv->map_render_pixbuf);
	height = gdk_pixbuf_get_height (priv->map_render_pixbuf);

	*world_longitude = (win_x + priv->xofs - (gdouble) width / 2.0) /
		((gdouble) width / 2.0) * 180.0;
	*world_latitude = ((gdouble) height / 2.0 - win_y - priv->yofs) /
		((gdouble) height / 2.0) * 90.0;
}

void
e_map_world_to_window (EMap *map, gdouble world_longitude, gdouble world_latitude, gdouble *win_x, gdouble *win_y)
{
	EMapPrivate *priv;
	gint width, height;

	g_return_if_fail (map);

	priv = map->priv;
	g_return_if_fail (priv->map_render_pixbuf);
	g_return_if_fail (world_longitude >= -180.0 && world_longitude <= 180.0);
	g_return_if_fail (world_latitude >= -90.0 && world_latitude <= 90.0);

	width = gdk_pixbuf_get_width (priv->map_render_pixbuf);
	height = gdk_pixbuf_get_height (priv->map_render_pixbuf);

	*win_x = (width / 2.0 + (width / 2.0) * world_longitude / 180.0) - priv->xofs;
	*win_y = (height / 2.0 - (height / 2.0) * world_latitude / 90.0) - priv->yofs;

#ifdef DEBUG
	printf ("Map size: (%d, %d)\nCoords: (%.1f, %.1f) -> (%.1f, %.1f)\n---\n", width, height, world_longitude, world_latitude, *win_x, *win_y);
#endif
}

/* --- Zoom --- */

gdouble
e_map_get_magnification (EMap *map)
{
	EMapPrivate *priv;

	priv = map->priv;
	if (priv->zoom_state == E_MAP_ZOOMED_IN) return 2.0;
	else return 1.0;
}

void
e_map_zoom_to_location (EMap *map, gdouble longitude, gdouble latitude)
{
	EMapPrivate *priv;

	g_return_if_fail (map);
	g_return_if_fail (gtk_widget_get_realized (GTK_WIDGET (map)));

	priv = map->priv;

	if (priv->zoom_state == E_MAP_ZOOMED_IN) e_map_zoom_out (map);
	else if (priv->zoom_state != E_MAP_ZOOMED_OUT) return;

	priv->zoom_state = E_MAP_ZOOMING_IN;
	priv->zoom_target_long = longitude;
	priv->zoom_target_lat = latitude;

	zoom_do (map);
}

void
e_map_zoom_out (EMap *map)
{
	EMapPrivate *priv;

	g_return_if_fail (map);
	g_return_if_fail (gtk_widget_get_realized (GTK_WIDGET (map)));

	priv = map->priv;

	if (priv->zoom_state != E_MAP_ZOOMED_IN) return;

	priv->zoom_state = E_MAP_ZOOMING_OUT;
	zoom_do (map);
	priv->zoom_state = E_MAP_ZOOMED_OUT;
}

void
e_map_set_smooth_zoom (EMap *map, gboolean state)
{
	((EMapPrivate *) map->priv)->smooth_zoom = state;
}

gboolean
e_map_get_smooth_zoom (EMap *map)
{
	return (((EMapPrivate *) map->priv)->smooth_zoom);
}

void
e_map_freeze (EMap *map)
{
	((EMapPrivate *) map->priv)->frozen = TRUE;
}

void
e_map_thaw (EMap *map)
{
	((EMapPrivate *) map->priv)->frozen = FALSE;
	update_and_paint (map);
}

/* --- Point manipulation --- */

EMapPoint *
e_map_add_point (EMap *map, gchar *name, gdouble longitude, gdouble latitude, guint32 color_rgba)
{
	EMapPrivate *priv;
	EMapPoint *point;

	priv = map->priv;
	point = g_new0 (EMapPoint, 1);

	point->name = name;  /* Can be NULL */
	point->longitude = longitude;
	point->latitude = latitude;
	point->rgba = color_rgba;

	g_ptr_array_add (priv->points, (gpointer) point);

	if (!priv->frozen)
	{
		update_render_point (map, point);
		repaint_point (map, point);
	}

	return point;
}

void
e_map_remove_point (EMap *map, EMapPoint *point)
{
	EMapPrivate *priv;

	priv = map->priv;
	g_ptr_array_remove (priv->points, point);

	if (!((EMapPrivate *) map->priv)->frozen)
	{
		/* FIXME: Re-scaling the whole pixbuf is more than a little
		 * overkill when just one point is removed */

		update_render_pixbuf (map, GDK_INTERP_BILINEAR, TRUE);
		repaint_point (map, point);
	}

	g_free (point);
}

void
e_map_point_get_location (EMapPoint *point, gdouble *longitude, gdouble *latitude)
{
	*longitude = point->longitude;
	*latitude = point->latitude;
}

gchar *
e_map_point_get_name (EMapPoint *point)
{
	return point->name;
}

guint32
e_map_point_get_color_rgba (EMapPoint *point)
{
	return point->rgba;
}

void
e_map_point_set_color_rgba (EMap *map, EMapPoint *point, guint32 color_rgba)
{
	point->rgba = color_rgba;

	if (!((EMapPrivate *) map->priv)->frozen)
	{
		/* TODO: Redraw area around point only */

		update_render_point (map, point);
		repaint_point (map, point);
	}
}

void
e_map_point_set_data (EMapPoint *point, gpointer data)
{
	point->user_data = data;
}

gpointer
e_map_point_get_data (EMapPoint *point)
{
	return point->user_data;
}

gboolean
e_map_point_is_in_view (EMap *map, EMapPoint *point)
{
	EMapPrivate *priv;
	GtkAllocation allocation;
	gdouble x, y;

	priv = map->priv;
	if (!priv->map_render_pixbuf) return FALSE;

	e_map_world_to_window (map, point->longitude, point->latitude, &x, &y);
	gtk_widget_get_allocation (GTK_WIDGET (map), &allocation);

	if (x >= 0 && x < allocation.width &&
	    y >= 0 && y < allocation.height)
		return TRUE;

	return FALSE;
}

EMapPoint *
e_map_get_closest_point (EMap *map, gdouble longitude, gdouble latitude, gboolean in_view)
{
	EMapPrivate *priv;
	EMapPoint *point_chosen = NULL, *point;
	gdouble min_dist = 0.0, dist;
	gdouble dx, dy;
	gint i;

	priv = map->priv;

	for (i = 0; i < priv->points->len; i++)
	{
		point = g_ptr_array_index (priv->points, i);
		if (in_view && !e_map_point_is_in_view (map, point)) continue;

		dx = point->longitude - longitude;
		dy = point->latitude - latitude;
		dist = dx * dx + dy * dy;

		if (!point_chosen || dist < min_dist)
		{
			min_dist = dist;
			point_chosen = point;
		}
	}

	return point_chosen;
}

/* ------------------ *
 * Internal functions *
 * ------------------ */

static void
repaint_visible (EMap *map)
{
	GdkRectangle area;
	GtkAllocation allocation;

	gtk_widget_get_allocation (GTK_WIDGET (map), &allocation);

	area.x = 0;
	area.y = 0;
	area.width = allocation.width;
	area.height = allocation.height;

	request_paint_area (map, &area);
}

static void
update_and_paint (EMap *map)
{
	update_render_pixbuf (map, GDK_INTERP_BILINEAR, TRUE);
	repaint_visible (map);
}

static gint
load_map_background (EMap *view, gchar *name)
{
	EMapPrivate *priv;
	GdkPixbuf *pb0;

	priv = view->priv;

	pb0 = gdk_pixbuf_new_from_file (name, NULL);
	if (!pb0)
		return FALSE;

	if (priv->map_pixbuf) g_object_unref (priv->map_pixbuf);
	priv->map_pixbuf = pb0;
	update_render_pixbuf (view, GDK_INTERP_BILINEAR, TRUE);

	return TRUE;
}

static void
update_render_pixbuf (EMap *map,
                      GdkInterpType interp,
                      gboolean render_overlays)
{
	EMapPrivate *priv;
	EMapPoint *point;
	GtkAllocation allocation;
	gint width, height, orig_width, orig_height;
	gdouble zoom;
	gint i;

	if (!gtk_widget_get_realized (GTK_WIDGET (map)))
		return;

	gtk_widget_get_allocation (GTK_WIDGET (map), &allocation);

	/* Set up value shortcuts */

	priv = map->priv;
	width = allocation.width;
	height = allocation.height;
	orig_width = gdk_pixbuf_get_width (priv->map_pixbuf);
	orig_height = gdk_pixbuf_get_height (priv->map_pixbuf);

	/* Compute scaled width and height based on the extreme dimension */

	if ((gdouble) width / orig_width > (gdouble) height / orig_height)
		zoom = (gdouble) width / (gdouble) orig_width;
	else
		zoom = (gdouble) height / (gdouble) orig_height;

	if (priv->zoom_state == E_MAP_ZOOMED_IN)
		zoom *= 2.0;
	height = (orig_height * zoom) + 0.5;
	width = (orig_width * zoom) + 0.5;

	/* Reallocate the pixbuf */

	if (priv->map_render_pixbuf) g_object_unref (priv->map_render_pixbuf);
	priv->map_render_pixbuf = gdk_pixbuf_new (GDK_COLORSPACE_RGB, FALSE,	/* No alpha */
						  8, width, height);

	/* Scale the original map into the rendering pixbuf */

	if (width > 1 && height > 1)
	{
		gdk_pixbuf_scale (priv->map_pixbuf, priv->map_render_pixbuf, 0, 0,  /* Dest (x, y) */
				  width, height, 0, 0,				    /* Offset (x, y) */
				  zoom, zoom,					    /* Scale (x, y) */
				  interp);
	}

	if (render_overlays)
	{
		/* Add points */

		for (i = 0; i < priv->points->len; i++)
		{
			point = g_ptr_array_index (priv->points, i);
			update_render_point (map, point);
		}
	}

	/* Compute image offsets with respect to window */

	set_scroll_area (map);
}

/* Queues a repaint of the specified area in window coordinates */

static void
request_paint_area (EMap *view, GdkRectangle *area)
{
	EMapPrivate *priv;
	gint width, height;

	if (!gtk_widget_is_drawable (GTK_WIDGET (view)) ||
	    !gtk_widget_get_realized (GTK_WIDGET (view))) return;

	priv = view->priv;
	if (!priv->map_render_pixbuf) return;

	width = MIN (area->width, E_MAP_GET_WIDTH (view));
	height = MIN (area->height, E_MAP_GET_HEIGHT (view));

	/* This satisfies paranoia. To be removed */

	if (priv->xofs + width > gdk_pixbuf_get_width (priv->map_render_pixbuf))
		width = gdk_pixbuf_get_width (priv->map_render_pixbuf) - priv->xofs;

	if (priv->yofs + height > gdk_pixbuf_get_height (priv->map_render_pixbuf))
		height = gdk_pixbuf_get_height (priv->map_render_pixbuf) - priv->yofs;

	/* We rely on the fast case always being the case, since we load and
   * preprocess the source pixbuf ourselves */

	if (gdk_pixbuf_get_colorspace (priv->map_render_pixbuf) == GDK_COLORSPACE_RGB && !gdk_pixbuf_get_has_alpha (priv->map_render_pixbuf) &&
	    gdk_pixbuf_get_bits_per_sample (priv->map_render_pixbuf) == 8)
	{
		GtkStyle *style;
		GdkWindow *window;
		guchar *pixels;
		gint rowstride;

		style = gtk_widget_get_style (GTK_WIDGET (view));
		window = gtk_widget_get_window (GTK_WIDGET (view));

		rowstride = gdk_pixbuf_get_rowstride (priv->map_render_pixbuf);
		pixels = gdk_pixbuf_get_pixels (priv->map_render_pixbuf) +
			(area->y + priv->yofs) * rowstride + 3 *
			(area->x + priv->xofs);
		gdk_draw_rgb_image_dithalign (
			window, style->black_gc, area->x, area->y,
			width, height, GDK_RGB_DITHER_NORMAL, pixels,
			rowstride, 0, 0);
		return;
	}

#ifdef DEBUG
	g_print ("Doing hard redraw.\n");
#endif
}

static void
put_pixel_with_clipping (GdkPixbuf *pixbuf, gint x, gint y, guint rgba)
{
	gint    width, height;
	gint    rowstride, n_channels;
	guchar *pixels, *pixel;

	width      = gdk_pixbuf_get_width      (pixbuf);
	height     = gdk_pixbuf_get_height     (pixbuf);
	rowstride  = gdk_pixbuf_get_rowstride  (pixbuf);
	n_channels = gdk_pixbuf_get_n_channels (pixbuf);
	pixels     = gdk_pixbuf_get_pixels     (pixbuf);

	if (x < 0 || x >= width || y < 0 || y >= height)
		return;

	pixel = pixels + (y * rowstride) + (x * n_channels);

	*pixel       = (rgba >> 24);
	*(pixel + 1) = (rgba >> 16) & 0x000000ff;
	*(pixel + 2) = (rgba >>  8) & 0x000000ff;

	if (n_channels > 3)
	{
		*(pixel + 3) = rgba & 0x000000ff;
	}
}

/* Redraw point in client pixbuf */

static void
update_render_point (EMap *map, EMapPoint *point)
{
	EMapPrivate *priv;
	GdkPixbuf *pb;
	gdouble px, py;

	priv = map->priv;
	pb = priv->map_render_pixbuf;
	if (!pb) return;

	e_map_world_to_window (map, point->longitude, point->latitude, &px, &py);
	px += priv->xofs;
	py += priv->yofs;

	put_pixel_with_clipping (pb, px,     py,     point->rgba);
	put_pixel_with_clipping (pb, px - 1, py,     point->rgba);
	put_pixel_with_clipping (pb, px + 1, py,     point->rgba);
	put_pixel_with_clipping (pb, px,     py - 1, point->rgba);
	put_pixel_with_clipping (pb, px,     py + 1, point->rgba);

	put_pixel_with_clipping (pb, px - 2, py,     0x000000ff);
	put_pixel_with_clipping (pb, px + 2, py,     0x000000ff);
	put_pixel_with_clipping (pb, px,     py - 2, 0x000000ff);
	put_pixel_with_clipping (pb, px,     py + 2, 0x000000ff);
	put_pixel_with_clipping (pb, px - 1, py - 1, 0x000000ff);
	put_pixel_with_clipping (pb, px - 1, py + 1, 0x000000ff);
	put_pixel_with_clipping (pb, px + 1, py - 1, 0x000000ff);
	put_pixel_with_clipping (pb, px + 1, py + 1, 0x000000ff);
}

/* Repaint point on X server */

static void
repaint_point (EMap *map, EMapPoint *point)
{
	GdkRectangle area;
	gdouble px, py;

	if (!e_map_point_is_in_view (map, point)) return;

	e_map_world_to_window (map, point->longitude, point->latitude, &px, &py);

	area.x = (gint) px - 2;
	area.y = (gint) py - 2;
	area.width = 5;
	area.height = 5;
	request_paint_area (map, &area);
}

static void
center_at (EMap *map, gint x, gint y, gboolean scroll)
{
	EMapPrivate *priv;
	GtkAllocation allocation;
	gint pb_width, pb_height;

	priv = map->priv;

	pb_width = E_MAP_GET_WIDTH (map);
	pb_height = E_MAP_GET_HEIGHT (map);

	gtk_widget_get_allocation (GTK_WIDGET (map), &allocation);

	x = CLAMP (x - (allocation.width / 2), 0, pb_width - allocation.width);
	y = CLAMP (y - (allocation.height / 2), 0, pb_height - allocation.height);

	if (scroll)
		scroll_to (map, x, y);
	else {
		priv->xofs = x;
		priv->yofs = y;
	}
}

static void
smooth_center_at (EMap *map, gint x, gint y)
{
	EMapPrivate *priv;
	GtkAllocation allocation;
	gint pb_width, pb_height;
	gint dx, dy;

	priv = map->priv;

	pb_width = E_MAP_GET_WIDTH (map);
	pb_height = E_MAP_GET_HEIGHT (map);

	gtk_widget_get_allocation (GTK_WIDGET (map), &allocation);

	x = CLAMP (x - (allocation.width / 2), 0, pb_width - allocation.width);
	y = CLAMP (y - (allocation.height / 2), 0, pb_height - allocation.height);

	for (;;)
	{
		if (priv->xofs == x && priv->yofs == y)
			break;

		dx = (x < priv->xofs) ? -1 : (x > priv->xofs) ? 1 : 0;
		dy = (y < priv->yofs) ? -1 : (y > priv->yofs) ? 1 : 0;

		scroll_to (map, priv->xofs + dx, priv->yofs + dy);
	}
}

/* Scrolls the view to the specified offsets.  Does not perform range checking!  */

static void
scroll_to (EMap *view, gint x, gint y)
{
	EMapPrivate *priv;
	gint xofs, yofs;
	GdkWindow *window;
	GtkAllocation allocation;
	GdkGC *gc;
	gint src_x, src_y;
	gint dest_x, dest_y;
#if 0  /* see comment below */
	GdkEvent *event;
#endif

	priv = view->priv;

	/* Compute offsets and check bounds */

	xofs = x - priv->xofs;
	yofs = y - priv->yofs;

	if (xofs == 0 && yofs == 0)
		return;

	priv->xofs = x;
	priv->yofs = y;

	if (!gtk_widget_is_drawable (GTK_WIDGET (view)))
		return;

	gtk_widget_get_allocation (GTK_WIDGET (view), &allocation);

	if (abs (xofs) >= allocation.width || abs (yofs) >= allocation.height)
	{
		GdkRectangle area;

		area.x = 0;
		area.y = 0;
		area.width = allocation.width;
		area.height = allocation.height;

		request_paint_area (view, &area);
		return;
	}

	window = gtk_widget_get_window (GTK_WIDGET (view));

	/* Copy the window area */

	src_x = xofs < 0 ? 0 : xofs;
	src_y = yofs < 0 ? 0 : yofs;
	dest_x = xofs < 0 ? -xofs : 0;
	dest_y = yofs < 0 ? -yofs : 0;

	gc = gdk_gc_new (window);
	gdk_gc_set_exposures (gc, TRUE);

	gdk_draw_drawable (
		GDK_DRAWABLE (window),
		gc, GDK_DRAWABLE (window),
		src_x, src_y, dest_x, dest_y,
		allocation.width - abs (xofs),
		allocation.height - abs (yofs));

	g_object_unref (gc);

	/* Add the scrolled-in region */

	if (xofs)
	{
		GdkRectangle r;

		r.x = xofs < 0 ? 0 : allocation.width - xofs;
		r.y = 0;
		r.width = abs (xofs);
		r.height = allocation.height;

		request_paint_area (view, &r);
	}

	if (yofs)
	{
		GdkRectangle r;

		r.x = 0;
		r.y = yofs < 0 ? 0 : allocation.height - yofs;
		r.width = allocation.width;
		r.height = abs (yofs);

		request_paint_area (view, &r);
	}

	/* Process graphics exposures */

	/* XXX gdk_event_get_graphics_expose() is deprecated now.
	 *     The map widget seems to work fine without this logic
	 *     (I think it was just an optimization) but leaving it
	 *     intact in case I'm wrong and we need to rewrite it. */
#if 0
	while ((event = gdk_event_get_graphics_expose (window)) != NULL)
	{
		gtk_widget_event (GTK_WIDGET (view), event);

		if (event->expose.count == 0)
		{
			gdk_event_free (event);
			break;
		}

		gdk_event_free (event);
	}
#endif
}

static gint divide_seq[] =
{
	/* Dividends for divisor of 2 */

	-2,

	1,

	/* Dividends for divisor of 4 */

	-4,

	1, 3,

	/* Dividends for divisor of 8 */

	-8,

	1, 5, 3, 7,

	/* Dividends for divisor of 16 */

	-16,

	1, 9, 5, 13, 3, 11, 7, 15,

	/* Dividends for divisor of 32 */

	-32,

	1, 17, 9, 25, 5, 21, 13, 29, 3, 19,
	11, 27, 7, 23, 15, 31,

	/* Dividends for divisor of 64 */

	-64,

	1, 33, 17, 49, 9, 41, 25, 57, 5, 37,
	21, 53, 13, 45, 29, 61, 3, 35, 19, 51,
	11, 43, 27, 59, 7, 39, 23, 55, 15, 47,
	31, 63,

	/* Dividends for divisor of 128 */

	-128,

	1, 65, 33, 97, 17, 81, 49, 113, 9, 73,
	41, 105, 25, 89, 57, 121, 5, 69, 37, 101,
	21, 85, 53, 117, 13, 77, 45, 109, 29, 93,
	61, 125, 3, 67, 35, 99, 19, 83, 51, 115,
	11, 75, 43, 107, 27, 91, 59, 123, 7, 71,
	39, 103, 23, 87, 55, 119, 15, 79, 47, 111,
	31, 95, 63, 127,

	/* Dividends for divisor of 256 */

	-256,

	1, 129, 65, 193, 33, 161, 97, 225, 17, 145,
	81, 209, 49, 177, 113, 241, 9, 137, 73, 201,
	41, 169, 105, 233, 25, 153, 89, 217, 57, 185,
	121, 249, 5, 133, 69, 197, 37, 165, 101, 229,
	21, 149, 85, 213, 53, 181, 117, 245, 13, 141,
	77, 205, 45, 173, 109, 237, 29, 157, 93, 221,
	61, 189, 125, 253, 3, 131, 67, 195, 35, 163,
	99, 227, 19, 147, 83, 211, 51, 179, 115, 243,
	11, 139, 75, 203, 43, 171, 107, 235, 27, 155,
	91, 219, 59, 187, 123, 251, 7, 135, 71, 199,
	39, 167, 103, 231, 23, 151, 87, 215, 55, 183,
	119, 247, 15, 143, 79, 207, 47, 175, 111, 239,
	31, 159, 95, 223, 63, 191, 127, 255,

	0
};

typedef enum
{
	AXIS_X,
	AXIS_Y
}
AxisType;

static void
blowup_window_area (GdkWindow *window, gint area_x, gint area_y, gint target_x, gint target_y, gint total_width, gint total_height, gfloat zoom_factor)
{
	GdkGC *gc;
	AxisType strong_axis;
	gfloat axis_factor, axis_counter;
	gint zoom_chunk;
	gint divisor_width = 0, divisor_height = 0;
	gint divide_width_index, divide_height_index;
	gint area_width, area_height;
	gint i, j;
	gint line;

	/* Set up the GC we'll be using */

	gc = gdk_gc_new (window);
	gdk_gc_set_exposures (gc, FALSE);

	/* Get area constraints */

	gdk_drawable_get_size (GDK_DRAWABLE (window), &area_width, &area_height);

	/* Initialize area division array indexes */

	divide_width_index = divide_height_index = 0;

	/* Initialize axis counter */

	axis_counter = 0.0;

	/* Find the strong axis (which is the basis for iteration) and the ratio
	 * at which the other axis will be scaled.
	 *
	 * Also determine how many lines to expand in one fell swoop, and store
	 * this figure in zoom_chunk. */

	if (area_width > area_height)
	{
		strong_axis = AXIS_X;
		axis_factor = (gdouble) area_height / (gdouble) area_width;
		zoom_chunk = MAX (1, area_width / 250);
		i = (area_width * (zoom_factor - 1.0)) / zoom_chunk;
	}
	else
	{
		strong_axis = AXIS_Y;
		axis_factor = (gdouble) area_width / (gdouble) area_height;
		zoom_chunk = MAX (1, area_height / 250);
		i = (area_height * (zoom_factor - 1.0)) / zoom_chunk;
	}

	/* Go, go, devil bunnies! Gogo devil bunnies! */

	for (; i > 0; i--)
	{
		/* Reset division sequence table indexes as necessary */

		if (!divide_seq[divide_width_index]) divide_width_index = 0;
		if (!divide_seq[divide_height_index]) divide_height_index = 0;

		/* Set new divisor if found in table */

		if (divide_seq[divide_width_index] < 0)
			divisor_width = abs (divide_seq[divide_width_index++]);
		if (divide_seq[divide_height_index] < 0)
			divisor_height = abs (divide_seq[divide_height_index++]);

		/* Widen */

		if (strong_axis == AXIS_X || axis_counter >= 1.0)
		{
			line = ((divide_seq[divide_width_index] * area_width) / divisor_width) + 0.5;

			if ((line < target_x && target_x > area_width / 2) || (line > target_x && target_x > (area_width / 2) + zoom_chunk))
			{
				/* Push left */

				for (j = 0; j < zoom_chunk - 1; j++)
					gdk_draw_drawable (GDK_DRAWABLE (window), gc, GDK_DRAWABLE (window), line, 0, line + j + 1, 0, 1, area_height);

				gdk_draw_drawable (GDK_DRAWABLE (window), gc, GDK_DRAWABLE (window), zoom_chunk, 0, 0, 0, line, area_height);
				if (line > target_x) target_x -= zoom_chunk;
			}
			else
			{
				/* Push right */

				for (j = 0; j < zoom_chunk - 1; j++)
					gdk_draw_drawable (GDK_DRAWABLE (window), gc, GDK_DRAWABLE (window), line - zoom_chunk, 0, line + j - (zoom_chunk - 1), 0, 1, area_height);

				gdk_draw_drawable (GDK_DRAWABLE (window), gc, GDK_DRAWABLE (window), line - zoom_chunk, 0, line, 0, area_width - line, area_height);
				if (line < target_x) target_x += zoom_chunk;
			}
		}

		if (strong_axis == AXIS_Y || axis_counter >= 1.0)
		{
			/* Heighten */

			line = ((divide_seq[divide_height_index] * area_height) / divisor_height) + 0.5;

			if ((line < target_y && target_y > area_height / 2) || (line > target_y && target_y > (area_height / 2) + zoom_chunk))
			{
				/* Push up */

				for (j = 0; j < zoom_chunk - 1; j++)
					gdk_draw_drawable (GDK_DRAWABLE (window), gc, GDK_DRAWABLE (window), 0, line, 0, line + j + 1, area_width, 1);

				gdk_draw_drawable (GDK_DRAWABLE (window), gc, GDK_DRAWABLE (window), 0, zoom_chunk, 0, 0, area_width, line);
				if (line > target_y) target_y -= zoom_chunk;
			}
			else
			{
				/* Push down */

				for (j = 0; j < zoom_chunk - 1; j++)
					gdk_draw_drawable (GDK_DRAWABLE (window), gc, GDK_WINDOW (window), 0, line - zoom_chunk, 0, line + j - (zoom_chunk - 1), area_width, 1);

				gdk_draw_drawable (GDK_DRAWABLE (window), gc, GDK_DRAWABLE (window), 0, line - zoom_chunk, 0, line, area_width, area_height - line);
				if (line < target_y) target_y += zoom_chunk;
			}
		}

		divide_width_index++;
		divide_height_index++;
		if (axis_counter >= 1.0) axis_counter -= 1.0;
		axis_counter += axis_factor;
	}

	/* Free our GC */

	g_object_unref (gc);
}

static void
zoom_in_smooth (EMap *map)
{
	GtkAllocation allocation;
	GdkRectangle area;
	EMapPrivate *priv;
	GdkWindow *window;
	gint width, height;
	gdouble x, y;

	g_return_if_fail (map);
	g_return_if_fail (gtk_widget_get_realized (GTK_WIDGET (map)));

	gtk_widget_get_allocation (GTK_WIDGET (map), &allocation);

	area.x = 0;
	area.y = 0;
	area.width = allocation.width;
	area.height = allocation.height;

	priv = map->priv;
	window = gtk_widget_get_window (GTK_WIDGET (map));
	width = gdk_pixbuf_get_width (priv->map_render_pixbuf);
	height = gdk_pixbuf_get_height (priv->map_render_pixbuf);

	/* Center the target point as much as possible */

	e_map_world_to_window (map, priv->zoom_target_long, priv->zoom_target_lat, &x, &y);
	smooth_center_at (map, x + priv->xofs, y + priv->yofs);

	/* Render and paint a temporary map without overlays, so they don't get in
	 * the way (look ugly) while zooming */

	update_render_pixbuf (map, GDK_INTERP_BILINEAR, FALSE);
	request_paint_area (map, &area);

	/* Find out where in the area we're going to zoom to */

	e_map_world_to_window (map, priv->zoom_target_long, priv->zoom_target_lat, &x, &y);

	/* Pre-render the zoomed-in map, so we can put it there quickly when the
	 * blowup sequence ends */

	priv->zoom_state = E_MAP_ZOOMED_IN;
	update_render_pixbuf (map, GDK_INTERP_BILINEAR, TRUE);

	/* Do the blowup */

	blowup_window_area (window, priv->xofs, priv->yofs, x, y, width, height, 1.68);

	/* Set new scroll offsets and paint the zoomed map */

	e_map_world_to_window (map, priv->zoom_target_long, priv->zoom_target_lat, &x, &y);
	priv->xofs = CLAMP (priv->xofs + x - area.width / 2.0, 0, E_MAP_GET_WIDTH (map) - area.width);
	priv->yofs = CLAMP (priv->yofs + y - area.height / 2.0, 0, E_MAP_GET_HEIGHT (map) - area.height);

	request_paint_area (map, &area);
}

static void
zoom_in (EMap *map)
{
	GtkAllocation allocation;
	GdkRectangle area;
	EMapPrivate *priv;
	gdouble x, y;

	priv = map->priv;

	gtk_widget_get_allocation (GTK_WIDGET (map), &allocation);

	area.x = 0;
	area.y = 0;
	area.width = allocation.width;
	area.height = allocation.height;

	priv->zoom_state = E_MAP_ZOOMED_IN;

	update_render_pixbuf (map, GDK_INTERP_BILINEAR, TRUE);

	e_map_world_to_window (
		map, priv->zoom_target_long,
		priv->zoom_target_lat, &x, &y);
	priv->xofs = CLAMP (
		priv->xofs + x - area.width / 2.0,
		0, E_MAP_GET_WIDTH (map) - area.width);
	priv->yofs = CLAMP (
		priv->yofs + y - area.height / 2.0,
		0, E_MAP_GET_HEIGHT (map) - area.height);

	request_paint_area (map, &area);
}

static void
zoom_out (EMap *map)
{
	GtkAllocation allocation;
	GdkRectangle area;
	EMapPrivate *priv;
	gdouble longitude, latitude;
	gdouble x, y;

	priv = map->priv;

	gtk_widget_get_allocation (GTK_WIDGET (map), &allocation);

	area.x = 0;
	area.y = 0;
	area.width = allocation.width;
	area.height = allocation.height;

	/* Must be done before update_render_pixbuf() */

	e_map_window_to_world (
		map, area.width / 2, area.height / 2,
		&longitude, &latitude);

	priv->zoom_state = E_MAP_ZOOMED_OUT;
	update_render_pixbuf (map, GDK_INTERP_BILINEAR, TRUE);

	e_map_world_to_window (map, longitude, latitude, &x, &y);
	center_at (map, x + priv->xofs, y + priv->yofs, FALSE);
/*	request_paint_area (map, &area); */
	repaint_visible (map);
}

static void
zoom_do (EMap *map)
{
	EMapPrivate *priv;

	priv = map->priv;
	g_signal_handlers_block_matched (priv->hadj, G_SIGNAL_MATCH_DATA, 0, 0, NULL, NULL, map);
	g_signal_handlers_block_matched (priv->vadj, G_SIGNAL_MATCH_DATA, 0, 0, NULL, NULL, map);

	if (priv->zoom_state == E_MAP_ZOOMING_IN)
	{
		if (e_map_get_smooth_zoom (map)) zoom_in_smooth (map);
		else zoom_in (map);
	}
	else if (priv->zoom_state == E_MAP_ZOOMING_OUT)
	{
/*    if (e_map_get_smooth_zoom(map)) zoom_out_smooth(map); */
		zoom_out (map);
	}

	g_signal_handlers_unblock_matched (priv->hadj, G_SIGNAL_MATCH_DATA, 0, 0, NULL, NULL, map);
	g_signal_handlers_unblock_matched (priv->vadj, G_SIGNAL_MATCH_DATA, 0, 0, NULL, NULL, map);

	set_scroll_area (map);
}

/* Callback used when an adjustment is changed */

static void
adjustment_changed_cb (GtkAdjustment *adj, gpointer data)
{
	EMap *view;
	gint hadj_value;
	gint vadj_value;

	view = E_MAP (data);

	hadj_value = gtk_adjustment_get_value (view->priv->hadj);
	vadj_value = gtk_adjustment_get_value (view->priv->vadj);

	scroll_to (view, hadj_value, vadj_value);
}

static void
set_scroll_area (EMap *view)
{
	EMapPrivate *priv;
	GtkAllocation allocation;
	gint upper, page_size;

	priv = view->priv;

	if (!gtk_widget_get_realized (GTK_WIDGET (view))) return;
	if (!priv->hadj || !priv->vadj) return;

	g_object_freeze_notify (G_OBJECT (priv->hadj));
	g_object_freeze_notify (G_OBJECT (priv->vadj));

	gtk_widget_get_allocation (GTK_WIDGET (view), &allocation);

	/* Set scroll increments */

	gtk_adjustment_set_page_size (priv->hadj, allocation.width);
	gtk_adjustment_set_page_increment (priv->hadj, allocation.width / 2);
	gtk_adjustment_set_step_increment (priv->hadj, SCROLL_STEP_SIZE);

	gtk_adjustment_set_page_size (priv->vadj, allocation.height);
	gtk_adjustment_set_page_increment (priv->vadj, allocation.height / 2);
	gtk_adjustment_set_step_increment (priv->vadj, SCROLL_STEP_SIZE);

	/* Set scroll bounds and new offsets */

	gtk_adjustment_set_lower (priv->hadj, 0);
	if (priv->map_render_pixbuf) {
		gint width = gdk_pixbuf_get_width (priv->map_render_pixbuf);
		gtk_adjustment_set_upper (priv->hadj, width);
	}

	gtk_adjustment_set_lower (priv->vadj, 0);
	if (priv->map_render_pixbuf) {
		gint height = gdk_pixbuf_get_height (priv->map_render_pixbuf);
		gtk_adjustment_set_upper (priv->vadj, height);
	}

	g_object_thaw_notify (G_OBJECT (priv->hadj));
	g_object_thaw_notify (G_OBJECT (priv->vadj));

	upper = gtk_adjustment_get_upper (priv->hadj);
	page_size = gtk_adjustment_get_page_size (priv->hadj);
	priv->xofs = CLAMP (priv->xofs, 0, upper - page_size);

	upper = gtk_adjustment_get_upper (priv->vadj);
	page_size = gtk_adjustment_get_page_size (priv->vadj);
	priv->yofs = CLAMP (priv->yofs, 0, upper - page_size);

	g_signal_handlers_block_matched (
		priv->hadj, G_SIGNAL_MATCH_DATA, 0, 0, NULL, NULL, view);
	gtk_adjustment_set_value (priv->hadj, priv->xofs);
	g_signal_handlers_unblock_matched (
		priv->hadj, G_SIGNAL_MATCH_DATA, 0, 0, NULL, NULL, view);

	g_signal_handlers_block_matched (
		priv->vadj, G_SIGNAL_MATCH_DATA, 0, 0, NULL, NULL, view);
	gtk_adjustment_set_value (priv->vadj, priv->yofs);
	g_signal_handlers_unblock_matched (
		priv->vadj, G_SIGNAL_MATCH_DATA, 0, 0, NULL, NULL, view);
}
