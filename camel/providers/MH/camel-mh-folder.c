/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* camel-mh-folder.c : Abstract class for an email folder */

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
#include <config.h> 
#include <sys/stat.h> 
#include <unistd.h>
#include <sys/types.h>
#include <fcntl.h>
#include <dirent.h>
#include <stdio.h>
#include <errno.h>
#include "camel-mh-folder.h"
#include "camel-mh-store.h"
#include "gstring-util.h"
#include "camel-log.h"
#include "camel-stream-fs.h"


static CamelFolderClass *parent_class=NULL;

/* Returns the class for a CamelMhFolder */
#define CMHF_CLASS(so) CAMEL_MH_FOLDER_CLASS (GTK_OBJECT(so)->klass)
#define CF_CLASS(so) CAMEL_FOLDER_CLASS (GTK_OBJECT(so)->klass)
#define CMHS_CLASS(so) CAMEL_STORE_CLASS (GTK_OBJECT(so)->klass)

static void _set_name(CamelFolder *folder, const gchar *name);
static void _init_with_store (CamelFolder *folder, CamelStore *parent_store);
static gboolean _exists (CamelFolder *folder);
static gboolean _create(CamelFolder *folder);
static gboolean _delete (CamelFolder *folder, gboolean recurse);
static gboolean _delete_messages (CamelFolder *folder);
static GList *_list_subfolders (CamelFolder *folder);
static CamelMimeMessage *_get_message (CamelFolder *folder, gint number);
static gint _get_message_count (CamelFolder *folder);

static void
camel_mh_folder_class_init (CamelMhFolderClass *camel_mh_folder_class)
{
	CamelFolderClass *camel_folder_class = CAMEL_FOLDER_CLASS (camel_mh_folder_class);

	parent_class = gtk_type_class (camel_folder_get_type ());
		
	/* virtual method definition */
	/* virtual method overload */
	camel_folder_class->init_with_store = _init_with_store;
	camel_folder_class->set_name = _set_name;
	camel_folder_class->exists = _exists;
	camel_folder_class->delete = _delete;
	camel_folder_class->delete_messages = _delete_messages;
	camel_folder_class->list_subfolders = _list_subfolders;
	camel_folder_class->get_message = _get_message;
	camel_folder_class->get_message_count = _get_message_count;
	
}







GtkType
camel_mh_folder_get_type (void)
{
	static GtkType camel_mh_folder_type = 0;
	
	if (!camel_mh_folder_type)	{
		GtkTypeInfo camel_mh_folder_info =	
		{
			"CamelMhFolder",
			sizeof (CamelMhFolder),
			sizeof (CamelMhFolderClass),
			(GtkClassInitFunc) camel_mh_folder_class_init,
			(GtkObjectInitFunc) NULL,
				/* reserved_1 */ NULL,
				/* reserved_2 */ NULL,
			(GtkClassInitFunc) NULL,
		};
		
		camel_mh_folder_type = gtk_type_unique (CAMEL_FOLDER_TYPE, &camel_mh_folder_info);
	}
	
	return camel_mh_folder_type;
}




static void 
_init_with_store (CamelFolder *folder, CamelStore *parent_store)
{
	/* call parent method */
	parent_class->init_with_store (folder, parent_store);
	
	folder->can_hold_messages = TRUE;
	folder->can_hold_folders = TRUE;
}



/**
 * camel_mh_folder_set_name: set the name of an MH folder
 * @folder: the folder to set the name
 * @name: a string representing the (short) name
 * 
 * 
 * 
 **/
static void
_set_name (CamelFolder *folder, const gchar *name)
{
	CamelMhFolder *mh_folder = CAMEL_MH_FOLDER (folder);
	const gchar *root_dir_path;
	gchar *full_name;
	const gchar *parent_full_name;
	gchar separator;
	
	CAMEL_LOG_FULL_DEBUG ("Entering CamelMhFolder::set_name\n");
	g_assert(folder);
	g_assert(name);
	g_assert(folder->parent_store);

	/* call default implementation */
	parent_class->set_name (folder, name);
	
	if (mh_folder->directory_path) g_free (mh_folder->directory_path);
	
	separator = camel_store_get_separator (folder->parent_store);
	root_dir_path = camel_mh_store_get_toplevel_dir (CAMEL_MH_STORE(folder->parent_store));

	CAMEL_LOG_FULL_DEBUG ("CamelMhFolder::set_name full_name is %s\n", folder->full_name);
	CAMEL_LOG_FULL_DEBUG ("CamelMhFolder::set_name root_dir_path is %s\n", root_dir_path);
	CAMEL_LOG_FULL_DEBUG ("CamelMhFolder::separator is %c\n", separator);

	mh_folder->directory_path = g_strdup_printf ("%s%c%s", root_dir_path, separator, folder->full_name);

	CAMEL_LOG_FULL_DEBUG ("CamelMhFolder::set_name mh_folder->directory_path is %s\n", mh_folder->directory_path);
	CAMEL_LOG_FULL_DEBUG ("Leaving CamelMhFolder::set_name\n");
}



