/*
 * Authors:
 *  David Trowbridge <trowbrds@cs.colorado.edu>
 *  Gary Ekker <gekker@novell.com>
 *
 * Copyright (C) 2005 Novell, Inc (www.novell.com)
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 *
 */

#include "publish-location.h"
#include <libxml/tree.h>
#include <gconf/gconf-client.h>
#include <libgnomevfs/gnome-vfs.h>
#include <libedataserverui/e-passwords.h>
#include <string.h>

static EPublishUri *
migrateURI (const gchar *xml, xmlDocPtr doc)
{
	GConfClient *client;
	GSList *uris, *l, *events = NULL;
	xmlChar *location, *enabled, *frequency, *username;
	xmlNodePtr root, p;
	EPublishUri *uri;
	GnomeVFSURI *vfs_uri;
	gchar *password, *temp;

	client = gconf_client_get_default ();
	uris = gconf_client_get_list (client, "/apps/evolution/calendar/publish/uris", GCONF_VALUE_STRING, NULL);
	l = uris;
	while (l && l->data) {
		gchar *str = l->data;
		if (strcmp (xml, str) == 0) {
			uris = g_slist_remove (uris, str);
			g_free (str);
		}
		l = g_slist_next (l);
	}

	uri = g_new0 (EPublishUri, 1);

	root = doc->children;
	location = xmlGetProp (root, "location");
	enabled = xmlGetProp (root, "enabled");
	frequency = xmlGetProp (root, "frequency");
	username = xmlGetProp (root, "username");

	vfs_uri = gnome_vfs_uri_new (location);
	
	if (!vfs_uri) {
		g_warning ("Could not form the uri for %s \n", location);
		goto cleanup;		
	}
	
	gnome_vfs_uri_set_user_name (vfs_uri, username);
	temp = gnome_vfs_uri_to_string (vfs_uri, GNOME_VFS_URI_HIDE_TOPLEVEL_METHOD | GNOME_VFS_URI_HIDE_PASSWORD);
	uri->location = g_strdup_printf ("dav://%s", temp);
	g_free (temp);
	gnome_vfs_uri_unref (vfs_uri);

	if (enabled != NULL)
		uri->enabled = atoi (enabled);
	if (frequency != NULL)
		uri->publish_frequency = atoi (frequency);
	uri->publish_format = URI_PUBLISH_AS_FB;

	password = e_passwords_get_password ("Calendar", location);
	if (password) {
		e_passwords_forget_password ("Calendar", location);
		e_passwords_add_password (uri->location, password);
		e_passwords_remember_password ("Calendar", uri->location);
	}

	for (p = root->children; p != NULL; p = p->next) {
		xmlChar *uid = xmlGetProp (p, "uid");
		if (strcmp (p->name, "source") == 0) {
			events = g_slist_append (events, uid);
		} else {
			g_free (uid);
		}
	}
	uri->events = events;

	uris = g_slist_prepend (uris, e_publish_uri_to_xml (uri));
	gconf_client_set_list (client, "/apps/evolution/calendar/publish/uris", GCONF_VALUE_STRING, uris, NULL);
	g_slist_foreach (uris, (GFunc) g_free, NULL);
	g_slist_free (uris);
	g_object_unref (client);

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
	xmlChar *location, *enabled, *frequency;
	xmlChar *publish_time, *format, *username = NULL;
	GSList *events = NULL;
	EPublishUri *uri;

	doc = xmlParseDoc ((char *) xml);
	if (doc == NULL)
		return NULL;

	root = doc->children;
	if (strcmp (root->name, "uri") != 0)
		return NULL;

	if ((username = xmlGetProp (root, "username"))) {
		xmlFree (username);
		return migrateURI (xml, doc);
		
	}

	uri = g_new0 (EPublishUri, 1);

	location = xmlGetProp (root, "location");
	enabled = xmlGetProp (root, "enabled");
	frequency = xmlGetProp (root, "frequency");
	format = xmlGetProp (root, "format");
	publish_time = xmlGetProp (root, "publish_time");

	if (location != NULL)
		uri->location = location;
	if (enabled != NULL)
		uri->enabled = atoi (enabled);
	if (frequency != NULL)
		uri->publish_frequency = atoi (frequency);
	if (format != NULL)
		uri->publish_format = atoi (format);
	if (publish_time != NULL)
		uri->last_pub_time = publish_time;

	uri->password = g_strdup ("");

	for (p = root->children; p != NULL; p = p->next) {
		xmlChar *uid = xmlGetProp (p, "uid");
		if (strcmp (p->name, "event") == 0) {
			events = g_slist_append (events, uid);
		} else {
			g_free (uid);
		}
	}
	uri->events = events;

	xmlFree (enabled);
	xmlFree (frequency);
	xmlFree (format);
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
	char *returned_buffer;
	int xml_buffer_size;

	g_return_val_if_fail (uri != NULL, NULL);
	g_return_val_if_fail (uri->location != NULL, NULL);

	doc = xmlNewDoc ("1.0");

	root = xmlNewDocNode (doc, NULL, "uri", NULL);
	enabled = g_strdup_printf ("%d", uri->enabled);
	frequency = g_strdup_printf ("%d", uri->publish_frequency);
	format = g_strdup_printf ("%d", uri->publish_format);
	xmlSetProp (root, "location", uri->location);
	xmlSetProp (root, "enabled", enabled);
	xmlSetProp (root, "frequency", frequency);
	xmlSetProp (root, "format", format);
	xmlSetProp (root, "publish_time", uri->last_pub_time);

	for (calendars = uri->events; calendars != NULL; calendars = g_slist_next (calendars)) {
		xmlNodePtr node;
		node = xmlNewChild (root, NULL, "event", NULL);
		xmlSetProp (node, "uid", calendars->data);
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
