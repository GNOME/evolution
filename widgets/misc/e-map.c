/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* Map widget.
 *
 * Copyright (C) 2000-2001 Ximian, Inc.
 *
 * Authors: Hans Petter Jansson <hpj@ximian.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of version 2 of the GNU General Public
 * License as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#include <config.h>
#include <math.h>
#include <stdlib.h>
#include <gdk/gdkkeysyms.h>
#include <gtk/gtksignal.h>
#include <gdk-pixbuf/gdk-pixbuf.h>
#include <libart_lgpl/art_filterlevel.h>
#include <libgnome/gnome-i18n.h>

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

typedef struct
{
	/* Pointer to map image */
	GdkPixbuf *map_pixbuf, *map_render_pixbuf;

	/* Settings */
	gboolean frozen, smooth_zoom;

	/* Adjustments for scrolling */
	GtkAdjustment *hadj;
	GtkAdjustment *vadj;

	/* Current scrolling offsets */
	int xofs, yofs;

	/* Realtime zoom data */
	EMapZoomState zoom_state;
	double zoom_target_long, zoom_target_lat;

	/* Dots */
	GPtrArray *points;
}
EMapPrivate;


/* Internal prototypes */

static void e_map_class_init (EMapClass *class);
static void e_map_init (EMap *view);
static void e_map_finalize (GObject *object);
static void e_map_destroy (GtkObject *object);
static void e_map_unmap (GtkWidget *widget);
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

static void update_render_pixbuf (EMap *map, ArtFilterLevel interp, gboolean render_overlays);
static void set_scroll_area (EMap *view);
static void request_paint_area (EMap *view, GdkRectangle *area);
static void center_at (EMap *map, int x, int y, gboolean scroll);
static void smooth_center_at (EMap *map, int x, int y);
static void scroll_to (EMap *view, int x, int y);
static void zoom_do (EMap *map);
static gint load_map_background (EMap *view, gchar *name);
static void adjustment_changed_cb (GtkAdjustment *adj, gpointer data);
static void update_and_paint (EMap *map);
static void update_render_point (EMap *map, EMapPoint *point);
static void repaint_point (EMap *map, EMapPoint *point);

static GtkWidgetClass *parent_class;


/* ----------------- *
 * Widget management *
 * ----------------- */


/**
 * e_map_get_type:
 * @void: 
 * 
 * Registers the #EMap class if necessary, and returns the type ID
 * associated to it.
 * 
 * Return value: The type ID of the #EMap class.
 **/

GtkType
e_map_get_type (void)
{
	static GtkType e_map_type = 0;

	if (!e_map_type)
	{
		static const GtkTypeInfo e_map_info =
		{
			"EMap",
			sizeof (EMap),
			sizeof (EMapClass),
			(GtkClassInitFunc) e_map_class_init,
			(GtkObjectInitFunc) e_map_init,
			NULL,	/* reserved_1 */
			NULL,	/* reserved_2 */
			(GtkClassInitFunc) NULL
		};

		e_map_type = gtk_type_unique (GTK_TYPE_WIDGET, &e_map_info);
	}

	return e_map_type;
}

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

	parent_class = g_type_class_ref(GTK_TYPE_WIDGET);

	gobject_class->finalize = e_map_finalize;

	object_class->destroy = e_map_destroy;

	class->set_scroll_adjustments = e_map_set_scroll_adjustments;
	widget_class->set_scroll_adjustments_signal = gtk_signal_new ("set_scroll_adjustments",
								      GTK_RUN_LAST,
								      GTK_CLASS_TYPE (object_class),
								      G_STRUCT_OFFSET (EMapClass, set_scroll_adjustments),
								      gtk_marshal_NONE__POINTER_POINTER,
								      GTK_TYPE_NONE, 2,
								      GTK_TYPE_ADJUSTMENT,
								      GTK_TYPE_ADJUSTMENT);

	widget_class->unmap = e_map_unmap;
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

	priv = g_new0 (EMapPrivate, 1);
	view->priv = priv;

	load_map_background (view, MAP_DIR"/world_map-960.png");
	priv->frozen = FALSE;
	priv->smooth_zoom = TRUE;
	priv->zoom_state = E_MAP_ZOOMED_OUT;
        priv->points = g_ptr_array_new ();

	GTK_WIDGET_SET_FLAGS (view, GTK_CAN_FOCUS);
	GTK_WIDGET_UNSET_FLAGS (view, GTK_NO_WINDOW);
}


