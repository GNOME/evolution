/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* e-summary-weather.c
 *
 * Copyright (C) 2001 Ximian, Inc.
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
 *
 * Author: Iain Holmes
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <glib.h>
#include <gtk/gtkctree.h>
#include <gtk/gtkmain.h>

#include <libgnome/gnome-i18n.h>
#include <libgnome/gnome-config.h>

#include <gal/widgets/e-unicode.h>

#include <string.h>

#include "e-summary.h"
#include "e-summary-shown.h"
#include "e-summary-weather.h"
#include "e-summary-preferences.h"
#include "weather.h"
#include "metar.h"

struct _ESummaryWeather {
	ESummaryConnection *connection;
	GList *weathers;

	char *html;
	guint32 timeout;

	gboolean online;
	gboolean errorshown;
};

static GHashTable *locations_hash = NULL;

char *
e_summary_weather_get_html (ESummary *summary)
{
	GList *weathers;
	GString *string;
	char *html;
	char *s;

	if (summary->weather == NULL || summary->weather->weathers == NULL) {
		return NULL;
	}

	string = g_string_new ("<dl><img src=\"ico-weather.png\" align=\"middle\" "
	                       "alt=\"\" width=\"48\" height=\"48\"><b>"
	                       "<a href=\"http://www.metoffice.gov.uk\">");
	g_string_append (string, _("My Weather"));
	g_string_append (string, "</a></b>");

	for (weathers = summary->weather->weathers; weathers; weathers = weathers->next) {
		if (((Weather *)weathers->data)->html == NULL) {
			continue;
		}

		g_string_append (string, ((Weather *)weathers->data)->html);
	}

	g_string_append (string, "</dl>");

	html = string->str;
	g_string_free (string, FALSE);

	return html;
}

static char *
make_url (const char *code)
{
	return g_strdup_printf ("http://weather.noaa.gov/cgi-bin/mgetmetar.pl?cccc=%s", code);
}

static void
weather_make_html (Weather *w)
{
	GString *string;
	ESummaryWeatherLocation *location;
	char *sky, *temp, *cond, *s;
	const char *icon_name;

	icon_name = icon_from_weather (w);
	string = g_string_new ("");
	g_string_sprintf (string, "<dd><img align=\"middle\" "
			  "src=\"%s\" width=\"16\" height=\"16\">&#160;<b>",
			  icon_name);
	location = g_hash_table_lookup (locations_hash, w->location);
#if 0
	if (location == NULL) {
		url = make_anchor (w->location, w->location);
	} else {
		url = make_anchor (location->name, w->location);
	}
#endif
	if (location == NULL) {
		g_string_append (string, w->location);
	} else {
		g_string_append (string, location->name);
	}

#if 0
	g_string_append (string, url);
	g_free (url);
#endif

	g_string_append (string, "</b>:<blockquote><font size=\"-1\">");
	sky = (char *) weather_sky_string (w);
	temp = weather_temp_string (w);
	cond = (char *) weather_conditions_string (w);

	g_string_append (string, sky);
	g_string_append_c (string, ' ');
	g_string_append (string, cond);
	g_string_append_c (string, ' ');
	g_string_append (string, temp);

	g_free (temp);

#if 0
	g_string_append (string, "<font size=\"-1\">");
	
	uri = g_strdup_printf ("<a href=\"weather://%p\">", w);
	g_string_append (string, uri);
	g_free (uri);
	g_string_append (string, "(More)</a></font></font></blockquote></dd>");
#else
	g_string_append (string, "</font></blockquote></dd>");
#endif
	if (w->html != NULL) {
		g_free (w->html);
	}
	w->html = string->str;
	g_string_free (string, FALSE);

	e_summary_draw (w->summary);
}

static ESummaryWeatherLocation *
weather_location_new (char **locdata)
{
	ESummaryWeatherLocation *location;

	location = g_new (ESummaryWeatherLocation, 1);
	location->name = g_strdup (locdata[0]);
	location->code = g_strdup (locdata[1]);
	location->zone = g_strdup (locdata[2]);
	location->radar = g_strdup (locdata[3]);

	return location;
}

