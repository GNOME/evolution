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

struct _WatcherList {
	char *path;
	GList *watchers;
};
typedef struct _WatcherList WatcherList;

/* This describes a folder and its children.  */
struct _Folder {
	struct _Folder *parent;
	EFolder *e_folder;
	GList *subfolders;
};
typedef struct _Folder Folder;

struct _EStoragePrivate {
	GHashTable *path_to_watcher_list;
	GHashTable *watcher_to_watcher_list;

	/* Every element here is a list of subfolders, hashed to the path of the parent.  */
	GHashTable *path_to_folder;
};


static Folder *
folder_new (EFolder *e_folder)
{
	Folder *folder;

	folder = g_new (Folder, 1);
	folder->parent = NULL;
	folder->e_folder = e_folder;
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
	GList *p;

	if (folder->parent != NULL)
		folder_remove_subfolder (folder->parent, folder);

	gtk_object_unref (GTK_OBJECT (folder->e_folder));

	for (p = folder->subfolders; p != NULL; p = p->next)
		folder_destroy (p->data);

	g_free (folder);
}


/* Watcher management.  */

static void
watcher_destroyed_cb (GtkObject *object,
		      gpointer data)
{
	EStorageWatcher *watcher;
	EStorage *storage;
	EStoragePrivate *priv;
	WatcherList *list;

	watcher = E_STORAGE_WATCHER (object);
	storage = E_STORAGE (data);
	priv = storage->priv;

	list = g_hash_table_lookup (priv->watcher_to_watcher_list, watcher);
	g_return_if_fail (list != NULL);

	list->watchers = g_list_remove (list->watchers, watcher);
}

static void
free_watcher_list (EStorage *storage,
		   WatcherList *watcher_list)
{
	GtkObject *watcher_object;
	GList *p;

	for (p = watcher_list->watchers; p != NULL; p = p->next) {
		watcher_object = GTK_OBJECT (p->data);
		gtk_signal_disconnect_by_func (watcher_object, watcher_destroyed_cb, storage);

		gtk_object_destroy (watcher_object); /* Make sure it does not live when we are dead.  */
		gtk_object_unref (watcher_object);
	}

	g_free (watcher_list->path);

	g_free (watcher_list);
}

static void
hash_foreach_free_watcher_list (gpointer key,
				gpointer value,
				gpointer data)
{
	WatcherList *watcher_list;
	EStorage *storage;

	storage = E_STORAGE (data);
	watcher_list = (WatcherList *) value;

	free_watcher_list (storage, watcher_list);
}

static void
free_private (EStorage *storage)
{
	EStoragePrivate *priv;

	priv = storage->priv;

	g_hash_table_foreach (priv->path_to_watcher_list, hash_foreach_free_watcher_list, storage);
	g_hash_table_destroy (priv->path_to_watcher_list);

	g_hash_table_destroy (priv->watcher_to_watcher_list);

	g_free (priv);
}


/* EStorage methods.  */

static GList *
list_folders (EStorage *storage,
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

static EStorageWatcher *
get_watcher_for_path (EStorage *storage,
		      const char *path)
{
	EStoragePrivate *priv;
	EStorageWatcher *watcher;
	WatcherList *watcher_list;

	priv = storage->priv;

	watcher = e_storage_watcher_new (storage, path);

	watcher_list = g_hash_table_lookup (priv->path_to_watcher_list, path);
	if (watcher_list == NULL) {
		watcher_list = g_new (WatcherList, 1);
		watcher_list->path = g_strdup (path);
		watcher_list->watchers = NULL;

		g_hash_table_insert (priv->path_to_watcher_list, watcher_list->path, watcher_list);
	}

	g_hash_table_insert (priv->watcher_to_watcher_list, watcher, watcher_list);

	watcher_list->watchers = g_list_prepend (watcher_list->watchers, watcher);

	gtk_signal_connect (GTK_OBJECT (watcher), "destroy",
			    GTK_SIGNAL_FUNC (watcher_destroyed_cb), storage);

	return watcher;
}

static EFolder *
get_folder (EStorage *storage,
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
get_name (EStorage *storage)
{
	return "(No name)";
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


/* Initialization.  */

static void
class_init (EStorageClass *class)
{
	GtkObjectClass *object_class;

	object_class = GTK_OBJECT_CLASS (class);
	parent_class = gtk_type_class (gtk_object_get_type ());

	object_class->destroy = destroy;

	class->list_folders         = list_folders;
	class->get_watcher_for_path = get_watcher_for_path;
	class->get_folder           = get_folder;
	class->get_name             = get_name;
}

static void
init (EStorage *storage)
{
	EStoragePrivate *priv;

	priv = g_new (EStoragePrivate, 1);

	priv->path_to_watcher_list    = g_hash_table_new (g_str_hash, g_str_equal);
	priv->watcher_to_watcher_list = g_hash_table_new (g_direct_hash, g_direct_equal);
	priv->path_to_folder          = g_hash_table_new (g_str_hash, g_str_equal);

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

	root_folder = folder_new (NULL);
	g_hash_table_insert (storage->priv->path_to_folder, G_DIR_SEPARATOR_S, root_folder);
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

EStorageWatcher *
e_storage_get_watcher_for_path  (EStorage *storage, const char *path)
{
	g_return_val_if_fail (storage != NULL, NULL);
	g_return_val_if_fail (E_IS_STORAGE (storage), NULL);
	g_return_val_if_fail (path != NULL, NULL);
	g_return_val_if_fail (e_storage_path_is_absolute (path), NULL);

	return (* ES_CLASS (storage)->get_watcher_for_path) (storage, path);
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

	folder = folder_new (e_folder);
	folder_add_subfolder (parent_folder, folder);

	g_hash_table_insert (priv->path_to_folder, full_path, folder);

	return TRUE;
}

gboolean
e_storage_remove_folder (EStorage *storage,
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

	folder_destroy (folder);

	return TRUE;
}


E_MAKE_TYPE (e_storage, "EStorage", EStorage, class_init, init, PARENT_TYPE)
