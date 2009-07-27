/*
 * Evolution Conduits - Pilot Map routines
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
 *
 * Authors:
 *		JP Rosevear <jpr@ximian.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#include <string.h>
#include <stdlib.h>

#include <glib.h>
#include <libxml/parser.h>

#include <libedataserver/e-xml-utils.h>

#include "e-pilot-map.h"

typedef struct
{
	gchar *uid;
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
real_e_pilot_map_insert (EPilotMap *map, guint32 pid, const gchar *uid, gboolean archived, gboolean touch)
{
	gchar *new_uid;
	guint32 *new_pid = NULL;
	EPilotMapPidNode *pnode = NULL;
	EPilotMapUidNode *unode;

	g_return_if_fail (map != NULL);
	g_return_if_fail (uid != NULL);

	/* Keys */
	if (pid != 0) {
		new_pid = g_new (guint32, 1);
		*new_pid = pid;
	}
	new_uid = g_strdup (uid);

	/* Values */
	if (pid != 0) {
		pnode = g_new0 (EPilotMapPidNode, 1);
		pnode->uid = new_uid;
		pnode->archived = archived;
		if (touch)
			pnode->touched = TRUE;
	}

	unode = g_new0 (EPilotMapUidNode, 1);
	unode->pid = pid;
	unode->archived = archived;
	if (touch)
		unode->touched = TRUE;

	/* Insertion */
	if (pid != 0)
		g_hash_table_insert (map->pid_map, new_pid, pnode);
	g_hash_table_insert (map->uid_map, new_uid, unode);
}

static void
map_set_node_timet (xmlNodePtr node, const gchar *name, time_t t)
{
	gchar *tstring;

	tstring = g_strdup_printf ("%ld", t);
	xmlSetProp (node, (guchar *)name, (guchar *)tstring);
	g_free (tstring);
}

static void
map_sax_start_element (gpointer data, const xmlChar *name,
		       const xmlChar **attrs)
{
	EPilotMap *map = (EPilotMap *)data;

	if (!strcmp ((gchar *)name, "PilotMap")) {
		while (attrs && *attrs != NULL) {
			const xmlChar **val = attrs;

			val++;
			if (!strcmp ((gchar *)*attrs, "timestamp"))
				map->since = (time_t)strtoul ((gchar *)*val, NULL, 0);

			attrs = ++val;
		}
	}

	if (!strcmp ((gchar *)name, "map")) {
		const gchar *uid = NULL;
		guint32 pid = 0;
		gboolean archived = FALSE;

		while (attrs && *attrs != NULL) {
			const xmlChar **val = attrs;

			val++;
			if (!strcmp ((gchar *)*attrs, "uid"))
				uid = (gchar *)*val;

			if (!strcmp ((gchar *)*attrs, "pilot_id"))
				pid = strtoul ((gchar *)*val, NULL, 0);

			if (!strcmp ((gchar *)*attrs, "archived"))
				archived = strtoul ((gchar *)*val, NULL, 0)== 1 ? TRUE : FALSE;

			attrs = ++val;
		}

		g_return_if_fail (uid != NULL);
		g_return_if_fail (pid != 0 || archived);

		real_e_pilot_map_insert (map, pid, uid, archived, FALSE);
	}
}

