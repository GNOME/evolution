/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* e-storage-watcher.c
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

#include <gtk/gtk.h>

#include "e-util/e-util.h"

#include "e-storage-watcher.h"


#define PARENT_TYPE gtk_object_get_type ()
static GtkObjectClass *parent_class = NULL;

struct _EStorageWatcherPrivate {
	EStorage *storage;
	char *path;
};


enum {
	NEW_FOLDER,
	REMOVED_FOLDER,
	LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = { 0 };


/* GtkObject methods.  */

static void
destroy (GtkObject *object)
{
	EStorageWatcher *storage_watcher;
	EStorageWatcherPrivate *priv;

	storage_watcher = E_STORAGE_WATCHER (object);
	priv = storage_watcher->priv;

	g_free (priv->path);
	g_free (priv);

	(* GTK_OBJECT_CLASS (parent_class)->destroy) (object);
}


/* Initialization.  */

static void
class_init (EStorageWatcherClass *klass)
{
	GtkObjectClass *object_class;

	object_class = (GtkObjectClass*) klass;
	object_class->destroy = destroy;

	parent_class = gtk_type_class (gtk_object_get_type ());

	signals[NEW_FOLDER] =
		gtk_signal_new ("new_folder",
				GTK_RUN_FIRST,
				object_class->type,
				GTK_SIGNAL_OFFSET (EStorageWatcherClass, new_folder),
				gtk_marshal_NONE__POINTER_STRING_STRING,
				GTK_TYPE_NONE, 2,
				GTK_TYPE_STRING,
				GTK_TYPE_STRING);
	signals[REMOVED_FOLDER] =
		gtk_signal_new ("removed_folder",
				GTK_RUN_FIRST,
				object_class->type,
				GTK_SIGNAL_OFFSET (EStorageWatcherClass, removed_folder),
				gtk_marshal_NONE__POINTER_STRING_STRING,
				GTK_TYPE_NONE, 2,
				GTK_TYPE_STRING,
				GTK_TYPE_STRING);

	gtk_object_class_add_signals (object_class, signals, LAST_SIGNAL);
}

static void
init (EStorageWatcher *storage_watcher)
{
	EStorageWatcherPrivate *priv;

	priv = g_new (EStorageWatcherPrivate, 1);
	priv->storage = NULL;
	priv->path = NULL;

	storage_watcher->priv = priv;
}


/* Initialization.  */

void
e_storage_watcher_construct (EStorageWatcher *watcher,
			     EStorage *storage,
			     const char *path)
{
	EStorageWatcherPrivate *priv;

	g_return_if_fail (watcher != NULL);
	g_return_if_fail (E_IS_STORAGE_WATCHER (watcher));
	g_return_if_fail (path != NULL);

	GTK_OBJECT_UNSET_FLAGS (GTK_OBJECT (watcher), GTK_FLOATING);

	priv = watcher->priv;

	priv->storage = storage;
	priv->path = g_strdup (path);
}

EStorageWatcher *
e_storage_watcher_new (EStorage *storage,
		       const char *path)
{
	EStorageWatcher *watcher;

	g_return_val_if_fail (path != NULL, NULL);

	watcher = gtk_type_new (e_storage_watcher_get_type ());

	e_storage_watcher_construct (watcher, storage, path);

	return watcher;
}


const char *
e_storage_watcher_get_path (EStorageWatcher *storage_watcher)
{
	g_return_val_if_fail (storage_watcher != NULL, NULL);
	g_return_val_if_fail (E_IS_STORAGE_WATCHER (storage_watcher), NULL);

	return storage_watcher->priv->path;
}


void
e_storage_watcher_emit_new_folder (EStorageWatcher *storage_watcher,
				   const char *name)
{
	g_return_if_fail (storage_watcher != NULL);
	g_return_if_fail (E_IS_STORAGE_WATCHER (storage_watcher));
	g_return_if_fail (name != NULL);

	gtk_signal_emit (GTK_OBJECT (storage_watcher), signals[NEW_FOLDER], name);
}

void
e_storage_watcher_emit_removed_folder (EStorageWatcher *storage_watcher,
				       const char *name)
{
	g_return_if_fail (storage_watcher != NULL);
	g_return_if_fail (E_IS_STORAGE_WATCHER (storage_watcher));
	g_return_if_fail (name != NULL);

	gtk_signal_emit (GTK_OBJECT (storage_watcher), signals[REMOVED_FOLDER], name);
}


E_MAKE_TYPE (e_storage_watcher, "EStorageWatcher", EStorageWatcher, class_init, init, PARENT_TYPE)
