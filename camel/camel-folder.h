/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* camelFolder.h : Abstract class for an email folder */

/* 
 *
 * Copyright (C) 1999 Bertrand Guiheneuf <Bertrand.Guiheneuf@inria.fr> .
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


#ifndef CAMEL_FOLDER_H
#define CAMEL_FOLDER_H 1


#ifdef __cplusplus
extern "C" {
#pragma }
#endif /* __cplusplus }*/

#include <gtk/gtk.h>


#define CAMEL_FOLDER_TYPE     (camel_folder_get_type ())
#define CAMEL_FOLDER(obj)     (GTK_CHECK_CAST((obj), CAMEL_FOLDER_TYPE, CamelFolder))
#define CAMEL_FOLDER_CLASS(k) (GTK_CHECK_CLASS_CAST ((k), CAMEL_FOLDER_TYPE, CamelFolderClass))
#define IS_CAMEL_FOLDER(o)    (GTK_CHECK_TYPE((o), CAMEL_FOLDER_TYPE))

typedef enum {
	FOLDER_OPEN,
	FOLDER_CLOSE
} CamelFolderState;

typedef enum {
	FOLDER_OPEN_UNKNOWN,   /* folder open mode is unknown */
	FOLDER_OPEN_READ,      /* folder is read only         */ 
	FOLDER_OPEN_READ_WRITE /* folder is read/write        */ 
} CamelFolderOpenMode;




typedef struct
{
	GtkObject parent_object;

	GList *message_list;
	gboolean can_hold_folders;
	gboolean can_hold_messages;
	gboolean exists_on_store;
	CamelFolderOpenMode open_mode;
	CamelFolderState open_state;
	GString *name;
	/*
	CamelStore *parent_store;
	CamelFolder *parent_folder;
	*/
	
} CamelFolder;



typedef struct {
	GtkObjectClass parent_class;

	/* Virtual methods */	
	void   (*open) (CamelFolder *object);
	void   (*close) (CamelFolder *folder, gboolean expunge);
	void   (*set_name) (CamelFolder *folder, GString *name_string);
	GString * (*get_name) (CamelFolder *folder);
} CamelFolderClass;


#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* CAMEL_FOLDER_H */
