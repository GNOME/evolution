/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* Evolution Conduits - Pilot Map routines
 *
 * Copyright (C) 2000 Ximian, Inc.
 *
 * Authors: JP Rosevear <jpr@ximian.com>
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

#include "e-pilot-map.h"

#include <string.h> /* memset(), strcmp() */
#include <stdlib.h>
#include <glib.h>
#include <gnome-xml/parser.h>
#include <libgnome/gnome-defs.h>
#include <libgnome/gnome-util.h>

typedef struct 
{
	char *uid;
	gboolean archived;
	gboolean touched;
} EPilotMapPidNode;

typedef struct
{
	guint32 pid;
	gboolean archived;
	gboolean touched;
} EPilotMapUidNode;

typedef struct
{
	gboolean touched_only;
	xmlNodePtr root;
} EPilotMapWriteData;

static void
real_e_pilot_map_insert (EPilotMap *map, guint32 pid, const char *uid, gboolean archived, gboolean touch)
{
	char *new_uid;
	guint32 *new_pid;
	EPilotMapPidNode *pnode;
	EPilotMapUidNode *unode;
	gpointer key, value;

	g_return_if_fail (map != NULL);
	g_return_if_fail (uid != NULL);

	new_pid = g_new (guint32, 1);
	*new_pid = pid;

	new_uid = g_strdup (uid);

	pnode = g_new0 (EPilotMapPidNode, 1);
	pnode->uid = new_uid;
	pnode->archived = archived;
	if (touch)
		pnode->touched = TRUE;
	
	unode = g_new0 (EPilotMapUidNode, 1);
	unode->pid = pid;
	unode->archived = archived;
	if (touch)
		unode->touched = TRUE;
	
	if (g_hash_table_lookup_extended (map->pid_map, new_pid, &key, &value)) {
		g_hash_table_remove (map->pid_map, new_pid);
		g_free (key);
		g_free (value);
	}
	if (g_hash_table_lookup_extended (map->uid_map, new_uid, &key, &value)) {
		g_hash_table_remove (map->uid_map, new_uid);
		g_free (key);
		g_free (value);
	}
	
	g_hash_table_insert (map->pid_map, new_pid, pnode);
	g_hash_table_insert (map->uid_map, new_uid, unode);
}

static void
map_set_node_timet (xmlNodePtr node, const char *name, time_t t)
{
	char *tstring;
	
	tstring = g_strdup_printf ("%ld", t);
	xmlSetProp (node, name, tstring);
	g_free (tstring);
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

		g_assert (uid != NULL);
		g_assert (pid != 0 || archived);

		real_e_pilot_map_insert (map, pid, uid, archived, FALSE);
	}
}

static void
map_write_foreach (gpointer key, gpointer value, gpointer data)
{
	EPilotMapWriteData *wd = data;
	xmlNodePtr root = wd->root;
	char *uid = key;
	EPilotMapUidNode *unode = value;
	xmlNodePtr mnode;

	if (wd->touched_only && !unode->touched)
		return;
	
	mnode = xmlNewChild (root, NULL, "map", NULL);
	xmlSetProp (mnode, "uid", uid);

	if (unode->archived) {
		xmlSetProp (mnode, "archived", "1");
	} else {
		char *pidstr;

		pidstr = g_strdup_printf ("%d", unode->pid);
		xmlSetProp (mnode, "pilot_id", pidstr);
		g_free (pidstr);
		xmlSetProp (mnode, "archived", "0");
	}
}

gboolean 
e_pilot_map_pid_is_archived (EPilotMap *map, guint32 pid)
{
	EPilotMapPidNode *pnode;

	g_return_val_if_fail (map != NULL, FALSE);
	
	pnode = g_hash_table_lookup (map->pid_map, &pid);

	if (pnode == NULL)
		return FALSE;
	
	return pnode->archived;
}

