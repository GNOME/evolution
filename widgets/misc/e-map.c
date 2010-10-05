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

/* backward-compatibility cruft */
#include "e-util/gtk-compat.h"

#define E_MAP_TWEEN_TIMEOUT_MSECS 25

/* Scroll step increment */

#define SCROLL_STEP_SIZE 32

/* */

#define E_MAP_GET_WIDTH(map) gtk_adjustment_get_upper((map)->priv->hadj)
#define E_MAP_GET_HEIGHT(map) gtk_adjustment_get_upper((map)->priv->vadj)

/* Zoom state - keeps track of animation hacks */

typedef enum
{
	E_MAP_ZOOMED_IN,
	E_MAP_ZOOMED_OUT,
	E_MAP_ZOOMING_IN,
	E_MAP_ZOOMING_OUT
}
EMapZoomState;

/* The Tween struct used for zooming */

typedef struct _EMapTween EMapTween;

struct _EMapTween {
        guint start_time;
        guint end_time;
        double longitude_offset;
        double latitude_offset;
        double zoom_factor;
};

/* Private part of the EMap structure */

struct _EMapPrivate {
	/* Pointer to map image */
	GdkPixbuf *map_pixbuf;
        cairo_surface_t *map_render_surface;

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

        /* Tweens */
        GSList *tweens;
        GTimer *timer;
        guint timer_current_ms;
        guint tween_id;
};

/* Internal prototypes */

static void e_map_finalize (GObject *object);
static void e_map_realize (GtkWidget *widget);
static void e_map_unrealize (GtkWidget *widget);
static void e_map_size_request (GtkWidget *widget, GtkRequisition *requisition);
static void e_map_size_allocate (GtkWidget *widget, GtkAllocation *allocation);
static gint e_map_button_press (GtkWidget *widget, GdkEventButton *event);
static gint e_map_button_release (GtkWidget *widget, GdkEventButton *event);
static gint e_map_motion (GtkWidget *widget, GdkEventMotion *event);
static gint e_map_expose (GtkWidget *widget, GdkEventExpose *event);
static gint e_map_key_press (GtkWidget *widget, GdkEventKey *event);
static void e_map_set_scroll_adjustments (GtkWidget *widget, GtkAdjustment *hadj, GtkAdjustment *vadj);

static void e_map_get_current_location (EMap *map, double *longitude, double *latitude);
static void e_map_world_to_render_surface (EMap *map, gdouble world_longitude, gdouble world_latitude,
                                           gdouble *win_x, gdouble *win_y);
static void update_render_surface (EMap *map, gboolean render_overlays);
static void set_scroll_area (EMap *view, int width, int height);
static void center_at (EMap *map, double longitude, double latitude);
static void scroll_to (EMap *view, gint x, gint y);
static gint load_map_background (EMap *view, gchar *name);
static void adjustment_changed_cb (GtkAdjustment *adj, gpointer data);
static void update_and_paint (EMap *map);
static void update_render_point (EMap *map, EMapPoint *point);
static void repaint_point (EMap *map, EMapPoint *point);

/* ------ *
 * Tweens *
 * ------ */

static gboolean
e_map_is_tweening (EMap *view)
{
        return view->priv->timer != NULL;
}

static void
e_map_stop_tweening (EMap *view)
{
        EMapPrivate *priv = view->priv;

        g_assert (priv->tweens == NULL);

        if (!e_map_is_tweening (view))
                return;

        g_timer_destroy (priv->timer);
        priv->timer = NULL;
        g_source_remove (priv->tween_id);
        priv->tween_id = 0;
}

static void
e_map_tween_destroy (EMap *view, EMapTween *tween)
{
        EMapPrivate *priv = view->priv;

        priv->tweens = g_slist_remove (priv->tweens, tween);
        g_slice_free (EMapTween, tween);

        if (priv->tweens == NULL)
                e_map_stop_tweening (view);
}

