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

static void vee_sync (CamelStore *store, int expunge, CamelException *ex);
static CamelFolder *vee_get_trash  (CamelStore *store, CamelException *ex);
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

	store_class->sync = vee_sync;
	store_class->get_trash = vee_get_trash;
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
	CamelURL *url;

	fi = g_malloc0(sizeof(*fi));
	fi->full_name = g_strdup(name);
	tmp = strrchr(name, '/');
	if (tmp == NULL)
		tmp = name;
	else
		tmp++;
	fi->name = g_strdup(tmp);
	url = camel_url_new("vfolder:", 0);
	camel_url_set_path(url, ((CamelService *)store)->url->path);
	if (flags & CHANGE_NOSELECT)
		camel_url_set_param(url, "noselect", "yes");
	camel_url_set_fragment(url, name);
	fi->uri = camel_url_to_string(url, 0);
	camel_url_free(url);
	/*fi->url = g_strdup_printf("vfolder:%s%s#%s", ((CamelService *)store)->url->path, (flags&CHANGE_NOSELECT)?";noselect=yes":"", name);*/
	fi->unread = count;
	fi->flags = CAMEL_FOLDER_VIRTUAL;
	if (!(flags & CHANGE_DELETE))
		fi->flags |= CAMEL_FOLDER_NOCHILDREN;
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

			folder = camel_object_bag_reserve(store->folders, name);
			if (folder == NULL) {
				/* create a dummy vFolder for this, makes get_folder_info simpler */
				folder = camel_vee_folder_new(store, name, flags);
				camel_object_bag_add(store->folders, name, folder);
				change_folder(store, name, CHANGE_ADD|CHANGE_NOSELECT, 0);
				/* FIXME: this sort of leaks folder, nobody owns a ref to it but us */
			} else {
				camel_object_unref(folder);
			}
			*p++='/';
		}

		change_folder(store, vf->vname, CHANGE_ADD, camel_folder_get_message_count((CamelFolder *)vf));
	}

	return (CamelFolder *)vf;
}

static void
vee_sync(CamelStore *store, int expunge, CamelException *ex)
{
	/* noop */;
}

static CamelFolder *
vee_get_trash (CamelStore *store, CamelException *ex)
{
	return NULL;
}

static CamelFolder *
vee_get_junk (CamelStore *store, CamelException *ex)
{
	return NULL;
}

static int
vee_folder_cmp(const void *ap, const void *bp)
{
	return strcmp(((CamelFolder **)ap)[0]->full_name, ((CamelFolder **)bp)[0]->full_name);
}

