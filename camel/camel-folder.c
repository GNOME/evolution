/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8; fill-column: 160 -*- */
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
		  gchar *separator, gboolean path_begins_with_sep,
		  CamelException *ex);

static void finalize (GtkObject *object);


static void folder_sync (CamelFolder *folder, gboolean expunge,
			 CamelException *ex);

static const gchar *get_name (CamelFolder *folder);
static const gchar *get_full_name (CamelFolder *folder);


static gboolean can_hold_folders (CamelFolder *folder);
static gboolean can_hold_messages (CamelFolder *folder);
static guint32 get_permanent_flags (CamelFolder *folder, CamelException *ex);


static GPtrArray *get_subfolder_names (CamelFolder *folder,
				       CamelException *ex);
static CamelFolder *get_subfolder     (CamelFolder *folder,
				       const gchar *folder_name,
				       gboolean create,
				       CamelException *ex);
static CamelFolder *get_parent_folder (CamelFolder *folder,
				       CamelException *ex);
static CamelStore *get_parent_store   (CamelFolder *folder,
				       CamelException *ex);


static gint get_message_count (CamelFolder *folder, CamelException *ex);


static void expunge             (CamelFolder *folder,
				 CamelException *ex);


static void append_message (CamelFolder *folder, CamelMimeMessage *message,
			    CamelException *ex);


static GPtrArray        *get_uids            (CamelFolder *folder,
					      CamelException *ex);
static void              free_uids           (CamelFolder *folder,
					      GPtrArray *array);
static GPtrArray        *get_summary         (CamelFolder *folder,
					      CamelException *ex);
static void              free_summary        (CamelFolder *folder,
					      GPtrArray *array);
static const gchar      *get_message_uid     (CamelFolder *folder,
					      CamelMimeMessage *message,
					      CamelException *ex);
static CamelMimeMessage *get_message_by_uid  (CamelFolder *folder,
					      const gchar *uid,
					      CamelException *ex);
static void delete_message_by_uid            (CamelFolder *folder,
					      const gchar *uid,
					      CamelException *ex);

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
	camel_folder_class->sync = folder_sync;
	camel_folder_class->get_name = get_name;
	camel_folder_class->get_full_name = get_full_name;
	camel_folder_class->can_hold_folders = can_hold_folders;
	camel_folder_class->can_hold_messages = can_hold_messages;
	camel_folder_class->get_subfolder = get_subfolder;
	camel_folder_class->get_parent_folder = get_parent_folder;
	camel_folder_class->get_parent_store = get_parent_store;
	camel_folder_class->get_subfolder_names = get_subfolder_names;
	camel_folder_class->free_subfolder_names = free_uids;
	camel_folder_class->expunge = expunge;
	camel_folder_class->get_message_count = get_message_count;
	camel_folder_class->append_message = append_message;
	camel_folder_class->get_permanent_flags = get_permanent_flags;
	camel_folder_class->get_message_uid = get_message_uid;
	camel_folder_class->get_message_by_uid = get_message_by_uid;
	camel_folder_class->delete_message_by_uid = delete_message_by_uid;
	camel_folder_class->get_uids = get_uids;
	camel_folder_class->free_uids = free_uids;
	camel_folder_class->get_summary = get_summary;
	camel_folder_class->free_summary = free_summary;
	camel_folder_class->search_by_expression = search_by_expression;
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
      gchar *separator, gboolean path_begins_with_sep,
      CamelException *ex)
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

	folder->separator = separator;
	folder->path_begins_with_sep = path_begins_with_sep;

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

		full_name = g_strdup_printf ("%s%s%s", parent_full_name,
					     folder->separator, name);
	} else {
		if (path_begins_with_sep)
			full_name = g_strdup_printf ("%s%s", folder->separator, name);
		else
			full_name = g_strdup (name);
	}

	folder->name = g_strdup (name);
	folder->full_name = full_name;
}


static void
folder_sync (CamelFolder *folder, gboolean expunge, CamelException *ex)
{
	g_warning ("CamelFolder::sync not implemented for `%s'",
		   gtk_type_name (GTK_OBJECT_TYPE (folder)));
}

/**
 * camel_folder_sync:
 * @folder: The folder object
 * @expunge: whether or not to expunge deleted messages
 * @ex: exception object
 *
 * Sync changes made to a folder to its backing store, possibly expunging
 * deleted messages as well.
 **/
