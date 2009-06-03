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
 *		Tor Lillqvist <tml@novell.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#include <config.h>

#include <windows.h>
#include <string.h>

#include <glib.h>

/* localedir uses system codepage as it is passed to the non-UTF8ified
 * gettext library
 */
static const gchar *localedir = NULL;

/* The others are in UTF-8 */
static const gchar *category_icons;
static const gchar *datadir;
static const gchar *ecpsdir;
static const gchar *etspecdir;
static const gchar *galviewsdir;
static const gchar *gladedir;
static const gchar *helpdir;
static const gchar *iconsdir;
static const gchar *imagesdir;
static const gchar *libdir;
static const gchar *libexecdir;
static const gchar *plugindir;
static const gchar *prefix;
static const gchar *privdatadir;
static const gchar *search_rule_dir;
static const gchar *sounddir;
static const gchar *sysconfdir;
static const gchar *toolsdir;
static const gchar *uidir;

static HMODULE hmodule;
G_LOCK_DEFINE_STATIC (mutex);

/* Silence gcc with a prototype. Yes, this is silly. */
BOOL WINAPI DllMain (HINSTANCE hinstDLL,
		     DWORD     fdwReason,
		     LPVOID    lpvReserved);

/* Minimal DllMain that just tucks away the DLL's HMODULE */
BOOL WINAPI
DllMain (HINSTANCE hinstDLL,
	 DWORD     fdwReason,
	 LPVOID    lpvReserved)
{
        switch (fdwReason) {
        case DLL_PROCESS_ATTACH:
                hmodule = hinstDLL;
                break;
        }
        return TRUE;
}

static gchar *
replace_prefix (const gchar *runtime_prefix,
                const gchar *configure_time_path)
{
        if (runtime_prefix &&
            strncmp (configure_time_path, EVOLUTION_PREFIX "/",
                     strlen (EVOLUTION_PREFIX) + 1) == 0) {
                return g_strconcat (runtime_prefix,
                                    configure_time_path + strlen (EVOLUTION_PREFIX),
                                    NULL);
        } else
                return g_strdup (configure_time_path);
}

static void
setup (void)
{
	gchar *full_prefix;
	gchar *cp_prefix;

        G_LOCK (mutex);
        if (localedir != NULL) {
                G_UNLOCK (mutex);
                return;
        }

	/* This requires that the libeutil DLL is installed in $bindir */
        full_prefix = g_win32_get_package_installation_directory_of_module(hmodule);
        cp_prefix = g_win32_locale_filename_from_utf8(full_prefix);

        localedir = replace_prefix (cp_prefix, EVOLUTION_LOCALEDIR);

        g_free (cp_prefix);

	prefix = g_strdup (full_prefix);

	/* It makes sense to have some of the paths overridable with
	 * environment variables.
	 */
	category_icons = replace_prefix (full_prefix, EVOLUTION_CATEGORY_ICONS);
	datadir = replace_prefix (full_prefix, EVOLUTION_DATADIR);
	ecpsdir = replace_prefix (full_prefix, EVOLUTION_ECPSDIR);
	etspecdir = replace_prefix (full_prefix, EVOLUTION_ETSPECDIR);
	galviewsdir = replace_prefix (full_prefix, EVOLUTION_GALVIEWSDIR);
	gladedir = replace_prefix (full_prefix, EVOLUTION_GLADEDIR);
	helpdir = replace_prefix (full_prefix, EVOLUTION_HELPDIR);
	if (g_getenv ("EVOLUTION_ICONSDIR") &&
	    g_file_test (g_getenv ("EVOLUTION_ICONSDIR"), G_FILE_TEST_IS_DIR))
		iconsdir = g_getenv ("EVOLUTION_ICONSDIR");
	else
		iconsdir = replace_prefix (full_prefix, EVOLUTION_ICONSDIR);
	if (g_getenv ("EVOLUTION_IMAGESDIR") &&
	    g_file_test (g_getenv ("EVOLUTION_IMAGESDIR"), G_FILE_TEST_IS_DIR))
		imagesdir = g_getenv ("EVOLUTION_IMAGESDIR");
	else
		imagesdir = replace_prefix (full_prefix, EVOLUTION_IMAGESDIR);
	libdir = replace_prefix (full_prefix, EVOLUTION_LIBDIR);
	libexecdir = replace_prefix (full_prefix, EVOLUTION_LIBEXECDIR);
	plugindir = replace_prefix (full_prefix, EVOLUTION_PLUGINDIR);
	privdatadir = replace_prefix (full_prefix, EVOLUTION_PRIVDATADIR);
	search_rule_dir = replace_prefix (full_prefix, SEARCH_RULE_DIR);
	sounddir = replace_prefix (full_prefix, EVOLUTION_SOUNDDIR);
	sysconfdir = replace_prefix (full_prefix, EVOLUTION_SYSCONFDIR);
	toolsdir = replace_prefix (full_prefix, EVOLUTION_TOOLSDIR);
	uidir = replace_prefix (full_prefix, EVOLUTION_UIDIR);

        g_free (full_prefix);

	G_UNLOCK (mutex);
}

#include "e-util-private.h"	/* For prototypes */

#define GETTER(varbl)				\
const gchar *					\
_e_get_##varbl (void)				\
{						\
        setup ();				\
        return varbl;				\
}

GETTER(category_icons)
GETTER(datadir)
GETTER(ecpsdir)
GETTER(etspecdir)
GETTER(galviewsdir)
GETTER(gladedir)
GETTER(helpdir)
GETTER(iconsdir)
GETTER(imagesdir)
GETTER(libdir)
GETTER(libexecdir)
GETTER(localedir)
GETTER(plugindir)
GETTER(prefix)
GETTER(privdatadir)
GETTER(search_rule_dir)
GETTER(sounddir)
GETTER(sysconfdir)
GETTER(toolsdir)
GETTER(uidir)
