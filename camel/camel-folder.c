/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* camelFolder.c : Abstract class for an email folder */

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

#include "camel-folder.h"

static GtkObjectClass *camel_folder_parent_class=NULL;


static void camel_folder_open(CamelFolder *folder);
static void camel_folder_close(CamelFolder *folder, gboolean expunge);
static void camel_folder_set_name(CamelFolder *folder, GString *name_string);
static GString *camel_folder_get_name(CamelFolder *folder);
static gboolean camel_folder_can_hold_folders(CamelFolder *folder);
static gboolean camel_folder_can_hold_messages(CamelFolder *folder);
static gboolean camel_folder_exists(CamelFolder *folder);
static gboolean camel_folder_is_open(CamelFolder *folder);
static CamelFolder *camel_folder_get_folder(CamelFolder *folder, GString *folderName);

static void
camel_folder_class_init (CamelFolderClass *camel_folder_class)
{
	camel_folder_parent_class = gtk_type_class (gtk_object_get_type ());
	
	/* virtual method definition */
	camel_folder_class->open = camel_folder_open;
	camel_folder_class->close = camel_folder_close;
	camel_folder_class->set_name = camel_folder_set_name;
	camel_folder_class->get_name = camel_folder_get_name;
	camel_folder_class->can_hold_folders = camel_folder_can_hold_folders;
	camel_folder_class->can_hold_messages = camel_folder_can_hold_messages;
	camel_folder_class->exists = camel_folder_exists;
	camel_folder_class->is_open = camel_folder_is_open;
	camel_folder_class->get_folder = camel_folder_get_folder;
	/* virtual method overload */
}







GtkType
gnome_camel_get_type (void)
{
	static GtkType camel_folder_type = 0;
	
	if (!camel_folder_type)	{
		GtkTypeInfo camel_folder_info =	
		{
			"CamelFolder",
			sizeof (CamelFolder),
			sizeof (CamelFolderClass),
			(GtkClassInitFunc) camel_folder_class_init,
			(GtkObjectInitFunc) NULL,
				/* reserved_1 */ NULL,
				/* reserved_2 */ NULL,
			(GtkClassInitFunc) NULL,
		};
		
		camel_folder_type = gtk_type_unique (gtk_object_get_type (), &camel_folder_info);
	}
	
	return camel_folder_type;
}


/** 
 * camel_folder_open: Open a folder
 *
 * Put a folder in its opened state. 
 * 
 **/ 
static void
camel_folder_open(CamelFolder *folder)
{
	folder->open_state = FOLDER_OPEN;
}

/** 
 * camel_folder_close: Close a folder.
 *
 * Put a folder in its closed state, and possibly 
 * expunge the flagged messages.
 * 
 * @expunge: if TRUE, the flagged message are
 * deleted.
 **/ 
static void
camel_folder_close(CamelFolder *folder, gboolean expunge)
{
#warning implement the expunge flag
    folder->open_state = FOLDER_CLOSE;
}



/** 
 * camel_folder_set_name : set the (short) name of the folder
 *
 * set the name of the folder. 
 * The old name object is freed.
 *
 * @name_string: new name of the folder
 *
 **/
static void
camel_folder_set_name(CamelFolder *folder, GString *name_string)
{
    if (folder->name) g_string_free(folder->name, 0);;
    folder->name = name_string;
}



/** 
 * camel_folder_get_name : get the (short) name of the folder
 *
 * get the name of the folder. The fully qualified name
 * can be obtained with the get_full_ame method (not implemented)
 *
 * @Return Value: name of the folder
 *
 **/
static GString *
camel_folder_get_name(CamelFolder *folder)
{
	return folder->name;
}


/**
 * camel_folder_can_hold_folders : tests if the folder can contain other folders
 *
 **/
static gboolean
camel_folder_can_hold_folders(CamelFolder *folder)
{
    return folder->can_hold_folders;
}



/** 
 * camel_folder_can_hold_messages : tests if the folder can contain messages
 *
 **/
static gboolean
camel_folder_can_hold_messages(CamelFolder *folder)
{
    return folder->can_hold_messages;
}



/** 
 * camel_folder_exists : tests if the folder object exists on the store.
 *
 **/
static gboolean
camel_folder_exists(CamelFolder *folder)
{
    return folder->exists_on_store;
}



/** 
 * camel_folder_is_open : test if the folder is open
 *
 **/
static gboolean
camel_folder_is_open(CamelFolder *folder)
{
    return (folder->open_state==FOLDER_OPEN);
}




/** 
 * camel_folder_get_folder: return the (sub)folder object that
 * is specified.
 *
 * This method returns a folder objects. This folder
 * is necessarily a subfolder of the current folder. 
 * It is an error to ask a folder begining with the 
 * folder separator character.  
 * 
 * @folderName: subfolder path. NULL if the subfolder object
 *        could not be created
 **/
static CamelFolder *
camel_folder_get_folder(CamelFolder *folder, GString *folderName)
{
    g_warning("getFolder called on the abstract CamelFolder class\n");
    return NULL;
}


