/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8; fill-column: 160 -*- */
/* camel-imap-folder.c: Abstract class for an email folder */

/* 
 * Authors: Jeffrey Stedfast <fejj@helixcode.com> 
 *
 * Copyright (C) 2000 Helix Code, Inc.
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

#include <stdlib.h>
#include <sys/types.h>
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <fcntl.h>

#include "camel-imap-folder.h"
#include "camel-imap-store.h"
#include "camel-imap-stream.h"
#include "string-utils.h"
#include "camel-stream.h"
#include "camel-stream-fs.h"
#include "camel-stream-mem.h"
#include "camel-stream-buffer.h"
#include "camel-data-wrapper.h"
#include "camel-mime-message.h"
#include "camel-stream-filter.h"
#include "camel-mime-filter-from.h"
#include "camel-mime-filter-crlf.h"
#include "camel-exception.h"
#include "camel-mime-utils.h"

#define d(x)

#define CF_CLASS(o) (CAMEL_FOLDER_CLASS (GTK_OBJECT (o)->klass))

static CamelFolderClass *parent_class = NULL;

static void imap_init (CamelFolder *folder, CamelStore *parent_store,
		       CamelFolder *parent_folder, const gchar *name,
		       gchar *separator, gboolean path_begns_with_sep,
		       CamelException *ex);

static void imap_sync (CamelFolder *folder, gboolean expunge, CamelException *ex);
static gint imap_get_message_count (CamelFolder *folder, CamelException *ex);
static void imap_append_message (CamelFolder *folder, CamelMimeMessage *message, CamelException *ex);
static void imap_copy_message_to (CamelFolder *source, const char *uid, CamelFolder *destination, CamelException *ex);
static void imap_move_message_to (CamelFolder *source, const char *uid, CamelFolder *destination, CamelException *ex);
static GPtrArray *imap_get_uids (CamelFolder *folder, CamelException *ex);
static gboolean imap_parse_subfolder_line (gchar *buf, gchar **flags, gchar **sep, gchar **folder);
static GPtrArray *imap_get_subfolder_names (CamelFolder *folder, CamelException *ex);
static GPtrArray *imap_get_summary (CamelFolder *folder, CamelException *ex);
static void imap_free_summary (CamelFolder *folder, GPtrArray *array);
static CamelMimeMessage *imap_get_message (CamelFolder *folder, const gchar *uid, CamelException *ex);
static void imap_delete_message (CamelFolder *folder, const gchar *uid, CamelException *ex);

static void imap_expunge (CamelFolder *folder, CamelException *ex);

static const CamelMessageInfo *imap_get_message_info (CamelFolder *folder, const char *uid);

static GList *imap_search_by_expression (CamelFolder *folder, const char *expression, CamelException *ex);

static void imap_finalize (GtkObject *object);

/* flag methods */
static guint32  imap_get_permanent_flags (CamelFolder *folder, CamelException *ex);
static guint32  imap_get_message_flags   (CamelFolder *folder, const char *uid, CamelException *ex);
static void     imap_set_message_flags   (CamelFolder *folder, const char *uid, guint32 flags, guint32 set,
					  CamelException *ex);
static gboolean imap_get_message_user_flag (CamelFolder *folder, const char *uid, const char *name,
					    CamelException *ex);
static void     imap_set_message_user_flag (CamelFolder *folder, const char *uid, const char *name,
					    gboolean value, CamelException *ex);

static void
camel_imap_folder_class_init (CamelImapFolderClass *camel_imap_folder_class)
{
	CamelFolderClass *camel_folder_class = CAMEL_FOLDER_CLASS (camel_imap_folder_class);
	GtkObjectClass *gtk_object_class = GTK_OBJECT_CLASS (camel_folder_class);

	parent_class = gtk_type_class (camel_folder_get_type ());
		
	/* virtual method definition */

	/* virtual method overload */
	camel_folder_class->init = imap_init;
	camel_folder_class->sync = imap_sync;
	camel_folder_class->expunge = imap_expunge;

	camel_folder_class->get_uids = imap_get_uids;
	camel_folder_class->get_subfolder_names = imap_get_subfolder_names;

	camel_folder_class->get_message_count = imap_get_message_count;
	camel_folder_class->get_message = imap_get_message;
	camel_folder_class->append_message = imap_append_message;
	camel_folder_class->delete_message = imap_delete_message;
	camel_folder_class->copy_message_to = imap_copy_message_to;
	camel_folder_class->move_message_to = imap_move_message_to;
	
	camel_folder_class->get_summary = imap_get_summary;
	camel_folder_class->get_message_info = imap_get_message_info;
	camel_folder_class->free_summary = imap_free_summary;

	camel_folder_class->search_by_expression = imap_search_by_expression;

	camel_folder_class->get_permanent_flags = imap_get_permanent_flags;
	camel_folder_class->get_message_flags = imap_get_message_flags;
	camel_folder_class->set_message_flags = imap_set_message_flags;
	/*camel_folder_class->get_message_user_flags = imap_get_message_user_flags;*/
	/*camel_folder_class->set_message_user_flags = imap_set_message_user_flags;*/

	gtk_object_class->finalize = imap_finalize;	
}

static void
camel_imap_folder_init (gpointer object, gpointer klass)
{
	CamelImapFolder *imap_folder = CAMEL_IMAP_FOLDER (object);
	CamelFolder *folder = CAMEL_FOLDER (object);

	folder->can_hold_messages = TRUE;
	folder->can_hold_folders = TRUE;
	folder->has_summary_capability = TRUE;
	folder->has_search_capability = FALSE; /* default - we have to query IMAP to know for sure */

	imap_folder->summary = NULL;
	imap_folder->count = -1;
}

GtkType
camel_imap_folder_get_type (void)
{
	static GtkType camel_imap_folder_type = 0;
	
	if (!camel_imap_folder_type)	{
		GtkTypeInfo camel_imap_folder_info =	
		{
			"CamelImapFolder",
			sizeof (CamelImapFolder),
			sizeof (CamelImapFolderClass),
			(GtkClassInitFunc) camel_imap_folder_class_init,
			(GtkObjectInitFunc) camel_imap_folder_init,
				/* reserved_1 */ NULL,
				/* reserved_2 */ NULL,
			(GtkClassInitFunc) NULL,
		};
		
		camel_imap_folder_type = gtk_type_unique (CAMEL_FOLDER_TYPE, &camel_imap_folder_info);
	}
	
	return camel_imap_folder_type;
}

