/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* camel-cache-store.c : class for a cache store */

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

#include "config.h"

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>

#include "camel-cache-store.h"
#include "camel-cache-folder.h"
#include "camel-stream-buffer.h"
#include "camel-stream-fs.h"
#include "camel-session.h"
#include "camel-exception.h"
#include "camel-url.h"
#include "md5-utils.h"

static CamelServiceClass *service_class = NULL;

static void finalize (CamelObject *object);

static gboolean cache_connect (CamelService *service, CamelException *ex);
static gboolean cache_disconnect (CamelService *service, CamelException *ex);

static CamelFolder *get_folder (CamelStore *store, const char *folder_name, 
				gboolean create, CamelException *ex);
static void delete_folder (CamelStore *store, const char *folder_name, 
			   CamelException *ex);
static char *get_folder_name (CamelStore *store, const char *folder_name, 
			      CamelException *ex);
static char *get_root_folder_name (CamelStore *store, CamelException *ex);
static char *get_default_folder_name (CamelStore *store, CamelException *ex);


static void
camel_cache_store_class_init (CamelCacheStoreClass *camel_cache_store_class)
{
	CamelServiceClass *camel_service_class =
		CAMEL_SERVICE_CLASS (camel_cache_store_class);
	CamelStoreClass *camel_store_class =
		CAMEL_STORE_CLASS (camel_cache_store_class);
	
	service_class = CAMEL_SERVICE_CLASS (camel_type_get_global_classfuncs (camel_service_get_type ()));

	/* virtual method overload */
	camel_service_class->connect = cache_connect;
	camel_service_class->disconnect = cache_disconnect;

	camel_store_class->get_folder = get_folder;
	camel_store_class->delete_folder = delete_folder;
	camel_store_class->get_folder_name = get_folder_name;
	camel_store_class->get_root_folder_name = get_root_folder_name;
	camel_store_class->get_default_folder_name = get_default_folder_name;
}


static void
camel_cache_store_init (gpointer object, gpointer klass)
{
	CamelService *service = CAMEL_SERVICE (object);

	service->url_flags = CAMEL_SERVICE_URL_NEED_PATH;
}


CamelType
camel_cache_store_get_type (void)
{
	static CamelType camel_cache_store_type = CAMEL_INVALID_TYPE;

	if (camel_cache_store_type == CAMEL_INVALID_TYPE) {
		camel_cache_store_type = camel_type_register (
			CAMEL_STORE_TYPE, "CamelCacheStore",
			sizeof (CamelCacheStore),
			sizeof (CamelCacheStoreClass),
			(CamelObjectClassInitFunc) camel_cache_store_class_init,
			NULL,
			(CamelObjectInitFunc) camel_cache_store_init,
			(CamelObjectFinalizeFunc) finalize);
	}

	return camel_cache_store_type;
}

static void
finalize (CamelObject *object)
{
	CamelCacheStore *cache_store = CAMEL_CACHE_STORE (object);

	camel_object_unref (CAMEL_OBJECT (cache_store->local));
	camel_object_unref (CAMEL_OBJECT (cache_store->remote));
}


static gboolean
cache_connect (CamelService *service, CamelException *ex)
{
	CamelCacheStore *cache_store = CAMEL_CACHE_STORE (service);

	if (!cache_store->remote) {
		cache_store->remote =
			camel_session_get_store (service->session,
						 service->url->path,
						 ex);
		if (camel_exception_is_set (ex))
			return FALSE;

		cache_store->local = XXX;
		if (camel_exception_is_set (ex))
			return FALSE;
	}

	if (!camel_service_connect (CAMEL_SERVICE (cache_store->remote), ex))
		return FALSE;
	if (!camel_service_connect (CAMEL_SERVICE (cache_store->local), ex)) {
		camel_service_disconnect (CAMEL_SERVICE (cache_store->remote),
					  NULL);
		return FALSE;
	}

	return service_class->connect (service, ex);
}

static gboolean
cache_disconnect (CamelService *service, CamelException *ex)
{
	CamelCacheStore *store = CAMEL_CACHE_STORE (service);

	if (!service_class->disconnect (service, ex))
		return FALSE;

	return camel_service_disconnect (CAMEL_SERVICE (store->local), ex) &&
		camel_service_disconnect (CAMEL_SERVICE (store->remote), ex);
}

static CamelFolder *
get_folder (CamelStore *store, const char *folder_name,
	    gboolean create, CamelException *ex)
{
	CamelCacheStore *cache_store = CAMEL_CACHE_STORE (store);
	CamelFolder *rf, *lf;

	rf = camel_store_get_folder (cache_store->remote, folder_name,
				     create, ex);
	if (!rf)
		return NULL;

	lf = camel_store_get_folder (cache_store->local, folder_name,
				     TRUE, ex);
	if (!lf) {
		camel_object_unref (CAMEL_OBJECT (rf));
		camel_exception_setv (ex, camel_exception_get_id (ex),
				      "Could not create cache folder:\n%s",
				      camel_exception_get_description (ex));
		return NULL;
	}

	return camel_cache_folder_new (cache_store, rf, lf, ex);
}

static char *
get_folder_name (CamelStore *store, const char *folder_name,
		 CamelException *ex)
{
	CamelCacheStore *cache_store = CAMEL_CACHE_STORE (store);

	return camel_store_get_folder_name (cache_store->remote,
					    folder_name, ex);
}

static char *
get_root_folder_name (CamelStore *store, CamelException *ex)
{
	CamelCacheStore *cache_store = CAMEL_CACHE_STORE (store);

	return camel_store_get_root_folder_name (cache_store->remote, ex);
}

static char *
get_default_folder_name (CamelStore *store, CamelException *ex)
{
	CamelCacheStore *cache_store = CAMEL_CACHE_STORE (store);

	return camel_store_get_default_folder_name (cache_store->remote, ex);
}