void
camel_folder_sync (CamelFolder *folder, gboolean expunge,
		   CamelException *ex)
{
	g_return_if_fail (CAMEL_IS_FOLDER (folder));

	CF_CLASS (folder)->sync (folder, expunge, ex);
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


static CamelFolder *
get_subfolder (CamelFolder *folder, const gchar *folder_name,
	       gboolean create, CamelException *ex)
{
	CamelFolder *new_folder;
	gchar *full_name;
	const gchar *current_folder_full_name;

	g_return_val_if_fail (CAMEL_IS_STORE (folder->parent_store), NULL);

	current_folder_full_name = camel_folder_get_full_name (folder);

	full_name = g_strdup_printf ("%s%s%s", current_folder_full_name,
				     folder->separator, folder_name);
	new_folder = camel_store_get_folder (folder->parent_store,
					     full_name, create, ex);
	g_free (full_name);

	return new_folder;
}

/**
 * camel_folder_get_subfolder:
 * @folder: a folder
 * @folder_name: subfolder path
 * @create: whether or not to create the folder if it doesn't exist
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
			    gboolean create, CamelException *ex)
{
	g_return_val_if_fail (CAMEL_IS_FOLDER (folder), NULL);
	g_return_val_if_fail (folder_name != NULL, NULL);

	return CF_CLASS (folder)->get_subfolder (folder, folder_name,
						 create, ex);
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


static GPtrArray *
get_subfolder_names (CamelFolder *folder, CamelException *ex)
{
	g_warning ("CamelFolder::get_subfolder_names not implemented for `%s'",
		   gtk_type_name (GTK_OBJECT_TYPE (folder)));
	return NULL;
}

/**
 * camel_folder_get_subfolder_names:
 * @folder: the folder
 * @ex: a CamelException
 *
 * Return value: an array containing the names of the folder's
 * subfolders. The array should not be modified and must be freed with
 * camel_folder_free_subfolder_names().
 **/
GPtrArray *
camel_folder_get_subfolder_names (CamelFolder *folder, CamelException *ex)
{
	g_return_val_if_fail (CAMEL_IS_FOLDER (folder), NULL);

	return CF_CLASS (folder)->get_subfolder_names (folder, ex);
}


/**
 * camel_folder_free_subfolder_names:
 * @folder: folder object
 * @array: the array of subfolder names to free
 *
 * Frees the array of names returned by camel_folder_get_subfolder_names().
 **/
void
camel_folder_free_subfolder_names (CamelFolder *folder, GPtrArray *array)
{
	g_return_if_fail (CAMEL_IS_FOLDER (folder));

	CF_CLASS (folder)->free_subfolder_names (folder, array);
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

	return CF_CLASS (folder)->expunge (folder, ex);
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

	return CF_CLASS (folder)->delete_message_by_uid (folder, uid, ex);
}


static GPtrArray *
get_uids (CamelFolder *folder, CamelException *ex)
{
	g_warning ("CamelFolder::get_uids not implemented for `%s'",
		   gtk_type_name (GTK_OBJECT_TYPE (folder)));
	return NULL;
}

/**
 * camel_folder_get_uids:
 * @folder: folder object
 * @ex: a CamelException
 *
 * Get the list of UIDs available in a folder. This routine is useful
 * for finding what messages are available when the folder does not
 * support summaries. The returned array shoudl not be modified, and
 * must be freed by passing it to camel_folder_free_uids().
 *
 * Return value: GPtrArray of UIDs corresponding to the messages
 * available in the folder.
 **/
GPtrArray *
camel_folder_get_uids (CamelFolder *folder, CamelException *ex)
{
	g_return_val_if_fail (CAMEL_IS_FOLDER (folder), NULL);

	return CF_CLASS (folder)->get_uids (folder, ex);
}


/* This is also the default implementation of free_subfolder_names. */
static void
free_uids (CamelFolder *folder, GPtrArray *array)
{
	int i;

	/* Default implementation: free all of the strings and
	 * the array itself.
	 */
	for (i = 0; i < array->len; i++)
		g_free (array->pdata[i]);
	g_ptr_array_free (array, TRUE);
}

/**
 * camel_folder_free_uids:
 * @folder: folder object
 * @array: the array of uids to free
 *
 * Frees the array of UIDs returned by camel_folder_get_uids().
 **/
void
camel_folder_free_uids (CamelFolder *folder, GPtrArray *array)
{
	g_return_if_fail (CAMEL_IS_FOLDER (folder));

	CF_CLASS (folder)->free_uids (folder, array);
}


static GPtrArray *
get_summary (CamelFolder *folder, CamelException *ex)
{
	g_warning ("CamelFolder::get_summary not implemented for `%s'",
		   gtk_type_name (GTK_OBJECT_TYPE (folder)));
	return NULL;
}

/**
 * camel_folder_get_summary:
 * @folder: a folder object
 * @ex: a CamelException
 *
 * This returns the summary information for the folder. This array
 * should not be modified, and must be freed with
 * camel_folder_free_summary().
 *
 * Return value: an array of CamelMessageInfo
 **/
GPtrArray *
camel_folder_get_summary (CamelFolder *folder, CamelException *ex)
{
	g_return_val_if_fail (CAMEL_IS_FOLDER (folder), NULL);

	return CF_CLASS (folder)->get_summary (folder, ex);
}


static void
free_summary (CamelFolder *folder, GPtrArray *array)
{
	g_warning ("CamelFolder::free_summary not implemented for `%s'",
		   gtk_type_name (GTK_OBJECT_TYPE (folder)));
}

/**
 * camel_folder_free_summary:
 * @folder: folder object
 * @array: the summary array to free
 *
 * Frees the summary array returned by camel_folder_get_summary().
 **/
void
camel_folder_free_summary (CamelFolder *folder, GPtrArray *array)
{
	g_return_if_fail (CAMEL_IS_FOLDER (folder));

	CF_CLASS (folder)->free_summary (folder, array);
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

/**
 * camel_folder_search_by_expression:
 * @folder: Folder object
 * @expression: a search expression
 * @ex: a CamelException
 *
 * Searches the folder for messages matching the given search expression.
 *
 * Return value: a list of uids of matching messages. The caller must
 * free the list and each of the elements when it is done.
 **/
GList *
camel_folder_search_by_expression (CamelFolder *folder, const char *expression,
				   CamelException *ex)
{
	g_return_val_if_fail (CAMEL_IS_FOLDER (folder), NULL);
	g_return_val_if_fail (folder->has_search_capability, NULL);

	return CF_CLASS (folder)->search_by_expression (folder, expression, ex);
}