static void
parse_metar_token (const char *token,
		   gboolean in_comment,
		   Weather *w)
{
	if (in_comment == FALSE) {
		if (metar_tok_time ((char *) token, w)) {
			return;
		} else if (metar_tok_wind ((char *) token, w)) {
			return;
		} else if (metar_tok_vis ((char *) token, w)) {
			return;
		} else if (metar_tok_cloud ((char *) token, w)) {
			return;
		} else if (metar_tok_temp ((char *) token, w)) {
			return;
		} else if (metar_tok_pres ((char *) token, w)) {
			return;
		} else if (metar_tok_cond ((char *) token, w)) {
			return;
		}
	}
}

static void
parse_metar (const char *metar,
	     Weather *w)
{
	char *metar_dup;
	char **toks;
	gint ntoks;
	gint i;
	gboolean in_remark = FALSE;

	metar_dup = g_strdup (metar + 6);

	metar_init_re ();

	toks = g_strsplit (metar, " ", 0);

	for (ntoks = 0; toks[ntoks]; ntoks++) {
		if (strcmp (toks[ntoks], "RMK") == 0) {
			in_remark = TRUE;
		}
	}

	for (i = ntoks - 1; i >= 0; i--) {
		if (*toks[i] != '\0') {
			if (strcmp (toks[i], "RMK") == 0) {
				in_remark = FALSE;
			} else {
				parse_metar_token (toks[i], in_remark, w);
			}
		}
	}

	g_strfreev (toks);
	g_free (metar_dup);
	weather_make_html (w);
}

static void
message_finished (SoupMessage *msg,
		  gpointer userdata)
{
	Weather *w = (Weather *) userdata;
	ESummary *summary;
	char *html, *metar, *end;
	char *search_str;

	summary = w->summary;
	if (summary->weather->connection->callback) {
		ESummaryConnection *connection = summary->weather->connection;
		connection->callback (summary, connection->callback_closure);
	}

	if (SOUP_MESSAGE_IS_ERROR (msg)) {
		ESummaryWeatherLocation *location;

		g_warning ("Message failed: %d\n%s", msg->errorcode,
			   msg->errorphrase);
		w->message = NULL;

		location = g_hash_table_lookup (locations_hash, w->location);

		w->html = g_strdup_printf ("<br><b>%s %s</b></br>",
					   _("There was an error downloading data for"),
					   location ? location->name : w->location);

		e_summary_draw (w->summary);
		return;
	}

	html = g_strdup (msg->response.body);
	w->message = NULL;

	/* Find the metar data */
	search_str = g_strdup_printf ("\n%s", w->location);
	metar = strstr (html, search_str);
	if (metar == NULL) {
		g_free (search_str);
		return;
	}

	metar++;
	end = strchr (metar, '\n');
	if (end == NULL) {
		g_free (search_str);
		return;
	}
	*end = '\0';

	parse_metar (metar, w);
	g_free (search_str);
	return;
}

gboolean
e_summary_weather_update (ESummary *summary)
{
	GList *w;

	if (summary->weather->online == FALSE) {
		g_warning ("%s: Repolling but offline", G_GNUC_FUNCTION);
		return TRUE;
	}

	summary->weather->errorshown = FALSE;
	for (w = summary->weather->weathers; w; w = w->next) {
		SoupContext *context;
		char *uri;
		Weather *weather = w->data;

		if (weather->message != NULL) {
			continue;
		}

		uri = g_strdup_printf ("http://weather.noaa.gov/cgi-bin/mgetmetar.pl?cccc=%s", weather->location);
		context = soup_context_get (uri);
		if (context == NULL) {
			g_warning ("Invalid URL: %s", uri);
			soup_context_unref (context);
			g_free (uri);
			continue;
		}

		weather->message = soup_message_new (context, SOUP_METHOD_GET);
		soup_context_unref (context);
		soup_message_queue (weather->message, message_finished, weather);

		g_free (uri);
	}

	return TRUE;
}

static void
weather_free (Weather *w)
{
	g_return_if_fail (w != NULL);

	if (w->message != NULL) {
		soup_message_cancel (w->message);
	}

	g_free (w->location);
	g_free (w->html);
	g_free (w);
}

static void
e_summary_weather_add_location (ESummary *summary,
				const char *location)
{
	Weather *w;

	w = g_new0 (Weather, 1);
	w->summary = summary;
	w->location = g_strdup (location);
	summary->weather->weathers = g_list_append (summary->weather->weathers, w);
}

