/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* e-storage-watcher.h
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

#ifndef _E_STORAGE_WATCHER_H_
#define _E_STORAGE_WATCHER_H_

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <gtk/gtkobject.h>

#ifdef __cplusplus
extern "C" {
#pragma }
#endif /* __cplusplus */

#define E_TYPE_STORAGE_WATCHER			(e_storage_watcher_get_type ())
#define E_STORAGE_WATCHER(obj)			(GTK_CHECK_CAST ((obj), E_TYPE_STORAGE_WATCHER, EStorageWatcher))
#define E_STORAGE_WATCHER_CLASS(klass)		(GTK_CHECK_CLASS_CAST ((klass), E_TYPE_STORAGE_WATCHER, EStorageWatcherClass))
#define E_IS_STORAGE_WATCHER(obj)		(GTK_CHECK_TYPE ((obj), E_TYPE_STORAGE_WATCHER))
#define E_IS_STORAGE_WATCHER_CLASS(klass)	(GTK_CHECK_CLASS_TYPE ((obj), E_TYPE_STORAGE_WATCHER))


typedef struct _EStorageWatcher        EStorageWatcher;
typedef struct _EStorageWatcherPrivate EStorageWatcherPrivate;
typedef struct _EStorageWatcherClass   EStorageWatcherClass;

#include "e-storage.h"

struct _EStorageWatcher {
	GtkObject parent;

	EStorageWatcherPrivate *priv;
};

struct _EStorageWatcherClass
{
	GtkObjectClass parent_class;

	/* Signals.  */

	void (* new_folder)     (EStorageWatcher *storage_watcher,
				 EStorage        *storage,
				 const char      *path,
				 const char      *name);

	void (* removed_folder)	(EStorageWatcher *storage_watcher,
				 EStorage        *storage,
				 const char      *path,
				 const char      *name);
};


GtkType          e_storage_watcher_get_type   (void);
void             e_storage_watcher_construct  (EStorageWatcher *watcher,
					       EStorage        *storage,
					       const char      *path);
EStorageWatcher *e_storage_watcher_new        (EStorage        *storage,
					       const char      *path);

const char *e_storage_watcher_get_path  (EStorageWatcher *storage_watcher);

void        e_storage_watcher_emit_new_folder      (EStorageWatcher *storage_watcher,
						    const char      *name);
void        e_storage_watcher_emit_removed_folder  (EStorageWatcher *storage_watcher,
						    const char      *name);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* __E_STORAGE_WATCHER_H__ */
