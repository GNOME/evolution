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
#include "e-util/e-memory.h"

static CamelObjectClass *parent_class = NULL;

/* Returns the class for a CamelFolder */
#define CF_CLASS(so) CAMEL_FOLDER_CLASS (CAMEL_OBJECT_GET_CLASS(so))

static void camel_folder_finalize (CamelObject *object);

static void refresh_info (CamelFolder *folder, CamelException *ex);

static void folder_sync (CamelFolder *folder, gboolean expunge,
			 CamelException *ex);

static const gchar *get_name (CamelFolder *folder);
static const gchar *get_full_name (CamelFolder *folder);
static CamelStore *get_parent_store   (CamelFolder *folder);

static guint32 get_permanent_flags (CamelFolder *folder);
static guint32 get_message_flags (CamelFolder *folder, const char *uid);
static void set_message_flags (CamelFolder *folder, const char *uid,
			       guint32 flags, guint32 set);
static gboolean get_message_user_flag (CamelFolder *folder, const char *uid, const char *name);
static void set_message_user_flag (CamelFolder *folder, const char *uid,
				   const char *name, gboolean value);
static const char *get_message_user_tag(CamelFolder *folder, const char *uid, const char *name);
static void set_message_user_tag(CamelFolder *folder, const char *uid, const char *name, const char *value);

static gint get_message_count (CamelFolder *folder);
static gint get_unread_message_count (CamelFolder *folder);

static void expunge             (CamelFolder *folder,
				 CamelException *ex);


static void append_message (CamelFolder *folder, CamelMimeMessage *message,
			    const CamelMessageInfo *info, CamelException *ex);


static GPtrArray        *get_uids            (CamelFolder *folder);
static void              free_uids           (CamelFolder *folder,
					      GPtrArray *array);
static GPtrArray        *get_summary         (CamelFolder *folder);
static void              free_summary        (CamelFolder *folder,
					      GPtrArray *array);

static CamelMimeMessage *get_message         (CamelFolder *folder,
					      const gchar *uid,
					      CamelException *ex);

static const CamelMessageInfo *get_message_info (CamelFolder *folder,
						 const char *uid);

static GPtrArray      *search_by_expression  (CamelFolder *folder,
					      const char *exp,
					      CamelException *ex);
static void            search_free           (CamelFolder * folder, 
					      GPtrArray * result);

static void            copy_message_to       (CamelFolder *source,
					      const char *uid,
					      CamelFolder *dest,
					      CamelException *ex);

static void            move_message_to       (CamelFolder *source,
					      const char *uid,
					      CamelFolder *dest,
					      CamelException *ex);

static void            freeze                (CamelFolder *folder);
static void            thaw                  (CamelFolder *folder);

static gboolean        folder_changed        (CamelObject *object,
					      gpointer event_data);
static gboolean        message_changed       (CamelObject *object,
					      /*const char *uid*/gpointer event_data);

static void
camel_folder_class_init (CamelFolderClass *camel_folder_class)
{
	CamelObjectClass *camel_object_class =
		CAMEL_OBJECT_CLASS (camel_folder_class);

	parent_class = camel_type_get_global_classfuncs (camel_object_get_type ());

	/* virtual method definition */
	camel_folder_class->sync = folder_sync;
	camel_folder_class->refresh_info = refresh_info;
	camel_folder_class->get_name = get_name;
	camel_folder_class->get_full_name = get_full_name;
	camel_folder_class->get_parent_store = get_parent_store;
	camel_folder_class->expunge = expunge;
	camel_folder_class->get_message_count = get_message_count;
	camel_folder_class->get_unread_message_count = get_unread_message_count;
	camel_folder_class->append_message = append_message;
	camel_folder_class->get_permanent_flags = get_permanent_flags;
	camel_folder_class->get_message_flags = get_message_flags;
	camel_folder_class->set_message_flags = set_message_flags;
	camel_folder_class->get_message_user_flag = get_message_user_flag;
	camel_folder_class->set_message_user_flag = set_message_user_flag;
	camel_folder_class->get_message_user_tag = get_message_user_tag;
	camel_folder_class->set_message_user_tag = set_message_user_tag;
	camel_folder_class->get_message = get_message;
	camel_folder_class->get_uids = get_uids;
	camel_folder_class->free_uids = free_uids;
	camel_folder_class->get_summary = get_summary;
	camel_folder_class->free_summary = free_summary;
	camel_folder_class->search_by_expression = search_by_expression;
	camel_folder_class->search_free = search_free;
	camel_folder_class->get_message_info = get_message_info;
	camel_folder_class->copy_message_to = copy_message_to;
	camel_folder_class->move_message_to = move_message_to;
	camel_folder_class->freeze = freeze;
	camel_folder_class->thaw = thaw;

	/* virtual method overload */
	camel_object_class_declare_event (camel_object_class,
					  "folder_changed", folder_changed);
	camel_object_class_declare_event (camel_object_class,
					  "message_changed", message_changed);
}

