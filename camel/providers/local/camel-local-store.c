/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Authors: Michael Zucchi <notzed@helixcode.com>
 *
 * Copyright (C) 2000 Helix Code, Inc.
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
#include <stdio.h>

#include "camel-local-store.h"
#include "camel-exception.h"
#include "camel-url.h"

#define d(x)

/* Returns the class for a CamelLocalStore */
#define CLOCALS_CLASS(so) CAMEL_LOCAL_STORE_CLASS (CAMEL_OBJECT_GET_CLASS(so))
#define CF_CLASS(so) CAMEL_FOLDER_CLASS (CAMEL_OBJECT_GET_CLASS(so))

static CamelFolder *get_folder(CamelStore * store, const char *folder_name, guint32 flags, CamelException * ex);
static char *get_name(CamelService *service, gboolean brief);
static char *get_root_folder_name (CamelStore *store, CamelException *ex);
static char *get_default_folder_name (CamelStore *store, CamelException *ex);
static void rename_folder(CamelStore *store, const char *old_name, const char *new_name, CamelException *ex);
static char *get_folder_name(CamelStore *store, const char *folder_name, CamelException *ex);
static CamelFolderInfo *get_folder_info (CamelStore *store, const char *top,
					 gboolean fast, gboolean recursive,
					 gboolean subscribed_only,
					 CamelException *ex);
static void delete_folder(CamelStore *store, const char *folder_name, CamelException *ex);
static void rename_folder(CamelStore *store, const char *old, const char *new, CamelException *ex);

static void
camel_local_store_class_init (CamelLocalStoreClass *camel_local_store_class)
{
	CamelStoreClass *camel_store_class = CAMEL_STORE_CLASS (camel_local_store_class);
	CamelServiceClass *camel_service_class = CAMEL_SERVICE_CLASS (camel_local_store_class);
	
	/* virtual method overload */
	camel_service_class->get_name = get_name;
	camel_store_class->get_folder = get_folder;
	camel_store_class->get_root_folder_name = get_root_folder_name;
	camel_store_class->get_default_folder_name = get_default_folder_name;
	camel_store_class->get_folder_name = get_folder_name;
	camel_store_class->get_folder_info = get_folder_info;
	camel_store_class->free_folder_info = camel_store_free_folder_info_full;

	camel_store_class->delete_folder = delete_folder;
	camel_store_class->rename_folder = rename_folder;
}

static void
camel_local_store_init (gpointer object, gpointer klass)
{
	CamelStore *store = CAMEL_STORE (object);

	/* local names are filenames, so they are case-sensitive. */
	store->folders = g_hash_table_new (g_str_hash, g_str_equal);
}

CamelType
camel_local_store_get_type (void)
{
	static CamelType camel_local_store_type = CAMEL_INVALID_TYPE;
	
	if (camel_local_store_type == CAMEL_INVALID_TYPE)	{
		camel_local_store_type = camel_type_register (CAMEL_STORE_TYPE, "CamelLocalStore",
							     sizeof (CamelLocalStore),
							     sizeof (CamelLocalStoreClass),
							     (CamelObjectClassInitFunc) camel_local_store_class_init,
							     NULL,
							     (CamelObjectInitFunc) camel_local_store_init,
							     NULL);
	}
	
	return camel_local_store_type;
}

const char *
camel_local_store_get_toplevel_dir (CamelLocalStore *store)
{
	CamelURL *url = CAMEL_SERVICE (store)->url;

	g_assert (url != NULL);
	return url->path;
}

static CamelFolder *
get_folder(CamelStore * store, const char *folder_name, guint32 flags, CamelException * ex)
{
	struct stat st;
	char *path = ((CamelService *)store)->url->path;
	char *sub, *slash;

	if (path[0] != '/') {
		camel_exception_setv(ex, CAMEL_EXCEPTION_STORE_NO_FOLDER,
				     _("Store root %s is not an absolute path"), path);
		return NULL;
	}

	if (stat(path, &st) == 0) {
		if (!S_ISDIR(st.st_mode)) {
			camel_exception_setv(ex, CAMEL_EXCEPTION_STORE_NO_FOLDER,
					     _("Store root %s is not a regular directory"), path);
		}
		return NULL;
	}

	if (errno != ENOENT
	    || (flags & CAMEL_STORE_FOLDER_CREATE) == 0) {
		camel_exception_setv(ex, CAMEL_EXCEPTION_STORE_NO_FOLDER,
				     _("Cannot get folder: %s: %s"), path, strerror(errno));
		return NULL;
	}

	/* need to create the dir heirarchy */
	sub = alloca(strlen(path)+1);
	strcpy(sub, path);
	slash = sub;
	do {
		slash = strchr(slash+1, '/');
		if (slash)
			*slash = 0;
		if (stat(sub, &st) == -1) {
			if (errno != ENOENT
			    || mkdir(sub, 0700) == -1) {
				camel_exception_setv(ex, CAMEL_EXCEPTION_STORE_NO_FOLDER,
						     _("Cannot get folder: %s: %s"), path, strerror(errno));
				return NULL;
			}
		}
		if (slash)
			*slash = '/';
	} while (slash);

	return NULL;
}