static CamelFolderInfo *
vee_get_folder_info(CamelStore *store, const char *top, guint32 flags, CamelException *ex)
{
	CamelFolderInfo *info, *res = NULL, *tail;
	GPtrArray *folders;
	GHashTable *infos_hash;
	CamelURL *url;
	int i;

	d(printf("Get folder info '%s'\n", top?top:"<null>"));

	infos_hash = g_hash_table_new(g_str_hash, g_str_equal);
	folders = camel_object_bag_list(store->folders);
	qsort(folders->pdata, folders->len, sizeof(folders->pdata[0]), vee_folder_cmp);
	for (i=0;i<folders->len;i++) {
		CamelVeeFolder *folder = folders->pdata[i];
		int add = FALSE;
		char *name = ((CamelFolder *)folder)->full_name, *pname, *tmp;
		CamelFolderInfo *pinfo;

		d(printf("folder '%s'\n", name));

		/* check we have to include this one */
		if (top) {
			int namelen = strlen(name);
			int toplen = strlen(top);

			add = ((namelen == toplen
				&& strcmp(name, top) == 0)
			       || ((namelen > toplen)
				   && strncmp(name, top, toplen) == 0
				   && name[toplen] == '/'
				   && ((flags & CAMEL_STORE_FOLDER_INFO_RECURSIVE)
				       || strchr(name+toplen+1, '/') == NULL)));
		} else {
			add = (flags & CAMEL_STORE_FOLDER_INFO_RECURSIVE)
				|| strchr(name, '/') == NULL;
		}

		d(printf("%sadding '%s'\n", add?"":"not ", name));

		if (add) {
			/* ensures unread is correct */
			if ((flags & CAMEL_STORE_FOLDER_INFO_FAST) == 0)
				camel_folder_refresh_info((CamelFolder *)folder, NULL);

			info = g_malloc0(sizeof(*info));
			url = camel_url_new("vfolder:", NULL);
			camel_url_set_path(url, ((CamelService *)((CamelFolder *)folder)->parent_store)->url->path);
			camel_url_set_fragment(url, ((CamelFolder *)folder)->full_name);
			info->uri = camel_url_to_string(url, 0);
			camel_url_free(url);
/*
			info->url = g_strdup_printf("vfolder:%s#%s", ((CamelService *)((CamelFolder *)folder)->parent_store)->url->path,
			((CamelFolder *)folder)->full_name);*/
			info->full_name = g_strdup(((CamelFolder *)folder)->full_name);
			info->name = g_strdup(((CamelFolder *)folder)->name);
			info->unread = camel_folder_get_unread_message_count((CamelFolder *)folder);
			info->flags = CAMEL_FOLDER_NOCHILDREN|CAMEL_FOLDER_VIRTUAL;
			camel_folder_info_build_path(info, '/');
			g_hash_table_insert(infos_hash, info->full_name, info);

			if (res == NULL)
				res = info;
		} else {
			info = NULL;
		}

		/* check for parent, if present, update flags and if adding, update parent linkage */
		pname = g_strdup(((CamelFolder *)folder)->full_name);
		d(printf("looking up parent of '%s'\n", pname));
		tmp = strrchr(pname, '/');
		if (tmp) {
			*tmp = 0;
			pinfo = g_hash_table_lookup(infos_hash, pname);
		} else
			pinfo = NULL;

		if (pinfo) {
			pinfo->flags = (pinfo->flags & ~(CAMEL_FOLDER_CHILDREN|CAMEL_FOLDER_NOCHILDREN))|CAMEL_FOLDER_CHILDREN;
			d(printf("updating parent flags for children '%s' %08x\n", pinfo->full_name, pinfo->flags));
			tail = pinfo->child;
			if (tail == NULL)
				pinfo->child = info;
		} else if (info != res) {
			tail = res;
		} else {
			tail = NULL;
		}

		if (info && tail) {
			while (tail->next)
				tail = tail->next;
			tail->next = info;
			info->parent = pinfo;
		}

		g_free(pname);
		camel_object_unref(folder);
	}
	g_ptr_array_free(folders, TRUE);
	g_hash_table_destroy(infos_hash);

	/* and always add UNMATCHED, if scanning from top/etc */
	if (top == NULL || top[0] == 0 || strncmp(top, CAMEL_UNMATCHED_NAME, strlen(CAMEL_UNMATCHED_NAME)) == 0) {
		info = g_malloc0(sizeof(*info));
		url = camel_url_new("vfolder:", NULL);
		camel_url_set_path(url, ((CamelService *)store)->url->path);
		camel_url_set_fragment(url, CAMEL_UNMATCHED_NAME);
		info->uri = camel_url_to_string(url, 0);
		camel_url_free(url);
		/*info->url = g_strdup_printf("vfolder:%s#%s", ((CamelService *)store)->url->path, CAMEL_UNMATCHED_NAME);*/
		info->full_name = g_strdup(CAMEL_UNMATCHED_NAME);
		info->name = g_strdup(CAMEL_UNMATCHED_NAME);
		info->unread = -1;
		info->flags = CAMEL_FOLDER_NOCHILDREN|CAMEL_FOLDER_NOINFERIORS|CAMEL_FOLDER_SYSTEM|CAMEL_FOLDER_VIRTUAL;
		camel_folder_info_build_path(info, '/');

		if (res == NULL)
			res = info;
		else {
			tail = res;
			while (tail->next)
				tail = tail->next;
			tail->next = info;
		}
	}

	return res;
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
		char *statefile;

		camel_object_get(folder, NULL, CAMEL_OBJECT_STATE_FILE, &statefile, NULL);
		if (statefile) {
			unlink(statefile);
			camel_object_free(folder, CAMEL_OBJECT_STATE_FILE, statefile);
			camel_object_set(folder, NULL, CAMEL_OBJECT_STATE_FILE, NULL, NULL);
		}

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
