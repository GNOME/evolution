/*
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
 *		David Trowbridge <trowbrds@cs.colorado.edu>
 *		Gary Ekker <gekker@novell.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#include "evolution-config.h"

#include "publish-location.h"

#include <string.h>
#include <libxml/tree.h>

#include "e-util/e-util.h"

static EPublishUri *
migrateURI (const gchar *xml,
            xmlDocPtr doc)
{
	GSettings *settings;
	GSList *events = NULL;
	gchar **set_uris;
	GPtrArray *uris_array;
	xmlChar *location, *enabled, *frequency, *username;
	xmlNodePtr root, p;
	EPublishUri *uri;
	gchar *password, *temp;
	GUri *guri;
	gint ii;
	gboolean found = FALSE;

	uri = g_new0 (EPublishUri, 1);

	root = doc->children;
	location = xmlGetProp (root, (const guchar *)"location");
	enabled = xmlGetProp (root, (const guchar *)"enabled");
	frequency = xmlGetProp (root, (const guchar *)"frequency");
	username = xmlGetProp (root, (const guchar *)"username");

	guri = g_uri_parse ((const gchar *) location, SOUP_HTTP_URI_FLAGS | G_URI_FLAGS_PARSE_RELAXED, NULL);

	if (guri == NULL) {
		g_warning ("Could not form the uri for %s \n", location);
		goto cleanup;
	}

	e_util_change_uri_component (&guri, SOUP_URI_USER, (const gchar *) username);

	temp = g_uri_to_string_partial (guri, G_URI_HIDE_PASSWORD);
	uri->location = g_strdup_printf ("dav://%s", strstr (temp, "//") + 2);
	g_free (temp);

	g_uri_unref (guri);

	if (enabled != NULL)
		uri->enabled = atoi ((gchar *) enabled);
	if (frequency != NULL)
		uri->publish_frequency = atoi ((gchar *) frequency);
	uri->publish_format = URI_PUBLISH_AS_FB;

	password = e_passwords_get_password ((gchar *) location);
	if (password) {
		e_passwords_forget_password ((gchar *) location);
		e_passwords_add_password (uri->location, password);
		e_passwords_remember_password (uri->location);
	}

	for (p = root->children; p != NULL; p = p->next) {
		xmlChar *uid = xmlGetProp (p, (const guchar *)"uid");
		if (strcmp ((gchar *) p->name, "source") == 0) {
			events = g_slist_append (events, uid);
		} else {
			g_free (uid);
		}
	}
	uri->events = events;

	uris_array = g_ptr_array_new_full (3, g_free);

	settings = e_util_ref_settings (PC_SETTINGS_ID);
	set_uris = g_settings_get_strv (settings, PC_SETTINGS_URIS);

	for (ii = 0; set_uris && set_uris[ii]; ii++) {
		const gchar *str = set_uris[ii];
		if (!found && g_str_equal (xml, str)) {
			found = TRUE;
			g_ptr_array_add (uris_array, e_publish_uri_to_xml (uri));
		} else {
			g_ptr_array_add (uris_array, g_strdup (str));
		}
	}

	g_strfreev (set_uris);

	/* this should not happen, right? */
	if (!found)
		g_ptr_array_add (uris_array, e_publish_uri_to_xml (uri));
	g_ptr_array_add (uris_array, NULL);

	g_settings_set_strv (settings, PC_SETTINGS_URIS, (const gchar * const *) uris_array->pdata);

	g_ptr_array_free (uris_array, TRUE);
	g_object_unref (settings);

cleanup:
	xmlFree (location);
	xmlFree (enabled);
	xmlFree (frequency);
	xmlFree (username);
	xmlFreeDoc (doc);

	return uri;
}

