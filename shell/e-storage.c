/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* e-storage.c
 *
 * Copyright (C) 2000, 2001 Ximian, Inc.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of version 2 of the GNU General Public
 * License as published by the Free Software Foundation.
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

/* FIXME: The EFolderTree is kept both in the EStorage and the
 * EvolutionStorage.  Bad design.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "e-storage.h"

#include "e-folder-tree.h"
#include "e-shell-constants.h"

#include <gtk/gtkobject.h>
#include <gtk/gtksignal.h>

#include <libgnome/gnome-defs.h>
#include <libgnome/gnome-i18n.h>
#include <gal/util/e-util.h>


#define PARENT_TYPE GTK_TYPE_OBJECT
static GtkObjectClass *parent_class = NULL;

#define ES_CLASS(obj) \
	E_STORAGE_CLASS (GTK_OBJECT (obj)->klass)

struct _EStoragePrivate {
	/* The set of folders we have in this storage.  */
	EFolderTree *folder_tree;

	/* The pseudofolders representing un-filled-in subtrees */
	GHashTable *pseudofolders;

	/* Internal name of the storage */
	char *name;
};

enum {
	NEW_FOLDER,
	UPDATED_FOLDER,
	REMOVED_FOLDER,
	CLOSE_FOLDER,
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


/* Signal callbacks for the EFolders.  */

static void
folder_changed_cb (EFolder *folder,
		   void *data)
{
	EStorage *storage;
	EStoragePrivate *priv;
	const char *path, *p;
	gboolean highlight;

	g_assert (E_IS_STORAGE (data));

	storage = E_STORAGE (data);
	priv = storage->priv;

	path = e_folder_tree_get_path_for_data (priv->folder_tree, folder);
	g_assert (path != NULL);

	gtk_signal_emit (GTK_OBJECT (storage), signals[UPDATED_FOLDER], path);

	highlight = GPOINTER_TO_INT (gtk_object_get_data (GTK_OBJECT (folder), "last_highlight"));
	if (highlight != e_folder_get_highlighted (folder)) {
		highlight = !highlight;
		gtk_object_set_data (GTK_OBJECT (folder), "last_highlight",
				     GINT_TO_POINTER (highlight));
		p = strrchr (path, '/');
		if (p && p != path) {
			char *name;
			
			name = g_strndup (path, p - path);
			folder = e_folder_tree_get_folder (priv->folder_tree, name);
			g_free (name);
			if (folder)
				e_folder_set_child_highlight (folder, highlight);
		}
	}
}


/* GtkObject methods.  */

static void
free_folder (gpointer path, gpointer folder, gpointer user_data)
{
	g_free (path);
	/* folders will have been freed by e_folder_tree_destroy */
}

static void
destroy (GtkObject *object)
{
	EStorage *storage;
	EStoragePrivate *priv;

	storage = E_STORAGE (object);
	priv = storage->priv;

	if (priv->folder_tree != NULL)
		e_folder_tree_destroy (priv->folder_tree);
	if (priv->pseudofolders) {
		g_hash_table_foreach (priv->pseudofolders, free_folder, NULL);
		g_hash_table_destroy (priv->pseudofolders);
	}

	g_free (priv->name);

	(* GTK_OBJECT_CLASS (parent_class)->destroy) (object);
}


/* EStorage methods.  */

static GList *
impl_get_subfolder_paths (EStorage *storage,
			  const char *path)
{
	EStoragePrivate *priv;

	priv = storage->priv;

	return e_folder_tree_get_subfolders (priv->folder_tree, path);
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
	return storage->priv->name;
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

static void
impl_async_xfer_folder (EStorage *storage,
			const char *source_path,
			const char *destination_path,
			gboolean remove_source,
			EStorageResultCallback callback,
			void *data)
{
	(* callback) (storage, E_STORAGE_NOTIMPLEMENTED, data);
}

static void
impl_async_open_folder (EStorage *storage,
			const char *path)
{
	;
}

static gboolean
impl_supports_shared_folders (EStorage *storage)
{
	return FALSE;
}

static void
impl_async_discover_shared_folder (EStorage *storage,
				   const char *owner,
				   const char *folder_name,
				   EStorageDiscoveryCallback callback,
				   void *data)
{
	(* callback) (storage, E_STORAGE_NOTIMPLEMENTED, NULL, data);
}

static void
impl_async_remove_shared_folder (EStorage *storage,
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

	class->get_subfolder_paths = impl_get_subfolder_paths;
	class->get_folder          = impl_get_folder;
	class->get_name            = impl_get_name;
	class->async_create_folder = impl_async_create_folder;
	class->async_remove_folder = impl_async_remove_folder;
	class->async_xfer_folder   = impl_async_xfer_folder;
	class->async_open_folder   = impl_async_open_folder;

	class->supports_shared_folders       = impl_supports_shared_folders;
	class->async_discover_shared_folder  = impl_async_discover_shared_folder;
	class->async_remove_shared_folder    = impl_async_remove_shared_folder;

	signals[NEW_FOLDER] =
		gtk_signal_new ("new_folder",
				GTK_RUN_FIRST,
				object_class->type,
				GTK_SIGNAL_OFFSET (EStorageClass, new_folder),
				gtk_marshal_NONE__STRING,
				GTK_TYPE_NONE, 1,
				GTK_TYPE_STRING);
	signals[UPDATED_FOLDER] =
		gtk_signal_new ("updated_folder",
				GTK_RUN_FIRST,
				object_class->type,
				GTK_SIGNAL_OFFSET (EStorageClass, updated_folder),
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
	signals[CLOSE_FOLDER] =
		gtk_signal_new ("close_folder",
				GTK_RUN_FIRST,
				object_class->type,
				GTK_SIGNAL_OFFSET (EStorageClass, close_folder),
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

	priv->folder_tree   = e_folder_tree_new (folder_destroy_notify, NULL);
	priv->pseudofolders = g_hash_table_new (g_str_hash, g_str_equal);
	priv->name          = NULL;

	storage->priv = priv;
}


/* Creation.  */

void
e_storage_construct (EStorage *storage,
		     const char *name,
		     EFolder *root_folder)
{
	EStoragePrivate *priv;

	g_return_if_fail (storage != NULL);
	g_return_if_fail (E_IS_STORAGE (storage));

	priv = storage->priv;

	priv->name               = g_strdup (name);

	e_storage_new_folder (storage, "/", root_folder);

	GTK_OBJECT_UNSET_FLAGS (GTK_OBJECT (storage), GTK_FLOATING);
}

EStorage *
e_storage_new (const char *name,
	       EFolder *root_folder)
{
	EStorage *new;

	new = gtk_type_new (e_storage_get_type ());

	e_storage_construct (new, name, root_folder);

	return new;
}


gboolean
e_storage_path_is_absolute (const char *path)
{
	g_return_val_if_fail (path != NULL, FALSE);

	return *path == E_PATH_SEPARATOR;
}

gboolean
e_storage_path_is_relative (const char *path)
{
	g_return_val_if_fail (path != NULL, FALSE);

	return *path != E_PATH_SEPARATOR;
}


GList *
e_storage_get_subfolder_paths (EStorage *storage,
			       const char *path)
{
	g_return_val_if_fail (storage != NULL, NULL);
	g_return_val_if_fail (E_IS_STORAGE (storage), NULL);
	g_return_val_if_fail (path != NULL, NULL);
	g_return_val_if_fail (g_path_is_absolute (path), NULL);

	return (* ES_CLASS (storage)->get_subfolder_paths) (storage, path);
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
e_storage_async_remove_folder (EStorage              *storage,
			       const char            *path,
			       EStorageResultCallback callback,
			       void                  *data)
{
	g_return_if_fail (storage != NULL);
	g_return_if_fail (E_IS_STORAGE (storage));
	g_return_if_fail (path != NULL);
	g_return_if_fail (g_path_is_absolute (path));
	g_return_if_fail (callback != NULL);

	(* ES_CLASS (storage)->async_remove_folder) (storage, path, callback, data);
}

void
e_storage_async_xfer_folder (EStorage *storage,
			     const char *source_path,
			     const char *destination_path,
			     const gboolean remove_source,
			     EStorageResultCallback callback,
			     void *data)
{
	g_return_if_fail (storage != NULL);
	g_return_if_fail (E_IS_STORAGE (storage));
	g_return_if_fail (source_path != NULL);
	g_return_if_fail (g_path_is_absolute (source_path));
	g_return_if_fail (destination_path != NULL);
	g_return_if_fail (g_path_is_absolute (destination_path));

	if (strcmp (source_path, destination_path) == 0) {
		(* callback) (storage, E_STORAGE_OK, data);
		return;
	}

	if (remove_source) {
		int destination_len;
		int source_len;

		source_len = strlen (source_path);
		destination_len = strlen (destination_path);

		if (source_len < destination_len
		    && destination_path[source_len] == E_PATH_SEPARATOR
		    && strncmp (destination_path, source_path, source_len) == 0) {
			(* callback) (storage, E_STORAGE_CANTMOVETODESCENDANT, data);
			return;
		}
	}

	(* ES_CLASS (storage)->async_xfer_folder) (storage, source_path, destination_path, remove_source, callback, data);
}

void
e_storage_async_open_folder (EStorage *storage,
			     const char *path)
{
	g_return_if_fail (storage != NULL);
	g_return_if_fail (E_IS_STORAGE (storage));
	g_return_if_fail (path != NULL);
	g_return_if_fail (g_path_is_absolute (path));

	if (g_hash_table_lookup (storage->priv->pseudofolders, path) == NULL)
		return;

	(* ES_CLASS (storage)->async_open_folder) (storage, path);
}


/* Shared folders.  */

gboolean
e_storage_supports_shared_folders (EStorage *storage)
{
	g_return_val_if_fail (storage != NULL, FALSE);
	g_return_val_if_fail (E_IS_STORAGE (storage), FALSE);

	return (* ES_CLASS (storage)->supports_shared_folders) (storage);
}

void
e_storage_async_discover_shared_folder (EStorage *storage,
					const char *owner,
					const char *folder_name,
					EStorageDiscoveryCallback callback,
					void *data)
{
	g_return_if_fail (storage != NULL);
	g_return_if_fail (E_IS_STORAGE (storage));
	g_return_if_fail (owner != NULL);
	g_return_if_fail (folder_name != NULL);

	(* ES_CLASS (storage)->async_discover_shared_folder) (storage, owner, folder_name, callback, data);
}

void
e_storage_cancel_discover_shared_folder (EStorage *storage,
					 const char *owner,
					 const char *folder_name)
{
	g_return_if_fail (E_IS_STORAGE (storage));
	g_return_if_fail (owner != NULL);
	g_return_if_fail (folder_name != NULL);
	g_return_if_fail (ES_CLASS (storage)->cancel_discover_shared_folder != NULL);

	(* ES_CLASS (storage)->cancel_discover_shared_folder) (storage, owner, folder_name);
}

void
e_storage_async_remove_shared_folder (EStorage *storage,
				      const char *path,
				      EStorageResultCallback callback,
				      void *data)
{
	g_return_if_fail (storage != NULL);
	g_return_if_fail (E_IS_STORAGE (storage));
	g_return_if_fail (path != NULL);
	g_return_if_fail (g_path_is_absolute (path));

	(* ES_CLASS (storage)->async_remove_shared_folder) (storage, path, callback, data);
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
	case E_STORAGE_NOTEMPTY:
		return _("The folder is not empty");
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
	case E_STORAGE_CANTCHANGESTOCKFOLDER:
		return _("The specified folder cannot be modified or removed");
	case E_STORAGE_CANTMOVETODESCENDANT:
		return _("Cannot make a folder a child of one of its descendants");
	case E_STORAGE_INVALIDNAME:
		return _("Cannot create a folder with that name");
	case E_STORAGE_NOTONLINE:
		return _("This operation cannot be performed in off-line mode");
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
				   void *path_data,
				   void *user_data)
{
	GetPathForPhysicalUriForeachData *foreach_data;
	const char *physical_uri;
	EFolder *e_folder;

	foreach_data = (GetPathForPhysicalUriForeachData *) user_data;
	if (foreach_data->retval != NULL)
		return;

	e_folder = (EFolder *) path_data;
	if (e_folder == NULL)
		return;

	physical_uri = e_folder_get_physical_uri (e_folder);
	if (physical_uri == NULL)
		return;

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
	char *parent_path, *p;
	gpointer stored_path, pseudofolder;

	g_return_val_if_fail (storage != NULL, FALSE);
	g_return_val_if_fail (E_IS_STORAGE (storage), FALSE);
	g_return_val_if_fail (path != NULL, FALSE);
	g_return_val_if_fail (g_path_is_absolute (path), FALSE);
	g_return_val_if_fail (e_folder != NULL, FALSE);
	g_return_val_if_fail (E_IS_FOLDER (e_folder), FALSE);

	priv = storage->priv;

	if (! e_folder_tree_add (priv->folder_tree, path, e_folder))
		return FALSE;

	p = strrchr (path, '/');
	if (p && p != path)
		parent_path = g_strndup (path, p - path);
	else
		parent_path = g_strdup ("/");
	if (g_hash_table_lookup_extended (priv->pseudofolders, parent_path,
					  &stored_path, &pseudofolder) &&
	    pseudofolder != e_folder) {
		g_hash_table_remove (priv->pseudofolders, parent_path);
		g_free (stored_path);		
		e_storage_removed_folder (storage, e_folder_get_physical_uri (pseudofolder));
	}
	g_free (parent_path);

	gtk_signal_connect_while_alive (GTK_OBJECT (e_folder), "changed", folder_changed_cb,
					storage, GTK_OBJECT (storage));

	gtk_signal_emit (GTK_OBJECT (storage), signals[NEW_FOLDER], path);

	folder_changed_cb (e_folder, storage);

	return TRUE;
}

gboolean
e_storage_has_subfolders (EStorage *storage,
			  const char *path,
			  const char *message)
{
	EStoragePrivate *priv;
	GList *subfolders, *f;
	EFolder *pseudofolder;
	char *pseudofolder_path;
	gboolean retval;

	g_return_val_if_fail (storage != NULL, FALSE);
	g_return_val_if_fail (E_IS_STORAGE (storage), FALSE);
	g_return_val_if_fail (path != NULL, FALSE);
	g_return_val_if_fail (g_path_is_absolute (path), FALSE);
	g_return_val_if_fail (message != NULL, FALSE);

	priv = storage->priv;

	gtk_signal_emit (GTK_OBJECT (storage), signals[CLOSE_FOLDER], path);

	if (g_hash_table_lookup (priv->pseudofolders, path))
		return TRUE;

	subfolders = e_folder_tree_get_subfolders (priv->folder_tree, path);
	if (subfolders != NULL) {
		for (f = subfolders; f; f = f->next)
			e_storage_removed_folder (storage, f->data);
		g_list_free (subfolders);
		/* FIXME: close parent */
	}

	pseudofolder = e_folder_new (message, "working", "");
	if (strcmp (path, "/") == 0)
		pseudofolder_path = g_strdup_printf ("/%s", message);
	else
		pseudofolder_path = g_strdup_printf ("%s/%s", path, message);
	e_folder_set_physical_uri (pseudofolder, pseudofolder_path);

	g_hash_table_insert (priv->pseudofolders, g_strdup (path), pseudofolder);

	retval = e_storage_new_folder (storage, pseudofolder_path, pseudofolder);
	g_free (pseudofolder_path);

	return retval;
}

gboolean
e_storage_removed_folder (EStorage *storage,
			  const char *path)
{
	EStoragePrivate *priv;
	EFolder *folder;
	const char *p;

	g_return_val_if_fail (storage != NULL, FALSE);
	g_return_val_if_fail (E_IS_STORAGE (storage), FALSE);
	g_return_val_if_fail (path != NULL, FALSE);
	g_return_val_if_fail (g_path_is_absolute (path), FALSE);

	priv = storage->priv;

	folder = e_folder_tree_get_folder (priv->folder_tree, path);
	if (folder == NULL)
		return FALSE;

	p = strrchr (path, '/');
	if (p != NULL && p != path) {
		EFolder *parent_folder;
		char *parent_path;

		parent_path = g_strndup (path, p - path);
		parent_folder = e_folder_tree_get_folder (priv->folder_tree, parent_path);

		if (e_folder_get_highlighted (folder))
			e_folder_set_child_highlight (parent_folder, FALSE);

		g_free (parent_path);
	}

	gtk_signal_emit (GTK_OBJECT (storage), signals[REMOVED_FOLDER], path);

	e_folder_tree_remove (priv->folder_tree, path);

	return TRUE;
}


E_MAKE_TYPE (e_storage, "EStorage", EStorage, class_init, init, PARENT_TYPE)
