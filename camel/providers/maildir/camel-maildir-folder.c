/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* camel-maildir-folder.c : camel-folder subclass for maildir folders */

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

/*
 * AUTHORS : Jukka Zitting 
 *  
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
#include <time.h>
#include <string.h>
#include "camel-maildir-folder.h"
#include "camel-maildir-store.h"
#include "camel-stream-fs.h"
#include "camel-log.h"

static CamelFolderClass *parent_class=NULL;

/* Returns the class for a CamelMaildirFolder */
#define CMAILDIRF_CLASS(so) CAMEL_MAILDIR_FOLDER_CLASS (GTK_OBJECT(so)->klass)
#define CF_CLASS(so) CAMEL_FOLDER_CLASS (GTK_OBJECT(so)->klass)
#define CMAILDIRS_CLASS(so) CAMEL_STORE_CLASS (GTK_OBJECT(so)->klass)

static void _init_with_store (CamelFolder *folder, CamelStore *parent_store, CamelException *ex);
static void _set_name (CamelFolder *folder, const gchar *name, CamelException *ex);
static gboolean _exists (CamelFolder *folder, CamelException *ex);
static gboolean _create (CamelFolder *folder, CamelException *ex);
static gboolean _delete (CamelFolder *folder, gboolean recurse, CamelException *ex);
static gboolean _delete_messages (CamelFolder *folder, CamelException *ex);
static CamelMimeMessage *_get_message (CamelFolder *folder, gint number, CamelException *ex);
static gint _get_message_count (CamelFolder *folder, CamelException *ex);
static void _expunge (CamelFolder *folder, CamelException *ex);
static GList *_list_subfolders (CamelFolder *folder, CamelException *ex);

/* fs utility functions */
static DIR * _xopendir (const gchar *path);
static gboolean _xstat (const gchar *path, struct stat *buf);
static gboolean _xmkdir (const gchar *path);
static gboolean _xrename (const gchar *from, const gchar *to);
static gboolean _xunlink (const gchar *path);
static gboolean _xrmdir (const gchar *path);
/* ** */

static void
camel_maildir_folder_class_init (CamelMaildirFolderClass *camel_maildir_folder_class)
{
	CamelFolderClass *camel_folder_class =
		CAMEL_FOLDER_CLASS (camel_maildir_folder_class);

	parent_class = gtk_type_class (camel_folder_get_type ());

	/* virtual method definition */
	/* virtual method overload */
	camel_folder_class->init_with_store   = _init_with_store;
	camel_folder_class->set_name          = _set_name;
	camel_folder_class->exists            = _exists;
	camel_folder_class->create            = _create;
	camel_folder_class->delete            = _delete;
	camel_folder_class->delete_messages   = _delete_messages;
	camel_folder_class->expunge           = _expunge;
	camel_folder_class->get_message       = _get_message;
	camel_folder_class->get_message_count = _get_message_count;
	camel_folder_class->list_subfolders   = _list_subfolders;
}

GtkType
camel_maildir_folder_get_type (void)
{
	static GtkType camel_maildir_folder_type = 0;
	
	if (!camel_maildir_folder_type)	{
		GtkTypeInfo camel_maildir_folder_info =	
		{
			"CamelMaildirFolder",
			sizeof (CamelMaildirFolder),
			sizeof (CamelMaildirFolderClass),
			(GtkClassInitFunc) camel_maildir_folder_class_init,
			(GtkObjectInitFunc) NULL,
				/* reserved_1 */ NULL,
				/* reserved_2 */ NULL,
			(GtkClassInitFunc) NULL,
		};
		
		camel_maildir_folder_type =
			gtk_type_unique (CAMEL_FOLDER_TYPE, &camel_maildir_folder_info);
	}
	
	return camel_maildir_folder_type;
}






/**
 * CamelMaildirFolder::init_with_store: initializes the folder object
 * @folder:       folder object to initialize
 * @parent_store: parent store object of the folder
 *
 * Simply tells that the folder can contain messages but not subfolders.
 * Perhaps we'll later implement subfolders too...
 */