static gboolean
_exists (CamelFolder *folder)
{
	CamelMhFolder *mh_folder = CAMEL_MH_FOLDER(folder);
	struct stat stat_buf;
	gint stat_error;
	gboolean exists;

	CAMEL_LOG_FULL_DEBUG ("Entering CamelMhFolder::exists\n");
	g_assert (folder);
	
	if (!mh_folder->directory_path)  return FALSE;
	
	stat_error = stat (mh_folder->directory_path, &stat_buf);
	if (stat_error == -1)  {
		CAMEL_LOG_FULL_DEBUG ("CamelMhFolder::exists when executing stat on %s, stat_error = %d\n", 
				      mh_folder->directory_path, stat_error);
		CAMEL_LOG_FULL_DEBUG ("  Full error text is : %s\n", strerror(errno));
		return FALSE;
	}
	exists = S_ISDIR (stat_buf.st_mode);

	CAMEL_LOG_FULL_DEBUG ("Leaving CamelMhFolder::exists\n");
	return exists;
}


static gboolean
_create (CamelFolder *folder)
{
	CamelMhFolder *mh_folder = CAMEL_MH_FOLDER(folder);
	const gchar *directory_path;
	mode_t dir_mode = S_IRWXU;
	gint mkdir_error;

	g_assert(folder);

	/* call default implementation */
	parent_class->create (folder);

	directory_path = mh_folder->directory_path;
	if (!directory_path) return FALSE;
	
	if (camel_folder_exists (folder)) return TRUE;
	
	mkdir_error = mkdir (directory_path, dir_mode);
	return (mkdir_error == -1);
}



static gboolean
_delete (CamelFolder *folder, gboolean recurse)
{

	CamelMhFolder *mh_folder = CAMEL_MH_FOLDER(folder);
	const gchar *directory_path;
	gint rmdir_error = 0;

	g_assert(folder);

	/* call default implementation */
	parent_class->delete (folder, recurse);
	/* the default implementation will care about deleting 
	   messages first and recursing the operation if 
	   necessary */
	
	directory_path = mh_folder->directory_path;
	if (!directory_path) return FALSE;
	
	if (!camel_folder_exists (folder)) return TRUE;
	
	/* physically delete the directory */
	CAMEL_LOG_FULL_DEBUG ("CamelMhFolder::delete removing directory %s\n", directory_path);
	rmdir_error = rmdir (directory_path);
	if (rmdir_error == -1) {
		CAMEL_LOG_WARNING ("CamelMhFolder::delete Error when removing directory %s\n", directory_path);
		CAMEL_LOG_FULL_DEBUG ( "  Full error text is : %s\n", strerror(errno));
	}

	return (rmdir_error != -1);
}


static gboolean 
_delete_messages (CamelFolder *folder)
{
	
	CamelMhFolder *mh_folder = CAMEL_MH_FOLDER(folder);
	const gchar *directory_path;
	struct stat stat_buf;
	gint stat_error = 0;
	gchar *entry_name;
	struct dirent *dir_entry;
	gint unlink_error = 0;
	DIR *dir_handle;

	g_assert(folder);

	/* call default implementation */
	parent_class->delete_messages (folder);

	directory_path = mh_folder->directory_path;
	if (!directory_path) return FALSE;
	
	if (!camel_folder_exists (folder)) return TRUE;
	
	dir_handle = opendir (directory_path);
	
	/* read first entry in the directory */
	dir_entry = readdir (dir_handle);
	while ((stat_error != -1) && (unlink_error != -1) && (dir_entry != NULL)) {

		/* get the name of the next entry in the dir */
		entry_name = dir_entry->d_name;
		stat_error = stat (mh_folder->directory_path, &stat_buf);

		/* is it a regular file ? */
		if ((stat_error != -1) && S_ISREG(stat_buf.st_mode)) {
			/* yes, delete it */
			CAMEL_LOG_FULL_DEBUG ("CamelMhFolder::delete_messages removing file %s\n", entry_name);
			unlink_error = unlink(entry_name);

			if (unlink_error == -1) {
				CAMEL_LOG_WARNING ("CamelMhFolder::delete_messages Error when deleting file %s\n", entry_name);
				CAMEL_LOG_FULL_DEBUG ( "  Full error text is : %s\n", strerror(errno));
			}
		}
		/* read next entry */
		dir_entry = readdir (dir_handle);
	}

	closedir (dir_handle);

	return ((stat_error != -1) && (unlink_error != -1));

}



