/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* camel-folder.c: Abstract class for an email folder */

/*
 * Author:
 *  Bertrand Guiheneuf <bertrand@helixcode.com>
 *
 * Copyright 1999, 2000 Helix Code, Inc. (http://www.helixcode.com)
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
#include <string.h>
#include "camel-folder.h"
#include "camel-exception.h"
#include "camel-store.h"
#include "camel-mime-message.h"
#include "string-utils.h"

static CamelObjectClass *parent_class = NULL;

/* Returns the class for a CamelFolder */
#define CF_CLASS(so) CAMEL_FOLDER_CLASS (GTK_OBJECT (so)->klass)


enum SIGNALS {
	FOLDER_CHANGED,
	LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = { 0 };


static void init (CamelFolder *folder, CamelStore *parent_store,
		  CamelFolder *parent_folder, const gchar *name,
		  gchar separator, CamelException *ex);
static void finalize (GtkObject *object);


static void folder_open (CamelFolder *folder, CamelFolderOpenMode mode,
			 CamelException *ex);
static void folder_close (CamelFolder *folder, gboolean expunge,
			  CamelException *ex);

static const gchar *get_name (CamelFolder *folder);
static const gchar *get_full_name (CamelFolder *folder);


static gboolean can_hold_folders (CamelFolder *folder);
static gboolean can_hold_messages (CamelFolder *folder);
static gboolean exists (CamelFolder *folder, CamelException *ex);
static gboolean is_open (CamelFolder *folder);
static guint32 get_permanent_flags (CamelFolder *folder, CamelException *ex);
static CamelFolderOpenMode get_mode (CamelFolder *folder, CamelException *ex);


static gboolean create (CamelFolder *folder, CamelException *ex);
static gboolean delete (CamelFolder *folder, gboolean recurse,
			CamelException *ex);


static GList *list_subfolders         (CamelFolder *folder,
				       CamelException *ex);
static CamelFolder *get_subfolder     (CamelFolder *folder,
				       const gchar *folder_name,
				       CamelException *ex);
static CamelFolder *get_parent_folder (CamelFolder *folder,
				       CamelException *ex);
static CamelStore *get_parent_store   (CamelFolder *folder,
				       CamelException *ex);


static gboolean has_message_number_capability  (CamelFolder *folder);
static CamelMimeMessage *get_message_by_number (CamelFolder *folder,
						gint number,
						CamelException *ex);
static void delete_message_by_number           (CamelFolder *folder,
						gint number,
						CamelException *ex);
static gint get_message_count                  (CamelFolder *folder,
						CamelException *ex);


static gboolean delete_messages (CamelFolder *folder,
				 CamelException *ex);
static void expunge             (CamelFolder *folder,
				 CamelException *ex);


static void append_message (CamelFolder *folder, CamelMimeMessage *message,
			    CamelException *ex);


static GList            *get_uid_list        (CamelFolder *folder,
					      CamelException *ex);
static const gchar      *get_message_uid     (CamelFolder *folder,
					      CamelMimeMessage *message,
					      CamelException *ex);
static CamelMimeMessage *get_message_by_uid  (CamelFolder *folder,
					      const gchar *uid,
					      CamelException *ex);
static void delete_message_by_uid            (CamelFolder *folder,
					      const gchar *uid,
					      CamelException *ex);

static GPtrArray *get_message_info   (CamelFolder *folder,
				      int first, int count);
static GPtrArray *get_subfolder_info (CamelFolder *folder,
				      int first, int count);

static const CamelMessageInfo *summary_get_by_uid (CamelFolder *folder,
						   const char *uid);

static GList *search_by_expression (CamelFolder *folder, const char *exp,
				    CamelException *ex);

static void
camel_folder_class_init (CamelFolderClass *camel_folder_class)
{
	GtkObjectClass *gtk_object_class =
		GTK_OBJECT_CLASS (camel_folder_class);

	parent_class = gtk_type_class (camel_object_get_type ());

	/* virtual method definition */
	camel_folder_class->init = init;
	camel_folder_class->open = folder_open;
	camel_folder_class->close = folder_close;
	camel_folder_class->get_name = get_name;
	camel_folder_class->get_full_name = get_full_name;
	camel_folder_class->can_hold_folders = can_hold_folders;
	camel_folder_class->can_hold_messages = can_hold_messages;
	camel_folder_class->exists = exists;
	camel_folder_class->is_open = is_open;
	camel_folder_class->get_subfolder = get_subfolder;
	camel_folder_class->create = create;
	camel_folder_class->delete = delete;
	camel_folder_class->delete_messages = delete_messages;
	camel_folder_class->get_parent_folder = get_parent_folder;
	camel_folder_class->get_parent_store = get_parent_store;
	camel_folder_class->get_mode = get_mode;
	camel_folder_class->list_subfolders = list_subfolders;
	camel_folder_class->expunge = expunge;
	camel_folder_class->has_message_number_capability =
		has_message_number_capability;
	camel_folder_class->get_message_by_number = get_message_by_number;
	camel_folder_class->delete_message_by_number =
		delete_message_by_number;
	camel_folder_class->get_message_count = get_message_count;
	camel_folder_class->append_message = append_message;
	camel_folder_class->get_permanent_flags = get_permanent_flags;
	camel_folder_class->get_message_uid = get_message_uid;
	camel_folder_class->get_message_by_uid = get_message_by_uid;
	camel_folder_class->delete_message_by_uid = delete_message_by_uid;
	camel_folder_class->get_uid_list = get_uid_list;
	camel_folder_class->search_by_expression = search_by_expression;
	camel_folder_class->get_subfolder_info = get_subfolder_info;
	camel_folder_class->get_message_info = get_message_info;
	camel_folder_class->summary_get_by_uid = summary_get_by_uid;

	/* virtual method overload */
	gtk_object_class->finalize = finalize;

        signals[FOLDER_CHANGED] =
                gtk_signal_new ("folder_changed",
                                GTK_RUN_LAST,
                                gtk_object_class->type,
                                GTK_SIGNAL_OFFSET (CamelFolderClass,
						   folder_changed),
                                gtk_marshal_NONE__INT,
                                GTK_TYPE_NONE, 1, GTK_TYPE_INT);

        gtk_object_class_add_signals (gtk_object_class, signals, LAST_SIGNAL);

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

		camel_folder_type = gtk_type_unique (camel_object_get_type (),
						     &camel_folder_info);
	}

