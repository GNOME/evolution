/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* evolution-storage.h
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

#ifndef __EVOLUTION_STORAGE_H__
#define __EVOLUTION_STORAGE_H__

#include <bonobo/bonobo-object.h>

#include "Evolution.h"

#ifdef __cplusplus
extern "C" {
#pragma }
#endif /* __cplusplus */

#define EVOLUTION_TYPE_STORAGE            (evolution_storage_get_type ())
#define EVOLUTION_STORAGE(obj)            (GTK_CHECK_CAST ((obj), EVOLUTION_TYPE_STORAGE, EvolutionStorage))
#define EVOLUTION_STORAGE_CLASS(klass)    (GTK_CHECK_CLASS_CAST ((klass), EVOLUTION_TYPE_STORAGE, EvolutionStorageClass))
#define EVOLUTION_IS_STORAGE(obj)         (GTK_CHECK_TYPE ((obj), EVOLUTION_TYPE_STORAGE))
#define EVOLUTION_IS_STORAGE_CLASS(klass) (GTK_CHECK_CLASS_TYPE ((obj), EVOLUTION_TYPE_STORAGE))


typedef struct _EvolutionStorage        EvolutionStorage;
typedef struct _EvolutionStoragePrivate EvolutionStoragePrivate;
typedef struct _EvolutionStorageClass   EvolutionStorageClass;

enum _EvolutionStorageResult {
	EVOLUTION_STORAGE_OK,

	/* Generic errors */
	EVOLUTION_STORAGE_ERROR_GENERIC,
	EVOLUTION_STORAGE_ERROR_CORBA,
	EVOLUTION_STORAGE_ERROR_INVALIDPARAMETER,

	/* Registration errors */
	EVOLUTION_STORAGE_ERROR_ALREADYREGISTERED,
	EVOLUTION_STORAGE_ERROR_NOTREGISTERED,
	EVOLUTION_STORAGE_ERROR_NOREGISTRY,
	EVOLUTION_STORAGE_ERROR_EXISTS,
	EVOLUTION_STORAGE_ERROR_NOTFOUND,

	/* Folder creation/deletion errors */
	EVOLUTION_STORAGE_ERROR_UNSUPPORTED_OPERATION,
	EVOLUTION_STORAGE_ERROR_UNSUPPORTED_TYPE,
	EVOLUTION_STORAGE_ERROR_INVALID_URI,
	EVOLUTION_STORAGE_ERROR_ALREADY_EXISTS,
	EVOLUTION_STORAGE_ERROR_DOES_NOT_EXIST,
	EVOLUTION_STORAGE_ERROR_PERMISSION_DENIED,
	EVOLUTION_STORAGE_ERROR_NO_SPACE,
	EVOLUTION_STORAGE_ERROR_NOT_EMPTY
};
typedef enum _EvolutionStorageResult EvolutionStorageResult;

struct _EvolutionStorage {
	BonoboObject parent;

	EvolutionStoragePrivate *priv;
};

struct _EvolutionStorageClass {
	BonoboObjectClass parent_class;

	/* signals */
	int (*create_folder) (EvolutionStorage *storage,
			      const char *path,
			      const char *type,
			      const char *description,
			      const char *parent_physical_uri);

	int (*remove_folder) (EvolutionStorage *storage,
			      const char *path,
			      const char *physical_uri);
};


POA_GNOME_Evolution_Storage__epv *evolution_storage_get_epv            (void);

GtkType           evolution_storage_get_type   (void);
void              evolution_storage_construct  (EvolutionStorage        *storage,
						GNOME_Evolution_Storage  corba_object,
						const char              *name,
						const char              *toplevel_node_uri,
						const char              *toplevel_node_type);
EvolutionStorage *evolution_storage_new        (const char              *name,
						const char              *toplevel_node_uri,
						const char              *toplevel_node_type);

EvolutionStorageResult  evolution_storage_register             (EvolutionStorage                *storage,
								GNOME_Evolution_StorageRegistry  corba_registry);
EvolutionStorageResult  evolution_storage_register_on_shell    (EvolutionStorage                *evolution_storage,
								GNOME_Evolution_Shell            corba_shell);
EvolutionStorageResult  evolution_storage_new_folder           (EvolutionStorage                *evolution_storage,
								const char                      *path,
								const char                      *display_name,
								const char                      *type,
								const char                      *physical_uri,
								const char                      *description,
								gboolean                         highlighted);
EvolutionStorageResult  evolution_storage_update_folder        (EvolutionStorage                *evolution_storage,
								const char                      *path,
								const char                      *display_name,
								gboolean                         highlighted);
EvolutionStorageResult  evolution_storage_update_folder_by_uri (EvolutionStorage                *evolution_storage,
								const char                      *physical_uri,
								const char                      *display_name,
								gboolean                         highlighted);
EvolutionStorageResult  evolution_storage_removed_folder       (EvolutionStorage                *evolution_storage,
								const char                      *path);
gboolean                evolution_storage_folder_exists        (EvolutionStorage                *evolution_storage,
								const char                      *path);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* __EVOLUTION_STORAGE_H__ */
