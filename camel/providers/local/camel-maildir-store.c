/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2000 Ximian, Inc.
 *
 * Authors: Michael Zucchi <notzed@ximian.com>
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

#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>

#include <dirent.h>

#include "camel-maildir-store.h"
#include "camel-maildir-folder.h"
#include "camel-exception.h"
#include "camel-url.h"
#include "camel-private.h"
#include "camel-maildir-summary.h"
#include "camel-i18n.h"

#define d(x)

static CamelLocalStoreClass *parent_class = NULL;

/* Returns the class for a CamelMaildirStore */
#define CMAILDIRS_CLASS(so) CAMEL_MAILDIR_STORE_CLASS (CAMEL_OBJECT_GET_CLASS(so))
#define CF_CLASS(so) CAMEL_FOLDER_CLASS (CAMEL_OBJECT_GET_CLASS(so))
#define CMAILDIRF_CLASS(so) CAMEL_MAILDIR_FOLDER_CLASS (CAMEL_OBJECT_GET_CLASS(so))

static CamelFolder *get_folder(CamelStore * store, const char *folder_name, guint32 flags, CamelException * ex);
static CamelFolder *get_inbox (CamelStore *store, CamelException *ex);
static void delete_folder(CamelStore * store, const char *folder_name, CamelException * ex);
static void maildir_rename_folder(CamelStore *store, const char *old, const char *new, CamelException *ex);

static CamelFolderInfo * get_folder_info (CamelStore *store, const char *top, guint32 flags, CamelException *ex);

static void camel_maildir_store_class_init(CamelObjectClass * camel_maildir_store_class)
{
	CamelStoreClass *camel_store_class = CAMEL_STORE_CLASS(camel_maildir_store_class);
	/*CamelServiceClass *camel_service_class = CAMEL_SERVICE_CLASS(camel_maildir_store_class);*/

	parent_class = (CamelLocalStoreClass *)camel_type_get_global_classfuncs(camel_local_store_get_type());

	/* virtual method overload, use defaults for most */
	camel_store_class->get_folder = get_folder;
	camel_store_class->get_inbox = get_inbox;
	camel_store_class->delete_folder = delete_folder;
	camel_store_class->rename_folder = maildir_rename_folder;

	camel_store_class->get_folder_info = get_folder_info;
	camel_store_class->free_folder_info = camel_store_free_folder_info_full;
}

CamelType camel_maildir_store_get_type(void)
{
	static CamelType camel_maildir_store_type = CAMEL_INVALID_TYPE;

	if (camel_maildir_store_type == CAMEL_INVALID_TYPE) {
		camel_maildir_store_type = camel_type_register(CAMEL_LOCAL_STORE_TYPE, "CamelMaildirStore",
							  sizeof(CamelMaildirStore),
							  sizeof(CamelMaildirStoreClass),
							  (CamelObjectClassInitFunc) camel_maildir_store_class_init,
							  NULL,
							  NULL,
							  NULL);
	}

	return camel_maildir_store_type;
}

static CamelFolder *
get_folder(CamelStore * store, const char *folder_name, guint32 flags, CamelException * ex)
{
	char *name, *tmp, *cur, *new;
	struct stat st;
	CamelFolder *folder = NULL;

	if (!((CamelStoreClass *)parent_class)->get_folder(store, folder_name, flags, ex))
		return NULL;

	name = g_strdup_printf("%s%s", CAMEL_LOCAL_STORE(store)->toplevel_dir, folder_name);
	tmp = g_strdup_printf("%s/tmp", name);
	cur = g_strdup_printf("%s/cur", name);
	new = g_strdup_printf("%s/new", name);

	if (stat(name, &st) == -1) {
		if (errno != ENOENT) {
			camel_exception_setv (ex, CAMEL_EXCEPTION_SYSTEM,
					      _("Cannot get folder `%s': %s"),
					      folder_name, g_strerror (errno));
		} else if ((flags & CAMEL_STORE_FOLDER_CREATE) == 0) {
			camel_exception_setv (ex, CAMEL_EXCEPTION_STORE_NO_FOLDER,
					      _("Cannot get folder `%s': folder does not exist."),
					      folder_name);
		} else {
			if (mkdir(name, 0700) != 0
			    || mkdir(tmp, 0700) != 0
			    || mkdir(cur, 0700) != 0
			    || mkdir(new, 0700) != 0) {
				camel_exception_setv (ex, CAMEL_EXCEPTION_SYSTEM,
						      _("Cannot create folder `%s': %s"),
						      folder_name, g_strerror (errno));
				rmdir(tmp);
				rmdir(cur);
				rmdir(new);
				rmdir(name);
			} else {
				folder = camel_maildir_folder_new(store, folder_name, flags, ex);
			}
		}
	} else if (!S_ISDIR(st.st_mode)
		   || stat(tmp, &st) != 0 || !S_ISDIR(st.st_mode)
		   || stat(cur, &st) != 0 || !S_ISDIR(st.st_mode)
		   || stat(new, &st) != 0 || !S_ISDIR(st.st_mode)) {
		camel_exception_setv(ex, CAMEL_EXCEPTION_SYSTEM,
				     _("Cannot get folder `%s': not a maildir directory."), name);
	} else if (flags & CAMEL_STORE_FOLDER_EXCL) {
		camel_exception_setv (ex, CAMEL_EXCEPTION_SYSTEM,
				      _("Cannot create folder `%s': folder exists."),
				      folder_name);
	} else {
		folder = camel_maildir_folder_new(store, folder_name, flags, ex);
	}

	g_free(name);
	g_free(tmp);
	g_free(cur);
	g_free(new);

	return folder;
}