	return camel_folder_type;
}


static void
finalize (GtkObject *object)
{
	CamelFolder *camel_folder = CAMEL_FOLDER (object);

	g_free (camel_folder->name);
	g_free (camel_folder->full_name);

	if (camel_folder->parent_store)
		gtk_object_unref (GTK_OBJECT (camel_folder->parent_store));
	if (camel_folder->parent_folder)
		gtk_object_unref (GTK_OBJECT (camel_folder->parent_folder));

	GTK_OBJECT_CLASS (parent_class)->finalize (object);
}


/**
 * init: init the folder
 * @folder: folder object to initialize
 * @parent_store: parent store object of the folder
 * @parent_folder: parent folder of the folder (may be NULL)
 * @name: (short) name of the folder
 * @separator: separator between the parent folder name and this name
 * @ex: a CamelException
 *
 * Initalizes the folder by setting the parent store, parent folder,
 * and name.
 **/
static void
init (CamelFolder *folder, CamelStore *parent_store,
      CamelFolder *parent_folder, const gchar *name,
      gchar separator, CamelException *ex)
{
	gchar *full_name;
	const gchar *parent_full_name;

	g_return_if_fail (CAMEL_IS_FOLDER (folder));
	g_return_if_fail (CAMEL_IS_STORE (parent_store));
	g_return_if_fail (parent_folder == NULL || CAMEL_IS_FOLDER (parent_folder));
	g_return_if_fail (folder->parent_store == NULL);

	folder->parent_store = parent_store;
	gtk_object_ref (GTK_OBJECT (parent_store));

	folder->parent_folder = parent_folder;
	if (parent_folder)
		gtk_object_ref (GTK_OBJECT (parent_folder));

	folder->open_mode = FOLDER_OPEN_UNKNOWN;
	folder->open_state = FOLDER_CLOSE;
	folder->separator = separator;

	/* if the folder already has a name, free it */
	g_free (folder->name);
	g_free (folder->full_name);

	/* set those fields to NULL now, so that if an
	   exception occurs, they will be set anyway */
	folder->name = NULL;
	folder->full_name = NULL;

	if (folder->parent_folder) {
		parent_full_name =
			camel_folder_get_full_name (folder->parent_folder);

		full_name = g_strdup_printf ("%s%c%s", parent_full_name,
					     folder->separator, name);
	} else {
		full_name = g_strdup_printf ("%c%s", folder->separator, name);
	}

	folder->name = g_strdup (name);
	folder->full_name = full_name;
}