static void 
_init_with_store (CamelFolder *folder, CamelStore *parent_store, CamelException *ex)
{
	CAMEL_LOG_FULL_DEBUG ("Entering CamelMaildirFolder::init_with_store\n");
	g_assert (folder);
	g_assert (parent_store);
	
	/* call parent method */
	parent_class->init_with_store (folder, parent_store, ex);
	
	folder->can_hold_messages = TRUE;
	folder->can_hold_folders = TRUE;
	folder->has_summary_capability = FALSE;

	CAMEL_LOG_FULL_DEBUG ("Leaving CamelMaildirFolder::init_with_store\n");
}

/**
 * CamelMaildirFolder::set_name: sets the name of the folder
 * @folder: folder object
 * @name:   name of the folder
 *
 * Sets the name of the folder object. The existence of a folder with
 * the given name is not checked in this function.
 */
static void
_set_name (CamelFolder *folder, const gchar *name, CamelException *ex)
{
	CamelMaildirFolder *maildir_folder;
	CamelMaildirStore *maildir_store;
	
	CAMEL_LOG_FULL_DEBUG ("Entering CamelMaildirFolder::set_name\n");
	g_assert (folder);
	g_assert (name);
	g_assert (folder->parent_store);

	maildir_folder = CAMEL_MAILDIR_FOLDER (folder);
	maildir_store = CAMEL_MAILDIR_STORE (folder->parent_store);

	/* call default implementation */
	parent_class->set_name (folder, name, ex);
	
	if (maildir_folder->directory_path)
		g_free (maildir_folder->directory_path);

	CAMEL_LOG_FULL_DEBUG ("CamelMaildirFolder::set_name full_name is %s\n", folder->full_name);
	CAMEL_LOG_FULL_DEBUG ("CamelMaildirFolder::set_name toplevel_dir is %s\n", maildir_store->toplevel_dir);
	CAMEL_LOG_FULL_DEBUG ("CamelMaildirFolder::set_name separator is %c\n", camel_store_get_separator (folder->parent_store));

	if (folder->full_name && folder->full_name[0])
		maildir_folder->directory_path =
			g_strconcat (maildir_store->toplevel_dir, G_DIR_SEPARATOR_S,
				     folder->full_name, NULL);
	else
		maildir_folder->directory_path = g_strdup (maildir_store->toplevel_dir);

	CAMEL_LOG_FULL_DEBUG ("CamelMaildirFolder::set_name: name set to %s\n", name);
	CAMEL_LOG_FULL_DEBUG ("Leaving CamelMaildirFolder::set_name\n");
}

/**
 * CamelMaildirFolder::exists: tests whether the named maildir exists
 * @folder: folder object
 *
 * A created maildir folder object doesn't necessarily exist yet in the
 * filesystem. This function checks whether the maildir exists.
 * The structure of the maildir is stated in the maildir.5 manpage.
 *
 * maildir.5:
 *     A directory in maildir format  has  three  subdirectories,
 *     all on the same filesystem: tmp, new, and cur.
 *
 * Return value: TRUE if the maildir exists, FALSE otherwise
 */
