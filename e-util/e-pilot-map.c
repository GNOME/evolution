/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* Evolution Conduits - Pilot Map routines
 *
 * Copyright (C) 2000 Helix Code, Inc.
 *
 * Authors: JP Rosevear <jpr@helixcode.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307, USA.
 */

#include <stdlib.h>
#include <time.h>
#include <glib.h>
#include <gnome-xml/parser.h>
#include <e-pilot-map.h>

struct map_sax_closure 
{
	GHashTable *pid_map;
	GHashTable *uid_map;
	time_t *since;
};

static void
map_set_node_timet (xmlNodePtr node, const char *name, time_t t)
{
	char *tstring;
	
	tstring = g_strdup_printf ("%ld", t);
	xmlSetProp (node, name, tstring);
}

static void
map_sax_start_element (void *data, const xmlChar *name, 
		       const xmlChar **attrs)
{
	struct map_sax_closure *closure = (struct map_sax_closure *)data;

	if (!strcmp (name, "PilotMap")) {
		while (attrs && *attrs != NULL) {
			const xmlChar **val = attrs;
			
			val++;
			if (!strcmp (*attrs, "timestamp")) 
				*closure->since = (time_t)strtoul (*val, NULL, 0);

			attrs = ++val;
		}
	}
	 
	if (!strcmp (name, "map")) {
		char *uid = NULL;
		guint32 *pid = g_new (guint32, 1);

		*pid = 0;

		while (attrs && *attrs != NULL) {
			const xmlChar **val = attrs;
			
			val++;
			if (!strcmp (*attrs, "uid")) 
				uid = g_strdup (*val);
			
			if (!strcmp (*attrs, "pilot_id"))
				*pid = strtoul (*val, NULL, 0);
				
			attrs = ++val;
		}
			
		if (uid && *pid != 0) {
			g_hash_table_insert (closure->pid_map, pid, uid);
			g_hash_table_insert (closure->uid_map, uid, pid);
		} else {
			g_free (pid);
		}
	}
}

static void
map_write_foreach (gpointer key, gpointer value, gpointer data)
{
	xmlNodePtr root = data;
	xmlNodePtr mnode;
	unsigned long *pid = key;
	const char *uid = value;
	char *pidstr;
	
	mnode = xmlNewChild (root, NULL, "map", NULL);
	xmlSetProp (mnode, "uid", uid);
	pidstr = g_strdup_printf ("%lu", *pid);
	xmlSetProp (mnode, "pilot_id", pidstr);
	g_free (pidstr);
}
		
int
e_pilot_map_write (const char *filename, GHashTable *pid_map)
{
	xmlDocPtr doc;
	int ret;
	
	g_return_val_if_fail (pid_map != NULL, -1);
	
	doc = xmlNewDoc ("1.0");
	if (doc == NULL) {
		g_warning ("Pilot map file could not be created\n");
		return -1;
	}
	doc->root = xmlNewDocNode(doc, NULL, "PilotMap", NULL);
	map_set_node_timet (doc->root, "timestamp", time (NULL));

	g_hash_table_foreach (pid_map, map_write_foreach, doc->root);
	
	/* Write the file */
	xmlSetDocCompressMode (doc, 0);
	ret = xmlSaveFile (filename, doc);
	if (ret < 0) {
		g_warning ("Pilot map file '%s' could not be saved\n", filename);
		return -1;
	}
	
	xmlFreeDoc (doc);

	return 0;
}

int 
e_pilot_map_read (const char *filename, GHashTable *pid_map, 
		  GHashTable *uid_map, time_t *since)
{
	xmlSAXHandler handler;
	struct map_sax_closure closure;

	memset (&handler, 0, sizeof (xmlSAXHandler));
	handler.startElement = map_sax_start_element;

	closure.pid_map = pid_map;
	closure.uid_map = uid_map;
	
	if (xmlSAXUserParseFile (&handler, &closure, filename) < 0)
		return -1;

	return 0;
}







