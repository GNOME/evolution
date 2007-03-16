/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* 
 * e-win32-reloc.c: Support relocatable installation on Win32
 * Copyright 2005, Novell, Inc.
 *
 * Authors:
 *   Tor Lillqvist <tml@novell.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License, version 2, as published by the Free Software Foundation.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
 * 02111-1307, USA.
 */

#include <config.h>

#include <windows.h>
#include <string.h>

#include <glib.h>
#include <libgnome/gnome-init.h>

/* localedir uses system codepage as it is passed to the non-UTF8ified
 * gettext library
 */
static const char *localedir = NULL;

/* The others are in UTF-8 */
static const char *category_icons;
static const char *datadir;
static const char *ecpsdir;
static const char *etspecdir;
static const char *galviewsdir;
static const char *gladedir;
static const char *helpdir;
static const char *iconsdir;
static const char *imagesdir;
static const char *libdir;
static const char *libexecdir;
static const char *plugindir;
static const char *prefix;
static const char *privdatadir;
static const char *search_rule_dir;
static const char *sounddir;
static const char *sysconfdir;
static const char *toolsdir;
static const char *uidir;

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

static char *
replace_prefix (const char *runtime_prefix,
                const char *configure_time_path)
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
	char *full_prefix;  
	char *cp_prefix; 

        G_LOCK (mutex);
        if (localedir != NULL) {
                G_UNLOCK (mutex);
                return;
        }

	/* This requires that the libeutil DLL is installed in $bindir */
        gnome_win32_get_prefixes (hmodule, &full_prefix, &cp_prefix);

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
const char *					\
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
