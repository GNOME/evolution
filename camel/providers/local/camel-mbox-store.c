/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Authors: Michael Zucchi <notzed@ximian.com>
 *
 * Copyright (C) 2000 Ximian, Inc.
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
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>

#include "camel-mbox-store.h"
#include "camel-mbox-folder.h"
#include "camel-exception.h"
#include "camel-url.h"

static CamelLocalStoreClass *parent_class = NULL;

/* Returns the class for a CamelMboxStore */
#define CMBOXS_CLASS(so) CAMEL_MBOX_STORE_CLASS (CAMEL_OBJECT_GET_CLASS(so))
#define CF_CLASS(so) CAMEL_FOLDER_CLASS (CAMEL_OBJECT_GET_CLASS(so))
#define CMBOXF_CLASS(so) CAMEL_MBOX_FOLDER_CLASS (CAMEL_OBJECT_GET_CLASS(so))

static CamelFolder *get_folder(CamelStore *store, const char *folder_name, guint32 flags, CamelException *ex);
static void delete_folder(CamelStore *store, const char *folder_name, CamelException *ex);

static void
camel_mbox_store_class_init (CamelMboxStoreClass *camel_mbox_store_class)
{
	CamelStoreClass *camel_store_class = CAMEL_STORE_CLASS (camel_mbox_store_class);

	parent_class = (CamelLocalStoreClass *)camel_type_get_global_classfuncs(camel_local_store_get_type());
	
	/* virtual method overload */
	camel_store_class->get_folder = get_folder;
	camel_store_class->delete_folder = delete_folder;
}

CamelType
camel_mbox_store_get_type (void)
{
	static CamelType camel_mbox_store_type = CAMEL_INVALID_TYPE;
	
	if (camel_mbox_store_type == CAMEL_INVALID_TYPE)	{
		camel_mbox_store_type = camel_type_register (CAMEL_LOCAL_STORE_TYPE, "CamelMboxStore",
							     sizeof (CamelMboxStore),
							     sizeof (CamelMboxStoreClass),
							     (CamelObjectClassInitFunc) camel_mbox_store_class_init,
							     NULL,
							     NULL,
							     NULL);
	}
	
	return camel_mbox_store_type;
}

static CamelFolder *
get_folder(CamelStore *store, const char *folder_name, guint32 flags, CamelException *ex)
{
	char *name;
	struct stat st;

	(void) ((CamelStoreClass *)parent_class)->get_folder(store, folder_name, flags, ex);
	if (camel_exception_is_set(ex))
		return NULL;

	name = g_strdup_printf("%s%s", CAMEL_LOCAL_STORE(store)->toplevel_dir, folder_name);

	if (stat(name, &st) == -1) {
		int fd;

		if (errno != ENOENT) {
			camel_exception_setv(ex, CAMEL_EXCEPTION_SYSTEM,
					     _("Could not open file `%s':\n%s"),
					     name, g_strerror(errno));
			g_free(name);
			return NULL;
		}
		if ((flags & CAMEL_STORE_FOLDER_CREATE) == 0) {
			camel_exception_setv(ex, CAMEL_EXCEPTION_STORE_NO_FOLDER,
					     _("Folder `%s' does not exist."),
					     folder_name);
			g_free(name);
			return NULL;
		}

		fd = open(name, O_WRONLY | O_CREAT | O_APPEND, 0600);
		if (fd == -1) {
			camel_exception_setv(ex, CAMEL_EXCEPTION_SYSTEM,
					     _("Could not create file `%s':\n%s"),
					     name, g_strerror(errno));
			g_free(name);
			return NULL;
		}
		g_free(name);
		close(fd);
	} else if (!S_ISREG(st.st_mode)) {
		camel_exception_setv(ex, CAMEL_EXCEPTION_STORE_NO_FOLDER,
				     _("`%s' is not a regular file."),
				     name);
		g_free(name);
		return NULL;
	} else
		g_free(name);

	return camel_mbox_folder_new(store, folder_name, flags, ex);
}

static void
delete_folder (CamelStore *store, const char *folder_name, CamelException *ex)
{
	char *name;
	struct stat st;

	name = g_strdup_printf ("%s%s", CAMEL_LOCAL_STORE (store)->toplevel_dir, folder_name);
	if (stat (name, &st) == -1) {
		camel_exception_setv (ex, CAMEL_EXCEPTION_SYSTEM,
				      _("Could not delete folder `%s':\n%s"),
				      folder_name, g_strerror (errno));
		g_free (name);
		return;
	}
	
	if (!S_ISREG (st.st_mode)) {
		camel_exception_setv (ex, CAMEL_EXCEPTION_STORE_NO_FOLDER,
				      _("`%s' is not a regular file."), name);
		g_free (name);
		return;
	}
	
	if (st.st_size != 0) {
		camel_exception_setv (ex, CAMEL_EXCEPTION_FOLDER_NON_EMPTY,
				      _("Folder `%s' is not empty. Not deleted."),
				      folder_name);
		g_free (name);
		return;
	}

	if (unlink(name) == -1 && errno != ENOENT) {
		camel_exception_setv (ex, CAMEL_EXCEPTION_SYSTEM,
				      _("Could not delete folder `%s':\n%s"),
				      name, g_strerror (errno));
		g_free(name);
		return;
	}

	g_free(name);

	/* and remove metadata */
	((CamelStoreClass *)parent_class)->delete_folder(store, folder_name, ex);
}
