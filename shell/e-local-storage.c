/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* e-local-storage.c
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
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 * Author: Ettore Perazzoli
 */

/* FIXMEs:
 *
 *   - If we have `.' or `..' as path elements, we lose.
 *
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#define _POSIX_SOURCE /* Yuck.  */
#include <dirent.h>

#include <string.h>
#include <sys/param.h>
#include <sys/types.h>
#include <sys/stat.h>

#include <gnome.h>

#include "e-util/e-util.h"
#include "e-local-folder.h"

#include "e-local-storage.h"


#define PARENT_TYPE E_TYPE_STORAGE
static EStorageClass *parent_class = NULL;

#define SUBFOLDER_DIR_NAME     "subfolders"
#define SUBFOLDER_DIR_NAME_LEN 10

struct _ELocalStoragePrivate {
	EComponentRegistry *component_registry;
	char *base_path;
};


/* Utility functions.  */

#if 0
/* Translate a storage path into a real path on the file system.  */
static char *
get_real_path (ELocalStorage *local_storage,
	       const char *path)
{
	EStorage *storage;
	ELocalStoragePrivate *priv;
	const char *p, *newp;
	char *dp;
	char *real_path;
	int real_path_len;
	int base_path_len;

	storage = E_STORAGE (local_storage);
	priv = local_storage->priv;

	/* @path is always absolute, so it starts with a slash.  The base class should
           make sure this is the case; if not, it's broken.  */
	g_assert (*path != G_DIR_SEPARATOR);
	path++;

	/* Calculate the length of the real path. */

	real_path_len = strlen (path);
	real_path_len++;	/* For the ending zero.  */

	base_path_len = strlen (priv->base_path);
	real_path_len += base_path_len;
	real_path_len++;	/* For the separating slash.  */

	/* Take account for the fact that we need to translate every separator into
           `children/'. */
	p = path;
	while (1) {
		newp = strchr (p, G_DIR_SEPARATOR);
		if (newp == NULL)
			break;

		real_path_len += SUBFOLDER_DIR_NAME_LEN;
		real_path_len++; /* For the separating slash.  */

		/* Skip consecutive slashes.  */
		while (*newp == G_DIR_SEPARATOR)
			newp++;

		p = newp;
	};

	real_path = g_malloc (real_path_len);
	dp = real_path;

	memcpy (dp, priv->base_path, base_path_len);
	dp += base_path_len;
	*(dp++) = G_DIR_SEPARATOR;

	/* Copy the mangled path.  */
	p = path;
 	while (1) {
		newp = strchr (p, G_DIR_SEPARATOR);
		if (newp == NULL)
			break;

		memcpy (dp, p, newp - p + 1); /* `+ 1' to copy the slash too.  */
		dp += newp - p + 1;

		memcpy (dp, SUBFOLDER_DIR_NAME, SUBFOLDER_DIR_NAME_LEN);
		dp += SUBFOLDER_DIR_NAME_LEN;

		*(dp++) = G_DIR_SEPARATOR;

		/* Skip consecutive slashes.  */
		while (*newp == G_DIR_SEPARATOR)
			newp++;

		p = newp;
	}

	return real_path;
}
#endif

static gboolean
load_folders (ELocalStorage *local_storage,
	      const char *parent_path,
	      const char *path,
	      const char *physical_path)
{
	DIR *dir;
	char *subfolder_directory_path;

	if (parent_path == NULL) {
		/* On the top level, we don't have any folders and, consequently, no
		   subfolder directory.  */

		subfolder_directory_path = g_strdup (physical_path);
	} else {
		EFolder *folder;

		/*  Otherwise, we have to load the corresponding folder.  */

		folder = e_local_folder_new_from_path (physical_path);
		if (folder == NULL)
			return FALSE;

		e_storage_new_folder (E_STORAGE (local_storage), parent_path, folder);

		subfolder_directory_path = g_concat_dir_and_file (physical_path, SUBFOLDER_DIR_NAME);
	}

	/* Now scan the subfolders and load them.  The subfolders are represented by
           directories under the "SUBFOLDER_DIR_NAME" directory.  */

	dir = opendir (subfolder_directory_path);

	if (dir == NULL) {
		g_free (subfolder_directory_path);
		return FALSE;
	}

	while (1) {
		struct stat file_stat;
		struct dirent *dirent;
		char *file_path;
		char *new_path;

		dirent = readdir (dir);
		if (dirent == NULL)
			break;

		if (strcmp (dirent->d_name, ".") == 0 || strcmp (dirent->d_name, "..") == 0)
			continue;

		file_path = g_concat_dir_and_file (subfolder_directory_path,
						   dirent->d_name);

		if (stat (file_path, &file_stat) < 0) {
			g_free (file_path);
			continue;
		}
		if (! S_ISDIR (file_stat.st_mode)) {
			g_free (file_path);
			continue;
		}

		new_path = g_concat_dir_and_file (path, dirent->d_name);

		load_folders (local_storage, path, new_path, file_path);

		g_free (file_path);
		g_free (new_path);
	}

	closedir (dir);
	g_free (subfolder_directory_path);

	return TRUE;
}

