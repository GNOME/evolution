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
#include "gstring-util.h"

static GtkObjectClass *parent_class=NULL;

/* Returns the class for a CamelFolder */
#define CF_CLASS(so) CAMEL_FOLDER_CLASS (GTK_OBJECT(so)->klass)

static void __camel_folder_init_with_store(CamelFolder *folder, CamelStore *parent_store);
static void __camel_folder_open(CamelFolder *folder, CamelFolderOpenMode mode);
static void __camel_folder_close(CamelFolder *folder, gboolean expunge);
static void __camel_folder_set_name(CamelFolder *folder, GString *name_);
static void __camel_folder_set_full_name(CamelFolder *folder, GString *name);
static GString *__camel_folder_get_name(CamelFolder *folder);
static GString *__camel_folder_get_full_name(CamelFolder *folder);
static gboolean __camel_folder_can_hold_folders(CamelFolder *folder);
static gboolean __camel_folder_can_hold_messages(CamelFolder *folder);
static gboolean __camel_folder_exists(CamelFolder *folder);
static gboolean __camel_folder_is_open(CamelFolder *folder);
static CamelFolder *__camel_folder_get_folder(CamelFolder *folder, GString *folder_name);
static gboolean __camel_folder_create(CamelFolder *folder);
static gboolean __camel_folder_delete (CamelFolder *folder, gboolean recurse);
static gboolean __camel_folder_delete_messages(CamelFolder *folder);
static CamelFolder *__camel_folder_get_parent_folder (CamelFolder *folder);
static CamelStore *__camel_folder_get_parent_store (CamelFolder *folder);
static CamelFolderOpenMode __camel_folder_get_mode(CamelFolder *folder);
static GList *__camel_folder_list_subfolders(CamelFolder *folder);

static void
camel_folder_class_init (CamelFolderClass *camel_folder_class)
{
	parent_class = gtk_type_class (gtk_object_get_type ());
	
	/* virtual method definition */
	camel_folder_class->init_with_store = __camel_folder_init_with_store;
	camel_folder_class->open = __camel_folder_open;
	camel_folder_class->close = __camel_folder_close;
	camel_folder_class->set_name = __camel_folder_set_name;
	camel_folder_class->get_name = __camel_folder_get_name;
	camel_folder_class->can_hold_folders = __camel_folder_can_hold_folders;
	camel_folder_class->can_hold_messages = __camel_folder_can_hold_messages;
	camel_folder_class->exists = __camel_folder_exists;
	camel_folder_class->is_open = __camel_folder_is_open;
	camel_folder_class->get_folder = __camel_folder_get_folder;
	camel_folder_class->create = __camel_folder_create;
	camel_folder_class->delete = __camel_folder_delete;
	camel_folder_class->delete_messages = __camel_folder_delete_messages;
	camel_folder_class->get_parent_folder = __camel_folder_get_parent_folder;
	camel_folder_class->get_parent_store = __camel_folder_get_parent_store;
	camel_folder_class->get_mode = __camel_folder_get_mode;
	camel_folder_class->list_subfolders = __camel_folder_list_subfolders;

	/* virtual method overload */
}







GtkType
camel_folder_get_type (void)
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
 * __camel_folder_init_with_store: init the folder by setting its parent store.
 * @folder: folder object to initialize
 * @parent_store: parent store object of the folder
 * 
 * 
 **/
static void 
__camel_folder_init_with_store(CamelFolder *folder, CamelStore *parent_store)
{
	g_assert(folder);
	g_assert(parent_store);

	folder->parent_store = parent_store;
}




/**
 * __camel_folder_open: Open a folder
 * @folder: 
 * @mode: open mode (R/W/RW ?)
 * 
 **/
static void
__camel_folder_open(CamelFolder *folder, CamelFolderOpenMode mode)
{
	folder->open_state = FOLDER_OPEN;
	folder->open_mode = mode;
}


/**
 * __camel_folder_close:Close a folder.
 * @folder: 
 * @expunge: if TRUE, the flagged message are deleted.
 * 
 * Put a folder in its closed state, and possibly 
 * expunge the flagged messages.
 **/
static void
__camel_folder_close(CamelFolder *folder, gboolean expunge)
{
#warning implement the expunge flag
    folder->open_state = FOLDER_CLOSE;
}




/**
 * __camel_folder_set_name:set the (short) name of the folder
 * @folder: folder
 * @name: new name of the folder
 * 
 * set the name of the folder. 
 * The old name object is freed.
 * 
 **/
static void
__camel_folder_set_name(CamelFolder *folder, GString *name)
{
    if (folder->name) g_string_free(folder->name, 0);;
    folder->name = name;
}



/**
 * __camel_folder_set_full_name:set the (full) name of the folder
 * @folder: folder
 * @name: new name of the folder
 * 
 * set the name of the folder. 
 * The old name object is freed.
 * 
 **/
