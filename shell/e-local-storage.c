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
 *   - If the LocalStorage is destroyed and an async operation on a shell component is
 *     pending, we get a callback on a bogus object.  We need support for cancelling
 *     operations on the shell component.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <dirent.h>

#include <errno.h>
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
	EFolderTypeRegistry *folder_type_registry;
	char *base_path;
};


/* Utility functions.  */

/* Translate a storage path into a physical path on the file system.  */
static char *
get_physical_path (ELocalStorage *local_storage,
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
	g_assert (*path == G_DIR_SEPARATOR);
	path++;

	/* Calculate the length of the real path. */

	real_path_len = strlen (path);
	real_path_len++;	/* For the ending zero.  */

	base_path_len = strlen (priv->base_path);
	real_path_len += base_path_len;
	real_path_len++;	/* For the separating slash.  */

	/* Take account for the fact that we need to translate every separator into
           `subfolders/'. */
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
		if (newp == NULL) {
			strcpy (dp, p);
			break;
		}

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

		e_storage_new_folder (E_STORAGE (local_storage), path, folder);

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

static EStorageResult
errno_to_storage_result (void)
{
	EStorageResult storage_result;

	switch (errno) {
	case EACCES:
	case EROFS:
		storage_result = E_STORAGE_PERMISSIONDENIED;
		break;
	case EEXIST:
		storage_result = E_STORAGE_EXISTS;
		break;
	case ENOSPC:
		storage_result = E_STORAGE_NOSPACE;
		break;
	default:
		storage_result = E_STORAGE_GENERICERROR;
	}

	return storage_result;
}

static EStorageResult
shell_component_result_to_storage_result (EvolutionShellComponentResult result)
{
	/* FIXME: Maybe we need better mapping here.  */
	switch (result) {
	case EVOLUTION_SHELL_COMPONENT_OK:
		return E_STORAGE_OK;
	case EVOLUTION_SHELL_COMPONENT_NOTFOUND:
		return E_STORAGE_NOTFOUND;
	case EVOLUTION_SHELL_COMPONENT_UNSUPPORTEDTYPE:
		return E_STORAGE_UNSUPPORTEDTYPE;
	case EVOLUTION_SHELL_COMPONENT_UNSUPPORTEDOPERATION:
		return E_STORAGE_UNSUPPORTEDOPERATION;
	case EVOLUTION_SHELL_COMPONENT_EXISTS:
		return E_STORAGE_EXISTS;
	case EVOLUTION_SHELL_COMPONENT_PERMISSIONDENIED:
		return E_STORAGE_PERMISSIONDENIED;
	case EVOLUTION_SHELL_COMPONENT_ALREADYOWNED:
	case EVOLUTION_SHELL_COMPONENT_BUSY:
	case EVOLUTION_SHELL_COMPONENT_CORBAERROR:
	case EVOLUTION_SHELL_COMPONENT_HASSUBFOLDERS:
	case EVOLUTION_SHELL_COMPONENT_INTERNALERROR:
	case EVOLUTION_SHELL_COMPONENT_INTERRUPTED:
	case EVOLUTION_SHELL_COMPONENT_INVALIDARG:
	case EVOLUTION_SHELL_COMPONENT_INVALIDURI:
	case EVOLUTION_SHELL_COMPONENT_NOSPACE:
	case EVOLUTION_SHELL_COMPONENT_NOTOWNED:
	case EVOLUTION_SHELL_COMPONENT_UNKNOWNERROR:
	default:
		return E_STORAGE_GENERICERROR;
	}
}


/* Callbacks for the async methods invoked on the `Evolution::ShellComponent's.  */

struct _AsyncCreateFolderCallbackData {
	EStorage *storage;

	char *path;
	char *display_name;
	char *type;
	char *description;
	char *physical_uri;
	char *physical_path;

	EStorageResultCallback callback;
	void *callback_data;
};
typedef struct _AsyncCreateFolderCallbackData AsyncCreateFolderCallbackData;

static void
component_async_create_folder_callback (EvolutionShellComponentClient *shell_component_client,
					EvolutionShellComponentResult result,
					void *data)
{
	AsyncCreateFolderCallbackData *callback_data;

	callback_data = (AsyncCreateFolderCallbackData *) data;

	if (result != EVOLUTION_SHELL_COMPONENT_OK) {
		/* XXX: This assumes the component won't leave any files in the directory.  */
		rmdir (callback_data->physical_path);
	} else {
		EFolder *folder;

		folder = e_local_folder_new (callback_data->display_name,
					     callback_data->type,
					     callback_data->description);

		e_folder_set_physical_uri (folder, callback_data->physical_uri);

		if (e_local_folder_save (E_LOCAL_FOLDER (folder))) {
			e_storage_new_folder (callback_data->storage,
					      callback_data->path,
					      folder);
		} else {
			rmdir (callback_data->physical_path);
			gtk_object_unref (GTK_OBJECT (folder));
			result = E_STORAGE_IOERROR;
		}
	}

	bonobo_object_unref (BONOBO_OBJECT (shell_component_client));

	(* callback_data->callback) (callback_data->storage,
				     shell_component_result_to_storage_result (result),
				     callback_data->callback_data);

	g_free (callback_data->path);
	g_free (callback_data->display_name);
	g_free (callback_data->type);
	g_free (callback_data->description);
	g_free (callback_data->physical_uri);
	g_free (callback_data->physical_path);
	g_free (callback_data);
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

	if (priv->folder_type_registry != NULL)
		gtk_object_unref (GTK_OBJECT (priv->folder_type_registry));
	
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
impl_async_create_folder (EStorage *storage,
			  const char *path,
			  const char *type,
			  const char *description,
			  EStorageResultCallback callback,
			  void *data)
{
	ELocalStorage *local_storage;
	ELocalStoragePrivate *priv;
	EvolutionShellComponentClient *component_client;
	const char *folder_name;
	AsyncCreateFolderCallbackData *callback_data;
	char *physical_path;
	char *physical_uri;
	char *parent_path;

	local_storage = E_LOCAL_STORAGE (storage);
	priv = local_storage->priv;

	component_client = e_folder_type_registry_get_handler_for_type (priv->folder_type_registry,
									type);
	if (component_client == NULL) {
		(* callback) (storage, E_STORAGE_INVALIDTYPE, data);
		return;
	}

	g_assert (g_path_is_absolute (path));

	folder_name = g_basename (path);
	if (folder_name == path + 1) {
		/* We want a direct child of the root, so we don't need to create a
		   `subfolders' directory.  */
		physical_path = get_physical_path (local_storage, path);
		parent_path = g_strdup (G_DIR_SEPARATOR_S);
	} else {
		char *parent_physical_path;
		char *subfolders_directory_physical_path;

		/* Create the `subfolders' subdirectory under the parent.  */

		parent_path = g_strndup (path, folder_name - path - 1);
		parent_physical_path = get_physical_path (local_storage, parent_path);
		subfolders_directory_physical_path = g_concat_dir_and_file (parent_physical_path,
									    SUBFOLDER_DIR_NAME);

		if (! g_file_exists (subfolders_directory_physical_path)
		    && mkdir (subfolders_directory_physical_path, 0700) == -1) {
			g_free (parent_path);
			g_free (subfolders_directory_physical_path);
			g_free (parent_physical_path);

			(* callback) (storage, errno_to_storage_result (), data);
			return;
		}

		physical_path = g_concat_dir_and_file (subfolders_directory_physical_path,
						       folder_name);
		g_free (subfolders_directory_physical_path);
		g_free (parent_physical_path);
	}

	/* Create the directory that holds the folder.  */

	if (mkdir (physical_path, 0700) == -1) {
		g_free (physical_path);
		(* callback) (storage, errno_to_storage_result (), data);
		return;
	}

	/* Finally tell the component to do the job of creating the physical files in
           it. */
	/* FIXME: We should put the operations on a queue so that we can cancel them when
           the ELocalStorage is destroyed.  */

	physical_uri = g_strconcat ("file://", physical_path, NULL);

	callback_data = g_new (AsyncCreateFolderCallbackData, 1);
	callback_data->storage       = storage;
	callback_data->path          = path;
	callback_data->display_name  = g_strdup (folder_name);
	callback_data->type          = g_strdup (type);
	callback_data->description   = g_strdup (description);
	callback_data->physical_uri  = physical_uri;
	callback_data->physical_path = physical_path;
	callback_data->callback      = callback;
	callback_data->callback_data = data;

	bonobo_object_ref (BONOBO_OBJECT (component_client));

	evolution_shell_component_client_async_create_folder (component_client,
							      physical_path,
							      type,
							      component_async_create_folder_callback,
							      callback_data);
}

static void
impl_async_remove_folder (EStorage *storage,
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

	object_class->destroy              = impl_destroy;

	storage_class->get_name            = impl_get_name;
	storage_class->async_create_folder = impl_async_create_folder;
	storage_class->async_remove_folder = impl_async_remove_folder;
}

static void
init (ELocalStorage *local_storage)
{
	ELocalStoragePrivate *priv;

	priv = g_new (ELocalStoragePrivate, 1);

	priv->base_path          = NULL;
	priv->folder_type_registry = NULL;

	local_storage->priv = priv;
}


static gboolean
construct (ELocalStorage *local_storage,
	   EFolderTypeRegistry *folder_type_registry,
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

	g_assert (priv->folder_type_registry == NULL);
	gtk_object_ref (GTK_OBJECT (folder_type_registry));
	priv->folder_type_registry = folder_type_registry;

	g_assert (priv->base_path == NULL);
	priv->base_path = g_strndup (base_path, base_path_len);

	return load_all_folders (local_storage);
}

EStorage *
e_local_storage_open (EFolderTypeRegistry *folder_type_registry,
		      const char *base_path)
{
	EStorage *new;

	g_return_val_if_fail (folder_type_registry != NULL, NULL);
	g_return_val_if_fail (E_IS_FOLDER_TYPE_REGISTRY (folder_type_registry), NULL);
	g_return_val_if_fail (base_path != NULL, NULL);

	new = gtk_type_new (e_local_storage_get_type ());

	if (! construct (E_LOCAL_STORAGE (new), folder_type_registry, base_path)) {
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