/* Destroy handler for the map view */

static void
e_map_destroy (GtkObject *object)
{
	EMap *view;
	EMapPrivate *priv;

	g_return_if_fail (object != NULL);
	g_return_if_fail (IS_E_MAP (object));

	view = E_MAP (object);
	priv = view->priv;

	g_signal_handlers_disconnect_by_func (priv->hadj, adjustment_changed_cb, view);
	g_signal_handlers_disconnect_by_func (priv->vadj, adjustment_changed_cb, view);

	if (GTK_OBJECT_CLASS (parent_class)->destroy)
		(*GTK_OBJECT_CLASS (parent_class)->destroy) (object);
}


/* Finalize handler for the map view */

static void
e_map_finalize (GObject *object)
{
	EMap *view;
	EMapPrivate *priv;

	g_return_if_fail (object != NULL);
	g_return_if_fail (IS_E_MAP (object));

	view = E_MAP (object);
	priv = view->priv;

	g_object_unref((priv->hadj));
	priv->hadj = NULL;

	g_object_unref((priv->vadj));
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

	if (G_OBJECT_CLASS (parent_class)->finalize)
		(*G_OBJECT_CLASS (parent_class)->finalize) (object);
}


/* Unmap handler for the map view */

static void
e_map_unmap (GtkWidget *widget)
{
	g_return_if_fail (widget != NULL);
	g_return_if_fail (IS_E_MAP (widget));

	if (GTK_WIDGET_CLASS (parent_class)->unmap)
		(*GTK_WIDGET_CLASS (parent_class)->unmap) (widget);
}


/* Realize handler for the map view */

static void
e_map_realize (GtkWidget *widget)
{
	GdkWindowAttr attr;
	int attr_mask;

	g_return_if_fail (widget != NULL);
	g_return_if_fail (IS_E_MAP (widget));

	GTK_WIDGET_SET_FLAGS (widget, GTK_REALIZED);

	attr.window_type = GDK_WINDOW_CHILD;
	attr.x = widget->allocation.x;
	attr.y = widget->allocation.y;
	attr.width = widget->allocation.width;
	attr.height = widget->allocation.height;
	attr.wclass = GDK_INPUT_OUTPUT;
	attr.visual = gdk_rgb_get_visual ();
	attr.colormap = gdk_rgb_get_cmap ();
	attr.event_mask = gtk_widget_get_events (widget) |
	  GDK_EXPOSURE_MASK | GDK_BUTTON_PRESS_MASK | GDK_KEY_PRESS_MASK |
	  GDK_POINTER_MOTION_MASK;

	attr_mask = GDK_WA_X | GDK_WA_Y | GDK_WA_VISUAL | GDK_WA_COLORMAP;

	widget->window = gdk_window_new (gtk_widget_get_parent_window (widget), &attr, attr_mask);
	gdk_window_set_user_data (widget->window, widget);

	widget->style = gtk_style_attach (widget->style, widget->window);

	gdk_window_set_back_pixmap (widget->window, NULL, FALSE);
	update_render_pixbuf (E_MAP (widget), GDK_INTERP_BILINEAR, TRUE);
}


/* Unrealize handler for the map view */

static void
e_map_unrealize (GtkWidget *widget)
{
	g_return_if_fail (widget != NULL);
	g_return_if_fail (IS_E_MAP (widget));

	if (GTK_WIDGET_CLASS (parent_class)->unrealize)
	        (*GTK_WIDGET_CLASS (parent_class)->unrealize) (widget);
}


/* Size_request handler for the map view */

