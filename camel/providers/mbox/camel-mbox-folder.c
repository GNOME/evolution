/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* camel-mbox-folder.c : Abstract class for an email folder */

/* 
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


#include <config.h> 

#include <sys/types.h>
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <fcntl.h>

#include "camel-mbox-folder.h"
#include "camel-mbox-store.h"
#include "string-utils.h"
#include "camel-log.h"
#include "camel-stream-buffered-fs.h"
#include "camel-folder-summary.h"
#include "camel-mbox-summary.h"
#include "camel-mbox-parser.h"
#include "camel-mbox-utils.h"
#include "md5-utils.h"
#include "gmime-utils.h"

#include "camel-exception.h"

static CamelFolderClass *parent_class=NULL;

/* Returns the class for a CamelMboxFolder */
#define CMBOXF_CLASS(so) CAMEL_MBOX_FOLDER_CLASS (GTK_OBJECT(so)->klass)
#define CF_CLASS(so) CAMEL_FOLDER_CLASS (GTK_OBJECT(so)->klass)
#define CMBOXS_CLASS(so) CAMEL_STORE_CLASS (GTK_OBJECT(so)->klass)


static void _init_with_store (CamelFolder *folder, CamelStore *parent_store, CamelException *ex);
static void _set_name(CamelFolder *folder, const gchar *name, CamelException *ex);


static void _open (CamelFolder *folder, CamelFolderOpenMode mode, CamelException *ex);
static void _close (CamelFolder *folder, gboolean expunge, CamelException *ex);
static gboolean _exists (CamelFolder *folder, CamelException *ex);
static gboolean _create(CamelFolder *folder, CamelException *ex);
static gboolean _delete (CamelFolder *folder, gboolean recurse, CamelException *ex);
static gboolean _delete_messages (CamelFolder *folder, CamelException *ex);
static GList *_list_subfolders (CamelFolder *folder, CamelException *ex);
/* static CamelMimeMessage *_get_message_by_number (CamelFolder *folder, gint number, CamelException *ex);*/
static gint _get_message_count (CamelFolder *folder, CamelException *ex);
static gint _append_message (CamelFolder *folder, CamelMimeMessage *message, CamelException *ex);
static GList *_get_uid_list  (CamelFolder *folder, CamelException *ex);
#if 0
static CamelMimeMessage *_get_message_by_uid (CamelFolder *folder, const gchar *uid, CamelException *ex);
static void _expunge (CamelFolder *folder, CamelException *ex);
static void _copy_message_to (CamelFolder *folder, CamelMimeMessage *message, CamelFolder *dest_folder, CamelException *ex);
static const gchar *_get_message_uid (CamelFolder *folder, CamelMimeMessage *message, CamelException *ex);
#endif

static void _finalize (GtkObject *object);

static void
camel_mbox_folder_class_init (CamelMboxFolderClass *camel_mbox_folder_class)
{
	CamelFolderClass *camel_folder_class = CAMEL_FOLDER_CLASS (camel_mbox_folder_class);
	GtkObjectClass *gtk_object_class = GTK_OBJECT_CLASS (camel_folder_class);

	parent_class = gtk_type_class (camel_folder_get_type ());
		
	/* virtual method definition */

	/* virtual method overload */
	camel_folder_class->init_with_store = _init_with_store;
	camel_folder_class->set_name = _set_name;
	camel_folder_class->open = _open;
	camel_folder_class->close = _close;
	camel_folder_class->exists = _exists;
	camel_folder_class->create = _create;
	camel_folder_class->delete = _delete;
	camel_folder_class->delete_messages = _delete_messages;
	camel_folder_class->list_subfolders = _list_subfolders;
	/* camel_folder_class->get_message_by_number = _get_message_by_number; */
	camel_folder_class->get_message_count = _get_message_count;
	camel_folder_class->append_message = _append_message;
	camel_folder_class->get_uid_list = _get_uid_list;
#if 0
	camel_folder_class->expunge = _expunge;
	camel_folder_class->copy_message_to = _copy_message_to;
	camel_folder_class->get_message_uid = _get_message_uid;
	camel_folder_class->get_message_by_uid = _get_message_by_uid;
#endif
	gtk_object_class->finalize = _finalize;
	
}



