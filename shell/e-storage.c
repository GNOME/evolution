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

#include "e-storage.h"


#define PARENT_TYPE GTK_TYPE_OBJECT
static GtkObjectClass *parent_class = NULL;

#define ES_CLASS(obj) \
	E_STORAGE_CLASS (GTK_OBJECT (obj)->klass)

/* This describes a folder and its children.  */
struct _Folder {
	struct _Folder *parent;

	char *path;
	EFolder *e_folder;
	GList *subfolders;
};
typedef struct _Folder Folder;

struct _EStoragePrivate {
	GHashTable *path_to_folder; /* Folder */
};

enum {
	NEW_FOLDER,
	REMOVED_FOLDER,
	LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = { 0 };


/* Folder handling.  */

static Folder *
folder_new (EFolder *e_folder,
	    const char *path)
{
	Folder *folder;

	folder = g_new (Folder, 1);
	folder->path       = g_strdup (path);
	folder->parent     = NULL;
	folder->e_folder   = e_folder;
	folder->subfolders = NULL;

	return folder;
}

static void
folder_remove_subfolder (Folder *folder, Folder *subfolder)
{
	g_list_remove (folder->subfolders, folder);
}

static void
folder_add_subfolder (Folder *folder, Folder *subfolder)
{
	folder->subfolders = g_list_prepend (folder->subfolders, subfolder);
	subfolder->parent = folder;
}

static void
folder_destroy (Folder *folder)
{
	g_assert (folder->subfolders == NULL);

	if (folder->parent != NULL)
		folder_remove_subfolder (folder->parent, folder);

	g_free (folder->path);

	if (folder->e_folder != NULL)
		gtk_object_unref (GTK_OBJECT (folder->e_folder));

	g_free (folder);
}

static void
remove_folder (EStorage *storage,
	       Folder *folder)
{
	EStoragePrivate *priv;

	priv = storage->priv;

	if (folder->subfolders != NULL) {
		GList *p;

		for (p = folder->subfolders; p != NULL; p = p->next) {
			Folder *subfolder;

			subfolder = (Folder *) p->data;
			remove_folder (storage, subfolder);
		}

		g_list_free (folder->subfolders);
		folder->subfolders = NULL;
	}

	g_hash_table_remove (priv->path_to_folder, folder->path);

	folder_destroy (folder);
}

static void
free_private (EStorage *storage)
{
	EStoragePrivate *priv;
	Folder *root_folder;

	priv = storage->priv;

	root_folder = g_hash_table_lookup (priv->path_to_folder, G_DIR_SEPARATOR_S);
	remove_folder (storage, root_folder);

	g_hash_table_destroy (priv->path_to_folder);

	g_free (priv);
}


/* GtkObject methods.  */

static void
destroy (GtkObject *object)
{
	EStorage *storage;

	storage = E_STORAGE (object);

	free_private (storage);

	(* GTK_OBJECT_CLASS (parent_class)->destroy) (object);
}


/* EStorage methods.  */

static GList *
impl_list_folders (EStorage *storage,
		   const char *path)
{
	Folder *folder;
	Folder *subfolder;
	GList *list;
	GList *p;

	folder = g_hash_table_lookup (storage->priv->path_to_folder, path);
	if (folder == NULL)
		return NULL;

	list = NULL;
	for (p = folder->subfolders; p != NULL; p = p->next) {
		subfolder = (Folder *) p->data;

		gtk_object_ref (GTK_OBJECT (subfolder->e_folder));
		list = g_list_prepend (list, subfolder->e_folder);
	}

	return list;
}

static EFolder *
impl_get_folder (EStorage *storage,
		 const char *path)
{
	EStoragePrivate *priv;
	Folder *folder;

	priv = storage->priv;

	folder = g_hash_table_lookup (priv->path_to_folder, path);
	if (folder == NULL)
		return NULL;

	return folder->e_folder;
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
	priv->path_to_folder = g_hash_table_new (g_str_hash, g_str_equal);

	storage->priv = priv;
}


/* Creation.  */

void
e_storage_construct (EStorage *storage)
{
	Folder *root_folder;

	g_return_if_fail (storage != NULL);
	g_return_if_fail (E_IS_STORAGE (storage));

	GTK_OBJECT_UNSET_FLAGS (GTK_OBJECT (storage), GTK_FLOATING);

	root_folder = folder_new (NULL, G_DIR_SEPARATOR_S);
	g_hash_table_insert (storage->priv->path_to_folder, root_folder->path, root_folder);
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


/* These functions are used by subclasses to add and remove folders from the
   state stored in the storage object.  */

gboolean
e_storage_new_folder (EStorage *storage,
		      const char *path,
		      EFolder *e_folder)
{
	EStoragePrivate *priv;
	Folder *folder;
	Folder *parent_folder;
	const char *name;
	char *full_path;

	g_return_val_if_fail (storage != NULL, FALSE);
	g_return_val_if_fail (E_IS_STORAGE (storage), FALSE);
	g_return_val_if_fail (path != NULL, FALSE);
	g_return_val_if_fail (g_path_is_absolute (path), FALSE);
	g_return_val_if_fail (e_folder != NULL, FALSE);
	g_return_val_if_fail (E_IS_FOLDER (e_folder), FALSE);

	priv = storage->priv;

	parent_folder = g_hash_table_lookup (priv->path_to_folder, path);
	if (parent_folder == NULL) {
		g_warning ("%s: Trying to add a subfolder to a path that does not exist yet -- %s",
			   __FUNCTION__, path);
		return FALSE;
	}

	name = e_folder_get_name (e_folder);
	g_assert (name != NULL);
	g_return_val_if_fail (*name != G_DIR_SEPARATOR, FALSE);

	full_path = g_concat_dir_and_file (path, name);

	folder = g_hash_table_lookup (priv->path_to_folder, full_path);
	if (folder != NULL) {
		g_warning ("%s: Trying to add a subfolder for a path that already exists -- %s",
			   __FUNCTION__, full_path);
		return FALSE;
	}

	folder = folder_new (e_folder, full_path);
	folder_add_subfolder (parent_folder, folder);

	g_hash_table_insert (priv->path_to_folder, folder->path, folder);

	g_print ("EStorage: New folder -- %s\n", folder->path);
	gtk_signal_emit (GTK_OBJECT (storage), signals[NEW_FOLDER], folder->path);

	g_free (full_path);

	return TRUE;
}

gboolean
e_storage_removed_folder (EStorage *storage,
			  const char *path)
{
	EStoragePrivate *priv;
	Folder *folder;

	g_return_val_if_fail (storage != NULL, FALSE);
	g_return_val_if_fail (E_IS_STORAGE (storage), FALSE);
	g_return_val_if_fail (path != NULL, FALSE);
	g_return_val_if_fail (g_path_is_absolute (path), FALSE);

	priv = storage->priv;

	folder = g_hash_table_lookup (priv->path_to_folder, path);
	if (folder == NULL) {
		g_warning ("%s: Folder not found -- %s", __FUNCTION__, path);
		return FALSE;
	}

	gtk_signal_emit (GTK_OBJECT (storage), signals[REMOVED_FOLDER], path);

	remove_folder (storage, folder);

	return TRUE;
}


E_MAKE_TYPE (e_storage, "EStorage", EStorage, class_init, init, PARENT_TYPE)
