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

static CamelFolder *vee_get_folder (CamelStore *store, const char *folder_name, gboolean create, CamelException *ex);
static char *vee_get_folder_name (CamelStore *store, const char *folder_name, CamelException *ex);

struct _CamelVeeStorePrivate {
};

#define _PRIVATE(o) (((CamelVeeStore *)(o))->priv)

static void camel_vee_store_class_init (CamelVeeStoreClass *klass);
static void camel_vee_store_init       (CamelVeeStore *obj);
static void camel_vee_store_finalise   (GtkObject *obj);

static CamelStoreClass *camel_vee_store_parent;

enum SIGNALS {
	LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = { 0 };

guint
camel_vee_store_get_type (void)
{
	static guint type = 0;
	
	if (!type) {
		GtkTypeInfo type_info = {
			"CamelVeeStore",
			sizeof (CamelVeeStore),
			sizeof (CamelVeeStoreClass),
			(GtkClassInitFunc) camel_vee_store_class_init,
			(GtkObjectInitFunc) camel_vee_store_init,
			(GtkArgSetFunc) NULL,
			(GtkArgGetFunc) NULL
		};
		
		type = gtk_type_unique (camel_store_get_type (), &type_info);
	}
	
	return type;
}

static void

camel_vee_store_class_init (CamelVeeStoreClass *klass)
{
	GtkObjectClass *object_class = (GtkObjectClass *) klass;
	CamelStoreClass *store_class = (CamelStoreClass *) klass;
	
	camel_vee_store_parent = gtk_type_class (camel_store_get_type ());

	/* virtual method overload */
	store_class->get_folder = vee_get_folder;
	store_class->get_folder_name = vee_get_folder_name;

	object_class->finalize = camel_vee_store_finalise;

	gtk_object_class_add_signals (object_class, signals, LAST_SIGNAL);
}

static void
camel_vee_store_init (CamelVeeStore *obj)
{
	struct _CamelVeeStorePrivate *p;

	p = _PRIVATE(obj) = g_malloc0(sizeof(*p));
}

static void
camel_vee_store_finalise (GtkObject *obj)
{
	((GtkObjectClass *)(camel_vee_store_parent))->finalize((GtkObject *)obj);
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
	CamelVeeStore *new = CAMEL_VEE_STORE ( gtk_type_new (camel_vee_store_get_type ()));
	return new;
}

static CamelFolder *
vee_get_folder (CamelStore *store, const char *folder_name, gboolean create, CamelException *ex)
{
	CamelFolder *folder;

	folder =  gtk_type_new (camel_vee_folder_get_type());

	((CamelFolderClass *)((GtkObject *)folder)->klass)->init (folder, store, NULL, folder_name, "/", TRUE, ex);
	return folder;
}

static char *
vee_get_folder_name (CamelStore *store, const char *folder_name, CamelException *ex)
{
	return g_strdup(folder_name);
}