static void           
_finalize (GtkObject *object)
{
	CamelMboxFolder *mbox_folder = CAMEL_MBOX_FOLDER (object);

	CAMEL_LOG_FULL_DEBUG ("Entering CamelFolder::finalize\n");

	
	g_free (mbox_folder->folder_file_path);
	g_free (mbox_folder->folder_dir_path);

	GTK_OBJECT_CLASS (parent_class)->finalize (object);
	CAMEL_LOG_FULL_DEBUG ("Leaving CamelFolder::finalize\n");
}





GtkType
camel_mbox_folder_get_type (void)
{
	static GtkType camel_mbox_folder_type = 0;
	
	if (!camel_mbox_folder_type)	{
		GtkTypeInfo camel_mbox_folder_info =	
		{
			"CamelMboxFolder",
			sizeof (CamelMboxFolder),
			sizeof (CamelMboxFolderClass),
			(GtkClassInitFunc) camel_mbox_folder_class_init,
			(GtkObjectInitFunc) NULL,
				/* reserved_1 */ NULL,
				/* reserved_2 */ NULL,
			(GtkClassInitFunc) NULL,
		};
		
		camel_mbox_folder_type = gtk_type_unique (CAMEL_FOLDER_TYPE, &camel_mbox_folder_info);
	}
	
	return camel_mbox_folder_type;
}






static void 
_init_with_store (CamelFolder *folder, CamelStore *parent_store, CamelException *ex)
{
	CamelMboxFolder *mbox_folder = CAMEL_MBOX_FOLDER (folder);


	CAMEL_LOG_FULL_DEBUG ("Entering CamelMhFolder::init_with_store\n");

	/* call parent method */
	parent_class->init_with_store (folder, parent_store, ex);
	if (camel_exception_get_id (ex)) return;

	/* we assume that the parent init_with_store 
	   method checks for the existance of @folder */
	   
	folder->can_hold_messages = TRUE;
	folder->can_hold_folders = TRUE;
	folder->has_summary_capability = TRUE;
	folder->has_uid_capability = TRUE;
 	folder->summary = NULL;

	mbox_folder->folder_file_path = NULL;
	mbox_folder->summary_file_path = NULL;
	mbox_folder->folder_dir_path = NULL;
	mbox_folder->summary = NULL;
	mbox_folder->uid_array = NULL;
	
	CAMEL_LOG_FULL_DEBUG ("Leaving CamelMhFolder::init_with_store\n");
}



/* internal method used to : 
   - test for the existence of a summary file 
   - test the sync between the summary and the mbox file
   - load the summary or create it if necessary 
*/ 
static void
_check_get_or_maybe_generate_summary_file (CamelMboxFolder *mbox_folder, CamelException *ex)
{
	GArray *message_info_array;
	gboolean summary_file_exists;
	gboolean summary_file_is_sync;
	GArray *mbox_summary_info;
	gint mbox_file_fd;
	guint32 next_uid;
	guint32 file_size;

	/* test for the existence of the summary file */
	summary_file_exists = (access (mbox_folder->summary_file_path, F_OK) == 0);

	/* if the summary file exists, test if the 
	   md5 of the mbox file is still in sync
	   with the one we had computed the last time
	   we saved the summary file */
	if (summary_file_exists) {
		
		summary_file_is_sync = 
			camel_mbox_check_summary_sync (mbox_folder->summary_file_path,
						       mbox_folder->folder_file_path,
						       ex);
		if (camel_exception_get_id (ex)) return;
	}


	/* in the case where the summary does not exist 
	   or is not in sync with the mbox file
	   regenerate it */
	if ( !(summary_file_exists && summary_file_is_sync)) {
		
		/* parse the mbox folder and get some
		   information about the messages */

		mbox_file_fd = open (mbox_folder->folder_file_path, O_RDONLY);
		message_info_array = camel_mbox_parse_file (mbox_file_fd, 
							    "From - ", 
							    0,
							    &file_size,
							    &next_uid,
							    TRUE,
							    NULL,
							    0,
							    ex); 
		
		close (mbox_file_fd);
		if (camel_exception_get_id (ex)) { 
			return;
		}

		
		next_uid = camel_mbox_write_xev (mbox_folder->folder_file_path, 
						 message_info_array, &file_size, next_uid, ex);

		if (camel_exception_get_id (ex)) { 
			/* ** FIXME : free the preparsed information */
			return;
		}
						
		mbox_summary_info =
			parsed_information_to_mbox_summary (message_info_array);
		
		/* **FIXME : Free the parsed information structure */

		/* allocate an internal summary object */
		mbox_folder->summary = g_new (CamelMboxSummary, 1);
		
		/* generate the folder md5 signature */
		md5_get_digest_from_file (mbox_folder->folder_file_path, mbox_folder->summary->md5_digest);

		/* store the number of messages as well as the summary array */
		mbox_folder->summary->nb_message = mbox_summary_info->len;		
		mbox_folder->summary->next_uid = next_uid;		
		mbox_folder->summary->mbox_file_size = file_size;		
		mbox_folder->summary->message_info = mbox_summary_info;
		
	} else {
		/* every thing seems ok, just read the summary file from disk */
		mbox_folder->summary = camel_mbox_load_summary (mbox_folder->summary_file_path, ex);
	}
}