CamelFolder *
camel_imap_folder_new (CamelStore *parent, char *folder_name, CamelException *ex)
{
	CamelFolder *folder = CAMEL_FOLDER (gtk_object_new (camel_imap_folder_get_type (), NULL));
	
	CF_CLASS (folder)->init (folder, parent, NULL, folder_name, "/", FALSE, ex);

	return folder;
}

static void           
imap_finalize (GtkObject *object)
{
	/* TODO: do we need to do more here? */
	CamelImapFolder *imap_folder = CAMEL_IMAP_FOLDER (object);
	CamelMessageInfo *info;
	gint i, max;

	GTK_OBJECT_CLASS (parent_class)->finalize (object);

	g_return_if_fail (imap_folder->summary != NULL);
	
	max = imap_folder->summary->len;
	for (i = 0; i < max; i++) {
		info = g_ptr_array_index (imap_folder->summary, i);
		g_free (info->subject);
		g_free (info->from);
		g_free (info->to);
		g_free (info->cc);
		g_free (info->uid);
		g_free (info->message_id);
		header_references_list_clear (&info->references);
		g_free (info);
		info = NULL;
	}

	g_ptr_array_free (imap_folder->summary, TRUE);
	imap_folder->summary = NULL;
}

static void 
imap_init (CamelFolder *folder, CamelStore *parent_store, CamelFolder *parent_folder,
	   const gchar *name, gchar *separator, gboolean path_begins_with_sep, CamelException *ex)
{
	CamelImapFolder *imap_folder = CAMEL_IMAP_FOLDER (folder);
	CamelStore *store = CAMEL_STORE (parent_store);
	CamelURL *url = CAMEL_SERVICE (store)->url;
	int status;
	char *result, *folder_path;
	
	/* call parent method */
	parent_class->init (folder, parent_store, parent_folder, name, separator, path_begins_with_sep, ex);
	if (camel_exception_get_id (ex))
		return;

	/* we assume that the parent init
	   method checks for the existance of @folder */
	folder->can_hold_messages = TRUE;
	folder->can_hold_folders = TRUE;
	folder->has_summary_capability = TRUE;
	folder->has_search_capability = CAMEL_IMAP_STORE (store)->has_search_capability;
	
        /* some IMAP daemons support user-flags           *
	 * I would not, however, rely on this feature as  *
	 * most IMAP daemons are not 100% RFC compliant   */
	folder->permanent_flags = CAMEL_MESSAGE_SEEN |
		CAMEL_MESSAGE_ANSWERED |
		CAMEL_MESSAGE_FLAGGED |
		CAMEL_MESSAGE_DELETED |
		CAMEL_MESSAGE_DRAFT |
		CAMEL_MESSAGE_USER;

	
 	imap_folder->search = NULL;
	imap_folder->summary = NULL;

	/* SELECT the IMAP mail spool */
	if (url && url->path && *(url->path + 1) && strcmp (folder->full_name, "INBOX"))
		folder_path = g_strdup_printf ("%s/%s", url->path + 1, folder->full_name);
	else
		folder_path = g_strdup (folder->full_name);

	status = camel_imap_command_extended (CAMEL_IMAP_STORE (folder->parent_store), folder,
					      &result, "SELECT %s", folder_path);
	if (status != CAMEL_IMAP_OK) {
		CamelService *service = CAMEL_SERVICE (folder->parent_store);
		
		camel_exception_setv (ex, CAMEL_EXCEPTION_SERVICE_UNAVAILABLE,
				      "Could not SELECT %s on IMAP server %s: %s.",
				      folder->full_name, service->url->host, 
				      status == CAMEL_IMAP_ERR ? result :
				      "Unknown error");
	} else {
		/* parse the mode we opened it in */
	}
	g_free (result);
	g_free (folder_path);
}

static void
imap_sync (CamelFolder *folder, gboolean expunge, CamelException *ex)
{
#if 0
	CamelImapFolder *imap_folder = CAMEL_IMAP_FOLDER (folder);
	gint i, max;

	/* uhhh...this is kinda unsafe so we'll leave it blocked out */
	if (imap_folder->summary) {
		max = imap_folder->summary->len;
		for (i = 0; i < max; i++) {
			CamelMessageInfo *info;

			info = (CamelMessageInfo *) g_ptr_array_index (imap_folder->summary, i);
			if (info->flags & CAMEL_MESSAGE_FOLDER_FLAGGED) {
				char *flags;
				
				flags = g_strconcat (info->flags & CAMEL_MESSAGE_SEEN ? "\\Seen " : "",
						     info->flags & CAMEL_MESSAGE_DRAFT ? "\\Draft " : "",
						     info->flags & CAMEL_MESSAGE_DELETED ? "\\Deleted " : "",
						     info->flags & CAMEL_MESSAGE_DELETED ? "\\Answered " : "",
						     NULL);
				if (*flags) {
					gchar *result;
					gint s;
					
					*(flags + strlen (flags) - 1) = '\0';
					s = camel_imap_command_extended (CAMEL_IMAP_STORE (folder->parent_store),
									 folder, &result,
									 "UID STORE %s FLAGS.SILENT (%s)",
									 info->uid, flags);
					
					if (s != CAMEL_IMAP_OK) {
						CamelService *service = CAMEL_SERVICE (folder->parent_store);
						camel_exception_setv (ex, CAMEL_EXCEPTION_SERVICE_UNAVAILABLE,
								      "Could not set flags on message %s on IMAP "
								      "server %s: %s.", info->uid,
								      service->url->host,
								      status == CAMEL_IMAP_ERR ? result :
								      "Unknown error");
						g_free (result);
						return;
					}
					
					g_free(result);
				}
				g_free (flags);
			}
		}
	}
#endif
	
	if (expunge)
		imap_expunge (folder, ex);
}

