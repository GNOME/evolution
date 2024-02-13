/*
 * e-contact-map.c
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
 * Copyright (C) 2011 Dan Vratil <dvratil@redhat.com>
 */

#include "evolution-config.h"

#ifdef ENABLE_CONTACT_MAPS

#include "e-contact-map.h"

#include <champlain/champlain.h>
#include <champlain-gtk/champlain-gtk.h>
#include <geocode-glib/geocode-glib.h>

#include <clutter/clutter.h>

#include <string.h>
#include <glib/gi18n.h>
#include <math.h>

#include "e-util/e-util.h"

typedef struct _AsyncContext AsyncContext;

struct _EContactMapPrivate {
	GHashTable *markers; /* Hash table contact-name -> marker */

	ChamplainMarkerLayer *marker_layer;
};

struct _AsyncContext {
	EContactMap *map;
	ClutterActor *marker;
	GHashTable *params;
	gint stage;
};

enum {
	CONTACT_ADDED,
	CONTACT_REMOVED,
	GEOCODING_STARTED,
	GEOCODING_FAILED,
	LAST_SIGNAL
};

static gint signals[LAST_SIGNAL] = {0};

G_DEFINE_TYPE_WITH_PRIVATE (EContactMap, e_contact_map, GTK_CHAMPLAIN_TYPE_EMBED)

static void
async_context_free (AsyncContext *async_context)
{
	g_clear_object (&async_context->map);
	g_hash_table_unref (async_context->params);

	g_slice_free (AsyncContext, async_context);
}

static ClutterActor *
texture_new_from_pixbuf (GdkPixbuf *pixbuf,
                         GError **error)
{
	ClutterActor *texture = NULL;
	const guchar *data;
	gboolean has_alpha, success;
	gint width, height, rowstride;
	ClutterTextureFlags flags = 0;

	data = gdk_pixbuf_get_pixels (pixbuf);
	width = gdk_pixbuf_get_width (pixbuf);
	height = gdk_pixbuf_get_height (pixbuf);
	has_alpha = gdk_pixbuf_get_has_alpha (pixbuf);
	rowstride = gdk_pixbuf_get_rowstride (pixbuf);

	texture = clutter_texture_new ();
	success = clutter_texture_set_from_rgb_data (
		CLUTTER_TEXTURE (texture),
		data, has_alpha, width, height, rowstride,
		(has_alpha ? 4: 3), flags, NULL);

	if (!success) {
		clutter_actor_destroy (CLUTTER_ACTOR (texture));
		texture = NULL;
	}

	return texture;
}

static ClutterActor *
contact_map_photo_to_texture (EContactPhoto *photo)
{
	ClutterActor *texture = NULL;
	GdkPixbuf *pixbuf = NULL;

	if  (photo->type == E_CONTACT_PHOTO_TYPE_INLINED) {
		GdkPixbufLoader *loader = gdk_pixbuf_loader_new ();

		gdk_pixbuf_loader_write (
			loader, photo->data.inlined.data,
			photo->data.inlined.length, NULL);
		gdk_pixbuf_loader_close (loader, NULL);
		pixbuf = gdk_pixbuf_loader_get_pixbuf (loader);
		if (pixbuf != NULL)
			g_object_ref (pixbuf);
		g_object_unref (loader);

	} else if (photo->type == E_CONTACT_PHOTO_TYPE_URI) {
		pixbuf = gdk_pixbuf_new_from_file (photo->data.uri, NULL);
	}

	if (pixbuf != NULL) {
		texture = texture_new_from_pixbuf (pixbuf, NULL);
		g_object_unref (pixbuf);
	}

	return texture;
}