static void
_open (CamelFolder *folder, CamelFolderOpenMode mode, CamelException *ex)
{
	CamelMboxFolder *mbox_folder = CAMEL_MBOX_FOLDER (folder);
	//struct dirent *dir_entry;
	//struct stat stat_buf;


	if (folder->open_state == FOLDER_OPEN) {
		camel_exception_set (ex, 
				     CAMEL_EXCEPTION_FOLDER_INVALID_STATE,
				     "folder is already open");
		return;
	}
	
	
	
	/* get (or create) uid list */
	//if (!(mbox_load_uid_list (mbox_folder) > 0))
	//	mbox_generate_uid_list (mbox_folder);
	
	_check_get_or_maybe_generate_summary_file (mbox_folder, ex);
}


static void
_close (CamelFolder *folder, gboolean expunge, CamelException *ex)
{
	CamelMboxFolder *mbox_folder = CAMEL_MBOX_FOLDER (folder);


	/* call parent implementation */
	parent_class->close (folder, expunge, ex);
	
	/* save the folder summary on disc */
	camel_mbox_save_summary (mbox_folder->summary, mbox_folder->summary_file_path, ex);
}




static void
_set_name (CamelFolder *folder, const gchar *name, CamelException *ex)
{
	CamelMboxFolder *mbox_folder = CAMEL_MBOX_FOLDER (folder);
	const gchar *root_dir_path;
	//gchar *full_name;
	//const gchar *parent_full_name;
	gchar separator;
	
	CAMEL_LOG_FULL_DEBUG ("Entering CamelMboxFolder::set_name\n");

	/* call default implementation */
	parent_class->set_name (folder, name, ex);
	if (camel_exception_get_id (ex)) return;

	g_free (mbox_folder->folder_file_path);
	g_free (mbox_folder->folder_dir_path);

	separator = camel_store_get_separator (folder->parent_store, ex);
	root_dir_path = camel_mbox_store_get_toplevel_dir (CAMEL_MBOX_STORE(folder->parent_store), ex);

	CAMEL_LOG_FULL_DEBUG ("CamelMboxFolder::set_name full_name is %s\n", folder->full_name);
	CAMEL_LOG_FULL_DEBUG ("CamelMboxFolder::set_name root_dir_path is %s\n", root_dir_path);
	CAMEL_LOG_FULL_DEBUG ("CamelMboxFolder::separator is %c\n", separator);

	mbox_folder->folder_file_path = g_strdup_printf ("%s%c%s", root_dir_path, separator, folder->full_name);
	mbox_folder->summary_file_path = g_strdup_printf ("%s%c%s-ev-summary", root_dir_path, separator, folder->full_name);
	mbox_folder->folder_dir_path = g_strdup_printf ("%s%c%s.sdb", root_dir_path, separator, folder->full_name);
	
	
	CAMEL_LOG_FULL_DEBUG ("CamelMboxFolder::set_name mbox_folder->folder_file_path is %s\n", 
			      mbox_folder->folder_file_path);
	CAMEL_LOG_FULL_DEBUG ("CamelMboxFolder::set_name mbox_folder->folder_dir_path is %s\n", 
			      mbox_folder->folder_dir_path);
	CAMEL_LOG_FULL_DEBUG ("Leaving CamelMboxFolder::set_name\n");
}






