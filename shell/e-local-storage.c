/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* e-local-storage.c
 *
 * Copyright (C) 2000, 2001 Ximian, Inc.
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
 *
 *   - The tree is kept both in the EStorage and the EvolutionStorage.  Very
 *     bad design.
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

#include <bonobo.h>

#include <gal/util/e-util.h>
#include "e-local-folder.h"

#include "evolution-local-storage.h"

#include "e-local-storage.h"


#define PARENT_TYPE E_TYPE_STORAGE
static EStorageClass *parent_class = NULL;

#define SUBFOLDER_DIR_NAME     "subfolders"
#define SUBFOLDER_DIR_NAME_LEN 10

struct _ELocalStoragePrivate {
	EFolderTypeRegistry *folder_type_registry;
	char *base_path;
	EvolutionLocalStorage *bonobo_interface;
};


/* EStorageResult <-> errno mapping.  */

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

static void
new_folder (ELocalStorage *local_storage,
	    const char *path,
	    EFolder *folder)
{
	ELocalStoragePrivate *priv;

	priv = local_storage->priv;

	e_storage_new_folder (E_STORAGE (local_storage), path, folder);

	evolution_storage_new_folder (EVOLUTION_STORAGE (priv->bonobo_interface),
				      path,
				      e_folder_get_name (folder),
				      e_folder_get_type_string (folder),
				      e_folder_get_physical_uri (folder),
				      e_folder_get_description (folder),
				      e_folder_get_highlighted (folder));
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

		new_folder (local_storage, path, folder);

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

static void
notify_bonobo_listener (const Bonobo_Listener listener,
			EStorageResult result,
			const char *physical_path)
{
	CORBA_any any;
	GNOME_Evolution_Storage_FolderResult folder_result;
	CORBA_Environment ev;

	folder_result.result = result;
	folder_result.path = CORBA_string_dup (physical_path ? physical_path : "");
	any._type = TC_GNOME_Evolution_Storage_FolderResult;
	any._value = &folder_result;

	CORBA_exception_init (&ev);
	Bonobo_Listener_event (listener, "evolution-shell:folder_created", 
			       &any, &ev);
	CORBA_exception_free (&ev);
}


/* Callbacks for the async methods invoked on the `Evolution::ShellComponent's.  */

struct _AsyncCreateFolderCallbackData {
	EStorage *storage;
	Bonobo_Listener listener;

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
	EStorageResult storage_result;

	callback_data = (AsyncCreateFolderCallbackData *) data;

	storage_result = shell_component_result_to_storage_result (result);

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
			new_folder (E_LOCAL_STORAGE (callback_data->storage),
				    callback_data->path, folder);
		} else {
			rmdir (callback_data->physical_path);
			gtk_object_unref (GTK_OBJECT (folder));
			storage_result = E_STORAGE_IOERROR;
		}
	}

	bonobo_object_unref (BONOBO_OBJECT (shell_component_client));

	if (callback_data->listener != CORBA_OBJECT_NIL)
		notify_bonobo_listener (callback_data->listener, storage_result,
					callback_data->physical_path);

	if (callback_data->callback != NULL)
		(* callback_data->callback) (callback_data->storage,
					     storage_result,
					     callback_data->callback_data);
	
	g_free (callback_data->path);
	g_free (callback_data->display_name);
	g_free (callback_data->type);
	g_free (callback_data->description);
	g_free (callback_data->physical_uri);
	g_free (callback_data->physical_path);
	g_free (callback_data);
}


/* Implementation for the folder operations.  */

