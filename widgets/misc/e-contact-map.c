/*
 * e-contact-map.c
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
 * Copyright (C) 2011 Dan Vratil <dvratil@redhat.com>
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#ifdef WITH_CONTACT_MAPS

#include "e-contact-map.h"
#include "e-contact-marker.h"

#include <e-util/e-marshal.h>

#include <champlain/champlain.h>
#include <champlain-gtk/champlain-gtk.h>
#include <geoclue/geoclue-address.h>
#include <geoclue/geoclue-position.h>
#include <geocode-glib.h>

#include <clutter/clutter.h>

#include <string.h>
#include <glib/gi18n.h>
#include <math.h>

#define E_CONTACT_MAP_GET_PRIVATE(obj) \
	(G_TYPE_INSTANCE_GET_PRIVATE \
	((obj), E_TYPE_CONTACT_MAP, EContactMapPrivate))

typedef struct _AsyncContext AsyncContext;

struct _EContactMapPrivate {
	GHashTable *markers; /* Hash table contact-name -> marker */

	ChamplainMarkerLayer *marker_layer;
};

struct _AsyncContext {
	EContactMap *map;
	EContactMarker *marker;
};

enum {
	CONTACT_ADDED,
	CONTACT_REMOVED,
	GEOCODING_STARTED,
	GEOCODING_FAILED,
	LAST_SIGNAL
};

static gint signals[LAST_SIGNAL] = {0};

G_DEFINE_TYPE (EContactMap, e_contact_map, GTK_CHAMPLAIN_TYPE_EMBED)

static void
async_context_free (AsyncContext *async_context)
{
	if (async_context->map != NULL)
		g_object_unref (async_context->map);

	g_slice_free (AsyncContext, async_context);
}

static void
contact_map_address_resolved_cb (GObject *source,
                                 GAsyncResult *result,
                                 gpointer user_data)
{
	GHashTable *resolved = NULL;
	gpointer marker_ptr;
	const gchar *name;
	gdouble latitude, longitude;
	AsyncContext *async_context = user_data;
	ChamplainMarkerLayer *marker_layer;
	ChamplainMarker *marker;
	GError *error = NULL;

	g_return_if_fail (async_context != NULL);
	g_return_if_fail (E_IS_CONTACT_MAP (async_context->map));
	g_return_if_fail (E_IS_CONTACT_MARKER (async_context->marker));

	marker = CHAMPLAIN_MARKER (async_context->marker);
	marker_layer = async_context->map->priv->marker_layer;

	/* If the marker_layer does not exist anymore, the map has
	 * probably been destroyed before this callback was launched.
	 * It's not a failure, just silently clean up what was left
	 * behind and pretend nothing happened. */

	if (!CHAMPLAIN_IS_MARKER_LAYER (marker_layer))
		goto exit;

	resolved = geocode_object_resolve_finish (
		GEOCODE_OBJECT (source), result, &error);

	if (resolved == NULL ||
	    !geocode_object_get_coords (resolved, &longitude, &latitude)) {
		const gchar *name;
		if (error)
			g_error_free (error);
		name = champlain_label_get_text (CHAMPLAIN_LABEL (marker));
		g_signal_emit (
			async_context->map,
			signals[GEOCODING_FAILED], 0, name);
		goto exit;
	}

	/* Move the marker to resolved position */
	champlain_location_set_location (
		CHAMPLAIN_LOCATION (marker), latitude, longitude);
	champlain_marker_layer_add_marker (marker_layer, marker);
	champlain_marker_set_selected (marker, FALSE);

	/* Store the marker in the hash table. Use it's label as key */
	name = champlain_label_get_text (CHAMPLAIN_LABEL (marker));
	marker_ptr = g_hash_table_lookup (
		async_context->map->priv->markers, name);
	if (marker_ptr != NULL) {
		g_hash_table_remove (async_context->map->priv->markers, name);
		champlain_marker_layer_remove_marker (marker_layer, marker_ptr);
	}
	g_hash_table_insert (
		async_context->map->priv->markers,
		g_strdup (name), marker);

	g_signal_emit (
		async_context->map,
		signals[CONTACT_ADDED], 0, marker);

exit:
	async_context_free (async_context);

	if (resolved != NULL)
		g_hash_table_unref (resolved);
}