static void
imap_expunge (CamelFolder *folder, CamelException *ex)
{
	gchar *result;
	gint status;

	g_return_if_fail (folder != NULL);
	
	status = camel_imap_command_extended (CAMEL_IMAP_STORE (folder->parent_store), folder,
					      &result, "EXPUNGE");
	
	if (status != CAMEL_IMAP_OK) {
		CamelService *service = CAMEL_SERVICE (folder->parent_store);
		camel_exception_setv (ex, CAMEL_EXCEPTION_SERVICE_UNAVAILABLE,
				      "Could not EXPUNGE from IMAP server %s: %s.",
				      service->url->host,
				      status == CAMEL_IMAP_ERR && result ? result :
				      "Unknown error");
		g_free (result);
		return;
	}
	
	g_free (result);
}

#if 0
static gboolean
imap_delete (CamelFolder *folder, gboolean recurse, CamelException *ex)
{
	/* TODO: code this & what should this do? delete messages or the folder? */
	CamelImapFolder *imap_folder = CAMEL_IMAP_FOLDER (folder);
	gboolean folder_already_exists;

	g_return_val_if_fail (folder != NULL, FALSE);

	/* check if the folder object exists */

	/* in the case where the folder does not exist, 
	   return immediatly */
	folder_already_exists = camel_folder_exists (folder, ex);
	if (camel_exception_get_id (ex))
		return FALSE;

	if (!folder_already_exists)
		return TRUE;

	/* call default implementation.
	   It should delete the messages in the folder
	   and recurse the operation to subfolders */
	parent_class->delete (folder, recurse, ex);
	if (camel_exception_get_id (ex))
		return FALSE;

	if (!(folder->full_name || folder->name)) {
		camel_exception_set (ex, CAMEL_EXCEPTION_FOLDER_INVALID,
				     "invalid folder path. Use set_name ?");
		return FALSE;
	}
	
        /* delete the directory - we must start with the leaves and work
	   back to the root if we are to delete recursively */

	/* TODO: Finish this... */

	return TRUE;
}
#endif

static gint
imap_get_message_count (CamelFolder *folder, CamelException *ex)
{
	CamelImapFolder *imap_folder = CAMEL_IMAP_FOLDER (folder);
	CamelStore *store = CAMEL_STORE (folder->parent_store);
	CamelURL *url = CAMEL_SERVICE (store)->url;
	gchar *result, *msg_count, *folder_path;
	gint status;

	g_return_val_if_fail (folder != NULL, -1);

	/* If we already have a count, return */
	if (imap_folder->count != -1)
		return imap_folder->count;

	if (url && url->path && *(url->path + 1) && strcmp (folder->full_name, "INBOX"))
		folder_path = g_strdup_printf ("%s/%s", url->path + 1, folder->full_name);
	else
		folder_path = g_strdup (folder->full_name);
	
	status = camel_imap_command_extended (CAMEL_IMAP_STORE (folder->parent_store), folder,
					      &result, "STATUS %s (MESSAGES)", folder_path);

	if (status != CAMEL_IMAP_OK) {
		CamelService *service = CAMEL_SERVICE (folder->parent_store);
		camel_exception_setv (ex, CAMEL_EXCEPTION_SERVICE_UNAVAILABLE,
				      "Could not get message count for %s from IMAP "
				      "server %s: %s.", folder_path, service->url->host,
				      status == CAMEL_IMAP_ERR ? result :
				      "Unknown error");
		g_free (result);
		g_free (folder_path);
		return -1;
	}
	g_free (folder_path);

	/* parse out the message count - should come in the form: "* STATUS <folder> (MESSAGES <count>)\r\n" */
	if (result && *result == '*') {
		if ((msg_count = strstr (result, "MESSAGES")) != NULL) {
			msg_count += strlen ("MESSAGES") + 1;

			for ( ; *msg_count == ' '; msg_count++);
			
			/* we should now be pointing to the message count */
			imap_folder->count = atoi (msg_count);
		}
	}
	g_free (result);

	return imap_folder->count;
}

static void
imap_append_message (CamelFolder *folder, CamelMimeMessage *message, CamelException *ex)
{
	CamelStore *store = CAMEL_STORE (folder->parent_store);
	CamelURL *url = CAMEL_SERVICE (store)->url;
	CamelStreamMem *mem;
	gchar *result, *folder_path;
	gint status;

	g_return_if_fail (folder != NULL);
	g_return_if_fail (message != NULL);

	/* write the message to a CamelStreamMem so we can get it's size */
	mem = CAMEL_STREAM_MEM (camel_stream_mem_new ());
	if (camel_data_wrapper_write_to_stream (CAMEL_DATA_WRAPPER (message), CAMEL_STREAM (mem)) == -1) {
		CamelService *service = CAMEL_SERVICE (folder->parent_store);
		camel_exception_setv (ex, CAMEL_EXCEPTION_SYSTEM,
				      "Could not APPEND message to IMAP server %s: %s.",
				      service->url->host,
				      g_strerror (errno));

	        return;
	}

	mem->buffer = g_byte_array_append (mem->buffer, g_strdup ("\r\n"), 3);
	
	if (url && url->path && *(url->path + 1) && strcmp (folder->full_name, "INBOX"))
		folder_path = g_strdup_printf ("%s/%s", url->path + 1, folder->full_name);
	else
		folder_path = g_strdup (folder->full_name);

	/* FIXME: len isn't really correct I don't think, we need to filter and possibly other things */
	status = camel_imap_command_extended (CAMEL_IMAP_STORE (folder->parent_store),
					      folder, &result,
					      "APPEND %s (\\Seen) {%d}\r\n%s",
					      folder_path, mem->buffer->len - 1, mem->buffer->data);

	if (status != CAMEL_IMAP_OK) {
		CamelService *service = CAMEL_SERVICE (folder->parent_store);
		camel_exception_setv (ex, CAMEL_EXCEPTION_SERVICE_UNAVAILABLE,
				      "Could not APPEND message to IMAP server %s: %s.",
				      service->url->host,
				      status == CAMEL_IMAP_ERR ? result :
				      "Unknown error");
		g_free (result);
		g_free (folder_path);
		return;
	}

	/* FIXME: we should close/free the mem stream */

	g_free (result);
	g_free (folder_path);
}

