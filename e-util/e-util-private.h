/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* 
 * e-util-private.h
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

#ifndef _E_UTIL_PRIVATE_H_
#define _E_UTIL_PRIVATE_H_

#include <glib.h>

#ifdef G_OS_WIN32

const char *_e_get_localedir (void) G_GNUC_CONST;
const char *_e_get_gladedir (void) G_GNUC_CONST;
const char *_e_get_helpdir (void) G_GNUC_CONST;
const char *_e_get_imagesdir (void) G_GNUC_CONST;
const char *_e_get_iconsdir (void) G_GNUC_CONST;
const char *_e_get_category_icons (void) G_GNUC_CONST;
const char *_e_get_plugindir (void) G_GNUC_CONST;
const char *_e_get_privdatadir (void) G_GNUC_CONST;
const char *_e_get_search_rule_dir (void) G_GNUC_CONST;
const char *_e_get_galviewsdir (void) G_GNUC_CONST;

#undef EVOLUTION_LOCALEDIR
#define EVOLUTION_LOCALEDIR _e_get_localedir ()

#undef EVOLUTION_GLADEDIR
#define EVOLUTION_GLADEDIR _e_get_gladedir ()

#undef EVOLUTION_HELPDIR
#define EVOLUTION_HELPDIR _e_get_helpdir ()

#undef EVOLUTION_ETSPECDIR
#define EVOLUTION_ETSPECDIR _e_get_etspecdir ()

#undef EVOLUTION_IMAGESDIR
#define EVOLUTION_IMAGESDIR _e_get_imagesdir ()

#undef EVOLUTION_IMAGES
#define EVOLUTION_IMAGES EVOLUTION_IMAGESDIR

#undef EVOLUTION_ICONSDIR
#define EVOLUTION_ICONSDIR _e_get_iconsdir ()

#undef EVOLUTION_CATEGORY_ICONS
#define EVOLUTION_CATEGORY_ICONS _e_get_category_icons ()

#undef EVOLUTION_PLUGINDIR
#define EVOLUTION_PLUGINDIR _e_get_plugindir ()

#undef EVOLUTION_PRIVDATADIR
#define EVOLUTION_PRIVDATADIR _e_get_privdatadir ()

#undef SEARCH_RULE_DIR
#define SEARCH_RULE_DIR _e_get_search_rule_dir ()

#undef EVOLUTION_GALVIEWSDIR
#define EVOLUTION_GALVIEWSDIR _e_get_galviewsdir ()

#endif	/* G_OS_WIN32 */

#endif	/* _E_UTIL_PRIVATE_H_ */
