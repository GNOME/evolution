/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* mail-folder-cache.h: Stores information about open folders */

/* 
 * Authors: Peter Williams <peterw@ximian.com>
 *	    Michael Zucchi <notzed@ximian.com>
 *
 * Copyright 2000,2001 Ximian, Inc. (www.ximian.com)
 *
 * This program is free software; you can redistribute it and/or 
 * modify it under the terms of version 2 of the GNU General Public 
 * License as published by the Free Software Foundation.
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

#include <camel/camel-store.h>

/* Add a store whose folders should appear in the shell
   The folders are scanned from the store, and/or added at
   runtime via the folder_created event */
void
mail_note_store (CamelStore *store, CamelOperation *op,
		 void (*done) (CamelStore *store, CamelFolderInfo *info, void *data),
		 void *data);

/* de-note a store */
void mail_note_store_remove (CamelStore *store);

/* When a folder has been opened, notify it for watching.
   The folder must have already been created on the store (which has already been noted)
   before the folder can be opened
 */
void mail_note_folder (CamelFolder *folder);

/* Returns true if a folder is available (yet), and also sets *folderp (if supplied)
   to a (referenced) copy of the folder if it has already been opened */
int mail_note_get_folder_from_uri (const char *uri, CamelFolder **folderp);

#endif
