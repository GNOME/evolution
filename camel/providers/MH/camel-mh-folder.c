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
#include <sys/param.h> 
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

static int copy_reg (const char *src_path, const char *dst_path);
static void _set_name(CamelFolder *folder, const gchar *name);
static void _init_with_store (CamelFolder *folder, CamelStore *parent_store);
static gboolean _exists (CamelFolder *folder);
static gboolean _create(CamelFolder *folder);
static gboolean _delete (CamelFolder *folder, gboolean recurse);
static gboolean _delete_messages (CamelFolder *folder);
static GList *_list_subfolders (CamelFolder *folder);
static CamelMimeMessage *_get_message (CamelFolder *folder, gint number);
static gint _get_message_count (CamelFolder *folder);
static gint _append_message (CamelFolder *folder, CamelMimeMessage *message);
static void _expunge (CamelFolder *folder);
static void _copy_message_to (CamelFolder *folder, CamelMimeMessage *message, CamelFolder *dest_folder);


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
	camel_folder_class->append_message = _append_message;
	camel_folder_class->expunge = _expunge;
	camel_folder_class->copy_message_to = _copy_message_to;
	
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

	CAMEL_LOG_FULL_DEBUG ("CamelMhFolder::set_name mh_folder->directory_path is %s\n", 
			      mh_folder->directory_path);
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
				CAMEL_LOG_WARNING ("CamelMhFolder::delete_messages Error when deleting file %s\n", 
						   entry_name);
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
_list_subfolders (CamelFolder *folder)
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

static void
_filename_free (gpointer data)
{
	g_free ((gchar *)data);
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
			message_list = g_list_insert_sorted (message_list, g_strdup (dir_entry->d_name), 
							     _message_name_compare);
		/* read next entry */
		dir_entry = readdir (dir_handle);
	}		

	closedir (dir_handle);
		

	message_name = g_list_nth_data (message_list, number);
	
	if (message_name != NULL) {
		CAMEL_LOG_FULL_DEBUG  ("CanelMhFolder::get_message message number = %d, name = %s\n", 
				       number, message_name);
		message_file_name = g_strdup_printf ("%s/%s", directory_path, message_name);
		input_stream = camel_stream_fs_new_with_name (message_file_name, CAMEL_STREAM_FS_READ);
		
		if (input_stream != NULL) {
#warning use session field here
			message = camel_mime_message_new_with_session ( (CamelSession *)NULL);
			camel_data_wrapper_construct_from_stream ( CAMEL_DATA_WRAPPER (message), input_stream);
			gtk_object_unref (GTK_OBJECT (input_stream));
			message->message_number = number;
			gtk_object_set_data_full (GTK_OBJECT (message), "fullpath", 
						  g_strdup (message_file_name), _filename_free);
			
#warning Set flags and all this stuff here
		}
		g_free (message_file_name);
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
	CAMEL_LOG_FULL_DEBUG ("CamelMhFolder::get_message_count found %d messages\n", message_count);
	return message_count;
}



static gboolean
_find_next_free_message_file (CamelFolder *folder, gint *new_msg_number, gchar **new_msg_filename)
{
	CamelMhFolder *mh_folder = CAMEL_MH_FOLDER(folder);
	const gchar *directory_path;
	struct dirent *dir_entry;
	DIR *dir_handle;
	gint last_max_message_number = 0;
	gint current_message_number;

	g_assert(folder);

	directory_path = mh_folder->directory_path;
	if (!directory_path) return FALSE;
	
	if (!camel_folder_exists (folder)) return FALSE;
	
	dir_handle = opendir (directory_path);
	
	/* read first entry in the directory */
	dir_entry = readdir (dir_handle);
	while (dir_entry != NULL) {
		/* tests if the entry correspond to a message file */
		if (_is_a_message_file (dir_entry->d_name, directory_path)) {
			/* see if the message number is the biggest found */
			current_message_number = atoi (dir_entry->d_name);
			if (current_message_number > last_max_message_number)
				last_max_message_number = current_message_number;
		}
		/* read next entry */
		dir_entry = readdir (dir_handle);
	}
	closedir (dir_handle);
	
	*new_msg_number = last_max_message_number + 1;
	*new_msg_filename = g_strdup_printf ("%s/%d", directory_path, *new_msg_number);
	CAMEL_LOG_FULL_DEBUG ("CamelMhFolder::find_next_free_message_file new message path is %s\n", 
			      *new_msg_filename);
	return TRUE;
	
	
}
static gint
_append_message (CamelFolder *folder, CamelMimeMessage *message)
{
	guint new_msg_number;
	gchar *new_msg_filename;
	CamelStream *output_stream;
	gboolean error;

	CAMEL_LOG_FULL_DEBUG ("Entering CamelMhFolder::append_message\n");
	if (!_find_next_free_message_file (folder, &new_msg_number, &new_msg_filename))
		return -1;

	output_stream = camel_stream_fs_new_with_name (new_msg_filename, CAMEL_STREAM_FS_WRITE);
	if (output_stream != NULL) {
		camel_data_wrapper_write_to_stream (CAMEL_DATA_WRAPPER (message), output_stream);
		camel_stream_close (output_stream);
	} else {
		CAMEL_LOG_WARNING ("CamelMhFolder::append_message could not open %s for writing\n", 
				   new_msg_filename);
		CAMEL_LOG_FULL_DEBUG ("  Full error text is : %s\n", strerror(errno));
		error = TRUE;
	}

	g_free (new_msg_filename);
	CAMEL_LOG_FULL_DEBUG ("Leaving CamelMhFolder::append_message\n");
	if (error) return -1;
	else return new_msg_number;
}