static char *
get_root_folder_name(CamelStore *store, CamelException *ex)
{
	camel_exception_set(ex, CAMEL_EXCEPTION_STORE_NO_FOLDER,
			    _("Local stores do not have a root folder"));
	return NULL;
}

static char *
get_default_folder_name(CamelStore *store, CamelException *ex)
{
	camel_exception_set(ex, CAMEL_EXCEPTION_STORE_NO_FOLDER,
			    _("Local stores do not have a default folder"));
	return NULL;
}

static char *
get_folder_name (CamelStore *store, const char *folder_name, CamelException *ex)
{
	/* For now, we don't allow hieararchy. FIXME. */
	if (strchr (folder_name + 1, '/')) {
		camel_exception_set (ex, CAMEL_EXCEPTION_STORE_NO_FOLDER,
				     _("Local folders may not be nested."));
		return NULL;
	}

	return *folder_name == '/' ? g_strdup (folder_name) :
		g_strdup_printf ("/%s", folder_name);
}

static char *
get_name (CamelService *service, gboolean brief)
{
	if (brief)
		return g_strdup (service->url->path);
	else
		return g_strdup_printf (_("Local mail file %s"), service->url->path);
}

static CamelFolderInfo *
get_folder_info (CamelStore *store, const char *top,
		 gboolean fast, gboolean recursive,
		 gboolean subscribed_only,
		 CamelException *ex)
{
	/* FIXME: This is broken, but it corresponds to what was
	 * there before.
	 */
	return NULL;
}

static int xrename(const char *oldp, const char *newp, const char *prefix, const char *suffix, int missingok, CamelException *ex)
{
	struct stat st;
	char *old = g_strconcat(prefix, oldp, suffix, 0);
	char *new = g_strconcat(prefix, newp, suffix, 0);
	int ret = -1;
	int err = 0;

	d(printf("renaming %s%s to %s%s\n", oldp, suffix, newp, suffix));

	if (stat(old, &st) == -1) {
		if (missingok && errno == ENOENT) {
			ret = 0;
		} else {
			err = errno;
			ret = -1;
		}
	} else if (S_ISDIR(st.st_mode)) { /* use rename for dirs */
		if (rename(old, new) == 0
		    || stat(new, &st) == 0) {
			ret = 0;
		} else {
			err = errno;
			ret = -1;
		}
	} else if (link(old, new) == 0 /* and link for files */
		   || (stat(new, &st) == 0 && st.st_nlink == 2)) {
		if (unlink(old) == 0) {
			ret = 0;
		} else {
			err = errno;
			unlink(new);
			ret = -1;
		}
	} else {
		err = errno;
		ret = -1;
	}

	if (ret == -1) {
		camel_exception_setv(ex, CAMEL_EXCEPTION_SYSTEM,
				     _("Could not rename folder %s to %s: %s"),
				     old, new, strerror(err));
	}

	g_free(old);
	g_free(new);
	return ret;
}

/* default implementation, rename all */
static void
rename_folder(CamelStore *store, const char *old, const char *new, CamelException *ex)
{
	char *path = CAMEL_SERVICE (store)->url->path;

	/* try to rollback failures, has obvious races */
	if (xrename(old, new, path, ".ibex", TRUE, ex)) {
		return;
	}
	if (xrename(old, new, path, ".ev-summary", TRUE, ex)) {
		xrename(new, old, path, ".ibex", TRUE, ex);
		return;
	}
	if (xrename(old, new, path, "", FALSE, ex)) {
		xrename(new, old, path, ".ev-summary", TRUE, ex);
		xrename(new, old, path, ".ibex", TRUE, ex);
	}
}

/* default implementation, only delete metadata */
static void
delete_folder(CamelStore *store, const char *folder_name, CamelException *ex)
{
	char *name;
	char *str;

	/* remove metadata only */
	name = g_strdup_printf("%s%s", CAMEL_SERVICE(store)->url->path, folder_name);
	str = g_strdup_printf("%s.ev-summary", name);
	if (unlink(str) == -1 && errno != ENOENT) {
		camel_exception_setv(ex, CAMEL_EXCEPTION_SYSTEM,
				     _("Could not delete folder summary file `%s': %s"),
				     str, strerror(errno));
		g_free(str);
		return;
	}
	g_free(str);
	str = g_strdup_printf("%s.ibex", name);
	if (unlink(str) == -1 && errno != ENOENT) {
		camel_exception_setv(ex, CAMEL_EXCEPTION_SYSTEM,
				     _("Could not delete folder index file `%s': %s"),
				     str, strerror(errno));
		g_free(str);
		return;
	}
	g_free(str);
	g_free(name);
}