static gboolean
_exists (CamelFolder *folder, CamelException *ex)
{
	CamelMaildirFolder *maildir_folder = CAMEL_MAILDIR_FOLDER (folder);
	static const gchar *dir[3] = { "new", "cur", "tmp" };
	gint i;
	struct stat statbuf;
	const gchar *maildir;
	gchar *path;
	gboolean rv = TRUE;

	CAMEL_LOG_FULL_DEBUG ("Entering CamelMaildirFolder::exists\n");
	g_assert (folder);
	g_return_val_if_fail (maildir_folder->directory_path, FALSE);

	maildir = maildir_folder->directory_path;

	CAMEL_LOG_FULL_DEBUG ("CamelMailFolder::exists: checking maildir %s\n",
			      maildir);

	/* check whether the toplevel directory exists */
	rv = _xstat (maildir, &statbuf) && S_ISDIR (statbuf.st_mode);

	/* check whether the maildir subdirectories exist */
	for (i = 0; rv && i < 3; i++) {
		path = g_strconcat (maildir, G_DIR_SEPARATOR_S, dir[i], NULL);

		rv = _xstat (path, &statbuf) && S_ISDIR (statbuf.st_mode);

		g_free (path);
	}

	CAMEL_LOG_FULL_DEBUG ("CamelMaildirFolder::exists: %s\n",
			      (rv) ? "maildir found" : "maildir not found");
	CAMEL_LOG_FULL_DEBUG ("Leaving CamelMaildirFolder::exists\n");
	return rv;
}

/**
 * CamelMaildirFolder::create: creates the named maildir
 * @folder: folder object
 *
 * A created maildir folder object doesn't necessarily exist yet in the
 * filesystem. This function creates the maildir if it doesn't yet exist.
 * The structure of the maildir is stated in the maildir.5 manpage.
 *
 * maildir.5:
 *     A directory in maildir format  has  three  subdirectories,
 *     all on the same filesystem: tmp, new, and cur.
 *
 * Return value: TRUE if the maildir existed already or was created,
 *               FALSE otherwise
 */
static gboolean
_create (CamelFolder *folder, CamelException *ex)
{
	CamelMaildirFolder *maildir_folder = CAMEL_MAILDIR_FOLDER (folder);
	static const gchar *dir[3] = { "new", "cur", "tmp" };
	gint i;
	const gchar *maildir;
	gchar *path;
	gboolean rv = TRUE;

	CAMEL_LOG_FULL_DEBUG ("Entering CamelMaildirFolder::create\n");
	g_assert (folder);

	/* check whether the maildir already exists */
	if (camel_folder_exists (folder, ex)) return TRUE;

	maildir = maildir_folder->directory_path;

	CAMEL_LOG_FULL_DEBUG ("CamelMailFolder::create: creating maildir %s\n",
			      maildir);

	/* create the toplevel directory */
	rv = _xmkdir (maildir);

	/* create the maildir subdirectories */
	for (i = 0; rv && i < 3; i++) {
		path = g_strconcat (maildir, G_DIR_SEPARATOR_S, dir[i], NULL);

		rv = _xmkdir (path);

		g_free (path);
	}

	CAMEL_LOG_FULL_DEBUG ("CamelMaildirFolder::create: %s\n",
			      rv ? "maildir created" : "an error occurred");
	CAMEL_LOG_FULL_DEBUG ("Leaving CamelMaildirFolder::create\n");
	return rv;
}

/**
 * CamelMaildirFolder::delete: delete the maildir folder
 * @folder: the folder object
 * @recurse:
 *
 * This function empties and deletes the maildir folder. The subdirectories
 * "tmp", "cur", and "new" are removed first and then the toplevel maildir
 * directory is deleted. All files from the directories are deleted as well, 
 * so you should be careful when using this function. If a subdirectory cannot
 * be deleted, then the operation it is stopped. Thus if an error occurs, the
 * maildir directory won't be removed, but it might no longer be a valid maildir.
 */