static void
e_map_size_request (GtkWidget *widget, GtkRequisition *requisition)
{
	EMap *view;
	EMapPrivate *priv;

	g_return_if_fail (widget != NULL);
	g_return_if_fail (IS_E_MAP (widget));
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
	EMapPrivate *priv;
	int xofs, yofs;
	GdkRectangle area;

	g_return_if_fail (widget != NULL);
	g_return_if_fail (IS_E_MAP (widget));
	g_return_if_fail (allocation != NULL);

	view = E_MAP (widget);
	priv = view->priv;

	xofs = priv->xofs;
	yofs = priv->yofs;

	/* Resize the window */

	widget->allocation = *allocation;

	if (GTK_WIDGET_REALIZED (widget))
	{
		gdk_window_move_resize (widget->window, allocation->x, allocation->y, allocation->width, allocation->height);

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
	EMap *view;
	EMapPrivate *priv;

	view = E_MAP (widget);
	priv = view->priv;

	if (!GTK_WIDGET_HAS_FOCUS (widget)) gtk_widget_grab_focus (widget);
	return TRUE;
}


/* Button release handler for the map view */

static gint
e_map_button_release (GtkWidget *widget, GdkEventButton *event)
{
	EMap *view;
	EMapPrivate *priv;

	view = E_MAP (widget);
	priv = view->priv;

	if (event->button != 1) return FALSE;

	gdk_pointer_ungrab (event->time);
	return TRUE;
}


/* Motion handler for the map view */

static gint
e_map_motion (GtkWidget *widget, GdkEventMotion *event)
{
	EMap *view;
	EMapPrivate *priv;

	view = E_MAP (widget);
	priv = view->priv;

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
	g_return_val_if_fail (IS_E_MAP (widget), FALSE);
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
	g_return_if_fail (IS_E_MAP (widget));

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
		g_object_ref (priv->hadj);
		gtk_object_sink (GTK_OBJECT (priv->hadj));

		g_signal_connect (priv->hadj, "value_changed",
				  G_CALLBACK (adjustment_changed_cb), view);

		need_adjust = TRUE;
	}

	if (priv->vadj != vadj)
	{
		priv->vadj = vadj;
		g_object_ref (priv->vadj);
		gtk_object_sink (GTK_OBJECT (priv->vadj));

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
	int xofs, yofs;

	view = E_MAP (widget);
	priv = view->priv;

	do_scroll = FALSE;
	xofs = yofs = 0;

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
		int x, y;

		x = CLAMP (priv->xofs + xofs, 0, priv->hadj->upper - priv->hadj->page_size);
		y = CLAMP (priv->yofs + yofs, 0, priv->vadj->upper - priv->vadj->page_size);

		scroll_to (view, x, y);

		gtk_signal_handler_block_by_data (GTK_OBJECT (priv->hadj), view);
		gtk_signal_handler_block_by_data (GTK_OBJECT (priv->vadj), view);

		priv->hadj->value = x;
		priv->vadj->value = y;

		gtk_signal_emit_by_name (GTK_OBJECT (priv->hadj), "value_changed");
		gtk_signal_emit_by_name (GTK_OBJECT (priv->vadj), "value_changed");

		gtk_signal_handler_unblock_by_data (GTK_OBJECT (priv->hadj), view);
		gtk_signal_handler_unblock_by_data (GTK_OBJECT (priv->vadj), view);
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
e_map_new ()
{
	GtkWidget *widget;
	AtkObject *a11y;

	widget = GTK_WIDGET (gtk_type_new (TYPE_E_MAP));
	a11y = gtk_widget_get_accessible (widget);
	atk_object_set_name (a11y, _("World Map"));
	atk_object_set_role (a11y, ATK_ROLE_IMAGE);
	atk_object_set_description (a11y, _("Mouse-based interactive map widget for selecting timezone. Keyboard users should select the timezone from the below combo box instead."));
	return (E_MAP (widget));
}


/* --- Coordinate translation --- */


/* These functions translate coordinates between longitude/latitude and
 * the image x/y offsets, using the equidistant cylindrical projection.
 * 
 * Longitude E <-180, 180]
 * Latitude  E <-90, 90]   */

void
e_map_window_to_world (EMap *map, double win_x, double win_y, double *world_longitude, double *world_latitude)
{
	EMapPrivate *priv;
	int width, height;

	g_return_if_fail (map);

	priv = map->priv;
	g_return_if_fail (GTK_WIDGET_REALIZED (GTK_WIDGET (map)));

	width = gdk_pixbuf_get_width (priv->map_render_pixbuf);
	height = gdk_pixbuf_get_height (priv->map_render_pixbuf);

	*world_longitude = (win_x + priv->xofs - (double) width / 2.0) /
		((double) width / 2.0) * 180.0;
	*world_latitude = ((double) height / 2.0 - win_y - priv->yofs) /
		((double) height / 2.0) * 90.0;
}


void
e_map_world_to_window (EMap *map, double world_longitude, double world_latitude, double *win_x, double *win_y)
{
	EMapPrivate *priv;
	int width, height;

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


double
e_map_get_magnification (EMap *map)
{
	EMapPrivate *priv;
	
	priv = map->priv;
	if (priv->zoom_state == E_MAP_ZOOMED_IN) return 2.0;
	else return 1.0;
}


void
e_map_zoom_to_location (EMap *map, double longitude, double latitude)
{
	EMapPrivate *priv;
	int width, height;

	g_return_if_fail (map);
	g_return_if_fail (GTK_WIDGET_REALIZED (GTK_WIDGET (map)));

	priv = map->priv;

	if (priv->zoom_state == E_MAP_ZOOMED_IN) e_map_zoom_out (map);
	else if (priv->zoom_state != E_MAP_ZOOMED_OUT) return;

	width = gdk_pixbuf_get_width (priv->map_render_pixbuf);
	height = gdk_pixbuf_get_height (priv->map_render_pixbuf);

	priv->zoom_state = E_MAP_ZOOMING_IN;
	priv->zoom_target_long = longitude;
	priv->zoom_target_lat = latitude;

	zoom_do (map);
}


void
e_map_zoom_out (EMap *map)
{
	EMapPrivate *priv;
	int width, height;

	g_return_if_fail (map);
	g_return_if_fail (GTK_WIDGET_REALIZED (GTK_WIDGET (map)));

	priv = map->priv;

	if (priv->zoom_state != E_MAP_ZOOMED_IN) return;

	width = gdk_pixbuf_get_width (priv->map_render_pixbuf);
	height = gdk_pixbuf_get_height (priv->map_render_pixbuf);

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
e_map_add_point (EMap *map, gchar *name, double longitude, double latitude, guint32 color_rgba)
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
e_map_point_get_location (EMapPoint *point, double *longitude, double *latitude)
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
	double x, y;

	priv = map->priv;
	if (!priv->map_render_pixbuf) return FALSE;

	e_map_world_to_window (map, point->longitude, point->latitude, &x, &y);
	
	if (x >= 0 && x < GTK_WIDGET (map)->allocation.width &&
	    y >= 0 && y < GTK_WIDGET (map)->allocation.height)
	        return TRUE;
	
	return FALSE;
}


EMapPoint *
e_map_get_closest_point (EMap *map, double longitude, double latitude, gboolean in_view)
{
	EMapPrivate *priv;
	EMapPoint *point_chosen = NULL, *point;
	double min_dist = 0.0, dist;
	double dx, dy;
	int i;

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

	area.x = 0;
	area.y = 0;
	area.width = GTK_WIDGET (map)->allocation.width;
	area.height = GTK_WIDGET (map)->allocation.height;
	
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
update_render_pixbuf (EMap *map, ArtFilterLevel interp, gboolean render_overlays)
{
	EMapPrivate *priv;
	EMapPoint *point;
	int width, height, orig_width, orig_height;
	double zoom;
	int i;

	if (!GTK_WIDGET_REALIZED (GTK_WIDGET (map))) return;

	/* Set up value shortcuts */

	priv = map->priv;
	width = GTK_WIDGET (map)->allocation.width;
	height = GTK_WIDGET (map)->allocation.height;
	orig_width = gdk_pixbuf_get_width (priv->map_pixbuf);
	orig_height = gdk_pixbuf_get_height (priv->map_pixbuf);

	/* Compute scaled width and height based on the extreme dimension */

	if ((double) width / orig_width > (double) height / orig_height)
	{
		zoom = (double) width / (double) orig_width;
	}
	else
	{
		zoom = (double) height / (double) orig_height;
	}

	if (priv->zoom_state == E_MAP_ZOOMED_IN) zoom *= 2.0;
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
				  width, height, 0, 0,	                            /* Offset (x, y) */
				  zoom, zoom,	                                    /* Scale (x, y) */
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
	int width, height;

	if (!GTK_WIDGET_DRAWABLE (GTK_WIDGET (view)) ||
	    !GTK_WIDGET_REALIZED (GTK_WIDGET (view))) return;

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
		guchar *pixels;
		int rowstride;

		rowstride = gdk_pixbuf_get_rowstride (priv->map_render_pixbuf);
		pixels = gdk_pixbuf_get_pixels (priv->map_render_pixbuf) + (area->y + priv->yofs) * rowstride + 3 * (area->x + priv->xofs);
		gdk_draw_rgb_image_dithalign (GTK_WIDGET (view)->window, GTK_WIDGET (view)->style->black_gc, area->x, area->y, width, height, GDK_RGB_DITHER_NORMAL, pixels, rowstride, 0, 0);
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
	int width, height;
	double px, py;

	priv = map->priv;
	pb = priv->map_render_pixbuf;
	if (!pb) return;

	width  = gdk_pixbuf_get_width (pb);
	height = gdk_pixbuf_get_height (pb);

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
	EMapPrivate *priv;
	GdkRectangle area;
	double px, py;

	if (!e_map_point_is_in_view (map, point)) return; 
	priv = map->priv;

	e_map_world_to_window (map, point->longitude, point->latitude, &px, &py);

	area.x = (int) px - 2;
	area.y = (int) py - 2;
	area.width = 5;
	area.height = 5;
	request_paint_area (map, &area);
}


static void
center_at (EMap *map, int x, int y, gboolean scroll)
{
	EMapPrivate *priv;
	int pb_width, pb_height,
	    view_width, view_height;

	priv = map->priv;

	pb_width = E_MAP_GET_WIDTH (map);
	pb_height = E_MAP_GET_HEIGHT (map);

	view_width = GTK_WIDGET (map)->allocation.width;
	view_height = GTK_WIDGET (map)->allocation.height;

	x = CLAMP (x - (view_width / 2), 0, pb_width - view_width);
	y = CLAMP (y - (view_height / 2), 0, pb_height - view_height);

	if (scroll) scroll_to (map, x, y);
	else
	{
		priv->xofs = x;
		priv->yofs = y;
	}
}


static void
smooth_center_at (EMap *map, int x, int y)
{
	EMapPrivate *priv;
	int pb_width, pb_height,
	    view_width, view_height;
	int dx, dy;

	priv = map->priv;

	pb_width = E_MAP_GET_WIDTH (map);
	pb_height = E_MAP_GET_HEIGHT (map);

	view_width = GTK_WIDGET (map)->allocation.width;
	view_height = GTK_WIDGET (map)->allocation.height;

	x = CLAMP (x - (view_width / 2), 0, pb_width - view_width);
	y = CLAMP (y - (view_height / 2), 0, pb_height - view_height);

	for (;;)
	{
		if (priv->xofs == x && priv->yofs == y) break;

		dx = (x < priv->xofs) ? -1 : (x > priv->xofs) ? 1 : 0;
		dy = (y < priv->yofs) ? -1 : (y > priv->yofs) ? 1 : 0;
    
		scroll_to (map, priv->xofs + dx, priv->yofs + dy);
	}
}


/* Scrolls the view to the specified offsets.  Does not perform range checking!  */

static void
scroll_to (EMap *view, int x, int y)
{
	EMapPrivate *priv;
	int xofs, yofs;
	GdkWindow *window;
	GdkGC *gc;
	int width, height;
	int src_x, src_y;
	int dest_x, dest_y;
	GdkEvent *event;

	priv = view->priv;

	/* Compute offsets and check bounds */

	xofs = x - priv->xofs;
	yofs = y - priv->yofs;

	if (xofs == 0 && yofs == 0) return;

	priv->xofs = x;
	priv->yofs = y;

	if (!GTK_WIDGET_DRAWABLE (view)) return;

	width = GTK_WIDGET (view)->allocation.width;
	height = GTK_WIDGET (view)->allocation.height;

	if (abs (xofs) >= width || abs (yofs) >= height)
	{
		GdkRectangle area;

		area.x = 0;
		area.y = 0;
		area.width = width;
		area.height = height;

		request_paint_area (view, &area);
		return;
	}

	window = GTK_WIDGET (view)->window;

	/* Copy the window area */

	src_x = xofs < 0 ? 0 : xofs;
	src_y = yofs < 0 ? 0 : yofs;
	dest_x = xofs < 0 ? -xofs : 0;
	dest_y = yofs < 0 ? -yofs : 0;

	gc = gdk_gc_new (window);
	gdk_gc_set_exposures (gc, TRUE);

	gdk_window_copy_area (window, gc, dest_x, dest_y, window, src_x, src_y, width - abs (xofs), height - abs (yofs));

	gdk_gc_destroy (gc);

	/* Add the scrolled-in region */

	if (xofs)
	{
		GdkRectangle r;

		r.x = xofs < 0 ? 0 : width - xofs;
		r.y = 0;
		r.width = abs (xofs);
		r.height = height;

		request_paint_area (view, &r);
	}

	if (yofs)
	{
		GdkRectangle r;

		r.x = 0;
		r.y = yofs < 0 ? 0 : height - yofs;
		r.width = width;
		r.height = abs (yofs);

		request_paint_area (view, &r);
	}

	/* Process graphics exposures */

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
}


static int divide_seq[] =
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
	int line;


	/* Set up the GC we'll be using */

	gc = gdk_gc_new (window);
	gdk_gc_set_exposures (gc, FALSE);

	/* Get area constraints */

	gdk_window_get_size (window, &area_width, &area_height);

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
		axis_factor = (double) area_height / (double) area_width;
		zoom_chunk = MAX (1, area_width / 250);
		i = (area_width * (zoom_factor - 1.0)) / zoom_chunk;
	}
	else
	{
		strong_axis = AXIS_Y;
		axis_factor = (double) area_width / (double) area_height;
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
					gdk_window_copy_area (window, gc, line + j + 1, 0, window, line, 0, 1, area_height);

				gdk_window_copy_area (window, gc, 0, 0, window, zoom_chunk, 0, line, area_height);
				if (line > target_x) target_x -= zoom_chunk;
			}
			else
			{
				/* Push right */

				for (j = 0; j < zoom_chunk - 1; j++)
					gdk_window_copy_area (window, gc, line + j - (zoom_chunk - 1), 0, window, line - zoom_chunk, 0, 1, area_height);

				gdk_window_copy_area (window, gc, line, 0, window, line - zoom_chunk, 0, area_width - line, area_height);
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
					gdk_window_copy_area (window, gc, 0, line + j + 1, window, 0, line, area_width, 1);

				gdk_window_copy_area (window, gc, 0, 0, window, 0, zoom_chunk, area_width, line);
				if (line > target_y) target_y -= zoom_chunk;
			}
			else
			{
				/* Push down */

				for (j = 0; j < zoom_chunk - 1; j++)
					gdk_window_copy_area (window, gc, 0, line + j - (zoom_chunk - 1), window, 0, line - zoom_chunk, area_width, 1);

				gdk_window_copy_area (window, gc, 0, line, window, 0, line - zoom_chunk, area_width, area_height - line);
				if (line < target_y) target_y += zoom_chunk;
			}
		}

		divide_width_index++;
		divide_height_index++;
		if (axis_counter >= 1.0) axis_counter -= 1.0;
		axis_counter += axis_factor;
	}

	/* Free our GC */

	gdk_gc_destroy (gc);
}