static void
folder_open (CamelFolder *folder, CamelFolderOpenMode mode, CamelException *ex)
{
	if (folder->open_state == FOLDER_OPEN) {
		camel_exception_set (ex,
				     CAMEL_EXCEPTION_FOLDER_INVALID_STATE,
				     "folder is already open");
		return;
	}

  	folder->open_state = FOLDER_OPEN;
  	folder->open_mode = mode;
}

/**
 * camel_folder_open:
 * @folder: The folder object
 * @mode: open mode (R/W/RW ?)
 * @ex: exception object
 *
 * Open a folder in a given mode.
 **/
void
camel_folder_open (CamelFolder *folder, CamelFolderOpenMode mode,
		   CamelException *ex)
{
	g_return_if_fail (CAMEL_IS_FOLDER (folder));
	g_return_if_fail (!camel_folder_is_open (folder));

	CF_CLASS (folder)->open (folder, mode, ex);
}


static void
folder_close (CamelFolder *folder, gboolean expunge, CamelException *ex)
{
	folder->open_state = FOLDER_CLOSE;
}

/**
 * camel_folder_close:
 * @folder: The folder object
 * @expunge: whether or not to expunge deleted messages
 * @ex: exception object
 *
 * Put a folder in its closed state, and possibly expunge the messages
 * flagged for deletion.
 **/
void
camel_folder_close (CamelFolder *folder, gboolean expunge,
		    CamelException *ex)
{
	g_return_if_fail (CAMEL_IS_FOLDER (folder));
	g_return_if_fail (camel_folder_is_open (folder));

	CF_CLASS (folder)->close (folder, expunge, ex);
}


static const gchar *
get_name (CamelFolder *folder)
{
	return folder->name;
}

/**
 * camel_folder_get_name:
 * @folder: a folder
 *
 * Get the (short) name of the folder. The fully qualified name
 * can be obtained with the get_full_name method.
 *
 * Return value: name of the folder
 **/
const gchar *
camel_folder_get_name (CamelFolder *folder)
{
	g_return_val_if_fail (CAMEL_IS_FOLDER (folder), NULL);

	return CF_CLASS (folder)->get_name (folder);
}


static const gchar *
get_full_name (CamelFolder *folder)
{
	return folder->full_name;
}

/**
 * camel_folder_get_full_name:
 * @folder: a folder
 *
 * Get the (full) name of the folder.
 *
 * Return value: full name of the folder
 **/
const gchar *
camel_folder_get_full_name (CamelFolder *folder)
{
	g_return_val_if_fail (CAMEL_IS_FOLDER (folder), NULL);

	return CF_CLASS (folder)->get_full_name (folder);
}


static gboolean
can_hold_folders (CamelFolder *folder)
{
	return folder->can_hold_folders;
}

static gboolean
can_hold_messages (CamelFolder *folder)
{
	return folder->can_hold_messages;
}


static gboolean
exists (CamelFolder *folder, CamelException *ex)
{
	return FALSE;
}

/**
 * camel_folder_exists:
 * @folder: folder object
 * @ex: a CamelException
 *
 * Test if a folder exists in a store. A CamelFolder can be created
 * without physically existing in a store. In that case, use
 * CamelFolder::create to create it.
 *
 * Return value: whether or not the folder exists
 **/
gboolean
camel_folder_exists (CamelFolder *folder, CamelException *ex)
{
	g_return_val_if_fail (CAMEL_IS_FOLDER (folder), FALSE);

	return CF_CLASS (folder)->exists (folder, ex);
}


static gboolean
is_open (CamelFolder *folder)
{
	return folder->open_state == FOLDER_OPEN;
}

/**
 * camel_folder_is_open:
 * @folder: a folder object
 *
 * Tests if a folder is open. If not open it can be opened with
 * CamelFolder::open
 *
 * Return value: whether or not the folder is open
 **/
gboolean
camel_folder_is_open (CamelFolder *folder)
{
	g_return_val_if_fail (CAMEL_IS_FOLDER (folder), FALSE);

	return CF_CLASS (folder)->is_open (folder);
}


