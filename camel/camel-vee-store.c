/*
 *  Copyright (C) 2000 Ximian Inc.
 *
 *  Authors: Michael Zucchi <notzed@ximian.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of version 2 of the GNU General Public
 * License as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "camel-exception.h"
#include "camel-vee-store.h"
#include "camel-vee-folder.h"

#include "camel-private.h"

#include <string.h>

#define d(x)

static CamelFolder *vee_get_folder (CamelStore *store, const char *folder_name, guint32 flags, CamelException *ex);
static void vee_delete_folder(CamelStore *store, const char *folder_name, CamelException *ex);
static void vee_rename_folder(CamelStore *store, const char *old, const char *new, CamelException *ex);
static void vee_init_trash (CamelStore *store);
static CamelFolder *vee_get_trash  (CamelStore *store, CamelException *ex);

static CamelFolderInfo *vee_get_folder_info(CamelStore *store, const char *top, guint32 flags, CamelException *ex);

struct _CamelVeeStorePrivate {
};

#define _PRIVATE(o) (((CamelVeeStore *)(o))->priv)

static void camel_vee_store_class_init (CamelVeeStoreClass *klass);
static void camel_vee_store_init       (CamelVeeStore *obj);
static void camel_vee_store_finalise   (CamelObject *obj);

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
					    (CamelObjectFinalizeFunc) camel_vee_store_finalise);
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
	store_class->rename_folder = vee_rename_folder;
	store_class->delete_folder = vee_delete_folder;
	store_class->get_folder_info = vee_get_folder_info;
	store_class->free_folder_info = camel_store_free_folder_info_full;

	store_class->init_trash = vee_init_trash;
	store_class->get_trash = vee_get_trash;
}

static void
camel_vee_store_init (CamelVeeStore *obj)
{
	struct _CamelVeeStorePrivate *p;
	CamelStore *store = (CamelStore *)obj;

	/* we dont want a vtrash on this one */
	store->flags &= ~(CAMEL_STORE_VTRASH);	

	p = _PRIVATE(obj) = g_malloc0(sizeof(*p));
}

static void
camel_vee_store_finalise (CamelObject *obj)
{
	CamelVeeStore *vs = (CamelVeeStore *)obj;

	g_free(vs->priv);
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
	CamelVeeStore *new = CAMEL_VEE_STORE(camel_object_new(camel_vee_store_get_type ()));
	return new;
}