static gboolean
e_summary_weather_init_locations (void) 
{
	int nregions, iregions;
	char **regions;

	if (locations_hash != NULL) {
		return TRUE;
	}

	locations_hash = g_hash_table_new (g_str_hash, g_str_equal);

	gnome_config_push_prefix ("=" LOCATIONDIR "/Locations=/");

	gnome_config_get_vector ("Main/regions", &nregions, &regions);
	for (iregions = nregions - 1; iregions >= 0; iregions--) {
		int nstates, istates;
		char **states;
		char *region_name;
		char *region_name_key;
		char *states_key;

		region_name_key = g_strconcat (regions[iregions], "/name", NULL);
		states_key = g_strconcat (regions[iregions], "/states", NULL);
		region_name = gnome_config_get_string (region_name_key);

		gnome_config_get_vector (states_key, &nstates, &states);

		for (istates = nstates - 1; istates >= 0; istates--) {
			void *iter;
			char *iter_key, *iter_val;
			char *state_path, *state_name_key, *state_name;

			state_path = g_strconcat (regions[iregions], "_", states[istates], "/", NULL);
			state_name_key = g_strconcat (state_path, "name", NULL);
			state_name = gnome_config_get_string (state_name_key);

			iter = gnome_config_init_iterator (state_path);

			while ((iter = gnome_config_iterator_next (iter, &iter_key, &iter_val)) != NULL) {
				if (strstr ((const char *) iter_key, "loc") != NULL) {
					char **locdata;
					int nlocdata;
					ESummaryWeatherLocation *location;

					gnome_config_make_vector (iter_val,
								  &nlocdata,
								  &locdata);

					if (nlocdata != 4) {
						g_warning ("Invalid location in Locations file: %s\n", iter_val);
						g_free (iter_key);
						g_free (iter_val);
						continue;
					}

					if (!g_hash_table_lookup (locations_hash, locdata[1])) {
						location = weather_location_new (locdata);
						g_hash_table_insert (locations_hash,
								     g_strdup (locdata[1]),
								     location);
					}
					
					g_strfreev (locdata);
				}
				
				g_free (iter_key);
				g_free (iter_val);
			}
			
			g_free (state_name);
			g_free (state_path);
			g_free (state_name_key);
		}

		g_strfreev (states);
		g_free (region_name);
		g_free (region_name_key);
		g_free (states_key);
	}

	g_strfreev (regions);
	gnome_config_pop_prefix ();

	return TRUE;
}

static void
e_summary_weather_protocol (ESummary *summary,
			    const char *uri,
			    void *closure)
{
}

static int
e_summary_weather_count (ESummary *summary,
			 void *data)
{
	ESummaryWeather *weather;
	GList *p;
	int count = 0;

	weather = summary->weather;
	for (p = weather->weathers; p; p = p->next) {
		Weather *w = p->data;

		if (w->message != NULL) {
			count++;
		}
	}

	return count;
}

static ESummaryConnectionData *
make_connection (Weather *w)
{
	ESummaryConnectionData *data;

	data = g_new (ESummaryConnectionData, 1);
	data->hostname = make_url (w->location);
	data->type = g_strdup (_("Weather"));

	return data;
}

static GList *
e_summary_weather_add (ESummary *summary,
		       void *data)
{
	ESummaryWeather *weather;
	GList *p, *connections = NULL;

	weather = summary->weather;
	for (p = weather->weathers; p; p = p->next) {
		Weather *w = p->data;

		if (w->message != NULL) {
			ESummaryConnectionData *d;

			d = make_connection (w);
			connections = g_list_prepend (connections, d);
		}
	}

	return connections;
}

static void
e_summary_weather_set_online (ESummary *summary,
			      GNOME_Evolution_OfflineProgressListener listener,
			      gboolean online,
			      void *data)
{
	ESummaryWeather *weather;
	GList *p;

	weather = summary->weather;
	if (weather->online == online) {
		return;
	}

	if (online == TRUE) {
		e_summary_weather_update (summary);

		if (summary->preferences->weather_refresh_time != 0)
			weather->timeout = gtk_timeout_add (summary->preferences->weather_refresh_time * 1000,
							    (GtkFunction) e_summary_weather_update,
							    summary);
	} else {
		for (p = weather->weathers; p; p = p->next) {
			Weather *w;

			w = p->data;
			if (w->message) {
				soup_message_cancel (w->message);
				w->message = NULL;
			}
		}

		gtk_timeout_remove (weather->timeout);
		weather->timeout = 0;
	}

	weather->online = online;
}

