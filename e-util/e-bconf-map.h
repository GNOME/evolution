/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 *  Authors: Jeffrey Stedfast <fejj@ximian.com>
 *
 *  Copyright 2004 Ximian, Inc. (www.ximian.com)
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Street #330, Boston, MA 02111-1307, USA.
 *
 */


#ifndef __E_BCONF_MAP_H__
#define __E_BCONF_MAP_H__

#include <gconf/gconf-client.h>

#include <libxml/tree.h>

#ifdef __cplusplus
extern "C" {
#pragma }
#endif /* __cplusplus */

enum {
	E_BCONF_MAP_END = 0,        /* end of table */
	E_BCONF_MAP_BOOL,           /* bool -> prop of name 'to' value true or false */
	E_BCONF_MAP_LONG,           /* long -> prop of name 'to' value a long */
	E_BCONF_MAP_STRING,	    /* string -> prop of name 'to' */
	E_BCONF_MAP_ENUM,	    /* long/bool -> prop of name 'to', with the value indexed into the child map table's from field */
	E_BCONF_MAP_CHILD,	    /* a new child of name 'to' */
	E_BCONF_MAP_MASK = 0x3f,
	E_BCONF_MAP_CONTENT = 0x80, /* if set, create a new node of name 'to' instead of a property */
};

typedef struct _e_bconf_map {
	char *from;
	char *to;
	int type;
	struct _e_bconf_map *child;
} e_bconf_map_t;


char *e_bconf_hex_decode (const char *val);
char *e_bconf_url_decode (const char *val);

xmlNodePtr e_bconf_get_path (xmlDocPtr doc, const char *path);
xmlNodePtr e_bconf_get_entry (xmlNodePtr root, const char *name);

char *e_bconf_get_value (xmlNodePtr root, const char *name);
char *e_bconf_get_bool (xmlNodePtr root, const char *name);
char *e_bconf_get_long (xmlNodePtr root, const char *name);
char *e_bconf_get_string (xmlNodePtr root, const char *name);

int e_bconf_import_xml_blob (GConfClient *gconf, xmlDocPtr config_xmldb, e_bconf_map_t *map,
			     const char *bconf_path, const char *gconf_path,
			     const char *name, const char *idparam);


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
	char *from;
	char *to;
	int type;
} e_gconf_map_t;

typedef struct {
	char *root;
	e_gconf_map_t *map;
} e_gconf_map_list_t;

int e_bconf_import (GConfClient *gconf, xmlDocPtr config_xmldb, e_gconf_map_list_t *remap_list);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* __E_BCONF_MAP_H__ */
