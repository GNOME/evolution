/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* e-storage.h
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

#ifndef _E_STORAGE_H_
#define _E_STORAGE_H_

#include <gtk/gtkobject.h>

#include "evolution-shell-component-client.h"

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

enum _EStorageResult {
	E_STORAGE_OK,
	E_STORAGE_GENERICERROR,
	E_STORAGE_EXISTS,
	E_STORAGE_INVALIDTYPE,
	E_STORAGE_IOERROR,
	E_STORAGE_NOSPACE,
	E_STORAGE_NOTEMPTY,
	E_STORAGE_NOTFOUND,
	E_STORAGE_NOTIMPLEMENTED,
	E_STORAGE_PERMISSIONDENIED,
	E_STORAGE_UNSUPPORTEDOPERATION,
	E_STORAGE_UNSUPPORTEDTYPE,
	E_STORAGE_CANTCHANGESTOCKFOLDER,
	E_STORAGE_CANTMOVETODESCENDANT,
	E_STORAGE_NOTONLINE,
	E_STORAGE_INVALIDNAME
};
typedef enum _EStorageResult EStorageResult;

typedef void (* EStorageResultCallback) (EStorage *storage, EStorageResult result, void *data);
typedef void (* EStorageDiscoveryCallback) (EStorage *storage, EStorageResult result, const char *path, void *data);

#include "e-folder.h"

struct _EStorage {
	GtkObject parent;

	EStoragePrivate *priv;
};

struct _EStorageClass {
	GtkObjectClass parent_class;

	/* Signals.  */

	void (* new_folder)     (EStorage *storage, const char *path);
	void (* updated_folder) (EStorage *storage, const char *path);
	void (* removed_folder) (EStorage *storage, const char *path);
	void (* close_folder)   (EStorage *storage, const char *path);

	/* Virtual methods.  */

	GList      * (* get_subfolder_paths)     (EStorage *storage,
						  const char *path);
	EFolder    * (* get_folder)    	         (EStorage *storage,
						  const char *path);
	const char * (* get_name)      	         (EStorage *storage);

	void         (* async_create_folder)  (EStorage *storage,
					       const char *path,
					       const char *type,
					       const char *description,
					       EStorageResultCallback callback,
					       void *data);

	void         (* async_remove_folder)  (EStorage *storage,
					       const char *path,
					       EStorageResultCallback callback,
					       void *data);

	void         (* async_xfer_folder)    (EStorage *storage,
					       const char *source_path,
					       const char *destination_path,
					       const gboolean remove_source,
					       EStorageResultCallback callback,
					       void *data);

	void         (* async_open_folder)    (EStorage *storage,
					       const char *path,
					       EStorageDiscoveryCallback callback,
					       void *data);

	gboolean     (* supports_shared_folders)       (EStorage *storage);
	void         (* async_discover_shared_folder)  (EStorage *storage,
						        const char *owner,
						        const char *folder_name,
						        EStorageDiscoveryCallback callback,
						        void *data);
	void         (* cancel_discover_shared_folder) (EStorage *storage,
							const char *owner,
							const char *folder_name);
	void         (* async_remove_shared_folder)    (EStorage *storage,
						        const char *path,
						        EStorageResultCallback callback,
						        void *data);
};


GtkType     e_storage_get_type                (void);
void        e_storage_construct               (EStorage   *storage,
					       const char *name,
					       EFolder    *root_folder);
EStorage   *e_storage_new                     (const char *name,
					       EFolder    *root_folder);

gboolean    e_storage_path_is_relative        (const char *path);
gboolean    e_storage_path_is_absolute        (const char *path);

GList      *e_storage_get_subfolder_paths     (EStorage   *storage,
					       const char *path);
EFolder    *e_storage_get_folder              (EStorage   *storage,
					       const char *path);

const char *e_storage_get_name                (EStorage *storage);

/* Folder operations.  */

void  e_storage_async_create_folder  (EStorage               *storage,
				      const char             *path,
				      const char             *type,
				      const char             *description,
				      EStorageResultCallback  callback,
				      void                   *data);
void  e_storage_async_remove_folder  (EStorage               *storage,
				      const char             *path,
				      EStorageResultCallback  callback,
				      void                   *data);
void  e_storage_async_xfer_folder    (EStorage               *storage,
				      const char             *source_path,
				      const char             *destination_path,
				      const gboolean          remove_source,
				      EStorageResultCallback  callback,
				      void                   *data);
void  e_storage_async_open_folder    (EStorage                  *storage,
				      const char                *path,
				      EStorageDiscoveryCallback  callback,
				      void                      *data);

const char *e_storage_result_to_string  (EStorageResult result);

/* Shared folders.  */
gboolean    e_storage_supports_shared_folders       (EStorage                 *storage);
void        e_storage_async_discover_shared_folder  (EStorage                 *storage,
						     const char               *owner,
						     const char               *folder_name,
						     EStorageDiscoveryCallback callback,
						     void                     *data);
void        e_storage_cancel_discover_shared_folder (EStorage                 *storage,
						     const char               *owner,
						     const char               *folder_name);
void        e_storage_async_remove_shared_folder    (EStorage                 *storage,
						     const char               *path,
						     EStorageResultCallback    callback,
						     void                     *data);

/* Utility functions.  */

char *e_storage_get_path_for_physical_uri  (EStorage   *storage,
					    const char *physical_uri);

/* Protected.  C++ anyone?  */
gboolean e_storage_new_folder             (EStorage   *storage,
					   const char *path,
					   EFolder    *folder);
gboolean e_storage_removed_folder         (EStorage   *storage,
					   const char *path);

gboolean e_storage_declare_has_subfolders (EStorage   *storage,
					   const char *path,
					   const char *message);
gboolean e_storage_get_has_subfolders     (EStorage   *storage,
					   const char *path);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* _E_STORAGE_H_ */
