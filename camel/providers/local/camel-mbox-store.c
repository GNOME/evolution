/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Authors: Michael Zucchi <notzed@ximian.com>
 *
 * Copyright(C) 2000 Ximian, Inc.
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

#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <dirent.h>
#include <fcntl.h>
#include <errno.h>

#include "camel-mbox-store.h"
#include "camel-mbox-folder.h"
#include "camel-file-utils.h"
#include "camel-text-index.h"
#include "camel-exception.h"
#include "camel-url.h"
#include "camel-i18n.h"

#define d(x) 

static CamelLocalStoreClass *parent_class = NULL;

/* Returns the class for a CamelMboxStore */
#define CMBOXS_CLASS(so) CAMEL_MBOX_STORE_CLASS(CAMEL_OBJECT_GET_CLASS(so))
#define CF_CLASS(so) CAMEL_FOLDER_CLASS(CAMEL_OBJECT_GET_CLASS(so))
#define CMBOXF_CLASS(so) CAMEL_MBOX_FOLDER_CLASS(CAMEL_OBJECT_GET_CLASS(so))

static CamelFolder *get_folder(CamelStore *store, const char *folder_name, guint32 flags, CamelException *ex);
static void delete_folder(CamelStore *store, const char *folder_name, CamelException *ex);
static void rename_folder(CamelStore *store, const char *old, const char *new, CamelException *ex);
static CamelFolderInfo *create_folder(CamelStore *store, const char *parent_name, const char *folder_name, CamelException *ex);
static CamelFolderInfo *get_folder_info(CamelStore *store, const char *top, guint32 flags, CamelException *ex);

static void
camel_mbox_store_class_init(CamelMboxStoreClass *camel_mbox_store_class)
{
	CamelStoreClass *camel_store_class = CAMEL_STORE_CLASS(camel_mbox_store_class);

	parent_class =(CamelLocalStoreClass *)camel_type_get_global_classfuncs(camel_local_store_get_type());
	
	/* virtual method overload */
	camel_store_class->get_folder = get_folder;
	camel_store_class->delete_folder = delete_folder;
	camel_store_class->rename_folder = rename_folder;
	camel_store_class->create_folder = create_folder;
	
	camel_store_class->get_folder_info = get_folder_info;
	camel_store_class->free_folder_info = camel_store_free_folder_info_full;
}

CamelType
camel_mbox_store_get_type(void)
{
	static CamelType camel_mbox_store_type = CAMEL_INVALID_TYPE;
	
	if (camel_mbox_store_type == CAMEL_INVALID_TYPE)	{
		camel_mbox_store_type = camel_type_register(CAMEL_LOCAL_STORE_TYPE, "CamelMboxStore",
							    sizeof(CamelMboxStore),
							    sizeof(CamelMboxStoreClass),
							    (CamelObjectClassInitFunc) camel_mbox_store_class_init,
							    NULL,
							    NULL,
							    NULL);
	}
	
	return camel_mbox_store_type;
}

static char *
mbox_folder_name_to_path(CamelStore *store, const char *folder_name)
{
	const char *toplevel_dir = CAMEL_LOCAL_STORE(store)->toplevel_dir;
	
	return camel_mbox_folder_get_full_path(NULL, toplevel_dir, folder_name);
}

static char *
mbox_folder_name_to_meta_path(CamelStore *store, const char *folder_name, const char *ext)
{
	const char *toplevel_dir = CAMEL_LOCAL_STORE(store)->toplevel_dir;
	
	return camel_mbox_folder_get_meta_path(NULL, toplevel_dir, folder_name, ext);
}

static char *extensions[] = {
	".msf", ".ev-summary", ".ibex.index", ".ibex.index.data", ".cmeta", ".lock"
};

