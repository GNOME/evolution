/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* camelFolder.c : Abstract class for an email folder */

/* 
 *
 * Author : 
 *  Bertrand Guiheneuf <bertrand@helixcode.com>
 *
 * Copyright 1999, 2000 HelixCode (http://www.helixcode.com) .
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
#include <config.h>
#include "camel-folder.h"
#include "camel-log.h"
#include "camel-exception.h"
#include "camel-store.h"
#include "string-utils.h"

static GtkObjectClass *parent_class=NULL;

/* Returns the class for a CamelFolder */
#define CF_CLASS(so) CAMEL_FOLDER_CLASS (GTK_OBJECT (so)->klass)




static void _init_with_store (CamelFolder *folder, 
			      CamelStore *parent_store, 
			      CamelException *ex);
static void _finalize (GtkObject *object);


static void _open (CamelFolder *folder, 
		   CamelFolderOpenMode mode, 
		   CamelException *ex);
static void _close (CamelFolder *folder, 
		    gboolean expunge, 
		    CamelException *ex);

#ifdef FOLDER_ASYNC_TEST

/* Async operations are not used for the moment */
static void _open_async (CamelFolder *folder, 
			 CamelFolderOpenMode mode, 
			 CamelFolderAsyncCallback callback, 
			 gpointer user_data, 
			 CamelException *ex);
static void _close_async (CamelFolder *folder, 
			  gboolean expunge, 
			  CamelFolderAsyncCallback callback, 
			  gpointer user_data, 
			  CamelException *ex);

#endif 

static void _set_name (CamelFolder *folder, 
		       const gchar *name, 
		       CamelException *ex);
static const gchar *_get_name (CamelFolder *folder, 
			       CamelException *ex);
static const gchar *_get_full_name (CamelFolder *folder, CamelException *ex);


static gboolean _can_hold_folders  (CamelFolder *folder);
static gboolean _can_hold_messages (CamelFolder *folder);
static gboolean _exists  (CamelFolder  *folder, CamelException *ex);
static gboolean _is_open (CamelFolder *folder);
static const GList *_list_permanent_flags (CamelFolder *folder,
					   CamelException *ex);
static CamelFolderOpenMode _get_mode      (CamelFolder *folder,
					   CamelException *ex);


static gboolean _create (CamelFolder *folder, CamelException *ex);
static gboolean _delete (CamelFolder *folder, gboolean recurse,
			 CamelException *ex);


static GList *_list_subfolders  (CamelFolder *folder, CamelException *ex);
static CamelFolder *_get_subfolder (CamelFolder *folder, 
				    const gchar *folder_name,
				    CamelException *ex);
static CamelFolder *_get_parent_folder (CamelFolder *folder,
					CamelException *ex);
static CamelStore * _get_parent_store  (CamelFolder *folder,
					CamelException *ex);


static gboolean _has_message_number_capability (CamelFolder *folder);
static CamelMimeMessage *_get_message_by_number (CamelFolder *folder, 
						 gint number, 
						 CamelException *ex);
static gint _get_message_count        (CamelFolder *folder, 
				       CamelException *ex);


static gboolean _delete_messages (CamelFolder *folder, 
				  CamelException *ex);
static GList * _expunge         (CamelFolder *folder, 
			      CamelException *ex);
static void _append_message  (CamelFolder *folder, 
			      CamelMimeMessage *message, 
			      CamelException *ex);
static void _copy_message_to (CamelFolder *folder, 
			      CamelMimeMessage *message, 
			      CamelFolder *dest_folder, 
			      CamelException *ex);


static GList            *_get_uid_list       (CamelFolder *folder,
					      CamelException *ex);
static const gchar      *_get_message_uid    (CamelFolder *folder, 
					      CamelMimeMessage *message, 
					      CamelException *ex);
static CamelMimeMessage *_get_message_by_uid (CamelFolder *folder, 
					      const gchar *uid, 
					      CamelException *ex);




