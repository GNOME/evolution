/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2000 Helix Code, Inc.
 *
 * Authors: Michael Zucchi <notzed@helixcode.com>
 *
 * This program is free software; you can redistribute it and/or 
 * modify it under the terms of the GNU General Public License as 
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
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

#include <config.h>

#include <sys/stat.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>

#include <dirent.h>

#include "camel-maildir-store.h"
#include "camel-maildir-folder.h"
#include "camel-exception.h"
#include "camel-url.h"

static CamelLocalStoreClass *parent_class = NULL;

/* Returns the class for a CamelMaildirStore */
#define CMAILDIRS_CLASS(so) CAMEL_MAILDIR_STORE_CLASS (CAMEL_OBJECT_GET_CLASS(so))
#define CF_CLASS(so) CAMEL_FOLDER_CLASS (CAMEL_OBJECT_GET_CLASS(so))
#define CMAILDIRF_CLASS(so) CAMEL_MAILDIR_FOLDER_CLASS (CAMEL_OBJECT_GET_CLASS(so))

static CamelFolder *get_folder(CamelStore * store, const char *folder_name, guint32 flags, CamelException * ex);
static void delete_folder(CamelStore * store, const char *folder_name, CamelException * ex);

static void camel_maildir_store_class_init(CamelObjectClass * camel_maildir_store_class)
{
	CamelStoreClass *camel_store_class = CAMEL_STORE_CLASS(camel_maildir_store_class);
	/*CamelServiceClass *camel_service_class = CAMEL_SERVICE_CLASS(camel_maildir_store_class);*/

	parent_class = (CamelLocalStoreClass *)camel_type_get_global_classfuncs(camel_local_store_get_type());

	/* virtual method overload, use defaults for most */
	camel_store_class->get_folder = get_folder;
	camel_store_class->delete_folder = delete_folder;
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

static CamelFolder *get_folder(CamelStore * store, const char *folder_name, guint32 flags, CamelException * ex)
{
	char *name, *tmp, *cur, *new;
	struct stat st;
	CamelFolder *folder = NULL;

	(void) ((CamelStoreClass *)parent_class)->get_folder(store, folder_name, flags, ex);
	if (camel_exception_is_set(ex))
		return NULL;

	name = g_strdup_printf("%s%s", CAMEL_SERVICE(store)->url->path, folder_name);
	tmp = g_strdup_printf("%s/tmp", name);
	cur = g_strdup_printf("%s/cur", name);
	new = g_strdup_printf("%s/new", name);

	if (stat(name, &st) == -1) {
		if (errno != ENOENT) {
			camel_exception_setv(ex, CAMEL_EXCEPTION_SYSTEM,
					     _("Could not open folder `%s':\n%s"),
					     folder_name, strerror(errno));
		} else if ((flags & CAMEL_STORE_FOLDER_CREATE) == 0) {
			camel_exception_setv(ex, CAMEL_EXCEPTION_STORE_NO_FOLDER,
					     _("Folder `%s' does not exist."), folder_name);
		} else {
			if (mkdir(name, 0700) != 0
			    || mkdir(tmp, 0700) != 0
			    || mkdir(cur, 0700) != 0
			    || mkdir(new, 0700) != 0) {
				camel_exception_setv(ex, CAMEL_EXCEPTION_SYSTEM,
						     _("Could not create folder `%s':\n%s"),
						     folder_name, strerror(errno));
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
		camel_exception_setv(ex, CAMEL_EXCEPTION_STORE_NO_FOLDER,
				     _("`%s' is not a maildir directory."), name);
	} else {
		folder = camel_maildir_folder_new(store, folder_name, flags, ex);
	}

	g_free(name);
	g_free(tmp);
	g_free(cur);
	g_free(new);

	return folder;
}

static void delete_folder(CamelStore * store, const char *folder_name, CamelException * ex)
{
	char *name, *tmp, *cur, *new;
	struct stat st;

	name = g_strdup_printf("%s%s", CAMEL_SERVICE(store)->url->path, folder_name);

	tmp = g_strdup_printf("%s/tmp", name);
	cur = g_strdup_printf("%s/cur", name);
	new = g_strdup_printf("%s/new", name);

	if (stat(name, &st) == -1 || !S_ISDIR(st.st_mode)
	    || stat(tmp, &st) == -1 || !S_ISDIR(st.st_mode)
	    || stat(cur, &st) == -1 || !S_ISDIR(st.st_mode)
	    || stat(new, &st) == -1 || !S_ISDIR(st.st_mode)) {
		camel_exception_setv(ex, CAMEL_EXCEPTION_SYSTEM,
				     _("Could not delete folder `%s': %s"),
				     folder_name, errno?strerror(errno):_("not a maildir directory"));
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
			camel_exception_setv(ex, CAMEL_EXCEPTION_SYSTEM,
					     _("Could not delete folder `%s': %s"),
					     folder_name, strerror(err));
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
