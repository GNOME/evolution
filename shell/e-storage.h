/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* e-storage.h
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

#ifndef _E_STORAGE_H_
#define _E_STORAGE_H_

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <gtk/gtkobject.h>

#ifdef __cplusplus
extern "C" {
#pragma }
#endif /* __cplusplus */

#define E_TYPE_STORAGE			(e_storage_get_type ())
#define E_STORAGE(obj)			(GTK_CHECK_CAST ((obj), E_TYPE_STORAGE, EStorage))
#define E_STORAGE_CLASS(klass)		(GTK_CHECK_CLASS_CAST ((klass), E_TYPE_STORAGE, EStorageClass))
#define E_IS_STORAGE(obj)		(GTK_CHECK_TYPE ((obj), E_TYPE_STORAGE))
#define E_IS_STORAGE_CLASS(klass)	(GTK_CHECK_CLASS_TYPE ((obj), E_TYPE_STORAGE))


typedef struct _EStorage        EStorage;
typedef struct _EStoragePrivate EStoragePrivate;
typedef struct _EStorageClass   EStorageClass;

#include "e-folder.h"

struct _EStorage {
	GtkObject parent;

	EStoragePrivate *priv;
};

struct _EStorageClass {
	GtkObjectClass parent_class;

	/* Signals.  */
	void * (* new_folder)     (EStorage *storage, const char *path);
	void * (* removed_folder) (EStorage *storage, const char *path);

	/* Virtual methods.  */
	GList      * (* list_folders) (EStorage *storage, const char *path);
	EFolder    * (* get_folder)   (EStorage *storage, const char *path);
	const char * (* get_name)     (EStorage *storage);
};


GtkType   e_storage_get_type    (void);
void      e_storage_construct   (EStorage   *storage);
EStorage *e_storage_new         (void);

gboolean  e_storage_path_is_relative  (const char *path);
gboolean  e_storage_path_is_absolute  (const char *path);

GList           *e_storage_list_folders          (EStorage   *storage, const char *path);
EFolder         *e_storage_get_folder            (EStorage   *storage, const char *path);

const char *e_storage_get_name  (EStorage *storage);

/* Protected.  C++ anyone?  */
gboolean  e_storage_new_folder     (EStorage *storage, const char *path, EFolder *folder);
gboolean  e_storage_remove_folder  (EStorage *storage, const char *path);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* _E_STORAGE_H_ */
