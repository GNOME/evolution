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

#include <string.h> /* memset(), strcmp() */
#include <stdlib.h>
#include <gnome-xml/parser.h>
#include <libgnome/gnome-defs.h>
#include <libgnome/gnome-util.h>

#include "e-pilot-map.h"

typedef struct 
{
	char *uid;
	gboolean archived;
} EPilotMapPidNode;

typedef struct
{
	guint32 pid;
	gboolean archived;
} EPilotMapUidNode;


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
	EPilotMap *map = (EPilotMap *)data;

	if (!strcmp (name, "PilotMap")) {
		while (attrs && *attrs != NULL) {
			const xmlChar **val = attrs;
			
			val++;
			if (!strcmp (*attrs, "timestamp")) 
				map->since = (time_t)strtoul (*val, NULL, 0);

			attrs = ++val;
		}
	}
	 
	if (!strcmp (name, "map")) {
		const char *uid = NULL;
		guint32 pid = 0;
		gboolean archived = FALSE;

		while (attrs && *attrs != NULL) {
			const xmlChar **val = attrs;
			
			val++;
			if (!strcmp (*attrs, "uid")) 
				uid = *val;
			
			if (!strcmp (*attrs, "pilot_id"))
				pid = strtoul (*val, NULL, 0);

			if (!strcmp (*attrs, "archived"))
				archived = strtoul (*val, NULL, 0)== 1 ? TRUE : FALSE;
				
			attrs = ++val;
		}
			
		if (uid && pid != 0)
			e_pilot_map_insert (map, pid, uid, archived);
	}
}

static void
map_write_foreach (gpointer key, gpointer value, gpointer data)
{
	xmlNodePtr root = data;
	xmlNodePtr mnode;
	unsigned long *pid = key;
	EPilotMapPidNode *pnode = value;
	char *pidstr;

	mnode = xmlNewChild (root, NULL, "map", NULL);
	
	pidstr = g_strdup_printf ("%lu", *pid);
	xmlSetProp (mnode, "pilot_id", pidstr);
	g_free (pidstr);

	xmlSetProp (mnode, "uid", pnode->uid);

	if (pnode->archived)
		xmlSetProp (mnode, "archived", "1");
	else
		xmlSetProp (mnode, "archived", "0");
}

gboolean 
e_pilot_map_pid_is_archived (EPilotMap *map, guint32 pid)
{
	EPilotMapPidNode *pnode;
	
	pnode = g_hash_table_lookup (map->pid_map, &pid);

	if (pnode == NULL)
		return FALSE;
	
	return pnode->archived;
}

gboolean 
e_pilot_map_uid_is_archived (EPilotMap *map, const char *uid)
{
	EPilotMapUidNode *unode;
	
	unode = g_hash_table_lookup (map->uid_map, uid);

	if (unode == NULL)
		return FALSE;
	
	return unode->archived;
}

void 
e_pilot_map_insert (EPilotMap *map, guint32 pid, const char *uid, gboolean archived)
{
	char *new_uid;
	guint32 *new_pid = g_new (guint32, 1);
	EPilotMapPidNode *pnode = g_new0 (EPilotMapPidNode, 1);
	EPilotMapUidNode *unode = g_new0 (EPilotMapUidNode, 1);
	
	*new_pid = pid;
	new_uid = g_strdup (uid);

	pnode->uid = new_uid;
	pnode->archived = archived;
	
	unode->pid = pid;
	unode->archived = archived;
	
	g_hash_table_insert (map->pid_map, new_pid, pnode);
	g_hash_table_insert (map->uid_map, new_uid, unode);
}

void 
e_pilot_map_remove_by_pid (EPilotMap *map, guint32 pid)
{	
	EPilotMapPidNode *pnode;
	EPilotMapUidNode *unode;

	pnode = g_hash_table_lookup (map->pid_map, &pid);
	if (!pnode)
		return;
	
	unode = g_hash_table_lookup (map->uid_map, pnode->uid);

	g_hash_table_remove (map->pid_map, &pid);
	g_hash_table_remove (map->uid_map, pnode->uid);

	g_free (pnode);
	g_free (unode);
}

void 
e_pilot_map_remove_by_uid (EPilotMap *map, const char *uid)
{
	EPilotMapPidNode *pnode;
	EPilotMapUidNode *unode;

	unode = g_hash_table_lookup (map->uid_map, uid);
	if (!unode)
		return;
	
	pnode = g_hash_table_lookup (map->pid_map, &unode->pid);

	g_hash_table_remove (map->uid_map, uid);
	g_hash_table_remove (map->pid_map, &unode->pid);

	g_free (unode);
	g_free (pnode);
}


guint32 
e_pilot_map_lookup_pid (EPilotMap *map, const char *uid) 
{
	EPilotMapUidNode *unode = NULL;
	
	unode = g_hash_table_lookup (map->uid_map, uid);

	if (unode == NULL)
		return 0;
	
	return unode->pid;
}

const char *
e_pilot_map_lookup_uid (EPilotMap *map, guint32 pid)
{
	EPilotMapPidNode *pnode = NULL;
	
	pnode = g_hash_table_lookup (map->pid_map, &pid);

	if (pnode == NULL)
		return NULL;
	
	return pnode->uid;
}

int 
e_pilot_map_read (const char *filename, EPilotMap **map)
{
	xmlSAXHandler handler;
	EPilotMap *new_map = g_new0 (EPilotMap, 1);

	*map = NULL;
	
	memset (&handler, 0, sizeof (xmlSAXHandler));
	handler.startElement = map_sax_start_element;

	new_map->pid_map = g_hash_table_new (g_int_hash, g_int_equal);
	new_map->uid_map = g_hash_table_new (g_str_hash, g_str_equal);

	if (g_file_exists (filename)) {
		if (xmlSAXUserParseFile (&handler, new_map, filename) < 0) {
			g_free (new_map);
			return -1;
		}
	}
	
	*map = new_map;
	
	return 0;
}
		
int
e_pilot_map_write (const char *filename, EPilotMap *map)
{
	xmlDocPtr doc;
	int ret;
	
	g_return_val_if_fail (map != NULL, -1);
	
	doc = xmlNewDoc ("1.0");
	if (doc == NULL) {
		g_warning ("Pilot map file could not be created\n");
		return -1;
	}
	doc->root = xmlNewDocNode(doc, NULL, "PilotMap", NULL);
	map->since = time (NULL);
	map_set_node_timet (doc->root, "timestamp", map->since);

	g_hash_table_foreach (map->pid_map, map_write_foreach, doc->root);
	
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

void 
e_pilot_map_destroy (EPilotMap *map)
{
	g_hash_table_destroy (map->pid_map);
	g_hash_table_destroy (map->uid_map);
	g_free (map);
}