static GList *
_list_subfolders(CamelFolder *folder)
{
	GList *subfolder_name_list = NULL;

	CamelMhFolder *mh_folder = CAMEL_MH_FOLDER(folder);
	const gchar *directory_path;
	struct stat stat_buf;
	gint stat_error = 0;
	GList *file_list;
	gchar *entry_name;
	gchar *full_entry_name;
	struct dirent *dir_entry;
	DIR *dir_handle;

	g_assert(folder);

	/* call default implementation */
	parent_class->delete_messages (folder);

	directory_path = mh_folder->directory_path;
	if (!directory_path) return NULL;
	
	if (!camel_folder_exists (folder)) return NULL;
	
	dir_handle = opendir (directory_path);
	
	/* read first entry in the directory */
	dir_entry = readdir (dir_handle);
	while ((stat_error != -1) && (dir_entry != NULL)) {

		/* get the name of the next entry in the dir */
		entry_name = dir_entry->d_name;
		full_entry_name = g_strdup_printf ("%s/%s", mh_folder->directory_path, entry_name);
		stat_error = stat (full_entry_name, &stat_buf);
		g_free (full_entry_name);

		/* is it a directory ? */
		if ((stat_error != -1) && S_ISDIR (stat_buf.st_mode)) {
			/* yes, add it to the list */
			if (entry_name[0] != '.') {
				CAMEL_LOG_FULL_DEBUG ("CamelMhFolder::list_subfolders adding  %s\n", entry_name);
				subfolder_name_list = g_list_append (subfolder_name_list, g_strdup (entry_name));
			}
		}
		/* read next entry */
		dir_entry = readdir (dir_handle);
	}

	closedir (dir_handle);

	return subfolder_name_list;
}




static gboolean
_is_a_message_file (const gchar *file_name, const gchar *file_path)
{
	struct stat stat_buf;
	gint stat_error = 0;
	gboolean ok;
	gchar *full_file_name;
	int i;
	
	/* test if the name is a number */
	i=0;
	while ((file_name[i] != '\0') && (file_name[i] >= '0') && (file_name[i] <= '9'))
		i++;
	if ((i==0) || (file_name[i] != '\0')) return FALSE;
	
	/* is it a regular file ? */
	full_file_name = g_strdup_printf ("%s/%s", file_path, file_name);
	stat_error = stat (full_file_name, &stat_buf);
	g_free (full_file_name);

	return  ((stat_error != -1) && S_ISREG (stat_buf.st_mode));
}


static gint
_message_name_compare (gconstpointer a, gconstpointer b)
{
	gchar *m1 = (gchar *)a;
	gchar *m2 = (gchar *)b;
	gint len_diff;

	return (atoi (m1) - atoi (m2));
}

/* slow routine, may be optimixed, or we should use
   caches if users complain */
static CamelMimeMessage *
_get_message (CamelFolder *folder, gint number)
{
	CamelMhFolder *mh_folder = CAMEL_MH_FOLDER(folder);
	const gchar *directory_path;
	gchar *message_name;
	gchar *message_file_name;
	struct dirent *dir_entry;
	DIR *dir_handle;
	CamelStream *input_stream = NULL;
	CamelMimeMessage *message = NULL;
	GList *message_list = NULL;
	
	g_assert(folder);
	
	directory_path = mh_folder->directory_path;
	if (!directory_path) return NULL;	
	if (!camel_folder_exists (folder)) return NULL;
	
	/* read the whole folder and sort message names */
	dir_handle = opendir (directory_path);
	/* read first entry in the directory */
	dir_entry = readdir (dir_handle);
	while (dir_entry != NULL) {
		/* tests if the entry correspond to a message file */
		if (_is_a_message_file (dir_entry->d_name, directory_path)) 
			message_list = g_list_insert_sorted (message_list, g_strdup (dir_entry->d_name), _message_name_compare);
		/* read next entry */
		dir_entry = readdir (dir_handle);
	}		

	closedir (dir_handle);
		

	message_name = g_list_nth_data (message_list, number);
	
	if (message_name != NULL) {
		CAMEL_LOG_FULL_DEBUG  ("CanelMhFolder::get_message message number = %d, name = %s\n", number, message_name);
		message_file_name = g_strdup_printf ("%s/%s", directory_path, message_name);
		input_stream = camel_stream_fs_new_with_name (message_file_name, CAMEL_STREAM_FS_READ);
		g_free (message_file_name);
		if (input_stream != NULL) {
#warning use session field here
			message = camel_mime_message_new_with_session ( (CamelSession *)NULL);
			camel_data_wrapper_construct_from_stream ( CAMEL_DATA_WRAPPER (message), input_stream);
			gtk_object_unref (GTK_OBJECT (input_stream));
		}
	} else 
		CAMEL_LOG_FULL_DEBUG  ("CanelMhFolder::get_message message number = %d, not found\n", number);
	string_list_free (message_list);
	
	return message;   
}



static gint
_get_message_count (CamelFolder *folder)
{
	
	CamelMhFolder *mh_folder = CAMEL_MH_FOLDER(folder);
	const gchar *directory_path;
	struct dirent *dir_entry;
	DIR *dir_handle;
	guint message_count = 0;

	g_assert(folder);

	directory_path = mh_folder->directory_path;
	if (!directory_path) return -1;
	
	if (!camel_folder_exists (folder)) return 0;
	
	dir_handle = opendir (directory_path);
	
	/* read first entry in the directory */
	dir_entry = readdir (dir_handle);
	while (dir_entry != NULL) {
		/* tests if the entry correspond to a message file */
		if (_is_a_message_file (dir_entry->d_name, directory_path)) 
			message_count++;	
		/* read next entry */
		dir_entry = readdir (dir_handle);
	}

	closedir (dir_handle);
	return message_count;
}