static void
camel_folder_init (gpointer object, gpointer klass)
{
	CamelFolder *folder = object;

	folder->frozen = 0;
	folder->changed_frozen = camel_folder_change_info_new();
}

static void
camel_folder_finalize (CamelObject *object)
{
	CamelFolder *camel_folder = CAMEL_FOLDER (object);

	g_free (camel_folder->name);
	g_free (camel_folder->full_name);

	if (camel_folder->parent_store)
		camel_object_unref (CAMEL_OBJECT (camel_folder->parent_store));

	camel_folder_change_info_free(camel_folder->changed_frozen);
}

CamelType
camel_folder_get_type (void)
{
	static CamelType camel_folder_type = CAMEL_INVALID_TYPE;

	if (camel_folder_type == CAMEL_INVALID_TYPE)	{
		camel_folder_type = camel_type_register (CAMEL_OBJECT_TYPE, "CamelFolder",
							 sizeof (CamelFolder),
							 sizeof (CamelFolderClass),
							 (CamelObjectClassInitFunc) camel_folder_class_init,
							 NULL,
							 (CamelObjectInitFunc) camel_folder_init,
							 (CamelObjectFinalizeFunc) camel_folder_finalize );
	}

	return camel_folder_type;
}


/**
 * camel_folder_construct:
 * @folder: folder object to construct
 * @parent_store: parent store object of the folder
 * @full_name: full name of the folder
 * @name: short name of the folder
 *
 * Initalizes the folder by setting the parent store and name.
 **/
void
camel_folder_construct (CamelFolder *folder, CamelStore *parent_store,
			const char *full_name, const char *name)
{
	g_return_if_fail (CAMEL_IS_FOLDER (folder));
	g_return_if_fail (CAMEL_IS_STORE (parent_store));
	g_return_if_fail (folder->parent_store == NULL);
	g_return_if_fail (folder->name == NULL);

	folder->parent_store = parent_store;
	camel_object_ref (CAMEL_OBJECT (parent_store));

	folder->name = g_strdup (name);
	folder->full_name = g_strdup (full_name);
}


