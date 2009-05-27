/*
 * e-upgrade.c - upgrade previous config versions
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
 *		Michael Zucchi <notzed@ximian.com>
 *	    Jeffery Stedfast <fejj@ximian.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#include <config.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <stdio.h>
#include <ctype.h>

#include <glib.h>

#include <gconf/gconf.h>
#include <gconf/gconf-client.h>

#include <libedataserver/e-xml-utils.h>

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
	{ NULL },
};

static e_gconf_map_t importer_pine_map[] = {
	/* /Importer/Pine */
	{ "mail", "importer/elm/mail", E_GCONF_MAP_BOOL },
	{ "address", "importer/elm/address", E_GCONF_MAP_BOOL },
	{ NULL },
};

static e_gconf_map_t importer_netscape_map[] = {
	/* /Importer/Netscape */
	{ "mail", "importer/netscape/mail", E_GCONF_MAP_BOOL },
	{ "settings", "importer/netscape/settings", E_GCONF_MAP_BOOL },
	{ "filters", "importer/netscape/filters", E_GCONF_MAP_BOOL },
	{ NULL },
};

/* ********************************************************************** */

/* This grabs the defaults from the first view ... (?) */
static e_gconf_map_t shell_views_map[] = {
	/* /Shell/Views/0 */
	{ "Width", "shell/view_defaults/width", E_GCONF_MAP_INT },
	{ "Height", "shell/view_defaults/height", E_GCONF_MAP_INT },
	{ "ViewPanedPosition", "shell/view_defaults/folder_bar/width", E_GCONF_MAP_INT },
	{ NULL },
};

static e_gconf_map_t offlinefolders_map[] = {
	/* /OfflineFolders */
	{ "paths", "shell/offline/folder_paths", E_GCONF_MAP_ANYLIST },
	{ NULL },
};

static e_gconf_map_t shell_map[] = {
	/* /Shell */
	{ "StartOffline", "shell/start_offline", E_GCONF_MAP_BOOL },
	{ NULL },
};

/* ********************************************************************** */

static e_gconf_map_t addressbook_map[] = {
	/* /Addressbook */
	{ "select_names_uri", "addressbook/select_names/last_used_uri", E_GCONF_MAP_STRING },
	{ NULL },
};

static e_gconf_map_t addressbook_completion_map[] = {
	/* /Addressbook/Completion */
	{ "uris", "addressbook/completion/uris", E_GCONF_MAP_STRING },
	{ NULL },
};

/* ********************************************************************** */

static e_gconf_map_t general_map[] = {
	/* /General */
	{ "CategoryMasterList", "general/category_master_list", E_GCONF_MAP_STRING }
};

/* ********************************************************************** */

static e_gconf_map_list_t remap_list[] = {
	{ "/Importer/Elm", importer_elm_map },
	{ "/Importer/Pine", importer_pine_map },
	{ "/Importer/Netscape", importer_netscape_map },

	{ "/Shell", shell_map },
	{ "/Shell/Views/0", shell_views_map },
	{ "/OfflineFolders", offlinefolders_map },

	{ "/Addressbook", addressbook_map },
	{ "/Addressbook/Completion", addressbook_completion_map },

	{ "/General", general_map },

	{ NULL },
};

gint
e_config_upgrade(gint major, gint minor, gint revision)
{
	xmlDocPtr config_doc;
	gchar *conf_file;
	gint res = 0;

	conf_file = g_build_filename (g_get_home_dir (), "evolution", "config.xmldb", NULL);
	config_doc = e_xml_parse_file (conf_file);
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