static CamelFolder *
get_inbox (CamelStore *store, CamelException *ex)
{
	return get_folder (store, ".", 0, ex);
}

static void delete_folder(CamelStore * store, const char *folder_name, CamelException * ex)
{
	char *name, *tmp, *cur, *new;
	struct stat st;

	name = g_strdup_printf("%s%s", CAMEL_LOCAL_STORE(store)->toplevel_dir, folder_name);

	tmp = g_strdup_printf("%s/tmp", name);
	cur = g_strdup_printf("%s/cur", name);
	new = g_strdup_printf("%s/new", name);

	if (stat(name, &st) == -1 || !S_ISDIR(st.st_mode)
	    || stat(tmp, &st) == -1 || !S_ISDIR(st.st_mode)
	    || stat(cur, &st) == -1 || !S_ISDIR(st.st_mode)
	    || stat(new, &st) == -1 || !S_ISDIR(st.st_mode)) {
		camel_exception_setv (ex, CAMEL_EXCEPTION_SYSTEM,
				      _("Could not delete folder `%s': %s"),
				      folder_name, errno ? g_strerror (errno) :
				      _("not a maildir directory"));
	} else {
		int err = 0;

		/* remove subdirs first - will fail if not empty */
		if (rmdir(cur) == -1 || rmdir(new) == -1) {
			err = errno;
		} else {
			DIR *dir;
			struct dirent *d;

			/* for tmp (only), its contents is irrelevant */
			dir = opendir(tmp);
			if (dir) {
				while ( (d=readdir(dir)) ) {
					char *name = d->d_name, *file;

					if (!strcmp(name, ".") || !strcmp(name, ".."))
						continue;
					file = g_strdup_printf("%s/%s", tmp, name);
					unlink(file);
					g_free(file);
				}
				closedir(dir);
			}
			if (rmdir(tmp) == -1 || rmdir(name) == -1)
				err = errno;
		}

		if (err != 0) {
			/* easier just to mkdir all (and let them fail), than remember what we got to */
			mkdir(name, 0700);
			mkdir(cur, 0700);
			mkdir(new, 0700);
			mkdir(tmp, 0700);
			camel_exception_setv (ex, CAMEL_EXCEPTION_SYSTEM,
					      _("Could not delete folder `%s': %s"),
					      folder_name, g_strerror (err));
		} else {
			/* and remove metadata */
			((CamelStoreClass *)parent_class)->delete_folder(store, folder_name, ex);
		}
	}

	g_free(name);
	g_free(tmp);
	g_free(cur);
	g_free(new);
}

static void
maildir_rename_folder(CamelStore *store, const char *old, const char *new, CamelException *ex)
{
	if (strcmp(old, ".") == 0) {
		camel_exception_setv(ex, CAMEL_EXCEPTION_STORE_NO_FOLDER,
				     _("Cannot rename folder: %s: Invalid operation"), _("Inbox"));
		return;
	}

	((CamelStoreClass *)parent_class)->rename_folder(store, old, new, ex);
}

static CamelFolderInfo *camel_folder_info_new(CamelURL *url, const char *full, const char *name)
{
	CamelFolderInfo *fi;

	fi = g_malloc0(sizeof(*fi));
	fi->uri = camel_url_to_string(url, 0);
	fi->full_name = g_strdup(full);
	if (!strcmp(full, ".")) {
		fi->flags |= CAMEL_FOLDER_SYSTEM;
		fi->name = g_strdup(_("Inbox"));
	} else
		fi->name = g_strdup(name);
	fi->unread = -1;
	fi->total = -1;

	d(printf("Adding maildir info: '%s' '%s' '%s' '%s'\n", fi->path, fi->name, fi->full_name, fi->url));

	return fi;
}

