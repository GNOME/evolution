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
static void digest_init_junk (CamelStore *store);
static CamelFolder *digest_get_junk  (CamelStore *store, CamelException *ex);

static CamelFolderInfo *digest_get_folder_info (CamelStore *store, const char *top, guint32 flags, CamelException *ex);

static void camel_digest_store_class_init (CamelDigestStoreClass *klass);
static void camel_digest_store_init       (CamelDigestStore *obj);
static void camel_digest_store_finalise   (CamelObject *obj);

static int digest_setv (CamelObject *object, CamelException *ex, CamelArgV *args);
static int digest_getv (CamelObject *object, CamelException *ex, CamelArgGetV *args);

static CamelStoreClass *parent_class = NULL;


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
	CamelObjectClass *object_class = (CamelObjectClass *) klass;
	CamelStoreClass *store_class = (CamelStoreClass *) klass;
	
	parent_class = CAMEL_STORE_CLASS(camel_type_get_global_classfuncs (camel_store_get_type ()));
	
	/* virtual method overload */
	object_class->setv = digest_setv;
	object_class->getv = digest_getv;
	
	store_class->get_folder = digest_get_folder;
	store_class->rename_folder = digest_rename_folder;
	store_class->delete_folder = digest_delete_folder;
	store_class->get_folder_info = digest_get_folder_info;
	store_class->free_folder_info = camel_store_free_folder_info_full;
	
	store_class->init_trash = digest_init_trash;
	store_class->get_trash = digest_get_trash;
	store_class->init_junk = digest_init_junk;
	store_class->get_junk = digest_get_junk;
}

static void
camel_digest_store_init (CamelDigestStore *obj)
{
	CamelStore *store = (CamelStore *) obj;
	
	/* we dont want a vtrash and vjunk on this one */
	store->flags &= ~(CAMEL_STORE_VTRASH | CAMEL_STORE_VJUNK);	
}

static void
camel_digest_store_finalise (CamelObject *obj)
{
	
}

static int
digest_setv (CamelObject *object, CamelException *ex, CamelArgV *args)
{
	/* CamelDigestStore doesn't currently have anything to set */
	return CAMEL_OBJECT_CLASS (parent_class)->setv (object, ex, args);
}

static int
digest_getv (CamelObject *object, CamelException *ex, CamelArgGetV *args)
{
	/* CamelDigestStore doesn't currently have anything to get */
	return CAMEL_OBJECT_CLASS (parent_class)->getv (object, ex, args);
}


/**
 * camel_digest_store_new:
 * @url:
 *
 * Create a new CamelDigestStore object.
 * 
 * Return value: A new CamelDigestStore widget.
 **/
CamelStore *
camel_digest_store_new (const char *url)
{
	CamelStore *store;
	CamelURL *uri;
	
	uri = camel_url_new (url, NULL);
	if (!uri)
		return NULL;
	
	store = CAMEL_STORE (camel_object_new (camel_digest_store_get_type ()));
	CAMEL_SERVICE (store)->url = uri;
	
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

static void
digest_init_junk (CamelStore *store)
{
	/* no-op */
	;
}

static CamelFolder *
digest_get_junk (CamelStore *store, CamelException *ex)
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