static EStorageResult
create_folder_directory (ELocalStorage *local_storage,
			 const char *path,
			 const char *type,
			 const char *description,
			 char **physical_path_return)
{
	EStorage *storage;
	ELocalStoragePrivate *priv;
	const char *folder_name;
	char *physical_path;
	char *parent_path;
	
	storage = E_STORAGE (local_storage);
	priv = local_storage->priv;

	*physical_path_return = NULL;
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

		if (! g_file_exists (subfolders_directory_physical_path)) {
			if (mkdir (subfolders_directory_physical_path, 0700) == -1) {
				g_free (subfolders_directory_physical_path);
				g_free (parent_physical_path);
				return errno_to_storage_result ();
			}
		}

		physical_path = g_concat_dir_and_file (subfolders_directory_physical_path,
						       folder_name);
		g_free (subfolders_directory_physical_path);
		g_free (parent_physical_path);
	}
	
	/* Create the directory that holds the folder.  */
	
	if (mkdir (physical_path, 0700) == -1) {
		g_free (physical_path);
		return errno_to_storage_result ();
	}

	*physical_path_return = physical_path;
	return E_STORAGE_OK;
}

static void
create_folder (ELocalStorage *local_storage,
	       Bonobo_Listener listener,
	       const char *path,
	       const char *type,
	       const char *description,
	       EStorageResultCallback callback,
	       void *data)
{
	EStorage *storage;
	ELocalStoragePrivate *priv;
	EvolutionShellComponentClient *component_client;
	AsyncCreateFolderCallbackData *callback_data;
	EStorageResult result;
	const char *folder_name;
	char *physical_path;
	char *physical_uri;
	
	storage = E_STORAGE (local_storage);
	priv = local_storage->priv;
	component_client = e_folder_type_registry_get_handler_for_type (priv->folder_type_registry,
									type);
	if (component_client == NULL) {
		if (callback != NULL)
			(* callback) (storage, E_STORAGE_INVALIDTYPE, data);
		if (listener != CORBA_OBJECT_NIL)
			notify_bonobo_listener (listener, E_STORAGE_INVALIDTYPE, NULL);
		return;
	}
	
	g_assert (g_path_is_absolute (path));

	result = create_folder_directory (local_storage, path, type, description, &physical_path);
	if (result != E_STORAGE_OK) {
		if (callback != NULL)
			(* callback) (storage, result, data);
		if (listener != CORBA_OBJECT_NIL)
			notify_bonobo_listener (listener, result, NULL);
		return;
	}
	
	folder_name = g_basename (path);

	/* Finally tell the component to do the job of creating the physical files in it.  */

	/* FIXME: We should put the operations on a queue so that we can cancel them when
           the ELocalStorage is destroyed.  */
	
	physical_uri = g_strconcat ("file://", physical_path, NULL);
	
	callback_data = g_new (AsyncCreateFolderCallbackData, 1);
	callback_data->storage       = E_STORAGE (local_storage);
	callback_data->path          = g_strdup (path);
	callback_data->display_name  = g_strdup (folder_name);
	callback_data->type          = g_strdup (type);
	callback_data->description   = g_strdup (description);
	callback_data->physical_uri  = physical_uri;
	callback_data->physical_path = physical_path;
	callback_data->listener      = listener;
	callback_data->callback      = callback;
	callback_data->callback_data = data;
	
	bonobo_object_ref (BONOBO_OBJECT (component_client));
	
	evolution_shell_component_client_async_create_folder (component_client,
							      physical_uri,
							      type,
							      component_async_create_folder_callback,
							      callback_data);
}

static EStorageResult
remove_folder_directory (ELocalStorage *local_storage,
			 const char *path)
{
	char *physical_path;
	char *file_name;

	physical_path = get_physical_path (local_storage, path);

	/* 1. Delete the subfolder directory.  If this fails, it means that we
	   have subfolders.  */
	file_name = g_concat_dir_and_file (physical_path, SUBFOLDER_DIR_NAME);
	if (g_file_exists (file_name) && rmdir (file_name) == -1)
		return E_STORAGE_NOTEMPTY; /* FIXME? */
	g_free (file_name);

	/* 2. Delete the metadata file associated with this folder.  */
	file_name = g_concat_dir_and_file (physical_path, E_LOCAL_FOLDER_METADATA_FILE_NAME);
	unlink (file_name);
	g_free (file_name);

	/* 3. Delete the physical directory.  */
	if (rmdir (physical_path) == -1) {
		g_free (physical_path);
		return E_STORAGE_GENERICERROR;
	}

	g_free (physical_path);
	return E_STORAGE_OK;
}


