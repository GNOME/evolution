/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* e-storage-set.c
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
#include <gtk/gtktypeutils.h>

#include <string.h>

#include "e-util/e-util.h"

#include "e-storage-set-view.h"
#include "e-storage-set.h"


#define PARENT_TYPE GTK_TYPE_OBJECT

static GtkObjectClass *parent_class = NULL;

/* This is just to make GHashTable happy.  */
struct _NamedStorage {
	char *name;
	EStorage *storage;
};
typedef struct _NamedStorage NamedStorage;

struct _EStorageSetPrivate {
	GList *storages;	/* EStorage */
	GHashTable *name_to_named_storage;

	EFolderTypeRegistry *folder_type_registry;
};

enum {
	NEW_STORAGE,
	REMOVED_STORAGE,
	NEW_FOLDER,
	REMOVED_FOLDER,
	LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = { 0 };


static NamedStorage *
named_storage_new (EStorage *storage)
{
	NamedStorage *new;

	new = g_new (NamedStorage, 1);
	new->name    = g_strdup (e_storage_get_name (storage));
	new->storage = storage;

	return new;
}

static void
named_storage_destroy (NamedStorage *named_storage)
{
	g_free (named_storage->name);
	g_free (named_storage);
}

static gboolean
name_to_named_storage_foreach_destroy (void *key,
				       void *value,
				       void *user_data)
{
	NamedStorage *named_storage;

	named_storage = (NamedStorage *) value;
	named_storage_destroy (named_storage);

	return TRUE;
}


/* Handling for signals coming from the EStorages.  */

static char *
make_full_path (EStorage *storage,
		const char *path)
{
	const char *storage_name;
	char *full_path;

	storage_name = e_storage_get_name (storage);

	if (! g_path_is_absolute (path))
		full_path = g_strconcat (G_DIR_SEPARATOR_S, storage_name,
					 G_DIR_SEPARATOR_S, path, NULL);
	else
		full_path = g_strconcat (G_DIR_SEPARATOR_S, storage_name,
					 path, NULL);

	return full_path;
}

static void
storage_new_folder_cb (EStorage *storage,
		       const char *path,
		       void *data)
{
	EStorageSet *storage_set;
	char *full_path;

	storage_set = E_STORAGE_SET (data);

	full_path = make_full_path (storage, path);
	gtk_signal_emit (GTK_OBJECT (storage_set), signals[NEW_FOLDER], full_path);
	g_free (full_path);
}

static void
storage_removed_folder_cb (EStorage *storage,
			   const char *path,
			   void *data)
{
	EStorageSet *storage_set;
	char *full_path;

	storage_set = E_STORAGE_SET (data);

	full_path = make_full_path (storage, path);
	g_print ("EStorageSet: Removed folder -- %s\n", full_path);
	gtk_signal_emit (GTK_OBJECT (storage_set), signals[REMOVED_FOLDER], full_path);
	g_free (full_path);
}


static EStorage *
get_storage_for_path (EStorageSet *storage_set,
		      const char *path,
		      const char **subpath_return)
{
	EStorage *storage;
	char *storage_name;
	const char *first_separator;

	g_return_val_if_fail (g_path_is_absolute (path), NULL);

	/* Skip initial separator.  */
	path++;

	first_separator = strchr (path, G_DIR_SEPARATOR);

	if (first_separator == NULL || first_separator == path || first_separator[1] == 0) {
		*subpath_return = NULL;
		return NULL;
	}

	storage_name = g_strndup (path, first_separator - path);
	storage = e_storage_set_get_storage (storage_set, storage_name);
	g_free (storage_name);

	*subpath_return = first_separator;

	return storage;
}


/* GtkObject methods.  */

static void
destroy (GtkObject *object)
{
	EStorageSet *storage_set;
	EStorageSetPrivate *priv;

	storage_set = E_STORAGE_SET (object);
	priv = storage_set->priv;

	e_free_object_list (priv->storages);

	gtk_object_unref (GTK_OBJECT (priv->folder_type_registry));

	g_hash_table_foreach (priv->name_to_named_storage, (GHFunc) named_storage_destroy, NULL);
	g_hash_table_destroy (priv->name_to_named_storage);

	g_free (priv);

	(* GTK_OBJECT_CLASS (parent_class)->destroy) (object);
}


static void
class_init (EStorageSetClass *klass)
{
	GtkObjectClass *object_class;

	parent_class = gtk_type_class (gtk_object_get_type ());
	object_class = GTK_OBJECT_CLASS (klass);

	object_class->destroy = destroy;

	signals[NEW_STORAGE] = 
		gtk_signal_new ("new_storage",
				GTK_RUN_FIRST,
				object_class->type,
				GTK_SIGNAL_OFFSET (EStorageSetClass, new_storage),
				gtk_marshal_NONE__POINTER,
				GTK_TYPE_NONE, 1,
				GTK_TYPE_POINTER);
	signals[REMOVED_STORAGE] = 
		gtk_signal_new ("removed_storage",
				GTK_RUN_FIRST,
				object_class->type,
				GTK_SIGNAL_OFFSET (EStorageSetClass, removed_storage),
				gtk_marshal_NONE__POINTER,
				GTK_TYPE_NONE, 1,
				GTK_TYPE_POINTER);
	signals[NEW_FOLDER] = 
		gtk_signal_new ("new_folder",
				GTK_RUN_FIRST,
				object_class->type,
				GTK_SIGNAL_OFFSET (EStorageSetClass, new_folder),
				gtk_marshal_NONE__STRING,
				GTK_TYPE_NONE, 1,
				GTK_TYPE_STRING);
	signals[REMOVED_FOLDER] = 
		gtk_signal_new ("removed_folder",
				GTK_RUN_FIRST,
				object_class->type,
				GTK_SIGNAL_OFFSET (EStorageSetClass, removed_folder),
				gtk_marshal_NONE__STRING,
				GTK_TYPE_NONE, 1,
				GTK_TYPE_STRING);

	gtk_object_class_add_signals (object_class, signals, LAST_SIGNAL);
}

static void
init (EStorageSet *storage_set)
{
	EStorageSetPrivate *priv;

	g_return_if_fail (E_IS_STORAGE_SET (storage_set));

	priv = g_new (EStorageSetPrivate, 1);
	priv->storages              = NULL;
	priv->name_to_named_storage = g_hash_table_new (g_str_hash, g_str_equal);
	priv->folder_type_registry  = NULL;

	storage_set->priv = priv;
}


void
e_storage_set_construct (EStorageSet *storage_set,
			 EFolderTypeRegistry *folder_type_registry)
{
	g_return_if_fail (storage_set != NULL);
	g_return_if_fail (E_IS_STORAGE_SET (storage_set));

	GTK_OBJECT_UNSET_FLAGS (storage_set, GTK_FLOATING);

	gtk_object_ref (GTK_OBJECT (folder_type_registry));
	storage_set->priv->folder_type_registry = folder_type_registry;
}

EStorageSet *
e_storage_set_new (EFolderTypeRegistry *folder_type_registry)
{
	EStorageSet *new;

	new = gtk_type_new (e_storage_set_get_type ());

	e_storage_set_construct (new, folder_type_registry);

	return new;
}


GList *
e_storage_set_get_storage_list (EStorageSet *storage_set)
{
	EStorageSetPrivate *priv;
	GList *list;
	GList *p;

	g_return_val_if_fail (storage_set != NULL, NULL);
	g_return_val_if_fail (E_IS_STORAGE_SET (storage_set), NULL);

	priv = storage_set->priv;

	list = NULL;
	for (p = priv->storages; p != NULL; p = p->next) {
		gtk_object_ref (GTK_OBJECT (p->data));
		list = g_list_prepend (list, p->data);
	}

	return g_list_reverse (list); /* Lame.  */
}

/**
 * e_storage_set_add_storage:
 * @storage_set: 
 * @storage: 
 * 
 * Add @storage to @storage_set.  Notice that will ref the storage.
 **/
gboolean
e_storage_set_add_storage (EStorageSet *storage_set,
			   EStorage *storage)
{
	EStorageSetPrivate *priv;
	const char *storage_name;
	NamedStorage *named_storage;

	g_return_val_if_fail (storage_set != NULL, FALSE);
	g_return_val_if_fail (E_IS_STORAGE_SET (storage_set), FALSE);
	g_return_val_if_fail (storage != NULL, FALSE);
	g_return_val_if_fail (E_IS_STORAGE (storage), FALSE);

	priv = storage_set->priv;

	storage_name = e_storage_get_name (storage);
	if (g_hash_table_lookup (priv->name_to_named_storage, storage_name) != NULL)
		return FALSE;

	gtk_object_ref (GTK_OBJECT (storage));

	gtk_signal_connect (GTK_OBJECT (storage), "new_folder",
			    GTK_SIGNAL_FUNC (storage_new_folder_cb), storage_set);
	gtk_signal_connect (GTK_OBJECT (storage), "removed_folder",
			    GTK_SIGNAL_FUNC (storage_removed_folder_cb), storage_set);

	priv->storages = g_list_append (priv->storages, storage);

	named_storage = named_storage_new (storage);
	g_hash_table_insert (priv->name_to_named_storage, named_storage->name, named_storage);

	gtk_signal_emit (GTK_OBJECT (storage_set), signals[NEW_STORAGE], storage);

	return TRUE;
}

gboolean
e_storage_set_remove_storage (EStorageSet *storage_set,
			      EStorage *storage)
{
	EStorageSetPrivate *priv;
	NamedStorage *named_storage;

	g_return_val_if_fail (storage_set != NULL, FALSE);
	g_return_val_if_fail (E_IS_STORAGE_SET (storage_set), FALSE);
	g_return_val_if_fail (storage != NULL, FALSE);
	g_return_val_if_fail (E_IS_STORAGE (storage), FALSE);

	priv = storage_set->priv;

	named_storage = g_hash_table_lookup (priv->name_to_named_storage,
					     e_storage_get_name (storage));
	if (named_storage == NULL)
		return FALSE;

	g_hash_table_remove (priv->name_to_named_storage, named_storage->name);
	named_storage_destroy (named_storage);

	priv->storages = g_list_remove (priv->storages, storage);

	gtk_signal_emit (GTK_OBJECT (storage_set), signals[REMOVED_STORAGE], storage);
	gtk_object_unref (GTK_OBJECT (storage));

	return TRUE;
}

void
e_storage_set_remove_all_storages (EStorageSet *storage_set)
{
	EStorageSetPrivate *priv;
	GList *p;

	g_return_if_fail (storage_set != NULL);
	g_return_if_fail (E_IS_STORAGE_SET (storage_set));

	priv = storage_set->priv;

	for (p = priv->storages; p != NULL; p = p->next) {
		EStorage *storage;

		storage = E_STORAGE (p->data);

		gtk_signal_emit (GTK_OBJECT (storage_set), signals[REMOVED_STORAGE], storage);
		gtk_object_unref (GTK_OBJECT (storage));
	}

	g_hash_table_foreach_remove (priv->name_to_named_storage,
				     name_to_named_storage_foreach_destroy,
				     NULL);

	g_list_free (priv->storages);
	priv->storages = NULL;
}


EStorage *
e_storage_set_get_storage (EStorageSet *storage_set,
			   const char *name)
{
	EStorageSetPrivate *priv;
	NamedStorage *named_storage;

	g_return_val_if_fail (storage_set != NULL, NULL);
	g_return_val_if_fail (E_IS_STORAGE_SET (storage_set), NULL);
	g_return_val_if_fail (name != NULL, NULL);

	priv = storage_set->priv;

	named_storage = g_hash_table_lookup (priv->name_to_named_storage, name);
	if (named_storage == NULL)
		return NULL;
	else
		return named_storage->storage;
}

EFolder *
e_storage_set_get_folder (EStorageSet *storage_set,
			  const char *path)
{
	EStorage *storage;
	const char *subpath;

	g_return_val_if_fail (storage_set != NULL, NULL);
	g_return_val_if_fail (E_IS_STORAGE_SET (storage_set), NULL);
	g_return_val_if_fail (path != NULL, NULL);
	g_return_val_if_fail (g_path_is_absolute (path), NULL);

	storage = get_storage_for_path (storage_set, path, &subpath);

	return e_storage_get_folder (storage, subpath);
}


GtkWidget *
e_storage_set_new_view (EStorageSet *storage_set)
{
	GtkWidget *storage_set_view;

	g_return_val_if_fail (storage_set != NULL, NULL);
	g_return_val_if_fail (E_IS_STORAGE_SET (storage_set), NULL);

	storage_set_view = e_storage_set_view_new (storage_set);

	return storage_set_view;
}


void
e_storage_set_async_create_folder  (EStorageSet            *storage_set,
				    const char             *path,
				    const char             *type,
				    const char             *description,
				    EStorageResultCallback  callback,
				    void                   *data)
{
	EStorage *storage;
	const char *subpath;

	g_return_if_fail (storage_set != NULL);
	g_return_if_fail (E_IS_STORAGE_SET (storage_set));
	g_return_if_fail (path != NULL);
	g_return_if_fail (g_path_is_absolute (path));
	g_return_if_fail (type != NULL);
	g_return_if_fail (callback != NULL);

	storage = get_storage_for_path (storage_set, path, &subpath);

	e_storage_async_create_folder (storage, subpath, type, description, callback, data);
}

void
e_storage_set_async_remove_folder  (EStorageSet            *storage_set,
				    const char             *path,
				    EStorageResultCallback  callback,
				    void                   *data)
{
	EStorage *storage;
	const char *subpath;

	g_return_if_fail (storage_set != NULL);
	g_return_if_fail (E_IS_STORAGE_SET (storage_set));
	g_return_if_fail (path != NULL);
	g_return_if_fail (g_path_is_absolute (path));
	g_return_if_fail (callback != NULL);

	storage = get_storage_for_path (storage_set, path, &subpath);

	e_storage_async_remove_folder (storage, path, callback, data);
}


EFolderTypeRegistry *
e_storage_set_get_folder_type_registry (EStorageSet *storage_set)
{
	g_return_val_if_fail (storage_set != NULL, NULL);
	g_return_val_if_fail (E_IS_STORAGE_SET (storage_set), NULL);

	return storage_set->priv->folder_type_registry;
}


E_MAKE_TYPE (e_storage_set, "EStorageSet", EStorageSet, class_init, init, PARENT_TYPE)