gboolean 
e_pilot_map_uid_is_archived (EPilotMap *map, const char *uid)
{
	EPilotMapUidNode *unode;

	g_return_val_if_fail (map != NULL, FALSE);
	g_return_val_if_fail (uid != NULL, FALSE);
	
	unode = g_hash_table_lookup (map->uid_map, uid);

	if (unode == NULL)
		return FALSE;
	
	return unode->archived;
}

void 
e_pilot_map_insert (EPilotMap *map, guint32 pid, const char *uid, gboolean archived)
{
	real_e_pilot_map_insert (map, pid, uid, archived, TRUE);
}

void 
e_pilot_map_remove_by_pid (EPilotMap *map, guint32 pid)
{
	EPilotMapPidNode *pnode;
	EPilotMapUidNode *unode;

	g_return_if_fail (map != NULL);

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

	g_return_if_fail (map != NULL);
	g_return_if_fail (uid != NULL);

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
e_pilot_map_lookup_pid (EPilotMap *map, const char *uid, gboolean touch) 
{
	EPilotMapUidNode *unode = NULL;

	g_return_val_if_fail (map != NULL, 0);
	g_return_val_if_fail (uid != NULL, 0);
	
	unode = g_hash_table_lookup (map->uid_map, uid);

	if (unode == NULL)
		return 0;
	
	if (touch) {
		EPilotMapPidNode *pnode = NULL;
		
		pnode = g_hash_table_lookup (map->pid_map, &unode->pid);
		if (pnode != NULL)
			pnode->touched = TRUE;
		unode->touched = TRUE;	
	}
	
	return unode->pid;
}

const char *
e_pilot_map_lookup_uid (EPilotMap *map, guint32 pid, gboolean touch)
{
	EPilotMapPidNode *pnode = NULL;

	g_return_val_if_fail (map != NULL, NULL);
	
	pnode = g_hash_table_lookup (map->pid_map, &pid);

	if (pnode == NULL)
		return NULL;
	
	if (touch) {
		EPilotMapUidNode *unode = NULL;
		
		unode = g_hash_table_lookup (map->uid_map, pnode->uid);
		g_assert (unode != NULL);
		
		unode->touched = TRUE;
		pnode->touched = TRUE;
	}
	
	return pnode->uid;
}

int 
e_pilot_map_read (const char *filename, EPilotMap **map)
{
	xmlSAXHandler handler;
	EPilotMap *new_map;

	g_return_val_if_fail (filename != NULL, -1);
	g_return_val_if_fail (map != NULL, -1);

	*map = NULL;
	new_map = g_new0 (EPilotMap, 1);

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

	new_map->write_touched_only = FALSE;
	
	*map = new_map;
	
	return 0;
}
		
int
e_pilot_map_write (const char *filename, EPilotMap *map)
{
	EPilotMapWriteData wd;
	xmlDocPtr doc;
	int ret;

	g_return_val_if_fail (filename != NULL, -1);
	g_return_val_if_fail (map != NULL, -1);
	
	doc = xmlNewDoc ("1.0");
	if (doc == NULL) {
		g_warning ("Pilot map file could not be created\n");
		return -1;
	}
	doc->root = xmlNewDocNode(doc, NULL, "PilotMap", NULL);
	map->since = time (NULL);
	map_set_node_timet (doc->root, "timestamp", map->since);

	wd.touched_only = map->write_touched_only;
	wd.root = doc->root;
	g_hash_table_foreach (map->uid_map, map_write_foreach, &wd);
	
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

static gboolean
foreach_remove (gpointer key, gpointer value, gpointer data)
{
	g_free (key);
	g_free (value);

	return TRUE;
}

void 
e_pilot_map_destroy (EPilotMap *map)
{
	g_return_if_fail (map != NULL);

	g_hash_table_foreach_remove (map->pid_map, foreach_remove, NULL);
	g_hash_table_foreach_remove (map->uid_map, foreach_remove, NULL);
	
	g_hash_table_destroy (map->pid_map);
	g_hash_table_destroy (map->uid_map);
	g_free (map);
}