static gboolean
_delete (CamelFolder *folder, gboolean recurse, CamelException *ex)
{
	CamelMaildirFolder *maildir_folder = CAMEL_MAILDIR_FOLDER (folder);
	static const gchar *dir[3] = { "new", "cur", "tmp" };
	gint i;
	const gchar *maildir;
	gchar *path;
	gboolean rv = TRUE;

	CAMEL_LOG_FULL_DEBUG ("Entering CamelMaildirFolder::create\n");
	g_assert (folder);

	/* check whether the maildir already exists */
	if (!camel_folder_exists (folder, ex)) return TRUE;

	maildir = maildir_folder->directory_path;

	CAMEL_LOG_FULL_DEBUG ("CamelMailFolder::delete: deleting maildir %s\n",
			      maildir);

	/* delete the maildir subdirectories */
	for (i = 0; rv && i < 3; i++) {
		path = g_strconcat (maildir, G_DIR_SEPARATOR_S, dir[i], NULL);

		rv = _xrmdir (path);

		g_free (path);
	}

	/* create the toplevel directory */
	if (rv)
		rv = _xrmdir (maildir);

	CAMEL_LOG_FULL_DEBUG ("CamelMaildirFolder::delete: %s\n",
			      rv ? "maildir deleted" : "an error occurred");
	CAMEL_LOG_FULL_DEBUG ("Leaving CamelMaildirFolder::delete\n");
	return rv;
}

/**
 * CamelMaildirFolder::delete_messages: empty the maildir folder
 * @folder:  the folder object
 *
 * This function empties the maildir folder. All messages from the
 * "cur" subdirectory are deleted. If a message cannot be deleted, then
 * it is just skipped and the rest of the messages are still deleted.
 * Files with names starting with a dot are skipped as described in the
 * maildir.5 manpage.
 *
 * maildir.5:
 *     It is a good idea for readers to skip all filenames in new
 *     and cur starting with a dot. Other than this, readers
 *     should not attempt to parse filenames.
 *
 * Return value: FALSE on error and if some messages could not be deleted.
 *               TRUE otherwise.
 */
static gboolean 
_delete_messages (CamelFolder *folder, CamelException *ex)
{
	CamelMaildirFolder *maildir_folder = CAMEL_MAILDIR_FOLDER (folder);
	const gchar *maildir;
	gchar *curdir, *file;
	DIR *dir_handle;
	struct dirent *dir_entry;
	gboolean rv = TRUE;

	CAMEL_LOG_FULL_DEBUG ("Entering CamelMaildirFolder::delete_messages\n");
	g_assert (folder);

	/* call default implementation */
	parent_class->delete_messages (folder, ex);

	/* Check if the folder didn't exist */
	if (!camel_folder_exists (folder, ex)) return TRUE;

	maildir = maildir_folder->directory_path;

	CAMEL_LOG_FULL_DEBUG ("CamelMaildirFolder::delete_messages: "
			      "deleting messages from %s\n", maildir);

	/* delete messages from the maildir subdirectory "cur" */
	curdir = g_strconcat (maildir, G_DIR_SEPARATOR_S, "cur", NULL);

	dir_handle = _xopendir (curdir);
	if (dir_handle) {
		while ((dir_entry = readdir (dir_handle))) {
			if (dir_entry->d_name[0] == '.') continue;
			file = g_strconcat (curdir, G_DIR_SEPARATOR_S,
					    dir_entry->d_name, NULL);

			if (!_xunlink (file)) rv = FALSE;

			g_free (file);
		}
		closedir (dir_handle);
	} else
		rv = FALSE;

	g_free (curdir);
	
	CAMEL_LOG_FULL_DEBUG ("CamelMaildirFolder::delete_messages: %s\n",
			      rv ? "messages deleted" : "an error occurred");
	CAMEL_LOG_FULL_DEBUG ("Leaving CamelMaildirFolder::delete_messages\n");
	return rv;
}

/**
 * CamelMaildirFolder::get_message: get a message from maildir
 * @folder: the folder object
 * @number: number of the message within the folder
 *
 * Return value: the message, NULL on error
 */