static CamelFolder *
get_subfolder (CamelFolder *folder, const gchar *folder_name,
	       CamelException *ex)
{
	CamelFolder *new_folder;
	gchar *full_name;
	const gchar *current_folder_full_name;

	g_return_val_if_fail (CAMEL_IS_STORE (folder->parent_store), NULL);

	current_folder_full_name = camel_folder_get_full_name (folder);

	full_name = g_strdup_printf ("%s%c%s", current_folder_full_name,
				     folder->separator, folder_name);
	new_folder = camel_store_get_folder (folder->parent_store,
					     full_name, ex);
	g_free (full_name);

	return new_folder;
}

/**
 * camel_folder_get_subfolder:
 * @folder: a folder
 * @folder_name: subfolder path
 * @ex: a CamelException
 *
 * This method returns a folder object. This folder is a subfolder of
 * the given folder. It is an error to ask for a folder whose name begins
 * with the folder separator character.
 *
 * Return value: the requested folder, or %NULL if the subfolder object
 * could not be obtained
 **/
CamelFolder *
camel_folder_get_subfolder (CamelFolder *folder, const gchar *folder_name,
			    CamelException *ex)
{
	g_return_val_if_fail (CAMEL_IS_FOLDER (folder), NULL);
	g_return_val_if_fail (camel_folder_is_open (folder), NULL);
	g_return_val_if_fail (folder_name != NULL, NULL);

	return CF_CLASS (folder)->get_subfolder (folder, folder_name, ex);
}


/**
 * create: creates a folder on its store
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
create (CamelFolder *folder, CamelException *ex)
{
	CamelFolder *parent;

	g_return_val_if_fail (folder->parent_store != NULL, FALSE);
	g_return_val_if_fail (folder->name != NULL, FALSE);

	/* if the folder already exists on the store, do nothing and return true */
	if (CF_CLASS (folder)->exists (folder, ex))
		return TRUE;

	if (folder->parent_folder) {
		camel_folder_create (folder->parent_folder, ex);
		if (camel_exception_get_id (ex))
			return FALSE;
	} else if (folder->full_name) {
		char *slash, *prefix;

		slash = strrchr(folder->full_name, folder->separator);
		if (slash && slash != folder->full_name) {
			prefix = g_strndup(folder->full_name, slash-folder->full_name);
			parent = camel_store_get_folder (folder->parent_store, prefix, ex);
			camel_folder_create (parent, ex);
			if (camel_exception_get_id (ex))
				return FALSE;
		}
	}
	return TRUE;
}


/**
 * camel_folder_create: create the folder object on the physical store
 * @folder: folder object to create
 * @ex: a CamelException
 *
 * This routine physically creates the folder on the store. Having
 * created the object does not mean the folder physically exists. If
 * it does not exist, this routine will create it. If the folder full
 * name contains more than one level of hierarchy, all folders between
 * the current folder and the last folder name will be created if not
 * existing.
 *
 * Return value: whether or not the operation succeeded
 **/
gboolean
camel_folder_create (CamelFolder *folder, CamelException *ex)
{
	g_return_val_if_fail (CAMEL_IS_FOLDER (folder), FALSE);
	g_return_val_if_fail (!camel_folder_is_open (folder), FALSE);

	return CF_CLASS (folder)->create (folder, ex);
}


/**
 * delete: delete folder
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
delete (CamelFolder *folder, gboolean recurse, CamelException *ex)
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
 * @recurse: %TRUE if subfolders must be deleted
 * @ex: a CamelException
 *
 * Delete a folder. All messages in the folder are deleted before the
 * folder is deleted. When @recurse is %TRUE, all subfolders are
 * deleted too. When @recurse is %FALSE and folder contains
 * subfolders, all messages are deleted, but folder deletion fails.
 *
 * Return value: whether or not deletion was successful
 **/
gboolean
camel_folder_delete (CamelFolder *folder, gboolean recurse, CamelException *ex)
{
	g_return_val_if_fail (CAMEL_IS_FOLDER (folder), FALSE);
	g_return_val_if_fail (!camel_folder_is_open (folder), FALSE);

	return CF_CLASS (folder)->delete (folder, recurse, ex);
}


static gboolean
delete_messages (CamelFolder *folder, CamelException *ex)
{
	g_warning ("CamelFolder::delete_messages not implemented for `%s'",
		   gtk_type_name (GTK_OBJECT_TYPE (folder)));
	return FALSE;
}

