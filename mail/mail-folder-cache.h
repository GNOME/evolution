/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* mail-folder-cache.h: Stores information about open folders */

/* 
 * Authors: Peter Williams <peterw@ximian.com>
 *
 * Copyright 2000,2001 Ximian, Inc. (www.ximian.com)
 *
 * This program is free software; you can redistribute it and/or 
 * modify it under the terms of the GNU General Public License as 
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307
 * USA
 */

#ifndef _MAIL_FOLDER_CACHE_H
#define _MAIL_FOLDER_CACHE_H

#include <camel/camel-folder.h>
#include <camel/camel-store.h>
#include <shell/evolution-storage.h>
#include <shell/Evolution.h>

#include "folder-browser.h"

/* No real order that these functions should be called. The idea is
 * that whenever a chunk of the mailer gets some up-to-date
 * information about a URI, it calls one of the _note_ functions and
 * the folder cache sees to it that the information is put to good
 * use.
 *
 * Thus there is no way to remove items from the cache. So it leaks a lot.  */

void mail_folder_cache_set_update_estorage (const gchar *uri, EvolutionStorage *estorage);
void mail_folder_cache_set_update_lstorage (const gchar *uri, 
					    GNOME_Evolution_Storage lstorage,
					    const gchar *path);

void mail_folder_cache_remove_folder (const gchar *uri);

/* We always update the shell view */
/*void mail_folder_cache_set_update_shellview (const gchar *uri);*/

void mail_folder_cache_note_folder         (const gchar *uri, CamelFolder *folder);
void mail_folder_cache_note_fb             (const gchar *uri, FolderBrowser *fb);
void mail_folder_cache_note_folderinfo     (const gchar *uri, CamelFolderInfo *fi);
void mail_folder_cache_note_name           (const gchar *uri, const gchar *name);

CamelFolder *mail_folder_cache_try_folder  (const gchar *uri);
gchar *      mail_folder_cache_try_name    (const gchar *uri);

void mail_folder_cache_set_shell_view      (GNOME_Evolution_ShellView sv);
void mail_folder_cache_set_folder_browser  (FolderBrowser *fb);

#endif