static gboolean
_exists (CamelFolder *folder, CamelException *ex)
{
	CamelMboxFolder *mbox_folder = CAMEL_MBOX_FOLDER(folder);
	struct stat stat_buf;
	gint stat_error;
	gboolean exists;

	CAMEL_LOG_FULL_DEBUG ("Entering CamelMboxFolder::exists\n");

	/* check if the folder object exists */
	if (!folder) {
		camel_exception_set (ex, 
				     CAMEL_EXCEPTION_FOLDER_NULL,
				     "folder object is NULL");
		return FALSE;
	}

	/* check if the mbox file path is determined */
	if (!mbox_folder->folder_file_path) {
		camel_exception_set (ex, 
				     CAMEL_EXCEPTION_FOLDER_INVALID,
				     "undetermined folder file path. Maybe use set_name ?");
		return FALSE;
	}

	/* check if the mbox dir path is determined */
	if (!mbox_folder->folder_dir_path) {
		camel_exception_set (ex, 
				     CAMEL_EXCEPTION_FOLDER_INVALID,
				     "undetermined folder directory path. Maybe use set_name ?");
		return FALSE;
	}


	/* we should not check for that here */
#if 0
	/* check if the mbox directory exists */
	access_result = access (mbox_folder->folder_dir_path, F_OK);
	if (access_result < 0) {
		CAMEL_LOG_FULL_DEBUG ("CamelMboxFolder::exists errot when executing access on %s\n", 
				      mbox_folder->folder_dir_path);
		CAMEL_LOG_FULL_DEBUG ("  Full error text is : %s\n", strerror(errno));
		camel_exception_set (ex, 
				     CAMEL_EXCEPTION_SYSTEM,
				     strerror(errno));
		return FALSE;
	}
	stat_error = stat (mbox_folder->folder_dir_path, &stat_buf);
	if (stat_error == -1)  {
		CAMEL_LOG_FULL_DEBUG ("CamelMboxFolder::exists when executing stat on %s, stat_error = %d\n", 
				      mbox_folder->folder_dir_path, stat_error);
		CAMEL_LOG_FULL_DEBUG ("  Full error text is : %s\n", strerror(errno));
		camel_exception_set (ex, 
				     CAMEL_EXCEPTION_SYSTEM,
				     strerror(errno));
		return FALSE;
	}
	exists = S_ISDIR (stat_buf.st_mode);
	if (!exists) return FALSE;
#endif 


	/* check if the mbox file exists */
	stat_error = stat (mbox_folder->folder_file_path, &stat_buf);
	if (stat_error == -1)
		return FALSE;
	
	exists = S_ISREG (stat_buf.st_mode);
	/* we should  check the rights here  */
	
	CAMEL_LOG_FULL_DEBUG ("Leaving CamelMboxFolder::exists\n");
	return exists;
}