static gboolean
ignore_file(const char *filename, gboolean sbd)
{
	int flen, len, i;
	
	/* TODO: Should probably just be 1 regex */
	flen = strlen(filename);
	if (flen > 0 && filename[flen-1] == '~')
		return TRUE;

	for (i = 0; i <(sizeof(extensions) / sizeof(extensions[0])); i++) {
		len = strlen(extensions[i]);
		if (len < flen && !strcmp(filename + flen - len, extensions[i]))
			return TRUE;
	}
	
	if (sbd && flen > 4 && !strcmp(filename + flen - 4, ".sbd"))
		return TRUE;
	
	return FALSE;
}

static CamelFolder *
get_folder(CamelStore *store, const char *folder_name, guint32 flags, CamelException *ex)
{
	struct stat st;
	char *name;
	
	if (!((CamelStoreClass *) parent_class)->get_folder(store, folder_name, flags, ex))
		return NULL;
	
	name = mbox_folder_name_to_path(store, folder_name);
	
	if (stat(name, &st) == -1) {
		const char *basename;
		char *dirname;
		int fd;
		
		if (errno != ENOENT) {
			camel_exception_setv(ex, CAMEL_EXCEPTION_SYSTEM,
					     _("Cannot get folder `%s': %s"),
					     folder_name, g_strerror (errno));
			g_free(name);
			return NULL;
		}
		
		if ((flags & CAMEL_STORE_FOLDER_CREATE) == 0) {
			camel_exception_setv(ex, CAMEL_EXCEPTION_STORE_NO_FOLDER,
					     _("Cannot get folder `%s': folder does not exist."),
					     folder_name);
			g_free(name);
			return NULL;
		}
		
		/* sanity check the folder name */
		if (!(basename = strrchr (folder_name, '/')))
			basename = folder_name;
		else
			basename++;
		
		if (basename[0] == '.' || ignore_file (basename, TRUE)) {
			camel_exception_set (ex, CAMEL_EXCEPTION_SYSTEM,
					     _("Cannot create a folder by this name."));
			g_free (name);
			return NULL;
		}
		
		dirname = g_path_get_dirname(name);
		if (camel_mkdir(dirname, 0777) == -1 && errno != EEXIST) {
			camel_exception_setv(ex, CAMEL_EXCEPTION_SYSTEM,
					     _("Cannot create folder `%s': %s"),
					     folder_name, g_strerror (errno));
			g_free(dirname);
			g_free(name);
			return NULL;
		}
		
		g_free(dirname);
		
		fd = open(name, O_WRONLY | O_CREAT | O_APPEND, 0666);
		if (fd == -1) {
			camel_exception_setv(ex, CAMEL_EXCEPTION_SYSTEM,
					     _("Cannot create folder `%s': %s"),
					     folder_name, g_strerror (errno));
			g_free(name);
			return NULL;
		}
		
		g_free(name);
		close(fd);
	} else if (!S_ISREG(st.st_mode)) {
		camel_exception_setv(ex, CAMEL_EXCEPTION_SYSTEM,
				     _("Cannot get folder `%s': not a regular file."),
				     folder_name);
		g_free(name);
		return NULL;
	} else if (flags & CAMEL_STORE_FOLDER_EXCL) {
		camel_exception_setv (ex, CAMEL_EXCEPTION_SYSTEM,
				      _("Cannot create folder `%s': folder exists."),
				      folder_name);
		g_free (name);
		return NULL;
	} else
		g_free(name);
	
	return camel_mbox_folder_new(store, folder_name, flags, ex);
}

