/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* evolution-storage.h
 *
 * Copyright (C) 2000  Ximian, Inc.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of version 2 of the GNU General Public
 * License as published by the Free Software Foundation.
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

#include "Evolution.h"

#include <glib.h>

#include <gdk-pixbuf/gdk-pixbuf.h>
#include <gtk/gtktypeutils.h>

#include <bonobo/bonobo-xobject.h>

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
	BonoboXObject parent;

	EvolutionStoragePrivate *priv;
};

struct _EvolutionStorageClass {
	BonoboXObjectClass parent_class;

	/* signals */
	void (*create_folder) (EvolutionStorage *storage,
			       const Bonobo_Listener listener,
			       const char *path,
			       const char *type,
			       const char *description,
			       const char *parent_physical_uri);

	void (*remove_folder) (EvolutionStorage *storage,
			       const Bonobo_Listener listener,
			       const char *path,
			       const char *physical_uri);

	void (*xfer_folder) (EvolutionStorage *storage,
			     const Bonobo_Listener listener,
			     const char *source_path,
			     const char *destination_path,
			     gboolean remove_source);

	void (*open_folder) (EvolutionStorage *storage,
			     const char *path);

	void (*update_folder) (EvolutionStorage *storage,
			       const char *path,
			       int unread_count);

	void (*discover_shared_folder) (EvolutionStorage *storage,
					Bonobo_Listener listener,
					const char *user,
					const char *folder_name);

	void (*cancel_discover_shared_folder)  (EvolutionStorage *storage,
						const char *user,
						const char *folder_name);

	void (*remove_shared_folder) (EvolutionStorage *storage,
				      Bonobo_Listener listener,
				      const char *path);

	void (*show_folder_properties) (EvolutionStorage *storage,
					const char *path,
					unsigned int itemNumber,
					unsigned long parentWindowId);

	POA_GNOME_Evolution_Storage__epv epv;
};


GtkType                 evolution_storage_get_type             (void);
void                    evolution_storage_construct            (EvolutionStorage                *storage,
								const char                      *name,
								gboolean                         has_shared_folders);
EvolutionStorage       *evolution_storage_new                  (const char                      *name,
								gboolean                         has_shared_folders);

void                    evolution_storage_rename               (EvolutionStorage                *storage,
								const char                      *new_name);

EvolutionStorageResult  evolution_storage_register             (EvolutionStorage                *storage,
								GNOME_Evolution_StorageRegistry  corba_registry);
EvolutionStorageResult  evolution_storage_register_on_shell    (EvolutionStorage                *evolution_storage,
								GNOME_Evolution_Shell            corba_shell);
EvolutionStorageResult  evolution_storage_deregister_on_shell  (EvolutionStorage                *storage,
							        GNOME_Evolution_Shell            corba_shell);
EvolutionStorageResult  evolution_storage_new_folder           (EvolutionStorage                *evolution_storage,
								const char                      *path,
								const char                      *display_name,
								const char                      *type,
								const char                      *physical_uri,
								const char                      *description,
								const char                      *custom_icon_name,
								int                              unread_count,
								gboolean                         can_sync_offline,
								int                              sorting_priority);
EvolutionStorageResult  evolution_storage_update_folder        (EvolutionStorage                *evolution_storage,
								const char                      *path,
								int                              unread_count);
EvolutionStorageResult  evolution_storage_update_folder_by_uri (EvolutionStorage                *evolution_storage,
								const char                      *physical_uri,
								int                              unread_count);
EvolutionStorageResult  evolution_storage_removed_folder       (EvolutionStorage                *evolution_storage,
								const char                      *path);
gboolean                evolution_storage_folder_exists        (EvolutionStorage                *evolution_storage,
								const char                      *path);
EvolutionStorageResult  evolution_storage_has_subfolders       (EvolutionStorage                *evolution_storage,
								const char                      *path,
								const char                      *message);

void  evolution_storage_add_property_item  (EvolutionStorage *evolution_storage,
					    const char       *label,
					    const char       *tooltip,
					    GdkPixbuf        *icon);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* __EVOLUTION_STORAGE_H__ */
