/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Authors: Michael Zucchi <notzed@ximian.com>
 *
 * Copyright (C) 2001 Ximian Inc (www.ximian.com/)
 *
 * This program is free software; you can redistribute it and/or 
 * modify it under the terms of version 2 of the GNU General Public 
 * License as published by the Free Software Foundation.
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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <sys/stat.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <stdio.h>
#include <dirent.h>

#include "camel-spoold-store.h"
#include "camel-spool-folder.h"
#include "camel-exception.h"
#include "camel-url.h"
#include "camel-private.h"

#define d(x)

/* Returns the class for a CamelSpoolDStore */
#define CSPOOLS_CLASS(so) CAMEL_SPOOLD_STORE_CLASS (CAMEL_OBJECT_GET_CLASS(so))
#define CF_CLASS(so) CAMEL_FOLDER_CLASS (CAMEL_OBJECT_GET_CLASS(so))

static void construct (CamelService *service, CamelSession *session, CamelProvider *provider, CamelURL *url, CamelException *ex);
static CamelFolder *get_folder(CamelStore * store, const char *folder_name, guint32 flags, CamelException * ex);
static char *get_name(CamelService *service, gboolean brief);
static CamelFolder *get_inbox (CamelStore *store, CamelException *ex);
static void rename_folder(CamelStore *store, const char *old_name, const char *new_name, CamelException *ex);
static CamelFolderInfo *get_folder_info (CamelStore *store, const char *top, guint32 flags, CamelException *ex);
static void free_folder_info (CamelStore *store, CamelFolderInfo *fi);

static void delete_folder(CamelStore *store, const char *folder_name, CamelException *ex);
static void rename_folder(CamelStore *store, const char *old, const char *new, CamelException *ex);

static CamelStoreClass *parent_class = NULL;

static void
camel_spoold_store_class_init (CamelSpoolDStoreClass *camel_spoold_store_class)
{
	CamelStoreClass *camel_store_class = CAMEL_STORE_CLASS (camel_spoold_store_class);
	CamelServiceClass *camel_service_class = CAMEL_SERVICE_CLASS (camel_spoold_store_class);
	
	parent_class = CAMEL_STORE_CLASS (camel_type_get_global_classfuncs (camel_store_get_type ()));

	/* virtual method overload */
	camel_service_class->construct = construct;
	camel_service_class->get_name = get_name;
	camel_store_class->get_folder = get_folder;
	camel_store_class->get_inbox = get_inbox;
	camel_store_class->get_folder_info = get_folder_info;
	camel_store_class->free_folder_info = free_folder_info;

	camel_store_class->delete_folder = delete_folder;
	camel_store_class->rename_folder = rename_folder;
}

CamelType
camel_spoold_store_get_type (void)
{
	static CamelType camel_spoold_store_type = CAMEL_INVALID_TYPE;
	
	if (camel_spoold_store_type == CAMEL_INVALID_TYPE)	{
		camel_spoold_store_type = camel_type_register (CAMEL_SPOOL_STORE_TYPE, "CamelSpoolDStore",
							     sizeof (CamelSpoolDStore),
							     sizeof (CamelSpoolDStoreClass),
							     (CamelObjectClassInitFunc) camel_spoold_store_class_init,
							     NULL,
							     NULL,
							     NULL);
	}
	
	return camel_spoold_store_type;
}

static void
construct (CamelService *service, CamelSession *session, CamelProvider *provider, CamelURL *url, CamelException *ex)
{
	struct stat st;

	d(printf("constructing store of type %s '%s:%s'\n",
		 camel_type_to_name(((CamelObject *)service)->s.type), url->protocol, url->path));

	CAMEL_SERVICE_CLASS (parent_class)->construct (service, session, provider, url, ex);
	if (camel_exception_is_set (ex))
		return;

	if (service->url->path[0] != '/') {
		camel_exception_setv(ex, CAMEL_EXCEPTION_STORE_NO_FOLDER,
				     _("Store root %s is not an absolute path"), service->url->path);
		return;
	}

	if (stat(service->url->path, &st) == -1 || !S_ISDIR(st.st_mode)) {
		camel_exception_setv(ex, CAMEL_EXCEPTION_STORE_NO_FOLDER,
				     _("Store `%s' does not exist or is not a directory"),
				     service->url->path);
		return;
	}
}