static void
delete_folder(CamelStore *store, const char *folder_name, CamelException *ex)
{
	CamelFolderInfo *fi;
	CamelException lex;
	CamelFolder *lf;
	char *name, *path;
	struct stat st;
	
	name = mbox_folder_name_to_path(store, folder_name);
	path = g_strdup_printf("%s.sbd", name);
	
	if (rmdir(path) == -1 && errno != ENOENT) {
		camel_exception_setv(ex, CAMEL_EXCEPTION_SYSTEM,
				     _("Could not delete folder `%s':\n%s"),
				     folder_name, g_strerror(errno));
		g_free(path);
		g_free(name);
		return;
	}
	
	g_free(path);
	
	if (stat(name, &st) == -1) {
		camel_exception_setv(ex, CAMEL_EXCEPTION_SYSTEM,
				     _("Could not delete folder `%s':\n%s"),
				     folder_name, g_strerror(errno));
		g_free(name);
		return;
	}
	
	if (!S_ISREG(st.st_mode)) {
		camel_exception_setv(ex, CAMEL_EXCEPTION_STORE_NO_FOLDER,
				     _("`%s' is not a regular file."), name);
		g_free(name);
		return;
	}
	
	if (st.st_size != 0) {
		camel_exception_setv(ex, CAMEL_EXCEPTION_FOLDER_NON_EMPTY,
				     _("Folder `%s' is not empty. Not deleted."),
				     folder_name);
		g_free(name);
		return;
	}
	
	if (unlink(name) == -1 && errno != ENOENT) {
		camel_exception_setv(ex, CAMEL_EXCEPTION_SYSTEM,
				     _("Could not delete folder `%s':\n%s"),
				     name, g_strerror(errno));
		g_free(name);
		return;
	}
	
	/* FIXME: we have to do our own meta cleanup here rather than
	 * calling our parent class' delete_folder() method since our
	 * naming convention is different. Need to find a way for
	 * CamelLocalStore to be able to construct the folder & meta
	 * paths itself */
	path = mbox_folder_name_to_meta_path(store, folder_name, ".ev-summary");
	if (unlink(path) == -1 && errno != ENOENT) {
		camel_exception_setv(ex, CAMEL_EXCEPTION_SYSTEM,
				     _("Could not delete folder summary file `%s': %s"),
				     path, g_strerror(errno));
		g_free(path);
		g_free(name);
		return;
	}
	
	g_free(path);
	
	path = mbox_folder_name_to_meta_path(store, folder_name, ".ibex");
	if (camel_text_index_remove(path) == -1 && errno != ENOENT) {
		camel_exception_setv(ex, CAMEL_EXCEPTION_SYSTEM,
				     _("Could not delete folder index file `%s': %s"),
				     path, g_strerror(errno));
		g_free(path);
		g_free(name);
		return;
	}
	
	g_free(path);

	path = NULL;
	camel_exception_init(&lex);
	if ((lf = camel_store_get_folder(store, folder_name, 0, &lex))) {
		camel_object_get(lf, NULL, CAMEL_OBJECT_STATE_FILE, &path, NULL);
		camel_object_set(lf, NULL, CAMEL_OBJECT_STATE_FILE, NULL, NULL);
		camel_object_unref(lf);
	} else {
		camel_exception_clear(&lex);
	}
	
	if (path == NULL)
		path = mbox_folder_name_to_meta_path(store, folder_name, ".cmeta");
	
	if (unlink(path) == -1 && errno != ENOENT) {
		camel_exception_setv(ex, CAMEL_EXCEPTION_SYSTEM,
				     _("Could not delete folder meta file `%s': %s"),
				     path, g_strerror(errno));
		
		g_free(path);
		g_free(name);
		return;
	}
	
	g_free(path);
	g_free(name);
	
	fi = g_new0(CamelFolderInfo, 1);
	fi->full_name = g_strdup(folder_name);
	fi->name = g_path_get_basename(folder_name);
	fi->uri = g_strdup_printf("mbox:%s#%s",((CamelService *) store)->url->path, folder_name);
	fi->unread = -1;
	
	camel_object_trigger_event(store, "folder_deleted", fi);
	
	camel_folder_info_free(fi);
}

