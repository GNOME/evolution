/*
 *  Copyright (C) 2000 Helix Code Inc.
 *
 *  Authors: Michael Zucchi <notzed@helixcode.com>
 *
 *  This program is free software; you can redistribute it and/or 
 *  modify it under the terms of the GNU General Public License as 
 *  published by the Free Software Foundation; either version 2 of the
 *  License, or (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307
 *  USA
 */

#include "camel-vee-store.h"
#include "camel-vee-folder.h"

static CamelFolder *vee_get_folder (CamelStore *store, const char *folder_name, guint32 flags, CamelException *ex);
static void vee_init_trash (CamelStore *store);
static CamelFolder *vee_get_trash  (CamelStore *store, CamelException *ex);

struct _CamelVeeStorePrivate {
};

#define _PRIVATE(o) (((CamelVeeStore *)(o))->priv)

static void camel_vee_store_class_init (CamelVeeStoreClass *klass);
static void camel_vee_store_init       (CamelVeeStore *obj);

static CamelStoreClass *camel_vee_store_parent;

CamelType
camel_vee_store_get_type (void)
{
	static CamelType type = CAMEL_INVALID_TYPE;
	
	if (type == CAMEL_INVALID_TYPE) {
		type = camel_type_register (camel_store_get_type (), "CamelVeeStore",
					    sizeof (CamelVeeStore),
					    sizeof (CamelVeeStoreClass),
					    (CamelObjectClassInitFunc) camel_vee_store_class_init,
					    NULL,
					    (CamelObjectInitFunc) camel_vee_store_init,
					    NULL);
	}
	
	return type;
}

static void

camel_vee_store_class_init (CamelVeeStoreClass *klass)
{
	CamelStoreClass *store_class = (CamelStoreClass *) klass;
	
	camel_vee_store_parent = CAMEL_STORE_CLASS(camel_type_get_global_classfuncs (camel_store_get_type ()));

	/* virtual method overload */
	store_class->get_folder = vee_get_folder;
	store_class->init_trash = vee_init_trash;
	store_class->get_trash = vee_get_trash;
}

static void
camel_vee_store_init (CamelVeeStore *obj)
{
	struct _CamelVeeStorePrivate *p;

	p = _PRIVATE(obj) = g_malloc0(sizeof(*p));
}

/**
 * camel_vee_store_new:
 *
 * Create a new CamelVeeStore object.
 * 
 * Return value: A new CamelVeeStore widget.
 **/
CamelVeeStore *
camel_vee_store_new (void)
{
	CamelVeeStore *new = CAMEL_VEE_STORE ( camel_object_new (camel_vee_store_get_type ()));
	return new;
}

static CamelFolder *
vee_get_folder (CamelStore *store, const char *folder_name, guint32 flags, CamelException *ex)
{
	return camel_vee_folder_new(store, folder_name, flags, ex);
}

static void
vee_init_trash (CamelStore *store)
{
	/* no-op */
	;
}

static CamelFolder *
vee_get_trash (CamelStore *store, CamelException *ex)
{
	return NULL;
}