static void
_expunge (CamelFolder *folder)
{
	/* For the moment, we look in the folder active message
	 * list. I did not make my mind for the moment, should 
	 * the gtk_object->destroy signal be used to expunge
	 * freed messages objects marked DELETED ? 
	 */
	CamelMimeMessage *message;
	GList *message_node;
	gchar *fullpath;
	gint unlink_error;

	CAMEL_LOG_FULL_DEBUG ("Entering CamelFolder::expunge\n");
	
	message_node = folder->message_list;

	/* look in folder message list which messages
	 * need to be expunged  */
	while ( message_node) {
		message = CAMEL_MIME_MESSAGE (message_node->data);
		
		if (message && camel_mime_message_get_flag (message, "DELETED")) {
			CAMEL_LOG_FULL_DEBUG ("CamelMhFolder::expunge, expunging  message %d\n", message->message_number);
			/* expunge the message */
			fullpath = gtk_object_get_data (GTK_OBJECT (message), "fullpath");
			CAMEL_LOG_FULL_DEBUG ("CamelMhFolder::expunge, message fullpath is %s\n", 
					      fullpath);
			unlink_error = unlink(fullpath);
			if (unlink_error != -1) {
				message->expunged = TRUE;
			} else {
				CAMEL_LOG_WARNING ("CamelMhFolder:: could not unlink %s (message %d)\n", 
						   fullpath, message->message_number);
				CAMEL_LOG_FULL_DEBUG ("  Full error text is : %s\n", strerror(errno));
			}
		}
		message_node = message_node->next;
		CAMEL_LOG_FULL_DEBUG ("CamelFolder::expunge, examined message node %p\n", message_node);
	}
	
	CAMEL_LOG_FULL_DEBUG ("Leaving CamelFolder::expunge\n");
}


static void
_copy_message_to (CamelFolder *folder, CamelMimeMessage *message, CamelFolder *dest_folder)
{
	gchar *src_msg_filename;
	guint dest_msg_number;
	gchar *dest_msg_filename;

	if (IS_CAMEL_MH_FOLDER (dest_folder)) {
		/*g_return_if_fail (message->parent_folder == folder);*/
		
		if (!_find_next_free_message_file (dest_folder, &dest_msg_number, &dest_msg_filename))
			return;
		src_msg_filename = gtk_object_get_data (GTK_OBJECT (message), "fullpath");
		CAMEL_LOG_FULL_DEBUG ("CamelMhFolder::copy_to copy file %s to %s\n", src_msg_filename, dest_msg_filename);
		copy_reg (src_msg_filename, dest_msg_filename);
		
	} else 
		parent_class->copy_message_to (folder, message, dest_folder);
}




/************************************************************************/

/*** Took directly from GNU fileutils-4.0                             ***/
/* Copyright (C) 89, 90, 91, 95, 96, 97, 1998 Free Software Foundation. */
/* This may be rwritten soon. -Bertrand                                 */               

    
/* Write LEN bytes at PTR to descriptor DESC, retrying if interrupted.
   Return LEN upon success, write's (negative) error code otherwise.  */
