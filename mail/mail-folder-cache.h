/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* mail-folder-cache.h: Stores information about open folders */

/* 
 * Authors: Peter Williams <peterw@ximian.com>
 *	    Michael Zucchi <notzed@ximian.com>
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

#include <shell/evolution-storage.h>

/* Add a store whose folders should appear in the shell
   The folders are scanned from the store, and/or added at
   runtime via the folder_created event */
void mail_note_store(struct _CamelStore *store);

/* Similar to above, but do updates via a local GNOME_Evolutuion_Storage
   rather than a remote proxy EvolutionStorage object */
void mail_note_local_store(struct _CamelStore *store, GNOME_Evolution_Storage corba_storage);

/* When a folder has been opened, notify it for watching.
   The path may be NULL if the shell-equivalent path can be determined
   from the folder->full_name, if it cannot, then the path must	
   be supplied */
void mail_note_folder(struct _CamelFolder *folder, const char *path);

#endif
