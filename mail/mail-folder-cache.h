/*
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
 *		Peter Williams <peterw@ximian.com>
 *	    Michael Zucchi <notzed@ximian.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

/* mail-folder-cache.h: Stores information about open folders */

#ifndef _MAIL_FOLDER_CACHE_H
#define _MAIL_FOLDER_CACHE_H

#include <camel/camel-store.h>

/* Add a store whose folders should appear in the shell
   The folders are scanned from the store, and/or added at
   runtime via the folder_created event.
   The 'done' function returns if we can free folder info. */
void
mail_note_store (CamelStore *store, CamelOperation *op,
		 gboolean (*done) (CamelStore *store, CamelFolderInfo *info, gpointer data),
		 gpointer data);

/* de-note a store */
void mail_note_store_remove (CamelStore *store);

/* When a folder has been opened, notify it for watching.
   The folder must have already been created on the store (which has already been noted)
   before the folder can be opened
 */
void mail_note_folder (CamelFolder *folder);

/* Returns true if a folder is available (yet), and also sets *folderp (if supplied)
   to a (referenced) copy of the folder if it has already been opened */
gint mail_note_get_folder_from_uri (const gchar *uri, CamelFolder **folderp);
gboolean mail_folder_cache_get_folder_info_flags (CamelFolder *folder, gint *flags);

#endif