int
full_write (int desc, const char *ptr, size_t len)
{
  int total_written;

  total_written = 0;
  while (len > 0)
    {
      int written = write (desc, ptr, len);
      if (written < 0)
	{
	  if (errno == EINTR)
	    continue;
	  return written;
	}
      total_written += written;
      ptr += written;
      len -= written;
    }
  return total_written;
}




static int
copy_reg (const char *src_path, const char *dst_path)
{
  char *buf;
  int buf_size;
  int dest_desc;
  int source_desc;
  int n_read;
  struct stat sb;
  char *cp;
  int *ip;
  int return_val = 0;
  off_t n_read_total = 0;
  int last_write_made_hole = 0;
  int make_holes = TRUE;

  source_desc = open (src_path, O_RDONLY);
  if (source_desc < 0)
    {
      /* If SRC_PATH doesn't exist, then chances are good that the
	 user did something like this `cp --backup foo foo': and foo
	 existed to start with, but copy_internal renamed DST_PATH
	 with the backup suffix, thus also renaming SRC_PATH.  */
      if (errno == ENOENT)
	error (0, 0, "`%s' and `%s' are the same file",
	       src_path, dst_path);
      else
	error (0, errno, "%s", src_path);

      return -1;
    }

  /* Create the new regular file with small permissions initially,
     to not create a security hole.  */

  dest_desc = open (dst_path, O_WRONLY | O_CREAT | O_TRUNC, 0600);
  if (dest_desc < 0)
    {
      error (0, errno, "cannot create regular file `%s'", dst_path);
      return_val = -1;
      goto ret2;
    }

  /* Find out the optimal buffer size.  */

  if (fstat (dest_desc, &sb))
    {
      error (0, errno, "%s", dst_path);
      return_val = -1;
      goto ret;
    }

  buf_size = 8192;



  /* Make a buffer with space for a sentinel at the end.  */

  buf = (char *) alloca (buf_size + sizeof (int));

  for (;;)
    {
      n_read = read (source_desc, buf, buf_size);
      if (n_read < 0)
	{
	  if (errno == EINTR)
	    continue;
	  error (0, errno, "%s", src_path);
	  return_val = -1;
	  goto ret;
	}
      if (n_read == 0)
	break;

      n_read_total += n_read;

      ip = 0;
      if (make_holes)
	{
	  buf[n_read] = 1;	/* Sentinel to stop loop.  */

	  /* Find first nonzero *word*, or the word with the sentinel.  */

	  ip = (int *) buf;
	  while (*ip++ == 0)
	    ;

	  /* Find the first nonzero *byte*, or the sentinel.  */

	  cp = (char *) (ip - 1);
	  while (*cp++ == 0)
	    ;

	  /* If we found the sentinel, the whole input block was zero,
	     and we can make a hole.  */

	  if (cp > buf + n_read)
	    {
	      /* Make a hole.  */
	      if (lseek (dest_desc, (off_t) n_read, SEEK_CUR) < 0L)
		{
		  error (0, errno, "%s", dst_path);
		  return_val = -1;
		  goto ret;
		}
	      last_write_made_hole = 1;
	    }
	  else
	    /* Clear to indicate that a normal write is needed. */
	    ip = 0;
	}
      if (ip == 0)
	{
	  if (full_write (dest_desc, buf, n_read) < 0)
	    {
	      error (0, errno, "%s", dst_path);
	      return_val = -1;
	      goto ret;
	    }
	  last_write_made_hole = 0;
	}
    }

  /* If the file ends with a `hole', something needs to be written at
     the end.  Otherwise the kernel would truncate the file at the end
     of the last write operation.  */

  if (last_write_made_hole)
    {
#if HAVE_FTRUNCATE
      /* Write a null character and truncate it again.  */
      if (full_write (dest_desc, "", 1) < 0
	  || ftruncate (dest_desc, n_read_total) < 0)
#else
      /* Seek backwards one character and write a null.  */
      if (lseek (dest_desc, (off_t) -1, SEEK_CUR) < 0L
	  || full_write (dest_desc, "", 1) < 0)
#endif
	{
	  error (0, errno, "%s", dst_path);
	  return_val = -1;
	}
    }

ret:
  if (close (dest_desc) < 0)
    {
      error (0, errno, "%s", dst_path);
      return_val = -1;
    }
ret2:
  if (close (source_desc) < 0)
    {
      error (0, errno, "%s", src_path);
      return_val = -1;
    }

  return return_val;
}