static void
contact_map_address_resolved_cb (GObject *source_object,
                                 GAsyncResult *result,
                                 gpointer user_data)
{
	GHashTable *resolved = NULL;
	gpointer marker_ptr;
	AsyncContext *async_context = user_data;
	ChamplainMarkerLayer *marker_layer;
	ChamplainMarker *marker;
	GeocodePlace *place;
	GeocodeLocation *location;
	GList *search_results;
	const gchar *name;
	GError *local_error = NULL;

	marker = CHAMPLAIN_MARKER (async_context->marker);
	marker_layer = async_context->map->priv->marker_layer;
	name = champlain_label_get_text (CHAMPLAIN_LABEL (marker));

	/* If the marker_layer does not exist anymore, the map has
	 * probably been destroyed before this callback was launched.
	 * It's not a failure, just silently clean up what was left
	 * behind and pretend nothing happened. */

	if (!CHAMPLAIN_IS_MARKER_LAYER (marker_layer))
		goto exit;

	search_results = geocode_forward_search_finish (
		GEOCODE_FORWARD (source_object), result, &local_error);

	/* Sanity check. */
	g_warn_if_fail (
		((search_results != NULL) && (local_error == NULL)) ||
		((search_results == NULL) && (local_error != NULL)));

	/* Keep quiet if the search just came up empty. */
	if (g_error_matches (local_error, GEOCODE_ERROR, GEOCODE_ERROR_NO_MATCHES)) {
		g_clear_error (&local_error);
		while (async_context->stage < 4) {
			gboolean limited = FALSE;

			async_context->stage++;

			switch (async_context->stage) {
			case 1:
				limited = g_hash_table_remove (async_context->params, "region");
				break;
			case 2:
				limited = g_hash_table_remove (async_context->params, "street");
				break;
			case 3:
				limited = g_hash_table_remove (async_context->params, "postalcode");
				break;
			case 4:
				limited = g_hash_table_remove (async_context->params, "locality");
				break;
			}

			if (limited && g_hash_table_size (async_context->params) > 0) {
				GeocodeForward *geocoder;

				geocoder = geocode_forward_new_for_params (async_context->params);

				geocode_forward_search_async (
					geocoder, NULL,
					contact_map_address_resolved_cb,
					async_context);

				g_object_unref (geocoder);

				return;
			}
		}

	/* Leave a breadcrumb on the console for any other errors. */
	} else if (local_error != NULL) {
		g_warning ("%s: %s", G_STRFUNC, local_error->message);
		g_clear_error (&local_error);
	}

	if (search_results == NULL) {
		g_signal_emit (
			async_context->map,
			signals[GEOCODING_FAILED], 0, name);
		goto exit;
	}

	place = GEOCODE_PLACE (search_results->data);
	location = geocode_place_get_location (place);

	/* Move the marker to resolved position. */
	champlain_location_set_location (
		CHAMPLAIN_LOCATION (marker),
		geocode_location_get_latitude (location),
		geocode_location_get_longitude (location));
	champlain_marker_layer_add_marker (marker_layer, marker);
	champlain_marker_set_selected (marker, FALSE);

	/* Do not unref the list elements. */
	g_list_free (search_results);

	/* Store the marker in the hash table, using its label as key. */
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
add_attr (GHashTable *hash_table,
          const gchar *key,
          const gchar *string)
{
	GValue *value;

	if (!string || !*string)
		return;

	value = g_new0 (GValue, 1);
	g_value_init (value, G_TYPE_STRING);
	g_value_set_string (value, string);

	g_hash_table_insert (hash_table, g_strdup (key), value);
}

static void
free_gvalue (gpointer ptr)
{
	GValue *value = ptr;

	if (value) {
		g_value_unset (value);
		g_free (value);
	}
}

static GHashTable *
address_to_xep (EContactAddress *address)
{

	GHashTable *hash_table;

	hash_table = g_hash_table_new_full (
		(GHashFunc) g_str_hash,
		(GEqualFunc) g_str_equal,
		(GDestroyNotify) g_free,
		(GDestroyNotify) free_gvalue);

	add_attr (hash_table, "postalcode", address->code);
	add_attr (hash_table, "country", address->country);
	add_attr (hash_table, "region", address->region);
	add_attr (hash_table, "locality", address->locality);
	add_attr (hash_table, "street", address->street);

	return hash_table;
}

static void
contact_map_finalize (GObject *object)
{
	EContactMap *self = E_CONTACT_MAP (object);

	g_hash_table_destroy (self->priv->markers);

	/* Chain up to parent's finalize() method. */
	G_OBJECT_CLASS (e_contact_map_parent_class)->finalize (object);
}

static void
e_contact_map_class_init (EContactMapClass *class)
{
	GObjectClass *object_class;

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
	ChamplainMarkerLayer *layer;
	ChamplainView *view;

	map->priv = e_contact_map_get_instance_private (map);

	map->priv->markers = g_hash_table_new_full (
		(GHashFunc) g_str_hash,
		(GEqualFunc) g_str_equal,
		(GDestroyNotify) g_free,
		(GDestroyNotify) NULL);

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
	return g_object_new (E_TYPE_CONTACT_MAP, NULL);
}

void
e_contact_map_add_contact (EContactMap *map,
                           EContact *contact)
{
	EContactAddress *address;
	EContactPhoto *photo;
	const gchar *contact_name;
	const gchar *contact_uid;

	g_return_if_fail (E_IS_CONTACT_MAP (map));
	g_return_if_fail (E_IS_CONTACT (contact));

	photo = e_contact_get (contact, E_CONTACT_PHOTO);
	contact_name = e_contact_get_const (contact, E_CONTACT_FILE_AS);
	contact_uid = e_contact_get_const (contact, E_CONTACT_UID);

	address = e_contact_get (contact, E_CONTACT_ADDRESS_HOME);
	if (address != NULL) {
		gchar *name;

		name = g_strdup_printf (
			"%s (%s)", contact_name, _("Home"));
		e_contact_map_add_marker (
			map, name, contact_uid, address, photo);
		g_free (name);

		e_contact_address_free (address);
	}

	address = e_contact_get (contact, E_CONTACT_ADDRESS_WORK);
	if (address != NULL) {
		gchar *name;

		name = g_strdup_printf (
			"%s (%s)", contact_name, _("Work"));
		e_contact_map_add_marker (
			map, name, contact_uid, address, photo);
		g_free (name);

		e_contact_address_free (address);
	}

	if (photo != NULL)
		e_contact_photo_free (photo);
}

void
e_contact_map_add_marker (EContactMap *map,
                          const gchar *name,
                          const gchar *contact_uid,
                          EContactAddress *address,
                          EContactPhoto *photo)
{
	ClutterActor *marker;
	GHashTable *hash_table;
	GeocodeForward *geocoder;
	AsyncContext *async_context;

	g_return_if_fail (E_IS_CONTACT_MAP (map));
	g_return_if_fail (name != NULL);
	g_return_if_fail (contact_uid != NULL);
	g_return_if_fail (address != NULL);

	hash_table = address_to_xep (address);

	if (!g_hash_table_size (hash_table)) {
		g_hash_table_unref (hash_table);
		return;
	}

	marker = champlain_label_new ();
	champlain_label_set_text (CHAMPLAIN_LABEL (marker), name);

	if (photo != NULL) {
		champlain_label_set_image (
			CHAMPLAIN_LABEL (marker),
			contact_map_photo_to_texture (photo));
	}

	/* Stash the contact UID for EContactMapWindow. */
	g_object_set_data_full (
		G_OBJECT (marker), "contact-uid",
		g_strdup (contact_uid),
		(GDestroyNotify) g_free);

	geocoder = geocode_forward_new_for_params (hash_table);

	async_context = g_slice_new0 (AsyncContext);
	async_context->map = g_object_ref (map);
	async_context->marker = marker;
	async_context->params = hash_table;
	async_context->stage = 0;

	geocode_forward_search_async (
		geocoder, NULL,
		contact_map_address_resolved_cb,
		async_context);

	g_object_unref (geocoder);

	g_signal_emit (map, signals[GEOCODING_STARTED], 0, marker);
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

	g_return_if_fail (E_IS_CONTACT_MAP (map));
	g_return_if_fail (name != NULL);

	marker = g_hash_table_lookup (map->priv->markers, name);

	champlain_marker_layer_remove_marker (map->priv->marker_layer, marker);

	g_hash_table_remove (map->priv->markers, name);

	g_signal_emit (map, signals[CONTACT_REMOVED], 0, name);
}

void
e_contact_map_zoom_on_marker (EContactMap *map,
                              ClutterActor *marker)
{
	ChamplainView *view;
	gdouble lat, lng;

	g_return_if_fail (E_IS_CONTACT_MAP (map));
	g_return_if_fail (CLUTTER_IS_ACTOR (marker));

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

#endif /* ENABLE_CONTACT_MAPS */
