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

#include "geo-utils.h"

static gboolean is_clutter_initialized = FALSE;

void
get_min_max (gdouble *min_lat, gdouble *max_lat,
	     gdouble *min_lng, gdouble *max_lng,
	     gdouble lat, gdouble lng)
{
	if (lat < *min_lat)
		*min_lat = lat;
	else if (lat > *max_lat)
		*max_lat = lat;

	if (lng < *min_lng)
		*min_lng = lng;
	else if (lng > *max_lng)
		*max_lng = lng;
}

void
add_marker (ChamplainLayer *layer, gdouble lat, gdouble lng, EContact *contact)
{
	ClutterActor *marker;

	gchar *contact_name = e_contact_get (contact, E_CONTACT_FULL_NAME);
	marker = champlain_marker_new_with_text (contact_name, "Serif 8", NULL, NULL);
	g_free (contact_name);

	champlain_marker_set_use_markup (CHAMPLAIN_MARKER (marker), FALSE);
	champlain_base_marker_set_position (CHAMPLAIN_BASE_MARKER (marker), lat, lng);

	champlain_layer_add_marker (layer, CHAMPLAIN_BASE_MARKER(marker));
}

GeoclueGeocode*
get_geocoder (void)
{
	GeoclueGeocode *geocoder = NULL;

	/* Create new GeoclueGeocode */
	geocoder = geoclue_geocode_new ("org.freedesktop.Geoclue.Providers.Yahoo",
			"/org/freedesktop/Geoclue/Providers/Yahoo");

	return geocoder;
}

GHashTable *
get_geoclue_from_address (const EContactAddress* addr)
{
	GHashTable *address = geoclue_address_details_new ();

	g_hash_table_insert (address, g_strdup (GEOCLUE_ADDRESS_KEY_POSTALCODE), g_strdup ((*addr).code));
	g_hash_table_insert (address, g_strdup (GEOCLUE_ADDRESS_KEY_COUNTRY), g_strdup ((*addr).country));
	g_hash_table_insert (address, g_strdup (GEOCLUE_ADDRESS_KEY_LOCALITY), g_strdup ((*addr).locality));
	g_hash_table_insert (address, g_strdup (GEOCLUE_ADDRESS_KEY_STREET), g_strdup ((*addr).street));

	return address;
}

void
init_map (ChamplainView **view, GtkWidget **widget)
{
	if (!is_clutter_initialized) {
		gtk_clutter_init (NULL, NULL);
		is_clutter_initialized = TRUE;
	}

	*widget = gtk_champlain_embed_new ();
	*view = gtk_champlain_embed_get_view (GTK_CHAMPLAIN_EMBED (*widget));

	champlain_view_set_show_license (*view, FALSE);

	g_object_set (G_OBJECT (*view), "scroll-mode", CHAMPLAIN_SCROLL_MODE_KINETIC,
			"zoom-level", 9, NULL);
}

void
create_map_window (GtkWidget *map_widget, const gchar *title)
{
	GtkWidget *window, *viewport;

	/* create the main, top level, window */
	window = gtk_window_new (GTK_WINDOW_TOPLEVEL);

	/* give the window a 10px wide border */
	gtk_container_set_border_width (GTK_CONTAINER (window), 10);

	/* give it the title */
	gtk_window_set_title (GTK_WINDOW (window), title );

	gtk_widget_set_size_request (map_widget, 300, 300);

	viewport = gtk_frame_new (NULL);
	gtk_container_add (GTK_CONTAINER (viewport), map_widget);

	/* and insert it into the main window  */
	gtk_container_add (GTK_CONTAINER (window), viewport);

	/* make sure that everything, window and label, are visible */
	gtk_widget_show_all (window);
}