static void
imap_copy_message_to (CamelFolder *source, const char *uid, CamelFolder *destination, CamelException *ex)
{
	CamelStore *store = CAMEL_STORE (source->parent_store);
	CamelURL *url = CAMEL_SERVICE (store)->url;
	char *result, *folder_path;
	int status;
	
	if (url && url->path && *(url->path + 1) && strcmp (destination->full_name, "INBOX"))
		folder_path = g_strdup_printf ("%s/%s", url->path + 1, destination->full_name);
	else
		folder_path = g_strdup (destination->full_name);
	
	status = camel_imap_command (CAMEL_IMAP_STORE (store), NULL, &result,
				     "COPY %s %s", uid, folder_path);
	
	if (status != CAMEL_IMAP_OK) {
		CamelService *service = CAMEL_SERVICE (store);
		camel_exception_setv (ex, CAMEL_EXCEPTION_SERVICE_UNAVAILABLE,
				      "Could not COPY message %s to %s on IMAP server %s: %s.",
				      uid, folder_path, service->url->host,
				      status == CAMEL_IMAP_ERR ? result :
				      "Unknown error");
		g_free (result);
		g_free (folder_path);
		return;
	}

	g_free (result);
	g_free (folder_path);
}

/* FIXME: Duplication of code! */
static void
imap_move_message_to (CamelFolder *source, const char *uid, CamelFolder *destination, CamelException *ex)
{
	CamelStore *store = CAMEL_STORE (source->parent_store);
	CamelURL *url = CAMEL_SERVICE (store)->url;
	CamelMessageInfo *info;
	char *result, *folder_path;
	int status;
	
	if (url && url->path && *(url->path + 1) && strcmp (destination->full_name, "INBOX"))
		folder_path = g_strdup_printf ("%s/%s", url->path + 1, destination->full_name);
	else
		folder_path = g_strdup (destination->full_name);
	
	status = camel_imap_command (CAMEL_IMAP_STORE (store), NULL, &result,
				     "COPY %s %s", uid, folder_path);
	
	if (status != CAMEL_IMAP_OK) {
		CamelService *service = CAMEL_SERVICE (store);
		camel_exception_setv (ex, CAMEL_EXCEPTION_SERVICE_UNAVAILABLE,
				      "Could not COPY message %s to %s on IMAP server %s: %s.",
				      uid, folder_path, service->url->host,
				      status == CAMEL_IMAP_ERR ? result :
				      "Unknown error");
		g_free (result);
		g_free (folder_path);
		return;
	}

	g_free (result);
	g_free (folder_path);
	
	if (!(info = (CamelMessageInfo *)imap_get_message_info (source, uid))) {
		CamelService *service = CAMEL_SERVICE (store);
		
		camel_exception_setv (ex, CAMEL_EXCEPTION_SERVICE_UNAVAILABLE,
				      "Could not set flags for message %s on IMAP server %s: %s",
				      uid, service->url->host, "Unknown error");
		return;
	}

	info->flags |= CAMEL_MESSAGE_DELETED | CAMEL_MESSAGE_FOLDER_FLAGGED;
}

static GPtrArray *
imap_get_uids (CamelFolder *folder, CamelException *ex) 
{
	CamelMessageInfo *info;
	GPtrArray *array, *infolist;
	gint i, count;

	infolist = imap_get_summary (folder, ex);
	count = infolist->len;
	
	array = g_ptr_array_new ();
	g_ptr_array_set_size (array, count);
	for (i = 0; i < count; i++) {
		info = (CamelMessageInfo *) g_ptr_array_index (infolist, i);
		array->pdata[i] = g_strdup (info->uid);
	}

	imap_free_summary (folder, infolist);
	
	return array;
}

static gboolean
imap_parse_subfolder_line (gchar *buf, gchar **flags, gchar **sep, gchar **folder)
{
	gchar *ptr, *eptr;

	*flags = NULL;
	*sep = NULL;
	*folder = NULL;

	if (strncasecmp (buf, "* LIST", 6))
		return FALSE;

	ptr = strstr (buf + 6, "(");
	if (!ptr)
		return FALSE;
	
	ptr++;
	eptr = strstr (ptr, ")");
	if (!eptr)
		return FALSE;
	
	*flags = g_strndup (ptr, (gint)(eptr - ptr));

	ptr = strstr (eptr, "\"");
	if (!ptr)
		return FALSE;

	ptr++;
	eptr = strstr (ptr, "\"");
	if (!eptr)
		return FALSE;

	*sep = g_strndup (ptr, (gint)(eptr - ptr));

	ptr = eptr + 1;
	*folder = g_strdup (ptr);
	g_strstrip (*folder);

	return TRUE;
}

static GPtrArray *
imap_get_subfolder_names (CamelFolder *folder, CamelException *ex)
{
	CamelImapFolder *imap_folder = CAMEL_IMAP_FOLDER (folder);
	CamelStore *store = CAMEL_STORE (folder->parent_store);
	CamelURL *url = CAMEL_SERVICE (store)->url;
	GPtrArray *listing;
	gint status;
	gchar *result, *folder_path;
	
	g_return_val_if_fail (folder != NULL, g_ptr_array_new());
	
	if (imap_folder->count != -1)
		return g_ptr_array_new ();

	if (url && url->path) {
		if (!strcmp (folder->full_name, "INBOX"))
			folder_path = g_strdup (url->path + 1);
		else
			folder_path = g_strdup_printf ("%s/%s", url->path + 1, folder->full_name);
	} else {
		folder_path = g_strdup (folder->full_name);
	}
	
	status = camel_imap_command_extended (CAMEL_IMAP_STORE (folder->parent_store), folder,
					      &result, "LIST \"\" \"%s%s\"", folder_path,
					      *folder_path ? "/*" : "*");
	
	if (status != CAMEL_IMAP_OK) {
		CamelService *service = CAMEL_SERVICE (folder->parent_store);
		camel_exception_setv (ex, CAMEL_EXCEPTION_SERVICE_UNAVAILABLE,
				      "Could not get subfolder listing from IMAP "
				      "server %s: %s.", service->url->host,
				      status == CAMEL_IMAP_ERR ? result :
				      "Unknown error");
		g_free (result);
		g_free (folder_path);
		return g_ptr_array_new ();
	}
	
	/* parse out the subfolders */
	listing = g_ptr_array_new ();
	if (result) {
		char *ptr = result;
		
		while (ptr && *ptr == '*') {
			gchar *flags, *sep, *folder, *buf, *end, *f;
			gboolean ret;
			
			for (end = ptr; *end && *end != '\n'; end++);
			buf = g_strndup (ptr, (gint)(end - ptr));
			ptr = end;
			
			ret = imap_parse_subfolder_line (buf, &flags, &sep, &folder);
			g_free (buf);
			
			if (!ret || (flags && strstr (flags, "NoSelect"))) {
				g_free (flags);
				g_free (sep);
				g_free (folder);

				if (*ptr == '\n')
					ptr++;
				
				continue;
			}
			g_free (flags);

			/* chop out the folder prefix */
			if (*folder_path && !strncmp (folder, folder_path, strlen (folder_path))) {
				f = folder + strlen (folder_path) + 1;
				memmove (folder, f, strlen (f) + 1);
			}
			printf ("adding folder: %s\n", folder);
			
			g_ptr_array_add (listing, folder);

			g_free (sep);  /* TODO: decide if we really need dir_sep */
			
			if (*ptr == '\n')
				ptr++;
		}
	}
	g_free (result);
	g_free (folder_path);

	return listing;
}

