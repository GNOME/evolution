/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* camel-cache-map.h: functions for dealing with UID maps */

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

#ifndef CAMEL_CACHE_MAP_H
#define CAMEL_CACHE_MAP_H 1

#ifdef __cplusplus
extern "C" {
#pragma }
#endif /* __cplusplus }*/

#include <glib.h>
#include <camel/camel-types.h>

typedef struct {
	GHashTable *l2r, *r2l;
} CamelCacheMap;

CamelCacheMap *camel_cache_map_new (void);
void camel_cache_map_destroy (CamelCacheMap *map);

void camel_cache_map_add (CamelCacheMap *map, const char *luid,
			  const char *ruid);
void camel_cache_map_remove (CamelCacheMap *map, const char *luid,
			     const char *ruid);
void camel_cache_map_update (CamelCacheMap *map, const char *luid,
			     const char *ruid);

const char *camel_cache_map_get_local (CamelCacheMap *map, const char *ruid);
const char *camel_cache_map_get_remote (CamelCacheMap *map, const char *luid);

void camel_cache_map_write (CamelCacheMap *map, const char *file,
			    CamelException *ex);
void camel_cache_map_read (CamelCacheMap *map, const char *file,
			   CamelException *ex);


#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* CAMEL_CACHE_MAP_H */
