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

#ifndef __E_BCONF_MAP_H__
#define __E_BCONF_MAP_H__

#include <gconf/gconf-client.h>

#include <libxml/tree.h>

G_BEGIN_DECLS

enum {
	E_BCONF_MAP_END = 0,        /* end of table */
	E_BCONF_MAP_BOOL,           /* bool -> prop of name 'to' value true or false */
	E_BCONF_MAP_LONG,           /* long -> prop of name 'to' value a long */
	E_BCONF_MAP_STRING,	    /* string -> prop of name 'to' */
	E_BCONF_MAP_ENUM,	    /* long/bool -> prop of name 'to', with the value indexed into the child map table's from field */
	E_BCONF_MAP_CHILD,	    /* a new child of name 'to' */
	E_BCONF_MAP_MASK = 0x3f,
	E_BCONF_MAP_CONTENT = 0x80  /* if set, create a new node of name 'to' instead of a property */
};

typedef struct _e_bconf_map {
	const gchar *from;
	const gchar *to;
	gint type;
	struct _e_bconf_map *child;
} e_bconf_map_t;

gchar *e_bconf_hex_decode (const gchar *val);
gchar *e_bconf_url_decode (const gchar *val);

xmlNodePtr e_bconf_get_path (xmlDocPtr doc, const gchar *path);
xmlNodePtr e_bconf_get_entry (xmlNodePtr root, const gchar *name);

gchar *e_bconf_get_value (xmlNodePtr root, const gchar *name);
gchar *e_bconf_get_bool (xmlNodePtr root, const gchar *name);
gchar *e_bconf_get_long (xmlNodePtr root, const gchar *name);
gchar *e_bconf_get_string (xmlNodePtr root, const gchar *name);

gint e_bconf_import_xml_blob (GConfClient *gconf, xmlDocPtr config_xmldb, e_bconf_map_t *map,
			     const gchar *bconf_path, const gchar *gconf_path,
			     const gchar *name, const gchar *idparam);

enum {
	E_GCONF_MAP_BOOL,
	E_GCONF_MAP_BOOLNOT,
	E_GCONF_MAP_INT,
	E_GCONF_MAP_STRING,
	E_GCONF_MAP_SIMPLESTRING,   /* a non-encoded string */
	E_GCONF_MAP_COLOUR,
	E_GCONF_MAP_FLOAT,	    /* bloody floats, who uses floats ... idiots */
	E_GCONF_MAP_STRLIST,	    /* strings separated to !<-->! -> gconf list */
	E_GCONF_MAP_ANYLIST,	    /* corba sequence corba string -> gconf list */
	E_GCONF_MAP_MASK = 0x7f,
	E_GCONF_MAP_LIST = 0x80     /* from includes a %i field for the index of the key, to be converted to a list of type MAP_* */
};

typedef struct {
	const gchar *from;
	const gchar *to;
	gint type;
} e_gconf_map_t;

typedef struct {
	const gchar *root;
	e_gconf_map_t *map;
} e_gconf_map_list_t;

gint e_bconf_import (GConfClient *gconf, xmlDocPtr config_xmldb, e_gconf_map_list_t *remap_list);

G_END_DECLS

#endif /* __E_BCONF_MAP_H__ */