static CamelFolderInfo *
create_folder(CamelStore *store, const char *parent_name, const char *folder_name, CamelException *ex)
{
	/* FIXME: this is almost an exact copy of CamelLocalStore::create_folder() except that we use
	 * different path schemes... need to find a way to share parent's code? */
	const char *toplevel_dir =((CamelLocalStore *) store)->toplevel_dir;
	CamelFolderInfo *info = NULL;
	char *path, *name, *dir;
	CamelFolder *folder;
	struct stat st;
	
	if (toplevel_dir[0] != '/') {
		camel_exception_setv(ex, CAMEL_EXCEPTION_STORE_NO_FOLDER,
				     _("Store root %s is not an absolute path"), toplevel_dir);
		return NULL;
	}
	
	if (folder_name[0] == '.' || ignore_file(folder_name, TRUE)) {
		camel_exception_set(ex, CAMEL_EXCEPTION_SYSTEM,
				    _("Cannot create a folder by this name."));
		return NULL;
	}
	
	if (parent_name && *parent_name)
		name = g_strdup_printf("%s/%s", parent_name, folder_name);
	else
		name = g_strdup(folder_name);
	
	path = mbox_folder_name_to_path(store, name);
	
	dir = g_path_get_dirname(path);
	if (camel_mkdir(dir, 0777) == -1 && errno != EEXIST) {
		camel_exception_setv(ex, CAMEL_EXCEPTION_SYSTEM, _("Cannot create directory `%s': %s."),
				     dir, g_strerror(errno));
		
		g_free(path);
		g_free(name);
		g_free(dir);
		
		return NULL;
	}
	
	g_free(dir);
	
	if (stat(path, &st) == 0 || errno != ENOENT) {
		camel_exception_setv(ex, CAMEL_EXCEPTION_STORE_NO_FOLDER,
				     _("Cannot create folder: %s: %s"),
				     path, errno ? g_strerror(errno) :
				     _("Folder already exists"));
		
		g_free(path);
		g_free(name);
		
		return NULL;
	}
	
	g_free(path);
	
	folder =((CamelStoreClass *)((CamelObject *) store)->klass)->get_folder(store, name, CAMEL_STORE_FOLDER_CREATE, ex);
	if (folder) {
		camel_object_unref(folder);
		info =((CamelStoreClass *)((CamelObject *) store)->klass)->get_folder_info(store, name, 0, ex);
	}
	
	g_free(name);
	
	return info;
}

static int
xrename(CamelStore *store, const char *old_name, const char *new_name, const char *ext, gboolean missingok)
{
	const char *toplevel_dir =((CamelLocalStore *) store)->toplevel_dir;
	char *oldpath, *newpath;
	struct stat st;
	int ret = -1;
	int err = 0;
	
	if (ext != NULL) {
		oldpath = camel_mbox_folder_get_meta_path(NULL, toplevel_dir, old_name, ext);
		newpath = camel_mbox_folder_get_meta_path(NULL, toplevel_dir, new_name, ext);
	} else {
		oldpath = camel_mbox_folder_get_full_path(NULL, toplevel_dir, old_name);
		newpath = camel_mbox_folder_get_full_path(NULL, toplevel_dir, new_name);
	}
	
	if (stat(oldpath, &st) == -1) {
		if (missingok && errno == ENOENT) {
			ret = 0;
		} else {
			err = errno;
			ret = -1;
		}
	} else if (S_ISDIR(st.st_mode)) {
		/* use rename for dirs */
		if (rename(oldpath, newpath) == 0 || stat(newpath, &st) == 0) {
			ret = 0;
		} else {
			err = errno;
			ret = -1;
		}
	} else if (link(oldpath, newpath) == 0 /* and link for files */
		   ||(stat(newpath, &st) == 0 && st.st_nlink == 2)) {
		if (unlink(oldpath) == 0) {
			ret = 0;
		} else {
			err = errno;
			unlink(newpath);
			ret = -1;
		}
	} else {
		err = errno;
		ret = -1;
	}
	
	g_free(oldpath);
	g_free(newpath);
	
	return ret;
}

