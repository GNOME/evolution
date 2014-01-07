/*
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

#if !defined (__E_UTIL_H_INSIDE__) && !defined (LIBEUTIL_COMPILATION)
#error "Only <e-util/e-util.h> should be included directly."
#endif

#ifndef E_MAP_H
#define E_MAP_H

#include <gtk/gtk.h>

/* Standard GObject macros */
#define E_TYPE_MAP \
	(e_map_get_type ())
#define E_MAP(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), E_TYPE_MAP, EMap))
#define E_MAP_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((cls), E_TYPE_MAP, EMapClass))
#define E_IS_MAP(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), E_TYPE_MAP))
#define E_IS_MAP_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_TYPE \
	((cls), E_TYPE_MAP))
#define E_MAP_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS \
	((obj), E_TYPE_MAP, EMapClass))

G_BEGIN_DECLS

typedef struct _EMap EMap;
typedef struct _EMapClass EMapClass;
typedef struct _EMapPrivate EMapPrivate;
typedef struct _EMapPoint EMapPoint;

struct _EMap {
	GtkWidget widget;
	EMapPrivate *priv;
};

struct _EMapClass {
	GtkWidgetClass parent_class;

	/* Notification signals */
	void (*zoom_fit) (EMap * view);

	/* GTK+ scrolling interface */
	void (*set_scroll_adjustments) (GtkWidget * widget,
					GtkAdjustment * hadj,
					GtkAdjustment * vadj);
};

/* The definition of Dot */

struct _EMapPoint {
	gchar *name;  /* Can be NULL */
	gdouble longitude, latitude;
	guint32 rgba;
	gpointer user_data;
};

/* --- Widget --- */

GType e_map_get_type (void) G_GNUC_CONST;

EMap *e_map_new (void);

/* Stop doing redraws when map data changes (e.g. by modifying points) */
void e_map_freeze (EMap *map);

/* Do an immediate repaint, and start doing realtime repaints again */
void e_map_thaw (EMap *map);

/* --- Coordinate translation --- */

/* Translates window-relative coords to lat/long */
void e_map_window_to_world (EMap *map,
			    gdouble win_x, gdouble win_y,
			    gdouble *world_longitude, gdouble *world_latitude);

/* Translates lat/long to window-relative coordinates. Note that the
 * returned coordinates can be negative or greater than the current size
 * of the allocation area */
void e_map_world_to_window (EMap *map,
			    gdouble world_longitude, gdouble world_latitude,
			    gdouble *win_x, gdouble *win_y);

/* --- Zoom --- */

gdouble e_map_get_magnification (EMap *map);

/* Pass TRUE if we want the smooth zoom hack */
void e_map_set_smooth_zoom (EMap *map, gboolean state);

/* TRUE if smooth zoom hack will be employed */
gboolean e_map_get_smooth_zoom (EMap *map);

/* NB: Function definition will change shortly */
void e_map_zoom_to_location (EMap *map, gdouble longitude, gdouble latitude);

/* Zoom to mag factor 1.0 */
void e_map_zoom_out (EMap *map);

/* --- Points --- */

EMapPoint *e_map_add_point (EMap *map, gchar *name,
			    gdouble longitude, gdouble latitude,
			    guint32 color_rgba);

void e_map_remove_point (EMap *map, EMapPoint *point);

void e_map_point_get_location (EMapPoint *point,
			       gdouble *longitude, gdouble *latitude);

gchar *e_map_point_get_name (EMapPoint *point);

guint32 e_map_point_get_color_rgba (EMapPoint *point);

void e_map_point_set_color_rgba (EMap *map, EMapPoint *point, guint32 color_rgba);

void e_map_point_set_data (EMapPoint *point, gpointer data);

gpointer e_map_point_get_data (EMapPoint *point);

gboolean e_map_point_is_in_view (EMap *map, EMapPoint *point);

EMapPoint *e_map_get_closest_point (EMap *map, gdouble longitude, gdouble latitude,
				    gboolean in_view);

G_END_DECLS

#endif
