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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <bonobo/bonobo-object.h>

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
	EVOLUTION_STORAGE_ERROR_ALREADYREGISTERED,
	EVOLUTION_STORAGE_ERROR_NOTREGISTERED,
	EVOLUTION_STORAGE_ERROR_NOREGISTRY,
	EVOLUTION_STORAGE_ERROR_CORBA,
	EVOLUTION_STORAGE_ERROR_EXISTS,
	EVOLUTION_STORAGE_ERROR_INVALIDPARAMETER,
	EVOLUTION_STORAGE_ERROR_NOTFOUND,
	EVOLUTION_STORAGE_ERROR_GENERIC
};
typedef enum _EvolutionStorageResult EvolutionStorageResult;

struct _EvolutionStorage {
	BonoboObject parent;

	EvolutionStoragePrivate *priv;
};

struct _EvolutionStorageClass {
	BonoboObjectClass parent_class;
};


GtkType           evolution_storage_get_type   (void);
void              evolution_storage_construct  (EvolutionStorage  *storage,
						Evolution_Storage  corba_object,
						const char        *name);
EvolutionStorage *evolution_storage_new        (const char        *name);

EvolutionStorageResult  evolution_storage_register           (EvolutionStorage          *storage,
							      Evolution_StorageRegistry  corba_registry);
EvolutionStorageResult  evolution_storage_register_on_shell  (EvolutionStorage          *evolution_storage,
							      Evolution_Shell            corba_shell);

EvolutionStorageResult  evolution_storage_new_folder      (EvolutionStorage *evolution_storage,
							   const char       *path,
							   const char       *type,
							   const char       *physical_uri,
							   const char       *description);
EvolutionStorageResult  evolution_storage_removed_folder  (EvolutionStorage *evolution_storage,
							   const char       *path);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* __EVOLUTION_STORAGE_H__ */