static void
map_write_foreach (gpointer key, gpointer value, gpointer data)
{
	EPilotMapWriteData *wd = data;
	xmlNodePtr root = wd->root;
	gchar *uid = key;
	EPilotMapUidNode *unode = value;
	xmlNodePtr mnode;

	if (wd->touched_only && !unode->touched)
		return;

	mnode = xmlNewChild (root, NULL, (const guchar *)"map", NULL);
	xmlSetProp (mnode, (const guchar *)"uid", (guchar *)uid);

	if (unode->archived) {
		xmlSetProp (mnode, (const guchar *)"archived", (const guchar *)"1");
	} else {
		gchar *pidstr;

		pidstr = g_strdup_printf ("%d", unode->pid);
		xmlSetProp (mnode, (const guchar *)"pilot_id", (guchar *)pidstr);
		g_free (pidstr);
		xmlSetProp (mnode, (const guchar *)"archived", (const guchar *)"0");
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
e_pilot_map_uid_is_archived (EPilotMap *map, const gchar *uid)
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
e_pilot_map_insert (EPilotMap *map, guint32 pid, const gchar *uid, gboolean archived)
{
        EPilotMapPidNode *pnode;
        EPilotMapUidNode *unode;

        pnode = g_hash_table_lookup (map->pid_map, &pid);
        if (pnode != NULL) {
		/* In case the pid<->uid mapping is not the same anymore */
                g_hash_table_remove (map->uid_map, pnode->uid);

		g_hash_table_remove (map->pid_map, &pid);
	}

        unode = g_hash_table_lookup (map->uid_map, uid);
        if (unode != NULL) {
		/* In case the pid<->uid mapping is not the same anymore */
                g_hash_table_remove (map->pid_map, &unode->pid);

		g_hash_table_remove (map->uid_map, uid);
	}

	real_e_pilot_map_insert (map, pid, uid, archived, TRUE);
}

void
e_pilot_map_remove_by_pid (EPilotMap *map, guint32 pid)
{
	EPilotMapPidNode *pnode;
	EPilotMapUidNode *unode;

	g_return_if_fail (map != NULL);

        pnode = g_hash_table_lookup (map->pid_map, &pid);
        if (pnode == NULL)
		return;

        unode = g_hash_table_lookup (map->uid_map, pnode->uid);
	g_return_if_fail (unode != NULL);

	g_hash_table_remove (map->uid_map, pnode->uid);
	g_hash_table_remove (map->pid_map, &pid);
}

void
e_pilot_map_remove_by_uid (EPilotMap *map, const gchar *uid)
{
	EPilotMapPidNode *pnode;
	EPilotMapUidNode *unode;

	g_return_if_fail (map != NULL);
	g_return_if_fail (uid != NULL);

        unode = g_hash_table_lookup (map->uid_map, uid);
        if (unode == NULL)
		return;

        pnode = g_hash_table_lookup (map->pid_map, &unode->pid);

	g_hash_table_remove (map->pid_map, &unode->pid);
	g_hash_table_remove (map->uid_map, uid);
}

guint32
e_pilot_map_lookup_pid (EPilotMap *map, const gchar *uid, gboolean touch)
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

const gchar *
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
		g_return_val_if_fail (unode != NULL, NULL);

		unode->touched = TRUE;
		pnode->touched = TRUE;
	}

	return pnode->uid;
}

gint
e_pilot_map_read (const gchar *filename, EPilotMap **map)
{
	xmlSAXHandler handler;
	EPilotMap *new_map;

	g_return_val_if_fail (filename != NULL, -1);
	g_return_val_if_fail (map != NULL, -1);

	*map = NULL;
	new_map = g_new0 (EPilotMap, 1);

	memset (&handler, 0, sizeof (xmlSAXHandler));
	handler.startElement = map_sax_start_element;

	new_map->pid_map = g_hash_table_new_full (
                g_int_hash, g_int_equal,
                (GDestroyNotify) g_free,
                (GDestroyNotify) g_free);
	new_map->uid_map = g_hash_table_new_full (
                g_str_hash, g_str_equal,
                (GDestroyNotify) g_free,
                (GDestroyNotify) g_free);

	if (g_file_test (filename, G_FILE_TEST_EXISTS)) {
		if (xmlSAXUserParseFile (&handler, new_map, filename) < 0) {
			g_free (new_map);
			return -1;
		}
	}

	new_map->write_touched_only = FALSE;

	*map = new_map;

	return 0;
}

gint
e_pilot_map_write (const gchar *filename, EPilotMap *map)
{
	EPilotMapWriteData wd;
	xmlDocPtr doc;
	gint ret;

	g_return_val_if_fail (filename != NULL, -1);
	g_return_val_if_fail (map != NULL, -1);

	doc = xmlNewDoc ((const guchar *)"1.0");
	if (doc == NULL) {
		g_warning ("Pilot map file could not be created\n");
		return -1;
	}
	xmlDocSetRootElement (doc, xmlNewDocNode(doc, NULL, (const guchar *)"PilotMap", NULL));
	map->since = time (NULL);
	map_set_node_timet (xmlDocGetRootElement (doc), "timestamp", map->since);

	wd.touched_only = map->write_touched_only;
	wd.root = xmlDocGetRootElement(doc);
	g_hash_table_foreach (map->uid_map, map_write_foreach, &wd);

	/* Write the file */
	xmlSetDocCompressMode (doc, 0);
	ret = e_xml_save_file (filename, doc);
	if (ret < 0) {
		g_warning ("Pilot map file '%s' could not be saved\n", filename);
		return -1;
	}

	xmlFreeDoc (doc);

	return 0;
}

void
e_pilot_map_clear (EPilotMap *map)
{
	g_return_if_fail (map != NULL);

        g_hash_table_remove_all (map->pid_map);
        g_hash_table_remove_all (map->uid_map);

	map->since = 0;
	map->write_touched_only = FALSE;
}

void
e_pilot_map_destroy (EPilotMap *map)
{
	g_return_if_fail (map != NULL);

	g_hash_table_destroy (map->pid_map);
	g_hash_table_destroy (map->uid_map);
	g_free (map);
}