static CamelMimeMessage *
_get_message (CamelFolder *folder, gint number, CamelException *ex)
{
	CamelMaildirFolder *maildir_folder = CAMEL_MAILDIR_FOLDER(folder);
	DIR *dir_handle;
	struct dirent *dir_entry;
	CamelStream *stream;
	CamelMimeMessage *message = NULL;
	const gchar *maildir;
	gchar *curdir, *file = NULL;
	gint count = -1;

	CAMEL_LOG_FULL_DEBUG ("Entering CamelMaildirFolder::get_message\n");
	g_assert(folder);

	/* Check if the folder exists */
	if (!camel_folder_exists (folder, ex)) return NULL;

	maildir = maildir_folder->directory_path;

	CAMEL_LOG_FULL_DEBUG ("CamelMaildirFolder::get_message: "
			      "getting message #%d from %s\n", number, maildir);

	/* Count until the desired message is reached */
	curdir = g_strconcat (maildir, G_DIR_SEPARATOR_S, "cur", NULL);
	if ((dir_handle = _xopendir (curdir))) {
		while ((count < number) && (dir_entry = readdir (dir_handle)))
			if (dir_entry->d_name[0] != '.') count++;

		if (count == number)
			file = g_strconcat (curdir, G_DIR_SEPARATOR_S,
					    dir_entry->d_name, NULL);

		closedir (dir_handle);
	}
	g_free (curdir);
	if (!file) return NULL;

	/* Create the message object */
	message = camel_mime_message_new ();
	stream = camel_stream_fs_new_with_name (file, CAMEL_STREAM_FS_READ);

	if (!message || !stream) {
		g_free (file);
		if (stream) gtk_object_unref (GTK_OBJECT (stream));
		if (message) gtk_object_unref (GTK_OBJECT (message));
		return NULL;
	}

	camel_data_wrapper_construct_from_stream (CAMEL_DATA_WRAPPER (message),
						  stream);
	gtk_object_unref (GTK_OBJECT (stream));
	gtk_object_set_data_full (GTK_OBJECT (message),
				  "fullpath", file, g_free);

	CAMEL_LOG_FULL_DEBUG ("CamelMaildirFolder::get_message: "
			      "message %p created from %s\n", message, file);
	CAMEL_LOG_FULL_DEBUG ("Leaving CamelMaildirFolder::get_message\n");
	return message;
}

/**
 * CamelMaildirFolder::get_message_count: count messages in maildir
 * @folder:  the folder object
 *
 * Returns the number of messages in the maildir folder. New messages
 * are included in this count. 
 *
 * Return value: number of messages in the maildir, -1 on error
 */
static gint
_get_message_count (CamelFolder *folder, CamelException *ex)
{
	CamelMaildirFolder *maildir_folder = CAMEL_MAILDIR_FOLDER(folder);
	const gchar *maildir;
	gchar *newdir, *curdir, *newfile, *curfile;
	DIR *dir_handle;
	struct dirent *dir_entry;
	guint count = 0;

	CAMEL_LOG_FULL_DEBUG ("Entering "
			      "CamelMaildirFolder::get_message_count\n");
	g_assert(folder);

	/* check if the maildir exists */
	if (!camel_folder_exists (folder, ex)) return -1;

	maildir = maildir_folder->directory_path;

	newdir = g_strconcat (maildir, G_DIR_SEPARATOR_S, "new", NULL);
	curdir = g_strconcat (maildir, G_DIR_SEPARATOR_S, "cur", NULL);

	/* Check new messages */
	CAMEL_LOG_FULL_DEBUG ("CamelMaildirFolder::get_message_count: "
			      "getting new messages from %s\n", newdir);
	if ((dir_handle = _xopendir (newdir))) {
		while ((dir_entry = readdir (dir_handle))) {
			if (dir_entry->d_name[0] == '.') continue;
			newfile = g_strconcat (newdir, G_DIR_SEPARATOR_S,
					       dir_entry->d_name, NULL);
			curfile = g_strconcat (curdir, G_DIR_SEPARATOR_S,
					       dir_entry->d_name, ":2,", NULL);
			
			_xrename (newfile, curfile);
			
			g_free (curfile);
			g_free (newfile);
		}
		closedir (dir_handle);
	}

	/* Count messages */
	CAMEL_LOG_FULL_DEBUG ("CamelMaildirFolder::get_message_count: "
			      "counting messages in %s\n", curdir);
	if ((dir_handle = _xopendir (curdir))) {
		while ((dir_entry = readdir (dir_handle)))
			if (dir_entry->d_name[0] != '.') count++;
		closedir (dir_handle);
	}

	g_free (curdir);
	g_free (newdir);

	CAMEL_LOG_FULL_DEBUG ("CamelMaildirFolder::get_message_count: "
			      " found %d messages\n", count);
	CAMEL_LOG_FULL_DEBUG ("Leaving "
			      "CamelMaildirFolder::get_message_count\n");
	return count;
}




