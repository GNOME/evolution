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

#include "e-storage-set.h"


enum {
	NEW_STORAGE,
	REMOVED_STORAGE,
	LAST_SIGNAL
};


#define PARENT_TYPE GTK_TYPE_OBJECT

static GtkObjectClass *parent_class = NULL;
static guint signals[LAST_SIGNAL] = { 0 };

struct _EStorageSetPrivate {
	GList *storages;
};


/* GtkObject methods.  */

static void
destroy (GtkObject *object)
{
	EStorageSet *storage_set;
	EStorageSetPrivate *priv;

	storage_set = E_STORAGE_SET (object);
	priv = storage_set->priv;

	e_free_object_list (priv->storages);

	g_free (priv);

	(* GTK_OBJECT_CLASS (parent_class)->destroy) (object);
}


static void
class_init (EStorageSetClass *klass)
{
	GtkObjectClass *object_class;

	parent_class = gtk_type_class (gtk_object_get_type ());

	object_class = (GtkObjectClass*) klass;

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

	gtk_object_class_add_signals (object_class, signals, LAST_SIGNAL);
}

static void
init (EStorageSet *storage_set)
{
	EStorageSetPrivate *priv;

	priv = g_new (EStorageSetPrivate, 1);

	priv->storages = NULL;

	storage_set->priv = priv;
}


void
e_storage_set_construct (EStorageSet *storage_set)
{
	g_return_if_fail (storage_set != NULL);
	g_return_if_fail (E_IS_STORAGE_SET (storage_set));

	GTK_OBJECT_UNSET_FLAGS (storage_set, GTK_FLOATING);
}

EStorageSet *
e_storage_set_new (void)
{
	EStorageSet *new;

	new = gtk_type_new (e_storage_set_get_type ());

	e_storage_set_construct (new);

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
 * Add @storage to @storage_set.  Notice that this won't ref the @storage, so
 * after the call @storage_set actually owns @storage.
 **/
void
e_storage_set_add_storage (EStorageSet *storage_set,
			   EStorage *storage)
{
	EStorageSetPrivate *priv;

	g_return_if_fail (storage_set != NULL);
	g_return_if_fail (E_IS_STORAGE_SET (storage_set));
	g_return_if_fail (storage != NULL);
	g_return_if_fail (E_IS_STORAGE (storage));

	priv = storage_set->priv;

	priv->storages = g_list_append (priv->storages, storage);

	gtk_signal_emit (GTK_OBJECT (storage_set), signals[NEW_STORAGE], storage);
}

void
e_storage_set_remove_storage (EStorageSet *storage_set,
			      EStorage *storage)
{
	EStorageSetPrivate *priv;

	g_return_if_fail (storage_set != NULL);
	g_return_if_fail (E_IS_STORAGE_SET (storage_set));
	g_return_if_fail (storage != NULL);
	g_return_if_fail (E_IS_STORAGE (storage));

	priv = storage_set->priv;

	priv->storages = g_list_remove (priv->storages, storage);

	gtk_signal_emit (GTK_OBJECT (storage_set), signals[REMOVED_STORAGE], storage);

	gtk_object_unref (GTK_OBJECT (storage));
}

EStorage *
e_storage_set_get_storage (EStorageSet *storage_set,
			   const char *name)
{
	EStorageSetPrivate *priv;
	GList *p;

	g_return_val_if_fail (storage_set != NULL, NULL);
	g_return_val_if_fail (E_IS_STORAGE_SET (storage_set), NULL);
	g_return_val_if_fail (name != NULL, NULL);

	priv = storage_set->priv;

	for (p = priv->storages; p != NULL; p = p->next) {
		const char *storage_name;
		EStorage *storage;

		storage = E_STORAGE (p->data);
		storage_name = e_storage_get_name (storage);
		if (strcmp (storage_name, name) == 0)
			return storage;
	}

	return NULL;
}

EFolder *
e_storage_set_get_folder (EStorageSet *storage_set,
			  const char *path)
{
	EStorage *storage;
	const char *first_separator;
	char *storage_name;

	g_return_val_if_fail (storage_set != NULL, NULL);
	g_return_val_if_fail (E_IS_STORAGE_SET (storage_set), NULL);
	g_return_val_if_fail (path != NULL, NULL);
	g_return_val_if_fail (g_path_is_absolute (path), NULL);

	/* Skip initial separator.  */
	path++;

	first_separator = strchr (path, G_DIR_SEPARATOR);

	if (first_separator == NULL || first_separator == path || first_separator[1] == 0)
		return NULL;

	storage_name = g_strndup (path, first_separator - path);
	storage = e_storage_set_get_storage (storage_set, storage_name);
	g_free (storage_name);

	if (storage == NULL)
		return NULL;

	return e_storage_get_folder (storage, first_separator);
}


E_MAKE_TYPE (e_storage_set, "EStorageSet", EStorageSet, class_init, init, PARENT_TYPE)