static void
__camel_folder_set_full_name(CamelFolder *folder, GString *name)
{
    if (folder->full_name) g_string_free(folder->full_name, 0);;
    folder->full_name = name;
}




/**
 * __camel_folder_get_name: get the (short) name of the folder
 * @folder: 
 * 
 * get the name of the folder. The fully qualified name
 * can be obtained with the get_full_ame method (not implemented)
 *
 * Return value: name of the folder
 **/
static GString *
__camel_folder_get_name(CamelFolder *folder)
{
	return folder->name;
}


/**
 * __camel_folder_get_full_name:get the (full) name of the folder
 * @folder: folder to get the name 
 * 
 * get the name of the folder. 
 * 
 * Return value: full name of the folder
 **/
static GString *
__camel_folder_get_full_name(CamelFolder *folder)
{
	return folder->full_name;
}



/**
 * __camel_folder_can_hold_folders: tests if the folder can contain other folders
 * @folder: 
 * 
 * 
 * 
 * Return value: 
 **/
static gboolean
__camel_folder_can_hold_folders(CamelFolder *folder)
{
    return folder->can_hold_folders;
}




/**
 * __camel_folder_can_hold_messages: tests if the folder can contain messages
 * @folder: 
 * 
 * 
 * 
 * Return value: 
 **/
static gboolean
__camel_folder_can_hold_messages(CamelFolder *folder)
{
    return folder->can_hold_messages;
}



/**
 * __camel_folder_exists: tests if the folder object exists on the store.
 * @folder: 
 * 
 * 
 * 
 * Return value: 
 **/
static gboolean
__camel_folder_exists(CamelFolder *folder)
{
    return folder->exists_on_store;
}



/**
 * __camel_folder_is_open:
 * @folder: 
 * 
 * 
 * 
 * Return value: 
 **/
static gboolean
__camel_folder_is_open(CamelFolder *folder)
{
    return (folder->open_state==FOLDER_OPEN);
}




/** 
 * __camel_folder_get_folder: return the (sub)folder object that
 * is specified.
 *
 * This method returns a folder objects. This folder
 * is necessarily a subfolder of the current folder. 
 * It is an error to ask a folder begining with the 
 * folder separator character.  
 * 
 * @folder : the folder
 * @folderName: subfolder path. NULL if the subfolder object
 *        could not be created
 **/