const char *
camel_spoold_store_get_toplevel_dir (CamelSpoolDStore *store)
{
	CamelURL *url = CAMEL_SERVICE (store)->url;

	g_assert (url != NULL);
	return url->path;
}

static CamelFolder *
get_folder(CamelStore * store, const char *folder_name, guint32 flags, CamelException * ex)
{
	char *path = ((CamelService *)store)->url->path;
	CamelFolder *folder;


	path = alloca(strlen(((CamelService *)store)->url->path) + strlen(folder_name)+2);
	sprintf(path, "%s/%s", ((CamelService *)store)->url->path, folder_name);

	d(printf("opening folder %s on path %s\n", folder_name, path));

#if 0
	/* need to check path? */
	if (strcmp(folder_name, "INBOX") != 0) {
		camel_exception_setv(ex, CAMEL_EXCEPTION_STORE_NO_FOLDER,
				     _("Folder `%s/%s' does not exist."),
				     path, folder_name);
		return NULL;
	}
#endif
	folder = camel_spool_folder_new(store, folder_name, path, flags, ex);

	return folder;
}

static CamelFolder *
get_inbox(CamelStore *store, CamelException *ex)
{
	camel_exception_setv(ex, CAMEL_EXCEPTION_STORE_NO_FOLDER,
			     _("Store does not support an INBOX"));

	return NULL;
}

static char *
get_name (CamelService *service, gboolean brief)
{
	if (brief)
		return g_strdup (service->url->path);
	else
		return g_strdup_printf (_("Mail tree %s"), service->url->path);
}

static void free_folder_info (CamelStore *store, CamelFolderInfo *fi)
{
	if (fi) {
		g_free(fi->url);
		g_free(fi->name);
		g_free(fi->full_name);
		g_free(fi->path);
		g_free(fi);
	}
}


/* default implementation, rename all */
static void
rename_folder(CamelStore *store, const char *old, const char *new, CamelException *ex)
{
	camel_exception_setv(ex, CAMEL_EXCEPTION_SYSTEM,
			     _("Spool folders cannot be renamed"));
}

/* default implementation, only delete metadata */
static void
delete_folder(CamelStore *store, const char *folder_name, CamelException *ex)
{
	camel_exception_setv(ex, CAMEL_EXCEPTION_SYSTEM,
			     _("Spool folders cannot be deleted"));
}



static CamelFolderInfo *camel_folder_info_new(const char *url, const char *full, const char *name, int unread)
{
	CamelFolderInfo *fi;

	fi = g_malloc0(sizeof(*fi));
	fi->url = g_strdup(url);
	fi->full_name = g_strdup(full);
	fi->name = g_strdup(name);
	fi->unread_message_count = unread;
	camel_folder_info_build_path(fi, '/');

	d(printf("Adding spoold info: '%s' '%s' '%s' '%s'\n", fi->path, fi->name, fi->full_name, fi->url));

	return fi;
}

/* used to find out where we've visited already */
struct _inode {
	dev_t dnode;
	ino_t inode;
};

