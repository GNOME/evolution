/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* camel-cache-folder.h: Class for a cache folder */

/* 
 * Author:
 *   Dan Winship <danw@helixcode.com>
 *
 * Copyright (C) 2000 Helix Code, Inc. (www.helixcode.com)
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

#ifndef CAMEL_CACHE_FOLDER_H
#define CAMEL_CACHE_FOLDER_H 1

#ifdef __cplusplus
extern "C" {
#pragma }
#endif /* __cplusplus }*/

#include <camel/camel-folder.h>
#include "camel-cache-map.h"

#define CAMEL_CACHE_FOLDER_TYPE     (camel_cache_folder_get_type ())
#define CAMEL_CACHE_FOLDER(obj)     (CAMEL_CHECK_CAST((obj), CAMEL_CACHE_FOLDER_TYPE, CamelCacheFolder))
#define CAMEL_CACHE_FOLDER_CLASS(k) (CAMEL_CHECK_CLASS_CAST ((k), CAMEL_CACHE_FOLDER_TYPE, CamelCacheFolderClass))
#define IS_CAMEL_CACHE_FOLDER(o)    (CAMEL_CHECK_TYPE((o), CAMEL_CACHE_FOLDER_TYPE))


typedef struct {
	CamelFolder parent_object;

	/* Remote and local folders */
	CamelFolder *remote, *local;

	/* Remote UIDs, in order, summary, and uid->info map if
	 * summary is from local info.
	 */
	GPtrArray *uids, *summary;
	GHashTable *summary_uids;

	/* UID map */
	CamelCacheMap *uidmap;
	char *mapfile;

	/* Is the summary remote? Is the folder known to be synced? */
	gboolean remote_summary, is_synced;

} CamelCacheFolder;



typedef struct {
	CamelFolderClass parent_class;

	/* Virtual methods */	
	
} CamelCacheFolderClass;


CamelFolder *camel_cache_folder_new (CamelStore *store, CamelFolder *parent,
				     CamelFolder *remote, CamelFolder *local);

void camel_cache_folder_sync (CamelCacheFolder *cache_folder,
			      CamelException *ex);

/* Standard Camel function */
CamelType camel_cache_folder_get_type (void);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* CAMEL_CACHE_FOLDER_H */