static void
resolve_marker_position (EContactMap *map,
                         EContactMarker *marker,
                         EContactAddress *address)
{
	GeocodeObject *geocoder;
	AsyncContext *async_context;
	const gchar *key;

	g_return_if_fail (E_IS_CONTACT_MAP (map));
	g_return_if_fail (address != NULL);

	geocoder = geocode_object_new ();

	key = GEOCODE_OBJECT_FIELD_POSTAL;
	geocode_object_add (geocoder, key, address->code);

	key = GEOCODE_OBJECT_FIELD_COUNTRY;
	geocode_object_add (geocoder, key, address->country);

	key = GEOCODE_OBJECT_FIELD_STATE;
	geocode_object_add (geocoder, key, address->region);

	key = GEOCODE_OBJECT_FIELD_CITY;
	geocode_object_add (geocoder, key, address->locality);

	key = GEOCODE_OBJECT_FIELD_STREET;
	geocode_object_add (geocoder, key, address->street);

	async_context = g_slice_new0 (AsyncContext);
	async_context->map = g_object_ref (map);
	async_context->marker = marker;

	geocode_object_resolve_async (
		geocoder, NULL,
		contact_map_address_resolved_cb,
		async_context);

	g_object_unref (geocoder);

	g_signal_emit (map, signals[GEOCODING_STARTED], 0, marker);
}

static void
contact_map_finalize (GObject *object)
{
	EContactMapPrivate *priv;

	priv = E_CONTACT_MAP (object)->priv;

	if (priv->markers) {
		g_hash_table_destroy (priv->markers);
		priv->markers = NULL;
	}

	/* Chain up to parent's finalize() method. */
	G_OBJECT_CLASS (e_contact_map_parent_class)->finalize (object);
}

static void
e_contact_map_class_init (EContactMapClass *class)
{
	GObjectClass *object_class;

	g_type_class_add_private (class, sizeof (EContactMapPrivate));

	object_class = G_OBJECT_CLASS (class);
	object_class->finalize = contact_map_finalize;

	signals[CONTACT_ADDED] = g_signal_new (
		"contact-added",
		G_TYPE_FROM_CLASS (class),
		G_SIGNAL_RUN_FIRST,
		G_STRUCT_OFFSET (EContactMapClass, contact_added),
		NULL, NULL,
		g_cclosure_marshal_VOID__OBJECT,
		G_TYPE_NONE, 1, G_TYPE_OBJECT);

	signals[CONTACT_REMOVED] = g_signal_new (
		"contact-removed",
		G_TYPE_FROM_CLASS (class),
		G_SIGNAL_RUN_FIRST,
		G_STRUCT_OFFSET (EContactMapClass, contact_removed),
		NULL, NULL,
		g_cclosure_marshal_VOID__STRING,
		G_TYPE_NONE, 1, G_TYPE_STRING);

	signals[GEOCODING_STARTED] = g_signal_new (
		"geocoding-started",
		G_TYPE_FROM_CLASS (class),
		G_SIGNAL_RUN_FIRST,
		G_STRUCT_OFFSET (EContactMapClass, geocoding_started),
		NULL, NULL,
		g_cclosure_marshal_VOID__OBJECT,
		G_TYPE_NONE, 1, G_TYPE_OBJECT);

	signals[GEOCODING_FAILED] = g_signal_new (
		"geocoding-failed",
		G_TYPE_FROM_CLASS (class),
		G_SIGNAL_RUN_FIRST,
		G_STRUCT_OFFSET (EContactMapClass, geocoding_failed),
		NULL, NULL,
		g_cclosure_marshal_VOID__STRING,
		G_TYPE_NONE, 1, G_TYPE_STRING);
}

static void
e_contact_map_init (EContactMap *map)
{
	GHashTable *hash_table;
	ChamplainMarkerLayer *layer;
	ChamplainView *view;

	map->priv = E_CONTACT_MAP_GET_PRIVATE (map);

	hash_table = g_hash_table_new_full (g_str_hash, g_str_equal,
			(GDestroyNotify) g_free, NULL);

	map->priv->markers = hash_table;

	view = gtk_champlain_embed_get_view (GTK_CHAMPLAIN_EMBED (map));
	/* This feature is somehow broken sometimes, so disable it for now */
	champlain_view_set_zoom_on_double_click (view, FALSE);
	layer = champlain_marker_layer_new_full (CHAMPLAIN_SELECTION_SINGLE);
	champlain_view_add_layer (view, CHAMPLAIN_LAYER (layer));
	map->priv->marker_layer = layer;
}