static void
rename_folder(CamelStore *store, const char *old, const char *new, CamelException *ex)
{
	CamelLocalFolder *folder = NULL;
	char *oldibex, *newibex, *newdir;
	int errnosav;
	
	if (new[0] == '.' || ignore_file(new, TRUE)) {
		camel_exception_setv(ex, CAMEL_EXCEPTION_SYSTEM,
				     _("The new folder name is illegal."));
		return;
	}
	
	/* try to rollback failures, has obvious races */
	
	oldibex = mbox_folder_name_to_meta_path(store, old, ".ibex");
	newibex = mbox_folder_name_to_meta_path(store, new, ".ibex");
	
	newdir = g_path_get_dirname(newibex);
	if (camel_mkdir(newdir, 0777) == -1) {
		if (errno != EEXIST) {
			camel_exception_setv(ex, CAMEL_EXCEPTION_SYSTEM,
					     _("Could not rename `%s': `%s': %s"),
					     old, new, g_strerror(errno));
			g_free(oldibex);
			g_free(newibex);
			g_free(newdir);
			
			return;
		}
		
		g_free(newdir);
		newdir = NULL;
	}
	
	folder = camel_object_bag_get(store->folders, old);
	if (folder && folder->index) {
		if (camel_index_rename(folder->index, newibex) == -1 && errno != ENOENT) {
			errnosav = errno;
			goto ibex_failed;
		}
	} else {
		/* TODO: camel_text_index_rename should find out if we have an active index itself? */
		if (camel_text_index_rename(oldibex, newibex) == -1 && errno != ENOENT) {
			errnosav = errno;
			goto ibex_failed;
		}
	}
	
	if (xrename(store, old, new, ".ev-summary", TRUE) == -1) {
		errnosav = errno;
		goto summary_failed;
	}

	if (xrename(store, old, new, ".cmeta", TRUE) == -1) {
		errnosav = errno;
		goto cmeta_failed;
	}
	
	if (xrename(store, old, new, ".sbd", TRUE) == -1) {
		errnosav = errno;
		goto subdir_failed;
	}
	
	if (xrename(store, old, new, NULL, FALSE) == -1) {
		errnosav = errno;
		goto base_failed;
	}
	
	g_free(oldibex);
	g_free(newibex);
	
	if (folder)
		camel_object_unref(folder);
	
	return;
	
base_failed:
	xrename(store, new, old, ".sbd", TRUE);
subdir_failed:
	xrename(store, new, old, ".cmeta", TRUE);
cmeta_failed:	
	xrename(store, new, old, ".ev-summary", TRUE);
summary_failed:
	if (folder) {
		if (folder->index)
			camel_index_rename(folder->index, oldibex);
	} else
		camel_text_index_rename(newibex, oldibex);
ibex_failed:
	if (newdir) {
		/* newdir is only non-NULL if we needed to mkdir */
		rmdir(newdir);
		g_free(newdir);
	}
	
	camel_exception_setv(ex, CAMEL_EXCEPTION_SYSTEM,
			     _("Could not rename '%s' to %s: %s"),
			     old, new, g_strerror(errnosav));
	
	g_free(newibex);
	g_free(oldibex);
	
	if (folder)
		camel_object_unref(folder);
}

/* used to find out where we've visited already */
struct _inode {
	dev_t dnode;
	ino_t inode;
};

static guint
inode_hash(const void *d)
{
	const struct _inode *v = d;
	
	return v->inode ^ v->dnode;
}

static gboolean
inode_equal(const void *a, const void *b)
{
	const struct _inode *v1 = a, *v2 = b;
	
	return v1->inode == v2->inode && v1->dnode == v2->dnode;
}

static void
inode_free(void *k, void *v, void *d)
{
	g_free(k);
}

