/*
 * Map widget.
 *
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
 *		Hans Petter Jansson <hpj@ximian.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#include "evolution-config.h"

#include <math.h>
#include <stdlib.h>

#include <glib/gi18n.h>
#include <gdk/gdkkeysyms.h>

#include <libedataserver/libedataserver.h>

#include "e-util-private.h"

#include "e-map.h"

#define E_MAP_TWEEN_TIMEOUT_MSECS 25
#define E_MAP_TWEEN_DURATION_MSECS 150

/* Scroll step increment */

#define SCROLL_STEP_SIZE 32

/* */

#define E_MAP_GET_WIDTH(map) gtk_adjustment_get_upper((map)->priv->hadjustment)
#define E_MAP_GET_HEIGHT(map) gtk_adjustment_get_upper((map)->priv->vadjustment)

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
	gdouble longitude_offset;
	gdouble latitude_offset;
	gdouble zoom_factor;
};

/* Private part of the EMap structure */

struct _EMapPrivate {
	/* Pointer to map image */
	GdkPixbuf *map_pixbuf;
	cairo_surface_t *map_render_surface;

	/* Settings */
	gboolean frozen, smooth_zoom;

	/* Adjustments for scrolling */
	GtkAdjustment *hadjustment;
	GtkAdjustment *vadjustment;

	/* GtkScrollablePolicy needs to be checked when
	 * driving the scrollable adjustment values */
	guint hscroll_policy : 1;
	guint vscroll_policy : 1;

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

/* Properties */

enum {
	PROP_0,