static void
fill_fi(CamelStore *store, CamelFolderInfo *fi, guint32 flags)
{
	CamelFolder *folder;

	folder = camel_object_bag_get(store->folders, fi->full_name);

	if (folder == NULL
	    && (flags & CAMEL_STORE_FOLDER_INFO_FAST) == 0)
		folder = camel_store_get_folder(store, fi->full_name, 0, NULL);

	if (folder) {
		if ((flags & CAMEL_STORE_FOLDER_INFO_FAST) == 0)
			camel_folder_refresh_info(folder, NULL);
		fi->unread = camel_folder_get_unread_message_count(folder);
		fi->total = camel_folder_get_message_count(folder);
		camel_object_unref(folder);
	} else {
		char *path, *folderpath;
		CamelFolderSummary *s;
		const char *root;

		/* This should be fast enough not to have to test for INFO_FAST */
		root = camel_local_store_get_toplevel_dir((CamelLocalStore *)store);
		path = g_strdup_printf("%s/%s.ev-summary", root, fi->full_name);
		folderpath = g_strdup_printf("%s/%s", root, fi->full_name);
		s = (CamelFolderSummary *)camel_maildir_summary_new(NULL, path, folderpath, NULL);
		if (camel_folder_summary_header_load(s) != -1) {
			fi->unread = s->unread_count;
			fi->total = s->saved_count;
		}
		camel_object_unref(s);
		g_free(folderpath);
		g_free(path);
	}
}

/* used to find out where we've visited already */
struct _inode {
	dev_t dnode;
	ino_t inode;
};

/* returns number of records found at or below this level */
static int scan_dir(CamelStore *store, GHashTable *visited, CamelURL *url, const char *path, guint32 flags, CamelFolderInfo *parent, CamelFolderInfo **fip, CamelException *ex)
{
	DIR *dir;
	struct dirent *d;
	char *name, *tmp, *cur, *new;
	const char *base, *root = ((CamelService *)store)->url->path;
	CamelFolderInfo *fi = NULL;
	struct stat st;

	/* look for folders matching the right structure, recursively */
	name = g_strdup_printf("%s/%s", root, path);

	d(printf("checking dir '%s' part '%s' for maildir content\n", root, path));

	tmp = g_strdup_printf("%s/tmp", name);
	cur = g_strdup_printf("%s/cur", name);
	new = g_strdup_printf("%s/new", name);

	base = strrchr(path, '/');
	if (base)
		base++;
	else
		base = path;

	camel_url_set_fragment(url, path);

	fi = camel_folder_info_new(url, path, base);
	fill_fi(store, fi, flags);

	if (!(stat(tmp, &st) == 0 && S_ISDIR(st.st_mode)
	      && stat(cur, &st) == 0 && S_ISDIR(st.st_mode)
	      && stat(new, &st) == 0 && S_ISDIR(st.st_mode)))
		fi->flags |= CAMEL_FOLDER_NOSELECT;

	d(printf("found! uri = %s\n", fi->uri));
	d(printf("  full_name = %s\n  name = '%s'\n", fi->full_name, fi->name));
	
	fi->parent = parent;
	fi->next = *fip;
	*fip = fi;

	g_free(tmp);
	g_free(cur);
	g_free(new);

	/* always look further if asked */
	if (((flags & CAMEL_STORE_FOLDER_INFO_RECURSIVE) || parent == NULL)) {
		int children = 0;

		dir = opendir(name);
		if (dir == NULL) {
			camel_exception_setv (ex, CAMEL_EXCEPTION_SYSTEM,
					      _("Could not scan folder `%s': %s"),
					      root, g_strerror (errno));
			g_free(name);
			return -1;
		}

		while ( (d = readdir(dir)) ) {
			if (strcmp(d->d_name, "tmp") == 0
			    || strcmp(d->d_name, "cur") == 0
			    || strcmp(d->d_name, "new") == 0
			    || strcmp(d->d_name, ".") == 0
			    || strcmp(d->d_name, "..") == 0)
				continue;

			tmp = g_strdup_printf("%s/%s", name, d->d_name);
			if (stat(tmp, &st) == 0 && S_ISDIR(st.st_mode)) {
				struct _inode in = { st.st_dev, st.st_ino };

				/* see if we've visited already */
				if (g_hash_table_lookup(visited, &in) == NULL) {
					struct _inode *inew = g_malloc(sizeof(*inew));

					children++;

					*inew = in;
					g_hash_table_insert(visited, inew, inew);
					new = g_strdup_printf("%s/%s", path, d->d_name);
					if (scan_dir(store, visited, url, new, flags, fi, &fi->child, ex) == -1) {
						g_free(tmp);
						g_free(new);
						closedir(dir);
						return -1;
					}
					g_free(new);
				}
			}
			g_free(tmp);
		}
		closedir(dir);

		if (children)
			fi->flags |= CAMEL_FOLDER_CHILDREN;
		else
			fi->flags |= CAMEL_FOLDER_NOCHILDREN;
	}

	g_free(name);

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
	CamelLocalStore *local_store = (CamelLocalStore *)store;
	GHashTable *visited;
	CamelURL *url;

	visited = g_hash_table_new(inode_hash, inode_equal);

	url = camel_url_new("maildir:", NULL);
	camel_url_set_path(url, ((CamelService *)local_store)->url->path);

	if (scan_dir(store, visited, url, top == NULL || top[0] == 0?".":top, flags, NULL, &fi, ex) == -1 && fi != NULL) {
		camel_store_free_folder_info_full(store, fi);
		fi = NULL;
	}

	camel_url_free(url);
	g_hash_table_foreach(visited, inode_free, NULL);
	g_hash_table_destroy(visited);

	return fi;
}
