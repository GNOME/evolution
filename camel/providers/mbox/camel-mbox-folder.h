/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* camel-mbox-folder.h : Abstract class for an email folder */

/* 
 *
 * Author : Bertrand Guiheneuf <bertrand@helixcode.com> 
 *
 * Copyright (C) 1999 Helix Code .
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


#ifndef CAMEL_MBOX_FOLDER_H
#define CAMEL_MBOX_FOLDER_H 1


#ifdef __cplusplus
extern "C" {
#pragma }
#endif /* __cplusplus }*/

#include <gtk/gtk.h>
#include "camel-folder.h"
/*  #include "camel-store.h" */

#define CAMEL_MBOX_FOLDER_TYPE     (camel_mbox_folder_get_type ())
#define CAMEL_MBOX_FOLDER(obj)     (GTK_CHECK_CAST((obj), CAMEL_MBOX_FOLDER_TYPE, CamelMboxFolder))
#define CAMEL_MBOX_FOLDER_CLASS(k) (GTK_CHECK_CLASS_CAST ((k), CAMEL_MBOX_FOLDER_TYPE, CamelMboxFolderClass))
#define IS_CAMEL_MBOX_FOLDER(o)    (GTK_CHECK_TYPE((o), CAMEL_MBOX_FOLDER_TYPE))


typedef struct {
	CamelFolder parent_object;
	
	gchar *folder_file_path; /* contains the messages */
	gchar *folder_dir_path;  /* contains the subfolders */
	GArray *uid_array;

} CamelMboxFolder;



typedef struct {
	CamelFolderClass parent_class;

	/* Virtual methods */	
	
} CamelMboxFolderClass;


/* public methods */

/* Standard Gtk function */
GtkType camel_mbox_folder_get_type (void);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* CAMEL_MBOX_FOLDER_H */