static gboolean
load_all_folders (ELocalStorage *local_storage)
{
	const char *base_path;

	base_path = e_local_storage_get_base_path (local_storage);

	return load_folders (local_storage, NULL, G_DIR_SEPARATOR_S, base_path);
}


/* GtkObject methods.  */

static void
impl_destroy (GtkObject *object)
{
	ELocalStorage *local_storage;
	ELocalStoragePrivate *priv;

	local_storage = E_LOCAL_STORAGE (object);
	priv = local_storage->priv;

	g_free (priv->base_path);

	if (priv->component_registry != NULL)
		gtk_object_unref (GTK_OBJECT (priv->component_registry));
	
	g_free (priv);

	(* GTK_OBJECT_CLASS (parent_class)->destroy) (object);
}


/* EStorage methods.  */

static const char *
impl_get_name (EStorage *storage)
{
	/* FIXME this sucks.  */
	return "local";
}

static void
impl_create_folder (EStorage *storage,
		    const char *path,
		    const char *type,
		    const char *description,
		    EStorageResultCallback callback,
		    void *data)
{
	ELocalStorage *local_storage;

	local_storage = E_LOCAL_STORAGE (storage);
}

static void
impl_remove_folder (EStorage *storage,
		    const char *path,
		    EStorageResultCallback callback,
		    void *data)
{
	ELocalStorage *local_storage;

	local_storage = E_LOCAL_STORAGE (storage);
}


/* Initialization.  */

static void
class_init (ELocalStorageClass *class)
{
	EStorageClass *storage_class;
	GtkObjectClass *object_class;

	parent_class  = gtk_type_class (e_storage_get_type ());
	object_class  = GTK_OBJECT_CLASS (class);
	storage_class = E_STORAGE_CLASS (class);

	object_class->destroy        = impl_destroy;

	storage_class->get_name      = impl_get_name;
	storage_class->create_folder = impl_create_folder;
	storage_class->remove_folder = impl_remove_folder;
}

static void
init (ELocalStorage *local_storage)
{
	ELocalStoragePrivate *priv;

	priv = g_new (ELocalStoragePrivate, 1);

	priv->base_path          = NULL;
	priv->component_registry = NULL;

	local_storage->priv = priv;
}


static gboolean
construct (ELocalStorage *local_storage,
	   EComponentRegistry *component_registry,
	   const char *base_path)
{
	ELocalStoragePrivate *priv;
	int base_path_len;

	e_storage_construct (E_STORAGE (local_storage));

	priv = local_storage->priv;

	base_path_len = strlen (base_path);
	while (base_path_len > 0 && base_path[base_path_len - 1] == G_DIR_SEPARATOR)
		base_path_len--;

	g_return_val_if_fail (base_path_len != 0, FALSE);

	g_assert (priv->component_registry == NULL);
	gtk_object_ref (GTK_OBJECT (component_registry));
	priv->component_registry = component_registry;

	g_assert (priv->base_path == NULL);
	priv->base_path = g_strndup (base_path, base_path_len);

	return load_all_folders (local_storage);
}

EStorage *
e_local_storage_open (EComponentRegistry *component_registry,
		      const char *base_path)
{
	EStorage *new;

	g_return_val_if_fail (component_registry != NULL, NULL);
	g_return_val_if_fail (E_IS_COMPONENT_REGISTRY (component_registry), NULL);
	g_return_val_if_fail (base_path != NULL, NULL);

	new = gtk_type_new (e_local_storage_get_type ());

	if (! construct (E_LOCAL_STORAGE (new), component_registry, base_path)) {
		gtk_object_unref (GTK_OBJECT (new));
		return NULL;
	}

	return new;
}

const char *
e_local_storage_get_base_path (ELocalStorage *local_storage)
{
	g_return_val_if_fail (local_storage != NULL, NULL);
	g_return_val_if_fail (E_IS_LOCAL_STORAGE (local_storage), NULL);

	return local_storage->priv->base_path;
}


E_MAKE_TYPE (e_local_storage, "ELocalStorage", ELocalStorage, class_init, init, PARENT_TYPE)