/**
 * camel_folder_delete_messages: delete all messages in the folder
 * @folder: folder
 * @ex: a CamelException
 *
 * Delete all messages stored in a folder.
 *
 * Return value: whether or not the messages could be deleted
 **/
gboolean
camel_folder_delete_messages (CamelFolder *folder, CamelException *ex)
{
	g_return_val_if_fail (CAMEL_IS_FOLDER (folder), FALSE);
	g_return_val_if_fail (!camel_folder_is_open (folder), FALSE);

	return CF_CLASS (folder)->delete_messages (folder, ex);
}


static CamelFolder *
get_parent_folder (CamelFolder *folder, CamelException *ex)
{
	return folder->parent_folder;
}

/**
 * camel_folder_get_parent_folder:
 * @folder: folder to get the parent of
 * @ex: a CamelException
 *
 * Return value: the folder's parent
 **/
CamelFolder *
camel_folder_get_parent_folder (CamelFolder *folder, CamelException *ex)
{
	g_return_val_if_fail (CAMEL_IS_FOLDER (folder), NULL);

	return CF_CLASS (folder)->get_parent_folder (folder, ex);
}


static CamelStore *
get_parent_store (CamelFolder *folder, CamelException *ex)
{
	return folder->parent_store;
}

/**
 * camel_folder_get_parent_store:
 * @folder: folder to get the parent of
 * @ex: a CamelException
 *
 * Return value: the parent store of the folder.
 **/
CamelStore *
camel_folder_get_parent_store (CamelFolder *folder, CamelException *ex)
{
	g_return_val_if_fail (CAMEL_IS_FOLDER (folder), NULL);

	return CF_CLASS (folder)->get_parent_store (folder, ex);
}


static CamelFolderOpenMode
get_mode (CamelFolder *folder, CamelException *ex)
{
	return folder->open_mode;
}

/**
 * camel_folder_get_mode:
 * @folder: a folder
 * @ex: a CamelException
 *
 * Return value: the open mode of the folder
 **/
CamelFolderOpenMode
camel_folder_get_mode (CamelFolder *folder, CamelException *ex)
{
	g_return_val_if_fail (CAMEL_IS_FOLDER (folder), -1);

	return CF_CLASS (folder)->get_mode (folder, ex);
}


static GList *
list_subfolders (CamelFolder *folder, CamelException *ex)
{
	g_warning ("CamelFolder::list_folders not implemented for `%s'",
		   gtk_type_name (GTK_OBJECT_TYPE (folder)));
	return NULL;
}

/**
 * camel_folder_list_subfolders:
 * @folder: the folder
 * @ex: a CamelException
 *
 * List subfolders in a folder.
 *
 * Return value: list of subfolder names
 **/
GList *
camel_folder_list_subfolders (CamelFolder *folder, CamelException *ex)
{
	g_return_val_if_fail (CAMEL_IS_FOLDER (folder), NULL);
	g_return_val_if_fail (camel_folder_is_open (folder), NULL);

	return CF_CLASS (folder)->list_subfolders (folder, ex);
}


static void
expunge (CamelFolder *folder, CamelException *ex)
{
	g_warning ("CamelFolder::expunge not implemented for `%s'",
		   gtk_type_name (GTK_OBJECT_TYPE (folder)));
}


/**
 * camel_folder_expunge:
 * @folder: the folder
 * @ex: a CamelException
 *
 * Delete messages which have been marked as "DELETED"
 **/
void
camel_folder_expunge (CamelFolder *folder, CamelException *ex)
{
	g_return_if_fail (CAMEL_IS_FOLDER (folder));
	g_return_if_fail (camel_folder_is_open (folder));

	return CF_CLASS (folder)->expunge (folder, ex);
}


static gboolean
has_message_number_capability (CamelFolder *folder)
{
	g_warning ("CamelFolder::has_message_number_capability not "
		   "implemented for `%s'",
		   gtk_type_name (GTK_OBJECT_TYPE (folder)));
	return FALSE;
}

/**
 * camel_folder_has_message_number_capability:
 * @folder: folder to test
 *
 * Test if the message in this folder can be obtained via the
 * get_by_number method. Usually, when the folder has the UID
 * capability, messages should be referred to by their UID rather than
 * by their number as the UID is more reliable.
 *
 * Return value: whether or not the folder supports message numbers
 **/