/**
 * CamelMaildirFolder::expunge: expunge messages marked as deleted
 * @folder:  the folder object
 *
 * Physically deletes the messages marked as deleted in the folder.
 */
static void
_expunge (CamelFolder *folder, CamelException *ex)
{
	CamelMimeMessage *message;
	GList *node;
	gchar *fullpath;

	CAMEL_LOG_FULL_DEBUG ("Entering CamelMaildirFolder::expunge\n");
	g_assert(folder);

	/* expunge messages marked for deletion */
	for (node = folder->message_list; node; node = g_list_next(node)) {
		message = CAMEL_MIME_MESSAGE (node->data);
		if (!message) {
			CAMEL_LOG_WARNING ("CamelMaildirFolder::expunge: "
					   "null message in node %p\n", node);
			continue;
		}
				
		if (camel_mime_message_get_flag (message, "DELETED")) {
			CAMEL_LOG_FULL_DEBUG ("CamelMaildirFolder::expunge: "
					      "expunging message #%d\n",
					      message->message_number);

			/* expunge the message */
			fullpath = gtk_object_get_data (GTK_OBJECT (message),
							"fullpath");
			CAMEL_LOG_FULL_DEBUG ("CamelMaildirFolder::expunge: "
					      "message fullpath is %s\n",
					      fullpath);

			if (_xunlink (fullpath))
				message->expunged = TRUE;
		} else {
			CAMEL_LOG_FULL_DEBUG ("CamelMaildirFolder::expunge: "
					      "skipping message #%d\n",
					      message->message_number);
		}
	}
	
	CAMEL_LOG_FULL_DEBUG ("Leaving CamelMaildirFolder::expunge\n");
}




/**
 * CamelMaildirFolder::list_subfolders: return a list of subfolders
 * @folder:  the folder object
 *
 * Returns the names of the maildir subfolders in a list.
 *
 * Return value: list of subfolder names
 */
static GList *
_list_subfolders (CamelFolder *folder, CamelException *ex)
{
	CamelMaildirFolder *maildir_folder = CAMEL_MAILDIR_FOLDER (folder);
	const gchar *maildir;
	gchar *subdir;
	struct stat statbuf;
	struct dirent *dir_entry;
	DIR *dir_handle;
	GList *subfolders = NULL;

	CAMEL_LOG_FULL_DEBUG ("Entering CamelMaildirFolder::list_subfolders\n");
	g_assert (folder);

	/* check if the maildir exists */
	if (!camel_folder_exists (folder, ex)) return NULL;

	/* scan through the maildir toplevel directory */
	maildir = maildir_folder->directory_path;
	if ((dir_handle = _xopendir (maildir))) {
		while ((dir_entry = readdir (dir_handle))) {
			if (dir_entry->d_name[0] == '.') continue;
			if (strcmp (dir_entry->d_name, "new") == 0) continue;
			if (strcmp (dir_entry->d_name, "cur") == 0) continue;
			if (strcmp (dir_entry->d_name, "tmp") == 0) continue;

			subdir = g_strconcat (maildir, G_DIR_SEPARATOR_S,
					      dir_entry->d_name, NULL);
			
			if (_xstat (subdir, &statbuf)
			    && S_ISDIR (statbuf.st_mode))
				subfolders =
					g_list_append (
						subfolders,
						g_strdup (dir_entry->d_name));
			
			g_free (subdir);
		}
		closedir (dir_handle);
	}

	CAMEL_LOG_FULL_DEBUG ("Leaving CamelMaildirFolder::list_subfolders\n");
	return subfolders;
}