static void
imap_delete_message (CamelFolder *folder, const gchar *uid, CamelException *ex)
{
	CamelMessageInfo *info;

	if (!(info = (CamelMessageInfo *)imap_get_message_info (folder, uid))) {
		CamelService *service = CAMEL_SERVICE (folder->parent_store);
		
		camel_exception_setv (ex, CAMEL_EXCEPTION_SERVICE_UNAVAILABLE,
				      "Could not set flags for message %s on IMAP server %s: %s",
				      uid, service->url->host, "Unknown error");
		return;
	}

	info->flags |= CAMEL_MESSAGE_DELETED | CAMEL_MESSAGE_FOLDER_FLAGGED;
}

static CamelMimeMessage *
imap_get_message (CamelFolder *folder, const gchar *uid, CamelException *ex)
{
	CamelStream *msgstream;
	/*CamelStreamFilter *f_stream;*/
	/*CamelMimeFilter *filter;*/
	CamelMimeMessage *msg;
	/*CamelMimePart *part;*/
	gchar *result, *header, *body, *mesg, *p, *q;
	int id, status, part_len;

	status = camel_imap_command_extended (CAMEL_IMAP_STORE (folder->parent_store), folder,
					      &result, "UID FETCH %s BODY.PEEK[HEADER]", uid);

	if (!result || status != CAMEL_IMAP_OK) {
		CamelService *service = CAMEL_SERVICE (folder->parent_store);
		
		camel_exception_setv (ex, CAMEL_EXCEPTION_SERVICE_UNAVAILABLE,
				      "Could not fetch message %s on IMAP server %s: %s",
				      uid, service->url->host,
				      status == CAMEL_IMAP_ERR ? result :
				      "Unknown error");
		g_free (result);
		return camel_mime_message_new ();
	}

	for (p = result; *p && *p != '{' && *p != '\n'; p++);
	if (*p != '{') {
		g_free (result);
		return camel_mime_message_new ();
	}

        part_len = atoi (p + 1);
	for ( ; *p && *p != '\n'; p++);
	if (*p != '\n') {
		g_free (result);
		return camel_mime_message_new ();
	}

	/* calculate the new part-length */
	for (q = p; *q && (q - p) <= part_len; q++) {
		if (*q == '\n')
			part_len--;
	}
	
	header = g_strndup (p, part_len);

	g_free (result);
	printf ("*** We got the header ***\n");

	status = camel_imap_command_extended (CAMEL_IMAP_STORE (folder->parent_store), folder,
					      &result, "UID FETCH %s BODY[TEXT]", uid);

	if (!result || status != CAMEL_IMAP_OK) {
		CamelService *service = CAMEL_SERVICE (folder->parent_store);
		
		camel_exception_setv (ex, CAMEL_EXCEPTION_SERVICE_UNAVAILABLE,
				      "Could not fetch message %s on IMAP server %s: %s",
				      uid, service->url->host,
				      status == CAMEL_IMAP_ERR ? result :
				      "Unknown error");
		g_free (result);
		g_free (header);
		return camel_mime_message_new ();
	}

	for (p = result; *p && *p != '{' && *p != '\n'; p++);
	if (*p != '{') {
		g_free (result);
		g_free (header);
		return camel_mime_message_new ();
	}

        part_len = atoi (p + 1);
	for ( ; *p && *p != '\n'; p++);
	if (*p != '\n') {
		g_free (result);
		g_free (header);
		return camel_mime_message_new ();
	}

	/* calculate the new part-length */
	for (q = p; *q && (q - p) <= part_len; q++) {
		if (*q == '\n')
			part_len--;
	}

	body = g_strndup (p, part_len);

	g_free (result);
	printf ("*** We got the body ***\n");

	mesg = g_strdup_printf ("%s\n%s", header, body);
	g_free (header);
	g_free (body);
	printf ("*** We got the mesg ***\n");

	fprintf (stderr, "Message:\n%s\n", mesg);

	msgstream = camel_stream_mem_new_with_buffer (mesg, strlen (mesg) + 1);
#if 0
	f_stream = camel_stream_filter_new_with_stream (msgstream);
	filter = camel_mime_filter_crlf_new (CAMEL_MIME_FILTER_CRLF_DECODE, CAMEL_MIME_FILTER_CRLF_MODE_CRLF_DOTS);
	id = camel_stream_filter_add (f_stream, CAMEL_MIME_FILTER (filter));
#endif	
	msg = camel_mime_message_new ();
	printf ("*** We created the camel_mime_message ***\n");
	
	camel_data_wrapper_construct_from_stream (CAMEL_DATA_WRAPPER (msg), msgstream);
#if 0	
	camel_stream_filter_remove (f_stream, id);
	camel_stream_close (CAMEL_STREAM (f_stream));
#endif
	gtk_object_unref (GTK_OBJECT (msgstream));
	/*gtk_object_unref (GTK_OBJECT (f_stream));*/

	printf ("*** We're returning... ***\n");
	
	return msg;
	
#if 0
	CamelStream *imap_stream;
	CamelStream *msgstream;
	CamelStreamFilter *f_stream;   /* will be used later w/ crlf filter */
	CamelMimeFilter *filter;       /* crlf/dot filter */
	CamelMimeMessage *msg;
	CamelMimePart *part;
	CamelDataWrapper *cdw;
	gchar *cmdbuf;
	int id;
	
	/* TODO: fetch the correct part, get rid of the hard-coded stuff */
	cmdbuf = g_strdup_printf ("UID FETCH %s BODY[TEXT]", uid);
	imap_stream = camel_imap_stream_new (CAMEL_IMAP_FOLDER (folder), cmdbuf);
	g_free (cmdbuf);


	/* Temp hack - basically we read in the entire message instead of getting a part as it's needed */
	msgstream = camel_stream_mem_new ();
	camel_stream_write_to_stream (CAMEL_STREAM (imap_stream), msgstream);
	gtk_object_unref (GTK_OBJECT (imap_stream));
	
	f_stream = camel_stream_filter_new_with_stream (msgstream);
	filter = camel_mime_filter_crlf_new (CAMEL_MIME_FILTER_CRLF_DECODE, CAMEL_MIME_FILTER_CRLF_MODE_CRLF_DOTS);
	id = camel_stream_filter_add (f_stream, CAMEL_MIME_FILTER (filter));
	
	msg = camel_mime_message_new ();
	
	/*cdw = camel_data_wrapper_new ();*/
	/*camel_data_wrapper_construct_from_stream (cdw, CAMEL_STREAM (f_stream));*/
	camel_data_wrapper_construct_from_stream (CAMEL_DATA_WRAPPER (msg), CAMEL_STREAM (f_stream));
	
	camel_stream_filter_remove (f_stream, id);
	camel_stream_close (CAMEL_STREAM (f_stream));
	gtk_object_unref (GTK_OBJECT (msgstream));
	gtk_object_unref (GTK_OBJECT (f_stream));
	
	/*camel_data_wrapper_set_mime_type (cdw, "text/plain");*/

	/*camel_medium_set_content_object (CAMEL_MEDIUM (msg), CAMEL_DATA_WRAPPER (cdw));*/
	/*gtk_object_unref (GTK_OBJECT (cdw));*/
	
	return msg;
#endif
}

