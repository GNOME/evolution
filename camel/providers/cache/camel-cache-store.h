/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* camel-cache-store.h: class for a cache store */

/* 
 * Authors:
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

#ifndef CAMEL_CACHE_STORE_H
#define CAMEL_CACHE_STORE_H 1

#ifdef __cplusplus
extern "C" {
#pragma }
#endif /* __cplusplus }*/

#include <camel/camel-store.h>

#define CAMEL_CACHE_STORE_TYPE     (camel_cache_store_get_type ())
#define CAMEL_CACHE_STORE(obj)     (GTK_CHECK_CAST((obj), CAMEL_CACHE_STORE_TYPE, CamelCacheStore))
#define CAMEL_CACHE_STORE_CLASS(k) (GTK_CHECK_CLASS_CAST ((k), CAMEL_CACHE_STORE_TYPE, CamelCacheStoreClass))
#define IS_CAMEL_CACHE_STORE(o)    (GTK_CHECK_TYPE((o), CAMEL_CACHE_STORE_TYPE))

typedef struct {
	CamelStore parent_object;

	CamelStore *remote, *local;

} CamelCacheStore;


typedef struct {
	CamelStoreClass parent_class;

} CamelCacheStoreClass;


/* support functions */
void camel_cache_store_sync (CamelCacheStore *store);

/* Standard Gtk function */
GtkType camel_cache_store_get_type (void);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* CAMEL_CACHE_STORE_H */


