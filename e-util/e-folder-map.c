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


#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include <libxml/tree.h>
#include <libxml/parser.h>
#include <libxml/xmlmemory.h>

#include "e-folder-map.h"

#define d(x) x

static gboolean
is_type_folder (const char *metadata, const char *search_type)
{
	xmlNodePtr node;
	xmlDocPtr doc;
	char *type;
	
	if (!(doc = xmlParseFile (metadata))) {
		g_warning ("Cannot parse `%s'", metadata);
		return FALSE;
	}
	
	if (!(node = xmlDocGetRootElement (doc))) {
		g_warning ("`%s' corrupt: document contains no root node", metadata);
		xmlFreeDoc (doc);
		return FALSE;
	}
	
	if (!node->name || strcmp (node->name, "efolder") != 0) {
		g_warning ("`%s' corrupt: root node is not 'efolder'", metadata);
		xmlFreeDoc (doc);
		return FALSE;
	}
	
	node = node->children;
	while (node != NULL) {
		if (node->name && !strcmp (node->name, "type")) {
			type = xmlNodeGetContent (node);
			if (!strcmp (type, search_type)) {
				xmlFreeDoc (doc);
				xmlFree (type);
				
				return TRUE;
			}
			
			xmlFree (type);
			
			break;
		}
		
		node = node->next;
	}
	
	xmlFreeDoc (doc);
	
	return FALSE;
}

static void
e_folder_map_dir (const char *dirname, const char *type, GSList **dir_list)
{
	struct dirent *dent;
	char *path, *name;
	struct stat st;
	DIR *dir;
	
	path = g_strdup_printf ("%s/folder-metadata.xml", dirname);
	if (stat (path, &st) == -1 || !S_ISREG (st.st_mode)) {
		g_free (path);
		return;
	}
	
	if (!is_type_folder (path, type)) {
		g_free (path);
		goto try_subdirs;
	}

	d(g_message ("Found '%s'", dirname));
	*dir_list = g_slist_prepend (*dir_list, g_strdup (dirname));
	
	g_free (path);	

 try_subdirs:
	
	path = g_strdup_printf ("%s/subfolders", dirname);
	if (stat (path, &st) == -1 || !S_ISDIR (st.st_mode)) {
		g_free (path);
		return;
	}
	
	if (!(dir = opendir (path))) {
		g_warning ("cannot open `%s': %s", path, strerror (errno));
		g_free (path);
		return;
	}
	
	while ((dent = readdir (dir))) {
		char *full_path;
		
		if (dent->d_name[0] == '.')
			continue;
		
		full_path = g_strdup_printf ("%s/%s", path, dent->d_name);
		if (stat (full_path, &st) == -1 || !S_ISDIR (st.st_mode)) {
			g_free (full_path);
			continue;
		}
		
		name = g_strdup_printf ("%s/%s", full_path, dent->d_name);
		e_folder_map_dir (full_path, name, dir_list);
		g_free (full_path);
		g_free (name);
	}
	
	closedir (dir);
	
	g_free (path);
}

GSList *
e_folder_map_local_folders (char *local_dir, char *type)
{
	struct dirent *dent;
	struct stat st;
	DIR *dir;	
	GSList *dir_list = NULL;
	
	if (!(dir = opendir (local_dir))) {
		g_warning ("cannot open `%s': %s", local_dir, strerror (errno));
		return NULL;
	}
	
	while ((dent = readdir (dir))) {
		char *full_path;
		
		if (dent->d_name[0] == '.')
			continue;
		
		full_path = g_build_filename (local_dir, dent->d_name, NULL);
		d(g_message ("Looking in %s", full_path));
		if (stat (full_path, &st) == -1 || !S_ISDIR (st.st_mode)) {
			g_free (full_path);
			continue;
		}
		
		e_folder_map_dir (full_path, type, &dir_list);

		g_free (full_path);
	}
	
	closedir (dir);

	return dir_list;
}
