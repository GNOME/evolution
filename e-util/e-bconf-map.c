/*
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
 *		Jeffrey Stedfast <fejj@ximian.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <glib.h>

#include <libxml/tree.h>
#include <libxml/parser.h>
#include <libxml/xmlmemory.h>

#include "e-bconf-map.h"

#define d(x)

static gchar hexnib[256] = {
	-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
	-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
	-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
	 0, 1, 2, 3, 4, 5, 6, 7, 8, 9,-1,-1,-1,-1,-1,-1,
	-1,10,11,12,13,14,15,16,-1,-1,-1,-1,-1,-1,-1,-1,
	-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
	-1,10,11,12,13,14,15,16,-1,-1,-1,-1,-1,-1,-1,-1,
	-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
	-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
};

gchar *
e_bconf_hex_decode (const gchar *val)
{
	const guchar *p = (const guchar *) val;
	gchar *o, *res;

	o = res = g_malloc (strlen (val) / 2 + 1);
	for (p = (const guchar *)val; (p[0] && p[1]); p += 2)
		*o++ = (hexnib[p[0]] << 4) | hexnib[p[1]];
	*o = 0;

	return res;
}

gchar *
e_bconf_url_decode (const gchar *val)
{
	const guchar *p = (const guchar *) val;
	gchar *o, *res, c;

	o = res = g_malloc (strlen (val) + 1);
	while (*p) {
		c = *p++;
		if (c == '%'
		    && hexnib[p[0]] != -1 && hexnib[p[1]] != -1) {
			*o++ = (hexnib[p[0]] << 4) | hexnib[p[1]];
			p+=2;
		} else
			*o++ = c;
	}
	*o = 0;

	return res;
}

xmlNodePtr
e_bconf_get_path (xmlDocPtr doc, const gchar *path)
{
	xmlNodePtr root;
	gchar *val;
	gint found;

	root = doc->children;
	if (strcmp ((gchar *)root->name, "bonobo-config") != 0) {
		g_warning ("not bonobo-config xml file");
		return NULL;
	}

	root = root->children;
	while (root) {
		if (!strcmp ((gchar *)root->name, "section")) {
			val = (gchar *)xmlGetProp (root, (const guchar *)"path");
			found = val && strcmp (val, path) == 0;
			xmlFree (val);
			if (found)
				break;
		}
		root = root->next;
	}

	return root;
}

xmlNodePtr
e_bconf_get_entry (xmlNodePtr root, const gchar *name)
{
	xmlNodePtr node = root->children;
	gint found;
	gchar *val;

	while (node) {
		if (!strcmp ((gchar *)node->name, "entry")) {
			val = (gchar *)xmlGetProp (node, (const guchar *)"name");
			found = val && strcmp (val, name) == 0;
			xmlFree (val);
			if (found)
				break;
		}
		node = node->next;
	}

	return node;
}

gchar *
e_bconf_get_value (xmlNodePtr root, const gchar *name)
{
	xmlNodePtr node = e_bconf_get_entry (root, name);
	gchar *prop, *val = NULL;

	if (node && (prop = (gchar *)xmlGetProp (node, (const guchar *)"value"))) {
		val = g_strdup (prop);
		xmlFree (prop);
	}

	return val;
}

gchar *
e_bconf_get_bool (xmlNodePtr root, const gchar *name)
{
	gchar *val, *res;

	if ((val = e_bconf_get_value (root, name))) {
		res = g_strdup (val[0] == '1' ? "true" : "false");
		g_free (val);
	} else
		res = NULL;

	return res;
}

gchar *
e_bconf_get_long (xmlNodePtr root, const gchar *name)
{
	gchar *val, *res;

	if ((val = e_bconf_get_value (root, name))) {
		res = g_strdup (val);
		g_free (val);
	} else
		res = NULL;

	return res;
}

gchar *
e_bconf_get_string (xmlNodePtr root, const gchar *name)
{
	gchar *val, *res;

	if ((val = e_bconf_get_value (root, name))) {
		res = e_bconf_hex_decode (val);
		g_free (val);
	} else
		res = NULL;

	return res;
}

/* lookup functions */
typedef gchar * (* bconf_lookup_func) (xmlNodePtr root, const gchar *name, e_bconf_map_t *nap);

static gchar *
bconf_lookup_bool (xmlNodePtr root, const gchar *name, e_bconf_map_t *map)
{
	return e_bconf_get_bool (root, name);
}

static gchar *
bconf_lookup_long (xmlNodePtr root, const gchar *name, e_bconf_map_t *map)
{
	return e_bconf_get_long (root, name);
}

static gchar *
bconf_lookup_string (xmlNodePtr root, const gchar *name, e_bconf_map_t *map)
{
	return e_bconf_get_string (root, name);
}

