/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* e-storage.c
 *
 * Copyright (C) 2000  Helix Code, Inc.
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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <gtk/gtkobject.h>
#include <gtk/gtksignal.h>

#include <gnome.h>

#include "e-util/e-util.h"

#include "e-folder-tree.h"

#include "e-storage.h"


#define PARENT_TYPE GTK_TYPE_OBJECT
static GtkObjectClass *parent_class = NULL;

#define ES_CLASS(obj) \
	E_STORAGE_CLASS (GTK_OBJECT (obj)->klass)

struct _EStoragePrivate {
	EFolderTree *folder_tree;
};

enum {
	NEW_FOLDER,
	REMOVED_FOLDER,
	LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = { 0 };


/* Destroy notification function for the folders in the tree.  */

static void
folder_destroy_notify (EFolderTree *tree,
		       const char *path,
		       void *data,
		       void *closure)
{
	EFolder *e_folder;

	if (data == NULL) {
		/* The root folder has no EFolder associated to it.  */
		return;
	}

	e_folder = E_FOLDER (data);
	gtk_object_unref (GTK_OBJECT (e_folder));
}


/* GtkObject methods.  */

static void
destroy (GtkObject *object)
{
	EStorage *storage;
	EStoragePrivate *priv;

	storage = E_STORAGE (object);
	priv = storage->priv;

	if (priv->folder_tree != NULL)
		e_folder_tree_destroy (priv->folder_tree);

	(* GTK_OBJECT_CLASS (parent_class)->destroy) (object);
}


/* EStorage methods.  */

static GList *
impl_list_folders (EStorage *storage,
		   const char *path)
{
	EStoragePrivate *priv;
	GList *path_list;
	GList *list;
	GList *p;

	priv = storage->priv;

	path_list = e_folder_tree_get_subfolders (priv->folder_tree, path);

	list = NULL;
	for (p = path_list; p != NULL; p = p->next) {
		EFolder *e_folder;
		const char *sub_path;

		sub_path = (const char *) p->data;
		e_folder = e_folder_tree_get_folder (priv->folder_tree, sub_path);

		gtk_object_ref (GTK_OBJECT (e_folder));
		list = g_list_prepend (list, e_folder);
	}

	e_free_string_list (path_list);

	return list;
}

static EFolder *
impl_get_folder (EStorage *storage,
		 const char *path)
{
	EStoragePrivate *priv;
	EFolder *e_folder;

	priv = storage->priv;

	e_folder = (EFolder *) e_folder_tree_get_folder (priv->folder_tree, path);

	return e_folder;
}

static const char *
impl_get_name (EStorage *storage)
{
	return _("(No name)");
}

static void
impl_async_create_folder (EStorage *storage,
			  const char *path,
			  const char *type,
			  const char *description,
			  EStorageResultCallback callback,
			  void *data)
{
	(* callback) (storage, E_STORAGE_NOTIMPLEMENTED, data);
}

static void
impl_async_remove_folder (EStorage *storage,
			  const char *path,
			  EStorageResultCallback callback,
			  void *data)
{
	(* callback) (storage, E_STORAGE_NOTIMPLEMENTED, data);
}


/* Initialization.  */

static void
class_init (EStorageClass *class)
{
	GtkObjectClass *object_class;

	object_class = GTK_OBJECT_CLASS (class);
	parent_class = gtk_type_class (gtk_object_get_type ());

	object_class->destroy = destroy;

	class->list_folders        = impl_list_folders;
	class->get_folder          = impl_get_folder;
	class->get_name            = impl_get_name;
	class->async_create_folder = impl_async_create_folder;
	class->async_remove_folder = impl_async_remove_folder;

	signals[NEW_FOLDER] =
		gtk_signal_new ("new_folder",
				GTK_RUN_FIRST,
				object_class->type,
				GTK_SIGNAL_OFFSET (EStorageClass, new_folder),
				gtk_marshal_NONE__STRING,
				GTK_TYPE_NONE, 1,
				GTK_TYPE_STRING);
	signals[REMOVED_FOLDER] =
		gtk_signal_new ("removed_folder",
				GTK_RUN_FIRST,
				object_class->type,
				GTK_SIGNAL_OFFSET (EStorageClass, removed_folder),
				gtk_marshal_NONE__STRING,
				GTK_TYPE_NONE, 1,
				GTK_TYPE_STRING);

	gtk_object_class_add_signals (object_class, signals, LAST_SIGNAL);
}

static void
init (EStorage *storage)
{
	EStoragePrivate *priv;

	priv = g_new (EStoragePrivate, 1);

	priv->folder_tree = e_folder_tree_new (folder_destroy_notify, NULL);

	storage->priv = priv;
}


/* Creation.  */

void
e_storage_construct (EStorage *storage)
{
	g_return_if_fail (storage != NULL);
	g_return_if_fail (E_IS_STORAGE (storage));

	GTK_OBJECT_UNSET_FLAGS (GTK_OBJECT (storage), GTK_FLOATING);
}

EStorage *
e_storage_new (void)
{
	EStorage *new;

	new = gtk_type_new (e_storage_get_type ());

	e_storage_construct (new);

	return new;
}


gboolean
e_storage_path_is_absolute (const char *path)
{
	g_return_val_if_fail (path != NULL, FALSE);

	return *path == G_DIR_SEPARATOR;
}

gboolean
e_storage_path_is_relative (const char *path)
{
	g_return_val_if_fail (path != NULL, FALSE);

	return *path != G_DIR_SEPARATOR;
}


GList *
e_storage_list_folders (EStorage *storage,
			const char *path)
{
	g_return_val_if_fail (storage != NULL, NULL);
	g_return_val_if_fail (E_IS_STORAGE (storage), NULL);
	g_return_val_if_fail (path != NULL, NULL);
	g_return_val_if_fail (g_path_is_absolute (path), NULL);

	return (* ES_CLASS (storage)->list_folders) (storage, path);
}

EFolder *
e_storage_get_folder (EStorage *storage,
		      const char *path)
{
	g_return_val_if_fail (storage != NULL, NULL);
	g_return_val_if_fail (E_IS_STORAGE (storage), NULL);
	g_return_val_if_fail (path != NULL, NULL);
	g_return_val_if_fail (e_storage_path_is_absolute (path), NULL);

	return (* ES_CLASS (storage)->get_folder) (storage, path);
}

const char *
e_storage_get_name (EStorage *storage)
{
	g_return_val_if_fail (storage != NULL, NULL);
	g_return_val_if_fail (E_IS_STORAGE (storage), NULL);

	return (* ES_CLASS (storage)->get_name) (storage);
}


/* Folder operations.  */

void
e_storage_async_create_folder (EStorage *storage,
			       const char *path,
			       const char *type,
			       const char *description,
			       EStorageResultCallback callback,
			       void *data)
{
	g_return_if_fail (storage != NULL);
	g_return_if_fail (E_IS_STORAGE (storage));
	g_return_if_fail (path != NULL);
	g_return_if_fail (g_path_is_absolute (path));
	g_return_if_fail (type != NULL);
	g_return_if_fail (callback != NULL);

	(* ES_CLASS (storage)->async_create_folder) (storage, path, type, description, callback, data);
}

void
e_storage_async_remove_folder (EStorage *storage,
			       const char *path,
			       EStorageResultCallback callback,
			       void *data)
{
	g_return_if_fail (storage != NULL);
	g_return_if_fail (E_IS_STORAGE (storage));
	g_return_if_fail (path != NULL);
	g_return_if_fail (g_path_is_absolute (path));
	g_return_if_fail (callback != NULL);

	(* ES_CLASS (storage)->async_remove_folder) (storage, path, callback, data);
}


const char *
e_storage_result_to_string (EStorageResult result)
{
	switch (result) {
	case E_STORAGE_OK:
		return _("No error");
	case E_STORAGE_GENERICERROR:
		return _("Generic error");
	case E_STORAGE_EXISTS:
		return _("A folder with the same name already exists");
	case E_STORAGE_INVALIDTYPE:
		return _("The specified folder type is not valid");
	case E_STORAGE_IOERROR:
		return _("I/O error");
	case E_STORAGE_NOSPACE:
		return _("Not enough space to create the folder");
	case E_STORAGE_NOTFOUND:
		return _("The specified folder was not found");
	case E_STORAGE_NOTIMPLEMENTED:
		return _("Function not implemented in this storage");
	case E_STORAGE_PERMISSIONDENIED:
		return _("Permission denied");
	case E_STORAGE_UNSUPPORTEDOPERATION:
		return _("Operation not supported");
	case E_STORAGE_UNSUPPORTEDTYPE:
		return _("The specified type is not supported in this storage");
	default:
		return _("Unknown error");
	}
}


/* Public utility functions.  */

struct _GetPathForPhysicalUriForeachData {
	const char *physical_uri;
	char *retval;
};
typedef struct _GetPathForPhysicalUriForeachData GetPathForPhysicalUriForeachData;

static void
get_path_for_physical_uri_foreach (EFolderTree *folder_tree,
				   const char *path,
				   void *data,
				   void *closure)
{
	GetPathForPhysicalUriForeachData *foreach_data;
	const char *physical_uri;
	EFolder *e_folder;

	foreach_data = (GetPathForPhysicalUriForeachData *) data;
	if (foreach_data->retval != NULL)
		return;

	e_folder = (EFolder *) data;
	if (e_folder == NULL)
		return;

	physical_uri = e_folder_get_physical_uri (e_folder);

	if (strcmp (foreach_data->physical_uri, physical_uri) == 0)
		foreach_data->retval = g_strdup (path);
}

/**
 * e_storage_get_path_for_physical_uri:
 * @storage: A storage
 * @physical_uri: A physical URI
 * 
 * Look for the folder having the specified @physical_uri.
 * 
 * Return value: The path of the folder having the specified @physical_uri in
 * @storage.  If such a folder does not exist, just return NULL.  The return
 * value must be freed by the caller.
 **/
char *
e_storage_get_path_for_physical_uri (EStorage *storage,
				     const char *physical_uri)
{
	GetPathForPhysicalUriForeachData foreach_data;
	EStoragePrivate *priv;

	g_return_val_if_fail (storage != NULL, NULL);
	g_return_val_if_fail (E_IS_STORAGE (storage), NULL);
	g_return_val_if_fail (physical_uri != NULL, NULL);

	priv = storage->priv;

	foreach_data.physical_uri = physical_uri;
	foreach_data.retval       = NULL;

	e_folder_tree_foreach (priv->folder_tree, get_path_for_physical_uri_foreach, &foreach_data);

	return foreach_data.retval;
}


/* Protected functions.  */

/* These functions are used by subclasses to add and remove folders from the
   state stored in the storage object.  */

gboolean
e_storage_new_folder (EStorage *storage,
		      const char *path,
		      EFolder *e_folder)
{
	EStoragePrivate *priv;

	g_return_val_if_fail (storage != NULL, FALSE);
	g_return_val_if_fail (E_IS_STORAGE (storage), FALSE);
	g_return_val_if_fail (path != NULL, FALSE);
	g_return_val_if_fail (g_path_is_absolute (path), FALSE);
	g_return_val_if_fail (e_folder != NULL, FALSE);
	g_return_val_if_fail (E_IS_FOLDER (e_folder), FALSE);

	priv = storage->priv;

	if (! e_folder_tree_add (priv->folder_tree, path, e_folder))
		return FALSE;

	gtk_signal_emit (GTK_OBJECT (storage), signals[NEW_FOLDER], path);

	return TRUE;
}

gboolean
e_storage_removed_folder (EStorage *storage,
			  const char *path)
{
	EStoragePrivate *priv;

	g_return_val_if_fail (storage != NULL, FALSE);
	g_return_val_if_fail (E_IS_STORAGE (storage), FALSE);
	g_return_val_if_fail (path != NULL, FALSE);
	g_return_val_if_fail (g_path_is_absolute (path), FALSE);

	priv = storage->priv;

	if (e_folder_tree_get_folder (priv->folder_tree, path) == NULL)
		return FALSE;

	gtk_signal_emit (GTK_OBJECT (storage), signals[REMOVED_FOLDER], path);

	e_folder_tree_remove (priv->folder_tree, path);

	return TRUE;
}


E_MAKE_TYPE (e_storage, "EStorage", EStorage, class_init, init, PARENT_TYPE)
