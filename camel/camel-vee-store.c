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
static void vee_init_junk (CamelStore *store);
static CamelFolder *vee_get_junk  (CamelStore *store, CamelException *ex);

static CamelFolderInfo *vee_get_folder_info(CamelStore *store, const char *top, guint32 flags, CamelException *ex);

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
	
	camel_vee_store_parent = (CamelStoreClass *)camel_store_get_type();

	/* virtual method overload */
	store_class->get_folder = vee_get_folder;
	store_class->rename_folder = vee_rename_folder;
	store_class->delete_folder = vee_delete_folder;
	store_class->get_folder_info = vee_get_folder_info;
	store_class->free_folder_info = camel_store_free_folder_info_full;

	store_class->init_trash = vee_init_trash;
	store_class->get_trash = vee_get_trash;
	store_class->init_junk = vee_init_junk;
	store_class->get_junk = vee_get_junk;
}

static void
camel_vee_store_init (CamelVeeStore *obj)
{
	CamelStore *store = (CamelStore *)obj;

	/* we dont want a vtrash/vjunk on this one */
	store->flags &= ~(CAMEL_STORE_VTRASH | CAMEL_STORE_VJUNK);	
}

static void
camel_vee_store_finalise (CamelObject *obj)
{
	;
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

/* flags
   1 = delete (0 = add)
   2 = noselect
*/
#define CHANGE_ADD (0)
#define CHANGE_DELETE (1)
#define CHANGE_NOSELECT (2)

static void
change_folder(CamelStore *store, const char *name, guint32 flags, int count)
{
	CamelFolderInfo *fi;
	const char *tmp;

	fi = g_malloc0(sizeof(*fi));
	fi->full_name = g_strdup(name);
	tmp = strrchr(name, '/');
	if (tmp == NULL)
		tmp = name;
	else
		tmp++;
	fi->name = g_strdup(tmp);
	fi->url = g_strdup_printf("vfolder:%s%s#%s", ((CamelService *)store)->url->path, (flags&CHANGE_NOSELECT)?";noselect=yes":"", name);
	fi->unread_message_count = count;
	camel_folder_info_build_path(fi, '/');
	camel_object_trigger_event(store, (flags&CHANGE_DELETE)?"folder_deleted":"folder_created", fi);
	camel_folder_info_free(fi);
}

static CamelFolder *
vee_get_folder (CamelStore *store, const char *folder_name, guint32 flags, CamelException *ex)
{
	CamelVeeFolder *vf;
	CamelFolder *folder;
	char *name, *p;

	vf = (CamelVeeFolder *)camel_vee_folder_new(store, folder_name, flags);
	if ((vf->flags & CAMEL_STORE_FOLDER_PRIVATE) == 0) {
		/* Check that parents exist, if not, create dummy ones */
		name = alloca(strlen(vf->vname)+1);
		strcpy(name, vf->vname);
		p = name;
		while ( (p = strchr(p, '/'))) {
			*p = 0;

			folder = camel_object_bag_get(store->folders, name);
			if (folder == NULL)
				change_folder(store, name, CHANGE_ADD|CHANGE_NOSELECT, -1);
			else
				camel_object_unref(folder);
			*p++='/';
		}

		change_folder(store, vf->vname, CHANGE_ADD, camel_folder_get_message_count((CamelFolder *)vf));
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

static void
vee_init_junk (CamelStore *store)
{
	/* no-op */
	;
}

static CamelFolder *
vee_get_junk (CamelStore *store, CamelException *ex)
{
	return NULL;
}

static CamelFolderInfo *
vee_get_folder_info(CamelStore *store, const char *top, guint32 flags, CamelException *ex)
{
	CamelFolderInfo *info;
	GPtrArray *folders, *infos;
	int i;

	infos = g_ptr_array_new();
	folders = camel_object_bag_list(store->folders);
	for (i=0;i<folders->len;i++) {
		CamelVeeFolder *folder = folders->pdata[i];
		int add = FALSE;
		char *name = ((CamelFolder *)folder)->full_name;

		/* check we have to include this one */
		if (top) {
			if (flags & CAMEL_STORE_FOLDER_INFO_RECURSIVE) {
				int namelen = strlen(name);
				int toplen = strlen(top);

				add = ((namelen == toplen &&
					strcmp(name, top) == 0)
				       || ((namelen > toplen)
					   && strncmp(name, top, toplen) == 0
					   && name[toplen] == '/'));
			} else {
				add = strcmp(name, top) == 0;
			}
		} else {
			if ((flags & CAMEL_STORE_FOLDER_INFO_RECURSIVE) == 0)
				add = strchr(name, '/') == NULL;
		}

		if (add) {
			/* ensures unread is correct */
			if ((flags & CAMEL_STORE_FOLDER_INFO_FAST) == 0)
				camel_folder_refresh_info((CamelFolder *)folder, NULL);

			info = g_malloc0(sizeof(*info));
			info->url = g_strdup_printf("vfolder:%s#%s", ((CamelService *)((CamelFolder *)folder)->parent_store)->url->path,
						    ((CamelFolder *)folder)->full_name);
			info->full_name = g_strdup(((CamelFolder *)folder)->full_name);
			info->name = g_strdup(((CamelFolder *)folder)->name);
			info->unread_message_count = camel_folder_get_unread_message_count((CamelFolder *)folder);
			g_ptr_array_add(infos, info);
		}
		camel_object_unref(folder);
	}
	g_ptr_array_free(folders, TRUE);

	/* and always add UNMATCHED, if scanning from top/etc */
	if (top == NULL || top[0] == 0 || strncmp(top, CAMEL_UNMATCHED_NAME, strlen(CAMEL_UNMATCHED_NAME)) == 0) {
		info = g_malloc0(sizeof(*info));
		info->url = g_strdup_printf("vfolder:%s#%s", ((CamelService *)store)->url->path, CAMEL_UNMATCHED_NAME);
		info->full_name = g_strdup(CAMEL_UNMATCHED_NAME);
		info->name = g_strdup(CAMEL_UNMATCHED_NAME);
		info->unread_message_count = -1;
		camel_folder_info_build_path(info, '/');
		g_ptr_array_add(infos, info);
	}
		
	/* convert it into a tree */
	info = camel_folder_info_build(infos, (top&&top[0])?top:"", '/', TRUE);
	g_ptr_array_free(infos, TRUE);

	return info;
}

static void
vee_delete_folder(CamelStore *store, const char *folder_name, CamelException *ex)
{
	CamelFolder *folder;

	if (strcmp(folder_name, CAMEL_UNMATCHED_NAME) == 0) {
		camel_exception_setv(ex, CAMEL_EXCEPTION_STORE_NO_FOLDER,
				     _("Cannot delete folder: %s: Invalid operation"), folder_name);
		return;
	}

	folder = camel_object_bag_get(store->folders, folder_name);
	if (folder) {
		camel_object_bag_remove(store->folders, folder);

		if (store->vtrash)
			camel_vee_folder_remove_folder((CamelVeeFolder *)store->vtrash, folder);
		if (store->vjunk)
			camel_vee_folder_remove_folder((CamelVeeFolder *)store->vjunk, folder);

		if ((((CamelVeeFolder *)folder)->flags & CAMEL_STORE_FOLDER_PRIVATE) == 0) {
			/* what about now-empty parents?  ignore? */
			change_folder(store, folder_name, CHANGE_DELETE, -1);
		}

		camel_object_unref(folder);
	} else {
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
	folder = camel_object_bag_get(store->folders, old);
	if (folder == NULL) {
		camel_exception_setv(ex, CAMEL_EXCEPTION_STORE_NO_FOLDER,
				     _("Cannot rename folder: %s: No such folder"), old);
	} else {
		camel_object_unref(folder);
	}
}