static gchar *
bconf_lookup_enum (xmlNodePtr root, const gchar *name, e_bconf_map_t *map)
{
	gint index = 0, i;
	gchar *val;

	if ((val = e_bconf_get_value (root, name))) {
		index = atoi (val);
		g_free (val);
	}

	for (i = 0; map->child[i].from; i++) {
		if (i == index)
			return g_strdup (map->child[i].from);
	}

	return NULL;
}

static bconf_lookup_func lookup_table[] = {
	bconf_lookup_bool, bconf_lookup_long, bconf_lookup_string, bconf_lookup_enum
};

static gchar *
get_name (const gchar *in, gint index)
{
	GString *out = g_string_new ("");
	gchar c, *res;

	while ((c = *in++)) {
		if (c == '%') {
			c = *in++;
			switch (c) {
			case '%':
				g_string_append_c (out, '%');
				break;
			case 'i':
				g_string_append_printf (out, "%d", index);
				break;
			}
		} else {
			g_string_append_c (out, c);
		}
	}

	res = out->str;
	g_string_free (out, FALSE);

	return res;
}

static void
build_xml (xmlNodePtr root, e_bconf_map_t *map, gint index, xmlNodePtr source)
{
	gchar *name, *value;
	xmlNodePtr node;

	while (map->type != E_BCONF_MAP_END) {
		if ((map->type & E_BCONF_MAP_MASK) == E_BCONF_MAP_CHILD) {
			node = xmlNewChild (root, NULL, (guchar *)map->to, NULL);
			build_xml (node, map->child, index, source);
		} else {
			name = get_name (map->from, index);
			value = lookup_table[(map->type & E_BCONF_MAP_MASK) - 1] (source, name, map);

			d(printf ("key '%s=%s' -> ", name, value));

			if (map->type & E_BCONF_MAP_CONTENT) {
				if (value && value[0])
					xmlNewTextChild (root, NULL, (guchar *)map->to, (guchar *)value);
			} else {
				xmlSetProp (root, (guchar *)map->to, (guchar *)value);
			}

			g_free (value);
			g_free (name);
		}
		map++;
	}
}

gint
e_bconf_import_xml_blob (GConfClient *gconf, xmlDocPtr config_xmldb, e_bconf_map_t *map,
			 const gchar *bconf_path, const gchar *gconf_path,
			 const gchar *name, const gchar *idparam)
{
	xmlNodePtr source;
	gint count = 0, i;
	GSList *list, *l;
	gchar *val;

	source = e_bconf_get_path (config_xmldb, bconf_path);
	if (source) {
		list = NULL;
		if ((val = e_bconf_get_value (source, "num"))) {
			count = atoi (val);
			g_free (val);
		}

		d(printf("Found %d blobs at %s\n", count, bconf_path));

		for (i = 0; i < count; i++) {
			xmlDocPtr doc;
			xmlNodePtr root;
			xmlChar *xmlbuf;
			gint n;

			doc = xmlNewDoc ((const guchar *)"1.0");
			root = xmlNewDocNode (doc, NULL, (guchar *)name, NULL);
			xmlDocSetRootElement (doc, root);

			/* This could be set with a MAP_UID type ... */
			if (idparam) {
				gchar buf[16];

				sprintf (buf, "%d", i);
				xmlSetProp (root, (guchar *)idparam, (guchar *)buf);
			}

			build_xml (root, map, i, source);

			xmlDocDumpMemory (doc, &xmlbuf, &n);
			xmlFreeDoc (doc);

			list = g_slist_append (list, xmlbuf);
		}

		gconf_client_set_list (gconf, gconf_path, GCONF_VALUE_STRING, list, NULL);

		while (list) {
			l = list->next;
			xmlFree (list->data);
			g_slist_free_1 (list);
			list = l;
		}
	} else {
		g_warning ("could not find '%s' in old config database, skipping", bconf_path);
	}

	return 0;
}

static gint gconf_type[] = { GCONF_VALUE_BOOL, GCONF_VALUE_BOOL, GCONF_VALUE_INT, GCONF_VALUE_STRING, GCONF_VALUE_STRING };