/* returns number of records found at or below this level */
static int scan_dir(CamelStore *store, GHashTable *visited, char *root, const char *path, guint32 flags, CamelFolderInfo *parent, CamelFolderInfo **fip, CamelException *ex)
{
	DIR *dir;
	struct dirent *d;
	char *name, *uri, *tmp, *fname;
	CamelFolderInfo *fi = NULL;
	struct stat st;
	CamelFolder *folder;
	int unread;
	char from[80];
	FILE *fp;

	d(printf("checking dir '%s' part '%s' for mbox content\n", root, path));

	/* look for folders matching the right structure, recursively */
	if (path) {
		name = alloca(strlen(root)+strlen(path)+2);
		sprintf(name, "%s/%s", root, path);
	} else
		name = root;

	dir = opendir(name);
	if (dir == NULL) {
		camel_exception_setv(ex, CAMEL_EXCEPTION_SYSTEM,
				     _("Could not scan folder `%s': %s"),
				     root, strerror(errno));
		g_free(name);
		return -1;
	}

	if (path != NULL) {
		uri = g_strdup_printf("spoold:%s;noselect=yes#%s", root, path);
		tmp = strrchr(path, '/');
		if (tmp == NULL)
			tmp = (char *)path;
		else
			tmp++;
		fi = camel_folder_info_new(uri, path, tmp, -1);
		fi->parent = parent;
		fi->sibling = *fip;
		*fip = fi;
		g_free(uri);
	
		fip = &fi->child;
		parent = fi;
	}
	
	while ( (d = readdir(dir)) ) {
		if (strcmp(d->d_name, ".") == 0
		    || strcmp(d->d_name, "..") == 0)
			continue;
		
		tmp = g_strdup_printf("%s/%s", name, d->d_name);
		if (stat(tmp, &st) == 0) {
			if (path)
				fname = g_strdup_printf("%s/%s", path, d->d_name);
			else
				fname = g_strdup(d->d_name);

			if (S_ISREG(st.st_mode)) {
				/* first, see if we already have it open */
				CAMEL_STORE_LOCK(store, cache_lock);
				folder = g_hash_table_lookup(store->folders, fname);
				if (folder)
					unread = camel_folder_get_unread_message_count(folder);
				else
					unread = -1;
				CAMEL_STORE_UNLOCK(store, cache_lock);

				/* no?  check its content to see if its a folder or not */
				if (folder == NULL) {
					fp = fopen(tmp, "r");
					if (fp != NULL) {
						if (fgets(from, sizeof(from), fp) != NULL
						    && strncmp(from, "From ", 5) == 0) {
							folder = (CamelFolder *)1;
							/* TODO: if slow mode selected, we could look up unread counts here -
							   but its pretty expensive */
						}
						fclose(fp);
					}
				}

				if (folder != NULL) {
					uri = g_strdup_printf("spoold:%s#%s", root, fname);
					fi = camel_folder_info_new(uri, fname, d->d_name, unread);
					fi->parent = parent;
					fi->sibling = *fip;
					*fip = fi;
					g_free(uri);
				}

			} else if (S_ISDIR(st.st_mode)) {
				struct _inode in = { st.st_dev, st.st_ino };
			
				/* see if we've visited already */
				if (g_hash_table_lookup(visited, &in) == NULL) {
					struct _inode *inew = g_malloc(sizeof(*inew));
				
					*inew = in;
					g_hash_table_insert(visited, inew, inew);

					if (scan_dir(store, visited, root, fname, flags, parent, fip, ex) == -1) {
						g_free(tmp);
						g_free(fname);
						closedir(dir);
						return -1;
					}
				}
			}
			g_free(fname);

		}
		g_free(tmp);
	}
	closedir(dir);

	return 0;
}

static guint inode_hash(const void *d)
{
	const struct _inode *v = d;

	return v->inode ^ v->dnode;
}

static gboolean inode_equal(const void *a, const void *b)
{
	const struct _inode *v1 = a, *v2 = b;
	
	return v1->inode == v2->inode && v1->dnode == v2->dnode;
}

static void inode_free(void *k, void *v, void *d)
{
	g_free(k);
}

static CamelFolderInfo *
get_folder_info (CamelStore *store, const char *top, guint32 flags, CamelException *ex)
{
	CamelFolderInfo *fi = NULL;
	GHashTable *visited;

	visited = g_hash_table_new(inode_hash, inode_equal);

	if (scan_dir(store, visited, ((CamelService *)store)->url->path, top, flags, NULL, &fi, ex) == -1 && fi != NULL) {
		camel_store_free_folder_info_full(store, fi);
		fi = NULL;
	}

	g_hash_table_foreach(visited, inode_free, NULL);
	g_hash_table_destroy(visited);

	return fi;
}