static gboolean
e_map_do_tween_cb (gpointer data)
{
        EMap *view = data;
        EMapPrivate *priv = view->priv;
        GSList *walk;

        priv->timer_current_ms = g_timer_elapsed (priv->timer, NULL) * 1000;
        gtk_widget_queue_draw (GTK_WIDGET (view));
        
        /* Can't use for loop here, because we need to advance 
         * the list before deleting.
         */
        walk = priv->tweens;
        while (walk)
        {
                EMapTween *tween = walk->data;
                
                walk = walk->next;

                if (tween->end_time <= priv->timer_current_ms)
                        e_map_tween_destroy (view, tween);
        }

        return TRUE;
}

static void
e_map_start_tweening (EMap *view)
{
        EMapPrivate *priv = view->priv;

        if (e_map_is_tweening (view))
                return;

        priv->timer = g_timer_new ();
        priv->timer_current_ms = 0;
        priv->tween_id = gdk_threads_add_timeout (E_MAP_TWEEN_TIMEOUT_MSECS,
                                                  e_map_do_tween_cb,
                                                  view);
        g_timer_start (priv->timer);
}

static void
e_map_tween_new (EMap *view, guint msecs, double longitude_offset, double latitude_offset, double zoom_factor)
{
        EMapPrivate *priv = view->priv;
        EMapTween *tween;
        
        if (!priv->smooth_zoom)
                return;

        e_map_start_tweening (view);

        tween = g_slice_new (EMapTween);

        tween->start_time = priv->timer_current_ms;
        tween->end_time = tween->start_time + msecs;
        tween->longitude_offset = longitude_offset;
        tween->latitude_offset = latitude_offset;
        tween->zoom_factor = zoom_factor;

        priv->tweens = g_slist_prepend (priv->tweens, tween);

        gtk_widget_queue_draw (GTK_WIDGET (view));
}

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
	GtkWidgetClass *widget_class;

	gobject_class = (GObjectClass *) class;
	widget_class = (GtkWidgetClass *) class;

	gobject_class->finalize = e_map_finalize;

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
	widget_class->unrealize = e_map_unrealize;
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

        while (priv->tweens)
                e_map_tween_destroy (view, priv->tweens->data);
        e_map_stop_tweening (view);

	g_signal_handlers_disconnect_by_func (priv->hadj, adjustment_changed_cb, view);
	g_signal_handlers_disconnect_by_func (priv->vadj, adjustment_changed_cb, view);

	g_object_unref ((priv->hadj));
	priv->hadj = NULL;

	g_object_unref ((priv->vadj));
	priv->vadj = NULL;

	if (priv->map_pixbuf)
	{
		g_object_unref (priv->map_pixbuf);
		priv->map_pixbuf = NULL;
	}

        /* gone in unrealize */
        g_assert (priv->map_render_surface == NULL);

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
	update_render_surface (E_MAP (widget), TRUE);
}

static void
e_map_unrealize (GtkWidget *widget)
{
        EMap *view = E_MAP (widget);
        EMapPrivate *priv = view->priv;

        cairo_surface_destroy (priv->map_render_surface);
        priv->map_render_surface = NULL;
 
        if (GTK_WIDGET_CLASS (e_map_parent_class)->unrealize)
                (*GTK_WIDGET_CLASS (e_map_parent_class)->unrealize) (widget);
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

                gtk_widget_queue_draw (widget);
	}

	update_render_surface (view, TRUE);
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

static double
e_map_get_tween_effect (EMap *view, EMapTween *tween)
{
  double elapsed;

  elapsed = (double) (view->priv->timer_current_ms - tween->start_time) / tween->end_time;

  return MAX (0.0, 1.0 - elapsed);
}

static void
e_map_apply_tween (EMapTween *tween, double effect, double *longitude, double *latitude, double *zoom)
{
  *zoom *= pow (tween->zoom_factor, effect);
  *longitude += tween->longitude_offset * effect;
  *latitude += tween->latitude_offset * effect;
}

