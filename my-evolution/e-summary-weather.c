/*
 * e-summary-weather.c
 *
 * Copyright (C) 2001  Ximian, Inc.
 *
 * Authors:  Iain Holmes  <iain@ximian.com>
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <glib.h>
#include <libgnome/gnome-defs.h>
#include <libgnome/gnome-i18n.h>
#include <libgnome/gnome-config.h>

#include <gal/widgets/e-unicode.h>

#include <libgnomevfs/gnome-vfs.h>
#include "e-summary.h"
#include "e-summary-weather.h"
#include "weather.h"
#include "metar.h"

struct _ESummaryWeather {
	GList *weathers;

	char *html;
};

static GHashTable *locations_hash = NULL;

char *
e_summary_weather_get_html (ESummary *summary)
{
	GList *weathers;
	GString *string;
	char *html;

	if (summary->weather == NULL) {
		return NULL;
	}

	string = g_string_new ("<dl><img src=\"ico-weather.png\" align=\"middle\" "
			       "alt=\"\" width=\"48\" height=\"48\"><b>"
			       "<a href=\"http://www.metoffice.gov.uk\">My Weather</a></b>");
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
make_url (const char *name,
	  const char *code)
{
	return g_strdup_printf ("<a href=\"http://weather.noaa.gov/cgi-bin/mgetmetar.pl?cccc=%s\">%s</a>", code, name);
}

static void
weather_make_html (Weather *w)
{
	GString *string;
	ESummaryWeatherLocation *location;
	char *sky, *temp, *cond, *uri, *url;

	string = g_string_new ("<dd><img align=\"middle\" "
			       "src=\"es-weather.png\">&#160;<b>");
	location = g_hash_table_lookup (locations_hash, w->location);
	if (location == NULL) {
		url = make_url (w->location, w->location);
	} else {
		url = make_url (location->name, w->location);
	}

	g_string_append (string, url);
	g_free (url);

	g_string_append (string, "</b>:<blockquote><font size=\"-1\">");
	sky = (char *) weather_sky_string (w);
	temp = weather_temp_string (w);
	cond = (char *) weather_conditions_string (w);

	g_string_append (string, e_utf8_from_locale_string (sky));
	g_string_append (string, " ");
	g_string_append (string, e_utf8_from_locale_string (cond));
	g_string_append (string, " ");
	g_string_append (string, e_utf8_from_locale_string (temp));
	g_free (temp);

	g_string_append (string, "<font size=\"-1\">");
	
	uri = g_strdup_printf ("<a href=\"weather://%p\">", w);
	g_string_append (string, uri);
	g_free (uri);
	g_string_append (string, "(More)</a></font></font></blockquote></dd>");

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
close_callback (GnomeVFSAsyncHandle *handle,
		GnomeVFSResult result,
		Weather *w)
{
	char *html, *metar, *end;
	char *search_str;

	if (w->handle == NULL) {
		g_free (w->buffer);
		g_string_free (w->string, TRUE);
		return;
	}

	w->handle = NULL;
	g_free (w->buffer);
	html = w->string->str;
	g_string_free (w->string, FALSE);

	/* Find the metar data */
	search_str = g_strdup_printf ("\n%s", w->location);
	metar = strstr (html, search_str);
	if (metar == NULL) {
		g_free (search_str);
		g_free (html);
		return;
	}

	metar++;
	end = strchr (metar, '\n');
	if (end == NULL) {
		g_free (search_str);
		g_free (html);
		return;
	}
	*end = '\0';

	g_warning ("Parsing %s", metar);

	parse_metar (metar, w);
	g_free (html);
	g_free (search_str);
	return;
}

static void
read_callback (GnomeVFSAsyncHandle *handle,
	       GnomeVFSResult result,
	       gpointer buffer,
	       GnomeVFSFileSize bytes_requested,
	       GnomeVFSFileSize bytes_read,
	       Weather *w)
{
	if (result != GNOME_VFS_OK && result != GNOME_VFS_ERROR_EOF) {
		w->html = g_strdup ("<b>Error downloading RDF</b>");

		e_summary_draw (w->summary);
		w->handle = NULL;
		gnome_vfs_async_close (handle, 
				       (GnomeVFSAsyncCloseCallback) close_callback, w);
		return;
	}

	if (bytes_read == 0) {
		gnome_vfs_async_close (handle,
				       (GnomeVFSAsyncCloseCallback) close_callback, w);
	} else {
		*((char *) buffer + bytes_read) = 0;
		g_string_append (w->string, (const char *) buffer);
		gnome_vfs_async_read (handle, buffer, 4095,
				      (GnomeVFSAsyncReadCallback) read_callback, w);
	}
}

static void
open_callback (GnomeVFSAsyncHandle *handle,
	       GnomeVFSResult result,
	       Weather *w)
{
	if (result != GNOME_VFS_OK) {
		w->html = g_strdup ("<b>Error downloading Metar</b>");

		e_summary_draw (w->summary);
		return;
	}

	w->string = g_string_new ("");
	w->buffer = g_new (char, 4096);

	gnome_vfs_async_read (handle, w->buffer, 4095,
			      (GnomeVFSAsyncReadCallback) read_callback, w);
}

static void
e_summary_weather_add_location (ESummary *summary,
				const char *location)
{
	Weather *w;
	char *uri;

	w = g_new0 (Weather, 1);
	w->summary = summary;
	w->location = g_strdup (location);
	summary->weather->weathers = g_list_prepend (summary->weather->weathers, w);

	uri = g_strdup_printf ("http://weather.noaa.gov/cgi-bin/mgetmetar.pl?cccc=%s", location);
	gnome_vfs_async_open (&w->handle, uri, GNOME_VFS_OPEN_READ,
			      (GnomeVFSAsyncOpenCallback) open_callback, w);
	g_free (uri);
}

static gboolean
e_summary_weather_init_locations (void) 
{
	char *key, *path;
	int nregions, iregions;
	char **regions;

	if (locations_hash != NULL) {
		return TRUE;
	}

	locations_hash = g_hash_table_new (g_str_hash, g_str_equal);
	path = g_strdup (EVOLUTION_DATADIR "/evolution/Locations");

	key = g_strdup_printf ("=%s=/", path);
	g_free (path);

	gnome_config_push_prefix (key);
	g_free (key);

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
				if (strstr (iter_key, "loc") != NULL) {
					char **locdata;
					int nlocdata;
					ESummaryWeatherLocation *location;

					gnome_config_make_vector (iter_val,
								  &nlocdata,
								  &locdata);
					g_return_val_if_fail (nlocdata == 4, FALSE);

					location = weather_location_new (locdata);
					g_hash_table_insert (locations_hash,
							     g_strdup (locdata[1]),
							     location);

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

void
e_summary_weather_init (ESummary *summary)
{
	ESummaryWeather *weather;

	g_return_if_fail (summary != NULL);
	g_return_if_fail (IS_E_SUMMARY (summary));

	if (e_summary_weather_init_locations () == FALSE) {
		return;
	}

	weather = g_new0 (ESummaryWeather, 1);
	summary->weather = weather;

	e_summary_add_protocol_listener (summary, "weather", e_summary_weather_protocol, weather);

	e_summary_weather_add_location (summary, "ENBR");
	e_summary_weather_add_location (summary, "EGAC");
	e_summary_weather_add_location (summary, "EGAA");
	return;
}
