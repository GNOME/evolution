/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* camel-mh-folder.h : Abstract class for an email folder */

/* 
 *
 * Copyright (C) 1999 Bertrand Guiheneuf <Bertrand.Guiheneuf@aful.org> .
 *
 * This program is free software; you can redistribute it and/or 
 * modify it under the terms of the GNU General Public License as 
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307
 * USA
 */


#ifndef CAMEL_MH_FOLDER_H
#define CAMEL_MH_FOLDER_H 1


#ifdef __cplusplus
extern "C" {
#pragma }
#endif /* __cplusplus }*/

#include <gtk/gtk.h>
#include "camel-folder.h"
/*  #include "camel-store.h" */

#define CAMEL_MH_FOLDER_TYPE     (camel_mh_folder_get_type ())
#define CAMEL_MH_FOLDER(obj)     (GTK_CHECK_CAST((obj), CAMEL_MH_FOLDER_TYPE, CamelMhFolder))
#define CAMEL_MH_FOLDER_CLASS(k) (GTK_CHECK_CLASS_CAST ((k), CAMEL_MH_FOLDER_TYPE, CamelMhFolderClass))
#define IS_CAMEL_MH_FOLDER(o)    (GTK_CHECK_TYPE((o), CAMEL_MH_FOLDER_TYPE))


typedef struct {
	CamelFolder parent_object;
	
	gchar *directory_path;
	GList *file_name_list;

} CamelMhFolder;



typedef struct {
	CamelFolderClass parent_class;

	/* Virtual methods */	
	
} CamelMhFolderClass;


/* public methods */

/* Standard Gtk function */
GtkType camel_mh_folder_get_type (void);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* CAMEL_MH_FOLDER_H */