gint
e_bconf_import (GConfClient *gconf, xmlDocPtr config_xmldb, e_gconf_map_list_t *remap_list)
{
	gchar *path, *val, *tmp;
	e_gconf_map_t *map;
	xmlNodePtr source;
	GSList *list, *l;
	gchar buf[32];
	gint i, j, k;

	/* process all flat config */
	for (i = 0; remap_list[i].root; i++) {
		d(printf ("Path: %s\n", remap_list[i].root));
		if (!(source = e_bconf_get_path (config_xmldb, remap_list[i].root)))
			continue;

		map = remap_list[i].map;
		for (j = 0; map[j].from; j++) {
			if (map[j].type & E_GCONF_MAP_LIST) {
				/* collapse a multi-entry indexed field into a list */
				list = NULL;
				k = 0;
				do {
					path = get_name (map[j].from, k);
					val = e_bconf_get_value (source, path);
					d(printf ("finding path '%s' = '%s'\n", path, val));
					g_free (path);
					if (val) {
						switch (map[j].type & E_GCONF_MAP_MASK) {
						case E_GCONF_MAP_BOOL:
						case E_GCONF_MAP_INT:
							list = g_slist_append (list, GINT_TO_POINTER (atoi (val)));
							break;
						case E_GCONF_MAP_STRING:
							d(printf (" -> '%s'\n", e_bconf_hex_decode (val)));
							list = g_slist_append (list, e_bconf_hex_decode (val));
							break;
						}

						g_free (val);
						k++;
					}
				} while (val);

				if (list) {
					path = g_strdup_printf ("/apps/evolution/%s", map[j].to);
					gconf_client_set_list (gconf, path, gconf_type[map[j].type & E_GCONF_MAP_MASK], list, NULL);
					g_free (path);
					if ((map[j].type & E_GCONF_MAP_MASK) == E_GCONF_MAP_STRING)
						g_slist_foreach (list, (GFunc) g_free, NULL);
					g_slist_free (list);
				}

				continue;
			} else if (map[j].type == E_GCONF_MAP_ANYLIST) {
				val = NULL;
			} else {
				if (!(val = e_bconf_get_value (source, map[j].from)))
					continue;
			}

			d(printf (" %s = '%s' -> %s [%d]\n",
				  map[j].from,
				  val == NULL ? "(null)" : val,
				  map[j].to,
				  map[j].type));

			path = g_strdup_printf ("/apps/evolution/%s", map[j].to);
			switch (map[j].type) {
			case E_GCONF_MAP_BOOL:
				gconf_client_set_bool (gconf, path, atoi (val), NULL);
				break;
			case E_GCONF_MAP_BOOLNOT:
				gconf_client_set_bool (gconf, path, !atoi (val), NULL);
				break;
			case E_GCONF_MAP_INT:
				gconf_client_set_int (gconf, path, atoi (val), NULL);
				break;
			case E_GCONF_MAP_STRING:
				tmp = e_bconf_hex_decode (val);
				gconf_client_set_string (gconf, path, tmp, NULL);
				g_free (tmp);
				break;
			case E_GCONF_MAP_SIMPLESTRING:
				gconf_client_set_string (gconf, path, val, NULL);
				break;
			case E_GCONF_MAP_FLOAT:
				gconf_client_set_float (gconf, path, strtod (val, NULL), NULL);
				break;
			case E_GCONF_MAP_STRLIST: {
				gchar *v = e_bconf_hex_decode (val);
				gchar **t = g_strsplit (v, " !<-->!", 8196);

				list = NULL;
				for (k = 0; t[k]; k++) {
					list = g_slist_append (list, t[k]);
					d(printf ("  [%d] = '%s'\n", k, t[k]));
				}

				gconf_client_set_list (gconf, path, GCONF_VALUE_STRING, list, NULL);
				g_slist_free (list);
				g_strfreev (t);
				g_free (v);
				break; }
			case E_GCONF_MAP_ANYLIST: {
				xmlNodePtr node = source->children;
				list = NULL;

				/* find the entry node */
				while (node) {
					if (!strcmp ((gchar *)node->name, "entry")) {
						gint found;

						if ((tmp = (gchar *)xmlGetProp (node, (const guchar *)"name"))) {
							found = strcmp ((gchar *)tmp, map[j].from) == 0;
							xmlFree (tmp);
							if (found)
								break;
						}
					}

					node = node->next;
				}

				/* find the the any block */
				if (node) {
					node = node->children;
					while (node) {
						if (strcmp ((gchar *)node->name, "any") == 0)
							break;
						node = node->next;
					}
				}

				/* skip to the value inside it */
				if (node) {
					node = node->children;
					while (node) {
						if (strcmp ((gchar *)node->name, "value") == 0)
							break;
						node = node->next;
					}
				}

				if (node) {
					node = node->children;
					while (node) {
						if (strcmp ((gchar *)node->name, "value") == 0)
							list = g_slist_append (list, xmlNodeGetContent (node));
						node = node->next;
					}
				}

				/* & store */
				if (list) {
					gconf_client_set_list (gconf, path, GCONF_VALUE_STRING, list, NULL);
					while (list) {
						l = list->next;
						xmlFree (list->data);
						g_slist_free_1 (list);
						list = l;
					}
				}

				break; }
			case E_GCONF_MAP_COLOUR:
				sprintf (buf, "#%06x", atoi (val) & 0xffffff);
				gconf_client_set_string (gconf, path, buf, NULL);
				break;
			}

			/* FIXME: handle errors */
			g_free (path);
			g_free (val);
		}
	}

	return 0;
}
