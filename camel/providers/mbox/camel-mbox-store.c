/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* camel-mbox-store.c : class for an mbox store */

/* 
 *
 * Copyright (C) 2000 Helix Code, Inc. <bertrand@helixcode.com>
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

#include "camel-mbox-store.h"
#include "camel-mbox-folder.h"
#include "camel-exception.h"
#include "camel-url.h"

/* Returns the class for a CamelMboxStore */
#define CMBOXS_CLASS(so) CAMEL_MBOX_STORE_CLASS (CAMEL_OBJECT_GET_CLASS(so))
#define CF_CLASS(so) CAMEL_FOLDER_CLASS (CAMEL_OBJECT_GET_CLASS(so))
#define CMBOXF_CLASS(so) CAMEL_MBOX_FOLDER_CLASS (CAMEL_OBJECT_GET_CLASS(so))

static char *get_name (CamelService *service, gboolean brief);
static CamelFolder *get_folder (CamelStore *store, const char *folder_name,
				gboolean create, CamelException *ex);
static void delete_folder (CamelStore *store, const char *folder_name,
			   CamelException *ex);
static void rename_folder(CamelStore *store, const char *old_name, const char *new_name, CamelException *ex);
static char *get_folder_name (CamelStore *store, const char *folder_name,
			      CamelException *ex);

static void
camel_mbox_store_class_init (CamelMboxStoreClass *camel_mbox_store_class)
{
	CamelStoreClass *camel_store_class = CAMEL_STORE_CLASS (camel_mbox_store_class);
	CamelServiceClass *camel_service_class = CAMEL_SERVICE_CLASS (camel_mbox_store_class);
	
	/* virtual method overload */
	camel_service_class->get_name = get_name;

	camel_store_class->get_folder = get_folder;
	camel_store_class->delete_folder = delete_folder;
	camel_store_class->rename_folder = rename_folder;
	camel_store_class->get_folder_name = get_folder_name;
}

static void
camel_mbox_store_init (gpointer object, gpointer klass)
{
	CamelService *service = CAMEL_SERVICE (object);
	CamelStore *store = CAMEL_STORE (object);

	service->url_flags = CAMEL_SERVICE_URL_NEED_PATH;

	/* mbox names are filenames, so they are case-sensitive. */
	store->folders = g_hash_table_new (g_str_hash, g_str_equal);
}

CamelType
camel_mbox_store_get_type (void)
{
	static CamelType camel_mbox_store_type = CAMEL_INVALID_TYPE;
	
	if (camel_mbox_store_type == CAMEL_INVALID_TYPE)	{
		camel_mbox_store_type = camel_type_register (CAMEL_STORE_TYPE, "CamelMboxStore",
							     sizeof (CamelMboxStore),
							     sizeof (CamelMboxStoreClass),
							     (CamelObjectClassInitFunc) camel_mbox_store_class_init,
							     NULL,
							     (CamelObjectInitFunc) camel_mbox_store_init,
							     NULL);
	}
	
	return camel_mbox_store_type;
}

const gchar *
camel_mbox_store_get_toplevel_dir (CamelMboxStore *store)
{
	CamelURL *url = CAMEL_SERVICE (store)->url;

	g_assert (url != NULL);
	return url->path;
}

static CamelFolder *
get_folder (CamelStore *store, const char *folder_name, gboolean create,
	    CamelException *ex)
{
	CamelFolder *new_folder;
	char *name;
	struct stat st;

	name = g_strdup_printf ("%s%s", CAMEL_SERVICE (store)->url->path,
				folder_name);

	if (stat (name, &st) == -1) {
		int fd;

		if (errno != ENOENT) {
			camel_exception_setv (ex, CAMEL_EXCEPTION_SYSTEM,
					      "Could not open folder `%s':"
					      "\n%s", folder_name,
					      g_strerror (errno));
			g_free (name);
			return NULL;
		}
		if (!create) {
			camel_exception_setv (ex, CAMEL_EXCEPTION_STORE_NO_FOLDER,
					      "Folder `%s' does not exist.",
					      folder_name);
			g_free (name);
			return NULL;
		}

		fd = open (name, O_WRONLY | O_CREAT | O_APPEND, S_IRUSR | S_IWUSR);
		g_free (name);
		if (fd == -1) {
			camel_exception_setv (ex, CAMEL_EXCEPTION_SYSTEM,
					      "Could not create folder `%s':"
					      "\n%s", folder_name,
					      g_strerror (errno));
			return NULL;
		}
		close (fd);
	} else if (!S_ISREG (st.st_mode)) {
		camel_exception_setv (ex, CAMEL_EXCEPTION_STORE_NO_FOLDER,
				      "`%s' is not a regular file.",
				      name);
		g_free (name);
		return NULL;
	} else
		g_free (name);

	new_folder =  CAMEL_FOLDER (camel_object_new (CAMEL_MBOX_FOLDER_TYPE));
	
	CF_CLASS (new_folder)->init (new_folder, store, NULL,
				     folder_name, "/", TRUE, ex);
	CF_CLASS (new_folder)->refresh_info (new_folder, ex);

	return new_folder;
}