static void
zoom_in_smooth (EMap *map)
{
	GdkRectangle area;
	EMapPrivate *priv;
	GdkWindow *window;
	int width, height;
	int win_width, win_height;
	int target_width, target_height;
	double x, y;

	g_return_if_fail (map);
	g_return_if_fail (GTK_WIDGET_REALIZED (GTK_WIDGET (map)));

	area.x = 0;
	area.y = 0;
	area.width = GTK_WIDGET (map)->allocation.width;
	area.height = GTK_WIDGET (map)->allocation.height;

	priv = map->priv;
	window = GTK_WIDGET (map)->window;
	width = gdk_pixbuf_get_width (priv->map_render_pixbuf);
	height = gdk_pixbuf_get_height (priv->map_render_pixbuf);
	win_width = GTK_WIDGET (map)->allocation.width;
	win_height = GTK_WIDGET (map)->allocation.height;
	target_width = win_width / 4;
	target_height = win_height / 4;

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
	GdkRectangle area;
	EMapPrivate *priv;
	double x, y;

	priv = map->priv;

	area.x = 0;
	area.y = 0;
	area.width = GTK_WIDGET (map)->allocation.width;
	area.height = GTK_WIDGET (map)->allocation.height;

	priv->zoom_state = E_MAP_ZOOMED_IN;

	update_render_pixbuf (map, GDK_INTERP_BILINEAR, TRUE);

	e_map_world_to_window (map, priv->zoom_target_long, priv->zoom_target_lat, &x, &y);
	priv->xofs = CLAMP (priv->xofs + x - area.width / 2.0, 0, E_MAP_GET_WIDTH (map) - area.width);
	priv->yofs = CLAMP (priv->yofs + y - area.height / 2.0, 0, E_MAP_GET_HEIGHT (map) - area.height);

	request_paint_area (map, &area);
}


