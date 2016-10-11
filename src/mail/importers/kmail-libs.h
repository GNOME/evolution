/*
 * Copyright (C) 2015 SUSE (www.suse.com)
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) version 3.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with the program; if not, see <http://www.gnu.org/licenses/>
 *
 * Authors:
 *           David Liang <dliang@suse.com>
 *
 */
#ifndef __KMAIL_LIBS_H__
#define __KMAIL_LIBS_H__

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <glib/gstdio.h>

#include <glib.h>
#include <gio/gio.h>
#include <stdlib.h>
#include <camel/camel.h>

#include "libemail-engine/libemail-engine.h"

#define EVOLUTION_LOCAL_BASE "folder://local"
#define EVOLUTION_DIR ".local/share/evolution/mail/local/"
#define KMAIL_4_10_DIR ".local/share/local-mail"
#define KMAIL_4_3_DIR ".kde4/share/apps/kmail/mail"
#define KCONTACT_4_3_DIR ".kde4/share/apps/kabc"

const CamelStore *		evolution_get_local_store (void);
gboolean 			kmail_is_supported (void);
gchar	*			kmail_get_base_dir (void);
GSList	*			kmail_get_folders (gchar *path);
gchar	*			kuri_to_euri (const gchar *k_uri);
GSList	*			kcontact_get_list (void);
void 				kcontact_load (GSList *files);

#endif