static gboolean
_create (CamelFolder *folder, CamelException *ex)
{
	CamelMboxFolder *mbox_folder = CAMEL_MBOX_FOLDER(folder);
	const gchar *folder_file_path, *folder_dir_path;
	mode_t dir_mode = S_IRWXU;
	gint mkdir_error;
	gboolean folder_already_exists;
	int creat_fd;
	mode_t old_umask;

	/* check if the folder object exists */
	if (!folder) {
		camel_exception_set (ex, 
				     CAMEL_EXCEPTION_FOLDER_NULL,
				     "folder object is NULL");
		return FALSE;
	}


	/* call default implementation */
	parent_class->create (folder, ex);

	/* get the paths of what we need to create */
	folder_file_path = mbox_folder->folder_file_path;
	folder_dir_path = mbox_folder->folder_file_path;
	
	if (!(folder_file_path || folder_dir_path)) {
		camel_exception_set (ex, 
				     CAMEL_EXCEPTION_FOLDER_INVALID,
				     "invalid folder path. Use set_name ?");
		return FALSE;
	}

	
	/* if the folder already exists, simply return */
	folder_already_exists = camel_folder_exists (folder,ex);
	if (camel_exception_get_id (ex)) return FALSE;

	if (folder_already_exists) return TRUE;


	/* create the directory for the subfolders */
	mkdir_error = mkdir (folder_dir_path, dir_mode);
	if (mkdir_error == -1) goto io_error;
	

	/* create the mbox file */ 
	/* it must be rw for the user and none for the others */
	old_umask = umask (0700);
	creat_fd = open (folder_file_path, 
			 O_WRONLY | O_CREAT | O_APPEND,
			 S_IRUSR  | S_IWUSR); 
	umask (old_umask);
	if (creat_fd == -1) goto io_error;
	close (creat_fd);

	/* create the summary object */	
	mbox_folder->summary = g_new (CamelMboxSummary, 1);
	mbox_folder->summary->nb_message = 0;
	mbox_folder->summary->next_uid = 1;
	mbox_folder->summary->mbox_file_size = 0;
	mbox_folder->summary->message_info = g_array_new (FALSE, FALSE, sizeof (CamelMboxSummaryInformation));

	return TRUE;

	/* exception handling for io errors */
	io_error :

		CAMEL_LOG_WARNING ("CamelMboxFolder::create, error when creating %s and %s\n", 
				   folder_dir_path, folder_file_path);
		CAMEL_LOG_FULL_DEBUG ( "  Full error text is : %s\n", strerror(errno));

		if (errno == EACCES) {
			camel_exception_set (ex, 
					     CAMEL_EXCEPTION_FOLDER_INSUFFICIENT_PERMISSION,
					     "You don't have the permission to create the mbox file.");
			return FALSE;
		} else {
			camel_exception_set (ex, 
					     CAMEL_EXCEPTION_SYSTEM,
					     "Unable to create the mbox file.");
			return FALSE;
		}
}








static gboolean
_delete (CamelFolder *folder, gboolean recurse, CamelException *ex)
{
	CamelMboxFolder *mbox_folder = CAMEL_MBOX_FOLDER(folder);
	const gchar *folder_file_path, *folder_dir_path;
	gint rmdir_error = 0;
	gint unlink_error = 0;
	gboolean folder_already_exists;

	/* check if the folder object exists */
	if (!folder) {
		camel_exception_set (ex, 
				     CAMEL_EXCEPTION_FOLDER_NULL,
				     "folder object is NULL");
		return FALSE;
	}


	/* in the case where the folder does not exist, 
	   return immediatly */
	folder_already_exists = camel_folder_exists (folder, ex);
	if (camel_exception_get_id (ex)) return FALSE;

	if (!folder_already_exists) return TRUE;


	/* call default implementation.
	   It should delete the messages in the folder
	   and recurse the operation to subfolders */
	parent_class->delete (folder, recurse, ex);
	

	/* get the paths of what we need to be deleted */
	folder_file_path = mbox_folder->folder_file_path;
	folder_dir_path = mbox_folder->folder_file_path;
	
	if (!(folder_file_path || folder_dir_path)) {
		camel_exception_set (ex, 
				     CAMEL_EXCEPTION_FOLDER_INVALID,
				     "invalid folder path. Use set_name ?");
		return FALSE;
	}

	
	/* physically delete the directory */
	CAMEL_LOG_FULL_DEBUG ("CamelMboxFolder::delete removing directory %s\n", folder_dir_path);
	rmdir_error = rmdir (folder_dir_path);
	if (rmdir_error == -1) 
		switch errno { 
		case EACCES :
			camel_exception_set (ex, 
					     CAMEL_EXCEPTION_FOLDER_INSUFFICIENT_PERMISSION,
					     "Not enough permission to delete the mbox folder");
			return FALSE;			
			break;
			
		case ENOTEMPTY :
				camel_exception_set (ex, 
					     CAMEL_EXCEPTION_FOLDER_NON_EMPTY,
						     "mbox folder not empty. Cannot delete it. Maybe use recurse flag ?");
				return FALSE;		
				break;
		default :
			camel_exception_set (ex, 
					     CAMEL_EXCEPTION_SYSTEM,
					     "Unable to delete the mbox folder.");
			return FALSE;
	}
	
	/* physically delete the file */
	unlink_error = unlink (folder_dir_path);
	if (unlink_error == -1) 
		switch errno { 
		case EACCES :
		case EPERM :
		case EROFS :
			camel_exception_set (ex, 
					     CAMEL_EXCEPTION_FOLDER_INSUFFICIENT_PERMISSION,
					     "Not enough permission to delete the mbox file");
			return FALSE;			
			break;
			
		case EFAULT :
		case ENOENT :
		case ENOTDIR :
		case EISDIR :
			camel_exception_set (ex, 
					     CAMEL_EXCEPTION_FOLDER_INVALID_PATH,
					     "Invalid mbox file");
			return FALSE;			
			break;

		default :
			camel_exception_set (ex, 
					     CAMEL_EXCEPTION_SYSTEM,
					     "Unable to delete the mbox folder.");
			return FALSE;
	}


	return TRUE;
}