static CamelFolder *
vee_get_folder (CamelStore *store, const char *folder_name, guint32 flags, CamelException *ex)
{
	CamelFolderInfo *fi;
	CamelVeeFolder *vf;
	char *name;

	vf = (CamelVeeFolder *)camel_vee_folder_new(store, folder_name, flags);
	if ((vf->flags & CAMEL_STORE_FOLDER_PRIVATE) == 0) {
		fi = g_malloc0(sizeof(*fi));
		fi->full_name = g_strdup(vf->vname);
		name = strrchr(vf->vname, '/');
		if (name == NULL)
			name = vf->vname;
		fi->name = g_strdup(name);
		fi->url = g_strdup_printf("vfolder:%s#%s", ((CamelService *)store)->url->path,
					  ((CamelFolder *)vf)->full_name);
		fi->unread_message_count = camel_folder_get_message_count((CamelFolder *)vf);
		camel_folder_info_build_path(fi, '/');
		camel_object_trigger_event(CAMEL_OBJECT(store), "folder_created", fi);
		camel_folder_info_free(fi);
	}

	return (CamelFolder *)vf;
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

struct _build_info {
	const char *top;
	guint32 flags;
	GPtrArray *infos;
	GPtrArray *folders;
};

static void
build_info(char *name, CamelVeeFolder *folder, struct _build_info *data)
{
	CamelFolderInfo *info;

	/* check we have to include this one */
	if (data->top) {
		if (data->flags & CAMEL_STORE_FOLDER_INFO_RECURSIVE) {
			int namelen = strlen(name);
			int toplen = strlen(data->top);

			if (!((namelen == toplen &&
			       strcmp(name, data->top) == 0)
			      || ((namelen > toplen)
				  && strncmp(name, data->top, toplen) == 0
				  && name[toplen] == '/')))
				return;
		} else {
			if (strcmp(name, data->top))
				return;
		}
	} else {
		if ((data->flags & CAMEL_STORE_FOLDER_INFO_RECURSIVE) == 0) {
			if (strchr(name, '/'))
				return;
		}
	}

	info = g_malloc0(sizeof(*info));
	info->url = g_strdup_printf("vfolder:%s#%s", ((CamelService *)((CamelFolder *)folder)->parent_store)->url->path,
				    ((CamelFolder *)folder)->full_name);
	info->full_name = g_strdup(((CamelFolder *)folder)->full_name);
	info->name = g_strdup(((CamelFolder *)folder)->name);
	info->unread_message_count = -1;
	g_ptr_array_add(data->infos, info);
	camel_object_ref((CamelObject *)folder);
	g_ptr_array_add(data->folders, folder);
}

static CamelFolderInfo *
vee_get_folder_info(CamelStore *store, const char *top, guint32 flags, CamelException *ex)
{
	struct _build_info data;
	CamelFolderInfo *info;
	int i;

	/* first, build the info list */
	data.top = top;
	data.flags = flags;
	data.infos = g_ptr_array_new();
	data.folders = g_ptr_array_new();
	CAMEL_STORE_LOCK(store, cache_lock);
	g_hash_table_foreach(store->folders, (GHFunc)build_info, &data);
	CAMEL_STORE_UNLOCK(store, cache_lock);

	/* then make sure the unread counts are accurate */
	for (i=0;i<data.infos->len;i++) {
		CamelFolderInfo *info = data.infos->pdata[i];
		CamelFolder *folder = data.folders->pdata[i];

		camel_folder_refresh_info(folder, NULL);
		info->unread_message_count = camel_folder_get_unread_message_count(folder);
		camel_object_unref((CamelObject *)folder);
	}
	g_ptr_array_free(data.folders, TRUE);

	/* and always add UNMATCHED, if scanning from top/etc */
	if (top == NULL || top[0] == 0 || strncmp(top, CAMEL_UNMATCHED_NAME, strlen(CAMEL_UNMATCHED_NAME)) == 0) {
		info = g_malloc0(sizeof(*info));
		info->url = g_strdup_printf("vfolder:%s#%s", ((CamelService *)store)->url->path, CAMEL_UNMATCHED_NAME);
		info->full_name = g_strdup(CAMEL_UNMATCHED_NAME);
		info->name = g_strdup(CAMEL_UNMATCHED_NAME);
		info->unread_message_count = -1;
		camel_folder_info_build_path(info, '/');
		g_ptr_array_add(data.infos, info);
	}
		
	/* convert it into a tree */
	info = camel_folder_info_build(data.infos, (top&&top[0])?top:"", '/', TRUE);
	g_ptr_array_free(data.infos, TRUE);

	return info;
}

static void
vee_delete_folder(CamelStore *store, const char *folder_name, CamelException *ex)
{
	CamelFolder *folder;
	char *key;

	if (strcmp(folder_name, CAMEL_UNMATCHED_NAME) == 0) {
		camel_exception_setv(ex, CAMEL_EXCEPTION_STORE_NO_FOLDER,
				     _("Cannot delete folder: %s: Invalid operation"), folder_name);
		return;
	}

	CAMEL_STORE_LOCK(store, cache_lock);
	if (g_hash_table_lookup_extended(store->folders, folder_name, (void **)&key, (void **)&folder)) {
		int update;

		update = (((CamelVeeFolder *)folder)->flags & CAMEL_STORE_FOLDER_PRIVATE) == 0;
		g_hash_table_remove(store->folders, key);
		CAMEL_STORE_UNLOCK(store, cache_lock);
		if (store->vtrash)
			camel_vee_folder_remove_folder((CamelVeeFolder *)store->vtrash, folder);

		if (update) {
			CamelFolderInfo *fi = g_malloc0(sizeof(*fi));

			fi->full_name = g_strdup(key);
			fi->name = strrchr(key, '/');
			if (fi->name == NULL)
				fi->name = g_strdup(key);
			else
				fi->name = g_strdup(fi->name);
			fi->url = g_strdup_printf("vfolder:%s#%s", ((CamelService *)store)->url->path, key);
			fi->unread_message_count = -1;
			camel_folder_info_build_path(fi, '/');
	
			camel_object_trigger_event(CAMEL_OBJECT(store), "folder_deleted", fi);
			camel_folder_info_free(fi);
		}
		g_free(key);
	} else {
		CAMEL_STORE_UNLOCK(store, cache_lock);

		camel_exception_setv(ex, CAMEL_EXCEPTION_STORE_NO_FOLDER,
				     _("Cannot delete folder: %s: No such folder"), folder_name);
	}
}

static void
vee_rename_folder(CamelStore *store, const char *old, const char *new, CamelException *ex)
{
	CamelFolder *folder;

	d(printf("vee rename folder '%s' '%s'\n", old, new));

	if (strcmp(old, CAMEL_UNMATCHED_NAME) == 0) {
		camel_exception_setv(ex, CAMEL_EXCEPTION_STORE_NO_FOLDER,
				     _("Cannot rename folder: %s: Invalid operation"), old);
		return;
	}

	/* See if it exists, for vfolders, all folders are in the folders hash */
	CAMEL_STORE_LOCK(store, cache_lock);
	if ((folder = g_hash_table_lookup(store->folders, old)) == NULL) {
		camel_exception_setv(ex, CAMEL_EXCEPTION_STORE_NO_FOLDER,
				     _("Cannot rename folder: %s: No such folder"), old);
	}

	CAMEL_STORE_UNLOCK(store, cache_lock);
}