EPublishUri *
e_publish_uri_from_xml (const gchar *xml)
{
	xmlDocPtr doc;
	xmlNodePtr root, p;
	xmlChar *location, *enabled, *frequency, *fb_duration_value, *fb_duration_type;
	xmlChar *publish_time, *format, *username = NULL;
	GSList *events = NULL;
	EPublishUri *uri;

	doc = xmlParseDoc ((const guchar *) xml);
	if (doc == NULL)
		return NULL;

	root = doc->children;
	if (strcmp ((gchar *) root->name, "uri") != 0)
		return NULL;

	if ((username = xmlGetProp (root, (const guchar *)"username"))) {
		xmlFree (username);
		return migrateURI (xml, doc);

	}

	uri = g_new0 (EPublishUri, 1);

	location = xmlGetProp (root, (const guchar *)"location");
	enabled = xmlGetProp (root, (const guchar *)"enabled");
	frequency = xmlGetProp (root, (const guchar *)"frequency");
	format = xmlGetProp (root, (const guchar *)"format");
	publish_time = xmlGetProp (root, (const guchar *)"publish_time");
	fb_duration_value = xmlGetProp (root, (xmlChar *)"fb_duration_value");
	fb_duration_type = xmlGetProp (root, (xmlChar *)"fb_duration_type");

	if (location != NULL)
		uri->location = (gchar *) location;
	if (enabled != NULL)
		uri->enabled = atoi ((gchar *) enabled);
	if (frequency != NULL)
		uri->publish_frequency = atoi ((gchar *) frequency);
	if (format != NULL)
		uri->publish_format = atoi ((gchar *) format);
	if (publish_time != NULL)
		uri->last_pub_time = (gchar *) publish_time;

	if (fb_duration_value)
		uri->fb_duration_value = atoi ((gchar *) fb_duration_value);
	else
		uri->fb_duration_value = -1;

	if (uri->fb_duration_value < 1)
		uri->fb_duration_value = 6;
	else if (uri->fb_duration_value > 100)
		uri->fb_duration_value = 100;

	if (fb_duration_type && g_str_equal ((gchar *) fb_duration_type, "days"))
		uri->fb_duration_type = FB_DURATION_DAYS;
	else if (fb_duration_type && g_str_equal ((gchar *) fb_duration_type, "months"))
		uri->fb_duration_type = FB_DURATION_MONTHS;
	else
		uri->fb_duration_type = FB_DURATION_WEEKS;

	uri->password = g_strdup ("");

	for (p = root->children; p != NULL; p = p->next) {
		xmlChar *uid = xmlGetProp (p, (const guchar *)"uid");
		if (strcmp ((gchar *) p->name, "event") == 0) {
			events = g_slist_append (events, uid);
		} else {
			g_free (uid);
		}
	}
	uri->events = events;

	xmlFree (enabled);
	xmlFree (frequency);
	xmlFree (format);
	xmlFree (fb_duration_value);
	xmlFree (fb_duration_type);
	xmlFreeDoc (doc);

	return uri;
}

gchar *
e_publish_uri_to_xml (EPublishUri *uri)
{
	xmlDocPtr doc;
	xmlNodePtr root;
	gchar *enabled, *frequency, *format;
	GSList *calendars = NULL;
	xmlChar *xml_buffer;
	gchar *returned_buffer;
	gint xml_buffer_size;

	g_return_val_if_fail (uri != NULL, NULL);
	g_return_val_if_fail (uri->location != NULL, NULL);

	doc = xmlNewDoc ((const guchar *)"1.0");

	root = xmlNewDocNode (doc, NULL, (const guchar *)"uri", NULL);
	enabled = g_strdup_printf ("%d", uri->enabled);
	frequency = g_strdup_printf ("%d", uri->publish_frequency);
	format = g_strdup_printf ("%d", uri->publish_format);
	xmlSetProp (root, (const guchar *)"location", (guchar *) uri->location);
	xmlSetProp (root, (const guchar *)"enabled", (guchar *) enabled);
	xmlSetProp (root, (const guchar *)"frequency", (guchar *) frequency);
	xmlSetProp (root, (const guchar *)"format", (guchar *) format);
	xmlSetProp (root, (const guchar *)"publish_time", (guchar *) uri->last_pub_time);

	g_free (format);
	format = g_strdup_printf ("%d", uri->fb_duration_value);
	xmlSetProp (root, (xmlChar *)"fb_duration_value", (xmlChar *) format);

	if (uri->fb_duration_type == FB_DURATION_DAYS)
		xmlSetProp (root, (xmlChar *)"fb_duration_type", (xmlChar *)"days");
	else if (uri->fb_duration_type == FB_DURATION_MONTHS)
		xmlSetProp (root, (xmlChar *)"fb_duration_type", (xmlChar *)"months");
	else
		xmlSetProp (root, (xmlChar *)"fb_duration_type", (xmlChar *)"weeks");

	for (calendars = uri->events; calendars != NULL;
		calendars = g_slist_next (calendars)) {
		xmlNodePtr node;
		node = xmlNewChild (root, NULL, (const guchar *)"event", NULL);
		xmlSetProp (node, (const guchar *)"uid", calendars->data);
	}
	xmlDocSetRootElement (doc, root);

	xmlDocDumpMemory (doc, &xml_buffer, &xml_buffer_size);
	xmlFreeDoc (doc);

	returned_buffer = g_malloc (xml_buffer_size + 1);
	memcpy (returned_buffer, xml_buffer, xml_buffer_size);
	returned_buffer[xml_buffer_size] = '\0';
	xmlFree (xml_buffer);
	g_free (enabled);
	g_free (frequency);
	g_free (format);

	return returned_buffer;
}