gboolean
_delete_messages (CamelFolder *folder, CamelException *ex)
{
	
	CamelMboxFolder *mbox_folder = CAMEL_MBOX_FOLDER(folder);
	const gchar *folder_file_path;
	gboolean folder_already_exists;
	int creat_fd;
	mode_t old_umask;
	

	/* check if the folder object exists */
	if (!folder) {
		camel_exception_set (ex, 
				     CAMEL_EXCEPTION_FOLDER_NULL,
				     "folder object is NULL");
		return FALSE;
	}

	/* in the case where the folder does not exist, 
	   return immediatly */
	folder_already_exists = camel_folder_exists (folder, ex);
	if (camel_exception_get_id (ex)) return FALSE;

	if (!folder_already_exists) return TRUE;



	/* get the paths of the mbox file we need to delete */
	folder_file_path = mbox_folder->folder_file_path;
	
	if (!folder_file_path) {
		camel_exception_set (ex, 
				     CAMEL_EXCEPTION_FOLDER_INVALID,
				     "invalid folder path. Use set_name ?");
		return FALSE;
	}

		
	/* create the mbox file */ 
	/* it must be rw for the user and none for the others */
	old_umask = umask (0700);
	creat_fd = open (folder_file_path, 
			 O_WRONLY | O_TRUNC,
			 S_IRUSR  | S_IWUSR); 
	umask (old_umask);
	if (creat_fd == -1) goto io_error;
	close (creat_fd);
	
	return TRUE;

	/* exception handling for io errors */
	io_error :

		CAMEL_LOG_WARNING ("CamelMboxFolder::create, error when deleting files  %s\n", 
				   folder_file_path);
		CAMEL_LOG_FULL_DEBUG ( "  Full error text is : %s\n", strerror(errno));

		if (errno == EACCES) {
			camel_exception_set (ex, 
					     CAMEL_EXCEPTION_FOLDER_INSUFFICIENT_PERMISSION,
					     "You don't have the permission to write in the mbox file.");
			return FALSE;
		} else {
			camel_exception_set (ex, 
					     CAMEL_EXCEPTION_SYSTEM,
					     "Unable to write in the mbox file.");
			return FALSE;
		}
	

}









