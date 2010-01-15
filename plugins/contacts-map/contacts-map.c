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

#include <glib/gi18n.h>

#include <libedataserver/e-source.h>
#include <libedataserverui/e-source-selector.h>

#include <shell/e-shell-sidebar.h>
#include <shell/e-shell-view.h>
#include <shell/e-shell-window.h>

/* Plugin entry points */

gboolean addressbook_map_init (GtkUIManager *ui_manager, EShellView *shell_view);
void action_show_ebook_map (GtkAction *action, EShellView *shell_view);
void show_map_general (ESourceSelector *selector);

/* Implementations */

gboolean
addressbook_map_init (GtkUIManager *ui_manager, EShellView *shell_view)
{
	EShellWindow *shell_window;
	GtkActionGroup *action_group;
	GtkAction *action;
	GIcon *icon;
	const gchar *tooltip;
	const gchar *name;
	const gchar *label;

	shell_window = e_shell_view_get_shell_window (shell_view);

	name = "contacts-map";
	label = _("Contacts map");
	tooltip = _("Show a map of all the contacts");
	action = gtk_action_new (name, NULL, tooltip, NULL);
	icon = g_themed_icon_new ("gnome-globe");
	gtk_action_set_gicon (action, icon);
	gtk_action_set_label (action, label);

	name = "contacts";
	action_group = e_shell_window_get_action_group (shell_window, name);
	gtk_action_group_add_action (action_group, action);

	g_signal_connect (
			action, "activate",
			G_CALLBACK (action_show_ebook_map), shell_view);

	g_object_unref (action);

	return TRUE;
}

void
action_show_ebook_map (GtkAction *action, EShellView *shell_view)
{
	EShellSidebar *shell_sidebar;
	ESourceSelector *selector = NULL;

	shell_sidebar = e_shell_view_get_shell_sidebar (shell_view);
	g_object_get (shell_sidebar, "selector", &selector, NULL);
	g_return_if_fail (selector != NULL);

	show_map_general (selector);

	g_object_unref (selector);
}

void
show_map_general (ESourceSelector *selector)
{
	EBook *book;
	ESource *primary_source;
	EBookQuery *query;
	GList *contacts, *tmp;
	gchar *uri;

	GeoclueGeocode *geocoder;
	GeocluePositionFields fields;
	GeoclueAccuracy *accuracy;

	gdouble lat = 0;
	gdouble lng = 0;

	GtkWidget *map_widget;
	ChamplainView  *view;
	ChamplainLayer *layer;

	gdouble *min_lat = NULL;
	gdouble *max_lat = NULL;
	gdouble *min_lng = NULL;
	gdouble *max_lng = NULL;

	primary_source = (ESource*)e_source_selector_peek_primary_selection (selector);
	uri = e_source_get_uri (primary_source);
	book = e_book_new_from_uri (uri, NULL);

	if (!book || !e_book_open (book, TRUE, NULL))
	{
		g_warning ("Couldn't load addressbook %s", uri);
		return;
	}

	/* Get all the contacts with an address */
	query = e_book_query_field_exists (E_CONTACT_ADDRESS);
	e_book_get_contacts (book, query, &contacts, NULL);
	e_book_query_unref (query);

	init_map (&view, &map_widget);
	layer = champlain_selection_layer_new ();

	geocoder = NULL;
	geocoder = get_geocoder ();
	if (geocoder != NULL) {
		for (tmp = contacts; tmp; tmp = tmp->next) {
			GError *error = NULL;
			EContact *contact;
			EContactAddress *addr;
			GHashTable *details;
			gint i;
			const gint addr_fields[] = {
				E_CONTACT_ADDRESS_HOME,
				E_CONTACT_ADDRESS_WORK,
				E_CONTACT_ADDRESS_OTHER
			};

			contact = tmp->data;

			/* Get the lat & lng and add the marker asynchronously */
			i = 0;
			addr = NULL;
			while (!addr && i<G_N_ELEMENTS(addr_fields)) {
				addr = e_contact_get(contact, addr_fields[i]);
				i++;
			}

			details = (GHashTable*) get_geoclue_from_address (addr);
			fields = geoclue_geocode_address_to_position (geocoder, details,
					&lat, &lng, NULL, &accuracy, &error);

			if (!error &&
			    (fields & GEOCLUE_POSITION_FIELDS_LATITUDE) != 0 &&
			    (fields & GEOCLUE_POSITION_FIELDS_LONGITUDE) != 0) {
				/* Add the marker to the map */
				add_marker (layer, lat, lng, contact);
				if (!min_lat) {
					min_lat = g_malloc (sizeof (gdouble));
					*min_lat = lat;
				}
				if (!max_lat) {
					max_lat = g_malloc (sizeof(gdouble));
					*max_lat = lat;
				}
				if (!min_lng) {
					min_lng = g_malloc (sizeof (gdouble));
					*min_lng = lng;
				}
				if (!max_lng) {
					max_lng = malloc (sizeof (gdouble));
					*max_lng = lng;
				}

				/* Store the min/max lat/lng */
				get_min_max (min_lat, max_lat,
						min_lng, max_lng, lat, lng);
			} else if (error) {
				g_warning ("Error while geocoding: %s\n", error->message);
				g_error_free (error);
			}

			g_hash_table_destroy (details);
			g_object_unref (contact);
		}
	}

	champlain_view_add_layer (view, layer);
	champlain_layer_show (layer);
	champlain_layer_show_all_markers (CHAMPLAIN_LAYER (layer));

	create_map_window (map_widget, _("Contacts map"));

	/* Do not ensure something visible is we have nothing */
	if (min_lat && min_lng && max_lat && max_lng)
		champlain_view_ensure_visible (view,
				*min_lat, *min_lng,
				*max_lat, *max_lng, FALSE);

	g_free (min_lat);
	g_free (max_lat);
	g_free (min_lng);
	g_free (max_lng);

	g_object_unref (geocoder);

	if (contacts != NULL)
		g_list_free (contacts);

	g_object_unref (book);
	g_free (uri);
}