void
e_summary_weather_init (ESummary *summary)
{
	ESummaryPrefs *prefs;
	ESummaryWeather *weather;
	ESummaryConnection *connection;
	int timeout;

	g_return_if_fail (summary != NULL);
	g_return_if_fail (IS_E_SUMMARY (summary));

	if (e_summary_weather_init_locations () == FALSE) {
		return;
	}

	prefs = summary->preferences;
	weather = g_new0 (ESummaryWeather, 1);
	weather->online = TRUE;
	summary->weather = weather;

	connection = g_new (ESummaryConnection, 1);
	connection->count = e_summary_weather_count;
	connection->add = e_summary_weather_add;
	connection->set_online = e_summary_weather_set_online;
	connection->closure = NULL;
	connection->callback = NULL;
	connection->callback_closure = NULL;

	weather->connection = connection;
	e_summary_add_online_connection (summary, connection);

	e_summary_add_protocol_listener (summary, "weather", e_summary_weather_protocol, weather);

	if (prefs == NULL) {
		/* translators: Put here a list of codes for locations you want to
		   see in My Evolution by default. You can find the list of all
		   stations and their codes in Evolution sources
		   (evolution/my-evolution/Locations) */
		char *default_stations = _("KBOS"), **stations_v, **p;

		stations_v = g_strsplit (default_stations, ":", 0);
		g_assert (stations_v != NULL);
		for (p = stations_v; *p != NULL; p++) {
			e_summary_weather_add_location (summary, *p);
		}
		g_strfreev (stations_v);
		timeout = 600;
	} else {
		GSList *p;

		for (p = prefs->stations; p; p = p->next) {
			e_summary_weather_add_location (summary, p->data);
		}
		timeout = prefs->weather_refresh_time;
	}

	e_summary_weather_update (summary);

	if (timeout == 0)
		weather->timeout = 0;
	else 
		weather->timeout = gtk_timeout_add (timeout * 1000, 
						    (GtkFunction) e_summary_weather_update,
						    summary);
	return;
}

const char *
e_summary_weather_code_to_name (const char *code)
{
	ESummaryWeatherLocation *location;

	if (locations_hash == NULL) {
		if (e_summary_weather_init_locations () == FALSE) {
			return code;
		}
	}

	location = g_hash_table_lookup (locations_hash, code);
	if (location == NULL) {
		return code;
	} else {
		return location->name;
	}
}

static gboolean
is_weather_shown (const char *code)
{
	GSList *p;
	ESummaryPrefs *global_preferences;

	global_preferences = e_summary_preferences_get_global ();
	for (p = global_preferences->stations; p; p = p->next) {
		if (strcmp (p->data, code) == 0) {
			return TRUE;
		}
	}

	return FALSE;
}