static void
delete_folder (CamelStore *store, const char *folder_name, CamelException *ex)
{
	char *name, *name2;
	struct stat st;
	int status;

	name = g_strdup_printf ("%s%s", CAMEL_SERVICE (store)->url->path, folder_name);
	if (stat (name, &st) == -1) {
		if (errno == ENOENT) {
			/* file doesn't exist - it's kinda like deleting it ;-) */
			g_free (name);
			return;
		}

		camel_exception_setv (ex, CAMEL_EXCEPTION_SYSTEM,
				      "Could not delete folder `%s':\n%s",
				      folder_name, g_strerror (errno));
		g_free (name);
		return;
	}
	
	if (!S_ISREG (st.st_mode)) {
		camel_exception_setv (ex, CAMEL_EXCEPTION_STORE_NO_FOLDER,
				      "`%s' is not a regular file.", name);
		g_free (name);
		return;
	}
	
	if (st.st_size != 0) {
		camel_exception_setv (ex, CAMEL_EXCEPTION_FOLDER_NON_EMPTY,
				      "Folder `%s' is not empty. Not deleted.",
				      folder_name);
		g_free (name);
		return;
	}

	/* Delete index and summary first, then the main file. */
	name2 = g_strdup_printf ("%s.ibex", name);
	status = unlink (name2);
	g_free (name2);
	if (status == 0 || errno == ENOENT) {
		name2 = g_strdup_printf ("%s-ev-summary", name);
		status = unlink (name2);
		g_free (name2);
	}
	if (status == 0 || errno == ENOENT)
		status = unlink (name);
	g_free (name);

	if (status == -1 && errno != ENOENT) {
		camel_exception_setv (ex, CAMEL_EXCEPTION_SYSTEM,
				      "Could not delete folder `%s':\n%s",
				      folder_name, g_strerror (errno));
	}
}

static int xrename(const char *oldp, const char *newp, const char *prefix, const char *suffix, CamelException *ex)
{
	struct stat st;
	char *old = g_strconcat(prefix, oldp, suffix, 0);
	char *new = g_strconcat(prefix, newp, suffix, 0);
	int ret = -1;

	printf("renaming %s%s to %s%s\n", oldp, suffix, newp, suffix);

	/* FIXME: this has races ... */
	if (!(stat(new, &st) == -1 && errno==ENOENT)) {
		camel_exception_setv(ex, CAMEL_EXCEPTION_SYSTEM,
				     "Could not rename folder %s to %s: destination exists",
				     old, new);
	} else if (rename(old, new) == 0 || errno==ENOENT) {
		ret = 0;
	} else if (stat(old, &st) == -1 && errno==ENOENT && stat(new, &st) == 0) {
		/* for nfs, check if the rename worked anyway ... */
		ret = 0;
	}
	printf("success = %d\n", ret);

	g_free(old);
	g_free(new);
	return ret;
}

static void rename_folder(CamelStore *store, const char *old, const char *new, CamelException *ex)
{
	char *path = CAMEL_SERVICE (store)->url->path;

	/* try to rollback failures, has obvious races */
	if (xrename(old, new, path, ".ibex", ex)) {
		return;
	}
	if (xrename(old, new, path, "-ev-summary", ex)) {
		xrename(new, old, path, ".ibex", ex);
		return;
	}
	if (xrename(old, new, path, "", ex)) {
		xrename(new, old, path, "-ev-summary", ex);
		xrename(new, old, path, ".ibex", ex);
	}
}

static char *
get_folder_name (CamelStore *store, const char *folder_name, CamelException *ex)
{
	/* For now, we don't allow hieararchy. FIXME. */
	if (strchr (folder_name + 1, '/')) {
		camel_exception_set (ex, CAMEL_EXCEPTION_STORE_NO_FOLDER,
				     "Mbox folders may not be nested.");
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
		return g_strdup_printf ("Local mail file %s", service->url->path);
}