/* This probably shouldn't go here...but it will for now */
static gchar *
get_header_field (gchar *header, gchar *field)
{
	gchar *part, *index, *p, *q;

	index = strstrcase (header, field);
	if (index == NULL)
		return NULL;

	p = index + strlen (field) + 1;
	for (q = p; *q; q++)
		if (*q == '\n' && (*(q + 1) != ' ' || *(q + 1) != '\t'))
			break;

	part = g_strndup (p, (gint)(q - p));

	/* it may be wrapped on multiple lines, so lets strip out \n's */
	for (p = part; *p; ) {
		if (*p == '\r' || *p == '\n')
			memmove (p, p + 1, strlen (p) - 1);
		else
			p++;
	}
	
	return part;
}

static char *header_fields[] = { "subject", "from", "to", "cc", "date",
				 "received", "message-id", "references",
				 "in-reply-to", "" };

GPtrArray *
imap_get_summary (CamelFolder *folder, CamelException *ex)
{
	CamelImapFolder *imap_folder = CAMEL_IMAP_FOLDER (folder);
	GPtrArray *array = NULL;
	CamelMessageInfo *info;
	gint num, i, j, status = 0;
	char *result, *p, *q;
	const char *received;
	struct _header_raw *h, *tail = NULL;

	if (imap_folder->summary)
		return imap_folder->summary;
	
	num = imap_get_message_count (folder, ex);
	
	array = g_ptr_array_new ();
	
	for (i = 1; i <= num; i++) {
		status = camel_imap_command_extended (CAMEL_IMAP_STORE (folder->parent_store), folder,
						      &result, "FETCH %d BODY.PEEK[HEADER]", i);
		
		if (status != CAMEL_IMAP_OK) {
			CamelService *service = CAMEL_SERVICE (folder->parent_store);
		
			camel_exception_setv (ex, CAMEL_EXCEPTION_SERVICE_UNAVAILABLE,
					      "Could not get summary for %s on IMAP server %s: %s",
					      folder->full_name, service->url->host,
					      status == CAMEL_IMAP_ERR ? result :
					      "Unknown error");
			g_free (result);
			break;
		}

		/* construct the header list */
		h = NULL;
		for (j = 0; *header_fields[j]; j++) {
			struct _header_raw *raw;
			char *field, *value;

			field = g_strdup_printf ("\n%s:", header_fields[j]);
			value = get_header_field (result, field);
			g_free (field);
			if (!value)
				continue;
			
			raw = g_malloc0 (sizeof (struct _header_raw));
			raw->next = NULL;
			raw->name = g_strdup (header_fields[j]);
			raw->value = value;
			raw->offset = -1;

			if (!h) {
				h = raw;
				tail = h;
			} else {
				tail->next = raw;
				tail = raw;
			}
		}

		/* construct the CamelMessageInfo */
		info = g_malloc0 (sizeof (CamelMessageInfo));
		info->subject = camel_summary_format_string (h, "subject");
		info->from = camel_summary_format_address (h, "from");
		info->to = camel_summary_format_address (h, "to");
		info->cc = camel_summary_format_address (h, "cc");
		info->user_flags = NULL;
		info->date_sent = header_decode_date (header_raw_find (&h, "date", NULL), NULL);
		received = header_raw_find (&h, "received", NULL);
		if (received)
			received = strrchr (received, ';');
		if (received)
			info->date_received = header_decode_date (received + 1, NULL);
		else
			info->date_received = 0;
		info->message_id = header_msgid_decode (header_raw_find (&h, "message-id", NULL));
		/* if we have a references, use that, otherwise, see if we have an in-reply-to
		   header, with parsable content, otherwise *shrug* */
		info->references = header_references_decode (header_raw_find (&h, "references", NULL));
		if (info->references == NULL)
			info->references = header_references_decode (header_raw_find (&h, "in-reply-to", NULL));

		g_free (result);

		while (h->next) {
			struct _header_raw *next = h->next;
			
			g_free (h->name);
			g_free (h->value);
			g_free (h);
			h = next;
		}

		/* now to get the UID */
		status = camel_imap_command_extended (CAMEL_IMAP_STORE (folder->parent_store), folder,
						      &result, "FETCH %d UID", i);
		
		if (status != CAMEL_IMAP_OK) {
			CamelService *service = CAMEL_SERVICE (folder->parent_store);
		
			camel_exception_setv (ex, CAMEL_EXCEPTION_SERVICE_UNAVAILABLE,
					      "Could not get summary for %s on IMAP server %s: %s",
					      folder->full_name, service->url->host,
					      status == CAMEL_IMAP_ERR ? result :
					      "Unknown error");
			g_free (result);

			g_free (info->subject);
			g_free (info->from);
			g_free (info->to);
			g_free (info->cc);
			g_free (info->message_id);
			header_references_list_clear (&info->references);
			g_free (info);
			info = NULL;

			break;
		}

		if (!result || *result != '*') {
			g_free (result);
			fprintf (stderr, "Warning: UID for message %d not found\n", i);

			g_free (info->subject);
			g_free (info->from);
			g_free (info->to);
			g_free (info->cc);
			g_free (info->message_id);
			header_references_list_clear (&info->references);
			g_free (info);
			info = NULL;
			
			break;
		}

		p = strchr (result, '(');
		if (!p || strncasecmp (p + 1, "UID", 3)) {
			g_free (result);
			fprintf (stderr, "Warning: UID for message %d not found\n", i);

			g_free (info->subject);
			g_free (info->from);
			g_free (info->to);
			g_free (info->cc);
			g_free (info->message_id);
			header_references_list_clear (&info->references);
			g_free (info);
			info = NULL;
			
			break;
		}

		for (p += 4; *p && (*p < '0' || *p > '9'); p++);    /* advance to <uid> */
		for (q = p; *q && *q != ')' && *q != ' '; q++);     /* find the end of the <uid> */
		info->uid = g_strndup (p, (gint)(q - p));
		printf ("*** info->uid = %s\n", info->uid);
		g_free (result);
		
		/* now to get the flags */
		status = camel_imap_command_extended (CAMEL_IMAP_STORE (folder->parent_store), folder,
						      &result, "FETCH %d FLAGS", i);
		
		if (status != CAMEL_IMAP_OK) {
			CamelService *service = CAMEL_SERVICE (folder->parent_store);
		
			camel_exception_setv (ex, CAMEL_EXCEPTION_SERVICE_UNAVAILABLE,
					      "Could not get summary for %s on IMAP server %s: %s",
					      folder->full_name, service->url->host,
					      status == CAMEL_IMAP_ERR ? result :
					      "Unknown error");
			g_free (result);

			g_free (info->subject);
			g_free (info->from);
			g_free (info->to);
			g_free (info->cc);
			g_free (info->message_id);
			g_free (info->uid);
			header_references_list_clear (&info->references);
			g_free (info);
			info = NULL;

			break;
		}

		if (!result || *result != '*') {
			g_free (result);
			fprintf (stderr, "Warning: FLAGS for message %d not found\n", i);

			g_free (info->subject);
			g_free (info->from);
			g_free (info->to);
			g_free (info->cc);
			g_free (info->message_id);
			g_free (info->uid);
			header_references_list_clear (&info->references);
			g_free (info);
			info = NULL;
			
			break;
		}

		p = strchr (result, '(') + 1;
		if (strncasecmp (p, "FLAGS", 5)) {
			g_free (result);
			fprintf (stderr, "Warning: FLAGS for message %d not found\n", i);

			g_free (info->subject);
			g_free (info->from);
			g_free (info->to);
			g_free (info->cc);
			g_free (info->message_id);
			g_free (info->uid);
			header_references_list_clear (&info->references);
			g_free (info);
			info = NULL;
			
			break;
		}

		/* now we gotta parse for the flags */
		info->flags = 0;
		if (strstr (p, "\\Seen"))
			info->flags |= CAMEL_MESSAGE_SEEN;
		if (strstr (p, "\\Answered"))
			info->flags |= CAMEL_MESSAGE_ANSWERED;
		if (strstr (p, "\\Flagged"))
			info->flags |= CAMEL_MESSAGE_FLAGGED;
		if (strstr (p, "\\Deleted"))
			info->flags |= CAMEL_MESSAGE_DELETED;
		if (strstr (p, "\\Draft"))
			info->flags |= CAMEL_MESSAGE_DRAFT;

		g_free (result);

		g_ptr_array_add (array, info);
	}

	imap_folder->summary = array;
	
	return array;
}