static void
e_map_tweens_compute_matrix (EMap *view, cairo_matrix_t *matrix)
{
	EMapPrivate *priv = view->priv;
        GSList *walk;
        double zoom, x, y, latitude, longitude, effect;
        GtkAllocation allocation;

        if (!e_map_is_tweening (view))
        {
                cairo_matrix_init_translate (matrix, -priv->xofs, -priv->yofs);
                return;
        }

        e_map_get_current_location (view, &longitude, &latitude);
        zoom = 1.0;

        for (walk = priv->tweens; walk; walk = walk->next)
        {
                EMapTween *tween = walk->data;

                effect = e_map_get_tween_effect (view, tween);
                e_map_apply_tween (tween, effect, &longitude, &latitude, &zoom);
        }

        gtk_widget_get_allocation (GTK_WIDGET (view), &allocation);
        cairo_matrix_init_translate (matrix,
                                allocation.width / 2.0,
                                allocation.height / 2.0);
        
        e_map_world_to_render_surface (view,
                                       longitude, latitude,
                                       &x, &y);
        cairo_matrix_scale (matrix, zoom, zoom);
        cairo_matrix_translate (matrix, -x, -y);
}

/* Expose handler for the map view */

static gboolean
e_map_expose (GtkWidget *widget, GdkEventExpose *event)
{
	EMap *view;
	EMapPrivate *priv;
        cairo_t *cr;
        cairo_matrix_t matrix;

	if (!gtk_widget_is_drawable (widget))
	        return FALSE;

	view = E_MAP (widget);
	priv = view->priv;

        cr = gdk_cairo_create (event->window);
        gdk_cairo_region (cr, event->region);
        cairo_clip (cr);

        e_map_tweens_compute_matrix (view, &matrix);
        cairo_transform (cr, &matrix);

        cairo_set_source_surface (cr, 
                                  priv->map_render_surface,
                                  0, 0);
        cairo_paint (cr);
        
        cairo_destroy (cr);

	return FALSE;
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
		case GDK_KEY_Up:
			do_scroll = TRUE;
			xofs = 0;
			yofs = -SCROLL_STEP_SIZE;
			break;

		case GDK_KEY_Down:
			do_scroll = TRUE;
			xofs = 0;
			yofs = SCROLL_STEP_SIZE;
			break;

		case GDK_KEY_Left:
			do_scroll = TRUE;
			xofs = -SCROLL_STEP_SIZE;
			yofs = 0;
			break;

		case GDK_KEY_Right:
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

		gtk_adjustment_set_value (priv->hadj, x);
		gtk_adjustment_set_value (priv->vadj, y);
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

	width = E_MAP_GET_WIDTH (map);
	height = E_MAP_GET_HEIGHT (map);

	*world_longitude = (win_x + priv->xofs - (gdouble) width / 2.0) /
		((gdouble) width / 2.0) * 180.0;
	*world_latitude = ((gdouble) height / 2.0 - win_y - priv->yofs) /
		((gdouble) height / 2.0) * 90.0;
}

static void
e_map_world_to_render_surface (EMap *map, gdouble world_longitude, gdouble world_latitude, gdouble *win_x, gdouble *win_y)
{
	gint width, height;

	width = E_MAP_GET_WIDTH (map);
	height = E_MAP_GET_HEIGHT (map);

	*win_x = (width / 2.0 + (width / 2.0) * world_longitude / 180.0);
	*win_y = (height / 2.0 - (height / 2.0) * world_latitude / 90.0);
}