void
e_summary_weather_fill_etable (ESummaryShown *ess)
{
	ETreePath region, state, location;
	ESummaryShownModelEntry *entry;
	int nregions, iregions;
	char **regions;

	gnome_config_push_prefix ("=" LOCATIONDIR "/Locations=/");

	gnome_config_get_vector ("Main/regions", &nregions, &regions);
	region = NULL;

	e_summary_shown_freeze (ess);
	for (iregions = nregions - 1; iregions >= 0; iregions--) {
		int nstates, istates;
		char **states;
		char *region_name;
		char *region_name_key;
		char *states_key;

		region_name_key = g_strconcat (regions[iregions], "/name", NULL);
		states_key = g_strconcat (regions[iregions], "/states", NULL);
		region_name = gnome_config_get_string (region_name_key);

		entry = g_new (ESummaryShownModelEntry, 1);
		entry->location = NULL;
		entry->name = g_strdup (region_name);
		entry->showable = FALSE;
		
		region = e_summary_shown_add_node (ess, TRUE, entry, NULL, FALSE, NULL);

		gnome_config_get_vector (states_key, &nstates, &states);

		state = NULL;
		for (istates = 0; istates < nstates; istates++) {
			void *iter;
			char *iter_key, *iter_val;
			char *state_path, *state_name_key, *state_name;

			state_path = g_strconcat (regions[iregions], "_", states[istates], "/", NULL);
			state_name_key = g_strconcat (state_path, "name", NULL);
			state_name = gnome_config_get_string (state_name_key);

			entry = g_new (ESummaryShownModelEntry, 1);
			entry->location = NULL;
			entry->name = g_strdup (state_name);
			entry->showable = FALSE;

			state = e_summary_shown_add_node (ess, TRUE, entry, region, FALSE, NULL);

			location = NULL;
			iter = gnome_config_init_iterator (state_path);

			while ((iter = gnome_config_iterator_next (iter, &iter_key, &iter_val)) != NULL) {
				if (strncmp (iter_key, "loc", 3) == 0) {
					char **locdata;
					int nlocdata;
					
					gnome_config_make_vector (iter_val,
								  &nlocdata,
								  &locdata);
					if (nlocdata != 4) {
						g_warning ("Invalid location in Locations file: %s\n", iter_val);
						g_free (iter_key);
						g_free (iter_val);
						continue;
					}
					
					entry = g_new (ESummaryShownModelEntry, 1);
					entry->location = g_strdup (locdata[1]);
					entry->name = g_strdup (locdata[0]);
					entry->showable = TRUE;

					location = e_summary_shown_add_node (ess, TRUE, entry, state, FALSE, NULL);
					if (is_weather_shown (locdata[1]) == TRUE) {
						entry = g_new (ESummaryShownModelEntry, 1);
						entry->location = g_strdup (locdata[1]);
						entry->name = g_strdup (locdata[0]);
						location = e_summary_shown_add_node (ess, FALSE, entry, NULL, FALSE, NULL);
					}
					g_strfreev (locdata);
				}

				g_free (iter_key);
				g_free (iter_val);
			}

			g_free (state_name);
			g_free (state_path);
			g_free (state_name_key);
		}

		g_strfreev (states);
		g_free (region_name);
		g_free (region_name_key);
		g_free (states_key);
	}

	g_strfreev (regions);
	gnome_config_pop_prefix ();
	e_summary_shown_thaw (ess);

	return;
}

void
e_summary_weather_reconfigure (ESummary *summary)
{
	ESummaryWeather *weather;
	GList *old;
	GList *weather_list;
	GSList *p;

	g_return_if_fail (summary != NULL);
	g_return_if_fail (IS_E_SUMMARY (summary));

	weather = summary->weather;

	/* Stop timeout so it doesn't occur while we're changing stuff*/
	gtk_timeout_remove (weather->timeout);

	/* Clear the weather list before doing weather_free() on each of them
	   because otherwise soup_message_cancel() could invoke the refresh
	   function just while we are freeing things.  [#31639]  */
	weather_list = weather->weathers;
	weather->weathers = NULL;

	for (old = weather_list; old != NULL; old = old->next) {
		Weather *w;

		w = old->data;
		weather_free (w);
	}
	g_list_free (weather_list);

	for (p = summary->preferences->stations; p; p = p->next) {
		e_summary_weather_add_location (summary, p->data);
	}

	if (summary->preferences->weather_refresh_time == 0)
		weather->timeout = 0;
	else
		weather->timeout = gtk_timeout_add (summary->preferences->weather_refresh_time * 1000, 
						    (GtkFunction) e_summary_weather_update, summary);

	e_summary_weather_update (summary);
}

void
e_summary_weather_free (ESummary *summary)
{
	ESummaryWeather *weather;
	GList *p;

	g_return_if_fail (summary != NULL);
	g_return_if_fail (IS_E_SUMMARY (summary));

	weather = summary->weather;

	if (weather->timeout != 0) {
		gtk_timeout_remove (weather->timeout);
	}
	for (p = weather->weathers; p; p = p->next) {
		Weather *w = p->data;

		weather_free (w);
	}
	g_list_free (weather->weathers);
	g_free (weather->html);

	e_summary_remove_online_connection (summary, weather->connection);
	g_free (weather->connection);

	g_free (weather);
	summary->weather = NULL;
}
	
