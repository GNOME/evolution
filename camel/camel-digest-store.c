/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 *  Authors: Jeffrey Stedfast <fejj@ximian.com>
 *
 *  Copyright 2002 Ximian, Inc. (www.ximian.com)
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Street #330, Boston, MA 02111-1307, USA.
 *
 */


#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <string.h>

#include "camel-exception.h"
#include "camel-digest-store.h"
#include "camel-digest-folder.h"

#include "camel-private.h"

#define d(x)

static CamelFolder *digest_get_folder (CamelStore *store, const char *folder_name, guint32 flags, CamelException *ex);
static void digest_delete_folder (CamelStore *store, const char *folder_name, CamelException *ex);
static void digest_rename_folder (CamelStore *store, const char *old, const char *new, CamelException *ex);
static void digest_init_trash (CamelStore *store);
static CamelFolder *digest_get_trash  (CamelStore *store, CamelException *ex);

static CamelFolderInfo *digest_get_folder_info (CamelStore *store, const char *top, guint32 flags, CamelException *ex);

static void camel_digest_store_class_init (CamelDigestStoreClass *klass);
static void camel_digest_store_init       (CamelDigestStore *obj);
static void camel_digest_store_finalise   (CamelObject *obj);

static CamelStoreClass *camel_digest_store_parent = NULL;


CamelType
camel_digest_store_get_type (void)
{
	static CamelType type = CAMEL_INVALID_TYPE;
	
	if (type == CAMEL_INVALID_TYPE) {
		type = camel_type_register (camel_store_get_type (),
					    "CamelDigestStore",
					    sizeof (CamelDigestStore),
					    sizeof (CamelDigestStoreClass),
					    (CamelObjectClassInitFunc) camel_digest_store_class_init,
					    NULL,
					    (CamelObjectInitFunc) camel_digest_store_init,
					    (CamelObjectFinalizeFunc) camel_digest_store_finalise);
	}
	
	return type;
}

static void
camel_digest_store_class_init (CamelDigestStoreClass *klass)
{
	CamelStoreClass *store_class = (CamelStoreClass *) klass;
	
	camel_digest_store_parent = CAMEL_STORE_CLASS(camel_type_get_global_classfuncs (camel_store_get_type ()));
	
	/* virtual method overload */
	store_class->get_folder = digest_get_folder;
	store_class->rename_folder = digest_rename_folder;
	store_class->delete_folder = digest_delete_folder;
	store_class->get_folder_info = digest_get_folder_info;
	store_class->free_folder_info = camel_store_free_folder_info_full;
	
	store_class->init_trash = digest_init_trash;
	store_class->get_trash = digest_get_trash;
}

static void
camel_digest_store_init (CamelDigestStore *obj)
{
	CamelStore *store = (CamelStore *) obj;
	
	/* we dont want a vtrash on this one */
	store->flags &= ~(CAMEL_STORE_VTRASH);	
}

static void
camel_digest_store_finalise (CamelObject *obj)
{
	
}


/**
 * camel_digest_store_new:
 *
 * Create a new CamelDigestStore object.
 * 
 * Return value: A new CamelDigestStore widget.
 **/
CamelStore *
camel_digest_store_new (void)
{
	CamelStore *store = CAMEL_STORE (camel_object_new (camel_digest_store_get_type ()));
	
	return store;
}

static CamelFolder *
digest_get_folder (CamelStore *store, const char *folder_name, guint32 flags, CamelException *ex)
{
	return NULL;
}

static void
digest_init_trash (CamelStore *store)
{
	/* no-op */
	;
}

static CamelFolder *
digest_get_trash (CamelStore *store, CamelException *ex)
{
	return NULL;
}

static CamelFolderInfo *
digest_get_folder_info (CamelStore *store, const char *top, guint32 flags, CamelException *ex)
{
	return NULL;
}

static void
digest_delete_folder (CamelStore *store, const char *folder_name, CamelException *ex)
{
	
}

static void
digest_rename_folder (CamelStore *store, const char *old, const char *new, CamelException *ex)
{
	
}