	/* For scrollable interface */
	PROP_HADJUSTMENT,
	PROP_VADJUSTMENT,
	PROP_HSCROLL_POLICY,
	PROP_VSCROLL_POLICY
};

G_DEFINE_TYPE_WITH_CODE (EMap, e_map, GTK_TYPE_WIDGET,
	G_ADD_PRIVATE (EMap)
	G_IMPLEMENT_INTERFACE (GTK_TYPE_SCROLLABLE, NULL))

/* Internal prototypes */

static void update_render_surface (EMap *map, gboolean render_overlays);
static void set_scroll_area (EMap *map, gint width, gint height);
static void center_at (EMap *map, gdouble longitude, gdouble latitude);
static void scroll_to (EMap *map, gint x, gint y);
static gint load_map_background (EMap *map, gchar *name);
static void update_and_paint (EMap *map);
static void update_render_point (EMap *map, EMapPoint *point);
static void repaint_point (EMap *map, EMapPoint *point);

/* ------ *
 * Tweens *
 * ------ */

static gboolean
e_map_is_tweening (EMap *map)
{
	return map->priv->timer != NULL;
}

static void
e_map_stop_tweening (EMap *map)
{
	g_return_if_fail (map->priv->tweens == NULL);

	if (!e_map_is_tweening (map))
		return;

	g_timer_destroy (map->priv->timer);
	map->priv->timer = NULL;
	g_source_remove (map->priv->tween_id);
	map->priv->tween_id = 0;
}

static void
e_map_tween_destroy (EMap *map,
                     EMapTween *tween)
{
	map->priv->tweens = g_slist_remove (map->priv->tweens, tween);
	g_slice_free (EMapTween, tween);

	if (map->priv->tweens == NULL)
		e_map_stop_tweening (map);
}

static gboolean
e_map_do_tween_cb (gpointer data)
{
	EMap *map = data;
	GSList *walk;

	map->priv->timer_current_ms =
		g_timer_elapsed (map->priv->timer, NULL) * 1000;
	gtk_widget_queue_draw (GTK_WIDGET (map));

	/* Can't use for loop here, because we need to advance 
	 * the list before deleting.
	 */
	walk = map->priv->tweens;
	while (walk)
	{
		EMapTween *tween = walk->data;

		walk = walk->next;

		if (tween->end_time <= map->priv->timer_current_ms)
			e_map_tween_destroy (map, tween);
	}

	return TRUE;
}

static void
e_map_start_tweening (EMap *map)
{
	if (e_map_is_tweening (map))
		return;

	map->priv->timer = g_timer_new ();
	map->priv->timer_current_ms = 0;
	map->priv->tween_id = e_named_timeout_add (
		E_MAP_TWEEN_TIMEOUT_MSECS, e_map_do_tween_cb, map);
	g_timer_start (map->priv->timer);
}

static void
e_map_tween_new (EMap *map,
                 guint msecs,
                 gdouble longitude_offset,
                 gdouble latitude_offset,
                 gdouble zoom_factor)
{
	EMapTween *tween;

	if (!map->priv->smooth_zoom)
		return;

	e_map_start_tweening (map);

	tween = g_slice_new (EMapTween);

	tween->start_time = map->priv->timer_current_ms;
	tween->end_time = tween->start_time + msecs;
	tween->longitude_offset = longitude_offset;
	tween->latitude_offset = latitude_offset;
	tween->zoom_factor = zoom_factor;

	map->priv->tweens = g_slist_prepend (map->priv->tweens, tween);

	gtk_widget_queue_draw (GTK_WIDGET (map));
}

static void
e_map_get_current_location (EMap *map,
                            gdouble *longitude,
                            gdouble *latitude)
{
	GtkAllocation allocation;

	gtk_widget_get_allocation (GTK_WIDGET (map), &allocation);

	e_map_window_to_world (
		map, allocation.width / 2.0,
		allocation.height / 2.0,
		longitude, latitude);
}

static void
e_map_world_to_render_surface (EMap *map,
                               gdouble world_longitude,
                               gdouble world_latitude,
                               gdouble *win_x,
                               gdouble *win_y)
{
	gint width, height;

	width = E_MAP_GET_WIDTH (map);
	height = E_MAP_GET_HEIGHT (map);

	*win_x = (width / 2.0 + (width / 2.0) * world_longitude / 180.0);
	*win_y = (height / 2.0 - (height / 2.0) * world_latitude / 90.0);
}

static void
e_map_tween_new_from (EMap *map,
                      guint msecs,
                      gdouble longitude,
                      gdouble latitude,
                      gdouble zoom)
{
	gdouble current_longitude, current_latitude;

	e_map_get_current_location (
		map, &current_longitude, &current_latitude);

	e_map_tween_new (
		map, msecs,
		longitude - current_longitude,
		latitude - current_latitude,
		zoom / e_map_get_magnification (map));
}

static gdouble
e_map_get_tween_effect (EMap *map,
                        EMapTween *tween)
{
	gdouble elapsed;

	elapsed = (gdouble)
		(map->priv->timer_current_ms - tween->start_time) /
		tween->end_time;

	return MAX (0.0, 1.0 - elapsed);
}

static void
e_map_apply_tween (EMapTween *tween,
                   gdouble effect,
                   gdouble *longitude,
                   gdouble *latitude,
                   gdouble *zoom)
{
	*zoom *= pow (tween->zoom_factor, effect);
	*longitude += tween->longitude_offset * effect;
	*latitude += tween->latitude_offset * effect;
}

static void
e_map_tweens_compute_matrix (EMap *map,
                             cairo_matrix_t *matrix)
{
	GSList *walk;
	gdouble zoom, x, y, latitude, longitude, effect;
	GtkAllocation allocation;

	if (!e_map_is_tweening (map)) {
		cairo_matrix_init_translate (
			matrix, -map->priv->xofs, -map->priv->yofs);
		return;
	}

	e_map_get_current_location (map, &longitude, &latitude);
	zoom = 1.0;

	for (walk = map->priv->tweens; walk; walk = walk->next) {
		EMapTween *tween = walk->data;

		effect = e_map_get_tween_effect (map, tween);
		e_map_apply_tween (tween, effect, &longitude, &latitude, &zoom);
	}

	gtk_widget_get_allocation (GTK_WIDGET (map), &allocation);
	cairo_matrix_init_translate (
		matrix,
		allocation.width / 2.0,
		allocation.height / 2.0);

	e_map_world_to_render_surface (map, longitude, latitude, &x, &y);
	cairo_matrix_scale (matrix, zoom, zoom);
	cairo_matrix_translate (matrix, -x, -y);
}

/* GtkScrollable implementation */

static void
e_map_adjustment_changed (GtkAdjustment *adjustment,
                          EMap *map)
{
	EMapPrivate *priv = map->priv;

	if (gtk_widget_get_realized (GTK_WIDGET (map))) {
		gint hadj_value;
		gint vadj_value;

		hadj_value = gtk_adjustment_get_value (priv->hadjustment);
		vadj_value = gtk_adjustment_get_value (priv->vadjustment);

		scroll_to (map, hadj_value, vadj_value);
	}
}

static void
e_map_set_hadjustment_values (EMap *map)
{
	GtkAllocation  allocation;
	EMapPrivate *priv = map->priv;
	GtkAdjustment *adj = priv->hadjustment;
	gdouble old_value;
	gdouble new_value;
	gdouble new_upper;

	gtk_widget_get_allocation (GTK_WIDGET (map), &allocation);

	old_value = gtk_adjustment_get_value (adj);
	new_upper = MAX (allocation.width, gdk_pixbuf_get_width (priv->map_pixbuf));

	g_object_set (
		adj,
		"lower", 0.0,
		"upper", new_upper,
		"page-size", (gdouble) allocation.height,
		"step-increment", allocation.height * 0.1,
		"page-increment", allocation.height * 0.9,
		NULL);

	new_value = CLAMP (old_value, 0, new_upper - allocation.width);
	if (new_value != old_value)
		gtk_adjustment_set_value (adj, new_value);
}

static void
e_map_set_vadjustment_values (EMap *map)
{
	GtkAllocation  allocation;
	EMapPrivate *priv = map->priv;
	GtkAdjustment *adj = priv->vadjustment;
	gdouble old_value;
	gdouble new_value;
	gdouble new_upper;

	gtk_widget_get_allocation (GTK_WIDGET (map), &allocation);

	old_value = gtk_adjustment_get_value (adj);
	new_upper = MAX (allocation.height, gdk_pixbuf_get_height (priv->map_pixbuf));

	g_object_set (
		adj,
		"lower", 0.0,
		"upper", new_upper,
		"page-size", (gdouble) allocation.height,
		"step-increment", allocation.height * 0.1,
		"page-increment", allocation.height * 0.9,
		NULL);

	new_value = CLAMP (old_value, 0, new_upper - allocation.height);
	if (new_value != old_value)
		gtk_adjustment_set_value (adj, new_value);
}

static void
e_map_set_hadjustment (EMap *map,
                       GtkAdjustment *adjustment)
{
	EMapPrivate *priv = map->priv;

	if (adjustment && priv->hadjustment == adjustment)
		return;

	if (priv->hadjustment != NULL) {
		g_signal_handlers_disconnect_matched (
			priv->hadjustment, G_SIGNAL_MATCH_DATA,
			0, 0, NULL, NULL, map);
		g_object_unref (priv->hadjustment);
	}

	if (!adjustment)
		adjustment = gtk_adjustment_new (0.0, 0.0, 0.0, 0.0, 0.0, 0.0);

	g_signal_connect (
		adjustment, "value-changed",
		G_CALLBACK (e_map_adjustment_changed), map);
	priv->hadjustment = g_object_ref_sink (adjustment);
	e_map_set_hadjustment_values (map);

	g_object_notify (G_OBJECT (map), "hadjustment");
}

static void
e_map_set_vadjustment (EMap *map,
                       GtkAdjustment *adjustment)
{
	EMapPrivate *priv = map->priv;

	if (adjustment && priv->vadjustment == adjustment)
		return;

	if (priv->vadjustment != NULL) {
		g_signal_handlers_disconnect_matched (
			priv->vadjustment, G_SIGNAL_MATCH_DATA,
			0, 0, NULL, NULL, map);
		g_object_unref (priv->vadjustment);
	}

	if (!adjustment)
		adjustment = gtk_adjustment_new (0.0, 0.0, 0.0, 0.0, 0.0, 0.0);

	g_signal_connect (
		adjustment, "value-changed",
		G_CALLBACK (e_map_adjustment_changed), map);
	priv->vadjustment = g_object_ref_sink (adjustment);
	e_map_set_vadjustment_values (map);

	g_object_notify (G_OBJECT (map), "vadjustment");
}

/* ----------------- *
 * Widget management *
 * ----------------- */

static void
e_map_set_property (GObject *object,
                    guint property_id,
                    const GValue *value,
                    GParamSpec *pspec)
{
	EMap *map;

	map = E_MAP (object);

	switch (property_id) {
	case PROP_HADJUSTMENT:
		e_map_set_hadjustment (map, g_value_get_object (value));
		break;
	case PROP_VADJUSTMENT:
		e_map_set_vadjustment (map, g_value_get_object (value));
		break;
	case PROP_HSCROLL_POLICY:
		map->priv->hscroll_policy = g_value_get_enum (value);
		gtk_widget_queue_resize (GTK_WIDGET (map));
		break;
	case PROP_VSCROLL_POLICY:
		map->priv->vscroll_policy = g_value_get_enum (value);
		gtk_widget_queue_resize (GTK_WIDGET (map));
		break;

	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
		break;
	}
}

static void
e_map_get_property (GObject *object,
                    guint property_id,
                    GValue *value,
                    GParamSpec *pspec)
{
	EMap *map;

	map = E_MAP (object);

	switch (property_id) {
	case PROP_HADJUSTMENT:
		g_value_set_object (value, map->priv->hadjustment);
		break;
	case PROP_VADJUSTMENT:
		g_value_set_object (value, map->priv->vadjustment);
		break;
	case PROP_HSCROLL_POLICY:
		g_value_set_enum (value, map->priv->hscroll_policy);
		break;
	case PROP_VSCROLL_POLICY:
		g_value_set_enum (value, map->priv->vscroll_policy);
		break;

	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
		break;
	}
}

static void
e_map_finalize (GObject *object)
{
	EMap *map;

	map = E_MAP (object);

	while (map->priv->tweens)
		e_map_tween_destroy (map, map->priv->tweens->data);
	e_map_stop_tweening (map);

	g_clear_object (&map->priv->map_pixbuf);

	/* gone in unrealize */
	g_warn_if_fail (map->priv->map_render_surface == NULL);

	G_OBJECT_CLASS (e_map_parent_class)->finalize (object);
}

static void
e_map_realize (GtkWidget *widget)
{
	GtkAllocation allocation;
	GdkWindowAttr attr;
	GdkWindow *window;
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
	attr.visual = gtk_widget_get_visual (widget);
	attr.event_mask = gtk_widget_get_events (widget) |
	  GDK_EXPOSURE_MASK | GDK_BUTTON_PRESS_MASK | GDK_KEY_PRESS_MASK |
	  GDK_POINTER_MOTION_MASK;

	attr_mask = GDK_WA_X | GDK_WA_Y | GDK_WA_VISUAL;

	window = gdk_window_new (
		gtk_widget_get_parent_window (widget), &attr, attr_mask);
	gtk_widget_set_window (widget, window);
	gdk_window_set_user_data (window, widget);

	update_render_surface (E_MAP (widget), TRUE);
}

static void
e_map_unrealize (GtkWidget *widget)
{
	EMap *map = E_MAP (widget);

	cairo_surface_destroy (map->priv->map_render_surface);
	map->priv->map_render_surface = NULL;

	if (GTK_WIDGET_CLASS (e_map_parent_class)->unrealize)
		(*GTK_WIDGET_CLASS (e_map_parent_class)->unrealize) (widget);
}

static void
e_map_get_preferred_width (GtkWidget *widget,
                           gint *minimum,
                           gint *natural)
{
	EMap *map;

	g_return_if_fail (widget != NULL);
	g_return_if_fail (E_IS_MAP (widget));

	map = E_MAP (widget);

	/* TODO: Put real sizes here. */

	*minimum = *natural = gdk_pixbuf_get_width (map->priv->map_pixbuf);
}

static void
e_map_get_preferred_height (GtkWidget *widget,
                            gint *minimum,
                            gint *natural)
{
	EMap *view;
	EMapPrivate *priv;

	g_return_if_fail (widget != NULL);
	g_return_if_fail (E_IS_MAP (widget));

	view = E_MAP (widget);
	priv = view->priv;

	/* TODO: Put real sizes here. */

	*minimum = *natural = gdk_pixbuf_get_height (priv->map_pixbuf);
}

static void
e_map_size_allocate (GtkWidget *widget,
                     GtkAllocation *allocation)
{
	EMap *map;

	g_return_if_fail (widget != NULL);
	g_return_if_fail (E_IS_MAP (widget));
	g_return_if_fail (allocation != NULL);

	map = E_MAP (widget);

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

	update_render_surface (map, TRUE);
}

static gboolean
e_map_draw (GtkWidget *widget,
            cairo_t *cr)
{
	EMap *map;
	cairo_matrix_t matrix;

	if (!gtk_widget_is_drawable (widget))
		return FALSE;

	map = E_MAP (widget);

	cairo_save (cr);

	e_map_tweens_compute_matrix (map, &matrix);
	cairo_transform (cr, &matrix);

	cairo_set_source_surface (cr, map->priv->map_render_surface, 0, 0);
	cairo_paint (cr);

	cairo_restore (cr);

	return FALSE;
}

static gint
e_map_button_press (GtkWidget *widget,
                    GdkEventButton *event)
{
	if (!gtk_widget_has_focus (widget))
		gtk_widget_grab_focus (widget);

	return TRUE;
}

static gint
e_map_button_release (GtkWidget *widget,
                      GdkEventButton *event)
{
	if (event->button != 1)
		return FALSE;

	gdk_device_ungrab (event->device, event->time);
	return TRUE;
}

static gint
e_map_motion (GtkWidget *widget,
              GdkEventMotion *event)
{
	return FALSE;
}

static gint
e_map_key_press (GtkWidget *widget,
                 GdkEventKey *event)
{
	EMap *map;
	gboolean do_scroll;
	gint xofs, yofs;

	map = E_MAP (widget);

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

	if (do_scroll) {
		gint page_size;
		gint upper;
		gint x, y;

		page_size = gtk_adjustment_get_page_size (map->priv->hadjustment);
		upper = gtk_adjustment_get_upper (map->priv->hadjustment);
		x = CLAMP (map->priv->xofs + xofs, 0, upper - page_size);

		page_size = gtk_adjustment_get_page_size (map->priv->vadjustment);
		upper = gtk_adjustment_get_upper (map->priv->vadjustment);
		y = CLAMP (map->priv->yofs + yofs, 0, upper - page_size);

		scroll_to (map, x, y);

		gtk_adjustment_set_value (map->priv->hadjustment, x);
		gtk_adjustment_set_value (map->priv->vadjustment, y);
	}

	return TRUE;
}

static void
e_map_class_init (EMapClass *class)
{
	GObjectClass *object_class;
	GtkWidgetClass *widget_class;

	object_class = G_OBJECT_CLASS (class);
	object_class->set_property = e_map_set_property;
	object_class->get_property = e_map_get_property;
	object_class->finalize = e_map_finalize;

	/* Scrollable interface properties */
	g_object_class_override_property (
		object_class, PROP_HADJUSTMENT, "hadjustment");
	g_object_class_override_property (
		object_class, PROP_VADJUSTMENT, "vadjustment");
	g_object_class_override_property (
		object_class, PROP_HSCROLL_POLICY, "hscroll-policy");
	g_object_class_override_property (
		object_class, PROP_VSCROLL_POLICY, "vscroll-policy");

	widget_class = GTK_WIDGET_CLASS (class);
	widget_class->realize = e_map_realize;
	widget_class->unrealize = e_map_unrealize;
	widget_class->get_preferred_height = e_map_get_preferred_height;
	widget_class->get_preferred_width = e_map_get_preferred_width;
	widget_class->size_allocate = e_map_size_allocate;
	widget_class->draw = e_map_draw;
	widget_class->button_press_event = e_map_button_press;
	widget_class->button_release_event = e_map_button_release;
	widget_class->motion_notify_event = e_map_motion;
	widget_class->key_press_event = e_map_key_press;
}

static void
e_map_init (EMap *map)
{
	GtkWidget *widget;
	gchar *map_file_name;

	map_file_name = g_build_filename (
		EVOLUTION_IMAGESDIR, "world_map-960.png", NULL);

	widget = GTK_WIDGET (map);

	map->priv = e_map_get_instance_private (map);

	load_map_background (map, map_file_name);
	g_free (map_file_name);
	map->priv->frozen = FALSE;
	map->priv->smooth_zoom = TRUE;
	map->priv->zoom_state = E_MAP_ZOOMED_OUT;
	map->priv->points = g_ptr_array_new ();

	gtk_widget_set_can_focus (widget, TRUE);
	gtk_widget_set_has_window (widget, TRUE);
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
	atk_object_set_description (
		a11y, _("Mouse-based interactive map widget for selecting "
		"timezone. Keyboard users should instead select the timezone "
		"from the drop-down combination box below."));
	return (E_MAP (widget));
}

/* --- Coordinate translation --- */

/* These functions translate coordinates between longitude/latitude and
 * the image x/y offsets, using the equidistant cylindrical projection.
 *
 * Longitude E <-180, 180]
 * Latitude  E <-90, 90]   */

void
e_map_window_to_world (EMap *map,
                       gdouble win_x,
                       gdouble win_y,
                       gdouble *world_longitude,
                       gdouble *world_latitude)
{
	gint width, height;

	g_return_if_fail (map);

	g_return_if_fail (gtk_widget_get_realized (GTK_WIDGET (map)));

	width = E_MAP_GET_WIDTH (map);
	height = E_MAP_GET_HEIGHT (map);

	*world_longitude = (win_x + map->priv->xofs - (gdouble) width / 2.0) /
		((gdouble) width / 2.0) * 180.0;
	*world_latitude = ((gdouble) height / 2.0 - win_y - map->priv->yofs) /
		((gdouble) height / 2.0) * 90.0;
}

void
e_map_world_to_window (EMap *map,
                       gdouble world_longitude,
                       gdouble world_latitude,
                       gdouble *win_x,
                       gdouble *win_y)
{
	g_return_if_fail (E_IS_MAP (map));
	g_return_if_fail (gtk_widget_get_realized (GTK_WIDGET (map)));
	g_return_if_fail (world_longitude >= -180.0 && world_longitude <= 180.0);
	g_return_if_fail (world_latitude >= -90.0 && world_latitude <= 90.0);

	e_map_world_to_render_surface (
		map, world_longitude, world_latitude, win_x, win_y);

	*win_x -= map->priv->xofs;
	*win_y -= map->priv->yofs;
}

/* --- Zoom --- */

gdouble
e_map_get_magnification (EMap *map)
{
	if (map->priv->zoom_state == E_MAP_ZOOMED_IN) return 2.0;
	else return 1.0;
}

static void
e_map_set_zoom (EMap *map,
                EMapZoomState zoom)
{
	if (map->priv->zoom_state == zoom)
		return;

	map->priv->zoom_state = zoom;
	update_render_surface (map, TRUE);
	gtk_widget_queue_draw (GTK_WIDGET (map));
}

void
e_map_zoom_to_location (EMap *map,
                        gdouble longitude,
                        gdouble latitude)
{
	gdouble prevlong, prevlat;
	gdouble prevzoom;

	g_return_if_fail (map);
	g_return_if_fail (gtk_widget_get_realized (GTK_WIDGET (map)));

	e_map_get_current_location (map, &prevlong, &prevlat);
	prevzoom = e_map_get_magnification (map);

	e_map_set_zoom (map, E_MAP_ZOOMED_IN);
	center_at (map, longitude, latitude);

	e_map_tween_new_from (
		map, E_MAP_TWEEN_DURATION_MSECS,
		prevlong, prevlat, prevzoom);
}

void
e_map_zoom_out (EMap *map)
{
	gdouble longitude, latitude;
	gdouble prevzoom;

	g_return_if_fail (map);
	g_return_if_fail (gtk_widget_get_realized (GTK_WIDGET (map)));

	e_map_get_current_location (map, &longitude, &latitude);
	prevzoom = e_map_get_magnification (map);
	e_map_set_zoom (map, E_MAP_ZOOMED_OUT);
	center_at (map, longitude, latitude);

	e_map_tween_new_from (
		map, E_MAP_TWEEN_DURATION_MSECS,
		longitude, latitude, prevzoom);
}

void
e_map_set_smooth_zoom (EMap *map,
                       gboolean state)
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
e_map_add_point (EMap *map,
                 gchar *name,
                 gdouble longitude,
                 gdouble latitude,
                 guint32 color_rgba)
{
	EMapPoint *point;

	point = g_new0 (EMapPoint, 1);

	point->name = name;  /* Can be NULL */
	point->longitude = longitude;
	point->latitude = latitude;
	point->rgba = color_rgba;

	g_ptr_array_add (map->priv->points, (gpointer) point);

	if (!map->priv->frozen)
	{
		update_render_point (map, point);
		repaint_point (map, point);
	}

	return point;
}

void
e_map_remove_point (EMap *map,
                    EMapPoint *point)
{
	g_ptr_array_remove (map->priv->points, point);

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
e_map_point_get_location (EMapPoint *point,
                          gdouble *longitude,
                          gdouble *latitude)
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
e_map_point_set_color_rgba (EMap *map,
                            EMapPoint *point,
                            guint32 color_rgba)
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
e_map_point_set_data (EMapPoint *point,
                      gpointer data)
{
	point->user_data = data;
}

gpointer
e_map_point_get_data (EMapPoint *point)
{
	return point->user_data;
}

gboolean
e_map_point_is_in_view (EMap *map,
                        EMapPoint *point)
{
	GtkAllocation allocation;
	gdouble x, y;

	if (!map->priv->map_render_surface) return FALSE;

	e_map_world_to_window (map, point->longitude, point->latitude, &x, &y);
	gtk_widget_get_allocation (GTK_WIDGET (map), &allocation);

	if (x >= 0 && x < allocation.width &&
	    y >= 0 && y < allocation.height)
		return TRUE;

	return FALSE;
}

EMapPoint *
e_map_get_closest_point (EMap *map,
                         gdouble longitude,
                         gdouble latitude,
                         gboolean in_view)
{
	EMapPoint *point_chosen = NULL, *point;
	gdouble min_dist = 0.0, dist;
	gdouble dx, dy;
	gint i;

	for (i = 0; i < map->priv->points->len; i++)
	{
		point = g_ptr_array_index (map->priv->points, i);
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
load_map_background (EMap *map,
                     gchar *name)
{
	GdkPixbuf *pb0;

	pb0 = gdk_pixbuf_new_from_file (name, NULL);
	if (!pb0)
		return FALSE;

	if (map->priv->map_pixbuf) g_object_unref (map->priv->map_pixbuf);
	map->priv->map_pixbuf = pb0;
	update_render_surface (map, TRUE);

	return TRUE;
}

static void
update_render_surface (EMap *map,
                       gboolean render_overlays)
{
	EMapPoint *point;
	GtkAllocation allocation;
	gint width, height, orig_width, orig_height;
	gdouble zoom;
	gint i;

	if (!gtk_widget_get_realized (GTK_WIDGET (map)))
		return;

	gtk_widget_get_allocation (GTK_WIDGET (map), &allocation);

	/* Set up value shortcuts */

	width = allocation.width;
	height = allocation.height;
	orig_width = gdk_pixbuf_get_width (map->priv->map_pixbuf);
	orig_height = gdk_pixbuf_get_height (map->priv->map_pixbuf);

	/* Compute scaled width and height based on the extreme dimension */

	if ((gdouble) width / orig_width > (gdouble) height / orig_height)
		zoom = (gdouble) width / (gdouble) orig_width;
	else
		zoom = (gdouble) height / (gdouble) orig_height;

	if (map->priv->zoom_state == E_MAP_ZOOMED_IN)
		zoom *= 2.0;
	height = (orig_height * zoom) + 0.5;
	width = (orig_width * zoom) + 0.5;

	/* Reallocate the pixbuf */

	if (map->priv->map_render_surface)
		cairo_surface_destroy (map->priv->map_render_surface);
	map->priv->map_render_surface = gdk_window_create_similar_surface (
		gtk_widget_get_window (GTK_WIDGET (map)),
		CAIRO_CONTENT_COLOR, width, height);

	/* Scale the original map into the rendering pixbuf */

	if (width > 1 && height > 1) {
		cairo_t *cr = cairo_create (map->priv->map_render_surface);
		cairo_scale (
			cr,
			(gdouble) width / orig_width,
			(gdouble) height / orig_height);
		gdk_cairo_set_source_pixbuf (cr, map->priv->map_pixbuf, 0, 0);
		cairo_paint (cr);
		cairo_destroy (cr);
	}

	/* Compute image offsets with respect to window */

	set_scroll_area (map, width, height);

	if (render_overlays) {
		/* Add points */

		for (i = 0; i < map->priv->points->len; i++) {
			point = g_ptr_array_index (map->priv->points, i);
			update_render_point (map, point);
		}
	}
}

/* Redraw point in client surface */

static void
update_render_point (EMap *map,
                     EMapPoint *point)
{
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

	if (map->priv->map_render_surface == NULL)
		return;

	cr = cairo_create (map->priv->map_render_surface);
	cairo_set_operator (cr, CAIRO_OPERATOR_SOURCE);

	e_map_world_to_window (map, point->longitude, point->latitude, &px, &py);
	px = floor (px + map->priv->xofs);
	py = floor (py + map->priv->yofs);

	cairo_set_source_rgb (cr, 0, 0, 0);
	mask = cairo_image_surface_create_for_data (mask1, CAIRO_FORMAT_A8, 5, 5, 8);
	cairo_mask_surface (cr, mask, px - 2, py - 2);
	cairo_surface_destroy (mask);

	cairo_set_source_rgba (
		cr,
		((point->rgba >> 24) & 0xff) / 255.0,
		((point->rgba >> 16) & 0xff) / 255.0,
		((point->rgba >> 8) & 0xff) / 255.0,
		( point->rgba & 0xff) / 255.0);
	mask = cairo_image_surface_create_for_data (mask2, CAIRO_FORMAT_A8, 3, 3, 4);
	cairo_mask_surface (cr, mask, px - 1, py - 1);
	cairo_surface_destroy (mask);

	cairo_destroy (cr);
}

/* Repaint point on X server */

static void
repaint_point (EMap *map,
               EMapPoint *point)
{
	gdouble px, py;

	if (!gtk_widget_is_drawable (GTK_WIDGET (map)))
		return;

	e_map_world_to_window (map, point->longitude, point->latitude, &px, &py);

	gtk_widget_queue_draw_area (
		GTK_WIDGET (map),
		(gint) px - 2, (gint) py - 2,
		5, 5);
}

static void
center_at (EMap *map,
           gdouble longitude,
           gdouble latitude)
{
	GtkAllocation allocation;
	gint pb_width, pb_height;
	gdouble x, y;

	e_map_world_to_render_surface (map, longitude, latitude, &x, &y);

	pb_width = E_MAP_GET_WIDTH (map);
	pb_height = E_MAP_GET_HEIGHT (map);

	gtk_widget_get_allocation (GTK_WIDGET (map), &allocation);

	x = CLAMP (x - (allocation.width / 2), 0, pb_width - allocation.width);
	y = CLAMP (y - (allocation.height / 2), 0, pb_height - allocation.height);

	gtk_adjustment_set_value (map->priv->hadjustment, x);
	gtk_adjustment_set_value (map->priv->vadjustment, y);

	gtk_widget_queue_draw (GTK_WIDGET (map));
}

/* Scrolls the view to the specified offsets.  Does not perform range checking!  */

static void
scroll_to (EMap *map,
           gint x,
           gint y)
{
	gint xofs, yofs;

	/* Compute offsets and check bounds */

	xofs = x - map->priv->xofs;
	yofs = y - map->priv->yofs;

	if (xofs == 0 && yofs == 0)
		return;

	map->priv->xofs = x;
	map->priv->yofs = y;

	gtk_widget_queue_draw (GTK_WIDGET (map));
}

static void
set_scroll_area (EMap *view,
                 gint width,
                 gint height)
{
	EMapPrivate *priv;
	GtkAllocation allocation;

	priv = view->priv;

	if (!gtk_widget_get_realized (GTK_WIDGET (view)))
		return;

	if (!priv->hadjustment || !priv->vadjustment)
		return;

	g_object_freeze_notify (G_OBJECT (priv->hadjustment));
	g_object_freeze_notify (G_OBJECT (priv->vadjustment));

	gtk_widget_get_allocation (GTK_WIDGET (view), &allocation);

	priv->xofs = CLAMP (priv->xofs, 0, width - allocation.width);
	priv->yofs = CLAMP (priv->yofs, 0, height - allocation.height);

	gtk_adjustment_configure (
		priv->hadjustment,
		priv->xofs,
		0, width,
		SCROLL_STEP_SIZE,
		allocation.width / 2,
		allocation.width);
	gtk_adjustment_configure (
		priv->vadjustment,
		priv->yofs,
		0, height,
		SCROLL_STEP_SIZE,
		allocation.height / 2,
		allocation.height);

	g_object_thaw_notify (G_OBJECT (priv->hadjustment));
	g_object_thaw_notify (G_OBJECT (priv->vadjustment));
}