/* GtkObject methods.  */

static void
impl_destroy (GtkObject *object)
{
	ELocalStorage *local_storage;
	ELocalStoragePrivate *priv;
	CORBA_Environment ev;

	local_storage = E_LOCAL_STORAGE (object);
	priv = local_storage->priv;

	CORBA_exception_init (&ev);

	g_free (priv->base_path);

	if (priv->folder_type_registry != NULL)
		gtk_object_unref (GTK_OBJECT (priv->folder_type_registry));

	if (priv->bonobo_interface != NULL)
		bonobo_object_unref (BONOBO_OBJECT (priv->bonobo_interface));

	g_free (priv);

	CORBA_exception_free (&ev);

	(* GTK_OBJECT_CLASS (parent_class)->destroy) (object);
}


/* EStorage methods.  */

static const char *
impl_get_name (EStorage *storage)
{
	return E_LOCAL_STORAGE_NAME;
}


/* Creating folders.  */

static void
impl_async_create_folder (EStorage *storage,
			  const char *path,
			  const char *type,
			  const char *description,
			  EStorageResultCallback callback,
			  void *data)
{
	ELocalStorage *local_storage;

	local_storage = E_LOCAL_STORAGE (storage);

	create_folder (local_storage, NULL, path, type, description, callback, data);
}


/* Removing folders.  */

static void
impl_async_remove_folder (EStorage *storage,
			  const char *path,
			  EStorageResultCallback callback,
			  void *data)
{
	ELocalStorage *local_storage;

	local_storage = E_LOCAL_STORAGE (storage);
}



/* Transferring folders.  */

struct _XferItem {
	char *source_path;
	char *destination_path;
};
typedef struct _XferItem XferItem;

static XferItem *
xfer_item_new (char *source_path,
	       char *destination_path)
{
	XferItem *new;

	new = g_new (XferItem, 1);
	new->source_path      = source_path;
	new->destination_path = destination_path;

	return new;
}

static void
xfer_item_free (XferItem *item)
{
	g_free (item->source_path);
	g_free (item->destination_path);
	g_free (item);
}

static void
append_xfer_item_list (EStorage *storage,
		       char *source_path,
		       char *destination_path,
		       GList **list)
{
	GList *subfolders;
	GList *p;

	*list = g_list_prepend (*list, xfer_item_new (source_path, destination_path));

	subfolders = e_storage_get_subfolder_paths (storage, source_path);
	for (p = subfolders; p != NULL; p = p->next) {
		char *source_subpath;
		char *destination_subpath;

		source_subpath = g_strdup ((const char *) p->data);
		destination_subpath = g_concat_dir_and_file (destination_path,
							     g_basename (source_subpath));

		append_xfer_item_list (storage, source_subpath, destination_subpath, list);
	}

	e_free_string_list (subfolders);
}

struct _XferData {
	/* The storage on which we are performing the xfer operation.  */
	ELocalStorage *local_storage;

	/* List of source/destination path couples to copy, in the right
	   order.  */
	GList *folder_items;

	/* Pointer into `folder_items'.  The folder item pointed by this is the
	   one handled by the previous CORBA invocation.  */
	GList *current_folder_item;

	/* Whether we want to remove the source too.  */
	gboolean remove_source;

	/* The callback, with its data.  */
	EStorageResultCallback callback;
	void *callback_data;
};
typedef struct _XferData XferData;

