/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* e-upgrade.c - upgrade previous config versions
 *
 * Copyright (C) 2003 Ximian, Inc.
 *
 * Authors: Michael Zucchi <notzed@ximian.com>
 *	    Jeffery Stedfast <fejj@ximian.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of version 2 of the GNU General Public
 * License as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 */

#include <sys/types.h>
#include <sys/stat.h>

#include <stdio.h>
#include <ctype.h>

#include <glib.h>
#include <gconf/gconf.h>
#include <gconf/gconf-client.h>

#include "e-util/e-bconf-map.h"
#include "e-config-upgrade.h"

/* ********************************************************************** */
/*  Tables for bonobo conf -> gconf conversion				  */
/* ********************************************************************** */

/* ********************************************************************** */

static e_gconf_map_t importer_elm_map[] = {
	/* /Importer/Elm */
	{ "mail", "importer/elm/mail", E_GCONF_MAP_BOOL },
	{ "mail-imported", "importer/elm/mail-imported", E_GCONF_MAP_BOOL },
	{ 0 },
};

static e_gconf_map_t importer_pine_map[] = {
	/* /Importer/Pine */
	{ "mail", "importer/elm/mail", E_GCONF_MAP_BOOL },
	{ "address", "importer/elm/address", E_GCONF_MAP_BOOL },
	{ 0 },
};

static e_gconf_map_t importer_netscape_map[] = {
	/* /Importer/Netscape */
	{ "mail", "importer/netscape/mail", E_GCONF_MAP_BOOL },
	{ "settings", "importer/netscape/settings", E_GCONF_MAP_BOOL },
	{ "filters", "importer/netscape/filters", E_GCONF_MAP_BOOL },
	{ 0 },
};

/* ********************************************************************** */

/* This grabs the defaults from the first view ... (?) */
static e_gconf_map_t shell_views_map[] = {
	/* /Shell/Views/0 */
	{ "Width", "shell/view_defaults/width", E_GCONF_MAP_INT },
	{ "Height", "shell/view_defaults/height", E_GCONF_MAP_INT },
	{ "ViewPanedPosition", "shell/view_defaults/folder_bar/width", E_GCONF_MAP_INT },
	{ 0 },
};

static e_gconf_map_t offlinefolders_map[] = {
	/* /OfflineFolders */
	{ "paths", "shell/offline/folder_paths", E_GCONF_MAP_ANYLIST },
	{ 0 },
};

static e_gconf_map_t shell_map[] = {
	/* /Shell */
	{ "StartOffline", "shell/start_offline", E_GCONF_MAP_BOOL },
	{ 0 },
};

/* ********************************************************************** */

static e_gconf_map_t addressbook_map[] = {
	/* /Addressbook */
	{ "select_names_uri", "addressbook/select_names/last_used_uri", E_GCONF_MAP_STRING },
	{ 0 },
};

static e_gconf_map_t addressbook_completion_map[] = {
	/* /Addressbook/Completion */
	{ "uris", "addressbook/completion/uris", E_GCONF_MAP_STRING },
	{ 0 },
};

/* ********************************************************************** */

static e_gconf_map_t general_map[] = {
	/* /General */
	{ "CategoryMasterList", "general/category_master_list", E_GCONF_MAP_STRING }
};

/* ********************************************************************** */

e_gconf_map_list_t remap_list[] = {
	{ "/Importer/Elm", importer_elm_map },
	{ "/Importer/Pine", importer_pine_map },
	{ "/Importer/Netscape", importer_netscape_map },

	{ "/Shell", shell_map },
	{ "/Shell/Views/0", shell_views_map },
	{ "/OfflineFolders", offlinefolders_map },

	{ "/Addressbook", addressbook_map },
	{ "/Addressbook/Completion", addressbook_completion_map },

	{ "/General", general_map },

	{ 0 },
};

int
e_config_upgrade(int major, int minor, int revision)
{
	xmlDocPtr config_doc = NULL;
	char *conf_file;
	struct stat st;
	int res = 0;


	conf_file = g_build_filename (g_get_home_dir (), "evolution", "config.xmldb", NULL);
	if (lstat (conf_file, &st) == 0 && S_ISREG (st.st_mode)) 
		config_doc = xmlParseFile (conf_file);
	g_free (conf_file);
	
	if (config_doc && major <=1 && minor < 3) {
		GConfClient *gconf;	

		/* move bonobo config to gconf */
		gconf = gconf_client_get_default ();

		res = e_bconf_import (gconf, config_doc, remap_list);
		if (res != 0)
			g_warning("Could not move config from bonobo-conf to gconf");

		g_object_unref (gconf);

		xmlFreeDoc(config_doc);
	}

	return res;
}
