/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Authors: Michael Zucchi <notzed@ximian.com>
 *
 * Copyright (C) 2001 Ximian Inc (http://www.ximian.com/)
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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <sys/stat.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <stdio.h>

#include "camel-spool-store.h"
#include "camel-spool-folder.h"
#include "camel-exception.h"
#include "camel-url.h"

#define d(x)

/* Returns the class for a CamelSpoolStore */
#define CSPOOLS_CLASS(so) CAMEL_SPOOL_STORE_CLASS (CAMEL_OBJECT_GET_CLASS(so))
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
camel_spool_store_class_init (CamelSpoolStoreClass *camel_spool_store_class)
{
	CamelStoreClass *camel_store_class = CAMEL_STORE_CLASS (camel_spool_store_class);
	CamelServiceClass *camel_service_class = CAMEL_SERVICE_CLASS (camel_spool_store_class);
	
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
camel_spool_store_get_type (void)
{
	static CamelType camel_spool_store_type = CAMEL_INVALID_TYPE;
	
	if (camel_spool_store_type == CAMEL_INVALID_TYPE)	{
		camel_spool_store_type = camel_type_register (CAMEL_STORE_TYPE, "CamelSpoolStore",
							     sizeof (CamelSpoolStore),
							     sizeof (CamelSpoolStoreClass),
							     (CamelObjectClassInitFunc) camel_spool_store_class_init,
							     NULL,
							     NULL,
							     NULL);
	}
	
	return camel_spool_store_type;
}

static void
construct (CamelService *service, CamelSession *session, CamelProvider *provider, CamelURL *url, CamelException *ex)
{
	int len;
	char *path, *name;
	struct stat st;

	printf("constructing store of type %s '%s:%s'\n",
	       camel_type_to_name(((CamelObject *)service)->s.type), url->protocol, url->path);

	CAMEL_SERVICE_CLASS (parent_class)->construct (service, session, provider, url, ex);
	if (camel_exception_is_set (ex))
		return;

	if (service->url->path[0] != '/') {
		camel_exception_setv(ex, CAMEL_EXCEPTION_STORE_NO_FOLDER,
				     _("Store root %s is not an absolute path"), service->url->path);
		return;
	}

	len = strlen (service->url->path);
	if (len > 0 && service->url->path[len - 1] == '/') {
		service->url->path = g_realloc (service->url->path, len + 2);
		service->url->path[len-1] = 0;
	}

	path = service->url->path;
	len = strlen(path);
	name = path;
#if 0
	name = alloca(len+1);
	strcpy(name, path);
	name[len-1] = 0;
#endif

	if (stat(name, &st) == -1 || !S_ISREG(st.st_mode)) {
		camel_exception_setv(ex, CAMEL_EXCEPTION_STORE_NO_FOLDER,
				     _("Spool `%s' does not exist or is not a regular file"),
				     path);
		return;
	}
}

const char *
camel_spool_store_get_toplevel_dir (CamelSpoolStore *store)
{
	CamelURL *url = CAMEL_SERVICE (store)->url;

	g_assert (url != NULL);
	return url->path;
}

static CamelFolder *
get_folder(CamelStore * store, const char *folder_name, guint32 flags, CamelException * ex)
{
	char *path = ((CamelService *)store)->url->path;
	char *name;
	int len;

	printf("opening folder %s on path %s\n", folder_name, path);

	/* we only support an 'INBOX' */
	if (strcmp(folder_name, "INBOX") != 0) {
		camel_exception_setv(ex, CAMEL_EXCEPTION_STORE_NO_FOLDER,
				     _("Folder `%s/%s' does not exist."),
				     path, folder_name);
		return NULL;
	}

	len = strlen(path);
	name = alloca(len+1);
	strcpy(name, path);
	name[len-1] = 0;

	return camel_spool_folder_new(store, name, flags, ex);
}

static CamelFolder *
get_inbox(CamelStore *store, CamelException *ex)
{
	camel_exception_set(ex, CAMEL_EXCEPTION_STORE_NO_FOLDER,
			    _("Spool stores do not have an inbox"));
	return NULL;
}

static char *
get_name (CamelService *service, gboolean brief)
{
	if (brief)
		return g_strdup (service->url->path);
	else
		return g_strdup_printf (_("Spool mail file %s"), service->url->path);
}

static CamelFolderInfo *
get_folder_info (CamelStore *store, const char *top,
		 guint32 flags, CamelException *ex)
{
	CamelFolderInfo *fi = NULL;
	CamelService *service = (CamelService *)store;

	if (top == NULL || strcmp(top, "/INBOX") == 0) {
		/* FIXME: if the folder is opened we could look it up? */
		fi = g_malloc0(sizeof(*fi));
		fi->full_name = "/INBOX";
		fi->name = "INBOX";
		fi->url = g_strdup_printf("spool://%s#%s", service->url->path, fi->name);
		fi->unread_message_count = -1;
	}

	return fi;
}

static void free_folder_info (CamelStore *store, CamelFolderInfo *fi)
{
	if (fi) {
		g_free(fi->url);
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