gboolean
camel_folder_has_message_number_capability (CamelFolder *folder)
{
	g_return_val_if_fail (CAMEL_IS_FOLDER (folder), FALSE);

	return CF_CLASS (folder)->has_message_number_capability (folder);
}


static CamelMimeMessage *
get_message_by_number (CamelFolder *folder, gint number, CamelException *ex)
{
	g_warning ("CamelFolder::get_message_by_number not implemented "
		   "for `%s'", gtk_type_name (GTK_OBJECT_TYPE (folder)));
	return NULL;
}

/**
 * camel_folder_get_message_by_number:
 * @folder: a CamelFolder object
 * @number: the number of the message within the folder.
 * @ex: a CamelException
 *
 * Return the message corresponding to that number within the folder.
 *
 * Return value: A pointer on the corresponding message, or %NULL if
 * no corresponding message exists
 **/
CamelMimeMessage *
camel_folder_get_message_by_number (CamelFolder *folder, gint number,
				    CamelException *ex)
{
	g_return_val_if_fail (CAMEL_IS_FOLDER (folder), NULL);
	g_return_val_if_fail (camel_folder_is_open (folder), NULL);
	g_return_val_if_fail (camel_folder_has_message_number_capability (folder), NULL);

	return CF_CLASS (folder)->get_message_by_number (folder, number, ex);
}


static void
delete_message_by_number (CamelFolder *folder, gint number,
			  CamelException *ex)
{
	g_warning ("CamelFolder::delete_message_by_number not implemented "
		   "for `%s'", gtk_type_name (GTK_OBJECT_TYPE (folder)));
}

/**
 * camel_folder_delete_message_by_number:
 * @folder: a CamelFolder object
 * @number: the number of the message within the folder.
 * @ex: a CamelException
 *
 * Delete the message corresponding to that number within the folder.
 **/
void
camel_folder_delete_message_by_number (CamelFolder *folder, gint number,
				       CamelException *ex)
{
	g_return_if_fail (CAMEL_IS_FOLDER (folder));
	g_return_if_fail (camel_folder_is_open (folder));
	g_return_if_fail (camel_folder_has_message_number_capability (folder));

	return CF_CLASS (folder)->delete_message_by_number (folder, number,
							    ex);
}


static gint
get_message_count (CamelFolder *folder, CamelException *ex)
{
	g_warning ("CamelFolder::get_message_count not implemented "
		   "for `%s'", gtk_type_name (GTK_OBJECT_TYPE (folder)));
	return -1;
}

/**
 * camel_folder_get_message_count:
 * @folder: A CamelFolder object
 * @ex: a CamelException
 *
 * Return value: the number of messages in the folder, or -1 if unknown.
 **/
gint
camel_folder_get_message_count (CamelFolder *folder, CamelException *ex)
{
	g_return_val_if_fail (CAMEL_IS_FOLDER (folder), -1);
	g_return_val_if_fail (camel_folder_is_open (folder), -1);

	return CF_CLASS (folder)->get_message_count (folder, ex);
}


static void
append_message (CamelFolder *folder, CamelMimeMessage *message,
		CamelException *ex)
{
	g_warning ("CamelFolder::append_message not implemented for `%s'",
		   gtk_type_name (GTK_OBJECT_TYPE (folder)));
	return;

}

/**
 * camel_folder_append_message: add a message to a folder
 * @folder: folder object to add the message to
 * @message: message object
 * @ex: exception object
 *
 * Add a message to a folder.
 **/
void
camel_folder_append_message (CamelFolder *folder,
			     CamelMimeMessage *message,
			     CamelException *ex)
{
	g_return_if_fail (CAMEL_IS_FOLDER (folder));
	g_return_if_fail (camel_folder_is_open (folder));

	CF_CLASS (folder)->append_message (folder, message, ex);
}


static guint32
get_permanent_flags (CamelFolder *folder, CamelException *ex)
{
	return folder->permanent_flags;
}

guint32
camel_folder_get_permanent_flags (CamelFolder *folder, CamelException *ex)
{
	g_return_val_if_fail (CAMEL_IS_FOLDER (folder), 0);

	return CF_CLASS (folder)->get_permanent_flags (folder, ex);
}