void
imap_free_summary (CamelFolder *folder, GPtrArray *array)
{
	/* no-op */
	return;
}

/* get a single message info, by uid */
static const CamelMessageInfo *
imap_get_message_info (CamelFolder *folder, const char *uid)
{
	CamelImapFolder *imap_folder = CAMEL_IMAP_FOLDER (folder);
	CamelMessageInfo *info = NULL;
	struct _header_raw *h, *tail = NULL;
	const char *received;
	char *result, *p;
	int j, status;

	/* lets first check to see if we have the message info cached */
	if (imap_folder->summary) {
		int max, i;

		max = imap_folder->summary->len;
		for (i = 0; i < max; i++) {
			info = g_ptr_array_index (imap_folder->summary, i);
			if (!strcmp (info->uid, uid))
				return info;
		}
	}

	/* we don't have a cached copy, so fetch it */
	
	status = camel_imap_command_extended (CAMEL_IMAP_STORE (folder->parent_store), folder,
					      &result, "UID FETCH %s BODY.PEEK[HEADER]", uid);

	if (status != CAMEL_IMAP_OK) {
		g_free (result);
		return NULL;
	}

	h = NULL;
	for (j = 0; *header_fields[j]; j++) {
		struct _header_raw *raw;
		char *field, *value;
		
		field = g_strdup_printf ("\n%s:", header_fields[j]);
		value = get_header_field (result, field);
		g_free (field);
		if (!value)
			continue;
		
		raw = g_malloc0 (sizeof (struct _header_raw));
		raw->next = NULL;
		raw->name = g_strdup (header_fields[j]);
		raw->value = value;
		raw->offset = -1;
		
		if (!h) {
			h = raw;
			tail = h;
		} else {
			tail->next = raw;
			tail = raw;
		}
	}
	
	/* construct the CamelMessageInfo */
	info = g_malloc0 (sizeof (CamelMessageInfo));
	info->subject = camel_summary_format_string (h, "subject");
	info->from = camel_summary_format_address (h, "from");
	info->to = camel_summary_format_address (h, "to");
	info->cc = camel_summary_format_address (h, "cc");
	info->user_flags = NULL;
	info->date_sent = header_decode_date (header_raw_find (&h, "date", NULL), NULL);
	received = header_raw_find (&h, "received", NULL);
	if (received)
		received = strrchr (received, ';');
	if (received)
		info->date_received = header_decode_date (received + 1, NULL);
	else
		info->date_received = 0;
	info->message_id = header_msgid_decode (header_raw_find (&h, "message-id", NULL));
	/* if we have a references, use that, otherwise, see if we have an in-reply-to
	   header, with parsable content, otherwise *shrug* */
	info->references = header_references_decode (header_raw_find (&h, "references", NULL));
	if (info->references == NULL)
		info->references = header_references_decode (header_raw_find (&h, "in-reply-to", NULL));
	
	info->uid = g_strdup (uid);
	g_free (result);

	while (h->next) {
		struct _header_raw *next = h->next;
		
		g_free (h->name);
		g_free (h->value);
		g_free (h);
		h = next;
	}
	
	/* now to get the flags */
	status = camel_imap_command_extended (CAMEL_IMAP_STORE (folder->parent_store), folder,
					      &result, "UID FETCH %s FLAGS", uid);
	
	if (status != CAMEL_IMAP_OK) {
		g_free (result);
		fprintf (stderr, "Warning: Error getting FLAGS for message %s\n", uid);
		
		return info; /* I guess we should return what we got so far? */
	}
	
	if (!result || *result != '*') {
		g_free (result);
		fprintf (stderr, "Warning: FLAGS for message %s not found\n", uid);

		return info; /* I guess we should return what we got so far? */
	}
	
	p = strchr (result, '(') + 1;
	if (strncasecmp (p, "FLAGS", 5)) {
		g_free (result);
		fprintf (stderr, "Warning: FLAGS for message %s not found\n", uid);

		return info; /* I guess we should return what we got so far? */
	}
	
	/* now we gotta parse for the flags */
	info->flags = 0;
	if (strstr (p, "\\Seen"))
		info->flags |= CAMEL_MESSAGE_SEEN;
	if (strstr (p, "\\Answered"))
		info->flags |= CAMEL_MESSAGE_ANSWERED;
	if (strstr (p, "\\Flagged"))
		info->flags |= CAMEL_MESSAGE_FLAGGED;
	if (strstr (p, "\\Deleted"))
		info->flags |= CAMEL_MESSAGE_DELETED;
	if (strstr (p, "\\Draft"))
		info->flags |= CAMEL_MESSAGE_DRAFT;

	g_free (result);

	/* since we didn't have it cached, lets add it to our cache */
	if (imap_folder->summary) {
		g_ptr_array_add (imap_folder->summary, info);
	} else {
		imap_folder->summary = g_ptr_array_new ();
		g_ptr_array_add (imap_folder->summary, info);
	}
	
	return info;
}

