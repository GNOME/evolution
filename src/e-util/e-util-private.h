/*
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 *
 *
 * Authors:
 *		Tor Lillqvist <tml@novell.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#ifndef _E_UTIL_PRIVATE_H_
#define _E_UTIL_PRIVATE_H_

#include <fcntl.h>
#include <glib.h>

#ifndef O_BINARY
#define O_BINARY 0
#endif

#ifdef G_OS_WIN32

#define fsync(fd) 0

gpointer _e_get_dll_hmodule (void) G_GNUC_CONST;

const gchar *_e_get_bindir (void) G_GNUC_CONST;
const gchar *_e_get_datadir (void) G_GNUC_CONST;
const gchar *_e_get_ecpsdir (void) G_GNUC_CONST;
const gchar *_e_get_etspecdir (void) G_GNUC_CONST;
const gchar *_e_get_galviewsdir (void) G_GNUC_CONST;
const gchar *_e_get_helpdir (void) G_GNUC_CONST;
const gchar *_e_get_icondir (void) G_GNUC_CONST;
const gchar *_e_get_imagesdir (void) G_GNUC_CONST;
const gchar *_e_get_libdir (void) G_GNUC_CONST;
const gchar *_e_get_libexecdir (void) G_GNUC_CONST;
const gchar *_e_get_localedir (void) G_GNUC_CONST;
const gchar *_e_get_moduledir (void) G_GNUC_CONST;
const gchar *_e_get_plugindir (void) G_GNUC_CONST;
const gchar *_e_get_prefix (void) G_GNUC_CONST;
const gchar *_e_get_privdatadir (void) G_GNUC_CONST;
const gchar *_e_get_ruledir (void) G_GNUC_CONST;
const gchar *_e_get_sounddir (void) G_GNUC_CONST;
const gchar *_e_get_sysconfdir (void) G_GNUC_CONST;
const gchar *_e_get_toolsdir (void) G_GNUC_CONST;
const gchar *_e_get_uidir (void) G_GNUC_CONST;
const gchar *_e_get_webkitdatadir (void) G_GNUC_CONST;
const gchar *_e_get_data_server_icondir (void) G_GNUC_CONST;

#undef DATADIR
#define DATADIR _e_get_datadir ()

#undef LIBDIR
#define LIBDIR _e_get_libdir ()

#undef SYSCONFDIR
#define SYSCONFDIR _e_get_sysconfdir ()

#undef PREFIX
#define PREFIX _e_get_prefix ()

#undef EVOLUTION_BINDIR
#define EVOLUTION_BINDIR _e_get_bindir ()

#undef EVOLUTION_DATADIR
#define EVOLUTION_DATADIR _e_get_datadir ()

#undef EVOLUTION_ECPSDIR
#define EVOLUTION_ECPSDIR _e_get_ecpsdir ()

#undef EVOLUTION_ETSPECDIR
#define EVOLUTION_ETSPECDIR _e_get_etspecdir ()

#undef EVOLUTION_LOCALEDIR
#define EVOLUTION_LOCALEDIR _e_get_localedir ()

#undef EVOLUTION_GALVIEWSDIR
#define EVOLUTION_GALVIEWSDIR _e_get_galviewsdir ()

#undef EVOLUTION_HELPDIR
#define EVOLUTION_HELPDIR _e_get_helpdir ()

#undef EVOLUTION_ICONDIR
#define EVOLUTION_ICONDIR _e_get_icondir ()

#undef EVOLUTION_IMAGESDIR
#define EVOLUTION_IMAGESDIR _e_get_imagesdir ()

#undef EVOLUTION_LIBEXECDIR
#define EVOLUTION_LIBEXECDIR _e_get_libexecdir ()

#undef EVOLUTION_MODULEDIR
#define EVOLUTION_MODULEDIR _e_get_moduledir ()

#undef EVOLUTION_PLUGINDIR
#define EVOLUTION_PLUGINDIR _e_get_plugindir ()

#undef EVOLUTION_PRIVDATADIR
#define EVOLUTION_PRIVDATADIR _e_get_privdatadir ()

#undef EVOLUTION_SOUNDDIR
#define EVOLUTION_SOUNDDIR _e_get_sounddir ()

#undef EVOLUTION_SYSCONFDIR
#define EVOLUTION_SYSCONFDIR _e_get_sysconfdir ()

#undef EVOLUTION_TOOLSDIR
#define EVOLUTION_TOOLSDIR _e_get_toolsdir ()

#undef EVOLUTION_UIDIR
#define EVOLUTION_UIDIR _e_get_uidir ()

#undef EVOLUTION_RULEDIR
#define EVOLUTION_RULEDIR _e_get_ruledir ()

#undef EVOLUTION_WEBKITDATADIR
#define EVOLUTION_WEBKITDATADIR _e_get_webkitdatadir ()

#undef E_DATA_SERVER_ICONDIR
#define E_DATA_SERVER_ICONDIR _e_get_data_server_icondir ()

#endif	/* G_OS_WIN32 */

#endif	/* _E_UTIL_PRIVATE_H_ */