static GPtrArray *
get_subfolder_info (CamelFolder *folder, int first, int count)
{
	g_warning ("CamelFolder::get_subfolder_info not implemented for `%s'",
		   gtk_type_name (GTK_OBJECT_TYPE (folder)));
	return NULL;
}

/**
 * camel_folder_summary_get_subfolder_info:
 * @summary: a summary
 * @first: the index of the first subfolder to return information for
 * (starting from 0)
 * @count: the number of subfolders to return information for
 *
 * Returns an array of pointers to CamelFolderInfo objects. The caller
 * must free the array when it is done with it, but should not modify
 * the elements.
 *
 * Return value: an array containing information about the subfolders.
 **/
GPtrArray *
camel_folder_summary_get_subfolder_info (CamelFolder *folder,
					 int first, int count)
{
	g_assert (folder != NULL);
	return CF_CLASS (folder)->get_subfolder_info (folder, first, count);
}


static GPtrArray *
get_message_info (CamelFolder *folder, int first, int count)
{
	g_warning ("CamelFolder::get_message_info not implemented for `%s'",
		   gtk_type_name (GTK_OBJECT_TYPE (folder)));
	return NULL;
}

/**
 * camel_folder_summary_get_message_info:
 * @folder: a camel folder
 * @first: the index of the first message to return information for
 * (starting from 0)
 * @count: the number of messages to return information for
 *
 * Returns an array of pointers to CamelMessageInfo objects. The caller
 * must free the array when it is done with it, but should not modify
 * the elements.
 *
 * Return value: an array containing information about the messages.
 **/
GPtrArray *
camel_folder_summary_get_message_info (CamelFolder *folder,
				       int first, int count)
{
	g_assert (folder != NULL);
	return CF_CLASS (folder)->get_message_info (folder, first, count);
}


static const CamelMessageInfo *
summary_get_by_uid (CamelFolder *folder, const char *uid)
{
	g_warning ("CamelFolder::summary_get_by_uid not implemented for `%s'",
		   gtk_type_name (GTK_OBJECT_TYPE (folder)));
	return NULL;
}

/**
 * camel_folder_summary_get_by_uid:
 * @folder: a CamelFolder
 * @uid: the uid of a message
 *
 * Return value: the summary information for the indicated message
 **/
const CamelMessageInfo *
camel_folder_summary_get_by_uid (CamelFolder *folder, const char *uid)
{
	g_return_val_if_fail (CAMEL_IS_FOLDER (folder), NULL);
	g_return_val_if_fail (uid != NULL, NULL);

	return CF_CLASS (folder)->summary_get_by_uid (folder, uid);
}


/* TODO: is this function required anyway? */
gboolean
camel_folder_has_summary_capability (CamelFolder *folder)
{
	g_return_val_if_fail (CAMEL_IS_FOLDER (folder), FALSE);

	return folder->has_summary_capability;
}


/* UIDs stuff */

/**
 * camel_folder_has_uid_capability: detect if the folder support UIDs
 * @folder: Folder object
 *
 * Detects if a folder supports UID operations, that is reference
 * messages by a Unique IDentifier instead of by message number.
 *
 * Return value: %TRUE if the folder supports UIDs
 **/
gboolean
camel_folder_has_uid_capability (CamelFolder *folder)
{
	g_return_val_if_fail (CAMEL_IS_FOLDER (folder), FALSE);

	return folder->has_uid_capability;
}


static const gchar *
get_message_uid (CamelFolder *folder, CamelMimeMessage *message,
		 CamelException *ex)
{
	g_warning ("CamelFolder::get_message_uid not implemented for `%s'",
		   gtk_type_name (GTK_OBJECT_TYPE (folder)));
	return NULL;
}

/**
 * camel_folder_get_message_uid:
 * @folder: Folder in which the UID must refer to
 * @message: Message object
 * @ex: a CamelException
 *
 * Return the UID of a message relatively to a folder.
 * A message can have different UID, each one corresponding
 * to a different folder, if the message is referenced in
 * several folders.
 *
 * Return value: The UID of the message in the folder
 **/
const gchar *
camel_folder_get_message_uid (CamelFolder *folder, CamelMimeMessage *message,
			      CamelException *ex)
{
	g_return_val_if_fail (CAMEL_IS_FOLDER (folder), NULL);
	g_return_val_if_fail (CAMEL_IS_MIME_MESSAGE (message), NULL);
	g_return_val_if_fail (folder->has_uid_capability, NULL);
	g_return_val_if_fail (camel_folder_is_open (folder), NULL);

	return CF_CLASS (folder)->get_message_uid (folder, message, ex);
}


