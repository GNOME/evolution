/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* e-storage-set.h
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

#ifndef _E_STORAGE_SET_H_
#define _E_STORAGE_SET_H_

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "e-folder-type-repository.h"

#include "e-storage.h"

#ifdef __cplusplus
extern "C" {
#pragma }
#endif /* __cplusplus */

#define E_TYPE_STORAGE_SET			(e_storage_set_get_type ())
#define E_STORAGE_SET(obj)			(GTK_CHECK_CAST ((obj), E_TYPE_STORAGE_SET, EStorageSet))
#define E_STORAGE_SET_CLASS(klass)		(GTK_CHECK_CLASS_CAST ((klass), E_TYPE_STORAGE_SET, EStorageSetClass))
#define E_IS_STORAGE_SET(obj)			(GTK_CHECK_TYPE ((obj), E_TYPE_STORAGE_SET))
#define E_IS_STORAGE_SET_CLASS(klass)		(GTK_CHECK_CLASS_TYPE ((obj), E_TYPE_STORAGE_SET))


typedef struct _EStorageSet        EStorageSet;
typedef struct _EStorageSetPrivate EStorageSetPrivate;
typedef struct _EStorageSetClass   EStorageSetClass;

struct _EStorageSet {
	GtkObject parent;

	EStorageSetPrivate *priv;
};

struct _EStorageSetClass {
	GtkObjectClass parent_class;

	/* Virtual methods.  */

	void (* add_storage)     (EStorageSet *storage_set, EStorage *storage);
	void (* remove_storage)  (EStorageSet *storage_set, EStorage *storage);

	/* Signals.  */

	void (* new_storage)      (EStorageSet *storage_set, EStorage *storage);
	void (* removed_storage)  (EStorageSet *storage_set, EStorage *storage);
};


GtkType      e_storage_set_get_type          (void);
void         e_storage_set_construct         (EStorageSet           *storage_set,
					      EFolderTypeRepository *folder_type_repository);
EStorageSet *e_storage_set_new               (EFolderTypeRepository *folder_type_repository);

GList       *e_storage_set_get_storage_list  (EStorageSet           *storage_set);
EStorage    *e_storage_set_get_storage       (EStorageSet           *storage_set,
					      const char            *name);
void         e_storage_set_add_storage       (EStorageSet           *storage_set,
					      EStorage              *storage);
void         e_storage_set_remove_storage    (EStorageSet           *storage_set,
					      EStorage              *storage);

EStorage    *e_storage_set_get_storage       (EStorageSet           *storage_set,
					      const char            *storage_name);
EFolder     *e_storage_set_get_folder        (EStorageSet           *storage_set,
					      const char            *path);

GtkWidget   *e_storage_set_new_view          (EStorageSet           *storage_set);

EFolderTypeRepository *e_storage_set_get_folder_type_repository (EStorageSet *storage_set);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* _E_STORAGE_SET_H_ */