/* NB: duplicated in maildir store */
static void
fill_fi(CamelStore *store, CamelFolderInfo *fi, guint32 flags)
{
	CamelFolder *folder;

	fi->unread = -1;
	fi->total = -1;
	folder = camel_object_bag_get(store->folders, fi->full_name);
	if (folder) {
		if ((flags & CAMEL_STORE_FOLDER_INFO_FAST) == 0)
			camel_folder_refresh_info(folder, NULL);
		fi->unread = camel_folder_get_unread_message_count(folder);
		fi->total = camel_folder_get_message_count(folder);
		camel_object_unref(folder);
	} else {
		char *path, *folderpath;
		CamelMboxSummary *mbs;
		const char *root;

		/* This should be fast enough not to have to test for INFO_FAST */
		root = camel_local_store_get_toplevel_dir((CamelLocalStore *)store);
		path = camel_mbox_folder_get_meta_path(NULL, root, fi->full_name, ".ev-summary");
		folderpath = camel_mbox_folder_get_full_path(NULL, root, fi->full_name);
		
		mbs = (CamelMboxSummary *)camel_mbox_summary_new(NULL, path, folderpath, NULL);
		if (camel_folder_summary_header_load((CamelFolderSummary *)mbs) != -1) {
			fi->unread = ((CamelFolderSummary *)mbs)->unread_count;
			fi->total = ((CamelFolderSummary *)mbs)->saved_count;
		}

		camel_object_unref(mbs);
		g_free(folderpath);
		g_free(path);
	}
}

static CamelFolderInfo *
scan_dir(CamelStore *store, CamelURL *url, GHashTable *visited, CamelFolderInfo *parent, const char *root,
	 const char *name, guint32 flags, CamelException *ex)
{
	CamelFolderInfo *folders, *tail, *fi;
	GHashTable *folder_hash;
	struct dirent *dent;
	DIR *dir;
	
	tail = folders = NULL;
	
	if (!(dir = opendir(root)))
		return NULL;
	
	folder_hash = g_hash_table_new(g_str_hash, g_str_equal);
	
	/* FIXME: it would be better if we queue'd up the recursive
	 * scans till the end so that we can limit the number of
	 * directory descriptors open at any given time... */
	
	while ((dent = readdir(dir))) {
		char *short_name, *full_name, *path, *ext;
		struct stat st;
		
		if (dent->d_name[0] == '.')
			continue;
		
		if (ignore_file(dent->d_name, FALSE))
			continue;
		
		path = g_strdup_printf("%s/%s", root, dent->d_name);
		if (stat(path, &st) == -1) {
			g_free(path);
			continue;
		}
		
		if (S_ISDIR(st.st_mode)) {
			struct _inode in = { st.st_dev, st.st_ino };
			
			if (g_hash_table_lookup(visited, &in)) {
				g_free(path);
				continue;
			}
		}
		
		short_name = g_strdup(dent->d_name);
		if ((ext = strrchr(short_name, '.')) && !strcmp(ext, ".sbd"))
			*ext = '\0';
		
		if (name != NULL)
			full_name = g_strdup_printf("%s/%s", name, short_name);
		else
			full_name = g_strdup(short_name);
				
		if ((fi = g_hash_table_lookup(folder_hash, short_name)) != NULL) {
			g_free(short_name);
			g_free(full_name);
			
			if (S_ISDIR(st.st_mode)) {
				fi->flags =(fi->flags & ~CAMEL_FOLDER_NOCHILDREN) | CAMEL_FOLDER_CHILDREN;
			} else {
				fi->flags &= ~CAMEL_FOLDER_NOSELECT;
			}
		} else {
			fi = g_new0(CamelFolderInfo, 1);
			fi->parent = parent;
			
			camel_url_set_fragment (url, full_name);
			
			fi->uri = camel_url_to_string (url, 0);
			fi->name = short_name;
			fi->full_name = full_name;
			fi->unread = -1;
			fi->total = -1;

			if (S_ISDIR(st.st_mode))
				fi->flags = CAMEL_FOLDER_NOSELECT;
			else
				fi->flags = CAMEL_FOLDER_NOCHILDREN;
			
			if (tail == NULL)
				folders = fi;
			else
				tail->next = fi;
			
			tail = fi;
			
			g_hash_table_insert(folder_hash, fi->name, fi);
		}
		
		if (!S_ISDIR(st.st_mode)) {
			fill_fi(store, fi, flags);
		} else if ((flags & CAMEL_STORE_FOLDER_INFO_RECURSIVE)) {
			struct _inode in = { st.st_dev, st.st_ino };
			
			if (g_hash_table_lookup(visited, &in) == NULL) {
				struct _inode *inew = g_new(struct _inode, 1);
				
				*inew = in;
				
				g_hash_table_insert(visited, inew, inew);
				
				if ((fi->child = scan_dir (store, url, visited, fi, path, fi->full_name, flags, ex)))
					fi->flags |= CAMEL_FOLDER_CHILDREN;
				else
					fi->flags =(fi->flags & ~CAMEL_FOLDER_CHILDREN) | CAMEL_FOLDER_NOCHILDREN;
			}
		}
		
		g_free(path);
	}
	
	closedir(dir);
	
	g_hash_table_destroy(folder_hash);
	
	return folders;
}