static GList *
imap_search_by_expression (CamelFolder *folder, const char *expression, CamelException *ex)
{
	return NULL;
#if 0
	/* NOTE: This is experimental code... */
	CamelImapFolder *imap_folder = CAMEL_IMAP_FOLDER (folder);
	char *result;
	int status;

	if (!imap_folder->has_search_capability)
		return NULL;

	status = camel_imap_command_extended (CAMEL_IMAP_STORE (folder->parent_store), folder,
					      &result, "SEARCH %s", expression);
	
	if (status != CAMEL_IMAP_OK) {
		CamelService *service = CAMEL_SERVICE (folder->parent_store);
		
		camel_exception_setv (ex, CAMEL_EXCEPTION_SERVICE_UNAVAILABLE,
				      "Could not get summary for %s on IMAP server %s: %s",
				      folder->full_name, service->url->host,
				      status == CAMEL_IMAP_ERR ? result :
				      "Unknown error");
		g_free (result);
		return NULL;
	}

	/* now to parse @result */
#endif	
}

static guint32
imap_get_permanent_flags (CamelFolder *folder, CamelException *ex)
{
	/* return permamnant flags */
	return folder->permanent_flags;
}

static guint32
imap_get_message_flags (CamelFolder *folder, const char *uid, CamelException *ex)
{
	CamelMessageInfo *info;

	if (!(info = (CamelMessageInfo *)imap_get_message_info (folder, uid))) {
		CamelService *service = CAMEL_SERVICE (folder->parent_store);
		
		camel_exception_setv (ex, CAMEL_EXCEPTION_SERVICE_UNAVAILABLE,
				      "Could not get flags for message %s on IMAP server %s: %s",
				      uid, service->url->host, "Unknown error");
		return 0;
	}

	return info->flags;
}

static void
imap_set_message_flags (CamelFolder *folder, const char *uid, guint32 flags, guint32 set, CamelException *ex)
{
	CamelMessageInfo *info;

	if (!(info = (CamelMessageInfo *)imap_get_message_info (folder, uid))) {
		CamelService *service = CAMEL_SERVICE (folder->parent_store);
		
		camel_exception_setv (ex, CAMEL_EXCEPTION_SERVICE_UNAVAILABLE,
				      "Could not set flags for message %s on IMAP server %s: %s",
				      uid, service->url->host, "Unknown error");
		return;
	}

	info->flags = (info->flags & ~flags) | (set & flags) | CAMEL_MESSAGE_FOLDER_FLAGGED;
}

static gboolean
imap_get_message_user_flag (CamelFolder *folder, const char *uid, const char *name, CamelException *ex)
{
	return FALSE;
}

static void
imap_set_message_user_flag (CamelFolder *folder, const char *uid, const char *name, gboolean value, CamelException *ex)
{
	return;
}