/*
 * fs utility function 
 *
 */

static DIR *
_xopendir (const gchar *path)
{
	DIR *handle;
	g_assert (path);

	handle = opendir (path);
	if (!handle) {
		CAMEL_LOG_WARNING ("ERROR: opendir (%s);\n", path);
		CAMEL_LOG_FULL_DEBUG ("  Full error text is: (%d) %s\n",
				      errno, strerror(errno));
	}

	return handle;
}

static gboolean
_xstat (const gchar *path, struct stat *buf)
{
	gint stat_error;
	g_assert (path);
	g_assert (buf);

	stat_error = stat (path, buf);
	if (stat_error == 0) {
		return TRUE;
	} else if (errno == ENOENT) {
		buf->st_mode = 0;
		return TRUE;
	} else {
		CAMEL_LOG_WARNING ("ERROR: stat (%s, %p);\n", path, buf);
		CAMEL_LOG_FULL_DEBUG ("  Full error text is: (%d) %s\n",
				      errno, strerror(errno));
		return FALSE;
	}
}

static gboolean
_xmkdir (const gchar *path)
{
	g_assert (path);

	if (mkdir (path, S_IRWXU) == -1) {
		CAMEL_LOG_WARNING ("ERROR: mkdir (%s, S_IRWXU);\n", path);
		CAMEL_LOG_FULL_DEBUG ("  Full error text is: (%d) %s\n",
				      errno, strerror(errno));
		return FALSE;
	} 

	return TRUE;
}

static gboolean
_xrename (const gchar *from, const gchar *to)
{
	g_assert (from);
	g_assert (to);

	if (rename (from, to) == 0) {
		return TRUE;
	} else {
		CAMEL_LOG_WARNING ("ERROR: rename (%s, %s);\n", from, to);
		CAMEL_LOG_FULL_DEBUG ("  Full error text is: (%d) %s\n",
				      errno, strerror(errno));
		return FALSE;
	}
}

static gboolean
_xunlink (const gchar *path)
{
	g_assert (path);

	if (unlink (path) == 0) {
		return TRUE;
	} else if (errno == ENOENT) {
		return TRUE;
	} else {
		CAMEL_LOG_WARNING ("ERROR: unlink (%s);\n", path);
		CAMEL_LOG_FULL_DEBUG ("  Full error text is: (%d) %s\n",
				      errno, strerror(errno));
		return FALSE;
	}
}

static gboolean
_xrmdir (const gchar *path)
{
	DIR *dir_handle;
	struct dirent *dir_entry;
	gchar *file;
	struct stat statbuf;
	g_assert (path);

	dir_handle = opendir (path);
	if (!dir_handle && errno == ENOENT) {
		return TRUE;
	} else if (!dir_handle) {
		CAMEL_LOG_WARNING ("ERROR: opendir (%s);\n", path);
		CAMEL_LOG_FULL_DEBUG ("  Full error text is: (%d) %s\n",
				      errno, strerror(errno));
		return FALSE;
	}

	while ((dir_entry = readdir (dir_handle))) {
		file = g_strconcat (path, G_DIR_SEPARATOR_S, dir_entry->d_name,
				    NULL);
		if (_xstat (file, &statbuf) && S_ISREG (statbuf.st_mode))
			_xunlink (file);
		g_free (file);
	}

	closedir (dir_handle);

	if (rmdir (path) == 0) {
		return TRUE;
	} else if (errno == ENOENT) {
		return TRUE;
	} else {
		CAMEL_LOG_WARNING ("ERROR: rmdir (%s);\n", path);
		CAMEL_LOG_FULL_DEBUG ("  Full error text is: (%d) %s\n",
				      errno, strerror(errno));
		return FALSE;
	} 
}

/** *** **/