GtkWidget *
e_contact_map_new (void)
{
	return g_object_new (
		E_TYPE_CONTACT_MAP,NULL);
}

void
e_contact_map_add_contact (EContactMap *map,
                           EContact *contact)
{
	EContactAddress *address;
	EContactPhoto *photo;
	const gchar *contact_uid;
	gchar *name;

	g_return_if_fail (map && E_IS_CONTACT_MAP (map));
	g_return_if_fail (contact && E_IS_CONTACT (contact));

	photo = e_contact_get (contact, E_CONTACT_PHOTO);
	contact_uid = e_contact_get_const (contact, E_CONTACT_UID);

	address = e_contact_get (contact, E_CONTACT_ADDRESS_HOME);
	if (address) {
		name = g_strconcat (e_contact_get_const (contact, E_CONTACT_FILE_AS), " (", _("Home"), ")", NULL);
		e_contact_map_add_marker (map, name, contact_uid, address, photo);
		g_free (name);
		e_contact_address_free (address);
	}

	address = e_contact_get (contact, E_CONTACT_ADDRESS_WORK);
	if (address) {
		name = g_strconcat (e_contact_get_const (contact, E_CONTACT_FILE_AS), " (", _("Work"), ")", NULL);
		e_contact_map_add_marker (map, name, contact_uid, address, photo);
		g_free (name);
		e_contact_address_free (address);
	}

	if (photo)
		e_contact_photo_free (photo);
}

void
e_contact_map_add_marker (EContactMap *map,
                          const gchar *name,
                          const gchar *contact_uid,
                          EContactAddress *address,
                          EContactPhoto *photo)
{
	EContactMarker *marker;

	g_return_if_fail (map && E_IS_CONTACT_MAP (map));
	g_return_if_fail (name && *name);
	g_return_if_fail (contact_uid && *contact_uid);
	g_return_if_fail (address);

	marker = E_CONTACT_MARKER (e_contact_marker_new (name, contact_uid, photo));

	resolve_marker_position (map, marker, address);
}

/**
 * The \name parameter must match the label of the
 * marker (for example "John Smith (work)")
 */
void
e_contact_map_remove_contact (EContactMap *map,
                              const gchar *name)
{
	ChamplainMarker *marker;

	g_return_if_fail (map && E_IS_CONTACT_MAP (map));
	g_return_if_fail (name && *name);

	marker = g_hash_table_lookup (map->priv->markers, name);

	champlain_marker_layer_remove_marker (map->priv->marker_layer, marker);

	g_hash_table_remove (map->priv->markers, name);

	g_signal_emit (map, signals[CONTACT_REMOVED], 0, name);
}

void
e_contact_map_remove_marker (EContactMap *map,
                             ClutterActor *marker)
{
	const gchar *name;

	g_return_if_fail (map && E_IS_CONTACT_MAP (map));
	g_return_if_fail (marker && CLUTTER_IS_ACTOR (marker));

	name = champlain_label_get_text (CHAMPLAIN_LABEL (marker));

	e_contact_map_remove_contact (map, name);
}

void
e_contact_map_zoom_on_marker (EContactMap *map,
                              ClutterActor *marker)
{
	ChamplainView *view;
	gdouble lat, lng;

	g_return_if_fail (map && E_IS_CONTACT_MAP (map));
	g_return_if_fail (marker && CLUTTER_IS_ACTOR (marker));

	lat = champlain_location_get_latitude (CHAMPLAIN_LOCATION (marker));
	lng = champlain_location_get_longitude (CHAMPLAIN_LOCATION (marker));

	view = gtk_champlain_embed_get_view (GTK_CHAMPLAIN_EMBED (map));

	champlain_view_center_on (view, lat, lng);
	champlain_view_set_zoom_level (view, 15);
}

ChamplainView *
e_contact_map_get_view (EContactMap *map)
{
	g_return_val_if_fail (E_IS_CONTACT_MAP (map), NULL);

	return gtk_champlain_embed_get_view (GTK_CHAMPLAIN_EMBED (map));
}

#endif /* WITH_CONTACT_MAPS */