static CamelMimeMessage *
get_message_by_uid (CamelFolder *folder, const gchar *uid, CamelException *ex)
{
	g_warning ("CamelFolder::get_message_by_uid not implemented for `%s'",
		   gtk_type_name (GTK_OBJECT_TYPE (folder)));
	return NULL;
}

/**
 * camel_folder_get_message_by_uid:
 * @folder: the folder object
 * @uid: the UID
 * @ex: a CamelException
 *
 * Get a message from its UID in the folder. Messages are cached
 * within a folder, that is, asking twice for the same UID returns the
 * same message object. (FIXME: is this true?)
 *
 * Return value: Message corresponding to the UID
 **/
CamelMimeMessage *
camel_folder_get_message_by_uid (CamelFolder *folder, const gchar *uid,
				 CamelException *ex)
{
	g_return_val_if_fail (CAMEL_IS_FOLDER (folder), NULL);
	g_return_val_if_fail (folder->has_uid_capability, NULL);
	g_return_val_if_fail (camel_folder_is_open (folder), NULL);

	return CF_CLASS (folder)->get_message_by_uid (folder, uid, ex);
}


static void
delete_message_by_uid (CamelFolder *folder, const gchar *uid,
		       CamelException *ex)
{
	g_warning ("CamelFolder::delete_message_by_uid not implemented "
		   "for `%s'", gtk_type_name (GTK_OBJECT_TYPE (folder)));
}

/**
 * camel_folder_delete_message_by_uid:
 * @folder: the folder object
 * @uid: the UID
 * @ex: a CamelException
 *
 * Delete a message from a folder given its UID.
 **/
void
camel_folder_delete_message_by_uid (CamelFolder *folder, const gchar *uid,
				    CamelException *ex)
{
	g_return_if_fail (CAMEL_IS_FOLDER (folder));
	g_return_if_fail (folder->has_uid_capability);
	g_return_if_fail (camel_folder_is_open (folder));

	return CF_CLASS (folder)->delete_message_by_uid (folder, uid, ex);
}


static GList *
get_uid_list (CamelFolder *folder, CamelException *ex)
{
	g_warning ("CamelFolder::get_uid_list not implemented for `%s'",
		   gtk_type_name (GTK_OBJECT_TYPE (folder)));
	return NULL;
}

/**
 * camel_folder_get_uid_list:
 * @folder: folder object
 * @ex: a CamelException
 *
 * Get the list of UIDs available in a folder. This routine is useful
 * for finding what messages are available when the folder does not
 * support summaries. The UIDs in the list must not be freed, the
 * folder object caches them.
 *
 * Return value: GList of UIDs corresponding to the messages available
 * in the folder.
 **/
GList *
camel_folder_get_uid_list (CamelFolder *folder, CamelException *ex)
{
	g_return_val_if_fail (CAMEL_IS_FOLDER (folder), NULL);
	g_return_val_if_fail (folder->has_uid_capability, NULL);
	g_return_val_if_fail (camel_folder_is_open (folder), NULL);

	return CF_CLASS (folder)->get_uid_list (folder, ex);
}

/**
 * camel_folder_has_search_capability:
 * @folder: Folder object
 *
 * Checks if a folder supports searching.
 *
 * Return value: %TRUE if the folder supports searching
 **/
gboolean
camel_folder_has_search_capability (CamelFolder *folder)
{
	g_return_val_if_fail (CAMEL_IS_FOLDER (folder), FALSE);

	return folder->has_search_capability;
}

static GList *
search_by_expression (CamelFolder *folder, const char *expression,
		      CamelException *ex)
{
	g_warning ("CamelFolder::search_by_expression not implemented for "
		   "`%s'", gtk_type_name (GTK_OBJECT_TYPE (folder)));
	return NULL;
}

GList *camel_folder_search_by_expression (CamelFolder *folder,
					  const char *expression,
					  CamelException *ex)
{
	g_return_val_if_fail (CAMEL_IS_FOLDER (folder), NULL);
	g_return_val_if_fail (folder->has_search_capability, NULL);

	return CF_CLASS (folder)->search_by_expression (folder, expression,
							ex);
}