static void
camel_folder_class_init (CamelFolderClass *camel_folder_class)
{
	GtkObjectClass *gtk_object_class = GTK_OBJECT_CLASS (camel_folder_class);
	
	parent_class = gtk_type_class (gtk_object_get_type ());
	
	/* virtual method definition */
	camel_folder_class->init_with_store = _init_with_store;
	camel_folder_class->open = _open;
#ifdef FOLDER_ASYNC_TEST
	camel_folder_class->open_async = _open_async;
#endif
	camel_folder_class->close = _close;
#ifdef FOLDER_ASYNC_TEST
	camel_folder_class->close_async = _close_async;
#endif
	camel_folder_class->set_name = _set_name;
	camel_folder_class->get_name = _get_name;
	camel_folder_class->get_full_name = _get_full_name;
	camel_folder_class->can_hold_folders = _can_hold_folders;
	camel_folder_class->can_hold_messages = _can_hold_messages;
	camel_folder_class->exists = _exists;
	camel_folder_class->is_open = _is_open;
	camel_folder_class->get_subfolder = _get_subfolder;
	camel_folder_class->create = _create;
	camel_folder_class->delete = _delete;
	camel_folder_class->delete_messages = _delete_messages;
	camel_folder_class->get_parent_folder = _get_parent_folder;
	camel_folder_class->get_parent_store = _get_parent_store;
	camel_folder_class->get_mode = _get_mode;
	camel_folder_class->list_subfolders = _list_subfolders;
	camel_folder_class->expunge = _expunge;
	camel_folder_class->has_message_number_capability = _has_message_number_capability;
	camel_folder_class->get_message_by_number = _get_message_by_number;
	camel_folder_class->get_message_count = _get_message_count;
	camel_folder_class->append_message = _append_message;
	camel_folder_class->list_permanent_flags = _list_permanent_flags;
	camel_folder_class->copy_message_to = _copy_message_to;
	camel_folder_class->get_message_uid = _get_message_uid;
	camel_folder_class->get_message_by_uid = _get_message_by_uid;
	camel_folder_class->get_uid_list = _get_uid_list;

	/* virtual method overload */
	gtk_object_class->finalize = _finalize;
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


static void           
_finalize (GtkObject *object)
{
	CamelFolder *camel_folder = CAMEL_FOLDER (object);

	CAMEL_LOG_FULL_DEBUG ("Entering CamelFolder::finalize\n");

	g_free (camel_folder->name);
	g_free (camel_folder->full_name);
	g_free (camel_folder->permanent_flags);

	if (camel_folder->parent_store)
		gtk_object_unref (GTK_OBJECT (camel_folder->parent_store));

	GTK_OBJECT_CLASS (parent_class)->finalize (object);
	CAMEL_LOG_FULL_DEBUG ("Leaving CamelFolder::finalize\n");
}


/**
 * _init_with_store: init the folder by setting its parent store.
 * @folder: folder object to initialize
 * @parent_store: parent store object of the folder
 * 
 * 
 **/
static void 
_init_with_store (CamelFolder *folder, CamelStore *parent_store, CamelException *ex)
{
	
	g_assert (folder != NULL);
	g_assert (parent_store != NULL);
	g_assert (folder->parent_store == NULL);
	
	folder->parent_store = parent_store;
	gtk_object_ref (GTK_OBJECT (parent_store));
	
	folder->open_mode = FOLDER_OPEN_UNKNOWN;
	folder->open_state = FOLDER_CLOSE;
	folder->name = NULL;
	folder->full_name = NULL;
}





static void
_open (CamelFolder *folder, 
       CamelFolderOpenMode mode, 
       CamelException *ex)
{
  	folder->open_state = FOLDER_OPEN;
  	folder->open_mode = mode;
}




/**
 * camel_folder_open: Open a folder
 * @folder: The folder object
 * @mode: open mode (R/W/RW ?)
 * @ex: exception object
 *
 * Open a folder in a given mode.
 * 
 **/
void 
camel_folder_open (CamelFolder *folder, 
		   CamelFolderOpenMode mode, 
		   CamelException *ex)
{	
	g_assert (folder != NULL);
	CF_CLASS (folder)->open (folder, mode, ex);
}




#ifdef FOLDER_ASYNC_TEST

static void
_open_async (CamelFolder *folder, 
	     CamelFolderOpenMode mode, 
	     CamelFolderAsyncCallback callback, 
	     gpointer user_data, 
	     CamelException *ex)
{
	CAMEL_LOG_WARNING ("Calling CamelFolder::open_async directly. "
			   "Should be overloaded\n");
}




/**
 * camel_folder_open: Open a folder
 * @folder: The folder object
 * @mode: open mode (R/W/RW ?)
 * @callback: function to call when the operation is over
 * @user_data: data to pass to the callback 
 * @ex: exception object
 *
 * Open a folder in a given mode. When the operation is over
 * the callback is called and the client program can determine
 * if the operation suceeded by examining the exception. 
 * 
 **/
void 
camel_folder_open_async (CamelFolder *folder, 
			 CamelFolderOpenMode mode, 
			 CamelFolderAsyncCallback callback, 
			 gpointer user_data, 
			 CamelException *ex)
{	
	g_assert (folder != NULL);
	CF_CLASS (folder)->open_async (folder, mode, callback, user_data, ex);
}


#endif /* FOLDER_ASYNC_TEST */



static void
_close (CamelFolder *folder, 
	gboolean expunge, 
	CamelException *ex)
{	
	folder->open_state = FOLDER_CLOSE;
}

/**
 * camel_folder_close: Close a folder.
 * @folder: The folder object
 * @expunge: if TRUE, the flagged message are deleted.
 * @ex: exception object
 *
 * Put a folder in its closed state, and possibly 
 * expunge the flagged messages. 
 * 
 **/
void 
camel_folder_close (CamelFolder *folder, 
		    gboolean expunge, 
		    CamelException *ex)
{
	g_assert (folder != NULL);
	CF_CLASS (folder)->close (folder, expunge, ex);
}




#ifdef FOLDER_ASYNC_TEST


static void
_close_async (CamelFolder *folder, 
	      gboolean expunge, 
	      CamelFolderAsyncCallback callback, 
	      gpointer user_data, 
	      CamelException *ex)
{	
	CAMEL_LOG_WARNING ("Calling CamelFolder::close_async directly. "
			   "Should be overloaded\n");
}

/**
 * camel_folder_close_async: Close a folder.
 * @folder: The folder object
 * @expunge: if TRUE, the flagged message are deleted.
 * @callback: function to call when the operation is over
 * @user_data: data to pass to the callback 
 * @ex: exception object
 *
 * Put a folder in its closed state, and possibly 
 * expunge the flagged messages. The callback is called 
 * when the operation is over and the client program can determine
 * if the operation suceeded by examining the exception. 
 * 
 **/
void 
camel_folder_close_async (CamelFolder *folder, 
			  gboolean expunge, 
			  CamelFolderAsyncCallback callback, 
			  gpointer user_data, 
			  CamelException *ex)
{
	g_assert (folder != NULL);
	CF_CLASS (folder)->close_async (folder, expunge, callback,
					user_data, ex);
}


#endif


static void
_set_name (CamelFolder *folder, 
	   const gchar *name, 
	   CamelException *ex)
{
	gchar separator;
	gchar *full_name;
	const gchar *parent_full_name;
	
	g_assert (folder->parent_store != NULL);
	g_assert (name != NULL);
	g_assert (!camel_folder_is_open (folder));
			
	/* if the folder already has a name, free it */	
	g_free (folder->name);
	g_free (folder->full_name);
	
	/* set those fields to NULL now, so that if an 
	   exception occurs, they will be set anyway */
	folder->name = NULL;
	folder->full_name = NULL;

	CAMEL_LOG_FULL_DEBUG ("CamelFolder::set_name, folder name is %s\n",
			      name);

	separator = camel_store_get_separator (folder->parent_store, ex);
	if (camel_exception_get_id (ex)) return;

	if (folder->parent_folder) {
		parent_full_name =
			camel_folder_get_full_name (folder->parent_folder, ex);
		if (camel_exception_get_id (ex)) return;
		
		full_name = g_strdup_printf ("%s%c%s", parent_full_name,
					     separator, name);		
	} else {
		full_name = g_strdup_printf ("%c%s", separator, name);
	}

	CAMEL_LOG_FULL_DEBUG ("CamelFolder::set_name, folder full name "
			      "set to %s\n", full_name);
	folder->name = g_strdup (name);
	folder->full_name = full_name;
	
}


/**
 * camel_folder_set_name:set the (short) name of the folder
 * @folder: folder
 * @name: new name of the folder
 * @ex: exception object
 **/
void
camel_folder_set_name (CamelFolder *folder, const gchar *name,
		       CamelException *ex)
{
	g_assert (folder != NULL);
	CF_CLASS (folder)->set_name (folder, name, ex);
}



static const gchar *
_get_name (CamelFolder *folder, CamelException *ex)
{
	return folder->name;
}


/**
 * camel_folder_get_name: get the (short) name of the folder
 * @folder: 
 * 
 * get the name of the folder. The fully qualified name
 * can be obtained with the get_full_ame method (not implemented)
 *
 * Return value: name of the folder
 **/
const gchar *
camel_folder_get_name (CamelFolder *folder, CamelException *ex)
{
	g_assert (folder != NULL);
	return CF_CLASS (folder)->get_name (folder, ex);
}



static const gchar *
_get_full_name (CamelFolder *folder, CamelException *ex)
{
	return folder->full_name;
}

/**
 * camel_folder_get_full_name:get the (full) name of the folder
 * @folder: folder to get the name 
 * 
 * get the name of the folder. 
 * 
 * Return value: full name of the folder
 **/
const gchar *
camel_folder_get_full_name (CamelFolder *folder, CamelException *ex)
{
	g_assert (folder != NULL);
	return CF_CLASS (folder)->get_full_name (folder, ex);
}


/**
 * _can_hold_folders: tests if the folder can contain other folders
 * @folder: The folder object 
 * 
 * Tests if a folder can contain other folder 
 * (as for example MH folders)
 * 
 * Return value: 
 **/
static gboolean
_can_hold_folders (CamelFolder *folder)
{
	return folder->can_hold_folders;
}




/**
 * _can_hold_messages: tests if the folder can contain messages
 * @folder: The folder object
 * 
 * Tests if a folder object can contain messages. 
 * In the case it can not, it most surely can only 
 * contain folders (rare).
 * 
 * Return value: true if it can contain messages false otherwise
 **/
static gboolean
_can_hold_messages (CamelFolder *folder)
{
	return folder->can_hold_messages;
}



static gboolean
_exists (CamelFolder *folder, CamelException *ex)
{
	return FALSE;
}


/**
 * _exists: tests if the folder object exists in its parent store.
 * @folder: folder object
 * 
 * Test if a folder exists on a store. A folder can be 
 * created without physically on a store. In that case, 
 * use CamelFolder::create to create it 
 * 
 * Return value: true if the folder exists on the store false otherwise 
 **/
gboolean
camel_folder_exists (CamelFolder *folder, CamelException *ex)
{
	g_assert (folder != NULL);
	return CF_CLASS (folder)->exists (folder, ex);
}



/**
 * _is_open: test if the folder is open 
 * @folder: The folder object
 * 
 * Tests if a folder is open. If not open it can be opened 
 * CamelFolder::open
 * 
 * Return value: true if the folder exists, false otherwise
 **/
static gboolean
_is_open (CamelFolder *folder)
{
	return folder->open_state == FOLDER_OPEN;
} 


/**
 * _is_open: test if the folder is open 
 * @folder: The folder object
 * 
 * Tests if a folder is open. If not open it can be opened 
 * CamelFolder::open
 * 
 * Return value: true if the folder exists, false otherwise
 **/
gboolean
camel_folder_is_open (CamelFolder *folder)
{
	g_assert (folder != NULL);
	return CF_CLASS (folder)->is_open (folder);
} 


static CamelFolder *
_get_subfolder (CamelFolder *folder, 
		const gchar *folder_name, 
		CamelException *ex)
{
	CamelFolder *new_folder;
	gchar *full_name;
	const gchar *current_folder_full_name;
	gchar separator;
	
	g_assert (folder->parent_store != NULL);
	
	current_folder_full_name = camel_folder_get_full_name (folder, ex);
	if (camel_exception_get_id (ex)) return NULL;


	separator = camel_store_get_separator (folder->parent_store, ex);
	full_name = g_strdup_printf ("%s%d%s", current_folder_full_name, separator, folder_name);
	
	new_folder = camel_store_get_folder (folder->parent_store, full_name, ex);
	return new_folder;
}



/**
 * camel_folder_get_subfolder: return the (sub)folder object that is specified
 * @folder: the folder
 * @folder_name: subfolder path
 * 
 * This method returns a folder objects. This folder
 * is necessarily a subfolder of the current folder. 
 * It is an error to ask a folder begining with the 
 * folder separator character.  
 * 
 * Return value: Required folder. NULL if the subfolder object  could not be obtained
 **/
CamelFolder *
camel_folder_get_subfolder (CamelFolder *folder, gchar *folder_name, CamelException *ex)
{
	g_assert (folder != NULL);
	g_assert (folder_name != NULL);
	g_assert (camel_folder_is_open (folder));

	return CF_CLASS (folder)->get_subfolder (folder, folder_name, ex);
}




/**
 * _create: creates a folder on its store
 * @folder: a CamelFolder object.
 * 
 * this routine handles the recursion mechanism.
 * Children classes have to implement the actual
 * creation mechanism. They must call this method
 * before physically creating the folder in order
 * to be sure the parent folder exists.
 * Calling this routine on an existing folder is
 * not an error, and returns %TRUE.
 * 
 * Return value: %TRUE if the folder exists, %FALSE otherwise 
 **/
static gboolean
_create (CamelFolder *folder, CamelException *ex)
{
	gchar *prefix;
	gchar dich_result;
	CamelFolder *parent;
	gchar sep;
	
	g_assert (folder->parent_store != NULL);
	g_assert (folder->name != NULL);
	
	/* if the folder already exists on the 
	   store, do nothing and return true */
	if (CF_CLASS (folder)->exists (folder, ex))
		return TRUE;
	
	
	sep = camel_store_get_separator (folder->parent_store, ex);	
	if (folder->parent_folder) {
		camel_folder_create (folder->parent_folder, ex);
		if (camel_exception_get_id (ex)) return FALSE;
	}
	else {   
		if (folder->full_name) {
			dich_result = string_dichotomy (
							folder->full_name, sep, &prefix, NULL,
							STRING_DICHOTOMY_STRIP_TRAILING | STRING_DICHOTOMY_RIGHT_DIR);
			if (dich_result!='o') {
				if (prefix == NULL) {
					/* separator is the first caracter, no folder above */
					 return TRUE;
				}
			} else {
				parent = camel_store_get_folder (folder->parent_store, prefix, ex);
				camel_folder_create (parent, ex);
				if (camel_exception_get_id (ex)) return FALSE;
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
camel_folder_create (CamelFolder *folder, CamelException *ex)
{
	g_assert (folder != NULL);
	g_assert (!camel_folder_is_open (folder));

	return CF_CLASS (folder)->create (folder, ex);
}





/**
 * _delete: delete folder 
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
_delete (CamelFolder *folder, gboolean recurse, CamelException *ex)
{
	GList *subfolders=NULL;
	GList *sf;
	gboolean ok;
	
	/* delete all messages in the folder */
	CF_CLASS (folder)->delete_messages (folder, ex);
	if (camel_exception_get_id (ex)) return FALSE;

	subfolders = CF_CLASS (folder)->list_subfolders (folder, ex); 
	if (camel_exception_get_id (ex)) {
		if (subfolders) g_list_free (subfolders);
		return FALSE;
	}
	
        ok = TRUE;
	if (recurse) { /* delete subfolders */
		if (subfolders) {
			sf = subfolders;
			do {
				CF_CLASS (sf->data)->delete (CAMEL_FOLDER (sf->data), TRUE, ex);
				if (camel_exception_get_id (ex)) ok = FALSE;
			} while (ok && (sf = sf->next));
		}
	} else if (subfolders) {
		camel_exception_set (ex, CAMEL_EXCEPTION_FOLDER_NON_EMPTY,
				     "folder has subfolders");
		ok = FALSE;
	}
	
	if (subfolders) g_list_free (subfolders);

	return ok;
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
gboolean 
camel_folder_delete (CamelFolder *folder, gboolean recurse, CamelException *ex)
{
	g_assert (folder != NULL);
	g_assert (!camel_folder_is_open (folder));

	return CF_CLASS (folder)->delete (folder, recurse, ex);
}





/**
 * _delete_messages: delete all messages in the folder
 * @folder: 
 * 
 * 
 * 
 * Return value: 
 **/
static gboolean 
_delete_messages (CamelFolder *folder, CamelException *ex)
{
	CAMEL_LOG_WARNING ("Calling CamelFolder::delete_messages directly. "
			   "Should be overloaded\n");
	return FALSE;
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
camel_folder_delete_messages (CamelFolder *folder, CamelException *ex)
{
	g_assert (folder != NULL);
	g_assert (!camel_folder_is_open (folder));

	return CF_CLASS (folder)->delete_messages (folder, ex);
}






/**
 * _get_parent_folder: return parent folder
 * @folder: folder to get the parent
 * 
 * 
 * 
 * Return value: 
 **/
static CamelFolder *
_get_parent_folder (CamelFolder *folder, CamelException *ex)
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
camel_folder_get_parent_folder (CamelFolder *folder, CamelException *ex)
{
	g_assert (folder != NULL);
	return CF_CLASS (folder)->get_parent_folder (folder, ex);
}


/**
 * _get_parent_store: return parent store
 * @folder: folder to get the parent
 * 
 * 
 * 
 * Return value: 
 **/
static CamelStore *
_get_parent_store (CamelFolder *folder, CamelException *ex)
{
	return folder->parent_store;
}


/**
 * camel_folder_get_parent_store: return parent store
 * @folder: folder to get the parent
 * 
 * Return the parent store of a folder
 * 
 * Return value: the parent store. 
 **/
CamelStore *
camel_folder_get_parent_store (CamelFolder *folder, CamelException *ex)
{
	g_assert (folder != NULL);
	return CF_CLASS (folder)->get_parent_store (folder, ex);
}




static CamelFolderOpenMode
_get_mode (CamelFolder *folder, CamelException *ex)
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
camel_folder_get_mode (CamelFolder *folder, CamelException *ex)
{
	g_assert (folder != NULL);
	return CF_CLASS (folder)->get_mode (folder, ex);
}




static GList *
_list_subfolders (CamelFolder *folder, CamelException *ex)
{
	CAMEL_LOG_WARNING ("Calling CamelFolder::list_subfolders directly. "
			   "Should be overloaded\n");
	return NULL;
}


/**
 * camel_folder_list_subfolders: list subfolders in a folder
 * @folder: the folder
 * 
 * List subfolders in a folder. 
 * 
 * Return value: list of subfolders
 **/
GList *
camel_folder_list_subfolders (CamelFolder *folder, CamelException *ex)
{
	g_assert (folder != NULL);
	g_assert (camel_folder_is_open (folder));

	return CF_CLASS (folder)->list_subfolders (folder, ex);
}




static GList *
_expunge (CamelFolder *folder, CamelException *ex)
{
	CAMEL_LOG_WARNING ("Calling CamelFolder::expunge directly. "
			   "Should be overloaded\n");
	return NULL;
}


/**
 * camel_folder_expunge: physically delete messages marked as "DELETED"
 * @folder: the folder
 * 
 * Delete messages which have been marked as  "DELETED"
 * 
 * Return value: list of expunged messages 
 **/
GList *
camel_folder_expunge (CamelFolder *folder, CamelException *ex)
{
	g_assert (folder != NULL);
	g_assert (!camel_folder_is_open (folder));

	return CF_CLASS (folder)->expunge (folder, ex);
}


static gboolean 
_has_message_number_capability (CamelFolder *folder)
{
	CAMEL_LOG_WARNING ("Calling CamelFolder::has_message_number_capability  directly. "
			   "Should be overloaded\n");
	return FALSE;

}


/**
 * camel_folder_has_message_number_capability: tests if the message can be numbered within the folder
 * @folder: folder to test
 * 
 * Test if the message in this folder can be
 * obtained via the get_by_number method. 
 * Usually, when the folder has the UID 
 * capability, messages should be referred to
 * by their UID rather than by their number
 * as the UID is more reliable. 
 * 
 * Return value: TRUE if the folder supports message numbering, FALSE otherwise.
 **/
gboolean 
camel_folder_has_message_number_capability (CamelFolder *folder)
{	
	g_assert (folder != NULL);
	return CF_CLASS (folder)->has_message_number_capability (folder);
}




static CamelMimeMessage *
_get_message_by_number (CamelFolder *folder, gint number, CamelException *ex)
{
	CAMEL_LOG_WARNING ("Calling CamelFolder::get_message_by_number "
			   "directly. Should be overloaded\n");
	return NULL;
}




/**
 * camel_folder_get_message_by_number: return the message corresponding to that number in the folder
 * @folder: a CamelFolder object
 * @number: the number of the message within the folder.
 * 
 * Return the message corresponding to that number within the folder.
 * 
 * Return value: A pointer on the corresponding message or NULL if no corresponding message exists
 **/
CamelMimeMessage *
camel_folder_get_message_by_number (CamelFolder *folder, gint number, CamelException *ex)
{
	g_assert (folder != NULL);
	g_assert (camel_folder_is_open (folder));

	return CF_CLASS (folder)->get_message_by_number (folder, number, ex);
}


static gint
_get_message_count (CamelFolder *folder, CamelException *ex)
{
	CAMEL_LOG_WARNING ("Calling CamelFolder::get_message_count directly. "
			   "Should be overloaded\n");
	return -1;
}



/**
 * camel_folder_get_message_count: get the number of messages in the folder
 * @folder: A CamelFolder object
 * 
 * Returns the number of messages in the folder.
 * 
 * Return value: the number of messages or -1 if unknown.
 **/
gint
camel_folder_get_message_count (CamelFolder *folder, CamelException *ex)
{
	g_assert (folder != NULL);
	g_assert (camel_folder_is_open (folder));

	return CF_CLASS (folder)->get_message_count (folder, ex);
}


static void
_append_message (CamelFolder *folder, CamelMimeMessage *message,
		 CamelException *ex)
{
	CAMEL_LOG_WARNING ("Calling CamelFolder::append_message directly. "
			   "Should be overloaded\n");
	return;

}


/**
 * camel_folder_append_message: add a message to a folder
 * @folder: folder object to add the message to
 * @message: message object
 * @ex: exception object
 * 
 * Add a message to a folder.
 * 
 **/
void 
camel_folder_append_message (CamelFolder *folder, 
			     CamelMimeMessage *message, 
			     CamelException *ex)
{	
	g_assert (folder != NULL);
	g_assert (camel_folder_is_open (folder));

	CF_CLASS (folder)->append_message (folder, message, ex);
}


static const GList *
_list_permanent_flags (CamelFolder *folder, CamelException *ex)
{
	return folder->permanent_flags;
}


const GList *
camel_folder_list_permanent_flags (CamelFolder *folder, CamelException *ex)
{
	g_assert (folder != NULL);
	return CF_CLASS (folder)->list_permanent_flags (folder, ex);
}




static void
_copy_message_to (CamelFolder *folder, CamelMimeMessage *message, CamelFolder *dest_folder, CamelException *ex)
{
	camel_folder_append_message (dest_folder, message, ex);
}


void
camel_folder_copy_message_to (CamelFolder *folder, 
			      CamelMimeMessage *message, 
			      CamelFolder *dest_folder, 
			      CamelException *ex)
{
	g_assert (folder != NULL);
	g_assert (camel_folder_is_open (folder));

	CF_CLASS (folder)->copy_message_to (folder, message, dest_folder, ex);;
}





/* summary stuff */

gboolean
camel_folder_has_summary_capability (CamelFolder *folder)
{
	g_assert (folder != NULL);
	return folder->has_summary_capability;
}


/**
 * camel_folder_get_summary: return the summary of a folder
 * @folder: folder object
 * @ex: exception object
 * 
 * Return a CamelFolderSummary object from 
 * which the main informations about a folder
 * can be retrieved.
 * 
 * Return value: the folder summary object.
 **/
CamelFolderSummary *
camel_folder_get_summary (CamelFolder *folder, 
			  CamelException *ex)
{
	g_assert (folder != NULL);
	g_assert (camel_folder_is_open (folder));

	return folder->summary;
}




/* UIDs stuff */

/**
 * camel_folder_has_uid_capability: detect if the folder support UIDs
 * @folder: Folder object
 * 
 * Detects if a folder supports UID operations, that is
 * reference messages by a Unique IDentifier instead
 * of by message number.  
 * 
 * Return value: TRUE if the folder supports UIDs 
 **/
gboolean
camel_folder_has_uid_capability (CamelFolder *folder)
{
	g_assert (folder != NULL);
	return folder->has_uid_capability;
}



static const gchar *
_get_message_uid (CamelFolder *folder, CamelMimeMessage *message, CamelException *ex)
{
	CAMEL_LOG_WARNING ("Calling CamelFolder::get_message_uid directly. "
			   "Should be overloaded\n");
	return NULL;
}

/**
 * camel_folder_get_message_uid: get the UID of a message in a folder
 * @folder: Folder in which the UID must refer to
 * @message: Message object 
 * 
 * Return the UID of a message relatively to a folder.
 * A message can have different UID, each one corresponding
 * to a different folder, if the message is referenced in
 * several folders. 
 * 
 * Return value: The UID of the message in the folder
 **/
const gchar * 
camel_folder_get_message_uid (CamelFolder *folder, CamelMimeMessage *message, CamelException *ex)
{
	g_assert (folder != NULL);
	g_assert (folder->has_uid_capability);
	g_assert (camel_folder_is_open (folder));

	return CF_CLASS (folder)->get_message_uid (folder, message, ex);
}



/* the next two func are left there temporarily */
#if 0

static const gchar *
_get_message_uid_by_number (CamelFolder *folder, gint message_number, CamelException *ex)
{
	CAMEL_LOG_WARNING ("Calling CamelFolder::get_message_uid_by_number "
			   "directly. Should be overloaded\n");
	return NULL;
}


const gchar * 
camel_folder_get_message_uid_by_number (CamelFolder *folder, gint message_number, CamelException *ex);

/**
 * camel_folder_get_message_uid_by_number: get the UID corresponding to a message number
 * @folder: Folder object
 * @message_number: Message number
 * 
 * get the UID corresponding to a message number. 
 * Use of this routine should be avoiding, as on 
 * folders supporting UIDs, message numbers should
 * not been used.
 * 
 * Return value: 
 **/
const gchar * 
camel_folder_get_message_uid_by_number (CamelFolder *folder, gint message_number, CamelException *ex)
{
	g_assert (folder != NULL);

	/*  if (!folder->has_uid_capability) return NULL; */
	/*  return CF_CLASS (folder)->get_message_uid_by_number (folder, message_number, ex); */
	
	return NULL;
}
#endif /* 0 */

static CamelMimeMessage *
_get_message_by_uid (CamelFolder *folder, const gchar *uid, CamelException *ex)
{
	CAMEL_LOG_WARNING ("Calling CamelFolder::get_message_by_uid directly. "
			   "Should be overloaded\n");
	return NULL;
}


/**
 * camel_folder_get_message_by_uid: Get a message by its UID in a folder
 * @folder: the folder object
 * @uid: the UID
 * 
 * Get a message from its UID in the folder. Messages 
 * are cached within a folder, that is, asking twice
 * for the same UID returns the same message object.
 * 
 * Return value: Message corresponding to the UID
 **/
CamelMimeMessage *
camel_folder_get_message_by_uid  (CamelFolder *folder, const gchar *uid, CamelException *ex)
{
	g_assert (folder != NULL);
	g_assert (folder->has_uid_capability);
	g_assert (camel_folder_is_open (folder));

	return CF_CLASS (folder)->get_message_by_uid (folder, uid, ex);
}

static GList *
_get_uid_list  (CamelFolder *folder, CamelException *ex)
{
	CAMEL_LOG_WARNING ("Calling CamelFolder::get_uid_list directly. "
			   "Should be overloaded\n");
	return NULL;
}

/**
 * camel_folder_get_uid_list: get the list of UID in a folder
 * @folder: folder object
 * 
 * get the list of UID available in a folder. This
 * routine is usefull to know what messages are
 * available when the folder does not support
 * summaries. The UIDs in the list must not be freed,
 * the folder object caches them.
 * 
 * Return value: Glist of UID correspondind to the messages available in the folder.
 **/
GList *
camel_folder_get_uid_list  (CamelFolder *folder, CamelException *ex)
{
	g_assert (folder != NULL);
	g_assert (folder->has_uid_capability);
	g_assert (camel_folder_is_open (folder));

	return CF_CLASS (folder)->get_uid_list (folder, ex);
}

/**
 * camel_folder_has_search_capability:
 * @folder: Folder object
 * 
 * Checks if a folder supports searching.
 * 
 * Return value: TRUE if the folder supports UIDs 
 **/
gboolean
camel_folder_has_search_capability (CamelFolder *folder)
{
	g_assert (folder != NULL);
	return folder->has_search_capability;
}

GList *camel_folder_search_by_expression  (CamelFolder *folder,
					   const char *expression,
					   CamelException *ex)
{
	g_assert (folder != NULL);
	g_assert (folder->has_search_capability);

	return CF_CLASS (folder)->search_by_expression (folder, expression, ex);
}

/* **** */