static CamelFolder *
__camel_folder_get_folder(CamelFolder *folder, GString *folder_name)
{
    g_warning("getFolder called on the abstract CamelFolder class\n");
    return NULL;
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
 * @folder : the folder
 * @folderName: subfolder path. NULL if the subfolder object
 *        could not be created
 **/
CamelFolder *
camel_folder_get_folder(CamelFolder *folder, GString *folder_name)
{
	return (CF_CLASS(folder)->get_folder(folder,folder_name));
}




/**
 * __camel_folder_create:
 * @folder: 
 * 
 * this routine handles the recursion mechanism.
 * Children classes have to implement the actual
 * creation mechanism. They must call this method
 * before physically creating the folder in order
 * to be sure the parent folder exists.
 * 
 * Return value: 
 **/
static gboolean
__camel_folder_create(CamelFolder *folder)
{
	GString *prefix;
	gchar dich_result;
	CamelFolder *parent;
	gchar sep;


	g_assert(folder->parent_store);
	g_assert(folder->name);

	if ( CF_CLASS(folder)->exists(folder) ) return TRUE;
	sep = camel_store_get_separator(folder->parent_store);	
	if (folder->parent_folder) camel_folder_create(folder->parent_folder);
	else {   
		if (folder->full_name) {
			dich_result = g_string_right_dichotomy(folder->full_name, sep, &prefix, NULL, STRIP_TRAILING);
			if (dich_result!='o') {
				g_warning("I have to handle the case where the path is not OK\n"); 
				return FALSE;
			} else {
				parent = camel_store_get_folder(folder->parent_store, prefix);
				camel_folder_create(parent);
				gtk_object_unref (GTK_OBJECT(parent));
			}
		}
	}	
	return TRUE;
}

	
/**
 * camel_folder_create: create the folder object on the physical store
 * @folder: folder object to create
 * 
 * This routine physically creates the folder object on 
 * the store. Having created the  object does not
 * mean the folder physically exists. If it does not
 * exists, this routine will create it.
 * if the folder full name contains more than one level
 * of hierarchy, all folders between the current folder
 * and the last folder name will be created if not existing.
 * 
 * Return value: 
 **/
gboolean
camel_folder_create(CamelFolder *folder)
{
	return (CF_CLASS(folder)->create(folder));
}





/**
 * __camel_folder_delete: delete folder 
 * @folder: folder to delete
 * @recurse: true is subfolders must also be deleted
 * 
 * Delete a folder and its subfolders (if recurse is TRUE).
 * The scheme is the following:
 * 1) delete all messages in the folder
 * 2) if recurse is FALSE, and if there are subfolders
 *    return FALSE, else delete current folder and retuen TRUE
 *    if recurse is TRUE, delete subfolders, delete
 *    current folder and return TRUE
 * 
 * subclasses implementing a protocol with a different 
 * deletion behaviour must emulate this one or implement
 * empty folders deletion and call  this routine which 
 * will do all the works for them.
 * Opertions must be done in the folllowing order:
 *  - call this routine
 *  - delete empty folder
 * 
 * Return value: true if the folder has been deleted
 **/
static gboolean
__camel_folder_delete (CamelFolder *folder, gboolean recurse)
{
	GList *subfolders=NULL;
	GList *sf;
	gboolean ok;
	
	g_assert(folder);

	/* method valid only on closed folders */
	if (folder->open_state != FOLDER_CLOSE) return FALSE;

	/* delete all messages in the folder */
	CF_CLASS(folder)->delete_messages(folder);

	subfolders = CF_CLASS(folder)->list_subfolders(folder); 
	if (recurse) { /* delete subfolders */
		if (subfolders) {
			sf = subfolders;
			do {
				CF_CLASS(sf->data)->delete(sf->data, TRUE);;
			} while (sf = sf->next);
		}
	} else if (subfolders) return FALSE;
	
	
	return TRUE;
}



/**
 * camel_folder_delete: delete a folder
 * @folder: folder to delete
 * @recurse: TRUE if subfolders must be deleted
 * 
 * Delete a folder. All messages in the folder 
 * are deleted before the folder is deleted. 
 * When recurse is true, all subfolders are
 * deleted too. When recurse is FALSE and folder 
 * contains subfolders, all messages are deleted,
 * but folder deletion fails. 
 * 
 * Return value: TRUE if deletion was successful
 **/
gboolean camel_folder_delete (CamelFolder *folder, gboolean recurse)
{
	return CF_CLASS(folder)->delete(folder, recurse);
}





/**
 * __camel_folder_delete_messages: delete all messages in the folder
 * @folder: 
 * 
 * 
 * 
 * Return value: 
 **/
static gboolean 
__camel_folder_delete_messages(CamelFolder *folder)
{
	return TRUE;
}


/**
 * camel_folder_delete_messages: delete all messages in the folder
 * @folder: folder 
 * 
 * delete all messages stored in a folder
 * 
 * Return value: TRUE if the messages could be deleted
 **/
gboolean
camel_folder_delete_messages (CamelFolder *folder)
{
	return CF_CLASS(folder)->delete_messages(folder);
}






/**
 * __camel_folder_get_parent_folder: return parent folder
 * @folder: folder to get the parent
 * 
 * 
 * 
 * Return value: 
 **/
static CamelFolder *
__camel_folder_get_parent_folder (CamelFolder *folder)
{
	return folder->parent_folder;
}


/**
 * camel_folder_get_parent_folder:return parent folder
 * @folder: folder to get the parent
 * 
 * 
 * 
 * Return value: 
 **/
CamelFolder *
camel_folder_get_parent_folder (CamelFolder *folder)
{
	return CF_CLASS(folder)->get_parent_folder(folder);
}


/**
 * __camel_folder_get_parent_store: return parent store
 * @folder: folder to get the parent
 * 
 * 
 * 
 * Return value: 
 **/
static CamelStore *
__camel_folder_get_parent_store (CamelFolder *folder)
{
	return folder->parent_store;
}


/**
 * camel_folder_get_parent_store:return parent store
 * @folder: folder to get the parent
 * 
 * 
 * 
 * Return value: 
 **/
CamelStore *
camel_folder_get_parent_store (CamelFolder *folder)
{
	return CF_CLASS(folder)->get_parent_store(folder);
}



/**
 * __camel_folder_get_mode: return the open mode of a folder
 * @folder: 
 * 
 * 
 * 
 * Return value:  open mode of the folder
 **/
static CamelFolderOpenMode
__camel_folder_get_mode(CamelFolder *folder)
{
    return folder->open_mode;
}


/**
 * camel_folder_get_mode: return the open mode of a folder
 * @folder: 
 * 
 * 
 * 
 * Return value:  open mode of the folder
 **/
CamelFolderOpenMode
camel_folder_get_mode(CamelFolder *folder)
{
    return CF_CLASS(folder)->get_mode(folder);
}




static GList *
__camel_folder_list_subfolders(CamelFolder *folder)
{
	return NULL;
}


GList *
camel_folder_list_subfolders(CamelFolder *folder)
{
    return CF_CLASS(folder)->list_subfolders(folder);
}