static GList *
_list_subfolders (CamelFolder *folder, CamelException *ex)
{
	GList *subfolder_name_list = NULL;

	CamelMboxFolder *mbox_folder = CAMEL_MBOX_FOLDER(folder);
	const gchar *folder_dir_path;
	gboolean folder_exists;

	struct stat stat_buf;
	gint stat_error = 0;
	//GList *file_list;
	gchar *entry_name;
	gchar *full_entry_name;
	gchar *real_folder_name;
	struct dirent *dir_entry;
	DIR *dir_handle;
	gboolean folder_suffix_found;
	
	//gchar *io_error_text;
	


	/* check if the folder object exists */
	if (!folder) {
		camel_exception_set (ex, 
				     CAMEL_EXCEPTION_FOLDER_NULL,
				     "folder object is NULL");
		return FALSE;
	}


	/* in the case the folder does not exist, 
	   raise an exception */
	folder_exists = camel_folder_exists (folder, ex);
	if (camel_exception_get_id (ex)) return FALSE;

	if (!folder_exists) {
		camel_exception_set (ex, 
				     CAMEL_EXCEPTION_FOLDER_INVALID,
				     "Inexistant folder.");
		return FALSE;
	}


	/* get the mbox subfolders directories */
	folder_dir_path = mbox_folder->folder_file_path;
	if (!folder_dir_path) {
		camel_exception_set (ex, 
				     CAMEL_EXCEPTION_FOLDER_INVALID,
				     "Invalid folder path. Use set_name ?");
		return FALSE;
	}

		
	dir_handle = opendir (folder_dir_path);
	
	/* read the first entry in the directory */
	dir_entry = readdir (dir_handle);
	while ((stat_error != -1) && (dir_entry != NULL)) {

		/* get the name of the next entry in the dir */
		entry_name = dir_entry->d_name;
		full_entry_name = g_strdup_printf ("%s/%s", folder_dir_path, entry_name);
		stat_error = stat (full_entry_name, &stat_buf);
		g_free (full_entry_name);

		/* is it a directory ? */
		if ((stat_error != -1) && S_ISDIR (stat_buf.st_mode)) {
			/* yes, add it to the list */
			if (entry_name[0] != '.') {
				CAMEL_LOG_FULL_DEBUG ("CamelMboxFolder::list_subfolders adding "
						      "%s\n", entry_name);

				/* if the folder is a netscape folder, remove the  
				   ".sdb" from the name */
				real_folder_name = string_prefix (entry_name, ".sdb", &folder_suffix_found);
				/* stick here the tests for other folder suffixes if any */
				
				if (!folder_suffix_found) real_folder_name = g_strdup (entry_name);
				
				/* add the folder name to the list */
				subfolder_name_list = g_list_append (subfolder_name_list, 
								     real_folder_name);
			}
		}
		/* read next entry */
		dir_entry = readdir (dir_handle);
	}

	closedir (dir_handle);

	return subfolder_name_list;

	

	/* io exception handling */
	io_error : 

		switch errno { 
		case EACCES :
			
			camel_exception_setv (ex, 
					      CAMEL_EXCEPTION_FOLDER_INSUFFICIENT_PERMISSION,
					      "Unable to list the directory. Full Error text is : %s ", 
					      strerror (errno));
			break;
			
		case ENOENT :
		case ENOTDIR :
			camel_exception_setv (ex, 
					      CAMEL_EXCEPTION_FOLDER_INVALID_PATH,
					      "Invalid mbox folder path. Full Error text is : %s ", 
					      strerror (errno));
			break;
			
		default :
			camel_exception_set (ex, 
					     CAMEL_EXCEPTION_SYSTEM,
					     "Unable to delete the mbox folder.");
			
		}
	
	g_list_free (subfolder_name_list);
	return NULL;
}




static gint
_get_message_count (CamelFolder *folder, CamelException *ex)
{
	
	CamelMboxFolder *mbox_folder = CAMEL_MBOX_FOLDER(folder);
	gint message_count;

	g_assert (folder);
	g_assert (mbox_folder->summary);
	
	message_count = mbox_folder->summary->nb_message;

	CAMEL_LOG_FULL_DEBUG ("CamelMboxFolder::get_message_count found %d messages\n", message_count);
	return message_count;
}


