/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* camel-uid-cache.h: UID caching code. */

/* 
 * Authors:
 *  Bertrand Guiheneuf <bertrand@helixcode.com>
 *
 * Copyright 2000 Helix Code, Inc. (http://www.helixcode.com)
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

#ifndef CAMEL_UID_CACHE_H
#define CAMEL_UID_CACHE_H 1

#ifdef __cplusplus
extern "C" {
#pragma }
#endif /* __cplusplus */

#include <glib.h>
#include <stdio.h>

typedef struct {
	int fd, level;
	GHashTable *uids;
} CamelUIDCache;

CamelUIDCache *camel_uid_cache_new (const char *filename);
gboolean camel_uid_cache_save (CamelUIDCache *cache);
void camel_uid_cache_destroy (CamelUIDCache *cache);

GPtrArray *camel_uid_cache_get_new_uids (CamelUIDCache *cache,
					 GPtrArray *uids);
void camel_uid_cache_free_uids (GPtrArray *uids);


#ifdef __cplusplus
}
#endif /* __cplusplus */


#endif /* CAMEL_UID_CACHE_H */
