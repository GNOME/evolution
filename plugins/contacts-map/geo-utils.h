/*
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
 *		Cedric Bosdonnat <cedric.bosdonnat@free.fr>
 *
 * Copyright (C) 2009 Cedric Bosdonnat (http://cedric.bosdonnat.free.fr)
 *
 */

#include <gtk/gtk.h>
#include <glib.h>

#include <libebook/e-book.h>
#include <libebook/e-contact.h>

#include <geoclue/geoclue-geocode.h>
#include <champlain/champlain.h>
#include <champlain-gtk/champlain-gtk.h>
#include <clutter-gtk/clutter-gtk.h>

void
get_min_max (gdouble *min_lat, gdouble *max_lat,
        gdouble *min_lng, gdouble *max_lng,
        gdouble lat, gdouble lng);

GeoclueGeocode *get_geocoder (void);

void add_marker (
        ChamplainLayer *layer,
        gdouble lat, gdouble lng,
        EContact *contact);

GHashTable *get_geoclue_from_address (const EContactAddress* addr);

void init_map (ChamplainView **view, GtkWidget **widget);

void create_map_window (GtkWidget *map_widget, const gchar *title);