static void
async_xfer_folder_step (ELocalStorage *local_storage,
			const char *source_path,
			const char *destination_path,
			gboolean remove_source,
			EvolutionShellComponentClientCallback component_client_callback,
			void *component_client_callback_data)
{
	ELocalStoragePrivate *priv;
	EFolder *source_folder;
	EvolutionShellComponentClient *component_client;
	char *physical_path;
	char *physical_uri;

	priv = local_storage->priv;

	g_print ("async_xfer_folder_step %s -> %s\n", source_path, destination_path);

	source_folder = e_storage_get_folder (E_STORAGE (local_storage), source_path);
	g_assert (source_folder != NULL);

	create_folder_directory (local_storage, destination_path,
				 e_folder_get_type_string (source_folder),
				 e_folder_get_description (source_folder),
				 &physical_path);

	physical_uri = g_strconcat ("file://", physical_path, NULL);
	g_free (physical_path);

	component_client = e_folder_type_registry_get_handler_for_type (priv->folder_type_registry,
									e_folder_get_type_string (source_folder));
	g_assert (component_client != NULL);

	evolution_shell_component_client_async_xfer_folder (component_client,
							    e_folder_get_physical_uri (source_folder),
							    physical_uri,
							    remove_source,
							    component_client_callback,
							    component_client_callback_data);

	g_free (physical_uri);
}

static void
async_xfer_folder_complete (XferData *xfer_data)
{
	ELocalStorage *local_storage;
	GList *p;

	local_storage = xfer_data->local_storage;

#if 0
	if (xfer_data->remove_source) {
		EStorageResult result;

		/* Remove all the source physical directories, and also the
		   corresponding folders from the folder tree.  */

		for (p = g_list_last (xfer_data->folder_items); p != NULL; p = p->prev) {
			XferItem *item;

			item = (XferItem *) p->data;

			result = remove_folder_directory (local_storage, item->source_path);

			/* FIXME handle failure differently?  This should be n
			   unlikely situation.  */
			if (result == E_STORAGE_OK)
				e_storage_removed_folder (E_STORAGE (local_storage), item->source_path);
		}
	}
#endif

	/* Free the data.  */

	for (p = xfer_data->folder_items; p != NULL; p = p->next) {
		XferItem *item;

		item = (XferItem *) p->data;
		xfer_item_free (item);
	}
	g_list_free (xfer_data->folder_items);

	g_free (xfer_data);
}

static void
async_xfer_folder_callback (EvolutionShellComponentClient *shell_component_client,
			    EvolutionShellComponentResult result,
			    void *callback_data)
{
	XferData *xfer_data;
	XferItem *item;
	EFolder *source_folder;
	EFolder *destination_folder;
	char *new_physical_uri;

	/* FIXME handle errors.  */

	xfer_data = (XferData *) callback_data;

	item = (XferItem *) xfer_data->current_folder_item->data;

	source_folder = e_storage_get_folder (E_STORAGE (xfer_data->local_storage), item->source_path);
	destination_folder = e_local_folder_new (e_folder_get_name (source_folder),
						 e_folder_get_type_string (source_folder),
						 e_folder_get_description (source_folder));

	new_physical_uri = g_strconcat ("file:///", item->destination_path, NULL);
	e_folder_set_physical_uri (destination_folder, new_physical_uri);
	g_free (new_physical_uri);

	e_local_folder_save (E_LOCAL_FOLDER (new_folder)); /* FIXME check for errors */
	new_folder (xfer_data->local_storage, item->destination_path, destination_folder);

	xfer_data->current_folder_item = xfer_data->current_folder_item->next;
	if (xfer_data->current_folder_item == NULL) {
		async_xfer_folder_complete (xfer_data);
		return;
	}

	item = (XferItem *) xfer_data->current_folder_item->data;

	async_xfer_folder_step (xfer_data->local_storage,
				item->source_path,
				item->destination_path,
				xfer_data->remove_source,
				async_xfer_folder_callback,
				xfer_data);
}