static void
zoom_out (EMap *map)
{
	GdkRectangle area;
	EMapPrivate *priv;
	double longitude, latitude;
	double x, y;

	priv = map->priv;

	area.x = 0;
	area.y = 0;
	area.width = GTK_WIDGET (map)->allocation.width;
	area.height = GTK_WIDGET (map)->allocation.height;

	/* Must be done before update_render_pixbuf() */

	e_map_window_to_world (map, area.width / 2, area.height / 2,
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

	gtk_signal_handler_block_by_data (GTK_OBJECT (priv->hadj), map);
	gtk_signal_handler_block_by_data (GTK_OBJECT (priv->vadj), map);

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
  
	gtk_signal_handler_unblock_by_data (GTK_OBJECT (priv->hadj), map);
	gtk_signal_handler_unblock_by_data (GTK_OBJECT (priv->vadj), map);

	set_scroll_area(map);
}


/* Callback used when an adjustment is changed */

static void
adjustment_changed_cb (GtkAdjustment *adj, gpointer data)
{
	EMap *view;
	EMapPrivate *priv;

	view = E_MAP (data);
	priv = view->priv;

	scroll_to (view, priv->hadj->value, priv->vadj->value);
}


static void
set_scroll_area (EMap *view)
{
	EMapPrivate *priv;

	priv = view->priv;

	if (!GTK_WIDGET_REALIZED (GTK_WIDGET (view))) return;
	if (!priv->hadj || !priv->vadj) return;

	/* Set scroll increments */

	priv->hadj->page_size = GTK_WIDGET (view)->allocation.width;
	priv->hadj->page_increment = GTK_WIDGET (view)->allocation.width / 2;
	priv->hadj->step_increment = SCROLL_STEP_SIZE;

	priv->vadj->page_size = GTK_WIDGET (view)->allocation.height;
	priv->vadj->page_increment = GTK_WIDGET (view)->allocation.height / 2;
	priv->vadj->step_increment = SCROLL_STEP_SIZE;

	/* Set scroll bounds and new offsets */

	priv->hadj->lower = 0;
	if (priv->map_render_pixbuf)
		priv->hadj->upper = gdk_pixbuf_get_width (priv->map_render_pixbuf);

	priv->vadj->lower = 0;
	if (priv->map_render_pixbuf)
		priv->vadj->upper = gdk_pixbuf_get_height (priv->map_render_pixbuf);

	gtk_signal_emit_by_name (GTK_OBJECT (priv->hadj), "changed");
	gtk_signal_emit_by_name (GTK_OBJECT (priv->vadj), "changed");

	priv->xofs = CLAMP (priv->xofs, 0, priv->hadj->upper - priv->hadj->page_size);
	priv->yofs = CLAMP (priv->yofs, 0, priv->vadj->upper - priv->vadj->page_size);

	if (priv->hadj->value != priv->xofs)
	{
		priv->hadj->value = priv->xofs;

		gtk_signal_handler_block_by_data (GTK_OBJECT (priv->hadj), view);
		gtk_signal_emit_by_name (GTK_OBJECT (priv->hadj), "value_changed");
		gtk_signal_handler_unblock_by_data (GTK_OBJECT (priv->hadj), view);
	}

	if (priv->vadj->value != priv->yofs)
	{
		priv->vadj->value = priv->yofs;

		gtk_signal_handler_block_by_data (GTK_OBJECT (priv->vadj), view);
		gtk_signal_emit_by_name (GTK_OBJECT (priv->vadj), "value_changed");
		gtk_signal_handler_unblock_by_data (GTK_OBJECT (priv->vadj), view);
	}
}
