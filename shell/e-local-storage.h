/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* e-local-storage.h
 *
 * Copyright (C) 2000 Helix Code, Inc.
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

#ifndef _E_LOCAL_STORAGE_H_
#define _E_LOCAL_STORAGE_H_

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "e-folder-type-registry.h"
#include "e-storage.h"

#ifdef __cplusplus
extern "C" {
#pragma }
#endif /* __cplusplus */

#define E_TYPE_LOCAL_STORAGE			(e_local_storage_get_type ())
#define E_LOCAL_STORAGE(obj)			(GTK_CHECK_CAST ((obj), E_TYPE_LOCAL_STORAGE, ELocalStorage))
#define E_LOCAL_STORAGE_CLASS(klass)		(GTK_CHECK_CLASS_CAST ((klass), E_TYPE_LOCAL_STORAGE, ELocalStorageClass))
#define E_IS_LOCAL_STORAGE(obj)			(GTK_CHECK_TYPE ((obj), E_TYPE_LOCAL_STORAGE))
#define E_IS_LOCAL_STORAGE_CLASS(klass)		(GTK_CHECK_CLASS_TYPE ((obj), E_TYPE_LOCAL_STORAGE))

typedef struct _ELocalStorage        ELocalStorage;
typedef struct _ELocalStoragePrivate ELocalStoragePrivate;
typedef struct _ELocalStorageClass   ELocalStorageClass;

struct _ELocalStorage {
	EStorage parent;

	ELocalStoragePrivate *priv;
};

struct _ELocalStorageClass {
	EStorageClass parent_class;
};


#define E_LOCAL_STORAGE_NAME "local"


GtkType                       e_local_storage_get_type             (void);

EStorage                     *e_local_storage_open                 (EFolderTypeRegistry *folder_type_registry,
								    const char          *base_path);
const char                   *e_local_storage_get_base_path        (ELocalStorage       *storage);

const Evolution_LocalStorage  e_local_storage_get_corba_interface  (ELocalStorage       *storage);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* __E_LOCAL_STORAGE_H__ */