static CamelFolderInfo *
get_folder_info(CamelStore *store, const char *top, guint32 flags, CamelException *ex)
{
	GHashTable *visited;
	struct _inode *inode;
	char *path, *subdir;
	CamelFolderInfo *fi;
	const char *base;
	struct stat st;
	CamelURL *url;
	
	top = top ? top : "";
	path = mbox_folder_name_to_path(store, top);
	
	if (*top == '\0') {
		/* requesting root dir scan */
		if (stat(path, &st) == -1 || !S_ISDIR(st.st_mode)) {
			g_free(path);
			return NULL;
		}
		
		visited = g_hash_table_new(inode_hash, inode_equal);
		
		inode = g_malloc0(sizeof(*inode));
		inode->dnode = st.st_dev;
		inode->inode = st.st_ino;
		
		g_hash_table_insert(visited, inode, inode);
		
		url = camel_url_copy (((CamelService *) store)->url);
		fi = scan_dir (store, url, visited, NULL, path, NULL, flags, ex);
		g_hash_table_foreach(visited, inode_free, NULL);
		g_hash_table_destroy(visited);
		camel_url_free (url);
		g_free (path);
		
		return fi;
	}
	
	/* requesting scan of specific folder */
	if (stat(path, &st) == -1 || !S_ISREG(st.st_mode)) {
		g_free(path);
		return NULL;
	}
	
	visited = g_hash_table_new(inode_hash, inode_equal);
	
	if (!(base = strrchr(top, '/')))
		base = top;
	else
		base++;
	
	url = camel_url_copy (((CamelService *) store)->url);
	camel_url_set_fragment (url, top);
	
	fi = g_new0(CamelFolderInfo, 1);
	fi->parent = NULL;
	fi->uri = camel_url_to_string (url, 0);
	fi->name = g_strdup(base);
	fi->full_name = g_strdup(top);
	fi->unread = -1;
	fi->total = -1;
	
	subdir = g_strdup_printf("%s.sbd", path);
	if (stat(subdir, &st) == 0) {
		if  (S_ISDIR(st.st_mode))
			fi->child = scan_dir (store, url, visited, fi, subdir, top, flags, ex);
		else
			fill_fi(store, fi, flags);
	} else
		fill_fi(store, fi, flags);
	
	camel_url_free (url);
	
	if (fi->child)
		fi->flags |= CAMEL_FOLDER_CHILDREN;
	else
		fi->flags |= CAMEL_FOLDER_NOCHILDREN;
	
	g_free(subdir);
	
	g_hash_table_foreach(visited, inode_free, NULL);
	g_hash_table_destroy(visited);
	g_free(path);
	
	return fi;
}
