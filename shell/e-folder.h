 /* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* e-folder.h
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

#ifndef _E_FOLDER_H_
#define _E_FOLDER_H_

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <gtk/gtkobject.h>

#ifdef __cplusplus
extern "C" {
#pragma }
#endif /* __cplusplus */

#define E_TYPE_FOLDER			(e_folder_get_type ())
#define E_FOLDER(obj)			(GTK_CHECK_CAST ((obj), E_TYPE_FOLDER, EFolder))
#define E_FOLDER_CLASS(klass)		(GTK_CHECK_CLASS_CAST ((klass), E_TYPE_FOLDER, EFolderClass))
#define E_IS_FOLDER(obj)		(GTK_CHECK_TYPE ((obj), E_TYPE_FOLDER))
#define E_IS_FOLDER_CLASS(klass)	(GTK_CHECK_CLASS_TYPE ((obj), E_TYPE_FOLDER))


typedef struct _EFolder        EFolder;
typedef struct _EFolderPrivate EFolderPrivate;
typedef struct _EFolderClass   EFolderClass;

struct _EFolder {
	GtkObject parent;

	EFolderPrivate *priv;
};

struct _EFolderClass {
	GtkObjectClass parent_class;

	/* Virtual methods.  */
	gboolean     (* save_info) 	  (EFolder *folder);
	gboolean     (* load_info) 	  (EFolder *folder);
	gboolean     (* remove)    	  (EFolder *folder);
	const char * (* get_physical_uri) (EFolder *folder);

	/* Signals.  */
	void (* changed) (EFolder *folder);
};


GtkType  e_folder_get_type   (void);
void     e_folder_construct  (EFolder    *folder,
			      const char *name,
			      const char *type,
			      const char *description);
EFolder *e_folder_new        (const char *name,
			      const char *type,
			      const char *description);

const char *e_folder_get_physical_uri  (EFolder *folder);

const char *e_folder_get_name         (EFolder *folder);
const char *e_folder_get_type_string  (EFolder *folder);
const char *e_folder_get_description  (EFolder *folder);

void  e_folder_set_name         (EFolder *folder, const char *name);
void  e_folder_set_type_string  (EFolder *folder, const char *type);
void  e_folder_set_description  (EFolder *folder, const char *description);
void  e_folder_set_physical_uri (EFolder *folder, const char *physical_uri);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* _E_FOLDER_H_ */
