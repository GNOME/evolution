/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* e-local-folder.h
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

#ifndef _E_LOCAL_FOLDER_H_
#define _E_LOCAL_FOLDER_H_

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <gtk/gtkobject.h>

#include "e-folder.h"

#ifdef __cplusplus
extern "C" {
#pragma }
#endif /* __cplusplus */

#define E_TYPE_LOCAL_FOLDER		(e_local_folder_get_type ())
#define E_LOCAL_FOLDER(obj)		(GTK_CHECK_CAST ((obj), E_TYPE_LOCAL_FOLDER, ELocalFolder))
#define E_LOCAL_FOLDER_CLASS(klass)	(GTK_CHECK_CLASS_CAST ((klass), E_TYPE_LOCAL_FOLDER, ELocalFolderClass))
#define E_IS_LOCAL_FOLDER(obj)		(GTK_CHECK_TYPE ((obj), E_TYPE_LOCAL_FOLDER))
#define E_IS_LOCAL_FOLDER_CLASS(klass)	(GTK_CHECK_CLASS_TYPE ((obj), E_TYPE_LOCAL_FOLDER))

typedef struct _ELocalFolder        ELocalFolder;
typedef struct _ELocalFolderClass   ELocalFolderClass;

struct _ELocalFolder {
	EFolder parent;
};

struct _ELocalFolderClass {
	EFolderClass parent_class;
};


GtkType   e_local_folder_get_type       (void);
void      e_local_folder_construct      (ELocalFolder *local_folder,
					 const char   *name,
					 const char   *type,
					 const char   *description);
EFolder  *e_local_folder_new            (const char   *name,
					 const char   *type,
					 const char   *description);
EFolder  *e_local_folder_new_from_path  (const char   *physical_path);
gboolean  e_local_folder_save           (ELocalFolder *local_folder);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* __E_LOCAL_FOLDER_H__ */
