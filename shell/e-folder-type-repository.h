/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* e-folder-type-repository.h
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

#ifndef _E_FOLDER_TYPE_REPOSITORY_H_
#define _E_FOLDER_TYPE_REPOSITORY_H_

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <gtk/gtkobject.h>
#include <gdk-pixbuf/gdk-pixbuf.h>

#ifdef __cplusplus
extern "C" {
#pragma }
#endif /* __cplusplus */

#define E_TYPE_FOLDER_TYPE_REPOSITORY			(e_folder_type_repository_get_type ())
#define E_FOLDER_TYPE_REPOSITORY(obj)			(GTK_CHECK_CAST ((obj), E_TYPE_FOLDER_TYPE_REPOSITORY, EFolderTypeRepository))
#define E_FOLDER_TYPE_REPOSITORY_CLASS(klass)		(GTK_CHECK_CLASS_CAST ((klass), E_TYPE_FOLDER_TYPE_REPOSITORY, EFolderTypeRepositoryClass))
#define E_IS_FOLDER_TYPE_REPOSITORY(obj)			(GTK_CHECK_TYPE ((obj), E_TYPE_FOLDER_TYPE_REPOSITORY))
#define E_IS_FOLDER_TYPE_REPOSITORY_CLASS(klass)		(GTK_CHECK_CLASS_TYPE ((obj), E_TYPE_FOLDER_TYPE_REPOSITORY))


typedef struct _EFolderTypeRepository        EFolderTypeRepository;
typedef struct _EFolderTypeRepositoryPrivate EFolderTypeRepositoryPrivate;
typedef struct _EFolderTypeRepositoryClass   EFolderTypeRepositoryClass;

struct _EFolderTypeRepository {
	GtkObject parent;

	EFolderTypeRepositoryPrivate *priv;
};

struct _EFolderTypeRepositoryClass {
	GtkObjectClass parent_class;
};


GtkType                e_folder_type_repository_get_type   (void);
void                   e_folder_type_repository_construct  (EFolderTypeRepository *folder_type_repository);
EFolderTypeRepository *e_folder_type_repository_new        (void);

GdkPixbuf  *e_folder_type_repository_get_icon_for_type        (EFolderTypeRepository *folder_type_repository,
							       const char            *type_name,
							       gboolean               mini);
const char *e_folder_type_repository_get_icon_name_for_type   (EFolderTypeRepository *folder_type_repository,
							       const char            *type_name);
const char *e_folder_type_repository_get_control_id_for_type  (EFolderTypeRepository *folder_type_repository,
							       const char            *type_name);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* _E_FOLDER_TYPE_REPOSITORY_H_ */
