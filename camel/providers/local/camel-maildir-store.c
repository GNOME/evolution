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

	parent_class = (CamelLocalStoreClass *)camel_type_get_global_classfuncs(camel_folder_get_type());

	/* virtual method overload, use defaults for most */
	camel_store_class->get_folder = get_folder;
	camel_store_class->delete_folder = delete_folder;
}

static void camel_maildir_store_init(CamelObject * object)
{
	CamelStore *store = CAMEL_STORE(object);

	/* maildir names are filenames, so they are case-sensitive. */
	store->folders = g_hash_table_new(g_str_hash, g_str_equal);
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
							  (CamelObjectInitFunc) camel_maildir_store_init,
							  NULL);
	}

	return camel_maildir_store_type;
}

static CamelFolder *get_folder(CamelStore * store, const char *folder_name, guint32 flags, CamelException * ex)
{
	char *name, *tmp, *cur, *new;
	struct stat st;
	CamelFolder *folder = NULL;

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
			printf("creating ...\n");

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
				printf("created ok?\n");
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

	name = g_strdup_printf("%s%s", CAMEL_SERVICE(store)->url->path, folder_name);
	tmp = g_strdup_printf("%s/tmp", name);
	cur = g_strdup_printf("%s/cur", name);
	new = g_strdup_printf("%s/new", name);

	/* remove subdirs first - will fail if not empty */
	if ((rmdir(tmp) == -1 && errno != ENOENT)
	    || (rmdir(new) == -1 && errno != ENOENT)
	    || (rmdir(cur) == -1 && errno != ENOENT)
	    || (rmdir(name) == -1  && errno != ENOENT)) {
		camel_exception_setv(ex, CAMEL_EXCEPTION_SYSTEM,
				     _("Could not delete folder `%s': %s"),
				     folder_name, strerror(errno));
	} else {
		/* and remove metadata */
		((CamelStoreClass *)parent_class)->delete_folder(store, folder_name, ex);
	}

	g_free(name);
	g_free(tmp);
	g_free(cur);
	g_free(new);
}