void
e_map_world_to_window (EMap *map, gdouble world_longitude, gdouble world_latitude, gdouble *win_x, gdouble *win_y)
{
        EMapPrivate *priv;

	g_return_if_fail (E_IS_MAP (map));
	g_return_if_fail (gtk_widget_get_realized (GTK_WIDGET (map)));
	g_return_if_fail (world_longitude >= -180.0 && world_longitude <= 180.0);
	g_return_if_fail (world_latitude >= -90.0 && world_latitude <= 90.0);

        priv = map->priv;

        e_map_world_to_render_surface (map, world_longitude, world_latitude, win_x, win_y);
        *win_x -= priv->xofs;
        *win_y -= priv->yofs;

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

static void
e_map_set_zoom (EMap *view, EMapZoomState zoom)
{
	EMapPrivate *priv = view->priv;

        if (priv->zoom_state == zoom)
                return;

        priv->zoom_state = zoom;
        update_render_surface (view, TRUE);
        gtk_widget_queue_draw (GTK_WIDGET (view));
}

void
e_map_zoom_to_location (EMap *map, gdouble longitude, gdouble latitude)
{
	EMapPrivate *priv;
        double prevlong, prevlat;
        double prevzoom;

	g_return_if_fail (map);
	g_return_if_fail (gtk_widget_get_realized (GTK_WIDGET (map)));

	priv = map->priv;
        e_map_get_current_location (map, &prevlong, &prevlat);
        prevzoom = e_map_get_magnification (map);

        e_map_set_zoom (map, E_MAP_ZOOMED_IN);
        center_at (map, longitude, latitude);
        /* need to reget location, centering might have clipped it */
        e_map_get_current_location (map, &longitude, &latitude);

        e_map_tween_new (map,
                         150,
                         prevlong - longitude,
                         prevlat - latitude,
                         prevzoom / e_map_get_magnification (map));
}

void
e_map_zoom_out (EMap *map)
{
        double longitude, latitude, actual_longitude, actual_latitude;
        double prevzoom;

	g_return_if_fail (map);
	g_return_if_fail (gtk_widget_get_realized (GTK_WIDGET (map)));

        e_map_get_current_location (map, &longitude, &latitude);
        prevzoom = e_map_get_magnification (map);
        e_map_set_zoom (map, E_MAP_ZOOMED_OUT);
        center_at (map, longitude, latitude);

        /* need to reget location, centering might have clipped it */
        e_map_get_current_location (map, &actual_longitude, &actual_latitude);

        e_map_tween_new (map,
                         150,
                         longitude - actual_longitude,
                         latitude - actual_latitude,
                         prevzoom / e_map_get_magnification (map));
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

		update_render_surface (map, TRUE);
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
	if (!priv->map_render_surface) return FALSE;

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
update_and_paint (EMap *map)
{
	update_render_surface (map, TRUE);
	gtk_widget_queue_draw (GTK_WIDGET (map));
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
	update_render_surface (view, TRUE);

	return TRUE;
}

static void
update_render_surface (EMap *map,
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

        if (priv->map_render_surface) cairo_surface_destroy (priv->map_render_surface);
        priv->map_render_surface = gdk_window_create_similar_surface (gtk_widget_get_window (GTK_WIDGET (map)),
                                                                      CAIRO_CONTENT_COLOR,
                                                                      width, height);

	/* Scale the original map into the rendering pixbuf */

	if (width > 1 && height > 1)
	{
                cairo_t *cr = cairo_create (priv->map_render_surface);
                cairo_scale (cr, (double) width / orig_width, (double) height / orig_height);
                gdk_cairo_set_source_pixbuf (cr, priv->map_pixbuf, 0, 0);
                cairo_paint (cr);
                cairo_destroy (cr);
	}

        /* Compute image offsets with respect to window */

        set_scroll_area (map, width, height);

	if (render_overlays)
	{
		/* Add points */

		for (i = 0; i < priv->points->len; i++)
		{
			point = g_ptr_array_index (priv->points, i);
			update_render_point (map, point);
		}
	}
}

/* Redraw point in client surface */

static void
update_render_point (EMap *map, EMapPoint *point)
{
	EMapPrivate *priv;
        cairo_t *cr;
	gdouble px, py;
        static guchar mask1[] = { 0x00, 0x00, 0xff, 0x00, 0x00,  0x00, 0x00, 0x00,
                                  0x00, 0xff, 0x00, 0xff, 0x00,  0x00, 0x00, 0x00,
                                  0xff, 0x00, 0x00, 0x00, 0xff,  0x00, 0x00, 0x00,
                                  0x00, 0xff, 0x00, 0xff, 0x00,  0x00, 0x00, 0x00,
                                  0x00, 0x00, 0xff, 0x00, 0x00,  0x00, 0x00, 0x00 };
        static guchar mask2[] = { 0x00, 0xff, 0x00,  0x00,
                                  0xff, 0xff, 0xff,  0x00,
                                  0x00, 0xff, 0x00,  0x00 };
        cairo_surface_t *mask;

	priv = map->priv;

        if (priv->map_render_surface == NULL)
                return;

        cr = cairo_create (priv->map_render_surface);
        cairo_set_operator (cr, CAIRO_OPERATOR_SOURCE);

	e_map_world_to_window (map, point->longitude, point->latitude, &px, &py);
	px = floor (px + priv->xofs);
	py = floor (py + priv->yofs);

        cairo_set_source_rgb (cr, 0, 0, 0);
        mask = cairo_image_surface_create_for_data (mask1, CAIRO_FORMAT_A8, 5, 5, 8);
        cairo_mask_surface (cr, mask, px - 2, py - 2);
        cairo_surface_destroy (mask);

        cairo_set_source_rgba (cr,
                               ((point->rgba >> 24) & 0xff) / 255.0,
                               ((point->rgba >> 16) & 0xff) / 255.0,
                               ((point->rgba >>  8) & 0xff) / 255.0,
                               ( point->rgba        & 0xff) / 255.0);
        mask = cairo_image_surface_create_for_data (mask2, CAIRO_FORMAT_A8, 3, 3, 4);
        cairo_mask_surface (cr, mask, px - 1, py - 1);
        cairo_surface_destroy (mask);

        cairo_destroy (cr);
}

/* Repaint point on X server */

static void
repaint_point (EMap *map, EMapPoint *point)
{
	gdouble px, py;

        if (!gtk_widget_is_drawable (GTK_WIDGET (map)))
          return;

	e_map_world_to_window (map, point->longitude, point->latitude, &px, &py);

	gtk_widget_queue_draw_area (GTK_WIDGET (map),
                                    (gint) px - 2, (gint) py - 2,
                                    5, 5);
}

static void
center_at (EMap *map, double longitude, double latitude)
{
	EMapPrivate *priv;
	GtkAllocation allocation;
	gint pb_width, pb_height;
        double x, y;

        e_map_world_to_render_surface (map, longitude, latitude, &x, &y);

	priv = map->priv;

	pb_width = E_MAP_GET_WIDTH (map);
	pb_height = E_MAP_GET_HEIGHT (map);

	gtk_widget_get_allocation (GTK_WIDGET (map), &allocation);

	x = CLAMP (x - (allocation.width / 2), 0, pb_width - allocation.width);
	y = CLAMP (y - (allocation.height / 2), 0, pb_height - allocation.height);

        gtk_adjustment_set_value (priv->hadj, x);
        gtk_adjustment_set_value (priv->vadj, y);

        gtk_widget_queue_draw (GTK_WIDGET (map));
}

/* Scrolls the view to the specified offsets.  Does not perform range checking!  */

static void
scroll_to (EMap *view, gint x, gint y)
{
	EMapPrivate *priv;
	gint xofs, yofs;

	priv = view->priv;

	/* Compute offsets and check bounds */

	xofs = x - priv->xofs;
	yofs = y - priv->yofs;

	if (xofs == 0 && yofs == 0)
		return;

	priv->xofs = x;
	priv->yofs = y;

        gtk_widget_queue_draw (GTK_WIDGET (view));
}

static void
e_map_get_current_location (EMap *map, double *longitude, double *latitude)
{
	GtkAllocation allocation;

	gtk_widget_get_allocation (GTK_WIDGET (map), &allocation);

	e_map_window_to_world (map,
                               allocation.width / 2.0, allocation.height / 2.0,
		               longitude, latitude);
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
set_scroll_area (EMap *view, int width, int height)
{
	EMapPrivate *priv;
	GtkAllocation allocation;

	priv = view->priv;

	if (!gtk_widget_get_realized (GTK_WIDGET (view))) return;
	if (!priv->hadj || !priv->vadj) return;

	g_object_freeze_notify (G_OBJECT (priv->hadj));
	g_object_freeze_notify (G_OBJECT (priv->vadj));

	gtk_widget_get_allocation (GTK_WIDGET (view), &allocation);

	priv->xofs = CLAMP (priv->xofs, 0, width - allocation.width);
	priv->yofs = CLAMP (priv->yofs, 0, height - allocation.height);

        gtk_adjustment_configure (priv->hadj,
                                  priv->xofs,
                                  0, width,
                                  SCROLL_STEP_SIZE,
                                  allocation.width / 2,
                                  allocation.width);
        gtk_adjustment_configure (priv->vadj,
                                  priv->yofs,
                                  0, height,
                                  SCROLL_STEP_SIZE,
                                  allocation.height / 2,
                                  allocation.height);

	g_object_thaw_notify (G_OBJECT (priv->hadj));
	g_object_thaw_notify (G_OBJECT (priv->vadj));
}