static void
folder_sync (CamelFolder *folder, gboolean expunge, CamelException *ex)
{
	g_warning ("CamelFolder::sync not implemented for `%s'",
		   camel_type_to_name (CAMEL_OBJECT_GET_TYPE (folder)));
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
camel_folder_sync (CamelFolder *folder, gboolean expunge, CamelException *ex)
{
	g_return_if_fail (CAMEL_IS_FOLDER (folder));

	CF_CLASS (folder)->sync (folder, expunge, ex);
}


static void
refresh_info (CamelFolder *folder, CamelException *ex)
{
	/* No op */
}

/**
 * camel_folder_refresh_info:
 * @folder: The folder object
 * @ex: exception object
 *
 * Updates a folder's summary to be in sync with its backing store.
 **/
void
camel_folder_refresh_info (CamelFolder *folder, CamelException *ex)
{
	g_return_if_fail (CAMEL_IS_FOLDER (folder));

	CF_CLASS (folder)->refresh_info (folder, ex);
}


static const char *
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
const char *
camel_folder_get_name (CamelFolder * folder)
{
	g_return_val_if_fail (CAMEL_IS_FOLDER (folder), NULL);

	return CF_CLASS (folder)->get_name (folder);
}


static const char *
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
const char *
camel_folder_get_full_name (CamelFolder *folder)
{
	g_return_val_if_fail (CAMEL_IS_FOLDER (folder), NULL);

	return CF_CLASS (folder)->get_full_name (folder);
}


static CamelStore *
get_parent_store (CamelFolder * folder)
{
	return folder->parent_store;
}

/**
 * camel_folder_get_parent_store:
 * @folder: folder to get the parent of
 *
 * Return value: the parent store of the folder.
 **/
CamelStore *
camel_folder_get_parent_store (CamelFolder *folder)
{
	g_return_val_if_fail (CAMEL_IS_FOLDER (folder), NULL);

	return CF_CLASS (folder)->get_parent_store (folder);
}


static void
expunge (CamelFolder *folder, CamelException *ex)
{
	g_warning ("CamelFolder::expunge not implemented for `%s'",
		   camel_type_to_name (CAMEL_OBJECT_GET_TYPE (folder)));
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

	CF_CLASS (folder)->expunge (folder, ex);
}


static int
get_message_count (CamelFolder *folder)
{
	g_warning ("CamelFolder::get_message_count not implemented "
		   "for `%s'", camel_type_to_name (CAMEL_OBJECT_GET_TYPE (folder)));
	return -1;
}

/**
 * camel_folder_get_message_count:
 * @folder: A CamelFolder object
 *
 * Return value: the number of messages in the folder, or -1 if unknown.
 **/
int
camel_folder_get_message_count (CamelFolder *folder)
{
	g_return_val_if_fail (CAMEL_IS_FOLDER (folder), -1);

	return CF_CLASS (folder)->get_message_count (folder);
}


static int
get_unread_message_count (CamelFolder *folder)
{
	g_warning ("CamelFolder::get_unread_message_count not implemented "
		   "for `%s'", camel_type_to_name (CAMEL_OBJECT_GET_TYPE (folder)));
	return -1;
}

/**
 * camel_folder_unread_get_message_count:
 * @folder: A CamelFolder object
 *
 * Return value: the number of unread messages in the folder, or -1 if unknown.
 **/
int
camel_folder_get_unread_message_count (CamelFolder *folder)
{
	g_return_val_if_fail (CAMEL_IS_FOLDER (folder), -1);

	return CF_CLASS (folder)->get_unread_message_count (folder);
}


static void
append_message (CamelFolder *folder, CamelMimeMessage *message,
		const CamelMessageInfo *info, CamelException *ex)
{
	g_warning ("CamelFolder::append_message not implemented for `%s'",
		   camel_type_to_name (CAMEL_OBJECT_GET_TYPE (folder)));
	return;

}

/**
 * camel_folder_append_message: add a message to a folder
 * @folder: folder object to add the message to
 * @message: message object
 * @info: message info with additional flags/etc to set on
 * new message, or %NULL
 * @ex: exception object
 *
 * Add a message to a folder. Only the flag and tag data from @info
 * is used. If @info is %NULL, no flags or tags will be set.
 **/
void
camel_folder_append_message (CamelFolder *folder, CamelMimeMessage *message,
			     const CamelMessageInfo *info, CamelException *ex)
{
	g_return_if_fail (CAMEL_IS_FOLDER (folder));

	CF_CLASS (folder)->append_message (folder, message, info, ex);
}


static guint32
get_permanent_flags (CamelFolder *folder)
{
	return folder->permanent_flags;
}

/**
 * camel_folder_get_permanent_flags:
 * @folder: a CamelFolder
 *
 * Return value: the set of CamelMessageFlags that can be permanently
 * stored on a message between sessions. If it includes %CAMEL_FLAG_USER,
 * then user-defined flags will be remembered.
 **/
guint32
camel_folder_get_permanent_flags (CamelFolder *folder)
{
	g_return_val_if_fail (CAMEL_IS_FOLDER (folder), 0);

	return CF_CLASS (folder)->get_permanent_flags (folder);
}


static guint32
get_message_flags (CamelFolder *folder, const char *uid)
{
	g_warning ("CamelFolder::get_message_flags not implemented for `%s'",
		   camel_type_to_name (CAMEL_OBJECT_GET_TYPE (folder)));
	return 0;
}

/**
 * camel_folder_get_message_flags:
 * @folder: a CamelFolder
 * @uid: the UID of a message in @folder
 *
 * Return value: the CamelMessageFlags that are set on the indicated
 * message.
 **/
guint32
camel_folder_get_message_flags (CamelFolder *folder, const char *uid)
{
	g_return_val_if_fail (CAMEL_IS_FOLDER (folder), 0);

	return CF_CLASS (folder)->get_message_flags (folder, uid);
}


static void
set_message_flags (CamelFolder *folder, const char *uid,
		   guint32 flags, guint32 set)
{
	g_warning ("CamelFolder::set_message_flags not implemented for `%s'",
		   camel_type_to_name (CAMEL_OBJECT_GET_TYPE (folder)));
}

/**
 * camel_folder_set_message_flags:
 * @folder: a CamelFolder
 * @uid: the UID of a message in @folder
 * @flags: a set of CamelMessageFlag values to set
 * @set: the mask of values in @flags to use.
 *
 * Sets those flags specified by @set to the values specified by @flags
 * on the indicated message. (This may or may not persist after the
 * folder or store is closed. See camel_folder_get_permanent_flags().)
 **/
void
camel_folder_set_message_flags (CamelFolder *folder, const char *uid,
				guint32 flags, guint32 set)
{
	g_return_if_fail (CAMEL_IS_FOLDER (folder));

	CF_CLASS (folder)->set_message_flags (folder, uid, flags, set);
}


static gboolean
get_message_user_flag (CamelFolder *folder, const char *uid,
		       const char *name)
{
	g_warning ("CamelFolder::get_message_user_flag not implemented "
		   "for `%s'", camel_type_to_name (CAMEL_OBJECT_GET_TYPE (folder)));
	return FALSE;
}

/**
 * camel_folder_get_message_user_flag:
 * @folder: a CamelFolder
 * @uid: the UID of a message in @folder
 * @name: the name of a user flag
 *
 * Return value: whether or not the given user flag is set on the message.
 **/
gboolean
camel_folder_get_message_user_flag (CamelFolder *folder, const char *uid,
				    const char *name)
{
	g_return_val_if_fail (CAMEL_IS_FOLDER (folder), 0);

	return CF_CLASS (folder)->get_message_user_flag (folder, uid, name);
}


static void
set_message_user_flag (CamelFolder *folder, const char *uid,
		       const char *name, gboolean value)
{
	g_warning ("CamelFolder::set_message_user_flag not implemented "
		   "for `%s'", camel_type_to_name (CAMEL_OBJECT_GET_TYPE (folder)));
}

/**
 * camel_folder_set_message_user_flag:
 * @folder: a CamelFolder
 * @uid: the UID of a message in @folder
 * @name: the name of the user flag to set
 * @value: the value to set it to
 *
 * Sets the user flag specified by @name to the value specified by @value
 * on the indicated message. (This may or may not persist after the
 * folder or store is closed. See camel_folder_get_permanent_flags().)
 **/
void
camel_folder_set_message_user_flag (CamelFolder *folder, const char *uid,
				    const char *name, gboolean value)
{
	g_return_if_fail (CAMEL_IS_FOLDER (folder));

	CF_CLASS (folder)->set_message_user_flag (folder, uid, name, value);
}

static const char *get_message_user_tag(CamelFolder *folder, const char *uid, const char *name)
{
	g_warning ("CamelFolder::get_message_user_tag not implemented "
		   "for `%s'", camel_type_to_name (CAMEL_OBJECT_GET_TYPE (folder)));
	return NULL;
}

/**
 * camel_folder_get_message_user_tag:
 * @folder: a CamelFolder
 * @uid: the UID of a message in @folder
 * @name: the name of a user tag
 *
 * Return value: Returns the value of the user tag.
 **/
const char *
camel_folder_get_message_user_tag (CamelFolder *folder, const char *uid,  const char *name)
{
	g_return_val_if_fail (CAMEL_IS_FOLDER (folder), 0);

	return CF_CLASS (folder)->get_message_user_tag (folder, uid, name);
}


static void
set_message_user_tag(CamelFolder *folder, const char *uid, const char *name, const char *value)
{
	g_warning ("CamelFolder::set_message_user_tag not implemented "
		   "for `%s'", camel_type_to_name (CAMEL_OBJECT_GET_TYPE (folder)));
}

/**
 * camel_folder_set_message_user_tag:
 * @folder: a CamelFolder
 * @uid: the UID of a message in @folder
 * @name: the name of the user tag to set
 * @value: the value to set it to
 *
 * Sets the user tag specified by @name to the value specified by @value
 * on the indicated message. (This may or may not persist after the
 * folder or store is closed. See camel_folder_get_permanent_flags().)
 **/
void
camel_folder_set_message_user_tag (CamelFolder *folder, const char *uid, const char *name, const char *value)
{
	g_return_if_fail (CAMEL_IS_FOLDER (folder));

	CF_CLASS (folder)->set_message_user_tag (folder, uid, name, value);
}


static const CamelMessageInfo *
get_message_info (CamelFolder *folder, const char *uid)
{
	g_warning ("CamelFolder::get_message_info not implemented for `%s'",
		   camel_type_to_name (CAMEL_OBJECT_GET_TYPE (folder)));
	return NULL;
}

/**
 * camel_folder_get_message_info:
 * @folder: a CamelFolder
 * @uid: the uid of a message
 *
 * Return value: the summary information for the indicated message
 **/
const CamelMessageInfo *
camel_folder_get_message_info (CamelFolder *folder, const char *uid)
{
	g_return_val_if_fail (CAMEL_IS_FOLDER (folder), NULL);
	g_return_val_if_fail (uid != NULL, NULL);

	return CF_CLASS (folder)->get_message_info (folder, uid);
}


/* TODO: is this function required anyway? */
gboolean
camel_folder_has_summary_capability (CamelFolder *folder)
{
	g_return_val_if_fail (CAMEL_IS_FOLDER (folder), FALSE);

	return folder->has_summary_capability;
}


/* UIDs stuff */

static CamelMimeMessage *
get_message (CamelFolder *folder, const gchar *uid, CamelException *ex)
{
	g_warning ("CamelFolder::get_message not implemented for `%s'",
		   camel_type_to_name (CAMEL_OBJECT_GET_TYPE (folder)));
	return NULL;
}

/**
 * camel_folder_get_message:
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
camel_folder_get_message (CamelFolder *folder, const gchar *uid,
			  CamelException *ex)
{
	g_return_val_if_fail (CAMEL_IS_FOLDER (folder), NULL);

	return CF_CLASS (folder)->get_message (folder, uid, ex);
}


static GPtrArray *
get_uids (CamelFolder *folder)
{
	g_warning ("CamelFolder::get_uids not implemented for `%s'",
		   camel_type_to_name (CAMEL_OBJECT_GET_TYPE (folder)));
	return NULL;
}

/**
 * camel_folder_get_uids:
 * @folder: folder object
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
camel_folder_get_uids (CamelFolder *folder)
{
	g_return_val_if_fail (CAMEL_IS_FOLDER (folder), NULL);

	return CF_CLASS (folder)->get_uids (folder);
}


static void
free_uids (CamelFolder *folder, GPtrArray *array)
{
	g_warning ("CamelFolder::free_uids not implemented for `%s'",
		   camel_type_to_name (CAMEL_OBJECT_GET_TYPE (folder)));
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
get_summary (CamelFolder *folder)
{
	g_warning ("CamelFolder::get_summary not implemented for `%s'",
		   camel_type_to_name (CAMEL_OBJECT_GET_TYPE (folder)));
	return NULL;
}

/**
 * camel_folder_get_summary:
 * @folder: a folder object
 *
 * This returns the summary information for the folder. This array
 * should not be modified, and must be freed with
 * camel_folder_free_summary().
 *
 * Return value: an array of CamelMessageInfo
 **/
GPtrArray *
camel_folder_get_summary (CamelFolder *folder)
{
	g_return_val_if_fail (CAMEL_IS_FOLDER (folder), NULL);

	return CF_CLASS (folder)->get_summary (folder);
}


static void
free_summary (CamelFolder *folder, GPtrArray *array)
{
	g_warning ("CamelFolder::free_summary not implemented for `%s'",
		   camel_type_to_name (CAMEL_OBJECT_GET_TYPE (folder)));
}

/**
 * camel_folder_free_summary:
 * @folder: folder object
 * @array: the summary array to free
 *
 * Frees the summary array returned by camel_folder_get_summary().
 **/
void camel_folder_free_summary(CamelFolder * folder, GPtrArray * array)
{
	g_return_if_fail(CAMEL_IS_FOLDER(folder));

	CF_CLASS(folder)->free_summary(folder, array);
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

static GPtrArray *
search_by_expression (CamelFolder *folder, const char *expression,
		      CamelException *ex)
{
	g_warning ("CamelFolder::search_by_expression not implemented for "
		   "`%s'", camel_type_to_name (CAMEL_OBJECT_GET_TYPE (folder)));
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
GPtrArray *
camel_folder_search_by_expression (CamelFolder *folder, const char *expression,
				   CamelException *ex)
{
	g_return_val_if_fail (CAMEL_IS_FOLDER (folder), NULL);
	g_return_val_if_fail (folder->has_search_capability, NULL);

	return CF_CLASS (folder)->search_by_expression (folder, expression, ex);
}

static void
search_free (CamelFolder *folder, GPtrArray *result)
{
	int i;

	for (i = 0; i < result->len; i++)
		g_free (g_ptr_array_index (result, i));
	g_ptr_array_free (result, TRUE);
}

/**
 * camel_folder_search_free:
 * @folder: 
 * @result: 
 * 
 * Free the result of a search.
 **/
void 
camel_folder_search_free (CamelFolder *folder, GPtrArray *result)
{
	g_return_if_fail (CAMEL_IS_FOLDER (folder));
	g_return_if_fail (folder->has_search_capability);

	return CF_CLASS (folder)->search_free (folder, result);
}


static void
copy_message_to (CamelFolder *source, const char *uid, CamelFolder *dest,
		 CamelException *ex)
{
	CamelMimeMessage *msg;
	const CamelMessageInfo *info;

	/* Default implementation. */
	
	msg = camel_folder_get_message (source, uid, ex);
	if (!msg)
		return;
	info = camel_folder_get_message_info (source, uid);
	camel_folder_append_message (dest, msg, info, ex);
	camel_object_unref (CAMEL_OBJECT (msg));
}

/**
 * camel_folder_copy_message_to:
 * @source: source folder
 * @uid: UID of message in @source
 * @dest: destination folder
 * @ex: a CamelException
 *
 * This copies a message from one folder to another. If the @source and
 * @dest folders have the same parent_store, this may be more efficient
 * than a camel_folder_append_message().
 **/
void
camel_folder_copy_message_to (CamelFolder *source, const char *uid,
			      CamelFolder *dest, CamelException *ex)
{
	g_return_if_fail (CAMEL_IS_FOLDER (source));
	g_return_if_fail (CAMEL_IS_FOLDER (dest));
	g_return_if_fail (uid != NULL);

	if (source->parent_store == dest->parent_store) {
		return CF_CLASS (source)->copy_message_to (source, uid,
							   dest, ex);
	} else
		return copy_message_to (source, uid, dest, ex);
}


static void
move_message_to (CamelFolder *source, const char *uid,
		 CamelFolder *dest, CamelException *ex)
{
	CamelMimeMessage *msg;
	const CamelMessageInfo *info;

	/* Default implementation. */
	
	msg = camel_folder_get_message (source, uid, ex);
	if (!msg)
		return;
	info = camel_folder_get_message_info (source, uid);
	camel_folder_append_message (dest, msg, info, ex);
	camel_object_unref (CAMEL_OBJECT (msg));
	if (camel_exception_is_set(ex))
		return;
	camel_folder_delete_message (source, uid);
}

/**
 * camel_folder_move_message_to:
 * @source: source folder
 * @uid: UID of message in @source
 * @dest: destination folder
 * @ex: a CamelException
 *
 * This moves a message from one folder to another. If the @source and
 * @dest folders have the same parent_store, this may be more efficient
 * than a camel_folder_append_message() followed by
 * camel_folder_delete_message().
 **/
void
camel_folder_move_message_to (CamelFolder *source, const char *uid,
			      CamelFolder *dest, CamelException *ex)
{
	g_return_if_fail (CAMEL_IS_FOLDER (source));
	g_return_if_fail (CAMEL_IS_FOLDER (dest));
	g_return_if_fail (uid != NULL);

	if (source->parent_store == dest->parent_store) {
		return CF_CLASS (source)->move_message_to (source, uid,
							   dest, ex);
	} else
		return move_message_to (source, uid, dest, ex);
}

static void
freeze (CamelFolder *folder)
{
	folder->frozen++;
}

/**
 * camel_folder_freeze:
 * @folder: a folder
 *
 * Freezes the folder so that a series of operation can be performed
 * without "message_changed" and "folder_changed" signals being emitted.
 * When the folder is later thawed with camel_folder_thaw(), the
 * suppressed signals will be emitted.
 **/
void
camel_folder_freeze (CamelFolder * folder)
{
	g_return_if_fail (CAMEL_IS_FOLDER (folder));

	CF_CLASS (folder)->freeze (folder);
}

static void
thaw (CamelFolder * folder)
{
	int i;
	CamelFolderChangeInfo *info;

	folder->frozen--;
	if (folder->frozen != 0)
		return;

	/* If we have more or less messages, do a folder changed, otherwise just
	   do a message changed for each one.
	   TODO: message_changed is now probably irrelevant and not required */
	info = folder->changed_frozen;
	if (info->uid_added->len > 0 || info->uid_removed->len > 0) {
		camel_object_trigger_event(CAMEL_OBJECT(folder), "folder_changed", info);
	} else if (info->uid_changed->len > 0) {
		for (i=0;i<info->uid_changed->len;i++) {
			camel_object_trigger_event(CAMEL_OBJECT(folder), "message_changed", info->uid_changed->pdata[i]);
		}
	}

	camel_folder_change_info_clear(info);
}

/**
 * camel_folder_thaw:
 * @folder: a folder
 *
 * Thaws the folder and emits any pending folder_changed or
 * message_changed signals.
 **/
void
camel_folder_thaw (CamelFolder *folder)
{
	g_return_if_fail (CAMEL_IS_FOLDER (folder));
	g_return_if_fail (folder->frozen != 0);

	CF_CLASS (folder)->thaw (folder);
}


/* Event hooks that block emission when frozen */
static gboolean
folder_changed (CamelObject *obj, gpointer event_data)
{
	CamelFolder *folder = CAMEL_FOLDER (obj);
	CamelFolderChangeInfo *changed = event_data;

	if (folder->frozen) {
		if (changed != NULL)
			camel_folder_change_info_cat(folder->changed_frozen, changed);
		else
			g_warning("Class %s is passing NULL to folder_changed event",
				  camel_type_to_name (CAMEL_OBJECT_GET_TYPE (folder)));
		return FALSE;
	}
	return TRUE;
}

static gboolean
message_changed (CamelObject *obj, /*const char *uid*/gpointer event_data)
{
	CamelFolder *folder = CAMEL_FOLDER (obj);

	if (folder->frozen) {
		camel_folder_change_info_change_uid(folder->changed_frozen, (char *)event_data);
		return FALSE;
	}

	return TRUE;
}


/**
 * camel_folder_free_nop:
 * @folder: a folder
 * @array: an array of uids or CamelMessageInfo
 *
 * "Frees" the provided array by doing nothing. Used by CamelFolder
 * subclasses as an implementation for free_uids, or free_summary when
 * the returned array is "static" information and should not be freed.
 **/
void
camel_folder_free_nop (CamelFolder *folder, GPtrArray *array)
{
	;
}

/**
 * camel_folder_free_shallow:
 * @folder: a folder
 * @array: an array of uids or CamelMessageInfo
 *
 * Frees the provided array but not its contents. Used by CamelFolder
 * subclasses as an implementation for free_uids or free_summary when
 * the returned array needs to be freed but its contents come from
 * "static" information.
 **/
void
camel_folder_free_shallow (CamelFolder *folder, GPtrArray *array)
{
	g_ptr_array_free (array, TRUE);
}

/**
 * camel_folder_free_deep:
 * @folder: a folder
 * @array: an array of uids
 *
 * Frees the provided array and its contents. Used by CamelFolder
 * subclasses as an implementation for free_uids when the provided
 * information was created explicitly by the corresponding get_ call.
 **/
void
camel_folder_free_deep (CamelFolder *folder, GPtrArray *array)
{
	int i;

	for (i = 0; i < array->len; i++)
		g_free (array->pdata[i]);
	g_ptr_array_free (array, TRUE);
}

/**
 * camel_folder_change_info_new:
 * @void: 
 * 
 * Create a new folder change info structure.
 * 
 * Return value: 
 **/
CamelFolderChangeInfo *
camel_folder_change_info_new(void)
{
	CamelFolderChangeInfo *info;

	info = g_malloc(sizeof(*info));
	info->uid_added = g_ptr_array_new();
	info->uid_removed = g_ptr_array_new();
	info->uid_changed = g_ptr_array_new();
	info->uid_source = NULL;
	info->uid_pool = e_mempool_new(512, 256, E_MEMPOOL_ALIGN_BYTE);

	return info;
}

static void
change_info_add_uid(CamelFolderChangeInfo *info, GPtrArray *uids, const char *uid, int copy)
{
	int i;

	/* TODO: Check that it is in the other arrays and remove it from them/etc? */
	for (i=0;i<uids->len;i++) {
		if (!strcmp(uids->pdata[i], uid))
			return;
	}
	if (copy)
		g_ptr_array_add(uids, e_mempool_strdup(info->uid_pool, uid));
	else
		g_ptr_array_add(uids, uid);
}

static void
change_info_cat(CamelFolderChangeInfo *info, GPtrArray *uids, GPtrArray *source)
{
	int i;

	for (i=0;i<source->len;i++) {
		change_info_add_uid(info, uids, source->pdata[i], TRUE);
	}
}

/**
 * camel_folder_change_info_add_source:
 * @info: 
 * @uid: 
 * 
 * Add a source uid for generating a changeset.
 **/
void
camel_folder_change_info_add_source(CamelFolderChangeInfo *info, const char *uid)
{
	if (info->uid_source == NULL)
		info->uid_source = g_hash_table_new(g_str_hash, g_str_equal);

	if (g_hash_table_lookup(info->uid_source, uid) == NULL)
		g_hash_table_insert(info->uid_source, e_mempool_strdup(info->uid_pool, uid), (void *)1);
}

/**
 * camel_folder_change_info_add_source_list:
 * @info: 
 * @list: 
 * 
 * Add a list of source uid's for generating a changeset.
 **/
void
camel_folder_change_info_add_source_list(CamelFolderChangeInfo *info, const GPtrArray *list)
{
	int i;

	if (info->uid_source == NULL)
		info->uid_source = g_hash_table_new(g_str_hash, g_str_equal);

	for (i=0;i<list->len;i++) {
		char *uid = list->pdata[i];

		if (g_hash_table_lookup(info->uid_source, uid) == NULL)
			g_hash_table_insert(info->uid_source, e_mempool_strdup(info->uid_pool, uid), (void *)1);
	}
}

/**
 * camel_folder_change_info_add_update:
 * @info: 
 * @uid: 
 * 
 * Add a uid from the updated list, used to generate a changeset diff.
 **/
void
camel_folder_change_info_add_update(CamelFolderChangeInfo *info, const char *uid)
{
	char *key;
	int value;

	if (info->uid_source == NULL) {
		change_info_add_uid(info, info->uid_added, uid, TRUE);
		return;
	}

	if (g_hash_table_lookup_extended(info->uid_source, uid, (void **)&key, (void **)&value)) {
		g_hash_table_remove(info->uid_source, key);
	} else {
		change_info_add_uid(info, info->uid_added, uid, TRUE);
	}
}

/**
 * camel_folder_change_info_add_update_list:
 * @info: 
 * @list: 
 * 
 * Add a list of uid's from the updated list.
 **/
void
camel_folder_change_info_add_update_list(CamelFolderChangeInfo *info, const GPtrArray *list)
{
	int i;

	for (i=0;i<list->len;i++) {
		camel_folder_change_info_add_update(info, list->pdata[i]);
	}
}

static void
change_info_remove(char *key, void *value, CamelFolderChangeInfo *info)
{
	/* we dont need to copy this, as they've already been copied into our pool */
	change_info_add_uid(info, info->uid_removed, key, FALSE);
}

/**
 * camel_folder_change_info_build_diff:
 * @info: 
 * 
 * Compare the source uid set to the updated uid set and generate the differences
 * into the added and removed lists.
 **/
void
camel_folder_change_info_build_diff(CamelFolderChangeInfo *info)
{
	if (info->uid_source) {
		g_hash_table_foreach(info->uid_source, (GHFunc)change_info_remove, info);
		g_hash_table_destroy(info->uid_source);
		info->uid_source = NULL;
	}
}

/**
 * camel_folder_change_info_cat:
 * @info: 
 * @source: 
 * 
 * Concatenate one change info onto antoher.  Can be used to copy
 * them too.
 **/
void
camel_folder_change_info_cat(CamelFolderChangeInfo *info, CamelFolderChangeInfo *source)
{
	change_info_cat(info, info->uid_added, source->uid_added);
	change_info_cat(info, info->uid_removed, source->uid_removed);
	change_info_cat(info, info->uid_changed, source->uid_changed);
}

/**
 * camel_folder_change_info_add_uid:
 * @info: 
 * @uid: 
 * 
 * Add a new uid to the changeinfo.
 **/
void
camel_folder_change_info_add_uid(CamelFolderChangeInfo *info, const char *uid)
{
	change_info_add_uid(info, info->uid_added, uid, TRUE);
}

/**
 * camel_folder_change_info_remove_uid:
 * @info: 
 * @uid: 
 * 
 * Add a uid to the removed uid list.
 **/
void
camel_folder_change_info_remove_uid(CamelFolderChangeInfo *info, const char *uid)
{
	change_info_add_uid(info, info->uid_removed, uid, TRUE);
}

/**
 * camel_folder_change_info_change_uid:
 * @info: 
 * @uid: 
 * 
 * Add a uid to the changed uid list.
 **/
void
camel_folder_change_info_change_uid(CamelFolderChangeInfo *info, const char *uid)
{
	change_info_add_uid(info, info->uid_changed, uid, TRUE);
}

/**
 * camel_folder_change_info_changed:
 * @info: 
 * 
 * Return true if the changeset contains any changes.
 *
 * Return Value:
 **/
gboolean
camel_folder_change_info_changed(CamelFolderChangeInfo *info)
{
	return (info->uid_added->len || info->uid_removed->len || info->uid_changed->len);
}

/**
 * camel_folder_change_info_clear:
 * @info: 
 * 
 * Empty out the change info; called after changes have been processed.
 **/
void
camel_folder_change_info_clear(CamelFolderChangeInfo *info)
{
	g_ptr_array_set_size(info->uid_added, 0);
	g_ptr_array_set_size(info->uid_removed, 0);
	g_ptr_array_set_size(info->uid_changed, 0);
	if (info->uid_source) {
		g_hash_table_destroy(info->uid_source);
		info->uid_source = NULL;
	}
	e_mempool_flush(info->uid_pool, TRUE);
}

/**
 * camel_folder_change_info_free:
 * @info: 
 * 
 * Free memory associated with the folder change info lists.
 **/
void
camel_folder_change_info_free(CamelFolderChangeInfo *info)
{
	if (info->uid_source)
		g_hash_table_destroy(info->uid_source);

	e_mempool_destroy(info->uid_pool);

	g_ptr_array_free(info->uid_added, TRUE);
	g_ptr_array_free(info->uid_removed, TRUE);
	g_ptr_array_free(info->uid_changed, TRUE);
	g_free(info);
}