static gint
_append_message (CamelFolder *folder, CamelMimeMessage *message, CamelException *ex)
{
	CamelMboxFolder *mbox_folder = CAMEL_MBOX_FOLDER(folder);
	//guint new_msg_number;
	CamelStream *output_stream;
	guint32 tmp_file_size;
	guint32 next_uid;
	gint tmp_file_fd;
	GArray *message_info_array;
	GArray *mbox_summary_info;
	gchar *tmp_message_filename;
	gint fd1, fd2;


	CAMEL_LOG_FULL_DEBUG ("Entering CamelMboxFolder::append_message\n");

	tmp_message_filename = g_strdup_printf ("%s.tmp", mbox_folder->folder_file_path);
	/* write the message itself */
	output_stream = camel_stream_fs_new_with_name (tmp_message_filename, CAMEL_STREAM_FS_WRITE);
	if (output_stream != NULL) {
		camel_stream_write_string (output_stream, "From - \n");
		camel_data_wrapper_write_to_stream (CAMEL_DATA_WRAPPER (message), output_stream);
	}
	camel_stream_close (output_stream);

	/* at this point we have saved the message to a
	   temporary file, now, we have to add the x-evolution 
	   field and also update the main summary summary */

	/* 
	   First : parse the mbox file, but only from the 
	   position where the message has been added, 
	   wich happens to be the last postion in the 
	   mbox file before we added the message.
	   This position is still stored in the summary 
	   for the moment 
	*/
	next_uid = mbox_folder->summary->next_uid;
	tmp_file_fd = open (tmp_message_filename, O_RDONLY);
	message_info_array = camel_mbox_parse_file (tmp_file_fd, 
						    "From - ", 
						    0,
						    &tmp_file_size,
						    &next_uid,
						    TRUE,
						    NULL,
						    0,
						    ex); 
	
	close (tmp_file_fd);

	/* get the value of the last available UID
	   as saved in the summary file */
	next_uid = mbox_folder->summary->next_uid;

	/* 
	   OK, this is not very efficient, we should not use the same
	   method as for parsing an entire mail file, 
	   but I have no time to write a simpler parser 
	*/
	next_uid = camel_mbox_write_xev (tmp_message_filename, 
					 message_info_array, &tmp_file_size, next_uid, ex);
	
	if (camel_exception_get_id (ex)) { 
		/* ** FIXME : free the preparsed information */
		return -1;
		}

	mbox_summary_info =
		parsed_information_to_mbox_summary (message_info_array);




	/* store the number of messages as well as the summary array */
	mbox_folder->summary->nb_message += 1;		
	mbox_folder->summary->next_uid = next_uid;	

	((CamelMboxSummaryInformation *)(mbox_summary_info->data))->position += mbox_folder->summary->mbox_file_size;
	mbox_folder->summary->mbox_file_size += tmp_file_size;		
	
	camel_summary_append_entries (mbox_folder->summary, mbox_summary_info);
	
	g_array_free (mbox_summary_info, TRUE); 
	

	/* append the temporary file message to the mbox file */
	fd1 = open (tmp_message_filename, O_RDONLY);
	fd2 = open (mbox_folder->folder_file_path, O_WRONLY | O_CREAT | O_APPEND);
	if (fd2 == -1) {
			camel_exception_setv (ex, 
					     CAMEL_EXCEPTION_FOLDER_INSUFFICIENT_PERMISSION,
					     "could not open the mbox folder file for appending the message\n"
					      "\t%s\n"
					      "Full error is : %s\n",
					      mbox_folder->folder_file_path,
					      strerror (errno));
			return -1;
		}

	camel_mbox_copy_file_chunk (fd1,
				    fd2, 
				    tmp_file_size, 
				    ex);
	close (fd1);
	close (fd2);

	/* remove the temporary file */
	unlink (tmp_message_filename);

	/* generate the folder md5 signature */
	md5_get_digest_from_file (mbox_folder->folder_file_path, mbox_folder->summary->md5_digest);


	g_free (tmp_message_filename);
	CAMEL_LOG_FULL_DEBUG ("Leaving CamelMboxFolder::append_message\n");
	
	return -1;
}




static GList *
_get_uid_list (CamelFolder *folder, CamelException *ex) 
{
	CamelMboxFolder *mbox_folder = CAMEL_MBOX_FOLDER(folder);
	GArray *message_info_array;
	CamelMboxSummaryInformation *message_info;
	GList *uid_list = NULL;
	int i;

	CAMEL_LOG_FULL_DEBUG ("Entering CamelMboxFolder::get_uid_list\n");
	
	message_info_array = mbox_folder->summary->message_info;
	
	for (i=0; i<message_info_array->len; i++) {
		
		message_info = (CamelMboxSummaryInformation *)(message_info_array->data) + i;
		uid_list = g_list_prepend (uid_list, g_strdup_printf ("%u", message_info->uid));
	}
	
	CAMEL_LOG_FULL_DEBUG ("Leaving CamelMboxFolder::get_uid_list\n");
	
	return uid_list;
}
