static void
impl_async_xfer_folder (EStorage *storage,
			const char *source_path,
			const char *destination_path,
			gboolean remove_source,
			EStorageResultCallback callback,
			void *callback_data)
{
	ELocalStorage *local_storage;
	ELocalStoragePrivate *priv;
	XferData *xfer_data;
	GList *folder_items;	/* <XferItem> */
	XferItem *first_item;

	local_storage = E_LOCAL_STORAGE (storage);
	priv = local_storage->priv;

	folder_items = NULL;
	append_xfer_item_list (storage, g_strdup (source_path), g_strdup (destination_path), &folder_items);
	folder_items = g_list_reverse (folder_items); /* lame */

	xfer_data = g_new (XferData, 1);
	xfer_data->local_storage       = local_storage;
	xfer_data->folder_items        = folder_items;
	xfer_data->current_folder_item = folder_items;
	xfer_data->remove_source       = remove_source;
	xfer_data->callback            = callback;
	xfer_data->callback_data       = callback_data;

	first_item = (XferItem *) xfer_data->folder_items->data;

	async_xfer_folder_step (E_LOCAL_STORAGE (storage),
				first_item->source_path,
				first_item->destination_path,
				remove_source,
				async_xfer_folder_callback,
				xfer_data);
}


/* Callbacks for the `Evolution::LocalStorage' interface we are exposing to the outside world.  */
static void
bonobo_interface_create_folder_cb (EvolutionStorage *storage,
				   const Bonobo_Listener listener,
				   const char *path,
				   const char *type,
				   const char *description,
				   const char *parent_path,
				   void *data)
{
	ELocalStorage *local_storage;

	local_storage = E_LOCAL_STORAGE (data);

	create_folder (local_storage, listener, path, type, description, NULL, NULL);
}

static void
bonobo_interface_update_folder_cb (EvolutionLocalStorage *bonobo_local_storage,
				   const char *path,
				   const char *display_name,
				   gboolean highlighted,
				   void *data)
{
	ELocalStorage *local_storage;
	EFolder *folder;

	local_storage = E_LOCAL_STORAGE (data);

	folder = e_storage_get_folder (E_STORAGE (local_storage), path);
	if (folder == NULL)
		return;

	e_folder_set_name (folder, display_name);
	e_folder_set_highlighted (folder, highlighted);
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
	storage_class->async_xfer_folder   = impl_async_xfer_folder;
}

static void
init (ELocalStorage *local_storage)
{
	ELocalStoragePrivate *priv;

	priv = g_new (ELocalStoragePrivate, 1);

	priv->base_path            = NULL;
	priv->folder_type_registry = NULL;
	priv->bonobo_interface     = NULL;

	local_storage->priv = priv;
}


static gboolean
construct (ELocalStorage *local_storage,
	   EFolderTypeRegistry *folder_type_registry,
	   const char *base_path)
{
	ELocalStoragePrivate *priv;
	int base_path_len;

	e_storage_construct (E_STORAGE (local_storage), NULL, NULL);

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

	g_assert (priv->bonobo_interface == NULL);
	priv->bonobo_interface = evolution_local_storage_new (E_LOCAL_STORAGE_NAME);

	gtk_signal_connect (GTK_OBJECT (priv->bonobo_interface), "create_folder",
			    GTK_SIGNAL_FUNC (bonobo_interface_create_folder_cb), 
			    local_storage);
	gtk_signal_connect (GTK_OBJECT (priv->bonobo_interface), "update_folder",
			    GTK_SIGNAL_FUNC (bonobo_interface_update_folder_cb),
			    local_storage);

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


const GNOME_Evolution_LocalStorage
e_local_storage_get_corba_interface (ELocalStorage *local_storage)
{
	ELocalStoragePrivate *priv;
	GNOME_Evolution_LocalStorage corba_interface;

	g_return_val_if_fail (local_storage != NULL, NULL);
	g_return_val_if_fail (E_IS_LOCAL_STORAGE (local_storage), NULL);

	priv = local_storage->priv;
	corba_interface = bonobo_object_corba_objref (BONOBO_OBJECT (priv->bonobo_interface));

	return corba_interface;
}


E_MAKE_TYPE (e_local_storage, "ELocalStorage", ELocalStorage, class_init, init, PARENT_TYPE)
