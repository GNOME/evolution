/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* camel-mbox-store.c : class for an mbox store */

/* 
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

#include "camel-mh-store.h"
#include "camel-mh-folder.h"
#include "camel-exception.h"
#include "camel-url.h"

/* Returns the class for a CamelMhStore */
#define CMHS_CLASS(so) CAMEL_MH_STORE_CLASS (GTK_OBJECT(so)->klass)
#define CF_CLASS(so) CAMEL_FOLDER_CLASS (GTK_OBJECT(so)->klass)
#define CMHF_CLASS(so) CAMEL_MH_FOLDER_CLASS (GTK_OBJECT(so)->klass)

static char *get_name(CamelService * service, gboolean brief);
static CamelFolder *get_folder(CamelStore * store, const char *folder_name, gboolean create, CamelException * ex);
static void delete_folder(CamelStore * store, const char *folder_name, CamelException * ex);
static char *get_folder_name(CamelStore * store, const char *folder_name, CamelException * ex);

static void camel_mh_store_class_init(CamelMhStoreClass * camel_mh_store_class)
{
	CamelStoreClass *camel_store_class = CAMEL_STORE_CLASS(camel_mh_store_class);
	CamelServiceClass *camel_service_class = CAMEL_SERVICE_CLASS(camel_mh_store_class);

	/* virtual method overload */
	camel_service_class->get_name = get_name;

	camel_store_class->get_folder = get_folder;
	camel_store_class->delete_folder = delete_folder;
	camel_store_class->get_folder_name = get_folder_name;
}

static void camel_mh_store_init(gpointer object, gpointer klass)
{
	CamelService *service = CAMEL_SERVICE(object);
	CamelStore *store = CAMEL_STORE(object);

	service->url_flags = CAMEL_SERVICE_URL_NEED_PATH;

	/* mh names are filenames, so they are case-sensitive. */
	store->folders = g_hash_table_new(g_str_hash, g_str_equal);
}

GtkType camel_mh_store_get_type(void)
{
	static GtkType camel_mh_store_type = 0;

	if (!camel_mh_store_type) {
		GtkTypeInfo camel_mh_store_info = {
			"CamelMhStore",
			sizeof(CamelMhStore),
			sizeof(CamelMhStoreClass),
			(GtkClassInitFunc) camel_mh_store_class_init,
			(GtkObjectInitFunc) camel_mh_store_init,
			/* reserved_1 */ NULL,
			/* reserved_2 */ NULL,
			(GtkClassInitFunc) NULL,
		};

		camel_mh_store_type = gtk_type_unique(CAMEL_STORE_TYPE, &camel_mh_store_info);
	}

	return camel_mh_store_type;
}

const gchar *camel_mh_store_get_toplevel_dir(CamelMhStore * store)
{
	CamelURL *url = CAMEL_SERVICE(store)->url;

	g_assert(url != NULL);
	return url->path;
}

static CamelFolder *get_folder(CamelStore * store, const char *folder_name, gboolean create, CamelException * ex)
{
	CamelFolder *new_folder = NULL;
	char *name;
	struct stat st;

	name = g_strdup_printf("%s%s", CAMEL_SERVICE(store)->url->path, folder_name);

	if (stat(name, &st) == -1) {
		int fd;

		if (errno != ENOENT) {
			camel_exception_setv(ex, CAMEL_EXCEPTION_SYSTEM,
					     "Could not open folder `%s':" "\n%s", folder_name, g_strerror(errno));
			goto done;
		}
		if (!create) {
			camel_exception_setv(ex, CAMEL_EXCEPTION_STORE_NO_FOLDER,
					     "Folder `%s' does not exist.", folder_name);
			goto done;
		}

		if (mkdir (name, 0600) != 0) {
			camel_exception_setv(ex, CAMEL_EXCEPTION_SYSTEM,
					     "Could not create folder `%s':" "\n%s", folder_name, g_strerror(errno));
			goto done;
		}
	} else if (!S_ISDIR(st.st_mode)) {
		camel_exception_setv(ex, CAMEL_EXCEPTION_STORE_NO_FOLDER, "`%s' is not a directory.", name);
		goto done;
	}

	new_folder = gtk_type_new(CAMEL_MH_FOLDER_TYPE);

	CF_CLASS(new_folder)->init(new_folder, store, NULL, folder_name, "/", TRUE, ex);
done:
	g_free(name);
	return new_folder;
}

static void delete_folder(CamelStore * store, const char *folder_name, CamelException * ex)
{
	char *name;

	name = g_strdup_printf("%s%s", CAMEL_SERVICE(store)->url->path, folder_name);
	camel_exception_setv(ex, CAMEL_EXCEPTION_SYSTEM,
			     "Could not delete folder `%s':\n%s", folder_name, "not implemented");
	g_free(name);
}

static char *get_folder_name(CamelStore * store, const char *folder_name, CamelException * ex)
{
	/* For now, we don't allow hieararchy. FIXME. */
	if (strchr(folder_name + 1, '/')) {
		camel_exception_set(ex, CAMEL_EXCEPTION_STORE_NO_FOLDER, "Mh folders may not be nested.");
		return NULL;
	}

	return *folder_name == '/' ? g_strdup(folder_name) : g_strdup_printf("/%s", folder_name);
}

static char *get_name(CamelService * service, gboolean brief)
{
	if (brief)
		return g_strdup(service->url->path);
	else
		return g_strdup_printf("Local mail file %s", service->url->path);
}
